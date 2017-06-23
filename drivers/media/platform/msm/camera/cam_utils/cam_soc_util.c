/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/of.h>
#include <linux/clk.h>
#include "cam_soc_util.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

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
	} else if (!strcmp(string, "turbo")) {
		*level = CAM_TURBO_VOTE;
	} else {
		pr_err("Invalid string %s\n", string);
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
		pr_err("Invalid clock level parameter %d\n", req_level);
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
			pr_err("No valid clock level found to apply, req=%d\n",
				req_level);
			return -EINVAL;
		}
	}

	CDBG("Req level %d, Applying %d\n", req_level, *apply_level);

	return 0;
}

int cam_soc_util_irq_enable(struct cam_hw_soc_info *soc_info)
{
	if (!soc_info) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	if (!soc_info->irq_line) {
		pr_err("No IRQ line available\n");
		return -ENODEV;
	}

	enable_irq(soc_info->irq_line->start);

	return 0;
}

int cam_soc_util_irq_disable(struct cam_hw_soc_info *soc_info)
{
	if (!soc_info) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	if (!soc_info->irq_line) {
		pr_err("No IRQ line available\n");
		return -ENODEV;
	}

	disable_irq(soc_info->irq_line->start);

	return 0;
}

/**
 * cam_soc_util_set_clk_rate()
 *
 * @brief:              Set the rate on a given clock.
 *
 * @clk:                Clock that needs to be set
 * @clk_name:           Clocks name associated with clk
 * @clk_rate:           Clocks rate associated with clk
 *
 * @return:             success or failure
 */
static int cam_soc_util_set_clk_rate(struct clk *clk, const char *clk_name,
	int32_t clk_rate)
{
	int rc = 0;
	long clk_rate_round;

	if (!clk || !clk_name)
		return -EINVAL;

	CDBG("set %s, rate %d\n", clk_name, clk_rate);
	if (clk_rate > 0) {
		clk_rate_round = clk_round_rate(clk, clk_rate);
		CDBG("new_rate %ld\n", clk_rate_round);
		if (clk_rate_round < 0) {
			pr_err("round failed for clock %s rc = %ld\n",
				clk_name, clk_rate_round);
			return clk_rate_round;
		}
		rc = clk_set_rate(clk, clk_rate_round);
		if (rc) {
			pr_err("set_rate failed on %s\n", clk_name);
			return rc;
		}
	} else if (clk_rate == INIT_RATE) {
		clk_rate_round = clk_get_rate(clk);
		CDBG("init new_rate %ld\n", clk_rate_round);
		if (clk_rate_round == 0) {
			clk_rate_round = clk_round_rate(clk, 0);
			if (clk_rate_round <= 0) {
				pr_err("round rate failed on %s\n", clk_name);
				return clk_rate_round;
			}
		}
		rc = clk_set_rate(clk, clk_rate_round);
		if (rc) {
			pr_err("set_rate failed on %s\n", clk_name);
			return rc;
		}
	}

	return rc;
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
		pr_err("enable failed for %s: rc(%d)\n", clk_name, rc);
		return rc;
	}

	return rc;
}

