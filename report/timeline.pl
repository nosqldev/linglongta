#!/usr/bin/perl -w

# © Copyright 2011 jingmi. All Rights Reserved.
#
# +-----------------------------------------------------------------------+
# | draw timeline graph:                                                  |
# | X axis - time point                                                   |
# | Y axis - operation(GET/SET) amount                                    |
# +-----------------------------------------------------------------------+
# | Author: jingmi@gmail.com                                              |
# +-----------------------------------------------------------------------+
# | Created: 2011-12-27 15:54                                             |
# +-----------------------------------------------------------------------+

use strict;
use Data::Dumper;
use Getopt::Long;
use Time::Local;

our $graphic = 0;
our $min_time = undef;
our $max_time = undef;
our %timeline;
our $set_counter = 0;
our $get_counter = 0;

GetOptions('g'=>\$graphic,);

sub convert_time
{
    my $time = shift;

    my ($year, $mon, $day, $hour, $min, $sec) = $time =~ /(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})/;

    return timelocal($sec, $min, $hour, $day, $mon-1, $year-1900);
}

sub do_analyse
{
    my $time = shift;
    my $op_timeline_ref = shift;
    my $op = shift;

    unless (defined($op_timeline_ref->{$time}))
    {
        $op_timeline_ref->{$time} = {$op=>1};
    }
    else
    {
        $op_timeline_ref->{$time}{$op} ++;
    }
}

sub analyse
{
    my $line = shift;
    my ($strtime) = $line =~ /^(\[.*?\])/;

    my $time = &convert_time($strtime);
    $min_time = $time unless defined($min_time);
    $max_time = $time;

    if ($line =~ /GET/)
    {
        &do_analyse($time, \%timeline, 'get');
        $get_counter ++;
    }
    elsif ($line =~ /SET/)
    {
        &do_analyse($time, \%timeline, 'set');
        $set_counter ++;
    }
}

sub print_timeline
{
    foreach my $time (sort keys %timeline)
    {
        my $id = $time - $min_time;
        $timeline{$time}{'get'} = 0 unless defined($timeline{$time}{'get'});
        $timeline{$time}{'set'} = 0 unless defined($timeline{$time}{'set'});
        print $id, "\t", $timeline{$time}{'get'}, "\t", $timeline{$time}{'set'}, "\n";
    }
}

sub output_gnuplot_datafile
{
    open my $output_fh, ">timeline.dat";

    foreach my $time (sort keys %timeline)
    {
        my $id = $time - $min_time;
        $timeline{$time}{'get'} = 0 unless defined($timeline{$time}{'get'});
        $timeline{$time}{'set'} = 0 unless defined($timeline{$time}{'set'});
        print $output_fh $id, "\t", $timeline{$time}{'get'}, "\t", $timeline{$time}{'set'}, "\n";
    }

    close($output_fh);
    `gnuplot gnuplot_timeline.plot`;
    `mkdir -p output`;
    `mv timeline.dat output/`;
    `mv timeline.png output/`;
}

while (<>)
{
    next unless /<Elapsed: \d+\.\d+s>/;
    &analyse($_);
}

&print_timeline;
&output_gnuplot_datafile if $graphic;

__END__
# vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker:
