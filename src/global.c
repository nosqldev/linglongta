/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | global file of img stack                                             |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-18 13:43                                            |
 * +----------------------------------------------------------------------+
 */

#include "global.h"
#include "storage.h"

struct settings_s g_settings;
struct runtime_vars_s g_runtime_vars;

int
init_datanode_vars()
{
    memset(&g_settings, 0, sizeof g_settings);
    memset(&g_runtime_vars, 0, sizeof g_runtime_vars);

    slab_init(MEMPOOL_SIZE, 1.125);

    signal(SIGPIPE, SIG_IGN);

    g_settings.bind_ip           = inet_addr(BIND_IP);
    g_settings.accept_port       = ACCEPT_PORT;
    g_settings.read_timeout      = READ_TIMEOUT;
    g_settings.write_timeout     = WRITE_TIMEOUT;
    g_settings.mempool_size      = MEMPOOL_SIZE;
    g_settings.cache_size        = CACHE_SIZE;
    g_settings.worker_thread_cnt = WORKER_THREAD_CNT;
    strcpy(g_settings.index_path, INDEX_PATH);
    strcpy(g_settings.data_path, DATA_PATH);
    strcpy(g_settings.log_path, LOG_PATH);

    atomic_uint_set(&g_runtime_vars.io_stats.disk_write_bytes, 0);
    atomic_uint_set(&g_runtime_vars.io_stats.disk_read_bytes, 0);
    atomic_uint_set(&g_runtime_vars.io_stats.net_write_bytes, 0);
    atomic_uint_set(&g_runtime_vars.io_stats.net_read_bytes, 0);
    atomic_uint_set(&g_runtime_vars.io_stats.curr_query_queue_len, 0);
    atomic_uint_set(&g_runtime_vars.io_stats.total_write_requests, 0);
    atomic_uint_set(&g_runtime_vars.io_stats.total_read_requests, 0);
    for (int i=0; i<MAX_BLOCK_CNT; i++)
        atomic_uint_set(&g_runtime_vars.store_stats.block_stats[i], 0);
    atomic_uint_set(&g_runtime_vars.store_stats.total_store_size, 0);
    atomic_uint_set(&g_runtime_vars.store_stats.total_files_cnt, 0);

    sem_init(&g_runtime_vars.sem_barrier, 0, 0);
    g_loop = ev_default_loop(EVBACKEND_EPOLL);
    g_pipe_fd = slab_calloc(sizeof(int) * g_settings.worker_thread_cnt * 2);

    for (size_t i=0; i<g_settings.worker_thread_cnt; i++)
    {
        if (pipe(&(g_pipe_fd[i*2])) < 0)
        {
            fprintf(stderr, "pipe() failed: %d - %s", errno, strerror(errno));
            return RET_FAILED;
        }
    }

    set_global_cache_size(0);

    pthread_mutex_init(&g_runtime_vars.lock, NULL);
    pthread_mutex_init(&g_runtime_vars.snapshot_lock, NULL);

    log_open("./datanode.log");
    if (open_storage("./data/so.db", 1024 * 1024 * 16, &g_storage) != 0)
        return RET_FAILED;

    return RET_SUCCESS;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
