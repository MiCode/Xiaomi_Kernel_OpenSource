/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "GFX_LDO: %s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define LDO_ATEST_REG			0x0
#define LDO_CFG0_REG			0x4
#define LDO_CFG1_REG			0x8
#define LDO_CFG2_REG			0xC

#define LDO_VREF_TEST_CFG		0x14
#define ENABLE_LDO_STATUS_BIT		(BIT(8) | BIT(12))
#define LDO_AUTOBYPASS_BIT		BIT(20)

#define LDO_VREF_SET_REG		0x18
#define UPDATE_VREF_BIT			BIT(31)
#define SEL_RST_BIT			BIT(16)
#define VREF_VAL_MASK			GENMASK(6 , 0)

#define PWRSWITCH_CTRL_REG		0x1C
#define LDO_CLAMP_IO_BIT		BIT(31)
#define CPR_BYPASS_IN_LDO_MODE_BIT	BIT(30)
#define EN_LDOAP_CTRL_CPR_BIT		BIT(29)
#define PWR_SRC_SEL_BIT			BIT(9)
#define ACK_SW_OVR_BIT			BIT(8)
#define LDO_PREON_SW_OVR_BIT		BIT(7)
#define LDO_BYPASS_BIT			BIT(6)
#define LDO_PDN_BIT			BIT(5)
#define LDO_UNDER_SW_CTRL_BIT		BIT(4)
#define BHS_EN_REST_BIT			BIT(2)
#define BHS_EN_FEW_BIT			BIT(1)
#define BHS_UNDER_SW_CTL		BIT(0)

#define LDO_STATUS1_REG			0x24

#define PWRSWITCH_STATUS_REG		0x28
#define LDO_VREF_SETTLED_BIT		BIT(4)
#define LDO_READY_BIT			BIT(2)
#define BHS_EN_REST_ACK_BIT		BIT(1)

#define MIN_LDO_VOLTAGE			375000
#define MAX_LDO_VOLTAGE			980000
#define LDO_STEP_VOLATGE		5000

#define MAX_LDO_REGS			11

#define BYTES_PER_FUSE_ROW		8
#define MAX_FUSE_ROW_BIT		63
#define MIN_CORNER_OFFSET		1

#define GFX_LDO_FUSE_STEP_VOLT		10000
#define GFX_LDO_FUSE_SIZE		5

enum regulator_mode {
	LDO,
	BHS,
};

enum direction {
	NO_CHANGE,
	UP,
	DOWN,
};

struct fuse_param {
	unsigned		row;
	unsigned		bit_start;
	unsigned		bit_end;
};

struct ldo_config {
	u32 offset;
	u32 value;
};

struct msm_gfx_ldo {
	struct device		*dev;
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct regulator	*vdd_cx;
	struct regulator	*mem_acc_vreg;
	struct dentry		*debugfs;

	u32			num_corners;
	u32			num_ldo_corners;
	u32			*open_loop_volt;
	u32			*ceiling_volt;
	u32			*floor_volt;
	u32			*ldo_corner_en_map;
	u32			*vdd_cx_corner_map;
	u32			*mem_acc_corner_map;
	const int		*ref_volt;
	const struct fuse_param	*ldo_enable_param;
	const struct fuse_param	**init_volt_param;
	bool			ldo_fuse_enable;
	bool			ldo_mode_disable;
	struct ldo_config	*ldo_init_config;

	void __iomem		*efuse_base;
	phys_addr_t		efuse_addr;
	void __iomem		*ldo_base;
	phys_addr_t		ldo_addr;

	bool			vreg_enabled;
	enum regulator_mode	mode;
	u32			corner;
	int			ldo_voltage_uv;
	struct mutex		ldo_mutex;
};

#define MSM8953_LDO_FUSE_CORNERS		3
#define LDO_MAX_OFFSET				0xFFFF
static struct ldo_config msm8953_ldo_config[] = {
	{LDO_ATEST_REG,		0x00000203},
	{LDO_CFG0_REG,		0x05008600},
	{LDO_CFG1_REG,		       0x0},
	{LDO_CFG2_REG,		0x0000C3FC},
	{LDO_VREF_TEST_CFG,	0x004B1102},
	{LDO_MAX_OFFSET,	LDO_MAX_OFFSET},
};

static struct fuse_param msm8953_ldo_enable_param[] = {
	{65, 11, 11},
	{},
};

static const struct fuse_param
msm8953_init_voltage_param[MSM8953_LDO_FUSE_CORNERS][2] = {
		{ {73, 42, 46}, {} },
		{ {73, 37, 41}, {} },
		{ {73, 32, 36}, {} },
};

static const int msm8953_fuse_ref_volt[MSM8953_LDO_FUSE_CORNERS] = {
	580000,
	650000,
	720000,
};

static int convert_open_loop_voltage_fuse(int ref_volt, int step_volt,
						u32 fuse, int fuse_len)
{
	int sign, steps;

	sign = (fuse & (1 << (fuse_len - 1))) ? -1 : 1;
	steps = fuse & ((1 << (fuse_len - 1)) - 1);

	return ref_volt + sign * steps * step_volt;
}

static int read_fuse_param(void __iomem *fuse_base_addr,
		const struct fuse_param *param, u64 *param_value)
{
	u64 fuse_val, val;
	int bits;
	int bits_total = 0;

	*param_value = 0;

	while (param->row || param->bit_start || param->bit_end) {
		if (param->bit_start > param->bit_end
		    || param->bit_end > MAX_FUSE_ROW_BIT) {
			pr_err("Invalid fuse parameter segment: row=%u, start=%u, end=%u\n",
				param->row, param->bit_start, param->bit_end);
			return -EINVAL;
		}

		bits = param->bit_end - param->bit_start + 1;
		if (bits_total + bits > 64) {
			pr_err("Invalid fuse parameter segments; total bits = %d\n",
				bits_total + bits);
			return -EINVAL;
		}

		fuse_val = readq_relaxed(fuse_base_addr
					 + param->row * BYTES_PER_FUSE_ROW);
		val = (fuse_val >> param->bit_start) & ((1ULL << bits) - 1);
		*param_value |= val << bits_total;
		bits_total += bits;

		param++;
	}

	return 0;
}

