#ifndef _MHASH_H_
#define _MHASH_H_
/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | 64 bit MurmurHash                                                    |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-07-11 14:45                                            |
 * +----------------------------------------------------------------------+
 */

//-----------------------------------------------------------------------------
// MurmurHash2, 64-bit versions, by Austin Appleby
//
// https://sites.google.com/site/murmurhash/MurmurHash2_64.cpp?attredirects=0

#include <stdint.h>

uint64_t MurmurHash64A(const void * key, int len);

#endif /* ! _MHASH_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
