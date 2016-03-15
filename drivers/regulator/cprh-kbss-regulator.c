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

#define MSMCOBALT_KBSS_FUSE_CORNERS	4

/**
 * struct cprh_msmcobalt_kbss_fuses - KBSS specific fuse data for MSMCOBALT
 * @ro_sel:		Ring oscillator select fuse parameter value for each
 *			fuse corner
 * @init_voltage:	Initial (i.e. open-loop) voltage fuse parameter value
 *			for each fuse corner (raw, not converted to a voltage)
 * @target_quot:	CPR target quotient fuse parameter value for each fuse
 *			corner
 * @quot_offset:	CPR target quotient offset fuse parameter value for each
 *			fuse corner (raw, not unpacked) used for target quotient
 *			interpolation
 * @speed_bin:		Application processor speed bin fuse parameter value for
 *			the given chip
 * @cpr_fusing_rev:	CPR fusing revision fuse parameter value
 *
 * This struct holds the values for all of the fuses read from memory.
 */
struct cprh_msmcobalt_kbss_fuses {
	u64	ro_sel[MSMCOBALT_KBSS_FUSE_CORNERS];
	u64	init_voltage[MSMCOBALT_KBSS_FUSE_CORNERS];
	u64	target_quot[MSMCOBALT_KBSS_FUSE_CORNERS];
	u64	quot_offset[MSMCOBALT_KBSS_FUSE_CORNERS];
	u64	speed_bin;
	u64	cpr_fusing_rev;
};

/*
 * Fuse combos 0 - 7 map to CPR fusing revision 0 - 7 with speed bin fuse = 0.
 */
#define CPRH_MSMCOBALT_KBSS_FUSE_COMBO_COUNT 8

/*
 * Constants which define the name of each fuse corner.
 */
enum cprh_msmcobalt_kbss_fuse_corner {
	CPRH_MSMCOBALT_KBSS_FUSE_CORNER_LOWSVS		= 0,
	CPRH_MSMCOBALT_KBSS_FUSE_CORNER_SVS		= 1,
	CPRH_MSMCOBALT_KBSS_FUSE_CORNER_NOM		= 2,
	CPRH_MSMCOBALT_KBSS_FUSE_CORNER_TURBO_L1	= 3,
};

static const char * const cprh_msmcobalt_kbss_fuse_corner_name[] = {
	[CPRH_MSMCOBALT_KBSS_FUSE_CORNER_LOWSVS]	= "LowSVS",
	[CPRH_MSMCOBALT_KBSS_FUSE_CORNER_SVS]		= "SVS",
	[CPRH_MSMCOBALT_KBSS_FUSE_CORNER_NOM]		= "NOM",
	[CPRH_MSMCOBALT_KBSS_FUSE_CORNER_TURBO_L1]	= "TURBO_L1",
};

/* KBSS cluster IDs */
#define MSMCOBALT_KBSS_POWER_CLUSTER_ID 0
#define MSMCOBALT_KBSS_PERFORMANCE_CLUSTER_ID 1

/* KBSS controller IDs */
#define MSMCOBALT_KBSS_MIN_CONTROLLER_ID 0
#define MSMCOBALT_KBSS_MAX_CONTROLLER_ID 1

/*
 * MSMCOBALT KBSS fuse parameter locations:
 *
 * Structs are organized with the following dimensions:
 *	Outer:  0 or 1 for power or performance cluster
 *	Middle: 0 to 3 for fuse corners from lowest to highest corner
 *	Inner:  large enough to hold the longest set of parameter segments which
 *		fully defines a fuse parameter, +1 (for NULL termination).
 *		Each segment corresponds to a contiguous group of bits from a
 *		single fuse row.  These segments are concatentated together in
 *		order to form the full fuse parameter value.  The segments for
 *		a given parameter may correspond to different fuse rows.
 *
 */
static const struct cpr3_fuse_param
msmcobalt_kbss_ro_sel_param[2][MSMCOBALT_KBSS_FUSE_CORNERS][2] = {
	[MSMCOBALT_KBSS_POWER_CLUSTER_ID] = {
		{{67, 12, 15}, {} },
		{{67,  8, 11}, {} },
		{{67,  4,  7}, {} },
		{{67,  0,  3}, {} },
	},
	[MSMCOBALT_KBSS_PERFORMANCE_CLUSTER_ID] = {
		{{69, 26, 29}, {} },
		{{69, 22, 25}, {} },
		{{69, 18, 21}, {} },
		{{69, 14, 17}, {} },
	},
};

static const struct cpr3_fuse_param
msmcobalt_kbss_init_voltage_param[2][MSMCOBALT_KBSS_FUSE_CORNERS][2] = {
	[MSMCOBALT_KBSS_POWER_CLUSTER_ID] = {
		{{67, 34, 39}, {} },
		{{67, 28, 33}, {} },
		{{67, 22, 27}, {} },
		{{67, 16, 21}, {} },
	},
	[MSMCOBALT_KBSS_PERFORMANCE_CLUSTER_ID] = {
		{{69, 48, 53}, {} },
		{{69, 42, 47}, {} },
		{{69, 36, 41}, {} },
		{{69, 30, 35}, {} },
	},
};

static const struct cpr3_fuse_param
msmcobalt_kbss_target_quot_param[2][MSMCOBALT_KBSS_FUSE_CORNERS][3] = {
	[MSMCOBALT_KBSS_POWER_CLUSTER_ID] = {
		{{68, 18, 29}, {} },
		{{68,  6, 17}, {} },
		{{67, 58, 63}, {68,  0,  5} },
		{{67, 46, 57}, {} },
	},
	[MSMCOBALT_KBSS_PERFORMANCE_CLUSTER_ID] = {
		{{70, 32, 43}, {} },
		{{70, 20, 31}, {} },
		{{70,  8, 19}, {} },
		{{69, 60, 63}, {70,  0,  7}, {} },
	},
};

