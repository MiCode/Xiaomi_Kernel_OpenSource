/* SPDX-License-Identifier: GPL-2.0
 * File:shub_opcode.h
 * Author:bao.yue@spreadtrum.com
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */

#ifndef SPRD_SHUB_OPCODE_H
#define SPRD_SHUB_OPCODE_H

#include <linux/firmware.h>

#define SHUB_SENSOR_NAME   "sprd-shub"
#define SHUB_SENSOR_NAME_LENGTH 10

struct iic_unit {
	/* read/write/msleep */
	u8 addr;		/* 0xFF means delay */
	u8 val;
	u8 shift;
	u8 mask;
};

struct fwshub_unit {
	u32 cmd;
	u32 shift;
	u32 units;
};

struct fwshub_head {
	char name[SHUB_SENSOR_NAME_LENGTH];
	int type;
	struct fwshub_unit index_opcode[8];
};
#endif
