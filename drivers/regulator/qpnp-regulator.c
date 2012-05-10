/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/qpnp-regulator.h>

#include <mach/qpnp.h>

/* Debug Flag Definitions */
enum {
	QPNP_VREG_DEBUG_REQUEST		= BIT(0), /* Show requests */
	QPNP_VREG_DEBUG_DUPLICATE	= BIT(1), /* Show duplicate requests */
	QPNP_VREG_DEBUG_INIT		= BIT(2), /* Show state after probe */
	QPNP_VREG_DEBUG_WRITES		= BIT(3), /* Show SPMI writes */
	QPNP_VREG_DEBUG_READS		= BIT(4), /* Show SPMI reads */
};

static int qpnp_vreg_debug_mask;
module_param_named(
	debug_mask, qpnp_vreg_debug_mask, int, S_IRUSR | S_IWUSR
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
};

enum qpnp_regulator_type {
	QPNP_REGULATOR_TYPE_HF_BUCK		= 0x03,
	QPNP_REGULATOR_TYPE_LDO			= 0x04,
	QPNP_REGULATOR_TYPE_VS			= 0x05,
	QPNP_REGULATOR_TYPE_BOOST		= 0x1B,
	QPNP_REGULATOR_TYPE_FTS			= 0x1C,
};

enum qpnp_regulator_subtype {
	QPNP_REGULATOR_SUBTYPE_GP_CTL		= 0x08,
	QPNP_REGULATOR_SUBTYPE_RF_CTL		= 0x09,
	QPNP_REGULATOR_SUBTYPE_N50		= 0x01,
	QPNP_REGULATOR_SUBTYPE_N150		= 0x02,
	QPNP_REGULATOR_SUBTYPE_N300		= 0x03,
	QPNP_REGULATOR_SUBTYPE_N600		= 0x04,
	QPNP_REGULATOR_SUBTYPE_N1200		= 0x05,
	QPNP_REGULATOR_SUBTYPE_P50		= 0x08,
	QPNP_REGULATOR_SUBTYPE_P150		= 0x09,
	QPNP_REGULATOR_SUBTYPE_P300		= 0x0A,
	QPNP_REGULATOR_SUBTYPE_P600		= 0x0B,
	QPNP_REGULATOR_SUBTYPE_P1200		= 0x0C,
	QPNP_REGULATOR_SUBTYPE_LV100		= 0x01,
	QPNP_REGULATOR_SUBTYPE_LV300		= 0x02,
	QPNP_REGULATOR_SUBTYPE_MV300		= 0x08,
	QPNP_REGULATOR_SUBTYPE_MV500		= 0x09,
	QPNP_REGULATOR_SUBTYPE_HDMI		= 0x10,
	QPNP_REGULATOR_SUBTYPE_OTG		= 0x11,
	QPNP_REGULATOR_SUBTYPE_5V_BOOST		= 0x01,
	QPNP_REGULATOR_SUBTYPE_FTS_CTL		= 0x08,
};

enum qpnp_common_regulator_registers {
	QPNP_COMMON_REG_TYPE			= 0x04,
	QPNP_COMMON_REG_SUBTYPE			= 0x05,
	QPNP_COMMON_REG_VOLTAGE_RANGE		= 0x40,
	QPNP_COMMON_REG_VOLTAGE_SET		= 0x41,
	QPNP_COMMON_REG_MODE			= 0x45,
	QPNP_COMMON_REG_ENABLE			= 0x46,
	QPNP_COMMON_REG_PULL_DOWN		= 0x48,
};

enum qpnp_ldo_registers {
	QPNP_LDO_REG_SOFT_START			= 0x4C,
};

enum qpnp_vs_registers {
	QPNP_VS_REG_OCP				= 0x4A,
	QPNP_VS_REG_SOFT_START			= 0x4C,
};

enum qpnp_boost_registers {
	QPNP_BOOST_REG_CURRENT_LIMIT		= 0x40,
};

/* Used for indexing into ctrl_reg.  These are offets from 0x40 */
enum qpnp_common_control_register_index {
	QPNP_COMMON_IDX_VOLTAGE_RANGE		= 0,
	QPNP_COMMON_IDX_VOLTAGE_SET		= 1,
	QPNP_COMMON_IDX_MODE			= 5,
	QPNP_COMMON_IDX_ENABLE			= 6,
};

