ifconfig br0 down
sleep 1
brctl delbr br0
sleep 1

ifconfig enp5s0f1 down
sleep 1
ifconfig enp5s0f2 down
sleep 1
ifconfig lan6 down
sleep 1

brctl addbr br0
sleep 1
brctl addif br0 lan6
sleep 1
brctl addif br0 enp5s0f2
sleep 1


ifconfig enp5s0f1 up
sleep 1
ifconfig lan6 up
sleep 1
ifconfig enp5s0f2 up
sleep 1
ifconfig br0 up

