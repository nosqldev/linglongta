#ifndef _ASSIGN_THREAD_H_
#define _ASSIGN_THREAD_H_
/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | header file of assign thread                                         |
 * +----------------------------------------------------------------------+
 * | Author: jingmi@gmail.com                                             |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-29 21:27                                            |
 * +----------------------------------------------------------------------+
 */

#include "global.h"
#include "memcached_protocol.h"
#include "query_thread.h"
#include "ev.h"

void *new_assign_thread(void *arg);
int  destroy_query_task(struct ev_loop *local_loop, struct query_task_s *task);

#endif /* ! _ASSIGN_THREAD_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
