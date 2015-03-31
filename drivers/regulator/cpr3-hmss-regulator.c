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

#define MSM8996_HMSS_FUSE_CORNERS	5

/**
 * struct cpr3_msm8996_hmss_fuses - HMSS specific fuse data for MSM8996
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
 * @redundant_fusing:	Redundant fusing select fuse parameter value
 * @limitation:		CPR limitation select fuse parameter value
 *
 * This struct holds the values for all of the fuses read from memory.  The
 * values for ro_sel, init_voltage, target_quot, and quot_offset come from
 * either the primary or redundant fuse locations depending upon the value of
 * redundant_fusing.
 */
struct cpr3_msm8996_hmss_fuses {
	u64	ro_sel[MSM8996_HMSS_FUSE_CORNERS];
	u64	init_voltage[MSM8996_HMSS_FUSE_CORNERS];
	u64	target_quot[MSM8996_HMSS_FUSE_CORNERS];
	u64	quot_offset[MSM8996_HMSS_FUSE_CORNERS];
	u64	speed_bin;
	u64	cpr_fusing_rev;
	u64	redundant_fusing;
	u64	limitation;
};

/**
 * enum cpr3_msm8996_hmss_fuse_combo - fuse combinations supported by the HMSS
 *			CPR3 controller on MSM8996
 * %CPR3_MSM8996_HMSS_FUSE_COMBO_DEFAULT:	Initial default combination
 * %CPR3_MSM8996_HMSS_FUSE_COMBO_COUNT:		Defines the number of
 *						combinations supported
 *
 * This list will be expanded as new requirements are added.
 */
enum cpr3_msm8996_hmss_fuse_combo {
	CPR3_MSM8996_HMSS_FUSE_COMBO_DEFAULT = 0,
	CPR3_MSM8996_HMSS_FUSE_COMBO_COUNT
};

/*
 * Constants which define the name of each fuse corner.  Note that no actual
 * fuses are defined for LowSVS.  However, a mapping from corner to LowSVS
 * is required in order to perform target quotient interpolation properly.
 */
enum cpr3_msm8996_hmss_fuse_corner {
	CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS	= 0,
	CPR3_MSM8996_HMSS_FUSE_CORNER_LOWSVS	= 1,
	CPR3_MSM8996_HMSS_FUSE_CORNER_SVS	= 2,
	CPR3_MSM8996_HMSS_FUSE_CORNER_NOM	= 3,
	CPR3_MSM8996_HMSS_FUSE_CORNER_TURBO	= 4,
};

static const char * const cpr3_msm8996_hmss_fuse_corner_name[] = {
	[CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS]	= "MinSVS",
	[CPR3_MSM8996_HMSS_FUSE_CORNER_LOWSVS]	= "LowSVS",
	[CPR3_MSM8996_HMSS_FUSE_CORNER_SVS]	= "SVS",
	[CPR3_MSM8996_HMSS_FUSE_CORNER_NOM]	= "NOM",
	[CPR3_MSM8996_HMSS_FUSE_CORNER_TURBO]	= "TURBO",
};

/* CPR3 hardware thread IDs */
#define MSM8996_HMSS_POWER_CLUSTER_THREAD_ID		0
#define MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID	1

/*
 * MSM8996 HMSS fuse parameter locations:
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
 * Note that there are only physically 4 sets of fuse parameters which
 * correspond to the MinSVS, SVS, NOM, and TURBO fuse corners.  However, the SVS
 * quotient offset fuse is used to define the target quotient for the LowSVS
 * fuse corner.  In order to utilize LowSVS, it must be treated as if it were a
 * real fully defined fuse corner.  Thus, LowSVS fuse parameter locations are
 * specified.  These locations duplicate the SVS values in order to simplify
 * interpolation logic.
 */
static const struct cpr3_fuse_param
msm8996_hmss_ro_sel_param[2][MSM8996_HMSS_FUSE_CORNERS][2] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{66, 42, 45}, {} },
		{{66, 38, 41}, {} },
		{{66, 38, 41}, {} },
		{{66, 34, 37}, {} },
		{{66, 30, 33}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{64, 58, 61}, {} },
		{{64, 54, 57}, {} },
		{{64, 54, 57}, {} },
		{{64, 50, 53}, {} },
		{{64, 46, 49}, {} },
	},
};

static const struct cpr3_fuse_param
msm8996_hmss_init_voltage_param[2][MSM8996_HMSS_FUSE_CORNERS][3] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{67,  0,  5}, {} },
		{{66, 58, 63}, {} },
		{{66, 58, 63}, {} },
		{{66, 52, 57}, {} },
		{{66, 46, 51}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{65, 16, 21}, {} },
		{{65, 10, 15}, {} },
		{{65, 10, 15}, {} },
		{{65,  4,  9}, {} },
		{{64, 62, 63}, {65,  0,  3}, {} },
	},
};

static const struct cpr3_fuse_param
msm8996_hmss_target_quot_param[2][MSM8996_HMSS_FUSE_CORNERS][3] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{67, 42, 53}, {} },
		{{67, 30, 41}, {} },
		{{67, 30, 41}, {} },
		{{67, 18, 29}, {} },
		{{67,  6, 17}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{65, 58, 63}, {66,  0,  5}, {} },
		{{65, 46, 57}, {} },
		{{65, 46, 57}, {} },
		{{65, 34, 45}, {} },
		{{65, 22, 33}, {} },
	},
};

static const struct cpr3_fuse_param
msm8996_hmss_quot_offset_param[2][MSM8996_HMSS_FUSE_CORNERS][3] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{} },
		{{} },
		{{68,  6, 13}, {} },
		{{67, 62, 63}, {68, 0, 5}, {} },
		{{67, 54, 61}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{} },
		{{} },
		{{66, 22, 29}, {} },
		{{66, 14, 21}, {} },
		{{66,  6, 13}, {} },
	},
};

/*
 * This fuse is used to define if the redundant set of fuses should be used for
 * any particular feature.  CPR is one such feature.  The redundant CPR fuses
 * should be used if this fuse parameter has a value of 1.
 */
