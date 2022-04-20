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


void
get_bounds(JNIEnv* env, jobject comp, int* x, int* y, int* width, int* height) {
    JNI_CLEAR_EXCEPTION();
    jclass rec_cls = (*env)->FindClass(env, "java/awt/Rectangle");
    jclass comp_cls = (*env)->FindClass(env, "java/awt/Component");
    jmethodID component_get_bounds = (*env)->GetMethodID(env, comp_cls, "getBounds", "()Ljava/awt/Rectangle;");
    jfieldID rec_x = (*env)->GetFieldID(env, rec_cls, "x", "I");
    jfieldID rec_y = (*env)->GetFieldID(env, rec_cls, "y", "I");
    jfieldID rec_width = (*env)->GetFieldID(env, rec_cls, "width", "I");
    jfieldID rec_height = (*env)->GetFieldID(env, rec_cls, "height", "I");
    (*env)->DeleteLocalRef(env, rec_cls);
    (*env)->DeleteLocalRef(env, comp_cls);
    jobject rbou = (*env)->CallObjectMethod(env, comp, component_get_bounds);
    *x = (*env)->GetIntField(env, rbou, rec_x);
    *y = (*env)->GetIntField(env, rbou, rec_y);
    *width = (*env)->GetIntField(env, rbou, rec_width);
    *height = (*env)->GetIntField(env, rbou, rec_height);
    (*env)->DeleteLocalRef(env, rbou);
}


void
get_location(JNIEnv*env, jobject comp, int* sx, int* sy) {
    JNI_CLEAR_EXCEPTION();
    jclass comp_cls = (*env)->FindClass(env, "java/awt/Component");
    jclass pt_cls = (*env)->FindClass(env, "java/awt/Point");
    jmethodID comp_get_location = (*env)->GetMethodID(env, comp_cls, "getLocationOnScreen", "()Ljava/awt/Point;");
    jfieldID pt_x = (*env)->GetFieldID(env, pt_cls, "x", "I");
    jfieldID pt_y = (*env)->GetFieldID(env, pt_cls, "y", "I");
    (*env)->DeleteLocalRef(env, comp_cls);
    (*env)->DeleteLocalRef(env, pt_cls);
    jobject pt = (*env)->CallObjectMethod(env, comp, comp_get_location);
    if (pt) {
        *sx = (*env)->GetIntField(env, pt, pt_x);
        *sy = (*env)->GetIntField(env, pt, pt_y);
        (*env)->DeleteLocalRef(env, pt);
    }
}


void
_cat_comp_text(JNIEnv* env, jobject comp, jmethodID get_text_method, sds* comp_info) {
    JNI_CLEAR_EXCEPTION();
    jstring text = (*env)->CallObjectMethod(env, comp, get_text_method);
    sds text_ = jstring2sds(env, text);
    if (sdslen(text_) > 0) {
        int n_tokens = 0;
        sds* tokens = sdssplitlen(text_, sdslen(text_), "\n", 1, &n_tokens);
        sds etext_ = sdsempty();
        for (int i = 0; i < n_tokens; i++)
            etext_ = sdscatprintf(etext_, "%s%s", tokens[i], (i < n_tokens - 1)?"\\n":"");
        sdsfreesplitres(tokens, n_tokens);
        *comp_info = sdscatprintf(*comp_info, ",text:%s", etext_);
        sdsfree(etext_);
    } else
        *comp_info = sdscat(*comp_info, ",");
    sdsfree(text_);
}


