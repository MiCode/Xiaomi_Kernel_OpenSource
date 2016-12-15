/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/msm-ldo-regulator.h>

#include "cpr3-regulator.h"

#define SDM660_MMSS_FUSE_CORNERS	6

/**
 * struct cpr4_sdm660_mmss_fuses - MMSS specific fuse data for SDM660
 * @init_voltage:	Initial (i.e. open-loop) voltage fuse parameter value
 *			for each fuse corner (raw, not converted to a voltage)
 * @offset_voltage:	The closed-loop voltage margin adjustment fuse parameter
 *			value for each fuse corner (raw, not converted to a
 *			voltage)
 * @cpr_fusing_rev:	CPR fusing revision fuse parameter value
 * @ldo_enable:		The ldo enable fuse parameter for each fuse corner
 *			indicates that VDD_GFX can be configured to LDO mode in
 *			the corresponding fuse corner.
 * @ldo_cpr_cl_enable:	A fuse parameter indicates that GFX CPR can be
 *			configured to operate in closed-loop mode when VDD_GFX
 *			is configured for LDO sub-regulated mode.
 *
 * This struct holds the values for all of the fuses read from memory.
 */
struct cpr4_sdm660_mmss_fuses {
	u64	init_voltage[SDM660_MMSS_FUSE_CORNERS];
	u64	offset_voltage[SDM660_MMSS_FUSE_CORNERS];
	u64	cpr_fusing_rev;
	u64	ldo_enable[SDM660_MMSS_FUSE_CORNERS];
	u64	ldo_cpr_cl_enable;
};

/* Fuse combos 0 -  7 map to CPR fusing revision 0 - 7 */
#define CPR4_SDM660_MMSS_FUSE_COMBO_COUNT	8

/*
 * SDM660 MMSS fuse parameter locations:
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
sdm660_mmss_init_voltage_param[SDM660_MMSS_FUSE_CORNERS][2] = {
	{{65, 39, 43}, {} },
	{{65, 39, 43}, {} },
	{{65, 34, 38}, {} },
	{{65, 34, 38}, {} },
	{{65, 29, 33}, {} },
	{{65, 24, 28}, {} },
};

static const struct cpr3_fuse_param sdm660_cpr_fusing_rev_param[] = {
	{71, 34, 36},
	{},
};

static const struct cpr3_fuse_param
sdm660_mmss_offset_voltage_param[SDM660_MMSS_FUSE_CORNERS][2] = {
	{{} },
	{{} },
	{{} },
	{{65, 52, 55}, {} },
	{{65, 48, 51}, {} },
	{{65, 44, 47}, {} },
};

static const struct cpr3_fuse_param
sdm660_mmss_ldo_enable_param[SDM660_MMSS_FUSE_CORNERS][2] = {
	{{73, 62, 62}, {} },
	{{73, 61, 61}, {} },
	{{73, 60, 60}, {} },
	{{73, 59, 59}, {} },
	{{73, 58, 58}, {} },
	{{73, 57, 57}, {} },
};

static const struct cpr3_fuse_param sdm660_ldo_cpr_cl_enable_param[] = {
	{71, 38, 38},
	{},
};

/* Additional SDM660 specific data: */

/* Open loop voltage fuse reference voltages in microvolts */
static const int sdm660_mmss_fuse_ref_volt[SDM660_MMSS_FUSE_CORNERS] = {
	585000,
	645000,
	725000,
	790000,
	870000,
	925000,
};

#define SDM660_MMSS_FUSE_STEP_VOLT		10000
#define SDM660_MMSS_OFFSET_FUSE_STEP_VOLT	10000
#define SDM660_MMSS_VOLTAGE_FUSE_SIZE	5

#define SDM660_MMSS_CPR_SENSOR_COUNT		11

#define SDM660_MMSS_CPR_CLOCK_RATE		19200000

