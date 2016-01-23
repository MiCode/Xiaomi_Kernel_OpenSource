/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

/* Register Offsets for RB-CPR and Bit Definitions */

/* RBCPR Version Register */
#define REG_RBCPR_VERSION		0
#define RBCPR_VER_2			0x02

/* RBCPR Gate Count and Target Registers */
#define REG_RBCPR_GCNT_TARGET(n)	(0x60 + 4 * n)

#define RBCPR_GCNT_TARGET_GCNT_BITS	10
#define RBCPR_GCNT_TARGET_GCNT_SHIFT	12
#define RBCPR_GCNT_TARGET_GCNT_MASK	((1<<RBCPR_GCNT_TARGET_GCNT_BITS)-1)

/* RBCPR Timer Control */
#define REG_RBCPR_TIMER_INTERVAL	0x44
#define REG_RBIF_TIMER_ADJUST		0x4C

#define RBIF_TIMER_ADJ_CONS_UP_BITS	4
#define RBIF_TIMER_ADJ_CONS_UP_MASK	((1<<RBIF_TIMER_ADJ_CONS_UP_BITS)-1)
#define RBIF_TIMER_ADJ_CONS_DOWN_BITS	4
#define RBIF_TIMER_ADJ_CONS_DOWN_MASK	((1<<RBIF_TIMER_ADJ_CONS_DOWN_BITS)-1)
#define RBIF_TIMER_ADJ_CONS_DOWN_SHIFT	4

/* RBCPR Config Register */
#define REG_RBIF_LIMIT			0x48
#define REG_RBCPR_STEP_QUOT		0x80
#define REG_RBIF_SW_VLEVEL		0x94

#define RBIF_LIMIT_CEILING_BITS		6
#define RBIF_LIMIT_CEILING_MASK		((1<<RBIF_LIMIT_CEILING_BITS)-1)
#define RBIF_LIMIT_CEILING_SHIFT	6
#define RBIF_LIMIT_FLOOR_BITS		6
#define RBIF_LIMIT_FLOOR_MASK		((1<<RBIF_LIMIT_FLOOR_BITS)-1)

#define RBIF_LIMIT_CEILING_DEFAULT	RBIF_LIMIT_CEILING_MASK
#define RBIF_LIMIT_FLOOR_DEFAULT	0
#define RBIF_SW_VLEVEL_DEFAULT		0x20

#define RBCPR_STEP_QUOT_STEPQUOT_BITS	8
#define RBCPR_STEP_QUOT_STEPQUOT_MASK	((1<<RBCPR_STEP_QUOT_STEPQUOT_BITS)-1)
#define RBCPR_STEP_QUOT_IDLE_CLK_BITS	4
#define RBCPR_STEP_QUOT_IDLE_CLK_MASK	((1<<RBCPR_STEP_QUOT_IDLE_CLK_BITS)-1)
#define RBCPR_STEP_QUOT_IDLE_CLK_SHIFT	8

/* RBCPR Control Register */
#define REG_RBCPR_CTL			0x90

#define RBCPR_CTL_LOOP_EN			BIT(0)
#define RBCPR_CTL_TIMER_EN			BIT(3)
#define RBCPR_CTL_SW_AUTO_CONT_ACK_EN		BIT(5)
#define RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN	BIT(6)
#define RBCPR_CTL_COUNT_MODE			BIT(10)
#define RBCPR_CTL_UP_THRESHOLD_BITS	4
#define RBCPR_CTL_UP_THRESHOLD_MASK	((1<<RBCPR_CTL_UP_THRESHOLD_BITS)-1)
#define RBCPR_CTL_UP_THRESHOLD_SHIFT	24
#define RBCPR_CTL_DN_THRESHOLD_BITS	4
#define RBCPR_CTL_DN_THRESHOLD_MASK	((1<<RBCPR_CTL_DN_THRESHOLD_BITS)-1)
#define RBCPR_CTL_DN_THRESHOLD_SHIFT	28

/* RBCPR Ack/Nack Response */
#define REG_RBIF_CONT_ACK_CMD		0x98
#define REG_RBIF_CONT_NACK_CMD		0x9C

/* RBCPR Result status Register */
#define REG_RBCPR_RESULT_0		0xA0

#define RBCPR_RESULT0_BUSY_SHIFT	19
#define RBCPR_RESULT0_BUSY_MASK		BIT(RBCPR_RESULT0_BUSY_SHIFT)
#define RBCPR_RESULT0_ERROR_LT0_SHIFT	18
#define RBCPR_RESULT0_ERROR_SHIFT	6
#define RBCPR_RESULT0_ERROR_BITS	12
#define RBCPR_RESULT0_ERROR_MASK	((1<<RBCPR_RESULT0_ERROR_BITS)-1)
#define RBCPR_RESULT0_ERROR_STEPS_SHIFT	2
#define RBCPR_RESULT0_ERROR_STEPS_BITS	4
#define RBCPR_RESULT0_ERROR_STEPS_MASK	((1<<RBCPR_RESULT0_ERROR_STEPS_BITS)-1)
#define RBCPR_RESULT0_STEP_UP_SHIFT	1

/* RBCPR Interrupt Control Register */
#define REG_RBIF_IRQ_EN(n)		(0x100 + 4 * n)
#define REG_RBIF_IRQ_CLEAR		0x110
#define REG_RBIF_IRQ_STATUS		0x114

#define CPR_INT_DONE		BIT(0)
#define CPR_INT_MIN		BIT(1)
#define CPR_INT_DOWN		BIT(2)
#define CPR_INT_MID		BIT(3)
#define CPR_INT_UP		BIT(4)
#define CPR_INT_MAX		BIT(5)
#define CPR_INT_CLAMP		BIT(6)
#define CPR_INT_ALL	(CPR_INT_DONE | CPR_INT_MIN | CPR_INT_DOWN | \
			CPR_INT_MID | CPR_INT_UP | CPR_INT_MAX | CPR_INT_CLAMP)
#define CPR_INT_DEFAULT	(CPR_INT_UP | CPR_INT_DOWN)

#define BYTES_PER_FUSE_ROW		8

#define FLAGS_IGNORE_1ST_IRQ_STATUS	BIT(0)

#define FUSE_REVISION_UNKNOWN		(-1)
#define FUSE_MAP_NO_MATCH		(-1)
#define FUSE_PARAM_MATCH_ANY		0xFFFFFFFF

#define CPR_CORNER_MIN		1
/*
 * This is an arbitrary upper limit which is used in a sanity check in order to
 * avoid excessive memory allocation due to bad device tree data.
 */
#define CPR_CORNER_LIMIT	100

enum voltage_change_dir {
	NO_CHANGE,
	DOWN,
	UP,
};

struct cpr2_gfx_regulator {
	struct list_head	list;
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
	struct device		*dev;
	struct clk		*core_clk;
	struct clk		*iface_clk;
	bool			vreg_enabled;
	int			corner;
	int			ceiling_max;
	struct dentry		*debugfs;

	/* eFuse parameters */
	phys_addr_t		efuse_addr;
	void __iomem		*efuse_base;

	/* Process voltage parameters */
	int			*open_loop_volt;
	/* Process voltage variables */
	u32			process_id;
	u32			foundry_id;

	/* GPU voltage regulator */
	struct regulator	*vdd_gfx;

	/* Dependency parameters */
	struct regulator	*vdd_mx;
	int			vdd_mx_vmin;
	int			*vdd_mx_corner_map;

	/* mem-acc regulator */
	struct regulator	*mem_acc_vreg;

	/* CPR parameters */
	bool			cpr_fuse_disable;
	int			cpr_fuse_revision;
	int			cpr_fuse_map_count;
	int			cpr_fuse_map_match;
	int			**cpr_target_quot;
	int			gcnt;

	unsigned int		cpr_irq;
	void __iomem		*rbcpr_base;
	struct mutex		cpr_mutex;

	int			*ceiling_volt;
	int			*floor_volt;
	int			*last_volt;
	int			step_volt;

	int			*save_ctl;
	int			*save_irq;

	/* Config parameters */
	bool			enable;
	u32			ref_clk_khz;
	u32			timer_delay_us;
	u32			timer_cons_up;
	u32			timer_cons_down;
	u32			irq_line;
	u32			step_quotient;
	u32			up_threshold;
	u32			down_threshold;
	u32			idle_clocks;
	u32			gcnt_time_us;
	u32			vdd_gfx_step_up_limit;
	u32			vdd_gfx_step_down_limit;
	u32			flags;
	u32			ro_count;
	u32			num_corners;

	bool			is_cpr_suspended;
	bool			ctrl_enable;
};

#define CPR_DEBUG_MASK_IRQ	BIT(0)
#define CPR_DEBUG_MASK_API	BIT(1)

static int cpr_debug_enable;
static struct dentry *cpr2_gfx_debugfs_base;

static DEFINE_MUTEX(cpr2_gfx_regulator_list_mutex);
static LIST_HEAD(cpr2_gfx_regulator_list);

