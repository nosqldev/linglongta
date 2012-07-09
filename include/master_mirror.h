#ifndef _MASTER_MIRROR_H_
#define _MASTER_MIRROR_H_
/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | Master node with mirror syncing policy                               |
 * | Data nodes are quite the same under this master                      |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-03-07 17:53                                            |
 * +----------------------------------------------------------------------+
 */

#define MASTER_BIND_IP "0.0.0.0"
#define MASTER_ACCEPT_PORT (6000)
#define MASTER_READ_TIMEOUT (1000 * 60)
#define MASTER_WRITE_TIMEOUT (1000 * 10)
#define MASTER_LOG_PATH "./log/master.log"
#define MASTER_MEMPOOL_SIZE (128 * 1024 * 1024)
#define MASTER_COROUTINE_STACKSIZE (64 * 128)

#define MASTER_CONNECT_SLAVE_TIMEOUT (1000 * 5)
#define MASTER_SEND_SLAVE_TIMEOUT (1000)

#endif /* ! _MASTER_MIRROR_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
