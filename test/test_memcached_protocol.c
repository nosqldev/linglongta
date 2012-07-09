/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | test for memcached protocol                                          |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-04 03:15                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <CUnit/Basic.h>

#include "memcached_protocol.h"
#include "global.h"

bool parse_get_cmd(char **saveptr, struct query_request_s *qr);
bool parse_delete_cmd(char **saveptr, struct query_request_s *qr);
bool parse_set_cmd(char **saveptr, struct query_request_s *qr);
bool parse_sync_cmd(char **saveptr, struct query_request_s *qr);
bool parse_power_cmd(char **saveptr, struct query_request_s *qr);

void
test_parse_get_cmd(void)
{
    char correct_header[] = "get abcd\r\n";
    char error_header[]  = "get abc\n";
    char error_header_long[]  = "get 111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111112222\r\n";
    char *saveptr;
    char *token;
    bool ret;
    query_request_t qr;

    token = strtok_r(correct_header, " ", &saveptr);
    CU_ASSERT(strcmp(token, "get") == 0);
    CU_ASSERT(strcmp(saveptr, "abcd\r\n") == 0);

    ret = parse_get_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(strcmp(saveptr, "abcd") == 0);
    CU_ASSERT(strcmp(qr.key, "abcd") == 0);

    saveptr = NULL;
    qr.cmd_type = cmd_unknown;
    token = strtok_r(error_header, " ", &saveptr);
    ret = parse_get_cmd(&saveptr, &qr);
    CU_ASSERT(ret == false);
    CU_ASSERT(strcmp(saveptr, "abc\n") == 0);
    CU_ASSERT(qr.cmd_type == cmd_unknown);

    saveptr = NULL;
    token = strtok_r(error_header_long, " ", &saveptr);
    ret = parse_get_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(strlen(qr.key) == MAX_KEY_SIZE-1);
    CU_ASSERT(strcmp(qr.key, "1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111") == 0);
}

void
test_parse_delete_cmd(void)
{
    char correct_header[] = "delete key\r\n";
    char error_header[] = "delete kkkk\r";
    char error_header_long[] = "delete 111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111\r\n";
    char *saveptr;
    char *token;
    bool ret;
    query_request_t qr;

    token = strtok_r(correct_header, " ", &saveptr);
    CU_ASSERT(strcmp(token, "delete") == 0);
    ret = parse_delete_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(strcmp(qr.key, "key") == 0);

    saveptr = NULL;
    token = strtok_r(error_header, " ", &saveptr);
    ret = parse_delete_cmd(&saveptr, &qr);
    CU_ASSERT(ret == false);

    saveptr = NULL;
    token = strtok_r(error_header_long, " ", &saveptr);
    CU_ASSERT(strcmp(token, "delete") == 0);
    ret = parse_delete_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(strlen(qr.key) == MAX_KEY_SIZE - 1);
    /* truncated key to 250 bytes */
    CU_ASSERT(strcmp(qr.key, "1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111") == 0);
}

void
test_parse_set_cmd(void)
{
    char correct_header[] = "set setkey  123   10 1\r\n";
    char *saveptr;
    char *token;
    bool ret;
    query_request_t qr;

    token = strtok_r(correct_header, " ", &saveptr);
    assert(strcmp(token, "set") == 0);
    ret = parse_set_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(qr.data_size == 1);
    CU_ASSERT(qr.exptime == 10);
    CU_ASSERT(qr.flag == 123);
    CU_ASSERT(strcmp(qr.key, "setkey") == 0);

    char error_header1[] = "set abcdefg 1 23 \r\n";
    saveptr = NULL;
    token = strtok_r(error_header1, " ", &saveptr);
    assert(strcmp(token, "set") == 0);
    ret = parse_set_cmd(&saveptr, &qr);
    CU_ASSERT(ret == false);
    CU_ASSERT(strcmp(qr.key, "abcdefg") == 0);
    CU_ASSERT(qr.flag == 1);
    CU_ASSERT(qr.exptime == 23);

    char error_header2[] = "set asdf\n";
    saveptr = NULL;
    token = strtok_r(error_header2, " ", &saveptr);
    assert(strcmp(token, "set") == 0);
    ret = parse_set_cmd(&saveptr, &qr);
    CU_ASSERT(ret == false);

    char error_header_long[] = "set 11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111 12 32 0\r\n";
    saveptr = NULL;
    token = strtok_r(error_header_long, " ", &saveptr);
    ret = parse_set_cmd(&saveptr, &qr);
    CU_ASSERT(ret == false);
}