sds
_enumerate_jtree(JNIEnv* env, jobject tree) {
    JNI_CLEAR_EXCEPTION();
    sds elems = sdsempty();
    jclass tree_cls = (*env)->FindClass(env, "javax/swing/JTree");
    jclass tpath_cls = (*env)->FindClass(env, "javax/swing/tree/TreePath");
    jclass utils_cls = (*env)->FindClass(env, "javax/swing/SwingUtilities");
    jclass rec_cls = (*env)->FindClass(env, "java/awt/Rectangle");
    jclass pt_cls = (*env)->FindClass(env, "java/awt/Point");
    jmethodID tree_get_row_count = (*env)->GetMethodID(env, tree_cls, "getRowCount", "()I");
    jmethodID tree_get_row_bounds = (*env)->GetMethodID(env, tree_cls, "getRowBounds", "(I)Ljava/awt/Rectangle;");
    jmethodID tree_get_row_path = (*env)->GetMethodID(env, tree_cls, "getPathForRow", "(I)Ljavax/swing/tree/TreePath;");
    jmethodID tpath_tostring = (*env)->GetMethodID(env, tpath_cls, "toString", "()Ljava/lang/String;");
    jmethodID utils_pt2screen = (*env)->GetStaticMethodID(env, utils_cls, "convertPointToScreen", "(Ljava/awt/Point;Ljava/awt/Component;)V");
    jmethodID new_pt = (*env)->GetMethodID(env, pt_cls, "<init>", "(II)V");
    jfieldID rec_x = (*env)->GetFieldID(env, rec_cls, "x", "I");
    jfieldID rec_y = (*env)->GetFieldID(env, rec_cls, "y", "I");
    jfieldID rec_width = (*env)->GetFieldID(env, rec_cls, "width", "I");
    jfieldID rec_height = (*env)->GetFieldID(env, rec_cls, "height", "I");
    jfieldID pt_x = (*env)->GetFieldID(env, pt_cls, "x", "I");
    jfieldID pt_y = (*env)->GetFieldID(env, pt_cls, "y", "I");
    (*env)->DeleteLocalRef(env, tree_cls);
    (*env)->DeleteLocalRef(env, tpath_cls);
    (*env)->DeleteLocalRef(env, rec_cls);

    jint n_rows = (*env)->CallIntMethod(env, tree, tree_get_row_count);
    for (int i = 0; i < n_rows; i++) {
        jobject rbou = (*env)->CallObjectMethod(env, tree, tree_get_row_bounds, i);
        int x = (*env)->GetIntField(env, rbou, rec_x);
        int y = (*env)->GetIntField(env, rbou, rec_y);
        int width = (*env)->GetIntField(env, rbou, rec_width);
        int height = (*env)->GetIntField(env, rbou, rec_height);
        jobject pt = (*env)->NewObject(env, pt_cls, new_pt, x, y);
        (*env)->CallStaticVoidMethod(env, utils_cls, utils_pt2screen, pt, tree);
        int sx = (*env)->GetIntField(env, pt, pt_x);
        int sy = (*env)->GetIntField(env, pt, pt_y);
        jobject path = (*env)->CallObjectMethod(env, tree, tree_get_row_path, i);
        jstring path_text = (*env)->CallObjectMethod(env, path, tpath_tostring);
        sds path_text_ = jstring2sds(env, path_text);
        (*env)->DeleteLocalRef(env, path);
        int n_tokens = 0;
        sds *tokens = sdssplitlen(path_text_, sdslen(path_text_), ",", 1, &n_tokens);
        sds revised_path = sdsempty();
        for (int j = 0; j < n_tokens; j++) {
            int catlen = sdslen(tokens[j]) - 1;
            if (j == n_tokens - 1)
                catlen--;
            revised_path = sdscatlen(revised_path, tokens[j] + 1, catlen);
            if (j < n_tokens - 1)
                revised_path = sdscat(revised_path, "/");
        }
        sdsfreesplitres(tokens, n_tokens);
        sdsfree(path_text_);
        int mid_x = sx + width / 2,
            mid_y = sy + height / 2;
        elems = sdscatprintf(elems, "javax.swing.JTree(row),%s,x:%d,y:%d,w:%d,h:%d,mx:%d,my:%d\n", revised_path, sx, sy, width, height, mid_x, mid_y);
        sdsfree(revised_path);
        (*env)->DeleteLocalRef(env, rbou);
        (*env)->DeleteLocalRef(env, pt);
    }
    (*env)->DeleteLocalRef(env, utils_cls);
    (*env)->DeleteLocalRef(env, pt_cls);
    return elems;
}


