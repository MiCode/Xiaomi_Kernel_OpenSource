/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef DEVICE_ORIENTATION_H
#define DEVICE_ORIENTATION_H

#include <linux/ioctl.h>

int __init device_orientation_init(void);
void __exit device_orientation_exit(void);

#endif
