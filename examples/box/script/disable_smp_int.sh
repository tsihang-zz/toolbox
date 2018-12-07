for dir in `find /proc/irq -maxdepth 1 -type d`;do
	if [ -f $dir/smp_affinity ] ;then 
		echo "$dir/smp_affinity: `cat $dir/smp_affinity` set to 0x1"
		echo 1 >  $dir/smp_affinity 
	fi
done


