/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "globals.h"
#include "queue.h"
#include "util.h"
#include "log.h"

int main(int argc, char **argv)
{
    int i;

    sp_log_set_level("debug");

    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);

    global_working_directory = "/tmp/queue_stress_test_dir";
    mkpath(global_working_directory);
    queue_init();

    struct timeval tv_init_done;
    gettimeofday(&tv_init_done, NULL);

    char nick[32] = "nick____";
    char tth[40] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ____";
    char remote_filename[64] = "share\\remote\\file____";
    char local_filename[64] = "/var/media/local/file____";
    uint64_t size;

    srand(time(0));

    for(i = 0; i < 1000; i++)
    {
	sprintf(nick + 4, "%04i", i);
	sprintf(tth + 26, "%04i", i);
	sprintf(remote_filename + 17, "%04i", i);
	sprintf(local_filename + 21, "%04i", i);
	size = random();

	queue_add(nick, remote_filename, size, local_filename, tth);
    }

    struct timeval tv_end;
    gettimeofday(&tv_end, NULL);

    queue_close();

    struct timeval d1;
    timersub(&tv_init_done, &tv_start, &d1);
    printf("queue_init() took %.2f seconds\n", (float)d1.tv_sec + (float)d1.tv_usec / 1000000.0);

    struct timeval d2;
    timersub(&tv_end, &tv_init_done, &d2);
    printf("1000 queue_add() took %.2f seconds\n", (float)d2.tv_sec + (float)d2.tv_usec / 1000000.0);

    return 0;
}

