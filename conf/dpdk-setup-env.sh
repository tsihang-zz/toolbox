#!/bin/bash

mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

HUGE_PAGE=`cat /proc/meminfo | grep HugePages_Total | awk -F: '{print int($2)}'`
if [ $HUGE_PAGE -eq 0 ]
then
	sysctl vm.nr_hugepages=1024
else
	echo "huge page already config"
fi

TARGET=arm64-thunderx-linuxapp-gcc

dpdk-devbind.py -b vfio-pci  05:00.2
dpdk-devbind.py -b vfio-pci  05:00.3

#export RTE_SDK=`pwd`
#export RTE_TARGET=$TARGET
