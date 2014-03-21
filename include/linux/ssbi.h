/* Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2011,2013-2014 The Linux Foundation. All rights reserved.
 * Author: Dima Zavin <dima@android.com>
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

#ifndef _LINUX_SSBI_H
#define _LINUX_SSBI_H

#include <linux/types.h>

struct ssbi_slave_info {
	const char	*name;
	void		*platform_data;
};

enum ssbi_controller_type {
	MSM_SBI_CTRL_SSBI = 0,
	MSM_SBI_CTRL_SSBI2,
	MSM_SBI_CTRL_PMIC_ARBITER,
	FSM_SBI_CTRL_GENI_SSBI_ARBITER,
	FSM_SBI_CTRL_GENI_SSBI2_ARBITER,
};

struct ssbi_platform_data {
	struct ssbi_slave_info	slave;
	enum ssbi_controller_type controller_type;
};

struct ssbi {
	struct device           *slave;
	void __iomem            *base;
	spinlock_t              lock;
	enum ssbi_controller_type controller_type;
	int (*read)(struct ssbi *, u16 addr, u8 *buf, int len);
	int (*write)(struct ssbi *, u16 addr, u8 *buf, int len);
};

int ssbi_write(struct device *dev, u16 addr, u8 *buf, int len);
int ssbi_read(struct device *dev, u16 addr, u8 *buf, int len);
void  set_ssbi_mode_1(void __iomem *geni_offset);
void  set_ssbi_mode_2(void __iomem *geni_offset);
#endif