sds
_enumerate_jcombobox(JNIEnv* env, jobject combo) {
    JNI_CLEAR_EXCEPTION();
    sds elems = sdsempty();
    jclass obj_cls = (*env)->FindClass(env, "java/lang/Object");
    jclass cb_cls = (*env)->FindClass(env, "javax/swing/JComboBox");
    jmethodID cb_get_item_count = (*env)->GetMethodID(env, cb_cls, "getItemCount", "()I");
    jmethodID cb_get_selected_index = (*env)->GetMethodID(env, cb_cls, "getSelectedIndex", "()I");
    jmethodID cb_get_item_at = (*env)->GetMethodID(env, cb_cls, "getItemAt", "(I)Ljava/lang/Object;");
    jmethodID obj_tostring = (*env)->GetMethodID(env, obj_cls, "toString", "()Ljava/lang/String;");
    (*env)->DeleteLocalRef(env, obj_cls);
    (*env)->DeleteLocalRef(env, cb_cls);

    jint n_items = (*env)->CallIntMethod(env, combo, cb_get_item_count);
    jint sel_idx = (*env)->CallIntMethod(env, combo, cb_get_selected_index);
    for (int i = 0; i < n_items; i++) {
        jobject item = (*env)->CallObjectMethod(env, combo, cb_get_item_at, i);
        jstring item_text = (*env)->CallObjectMethod(env, item, obj_tostring);
        sds item_text_ = jstring2sds(env, item_text);
        elems = sdscatprintf(elems, "javax.swing.JComboBox(row),%d", i);
        if (i == sel_idx)
            elems = sdscat(elems, ",selected:y");
        elems = sdscatprintf(elems, ",text:%s\n", item_text_);
        sdsfree(item_text_);
        (*env)->DeleteLocalRef(env, item);
    }

    return elems;
}


sds
_enumerate_jlist(JNIEnv* env, jobject list) {
    JNI_CLEAR_EXCEPTION();
    sds elems = sdsempty();
    jclass obj_cls = (*env)->FindClass(env, "java/lang/Object");
    jclass list_cls = (*env)->FindClass(env, "javax/swing/JList");
    jclass lm_cls = (*env)->FindClass(env, "javax/swing/ListModel");
    jmethodID list_get_model = (*env)->GetMethodID(env, list_cls, "getModel", "()Ljavax/swing/ListModel;");
    jmethodID list_get_selected_index = (*env)->GetMethodID(env, list_cls, "getSelectedIndex", "()I");
    jmethodID lm_get_element_at = (*env)->GetMethodID(env, lm_cls, "getElementAt", "(I)Ljava/lang/Object;");
    jmethodID lm_get_size = (*env)->GetMethodID(env, lm_cls, "getSize", "()I");
    jmethodID obj_tostring = (*env)->GetMethodID(env, obj_cls, "toString", "()Ljava/lang/String;");
    (*env)->DeleteLocalRef(env, obj_cls);
    (*env)->DeleteLocalRef(env, list_cls);

    jint sel_idx = (*env)->CallIntMethod(env, list, list_get_selected_index);
    jobject lm = (*env)->CallObjectMethod(env, list, list_get_model);
    if (lm) {
        jint n_elems = (*env)->CallIntMethod(env, lm, lm_get_size);
        for (int i = 0; i < n_elems; i++) {
            jobject elem = (*env)->CallObjectMethod(env, lm, lm_get_element_at, i);
            if (!elem)
                continue;
            jstring elem_text = (*env)->CallObjectMethod(env, elem, obj_tostring);
            sds elem_text_ = jstring2sds(env, elem_text);
            elems = sdscatprintf(elems, "javax.swing.JList(row),%d", i);
            if (i == sel_idx)
                elems = sdscat(elems, ",selected:y");
            elems = sdscatprintf(elems, ",text:%s\n", elem_text_);
            sdsfree(elem_text_);
            (*env)->DeleteLocalRef(env, elem);
        }
        (*env)->DeleteLocalRef(env, lm);
    }

    return elems;
}


