/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

#include <linux/of.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_cx_ipeak.h"

static char supported_clk_info[256];
static char debugfs_dir_name[64];

static int cam_soc_util_get_clk_level(struct cam_hw_soc_info *soc_info,
	int32_t src_clk_idx, int64_t clk_rate)
{
	int i;
	long clk_rate_round;

	clk_rate_round = clk_round_rate(soc_info->clk[src_clk_idx], clk_rate);
	if (clk_rate_round < 0) {
		CAM_ERR(CAM_UTIL, "round failed rc = %ld",
			clk_rate_round);
		return -EINVAL;
	}

	for (i = 0; i < CAM_MAX_VOTE; i++) {
		if (soc_info->clk_rate[i][src_clk_idx] >= clk_rate_round) {
			CAM_DBG(CAM_UTIL,
				"soc = %d round rate = %ld actual = %lld",
				soc_info->clk_rate[i][src_clk_idx],
				clk_rate_round,	clk_rate);
			return i;
		}
	}

	CAM_WARN(CAM_UTIL, "Invalid clock rate %ld", clk_rate_round);
	return -EINVAL;
}

/**
 * cam_soc_util_get_string_from_level()
 *
 * @brief:     Returns the string for a given clk level
 *
 * @level:     Clock level
 *
 * @return:    String corresponding to the clk level
 */
static const char *cam_soc_util_get_string_from_level(
	enum cam_vote_level level)
{
	switch (level) {
	case CAM_SUSPEND_VOTE:
		return "";
	case CAM_MINSVS_VOTE:
		return "MINSVS[1]";
	case CAM_LOWSVS_VOTE:
		return "LOWSVS[2]";
	case CAM_SVS_VOTE:
		return "SVS[3]";
	case CAM_SVSL1_VOTE:
		return "SVSL1[4]";
	case CAM_NOMINAL_VOTE:
		return "NOM[5]";
	case CAM_NOMINALL1_VOTE:
		return "NOML1[6]";
	case CAM_TURBO_VOTE:
		return "TURBO[7]";
	default:
		return "";
	}
}

/**
 * cam_soc_util_get_supported_clk_levels()
 *
 * @brief:      Returns the string of all the supported clk levels for
 *              the given device
 *
 * @soc_info:   Device soc information
 *
 * @return:     String containing all supported clk levels
 */
static const char *cam_soc_util_get_supported_clk_levels(
	struct cam_hw_soc_info *soc_info)
{
	int i = 0;

	memset(supported_clk_info, 0, sizeof(supported_clk_info));
	strlcat(supported_clk_info, "Supported levels: ",
		sizeof(supported_clk_info));

	for (i = 0; i < CAM_MAX_VOTE; i++) {
		if (soc_info->clk_level_valid[i] == true) {
			strlcat(supported_clk_info,
				cam_soc_util_get_string_from_level(i),
				sizeof(supported_clk_info));
			strlcat(supported_clk_info, " ",
				sizeof(supported_clk_info));
		}
	}

	strlcat(supported_clk_info, "\n", sizeof(supported_clk_info));
	return supported_clk_info;
}