module_param_named(debug_enable, cpr_debug_enable, int, S_IRUGO | S_IWUSR);
#define cpr_debug(cpr_vreg, message, ...) \
	do { \
		if (cpr_debug_enable & CPR_DEBUG_MASK_API) \
			pr_info("%s: " message, (cpr_vreg)->rdesc.name, \
				##__VA_ARGS__); \
	} while (0)
#define cpr_debug_irq(cpr_vreg, message, ...) \
	do { \
		if (cpr_debug_enable & CPR_DEBUG_MASK_IRQ) \
			pr_info("%s: " message, (cpr_vreg)->rdesc.name, \
				##__VA_ARGS__); \
		else \
			pr_debug("%s: " message, (cpr_vreg)->rdesc.name, \
				##__VA_ARGS__); \
	} while (0)
#define cpr_info(cpr_vreg, message, ...) \
	pr_info("%s: " message, (cpr_vreg)->rdesc.name, ##__VA_ARGS__)
#define cpr_err(cpr_vreg, message, ...) \
	pr_err("%s: " message, (cpr_vreg)->rdesc.name, ##__VA_ARGS__)

static u64 cpr_read_efuse_row(struct cpr2_gfx_regulator *cpr_vreg, u32 row_num)
{
	u64 efuse_bits;

	efuse_bits = readq_relaxed(cpr_vreg->efuse_base
			+ row_num * BYTES_PER_FUSE_ROW);
	return efuse_bits;
}

/**
 * cpr_read_efuse_param() - read a parameter from one or two eFuse rows
 * @cpr_vreg:	Pointer to cpr2_gfx_regulator struct for this regulator.
 * @row_start:	Fuse row number to start reading from.
 * @bit_start:	The LSB of the parameter to read from the fuse.
 * @bit_len:	The length of the parameter in bits.
 *
 * This function reads a parameter of specified offset and bit size out of one
 * or two consecutive eFuse rows.  This allows for the reading of parameters
 * that happen to be split between two eFuse rows.
 *
 * Returns the fuse parameter on success or 0 on failure.
 */
static u64 cpr_read_efuse_param(struct cpr2_gfx_regulator *cpr_vreg,
				int row_start, int bit_start, int bit_len)
{
	u64 fuse[2];
	u64 param = 0;
	int bits_first, bits_second;

	if (bit_start < 0) {
		cpr_err(cpr_vreg, "Invalid LSB = %d specified\n", bit_start);
		return 0;
	}

	if (bit_len < 0 || bit_len > 64) {
		cpr_err(cpr_vreg, "Invalid bit length = %d specified\n",
			bit_len);
		return 0;
	}

	/* Allow bit indexing to start beyond the end of the start row. */
	if (bit_start >= 64) {
		row_start += bit_start >> 6; /* equivalent to bit_start / 64 */
		bit_start &= 0x3F;
	}

	fuse[0] = cpr_read_efuse_row(cpr_vreg, row_start);

	if (bit_start == 0 && bit_len == 64) {
		param = fuse[0];
	} else if (bit_start + bit_len <= 64) {
		param = (fuse[0] >> bit_start) & ((1ULL << bit_len) - 1);
	} else {
		fuse[1] = cpr_read_efuse_row(cpr_vreg, row_start + 1);
		bits_first = 64 - bit_start;
		bits_second = bit_len - bits_first;
		param = (fuse[0] >> bit_start) & ((1ULL << bits_first) - 1);
		param |= (fuse[1] & ((1ULL << bits_second) - 1)) << bits_first;
	}

	return param;
}

static bool cpr_is_allowed(struct cpr2_gfx_regulator *cpr_vreg)
{
	if (cpr_vreg->cpr_fuse_disable || !cpr_vreg->enable)
		return false;
	else
		return true;
}

static void cpr_write(struct cpr2_gfx_regulator *cpr_vreg, u32 offset,
			u32 value)
{
	writel_relaxed(value, cpr_vreg->rbcpr_base + offset);
}

static u32 cpr_read(struct cpr2_gfx_regulator *cpr_vreg, u32 offset)
{
	return readl_relaxed(cpr_vreg->rbcpr_base + offset);
}

static void cpr_masked_write(struct cpr2_gfx_regulator *cpr_vreg, u32 offset,
			     u32 mask, u32 value)
{
	u32 reg_val;

	reg_val = readl_relaxed(cpr_vreg->rbcpr_base + offset);
	reg_val &= ~mask;
	reg_val |= value & mask;
	writel_relaxed(reg_val, cpr_vreg->rbcpr_base + offset);
}

static void cpr_irq_clr(struct cpr2_gfx_regulator *cpr_vreg)
{
	if (cpr_vreg->ctrl_enable)
		cpr_write(cpr_vreg, REG_RBIF_IRQ_CLEAR, CPR_INT_ALL);
}

static void cpr_irq_clr_nack(struct cpr2_gfx_regulator *cpr_vreg)
{
	cpr_irq_clr(cpr_vreg);
	cpr_write(cpr_vreg, REG_RBIF_CONT_NACK_CMD, 1);
}

static void cpr_irq_clr_ack(struct cpr2_gfx_regulator *cpr_vreg)
{
	cpr_irq_clr(cpr_vreg);
	cpr_write(cpr_vreg, REG_RBIF_CONT_ACK_CMD, 1);
}

static void cpr_irq_set(struct cpr2_gfx_regulator *cpr_vreg, u32 int_bits)
{
	if (cpr_vreg->ctrl_enable)
		cpr_write(cpr_vreg, REG_RBIF_IRQ_EN(cpr_vreg->irq_line),
			int_bits);
}

static void cpr_ctl_modify(struct cpr2_gfx_regulator *cpr_vreg, u32 mask,
				u32 value)
{
	cpr_masked_write(cpr_vreg, REG_RBCPR_CTL, mask, value);
}

static void cpr_ctl_enable(struct cpr2_gfx_regulator *cpr_vreg, int corner)
{
	u32 val;

	if (cpr_vreg->is_cpr_suspended || !cpr_vreg->ctrl_enable)
		return;

	/* Program Consecutive Up & Down */
	val = ((cpr_vreg->timer_cons_down & RBIF_TIMER_ADJ_CONS_DOWN_MASK)
			<< RBIF_TIMER_ADJ_CONS_DOWN_SHIFT) |
		(cpr_vreg->timer_cons_up & RBIF_TIMER_ADJ_CONS_UP_MASK);
	cpr_masked_write(cpr_vreg, REG_RBIF_TIMER_ADJUST,
			RBIF_TIMER_ADJ_CONS_UP_MASK |
			RBIF_TIMER_ADJ_CONS_DOWN_MASK, val);
	cpr_masked_write(cpr_vreg, REG_RBCPR_CTL,
			RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN |
			RBCPR_CTL_SW_AUTO_CONT_ACK_EN,
			cpr_vreg->save_ctl[corner]);
	cpr_irq_set(cpr_vreg, cpr_vreg->save_irq[corner]);

	if (cpr_vreg->ceiling_volt[corner] > cpr_vreg->floor_volt[corner])
		val = RBCPR_CTL_LOOP_EN;
	else
		val = 0;
	cpr_ctl_modify(cpr_vreg, RBCPR_CTL_LOOP_EN, val);
}

static void cpr_ctl_disable(struct cpr2_gfx_regulator *cpr_vreg)
{
	if (cpr_vreg->is_cpr_suspended || !cpr_vreg->ctrl_enable)
		return;

	cpr_irq_set(cpr_vreg, 0);
	cpr_ctl_modify(cpr_vreg, RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN |
			RBCPR_CTL_SW_AUTO_CONT_ACK_EN, 0);
	cpr_masked_write(cpr_vreg, REG_RBIF_TIMER_ADJUST,
			RBIF_TIMER_ADJ_CONS_UP_MASK |
			RBIF_TIMER_ADJ_CONS_DOWN_MASK, 0);
	cpr_irq_clr(cpr_vreg);
	cpr_write(cpr_vreg, REG_RBIF_CONT_ACK_CMD, 1);
	cpr_write(cpr_vreg, REG_RBIF_CONT_NACK_CMD, 1);
	cpr_ctl_modify(cpr_vreg, RBCPR_CTL_LOOP_EN, 0);
}

static bool cpr_ctl_is_enabled(struct cpr2_gfx_regulator *cpr_vreg)
{
	u32 reg_val;

	reg_val = cpr_read(cpr_vreg, REG_RBCPR_CTL);
	return reg_val & RBCPR_CTL_LOOP_EN;
}

static bool cpr_ctl_is_busy(struct cpr2_gfx_regulator *cpr_vreg)
{
	u32 reg_val;

	reg_val = cpr_read(cpr_vreg, REG_RBCPR_RESULT_0);
	return reg_val & RBCPR_RESULT0_BUSY_MASK;
}

static void cpr_corner_save(struct cpr2_gfx_regulator *cpr_vreg, int corner)
{
	cpr_vreg->save_ctl[corner] = cpr_read(cpr_vreg, REG_RBCPR_CTL);
	cpr_vreg->save_irq[corner] =
		cpr_read(cpr_vreg, REG_RBIF_IRQ_EN(cpr_vreg->irq_line));
}

#define MAX_CHARS_PER_INT	10

static void cpr_corner_restore(struct cpr2_gfx_regulator *cpr_vreg, int corner)
{
	u32 gcnt, ctl, irq, step_quot;
	int i;

	if (!cpr_vreg->ctrl_enable)
		return;

	/* Program the step quotient and idle clocks */
	step_quot = ((cpr_vreg->idle_clocks & RBCPR_STEP_QUOT_IDLE_CLK_MASK)
			<< RBCPR_STEP_QUOT_IDLE_CLK_SHIFT) |
		(cpr_vreg->step_quotient & RBCPR_STEP_QUOT_STEPQUOT_MASK);
	cpr_write(cpr_vreg, REG_RBCPR_STEP_QUOT, step_quot);

	/* Program the target quotient value and gate count of all ROs */
	for (i = 0; i < cpr_vreg->ro_count; i++) {
		gcnt = cpr_vreg->gcnt
				| (cpr_vreg->cpr_target_quot[corner][i]);
		cpr_write(cpr_vreg, REG_RBCPR_GCNT_TARGET(i), gcnt);
	}

	ctl = cpr_vreg->save_ctl[corner];
	cpr_write(cpr_vreg, REG_RBCPR_CTL, ctl);
	irq = cpr_vreg->save_irq[corner];
	cpr_irq_set(cpr_vreg, irq);
	cpr_debug(cpr_vreg, "ctl = 0x%08x, irq = 0x%08x\n", ctl, irq);
}

static void cpr_corner_switch(struct cpr2_gfx_regulator *cpr_vreg, int corner)
{
	if (cpr_vreg->corner == corner)
		return;

	cpr_corner_restore(cpr_vreg, corner);
}

static int cpr_gfx_set(struct cpr2_gfx_regulator *cpr_vreg, u32 new_volt)
{
	int max_volt, rc;

	max_volt = cpr_vreg->ceiling_max;
	rc = regulator_set_voltage(cpr_vreg->vdd_gfx, new_volt, max_volt);
	if (rc)
		cpr_err(cpr_vreg, "set: vdd_gfx = %d uV: rc=%d\n",
			new_volt, rc);
	return rc;
}

static int cpr_mx_set(struct cpr2_gfx_regulator *cpr_vreg, int corner,
		      int vdd_mx_vmin)
{
	int rc, max_uV = INT_MAX;

	rc = regulator_set_voltage(cpr_vreg->vdd_mx, vdd_mx_vmin, max_uV);
	cpr_debug(cpr_vreg, "[corner:%d] %d uV\n", corner, vdd_mx_vmin);

	if (!rc)
		cpr_vreg->vdd_mx_vmin = vdd_mx_vmin;
	else
		cpr_err(cpr_vreg, "set: vdd_mx [corner:%d] = %d uV failed: rc=%d\n",
			corner, vdd_mx_vmin, rc);
	return rc;
}

static int cpr2_gfx_scale_voltage(struct cpr2_gfx_regulator *cpr_vreg,
					int corner, int new_gfx_volt,
					enum voltage_change_dir dir)
{
	int rc = 0, vdd_mx_vmin = 0;

	/* Determine the vdd_mx voltage */
	if (dir != NO_CHANGE && cpr_vreg->vdd_mx != NULL)
		vdd_mx_vmin = cpr_vreg->vdd_mx_corner_map[corner];

	if (cpr_vreg->mem_acc_vreg && dir == DOWN) {
		rc = regulator_set_voltage(cpr_vreg->mem_acc_vreg,
					corner, corner);
		if (rc)
			cpr_err(cpr_vreg, "set: mem_acc corner:%d failed: rc=%d\n",
				corner, rc);
	}

	if (!rc && vdd_mx_vmin && dir == UP) {
		if (vdd_mx_vmin != cpr_vreg->vdd_mx_vmin)
			rc = cpr_mx_set(cpr_vreg, corner, vdd_mx_vmin);
	}

	if (!rc)
		rc = cpr_gfx_set(cpr_vreg, new_gfx_volt);

	if (!rc && cpr_vreg->mem_acc_vreg && dir == UP) {
		rc = regulator_set_voltage(cpr_vreg->mem_acc_vreg, corner,
						corner);
		if (rc)
			cpr_err(cpr_vreg, "set: mem_acc corner:%d failed: rc=%d\n",
				corner, rc);
	}

	if (!rc && vdd_mx_vmin && dir == DOWN) {
		if (vdd_mx_vmin != cpr_vreg->vdd_mx_vmin)
			rc = cpr_mx_set(cpr_vreg, corner, vdd_mx_vmin);
	}

	return rc;
}

static void cpr2_gfx_scale(struct cpr2_gfx_regulator *cpr_vreg,
		      enum voltage_change_dir dir)
{
	u32 reg_val, error_steps, reg_mask, gcnt;
	int last_volt, new_volt, corner, i, pos;
	size_t buf_len;
	char *buf;

	corner = cpr_vreg->corner;

	reg_val = cpr_read(cpr_vreg, REG_RBCPR_RESULT_0);

	error_steps = (reg_val >> RBCPR_RESULT0_ERROR_STEPS_SHIFT)
				& RBCPR_RESULT0_ERROR_STEPS_MASK;
	last_volt = cpr_vreg->last_volt[corner];

	cpr_debug_irq(cpr_vreg, "last_volt[corner:%d] = %d uV\n", corner,
			last_volt);

	buf_len = cpr_vreg->ro_count * (MAX_CHARS_PER_INT + 2) * sizeof(*buf);
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (buf == NULL) {
		cpr_err(cpr_vreg, "Could not allocate memory for target register logging\n");
		return;
	}

	for (i = 0, pos = 0; i < cpr_vreg->ro_count; i++) {
		gcnt = cpr_read(cpr_vreg, REG_RBCPR_GCNT_TARGET(i));
		pos += scnprintf(buf + pos, buf_len - pos, "%u%s", gcnt,
				i < cpr_vreg->ro_count - 1 ? " " : "");
	}

	if (dir == UP) {
		cpr_debug_irq(cpr_vreg,
				"Up: cpr status = 0x%08x (error_steps=%d)\n",
				reg_val, error_steps);

		if (last_volt >= cpr_vreg->ceiling_volt[corner]) {
			cpr_debug_irq(cpr_vreg,
					"[corn:%d] @ ceiling: %d >= %d: NACK\n",
					corner, last_volt,
					cpr_vreg->ceiling_volt[corner]);
			cpr_irq_clr_nack(cpr_vreg);

			cpr_debug_irq(cpr_vreg, "gcnt target dump: [%s]\n",
					buf);

			/* Maximize the UP threshold */
			reg_mask = RBCPR_CTL_UP_THRESHOLD_MASK <<
					RBCPR_CTL_UP_THRESHOLD_SHIFT;
			reg_val = reg_mask;
			cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

			/* Disable UP interrupt */
			cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT & ~CPR_INT_UP);

			goto _exit;
		}

		if (error_steps > cpr_vreg->vdd_gfx_step_up_limit) {
			cpr_debug_irq(cpr_vreg,
				      "%d is over up-limit(%d): Clamp\n",
				      error_steps,
				      cpr_vreg->vdd_gfx_step_up_limit);
			error_steps = cpr_vreg->vdd_gfx_step_up_limit;
		}

		/* Calculate new voltage */
		new_volt = last_volt + (error_steps * cpr_vreg->step_volt);
		if (new_volt > cpr_vreg->ceiling_volt[corner]) {
			cpr_debug_irq(cpr_vreg,
				      "new_volt(%d) >= ceiling(%d): Clamp\n",
				      new_volt,
				      cpr_vreg->ceiling_volt[corner]);

			new_volt = cpr_vreg->ceiling_volt[corner];
		}

		if (cpr2_gfx_scale_voltage(cpr_vreg, corner, new_volt, dir)) {
			cpr_irq_clr_nack(cpr_vreg);
			goto _exit;
		}
		cpr_vreg->last_volt[corner] = new_volt;

		/* Disable auto nack down */
		reg_mask = RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;
		reg_val = 0;

		cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

		/* Re-enable default interrupts */
		cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT);

		/* Ack */
		cpr_irq_clr_ack(cpr_vreg);

		cpr_debug_irq(cpr_vreg, "UP: -> new_volt[corner:%d] = %d uV\n",
			corner, new_volt);
	} else if (dir == DOWN) {
		cpr_debug_irq(cpr_vreg,
			      "Down: cpr status = 0x%08x (error_steps=%d)\n",
			      reg_val, error_steps);

		if (last_volt <= cpr_vreg->floor_volt[corner]) {
			cpr_debug_irq(cpr_vreg,
					"[corn:%d] @ floor: %d <= %d: NACK\n",
					corner, last_volt,
					cpr_vreg->floor_volt[corner]);
			cpr_irq_clr_nack(cpr_vreg);

			cpr_debug_irq(cpr_vreg, "gcnt target dump: [%s]\n",
					buf);

			/* Enable auto nack down */
			reg_mask = RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;
			reg_val = RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;

			cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

			/* Disable DOWN interrupt */
			cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT & ~CPR_INT_DOWN);

			goto _exit;
		}

		if (error_steps > cpr_vreg->vdd_gfx_step_down_limit) {
			cpr_debug_irq(cpr_vreg,
					"%d is over down-limit(%d): Clamp\n",
					error_steps,
					cpr_vreg->vdd_gfx_step_down_limit);
			error_steps = cpr_vreg->vdd_gfx_step_down_limit;
		}

		/* Calculte new voltage */
		new_volt = last_volt - (error_steps * cpr_vreg->step_volt);
		if (new_volt < cpr_vreg->floor_volt[corner]) {
			cpr_debug_irq(cpr_vreg,
					"new_volt(%d) < floor(%d): Clamp\n",
					new_volt, cpr_vreg->floor_volt[corner]);
			new_volt = cpr_vreg->floor_volt[corner];
		}

		if (cpr2_gfx_scale_voltage(cpr_vreg, corner, new_volt, dir)) {
			cpr_irq_clr_nack(cpr_vreg);
			goto _exit;
		}
		cpr_vreg->last_volt[corner] = new_volt;

		/* Restore default threshold for UP */
		reg_mask = RBCPR_CTL_UP_THRESHOLD_MASK <<
				RBCPR_CTL_UP_THRESHOLD_SHIFT;
		reg_val = cpr_vreg->up_threshold <<
				RBCPR_CTL_UP_THRESHOLD_SHIFT;
		cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

		/* Re-enable default interrupts */
		cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT);

		/* Ack */
		cpr_irq_clr_ack(cpr_vreg);

		cpr_debug_irq(cpr_vreg,
				"DOWN: -> new_volt[corner:%d] = %d uV\n",
				corner, new_volt);
	}

_exit:
	kfree(buf);
}

static irqreturn_t cpr2_gfx_irq_handler(int irq, void *dev)
{
	struct cpr2_gfx_regulator *cpr_vreg = dev;
	u32 reg_val;

	mutex_lock(&cpr_vreg->cpr_mutex);

	reg_val = cpr_read(cpr_vreg, REG_RBIF_IRQ_STATUS);
	if (cpr_vreg->flags & FLAGS_IGNORE_1ST_IRQ_STATUS)
		reg_val = cpr_read(cpr_vreg, REG_RBIF_IRQ_STATUS);

	cpr_debug_irq(cpr_vreg, "IRQ_STATUS = 0x%02X\n", reg_val);

	if (!cpr_ctl_is_enabled(cpr_vreg)) {
		cpr_debug_irq(cpr_vreg, "CPR is disabled\n");
		goto _exit;
	} else if (cpr_ctl_is_busy(cpr_vreg)) {
		cpr_debug_irq(cpr_vreg, "CPR measurement is not ready\n");
		goto _exit;
	} else if (!cpr_is_allowed(cpr_vreg)) {
		reg_val = cpr_read(cpr_vreg, REG_RBCPR_CTL);
		cpr_err(cpr_vreg, "Interrupt broken? RBCPR_CTL = 0x%02X\n",
			reg_val);
		goto _exit;
	}

	/* Following sequence of handling is as per each IRQ's priority */
	if (reg_val & CPR_INT_UP) {
		cpr2_gfx_scale(cpr_vreg, UP);
	} else if (reg_val & CPR_INT_DOWN) {
		cpr2_gfx_scale(cpr_vreg, DOWN);
	} else if (reg_val & CPR_INT_MIN) {
		cpr_irq_clr_nack(cpr_vreg);
	} else if (reg_val & CPR_INT_MAX) {
		cpr_irq_clr_nack(cpr_vreg);
	} else if (reg_val & CPR_INT_MID) {
		/* RBCPR_CTL_SW_AUTO_CONT_ACK_EN is enabled */
		cpr_debug_irq(cpr_vreg, "IRQ occurred for Mid Flag\n");
	} else {
		cpr_debug_irq(cpr_vreg,
			"IRQ occurred for unknown flag (0x%08x)\n", reg_val);
	}

	/* Save register values for the corner */
	cpr_corner_save(cpr_vreg, cpr_vreg->corner);

_exit:
	mutex_unlock(&cpr_vreg->cpr_mutex);
	return IRQ_HANDLED;
}

/**
 * cpr2_gfx_clock_enable() - prepare and enable all clocks used by this CPR GFX
 *			controller
 * @cpr_verg:		Pointer to the cpr2 gfx controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr2_gfx_clock_enable(struct cpr2_gfx_regulator *cpr_vreg)
{
	int rc;

	if (cpr_vreg->iface_clk) {
		rc = clk_prepare_enable(cpr_vreg->iface_clk);
		if (rc) {
			cpr_err(cpr_vreg, "failed to enable interface clock, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (cpr_vreg->core_clk) {
		rc = clk_prepare_enable(cpr_vreg->core_clk);
		if (rc) {
			cpr_err(cpr_vreg, "failed to enable core clock, rc=%d\n",
				rc);
			clk_disable_unprepare(cpr_vreg->iface_clk);
			return rc;
		}
	}

	return 0;
}

/**
 * cpr2_gfx_clock_disable() - disable and unprepare all clocks used by this CPR
 *			GFX controller
 * @cpr_vreg:		Pointer to the CPR2 controller
 *
 * Return: none
 */
static void cpr2_gfx_clock_disable(struct cpr2_gfx_regulator *cpr_vreg)
{
	if (cpr_vreg->core_clk)
		clk_disable_unprepare(cpr_vreg->core_clk);

	if (cpr_vreg->iface_clk)
		clk_disable_unprepare(cpr_vreg->iface_clk);
}

static int cpr2_gfx_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct cpr2_gfx_regulator *cpr_vreg = rdev_get_drvdata(rdev);

	return cpr_vreg->vreg_enabled;
}

