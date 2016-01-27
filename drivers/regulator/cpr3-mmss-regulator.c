/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include "cpr3-regulator.h"

#define MSM8996_MMSS_FUSE_CORNERS	4

/**
 * struct cpr3_msm8996_mmss_fuses - MMSS specific fuse data for MSM8996
 * @init_voltage:	Initial (i.e. open-loop) voltage fuse parameter value
 *			for each fuse corner (raw, not converted to a voltage)
 * @offset_voltage:	The closed-loop voltage margin adjustment fuse parameter
 *			value for each fuse corner (raw, not converted to a
 *			voltage)
 * @speed_bin:		Graphics processor speed bin fuse parameter value for
 *			the given chip
 * @cpr_fusing_rev:	CPR fusing revision fuse parameter value
 * @limitation:		CPR limitation select fuse parameter value
 * @aging_init_quot_diff:	Initial quotient difference between CPR aging
 *			min and max sensors measured at time of manufacturing
 *
 * This struct holds the values for all of the fuses read from memory.
 */
struct cpr3_msm8996_mmss_fuses {
	u64	init_voltage[MSM8996_MMSS_FUSE_CORNERS];
	u64	offset_voltage[MSM8996_MMSS_FUSE_CORNERS];
	u64	speed_bin;
	u64	cpr_fusing_rev;
	u64	limitation;
	u64	aging_init_quot_diff;
};

/* Fuse combos 0 -  7 map to CPR fusing revision 0 - 7 */
#define CPR3_MSM8996_MMSS_FUSE_COMBO_COUNT	8

/*
 * Fuse combos 0 -  7 map to CPR fusing revision 0 - 7 with speed bin fuse = 0.
 * Fuse combos 8 - 15 map to CPR fusing revision 0 - 7 with speed bin fuse = 1.
 */
#define CPR3_MSM8996PRO_MMSS_FUSE_COMBO_COUNT	16

/*
 * MSM8996 MMSS fuse parameter locations:
 *
 * Structs are organized with the following dimensions:
 *	Outer: 0 to 3 for fuse corners from lowest to highest corner
 *	Inner: large enough to hold the longest set of parameter segments which
 *		fully defines a fuse parameter, +1 (for NULL termination).
 *		Each segment corresponds to a contiguous group of bits from a
 *		single fuse row.  These segments are concatentated together in
 *		order to form the full fuse parameter value.  The segments for
 *		a given parameter may correspond to different fuse rows.
 */
static const struct cpr3_fuse_param
msm8996_mmss_init_voltage_param[MSM8996_MMSS_FUSE_CORNERS][2] = {
	{{63, 55, 59}, {} },
	{{63, 50, 54}, {} },
	{{63, 45, 49}, {} },
	{{63, 40, 44}, {} },
};

static const struct cpr3_fuse_param msm8996_cpr_fusing_rev_param[] = {
	{39, 48, 50},
	{},
};

static const struct cpr3_fuse_param msm8996_cpr_limitation_param[] = {
	{41, 31, 32},
	{},
};

static const struct cpr3_fuse_param
msm8996_mmss_aging_init_quot_diff_param[] = {
	{68, 26, 31},
	{},
};

/* Offset voltages are defined for SVS and Turbo fuse corners only */
static const struct cpr3_fuse_param
msm8996_mmss_offset_voltage_param[MSM8996_MMSS_FUSE_CORNERS][2] = {
	{{} },
	{{66, 42, 44}, {} },
	{{} },
	{{64, 58, 61}, {} },
};

static const struct cpr3_fuse_param msm8996pro_mmss_speed_bin_param[] = {
	{39, 60, 61},
	{},
};

#define MSM8996PRO_SOC_ID			4

/*
 * Some initial msm8996 parts cannot be used in a meaningful way by software.
 * Other parts can only be used when operating with CPR disabled (i.e. at the
 * fused open-loop voltage) when no voltage interpolation is applied.  A fuse
 * parameter is provided so that software can properly handle these limitations.
 */
enum msm8996_cpr_limitation {
	MSM8996_CPR_LIMITATION_NONE = 0,
	MSM8996_CPR_LIMITATION_UNSUPPORTED = 2,
	MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION = 3,
};

/* Additional MSM8996 specific data: */

/* Open loop voltage fuse reference voltages in microvolts */
static const int msm8996_mmss_fuse_ref_volt[MSM8996_MMSS_FUSE_CORNERS] = {
	670000,
	745000,
	905000,
	1015000,
};