static const struct cpr3_fuse_param msm8996_redundant_fusing_param[] = {
	{73, 61, 63},
	{},
};
#define MSM8996_CPR_REDUNDANT_FUSING 1

static const struct cpr3_fuse_param
msm8996_hmss_redun_ro_sel_param[2][MSM8996_HMSS_FUSE_CORNERS][2] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{76, 36, 39}, {} },
		{{76, 32, 35}, {} },
		{{76, 32, 35}, {} },
		{{76, 28, 31}, {} },
		{{76, 24, 27}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{74, 52, 55}, {} },
		{{74, 48, 51}, {} },
		{{74, 48, 51}, {} },
		{{74, 44, 47}, {} },
		{{74, 40, 43}, {} },
	},
};

static const struct cpr3_fuse_param
msm8996_hmss_redun_init_voltage_param[2][MSM8996_HMSS_FUSE_CORNERS][3] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{76, 58, 63}, {} },
		{{76, 52, 57}, {} },
		{{76, 52, 57}, {} },
		{{76, 46, 51}, {} },
		{{76, 40, 45}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{75, 10, 15}, {} },
		{{75,  4,  9}, {} },
		{{75,  4,  9}, {} },
		{{74, 62, 63}, {75,  0,  3}, {} },
		{{74, 56, 61}, {} },
	},
};

static const struct cpr3_fuse_param
msm8996_hmss_redun_target_quot_param[2][MSM8996_HMSS_FUSE_CORNERS][2] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{77, 36, 47}, {} },
		{{77, 24, 35}, {} },
		{{77, 24, 35}, {} },
		{{77, 12, 23}, {} },
		{{77,  0, 11}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{75, 52, 63}, {} },
		{{75, 40, 51}, {} },
		{{75, 40, 51}, {} },
		{{75, 28, 39}, {} },
		{{75, 16, 27}, {} },
	},
};

static const struct cpr3_fuse_param
msm8996_hmss_redun_quot_offset_param[2][MSM8996_HMSS_FUSE_CORNERS][2] = {
	[MSM8996_HMSS_POWER_CLUSTER_THREAD_ID] = {
		{{} },
		{{} },
		{{68, 11, 18}, {} },
		{{77, 56, 63}, {} },
		{{77, 48, 55}, {} },
	},
	[MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID] = {
		{{} },
		{{} },
		{{76, 16, 23}, {} },
		{{76,  8, 15}, {} },
		{{76,  0,  7}, {} },
	},
};

static const struct cpr3_fuse_param msm8996_cpr_fusing_rev_param[] = {
	{39, 51, 53},
	{},
};

static const struct cpr3_fuse_param msm8996_hmss_speed_bin_param[] = {
	{38, 29, 31},
	{},
};

static const struct cpr3_fuse_param msm8996_cpr_limitation_param[] = {
	{41, 31, 32},
	{},
};

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
static const int msm8996_hmss_fuse_ref_volt[MSM8996_HMSS_FUSE_CORNERS] = {
	605000,
	745000, /* Place holder entry for LowSVS */
	745000,
	905000,
	1015000,
};

#define MSM8996_HMSS_FUSE_STEP_VOLT		10000
#define MSM8996_HMSS_VOLTAGE_FUSE_SIZE		6
#define MSM8996_HMSS_QUOT_OFFSET_SCALE		5

#define MSM8996_HMSS_CPR_SENSOR_COUNT		25
#define MSM8996_HMSS_THREAD0_SENSOR_MIN		0
#define MSM8996_HMSS_THREAD0_SENSOR_MAX		14
#define MSM8996_HMSS_THREAD1_SENSOR_MIN		15
#define MSM8996_HMSS_THREAD1_SENSOR_MAX		24

#define MSM8996_HMSS_CPR_CLOCK_RATE		19200000

