#!/bin/bash

# Edit the nic list for dpdk binding later.
# example: target_nic_list=("0000:09:00.0" "0000:09:00.1" "$next" "$next")
target_nic_list=("04:00.2" "04:00.3" "04:00.4")

# UIO driver the NICs used.
uio_driver="igb_uio"

# Where the hugepage mounted.
huge_dir="/mnt/huge"
dpdk_bind_bin=dpdk-devbind.py

# Target ethernet NIC number. 
# number of elements hold by the $target_nic_list.
target_nic_num=${#target_nic_list[@]}

system_nic_list=`lspci | grep Eth | grep Intel | awk '{print$1}'`
system_nic_num=`lspci | grep Eth | grep Intel | awk '{print$1}' | wc -l`

HUGE_PAGE=`cat /proc/meminfo | grep HugePages_Total | awk -F: '{print int($2)}'`
hugepages_num=1024

logfile=/tmp/dpdk_config_file.log

# check if system memory is enough for applications.
system_mem=`free -m | grep Mem | awk '{print $2}'`
system_mem_free=`free -m | grep Mem | awk '{print $4}'`

BIN=./src/x86_64-native-linuxapp-gcc/napd

# Minimum memroy
mem_threshold=16000

portmask=0
coremask=0

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
	# check NIC count.
	if [ $target_nic_num -gt $total_nic_num ]
	then
		log2 "no enough ethernet NICs: $target_nic_num $total_nic_num"
		cat $logfile
		exit
	fi

	# check NIC existence.
	for target_nic in ${target_nic_list[@]};do
		dpdk_nic_exist $target_nic
		if [ $? -eq 0 ]
		then
			log2 "no such NIC $target_nic: target NIC list error."
			cat $logfile
			exit
		fi
	done
}

dpdk_env_check() {
	# system memory
	if [ $system_mem -lt $mem_threshold ]
	then
		log2 "System memory is less than $mem_threshold"
	fi

	# mount huge dirent.
	huge_mount=`mount | grep $huge_dir`
	if [ "$huge_mount" == "" ]; then
		mkdir -p $huge_dir
		mount -t hugetlbfs nodev $huge_dir
	else
		log2 "hugetlbfs has already been mounted to $huge_dir" 
	fi

	if [ $HUGE_PAGE -eq 0 ]
	then
		sysctl vm.nr_hugepages=$hugepages_num
	else
		log2 "hugepages (num=$hugepages_num) already done"
	fi

	#disable ASLR
	echo 0 > /proc/sys/kernel/randomize_va_space
}

dpdk_nic_bind() {
	portmask=`echo "obase=16;ibase=10;2^$target_nic_num-1" |bc`

	#bind NIC
	for nic in ${target_nic_list[@]};do
		log2 "Trying to bind NIC $nic"
		#$dpdk_bind_bin -b $uio_driver $nic
	done
}

dpdk_nic_check
dpdk_env_check
dpdk_nic_bind

log2 "SYSM MEM  : $system_total_mem"
log2 "HUGE PAGES: $hugepages_num"
log2 "HUGE DIR  : $huge_dir"
log2 "PORT MASK : $portmask"
log2 "CORE MASK : $coremask"

# Startnapd
dpdk_lcore_conf="(0,0,1),(0,1,2),(0,2,3),(1,0,1),(1,1,2),(1,2,3),(2,0,1),(2,1,2),(2,2,3)"
#nohup $BIN -c 0xf -n 4 -- -p $pormask  --config=$dpdk_lcore_conf >> $logfile &
#$BIN -c 0xf -n 4 -- -p $pormask  --config=$dpdk_lcore_conf >> $logfile &

log2 "Startc $BIN done !"
