/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/cpr-regulator.h>
#include <mach/scm.h>

/* Register Offsets for RB-CPR and Bit Definitions */

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

#define RBCPR_RESULT0_ERROR_STEPS_SHIFT	2
#define RBCPR_RESULT0_ERROR_STEPS_BITS	4
#define RBCPR_RESULT0_ERROR_STEPS_MASK	((1<<RBCPR_RESULT0_ERROR_STEPS_BITS)-1)

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

#define CPR_NUM_RING_OSC	8
#define CPR_NUM_SAVE_REGS	10

/* RBCPR Clock Control Register */
#define RBCPR_CLK_SEL_MASK	BIT(0)
#define RBCPR_CLK_SEL_19P2_MHZ	0
#define RBCPR_CLK_SEL_AHB_CLK	BIT(0)

/* CPR eFuse parameters */
#define CPR_FUSE_TARGET_QUOT_BITS	12
#define CPR_FUSE_TARGET_QUOT_BITS_MASK	((1<<CPR_FUSE_TARGET_QUOT_BITS)-1)
#define CPR_FUSE_RO_SEL_BITS		3
#define CPR_FUSE_RO_SEL_BITS_MASK	((1<<CPR_FUSE_RO_SEL_BITS)-1)

#define CPR_FUSE_MIN_QUOT_DIFF		100

#define BYTES_PER_FUSE_ROW		8

enum voltage_change_dir {
	NO_CHANGE,
	DOWN,
	UP,
};

struct cpr_regulator {
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	bool				vreg_enabled;
	int				corner;
	int				ceiling_max;

	/* eFuse parameters */
	phys_addr_t	efuse_addr;
	void __iomem	*efuse_base;

	/* Process voltage parameters */
	u32		pvs_bin_process[CPR_PVS_EFUSE_BINS_MAX];
	u32		pvs_corner_v[NUM_APC_PVS][CPR_CORNER_MAX];
	/* Process voltage variables */
	u32		pvs_bin;
	u32		process;

	/* Control parameter to read efuse parameters by trustzone API */
	bool		use_tz_api;

	/* APC voltage regulator */
	struct regulator	*vdd_apc;

	/* Dependency parameters */
	struct regulator	*vdd_mx;
	int			vdd_mx_vmax;
	int			vdd_mx_vmin_method;
	int			vdd_mx_vmin;

	/* CPR parameters */
	u64		cpr_fuse_bits;
	bool		cpr_fuse_disable;
	bool		cpr_fuse_local;
	int		cpr_fuse_target_quot[CPR_CORNER_MAX];
	int		cpr_fuse_ro_sel[CPR_CORNER_MAX];
	int		gcnt;

	unsigned int	cpr_irq;
	void __iomem	*rbcpr_base;
	phys_addr_t	rbcpr_clk_addr;
	struct mutex	cpr_mutex;

	int		ceiling_volt[CPR_CORNER_MAX];
	int		floor_volt[CPR_CORNER_MAX];
	int		last_volt[CPR_CORNER_MAX];
	int		step_volt;

	int		save_ctl[CPR_CORNER_MAX];
	int		save_irq[CPR_CORNER_MAX];

	u32		save_regs[CPR_NUM_SAVE_REGS];
	u32		save_reg_val[CPR_NUM_SAVE_REGS];

	/* Config parameters */
	bool		enable;
	u32		ref_clk_khz;
	u32		timer_delay_us;
	u32		timer_cons_up;
	u32		timer_cons_down;
	u32		irq_line;
	u32		step_quotient;
	u32		up_threshold;
	u32		down_threshold;
	u32		idle_clocks;
	u32		gcnt_time_us;
	u32		vdd_apc_step_up_limit;
	u32		vdd_apc_step_down_limit;
};

#define CPR_DEBUG_MASK_IRQ	BIT(0)
#define CPR_DEBUG_MASK_API	BIT(1)

static int cpr_debug_enable = CPR_DEBUG_MASK_IRQ;
static int cpr_enable;
static struct cpr_regulator *the_cpr;

