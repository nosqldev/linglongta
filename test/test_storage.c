/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | test storage                                                         |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-29 10:54                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CUnit/Basic.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "storage.h"
#include "indexlog.h"

/* TODO
 * 1. check_find_block
 * 2. check_delete_block
 * 3. check_update_block
 */

void
test_storage_init(void)
{
    struct bucket_storage_t *bsp;
    struct stat sb;
    int lowest_fd = dup(1);
    close(lowest_fd);

    int ret = open_storage("./data/index.db", 1024, &bsp);
    CU_ASSERT(ret == 0);
    CU_ASSERT(stat("./data", &sb) == 0);
    CU_ASSERT(access("./data/128.dat", F_OK | R_OK | W_OK) == 0);
    CU_ASSERT(access("./data/256.dat", F_OK | R_OK | W_OK) == 0);
    CU_ASSERT(access("./data/512.dat", F_OK | R_OK | W_OK) == 0);
    CU_ASSERT(access("./data/1024.dat", F_OK | R_OK | W_OK) == 0);
    CU_ASSERT(access("./data/2048.dat", F_OK | R_OK | W_OK) != 0);

    /* TODO check dat file header */

    ret = close_storage(bsp);
    CU_ASSERT(ret == 0);
    int newlowest_fd = dup(1);
    close(newlowest_fd);
    CU_ASSERT(lowest_fd == newlowest_fd); /* to ensure that all fd opened by storage have been closed */
    system("rm -rf data");
}

void
test_storage_update(void)
{
    struct bucket_storage_t *bsp;
    int ret = open_storage("./data/index.db", 1024, &bsp);
    CU_ASSERT(ret == 0);

    void *value_ptr;
    size_t vlen;
    struct stat sb;

    ret = find_block(bsp, "str_key", 8, &value_ptr, &vlen);
    CU_ASSERT(ret != 0);
    CU_ASSERT(value_ptr == NULL);

    value_ptr = slab_calloc(256);
    memset(value_ptr, 'c', 12);
    vlen = 12;
    ret = update_block(bsp, "str_key", 8, value_ptr, vlen);
    CU_ASSERT(index_logger_flush(bsp->index_logger) == RET_SUCCESS);
    CU_ASSERT(ret == 0);
    slab_free(value_ptr);

    struct index_logger_s * il = index_logger_open("data/", "index.db.journal");
    CU_ASSERT(il != NULL);
    struct index_log_item_s *ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_append);
    CU_ASSERT(memcmp(ili->index_key->key, "str_key", 8) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 12);
    index_logger_free_item(ili);

    ret = find_block(bsp, "str_key", 8, &value_ptr, &vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 12);
    CU_ASSERT(memcmp(value_ptr, "cccccccccccc", 12) == 0);
    slab_free(value_ptr);
    stat("./data/128.dat", &sb);
    CU_ASSERT(sb.st_size == 800024 + 12);

    value_ptr = slab_calloc(256);
    memset(value_ptr, 'c', 18);
    vlen = 18;
    ret = update_block(bsp, "str_key", 8, value_ptr, vlen);
    CU_ASSERT(ret == 0);
    slab_free(value_ptr);

    CU_ASSERT(index_logger_flush(bsp->index_logger) == RET_SUCCESS);
    ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_modify);
    CU_ASSERT(memcmp(ili->index_key->key, "str_key", 8) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 18);
    index_logger_free_item(ili);
    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);

    ret = find_block(bsp, "str_key", 8, &value_ptr, &vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 18);
    CU_ASSERT(memcmp(value_ptr, "cccccccccccccccccc", 18) == 0);
    slab_free(value_ptr);
    stat("./data/128.dat", &sb);
    CU_ASSERT(sb.st_size == 800024 + 18);

    value_ptr = slab_calloc(256);
    memset(value_ptr, 'c', 256);
    vlen = 256;
    ret = update_block(bsp, "str_key", 8, value_ptr, vlen);
    CU_ASSERT(ret == 0);
    slab_free(value_ptr);

    ret = find_block(bsp, "str_key", 8, &value_ptr, &vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 256);
    CU_ASSERT(*(char*)value_ptr == 'c');
    CU_ASSERT(*(char*)(value_ptr + 255) == 'c');
    slab_free(value_ptr);
    stat("./data/256.dat", &sb);
    CU_ASSERT(sb.st_size == 800024 + 256);

    ret = close_storage(bsp);
    CU_ASSERT(ret == 0);
}

