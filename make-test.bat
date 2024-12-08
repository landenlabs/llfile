mkdir test
dir > test\x1
dir > test\x2
dir > test\x3
cd test
mkdir y dst val1 val2
copy x* y\
dir > val1\data1.txt
copy val1\data1.txt val2\data2.txt
dir > dst\data1.txt
copy dst\data1.txt dat\data2.txt

