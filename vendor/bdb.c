/* © Copyright 2012 jingmi. All Rights Reserved.
 *
 * +----------------------------------------------------------------------+
 * | berkeley db example                                                  |
 * +----------------------------------------------------------------------+
 * | Author: mi.jing@jiepang.com                                          |
 * +----------------------------------------------------------------------+
 * | Created: 2012-02-14 12:13                                            |
 * +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <time.h>

#include "bdblib.h"

static struct bdb_t *bdb_ptr;

void
interrupt(int sig)
{
    assert(sig == SIGINT);
    printf("\rcontrol c\n");
    close_bdb(bdb_ptr);
    exit(0);
}

int
main(void)
{
    char key[128];
    char value[128];
    bdb_ptr = open_transaction_bdb("/tmp", "test.db", 1024 * 1024 * 128, 0, stdout);
    assert(bdb_ptr != NULL);

    signal(SIGINT, interrupt);

    for (int i=0; i<10 * 1000 * 1000; i++)
    {
        snprintf(key, sizeof key, "%d", rand() * 10 + rand());
        snprintf(value, sizeof value, "%d", rand());
        int ret = insert_record(bdb_ptr, key, strlen(key), value, strlen(value));
        if (i % 100000 == 0)
        {
            sync_bdb(bdb_ptr);
        }
        assert(ret == 0);
        printf("%ju %d - %s\n", time(NULL), i, key);
    }

    close_bdb(bdb_ptr);

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker: */
