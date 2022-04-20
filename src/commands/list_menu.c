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


sds     jstring2sds(JNIEnv* env, jstring str_in);
void    find_windows(JNIEnv* env, jvmtiEnv* jvmti, command_t* cmd, int* count_out, window_info_t** info_out);
void    get_baseclass(JNIEnv* env, jobject obj, sds* out_baseclass);
void    get_bounds(JNIEnv* env, jobject comp, int* x, int* y, int* width, int* height);


void
enumerate_menu(JNIEnv* env, jobject window, const char* wnd_class, sds* output) {
    JNI_CLEAR_EXCEPTION();
    jclass frm_cls = (*env)->FindClass(env, wnd_class);
    jclass comp_cls = (*env)->FindClass(env, "java/awt/Component");
    jclass btn_cls = (*env)->FindClass(env, "javax/swing/AbstractButton");
    jclass mnb_cls = (*env)->FindClass(env, "javax/swing/JMenuBar");
    jclass mn_cls = (*env)->FindClass(env, "javax/swing/JMenu");
    jclass pt_cls = (*env)->FindClass(env, "java/awt/Point");
    jmethodID frm_get_jmenubar = (*env)->GetMethodID(env, frm_cls, "getJMenuBar", "()Ljavax/swing/JMenuBar;");
    jmethodID mnb_get_menucount = (*env)->GetMethodID(env, mnb_cls, "getMenuCount", "()I");
    jmethodID mnb_get_menu = (*env)->GetMethodID(env, mnb_cls, "getMenu", "(I)Ljavax/swing/JMenu;");
    jmethodID btn_get_text = (*env)->GetMethodID(env, btn_cls, "getText", "()Ljava/lang/String;");
    jmethodID btn_is_selected = (*env)->GetMethodID(env, btn_cls, "isSelected", "()Z");
    jmethodID comp_get_location = (*env)->GetMethodID(env, comp_cls, "getLocationOnScreen", "()Ljava/awt/Point;");
    jmethodID mn_get_item_count = (*env)->GetMethodID(env, mn_cls, "getItemCount", "()I");
    jmethodID mn_get_item = (*env)->GetMethodID(env, mn_cls, "getItem", "(I)Ljavax/swing/JMenuItem;");
    jmethodID mn_is_pop = (*env)->GetMethodID(env, mn_cls, "isPopupMenuVisible", "()Z");
    jfieldID pt_x = (*env)->GetFieldID(env, pt_cls, "x", "I");
    jfieldID pt_y = (*env)->GetFieldID(env, pt_cls, "y", "I");
    (*env)->DeleteLocalRef(env, frm_cls);
    (*env)->DeleteLocalRef(env, comp_cls);
    (*env)->DeleteLocalRef(env, btn_cls);
    (*env)->DeleteLocalRef(env, mnb_cls);
    (*env)->DeleteLocalRef(env, mn_cls);
    (*env)->DeleteLocalRef(env, pt_cls);

    if (frm_get_jmenubar) {
        jobject menubar = (*env)->CallObjectMethod(env, window, frm_get_jmenubar);
        if (menubar) {
            jint n_menu = (*env)->CallIntMethod(env, menubar, mnb_get_menucount);
            for (int i = 0; i < n_menu; i++) {
                jobject menu = (*env)->CallObjectMethod(env, menubar, mnb_get_menu, i);
                if (!menu)
                    continue;
                jstring menu_text = (*env)->CallObjectMethod(env, menu, btn_get_text);
                jobject menu_pt = (*env)->CallObjectMethod(env, menu, comp_get_location);
                jboolean menu_popv = (*env)->CallBooleanMethod(env, menu, mn_is_pop);
                sds menu_text_ = jstring2sds(env, menu_text);
                jint n_item = (*env)->CallIntMethod(env, menu, mn_get_item_count);
                int x, y, width, height,
                    sx = 0, sy = 0;
                get_bounds(env, menu, &x, &y, &width, &height);
                if (menu_pt) {
                    sx = (*env)->GetIntField(env, menu_pt, pt_x);
                    sy = (*env)->GetIntField(env, menu_pt, pt_y);
                }
                int mid_x = sx + width / 2,
                    mid_y = sy + height / 2;
                *output = sdscatprintf(*output, "%s,x:%d,y:%d,w:%d,h:%d,mx:%d,my:%d,pop:%s\n", menu_text_, x, y, width, height, mid_x, mid_y, menu_popv?"y":"n");
                for (int j = 0; j < n_item; j++) {
                    jobject item = (*env)->CallObjectMethod(env, menu, mn_get_item, j);
                    if (!item)
                        continue;
                    jstring item_text = (*env)->CallObjectMethod(env, item, btn_get_text);
                    jobject item_pt = (*env)->CallObjectMethod(env, item, comp_get_location);
                    jboolean item_sel = (*env)->CallBooleanMethod(env, item, btn_is_selected);
                    int x, y, width, height,
                        sx = 0, sy = 0;
                    get_bounds(env, menu, &x, &y, &width, &height);
                    if (item_pt) {
                        sx = (*env)->GetIntField(env, item_pt, pt_x);
                        sy = (*env)->GetIntField(env, item_pt, pt_y);
                    }
                    int mid_x = sx + width / 2,
                        mid_y = sy + height / 2;
                    sds item_text_ = jstring2sds(env, item_text);
                    sds entry_text = sdscatprintf(sdsempty(), "%s/%s", menu_text_, item_text_);
                    if (item_sel)
                        *output = sdscatprintf(*output, "%s,x:%d,y:%d,w:%d,h:%d,mx:%d,my:%d,selected:%d\n", entry_text, x, y, width, height, mid_x, mid_y, item_sel);
                    else
                        *output = sdscatprintf(*output, "%s,x:%d,y:%d,w:%d,h:%d,mx:%d,my:%d\n", entry_text, x, y, width, height, mid_x, mid_y);
                    sdsfree(item_text_);
                    sdsfree(entry_text);
                    if (item_pt)
                        (*env)->DeleteLocalRef(env, item_pt);
                    (*env)->DeleteLocalRef(env, item);
                }
                sdsfree(menu_text_);
                (*env)->DeleteLocalRef(env, menu_pt);
                (*env)->DeleteLocalRef(env, menu);
            }
            (*env)->DeleteLocalRef(env, menubar);
        }
    }
}


int
list_menu(command_t* cmd, JNIEnv* env, jvmtiEnv* jvmti) {
    int count = 0;
    window_info_t* info = NULL;
    find_windows(env, jvmti, cmd, &count, &info);

    if (count == 0)
        cmd->output = sdsnew("none");
    else {
        cmd->output = sdsempty();
        for (int i = 0; i < count; i++) {
            cmd->output = sdscatprintf(cmd->output, "%s,window:%d,x:%d,y:%d,w:%d,h:%d,act:%d,title:%s\n", info[i].name, info[i].index, info[i].x, info[i].y, info[i].width, info[i].height, info[i].active, info[i].title);
            enumerate_menu(env, info[i].instance, cmd->arg_wnd_cn, &cmd->output);
        }
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
