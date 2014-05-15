
/*
 *  sst_app_compat_interface.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2013-14 Intel Corp
 *  Authors: Subhransu S. Prusty <subhransu.s.prusty@intel.com>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This driver exposes the audio engine functionalities to the ALSA
 *	and middleware.
 */

/* This file is included from sst.c */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/compat.h>
#include <linux/types.h>
#include <sound/intel_sst_ioctl.h>
#include "sst.h"

struct snd_ppp_params32 {
	__u8			algo_id;/* Post/Pre processing algorithm ID  */
	__u8			str_id;	/*Only 5 bits used 0 - 31 are valid*/
	__u8			enable;	/* 0= disable, 1= enable*/
	__u8			operation;
	__u32			size;	/*Size of parameters for all blocks*/
	__u32			params;
} __packed;

enum {
SNDRV_SST_SET_ALGO32 = _IOW('L', 0x30,  struct snd_ppp_params32),
SNDRV_SST_GET_ALGO32 = _IOWR('L', 0x31,  struct snd_ppp_params32),
};

static long sst_algo_compat(unsigned int cmd,
				struct snd_ppp_params32 __user *arg32)
{
	int retval = 0;
	struct snd_ppp_params32 algo_params32;
	struct snd_ppp_params algo_params;

	if (copy_from_user(&algo_params32, arg32, sizeof(algo_params32))) {
		pr_debug("%s: copy from user failed: %d\n", __func__, retval);
		return -EINVAL;
	}

	memcpy(&algo_params, &algo_params32, sizeof(algo_params32)-sizeof(__u32));
	algo_params.params = compat_ptr(algo_params32.params);
	retval = intel_sst_ioctl_dsp(cmd, &algo_params, (unsigned long)arg32);
	return retval;
}

static long intel_sst_ioctl_compat(struct file *file_ptr,
				unsigned int cmd, unsigned long arg)
{
	void __user *argp = compat_ptr(arg);

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(SNDRV_SST_DRIVER_INFO):
	case _IOC_NR(SNDRV_SST_TUNING_PARAMS):
		return intel_sst_ioctl(file_ptr, cmd, (unsigned long)argp);
	case _IOC_NR(SNDRV_SST_SET_ALGO32):
		return sst_algo_compat(SNDRV_SST_SET_ALGO, argp);
	case _IOC_NR(SNDRV_SST_GET_ALGO32):
		return sst_algo_compat(SNDRV_SST_GET_ALGO, argp);

	default:
		return -ENOTTY;
	}
	return 0;
}
