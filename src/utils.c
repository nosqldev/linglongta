/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | implementation of utils                                              |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-18 13:58                                            |
 * +----------------------------------------------------------------------+
 */

#include "utils.h"

int
set_nonblock(int fd)
{
    int flag;
    int ret = 0;

    if ((flag = fcntl(fd, F_GETFL)) < 0)
    {
        return errno;
    }
    if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0)
    {
        return errno;
    }

    return ret;
}

int
set_block(int fd)
{
    int flag;
    int ret = 0;

    if ((flag = fcntl(fd, F_GETFL)) < 0)
    {
        return errno;
    }
    flag = (flag & (~O_NONBLOCK));
    if (fcntl(fd, F_SETFL, flag) < 0)
    {
        return errno;
    }

    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
