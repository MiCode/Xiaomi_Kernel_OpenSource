#!/bin/sh

# ensure CONFIG_MI_MEMPOOL = m
if [ "$(ls /vendor/lib/modules/ | grep -i mi_mempool.ko)" != "mi_mempool.ko" ]; then
	echo "WARN: CONFIG_MI_MEMPOOL didn't set."
	exit 0
fi

# testcase1
if [ ! -d "/sys/kernel/mi_mempool" ]; then
	echo "ERROR: /sys/kernel/mi_mempool directory doesn't exist."
	exit 1
fi

# testcase2
cat /sys/kernel/mi_mempool/general_stat_info | awk 'NR > 2 && $2 != 0 { found_non_zero = 1; exit } END { exit !found_non_zero }'

if [ $? -ne 0  ]; then
	echo -e "ERROR: mi_mempool initial fill failed: \n $(cat /sys/kernel/mi_mempool/general_stat_info)"
	exit 1
fi

# testcase3

home_name_check=`ps -A | grep -E 'com\.miui\.home|\.globallauncher' | wc -l`

if [ $home_name_check -eq 0 ]; then
	echo -e "ERROR: can't find a process name in ('com.miui.home', '.globallauncher')"
	exit 1
fi

comm_list=` cat /sys/kernel/mi_mempool/process_use_count | grep comm | grep -v home | grep -v globallauncher`

IFS=' '
echo $comm_list | while read item
do
	item=${item#*:}
	name_check=`ps -A | grep ${item} | wc -l`
    if [ $name_check -eq 0 ]; then
		echo -e "ERROR: can't find a process name like ${item}"
		exit 1
	fi
done