static const struct cpr3_fuse_param
msmcobalt_kbss_quot_offset_param[2][MSMCOBALT_KBSS_FUSE_CORNERS][3] = {
	[MSMCOBALT_KBSS_POWER_CLUSTER_ID] = {
		{{} },
		{{68, 63, 63}, {69, 0, 5}, {} },
		{{68, 56, 62}, {} },
		{{68, 49, 55}, {} },
	},
	[MSMCOBALT_KBSS_PERFORMANCE_CLUSTER_ID] = {
		{{} },
		{{71, 13, 15}, {71, 21, 24}, {} },
		{{71,  6, 12}, {} },
		{{70, 63, 63}, {71,  0,  5}, {} },
	},
};

static const struct cpr3_fuse_param msmcobalt_cpr_fusing_rev_param[] = {
	{39, 51, 53},
	{},
};

static const struct cpr3_fuse_param msmcobalt_kbss_speed_bin_param[] = {
	{38, 29, 31},
	{},
};

/*
 * Open loop voltage fuse reference voltages in microvolts for MSMCOBALT v1
 */
static const int msmcobalt_kbss_fuse_ref_volt[MSMCOBALT_KBSS_FUSE_CORNERS] = {
	696000,
	768000,
	896000,
	1112000,
};

#define MSMCOBALT_KBSS_FUSE_STEP_VOLT		10000
#define MSMCOBALT_KBSS_VOLTAGE_FUSE_SIZE	6
#define MSMCOBALT_KBSS_QUOT_OFFSET_SCALE	5

#define MSMCOBALT_KBSS_POWER_CPR_SENSOR_COUNT	6
#define MSMCOBALT_KBSS_PERFORMANCE_CPR_SENSOR_COUNT	9

#define MSMCOBALT_KBSS_CPR_CLOCK_RATE		19200000

#define MSMCOBALT_KBSS_MAX_CORNER_BAND_COUNT	4
#define MSMCOBALT_KBSS_MAX_CORNER_COUNT		40

#define MSMCOBALT_KBSS_CPR_SDELTA_CORE_COUNT	4

#define MSMCOBALT_KBSS_MAX_TEMP_POINTS		3
#define MSMCOBALT_KBSS_POWER_TEMP_SENSOR_ID_START	1
#define MSMCOBALT_KBSS_POWER_TEMP_SENSOR_ID_END		5
#define MSMCOBALT_KBSS_PERFORMANCE_TEMP_SENSOR_ID_START	6
#define MSMCOBALT_KBSS_PERFORMANCE_TEMP_SENSOR_ID_END	11

/**
 * cprh_msmcobalt_kbss_read_fuse_data() - load KBSS specific fuse parameter values
 * @vreg:		Pointer to the CPR3 regulator
 *
 * This function allocates a cprh_msmcobalt_kbss_fuses struct, fills it with
 * values read out of hardware fuses, and finally copies common fuse values
 * into the CPR3 regulator struct.
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_msmcobalt_kbss_read_fuse_data(struct cpr3_regulator *vreg)
{
	void __iomem *base = vreg->thread->ctrl->fuse_base;
	struct cprh_msmcobalt_kbss_fuses *fuse;
	int i, id, rc;

	fuse = devm_kzalloc(vreg->thread->ctrl->dev, sizeof(*fuse), GFP_KERNEL);
	if (!fuse)
		return -ENOMEM;

	rc = cpr3_read_fuse_param(base, msmcobalt_kbss_speed_bin_param,
				&fuse->speed_bin);
	if (rc) {
		cpr3_err(vreg, "Unable to read speed bin fuse, rc=%d\n", rc);
		return rc;
	}
	cpr3_info(vreg, "speed bin = %llu\n", fuse->speed_bin);

	rc = cpr3_read_fuse_param(base, msmcobalt_cpr_fusing_rev_param,
				&fuse->cpr_fusing_rev);
	if (rc) {
		cpr3_err(vreg, "Unable to read CPR fusing revision fuse, rc=%d\n",
			rc);
		return rc;
	}
	cpr3_info(vreg, "CPR fusing revision = %llu\n", fuse->cpr_fusing_rev);

	id = vreg->thread->ctrl->ctrl_id;

	for (i = 0; i < MSMCOBALT_KBSS_FUSE_CORNERS; i++) {
		rc = cpr3_read_fuse_param(base,
				msmcobalt_kbss_init_voltage_param[id][i],
				&fuse->init_voltage[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d initial voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
				msmcobalt_kbss_target_quot_param[id][i],
				&fuse->target_quot[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d target quotient fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
				msmcobalt_kbss_ro_sel_param[id][i],
				&fuse->ro_sel[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d RO select fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
				msmcobalt_kbss_quot_offset_param[id][i],
				&fuse->quot_offset[i]);
		if (rc) {
			cpr3_err(vreg, "Unable to read fuse-corner %d quotient offset fuse, rc=%d\n",
				i, rc);
			return rc;
		}

	}

	vreg->fuse_combo = fuse->cpr_fusing_rev + 8 * fuse->speed_bin;
	if (vreg->fuse_combo >= CPRH_MSMCOBALT_KBSS_FUSE_COMBO_COUNT) {
		cpr3_err(vreg, "invalid CPR fuse combo = %d found\n",
			vreg->fuse_combo);
		return -EINVAL;
	}

	vreg->speed_bin_fuse	= fuse->speed_bin;
	vreg->cpr_rev_fuse	= fuse->cpr_fusing_rev;
	vreg->fuse_corner_count	= MSMCOBALT_KBSS_FUSE_CORNERS;
	vreg->platform_fuses	= fuse;

	return 0;
}

/**
 * cprh_kbss_parse_corner_data() - parse KBSS corner data from device tree
 *		properties of the CPR3 regulator's device node
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_parse_corner_data(struct cpr3_regulator *vreg)
{
	int rc;

	rc = cpr3_parse_common_corner_data(vreg);
	if (rc) {
		cpr3_err(vreg, "error reading corner data, rc=%d\n", rc);
		return rc;
	}

	/*
	 * A total of MSMCOBALT_KBSS_MAX_CORNER_COUNT - 1 corners
	 * may be specified in device tree as an additional corner
	 * must be allocated to correspond to the APM crossover voltage.
	 */
	if (vreg->corner_count > MSMCOBALT_KBSS_MAX_CORNER_COUNT - 1) {
		cpr3_err(vreg, "corner count %d exceeds supported maximum %d\n",
		 vreg->corner_count, MSMCOBALT_KBSS_MAX_CORNER_COUNT - 1);
		return -EINVAL;
	}

	return rc;
}

