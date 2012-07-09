/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | indexlog.c testsuites                                                |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-02-20 16:30                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CUnit/Basic.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "indexlog.h"

uint64_t alloc_times;
uint64_t free_times;
uint64_t realloc_times;

void
test_open_append_fetch(void)
{
    int lowest_fd = dup(0);
    close(lowest_fd);

    struct index_logger_s *il = index_logger_create("data/", "indexlog.bin", false);
    CU_ASSERT(il != NULL);

    struct index_log_item_s *ili = slab_alloc(sizeof *ili);
    ili->index_key = slab_alloc(sizeof *ili->index_key);
    ili->index_value = slab_alloc(sizeof *ili->index_value);

    ili->type = index_append;
    memset(ili->index_key, 0x01, sizeof(*ili->index_key));
    ili->index_value->crc32 = 0xcc;
    ili->index_value->block_id = 1;
    ili->index_value->data_size = 2;
    ili->index_value->offset = 3;
    CU_ASSERT(index_logger_append(il, ili) == RET_SUCCESS);
    CU_ASSERT(index_logger_free_item(ili) == RET_SUCCESS);
    CU_ASSERT(index_logger_flush(il) == RET_SUCCESS);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);

    int curr_lowest_fd = dup(0);
    close(curr_lowest_fd);
    CU_ASSERT(lowest_fd == curr_lowest_fd);
    int fd = open("./data/indexlog.bin", O_RDONLY);
    CU_ASSERT(fd == 3);
    struct stat sb;
    CU_ASSERT(stat("./data/indexlog.bin", &sb) == 0);
    CU_ASSERT(sb.st_size == 72);

    char correct_indexbin[] = {
        0x1d, 0x81, 0x09, 0x00, 0x9a, 0xcb, 0xcb, 0x1f, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0xcc, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    /* this might only works on 64-bit machines */
    char buf[72];
    CU_ASSERT(read(fd, buf, 72) == 72);
    CU_ASSERT(memcmp(buf, correct_indexbin, 72) == 0);

    close(fd);

    il = index_logger_open("data/", "indexlog.bin");
    CU_ASSERT(il != NULL);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili != NULL);
    CU_ASSERT(ili->type == index_append);
    char key[32] = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,};
    CU_ASSERT(memcmp(ili->index_key->key, key, 32) == 0);
    CU_ASSERT(ili->index_value->crc32 == 0xcc);
    CU_ASSERT(ili->index_value->block_id == 1);
    CU_ASSERT(ili->index_value->data_size == 2);
    CU_ASSERT(ili->index_value->offset == 3);

    index_logger_free_item(ili);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);
}

void
test_append_twice(void)
{
    struct index_logger_s *il = index_logger_create("data/", "indexlog.bin", false);
    CU_ASSERT(il != NULL);

    struct index_log_item_s *ili = slab_alloc(sizeof *ili);
    ili->index_key = slab_alloc(sizeof *ili->index_key);
    ili->index_value = slab_alloc(sizeof *ili->index_value);

    ili->type = index_append;
    memset(ili->index_key, 0x01, sizeof(*ili->index_key));
    ili->index_value->crc32 = 0xcc;
    ili->index_value->block_id = 1;
    ili->index_value->data_size = 2;
    ili->index_value->offset = 3;
    CU_ASSERT(index_logger_append(il, ili) == RET_SUCCESS);

    ili->type = index_del;
    memset(ili->index_key, 0x2, sizeof(*ili->index_key));
    ili->index_value->crc32 = 0xdd;
    ili->index_value->block_id = 9;
    ili->index_value->data_size = 8;
    ili->index_value->offset = 7;
    CU_ASSERT(index_logger_append(il, ili) == RET_SUCCESS);

    CU_ASSERT(index_logger_free_item(ili) == RET_SUCCESS);
    CU_ASSERT(index_logger_flush(il) == RET_SUCCESS);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);

    struct stat sb;
    CU_ASSERT(stat("./data/indexlog.bin", &sb) == 0);
    CU_ASSERT(sb.st_size == 132);

    il = index_logger_open("data/", "indexlog.bin");
    CU_ASSERT(il != NULL);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili != NULL);
    CU_ASSERT(ili->type == index_append);
    char key[32] = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,};
    CU_ASSERT(memcmp(ili->index_key->key, key, 32) == 0);
    CU_ASSERT(ili->index_value->crc32 == 0xcc);
    CU_ASSERT(ili->index_value->block_id == 1);
    CU_ASSERT(ili->index_value->data_size == 2);
    CU_ASSERT(ili->index_value->offset == 3);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(is_valid_mem(ili));
    CU_ASSERT(ili->type == index_del);
    char key2[32] = {0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,};
    CU_ASSERT(memcmp(ili->index_key->key, key2, 32) == 0);
    CU_ASSERT(ili->index_value->crc32 == 0xdd);
    CU_ASSERT(ili->index_value->block_id == 9);
    CU_ASSERT(ili->index_value->data_size == 8);
    CU_ASSERT(ili->index_value->offset == 7);
    index_logger_free_item(ili);
    CU_ASSERT(index_logger_fetch(il) == NULL);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);
}