/**
 * cpr4_sdm660_mmss_read_fuse_data() - load MMSS specific fuse parameter
 *		values
 * @vreg:		Pointer to the CPR3 regulator
 *
 * This function allocates a cpr4_sdm660_mmss_fuses struct, fills it with
 * values read out of hardware fuses, and finally copies common fuse values
 * into the regulator struct.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_sdm660_mmss_read_fuse_data(struct cpr3_regulator *vreg)
{
	void __iomem *base = vreg->thread->ctrl->fuse_base;
	struct cpr4_sdm660_mmss_fuses *fuse;
	int i, rc;

	fuse = devm_kzalloc(vreg->thread->ctrl->dev, sizeof(*fuse), GFP_KERNEL);
	if (!fuse)
		return -ENOMEM;

	rc = cpr3_read_fuse_param(base, sdm660_cpr_fusing_rev_param,
			&fuse->cpr_fusing_rev);
	if (rc) {
		cpr3_err(vreg, "Unable to read CPR fusing revision fuse, rc=%d\n",
			rc);
		return rc;
	}
	cpr3_info(vreg, "CPR fusing revision = %llu\n", fuse->cpr_fusing_rev);

	rc = cpr3_read_fuse_param(base, sdm660_ldo_cpr_cl_enable_param,
			&fuse->ldo_cpr_cl_enable);
	if (rc) {
		cpr3_err(vreg, "Unable to read ldo cpr closed-loop enable fuse, rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < SDM660_MMSS_FUSE_CORNERS; i++) {
		rc = cpr3_read_fuse_param(base,
			sdm660_mmss_init_voltage_param[i],
			&fuse->init_voltage[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d initial voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
			sdm660_mmss_offset_voltage_param[i],
			&fuse->offset_voltage[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d offset voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
			sdm660_mmss_ldo_enable_param[i],
			&fuse->ldo_enable[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d ldo enable fuse, rc=%d\n",
				i, rc);
			return rc;
		}
	}

	vreg->fuse_combo = fuse->cpr_fusing_rev;
	if (vreg->fuse_combo >= CPR4_SDM660_MMSS_FUSE_COMBO_COUNT) {
		cpr3_err(vreg, "invalid CPR fuse combo = %d found, not in range 0 - %d\n",
			vreg->fuse_combo,
			CPR4_SDM660_MMSS_FUSE_COMBO_COUNT - 1);
		return -EINVAL;
	}

	vreg->cpr_rev_fuse	= fuse->cpr_fusing_rev;
	vreg->fuse_corner_count	= SDM660_MMSS_FUSE_CORNERS;
	vreg->platform_fuses	= fuse;

	return 0;
}

/**
 * cpr3_sdm660_mmss_calculate_open_loop_voltages() - calculate the open-loop
 *		voltage for each corner of a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_sdm660_mmss_calculate_open_loop_voltages(
			struct cpr3_regulator *vreg)
{
	struct cpr4_sdm660_mmss_fuses *fuse = vreg->platform_fuses;
	int i, rc = 0;
	const int *ref_volt;
	int *fuse_volt;

	fuse_volt = kcalloc(vreg->fuse_corner_count, sizeof(*fuse_volt),
				GFP_KERNEL);
	if (!fuse_volt)
		return -ENOMEM;

	ref_volt = sdm660_mmss_fuse_ref_volt;
	for (i = 0; i < vreg->fuse_corner_count; i++) {
		fuse_volt[i] = cpr3_convert_open_loop_voltage_fuse(ref_volt[i],
			SDM660_MMSS_FUSE_STEP_VOLT, fuse->init_voltage[i],
			SDM660_MMSS_VOLTAGE_FUSE_SIZE);
		cpr3_info(vreg, "fuse_corner[%d] open-loop=%7d uV\n",
			i, fuse_volt[i]);
	}

	rc = cpr3_adjust_fused_open_loop_voltages(vreg, fuse_volt);
	if (rc) {
		cpr3_err(vreg, "fused open-loop voltage adjustment failed, rc=%d\n",
			rc);
		goto done;
	}

	for (i = 1; i < vreg->fuse_corner_count; i++) {
		if (fuse_volt[i] < fuse_volt[i - 1]) {
			cpr3_debug(vreg, "fuse corner %d voltage=%d uV < fuse corner %d voltage=%d uV; overriding: fuse corner %d voltage=%d\n",
				i, fuse_volt[i], i - 1, fuse_volt[i - 1],
				i, fuse_volt[i - 1]);
			fuse_volt[i] = fuse_volt[i - 1];
		}
	}

	for (i = 0; i < vreg->corner_count; i++)
		vreg->corner[i].open_loop_volt
			= fuse_volt[vreg->corner[i].cpr_fuse_corner];

	cpr3_debug(vreg, "unadjusted per-corner open-loop voltages:\n");
	for (i = 0; i < vreg->corner_count; i++)
		cpr3_debug(vreg, "open-loop[%2d] = %d uV\n", i,
			vreg->corner[i].open_loop_volt);

	rc = cpr3_adjust_open_loop_voltages(vreg);
	if (rc)
		cpr3_err(vreg, "open-loop voltage adjustment failed, rc=%d\n",
			rc);

done:
	kfree(fuse_volt);
	return rc;
}

/**
 * cpr4_mmss_parse_ldo_mode_data() - Parse the LDO mode enable state for each
 *		corner of a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * This function considers 2 sets of data: one set from device node and other
 * set from fuses and applies set intersection to decide the final LDO mode
 * enable state of each corner. If the device node configuration is not
 * specified, then the function applies LDO mode disable for all corners.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_mmss_parse_ldo_mode_data(struct cpr3_regulator *vreg)
{
	struct cpr4_sdm660_mmss_fuses *fuse = vreg->platform_fuses;
	int i, rc = 0;
	u32 *ldo_allowed;
	char *prop_str = "qcom,cpr-corner-allow-ldo-mode";

	if (!of_find_property(vreg->of_node, prop_str, NULL)) {
		cpr3_debug(vreg, "%s property is missing. LDO mode is disabled for all corners\n",
			prop_str);
		return 0;
	}

	ldo_allowed = kcalloc(vreg->corner_count, sizeof(*ldo_allowed),
			GFP_KERNEL);
	if (!ldo_allowed)
		return -ENOMEM;

	rc = cpr3_parse_corner_array_property(vreg, prop_str, 1, ldo_allowed);
	if (rc) {
		cpr3_err(vreg, "%s read failed, rc=%d\n", prop_str, rc);
		goto done;
	}

	for (i = 0; i < vreg->corner_count; i++)
		vreg->corner[i].ldo_mode_allowed
			= (ldo_allowed[i] && fuse->ldo_enable[i]);

done:
	kfree(ldo_allowed);
	return rc;
}

/**
 * cpr4_mmss_parse_corner_operating_mode() - Parse the CPR closed-loop operation
 *		enable state for each corner of a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * This function ensures that closed-loop operation is enabled only for LDO
 * mode allowed corners.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_mmss_parse_corner_operating_mode(struct cpr3_regulator *vreg)
{
	struct cpr4_sdm660_mmss_fuses *fuse = vreg->platform_fuses;
	int i, rc = 0;
	u32 *use_closed_loop;
	char *prop_str = "qcom,cpr-corner-allow-closed-loop";

	if (!of_find_property(vreg->of_node, prop_str, NULL)) {
		cpr3_debug(vreg, "%s property is missing. Use open-loop for all corners\n",
			prop_str);
		for (i = 0; i < vreg->corner_count; i++)
			vreg->corner[i].use_open_loop = true;

		return 0;
	}

	use_closed_loop = kcalloc(vreg->corner_count, sizeof(*use_closed_loop),
				GFP_KERNEL);
	if (!use_closed_loop)
		return -ENOMEM;

	rc = cpr3_parse_corner_array_property(vreg, prop_str, 1,
			use_closed_loop);
	if (rc) {
		cpr3_err(vreg, "%s read failed, rc=%d\n", prop_str, rc);
		goto done;
	}

	for (i = 0; i < vreg->corner_count; i++)
		vreg->corner[i].use_open_loop
			= !(fuse->ldo_cpr_cl_enable && use_closed_loop[i]
				&& vreg->corner[i].ldo_mode_allowed);

done:
	kfree(use_closed_loop);
	return rc;
}

/**
 * cpr4_mmss_parse_corner_data() - parse MMSS corner data from device tree
 *		properties of the regulator's device node
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_mmss_parse_corner_data(struct cpr3_regulator *vreg)
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
 * cpr4_sdm660_mmss_adjust_target_quotients() - adjust the target quotients for
 *		each corner according to device tree values and fuse values
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_sdm660_mmss_adjust_target_quotients(struct cpr3_regulator *vreg)
{
	struct cpr4_sdm660_mmss_fuses *fuse = vreg->platform_fuses;
	const struct cpr3_fuse_param (*offset_param)[2];
	int *volt_offset;
	int i, fuse_len, rc = 0;

	volt_offset = kcalloc(vreg->fuse_corner_count, sizeof(*volt_offset),
				GFP_KERNEL);
	if (!volt_offset)
		return -ENOMEM;

	offset_param = sdm660_mmss_offset_voltage_param;
	for (i = 0; i < vreg->fuse_corner_count; i++) {
		fuse_len = offset_param[i][0].bit_end + 1
			   - offset_param[i][0].bit_start;
		volt_offset[i] = cpr3_convert_open_loop_voltage_fuse(
			0, SDM660_MMSS_OFFSET_FUSE_STEP_VOLT,
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
 * cpr4_mmss_print_settings() - print out MMSS CPR configuration settings into
 *		the kernel log for debugging purposes
 * @vreg:		Pointer to the CPR3 regulator
 */