static enum regulator_mode get_operating_mode(struct msm_gfx_ldo *ldo_vreg,
								int corner)
{
	if (!ldo_vreg->ldo_mode_disable && ldo_vreg->ldo_fuse_enable
			&& ldo_vreg->ldo_corner_en_map[corner])
		return LDO;

	return BHS;
}

static char *register_str[] = {
	"LDO_ATEST",
	"LDO_CFG0",
	"LDO_CFG1",
	"LDO_CFG2",
	"LDO_LD_DATA",
	"LDO_VREF_TEST_CFG",
	"LDO_VREF_SET",
	"PWRSWITCH_CTL",
	"LDO_STATUS0",
	"LDO_STATUS1",
	"PWRSWITCH_STATUS",
};

static void dump_registers(struct msm_gfx_ldo *ldo_vreg, char *func)
{
	u32 reg[MAX_LDO_REGS];
	int i;

	for (i = 0; i < MAX_LDO_REGS; i++) {
		reg[i] = 0;
		reg[i] = readl_relaxed(ldo_vreg->ldo_base + (i * 4));
		pr_debug("%s -- %s = 0x%x\n", func, register_str[i], reg[i]);
	}
}

#define GET_VREF(a) DIV_ROUND_UP(a - MIN_LDO_VOLTAGE, LDO_STEP_VOLATGE)

