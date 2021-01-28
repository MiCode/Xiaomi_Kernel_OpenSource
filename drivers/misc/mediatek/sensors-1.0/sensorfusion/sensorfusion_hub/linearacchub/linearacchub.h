/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef LINEARACC_H
#define LINEARACC_H

#include <linux/ioctl.h>

int __init linearacchub_init(void);
void __exit linearacchub_exit(void);

#endif