static void cpr4_mmss_print_settings(struct cpr3_regulator *vreg)
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
 * cpr4_mmss_init_thread() - perform all steps necessary to initialize the
 *		configuration data for a CPR3 thread
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_mmss_init_thread(struct cpr3_thread *thread)
{
	struct cpr3_controller *ctrl = thread->ctrl;
	struct cpr3_regulator *vreg = &thread->vreg[0];
	int rc;

	rc = cpr3_parse_common_thread_data(thread);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR thread data from device tree, rc=%d\n",
			rc);
		return rc;
	}

	if (!of_find_property(ctrl->dev->of_node, "vdd-thread0-ldo-supply",
		NULL)) {
		cpr3_err(vreg, "ldo supply regulator is not specified\n");
		return -EINVAL;
	}

	vreg->ldo_regulator = devm_regulator_get(ctrl->dev, "vdd-thread0-ldo");
	if (IS_ERR(vreg->ldo_regulator)) {
		rc = PTR_ERR(vreg->ldo_regulator);
		if (rc != -EPROBE_DEFER)
			cpr3_err(vreg, "unable to request vdd-thread0-ldo regulator, rc=%d\n",
				rc);
		return rc;
	}

	vreg->ldo_mode_allowed = !of_property_read_bool(vreg->of_node,
							"qcom,ldo-disable");
	vreg->ldo_regulator_bypass = BHS_MODE;
	vreg->ldo_type = CPR3_LDO300;

	rc = cpr4_sdm660_mmss_read_fuse_data(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR fuse data, rc=%d\n", rc);
		return rc;
	}

	rc = cpr4_mmss_parse_corner_data(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR corner data from device tree, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr4_sdm660_mmss_adjust_target_quotients(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to adjust target quotients, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr4_sdm660_mmss_calculate_open_loop_voltages(vreg);
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

	rc = cpr4_mmss_parse_ldo_mode_data(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to parse ldo mode data, rc=%d\n", rc);
		return rc;
	}

	rc = cpr4_mmss_parse_corner_operating_mode(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to parse closed-loop operating mode data, rc=%d\n",
			rc);
		return rc;
	}

	cpr4_mmss_print_settings(vreg);

	return 0;
}

