/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | main of datanode                                                     |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-29 21:02                                            |
 * +----------------------------------------------------------------------+
 */

#include "global.h"
#include "iolayer.h"
#include "storage.h"
#include "memcached_protocol.h"
#include "query_thread.h"
#include "assign_thread.h"
#include "query_thread.h"

int
main(void)
{
    pthread_t assign_tid;
    pthread_t query_tid[WORKER_THREAD_CNT];
    struct worker_thread_arg_s query_thread_args[WORKER_THREAD_CNT];
    int query_thread_cnt;

    init_datanode_vars();
    query_thread_cnt = g_settings.worker_thread_cnt;

    pthread_create(&assign_tid, NULL, new_assign_thread, NULL);
    sched_yield();
    sem_wait(&g_runtime_vars.sem_barrier);

    for (int i=0; i<query_thread_cnt; i++)
    {
        query_thread_args[i].thread_id = i;
        query_thread_args[i].pipe_fd = g_pipe_fd[i * 2];

        pthread_create(&query_tid[i], NULL, new_worker_thread, (void*)&query_thread_args[i]);
        sched_yield();
        sem_wait(&g_runtime_vars.sem_barrier);
    }

    pthread_join(assign_tid, NULL);
    for (int i=0; i<query_thread_cnt; i++)
    {
        pthread_join(query_tid[i], NULL);
    }

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
