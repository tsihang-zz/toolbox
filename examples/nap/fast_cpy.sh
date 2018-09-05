host=192.168.0.193
home_dir=/root/toolbox/examples/nap
scp src/* $host:$home_dir/src/
scp src/include/* $host:$home_dir/src/include/
scp quagga/* $host:$home_dir/quagga/
scp oryx/* $host:$home_dir/oryx/
