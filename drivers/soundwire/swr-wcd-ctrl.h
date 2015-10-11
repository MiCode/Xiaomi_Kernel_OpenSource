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
#include <linux/soundwire/swr-wcd.h>

#define SWR_MAX_ROW		0 /* Rows = 48 */
#define SWR_MAX_COL		7 /* Cols = 16 */
#define SWR_MIN_COL		0 /* Cols = 2 */

#define SWR_WCD_NAME	"swr-wcd"

#define SWR_MSTR_PORT_LEN	8 /* Number of master ports */

enum {
	SWR_MSTR_PAUSE,
	SWR_MSTR_RESUME,
	SWR_MSTR_UP,
	SWR_MSTR_DOWN,
};

enum {
	SWR_IRQ_FREE,
	SWR_IRQ_REGISTER,
};

enum {
	SWR_DAC_PORT,
	SWR_COMP_PORT,
	SWR_BOOST_PORT,
	SWR_VISENSE_PORT,
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

struct swrm_mports {
	struct list_head list;
	u8 id;
};

struct swr_ctrl_platform_data {
	void *handle; /* holds priv data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
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
	struct mutex reslock;
	u8 rcmd_id;
	u8 wcmd_id;
	void *handle; /* SWR Master handle from client for read and writes */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*reg_irq)(void *handle, irqreturn_t(*irq_handler)(int irq,
			void *data), void *swr_handle, int type);
	int irq;
	int num_enum_slaves;
	int slave_status;
	struct list_head mport_list;
	struct swr_mstr_port *mstr_port;
	int state;
	struct platform_device *pdev;
};

#endif /* _SWR_WCD_CTRL_H */
