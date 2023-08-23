/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <drm/drm_edid.h>
#include <soc/qcom/msm_dp_aux_bridge.h>
#include <soc/qcom/msm_dp_mst_sim_helper.h>

struct dp_sim_device {
	struct msm_dp_aux_bridge bridge;
	void *host_dev;
	int (*hpd_cb)(void *, bool, bool);
};

#define to_dp_sim_dev(x) container_of((x), struct dp_sim_device, bridge)

static const struct msm_dp_mst_sim_port output_port = {
	false, false, true, 3, false, 0x12,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	0, 0, 2520, 2520, NULL, 0
};

static int dp_sim_register_hpd(struct msm_dp_aux_bridge *bridge,
	int (*hpd_cb)(void *, bool, bool), void *dev)
{
	struct dp_sim_device *sim_dev = to_dp_sim_dev(bridge);

	sim_dev->host_dev = dev;
	sim_dev->hpd_cb = hpd_cb;

	return 0;
}

static ssize_t dp_sim_transfer(struct msm_dp_aux_bridge *bridge,
	struct drm_dp_aux *drm_aux,
	struct drm_dp_aux_msg *msg)
{
	struct dp_sim_device *sim_dev = to_dp_sim_dev(bridge);
	int ret;

	ret = msm_dp_mst_sim_transfer(sim_dev->bridge.mst_ctx, msg);
	if (ret < 0)
		ret = drm_aux->transfer(drm_aux, msg);
	else
		ret = msg->size;

	return ret;
}

static void dp_sim_host_hpd_irq(void *host_dev)
{
	struct dp_sim_device *sim_dev = host_dev;

	if (sim_dev->hpd_cb)
		sim_dev->hpd_cb(sim_dev->host_dev, false, true);
}

static void dp_sim_update_dtd(struct edid *edid,
		struct drm_display_mode *mode)
{
	struct detailed_timing *dtd = &edid->detailed_timings[0];
	struct detailed_pixel_timing *pd = &dtd->data.pixel_data;
	u32 h_blank = mode->htotal - mode->hdisplay;
	u32 v_blank = mode->vtotal - mode->vdisplay;
	u32 h_img = 0, v_img = 0;

	dtd->pixel_clock = cpu_to_le16(mode->clock / 10);

	pd->hactive_lo = mode->hdisplay & 0xFF;
	pd->hblank_lo = h_blank & 0xFF;
	pd->hactive_hblank_hi = ((h_blank >> 8) & 0xF) |
			((mode->hdisplay >> 8) & 0xF) << 4;

	pd->vactive_lo = mode->vdisplay & 0xFF;
	pd->vblank_lo = v_blank & 0xFF;
	pd->vactive_vblank_hi = ((v_blank >> 8) & 0xF) |
			((mode->vdisplay >> 8) & 0xF) << 4;

	pd->hsync_offset_lo =
		(mode->hsync_start - mode->hdisplay) & 0xFF;
	pd->hsync_pulse_width_lo =
		(mode->hsync_end - mode->hsync_start) & 0xFF;
	pd->vsync_offset_pulse_width_lo =
		(((mode->vsync_start - mode->vdisplay) & 0xF) << 4) |
		((mode->vsync_end - mode->vsync_start) & 0xF);

	pd->hsync_vsync_offset_pulse_width_hi =
		((((mode->hsync_start - mode->hdisplay) >> 8) & 0x3) << 6) |
		((((mode->hsync_end - mode->hsync_start) >> 8) & 0x3) << 4) |
		((((mode->vsync_start - mode->vdisplay) >> 4) & 0x3) << 2) |
		((((mode->vsync_end - mode->vsync_start) >> 4) & 0x3) << 0);

	pd->width_mm_lo = h_img & 0xFF;
	pd->height_mm_lo = v_img & 0xFF;
	pd->width_height_mm_hi = (((h_img >> 8) & 0xF) << 4) |
		((v_img >> 8) & 0xF);

	pd->hborder = 0;
	pd->vborder = 0;
	pd->misc = 0;
}

static void dp_sim_update_checksum(struct edid *edid)
{
	u8 *data = (u8 *)edid;
	u32 i, sum = 0;

	for (i = 0; i < EDID_LENGTH - 1; i++)
		sum += data[i];

	edid->checksum = 0x100 - (sum & 0xFF);
}