void
test_arbitrary_keylen(void)
{
    system("rm -f ./data/128.dat");

    struct bucket_storage_t *bsp;
    int ret = open_storage("./data/index.db", 1024, &bsp);
    CU_ASSERT(ret == 0);

    void *value_ptr;
    size_t vlen;
    struct stat sb;

    assert(stat("./data/128.dat", &sb) == 0);
    size_t oldsize = sb.st_size;

    value_ptr = slab_calloc(128);
    vlen = 7;
    memset(value_ptr, 'd', vlen);
    ret = update_block(bsp, "abc", 3, value_ptr, vlen);
    CU_ASSERT(ret == 0);
    memset(value_ptr, '\0', 128);
    slab_free(value_ptr);
    value_ptr = NULL;

    assert(stat("./data/128.dat", &sb) == 0);
    size_t newsize = sb.st_size;
    CU_ASSERT(newsize == oldsize + 7);

    ret = find_block(bsp, "abc", 3, &value_ptr, &vlen);
    CU_ASSERT(value_ptr != NULL);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 7);
    CU_ASSERT(memcmp(value_ptr, "ddddddd", vlen) == 0);
    slab_free(value_ptr);

    ret = close_storage(bsp);
    CU_ASSERT(ret == 0);
}

void
test_storage_delete(void)
{
    struct bucket_storage_t *bsp;
    int ret = open_storage("./data/index.db", 1024, &bsp);
    CU_ASSERT(ret == 0);

    void *value_ptr;
    size_t vlen;

    value_ptr = slab_calloc(128);
    vlen = 100;
    memset(value_ptr, 'r', vlen);
    ret = update_block(bsp, "test_key", 8, value_ptr, vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(ret == 0);
    memset(value_ptr, '\0', 128);
    slab_free(value_ptr);
    value_ptr = NULL;
    vlen = 0;

    ret = find_block(bsp, "test_key", 8, &value_ptr, &vlen);
    CU_ASSERT(value_ptr != NULL);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 100);
    CU_ASSERT(memcmp(value_ptr, "rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr", vlen) == 0);
    slab_free(value_ptr);

    ret = delete_block(bsp, "test_key", 8);
    CU_ASSERT(ret == 0);

    ret = find_block(bsp, "test_key", 8, &value_ptr, &vlen);
    CU_ASSERT(value_ptr == NULL);
    CU_ASSERT(ret == DB_NOTFOUND);

    ret = close_storage(bsp);
    CU_ASSERT(ret == 0);
}

