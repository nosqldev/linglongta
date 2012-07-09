/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * |                                                                  |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-22 13:43                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include "ev.h"

__thread int thread_id;
__thread struct ev_loop *local_loop;
struct ev_loop *main_loop;
int pipe1[2];
int pipe2[2];

struct worker_arg_s
{
    ev_io io;
    int tid;
    int fd;
};

static void
setnblock(int fd)
{
    int flag;

    flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;
    assert(fcntl(fd, F_SETFL, flag) == 0);
}

static void
pipe_handler(struct ev_loop *loop, ev_io *io, int event)
{
    char buf[1024] = {0};
    assert(loop != NULL);
    struct worker_arg_s *arg = (struct worker_arg_s*)io;
    ssize_t nread;

    assert(event == EV_READ);
    /*printf("thread %d ready to read\n", thread_id);*/
    nread = read(arg->fd, buf, sizeof buf);
    printf("worker [%d] recv[%zd]: %s\n", thread_id, nread, buf);
}

static void *
worker(void *arg)
{
    struct worker_arg_s *worker_arg = (struct worker_arg_s *)arg;
    thread_id = worker_arg->tid;
    local_loop = ev_loop_new(0);

    printf("thread %d install pipe id: %d\n", thread_id, worker_arg->fd);
    ev_io_init(&worker_arg->io, pipe_handler, worker_arg->fd, EV_READ);
    ev_io_start(local_loop, &worker_arg->io);

    ev_run(local_loop, 0);

    return NULL;
}

int
main(void)
{
    main_loop = ev_default_loop(0);
    assert(pipe(pipe1) == 0);
    assert(pipe(pipe2) == 0);

    thread_id = -1;
    pthread_t tid1, tid2;
    struct worker_arg_s warg1, warg2;
    bzero(&warg1, sizeof warg1);
    bzero(&warg2, sizeof warg2);
    warg1.tid = 1;
    warg2.tid = 2;
    warg1.fd = pipe1[0];
    warg2.fd = pipe2[0];

    setnblock(pipe1[0]);
    setnblock(pipe1[1]);
    setnblock(pipe2[0]);
    setnblock(pipe2[1]);

    pthread_create(&tid1, NULL, worker, (void*)&warg1);
    pthread_create(&tid2, NULL, worker, (void*)&warg2);

    write(pipe1[1], "abc", 3);
    write(pipe2[1], "zzz", 3);
    puts("1");
    write(pipe1[1], "abc", 3);
    write(pipe2[1], "zzz", 3);
    puts("2");

    sleep(1);
    write(pipe1[1], "abc", 3);
    write(pipe2[1], "zzz", 3);
    puts("3");
    write(pipe1[1], "abc", 3);
    write(pipe2[1], "zzz", 3);
    puts("4");

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
