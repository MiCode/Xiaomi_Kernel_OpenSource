/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DEV_DEP_H
#define _DEV_DEP_H

struct dolby_param_data {
	int32_t version;
	int32_t device_id;
	int32_t be_id;
	int32_t param_id;
	int32_t length;
	int32_t __user *data;
};

struct dolby_param_license {
	int32_t dmid;
	int32_t license_key;
};

#define SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM\
		_IOWR('U', 0x10, struct dolby_param_data)
#define SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM\
		_IOR('U', 0x11, struct dolby_param_data)
#define SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND\
		_IOWR('U', 0x13, struct dolby_param_data)
#define SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE\
		_IOWR('U', 0x14, struct dolby_param_license)
#define SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER\
		_IOR('U', 0x15, struct dolby_param_data)
#endif
