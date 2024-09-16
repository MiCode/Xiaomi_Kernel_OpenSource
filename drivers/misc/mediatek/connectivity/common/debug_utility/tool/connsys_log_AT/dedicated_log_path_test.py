#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

import sys, os, time
import subprocess
import re
import optparse

emi_log = 'emi.log'
out_path = ''
MCU_INDEX = 0
WIFI_INDEX = 1
BT_INDEX =2
GPS_INDEX = 3
WMTDRV_TYPE_BT = 0
WMTDRV_TYPE_GPS = 2
WMTDRV_TYPE_WIFI = 3

def get_last_emi_log(log_fd, num_line):
	log_list = []
	for line in log_fd:
		if line.find('emi:') > 0:
			log_list.append(line)
	num_line *= -1
	return log_list[num_line:]

def dump_emi(offset, size):
	global out_path
	dump_emi_init_cmd = "adb shell \"echo 0x2d " + hex(offset) + " " + hex(size) + " " + "> /proc/driver/wmt_dbg\""
	get_emi_log_cmd = "adb shell \"dmesg | grep emi\" > " + os.path.join(out_path, emi_log)
	subprocess.call(dump_emi_init_cmd, shell=True)
	subprocess.call(get_emi_log_cmd, shell=True)
	log_fd = open(os.path.join(out_path, emi_log), 'rb')
	log_list = get_last_emi_log(log_fd, 4)
	log_fd.close
	return log_list

def find_base_addr(line_list):
	base_addr_list = []
	count = 0
	for line in line_list:
		# print line
		base_addr = ''
		pos = line.find('emi:')
		digit_list = line[pos+4:].split(' ')
		for no, dig in enumerate(digit_list, start=1):
			if len(dig) == 2:
				base_addr = dig + base_addr
				if no % 4 == 1:
					count+=1
					if count % 2 == 1:
						base_addr_list.append(int(base_addr, 16))
					base_addr = ''
				if count >= 8:
					break
	return base_addr_list

def subsys_test(line_list, subsys_name):
	for line in line_list:
		base_addr = ''
		count = 0
		pos = line.find('emi:')
		digit_list = line[pos+4:].split(' ')
		for no, dig in enumerate(digit_list, start=1):
			if len(dig) == 2:
				base_addr = dig + base_addr
				if no % 4 == 1:
					if int(base_addr, 16) > 0:
						count += 1
						if count == 2:
							print '------------------->' + subsys_name + ' connsys log Test PASS!'
							return;
					base_addr = ''
	print '------------------->' + subsys_name + ' connsys log Test FAIL!'

def subsys_fucn_ctrl(wmtdrv_type, on):
	func_ctrl_cmd = "adb shell \"echo 0x07 " + hex(wmtdrv_type) + " " + hex(on) + " " + "> /proc/driver/wmt_dbg\""
	subprocess.call(func_ctrl_cmd, shell=True)
	if on == 1:
		time.sleep(5)
	else:
		time.sleep(1)

def main():
	global out_path
	out_folder = sys.argv[1]
	out_path = os.path.join(os.getcwd(), out_folder)
	print out_path
	if(not os.path.exists(out_path)):
		os.makedirs(out_path)

	line_list = dump_emi(0, 64)
	base_addr_list = []

	# TEST CASE1 INIT
	if line_list[-1].find('EMIFWLOG'):
		print '------------------->' + 'Connsys Debug Utility init Test PASS!'
	else:
		print 'EMIFWLOG not found.'
		print '------------------->' + 'Connsys Debug Utility init Test FAIL!'
		return

	base_addr_list = find_base_addr(line_list)
	print 'subsys base address offset:'
	print base_addr_list

	subsys_fucn_ctrl(WMTDRV_TYPE_WIFI, 1)
	line_list = dump_emi(base_addr_list[WIFI_INDEX], 64)
	subsys_test(line_list, 'WIFI')

	subsys_fucn_ctrl(WMTDRV_TYPE_BT, 1)
	line_list = dump_emi(base_addr_list[BT_INDEX], 64)
	subsys_test(line_list, 'BT')

	ygps_cmd = "adb shell \"am start -n com.mediatek.ygps/.YgpsActivity\""
	subprocess.call(ygps_cmd, shell=True)
	time.sleep(30)
	line_list = dump_emi(base_addr_list[GPS_INDEX], 64)
	subsys_test(line_list, 'GPS')
	ygps_cmd = "adb shell \"am force-stop com.mediatek.ygps\""
	time.sleep(1)

if __name__ == '__main__':
	main()
