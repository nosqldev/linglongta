#!/bin/bash

cputs()
{
    printf "\033[01;36;40m$1\033[00m\n"
}

gputs()
{
    printf "\033[01;32m$1\033[00m\n"
}

yputs()
{
    printf "\033[01;33m$1\033[00m\n"
}

rputs()
{
    printf "\033[01;31;40m$1\033[00m\n"
}

puts_title()
{
    printf "\033[07;34;43m$1\033[00m\n"
}

warn_puts()
{
    printf "\033[01;33;41m$1\033[00m\n"
}

compile_obj()
{
    src=$1

    echo -n "compiling "
    cputs "$src.c"
    cc -Wall -Wextra -c -g -std=gnu99 -D_GNU_SOURCE -DTESTMODE -I../include -I../vendor ../src/${src}.c -o ${src}.o
}

CNT=0

run_ts()
{
    rm -rf data
    if [ -x $1 ]
    then
        CNT=$[ $CNT + 1 ]
        printf "\n                "
        puts_title "[$CNT] $1"
        $1 | sed -n '7,$p' | perl -lne 's/asserts(\s+\d+)(\s+\d+)(\s+\d+)(\s+\d+)(\s+.*)$/asserts\033[01;32m\1\033[00m\033[01;32m\2\033[00m\033[01;34m\3\033[00m\033[01;31m\4\033[00m\033[01;32m\5\033[00m/g; s!^(stat:.+)!\033[04;36;40m\1\033[00m!; print;'
    else
        warn_puts "\n ####### $1 doesn't exists ########\n"
        exit
    fi

    if [ $2 ]
    then
        rm -f $1
    fi
}

cleanup_env()
{
    rm -f core*
    rm -f *.o
    rm -f datanode.log
}

gputs "remove temp files"
cleanup_env

gputs "compile necessary obj files\n"
cc -c -g ../vendor/ev.c -o ./ev.o
compile_obj "crc32"
compile_obj "rwlock"
compile_obj "bdblib"
compile_obj "storage"
compile_obj "global"
compile_obj "slab"
compile_obj "utils"
compile_obj "rwlock"
compile_obj "log"
compile_obj "assign_thread"
compile_obj "memcached_protocol"
compile_obj "iolayer"
compile_obj "query_thread"
compile_obj "indexlog"

gputs "\ncompile test suites"
printf "\033[01;31;40m" # red
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -lm -I../include -I../vendor test_global.c global.o slab.o log.o ev.o storage.o indexlog.o bdblib.o rwlock.o crc32.o -o test_global
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -I../include -I../vendor test_storage.c indexlog.o storage.o bdblib.o slab.o log.o rwlock.o crc32.o -o test_storage
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -lm -I../include -I../vendor test_utils.c slab.o log.o utils.o -o test_utils
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -lm -I../include -I../vendor test_memcached_protocol.c global.o slab.o log.o ev.o utils.o memcached_protocol.o storage.o indexlog.o bdblib.o rwlock.o crc32.o -o test_memcached_protocol
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -lm -I../include -I../vendor test_assign_thread.c global.o slab.o log.o ev.o assign_thread.o utils.o iolayer.o memcached_protocol.o storage.o indexlog.o bdblib.o rwlock.o crc32.o -o test_assign_thread
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -lm -I../include -I../vendor test_query_thread.c query_thread.o global.o slab.o log.o ev.o assign_thread.o utils.o iolayer.o memcached_protocol.o storage.o indexlog.o bdblib.o rwlock.o crc32.o -o test_query_thread
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -lpthread -lcunit -lm -I../include -I../vendor test_bsdqueue.c slab.o ev.o -o test_bsdqueue
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -lm -I../include -I../vendor test_iolayer.c iolayer.o slab.o ev.o global.o utils.o log.o storage.o indexlog.o bdblib.o rwlock.o crc32.o -o test_iolayer
cc -Wall -Wextra -g -std=gnu99 -D_GNU_SOURCE -ldb -lpthread -lcunit -lm -I../include -I../vendor test_indexlog.c indexlog.o slab.o ev.o global.o utils.o log.o storage.o bdblib.o rwlock.o crc32.o -o test_indexlog
printf "\033[00m"

run_ts "./test_global" del
run_ts "./test_storage" del
run_ts "./test_utils" del
run_ts "./test_memcached_protocol" del
run_ts "./test_assign_thread" del
run_ts "./test_query_thread" del
run_ts "./test_bsdqueue" del
run_ts "./test_iolayer" del
run_ts "./test_indexlog" del

gputs "finished"
rm -rf data
cleanup_env