/**
 * cprh_msmcobalt_kbss_calculate_open_loop_voltages() - calculate the open-loop
 *		voltage for each corner of a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * If open-loop voltage interpolation is allowed in device tree, then this
 * function calculates the open-loop voltage for a given corner using linear
 * interpolation.  This interpolation is performed using the processor
 * frequencies of the lower and higher Fmax corners along with their fused
 * open-loop voltages.
 *
 * If open-loop voltage interpolation is not allowed, then this function uses
 * the Fmax fused open-loop voltage for all of the corners associated with a
 * given fuse corner.
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_msmcobalt_kbss_calculate_open_loop_voltages(
			struct cpr3_regulator *vreg)
{
	struct device_node *node = vreg->of_node;
	struct cprh_msmcobalt_kbss_fuses *fuse = vreg->platform_fuses;
	int i, j, rc = 0;
	bool allow_interpolation;
	u64 freq_low, volt_low, freq_high, volt_high;
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

	for (i = 0; i < vreg->fuse_corner_count; i++) {
		fuse_volt[i] = cpr3_convert_open_loop_voltage_fuse(
			msmcobalt_kbss_fuse_ref_volt[i],
			MSMCOBALT_KBSS_FUSE_STEP_VOLT, fuse->init_voltage[i],
			MSMCOBALT_KBSS_VOLTAGE_FUSE_SIZE);

		/* Log fused open-loop voltage values for debugging purposes. */
		cpr3_info(vreg, "fused %8s: open-loop=%7d uV\n",
			  cprh_msmcobalt_kbss_fuse_corner_name[i],
			  fuse_volt[i]);
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
			cpr3_info(vreg, "fuse corner %d voltage=%d uV < fuse corner %d voltage=%d uV; overriding: fuse corner %d voltage=%d\n",
				i, fuse_volt[i], i - 1, fuse_volt[i - 1],
				i, fuse_volt[i - 1]);
			fuse_volt[i] = fuse_volt[i - 1];
		}
	}

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
	for (i = 1; i < vreg->fuse_corner_count - 1; i++) {
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
 * cprh_kbss_parse_core_count_temp_adj_properties() - load device tree
 *		properties associated with per-corner-band and temperature
 *		voltage adjustments.
 * @vreg:	Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_parse_core_count_temp_adj_properties(
		struct cpr3_regulator *vreg)
{
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	struct device_node *node = vreg->of_node;
	u32 *temp, *combo_corner_bands, *speed_bin_corner_bands;
	int rc, i, len, temp_point_count;

	vreg->allow_core_count_adj = of_find_property(node,
					"qcom,corner-band-allow-core-count-adjustment",
					NULL);
	vreg->allow_temp_adj = of_find_property(node,
					"qcom,corner-band-allow-temp-adjustment",
					NULL);

	if (!vreg->allow_core_count_adj && !vreg->allow_temp_adj)
		return 0;

	combo_corner_bands = kcalloc(vreg->fuse_combos_supported,
				     sizeof(*combo_corner_bands),
				     GFP_KERNEL);
	if (!combo_corner_bands)
		return -ENOMEM;

	rc = of_property_read_u32_array(node, "qcom,cpr-corner-bands",
					combo_corner_bands,
					vreg->fuse_combos_supported);
	if (rc == -EOVERFLOW) {
		/* Single value case */
		rc = of_property_read_u32(node, "qcom,cpr-corner-bands",
					  combo_corner_bands);
		for (i = 1; i < vreg->fuse_combos_supported; i++)
			combo_corner_bands[i] = combo_corner_bands[0];
	}
	if (rc) {
		cpr3_err(vreg, "error reading property qcom,cpr-corner-bands, rc=%d\n",
			rc);
		kfree(combo_corner_bands);
		return rc;
	}

	vreg->fuse_combo_corner_band_offset = 0;
	vreg->fuse_combo_corner_band_sum = 0;
	for (i = 0; i < vreg->fuse_combos_supported; i++) {
		vreg->fuse_combo_corner_band_sum += combo_corner_bands[i];
		if (i < vreg->fuse_combo)
			vreg->fuse_combo_corner_band_offset +=
				combo_corner_bands[i];
	}

	vreg->corner_band_count = combo_corner_bands[vreg->fuse_combo];

	kfree(combo_corner_bands);

	if (vreg->corner_band_count <= 0 ||
	    vreg->corner_band_count > MSMCOBALT_KBSS_MAX_CORNER_BAND_COUNT ||
	    vreg->corner_band_count > vreg->corner_count) {
		cpr3_err(vreg, "invalid corner band count %d > %d (max) for %d corners\n",
			 vreg->corner_band_count,
			 MSMCOBALT_KBSS_MAX_CORNER_BAND_COUNT,
			 vreg->corner_count);
		return -EINVAL;
	}

	vreg->speed_bin_corner_band_offset = 0;
	vreg->speed_bin_corner_band_sum = 0;
	if (vreg->speed_bins_supported > 0) {
		speed_bin_corner_bands = kcalloc(vreg->speed_bins_supported,
					sizeof(*speed_bin_corner_bands),
					GFP_KERNEL);
		if (!speed_bin_corner_bands)
			return -ENOMEM;

		rc = of_property_read_u32_array(node,
						"qcom,cpr-speed-bin-corner-bands",
						speed_bin_corner_bands,
						vreg->speed_bins_supported);
		if (rc) {
			cpr3_err(vreg, "error reading property qcom,cpr-speed-bin-corner-bands, rc=%d\n",
				 rc);
			kfree(speed_bin_corner_bands);
			return rc;
		}

		for (i = 0; i < vreg->speed_bins_supported; i++) {
			vreg->speed_bin_corner_band_sum +=
				speed_bin_corner_bands[i];
			if (i < vreg->speed_bin_fuse)
				vreg->speed_bin_corner_band_offset +=
					speed_bin_corner_bands[i];
		}

		if (speed_bin_corner_bands[vreg->speed_bin_fuse]
		    != vreg->corner_band_count) {
			cpr3_err(vreg, "qcom,cpr-corner-bands and qcom,cpr-speed-bin-corner-bands conflict on number of corners bands: %d vs %u\n",
				vreg->corner_band_count,
				speed_bin_corner_bands[vreg->speed_bin_fuse]);
			kfree(speed_bin_corner_bands);
			return -EINVAL;
		}

		kfree(speed_bin_corner_bands);
	}

	vreg->corner_band = devm_kcalloc(ctrl->dev,
					 vreg->corner_band_count,
					 sizeof(*vreg->corner_band),
					 GFP_KERNEL);

	temp = kcalloc(vreg->corner_band_count, sizeof(*temp), GFP_KERNEL);

	if (!vreg->corner_band || !temp) {
		rc = -ENOMEM;
		goto free_temp;
	}

	rc = cpr3_parse_corner_band_array_property(vreg,
						   "qcom,cpr-corner-band-map",
						   1, temp);
	if (rc) {
		cpr3_err(vreg, "could not load corner band map, rc=%d\n",
			 rc);
		goto free_temp;
	}

	for (i = 1; i < vreg->corner_band_count; i++) {
		if (temp[i - 1] > temp[i]) {
			cpr3_err(vreg, "invalid corner band mapping: band %d corner %d, band %d corner %d\n",
				 i - 1, temp[i - 1],
				 i, temp[i]);
			rc = -EINVAL;
			goto free_temp;
		}
	}

	for (i = 0; i < vreg->corner_band_count; i++)
		vreg->corner_band[i].corner = temp[i] - CPR3_CORNER_OFFSET;

	if (!of_find_property(ctrl->dev->of_node,
			      "qcom,cpr-temp-point-map", &len)) {
		/*
		 * Temperature based adjustments are not defined. Single
		 * temperature band is still valid for per-online-core
		 * adjustments.
		 */
		ctrl->temp_band_count = 1;
		rc = 0;
		goto free_temp;
	}

	if (!vreg->allow_temp_adj) {
		rc = 0;
		goto free_temp;
	}

	temp_point_count = len / sizeof(u32);
	if (temp_point_count <= 0 || temp_point_count >
	    MSMCOBALT_KBSS_MAX_TEMP_POINTS) {
		cpr3_err(ctrl, "invalid number of temperature points %d > %d (max)\n",
			 temp_point_count, MSMCOBALT_KBSS_MAX_TEMP_POINTS);
		rc = -EINVAL;
		goto free_temp;
	}

	ctrl->temp_points = devm_kcalloc(ctrl->dev, temp_point_count,
					sizeof(*ctrl->temp_points), GFP_KERNEL);
	if (!ctrl->temp_points) {
		rc = -ENOMEM;
		goto free_temp;
	}
	rc = of_property_read_u32_array(ctrl->dev->of_node,
					"qcom,cpr-temp-point-map",
					ctrl->temp_points, temp_point_count);
	if (rc) {
		cpr3_err(ctrl, "error reading property qcom,cpr-temp-point-map, rc=%d\n",
			 rc);
		goto free_temp;
	}

	for (i = 0; i < temp_point_count; i++)
		cpr3_debug(ctrl, "Temperature Point %d=%d\n", i,
				   ctrl->temp_points[i]);

	/*
	 * If t1, t2, and t3 are the temperature points, then the temperature
	 * bands are: (-inf, t1], (t1, t2], (t2, t3], and (t3, inf).
	 */
	ctrl->temp_band_count = temp_point_count + 1;
	cpr3_debug(ctrl, "Number of temp bands=%d\n",
		   ctrl->temp_band_count);

	rc = of_property_read_u32(ctrl->dev->of_node,
				  "qcom,cpr-initial-temp-band",
				  &ctrl->initial_temp_band);
	if (rc) {
		cpr3_err(ctrl, "error reading qcom,cpr-initial-temp-band, rc=%d\n",
			rc);
		goto free_temp;
	}

	if (ctrl->initial_temp_band >= ctrl->temp_band_count) {
		cpr3_err(ctrl, "Initial temperature band value %d should be in range [0 - %d]\n",
			ctrl->initial_temp_band, ctrl->temp_band_count - 1);
		rc = -EINVAL;
		goto free_temp;
	}

	ctrl->temp_sensor_id_start = ctrl->ctrl_id ==
		MSMCOBALT_KBSS_POWER_CLUSTER_ID
		? MSMCOBALT_KBSS_POWER_TEMP_SENSOR_ID_START :
		MSMCOBALT_KBSS_PERFORMANCE_TEMP_SENSOR_ID_START;
	ctrl->temp_sensor_id_end = ctrl->ctrl_id ==
		MSMCOBALT_KBSS_POWER_CLUSTER_ID
		? MSMCOBALT_KBSS_PERFORMANCE_TEMP_SENSOR_ID_START :
		MSMCOBALT_KBSS_PERFORMANCE_TEMP_SENSOR_ID_END;
	ctrl->allow_temp_adj = true;

	return 0;

free_temp:
	kfree(temp);

	return rc;
}

/**
 * cprh_kbss_apm_threshold_as_corner() - introduce a corner whose floor, open-loop,
 *		and ceiling voltages correspond to the APM threshold voltage.
 * @vreg:		Pointer to the CPR3 regulator
 *
 * The APM corner is utilized as a crossover corner by OSM and CPRh
 * hardware to determine the correct APM supply selection for the
 * rest of the corners. This function must be called after all other
 * functions which load per-corner values.
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_apm_threshold_as_corner(struct cpr3_regulator *vreg)
{
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	struct cpr3_corner *corner;
	struct cprh_corner_band *corner_band;
	int i, threshold, apm_corner = 0;

	if (!ctrl->apm_threshold_volt) {
		/* APM voltage threshold corner not required. */
		return 0;
	}

	threshold = ctrl->apm_threshold_volt;
	vreg->corner_count++;

	for (i = vreg->corner_count - 1; i >= 1; i--) {
		corner = &vreg->corner[i];

		if (threshold >= vreg->corner[i - 1].open_loop_volt) {
			apm_corner = i;
			break;
		}

		memcpy(corner, &vreg->corner[i - 1], sizeof(*corner));
	}

	corner = &vreg->corner[apm_corner];
	corner->proc_freq = 0;
	corner->floor_volt = threshold;
	corner->ceiling_volt = threshold;
	corner->open_loop_volt = threshold;
	corner->use_open_loop = true;
	cpr3_info(vreg, "APM threshold corner=%d, open-loop=%d\n",
		  apm_corner, threshold);

	/*
	 * Update corner band mappings to account for the inserted
	 * APM crossover corner.
	 */
	for (i = 0; i < vreg->corner_band_count; i++) {
		corner_band = &vreg->corner_band[i];
		if (corner_band->corner >= apm_corner)
			corner_band->corner++;
	}

	return 0;
}

