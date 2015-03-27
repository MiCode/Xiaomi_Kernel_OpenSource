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

/*
 * This file contains utility functions to be used by platform specific CPR3
 * regulator drivers.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "cpr3-regulator.h"

#define BYTES_PER_FUSE_ROW		8
#define MAX_FUSE_ROW_BIT		63

/**
 * cpr3_get_thread_name() - loads the name of the thread specified in device
 *			    tree into thread->name
 * @thread:		Pointer to the CPR3 thread
 * @thread_node:	Device node pointer for the CPR3 thread device node
 *
 * Stores the name specified in device tree via regulator-name into
 * thread->name.
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_get_thread_name(struct cpr3_thread *thread,
			struct device_node *thread_node) {
	int rc;

	rc = of_property_read_string(thread_node, "regulator-name",
					&thread->name);
	if (rc)
		pr_err("Could not find name for CPR3 thread, rc=%d\n", rc);
	return rc;
}

/**
 * cpr3_allocate_threads() - allocate and initialize CPR3 threads for a given
 *			     controller based upon device tree data
 * @ctrl:		Pointer to the CPR3 controller
 * @min_thread_id:	Minimum allowed hardware thread ID for this controller
 * @max_thread_id:	Maximum allowed hardware thread ID for this controller
 *
 * This function allocates the ctrl->thread array based upon the number of
 * device tree thread subnodes.  It also initializes generic elements of each
 * thread struct such as thread_id, name, of_node, and ctrl.
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_allocate_threads(struct cpr3_controller *ctrl, u32 min_thread_id,
			u32 max_thread_id)
{
	struct device *dev = ctrl->dev;
	struct device_node *thread_node;
	int i, j, rc;

	ctrl->thread_count = 0;

	for_each_available_child_of_node(dev->of_node, thread_node) {
		ctrl->thread_count++;
	}

	ctrl->thread = devm_kzalloc(dev,
			sizeof(*ctrl->thread) * ctrl->thread_count, GFP_KERNEL);
	if (!ctrl->thread) {
		dev_err(dev, "could not allocate memory for CPR threads\n");
		return -ENOMEM;
	}

	i = 0;
	for_each_available_child_of_node(dev->of_node, thread_node) {
		ctrl->thread[i].of_node = thread_node;
		ctrl->thread[i].ctrl = ctrl;

		rc = cpr3_get_thread_name(&ctrl->thread[i], thread_node);
		if (rc) {
			dev_err(dev, "could not find thread name, rc=%d\n", rc);
			return rc;
		}

		rc = of_property_read_u32(thread_node, "qcom,cpr-thread-id",
					  &ctrl->thread[i].thread_id);
		if (rc) {
			dev_err(dev, "could not read DT property qcom,cpr-thread-id, rc=%d\n",
				rc);
			return rc;
		}

		if (ctrl->thread[i].thread_id < min_thread_id ||
				ctrl->thread[i].thread_id > max_thread_id) {
			dev_err(dev, "invalid thread id = %u; not within [%u, %u]\n",
				ctrl->thread[i].thread_id, min_thread_id,
				max_thread_id);
			return -EINVAL;
		}

		/* Verify that the thread ID is unique for all child nodes. */
		for (j = 0; j < i; j++) {
			if (ctrl->thread[j].thread_id
					== ctrl->thread[i].thread_id) {
				dev_err(dev, "duplicate thread id = %u found\n",
					ctrl->thread[i].thread_id);
				return -EINVAL;
			}
		}

		i++;
	}

	return 0;
}

