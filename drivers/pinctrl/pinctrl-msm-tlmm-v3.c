/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pinctrl/pinconf-generic.h>
#include "pinctrl-msm.h"

/* config translations */
#define drv_str_to_rval(drv)	((drv >> 1) - 1)
#define rval_to_drv_str(val)	((val + 1) << 1)
#define dir_to_inout_val(dir)	(dir << 1)
#define inout_val_to_dir(val)	(val >> 1)
#define rval_to_pull(val)	((val > 2) ? 1 : val)
#define TLMMV3_NO_PULL		0
#define TLMMV3_PULL_DOWN	1
#define TLMMV3_PULL_UP		3
/* GP PIN TYPE REG MASKS */
#define TLMMV3_GP_DRV_SHFT		6
#define TLMMV3_GP_DRV_MASK		0x7
#define TLMMV3_GP_PULL_SHFT		0
#define TLMMV3_GP_PULL_MASK		0x3
#define TLMMV3_GP_DIR_SHFT		9
#define TLMMV3_GP_DIR_MASK		1
#define TLMMV3_GP_FUNC_SHFT		2
#define TLMMV3_GP_FUNC_MASK		0xF
/* SDC1 PIN TYPE REG MASKS */
#define TLMMV3_SDC1_CLK_DRV_SHFT	6
#define TLMMV3_SDC1_CLK_DRV_MASK	0x7
#define TLMMV3_SDC1_DATA_DRV_SHFT	0
#define TLMMV3_SDC1_DATA_DRV_MASK	0x7
#define TLMMV3_SDC1_CMD_DRV_SHFT	3
#define TLMMV3_SDC1_CMD_DRV_MASK	0x7
#define TLMMV3_SDC1_CLK_PULL_SHFT	13
#define TLMMV3_SDC1_CLK_PULL_MASK	0x3
#define TLMMV3_SDC1_DATA_PULL_SHFT	9
#define TLMMV3_SDC1_DATA_PULL_MASK	0x3
#define TLMMV3_SDC1_CMD_PULL_SHFT	11
#define TLMMV3_SDC1_CMD_PULL_MASK	0x3
/* SDC2 PIN TYPE REG MASKS */
#define TLMMV3_SDC2_CLK_DRV_SHFT	6
#define TLMMV3_SDC2_CLK_DRV_MASK	0x7
#define TLMMV3_SDC2_DATA_DRV_SHFT	0
#define TLMMV3_SDC2_DATA_DRV_MASK	0x7
#define TLMMV3_SDC2_CMD_DRV_SHFT	3
#define TLMMV3_SDC2_CMD_DRV_MASK	0x7
#define TLMMV3_SDC2_CLK_PULL_SHFT	14
#define TLMMV3_SDC2_CLK_PULL_MASK	0x3
#define TLMMV3_SDC2_DATA_PULL_SHFT	9
#define TLMMV3_SDC2_DATA_PULL_MASK	0x3
#define TLMMV3_SDC2_CMD_PULL_SHFT	11
#define TLMMV3_SDC2_CMD_PULL_MASK	0x3

#define TLMMV3_GP_INOUT_BIT		1
#define TLMMV3_GP_OUT			BIT(TLMMV3_GP_INOUT_BIT)
#define TLMMV3_GP_IN			0

/* SDC Pin type register offsets */
#define TLMMV3_SDC_OFFSET		0x2044
#define TLMMV3_SDC1_CFG(base)		(base)
#define TLMMV3_SDC2_CFG(base)		(TLMMV3_SDC1_CFG(base) + 0x4)

/* GP pin type register offsets */
#define TLMMV3_GP_CFG(base, pin)	(base + 0x1000 + 0x10 * (pin))
#define TLMMV3_GP_INOUT(base, pin)	(base + 0x1004 + 0x10 * (pin))

struct msm_sdc_regs {
	unsigned int offset;
	unsigned long pull_mask;
	unsigned long pull_shft;
	unsigned long drv_mask;
	unsigned long drv_shft;
};

