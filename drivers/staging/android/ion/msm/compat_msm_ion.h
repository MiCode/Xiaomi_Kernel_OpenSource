/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_COMPAT_ION_H
#define _LINUX_COMPAT_ION_H

#include <linux/ion.h>

#if IS_ENABLED(CONFIG_COMPAT)

long compat_msm_ion_ioctl(struct ion_client *client, unsigned int cmd,
					unsigned long arg);

#define compat_ion_user_handle_t compat_int_t

#else

#define compat_msm_ion_ioctl  msm_ion_custom_ioctl

#endif
#endif