void
test_header(void)
{
    system("rm -rf data");
    struct bucket_storage_t *bsp;
    int ret = open_storage("./data/index.db", 1024, &bsp);
    CU_ASSERT(ret == 0);
    void *value_ptr;
    size_t vlen;
    char data[1024];

    value_ptr = &data[0];
    vlen = 18;
    memset(value_ptr, 'd', vlen);

    ret = update_block(bsp, "newkey", 7, value_ptr, vlen);
    CU_ASSERT(index_logger_flush(bsp->index_logger) == RET_SUCCESS);
    CU_ASSERT(ret == 0);

    int fd128, fd256;
    size_t tmp_size;
    off_t tmp_offset;
    fd128 = open("./data/128.dat", O_RDONLY);
    CU_ASSERT(fd128 >= 0);
    fd256 = open("./data/256.dat", O_RDONLY);
    CU_ASSERT(fd256 >= 0);

    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 128); /* block_size */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 1); /* block_cnt */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 0); /* free_slots_cnt */
    ret = find_block(bsp, "newkey", 7, &value_ptr, &vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 18);
    CU_ASSERT(memcmp(value_ptr, "dddddddddddddddddd", 18) == 0);
    slab_free(value_ptr);

    value_ptr = &data[0];
    memset(value_ptr, 'z', vlen);
    ret = update_block(bsp, "key", 4, value_ptr, vlen);
    CU_ASSERT(ret == 0);
    lseek(fd128, 0, SEEK_SET);
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 128); /* block_size */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 2); /* block_cnt */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 0); /* free_slots_cnt */
    ret = find_block(bsp, "key", 4, &value_ptr, &vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 18);
    CU_ASSERT(memcmp(value_ptr, "zzzzzzzzzzzzzzzzzz", 18) == 0);
    slab_free(value_ptr);

    value_ptr = &data[0];
    vlen = 256;
    memset(value_ptr, 'x', vlen);
    ret = update_block(bsp, "newkey", 7, value_ptr, vlen);
    CU_ASSERT(ret == 0);
    ret = find_block(bsp, "newkey", 7, &value_ptr, &vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 256);
    CU_ASSERT(memcmp(value_ptr, "x", 1) == 0);
    CU_ASSERT(memcmp(value_ptr+255, "x", 1) == 0);
    slab_free(value_ptr);
    lseek(fd128, 0, SEEK_SET);
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 128); /* block_size */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 1); /* block_cnt */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 1); /* free_slots_cnt */
    CU_ASSERT(read(fd128, &tmp_offset, sizeof(off_t)) == sizeof(off_t));
    CU_ASSERT(tmp_offset == DATA_SECTION_POS); /* free_slots_offset[0] */
    CU_ASSERT(read(fd256, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 256); /* block_size */
    CU_ASSERT(read(fd256, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 1); /* block_cnt */
    CU_ASSERT(read(fd256, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 0); /* free_slots_cnt */

    CU_ASSERT(update_block(bsp, "key", 4, "abc", 3) == 0);
    ret = find_block(bsp, "key", 4, &value_ptr, &vlen);
    CU_ASSERT(ret == 0);
    CU_ASSERT(vlen == 3);
    CU_ASSERT(memcmp(value_ptr, "abc", 3) == 0);
    slab_free(value_ptr);
    lseek(fd128, 0, SEEK_SET);
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 128); /* block_size */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 1); /* block_cnt */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 1); /* free_slots_cnt */
    CU_ASSERT(read(fd128, &tmp_offset, sizeof(off_t)) == sizeof(off_t));
    CU_ASSERT(tmp_offset == DATA_SECTION_POS); /* free_slots_offset[0] */

    CU_ASSERT(delete_block(bsp, "key", 4) == 0);
    CU_ASSERT(find_block(bsp, "key", 4, &value_ptr, &vlen) == DB_NOTFOUND);
    lseek(fd128, 0, SEEK_SET);
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 128); /* block_size */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 0); /* block_cnt */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 2); /* free_slots_cnt */
    CU_ASSERT(read(fd128, &tmp_offset, sizeof(off_t)) == sizeof(off_t));
    CU_ASSERT(tmp_offset == DATA_SECTION_POS); /* free_slots_offset[0] */
    CU_ASSERT(read(fd128, &tmp_offset, sizeof(off_t)) == sizeof(off_t));
    CU_ASSERT(tmp_offset == DATA_SECTION_POS + 128); /* free_slots_offset[0] */

    CU_ASSERT(index_logger_flush(bsp->index_logger) == RET_SUCCESS);

    /* {{{ check journal */
    struct index_logger_s *il = index_logger_open("data/", "index.db.journal");
    CU_ASSERT(il != NULL);
    struct index_log_item_s *ili;

    ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_append);
    CU_ASSERT(memcmp(ili->index_key->key, "newkey", 7) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 18);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_append);
    CU_ASSERT(memcmp(ili->index_key->key, "key", 4) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 18);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS + 128);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_del);
    CU_ASSERT(memcmp(ili->index_key->key, "newkey", 7) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 18);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_append);
    CU_ASSERT(memcmp(ili->index_key->key, "newkey", 7) == 0);
    CU_ASSERT(ili->index_value->block_id == 1);
    CU_ASSERT(ili->index_value->data_size == 256);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_modify);
    CU_ASSERT(memcmp(ili->index_key->key, "key", 4) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 3);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS + 128);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili->type == index_del);
    CU_ASSERT(memcmp(ili->index_key->key, "key", 4) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 3);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS + 128);
    index_logger_free_item(ili);

    ili = index_logger_fetch(il);
    CU_ASSERT(ili == NULL);

    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);
    /* }}} */

    close(fd128);
    close(fd256);
    ret = close_storage(bsp);
    CU_ASSERT(ret == 0);
}