static const int msm8996pro_mmss_fuse_ref_volt[MSM8996_MMSS_FUSE_CORNERS] = {
	670000,
	745000,
	905000,
	1065000,
};

#define MSM8996_MMSS_FUSE_STEP_VOLT		10000
#define MSM8996_MMSS_OFFSET_FUSE_STEP_VOLT	10000
#define MSM8996_MMSS_VOLTAGE_FUSE_SIZE		5
#define MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SCALE	2
#define MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SIZE	6

#define MSM8996_MMSS_CPR_SENSOR_COUNT		35

#define MSM8996_MMSS_CPR_CLOCK_RATE		19200000

#define MSM8996_MMSS_AGING_SENSOR_ID		29
#define MSM8996_MMSS_AGING_BYPASS_MASK0		(GENMASK(23, 0))

/**
 * cpr3_msm8996_mmss_read_fuse_data() - load MMSS specific fuse parameter values
 * @vreg:		Pointer to the CPR3 regulator
 *
 * This function allocates a cpr3_msm8996_mmss_fuses struct, fills it with
 * values read out of hardware fuses, and finally copies common fuse values
 * into the regulator struct.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8996_mmss_read_fuse_data(struct cpr3_regulator *vreg)
{
	void __iomem *base = vreg->thread->ctrl->fuse_base;
	struct cpr3_msm8996_mmss_fuses *fuse;
	int i, rc, combo_max;

	fuse = devm_kzalloc(vreg->thread->ctrl->dev, sizeof(*fuse), GFP_KERNEL);
	if (!fuse)
		return -ENOMEM;

	if (vreg->thread->ctrl->soc_revision == MSM8996PRO_SOC_ID) {
		rc = cpr3_read_fuse_param(base, msm8996pro_mmss_speed_bin_param,
					&fuse->speed_bin);
		if (rc) {
			cpr3_err(vreg, "Unable to read speed bin fuse, rc=%d\n",
				rc);
			return rc;
		}
		cpr3_info(vreg, "speed bin = %llu\n", fuse->speed_bin);
	}

	rc = cpr3_read_fuse_param(base, msm8996_cpr_fusing_rev_param,
				&fuse->cpr_fusing_rev);
	if (rc) {
		cpr3_err(vreg, "Unable to read CPR fusing revision fuse, rc=%d\n",
			rc);
		return rc;
	}
	cpr3_info(vreg, "CPR fusing revision = %llu\n", fuse->cpr_fusing_rev);

	rc = cpr3_read_fuse_param(base, msm8996_cpr_limitation_param,
				&fuse->limitation);
	if (rc) {
		cpr3_err(vreg, "Unable to read CPR limitation fuse, rc=%d\n",
			rc);
		return rc;
	}
	cpr3_info(vreg, "CPR limitation = %s\n",
		fuse->limitation == MSM8996_CPR_LIMITATION_UNSUPPORTED
		? "unsupported chip" : fuse->limitation
			  == MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION
		? "CPR disabled and no interpolation" : "none");

	rc = cpr3_read_fuse_param(base, msm8996_mmss_aging_init_quot_diff_param,
				&fuse->aging_init_quot_diff);
	if (rc) {
		cpr3_err(vreg, "Unable to read aging initial quotient difference fuse, rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < MSM8996_MMSS_FUSE_CORNERS; i++) {
		rc = cpr3_read_fuse_param(base,
			msm8996_mmss_init_voltage_param[i],
			&fuse->init_voltage[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d initial voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
			msm8996_mmss_offset_voltage_param[i],
			&fuse->offset_voltage[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d offset voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}
	}

	if (vreg->thread->ctrl->soc_revision == MSM8996PRO_SOC_ID) {
		combo_max = CPR3_MSM8996PRO_MMSS_FUSE_COMBO_COUNT;
		vreg->fuse_combo = fuse->cpr_fusing_rev + 8 * fuse->speed_bin;
	} else {
		combo_max = CPR3_MSM8996_MMSS_FUSE_COMBO_COUNT;
		vreg->fuse_combo = fuse->cpr_fusing_rev;
	}

	if (vreg->fuse_combo >= combo_max) {
		cpr3_err(vreg, "invalid CPR fuse combo = %d found, not in range 0 - %d\n",
			vreg->fuse_combo, combo_max - 1);
		return -EINVAL;
	}

	vreg->speed_bin_fuse	= fuse->speed_bin;
	vreg->cpr_rev_fuse	= fuse->cpr_fusing_rev;
	vreg->fuse_corner_count	= MSM8996_MMSS_FUSE_CORNERS;
	vreg->platform_fuses	= fuse;

	return 0;
}

/**
 * cpr3_mmss_parse_corner_data() - parse MMSS corner data from device tree
 *		properties of the regulator's device node
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_mmss_parse_corner_data(struct cpr3_regulator *vreg)
{
	int i, rc;
	u32 *temp;

	rc = cpr3_parse_common_corner_data(vreg);
	if (rc) {
		cpr3_err(vreg, "error reading corner data, rc=%d\n", rc);
		return rc;
	}

	temp = kcalloc(vreg->corner_count * CPR3_RO_COUNT, sizeof(*temp),
			GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	rc = cpr3_parse_corner_array_property(vreg, "qcom,cpr-target-quotients",
			CPR3_RO_COUNT, temp);
	if (rc) {
		cpr3_err(vreg, "could not load target quotients, rc=%d\n", rc);
		goto done;
	}

	for (i = 0; i < vreg->corner_count; i++)
		memcpy(vreg->corner[i].target_quot, &temp[i * CPR3_RO_COUNT],
			sizeof(*temp) * CPR3_RO_COUNT);

done:
	kfree(temp);
	return rc;
}

/**
 * cpr3_msm8996_mmss_apply_closed_loop_offset_voltages() - modify the
 *		closed-loop voltage adjustments by the amounts that are needed
 *		for this fuse combo
 * @vreg:		Pointer to the CPR3 regulator
 * @volt_adjust:	Array of closed-loop voltage adjustment values of length
 *			vreg->corner_count which is further adjusted based upon
 *			offset voltage fuse values.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8996_mmss_apply_closed_loop_offset_voltages(
			struct cpr3_regulator *vreg, int *volt_adjust)
{
	struct cpr3_msm8996_mmss_fuses *fuse = vreg->platform_fuses;
	u32 *corner_map;
	int *volt_offset;
	int rc = 0, i, fuse_len;

	if (!of_find_property(vreg->of_node,
		"qcom,cpr-fused-closed-loop-voltage-adjustment-map", NULL)) {
		/* No closed-loop offset required. */
		return 0;
	}

	corner_map = kcalloc(vreg->corner_count, sizeof(*corner_map),
				GFP_KERNEL);
	volt_offset = kcalloc(vreg->fuse_corner_count, sizeof(*volt_offset),
				GFP_KERNEL);
	if (!corner_map || !volt_offset) {
		rc = -ENOMEM;
		goto done;
	}

	rc = cpr3_parse_corner_array_property(vreg,
		"qcom,cpr-fused-closed-loop-voltage-adjustment-map",
		1, corner_map);
	if (rc)
		goto done;

	for (i = 0; i < vreg->fuse_corner_count; i++) {
		fuse_len = msm8996_mmss_offset_voltage_param[i][0].bit_end + 1
			   - msm8996_mmss_offset_voltage_param[i][0].bit_start;
		volt_offset[i] = cpr3_convert_open_loop_voltage_fuse(
			0, MSM8996_MMSS_OFFSET_FUSE_STEP_VOLT,
			fuse->offset_voltage[i], fuse_len);
		if (volt_offset[i])
			cpr3_info(vreg, "fuse_corner[%d] offset=%7d uV\n",
				i, volt_offset[i]);
	}

	for (i = 0; i < vreg->corner_count; i++) {
		if (corner_map[i] == 0) {
			continue;
		} else if (corner_map[i] > vreg->fuse_corner_count) {
			cpr3_err(vreg, "corner %d mapped to invalid fuse corner: %u\n",
				i, corner_map[i]);
			rc = -EINVAL;
			goto done;
		}

		volt_adjust[i] += volt_offset[corner_map[i] - 1];
	}