/**
 * cpr3_msm8996_hmss_read_fuse_data() - load HMSS specific fuse parameter values
 * @thread:		Pointer to the CPR3 thread
 *
 * This function allocates a cpr3_msm8996_hmss_fuses struct, fills it with
 * values read out of hardware fuses, and finally copies common fuse values
 * into the thread struct.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8996_hmss_read_fuse_data(struct cpr3_thread *thread)
{
	void __iomem *base = thread->ctrl->fuse_base;
	struct cpr3_msm8996_hmss_fuses *fuse;
	bool redundant;
	int i, id, rc;

	fuse = devm_kzalloc(thread->ctrl->dev, sizeof(*fuse), GFP_KERNEL);
	if (!fuse) {
		cpr3_err(thread, "could not allocate memory for fuse data\n");
		return -ENOMEM;
	}

	rc = cpr3_read_fuse_param(base, msm8996_hmss_speed_bin_param,
				&fuse->speed_bin);
	if (rc) {
		cpr3_err(thread, "Unable to read speed bin fuse, rc=%d\n", rc);
		return rc;
	}
	cpr3_info(thread, "speed bin = %llu\n", fuse->speed_bin);

	rc = cpr3_read_fuse_param(base, msm8996_cpr_fusing_rev_param,
				&fuse->cpr_fusing_rev);
	if (rc) {
		cpr3_err(thread, "Unable to read CPR fusing revision fuse, rc=%d\n",
			rc);
		return rc;
	}
	cpr3_info(thread, "CPR fusing revision = %llu\n", fuse->cpr_fusing_rev);

	rc = cpr3_read_fuse_param(base, msm8996_redundant_fusing_param,
				&fuse->redundant_fusing);
	if (rc) {
		cpr3_err(thread, "Unable to read redundant fusing config fuse, rc=%d\n",
			rc);
		return rc;
	}

	redundant = (fuse->redundant_fusing == MSM8996_CPR_REDUNDANT_FUSING);
	cpr3_info(thread, "using redundant fuses = %c\n",
		redundant ? 'Y' : 'N');

	rc = cpr3_read_fuse_param(base, msm8996_cpr_limitation_param,
				&fuse->limitation);
	if (rc) {
		cpr3_err(thread, "Unable to read CPR limitation fuse, rc=%d\n",
			rc);
		return rc;
	}
	cpr3_info(thread, "CPR limitation = %s\n",
		fuse->limitation == MSM8996_CPR_LIMITATION_UNSUPPORTED
		? "unsupported chip" : fuse->limitation
			  == MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION
		? "CPR disabled and no interpolation" : "none");

	id = thread->thread_id;

	for (i = 0; i < MSM8996_HMSS_FUSE_CORNERS; i++) {
		rc = cpr3_read_fuse_param(base,
			redundant
			    ? msm8996_hmss_redun_init_voltage_param[id][i]
			    : msm8996_hmss_init_voltage_param[id][i],
			&fuse->init_voltage[i]);
		if (rc) {
			cpr3_err(thread, "Unable to read fuse-corner %d initial voltage fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
			redundant
			    ? msm8996_hmss_redun_target_quot_param[id][i]
			    : msm8996_hmss_target_quot_param[id][i],
			&fuse->target_quot[i]);
		if (rc) {
			cpr3_err(thread, "Unable to read fuse-corner %d target quotient fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
			redundant
			    ? msm8996_hmss_redun_ro_sel_param[id][i]
			    : msm8996_hmss_ro_sel_param[id][i],
			&fuse->ro_sel[i]);
		if (rc) {
			cpr3_err(thread, "Unable to read fuse-corner %d RO select fuse, rc=%d\n",
				i, rc);
			return rc;
		}

		rc = cpr3_read_fuse_param(base,
			redundant
			    ? msm8996_hmss_redun_quot_offset_param[id][i]
			    : msm8996_hmss_quot_offset_param[id][i],
			&fuse->quot_offset[i]);
		if (rc) {
			cpr3_err(thread, "Unable to read fuse-corner %d quotient offset fuse, rc=%d\n",
				i, rc);
			return rc;
		}
	}

	thread->speed_bin_fuse		= fuse->speed_bin;
	thread->cpr_rev_fuse		= fuse->cpr_fusing_rev;
	thread->fuse_corner_count	= MSM8996_HMSS_FUSE_CORNERS;
	thread->platform_fuses		= fuse;
	thread->fuse_combo		= CPR3_MSM8996_HMSS_FUSE_COMBO_DEFAULT;

	return 0;
}

/**
 * cpr3_hmss_parse_corner_data() - parse HMSS corner data from device tree
 *				properties of the thread's device node
 * @thread:		Pointer to the CPR3 thread
 * @corner_sum:		Pointer which is output with the sum of the corner
 *			counts across all fuse combos
 * @combo_offset:	Pointer which is output with the array offset for the
 *			selected fuse combo
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_hmss_parse_corner_data(struct cpr3_thread *thread,
			int *corner_sum, int *combo_offset)
{
	int rc;

	rc = cpr3_parse_common_corner_data(thread, corner_sum, combo_offset);
	if (rc) {
		cpr3_err(thread, "error reading corner data, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

/**
 * cpr3_msm8996_hmss_calculate_open_loop_voltages() - calculate the open-loop
 *		voltage for each corner of a CPR3 thread
 * @thread:		Pointer to the CPR3 thread
 * @corner_sum:		Sum of the corner counts across all fuse combos
 * @combo_offset:	Array offset for the selected fuse combo
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
static int cpr3_msm8996_hmss_calculate_open_loop_voltages(
			struct cpr3_thread *thread, int corner_sum,
			int combo_offset)
{
	struct device_node *node = thread->of_node;
	struct cpr3_msm8996_hmss_fuses *fuse = thread->platform_fuses;
	int rc = 0;
	bool allow_interpolation;
	u64 freq_low, volt_low, freq_high, volt_high;
	int i, j;
	int *fuse_volt;
	int *fmax_corner;

	fuse_volt = kzalloc(sizeof(*fuse_volt) * thread->fuse_corner_count,
				GFP_KERNEL);
	fmax_corner = kzalloc(sizeof(*fmax_corner) * thread->fuse_corner_count,
					GFP_KERNEL);
	if (!fuse_volt || !fmax_corner) {
		cpr3_err(thread, "unable to allocate temp memory\n");
		rc = -ENOMEM;
		goto done;
	}

	for (i = 0; i < thread->fuse_corner_count; i++) {
		fuse_volt[i] = cpr3_convert_open_loop_voltage_fuse(
			msm8996_hmss_fuse_ref_volt[i],
			MSM8996_HMSS_FUSE_STEP_VOLT, fuse->init_voltage[i],
			MSM8996_HMSS_VOLTAGE_FUSE_SIZE);

		/* Log fused open-loop voltage values for debugging purposes. */
		if (i != CPR3_MSM8996_HMSS_FUSE_CORNER_LOWSVS)
			cpr3_info(thread, "fused %6s: open-loop=%7d uV\n",
				cpr3_msm8996_hmss_fuse_corner_name[i],
				fuse_volt[i]);
	}

	rc = cpr3_adjust_fused_open_loop_voltages(thread, fuse_volt);
	if (rc) {
		cpr3_err(thread, "fused open-loop voltage adjustment failed, rc=%d\n",
			rc);
		goto done;
	}

	allow_interpolation = of_property_read_bool(node,
				"qcom,allow-voltage-interpolation");

	for (i = 1; i < thread->fuse_corner_count; i++) {
		if (fuse_volt[i] < fuse_volt[i - 1]) {
			cpr3_err(thread, "voltage fuse[%d]=%d < fuse[%d]=%d uV; interpolation not possible\n",
				i, fuse_volt[i], i - 1, fuse_volt[i - 1]);
			allow_interpolation = false;
		}
	}

	if (fuse->limitation == MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION)
		allow_interpolation = false;

	if (!allow_interpolation) {
		/* Use fused open-loop voltage for lower frequencies. */
		for (i = 0; i < thread->corner_count; i++)
			thread->corner[i].open_loop_volt
				= fuse_volt[thread->corner[i].cpr_fuse_corner];
		goto done;
	}

	/* Determine highest corner mapped to each fuse corner */
	j = thread->fuse_corner_count - 1;
	for (i = thread->corner_count - 1; i >= 0; i--) {
		if (thread->corner[i].cpr_fuse_corner == j) {
			fmax_corner[j] = i;
			j--;
		}
	}
	if (j >= 0) {
		cpr3_err(thread, "invalid fuse corner mapping\n");
		rc = -EINVAL;
		goto done;
	}

	/*
	 * Interpolation is not possible for corners mapped to the lowest fuse
	 * corner so use the fuse corner value directly.
	 */
	for (i = 0; i <= fmax_corner[0]; i++)
		thread->corner[i].open_loop_volt = fuse_volt[0];

	/*
	 * Corner LowSVS should be skipped for voltage interpolation
	 * since no fuse exists for it.  Instead, the lowest interpolation
	 * should be between MinSVS and SVS.
	 */
	for (i = CPR3_MSM8996_HMSS_FUSE_CORNER_LOWSVS;
	     i < thread->fuse_corner_count - 1; i++) {
		fmax_corner[i] = fmax_corner[i + 1];
		fuse_volt[i] = fuse_volt[i + 1];
	}

	/* Interpolate voltages for the higher fuse corners. */
	for (i = 1; i < thread->fuse_corner_count - 1; i++) {
		freq_low = thread->corner[fmax_corner[i - 1]].proc_freq;
		volt_low = fuse_volt[i - 1];
		freq_high = thread->corner[fmax_corner[i]].proc_freq;
		volt_high = fuse_volt[i];

		for (j = fmax_corner[i - 1] + 1; j <= fmax_corner[i]; j++)
			thread->corner[j].open_loop_volt = cpr3_interpolate(
				freq_low, volt_low, freq_high, volt_high,
				thread->corner[j].proc_freq);
	}

