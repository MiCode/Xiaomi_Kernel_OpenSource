/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include "mtk_cec.h"
#include "mtk_hdmi.h"
#include "mtk_hdmi_hw.h"

static const char * const mtk_hdmi_clk_names[MTK_HDMI_CLK_COUNT] = {
	[MTK_HDMI_CLK_HDMI_PIXEL] = "pixel",
	[MTK_HDMI_CLK_HDMI_PLL] = "pll",
	[MTK_HDMI_CLK_AUD_BCLK] = "bclk",
	[MTK_HDMI_CLK_AUD_SPDIF] = "spdif",
};

static const enum mtk_hdmi_clk_id mtk_hdmi_enable_clocks[] = {
	MTK_HDMI_CLK_AUD_BCLK,
	MTK_HDMI_CLK_AUD_SPDIF,
};

static int mtk_hdmi_get_all_clk(struct mtk_hdmi *hdmi,
				struct device_node *np)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_clk_names); i++) {
		hdmi->clk[i] = of_clk_get_by_name(np,
						  mtk_hdmi_clk_names[i]);
		if (IS_ERR(hdmi->clk[i]))
			return PTR_ERR(hdmi->clk[i]);
	}
	return 0;
}

static int mtk_hdmi_clk_enable_audio(struct mtk_hdmi *hdmi)
{
	int ret;

	ret = clk_prepare_enable(hdmi->clk[MTK_HDMI_CLK_AUD_BCLK]);
	if (ret)
		return ret;

	ret = clk_prepare_enable(hdmi->clk[MTK_HDMI_CLK_AUD_SPDIF]);
	if (ret)
		goto err;

	return 0;
err:
	clk_disable_unprepare(hdmi->clk[MTK_HDMI_CLK_AUD_BCLK]);
	return ret;
}

static void mtk_hdmi_clk_disable_audio(struct mtk_hdmi *hdmi)
{
	clk_disable_unprepare(hdmi->clk[MTK_HDMI_CLK_AUD_BCLK]);
	clk_disable_unprepare(hdmi->clk[MTK_HDMI_CLK_AUD_SPDIF]);
}

static enum drm_connector_status hdmi_conn_detect(struct drm_connector *conn,
						  bool force)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	return mtk_hdmi_hpd_high(hdmi) ?
	       connector_status_connected : connector_status_disconnected;
}

static void hdmi_conn_destroy(struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	mtk_cec_set_hpd_event(hdmi->cec_dev, NULL, NULL);

	drm_connector_unregister(conn);
	drm_connector_cleanup(conn);
}

static int hdmi_conn_set_property(struct drm_connector *conn,
				  struct drm_property *property, uint64_t val)
{
	return 0;
}

static int mtk_hdmi_conn_get_modes(struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);
	struct edid *edid;
	int ret;

	if (!hdmi->ddc_adpt)
		return -ENODEV;

	edid = drm_get_edid(conn, hdmi->ddc_adpt);
	if (!edid)
		return -ENODEV;

	hdmi->dvi_mode = !drm_detect_hdmi_monitor(edid);

	drm_mode_connector_update_edid_property(conn, edid);

	ret = drm_add_edid_modes(conn, edid);
	kfree(edid);
	return ret;
}

static int mtk_hdmi_conn_mode_valid(struct drm_connector *conn,
				    struct drm_display_mode *mode)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	dev_info(hdmi->dev, "xres=%d, yres=%d, refresh=%d, intl=%d clock=%d\n",
		 mode->hdisplay, mode->vdisplay, mode->vrefresh,
		 !!(mode->flags & DRM_MODE_FLAG_INTERLACE), mode->clock * 1000);

	if (hdmi->bridge.next) {
		struct drm_display_mode adjusted_mode;

		drm_mode_copy(&adjusted_mode, mode);
		if (!drm_bridge_mode_fixup(hdmi->bridge.next, mode,
					   &adjusted_mode))
			return MODE_BAD;
	}

	if (mode->clock >= 27000 &&
	    mode->clock <= 297000 &&
	    mode->hdisplay <= 0x1fff &&
	    mode->vdisplay <= 0x1fff)
		return MODE_OK;

	return MODE_BAD;
}