/**
 * cpr2_gfx_closed_loop_enable() - enable logical CPR closed-loop operation
 * @cpr_vreg:	Pointer to the cpr2 gfx regulator
 *
 * Return: 0 on success, error on failure
 */
static inline int cpr2_gfx_closed_loop_enable(struct cpr2_gfx_regulator
						*cpr_vreg)
{
	int rc = 0;

	if (!cpr_is_allowed(cpr_vreg)) {
		return -EPERM;
	} else if (cpr_vreg->ctrl_enable) {
		/* Already enabled */
		return 0;
	} else if (cpr_vreg->is_cpr_suspended) {
		/* CPR must remain disabled as the system is entering suspend */
		return 0;
	}

	rc = cpr2_gfx_clock_enable(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "unable to enable CPR clocks, rc=%d\n",
			rc);
		return rc;
	}

	cpr_vreg->ctrl_enable = true;
	cpr_debug(cpr_vreg, "CPR closed-loop operation enabled\n");

	return 0;
}

/**
 * cpr2_gfx_closed_loop_disable() - disable logical CPR closed-loop operation
 * @cpr_vreg:	Pointer to the cpr2 gfx regulator
 *
 * Return: 0 on success, error on failure
 */
static inline int cpr2_gfx_closed_loop_disable(struct cpr2_gfx_regulator
						*cpr_vreg)
{
	if (!cpr_vreg->ctrl_enable) {
		/* Already disabled */
		return 0;
	}

	cpr2_gfx_clock_disable(cpr_vreg);
	cpr_vreg->ctrl_enable = false;
	cpr_debug(cpr_vreg, "CPR closed-loop operation disabled\n");

	return 0;
}