module_param_named(debug_enable, cpr_debug_enable, int, S_IRUGO | S_IWUSR);
#define cpr_debug(message, ...) \
	do { \
		if (cpr_debug_enable & CPR_DEBUG_MASK_API) \
			pr_info(message, ##__VA_ARGS__); \
	} while (0)
#define cpr_debug_irq(message, ...) \
	do { \
		if (cpr_debug_enable & CPR_DEBUG_MASK_IRQ) \
			pr_info(message, ##__VA_ARGS__); \
		else \
			pr_debug(message, ##__VA_ARGS__); \
	} while (0)


static u64 cpr_read_efuse_row(struct cpr_regulator *cpr_vreg, u32 row_num)
{
	int rc;
	u64 efuse_bits;
	struct cpr_read_req {
		u32 row_address;
		int addr_type;
	} req;

	struct cpr_read_rsp {
		u32 row_data[2];
		u32 status;
	} rsp;

	if (cpr_vreg->use_tz_api != true) {
		efuse_bits = readll_relaxed(cpr_vreg->efuse_base
			+ row_num * BYTES_PER_FUSE_ROW);
		return efuse_bits;
	}

	req.row_address = cpr_vreg->efuse_addr + row_num * BYTES_PER_FUSE_ROW;
	req.addr_type = 0;
	efuse_bits = 0;

	rc = scm_call(SCM_SVC_FUSE, SCM_FUSE_READ,
			&req, sizeof(req), &rsp, sizeof(rsp));

	if (rc) {
		pr_err("read row %d failed, err code = %d", row_num, rc);
	} else {
		efuse_bits = ((u64)(rsp.row_data[1]) << 32) +
				(u64)rsp.row_data[0];
	}

	return efuse_bits;
}


static bool cpr_is_allowed(struct cpr_regulator *cpr_vreg)
{
	if (cpr_vreg->cpr_fuse_disable || !cpr_enable)
		return false;
	else
		return true;
}

static void cpr_write(struct cpr_regulator *cpr_vreg, u32 offset, u32 value)
{
	writel_relaxed(value, cpr_vreg->rbcpr_base + offset);
}

static u32 cpr_read(struct cpr_regulator *cpr_vreg, u32 offset)
{
	return readl_relaxed(cpr_vreg->rbcpr_base + offset);
}

static void cpr_masked_write(struct cpr_regulator *cpr_vreg, u32 offset,
			     u32 mask, u32 value)
{
	u32 reg_val;

	reg_val = readl_relaxed(cpr_vreg->rbcpr_base + offset);
	reg_val &= ~mask;
	reg_val |= value & mask;
	writel_relaxed(reg_val, cpr_vreg->rbcpr_base + offset);
}

static void cpr_irq_clr(struct cpr_regulator *cpr_vreg)
{
	cpr_write(cpr_vreg, REG_RBIF_IRQ_CLEAR, CPR_INT_ALL);
}

static void cpr_irq_clr_nack(struct cpr_regulator *cpr_vreg)
{
	cpr_irq_clr(cpr_vreg);
	cpr_write(cpr_vreg, REG_RBIF_CONT_NACK_CMD, 1);
}

static void cpr_irq_clr_ack(struct cpr_regulator *cpr_vreg)
{
	cpr_irq_clr(cpr_vreg);
	cpr_write(cpr_vreg, REG_RBIF_CONT_ACK_CMD, 1);
}

static void cpr_irq_set(struct cpr_regulator *cpr_vreg, u32 int_bits)
{
	cpr_write(cpr_vreg, REG_RBIF_IRQ_EN(cpr_vreg->irq_line), int_bits);
}

static void cpr_ctl_modify(struct cpr_regulator *cpr_vreg, u32 mask, u32 value)
{
	cpr_masked_write(cpr_vreg, REG_RBCPR_CTL, mask, value);
}

static void cpr_ctl_enable(struct cpr_regulator *cpr_vreg, int corner)
{
	u32 val;

	if (cpr_is_allowed(cpr_vreg) &&
	    (cpr_vreg->ceiling_volt[corner] > cpr_vreg->floor_volt[corner]))
		val = RBCPR_CTL_LOOP_EN;
	else
		val = 0;
	cpr_ctl_modify(cpr_vreg, RBCPR_CTL_LOOP_EN, val);
}

static void cpr_ctl_disable(struct cpr_regulator *cpr_vreg)
{
	cpr_ctl_modify(cpr_vreg, RBCPR_CTL_LOOP_EN, 0);
}

static void cpr_regs_save(struct cpr_regulator *cpr_vreg)
{
	int i, offset;

	for (i = 0; i < CPR_NUM_SAVE_REGS; i++) {
		offset = cpr_vreg->save_regs[i];
		cpr_vreg->save_reg_val[i] = cpr_read(cpr_vreg, offset);
	}
}

static void cpr_regs_restore(struct cpr_regulator *cpr_vreg)
{
	int i, offset;
	u32 val;

	for (i = 0; i < CPR_NUM_SAVE_REGS; i++) {
		offset = cpr_vreg->save_regs[i];
		val = cpr_vreg->save_reg_val[i];
		cpr_write(cpr_vreg, offset, val);
	}
}

static void cpr_corner_save(struct cpr_regulator *cpr_vreg, int corner)
{
	cpr_vreg->save_ctl[corner] = cpr_read(cpr_vreg, REG_RBCPR_CTL);
	cpr_vreg->save_irq[corner] =
		cpr_read(cpr_vreg, REG_RBIF_IRQ_EN(cpr_vreg->irq_line));
}

static void cpr_corner_restore(struct cpr_regulator *cpr_vreg, int corner)
{
	u32 gcnt, ctl, irq, ro_sel;

	ro_sel = cpr_vreg->cpr_fuse_ro_sel[corner];
	gcnt = cpr_vreg->gcnt | cpr_vreg->cpr_fuse_target_quot[corner];
	cpr_write(cpr_vreg, REG_RBCPR_GCNT_TARGET(ro_sel), gcnt);
	ctl = cpr_vreg->save_ctl[corner];
	cpr_write(cpr_vreg, REG_RBCPR_CTL, ctl);
	irq = cpr_vreg->save_irq[corner];
	cpr_irq_set(cpr_vreg, irq);
	cpr_debug("gcnt = 0x%08x, ctl = 0x%08x, irq = 0x%08x\n",
		  gcnt, ctl, irq);
}

static void cpr_corner_switch(struct cpr_regulator *cpr_vreg, int corner)
{
	if (cpr_vreg->corner == corner)
		return;

	cpr_corner_restore(cpr_vreg, corner);
}

/* Module parameter ops */
static int cpr_enable_param_set(const char *val, const struct kernel_param *kp)
{
	int rc;
	int old_cpr_enable;

	if (!the_cpr) {
		pr_err("the_cpr = NULL\n");
		return -ENXIO;
	}

	mutex_lock(&the_cpr->cpr_mutex);

	old_cpr_enable = cpr_enable;
	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("param_set_int: rc = %d\n", rc);
		goto _exit;
	}

	cpr_debug("%d -> %d [corner=%d]\n",
		  old_cpr_enable, cpr_enable, the_cpr->corner);

	if (the_cpr->cpr_fuse_disable) {
		/* Already disabled */
		pr_info("CPR disabled by fuse\n");
		goto _exit;
	}

	if ((old_cpr_enable != cpr_enable) && the_cpr->corner) {
		if (cpr_enable) {
			cpr_ctl_disable(the_cpr);
			cpr_irq_clr(the_cpr);
			cpr_corner_restore(the_cpr, the_cpr->corner);
			cpr_ctl_enable(the_cpr, the_cpr->corner);
		} else {
			cpr_ctl_disable(the_cpr);
			cpr_irq_set(the_cpr, 0);
		}
	}

_exit:
	mutex_unlock(&the_cpr->cpr_mutex);
	return 0;
}

static struct kernel_param_ops cpr_enable_ops = {
	.set = cpr_enable_param_set,
	.get = param_get_int,
};

module_param_cb(cpr_enable, &cpr_enable_ops, &cpr_enable, S_IRUGO | S_IWUSR);

static int cpr_apc_set(struct cpr_regulator *cpr_vreg, u32 new_volt)
{
	int max_volt, rc;

	max_volt = cpr_vreg->ceiling_max;
	rc = regulator_set_voltage(cpr_vreg->vdd_apc, new_volt, max_volt);
	if (rc)
		pr_err("set: vdd_apc = %d uV: rc=%d\n", new_volt, rc);
	return rc;
}

static int cpr_mx_get(struct cpr_regulator *cpr_vreg, int corner, int apc_volt)
{
	int vdd_mx;

	switch (cpr_vreg->vdd_mx_vmin_method) {
	case VDD_MX_VMIN_APC:
		vdd_mx = apc_volt;
		break;
	case VDD_MX_VMIN_APC_CORNER_CEILING:
		vdd_mx = cpr_vreg->ceiling_volt[corner];
		break;
	case VDD_MX_VMIN_APC_SLOW_CORNER_CEILING:
		vdd_mx = cpr_vreg->pvs_corner_v[APC_PVS_SLOW]
						[CPR_CORNER_TURBO];
		break;
	case VDD_MX_VMIN_MX_VMAX:
		vdd_mx = cpr_vreg->vdd_mx_vmax;
		break;
	default:
		vdd_mx = 0;
		break;
	}

	return vdd_mx;
}

static int cpr_mx_set(struct cpr_regulator *cpr_vreg, int corner,
		      int vdd_mx_vmin)
{
	int rc;

	rc = regulator_set_voltage(cpr_vreg->vdd_mx, vdd_mx_vmin,
				   cpr_vreg->vdd_mx_vmax);
	cpr_debug("[corner:%d] %d uV\n", corner, vdd_mx_vmin);
	if (!rc)
		cpr_vreg->vdd_mx_vmin = vdd_mx_vmin;
	else
		pr_err("set: vdd_mx [%d] = %d uV: rc=%d\n",
		       corner, vdd_mx_vmin, rc);
	return rc;
}

static int cpr_scale_voltage(struct cpr_regulator *cpr_vreg, int corner,
			     int new_apc_volt, enum voltage_change_dir dir)
{
	int rc = 0, vdd_mx_vmin = 0;

	/* No MX scaling if no vdd_mx */
	if (cpr_vreg->vdd_mx == NULL)
		dir = NO_CHANGE;

	if (dir != NO_CHANGE) {
		/* Determine the vdd_mx voltage */
		vdd_mx_vmin = cpr_mx_get(cpr_vreg, corner, new_apc_volt);
	}

	if (vdd_mx_vmin && dir == UP) {
		if (vdd_mx_vmin != cpr_vreg->vdd_mx_vmin)
			rc = cpr_mx_set(cpr_vreg, corner, vdd_mx_vmin);
	}

	if (!rc)
		rc = cpr_apc_set(cpr_vreg, new_apc_volt);

	if (!rc && vdd_mx_vmin && dir == DOWN) {
		if (vdd_mx_vmin != cpr_vreg->vdd_mx_vmin)
			rc = cpr_mx_set(cpr_vreg, corner, vdd_mx_vmin);
	}

	return rc;
}

static void cpr_scale(struct cpr_regulator *cpr_vreg,
		      enum voltage_change_dir dir)
{
	u32 reg_val, error_steps, reg_mask;
	int last_volt, new_volt, corner;

	corner = cpr_vreg->corner;

	reg_val = cpr_read(cpr_vreg, REG_RBCPR_RESULT_0);

	error_steps = (reg_val >> RBCPR_RESULT0_ERROR_STEPS_SHIFT)
				& RBCPR_RESULT0_ERROR_STEPS_MASK;
	last_volt = cpr_vreg->last_volt[corner];

	cpr_debug_irq("last_volt[corner:%d] = %d uV\n", corner, last_volt);

	if (dir == UP) {
		cpr_debug_irq("Up: cpr status = 0x%08x (error_steps=%d)\n",
			      reg_val, error_steps);

		if (last_volt >= cpr_vreg->ceiling_volt[corner]) {
			cpr_debug_irq("[corn:%d] @ ceiling: %d >= %d: NACK\n",
				      corner, last_volt,
				      cpr_vreg->ceiling_volt[corner]);
			cpr_irq_clr_nack(cpr_vreg);

			/* Maximize the UP threshold */
			reg_mask = RBCPR_CTL_UP_THRESHOLD_MASK <<
					RBCPR_CTL_UP_THRESHOLD_SHIFT;
			reg_val = reg_mask;
			cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);
			return;
		}

		if (error_steps > cpr_vreg->vdd_apc_step_up_limit) {
			cpr_debug_irq("%d is over up-limit(%d): Clamp\n",
				      error_steps,
				      cpr_vreg->vdd_apc_step_up_limit);
			error_steps = cpr_vreg->vdd_apc_step_up_limit;
		}

		/* Calculate new voltage */
		new_volt = last_volt + (error_steps * cpr_vreg->step_volt);
		if (new_volt > cpr_vreg->ceiling_volt[corner]) {
			cpr_debug_irq("new_volt(%d) >= ceiling(%d): Clamp\n",
				      new_volt,
				      cpr_vreg->ceiling_volt[corner]);
			new_volt = cpr_vreg->ceiling_volt[corner];
		}

		if (cpr_scale_voltage(cpr_vreg, corner, new_volt, dir)) {
			cpr_irq_clr_nack(cpr_vreg);
			return;
		}
		cpr_vreg->last_volt[corner] = new_volt;

		/* Restore default threshold for DOWN */
		reg_mask = RBCPR_CTL_DN_THRESHOLD_MASK <<
				RBCPR_CTL_DN_THRESHOLD_SHIFT;
		reg_val = cpr_vreg->down_threshold <<
				RBCPR_CTL_DN_THRESHOLD_SHIFT;
		/* and disable auto nack down */
		reg_mask |= RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;

		cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

		/* Re-enable default interrupts */
		cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT);

		/* Ack */
		cpr_irq_clr_ack(cpr_vreg);

		cpr_debug_irq("UP: -> new_volt[corner:%d] = %d uV\n",
			      corner, new_volt);
	} else if (dir == DOWN) {
		cpr_debug_irq("Down: cpr status = 0x%08x (error_steps=%d)\n",
			      reg_val, error_steps);

		if (last_volt <= cpr_vreg->floor_volt[corner]) {
			cpr_debug_irq("[corn:%d] @ floor: %d <= %d: NACK\n",
				      corner, last_volt,
				      cpr_vreg->floor_volt[corner]);
			cpr_irq_clr_nack(cpr_vreg);

			/* Maximize the DOWN threshold */
			reg_mask = RBCPR_CTL_DN_THRESHOLD_MASK <<
					RBCPR_CTL_DN_THRESHOLD_SHIFT;
			reg_val = reg_mask;

			/* Enable auto nack down */
			reg_mask |= RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;
			reg_val |= RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;

			cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

			/* Disable DOWN interrupt */
			cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT & ~CPR_INT_DOWN);

			return;
		}

		if (error_steps > cpr_vreg->vdd_apc_step_down_limit) {
			cpr_debug_irq("%d is over down-limit(%d): Clamp\n",
				      error_steps,
				      cpr_vreg->vdd_apc_step_down_limit);
			error_steps = cpr_vreg->vdd_apc_step_down_limit;
		}

		/* Calculte new voltage */
		new_volt = last_volt - (error_steps * cpr_vreg->step_volt);
		if (new_volt < cpr_vreg->floor_volt[corner]) {
			cpr_debug_irq("new_volt(%d) < floor(%d): Clamp\n",
				      new_volt,
				      cpr_vreg->floor_volt[corner]);
			new_volt = cpr_vreg->floor_volt[corner];
		}

		if (cpr_scale_voltage(cpr_vreg, corner, new_volt, dir)) {
			cpr_irq_clr_nack(cpr_vreg);
			return;
		}
		cpr_vreg->last_volt[corner] = new_volt;

		/* Restore default threshold for UP */
		reg_mask = RBCPR_CTL_UP_THRESHOLD_MASK <<
				RBCPR_CTL_UP_THRESHOLD_SHIFT;
		reg_val = cpr_vreg->up_threshold <<
				RBCPR_CTL_UP_THRESHOLD_SHIFT;
		cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

		/* Ack */
		cpr_irq_clr_ack(cpr_vreg);

		cpr_debug_irq("DOWN: -> new_volt[corner:%d] = %d uV\n",
			      corner, new_volt);
	}
}

