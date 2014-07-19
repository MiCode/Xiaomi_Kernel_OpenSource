/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/cpr-regulator.h>
#include <soc/qcom/scm.h>

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
#define RBIF_TIMER_ADJ_CLAMP_INT_BITS	8
#define RBIF_TIMER_ADJ_CLAMP_INT_MASK	((1<<RBIF_TIMER_ADJ_CLAMP_INT_BITS)-1)
#define RBIF_TIMER_ADJ_CLAMP_INT_SHIFT	8

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

#define CPR_NUM_RING_OSC	8

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

#define SPEED_BIN_NONE			UINT_MAX

#define FLAGS_IGNORE_1ST_IRQ_STATUS	BIT(0)
#define FLAGS_SET_MIN_VOLTAGE		BIT(1)
#define FLAGS_UPLIFT_QUOT_VOLT		BIT(2)

#define CPR_REGULATOR_DRIVER_NAME	"qcom,cpr-regulator"

/**
 * enum vdd_mx_vmin_method - Method to determine vmin for vdd-mx
 * %VDD_MX_VMIN_APC:			Equal to APC voltage
 * %VDD_MX_VMIN_APC_CORNER_CEILING:	Equal to PVS corner ceiling voltage
 * %VDD_MX_VMIN_APC_SLOW_CORNER_CEILING:
 *					Equal to slow speed corner ceiling
 * %VDD_MX_VMIN_MX_VMAX:		Equal to specified vdd-mx-vmax voltage
 * %VDD_MX_VMIN_APC_CORNER_MAP:		Equal to the APC corner mapped MX
 *					voltage
 */
enum vdd_mx_vmin_method {
	VDD_MX_VMIN_APC,
	VDD_MX_VMIN_APC_CORNER_CEILING,
	VDD_MX_VMIN_APC_SLOW_CORNER_CEILING,
	VDD_MX_VMIN_MX_VMAX,
	VDD_MX_VMIN_APC_CORNER_MAP,
};

#define CPR_CORNER_MIN		1
#define CPR_FUSE_CORNER_MIN	1
/*
 * This is an arbitrary upper limit which is used in a sanity check in order to
 * avoid excessive memory allocation due to bad device tree data.
 */
#define CPR_FUSE_CORNER_LIMIT	100

struct quot_adjust_info {
	int speed_bin;
	int virtual_corner;
	int quot_adjust;
};

static const char * const vdd_apc_name[] =	{"vdd-apc-optional-prim",
						"vdd-apc-optional-sec",
						"vdd-apc"};

enum voltage_change_dir {
	NO_CHANGE,
	DOWN,
	UP,
};

struct cpr_regulator {
	struct list_head		list;
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	bool				vreg_enabled;
	int				corner;
	int				ceiling_max;
	struct dentry			*debugfs;

	/* eFuse parameters */
	phys_addr_t	efuse_addr;
	void __iomem	*efuse_base;

	/* Process voltage parameters */
	u32		*pvs_corner_v;
	/* Process voltage variables */
	u32		pvs_bin;
	u32		speed_bin;
	u32		pvs_version;

	/* APC voltage regulator */
	struct regulator	*vdd_apc;

	/* Dependency parameters */
	struct regulator	*vdd_mx;
	int			vdd_mx_vmax;
	int			vdd_mx_vmin_method;
	int			vdd_mx_vmin;
	int			*vdd_mx_corner_map;

	/* mem-acc regulator */
	struct regulator	*mem_acc_vreg;

	/* CPR parameters */
	u32		num_fuse_corners;
	u64		cpr_fuse_bits;
	bool		cpr_fuse_disable;
	bool		cpr_fuse_local;
	int		*cpr_fuse_target_quot;
	int		*cpr_fuse_ro_sel;
	int		gcnt;

	unsigned int	cpr_irq;
	void __iomem	*rbcpr_base;
	phys_addr_t	rbcpr_clk_addr;
	struct mutex	cpr_mutex;

	int		*ceiling_volt;
	int		*floor_volt;
	int		*last_volt;
	int		step_volt;

	int		*save_ctl;
	int		*save_irq;

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
	u32		clamp_timer_interval;
	u32		vdd_apc_step_up_limit;
	u32		vdd_apc_step_down_limit;
	u32		flags;
	int		*corner_map;
	u32		num_corners;
	int		*quot_adjust;

	bool		is_cpr_suspended;
};

#define CPR_DEBUG_MASK_IRQ	BIT(0)
#define CPR_DEBUG_MASK_API	BIT(1)

static int cpr_debug_enable = CPR_DEBUG_MASK_IRQ;
#if defined(CONFIG_DEBUG_FS)
static struct dentry *cpr_debugfs_base;
#endif

static DEFINE_MUTEX(cpr_regulator_list_mutex);
static LIST_HEAD(cpr_regulator_list);

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

static u64 cpr_read_efuse_row(struct cpr_regulator *cpr_vreg, u32 row_num,
				bool use_tz_api)
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

	if (!use_tz_api) {
		efuse_bits = readq_relaxed(cpr_vreg->efuse_base
			+ row_num * BYTES_PER_FUSE_ROW);
		return efuse_bits;
	}

	req.row_address = cpr_vreg->efuse_addr + row_num * BYTES_PER_FUSE_ROW;
	req.addr_type = 0;
	efuse_bits = 0;

	rc = scm_call(SCM_SVC_FUSE, SCM_FUSE_READ,
			&req, sizeof(req), &rsp, sizeof(rsp));

	if (rc) {
		cpr_err(cpr_vreg, "read row %d failed, err code = %d",
			row_num, rc);
	} else {
		efuse_bits = ((u64)(rsp.row_data[1]) << 32) +
				(u64)rsp.row_data[0];
	}

	return efuse_bits;
}

/**
 * cpr_read_efuse_param() - read a parameter from one or two eFuse rows
 * @cpr_vreg:	Pointer to cpr_regulator struct for this regulator.
 * @row_start:	Fuse row number to start reading from.
 * @bit_start:	The LSB of the parameter to read from the fuse.
 * @bit_len:	The length of the parameter in bits.
 * @use_tz_api:	Flag to indicate if an SCM call should be used to read the fuse.
 *
 * This function reads a parameter of specified offset and bit size out of one
 * or two consecutive eFuse rows.  This allows for the reading of parameters
 * that happen to be split between two eFuse rows.
 *
 * Returns the fuse parameter on success or 0 on failure.
 */
static u64 cpr_read_efuse_param(struct cpr_regulator *cpr_vreg, int row_start,
		int bit_start, int bit_len, bool use_tz_api)
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

	fuse[0] = cpr_read_efuse_row(cpr_vreg, row_start, use_tz_api);

	if (bit_start == 0 && bit_len == 64) {
		param = fuse[0];
	} else if (bit_start + bit_len <= 64) {
		param = (fuse[0] >> bit_start) & ((1 << bit_len) - 1);
	} else {
		fuse[1] = cpr_read_efuse_row(cpr_vreg, row_start + 1,
						use_tz_api);
		bits_first = 64 - bit_start;
		bits_second = bit_len - bits_first;
		param = (fuse[0] >> bit_start) & ((1 << bits_first) - 1);
		param |= (fuse[1] & ((1 << bits_second) - 1)) << bits_first;
	}

	return param;
}

static bool cpr_is_allowed(struct cpr_regulator *cpr_vreg)
{
	if (cpr_vreg->cpr_fuse_disable || !cpr_vreg->enable)
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
	int fuse_corner = cpr_vreg->corner_map[corner];

	if (cpr_vreg->is_cpr_suspended)
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

	if (cpr_is_allowed(cpr_vreg) && cpr_vreg->vreg_enabled &&
	    (cpr_vreg->ceiling_volt[fuse_corner] >
		cpr_vreg->floor_volt[fuse_corner]))
		val = RBCPR_CTL_LOOP_EN;
	else
		val = 0;
	cpr_ctl_modify(cpr_vreg, RBCPR_CTL_LOOP_EN, val);
}

