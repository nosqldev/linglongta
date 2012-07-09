/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | replay index log so that we can recover index data                   |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-02-23 14:00                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "indexlog.h"

static char indexlog_op_type_mappings[][32] = {"index_unknown_op", "index_append", "index_modify", "index_del"};

static void
die(const char *fmt, ...)
{
    char output_buf[BUFSIZ] = {0};
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(output_buf, sizeof output_buf, fmt, ap);
    va_end(ap);

    fprintf(stderr, "replay_journal: error\n");
    fprintf(stderr, "\t%s\n", output_buf);

    exit(-1);
}

static void
print_indexlog(const char *filepath)
{
    char *file_dir;
    char *file_name;

    file_dir = dirname(strdup((char*)filepath));
    file_name = basename(strdup((char*)filepath));

    printf("reading %s under %s\n", file_name, file_dir);

    struct index_logger_s *il = index_logger_open(file_dir, file_name);
    if (il == NULL) die("Cannot open %s", filepath);

    struct index_log_item_s *ili;
    char index_key_buf[sizeof(union index_key_t) + 1];
    int cnt = 0;
    while ((ili = index_logger_fetch(il)) != NULL)
    {
        cnt ++;
        snprintf(index_key_buf, sizeof index_key_buf, "%s", ili->index_key->key);
        index_key_buf[sizeof index_key_buf - 1] = '\0';
        printf("[%d] Type: %s(%d),\tKey: %s,\t{ Value: crc32=%u,\tblock_id=%u,\tdata_size=%zu,\toffset=%zu }\n",
               cnt,
               indexlog_op_type_mappings[ili->type], ili->type,
               index_key_buf,
               ili->index_value->crc32, ili->index_value->block_id,
               ili->index_value->data_size, ili->index_value->offset);
    }
    index_logger_close(il);
    printf("[done]\n");
}

int
main(int argc, char **argv)
{
    slab_init(0, 1.125);
    if (argc == 2)
    {
        /* replay indexlog then rebuild index */
        die("not available now");
    }
    else if (argc == 3)
    {
        /* read indexlog and output text info */
        if (strcmp(argv[1], "-d") == 0)
        {
            if (access(argv[2], R_OK) == 0)
            {
                print_indexlog(argv[2]);
            } 
            else
            {
                die("%s is not readable", argv[2]);
            }
        }
    }
    else
    {
        die("arg error");
    }

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