done:
	if (rc == 0) {
		cpr3_debug(thread, "unadjusted per-corner open-loop voltages:\n");
		for (i = 0; i < thread->corner_count; i++)
			cpr3_debug(thread, "open-loop[%2d] = %d uV\n", i,
				thread->corner[i].open_loop_volt);

		rc = cpr3_adjust_open_loop_voltages(thread, corner_sum,
			combo_offset);
		if (rc)
			cpr3_err(thread, "open-loop voltage adjustment failed, rc=%d\n",
				rc);
	}

	kfree(fuse_volt);
	kfree(fmax_corner);
	return rc;
}

/**
 * cpr3_hmss_adjust_voltages_for_apm() - adjust per-corner floor and ceiling
 *		voltages so that they do not overlap the APM threshold voltage
 * @thread:		Pointer to the CPR3 thread
 *
 * The HMSS memory array power mux (APM) must be configured for a specific
 * supply based upon where the VDD voltage lies with respect to the APM
 * threshold voltage.  When using CPR hardware closed-loop, the voltage may vary
 * anywhere between the floor and ceiling voltage without software notification.
 * Therefore, it is required that the floor to ceiling range for every corner
 * not intersect the APM threshold voltage.  This function adjusts the floor to
 * ceiling range for each corner which violates this requirement.
 *
 * The following algorithm is applied in the case that
 * floor < threshold < ceiling:
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
static int cpr3_hmss_adjust_voltages_for_apm(struct cpr3_thread *thread)
{
	struct cpr3_corner *corner;
	int i, adj, threshold, prev_ceiling, prev_floor, prev_open_loop;

	if (!thread->ctrl->apm || !thread->ctrl->apm_threshold_volt) {
		/* APM not being used. */
		return 0;
	}

	thread->ctrl->apm_threshold_volt
	      = CPR3_ROUND(thread->ctrl->apm_threshold_volt, thread->step_volt);
	thread->ctrl->apm_adj_volt
		= CPR3_ROUND(thread->ctrl->apm_adj_volt, thread->step_volt);

	threshold = thread->ctrl->apm_threshold_volt;
	adj = thread->ctrl->apm_adj_volt;

	for (i = 0; i < thread->corner_count; i++) {
		corner = &thread->corner[i];

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
			corner->ceiling_volt = threshold - thread->step_volt;
		}

		cpr3_debug(thread, "APM threshold=%d, APM adj=%d changed corner %d voltages; prev: floor=%d, ceiling=%d, open-loop=%d; new: floor=%d, ceiling=%d, open-loop=%d\n",
			threshold, adj, i, prev_floor, prev_ceiling,
			prev_open_loop, corner->floor_volt,
			corner->ceiling_volt, corner->open_loop_volt);
	}

	return 0;
}