static void cpr_ctl_disable(struct cpr_regulator *cpr_vreg)
{
	if (cpr_vreg->is_cpr_suspended)
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

static bool cpr_ctl_is_enabled(struct cpr_regulator *cpr_vreg)
{
	u32 reg_val;

	reg_val = cpr_read(cpr_vreg, REG_RBCPR_CTL);
	return reg_val & RBCPR_CTL_LOOP_EN;
}

static bool cpr_ctl_is_busy(struct cpr_regulator *cpr_vreg)
{
	u32 reg_val;

	reg_val = cpr_read(cpr_vreg, REG_RBCPR_RESULT_0);
	return reg_val & RBCPR_RESULT0_BUSY_MASK;
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
	int fuse_corner = cpr_vreg->corner_map[corner];

	ro_sel = cpr_vreg->cpr_fuse_ro_sel[fuse_corner];
	gcnt = cpr_vreg->gcnt | (cpr_vreg->cpr_fuse_target_quot[fuse_corner] -
					cpr_vreg->quot_adjust[corner]);

	cpr_write(cpr_vreg, REG_RBCPR_GCNT_TARGET(ro_sel), gcnt);
	ctl = cpr_vreg->save_ctl[corner];
	cpr_write(cpr_vreg, REG_RBCPR_CTL, ctl);
	irq = cpr_vreg->save_irq[corner];
	cpr_irq_set(cpr_vreg, irq);
	cpr_debug(cpr_vreg, "gcnt = 0x%08x, ctl = 0x%08x, irq = 0x%08x\n",
		  gcnt, ctl, irq);
}

static void cpr_corner_switch(struct cpr_regulator *cpr_vreg, int corner)
{
	if (cpr_vreg->corner == corner)
		return;

	cpr_corner_restore(cpr_vreg, corner);
}

static int cpr_apc_set(struct cpr_regulator *cpr_vreg, u32 new_volt)
{
	int max_volt, rc;

	max_volt = cpr_vreg->ceiling_max;
	rc = regulator_set_voltage(cpr_vreg->vdd_apc, new_volt, max_volt);
	if (rc)
		cpr_err(cpr_vreg, "set: vdd_apc = %d uV: rc=%d\n",
			new_volt, rc);
	return rc;
}

static int cpr_mx_get(struct cpr_regulator *cpr_vreg, int corner, int apc_volt)
{
	int vdd_mx;
	int fuse_corner = cpr_vreg->corner_map[corner];
	int highest_fuse_corner = cpr_vreg->num_fuse_corners;

	switch (cpr_vreg->vdd_mx_vmin_method) {
	case VDD_MX_VMIN_APC:
		vdd_mx = apc_volt;
		break;
	case VDD_MX_VMIN_APC_CORNER_CEILING:
		vdd_mx = cpr_vreg->ceiling_volt[fuse_corner];
		break;
	case VDD_MX_VMIN_APC_SLOW_CORNER_CEILING:
		vdd_mx = cpr_vreg->ceiling_volt[highest_fuse_corner];
		break;
	case VDD_MX_VMIN_MX_VMAX:
		vdd_mx = cpr_vreg->vdd_mx_vmax;
		break;
	case VDD_MX_VMIN_APC_CORNER_MAP:
		vdd_mx = cpr_vreg->vdd_mx_corner_map[fuse_corner];
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
	int fuse_corner = cpr_vreg->corner_map[corner];

	rc = regulator_set_voltage(cpr_vreg->vdd_mx, vdd_mx_vmin,
				   cpr_vreg->vdd_mx_vmax);
	cpr_debug(cpr_vreg, "[corner:%d, fuse_corner:%d] %d uV\n", corner,
			fuse_corner, vdd_mx_vmin);

	if (!rc) {
		cpr_vreg->vdd_mx_vmin = vdd_mx_vmin;
	} else {
		cpr_err(cpr_vreg, "set: vdd_mx [corner:%d, fuse_corner:%d] = %d uV failed: rc=%d\n",
			corner, fuse_corner, vdd_mx_vmin, rc);
	}
	return rc;
}

static int cpr_scale_voltage(struct cpr_regulator *cpr_vreg, int corner,
			     int new_apc_volt, enum voltage_change_dir dir)
{
	int rc = 0, vdd_mx_vmin = 0;
	int fuse_corner = cpr_vreg->corner_map[corner];

	/* Determine the vdd_mx voltage */
	if (dir != NO_CHANGE && cpr_vreg->vdd_mx != NULL)
		vdd_mx_vmin = cpr_mx_get(cpr_vreg, corner, new_apc_volt);

	if (cpr_vreg->mem_acc_vreg && dir == DOWN)
		rc = regulator_set_voltage(cpr_vreg->mem_acc_vreg,
					fuse_corner, fuse_corner);

	if (vdd_mx_vmin && dir == UP) {
		if (vdd_mx_vmin != cpr_vreg->vdd_mx_vmin)
			rc = cpr_mx_set(cpr_vreg, corner, vdd_mx_vmin);
	}

	if (!rc)
		rc = cpr_apc_set(cpr_vreg, new_apc_volt);

	if (!rc && cpr_vreg->mem_acc_vreg && dir == UP)
		rc = regulator_set_voltage(cpr_vreg->mem_acc_vreg,
					fuse_corner, fuse_corner);

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
	int last_volt, new_volt, corner, fuse_corner;
	u32 gcnt, quot;

	corner = cpr_vreg->corner;
	fuse_corner = cpr_vreg->corner_map[corner];

	reg_val = cpr_read(cpr_vreg, REG_RBCPR_RESULT_0);

	error_steps = (reg_val >> RBCPR_RESULT0_ERROR_STEPS_SHIFT)
				& RBCPR_RESULT0_ERROR_STEPS_MASK;
	last_volt = cpr_vreg->last_volt[corner];

	cpr_debug_irq(cpr_vreg,
			"last_volt[corner:%d, fuse_corner:%d] = %d uV\n",
			corner, fuse_corner, last_volt);

	gcnt = cpr_read(cpr_vreg, REG_RBCPR_GCNT_TARGET
			(cpr_vreg->cpr_fuse_ro_sel[fuse_corner]));
	quot = gcnt & ((1 << RBCPR_GCNT_TARGET_GCNT_SHIFT) - 1);

	if (dir == UP) {
		if (cpr_vreg->clamp_timer_interval
				&& error_steps < cpr_vreg->up_threshold) {
			/*
			 * Handle the case where another measurement started
			 * after the interrupt was triggered due to a core
			 * exiting from power collapse.
			 */
			error_steps = max(cpr_vreg->up_threshold,
					cpr_vreg->vdd_apc_step_up_limit);
		}
		cpr_debug_irq(cpr_vreg,
				"Up: cpr status = 0x%08x (error_steps=%d)\n",
				reg_val, error_steps);

		if (last_volt >= cpr_vreg->ceiling_volt[fuse_corner]) {
			cpr_debug_irq(cpr_vreg,
			"[corn:%d, fuse_corn:%d] @ ceiling: %d >= %d: NACK\n",
				corner, fuse_corner, last_volt,
				cpr_vreg->ceiling_volt[fuse_corner]);
			cpr_irq_clr_nack(cpr_vreg);

			cpr_debug_irq(cpr_vreg, "gcnt = 0x%08x (quot = %d)\n",
					gcnt, quot);

			/* Maximize the UP threshold */
			reg_mask = RBCPR_CTL_UP_THRESHOLD_MASK <<
					RBCPR_CTL_UP_THRESHOLD_SHIFT;
			reg_val = reg_mask;
			cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

			/* Disable UP interrupt */
			cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT & ~CPR_INT_UP);

			return;
		}

		if (error_steps > cpr_vreg->vdd_apc_step_up_limit) {
			cpr_debug_irq(cpr_vreg,
				      "%d is over up-limit(%d): Clamp\n",
				      error_steps,
				      cpr_vreg->vdd_apc_step_up_limit);
			error_steps = cpr_vreg->vdd_apc_step_up_limit;
		}

		/* Calculate new voltage */
		new_volt = last_volt + (error_steps * cpr_vreg->step_volt);
		if (new_volt > cpr_vreg->ceiling_volt[fuse_corner]) {
			cpr_debug_irq(cpr_vreg,
				      "new_volt(%d) >= ceiling(%d): Clamp\n",
				      new_volt,
				      cpr_vreg->ceiling_volt[fuse_corner]);

			new_volt = cpr_vreg->ceiling_volt[fuse_corner];
		}

		if (cpr_scale_voltage(cpr_vreg, corner, new_volt, dir)) {
			cpr_irq_clr_nack(cpr_vreg);
			return;
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

		cpr_debug_irq(cpr_vreg,
			"UP: -> new_volt[corner:%d, fuse_corner:%d] = %d uV\n",
			corner, fuse_corner, new_volt);
	} else if (dir == DOWN) {
		if (cpr_vreg->clamp_timer_interval
				&& error_steps < cpr_vreg->down_threshold) {
			/*
			 * Handle the case where another measurement started
			 * after the interrupt was triggered due to a core
			 * exiting from power collapse.
			 */
			error_steps = max(cpr_vreg->down_threshold,
					cpr_vreg->vdd_apc_step_down_limit);
		}
		cpr_debug_irq(cpr_vreg,
			      "Down: cpr status = 0x%08x (error_steps=%d)\n",
			      reg_val, error_steps);

		if (last_volt <= cpr_vreg->floor_volt[fuse_corner]) {
			cpr_debug_irq(cpr_vreg,
			"[corn:%d, fuse_corner:%d] @ floor: %d <= %d: NACK\n",
				corner, fuse_corner, last_volt,
				cpr_vreg->floor_volt[fuse_corner]);
			cpr_irq_clr_nack(cpr_vreg);

			cpr_debug_irq(cpr_vreg, "gcnt = 0x%08x (quot = %d)\n",
					gcnt, quot);

			/* Enable auto nack down */
			reg_mask = RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;
			reg_val = RBCPR_CTL_SW_AUTO_CONT_NACK_DN_EN;

			cpr_ctl_modify(cpr_vreg, reg_mask, reg_val);

			/* Disable DOWN interrupt */
			cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT & ~CPR_INT_DOWN);

			return;
		}

		if (error_steps > cpr_vreg->vdd_apc_step_down_limit) {
			cpr_debug_irq(cpr_vreg,
				      "%d is over down-limit(%d): Clamp\n",
				      error_steps,
				      cpr_vreg->vdd_apc_step_down_limit);
			error_steps = cpr_vreg->vdd_apc_step_down_limit;
		}

		/* Calculte new voltage */
		new_volt = last_volt - (error_steps * cpr_vreg->step_volt);
		if (new_volt < cpr_vreg->floor_volt[fuse_corner]) {
			cpr_debug_irq(cpr_vreg,
				      "new_volt(%d) < floor(%d): Clamp\n",
				      new_volt,
				      cpr_vreg->floor_volt[fuse_corner]);
			new_volt = cpr_vreg->floor_volt[fuse_corner];
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

		/* Re-enable default interrupts */
		cpr_irq_set(cpr_vreg, CPR_INT_DEFAULT);

		/* Ack */
		cpr_irq_clr_ack(cpr_vreg);

		cpr_debug_irq(cpr_vreg,
		"DOWN: -> new_volt[corner:%d, fuse_corner:%d] = %d uV\n",
			corner, fuse_corner, new_volt);
	}
}

static irqreturn_t cpr_irq_handler(int irq, void *dev)
{
	struct cpr_regulator *cpr_vreg = dev;
	u32 reg_val;

	mutex_lock(&cpr_vreg->cpr_mutex);

	reg_val = cpr_read(cpr_vreg, REG_RBIF_IRQ_STATUS);
	if (cpr_vreg->flags & FLAGS_IGNORE_1ST_IRQ_STATUS)
		reg_val = cpr_read(cpr_vreg, REG_RBIF_IRQ_STATUS);

	cpr_debug_irq(cpr_vreg, "IRQ_STATUS = 0x%02X\n", reg_val);

	if (!cpr_ctl_is_enabled(cpr_vreg)) {
		cpr_debug_irq(cpr_vreg, "CPR is disabled\n");
		goto _exit;
	} else if (cpr_ctl_is_busy(cpr_vreg)
			&& !cpr_vreg->clamp_timer_interval) {
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
		cpr_scale(cpr_vreg, UP);
	} else if (reg_val & CPR_INT_DOWN) {
		cpr_scale(cpr_vreg, DOWN);
	} else if (reg_val & CPR_INT_MIN) {
		cpr_irq_clr_nack(cpr_vreg);
	} else if (reg_val & CPR_INT_MAX) {
		cpr_irq_clr_nack(cpr_vreg);
	} else if (reg_val & CPR_INT_MID) {
		/* RBCPR_CTL_SW_AUTO_CONT_ACK_EN is enabled */
		cpr_debug_irq(cpr_vreg, "IRQ occured for Mid Flag\n");
	} else {
		cpr_err(cpr_vreg, "IRQ occured for unknown flag (0x%08x)\n",
			reg_val);
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
			cpr_err(cpr_vreg, "regulator_enable: vdd_mx: rc=%d\n",
				rc);
			return rc;
		}
	}

	rc = regulator_enable(cpr_vreg->vdd_apc);
	if (rc) {
		cpr_err(cpr_vreg, "regulator_enable: vdd_apc: rc=%d\n", rc);
		return rc;
	}

	mutex_lock(&cpr_vreg->cpr_mutex);
	cpr_vreg->vreg_enabled = true;
	if (cpr_is_allowed(cpr_vreg) && cpr_vreg->corner) {
		cpr_irq_clr(cpr_vreg);
		cpr_corner_restore(cpr_vreg, cpr_vreg->corner);
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
			cpr_err(cpr_vreg, "regulator_disable: vdd_mx: rc=%d\n",
				rc);
			return rc;
		}

		mutex_lock(&cpr_vreg->cpr_mutex);
		cpr_vreg->vreg_enabled = false;
		if (cpr_is_allowed(cpr_vreg))
			cpr_ctl_disable(cpr_vreg);
		mutex_unlock(&cpr_vreg->cpr_mutex);
	} else {
		cpr_err(cpr_vreg, "regulator_disable: vdd_apc: rc=%d\n", rc);
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
	int fuse_corner = cpr_vreg->corner_map[corner];

	mutex_lock(&cpr_vreg->cpr_mutex);

	if (cpr_is_allowed(cpr_vreg)) {
		cpr_ctl_disable(cpr_vreg);
		new_volt = cpr_vreg->last_volt[corner];
	} else {
		new_volt = cpr_vreg->pvs_corner_v[fuse_corner];
	}

	cpr_debug(cpr_vreg, "[corner:%d, fuse_corner:%d] = %d uV\n",
		corner, fuse_corner, new_volt);

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
	cpr_debug(cpr_vreg, "suspend\n");

	mutex_lock(&cpr_vreg->cpr_mutex);

	cpr_ctl_disable(cpr_vreg);

	cpr_irq_clr(cpr_vreg);

	cpr_vreg->is_cpr_suspended = true;

	mutex_unlock(&cpr_vreg->cpr_mutex);
	return 0;
}

static int cpr_resume(struct cpr_regulator *cpr_vreg)

{
	cpr_debug(cpr_vreg, "resume\n");

	mutex_lock(&cpr_vreg->cpr_mutex);

	cpr_vreg->is_cpr_suspended = false;
	cpr_irq_clr(cpr_vreg);

	cpr_ctl_enable(cpr_vreg, cpr_vreg->corner);

	mutex_unlock(&cpr_vreg->cpr_mutex);
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

static int cpr_config(struct cpr_regulator *cpr_vreg, struct device *dev)
{
	int i;
	u32 val, gcnt, reg;
	void __iomem *rbcpr_clk;
	int size;

	/* Use 19.2 MHz clock for CPR. */
	rbcpr_clk = ioremap(cpr_vreg->rbcpr_clk_addr, 4);
	if (!rbcpr_clk) {
		cpr_err(cpr_vreg, "Unable to map rbcpr_clk\n");
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
	cpr_info(cpr_vreg, "Timer count: 0x%0x (for %d us)\n", val,
		cpr_vreg->timer_delay_us);

	/* Program Consecutive Up & Down */
	val = ((cpr_vreg->timer_cons_down & RBIF_TIMER_ADJ_CONS_DOWN_MASK)
			<< RBIF_TIMER_ADJ_CONS_DOWN_SHIFT) |
	       (cpr_vreg->timer_cons_up & RBIF_TIMER_ADJ_CONS_UP_MASK) |
	       ((cpr_vreg->clamp_timer_interval & RBIF_TIMER_ADJ_CLAMP_INT_MASK)
			<< RBIF_TIMER_ADJ_CLAMP_INT_SHIFT);
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
	cpr_vreg->save_ctl = devm_kzalloc(dev, sizeof(int) * size, GFP_KERNEL);
	cpr_vreg->save_irq = devm_kzalloc(dev, sizeof(int) * size, GFP_KERNEL);
	if (!cpr_vreg->save_ctl || !cpr_vreg->save_irq)
		return -ENOMEM;

	for (i = 1; i < size; i++)
		cpr_corner_save(cpr_vreg, i);

	return 0;
}

static int cpr_fuse_is_setting_expected(struct cpr_regulator *cpr_vreg,
					u32 sel_array[5])
{
	u64 fuse_bits;
	u32 ret;

	fuse_bits = cpr_read_efuse_row(cpr_vreg, sel_array[0], sel_array[4]);
	ret = (fuse_bits >> sel_array[1]) & ((1 << sel_array[2]) - 1);
	if (ret == sel_array[3])
		ret = 1;
	else
		ret = 0;

	cpr_info(cpr_vreg, "[row:%d] = 0x%llx @%d:%d == %d ?: %s\n",
			sel_array[0], fuse_bits,
			sel_array[1], sel_array[2],
			sel_array[3],
			(ret == 1) ? "yes" : "no");
	return ret;
}

static int cpr_voltage_uplift_wa_inc_volt(struct cpr_regulator *cpr_vreg,
					struct device_node *of_node)
{
	u32 uplift_voltage;
	u32 uplift_max_volt = 0;
	int highest_fuse_corner = cpr_vreg->num_fuse_corners;
	int rc;

	rc = of_property_read_u32(of_node,
		"qcom,cpr-uplift-voltage", &uplift_voltage);
	if (rc < 0) {
		cpr_err(cpr_vreg, "cpr-uplift-voltage is missing, rc = %d", rc);
		return rc;
	}
	rc = of_property_read_u32(of_node,
		"qcom,cpr-uplift-max-volt", &uplift_max_volt);
	if (rc < 0) {
		cpr_err(cpr_vreg, "cpr-uplift-max-volt is missing, rc = %d",
			rc);
		return rc;
	}

	cpr_vreg->pvs_corner_v[highest_fuse_corner] += uplift_voltage;
	if (cpr_vreg->pvs_corner_v[highest_fuse_corner] > uplift_max_volt)
		cpr_vreg->pvs_corner_v[highest_fuse_corner] = uplift_max_volt;

	return rc;
}

/*
 * Property qcom,cpr-fuse-init-voltage specifies the fuse position of the
 * initial voltage for each fuse corner. MSB of the fuse value is a sign
 * bit, and the remaining bits define the steps of the offset. Each step has
 * units of microvolts defined in the qcom,cpr-fuse-init-voltage-step property.
 * The initial voltages can be calculated using the formula:
 * pvs_corner_v[corner] = ceiling_volt[corner] + (sign * steps * step_size_uv)
 */
static int cpr_pvs_per_corner_init(struct device_node *of_node,
				struct cpr_regulator *cpr_vreg)
{
	u64 efuse_bits;
	int i, size, sign, steps, step_size_uv, rc;
	u32 *fuse_sel, *tmp, *ref_uv;
	struct property *prop;

	prop = of_find_property(of_node, "qcom,cpr-fuse-init-voltage", NULL);
	if (!prop) {
		cpr_err(cpr_vreg, "qcom,cpr-fuse-init-voltage is missing\n");
		return -EINVAL;
	}
	size = prop->length / sizeof(u32);
	if (size != cpr_vreg->num_fuse_corners * 4) {
		cpr_err(cpr_vreg,
			"fuse position for init voltages is invalid\n");
		return -EINVAL;
	}
	fuse_sel = kzalloc(sizeof(u32) * size, GFP_KERNEL);
	if (!fuse_sel) {
		cpr_err(cpr_vreg, "memory alloc failed.\n");
		return -ENOMEM;
	}
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

	ref_uv = kzalloc((cpr_vreg->num_fuse_corners + 1) * sizeof(*ref_uv),
			GFP_KERNEL);
	if (!ref_uv) {
		cpr_err(cpr_vreg,
			"Could not allocate memory for reference voltages\n");
		kfree(fuse_sel);
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cpr-init-voltage-ref",
		&ref_uv[CPR_FUSE_CORNER_MIN], cpr_vreg->num_fuse_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg,
			"read qcom,cpr-init-voltage-ref failed, rc = %d\n", rc);
		kfree(fuse_sel);
		kfree(ref_uv);
		return rc;
	}

	tmp = fuse_sel;
	for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners; i++) {
		efuse_bits = cpr_read_efuse_param(cpr_vreg, fuse_sel[0],
					fuse_sel[1], fuse_sel[2], fuse_sel[3]);
		sign = (efuse_bits & (1 << (fuse_sel[2] - 1))) ? -1 : 1;
		steps = efuse_bits & ((1 << (fuse_sel[2] - 1)) - 1);
		cpr_debug(cpr_vreg,
			"corner %d: sign = %d, steps = %d\n", i, sign, steps);
		cpr_vreg->pvs_corner_v[i] =
				ref_uv[i] + sign * steps * step_size_uv;
		cpr_vreg->pvs_corner_v[i] = DIV_ROUND_UP(
				cpr_vreg->pvs_corner_v[i],
				cpr_vreg->step_volt) *
				cpr_vreg->step_volt;
		if (cpr_vreg->pvs_corner_v[i] > cpr_vreg->ceiling_volt[i]) {
			cpr_info(cpr_vreg, "Warning: initial voltage[%d] %d above ceiling %d\n",
						i, cpr_vreg->pvs_corner_v[i],
						cpr_vreg->ceiling_volt[i]);
			cpr_vreg->pvs_corner_v[i] = cpr_vreg->ceiling_volt[i];
		} else if (cpr_vreg->pvs_corner_v[i] <
				cpr_vreg->floor_volt[i]) {
			cpr_info(cpr_vreg, "Warning: initial voltage[%d] %d below floor %d\n",
						i, cpr_vreg->pvs_corner_v[i],
						cpr_vreg->floor_volt[i]);
			cpr_vreg->pvs_corner_v[i] = cpr_vreg->floor_volt[i];
		}
		fuse_sel += 4;
	}
	kfree(tmp);
	kfree(ref_uv);

	return 0;
}

/*
 * A single PVS bin is stored in a fuse that's position is defined either
 * in the qcom,pvs-fuse-redun property or in the qcom,pvs-fuse property.
 * The fuse value defined in the qcom,pvs-fuse-redun-sel property is used
 * to pick between the primary or redudant PVS fuse position.
 * After the PVS bin value is read out successfully, it is used as the row
 * index to get initial voltages for each fuse corner from the voltage table
 * defined in the qcom,pvs-voltage-table property.
 */
static int cpr_pvs_single_bin_init(struct device_node *of_node,
				struct cpr_regulator *cpr_vreg)
{
	u64 efuse_bits;
	u32 pvs_fuse[4], pvs_fuse_redun_sel[5];
	int rc, i, stripe_size;
	bool redundant;
	size_t pvs_bins;
	u32 *tmp;

	rc = of_property_read_u32_array(of_node, "qcom,pvs-fuse-redun-sel",
						pvs_fuse_redun_sel, 5);
	if (rc < 0) {
		cpr_err(cpr_vreg, "pvs-fuse-redun-sel missing: rc=%d\n", rc);
		return rc;
	}

	redundant = cpr_fuse_is_setting_expected(cpr_vreg, pvs_fuse_redun_sel);
	if (redundant) {
		rc = of_property_read_u32_array(of_node, "qcom,pvs-fuse-redun",
								pvs_fuse, 4);
		if (rc < 0) {
			cpr_err(cpr_vreg, "pvs-fuse-redun missing: rc=%d\n",
				rc);
			return rc;
		}
	} else {
		rc = of_property_read_u32_array(of_node, "qcom,pvs-fuse",
							pvs_fuse, 4);
		if (rc < 0) {
			cpr_err(cpr_vreg, "pvs-fuse missing: rc=%d\n", rc);
			return rc;
		}
	}

	/* Construct PVS process # from the efuse bits */
	efuse_bits = cpr_read_efuse_row(cpr_vreg, pvs_fuse[0], pvs_fuse[3]);
	cpr_vreg->pvs_bin = (efuse_bits >> pvs_fuse[1]) &
				((1 << pvs_fuse[2]) - 1);
	pvs_bins = 1 << pvs_fuse[2];
	stripe_size = cpr_vreg->num_fuse_corners;
	tmp = kzalloc(sizeof(u32) * pvs_bins * stripe_size, GFP_KERNEL);
	if (!tmp) {
		cpr_err(cpr_vreg, "memory alloc failed\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node, "qcom,pvs-voltage-table",
						tmp, pvs_bins * stripe_size);
	if (rc < 0) {
		cpr_err(cpr_vreg, "pvs-voltage-table missing: rc=%d\n", rc);
		kfree(tmp);
		return rc;
	}

	for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners; i++)
		cpr_vreg->pvs_corner_v[i] = tmp[cpr_vreg->pvs_bin *
						stripe_size + i - 1];
	kfree(tmp);

	return 0;
}

#define MAX_CHARS_PER_INT	10

/*
 * The initial voltage for each fuse corner may be determined by one of two
 * possible styles of fuse. If qcom,cpr-fuse-init-voltage is present, then
 * the initial voltages are encoded in a fuse for each fuse corner. If it is
 * not present, then the initial voltages are all determined using a single
 * PVS bin fuse value.
 */
static int cpr_pvs_init(struct platform_device *pdev,
			       struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int highest_fuse_corner = cpr_vreg->num_fuse_corners;
	int i, rc, pos;
	size_t buflen;
	char *buf;

	rc = of_property_read_u32(of_node, "qcom,cpr-apc-volt-step",
					&cpr_vreg->step_volt);
	if (rc < 0) {
		cpr_err(cpr_vreg, "read cpr-apc-volt-step failed, rc = %d\n",
			rc);
		return rc;
	} else if (cpr_vreg->step_volt == 0) {
		cpr_err(cpr_vreg, "apc voltage step size can't be set to 0.\n");
		return -EINVAL;
	}

	if (of_find_property(of_node, "qcom,cpr-fuse-init-voltage", NULL)) {
		rc = cpr_pvs_per_corner_init(of_node, cpr_vreg);
		if (rc < 0) {
			cpr_err(cpr_vreg, "get pvs per corner failed, rc = %d",
				rc);
			return rc;
		}
	} else {
		rc = cpr_pvs_single_bin_init(of_node, cpr_vreg);
		if (rc < 0) {
			cpr_err(cpr_vreg,
				"get pvs from single bin failed, rc = %d", rc);
			return rc;
		}
	}

	if (cpr_vreg->flags & FLAGS_UPLIFT_QUOT_VOLT) {
		rc = cpr_voltage_uplift_wa_inc_volt(cpr_vreg, of_node);
		if (rc < 0) {
			cpr_err(cpr_vreg, "pvs volt uplift wa apply failed: %d",
				rc);
			return rc;
		}
	}

	/*
	 * Allow the highest fuse corner's PVS voltage to define the ceiling
	 * voltage for that corner in order to support SoC's in which variable
	 * ceiling values are required.
	 */
	if (cpr_vreg->pvs_corner_v[highest_fuse_corner] >
		cpr_vreg->ceiling_volt[highest_fuse_corner])
		cpr_vreg->ceiling_volt[highest_fuse_corner] =
			cpr_vreg->pvs_corner_v[highest_fuse_corner];

	/*
	 * Restrict all fuse corner PVS voltages based upon per corner
	 * ceiling and floor voltages.
	 */
	for (i = CPR_FUSE_CORNER_MIN; i <= highest_fuse_corner; i++)
		if (cpr_vreg->pvs_corner_v[i] > cpr_vreg->ceiling_volt[i])
			cpr_vreg->pvs_corner_v[i] = cpr_vreg->ceiling_volt[i];
		else if (cpr_vreg->pvs_corner_v[i] < cpr_vreg->floor_volt[i])
			cpr_vreg->pvs_corner_v[i] = cpr_vreg->floor_volt[i];

	cpr_vreg->ceiling_max = cpr_vreg->ceiling_volt[highest_fuse_corner];

	/*
	 * Log ceiling, floor, and inital voltages since they are critical for
	 * all CPR debugging.
	 */
	buflen = cpr_vreg->num_fuse_corners * (MAX_CHARS_PER_INT + 2)
			* sizeof(*buf);
	buf = kzalloc(buflen, GFP_KERNEL);
	if (buf == NULL) {
		cpr_err(cpr_vreg, "Could not allocate memory for corner voltage logging\n");
		return 0;
	}

	for (i = CPR_FUSE_CORNER_MIN, pos = 0; i <= highest_fuse_corner; i++)
		pos += scnprintf(buf + pos, buflen - pos, "%u%s",
				cpr_vreg->pvs_corner_v[i],
				i < highest_fuse_corner ? " " : "");
	cpr_info(cpr_vreg, "pvs voltage: [%s] uV\n", buf);

	for (i = CPR_FUSE_CORNER_MIN, pos = 0; i <= highest_fuse_corner; i++)
		pos += scnprintf(buf + pos, buflen - pos, "%d%s",
				cpr_vreg->ceiling_volt[i],
				i < highest_fuse_corner ? " " : "");
	cpr_info(cpr_vreg, "ceiling voltage: [%s] uV\n", buf);

	for (i = CPR_FUSE_CORNER_MIN, pos = 0; i <= highest_fuse_corner; i++)
		pos += scnprintf(buf + pos, buflen - pos, "%d%s",
				cpr_vreg->floor_volt[i],
				i < highest_fuse_corner ? " " : "");
	cpr_info(cpr_vreg, "floor voltage: [%s] uV\n", buf);

	kfree(buf);
	return 0;
}

#define CPR_PROP_READ_U32(cpr_vreg, of_node, cpr_property, cpr_config, rc) \
do {									\
	if (!rc) {							\
		rc = of_property_read_u32(of_node,			\
				"qcom," cpr_property,			\
				cpr_config);				\
		if (rc) {						\
			cpr_err(cpr_vreg, "Missing " #cpr_property	\
				": rc = %d\n", rc);			\
		}							\
	}								\
} while (0)

static int cpr_apc_init(struct platform_device *pdev,
			       struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(vdd_apc_name); i++) {
		cpr_vreg->vdd_apc = devm_regulator_get(&pdev->dev,
					vdd_apc_name[i]);
		rc = PTR_RET(cpr_vreg->vdd_apc);
		if (!IS_ERR_OR_NULL(cpr_vreg->vdd_apc))
			break;
	}

	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "devm_regulator_get: rc=%d\n", rc);
		return rc;
	}

	/* Check dependencies */
	if (of_property_read_bool(of_node, "vdd-mx-supply")) {
		cpr_vreg->vdd_mx = devm_regulator_get(&pdev->dev, "vdd-mx");
		if (IS_ERR_OR_NULL(cpr_vreg->vdd_mx)) {
			rc = PTR_RET(cpr_vreg->vdd_mx);
			if (rc != -EPROBE_DEFER)
				cpr_err(cpr_vreg,
					"devm_regulator_get: vdd-mx: rc=%d\n",
					rc);
			return rc;
		}
	}

	/* Parse dependency parameters */
	if (cpr_vreg->vdd_mx) {
		rc = of_property_read_u32(of_node, "qcom,vdd-mx-vmax",
				 &cpr_vreg->vdd_mx_vmax);
		if (rc < 0) {
			cpr_err(cpr_vreg, "vdd-mx-vmax missing: rc=%d\n", rc);
			return rc;
		}

		rc = of_property_read_u32(of_node, "qcom,vdd-mx-vmin-method",
				 &cpr_vreg->vdd_mx_vmin_method);
		if (rc < 0) {
			cpr_err(cpr_vreg, "vdd-mx-vmin-method missing: rc=%d\n",
				rc);
			return rc;
		}
		if (cpr_vreg->vdd_mx_vmin_method > VDD_MX_VMIN_APC_CORNER_MAP) {
			cpr_err(cpr_vreg, "Invalid vdd-mx-vmin-method(%d)\n",
				cpr_vreg->vdd_mx_vmin_method);
			return -EINVAL;
		}

		rc = of_property_read_u32_array(of_node,
					"qcom,vdd-mx-corner-map",
					&cpr_vreg->vdd_mx_corner_map[1],
					cpr_vreg->num_fuse_corners);
		if (rc && cpr_vreg->vdd_mx_vmin_method ==
			VDD_MX_VMIN_APC_CORNER_MAP) {
			cpr_err(cpr_vreg,
				"qcom,vdd-mx-corner-map missing: rc=%d\n", rc);
			return rc;
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

static int cpr_voltage_uplift_wa_inc_quot(struct cpr_regulator *cpr_vreg,
					struct device_node *of_node)
{
	u32 delta_quot[3];
	int rc, i;

	rc = of_property_read_u32_array(of_node,
			"qcom,cpr-uplift-quotient", delta_quot, 3);
	if (rc < 0) {
		cpr_err(cpr_vreg, "cpr-uplift-quotient is missing: %d", rc);
		return rc;
	}
	for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners; i++)
		cpr_vreg->cpr_fuse_target_quot[i] += delta_quot[i-1];
	return rc;
}

static void cpr_parse_pvs_version_fuse(struct cpr_regulator *cpr_vreg,
				struct device_node *of_node)
{
	int rc;
	u64 fuse_bits;
	u32 fuse_sel[4];

	rc = of_property_read_u32_array(of_node,
			"qcom,pvs-version-fuse-sel", fuse_sel, 4);
	if (!rc) {
		fuse_bits = cpr_read_efuse_row(cpr_vreg,
				fuse_sel[0], fuse_sel[3]);
		cpr_vreg->pvs_version = (fuse_bits >> fuse_sel[1]) &
			((1 << fuse_sel[2]) - 1);
		cpr_info(cpr_vreg, "[row: %d]: 0x%llx, pvs_version = %d\n",
				fuse_sel[0], fuse_bits, cpr_vreg->pvs_version);
	} else {
		cpr_vreg->pvs_version = 0;
	}
}

/*
 * cpr_get_corner_quot_adjustment() -- get the quot_adjust for each corner.
 *
 * Get the virtual corner to fuse corner mapping and virtual corner to APC clock
 * frequency mapping from device tree.
 * Calculate the quotient adjustment scaling factor for those corners mapping
 * to the highest fuse corner.
 * Calculate the quotient adjustment for each virtual corner which maps to the
 * highest fuse corner.
 */
static int cpr_get_corner_quot_adjustment(struct cpr_regulator *cpr_vreg,
					struct device *dev)
{
	int rc = 0;
	int highest_fuse_corner = cpr_vreg->num_fuse_corners;
	int i, j, size;
	struct property *prop;
	bool corners_mapped, match_found;
	u32 *tmp, *freq_mappings = NULL;
	u32 scaling, max_factor, corner, freq_corner;
	u32 *freq_max = NULL;
	u32 *corner_max = NULL;

	prop = of_find_property(dev->of_node, "qcom,cpr-corner-map", NULL);

	if (prop) {
		size = prop->length / sizeof(u32);
		corners_mapped = true;
	} else {
		size = cpr_vreg->num_fuse_corners;
		corners_mapped = false;
	}

	cpr_vreg->corner_map = devm_kzalloc(dev, sizeof(int) * (size + 1),
					GFP_KERNEL);
	if (!cpr_vreg->corner_map) {
		cpr_err(cpr_vreg,
			"Can't allocate memory for cpr_vreg->corner_map\n");
		return -ENOMEM;
	}
	cpr_vreg->num_corners = size;

	cpr_vreg->quot_adjust = devm_kzalloc(dev,
			sizeof(u32) * (cpr_vreg->num_corners + 1),
			GFP_KERNEL);
	if (!cpr_vreg->quot_adjust) {
		cpr_err(cpr_vreg,
			"Can't allocate memory for cpr_vreg->quot_adjust\n");
		return -ENOMEM;
	}

	if (!corners_mapped) {
		for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners;
		     i++)
			cpr_vreg->corner_map[i] = i;
		return 0;
	} else {
		rc = of_property_read_u32_array(dev->of_node,
			"qcom,cpr-corner-map", &cpr_vreg->corner_map[1], size);

		if (rc) {
			cpr_err(cpr_vreg,
				"qcom,cpr-corner-map missing, rc = %d\n", rc);
			return rc;
		}

		/*
		 * Verify that the virtual corner to fuse corner mapping is
		 * valid.
		 */
		for (i = CPR_CORNER_MIN; i <= cpr_vreg->num_corners; i++) {
			if (cpr_vreg->corner_map[i] > cpr_vreg->num_fuse_corners
			    || cpr_vreg->corner_map[i] < CPR_FUSE_CORNER_MIN) {
				cpr_err(cpr_vreg, "qcom,cpr-corner-map contains an element %d which isn't in the allowed range [%d, %d]\n",
					cpr_vreg->corner_map[i],
					CPR_FUSE_CORNER_MIN,
					cpr_vreg->num_fuse_corners);
				return -EINVAL;
			}
		}
	}

	prop = of_find_property(dev->of_node,
			"qcom,cpr-speed-bin-max-corners", NULL);
	if (!prop) {
		cpr_debug(cpr_vreg, "qcom,cpr-speed-bin-max-corner missing\n");
		return 0;
	}

	size = prop->length / sizeof(u32);
	tmp = kzalloc(size * sizeof(u32), GFP_KERNEL);
	if (!tmp) {
		cpr_err(cpr_vreg, "memory alloc failed\n");
		return -ENOMEM;
	}
	rc = of_property_read_u32_array(dev->of_node,
		"qcom,cpr-speed-bin-max-corners", tmp, size);
	if (rc < 0) {
		kfree(tmp);
		cpr_err(cpr_vreg,
			"get cpr-speed-bin-max-corners failed, rc = %d\n", rc);
		return rc;
	}

	cpr_parse_pvs_version_fuse(cpr_vreg, dev->of_node);

	corner_max = kzalloc((cpr_vreg->num_fuse_corners + 1)
				* sizeof(*corner_max), GFP_KERNEL);
	freq_max = kzalloc((cpr_vreg->num_fuse_corners + 1) * sizeof(*freq_max),
				GFP_KERNEL);
	if (corner_max == NULL || freq_max == NULL) {
		cpr_err(cpr_vreg, "Could not allocate memory for quotient scaling arrays\n");
		kfree(tmp);
		rc = -ENOMEM;
		goto free_arrays;
	}

	/*
	 * Get the maximum virtual corner for each fuse corner based upon the
	 * speed_bin and pvs_version values.
	 */
	match_found = false;
	for (i = 0; i < size; i += cpr_vreg->num_fuse_corners + 2) {
		if (tmp[i] == cpr_vreg->speed_bin &&
		    tmp[i + 1] == cpr_vreg->pvs_version) {
			for (j = CPR_FUSE_CORNER_MIN;
			     j <= cpr_vreg->num_fuse_corners; j++)
				corner_max[j]
					= tmp[i + 2 + j - CPR_FUSE_CORNER_MIN];
			match_found = true;
			break;
		}
	}
	kfree(tmp);

	if (!match_found) {
		cpr_debug(cpr_vreg, "No quotient adjustment possible for speed bin=%u, pvs version=%u\n",
			cpr_vreg->speed_bin, cpr_vreg->pvs_version);
		goto free_arrays;
	}

	/* Verify that fuse corner to max virtual corner mapping is valid. */
	for (i = CPR_FUSE_CORNER_MIN; i <= highest_fuse_corner; i++) {
		if (corner_max[i] < CPR_CORNER_MIN
		    || corner_max[i] > cpr_vreg->num_corners) {
			cpr_err(cpr_vreg, "Invalid corner=%d in qcom,cpr-speed-bin-max-corners\n",
				corner_max[i]);
			goto free_arrays;
		}
	}

	/*
	 * Return success if the virtual corner values read from
	 * qcom,cpr-speed-bin-max-corners property are incorrect.  This allows
	 * the driver to continue to run without quotient scaling.
	 */
	if (corner_max[highest_fuse_corner]
	    <= corner_max[highest_fuse_corner - 1]) {
		cpr_err(cpr_vreg, "highest corner=%u should be larger than the second highest corner=%u\n",
			corner_max[highest_fuse_corner],
			corner_max[highest_fuse_corner - 1]);
		goto free_arrays;
	}

	prop = of_find_property(dev->of_node,
			"qcom,cpr-corner-frequency-map", NULL);
	if (!prop) {
		cpr_debug(cpr_vreg, "qcom,cpr-corner-frequency-map missing\n");
		goto free_arrays;
	}

	size = prop->length / sizeof(u32);
	tmp = kzalloc(sizeof(u32) * size, GFP_KERNEL);
	if (!tmp) {
		cpr_err(cpr_vreg, "memory alloc failed\n");
		rc = -ENOMEM;
		goto free_arrays;
	}
	rc = of_property_read_u32_array(dev->of_node,
		"qcom,cpr-corner-frequency-map", tmp, size);
	if (rc < 0) {
		cpr_err(cpr_vreg,
			"get cpr-corner-frequency-map failed, rc = %d\n", rc);
		kfree(tmp);
		goto free_arrays;
	}
	freq_mappings = kzalloc(sizeof(u32) * (cpr_vreg->num_corners + 1),
			GFP_KERNEL);
	if (!freq_mappings) {
		cpr_err(cpr_vreg, "memory alloc for freq_mappings failed!\n");
		kfree(tmp);
		rc = -ENOMEM;
		goto free_arrays;
	}
	for (i = 0; i < size; i += 2) {
		corner = tmp[i];
		if ((corner < 1) || (corner > cpr_vreg->num_corners)) {
			cpr_err(cpr_vreg,
				"corner should be in 1~%d range: %d\n",
				cpr_vreg->num_corners, corner);
			continue;
		}
		freq_mappings[corner] = tmp[i + 1];
		cpr_debug(cpr_vreg,
				"Frequency at virtual corner %d is %d Hz.\n",
				corner, freq_mappings[corner]);
	}
	kfree(tmp);

	rc = of_property_read_u32(dev->of_node,
		"qcom,cpr-quot-adjust-scaling-factor-max",
		&max_factor);
	if (rc < 0) {
		cpr_debug(cpr_vreg,
			"get cpr-quot-adjust-scaling-factor-max failed\n");
		rc = 0;
		goto free_arrays;
	}

	/*
	 * Get the quotient adjustment scaling factor, according to:
	 * scaling = min(1000 * (QUOT(corner_N) - QUOT(corner_N-1))
	 *		/ (freq(corner_N) - freq(corner_N-1)), max_factor)
	 *
	 * QUOT(corner_N):	quotient read from fuse for highest fuse corner
	 * QUOT(corner_N-1):	quotient read from fuse for second highest fuse
	 *			  corner
	 * freq(corner_N):	max frequency in MHz supported by the highest
	 *			  fuse corner
	 * freq(corner_N-1):	max frequency in MHz supported by the second
	 *			  highest fuse corner
	 */

	for (i = CPR_FUSE_CORNER_MIN; i <= highest_fuse_corner; i++)
		freq_max[i] = freq_mappings[corner_max[i]];
	if (freq_max[highest_fuse_corner] <= freq_max[highest_fuse_corner - 1]
	    || freq_max[highest_fuse_corner - 1] == 0) {
		cpr_err(cpr_vreg, "highest corner freq=%u should be larger than second highest corner freq=%u\n",
		      freq_max[highest_fuse_corner],
		      freq_max[highest_fuse_corner - 1]);
		rc = -EINVAL;
		goto free_arrays;
	}

	/* Convert corner max frequencies from Hz to MHz. */
	for (i = CPR_FUSE_CORNER_MIN; i <= highest_fuse_corner; i++)
		freq_max[i] /= 1000000;

	scaling = 1000 * (cpr_vreg->cpr_fuse_target_quot[highest_fuse_corner]
		      - cpr_vreg->cpr_fuse_target_quot[highest_fuse_corner - 1])
		  / (freq_max[highest_fuse_corner]
			- freq_max[highest_fuse_corner - 1]);
	scaling = min(scaling, max_factor);
	cpr_info(cpr_vreg, "quotient adjustment scaling factor: %d.%03d\n",
			scaling / 1000, scaling % 1000);

	/*
	 * Walk through the virtual corners mapped to the highest fuse corner
	 * and calculate the quotient adjustment for each one using the
	 * following formula:
	 * quot_adjust = (freq_max - freq_corner) * scaling / 1000
	 *
	 * @freq_max: max frequency in MHz supported by the highest fuse corner
	 * @freq_corner: frequency in MHz corresponding to the virtual corner
	 */
	for (i = corner_max[highest_fuse_corner];
	     i > corner_max[highest_fuse_corner - 1]; i--) {
		freq_corner = freq_mappings[i] / 1000000; /* MHz */
		if (freq_corner > 0) {
			cpr_vreg->quot_adjust[i] = scaling *
			   (freq_max[highest_fuse_corner] - freq_corner) / 1000;
		}
		cpr_info(cpr_vreg, "adjusted quotient[%d] = %d\n", i,
			(cpr_vreg->cpr_fuse_target_quot[cpr_vreg->corner_map[i]]
				- cpr_vreg->quot_adjust[i]));
	}

free_arrays:
	kfree(freq_mappings);
	kfree(corner_max);
	kfree(freq_max);
	return rc;
}

