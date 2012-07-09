/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | test iolayer.c                                                       |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-01-16 00:39                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CUnit/Basic.h>
#include <errno.h>
#include <string.h>
#include "iolayer.h"

void *
init_normal_listener(void *arg)
{
    int listenfd;
    struct sockaddr_in server_addr;
    int flag = 1;
    (void)arg;

    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(listenfd > 0);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof flag);
    bzero(&server_addr, sizeof server_addr);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(3006);
    assert(bind(listenfd, (struct sockaddr *) &server_addr, sizeof server_addr) == 0);
    assert(listen(listenfd, 5) == 0);

    int sockfd = accept(listenfd, NULL, NULL);
    assert(sockfd > 0);

    char buffer[1024] = {0};
    assert(strlen(buffer) == 0);
    read(sockfd, buffer, 4);
    CU_ASSERT(memcmp(buffer, "hehe", 4) == 0);
    ssize_t nwrite = write(sockfd, "*hello*", 8);
    CU_ASSERT(nwrite == 8);

    close(sockfd);
    close(listenfd);

    return NULL;
}

void
test_fast_connect(void)
{
    pthread_t tid;
    pthread_create(&tid, NULL, init_normal_listener, NULL);
    char buf[1024] = {0};

    usleep(1000 * 10);

    int fd = timed_connect(tcp_pton("127.0.0.1"), htons(3006), 0, 100);
    CU_ASSERT(fd > 0);
    CU_ASSERT(write(fd, "hehe", 4) == 4);
    ssize_t nread = read(fd, buf, 8);
    CU_ASSERT(nread == 8);
    close(fd);

    pthread_join(tid, NULL);
}

int
check_timed_connect(void)
{
    /* {{{ init CU suite check_timed_connect */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_timed_connect", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_fast_connect */
    if (CU_add_test(pSuite, "test_fast_connect", test_fast_connect) == NULL)
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

    check_timed_connect();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
