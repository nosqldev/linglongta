/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | implement index log                                                  |
 * | index logger will maintain it's I/O buffer by itself                 |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-02-13 18:08                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "slab.h"
#include "crc32.h"
#include "indexlog.h"

#ifndef LOG_BUF_SIZE
    #define LOG_BUF_SIZE (1024*8 - sizeof(uint32_t) * 2)
#endif /* ! LOG_BUF_SIZE */
#define MAGIC_HEADER (0x1d810900) /* 1d8109 = idxlog, version: 00 */
#define INDEX_LOG_ITEM_SIZE (sizeof(enum op_type_t) + sizeof(union index_key_t) + sizeof(struct index_value_t))

#pragma pack(1)

struct buf_s
{
    /* Be careful if you want to change the data type
     * of crc32_sum and count, or you want to add new
     * item in this structure.
     * Take a look at the definition of LOG_BUF_SIZE.
     */
    uint32_t crc32sum;
    uint32_t count; /* number of index_log_item_s in buf_ptr */
    char buf_ptr[LOG_BUF_SIZE];
};

#pragma pack()

struct index_logger_s
{
    int fd;
    bool sync; /* do we need sync data to disk immediately */

    pthread_mutex_t working_lock;   /* while appending to working_buffer */
    pthread_mutex_t storing_lock;   /* while writing storing_buffer to disk */

    struct buf_s *working_buffer;
    struct buf_s *storing_buffer;

    int curr_working_buf;
    struct buf_s buffer1;
    struct buf_s buffer2;

    off_t  fetch_pos; /* pos in index log file */
    ssize_t fetch_records_amount; /* number of records in this block */
    ssize_t fetch_id;  /* id of index_log_item_s in buffer to be read */

    char index_log_dir[PATH_MAX];
    char index_log_filename[FILENAME_MAX];
};

#define Lock(buf) do { pthread_mutex_lock(&(il->buf##_lock)); } while (0)
#define UnLock(buf) do { pthread_mutex_unlock(&(il->buf##_lock)); } while (0)

struct index_logger_s *
index_logger_create(const char * restrict index_log_dir, const char * restrict index_log_filename, bool sync)
{
    struct index_logger_s *il;
    char path[PATH_MAX];
    uint32_t magic_header;
    ssize_t nwrite;
    struct stat sb;

    magic_header = htonl(MAGIC_HEADER);
    il = slab_calloc(sizeof(*il));
    pthread_mutex_init(&il->working_lock, NULL);
    pthread_mutex_init(&il->storing_lock, NULL);
    if (il == NULL)
        goto done;
    strcpy(il->index_log_dir, index_log_dir);
    strcpy(il->index_log_filename, index_log_filename);

    strcpy(path, index_log_dir);
    strcat(path, "/");
    strcat(path, index_log_filename);

    if ((stat(index_log_dir, &sb) != 0) && (errno == ENOENT))
    {
        if (mkdir(index_log_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXGRP) != 0)
        {
            log_error("cannot mkdir: %s, error: %d - %s", index_log_dir,
                      errno, strerror(errno));
            goto err;
        }
    }

    if (sync)
        il->fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_APPEND | O_SYNC, S_IRUSR | S_IWUSR);
    else
        il->fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, S_IRUSR | S_IWUSR);
    if (il->fd < 0)
        goto err;
    nwrite = write(il->fd, &magic_header, sizeof magic_header);
    assert(nwrite == sizeof magic_header);
    fsync(il->fd);
    il->curr_working_buf = 1;
    il->working_buffer = &il->buffer1;
    il->storing_buffer = &il->buffer2;
    il->fetch_records_amount = -1;
    il->fetch_id = -1;
    goto done;

err:
    if (il != NULL)
    {
        pthread_mutex_destroy(&il->working_lock);
        pthread_mutex_destroy(&il->storing_lock);

        slab_free(il);
        il = NULL;
    }

done:
    return il;
}

struct index_logger_s *
index_logger_open(const char * restrict index_log_dir, const char * restrict index_log_filename)
{
    struct index_logger_s *il;
    char path[PATH_MAX];

    il = slab_calloc(sizeof(*il));
    pthread_mutex_init(&il->working_lock, NULL);
    pthread_mutex_init(&il->storing_lock, NULL);
    if (il == NULL)
        goto done;

    strcpy(il->index_log_dir, index_log_dir);
    strcpy(il->index_log_filename, index_log_filename);
    strcpy(path, index_log_dir);
    strcat(path, "/");
    strcat(path, index_log_filename);

    il->fd = open(path, O_RDONLY);
    if (il->fd < 0)
    {
        pthread_mutex_destroy(&il->working_lock);
        pthread_mutex_destroy(&il->storing_lock);

        slab_free(il);
        il = NULL;
        goto done;
    }
    il->curr_working_buf = 0; /* To denote that in read mode */
    il->fetch_records_amount = -1;
    il->fetch_id = -1;

done:
    return il;
}

static inline int
switch_buffer(struct index_logger_s *il)
{
    assert((il->curr_working_buf==1) || (il->curr_working_buf==2));

    if (il->curr_working_buf == 1)
    {
        il->working_buffer = &il->buffer2;
        il->storing_buffer = &il->buffer1;
        il->curr_working_buf = 2;
    }
    else
    {
        il->working_buffer = &il->buffer1;
        il->storing_buffer = &il->buffer2;
        il->curr_working_buf = 1;
    }
    il->working_buffer->crc32sum = 0;
    il->working_buffer->count = 0;

    return 0;
}

/**
 * @brief Only flush index log to disk, will not change buffer pointer
 *        Before use, make sure that:
 *        1. il->storing_buffer->count > 0
 *        2. Have locked working_lock & storing_lock
 */