struct cpr_quot_scale {
	u32 offset;
	u32 multiplier;
};

static int cpr_init_cpr_efuse(struct platform_device *pdev,
				     struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int i, rc = 0;
	bool redundant = false, scheme_fuse_valid = false;
	bool disable_fuse_valid = false;
	u32 cpr_fuse_redun_sel[5];
	char *targ_quot_str, *ro_sel_str;
	u32 cpr_fuse_row[2];
	u32 bp_cpr_disable, bp_scheme;
	size_t len;
	int *bp_target_quot;
	int *bp_ro_sel;
	u64 fuse_bits, fuse_bits_2;
	u32 *quot_adjust;
	u32 *target_quot_size;
	struct cpr_quot_scale *quot_scale;

	len = cpr_vreg->num_fuse_corners + 1;

	bp_target_quot = kzalloc(len * sizeof(*bp_target_quot), GFP_KERNEL);
	bp_ro_sel = kzalloc(len * sizeof(*bp_ro_sel), GFP_KERNEL);
	quot_adjust = kzalloc(len * sizeof(*quot_adjust), GFP_KERNEL);
	target_quot_size = kzalloc(len * sizeof(*target_quot_size), GFP_KERNEL);
	quot_scale = kzalloc(len * sizeof(*quot_scale), GFP_KERNEL);

	if (bp_target_quot == NULL || bp_ro_sel == NULL || quot_adjust == NULL
	    || target_quot_size == NULL || quot_scale == NULL) {
		cpr_err(cpr_vreg,
			"Could not allocate memory for fuse parsing arrays\n");
		rc = -ENOMEM;
		goto error;
	}

	if (of_find_property(of_node, "qcom,cpr-fuse-redun-sel", NULL)) {
		rc = of_property_read_u32_array(of_node,
					"qcom,cpr-fuse-redun-sel",
					cpr_fuse_redun_sel, 5);
		if (rc < 0) {
			cpr_err(cpr_vreg, "cpr-fuse-redun-sel missing: rc=%d\n",
				rc);
			goto error;
		}
		redundant = cpr_fuse_is_setting_expected(cpr_vreg,
						cpr_fuse_redun_sel);
	}

	if (redundant) {
		rc = of_property_read_u32_array(of_node,
				"qcom,cpr-fuse-redun-row",
				cpr_fuse_row, 2);
		targ_quot_str = "qcom,cpr-fuse-redun-target-quot";
		ro_sel_str = "qcom,cpr-fuse-redun-ro-sel";
	} else {
		rc = of_property_read_u32_array(of_node, "qcom,cpr-fuse-row",
				cpr_fuse_row, 2);
		targ_quot_str = "qcom,cpr-fuse-target-quot";
		ro_sel_str = "qcom,cpr-fuse-ro-sel";
	}
	if (rc)
		goto error;

	rc = of_property_read_u32_array(of_node, targ_quot_str,
		&bp_target_quot[CPR_FUSE_CORNER_MIN],
		cpr_vreg->num_fuse_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "missing %s: rc=%d\n", targ_quot_str, rc);
		goto error;
	}

	if (of_property_read_bool(of_node, "qcom,cpr-fuse-target-quot-size")) {
		rc = of_property_read_u32_array(of_node,
			"qcom,cpr-fuse-target-quot-size",
			&target_quot_size[CPR_FUSE_CORNER_MIN],
			cpr_vreg->num_fuse_corners);
		if (rc < 0) {
			cpr_err(cpr_vreg, "error while reading qcom,cpr-fuse-target-quot-size: rc=%d\n",
				rc);
			goto error;
		}
	} else {
		/*
		 * Default fuse quotient parameter size to match target register
		 * size.
		 */
		for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners;
		     i++)
			target_quot_size[i] = CPR_FUSE_TARGET_QUOT_BITS;
	}

	if (of_property_read_bool(of_node, "qcom,cpr-fuse-target-quot-scale")) {
		for (i = 0; i < cpr_vreg->num_fuse_corners; i++) {
			rc = of_property_read_u32_index(of_node,
				"qcom,cpr-fuse-target-quot-scale", i * 2,
				&quot_scale[i + CPR_FUSE_CORNER_MIN].offset);
			if (rc < 0) {
				cpr_err(cpr_vreg, "error while reading qcom,cpr-fuse-target-quot-scale: rc=%d\n",
					rc);
				goto error;
			}

			rc = of_property_read_u32_index(of_node,
				"qcom,cpr-fuse-target-quot-scale", i * 2 + 1,
			       &quot_scale[i + CPR_FUSE_CORNER_MIN].multiplier);
			if (rc < 0) {
				cpr_err(cpr_vreg, "error while reading qcom,cpr-fuse-target-quot-scale: rc=%d\n",
					rc);
				goto error;
			}
		}
	} else {
		/*
		 * In the default case, target quotients require no scaling so
		 * use offset = 0, multiplier = 1.
		 */
		for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners;
		     i++) {
			quot_scale[i].offset = 0;
			quot_scale[i].multiplier = 1;
		}
	}

	rc = of_property_read_u32_array(of_node, ro_sel_str,
		&bp_ro_sel[CPR_FUSE_CORNER_MIN], cpr_vreg->num_fuse_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "missing %s: rc=%d\n", ro_sel_str, rc);
		goto error;
	}

	/* Read the control bits of eFuse */
	fuse_bits = cpr_read_efuse_row(cpr_vreg, cpr_fuse_row[0],
					cpr_fuse_row[1]);
	cpr_info(cpr_vreg, "[row:%d] = 0x%llx\n", cpr_fuse_row[0], fuse_bits);

	if (redundant) {
		if (of_property_read_bool(of_node,
				"qcom,cpr-fuse-redun-bp-cpr-disable")) {
			CPR_PROP_READ_U32(cpr_vreg, of_node,
					  "cpr-fuse-redun-bp-cpr-disable",
					  &bp_cpr_disable, rc);
			disable_fuse_valid = true;
			if (of_find_property(of_node,
					"qcom,cpr-fuse-redun-bp-scheme",
					NULL)) {
				CPR_PROP_READ_U32(cpr_vreg, of_node,
						"cpr-fuse-redun-bp-scheme",
						&bp_scheme, rc);
				scheme_fuse_valid = true;
			}
			if (rc)
				goto error;
			fuse_bits_2 = fuse_bits;
		} else {
			u32 temp_row[2];

			/* Use original fuse if no optional property */
			if (of_property_read_bool(of_node,
					"qcom,cpr-fuse-bp-cpr-disable")) {
				CPR_PROP_READ_U32(cpr_vreg, of_node,
					"cpr-fuse-bp-cpr-disable",
					&bp_cpr_disable, rc);
				disable_fuse_valid = true;
			}
			if (of_find_property(of_node,
					"qcom,cpr-fuse-bp-scheme",
					NULL)) {
				CPR_PROP_READ_U32(cpr_vreg, of_node,
						"cpr-fuse-bp-scheme",
						&bp_scheme, rc);
				scheme_fuse_valid = true;
			}
			rc = of_property_read_u32_array(of_node,
					"qcom,cpr-fuse-row",
					temp_row, 2);
			if (rc)
				goto error;

			fuse_bits_2 = cpr_read_efuse_row(cpr_vreg, temp_row[0],
							temp_row[1]);
			cpr_info(cpr_vreg, "[original row:%d] = 0x%llx\n",
				temp_row[0], fuse_bits_2);
		}
	} else {
		if (of_property_read_bool(of_node,
					"qcom,cpr-fuse-bp-cpr-disable")) {
			CPR_PROP_READ_U32(cpr_vreg, of_node,
				"cpr-fuse-bp-cpr-disable", &bp_cpr_disable, rc);
			disable_fuse_valid = true;
		}
		if (of_find_property(of_node, "qcom,cpr-fuse-bp-scheme",
							NULL)) {
			CPR_PROP_READ_U32(cpr_vreg, of_node,
					"cpr-fuse-bp-scheme", &bp_scheme, rc);
			scheme_fuse_valid = true;
		}
		if (rc)
			goto error;
		fuse_bits_2 = fuse_bits;
	}

	if (disable_fuse_valid) {
		cpr_vreg->cpr_fuse_disable =
					(fuse_bits_2 >> bp_cpr_disable) & 0x01;
		cpr_info(cpr_vreg, "CPR disable fuse = %d\n",
			cpr_vreg->cpr_fuse_disable);
	} else {
		cpr_vreg->cpr_fuse_disable = false;
	}

	if (scheme_fuse_valid) {
		cpr_vreg->cpr_fuse_local = (fuse_bits_2 >> bp_scheme) & 0x01;
		cpr_info(cpr_vreg, "local = %d\n", cpr_vreg->cpr_fuse_local);
	} else {
		cpr_vreg->cpr_fuse_local = true;
	}

	for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners; i++) {
		cpr_vreg->cpr_fuse_ro_sel[i]
			= cpr_read_efuse_param(cpr_vreg, cpr_fuse_row[0],
				bp_ro_sel[i], CPR_FUSE_RO_SEL_BITS,
				cpr_fuse_row[1]);
		cpr_vreg->cpr_fuse_target_quot[i]
			= cpr_read_efuse_param(cpr_vreg, cpr_fuse_row[0],
				bp_target_quot[i], target_quot_size[i],
				cpr_fuse_row[1]);
		/* Unpack the target quotient by scaling. */
		cpr_vreg->cpr_fuse_target_quot[i] *= quot_scale[i].multiplier;
		cpr_vreg->cpr_fuse_target_quot[i] += quot_scale[i].offset;
		cpr_info(cpr_vreg,
			"Corner[%d]: ro_sel = %d, target quot = %d\n", i,
			cpr_vreg->cpr_fuse_ro_sel[i],
			cpr_vreg->cpr_fuse_target_quot[i]);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cpr-quotient-adjustment",
		&quot_adjust[CPR_FUSE_CORNER_MIN], cpr_vreg->num_fuse_corners);
	if (!rc) {
		for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners;
		     i++) {
			cpr_vreg->cpr_fuse_target_quot[i] += quot_adjust[i];
			cpr_info(cpr_vreg,
				"Corner[%d]: adjusted target quot = %d\n",
				i, cpr_vreg->cpr_fuse_target_quot[i]);
		}
	}

	if (cpr_vreg->flags & FLAGS_UPLIFT_QUOT_VOLT) {
		cpr_voltage_uplift_wa_inc_quot(cpr_vreg, of_node);
		for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners;
		     i++) {
			cpr_info(cpr_vreg,
				"Corner[%d]: uplifted target quot = %d\n",
				i, cpr_vreg->cpr_fuse_target_quot[i]);
		}
	}

	rc = cpr_get_corner_quot_adjustment(cpr_vreg, &pdev->dev);
	if (rc)
		goto error;

	cpr_vreg->cpr_fuse_bits = fuse_bits;
	if (!cpr_vreg->cpr_fuse_bits) {
		cpr_vreg->cpr_fuse_disable = true;
		cpr_err(cpr_vreg,
			"cpr_fuse_bits == 0; permanently disabling CPR\n");
	} else {
		/*
		 * Check if the target quotients for the highest two fuse
		 * corners are too close together.
		 */
		int *quot = cpr_vreg->cpr_fuse_target_quot;
		int highest_fuse_corner = cpr_vreg->num_fuse_corners;
		bool valid_fuse = true;

		if (quot[highest_fuse_corner] > quot[highest_fuse_corner - 1]) {
			if ((quot[highest_fuse_corner]
				- quot[highest_fuse_corner - 1])
					<= CPR_FUSE_MIN_QUOT_DIFF)
				valid_fuse = false;
		} else {
			valid_fuse = false;
		}

		if (!valid_fuse) {
			cpr_vreg->cpr_fuse_disable = true;
			cpr_err(cpr_vreg, "invalid quotient values; permanently disabling CPR\n");
		}
	}


