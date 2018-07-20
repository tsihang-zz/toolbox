#!/bin/bash

# reset all configurations

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

ifconfig enp5s0f1 down
dpdk-devbind.py -b vfio-pci  05:00.1
dpdk-devbind.py -b vfio-pci  05:00.2
dpdk-devbind.py -b vfio-pci  05:00.3

export RTE_SDK=`pwd`
export RTE_TARGET=$TARGET
