/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | implement assign thread: listen, accept, recv and forward requests   |
 * | to workers                                                           |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-29 21:40                                            |
 * +----------------------------------------------------------------------+
 */

#include "assign_thread.h"
#include "iolayer.h"
#include "storage.h"

#define AssignTaskRef(p) ((struct assign_task_s*)p)
#define QueryTaskRef(p) (query_task_t *)(((void *)p) - __offset_of(query_task_t, query_io_w))
#define QueryTaskRefFromTimer(p) (query_task_t *)(((void *)p) - __offset_of(query_task_t, query_timer_w))

#define Log(taskptr, fmt, ...) do {     \
    assert(taskptr->error_log == NULL);                                 \
    if (!taskptr->error_log) { taskptr->error_log = slab_alloc(1024); } \
    snprintf(taskptr->error_log, 1024, "(%s:%d) "fmt, __FILE__, __LINE__, ##__VA_ARGS__);             \
} while (0)

struct assign_task_s
{
    ev_io listen_io_w;
    struct ev_loop *local_loop;
    int listen_fd;
} assign_task_t;

STATIC_MODIFIER struct atomic_uint_s last_fd;

STATIC_MODIFIER bool recv_req_head(struct query_task_s *task);
STATIC_MODIFIER bool recv_req_data(struct query_task_s *task);
STATIC_MODIFIER bool parse_query_request_head(struct query_task_s *task);
STATIC_MODIFIER bool dispatch_query_task(struct ev_loop *local_loop, struct query_task_s *qt);
STATIC_MODIFIER void change_query_task_state(struct query_task_s *task, int new_state);

STATIC_MODIFIER int
init_listen_socket()
{
    int listenfd;
    int retval;
    struct sockaddr_in server_addr;
    int flag;
    int ret;

    retval = RET_FAILED;
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0)
    {
        log_error("socket() failed: %d - %s", errno, strerror(errno));
        retval = RET_FAILED;
        goto outlet;
    }
    retval = listenfd;
    if ((ret=set_nonblock(listenfd)) != 0)
    {
        log_error("set nonblock failed: %d - %s", ret, strerror(ret));
        retval = RET_FAILED;
        goto outlet;
    }

    flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_addr.s_addr = g_settings.bind_ip;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_settings.accept_port);
    if (bind(listenfd, (struct sockaddr *)(&server_addr), sizeof(server_addr)) != 0)
    {
        log_error("bind port %d failed: %d - %s", g_settings.accept_port, errno, strerror(errno));
        retval = RET_FAILED;
        goto outlet;
    }
    if (listen(listenfd, 128) < 0)
    {
        log_error("listen() failed: %d - %s", errno, strerror(errno));
        retval = RET_FAILED;
        goto outlet;
    }

outlet:
    log_debug("listenfd = %d", retval);
    return retval;
}

STATIC_MODIFIER void
query_io_event_handler(struct ev_loop *loop, ev_io *io_w, int event)
{
    struct query_task_s *task;
    bool need_loop;

    assert(loop != NULL);
    assert(io_w != NULL);
    if ((event != EV_READ) && (event != EV_WRITE))
    {
        log_error("Unknow event = %d", event);
        return;
    }

    task = QueryTaskRef(io_w);
    do
    {
        switch (task->state)
        {
        case s_recv_req_head:
            need_loop = recv_req_head(task);
            break;

        case s_parse_req_head:
            need_loop = parse_query_request_head(task);
            break;

        case s_recv_req_data:
            need_loop = recv_req_data(task);
            break;

        case s_dispatch_query_task:
            need_loop = dispatch_query_task(loop, task);
            break;

        case s_return_failed:
            need_loop = dispatch_query_task(loop, task);
            break;

        case s_destroy_recv:
            destroy_query_task(loop, task);
            need_loop = false;
            break;

        case s_do_sync:
            sync_storage(g_storage, task->query_request.key);
            change_query_task_state(task, s_destroy_recv);
            /* TODO send back to user: use async_coro */
            break;

        case s_do_power:
            sync_storage(g_storage, "all");
            destroy_query_task(loop, task);
            close_storage(g_storage);
            log_close();
            /* exit directly */
            exit(0);
            break;

        default:
            log_error("Unknown query task state: %d", task->state);
            change_query_task_state(task, s_destroy_recv);
            break;
        }
    } while(need_loop);
}