void
ui_enumerate_containers(JNIEnv* env, jobject container, int level, sds* output) {
    JNI_CLEAR_EXCEPTION();
    jclass cont_cls = (*env)->FindClass(env, "java/awt/Container");
    jclass comp_cls = (*env)->FindClass(env, "java/awt/Component");
    jclass btn_cls = (*env)->FindClass(env, "javax/swing/AbstractButton");
    jclass textc_cls = (*env)->FindClass(env, "javax/swing/text/JTextComponent");
    jclass label_cls = (*env)->FindClass(env, "javax/swing/JLabel");
    jclass pb_cls = (*env)->FindClass(env, "javax/swing/JProgressBar");
    jclass obj_cls = (*env)->FindClass(env, "java/lang/Object");
    jclass cb_cls = (*env)->FindClass(env, "javax/swing/JComboBox");
    jclass list_cls = (*env)->FindClass(env, "javax/swing/JList");
    jmethodID container_get_component = (*env)->GetMethodID(env, cont_cls, "getComponent", "(I)Ljava/awt/Component;");
    jmethodID container_get_component_count = (*env)->GetMethodID(env, cont_cls, "getComponentCount", "()I");
    jmethodID button_get_text = (*env)->GetMethodID(env, btn_cls, "getText", "()Ljava/lang/String;");
    jmethodID button_is_selected = (*env)->GetMethodID(env, btn_cls, "isSelected", "()Z");
    jmethodID textc_is_editable = (*env)->GetMethodID(env, textc_cls, "isEditable", "()Z");
    jmethodID textc_get_text = (*env)->GetMethodID(env, textc_cls, "getText", "()Ljava/lang/String;");
    jmethodID label_get_text = (*env)->GetMethodID(env, label_cls, "getText", "()Ljava/lang/String;");
    jmethodID comp_is_visible = (*env)->GetMethodID(env, comp_cls, "isVisible", "()Z");
    jmethodID pb_get_percent = (*env)->GetMethodID(env, pb_cls, "getPercentComplete", "()D");
    jmethodID cb_get_selected_item = (*env)->GetMethodID(env, cb_cls, "getSelectedItem", "()Ljava/lang/Object;");
    jmethodID cb_get_selected_index = (*env)->GetMethodID(env, cb_cls, "getSelectedIndex", "()I");
    jmethodID list_get_selected_value = (*env)->GetMethodID(env, list_cls, "getSelectedValue", "()Ljava/lang/Object;");
    jmethodID list_get_selected_index = (*env)->GetMethodID(env, list_cls, "getSelectedIndex", "()I");
    jmethodID obj_tostring = (*env)->GetMethodID(env, obj_cls, "toString", "()Ljava/lang/String;");
    (*env)->DeleteLocalRef(env, cont_cls);
    (*env)->DeleteLocalRef(env, comp_cls);
    (*env)->DeleteLocalRef(env, btn_cls);
    (*env)->DeleteLocalRef(env, textc_cls);
    (*env)->DeleteLocalRef(env, label_cls);
    (*env)->DeleteLocalRef(env, pb_cls);
    (*env)->DeleteLocalRef(env, obj_cls);
    (*env)->DeleteLocalRef(env, cb_cls);

    int n_comp = (*env)->CallIntMethod(env, container, container_get_component_count);

    for (int i = 0; i < n_comp; i++) {
        jobject comp = (*env)->CallObjectMethod(env, container, container_get_component, i);
        jboolean visible = (*env)->CallBooleanMethod(env, comp, comp_is_visible);
        if (!visible) {
            (*env)->DeleteLocalRef(env, comp);
            continue;
        }

        int x, y, width, height,
            sx = 0, sy = 0;
        get_bounds(env, comp, &x, &y, &width, &height);
        get_location(env, comp, &sx, &sy);
        int mid_x = sx + width / 2,
            mid_y = sy + height / 2;

        sds comp_name = NULL;
        get_baseclass(env, comp, &comp_name);
        char *comp_comma = strchr(comp_name, ',');

        if (strncmp(comp_name, "javax.swing.Box$Filler", comp_comma - comp_name - 1) == 0) {
            sdsfree(comp_name);
            (*env)->DeleteLocalRef(env, comp);
            continue;
        }

        sds comp_info = sdscatprintf(sdsempty(), "%s,x:%d,y:%d,w:%d,h:%d,mx:%d,my:%d", comp_name, sx, sy, width, height, mid_x, mid_y);
        sds extra_elems = sdsempty();

        if (strncmp(comp_name, "javax.swing.JButton", comp_comma - comp_name - 1) == 0 ||
            strncmp(comp_name, "javax.swing.JToggleButton", comp_comma - comp_name - 1) == 0 ||
            strncmp(comp_name, "javax.swing.JRadioButton", comp_comma - comp_name - 1) == 0 ||
            strncmp(comp_name, "javax.swing.JCheckBox", comp_comma - comp_name - 1) == 0) {
            jboolean selected = (*env)->CallBooleanMethod(env, comp, button_is_selected);
            comp_info = sdscatprintf(comp_info, ",selected:%s", selected?"y":"n");
            _cat_comp_text(env, comp, button_get_text, &comp_info);
        } else if (strncmp(comp_name, "javax.swing.JLabel", comp_comma - comp_name - 1) == 0) {
            _cat_comp_text(env, comp, label_get_text, &comp_info);
        } else if (strncmp(comp_name, "javax.swing.JProgressBar", comp_comma - comp_name - 1) == 0) {
            jdouble percent = (*env)->CallDoubleMethod(env, comp, pb_get_percent);
            comp_info = sdscatprintf(comp_info, ",progress:%d", (int) (percent * 100));
        } else if (strncmp(comp_name, "javax.swing.JTextField", comp_comma - comp_name - 1) == 0 ||
            strncmp(comp_name, "javax.swing.JPasswordField", comp_comma - comp_name - 1) == 0 ||
            strncmp(comp_name, "javax.swing.JTextPane", comp_comma - comp_name - 1) == 0) {
            jboolean editable = (*env)->CallBooleanMethod(env, comp, textc_is_editable);
            comp_info = sdscatprintf(comp_info, ",editable:%s", editable?"y":"n");
            _cat_comp_text(env, comp, textc_get_text, &comp_info);
        } else if (strncmp(comp_name, "javax.swing.JTree", comp_comma - comp_name - 1) == 0) {
            sds elems = _enumerate_jtree(env, comp);
            extra_elems = sdscat(extra_elems, elems);
            sdsfree(elems);
        } else if (strncmp(comp_name, "javax.swing.JComboBox", comp_comma - comp_name - 1) == 0) {
            jint sel_idx = (*env)->CallIntMethod(env, comp, cb_get_selected_index);
            comp_info = sdscatprintf(comp_info, ",index:%d", sel_idx);
            jobject sel_item = (*env)->CallObjectMethod(env, comp, cb_get_selected_item);
            if (sel_item) {
                jstring sel_text = (*env)->CallObjectMethod(env, sel_item, obj_tostring);
                sds sel_text_ = jstring2sds(env, sel_text);
                if (sdslen(sel_text_) > 0)
                    comp_info = sdscatprintf(comp_info, ",text:%s", sel_text_);
                sdsfree(sel_text_);
                (*env)->DeleteLocalRef(env, sel_item);
            }
            sds elems = _enumerate_jcombobox(env, comp);
            extra_elems = sdscat(extra_elems, elems);
            sdsfree(elems);
        } else if (strncmp(comp_name, "javax.swing.JList", comp_comma - comp_name - 1) == 0) {
            jint sel_idx = (*env)->CallIntMethod(env, comp, list_get_selected_index);
            comp_info = sdscatprintf(comp_info, ",index:%d", sel_idx);
            jobject sel_item = (*env)->CallObjectMethod(env, comp, list_get_selected_value);
            if (sel_item) {
                jstring sel_text = (*env)->CallObjectMethod(env, sel_item, obj_tostring);
                sds sel_text_ = jstring2sds(env, sel_text);
                if (sdslen(sel_text_) > 0)
                    comp_info = sdscatprintf(comp_info, ",text:%s", sel_text_);
                sdsfree(sel_text_);
                (*env)->DeleteLocalRef(env, sel_item);
            }
            sds elems = _enumerate_jlist(env, comp);
            extra_elems = sdscat(extra_elems, elems);
            sdsfree(elems);
        }

        *output = sdscatprintf(*output, "%s\n", comp_info);
        *output = sdscatprintf(*output, "%s", extra_elems);
        sdsfree(extra_elems);

        int n_sub = (*env)->CallIntMethod(env, comp, container_get_component_count);
        if (n_sub > 0) {
            ui_enumerate_containers(env, comp, level + 1, output);
        }

        sdsfree(comp_name);
        sdsfree(comp_info);
        (*env)->DeleteLocalRef(env, comp);
    }
}


int
list_ui_components(command_t* cmd, JNIEnv* env, jvmtiEnv* jvmti) {
    int count = 0;
    window_info_t* info = NULL;
    find_windows(env, jvmti, cmd, &count, &info);

    if (count == 0)
        cmd->output = sdsnew("none");
    else {
        jclass wnd_cls = (*env)->FindClass(env, cmd->arg_wnd_cn);
        jmethodID wnd_get_content_pane = (*env)->GetMethodID(env, wnd_cls, "getContentPane", "()Ljava/awt/Container;");
        (*env)->DeleteLocalRef(env, wnd_cls);

        cmd->output = sdsempty();
        for (int i = 0; i < count; i++) {
            jobject cpane = info[i].instance;
            cpane = (*env)->CallObjectMethod(env, info[i].instance, wnd_get_content_pane);
            cmd->output = sdscatprintf(cmd->output, "%s,window:%d,x:%d,y:%d,w:%d,h:%d,act:%d,title:%s\n", info[i].name, info[i].index, info[i].x, info[i].y, info[i].width, info[i].height, info[i].active, info[i].title);
            ui_enumerate_containers(env, cpane, 0, &cmd->output);
            (*env)->DeleteLocalRef(env, cpane);
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
