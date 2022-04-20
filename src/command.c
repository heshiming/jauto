/*
 * JAuto: Lightweight Java GUI Automation
 * Copyright (C) 2022 He Shiming.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "jvmti.h"

#include "command.h"
#include "logger.h"


#define BUFSZ_LONG      1024
#define BUFSZ_SHORT     256


int                     COMMAND_ENABLED = 0;
int                     J_GLOBAL_ITER_TAG = 1;  // reserving a tag number for each call
static JavaVM*          jvm = NULL;
static jvmtiEnv*        jvmti = NULL;

static pthread_t        thread;
static pthread_mutex_t  lock;
static int              shutdown = 0;

static command_t*       command_list = NULL;
static int              n_commands = 0;
static char             cn_jframe[] = "javax/swing/JFrame";
static char             cn_jdialog[] = "javax/swing/JDialog";
static char             cn_jwindow[] = "javax/swing/JWindow";

static void*    run(void* useless);


/* java essentials ----------------------------------------------- */


void*
get_jnienv(int* need_detach) {
    jint r;
    JNIEnv* env = NULL;
    r = (*jvm)->GetEnv(jvm, (void**) &env, JNI_VERSION_1_6);
    if (r == JNI_EDETACHED) {
        r = (*jvm)->AttachCurrentThread(jvm, (void**) &env, NULL);
        *need_detach = 1;
        if (r != JNI_OK || env == NULL) {
            //
        }
    }
    return env;
}


void
detach_jnienv() {
    (*jvm)->DetachCurrentThread(jvm);
}


jvmtiIterationControl JNICALL
heap_object_callback(jlong class_tag, jlong size, jlong* tag_ptr, void* user_data) {
    *tag_ptr = *(jlong*)user_data;
    return JVMTI_ITERATION_CONTINUE;
}


void
prepare_jvmti_iteration(JNIEnv* env, jvmtiEnv* jvmti, jlong* tag) {
    /* 

    `IterateOverInstancesOfClass` is used to search through all classes via
    a tagging mechanism. Once iterated, the tags are not to be automatically
    reset until another iteration resets the tags. To simplify so that
    more `find_windows` calls can be made without hassle, `J_GLOBAL_ITER_TAG`
    keeps global 'tag number' that must be increased once the current one is
    used, so that each iteration gets a unique tag number.

    For long running applications, this routine resets all 'java.lang.Object'
    tag to 1 once counter reaches 1000000.

     */
    if (J_GLOBAL_ITER_TAG >= 5) {
        jlong tag = 0;
        jclass cls = (*env)->FindClass(env, "java/lang/Object");
        (*jvmti)->IterateOverInstancesOfClass(jvmti, cls, JVMTI_HEAP_OBJECT_EITHER, heap_object_callback, &tag);
        (*env)->DeleteLocalRef(env, cls);
        J_GLOBAL_ITER_TAG = 1;
    }
    *tag = J_GLOBAL_ITER_TAG;
    J_GLOBAL_ITER_TAG++;
}


/* command declaration and registration -------------------------- */


DECLARE_COMMAND(ping)
DECLARE_COMMAND(get_windows)
DECLARE_COMMAND(list_ui_components)
DECLARE_COMMAND(list_menu)

int
command_registry(command_t* cmd) {
    if (!COMMAND_ENABLED) {
        logger.info("received command %s, but processing is disabled, ignored\n", cmd->input);
        cmd->state = 1;
        return -2;
    }
    int jnienv_need_detach = 0;
    JNIEnv* env = get_jnienv(&jnienv_need_detach);
    if (!env) {
        logger.error("attempted to execute command %s, but JNIEnv is unavailable\n", cmd->input);
        cmd->state = 1;
        return -2;
    }
    int ret = -1;
    cmd->state = 0;
    //
    REGISTER_COMMAND(ping, "ping")
    REGISTER_COMMAND(get_windows, "get_windows")
    REGISTER_COMMAND(list_ui_components, "list_ui_components")
    REGISTER_COMMAND(list_menu, "list_menu")
    //
    if (ret == -1 &&
        cmd->state == 0) {
        cmd->state = 1;
        cmd->output = sdsnew("unknown command");
        cmd->output = sdscatprintf(cmd->output, ": %s", cmd->input);
        logger.info("unknown command: %s\n", cmd->input);
    }
    if (jnienv_need_detach)
        detach_jnienv();
    return ret;
}


/* exposed ------------------------------------------------------- */


static void
release_one(command_t* cmd) {
    if (cmd->output)
        sdsfree(cmd->output);
    if (cmd->output_path)
        sdsfree(cmd->output_path);
    if (cmd->args)
        sdsfreesplitres(cmd->args, cmd->n_args);
    if (cmd->input)
        sdsfree(cmd->input);
}