/**
 * cprh_kbss_adjust_voltages_for_apm() - adjust per-corner floor and ceiling
 *		voltages so that they do not overlap the APM threshold voltage.
 * @vreg:		Pointer to the CPR3 regulator
 *
 * The KBSS memory array power mux (APM) must be configured for a specific
 * supply based upon where the VDD voltage lies with respect to the APM
 * threshold voltage.  When using CPR hardware closed-loop, the voltage may vary
 * anywhere between the floor and ceiling voltage without software notification.
 * Therefore, it is required that the floor to ceiling range for every corner
 * not intersect the APM threshold voltage.  This function adjusts the floor to
 * ceiling range for each corner which violates this requirement.
 *
 * The following algorithm is applied in the case that
 * floor < threshold <= ceiling:
 *	if open_loop >= threshold - adj, then floor = threshold
 *	else ceiling = threshold - step
 * where adj = an adjustment factor to ensure sufficient voltage margin and
 * step = VDD output step size
 *
 * The open-loop voltage is also bounded by the new floor or ceiling value as
 * needed.
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_adjust_voltages_for_apm(struct cpr3_regulator *vreg)
{
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	struct cpr3_corner *corner;
	int i, adj, threshold, prev_ceiling, prev_floor, prev_open_loop;

	if (!ctrl->apm_threshold_volt) {
		/* APM not being used. */
		return 0;
	}

	ctrl->apm_threshold_volt = CPR3_ROUND(ctrl->apm_threshold_volt,
						ctrl->step_volt);
	ctrl->apm_adj_volt = CPR3_ROUND(ctrl->apm_adj_volt, ctrl->step_volt);

	threshold = ctrl->apm_threshold_volt;
	adj = ctrl->apm_adj_volt;

	for (i = 0; i < vreg->corner_count; i++) {
		corner = &vreg->corner[i];

		if (threshold <= corner->floor_volt
		    || threshold > corner->ceiling_volt)
			continue;

		prev_floor = corner->floor_volt;
		prev_ceiling = corner->ceiling_volt;
		prev_open_loop = corner->open_loop_volt;

		if (corner->open_loop_volt >= threshold - adj) {
			corner->floor_volt = threshold;
			if (corner->open_loop_volt < corner->floor_volt)
				corner->open_loop_volt = corner->floor_volt;
		} else {
			corner->ceiling_volt = threshold - ctrl->step_volt;
		}

		cpr3_debug(vreg, "APM threshold=%d, APM adj=%d changed corner %d voltages; prev: floor=%d, ceiling=%d, open-loop=%d; new: floor=%d, ceiling=%d, open-loop=%d\n",
			threshold, adj, i, prev_floor, prev_ceiling,
			prev_open_loop, corner->floor_volt,
			corner->ceiling_volt, corner->open_loop_volt);
	}

	return 0;
}

