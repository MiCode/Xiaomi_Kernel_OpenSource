/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef WAKEHUB_H
#define WAKEHUB_H

#include <linux/ioctl.h>

int __init wakehub_init(void);
void __exit wakehub_exit(void);

#endif
