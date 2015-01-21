/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/iopoll.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include "dsi_v2.h"

static struct dsi_interface dsi_intf;

static int dsi_off(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("turn off dsi controller\n");
	if (dsi_intf.off)
		rc = dsi_intf.off(pdata);

	if (rc) {
		pr_err("mdss_dsi_off DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}

static int dsi_on(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("dsi_on DSI controller on\n");
	if (dsi_intf.on)
		rc = dsi_intf.on(pdata);

	if (rc) {
		pr_err("mdss_dsi_on DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}

static int dsi_update_pconfig(struct mdss_panel_data *pdata,
				int mode)
{
	int ret = 0;
	struct mdss_panel_info *pinfo = &pdata->panel_info;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (!pdata)
		return -ENODEV;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (mode == DSI_CMD_MODE) {
		pinfo->mipi.mode = DSI_CMD_MODE;
		pinfo->type = MIPI_CMD_PANEL;
		pinfo->mipi.vsync_enable = 1;
		pinfo->mipi.hw_vsync_mode = 1;
	} else {
		pinfo->mipi.mode = DSI_VIDEO_MODE;
		pinfo->type = MIPI_VIDEO_PANEL;
		pinfo->mipi.vsync_enable = 0;
		pinfo->mipi.hw_vsync_mode = 0;
	}

	ctrl_pdata->panel_mode = pinfo->mipi.mode;
	mdss_panel_get_dst_fmt(pinfo->bpp, pinfo->mipi.mode,
			pinfo->mipi.pixel_packing, &(pinfo->mipi.dst_format));
	pinfo->cont_splash_enabled = 0;

	return ret;
}

static int dsi_panel_handler(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("dsi_panel_handler enable=%d\n", enable);
	if (!pdata)
		return -ENODEV;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (enable &&
		(pdata->panel_info.panel_power_state == MDSS_PANEL_POWER_OFF)) {
		if (!pdata->panel_info.dynamic_switch_pending) {
			mdss_dsi_panel_reset(pdata, 1);
			rc = ctrl_pdata->on(pdata);
			if (rc)
				pr_err("dsi_panel_handler panel on failed %d\n",
									rc);
		}
		pdata->panel_info.panel_power_state = MDSS_PANEL_POWER_ON;
		if (pdata->panel_info.type == MIPI_CMD_PANEL)
			mdss_dsi_set_tear_on(ctrl_pdata);
	} else if (!enable &&
		(pdata->panel_info.panel_power_state == MDSS_PANEL_POWER_ON)) {
		msm_dsi_sw_reset();
		if (dsi_intf.op_mode_config)
			dsi_intf.op_mode_config(DSI_CMD_MODE, pdata);
		if (pdata->panel_info.dynamic_switch_pending) {
			pr_info("%s: switching to %s mode\n", __func__,
			(pdata->panel_info.mipi.mode ? "video" : "command"));
			if (pdata->panel_info.type == MIPI_CMD_PANEL) {
				ctrl_pdata->switch_mode(pdata, DSI_VIDEO_MODE);
			} else if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
				ctrl_pdata->switch_mode(pdata, DSI_CMD_MODE);
				mdss_dsi_set_tear_off(ctrl_pdata);
			}
		}
		pdata->panel_info.panel_power_state = MDSS_PANEL_POWER_OFF;
		if (!pdata->panel_info.dynamic_switch_pending) {
			rc = ctrl_pdata->off(pdata);
			mdss_dsi_panel_reset(pdata, 0);
		}
	}
	return rc;
}

static int dsi_splash_on(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("%s:\n", __func__);

	if (dsi_intf.cont_on)
		rc = dsi_intf.cont_on(pdata);

	if (rc) {
		pr_err("mdss_dsi_on DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}

static int dsi_clk_ctrl(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;

	pr_debug("%s:\n", __func__);

	if (dsi_intf.clk_ctrl)
		rc = dsi_intf.clk_ctrl(pdata, enable);

	return rc;
}

static int dsi_event_handler(struct mdss_panel_data *pdata,
				int event, void *arg)
{
	int rc = 0;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -ENODEV;
	}

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = dsi_on(pdata);
		break;
	case MDSS_EVENT_BLANK:
		rc = dsi_off(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		rc = dsi_panel_handler(pdata, 1);
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = dsi_panel_handler(pdata, 0);
		break;
	case MDSS_EVENT_CONT_SPLASH_BEGIN:
		rc = dsi_splash_on(pdata);
		break;
	case MDSS_EVENT_PANEL_CLK_CTRL:
		rc = dsi_clk_ctrl(pdata, (int)arg);
		break;
	case MDSS_EVENT_DSI_UPDATE_PANEL_DATA:
		rc = dsi_update_pconfig(pdata, (int)(unsigned long) arg);
		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	return rc;
}

static int dsi_parse_gpio(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct device_node *np = pdev->dev.of_node;

	ctrl_pdata->disp_en_gpio = of_get_named_gpio(np,
		"qcom,platform-enable-gpio", 0);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio))
		pr_err("%s:%d, Disp_en gpio not specified\n",
						__func__, __LINE__);

	ctrl_pdata->rst_gpio = of_get_named_gpio(np,
					"qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio))
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);

	ctrl_pdata->mode_gpio = -1;
	if (ctrl_pdata->panel_data.panel_info.mode_gpio_state !=
						MODE_GPIO_NOT_VALID) {
		ctrl_pdata->mode_gpio = of_get_named_gpio(np,
						"qcom,platform-mode-gpio", 0);
		if (!gpio_is_valid(ctrl_pdata->mode_gpio))
			pr_info("%s:%d, reset gpio not specified\n",
							__func__, __LINE__);
	}

	ctrl_pdata->bklt_en_gpio = of_get_named_gpio(np,
					"qcom,platform-bklight-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		pr_err("%s:%d, bklt_en gpio not specified\n",
						__func__, __LINE__);

	return 0;
}

static void mdss_dsi_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
}

static int mdss_dsi_get_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp, enum dsi_pm_type module)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_node = NULL;
	const char *pm_supply_name = NULL;
	struct device_node *supply_root_node = NULL;

	if (!dev || !mp) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	of_node = dev->of_node;

	mp->num_vreg = 0;
	pm_supply_name = __mdss_dsi_pm_supply_node_name(module);
	supply_root_node = of_get_child_by_name(of_node, pm_supply_name);
	if (!supply_root_node) {
		pr_err("no supply entry present\n");
		goto novreg;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		mp->num_vreg++;
	}

	if (mp->num_vreg == 0) {
		pr_debug("%s: no vreg\n", __func__);
		goto novreg;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, mp->num_vreg);
	}

	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
		mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		pr_err("%s: can't alloc vreg mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string(supply_node,
			"qcom,supply-name", &st);
		if (rc) {
			pr_err("%s: error reading name. rc=%d\n",
				__func__, rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name,
			ARRAY_SIZE((mp->vreg_config[i].vreg_name)), "%s", st);
		/* vreg-min-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading min volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = tmp;

		/* vreg-max-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading max volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = tmp;

		/* enable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading enable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].enable_load = tmp;

		/* disable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading disable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].disable_load = tmp;

		/* pre-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_off_sleep = tmp;
		}

		/* post-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_off_sleep = tmp;
		}

		pr_debug("%s: %s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
			__func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].enable_load,
			mp->vreg_config[i].disable_load,
			mp->vreg_config[i].pre_on_sleep,
			mp->vreg_config[i].post_on_sleep,
			mp->vreg_config[i].pre_off_sleep,
			mp->vreg_config[i].post_off_sleep
			);
		++i;
	}

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
novreg:
	mp->num_vreg = 0;

	return rc;
}

static int dsi_parse_phy(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct device_node *np = pdev->dev.of_node;
	int i, len;
	const char *data;
	struct mdss_dsi_phy_ctrl *phy_db
		= &(ctrl_pdata->panel_data.panel_info.mipi.dsi_phy_db);

	data = of_get_property(np, "qcom,platform-regulator-settings", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_db->regulator[i] = data[i];

	data = of_get_property(np, "qcom,platform-strength-ctrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	phy_db->strength[0] = data[0];
	phy_db->strength[1] = data[1];

	data = of_get_property(np, "qcom,platform-bist-ctrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_db->bistctrl[i] = data[i];

	data = of_get_property(np, "qcom,platform-lane-config", &len);
	if ((!data) || (len != 30)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_db->lanecfg[i] = data[i];

	return 0;
}

void dsi_ctrl_config_deinit(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int i;

	for (i = DSI_MAX_PM - 1; i >= 0; i--) {
		mdss_dsi_put_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i]);
	}
}

int dsi_ctrl_config_init(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0, i;

	for (i = 0; i < DSI_MAX_PM; i++) {
		rc = mdss_dsi_get_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
				__func__, __mdss_dsi_pm_name(i), rc);
			return rc;
		}
	}

	rc = dsi_parse_gpio(pdev, ctrl_pdata);
	if (rc) {
		pr_err("fail to parse panel GPIOs\n");
		return rc;
	}

	rc = dsi_parse_phy(pdev, ctrl_pdata);
	if (rc) {
		pr_err("fail to parse DSI PHY settings\n");
		return rc;
	}

	return 0;
}
int dsi_panel_device_register_v2(struct platform_device *dev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mipi_panel_info *mipi;
	int rc;
	u8 lanes = 0, bpp;
	u32 h_period, v_period;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	h_period = ((pinfo->lcdc.h_pulse_width)
			+ (pinfo->lcdc.h_back_porch)
			+ (pinfo->xres)
			+ (pinfo->lcdc.h_front_porch));

	v_period = ((pinfo->lcdc.v_pulse_width)
			+ (pinfo->lcdc.v_back_porch)
			+ (pinfo->yres)
			+ (pinfo->lcdc.v_front_porch));

	mipi  = &pinfo->mipi;

	pinfo->type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	if (mipi->data_lane3)
		lanes += 1;
	if (mipi->data_lane2)
		lanes += 1;
	if (mipi->data_lane1)
		lanes += 1;
	if (mipi->data_lane0)
		lanes += 1;

	if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
		|| (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB888)
		|| (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB666_LOOSE))
		bpp = 3;
	else if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
		|| (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB565))
		bpp = 2;
	else
		bpp = 3; /* Default format set to RGB888 */

	if (pinfo->type == MIPI_VIDEO_PANEL &&
		!pinfo->clk_rate) {
		h_period += pinfo->lcdc.xres_pad;
		v_period += pinfo->lcdc.yres_pad;

		if (lanes > 0) {
			pinfo->clk_rate =
			((h_period * v_period * (mipi->frame_rate) * bpp * 8)
			   / lanes);
		} else {
			pr_err("%s: forcing mdss_dsi lanes to 1\n", __func__);
			pinfo->clk_rate =
				(h_period * v_period
					 * (mipi->frame_rate) * bpp * 8);
		}
	}

	ctrl_pdata->panel_data.event_handler = dsi_event_handler;

	/*
	 * register in mdp driver
	 */
	rc = mdss_register_panel(dev, &(ctrl_pdata->panel_data));
	if (rc) {
		dev_err(&dev->dev, "unable to register MIPI DSI panel\n");
		return rc;
	}

	pr_debug("%s: Panal data initialized\n", __func__);
	return 0;
}

void dsi_register_interface(struct dsi_interface *intf)
{
	dsi_intf = *intf;
}

int dsi_buf_alloc(struct dsi_buf *dp, int size)
{
	dp->start = kzalloc(size, GFP_KERNEL);
	if (dp->start == NULL) {
		pr_err("%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}

	dp->end = dp->start + size;
	dp->size = size;

	if ((int)dp->start & 0x07) {
		pr_err("%s: buf NOT 8 bytes aligned\n", __func__);
		return -EINVAL;
	}

	dp->data = dp->start;
	dp->len = 0;
	return 0;
}