static irqreturn_t cpr_irq_handler(int irq, void *dev)
{
	struct cpr_regulator *cpr_vreg = dev;
	u32 reg_val;

	mutex_lock(&cpr_vreg->cpr_mutex);

	reg_val = cpr_read(cpr_vreg, REG_RBIF_IRQ_STATUS);
	cpr_debug_irq("IRQ_STATUS = 0x%02X\n", reg_val);

	if (!cpr_is_allowed(cpr_vreg)) {
		reg_val = cpr_read(cpr_vreg, REG_RBCPR_CTL);
		pr_err("Interrupt broken? RBCPR_CTL = 0x%02X\n", reg_val);
		goto _exit;
	}

	/* Following sequence of handling is as per each IRQ's priority */
	if (reg_val & CPR_INT_UP) {
		cpr_scale(cpr_vreg, UP);
	} else if (reg_val & CPR_INT_DOWN) {
		cpr_scale(cpr_vreg, DOWN);
	} else if (reg_val & CPR_INT_MIN) {
		cpr_irq_clr_nack(cpr_vreg);
	} else if (reg_val & CPR_INT_MAX) {
		cpr_irq_clr_nack(cpr_vreg);
	} else if (reg_val & CPR_INT_MID) {
		/* RBCPR_CTL_SW_AUTO_CONT_ACK_EN is enabled */
		cpr_debug_irq("IRQ occured for Mid Flag\n");
	} else {
		pr_err("IRQ occured for unknown flag (0x%08x)\n", reg_val);
	}

	/* Save register values for the corner */
	cpr_corner_save(cpr_vreg, cpr_vreg->corner);

_exit:
	mutex_unlock(&cpr_vreg->cpr_mutex);
	return IRQ_HANDLED;
}

