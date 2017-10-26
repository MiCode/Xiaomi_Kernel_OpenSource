/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/ktime.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/qpnp-regulator.h>

/* Debug Flag Definitions */
enum {
	QPNP_VREG_DEBUG_REQUEST		= BIT(0), /* Show requests */
	QPNP_VREG_DEBUG_DUPLICATE	= BIT(1), /* Show duplicate requests */
	QPNP_VREG_DEBUG_INIT		= BIT(2), /* Show state after probe */
	QPNP_VREG_DEBUG_WRITES		= BIT(3), /* Show SPMI writes */
	QPNP_VREG_DEBUG_READS		= BIT(4), /* Show SPMI reads */
	QPNP_VREG_DEBUG_OCP		= BIT(5), /* Show VS OCP IRQ events */
};

static int qpnp_vreg_debug_mask;
module_param_named(
	debug_mask, qpnp_vreg_debug_mask, int, 0600
);

#define vreg_err(vreg, fmt, ...) \
	pr_err("%s: " fmt, vreg->rdesc.name, ##__VA_ARGS__)

/* These types correspond to unique register layouts. */
enum qpnp_regulator_logical_type {
	QPNP_REGULATOR_LOGICAL_TYPE_SMPS,
	QPNP_REGULATOR_LOGICAL_TYPE_LDO,
	QPNP_REGULATOR_LOGICAL_TYPE_VS,
	QPNP_REGULATOR_LOGICAL_TYPE_BOOST,
	QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS,
	QPNP_REGULATOR_LOGICAL_TYPE_BOOST_BYP,
	QPNP_REGULATOR_LOGICAL_TYPE_LN_LDO,
	QPNP_REGULATOR_LOGICAL_TYPE_ULT_LO_SMPS,
	QPNP_REGULATOR_LOGICAL_TYPE_ULT_HO_SMPS,
	QPNP_REGULATOR_LOGICAL_TYPE_ULT_LDO,
	QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS2,
};

enum qpnp_regulator_type {
	QPNP_REGULATOR_TYPE_BUCK		= 0x03,
	QPNP_REGULATOR_TYPE_LDO			= 0x04,
	QPNP_REGULATOR_TYPE_VS			= 0x05,
	QPNP_REGULATOR_TYPE_BOOST		= 0x1B,
	QPNP_REGULATOR_TYPE_FTS			= 0x1C,
	QPNP_REGULATOR_TYPE_BOOST_BYP		= 0x1F,
	QPNP_REGULATOR_TYPE_ULT_LDO		= 0x21,
	QPNP_REGULATOR_TYPE_ULT_BUCK		= 0x22,
};

enum qpnp_regulator_subtype {
	QPNP_REGULATOR_SUBTYPE_GP_CTL		= 0x08,
	QPNP_REGULATOR_SUBTYPE_RF_CTL		= 0x09,
	QPNP_REGULATOR_SUBTYPE_N50		= 0x01,
	QPNP_REGULATOR_SUBTYPE_N150		= 0x02,
	QPNP_REGULATOR_SUBTYPE_N300		= 0x03,
	QPNP_REGULATOR_SUBTYPE_N600		= 0x04,
	QPNP_REGULATOR_SUBTYPE_N1200		= 0x05,
	QPNP_REGULATOR_SUBTYPE_N600_ST		= 0x06,
	QPNP_REGULATOR_SUBTYPE_N1200_ST		= 0x07,
	QPNP_REGULATOR_SUBTYPE_N300_ST		= 0x15,
	QPNP_REGULATOR_SUBTYPE_P50		= 0x08,
	QPNP_REGULATOR_SUBTYPE_P150		= 0x09,
	QPNP_REGULATOR_SUBTYPE_P300		= 0x0A,
	QPNP_REGULATOR_SUBTYPE_P600		= 0x0B,
	QPNP_REGULATOR_SUBTYPE_P1200		= 0x0C,
	QPNP_REGULATOR_SUBTYPE_LN		= 0x10,
	QPNP_REGULATOR_SUBTYPE_LV_P50		= 0x28,
	QPNP_REGULATOR_SUBTYPE_LV_P150		= 0x29,
	QPNP_REGULATOR_SUBTYPE_LV_P300		= 0x2A,
	QPNP_REGULATOR_SUBTYPE_LV_P600		= 0x2B,
	QPNP_REGULATOR_SUBTYPE_LV_P1200		= 0x2C,
	QPNP_REGULATOR_SUBTYPE_LV100		= 0x01,
	QPNP_REGULATOR_SUBTYPE_LV300		= 0x02,
	QPNP_REGULATOR_SUBTYPE_MV300		= 0x08,
	QPNP_REGULATOR_SUBTYPE_MV500		= 0x09,
	QPNP_REGULATOR_SUBTYPE_HDMI		= 0x10,
	QPNP_REGULATOR_SUBTYPE_OTG		= 0x11,
	QPNP_REGULATOR_SUBTYPE_5V_BOOST		= 0x01,
	QPNP_REGULATOR_SUBTYPE_FTS_CTL		= 0x08,
	QPNP_REGULATOR_SUBTYPE_FTS2p5_CTL	= 0x09,
	QPNP_REGULATOR_SUBTYPE_FTS426		= 0x0A,
	QPNP_REGULATOR_SUBTYPE_BB_2A		= 0x01,
	QPNP_REGULATOR_SUBTYPE_ULT_HF_CTL1	= 0x0D,
	QPNP_REGULATOR_SUBTYPE_ULT_HF_CTL2	= 0x0E,
	QPNP_REGULATOR_SUBTYPE_ULT_HF_CTL3	= 0x0F,
	QPNP_REGULATOR_SUBTYPE_ULT_HF_CTL4	= 0x10,
};

/* First common register layout used by older devices */
enum qpnp_common_regulator_registers {
	QPNP_COMMON_REG_DIG_MAJOR_REV		= 0x01,
	QPNP_COMMON_REG_TYPE			= 0x04,
	QPNP_COMMON_REG_SUBTYPE			= 0x05,
	QPNP_COMMON_REG_VOLTAGE_RANGE		= 0x40,
	QPNP_COMMON_REG_VOLTAGE_SET		= 0x41,
	QPNP_COMMON_REG_MODE			= 0x45,
	QPNP_COMMON_REG_ENABLE			= 0x46,
	QPNP_COMMON_REG_PULL_DOWN		= 0x48,
	QPNP_COMMON_REG_STEP_CTRL		= 0x61,
	QPNP_COMMON_REG_UL_LL_CTRL		= 0x68,
	QPNP_COMMON_REG_VOLTAGE_ULS_VALID	= 0x6A,
	QPNP_COMMON_REG_VOLTAGE_LLS_VALID	= 0x6C,
};

/*
 * Second common register layout used by newer devices
 * Note that some of the registers from the first common layout remain
 * unchanged and their definition is not duplicated.
 */
enum qpnp_common2_regulator_registers {
	QPNP_COMMON2_REG_VOLTAGE_LSB		= 0x40,
	QPNP_COMMON2_REG_VOLTAGE_MSB		= 0x41,
	QPNP_COMMON2_REG_MODE			= 0x45,
	QPNP_COMMON2_REG_STEP_CTRL		= 0x61,
	QPNP_COMMON2_REG_VOLTAGE_ULS_LSB	= 0x68,
	QPNP_COMMON2_REG_VOLTAGE_ULS_MSB	= 0x69,
};

enum qpnp_ldo_registers {
	QPNP_LDO_REG_SOFT_START			= 0x4C,
};

enum qpnp_vs_registers {
	QPNP_VS_REG_OCP				= 0x4A,
	QPNP_VS_REG_SOFT_START			= 0x4C,
};

enum qpnp_boost_registers {
	QPNP_BOOST_REG_CURRENT_LIMIT		= 0x4A,
};

enum qpnp_boost_byp_registers {
	QPNP_BOOST_BYP_REG_CURRENT_LIMIT	= 0x4B,
};

/* Used for indexing into ctrl_reg.  These are offets from 0x40 */
enum qpnp_common_control_register_index {
	QPNP_COMMON_IDX_VOLTAGE_RANGE		= 0,
	QPNP_COMMON_IDX_VOLTAGE_SET		= 1,
	QPNP_COMMON_IDX_MODE			= 5,
	QPNP_COMMON_IDX_ENABLE			= 6,
};

enum qpnp_common2_control_register_index {
	QPNP_COMMON2_IDX_VOLTAGE_LSB		= 0,
	QPNP_COMMON2_IDX_VOLTAGE_MSB		= 1,
	QPNP_COMMON2_IDX_MODE			= 5,
};

/* Common regulator control register layout */
#define QPNP_COMMON_ENABLE_MASK			0x80
#define QPNP_COMMON_ENABLE			0x80
#define QPNP_COMMON_DISABLE			0x00
#define QPNP_COMMON_ENABLE_FOLLOW_HW_EN3_MASK	0x08
#define QPNP_COMMON_ENABLE_FOLLOW_HW_EN2_MASK	0x04
#define QPNP_COMMON_ENABLE_FOLLOW_HW_EN1_MASK	0x02
#define QPNP_COMMON_ENABLE_FOLLOW_HW_EN0_MASK	0x01
#define QPNP_COMMON_ENABLE_FOLLOW_ALL_MASK	0x0F

/* First common regulator mode register layout */
#define QPNP_COMMON_MODE_HPM_MASK		0x80
#define QPNP_COMMON_MODE_AUTO_MASK		0x40
#define QPNP_COMMON_MODE_BYPASS_MASK		0x20
#define QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK	0x10
#define QPNP_COMMON_MODE_FOLLOW_HW_EN3_MASK	0x08
#define QPNP_COMMON_MODE_FOLLOW_HW_EN2_MASK	0x04
#define QPNP_COMMON_MODE_FOLLOW_HW_EN1_MASK	0x02
#define QPNP_COMMON_MODE_FOLLOW_HW_EN0_MASK	0x01
#define QPNP_COMMON_MODE_FOLLOW_ALL_MASK	0x1F

/* Second common regulator mode register values */
#define QPNP_COMMON2_MODE_BYPASS		3
#define QPNP_COMMON2_MODE_RETENTION		4
#define QPNP_COMMON2_MODE_LPM			5
#define QPNP_COMMON2_MODE_AUTO			6
#define QPNP_COMMON2_MODE_HPM			7

#define QPNP_COMMON2_MODE_MASK			0x07

/* Common regulator pull down control register layout */
#define QPNP_COMMON_PULL_DOWN_ENABLE_MASK	0x80

/* Common regulator UL & LL limits control register layout */
#define QPNP_COMMON_UL_EN_MASK			0x80
#define QPNP_COMMON_LL_EN_MASK			0x40

/* LDO regulator current limit control register layout */
#define QPNP_LDO_CURRENT_LIMIT_ENABLE_MASK	0x80

/* LDO regulator soft start control register layout */
#define QPNP_LDO_SOFT_START_ENABLE_MASK		0x80

/* VS regulator over current protection control register layout */
#define QPNP_VS_OCP_OVERRIDE			0x01
#define QPNP_VS_OCP_NO_OVERRIDE			0x00

/* VS regulator soft start control register layout */
#define QPNP_VS_SOFT_START_ENABLE_MASK		0x80
#define QPNP_VS_SOFT_START_SEL_MASK		0x03

/* Boost regulator current limit control register layout */
#define QPNP_BOOST_CURRENT_LIMIT_ENABLE_MASK	0x80
#define QPNP_BOOST_CURRENT_LIMIT_MASK		0x07

#define QPNP_VS_OCP_DEFAULT_MAX_RETRIES		10
#define QPNP_VS_OCP_DEFAULT_RETRY_DELAY_MS	30
#define QPNP_VS_OCP_FALL_DELAY_US		90
#define QPNP_VS_OCP_FAULT_DELAY_US		20000

#define QPNP_FTSMPS_STEP_CTRL_STEP_MASK		0x18
#define QPNP_FTSMPS_STEP_CTRL_STEP_SHIFT	3
#define QPNP_FTSMPS_STEP_CTRL_DELAY_MASK	0x07
#define QPNP_FTSMPS_STEP_CTRL_DELAY_SHIFT	0

/* Clock rate in kHz of the FTSMPS regulator reference clock. */
#define QPNP_FTSMPS_CLOCK_RATE		19200

/* Minimum voltage stepper delay for each step. */
#define QPNP_FTSMPS_STEP_DELAY		8

/*
 * The ratio QPNP_FTSMPS_STEP_MARGIN_NUM/QPNP_FTSMPS_STEP_MARGIN_DEN is used to
 * adjust the step rate in order to account for oscillator variance.
 */
#define QPNP_FTSMPS_STEP_MARGIN_NUM	4
#define QPNP_FTSMPS_STEP_MARGIN_DEN	5

#define QPNP_FTSMPS2_STEP_CTRL_DELAY_MASK	0x03
#define QPNP_FTSMPS2_STEP_CTRL_DELAY_SHIFT	0

/* Clock rate in kHz of the FTSMPS2 regulator reference clock. */
#define QPNP_FTSMPS2_CLOCK_RATE		4800

/* Minimum voltage stepper delay for each step. */
#define QPNP_FTSMPS2_STEP_DELAY		2

/*
 * The ratio QPNP_FTSMPS2_STEP_MARGIN_NUM/QPNP_FTSMPS2_STEP_MARGIN_DEN is used
 * to adjust the step rate in order to account for oscillator variance.
 */
#define QPNP_FTSMPS2_STEP_MARGIN_NUM	10
#define QPNP_FTSMPS2_STEP_MARGIN_DEN	11

/*
 * This voltage in uV is returned by get_voltage functions when there is no way
 * to determine the current voltage level.  It is needed because the regulator
 * framework treats a 0 uV voltage as an error.
 */
#define VOLTAGE_UNKNOWN 1

/* VSET value to decide the range of ULT SMPS */
#define ULT_SMPS_RANGE_SPLIT 0x60

/**
 * struct qpnp_voltage_range - regulator set point voltage mapping description
 * @min_uV:		Minimum programmable output voltage resulting from
 *			set point register value 0x00
 * @max_uV:		Maximum programmable output voltage
 * @step_uV:		Output voltage increase resulting from the set point
 *			register value increasing by 1
 * @set_point_min_uV:	Minimum allowed voltage
 * @set_point_max_uV:	Maximum allowed voltage.  This may be tweaked in order
 *			to pick which range should be used in the case of
 *			overlapping set points.
 * @n_voltages:		Number of preferred voltage set points present in this
 *			range
 * @range_sel:		Voltage range register value corresponding to this range
 *
 * The following relationships must be true for the values used in this struct:
 * (max_uV - min_uV) % step_uV == 0
 * (set_point_min_uV - min_uV) % step_uV == 0*
 * (set_point_max_uV - min_uV) % step_uV == 0*
 * n_voltages = (set_point_max_uV - set_point_min_uV) / step_uV + 1
 *
 * *Note, set_point_min_uV == set_point_max_uV == 0 is allowed in order to
 * specify that the voltage range has meaning, but is not preferred.
 */
struct qpnp_voltage_range {
	int					min_uV;
	int					max_uV;
	int					step_uV;
	int					set_point_min_uV;
	int					set_point_max_uV;
	unsigned int				n_voltages;
	u8					range_sel;
};

/*
 * The ranges specified in the qpnp_voltage_set_points struct must be listed
 * so that range[i].set_point_max_uV < range[i+1].set_point_min_uV.
 */
struct qpnp_voltage_set_points {
	struct qpnp_voltage_range		*range;
	int					count;
	unsigned int				n_voltages;
};

struct qpnp_regulator_mapping {
	enum qpnp_regulator_type		type;
	enum qpnp_regulator_subtype		subtype;
	enum qpnp_regulator_logical_type	logical_type;
	u32					revision_min;
	u32					revision_max;
	struct regulator_ops			*ops;
	struct qpnp_voltage_set_points		*set_points;
	int					hpm_min_load;
};

struct qpnp_regulator {
	struct regulator_desc			rdesc;
	struct delayed_work			ocp_work;
	struct platform_device			*pdev;
	struct regmap				*regmap;
	struct regulator_dev			*rdev;
	struct qpnp_voltage_set_points		*set_points;
	enum qpnp_regulator_logical_type	logical_type;
	int					enable_time;
	int					ocp_enable;
	int					ocp_irq;
	int					ocp_count;
	int					ocp_max_retries;
	int					ocp_retry_delay_ms;
	int					system_load;
	int					hpm_min_load;
	int					slew_rate;
	u32					write_count;
	u32					prev_write_count;
	ktime_t					vs_enable_time;
	u16					base_addr;
	/* ctrl_reg provides a shadow copy of register values 0x40 to 0x47. */
	u8					ctrl_reg[8];
	u8					init_mode;
};

#define QPNP_VREG_MAP(_type, _subtype, _dig_major_min, _dig_major_max, \
		      _logical_type, _ops_val, _set_points_val, _hpm_min_load) \
	{ \
		.type		= QPNP_REGULATOR_TYPE_##_type, \
		.subtype	= QPNP_REGULATOR_SUBTYPE_##_subtype, \
		.revision_min	= _dig_major_min, \
		.revision_max	= _dig_major_max, \
		.logical_type	= QPNP_REGULATOR_LOGICAL_TYPE_##_logical_type, \
		.ops		= &qpnp_##_ops_val##_ops, \
		.set_points	= &_set_points_val##_set_points, \
		.hpm_min_load	= _hpm_min_load, \
	}

