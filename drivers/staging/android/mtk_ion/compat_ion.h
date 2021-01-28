/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/staging/android/mtk_ion/compat_ion.h
 *
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _LINUX_COMPAT_ION_H
#define _LINUX_COMPAT_ION_H

#if IS_ENABLED(CONFIG_COMPAT)

long compat_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#else

#define compat_ion_ioctl  NULL

#endif /* CONFIG_COMPAT */
#endif /* _LINUX_COMPAT_ION_H */
