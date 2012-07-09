#ifndef _INDEXLOG_H_
#define _INDEXLOG_H_
/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | index log header                                                     |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-02-13 18:08                                            |
 * +----------------------------------------------------------------------+
 */

#include "storage.h"

enum op_type_t
{
    index_unknown_op,
    index_append,
    index_modify,
    index_del,
};

struct index_log_item_s
{
    enum op_type_t type;
    union index_key_t *index_key;
    struct index_value_t *index_value;
};

struct index_logger_s;

struct index_logger_s * index_logger_create(const char * restrict index_log_dir, const char * restrict index_log_filename, bool fsync);
struct index_logger_s * index_logger_open(const char * restrict index_log_dir, const char * restrict index_log_filename);
int index_logger_append(struct index_logger_s *il, struct index_log_item_s *ili);
struct index_log_item_s *index_logger_fetch(struct index_logger_s *il);
int index_logger_free_item(struct index_log_item_s *ili);
int index_logger_flush(struct index_logger_s *il);
int index_logger_close(struct index_logger_s *il);

#endif /* ! _INDEXLOG_H_ */
/* vim: set expandtab tabstop=4 shiftwidth=4 ft=c foldmethod=marker: */