static int cpr_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);

	return cpr_vreg->vreg_enabled;
}

static int cpr_regulator_enable(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	/* Enable dependency power before vdd_apc */
	if (cpr_vreg->vdd_mx) {
		rc = regulator_enable(cpr_vreg->vdd_mx);
		if (rc) {
			pr_err("regulator_enable: vdd_mx: rc=%d\n", rc);
			return rc;
		}
	}

	rc = regulator_enable(cpr_vreg->vdd_apc);
	if (rc) {
		pr_err("regulator_enable: vdd_apc: rc=%d\n", rc);
		return rc;
	}

	cpr_vreg->vreg_enabled = true;

	mutex_lock(&cpr_vreg->cpr_mutex);
	if (cpr_is_allowed(cpr_vreg) && cpr_vreg->corner) {
		cpr_irq_clr(cpr_vreg);
		cpr_corner_switch(cpr_vreg, cpr_vreg->corner);
		cpr_ctl_enable(cpr_vreg, cpr_vreg->corner);
	}
	mutex_unlock(&cpr_vreg->cpr_mutex);

	return rc;
}

static int cpr_regulator_disable(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = regulator_disable(cpr_vreg->vdd_apc);
	if (!rc) {
		if (cpr_vreg->vdd_mx)
			rc = regulator_disable(cpr_vreg->vdd_mx);

		if (rc) {
			pr_err("regulator_disable: vdd_mx: rc=%d\n", rc);
			return rc;
		}

		cpr_vreg->vreg_enabled = false;

		mutex_lock(&cpr_vreg->cpr_mutex);
		if (cpr_is_allowed(cpr_vreg))
			cpr_ctl_disable(cpr_vreg);
		mutex_unlock(&cpr_vreg->cpr_mutex);
	} else {
		pr_err("regulator_disable: vdd_apc: rc=%d\n", rc);
	}

	return rc;
}

static int cpr_regulator_set_voltage(struct regulator_dev *rdev,
		int corner, int corner_max, unsigned *selector)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc;
	int new_volt;
	enum voltage_change_dir change_dir = NO_CHANGE;

	mutex_lock(&cpr_vreg->cpr_mutex);

	if (cpr_is_allowed(cpr_vreg)) {
		cpr_ctl_disable(cpr_vreg);
		new_volt = cpr_vreg->last_volt[corner];
	} else {
		new_volt = cpr_vreg->pvs_corner_v[cpr_vreg->process][corner];
	}

	cpr_debug("[corner:%d] = %d uV\n", corner, new_volt);

	if (corner > cpr_vreg->corner)
		change_dir = UP;
	else if (corner < cpr_vreg->corner)
		change_dir = DOWN;

	rc = cpr_scale_voltage(cpr_vreg, corner, new_volt, change_dir);
	if (rc)
		goto _exit;

	if (cpr_is_allowed(cpr_vreg) && cpr_vreg->vreg_enabled) {
		cpr_irq_clr(cpr_vreg);
		cpr_corner_switch(cpr_vreg, corner);
		cpr_ctl_enable(cpr_vreg, corner);
	}

	cpr_vreg->corner = corner;

_exit:
	mutex_unlock(&cpr_vreg->cpr_mutex);

	return rc;
}

static int cpr_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);

	return cpr_vreg->corner;
}

static struct regulator_ops cpr_corner_ops = {
	.enable			= cpr_regulator_enable,
	.disable		= cpr_regulator_disable,
	.is_enabled		= cpr_regulator_is_enabled,
	.set_voltage		= cpr_regulator_set_voltage,
	.get_voltage		= cpr_regulator_get_voltage,
};

#ifdef CONFIG_PM
static int cpr_suspend(struct cpr_regulator *cpr_vreg)
{
	cpr_debug("suspend\n");

	cpr_ctl_disable(cpr_vreg);
	disable_irq(cpr_vreg->cpr_irq);

	cpr_irq_clr(cpr_vreg);
	cpr_regs_save(cpr_vreg);

	return 0;
}

static int cpr_resume(struct cpr_regulator *cpr_vreg)

