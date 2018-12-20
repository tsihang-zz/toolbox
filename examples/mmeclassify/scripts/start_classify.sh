
bin=src/$RTE_TARGET/mme_classify
pid=`pgrep $bin`
if [ "$pid"X == ""X ];then
        nohup ./$bin  2>&1 &
fi
