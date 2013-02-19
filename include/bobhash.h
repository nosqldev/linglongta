
#ifndef _BOBHASH_H_
#define _BOBHASH_H_

#include <stdio.h>      /* defines printf for tests */
#include <time.h>       /* defines time_t for timings in the test */
#include <stdint.h>     /* defines uint32_t etc */
#include <sys/param.h>  /* attempt to define endianness */

uint32_t hashword(const uint32_t *k, size_t length, uint32_t initval);
void hashword2(const uint32_t *k, size_t length, uint32_t *pc, uint32_t *pb);
uint32_t hashlittle(const void *key, size_t length, uint32_t initval);
void hashlittle2(const void *key, size_t length, uint32_t   *pc, uint32_t *pb);
uint32_t bobhash(const void *key, size_t length) __attribute__((always_inline));
uint64_t bobhash64(const void *key, size_t length) __attribute__((always_inline));

#endif /* ! _BOBHASH_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
