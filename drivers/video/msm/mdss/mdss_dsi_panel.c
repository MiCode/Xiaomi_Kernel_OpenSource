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
#include <linux/qpnp/pin.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>

#include "mdss_dsi.h"

#define DT_CMD_HDR 6

static struct dsi_buf dsi_panel_tx_buf;
static struct dsi_buf dsi_panel_rx_buf;

DEFINE_LED_TRIGGER(bl_led_trigger);

static struct mdss_dsi_phy_ctrl phy_params;

void mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (enable) {
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		wmb();
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		udelay(200);
		wmb();
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		wmb();
		gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
		wmb();
	} else {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
	}
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (ctrl_pdata->bl_ctrl) {
		switch (ctrl_pdata->bl_ctrl) {
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

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s:%d, debug info (mode) : %d\n", __func__, __LINE__,
		 mipi->mode);

	if (mipi->mode == DSI_VIDEO_MODE) {
		mdss_dsi_cmds_tx(pdata, &dsi_panel_tx_buf,
				 ctrl_pdata->on_cmds->buf,
				 ctrl_pdata->on_cmds->size);
	} else {
		pr_err("%s:%d, CMD MODE NOT SUPPORTED", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s:%d, debug info\n", __func__, __LINE__);

	if (mipi->mode == DSI_VIDEO_MODE) {
		mdss_dsi_cmds_tx(pdata, &dsi_panel_tx_buf,
				 ctrl_pdata->off_cmds->buf,
				 ctrl_pdata->off_cmds->size);
	} else {
		pr_debug("%s:%d, CMD mode not supported", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int mdss_panel_parse_dt(struct platform_device *pdev,
			       struct mdss_panel_common_pdata *panel_data,
			       char *bl_ctrl)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	int rc, i, len;
	int cmd_plen, data_offset;
	const char *data;
	static const char *bl_ctrl_type, *pdest;
	static const char *on_cmds_state, *off_cmds_state;
	char *on_cmds = NULL, *off_cmds = NULL;
	int num_of_on_cmds = 0, num_of_off_cmds = 0;

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.xres = (!rc ? res[0] : 640);
	panel_data->panel_info.yres = (!rc ? res[1] : 480);

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

	pdest = of_get_property(pdev->dev.of_node,
				"qcom,mdss-pan-dest", NULL);
	if (strlen(pdest) != 9) {
		pr_err("%s: Unknown pdest specified\n", __func__);
		return -EINVAL;
	}
	if (!strncmp(pdest, "display_1", 9))
		panel_data->panel_info.pdest = DISPLAY_1;
	else if (!strncmp(pdest, "display_2", 9))
		panel_data->panel_info.pdest = DISPLAY_2;
	else {
		pr_debug("%s: pdest not specified. Set Default\n",
							__func__);
		panel_data->panel_info.pdest = DISPLAY_1;
	}

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-porch-values", res, 6);
	panel_data->panel_info.lcdc.h_back_porch = (!rc ? res[0] : 6);
	panel_data->panel_info.lcdc.h_pulse_width = (!rc ? res[1] : 2);
	panel_data->panel_info.lcdc.h_front_porch = (!rc ? res[2] : 6);
	panel_data->panel_info.lcdc.v_back_porch = (!rc ? res[3] : 6);
	panel_data->panel_info.lcdc.v_pulse_width = (!rc ? res[4] : 2);
	panel_data->panel_info.lcdc.v_front_porch = (!rc ? res[5] : 6);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-underflow-clr", &tmp);
	panel_data->panel_info.lcdc.underflow_clr = (!rc ? tmp : 0xff);

	bl_ctrl_type = of_get_property(pdev->dev.of_node,
				  "qcom,mdss-pan-bl-ctrl", NULL);
	if ((bl_ctrl_type) && (!strncmp(bl_ctrl_type, "bl_ctrl_wled", 12))) {
		led_trigger_register_simple("bkl-trigger", &bl_led_trigger);
		pr_debug("%s: SUCCESS-> WLED TRIGGER register\n", __func__);
		*bl_ctrl = BL_WLED;
	}

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-bl-levels", res, 2);
	panel_data->panel_info.bl_min = (!rc ? res[0] : 0);
	panel_data->panel_info.bl_max = (!rc ? res[1] : 255);

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

	data = of_get_property(np, "qcom,panel-phy-regulatorSettings", &len);
	if ((!data) || (len != 7)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.regulator[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-timingSettings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.timing[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-strengthCtrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
		       __func__, __LINE__);
		goto error;
	}
	phy_params.strength[0] = data[0];
	phy_params.strength[1] = data[1];

	data = of_get_property(np, "qcom,panel-phy-bistCtrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.bistCtrl[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-laneConfig", &len);
	if ((!data) || (len != 45)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		phy_params.laneCfg[i] = data[i];

	panel_data->panel_info.mipi.dsi_phy_db = &phy_params;

	data = of_get_property(np, "qcom,panel-on-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read ON cmds", __func__, __LINE__);
		goto error;
	}

	on_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!on_cmds)
		return -ENOMEM;

	memcpy(on_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = on_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_on_cmds++;
	}
	if (!num_of_on_cmds) {
		pr_err("%s:%d, No ON cmds specified", __func__, __LINE__);
		goto error;
	}

	panel_data->dsi_panel_on_cmds =
		kzalloc(sizeof(struct dsi_panel_cmds_list), GFP_KERNEL);
	if (!panel_data->dsi_panel_on_cmds)
		return -ENOMEM;

	(panel_data->dsi_panel_on_cmds)->buf =
		kzalloc((num_of_on_cmds * sizeof(struct dsi_cmd_desc)),
						GFP_KERNEL);
	if (!(panel_data->dsi_panel_on_cmds)->buf)
		return -ENOMEM;

	data_offset = 0;
	for (i = 0; i < num_of_on_cmds; i++) {
		panel_data->dsi_panel_on_cmds->buf[i].dtype =
						on_cmds[data_offset++];
		panel_data->dsi_panel_on_cmds->buf[i].last =
						on_cmds[data_offset++];
		panel_data->dsi_panel_on_cmds->buf[i].vc =
						on_cmds[data_offset++];
		panel_data->dsi_panel_on_cmds->buf[i].ack =
						on_cmds[data_offset++];
		panel_data->dsi_panel_on_cmds->buf[i].wait =
						on_cmds[data_offset++];
		panel_data->dsi_panel_on_cmds->buf[i].dlen =
						on_cmds[data_offset++];
		panel_data->dsi_panel_on_cmds->buf[i].payload =
						&on_cmds[data_offset];
		data_offset += (panel_data->dsi_panel_on_cmds->buf[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect ON command entries",
						__func__, __LINE__);
		goto error;
	}

	(panel_data->dsi_panel_on_cmds)->size = num_of_on_cmds;

	on_cmds_state = of_get_property(pdev->dev.of_node,
				"qcom,on-cmds-dsi-state", NULL);
	if (!strncmp(on_cmds_state, "DSI_LP_MODE", 11)) {
		(panel_data->dsi_panel_on_cmds)->ctrl_state =
						DSI_LP_MODE;
	} else if (!strncmp(on_cmds_state, "DSI_HS_MODE", 11)) {
		(panel_data->dsi_panel_on_cmds)->ctrl_state =
						DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
							__func__);
		(panel_data->dsi_panel_on_cmds)->ctrl_state =
						DSI_LP_MODE;
	}

	data = of_get_property(np, "qcom,panel-off-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read OFF cmds", __func__, __LINE__);
		goto error;
	}

	off_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!off_cmds)
		return -ENOMEM;

	memcpy(off_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = off_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_off_cmds++;
	}
	if (!num_of_off_cmds) {
		pr_err("%s:%d, No OFF cmds specified", __func__, __LINE__);
		goto error;
	}

	panel_data->dsi_panel_off_cmds =
		kzalloc(sizeof(struct dsi_panel_cmds_list), GFP_KERNEL);
	if (!panel_data->dsi_panel_off_cmds)
		return -ENOMEM;

	(panel_data->dsi_panel_off_cmds)->buf = kzalloc(num_of_off_cmds
				* sizeof(struct dsi_cmd_desc),
					GFP_KERNEL);
	if (!(panel_data->dsi_panel_off_cmds)->buf)
		return -ENOMEM;

	data_offset = 0;
	for (i = 0; i < num_of_off_cmds; i++) {
		panel_data->dsi_panel_off_cmds->buf[i].dtype =
						off_cmds[data_offset++];
		panel_data->dsi_panel_off_cmds->buf[i].last =
						off_cmds[data_offset++];
		panel_data->dsi_panel_off_cmds->buf[i].vc =
						off_cmds[data_offset++];
		panel_data->dsi_panel_off_cmds->buf[i].ack =
						off_cmds[data_offset++];
		panel_data->dsi_panel_off_cmds->buf[i].wait =
						off_cmds[data_offset++];
		panel_data->dsi_panel_off_cmds->buf[i].dlen =
						off_cmds[data_offset++];
		panel_data->dsi_panel_off_cmds->buf[i].payload =
						&off_cmds[data_offset];
		data_offset += (panel_data->dsi_panel_off_cmds->buf[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect OFF command entries",
						__func__, __LINE__);
		goto error;
	}

	(panel_data->dsi_panel_off_cmds)->size = num_of_off_cmds;

	off_cmds_state = of_get_property(pdev->dev.of_node,
				"qcom,off-cmds-dsi-state", NULL);
	if (!strncmp(off_cmds_state, "DSI_LP_MODE", 11)) {
		(panel_data->dsi_panel_off_cmds)->ctrl_state =
						DSI_LP_MODE;
	} else if (!strncmp(off_cmds_state, "DSI_HS_MODE", 11)) {
		(panel_data->dsi_panel_off_cmds)->ctrl_state =
						DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
							__func__);
		(panel_data->dsi_panel_off_cmds)->ctrl_state =
						DSI_LP_MODE;
	}

	return 0;
error:
	kfree((panel_data->dsi_panel_on_cmds)->buf);
	kfree((panel_data->dsi_panel_off_cmds)->buf);
	kfree(panel_data->dsi_panel_on_cmds);
	kfree(panel_data->dsi_panel_off_cmds);
	kfree(on_cmds);
	kfree(off_cmds);

	return -EINVAL;
}

static int __devinit mdss_dsi_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct mdss_panel_common_pdata vendor_pdata;
	static const char *panel_name;
	char bl_ctrl = UNKNOWN_CTRL;

	pr_debug("%s:%d, debug info id=%d", __func__, __LINE__, pdev->id);
	if (!pdev->dev.of_node)
		return -ENODEV;

	panel_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!panel_name)
		pr_info("%s:%d, panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	rc = mdss_panel_parse_dt(pdev, &vendor_pdata, &bl_ctrl);
	if (rc)
		return rc;

	vendor_pdata.on = mdss_dsi_panel_on;
	vendor_pdata.off = mdss_dsi_panel_off;
	vendor_pdata.bl_fnc = mdss_dsi_panel_bl_ctrl;

	rc = dsi_panel_device_register(pdev, &vendor_pdata, bl_ctrl);
	if (rc)
		return rc;

	return 0;
}

static const struct of_device_id mdss_dsi_panel_match[] = {
	{.compatible = "qcom,mdss-dsi-panel"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = mdss_dsi_panel_probe,
	.driver = {
		.name   = "dsi_panel",
		.of_match_table = mdss_dsi_panel_match,
	},
};

static int __init mdss_dsi_panel_init(void)
{
	mdss_dsi_buf_alloc(&dsi_panel_tx_buf, ALIGN(DSI_BUF_SIZE, SZ_4K));
	mdss_dsi_buf_alloc(&dsi_panel_rx_buf, ALIGN(DSI_BUF_SIZE, SZ_4K));

	return platform_driver_register(&this_driver);
}
module_init(mdss_dsi_panel_init);