#define VOLTAGE_RANGE(_range_sel, _min_uV, _set_point_min_uV, \
			_set_point_max_uV, _max_uV, _step_uV) \
	{ \
		.min_uV			= _min_uV, \
		.max_uV			= _max_uV, \
		.set_point_min_uV	= _set_point_min_uV, \
		.set_point_max_uV	= _set_point_max_uV, \
		.step_uV		= _step_uV, \
		.range_sel		= _range_sel, \
	}

#define SET_POINTS(_ranges) \
{ \
	.range	= _ranges, \
	.count	= ARRAY_SIZE(_ranges), \
}

/*
 * These tables contain the physically available PMIC regulator voltage setpoint
 * ranges.  Where two ranges overlap in hardware, one of the ranges is trimmed
 * to ensure that the setpoints available to software are monotonically
 * increasing and unique.  The set_voltage callback functions expect these
 * properties to hold.
 */
static struct qpnp_voltage_range pldo_ranges[] = {
	VOLTAGE_RANGE(2,  750000,  750000, 1537500, 1537500, 12500),
	VOLTAGE_RANGE(3, 1500000, 1550000, 3075000, 3075000, 25000),
	VOLTAGE_RANGE(4, 1750000, 3100000, 4900000, 4900000, 50000),
};

static struct qpnp_voltage_range nldo1_ranges[] = {
	VOLTAGE_RANGE(2,  750000,  750000, 1537500, 1537500, 12500),
};

static struct qpnp_voltage_range nldo2_ranges[] = {
	VOLTAGE_RANGE(0,  375000,       0,       0, 1537500, 12500),
	VOLTAGE_RANGE(1,  375000,  375000,  768750,  768750,  6250),
	VOLTAGE_RANGE(2,  750000,  775000, 1537500, 1537500, 12500),
};

static struct qpnp_voltage_range nldo3_ranges[] = {
	VOLTAGE_RANGE(0,  375000,  375000, 1537500, 1537500, 12500),
	VOLTAGE_RANGE(1,  375000,       0,       0, 1537500, 12500),
	VOLTAGE_RANGE(2,  750000,       0,       0, 1537500, 12500),
};

static struct qpnp_voltage_range ln_ldo_ranges[] = {
	VOLTAGE_RANGE(1,  690000,  690000, 1110000, 1110000, 60000),
	VOLTAGE_RANGE(0, 1380000, 1380000, 2220000, 2220000, 120000),
};

static struct qpnp_voltage_range smps_ranges[] = {
	VOLTAGE_RANGE(0,  375000,  375000, 1562500, 1562500, 12500),
	VOLTAGE_RANGE(1, 1550000, 1575000, 3125000, 3125000, 25000),
};

static struct qpnp_voltage_range ftsmps_ranges[] = {
	VOLTAGE_RANGE(0,       0,  350000, 1275000, 1275000,  5000),
	VOLTAGE_RANGE(1,       0, 1280000, 2040000, 2040000, 10000),
};

static struct qpnp_voltage_range ftsmps2p5_ranges[] = {
	VOLTAGE_RANGE(0,   80000,  350000, 1355000, 1355000,  5000),
	VOLTAGE_RANGE(1,  160000, 1360000, 2200000, 2200000, 10000),
};

static struct qpnp_voltage_range boost_ranges[] = {
	VOLTAGE_RANGE(0, 4000000, 4000000, 5550000, 5550000, 50000),
};

static struct qpnp_voltage_range boost_byp_ranges[] = {
	VOLTAGE_RANGE(0, 2500000, 2500000, 5200000, 5650000, 50000),
};

static struct qpnp_voltage_range ult_lo_smps_ranges[] = {
	VOLTAGE_RANGE(0,  375000,  375000, 1562500, 1562500, 12500),
	VOLTAGE_RANGE(1,  750000,       0,       0, 1525000, 25000),
};

static struct qpnp_voltage_range ult_ho_smps_ranges[] = {
	VOLTAGE_RANGE(0, 1550000, 1550000, 2325000, 2325000, 25000),
};

static struct qpnp_voltage_range ult_nldo_ranges[] = {
	VOLTAGE_RANGE(0,  375000,  375000, 1537500, 1537500, 12500),
};

static struct qpnp_voltage_range ult_pldo_ranges[] = {
	VOLTAGE_RANGE(0, 1750000, 1750000, 3337500, 3337500, 12500),
};

static struct qpnp_voltage_range ftsmps426_ranges[] = {
	VOLTAGE_RANGE(0,       0,  320000, 1352000, 1352000,  4000),
};

static struct qpnp_voltage_set_points pldo_set_points = SET_POINTS(pldo_ranges);
static struct qpnp_voltage_set_points nldo1_set_points
					= SET_POINTS(nldo1_ranges);
static struct qpnp_voltage_set_points nldo2_set_points
					= SET_POINTS(nldo2_ranges);
static struct qpnp_voltage_set_points nldo3_set_points
					= SET_POINTS(nldo3_ranges);
static struct qpnp_voltage_set_points ln_ldo_set_points
					= SET_POINTS(ln_ldo_ranges);
static struct qpnp_voltage_set_points smps_set_points = SET_POINTS(smps_ranges);
static struct qpnp_voltage_set_points ftsmps_set_points
					= SET_POINTS(ftsmps_ranges);
static struct qpnp_voltage_set_points ftsmps2p5_set_points
					= SET_POINTS(ftsmps2p5_ranges);
static struct qpnp_voltage_set_points boost_set_points
					= SET_POINTS(boost_ranges);
static struct qpnp_voltage_set_points boost_byp_set_points
					= SET_POINTS(boost_byp_ranges);
static struct qpnp_voltage_set_points ult_lo_smps_set_points
					= SET_POINTS(ult_lo_smps_ranges);
static struct qpnp_voltage_set_points ult_ho_smps_set_points
					= SET_POINTS(ult_ho_smps_ranges);
static struct qpnp_voltage_set_points ult_nldo_set_points
					= SET_POINTS(ult_nldo_ranges);
