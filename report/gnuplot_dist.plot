name = system("cat title.gnuplot")
set title name

set autoscale
set yrange [0:]
set term png size 1200,800
set output "dist.png"
set style line 1 lw 2 lc 3
set boxwidth 0.5
set style fill solid 0.5
plot "dist.dat" using 1:3:xticlabels(2) with boxes ls 1