static void configure_ldo_voltage(struct msm_gfx_ldo *ldo_vreg, int new_corner)
{
	int new_uv = 0, val = 0;
	u32 reg = 0;

	new_uv = ldo_vreg->open_loop_volt[new_corner];
	val = GET_VREF(new_uv);

	reg = readl_relaxed(ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	/* set the new voltage */
	reg &= ~VREF_VAL_MASK;
	reg |= val & VREF_VAL_MASK;
	writel_relaxed(reg, ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	/* Initiate VREF update */
	reg |= UPDATE_VREF_BIT;
	writel_relaxed(reg, ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	reg &= ~UPDATE_VREF_BIT;
	writel_relaxed(reg, ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	ldo_vreg->ldo_voltage_uv = new_uv;

	/* complete the write sequence */
	mb();
}

static int ldo_update_voltage(struct msm_gfx_ldo *ldo_vreg, int new_corner)
{
	int timeout = 50;
	u32 reg = 0;

	configure_ldo_voltage(ldo_vreg, new_corner);

	while (--timeout) {
		reg = readl_relaxed(ldo_vreg->ldo_base +
					PWRSWITCH_STATUS_REG);
		if (reg & (LDO_VREF_SETTLED_BIT | LDO_READY_BIT))
			break;

		udelay(10);
	}
	if (!timeout) {
		pr_err("LDO_VREF_SETTLED not set PWRSWITCH_STATUS = 0x%x\n",
									reg);
		return -EBUSY;
	}

	pr_debug("LDO voltage set to=%d uV VREF_REG=%x\n",
			ldo_vreg->ldo_voltage_uv,
			readl_relaxed(ldo_vreg->ldo_base + LDO_VREF_SET_REG));

	return 0;
}

static int enable_ldo_mode(struct msm_gfx_ldo *ldo_vreg)
{
	u32 ctl = 0;

	/* set the ldo-vref */
	configure_ldo_voltage(ldo_vreg, ldo_vreg->corner);

	pr_debug("LDO voltage configured =%d uV corner=%d\n",
			ldo_vreg->ldo_voltage_uv,
			ldo_vreg->corner + MIN_CORNER_OFFSET);

	/* configure the LDO for power-up */
	ctl = readl_relaxed(ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Move BHS under SW control */
	ctl |= BHS_UNDER_SW_CTL;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Set LDO under gdsc control */
	ctl &= ~LDO_UNDER_SW_CTRL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* enable hw_pre-on to gdsc */
	ctl |= LDO_PREON_SW_OVR_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* remove LDO bypass */
	ctl &= ~LDO_BYPASS_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* set power-source as LDO */
	ctl |= PWR_SRC_SEL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* clear fake-sw ack to gdsc */
	ctl &= ~ACK_SW_OVR_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* put CPR in bypass mode */
	ctl |= CPR_BYPASS_IN_LDO_MODE_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* complete all writes */
	mb();

	dump_registers(ldo_vreg, "enable_ldo_mode");

	return 0;
}

static int enable_bhs_mode(struct msm_gfx_ldo *ldo_vreg)
{
	u32 ctl = 0;

	ctl = readl_relaxed(ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Put LDO under SW control */
	ctl |= LDO_UNDER_SW_CTRL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* set power-source as BHS */
	ctl &= ~PWR_SRC_SEL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* clear CPR in by-pass mode */
	ctl &= ~CPR_BYPASS_IN_LDO_MODE_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Enable the BHS control signals to gdsc */
	ctl &= ~BHS_EN_FEW_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);
	ctl &= ~BHS_EN_REST_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Put BHS under GDSC control */
	ctl &= ~BHS_UNDER_SW_CTL;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	dump_registers(ldo_vreg, "enable_bhs_mode");

	return 0;
}

static int msm_gfx_ldo_enable(struct regulator_dev *rdev)
{
	struct msm_gfx_ldo *ldo_vreg  = rdev_get_drvdata(rdev);
	int rc = 0;
	enum regulator_mode enable_mode;

	mutex_lock(&ldo_vreg->ldo_mutex);

	pr_debug("regulator_enable requested. corner=%d\n",
				ldo_vreg->corner + MIN_CORNER_OFFSET);

	if (ldo_vreg->vdd_cx) {
		rc = regulator_set_voltage(ldo_vreg->vdd_cx,
			ldo_vreg->vdd_cx_corner_map[ldo_vreg->corner],
			INT_MAX);
		if (rc) {
			pr_err("Unable to set CX for corner %d rc=%d\n",
				ldo_vreg->corner + MIN_CORNER_OFFSET, rc);
			goto fail;
		}

		rc = regulator_enable(ldo_vreg->vdd_cx);
		if (rc) {
			pr_err("regulator_enable: vdd_cx: failed rc=%d\n", rc);
			goto fail;
		}
	}

	enable_mode = get_operating_mode(ldo_vreg, ldo_vreg->corner);
	if (enable_mode == LDO)
		rc = enable_ldo_mode(ldo_vreg);
	else
		rc = enable_bhs_mode(ldo_vreg);

	if (rc) {
		pr_err("Failed to enable regulator in %s mode rc=%d\n",
			(enable_mode == LDO) ? "LDO" : "BHS", rc);
		goto disable_cx;
	}

	pr_debug("regulator_enable complete. mode=%s, corner=%d\n",
			(enable_mode == LDO) ? "LDO" : "BHS",
			ldo_vreg->corner + MIN_CORNER_OFFSET);

	ldo_vreg->mode = enable_mode;
	ldo_vreg->vreg_enabled = true;

disable_cx:
	if (rc && ldo_vreg->vdd_cx) {
		rc = regulator_disable(ldo_vreg->vdd_cx);
		if (rc)
			pr_err("regulator_enable: vdd_cx: failed rc=%d\n", rc);
	}
fail:
	mutex_unlock(&ldo_vreg->ldo_mutex);
	return rc;
}

static int msm_gfx_ldo_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct msm_gfx_ldo *ldo_vreg  = rdev_get_drvdata(rdev);

	mutex_lock(&ldo_vreg->ldo_mutex);

	if (ldo_vreg->vdd_cx) {
		rc = regulator_disable(ldo_vreg->vdd_cx);
		if (rc) {
			pr_err("regulator_disable: vdd_cx: failed rc=%d\n", rc);
			goto done;
		}
		rc = regulator_set_voltage(ldo_vreg->vdd_cx, 0, INT_MAX);
		if (rc)
			pr_err("failed to set voltage on CX rc=%d\n", rc);
	}

	/* No additional configuration for LDO/BHS - taken care by gsdc */
	ldo_vreg->vreg_enabled = false;

	pr_debug("regulator_disabled complete\n");
done:
	mutex_unlock(&ldo_vreg->ldo_mutex);
	return rc;
}

static int switch_mode_to_ldo(struct msm_gfx_ldo *ldo_vreg, int new_corner)
{
	u32 ctl = 0, status = 0, timeout = 50;

	ctl = readl_relaxed(ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* enable CPR bypass mode for LDO */
	ctl |= CPR_BYPASS_IN_LDO_MODE_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* fake ack to GDSC */
	ctl |= ACK_SW_OVR_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* set power-source as LDO */
	ctl |= PWR_SRC_SEL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Make sure BHS continues to power the rail */
	ctl |= BHS_EN_FEW_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);
	ctl |= BHS_EN_REST_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* move BHS to SW control */
	ctl |= BHS_UNDER_SW_CTL;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* set LDO under SW control */
	ctl |= LDO_UNDER_SW_CTRL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* bypass LDO */
	ctl |= LDO_BYPASS_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* power-on LDO */
	ctl &= ~LDO_PDN_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* set the new LDO voltage */
	ldo_update_voltage(ldo_vreg, new_corner);

	pr_debug("LDO voltage =%d uV\n", ldo_vreg->ldo_voltage_uv);

	/* make sure that the configuration is complete */
	mb();

	/* power down BHS */
	ctl &= ~BHS_EN_FEW_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);
	ctl &= ~BHS_EN_REST_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* make sure that the configuration is complete */
	mb();

	/* wait for BHS to turn-off */
	while (--timeout) {
		status = readl_relaxed(ldo_vreg->ldo_base +
					PWRSWITCH_STATUS_REG);
		if (!(status & BHS_EN_REST_ACK_BIT))
			break;

		udelay(10);
	}

	if (!timeout)
		pr_err("BHS_EN_RESET_ACK not clear PWRSWITCH_STATUS = 0x%x\n",
								status);

	/* remove LDO bypass */
	ctl &= ~LDO_BYPASS_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* expose LDO to gdsc */
	ctl &= ~ACK_SW_OVR_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	ctl &= ~LDO_UNDER_SW_CTRL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	dump_registers(ldo_vreg, "switch_mode_to_ldo");

	return 0;
}

static int switch_mode_to_bhs(struct msm_gfx_ldo *ldo_vreg)
{
	u32 ctl = 0, status = 0, timeout = 50;

	ctl = readl_relaxed(ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* fake ack to gdsc */
	ctl |= ACK_SW_OVR_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* select BHS as power source */
	ctl &= ~PWR_SRC_SEL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* LDO stays ON */
	ctl &= ~LDO_PDN_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Move LDO to SW control */
	ctl |= LDO_UNDER_SW_CTRL_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Power-up BHS */
	ctl |= BHS_EN_FEW_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);
	ctl |= BHS_EN_REST_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* make sure that the configuration is complete */
	mb();

	/* wait for BHS to power-up */
	while (--timeout) {
		status = readl_relaxed(ldo_vreg->ldo_base +
					PWRSWITCH_STATUS_REG);
		if (status & BHS_EN_REST_ACK_BIT)
			break;

		udelay(10);
	}
	if (!timeout)
		pr_err("BHS_EN_RESET_ACK not set PWRSWITCH_STATUS = 0x%x\n",
								status);

	/* bypass LDO */
	ctl |= LDO_BYPASS_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* pull-down LDO */
	ctl |= LDO_PDN_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Expose BHS to gdsc */
	ctl &= ~ACK_SW_OVR_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);
	ctl &= ~BHS_UNDER_SW_CTL;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* Enable CPR in BHS mode */
	ctl &= ~CPR_BYPASS_IN_LDO_MODE_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	/* make sure that all configuration is complete */
	mb();

	dump_registers(ldo_vreg, "switch_mode_to_bhs");

	return 0;
}

static int msm_gfx_ldo_set_voltage(struct regulator_dev *rdev,
		int corner, int corner_max, unsigned *selector)
{
	struct msm_gfx_ldo *ldo_vreg  = rdev_get_drvdata(rdev);
	int rc = 0, mem_acc_corner;
	enum regulator_mode new_mode;
	enum direction dir = NO_CHANGE;

	corner -= MIN_CORNER_OFFSET;
	corner_max -= MIN_CORNER_OFFSET;

	mutex_lock(&ldo_vreg->ldo_mutex);

	if (corner == ldo_vreg->corner)
		goto done;

	pr_debug("set-voltage requested: old_mode=%s old_corner=%d new_corner=%d vreg_enabled=%d\n",
				ldo_vreg->mode == BHS ? "BHS" : "LDO",
				ldo_vreg->corner + MIN_CORNER_OFFSET,
				corner + MIN_CORNER_OFFSET,
				ldo_vreg->vreg_enabled);

	if (corner > ldo_vreg->corner)
		dir = UP;
	else if (corner < ldo_vreg->corner)
		dir = DOWN;

	if (ldo_vreg->mem_acc_vreg && dir == DOWN) {
		mem_acc_corner = ldo_vreg->mem_acc_corner_map[corner];
		rc = regulator_set_voltage(ldo_vreg->mem_acc_vreg,
				mem_acc_corner, mem_acc_corner);
	}

	if (!ldo_vreg->vreg_enabled) {
		ldo_vreg->corner = corner;
		goto done;
	}

	if (ldo_vreg->vdd_cx) {
		rc = regulator_set_voltage(ldo_vreg->vdd_cx,
			ldo_vreg->vdd_cx_corner_map[corner],
			INT_MAX);
		if (rc) {
			pr_err("Unable to set CX for corner %d rc=%d\n",
					corner + MIN_CORNER_OFFSET, rc);
			goto done;
		}
	}

	new_mode = get_operating_mode(ldo_vreg, corner);

	if (new_mode == BHS) {
		if (ldo_vreg->mode == LDO) {
			rc = switch_mode_to_bhs(ldo_vreg);
			if (rc)
				pr_err("Switch to BHS corner=%d failed rc=%d\n",
						corner + MIN_CORNER_OFFSET, rc);
		}
	} else { /* new mode - LDO */
		if (ldo_vreg->mode == BHS) {
			rc = switch_mode_to_ldo(ldo_vreg, corner);
			if (rc)
				pr_err("Switch to LDO failed corner=%d rc=%d\n",
						corner + MIN_CORNER_OFFSET, rc);
		} else {
			rc = ldo_update_voltage(ldo_vreg, corner);
			if (rc)
				pr_err("Update voltage failed corner=%d rc=%d\n",
						corner + MIN_CORNER_OFFSET, rc);
		}
	}

	if (!rc) {
		pr_debug("set-voltage complete. old_mode=%s new_mode=%s old_corner=%d new_corner=%d\n",
				ldo_vreg->mode == BHS ? "BHS" : "LDO",
				new_mode == BHS ? "BHS" : "LDO",
				ldo_vreg->corner + MIN_CORNER_OFFSET,
				corner + MIN_CORNER_OFFSET);

		ldo_vreg->mode = new_mode;
		ldo_vreg->corner = corner;
	}

done:
	if (!rc && ldo_vreg->mem_acc_vreg && dir == UP) {
		mem_acc_corner = ldo_vreg->mem_acc_corner_map[corner];
		rc = regulator_set_voltage(ldo_vreg->mem_acc_vreg,
				mem_acc_corner, mem_acc_corner);
	}
	mutex_unlock(&ldo_vreg->ldo_mutex);
	return rc;
}

static int msm_gfx_ldo_get_voltage(struct regulator_dev *rdev)
{
	struct msm_gfx_ldo *ldo_vreg  = rdev_get_drvdata(rdev);

	return ldo_vreg->corner + MIN_CORNER_OFFSET;
}

static int msm_gfx_ldo_is_enabled(struct regulator_dev *rdev)
{
	struct msm_gfx_ldo *ldo_vreg  = rdev_get_drvdata(rdev);

	return ldo_vreg->vreg_enabled;
}

static struct regulator_ops msm_gfx_ldo_corner_ops = {
	.enable		= msm_gfx_ldo_enable,
	.disable	= msm_gfx_ldo_disable,
	.is_enabled	= msm_gfx_ldo_is_enabled,
	.set_voltage	= msm_gfx_ldo_set_voltage,
	.get_voltage	= msm_gfx_ldo_get_voltage,
};

static int msm_gfx_ldo_adjust_init_voltage(struct msm_gfx_ldo *ldo_vreg)
{
	int rc, len, size, i;
	u32 *volt_adjust;
	struct device_node *of_node = ldo_vreg->dev->of_node;
	char *prop_name = "qcom,ldo-init-voltage-adjustment";

	if (!of_find_property(of_node, prop_name, &len)) {
		/* No initial voltage adjustment needed. */
		return 0;
	}

	size = len / sizeof(u32);
	if (size != ldo_vreg->num_ldo_corners) {
		pr_err("%s length=%d is invalid: required:%d\n",
				prop_name, size, ldo_vreg->num_ldo_corners);
		return -EINVAL;
	}

	volt_adjust = devm_kcalloc(ldo_vreg->dev, size, sizeof(*volt_adjust),
								GFP_KERNEL);
	if (!volt_adjust)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, prop_name, volt_adjust, size);
	if (rc) {
		pr_err("failed to read %s property rc=%d\n", prop_name, rc);
		return rc;
	}

	for (i = 0; i < ldo_vreg->num_corners; i++) {
		if (volt_adjust[i]) {
			ldo_vreg->open_loop_volt[i] += volt_adjust[i];
			pr_info("adjusted the open-loop voltage[%d] %d -> %d\n",
				i + MIN_CORNER_OFFSET,
				ldo_vreg->open_loop_volt[i] - volt_adjust[i],
				ldo_vreg->open_loop_volt[i]);
		}
	}

	return 0;
}

static int msm_gfx_ldo_voltage_init(struct msm_gfx_ldo *ldo_vreg)
{
	struct device_node *of_node = ldo_vreg->dev->of_node;
	int i, rc, len;
	u64 efuse_bits;

	len = ldo_vreg->num_ldo_corners;

	ldo_vreg->open_loop_volt = devm_kcalloc(ldo_vreg->dev,
			len, sizeof(*ldo_vreg->open_loop_volt),
			GFP_KERNEL);
	ldo_vreg->ceiling_volt = devm_kcalloc(ldo_vreg->dev,
			len, sizeof(*ldo_vreg->ceiling_volt),
			GFP_KERNEL);
	ldo_vreg->floor_volt = devm_kcalloc(ldo_vreg->dev,
			len, sizeof(*ldo_vreg->floor_volt),
			GFP_KERNEL);

	if (!ldo_vreg->open_loop_volt || !ldo_vreg->ceiling_volt
					|| !ldo_vreg->floor_volt)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,ldo-voltage-ceiling",
						ldo_vreg->ceiling_volt, len);
	if (rc) {
		pr_err("Unable to read qcom,ldo-voltage-ceiling rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node, "qcom,ldo-voltage-floor",
						ldo_vreg->floor_volt, len);
	if (rc) {
		pr_err("Unable to read qcom,ldo-voltage-floor rc=%d\n", rc);
		return rc;
	}

	for (i = 0; i < ldo_vreg->num_ldo_corners; i++) {
		rc = read_fuse_param(ldo_vreg->efuse_base,
				ldo_vreg->init_volt_param[i],
				&efuse_bits);
		if (rc) {
			pr_err("Unable to read init-voltage rc=%d\n", rc);
			return rc;
		}
		ldo_vreg->open_loop_volt[i] = convert_open_loop_voltage_fuse(
					ldo_vreg->ref_volt[i],
					GFX_LDO_FUSE_STEP_VOLT,
					efuse_bits,
					GFX_LDO_FUSE_SIZE);
		pr_info("LDO corner %d: target-volt = %d uV\n",
			i + MIN_CORNER_OFFSET, ldo_vreg->open_loop_volt[i]);
	}

	rc = msm_gfx_ldo_adjust_init_voltage(ldo_vreg);
	if (rc) {
		pr_err("Unable to adjust init voltages rc=%d\n", rc);
		return rc;
	}

	for (i = 0; i < ldo_vreg->num_ldo_corners; i++) {
		if (ldo_vreg->open_loop_volt[i] > ldo_vreg->ceiling_volt[i]) {
			pr_info("Warning: initial voltage[%d] %d above ceiling %d\n",
				i + MIN_CORNER_OFFSET,
				ldo_vreg->open_loop_volt[i],
				ldo_vreg->ceiling_volt[i]);
			ldo_vreg->open_loop_volt[i] = ldo_vreg->ceiling_volt[i];
		} else if (ldo_vreg->open_loop_volt[i] <
				ldo_vreg->floor_volt[i]) {
			pr_info("Warning: initial voltage[%d] %d below floor %d\n",
				i + MIN_CORNER_OFFSET,
				ldo_vreg->open_loop_volt[i],
				ldo_vreg->floor_volt[i]);
			ldo_vreg->open_loop_volt[i] = ldo_vreg->floor_volt[i];
		}
	}

	efuse_bits = 0;
	rc = read_fuse_param(ldo_vreg->efuse_base, ldo_vreg->ldo_enable_param,
								&efuse_bits);
	if (rc) {
		pr_err("Unable to read ldo_enable_param rc=%d\n", rc);
		return rc;
	}
	ldo_vreg->ldo_fuse_enable = !!efuse_bits;
	pr_info("LDO-mode fuse %s by default\n", ldo_vreg->ldo_fuse_enable ?
					"enabled" : "disabled");

	return rc;
}

static int msm_gfx_ldo_efuse_init(struct platform_device *pdev,
				struct msm_gfx_ldo *ldo_vreg)
{
	struct resource *res;
	u32 len;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse_addr");
	if (!res || !res->start) {
		pr_err("efuse_addr missing: res=%p\n", res);
		return -EINVAL;
	}

	ldo_vreg->efuse_addr = res->start;
	len = res->end - res->start + 1;

	ldo_vreg->efuse_base = devm_ioremap(&pdev->dev,
				ldo_vreg->efuse_addr, len);
	if (!ldo_vreg->efuse_base) {
		pr_err("Unable to map efuse_addr %pa\n",
				&ldo_vreg->efuse_addr);
		return -EINVAL;
	}

	return 0;
}

static int msm_gfx_ldo_mem_acc_init(struct msm_gfx_ldo *ldo_vreg)
{
	int rc;
	u32 len, size;
	struct device_node *of_node = ldo_vreg->dev->of_node;

	if (of_find_property(ldo_vreg->dev->of_node, "mem-acc-supply", NULL)) {
		ldo_vreg->mem_acc_vreg = devm_regulator_get(ldo_vreg->dev,
							"mem-acc");
		if (IS_ERR_OR_NULL(ldo_vreg->mem_acc_vreg)) {
			rc = PTR_RET(ldo_vreg->mem_acc_vreg);
			if (rc != -EPROBE_DEFER)
				pr_err("devm_regulator_get: mem-acc: rc=%d\n",
					rc);
			return rc;
		}
	} else {
		pr_debug("mem-acc-supply not specified\n");
		return 0;
	}

	if (!of_find_property(of_node, "qcom,mem-acc-corner-map", &len)) {
		pr_err("qcom,mem-acc-corner-map missing");
		return -EINVAL;
	}

	size = len / sizeof(u32);
	if (size != ldo_vreg->num_corners) {
		pr_err("qcom,mem-acc-corner-map length=%d is invalid: required:%u\n",
						size, ldo_vreg->num_corners);
		return -EINVAL;
	}

	ldo_vreg->mem_acc_corner_map = devm_kcalloc(ldo_vreg->dev, size,
			sizeof(*ldo_vreg->mem_acc_corner_map), GFP_KERNEL);
	if (!ldo_vreg->mem_acc_corner_map)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,mem-acc-corner-map",
					ldo_vreg->mem_acc_corner_map, size);
	if (rc)
		pr_err("Unable to read qcom,mem-acc-corner-map rc=%d\n", rc);

	return rc;
}