error:
	kfree(bp_target_quot);
	kfree(bp_ro_sel);
	kfree(quot_adjust);
	kfree(target_quot_size);
	kfree(quot_scale);

	return rc;
}

static int cpr_init_cpr_voltages(struct cpr_regulator *cpr_vreg,
			struct device *dev)
{
	int i;
	int size = cpr_vreg->num_corners + 1;

	cpr_vreg->last_volt = devm_kzalloc(dev, sizeof(int) * size, GFP_KERNEL);
	if (!cpr_vreg->last_volt)
		return -EINVAL;

	for (i = 1; i < size; i++) {
		cpr_vreg->last_volt[i] = cpr_vreg->pvs_corner_v
						[cpr_vreg->corner_map[i]];
	}

	return 0;
}

static int cpr_init_cpr_parameters(struct platform_device *pdev,
					  struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc = 0;

	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-ref-clk",
			  &cpr_vreg->ref_clk_khz, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-timer-delay",
			  &cpr_vreg->timer_delay_us, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-timer-cons-up",
			  &cpr_vreg->timer_cons_up, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-timer-cons-down",
			  &cpr_vreg->timer_cons_down, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-irq-line",
			  &cpr_vreg->irq_line, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-step-quotient",
			  &cpr_vreg->step_quotient, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-up-threshold",
			  &cpr_vreg->up_threshold, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-down-threshold",
			  &cpr_vreg->down_threshold, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-idle-clocks",
			  &cpr_vreg->idle_clocks, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "cpr-gcnt-time",
			  &cpr_vreg->gcnt_time_us, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "vdd-apc-step-up-limit",
			  &cpr_vreg->vdd_apc_step_up_limit, rc);
	if (rc)
		return rc;
	CPR_PROP_READ_U32(cpr_vreg, of_node, "vdd-apc-step-down-limit",
			  &cpr_vreg->vdd_apc_step_down_limit, rc);
	if (rc)
		return rc;

	rc = of_property_read_u32(of_node, "qcom,cpr-clamp-timer-interval",
				  &cpr_vreg->clamp_timer_interval);
	if (rc && rc != -EINVAL) {
		cpr_err(cpr_vreg,
			"error reading qcom,cpr-clamp-timer-interval, rc=%d\n",
			rc);
		return rc;
	}

	cpr_vreg->clamp_timer_interval = min(cpr_vreg->clamp_timer_interval,
					(u32)RBIF_TIMER_ADJ_CLAMP_INT_MASK);

	/* Init module parameter with the DT value */
	cpr_vreg->enable = of_property_read_bool(of_node, "qcom,cpr-enable");
	cpr_info(cpr_vreg, "CPR is %s by default.\n",
		cpr_vreg->enable ? "enabled" : "disabled");

	return 0;
}