done:
	kfree(corner_map);
	kfree(volt_offset);

	return rc;
}

/**
 * cpr3_mmss_enforce_inc_quotient_monotonicity() - Ensure that target quotients
 *		increase monotonically from lower to higher corners
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static void cpr3_mmss_enforce_inc_quotient_monotonicity(
		struct cpr3_regulator *vreg)
{
	int i, j;

	for (i = 1; i < vreg->corner_count; i++) {
		for (j = 0; j < CPR3_RO_COUNT; j++) {
			if (vreg->corner[i].target_quot[j]
			    && vreg->corner[i].target_quot[j]
					< vreg->corner[i - 1].target_quot[j]) {
				cpr3_debug(vreg, "corner %d RO%u target quot=%u < corner %d RO%u target quot=%u; overriding: corner %d RO%u target quot=%u\n",
					i, j,
					vreg->corner[i].target_quot[j],
					i - 1, j,
					vreg->corner[i - 1].target_quot[j],
					i, j,
					vreg->corner[i - 1].target_quot[j]);
				vreg->corner[i].target_quot[j]
					= vreg->corner[i - 1].target_quot[j];
			}
		}
	}
}

/**
 * cpr3_mmss_enforce_dec_quotient_monotonicity() - Ensure that target quotients
 *		decrease monotonically from higher to lower corners
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static void cpr3_mmss_enforce_dec_quotient_monotonicity(
		struct cpr3_regulator *vreg)
{
	int i, j;

	for (i = vreg->corner_count - 2; i >= 0; i--) {
		for (j = 0; j < CPR3_RO_COUNT; j++) {
			if (vreg->corner[i].target_quot[j]
			    && vreg->corner[i].target_quot[j]
					> vreg->corner[i + 1].target_quot[j]) {
				cpr3_debug(vreg, "corner %d RO%u target quot=%u > corner %d RO%u target quot=%u; overriding: corner %d RO%u target quot=%u\n",
					i, j,
					vreg->corner[i].target_quot[j],
					i + 1, j,
					vreg->corner[i + 1].target_quot[j],
					i, j,
					vreg->corner[i + 1].target_quot[j]);
				vreg->corner[i].target_quot[j]
					= vreg->corner[i + 1].target_quot[j];
			}
		}
	}
}

/**
 * _cpr3_mmss_adjust_target_quotients() - adjust the target quotients for each
 *		corner of the regulator according to input adjustment and
 *		scaling arrays
 * @vreg:		Pointer to the CPR3 regulator
 * @volt_adjust:	Pointer to an array of closed-loop voltage adjustments
 *			with units of microvolts.  The array must have
 *			vreg->corner_count number of elements.
 * @ro_scale:		Pointer to a flattened 2D array of RO scaling factors.
 *			The array must have an inner dimension of CPR3_RO_COUNT
 *			and an outer dimension of vreg->corner_count
 * @label:		Null terminated string providing a label for the type
 *			of adjustment.
 *
 * Return: true if any corners received a positive voltage adjustment (> 0),
 *	   else false
 */
