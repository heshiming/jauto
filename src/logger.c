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
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "logger.h"
#include "sds/sds.h"


static FILE* log_file = NULL;


static void
log_output(const char* tag, const char* message, va_list args) {
    if (!log_file) {
        sds path = sdscatprintf(sdsempty(), "%s/.jauto", getenv("HOME"));
        struct stat st = {0};
        if (stat(path, &st) == -1)
            mkdir(path, 0700);
        path = sdscat(path, "/jauto.log");
        log_file = fopen(path, "a+");
    }
    time_t now;
    time(&now);
    char date[20];
    strftime(date, 20, "%F %T", localtime(&now));
    sds line = sdsempty();
    line = sdscatprintf(line, "%s [%s] ", date, tag);
    line = sdscatvprintf(line, message, args);
    fwrite(line, sdslen(line), 1, log_file);
    printf("%s", line);
    sdsfree(line);
    fflush(log_file);
}


static void
info(const char* message, ...) {
    va_list args;
    va_start(args, message);
    log_output("I", message, args);
    va_end(args);
}


static void
error(const char* message, ...) {
    va_list args;
    va_start(args, message);
    log_output("E", message, args);
    va_end(args);
}


static void
shutdown() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}


logger_cls const logger = {
    info,
    error,
    shutdown
};
