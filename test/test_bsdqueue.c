/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | test generic list                                                    |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-01-14 18:06                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CUnit/Basic.h>
#include "bsdqueue.h"

list_def(test_list);

struct list_item_name(test_list)
{
    int id;
    list_next_ptr(test_list);
};

void
test_bsdqueue()
{
    list_new(test_list, listhead);
    list_item_ptr(test_list) item_ptr;
    list_item_new(test_list, i);

    assert(listhead != NULL);
    CU_ASSERT(is_valid_mem(i));

    i->id = 100;
    CU_ASSERT(list_size(listhead) == 0);
    CU_ASSERT(list_last(listhead) == NULL);
    list_push(listhead, i);
    CU_ASSERT(*(listhead->stqh_last) == NULL);
    item_ptr = STAILQ_LAST(listhead, LINK_NEXT_PTR);
    CU_ASSERT(STAILQ_LAST(listhead, LINK_NEXT_PTR) == i);
    CU_ASSERT(list_last(listhead) == i);
    CU_ASSERT(list_first(listhead) == i);
    CU_ASSERT(list_size(listhead) == 1);

    list_pop(listhead, item_ptr);
    CU_ASSERT(item_ptr == i);
    CU_ASSERT(list_size(listhead) == 0);
    CU_ASSERT(list_first(listhead) == NULL);
    slab_free(item_ptr);
    CU_ASSERT(listhead->stqh_first == NULL);
    CU_ASSERT(*listhead->stqh_last == NULL);
    CU_ASSERT(listhead->stqh_first == *listhead->stqh_last);
    CU_ASSERT(list_empty(listhead));

    slab_free(listhead);
    /* TODO add alloc_times == free_times after merge improved slab branch */
}

void
test_list_operations(void)
{
    list_new(test_list, listhead);

    list_item_new(test_list, i);
    i->id = 1;
    list_push(listhead, i);
    CU_ASSERT(list_size(listhead) == 1);

    list_item_init(test_list, i);
    i->id = 2;
    list_push(listhead, i);
    CU_ASSERT(list_size(listhead) == 2);

    list_item_init(test_list, i);
    i->id = 3;
    list_push(listhead, i);
    CU_ASSERT(list_size(listhead) == 3);

    list_shift(listhead, i);
    CU_ASSERT(i->id == 1);
    CU_ASSERT(list_size(listhead) == 2);

    list_unshift(listhead, i);
    CU_ASSERT(list_size(listhead) == 3);

    list_lock(listhead);

    list_pop(listhead, i);
    CU_ASSERT(i->id == 3);
    CU_ASSERT(list_size(listhead) == 2);
    CU_ASSERT(is_valid_mem(i));
    slab_free(i);

    list_unlock(listhead);

    list_clear(listhead);
    list_item_init(test_list, i);
    i->id = 0xFF;
    list_push(listhead, i);

    list_destroy(listhead);
    /* TODO add alloc_times == free_times after merge improved slab branch */
}

void
test_list_foreach(void)
{
    list_new(test_list, listhead);
    list_item_ptr(test_list) i_ptr;

    list_addr(listhead, 0, i_ptr);
    CU_ASSERT(i_ptr == NULL);

    list_item_new(test_list, i);
    i->id = 1;
    list_push(listhead, i);
    CU_ASSERT(list_size(listhead) == 1);

    list_addr(listhead, 0, i_ptr);
    CU_ASSERT(i_ptr == i);
    list_addr(listhead, 1, i_ptr);
    CU_ASSERT(i_ptr == NULL);

    list_item_init(test_list, i);
    i->id = 2;
    list_push(listhead, i);
    CU_ASSERT(list_size(listhead) == 2);

    list_addr(listhead, 1, i_ptr);
    CU_ASSERT(is_valid_mem(i_ptr));
    CU_ASSERT(i_ptr->id == 2);
    CU_ASSERT(i_ptr == i);
    list_addr(listhead, 2, i_ptr);
    CU_ASSERT(i_ptr == NULL);

    list_item_init(test_list, i);
    i->id = 3;
    list_push(listhead, i);
    CU_ASSERT(list_size(listhead) == 3);

    list_addr(listhead, 0, i_ptr);
    CU_ASSERT(i_ptr->id == 1);
    CU_ASSERT(is_valid_mem(i_ptr));
    list_addr(listhead, 1, i_ptr);
    CU_ASSERT(i_ptr->id == 2);
    CU_ASSERT(is_valid_mem(i_ptr));
    list_addr(listhead, 2, i_ptr);
    CU_ASSERT(i_ptr->id == 3);
    CU_ASSERT(is_valid_mem(i_ptr));
    CU_ASSERT(i_ptr == i);
    list_addr(listhead, 3, i_ptr);
    CU_ASSERT(i_ptr == NULL);

    list_item_init(test_list, i);
    i->id = 4;
    list_push(listhead, i);
    CU_ASSERT(list_size(listhead) == 4);

    int cnt = 0;
    list_foreach(listhead, i_ptr)
    {
        cnt ++;
        CU_ASSERT(i_ptr->id == cnt);
    }

    cnt = 0;
    list_item_ptr(test_list) tmp;
    list_foreach_safe(listhead, i_ptr, tmp)
    {
        cnt ++;
        CU_ASSERT(i_ptr->id == cnt);
    }

    list_destroy(listhead);
}

int
check_bsdqueue()
{
    /* {{{ init CU suite check_bsdqueue */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_bsdqueue", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_bsdqueue */
    if (CU_add_test(pSuite, "test_bsdqueue", test_bsdqueue) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_list_operations */
    if (CU_add_test(pSuite, "test_list_operations", test_list_operations) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_list_foreach */
    if (CU_add_test(pSuite, "test_list_foreach", test_list_foreach) == NULL)
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

    slab_init(0, 1.125);

    check_bsdqueue();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    slab_stats();

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
