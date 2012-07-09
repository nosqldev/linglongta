all:
	cc -Wall -Wextra -g -c main.c -o main.o
	cc -g -c ev.c -o ev.o
	cc -Wall -Wextra -g -o main main.o ev.o -lpthread -lm
	cc -Wall -Wextra -I../include -I. -std=gnu99 -g -o bdb bdb.c ../src/bdblib.c ../src/slab.c ../src/log.c ../src/utils.c -lpthread -ldb -lm

clean:
	-rm -f main main.o ev.o bdb /tmp/test.db /tmp/__db.*
