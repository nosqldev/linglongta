/* © Copyright 2011 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | data block store                                                     |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2011-11-04 12:51                                            |
 * +----------------------------------------------------------------------+
 */

#include "storage.h"

/* TODO
 * 1. interface - DONE
 * 2. lock - HALF DONE(not thread-safe) - DONE
 * 3. sync header - DONE
 * 4. split functions & log - DONE
 * 5. bdb transaction - DONE
 */

/* NOTE key is supposed to be 8 bytes while logging */

STATIC_MODIFIER bool inited_global_vars = false;

/** 
 * @brief Size of each block
 */
STATIC_MODIFIER size_t block_size_array[MAX_BLOCK_CNT] = {0};

/** 
 * @brief Array of block files' info
 */
STATIC_MODIFIER struct block_file_t block_filehandler_array[MAX_BLOCK_CNT];

/** 
 * @brief Count of block types, computed in init_storage()
 */
STATIC_MODIFIER size_t block_type_count;

/** 
 * @brief Smallest block size
 */
#define BEGINNING_SIZE (128)

/** 
 * @brief Largest block size: 4M
 */
#define LARGEST_SIZE (1024 * 1024 * 4)

#define EX_LOCK do { rwlock_wrlock(&(fh->lock)); } while (0)
#define EX_UNLOCK do { rwlock_unwrlock(&(fh->lock)); } while (0)
#define SH_LOCK do { rwlock_rdlock(&(fh->lock)); } while (0)
#define SH_UNLOCK do { rwlock_unrdlock(&(fh->lock)); } while (0)

#define EX_LOCK_IDX(_bsp) do { rwlock_wrlock(_bsp->index_lock); } while (0)
#define EX_UNLOCK_IDX(_bsp) do { rwlock_unwrlock(_bsp->index_lock); } while (0)
#define SH_LOCK_IDX(_bsp) do { rwlock_rdlock(_bsp->index_lock); } while (0)
#define SH_UNLOCK_IDX(_bsp) do { rwlock_unrdlock(_bsp->index_lock); } while (0)

STATIC_MODIFIER int do_delete_block(struct bucket_storage_t *bsp, struct index_value_t *iv, void *key, size_t key_size);

STATIC_MODIFIER int
read_data_file(size_t block_id, void *buffer, size_t data_size, off_t offset)
{
    int ret;
    struct block_file_t *fh;

    assert(buffer != NULL);
    assert(block_id < block_type_count);

    fh = &block_filehandler_array[block_id];
    ret = 0;

    if (pread(fh->fd, buffer, data_size, offset) != (ssize_t)data_size)
    {
        ret = -1;
        log_warn("cannot read data from file: %zu.dat: %s", block_size_array[block_id], strerror(errno));
        goto err;
    }

err:
    return ret;
}

/* TODO add an argu to specify whether flush data immediately */
STATIC_MODIFIER int
sync_data_file(size_t block_id, void *data, size_t data_size, off_t offset, bool sync_flag)
{
    int ret;
    struct block_file_t *fh;
    ssize_t nwritten;

    assert(data != NULL);
    assert(block_id < block_type_count);

    fh = &block_filehandler_array[block_id];
    ret = 0;

    if ((nwritten = pwrite(fh->fd, data, data_size, offset)) != (ssize_t)data_size)
    {
        ret = -1;
        log_warn("cannot write data to file: %zu.dat, data_size = %zu, nwritten = %zd: %s",
                 block_size_array[block_id], data_size, nwritten, strerror(errno));
        goto err;
    }
    if (sync_flag)
        fdatasync(fh->fd);

err:
    return ret;
}

STATIC_MODIFIER int
sync_data_file_header(size_t block_id, bool sync_flag __attribute__((unused)))
{
    struct block_file_t *fh;

    fh = &block_filehandler_array[block_id];
    sync_data_file(block_id,
                   &fh->block_size,
                   __offset_of(block_file_t, free_slots_offset) - __offset_of(block_file_t, block_size),
                   0, sync_flag);

    return 0;
}