void
test_append_full_block(void)
{
    struct index_logger_s *il = index_logger_create("data/", "indexlog.bin", false);
    CU_ASSERT(il != NULL);

    struct index_log_item_s *ili = slab_alloc(sizeof *ili);
    ili->index_key = slab_alloc(sizeof *ili->index_key);
    ili->index_value = slab_alloc(sizeof *ili->index_value);

    ili->type = index_append;

    for (int i=0; i<2000; i++) /* make sure that this will fill one log block to the full */
    {
        memset(ili->index_key, 0x00, sizeof(*ili->index_key));
        ili->index_key->key_num = i;
        ili->index_value->crc32 = i * 2;
        ili->index_value->block_id = i * 3;
        ili->index_value->data_size = i * 4;
        ili->index_value->offset = i * 5;
        CU_ASSERT(index_logger_append(il, ili) == RET_SUCCESS);
    }
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);
    index_logger_free_item(ili);

    il = index_logger_open("data/", "indexlog.bin");
    CU_ASSERT(il != NULL);

    for (unsigned int i=0; i<2000; i++)
    {
        ili = index_logger_fetch(il);
        CU_ASSERT(ili != NULL);
        CU_ASSERT(ili->type == index_append);
        CU_ASSERT(ili->index_key->key_num == i);
        CU_ASSERT(ili->index_value->crc32 == i * 2);
        CU_ASSERT(ili->index_value->block_id == i * 3);
        CU_ASSERT(ili->index_value->data_size == i * 4);
        CU_ASSERT(ili->index_value->offset == i * 5);
        index_logger_free_item(ili);
    }
    CU_ASSERT(index_logger_fetch(il) == NULL);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);
}

void
test_flush_twice(void)
{
    struct index_logger_s *il = index_logger_create("data/", "indexlog.bin", false);
    CU_ASSERT(il != NULL);
    struct index_log_item_s *ili = slab_alloc(sizeof *ili);
    ili->index_key = slab_alloc(sizeof *ili->index_key);
    ili->index_value = slab_alloc(sizeof *ili->index_value);

    ili->type = index_append;
    memset(ili->index_key, 0x01, sizeof(*ili->index_key));
    ili->index_value->crc32 = 0xcc;
    ili->index_value->block_id = 1;
    ili->index_value->data_size = 2;
    ili->index_value->offset = 3;
    CU_ASSERT(index_logger_append(il, ili) == RET_SUCCESS);
    CU_ASSERT(index_logger_flush(il) == RET_SUCCESS);

    CU_ASSERT(index_logger_flush(il) == RET_SUCCESS);

    ili->type = index_del;
    memset(ili->index_key, 0x2, sizeof(*ili->index_key));
    ili->index_value->crc32 = 0xdd;
    ili->index_value->block_id = 9;
    ili->index_value->data_size = 8;
    ili->index_value->offset = 7;
    CU_ASSERT(index_logger_append(il, ili) == RET_SUCCESS);
    CU_ASSERT(index_logger_free_item(ili) == RET_SUCCESS);
    CU_ASSERT(index_logger_flush(il) == RET_SUCCESS);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);

    struct stat sb;
    CU_ASSERT(stat("./data/indexlog.bin", &sb) == 0);
    CU_ASSERT(sb.st_size == 140);

    il = index_logger_open("data/", "indexlog.bin");
    CU_ASSERT(il != NULL);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili != NULL);
    CU_ASSERT(ili->type == index_append);
    char key[32] = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,};
    CU_ASSERT(memcmp(ili->index_key->key, key, 32) == 0);
    CU_ASSERT(ili->index_value->crc32 == 0xcc);
    CU_ASSERT(ili->index_value->block_id == 1);
    CU_ASSERT(ili->index_value->data_size == 2);
    CU_ASSERT(ili->index_value->offset == 3);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(is_valid_mem(ili));
    CU_ASSERT(ili->type == index_del);
    char key2[32] = {0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,};
    CU_ASSERT(memcmp(ili->index_key->key, key2, 32) == 0);
    CU_ASSERT(ili->index_value->crc32 == 0xdd);
    CU_ASSERT(ili->index_value->block_id == 9);
    CU_ASSERT(ili->index_value->data_size == 8);
    CU_ASSERT(ili->index_value->offset == 7);
    index_logger_free_item(ili);
    CU_ASSERT(index_logger_fetch(il) == NULL);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);
}

void
test_memory_test(void)
{
    CU_ASSERT(realloc_times == 0);
    CU_ASSERT(alloc_times == free_times);
}

int
check_indexlog()
{
    /* {{{ init CU suite check_indexlog */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_indexlog", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_open_append_fetch */
    if (CU_add_test(pSuite, "test_open_append_fetch", test_open_append_fetch) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_append_twice */
    if (CU_add_test(pSuite, "test_append_twice", test_append_twice) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_append_full_block */
    if (CU_add_test(pSuite, "test_append_full_block", test_append_full_block) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_flush_twice */
    if (CU_add_test(pSuite, "test_flush_twice", test_flush_twice) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_memory_test */
    if (CU_add_test(pSuite, "test_memory_test", test_memory_test) == NULL)
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
    check_indexlog();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    slab_stats();

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