/**
 * cprh_msmcobalt_kbss_set_no_interpolation_quotients() - use the fused target
 *		quotient values for lower frequencies.
 * @vreg:		Pointer to the CPR3 regulator
 * @volt_adjust:	Pointer to array of per-corner closed-loop adjustment
 *			voltages
 * @volt_adjust_fuse:	Pointer to array of per-fuse-corner closed-loop
 *			adjustment voltages
 * @ro_scale:		Pointer to array of per-fuse-corner RO scaling factor
 *			values with units of QUOT/V
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_msmcobalt_kbss_set_no_interpolation_quotients(
			struct cpr3_regulator *vreg, int *volt_adjust,
			int *volt_adjust_fuse, int *ro_scale)
{
	struct cprh_msmcobalt_kbss_fuses *fuse = vreg->platform_fuses;
	u32 quot, ro;
	int quot_adjust;
	int i, fuse_corner;

	for (i = 0; i < vreg->corner_count; i++) {
		fuse_corner = vreg->corner[i].cpr_fuse_corner;
		quot = fuse->target_quot[fuse_corner];
		quot_adjust = cpr3_quot_adjustment(ro_scale[fuse_corner],
					   volt_adjust_fuse[fuse_corner] +
					   volt_adjust[i]);
		ro = fuse->ro_sel[fuse_corner];
		vreg->corner[i].target_quot[ro] = quot + quot_adjust;
		cpr3_debug(vreg, "corner=%d RO=%u target quot=%u\n",
			  i, ro, quot);

		if (quot_adjust)
			cpr3_debug(vreg, "adjusted corner %d RO%u target quot: %u --> %u (%d uV)\n",
				  i, ro, quot, vreg->corner[i].target_quot[ro],
				  volt_adjust_fuse[fuse_corner] +
				  volt_adjust[i]);
	}

	return 0;
}

/**
 * cprh_msmcobalt_kbss_calculate_target_quotients() - calculate the CPR target
 *		quotient for each corner of a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * If target quotient interpolation is allowed in device tree, then this
 * function calculates the target quotient for a given corner using linear
 * interpolation.  This interpolation is performed using the processor
 * frequencies of the lower and higher Fmax corners along with the fused
 * target quotient and quotient offset of the higher Fmax corner.
 *
 * If target quotient interpolation is not allowed, then this function uses
 * the Fmax fused target quotient for all of the corners associated with a
 * given fuse corner.
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_msmcobalt_kbss_calculate_target_quotients(
			struct cpr3_regulator *vreg)
{
	struct cprh_msmcobalt_kbss_fuses *fuse = vreg->platform_fuses;
	int rc;
	bool allow_interpolation;
	u64 freq_low, freq_high, prev_quot;
	u64 *quot_low;
	u64 *quot_high;
	u32 quot, ro;
	int i, j, fuse_corner, quot_adjust;
	int *fmax_corner;
	int *volt_adjust, *volt_adjust_fuse, *ro_scale;

	/* Log fused quotient values for debugging purposes. */
	cpr3_info(vreg, "fused   LowSVS: quot[%2llu]=%4llu\n",
		fuse->ro_sel[CPRH_MSMCOBALT_KBSS_FUSE_CORNER_LOWSVS],
		fuse->target_quot[CPRH_MSMCOBALT_KBSS_FUSE_CORNER_LOWSVS]);
	for (i = CPRH_MSMCOBALT_KBSS_FUSE_CORNER_SVS;
		i <= CPRH_MSMCOBALT_KBSS_FUSE_CORNER_TURBO_L1; i++)
		cpr3_info(vreg, "fused %8s: quot[%2llu]=%4llu, quot_offset[%2llu]=%4llu\n",
			cprh_msmcobalt_kbss_fuse_corner_name[i],
			fuse->ro_sel[i], fuse->target_quot[i],
			fuse->ro_sel[i], fuse->quot_offset[i] *
			MSMCOBALT_KBSS_QUOT_OFFSET_SCALE);

	allow_interpolation = of_property_read_bool(vreg->of_node,
					"qcom,allow-quotient-interpolation");

	volt_adjust = kcalloc(vreg->corner_count, sizeof(*volt_adjust),
					GFP_KERNEL);
	volt_adjust_fuse = kcalloc(vreg->fuse_corner_count,
					sizeof(*volt_adjust_fuse), GFP_KERNEL);
	ro_scale = kcalloc(vreg->fuse_corner_count, sizeof(*ro_scale),
					GFP_KERNEL);
	fmax_corner = kcalloc(vreg->fuse_corner_count, sizeof(*fmax_corner),
					GFP_KERNEL);
	quot_low = kcalloc(vreg->fuse_corner_count, sizeof(*quot_low),
					GFP_KERNEL);
	quot_high = kcalloc(vreg->fuse_corner_count, sizeof(*quot_high),
					GFP_KERNEL);
	if (!volt_adjust || !volt_adjust_fuse || !ro_scale ||
	    !fmax_corner || !quot_low || !quot_high) {
		rc = -ENOMEM;
		goto done;
	}

	rc = cpr3_parse_closed_loop_voltage_adjustments(vreg, &fuse->ro_sel[0],
				volt_adjust, volt_adjust_fuse, ro_scale);
	if (rc) {
		cpr3_err(vreg, "could not load closed-loop voltage adjustments, rc=%d\n",
			rc);
		goto done;
	}

	if (!allow_interpolation) {
		/* Use fused target quotients for lower frequencies. */
		return cprh_msmcobalt_kbss_set_no_interpolation_quotients(
				vreg, volt_adjust, volt_adjust_fuse, ro_scale);
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
	i = CPRH_MSMCOBALT_KBSS_FUSE_CORNER_LOWSVS;
	quot_adjust = cpr3_quot_adjustment(ro_scale[i], volt_adjust_fuse[i]);
	quot = fuse->target_quot[i] + quot_adjust;
	quot_high[i] = quot_low[i] = quot;
	ro = fuse->ro_sel[i];
	if (quot_adjust)
		cpr3_debug(vreg, "adjusted fuse corner %d RO%u target quot: %llu --> %u (%d uV)\n",
			i, ro, fuse->target_quot[i], quot, volt_adjust_fuse[i]);

	for (i = 0; i <= fmax_corner[CPRH_MSMCOBALT_KBSS_FUSE_CORNER_LOWSVS];
		i++)
		vreg->corner[i].target_quot[ro] = quot;

	for (i = CPRH_MSMCOBALT_KBSS_FUSE_CORNER_SVS;
	     i < vreg->fuse_corner_count; i++) {
		quot_high[i] = fuse->target_quot[i];
		if (fuse->ro_sel[i] == fuse->ro_sel[i - 1])
			quot_low[i] = quot_high[i - 1];
		else
			quot_low[i] = quot_high[i]
					- fuse->quot_offset[i]
					  * MSMCOBALT_KBSS_QUOT_OFFSET_SCALE;
		if (quot_high[i] < quot_low[i]) {
			cpr3_debug(vreg, "quot_high[%d]=%llu < quot_low[%d]=%llu; overriding: quot_high[%d]=%llu\n",
				i, quot_high[i], i, quot_low[i],
				i, quot_low[i]);
			quot_high[i] = quot_low[i];
		}
	}

	/* Perform per-fuse-corner target quotient adjustment */
	for (i = 1; i < vreg->fuse_corner_count; i++) {
		quot_adjust = cpr3_quot_adjustment(ro_scale[i],
						   volt_adjust_fuse[i]);
		if (quot_adjust) {
			prev_quot = quot_high[i];
			quot_high[i] += quot_adjust;
			cpr3_debug(vreg, "adjusted fuse corner %d RO%llu target quot: %llu --> %llu (%d uV)\n",
				i, fuse->ro_sel[i], prev_quot, quot_high[i],
				volt_adjust_fuse[i]);
		}

		if (fuse->ro_sel[i] == fuse->ro_sel[i - 1])
			quot_low[i] = quot_high[i - 1];
		else
			quot_low[i] += cpr3_quot_adjustment(ro_scale[i],
						    volt_adjust_fuse[i - 1]);

		if (quot_high[i] < quot_low[i]) {
			cpr3_debug(vreg, "quot_high[%d]=%llu < quot_low[%d]=%llu after adjustment; overriding: quot_high[%d]=%llu\n",
				i, quot_high[i], i, quot_low[i],
				i, quot_low[i]);
			quot_high[i] = quot_low[i];
		}
	}

	/* Interpolate voltages for the higher fuse corners. */
	for (i = 1; i < vreg->fuse_corner_count; i++) {
		freq_low = vreg->corner[fmax_corner[i - 1]].proc_freq;
		freq_high = vreg->corner[fmax_corner[i]].proc_freq;

		ro = fuse->ro_sel[i];
		for (j = fmax_corner[i - 1] + 1; j <= fmax_corner[i]; j++)
			vreg->corner[j].target_quot[ro] = cpr3_interpolate(
				freq_low, quot_low[i], freq_high, quot_high[i],
				vreg->corner[j].proc_freq);
	}

	/* Perform per-corner target quotient adjustment */
	for (i = 0; i < vreg->corner_count; i++) {
		fuse_corner = vreg->corner[i].cpr_fuse_corner;
		ro = fuse->ro_sel[fuse_corner];
		quot_adjust = cpr3_quot_adjustment(ro_scale[fuse_corner],
						   volt_adjust[i]);
		if (quot_adjust) {
			prev_quot = vreg->corner[i].target_quot[ro];
			vreg->corner[i].target_quot[ro] += quot_adjust;
			cpr3_debug(vreg, "adjusted corner %d RO%u target quot: %llu --> %u (%d uV)\n",
				i, ro, prev_quot,
				vreg->corner[i].target_quot[ro],
				volt_adjust[i]);
		}
	}

	/* Ensure that target quotients increase monotonically */
	for (i = 1; i < vreg->corner_count; i++) {
		ro = fuse->ro_sel[vreg->corner[i].cpr_fuse_corner];
		if (fuse->ro_sel[vreg->corner[i - 1].cpr_fuse_corner] == ro
		    && vreg->corner[i].target_quot[ro]
				< vreg->corner[i - 1].target_quot[ro]) {
			cpr3_debug(vreg, "adjusted corner %d RO%u target quot=%u < adjusted corner %d RO%u target quot=%u; overriding: corner %d RO%u target quot=%u\n",
				i, ro, vreg->corner[i].target_quot[ro],
				i - 1, ro, vreg->corner[i - 1].target_quot[ro],
				i, ro, vreg->corner[i - 1].target_quot[ro]);
			vreg->corner[i].target_quot[ro]
				= vreg->corner[i - 1].target_quot[ro];
		}
	}

done:
	kfree(volt_adjust);
	kfree(volt_adjust_fuse);
	kfree(ro_scale);
	kfree(fmax_corner);
	kfree(quot_low);
	kfree(quot_high);
	return rc;
}

