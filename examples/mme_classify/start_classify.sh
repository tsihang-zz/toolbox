
bin_home=src/x86_64-native-linuxapp-gcc
bin=classify
pid=`pgrep $bin`
if [ "$pid"X == ""X ];then
        nohup ./$bin_home/$bin  2>&1 &
fi
