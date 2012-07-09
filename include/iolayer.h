#ifndef _IOUTILS_H_
#define _IOUTILS_H_
/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | header of IO utils                                                   |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-12-05 01:14                                            |
 * +----------------------------------------------------------------------+
 */

#include "global.h"

ssize_t disk_read(int fd, void *buf, size_t count);
ssize_t disk_write(int fd, const void *buf, size_t count);
ssize_t net_read(int fd, void *buf, size_t count);
ssize_t net_write(int fd, const void *buf, size_t count);
ssize_t net_writev(int fd, const struct iovec *iov, int iovcnt);
in_addr_t tcp_pton(const char *presentation_ip);
int blocked_connect(in_addr_t host, in_port_t port);
int timed_connect(in_addr_t host, in_port_t port, uint32_t sec, uint32_t msec);

#endif /* ! _IOUTILS_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