/**
 * cprh_kbss_print_settings() - print out KBSS CPR configuration settings into
 *		the kernel log for debugging purposes
 * @vreg:		Pointer to the CPR3 regulator
 */
static void cprh_kbss_print_settings(struct cpr3_regulator *vreg)
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
 * cprh_kbss_init_thread() - perform steps necessary to initialize the
 *		configuration data for a CPR3 thread
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_init_thread(struct cpr3_thread *thread)
{
	int rc;

	rc = cpr3_parse_common_thread_data(thread);
	if (rc) {
		cpr3_err(thread->ctrl, "thread %u unable to read CPR thread data from device tree, rc=%d\n",
			thread->thread_id, rc);
		return rc;
	}

	return 0;
}

/**
 * cprh_kbss_init_regulator() - perform all steps necessary to initialize the
 *		configuration data for a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_init_regulator(struct cpr3_regulator *vreg)
{
	struct cprh_msmcobalt_kbss_fuses *fuse;
	int rc;

	rc = cprh_msmcobalt_kbss_read_fuse_data(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR fuse data, rc=%d\n", rc);
		return rc;
	}

	fuse = vreg->platform_fuses;

	rc = cprh_kbss_parse_corner_data(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to read CPR corner data from device tree, rc=%d\n",
			rc);
		return rc;
	}

	rc = cprh_msmcobalt_kbss_calculate_open_loop_voltages(vreg);
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

	rc = cprh_kbss_adjust_voltages_for_apm(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to adjust voltages for APM\n, rc=%d\n",
			rc);
		return rc;
	}

	cpr3_open_loop_voltage_as_ceiling(vreg);

	rc = cpr3_limit_floor_voltages(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to limit floor voltages, rc=%d\n", rc);
		return rc;
	}

	rc = cprh_msmcobalt_kbss_calculate_target_quotients(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to calculate target quotients, rc=%d\n",
			rc);
		return rc;
	}

	rc = cprh_kbss_parse_core_count_temp_adj_properties(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to parse core count and temperature adjustment properties, rc=%d\n",
			 rc);
		return rc;
	}

	rc = cpr4_parse_core_count_temp_voltage_adj(vreg, true);
	if (rc) {
		cpr3_err(vreg, "unable to parse temperature and core count voltage adjustments, rc=%d\n",
			 rc);
		return rc;
	}

	if (vreg->allow_core_count_adj && (vreg->max_core_count <= 0
				   || vreg->max_core_count >
				   MSMCOBALT_KBSS_CPR_SDELTA_CORE_COUNT)) {
		cpr3_err(vreg, "qcom,max-core-count has invalid value = %d\n",
			 vreg->max_core_count);
		return -EINVAL;
	}

	rc = cprh_kbss_apm_threshold_as_corner(vreg);
	if (rc) {
		cpr3_err(vreg, "unable to introduce APM voltage threshold corner\n, rc=%d\n",
			rc);
		return rc;
	}

	cprh_kbss_print_settings(vreg);

	return 0;
}

/**
 * cprh_kbss_init_controller() - perform KBSS CPRh controller specific
 *		initializations
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_init_controller(struct cpr3_controller *ctrl)
{
	int rc;

	rc = cpr3_parse_common_ctrl_data(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable to parse common controller data, rc=%d\n",
				rc);
		return rc;
	}

	rc = of_property_read_u32(ctrl->dev->of_node, "qcom,cpr-controller-id",
				  &ctrl->ctrl_id);
	if (rc) {
		cpr3_err(ctrl, "could not read DT property qcom,cpr-controller-id, rc=%d\n",
			rc);
		return rc;
	}

	if (ctrl->ctrl_id < MSMCOBALT_KBSS_MIN_CONTROLLER_ID ||
	    ctrl->ctrl_id > MSMCOBALT_KBSS_MAX_CONTROLLER_ID) {
		cpr3_err(ctrl, "invalid qcom,cpr-controller-id specified\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(ctrl->dev->of_node,
				  "qcom,cpr-down-error-step-limit",
				  &ctrl->down_error_step_limit);
	if (rc) {
		cpr3_err(ctrl, "error reading qcom,cpr-down-error-step-limit, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(ctrl->dev->of_node,
				  "qcom,cpr-up-error-step-limit",
				  &ctrl->up_error_step_limit);
	if (rc) {
		cpr3_err(ctrl, "error reading qcom,cpr-up-error-step-limit, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(ctrl->dev->of_node,
				  "qcom,voltage-base",
				  &ctrl->base_volt);
	if (rc) {
		cpr3_err(ctrl, "error reading property qcom,voltage-base, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(ctrl->dev->of_node,
			"qcom,cpr-up-down-delay-time",
			&ctrl->up_down_delay_time);
	if (rc) {
		cpr3_err(ctrl, "error reading property qcom,cpr-up-down-delay-time, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(ctrl->dev->of_node,
				  "qcom,apm-threshold-voltage",
				  &ctrl->apm_threshold_volt);
	if (rc)
		cpr3_debug(ctrl, "qcom,apm-threshold-voltage not specified\n");

	ctrl->saw_use_unit_mV = of_property_read_bool(ctrl->dev->of_node,
					"qcom,cpr-saw-use-unit-mV");

	of_property_read_u32(ctrl->dev->of_node,
			"qcom,cpr-voltage-settling-time",
			&ctrl->voltage_settling_time);

	of_property_read_u32(ctrl->dev->of_node,
			     "qcom,cpr-corner-switch-delay-time",
			     &ctrl->corner_switch_delay_time);

	ctrl->sensor_count = ctrl->ctrl_id == MSMCOBALT_KBSS_POWER_CLUSTER_ID ?
		MSMCOBALT_KBSS_POWER_CPR_SENSOR_COUNT :
		MSMCOBALT_KBSS_PERFORMANCE_CPR_SENSOR_COUNT;

	/*
	 * KBSS only has one thread (0) per controller so the zeroed
	 * array does not need further modification.
	 */
	ctrl->sensor_owner = devm_kcalloc(ctrl->dev, ctrl->sensor_count,
		sizeof(*ctrl->sensor_owner), GFP_KERNEL);
	if (!ctrl->sensor_owner)
		return -ENOMEM;

	ctrl->cpr_clock_rate = MSMCOBALT_KBSS_CPR_CLOCK_RATE;
	ctrl->ctrl_type = CPR_CTRL_TYPE_CPRH;
	ctrl->supports_hw_closed_loop = true;
	ctrl->use_hw_closed_loop = of_property_read_bool(ctrl->dev->of_node,
						 "qcom,cpr-hw-closed-loop");

	return 0;
}