static int cpr2_gfx_regulator_enable(struct regulator_dev *rdev)
{
	struct cpr2_gfx_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	/* Enable dependency power before vdd_gfx */
	if (cpr_vreg->vdd_mx) {
		rc = regulator_enable(cpr_vreg->vdd_mx);
		if (rc) {
			cpr_err(cpr_vreg, "regulator_enable: vdd_mx: rc=%d\n",
				rc);
			return rc;
		}
	}

	rc = regulator_enable(cpr_vreg->vdd_gfx);
	if (rc) {
		cpr_err(cpr_vreg, "regulator_enable: vdd_gfx: rc=%d\n", rc);
		return rc;
	}

	mutex_lock(&cpr_vreg->cpr_mutex);
	cpr_vreg->vreg_enabled = true;
	if (cpr_is_allowed(cpr_vreg)) {
		rc = cpr2_gfx_closed_loop_enable(cpr_vreg);
		if (rc) {
			cpr_err(cpr_vreg, "could not enable CPR, rc=%d\n", rc);
			goto _exit;
		}

		if (cpr_vreg->corner) {
			cpr_irq_clr(cpr_vreg);
			cpr_corner_restore(cpr_vreg, cpr_vreg->corner);
			cpr_ctl_enable(cpr_vreg, cpr_vreg->corner);
		}
	}

	cpr_debug(cpr_vreg, "cpr_enable = %s cpr_corner = %d\n",
		cpr_vreg->enable ? "enabled" : "disabled",
		cpr_vreg->corner);
_exit:
	mutex_unlock(&cpr_vreg->cpr_mutex);
	return 0;
}

static int cpr2_gfx_regulator_disable(struct regulator_dev *rdev)
{
	struct cpr2_gfx_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = regulator_disable(cpr_vreg->vdd_gfx);
	if (!rc) {
		if (cpr_vreg->vdd_mx) {
			rc = regulator_disable(cpr_vreg->vdd_mx);
			if (rc) {
				cpr_err(cpr_vreg, "regulator_disable: vdd_mx: rc=%d\n",
					rc);
				return rc;
			}
		}

		mutex_lock(&cpr_vreg->cpr_mutex);
		cpr_vreg->vreg_enabled = false;
		if (cpr_is_allowed(cpr_vreg)) {
			cpr_ctl_disable(cpr_vreg);
			cpr2_gfx_closed_loop_disable(cpr_vreg);
		}
		mutex_unlock(&cpr_vreg->cpr_mutex);
	} else {
		cpr_err(cpr_vreg, "regulator_disable: vdd_gfx: rc=%d\n", rc);
	}

	cpr_debug(cpr_vreg, "cpr_enable = %s\n",
		cpr_vreg->enable ? "enabled" : "disabled");
	return rc;
}

static int cpr2_gfx_regulator_set_voltage(struct regulator_dev *rdev,
		int corner, int corner_max, unsigned *selector)
{
	struct cpr2_gfx_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	int new_volt;
	enum voltage_change_dir change_dir = NO_CHANGE;

	mutex_lock(&cpr_vreg->cpr_mutex);

	if (cpr_vreg->ctrl_enable) {
		cpr_ctl_disable(cpr_vreg);
		new_volt = cpr_vreg->last_volt[corner];
	} else {
		new_volt = cpr_vreg->open_loop_volt[corner];
	}

	cpr_debug(cpr_vreg, "[corner:%d] = %d uV\n", corner, new_volt);

	if (corner > cpr_vreg->corner)
		change_dir = UP;
	else if (corner < cpr_vreg->corner)
		change_dir = DOWN;

	rc = cpr2_gfx_scale_voltage(cpr_vreg, corner, new_volt, change_dir);
	if (rc)
		goto _exit;

	if (cpr_vreg->ctrl_enable) {
		cpr_irq_clr(cpr_vreg);
		cpr_corner_switch(cpr_vreg, corner);
		cpr_ctl_enable(cpr_vreg, corner);
	}

	cpr_vreg->corner = corner;

_exit:
	mutex_unlock(&cpr_vreg->cpr_mutex);
	return rc;
}

static int cpr2_gfx_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct cpr2_gfx_regulator *cpr_vreg = rdev_get_drvdata(rdev);

	return cpr_vreg->corner;
}

static struct regulator_ops cpr_corner_ops = {
	.enable			= cpr2_gfx_regulator_enable,
	.disable		= cpr2_gfx_regulator_disable,
	.is_enabled		= cpr2_gfx_regulator_is_enabled,
	.set_voltage		= cpr2_gfx_regulator_set_voltage,
	.get_voltage		= cpr2_gfx_regulator_get_voltage,
};

static int cpr2_gfx_regulator_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct cpr2_gfx_regulator *cpr_vreg = platform_get_drvdata(pdev);

	mutex_lock(&cpr_vreg->cpr_mutex);

	cpr_debug(cpr_vreg, "suspend\n");

	if (cpr_vreg->vreg_enabled && cpr_is_allowed(cpr_vreg)) {
		cpr_ctl_disable(cpr_vreg);
		cpr_irq_clr(cpr_vreg);
		cpr2_gfx_closed_loop_disable(cpr_vreg);
	}

	cpr_vreg->is_cpr_suspended = true;

	mutex_unlock(&cpr_vreg->cpr_mutex);

	return 0;
}

static int cpr2_gfx_regulator_resume(struct platform_device *pdev)
{
	struct cpr2_gfx_regulator *cpr_vreg = platform_get_drvdata(pdev);
	int rc = 0;

	mutex_lock(&cpr_vreg->cpr_mutex);

	cpr_vreg->is_cpr_suspended = false;
	cpr_debug(cpr_vreg, "resume\n");

	if (cpr_vreg->vreg_enabled && cpr_is_allowed(cpr_vreg)) {
		rc = cpr2_gfx_closed_loop_enable(cpr_vreg);
		if (rc)
			cpr_err(cpr_vreg, "could not enable CPR, rc=%d\n", rc);

		cpr_irq_clr(cpr_vreg);
		cpr_ctl_enable(cpr_vreg, cpr_vreg->corner);
	}

	mutex_unlock(&cpr_vreg->cpr_mutex);
	return 0;
}