static int msm_gfx_ldo_init(struct platform_device *pdev,
				struct msm_gfx_ldo *ldo_vreg)
{
	struct resource *res;
	u32 len, ctl;
	int rc, i = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ldo_addr");
	if (!res || !res->start) {
		pr_err("ldo_addr missing: res=%p\n", res);
		return -EINVAL;
	}

	ldo_vreg->ldo_addr = res->start;
	len = res->end - res->start + 1;

	ldo_vreg->ldo_base = devm_ioremap(ldo_vreg->dev,
					ldo_vreg->ldo_addr, len);
	if (!ldo_vreg->ldo_base) {
		pr_err("Unable to map efuse_addr %pa\n",
				&ldo_vreg->ldo_addr);
		return -EINVAL;
	}

	rc = msm_gfx_ldo_mem_acc_init(ldo_vreg);
	if (rc) {
		pr_err("Unable to initialize mem_acc rc=%d\n", rc);
		return rc;
	}

	/* HW initialization */

	/* clear clamp_io, enable CPR in auto-bypass*/
	ctl = readl_relaxed(ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);
	ctl &= ~LDO_CLAMP_IO_BIT;
	ctl |= EN_LDOAP_CTRL_CPR_BIT;
	writel_relaxed(ctl, ldo_vreg->ldo_base + PWRSWITCH_CTRL_REG);