static struct qpnp_voltage_set_points ult_pldo_set_points
					= SET_POINTS(ult_pldo_ranges);
static struct qpnp_voltage_set_points ftsmps426_set_points
					= SET_POINTS(ftsmps426_ranges);
static struct qpnp_voltage_set_points none_set_points;

static struct qpnp_voltage_set_points *all_set_points[] = {
	&pldo_set_points,
	&nldo1_set_points,
	&nldo2_set_points,
	&nldo3_set_points,
	&ln_ldo_set_points,
	&smps_set_points,
	&ftsmps_set_points,
	&ftsmps2p5_set_points,
	&boost_set_points,
	&boost_byp_set_points,
	&ult_lo_smps_set_points,
	&ult_ho_smps_set_points,
	&ult_nldo_set_points,
	&ult_pldo_set_points,
	&ftsmps426_set_points,
};

/* Determines which label to add to a debug print statement. */
enum qpnp_regulator_action {
	QPNP_REGULATOR_ACTION_INIT,
	QPNP_REGULATOR_ACTION_ENABLE,
	QPNP_REGULATOR_ACTION_DISABLE,
	QPNP_REGULATOR_ACTION_VOLTAGE,
	QPNP_REGULATOR_ACTION_MODE,
};

static void qpnp_vreg_show_state(struct regulator_dev *rdev,
				   enum qpnp_regulator_action action);

#define DEBUG_PRINT_BUFFER_SIZE 64
static void fill_string(char *str, size_t str_len, u8 *buf, int buf_len)
{
	int pos = 0;
	int i;

	for (i = 0; i < buf_len; i++) {
		pos += scnprintf(str + pos, str_len - pos, "0x%02X", buf[i]);
		if (i < buf_len - 1)
			pos += scnprintf(str + pos, str_len - pos, ", ");
	}
}

static inline int qpnp_vreg_read(struct qpnp_regulator *vreg, u16 addr, u8 *buf,
				 int len)
{
	char str[DEBUG_PRINT_BUFFER_SIZE];
	int rc = 0;

	rc = regmap_bulk_read(vreg->regmap, vreg->base_addr + addr, buf, len);

	if (!rc && (qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_READS)) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, buf, len);
		pr_info(" %-11s:  read(0x%04X), sid=%d, len=%d; %s\n",
			vreg->rdesc.name, vreg->base_addr + addr,
			to_spmi_device(vreg->pdev->dev.parent)->usid, len,
			str);
	}

	return rc;
}

static inline int qpnp_vreg_write(struct qpnp_regulator *vreg, u16 addr,
				u8 *buf, int len)
{
	char str[DEBUG_PRINT_BUFFER_SIZE];
	int rc = 0;

	if (qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_WRITES) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, buf, len);
		pr_info("%-11s: write(0x%04X), sid=%d, len=%d; %s\n",
			vreg->rdesc.name, vreg->base_addr + addr,
			to_spmi_device(vreg->pdev->dev.parent)->usid, len,
			str);
	}

	rc = regmap_bulk_write(vreg->regmap, vreg->base_addr + addr, buf, len);
	if (!rc)
		vreg->write_count += len;

	return rc;
}

/*
 * qpnp_vreg_write_optimized - write the minimum sized contiguous subset of buf
 * @vreg:	qpnp_regulator pointer for this regulator
 * @addr:	local SPMI address offset from this peripheral's base address
 * @buf:	new data to write into the SPMI registers
 * @buf_save:	old data in the registers
 * @len:	number of bytes to write
 *
 * This function checks for unchanged register values between buf and buf_save
 * starting at both ends of buf.  Only the contiguous subset in the middle of
 * buf starting and ending with new values is sent.
 *
 * Consider the following example:
 * buf offset: 0 1 2 3 4 5 6 7
 * reg state:  U U C C U C U U
 * (U = unchanged, C = changed)
 * In this example registers 2 through 5 will be written with a single
 * transaction.
 */
static inline int qpnp_vreg_write_optimized(struct qpnp_regulator *vreg,
		u16 addr, u8 *buf, u8 *buf_save, int len)
{
	int i, rc, start, end;

	for (i = 0; i < len; i++)
		if (buf[i] != buf_save[i])
			break;
	start = i;

	for (i = len - 1; i >= 0; i--)
		if (buf[i] != buf_save[i])
			break;
	end = i;

	if (start > end) {
		/* No modified register values present. */
		return 0;
	}

	rc = qpnp_vreg_write(vreg, addr + start, &buf[start], end - start + 1);
	if (!rc)
		for (i = start; i <= end; i++)
			buf_save[i] = buf[i];

	return rc;
}

/*
 * Perform a masked write to a PMIC register only if the new value differs
 * from the last value written to the register.  This removes redundant
 * register writing.
 */
static int qpnp_vreg_masked_write(struct qpnp_regulator *vreg, u16 addr, u8 val,
		u8 mask, u8 *reg_save)
{
	int rc = 0;
	u8 reg;

	reg = (*reg_save & ~mask) | (val & mask);
	if (reg != *reg_save) {
		rc = qpnp_vreg_write(vreg, addr, &reg, 1);

		if (rc) {
			vreg_err(vreg, "write failed; addr=0x%03X, rc=%d\n",
				addr, rc);
		} else {
			*reg_save = reg;
		}
	}

	return rc;
}

/*
 * Perform a masked read-modify-write to a PMIC register only if the new value
 * differs from the value currently in the register.  This removes redundant
 * register writing.
 */
static int qpnp_vreg_masked_read_write(struct qpnp_regulator *vreg, u16 addr,
		u8 val, u8 mask)
{
	int rc;
	u8 reg;

	rc = qpnp_vreg_read(vreg, addr, &reg, 1);
	if (rc) {
		vreg_err(vreg, "read failed; addr=0x%03X, rc=%d\n", addr, rc);
		return rc;
	}

	return qpnp_vreg_masked_write(vreg, addr, val, mask, &reg);
}

static int qpnp_regulator_common_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);

	return (vreg->ctrl_reg[QPNP_COMMON_IDX_ENABLE]
		& QPNP_COMMON_ENABLE_MASK)
			== QPNP_COMMON_ENABLE;
}

static int qpnp_regulator_common_enable(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_ENABLE,
		QPNP_COMMON_ENABLE, QPNP_COMMON_ENABLE_MASK,
		&vreg->ctrl_reg[QPNP_COMMON_IDX_ENABLE]);

	if (rc)
		vreg_err(vreg, "qpnp_vreg_masked_write failed, rc=%d\n", rc);
	else
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_ENABLE);

	return rc;
}

static int qpnp_regulator_vs_enable(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);

	if (vreg->ocp_irq) {
		vreg->ocp_count = 0;
		vreg->vs_enable_time = ktime_get();
	}

	return qpnp_regulator_common_enable(rdev);
}

static int qpnp_regulator_common_disable(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_ENABLE,
		QPNP_COMMON_DISABLE, QPNP_COMMON_ENABLE_MASK,
		&vreg->ctrl_reg[QPNP_COMMON_IDX_ENABLE]);

	if (rc)
		vreg_err(vreg, "qpnp_vreg_masked_write failed, rc=%d\n", rc);
	else
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_DISABLE);

	return rc;
}

/*
 * Returns 1 if the voltage can be set in the current range, 0 if the voltage
 * cannot be set in the current range, or errno if an error occurred.
 */
static int qpnp_regulator_select_voltage_same_range(struct qpnp_regulator *vreg,
		int min_uV, int max_uV, int *range_sel, int *voltage_sel,
		unsigned int *selector)
{
	struct qpnp_voltage_range *range = NULL;
	int uV = min_uV;
	int i;

	*range_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE];

	for (i = 0; i < vreg->set_points->count; i++) {
		if (vreg->set_points->range[i].range_sel == *range_sel) {
			range = &vreg->set_points->range[i];
			break;
		}
	}

	if (!range) {
		/* Unknown range */
		return 0;
	}

	if (uV < range->min_uV && max_uV >= range->min_uV)
		uV = range->min_uV;

	if (uV < range->min_uV || uV > range->max_uV) {
		/* Current range doesn't support the requested voltage. */
		return 0;
	}

	/*
	 * Force uV to be an allowed set point by applying a ceiling function to
	 * the uV value.
	 */
	*voltage_sel = DIV_ROUND_UP(uV - range->min_uV, range->step_uV);
	uV = *voltage_sel * range->step_uV + range->min_uV;

	if (uV > max_uV) {
		/*
		 * No set point in the current voltage range is within the
		 * requested min_uV to max_uV range.
		 */
		return 0;
	}

	*selector = 0;
	for (i = 0; i < vreg->set_points->count; i++) {
		if (uV >= vreg->set_points->range[i].set_point_min_uV
		    && uV <= vreg->set_points->range[i].set_point_max_uV) {
			*selector +=
			    (uV - vreg->set_points->range[i].set_point_min_uV)
				/ vreg->set_points->range[i].step_uV;
			break;
		}

		*selector += vreg->set_points->range[i].n_voltages;
	}

	if (*selector >= vreg->set_points->n_voltages)
		return 0;

	return 1;
}

static int qpnp_regulator_select_voltage(struct qpnp_regulator *vreg,
		int min_uV, int max_uV, int *range_sel, int *voltage_sel,
		unsigned int *selector)
{
	struct qpnp_voltage_range *range;
	int uV = min_uV;
	int lim_min_uV, lim_max_uV, i, range_id, range_max_uV;

	/* Check if request voltage is outside of physically settable range. */
	lim_min_uV = vreg->set_points->range[0].set_point_min_uV;
	lim_max_uV =
	  vreg->set_points->range[vreg->set_points->count - 1].set_point_max_uV;

	if (uV < lim_min_uV && max_uV >= lim_min_uV)
		uV = lim_min_uV;

	if (uV < lim_min_uV || uV > lim_max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, lim_min_uV, lim_max_uV);
		return -EINVAL;
	}

	/* Find the range which uV is inside of. */
	for (i = vreg->set_points->count - 1; i > 0; i--) {
		range_max_uV = vreg->set_points->range[i - 1].set_point_max_uV;
		if (uV > range_max_uV && range_max_uV > 0)
			break;
	}

	range_id = i;
	range = &vreg->set_points->range[range_id];
	*range_sel = range->range_sel;

	/*
	 * Force uV to be an allowed set point by applying a ceiling function to
	 * the uV value.
	 */
	*voltage_sel = (uV - range->min_uV + range->step_uV - 1)
			/ range->step_uV;
	uV = *voltage_sel * range->step_uV + range->min_uV;

	if (uV > max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] cannot be met by any set point; "
			"next set point: %d\n",
			min_uV, max_uV, uV);
		return -EINVAL;
	}

	*selector = 0;
	for (i = 0; i < range_id; i++)
		*selector += vreg->set_points->range[i].n_voltages;
	*selector += (uV - range->set_point_min_uV) / range->step_uV;

	return 0;
}

static int qpnp_regulator_delay_for_slewing(struct qpnp_regulator *vreg,
		int prev_voltage)
{
	int current_voltage;

	/* Delay for voltage slewing if a step rate is specified. */
	if (vreg->slew_rate && vreg->rdesc.ops->get_voltage) {
		current_voltage = vreg->rdesc.ops->get_voltage(vreg->rdev);
		if (current_voltage < 0) {
			vreg_err(vreg, "could not get new voltage, rc=%d\n",
				current_voltage);
			return current_voltage;
		}

		udelay(DIV_ROUND_UP(abs(current_voltage - prev_voltage),
					vreg->slew_rate));
	}

	return 0;
}

