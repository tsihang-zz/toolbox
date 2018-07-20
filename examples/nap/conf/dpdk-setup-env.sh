#!/bin/bash

# Edit the nic list for dpdk binding later.
# target_nic_list="09:00.0 09:00.1"
target_nic_list="09:00.1"

# uio driver the NICs used.
uio_driver="vfio-pci"

# Where the hugepage mounted.
huge_dir="/mnt/huge"

total_eth_num=`lspci | grep Eth | wc -l`

# Target ethernet NIC number. 
# number of elements hold by the $target_nic_list.
target_eth_num=`lspci | grep Eth | wc -l`

logfile=/tmp/dpdk_config_file.log

dpdk_bind_bin=dpdk-devbind.py

# check if system memory is enough for applications.
system_mem=`free -m | grep Mem | awk '{print $2}'`

log2()
{
	echo `date`:  $1 >> $logfile
}

huge_mount=`mount | grep $huge_dir`
if [ "$huge_mount" == "" ]; then
	mkdir -p $huge_dir
	mount -t hugetlbfs nodev $huge_dir
	log2 "hugetlbfs mount to $huge_dir success"
else
	log2 "hugetlbfs has already been mounted to $huge_dir" 
fi

HUGE_PAGE=`cat /proc/meminfo | grep HugePages_Total | awk -F: '{print int($2)}'`
hugepages_num=1024
if [ $HUGE_PAGE -eq 0 ]
then
	sysctl vm.nr_hugepages=$hugepages_num
	log2 "hugepages (num=$hugepages_num) config success"
else
	log2 "hugepages (num=$hugepages_num) already done"
fi

#disable ASLR
echo 0 > /proc/sys/kernel/randomize_va_space

#check NIC count
for nic in $target_nic_list;do
	target_eth_num=`expr $target_eth_num + 1`;
done

# right count
target_eth_num=`expr $target_eth_num - 1`;
if [ $target_eth_num -gt $total_eth_num ]
then
	log2 "no enough ethernet NICs"
	echo "$target_eth_num $total_eth_num no enough ethernet NICs"
	exit
fi

portmask=`echo "obase=16;ibase=10;2^$target_eth_num-1" |bc`

#bind NIC
for nic in $target_nic_list;do
	echo "Trying to bind NIC $nic"
	$dpdk_bind_bin -b $uio_driver $nic
done

# Startnapd
dpdk_lcore_conf="(0,0,1),(0,1,2),(0,2,3),(1,0,1),(1,1,2),(1,2,3),(2,0,1),(2,1,2),(2,2,3)"
nohup napd -c 0xf -n 4 -- -p $pormask  --config=$dpdk_lcore_conf >> $logfile &

