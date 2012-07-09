/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | Master node with mirror sync policy                                  |
 * | Data nodes are quite the same under this master                      |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-03-07 17:53                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "global.h"
#include "memcached_protocol.h"
#include "query_thread.h"
#include "master_mirror.h"
#include "acoro.h"

#define MAX_SLAVE_CNT (4)

/* TODO
 * 1. Add "readonly" keyword
 * 2. Add "add slave" keyword
 */
enum service_status_t
{
    read_write,
    readonly,
};

struct master_mirror_settings_s
{
    in_addr_t bind_ip;
    uint16_t accept_port;

    size_t read_timeout;     /* millisecond */
    size_t write_timeout;    /* millisecond */

    char log_path[PATH_MAX];
};

struct slave_t
{
    in_addr_t ip;
    in_port_t port;
    char addr[128];
};

struct master_mirror_runtime_vars_s
{
    struct service_status
    {
        pthread_mutex_t lock;
        volatile enum service_status_t status;

        int slave_cnt;
        struct slave_t slaves[MAX_SLAVE_CNT];
    } ss;

    struct io_stat_s io_stats;
    struct
    {
        atomic_uint_t set_request_cnt;
        atomic_uint_t get_request_cnt;
        atomic_uint_t del_request_cnt;
        atomic_uint_t other_request_cnt;
    } request_stats;
};

static struct master_mirror_settings_s     g_master_settings;
static struct master_mirror_runtime_vars_s g_master_runtime_vars;

#define slave_ip(id)   (g_master_runtime_vars.ss.slaves[id].ip)
#define slave_port(id) (g_master_runtime_vars.ss.slaves[id].port)
#define slave_addr(id) (g_master_runtime_vars.ss.slaves[id].addr)
#define slave_cnt()    (g_master_runtime_vars.ss.slave_cnt) /* a bit strange here */

enum mm_ret_t
{
    RET_SUCC,
    RET_ADDR_ERROR,
    RET_IP_ERROR,
    RET_PORT_ERROR,
    RET_CONNECT_TIMEOUT,
    RET_CONNECT_REFUSED,
    RET_CONNECT_FAILED,
};

static int
init_vars(void)
{
    memset(&g_master_settings, 0, sizeof g_master_settings);
    memset(&g_master_runtime_vars, 0, sizeof g_master_runtime_vars);

    slab_init(MASTER_MEMPOOL_SIZE, 1.125);
    signal(SIGPIPE, SIG_IGN);

    g_master_settings.bind_ip       = inet_addr(MASTER_BIND_IP);
    g_master_settings.accept_port   = MASTER_ACCEPT_PORT;
    g_master_settings.read_timeout  = MASTER_READ_TIMEOUT;
    g_master_settings.write_timeout = MASTER_WRITE_TIMEOUT;
    strcpy(g_master_settings.log_path, MASTER_LOG_PATH);

    g_master_runtime_vars.ss.status = read_write;
    pthread_mutex_init(&g_master_runtime_vars.ss.lock, NULL);

    log_open("./master_mirror.log");

    return 0;
}

static int
parse_slave_info(const char * str, struct slave_t *slave_ptr)
{
    char *delim_ptr;
    char slave_info_copy[BUFSIZ] = {0};

    strcpy(slave_info_copy, str);
    if ((delim_ptr=strchr(slave_info_copy, ':')) == NULL)
    {
        return -1;
    }
    *delim_ptr = '\0';
    if (inet_pton(AF_INET, slave_info_copy, &slave_ptr->ip) != 1)
        return -1;
    if (atoi(delim_ptr + 1) < 80) /* assume that port should be larger than 80 */
        return -1;
    else
        slave_ptr->port = htons(atoi(delim_ptr + 1));

    return 0;
}

