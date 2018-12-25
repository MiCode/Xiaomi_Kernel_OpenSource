#!/usr/bin/env python
# -*- coding: utf-8 -*-

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

import os
import sys
import struct


head = [struct.pack('I',0xffffffff) for n in range(128)]

def parse_dtb(input):
	dtb_list = []
	with open(input, 'rb') as f:
		img_data = f.read()
		img_size = f.tell()
		dtb_offset = 0
		while dtb_offset <= img_size - 8:
			dtb_magic = struct.unpack('>I', img_data[dtb_offset : dtb_offset+4])[0]
			if dtb_magic == 0xD00DFEED:
				dtb_size = struct.unpack('>I', img_data[dtb_offset+4 : dtb_offset+8])[0]
				dtb_list.append(dtb_offset)
				dtb_offset = dtb_offset + dtb_size
			else:
				dtb_offset = dtb_offset + 1
		print('{}.'.format(dtb_list))
		f.closed
	return dtb_list

def write_header(output_file, input_file, dtb_list):
	head[0] = struct.pack('I',0xdeaddead) #Magic number
	head[1] = struct.pack('I', os.path.getsize(input_file))#dtbo size without header
	head[2] = struct.pack('I', 512) #Header Size
	head[3] = struct.pack('I', 2) #Header version
	head[4] = struct.pack('I', len(dtb_list)) #number of dtbo
	head[5] = struct.pack('I', 0xffffffff) #Reserved
	head[6] = struct.pack('I', 0xffffffff) #Reserved
	head[7] = struct.pack('I', 0xffffffff) #Reserved

	i = 0
	for offset in dtb_list:
		head[8 + i] = struct.pack('I', offset)
		i = i + 1

	with open(output_file, 'w') as fo:
		for item in head:
			fo.write("%s" % item)
		with open(input_file, 'r') as fi:
			for line in fi.readlines():
				fo.write(line)
			fi.close
		fo.close

def main(argv):
	if len(argv) < 2:
		print("Usage: python post_process_dtbs.py odmdtbs.dtb odmdtbo.img")
		sys.exit(1)
	input_img = argv[1]
	output_img = argv[2]
	dtb_list = parse_dtb(input_img)

	if len(dtb_list) > 111 :
		print("ERROR: Append too much DTBO")
		sys.exit(1)

	write_header(output_img, input_img, dtb_list)

if __name__ == '__main__':
	main(sys.argv)