enum qpnp_boost_control_register_index {
	QPNP_BOOST_IDX_CURRENT_LIMIT		= 0,
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

/* Common regulator mode register layout */
#define QPNP_COMMON_MODE_HPM_MASK		0x80
#define QPNP_COMMON_MODE_AUTO_MASK		0x40
#define QPNP_COMMON_MODE_BYPASS_MASK		0x20
#define QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK	0x10
#define QPNP_COMMON_MODE_FOLLOW_HW_EN3_MASK	0x08
#define QPNP_COMMON_MODE_FOLLOW_HW_EN2_MASK	0x04
#define QPNP_COMMON_MODE_FOLLOW_HW_EN1_MASK	0x02
#define QPNP_COMMON_MODE_FOLLOW_HW_EN0_MASK	0x01
#define QPNP_COMMON_MODE_FOLLOW_ALL_MASK	0x1F

/* Common regulator pull down control register layout */
#define QPNP_COMMON_PULL_DOWN_ENABLE_MASK	0x80

/* LDO regulator current limit control register layout */
#define QPNP_LDO_CURRENT_LIMIT_ENABLE_MASK	0x80

/* LDO regulator soft start control register layout */
#define QPNP_LDO_SOFT_START_ENABLE_MASK		0x80

/* VS regulator over current protection control register layout */
#define QPNP_VS_OCP_ENABLE_MASK			0x80
#define QPNP_VS_OCP_OVERRIDE_MASK		0x01
#define QPNP_VS_OCP_DISABLE			0x00

/* VS regulator soft start control register layout */
#define QPNP_VS_SOFT_START_ENABLE_MASK		0x80
#define QPNP_VS_SOFT_START_SEL_MASK		0x03

/* Boost regulator current limit control register layout */
#define QPNP_BOOST_CURRENT_LIMIT_ENABLE_MASK	0x80
#define QPNP_BOOST_CURRENT_LIMIT_MASK		0x07

/*
 * This voltage in uV is returned by get_voltage functions when there is no way
 * to determine the current voltage level.  It is needed because the regulator
 * framework treats a 0 uV voltage as an error.
 */
#define VOLTAGE_UNKNOWN 1

struct qpnp_voltage_range {
	int					min_uV;
	int					max_uV;
	int					step_uV;
	int					set_point_min_uV;
	unsigned				n_voltages;
	u8					range_sel;
};

struct qpnp_voltage_set_points {
	struct qpnp_voltage_range		*range;
	int					count;
	unsigned				n_voltages;
};

struct qpnp_regulator_mapping {
	enum qpnp_regulator_type		type;
	enum qpnp_regulator_subtype		subtype;
	enum qpnp_regulator_logical_type	logical_type;
	struct regulator_ops			*ops;
	struct qpnp_voltage_set_points		*set_points;
	int					hpm_min_load;
};

struct qpnp_regulator {
	struct regulator_desc			rdesc;
	struct spmi_device			*spmi_dev;
	struct regulator_dev			*rdev;
	struct qpnp_voltage_set_points		*set_points;
	enum qpnp_regulator_logical_type	logical_type;
	int					enable_time;
	int					ocp_enable_time;
	int					ocp_enable;
	int					system_load;
	int					hpm_min_load;
	u32					write_count;
	u32					prev_write_count;
	u16					base_addr;
	/* ctrl_reg provides a shadow copy of register values 0x40 to 0x47. */
	u8					ctrl_reg[8];
};

#define QPNP_VREG_MAP(_type, _subtype, _logical_type, _ops_val, \
		      _set_points_val, _hpm_min_load) \
	{ \
		.type		= QPNP_REGULATOR_TYPE_##_type, \
		.subtype	= QPNP_REGULATOR_SUBTYPE_##_subtype, \
		.logical_type	= QPNP_REGULATOR_LOGICAL_TYPE_##_logical_type, \
		.ops		= &qpnp_##_ops_val##_ops, \
		.set_points	= &_set_points_val##_set_points, \
		.hpm_min_load	= _hpm_min_load, \
	}