static int qpnp_regulator_common_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc, range_sel, voltage_sel, voltage_old = 0;
	u8 buf[2];

	if (vreg->slew_rate && vreg->rdesc.ops->get_voltage) {
		voltage_old = vreg->rdesc.ops->get_voltage(rdev);
		if (voltage_old < 0) {
			vreg_err(vreg, "could not get current voltage, rc=%d\n",
				voltage_old);
			return voltage_old;
		}
	}

	/*
	 * Favor staying in the current voltage range if possible.  This avoids
	 * voltage spikes that occur when changing the voltage range.
	 */
	rc = qpnp_regulator_select_voltage_same_range(vreg, min_uV, max_uV,
		&range_sel, &voltage_sel, selector);
	if (rc == 0)
		rc = qpnp_regulator_select_voltage(vreg, min_uV, max_uV,
			&range_sel, &voltage_sel, selector);
	if (rc < 0) {
		vreg_err(vreg, "could not set voltage, rc=%d\n", rc);
		return rc;
	}

	buf[0] = range_sel;
	buf[1] = voltage_sel;
	if ((vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE] != range_sel)
	    && (vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET] == voltage_sel)) {
		/* Handle latched range change. */
		rc = qpnp_vreg_write(vreg, QPNP_COMMON_REG_VOLTAGE_RANGE,
				buf, 2);
		if (!rc) {
			vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE] = buf[0];
			vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET] = buf[1];
		}
	} else {
		/* Either write can be optimized away safely. */
		rc = qpnp_vreg_write_optimized(vreg,
			QPNP_COMMON_REG_VOLTAGE_RANGE, buf,
			&vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE], 2);
	}

	if (rc) {
		vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
	} else {
		rc = qpnp_regulator_delay_for_slewing(vreg, voltage_old);
		if (rc)
			return rc;

		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_VOLTAGE);
	}

	return rc;
}

static int qpnp_regulator_common_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	struct qpnp_voltage_range *range = NULL;
	int range_sel, voltage_sel, i;

	range_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE];
	voltage_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET];

	for (i = 0; i < vreg->set_points->count; i++) {
		if (vreg->set_points->range[i].range_sel == range_sel) {
			range = &vreg->set_points->range[i];
			break;
		}
	}

	if (!range) {
		vreg_err(vreg, "voltage unknown, range %d is invalid\n",
			range_sel);
		return VOLTAGE_UNKNOWN;
	}

	return range->step_uV * voltage_sel + range->min_uV;
}

static int qpnp_regulator_single_range_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc, range_sel, voltage_sel;

	rc = qpnp_regulator_select_voltage(vreg, min_uV, max_uV, &range_sel,
		&voltage_sel, selector);
	if (rc) {
		vreg_err(vreg, "could not set voltage, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Certain types of regulators do not have a range select register so
	 * only voltage set register needs to be written.
	 */
	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_VOLTAGE_SET,
	       voltage_sel, 0xFF, &vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET]);

	if (rc)
		vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
	else
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int qpnp_regulator_single_range_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	struct qpnp_voltage_range *range = &vreg->set_points->range[0];
	int voltage_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET];

	return range->step_uV * voltage_sel + range->min_uV;
}

static int qpnp_regulator_ult_lo_smps_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc, range_sel, voltage_sel;

	/*
	 * Favor staying in the current voltage range if possible. This avoids
	 * voltage spikes that occur when changing the voltage range.
	 */
	rc = qpnp_regulator_select_voltage_same_range(vreg, min_uV, max_uV,
		&range_sel, &voltage_sel, selector);
	if (rc == 0)
		rc = qpnp_regulator_select_voltage(vreg, min_uV, max_uV,
			&range_sel, &voltage_sel, selector);
	if (rc < 0) {
		vreg_err(vreg, "could not set voltage, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Calculate VSET based on range
	 * In case of range 0: voltage_sel is a 7 bit value, can be written
	 *			witout any modification.
	 * In case of range 1: voltage_sel is a 5 bit value, bits[7-5] set to
	 *			[011].
	 */
	if (range_sel == 1)
		voltage_sel |= ULT_SMPS_RANGE_SPLIT;

	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_VOLTAGE_SET,
	       voltage_sel, 0xFF, &vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET]);
	if (rc) {
		vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
	} else {
		vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE] = range_sel;
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_VOLTAGE);
	}

	return rc;
}

static int qpnp_regulator_ult_lo_smps_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	struct qpnp_voltage_range *range = NULL;
	int range_sel, voltage_sel, i;

	range_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE];
	voltage_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET];

	for (i = 0; i < vreg->set_points->count; i++) {
		if (vreg->set_points->range[i].range_sel == range_sel) {
			range = &vreg->set_points->range[i];
			break;
		}
	}

	if (!range) {
		vreg_err(vreg, "voltage unknown, range %d is invalid\n",
			range_sel);
		return VOLTAGE_UNKNOWN;
	}

	if (range_sel == 1)
		voltage_sel &= ~ULT_SMPS_RANGE_SPLIT;

	return range->step_uV * voltage_sel + range->min_uV;
}

static int qpnp_regulator_common_list_voltage(struct regulator_dev *rdev,
			unsigned int selector)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int uV = 0;
	int i;

	if (selector >= vreg->set_points->n_voltages)
		return 0;

	for (i = 0; i < vreg->set_points->count; i++) {
		if (selector < vreg->set_points->range[i].n_voltages) {
			uV = selector * vreg->set_points->range[i].step_uV
				+ vreg->set_points->range[i].set_point_min_uV;
			break;
		}

		selector -= vreg->set_points->range[i].n_voltages;
	}

	return uV;
}

static int qpnp_regulator_common2_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned int *selector)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc, range_sel, voltage_sel, voltage_old = 0;
	int voltage_uV, voltage_mV;
	u8 buf[2];

	if (vreg->slew_rate && vreg->rdesc.ops->get_voltage) {
		voltage_old = vreg->rdesc.ops->get_voltage(rdev);
		if (voltage_old < 0) {
			vreg_err(vreg, "could not get current voltage, rc=%d\n",
				voltage_old);
			return voltage_old;
		}
	}

	rc = qpnp_regulator_select_voltage(vreg, min_uV, max_uV, &range_sel,
					   &voltage_sel, selector);
	if (rc < 0) {
		vreg_err(vreg, "could not set voltage, rc=%d\n", rc);
		return rc;
	}

	voltage_uV = qpnp_regulator_common_list_voltage(rdev, *selector);
	voltage_mV = voltage_uV / 1000;
	buf[0] = voltage_mV & 0xFF;
	buf[1] = (voltage_mV >> 8) & 0xFF;

	if (vreg->ctrl_reg[QPNP_COMMON2_IDX_VOLTAGE_LSB] != buf[0]
	    || vreg->ctrl_reg[QPNP_COMMON2_IDX_VOLTAGE_MSB] != buf[1]) {
		/* MSB must always be written even if it is unchanged. */
		rc = qpnp_vreg_write(vreg, QPNP_COMMON2_REG_VOLTAGE_LSB,
				     buf, 2);
		if (rc) {
			vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
			return rc;
		}

		vreg->ctrl_reg[QPNP_COMMON2_IDX_VOLTAGE_LSB] = buf[0];
		vreg->ctrl_reg[QPNP_COMMON2_IDX_VOLTAGE_MSB] = buf[1];

		rc = qpnp_regulator_delay_for_slewing(vreg, voltage_old);
		if (rc)
			return rc;

		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_VOLTAGE);
	}

	return rc;
}

static int qpnp_regulator_common2_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);

	return (((int)vreg->ctrl_reg[QPNP_COMMON2_IDX_VOLTAGE_MSB] << 8)
		| (int)vreg->ctrl_reg[QPNP_COMMON2_IDX_VOLTAGE_LSB]) * 1000;
}

static unsigned int qpnp_regulator_common_get_mode(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);

	return (vreg->ctrl_reg[QPNP_COMMON_IDX_MODE]
		& QPNP_COMMON_MODE_HPM_MASK)
			? REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
}

static int qpnp_regulator_common_set_mode(struct regulator_dev *rdev,
					unsigned int mode)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 val;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	val = (mode == REGULATOR_MODE_NORMAL ? QPNP_COMMON_MODE_HPM_MASK : 0);

	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_MODE, val,
		QPNP_COMMON_MODE_HPM_MASK,
		&vreg->ctrl_reg[QPNP_COMMON_IDX_MODE]);

	if (rc)
		vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
	else
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_MODE);

	return rc;
}

static unsigned int qpnp_regulator_common_get_optimum_mode(
		struct regulator_dev *rdev, int input_uV, int output_uV,
		int load_uA)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	unsigned int mode;

	if (load_uA + vreg->system_load >= vreg->hpm_min_load)
		mode = REGULATOR_MODE_NORMAL;
	else
		mode = REGULATOR_MODE_IDLE;

	return mode;
}

static unsigned int qpnp_regulator_common2_get_mode(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);

	return vreg->ctrl_reg[QPNP_COMMON2_IDX_MODE] == QPNP_COMMON2_MODE_HPM
		? REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
}

static int qpnp_regulator_common2_set_mode(struct regulator_dev *rdev,
					unsigned int mode)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 val = QPNP_COMMON2_MODE_HPM;

	if (mode != REGULATOR_MODE_NORMAL && mode != REGULATOR_MODE_IDLE) {
		vreg_err(vreg, "invalid mode: %u\n", mode);
		return -EINVAL;
	}

	/*
	 * Use init_mode as the low power mode unless it is equal to HPM.  This
	 * ensures that AUTO mode is re-asserted after switching away from
	 * forced HPM if it was configured initially.
	 */
	if (mode == REGULATOR_MODE_NORMAL)
		val = QPNP_COMMON2_MODE_HPM;
	else if (vreg->init_mode == QPNP_COMMON2_MODE_HPM)
		val = QPNP_COMMON2_MODE_LPM;
	else
		val = vreg->init_mode;

	rc = qpnp_vreg_write_optimized(vreg, QPNP_COMMON2_REG_MODE, &val,
				&vreg->ctrl_reg[QPNP_COMMON2_IDX_MODE], 1);
	if (rc)
		vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
	else
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_MODE);

	return rc;
}

static int qpnp_regulator_common_enable_time(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);

	return vreg->enable_time;
}

static int qpnp_regulator_vs_clear_ocp(struct qpnp_regulator *vreg)
{
	int rc;

	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_ENABLE,
		QPNP_COMMON_DISABLE, QPNP_COMMON_ENABLE_MASK,
		&vreg->ctrl_reg[QPNP_COMMON_IDX_ENABLE]);
	if (rc)
		vreg_err(vreg, "qpnp_vreg_masked_write failed, rc=%d\n", rc);

	vreg->vs_enable_time = ktime_get();

	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_ENABLE,
		QPNP_COMMON_ENABLE, QPNP_COMMON_ENABLE_MASK,
		&vreg->ctrl_reg[QPNP_COMMON_IDX_ENABLE]);
	if (rc)
		vreg_err(vreg, "qpnp_vreg_masked_write failed, rc=%d\n", rc);

	if (qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_OCP) {
		pr_info("%s: switch state toggled after OCP event\n",
			vreg->rdesc.name);
	}

	return rc;
}

static void qpnp_regulator_vs_ocp_work(struct work_struct *work)
{
	struct delayed_work *dwork
		= container_of(work, struct delayed_work, work);
	struct qpnp_regulator *vreg
		= container_of(dwork, struct qpnp_regulator, ocp_work);

	qpnp_regulator_vs_clear_ocp(vreg);
}

