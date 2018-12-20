bin=classify
pid=`pgrep $bin`

for p in $pid
do
	echo "killing $p"
	kill -9 $p
done