	i = 0;
	while (ldo_vreg->ldo_init_config &&
			ldo_vreg->ldo_init_config[i].offset != LDO_MAX_OFFSET) {
		writel_relaxed(ldo_vreg->ldo_init_config[i].value,
				ldo_vreg->ldo_base +
				ldo_vreg->ldo_init_config[i].offset);
		i++;
	}
	/* complete the writes */
	mb();

	return 0;
}

static int ldo_parse_cx_parameters(struct msm_gfx_ldo *ldo_vreg)
{
	struct device_node *of_node = ldo_vreg->dev->of_node;
	int rc, len, size;

	if (of_find_property(of_node, "vdd-cx-supply", NULL)) {
		ldo_vreg->vdd_cx = devm_regulator_get(ldo_vreg->dev, "vdd-cx");
		if (IS_ERR_OR_NULL(ldo_vreg->vdd_cx)) {
			rc = PTR_RET(ldo_vreg->vdd_cx);
			if (rc != -EPROBE_DEFER)
				pr_err("devm_regulator_get: vdd-cx: rc=%d\n",
					rc);
			return rc;
		}
	} else {
		pr_debug("vdd-cx-supply not specified\n");
		return 0;
	}

	if (!of_find_property(of_node, "qcom,vdd-cx-corner-map", &len)) {
		pr_err("qcom,vdd-cx-corner-map missing");
		return -EINVAL;
	}

	size = len / sizeof(u32);
	if (size != ldo_vreg->num_corners) {
		pr_err("qcom,vdd-cx-corner-map length=%d is invalid: required:%u\n",
						size, ldo_vreg->num_corners);
		return -EINVAL;
	}

	ldo_vreg->vdd_cx_corner_map = devm_kcalloc(ldo_vreg->dev, size,
			sizeof(*ldo_vreg->vdd_cx_corner_map), GFP_KERNEL);
	if (!ldo_vreg->vdd_cx_corner_map)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,vdd-cx-corner-map",
					ldo_vreg->vdd_cx_corner_map, size);
	if (rc)
		pr_err("Unable to read qcom,vdd-cx-corner-map rc=%d\n", rc);


	return rc;
}

