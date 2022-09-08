/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */


#ifndef __DRV_CLK_FHCTL_H
#define __DRV_CLK_FHCTL_H

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/bitops.h>


/************************************************
 **********      register field        **********
 ************************************************/
// REG_CFG mask
#define MASK_FRDDSX_DYS         GENMASK(23, 20)
#define MASK_FRDDSX_DTS         GENMASK(19, 16)
#define FH_FHCTLX_CFG_PAUSE     BIT(4)
#define FH_SFSTRX_EN            BIT(2)
#define FH_FRDDSX_EN            BIT(1)
#define FH_FHCTLX_EN            BIT(0)

//REG_DDS mask
#define FH_FHCTLX_PLL_TGL_ORG   BIT(31)

//FHCTLX_DVFS mask
#define FH_FHCTLX_PLL_DVFS_TRI  BIT(31)

//XXPLL_CON1 mask (APMIXED)
#define FH_XXPLL_CON1_PCWCHG    BIT(31)

// Default DT/DF valude
#define REG_CFG_DT_VAL 0x0
#define REG_CFG_DF_VAL 0x9

#define REG_HP_EN_APMIXEDSYS_CTR 0
#define REG_HP_EN_FHCTL_CTR 1

#define fh_set_field(reg, field, val) \
do { \
	unsigned int tv = readl(reg); \
	tv &= ~(field); \
	tv |= ((val) << (ffs(field) - 1)); \
	writel(tv, reg); \
} while (0)

#define fh_get_field(reg, field, val) \
do { \
	unsigned int tv = readl(reg); \
	val = ((tv & (field)) >> (ffs(field) - 1)); \
} while (0)


struct clk_mt_fhctl_regs {
	/* Common reg */
	void __iomem *reg_unitslope_en;
	void __iomem *reg_hp_en;
	void __iomem *reg_clk_con;
	void __iomem *reg_rst_con;
	void __iomem *reg_slope0;
	void __iomem *reg_slope1;

	/* For PLL specific */
	void __iomem *reg_cfg;
	void __iomem *reg_updnlmt;
	void __iomem *reg_dds;
	void __iomem *reg_dvfs;
	void __iomem *reg_mon;
	void __iomem *reg_con0;
	void __iomem *reg_con1;
};

struct clk_mt_fhctl_pll_data {
	const char *pll_name;
	int pll_id;
	int pll_type;
	int pll_default_ssc_rate;
	unsigned int slope0_value;
	unsigned int slope1_value;
	unsigned int dds_mask;
	unsigned int *hp_tbl;
	unsigned int hp_tbl_size;
	unsigned int dds_max;  // for UT dds max value.
	unsigned int dds_min;  // for UT dds min value.
};

struct clk_mt_fhctl {
	struct clk_mt_fhctl_pll_data *pll_data;
	struct clk_mt_fhctl_regs *fh_regs;
	const struct clk_mt_fhctl_hal_ops *hal_ops;
	spinlock_t *lock;
	struct list_head node;
};

struct clk_mt_fhctl_hal_ops {
	int (*pll_init)(struct clk_mt_fhctl *fh);
	int (*pll_unpause)(struct clk_mt_fhctl *fh);
	int (*pll_pause)(struct clk_mt_fhctl *fh);
	int (*pll_ssc_disable)(struct clk_mt_fhctl *fh);
	int (*pll_ssc_enable)(struct clk_mt_fhctl *fh, int rate);
	int (*pll_hopping)(struct clk_mt_fhctl *fh,
					unsigned int new_dds, int postdiv);
};


struct mtk_fhctl {
	struct device *dev;

	/* set in fhctl probe */
	void __iomem *fhctl_base;
	void __iomem *apmixed_base;
	unsigned int pll_num;
	int *idmap;

	struct clk_mt_fhctl **fh_tbl;
	const struct mtk_fhctl_compatible *dev_comp;
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs_root;
#endif
};

struct mtk_fhctl_compatible {
	const u16 *common_regs;
	const u16 *pll_regs;
	const u16 *pll_con0_regs;
	const char * const *pll_names;
	unsigned int pll_num;
	unsigned int pll_dds_reg_field_size;
	unsigned int pll_slope0_reg_setting;
	unsigned int pll_slope1_reg_setting;
};

enum FHCTL_COMMON_REGS_OFFSET {
	OFFSET_UNITSLOPE_EN,
	OFFSET_HP_EN,
	OFFSET_CLK_CON,
	OFFSET_RST_CON,
	OFFSET_SLOPE0,
	OFFSET_SLOPE1,
	OFFSET_FHCTL_DSSC_CFG,
};

enum FHCTL_PLL_REGS_OFFSET {
	OFFSET_CFG,
	OFFSET_UPDNLMT,
	OFFSET_DDS,
	OFFSET_DVFS,
	OFFSET_MON,
	OFFSET_CON0,
	OFFSET_CON1,
};

enum FHCTL_PLL_TYPE {
	FH_PLL_TYPE_NOT_SUPPORT,
	FH_PLL_TYPE_GENERAL,
	FH_PLL_TYPE_CPU,
	FH_PLL_TYPE_FORCE,
};


struct clk_mt_fhctl *mtk_fh_get_fh_obj_tbl(struct mtk_fhctl *fhctl, int posi);
extern const struct clk_mt_fhctl_hal_ops mt_fhctl_hal_ops;


#endif /* __DRV_CLK_FHCTL_H */