static int cam_soc_util_clk_lvl_options_open(struct inode *inode,
	struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t cam_soc_util_clk_lvl_options_read(struct file *file,
	char __user *clk_info, size_t size_t, loff_t *loff_t)
{
	struct cam_hw_soc_info *soc_info =
		(struct cam_hw_soc_info *)file->private_data;
	const char *display_string =
		cam_soc_util_get_supported_clk_levels(soc_info);

	return simple_read_from_buffer(clk_info, size_t, loff_t, display_string,
		strlen(display_string));
}

static const struct file_operations cam_soc_util_clk_lvl_options = {
	.open = cam_soc_util_clk_lvl_options_open,
	.read = cam_soc_util_clk_lvl_options_read,
};

static int cam_soc_util_set_clk_lvl(void *data, u64 val)
{
	struct cam_hw_soc_info *soc_info = (struct cam_hw_soc_info *)data;

	if (val <= CAM_SUSPEND_VOTE || val >= CAM_MAX_VOTE)
		return 0;

	if (soc_info->clk_level_valid[val] == true)
		soc_info->clk_level_override = val;
	else
		soc_info->clk_level_override = 0;

	return 0;
}

static int cam_soc_util_get_clk_lvl(void *data, u64 *val)
{
	struct cam_hw_soc_info *soc_info = (struct cam_hw_soc_info *)data;

	*val = soc_info->clk_level_override;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cam_soc_util_clk_lvl_control,
	cam_soc_util_get_clk_lvl, cam_soc_util_set_clk_lvl, "%08llu");

/**
 * cam_soc_util_create_clk_lvl_debugfs()
 *
 * @brief:      Creates debugfs files to view/control device clk rates
 *
 * @soc_info:   Device soc information
 *
 * @return:     Success or failure
 */
static int cam_soc_util_create_clk_lvl_debugfs(
	struct cam_hw_soc_info *soc_info)
{
	struct dentry *dentry = NULL;

	if (!soc_info) {
		CAM_ERR(CAM_UTIL, "soc info is NULL");
		return -EINVAL;
	}

	if (soc_info->dentry)
		return 0;

	memset(debugfs_dir_name, 0, sizeof(debugfs_dir_name));
	strlcat(debugfs_dir_name, "clk_dir_", sizeof(debugfs_dir_name));
	strlcat(debugfs_dir_name, soc_info->dev_name, sizeof(debugfs_dir_name));

	dentry = soc_info->dentry;
	dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (!dentry) {
		CAM_ERR(CAM_UTIL, "failed to create debug directory");
		return -ENOMEM;
	}

	if (!debugfs_create_file("clk_lvl_options", 0444,
		dentry, soc_info, &cam_soc_util_clk_lvl_options)) {
		CAM_ERR(CAM_UTIL, "failed to create clk_lvl_options");
		goto err;
	}

	if (!debugfs_create_file("clk_lvl_control", 0644,
		dentry, soc_info, &cam_soc_util_clk_lvl_control)) {
		CAM_ERR(CAM_UTIL, "failed to create clk_lvl_control");
		goto err;
	}

	CAM_DBG(CAM_UTIL, "clk lvl debugfs for %s successfully created",
		soc_info->dev_name);

	return 0;

err:
	debugfs_remove_recursive(dentry);
	dentry = NULL;
	return -ENOMEM;
}

/**
 * cam_soc_util_remove_clk_lvl_debugfs()
 *
 * @brief:      Removes the debugfs files used to view/control
 *              device clk rates
 *
 * @soc_info:   Device soc information
 *
 */
static void cam_soc_util_remove_clk_lvl_debugfs(
	struct cam_hw_soc_info *soc_info)
{
	debugfs_remove_recursive(soc_info->dentry);
	soc_info->dentry = NULL;
}

int cam_soc_util_get_level_from_string(const char *string,
	enum cam_vote_level *level)
{
	if (!level)
		return -EINVAL;

	if (!strcmp(string, "suspend")) {
		*level = CAM_SUSPEND_VOTE;
	} else if (!strcmp(string, "minsvs")) {
		*level = CAM_MINSVS_VOTE;
	} else if (!strcmp(string, "lowsvs")) {
		*level = CAM_LOWSVS_VOTE;
	} else if (!strcmp(string, "svs")) {
		*level = CAM_SVS_VOTE;
	} else if (!strcmp(string, "svs_l1")) {
		*level = CAM_SVSL1_VOTE;
	} else if (!strcmp(string, "nominal")) {
		*level = CAM_NOMINAL_VOTE;
	} else if (!strcmp(string, "nominal_l1")) {
		*level = CAM_NOMINALL1_VOTE;
	} else if (!strcmp(string, "turbo")) {
		*level = CAM_TURBO_VOTE;
	} else {
		CAM_ERR(CAM_UTIL, "Invalid string %s", string);
		return -EINVAL;
	}

	return 0;
}

/**
 * cam_soc_util_get_clk_level_to_apply()
 *
 * @brief:              Get the clock level to apply. If the requested level
 *                      is not valid, bump the level to next available valid
 *                      level. If no higher level found, return failure.
 *
 * @soc_info:           Device soc struct to be populated
 * @req_level:          Requested level
 * @apply_level         Level to apply
 *
 * @return:             success or failure
 */
static int cam_soc_util_get_clk_level_to_apply(
	struct cam_hw_soc_info *soc_info, enum cam_vote_level req_level,
	enum cam_vote_level *apply_level)
{
	if (req_level >= CAM_MAX_VOTE) {
		CAM_ERR(CAM_UTIL, "Invalid clock level parameter %d",
			req_level);
		return -EINVAL;
	}

	if (soc_info->clk_level_valid[req_level] == true) {
		*apply_level = req_level;
	} else {
		int i;

		for (i = (req_level + 1); i < CAM_MAX_VOTE; i++)
			if (soc_info->clk_level_valid[i] == true) {
				*apply_level = i;
				break;
			}

		if (i == CAM_MAX_VOTE) {
			CAM_ERR(CAM_UTIL,
				"No valid clock level found to apply, req=%d",
				req_level);
			return -EINVAL;
		}
	}

	CAM_DBG(CAM_UTIL, "Req level %d, Applying %d",
		req_level, *apply_level);

	return 0;
}

int cam_soc_util_irq_enable(struct cam_hw_soc_info *soc_info)
{
	if (!soc_info) {
		CAM_ERR(CAM_UTIL, "Invalid arguments");
		return -EINVAL;
	}

	if (!soc_info->irq_line) {
		CAM_ERR(CAM_UTIL, "No IRQ line available");
		return -ENODEV;
	}

	enable_irq(soc_info->irq_line->start);

	return 0;
}

int cam_soc_util_irq_disable(struct cam_hw_soc_info *soc_info)
{
	if (!soc_info) {
		CAM_ERR(CAM_UTIL, "Invalid arguments");
		return -EINVAL;
	}

	if (!soc_info->irq_line) {
		CAM_ERR(CAM_UTIL, "No IRQ line available");
		return -ENODEV;
	}

	disable_irq(soc_info->irq_line->start);

	return 0;
}

long cam_soc_util_get_clk_round_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_index, unsigned long clk_rate)
{
	if (!soc_info || (clk_index >= soc_info->num_clk) || (clk_rate == 0)) {
		CAM_ERR(CAM_UTIL, "Invalid input params %pK, %d %lu",
			soc_info, clk_index, clk_rate);
		return clk_rate;
	}

	return clk_round_rate(soc_info->clk[clk_index], clk_rate);
}

int cam_soc_util_set_clk_flags(struct cam_hw_soc_info *soc_info,
	uint32_t clk_index, unsigned long flags)
{
	if (!soc_info || (clk_index >= soc_info->num_clk)) {
		CAM_ERR(CAM_UTIL, "Invalid input params %pK, %d",
			soc_info, clk_index);
		return -EINVAL;
	}

	return clk_set_flags(soc_info->clk[clk_index], flags);
}

/**
 * cam_soc_util_set_clk_rate()
 *
 * @brief:          Sets the given rate for the clk requested for
 *
 * @clk:            Clock structure information for which rate is to be set
 * @clk_name:       Name of the clock for which rate is being set
 * @clk_rate        Clock rate to be set
 *
 * @return:         Success or failure
 */
static int cam_soc_util_set_clk_rate(struct clk *clk, const char *clk_name,
	int64_t clk_rate)
{
	int rc = 0;
	long clk_rate_round;

	if (!clk || !clk_name)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "set %s, rate %lld", clk_name, clk_rate);
	if (clk_rate > 0) {
		clk_rate_round = clk_round_rate(clk, clk_rate);
		CAM_DBG(CAM_UTIL, "new_rate %ld", clk_rate_round);
		if (clk_rate_round < 0) {
			CAM_ERR(CAM_UTIL, "round failed for clock %s rc = %ld",
				clk_name, clk_rate_round);
			return clk_rate_round;
		}
		rc = clk_set_rate(clk, clk_rate_round);
		if (rc) {
			CAM_ERR(CAM_UTIL, "set_rate failed on %s", clk_name);
			return rc;
		}
	} else if (clk_rate == INIT_RATE) {
		clk_rate_round = clk_get_rate(clk);
		CAM_DBG(CAM_UTIL, "init new_rate %ld", clk_rate_round);
		if (clk_rate_round == 0) {
			clk_rate_round = clk_round_rate(clk, 0);
			if (clk_rate_round <= 0) {
				CAM_ERR(CAM_UTIL, "round rate failed on %s",
					clk_name);
				return clk_rate_round;
			}
		}
		rc = clk_set_rate(clk, clk_rate_round);
		if (rc) {
			CAM_ERR(CAM_UTIL, "set_rate failed on %s", clk_name);
			return rc;
		}
	}

	return rc;
}

