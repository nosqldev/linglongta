/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | implementation of query thread                                       |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-02 01:15                                            |
 * +----------------------------------------------------------------------+
 */

#include <sys/uio.h>
#include "global.h"
#include "iolayer.h"
#include "storage.h"
#include "memcached_protocol.h"
#include "assign_thread.h"
#include "query_thread.h"

typedef struct worker_task_s
{
    ev_io pipe_watcher; /* MUST keep it as first field */
    int thread_id;
    int pipe_fd;
} worker_task_t;

#define WorkerTaskRef(p) (worker_task_t*)(((char*)p) - __offset_of(worker_task_t, pipe_watcher))

/* TODO combine this macro */
#define Log(taskptr, fmt, ...) do {     \
    assert(taskptr->error_log == NULL);                                 \
    if (!taskptr->error_log) { taskptr->error_log = slab_alloc(1024); } \
    snprintf(taskptr->error_log, 1024, fmt, ##__VA_ARGS__);             \
} while (0)

STATIC_MODIFIER int
send_response(struct query_task_s *qtask)
{
    struct query_response_s *qresponse;
    int retval, ret;
    ssize_t nwritten;
    struct iovec iov[2];

    retval = RET_SUCCESS;
    qresponse = &qtask->query_response;
    ret = set_block(qtask->sock_fd);
    assert(ret == 0);
    /* TODO handle return code of net_write */
    if (net_write(qtask->sock_fd, qresponse->resp_head_buf, qresponse->resp_head_buf_size) != (ssize_t)qresponse->resp_head_buf_size)
    {
        Log(qtask, "send header failed(fd = %d): %d - %s", qtask->sock_fd, errno, strerror(errno));
        retval = RET_FAILED;
        goto outlet;
    }
    if (qresponse->resp_data_buf != NULL)
    {
        if (qresponse->resp_tailor_buf == NULL)
        {
            assert(qresponse->resp_tailor_size == 0);
            if ((nwritten=net_write(qtask->sock_fd, qresponse->resp_data_buf, qresponse->resp_data_size)) != (ssize_t)qresponse->resp_data_size)
            {
                Log(qtask, "send data failed(fd = %d): %d - %s, need_write = %zd, written_bytes = %zd",
                        qtask->sock_fd, errno, strerror(errno), qresponse->resp_data_size, nwritten);
                retval = RET_FAILED;
                goto outlet;
            }
        }
        else
        {
            assert(qresponse->resp_tailor_size != 0);
            iov[0].iov_base = qresponse->resp_data_buf;
            iov[0].iov_len  = qresponse->resp_data_size;
            iov[1].iov_base = qresponse->resp_tailor_buf;
            iov[1].iov_len  = qresponse->resp_tailor_size;
            nwritten = net_writev(qtask->sock_fd, &iov[0], 2);
            if (nwritten != (ssize_t)iov[0].iov_len + (ssize_t)iov[1].iov_len)
            {
                Log(qtask, "send data failed(fd = %d): %d - %s, need_write = %zd, written_bytes = %zd",
                        qtask->sock_fd, errno, strerror(errno), iov[0].iov_len+iov[1].iov_len, nwritten);
                retval = RET_FAILED;
                goto outlet;
            }
        }
    }

outlet:
    return retval;
}

STATIC_MODIFIER int
build_response(struct query_task_s *qtask)
{
    struct query_response_s *qresponse;

    qresponse = &qtask->query_response;
    switch (qtask->state)
    {
    case s_set_success:
        strcpy(qresponse->resp_head_buf, MC_SET_DONE_STR);
        qresponse->resp_head_buf_size = strlen(qresponse->resp_head_buf);
        assert(qresponse->resp_data_size == 0);
        assert(qresponse->resp_data_buf == NULL);
        qresponse->resp_data_size = 0;
        qresponse->resp_data_buf = NULL;
        break;

    case s_set_failed:
        Log(qtask, "set cmd failed");
        strcpy(qresponse->resp_head_buf, MC_ERR_STR);
        qresponse->resp_head_buf_size = strlen(qresponse->resp_head_buf);
        assert(qresponse->resp_data_size == 0);
        assert(qresponse->resp_data_buf == NULL);
        qresponse->resp_data_size = 0;
        qresponse->resp_data_buf = NULL;
        break;

    case s_get_success:
        snprintf(qresponse->resp_head_buf, sizeof qresponse->resp_head_buf,
                 MC_GET_HEADER_PATTERN, qtask->query_request.key, 0, qresponse->resp_data_size);
        qresponse->resp_head_buf_size = strlen(qresponse->resp_head_buf);
        qresponse->resp_tailor_buf = MC_TERM_STR;
        qresponse->resp_tailor_size = strlen(MC_TERM_STR);
        break;

    case s_get_failed:
        /* TODO need to add s_get_nothing */
        Log(qtask, "get cmd failed");
        strcpy(qresponse->resp_head_buf, MC_GET_DONE_STR);
        qresponse->resp_head_buf_size = strlen(qresponse->resp_head_buf);
        assert(qresponse->resp_data_size == 0);
        assert(qresponse->resp_data_buf == NULL);
        qresponse->resp_data_size = 0;
        qresponse->resp_data_buf = NULL;
        break;

    case s_delete_success:
        strcpy(qresponse->resp_head_buf, MC_DEL_DONE_STR);
        qresponse->resp_head_buf_size = strlen(qresponse->resp_head_buf);
        assert(qresponse->resp_data_size == 0);
        assert(qresponse->resp_data_buf == NULL);
        qresponse->resp_data_size = 0;
        qresponse->resp_data_buf = NULL;
        break;

    case s_delete_failed:
        /* TODO need to add s_del_nonexists */
        Log(qtask, "delete cmd failed");
        strcpy(qresponse->resp_head_buf, MC_ERR_STR);
        qresponse->resp_head_buf_size = strlen(qresponse->resp_head_buf);
        assert(qresponse->resp_data_size == 0);
        assert(qresponse->resp_data_buf == NULL);
        qresponse->resp_data_size = 0;
        qresponse->resp_data_buf = NULL;
        break;

    case s_return_failed:
        /* have been logged in assign thread */
        strcpy(qresponse->resp_head_buf, MC_ERR_STR);
        qresponse->resp_head_buf_size = strlen(qresponse->resp_head_buf);
        assert(qresponse->resp_data_size == 0);
        assert(qresponse->resp_data_buf == NULL);
        qresponse->resp_data_size = 0;
        qresponse->resp_data_buf = NULL;
        break;

    default:
        log_error("Unknown query task state: %d", qtask->state);
        break;
    }

    return RET_SUCCESS;
}

