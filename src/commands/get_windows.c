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

#include <stdlib.h>
#include <string.h>

#include "jvmti.h"

#include "../command.h"
#include "../logger.h"
#include "window.h"


extern void                             prepare_jvmti_iteration(JNIEnv* env, jvmtiEnv* jvmti, jlong* tag);
extern jvmtiIterationControl JNICALL    heap_object_callback(jlong class_tag, jlong size, jlong* tag_ptr, void* user_data);


sds
jstring2sds(JNIEnv* env, jstring str_in) {
    sds output_ = sdsempty();
    JNI_CLEAR_EXCEPTION();
    if (str_in &&
        (*env)->GetObjectRefType(env, str_in)) {
        const char* str_in_ = (*env)->GetStringUTFChars(env, str_in, NULL);
        output_ = sdsnew(str_in_);
        (*env)->ReleaseStringUTFChars(env, str_in, str_in_);
        (*env)->DeleteLocalRef(env, str_in);
    }
    return output_;
}


void
get_baseclass(JNIEnv* env, jobject obj, sds* out_baseclass) {
    JNI_CLEAR_EXCEPTION();
    jclass class_cls = (*env)->FindClass(env, "java/lang/Class");
    jclass obj_cls = (*env)->FindClass(env, "java/lang/Object");
    jmethodID class_get_name = (*env)->GetMethodID(env, class_cls, "getName", "()Ljava/lang/String;");
    jmethodID class_get_superclass = (*env)->GetMethodID(env, class_cls, "getSuperclass", "()Ljava/lang/Class;");
    jmethodID get_class = (*env)->GetMethodID(env, obj_cls, "getClass", "()Ljava/lang/Class;");
    (*env)->DeleteLocalRef(env, class_cls);
    (*env)->DeleteLocalRef(env, obj_cls);

    // get class name for 'obj'
    sds cls_name = NULL;
    jobject cls = (*env)->CallObjectMethod(env, obj, get_class);
    jstring text = (*env)->CallObjectMethod(env, cls, class_get_name);
    if (text)
        cls_name = jstring2sds(env, text);

    jobject cls_c[16];
    int cls_n = 1;
    cls_c[0] = cls;

    sds ccls_name = NULL;

    if (strncmp(cls_name, "java", 4) != 0) {
        // class name not starting with "java", get superclass until the condition is met
        jobject target = cls;
        int to_break = 0;
        while (1) {
            jobject supcls = (*env)->CallObjectMethod(env, target, class_get_superclass);
            text = (*env)->CallObjectMethod(env, supcls, class_get_name);
            sds text_ = jstring2sds(env, text);
            if (strncmp(text_, "java", 4) == 0) {
                ccls_name = cls_name;
                cls_name = sdsnew(text_);
                to_break = 1;
            }
            sdsfree(text_);
            cls_c[cls_n] = supcls;
            cls_n += 1;
            if (to_break ||
                cls_n >= 16)
                break;
            target = supcls;
        }
    }

    if (!ccls_name)
        ccls_name = sdsempty();

    *out_baseclass = sdscatprintf(sdsempty(), "%s,%s", cls_name, ccls_name);

    for (int i = 0; i < cls_n; i++)
        (*env)->DeleteLocalRef(env, cls_c[i]);

    sdsfree(cls_name);
    sdsfree(ccls_name);
}


