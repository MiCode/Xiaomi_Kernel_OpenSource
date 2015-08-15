/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _CALIB_HWDEP_H
#define _CALIB_HWDEP_H

#define WCD9XXX_CODEC_HWDEP_NODE    1000
enum wcd_cal_type {
	WCD9XXX_MIN_CAL,
	WCD9XXX_ANC_CAL = WCD9XXX_MIN_CAL,
	WCD9XXX_MAD_CAL,
	WCD9XXX_MBHC_CAL,
	WCD9XXX_VBAT_CAL,
	WCD9XXX_MAX_CAL,
};

struct wcdcal_ioctl_buffer {
	__u32 size;
	__u8 __user *buffer;
	enum wcd_cal_type cal_type;
};

#define SNDRV_CTL_IOCTL_HWDEP_CAL_TYPE \
	_IOW('U', 0x1, struct wcdcal_ioctl_buffer)

#endif /*_CALIB_HWDEP_H*/