STATIC_MODIFIER int
load_data_file(const char *filename, size_t block_id)
{
    int fd;
    int ret;
    struct block_file_t *fh;
    size_t header_size;

    ret = 0;
    header_size = sizeof(struct block_file_t) - __offset_of(struct block_file_t, block_size);
    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        ret = -1;
        log_error("cannot open data file - %s: %s", filename, strerror(errno));
        goto err;
    }
    fh = &block_filehandler_array[block_id];
    fh->fd = fd;

    ret = read_data_file(block_id, (void*)&(fh->block_size), header_size, 0);
    if (ret != 0)
    {
        goto err;
    }
    log_notice("load data file successfully: %s", filename);

err:
    return ret;
}

STATIC_MODIFIER int
create_data_file(const char *filename, size_t block_id)
{
    int fd;
    int ret;
    struct block_file_t *fh;
    void *buffer;
    size_t bufsize;
    ssize_t nwrite;

    ret = 0;
    fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
        ret = -1;
        log_error("cannot open data file - %s: %s", filename, strerror(errno));
        goto err;
    }
    fh = &block_filehandler_array[block_id];
    fh->fd = fd;
    rwlock_build(&(fh->lock));
    fh->block_size = block_size_array[block_id];
    fh->block_cnt = 0;
    fh->free_slots_cnt = 0;

    buffer = (void*)fh + __offset_of(block_file_t, block_size);
    bufsize = sizeof(struct block_file_t) - __offset_of(block_file_t, block_size);
    nwrite = write(fh->fd, buffer, bufsize);
    if (nwrite != (ssize_t)bufsize)
    {
        ret = -1;
        log_error("write data file header failed - %s: %s", filename, strerror(errno));
        goto err;
    }
    fdatasync(fh->fd);
    log_notice("create data file successfully: %s", filename);

err:
    return ret;
}

STATIC_MODIFIER int
find_block_file(size_t block_size)
{
    for (size_t i=0; i<block_type_count; i++)
    {
        if (block_size == block_size_array[i])
            return i;
    }

    log_warn("invalid block_size: %zu", block_size);
    return -1;
}

STATIC_MODIFIER int
append_block(struct bucket_storage_t *bsp, size_t block_id, void *key, size_t key_size, void *data, size_t data_size)
{
    int ret;
    struct block_file_t *fh;
    off_t offset;
    union index_key_t index_key;
    struct index_value_t index_value;
    uint32_t crc32_value;
    struct index_log_item_s ili;

    assert(bsp != NULL);
    ret = 0;
    if (block_id >= block_type_count)
    {
        log_warn("invalid block_id: %zu", block_id);
        ret = -1;
        return ret;
    }

    fh = &(block_filehandler_array[block_id]);
    assert(fh->fd > 0);
    assert(data_size <= block_size_array[block_id]);
    crc32_value = crc32_calc(data, data_size);
    bzero(index_key.key, sizeof index_key.key);
    key_size = (key_size >= KEY_LEN ? KEY_LEN : key_size);
    memcpy(&index_key.key, key, key_size);

    EX_LOCK;
    /* MUST sync data file before index file */
    if (fh->free_slots_cnt != 0)
    {
        /* use free slots */
        offset = fh->free_slots_offset[ fh->free_slots_cnt-1 ];
        fh->free_slots_offset[ fh->free_slots_cnt-1 ] = 0;
        -- fh->free_slots_cnt;
        ++ fh->block_cnt;
        sync_data_file_header(block_id, false);
        sync_data_file(block_id, data, data_size, offset, false);
        log_notice("write free slot %zu.dat, free_slot = %zu, key = 0x%lX, data_size = %zu, offset = %zu, data crc32 = %u",
                block_size_array[block_id], fh->free_slots_cnt,
                index_key.key_num, data_size, offset, crc32_value);
    }
    else
    {
        ++ fh->block_cnt;
        sync_data_file_header(block_id, false);
        offset = DATA_SECTION_POS + fh->block_size * (fh->block_cnt - 1);
        sync_data_file(block_id, data, data_size, offset, false);
    }
    EX_UNLOCK;

    index_value.crc32     = crc32_value;
    index_value.block_id  = (uint32_t)block_id;
    index_value.offset    = offset;
    index_value.data_size = data_size;
    ili.type = index_append;
    ili.index_key = &index_key;
    ili.index_value = &index_value;
    index_logger_append(bsp->index_logger, &ili);

    if (insert_record(bsp->indexdb, &index_key, sizeof index_key, &index_value, sizeof index_value) != 0)
    {
        /* no need to log here since we log mesg in insert_record() */
        goto err;
    }

err:

    return ret;
}