static struct msm_sdc_regs sdc_regs[] = {
	/* SDC1 CLK */
	{
		.offset = 0,
		.pull_mask = TLMMV3_SDC1_CLK_PULL_MASK,
		.pull_shft = TLMMV3_SDC1_CLK_PULL_SHFT,
		.drv_mask = TLMMV3_SDC1_CLK_DRV_MASK,
		.drv_shft = TLMMV3_SDC1_CLK_DRV_SHFT,
	},
	/* SDC1 CMD */
	{
		.offset = 0,
		.pull_mask = TLMMV3_SDC1_CMD_PULL_MASK,
		.pull_shft = TLMMV3_SDC1_CMD_PULL_SHFT,
		.drv_mask = TLMMV3_SDC1_CMD_DRV_MASK,
		.drv_shft = TLMMV3_SDC1_CMD_DRV_SHFT,
	},
	/* SDC1 DATA */
	{
		.offset = 0,
		.pull_mask = TLMMV3_SDC1_DATA_PULL_MASK,
		.pull_shft = TLMMV3_SDC1_DATA_PULL_SHFT,
		.drv_mask = TLMMV3_SDC1_DATA_DRV_MASK,
		.drv_shft = TLMMV3_SDC1_DATA_DRV_SHFT,
	},
	/* SDC2 CLK */
	{
		.offset = 0x4,
		.pull_mask = TLMMV3_SDC2_CLK_PULL_MASK,
		.pull_shft = TLMMV3_SDC2_CLK_PULL_SHFT,
		.drv_mask = TLMMV3_SDC2_CLK_DRV_MASK,
		.drv_shft = TLMMV3_SDC2_CLK_DRV_SHFT,
	},
	/* SDC2 CMD */
	{
		.offset = 0x4,
		.pull_mask = TLMMV3_SDC2_CMD_PULL_MASK,
		.pull_shft = TLMMV3_SDC2_CMD_PULL_SHFT,
		.drv_mask = TLMMV3_SDC2_CMD_DRV_MASK,
		.drv_shft = TLMMV3_SDC2_CMD_DRV_SHFT,
	},
	/* SDC2 DATA */
	{
		.offset = 0x4,
		.pull_mask = TLMMV3_SDC2_DATA_PULL_MASK,
		.pull_shft = TLMMV3_SDC2_DATA_PULL_SHFT,
		.drv_mask = TLMMV3_SDC2_DATA_DRV_MASK,
		.drv_shft = TLMMV3_SDC2_DATA_DRV_SHFT,
	},
};

static int msm_tlmm_v3_sdc_cfg(uint pin_no, unsigned long *config,
						void __iomem *reg_base,
						bool write)
{
	unsigned int val, id, data;
	u32 mask, shft;
	void __iomem *cfg_reg;

	cfg_reg = reg_base + sdc_regs[pin_no].offset;
	id = pinconf_to_config_param(*config);
	val = readl_relaxed(cfg_reg);
	/* Get mask and shft values for this config type */
	switch (id) {
	case PIN_CONFIG_BIAS_DISABLE:
		mask = sdc_regs[pin_no].pull_mask;
		shft = sdc_regs[pin_no].pull_shft;
		data = TLMMV3_NO_PULL;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		mask = sdc_regs[pin_no].pull_mask;
		shft = sdc_regs[pin_no].pull_shft;
		data = TLMMV3_PULL_DOWN;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		mask = sdc_regs[pin_no].pull_mask;
		shft = sdc_regs[pin_no].pull_shft;
		data = TLMMV3_PULL_UP;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		mask = sdc_regs[pin_no].drv_mask;
		shft = sdc_regs[pin_no].drv_shft;
		if (write) {
			data = pinconf_to_config_argument(*config);
			data = drv_str_to_rval(data);
		} else {
			val >>= shft;
			val &= mask;
			data = rval_to_drv_str(val);
		}
		break;
	default:
		return -EINVAL;
	};

	if (write) {
		val &= ~(mask << shft);
		val |= (data << shft);
		writel_relaxed(val, cfg_reg);
	} else
		*config = pinconf_to_config_packed(id, data);
	return 0;
}

