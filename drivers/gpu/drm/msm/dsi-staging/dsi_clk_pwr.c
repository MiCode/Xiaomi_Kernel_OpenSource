/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk/msm-clk.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "dsi_clk_pwr.h"

#define INC_REFCOUNT(s, start_func) \
	({ \
		int rc = 0; \
		if ((s)->refcount == 0) { \
			rc = start_func(s); \
			if (rc) \
				pr_err("failed to enable, rc = %d\n", rc); \
		} \
		(s)->refcount++; \
		rc; \
	})

#define DEC_REFCOUNT(s, stop_func) \
	({ \
		int rc = 0; \
		if ((s)->refcount == 0) { \
			pr_err("unbalanced refcount\n"); \
		} else { \
			(s)->refcount--; \
			if ((s)->refcount == 0) { \
				rc = stop_func(s); \
				if (rc) \
					pr_err("disable failed, rc=%d\n", rc); \
			} \
		} \
		rc; \
	})

static int dsi_core_clk_start(struct dsi_core_clk_info *clks)
{
	int rc = 0;

	rc = clk_prepare_enable(clks->mdp_core_clk);
	if (rc) {
		pr_err("failed to enable mdp_core_clk, rc=%d\n", rc);
		goto error;
	}

	rc = clk_prepare_enable(clks->iface_clk);
	if (rc) {
		pr_err("failed to enable iface_clk, rc=%d\n", rc);
		goto error_disable_core_clk;
	}

	rc = clk_prepare_enable(clks->bus_clk);
	if (rc) {
		pr_err("failed to enable bus_clk, rc=%d\n", rc);
		goto error_disable_iface_clk;
	}

	rc = clk_prepare_enable(clks->core_mmss_clk);
	if (rc) {
		pr_err("failed to enable core_mmss_clk, rc=%d\n", rc);
		goto error_disable_bus_clk;
	}

	return rc;

error_disable_bus_clk:
	clk_disable_unprepare(clks->bus_clk);
error_disable_iface_clk:
	clk_disable_unprepare(clks->iface_clk);
error_disable_core_clk:
	clk_disable_unprepare(clks->mdp_core_clk);
error:
	return rc;
}

static int dsi_core_clk_stop(struct dsi_core_clk_info *clks)
{
	clk_disable_unprepare(clks->core_mmss_clk);
	clk_disable_unprepare(clks->bus_clk);
	clk_disable_unprepare(clks->iface_clk);
	clk_disable_unprepare(clks->mdp_core_clk);

	return 0;
}

static int dsi_link_clk_set_rate(struct dsi_link_clk_info *l_clks)
{
	int rc = 0;

	rc = clk_set_rate(l_clks->esc_clk, l_clks->esc_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for esc_clk rc = %d\n", rc);
		goto error;
	}

	rc = clk_set_rate(l_clks->byte_clk, l_clks->byte_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for byte_clk rc = %d\n", rc);
		goto error;
	}

	rc = clk_set_rate(l_clks->pixel_clk, l_clks->pixel_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for pixel_clk rc = %d\n", rc);
		goto error;
	}
error:
	return rc;
}

static int dsi_link_clk_prepare(struct dsi_link_clk_info *l_clks)
{
	int rc = 0;

	rc = clk_prepare(l_clks->esc_clk);
	if (rc) {
		pr_err("Failed to prepare dsi esc clk, rc=%d\n", rc);
		goto esc_clk_err;
	}

	rc = clk_prepare(l_clks->byte_clk);
	if (rc) {
		pr_err("Failed to prepare dsi byte clk, rc=%d\n", rc);
		goto byte_clk_err;
	}

	rc = clk_prepare(l_clks->pixel_clk);
	if (rc) {
		pr_err("Failed to prepare dsi pixel clk, rc=%d\n", rc);
		goto pixel_clk_err;
	}

	return rc;

pixel_clk_err:
	clk_unprepare(l_clks->byte_clk);
byte_clk_err:
	clk_unprepare(l_clks->esc_clk);
esc_clk_err:
	return rc;
}