STATIC_MODIFIER int
handle_request(struct query_task_s *qtask)
{
    struct query_request_s *qrequest;
    struct query_response_s *qresponse;
    int ret, retval;

    retval = RET_SUCCESS;
    qrequest = &qtask->query_request;
    qresponse = &qtask->query_response;
    assert(qrequest != NULL);

    if (qrequest->parse_status != ps_succ)
    {
        assert(qtask->state == s_return_failed);
        qtask->state = s_return_failed;
    }
    else
    {
        switch (qrequest->cmd_type)
        {
        case cmd_set:
            ret = update_block(g_storage, qrequest->key, strlen(qrequest->key), qrequest->data, qrequest->data_size);
            if (ret == 0)
                qtask->state = s_set_success;
            else
            {
                qtask->state = s_set_failed;
                retval = RET_FAILED;
            }
            break;

        case cmd_get:
            assert(qresponse->resp_data_buf == NULL);
            ret = find_block(g_storage, qrequest->key, strlen(qrequest->key), &qresponse->resp_data_buf, &qresponse->resp_data_size);
            if (ret == 0)
                qtask->state = s_get_success;
            else
            {
                qtask->state = s_get_failed;
                retval = RET_FAILED;
            }
            break;

        case cmd_delete:
            ret = delete_block(g_storage, qrequest->key, strlen(qrequest->key));
            if (ret == 0)
                qtask->state = s_delete_success;
            else
            {
                qtask->state = s_delete_failed;
                retval = RET_FAILED;
            }
            break;

        default:
            log_error("Unknown cmd type: %d", qrequest->cmd_type);
            retval = RET_FAILED;
            goto outlet;
            break;
        }
    }
    build_response(qtask);

outlet:
    return retval;
}

STATIC_MODIFIER void
pipe_event_handler(struct ev_loop *loop, ev_io *w, int event)
{
    struct worker_task_s *wtask;
    struct query_task_s *qtask;

    assert(loop != NULL);

    if (event == EV_READ)
    {
        wtask = WorkerTaskRef(w);
        if (read(wtask->pipe_fd, &qtask, sizeof qtask) != sizeof qtask)
        {
            log_warn("read query_task failed from pipe[%d]: %d - %s", wtask->pipe_fd, errno, strerror(errno));
        }
        else
        {
            qtask->thread_id = wtask->thread_id;
            handle_request(qtask);
            send_response(qtask);
            destroy_query_task(loop, qtask);
        }
    }
    else
    {
        log_error("got unexpected event = %d, pipe ev_io = %p", event, w);
    }
}

void *
new_worker_thread(void *arg)
{
    struct worker_thread_arg_s *worker_arg = arg;
    struct worker_task_s *wtask;
    struct ev_loop *worker_loop;

    wtask = slab_alloc(sizeof *wtask);
    if (wtask == NULL)
    {
        log_no_mem;
        abort();
    }

    wtask->thread_id = worker_arg->thread_id;
    wtask->pipe_fd = worker_arg->pipe_fd;
    worker_loop = ev_loop_new(EVBACKEND_EPOLL);

    ev_io_init(&wtask->pipe_watcher, pipe_event_handler, wtask->pipe_fd, EV_READ);
    ev_io_start(worker_loop, &wtask->pipe_watcher);
    sem_post(&g_runtime_vars.sem_barrier); /* tell main thread that workers are ready */
    log_notice("query thread[%d] initialized successfully", wtask->thread_id);

    ev_run(worker_loop, 0);

    return NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
