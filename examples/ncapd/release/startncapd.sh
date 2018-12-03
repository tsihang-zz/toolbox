#!/bin/bash
source /etc/profile
source /usr/local/dpdk/ncapdcfg.sh

log2() {
	echo `date`:  $1 >> $logfile
}

dpdk_nic_exist() {
	for system_nic in ${system_nic_list[@]};do
		if [ $system_nic == "$1" ]
		then
			return 1
		fi
	done	
	return 0
}

dpdk_nic_check() {
	# check NIC existence.
	for target_nic in ${target_nic_list[@]};do
		dpdk_nic_exist $target_nic
		if [ $? -eq 0 ]
		then
			$dpdk_bind_bin --status >> $logfile
			log2 "%% No such NIC $target_nic: target NIC list error,
						check $logfile for more details."
			cat $logfile
			exit
		fi
	done
}

#env check
dpdk_env_check() {

	# remove Kernel supplied UIO
	modprobe uio > /dev/null
	
	# VFIO-PCI is an UIO for Thunderx NIC
	# modprobe vfio-pci > /dev/null
	
	# Insert DPDK supplied UIO driver 
	insmod /usr/local/bin/$uio_driver.ko > /dev/null

	# Memory check.
	if [ $system_mem -lt $mem_threshold ];then
		log2 "Insufficient memory, $mem_threshold GB+."
	fi

	# Mount huge dirent.
	huge_mount=`mount | grep $huge_dir`
	if [ "$huge_mount" == "" ];then
		mkdir -p $huge_dir
		mount -t hugetlbfs nodev $huge_dir
		log2 "Trying to mount hugetlbfs to $huge_dir ... done"
	else
		log2 "hugetlbfs has already been mounted to $huge_dir" 
	fi
	
	# Mount ramdisk for raw cdr storage.
	data_mount=`mount | grep "/data"`
	if [ "$data_mount" == "" ]; then
		mkdir -p $data_dir
		mount -t ramfs none $data_dir
		log2 "Trying to mount ramdisk to $data_dir ... done"
	else
		log2 "ramdisk has already been mounted to $data_dir" 
	fi

	# hugepages allocation.
	hugepages_num=`echo "3076*$target_eth_num" | bc`
	if [ $HUGE_PAGE -eq 0 ]
	then
		sysctl vm.nr_hugepages=$hugepages_num
		log2 "Trying to allocate hugepages $hugepages_num ... done"
	else
		log2 "hugepages has (num=$hugepages_num) already done"
	fi

	# $ncapd_home
	if [ ! -d $ncapd_home ]; then
		mkdir -p $ncapd_home
		log2 "mkdir $ncapd_home"
	fi

	# disable ASLR
	echo 0 > /proc/sys/kernel/randomize_va_space
}

dpdk_nic_bind() {
	if [ "$capture_mode"X == "dpdk"X ]; then
		portmask=`echo "obase=16;ibase=10;2^$target_nic_num-1" |bc`

		# bind each NIC in $target_nic_list
		for nic in ${target_nic_list[@]}; do
			log2 "Trying to bind NIC $nic"
			$dpdk_bind_bin -b $uio_driver $nic > /dev/null
			$dpdk_bind_bin -s | grep $nic >> /tmp/eth_info
		done
	else
		echo "$vdev" >> /tmp/eth_info
	fi
}

proc_id() {

	id=`pgrep $1 2>&1`

	num=`pgrep $1 | wc -l 2>&1`
	if [ $num -eq 1 ]; then
		return $id
	fi

	if [ $num -gt 1 ]; then
		for v in ${id[@]};do
			return $v
		done
	fi
}

proc_num() {
	n=`pgrep $1 | wc -l 2>&1`
	return $n
}

dpdk_start_ncapds() {
#	bin_name="ncapds"
#	proc_id $bin_name
#	pid=$?
#
#	if [ -n $pid ]
#	then
#		log2 "%% $bin_name is running"
#		return
#	else
#		log2 "Trying to startup $bin_name "
#	fi

	wait_sec=5
	nr_channel=2

	if [ "$capture_mode"X == "dpdk"X ]; then
		nohup /usr/local/bin/ncapds -c $1 -n $nr_channel -- -p $2  >> $logfile 2>&1 &
	else
		log2 "run ncapds with $capture_mode mode"
		nohup /usr/local/bin/ncapds -c $1 -n 2 --vdev=eth_pcap0,iface=$vdev --log-level 8 -- -p $2  >> $logfile 2>&1 &
	fi

	echo "Starting ncapds ... (wait $wait_sec secs)"	
	sleep $wait_sec

	ncapds_pid=`pgrep ncapds 2>&1`
	if [ "$ncapds_pid" == "" ];then
		echo "%% ncapds init failed, please see $logfile for more details." 
		$dpdk_bind_bin --status >> $logfile
		exit
	fi

	log2 "startup ncapds @coremask $coremask, @portmask $portmask"
}

