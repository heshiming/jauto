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
#include <string.h>
#include <stdlib.h>

#include "jvmti.h"

#include "logger.h"
#include "pipe_in.h"
#include "file_out.h"
#include "command.h"


extern int COMMAND_ENABLED;


JNIEXPORT jint JNICALL  Agent_OnLoad(JavaVM *jvm, char *options, void *reserved);
JNIEXPORT void JNICALL  Agent_OnUnload(JavaVM *vm);


static void
check_jvmti_errors(jvmtiEnv *jvmti, jvmtiError errnum, const char *str) {
    if (errnum != JVMTI_ERROR_NONE) {
        char *errnum_str;
        errnum_str = NULL;
        (void) (*jvmti)->GetErrorName(jvmti, errnum, &errnum_str);
        logger.error("jauto jvmti error: [%d] %s - %s\n", errnum, (errnum_str == NULL ? "Unknown": errnum_str), (str == NULL? "" : str));
    }
}


void
JNICALL VMStart(jvmtiEnv* jvmti, JNIEnv* jni) {
}


void
JNICALL VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    logger.info("jauto command processing enabled\n");
    COMMAND_ENABLED = 1;
}


void
JNICALL VMDeath(jvmtiEnv* jvmti, JNIEnv* jni) {
    COMMAND_ENABLED = 0;
    logger.info("jauto command processing disabled\n");
}


JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    logger.info("----------------\n");
    logger.info(" jauto started. \n");
    logger.info("----------------\n");

    int n_opt = 0;
    sds* opt_tok = NULL;
    if (options)
        opt_tok = sdssplitlen(options, strlen(options), ",", 1, &n_opt);

    char* pipe_path = NULL;
    if (n_opt > 0)
        pipe_path = opt_tok[0];
    init_pipe_in(pipe_path);
    init_file_out();
    command.start_handler((void*)jvm);

    if (n_opt > 0)
        sdsfreesplitres(opt_tok, n_opt);

    jvmtiEnv *jvmti;
    jint res;

    // Get the JVMTI environment
    res = (*jvm)->GetEnv(jvm, (void **) &jvmti, JVMTI_VERSION_1_0);
    if (res != JNI_OK || jvmti == NULL) {
        logger.error("jauto jvm error: unable to get access to JVMTI version 1.0\n");
        return JNI_ERR;
    }
    jvmtiEventCallbacks callbacks = {0};
    callbacks.VMStart = VMStart;
    callbacks.VMInit = VMInit;
    callbacks.VMDeath = VMDeath;
    (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_START, NULL);
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
    return JNI_OK;
}


JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm) {
    command.stop_handler();
    destroy_pipe_in();
    destroy_file_out();
    logger.info("jauto is shutdown.\n");
    logger.shutdown();
}
