set terminal png transparent size 640,240
set size 1.0,1.0

set terminal png transparent size 640,480
set output 'lines_of_code_by_author.png'
set key left top
set yrange [0:]
set xdata time
set timefmt "%s"
set format x "%Y-%m-%d"
set grid y
set ylabel "Lines"
set xtics rotate
set bmargin 6
plot 'lines_of_code_by_author.dat' using 1:2 title "j1nma" w lines, 'lines_of_code_by_author.dat' using 1:3 title "Martin Grabina" w lines, 'lines_of_code_by_author.dat' using 1:4 title "juan" w lines, 'lines_of_code_by_author.dat' using 1:5 title "Unknown" w lines, 'lines_of_code_by_author.dat' using 1:6 title "Martin Capparelli" w lines, 'lines_of_code_by_author.dat' using 1:7 title "Martín Capparelli" w lines, 'lines_of_code_by_author.dat' using 1:8 title "Juan Manuel Alonso" w lines
