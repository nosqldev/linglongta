#!/usr/bin/perl -w

# © Copyright 2011 jingmi. All Rights Reserved.
#
# +-----------------------------------------------------------------------+
# | Build set and get requests for imgstack.                              |
# | It will ensure that the requests are the same if without seeds set.   |
# |                                                                       |
# | Command Usage:                                                        |
# | 1. ./simple_set_then_get.pl                                           |
# | 2. ./simple_set_then_get.pl REQUESTS_COUNT RANDOM_GENERATOR_SEED      |
# +-----------------------------------------------------------------------+
# | Author: jingmi@gmail.com                                              |
# +-----------------------------------------------------------------------+
# | Created: 2011-12-19 12:06                                             |
# +-----------------------------------------------------------------------+

use strict;
use Data::Dumper;
use IO::Socket;
use Socket;
use Term::ANSIColor;

my $count = 1;
my $srand = 1224;

if (@ARGV == 2)
{
    $count = int($ARGV[0]);
    $srand = int($ARGV[1]);
}

srand($srand); # use fixed number to ensure the same output from rand()

for (1...$count)
{
    my $key = int(rand(100000000));
    $key .= 'xxooOrz';
    $key = substr($key, 0, 7);

    my $socket = IO::Socket::INET->new("localhost:5000");
    die "$!\n" unless $socket;

    my $len = int(rand(1024 * 256));
    $len += 10; # make sure len is not 0
    my $data = 'z' x ($len-1) . 'x';
    my $send_buf = "set $key 0 0 $len\r\n$data\r\n";
    my $buffer = '';
    my $ret = syswrite($socket, $send_buf, length($send_buf));
    print color("green") . "[set key=$key, len=$len] send done $ret\n" . color("reset");
    sysread($socket, $buffer, 1024);
    close($socket);
    #print $buffer;

    $socket = IO::Socket::INET->new("localhost:5000");
    die "$!\n" unless $socket;
    $send_buf = "get $key\r\n";
    $ret = syswrite($socket, $send_buf, length($send_buf));
    print "[get key=$key] send done $ret\n";
    read($socket, $buffer, 1024);
    my ($header) = $buffer =~ /^(VALUE.*?\r\n)/;
    print $header;
    my $recv_buf = substr($buffer, length($header), length($buffer) - length($header));
    my ($data_size) = $header =~ /\s(\d+)\r\n/;
    #print "data_size = $data_size\n";
    $data_size = int($data_size);
    if ($data_size > length($recv_buf))
    {
        $ret = read($socket, $buffer, $data_size - length($recv_buf));
        $recv_buf .= $buffer;
    }
    close($socket);
    if (length($recv_buf) == length($data) + 2)
    {
        # ignore tailing "\r\n"
        if (substr($recv_buf, length($recv_buf) - 2) eq "\r\n")
        {
            $recv_buf = substr($recv_buf, 0, length($recv_buf) - 2);
        }
    }

    die "Oops!\n$data\n$recv_buf\n" if ($recv_buf ne $data);
}

__END__
# vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker:
