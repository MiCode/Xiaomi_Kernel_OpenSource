/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef __SPK_ID_H_
#define __SPK_ID_H_

#include <linux/types.h>
#include <linux/of.h>

#define PIN_PULL_DOWN		0
#define PIN_PULL_UP		1
#define PIN_FLOAT		2

#define VENDOR_ID_NONE		0
#define VENDOR_ID_AAC		1
#define VENDOR_ID_SSI		2
#define VENDOR_ID_GOER		3

#define VENDOR_ID_UNKNOWN	4

extern int spk_id_get_pin_3state(struct device_node *np);

#endif