/**
 * Currently, I think this function is not necessary, it makes the code
 * more complex to understand.
 */
STATIC_MODIFIER void
change_query_task_state(struct query_task_s *task, int new_state)
{
    task->state = new_state;
    switch (new_state)
    {
    case s_recv_req_head:
        task->need_read = sizeof(task->query_request.query_head_buf) - task->have_read;
        if (task->need_read == 0)
        {
            assert(task->error_log == NULL);
            Log(task, "Request header is longger than expected");
            task->state = s_destroy_recv;
        }
        else
            task->state = s_recv_req_head;
        break;

    case s_recv_req_data:
        assert(task->error_log == NULL);
        break;

    case s_parse_req_head:
        /* nothing */
        break;

    case s_dispatch_query_task:
        assert(task->error_log == NULL);
        task->query_request.data_size -= 2; /* ignore tailing '\r\n' from data content */
        break;

    case s_destroy_recv:
        /* nothing */
        break;

    case s_return_failed:
        assert(task->error_log != NULL);
        break;

    case s_do_sync:
        /* nothing */
        break;

    case s_do_power:
        /* nothing */
        break;

    default:
        /* should never reach here */
        log_error("Unknown new state: %d", new_state);
        break;
    }
}

int
destroy_query_task(struct ev_loop *local_loop, struct query_task_s *task)
{
    struct timeval used;
    char log_buf[BUFSIZ] = {0};

    ev_io_stop(local_loop, &task->query_io_w);
    ev_timer_stop(local_loop, &task->query_timer_w);
    gettimeofday(&task->done_time, NULL);
    timeval_sub(&task->done_time, &task->connected_time, &used);

    if (task->error_log)
    {
        snprintf(log_buf, sizeof log_buf, "<Elapsed: %ld.%06lds> {tid = %d, from: %s, fd = %d} %s",
                 used.tv_sec, used.tv_usec, task->thread_id, task->query_ip, task->sock_fd, task->error_log);
        log_warn("%s", log_buf);
        slab_free(task->error_log);
    }
    else
    {
        snprintf(log_buf, sizeof log_buf, "%s <Elapsed: %ld.%06lds> {tid = %d, from: %s, fd = %d}",
                 cmd_type_name(task->query_request.cmd_type), used.tv_sec, used.tv_usec, task->thread_id, task->query_ip, task->sock_fd);
        log_notice("%s", log_buf);
    }

    close(task->sock_fd);
    if (task->query_request.data != NULL)
        slab_free(task->query_request.data);
    if (task->query_response.resp_data_buf != NULL)
        slab_free(task->query_response.resp_data_buf);
    slab_free(task);

    return 0;
}

STATIC_MODIFIER bool
parse_query_request_head(struct query_task_s *task)
{
    struct query_request_s *request;
    bool need_loop;
    char *p;
    char tmp_buf[sizeof(task->query_request.query_head_buf)];
    size_t data_len;

    need_loop = true;
    request = &task->query_request;
    if ((p=try_parse_memcached_string_head(request)) != NULL)
    {
        p += 2; /* move to next char after "\r\n" */
        request->query_head_buf_size = p - request->query_head_buf;
        assert(task->have_read >= (ssize_t)request->query_head_buf_size);
        data_len = task->have_read - request->query_head_buf_size;
        memmove(tmp_buf, p, data_len);
        *p = '\0';

        if ((parse_memcached_string_head(request) == RET_SUCCESS) &&
            (request->parse_status == ps_succ))
        {
            switch (request->cmd_type)
            {
            case cmd_set:
                request->data_size += 2; /* tailing '\r\n' */
                request->data = slab_alloc(request->data_size);
                /* TODO judge request->data != NULL */
                assert(request->data != NULL);
                memmove(request->data, tmp_buf, data_len);

                task->need_read = request->data_size - data_len;
                if (task->need_read == 0)
                {
                    change_query_task_state(task, s_dispatch_query_task);
                    need_loop = true;
                }
                else if (task->need_read > 0)
                {
                    change_query_task_state(task, s_recv_req_data);
                    need_loop = false;
                }
                else /* task->need_read < 0 */
                {
                    change_query_task_state(task, s_destroy_recv);
                }
                break;

            case cmd_get:
                change_query_task_state(task, s_dispatch_query_task);
                break;

            case cmd_delete:
                change_query_task_state(task, s_dispatch_query_task);
                break;

            case cmd_sync:
                change_query_task_state(task, s_do_sync);
                break;

            case cmd_power:
                change_query_task_state(task, s_do_power);
                break;

            default:
                /* should never reach here */
                Log(task, "Oops! cmd type: %d", request->cmd_type);
                change_query_task_state(task, s_destroy_recv);
                break;
            }
        }
        else
        {
            request->query_head_buf[ strlen(request->query_head_buf) - 2 ] = '\0'; /* truncate tailing '\r\n' */
            log_debug("readbytes = %zd, request = %s", task->have_read, request->query_head_buf);
            Log(task, "parse request failed, cmd type: %d", request->cmd_type);
            change_query_task_state(task, s_return_failed);
        }
    }
    else
    {
        change_query_task_state(task, s_recv_req_head);
    }

    return need_loop;
}

