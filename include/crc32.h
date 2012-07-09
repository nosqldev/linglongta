/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | CRC32 implementation                                                 |
 * | Introduce crc32 for data verification                                |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-07 09:42                                            |
 * +----------------------------------------------------------------------+
 */

#ifndef _CRC32_C_
#define _CRC32_C_

#include <stdio.h>
#include <stdint.h>

uint32_t crc32_calc(const char *buf, size_t size);

#endif /* ! _CRC32_H_ */
