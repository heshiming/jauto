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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "logger.h"
#include "command.h"
#include "file_out.h"


pthread_t   file_out_th;
int         file_out_shutdown = 0;


void*   file_out(void* useless);


void
init_file_out() {
    pthread_create(&file_out_th, NULL, &file_out, NULL);
}


void
destroy_file_out() {
    file_out_shutdown = 1;
    pthread_join(file_out_th, NULL);
}


void*
file_out(void* useless) {
    while(1) {
        command_t cmd;
        int nf = command.fetch_one(&cmd);
        if (nf) {
            if (cmd.output_path &&
                sdslen(cmd.output_path) > 0 &&
                cmd.output &&
                sdslen(cmd.output) > 0) {
                FILE* out_fh = fopen(cmd.output_path, "w");
                int written = fwrite(cmd.output, 1, sdslen(cmd.output), out_fh);
                fclose(out_fh);
            } else
                logger.info("file_out: discarded command %s due to no output or output_path\n", cmd.input);
            command.release_one(&cmd);
        }
        usleep(10000);
        if (file_out_shutdown)
            break;
    }
    return NULL;
}