int cam_soc_util_clk_disable(struct clk *clk, const char *clk_name)
{
	if (!clk || !clk_name)
		return -EINVAL;

	CDBG("disable %s\n", clk_name);
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
static int cam_soc_util_clk_enable_default(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level clk_level)
{
	int i, rc = 0;
	enum cam_vote_level apply_level;

	if ((soc_info->num_clk == 0) ||
		(soc_info->num_clk >= CAM_SOC_MAX_CLK)) {
		pr_err("Invalid number of clock %d\n", soc_info->num_clk);
		return -EINVAL;
	}

	rc = cam_soc_util_get_clk_level_to_apply(soc_info, clk_level,
		&apply_level);
	if (rc)
		return rc;

	for (i = 0; i < soc_info->num_clk; i++) {
		rc = cam_soc_util_clk_enable(soc_info->clk[i],
			soc_info->clk_name[i],
			soc_info->clk_rate[apply_level][i]);
		if (rc)
			goto clk_disable;
	}

	return rc;

clk_disable:
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
static void cam_soc_util_clk_disable_default(struct cam_hw_soc_info *soc_info)
{
	int i;

	if (soc_info->num_clk == 0)
		return;

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
	struct platform_device *pdev = NULL;
	const char *src_clk_str = NULL;
	const char *clk_cntl_lvl_string = NULL;
	enum cam_vote_level level;

	if (!soc_info || !soc_info->pdev)
		return -EINVAL;

	pdev = soc_info->pdev;

	of_node = pdev->dev.of_node;

	count = of_property_count_strings(of_node, "clock-names");

	CDBG("count = %d\n", count);
	if (count > CAM_SOC_MAX_CLK) {
		pr_err("invalid count of clocks, count=%d", count);
		rc = -EINVAL;
		return rc;
	}
	if (count <= 0) {
		CDBG("No clock-names found\n");
		count = 0;
		soc_info->num_clk = count;
		return 0;
	}
	soc_info->num_clk = count;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &(soc_info->clk_name[i]));
		CDBG("clock-names[%d] = %s\n", i, soc_info->clk_name[i]);
		if (rc) {
			pr_err("i= %d count= %d reading clock-names failed\n",
				i, count);
			return rc;
		}
	}

	num_clk_rates = of_property_count_u32_elems(of_node, "clock-rates");
	if (num_clk_rates <= 0) {
		pr_err("reading clock-rates count failed\n");
		return -EINVAL;
	}

	if ((num_clk_rates % soc_info->num_clk) != 0) {
		pr_err("mismatch clk/rates, No of clocks=%d, No of rates=%d\n",
			soc_info->num_clk, num_clk_rates);
		return -EINVAL;
	}

	num_clk_levels = (num_clk_rates / soc_info->num_clk);

	num_clk_level_strings = of_property_count_strings(of_node,
		"clock-cntl-level");
	if (num_clk_level_strings != num_clk_levels) {
		pr_err("Mismatch No of levels=%d, No of level string=%d\n",
			num_clk_levels, num_clk_level_strings);
		return -EINVAL;
	}

	for (i = 0; i < num_clk_levels; i++) {
		rc = of_property_read_string_index(of_node,
			"clock-cntl-level", i, &clk_cntl_lvl_string);
		if (rc) {
			pr_err("Error reading clock-cntl-level, rc=%d\n", rc);
			return rc;
		}

		rc = cam_soc_util_get_level_from_string(clk_cntl_lvl_string,
			&level);
		if (rc)
			return rc;

		CDBG("[%d] : %s %d\n", i, clk_cntl_lvl_string, level);
		soc_info->clk_level_valid[level] = true;
		for (j = 0; j < soc_info->num_clk; j++) {
			rc = of_property_read_u32_index(of_node, "clock-rates",
				((i * soc_info->num_clk) + j),
				&soc_info->clk_rate[level][j]);
			if (rc) {
				pr_err("Error reading clock-rates, rc=%d\n",
					rc);
				return rc;
			}

			soc_info->clk_rate[level][j] =
				(soc_info->clk_rate[level][j] == 0) ?
				(long)NO_SET_RATE :
				soc_info->clk_rate[level][j];

			CDBG("soc_info->clk_rate[%d][%d] = %d\n", level, j,
				soc_info->clk_rate[level][j]);
		}
	}

	soc_info->src_clk_idx = -1;
	rc = of_property_read_string_index(of_node, "src-clock-name", 0,
		&src_clk_str);
	if (rc || !src_clk_str) {
		CDBG("No src_clk_str found\n");
		rc = 0;
		/* Bottom loop is dependent on src_clk_str. So return here */
		return rc;
	}

	for (i = 0; i < soc_info->num_clk; i++) {
		if (strcmp(soc_info->clk_name[i], src_clk_str) == 0) {
			soc_info->src_clk_idx = i;
			CDBG("src clock = %s, index = %d\n", src_clk_str, i);
			break;
		}
	}

	return rc;
}