static int cpr2_gfx_allocate_memory(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device *dev = cpr_vreg->dev;
	int rc, i;
	size_t len;

	rc = of_property_read_u32(dev->of_node, "qcom,cpr-corners",
						&cpr_vreg->num_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "qcom,cpr-corners missing: rc=%d\n", rc);
		return rc;
	}

	if (cpr_vreg->num_corners < CPR_CORNER_MIN
		|| cpr_vreg->num_corners > CPR_CORNER_LIMIT) {
		cpr_err(cpr_vreg, "corner count=%d is invalid\n",
			cpr_vreg->num_corners);
		return -EINVAL;
	}

	rc = of_property_read_u32(dev->of_node, "qcom,cpr-ro-count",
					&cpr_vreg->ro_count);
	if (rc < 0) {
		cpr_err(cpr_vreg, "qcom,cpr-ro-count missing or read failed: rc=%d\n",
			rc);
		return rc;
	}
	cpr_info(cpr_vreg, "ro_count = %d\n", cpr_vreg->ro_count);

	/*
	 * The arrays sized based on the corner count ignore element 0
	 * in order to simplify indexing throughout the driver since min_uV = 0
	 * cannot be passed into a set_voltage() callback.
	 */
	len = cpr_vreg->num_corners + 1;

	cpr_vreg->open_loop_volt = devm_kzalloc(dev,
			len * sizeof(*cpr_vreg->open_loop_volt), GFP_KERNEL);
	cpr_vreg->cpr_target_quot = devm_kzalloc(dev,
			len  * sizeof(int *), GFP_KERNEL);
	cpr_vreg->ceiling_volt = devm_kzalloc(dev,
		len * (sizeof(*cpr_vreg->ceiling_volt)), GFP_KERNEL);
	cpr_vreg->floor_volt = devm_kzalloc(dev,
		len * (sizeof(*cpr_vreg->floor_volt)), GFP_KERNEL);

	if (cpr_vreg->open_loop_volt == NULL
		|| cpr_vreg->cpr_target_quot == NULL
		|| cpr_vreg->ceiling_volt == NULL
		|| cpr_vreg->floor_volt == NULL) {
		cpr_err(cpr_vreg, "Could not allocate memory for CPR arrays\n");
		return -ENOMEM;
	}

	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
		cpr_vreg->cpr_target_quot[i] = devm_kzalloc(dev,
			cpr_vreg->ro_count * sizeof(*cpr_vreg->cpr_target_quot),
			GFP_KERNEL);
		if (!cpr_vreg->cpr_target_quot[i]) {
			cpr_err(cpr_vreg, "Could not allocate memory\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static int cpr_mem_acc_init(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device *dev = cpr_vreg->dev;
	int rc;

	if (of_find_property(dev->of_node, "mem-acc-supply", NULL)) {
		cpr_vreg->mem_acc_vreg = devm_regulator_get(dev, "mem-acc");
		if (IS_ERR_OR_NULL(cpr_vreg->mem_acc_vreg)) {
			rc = PTR_RET(cpr_vreg->mem_acc_vreg);
			if (rc != -EPROBE_DEFER)
				cpr_err(cpr_vreg,
					"devm_regulator_get: mem-acc: rc=%d\n",
					rc);
			return rc;
		}
	}
	return 0;
}

static int cpr_efuse_init(struct platform_device *pdev,
				 struct cpr2_gfx_regulator *cpr_vreg)
{
	struct resource *res;
	int len;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse_addr");
	if (!res || !res->start) {
		cpr_err(cpr_vreg, "efuse_addr missing: res=%p\n", res);
		return -EINVAL;
	}

	cpr_vreg->efuse_addr = res->start;
	len = res->end - res->start + 1;

	cpr_info(cpr_vreg, "efuse_addr = %pa (len=0x%x)\n", &res->start, len);

	cpr_vreg->efuse_base = ioremap(cpr_vreg->efuse_addr, len);
	if (!cpr_vreg->efuse_base) {
		cpr_err(cpr_vreg, "Unable to map efuse_addr %pa\n",
				&cpr_vreg->efuse_addr);
		return -EINVAL;
	}

	return 0;
}

static int cpr_parse_fuse_parameters(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device *dev = cpr_vreg->dev;
	u32 fuse_sel[3];
	int rc;

	rc = of_property_read_u32_array(dev->of_node, "qcom,cpr-fuse-revision",
					fuse_sel, 3);
	if (rc < 0) {
		if (rc != -EINVAL) {
			cpr_err(cpr_vreg, "qcom,cpr-fuse-revision read failed: rc=%d\n",
				rc);
			return rc;
		} else {
			/* Property not exist; Assigning a wild card value */
			cpr_vreg->cpr_fuse_revision = FUSE_REVISION_UNKNOWN;
		}
	} else {
		cpr_vreg->cpr_fuse_revision = cpr_read_efuse_param(cpr_vreg,
					fuse_sel[0], fuse_sel[1], fuse_sel[2]);
		cpr_info(cpr_vreg, "fuse revision = %d\n",
					cpr_vreg->cpr_fuse_revision);
	}

	rc = of_property_read_u32_array(dev->of_node, "qcom,process-id-fuse",
					fuse_sel, 3);
	if (rc < 0) {
		if (rc != -EINVAL) {
			cpr_err(cpr_vreg, "qcom,process-id-fuse read failed: rc=%d\n",
				rc);
			return rc;
		} else {
			/* Property not exist; Assigning a wild card value */
			cpr_vreg->process_id = (INT_MAX - 1);
		}
	} else {
		cpr_vreg->process_id = cpr_read_efuse_param(cpr_vreg,
					fuse_sel[0], fuse_sel[1], fuse_sel[2]);
		cpr_info(cpr_vreg, "process id = %d\n", cpr_vreg->process_id);
	}

	rc = of_property_read_u32_array(dev->of_node, "qcom,foundry-id-fuse",
					fuse_sel, 3);
	if (rc < 0) {
		if (rc != -EINVAL) {
			cpr_err(cpr_vreg, "qcom,foundry-id-fuse read failed: rc=%d\n",
				rc);
			return rc;
		} else {
			/* Property not exist; Assigning a wild card value */
			cpr_vreg->foundry_id = (INT_MAX - 1);
		}
	} else {
		cpr_vreg->foundry_id
			= cpr_read_efuse_param(cpr_vreg, fuse_sel[0],
					fuse_sel[1], fuse_sel[2]);
		cpr_info(cpr_vreg, "foundry_id = %d\n", cpr_vreg->foundry_id);
	}

	return 0;
}

static int cpr_find_fuse_map_match(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	int i, rc, tuple_size;
	int len = 0;
	u32 *tmp;

	/* Specify default no match case. */
	cpr_vreg->cpr_fuse_map_match = FUSE_MAP_NO_MATCH;
	cpr_vreg->cpr_fuse_map_count = 0;

	if (!of_find_property(of_node, "qcom,cpr-fuse-version-map", &len)) {
		/* No mapping present. */
		return 0;
	}

	tuple_size = 3; /* <foundry_id> <cpr_fuse_revision> <process_id> */
	cpr_vreg->cpr_fuse_map_count = len / (sizeof(u32) * tuple_size);

	if (len == 0 || len % (sizeof(u32) * tuple_size)) {
		cpr_err(cpr_vreg, "qcom,cpr-fuse-version-map length=%d is invalid\n",
			len);
		return -EINVAL;
	}

	tmp = kzalloc(len, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,cpr-fuse-version-map",
				tmp, cpr_vreg->cpr_fuse_map_count * tuple_size);
	if (rc) {
		cpr_err(cpr_vreg, "could not read qcom,cpr-fuse-version-map, rc=%d\n",
			rc);
		goto done;
	}

	/*
	 * qcom,cpr-fuse-version-map tuple format:
	 * <foundry_id, cpr_fuse_revision process_id>
	 */
	for (i = 0; i < cpr_vreg->cpr_fuse_map_count; i++) {
		if (tmp[i * tuple_size] != cpr_vreg->foundry_id
		    && tmp[i * tuple_size] != FUSE_PARAM_MATCH_ANY)
			continue;
		if (tmp[i * tuple_size + 1] != cpr_vreg->cpr_fuse_revision
		    && tmp[i * tuple_size + 1] != FUSE_PARAM_MATCH_ANY)
			continue;
		if (tmp[i * tuple_size + 1] != cpr_vreg->process_id
		    && tmp[i * tuple_size + 1] != FUSE_PARAM_MATCH_ANY)
			continue;

		cpr_vreg->cpr_fuse_map_match = i;
		break;
	}

	if (cpr_vreg->cpr_fuse_map_match != FUSE_MAP_NO_MATCH)
		cpr_debug(cpr_vreg, "qcom,cpr-fuse-version-map tuple match found: %d\n",
			cpr_vreg->cpr_fuse_map_match);
	else
		cpr_debug(cpr_vreg, "qcom,cpr-fuse-version-map tuple match not found\n");

done:
	kfree(tmp);
	return rc;
}

static int cpr_voltage_plan_init(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	int highest_corner = cpr_vreg->num_corners;
	int rc;

	rc = of_property_read_u32_array(of_node, "qcom,cpr-voltage-ceiling",
		&cpr_vreg->ceiling_volt[CPR_CORNER_MIN], cpr_vreg->num_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "cpr-voltage-ceiling missing: rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cpr-voltage-floor",
				&cpr_vreg->floor_volt[CPR_CORNER_MIN],
				cpr_vreg->num_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "cpr-voltage-floor missing: rc=%d\n", rc);
		return rc;
	}

	cpr_vreg->ceiling_max
		= cpr_vreg->ceiling_volt[highest_corner];

	return 0;
}

static int cpr_adjust_init_voltages(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	int tuple_count, tuple_match, i;
	u32 index;
	u32 volt_adjust = 0;
	int len = 0;
	int rc = 0;

	if (!of_find_property(of_node, "qcom,cpr-init-voltage-adjustment",
				&len)) {
		/* No initial voltage adjustment needed. */
		return 0;
	}

	if (cpr_vreg->cpr_fuse_map_count) {
		if (cpr_vreg->cpr_fuse_map_match == FUSE_MAP_NO_MATCH) {
			/*
			 * No matching index to use for initial voltage
			 * adjustment.
			 */
			return 0;
		}
		tuple_count = cpr_vreg->cpr_fuse_map_count;
		tuple_match = cpr_vreg->cpr_fuse_map_match;
	} else {
		tuple_count = 1;
		tuple_match = 0;
	}

	if (len != cpr_vreg->num_corners * tuple_count * sizeof(u32)) {
		cpr_err(cpr_vreg, "qcom,cpr-init-voltage-adjustment length=%d is invalid\n",
			len);
		return -EINVAL;
	}

	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
		index = tuple_match * cpr_vreg->num_corners
				+ i - CPR_CORNER_MIN;
		rc = of_property_read_u32_index(of_node,
			"qcom,cpr-init-voltage-adjustment", index,
			&volt_adjust);
		if (rc) {
			cpr_err(cpr_vreg, "could not read qcom,cpr-init-voltage-adjustment index %u, rc=%d\n",
				index, rc);
			return rc;
		}

		if (volt_adjust) {
			cpr_vreg->open_loop_volt[i] += volt_adjust;
			cpr_info(cpr_vreg, "adjusted initial voltage[%d]: %d -> %d uV\n",
				i, cpr_vreg->open_loop_volt[i] - volt_adjust,
				cpr_vreg->open_loop_volt[i]);
		}
	}

	return rc;
}