#define VOLTAGE_RANGE(_range_sel, _min_uV, _set_point_min_uV, _max_uV, \
			_step_uV) \
	{ \
		.min_uV			= _min_uV, \
		.set_point_min_uV	= _set_point_min_uV, \
		.max_uV			= _max_uV, \
		.step_uV		= _step_uV, \
		.range_sel		= _range_sel, \
	}

#define SET_POINTS(_ranges) \
{ \
	.range	= _ranges, \
	.count	= ARRAY_SIZE(_ranges), \
};

/*
 * These tables contain the physically available PMIC regulator voltage setpoint
 * ranges.  Where two ranges overlap in hardware, one of the ranges is trimmed
 * to ensure that the setpoints available to software are monotonically
 * increasing and unique.  The set_voltage callback functions expect these
 * properties to hold.
 */
static struct qpnp_voltage_range pldo_ranges[] = {
	VOLTAGE_RANGE(2,  750000,  750000, 1537500, 12500),
	VOLTAGE_RANGE(3, 1500000, 1550000, 3075000, 25000),
	VOLTAGE_RANGE(4, 1750000, 3100000, 4900000, 50000),
};

static struct qpnp_voltage_range nldo1_ranges[] = {
	VOLTAGE_RANGE(2,  750000,  750000, 1537500, 12500),
};

static struct qpnp_voltage_range nldo2_ranges[] = {
	VOLTAGE_RANGE(1,  375000,  375000,  768750,  6250),
	VOLTAGE_RANGE(2,  750000,  775000, 1537500, 12500),
};

static struct qpnp_voltage_range smps_ranges[] = {
	VOLTAGE_RANGE(0,  375000,  375000, 1562500, 12500),
	VOLTAGE_RANGE(1, 1550000, 1575000, 3125000, 25000),
};

static struct qpnp_voltage_range ftsmps_ranges[] = {
	VOLTAGE_RANGE(0,   80000,  350000, 1355000,  5000),
	VOLTAGE_RANGE(1,  160000, 1360000, 2710000, 10000),
};

static struct qpnp_voltage_range boost_ranges[] = {
	VOLTAGE_RANGE(0, 4000000, 4000000, 5550000, 50000),
};

static struct qpnp_voltage_set_points pldo_set_points = SET_POINTS(pldo_ranges);
static struct qpnp_voltage_set_points nldo1_set_points
					= SET_POINTS(nldo1_ranges);
static struct qpnp_voltage_set_points nldo2_set_points
					= SET_POINTS(nldo2_ranges);
static struct qpnp_voltage_set_points smps_set_points = SET_POINTS(smps_ranges);
static struct qpnp_voltage_set_points ftsmps_set_points
					= SET_POINTS(ftsmps_ranges);
static struct qpnp_voltage_set_points boost_set_points
					= SET_POINTS(boost_ranges);
static struct qpnp_voltage_set_points none_set_points;

static struct qpnp_voltage_set_points *all_set_points[] = {
	&pldo_set_points,
	&nldo1_set_points,
	&nldo2_set_points,
	&smps_set_points,
	&ftsmps_set_points,
	&boost_set_points,
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

	rc = spmi_ext_register_readl(vreg->spmi_dev->ctrl, vreg->spmi_dev->sid,
		vreg->base_addr + addr, buf, len);

	if (!rc && (qpnp_vreg_debug_mask & QPNP_VREG_DEBUG_READS)) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, buf, len);
		pr_info(" %-11s:  read(0x%04X), sid=%d, len=%d; %s\n",
			vreg->rdesc.name, vreg->base_addr + addr,
			vreg->spmi_dev->sid, len, str);
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
			vreg->spmi_dev->sid, len, str);
	}

	rc = spmi_ext_register_writel(vreg->spmi_dev->ctrl,
		vreg->spmi_dev->sid, vreg->base_addr + addr, buf, len);
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
	int rc;
	u8 reg;

	if (vreg->ocp_enable == QPNP_REGULATOR_ENABLE) {
		/* Disable OCP */
		reg = QPNP_VS_OCP_DISABLE;
		rc = qpnp_vreg_write(vreg, QPNP_VS_REG_OCP, &reg, 1);
		if (rc)
			goto fail;
	}

	rc = qpnp_regulator_common_enable(rdev);
	if (rc)
		goto fail;

	if (vreg->ocp_enable == QPNP_REGULATOR_ENABLE) {
		/* Wait for inrush current to subsided, then enable OCP. */
		udelay(vreg->ocp_enable_time);
		reg = QPNP_VS_OCP_ENABLE_MASK;
		rc = qpnp_vreg_write(vreg, QPNP_VS_REG_OCP, &reg, 1);
		if (rc)
			goto fail;
	}

	return rc;