int cam_soc_util_set_clk_rate_level(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level clk_level)
{
	int i, rc = 0;
	enum cam_vote_level apply_level;

	if ((soc_info->num_clk == 0) ||
		(soc_info->num_clk >= CAM_SOC_MAX_CLK)) {
		pr_err("Invalid number of clock %d\n", soc_info->num_clk);
		return -EINVAL;
	}

	rc = cam_soc_util_get_clk_level_to_apply(soc_info, clk_level,
		&apply_level);
	if (rc)
		return rc;

	for (i = 0; i < soc_info->num_clk; i++) {
		rc = cam_soc_util_set_clk_rate(soc_info->clk[i],
			soc_info->clk_name[i],
			soc_info->clk_rate[apply_level][i]);
		if (rc)
			break;
	}

	return rc;
};

int cam_soc_util_get_dt_properties(struct cam_hw_soc_info *soc_info)
{
	struct device_node *of_node = NULL;
	int count = 0, i = 0, rc = 0;
	struct platform_device *pdev = NULL;

	if (!soc_info || !soc_info->pdev)
		return -EINVAL;

	pdev = soc_info->pdev;

	of_node = pdev->dev.of_node;

	rc = of_property_read_u32(of_node, "cell-index", &soc_info->index);
	if (rc) {
		pr_err("device %s failed to read cell-index\n", pdev->name);
		return rc;
	}

	count = of_property_count_strings(of_node, "regulator-names");
	if (count <= 0) {
		pr_err("no regulators found\n");
		count = 0;
	}
	soc_info->num_rgltr = count;

	for (i = 0; i < soc_info->num_rgltr; i++) {
		rc = of_property_read_string_index(of_node,
			"regulator-names", i, &soc_info->rgltr_name[i]);
		CDBG("rgltr_name[%d] = %s\n", i, soc_info->rgltr_name[i]);
		if (rc) {
			pr_err("no regulator resource at cnt=%d\n", i);
			rc = -ENODEV;
			return rc;
		}
	}

	count = of_property_count_strings(of_node, "reg-names");
	if (count <= 0) {
		pr_err("no reg-names found\n");
		count = 0;
	}
	soc_info->num_mem_block = count;

	for (i = 0; i < soc_info->num_mem_block; i++) {
		rc = of_property_read_string_index(of_node, "reg-names", i,
			&soc_info->mem_block_name[i]);
		if (rc) {
			pr_err("failed to read reg-names at %d\n", i);
			return rc;
		}
		soc_info->mem_block[i] =
			platform_get_resource_byname(pdev, IORESOURCE_MEM,
			soc_info->mem_block_name[i]);

		if (!soc_info->mem_block[i]) {
			pr_err("no mem resource by name %s\n",
				soc_info->mem_block_name[i]);
			rc = -ENODEV;
			return rc;
		}
	}

	if (soc_info->num_mem_block > 0) {
		rc = of_property_read_u32_array(of_node, "reg-cam-base",
			soc_info->mem_block_cam_base, soc_info->num_mem_block);
		if (rc) {
			pr_err("Error reading register offsets\n");
			return rc;
		}
	}

	rc = of_property_read_string_index(of_node, "interrupt-names", 0,
		&soc_info->irq_name);
	if (rc) {
		pr_warn("No interrupt line present\n");
	} else {
		soc_info->irq_line = platform_get_resource_byname(pdev,
			IORESOURCE_IRQ, soc_info->irq_name);
		if (!soc_info->irq_line) {
			pr_err("no irq resource\n");
			rc = -ENODEV;
			return rc;
		}
	}

	rc = cam_soc_util_get_dt_clk_info(soc_info);

	return rc;
}

/**
 * cam_soc_util_get_regulator()
 *
 * @brief:              Get regulator resource named vdd
 *
 * @pdev:               Platform device associated with regulator
 * @reg:                Return pointer to be filled with regulator on success
 * @rgltr_name:         Name of regulator to get
 *
 * @return:             0 for Success, negative value for failure
 */
static int cam_soc_util_get_regulator(struct platform_device *pdev,
	struct regulator **reg, const char *rgltr_name)
{
	int rc = 0;
	*reg = regulator_get(&pdev->dev, rgltr_name);
	if (IS_ERR_OR_NULL(*reg)) {
		rc = PTR_ERR(*reg);
		rc = rc ? rc : -EINVAL;
		pr_err("Regulator %s get failed %d\n", rgltr_name, rc);
		*reg = NULL;
	}
	return rc;
}

