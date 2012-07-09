/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | IO utils                                                             |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-05 01:15                                            |
 * +----------------------------------------------------------------------+
 */

#include <sys/select.h>
#include <sys/uio.h>
#include "iolayer.h"
#include "utils.h"

ssize_t
disk_read(int fd, void *buf, size_t count)
{
    ssize_t nread;

    nread = read(fd, buf, count);
    if (nread > 0)
        atomic_uint_add(&g_runtime_vars.io_stats.disk_read_bytes, (uint64_t)nread);

    return nread;
}

ssize_t
disk_write(int fd, const void *buf, size_t count)
{
    ssize_t nwrite;

    nwrite = write(fd, buf, count);
    if (nwrite > 0)
        atomic_uint_add(&g_runtime_vars.io_stats.disk_write_bytes, (uint64_t)nwrite);

    return nwrite;
}

ssize_t
net_read(int fd, void *buf, size_t count)
{
    ssize_t nread;

    nread = read(fd, buf, count);
    if (nread > 0)
        atomic_uint_add(&g_runtime_vars.io_stats.net_read_bytes, (uint64_t)nread);

    return nread;
}

ssize_t
net_write(int fd, const void *buf, size_t count)
{
    ssize_t nwrite;

    nwrite = write(fd, buf, count);
    if (nwrite > 0)
        atomic_uint_add(&g_runtime_vars.io_stats.net_write_bytes, (uint64_t)nwrite);

    return nwrite;
}

ssize_t
net_writev(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t nwrite;

    nwrite = writev(fd, iov, iovcnt);
    if (nwrite > 0)
        atomic_uint_add(&g_runtime_vars.io_stats.net_write_bytes, (uint64_t)nwrite);

    return nwrite;
}

in_addr_t
tcp_pton(const char *presentation_ip)
{
    in_addr_t numeric_ip;
    if (inet_pton(AF_INET, presentation_ip, &numeric_ip) != 1)
    {
        return 0;
    }

    return numeric_ip;
}

int
blocked_connect(in_addr_t host, in_port_t port)
{
    int ret;
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
        return RET_FAILED;

    memset(&server_addr, 0, sizeof server_addr);
    server_addr.sin_addr.s_addr = host;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;

    ret = connect(sockfd, (struct sockaddr*)&server_addr, sizeof server_addr);
    if (ret == 0)
        return sockfd;

    return RET_FAILED;
}

int
timed_connect(in_addr_t host, in_port_t port, uint32_t sec, uint32_t msec)
{
    int ret;
    int sockfd;
    struct sockaddr_in server_addr;
    fd_set wset, rset;
    struct timeval to;
    socklen_t len;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
        return RET_FAILED;
    set_nonblock(sockfd);

    memset(&server_addr, 0, sizeof server_addr);
    server_addr.sin_addr.s_addr = host;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    FD_ZERO(&wset);
    FD_ZERO(&rset);
    FD_SET(sockfd, &wset);
    FD_SET(sockfd, &rset);
    to.tv_sec = sec;
    to.tv_usec = msec * 1000;

    if ((ret = connect(sockfd, (struct sockaddr*)&server_addr, sizeof server_addr)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            close(sockfd);
            return RET_FAILED;
        }
    }
    else if (ret == 0)
    {
        set_block(sockfd);
        return sockfd;
    }

    if ((ret = select(sockfd+1, &rset, &wset, NULL, &to)) <= 0)
    {
        /* connect timeout */
        close(sockfd);
        return RET_FAILED;
    }
    if (FD_ISSET(sockfd, &wset) || FD_ISSET(sockfd, &rset))
    {
        len = sizeof(ret);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &ret, &len) < 0)
        {
            close(sockfd);
            return RET_FAILED;
        }
        if (ret != 0)
        {
            printf("%s\n", strerror(ret));
            close(sockfd);
            return RET_FAILED;
        }
    }
    set_block(sockfd);

    return sockfd;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
