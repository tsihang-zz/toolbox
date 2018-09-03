#!/bin/bash

source /usr/local/etc/nap/napcfg.sh

log2() {
	echo `date`: $1
	echo `date`: $1 >> $logfile
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
	
	echo "all system NICs($system_nic_num) ..."
	for system_nic in ${system_nic_list[@]};do
		echo "$system_nic"
	done	
	
	# check NIC existence.
	for target_nic in ${target_nic_list[@]};do
		dpdk_nic_exist $target_nic
		if [ $? -eq 0 ]
		then
			log2 "%% No such NIC $target_nic: target NIC list error,
						check $logfile for more details."
			exit
		fi
	done
}

#env check
dpdk_env_check() {
	
	# VFIO-PCI is an UIO for Thunderx NIC
	# modprobe vfio-pci
	
	# Insert DPDK supplied UIO driver 
	insmod $uio_path/$uio_driver.ko

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
	portmask=`echo "obase=16;ibase=10;2^$target_nic_num-1" |bc`

	# bind each NIC in $target_nic_list
	for nic in ${target_nic_list[@]};do
		log2 "Trying to bind NIC $nic"
		$dpdk_bind_bin -b $uio_driver $nic
		$dpdk_bind_bin -s | grep $nic >> /tmp/eth_info
	done
}

proc_id() {

	id=`pgrep $1`

	num=`pgrep $1 | wc -l`
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
	n=`pgrep $1 | wc -l`
	return $n
}

dpdk_start_nap() {
	
	wait_sec=5

	# config="(port,queue,lcore)"
	#nohup $app_bin -c 0xf -n 2 -- -p 0x7 --config="(0,0,1),(0,1,2),(0,2,3),(1,0,1),(1,1,2),(1,2,3),(2,0,1),(2,1,2),(2,2,3)" >> $logfile 2>&1 &
	nohup $app_bin -c 0xf -n 2 -- -p 0x3 --config="(0,0,1),(0,1,2),(1,0,1),(1,1,2)" >> $logfile 2>&1 &

	echo "Starting $app_bin ... (wait $wait_sec secs)"	
	sleep $wait_sec

	app_pid=`ps -ef|grep  "napd" |grep -v "grep"  |awk '{print $2}'`
	if [ "$app_pid" == "" ];then
		echo "%% napd init failed, please see $logfile for more details." 
		log2 "%% napd init failed, please see $logfile for more details." 
		exit
	fi

	log2 "startup napd success."
}

log2 "Start NAPD initialization ..."

dpdk-devbind.py -b thunder-nicvf 05:00.1
sleep 3

ifconfig enp5s0f1 up
sleep 3

ifconfig lan1 up
ifconfig lan2 up
ifconfig lan3 up
ifconfig lan4 up
ifconfig lan5 up
ifconfig lan6 up
ifconfig lan7 up
ifconfig lan8 up


ifconfig enp5s0f1 down

dpdk_nic_check
dpdk_env_check
dpdk_nic_bind
dpdk_start_nap 

log2 "Initialize successfull!"
