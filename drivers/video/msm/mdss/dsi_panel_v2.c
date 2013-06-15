/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/qpnp/pin.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include "dsi_v2.h"

#define DT_CMD_HDR 6

struct dsi_panel_private {
	struct dsi_buf dsi_panel_tx_buf;
	struct dsi_buf dsi_panel_rx_buf;

	int rst_gpio;
	int disp_en_gpio;
	int video_mode_gpio;
	int te_gpio;
	char bl_ctrl;

	struct regulator *vddio_vreg;
	struct regulator *vdda_vreg;

	struct dsi_panel_cmds_list *on_cmds_list;
	struct dsi_panel_cmds_list *off_cmds_list;
	struct mdss_dsi_phy_ctrl phy_params;

	char *on_cmds;
	char *off_cmds;
};

static struct dsi_panel_private *panel_private;

DEFINE_LED_TRIGGER(bl_led_trigger);

int dsi_panel_init(void)
{
	int rc;

	if (!panel_private) {
		panel_private = kzalloc(sizeof(struct dsi_panel_private),
					GFP_KERNEL);
		if (!panel_private) {
			pr_err("fail to alloc dsi panel private data\n");
			return -ENOMEM;
		}
	}

	rc = dsi_buf_alloc(&panel_private->dsi_panel_tx_buf,
				ALIGN(DSI_BUF_SIZE,
				SZ_4K));
	if (rc)
		return rc;

	rc = dsi_buf_alloc(&panel_private->dsi_panel_rx_buf,
				ALIGN(DSI_BUF_SIZE,
				SZ_4K));
	if (rc)
		return rc;

	return 0;
}

void dsi_panel_deinit(void)
{
	if (!panel_private)
		return;

	kfree(panel_private->dsi_panel_tx_buf.start);
	kfree(panel_private->dsi_panel_rx_buf.start);

	if (!IS_ERR(panel_private->vddio_vreg))
		devm_regulator_put(panel_private->vddio_vreg);

	if (!IS_ERR(panel_private->vdda_vreg))
		devm_regulator_put(panel_private->vdda_vreg);

	if (panel_private->on_cmds_list) {
		kfree(panel_private->on_cmds_list->buf);
		kfree(panel_private->on_cmds_list);
	}
	if (panel_private->off_cmds_list) {
		kfree(panel_private->off_cmds_list->buf);
		kfree(panel_private->off_cmds_list);
	}

	kfree(panel_private->on_cmds);
	kfree(panel_private->off_cmds);

	kfree(panel_private);
	panel_private = NULL;

	if (bl_led_trigger) {
		led_trigger_unregister_simple(bl_led_trigger);
		bl_led_trigger = NULL;
	}
}
int dsi_panel_power(int enable)
{
	int ret;
	if (enable) {
		ret = regulator_enable(panel_private->vddio_vreg);
		if (ret) {
			pr_err("dsi_panel_power regulator enable vddio fail\n");
			return ret;
		}
		ret = regulator_enable(panel_private->vdda_vreg);
		if (ret) {
			pr_err("dsi_panel_power regulator enable vdda fail\n");
			return ret;
		}
	} else {
		ret = regulator_disable(panel_private->vddio_vreg);
		if (ret) {
			pr_err("dsi_panel_power regulator disable vddio fail\n");
			return ret;
		}
		ret = regulator_disable(panel_private->vdda_vreg);
		if (ret) {
			pr_err("dsi_panel_power regulator dsiable vdda fail\n");
			return ret;
		}
	}
	return 0;
}

void dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (!gpio_is_valid(panel_private->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}

	if (!gpio_is_valid(panel_private->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (enable) {
		dsi_panel_power(1);
		gpio_request(panel_private->rst_gpio, "panel_reset");
		gpio_set_value(panel_private->rst_gpio, 1);
		/*
		 * these delay values are by experiments currently, will need
		 * to move to device tree late
		 */
		msleep(20);
		gpio_set_value(panel_private->rst_gpio, 0);
		udelay(200);
		gpio_set_value(panel_private->rst_gpio, 1);
		msleep(20);
		if (gpio_is_valid(panel_private->disp_en_gpio)) {
			gpio_request(panel_private->disp_en_gpio,
					"panel_enable");
			gpio_set_value(panel_private->disp_en_gpio, 1);
		}
		if (gpio_is_valid(panel_private->video_mode_gpio)) {
			gpio_request(panel_private->video_mode_gpio,
					"panel_video_mdoe");
			if (pdata->panel_info.mipi.mode == DSI_VIDEO_MODE)
				gpio_set_value(panel_private->video_mode_gpio,
						1);
			else
				gpio_set_value(panel_private->video_mode_gpio,
						0);
		}
		if (gpio_is_valid(panel_private->te_gpio))
			gpio_request(panel_private->te_gpio, "panel_te");
	} else {
		gpio_set_value(panel_private->rst_gpio, 0);
		gpio_free(panel_private->rst_gpio);

		if (gpio_is_valid(panel_private->disp_en_gpio)) {
			gpio_set_value(panel_private->disp_en_gpio, 0);
			gpio_free(panel_private->disp_en_gpio);
		}

		if (gpio_is_valid(panel_private->video_mode_gpio))
			gpio_free(panel_private->video_mode_gpio);

		if (gpio_is_valid(panel_private->te_gpio))
			gpio_free(panel_private->te_gpio);
		dsi_panel_power(0);
	}
}

static void dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
				u32 bl_level)
{
	if (panel_private->bl_ctrl) {
		switch (panel_private->bl_ctrl) {
		case BL_WLED:
			led_trigger_event(bl_led_trigger, bl_level);
			break;

		default:
			pr_err("%s: Unknown bl_ctrl configuration\n",
				__func__);
			break;
		}
	} else
		pr_err("%s:%d, bl_ctrl not configured", __func__, __LINE__);
}

static int dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;

	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s:%d, debug info (mode) : %d\n", __func__, __LINE__,
		 mipi->mode);


	return dsi_cmds_tx_v2(pdata, &panel_private->dsi_panel_tx_buf,
					panel_private->on_cmds_list->buf,
					panel_private->on_cmds_list->size);
}

static int dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s:%d, debug info\n", __func__, __LINE__);


	return dsi_cmds_tx_v2(pdata, &panel_private->dsi_panel_tx_buf,
					panel_private->off_cmds_list->buf,
					panel_private->off_cmds_list->size);
}

static int dsi_panel_parse_gpio(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	panel_private->disp_en_gpio = of_get_named_gpio(np,
						"qcom,enable-gpio", 0);
	panel_private->rst_gpio = of_get_named_gpio(np, "qcom,rst-gpio", 0);
	panel_private->video_mode_gpio = of_get_named_gpio(np,
						"qcom,mode-selection-gpio", 0);
	panel_private->te_gpio = of_get_named_gpio(np,
						"qcom,te-gpio", 0);
	return 0;
}

static int dsi_panel_parse_regulator(struct platform_device *pdev)
{
	int ret;
	panel_private->vddio_vreg = devm_regulator_get(&pdev->dev, "vddio");
	if (IS_ERR(panel_private->vddio_vreg)) {
		pr_err("%s: could not get vddio vreg, rc=%ld\n",
			__func__, PTR_ERR(panel_private->vddio_vreg));
		return PTR_ERR(panel_private->vddio_vreg);
	}
	ret = regulator_set_voltage(panel_private->vddio_vreg,
					1800000,
					1800000);
	if (ret) {
		pr_err("%s: set voltage failed on vddio_vreg, rc=%d\n",
			__func__, ret);
		return ret;
	}
	panel_private->vdda_vreg = devm_regulator_get(&pdev->dev, "vdda");
	if (IS_ERR(panel_private->vdda_vreg)) {
		pr_err("%s: could not get vdda_vreg , rc=%ld\n",
			__func__, PTR_ERR(panel_private->vdda_vreg));
		return PTR_ERR(panel_private->vdda_vreg);
	}
	ret = regulator_set_voltage(panel_private->vdda_vreg,
					2850000,
					2850000);
	if (ret) {
		pr_err("%s: set voltage failed on vdda_vreg, rc=%d\n",
			__func__, ret);
		return ret;
	}
	return 0;
}

