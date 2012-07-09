#!/usr/bin/perl -w

# © Copyright 2011 jingmi. All Rights Reserved.
#
# +-----------------------------------------------------------------------+
# | analyse distribution of time cost from log                            |
# +-----------------------------------------------------------------------+
# | Author: jingmi@gmail.com                                              |
# +-----------------------------------------------------------------------+
# | Created: 2011-12-27 11:36                                             |
# +-----------------------------------------------------------------------+

use strict;
use Data::Dumper;
use Getopt::Long;

our %distribution_map;
our $total_cnt = 0;
our $graphic = 0;

GetOptions('g'=>\$graphic,);

sub do_analyse
{
    my $time_cost = shift;
    my $up_limit = shift;
    my $lo_limit = shift;
    my $key = sprintf("%.3f", $lo_limit) . "~" . sprintf("%.3f", $up_limit);

    if (($time_cost < $up_limit) and ($time_cost >= $lo_limit))
    {
        unless (defined($distribution_map{$key}))
        {
            $distribution_map{$key} = {'cnt'=>1, 'ratio'=>0};
        }
        else
        {
            $distribution_map{$key}{'cnt'} ++;
        }
        return 1;
    }

    return 0;
}

sub analyse
{
    my $time_cost = shift;

    $total_cnt ++;
    while (1)
    {
        last if &do_analyse($1, 0.001, 0.000);
        last if &do_analyse($1, 0.005, 0.001);
        last if &do_analyse($1, 0.010, 0.005);
        last if &do_analyse($1, 0.020, 0.010);
        last if &do_analyse($1, 0.050, 0.020);
        last if &do_analyse($1, 0.100, 0.050);
        last if &do_analyse($1, 0.200, 0.100);
        last if &do_analyse($1, 0.500, 0.200);
        last if &do_analyse($1, 1.000, 0.500);
        last if &do_analyse($1, 2.000, 1.000);
        last if &do_analyse($1, 5.000, 2.000);
        last if &do_analyse($1, 1000.000, 5.000);
    }
}

sub print_distribution_map
{
    my %keys = map {/(.+)~.+/; $1=>$_} keys %distribution_map;
    foreach (sort keys %keys)
    {
        print $keys{$_}, "\t", $distribution_map{$keys{$_}}{'cnt'}, "\t", $distribution_map{$keys{$_}}{'ratio'} * 100, " %\n";
    }
}

sub calc_ratio
{
    foreach (keys %distribution_map)
    {
        $distribution_map{$_}{'ratio'} = $distribution_map{$_}{'cnt'} / $total_cnt;
    }
}

sub output_gnuplot_datafile
{
    open my $output_fh, ">dist.dat";

    my %keys = map {/(.+)~.+/; $1=>$_} keys %distribution_map;
    my $i = 0;
    foreach (sort keys %keys)
    {
        $i++;
        print $output_fh $i, "\t", $_, "\t", $distribution_map{$keys{$_}}{'ratio'} * 100, "\n";
    }

    close($output_fh);

    open $output_fh, ">title.gnuplot";
    print $output_fh "Time Cost Distribution: $total_cnt Operations";
    close($output_fh);

    `gnuplot gnuplot_dist.plot`;
    `rm -f title.gnuplot`;
    `mkdir -p output`;
    `mv dist.dat output/`;
    `mv dist.png output/`;
}

while (<>)
{
    next unless /<Elapsed: (\d+\.\d+)s>/;
    &analyse($1);
}

&calc_ratio;
&print_distribution_map;
&output_gnuplot_datafile if $graphic;

__END__
# vim: set expandtab tabstop=4 shiftwidth=4 foldmethod=marker:
