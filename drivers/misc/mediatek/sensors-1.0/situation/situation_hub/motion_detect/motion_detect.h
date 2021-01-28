/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MOTION_DETECT_H
#define MOTION_DETECT_H

#include <linux/ioctl.h>

int __init motion_detect_init(void);
void __exit motion_detect_exit(void);
#endif
