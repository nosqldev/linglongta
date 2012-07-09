#ifndef _UTILS_H_
#define _UTILS_H_
/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | utils for small objects store                                        |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-18 13:57                                            |
 * +----------------------------------------------------------------------+
 */

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

#if defined(__CPU_i386) || defined(__CPU_x86_64) || defined(__x86_64__)

/* currently, it's implemented for *GCC* on *x86_64* ONLY */
#define memory_barrier()  asm volatile("mfence":::"memory")

/* atomic_*_set/get/equal/lte is NOT atomic */
typedef struct atomic_uint_s { volatile uint64_t counter; } atomic_uint_t;
#define atomic_uint_set(p,i)    do { memory_barrier(); ((p)->counter) = (i); } while(0)
#define atomic_uint_get(p)      ({ memory_barrier(); ((p)->counter); })
#define atomic_uint_equal(p, v) ({ memory_barrier(); (((p)->counter) == v); })
#define atomic_uint_lte(p, v)   ({ memory_barrier(); (((p)->counter) >= v); })
#define atomic_uint_add(p, i)   __sync_add_and_fetch(&((p)->counter), i)
#define atomic_uint_sub(p, i)   __sync_sub_and_fetch(&((p)->counter), i)
#define atomic_uint_inc(p)      atomic_uint_add(p, 1)
#define atomic_uint_dec(p)      atomic_uint_sub(p, 1)
#define atomic_uint_cas(p, i, newvalue) __sync_bool_compare_and_swap(&((p)->counter), i, newvalue)

typedef struct atomic_int_s { volatile int64_t counter; } atomic_int_t;
#define atomic_int_set(p, i)    atomic_uint_set(p, i)
#define atomic_int_get(p)       atomic_uint_get(p)
#define atomic_int_equal(p, v)  atomic_uint_equal(p, v)
#define atomic_int_lte(p, v)    atomic_uint_lte(p, v)
#define atomic_int_add(p, i)    (void)__sync_add_and_fetch(&((p)->counter), i)
#define atomic_int_sub(p, i)    (void)__sync_sub_and_fetch(&((p)->counter), i)
#define atomic_int_inc(p) atomic_int_add(p, 1)
#define atomic_int_dec(p) atomic_int_sub(p, 1)
#define atomic_int_cas(p, i, newvalue) __sync_bool_compare_and_swap(&((p)->counter), i, newvalue)

#endif /* __CPU_i386 || __CPU_x86_64 || __x86_64__*/

int set_nonblock(int fd);
int set_block(int fd);

#endif /* ! _UTILS_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