dpdk_start_ncapdc() {
#	bin_name="ncapdc"
#	proc_id $bin_name
#	pid=$?
#
#	if [ -n $pid ]
#	then
#		log2 "%% $bin_name is running"
#		return
#	else
#		log2 "Trying to startup $bin_name "
#	fi

	wait_sec=5
	nr_channel=2
	
	for i in `seq $target_eth_num`;do
		ip_addr_cfg+=" -i "${ip_addr[$i-1]}
	done

	for n in `seq $1`;do
		id=`expr $n - 1`
		local_coremask=`echo "obase=16;ibase=10;(2^($ncapdc_cores)-1)*(2^($ncapds_cores + $n))" |bc`

		if [ "$capture_mode"X == "dpdk"X ]; then
			nohup /usr/local/bin/ncapdc  -c $local_coremask -n $nr_channel --proc-type=auto --no-hpet -- -p $data_dir -t 2 -D -P $ncapd_home/ncapd.$id.pid ${ip_addr_cfg} >> $logfile 2>&1 &
			#/usr/local/bin/ncapdc  -c $coremask -n $nr_channel --proc-type=auto --no-hpet -- -p $data_dir -t 2 -D -P $ncapd_home/ncapd.$id.pid ${ip_addr_cfg} -F forword >> $logfile 2>&1 &
		else
			log2 "run ncapdc with $capture_mode mode"
			nohup /usr/local/bin/ncapdc  -c $local_coremask -n $nr_channel --vdev=eth_pcap0,iface=$vdev --proc-type=auto --no-hpet -- -p /data/ -t 2 -D -P /var/run/ncapd/ncapd.$id.pid ${ip_addr_cfg} >> $logfile 2>&1 &
			#/usr/local/bin/ncapdc  -c $local_coremask -n $nr_channel --vdev=eth_pcap0,iface=$vdev --proc-type=auto --no-hpet -- -p /data/ -t 2 -D -P /var/run/ncapd/ncapd.$id.pid ${ip_addr_cfg} -F forward >> $logfile 2>&1 &
		fi
		echo "Starting ncapdc$id ... (waiting $wait_sec secs)"	
	done
	sleep $wait_sec
	ncapdcs=`pgrep ncapdc | wc -l 2>&1`
	echo "$ncapdcs $1"
	if [ $ncapdcs -ne $1 ];then
		echo "%% Trying to create $1 ncapdc(s), but failed, please see $logfile for more details." 
		log2 "%% Trying to create $1 ncapdc(s), but failed, please see $logfile for more details." 
		exit
	fi
	log2 "startup $ncapdc_num ncapdc(s)"
}

umount_point(){
	# eth_info is used by nfdump2fifo
	rm /tmp/eth_info
	# Check hugepage mounted point.
	huge_dir=`mount | grep hugetlbfs | awk '{print $3}'`
	if [ "$huge_dir"x != ""x ]; then
		umount $huge_dir > /dev/null
	fi
	# Check ramfs mounted point
	ramfs_dir=`mount | grep ramfs | awk '{print $3}'`
	if [ "$ramfs_dir"x != ""x ]; then
		umount $ramfs_dir > /dev/null
	fi
}

log2 "Start NCAPD initialization ..."

ncapds_pid=`pgrep ncapds 2>&1`
if [ "$ncapds_pid"x != ""x ]; then
	echo "%% ncapds (pid=$ncapds_pid) is running"
	exit
fi

ncapdc_pid=`pgrep ncapdc 2>&1`
if [ "$ncapdc_pid"x != ""x ]; then
	echo "%% ncapdc (pid=$ncapdc_pid) is running"
	exit
fi

if [ -z "$capture_mode" ]; then
	echo "capture_mode not set"
	exit
fi

if [ "$capture_mode"X != "kernel"X -a "$capture_mode"X != "dpdk"X ]; then
	echo "Undefined capture mode $capture_mode"
fi

echo "Run ncapd with $capture_mode mode"

rm /tmp/eth_info
dpdk_nic_check
dpdk_env_check
dpdk_nic_bind

nfdump_pid=`pgrep nfdump2fifo 2>&1`
if [ "$nfdump_pid" != "" ];then
	echo "nfdump is still running, try stopnfdump2fifo.sh to close"
	exit
fi

coremask=`echo "obase=16;ibase=10;(2^($ncapds_cores+1)-1)" |bc`
portmask=`echo "obase=16;ibase=10;2^$target_eth_num-1" |bc`

dpdk_start_ncapds $coremask $portmask
dpdk_start_ncapdc $ncapdc_num

#(/usr/local/bin/nfdump2fifo &)
#(/usr/local/bin/nfdump2fifo_db&)
#(/usr/local/bin/nfdump2fifo_kafka &)
#(/usr/local/bin/nfdump2fifo_refill &)
#(/usr/local/bin/nfdump2fifo_stat &)
#(/usr/local/bin/nfdump2fifo_track &)

log2 "Initialize successfull!"