static int cpr_init_cpr(struct platform_device *pdev,
			       struct cpr_regulator *cpr_vreg)
{
	struct resource *res;
	int rc = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rbcpr_clk");
	if (!res || !res->start) {
		cpr_err(cpr_vreg, "missing rbcpr_clk address: res=%p\n", res);
		return -EINVAL;
	}
	cpr_vreg->rbcpr_clk_addr = res->start;

	rc = cpr_init_cpr_efuse(pdev, cpr_vreg);
	if (rc)
		return rc;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rbcpr");
	if (!res || !res->start) {
		cpr_err(cpr_vreg, "missing rbcpr address: res=%p\n", res);
		return -EINVAL;
	}
	cpr_vreg->rbcpr_base = devm_ioremap(&pdev->dev, res->start,
					    resource_size(res));

	/* Init all voltage set points of APC regulator for CPR */
	rc = cpr_init_cpr_voltages(cpr_vreg, &pdev->dev);
	if (rc)
		return rc;

	/* Init CPR configuration parameters */
	rc = cpr_init_cpr_parameters(pdev, cpr_vreg);
	if (rc)
		return rc;

	/* Get and Init interrupt */
	cpr_vreg->cpr_irq = platform_get_irq(pdev, 0);
	if (!cpr_vreg->cpr_irq) {
		cpr_err(cpr_vreg, "missing CPR IRQ\n");
		return -EINVAL;
	}