void
test_parse_sync_cmd(void)
{
    char correct_sync_index_instruction[] = "sync index\r\n";
    char *saveptr;
    char *token;
    query_request_t qr;

    token = strtok_r(correct_sync_index_instruction, " ", &saveptr);
    CU_ASSERT(strcmp(token, "sync") == 0);
    int ret = parse_sync_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(strcmp(qr.key, "index") == 0);

    char correct_sync_index_instruction2[] = "sync all\r\n";
    token = strtok_r(correct_sync_index_instruction2, " ", &saveptr);
    CU_ASSERT(strcmp(token, "sync") == 0);
    ret = parse_sync_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(strcmp(qr.key, "all") == 0);

    char error_sync_index_instruction[] = "sync index_error\r\n";
    token = strtok_r(error_sync_index_instruction, " ", &saveptr);
    CU_ASSERT(strcmp(token, "sync") == 0);
    CU_ASSERT(parse_sync_cmd(&saveptr, &qr) == false);
}

void
test_parse_power_cmd(void)
{
    char correct_instruction[] = "power  halt\r\n";
    char *saveptr;
    char *token;
    query_request_t qr;

    token = strtok_r(correct_instruction, " ", &saveptr);
    CU_ASSERT(strcmp(token, "power") == 0);
    int ret = parse_power_cmd(&saveptr, &qr);
    CU_ASSERT(ret == true);
    CU_ASSERT(strcmp(qr.key, "halt") == 0);
}

