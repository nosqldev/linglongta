#ifndef _QUERY_THREAD_H_
#define _QUERY_THREAD_H_
/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | header of query thread                                               |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-30 16:35                                            |
 * +----------------------------------------------------------------------+
 */

#define MAX_RESP_HEADER_SIZE (512)

enum query_state_e
{
    s_recv_req_head = 0,
    s_parse_req_head,
    s_recv_req_data,
    s_dispatch_query_task,
    s_destroy_recv,

    s_do_sync,
    s_do_power,

    s_set_success,
    s_get_success,
    s_delete_success,

    s_set_failed,
    s_get_failed,
    s_delete_failed,

    s_return_resp_head,
    s_return_resp_data,
    s_return_failed,
} query_state_t;

typedef struct worker_thread_arg_s
{
    int thread_id;
    int pipe_fd;
} worker_thread_arg_t;

typedef struct query_response_s
{
    char resp_head_buf[MAX_RESP_HEADER_SIZE];
    size_t resp_head_buf_size;

    void *resp_data_buf;
    size_t resp_data_size;

    void *resp_tailor_buf;
    size_t resp_tailor_size;
} query_response_t;

typedef struct query_task_s
{
    int sock_fd;
    int file_fd; /* unused now, for sendfile(2) */
    int thread_id;

    struct timeval connected_time;
    struct timeval done_time;
    struct sockaddr_in query_sockaddr;
    char query_ip[IP_STR_LEN];                  /* string form of client ip */
    char *error_log;

    enum query_state_e state;                   /* state of the query_task */
    ssize_t have_read;
    ssize_t need_read;
    ssize_t have_written;
    ssize_t need_write;

    ev_io query_io_w;
    ev_timer query_timer_w;                     /* controlling timeout event of both request and response */

    struct query_request_s  query_request;
    struct query_response_s query_response;
} query_task_t;

void *new_worker_thread(void *arg);

#endif /* ! _QUERY_THREAD_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
