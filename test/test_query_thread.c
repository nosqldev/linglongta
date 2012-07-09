/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | test query_thread.c                                                  |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-18 18:39                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <CUnit/Basic.h>

#include "global.h"
#include "iolayer.h"
#include "storage.h"
#include "memcached_protocol.h"
#include "query_thread.h"
#include "assign_thread.h"
#include "query_thread.h"

static int socket_fd[2];
static int pipe_fd[2];

static int
init_query_thread(void)
{
    pthread_t tid;
    struct worker_thread_arg_s *worker_arg;
    init_datanode_vars();
    g_settings.worker_thread_cnt = 4;
    pipe(pipe_fd);
    pipe(socket_fd);

    worker_arg = slab_alloc(sizeof *worker_arg);
    worker_arg->thread_id = 0;
    worker_arg->pipe_fd = pipe_fd[0];

    pthread_create(&tid, NULL, new_worker_thread, worker_arg);
    sem_wait(&g_runtime_vars.sem_barrier);

    return 0;
}

static void
test_query_thread(void)
{
    struct query_task_s *qtask;
    struct query_request_s *qrequest;
    char buffer[1024];

#define Set(arg_type, arg_key, arg_header, arg_data) do {   \
    qtask = slab_calloc(sizeof *qtask);                     \
    qrequest = &qtask->query_request;                       \
    qtask->sock_fd = socket_fd[1];                          \
    strcpy(qtask->query_ip, "127.1.1.1");                   \
    qtask->state = s_dispatch_query_task;                   \
    qrequest->parse_status = ps_succ;                       \
    qrequest->flag = qrequest->exptime = 0;                 \
    gettimeofday(&qtask->connected_time, NULL);             \
    qrequest->cmd_type = arg_type;                          \
    strcpy(qrequest->key, arg_key);                         \
    strcpy(qrequest->query_head_buf, arg_header);           \
    qrequest->query_head_buf_size = strlen(arg_header);     \
    if (arg_data != NULL) {                                 \
        qrequest->data = slab_alloc(strlen(arg_data) + 1);  \
        strcpy((char *)qrequest->data, arg_data);           \
        qrequest->data_size = strlen(arg_data)-2;           \
    }                                           \
    write(pipe_fd[1], &qtask, sizeof &qtask);   \
    bzero(buffer, 1024);                        \
    read(socket_fd[0], buffer, 1024);           \
    close(socket_fd[0]);                        \
    usleep(100*1000);                         \
} while (0)

    assert(pipe(socket_fd) == 0);
    Set(cmd_set, "str_key", "set str_key 0 0 3\r\n", "abc\r\n");
    CU_ASSERT(strcmp(buffer, "STORED\r\n") == 0);
    CU_ASSERT(!is_valid_mem(qtask));

#define Query(arg_type, arg_key, arg_header) do {           \
    qtask = slab_calloc(sizeof *qtask);                     \
    qrequest = &qtask->query_request;                       \
    qtask->sock_fd = socket_fd[1];                          \
    strcpy(qtask->query_ip, "127.1.1.1");                   \
    qtask->state = s_dispatch_query_task;                   \
    qrequest->parse_status = ps_succ;                       \
    qrequest->flag = qrequest->exptime = 0;                 \
    gettimeofday(&qtask->connected_time, NULL);             \
    qrequest->cmd_type = arg_type;                          \
    strcpy(qrequest->key, arg_key);                         \
    strcpy(qrequest->query_head_buf, arg_header);           \
    qrequest->query_head_buf_size = strlen(arg_header);     \
    write(pipe_fd[1], &qtask, sizeof &qtask);   \
    bzero(buffer, 1024);                        \
    usleep(10*1000); /*wait worker send 2 data fragments */ \
    read(socket_fd[0], buffer, 1024);           \
    close(socket_fd[0]);                        \
    usleep(100*1000);                         \
} while (0)

    assert(pipe(socket_fd) == 0);
    Query(cmd_get, "str_key", "get str_key\r\n");
    CU_ASSERT(strcmp(buffer, "VALUE str_key 0 3\r\nabc\r\n") == 0);
    CU_ASSERT(!is_valid_mem(qtask));

#define Delete(arg_key, arg_header) do {        \
    qtask = slab_calloc(sizeof *qtask);         \
    qrequest = &qtask->query_request;                       \
    qtask->sock_fd = socket_fd[1];                          \
    strcpy(qtask->query_ip, "127.1.1.1");                   \
    qtask->state = s_dispatch_query_task;                   \
    qrequest->parse_status = ps_succ;                       \
    qrequest->flag = qrequest->exptime = 0;                 \
    gettimeofday(&qtask->connected_time, NULL);             \
    qrequest->cmd_type = cmd_delete;                        \
    strcpy(qrequest->key, arg_key);                         \
    strcpy(qrequest->query_head_buf, arg_header);           \
    qrequest->query_head_buf_size = strlen(arg_header);     \
    write(pipe_fd[1], &qtask, sizeof &qtask);   \
    bzero(buffer, 1024);                        \
    usleep(10*1000); /*wait worker send 2 data fragments */ \
    read(socket_fd[0], buffer, 1024);           \
    close(socket_fd[0]);                        \
    usleep(100*1000);                         \
} while (0)

    assert(pipe(socket_fd) == 0);
    Delete("str_key", "delete str_key\r\n");
    CU_ASSERT(strcmp(buffer, "DELETED\r\n") == 0);
    CU_ASSERT(!is_valid_mem(qtask));
}

static int
check_whole_query_thread(void)
{
    /* {{{ init CU suite check_whole_query_thread */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_whole_query_thread", init_query_thread, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_query_thread */
    if (CU_add_test(pSuite, "test_query_thread", test_query_thread) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    return 0;
}

int
main(void)
{
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    check_whole_query_thread();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