	/* Configure CPR HW but keep it disabled */
	rc = cpr_config(cpr_vreg, &pdev->dev);
	if (rc)
		return rc;

	rc = request_threaded_irq(cpr_vreg->cpr_irq, NULL, cpr_irq_handler,
				  IRQF_ONESHOT | IRQF_TRIGGER_RISING, "cpr",
				  cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "CPR: request irq failed for IRQ %d\n",
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

static void cpr_efuse_free(struct cpr_regulator *cpr_vreg)
{
	iounmap(cpr_vreg->efuse_base);
}

static void cpr_parse_cond_min_volt_fuse(struct cpr_regulator *cpr_vreg,
						struct device_node *of_node)
{
	int rc;
	u32 fuse_sel[5];
	/*
	 * Restrict all pvs corner voltages to a minimum value of
	 * qcom,cpr-cond-min-voltage if the fuse defined in
	 * qcom,cpr-fuse-cond-min-volt-sel does not read back with
	 * the expected value.
	 */
	rc = of_property_read_u32_array(of_node,
			"qcom,cpr-fuse-cond-min-volt-sel", fuse_sel, 5);
	if (!rc) {
		if (!cpr_fuse_is_setting_expected(cpr_vreg, fuse_sel))
			cpr_vreg->flags |= FLAGS_SET_MIN_VOLTAGE;
	}
}

static void cpr_parse_speed_bin_fuse(struct cpr_regulator *cpr_vreg,
				struct device_node *of_node)
{
	int rc;
	u64 fuse_bits;
	u32 fuse_sel[4];
	u32 speed_bits;

	rc = of_property_read_u32_array(of_node,
			"qcom,speed-bin-fuse-sel", fuse_sel, 4);

	if (!rc) {
		fuse_bits = cpr_read_efuse_row(cpr_vreg,
				fuse_sel[0], fuse_sel[3]);
		speed_bits = (fuse_bits >> fuse_sel[1]) &
			((1 << fuse_sel[2]) - 1);
		cpr_info(cpr_vreg, "[row: %d]: 0x%llx, speed_bits = %d\n",
				fuse_sel[0], fuse_bits, speed_bits);
		cpr_vreg->speed_bin = speed_bits;
	} else {
		cpr_vreg->speed_bin = SPEED_BIN_NONE;
	}
}

static int cpr_voltage_uplift_enable_check(struct cpr_regulator *cpr_vreg,
					struct device_node *of_node)
{
	int rc;
	u32 fuse_sel[5];
	u32 uplift_speed_bin;

	rc = of_property_read_u32_array(of_node,
			"qcom,cpr-fuse-uplift-sel", fuse_sel, 5);
	if (!rc) {
		rc = of_property_read_u32(of_node,
				"qcom,cpr-uplift-speed-bin",
				&uplift_speed_bin);
		if (rc < 0) {
			cpr_err(cpr_vreg,
				"qcom,cpr-uplift-speed-bin missing\n");
			return rc;
		}
		if (cpr_fuse_is_setting_expected(cpr_vreg, fuse_sel)
			&& (uplift_speed_bin == cpr_vreg->speed_bin)
			&& !(cpr_vreg->flags & FLAGS_SET_MIN_VOLTAGE)) {
			cpr_vreg->flags |= FLAGS_UPLIFT_QUOT_VOLT;
		}
	}
	return 0;
}

/*
 * Read in the number of fuse corners and then allocate memory for arrays that
 * are sized based upon the number of fuse corners.
 */
static int cpr_fuse_corner_array_alloc(struct device *dev,
					struct cpr_regulator *cpr_vreg)
{
	int rc;
	size_t len;

	rc = of_property_read_u32(dev->of_node, "qcom,cpr-fuse-corners",
				&cpr_vreg->num_fuse_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "qcom,cpr-fuse-corners missing: rc=%d\n", rc);
		return rc;
	}

	if (cpr_vreg->num_fuse_corners < CPR_FUSE_CORNER_MIN
	    || cpr_vreg->num_fuse_corners > CPR_FUSE_CORNER_LIMIT) {
		cpr_err(cpr_vreg, "corner count=%d is invalid\n",
			cpr_vreg->num_fuse_corners);
		return -EINVAL;
	}

	/*
	 * The arrays sized based on the fuse corner count ignore element 0
	 * in order to simplify indexing throughout the driver since min_uV = 0
	 * cannot be passed into a set_voltage() callback.
	 */
	len = cpr_vreg->num_fuse_corners + 1;

	cpr_vreg->pvs_corner_v = devm_kzalloc(dev,
			len * sizeof(*cpr_vreg->pvs_corner_v), GFP_KERNEL);
	cpr_vreg->vdd_mx_corner_map = devm_kzalloc(dev,
			len * sizeof(*cpr_vreg->vdd_mx_corner_map), GFP_KERNEL);
	cpr_vreg->cpr_fuse_target_quot = devm_kzalloc(dev,
		len * sizeof(*cpr_vreg->cpr_fuse_target_quot), GFP_KERNEL);
	cpr_vreg->cpr_fuse_ro_sel = devm_kzalloc(dev,
			len * sizeof(*cpr_vreg->cpr_fuse_ro_sel), GFP_KERNEL);
	cpr_vreg->ceiling_volt = devm_kzalloc(dev,
			len * sizeof(*cpr_vreg->ceiling_volt), GFP_KERNEL);
	cpr_vreg->floor_volt = devm_kzalloc(dev,
			len * sizeof(*cpr_vreg->floor_volt), GFP_KERNEL);

	if (cpr_vreg->pvs_corner_v == NULL || cpr_vreg->cpr_fuse_ro_sel == NULL
	    || cpr_vreg->ceiling_volt == NULL || cpr_vreg->floor_volt == NULL
	    || cpr_vreg->vdd_mx_corner_map == NULL
	    || cpr_vreg->cpr_fuse_target_quot == NULL) {
		cpr_err(cpr_vreg, "Could not allocate memory for CPR arrays\n");
		return -ENOMEM;
	}

	return 0;
}

static int cpr_voltage_plan_init(struct platform_device *pdev,
					struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc, i;
	u32 min_uv = 0;

	rc = of_property_read_u32_array(of_node, "qcom,cpr-voltage-ceiling",
		&cpr_vreg->ceiling_volt[CPR_FUSE_CORNER_MIN],
		cpr_vreg->num_fuse_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "cpr-voltage-ceiling missing: rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cpr-voltage-floor",
		&cpr_vreg->floor_volt[CPR_FUSE_CORNER_MIN],
		cpr_vreg->num_fuse_corners);
	if (rc < 0) {
		cpr_err(cpr_vreg, "cpr-voltage-floor missing: rc=%d\n", rc);
		return rc;
	}

	cpr_parse_cond_min_volt_fuse(cpr_vreg, of_node);
	cpr_parse_speed_bin_fuse(cpr_vreg, of_node);
	rc = cpr_voltage_uplift_enable_check(cpr_vreg, of_node);
	if (rc < 0) {
		cpr_err(cpr_vreg, "voltage uplift enable check failed, %d\n",
			rc);
		return rc;
	}
	if (cpr_vreg->flags & FLAGS_SET_MIN_VOLTAGE) {
		of_property_read_u32(of_node, "qcom,cpr-cond-min-voltage",
					&min_uv);
		for (i = CPR_FUSE_CORNER_MIN; i <= cpr_vreg->num_fuse_corners;
		     i++)
			if (cpr_vreg->ceiling_volt[i] < min_uv) {
				cpr_vreg->ceiling_volt[i] = min_uv;
				cpr_vreg->floor_volt[i] = min_uv;
			} else if (cpr_vreg->floor_volt[i] < min_uv) {
				cpr_vreg->floor_volt[i] = min_uv;
			}
	}

	return 0;
}

static int cpr_mem_acc_init(struct platform_device *pdev,
				struct cpr_regulator *cpr_vreg)
{
	int rc;