STATIC_MODIFIER bool
recv_req_head(struct query_task_s *task)
{
    ssize_t nread;
    struct query_request_s *request;
    bool need_loop;

    request = &task->query_request;
    need_loop = false;

    nread = net_read(task->sock_fd, &(request->query_head_buf[ request->query_head_buf_size ]), task->need_read);
    if ((nread == 0) && (task->need_read != 0))
    {
        /* still have data to be read but remote peer close connection */
        change_query_task_state(task, s_destroy_recv);
    }
    else if ((nread > 0) && (nread <= task->need_read))
    {
        task->have_read += nread;
        task->need_read = 0;
        change_query_task_state(task, s_parse_req_head);
        need_loop = true;
    }
    else if (nread < 0)
    {
        Log(task, "net_read(need_read = %zd, have_read = %zd) return %d - %s", task->need_read, task->have_read, errno, strerror(errno));
        change_query_task_state(task, s_destroy_recv);
        need_loop = true;
    }
    else
    {
        /* should never reach here */
        Log(task, "read request head failed: unknonwn");
        change_query_task_state(task, s_destroy_recv);
        need_loop = true;
    }

    return need_loop;
}

STATIC_MODIFIER bool
recv_req_data(struct query_task_s *task)
{
    bool need_loop;
    size_t data_len;
    ssize_t nread;
    struct query_request_s *request;

    need_loop = true;
    request = &task->query_request;

    if (task->need_read == 0)
    {
        /* have read complete before, probably in recv_req_head */
        change_query_task_state(task, s_dispatch_query_task);
        goto outlet;
    }

go:
    data_len = task->have_read - request->query_head_buf_size;
    nread = net_read(task->sock_fd, (void*)&request->data[data_len], task->need_read);
    if ((nread == 0) && (task->need_read != 0))
    {
        /* still have data to be read but remote peer close connection */
        /* actually, the situation of task->need_read == 0 have been handled at
         * the entrance of this function */
        Log(task, "Remote reset connection unexpected, need_read = %zd, have_read = %zd",
            task->need_read, task->have_read);
        change_query_task_state(task, s_destroy_recv);
    }
    else if ((nread > 0) && (nread < task->need_read))
    {
        /* read have not been finished */
        task->have_read += nread;
        task->need_read -= nread;
        need_loop = false;
    }
    else if (nread == task->need_read)
    {
        /* read done */
        change_query_task_state(task, s_dispatch_query_task);
    }
    else if (nread < 0)
    {
        if ((errno == EAGAIN) || (errno == EINTR))
        {
            log_error("net_read return %d", errno);
            goto go;
        }
        else
        {
            Log(task, "read request data failed: %d - %s", errno, strerror(errno));
            change_query_task_state(task, s_destroy_recv);
        }
    }
    else
    {
        /* should never reach here */
        Log(task, "read request head failed: unknonwn");
        change_query_task_state(task, s_destroy_recv);
    }

outlet:

    return need_loop;
}