static void
start_handler(void* jvm_) {
    jint r;
    jvm = (JavaVM*)jvm_;
    // Get the JVMTI environment
    r = (*jvm)->GetEnv(jvm, (void**) &jvmti, JVMTI_VERSION_1_0);
    if (r != JNI_OK || jvmti == NULL) {
        printf("Unable to get access to JVMTI version 1.0, r: %d\n", r);
    }
    jvmtiCapabilities capabilities = {0};
    capabilities.can_tag_objects = 1;
    (*jvmti)->AddCapabilities(jvmti, &capabilities);

    pthread_mutex_init(&lock, NULL);
    pthread_create(&thread, NULL, &run, (void*) NULL);
}


static void
stop_handler() {
    shutdown = 1;
    pthread_join(thread, NULL);
    pthread_mutex_lock(&lock);
    if (command_list) {
        for (int i = 0; i < n_commands; i++)
            release_one(&command_list[i]);
        free(command_list);
        n_commands = 0;
    }
    pthread_mutex_unlock(&lock);
}


static void
_parse_command(const char* cmd_in, command_t* cmd_out) {
    memset(cmd_out, 0, sizeof(command_t));
    int cmd_len = strlen(cmd_in);
    char* cmd_pos = strchr(cmd_in, '|');
    char* arg_pos = strchr(cmd_in, '?');
    cmd_out->arg_wnd_cn = &cn_jframe[0];
    if (arg_pos) {
        // has argument after command
        sds args = sdsnew(arg_pos + 1);
        cmd_out->args = sdssplitlen(args, sdslen(args), "&", 1, &cmd_out->n_args);
        sdsfree(args);
        arg_pos[0] = '\0';
        // pre-parse window_type
        for (int i = 0; i < cmd_out->n_args; i++) {
            if (strncmp_lit2(cmd_out->args[i], "window_type=dialog") == 0) {
                cmd_out->arg_wnd_cn = &cn_jdialog[0];
                break;
            }
            if (strncmp_lit2(cmd_out->args[i], "window_type=window") == 0) {
                cmd_out->arg_wnd_cn = &cn_jwindow[0];
                break;
            }
        }
    }
    if (cmd_pos) {
        // has at least an output path, and a command, delimited by |
        cmd_out->input = sdsnew(cmd_pos + 1);
        cmd_out->output_path = sdsnewlen(cmd_in, cmd_pos - cmd_in);
    } else
        // has only the command
        cmd_out->input = sdsnewlen(cmd_in, arg_pos?(arg_pos - cmd_in):cmd_len);
    cmd_out->state = -1;
}


static void
schedule(const char* cmd_in) {
    if (shutdown)
        return;
    pthread_mutex_lock(&lock);
    if (command_list == NULL) {
        n_commands = 1;
        command_list = malloc(sizeof(command_t));
        _parse_command(cmd_in, &command_list[0]);
    } else {
        n_commands += 1;
        command_list = realloc(command_list, sizeof(command_t) * n_commands);
        _parse_command(cmd_in, &command_list[n_commands - 1]);
    }
    pthread_mutex_unlock(&lock);
}


static int
fetch_one(command_t* cmd_out) {
    if (shutdown)
        return 0;
    pthread_mutex_lock(&lock);
    if (command_list &&
        n_commands > 0) {
        int i;
        for (i = 0; i < n_commands; i++) {
            if (command_list[i].state == 1)
                break;
        }
        if (i < n_commands) {
            // early break indicates a finished entry
            // fetch the return value
            memcpy(cmd_out, &command_list[i], sizeof(command_t));
            // prepare to destroy the entry, first copy the rest of array over
            for (int j = i; j < n_commands - 1; j++) {
                command_list[j] = command_list[j + 1];
            }
            // shrink memory and set count
            n_commands -= 1;
            if (n_commands == 0) {
                free(command_list);
                command_list = NULL;
            } else
                command_list = realloc(command_list, sizeof(command_t) * n_commands);
            pthread_mutex_unlock(&lock);
            // the receiver of this object is responsible for freeing it
            // fprintf(log_file, "fetch_one_command_output returned: %s, count: %d\n", ret, n_commands);
            return 1;
        }
    }
    pthread_mutex_unlock(&lock);
    return 0;
}


command_cls const command = {
    start_handler,
    stop_handler,
    schedule,
    fetch_one,
    release_one
};


/* internal ------------------------------------------------------ */


static void*
run(void* useless) {
    while(1) {
        pthread_mutex_lock(&lock);
        if (command_list &&
            n_commands > 0) {
            for (int i = 0; i < n_commands; i++) {
                if (command_list[i].state > -1)
                    continue;
                int ret = command_registry(&command_list[i]);
                if (ret == -2)
                    usleep(100000);
                break;
            }
        }
        pthread_mutex_unlock(&lock);
        usleep(10000);
        if (shutdown)
            break;
    }
    return NULL;
}
