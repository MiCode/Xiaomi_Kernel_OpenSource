/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __EDMC_IOCTL_H__
#define __EDMC_IOCTL_H__


#include <linux/ioctl.h>
#include <linux/types.h>
#include "edmc_debug.h"

struct ioctl_edmc_descript {
	__u32 src_tile_width;
	__u32 dst_tile_width;
	__u32 tile_height;
	__u32 src_stride;
	__u32 dst_stride;
	__u32 src_addr;
	__u32 dst_addr;
	__u8  user_desp_id;
	__u8  despcript_id;
	__u32 fill_value;
	__u32 range_scale;
	__u32 min_fp32;
	__u64 wait_id;
};

#define IOC_EDMC ('\x1c')

#define IOCTL_EDMC_COPY        _IOWR(IOC_EDMC, 0, struct ioctl_edmc_descript)
#define IOCTL_EDMC_FILL        _IOWR(IOC_EDMC, 1, struct ioctl_edmc_descript)
#define IOCTL_EDMC_RGBTORGBA   _IOWR(IOC_EDMC, 2, struct ioctl_edmc_descript)
#define IOCTL_EDMC_FP32TOFIX8  _IOWR(IOC_EDMC, 3, struct ioctl_edmc_descript)
#define IOCTL_EDMC_FIX8TOFP32  _IOWR(IOC_EDMC, 4, struct ioctl_edmc_descript)
#define IOCTL_EDMC_STATUS      _IOWR(IOC_EDMC, 5, int)
#define IOCTL_EDMC_RESET       _IOWR(IOC_EDMC, 6, int)
#define IOCTL_EDMC_WAIT        _IOWR(IOC_EDMC, 7, uint64_t)
//#ifdef ERROR_TRIGGER_TEST
//#define IOCTL_EDMC_TEST_TRIGGER_ERROR       _IOW(IOC_EDMC, 8, int)
//#endif

#endif