static struct drm_encoder *mtk_hdmi_conn_best_enc(struct drm_connector *conn)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_conn(conn);

	return hdmi->bridge.encoder;
}

static const struct drm_connector_funcs mtk_hdmi_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.detect = hdmi_conn_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = hdmi_conn_destroy,
	.set_property = hdmi_conn_set_property,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs
		mtk_hdmi_connector_helper_funcs = {
	.get_modes = mtk_hdmi_conn_get_modes,
	.mode_valid = mtk_hdmi_conn_mode_valid,
	.best_encoder = mtk_hdmi_conn_best_enc,
};

static void mtk_hdmi_hpd_event(bool hpd, void *data)
{
	struct mtk_hdmi *hdmi = data;

	if (hdmi && hdmi->bridge.encoder && hdmi->bridge.encoder->dev)
		drm_helper_hpd_irq_event(hdmi->bridge.encoder->dev);
}

/*
 * Bridge callbacks
 */

static int mtk_hdmi_bridge_attach(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);
	int ret;

	ret = drm_connector_init(bridge->encoder->dev, &hdmi->conn,
				 &mtk_hdmi_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		dev_err(hdmi->dev, "Failed to initialize connector: %d\n", ret);
		return ret;
	}
	drm_connector_helper_add(&hdmi->conn, &mtk_hdmi_connector_helper_funcs);

	hdmi->conn.polled = DRM_CONNECTOR_POLL_HPD;
	hdmi->conn.interlace_allowed = true;
	hdmi->conn.doublescan_allowed = false;

	ret = drm_connector_register(&hdmi->conn);
	if (ret) {
		dev_err(hdmi->dev, "Failed to register connector: %d\n", ret);
		return ret;
	}

	ret = drm_mode_connector_attach_encoder(&hdmi->conn,
						bridge->encoder);
	if (ret) {
		dev_err(hdmi->dev,
			"Failed to attach connector to encoder: %d\n", ret);
		return ret;
	}

	if (bridge->next) {
		bridge->next->encoder = bridge->encoder;
		ret = drm_bridge_attach(bridge->encoder->dev, bridge->next);
		if (ret) {
			dev_err(hdmi->dev,
				"Failed to attach external bridge: %d\n", ret);
			return ret;
		}
	}

	mtk_cec_set_hpd_event(hdmi->cec_dev, mtk_hdmi_hpd_event, hdmi);

	return 0;
}

static bool mtk_hdmi_bridge_mode_fixup(struct drm_bridge *bridge,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void mtk_hdmi_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	clk_disable_unprepare(hdmi->clk[MTK_HDMI_CLK_HDMI_PIXEL]);
	clk_disable_unprepare(hdmi->clk[MTK_HDMI_CLK_HDMI_PLL]);
}

static void mtk_hdmi_bridge_post_disable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	mtk_hdmi_power_off(hdmi);
}

static void mtk_hdmi_bridge_mode_set(struct drm_bridge *bridge,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	dev_info(hdmi->dev, "cur info: name:%s, hdisplay:%d\n",
		 adjusted_mode->name, adjusted_mode->hdisplay);
	dev_info(hdmi->dev, "hsync_start:%d,hsync_end:%d, htotal:%d",
		 adjusted_mode->hsync_start, adjusted_mode->hsync_end,
		 adjusted_mode->htotal);
	dev_info(hdmi->dev, "hskew:%d, vdisplay:%d\n",
		 adjusted_mode->hskew, adjusted_mode->vdisplay);
	dev_info(hdmi->dev, "vsync_start:%d, vsync_end:%d, vtotal:%d",
		 adjusted_mode->vsync_start, adjusted_mode->vsync_end,
		 adjusted_mode->vtotal);
	dev_info(hdmi->dev, "vscan:%d, flag:%d\n",
		 adjusted_mode->vscan, adjusted_mode->flags);

	drm_mode_copy(&hdmi->mode, adjusted_mode);
}