{
	cpr_debug("resume\n");

	cpr_regs_restore(cpr_vreg);
	cpr_irq_clr(cpr_vreg);

	enable_irq(cpr_vreg->cpr_irq);
	cpr_ctl_enable(cpr_vreg, cpr_vreg->corner);

	return 0;
}

static int cpr_regulator_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct cpr_regulator *cpr_vreg = platform_get_drvdata(pdev);

	if (cpr_is_allowed(cpr_vreg))
		return cpr_suspend(cpr_vreg);
	else
		return 0;
}

static int cpr_regulator_resume(struct platform_device *pdev)
{
	struct cpr_regulator *cpr_vreg = platform_get_drvdata(pdev);

	if (cpr_is_allowed(cpr_vreg))
		return cpr_resume(cpr_vreg);
	else
		return 0;
}
#else
#define cpr_regulator_suspend NULL
#define cpr_regulator_resume NULL
#endif

static int cpr_config(struct cpr_regulator *cpr_vreg)
{
	int i;
	u32 val, gcnt, reg;
	void __iomem *rbcpr_clk;

	/* Use 19.2 MHz clock for CPR. */
	rbcpr_clk = ioremap(cpr_vreg->rbcpr_clk_addr, 4);
	if (!rbcpr_clk) {
		pr_err("Unable to map rbcpr_clk\n");
		return -EINVAL;
	}
	reg = readl_relaxed(rbcpr_clk);
	reg &= ~RBCPR_CLK_SEL_MASK;
	reg |= RBCPR_CLK_SEL_19P2_MHZ & RBCPR_CLK_SEL_MASK;
	writel_relaxed(reg, rbcpr_clk);
	iounmap(rbcpr_clk);

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
	for (i = 0; i < CPR_NUM_RING_OSC; i++)
		cpr_write(cpr_vreg, REG_RBCPR_GCNT_TARGET(i), 0);

	/* Init and save gcnt */
	gcnt = (cpr_vreg->ref_clk_khz * cpr_vreg->gcnt_time_us) / 1000;
	gcnt = (gcnt & RBCPR_GCNT_TARGET_GCNT_MASK) <<
			RBCPR_GCNT_TARGET_GCNT_SHIFT;
	cpr_vreg->gcnt = gcnt;

	/* Program the step quotient and idle clocks */
	val = ((cpr_vreg->idle_clocks & RBCPR_STEP_QUOT_IDLE_CLK_MASK)
			<< RBCPR_STEP_QUOT_IDLE_CLK_SHIFT) |
		(cpr_vreg->step_quotient & RBCPR_STEP_QUOT_STEPQUOT_MASK);
	cpr_write(cpr_vreg, REG_RBCPR_STEP_QUOT, val);

	/* Program the delay count for the timer */
	val = (cpr_vreg->ref_clk_khz * cpr_vreg->timer_delay_us) / 1000;
	cpr_write(cpr_vreg, REG_RBCPR_TIMER_INTERVAL, val);
	pr_info("Timer count: 0x%0x (for %d us)\n", val,
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

	/* Registers to save & restore for suspend */
	cpr_vreg->save_regs[0] = REG_RBCPR_TIMER_INTERVAL;
	cpr_vreg->save_regs[1] = REG_RBCPR_STEP_QUOT;
	cpr_vreg->save_regs[2] = REG_RBIF_TIMER_ADJUST;
	cpr_vreg->save_regs[3] = REG_RBIF_LIMIT;
	cpr_vreg->save_regs[4] = REG_RBIF_SW_VLEVEL;
	cpr_vreg->save_regs[5] = REG_RBIF_IRQ_EN(cpr_vreg->irq_line);
	cpr_vreg->save_regs[6] = REG_RBCPR_CTL;
	cpr_vreg->save_regs[7] = REG_RBCPR_GCNT_TARGET
		(cpr_vreg->cpr_fuse_ro_sel[CPR_CORNER_SVS]);
	cpr_vreg->save_regs[8] = REG_RBCPR_GCNT_TARGET
		(cpr_vreg->cpr_fuse_ro_sel[CPR_CORNER_NORMAL]);
	cpr_vreg->save_regs[9] = REG_RBCPR_GCNT_TARGET
		(cpr_vreg->cpr_fuse_ro_sel[CPR_CORNER_TURBO]);

	cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT);

	cpr_corner_save(cpr_vreg, CPR_CORNER_SVS);
	cpr_corner_save(cpr_vreg, CPR_CORNER_NORMAL);
	cpr_corner_save(cpr_vreg, CPR_CORNER_TURBO);

	return 0;
}

static int cpr_is_fuse_redundant(struct cpr_regulator *cpr_vreg,
					 u32 redun_sel[4])
{
	u64 fuse_bits;
	int redundant;

	fuse_bits = cpr_read_efuse_row(cpr_vreg, redun_sel[0]);
	fuse_bits = (fuse_bits >> redun_sel[1]) & ((1 << redun_sel[2]) - 1);
	if (fuse_bits == redun_sel[3])
		redundant = 1;
	else
		redundant = 0;

	pr_info("[row:%d] = 0x%llx @%d:%d = %d?: redundant=%d\n",
		redun_sel[0], fuse_bits,
		redun_sel[1], redun_sel[2], redun_sel[3], redundant);
	return redundant;
}

