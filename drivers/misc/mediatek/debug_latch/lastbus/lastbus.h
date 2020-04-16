/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _LASTBUS_H__
#define _LASTBUS_H__
#include <linux/sizes.h>
#include <linux/io.h>

#define LATCH_BUF_LENGTH SZ_4K

struct lastbus_perisys_offsets {
	unsigned int bus_peri_r0;
	unsigned int bus_peri_r1;
	unsigned int bus_peri_mon;
};

struct lastbus_infrasys_offsets {
	unsigned int bus_infra_ctrl;
	unsigned int bus_infra_mask_l;
	unsigned int bus_infra_mask_h;
	unsigned int bus_infra_snapshot;
};

struct plt_cfg_bus_latch;

struct lastbus_ops {
	int (*init)(const struct plt_cfg_bus_latch *self);
	int (*is_timeout)(const struct plt_cfg_bus_latch *self);
	int (*dump)(const struct plt_cfg_bus_latch *self, char *buf, int *wp);
	int (*set_event)(const struct plt_cfg_bus_latch *self,
		const char *buf);
	int (*get_event)(const struct plt_cfg_bus_latch *self, char *buf);
	int (*set_timeout)(const struct plt_cfg_bus_latch *self,
		const char *buf);
	int (*get_timeout)(const struct plt_cfg_bus_latch *self, char *buf);
};

struct plt_cfg_bus_latch {
	unsigned int supported;
	unsigned int num_perisys_mon;
	unsigned int num_infrasys_mon;
	unsigned int num_infra_event_reg;
	unsigned int num_peri_event_reg;
	unsigned int secure_perisys;
	unsigned int perisys_enable;
	unsigned int perisys_timeout;
	unsigned int perisys_eventmask;
	unsigned int infrasys_enable;
	unsigned int infrasys_config;
	struct lastbus_ops perisys_ops;
	struct lastbus_ops infrasys_ops;
	void __iomem *peri_base;
	void __iomem *infra_base;
	void __iomem *spm_flag_base;
	struct lastbus_perisys_offsets perisys_offsets;
	struct lastbus_infrasys_offsets infrasys_offsets;
	int (*init)(const struct plt_cfg_bus_latch *self);
};
int lastbus_setup(struct plt_cfg_bus_latch *p);
#endif
