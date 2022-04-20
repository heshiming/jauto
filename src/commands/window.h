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

/*

 window.h contains shared definitions to support iteration and
 obtaining of key window information.

 "jvmti.h", "sds.h" must be included before this file.

 */

typedef struct {
    jobject instance;
    int     index;
    int     x;
    int     y;
    int     width;
    int     height;
    sds     title;
    sds     name;
    int     active;
} window_info_t;