/**
 * cpr3_map_fuse_base() - ioremap the base address of the fuse region
 * @ctrl:	Pointer to the CPR3 controller
 * @pdev:	Platform device pointer for the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_map_fuse_base(struct cpr3_controller *ctrl,
			struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fuse_base");
	if (!res || !res->start) {
		dev_err(&pdev->dev, "fuse base address is missing\n");
		return -ENXIO;
	}

	ctrl->fuse_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));

	return 0;
}

/**
 * cpr3_read_fuse_param() - reads a CPR3 fuse parameter out of eFuses
 * @fuse_base_addr:	Virtual memory address of the eFuse base address
 * @param:		Null terminated array of fuse param segments to read
 *			from
 * @param_value:	Output with value read from the eFuses
 *
 * This function reads from each of the parameter segments listed in the param
 * array and concatenates their values together.  Reading stops when an element
 * is reached which has all 0 struct values.  The total number of bits specified
 * for the fuse parameter across all segments must be less than or equal to 64.
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_read_fuse_param(void __iomem *fuse_base_addr,
		const struct cpr3_fuse_param *param, u64 *param_value)
{
	u64 fuse_val, val;
	int bits;
	int bits_total = 0;

	*param_value = 0;

	while (param->row || param->bit_start || param->bit_end) {
		if (param->bit_start > param->bit_end
		    || param->bit_end > MAX_FUSE_ROW_BIT) {
			pr_err("Invalid fuse parameter segment: row=%u, start=%u, end=%u\n",
				param->row, param->bit_start, param->bit_end);
			return -EINVAL;
		}

		bits = param->bit_end - param->bit_start + 1;
		if (bits_total + bits > 64) {
			pr_err("Invalid fuse parameter segments; total bits = %d\n",
				bits_total + bits);
			return -EINVAL;
		}

		fuse_val = readq_relaxed(fuse_base_addr
					 + param->row * BYTES_PER_FUSE_ROW);
		val = (fuse_val >> param->bit_start) & ((1ULL << bits) - 1);
		*param_value |= val << bits_total;
		bits_total += bits;

		param++;
	}

	return 0;
}

/**
 * cpr3_convert_open_loop_voltage_fuse() - converts an open loop voltage fuse
 *		value into an absolute voltage with units of microvolts
 * @ref_volt:		Reference voltage in microvolts
 * @step_volt:		The step size in microvolts of the fuse LSB
 * @fuse:		Open loop voltage fuse value
 * @fuse_len:		The bit length of the fuse value
 *
 * The MSB of the fuse parameter corresponds to a sign bit.  If it is set, then
 * the lower bits correspond to the number of steps to go down from the
 * reference voltage.  If it is not set, then the lower bits correspond to the
 * number of steps to go up from the reference voltage.
 */
int cpr3_convert_open_loop_voltage_fuse(int ref_volt, int step_volt, u32 fuse,
					int fuse_len)
{
	int sign, steps;

	sign = (fuse & (1 << (fuse_len - 1))) ? -1 : 1;
	steps = fuse & ((1 << (fuse_len - 1)) - 1);

	return ref_volt + sign * steps * step_volt;
}

/**
 * cpr3_interpolate() - performs linear interpolation
 * @x1		Lower known x value
 * @y1		Lower known y value
 * @x2		Upper known x value
 * @y2		Upper known y value
 * @x		Intermediate x value
 *
 * Returns y where (x, y) falls on the line between (x1, y1) and (x2, y2).
 * It is required that x1 < x2, y1 <= y2, and x1 <= x <= x2.  If these
 * conditions are not met, then y2 will be returned.
 */
u64 cpr3_interpolate(u64 x1, u64 y1, u64 x2, u64 y2, u64 x)
{
	u64 temp;

	if (x1 >= x2 || y1 > y2 || x1 > x || x > x2)
		return y2;

	temp = (x2 - x) * (y2 - y1);
	do_div(temp, (u32)(x2 - x1));

	return y2 - temp;
}