int cam_soc_util_request_platform_resource(struct cam_hw_soc_info *soc_info,
	irq_handler_t handler, void *irq_data)
{
	int i = 0, rc = 0;
	struct platform_device *pdev = NULL;

	if (!soc_info || !soc_info->pdev)
		return -EINVAL;

	pdev = soc_info->pdev;

	for (i = 0; i < soc_info->num_mem_block; i++) {
		soc_info->reg_map[i].mem_base = ioremap(
			soc_info->mem_block[i]->start,
			resource_size(soc_info->mem_block[i]));
		if (!soc_info->reg_map[i].mem_base) {
			pr_err("i= %d base NULL\n", i);
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
		rc = cam_soc_util_get_regulator(pdev, &soc_info->rgltr[i],
			soc_info->rgltr_name[i]);
		if (rc)
			goto put_regulator;
	}

	if (soc_info->irq_line) {
		rc = devm_request_irq(&pdev->dev, soc_info->irq_line->start,
			handler, IRQF_TRIGGER_RISING,
			soc_info->irq_name, irq_data);
		if (rc < 0) {
			pr_err("irq request fail\n");
			rc = -EBUSY;
			goto put_regulator;
		}
		disable_irq(soc_info->irq_line->start);
		soc_info->irq_data = irq_data;
	}

	/* Get Clock */
	for (i = 0; i < soc_info->num_clk; i++) {
		soc_info->clk[i] = clk_get(&soc_info->pdev->dev,
			soc_info->clk_name[i]);
		if (!soc_info->clk[i]) {
			pr_err("get failed for %s\n", soc_info->clk_name[i]);
			rc = -ENOENT;
			goto put_clk;
		}
	}

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
		devm_free_irq(&soc_info->pdev->dev,
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
		iounmap(soc_info->reg_map[i].mem_base);
		soc_info->reg_map[i].mem_base = NULL;
		soc_info->reg_map[i].size = 0;
	}

	return rc;
}

int cam_soc_util_release_platform_resource(struct cam_hw_soc_info *soc_info)
{
	int i;
	struct platform_device *pdev = NULL;

	if (!soc_info || !soc_info->pdev)
		return -EINVAL;

	pdev = soc_info->pdev;

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
		devm_free_irq(&soc_info->pdev->dev,
			soc_info->irq_line->start, soc_info->irq_data);
	}

	return 0;
}

int cam_soc_util_enable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool enable_clocks, enum cam_vote_level clk_level, bool enable_irq)
{
	int i, rc = 0;

	if (!soc_info)
		return -EINVAL;

	for (i = 0; i < soc_info->num_rgltr; i++) {
		rc = regulator_enable(soc_info->rgltr[i]);
		if (rc) {
			pr_err("Regulator enable %s failed\n",
				soc_info->rgltr_name[i]);
			goto disable_regulator;
		}
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

	return rc;

disable_clk:
	if (enable_clocks)
		cam_soc_util_clk_disable_default(soc_info);

disable_regulator:
	if (i == -1)
		i = soc_info->num_rgltr;
	for (i = i - 1; i >= 0; i--) {
		if (soc_info->rgltr[i])
			regulator_disable(soc_info->rgltr[i]);
	}

	return rc;
}

int cam_soc_util_disable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool disable_clocks, bool disble_irq)
{
	int i, rc = 0;

	if (!soc_info)
		return -EINVAL;

	if (disable_clocks)
		cam_soc_util_clk_disable_default(soc_info);

	for (i = soc_info->num_rgltr - 1; i >= 0; i--) {
		rc |= regulator_disable(soc_info->rgltr[i]);
		if (rc) {
			pr_err("Regulator disble %s failed\n",
				soc_info->rgltr_name[i]);
			continue;
		}
	}

	if (disble_irq)
		rc |= cam_soc_util_irq_disable(soc_info);

	return rc;
}

int cam_soc_util_reg_dump(struct cam_hw_soc_info *soc_info,
	uint32_t base_index, uint32_t offset, int size)
{
	void __iomem     *base_addr = NULL;

	CDBG("base_idx %u size=%d\n", base_index, size);

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

