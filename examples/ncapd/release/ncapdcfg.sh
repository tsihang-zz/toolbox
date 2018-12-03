#!/bin/bash

# File store all startup information.
logfile=/root/ncapd_startup.log

# Step1: NCAPD capture mode selection.
# Valid options including: "dpdk" or "kernel"
capture_mode="kernel"

# Step2: NIC list we are going to use.
# vlaid when $capture_mode equal with "dpdk"
# @example: target_nic_list=("09:00.0" "09:00.1" "$next" "$next")
# you can see system supported NIC list by running "lspci | grep Eth"
target_nic_list=("$NIC0" "$NIC1" "$NIC2")
if [ "$capture_mode"X != "dpdk"X ]; then
	# Valid when $capture_mode equal with "kernel"
	vdev=eth0
	target_eth_num=1
else
	target_eth_num=${#target_nic_list[@]}
fi

# Step3: Optional NIC filter setting
# @examples: "VMXNET3", "Intel" etc..
#nic_key_word=""
#if [ "$nic_key_word" == "" ];then
	# DPDK supported NIC list, almost Intel.
#	system_nic_list=(`lspci | grep Eth | grep $nic_key_word | awk '{print$1}'`)
#else
#	system_nic_list=(`lspci | grep Eth | awk '{print$1}'`)
#fi
system_nic_list=`lspci | grep Eth | awk '{print$1}'`
# Count of $sys_nic_list
#system_nic_num=${#system_nic_list[@]}
system_nic_num=`lspci | grep Eth | awk '{print$1}' | wc -l`

echo "System NIC list($system_nic_num): $system_nic_list"
echo "Working NIC list($target_eth_num): $target_nic_list"

# Step4: specify the UIO driver.
# DPDK supplied UIO driver.
uio_driver="igb_uio"

# Mount point of huge pages.
huge_dir="/mnt/huge"

# Cache for CDR, this is a ramdisk
data_dir="/data/"

# PID
ncapd_home="/var/run/ncapd"

# Total remain memory. 
system_mem=`free -m | grep Mem | awk '{print $2}'`

# $dpdk_bind_bin calls "dpdk_nic_bind.py in dpdk-16.07, it is
# different with dpdk-17.xx
dpdk_bind_bin=/usr/local/dpdk/dpdk_nic_bind.py

# Total hugepages which configured with /boot/default/grub.cfg
HUGE_PAGE=`cat /proc/meminfo | grep HugePages_Total | awk -F: '{print int($2)}'`

# IP address for each port
# @WARNING unsued now
ip_addr=("0.0.0.0" "0.0.0.0" "0.0.0.0" "0.0.0.0")

# Number of cores per ncapds occupied
# @WARNING "DO NOT EDIT IT."
ncapds_cores=1

# Number of cores per ncapdc occupied
# @WARNING "DO NOT EDIT IT."
ncapdc_cores=1

# Number of ncapdcs, which should be lanuched for CDRs classifier
# @wARNING "DO NOT EDIT IT."
ncapdc_num=1

# Minimum memroy for NCAPD
mem_threshold=64000

