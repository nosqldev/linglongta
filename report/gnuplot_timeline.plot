set autoscale
set yrange [0:]
set xlabel "Timeline(Sec)"
set ylabel "Qps"
set term png size 1600,1000
set output "timeline.png"
set style line 1 lw 0.5 lc 8
set style line 2 lw 0.5 lc 1

plot "timeline.dat" using 1:2 title "Get Operations" with lines ls 1, \
     "timeline.dat" using 1:3 title "Set Operations" with dots ls 2