static int
init_args(int argc, char **argv)
{
    for (int i=1; i<argc; i++)
    {
        if (strcmp(argv[i], "-s") == 0)
        {
            for (int j=i+1; j<argc; j++)
            {
                if (parse_slave_info(argv[j], &g_master_runtime_vars.ss.slaves[ slave_cnt() ])
                    == -1)
                {
                    break;
                }
                if (slave_cnt() >= MAX_SLAVE_CNT)
                {
                    fprintf(stderr, "too many slave\n");
                    exit(-1);
                }
                strcpy(slave_addr(slave_cnt()), argv[j]);
                slave_cnt() ++; /* a bit strange here */
                i = j;
            }
        }
        else
        {
            fprintf(stderr, "arg error\n");
            exit(-1);
        }
    }

    if (slave_cnt() == 0)
    {
        fprintf(stderr, "no slave provided\n");
        exit(-1);
    }
    else
    {
        printf("%d slaves, they are:\n", slave_cnt());
        for (int i=0; i<slave_cnt(); i++)
        {
            printf("\t%s\n", slave_addr(i));
        }
    }

    return 0;
}

static int
execute_all_slaves(int para_cnt, ...)
{
    va_list ap;

    return 0;
}

static int
execute_one_slave(int slave_id, int arg_cnt, ...)
{
    va_list ap;
    int fd;
    char *request_stream;
    size_t request_len;
    int nread;

    fd = crt_tcp_timeout_connect(slave_ip(slave_id), slave_port(slave_id), MASTER_CONNECT_SLAVE_TIMEOUT);
    if (fd < 0)
    {
        if (crt_errno == ECONNREFUSED)
            return RET_CONNECT_REFUSED;
        else if (crt_errno == EWOULDBLOCK)
            return RET_CONNECT_TIMEOUT;
        else
            return RET_CONNECT_FAILED;
    }

    va_start(ap, arg_cnt);
    for (int i=0; i<arg_cnt; i++)
    {
        request_stream = va_arg(ap, char *);
        request_len = va_arg(ap, size_t);
        crt_tcp_write_to(fd, request_stream, request_len, MASTER_SEND_SLAVE_TIMEOUT);
    }
    va_end(ap);

    for ( ; ; )
    {
        nread = crt_tcp_read_to(fd, 

    return 0;
}

static struct query_response_s *
fetch_result(struct query_request_s *request_ptr)
{
    switch (request_ptr->cmd_type)
    {
    case cmd_sync:
    case cmd_power:
    case cmd_set:
    case cmd_delete:
        break;

    case cmd_get:
        break;

    default:
        break;
    }
    return NULL;
}

static int
sendback_result(int fd, struct query_response_s *response_ptr)
{
    return 0;
}

static void 
sendback_error_mesg(int fd)
{
    const char *error_mesg = MC_ERR_STR;
    crt_tcp_write_to(fd, (void*)error_mesg, strlen(error_mesg), g_master_settings.write_timeout);
}

static void *
handle_request(void *arg)
{
    int fd = (intptr_t)arg;
    ssize_t nread;
    struct query_request_s request;
    struct query_response_s *response_ptr;
    char *p;
    char remote_ip[IP_STR_LEN];
    struct sockaddr_in remote_peer;
    socklen_t addrlen;
    char warn_log_mesg[1024] = {0};
    int ret;
    pre_timer();

    launch_timer();
    crt_set_nonblock(fd);
    addrlen = sizeof remote_peer;
    getpeername(fd, (struct sockaddr *)&remote_peer, &addrlen);
    inet_ntop(AF_INET, &(remote_peer.sin_addr), remote_ip, sizeof remote_ip);
    memset(&request, 0, sizeof request);
    /* {{{ recv header */
    for (int i=0; i<MAX_RESP_HEADER_SIZE; i++)
    {
        nread = crt_tcp_read_to(fd, &request.query_head_buf[i], 1, g_master_settings.read_timeout);
        if (nread == 1)
        {
            if ((p=try_parse_memcached_string_head(&request)) == NULL)
                continue;
            else
                break;
        }
        else
        {
            ret = snprintf(warn_log_mesg, sizeof warn_log_mesg, "read sock failed: %d - %s", crt_errno, strerror(crt_errno));
            assert(ret < (int)sizeof warn_log_mesg); /* NOT <= */
            assert(ret >= 0);
            goto done;
        }
    }
    /* }}} */
    /* {{{ parse header & prepare for receiving data */
    request.query_head_buf_size = strlen(request.query_head_buf);
    if ((parse_memcached_string_head(&request) == RET_SUCCESS)
         && (request.parse_status == ps_succ))
    {
        switch (request.cmd_type)
        {
            case cmd_set:
                request.data_size += 2; /* alloc for tailing '\r\n' */
                request.data = slab_alloc(request.data_size);
                assert(request.data != NULL);
                nread = crt_tcp_read(fd, request.data, request.data_size);
                if (nread != (ssize_t)request.data_size)
                {
                    ret = snprintf(warn_log_mesg, sizeof warn_log_mesg,
                                   "read request body failed: nread = %zd, error = %d - %s",
                                   nread, crt_errno, strerror(crt_errno));
                    assert(ret < (int)sizeof warn_log_mesg); /* NOT <= */
                    assert(ret >= 0);
                    goto done;
                }
                break;

            case cmd_get:
            case cmd_delete:
                /* nothing to do */
                break;

            /* TODO */
            case cmd_sync:
            case cmd_power:
            default:
                break;
        }
    }
    else
    {
        sendback_error_mesg(fd);
        ret = snprintf(warn_log_mesg, sizeof warn_log_mesg, "parse request failed, cmd type: %d", request.cmd_type);
        assert(ret < (int)sizeof warn_log_mesg); /* NOT <= */
        assert(ret >= 0);
    }
    /* }}} */
    response_ptr = fetch_result(&request);
    sendback_result(fd, response_ptr);

done:
    if (request.data != NULL)
        slab_free(request.data);
    stop_timer();
    if (warn_log_mesg[0] == '\0')
    {
        log_notice("%s <Elapsed %ld.%06lds> {from: %s, fd = %d}", cmd_type_name(request.cmd_type), 
                   __used.tv_sec, __used.tv_usec, remote_ip, fd);
    }
    else
    {
        log_warn("%s <Elapsed %ld.%06lds> {from: %s, fd = %d} %s", cmd_type_name(request.cmd_type),
                 __used.tv_sec, __used.tv_usec, remote_ip, fd, warn_log_mesg);
    }
    close(fd);
    crt_exit(NULL);
}

static int
accept_loop(void)
{
    int retval;
    int listenfd;
    struct sockaddr_in server_addr;
    int flag;

    retval = RET_FAILED;
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0)
    {
        log_error("socket() failed: %d - %s", errno, strerror(errno));
        retval = RET_FAILED;
        goto done;
    }
    flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_addr.s_addr = g_master_settings.bind_ip;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_master_settings.accept_port);
    if (bind(listenfd, (struct sockaddr *)(&server_addr), sizeof(server_addr)) != 0)
    {
        log_error("bind port %d failed: %d - %s", g_master_settings.accept_port, errno, strerror(errno));
        retval = RET_FAILED;
        goto done;
    }
    if (listen(listenfd, 128) < 0)
    {
        log_error("listen() failed: %d - %s", errno, strerror(errno));
        retval = RET_FAILED;
        goto done;
    }

    intptr_t sockfd;
    struct sockaddr_in remote_peer;
    socklen_t len;
    coroutine_attr_t crt_attr;

    crt_attr_setstacksize(&crt_attr, MASTER_COROUTINE_STACKSIZE);
    len = sizeof remote_peer;
    for ( ; ; )
    {
        sockfd = (intptr_t)accept(listenfd, (struct sockaddr *)&remote_peer, &len);
        if (sockfd < 0)
        {
            char ip[IP_STR_LEN];
            inet_ntop(AF_INET, &(remote_peer.sin_addr), ip, sizeof ip);
            log_warn("accept() failed: %d - %s, ret = %d, ip = %s", errno, strerror(errno), (int)sockfd, ip);
            continue;
        }

        crt_create(NULL, NULL, handle_request, (void*)sockfd);
    }

done:

    return retval;
}

#ifndef TESTMODE

int
main(int argc, char **argv)
{
    int ret;

    init_vars();
    init_args(argc, argv);

    ret = init_coroutine_env();
    if (ret != 0)
    {
        fprintf(stderr, "init async_coro failed\n");
        exit(-1);
    }

    if (accept_loop() != RET_SUCCESS)
    {
        fprintf(stderr, "accept_loop() failed\n");
        exit(-1);
    }

    return 0;
}

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
