/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | test cases for utils.c                                               |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-02 15:26                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CUnit/Basic.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#include "utils.h"

/* TODO add more multi-thread test cases for atomic series functions */

static sem_t sem;

static void *
adder(void *arg)
{
    atomic_uint_t *au = (atomic_uint_t*)arg;

    for (int counter=0; counter<10000000; counter++)
    {
        /*
         *if (atomic_uint_lte(au, 2))
         *   atomic_uint_set(au, 0);
         */
        atomic_uint_cas(au, 2, 0);
        atomic_uint_inc(au);
    }

    return NULL;
}

static void *
validater(void *arg)
{
    atomic_uint_t *au = (atomic_uint_t *)arg;
    atomic_uint_t c;

    atomic_uint_set(&c, 0);
    for (; sem_trywait(&sem) == -1; )
    {
        if (atomic_uint_equal(au, 3)) atomic_uint_inc(&c);
        if (atomic_uint_equal(au, 4)) atomic_uint_inc(&c);
        if (atomic_uint_lte(au, 3)) atomic_uint_inc(&c);
    }

    CU_ASSERT(atomic_uint_equal(&c, 0));
    printf("\n\n%ld\t%ld\n\n", au->counter, c.counter);

    return NULL;
}

void
test_mutil_equal(void)
{
    pthread_t tid1, tid2, tid3, tid4;
    atomic_uint_t au;

    sem_init(&sem, 0, 0);

    atomic_uint_set(&au, 0);
    pthread_create(&tid1, NULL, adder, (void*)&au);
    pthread_create(&tid2, NULL, adder, (void*)&au);
    pthread_create(&tid3, NULL, adder, (void*)&au);
    pthread_create(&tid4, NULL, validater, (void*)&au);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);

    sem_post(&sem);

    pthread_join(tid4, NULL);
}

void
test_atomic_int(void)
{
    atomic_int_t ai;

    atomic_int_set(&ai, 0);
    int64_t i = atomic_int_get(&ai);
    CU_ASSERT(i == 0);
    CU_ASSERT(atomic_int_equal(&ai, 0));

    atomic_int_add(&ai, 100);
    CU_ASSERT(atomic_int_equal(&ai, 100));

    atomic_int_sub(&ai, 201);
    CU_ASSERT(atomic_int_equal(&ai, -101));
    i = atomic_int_get(&ai);
    CU_ASSERT(i == -101);

    atomic_int_set(&ai, 100);
    CU_ASSERT(atomic_int_equal(&ai, 100));
    atomic_int_cas(&ai, 100, 200);
    CU_ASSERT(atomic_int_equal(&ai, 200));
}

void
test_atomic_uint(void)
{
    atomic_uint_t au;

    atomic_uint_set(&au, 0);
    uint64_t u = atomic_uint_get(&au);
    CU_ASSERT(u == 0);
    CU_ASSERT(atomic_uint_equal(&au, 0));
    atomic_uint_add(&au, 10);
    CU_ASSERT(atomic_uint_equal(&au, 10));
    atomic_uint_sub(&au, 1);
    CU_ASSERT(atomic_uint_equal(&au, 9));
    u = atomic_uint_get(&au);
    CU_ASSERT(u == 9);

    uint32_t max_u32 = 0;
    uint64_t correct_u64 = 0;
    max_u32 = ~0;
    correct_u64 = (uint64_t)max_u32 + 100;
    atomic_uint_set(&au, max_u32);
    CU_ASSERT(atomic_uint_equal(&au, max_u32));
    atomic_uint_add(&au, 100);
    CU_ASSERT(atomic_uint_equal(&au, correct_u64));

    atomic_uint_set(&au, 100);
    CU_ASSERT(atomic_uint_equal(&au, 100));
    atomic_uint_cas(&au, 100, 200);
    CU_ASSERT(atomic_uint_equal(&au, 200));
}

int
check_atomic()
{
    /* {{{ init CU suite check_atomic */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_atomic", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_atomic_uint */
    if (CU_add_test(pSuite, "test_atomic_uint", test_atomic_uint) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_atomic_int */
    if (CU_add_test(pSuite, "test_atomic_int", test_atomic_int) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    return 0;
}

int
check_multiple_thread()
{
    /* {{{ init CU suite check_multiple_thread */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_multiple_thread", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_mutil_equal */
    if (CU_add_test(pSuite, "test_mutil_equal", test_mutil_equal) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    return 0;
}

void
test_set_block(void)
{
    int flag;

    flag = fcntl(STDIN_FILENO, F_GETFL);
    assert(flag > 0);

    CU_ASSERT((flag & O_NONBLOCK) == 0);
    assert(set_nonblock(STDIN_FILENO) == 0);

    flag = fcntl(STDIN_FILENO, F_GETFL);
    CU_ASSERT((flag & O_NONBLOCK) != 0);

    assert(set_block(STDIN_FILENO) == 0);
    flag = fcntl(STDIN_FILENO, F_GETFL);
    CU_ASSERT((flag & O_NONBLOCK) == 0);
}

int
check_set_block()
{
    /* {{{ init CU suite check_set_block */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_set_block", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_set_block */
    if (CU_add_test(pSuite, "test_set_block", test_set_block) == NULL)
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

    check_atomic();
    check_set_block();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
