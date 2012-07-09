#!/bin/sh

# stat command: /bin/grep SET datanode.log | awk '{print $2}' | uniq -c

date >> log.simple_set_then_get

if [ -z $1 ]
then
    seq 16 | ../../utils/bin/parallel -u --progress --load 4 -v -j 8 ./simple_set_then_get.pl 10000
else
    seq 16 | ../../utils/bin/parallel -u --progress --load 4 -v -j 8 ./simple_set_then_get.pl $1
fi

date >> log.simple_set_then_get