static void msm_tlmm_v3_sdc_set_reg_base(void __iomem **ptype_base,
							void __iomem *tlmm_base)
{
	*ptype_base = tlmm_base + TLMMV3_SDC_OFFSET;
}

static int msm_tlmm_v3_gp_cfg(uint pin_no, unsigned long *config,
						void *reg_base, bool write)
{
	unsigned int val, id, data, inout_val;
	u32 mask = 0, shft = 0;
	void __iomem *inout_reg = NULL;
	void __iomem *cfg_reg = TLMMV3_GP_CFG(reg_base, pin_no);

	id = pinconf_to_config_param(*config);
	val = readl_relaxed(cfg_reg);
	/* Get mask and shft values for this config type */
	switch (id) {
	case PIN_CONFIG_BIAS_DISABLE:
		mask = TLMMV3_GP_PULL_MASK;
		shft = TLMMV3_GP_PULL_SHFT;
		data = TLMMV3_NO_PULL;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		mask = TLMMV3_GP_PULL_MASK;
		shft = TLMMV3_GP_PULL_SHFT;
		data = TLMMV3_PULL_DOWN;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		mask = TLMMV3_GP_PULL_MASK;
		shft = TLMMV3_GP_PULL_SHFT;
		data = TLMMV3_PULL_UP;
		if (!write) {
			val >>= shft;
			val &= mask;
			data = rval_to_pull(val);
		}
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		mask = TLMMV3_GP_DRV_MASK;
		shft = TLMMV3_GP_DRV_SHFT;
		if (write) {
			data = pinconf_to_config_argument(*config);
			data = drv_str_to_rval(data);
		} else {
			val >>= shft;
			val &= mask;
			data = rval_to_drv_str(val);
		}
		break;
	case PIN_CONFIG_OUTPUT:
		mask = TLMMV3_GP_DIR_MASK;
		shft = TLMMV3_GP_DIR_SHFT;
		inout_reg = TLMMV3_GP_INOUT(reg_base, pin_no);
		if (write) {
			data = pinconf_to_config_argument(*config);
			inout_val = dir_to_inout_val(data);
			writel_relaxed(inout_val, inout_reg);
			data = (mask << shft);
		} else {
			inout_val = readl_relaxed(inout_reg);
			data = inout_val_to_dir(inout_val);
		}
		break;
	default:
		return -EINVAL;
	};

	if (write) {
		val &= ~(mask << shft);
		val |= (data << shft);
		writel_relaxed(val, cfg_reg);
	} else
		*config = pinconf_to_config_packed(id, data);
	return 0;
}

static void msm_tlmm_v3_gp_fn(uint pin_no, u32 func, void *reg_base,
								bool enable)
{
	unsigned int val;
	void __iomem *cfg_reg = TLMMV3_GP_CFG(reg_base, pin_no);
	val = readl_relaxed(cfg_reg);
	val &= ~(TLMMV3_GP_FUNC_MASK << TLMMV3_GP_FUNC_SHFT);
	if (enable)
		val |= (func << TLMMV3_GP_FUNC_SHFT);
	writel_relaxed(val, cfg_reg);
}

static void msm_tlmm_v3_gp_set_reg_base(void __iomem **ptype_base,
						void __iomem *tlmm_base)
{
	*ptype_base = tlmm_base;
}

static struct msm_pintype_info tlmm_v3_pininfo[] = {
	{
		.prg_cfg = msm_tlmm_v3_gp_cfg,
		.prg_func = msm_tlmm_v3_gp_fn,
		.set_reg_base = msm_tlmm_v3_gp_set_reg_base,
		.reg_base = NULL,
		.prop_name = "qcom,pin-type-gp",
		.name = "gp",
	},
	{
		.prg_cfg = msm_tlmm_v3_sdc_cfg,
		.set_reg_base = msm_tlmm_v3_sdc_set_reg_base,
		.reg_base = NULL,
		.prop_name = "qcom,pin-type-sdc",
		.name = "sdc",
	}
};

struct msm_tlmm_pintype tlmm_v3_pintypes = {
	.num_entries = ARRAY_SIZE(tlmm_v3_pininfo),
	.pintype_info = tlmm_v3_pininfo,
};

