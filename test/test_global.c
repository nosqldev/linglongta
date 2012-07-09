/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | global module test                                                   |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-28 17:12                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CUnit/Basic.h>

#include "global.h"

extern size_t mem_limit;

void
test_init_datanode_vars(void)
{
    CU_ASSERT(strcmp(g_settings.index_path, "./data/") != 0);
    atomic_uint_set(&g_runtime_vars.io_stats.disk_read_bytes, 1);
    CU_ASSERT(g_runtime_vars.io_stats.disk_read_bytes.counter == 1);

    init_datanode_vars();
    CU_ASSERT(strcmp(g_settings.index_path, "./data/") == 0);
    CU_ASSERT(g_runtime_vars.io_stats.disk_read_bytes.counter == 0);
    CU_ASSERT(g_runtime_vars.io_stats.disk_write_bytes.counter == 0);
    CU_ASSERT(mem_limit == (1024 * 1024 * 1024 * 4L));
    CU_ASSERT(g_loop != NULL);

    CU_ASSERT(g_runtime_vars.cache_size == 0);
    set_global_cache_size(10);
    CU_ASSERT(g_runtime_vars.cache_size == 10);
    ssize_t ret;
    cmp_global_cache_size(12, ret);
    CU_ASSERT(ret == -2);

    for (size_t i=0; i<g_settings.worker_thread_cnt; i++)
    {
        CU_ASSERT(g_pipe_fd[i*2] > 0);
        CU_ASSERT(g_pipe_fd[i*2+1] > 0);
    }
}

int
check_init()
{
    /* {{{ init CU suite check_init */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_init", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_init_datanode_vars */
    if (CU_add_test(pSuite, "test_init_datanode_vars", test_init_datanode_vars) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    return 0;
}

void
test_others()
{
    CU_ASSERT(IP_STR_LEN == 16);

    atomic_uint_set(&g_runtime_vars.io_stats.disk_read_bytes, 1);
    uint64_t u = atomic_uint_get(&g_runtime_vars.io_stats.disk_read_bytes);
    CU_ASSERT(u == 1);
    atomic_uint_add(&g_runtime_vars.io_stats.disk_read_bytes, 10);
    CU_ASSERT(atomic_uint_equal(&g_runtime_vars.io_stats.disk_read_bytes, 11));
    CU_ASSERT(atomic_uint_equal(&g_runtime_vars.io_stats.disk_read_bytes, 11) == 1);
}

int
check_others()
{
    /* {{{ init CU suite check_others */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_others", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_others */
    if (CU_add_test(pSuite, "test_others", test_others) == NULL)
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

    check_init();
    check_others();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