int cam_soc_util_set_src_clk_rate(struct cam_hw_soc_info *soc_info,
	int64_t clk_rate)
{
	int32_t src_clk_idx;
	struct clk *clk = NULL;
	int32_t apply_level;
	uint32_t clk_level_override = 0;

	if (!soc_info || (soc_info->src_clk_idx < 0))
		return -EINVAL;

	src_clk_idx = soc_info->src_clk_idx;
	clk_level_override = soc_info->clk_level_override;
	if (clk_level_override && clk_rate)
		clk_rate =
			soc_info->clk_rate[clk_level_override][src_clk_idx];

	clk = soc_info->clk[src_clk_idx];

	if (soc_info->cam_cx_ipeak_enable && clk_rate >= 0) {
		apply_level = cam_soc_util_get_clk_level(soc_info, src_clk_idx,
				clk_rate);
		CAM_DBG(CAM_UTIL, "set %s, rate %lld dev_name = %s\n"
			"apply level = %d",
			soc_info->clk_name[src_clk_idx], clk_rate,
			soc_info->dev_name, apply_level);
		if (apply_level >= 0)
			cam_cx_ipeak_update_vote_cx_ipeak(soc_info,
				apply_level);
	}
	return cam_soc_util_set_clk_rate(clk,
		soc_info->clk_name[src_clk_idx], clk_rate);

}

int cam_soc_util_clk_put(struct clk **clk)
{
	if (!(*clk)) {
		CAM_ERR(CAM_UTIL, "Invalid params clk");
		return -EINVAL;
	}

	clk_put(*clk);
	*clk = NULL;

	return 0;
}

static struct clk *cam_soc_util_option_clk_get(struct device_node *np,
	int index)
{
	struct of_phandle_args clkspec;
	struct clk *clk;
	int rc;

	if (index < 0)
		return ERR_PTR(-EINVAL);

	rc = of_parse_phandle_with_args(np, "clocks-option", "#clock-cells",
		index, &clkspec);
	if (rc)
		return ERR_PTR(rc);

	clk = of_clk_get_from_provider(&clkspec);
	of_node_put(clkspec.np);

	return clk;
}

int cam_soc_util_get_option_clk_by_name(struct cam_hw_soc_info *soc_info,
	const char *clk_name, struct clk **clk, int32_t *clk_index,
	int32_t *clk_rate)
{
	int index = 0;
	int rc = 0;
	struct device_node *of_node = NULL;

	if (!soc_info || !clk_name || !clk) {
		CAM_ERR(CAM_UTIL,
			"Invalid params soc_info %pK clk_name %s clk %pK",
			soc_info, clk_name, clk);
		return -EINVAL;
	}

	of_node = soc_info->dev->of_node;

	index = of_property_match_string(of_node, "clock-names-option",
		clk_name);

	*clk = cam_soc_util_option_clk_get(of_node, index);
	if (IS_ERR(*clk)) {
		CAM_ERR(CAM_UTIL, "No clk named %s found. Dev %s", clk_name,
			soc_info->dev_name);
		*clk_index = -1;
		return -EFAULT;
	}
	*clk_index = index;

	rc = of_property_read_u32_index(of_node, "clock-rates-option",
		index, clk_rate);
	if (rc) {
		CAM_ERR(CAM_UTIL,
			"Error reading clock-rates clk_name %s index %d",
			clk_name, index);
		cam_soc_util_clk_put(clk);
		*clk_rate = 0;
		return rc;
	}

	/*
	 * Option clocks are assumed to be available to single Device here.
	 * Hence use INIT_RATE instead of NO_SET_RATE.
	 */
	*clk_rate = (*clk_rate == 0) ? (int32_t)INIT_RATE : *clk_rate;

	CAM_DBG(CAM_UTIL, "clk_name %s index %d clk_rate %d",
		clk_name, *clk_index, *clk_rate);

	return 0;
}

int cam_soc_util_clk_enable(struct clk *clk, const char *clk_name,
	int32_t clk_rate)
{
	int rc = 0;

	if (!clk || !clk_name)
		return -EINVAL;

	rc = cam_soc_util_set_clk_rate(clk, clk_name, clk_rate);
	if (rc)
		return rc;

	rc = clk_prepare_enable(clk);
	if (rc) {
		CAM_ERR(CAM_UTIL, "enable failed for %s: rc(%d)", clk_name, rc);
		return rc;
	}

	return rc;
}

int cam_soc_util_clk_disable(struct clk *clk, const char *clk_name)
{
	if (!clk || !clk_name)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "disable %s", clk_name);
	clk_disable_unprepare(clk);

	return 0;
}

/**
 * cam_soc_util_clk_enable_default()
 *
 * @brief:              This function enables the default clocks present
 *                      in soc_info
 *
 * @soc_info:           Device soc struct to be populated
 * @clk_level:          Clk level to apply while enabling
 *
 * @return:             success or failure
 */
int cam_soc_util_clk_enable_default(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level clk_level)
{
	int i, rc = 0;
	enum cam_vote_level apply_level;

	if ((soc_info->num_clk == 0) ||
		(soc_info->num_clk >= CAM_SOC_MAX_CLK)) {
		CAM_ERR(CAM_UTIL, "Invalid number of clock %d",
			soc_info->num_clk);
		return -EINVAL;
	}

	rc = cam_soc_util_get_clk_level_to_apply(soc_info, clk_level,
		&apply_level);
	if (rc)
		return rc;

	if (soc_info->cam_cx_ipeak_enable)
		cam_cx_ipeak_update_vote_cx_ipeak(soc_info, apply_level);

	for (i = 0; i < soc_info->num_clk; i++) {
		rc = cam_soc_util_clk_enable(soc_info->clk[i],
			soc_info->clk_name[i],
			soc_info->clk_rate[apply_level][i]);
		if (rc)
			goto clk_disable;
		if (soc_info->cam_cx_ipeak_enable) {
			CAM_DBG(CAM_UTIL,
			"dev name = %s clk name = %s idx = %d\n"
			"apply_level = %d clc idx = %d",
			soc_info->dev_name, soc_info->clk_name[i], i,
			apply_level, i);
		}

	}

	return rc;

clk_disable:
	if (soc_info->cam_cx_ipeak_enable)
		cam_cx_ipeak_update_vote_cx_ipeak(soc_info, 0);
	for (i--; i >= 0; i--) {
		cam_soc_util_clk_disable(soc_info->clk[i],
			soc_info->clk_name[i]);
	}

	return rc;
}

/**
 * cam_soc_util_clk_disable_default()
 *
 * @brief:              This function disables the default clocks present
 *                      in soc_info
 *
 * @soc_info:           device soc struct to be populated
 *
 * @return:             success or failure
 */
void cam_soc_util_clk_disable_default(struct cam_hw_soc_info *soc_info)
{
	int i;

	if (soc_info->num_clk == 0)
		return;

	if (soc_info->cam_cx_ipeak_enable)
		cam_cx_ipeak_unvote_cx_ipeak(soc_info);
	for (i = soc_info->num_clk - 1; i >= 0; i--)
		cam_soc_util_clk_disable(soc_info->clk[i],
			soc_info->clk_name[i]);
}

/**
 * cam_soc_util_get_dt_clk_info()
 *
 * @brief:              Parse the DT and populate the Clock properties
 *
 * @soc_info:           device soc struct to be populated
 * @src_clk_str         name of src clock that has rate control
 *
 * @return:             success or failure
 */