static bool _cpr3_mmss_adjust_target_quotients(struct cpr3_regulator *vreg,
		const int *volt_adjust, const int *ro_scale, const char *label)
{
	int i, j, quot_adjust;
	bool is_increasing = false;
	u32 prev_quot;

	for (i = 0; i < vreg->corner_count; i++) {
		for (j = 0; j < CPR3_RO_COUNT; j++) {
			if (vreg->corner[i].target_quot[j]) {
				quot_adjust = cpr3_quot_adjustment(
					ro_scale[i * CPR3_RO_COUNT + j],
					volt_adjust[i]);
				if (quot_adjust) {
					prev_quot = vreg->corner[i].
							target_quot[j];
					vreg->corner[i].target_quot[j]
						+= quot_adjust;
					cpr3_debug(vreg, "adjusted corner %d RO%d target quot %s: %u --> %u (%d uV)\n",
						i, j, label, prev_quot,
						vreg->corner[i].target_quot[j],
						volt_adjust[i]);
				}
			}
		}
		if (volt_adjust[i] > 0)
			is_increasing = true;
	}

	return is_increasing;
}

/**
 * cpr3_mmss_adjust_target_quotients() - adjust the target quotients for each
 *		corner according to device tree values and fuse values
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_mmss_adjust_target_quotients(struct cpr3_regulator *vreg)
{
	int i, rc;
	int *volt_adjust, *ro_scale;
	bool explicit_adjustment, fused_adjustment, is_increasing;

	explicit_adjustment = of_find_property(vreg->of_node,
		"qcom,cpr-closed-loop-voltage-adjustment", NULL);
	fused_adjustment = of_find_property(vreg->of_node,
		"qcom,cpr-fused-closed-loop-voltage-adjustment-map", NULL);

	if (!explicit_adjustment && !fused_adjustment && !vreg->aging_allowed) {
		/* No adjustment required. */
		return 0;
	} else if (!of_find_property(vreg->of_node,
			"qcom,cpr-ro-scaling-factor", NULL)) {
		cpr3_err(vreg, "qcom,cpr-ro-scaling-factor is required for closed-loop voltage adjustment, but is missing\n");
		return -EINVAL;
	}

	volt_adjust = kcalloc(vreg->corner_count, sizeof(*volt_adjust),
				GFP_KERNEL);
	ro_scale = kcalloc(vreg->corner_count * CPR3_RO_COUNT,
				sizeof(*ro_scale), GFP_KERNEL);
	if (!volt_adjust || !ro_scale) {
		rc = -ENOMEM;
		goto done;
	}

	rc = cpr3_parse_corner_array_property(vreg,
			"qcom,cpr-ro-scaling-factor", CPR3_RO_COUNT, ro_scale);
	if (rc) {
		cpr3_err(vreg, "could not load RO scaling factors, rc=%d\n",
			rc);
		goto done;
	}

	for (i = 0; i < vreg->corner_count; i++)
		memcpy(vreg->corner[i].ro_scale, &ro_scale[i * CPR3_RO_COUNT],
			sizeof(*ro_scale) * CPR3_RO_COUNT);

	if (explicit_adjustment) {
		rc = cpr3_parse_corner_array_property(vreg,
			"qcom,cpr-closed-loop-voltage-adjustment",
			1, volt_adjust);
		if (rc) {
			cpr3_err(vreg, "could not load closed-loop voltage adjustments, rc=%d\n",
				rc);
			goto done;
		}

		_cpr3_mmss_adjust_target_quotients(vreg, volt_adjust, ro_scale,
			"from DT");
		cpr3_mmss_enforce_inc_quotient_monotonicity(vreg);
	}

	if (fused_adjustment) {
		memset(volt_adjust, 0,
			sizeof(*volt_adjust) * vreg->corner_count);

		rc = cpr3_msm8996_mmss_apply_closed_loop_offset_voltages(vreg,
			volt_adjust);
		if (rc) {
			cpr3_err(vreg, "could not apply fused closed-loop voltage reductions, rc=%d\n",
				rc);
			goto done;
		}

		is_increasing = _cpr3_mmss_adjust_target_quotients(vreg,
					volt_adjust, ro_scale, "from fuse");
		if (is_increasing)
			cpr3_mmss_enforce_inc_quotient_monotonicity(vreg);
		else
			cpr3_mmss_enforce_dec_quotient_monotonicity(vreg);
	}

