#!/bin/bash
echo "check bin tools"
check_bin="lspci  bc  python  awk grep wc ethtool"

for file in $check_bin
do
        which $file >/dev/null
        if [ $? -eq 0 ] 
        then
                continue
        fi  
        echo Error ! "$file" not exist! Install failed!!
        exit
done

echo "install ncapd bin and script..."
chmod 755 ./*

mkdir -p /usr/local/dpdk
cp ./* /usr/local/dpdk 2>/dev/null

curtime=`date +%Y%m%d%H%M%S`

cp /usr/local/dpdk/ncapdcfg.sh /usr/local/dpdk/ncapdcfg.sh.$curtime
file_list="igb_uio.ko ncapds ncapdc startncapd.sh stopncapd.sh ncapdcfg.sh"
for i in $file_list
do
   rm /usr/local/bin/$i>/dev/null 2>/dev/null
   ln -s /usr/local/dpdk/$i /usr/local/bin
done

echo "done"