STATIC_MODIFIER int
modify_block(struct bucket_storage_t *bsp, struct index_value_t *iv, void *key, size_t key_size, void *data, size_t data_size)
{
    int ret;
    struct block_file_t *fh;
    off_t offset;
    uint32_t crc32_value;
    int retno;
    size_t block_id;
    union index_key_t index_key;
    struct index_log_item_s ili;

    assert(bsp != NULL);
    block_id = iv->block_id;
    ret = 0;
    if (block_id >= block_type_count)
    {
        log_warn("invalid block_id: %zu", block_id);
        ret = -1;
        goto outlet;
    }

    fh = &(block_filehandler_array[block_id]);
    assert(fh->fd > 0);
    assert(data_size <= block_size_array[block_id]);
    crc32_value = crc32_calc(data, data_size);
    key_size = (key_size >= KEY_LEN ? KEY_LEN : key_size);

    offset = iv->offset;

    EX_LOCK;
    retno = sync_data_file(block_id, data, data_size, offset, false);
    assert(retno == 0);
    EX_UNLOCK;

    iv->crc32 = crc32_value;
    iv->data_size = data_size;

    bzero(index_key.key, sizeof index_key.key);
    key_size = (key_size >= KEY_LEN ? KEY_LEN : key_size);
    memcpy(&index_key.key, key, key_size);
    ili.type = index_modify;
    ili.index_key = &index_key;
    ili.index_value = iv;
    index_logger_append(bsp->index_logger, &ili);

    retno = insert_record(bsp->indexdb, key, key_size, iv, sizeof(*iv));
    if (retno != 0)
    {
        ret = -1;
        log_warn("modify index failed");
        goto outlet;
    }

outlet:
    return ret;
}

STATIC_MODIFIER int
calc_block_size(size_t data_size)
{
    int size;

    size = BEGINNING_SIZE;
    for (int i=0; i<MAX_BLOCK_CNT; i++)
    {
        if (size >= (int)data_size)
            return size;
        size *= 2;
    }

    return -1;
}

int
update_block(struct bucket_storage_t *bsp, void *key, size_t key_size, void *data, size_t data_size)
{
    int ret;
    int retno;
    DBT dbt;
    DBT *dbt_ptr;
    size_t value_len;
    struct index_value_t *iv;
    ssize_t new_block_id; /* use ssize_t to express negative */
    char fixed_key_buffer[KEY_LEN] = {0};
    size_t fixed_key_size;
    char log_buf[1024] = {0};
    union index_key_t index_key;
    pre_timer();

    assert(bsp != NULL);
    launch_timer();
    ret = 0;
    value_len = sizeof(struct index_value_t);
    dbt_ptr = &dbt;
    key_size = (key_size >= KEY_LEN ? KEY_LEN : key_size);
    memcpy(fixed_key_buffer, key, key_size);
    fixed_key_size = KEY_LEN;
    bzero(index_key.key, sizeof index_key.key);
    memcpy(&index_key.key, fixed_key_buffer, key_size);

    EX_LOCK_IDX(bsp);
    retno = find_record(bsp->indexdb, &dbt_ptr, fixed_key_buffer, fixed_key_size, &value_len);
    if (retno == 0)
    {
        /* found exist record */
        iv = dbt.data;
        if (data_size <= block_size_array[ iv->block_id ])
        {
            /* stay in current block file */
            retno = modify_block(bsp, iv, fixed_key_buffer, fixed_key_size, data, data_size);
            if (retno != 0)
            {
                ret = -1;
                strcpy(log_buf, "modify block failed");
                goto outlet;
            }
            else
            {
                snprintf(log_buf, sizeof log_buf, "Modify %zu.dat successfully, key = 0x%lX, data_size = %zu, offset = %zu",
                         block_size_array[iv->block_id], index_key.key_num, data_size, iv->offset);
                goto outlet;
            }
        }
        else
        {
            /* upgrade to larger size block file */
            /* TODO put append_block() executed before delete(but not delete_block()) */
            retno = do_delete_block(bsp, iv, fixed_key_buffer, fixed_key_size);
            if (retno != 0)
            {
                ret = -2;
                strcpy(log_buf, "delete failed while updating block");
                goto outlet;
            }
            new_block_id = find_block_file(calc_block_size(data_size));
            retno = append_block(bsp, new_block_id, fixed_key_buffer, fixed_key_size, data, data_size);
            if (retno != 0)
            {
                ret = -3;
                log_error("Append block failed while updating block. Might lost record!!!");
                goto outlet;
            }
            else
            {
                snprintf(log_buf, sizeof log_buf, "Move records to %zu.dat successfully, key = 0x%lX, data_size = %zu",
                         block_size_array[iv->block_id], index_key.key_num, data_size);
                goto outlet;
            }
        }
    }
    else if (retno == DB_NOTFOUND)
    {
        /* record is not exist in databases, begin insert */
        /* TODO add test suites for this case */
        new_block_id = find_block_file(calc_block_size(data_size));
        if (new_block_id < 0)
        {
            snprintf(log_buf, sizeof log_buf, "invalid data size: %zu", data_size);
            goto outlet;
        }
        retno = append_block(bsp, new_block_id, fixed_key_buffer, fixed_key_size, data, data_size);
        if (retno != 0)
        {
            ret = -4;
            strcpy(log_buf, "Append block failed while add new block");
            goto outlet;
        }
        else
        {
            snprintf(log_buf, sizeof log_buf, "Append %zu.dat successfully, key = 0x%lX, data_size = %zu",
                     block_size_array[new_block_id], index_key.key_num, data_size);
        }
    }
    else
    {
        snprintf(log_buf, sizeof log_buf, "update failed: %d", retno);
        ret = -1;
        goto outlet;
    }

outlet:
    EX_UNLOCK_IDX(bsp);
    if (dbt.data != NULL)
        slab_free(dbt.data);

    stop_timer();
    log_notice("%s, took %ld.%06ld sec", log_buf, __used.tv_sec, __used.tv_usec);

    return ret;
}

