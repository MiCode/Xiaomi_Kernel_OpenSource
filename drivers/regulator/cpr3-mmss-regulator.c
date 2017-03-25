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
 * @force_highest_corner:	Flag indicating that all corners must operate
 *			at the voltage of the highest corner.  This is
 *			applicable to MSM8998 only.
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
	u64	force_highest_corner;
};

/* Fuse combos 0 -  7 map to CPR fusing revision 0 - 7 */
#define CPR3_MSM8996_MMSS_FUSE_COMBO_COUNT	8

/*
 * Fuse combos 0 -  7 map to CPR fusing revision 0 - 7 with speed bin fuse = 0.
 * Fuse combos 8 - 15 map to CPR fusing revision 0 - 7 with speed bin fuse = 1.
 * Fuse combos 16 - 23 map to CPR fusing revision 0 - 7 with speed bin fuse = 2.
 */
#define CPR3_MSM8996PRO_MMSS_FUSE_COMBO_COUNT	24

/* Fuse combos 0 -  7 map to CPR fusing revision 0 - 7 */
#define CPR3_MSM8998_MMSS_FUSE_COMBO_COUNT	8

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

/* MSM8998 MMSS fuse parameter locations: */
static const struct cpr3_fuse_param
msm8998_mmss_init_voltage_param[MSM8996_MMSS_FUSE_CORNERS][2] = {
	{{65, 39, 43}, {} },
	{{65, 34, 38}, {} },
	{{65, 29, 33}, {} },
	{{65, 24, 28}, {} },
};

static const struct cpr3_fuse_param msm8998_cpr_fusing_rev_param[] = {
	{39, 48, 50},
	{},
};

static const struct cpr3_fuse_param msm8998_cpr_limitation_param[] = {
	{41, 46, 47},
	{},
};

static const struct cpr3_fuse_param
msm8998_mmss_aging_init_quot_diff_param[] = {
	{65, 60, 63},
	{66, 0, 3},
	{},
};

static const struct cpr3_fuse_param
msm8998_mmss_offset_voltage_param[MSM8996_MMSS_FUSE_CORNERS][2] = {
	{{65, 56, 59}, {} },
	{{65, 52, 55}, {} },
	{{65, 48, 51}, {} },
	{{65, 44, 47}, {} },
};

static const struct cpr3_fuse_param
msm8998_cpr_force_highest_corner_param[] = {
	{100, 45, 45},
	{},
};

#define MSM8996PRO_SOC_ID			4
#define MSM8998_V1_SOC_ID			5
#define MSM8998_V2_SOC_ID			6

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

static const int msm8998_v1_mmss_fuse_ref_volt[MSM8996_MMSS_FUSE_CORNERS] = {
	528000,
	656000,
	812000,
	932000,
};

static const int
msm8998_v1_rev0_mmss_fuse_ref_volt[MSM8996_MMSS_FUSE_CORNERS] = {
	632000,
	768000,
	896000,
	1032000,
};

static const int msm8998_v2_mmss_fuse_ref_volt[MSM8996_MMSS_FUSE_CORNERS] = {
	516000,
	628000,
	752000,
	924000,
};

static const int
msm8998_v2_rev0_mmss_fuse_ref_volt[MSM8996_MMSS_FUSE_CORNERS] = {
	616000,
	740000,
	828000,
	1024000,
};

#define MSM8996_MMSS_FUSE_STEP_VOLT		10000
#define MSM8996_MMSS_OFFSET_FUSE_STEP_VOLT	10000
#define MSM8996_MMSS_VOLTAGE_FUSE_SIZE		5
#define MSM8996_MMSS_MIN_VOLTAGE_FUSE_VAL	0x1F
#define MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SCALE	2
#define MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SIZE	6

#define MSM8996_MMSS_CPR_SENSOR_COUNT		35

#define MSM8996_MMSS_CPR_CLOCK_RATE		19200000