/**
 * cprh_kbss_populate_opp_table() - populate an Operating Performance Point
 *		table with the frequencies associated with each corner.
 *		This table may be used to resolve corner to frequency to
 *		open-loop voltage mappings.
 * @pdev:		Pointer to the platform device
 *
 * Return: 0 on success, errno on failure
 */
static int cprh_kbss_populate_opp_table(struct cpr3_controller *ctrl)
{
	struct device *dev = ctrl->dev;
	struct cpr3_regulator *vreg = &ctrl->thread[0].vreg[0];
	struct cpr3_corner *corner;
	int rc, i;

	for (i = 0; i < vreg->corner_count; i++) {
		corner = &vreg->corner[i];
		rc = dev_pm_opp_add(dev, corner->proc_freq, i + 1);
		if (rc) {
			cpr3_err(ctrl, "could not add OPP for corner %d with frequency %u MHz, rc=%d\n",
				 i + 1, corner->proc_freq, rc);
			return rc;
		}
	}

	return 0;
}

static int cprh_kbss_regulator_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_suspend(ctrl);
}

static int cprh_kbss_regulator_resume(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_resume(ctrl);
}

static struct of_device_id cprh_regulator_match_table[] = {
	{ .compatible = "qcom,cprh-msmcobalt-kbss-regulator", },
	{}
};