/* TODO split find_block() to 2 functions: find_block() & do_find_block()
 * log info in find_block()
 */
int
find_block(struct bucket_storage_t *bsp __attribute__((unused)), void *key, size_t key_size, void **value_ptr, size_t *value_len)
{
    DBT dbt;
    DBT *dbt_ptr;
    int ret, retno;
    size_t vlen;
    char fixed_key_buffer[KEY_LEN] = {0};
    size_t fixed_key_size;
    ssize_t nread;
    struct index_value_t *iv;
    struct block_file_t *fh;
    pre_timer();

    assert(bsp != NULL);
    /* TODO make value_ptr to use caller's memory if value_ptr is not NULL */
    launch_timer();
    dbt_ptr = &dbt;
    *value_ptr = NULL;
    *value_len = 0;
    vlen = sizeof(struct index_value_t);
    ret = 0;
    key_size = (key_size >= KEY_LEN ? KEY_LEN : key_size);
    memcpy(fixed_key_buffer, key, key_size);
    fixed_key_size = KEY_LEN;
    iv = NULL;

    SH_LOCK_IDX(bsp);
    retno = find_record(bsp->indexdb, &dbt_ptr, fixed_key_buffer, fixed_key_size, &vlen);
    if (retno == 0)
    {
        iv = dbt.data;
        fh = &block_filehandler_array[ iv->block_id ];
        *value_len = iv->data_size;
        if ((*value_ptr = slab_alloc(iv->data_size)) == NULL)
        {
            log_error("no memory");
            ret = -1;
            goto err;
        }

        SH_LOCK;
        SH_UNLOCK_IDX(bsp); /* unlock here in order to improve concurrent */
        nread = pread(fh->fd, *value_ptr, iv->data_size, iv->offset);
        SH_UNLOCK;
        if (nread != (ssize_t)iv->data_size)
        {
            ret = -1;
            log_warn("cannot read data, nread = %zd, to_read = %zu, offset = %zu: %d - %s",
                     nread, iv->data_size, iv->offset, errno, strerror(errno));
            goto err;
        }
    }
    else
    {
        SH_UNLOCK_IDX(bsp);
        /* retno == DB_NOTFOUND or NOT */
        /* TODO log if error occur*/
        ret = retno;
        goto err;
    }

err:
    stop_timer();
    if (ret == 0)
    {
        /* TODO print whole key content */
        uint64_t *avoid_strict_aliasing_ptr = (uint64_t*)fixed_key_buffer;
        log_notice("Search successfully, key = 0x%lX, size = %zu, took %ld.%06ld sec",
                   *avoid_strict_aliasing_ptr, iv->data_size, __used.tv_sec, __used.tv_usec);
    }
    else if (ret == DB_NOTFOUND)
    {
        uint64_t *avoid_strict_aliasing_ptr = (uint64_t*)fixed_key_buffer;
        log_notice("found nothing, key = 0x%lX, took %ld.%06ld sec", *avoid_strict_aliasing_ptr, __used.tv_sec, __used.tv_usec);
    }
    else
    {
        uint64_t *avoid_strict_aliasing_ptr = (uint64_t*)fixed_key_buffer;
        log_warn("search failed, key = 0x%lX, took %ld.%06ld sec", *avoid_strict_aliasing_ptr, __used.tv_sec, __used.tv_usec);
    }

    slab_free(iv);
    return ret;
}