static int cam_soc_util_get_dt_clk_info(struct cam_hw_soc_info *soc_info)
{
	struct device_node *of_node = NULL;
	int count;
	int num_clk_rates, num_clk_levels;
	int i, j, rc;
	int32_t num_clk_level_strings;
	const char *src_clk_str = NULL;
	const char *clk_control_debugfs = NULL;
	const char *clk_cntl_lvl_string = NULL;
	enum cam_vote_level level;

	if (!soc_info || !soc_info->dev)
		return -EINVAL;

	of_node = soc_info->dev->of_node;

	if (!of_property_read_bool(of_node, "use-shared-clk")) {
		CAM_DBG(CAM_UTIL, "No shared clk parameter defined");
		soc_info->use_shared_clk = false;
	} else {
		soc_info->use_shared_clk = true;
	}

	count = of_property_count_strings(of_node, "clock-names");

	CAM_DBG(CAM_UTIL, "E: dev_name = %s count = %d",
		soc_info->dev_name, count);
	if (count > CAM_SOC_MAX_CLK) {
		CAM_ERR(CAM_UTIL, "invalid count of clocks, count=%d", count);
		rc = -EINVAL;
		return rc;
	}
	if (count <= 0) {
		CAM_DBG(CAM_UTIL, "No clock-names found");
		count = 0;
		soc_info->num_clk = count;
		return 0;
	}
	soc_info->num_clk = count;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &(soc_info->clk_name[i]));
		CAM_DBG(CAM_UTIL, "clock-names[%d] = %s",
			i, soc_info->clk_name[i]);
		if (rc) {
			CAM_ERR(CAM_UTIL,
				"i= %d count= %d reading clock-names failed",
				i, count);
			return rc;
		}
	}

	num_clk_rates = of_property_count_u32_elems(of_node, "clock-rates");
	if (num_clk_rates <= 0) {
		CAM_ERR(CAM_UTIL, "reading clock-rates count failed");
		return -EINVAL;
	}

	if ((num_clk_rates % soc_info->num_clk) != 0) {
		CAM_ERR(CAM_UTIL,
			"mismatch clk/rates, No of clocks=%d, No of rates=%d",
			soc_info->num_clk, num_clk_rates);
		return -EINVAL;
	}

	num_clk_levels = (num_clk_rates / soc_info->num_clk);

	num_clk_level_strings = of_property_count_strings(of_node,
		"clock-cntl-level");
	if (num_clk_level_strings != num_clk_levels) {
		CAM_ERR(CAM_UTIL,
			"Mismatch No of levels=%d, No of level string=%d",
			num_clk_levels, num_clk_level_strings);
		return -EINVAL;
	}

	for (i = 0; i < num_clk_levels; i++) {
		rc = of_property_read_string_index(of_node,
			"clock-cntl-level", i, &clk_cntl_lvl_string);
		if (rc) {
			CAM_ERR(CAM_UTIL,
				"Error reading clock-cntl-level, rc=%d", rc);
			return rc;
		}

		rc = cam_soc_util_get_level_from_string(clk_cntl_lvl_string,
			&level);
		if (rc)
			return rc;

		CAM_DBG(CAM_UTIL,
			"[%d] : %s %d", i, clk_cntl_lvl_string, level);
		soc_info->clk_level_valid[level] = true;
		for (j = 0; j < soc_info->num_clk; j++) {
			rc = of_property_read_u32_index(of_node, "clock-rates",
				((i * soc_info->num_clk) + j),
				&soc_info->clk_rate[level][j]);
			if (rc) {
				CAM_ERR(CAM_UTIL,
					"Error reading clock-rates, rc=%d",
					rc);
				return rc;
			}

			soc_info->clk_rate[level][j] =
				(soc_info->clk_rate[level][j] == 0) ?
				(int32_t)NO_SET_RATE :
				soc_info->clk_rate[level][j];

			CAM_DBG(CAM_UTIL, "soc_info->clk_rate[%d][%d] = %d",
				level, j,
				soc_info->clk_rate[level][j]);
		}
	}

	soc_info->src_clk_idx = -1;
	rc = of_property_read_string_index(of_node, "src-clock-name", 0,
		&src_clk_str);
	if (rc || !src_clk_str) {
		CAM_DBG(CAM_UTIL, "No src_clk_str found");
		rc = 0;
		goto end;
	}

	for (i = 0; i < soc_info->num_clk; i++) {
		if (strcmp(soc_info->clk_name[i], src_clk_str) == 0) {
			soc_info->src_clk_idx = i;
			CAM_DBG(CAM_UTIL, "src clock = %s, index = %d",
				src_clk_str, i);
			break;
		}
	}

	rc = of_property_read_string_index(of_node,
		"clock-control-debugfs", 0, &clk_control_debugfs);
	if (rc || !clk_control_debugfs) {
		CAM_DBG(CAM_UTIL, "No clock_control_debugfs property found");
		rc = 0;
		goto end;
	}

	if (strcmp("true", clk_control_debugfs) == 0)
		soc_info->clk_control_enable = true;

	CAM_DBG(CAM_UTIL, "X: dev_name = %s count = %d",
		soc_info->dev_name, count);
end:
	return rc;
}

int cam_soc_util_set_clk_rate_level(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level clk_level)
{
	int i, rc = 0;
	enum cam_vote_level apply_level;

	if ((soc_info->num_clk == 0) ||
		(soc_info->num_clk >= CAM_SOC_MAX_CLK)) {
		CAM_ERR(CAM_UTIL, "Invalid number of clock %d",
			soc_info->num_clk);
		return -EINVAL;
	}

	rc = cam_soc_util_get_clk_level_to_apply(soc_info, clk_level,
		&apply_level);
	if (rc)
		return rc;

	if (soc_info->cam_cx_ipeak_enable)
		cam_cx_ipeak_update_vote_cx_ipeak(soc_info, apply_level);

	for (i = 0; i < soc_info->num_clk; i++) {
		rc = cam_soc_util_set_clk_rate(soc_info->clk[i],
			soc_info->clk_name[i],
			soc_info->clk_rate[apply_level][i]);
		if (rc < 0) {
			CAM_DBG(CAM_UTIL,
				"dev name = %s clk_name = %s idx = %d\n"
				"apply_level = %d",
				soc_info->dev_name, soc_info->clk_name[i],
				i, apply_level);
			if (soc_info->cam_cx_ipeak_enable)
				cam_cx_ipeak_update_vote_cx_ipeak(soc_info, 0);
			break;
		}
	}

	return rc;
};

