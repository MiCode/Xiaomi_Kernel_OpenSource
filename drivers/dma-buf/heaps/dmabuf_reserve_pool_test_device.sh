#!/bin/sh
#set -x

POOL_DIR="/sys/kernel/reserve_pool"
POOL_PID_FILE="/sys/kernel/reserve_pool/pid"
POOL_CONFIG_FILE="/sys/kernel/reserve_pool/config"

# check function
if [ ! -d ${POOL_DIR} ]; then
	echo "ERROR: reserve_pool not adapted."
	echo "check func dir [fail]"
	exit -1
fi
echo "check func dir [pass]"

# check white list
pid_list=$(cat ${POOL_PID_FILE} | awk '{print $1" "$2" "$3}')
first_pid=$(cat ${POOL_PID_FILE} | awk '{print $1}')
cmp=$(cat ${POOL_PID_FILE} | awk -F ":" '{print $2}')
if [ ${cmp} -ne 0 ]; then
	echo "error: Missing pid"
	echo "check white list pid:${pid_list} cmp:${cmp} [fail]"
	exit -1
else
	kill -9 ${first_pid}
	sleep 8
	pid_update_log=$(dmesg| grep "from list" | grep ${first_pid})
	if [ $? -ne 0 ]; then
		echo "error: pid updated"
		echo "check pid update [fail]"
		exit -1
	fi
fi
new_pid_list=$(cat ${POOL_PID_FILE} | awk '{print $1" "$2" "$3}')
echo "pid updated old_pid:${pid_list} new_pid:${new_pid_list} cmp:${cmp} [pass]"

# check refill thread
refill_thread_name="reserve-refill"
ps -A | grep ${refill_thread_name} > /dev/null
if [ $? -ne 0 ]; then
	echo "error: refill thread absent"
	echo "check refill thread [fail]"
	exit -1
fi
echo "check refill thread [pass]"

# check refill pool
refill_count_key="reserve_pool_refill_count"
pool_reserve_order_key="reserve_pool_reserve_order"
refill_count=$(cat ${POOL_CONFIG_FILE} | grep ${refill_count_key} | sed s/]//g | awk '{print $NF}')
pool_reserve_order=$(cat ${POOL_CONFIG_FILE} | grep ${pool_reserve_order_key} | sed s/]//g | awk '{print $NF}')
if [ ${refill_count} -eq 0 -o ${pool_reserve_order} -eq 0 ]; then
	echo "error: empty pool"
	echo "check refill pool [fail]"
	exit -1
fi
echo "check refill pool [pass]"
