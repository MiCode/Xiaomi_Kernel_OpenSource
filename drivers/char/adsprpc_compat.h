/*
 * Copyright (c) 2014,2018 The Linux Foundation. All rights reserved.
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
#ifndef ADSPRPC_COMPAT_H
#define ADSPRPC_COMPAT_H

#ifdef CONFIG_COMPAT

long compat_fastrpc_device_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg);
#else

#define compat_fastrpc_device_ioctl	NULL

#endif /* CONFIG_COMPAT */
#endif /* ADSPRPC_COMPAT_H */
