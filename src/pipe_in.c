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

#include "pipe_in.h"
#include "command.h"
#include "logger.h"


const char  default_pipe_path[] = "/tmp/jauto.in";
pthread_t   pipe_reader_th;
int         pipe_shutdown = 0;

#define READ_BUFFER_SIZE    256

void*   pipe_in(void* fd);
void*   pipe_out(void* fd);


void
init_pipe_in(const char* target_pipe_path) {
    sds pipe_path = sdsnew(target_pipe_path?target_pipe_path:default_pipe_path);
    logger.info("pipe_in: going with input path: [%s]\n", pipe_path);
    pthread_create(&pipe_reader_th, NULL, &pipe_in, (void*)pipe_path);
}


void
destroy_pipe_in() {
    pipe_shutdown = 1;
    pthread_join(pipe_reader_th, NULL);
}


void*
pipe_in(void* arg) {
    sds pipe_path = (sds)arg;
    // portions from http://ashishgrover.com/linux-multi-threading-fifos-or-named-pipes/
    mkfifo(pipe_path, 0666);
    int     fd_ = open(pipe_path, O_RDONLY|O_NONBLOCK);
    int     err = 0, size = 0, rsize = 0;
    char    buf[READ_BUFFER_SIZE];
    sds     cmd = sdsempty();
    fd_set  readset;
    struct timeval tv;
    // Implement the receiver loop
    while(1) {
        // Initialize the set
        FD_ZERO(&readset);
        FD_SET(fd_, &readset);
        // Now, check for readability
        tv.tv_sec = 0;      // select will reset tv upon timeout
        tv.tv_usec = 100000;
        err = select(fd_ + 1, &readset, NULL, NULL, &tv);
        // logger.info("marker 1: %d\n", err);
        if (err > 0 &&
            FD_ISSET(fd_, &readset)) {
            // Clear flags
            FD_CLR(fd_, &readset);
            rsize = read(fd_, buf, READ_BUFFER_SIZE);
            if (rsize < 0)
                logger.error("pipe_in: read error: %d, errno: %d\n", rsize, errno);
            else if (rsize > 0) {
                cmd = sdscatlen(cmd, buf, rsize);
                int cmd_len = sdslen(cmd);
                int cmd_l = 0;
                for (int i = 0; i < cmd_len; i++) {
                    if (cmd[i] == 10) {
                        // return char, end of command
                        sds cmd_ = sdsnewlen(cmd + cmd_l, i - cmd_l);
                        command.schedule(cmd_);
                        sdsfree(cmd_);
                        cmd_l = i + 1;
                    }
                }
                if (cmd_l < cmd_len) {
                    // more characters left behind, but no return char
                    // keep the left over chars and wait for return
                    sdsrange(cmd, cmd_l, cmd_len);
                } else {
                    // all commands scheduled, clear cmd
                    cmd[0] = '\0';
                    sdsupdatelen(cmd);
                }
            } else {
                // https://stackoverflow.com/a/52017025/108574
                int refd = open(pipe_path, O_RDONLY|O_NONBLOCK);
                if (refd == -1) {
                    logger.error("pipe_in: re-opening of pipe has failed, errno: %d, shutting down input entirely.\n", errno);
                    break;
                }
                close(fd_);
                fd_ = refd;
            }
        } else if (err != 0 /* not a timeout */)
            logger.error("pipe_in: error on select: %d, errno: %d\n", err, errno);
        usleep(10000);
        if (pipe_shutdown)
            break;
    }
    sdsfree(cmd);
    close(fd_);
    sdsfree(pipe_path);
    return NULL;
}
