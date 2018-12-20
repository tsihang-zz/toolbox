#!/bin/bash

proc_id() {

	id=`pgrep $1`

	num=`pgrep $1 | wc -l`
	if [ $num -eq 1 ]; then
		return $id
	fi

	if [ $num -gt 1 ]; then
		for v in ${id[@]};do
			return $v
		done
	fi
}

proc_num() {
	n=`pgrep $1 | wc -l`
	return $n
}

stop_app() {
	app=$1
	
	proc_num $app
	n=$?

	if [ $n -ne 0 ];then
		killall -9 $app > /dev/null 2>&1
	else
		echo "Stopping $app ... done"
		return
	fi
	
	while [ $n -ne 0 ];do
		proc_num $app
		n=$?
	done	

	echo "Stopping $app ... done"
}

stop_app napd

