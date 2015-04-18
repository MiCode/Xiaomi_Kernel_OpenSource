/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef _SWR_WCD_CTRL_H
#define _SWR_WCD_CTRL_H
#include <linux/module.h>

#define SWR_MAX_ROW		48
#define SWR_MAX_COL		16
#define SWR_MIN_COL		2

#define SWR_WCD_NAME	"swr-wcd"

enum {
	SWR_IRQ_FREE,
	SWR_IRQ_REGISTER,
};

struct usecase {
	u8 num_port;
	u8 num_ch;
	u32 chrate;
};

struct port_params {
	u8 si;
	u8 off1;
	u8 off2;
};

struct swr_ctrl_platform_data {
	void *handle; /* holds priv data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*clk)(void *handle, bool enable);
	int (*reg_irq)(void *handle, irqreturn_t(*irq_handler)(int irq,
			void *data), void *swr_handle, int type);
};

struct swr_mstr_ctrl {
	struct swr_master master;
	struct device *dev;
	struct resource *supplies;
	struct clk *mclk;
	struct completion reset;
	struct completion broadcast;
	struct mutex mlock;
	u8 rcmd_id;
	u8 wcmd_id;
	void *handle; /* SWR Master handle from client for read and writes */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*clk)(void *handle, bool enable);
	int (*reg_irq)(void *handle, irqreturn_t(*irq_handler)(int irq,
			void *data), void *swr_handle, int type);
	int irq;
	int num_enum_slaves;
	int slave_status;
};

#endif /* _SWR_WCD_CTRL_H */
