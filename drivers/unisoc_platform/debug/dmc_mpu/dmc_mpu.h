// SPDX-License-Identifier: GPL-2.0-only
/*
 * dmc_mpu.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SPRD_DMC_MPU_H__
#define __SPRD_DMC_MPU_H__

#include <linux/device.h>
#include <linux/miscdevice.h>

struct sprd_dmpu_core;
struct sprd_dmpu_ops {
	void (*enable)(struct sprd_dmpu_core *, u32, bool);
	void (*clr_irq)(struct sprd_dmpu_core *, u32);
	void (*irq_enable)(struct sprd_dmpu_core *, u32);
	void (*irq_disable)(struct sprd_dmpu_core *, u32);
	void (*config)(struct sprd_dmpu_core *, u32, u32);
	void (*vio_cmd)(struct sprd_dmpu_core *, u32);
	void (*dump_cfg)(struct sprd_dmpu_core *, u32);
};

struct sprd_dmpu_violate {
	u32 userid;
	bool wr;
	u32 port;
	u32 id;
	u64 addr;
};

struct sprd_dmpu_chn_cfg {
	u32 en;
	u32 include;
	 /* 0x1 W_MODE, 0x2 R_MODE, RW 0x3 */
	u32 mode;
	/* Chose record id type matser id or userid */
	u32 id_type;
	u32 userid;
	u32 id_mask;
	u64 addr_start;
	u64 addr_end;
	u32 port;
};


struct sprd_dmpu_info {
	struct sprd_dmpu_core *core;
	u32 pub_id;
	int pub_irq;
	u32 dump_paddr;
	void __iomem *dump_vaddr;
	struct sprd_dmpu_violate vio;
};

struct sprd_dmpu_core {
	struct device *dev;
	struct miscdevice misc;
	u32 chn_num;
	u32 mpu_num;
	bool interleaved;
	u32 ddr_addr_offset;
	bool panic;
	const char **channel;
	u32 irq_count;
	struct sprd_dmpu_info *mpu_info;
	struct sprd_dmpu_chn_cfg *cfg;
	struct sprd_dmpu_ops *ops;
};

#define SPRD_MPU_W_MODE			0x1
#define SPRD_MPU_R_MODE			0x2

extern int sprd_dmc_mpu_register(struct platform_device *pdev,
				 struct sprd_dmpu_core *core,
				 struct sprd_dmpu_ops *ops);
extern void sprd_dmc_mpu_unregister(struct sprd_dmpu_core *core);
#endif
