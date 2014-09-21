/*
 * Japan Display Inc. INPUT_MT_WRAPPER Device Driver
 *
 * Copyright (C) 2013-2014 Japan Display Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 *
 */
#ifndef _INPUT_MT_WRAPPER_H_
#define _INPUT_MT_WRAPPER_H_

#define INPUT_MT_WRAPPER_MAX_FINGERS (10)
#define INPUT_MT_WRAPPER_MIN_AREA (0)
#define INPUT_MT_WRAPPER_MAX_AREA (10)
#define INPUT_MT_WRAPPER_MIN_X (0)
#define INPUT_MT_WRAPPER_MAX_X (1080)
#define INPUT_MT_WRAPPER_MIN_Y (0)
#define INPUT_MT_WRAPPER_MAX_Y (1920)
#define INPUT_MT_WRAPPER_MIN_Z (0)
#define INPUT_MT_WRAPPER_MAX_Z (255)

struct input_mt_wrapper_touch_data {
	unsigned short x;
	unsigned short y;
	unsigned short z;
	unsigned short t;
};

struct input_mt_wrapper_ioctl_touch_data {
	struct input_mt_wrapper_touch_data touch[INPUT_MT_WRAPPER_MAX_FINGERS];
	unsigned char t_num;
};

/* commands */
#define INPUT_MT_WRAPPER_IO_TYPE  (0xB9)
#define INPUT_MT_WRAPPER_IOCTL_CMD_SET_COORDINATES \
	_IOWR(INPUT_MT_WRAPPER_IO_TYPE, 0x01, \
	struct input_mt_wrapper_ioctl_touch_data)

#endif /* _INPUT_MT_WRAPPER_H_ */