int
check_parse_XXX_cmd()
{
    /* {{{ init CU suite check_parse_XXX_cmd */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_parse_XXX_cmd", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    /* {{{ CU_add_test: test_parse_get_cmd */
    if (CU_add_test(pSuite, "test_parse_get_cmd", test_parse_get_cmd) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_delete_cmd */
    if (CU_add_test(pSuite, "test_parse_delete_cmd", test_parse_delete_cmd) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_set_cmd */
    if (CU_add_test(pSuite, "test_parse_set_cmd", test_parse_set_cmd) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_sync_cmd */
    if (CU_add_test(pSuite, "test_parse_sync_cmd", test_parse_sync_cmd) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_power_cmd */
    if (CU_add_test(pSuite, "test_parse_power_cmd", test_parse_power_cmd) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */

    return 0;
}

void
test_parse_memcached_string_correct(void)
{
    struct query_request_s qr;
    int ret;

#define InitReq(req_string) do {    \
    memset(&qr, 0, sizeof qr);      \
    strcpy(qr.query_head_buf, req_string);  \
} while (0)

#define RunParseFunc(req_string) do { \
    InitReq(req_string);                \
    ret = parse_memcached_string_head(&qr);\
} while (0)

#define RunParseTest(req_string, ct, k, f, et, ds, ps) do { \
    RunParseFunc(req_string);   \
    CU_ASSERT(ret == RET_SUCCESS);  \
    CU_ASSERT(strcmp(qr.key, k) == 0);   \
    CU_ASSERT(qr.cmd_type == ct);   \
    CU_ASSERT(qr.data_size == ds);  \
    CU_ASSERT(qr.parse_status == ps);\
    CU_ASSERT(qr.flag == f);        \
    CU_ASSERT(qr.exptime == et);    \
} while (0)

    RunParseFunc("Set google 1 2 3\r\n");
    CU_ASSERT(ret == RET_SUCCESS);
    CU_ASSERT(strcmp(qr.key, "google") == 0);
    CU_ASSERT(qr.cmd_type == cmd_set);
    CU_ASSERT(qr.data_size == 3);
    CU_ASSERT(qr.parse_status == ps_succ);
    CU_ASSERT(qr.flag == 1);
    CU_ASSERT(qr.exptime == 2);

    RunParseTest("sEt      google a 0 0\r\n",
            cmd_set, "google", 0, 0, 0, ps_succ);
    RunParseTest("  set baidu 123456   654321    1234567   \r\n",
            cmd_set, "baidu", 123456, 654321, 1234567, ps_succ);
    RunParseTest("get abcdefg\r\n",
            cmd_get, "abcdefg", 0, 0, 0, ps_succ);
    RunParseTest("    GeT abcdefg\r\n",
            cmd_get, "abcdefg", 0, 0, 0, ps_succ);
    RunParseTest(" deletE cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\r\n",
            cmd_delete, "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
            0, 0, 0, ps_succ);

    RunParseTest("get   abc \r\n",
            cmd_get, "abc", 0, 0, 0, ps_succ);

    RunParseTest("delete abc \r\n",
            cmd_delete, "abc", 0, 0, 0, ps_succ);

    RunParseTest("sync  index\r\n",
            cmd_sync, "index", 0, 0, 0, ps_succ);

    RunParseTest("power halt\r\n",
            cmd_power, "halt", 0, 0, 0, ps_succ);
}

void
test_parse_memcached_string_failed(void)
{
    struct query_request_s qr;
    int ret;

    RunParseFunc("google\r\n");
    CU_ASSERT(ret == RET_PARSE_REQ_FAILED);
    CU_ASSERT(qr.cmd_type == cmd_unknown);
    CU_ASSERT(qr.parse_status == ps_failed);

    RunParseFunc("set google 0 0\r\n");
    CU_ASSERT(ret == RET_PARSE_REQ_FAILED);
    CU_ASSERT(qr.parse_status == ps_failed);
    CU_ASSERT(qr.cmd_type == cmd_set);

    RunParseFunc("sEt go0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000ogle a 0 0\r\n");
    CU_ASSERT(ret == RET_PARSE_REQ_FAILED);
    CU_ASSERT(qr.parse_status == ps_failed);
    CU_ASSERT(qr.cmd_type == cmd_set);

    RunParseFunc("get dddd");
    CU_ASSERT(ret == RET_PARSE_REQ_FAILED);
    CU_ASSERT(qr.parse_status == ps_failed);
    CU_ASSERT(qr.cmd_type == cmd_get);

    RunParseFunc("delete abc\n");
    CU_ASSERT(ret == RET_PARSE_REQ_FAILED);
    CU_ASSERT(qr.parse_status == ps_failed);
    CU_ASSERT(qr.cmd_type == cmd_delete);

    RunParseFunc("delete dddd");
    CU_ASSERT(ret == RET_PARSE_REQ_FAILED);
    CU_ASSERT(qr.parse_status == ps_failed);
    CU_ASSERT(qr.cmd_type == cmd_delete);

    RunParseFunc("power halt!\r\n");
    CU_ASSERT(ret == RET_PARSE_REQ_FAILED);
    CU_ASSERT(qr.parse_status == ps_failed);
    CU_ASSERT(qr.cmd_type == cmd_power);
}

int
check_parse_query_request_head()
{
    /* {{{ init CU suite check_parse_query_request_head */
    CU_pSuite pSuite = NULL;
    pSuite = CU_add_suite("check_parse_query_request_head", NULL, NULL);
    if (pSuite == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_memcached_string_correct */
    if (CU_add_test(pSuite, "test_parse_memcached_string_correct", test_parse_memcached_string_correct) == NULL)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    /* }}} */
    /* {{{ CU_add_test: test_parse_memcached_string_failed */
    if (CU_add_test(pSuite, "test_parse_memcached_stringfailed", test_parse_memcached_string_failed) == NULL)
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

    check_parse_XXX_cmd();
    check_parse_query_request_head();

    /* {{{ CU run & cleanup */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    /* }}} */

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
