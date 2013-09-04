/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __HBM_H
#define __HBM_H

#include <linux/kernel.h>

/**
 * usb_host_bam_type
 * @pipe_num: usb bam pipe number
 * @dir: direction (to/from usb bam)
 */
struct usb_host_bam_type {
	u32 pipe_num;
	u32 dir;
};

int set_disable_park_mode(u8 pipe_num, bool disable_park_mode);
int set_disable_zlt(u8 pipe_num, bool disable_zlt);
int hbm_pipe_init(u32 QH_addr, u32 pipe_num, bool is_consumer);

#endif