static int cpr_pvs_init(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	u64 efuse_bits;
	int i, size, sign, steps, step_size_uv, rc, pos;
	u32 *fuse_sel, *tmp, *ref_uv;
	struct property *prop;
	size_t buflen;
	char *buf;

	rc = of_property_read_u32(of_node, "qcom,cpr-gfx-volt-step",
					&cpr_vreg->step_volt);
	if (rc < 0) {
		cpr_err(cpr_vreg, "read cpr-gfx-volt-step failed, rc = %d\n",
			rc);
		return rc;
	} else if (cpr_vreg->step_volt == 0) {
		cpr_err(cpr_vreg, "gfx voltage step size can't be set to 0.\n");
		return -EINVAL;
	}

	prop = of_find_property(of_node, "qcom,cpr-fuse-init-voltage", NULL);
	if (!prop) {
		cpr_err(cpr_vreg, "qcom,cpr-fuse-init-voltage is missing\n");
		return -EINVAL;
	}
	size = prop->length / sizeof(u32);
	if (size != cpr_vreg->num_corners * 3) {
		cpr_err(cpr_vreg,
			"fuse position for init voltages is invalid\n");
		return -EINVAL;
	}
	fuse_sel = kcalloc(size, sizeof(u32), GFP_KERNEL);
	if (!fuse_sel)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,cpr-fuse-init-voltage",
							fuse_sel, size);
	if (rc < 0) {
		cpr_err(cpr_vreg,
			"read cpr-fuse-init-voltage failed, rc = %d\n", rc);
		kfree(fuse_sel);
		return rc;
	}
	rc = of_property_read_u32(of_node, "qcom,cpr-init-voltage-step",
							&step_size_uv);
	if (rc < 0) {
		cpr_err(cpr_vreg,
			"read cpr-init-voltage-step failed, rc = %d\n", rc);
		kfree(fuse_sel);
		return rc;
	}

	ref_uv = kcalloc((cpr_vreg->num_corners + 1), sizeof(*ref_uv),
			GFP_KERNEL);
	if (!ref_uv) {
		kfree(fuse_sel);
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cpr-init-voltage-ref",
		&ref_uv[CPR_CORNER_MIN], cpr_vreg->num_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg,
			"read qcom,cpr-init-voltage-ref failed, rc = %d\n", rc);
		goto done;
	}

	tmp = fuse_sel;
	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
		efuse_bits = cpr_read_efuse_param(cpr_vreg, fuse_sel[0],
					fuse_sel[1], fuse_sel[2]);
		sign = (efuse_bits & (1 << (fuse_sel[2] - 1))) ? -1 : 1;
		steps = efuse_bits & ((1 << (fuse_sel[2] - 1)) - 1);
		cpr_vreg->open_loop_volt[i] =
				ref_uv[i] + sign * steps * step_size_uv;
		cpr_vreg->open_loop_volt[i] = DIV_ROUND_UP(
				cpr_vreg->open_loop_volt[i],
				cpr_vreg->step_volt) *
				cpr_vreg->step_volt;
		cpr_debug(cpr_vreg, "corner %d: sign = %d, steps = %d, volt = %d uV\n",
			i, sign, steps, cpr_vreg->open_loop_volt[i]);
		fuse_sel += 3;
	}

	rc = cpr_adjust_init_voltages(cpr_vreg);
	if (rc)
		goto done;

	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
		if (cpr_vreg->open_loop_volt[i]
		    > cpr_vreg->ceiling_volt[i]) {
			cpr_info(cpr_vreg, "Warning: initial voltage[%d] %d above ceiling %d\n",
				i, cpr_vreg->open_loop_volt[i],
				cpr_vreg->ceiling_volt[i]);
			cpr_vreg->open_loop_volt[i]
				= cpr_vreg->ceiling_volt[i];
		} else if (cpr_vreg->open_loop_volt[i] <
				cpr_vreg->floor_volt[i]) {
			cpr_info(cpr_vreg, "Warning: initial voltage[%d] %d below floor %d\n",
				i, cpr_vreg->open_loop_volt[i],
				cpr_vreg->floor_volt[i]);
			cpr_vreg->open_loop_volt[i]
				= cpr_vreg->floor_volt[i];
		}
	}

	/*
	 * Log ceiling, floor, and initial voltages since they are critical for
	 * all CPR debugging.
	 */
	buflen = cpr_vreg->num_corners * (MAX_CHARS_PER_INT + 2)
			* sizeof(*buf);
	buf = kzalloc(buflen, GFP_KERNEL);
	if (buf == NULL) {
		cpr_err(cpr_vreg, "Could not allocate memory for corner voltage logging\n");
		rc = -ENOMEM;
		goto done;
	}

	for (i = CPR_CORNER_MIN, pos = 0; i <= cpr_vreg->num_corners; i++)
		pos += scnprintf(buf + pos, buflen - pos, "%u%s",
				cpr_vreg->open_loop_volt[i],
				i < cpr_vreg->num_corners ? " " : "");
	cpr_info(cpr_vreg, "pvs voltage: [%s] uV\n", buf);

	for (i = CPR_CORNER_MIN, pos = 0; i <= cpr_vreg->num_corners; i++)
		pos += scnprintf(buf + pos, buflen - pos, "%d%s",
				cpr_vreg->ceiling_volt[i],
				i < cpr_vreg->num_corners ? " " : "");
	cpr_info(cpr_vreg, "ceiling voltage: [%s] uV\n", buf);

	for (i = CPR_CORNER_MIN, pos = 0; i <= cpr_vreg->num_corners; i++)
		pos += scnprintf(buf + pos, buflen - pos, "%d%s",
				cpr_vreg->floor_volt[i],
				i < cpr_vreg->num_corners ? " " : "");
	cpr_info(cpr_vreg, "floor voltage: [%s] uV\n", buf);

	kfree(buf);

done:
	kfree(tmp);
	kfree(ref_uv);

	return rc;
}

static int cpr_parse_vdd_mx_parameters(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	int rc, len, size;

	if (!of_find_property(of_node, "qcom,vdd-mx-corner-map", &len)) {
		cpr_err(cpr_vreg, "qcom,vdd-mx-corner-map missing");
		return -EINVAL;
	}

	size = len / sizeof(u32);
	if (size != cpr_vreg->num_corners) {
		cpr_err(cpr_vreg,
			"qcom,vdd-mx-corner-map length=%d is invalid: required:%u\n",
			size, cpr_vreg->num_corners);
		return -EINVAL;
	}

	cpr_vreg->vdd_mx_corner_map = devm_kzalloc(cpr_vreg->dev,
		(size + 1) * sizeof(*cpr_vreg->vdd_mx_corner_map),
			GFP_KERNEL);
	if (!cpr_vreg->vdd_mx_corner_map) {
		cpr_err(cpr_vreg,
			"Can't allocate memory for cpr_vreg->vdd_mx_corner_map\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node,
				"qcom,vdd-mx-corner-map",
				&cpr_vreg->vdd_mx_corner_map[1],
				cpr_vreg->num_corners);
	if (rc)
		cpr_err(cpr_vreg,
			"read qcom,vdd-mx-corner-map failed, rc = %d\n", rc);

	return rc;
}

static int cpr_gfx_init(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	int rc = 0;

	cpr_vreg->vdd_gfx = devm_regulator_get(cpr_vreg->dev, "vdd-gfx");
	rc = PTR_RET(cpr_vreg->vdd_gfx);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "devm_regulator_get: rc=%d\n", rc);
		return rc;
	}

	/* Check dependencies */
	if (of_find_property(of_node, "vdd-mx-supply", NULL)) {
		cpr_vreg->vdd_mx = devm_regulator_get(cpr_vreg->dev, "vdd-mx");
		if (IS_ERR_OR_NULL(cpr_vreg->vdd_mx)) {
			rc = PTR_RET(cpr_vreg->vdd_mx);
			if (rc != -EPROBE_DEFER)
				cpr_err(cpr_vreg, "devm_regulator_get: vdd_mx: rc=%d\n",
							rc);
			return rc;
		}

		rc = cpr_parse_vdd_mx_parameters(cpr_vreg);
		if (rc) {
			cpr_err(cpr_vreg, "parsing vdd_mx parameters failed: rc=%d\n",
				rc);
			return rc;
		}
	}

	return 0;
}

static int cpr_get_clock_handles(struct cpr2_gfx_regulator *cpr_vreg)
{
	int rc;

	cpr_vreg->core_clk = devm_clk_get(cpr_vreg->dev, "core_clk");
	if (IS_ERR(cpr_vreg->core_clk)) {
		rc = PTR_RET(cpr_vreg->core_clk);
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "unable to request core clock, rc=%d\n",
				rc);
		return rc;
	}

	cpr_vreg->iface_clk = devm_clk_get(cpr_vreg->dev, "iface_clk");
	if (IS_ERR(cpr_vreg->iface_clk)) {
		rc = PTR_RET(cpr_vreg->iface_clk);
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "unable to request interface clock, rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static int cpr_init_target_quotients(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	int rc, len, size, tuple_count, tuple_match, pos, i, j, k;
	char *buf, *target_quot_str = "qcom,cpr-target-quotients";
	size_t buflen;
	u32 index;
	int *temp;

	if (!of_find_property(of_node, target_quot_str, &len)) {
		cpr_err(cpr_vreg, "%s missing\n", target_quot_str);
		return -EINVAL;
	}

	if (cpr_vreg->cpr_fuse_map_count) {
		if (cpr_vreg->cpr_fuse_map_match == FUSE_MAP_NO_MATCH) {
			/*
			 * No matching index to use for initial voltage
			 * adjustment.
			 */
			return 0;
		}
		tuple_count = cpr_vreg->cpr_fuse_map_count;
		tuple_match = cpr_vreg->cpr_fuse_map_match;
	} else {
		tuple_count = 1;
		tuple_match = 0;
	}

	size = len / sizeof(u32);

	if (size != tuple_count * cpr_vreg->ro_count * cpr_vreg->num_corners) {
		cpr_err(cpr_vreg, "%s length=%d is invalid\n", target_quot_str,
			size);
		return -EINVAL;
	}

	temp = kcalloc(size, sizeof(int), GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, target_quot_str, temp, size);
	if (rc) {
		cpr_err(cpr_vreg, "failed to read %s, rc=%d\n",
					target_quot_str, rc);
		kfree(temp);
		return rc;
	}

	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
		index = tuple_match * cpr_vreg->num_corners
			* cpr_vreg->ro_count + i - CPR_CORNER_MIN;
		for (j = 0; j < cpr_vreg->ro_count; j++) {
			k = index * cpr_vreg->ro_count + j;
			cpr_vreg->cpr_target_quot[i][j] = temp[k];
		}
	}
	kfree(temp);
	/*
	 * Log per-virtual corner target quotients since they are useful for
	 * baseline CPR logging.
	 */
	buflen = cpr_vreg->ro_count * (MAX_CHARS_PER_INT + 2) * sizeof(*buf);
	buf = kzalloc(buflen, GFP_KERNEL);
	if (buf == NULL) {
		cpr_err(cpr_vreg, "Could not allocate memory for target quotient logging\n");
		return 0;
	}

	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
		pos = 0;
		for (j = 0; j < cpr_vreg->ro_count; j++)
			pos += scnprintf(buf + pos, buflen - pos, "%d%s",
				cpr_vreg->cpr_target_quot[i][j],
				j < cpr_vreg->ro_count ? " " : "\0");
		cpr_info(cpr_vreg, "Corner[%d]: Target quotients: %s\n",
				i, buf);
	}
	kfree(buf);

	for (j = 0; j < cpr_vreg->ro_count; j++) {
		for (i = CPR_CORNER_MIN + 1; i <= cpr_vreg->num_corners; i++) {
			if (cpr_vreg->cpr_target_quot[i][j]
					< cpr_vreg->cpr_target_quot[i - 1][j]) {
				cpr_vreg->cpr_fuse_disable = true;
				cpr_err(cpr_vreg, "invalid quotient values; permanently disabling CPR\n");
			}
		}
	}

	return rc;
}

/*
 * Conditionally reduce the per-virtual-corner ceiling voltages if certain
 * device tree flags are present.
 */
static int cpr_reduce_ceiling_voltage(struct cpr2_gfx_regulator *cpr_vreg)
{
	int i;

	if (!of_property_read_bool(cpr_vreg->dev->of_node,
				"qcom,cpr-init-voltage-as-ceiling"))
		return 0;

	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
		cpr_vreg->ceiling_volt[i] = cpr_vreg->open_loop_volt[i];
		cpr_debug(cpr_vreg, "lowered ceiling[%d] = %d uV\n",
			i, cpr_vreg->ceiling_volt[i]);
	}

	return 0;
}

static int cpr_init_cpr_voltages(struct cpr2_gfx_regulator *cpr_vreg)
{
	int i;
	int size = cpr_vreg->num_corners + 1;

	cpr_vreg->last_volt = devm_kzalloc(cpr_vreg->dev, sizeof(int) * size,
					 GFP_KERNEL);
	if (!cpr_vreg->last_volt)
		return -EINVAL;

	for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++)
		cpr_vreg->last_volt[i] = cpr_vreg->open_loop_volt[i];

	return 0;
}

#define CPR_PROP_READ_U32(cpr_vreg, of_node, cpr_property, cpr_config, rc) \
do {									\
	if (!rc) {							\
		rc = of_property_read_u32(of_node, cpr_property,	\
				cpr_config);				\
		if (rc) {						\
			cpr_err(cpr_vreg, "Missing " #cpr_property	\
				": rc = %d\n", rc);			\
		}							\
	}								\
} while (0)

