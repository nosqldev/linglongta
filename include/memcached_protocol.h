#ifndef _MEMCACHED_PROTOCOL_H_
#define _MEMCACHED_PROTOCOL_H_
/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | parse memcached protocol: only parse string protocol now             |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-02 18:22                                            |
 * +----------------------------------------------------------------------+
 */

#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>

/* I simply implement the string memcached protocol here, since it sucks and I
 * will changed protocol sooner or later */

/* memcache string protocol
 * ------------------------
 *
 * CLIENT REQUEST:
 * set <key> <flags> <exptime> <bytes>\r\nDATA\r\n
 * get <key>\r\n
 * delete <key>\r\n
 *
 * SERVER RESPONSE:
 * VALUE <key> <flags> <bytes>\r\n
 * <data block>\r\n
 */

#define MAX_KEY_SIZE (251)
#define MAX_QUERY_HEADER_SIZE (512)

#define MC_TERM_STR "\r\n"
#define MC_ERR_STR "ERROR\r\n"
#define MC_ERR_STR_NONEXIST "ERROR\r\n"
#define MC_ERR_STR_CLIENT_PROTOCOL "CLIENT protocol error\r\n"
#define MC_ERR_STR_CLIENT_OTHER "CLIENT other error\r\n"
#define MC_ERR_STR_SERVER "SERVER query failed\r\n"
#define MC_SET_DONE_STR "STORED\r\n"
#define MC_GET_HEADER_PATTERN "VALUE %s %d %zu\r\n"
#define MC_GET_DONE_STR "END\r\n"
#define MC_DEL_DONE_STR "DELETED\r\n"

enum cmd_type_e
{
    cmd_unknown = 0,
    cmd_set,
    cmd_get,
    cmd_delete,
    cmd_sync,  /* internal instruction: to sync index/data to disk */
    cmd_power, /* internal instruction: power halt to shutdown server safely */
} cmd_type_t;

enum parse_status_e
{
    ps_unknown = 0,
    ps_succ,
    ps_failed,
} parse_status_t;

typedef struct query_request_s
{
    char query_head_buf[MAX_QUERY_HEADER_SIZE]; /* header of request: memcached string protocol */
    size_t query_head_buf_size;
    unsigned char *data;
    size_t data_size;

    enum cmd_type_e cmd_type;
    enum parse_status_e parse_status;
    int flag;
    int exptime;
    char key[MAX_KEY_SIZE];
} query_request_t;

int parse_memcached_string_head(struct query_request_s *qr);
char *try_parse_memcached_string_head(struct query_request_s *qr);
char *cmd_type_name(enum cmd_type_e cmd_type);

#endif /* ! _MEMCACHED_PROTOCOL_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
