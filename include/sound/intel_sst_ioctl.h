#ifndef __INTEL_SST_IOCTL_H__
#define __INTEL_SST_IOCTL_H__
/*
 *  intel_sst_ioctl.h - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Harsha Priya <priya.harsha@intel.com>
 *		Dharageswari R <dharageswari.r@intel.com>
 *		KP Jeeja <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This file defines all sst ioctls
 */

/* codec and post/pre processing related info */

#include <linux/types.h>

/* Pre and post processing params structure */
struct snd_ppp_params {
	__u8			algo_id;/* Post/Pre processing algorithm ID  */
	__u8			str_id;	/*Only 5 bits used 0 - 31 are valid*/
	__u8			enable;	/* 0= disable, 1= enable*/
	__u8			operation; /* 0 = set_algo, 1 = get_algo */
	__u32			size;	/*Size of parameters for all blocks*/
	void			*params;
} __packed;

struct snd_sst_driver_info {
	__u32 max_streams;
};

struct snd_sst_tuning_params {
	__u8 type;
	__u8 str_id;
	__u8 size;
	__u8 rsvd;
	__u64 addr;
} __packed;

/*IOCTL defined here */
/*SST common ioctls */
#define SNDRV_SST_DRIVER_INFO	_IOR('L', 0x10, struct snd_sst_driver_info)
#define SNDRV_SST_SET_ALGO	_IOW('L', 0x30,  struct snd_ppp_params)
#define SNDRV_SST_GET_ALGO	_IOWR('L', 0x31,  struct snd_ppp_params)
#define SNDRV_SST_TUNING_PARAMS	_IOW('L', 0x32,  struct snd_sst_tuning_params)
#endif /* __INTEL_SST_IOCTL_H__ */
