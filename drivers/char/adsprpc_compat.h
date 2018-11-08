/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2014, 2018 The Linux Foundation. All rights reserved.
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