static int cprh_kbss_regulator_probe(struct platform_device *pdev)
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

	rc = cprh_kbss_init_controller(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "failed to initialize CPR controller parameters, rc=%d\n",
				rc);
		return rc;
	}

	rc = cprh_kbss_init_thread(&ctrl->thread[0]);
	if (rc) {
		cpr3_err(ctrl, "thread initialization failed, rc=%d\n", rc);
		return rc;
	}

	rc = cprh_kbss_init_regulator(&ctrl->thread[0].vreg[0]);
	if (rc) {
		cpr3_err(&ctrl->thread[0].vreg[0], "regulator initialization failed, rc=%d\n",
			 rc);
		return rc;
	}

	platform_set_drvdata(pdev, ctrl);

	rc = cprh_kbss_populate_opp_table(ctrl);
	if (rc)
		panic("cprh-kbss-regulator OPP table initialization failed\n");

	return cpr3_regulator_register(pdev, ctrl);
}

static int cprh_kbss_regulator_remove(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_unregister(ctrl);
}

static struct platform_driver cprh_kbss_regulator_driver = {
	.driver		= {
		.name		= "qcom,cprh-kbss-regulator",
		.of_match_table	= cprh_regulator_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= cprh_kbss_regulator_probe,
	.remove		= cprh_kbss_regulator_remove,
	.suspend	= cprh_kbss_regulator_suspend,
	.resume		= cprh_kbss_regulator_resume,
};

static int cpr_regulator_init(void)
{
	return platform_driver_register(&cprh_kbss_regulator_driver);
}

static void cpr_regulator_exit(void)
{
	platform_driver_unregister(&cprh_kbss_regulator_driver);
}

MODULE_DESCRIPTION("CPRh KBSS regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(cpr_regulator_init);
module_exit(cpr_regulator_exit);
