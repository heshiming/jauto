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

#ifndef COMMAND_H
#define COMMAND_H

#include "sds/sds.h"


typedef struct {
    sds     input;
    sds*    args;
    int     n_args;
    char*   arg_wnd_cn;
    sds     output_path;
    sds     output;
    int     state;
} command_t;


typedef struct {
    int x;
    int y;
    int width;
    int height;
} rectangle_t;


typedef struct {
    void    (* const start_handler)(void* jvm_);
    void    (* const stop_handler)();
    void    (* const schedule)(const char* cmd_in);
    int     (* const fetch_one)(command_t* cmd_out);
    void    (* const release_one)(command_t* cmd);
} command_cls;
extern command_cls const command;


#define REGISTER_COMMAND(NAME, QNAME) \
    if (strcmp(cmd->input, QNAME) == 0) { \
        ret = NAME(cmd, env, jvmti); \
    }

#define DECLARE_COMMAND(NAME) \
    int NAME(command_t* cmd, void* env, void* jvmti);


#define strlen_lit(str1) \
    sizeof(str1) / sizeof(str1[0]) - 1

#define strncmp_lit2(str1, str2) \
    strncmp(str1, str2, strlen_lit(str2))

#define JNI_CLEAR_EXCEPTION() \
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env)


#endif  // COMMAND_H