static int dsi_panel_parse_timing(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	int rc;

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}

	panel_data->panel_info.xres = (!rc ? res[0] : 480);
	panel_data->panel_info.yres = (!rc ? res[1] : 800);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-active-res", res, 2);
	if (rc == 0) {
		panel_data->panel_info.lcdc.xres_pad =
			panel_data->panel_info.xres - res[0];
		panel_data->panel_info.lcdc.yres_pad =
			panel_data->panel_info.yres - res[1];
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, panel bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.bpp = (!rc ? tmp : 24);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-porch-values", res, 6);
	if (rc) {
		pr_err("%s:%d, panel porch not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}

	panel_data->panel_info.lcdc.h_back_porch = (!rc ? res[0] : 6);
	panel_data->panel_info.lcdc.h_pulse_width = (!rc ? res[1] : 2);
	panel_data->panel_info.lcdc.h_front_porch = (!rc ? res[2] : 6);
	panel_data->panel_info.lcdc.v_back_porch = (!rc ? res[3] : 6);
	panel_data->panel_info.lcdc.v_pulse_width = (!rc ? res[4] : 2);
	panel_data->panel_info.lcdc.v_front_porch = (!rc ? res[5] : 6);

	return 0;
}

static int dsi_panel_parse_phy(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	int i, len;
	const char *data;

	data = of_get_property(np, "qcom,panel-phy-regulatorSettings", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		panel_private->phy_params.regulator[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-timingSettings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		panel_private->phy_params.timing[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-strengthCtrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	panel_private->phy_params.strength[0] = data[0];
	panel_private->phy_params.strength[1] = data[1];

	data = of_get_property(np, "qcom,panel-phy-bistCtrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		panel_private->phy_params.bistCtrl[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-laneConfig", &len);
	if ((!data) || (len != 30)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		panel_private->phy_params.laneCfg[i] = data[i];

	panel_data->panel_info.mipi.dsi_phy_db = &panel_private->phy_params;
	return 0;
}

static int dsi_panel_parse_init_cmds(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	int i, len;
	int cmd_plen, data_offset;
	const char *data;
	const char *on_cmds_state, *off_cmds_state;
	int num_of_on_cmds = 0, num_of_off_cmds = 0;

	data = of_get_property(np, "qcom,panel-on-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read ON cmds", __func__, __LINE__);
		return -EINVAL;
	}

	panel_private->on_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!panel_private->on_cmds)
		return -ENOMEM;

	memcpy(panel_private->on_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = panel_private->on_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_on_cmds++;
	}
	if (!num_of_on_cmds) {
		pr_err("%s:%d, No ON cmds specified", __func__, __LINE__);
		return -EINVAL;
	}

	panel_private->on_cmds_list =
		kzalloc(sizeof(struct dsi_panel_cmds_list), GFP_KERNEL);
	if (!panel_private->on_cmds_list)
		return -ENOMEM;

	panel_private->on_cmds_list->buf =
		kzalloc((num_of_on_cmds * sizeof(struct dsi_cmd_desc)),
			GFP_KERNEL);
	if (!panel_private->on_cmds_list->buf)
		return -ENOMEM;

	data_offset = 0;
	for (i = 0; i < num_of_on_cmds; i++) {
		panel_private->on_cmds_list->buf[i].dtype =
					panel_private->on_cmds[data_offset++];
		panel_private->on_cmds_list->buf[i].last =
					panel_private->on_cmds[data_offset++];
		panel_private->on_cmds_list->buf[i].vc =
					panel_private->on_cmds[data_offset++];
		panel_private->on_cmds_list->buf[i].ack =
					panel_private->on_cmds[data_offset++];
		panel_private->on_cmds_list->buf[i].wait =
					panel_private->on_cmds[data_offset++];
		panel_private->on_cmds_list->buf[i].dlen =
					panel_private->on_cmds[data_offset++];
		panel_private->on_cmds_list->buf[i].payload =
					&panel_private->on_cmds[data_offset];
		data_offset += (panel_private->on_cmds_list->buf[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect ON command entries",
						__func__, __LINE__);
		return -EINVAL;
	}

	panel_private->on_cmds_list->size = num_of_on_cmds;

	on_cmds_state = of_get_property(pdev->dev.of_node,
					"qcom,on-cmds-dsi-state", NULL);
	if (!strncmp(on_cmds_state, "DSI_LP_MODE", 11)) {
		panel_private->on_cmds_list->ctrl_state = DSI_LP_MODE;
	} else if (!strncmp(on_cmds_state, "DSI_HS_MODE", 11)) {
		panel_private->on_cmds_list->ctrl_state = DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
							__func__);
		panel_private->on_cmds_list->ctrl_state = DSI_LP_MODE;
	}

	panel_data->dsi_panel_on_cmds = panel_private->on_cmds_list;

	data = of_get_property(np, "qcom,panel-off-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read OFF cmds", __func__, __LINE__);
		return -EINVAL;
	}

	panel_private->off_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!panel_private->off_cmds)
		return -ENOMEM;

	memcpy(panel_private->off_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = panel_private->off_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_off_cmds++;
	}
	if (!num_of_off_cmds) {
		pr_err("%s:%d, No OFF cmds specified", __func__, __LINE__);
		return -ENOMEM;
	}

	panel_private->off_cmds_list =
		kzalloc(sizeof(struct dsi_panel_cmds_list), GFP_KERNEL);
	if (!panel_private->off_cmds_list)
		return -ENOMEM;

	panel_private->off_cmds_list->buf = kzalloc(num_of_off_cmds
					* sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!panel_private->off_cmds_list->buf)
		return -ENOMEM;

	data_offset = 0;
	for (i = 0; i < num_of_off_cmds; i++) {
		panel_private->off_cmds_list->buf[i].dtype =
					panel_private->off_cmds[data_offset++];
		panel_private->off_cmds_list->buf[i].last =
					panel_private->off_cmds[data_offset++];
		panel_private->off_cmds_list->buf[i].vc =
					panel_private->off_cmds[data_offset++];
		panel_private->off_cmds_list->buf[i].ack =
					panel_private->off_cmds[data_offset++];
		panel_private->off_cmds_list->buf[i].wait =
					panel_private->off_cmds[data_offset++];
		panel_private->off_cmds_list->buf[i].dlen =
					panel_private->off_cmds[data_offset++];
		panel_private->off_cmds_list->buf[i].payload =
					&panel_private->off_cmds[data_offset];
		data_offset += (panel_private->off_cmds_list->buf[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect OFF command entries",
						__func__, __LINE__);
		return -EINVAL;
	}

	panel_private->off_cmds_list->size = num_of_off_cmds;
	off_cmds_state = of_get_property(pdev->dev.of_node,
				"qcom,off-cmds-dsi-state", NULL);
	if (!strncmp(off_cmds_state, "DSI_LP_MODE", 11)) {
		panel_private->off_cmds_list->ctrl_state =
						DSI_LP_MODE;
	} else if (!strncmp(off_cmds_state, "DSI_HS_MODE", 11)) {
		panel_private->off_cmds_list->ctrl_state = DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
							__func__);
		panel_private->off_cmds_list->ctrl_state = DSI_LP_MODE;
	}

	panel_data->dsi_panel_off_cmds = panel_private->off_cmds_list;

	return 0;
}

static int dsi_panel_parse_backlight(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data,
				char *bl_ctrl)
{
	int rc;
	u32 res[6];
	static const char *bl_ctrl_type;

	bl_ctrl_type = of_get_property(pdev->dev.of_node,
				  "qcom,mdss-pan-bl-ctrl", NULL);
	if ((bl_ctrl_type) && (!strncmp(bl_ctrl_type, "bl_ctrl_wled", 12))) {
		led_trigger_register_simple("bkl-trigger", &bl_led_trigger);
		pr_debug("%s: SUCCESS-> WLED TRIGGER register\n", __func__);
		*bl_ctrl = BL_WLED;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
		"qcom,mdss-pan-bl-levels", res, 2);
	panel_data->panel_info.bl_min = (!rc ? res[0] : 0);
	panel_data->panel_info.bl_max = (!rc ? res[1] : 255);
	return rc;
}

static int dsi_panel_parse_other(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data)
{
	const char *pdest;
	u32 tmp;
	int rc;

	pdest = of_get_property(pdev->dev.of_node,
				"qcom,mdss-pan-dest", NULL);
	if (strlen(pdest) != 9) {
		pr_err("%s: Unknown pdest specified\n", __func__);
		return -EINVAL;
	}
	if (!strncmp(pdest, "display_1", 9)) {
		panel_data->panel_info.pdest = DISPLAY_1;
	} else if (!strncmp(pdest, "display_2", 9)) {
		panel_data->panel_info.pdest = DISPLAY_2;
	} else {
		pr_debug("%s: pdest not specified. Set Default\n",
							__func__);
		panel_data->panel_info.pdest = DISPLAY_1;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-pan-underflow-clr", &tmp);
	panel_data->panel_info.lcdc.underflow_clr = (!rc ? tmp : 0xff);

	return rc;
}

static int dsi_panel_parse_host_cfg(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	int rc;

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mode", &tmp);
	panel_data->panel_info.mipi.mode = (!rc ? tmp : DSI_VIDEO_MODE);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-h-pulse-mode", &tmp);
	panel_data->panel_info.mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-h-power-stop", res, 3);
	panel_data->panel_info.mipi.hbp_power_stop = (!rc ? res[0] : false);
	panel_data->panel_info.mipi.hsa_power_stop = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.hfp_power_stop = (!rc ? res[2] : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-bllp-power-stop", res, 2);
	panel_data->panel_info.mipi.bllp_power_stop =
					(!rc ? res[0] : false);
	panel_data->panel_info.mipi.eof_bllp_power_stop =
					(!rc ? res[1] : false);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-traffic-mode", &tmp);
	panel_data->panel_info.mipi.traffic_mode =
			(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-dst-format", &tmp);
	panel_data->panel_info.mipi.dst_format =
			(!rc ? tmp : DSI_VIDEO_DST_FORMAT_RGB888);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-vc", &tmp);
	panel_data->panel_info.mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-rgb-swap", &tmp);
	panel_data->panel_info.mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-data-lanes", res, 4);
	panel_data->panel_info.mipi.data_lane0 = (!rc ? res[0] : true);
	panel_data->panel_info.mipi.data_lane1 = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.data_lane2 = (!rc ? res[2] : false);
	panel_data->panel_info.mipi.data_lane3 = (!rc ? res[3] : false);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dlane-swap", &tmp);
	panel_data->panel_info.mipi.dlane_swap = (!rc ? tmp : 0);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-dsi-t-clk", res, 2);
	panel_data->panel_info.mipi.t_clk_pre = (!rc ? res[0] : 0x24);
	panel_data->panel_info.mipi.t_clk_post = (!rc ? res[1] : 0x03);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-stream", &tmp);
	panel_data->panel_info.mipi.stream = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-te-sel", &tmp);
	panel_data->panel_info.mipi.te_sel = (!rc ? tmp : 1);

	rc = of_property_read_u32(np, "qcom,mdss-pan-insert-dcs-cmd", &tmp);
	panel_data->panel_info.mipi.insert_dcs_cmd = (!rc ? tmp : 1);

	rc = of_property_read_u32(np, "qcom,mdss-pan-wr-mem-continue", &tmp);
	panel_data->panel_info.mipi.wr_mem_continue = (!rc ? tmp : 0x3c);
	rc = of_property_read_u32(np, "qcom,mdss-pan-wr-mem-start", &tmp);
	panel_data->panel_info.mipi.wr_mem_start = (!rc ? tmp : 0x2c);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mdp-tr", &tmp);
	panel_data->panel_info.mipi.mdp_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.mdp_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dma-tr", &tmp);
	panel_data->panel_info.mipi.dma_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.dma_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-frame-rate", &tmp);
	panel_data->panel_info.mipi.frame_rate = (!rc ? tmp : 60);
	return rc;
}

static int dsi_panel_parse_dt(struct platform_device *pdev,
				struct dsi_panel_common_pdata *panel_data,
				char *bl_ctrl)
{
	int rc;

	rc = dsi_panel_parse_gpio(pdev);
	if (rc) {
		pr_err("fail to parse panel GPIOs\n");
		return rc;
	}

	rc = dsi_panel_parse_regulator(pdev);
	if (rc) {
		pr_err("fail to parse panel regulators\n");
		return rc;
	}

	rc = dsi_panel_parse_timing(pdev, panel_data);
	if (rc) {
		pr_err("fail to parse panel timing\n");
		return rc;
	}

	rc = dsi_panel_parse_phy(pdev, panel_data);
	if (rc) {
		pr_err("fail to parse DSI PHY settings\n");
		return rc;
	}

	rc = dsi_panel_parse_backlight(pdev, panel_data, bl_ctrl);
	if (rc) {
		pr_err("fail to parse DSI backlight\n");
		return rc;
	}

	rc = dsi_panel_parse_other(pdev, panel_data);
	if (rc) {
		pr_err("fail to parse DSI panel destination\n");
		return rc;
	}

	rc = dsi_panel_parse_host_cfg(pdev, panel_data);
	if (rc) {
		pr_err("fail to parse DSI host configs\n");
		return rc;
	}

	rc = dsi_panel_parse_init_cmds(pdev, panel_data);
	if (rc) {
		pr_err("fail to parse DSI init commands\n");
		return rc;
	}

	return 0;
}

static int __devinit dsi_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct dsi_panel_common_pdata vendor_pdata;
	static const char *panel_name;

	pr_debug("%s:%d, debug info id=%d", __func__, __LINE__, pdev->id);
	if (!pdev->dev.of_node)
		return -ENODEV;

	panel_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!panel_name)
		pr_debug("%s:%d, panel name not specified\n",
						__func__, __LINE__);
	else
		pr_debug("%s: Panel Name = %s\n", __func__, panel_name);

	rc = dsi_panel_init();
	if (rc) {
		pr_err("dsi_panel_init failed %d\n", rc);
		goto dsi_panel_probe_error;

	}
	rc = dsi_panel_parse_dt(pdev, &vendor_pdata, &panel_private->bl_ctrl);
	if (rc) {
		pr_err("dsi_panel_parse_dt failed %d\n", rc);
		goto dsi_panel_probe_error;
	}

	vendor_pdata.on = dsi_panel_on;
	vendor_pdata.off = dsi_panel_off;
	vendor_pdata.reset = dsi_panel_reset;
	vendor_pdata.bl_fnc = dsi_panel_bl_ctrl;

	rc = dsi_panel_device_register_v2(pdev, &vendor_pdata,
					panel_private->bl_ctrl);

	if (rc) {
		pr_err("dsi_panel_device_register_v2 failed %d\n", rc);
		goto dsi_panel_probe_error;
	}

	return 0;
dsi_panel_probe_error:
	dsi_panel_deinit();
	return rc;
}

static int __devexit dsi_panel_remove(struct platform_device *pdev)
{
	dsi_panel_deinit();
	return 0;
}


static const struct of_device_id dsi_panel_match[] = {
	{.compatible = "qcom,dsi-panel-v2"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = dsi_panel_probe,
	.remove = __devexit_p(dsi_panel_remove),
	.driver = {
		.name = "dsi_v2_panel",
		.of_match_table = dsi_panel_match,
	},
};

static int __init dsi_panel_module_init(void)
{
	return platform_driver_register(&this_driver);
}
module_init(dsi_panel_module_init);
