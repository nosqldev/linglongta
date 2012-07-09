/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | test assign_thread.c                                                 |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-01 17:53                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <CUnit/Basic.h>

#include "assign_thread.h"

extern struct atomic_uint_s last_fd;
bool dispatch_query_task(struct ev_loop *local_loop, struct query_task_s *qt);
bool parse_query_request_head(struct query_task_s *task);

static volatile int go_flag = 0;
static pid_t assign_proc_id;

static void
go(int signum)
{
    (void)signum;
    go_flag = 1;
}

static int
init_assign_proc(void)
{
    signal(SIGHUP, go);
    signal(SIGCHLD, SIG_IGN);
    assign_proc_id = fork();
    if (assign_proc_id != 0)
    {
        init_datanode_vars();
        g_settings.worker_thread_cnt = 4;

        for ( ; go_flag == 0; )
        {
            usleep(10000);
        }
        return 0;
    }
    else
    {

        init_datanode_vars();
        g_settings.worker_thread_cnt = 4;

        pthread_t tid;
        pthread_create(&tid, NULL, new_assign_thread, NULL);
        sched_yield();
        sem_wait(&g_runtime_vars.sem_barrier);

        kill(getppid(), SIGHUP);
        pthread_join(tid, NULL);

        exit(0);
    }

    return 0;
}

static int
term_assign_proc(void)
{
    kill(assign_proc_id, SIGTERM);
    return 0;
}

static int
init_for_test(void)
{
    init_datanode_vars();
    g_settings.worker_thread_cnt = 4;
    return 0;
}

static void
test_parse_query_request_head_set(void)
{
    struct query_task_s qtask;

    memset(&qtask, 0, sizeof qtask);
    gettimeofday(&(qtask.connected_time), NULL);
    strcpy(qtask.query_ip, "1.2.3.4");
    qtask.state = s_parse_req_head;
    strcpy(qtask.query_request.query_head_buf, "set stringkey 1 2 3\r\n");
    qtask.have_read = strlen("set stringkey 1 2 3\r\n");
    bool ret = parse_query_request_head(&qtask);
    CU_ASSERT(ret == false);
    CU_ASSERT(qtask.error_log == NULL);
    CU_ASSERT(qtask.state == s_recv_req_data);
    CU_ASSERT(qtask.have_read == strlen("set stringkey 1 2 3\r\n"));
    CU_ASSERT(qtask.need_read == 5);
    CU_ASSERT(qtask.have_written == 0);
    CU_ASSERT(qtask.need_write == 0);
    CU_ASSERT(qtask.query_request.cmd_type == cmd_set);
    CU_ASSERT(qtask.query_request.parse_status == ps_succ);
    CU_ASSERT(qtask.query_request.flag == 1);
    CU_ASSERT(qtask.query_request.exptime == 2);
    CU_ASSERT(strcmp(qtask.query_request.key, "stringkey") == 0);
    CU_ASSERT(qtask.query_request.data_size == 5);
    CU_ASSERT(is_valid_mem(qtask.query_request.data));
    CU_ASSERT(strcmp(qtask.query_request.query_head_buf, "set stringkey 1 2 3\r\n") == 0);
    CU_ASSERT(qtask.query_request.query_head_buf_size == 21);

    memset(&qtask, 0, sizeof qtask);
    gettimeofday(&(qtask.connected_time), NULL);
    strcpy(qtask.query_ip, "1.2.3.4");
    qtask.state = s_parse_req_head;
    strcpy(qtask.query_request.query_head_buf, "set stringkey 1 2 3\r\nabc\r\n");
    qtask.have_read = strlen("set stringkey 1 2 3\r\nabc\r\n");
    ret = parse_query_request_head(&qtask);
    CU_ASSERT(ret == true);
    CU_ASSERT(qtask.error_log == NULL);
    CU_ASSERT(qtask.state == s_dispatch_query_task);
    CU_ASSERT(qtask.have_read == strlen("set stringkey 1 2 3\r\nabc\r\n"));
    CU_ASSERT(qtask.need_read == 0);
    CU_ASSERT(qtask.have_written == 0);
    CU_ASSERT(qtask.need_write == 0);
    CU_ASSERT(qtask.query_request.cmd_type == cmd_set);
    CU_ASSERT(qtask.query_request.parse_status == ps_succ);
    CU_ASSERT(qtask.query_request.flag == 1);
    CU_ASSERT(qtask.query_request.exptime == 2);
    CU_ASSERT(strcmp(qtask.query_request.key, "stringkey") == 0);
    CU_ASSERT(qtask.query_request.data_size == 3);
    CU_ASSERT(is_valid_mem(qtask.query_request.data));
    CU_ASSERT(strcmp(qtask.query_request.query_head_buf, "set stringkey 1 2 3\r\n") == 0);
    CU_ASSERT(qtask.query_request.query_head_buf_size == 21);
    CU_ASSERT(memcmp(qtask.query_request.data, "abc\r\n", strlen("abc\r\n")) == 0);
}

