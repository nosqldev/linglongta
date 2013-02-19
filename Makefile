CC=cc
OPTIMIZE=-g -O2 -march=native
CFLAGS=$(OPTIMIZE) -Wall -Wextra -std=gnu99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_REENTRANT $(INCLUDE)
INCLUDE=-Iinclude/ -Ivendor/ -Iutils/include
LDFLAGS=-Lutils/lib
LIBS=-lpthread -ldb -lm
SRC_PATH=src
VENDOR_PATH=vendor/

SOURCES=$(SRC_PATH)/assign_thread.c $(SRC_PATH)/bdblib.c $(SRC_PATH)/crc32.c $(SRC_PATH)/global.c \
        $(SRC_PATH)/indexlog.c $(SRC_PATH)/iolayer.c $(SRC_PATH)/log.c $(SRC_PATH)/memcached_protocol.c \
        $(SRC_PATH)/query_thread.c $(SRC_PATH)/rwlock.c $(SRC_PATH)/slab.c $(SRC_PATH)/storage.c \
        $(SRC_PATH)/utils.c $(SRC_PATH)/mhash.c $(SRC_PATH)/bobhash.c
OBJS=$(patsubst %.c,%.o,$(SOURCES))

all: datanode replay_journal

datanode: $(OBJS) $(SRC_PATH)/datanode.o $(VENDOR_PATH)/ev.o
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o datanode $(SRC_PATH)/datanode.o $(OBJS) $(VENDOR_PATH)/ev.o $(LIBS)

master_mirror: $(OBJS) $(SRC_PATH)/master_mirror.o $(VENDOR_PATH)/ev.o $(VENDOR_PATH)/acoro.o
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o master_mirror $(SRC_PATH)/master_mirror.o $(OBJS) $(VENDOR_PATH)/ev.o $(VENDOR_PATH)/acoro.o $(LIBS)

replay_journal: $(OBJS) $(VENDOR_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o tools/replay_journal tools/replay_journal.c src/indexlog.o src/log.o src/slab.o src/storage.o src/rwlock.o src/crc32.o src/bdblib.o $(LIBS)

$(VENDOR_PATH)/ev.o: vendor/*.c vendor/*.h
	$(CC) -c -g -std=gnu99 vendor/ev.c -o $(VENDOR_PATH)/ev.o

$(VENDOR_PATH)/acoro.o: vendor/*.c vendor/*.h
	$(CC) $(CFLAGS) -c vendor/acoro.c -o $(VENDOR_PATH)/acoro.o

.PHONY: clean
clean:
	- rm -f src/*.o
	- rm -f $(VENDOR_PATH)/*.o
	- rm -f datanode master_mirror
	- rm -f tools/replay_journal