/**
 * cpr4_mmss_init_controller() - perform MMSS CPR4 controller specific
 *		initializations
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr4_mmss_init_controller(struct cpr3_controller *ctrl)
{
	int rc;

	rc = cpr3_parse_common_ctrl_data(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable to parse common controller data, rc=%d\n",
				rc);
		return rc;
	}

	ctrl->sensor_count = SDM660_MMSS_CPR_SENSOR_COUNT;

	/*
	 * MMSS only has one thread (0) so the zeroed array does not need
	 * further modification.
	 */
	ctrl->sensor_owner = devm_kcalloc(ctrl->dev, ctrl->sensor_count,
				sizeof(*ctrl->sensor_owner), GFP_KERNEL);
	if (!ctrl->sensor_owner)
		return -ENOMEM;

	ctrl->cpr_clock_rate = SDM660_MMSS_CPR_CLOCK_RATE;
	ctrl->ctrl_type = CPR_CTRL_TYPE_CPR4;
	ctrl->support_ldo300_vreg = true;

	/*
	 * Use fixed step quotient if specified otherwise use dynamic
	 * calculated per RO step quotient
	 */
	of_property_read_u32(ctrl->dev->of_node,
			     "qcom,cpr-step-quot-fixed",
			     &ctrl->step_quot_fixed);
	ctrl->use_dynamic_step_quot = !ctrl->step_quot_fixed;

	/* iface_clk is optional for sdm660 */
	ctrl->iface_clk = NULL;
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

static int cpr4_mmss_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
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

	rc = cpr4_mmss_init_controller(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "failed to initialize CPR controller parameters, rc=%d\n",
				rc);
		return rc;
	}

	rc = cpr4_mmss_init_thread(&ctrl->thread[0]);
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

	platform_set_drvdata(pdev, ctrl);

	return cpr3_regulator_register(pdev, ctrl);
}

static int cpr4_mmss_regulator_remove(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_unregister(ctrl);
}

static int cpr4_mmss_regulator_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_suspend(ctrl);
}

static int cpr4_mmss_regulator_resume(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_resume(ctrl);
}

/* Data corresponds to the SoC revision */
static const struct of_device_id cpr4_mmss_regulator_match_table[] = {
	{
		.compatible = "qcom,cpr4-sdm660-mmss-ldo-regulator",
		.data = (void *)NULL,
	},
};

static struct platform_driver cpr4_mmss_regulator_driver = {
	.driver		= {
		.name		= "qcom,cpr4-mmss-ldo-regulator",
		.of_match_table	= cpr4_mmss_regulator_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= cpr4_mmss_regulator_probe,
	.remove		= cpr4_mmss_regulator_remove,
	.suspend	= cpr4_mmss_regulator_suspend,
	.resume		= cpr4_mmss_regulator_resume,
};

static int cpr_regulator_init(void)
{
	return platform_driver_register(&cpr4_mmss_regulator_driver);
}

static void cpr_regulator_exit(void)
{
	platform_driver_unregister(&cpr4_mmss_regulator_driver);
}

MODULE_DESCRIPTION("CPR4 MMSS LDO regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(cpr_regulator_init);
module_exit(cpr_regulator_exit);