STATIC_MODIFIER bool 
dispatch_query_task(struct ev_loop *local_loop, struct query_task_s *task)
{
    intptr_t query_task_pointer;
    ssize_t nwrite;

    ev_io_stop(local_loop, &task->query_io_w);
    ev_timer_stop(local_loop, &task->query_timer_w);

    query_task_pointer = (intptr_t)task;
    /* XXX NOT real atomic && non-threadsafe here, but threadsafe is not
     *     necessary here, since only one assign thread */
    if (atomic_uint_lte(&last_fd, g_settings.worker_thread_cnt))
    {
        atomic_uint_set(&last_fd, 0);
    }
    nwrite = write(g_pipe_fd[atomic_uint_get(&last_fd) * 2 + 1], &query_task_pointer, sizeof(query_task_pointer));
    atomic_uint_inc(&last_fd);

    /* assume that write(2) successfully */
    /* XXX assert(nwrite == sizeof(query_task_pointer));*/
    if (nwrite != sizeof(query_task_pointer))
    {
        log_error("write to pipe failed, fd = %d, nwrite = %zd: %d - %s",
                  g_pipe_fd[atomic_uint_get(&last_fd) * 2 + 1], nwrite, errno, strerror(errno));
        abort();
    }
    return false;
}

STATIC_MODIFIER void
query_timer_event_handler(struct ev_loop *loop, ev_timer *timer_w, int event)
{
    struct query_task_s *task;

    assert(loop != NULL);
    assert(timer_w != NULL);

    task = QueryTaskRefFromTimer(timer_w);
    if (event == EV_TIMEOUT)
    {
        Log(task, "read request timeout: have_read = %zd, need_read = %zd", task->have_read, task->need_read);
        destroy_query_task(loop, task);
    }
    else
    {
        log_error("Unknow event = %d", event);
        return;
    }
}

STATIC_MODIFIER struct query_task_s *
do_accept_connection(int accept_fd, struct sockaddr_in *remote_peer)
{
    struct query_task_s *qt;

    qt = slab_calloc(sizeof(*qt));
    if (qt == NULL)
    {
        return NULL;
    }
    qt->sock_fd = accept_fd;
    gettimeofday(&(qt->connected_time), NULL);
    memmove(&(qt->query_sockaddr), remote_peer, sizeof(*remote_peer));
    inet_ntop(AF_INET, &(remote_peer->sin_addr),
              qt->query_ip, sizeof(qt->query_ip));
    set_nonblock(qt->sock_fd);
    change_query_task_state(qt, s_recv_req_head);

    return qt;
}

STATIC_MODIFIER void
accept_connection(struct assign_task_s *as)
{
    int sockfd;
    struct sockaddr_in remote_peer;
    socklen_t len;
    struct query_task_s *qt;

    len = sizeof(remote_peer);
    sockfd = accept(as->listen_fd, (struct sockaddr *)&remote_peer, &len);
    if (sockfd < 0)
    {
        log_warn("accept() failed: %d - %s, listen_fd = %d", errno, strerror(errno), as->listen_fd);
        return;
    }

    qt = do_accept_connection(sockfd, &remote_peer);
    if (qt == NULL)
    {
        log_no_mem;
        return;
    }

    ev_io_init(&qt->query_io_w, query_io_event_handler, qt->sock_fd, EV_READ);
    ev_io_start(as->local_loop, &qt->query_io_w);

    ev_timer_init(&qt->query_timer_w, query_timer_event_handler, g_settings.read_timeout * 1.0 / 1000, 0.);
    ev_timer_start(as->local_loop, &qt->query_timer_w);

    /* log_debug("accept new connection: fd = %d", sockfd); */
}

STATIC_MODIFIER void
accept_event_handler(struct ev_loop *loop, ev_io *w, int event)
{
    assert(loop != NULL);
    struct assign_task_s *as;

    as = AssignTaskRef(w);

    if (event == (int)EV_UNDEF)
    {
        /* should never reach here */
        log_error("assign_thread got undef event");
    }
    else if (event == EV_READ)
    {
        accept_connection(as);
    }
    else
    {
        /* should never reach here */
        log_error("got unknown event: %d", event);
    }
}

void *
new_assign_thread(void *arg __attribute__((unused)))
{
    int listenfd;
    struct assign_task_s *at;

    atomic_uint_set(&last_fd, 0);
    listenfd = init_listen_socket();
    if (listenfd == RET_FAILED)
        return NULL;
    at = slab_alloc(sizeof(*at));
    at->listen_fd = listenfd;
    at->local_loop = ev_loop_new(EVBACKEND_EPOLL);

    ev_io_init(&at->listen_io_w, accept_event_handler, listenfd, EV_READ);
    ev_io_start(at->local_loop, &at->listen_io_w);
    sem_post(&g_runtime_vars.sem_barrier); /* tell main thread that accpet() is ready(actually there is a small time window) */

    log_notice("assign thread initialized successfully");

    ev_run(at->local_loop, 0);

    return NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
