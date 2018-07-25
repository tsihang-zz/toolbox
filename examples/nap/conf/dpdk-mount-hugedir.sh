#!/bin/bash

umount /mnt/huge
mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

HUGE_PAGE=`cat /proc/meminfo | grep HugePages_Total | awk -F: '{print int($2)}'`
if [ $HUGE_PAGE -eq 0 ]
then
	sysctl vm.nr_hugepages=1024
else
	echo "huge page already config"
fi