static int dp_sim_parse(struct dp_sim_device *sim_dev)
{
	struct device_node *of_node = sim_dev->bridge.of_node;
	struct device_node *node;
	struct msm_dp_mst_sim_port *ports;
	struct drm_display_mode mode_buf, *mode = &mode_buf;
	u32 h_front_porch, h_pulse_width, h_back_porch;
	u32 v_front_porch, v_pulse_width, v_back_porch;
	bool h_active_high, v_active_high;
	u32 flags = 0;
	int rc, port_num, i;
	struct edid *edid;

	const u8 edid_buf[EDID_LENGTH] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x44, 0x6D,
		0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1B, 0x10, 0x01, 0x03,
		0x80, 0x50, 0x2D, 0x78, 0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47,
		0x98, 0x27, 0x12, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
	};

	port_num = of_get_child_count(of_node);

	if (!port_num)
		return 0;

	if (port_num >= 15)
		return -EINVAL;

	ports = kcalloc(port_num, sizeof(*ports), GFP_KERNEL);
	if (!ports)
		return -ENOMEM;

	i = 0;
	for_each_child_of_node(of_node, node) {
		rc = of_property_read_u32(node, "qcom,mode-h-active",
						&mode->hdisplay);
		if (rc) {
			pr_err("failed to read h-active, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "qcom,mode-h-front-porch",
						&h_front_porch);
		if (rc) {
			pr_err("failed to read h-front-porch, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "qcom,mode-h-pulse-width",
						&h_pulse_width);
		if (rc) {
			pr_err("failed to read h-pulse-width, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "qcom,mode-h-back-porch",
						&h_back_porch);
		if (rc) {
			pr_err("failed to read h-back-porch, rc=%d\n", rc);
			goto fail;
		}

		h_active_high = of_property_read_bool(node,
						"qcom,mode-h-active-high");

		rc = of_property_read_u32(node, "qcom,mode-v-active",
						&mode->vdisplay);
		if (rc) {
			pr_err("failed to read v-active, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "qcom,mode-v-front-porch",
						&v_front_porch);
		if (rc) {
			pr_err("failed to read v-front-porch, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "qcom,mode-v-pulse-width",
						&v_pulse_width);
		if (rc) {
			pr_err("failed to read v-pulse-width, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "qcom,mode-v-back-porch",
						&v_back_porch);
		if (rc) {
			pr_err("failed to read v-back-porch, rc=%d\n", rc);
			goto fail;
		}

		v_active_high = of_property_read_bool(node,
						"qcom,mode-v-active-high");

		rc = of_property_read_u32(node, "qcom,mode-refresh-rate",
						&mode->vrefresh);
		if (rc) {
			pr_err("failed to read refresh-rate, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "qcom,mode-clock-in-khz",
						&mode->clock);
		if (rc) {
			pr_err("failed to read clock, rc=%d\n", rc);
			goto fail;
		}

		mode->hsync_start = mode->hdisplay + h_front_porch;
		mode->hsync_end = mode->hsync_start + h_pulse_width;
		mode->htotal = mode->hsync_end + h_back_porch;
		mode->vsync_start = mode->vdisplay + v_front_porch;
		mode->vsync_end = mode->vsync_start + v_pulse_width;
		mode->vtotal = mode->vsync_end + v_back_porch;
		if (h_active_high)
			flags |= DRM_MODE_FLAG_PHSYNC;
		else
			flags |= DRM_MODE_FLAG_NHSYNC;
		if (v_active_high)
			flags |= DRM_MODE_FLAG_PVSYNC;
		else
			flags |= DRM_MODE_FLAG_NVSYNC;
		mode->flags = flags;

		edid = kzalloc(sizeof(*edid), GFP_KERNEL);
		if (!edid) {
			rc = -ENOMEM;
			goto fail;
		}

		memcpy(edid, edid_buf, sizeof(edid_buf));
		dp_sim_update_dtd(edid, mode);
		dp_sim_update_checksum(edid);

		memcpy(&ports[i], &output_port, sizeof(*ports));
		ports[i].peer_guid[0] = i;
		ports[i].edid = (u8 *)edid;
		ports[i].edid_size = sizeof(*edid);
		i++;
	}

	rc = msm_dp_mst_sim_update(sim_dev->bridge.mst_ctx, port_num, ports);

fail:
	for (i = 0; i < port_num; i++)
		kfree(ports[i].edid);
	kfree(ports);

	return rc;
}

static int dp_sim_probe(struct platform_device *pdev)
{
	struct dp_sim_device *dp_sim_dev;
	struct msm_dp_mst_sim_cfg cfg;
	int ret;

	dp_sim_dev = devm_kzalloc(&pdev->dev, sizeof(*dp_sim_dev), GFP_KERNEL);
	if (!dp_sim_dev)
		return -ENOMEM;

	dp_sim_dev->bridge.of_node = pdev->dev.of_node;
	dp_sim_dev->bridge.register_hpd = dp_sim_register_hpd;
	dp_sim_dev->bridge.transfer = dp_sim_transfer;
	dp_sim_dev->bridge.dev_priv = dp_sim_dev;
	dp_sim_dev->bridge.flag = MSM_DP_AUX_BRIDGE_MST;

	memset(&cfg, 0, sizeof(cfg));
	cfg.host_dev = dp_sim_dev;
	cfg.host_hpd_irq = dp_sim_host_hpd_irq;

	ret = msm_dp_mst_sim_create(&cfg, &dp_sim_dev->bridge.mst_ctx);
	if (ret)
		return ret;

	ret = dp_sim_parse(dp_sim_dev);
	if (ret)
		goto fail;

	ret = msm_dp_aux_add_bridge(&dp_sim_dev->bridge);
	if (ret)
		goto fail;

	platform_set_drvdata(pdev, dp_sim_dev);

	return 0;

fail:
	msm_dp_mst_sim_destroy(dp_sim_dev->bridge.mst_ctx);
	return ret;
}

static int dp_sim_remove(struct platform_device *pdev)
{
	struct dp_sim_device *dp_sim_dev;

	dp_sim_dev = platform_get_drvdata(pdev);
	if (!dp_sim_dev)
		return 0;

	msm_dp_mst_sim_destroy(dp_sim_dev->bridge.mst_ctx);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,dp-mst-sim"},
	{},
};

static struct platform_driver dp_sim_driver = {
	.probe = dp_sim_probe,
	.remove = dp_sim_remove,
	.driver = {
		.name = "dp_sim",
		.of_match_table = dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init dp_sim_register(void)
{
	return platform_driver_register(&dp_sim_driver);
}

static void __exit dp_sim_unregister(void)
{
	platform_driver_unregister(&dp_sim_driver);
}

module_init(dp_sim_register);
module_exit(dp_sim_unregister);