done:
	kfree(volt_adjust);
	kfree(ro_scale);
	return rc;
}

/**
 * cpr3_msm8996_mmss_calculate_open_loop_voltages() - calculate the open-loop
 *		voltage for each corner of a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * If open-loop voltage interpolation is allowed in both device tree and in
 * hardware fuses, then this function calculates the open-loop voltage for a
 * given corner using linear interpolation.  This interpolation is performed
 * using the processor frequencies of the lower and higher Fmax corners along
 * with their fused open-loop voltages.
 *
 * If open-loop voltage interpolation is not allowed, then this function uses
 * the Fmax fused open-loop voltage for all of the corners associated with a
 * given fuse corner.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8996_mmss_calculate_open_loop_voltages(
			struct cpr3_regulator *vreg)
{
	struct device_node *node = vreg->of_node;
	struct cpr3_msm8996_mmss_fuses *fuse = vreg->platform_fuses;
	int rc = 0;
	bool allow_interpolation;
	u64 freq_low, volt_low, freq_high, volt_high;
	int i, j;
	const int *ref_volt;
	int *fuse_volt;
	int *fmax_corner;

	fuse_volt = kcalloc(vreg->fuse_corner_count, sizeof(*fuse_volt),
				GFP_KERNEL);
	fmax_corner = kcalloc(vreg->fuse_corner_count, sizeof(*fmax_corner),
				GFP_KERNEL);
	if (!fuse_volt || !fmax_corner) {
		rc = -ENOMEM;
		goto done;
	}

	if (vreg->thread->ctrl->soc_revision == MSM8996PRO_SOC_ID)
		ref_volt = msm8996pro_mmss_fuse_ref_volt;
	else
		ref_volt = msm8996_mmss_fuse_ref_volt;

	for (i = 0; i < vreg->fuse_corner_count; i++) {
		fuse_volt[i] = cpr3_convert_open_loop_voltage_fuse(ref_volt[i],
			MSM8996_MMSS_FUSE_STEP_VOLT, fuse->init_voltage[i],
			MSM8996_MMSS_VOLTAGE_FUSE_SIZE);
		cpr3_info(vreg, "fuse_corner[%d] open-loop=%7d uV\n",
			i, fuse_volt[i]);
	}

	rc = cpr3_adjust_fused_open_loop_voltages(vreg, fuse_volt);
	if (rc) {
		cpr3_err(vreg, "fused open-loop voltage adjustment failed, rc=%d\n",
			rc);
		goto done;
	}

	allow_interpolation = of_property_read_bool(node,
				"qcom,allow-voltage-interpolation");

	for (i = 1; i < vreg->fuse_corner_count; i++) {
		if (fuse_volt[i] < fuse_volt[i - 1]) {
			cpr3_debug(vreg, "fuse corner %d voltage=%d uV < fuse corner %d voltage=%d uV; overriding: fuse corner %d voltage=%d\n",
				i, fuse_volt[i], i - 1, fuse_volt[i - 1],
				i, fuse_volt[i - 1]);
			fuse_volt[i] = fuse_volt[i - 1];
		}
	}

	if (fuse->limitation == MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION)
		allow_interpolation = false;

	if (!allow_interpolation) {
		/* Use fused open-loop voltage for lower frequencies. */
		for (i = 0; i < vreg->corner_count; i++)
			vreg->corner[i].open_loop_volt
				= fuse_volt[vreg->corner[i].cpr_fuse_corner];
		goto done;
	}

	/* Determine highest corner mapped to each fuse corner */
	j = vreg->fuse_corner_count - 1;
	for (i = vreg->corner_count - 1; i >= 0; i--) {
		if (vreg->corner[i].cpr_fuse_corner == j) {
			fmax_corner[j] = i;
			j--;
		}
	}
	if (j >= 0) {
		cpr3_err(vreg, "invalid fuse corner mapping\n");
		rc = -EINVAL;
		goto done;
	}

	/*
	 * Interpolation is not possible for corners mapped to the lowest fuse
	 * corner so use the fuse corner value directly.
	 */
	for (i = 0; i <= fmax_corner[0]; i++)
		vreg->corner[i].open_loop_volt = fuse_volt[0];

	/* Interpolate voltages for the higher fuse corners. */
	for (i = 1; i < vreg->fuse_corner_count; i++) {
		freq_low = vreg->corner[fmax_corner[i - 1]].proc_freq;
		volt_low = fuse_volt[i - 1];
		freq_high = vreg->corner[fmax_corner[i]].proc_freq;
		volt_high = fuse_volt[i];

		for (j = fmax_corner[i - 1] + 1; j <= fmax_corner[i]; j++)
			vreg->corner[j].open_loop_volt = cpr3_interpolate(
				freq_low, volt_low, freq_high, volt_high,
				vreg->corner[j].proc_freq);
	}

