#!/bin/bash

app_bin=/usr/local/bin/napd

# File store all startup information.
logfile=/tmp/nap_startup.log

# NIC list trying to bind to DPDK.
# @example: target_nic_list=("0000:09:00.0" "09:00.1" "$next" "$next")
target_nic_list=("07:00.2" "07:00.3")

# Target ethernet NIC number. 
# number of elements hold by the $target_nic_list.
# @WARNING "DO NOT EDIT IT".
target_eth_num=${#target_nic_list[@]}

# DPDK supplied UIO driver.
#uio_driver="vfio-pci"
uio_driver="igb_uio"

keyword="Intel"
#keyword="a034"

# $dpdk_bind_bin calls "dpdk_nic_bind.py in dpdk-16.07, it is
# different with dpdk-17.xx
dpdk_bind_bin=$RTE_SDK/usertools/dpdk-devbind.py

uio_path=$RTE_SDK/$RTE_TARGET/kmod

# Mount point of huge pages.
huge_dir="/mnt/huge"

# Cache for temp data, this is a ramdisk
data_dir="/data/"

# PID
app_home="/var/run/nap"

# Total remain memory. 
system_mem=`free -m | grep Mem | awk '{print $2}'`

# DPDK supported NIC list, almost Intel.
system_nic_list=`lspci | grep Eth | grep $keyword | awk '{print$1}'`

# Count of $sys_nic_list
system_nic_num=`lspci | grep Eth | grep $keyword | awk '{print$1}' | wc -l`

# Total hugepages which configured with /boot/default/grub.cfg
HUGE_PAGE=`cat /proc/meminfo | grep HugePages_Total | awk -F: '{print int($2)}'`

# Number of cores per app occupied
# @WARNING "DO NOT EDIT IT."
app_cores=1

# Minimum memroy for NCAPD
mem_threshold=64000