fail:
	vreg_err(vreg, "qpnp_vreg_write failed, rc=%d\n", rc);

	return rc;
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

static int qpnp_regulator_select_voltage(struct qpnp_regulator *vreg,
		int min_uV, int max_uV, int *range_sel, int *voltage_sel)
{
	struct qpnp_voltage_range *range;
	int uV = min_uV;
	int lim_min_uV, lim_max_uV, i;

	/* Check if request voltage is outside of physically settable range. */
	lim_min_uV = vreg->set_points->range[0].set_point_min_uV;
	lim_max_uV =
		vreg->set_points->range[vreg->set_points->count - 1].max_uV;

	if (uV < lim_min_uV && max_uV >= lim_min_uV)
		uV = lim_min_uV;

	if (uV < lim_min_uV || uV > lim_max_uV) {
		vreg_err(vreg,
			"request v=[%d, %d] is outside possible v=[%d, %d]\n",
			 min_uV, max_uV, lim_min_uV, lim_max_uV);
		return -EINVAL;
	}

	/* Find the range which uV is inside of. */
	for (i = vreg->set_points->count - 1; i > 0; i--)
		if (uV > vreg->set_points->range[i - 1].max_uV)
			break;
	range = &vreg->set_points->range[i];
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

	return 0;
}

static int qpnp_regulator_common_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc, range_sel, voltage_sel;
	u8 buf[2];

	rc = qpnp_regulator_select_voltage(vreg, min_uV, max_uV, &range_sel,
		&voltage_sel);
	if (rc) {
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

	if (rc)
		vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
	else
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_VOLTAGE);

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

static int qpnp_regulator_boost_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int rc, range_sel, voltage_sel;

	rc = qpnp_regulator_select_voltage(vreg, min_uV, max_uV, &range_sel,
		&voltage_sel);
	if (rc) {
		vreg_err(vreg, "could not set voltage, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Boost type regulators do not have range select register so only
	 * voltage set register needs to be written.
	 */
	rc = qpnp_vreg_masked_write(vreg, QPNP_COMMON_REG_VOLTAGE_SET,
	       voltage_sel, 0xFF, &vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET]);

	if (rc)
		vreg_err(vreg, "SPMI write failed, rc=%d\n", rc);
	else
		qpnp_vreg_show_state(rdev, QPNP_REGULATOR_ACTION_VOLTAGE);

	return rc;
}

static int qpnp_regulator_boost_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);
	int voltage_sel = vreg->ctrl_reg[QPNP_COMMON_IDX_VOLTAGE_SET];

	return boost_ranges[0].step_uV * voltage_sel + boost_ranges[0].min_uV;
}

static int qpnp_regulator_common_list_voltage(struct regulator_dev *rdev,
			unsigned selector)
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
		} else {
			selector -= vreg->set_points->range[i].n_voltages;
		}
	}

	return uV;
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

static int qpnp_regulator_common_enable_time(struct regulator_dev *rdev)
{
	struct qpnp_regulator *vreg = rdev_get_drvdata(rdev);

	return vreg->enable_time;
}

