/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include "serdes.h"

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "[modem-dbg-log][%20s] "fmt,  __func__
#endif

#define CLK_SRC_MAX (5)
#define STR_CH_DISABLE "DISABLE"

#define DEBUG_LOG_PRINT pr_info

enum channel_en {
	CH_DISABLE = 0,
	CH_EN_TRAINING,
	CH_EN_FUNC_START,
	CH_EN_WTL = CH_EN_FUNC_START,
	CH_EN_MDAR,
	CH_EN_DBG_SYS,
	CH_EN_DBG_BUS,
	CH_EN_WCN,
	CH_EN_MAX
};

extern const char *ch_str[];

struct dbg_log_device;
struct serdes_drv_data;

struct dbg_log_ops {
	void (*init)(struct dbg_log_device *dbg);
	void (*exit)(struct dbg_log_device *dbg);
	void (*select)(struct dbg_log_device *dbg);
	bool (*is_freq_valid)(struct dbg_log_device *dbg, unsigned int freq);
	bool (*fill_freq_array)(struct dbg_log_device *dbg, char *sbuf);
	int (*get_valid_channel)(struct dbg_log_device *dbg, const char *buf);
};

struct phy_ctx {
	unsigned long base;
	unsigned int freq;
	unsigned int lanes;
	u32 clk_sel;
	u32 div1_map[CLK_SRC_MAX];
	struct regmap *dsi_apb;
	struct regmap *pll_apb;
	struct regmap *mm_ahb;
	struct regmap *pmu_apb;
};

struct dbg_log_device {
	struct device dev;
	struct phy_ctx *phy;
	struct dbg_log_ops *ops;
	u32 channel;
	bool is_inited;
	bool mm;
	int serdes_id;
	struct regmap *aon_apb;
	struct serdes_drv_data serdes;
	struct clk *clk_serdes_eb;
	struct clk *clk_serdes1_eb;
	struct clk *clk_mm_eb;
	struct clk *clk_ana_eb;
	struct clk *clk_dphy_cfg_eb;
	struct clk *clk_dphy_ref_eb;
	struct clk *clk_dsi_csi_test_eb;
	struct clk *clk_dsi_ref_eb;
	struct clk *clk_cphy_cfg_eb;
	struct clk *clk_dsi_cfg_eb;
	struct clk *clk_cgm_cphy_cfg_en;

	struct clk *clk_src[CLK_SRC_MAX];
};

static inline void reg_write(unsigned long reg, u32 val)
{
	writel(val, (void __iomem *)reg);
}

static inline u32 reg_read(unsigned long reg)
{
	return readl((void __iomem *)reg);
}

static inline void reg_bits_set(unsigned long reg, u32 bits)
{
	reg_write(reg, reg_read(reg) | bits);
}

static inline void reg_bits_clr(unsigned long reg, u32 bits)
{
	reg_write(reg, reg_read(reg) & ~bits);
}

void dbg_log_channel_sel(struct dbg_log_device *dbg);
bool dbg_log_is_freq_valid(struct dbg_log_device *dbg, unsigned int freq);
int dbg_log_get_valid_channel(struct dbg_log_device *dbg, const char *buf);
bool dbg_log_fill_freq_array(struct dbg_log_device *dbg, char *sbuf);
struct dbg_log_device *dbg_log_device_register(struct device *parent,
					       struct dbg_log_ops *ops,
					       struct phy_ctx *phy,
					       const char *serdes_name);
int dbg_log_sysfs_init(struct device *dev);

#endif