done:
	if (rc == 0) {
		cpr3_debug(vreg, "unadjusted per-corner open-loop voltages:\n");
		for (i = 0; i < vreg->corner_count; i++)
			cpr3_debug(vreg, "open-loop[%2d] = %d uV\n", i,
				vreg->corner[i].open_loop_volt);

		rc = cpr3_adjust_open_loop_voltages(vreg);
		if (rc)
			cpr3_err(vreg, "open-loop voltage adjustment failed, rc=%d\n",
				rc);
	}

	kfree(fuse_volt);
	kfree(fmax_corner);
	return rc;
}

/**
 * cpr3_mmss_print_settings() - print out MMSS CPR configuration settings into
 *		the kernel log for debugging purposes
 * @vreg:		Pointer to the CPR3 regulator
 */
static void cpr3_mmss_print_settings(struct cpr3_regulator *vreg)
{
	struct cpr3_corner *corner;
	int i;

	cpr3_debug(vreg, "Corner: Frequency (Hz), Fuse Corner, Floor (uV), Open-Loop (uV), Ceiling (uV)\n");
	for (i = 0; i < vreg->corner_count; i++) {
		corner = &vreg->corner[i];
		cpr3_debug(vreg, "%3d: %10u, %2d, %7d, %7d, %7d\n",
			i, corner->proc_freq, corner->cpr_fuse_corner,
			corner->floor_volt, corner->open_loop_volt,
			corner->ceiling_volt);
	}
}

