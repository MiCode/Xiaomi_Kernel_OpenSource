/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef UNCALI_GYROHUB_H
#define UNCALI_GYROHUB_H

#include <linux/ioctl.h>

int __init uncali_gyrohub_init(void);
void __exit uncali_gyrohub_exit(void);

#endif