static const char const *qpnp_print_actions[] = {
	[QPNP_REGULATOR_ACTION_INIT]	= "initial    ",
	[QPNP_REGULATOR_ACTION_ENABLE]	= "enable     ",
	[QPNP_REGULATOR_ACTION_DISABLE]	= "disable    ",
	[QPNP_REGULATOR_ACTION_VOLTAGE]	= "set voltage",
	[QPNP_REGULATOR_ACTION_MODE]	= "set mode   ",
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
	const char *enable_label;
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

	enable_label = qpnp_regulator_common_is_enabled(rdev) ? "on " : "off";

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS)
		uV = qpnp_regulator_common_get_voltage(rdev);

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_BOOST)
		uV = qpnp_regulator_boost_get_voltage(rdev);

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	    || type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS) {
		mode = qpnp_regulator_common_get_mode(rdev);
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

		pr_info("%s %-11s: %s, v=%7d uV, mode=%s, pc_en=%s, "
			"alt_mode=%s\n",
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

		pr_info("%s %-11s: %s, v=%7d uV, mode=%s, pc_en=%s, "
			"alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label, uV,
			mode_label, pc_enable_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_VS:
		mode_reg = vreg->ctrl_reg[QPNP_COMMON_IDX_MODE];
		pc_mode_label[0] =
		     mode_reg & QPNP_COMMON_MODE_AUTO_MASK          ? 'A' : '_';
		pc_mode_label[1] =
		     mode_reg & QPNP_COMMON_MODE_FOLLOW_AWAKE_MASK  ? 'W' : '_';

		pr_info("%s %-11s: %s, pc_en=%s, alt_mode=%s\n",
			action_label, vreg->rdesc.name, enable_label,
			pc_enable_label, pc_mode_label);
		break;
	case QPNP_REGULATOR_LOGICAL_TYPE_BOOST:
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
	.set_voltage		= qpnp_regulator_boost_set_voltage,
	.get_voltage		= qpnp_regulator_boost_get_voltage,
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

static const struct qpnp_regulator_mapping supported_regulators[] = {
	QPNP_VREG_MAP(HF_BUCK,  GP_CTL,    SMPS,   smps,   smps,   100000),
	QPNP_VREG_MAP(LDO,      N300,      LDO,    ldo,    nldo1,   10000),
	QPNP_VREG_MAP(LDO,      N600,      LDO,    ldo,    nldo2,   10000),
	QPNP_VREG_MAP(LDO,      N1200,     LDO,    ldo,    nldo2,   10000),
	QPNP_VREG_MAP(LDO,      P50,       LDO,    ldo,    pldo,     5000),
	QPNP_VREG_MAP(LDO,      P150,      LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,      P300,      LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,      P600,      LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(LDO,      P1200,     LDO,    ldo,    pldo,    10000),
	QPNP_VREG_MAP(VS,       LV100,     VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,       LV300,     VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,       MV300,     VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,       MV500,     VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,       HDMI,      VS,     vs,     none,        0),
	QPNP_VREG_MAP(VS,       OTG,       VS,     vs,     none,        0),
	QPNP_VREG_MAP(BOOST,    5V_BOOST,  BOOST,  boost,  boost,       0),
	QPNP_VREG_MAP(FTS,      FTS_CTL,   FTSMPS, ftsmps, ftsmps, 100000),
};

static int qpnp_regulator_match(struct qpnp_regulator *vreg)
{
	const struct qpnp_regulator_mapping *mapping;
	int rc, i;
	u8 raw_type[2], type, subtype;

	rc = qpnp_vreg_read(vreg, QPNP_COMMON_REG_TYPE, raw_type, 2);
	if (rc) {
		vreg_err(vreg, "could not read type register, rc=%d\n", rc);
		return rc;
	}
	type = raw_type[0];
	subtype = raw_type[1];

	rc = -ENODEV;
	for (i = 0; i < ARRAY_SIZE(supported_regulators); i++) {
		mapping = &supported_regulators[i];
		if (mapping->type == type && mapping->subtype == subtype) {
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

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
	    && pdata->bypass_mode_enable != QPNP_REGULATOR_USE_HW_DEFAULT) {
		ctrl_reg[QPNP_COMMON_IDX_MODE] &=
			~QPNP_COMMON_MODE_BYPASS_MASK;
		ctrl_reg[QPNP_COMMON_IDX_MODE] |=
			(pdata->bypass_mode_enable
				? QPNP_COMMON_MODE_BYPASS_MASK : 0);
	}

	/* Set boost current limit. */
	if (type == QPNP_REGULATOR_LOGICAL_TYPE_BOOST
		&& pdata->boost_current_limit
			!= QPNP_BOOST_CURRENT_LIMIT_HW_DEFAULT) {
		ctrl_reg[QPNP_BOOST_IDX_CURRENT_LIMIT] &=
			~QPNP_BOOST_CURRENT_LIMIT_MASK;
		ctrl_reg[QPNP_BOOST_IDX_CURRENT_LIMIT] |=
		     pdata->boost_current_limit & QPNP_BOOST_CURRENT_LIMIT_MASK;
	}

	/* Write back any control register values that were modified. */
	rc = qpnp_vreg_write_optimized(vreg, QPNP_COMMON_REG_VOLTAGE_RANGE,
		ctrl_reg, vreg->ctrl_reg, 8);
	if (rc) {
		vreg_err(vreg, "spmi write failed, rc=%d\n", rc);
		return rc;
	}

	/* Set pull down. */
	if ((type == QPNP_REGULATOR_LOGICAL_TYPE_SMPS
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

	if (type == QPNP_REGULATOR_LOGICAL_TYPE_FTSMPS
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
	if (type == QPNP_REGULATOR_LOGICAL_TYPE_LDO
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
			reg = pdata->ocp_enable ? QPNP_VS_OCP_ENABLE_MASK : 0;
			rc = qpnp_vreg_write(vreg, QPNP_VS_REG_OCP, &reg, 1);
			if (rc) {
				vreg_err(vreg, "spmi write failed, rc=%d\n",
					rc);
				return rc;
			}
		}
	}

	return rc;
}

/* Fill in pdata elements based on values found in device tree. */
static int qpnp_regulator_get_dt_config(struct spmi_device *spmi,
				struct qpnp_regulator_platform_data *pdata)
{
	struct resource *res;
	struct device_node *node = spmi->dev.of_node;
	int rc = 0;

	pdata->init_data.constraints.input_uV
		= pdata->init_data.constraints.max_uV;

	res = qpnp_get_resource(spmi, 0, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing base address\n",
			__func__);
		return -EINVAL;
	}
	pdata->base_addr = res->start;

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

	/* These bindings are optional, so it is okay if they are not found. */
	of_property_read_u32(node, "qcom,auto-mode-enable",
		&pdata->auto_mode_enable);
	of_property_read_u32(node, "qcom,bypass-mode-enable",
		&pdata->bypass_mode_enable);
	of_property_read_u32(node, "qcom,ocp-enable", &pdata->ocp_enable);
	of_property_read_u32(node, "qcom,pull-down-enable",
		&pdata->pull_down_enable);
	of_property_read_u32(node, "qcom,soft-start-enable",
		&pdata->soft_start_enable);
	of_property_read_u32(node, "qcom,boost-current-limit",
		&pdata->boost_current_limit);
	of_property_read_u32(node, "qcom,pin-ctrl-enable",
		&pdata->pin_ctrl_enable);
	of_property_read_u32(node, "qcom,pin-ctrl-hpm", &pdata->pin_ctrl_hpm);
	of_property_read_u32(node, "qcom,vs-soft-start-strength",
		&pdata->vs_soft_start_strength);
	of_property_read_u32(node, "qcom,system-load", &pdata->system_load);
	of_property_read_u32(node, "qcom,enable-time", &pdata->enable_time);
	of_property_read_u32(node, "qcom,ocp-enable-time",
		&pdata->ocp_enable_time);

	return rc;
}

static struct of_device_id spmi_match_table[];

#define MAX_NAME_LEN	127

static int __devinit qpnp_regulator_probe(struct spmi_device *spmi)
{
	struct qpnp_regulator_platform_data *pdata;
	struct qpnp_regulator *vreg;
	struct regulator_desc *rdesc;
	struct qpnp_regulator_platform_data of_pdata;
	struct regulator_init_data *init_data;
	char *reg_name;
	int rc;
	bool is_dt;

	vreg = kzalloc(sizeof(struct qpnp_regulator), GFP_KERNEL);
	if (!vreg) {
		dev_err(&spmi->dev, "%s: Can't allocate qpnp_regulator\n",
			__func__);
		return -ENOMEM;
	}

	is_dt = of_match_device(spmi_match_table, &spmi->dev);

	/* Check if device tree is in use. */
	if (is_dt) {
		init_data = of_get_regulator_init_data(&spmi->dev);
		if (!init_data) {
			dev_err(&spmi->dev, "%s: unable to allocate memory\n",
					__func__);
			kfree(vreg);
			return -ENOMEM;
		}
		memset(&of_pdata, 0,
			sizeof(struct qpnp_regulator_platform_data));
		memcpy(&of_pdata.init_data, init_data,
			sizeof(struct regulator_init_data));

		if (of_get_property(spmi->dev.of_node, "parent-supply", NULL))
			of_pdata.init_data.supply_regulator = "parent";

		rc = qpnp_regulator_get_dt_config(spmi, &of_pdata);
		if (rc) {
			dev_err(&spmi->dev, "%s: DT parsing failed, rc=%d\n",
					__func__, rc);
			kfree(vreg);
			return -ENOMEM;
		}

		pdata = &of_pdata;
	} else {
		pdata = spmi->dev.platform_data;
	}

	if (pdata == NULL) {
		dev_err(&spmi->dev, "%s: no platform data specified\n",
			__func__);
		kfree(vreg);
		return -EINVAL;
	}

	vreg->spmi_dev		= spmi;
	vreg->prev_write_count	= -1;
	vreg->write_count	= 0;
	vreg->base_addr		= pdata->base_addr;
	vreg->enable_time	= pdata->enable_time;
	vreg->system_load	= pdata->system_load;
	vreg->ocp_enable	= pdata->ocp_enable;
	vreg->ocp_enable_time	= pdata->ocp_enable_time;

	rdesc			= &vreg->rdesc;
	rdesc->id		= spmi->ctrl->nr;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;

	reg_name = kzalloc(strnlen(pdata->init_data.constraints.name,
				MAX_NAME_LEN) + 1, GFP_KERNEL);
	if (!reg_name) {
		dev_err(&spmi->dev, "%s: Can't allocate regulator name\n",
			__func__);
		kfree(vreg);
		return -ENOMEM;
	}
	strlcpy(reg_name, pdata->init_data.constraints.name,
		strnlen(pdata->init_data.constraints.name, MAX_NAME_LEN) + 1);
	rdesc->name = reg_name;

	dev_set_drvdata(&spmi->dev, vreg);

	rc = qpnp_regulator_match(vreg);
	if (rc) {
		vreg_err(vreg, "regulator type unknown, rc=%d\n", rc);
		goto bail;
	}

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

	rc = qpnp_regulator_init_registers(vreg, pdata);
	if (rc) {
		vreg_err(vreg, "common initialization failed, rc=%d\n", rc);
		goto bail;
	}

	vreg->rdev = regulator_register(rdesc, &spmi->dev,
			&(pdata->init_data), vreg, spmi->dev.of_node);
	if (IS_ERR(vreg->rdev)) {
		rc = PTR_ERR(vreg->rdev);
		vreg_err(vreg, "regulator_register failed, rc=%d\n", rc);
		goto bail;
	}

	qpnp_vreg_show_state(vreg->rdev, QPNP_REGULATOR_ACTION_INIT);

	return 0;

bail:
	if (rc)
		vreg_err(vreg, "probe failed, rc=%d\n", rc);

	kfree(vreg->rdesc.name);
	kfree(vreg);

	return rc;
}

static int __devexit qpnp_regulator_remove(struct spmi_device *spmi)
{
	struct qpnp_regulator *vreg;

	vreg = dev_get_drvdata(&spmi->dev);
	dev_set_drvdata(&spmi->dev, NULL);

	if (vreg) {
		regulator_unregister(vreg->rdev);
		kfree(vreg->rdesc.name);
		kfree(vreg);
	}

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = QPNP_REGULATOR_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id qpnp_regulator_id[] = {
	{ QPNP_REGULATOR_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, qpnp_regulator_id);

static struct spmi_driver qpnp_regulator_driver = {
	.driver		= {
		.name	= QPNP_REGULATOR_DRIVER_NAME,
		.of_match_table = spmi_match_table,
		.owner = THIS_MODULE,
	},
	.probe		= qpnp_regulator_probe,
	.remove		= __devexit_p(qpnp_regulator_remove),
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
				= (all_set_points[i]->range[j].max_uV
				 - all_set_points[i]->range[j].set_point_min_uV)
				   / all_set_points[i]->range[j].step_uV + 1;
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
	else
		has_registered = true;

	qpnp_regulator_set_point_init();

	return spmi_driver_register(&qpnp_regulator_driver);
}
EXPORT_SYMBOL(qpnp_regulator_init);

static void __exit qpnp_regulator_exit(void)
{
	spmi_driver_unregister(&qpnp_regulator_driver);
}

MODULE_DESCRIPTION("QPNP PMIC regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(qpnp_regulator_init);
module_exit(qpnp_regulator_exit);