static int cpr_init_cpr_parameters(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct device_node *of_node = cpr_vreg->dev->of_node;
	int rc = 0;

	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-ref-clk",
			  &cpr_vreg->ref_clk_khz, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-timer-delay",
			  &cpr_vreg->timer_delay_us, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-timer-cons-up",
			  &cpr_vreg->timer_cons_up, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-timer-cons-down",
			  &cpr_vreg->timer_cons_down, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-irq-line",
			  &cpr_vreg->irq_line, rc);
	if (rc)
		return rc;

	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-step-quotient",
			  &cpr_vreg->step_quotient, rc);
	if (rc)
		return rc;
	cpr_info(cpr_vreg, "step_quotient = %u\n", cpr_vreg->step_quotient);

	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-up-threshold",
			  &cpr_vreg->up_threshold, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-down-threshold",
			  &cpr_vreg->down_threshold, rc);
	if (rc)
		return rc;
	cpr_info(cpr_vreg, "up threshold = %u, down threshold = %u\n",
		cpr_vreg->up_threshold, cpr_vreg->down_threshold);

	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-idle-clocks",
			  &cpr_vreg->idle_clocks, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,cpr-gcnt-time",
			  &cpr_vreg->gcnt_time_us, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,vdd-gfx-step-up-limit",
			  &cpr_vreg->vdd_gfx_step_up_limit, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "qcom,vdd-gfx-step-down-limit",
			  &cpr_vreg->vdd_gfx_step_down_limit, rc);
	if (rc)
		return rc;

	/* Init module parameter with the DT value */
	cpr_vreg->enable = of_property_read_bool(of_node, "qcom,cpr-enable");
	cpr_info(cpr_vreg, "CPR is %s by default.\n",
		cpr_vreg->enable ? "enabled" : "disabled");

	return 0;
}

static int cpr_config(struct cpr2_gfx_regulator *cpr_vreg)
{
	int i, rc;
	u32 val, gcnt;
	int size;

	rc = clk_set_rate(cpr_vreg->core_clk, cpr_vreg->ref_clk_khz * 1000);
	if (rc) {
		cpr_err(cpr_vreg, "clk_set_rate(core_clk, %u) failed, rc=%d\n",
			cpr_vreg->ref_clk_khz, rc);
		return rc;
	}

	rc = cpr2_gfx_clock_enable(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "unable to enable CPR clocks, rc=%d\n", rc);
		return rc;
	}

	/* Disable interrupt and CPR */
	cpr_write(cpr_vreg, REG_RBIF_IRQ_EN(cpr_vreg->irq_line), 0);
	cpr_write(cpr_vreg, REG_RBCPR_CTL, 0);

	/* Program the default HW Ceiling, Floor and vlevel */
	val = ((RBIF_LIMIT_CEILING_DEFAULT & RBIF_LIMIT_CEILING_MASK)
			<< RBIF_LIMIT_CEILING_SHIFT)
		| (RBIF_LIMIT_FLOOR_DEFAULT & RBIF_LIMIT_FLOOR_MASK);
	cpr_write(cpr_vreg, REG_RBIF_LIMIT, val);
	cpr_write(cpr_vreg, REG_RBIF_SW_VLEVEL, RBIF_SW_VLEVEL_DEFAULT);

	/* Clear the target quotient value and gate count of all ROs */
	for (i = 0; i < cpr_vreg->ro_count; i++)
		cpr_write(cpr_vreg, REG_RBCPR_GCNT_TARGET(i), 0);

	/* Init and save gcnt */
	gcnt = (cpr_vreg->ref_clk_khz * cpr_vreg->gcnt_time_us) / 1000;
	gcnt = (gcnt & RBCPR_GCNT_TARGET_GCNT_MASK) <<
			RBCPR_GCNT_TARGET_GCNT_SHIFT;
	cpr_vreg->gcnt = gcnt;

	/* Program the delay count for the timer */
	val = (cpr_vreg->ref_clk_khz * cpr_vreg->timer_delay_us) / 1000;
	cpr_write(cpr_vreg, REG_RBCPR_TIMER_INTERVAL, val);
	cpr_info(cpr_vreg, "Timer count: 0x%0x (for %d us)\n", val,
		cpr_vreg->timer_delay_us);

	/* Program Consecutive Up & Down */
	val = ((cpr_vreg->timer_cons_down & RBIF_TIMER_ADJ_CONS_DOWN_MASK)
			<< RBIF_TIMER_ADJ_CONS_DOWN_SHIFT) |
	       (cpr_vreg->timer_cons_up & RBIF_TIMER_ADJ_CONS_UP_MASK);
	cpr_write(cpr_vreg, REG_RBIF_TIMER_ADJUST, val);

	/* Program the control register */
	cpr_vreg->up_threshold &= RBCPR_CTL_UP_THRESHOLD_MASK;
	cpr_vreg->down_threshold &= RBCPR_CTL_DN_THRESHOLD_MASK;
	val = (cpr_vreg->up_threshold << RBCPR_CTL_UP_THRESHOLD_SHIFT)
		| (cpr_vreg->down_threshold << RBCPR_CTL_DN_THRESHOLD_SHIFT);
	val |= RBCPR_CTL_TIMER_EN | RBCPR_CTL_COUNT_MODE;
	val |= RBCPR_CTL_SW_AUTO_CONT_ACK_EN;
	cpr_write(cpr_vreg, REG_RBCPR_CTL, val);

	cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT);

	val = cpr_read(cpr_vreg, REG_RBCPR_VERSION);
	if (val <= RBCPR_VER_2)
		cpr_vreg->flags |= FLAGS_IGNORE_1ST_IRQ_STATUS;

	size = cpr_vreg->num_corners + 1;
	cpr_vreg->save_ctl = devm_kzalloc(cpr_vreg->dev, sizeof(int) * size,
						GFP_KERNEL);
	cpr_vreg->save_irq = devm_kzalloc(cpr_vreg->dev, sizeof(int) * size,
						GFP_KERNEL);
	if (!cpr_vreg->save_ctl || !cpr_vreg->save_irq) {
		rc = -ENOMEM;
		goto _exit;
	}

	for (i = 1; i < size; i++)
		cpr_corner_save(cpr_vreg, i);

_exit:
	cpr2_gfx_clock_disable(cpr_vreg);
	return rc;
}

static int cpr_init_cpr(struct platform_device *pdev,
				struct cpr2_gfx_regulator *cpr_vreg)
{
	struct resource *res;
	int rc;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rbcpr");
	if (!res || !res->start) {
		cpr_err(cpr_vreg, "missing rbcpr address: res=%p\n", res);
		return -EINVAL;
	}
	cpr_vreg->rbcpr_base = devm_ioremap(&pdev->dev, res->start, GFP_KERNEL);
	if (!cpr_vreg->rbcpr_base) {
		cpr_err(cpr_vreg, "ioremap rbcpr address=%p failed\n", res);
		return -ENXIO;
	}

	rc = cpr_get_clock_handles(cpr_vreg);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "clocks read failed, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Read target quotients from global target-quotient table passed
	 * through device node.
	 */
	rc = cpr_init_target_quotients(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "target quotient table read failed, rc=%d\n",
			rc);
		return rc;
	}

	/* Reduce the ceiling voltage if allowed. */
	rc = cpr_reduce_ceiling_voltage(cpr_vreg);
	if (rc)
		return rc;

	/* Init all voltage set points of GFX regulator for CPR */
	rc = cpr_init_cpr_voltages(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "init closed loop voltages failed, rc=%d\n",
			rc);
		return rc;
	}

	/* Init CPR configuration parameters */
	rc = cpr_init_cpr_parameters(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "init cpr configuration parameters failed, rc=%d\n",
			rc);
		return rc;
	}

	/* Get and Init interrupt */
	cpr_vreg->cpr_irq = platform_get_irq(pdev, 0);
	if (!cpr_vreg->cpr_irq) {
		cpr_err(cpr_vreg, "missing CPR IRQ\n");
		return -EINVAL;
	}

	/* Configure CPR HW but keep it disabled */
	rc = cpr_config(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "configure CPR HW failed, rc=%d\n", rc);
		return rc;
	}

	rc = devm_request_threaded_irq(&pdev->dev, cpr_vreg->cpr_irq, NULL,
					cpr2_gfx_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					"cpr", cpr_vreg);
	if (rc)
		cpr_err(cpr_vreg, "CPR: request irq failed for IRQ %d\n",
				cpr_vreg->cpr_irq);

	return rc;
}

static void cpr_gfx_exit(struct cpr2_gfx_regulator *cpr_vreg)
{
	if (cpr_vreg->vreg_enabled) {
		regulator_disable(cpr_vreg->vdd_gfx);

		if (cpr_vreg->vdd_mx)
			regulator_disable(cpr_vreg->vdd_mx);
	}
}

static void cpr_efuse_free(struct cpr2_gfx_regulator *cpr_vreg)
{
	iounmap(cpr_vreg->efuse_base);
}

static int cpr_enable_set(void *data, u64 val)
{
	struct cpr2_gfx_regulator *cpr_vreg = data;
	bool old_cpr_enable;
	int rc = 0;

	mutex_lock(&cpr_vreg->cpr_mutex);

	old_cpr_enable = cpr_vreg->enable;
	cpr_vreg->enable = val;

	if (old_cpr_enable == cpr_vreg->enable)
		goto _exit;

	if (cpr_vreg->enable && cpr_vreg->cpr_fuse_disable) {
		cpr_info(cpr_vreg,
			"CPR permanently disabled due to fuse values\n");
		cpr_vreg->enable = false;
		goto _exit;
	}

	cpr_debug(cpr_vreg, "%s CPR [corner=%d]\n",
		cpr_vreg->enable ? "enabling" : "disabling", cpr_vreg->corner);

	if (cpr_vreg->corner) {
		if (cpr_vreg->enable) {
			rc = cpr2_gfx_closed_loop_enable(cpr_vreg);
			if (rc) {
				cpr_err(cpr_vreg, "could not enable CPR, rc=%d\n",
					rc);
				goto _exit;
			}

			cpr_ctl_disable(cpr_vreg);
			cpr_irq_clr(cpr_vreg);
			cpr_corner_restore(cpr_vreg, cpr_vreg->corner);
			cpr_ctl_enable(cpr_vreg, cpr_vreg->corner);
		} else {
			cpr_ctl_disable(cpr_vreg);
			cpr_irq_set(cpr_vreg, 0);
			cpr2_gfx_closed_loop_disable(cpr_vreg);
		}
	}

_exit:
	mutex_unlock(&cpr_vreg->cpr_mutex);
	return 0;
}

static int cpr_enable_get(void *data, u64 *val)
{
	struct cpr2_gfx_regulator *cpr_vreg = data;

	*val = cpr_vreg->enable;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr_enable_fops, cpr_enable_get, cpr_enable_set,
			"%llu\n");