static int cpr_pvs_init(struct platform_device *pdev,
			       struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	u64 efuse_bits;
	int rc, process;
	u32 pvs_fuse[3], pvs_fuse_redun_sel[4];
	bool redundant;
	size_t pvs_bins;

	rc = of_property_read_u32_array(of_node, "qcom,pvs-fuse-redun-sel",
					pvs_fuse_redun_sel, 4);
	if (rc < 0) {
		pr_err("pvs-fuse-redun-sel missing: rc=%d\n", rc);
		return rc;
	}

	redundant = cpr_is_fuse_redundant(cpr_vreg, pvs_fuse_redun_sel);

	if (redundant) {
		rc = of_property_read_u32_array(of_node, "qcom,pvs-fuse-redun",
						pvs_fuse, 3);
		if (rc < 0) {
			pr_err("pvs-fuse-redun missing: rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = of_property_read_u32_array(of_node, "qcom,pvs-fuse",
						pvs_fuse, 3);
		if (rc < 0) {
			pr_err("pvs-fuse missing: rc=%d\n", rc);
			return rc;
		}
	}

	/* Construct PVS process # from the efuse bits */

	efuse_bits = cpr_read_efuse_row(cpr_vreg, pvs_fuse[0]);
	cpr_vreg->pvs_bin = (efuse_bits >> pvs_fuse[1]) &
				   ((1 << pvs_fuse[2]) - 1);

	pvs_bins = 1 << pvs_fuse[2];
	rc = of_property_read_u32_array(of_node, "qcom,pvs-bin-process",
					cpr_vreg->pvs_bin_process,
					pvs_bins);
	if (rc < 0) {
		pr_err("pvs-bin-process missing: rc=%d\n", rc);
		return rc;
	}

	process = cpr_vreg->pvs_bin_process[cpr_vreg->pvs_bin];
	pr_info("[row:%d] = 0x%llX, n_bits=%d, bin=%d (%d)\n",
		pvs_fuse[0], efuse_bits, pvs_fuse[2],
		cpr_vreg->pvs_bin, process);

	if (process == APC_PVS_NO || process >= NUM_APC_PVS) {
		pr_err("Bin=%d (%d) is out of spec. Assume SLOW.\n",
		       cpr_vreg->pvs_bin, process);
		process = APC_PVS_SLOW;
	}

	cpr_vreg->process = process;

	return 0;
}

#define CPR_PROP_READ_U32(of_node, cpr_property, cpr_config, rc)	\
do {									\
	if (!rc) {							\
		rc = of_property_read_u32(of_node,			\
				"qcom," cpr_property,			\
				cpr_config);				\
		if (rc) {						\
			pr_err("Missing " #cpr_property			\
				": rc = %d\n", rc);			\
		}							\
	}								\
} while (0)

static int cpr_apc_init(struct platform_device *pdev,
			       struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc;

	cpr_vreg->vdd_apc = devm_regulator_get(&pdev->dev, "vdd-apc");
	if (IS_ERR_OR_NULL(cpr_vreg->vdd_apc)) {
		rc = PTR_RET(cpr_vreg->vdd_apc);
		if (rc != -EPROBE_DEFER)
			pr_err("devm_regulator_get: rc=%d\n", rc);
		return rc;
	}

	/* Check dependencies */
	if (of_property_read_bool(of_node, "vdd-mx-supply")) {
		cpr_vreg->vdd_mx = devm_regulator_get(&pdev->dev, "vdd-mx");
		if (IS_ERR_OR_NULL(cpr_vreg->vdd_mx)) {
			rc = PTR_RET(cpr_vreg->vdd_mx);
			if (rc != -EPROBE_DEFER)
				pr_err("devm_regulator_get: vdd-mx: rc=%d\n",
				       rc);
			return rc;
		}
	}

	/* Parse dependency parameters */
	if (cpr_vreg->vdd_mx) {
		rc = of_property_read_u32(of_node, "qcom,vdd-mx-vmax",
				 &cpr_vreg->vdd_mx_vmax);
		if (rc < 0) {
			pr_err("vdd-mx-vmax missing: rc=%d\n", rc);
			return rc;
		}

		rc = of_property_read_u32(of_node, "qcom,vdd-mx-vmin-method",
				 &cpr_vreg->vdd_mx_vmin_method);
		if (rc < 0) {
			pr_err("vdd-mx-vmin-method missing: rc=%d\n", rc);
			return rc;
		}
		if (cpr_vreg->vdd_mx_vmin_method > VDD_MX_VMIN_MX_VMAX) {
			pr_err("Invalid vdd-mx-vmin-method(%d)\n",
				cpr_vreg->vdd_mx_vmin_method);
			return -EINVAL;
		}
	}

	return 0;
}

static void cpr_apc_exit(struct cpr_regulator *cpr_vreg)
{
	if (cpr_vreg->vreg_enabled) {
		regulator_disable(cpr_vreg->vdd_apc);

		if (cpr_vreg->vdd_mx)
			regulator_disable(cpr_vreg->vdd_mx);
	}
}

static int cpr_init_cpr_efuse(struct platform_device *pdev,
				     struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int i, rc = 0;
	bool redundant;
	u32 cpr_fuse_redun_sel[4];
	char *targ_quot_str, *ro_sel_str;
	u32 cpr_fuse_row;
	u32 bp_cpr_disable, bp_scheme;
	int bp_target_quot[CPR_CORNER_MAX];
	int bp_ro_sel[CPR_CORNER_MAX];
	u32 ro_sel, val;
	u64 fuse_bits, fuse_bits_2;

	rc = of_property_read_u32_array(of_node, "qcom,cpr-fuse-redun-sel",
					cpr_fuse_redun_sel, 4);
	if (rc < 0) {
		pr_err("cpr-fuse-redun-sel missing: rc=%d\n", rc);
		return rc;
	}

	redundant = cpr_is_fuse_redundant(cpr_vreg, cpr_fuse_redun_sel);

	if (redundant) {
		CPR_PROP_READ_U32(of_node, "cpr-fuse-redun-row",
				  &cpr_fuse_row, rc);
		targ_quot_str = "qcom,cpr-fuse-redun-target-quot";
		ro_sel_str = "qcom,cpr-fuse-redun-ro-sel";
	} else {
		CPR_PROP_READ_U32(of_node, "cpr-fuse-row",
				  &cpr_fuse_row, rc);
		targ_quot_str = "qcom,cpr-fuse-target-quot";
		ro_sel_str = "qcom,cpr-fuse-ro-sel";
	}
	if (rc)
		return rc;

	rc = of_property_read_u32_array(of_node,
		targ_quot_str,
		&bp_target_quot[CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("missing %s: rc=%d\n", targ_quot_str, rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node,
		ro_sel_str,
		&bp_ro_sel[CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("missing %s: rc=%d\n", ro_sel_str, rc);
		return rc;
	}

	/* Read the control bits of eFuse */
	fuse_bits = cpr_read_efuse_row(cpr_vreg, cpr_fuse_row);
	pr_info("[row:%d] = 0x%llx\n", cpr_fuse_row, fuse_bits);

	if (redundant) {
		if (of_property_read_bool(of_node,
				"qcom,cpr-fuse-redun-bp-cpr-disable")) {
			CPR_PROP_READ_U32(of_node,
					  "cpr-fuse-redun-bp-cpr-disable",
					  &bp_cpr_disable, rc);
			CPR_PROP_READ_U32(of_node,
					  "cpr-fuse-redun-bp-scheme",
					  &bp_scheme, rc);
			if (rc)
				return rc;
			fuse_bits_2 = fuse_bits;
		} else {
			u32 temp_row;

			/* Use original fuse if no optional property */
			CPR_PROP_READ_U32(of_node, "cpr-fuse-bp-cpr-disable",
					  &bp_cpr_disable, rc);
			CPR_PROP_READ_U32(of_node, "cpr-fuse-bp-scheme",
					  &bp_scheme, rc);
			CPR_PROP_READ_U32(of_node, "cpr-fuse-row",
					  &temp_row, rc);
			if (rc)
				return rc;

			fuse_bits_2 = cpr_read_efuse_row(cpr_vreg, temp_row);
			pr_info("[original row:%d] = 0x%llx\n",
				temp_row, fuse_bits_2);
		}
	} else {
		CPR_PROP_READ_U32(of_node, "cpr-fuse-bp-cpr-disable",
				  &bp_cpr_disable, rc);
		CPR_PROP_READ_U32(of_node, "cpr-fuse-bp-scheme",
				  &bp_scheme, rc);
		if (rc)
			return rc;
		fuse_bits_2 = fuse_bits;
	}

	cpr_vreg->cpr_fuse_disable = (fuse_bits_2 >> bp_cpr_disable) & 0x01;
	cpr_vreg->cpr_fuse_local = (fuse_bits_2 >> bp_scheme) & 0x01;

	pr_info("disable = %d, local = %d\n",
		cpr_vreg->cpr_fuse_disable, cpr_vreg->cpr_fuse_local);

	for (i = CPR_CORNER_SVS; i < CPR_CORNER_MAX; i++) {
		ro_sel = (fuse_bits >> bp_ro_sel[i])
				& CPR_FUSE_RO_SEL_BITS_MASK;
		val = (fuse_bits >> bp_target_quot[i])
				& CPR_FUSE_TARGET_QUOT_BITS_MASK;
		cpr_vreg->cpr_fuse_target_quot[i] = val;
		cpr_vreg->cpr_fuse_ro_sel[i] = ro_sel;
		pr_info("Corner[%d]: ro_sel = %d, target quot = %d\n",
			i, ro_sel, val);
	}

	cpr_vreg->cpr_fuse_bits = fuse_bits;
	if (!cpr_vreg->cpr_fuse_bits) {
		cpr_vreg->cpr_fuse_disable = 1;
		pr_err("cpr_fuse_bits = 0: set cpr_fuse_disable = 1\n");
	} else {
		/* Check if the target quotients are too close together */
		int *quot = cpr_vreg->cpr_fuse_target_quot;
		bool valid_fuse = true;

		if ((quot[CPR_CORNER_TURBO] > quot[CPR_CORNER_NORMAL]) &&
		    (quot[CPR_CORNER_NORMAL] > quot[CPR_CORNER_SVS])) {
			if ((quot[CPR_CORNER_TURBO] -
			     quot[CPR_CORNER_NORMAL])
					<= CPR_FUSE_MIN_QUOT_DIFF)
				valid_fuse = false;
		} else {
			valid_fuse = false;
		}

		if (!valid_fuse) {
			cpr_vreg->cpr_fuse_disable = 1;
			pr_err("invalid quotient values\n");
		}
	}

	return 0;
}

static int cpr_init_cpr_voltages(struct cpr_regulator *cpr_vreg)
{
	int i;

	/* Construct CPR voltage limits */
	for (i = CPR_CORNER_SVS; i < CPR_CORNER_MAX; i++) {
		cpr_vreg->floor_volt[i] =
			cpr_vreg->pvs_corner_v[APC_PVS_FAST][i];
		cpr_vreg->ceiling_volt[i] =
			cpr_vreg->pvs_corner_v[APC_PVS_SLOW][i];
		cpr_vreg->last_volt[i] =
			cpr_vreg->pvs_corner_v[cpr_vreg->process][i];
	}

	return 0;
}

static int cpr_init_cpr_parameters(struct platform_device *pdev,
					  struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc = 0;

	CPR_PROP_READ_U32(of_node, "cpr-ref-clk",
			  &cpr_vreg->ref_clk_khz, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-timer-delay",
			  &cpr_vreg->timer_delay_us, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-timer-cons-up",
			  &cpr_vreg->timer_cons_up, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-timer-cons-down",
			  &cpr_vreg->timer_cons_down, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-irq-line",
			  &cpr_vreg->irq_line, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-step-quotient",
			  &cpr_vreg->step_quotient, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-up-threshold",
			  &cpr_vreg->up_threshold, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-down-threshold",
			  &cpr_vreg->down_threshold, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-idle-clocks",
			  &cpr_vreg->idle_clocks, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-gcnt-time",
			  &cpr_vreg->gcnt_time_us, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "vdd-apc-step-up-limit",
			  &cpr_vreg->vdd_apc_step_up_limit, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "vdd-apc-step-down-limit",
			  &cpr_vreg->vdd_apc_step_down_limit, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(of_node, "cpr-apc-volt-step",
			  &cpr_vreg->step_volt, rc);
	if (rc)
		return rc;

	/* Init module parameter with the DT value */
	cpr_vreg->enable = of_property_read_bool(of_node, "qcom,cpr-enable");
	cpr_enable = (int) cpr_vreg->enable;
	pr_info("CPR is %s by default.\n",
		cpr_vreg->enable ? "enabled" : "disabled");

	return rc;
}

static int cpr_init_cpr(struct platform_device *pdev,
			       struct cpr_regulator *cpr_vreg)
{
	struct resource *res;
	int rc = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rbcpr_clk");
	if (!res || !res->start) {
		pr_err("missing rbcpr_clk address: res=%p\n", res);
		return -EINVAL;
	}
	cpr_vreg->rbcpr_clk_addr = res->start;

	rc = cpr_init_cpr_efuse(pdev, cpr_vreg);
	if (rc)
		return rc;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rbcpr");
	if (!res || !res->start) {
		pr_err("missing rbcpr address: res=%p\n", res);
		return -EINVAL;
	}
	cpr_vreg->rbcpr_base = devm_ioremap(&pdev->dev, res->start,
					    resource_size(res));

	/* Init all voltage set points of APC regulator for CPR */
	cpr_init_cpr_voltages(cpr_vreg);

	/* Init CPR configuration parameters */
	rc = cpr_init_cpr_parameters(pdev, cpr_vreg);
	if (rc)
		return rc;

	/* Get and Init interrupt */
	cpr_vreg->cpr_irq = platform_get_irq(pdev, 0);
	if (!cpr_vreg->cpr_irq) {
		pr_err("missing CPR IRQ\n");
		return -EINVAL;
	}

	/* Configure CPR HW but keep it disabled */
	rc = cpr_config(cpr_vreg);
	if (rc)
		return rc;

	rc = request_threaded_irq(cpr_vreg->cpr_irq, NULL, cpr_irq_handler,
				  IRQF_ONESHOT | IRQF_TRIGGER_RISING, "cpr",
				  cpr_vreg);
	if (rc) {
		pr_err("CPR: request irq failed for IRQ %d\n",
				cpr_vreg->cpr_irq);
		return rc;
	}

	return 0;
}

static int cpr_efuse_init(struct platform_device *pdev,
				 struct cpr_regulator *cpr_vreg)
{
	struct resource *res;
	int len;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse_addr");
	if (!res || !res->start) {
		pr_err("efuse_addr missing: res=%p\n", res);
		return -EINVAL;
	}

	cpr_vreg->efuse_addr = res->start;
	len = res->end - res->start + 1;

	pr_info("efuse_addr = 0x%x (len=0x%x)\n", res->start, len);

	cpr_vreg->efuse_base = ioremap(cpr_vreg->efuse_addr, len);
	if (!cpr_vreg->efuse_base) {
		pr_err("Unable to map efuse_addr 0x%08x\n",
				cpr_vreg->efuse_addr);
		return -EINVAL;
	}

	if (of_property_read_bool(pdev->dev.of_node, "qcom,use-tz-api"))
		cpr_vreg->use_tz_api = true;
	else
		cpr_vreg->use_tz_api = false;

	return 0;
}

static void cpr_efuse_free(struct cpr_regulator *cpr_vreg)
{
	iounmap(cpr_vreg->efuse_base);
}

static int cpr_voltage_plan_init(struct platform_device *pdev,
					struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc, i;

	rc = of_property_read_u32_array(of_node,
		"qcom,pvs-corner-ceiling-slow",
		&cpr_vreg->pvs_corner_v[APC_PVS_SLOW][CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("pvs-corner-ceiling-slow missing: rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,pvs-corner-ceiling-nom",
		&cpr_vreg->pvs_corner_v[APC_PVS_NOM][CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("pvs-corner-ceiling-norm missing: rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,pvs-corner-ceiling-fast",
		&cpr_vreg->pvs_corner_v[APC_PVS_FAST][CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("pvs-corner-ceiling-fast missing: rc=%d\n", rc);
		return rc;
	}

	/* Set ceiling max and use it for APC_PVS_NO */
	cpr_vreg->ceiling_max =
		cpr_vreg->pvs_corner_v[APC_PVS_SLOW][CPR_CORNER_TURBO];

	for (i = APC_PVS_SLOW; i < NUM_APC_PVS; i++) {
		pr_info("[%d] [%d %d %d] uV\n", i,
			cpr_vreg->pvs_corner_v[i][CPR_CORNER_SVS],
			cpr_vreg->pvs_corner_v[i][CPR_CORNER_NORMAL],
			cpr_vreg->pvs_corner_v[i][CPR_CORNER_TURBO]);
	}

	return 0;
}

static int cpr_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
	struct cpr_regulator *cpr_vreg;
	struct regulator_desc *rdesc;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	int rc;

	if (!pdev->dev.of_node) {
		pr_err("Device tree node is missing\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(&pdev->dev, pdev->dev.of_node);
	if (!init_data) {
		pr_err("regulator init data is missing\n");
		return -EINVAL;
	} else {
		init_data->constraints.input_uV
			= init_data->constraints.max_uV;
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS;
	}

	cpr_vreg = devm_kzalloc(&pdev->dev, sizeof(struct cpr_regulator),
				GFP_KERNEL);
	if (!cpr_vreg) {
		pr_err("Can't allocate cpr_regulator memory\n");
		return -ENOMEM;
	}

	rc = cpr_efuse_init(pdev, cpr_vreg);
	if (rc) {
		pr_err("Wrong eFuse address specified: rc=%d\n", rc);
		return rc;
	}

	rc = cpr_voltage_plan_init(pdev, cpr_vreg);
	if (rc) {
		pr_err("Wrong DT parameter specified: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_pvs_init(pdev, cpr_vreg);
	if (rc) {
		pr_err("Initialize PVS wrong: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_apc_init(pdev, cpr_vreg);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			pr_err("Initialize APC wrong: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_init_cpr(pdev, cpr_vreg);
	if (rc) {
		pr_err("Initialize CPR failed: rc=%d\n", rc);
		goto err_out;
	}

	cpr_efuse_free(cpr_vreg);

	mutex_init(&cpr_vreg->cpr_mutex);

	rdesc			= &cpr_vreg->rdesc;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;
	rdesc->ops		= &cpr_corner_ops;
	rdesc->name		= init_data->constraints.name;

	reg_config.dev = &pdev->dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = cpr_vreg;
	reg_config.of_node = pdev->dev.of_node;
	cpr_vreg->rdev = regulator_register(rdesc, &reg_config);
	if (IS_ERR(cpr_vreg->rdev)) {
		rc = PTR_ERR(cpr_vreg->rdev);
		pr_err("regulator_register failed: rc=%d\n", rc);

		cpr_apc_exit(cpr_vreg);
		return rc;
	}

	platform_set_drvdata(pdev, cpr_vreg);
	the_cpr = cpr_vreg;

	return 0;

err_out:
	cpr_efuse_free(cpr_vreg);
	return rc;
}

static int cpr_regulator_remove(struct platform_device *pdev)
{
	struct cpr_regulator *cpr_vreg;

	cpr_vreg = platform_get_drvdata(pdev);
	if (cpr_vreg) {
		/* Disable CPR */
		if (cpr_is_allowed(cpr_vreg)) {
			cpr_ctl_disable(cpr_vreg);
			cpr_irq_set(cpr_vreg, 0);
		}

		cpr_apc_exit(cpr_vreg);
		regulator_unregister(cpr_vreg->rdev);
	}

	return 0;
}

static struct of_device_id cpr_regulator_match_table[] = {
	{ .compatible = CPR_REGULATOR_DRIVER_NAME, },
	{}
};

static struct platform_driver cpr_regulator_driver = {
	.driver		= {
		.name	= CPR_REGULATOR_DRIVER_NAME,
		.of_match_table = cpr_regulator_match_table,
		.owner = THIS_MODULE,
	},
	.probe		= cpr_regulator_probe,
	.remove		= cpr_regulator_remove,
	.suspend	= cpr_regulator_suspend,
	.resume		= cpr_regulator_resume,
};

/**
 * cpr_regulator_init() - register cpr-regulator driver
 *
 * This initialization function should be called in systems in which driver
 * registration ordering must be controlled precisely.
 */
int __init cpr_regulator_init(void)
{
	static bool initialized;

	if (initialized)
		return 0;
	else
		initialized = true;

	return platform_driver_register(&cpr_regulator_driver);
}
EXPORT_SYMBOL(cpr_regulator_init);

static void __exit cpr_regulator_exit(void)
{
	platform_driver_unregister(&cpr_regulator_driver);
}

MODULE_DESCRIPTION("CPR regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(cpr_regulator_init);
module_exit(cpr_regulator_exit);