static irqreturn_t qpnp_regulator_vs_ocp_isr(int irq, void *data)
{
	struct qpnp_regulator *vreg = data;
	ktime_t ocp_irq_time;
	s64 ocp_trigger_delay_us;

	ocp_irq_time = ktime_get();
	ocp_trigger_delay_us = ktime_us_delta(ocp_irq_time,
						vreg->vs_enable_time);

	/*
	 * Reset the OCP count if there is a large delay between switch enable
	 * and when OCP triggers.  This is indicative of a hotplug event as
	 * opposed to a fault.
	 */
	if (ocp_trigger_delay_us > QPNP_VS_OCP_FAULT_DELAY_US)
		vreg->ocp_count = 0;

	/* Wait for switch output to settle back to 0 V after OCP triggered. */
	udelay(QPNP_VS_OCP_FALL_DELAY_US);

	vreg->ocp_count++;

	if (qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_OCP) {
		pr_info("%s: VS OCP triggered, count = %d, delay = %lld us\n",
			vreg->rdesc.name, vreg->ocp_count,
			ocp_trigger_delay_us);
	}

	if (vreg->ocp_count == 1) {
		/* Immediately clear the over current condition. */
		qpnp_regulator_vs_clear_ocp(vreg);
	} else if (vreg->ocp_count <= vreg->ocp_max_retries) {
		/* Schedule the over current clear task to run later. */
		schedule_delayed_work(&vreg->ocp_work,
			msecs_to_jiffies(vreg->ocp_retry_delay_ms) + 1);
	} else {
		vreg_err(vreg, "OCP triggered %d times; no further retries\n",
			vreg->ocp_count);
	}

	return IRQ_HANDLED;
}

static const char * const qpnp_print_actions[] = {
	[QPNP_REGULATOR_ACTION_INIT]	= "initial    ",
	[QPNP_REGULATOR_ACTION_ENABLE]	= "enable     ",
	[QPNP_REGULATOR_ACTION_DISABLE]	= "disable    ",
	[QPNP_REGULATOR_ACTION_VOLTAGE]	= "set voltage",
	[QPNP_REGULATOR_ACTION_MODE]	= "set mode   ",
};

static const char * const qpnp_common2_mode_label[] = {
	[0]				= "RSV",
	[1]				= "RSV",
	[2]				= "RSV",
	[QPNP_COMMON2_MODE_BYPASS]	= "BYP",
	[QPNP_COMMON2_MODE_RETENTION]	= "RET",
	[QPNP_COMMON2_MODE_LPM]		= "LPM",
	[QPNP_COMMON2_MODE_AUTO]	= "AUTO",
	[QPNP_COMMON2_MODE_HPM]		= "HPM",
};