static void mtk_hdmi_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	mtk_hdmi_power_on(hdmi);
}

static void mtk_hdmi_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_bridge(bridge);

	mtk_hdmi_output_set_display_mode(hdmi, &hdmi->mode);
	clk_prepare_enable(hdmi->clk[MTK_HDMI_CLK_HDMI_PLL]);
	clk_prepare_enable(hdmi->clk[MTK_HDMI_CLK_HDMI_PIXEL]);
}

static const struct drm_bridge_funcs mtk_hdmi_bridge_funcs = {
	.attach = mtk_hdmi_bridge_attach,
	.mode_fixup = mtk_hdmi_bridge_mode_fixup,
	.disable = mtk_hdmi_bridge_disable,
	.post_disable = mtk_hdmi_bridge_post_disable,
	.mode_set = mtk_hdmi_bridge_mode_set,
	.pre_enable = mtk_hdmi_bridge_pre_enable,
	.enable = mtk_hdmi_bridge_enable,
};

static int mtk_hdmi_dt_parse_pdata(struct mtk_hdmi *hdmi,
				   struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *cec_np, *port, *ep, *remote, *i2c_np;
	struct platform_device *cec_pdev;
	struct regmap *regmap;
	struct resource *mem;
	int ret;

	hdmi->flt_n_5v_gpio = of_get_named_gpio(np, "flt_n_5v-gpios", 0);
	if (hdmi->flt_n_5v_gpio < 0) {
		dev_warn(dev, "Failed to get flt_n_5v gpio: %d\n",
			hdmi->flt_n_5v_gpio);
	}

	ret = mtk_hdmi_get_all_clk(hdmi, np);
	if (ret) {
		dev_err(dev, "Failed to get clocks: %d\n", ret);
		return ret;
	}

	cec_np = of_parse_phandle(np, "cec", 0);
	if (!cec_np) {
		dev_err(dev, "Failed to find CEC node\n");
		return -EINVAL;
	}

	cec_pdev = of_find_device_by_node(cec_np);
	if (!cec_pdev) {
		dev_err(hdmi->dev, "Waiting for CEC device %s\n",
			cec_np->full_name);
		return -EPROBE_DEFER;
	}
	hdmi->cec_dev = &cec_pdev->dev;

	/*
	 * The mediatek,syscon-hdmi property contains a phandle link to the
	 * MMSYS_CONFIG device and the register offset of the HDMI_SYS_CFG
	 * registers it contains.
	 */
	regmap = syscon_regmap_lookup_by_phandle(np, "mediatek,syscon-hdmi");
	ret = of_property_read_u32_index(np, "mediatek,syscon-hdmi", 1,
					 &hdmi->sys_offset);
	if (IS_ERR(regmap))
		ret = PTR_ERR(regmap);
	if (ret) {
		ret = PTR_ERR(regmap);
		dev_err(dev,
			"Failed to get system configuration registers: %d\n",
			ret);
		return ret;
	}
	hdmi->sys_regmap = regmap;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdmi->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(hdmi->regs)) {
		dev_err(dev, "Failed to ioremap hdmi_shell: %ld\n",
			PTR_ERR(hdmi->regs));
		return PTR_ERR(hdmi->regs);
	}

	port = of_graph_get_port_by_id(np, 1);
	if (port) {
		ep = of_get_child_by_name(port, "endpoint");
		if (!ep) {
			dev_err(dev, "Missing endpoint node in port %s\n",
				port->full_name);
			of_node_put(port);
			return -EINVAL;
		}
		of_node_put(port);

		remote = of_graph_get_remote_port_parent(ep);
		if (!remote) {
			dev_err(dev, "Missing connector/bridge node for endpoint %s\n",
				ep->full_name);
			of_node_put(ep);
			return -EINVAL;
		}
		of_node_put(ep);

		if (!of_device_is_compatible(remote, "hdmi-connector")) {
			hdmi->bridge.next = of_drm_find_bridge(remote);
			if (!hdmi->bridge.next) {
				dev_err(dev, "Waiting for external bridge\n");
				of_node_put(remote);
				return -EPROBE_DEFER;
			}
		}

		i2c_np = of_parse_phandle(remote, "ddc-i2c-bus", 0);
		if (!i2c_np) {
			dev_err(dev, "Failed to find ddc-i2c-bus node in %s\n",
				remote->full_name);
			of_node_put(remote);
			return -EINVAL;
		}
		of_node_put(remote);
	} else {
		i2c_np = of_parse_phandle(np, "ddc-i2c-bus", 0);
		if (!i2c_np) {
			dev_err(dev, "Failed to find ddc-i2c-bus node\n");
			return -EINVAL;
		}
	}

	hdmi->ddc_adpt = of_find_i2c_adapter_by_node(i2c_np);
	if (!hdmi->ddc_adpt) {
		dev_err(dev, "Failed to get ddc i2c adapter by node\n");
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t hdmi_flt_n_5v_irq_thread(int irq, void *arg)
{
	struct mtk_hdmi *hdmi = arg;

	dev_err(hdmi->dev, "detected 5v pin error status\n");
	return IRQ_HANDLED;
}

static int mtk_drm_hdmi_probe(struct platform_device *pdev)
{
	struct mtk_hdmi *hdmi;
	struct device *dev = &pdev->dev;
	int ret;
	struct mtk_hdmi_audio_data audio_data;
	struct platform_device_info audio_pdev_info;

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	hdmi->dev = dev;

	ret = mtk_hdmi_dt_parse_pdata(hdmi, pdev);
	if (ret)
		return ret;

	if (hdmi->flt_n_5v_gpio > 0) {
		hdmi->flt_n_5v_irq = gpio_to_irq(hdmi->flt_n_5v_gpio);
		if (hdmi->flt_n_5v_irq < 0) {
			dev_err(dev, "hdmi->flt_n_5v_irq = %d\n",
				hdmi->flt_n_5v_irq);
			return hdmi->flt_n_5v_irq;
		}

		ret = devm_request_threaded_irq(dev, hdmi->flt_n_5v_irq,
						NULL, hdmi_flt_n_5v_irq_thread,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"hdmi flt_n_5v", hdmi);
		if (ret) {
			dev_err(dev, "Failed to register hdmi flt_n_5v interrupt\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, hdmi);

	ret = mtk_drm_hdmi_debugfs_init(hdmi);
	if (ret) {
		dev_err(dev, "Failed to initialize hdmi debugfs\n");
		return ret;
	}

	ret = mtk_hdmi_output_init(hdmi);
	if (ret) {
		dev_err(dev, "Failed to initialize hdmi output\n");
		return ret;
	}

	memset(&audio_data, 0, sizeof(audio_data));
	audio_data.irq = mtk_cec_irq(hdmi->cec_dev);
	audio_data.mtk_hdmi = hdmi;
	audio_data.enable = mtk_hdmi_audio_enable;
	audio_data.disable = mtk_hdmi_audio_disable;
	audio_data.set_audio_param = mtk_hdmi_audio_set_param;
	audio_data.hpd_detect = mtk_hdmi_hpd_high;
	audio_data.detect_dvi_monitor = mtk_hdmi_detect_dvi_monitor;

	memset(&audio_pdev_info, 0, sizeof(audio_pdev_info));
	audio_pdev_info.parent = dev;
	audio_pdev_info.id = PLATFORM_DEVID_NONE;
	audio_pdev_info.name = "mtk-hdmi-codec";
	audio_pdev_info.dma_mask = DMA_BIT_MASK(32);
	audio_pdev_info.data = &audio_data;
	audio_pdev_info.size_data = sizeof(audio_data);
	hdmi->audio_pdev = platform_device_register_full(&audio_pdev_info);
	if (IS_ERR(hdmi->audio_pdev))
		dev_err(dev, "Failed to register audio device: %ld\n", PTR_ERR(hdmi->audio_pdev));

	hdmi->bridge.funcs = &mtk_hdmi_bridge_funcs;
	hdmi->bridge.of_node = pdev->dev.of_node;
	ret = drm_bridge_add(&hdmi->bridge);
	if (ret) {
		dev_err(dev, "failed to add bridge, ret = %d\n", ret);
		goto err_debugfs_exit;
	}

	ret = mtk_hdmi_clk_enable_audio(hdmi);
	if (ret) {
		dev_err(dev, "Failed to enable audio clocks: %d\n", ret);
		goto err_bridge_remove;
	}

	dev_info(dev, "mediatek hdmi probe success\n");
	return 0;

err_bridge_remove:
	drm_bridge_remove(&hdmi->bridge);
err_debugfs_exit:
	mtk_drm_hdmi_debugfs_exit(hdmi);
	return ret;
}

static int mtk_drm_hdmi_remove(struct platform_device *pdev)
{
	struct mtk_hdmi *hdmi = platform_get_drvdata(pdev);

	drm_bridge_remove(&hdmi->bridge);
	platform_device_unregister(hdmi->audio_pdev);
	platform_set_drvdata(pdev, NULL);
	mtk_drm_hdmi_debugfs_exit(hdmi);
	mtk_hdmi_clk_disable_audio(hdmi);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_hdmi_suspend(struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);

	mtk_hdmi_power_off(hdmi);
	mtk_hdmi_clk_disable_audio(hdmi);
	dev_info(dev, "hdmi suspend success!\n");
	return 0;
}

static int mtk_hdmi_resume(struct device *dev)
{
	struct mtk_hdmi *hdmi = dev_get_drvdata(dev);
	int ret = 0;

	ret = mtk_hdmi_clk_enable_audio(hdmi);
	if (ret) {
		dev_err(dev, "hdmi resume failed!\n");
		return ret;
	}

	mtk_hdmi_power_on(hdmi);
	mtk_hdmi_output_set_display_mode(hdmi, &hdmi->mode);
	dev_info(dev, "hdmi resume success!\n");
	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(mtk_hdmi_pm_ops,
			 mtk_hdmi_suspend, mtk_hdmi_resume);

static const struct of_device_id mtk_drm_hdmi_of_ids[] = {
	{ .compatible = "mediatek,mt8173-hdmi", },
	{ .compatible = "mediatek,mt2701-hdmi", },
	{}
};

static struct platform_driver mtk_hdmi_driver = {
	.probe = mtk_drm_hdmi_probe,
	.remove = mtk_drm_hdmi_remove,
	.driver = {
		.name = "mediatek-drm-hdmi",
		.of_match_table = mtk_drm_hdmi_of_ids,
		.pm = &mtk_hdmi_pm_ops,
	},
};

static struct platform_driver * const mtk_hdmi_drivers[] = {
	&mtk_hdmi_ddc_driver,
	&mtk_cec_driver,
	&mtk_hdmi_driver,
};

static int __init mtk_hdmitx_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_hdmi_drivers); i++) {
		ret = platform_driver_register(mtk_hdmi_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
			       mtk_hdmi_drivers[i]->driver.name, ret);
			goto err;
		}
	}

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_hdmi_drivers[i]);

	return ret;
}

static void __exit mtk_hdmitx_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_hdmi_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_hdmi_drivers[i]);
}

module_init(mtk_hdmitx_init);
module_exit(mtk_hdmitx_exit);

MODULE_AUTHOR("Jie Qiu <jie.qiu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek HDMI Driver");
MODULE_LICENSE("GPL v2");