	if (of_property_read_bool(pdev->dev.of_node, "mem-acc-supply")) {
		cpr_vreg->mem_acc_vreg = devm_regulator_get(&pdev->dev,
							"mem-acc");
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

#if defined(CONFIG_DEBUG_FS)

static int cpr_enable_set(void *data, u64 val)
{
	struct cpr_regulator *cpr_vreg = data;
	bool old_cpr_enable;

	if (!cpr_vreg) {
		cpr_err(cpr_vreg, "cpr-regulator pointer missing\n");
		return -ENXIO;
	}

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

	cpr_debug(cpr_vreg, "%s CPR [corner=%d, fuse_corner=%d]\n",
		cpr_vreg->enable ? "enabling" : "disabling",
		cpr_vreg->corner, cpr_vreg->corner_map[cpr_vreg->corner]);

	if (cpr_vreg->corner) {
		if (cpr_vreg->enable) {
			cpr_ctl_disable(cpr_vreg);
			cpr_irq_clr(cpr_vreg);
			cpr_corner_restore(cpr_vreg, cpr_vreg->corner);
			cpr_ctl_enable(cpr_vreg, cpr_vreg->corner);
		} else {
			cpr_ctl_disable(cpr_vreg);
			cpr_irq_set(cpr_vreg, 0);
		}
	}

_exit:
	mutex_unlock(&cpr_vreg->cpr_mutex);

	return 0;
}

static int cpr_enable_get(void *data, u64 *val)
{
	struct cpr_regulator *cpr_vreg = data;

	if (!cpr_vreg) {
		cpr_err(cpr_vreg, "cpr-regulator pointer missing\n");
		return -ENXIO;
	}

	*val = cpr_vreg->enable;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr_enable_fops, cpr_enable_get, cpr_enable_set,
			"%llu\n");

static int cpr_debug_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t cpr_debug_info_read(struct file *file, char __user *buff,
				size_t count, loff_t *ppos)
{
	struct cpr_regulator *cpr_vreg = file->private_data;
	char *debugfs_buf;
	ssize_t len, ret = 0;
	u32 gcnt, ro_sel, ctl, irq_status, reg, error_steps;
	u32 step_dn, step_up, error, error_lt0, busy;
	int fuse_corner;

	if (!cpr_vreg) {
		cpr_err(cpr_vreg, "cpr-regulator pointer missing\n");
		return -ENXIO;
	}

	debugfs_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!debugfs_buf)
		return -ENOMEM;

	mutex_lock(&cpr_vreg->cpr_mutex);

	fuse_corner = cpr_vreg->corner_map[cpr_vreg->corner];

	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
		"corner = %d, current_volt = %d uV\n",
		cpr_vreg->corner, cpr_vreg->last_volt[cpr_vreg->corner]);
	ret += len;

	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			"fuse_corner = %d, current_volt = %d uV\n",
			fuse_corner, cpr_vreg->last_volt[cpr_vreg->corner]);
	ret += len;