static void
test_parse_query_request_head_set_failed_miss_field(void)
{
    struct query_task_s qtask;

    memset(&qtask, 0, sizeof qtask);
    gettimeofday(&(qtask.connected_time), NULL);
    strcpy(qtask.query_ip, "1.2.3.4");
    qtask.state = s_parse_req_head;
    strcpy(qtask.query_request.query_head_buf, "set Stringkey 1 2 \r\n");
    qtask.have_read = strlen("set Stringkey 1 2 \r\n");

    bool need_loop = parse_query_request_head(&qtask);
    CU_ASSERT(need_loop == true);
    CU_ASSERT(qtask.error_log != NULL);
    CU_ASSERT(strstr(qtask.error_log, "parse request failed, cmd type: 1") != NULL);
    CU_ASSERT(qtask.state == s_return_failed);
}

static void
test_parse_query_request_head_set_failed_long(void)
{
    struct query_task_s qtask;

    memset(&qtask, 0, sizeof qtask);
    gettimeofday(&(qtask.connected_time), NULL);
    strcpy(qtask.query_ip, "1.2.3.4");
    qtask.state = s_parse_req_head;
    memcpy(qtask.query_request.query_head_buf, "11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111", 512);
    qtask.have_read = 512;

    bool need_loop = parse_query_request_head(&qtask);
    CU_ASSERT(need_loop == true);
    CU_ASSERT(qtask.error_log != NULL);
    CU_ASSERT(strstr(qtask.error_log, "Request header is longger than expected") != NULL);
}

static void
test_dispatch_query_task(void)
{
    struct query_task_s qtask;
    struct query_task_s *qtp;

    assert(sizeof(intptr_t) == sizeof qtp);

    memset(&qtask, 0, sizeof(qtask));
    qtask.sock_fd = 1;
    gettimeofday(&(qtask.connected_time), NULL);
    strcpy(qtask.query_ip, "1.2.3.4");
    qtask.state = s_recv_req_head;
    atomic_uint_set(&last_fd, 0);

    CU_ASSERT(atomic_uint_equal(&last_fd, 0));

    bool ret = dispatch_query_task(NULL, &qtask);
    CU_ASSERT(ret == false);
    CU_ASSERT(atomic_uint_equal(&last_fd, 1));
    ssize_t nread = read(g_pipe_fd[0], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp == &qtask);
    CU_ASSERT(qtp->sock_fd == 1);
    CU_ASSERT(qtp->state == s_recv_req_head);

    ret = dispatch_query_task(NULL, &qtask);
    CU_ASSERT(ret == false);
    CU_ASSERT(atomic_uint_equal(&last_fd, 2));
    nread = read(g_pipe_fd[2], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp == &qtask);
    CU_ASSERT(qtp->sock_fd == 1);
    CU_ASSERT(qtp->state == s_recv_req_head);

    ret = dispatch_query_task(NULL, &qtask);
    CU_ASSERT(ret == false);
    CU_ASSERT(atomic_uint_equal(&last_fd, 3));
    nread = read(g_pipe_fd[4], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp == &qtask);

    ret = dispatch_query_task(NULL, &qtask);
    CU_ASSERT(ret == false);
    CU_ASSERT(atomic_uint_equal(&last_fd, 4));
    nread = read(g_pipe_fd[6], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp == &qtask);

    ret = dispatch_query_task(NULL, &qtask);
    CU_ASSERT(ret == false);
    CU_ASSERT(atomic_uint_equal(&last_fd, 1));
    nread = read(g_pipe_fd[0], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp == &qtask);
}

