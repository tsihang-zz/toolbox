#!/bin/bash

# reset all configurations

uio_driver=vfio-pci
dpdk_bind_bin=dpdk-devbind.py
netdev_nic_list=("05:00.1" "05:00.2" "05:00.3")

#sleep 3

#ifconfig enp5s0f1 up
#sleep 3

#ifconfig lan1 up
#ifconfig lan2 up
#ifconfig lan3 up
#ifconfig lan4 up
#ifconfig lan5 up
#ifconfig lan6 up
#ifconfig lan7 up
#ifconfig lan8 up
#

mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

HUGE_PAGE=`cat /proc/meminfo | grep HugePages_Total | awk -F: '{print int($2)}'`
if [ $HUGE_PAGE -eq 0 ]
then
        sysctl vm.nr_hugepages=1024
else
        echo "huge page already config"
fi

$dpdk_bind_bin -b $uio_driver  05:00.1
$dpdk_bind_bin -b $uio_driver  05:00.2
$dpdk_bind_bin -b $uio_driver  05:00.3