	ro_sel = cpr_vreg->cpr_fuse_ro_sel[fuse_corner];
	gcnt = cpr_read(cpr_vreg, REG_RBCPR_GCNT_TARGET(ro_sel));
	len = snprintf(debugfs_buf + ret, PAGE_SIZE - ret,
			"rbcpr_gcnt_target (%u) = 0x%02X\n", ro_sel, gcnt);
	ret += len;

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

static const struct file_operations cpr_debug_info_fops = {
	.open = cpr_debug_info_open,
	.read = cpr_debug_info_read,
};

static void cpr_debugfs_init(struct cpr_regulator *cpr_vreg)
{
	struct dentry *temp;

	if (IS_ERR_OR_NULL(cpr_debugfs_base)) {
		cpr_err(cpr_vreg, "Could not create debugfs nodes since base directory is missing\n");
		return;
	}

	cpr_vreg->debugfs = debugfs_create_dir(cpr_vreg->rdesc.name,
						cpr_debugfs_base);
	if (IS_ERR_OR_NULL(cpr_vreg->debugfs)) {
		cpr_err(cpr_vreg, "debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_file("debug_info", S_IRUGO, cpr_vreg->debugfs,
					cpr_vreg, &cpr_debug_info_fops);
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
}

static void cpr_debugfs_remove(struct cpr_regulator *cpr_vreg)
{
	debugfs_remove_recursive(cpr_vreg->debugfs);
}

static void cpr_debugfs_base_init(void)
{
	cpr_debugfs_base = debugfs_create_dir("cpr-regulator", NULL);
	if (IS_ERR_OR_NULL(cpr_debugfs_base))
		pr_err("cpr-regulator debugfs base directory creation failed\n");
}

static void cpr_debugfs_base_remove(void)
{
	debugfs_remove_recursive(cpr_debugfs_base);
}

#else

static void cpr_debugfs_init(struct cpr_regulator *cpr_vreg)
{}

static void cpr_debugfs_remove(struct cpr_regulator *cpr_vreg)
{}

static void cpr_debugfs_base_init(void)
{}

static void cpr_debugfs_base_remove(void)
{}

#endif

static int cpr_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
	struct cpr_regulator *cpr_vreg;
	struct regulator_desc *rdesc;
	struct device *dev = &pdev->dev;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	int rc;

	if (!pdev->dev.of_node) {
		dev_err(dev, "Device tree node is missing\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(&pdev->dev, pdev->dev.of_node);
	if (!init_data) {
		dev_err(dev, "regulator init data is missing\n");
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
		dev_err(dev, "Can't allocate cpr_regulator memory\n");
		return -ENOMEM;
	}

	cpr_vreg->rdesc.name = init_data->constraints.name;
	if (cpr_vreg->rdesc.name == NULL) {
		dev_err(dev, "regulator-name missing\n");
		return -EINVAL;
	}

	rc = cpr_fuse_corner_array_alloc(&pdev->dev, cpr_vreg);
	if (rc)
		return rc;

	rc = cpr_mem_acc_init(pdev, cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "mem_acc intialization error rc=%d\n", rc);
		return rc;
	}

	rc = cpr_efuse_init(pdev, cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Wrong eFuse address specified: rc=%d\n", rc);
		return rc;
	}

	rc = cpr_voltage_plan_init(pdev, cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Wrong DT parameter specified: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_pvs_init(pdev, cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Initialize PVS wrong: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_apc_init(pdev, cpr_vreg);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr_err(cpr_vreg, "Initialize APC wrong: rc=%d\n", rc);
		goto err_out;
	}

	rc = cpr_init_cpr(pdev, cpr_vreg);
	if (rc) {
		cpr_err(cpr_vreg, "Initialize CPR failed: rc=%d\n", rc);
		goto err_out;
	}

	cpr_efuse_free(cpr_vreg);

	/*
	 * Ensure that enable state accurately reflects the case in which CPR
	 * is permanently disabled.
	 */
	cpr_vreg->enable &= !cpr_vreg->cpr_fuse_disable;

	mutex_init(&cpr_vreg->cpr_mutex);

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

		cpr_apc_exit(cpr_vreg);
		return rc;
	}

	platform_set_drvdata(pdev, cpr_vreg);
	cpr_debugfs_init(cpr_vreg);

	mutex_lock(&cpr_regulator_list_mutex);
	list_add(&cpr_vreg->list, &cpr_regulator_list);
	mutex_unlock(&cpr_regulator_list_mutex);

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

		mutex_lock(&cpr_regulator_list_mutex);
		list_del(&cpr_vreg->list);
		mutex_unlock(&cpr_regulator_list_mutex);

		cpr_apc_exit(cpr_vreg);
		cpr_debugfs_remove(cpr_vreg);
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

	cpr_debugfs_base_init();
	return platform_driver_register(&cpr_regulator_driver);
}
EXPORT_SYMBOL(cpr_regulator_init);

static void __exit cpr_regulator_exit(void)
{
	platform_driver_unregister(&cpr_regulator_driver);
	cpr_debugfs_base_remove();
}

MODULE_DESCRIPTION("CPR regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(cpr_regulator_init);
module_exit(cpr_regulator_exit);