static int cam_soc_util_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct cam_soc_gpio_data *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "gpio-req-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		CAM_ERR(CAM_UTIL, "gpio-req-tbl-num 0");
		return 0;
	}

	val_array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!val_array)
		return -ENOMEM;

	gconf->cam_gpio_req_tbl = kcalloc(count, sizeof(struct gpio),
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		rc = -ENOMEM;
		goto free_val_array;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "gpio-req-tbl-num",
		val_array, count);
	if (rc) {
		CAM_ERR(CAM_UTIL, "failed in reading gpio-req-tbl-num, rc = %d",
			rc);
		goto free_gpio_req_tbl;
	}

	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			CAM_ERR(CAM_UTIL, "gpio req tbl index %d invalid",
				val_array[i]);
			goto free_gpio_req_tbl;
		}
		gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
		CAM_DBG(CAM_UTIL, "cam_gpio_req_tbl[%d].gpio = %d", i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "gpio-req-tbl-flags",
		val_array, count);
	if (rc) {
		CAM_ERR(CAM_UTIL, "Failed in gpio-req-tbl-flags, rc %d", rc);
		goto free_gpio_req_tbl;
	}

	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CAM_DBG(CAM_UTIL, "cam_gpio_req_tbl[%d].flags = %ld", i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"gpio-req-tbl-label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		if (rc) {
			CAM_ERR(CAM_UTIL, "Failed rc %d", rc);
			goto free_gpio_req_tbl;
		}
		CAM_DBG(CAM_UTIL, "cam_gpio_req_tbl[%d].label = %s", i,
			gconf->cam_gpio_req_tbl[i].label);
	}

	kfree(val_array);

	return rc;

free_gpio_req_tbl:
	kfree(gconf->cam_gpio_req_tbl);
free_val_array:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;

	return rc;
}

static int cam_soc_util_get_gpio_info(struct cam_hw_soc_info *soc_info)
{
	int32_t rc = 0, i = 0;
	uint16_t *gpio_array = NULL;
	int16_t gpio_array_size = 0;
	struct cam_soc_gpio_data *gconf = NULL;
	struct device_node *of_node = NULL;

	if (!soc_info || !soc_info->dev)
		return -EINVAL;

	of_node = soc_info->dev->of_node;

	/* Validate input parameters */
	if (!of_node) {
		CAM_ERR(CAM_UTIL, "Invalid param of_node");
		return -EINVAL;
	}

	gpio_array_size = of_gpio_count(of_node);

	if (gpio_array_size <= 0)
		return 0;

	CAM_DBG(CAM_UTIL, "gpio count %d", gpio_array_size);

	gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t), GFP_KERNEL);
	if (!gpio_array)
		goto free_gpio_conf;

	for (i = 0; i < gpio_array_size; i++) {
		gpio_array[i] = of_get_gpio(of_node, i);
		CAM_DBG(CAM_UTIL, "gpio_array[%d] = %d", i, gpio_array[i]);
	}

	gconf = kzalloc(sizeof(*gconf), GFP_KERNEL);
	if (!gconf)
		return -ENOMEM;

	rc = cam_soc_util_get_dt_gpio_req_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc) {
		CAM_ERR(CAM_UTIL, "failed in msm_camera_get_dt_gpio_req_tbl");
		goto free_gpio_array;
	}

	gconf->cam_gpio_common_tbl = kcalloc(gpio_array_size,
				sizeof(struct gpio), GFP_KERNEL);
	if (!gconf->cam_gpio_common_tbl) {
		rc = -ENOMEM;
		goto free_gpio_array;
	}

	for (i = 0; i < gpio_array_size; i++)
		gconf->cam_gpio_common_tbl[i].gpio = gpio_array[i];

	gconf->cam_gpio_common_tbl_size = gpio_array_size;
	soc_info->gpio_data = gconf;
	kfree(gpio_array);

	return rc;

free_gpio_array:
	kfree(gpio_array);
free_gpio_conf:
	kfree(gconf);
	soc_info->gpio_data = NULL;

	return rc;
}

static int cam_soc_util_request_gpio_table(
	struct cam_hw_soc_info *soc_info, bool gpio_en)
{
	int rc = 0, i = 0;
	uint8_t size = 0;
	struct cam_soc_gpio_data *gpio_conf =
			soc_info->gpio_data;
	struct gpio *gpio_tbl = NULL;


	if (!gpio_conf) {
		CAM_DBG(CAM_UTIL, "No GPIO entry");
		return 0;
	}
	if (gpio_conf->cam_gpio_common_tbl_size <= 0) {
		CAM_ERR(CAM_UTIL, "GPIO table size is invalid");
		return -EINVAL;
	}
	size = gpio_conf->cam_gpio_req_tbl_size;
	gpio_tbl = gpio_conf->cam_gpio_req_tbl;

	if (!gpio_tbl || !size) {
		CAM_ERR(CAM_UTIL, "Invalid gpio_tbl %pK / size %d",
			gpio_tbl, size);
		return -EINVAL;
	}
	for (i = 0; i < size; i++) {
		CAM_DBG(CAM_UTIL, "i=%d, gpio=%d dir=%ld", i,
			gpio_tbl[i].gpio, gpio_tbl[i].flags);
	}
	if (gpio_en) {
		for (i = 0; i < size; i++) {
			rc = gpio_request_one(gpio_tbl[i].gpio,
				gpio_tbl[i].flags, gpio_tbl[i].label);
			if (rc) {
				/*
				 * After GPIO request fails, contine to
				 * apply new gpios, outout a error message
				 * for driver bringup debug
				 */
				CAM_ERR(CAM_UTIL, "gpio %d:%s request fails",
					gpio_tbl[i].gpio, gpio_tbl[i].label);
			}
		}
	} else {
		gpio_free_array(gpio_tbl, size);
	}

	return rc;
}