#define MSM8996_MMSS_AGING_SENSOR_ID		29
#define MSM8996_MMSS_AGING_BYPASS_MASK0		(GENMASK(23, 0))

/* Use scaled gate count (GCNT) for aging measurements */
#define MSM8996_MMSS_AGING_GCNT_SCALING_FACTOR	1500

#define MSM8998_MMSS_AGING_INIT_QUOT_DIFF_SCALE	1
#define MSM8998_MMSS_AGING_INIT_QUOT_DIFF_SIZE	8

#define MSM8998_MMSS_CPR_SENSOR_COUNT			35

#define MSM8998_MMSS_AGING_SENSOR_ID			29
#define MSM8998_MMSS_AGING_BYPASS_MASK0		(GENMASK(23, 0))

#define MSM8998_MMSS_MAX_TEMP_POINTS			3
#define MSM8998_MMSS_TEMP_SENSOR_ID_START		12
#define MSM8998_MMSS_TEMP_SENSOR_ID_END		13

/*
 * Some initial msm8998 parts cannot be operated at low voltages.  The
 * open-loop voltage fuses are reused to identify these parts so that software
 * can properly handle the limitation.  0xF means that the next higher fuse
 * corner should be used.  0xE means that the next higher fuse corner which
 * does not have a voltage limitation should be used.
 */
enum msm8998_cpr_partial_binning {
	MSM8998_CPR_PARTIAL_BINNING_NEXT_CORNER = 0xF,
	MSM8998_CPR_PARTIAL_BINNING_SAFE_CORNER = 0xE,
};

/*
 * The partial binning open-loop voltage fuse values only apply to the lowest
 * two fuse corners (0 and 1, i.e. MinSVS and SVS).
 */
#define MSM8998_CPR_PARTIAL_BINNING_MAX_FUSE_CORNER	1