/**
 * cpr3_hmss_parse_closed_loop_voltage_adjustments() - load per-fuse-corner and
 *		per-corner closed-loop adjustment values from device tree
 * @thread:		Pointer to the CPR3 thread
 * @corner_sum:		Sum of the corner counts across all fuse combos
 * @combo_offset:	Array offset for the selected fuse combo
 * @volt_adjust:	Pointer to array which will be filled with the
 *			per-corner closed-loop adjustment voltages
 * @volt_adjust_fuse:	Pointer to array which will be filled with the
 *			per-fuse-corner closed-loop adjustment voltages
 * @ro_scale:		Pointer to array which will be filled with the
 *			per-fuse-corner RO scaling factor values with units of
 *			QUOT/V
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_hmss_parse_closed_loop_voltage_adjustments(
			struct cpr3_thread *thread, int corner_sum,
			int combo_offset, int *volt_adjust,
			int *volt_adjust_fuse, int *ro_scale)
{
	struct cpr3_msm8996_hmss_fuses *fuse = thread->platform_fuses;
	int i, rc;
	u32 *ro_all_scale;

	if (!of_find_property(thread->of_node,
			"qcom,cpr-closed-loop-voltage-adjustment", NULL)
	    && !of_find_property(thread->of_node,
			"qcom,cpr-closed-loop-voltage-fuse-adjustment", NULL)) {
		/* No adjustment required. */
		return 0;
	} else if (!of_find_property(thread->of_node,
			"qcom,cpr-ro-scaling-factor", NULL)) {
		cpr3_err(thread, "qcom,cpr-ro-scaling-factor is required for closed-loop voltage adjustment, but is missing\n");
		return -EINVAL;
	}

	ro_all_scale = kzalloc(sizeof(*ro_all_scale)
				* thread->fuse_corner_count * CPR3_RO_COUNT,
				GFP_KERNEL);
	if (!ro_all_scale) {
		cpr3_err(thread, "memory allocation failed\n");
		return -ENOMEM;
	}

	rc = cpr3_parse_array_property(thread, "qcom,cpr-ro-scaling-factor",
		thread->fuse_corner_count * CPR3_RO_COUNT,
		thread->fuse_combos_supported * thread->fuse_corner_count
			* CPR3_RO_COUNT,
		thread->fuse_combo * thread->fuse_corner_count * CPR3_RO_COUNT,
		ro_all_scale);
	if (rc) {
		cpr3_err(thread, "could not load RO scaling factors, rc=%d\n",
			rc);
		goto done;
	}

	for (i = 0; i < thread->fuse_corner_count; i++)
		ro_scale[i] = ro_all_scale[i * CPR3_RO_COUNT + fuse->ro_sel[i]];

	if (of_find_property(thread->of_node,
			"qcom,cpr-closed-loop-voltage-fuse-adjustment", NULL)) {
		rc = cpr3_parse_array_property(thread,
			"qcom,cpr-closed-loop-voltage-fuse-adjustment",
			thread->fuse_corner_count,
			thread->fuse_combos_supported
				* thread->fuse_corner_count,
			thread->fuse_combo * thread->fuse_corner_count,
			volt_adjust_fuse);
		if (rc) {
			cpr3_err(thread, "could not load closed-loop fused voltage adjustments, rc=%d\n",
				rc);
			goto done;
		}
	}

	if (of_find_property(thread->of_node,
			"qcom,cpr-closed-loop-voltage-adjustment", NULL)) {
		rc = cpr3_parse_array_property(thread,
			"qcom,cpr-closed-loop-voltage-adjustment",
			thread->corner_count, corner_sum, combo_offset,
			volt_adjust);
		if (rc) {
			cpr3_err(thread, "could not load closed-loop voltage adjustments, rc=%d\n",
				rc);
			goto done;
		}
	}

done:
	kfree(ro_all_scale);
	return rc;
}