void
find_windows(JNIEnv* env, jvmtiEnv* jvmti, command_t* cmd, int* count_out, window_info_t** info_out) {
    JNI_CLEAR_EXCEPTION();
    jclass wnd_cls = (*env)->FindClass(env, cmd->arg_wnd_cn);
    jclass rec_cls = (*env)->FindClass(env, "java/awt/Rectangle");
    jmethodID wnd_get_title = (*env)->GetMethodID(env, wnd_cls, "getTitle", "()Ljava/lang/String;");
    jmethodID wnd_get_bounds = (*env)->GetMethodID(env, wnd_cls, "getBounds", "()Ljava/awt/Rectangle;");
    jmethodID wnd_is_visible = (*env)->GetMethodID(env, wnd_cls, "isVisible", "()Z");
    jmethodID wnd_is_active = (*env)->GetMethodID(env, wnd_cls, "isActive", "()Z");
    jfieldID rec_x = (*env)->GetFieldID(env, rec_cls, "x", "I");
    jfieldID rec_y = (*env)->GetFieldID(env, rec_cls, "y", "I");
    jfieldID rec_width = (*env)->GetFieldID(env, rec_cls, "width", "I");
    jfieldID rec_height = (*env)->GetFieldID(env, rec_cls, "height", "I");
    (*env)->DeleteLocalRef(env, rec_cls);

    int i;

    char* p_title = NULL;
    char* p_class = NULL;
    int p_index = -1;
    int p_is_active = 0;
    for (i = 0; i < cmd->n_args; i++) {
        if (strncmp_lit2(cmd->args[i], "window_title=") == 0)
            p_title = cmd->args[i] + strlen_lit("window_title=");
        else if (strncmp_lit2(cmd->args[i], "window_class=") == 0)
            p_class = cmd->args[i] + strlen_lit("window_class=");
        else if (strncmp_lit2(cmd->args[i], "is_active=1") == 0)
            p_is_active = 1;
        else if (strncmp_lit2(cmd->args[i], "window_index=") == 0) {
            char* start = cmd->args[i] + strlen_lit("window_index=");
            char* end;
            long tmp = strtol(start, &end, 10);
            if (tmp >= 0 &&
                end != start)
                p_index = tmp;
        }
    }

    jint count = 0;
    jobject* instances = NULL;
    jlong tag;
    prepare_jvmti_iteration(env, jvmti, &tag);
    (*jvmti)->IterateOverInstancesOfClass(jvmti, wnd_cls, JVMTI_HEAP_OBJECT_EITHER, heap_object_callback, &tag);
    (*jvmti)->GetObjectsWithTags(jvmti, 1, &tag, &count, &instances, NULL);
    (*env)->DeleteLocalRef(env, wnd_cls);


    if (count == 0) {
        *count_out = 0;
        *info_out = NULL;
    } else {
        jint act_count = 0;
        int index = 0;
        window_info_t* tmp_info = malloc(sizeof(window_info_t) * count);
        memset(tmp_info, 0, sizeof(window_info_t) * count);
        for (i = 0; i < count; i++) {
            int skip = 0;
            jobject instance = instances[i];
            jboolean visible = (*env)->CallBooleanMethod(env, instance, wnd_is_visible);
            if (!visible)
                skip = 1;
            if (p_index >= 0 &&     // target a single item by index
                p_index != index) {
                index++;
                skip = 1;
            }
            if (skip)
                continue;
            jstring title = NULL;
            if (wnd_get_title)
                title = (*env)->CallObjectMethod(env, instance, wnd_get_title);
            sds title_ = jstring2sds(env, title);
            jobject rbou = (*env)->CallObjectMethod(env, instance, wnd_get_bounds);
            int x = (*env)->GetIntField(env, rbou, rec_x);
            int y = (*env)->GetIntField(env, rbou, rec_y);
            int width = (*env)->GetIntField(env, rbou, rec_width);
            int height = (*env)->GetIntField(env, rbou, rec_height);
            int active = 0;
            sds comp_name = NULL;
            get_baseclass(env, instance, &comp_name);
            if (wnd_is_active) {
                jboolean active_ = (*env)->CallBooleanMethod(env, instance, wnd_is_active);
                active = active_;
            }
            if ((!p_is_active ||
                (p_is_active &&     // return only "active" windows
                active)) &&
                (!p_title ||        // return with title match
                (p_title &&
                strcasecmp(title_, p_title) == 0)) &&
                (!p_class ||
                p_class &&          // return with class substr match
                strstr(comp_name, p_class))) {
                tmp_info[act_count].instance    = (*env)->NewLocalRef(env, instance);
                tmp_info[act_count].index       = index;
                tmp_info[act_count].x           = x;
                tmp_info[act_count].y           = y;
                tmp_info[act_count].width       = width;
                tmp_info[act_count].height      = height;
                tmp_info[act_count].title       = sdsnew(title_);
                tmp_info[act_count].name        = sdsnew(comp_name);
                tmp_info[act_count].active      = active;
                act_count++;
            }
            index++;
            sdsfree(title_);
            sdsfree(comp_name);
            (*env)->DeleteLocalRef(env, rbou);
        }
        *count_out = act_count;
        *info_out = malloc(act_count * sizeof(window_info_t));
        memcpy(*info_out, tmp_info, act_count * sizeof(window_info_t));
        free(tmp_info);
        for (i = 0; i < count; i++)
            (*env)->DeleteLocalRef(env, instances[i]);
        (*jvmti)->Deallocate(jvmti, (unsigned char*)instances);
    }

}


int
get_windows(command_t* cmd, JNIEnv* env, jvmtiEnv* jvmti) {
    int count = 0;
    window_info_t* info = NULL;
    find_windows(env, jvmti, cmd, &count, &info);

    if (count == 0)
        cmd->output = sdsnew("none");
    else {
        cmd->output = sdsempty();
        for (int i = 0; i < count; i++)
            cmd->output = sdscatprintf(cmd->output, "%s,window:%d,x:%d,y:%d,w:%d,h:%d,act:%d,title:%s\n", info[i].name, info[i].index, info[i].x, info[i].y, info[i].width, info[i].height, info[i].active, info[i].title);
    }
    cmd->state = 1;

    for (int i = 0; i < count; i++) {
        if (info[i].instance)
            (*env)->DeleteLocalRef(env, info[i].instance);
        if (info[i].title)
            sdsfree(info[i].title);
        if (info[i].name)
            sdsfree(info[i].name);
    }
    if (info)
        free(info);

    return 0;
}