/**
 * cpr3_mmss_init_aging() - perform MMSS CPR3 controller specific
 *		aging initializations
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_mmss_init_aging(struct cpr3_controller *ctrl)
{
	struct cpr3_msm8996_mmss_fuses *fuse;
	struct cpr3_regulator *vreg;
	u32 aging_ro_scale;
	int rc;

	vreg = &ctrl->thread[0].vreg[0];

	ctrl->aging_required = vreg->aging_allowed;
	fuse = vreg->platform_fuses;

	if (!ctrl->aging_required || !fuse)
		return 0;

	rc = cpr3_parse_array_property(vreg, "qcom,cpr-aging-ro-scaling-factor",
			1, &aging_ro_scale);
	if (rc)
		return rc;

	if (aging_ro_scale == 0) {
		cpr3_err(ctrl, "aging RO scaling factor is invalid: %u\n",
			aging_ro_scale);
		return -EINVAL;
	}

	ctrl->aging_vdd_mode = REGULATOR_MODE_NORMAL;
	ctrl->aging_complete_vdd_mode = REGULATOR_MODE_IDLE;

	ctrl->aging_sensor_count = 1;
	ctrl->aging_sensor = kzalloc(sizeof(*ctrl->aging_sensor), GFP_KERNEL);
	if (!ctrl->aging_sensor)
		return -ENOMEM;

	ctrl->aging_sensor->sensor_id = MSM8996_MMSS_AGING_SENSOR_ID;
	ctrl->aging_sensor->bypass_mask[0] = MSM8996_MMSS_AGING_BYPASS_MASK0;
	ctrl->aging_sensor->ro_scale = aging_ro_scale;

	ctrl->aging_sensor->init_quot_diff
		= cpr3_convert_open_loop_voltage_fuse(0,
			MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SCALE,
			fuse->aging_init_quot_diff,
			MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SIZE);

	cpr3_debug(ctrl, "sensor %u aging init quotient diff = %d, aging RO scale = %u QUOT/V\n",
		ctrl->aging_sensor->sensor_id,
		ctrl->aging_sensor->init_quot_diff,
		ctrl->aging_sensor->ro_scale);

	return 0;
}

/**
 * cpr3_mmss_init_thread() - perform all steps necessary to initialize the
 *		configuration data for a CPR3 thread
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_mmss_init_thread(struct cpr3_thread *thread)
{
	struct cpr3_regulator *vreg = &thread->vreg[0];
	struct cpr3_msm8996_mmss_fuses *fuse;
	int rc;

	rc = cpr3_parse_common_thread_data(thread);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR thread data from device tree, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_msm8996_mmss_read_fuse_data(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR fuse data, rc=%d\n", rc);
		return rc;
	}

	fuse = vreg->platform_fuses;
	if (fuse->limitation == MSM8996_CPR_LIMITATION_UNSUPPORTED) {
		cpr3_err(vreg, "this chip requires an unsupported voltage\n");
		return -EPERM;
	} else if (fuse->limitation
			== MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION) {
		thread->ctrl->cpr_allowed_hw = false;
	}

	rc = cpr3_mmss_parse_corner_data(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR corner data from device tree, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_mmss_adjust_target_quotients(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to adjust target quotients, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_msm8996_mmss_calculate_open_loop_voltages(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to calculate open-loop voltages, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_limit_open_loop_voltages(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to limit open-loop voltages, rc=%d\n",
			rc);
		return rc;
	}

	cpr3_open_loop_voltage_as_ceiling(vreg);

	rc = cpr3_limit_floor_voltages(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to limit floor voltages, rc=%d\n", rc);
		return rc;
	}

	cpr3_mmss_print_settings(vreg);

	return 0;
}

/**
 * cpr3_mmss_init_controller() - perform MMSS CPR3 controller specific
 *		initializations
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_mmss_init_controller(struct cpr3_controller *ctrl)
{
	int rc;

	rc = cpr3_parse_common_ctrl_data(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable to parse common controller data, rc=%d\n",
				rc);
		return rc;
	}

	ctrl->sensor_count = MSM8996_MMSS_CPR_SENSOR_COUNT;

	/*
	 * MMSS only has one thread (0) so the zeroed array does not need
	 * further modification.
	 */
	ctrl->sensor_owner = devm_kcalloc(ctrl->dev, ctrl->sensor_count,
				sizeof(*ctrl->sensor_owner), GFP_KERNEL);
	if (!ctrl->sensor_owner)
		return -ENOMEM;

	ctrl->cpr_clock_rate = MSM8996_MMSS_CPR_CLOCK_RATE;
	ctrl->ctrl_type = CPR_CTRL_TYPE_CPR3;

	ctrl->iface_clk = devm_clk_get(ctrl->dev, "iface_clk");
	if (IS_ERR(ctrl->iface_clk)) {
		rc = PTR_ERR(ctrl->iface_clk);
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable request interface clock, rc=%d\n",
				rc);
		return rc;
	}

	ctrl->bus_clk = devm_clk_get(ctrl->dev, "bus_clk");
	if (IS_ERR(ctrl->bus_clk)) {
		rc = PTR_ERR(ctrl->bus_clk);
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable request bus clock, rc=%d\n",
				rc);
		return rc;
	}

	return 0;
}