STATIC_MODIFIER int
do_delete_block(struct bucket_storage_t *bsp, struct index_value_t *iv, void *key, size_t key_size)
{
    int ret;
    int retval;
    struct block_file_t *fh;
    struct index_log_item_s ili;
    union index_key_t index_key;
    size_t slot_pos;

    retval = 0;
    fh = &block_filehandler_array[ iv->block_id ];

    bzero(index_key.key, sizeof index_key.key);
    memcpy(&index_key.key, key, key_size);
    ili.type = index_del;
    ili.index_key = &index_key;
    ili.index_value = iv;
    index_logger_append(bsp->index_logger, &ili);

    if ((ret = del_record(bsp->indexdb, key, key_size)) != 0)
    {
        log_warn("delete index failed: %d - %s", ret, db_strerror(ret));
        retval = -1;
        goto done;
    }
    EX_LOCK;
    if (fh->free_slots_cnt >= MAX_FREE_SLOTS)
    {
        EX_UNLOCK;
        log_warn("[slot leak] free slot is not enough in %zu.dat, data_size = %zu, offset = %zu",
                 block_size_array[iv->block_id], iv->data_size, iv->offset);
        goto done;
    }
    -- fh->block_cnt;
    slot_pos = fh->free_slots_cnt;
    fh->free_slots_offset[ slot_pos ] = iv->offset;
    ++ fh->free_slots_cnt;
    EX_UNLOCK;
    sync_data_file_header(iv->block_id, false);
    sync_data_file(iv->block_id, &(fh->free_slots_offset[ slot_pos ]), sizeof(fh->free_slots_offset[0]),
            SLOTS_SECTION_POS + sizeof(fh->free_slots_offset[0]) * slot_pos, false);

done:
    return retval;
}

int
delete_block(struct bucket_storage_t *bsp, void *key, size_t key_size)
{
    struct index_value_t *iv;
    struct block_file_t *fh;
    DBT dbt;
    DBT *dbt_ptr;
    int ret, retno;
    size_t vlen;
    char fixed_key_buffer[KEY_LEN] = {0};
    size_t fixed_key_size;
    pre_timer();

    assert(bsp != NULL);
    launch_timer();
    dbt_ptr = &dbt;
    vlen = sizeof(struct index_value_t);
    ret = 0;
    key_size = (key_size >= KEY_LEN ? KEY_LEN : key_size);
    memcpy(fixed_key_buffer, key, key_size);
    fixed_key_size = KEY_LEN;
    iv = NULL; /* to avoid uninitialized warning */
    fh = NULL; /* to avoid uninitialized warning */

    EX_LOCK_IDX(bsp);
    retno = find_record(bsp->indexdb, &dbt_ptr, fixed_key_buffer, fixed_key_size, &vlen);
    if (retno == 0)
    {
        iv = dbt.data;
        fh = &block_filehandler_array[ iv->block_id ];

        retno = do_delete_block(bsp, iv, fixed_key_buffer, fixed_key_size);
        EX_UNLOCK_IDX(bsp);
        if (retno != 0)
        {
            ret = -1;
            goto done;
        }
    }
    else if (retno == DB_NOTFOUND)
    {
        EX_UNLOCK_IDX(bsp);
        ret = DB_NOTFOUND;
        goto done;
    }
    else
    {
        EX_UNLOCK_IDX(bsp);
        ret = -1;
        goto done;
    }

done:
    if (dbt.data != NULL)
        slab_free(dbt.data);

    stop_timer();
    if (ret == 0)
    {
        log_notice("delete record successfully: file = %zu.dat, free_slot = %zu, data_size = %zu, offset = %zu, data crc32 = %u, took %ld.%06ld sec",
                fh->block_size, fh->free_slots_cnt, iv->data_size, iv->offset, iv->crc32, __used.tv_sec, __used.tv_usec);
    }
    else if (ret != DB_NOTFOUND)
    {
        log_warn("delete index failed: %d - %s, took %ld.%06ld sec", retno, db_strerror(retno), __used.tv_sec, __used.tv_usec);
    }

    return ret;
}