void
test_sync_storage(void)
{
    system("rm -rf data");
    struct bucket_storage_t *bsp;
    int ret = open_storage("./data/index.db", 1024, &bsp);
    CU_ASSERT(ret == 0);
    void *value_ptr;
    size_t vlen;
    char data[1024];

    value_ptr = &data[0];
    vlen = 18;
    memset(value_ptr, 'd', vlen);

    ret = update_block(bsp, "newkey", 7, value_ptr, vlen);
    CU_ASSERT(ret == 0);

    struct index_logger_s *il = index_logger_open("data/", "index.db.journal");
    CU_ASSERT(il != NULL);
    struct index_log_item_s *ili;
    ili = index_logger_fetch(il);
    CU_ASSERT(ili == NULL);

    sync_storage(bsp, "index");

    ili = index_logger_fetch(il);
    CU_ASSERT(ili != NULL);
    CU_ASSERT(ili->type == index_append);
    CU_ASSERT(memcmp(ili->index_key->key, "newkey", 7) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 18);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS);
    index_logger_free_item(ili);

    CU_ASSERT(update_block(bsp, "key", 4, "abc", 3) == 0);
    sync_storage(bsp, "all");

    ili = index_logger_fetch(il);
    CU_ASSERT(ili != NULL);
    CU_ASSERT(ili->type == index_append);
    CU_ASSERT(memcmp(ili->index_key->key, "key", 4) == 0);
    CU_ASSERT(ili->index_value->block_id == 0);
    CU_ASSERT(ili->index_value->data_size == 3);
    CU_ASSERT(ili->index_value->offset == DATA_SECTION_POS+128);
    index_logger_free_item(ili);

    int fd128;
    size_t tmp_size;
    fd128 = open("./data/128.dat", O_RDONLY);
    CU_ASSERT(fd128 >= 0);

    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 128);
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 2); /* block_cnt */
    CU_ASSERT(read(fd128, &tmp_size, sizeof(size_t)) == sizeof(size_t));
    CU_ASSERT(tmp_size == 0); /* free_slots_cnt */

    char buf[128];
    CU_ASSERT(pread(fd128, buf, 19, DATA_SECTION_POS) == 19);
    CU_ASSERT(memcmp(buf, "dddddddddddddddddd", 19) == 0);

    CU_ASSERT(pread(fd128, buf, 3, DATA_SECTION_POS+128) == 3);
    CU_ASSERT(memcmp(buf, "abc", 3) == 0);

    struct stat old_sb, new_sb;
    char str[128];
    stat("./data/index.db", &old_sb);
    for (int i=0; i<100; i++)
    {
        sprintf(str, "%d", i);
        ret = update_block(bsp, str, strlen(str), str, strlen(str));
        assert(ret == 0);
    }
    sync_storage(bsp, "all");
    stat("./data/index.db", &new_sb);
    CU_ASSERT(new_sb.st_size > old_sb.st_size);

    CU_ASSERT(index_logger_close(il) == RET_SUCCESS);
    ret = close_storage(bsp);
    CU_ASSERT(ret == 0);
}

int
check_storage_init()
{
    /* {{{ init CU suite check_storage_init */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_storage_init", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_storage_init */
    if (CU_add_test(pSuite, "test_storage_init", test_storage_init) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_storage_update */
    if (CU_add_test(pSuite, "test_storage_update", test_storage_update) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_arbitrary_keylen */
    if (CU_add_test(pSuite, "test_arbitrary_keylen", test_arbitrary_keylen) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_storage_delete */
    if (CU_add_test(pSuite, "test_storage_delete", test_storage_delete) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_header */
    if (CU_add_test(pSuite, "test_header", test_header) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_sync_storage */
    if (CU_add_test(pSuite, "test_sync_storage", test_sync_storage) == NULL)
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

    int lowest_fd = dup(0);
    close(lowest_fd);

    slab_init(MEMPOOL_SIZE, 1.125);
    log_open("./datanode.log");
    check_storage_init();

    log_close();
    int curr_fd = dup(0);
    close(curr_fd);
    assert(curr_fd == lowest_fd);

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */
    slab_stats();

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