/**
 * cpr3_parse_array_property() - fill an array from a portion of the values
 *				specified for a device tree property
 * @thread:		Pointer to the CPR3 thread
 * @corner_count:	The number of corners
 * @corner_sum:		The sum of the corner counts across all fuse combos
 * @combo_offset:	The array offset for the selected fuse combo
 * @prop_name:		The name of the device tree property to read from
 * @out:		Output data array
 *
 * Two formats are supported for the device tree property:
 * 1. Length == corner_count
 *	(reading begins at index 0)
 * 2. Length == corner_sum
 *	(reading begins at index combo_offset)
 *
 * All other property lengths are treated as errors.
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_parse_array_property(struct cpr3_thread *thread,
		const char *prop_name, int corner_count, int corner_sum,
		int combo_offset, u32 *out)
{
	struct device_node *node = thread->of_node;
	int len = 0;
	int i, offset, rc;

	if (!of_find_property(node, prop_name, &len)) {
		cpr3_err(thread, "property %s is missing\n", prop_name);
		return -EINVAL;
	}

	if (len == corner_count * sizeof(u32)) {
		offset = 0;
	} else if (len == corner_sum * sizeof(u32)) {
		offset = combo_offset;
	} else {
		cpr3_err(thread, "property %s has invalid length=%d, should be %lu or %lu\n",
			prop_name, len, corner_count * sizeof(u32),
			corner_sum * sizeof(u32));
		return -EINVAL;
	}

	for (i = 0; i < corner_count; i++) {
		rc = of_property_read_u32_index(node, prop_name, offset + i,
						&out[i]);
		if (rc) {
			cpr3_err(thread, "error reading property %s, rc=%d\n",
				prop_name, rc);
			return rc;
		}
	}

	return 0;
}

/**
 * cpr3_parse_common_corner_data() - parse common CPR3 properties relating to
 *					the corners supported by a CPR3
 *					thread from device tree.
 * @thread:		Pointer to the CPR3 thread
 * @corner_sum:		Pointer which is output with the sum of the corner
 *			counts across all fuse combos
 * @combo_offset:	Pointer which is output with the array offset for the
 *			selected fuse combo
 *
 * This function reads, validates, and utilizes the following device tree
 * properties: qcom,cpr-fuse-corners, qcom,voltage-step, qcom,cpr-fuse-combos,
 * qcom,cpr-corners, qcom,cpr-voltage-ceiling, qcom,cpr-voltage-floor,
 * qcom,corner-frequencies, and qcom,cpr-corner-fmax-map.
 *
 * It initializes these thread elements: step_volt, corner_count, corner.  It
 * initializes these elements for each corner: ceiling_volt, floor_volt,
 * proc_freq, and cpr_fuse_corner.
 *
 * It requires that the following thread elements be initialized before being
 * called: fuse_corner_count, fuse_combo.
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_parse_common_corner_data(struct cpr3_thread *thread, int *corner_sum,
				int *combo_offset)
{
	struct device_node *node = thread->of_node;
	u32 max_fuse_combos, fuse_corners;
	u32 *combo_corners;
	u32 *temp;
	int i, j, rc;

	rc = of_property_read_u32(node, "qcom,cpr-fuse-corners", &fuse_corners);
	if (rc) {
		cpr3_err(thread, "error reading property qcom,cpr-fuse-corners, rc=%d\n",
			rc);
		return rc;
	}

	if (thread->fuse_corner_count != fuse_corners) {
		cpr3_err(thread, "device tree config supports %d fuse corners but the hardware has %d fuse corners\n",
			fuse_corners, thread->fuse_corner_count);
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom,voltage-step",
				&thread->step_volt);
	if (rc) {
		cpr3_err(thread, "error reading property qcom,voltage-step, rc=%d\n",
			rc);
		return rc;
	}
	if (thread->step_volt <= 0) {
		cpr3_err(thread, "qcom,voltage-step=%d is invalid\n",
			thread->step_volt);
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom,cpr-fuse-combos",
				&max_fuse_combos);
	if (rc) {
		cpr3_err(thread, "error reading property qcom,cpr-fuse-combos, rc=%d\n",
			rc);
		return rc;
	}

	/*
	 * Sanity check against arbitrarily large value to avoid excessive
	 * memory allocation.
	 */
	if (max_fuse_combos > 100 || max_fuse_combos == 0) {
		cpr3_err(thread, "qcom,cpr-fuse-combos is invalid: %u\n",
			max_fuse_combos);
		return -EINVAL;
	}

	if (thread->fuse_combo >= max_fuse_combos) {
		cpr3_err(thread, "device tree config supports fuse combos 0-%u but the hardware has config %d\n",
			max_fuse_combos - 1, thread->fuse_combo);
		return -EINVAL;
	}

	combo_corners = kzalloc(sizeof(*combo_corners) * max_fuse_combos,
				GFP_KERNEL);
	if (!combo_corners) {
		cpr3_err(thread, "could not allocate temp memory\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(node, "qcom,cpr-corners", combo_corners,
					max_fuse_combos);
	if (rc == -EOVERFLOW) {
		/* Single value case */
		rc = of_property_read_u32(node, "qcom,cpr-corners",
					combo_corners);
		for (i = 1; i < max_fuse_combos; i++)
			combo_corners[i] = combo_corners[0];
	}
	if (rc) {
		cpr3_err(thread, "error reading property qcom,cpr-corners, rc=%d\n",
			rc);
		kfree(combo_corners);
		return rc;
	}

	*combo_offset = 0;
	*corner_sum = 0;
	for (i = 0; i < max_fuse_combos; i++) {
		*corner_sum += combo_corners[i];
		if (i < thread->fuse_combo)
			*combo_offset += combo_corners[i];
	}

	thread->corner_count = combo_corners[thread->fuse_combo];

	kfree(combo_corners);

	thread->corner = devm_kzalloc(thread->ctrl->dev,
			sizeof(*thread->corner) * thread->corner_count,
			GFP_KERNEL);
	if (!thread->corner) {
		cpr3_err(thread, "could not allocate memory for corner array\n");
		return -ENOMEM;
	}

	temp = kzalloc(sizeof(*temp) * thread->corner_count, GFP_KERNEL);
	if (!temp) {
		cpr3_err(thread, "could not allocate temp memory\n");
		return -ENOMEM;
	}

	rc = cpr3_parse_array_property(thread, "qcom,cpr-voltage-ceiling",
			thread->corner_count, *corner_sum, *combo_offset, temp);
	if (rc)
		goto free_temp;
	for (i = 0; i < thread->corner_count; i++)
		thread->corner[i].ceiling_volt
			= CPR3_ROUND(temp[i], thread->step_volt);

	rc = cpr3_parse_array_property(thread, "qcom,cpr-voltage-floor",
			thread->corner_count, *corner_sum, *combo_offset, temp);
	if (rc)
		goto free_temp;
	for (i = 0; i < thread->corner_count; i++)
		thread->corner[i].floor_volt
			= CPR3_ROUND(temp[i], thread->step_volt);

	/* Validate ceiling and floor values */
	for (i = 0; i < thread->corner_count; i++) {
		if (thread->corner[i].floor_volt
		    > thread->corner[i].ceiling_volt) {
			cpr3_err(thread, "CPR floor[%d]=%d > ceiling[%d]=%d uV\n",
				i, thread->corner[i].floor_volt,
				i, thread->corner[i].ceiling_volt);
			rc = -EINVAL;
			goto free_temp;
		}
	}

	rc = cpr3_parse_array_property(thread, "qcom,corner-frequencies",
			thread->corner_count, *corner_sum, *combo_offset, temp);
	if (rc)
		goto free_temp;
	for (i = 0; i < thread->corner_count; i++)
		thread->corner[i].proc_freq = temp[i];

	/* Validate frequencies */
	for (i = 1; i < thread->corner_count; i++) {
		if (thread->corner[i].proc_freq
		    < thread->corner[i - 1].proc_freq) {
			cpr3_err(thread, "invalid frequency: freq[%d]=%u < freq[%d]=%u\n",
				i, thread->corner[i].proc_freq, i - 1,
				thread->corner[i - 1].proc_freq);
			rc = -EINVAL;
			goto free_temp;
		}
	}

	rc = cpr3_parse_array_property(thread, "qcom,cpr-corner-fmax-map",
		thread->fuse_corner_count,
		thread->fuse_corner_count * max_fuse_combos,
		thread->fuse_corner_count * thread->fuse_combo, temp);
	if (rc)
		goto free_temp;
	for (i = 0; i < thread->fuse_corner_count; i++) {
		if (temp[i] < CPR3_CORNER_OFFSET
		    || temp[i] > thread->corner_count + CPR3_CORNER_OFFSET) {
			cpr3_err(thread, "invalid corner value specified in qcom,cpr-corner-fmax-map: %u\n",
				temp[i]);
			rc = -EINVAL;
			goto free_temp;
		} else if (i > 0 && temp[i - 1] >= temp[i]) {
			cpr3_err(thread, "invalid corner %u less than or equal to previous corner %u\n",
				temp[i], temp[i - 1]);
			rc = -EINVAL;
			goto free_temp;
		}
	}
	if (temp[thread->fuse_corner_count - 1] != thread->corner_count) {
		cpr3_err(thread, "highest Fmax corner %u in qcom,cpr-corner-fmax-map does not match highest supported corner %d\n",
			temp[thread->fuse_corner_count - 1],
			thread->corner_count);
		rc = -EINVAL;
		goto free_temp;
	}
	for (i = 0; i < thread->corner_count; i++) {
		for (j = 0; j < thread->fuse_corner_count; j++) {
			if (i + CPR3_CORNER_OFFSET <= temp[j]) {
				thread->corner[i].cpr_fuse_corner = j;
				break;
			}
		}
	}

free_temp:
	kfree(temp);
	return rc;
}

/**
 * cpr3_limit_open_loop_voltages() - modify the open-loop voltage of each corner
 *				so that it fits within the floor to ceiling
 *				voltage range of the corner
 * @thread:		Pointer to the CPR3 thread
 *
 * This function clips the open-loop voltage for each corner so that it is
 * limited to the floor to ceiling range.  It also rounds each open-loop voltage
 * so that it corresponds to a set point available to the underlying regulator.
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_limit_open_loop_voltages(struct cpr3_thread *thread)
{
	int i, volt;

	cpr3_debug(thread, "open-loop voltages after trimming and rounding:\n");
	for (i = 0; i < thread->corner_count; i++) {
		volt = CPR3_ROUND(thread->corner[i].open_loop_volt,
					thread->step_volt);
		if (volt < thread->corner[i].floor_volt)
			volt = thread->corner[i].floor_volt;
		else if (volt > thread->corner[i].ceiling_volt)
			volt = thread->corner[i].ceiling_volt;
		thread->corner[i].open_loop_volt = volt;
		cpr3_debug(thread, "corner[%2d]: open-loop=%d uV\n", i, volt);
	}

	return 0;
}