static void dsi_link_clk_unprepare(struct dsi_link_clk_info *l_clks)
{
	clk_unprepare(l_clks->pixel_clk);
	clk_unprepare(l_clks->byte_clk);
	clk_unprepare(l_clks->esc_clk);
}

static int dsi_link_clk_enable(struct dsi_link_clk_info *l_clks)
{
	int rc = 0;

	rc = clk_enable(l_clks->esc_clk);
	if (rc) {
		pr_err("Failed to enable dsi esc clk, rc=%d\n", rc);
		goto esc_clk_err;
	}

	rc = clk_enable(l_clks->byte_clk);
	if (rc) {
		pr_err("Failed to enable dsi byte clk, rc=%d\n", rc);
		goto byte_clk_err;
	}

	rc = clk_enable(l_clks->pixel_clk);
	if (rc) {
		pr_err("Failed to enable dsi pixel clk, rc=%d\n", rc);
		goto pixel_clk_err;
	}

	return rc;

pixel_clk_err:
	clk_disable(l_clks->byte_clk);
byte_clk_err:
	clk_disable(l_clks->esc_clk);
esc_clk_err:
	return rc;
}

static void dsi_link_clk_disable(struct dsi_link_clk_info *l_clks)
{
	clk_disable(l_clks->esc_clk);
	clk_disable(l_clks->pixel_clk);
	clk_disable(l_clks->byte_clk);
}

/**
 * dsi_link_clk_start() - enable dsi link clocks
 */
static int dsi_link_clk_start(struct dsi_link_clk_info *clks)
{
	int rc = 0;

	if (clks->set_new_rate) {
		rc = dsi_link_clk_set_rate(clks);
		if (rc) {
			pr_err("failed to set clk rates, rc = %d\n", rc);
			goto error;
		} else {
			clks->set_new_rate = false;
		}
	}

	rc = dsi_link_clk_prepare(clks);
	if (rc) {
		pr_err("failed to prepare link clks, rc = %d\n", rc);
		goto error;
	}

	rc = dsi_link_clk_enable(clks);
	if (rc) {
		pr_err("failed to enable link clks, rc = %d\n", rc);
		goto error_unprepare;
	}

	pr_debug("Link clocks are enabled\n");
	return rc;
error_unprepare:
	dsi_link_clk_unprepare(clks);
error:
	return rc;
}

/**
 * dsi_link_clk_stop() - Stop DSI link clocks.
 */
static int dsi_link_clk_stop(struct dsi_link_clk_info *clks)
{
	dsi_link_clk_disable(clks);
	dsi_link_clk_unprepare(clks);

	pr_debug("Link clocks disabled\n");

	return 0;
}

/*
 * dsi_pwr_parse_supply_node() - parse power supply node from root device node
 */
static int dsi_pwr_parse_supply_node(struct device_node *root,
				     struct dsi_regulator_info *regs)
{
	int rc = 0;
	int i = 0;
	u32 tmp = 0;
	struct device_node *node = NULL;

