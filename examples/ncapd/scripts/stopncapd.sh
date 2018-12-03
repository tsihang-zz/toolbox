#!/bin/bash

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

stop_ncapd() {
	app=$1
	
	proc_num $app
	n=$?

	if [ $n -ne 0 ];then
		killall -9 $app > /dev/null 2>&1
	else
		echo "Stopping $app ... done"
		return
	fi
	
	while [ $n -ne 0 ];do
		proc_num $app
		n=$?
	done	


	echo "Stopping $app ... done"
}

stop_ncapd_final(){
	
	huge_dir=`mount | grep hugetlbfs | awk '{print $3}'`
	if [ "$huge_dir"x != ""x ]; then
		umount $huge_dir > /dev/null
	fi

	ramfs_dir=`mount | grep ramfs | awk '{print $3}'`
	if [ "$ramfs_dir"x != ""x ]; then
		umount $ramfs_dir > /dev/null
	fi
}

stop_ncapd ncapdc
stop_ncapd ncapds
stop_ncapd_final

#source /usr/local/dpdk/stopnfdump2fifo.sh

#pci_bind_script=/usr/local/dpdk/dpdk_nic_bind.py
#
#$pci_bind_script --status | grep "drv=igb_uio" > /tmp/eth_info
#active_eth_num=`cat /tmp/eth_info | wc -l`
#
#for i in `seq $active_eth_num`
#do
#	PCI_ID=`cat /tmp/eth_info | awk -v row=$i 'NR==row {print $1}'`
#	DRV=`cat /tmp/eth_info | awk -F '=' -v row=$i 'NR==row {print $3}'`
#	$pci_bind_script -b $DRV $PCI_ID
#done