/**
 * cpr3_msm8996_hmss_set_no_interpolation_quotients() - use the fused target
 *		quotient values for lower frequencies.
 * @thread:		Pointer to the CPR3 thread
 * @volt_adjust:	Pointer to array of per-corner closed-loop adjustment
 *			voltages
 * @volt_adjust_fuse:	Pointer to array of per-fuse-corner closed-loop
 *			adjustment voltages
 * @ro_scale:		Pointer to array of per-fuse-corner RO scaling factor
 *			values with units of QUOT/V
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8996_hmss_set_no_interpolation_quotients(
			struct cpr3_thread *thread, int *volt_adjust,
			int *volt_adjust_fuse, int *ro_scale)
{
	struct cpr3_msm8996_hmss_fuses *fuse = thread->platform_fuses;
	u32 quot, ro;
	int quot_adjust;
	int i, fuse_corner;

	for (i = 0; i < thread->corner_count; i++) {
		fuse_corner = thread->corner[i].cpr_fuse_corner;
		quot = fuse->target_quot[fuse_corner];
		quot_adjust = cpr3_quot_adjustment(ro_scale[fuse_corner],
				volt_adjust_fuse[fuse_corner] + volt_adjust[i]);
		ro = fuse->ro_sel[fuse_corner];
		thread->corner[i].target_quot[ro] = quot + quot_adjust;
		if (quot_adjust)
			cpr3_info(thread, "adjusted corner %d RO%u target quot: %u --> %u (%d uV)\n",
				i, ro, quot, thread->corner[i].target_quot[ro],
				volt_adjust_fuse[fuse_corner] + volt_adjust[i]);
	}

	return 0;
}

/**
 * cpr3_msm8996_hmss_calculate_target_quotients() - calculate the CPR target
 *		quotient for each corner of a CPR3 thread
 * @thread:		Pointer to the CPR3 thread
 * @corner_sum:		Sum of the corner counts across all fuse combos
 * @combo_offset:	Array offset for the selected fuse combo
 *
 * If target quotient interpolation is allowed in both device tree and in
 * hardware fuses, then this function calculates the target quotient for a
 * given corner using linear interpolation.  This interpolation is performed
 * using the processor frequencies of the lower and higher Fmax corners along
 * with the fused target quotient and quotient offset of the higher Fmax corner.
 *
 * If target quotient interpolation is not allowed, then this function uses
 * the Fmax fused target quotient for all of the corners associated with a
 * given fuse corner.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_msm8996_hmss_calculate_target_quotients(
			struct cpr3_thread *thread, int corner_sum,
			int combo_offset)
{
	struct cpr3_msm8996_hmss_fuses *fuse = thread->platform_fuses;
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
	cpr3_info(thread, "fused MinSVS: quot[%2llu]=%4llu\n",
		fuse->ro_sel[CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS],
		fuse->target_quot[CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS]);
	for (i = CPR3_MSM8996_HMSS_FUSE_CORNER_SVS;
	     i <= CPR3_MSM8996_HMSS_FUSE_CORNER_TURBO; i++)
		cpr3_info(thread, "fused %6s: quot[%2llu]=%4llu, quot_offset[%2llu]=%4llu\n",
			cpr3_msm8996_hmss_fuse_corner_name[i],
			fuse->ro_sel[i], fuse->target_quot[i], fuse->ro_sel[i],
			fuse->quot_offset[i] * MSM8996_HMSS_QUOT_OFFSET_SCALE);

	allow_interpolation = of_property_read_bool(thread->of_node,
					"qcom,allow-quotient-interpolation");

	if (fuse->limitation == MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION)
		allow_interpolation = false;

	volt_adjust = kzalloc(sizeof(*volt_adjust) * thread->corner_count,
					GFP_KERNEL);
	volt_adjust_fuse = kzalloc(sizeof(*volt_adjust_fuse)
				       * thread->fuse_corner_count, GFP_KERNEL);
	ro_scale = kzalloc(sizeof(*ro_scale) * thread->fuse_corner_count,
					GFP_KERNEL);
	fmax_corner = kzalloc(sizeof(*fmax_corner) * thread->fuse_corner_count,
					GFP_KERNEL);
	quot_low = kzalloc(sizeof(*quot_low) * thread->fuse_corner_count,
					GFP_KERNEL);
	quot_high = kzalloc(sizeof(*quot_high) * thread->fuse_corner_count,
					GFP_KERNEL);
	if (!volt_adjust || !volt_adjust_fuse || !ro_scale ||
	    !fmax_corner || !quot_low || !quot_high) {
		cpr3_err(thread, "unable to allocate temp memory\n");
		rc = -ENOMEM;
		goto done;
	}

	rc = cpr3_hmss_parse_closed_loop_voltage_adjustments(thread, corner_sum,
		combo_offset, volt_adjust, volt_adjust_fuse, ro_scale);
	if (rc) {
		cpr3_err(thread, "could not load closed-loop voltage adjustments, rc=%d\n",
			rc);
		goto done;
	}

	if (!allow_interpolation) {
		/* Use fused target quotients for lower frequencies. */
		return cpr3_msm8996_hmss_set_no_interpolation_quotients(thread,
				volt_adjust, volt_adjust_fuse, ro_scale);
	}

	/* Determine highest corner mapped to each fuse corner */
	j = thread->fuse_corner_count - 1;
	for (i = thread->corner_count - 1; i >= 0; i--) {
		if (thread->corner[i].cpr_fuse_corner == j) {
			fmax_corner[j] = i;
			j--;
		}
	}
	if (j >= 0) {
		cpr3_err(thread, "invalid fuse corner mapping\n");
		rc = -EINVAL;
		goto done;
	}

	/*
	 * Interpolation is not possible for corners mapped to the lowest fuse
	 * corner so use the fuse corner value directly.
	 */
	i = CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS;
	quot_adjust = cpr3_quot_adjustment(ro_scale[i], volt_adjust_fuse[i]);
	quot = fuse->target_quot[i] + quot_adjust;
	ro = fuse->ro_sel[i];
	if (quot_adjust)
		cpr3_info(thread, "adjusted fuse corner %d RO%u target quot: %llu --> %u (%d uV)\n",
			i, ro, fuse->target_quot[i], quot, volt_adjust_fuse[i]);
	for (i = 0; i <= fmax_corner[CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS]; i++)
		thread->corner[i].target_quot[ro] = quot;

	for (i = CPR3_MSM8996_HMSS_FUSE_CORNER_SVS;
	     i < thread->fuse_corner_count; i++) {
		quot_high[i] = fuse->target_quot[i];
		quot_low[i] = quot_high[i]
		     - fuse->quot_offset[i] * MSM8996_HMSS_QUOT_OFFSET_SCALE;
		if (quot_low[i] > quot_high[i]) {
			cpr3_err(thread, "invalid quot[%d]=%llu and quot_offset[%d]=-%llu; interpolation not possible\n",
				i, quot_high[i], i,
				fuse->quot_offset[i]
					* MSM8996_HMSS_QUOT_OFFSET_SCALE);
			rc = cpr3_msm8996_hmss_set_no_interpolation_quotients(
				thread, volt_adjust, volt_adjust_fuse,
				ro_scale);
			goto done;
		}
	}

	if (fuse->ro_sel[CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS]
	    != fuse->ro_sel[CPR3_MSM8996_HMSS_FUSE_CORNER_SVS]) {
		cpr3_err(thread, "MinSVS RO=%llu is not the same as SVS RO=%llu; interpolation not possible\n",
			fuse->ro_sel[CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS],
			fuse->ro_sel[CPR3_MSM8996_HMSS_FUSE_CORNER_SVS]);
		rc = cpr3_msm8996_hmss_set_no_interpolation_quotients(thread,
			volt_adjust, volt_adjust_fuse, ro_scale);
		goto done;
	}

	/*
	 * The LowSVS target quotient is defined as:
	 *	(SVS target quotient) - (the unpacked SVS quotient offset)
	 * MinSVS, LowSVS, and SVS fuse corners all share the same RO so it is
	 * possible to interpolate between their target quotient values.
	 */
	i = CPR3_MSM8996_HMSS_FUSE_CORNER_LOWSVS;
	quot_high[i] = quot_low[CPR3_MSM8996_HMSS_FUSE_CORNER_SVS];
	quot_low[i] = fuse->target_quot[CPR3_MSM8996_HMSS_FUSE_CORNER_MINSVS];
	if (quot_low[i] > quot_high[i]) {
		cpr3_err(thread, "invalid quot_lowsvs=%llu and quot_minsvs=%llu; interpolation not possible\n",
			quot_high[i], quot_low[i]);
		rc = cpr3_msm8996_hmss_set_no_interpolation_quotients(thread,
			volt_adjust, volt_adjust_fuse, ro_scale);
		goto done;
	}

	/* Perform per-fuse-corner target quotient adjustment */
	for (i = 1; i < thread->fuse_corner_count; i++) {
		quot_adjust = cpr3_quot_adjustment(ro_scale[i],
						   volt_adjust_fuse[i]);
		if (quot_adjust) {
			prev_quot = quot_high[i];
			quot_high[i] += quot_adjust;
			cpr3_info(thread, "adjusted fuse corner %d RO%llu target quot: %llu --> %llu (%d uV)\n",
				i, fuse->ro_sel[i], prev_quot, quot_high[i],
				volt_adjust_fuse[i]);
		}

		quot_low[i] += cpr3_quot_adjustment(ro_scale[i],
						    volt_adjust_fuse[i - 1]);

		if (quot_low[i] > quot_high[i]) {
			cpr3_err(thread, "invalid quot_high[%d]=%llu and quot_low[%d]=%llu after adjustment; interpolation not possible\n",
				i, quot_high[i], i, quot_low[i]);
			rc = cpr3_msm8996_hmss_set_no_interpolation_quotients(
				thread, volt_adjust, volt_adjust_fuse,
				ro_scale);
			goto done;
		}
	}

	/* Interpolate voltages for the higher fuse corners. */
	for (i = 1; i < thread->fuse_corner_count; i++) {
		freq_low = thread->corner[fmax_corner[i - 1]].proc_freq;
		freq_high = thread->corner[fmax_corner[i]].proc_freq;

		ro = fuse->ro_sel[i];
		for (j = fmax_corner[i - 1] + 1; j <= fmax_corner[i]; j++)
			thread->corner[j].target_quot[ro] = cpr3_interpolate(
				freq_low, quot_low[i], freq_high, quot_high[i],
				thread->corner[j].proc_freq);
	}

	/* Perform per-corner target quotient adjustment */
	for (i = 0; i < thread->corner_count; i++) {
		fuse_corner = thread->corner[i].cpr_fuse_corner;
		ro = fuse->ro_sel[fuse_corner];
		quot_adjust = cpr3_quot_adjustment(ro_scale[fuse_corner],
						   volt_adjust[i]);
		if (quot_adjust) {
			prev_quot = thread->corner[i].target_quot[ro];
			thread->corner[i].target_quot[ro] += quot_adjust;
			cpr3_info(thread, "adjusted corner %d RO%u target quot: %llu --> %u (%d uV)\n",
				i, ro, prev_quot,
				thread->corner[i].target_quot[ro],
				volt_adjust[i]);
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
 * cpr3_hmss_print_settings() - print out HMSS CPR configuration settings into
 *		the kernel log for debugging purposes
 * @thread:		Pointer to the CPR3 thread
 */
static void cpr3_hmss_print_settings(struct cpr3_thread *thread)
{
	struct cpr3_corner *corner;
	int i;

	cpr3_debug(thread, "Corner: Frequency (Hz), Fuse Corner, Floor (uV), Open-Loop (uV), Ceiling (uV)\n");
	for (i = 0; i < thread->corner_count; i++) {
		corner = &thread->corner[i];
		cpr3_debug(thread, "%3d: %10u, %2d, %7d, %7d, %7d\n",
			i, corner->proc_freq, corner->cpr_fuse_corner,
			corner->floor_volt, corner->open_loop_volt,
			corner->ceiling_volt);
	}

	if (thread->ctrl->apm)
		cpr3_debug(thread->ctrl, "APM threshold = %d uV, APM adjust = %d uV\n",
			thread->ctrl->apm_threshold_volt,
			thread->ctrl->apm_adj_volt);
}

/**
 * cpr3_hmss_init_thread() - perform all steps necessary to initialize the
 *		configuration data for a CPR3 thread
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_hmss_init_thread(struct cpr3_thread *thread)
{
	struct cpr3_msm8996_hmss_fuses *fuse;
	int corner_sum = 0;
	int combo_offset = 0;
	int rc;

	rc = cpr3_msm8996_hmss_read_fuse_data(thread);
	if (rc) {
		cpr3_err(thread, "unable to read CPR fuse data, rc=%d\n", rc);
		return rc;
	}

	fuse = thread->platform_fuses;
	if (fuse->limitation == MSM8996_CPR_LIMITATION_UNSUPPORTED) {
		cpr3_err(thread, "this chip requires an unsupported voltage\n");
		return -EPERM;
	} else if (fuse->limitation
			== MSM8996_CPR_LIMITATION_NO_CPR_OR_INTERPOLATION) {
		thread->ctrl->cpr_allowed_hw = false;
	}

	rc = cpr3_parse_common_thread_data(thread);
	if (rc) {
		cpr3_err(thread, "unable to read CPR thread data from device tree, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_hmss_parse_corner_data(thread, &corner_sum, &combo_offset);
	if (rc) {
		cpr3_err(thread, "unable to read CPR corner data from device tree, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_msm8996_hmss_calculate_open_loop_voltages(thread, corner_sum,
			combo_offset);
	if (rc) {
		cpr3_err(thread, "unable to calculate open-loop voltages, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_limit_open_loop_voltages(thread);
	if (rc) {
		cpr3_err(thread, "unable to limit open-loop voltages, rc=%d\n",
			rc);
		return rc;
	}

	rc = cpr3_hmss_adjust_voltages_for_apm(thread);
	if (rc) {
		cpr3_err(thread, "unable to adjust voltages for APM\n, rc=%d\n",
			rc);
		return rc;
	}

	cpr3_open_loop_voltage_as_ceiling(thread);

	rc = cpr3_msm8996_hmss_calculate_target_quotients(thread, corner_sum,
			combo_offset);
	if (rc) {
		cpr3_err(thread, "unable to calculate target quotients, rc=%d\n",
			rc);
		return rc;
	}

	cpr3_hmss_print_settings(thread);

	return 0;
}

/**
 * cpr3_hmss_apm_init() - initialize HMSS APM data for a CPR3 controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * This function loads HMSS memory array power mux (APM) data from device tree
 * if it is present and requests a handle to the appropriate APM controller
 * device.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_hmss_apm_init(struct cpr3_controller *ctrl)
{
	struct device_node *node = ctrl->dev->of_node;
	int rc;

	if (!of_find_property(node, "qcom,apm-ctrl", NULL)) {
		/* No APM used */
		return 0;
	}

	ctrl->apm = msm_apm_ctrl_dev_get(ctrl->dev);
	if (IS_ERR(ctrl->apm)) {
		rc = PTR_ERR(ctrl->apm);
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "APM get failed, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,apm-threshold-voltage",
				&ctrl->apm_threshold_volt);
	if (rc) {
		cpr3_err(ctrl, "error reading qcom,apm-threshold-voltage, rc=%d\n",
			rc);
		return rc;
	}

	/* No error check since this is an optional property. */
	of_property_read_u32(node, "qcom,apm-hysteresis-voltage",
				&ctrl->apm_adj_volt);

	ctrl->apm_high_supply = MSM_APM_SUPPLY_APCC;
	ctrl->apm_low_supply = MSM_APM_SUPPLY_MX;

	return 0;
}