STATIC_MODIFIER int
open_index_file(const char *dir_name, const char *file_name, struct bucket_storage_t *bsp)
{
    int ret;
    struct bdb_t *bdbp;
    FILE *log_fp;
    char indexlog_filename[FILENAME_MAX];

    assert(bsp != NULL);

    ret = 0;
    log_fp = log_file();
    bdbp = open_transaction_bdb(dir_name, file_name, BDB_CACHE_SIZE, 0, NULL);
    if (bdbp == NULL)
    {
        log_error("cannot open index file");
        ret = -1;
        goto err;
    }

    if (bdbp->error_no != 0)
    {
        log_error("cannot open index file: %s", db_strerror(bdbp->error_no));
        ret = -1;
        goto err;
    }
    log_notice("index file open successfully");

    bsp->indexdb = bdbp;
    bsp->index_lock = rwlock_new();

    strcpy(indexlog_filename, file_name);
    strcat(indexlog_filename, ".journal");
    bsp->index_logger = index_logger_create(dir_name, indexlog_filename, true);
    if (bsp->index_logger == NULL)
    {
        log_error("cannot open index logger: %s/%s", dir_name, indexlog_filename);
        goto err;
    }

err:
    return ret;
}

STATIC_MODIFIER int
open_data_files(const char *dir_name)
{
    int ret;
    int retno;
    char filename[PATH_MAX];

    ret = 0;
    for (size_t i=0; i<block_type_count; i++)
    {
        snprintf(filename, sizeof filename, "%s/%zu.dat", dir_name, block_size_array[i]);
        if ((retno=access(filename, R_OK | W_OK)) == 0)
        {
            if (load_data_file(filename, i) != 0)
            {
                ret = -1;
                goto err;
            }
        }
        else
        {
            if (create_data_file(filename, i) != 0)
            {
                ret = -1;
                goto err;
            }
        }
    }

err:
    return ret;
}

STATIC_MODIFIER int
init_storage(const char *dir_path __attribute__((unused)), size_t largest_size)
{
    int ret;
    size_t curr_size;

    ret = 0;
    if (largest_size < BEGINNING_SIZE)
        largest_size = LARGEST_SIZE;
    block_type_count = 0;
    memset(block_filehandler_array, 0, sizeof(block_filehandler_array));
    memset(block_size_array, 0, sizeof(block_size_array));

    curr_size = BEGINNING_SIZE;
    for (int i = 0; curr_size <= largest_size; i++)
    {
        if (i >= MAX_BLOCK_CNT)
        {
            log_error("init storage failed: largest block size exceeded the max limitation");
            ret = -1;
            break;
        }
        block_size_array[i] = curr_size;
        curr_size <<= 1;

        block_type_count = i + 1;
    }
    log_notice("init storage successfully");

    return ret;
}