static int cpr3_mmss_regulator_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_suspend(ctrl);
}

static int cpr3_mmss_regulator_resume(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_resume(ctrl);
}

/* Data corresponds to the SoC revision */
static struct of_device_id cpr_regulator_match_table[] = {
	{
		.compatible = "qcom,cpr3-msm8996-v1-mmss-regulator",
		.data = (void *)(uintptr_t)1,
	},
	{
		.compatible = "qcom,cpr3-msm8996-v2-mmss-regulator",
		.data = (void *)(uintptr_t)2,
	},
	{
		.compatible = "qcom,cpr3-msm8996-v3-mmss-regulator",
		.data = (void *)(uintptr_t)3,
	},
	{
		.compatible = "qcom,cpr3-msm8996-mmss-regulator",
		.data = (void *)(uintptr_t)3,
	},
	{
		.compatible = "qcom,cpr3-msm8996pro-mmss-regulator",
		.data = (void *)(uintptr_t)MSM8996PRO_SOC_ID,
	},
	{}
};

static int cpr3_mmss_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct cpr3_controller *ctrl;
	int rc;

	if (!dev->of_node) {
		dev_err(dev, "Device tree node is missing\n");
		return -EINVAL;
	}

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	ctrl->dev = dev;
	/* Set to false later if anything precludes CPR operation. */
	ctrl->cpr_allowed_hw = true;

	rc = of_property_read_string(dev->of_node, "qcom,cpr-ctrl-name",
					&ctrl->name);
	if (rc) {
		cpr3_err(ctrl, "unable to read qcom,cpr-ctrl-name, rc=%d\n",
			rc);
		return rc;
	}

	match = of_match_node(cpr_regulator_match_table, dev->of_node);
	if (match)
		ctrl->soc_revision = (uintptr_t)match->data;
	else
		cpr3_err(ctrl, "could not find compatible string match\n");

	rc = cpr3_map_fuse_base(ctrl, pdev);
	if (rc) {
		cpr3_err(ctrl, "could not map fuse base address\n");
		return rc;
	}

	rc = cpr3_allocate_threads(ctrl, 0, 0);
	if (rc) {
		cpr3_err(ctrl, "failed to allocate CPR thread array, rc=%d\n",
			rc);
		return rc;
	}

	if (ctrl->thread_count != 1) {
		cpr3_err(ctrl, "expected 1 thread but found %d\n",
			ctrl->thread_count);
		return -EINVAL;
	} else if (ctrl->thread[0].vreg_count != 1) {
		cpr3_err(ctrl, "expected 1 regulator but found %d\n",
			ctrl->thread[0].vreg_count);
		return -EINVAL;
	}

	rc = cpr3_mmss_init_controller(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "failed to initialize CPR controller parameters, rc=%d\n",
				rc);
		return rc;
	}

	rc = cpr3_mmss_init_thread(&ctrl->thread[0]);
	if (rc) {
		cpr3_err(&ctrl->thread[0].vreg[0], "thread initialization failed, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_mem_acc_init(&ctrl->thread[0].vreg[0]);
	if (rc) {
		cpr3_err(ctrl, "failed to initialize mem-acc configuration, rc=%d\n",
			 rc);
		return rc;
	}

	rc = cpr3_mmss_init_aging(ctrl);
	if (rc) {
		cpr3_err(ctrl, "failed to initialize aging configurations, rc=%d\n",
			rc);
		return rc;
	}

	platform_set_drvdata(pdev, ctrl);

	return cpr3_regulator_register(pdev, ctrl);
}

static int cpr3_mmss_regulator_remove(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_unregister(ctrl);
}

static struct platform_driver cpr3_mmss_regulator_driver = {
	.driver		= {
		.name		= "qcom,cpr3-mmss-regulator",
		.of_match_table	= cpr_regulator_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= cpr3_mmss_regulator_probe,
	.remove		= cpr3_mmss_regulator_remove,
	.suspend	= cpr3_mmss_regulator_suspend,
	.resume		= cpr3_mmss_regulator_resume,
};

static int cpr_regulator_init(void)
{
	return platform_driver_register(&cpr3_mmss_regulator_driver);
}

static void cpr_regulator_exit(void)
{
	platform_driver_unregister(&cpr3_mmss_regulator_driver);
}

MODULE_DESCRIPTION("CPR3 MMSS regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(cpr_regulator_init);
module_exit(cpr_regulator_exit);