	for_each_child_of_node(root, node) {
		const char *st = NULL;

		rc = of_property_read_string(node, "qcom,supply-name", &st);
		if (rc) {
			pr_err("failed to read name, rc = %d\n", rc);
			goto error;
		}

		snprintf(regs->vregs[i].vreg_name,
			 ARRAY_SIZE(regs->vregs[i].vreg_name),
			 "%s", st);

		rc = of_property_read_u32(node, "qcom,supply-min-voltage",
					  &tmp);
		if (rc) {
			pr_err("failed to read min voltage, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].min_voltage = tmp;

		rc = of_property_read_u32(node, "qcom,supply-max-voltage",
					  &tmp);
		if (rc) {
			pr_err("failed to read max voltage, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].max_voltage = tmp;

		rc = of_property_read_u32(node, "qcom,supply-enable-load",
					  &tmp);
		if (rc) {
			pr_err("failed to read enable load, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].enable_load = tmp;

		rc = of_property_read_u32(node, "qcom,supply-disable-load",
					  &tmp);
		if (rc) {
			pr_err("failed to read disable load, rc = %d\n", rc);
			goto error;
		}
		regs->vregs[i].disable_load = tmp;

		/* Optional values */
		rc = of_property_read_u32(node, "qcom,supply-pre-on-sleep",
					  &tmp);
		if (rc) {
			pr_debug("pre-on-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].pre_on_sleep = tmp;
		}

		rc = of_property_read_u32(node, "qcom,supply-pre-off-sleep",
					  &tmp);
		if (rc) {
			pr_debug("pre-off-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].pre_off_sleep = tmp;
		}

		rc = of_property_read_u32(node, "qcom,supply-post-on-sleep",
					  &tmp);
		if (rc) {
			pr_debug("post-on-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].post_on_sleep = tmp;
		}

		rc = of_property_read_u32(node, "qcom,supply-post-off-sleep",
					  &tmp);
		if (rc) {
			pr_debug("post-off-sleep not specified\n");
			rc = 0;
		} else {
			regs->vregs[i].post_off_sleep = tmp;
		}

		++i;
		pr_debug("[%s] minv=%d maxv=%d, en_load=%d, dis_load=%d\n",
			 regs->vregs[i].vreg_name,
			 regs->vregs[i].min_voltage,
			 regs->vregs[i].max_voltage,
			 regs->vregs[i].enable_load,
			 regs->vregs[i].disable_load);
	}

error:
	return rc;
}

/**
 * dsi_pwr_enable_vregs() - enable/disable regulators
 */
static int dsi_pwr_enable_vregs(struct dsi_regulator_info *regs, bool enable)
{
	int rc = 0, i = 0;
	struct dsi_vreg *vreg;
	int num_of_v = 0;

	if (enable) {
		for (i = 0; i < regs->count; i++) {
			vreg = &regs->vregs[i];
			if (vreg->pre_on_sleep)
				msleep(vreg->pre_on_sleep);

			rc = regulator_set_optimum_mode(
						vreg->vreg,
						vreg->enable_load
						);
			if (rc < 0) {
				pr_err("Setting optimum mode failed for %s\n",
				       vreg->vreg_name);
				goto error;
			}
			num_of_v = regulator_count_voltages(vreg->vreg);
			if (num_of_v > 0) {
				rc = regulator_set_voltage(vreg->vreg,
							   vreg->min_voltage,
							   vreg->max_voltage);
				if (rc) {
					pr_err("Set voltage(%s) fail, rc=%d\n",
						 vreg->vreg_name, rc);
					goto error_disable_opt_mode;
				}
			}

			rc = regulator_enable(vreg->vreg);
			if (rc) {
				pr_err("enable failed for %s, rc=%d\n",
				       vreg->vreg_name, rc);
				goto error_disable_voltage;
			}

			if (vreg->post_on_sleep)
				msleep(vreg->post_on_sleep);
		}
	} else {
		for (i = (regs->count - 1); i >= 0; i--) {
			if (regs->vregs[i].pre_off_sleep)
				msleep(regs->vregs[i].pre_off_sleep);

			(void)regulator_set_optimum_mode(
						regs->vregs[i].vreg,
						regs->vregs[i].disable_load
						);
			(void)regulator_disable(regs->vregs[i].vreg);

			if (regs->vregs[i].post_off_sleep)
				msleep(regs->vregs[i].post_off_sleep);
		}
	}

	return 0;
error_disable_opt_mode:
	(void)regulator_set_optimum_mode(regs->vregs[i].vreg,
					 regs->vregs[i].disable_load);

error_disable_voltage:
	if (num_of_v > 0)
		(void)regulator_set_voltage(regs->vregs[i].vreg,
					    0, regs->vregs[i].max_voltage);
error:
	for (i--; i >= 0; i--) {
		if (regs->vregs[i].pre_off_sleep)
			msleep(regs->vregs[i].pre_off_sleep);

		(void)regulator_set_optimum_mode(regs->vregs[i].vreg,
						 regs->vregs[i].disable_load);

		num_of_v = regulator_count_voltages(regs->vregs[i].vreg);
		if (num_of_v > 0)
			(void)regulator_set_voltage(regs->vregs[i].vreg,
					    0, regs->vregs[i].max_voltage);

		(void)regulator_disable(regs->vregs[i].vreg);

		if (regs->vregs[i].post_off_sleep)
			msleep(regs->vregs[i].post_off_sleep);
	}

	return rc;
}

/**
* dsi_clk_pwr_of_get_vreg_data - Parse regulator supply information
* @of_node:        Device of node to parse for supply information.
* @regs:           Pointer where regulator information will be copied to.
* @supply_name:    Name of the supply node.
*
* return: error code in case of failure or 0 for success.
*/
int dsi_clk_pwr_of_get_vreg_data(struct device_node *of_node,
				 struct dsi_regulator_info *regs,
				 char *supply_name)
{
	int rc = 0;
	struct device_node *supply_root_node = NULL;

	if (!of_node || !regs) {
		pr_err("Bad params\n");
		return -EINVAL;
	}

	regs->count = 0;
	supply_root_node = of_get_child_by_name(of_node, supply_name);
	if (!supply_root_node) {
		supply_root_node = of_parse_phandle(of_node, supply_name, 0);
		if (!supply_root_node) {
			pr_err("No supply entry present for %s\n", supply_name);
			return -EINVAL;
		}
	}

	regs->count = of_get_available_child_count(supply_root_node);
	if (regs->count == 0) {
		pr_err("No vregs defined for %s\n", supply_name);
		return -EINVAL;
	}

	regs->vregs = kcalloc(regs->count, sizeof(*regs->vregs), GFP_KERNEL);
	if (!regs->vregs) {
		regs->count = 0;
		return -ENOMEM;
	}

	rc = dsi_pwr_parse_supply_node(supply_root_node, regs);
	if (rc) {
		pr_err("failed to parse supply node for %s, rc = %d\n",
			supply_name, rc);

		kfree(regs->vregs);
		regs->vregs = NULL;
		regs->count = 0;
	}

	return rc;
}

/**
 * dsi_clk_pwr_get_dt_vreg_data - parse regulator supply information
 * @dev:            Device whose of_node needs to be parsed.
 * @regs:           Pointer where regulator information will be copied to.
 * @supply_name:    Name of the supply node.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_pwr_get_dt_vreg_data(struct device *dev,
				 struct dsi_regulator_info *regs,
				 char *supply_name)
{
	int rc = 0;
	struct device_node *of_node = NULL;
	struct device_node *supply_node = NULL;
	struct device_node *supply_root_node = NULL;

	if (!dev || !regs) {
		pr_err("Bad params\n");
		return -EINVAL;
	}

	of_node = dev->of_node;
	regs->count = 0;
	supply_root_node = of_get_child_by_name(of_node, supply_name);
	if (!supply_root_node) {
		supply_root_node = of_parse_phandle(of_node, supply_name, 0);
		if (!supply_root_node) {
			pr_err("No supply entry present for %s\n", supply_name);
			return -EINVAL;
		}
	}

	for_each_child_of_node(supply_root_node, supply_node)
		regs->count++;

	if (regs->count == 0) {
		pr_err("No vregs defined for %s\n", supply_name);
		return -EINVAL;
	}

	regs->vregs = devm_kcalloc(dev, regs->count, sizeof(*regs->vregs),
				   GFP_KERNEL);
	if (!regs->vregs) {
		regs->count = 0;
		return -ENOMEM;
	}

	rc = dsi_pwr_parse_supply_node(supply_root_node, regs);
	if (rc) {
		pr_err("failed to parse supply node for %s, rc = %d\n",
		       supply_name, rc);
		devm_kfree(dev, regs->vregs);
		regs->vregs = NULL;
		regs->count = 0;
	}

	return rc;
}

/**
 * dsi_pwr_enable_regulator() - enable a set of regulators
 * @regs:       Pointer to set of regulators to enable or disable.
 * @enable:     Enable/Disable regulators.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_pwr_enable_regulator(struct dsi_regulator_info *regs, bool enable)
{
	int rc = 0;

	if (enable) {
		if (regs->refcount == 0) {
			rc = dsi_pwr_enable_vregs(regs, true);
			if (rc)
				pr_err("failed to enable regulators\n");
		}
		regs->refcount++;
	} else {
		if (regs->refcount == 0) {
			pr_err("Unbalanced regulator off\n");
		} else {
			regs->refcount--;
			if (regs->refcount == 0) {
				rc = dsi_pwr_enable_vregs(regs, false);
				if (rc)
					pr_err("failed to disable vregs\n");
			}
		}
	}

	return rc;
}

/**
 * dsi_clk_enable_core_clks() - enable DSI core clocks
 * @clks:      DSI core clock information.
 * @enable:    enable/disable DSI core clocks.
 *
 * A ref count is maintained, so caller should make sure disable and enable
 * calls are balanced.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_enable_core_clks(struct dsi_core_clk_info *clks, bool enable)
{
	int rc = 0;

	if (enable)
		rc = INC_REFCOUNT(clks, dsi_core_clk_start);
	else
		rc = DEC_REFCOUNT(clks, dsi_core_clk_stop);

	return rc;
}

/**
 * dsi_clk_enable_link_clks() - enable DSI link clocks
 * @clks:      DSI link clock information.
 * @enable:    enable/disable DSI link clocks.
 *
 * A ref count is maintained, so caller should make sure disable and enable
 * calls are balanced.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_enable_link_clks(struct dsi_link_clk_info *clks, bool enable)
{
	int rc = 0;

	if (enable)
		rc = INC_REFCOUNT(clks, dsi_link_clk_start);
	else
		rc = DEC_REFCOUNT(clks, dsi_link_clk_stop);

	return rc;
}

/**
 * dsi_clk_set_link_frequencies() - set frequencies for link clks
 * @clks:         Link clock information
 * @pixel_clk:    pixel clock frequency in KHz.
 * @byte_clk:     Byte clock frequency in KHz.
 * @esc_clk:      Escape clock frequency in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_link_frequencies(struct dsi_link_clk_info *clks,
				 u64 pixel_clk,
				 u64 byte_clk,
				 u64 esc_clk)
{
	int rc = 0;

	clks->pixel_clk_rate = pixel_clk;
	clks->byte_clk_rate = byte_clk;
	clks->esc_clk_rate = esc_clk;
	clks->set_new_rate = true;

	return rc;
}

/**
 * dsi_clk_set_pixel_clk_rate() - set frequency for pixel clock
 * @clks:      DSI link clock information.
 * @pixel_clk: Pixel clock rate in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_pixel_clk_rate(struct dsi_link_clk_info *clks, u64 pixel_clk)
{
	int rc = 0;

	rc = clk_set_rate(clks->pixel_clk, pixel_clk);
	if (rc)
		pr_err("failed to set clk rate for pixel clk, rc=%d\n", rc);
	else
		clks->pixel_clk_rate = pixel_clk;

	return rc;
}

/**
 * dsi_clk_set_byte_clk_rate() - set frequency for byte clock
 * @clks:      DSI link clock information.
 * @byte_clk: Byte clock rate in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_byte_clk_rate(struct dsi_link_clk_info *clks, u64 byte_clk)
{
	int rc = 0;

	rc = clk_set_rate(clks->byte_clk, byte_clk);
	if (rc)
		pr_err("failed to set clk rate for byte clk, rc=%d\n", rc);
	else
		clks->byte_clk_rate = byte_clk;

	return rc;
}

/**
 * dsi_clk_update_parent() - update parent clocks for specified clock
 * @parent:       link clock pair which are set as parent.
 * @child:        link clock pair whose parent has to be set.
 */
int dsi_clk_update_parent(struct dsi_clk_link_set *parent,
			  struct dsi_clk_link_set *child)
{
	int rc = 0;

	rc = clk_set_parent(child->byte_clk, parent->byte_clk);
	if (rc) {
		pr_err("failed to set byte clk parent\n");
		goto error;
	}

	rc = clk_set_parent(child->pixel_clk, parent->pixel_clk);
	if (rc) {
		pr_err("failed to set pixel clk parent\n");
		goto error;
	}
error:
	return rc;
}