static int cpr_get_cpr_ceiling(void *data, u64 *val)
{
	struct cpr2_gfx_regulator *cpr_vreg = data;

	*val = cpr_vreg->ceiling_volt[cpr_vreg->corner];

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr_ceiling_fops, cpr_get_cpr_ceiling, NULL,
			"%llu\n");

static int cpr_get_cpr_floor(void *data, u64 *val)
{
	struct cpr2_gfx_regulator *cpr_vreg = data;

	*val = cpr_vreg->floor_volt[cpr_vreg->corner];

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr_floor_fops, cpr_get_cpr_floor, NULL,
			"%llu\n");

static int cpr2_gfx_debug_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t cpr2_gfx_debug_info_read(struct file *file, char __user *buff,
				size_t count, loff_t *ppos)
{
	struct cpr2_gfx_regulator *cpr_vreg = file->private_data;
	char *debugfs_buf;
	ssize_t len, ret = 0;
	u32 gcnt, ro_sel, ctl, irq_status, reg, error_steps;
	u32 step_dn, step_up, error, error_lt0, busy;

	debugfs_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!debugfs_buf)
		return -ENOMEM;

	mutex_lock(&cpr_vreg->cpr_mutex);

	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
		"corner = %d, current_volt = %d uV\n",
		cpr_vreg->corner, cpr_vreg->last_volt[cpr_vreg->corner]);
	ret += len;

	for (ro_sel = 0; ro_sel < cpr_vreg->ro_count; ro_sel++) {
		gcnt = cpr_read(cpr_vreg, REG_RBCPR_GCNT_TARGET(ro_sel));
		len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
				"rbcpr_gcnt_target (%u) = 0x%02X\n",
				ro_sel, gcnt);
		ret += len;
	}

	ctl = cpr_read(cpr_vreg, REG_RBCPR_CTL);
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			"rbcpr_ctl = 0x%02X\n", ctl);
	ret += len;

	irq_status = cpr_read(cpr_vreg, REG_RBIF_IRQ_STATUS);
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			"rbcpr_irq_status = 0x%02X\n", irq_status);
	ret += len;

	reg = cpr_read(cpr_vreg, REG_RBCPR_RESULT_0);
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			"rbcpr_result_0 = 0x%02X\n", reg);
	ret += len;

	step_dn = reg & 0x01;
	step_up = (reg >> RBCPR_RESULT0_STEP_UP_SHIFT) & 0x01;
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			"  [step_dn = %u", step_dn);
	ret += len;

	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			", step_up = %u", step_up);
	ret += len;

	error_steps = (reg >> RBCPR_RESULT0_ERROR_STEPS_SHIFT)
				& RBCPR_RESULT0_ERROR_STEPS_MASK;
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			", error_steps = %u", error_steps);
	ret += len;

	error = (reg >> RBCPR_RESULT0_ERROR_SHIFT) & RBCPR_RESULT0_ERROR_MASK;
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			", error = %u", error);
	ret += len;

	error_lt0 = (reg >> RBCPR_RESULT0_ERROR_LT0_SHIFT) & 0x01;
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			", error_lt_0 = %u", error_lt0);
	ret += len;

	busy = (reg >> RBCPR_RESULT0_BUSY_SHIFT) & 0x01;
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			", busy = %u]\n", busy);
	ret += len;
	mutex_unlock(&cpr_vreg->cpr_mutex);

	ret = simple_read_from_buffer(buff, count, ppos, debugfs_buf, ret);
	kfree(debugfs_buf);
	return ret;
}

static const struct file_operations cpr2_gfx_debug_info_fops = {
	.open = cpr2_gfx_debug_info_open,
	.read = cpr2_gfx_debug_info_read,
};

static void cpr2_gfx_debugfs_init(struct cpr2_gfx_regulator *cpr_vreg)
{
	struct dentry *temp;

	if (IS_ERR_OR_NULL(cpr2_gfx_debugfs_base)) {
		cpr_err(cpr_vreg, "Could not create debugfs nodes since base directory is missing\n");
		return;
	}

	cpr_vreg->debugfs = debugfs_create_dir(cpr_vreg->rdesc.name,
						cpr2_gfx_debugfs_base);
	if (IS_ERR_OR_NULL(cpr_vreg->debugfs)) {
		cpr_err(cpr_vreg, "debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_file("debug_info", S_IRUGO, cpr_vreg->debugfs,
					cpr_vreg, &cpr2_gfx_debug_info_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr_err(cpr_vreg, "debug_info node creation failed\n");
		return;
	}

	temp = debugfs_create_file("cpr_enable", S_IRUGO | S_IWUSR,
			cpr_vreg->debugfs, cpr_vreg, &cpr_enable_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr_err(cpr_vreg, "cpr_enable node creation failed\n");
		return;
	}

	temp = debugfs_create_file("cpr_ceiling", S_IRUGO,
			cpr_vreg->debugfs, cpr_vreg, &cpr_ceiling_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr_err(cpr_vreg, "cpr_ceiling node creation failed\n");
		return;
	}

	temp = debugfs_create_file("cpr_floor", S_IRUGO,
			cpr_vreg->debugfs, cpr_vreg, &cpr_floor_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr_err(cpr_vreg, "cpr_floor node creation failed\n");
		return;
	}
}

static void cpr2_gfx_debugfs_remove(struct cpr2_gfx_regulator *cpr_vreg)
{
	debugfs_remove_recursive(cpr_vreg->debugfs);
}

static void cpr2_gfx_debugfs_base_init(void)
{
	cpr2_gfx_debugfs_base = debugfs_create_dir("cpr2-gfx-regulator",
							NULL);
	if (IS_ERR_OR_NULL(cpr2_gfx_debugfs_base))
		pr_err("cpr2-gfx-regulator debugfs base directory creation failed\n");
}

static void cpr2_gfx_debugfs_base_remove(void)
{
	debugfs_remove_recursive(cpr2_gfx_debugfs_base);
}

static int cpr2_gfx_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
	struct cpr2_gfx_regulator *cpr_vreg;
	struct regulator_desc *rdesc;
	struct device *dev = &pdev->dev;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	int rc;

	if (!dev->of_node) {
		dev_err(dev, "Device tree node is missing\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(dev, dev->of_node);
	if (!init_data) {
		dev_err(dev, "regulator init data is missing\n");
		return -EINVAL;
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS;

	cpr_vreg = devm_kzalloc(dev, sizeof(*cpr_vreg), GFP_KERNEL);
	if (!cpr_vreg)
		return -ENOMEM;

	cpr_vreg->dev = dev;
	mutex_init(&cpr_vreg->cpr_mutex);

	cpr_vreg->rdesc.name = init_data->constraints.name;
	if (cpr_vreg->rdesc.name == NULL) {
		dev_err(dev, "regulator-name missing\n");
		return -EINVAL;
	}

	rc = cpr2_gfx_allocate_memory(cpr_vreg);
	if (rc)
		return rc;

	rc = cpr_mem_acc_init(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "mem_acc initialization error: rc=%d\n", rc);
		return rc;
	}

	rc = cpr_efuse_init(pdev, cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Wrong eFuse address specified: rc=%d\n", rc);
		return rc;
	}

	rc = cpr_parse_fuse_parameters(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Failed to parse fuse parameters: rc=%d\n",
			rc);
		goto err_out;
	}

	rc = cpr_find_fuse_map_match(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Could not determine fuse mapping match: rc=%d\n",
			rc);
		goto err_out;
	}

	rc = cpr_voltage_plan_init(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Wrong DT parameter specified: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_pvs_init(cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Initialize PVS wrong: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_gfx_init(cpr_vreg);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "Initialize GFX wrong: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_init_cpr(pdev, cpr_vreg);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "Initialize CPR failed: rc=%d\n", rc);
		goto err_out;
	}

	/*
	 * Ensure that enable state accurately reflects the case in which CPR
	 * is permanently disabled.
	 */
	cpr_vreg->enable &= !cpr_vreg->cpr_fuse_disable;

	platform_set_drvdata(pdev, cpr_vreg);

	rdesc			= &cpr_vreg->rdesc;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;
	rdesc->ops		= &cpr_corner_ops;

	reg_config.dev = &pdev->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = cpr_vreg;
	reg_config.of_node = pdev->dev.of_node;
	cpr_vreg->rdev = regulator_register(rdesc, &reg_config);
	if (IS_ERR(cpr_vreg->rdev)) {
		rc = PTR_ERR(cpr_vreg->rdev);
		cpr_err(cpr_vreg, "regulator_register failed: rc=%d\n", rc);

		cpr_gfx_exit(cpr_vreg);
		goto err_out;
	}

	cpr2_gfx_debugfs_init(cpr_vreg);

	mutex_lock(&cpr2_gfx_regulator_list_mutex);
	list_add(&cpr_vreg->list, &cpr2_gfx_regulator_list);
	mutex_unlock(&cpr2_gfx_regulator_list_mutex);

err_out:
	cpr_efuse_free(cpr_vreg);
	return rc;
}

static int cpr2_gfx_regulator_remove(struct platform_device *pdev)
{
	struct cpr2_gfx_regulator *cpr_vreg = platform_get_drvdata(pdev);

	if (cpr_vreg) {
		/* Disable CPR */
		if (cpr_vreg->ctrl_enable) {
			cpr_ctl_disable(cpr_vreg);
			cpr_irq_set(cpr_vreg, 0);
			cpr2_gfx_closed_loop_disable(cpr_vreg);
		}

		mutex_lock(&cpr2_gfx_regulator_list_mutex);
		list_del(&cpr_vreg->list);
		mutex_unlock(&cpr2_gfx_regulator_list_mutex);

		cpr_gfx_exit(cpr_vreg);
		cpr2_gfx_debugfs_remove(cpr_vreg);
		regulator_unregister(cpr_vreg->rdev);
	}

	return 0;
}

static struct of_device_id cpr2_gfx_regulator_match_table[] = {
	{ .compatible = "qcom,cpr2-gfx-regulator", },
	{}
};

static struct platform_driver cpr2_gfx_regulator_driver = {
	.driver		= {
		.name	= "qcom,cpr2-gfx-regulator",
		.of_match_table = cpr2_gfx_regulator_match_table,
		.owner = THIS_MODULE,
	},
	.probe		= cpr2_gfx_regulator_probe,
	.remove		= cpr2_gfx_regulator_remove,
	.suspend	= cpr2_gfx_regulator_suspend,
	.resume		= cpr2_gfx_regulator_resume,
};

static int cpr2_gfx_regulator_init(void)
{
	cpr2_gfx_debugfs_base_init();
	return platform_driver_register(&cpr2_gfx_regulator_driver);
}
arch_initcall(cpr2_gfx_regulator_init);

static void cpr2_gfx_regulator_exit(void)
{
	cpr2_gfx_debugfs_base_remove();
	platform_driver_unregister(&cpr2_gfx_regulator_driver);
}
module_exit(cpr2_gfx_regulator_exit);

MODULE_DESCRIPTION("CPR2 GFX regulator driver");
MODULE_LICENSE("GPL v2");