static void qpnp_vreg_show_state(struct regulator_dev *rdev,
				   enum qpnp_regulator_action action)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	const char *action_label = qpnp_print_actions[action];
	unsigned int mode = 0;
	int uV = 0;
	const char *mode_label = "";
	enum qpnp_regulator_logical_type type;
	const char *enable_label = "";
	char pc_enable_label[5] = {'\0'};
	char pc_mode_label[8] = {'\0'};
	bool show_req, show_dupe, show_init, has_changed;
	u8 en_reg, mode_reg;

	/* Do not print unless appropriate flags are set. */
	show_req = qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_REQUEST;
	show_dupe = qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_DUPLICATE;
	show_init = qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_INIT;
	has_changed = vreg->write_count != vreg->prev_write_count;
	if (!((show_init && action == QPNP_REGULATOR_ACTION_INIT)
	      || (show_req && (has_changed || show_dupe)))) {
		return;
	}

	vreg->prev_write_count = vreg->write_count;

	type = vreg->logical_type;

	if (vreg->rdesc.ops->is_enabled)
		enable_label = vreg->rdesc.ops->is_enabled(rdev)
				? "on " : "off";

	if (vreg->rdesc.ops->get_voltage)
		uV = vreg->rdesc.ops->get_voltage(rdev);

	if (vreg->rdesc.ops->get_mode) {
		mode = vreg->rdesc.ops->get_mode(rdev);
		mode_label = mode == REGULATOR_MODE_NORMAL ? "HPM" : "LPM";
	}

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_VS) {
		en_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_ENABLE];
		pc_enable_label[0] =
		     en_reg & QPNP_COMMON_ENABLE_FOLLOW_HW_EN3_MASK ? '3' : '_';
		pc_enable_label[1] =
		     en_reg & QPNP_COMMON_ENABLE_FOLLOW_HW_EN2_MASK ? '2' : '_';
		pc_enable_label[2] =
		     en_reg & QPNP_COMMON_ENABLE_FOLLOW_HW_EN1_MASK ? '1' : '_';
		pc_enable_label[3] =
		     en_reg & QPNP_COMMON_ENABLE_FOLLOW_HW_EN0_MASK ? '0' : '_';
	}

	switch (type) {
	case QPNP_REGULATOR_LOGICAL_TYPE_SMPS:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_AUTO_MASK          ? 'A' : '_';
		pc_mode_label[1] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK  ? 'W' : '_';
		pc_mode_label[2] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN3_MASK ? '3' : '_';
		pc_mode_label[3] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN2_MASK ? '2' : '_';
		pc_mode_label[4] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN1_MASK ? '1' : '_';
		pc_mode_label[5] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN0_MASK ? '0' : '_';

		pr_info("%s %-11s: %s, v=%7d uV, mode=%s, pc_en=%s, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			mode_label, pc_enable_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_LDO:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_AUTO_MASK          ? 'A' : '_';
		pc_mode_label[1] =
		     mode_reg & QPNP_COMMON_MODE_BYPASS_MASK        ? 'B' : '_';
		pc_mode_label[2] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK  ? 'W' : '_';
		pc_mode_label[3] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN3_MASK ? '3' : '_';
		pc_mode_label[4] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN2_MASK ? '2' : '_';
		pc_mode_label[5] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN1_MASK ? '1' : '_';
		pc_mode_label[6] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_HW_EN0_MASK ? '0' : '_';

		pr_info("%s %-11s: %s, v=%7d uV, mode=%s, pc_en=%s, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			mode_label, pc_enable_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_LN_LDO:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_BYPASS_MASK ? 'B' : '_';

		pr_info("%s %-11s: %s, v=%7d uV, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_VS:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_AUTO_MASK          ? 'A' : '_';
		pc_mode_label[1] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK  ? 'W' : '_';

		pr_info("%s %-11s: %s, mode=%s, pc_en=%s, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label,
			mode_label, pc_enable_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_BOOST:
		pr_info("%s %-11s: %s, v=%7d uV\n",
			action_label, vreg->rdesc.name, enable_label, uV);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_BOOST_BYP:
		pr_info("%s %-11s: %s, v=%7d uV\n",
			action_label, vreg->rdesc.name, enable_label, uV);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_AUTO_MASK          ? 'A' : '_';

		pr_info("%s %-11s: %s, v=%7d uV, mode=%s, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			mode_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_ULT_LO_SMPS:
	case QPNP_REGULATOR_LOGICAL_TYPE_ULT_HO_SMPS:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK  ? 'W' : '_';
		pr_info("%s %-11s: %s, v=%7d uV, mode=%s, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			mode_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_ULT_LDO:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_BYPASS_MASK        ? 'B' : '_';
		pc_mode_label[1] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK  ? 'W' : '_';
		pr_info("%s %-11s: %s, v=%7d uV, mode=%s, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			mode_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS2:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		mode_label = qpnp_common2_mode_label[mode_reg
						     & QPNP_COMMON2_MODE_MASK];
		pr_info("%s %-11s: %s, v=%7d uV, mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			mode_label);
		break;
	default:
		break;
	}
}

static struct regulator_ops qpnp_smps_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_common_set_voltage,
	.get_voltage		= qpnp_regulator_common_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.set_mode		= qpnp_regulator_common_set_mode,
	.get_mode		= qpnp_regulator_common_get_mode,
	.get_optimum_mode	= qpnp_regulator_common_get_optimum_mode,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_ldo_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_common_set_voltage,
	.get_voltage		= qpnp_regulator_common_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.set_mode		= qpnp_regulator_common_set_mode,
	.get_mode		= qpnp_regulator_common_get_mode,
	.get_optimum_mode	= qpnp_regulator_common_get_optimum_mode,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_ln_ldo_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_common_set_voltage,
	.get_voltage		= qpnp_regulator_common_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_vs_ops = {
	.enable			= qpnp_regulator_vs_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_boost_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_single_range_set_voltage,
	.get_voltage		= qpnp_regulator_single_range_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_ftsmps_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_common_set_voltage,
	.get_voltage		= qpnp_regulator_common_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.set_mode		= qpnp_regulator_common_set_mode,
	.get_mode		= qpnp_regulator_common_get_mode,
	.get_optimum_mode	= qpnp_regulator_common_get_optimum_mode,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_ult_lo_smps_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_ult_lo_smps_set_voltage,
	.get_voltage		= qpnp_regulator_ult_lo_smps_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.set_mode		= qpnp_regulator_common_set_mode,
	.get_mode		= qpnp_regulator_common_get_mode,
	.get_optimum_mode	= qpnp_regulator_common_get_optimum_mode,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_ult_ho_smps_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_single_range_set_voltage,
	.get_voltage		= qpnp_regulator_single_range_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.set_mode		= qpnp_regulator_common_set_mode,
	.get_mode		= qpnp_regulator_common_get_mode,
	.get_optimum_mode	= qpnp_regulator_common_get_optimum_mode,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_ult_ldo_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_single_range_set_voltage,
	.get_voltage		= qpnp_regulator_single_range_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.set_mode		= qpnp_regulator_common_set_mode,
	.get_mode		= qpnp_regulator_common_get_mode,
	.get_optimum_mode	= qpnp_regulator_common_get_optimum_mode,
	.enable_time		= qpnp_regulator_common_enable_time,
};

static struct regulator_ops qpnp_ftsmps426_ops = {
	.enable			= qpnp_regulator_common_enable,
	.disable		= qpnp_regulator_common_disable,
	.is_enabled		= qpnp_regulator_common_is_enabled,
	.set_voltage		= qpnp_regulator_common2_set_voltage,
	.get_voltage		= qpnp_regulator_common2_get_voltage,
	.list_voltage		= qpnp_regulator_common_list_voltage,
	.set_mode		= qpnp_regulator_common2_set_mode,
	.get_mode		= qpnp_regulator_common2_get_mode,
	.get_optimum_mode	= qpnp_regulator_common_get_optimum_mode,
	.enable_time		= qpnp_regulator_common_enable_time,
};

/* Maximum possible digital major revision value */
#define INF 0xFF

static const struct qpnp_regulator_mapping supported_regulators[] = {
	/*           type subtype dig_min dig_max ltype ops setpoints hpm_min */
	QPNP_VREG_MAP(BUCK,  GP_CTL,   0, INF, SMPS,   smps,   smps,   100000),
	QPNP_VREG_MAP(LDO,   N300,     0, INF, LDO,    ldo,    nldo1,   10000),
	QPNP_VREG_MAP(LDO,   N600,     0,   0, LDO,    ldo,    nldo2,   10000),
	QPNP_VREG_MAP(LDO,   N1200,    0,   0, LDO,    ldo,    nldo2,   10000),
	QPNP_VREG_MAP(LDO,   N600,     1, INF, LDO,    ldo,    nldo3,   10000),
	QPNP_VREG_MAP(LDO,   N1200,    1, INF, LDO,    ldo,    nldo3,   10000),
	QPNP_VREG_MAP(LDO,   N600_ST,  0,   0, LDO,    ldo,    nldo2,   10000),
	QPNP_VREG_MAP(LDO,   N1200_ST, 0,   0, LDO,    ldo,    nldo2,   10000),
	QPNP_VREG_MAP(LDO,   N600_ST,  1, INF, LDO,    ldo,    nldo3,   10000),
	QPNP_VREG_MAP(LDO,   N1200_ST, 1, INF, LDO,    ldo,    nldo3,   10000),
	QPNP_VREG_MAP(LDO,   P50,      0, INF, LDO,    ldo,    pldo,     5000),
	QPNP_VREG_MAP(LDO,   P150,     0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,   P300,     0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,   P600,     0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,   P1200,    0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,   LN,       0, INF, LN_LDO, ln_ldo, ln_ldo,      0),
	QPNP_VREG_MAP(LDO,   LV_P50,   0, INF, LDO,    ldo,    pldo,     5000),
	QPNP_VREG_MAP(LDO,   LV_P150,  0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,   LV_P300,  0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,   LV_P600,  0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,   LV_P1200, 0, INF, LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(VS,    LV100,    0, INF, VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,    LV300,    0, INF, VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,    MV300,    0, INF, VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,    MV500,    0, INF, VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,    HDMI,     0, INF, VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,    OTG,      0, INF, VS,     vs,     none,        0),
	QPNP_VREG_MAP(BOOST, 5V_BOOST, 0, INF, BOOST,  boost,  boost,       0),
	QPNP_VREG_MAP(FTS,   FTS_CTL,  0, INF, FTSMPS, ftsmps, ftsmps, 100000),
	QPNP_VREG_MAP(FTS, FTS2p5_CTL, 0, INF, FTSMPS, ftsmps, ftsmps2p5,
								       100000),
	QPNP_VREG_MAP(BOOST_BYP, BB_2A, 0, INF, BOOST_BYP, boost, boost_byp, 0),
	QPNP_VREG_MAP(ULT_BUCK, ULT_HF_CTL1, 0, INF, ULT_LO_SMPS, ult_lo_smps,
							ult_lo_smps,   100000),
	QPNP_VREG_MAP(ULT_BUCK, ULT_HF_CTL2, 0, INF, ULT_LO_SMPS, ult_lo_smps,
							ult_lo_smps,   100000),
	QPNP_VREG_MAP(ULT_BUCK, ULT_HF_CTL3, 0, INF, ULT_LO_SMPS, ult_lo_smps,
							ult_lo_smps,   100000),
	QPNP_VREG_MAP(ULT_BUCK, ULT_HF_CTL4, 0, INF, ULT_HO_SMPS, ult_ho_smps,
							ult_ho_smps,   100000),
	QPNP_VREG_MAP(ULT_LDO, N300_ST, 0, INF, ULT_LDO, ult_ldo, ult_nldo,
									10000),
	QPNP_VREG_MAP(ULT_LDO, N600_ST, 0, INF, ULT_LDO, ult_ldo, ult_nldo,
									10000),
	QPNP_VREG_MAP(ULT_LDO, N1200_ST, 0, INF, ULT_LDO, ult_ldo, ult_nldo,
									10000),
	QPNP_VREG_MAP(ULT_LDO, LV_P150,  0, INF, ULT_LDO, ult_ldo, ult_pldo,
									10000),
	QPNP_VREG_MAP(ULT_LDO, LV_P300,  0, INF, ULT_LDO, ult_ldo, ult_pldo,
									10000),
	QPNP_VREG_MAP(ULT_LDO, P600,     0, INF, ULT_LDO, ult_ldo, ult_pldo,
									10000),
	QPNP_VREG_MAP(ULT_LDO, P150,     0, INF, ULT_LDO, ult_ldo, ult_pldo,
									10000),
	QPNP_VREG_MAP(ULT_LDO, P50,     0, INF, ULT_LDO, ult_ldo, ult_pldo,
									 5000),
	QPNP_VREG_MAP(FTS,     FTS426,  0, INF, FTSMPS2, ftsmps426, ftsmps426,
								       100000),
};

static int qpnp_regulator_match(struct qpnp_regulator *vreg)
{
	const struct qpnp_regulator_mapping *mapping;
	struct device_node *node = vreg->pdev->dev.of_node;
	int rc, i;
	u32 type_reg[2], dig_major_rev;
	u8 version[QPNP_COMMON_REG_SUBTYPE - QPNP_COMMON_REG_DIG_MAJOR_REV + 1];
	u8 type, subtype;

	rc = qpnp_vreg_read(vreg, QPNP_COMMON_REG_DIG_MAJOR_REV, version,
		ARRAY_SIZE(version));
	if (rc) {
		vreg_err(vreg, "could not read version registers, rc=%d\n", rc);
		return rc;
	}
	dig_major_rev	= version[QPNP_COMMON_REG_DIG_MAJOR_REV
					- QPNP_COMMON_REG_DIG_MAJOR_REV];
	type		= version[QPNP_COMMON_REG_TYPE
					- QPNP_COMMON_REG_DIG_MAJOR_REV];
	subtype		= version[QPNP_COMMON_REG_SUBTYPE
					- QPNP_COMMON_REG_DIG_MAJOR_REV];

	/*
	 * Override type and subtype register values if qcom,force-type is
	 * present in the device tree node.
	 */
	rc = of_property_read_u32_array(node, "qcom,force-type", type_reg, 2);
	if (!rc) {
		type = type_reg[0];
		subtype = type_reg[1];
	}

	rc = -ENODEV;
	for (i = 0; i < ARRAY_SIZE(supported_regulators); i++) {
		mapping = &supported_regulators[i];
		if (mapping->type == type && mapping->subtype == subtype
		    && mapping->revision_min <= dig_major_rev
		    && mapping->revision_max >= dig_major_rev) {
			vreg->logical_type	= mapping->logical_type;
			vreg->set_points	= mapping->set_points;
			vreg->hpm_min_load	= mapping->hpm_min_load;
			vreg->rdesc.ops		= mapping->ops;
			vreg->rdesc.n_voltages
				= mapping->set_points->n_voltages;
			rc = 0;
			break;
		}
	}

	if (rc)
		vreg_err(vreg, "unsupported regulator: type=0x%02X, subtype=0x%02X, dig major rev=0x%02X\n",
			type, subtype, dig_major_rev);

	return rc;
}

static int qpnp_regulator_check_constraints(struct qpnp_regulator *vreg,
				struct qpnp_regulator_platform_data *pdata)
{
	struct qpnp_voltage_range *range = NULL;
	int i, rc = 0, limit_min_uV, limit_max_uV, max_uV;
	u8 reg[2];

	limit_min_uV = 0;
	limit_max_uV = INT_MAX;

	if (vreg->logical_type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS) {
		max_uV = pdata->init_data.constraints.max_uV;
		/* Find the range which max_uV is inside of. */
		for (i = vreg->set_points->count - 1; i >= 0; i--) {
			range = &vreg->set_points->range[i];
			if (range->set_point_max_uV > 0
				&& max_uV >= range->set_point_min_uV
				&& max_uV <= range->set_point_max_uV)
				break;
		}

		if (i < 0 || range == NULL) {
			vreg_err(vreg, "max_uV doesn't fit in any voltage range\n");
			return -EINVAL;
		}

		rc = qpnp_vreg_read(vreg, QPNP_COMMON_REG_UL_LL_CTRL,
					&reg[0], 1);
		if (rc) {
			vreg_err(vreg, "UL_LL register read failed, rc=%d\n",
				rc);
			return rc;
		}

		if (reg[0] & QPNP_COMMON_UL_EN_MASK) {
			rc = qpnp_vreg_read(vreg,
					QPNP_COMMON_REG_VOLTAGE_ULS_VALID,
					&reg[1], 1);
			if (rc) {
				vreg_err(vreg, "ULS_VALID register read failed, rc=%d\n",
					rc);
				return rc;
			}

			limit_max_uV =  range->step_uV * reg[1] + range->min_uV;
		}

		if (reg[0] & QPNP_COMMON_LL_EN_MASK) {
			rc = qpnp_vreg_read(vreg,
					QPNP_COMMON_REG_VOLTAGE_LLS_VALID,
					&reg[1], 1);
			if (rc) {
				vreg_err(vreg, "LLS_VALID register read failed, rc=%d\n",
					rc);
				return rc;
			}

			limit_min_uV =  range->step_uV * reg[1] + range->min_uV;
		}
	} else if (vreg->logical_type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS2) {
		rc = qpnp_vreg_read(vreg, QPNP_COMMON2_REG_VOLTAGE_ULS_LSB,
					reg, 2);
		if (rc) {
			vreg_err(vreg, "ULS registers read failed, rc=%d\n",
				rc);
			return rc;
		}

		limit_max_uV = (((int)reg[1] << 8) | (int)reg[0]) * 1000;
	}

	if (pdata->init_data.constraints.min_uV < limit_min_uV
	    || pdata->init_data.constraints.max_uV >  limit_max_uV) {
		vreg_err(vreg, "regulator min/max(%d/%d) constraints do not fit within HW configured min/max(%d/%d) constraints\n",
			pdata->init_data.constraints.min_uV,
			pdata->init_data.constraints.max_uV,
			limit_min_uV, limit_max_uV);
		return -EINVAL;
	}

	return 0;
}

static int qpnp_regulator_ftsmps_init_slew_rate(struct qpnp_regulator *vreg)
{
	int rc;
	u8 reg = 0;
	int step = 0, delay, i, range_sel;
	struct qpnp_voltage_range *range = NULL;

	rc = qpnp_vreg_read(vreg, QPNP_COMMON_REG_STEP_CTRL, &reg, 1);
	if (rc) {
		vreg_err(vreg, "spmi read failed, rc=%d\n", rc);
		return rc;
	}

	range_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE];

	for (i = 0; i < vreg->set_points->count; i++) {
		if (vreg->set_points->range[i].range_sel == range_sel) {
			range = &vreg->set_points->range[i];
			break;
		}
	}

	if (!range) {
		vreg_err(vreg, "range %d is invalid\n", range_sel);
		return -EINVAL;
	}

	step = (reg & QPNP_FTSMPS_STEP_CTRL_STEP_MASK)
		>> QPNP_FTSMPS_STEP_CTRL_STEP_SHIFT;

	delay = (reg & QPNP_FTSMPS_STEP_CTRL_DELAY_MASK)
		>> QPNP_FTSMPS_STEP_CTRL_DELAY_SHIFT;

	/* slew_rate has units of uV/us. */
	vreg->slew_rate = QPNP_FTSMPS_CLOCK_RATE * range->step_uV * (1 << step);

	vreg->slew_rate /= 1000 * (QPNP_FTSMPS_STEP_DELAY << delay);

	vreg->slew_rate = vreg->slew_rate * QPNP_FTSMPS_STEP_MARGIN_NUM
				/ QPNP_FTSMPS_STEP_MARGIN_DEN;

	/* Ensure that the slew rate is greater than 0. */
	vreg->slew_rate = max(vreg->slew_rate, 1);

	return rc;
}

static int qpnp_regulator_ftsmps2_init_slew_rate(struct qpnp_regulator *vreg)
{
	struct qpnp_voltage_range *range = NULL;
	int i, rc, delay;
	u8 reg = 0;

	rc = qpnp_vreg_read(vreg, QPNP_COMMON2_REG_STEP_CTRL, &reg, 1);
	if (rc) {
		vreg_err(vreg, "spmi read failed, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Regulators using the common #2 register layout do not have a voltage
	 * range select register.  Choose the lowest possible step size to be
	 * conservative in the slew rate calculation.
	 */
	for (i = 0; i < vreg->set_points->count; i++) {
		if (!range || vreg->set_points->range[i].step_uV
				< range->step_uV)
			range = &vreg->set_points->range[i];
	}

	if (!range) {
		vreg_err(vreg, "range is invalid\n");
		return -EINVAL;
	}

	delay = (reg & QPNP_FTSMPS2_STEP_CTRL_DELAY_MASK)
		>> QPNP_FTSMPS2_STEP_CTRL_DELAY_SHIFT;

	/* slew_rate has units of uV/us. */
	vreg->slew_rate = QPNP_FTSMPS2_CLOCK_RATE * range->step_uV;
	vreg->slew_rate /= 1000 * (QPNP_FTSMPS2_STEP_DELAY << delay);
	vreg->slew_rate = vreg->slew_rate * QPNP_FTSMPS2_STEP_MARGIN_NUM
				/ QPNP_FTSMPS2_STEP_MARGIN_DEN;

	/* Ensure that the slew rate is greater than 0. */
	vreg->slew_rate = max(vreg->slew_rate, 1);

	return rc;
}

static int qpnp_regulator_init_registers(struct qpnp_regulator *vreg,
				struct qpnp_regulator_platform_data *pdata)
{
	int rc, i;
	enum qpnp_regulator_logical_type type;
	u8 ctrl_reg[8], reg, mask;

	type = vreg->logical_type;

	rc = qpnp_vreg_read(vreg, QPNP_COMMON_REG_VOLTAGE_RANGE,
			    vreg->ctrl_reg, 8);
	if (rc) {
		vreg_err(vreg, "spmi read failed, rc=%d\n", rc);
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(ctrl_reg); i++)
		ctrl_reg[i] = vreg->ctrl_reg[i];

	/* Set up enable pin control. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_VS)
	    && !(pdata->pin_ctrl_enable
			& QPNP_REGULATOR_PIN_CTRL_ENABLE_HW_DEFAULT)) {
		ctrl_reg[QPNP_COMMON_IDX_ENABLE] &=
			~QPNP_COMMON_ENABLE_FOLLOW_ALL_MASK;
		ctrl_reg[QPNP_COMMON_IDX_ENABLE] |=
		    pdata->pin_ctrl_enable & QPNP_COMMON_ENABLE_FOLLOW_ALL_MASK;
	}

	/* Set up HPM control. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LO_SMPS
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_HO_SMPS
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LDO
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_VS
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS)
	    && (pdata->hpm_enable != QPNP_REGULATOR_USE_HW_DEFAULT)) {
		ctrl_reg[QPNP_COMMON_IDX_MODE] &= ~QPNP_COMMON_MODE_HPM_MASK;
		ctrl_reg[QPNP_COMMON_IDX_MODE] |=
		     (pdata->hpm_enable ? QPNP_COMMON_MODE_HPM_MASK : 0);
	}

	/* Set up auto mode control. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_VS
	     || type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS)
	    && (pdata->auto_mode_enable != QPNP_REGULATOR_USE_HW_DEFAULT)) {
		ctrl_reg[QPNP_COMMON_IDX_MODE] &=
			~QPNP_COMMON_MODE_AUTO_MASK;
		ctrl_reg[QPNP_COMMON_IDX_MODE] |=
		     (pdata->auto_mode_enable ? QPNP_COMMON_MODE_AUTO_MASK : 0);
	}

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS2) {
		if (pdata->hpm_enable == QPNP_REGULATOR_ENABLE)
			ctrl_reg[QPNP_COMMON2_IDX_MODE]
				= QPNP_COMMON2_MODE_HPM;
		else if (pdata->auto_mode_enable == QPNP_REGULATOR_ENABLE)
			ctrl_reg[QPNP_COMMON2_IDX_MODE]
				= QPNP_COMMON2_MODE_AUTO;
		else if (pdata->hpm_enable == QPNP_REGULATOR_DISABLE
			 && ctrl_reg[QPNP_COMMON2_IDX_MODE]
					== QPNP_COMMON2_MODE_HPM)
			ctrl_reg[QPNP_COMMON2_IDX_MODE]
				= QPNP_COMMON2_MODE_LPM;
		else if (pdata->auto_mode_enable == QPNP_REGULATOR_DISABLE
			 && ctrl_reg[QPNP_COMMON2_IDX_MODE]
					== QPNP_COMMON2_MODE_AUTO)
			ctrl_reg[QPNP_COMMON2_IDX_MODE]
				= QPNP_COMMON2_MODE_LPM;
	}

	/* Set up mode pin control. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO)
		&& !(pdata->pin_ctrl_hpm
			& QPNP_REGULATOR_PIN_CTRL_HPM_HW_DEFAULT)) {
		ctrl_reg[QPNP_COMMON_IDX_MODE] &=
			~QPNP_COMMON_MODE_FOLLOW_ALL_MASK;
		ctrl_reg[QPNP_COMMON_IDX_MODE] |=
			pdata->pin_ctrl_hpm & QPNP_COMMON_MODE_FOLLOW_ALL_MASK;
	}

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_VS
	   && !(pdata->pin_ctrl_hpm & QPNP_REGULATOR_PIN_CTRL_HPM_HW_DEFAULT)) {
		ctrl_reg[QPNP_COMMON_IDX_MODE] &=
			~QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK;
		ctrl_reg[QPNP_COMMON_IDX_MODE] |=
		       pdata->pin_ctrl_hpm & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK;
	}

	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LO_SMPS
		|| type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_HO_SMPS
		|| type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LDO)
		&& !(pdata->pin_ctrl_hpm
			& QPNP_REGULATOR_PIN_CTRL_HPM_HW_DEFAULT)) {
		ctrl_reg[QPNP_COMMON_IDX_MODE] &=
			~QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK;
		ctrl_reg[QPNP_COMMON_IDX_MODE] |=
		       pdata->pin_ctrl_hpm & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK;
	}

	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_LN_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LDO)
	      && pdata->bypass_mode_enable != QPNP_REGULATOR_USE_HW_DEFAULT) {
		ctrl_reg[QPNP_COMMON_IDX_MODE] &=
			~QPNP_COMMON_MODE_BYPASS_MASK;
		ctrl_reg[QPNP_COMMON_IDX_MODE] |=
			(pdata->bypass_mode_enable
				? QPNP_COMMON_MODE_BYPASS_MASK : 0);
	}

	/* Set boost current limit. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_BOOST
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_BOOST_BYP)
		&& pdata->boost_current_limit
			!= QPNP_BOOST_CURRENT_LIMIT_HW_DEFAULT) {
		reg = pdata->boost_current_limit;
		mask = QPNP_BOOST_CURRENT_LIMIT_MASK;
		rc = qpnp_vreg_masked_read_write(vreg,
			(type == QPNP_REGULATOR_LOGICAL_TYPE_BOOST
				? QPNP_BOOST_REG_CURRENT_LIMIT
				: QPNP_BOOST_BYP_REG_CURRENT_LIMIT),
			reg, mask);
		if (rc) {
			vreg_err(vreg, "spmi write failed, rc=%d\n", rc);
			return rc;
		}
	}

	/* Write back any control register values that were modified. */
	rc = qpnp_vreg_write_optimized(vreg, QPNP_COMMON_REG_VOLTAGE_RANGE,
		ctrl_reg, vreg->ctrl_reg, 8);
	if (rc) {
		vreg_err(vreg, "spmi write failed, rc=%d\n", rc);
		return rc;
	}

	/* Setup initial range for ULT_LO_SMPS */
	if (type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LO_SMPS) {
		ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_RANGE] =
			(ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET]
			 < ULT_SMPS_RANGE_SPLIT) ? 0 : 1;
	}

	/* Set pull down. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LO_SMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_HO_SMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_VS)
	    && pdata->pull_down_enable != QPNP_REGULATOR_USE_HW_DEFAULT) {
		reg = pdata->pull_down_enable
			? QPNP_COMMON_PULL_DOWN_ENABLE_MASK : 0;
		rc = qpnp_vreg_write(vreg, QPNP_COMMON_REG_PULL_DOWN, &reg, 1);
		if (rc) {
			vreg_err(vreg, "spmi write failed, rc=%d\n", rc);
			return rc;
		}
	}

	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS2)
	    && pdata->pull_down_enable != QPNP_REGULATOR_USE_HW_DEFAULT) {
		/* FTSMPS has other bits in the pull down control register. */
		reg = pdata->pull_down_enable
			? QPNP_COMMON_PULL_DOWN_ENABLE_MASK : 0;
		rc = qpnp_vreg_masked_read_write(vreg,
			QPNP_COMMON_REG_PULL_DOWN, reg,
			QPNP_COMMON_PULL_DOWN_ENABLE_MASK);
		if (rc) {
			vreg_err(vreg, "spmi write failed, rc=%d\n", rc);
			return rc;
		}
	}

	/* Set soft start for LDO. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_ULT_LDO)
	    && pdata->soft_start_enable != QPNP_REGULATOR_USE_HW_DEFAULT) {
		reg = pdata->soft_start_enable
			? QPNP_LDO_SOFT_START_ENABLE_MASK : 0;
		rc = qpnp_vreg_write(vreg, QPNP_LDO_REG_SOFT_START, &reg, 1);
		if (rc) {
			vreg_err(vreg, "spmi write failed, rc=%d\n", rc);
			return rc;
		}
	}

	/* Set soft start strength and over current protection for VS. */
	if (type == QPNP_REGULATOR_LOGICAL_TYPE_VS) {
		reg = 0;
		mask = 0;
		if (pdata->soft_start_enable != QPNP_REGULATOR_USE_HW_DEFAULT) {
			reg |= pdata->soft_start_enable
				? QPNP_VS_SOFT_START_ENABLE_MASK : 0;
			mask |= QPNP_VS_SOFT_START_ENABLE_MASK;
		}
		if (pdata->vs_soft_start_strength
				!= QPNP_VS_SOFT_START_STR_HW_DEFAULT) {
			reg |= pdata->vs_soft_start_strength
				& QPNP_VS_SOFT_START_SEL_MASK;
			mask |= QPNP_VS_SOFT_START_SEL_MASK;
		}
		rc = qpnp_vreg_masked_read_write(vreg, QPNP_VS_REG_SOFT_START,
						 reg, mask);
		if (rc) {
			vreg_err(vreg, "spmi write failed, rc=%d\n", rc);
			return rc;
		}

		if (pdata->ocp_enable != QPNP_REGULATOR_USE_HW_DEFAULT) {
			reg = pdata->ocp_enable ? QPNP_VS_OCP_NO_OVERRIDE
						: QPNP_VS_OCP_OVERRIDE;
			rc = qpnp_vreg_write(vreg, QPNP_VS_REG_OCP, &reg, 1);
			if (rc) {
				vreg_err(vreg, "spmi write failed, rc=%d\n",
					rc);
				return rc;
			}
		}
	}

	/* Calculate the slew rate for FTSMPS regulators. */
	if (type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS) {
		rc = qpnp_regulator_ftsmps_init_slew_rate(vreg);
		if (rc) {
			vreg_err(vreg, "failed to initialize step rate, rc=%d\n",
				 rc);
			return rc;
		}
	}

	/* Calculate the slew rate for FTSMPS2 regulators. */
	if (type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS2) {
		rc = qpnp_regulator_ftsmps2_init_slew_rate(vreg);
		if (rc) {
			vreg_err(vreg, "failed to initialize step rate, rc=%d\n",
				 rc);
			return rc;
		}
	}

	vreg->init_mode = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];

	return rc;
}

/* Fill in pdata elements based on values found in device tree. */
static int qpnp_regulator_get_dt_config(struct platform_device *pdev,
				struct qpnp_regulator_platform_data *pdata)
{
	unsigned int base;
	struct device_node *node = pdev->dev.of_node;
	int rc = 0;

	pdata->init_data.constraints.input_uV
		= pdata->init_data.constraints.max_uV;

	rc = of_property_read_u32(pdev->dev.of_node, "reg", &base);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Couldn't find reg in node = %s rc = %d\n",
			pdev->dev.of_node->full_name, rc);
		return rc;
	}
	pdata->base_addr = base;

	/* OCP IRQ is optional so ignore get errors. */
	pdata->ocp_irq = platform_get_irq_byname(pdev, "ocp");
	if (pdata->ocp_irq < 0)
		pdata->ocp_irq = 0;

	/*
	 * Initialize configuration parameters to use hardware default in case
	 * no value is specified via device tree.
	 */
	pdata->auto_mode_enable		= QPNP_REGULATOR_USE_HW_DEFAULT;
	pdata->bypass_mode_enable	= QPNP_REGULATOR_USE_HW_DEFAULT;
	pdata->ocp_enable		= QPNP_REGULATOR_USE_HW_DEFAULT;
	pdata->pull_down_enable		= QPNP_REGULATOR_USE_HW_DEFAULT;
	pdata->soft_start_enable	= QPNP_REGULATOR_USE_HW_DEFAULT;
	pdata->boost_current_limit	= QPNP_BOOST_CURRENT_LIMIT_HW_DEFAULT;
	pdata->pin_ctrl_enable	    = QPNP_REGULATOR_PIN_CTRL_ENABLE_HW_DEFAULT;
	pdata->pin_ctrl_hpm	    = QPNP_REGULATOR_PIN_CTRL_HPM_HW_DEFAULT;
	pdata->vs_soft_start_strength	= QPNP_VS_SOFT_START_STR_HW_DEFAULT;
	pdata->hpm_enable		= QPNP_REGULATOR_USE_HW_DEFAULT;

	/* These bindings are optional, so it is okay if they are not found. */
	of_property_read_u32(node, "qcom,auto-mode-enable",
		&pdata->auto_mode_enable);
	of_property_read_u32(node, "qcom,bypass-mode-enable",
		&pdata->bypass_mode_enable);
	of_property_read_u32(node, "qcom,ocp-enable", &pdata->ocp_enable);
	of_property_read_u32(node, "qcom,ocp-max-retries",
		&pdata->ocp_max_retries);
	of_property_read_u32(node, "qcom,ocp-retry-delay",
		&pdata->ocp_retry_delay_ms);
	of_property_read_u32(node, "qcom,pull-down-enable",
		&pdata->pull_down_enable);
	of_property_read_u32(node, "qcom,soft-start-enable",
		&pdata->soft_start_enable);
	of_property_read_u32(node, "qcom,boost-current-limit",
		&pdata->boost_current_limit);
	of_property_read_u32(node, "qcom,pin-ctrl-enable",
		&pdata->pin_ctrl_enable);
	of_property_read_u32(node, "qcom,pin-ctrl-hpm", &pdata->pin_ctrl_hpm);
	of_property_read_u32(node, "qcom,hpm-enable", &pdata->hpm_enable);
	of_property_read_u32(node, "qcom,vs-soft-start-strength",
		&pdata->vs_soft_start_strength);
	of_property_read_u32(node, "qcom,system-load", &pdata->system_load);
	of_property_read_u32(node, "qcom,enable-time", &pdata->enable_time);

	return rc;
}

static const struct of_device_id spmi_match_table[];

#define MAX_NAME_LEN	127

static int qpnp_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config reg_config = {};
	struct qpnp_regulator_platform_data *pdata;
	struct qpnp_regulator *vreg;
	struct regulator_desc *rdesc;
	struct qpnp_regulator_platform_data of_pdata;
	struct regulator_init_data *init_data;
	char *reg_name;
	int rc;
	bool is_dt;

	vreg = kzalloc(sizeof(struct qpnp_regulator), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	vreg->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!vreg->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	is_dt = of_match_device(spmi_match_table, &pdev->dev);

	/* Check if device tree is in use. */
	if (is_dt) {
		init_data = of_get_regulator_init_data(&pdev->dev,
						       pdev->dev.of_node,
						       &vreg->rdesc);
		if (!init_data) {
			dev_err(&pdev->dev, "%s: unable to allocate memory\n",
					__func__);
			kfree(vreg);
			return -ENOMEM;
		}
		memset(&of_pdata, 0,
			sizeof(struct qpnp_regulator_platform_data));
		memcpy(&of_pdata.init_data, init_data,
			sizeof(struct regulator_init_data));

		if (of_get_property(pdev->dev.of_node, "parent-supply", NULL))
			of_pdata.init_data.supply_regulator = "parent";

		rc = qpnp_regulator_get_dt_config(pdev, &of_pdata);
		if (rc) {
			dev_err(&pdev->dev, "%s: DT parsing failed, rc=%d\n",
					__func__, rc);
			kfree(vreg);
			return -ENOMEM;
		}

		pdata = &of_pdata;
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (pdata == NULL) {
		dev_err(&pdev->dev, "%s: no platform data specified\n",
			__func__);
		kfree(vreg);
		return -EINVAL;
	}

	vreg->pdev		= pdev;
	vreg->prev_write_count	= -1;
	vreg->write_count	= 0;
	vreg->base_addr		= pdata->base_addr;
	vreg->enable_time	= pdata->enable_time;
	vreg->system_load	= pdata->system_load;
	vreg->ocp_enable	= pdata->ocp_enable;
	vreg->ocp_irq		= pdata->ocp_irq;
	vreg->ocp_max_retries	= pdata->ocp_max_retries;
	vreg->ocp_retry_delay_ms = pdata->ocp_retry_delay_ms;

	if (vreg->ocp_max_retries == 0)
		vreg->ocp_max_retries = QPNP_VS_OCP_DEFAULT_MAX_RETRIES;
	if (vreg->ocp_retry_delay_ms == 0)
		vreg->ocp_retry_delay_ms = QPNP_VS_OCP_DEFAULT_RETRY_DELAY_MS;

	rdesc			= &vreg->rdesc;
	rdesc->id		= to_spmi_device(pdev->dev.parent)->ctrl->nr;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;

	reg_name = kzalloc(strnlen(pdata->init_data.constraints.name,
				MAX_NAME_LEN) + 1, GFP_KERNEL);
	if (!reg_name) {
		kfree(vreg);
		return -ENOMEM;
	}
	strlcpy(reg_name, pdata->init_data.constraints.name,
		strnlen(pdata->init_data.constraints.name, MAX_NAME_LEN) + 1);
	rdesc->name = reg_name;

	dev_set_drvdata(&pdev->dev, vreg);

	rc = qpnp_regulator_match(vreg);
	if (rc)
		goto bail;

	if (is_dt && rdesc->ops) {
		/* Fill in ops and mode masks when using device tree. */
		if (rdesc->ops->enable)
			pdata->init_data.constraints.valid_ops_mask
				|= REGULATOR_CHANGE_STATUS;
		if (rdesc->ops->get_voltage)
			pdata->init_data.constraints.valid_ops_mask
				|= REGULATOR_CHANGE_VOLTAGE;
		if (rdesc->ops->get_mode) {
			pdata->init_data.constraints.valid_ops_mask
				|= REGULATOR_CHANGE_MODE
				| REGULATOR_CHANGE_DRMS;
			pdata->init_data.constraints.valid_modes_mask
				= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE;
		}
	}

	rc = qpnp_regulator_check_constraints(vreg, pdata);
	if (rc) {
		vreg_err(vreg, "regulator constraints check failed, rc=%d\n",
			rc);
		goto bail;
	}

	rc = qpnp_regulator_init_registers(vreg, pdata);
	if (rc) {
		vreg_err(vreg, "common initialization failed, rc=%d\n", rc);
		goto bail;
	}

	if (vreg->logical_type != QPNP_REGULATOR_LOGICAL_TYPE_VS)
		vreg->ocp_irq = 0;

	if (vreg->ocp_irq) {
		rc = devm_request_irq(&pdev->dev, vreg->ocp_irq,
			qpnp_regulator_vs_ocp_isr, IRQF_TRIGGER_RISING, "ocp",
			vreg);
		if (rc < 0) {
			vreg_err(vreg, "failed to request irq %d, rc=%d\n",
				vreg->ocp_irq, rc);
			goto bail;
		}

		INIT_DELAYED_WORK(&vreg->ocp_work, qpnp_regulator_vs_ocp_work);
	}

	reg_config.dev = &pdev->dev;
	reg_config.init_data = &pdata->init_data;
	reg_config.driver_data = vreg;
	reg_config.of_node = pdev->dev.of_node;
	vreg->rdev = regulator_register(rdesc, &reg_config);
	if (IS_ERR(vreg->rdev)) {
		rc = PTR_ERR(vreg->rdev);
		if (rc != -EPROBE_DEFER)
			vreg_err(vreg, "regulator_register failed, rc=%d\n",
				rc);
		goto cancel_ocp_work;
	}

	if (qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_INIT && vreg->slew_rate)
		pr_info("%-11s: step rate=%d uV/us\n", vreg->rdesc.name,
			vreg->slew_rate);

	qpnp_vreg_show_state(vreg->rdev, QPNP_REGULATOR_ACTION_INIT);

	return 0;

cancel_ocp_work:
	if (vreg->ocp_irq)
		cancel_delayed_work_sync(&vreg->ocp_work);
bail:
	if (rc && rc != -EPROBE_DEFER)
		vreg_err(vreg, "probe failed, rc=%d\n", rc);

	kfree(vreg->rdesc.name);
	kfree(vreg);

	return rc;
}

static int qpnp_regulator_remove(struct platform_device *pdev)
{
	struct qpnp_regulator *vreg;

	vreg = dev_get_drvdata(&pdev->dev);
	dev_set_drvdata(&pdev->dev, NULL);

	if (vreg) {
		regulator_unregister(vreg->rdev);
		if (vreg->ocp_irq)
			cancel_delayed_work_sync(&vreg->ocp_work);
		kfree(vreg->rdesc.name);
		kfree(vreg);
	}

	return 0;
}

static const struct of_device_id spmi_match_table[] = {
	{ .compatible = QPNP_REGULATOR_DRIVER_NAME, },
	{}
};

static const struct platform_device_id qpnp_regulator_id[] = {
	{ QPNP_REGULATOR_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, qpnp_regulator_id);

static struct platform_driver qpnp_regulator_driver = {
	.driver		= {
		.name		= QPNP_REGULATOR_DRIVER_NAME,
		.of_match_table	= spmi_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= qpnp_regulator_probe,
	.remove		= qpnp_regulator_remove,
	.id_table	= qpnp_regulator_id,
};

/*
 * Pre-compute the number of set points available for each regulator type to
 * avoid unnecessary calculations later in runtime.
 */
static void qpnp_regulator_set_point_init(void)
{
	struct qpnp_voltage_set_points **set_points;
	int i, j, temp;

	set_points = all_set_points;

	for (i = 0; i < ARRAY_SIZE(all_set_points); i++) {
		temp = 0;
		for (j = 0; j < all_set_points[i]->count; j++) {
			all_set_points[i]->range[j].n_voltages
				= (all_set_points[i]->range[j].set_point_max_uV
				 - all_set_points[i]->range[j].set_point_min_uV)
				   / all_set_points[i]->range[j].step_uV + 1;
			if (all_set_points[i]->range[j].set_point_max_uV == 0)
				all_set_points[i]->range[j].n_voltages = 0;
			temp += all_set_points[i]->range[j].n_voltages;
		}
		all_set_points[i]->n_voltages = temp;
	}
}

/**
 * qpnp_regulator_init() - register spmi driver for qpnp-regulator
 *
 * This initialization function should be called in systems in which driver
 * registration ordering must be controlled precisely.
 */
int __init qpnp_regulator_init(void)
{
	static bool has_registered;

	if (has_registered)
		return 0;
	has_registered = true;

	qpnp_regulator_set_point_init();

	return platform_driver_register(&qpnp_regulator_driver);
}
EXPORT_SYMBOL(qpnp_regulator_init);

static void __exit qpnp_regulator_exit(void)
{
	platform_driver_unregister(&qpnp_regulator_driver);
}

MODULE_DESCRIPTION("QPNP PMIC regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(qpnp_regulator_init);
module_exit(qpnp_regulator_exit);