void
test_new_assign_thread(void)
{
    atomic_uint_set(&last_fd, 0);
    pthread_t tid;
    pthread_create(&tid, NULL, new_assign_thread, NULL);

    sched_yield();
    sem_wait(&g_runtime_vars.sem_barrier);
    int connfd;
    struct sockaddr_in sa;

    sa.sin_family = AF_INET;
    sa.sin_port = htons(g_settings.accept_port);
    inet_aton("127.0.0.1", &sa.sin_addr);

    connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(connfd > 0);
    int ret = connect(connfd, &sa, sizeof sa);
    CU_ASSERT(ret == 0);
    close(connfd);
    struct query_task_s *qtp;
    puts("1111");
    ssize_t nread = read(g_pipe_fd[0], &qtp, sizeof qtp);
    puts("2222");
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp->sock_fd > 0);
    CU_ASSERT(qtp->state == s_recv_req_head);
    CU_ASSERT(strcmp("127.0.0.1", qtp->query_ip) == 0);
    slab_free(qtp);

    connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(connfd > 0);
    ret = connect(connfd, &sa, sizeof sa);
    CU_ASSERT(ret == 0);
    close(connfd);
    nread = read(g_pipe_fd[2], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp->sock_fd > 0);
    CU_ASSERT(qtp->state == s_recv_req_head);
    CU_ASSERT(strcmp("127.0.0.1", qtp->query_ip) == 0);
    slab_free(qtp);

    connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(connfd > 0);
    ret = connect(connfd, &sa, sizeof sa);
    CU_ASSERT(ret == 0);
    close(connfd);
    nread = read(g_pipe_fd[4], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp->sock_fd > 0);
    CU_ASSERT(qtp->state == s_recv_req_head);
    CU_ASSERT(strcmp("127.0.0.1", qtp->query_ip) == 0);
    slab_free(qtp);

    connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(connfd > 0);
    ret = connect(connfd, &sa, sizeof sa);
    CU_ASSERT(ret == 0);
    close(connfd);
    nread = read(g_pipe_fd[6], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp->sock_fd > 0);
    CU_ASSERT(qtp->state == s_recv_req_head);
    CU_ASSERT(strcmp("127.0.0.1", qtp->query_ip) == 0);
    slab_free(qtp);

    connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(connfd > 0);
    ret = connect(connfd, &sa, sizeof sa);
    CU_ASSERT(ret == 0);
    close(connfd);
    nread = read(g_pipe_fd[0], &qtp, sizeof qtp);
    CU_ASSERT(nread == sizeof qtp);
    CU_ASSERT(qtp->sock_fd > 0);
    CU_ASSERT(qtp->state == s_recv_req_head);
    CU_ASSERT(strcmp("127.0.0.1", qtp->query_ip) == 0);
    slab_free(qtp);
}

static void
test_unexpected_command(void)
{
    struct query_task_s qtask;

    memset(&qtask, 0, sizeof qtask);
    gettimeofday(&(qtask.connected_time), NULL);
    strcpy(qtask.query_ip, "1.2.3.4");
    qtask.state = s_parse_req_head;
    strcpy(qtask.query_request.query_head_buf, "adiasd asdfi\r\n");
    qtask.have_read = strlen("adiasd asdfi\r\n");
    bool need_loop = parse_query_request_head(&qtask);
    CU_ASSERT(need_loop == true);
    CU_ASSERT(qtask.error_log != NULL);
    CU_ASSERT(strstr(qtask.error_log, "parse request failed, cmd type: 0") != NULL);
    CU_ASSERT(qtask.state == s_return_failed);
}

int
check_assign_thread_utils(void)
{
    /* {{{ init CU suite check_assign_thread */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_assign_thread", init_for_test, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_dispatch_query_task */
    if (CU_add_test(pSuite, "test_dispatch_query_task", test_dispatch_query_task) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_query_request_head_set */
    if (CU_add_test(pSuite, "test_parse_query_request_head_set", test_parse_query_request_head_set) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_query_request_head_set_failed_miss_field */
    if (CU_add_test(pSuite, "test_parse_query_request_head_set_failed_miss_field", test_parse_query_request_head_set_failed_miss_field) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_query_request_head_set_failed_long */
    if (CU_add_test(pSuite, "test_parse_query_request_head_set_failed_long", test_parse_query_request_head_set_failed_long) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_unexpected_command */
    if (CU_add_test(pSuite, "test_unexpected_command", test_unexpected_command) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    return 0;
}

static void
test_handle_set_request(void)
{
    int connfd;
    struct sockaddr_in sa;

    sa.sin_family = AF_INET;
    sa.sin_port = htons(g_settings.accept_port);
    inet_aton("127.0.0.1", &sa.sin_addr);

    connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(connfd > 0);
    int ret = connect(connfd, &sa, sizeof sa);
    if (ret < 0)
    {
        printf("connect failed: %d - %s", errno, strerror(errno));
        exit(-1);
    }
    CU_ASSERT(ret == 0);
    ssize_t nwrite = write(connfd, "SET STRINGkey 1 2 3\r\n", strlen("set stringkey 1 2 3\r\n"));
    assert(nwrite == strlen("set stringkey 1 2 3\r\n"));
    CU_ASSERT(nwrite == strlen("set stringkey 1 2 3\r\n"));
    close(connfd);
    usleep(200 * 1000);

    FILE *fp = fopen("./datanode.log", "r+");
    assert(fp != NULL);
    char line[BUFSIZ];
    while (fgets(line, sizeof line, fp) != NULL);
    CU_ASSERT(strstr(line, "Remote reset connection unexpected, need_read =") != NULL);
    /* if this CU_ASSERT failed, re-run and tests for serveral times */
    fclose(fp);
}

static int
check_whole_assign_thread(void)
{
    /* {{{ init CU suite check_whole_assign_thread */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_whole_assign_thread", init_assign_proc, term_assign_proc);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_handle_set_request */
    if (CU_add_test(pSuite, "test_handle_set_request", test_handle_set_request) == NULL)
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

    check_assign_thread_utils();
    check_whole_assign_thread();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