static int cam_soc_util_get_dt_regulator_info
	(struct cam_hw_soc_info *soc_info)
{
	int rc = 0, count = 0, i = 0;
	struct device_node *of_node = NULL;

	if (!soc_info || !soc_info->dev) {
		CAM_ERR(CAM_UTIL, "Invalid parameters");
		return -EINVAL;
	}

	of_node = soc_info->dev->of_node;

	soc_info->num_rgltr = 0;
	count = of_property_count_strings(of_node, "regulator-names");
	if (count != -EINVAL) {
		if (count <= 0) {
			CAM_ERR(CAM_UTIL, "no regulators found");
			count = 0;
			return -EINVAL;
		}

		soc_info->num_rgltr = count;

	} else {
		CAM_DBG(CAM_UTIL, "No regulators node found");
		return 0;
	}

	for (i = 0; i < soc_info->num_rgltr; i++) {
		rc = of_property_read_string_index(of_node,
			"regulator-names", i, &soc_info->rgltr_name[i]);
		CAM_DBG(CAM_UTIL, "rgltr_name[%d] = %s",
			i, soc_info->rgltr_name[i]);
		if (rc) {
			CAM_ERR(CAM_UTIL, "no regulator resource at cnt=%d", i);
			return -ENODEV;
		}
	}

	if (!of_property_read_bool(of_node, "rgltr-cntrl-support")) {
		CAM_DBG(CAM_UTIL, "No regulator control parameter defined");
		soc_info->rgltr_ctrl_support = false;
		return 0;
	}

	soc_info->rgltr_ctrl_support = true;

	rc = of_property_read_u32_array(of_node, "rgltr-min-voltage",
		soc_info->rgltr_min_volt, soc_info->num_rgltr);
	if (rc) {
		CAM_ERR(CAM_UTIL, "No minimum volatage value found, rc=%d", rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node, "rgltr-max-voltage",
		soc_info->rgltr_max_volt, soc_info->num_rgltr);
	if (rc) {
		CAM_ERR(CAM_UTIL, "No maximum volatage value found, rc=%d", rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node, "rgltr-load-current",
		soc_info->rgltr_op_mode, soc_info->num_rgltr);
	if (rc) {
		CAM_ERR(CAM_UTIL, "No Load curent found rc=%d", rc);
		return -EINVAL;
	}

	return rc;
}

int cam_soc_util_get_dt_properties(struct cam_hw_soc_info *soc_info)
{
	struct device_node *of_node = NULL;
	int count = 0, i = 0, rc = 0;

	if (!soc_info || !soc_info->dev)
		return -EINVAL;

	of_node = soc_info->dev->of_node;

	rc = of_property_read_u32(of_node, "cell-index", &soc_info->index);
	if (rc) {
		CAM_ERR(CAM_UTIL, "device %s failed to read cell-index",
			soc_info->dev_name);
		return rc;
	}

	count = of_property_count_strings(of_node, "reg-names");
	if (count <= 0) {
		CAM_DBG(CAM_UTIL, "no reg-names found for: %s",
			soc_info->dev_name);
		count = 0;
	}
	soc_info->num_mem_block = count;

	for (i = 0; i < soc_info->num_mem_block; i++) {
		rc = of_property_read_string_index(of_node, "reg-names", i,
			&soc_info->mem_block_name[i]);
		if (rc) {
			CAM_ERR(CAM_UTIL, "failed to read reg-names at %d", i);
			return rc;
		}
		soc_info->mem_block[i] =
			platform_get_resource_byname(soc_info->pdev,
			IORESOURCE_MEM, soc_info->mem_block_name[i]);

		if (!soc_info->mem_block[i]) {
			CAM_ERR(CAM_UTIL, "no mem resource by name %s",
				soc_info->mem_block_name[i]);
			rc = -ENODEV;
			return rc;
		}
	}

	if (soc_info->num_mem_block > 0) {
		rc = of_property_read_u32_array(of_node, "reg-cam-base",
			soc_info->mem_block_cam_base, soc_info->num_mem_block);
		if (rc) {
			CAM_ERR(CAM_UTIL, "Error reading register offsets");
			return rc;
		}
	}

	rc = of_property_read_string_index(of_node, "interrupt-names", 0,
		&soc_info->irq_name);
	if (rc) {
		CAM_DBG(CAM_UTIL, "No interrupt line preset for: %s",
			soc_info->dev_name);
		rc = 0;
	} else {
		soc_info->irq_line =
			platform_get_resource_byname(soc_info->pdev,
			IORESOURCE_IRQ, soc_info->irq_name);
		if (!soc_info->irq_line) {
			CAM_ERR(CAM_UTIL, "no irq resource");
			rc = -ENODEV;
			return rc;
		}
	}

	rc = cam_soc_util_get_dt_regulator_info(soc_info);
	if (rc)
		return rc;

	rc = cam_soc_util_get_dt_clk_info(soc_info);
	if (rc)
		return rc;

	rc = cam_soc_util_get_gpio_info(soc_info);
	if (rc)
		return rc;

	if (of_find_property(of_node, "qcom,cam-cx-ipeak", NULL))
		rc = cam_cx_ipeak_register_cx_ipeak(soc_info);

	return rc;
}

/**
 * cam_soc_util_get_regulator()
 *
 * @brief:              Get regulator resource named vdd
 *
 * @dev:                Device associated with regulator
 * @reg:                Return pointer to be filled with regulator on success
 * @rgltr_name:         Name of regulator to get
 *
 * @return:             0 for Success, negative value for failure
 */
static int cam_soc_util_get_regulator(struct device *dev,
	struct regulator **reg, const char *rgltr_name)
{
	int rc = 0;
	*reg = regulator_get(dev, rgltr_name);
	if (IS_ERR_OR_NULL(*reg)) {
		rc = PTR_ERR(*reg);
		rc = rc ? rc : -EINVAL;
		CAM_ERR(CAM_UTIL, "Regulator %s get failed %d", rgltr_name, rc);
		*reg = NULL;
	}
	return rc;
}

int cam_soc_util_regulator_disable(struct regulator *rgltr,
	const char *rgltr_name, uint32_t rgltr_min_volt,
	uint32_t rgltr_max_volt, uint32_t rgltr_op_mode,
	uint32_t rgltr_delay_ms)
{
	int32_t rc = 0;

	if (!rgltr) {
		CAM_ERR(CAM_UTIL, "Invalid NULL parameter");
		return -EINVAL;
	}

	rc = regulator_disable(rgltr);
	if (rc) {
		CAM_ERR(CAM_UTIL, "%s regulator disable failed", rgltr_name);
		return rc;
	}

	if (rgltr_delay_ms > 20)
		msleep(rgltr_delay_ms);
	else if (rgltr_delay_ms)
		usleep_range(rgltr_delay_ms * 1000,
			(rgltr_delay_ms * 1000) + 1000);

	if (regulator_count_voltages(rgltr) > 0) {
		regulator_set_load(rgltr, 0);
		regulator_set_voltage(rgltr, 0, rgltr_max_volt);
	}

	return rc;
}


int cam_soc_util_regulator_enable(struct regulator *rgltr,
	const char *rgltr_name,
	uint32_t rgltr_min_volt, uint32_t rgltr_max_volt,
	uint32_t rgltr_op_mode, uint32_t rgltr_delay)
{
	int32_t rc = 0;

	if (!rgltr) {
		CAM_ERR(CAM_UTIL, "Invalid NULL parameter");
		return -EINVAL;
	}

	if (regulator_count_voltages(rgltr) > 0) {
		CAM_DBG(CAM_UTIL, "voltage min=%d, max=%d",
			rgltr_min_volt, rgltr_max_volt);

		rc = regulator_set_voltage(
			rgltr, rgltr_min_volt, rgltr_max_volt);
		if (rc) {
			CAM_ERR(CAM_UTIL, "%s set voltage failed", rgltr_name);
			return rc;
		}

		rc = regulator_set_load(rgltr, rgltr_op_mode);
		if (rc) {
			CAM_ERR(CAM_UTIL, "%s set optimum mode failed",
				rgltr_name);
			return rc;
		}
	}

	rc = regulator_enable(rgltr);
	if (rc) {
		CAM_ERR(CAM_UTIL, "%s regulator_enable failed", rgltr_name);
		return rc;
	}

	if (rgltr_delay > 20)
		msleep(rgltr_delay);
	else if (rgltr_delay)
		usleep_range(rgltr_delay * 1000,
			(rgltr_delay * 1000) + 1000);

	return rc;
}

static int cam_soc_util_request_pinctrl(
	struct cam_hw_soc_info *soc_info)
{

	struct cam_soc_pinctrl_info *device_pctrl = &soc_info->pinctrl_info;
	struct device *dev = soc_info->dev;

	device_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(device_pctrl->pinctrl)) {
		CAM_DBG(CAM_UTIL, "Pinctrl not available");
		device_pctrl->pinctrl = NULL;
		return 0;
	}
	device_pctrl->gpio_state_active =
		pinctrl_lookup_state(device_pctrl->pinctrl,
				CAM_SOC_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(device_pctrl->gpio_state_active)) {
		CAM_ERR(CAM_UTIL,
			"Failed to get the active state pinctrl handle");
		device_pctrl->gpio_state_active = NULL;
		return -EINVAL;
	}
	device_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(device_pctrl->pinctrl,
				CAM_SOC_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(device_pctrl->gpio_state_suspend)) {
		CAM_ERR(CAM_UTIL,
			"Failed to get the suspend state pinctrl handle");
		device_pctrl->gpio_state_suspend = NULL;
		return -EINVAL;
	}
	return 0;
}

static void cam_soc_util_regulator_disable_default(
	struct cam_hw_soc_info *soc_info)
{
	int j = 0;
	uint32_t num_rgltr = soc_info->num_rgltr;

	for (j = num_rgltr-1; j >= 0; j--) {
		if (soc_info->rgltr_ctrl_support == true) {
			cam_soc_util_regulator_disable(soc_info->rgltr[j],
				soc_info->rgltr_name[j],
				soc_info->rgltr_min_volt[j],
				soc_info->rgltr_max_volt[j],
				soc_info->rgltr_op_mode[j],
				soc_info->rgltr_delay[j]);
		} else {
			if (soc_info->rgltr[j])
				regulator_disable(soc_info->rgltr[j]);
		}
	}
}

static int cam_soc_util_regulator_enable_default(
	struct cam_hw_soc_info *soc_info)
{
	int j = 0, rc = 0;
	uint32_t num_rgltr = soc_info->num_rgltr;

	for (j = 0; j < num_rgltr; j++) {
		if (soc_info->rgltr_ctrl_support == true) {
			rc = cam_soc_util_regulator_enable(soc_info->rgltr[j],
				soc_info->rgltr_name[j],
				soc_info->rgltr_min_volt[j],
				soc_info->rgltr_max_volt[j],
				soc_info->rgltr_op_mode[j],
				soc_info->rgltr_delay[j]);
		} else {
			if (soc_info->rgltr[j])
				rc = regulator_enable(soc_info->rgltr[j]);
		}

		if (rc) {
			CAM_ERR(CAM_UTIL, "%s enable failed",
				soc_info->rgltr_name[j]);
			goto disable_rgltr;
		}
	}

	return rc;
disable_rgltr:

	for (j--; j >= 0; j--) {
		if (soc_info->rgltr_ctrl_support == true) {
			cam_soc_util_regulator_disable(soc_info->rgltr[j],
				soc_info->rgltr_name[j],
				soc_info->rgltr_min_volt[j],
				soc_info->rgltr_max_volt[j],
				soc_info->rgltr_op_mode[j],
				soc_info->rgltr_delay[j]);
		} else {
			if (soc_info->rgltr[j])
				regulator_disable(soc_info->rgltr[j]);
		}
	}

	return rc;
}

int cam_soc_util_request_platform_resource(
	struct cam_hw_soc_info *soc_info,
	irq_handler_t handler, void *irq_data)
{
	int i = 0, rc = 0;

	if (!soc_info || !soc_info->dev) {
		CAM_ERR(CAM_UTIL, "Invalid parameters");
		return -EINVAL;
	}

	for (i = 0; i < soc_info->num_mem_block; i++) {
		if (soc_info->reserve_mem) {
			if (!request_mem_region(soc_info->mem_block[i]->start,
				resource_size(soc_info->mem_block[i]),
				soc_info->mem_block_name[i])){
				CAM_ERR(CAM_UTIL,
					"Error Mem region request Failed:%s",
					soc_info->mem_block_name[i]);
				rc = -ENOMEM;
				goto unmap_base;
			}
		}
		soc_info->reg_map[i].mem_base = ioremap(
			soc_info->mem_block[i]->start,
			resource_size(soc_info->mem_block[i]));
		if (!soc_info->reg_map[i].mem_base) {
			CAM_ERR(CAM_UTIL, "i= %d base NULL", i);
			rc = -ENOMEM;
			goto unmap_base;
		}
		soc_info->reg_map[i].mem_cam_base =
			soc_info->mem_block_cam_base[i];
		soc_info->reg_map[i].size =
			resource_size(soc_info->mem_block[i]);
		soc_info->num_reg_map++;
	}

	for (i = 0; i < soc_info->num_rgltr; i++) {
		if (soc_info->rgltr_name[i] == NULL) {
			CAM_ERR(CAM_UTIL, "can't find regulator name");
			goto put_regulator;
		}

		rc = cam_soc_util_get_regulator(soc_info->dev,
			&soc_info->rgltr[i],
			soc_info->rgltr_name[i]);
		if (rc)
			goto put_regulator;
	}

	if (soc_info->irq_line) {
		rc = devm_request_irq(soc_info->dev, soc_info->irq_line->start,
			handler, IRQF_TRIGGER_RISING,
			soc_info->irq_name, irq_data);
		if (rc) {
			CAM_ERR(CAM_UTIL, "irq request fail");
			rc = -EBUSY;
			goto put_regulator;
		}
		disable_irq(soc_info->irq_line->start);
		soc_info->irq_data = irq_data;
	}

	/* Get Clock */
	for (i = 0; i < soc_info->num_clk; i++) {
		soc_info->clk[i] = clk_get(soc_info->dev,
			soc_info->clk_name[i]);
		if (!soc_info->clk[i]) {
			CAM_ERR(CAM_UTIL, "get failed for %s",
				soc_info->clk_name[i]);
			rc = -ENOENT;
			goto put_clk;
		}
	}

	rc = cam_soc_util_request_pinctrl(soc_info);
	if (rc)
		CAM_DBG(CAM_UTIL, "Failed in request pinctrl, rc=%d", rc);

	rc = cam_soc_util_request_gpio_table(soc_info, true);
	if (rc) {
		CAM_ERR(CAM_UTIL, "Failed in request gpio table, rc=%d", rc);
		goto put_clk;
	}

	if (soc_info->clk_control_enable)
		cam_soc_util_create_clk_lvl_debugfs(soc_info);

	return rc;

put_clk:
	if (i == -1)
		i = soc_info->num_clk;
	for (i = i - 1; i >= 0; i--) {
		if (soc_info->clk[i]) {
			clk_put(soc_info->clk[i]);
			soc_info->clk[i] = NULL;
		}
	}

	if (soc_info->irq_line) {
		disable_irq(soc_info->irq_line->start);
		devm_free_irq(soc_info->dev,
			soc_info->irq_line->start, irq_data);
	}

put_regulator:
	if (i == -1)
		i = soc_info->num_rgltr;
	for (i = i - 1; i >= 0; i--) {
		if (soc_info->rgltr[i]) {
			regulator_disable(soc_info->rgltr[i]);
			regulator_put(soc_info->rgltr[i]);
			soc_info->rgltr[i] = NULL;
		}
	}

unmap_base:
	if (i == -1)
		i = soc_info->num_reg_map;
	for (i = i - 1; i >= 0; i--) {
		if (soc_info->reserve_mem)
			release_mem_region(soc_info->mem_block[i]->start,
				resource_size(soc_info->mem_block[i]));
		iounmap(soc_info->reg_map[i].mem_base);
		soc_info->reg_map[i].mem_base = NULL;
		soc_info->reg_map[i].size = 0;
	}

	return rc;
}

int cam_soc_util_release_platform_resource(struct cam_hw_soc_info *soc_info)
{
	int i;

	if (!soc_info || !soc_info->dev) {
		CAM_ERR(CAM_UTIL, "Invalid parameter");
		return -EINVAL;
	}

	for (i = soc_info->num_clk - 1; i >= 0; i--) {
		clk_put(soc_info->clk[i]);
		soc_info->clk[i] = NULL;
	}

	for (i = soc_info->num_rgltr - 1; i >= 0; i--) {
		if (soc_info->rgltr[i]) {
			regulator_put(soc_info->rgltr[i]);
			soc_info->rgltr[i] = NULL;
		}
	}

	for (i = soc_info->num_reg_map - 1; i >= 0; i--) {
		iounmap(soc_info->reg_map[i].mem_base);
		soc_info->reg_map[i].mem_base = NULL;
		soc_info->reg_map[i].size = 0;
	}

	if (soc_info->irq_line) {
		disable_irq(soc_info->irq_line->start);
		devm_free_irq(soc_info->dev,
			soc_info->irq_line->start, soc_info->irq_data);
	}

	if (soc_info->pinctrl_info.pinctrl)
		devm_pinctrl_put(soc_info->pinctrl_info.pinctrl);


	/* release for gpio */
	cam_soc_util_request_gpio_table(soc_info, false);

	if (soc_info->clk_control_enable)
		cam_soc_util_remove_clk_lvl_debugfs(soc_info);

	return 0;
}

int cam_soc_util_enable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool enable_clocks, enum cam_vote_level clk_level, bool enable_irq)
{
	int rc = 0;

	if (!soc_info)
		return -EINVAL;

	rc = cam_soc_util_regulator_enable_default(soc_info);
	if (rc) {
		CAM_ERR(CAM_UTIL, "Regulators enable failed");
		return rc;
	}

	if (enable_clocks) {
		rc = cam_soc_util_clk_enable_default(soc_info, clk_level);
		if (rc)
			goto disable_regulator;
	}

	if (enable_irq) {
		rc  = cam_soc_util_irq_enable(soc_info);
		if (rc)
			goto disable_clk;
	}

	if (soc_info->pinctrl_info.pinctrl &&
		soc_info->pinctrl_info.gpio_state_active) {
		rc = pinctrl_select_state(soc_info->pinctrl_info.pinctrl,
			soc_info->pinctrl_info.gpio_state_active);

		if (rc)
			goto disable_irq;
	}

	return rc;

disable_irq:
	if (enable_irq)
		cam_soc_util_irq_disable(soc_info);

disable_clk:
	if (enable_clocks)
		cam_soc_util_clk_disable_default(soc_info);

disable_regulator:
	cam_soc_util_regulator_disable_default(soc_info);


	return rc;
}

