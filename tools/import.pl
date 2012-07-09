#!/usr/bin/perl -w

# © Copyright 2012 jingmi. All Rights Reserved.
#
# +-----------------------------------------------------------------------+
# | import local jpg directories to imgstack                              |
# | local directories structure should be:                                |
# | root_dir/                                                             |
# |         |-- 00                                                        |
# |         |    |_ 00                                                    |
# |         |    |_ 01                                                    |
# |         |    |_ ...                                                   |
# |         |    |_ FF                                                    |
# |         |                                                             |
# |         |-- 01                                                        |
# |         |    |_ 00                                                    |
# |         |    |_ 01                                                    |
# |         |    |_ ...                                                   |
# |         |    |_ FF                                                    |
# |         |                                                             |
# |         |-- 02                                                        |
# |         |                                                             |
# |         |-- ...                                                       |
# |         |                                                             |
# |         |__ FF                                                        |
# |                                                                       |
# +-----------------------------------------------------------------------+
# | Author: jingmi@gmail.com                                              |
# +-----------------------------------------------------------------------+
# | Created: 2011-12-31 17:35                                             |
# +-----------------------------------------------------------------------+

use strict;
use Data::Dumper;
use IO::Socket;
use Socket;

sub store
{
    my $key = shift;
    my $buf = shift;

    my $socket = IO::Socket::INET->new("localhost:5000");
    die "$!\n" unless $socket;
    my $len = length($buf);

    return if (length($key) > 32);
    #$key = $key . 'z' x (32 - length($key)) if (length($key) < 32);
    print $key, "\t", $len, "\n";

    my $send_buf = "set $key 0 0 $len\r\n$buf\r\n";
    syswrite($socket, $send_buf, length($send_buf));
    sysread($socket, $send_buf, 1024);
    close($socket);
}

die "arg error: import.pl PHOTO_DIR\n" unless (@ARGV == 1);

foreach (00..0xFF)
{
    my $dirname = sprintf("$ARGV[0]/%02x", $_);
    my $dir_fh;
    if (opendir($dir_fh, $dirname))
    {
        print $dirname, "\n";
    }
    else
    {
        #print "opendir [$dirname] failed: $!\n";
        next;
    }
    foreach (readdir($dir_fh))
    {
        next if (/^\./);
        my $filepath = $dirname . '/' . $_;
        opendir(my $dh, $filepath) or die "open sub dir[$filepath] failed: $!\n";
        foreach my $fn (readdir($dh))
        {
            next if ($fn =~ /^\./);
            my $filename = $filepath . '/' . $fn;
            print $filename, "\n";
            my @attr = stat($filename);
            open my $fh, $filename or die "$filename - $!\n";
            my $buf;
            read($fh, $buf, $attr[7]);
            my ($key) = $fn =~ /^(.+?).jpg/;
            &store($key, $buf);
            close($fh);
        }
        closedir($dh);
    }
    closedir($dir_fh);
}