static inline bool cpr3_ctrl_is_msm8998(const struct cpr3_controller *ctrl)
{
	return ctrl->soc_revision == MSM8998_V1_SOC_ID ||
		ctrl->soc_revision == MSM8998_V2_SOC_ID;
}

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

	rc = cpr3_read_fuse_param(base,
			cpr3_ctrl_is_msm8998(vreg->thread->ctrl)
				? msm8998_cpr_fusing_rev_param
				: msm8996_cpr_fusing_rev_param,
			&fuse->cpr_fusing_rev);
	if (rc) {
		cpr3_err(vreg, "Unable to read CPR fusing revision fuse, rc=%d\n",
			rc);
		return rc;
	}
	cpr3_info(vreg, "CPR fusing revision = %llu\n", fuse->cpr_fusing_rev);

	rc = cpr3_read_fuse_param(base,
			cpr3_ctrl_is_msm8998(vreg->thread->ctrl)
				? msm8998_cpr_limitation_param
				: msm8996_cpr_limitation_param,
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

	rc = cpr3_read_fuse_param(base,
			cpr3_ctrl_is_msm8998(vreg->thread->ctrl)
				? msm8998_mmss_aging_init_quot_diff_param
				: msm8996_mmss_aging_init_quot_diff_param,
			&fuse->aging_init_quot_diff);
	if (rc) {
		cpr3_err(vreg, "Unable to read aging initial quotient difference fuse, rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < MSM8996_MMSS_FUSE_CORNERS; i++) {
		rc = cpr3_read_fuse_param(base,
			cpr3_ctrl_is_msm8998(vreg->thread->ctrl)
				? msm8998_mmss_init_voltage_param[i]
				: msm8996_mmss_init_voltage_param[i],
			&fuse->init_voltage[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d initial voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
			cpr3_ctrl_is_msm8998(vreg->thread->ctrl)
				? msm8998_mmss_offset_voltage_param[i]
				: msm8996_mmss_offset_voltage_param[i],
			&fuse->offset_voltage[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d offset voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}
	}

	if (cpr3_ctrl_is_msm8998(vreg->thread->ctrl)) {
		rc = cpr3_read_fuse_param(base,
			msm8998_cpr_force_highest_corner_param,
			&fuse->force_highest_corner);
		if (rc) {
			cpr3_err(vreg, "Unable to read CPR force highest corner fuse, rc=%d\n",
				rc);
			return rc;
		}
		if (fuse->force_highest_corner)
			cpr3_info(vreg, "Fusing requires all operation at the highest corner\n");
	}

	if (cpr3_ctrl_is_msm8998(vreg->thread->ctrl)) {
		combo_max = CPR3_MSM8998_MMSS_FUSE_COMBO_COUNT;
		vreg->fuse_combo = fuse->cpr_fusing_rev;
	} else if (vreg->thread->ctrl->soc_revision == MSM8996PRO_SOC_ID) {
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
 * cpr3_msm8996_mmss_adjust_target_quotients() - adjust the target quotients
 *		for each corner according to device tree values and fuse values
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8996_mmss_adjust_target_quotients(
			struct cpr3_regulator *vreg)
{
	struct cpr3_msm8996_mmss_fuses *fuse = vreg->platform_fuses;
	const struct cpr3_fuse_param (*offset_param)[2];
	int *volt_offset;
	int i, fuse_len, rc = 0;

	volt_offset = kcalloc(vreg->fuse_corner_count, sizeof(*volt_offset),
				GFP_KERNEL);
	if (!volt_offset)
		return -ENOMEM;

	offset_param = cpr3_ctrl_is_msm8998(vreg->thread->ctrl)
			? msm8998_mmss_offset_voltage_param
			: msm8996_mmss_offset_voltage_param;
	for (i = 0; i < vreg->fuse_corner_count; i++) {
		fuse_len = offset_param[i][0].bit_end + 1
			   - offset_param[i][0].bit_start;
		volt_offset[i] = cpr3_convert_open_loop_voltage_fuse(
			0, MSM8996_MMSS_OFFSET_FUSE_STEP_VOLT,
			fuse->offset_voltage[i], fuse_len);
		if (volt_offset[i])
			cpr3_info(vreg, "fuse_corner[%d] offset=%7d uV\n",
				i, volt_offset[i]);
	}

	rc = cpr3_adjust_target_quotients(vreg, volt_offset);
	if (rc)
		cpr3_err(vreg, "adjust target quotients failed, rc=%d\n", rc);

	kfree(volt_offset);
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
	bool is_msm8998 = cpr3_ctrl_is_msm8998(vreg->thread->ctrl);
	int rc = 0;
	bool allow_interpolation;
	u64 freq_low, volt_low, freq_high, volt_high, volt_init;
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

	if (vreg->thread->ctrl->soc_revision == MSM8998_V2_SOC_ID
	    && fuse->cpr_fusing_rev == 0)
		ref_volt = msm8998_v2_rev0_mmss_fuse_ref_volt;
	else if (vreg->thread->ctrl->soc_revision == MSM8998_V2_SOC_ID)
		ref_volt = msm8998_v2_mmss_fuse_ref_volt;
	else if (vreg->thread->ctrl->soc_revision == MSM8998_V1_SOC_ID
	    && fuse->cpr_fusing_rev == 0)
		ref_volt = msm8998_v1_rev0_mmss_fuse_ref_volt;
	else if (vreg->thread->ctrl->soc_revision == MSM8998_V1_SOC_ID)
		ref_volt = msm8998_v1_mmss_fuse_ref_volt;
	else if (vreg->thread->ctrl->soc_revision == MSM8996PRO_SOC_ID)
		ref_volt = msm8996pro_mmss_fuse_ref_volt;
	else
		ref_volt = msm8996_mmss_fuse_ref_volt;

	for (i = 0; i < vreg->fuse_corner_count; i++) {
		volt_init = fuse->init_voltage[i];
		/*
		 * Handle partial binning on MSM8998 where the initial voltage
		 * fuse is reused as a flag for partial binning needs.  Set the
		 * open-loop voltage to the minimum possible value so that it
		 * does not result in higher fuse corners getting forced to
		 * higher open-loop voltages after monotonicity enforcement.
		 */
		if (is_msm8998 &&
		    (volt_init == MSM8998_CPR_PARTIAL_BINNING_NEXT_CORNER ||
		     volt_init == MSM8998_CPR_PARTIAL_BINNING_SAFE_CORNER) &&
		    i <= MSM8998_CPR_PARTIAL_BINNING_MAX_FUSE_CORNER)
			volt_init = MSM8996_MMSS_MIN_VOLTAGE_FUSE_VAL;

		fuse_volt[i] = cpr3_convert_open_loop_voltage_fuse(ref_volt[i],
			MSM8996_MMSS_FUSE_STEP_VOLT, volt_init,
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
 * cpr3_msm8998_partial_binning_override() - override the voltage and quotient
 *		settings for low corners based upon the special partial binning
 *		open-loop voltage fuse values
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Some parts are not able to operate at low voltages.  The partial binning
 * open-loop voltage fuse values specify if a given part has such limitations.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8998_partial_binning_override(struct cpr3_regulator *vreg)
{
	struct cpr3_msm8996_mmss_fuses *fuse = vreg->platform_fuses;
	u64 next = MSM8998_CPR_PARTIAL_BINNING_NEXT_CORNER;
	u64 safe = MSM8998_CPR_PARTIAL_BINNING_SAFE_CORNER;
	u32 proc_freq;
	struct cpr3_corner *corner;
	struct cpr3_corner *safe_corner;
	int i, j, low, high, safe_fuse_corner, max_fuse_corner;

	if (!cpr3_ctrl_is_msm8998(vreg->thread->ctrl))
		return 0;

	/* Handle the force highest corner fuse. */
	if (fuse->force_highest_corner) {
		cpr3_info(vreg, "overriding CPR parameters for corners 0 to %d with quotients and voltages of corner %d\n",
			vreg->corner_count - 2, vreg->corner_count - 1);
		corner = &vreg->corner[vreg->corner_count - 1];
		for (i = 0; i < vreg->corner_count - 1; i++) {
			proc_freq = vreg->corner[i].proc_freq;
			vreg->corner[i] = *corner;
			vreg->corner[i].proc_freq = proc_freq;
		}

		/*
		 * Return since the potential partial binning fuse values are
		 * superceded by the force highest corner fuse value.
		 */
		return 0;
	}

	/*
	 * Allow up to the max corner which can be fused with partial
	 * binning values.
	 */
	max_fuse_corner = min(MSM8998_CPR_PARTIAL_BINNING_MAX_FUSE_CORNER,
				vreg->fuse_corner_count - 2);

	for (i = 0; i <= max_fuse_corner; i++) {
		/* Determine which higher corners to override with (if any). */
		if (fuse->init_voltage[i] != next
		    && fuse->init_voltage[i] != safe)
			continue;

		for (j = i + 1; j <= max_fuse_corner; j++)
			if (fuse->init_voltage[j] != next
			    && fuse->init_voltage[j] != safe)
				break;
		safe_fuse_corner = j;

		j = fuse->init_voltage[i] == next ? i + 1 : safe_fuse_corner;

		low = i > 0 ? vreg->fuse_corner_map[i] : 0;
		high = vreg->fuse_corner_map[i + 1] - 1;

		cpr3_info(vreg, "overriding CPR parameters for corners %d to %d with quotients of corner %d and voltages of corner %d\n",
			low, high, vreg->fuse_corner_map[j],
			vreg->fuse_corner_map[safe_fuse_corner]);

		corner = &vreg->corner[vreg->fuse_corner_map[j]];
		safe_corner
		       = &vreg->corner[vreg->fuse_corner_map[safe_fuse_corner]];

		for (j = low; j <= high; j++) {
			proc_freq = vreg->corner[j].proc_freq;
			vreg->corner[j] = *corner;
			vreg->corner[j].proc_freq = proc_freq;

			vreg->corner[j].floor_volt
				= safe_corner->floor_volt;
			vreg->corner[j].ceiling_volt
				= safe_corner->ceiling_volt;
			vreg->corner[j].open_loop_volt
				= safe_corner->open_loop_volt;
			vreg->corner[j].abs_ceiling_volt
				= safe_corner->abs_ceiling_volt;
		}
	}

	return 0;
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

	ctrl->aging_sensor->ro_scale = aging_ro_scale;
	ctrl->aging_gcnt_scaling_factor
				= MSM8996_MMSS_AGING_GCNT_SCALING_FACTOR;

	if (cpr3_ctrl_is_msm8998(ctrl)) {
		ctrl->aging_sensor->sensor_id = MSM8998_MMSS_AGING_SENSOR_ID;
		ctrl->aging_sensor->bypass_mask[0]
					= MSM8998_MMSS_AGING_BYPASS_MASK0;
		ctrl->aging_sensor->init_quot_diff
			= cpr3_convert_open_loop_voltage_fuse(0,
				MSM8998_MMSS_AGING_INIT_QUOT_DIFF_SCALE,
				fuse->aging_init_quot_diff,
				MSM8998_MMSS_AGING_INIT_QUOT_DIFF_SIZE);
	} else {
		ctrl->aging_sensor->sensor_id = MSM8996_MMSS_AGING_SENSOR_ID;
		ctrl->aging_sensor->bypass_mask[0]
					= MSM8996_MMSS_AGING_BYPASS_MASK0;
		ctrl->aging_sensor->init_quot_diff
			= cpr3_convert_open_loop_voltage_fuse(0,
				MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SCALE,
				fuse->aging_init_quot_diff,
				MSM8996_MMSS_AGING_INIT_QUOT_DIFF_SIZE);
	}

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

	rc = cpr3_msm8996_mmss_adjust_target_quotients(vreg);
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

	if (cpr3_ctrl_is_msm8998(thread->ctrl)) {
		rc = cpr4_parse_core_count_temp_voltage_adj(vreg, false);
		if (rc) {
			cpr3_err(vreg, "unable to parse temperature based voltage adjustments, rc=%d\n",
				 rc);
			return rc;
		}
	}

	rc = cpr3_msm8998_partial_binning_override(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to override CPR parameters based on partial binning fuse values, rc=%d\n",
			rc);
		return rc;
	}

	cpr3_mmss_print_settings(vreg);

	return 0;
}

/**
 * cpr4_mmss_parse_temp_adj_properties() - parse temperature based
 *		adjustment properties from device tree
 * @ctrl:	Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_mmss_parse_temp_adj_properties(struct cpr3_controller *ctrl)
{
	struct device_node *of_node = ctrl->dev->of_node;
	int rc, len, temp_point_count;

	if (!of_find_property(of_node, "qcom,cpr-temp-point-map", &len))
		return 0;

	temp_point_count = len / sizeof(u32);
	if (temp_point_count <= 0
	    || temp_point_count > MSM8998_MMSS_MAX_TEMP_POINTS) {
		cpr3_err(ctrl, "invalid number of temperature points %d > %d (max)\n",
			 temp_point_count, MSM8998_MMSS_MAX_TEMP_POINTS);
		return -EINVAL;
	}

	ctrl->temp_points = devm_kcalloc(ctrl->dev, temp_point_count,
					sizeof(*ctrl->temp_points), GFP_KERNEL);
	if (!ctrl->temp_points)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "qcom,cpr-temp-point-map",
					ctrl->temp_points, temp_point_count);
	if (rc) {
		cpr3_err(ctrl, "error reading property qcom,cpr-temp-point-map, rc=%d\n",
			 rc);
		return rc;
	}

	/*
	 * If t1, t2, and t3 are the temperature points, then the temperature
	 * bands are: (-inf, t1], (t1, t2], (t2, t3], and (t3, inf).
	 */
	ctrl->temp_band_count = temp_point_count + 1;

	rc = of_property_read_u32(of_node, "qcom,cpr-initial-temp-band",
				  &ctrl->initial_temp_band);
	if (rc) {
		cpr3_err(ctrl, "error reading qcom,cpr-initial-temp-band, rc=%d\n",
			rc);
		return rc;
	}

	if (ctrl->initial_temp_band >= ctrl->temp_band_count) {
		cpr3_err(ctrl, "Initial temperature band value %d should be in range [0 - %d]\n",
			ctrl->initial_temp_band, ctrl->temp_band_count - 1);
		return -EINVAL;
	}

	ctrl->temp_sensor_id_start = MSM8998_MMSS_TEMP_SENSOR_ID_START;
	ctrl->temp_sensor_id_end = MSM8998_MMSS_TEMP_SENSOR_ID_END;
	ctrl->allow_temp_adj = true;

	return rc;
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

	if (cpr3_ctrl_is_msm8998(ctrl)) {
		rc = cpr4_mmss_parse_temp_adj_properties(ctrl);
		if (rc)
			return rc;
	}

	ctrl->sensor_count = cpr3_ctrl_is_msm8998(ctrl)
				? MSM8998_MMSS_CPR_SENSOR_COUNT
				: MSM8996_MMSS_CPR_SENSOR_COUNT;

	/*
	 * MMSS only has one thread (0) so the zeroed array does not need
	 * further modification.
	 */
	ctrl->sensor_owner = devm_kcalloc(ctrl->dev, ctrl->sensor_count,
				sizeof(*ctrl->sensor_owner), GFP_KERNEL);
	if (!ctrl->sensor_owner)
		return -ENOMEM;

	ctrl->cpr_clock_rate = MSM8996_MMSS_CPR_CLOCK_RATE;
	ctrl->ctrl_type = cpr3_ctrl_is_msm8998(ctrl)
				? CPR_CTRL_TYPE_CPR4 : CPR_CTRL_TYPE_CPR3;

	if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR4) {
		/*
		 * Use fixed step quotient if specified otherwise use dynamic
		 * calculated per RO step quotient
		 */
		of_property_read_u32(ctrl->dev->of_node,
				     "qcom,cpr-step-quot-fixed",
				     &ctrl->step_quot_fixed);
		ctrl->use_dynamic_step_quot = !ctrl->step_quot_fixed;
	}

	ctrl->iface_clk = devm_clk_get(ctrl->dev, "iface_clk");
	if (IS_ERR(ctrl->iface_clk)) {
		rc = PTR_ERR(ctrl->iface_clk);
		if (cpr3_ctrl_is_msm8998(ctrl)) {
			/* iface_clk is optional for msm8998 */
			ctrl->iface_clk = NULL;
		} else if (rc == -EPROBE_DEFER) {
			return rc;
		} else {
			cpr3_err(ctrl, "unable to request interface clock, rc=%d\n",
				rc);
			return rc;
		}
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
static const struct of_device_id cpr_regulator_match_table[] = {
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
	{
		.compatible = "qcom,cpr4-msm8998-v1-mmss-regulator",
		.data = (void *)(uintptr_t)MSM8998_V1_SOC_ID,
	},
	{
		.compatible = "qcom,cpr4-msm8998-v2-mmss-regulator",
		.data = (void *)(uintptr_t)MSM8998_V2_SOC_ID,
	},
	{
		.compatible = "qcom,cpr4-msm8998-mmss-regulator",
		.data = (void *)(uintptr_t)MSM8998_V2_SOC_ID,
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