int
open_storage(const char *dir_path, size_t largest_size, struct bucket_storage_t **bsp)
{
    int retno;
    int ret;
    struct stat sb;
    char *dir_name_copy, *file_name_copy;
    char *dir_name, *file_name;

    ret = 0;
    assert(bsp != NULL);
    *bsp = NULL;
    dir_name_copy = NULL;
    file_name_copy = NULL;

    *bsp = slab_calloc(sizeof(struct bucket_storage_t));
    if (*bsp == NULL)
    {
        log_error("memory is not enough");
        ret = -4;
        goto err;
    }

    dir_name_copy  = strdup(dir_path);
    file_name_copy = strdup(dir_path);
    dir_name  = dirname(dir_name_copy);
    file_name = basename(file_name_copy);
    if ((stat(dir_name, &sb) != 0) && (errno == ENOENT))
    {
        retno = mkdir(dir_name, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXGRP);
        if (retno != 0)
        {
            log_error("cannot mkdir: %s, error: %d - %s", dir_name, errno, strerror(errno));
            ret = -5;
            goto err;
        }
    }
    else if (stat(dir_name, &sb) != 0)
    {
        log_error("cannot stat dir: %s, error: %d - %s", dir_name, errno, strerror(errno));
        ret = -5;
        goto err;
    }

    if (!inited_global_vars)
    {
        inited_global_vars = true;

        retno = init_storage(dir_name, largest_size);
        if (retno != 0)
        {
            ret = -1;
            goto err;
        }
        retno = open_data_files(dir_name);
        if (retno != 0)
        {
            ret = -2;
            goto err;
        }
    }

    retno = open_index_file(dir_name, file_name, *bsp);
    if (retno != 0)
    {
        ret = -3;
        goto err;
    }

err:
    if (ret == 0)
        log_notice("open storage successfully");
    else
    {
        if (*bsp != NULL)
            slab_free(*bsp);
        log_error("open storage failed: %d", ret);
    }

    if (dir_name_copy)
        free(dir_name_copy);
    if (file_name_copy)
        free(file_name_copy);

    return ret;
}

/** 
 * @brief Close all file handlers to data file, sync header info to disk before
 * exists, then set the fd to -1
 * 
 * @return 0 if success, otherwise -1
 */
int
close_storage(struct bucket_storage_t *bsp)
{
    struct block_file_t *fh;
    int ret;
    int closed_file_cnt;
    int retno;

    assert(bsp != NULL);
    ret = 0;
    closed_file_cnt = 0;

    if (inited_global_vars)
    {
        for (size_t i=0; i<block_type_count; i++)
        {
            fh = &block_filehandler_array[i];
            if (fh->fd > 0)
            {
                sync_data_file_header(i, true);
                close(fh->fd);
                fh->fd = -1;
                closed_file_cnt ++;
                log_notice("closed data file: %zu.dat", block_size_array[i]);
            }
            else
            {
                /* should never reach here */
                log_error("unreachable code section, block_id = %zu", i);
                ret = -1;
                goto err;
            }
        }
        block_type_count -= closed_file_cnt;
        inited_global_vars = false;
    }

    if (bsp->indexdb != NULL)
    {
        retno = close_bdb(bsp->indexdb);
        if (retno != 0)
        {
            ret = -1;
            log_error("close_bdb failed: (%d) %s", retno, db_strerror(retno));
            goto err;
        }
        bsp->indexdb = NULL;
    }

    if (bsp->index_logger != NULL)
    {
        retno = index_logger_close(bsp->index_logger);
        if (retno != 0)
        {
            ret = -1;
            log_error("close index logger failed: %d", retno);
            goto err;
        }
        bsp->index_logger = NULL;
    }

    slab_free(bsp);

err:
    return ret;
}

int
sync_storage(struct bucket_storage_t *bsp, const char *object)
{
    if (strcasecmp(object, "index") == 0)
    {
        sync_bdb(bsp->indexdb);                /* flush bdb */
        index_logger_flush(bsp->index_logger); /* flush index log */
    }
    else if (strcasecmp(object, "all") == 0)
    {
        sync_bdb(bsp->indexdb);                /* flush bdb */
        index_logger_flush(bsp->index_logger); /* flush index log */
        /* flush data file */
        for (size_t i=0; i<block_type_count; i++)
        {
            sync_data_file_header(i, true);
        }
    }
    else
    {
        /* should never reach here */
        abort();
    }

    return 0;
}

/* {{{ for testing only */

#ifdef TESTMODE

STATIC_MODIFIER void
testing_fetch_internal_data(size_t **block_size_array_ptr, size_t *block_type_count_ptr,
        struct block_file_t **block_filehandler_array_ptr)
{
    *block_size_array_ptr = block_size_array;
    *block_type_count_ptr = block_type_count;
    *block_filehandler_array_ptr = block_filehandler_array;
}

STATIC_MODIFIER void
increase_block_type_count(void)
{
    block_type_count ++;
}

STATIC_MODIFIER size_t *
get_block_type_count()
{
    return &block_type_count;
}

#endif

/* }}} */

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
