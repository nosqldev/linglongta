#ifndef _GLOBAL_H_
#define _GLOBAL_H_
/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | global header                                                        |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-18 13:48                                            |
 * +----------------------------------------------------------------------+
 */

/* {{{ system header */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/stddef.h>
#include <errno.h>
#include <sched.h>
#include <semaphore.h>

/* }}} */
/* {{{ my header */
/* TODO remove these header here */

#include "ev.h"
#include "crc32.h"
#include "log.h"
#include "slab.h"
#include "utils.h"
#include "rwlock.h"

/* }}} */
/* {{{ pre-defined */

/**
 * @brief Max amount of block types
 */
#define MAX_BLOCK_CNT (20)

enum imgstack_ret_code
{
    RET_FAILED = -1,
    RET_SUCCESS = 0,
    RET_NO_MEM,
    RET_PARSE_REQ_FAILED,
};

#define IP_STR_LEN (INET_ADDRSTRLEN)

#define log_no_mem do { log_error("memory is not enough"); } while (0)

#ifdef TESTMODE
#define STATIC_MODIFIER
#else
#define STATIC_MODIFIER static
#endif

/* }}} */
/* {{{ structure */

typedef struct settings_s
{
    in_addr_t bind_ip;
    uint16_t accept_port;

    size_t read_timeout;     /* millisecond */
    size_t write_timeout;    /* millisecond */

    size_t mempool_size;
    size_t cache_size;

    size_t worker_thread_cnt;

    char index_path[PATH_MAX];
    char data_path[PATH_MAX];
    char log_path[PATH_MAX];
} settings_t;

typedef struct store_stat_s
{
    struct atomic_uint_s total_store_size;
    struct atomic_uint_s total_files_cnt;
    struct atomic_uint_s block_stats[MAX_BLOCK_CNT];
} store_stat_t;

typedef struct io_stat_s
{
    struct atomic_uint_s curr_query_queue_len;
    struct atomic_uint_s total_read_requests;
    struct atomic_uint_s total_write_requests;

    struct atomic_uint_s disk_read_bytes;
    struct atomic_uint_s disk_write_bytes;
    struct atomic_uint_s net_read_bytes;
    struct atomic_uint_s net_write_bytes;
} io_stat_t;

typedef struct runtime_vars_s
{
    struct ev_loop *main_loop;
    int *pipe_fd;
    struct bucket_storage_t *block_storage;

    struct store_stat_s store_stats;
    struct io_stat_s io_stats;

    sem_t sem_barrier;

    pthread_mutex_t lock; /* TODO anyway to remove lock here? */
    volatile ssize_t cache_size;

    /* below fields are unused now */
    pthread_mutex_t snapshot_lock;

} runtime_vars_t;

/* }}} */
/* {{{ declare vars & funcations */

extern struct settings_s g_settings;
extern struct runtime_vars_s g_runtime_vars;

int init_datanode_vars();

/* }}} */
/* {{{ code snippets */
#define g_loop (g_runtime_vars.main_loop)
#define g_pipe_fd g_runtime_vars.pipe_fd
#define g_storage (g_runtime_vars.block_storage)

#define set_global_cache_size(v) do {           \
    pthread_mutex_lock(&g_runtime_vars.lock);   \
    g_runtime_vars.cache_size = v;              \
    pthread_mutex_unlock(&g_runtime_vars.lock); \
} while (0)
#define cmp_global_cache_size(v, ret) do {      \
    pthread_mutex_lock(&g_runtime_vars.lock);   \
    ret = g_runtime_vars.cache_size - (v);      \
    pthread_mutex_unlock(&g_runtime_vars.lock); \
} while (0)

#define TO_S(s) #s

#ifndef __offset_of
#define __offset_of(type, field)    ((size_t)(&((type *)0)->field))
#endif

#define timeval_sub(vpa, vpb, vpc)                      \
do {                                                    \
    (vpc)->tv_sec = (vpa)->tv_sec - (vpb)->tv_sec;      \
    (vpc)->tv_usec = (vpa)->tv_usec - (vpb)->tv_usec;   \
    if ((vpc)->tv_usec < 0){                            \
        (vpc)->tv_sec --;                               \
        (vpc)->tv_usec += 1000000;                      \
    }                                                   \
} while (0)

#define pre_timer() struct timeval __start, __end, __used
#define launch_timer() gettimeofday(&__start, NULL);
#define stop_timer() do { gettimeofday(&__end, NULL); timeval_sub(&__end, &__start, &__used); } while (0)

/* }}} */
/* {{{ configurations */
#define BIND_IP "0.0.0.0"
#define ACCEPT_PORT (5000)
#define READ_TIMEOUT (1000 * 60)
#define WRITE_TIMEOUT (1000 * 10)
#define MEMPOOL_SIZE (1024 * 1024 * 1024 * 4L)
#define CACHE_SIZE (1024 * 1024 * 1024 * 1L)
#define WORKER_THREAD_CNT (8)
#define INDEX_PATH "./data/"
#define DATA_PATH "./data/"
#define LOG_PATH "./log/sos.log"
#define BDB_CACHE_SIZE (1024 * 1024 * 1024)
/* }}} */

#endif /* ! _GLOBAL_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