int cam_soc_util_disable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool disable_clocks, bool disable_irq)
{
	int rc = 0;

	if (!soc_info)
		return -EINVAL;

	if (disable_irq)
		rc |= cam_soc_util_irq_disable(soc_info);

	if (disable_clocks)
		cam_soc_util_clk_disable_default(soc_info);

	cam_soc_util_regulator_disable_default(soc_info);

	if (soc_info->pinctrl_info.pinctrl &&
		soc_info->pinctrl_info.gpio_state_suspend)
		rc = pinctrl_select_state(soc_info->pinctrl_info.pinctrl,
			soc_info->pinctrl_info.gpio_state_suspend);

	return rc;
}

int cam_soc_util_reg_dump(struct cam_hw_soc_info *soc_info,
	uint32_t base_index, uint32_t offset, int size)
{
	void __iomem     *base_addr = NULL;

	CAM_DBG(CAM_UTIL, "base_idx %u size=%d", base_index, size);

	if (!soc_info || base_index >= soc_info->num_reg_map ||
		size <= 0 || (offset + size) >=
		CAM_SOC_GET_REG_MAP_SIZE(soc_info, base_index))
		return -EINVAL;

	base_addr = CAM_SOC_GET_REG_MAP_START(soc_info, base_index);

	/*
	 * All error checking already done above,
	 * hence ignoring the return value below.
	 */
	cam_io_dump(base_addr, offset, size);

	return 0;
}

uint32_t cam_soc_util_get_vote_level(struct cam_hw_soc_info *soc_info,
	uint64_t clock_rate)
{
	int i = 0;

	if (!clock_rate)
		return CAM_SVS_VOTE;

	for (i = 0; i < CAM_MAX_VOTE; i++) {
		if (soc_info->clk_level_valid[i] &&
			soc_info->clk_rate[i][soc_info->src_clk_idx] >=
			clock_rate) {
			CAM_DBG(CAM_UTIL,
				"Clock rate %lld, selected clock level %d",
				clock_rate, i);
			return i;
		}
	}

	return CAM_TURBO_VOTE;
}