/**
 * cpr3_hmss_init_controller() - perform HMSS CPR3 controller specific
 *		initializations
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_hmss_init_controller(struct cpr3_controller *ctrl)
{
	int i, rc;

	rc = cpr3_parse_common_ctrl_data(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable to parse common controller data, rc=%d\n",
				rc);
		return rc;
	}

	ctrl->vdd_limit_regulator = devm_regulator_get(ctrl->dev, "vdd-limit");
	if (IS_ERR(ctrl->vdd_limit_regulator)) {
		rc = PTR_ERR(ctrl->vdd_limit_regulator);
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable to request vdd-supply regulator, rc=%d\n",
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

	/* No error check since this is an optional property. */
	of_property_read_u32(ctrl->dev->of_node, "qcom,cpr-clock-throttling",
			&ctrl->proc_clock_throttle);

	rc = cpr3_hmss_apm_init(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "unable to initialize APM settings, rc=%d\n",
				rc);
		return rc;
	}

	ctrl->sensor_count = MSM8996_HMSS_CPR_SENSOR_COUNT;

	ctrl->sensor_owner = devm_kzalloc(ctrl->dev,
		sizeof(*ctrl->sensor_owner) * ctrl->sensor_count, GFP_KERNEL);
	if (!ctrl->sensor_owner) {
		cpr3_err(ctrl, "memory allocation failed\n");
		return -ENOMEM;
	}

	/* Specify sensor ownership */
	for (i = MSM8996_HMSS_THREAD0_SENSOR_MIN;
	     i <= MSM8996_HMSS_THREAD0_SENSOR_MAX; i++)
		ctrl->sensor_owner[i] = 0;
	for (i = MSM8996_HMSS_THREAD1_SENSOR_MIN;
	     i <= MSM8996_HMSS_THREAD1_SENSOR_MAX; i++)
		ctrl->sensor_owner[i] = 1;

	ctrl->cpr_clock_rate = MSM8996_HMSS_CPR_CLOCK_RATE;
	ctrl->supports_hw_closed_loop = true;
	ctrl->use_hw_closed_loop = of_property_read_bool(ctrl->dev->of_node,
						"qcom,cpr-hw-closed-loop");

	return 0;
}