static int msm_gfx_ldo_parse_dt(struct msm_gfx_ldo *ldo_vreg)
{
	struct device_node *of_node = ldo_vreg->dev->of_node;
	int rc, size, len;

	rc = of_property_read_u32(of_node, "qcom,num-corners",
					&ldo_vreg->num_corners);
	if (rc < 0) {
		pr_err("Unable to read qcom,num-corners rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,num-ldo-corners",
					&ldo_vreg->num_ldo_corners);
	if (rc) {
		pr_err("Unable to read qcom,num-ldo-corners rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,init-corner",
					&ldo_vreg->corner);
	if (rc) {
		pr_err("Unable to read qcom,init-corner rc=%d\n", rc);
		return rc;
	}

	if (!of_find_property(of_node, "qcom,ldo-enable-corner-map", &len)) {
		pr_err("qcom,ldo-enable-corner-map missing\n");
		return -EINVAL;
	}

	size = len / sizeof(u32);
	if (size != ldo_vreg->num_corners) {
		pr_err("qcom,ldo-enable-corner-map length=%d is invalid: required:%u\n",
					size, ldo_vreg->num_corners);
		return -EINVAL;
	}

	ldo_vreg->ldo_corner_en_map = devm_kcalloc(ldo_vreg->dev, size,
			sizeof(*ldo_vreg->ldo_corner_en_map), GFP_KERNEL);
	if (!ldo_vreg->ldo_corner_en_map)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,ldo-enable-corner-map",
					ldo_vreg->ldo_corner_en_map, size);
	if (rc) {
		pr_err("Unable to read qcom,ldo-enable-corner-map rc=%d\n",
									rc);
		return rc;
	}

	rc = ldo_parse_cx_parameters(ldo_vreg);
	if (rc) {
		pr_err("Unable to parse CX parameters rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int msm_gfx_ldo_target_init(struct msm_gfx_ldo *ldo_vreg)
{
	int i;

	/* MSM8953 */
	ldo_vreg->init_volt_param = devm_kzalloc(ldo_vreg->dev,
			(MSM8953_LDO_FUSE_CORNERS *
			sizeof(struct fuse_param *)), GFP_KERNEL);
	if (!ldo_vreg->init_volt_param)
		return -ENOMEM;

	for (i = 0; i < MSM8953_LDO_FUSE_CORNERS; i++)
		ldo_vreg->init_volt_param[i] =
				msm8953_init_voltage_param[i];

	ldo_vreg->ldo_init_config = msm8953_ldo_config;
	ldo_vreg->ref_volt = msm8953_fuse_ref_volt;
	ldo_vreg->ldo_enable_param = msm8953_ldo_enable_param;

	return 0;
}

static int debugfs_ldo_mode_disable_set(void *data, u64 val)
{
	struct msm_gfx_ldo *ldo_vreg = data;

	ldo_vreg->ldo_mode_disable = !!val;

	pr_debug("LDO-mode %s\n", ldo_vreg->ldo_mode_disable ?
					"disabled" : "enabled");

	return 0;
}

static int debugfs_ldo_mode_disable_get(void *data, u64 *val)
{
	struct msm_gfx_ldo *ldo_vreg = data;

	*val = ldo_vreg->ldo_mode_disable;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(ldo_mode_disable_fops, debugfs_ldo_mode_disable_get,
				debugfs_ldo_mode_disable_set, "%llu\n");

static int debugfs_ldo_set_voltage(void *data, u64 val)
{
	struct msm_gfx_ldo *ldo_vreg = data;
	int rc = 0, timeout = 50;
	u32 reg = 0, voltage = 0;

	mutex_lock(&ldo_vreg->ldo_mutex);

	if (ldo_vreg->mode == BHS || !ldo_vreg->vreg_enabled ||
		val > MAX_LDO_VOLTAGE || val < MIN_LDO_VOLTAGE) {
		rc = -EINVAL;
		goto done;
	}
	voltage = GET_VREF((u32)val);

	reg = readl_relaxed(ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	/* set the new voltage */
	reg &= ~VREF_VAL_MASK;
	reg |= voltage & VREF_VAL_MASK;
	writel_relaxed(reg, ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	/* Initiate VREF update */
	reg |= UPDATE_VREF_BIT;
	writel_relaxed(reg, ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	reg &= ~UPDATE_VREF_BIT;
	writel_relaxed(reg, ldo_vreg->ldo_base + LDO_VREF_SET_REG);

	/* complete the writes */
	mb();

	while (--timeout) {
		reg = readl_relaxed(ldo_vreg->ldo_base +
					PWRSWITCH_STATUS_REG);
		if (reg & (LDO_VREF_SETTLED_BIT | LDO_READY_BIT))
			break;

		udelay(10);
	}
	if (!timeout) {
		pr_err("LDO_VREF_SETTLED not set PWRSWITCH_STATUS = 0x%x\n",
								reg);
		rc = -EBUSY;
	} else {
		ldo_vreg->ldo_voltage_uv = val;
		pr_debug("LDO voltage set to %d uV\n",
				ldo_vreg->ldo_voltage_uv);
	}
done:
	mutex_unlock(&ldo_vreg->ldo_mutex);
	return rc;
}

static int debugfs_ldo_get_voltage(void *data, u64 *val)
{
	struct msm_gfx_ldo *ldo_vreg = data;
	int rc = 0;
	u32 reg;

	mutex_lock(&ldo_vreg->ldo_mutex);

	if (ldo_vreg->mode == BHS || !ldo_vreg->vreg_enabled) {
		rc = -EINVAL;
		goto done;
	}

	reg = readl_relaxed(ldo_vreg->ldo_base + LDO_VREF_SET_REG);
	reg &= VREF_VAL_MASK;

	*val = (reg * LDO_STEP_VOLATGE) + MIN_LDO_VOLTAGE;
done:
	mutex_unlock(&ldo_vreg->ldo_mutex);
	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(ldo_voltage_fops, debugfs_ldo_get_voltage,
				debugfs_ldo_set_voltage, "%llu\n");

static int msm_gfx_ldo_debug_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t msm_gfx_ldo_debug_info_read(struct file *file, char __user *buff,
				size_t count, loff_t *ppos)
{
	struct msm_gfx_ldo *ldo_vreg = file->private_data;
	char *debugfs_buf;
	ssize_t len, ret = 0;
	u32 i = 0, reg[MAX_LDO_REGS];

	debugfs_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!debugfs_buf)
		return -ENOMEM;

	mutex_lock(&ldo_vreg->ldo_mutex);

	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
		"Regulator_enable = %d Regulator mode = %s Corner = %d LDO-voltage = %d uV\n",
		ldo_vreg->vreg_enabled,
		ldo_vreg->mode == BHS ? "BHS" : "LDO",
		ldo_vreg->corner + MIN_CORNER_OFFSET,
		ldo_vreg->ldo_voltage_uv);
	ret += len;

	for (i = 0; i < MAX_LDO_REGS; i++) {
		reg[i] = 0;
		reg[i] = readl_relaxed(ldo_vreg->ldo_base + (i * 4));
		len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
				"%s = 0x%x\n",  register_str[i], reg[i]);
		ret += len;
	}

	mutex_unlock(&ldo_vreg->ldo_mutex);

	ret = simple_read_from_buffer(buff, count, ppos, debugfs_buf, ret);
	kfree(debugfs_buf);

	return ret;
}

static const struct file_operations msm_gfx_ldo_debug_info_fops = {
	.open = msm_gfx_ldo_debug_info_open,
	.read = msm_gfx_ldo_debug_info_read,
};

static void msm_gfx_ldo_debugfs_init(struct msm_gfx_ldo *ldo_vreg)
{
	struct dentry *temp;

	ldo_vreg->debugfs = debugfs_create_dir("msm_gfx_ldo", NULL);
	if (!ldo_vreg->debugfs) {
		pr_err("Couldn't create debug dir\n");
		return;
	}

	temp = debugfs_create_file("debug_info", S_IRUGO, ldo_vreg->debugfs,
					ldo_vreg, &msm_gfx_ldo_debug_info_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("debug_info node creation failed\n");
		return;
	}

	temp = debugfs_create_file("ldo_voltage", S_IRUGO | S_IWUSR,
			ldo_vreg->debugfs, ldo_vreg, &ldo_voltage_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("ldo_voltage node creation failed\n");
		return;
	}

	temp = debugfs_create_file("ldo_mode_disable", S_IRUGO | S_IWUSR,
			ldo_vreg->debugfs, ldo_vreg, &ldo_mode_disable_fops);
	if (IS_ERR_OR_NULL(temp)) {
		pr_err("ldo_mode_disable node creation failed\n");
		return;
	}
}

static void msm_gfx_ldo_debugfs_remove(struct msm_gfx_ldo *ldo_vreg)
{
	debugfs_remove_recursive(ldo_vreg->debugfs);
}

static int msm_gfx_ldo_probe(struct platform_device *pdev)
{
	struct msm_gfx_ldo *ldo_vreg;
	struct regulator_config reg_config = {};
	struct regulator_desc *rdesc;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	int rc;

	ldo_vreg = devm_kzalloc(dev, sizeof(*ldo_vreg), GFP_KERNEL);
	if (!ldo_vreg)
		return -ENOMEM;

	init_data = of_get_regulator_init_data(dev, dev->of_node);
	if (!init_data) {
		pr_err("regulator init data is missing\n");
		return -EINVAL;
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS;

	ldo_vreg->rdesc.name = init_data->constraints.name;
	if (ldo_vreg->rdesc.name == NULL) {
		dev_err(dev, "regulator-name missing\n");
		return -EINVAL;
	}

	ldo_vreg->dev = &pdev->dev;
	mutex_init(&ldo_vreg->ldo_mutex);
	platform_set_drvdata(pdev, ldo_vreg);

	rc = msm_gfx_ldo_target_init(ldo_vreg);
	if (rc) {
		pr_err("Unable to initialize target specific data rc=%d", rc);
		return rc;
	}

	rc = msm_gfx_ldo_parse_dt(ldo_vreg);
	if (rc) {
		pr_err("Unable to pasrse dt rc=%d\n", rc);
		return rc;
	}

	rc = msm_gfx_ldo_efuse_init(pdev, ldo_vreg);
	if (rc) {
		pr_err("efuse_init failed rc=%d\n", rc);
		return rc;
	}

	rc = msm_gfx_ldo_voltage_init(ldo_vreg);
	if (rc) {
		pr_err("ldo_voltage_init failed rc=%d\n", rc);
		return rc;
	}

	rc = msm_gfx_ldo_init(pdev, ldo_vreg);
	if (rc) {
		pr_err("ldo_init failed rc=%d\n", rc);
		return rc;
	}

	rdesc			= &ldo_vreg->rdesc;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;
	rdesc->ops		= &msm_gfx_ldo_corner_ops;

	reg_config.dev = &pdev->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = ldo_vreg;
	reg_config.of_node = pdev->dev.of_node;
	ldo_vreg->rdev = regulator_register(rdesc, &reg_config);
	if (IS_ERR(ldo_vreg->rdev)) {
		rc = PTR_ERR(ldo_vreg->rdev);
		pr_err("regulator_register failed: rc=%d\n", rc);
		return rc;
	}

	msm_gfx_ldo_debugfs_init(ldo_vreg);

	return 0;
}

static int msm_gfx_ldo_remove(struct platform_device *pdev)
{
	struct msm_gfx_ldo *ldo_vreg = platform_get_drvdata(pdev);

	regulator_unregister(ldo_vreg->rdev);

	msm_gfx_ldo_debugfs_remove(ldo_vreg);

	return 0;
}

static struct of_device_id msm_gfx_ldo_match_table[] = {
	{ .compatible = "qcom,msm8953-gfx-ldo", },
	{}
};

static struct platform_driver msm_gfx_ldo_driver = {
	.driver		= {
		.name		= "qcom,msm-gfx-ldo",
		.of_match_table = msm_gfx_ldo_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= msm_gfx_ldo_probe,
	.remove		= msm_gfx_ldo_remove,
};

static int msm_gfx_ldo_platform_init(void)
{
	return platform_driver_register(&msm_gfx_ldo_driver);
}
arch_initcall(msm_gfx_ldo_platform_init);

static void msm_gfx_ldo_platform_exit(void)
{
	platform_driver_unregister(&msm_gfx_ldo_driver);
}
module_exit(msm_gfx_ldo_platform_exit);

MODULE_DESCRIPTION("MSM GFX LDO driver");
MODULE_LICENSE("GPL v2");
