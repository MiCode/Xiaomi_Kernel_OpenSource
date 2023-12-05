#!/bin/sh
#set -x

POOL_DIR="/sys/kernel/reserve_pool"
POOL_PID_FILE="/sys/kernel/reserve_pool/pid"
WHITE_LIST="vendor.qti.came allocator-servi composer-servic"
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
cmp=$(cat ${POOL_PID_FILE} | awk -F ":" '{print $2}')
find_pid=""
if [ ${cmp} -ne 0 ]; then
	echo "error: Missing pid"
	echo "check white list pid:${pid} cmp:${cmp} white_list:${WHITE_LIST} [fail]"
	exit -1
else
	for white_list_comm in ${WHITE_LIST}; do
		for pid in ${pid_list}; do
			pid_comm=$(cat /proc/${pid}/comm)
			if [ ${pid_comm} = ${white_list_comm} ]; then
				find_pid="ok"
				break
			fi
		done

		if [ -z ${find_pid} ]; then
			echo "error: no find pid in white list."
			echo "check white list pid:${pid} cmp:${cmp} white_list:${WHITE_LIST} [fail]"
			exit -1
		fi
		find_pid=""
	done
fi
echo "check white list pid:${pid} cmp:${cmp} white_list:${WHITE_LIST} [pass]"

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