static int cpr3_hmss_regulator_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_suspend(ctrl);
}

static int cpr3_hmss_regulator_resume(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_resume(ctrl);
}

static struct of_device_id cpr_regulator_match_table[] = {
	{ .compatible = "qcom,cpr3-msm8996-hmss-regulator", },
	{}
};

static int cpr3_hmss_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cpr3_controller *ctrl;
	int i, rc;

	if (!dev->of_node) {
		dev_err(dev, "Device tree node is missing\n");
		return -EINVAL;
	}

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl) {
		dev_err(dev, "cpr3 controller memory allocation failed\n");
		return -ENOMEM;
	}

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

	rc = cpr3_allocate_threads(ctrl, MSM8996_HMSS_POWER_CLUSTER_THREAD_ID,
		MSM8996_HMSS_PERFORMANCE_CLUSTER_THREAD_ID);
	if (rc) {
		cpr3_err(ctrl, "failed to allocate CPR thread array, rc=%d\n",
			rc);
		return rc;
	}

	if (ctrl->thread_count < 1) {
		cpr3_err(ctrl, "thread nodes are missing\n");
		return -EINVAL;
	}

	rc = cpr3_hmss_init_controller(ctrl);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			cpr3_err(ctrl, "failed to initialize CPR controller parameters, rc=%d\n",
				rc);
		return rc;
	}

	for (i = 0; i < ctrl->thread_count; i++) {
		rc = cpr3_hmss_init_thread(&ctrl->thread[i]);
		if (rc) {
			cpr3_err(&ctrl->thread[i], "thread initialization failed, rc=%d\n",
				rc);
			return rc;
		}

		if (i > 0 && ctrl->thread[i].step_volt
					!= ctrl->thread[i - 1].step_volt) {
			pr_err("%s step_volt=%d != %s step_volt=%d\n",
				ctrl->thread[i].name, ctrl->thread[i].step_volt,
				ctrl->thread[i - 1].name,
				ctrl->thread[i - 1].step_volt);
			return -EINVAL;
		}
	}

	platform_set_drvdata(pdev, ctrl);

	return cpr3_regulator_register(pdev, ctrl);
}

static int cpr3_hmss_regulator_remove(struct platform_device *pdev)
{
	struct cpr3_controller *ctrl = platform_get_drvdata(pdev);

	return cpr3_regulator_unregister(ctrl);
}

static struct platform_driver cpr3_hmss_regulator_driver = {
	.driver		= {
		.name		= "qcom,cpr3-hmss-regulator",
		.of_match_table	= cpr_regulator_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= cpr3_hmss_regulator_probe,
	.remove		= cpr3_hmss_regulator_remove,
	.suspend	= cpr3_hmss_regulator_suspend,
	.resume		= cpr3_hmss_regulator_resume,
};

static int cpr_regulator_init(void)
{
	return platform_driver_register(&cpr3_hmss_regulator_driver);
}

static void cpr_regulator_exit(void)
{
	platform_driver_unregister(&cpr3_hmss_regulator_driver);
}

MODULE_DESCRIPTION("CPR3 HMSS regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(cpr_regulator_init);
module_exit(cpr_regulator_exit);