static inline int
sync_index_log(struct index_logger_s *il)
{
    ssize_t nwrite;
    int retval;

    il->storing_buffer->crc32sum = crc32_calc(il->storing_buffer->buf_ptr, il->storing_buffer->count * INDEX_LOG_ITEM_SIZE);
    nwrite = write(il->fd,
                   (void*)il->storing_buffer,
                   il->storing_buffer->count * INDEX_LOG_ITEM_SIZE + sizeof(uint32_t) * 2);
    if (nwrite == (ssize_t)(il->storing_buffer->count * INDEX_LOG_ITEM_SIZE + sizeof(uint32_t) * 2))
    {
        fsync(il->fd);
        retval = RET_SUCCESS;
    }
    else
    {
        retval = RET_FAILED;
    }

    return retval;
}

int
index_logger_append(struct index_logger_s *il, struct index_log_item_s *ili)
{
    Lock(working);
    int retval;
    uintptr_t ptr;

    retval = RET_SUCCESS;
    if (LOG_BUF_SIZE < ((il->working_buffer->count + 1 ) * INDEX_LOG_ITEM_SIZE))
    {
        /* working_buffer is full */
        Lock(storing);
        switch_buffer(il);
        if (sync_index_log(il) != RET_SUCCESS)
        {
            retval = RET_FAILED;
            UnLock(storing);
            goto done;
        }
        UnLock(storing);
    }

    ptr = (uintptr_t)&il->working_buffer->buf_ptr[il->working_buffer->count * INDEX_LOG_ITEM_SIZE];
    memcpy((void*)ptr, &ili->type, sizeof ili->type);
    ptr += sizeof ili->type;
    memcpy((void*)ptr, ili->index_key, sizeof(*ili->index_key));
    ptr += sizeof(*ili->index_key);
    memcpy((void*)ptr, ili->index_value, sizeof(*ili->index_value));
    ++ il->working_buffer->count;

done:
    UnLock(working);

    return retval;
}

struct index_log_item_s *
index_logger_fetch(struct index_logger_s *il)
{
    struct index_log_item_s *ili;
    ssize_t nread;
    uint32_t magic_header;
    uint32_t crc32sum, count; /* the type should be conformed to struct buf_s */

    ili = NULL;
    if (il->fetch_pos == 0)
    {
        /* first time to read index log */
        nread = read(il->fd, &magic_header, sizeof magic_header);
        if ((nread!=sizeof magic_header) || (magic_header != htonl(MAGIC_HEADER)))
            goto done;
    }
    if (il->fetch_id == -1)
    {
        /* read a new block */
        nread = read(il->fd, &crc32sum, sizeof crc32sum);
        if (nread != sizeof crc32sum)
            goto done;
        nread = read(il->fd, &count, sizeof count);
        if (nread != sizeof count)
            goto done;
        il->fetch_records_amount = count;
        il->fetch_id = 0;
        /* TODO check the crc32sum */
    }

    ili = slab_alloc(sizeof *ili);
    if (ili == NULL)
        goto done;
    ili->index_key = slab_alloc(sizeof(*ili->index_key));
    if (ili->index_key == NULL)
    {
        slab_free(ili);
        ili = NULL;
        goto done;
    }
    ili->index_value = slab_alloc(sizeof(*ili->index_value));
    if (ili->index_value == NULL)
    {
        slab_free(ili->index_key);
        slab_free(ili);
        ili = NULL;
        goto done;
    }

#define DetermineRead(actual_read, should_read) do {    \
        if (actual_read != should_read) {               \
            index_logger_free_item(ili);                \
            ili = NULL;                                 \
            goto done;                                  \
        }                                               \
    } while (0)

    nread = read(il->fd, &ili->type, sizeof ili->type);
    DetermineRead(nread, sizeof ili->type);
    nread = read(il->fd, ili->index_key, sizeof(*ili->index_key));
    DetermineRead(nread, sizeof(*ili->index_key));
    nread = read(il->fd, ili->index_value, sizeof(*ili->index_value));
    DetermineRead(nread, sizeof(*ili->index_value));

    ++ il->fetch_id;
    if (il->fetch_id == il->fetch_records_amount)
        il->fetch_id = -1;

done:
    il->fetch_pos = lseek(il->fd, 0, SEEK_CUR);
    assert(il->fetch_pos >= 0);
    return ili;
}

int
index_logger_free_item(struct index_log_item_s *ili)
{
    slab_free(ili->index_value);
    slab_free(ili->index_key);
    slab_free(ili);
    return RET_SUCCESS;
}

int
index_logger_flush(struct index_logger_s *il)
{
    Lock(working);
    if (il->working_buffer->count == 0)
    {
        UnLock(working);
        return RET_SUCCESS;
    }
    Lock(storing);
    switch_buffer(il);
    if (sync_index_log(il) != RET_SUCCESS)
    {
        UnLock(storing);
        UnLock(working);
        return RET_FAILED;
    }
    UnLock(storing);
    UnLock(working);

    return RET_SUCCESS;
}

int
index_logger_close(struct index_logger_s *il)
{
    assert((il->curr_working_buf==1) || (il->curr_working_buf==2) || (il->curr_working_buf==0));
    if (il->curr_working_buf != 0)
    {
        /* append mode */
        if (index_logger_flush(il) != RET_SUCCESS)
        {
            log_error("flush index log failed");
        }
    }

    close(il->fd);
    pthread_mutex_destroy(&il->working_lock);
    pthread_mutex_destroy(&il->storing_lock);
    slab_free(il);

    return RET_SUCCESS;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
