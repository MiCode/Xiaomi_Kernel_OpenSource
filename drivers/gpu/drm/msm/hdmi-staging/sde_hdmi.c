/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"sde-hdmi:[%s] " fmt, __func__

#include <linux/list.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_irq.h>

#include "sde_kms.h"
#include "msm_drv.h"
#include "sde_hdmi.h"

static DEFINE_MUTEX(sde_hdmi_list_lock);
static LIST_HEAD(sde_hdmi_list);

static const struct of_device_id sde_hdmi_dt_match[] = {
	{.compatible = "qcom,hdmi-display"},
	{}
};

static ssize_t _sde_hdmi_debugfs_dump_info_read(struct file *file,
						char __user *buff,
						size_t count,
						loff_t *ppos)
{
	struct sde_hdmi *display = file->private_data;
	char *buf;
	u32 len = 0;

	if (!display)
		return -ENODEV;

	if (*ppos)
		return 0;

	buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf, SZ_4K, "name = %s\n", display->name);

	if (copy_to_user(buff, buf, len)) {
		kfree(buf);
		return -EFAULT;
	}

	*ppos += len;

	kfree(buf);
	return len;
}


static const struct file_operations dump_info_fops = {
	.open = simple_open,
	.read = _sde_hdmi_debugfs_dump_info_read,
};

static int _sde_hdmi_debugfs_init(struct sde_hdmi *display)
{
	int rc = 0;
	struct dentry *dir, *dump_file;

	dir = debugfs_create_dir(display->name, NULL);
	if (!dir) {
		rc = -ENOMEM;
		SDE_ERROR("[%s]debugfs create dir failed, rc = %d\n",
			display->name, rc);
		goto error;
	}

	dump_file = debugfs_create_file("dump_info",
					0444,
					dir,
					display,
					&dump_info_fops);
	if (IS_ERR_OR_NULL(dump_file)) {
		rc = PTR_ERR(dump_file);
		SDE_ERROR("[%s]debugfs create file failed, rc=%d\n",
		       display->name, rc);
		goto error_remove_dir;
	}

	display->root = dir;
	return rc;
error_remove_dir:
	debugfs_remove(dir);
error:
	return rc;
}

static int _sde_hdmi_debugfs_deinit(struct sde_hdmi *display)
{
	debugfs_remove(display->root);

	return 0;
}

static void _sde_hdmi_phy_reset(struct hdmi *hdmi)
{
	unsigned int val;

	val = hdmi_read(hdmi, REG_HDMI_PHY_CTRL);

	if (val & HDMI_PHY_CTRL_SW_RESET_LOW) {
		/* pull low */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val & ~HDMI_PHY_CTRL_SW_RESET);
	} else {
		/* pull high */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val | HDMI_PHY_CTRL_SW_RESET);
	}

	if (val & HDMI_PHY_CTRL_SW_RESET_PLL_LOW) {
		/* pull low */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val & ~HDMI_PHY_CTRL_SW_RESET_PLL);
	} else {
		/* pull high */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val | HDMI_PHY_CTRL_SW_RESET_PLL);
	}

	msleep(100);

	if (val & HDMI_PHY_CTRL_SW_RESET_LOW) {
		/* pull high */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val | HDMI_PHY_CTRL_SW_RESET);
	} else {
		/* pull low */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val & ~HDMI_PHY_CTRL_SW_RESET);
	}

	if (val & HDMI_PHY_CTRL_SW_RESET_PLL_LOW) {
		/* pull high */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val | HDMI_PHY_CTRL_SW_RESET_PLL);
	} else {
		/* pull low */
		hdmi_write(hdmi, REG_HDMI_PHY_CTRL,
				val & ~HDMI_PHY_CTRL_SW_RESET_PLL);
	}
}

static int _sde_hdmi_gpio_config(struct hdmi *hdmi, bool on)
{
	const struct hdmi_platform_config *config = hdmi->config;
	int ret;

	if (on) {
		if (config->ddc_clk_gpio != -1) {
			ret = gpio_request(config->ddc_clk_gpio,
				"HDMI_DDC_CLK");
			if (ret) {
				SDE_ERROR("'%s'(%d) gpio_request failed: %d\n",
					"HDMI_DDC_CLK", config->ddc_clk_gpio,
					ret);
				goto error_ddc_clk_gpio;
			}
			gpio_set_value_cansleep(config->ddc_clk_gpio, 1);
		}

		if (config->ddc_data_gpio != -1) {
			ret = gpio_request(config->ddc_data_gpio,
				"HDMI_DDC_DATA");
			if (ret) {
				SDE_ERROR("'%s'(%d) gpio_request failed: %d\n",
					"HDMI_DDC_DATA", config->ddc_data_gpio,
					ret);
				goto error_ddc_data_gpio;
			}
			gpio_set_value_cansleep(config->ddc_data_gpio, 1);
		}

		ret = gpio_request(config->hpd_gpio, "HDMI_HPD");
		if (ret) {
			SDE_ERROR("'%s'(%d) gpio_request failed: %d\n",
				"HDMI_HPD", config->hpd_gpio, ret);
			goto error_hpd_gpio;
		}
		gpio_direction_output(config->hpd_gpio, 1);

		if (config->mux_en_gpio != -1) {
			ret = gpio_request(config->mux_en_gpio, "HDMI_MUX_EN");
			if (ret) {
				SDE_ERROR("'%s'(%d) gpio_request failed: %d\n",
					"HDMI_MUX_EN", config->mux_en_gpio,
					ret);
				goto error_en_gpio;
			}
			gpio_set_value_cansleep(config->mux_en_gpio, 1);
		}

		if (config->mux_sel_gpio != -1) {
			ret = gpio_request(config->mux_sel_gpio,
				"HDMI_MUX_SEL");
			if (ret) {
				SDE_ERROR("'%s'(%d) gpio_request failed: %d\n",
					"HDMI_MUX_SEL", config->mux_sel_gpio,
					ret);
				goto error_sel_gpio;
			}
			gpio_set_value_cansleep(config->mux_sel_gpio, 0);
		}

		if (config->mux_lpm_gpio != -1) {
			ret = gpio_request(config->mux_lpm_gpio,
					"HDMI_MUX_LPM");
			if (ret) {
				SDE_ERROR("'%s'(%d) gpio_request failed: %d\n",
					"HDMI_MUX_LPM",
					config->mux_lpm_gpio, ret);
				goto error_lpm_gpio;
			}
			gpio_set_value_cansleep(config->mux_lpm_gpio, 1);
		}
		SDE_DEBUG("gpio on");
	} else {
		if (config->ddc_clk_gpio != -1)
			gpio_free(config->ddc_clk_gpio);

		if (config->ddc_data_gpio != -1)
			gpio_free(config->ddc_data_gpio);

		gpio_free(config->hpd_gpio);

		if (config->mux_en_gpio != -1) {
			gpio_set_value_cansleep(config->mux_en_gpio, 0);
			gpio_free(config->mux_en_gpio);
		}

		if (config->mux_sel_gpio != -1) {
			gpio_set_value_cansleep(config->mux_sel_gpio, 1);
			gpio_free(config->mux_sel_gpio);
		}

		if (config->mux_lpm_gpio != -1) {
			gpio_set_value_cansleep(config->mux_lpm_gpio, 0);
			gpio_free(config->mux_lpm_gpio);
		}
		SDE_DEBUG("gpio off");
	}

	return 0;

error_lpm_gpio:
	if (config->mux_sel_gpio != -1)
		gpio_free(config->mux_sel_gpio);
error_sel_gpio:
	if (config->mux_en_gpio != -1)
		gpio_free(config->mux_en_gpio);
error_en_gpio:
	gpio_free(config->hpd_gpio);
error_hpd_gpio:
	if (config->ddc_data_gpio != -1)
		gpio_free(config->ddc_data_gpio);
error_ddc_data_gpio:
	if (config->ddc_clk_gpio != -1)
		gpio_free(config->ddc_clk_gpio);
error_ddc_clk_gpio:
	return ret;
}

static int _sde_hdmi_hpd_enable(struct sde_hdmi *sde_hdmi)
{
	struct hdmi *hdmi = sde_hdmi->ctrl.ctrl;
	const struct hdmi_platform_config *config = hdmi->config;
	struct device *dev = &hdmi->pdev->dev;
	uint32_t hpd_ctrl;
	int i, ret;
	unsigned long flags;

	for (i = 0; i < config->hpd_reg_cnt; i++) {
		ret = regulator_enable(hdmi->hpd_regs[i]);
		if (ret) {
			SDE_ERROR("failed to enable hpd regulator: %s (%d)\n",
					config->hpd_reg_names[i], ret);
			goto fail;
		}
	}

	ret = pinctrl_pm_select_default_state(dev);
	if (ret) {
		SDE_ERROR("pinctrl state chg failed: %d\n", ret);
		goto fail;
	}

	ret = _sde_hdmi_gpio_config(hdmi, true);
	if (ret) {
		SDE_ERROR("failed to configure GPIOs: %d\n", ret);
		goto fail;
	}

	for (i = 0; i < config->hpd_clk_cnt; i++) {
		if (config->hpd_freq && config->hpd_freq[i]) {
			ret = clk_set_rate(hdmi->hpd_clks[i],
					config->hpd_freq[i]);
			if (ret)
				pr_warn("failed to set clk %s (%d)\n",
						config->hpd_clk_names[i], ret);
		}

		ret = clk_prepare_enable(hdmi->hpd_clks[i]);
		if (ret) {
			SDE_ERROR("failed to enable hpd clk: %s (%d)\n",
					config->hpd_clk_names[i], ret);
			goto fail;
		}
	}

	hdmi_set_mode(hdmi, false);
	_sde_hdmi_phy_reset(hdmi);
	hdmi_set_mode(hdmi, true);

	hdmi_write(hdmi, REG_HDMI_USEC_REFTIMER, 0x0001001b);

	/* enable HPD events: */
	hdmi_write(hdmi, REG_HDMI_HPD_INT_CTRL,
			HDMI_HPD_INT_CTRL_INT_CONNECT |
			HDMI_HPD_INT_CTRL_INT_EN);

	/* set timeout to 4.1ms (max) for hardware debounce */
	spin_lock_irqsave(&hdmi->reg_lock, flags);
	hpd_ctrl = hdmi_read(hdmi, REG_HDMI_HPD_CTRL);
	hpd_ctrl |= HDMI_HPD_CTRL_TIMEOUT(0x1fff);

	/* Toggle HPD circuit to trigger HPD sense */
	hdmi_write(hdmi, REG_HDMI_HPD_CTRL,
			~HDMI_HPD_CTRL_ENABLE & hpd_ctrl);
	hdmi_write(hdmi, REG_HDMI_HPD_CTRL,
			HDMI_HPD_CTRL_ENABLE | hpd_ctrl);
	spin_unlock_irqrestore(&hdmi->reg_lock, flags);

	return 0;

fail:
	return ret;
}

static void _sde_hdmi_hdp_disable(struct sde_hdmi *sde_hdmi)
{
	struct hdmi *hdmi = sde_hdmi->ctrl.ctrl;
	const struct hdmi_platform_config *config = hdmi->config;
	struct device *dev = &hdmi->pdev->dev;
	int i, ret = 0;

	/* Disable HPD interrupt */
	hdmi_write(hdmi, REG_HDMI_HPD_INT_CTRL, 0);

	hdmi_set_mode(hdmi, false);

	for (i = 0; i < config->hpd_clk_cnt; i++)
		clk_disable_unprepare(hdmi->hpd_clks[i]);

	ret = _sde_hdmi_gpio_config(hdmi, false);
	if (ret)
		pr_warn("failed to unconfigure GPIOs: %d\n", ret);

	ret = pinctrl_pm_select_sleep_state(dev);
	if (ret)
		pr_warn("pinctrl state chg failed: %d\n", ret);

	for (i = 0; i < config->hpd_reg_cnt; i++) {
		ret = regulator_disable(hdmi->hpd_regs[i]);
		if (ret)
			pr_warn("failed to disable hpd regulator: %s (%d)\n",
					config->hpd_reg_names[i], ret);
	}
}

static void _sde_hdmi_hotplug_work(struct work_struct *work)
{
	struct sde_hdmi *sde_hdmi =
		container_of(work, struct sde_hdmi, hpd_work);
	struct drm_connector *connector;

	if (!sde_hdmi || !sde_hdmi->ctrl.ctrl ||
		!sde_hdmi->ctrl.ctrl->connector) {
		SDE_ERROR("sde_hdmi=%p or hdmi or connector is NULL\n",
				sde_hdmi);
		return;
	}

	connector = sde_hdmi->ctrl.ctrl->connector;
	drm_helper_hpd_irq_event(connector->dev);
}

static void _sde_hdmi_connector_irq(struct sde_hdmi *sde_hdmi)
{
	struct hdmi *hdmi = sde_hdmi->ctrl.ctrl;
	uint32_t hpd_int_status, hpd_int_ctrl;

	/* Process HPD: */
	hpd_int_status = hdmi_read(hdmi, REG_HDMI_HPD_INT_STATUS);
	hpd_int_ctrl   = hdmi_read(hdmi, REG_HDMI_HPD_INT_CTRL);

	if ((hpd_int_ctrl & HDMI_HPD_INT_CTRL_INT_EN) &&
			(hpd_int_status & HDMI_HPD_INT_STATUS_INT)) {
		bool detected = !!(hpd_int_status &
					HDMI_HPD_INT_STATUS_CABLE_DETECTED);

		/* ack & disable (temporarily) HPD events: */
		hdmi_write(hdmi, REG_HDMI_HPD_INT_CTRL,
			HDMI_HPD_INT_CTRL_INT_ACK);

		DRM_DEBUG("status=%04x, ctrl=%04x", hpd_int_status,
				hpd_int_ctrl);

		/* detect disconnect if we are connected or visa versa: */
		hpd_int_ctrl = HDMI_HPD_INT_CTRL_INT_EN;
		if (!detected)
			hpd_int_ctrl |= HDMI_HPD_INT_CTRL_INT_CONNECT;
		hdmi_write(hdmi, REG_HDMI_HPD_INT_CTRL, hpd_int_ctrl);

		queue_work(hdmi->workq, &sde_hdmi->hpd_work);
	}
}

static irqreturn_t _sde_hdmi_irq(int irq, void *dev_id)
{
	struct sde_hdmi *sde_hdmi = dev_id;
	struct hdmi *hdmi;

	if (!sde_hdmi || !sde_hdmi->ctrl.ctrl) {
		SDE_ERROR("sde_hdmi=%p or hdmi is NULL\n", sde_hdmi);
		return IRQ_NONE;
	}
	hdmi = sde_hdmi->ctrl.ctrl;

	/* Process HPD: */
	_sde_hdmi_connector_irq(sde_hdmi);

	/* Process DDC: */
	hdmi_i2c_irq(hdmi->i2c);

#ifdef HDCP
	/* Process HDCP: */
	if (hdmi->hdcp_ctrl)
		hdmi_hdcp_irq(hdmi->hdcp_ctrl);
#endif

	/* TODO audio.. */

	return IRQ_HANDLED;
}

int sde_hdmi_get_info(struct msm_display_info *info,
				void *display)
{
	int rc = 0;
	struct sde_hdmi *hdmi_display = (struct sde_hdmi *)display;

	if (!display || !info) {
		SDE_ERROR("display=%p or info=%p is NULL\n", display, info);
		return -EINVAL;
	}

	DBG("");
	mutex_lock(&hdmi_display->display_lock);

	info->intf_type = DRM_MODE_CONNECTOR_HDMIA;
	info->num_of_h_tiles = 1;
	info->h_tile_instance[0] = 0;
	info->is_connected = true;
	info->capabilities = MSM_DISPLAY_CAP_HOT_PLUG | MSM_DISPLAY_CAP_EDID |
				MSM_DISPLAY_CAP_VID_MODE;
	info->max_width = 4096;
	info->max_height = 2160;
	info->compression = MSM_DISPLAY_COMPRESS_NONE;

	mutex_unlock(&hdmi_display->display_lock);
	return rc;
}

u32 sde_hdmi_get_num_of_displays(void)
{
	u32 count = 0;
	struct sde_hdmi *display;

	mutex_lock(&sde_hdmi_list_lock);

	list_for_each_entry(display, &sde_hdmi_list, list) {
		count++;
	}

	mutex_unlock(&sde_hdmi_list_lock);
	return count;
}

int sde_hdmi_get_displays(void **display_array, u32 max_display_count)
{
	struct sde_hdmi *display;
	int i = 0;

	SDE_DEBUG("\n");

	if (!display_array || !max_display_count) {
		if (!display_array)
			SDE_ERROR("invalid param\n");
		return 0;
	}

	mutex_lock(&sde_hdmi_list_lock);
	list_for_each_entry(display, &sde_hdmi_list, list) {
		if (i >= max_display_count)
			break;
		display_array[i++] = display;
	}
	mutex_unlock(&sde_hdmi_list_lock);

	return i;
}

int sde_hdmi_connector_pre_deinit(struct drm_connector *connector,
		void *display)
{
	struct sde_hdmi *sde_hdmi = (struct sde_hdmi *)display;

	if (!sde_hdmi || !sde_hdmi->ctrl.ctrl) {
		SDE_ERROR("sde_hdmi=%p or hdmi is NULL\n", sde_hdmi);
		return -EINVAL;
	}

	_sde_hdmi_hdp_disable(sde_hdmi);

	return 0;
}

int sde_hdmi_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display)
{
	int rc;
	struct sde_hdmi *sde_hdmi = (struct sde_hdmi *)display;
	struct hdmi *hdmi;

	if (!sde_hdmi) {
		SDE_ERROR("sde_hdmi is NULL\n");
		return -EINVAL;
	}

	hdmi = sde_hdmi->ctrl.ctrl;
	if (!hdmi) {
		SDE_ERROR("hdmi is NULL\n");
		return -EINVAL;
	}

	if (info)
		sde_kms_info_add_keystr(info,
				"DISPLAY_TYPE",
				sde_hdmi->display_type);

	hdmi->connector = connector;
	INIT_WORK(&sde_hdmi->hpd_work, _sde_hdmi_hotplug_work);

	/* Enable HPD detection */
	rc = _sde_hdmi_hpd_enable(sde_hdmi);
	if (rc)
		SDE_ERROR("failed to enable HPD: %d\n", rc);

	return 0;
}

enum drm_connector_status
sde_hdmi_connector_detect(struct drm_connector *connector,
		bool force,
		void *display)
{
	enum drm_connector_status status = connector_status_unknown;
	struct msm_display_info info;
	int rc;

	if (!connector || !display) {
		SDE_ERROR("connector=%p or display=%p is NULL\n",
			connector, display);
		return status;
	}

	SDE_DEBUG("\n");

	/* get display dsi_info */
	memset(&info, 0x0, sizeof(info));
	rc = sde_hdmi_get_info(&info, display);
	if (rc) {
		SDE_ERROR("failed to get display info, rc=%d\n", rc);
		return connector_status_disconnected;
	}

	if (info.capabilities & MSM_DISPLAY_CAP_HOT_PLUG)
		status = (info.is_connected ? connector_status_connected :
					      connector_status_disconnected);
	else
		status = connector_status_connected;

	connector->display_info.width_mm = info.width_mm;
	connector->display_info.height_mm = info.height_mm;

	return status;
}

int sde_hdmi_connector_get_modes(struct drm_connector *connector, void *display)
{
	struct sde_hdmi *hdmi_display = (struct sde_hdmi *)display;
	struct hdmi *hdmi;
	struct edid *edid;
	uint32_t hdmi_ctrl;
	int ret = 0;

	if (!connector || !display) {
		SDE_ERROR("connector=%p or display=%p is NULL\n",
			connector, display);
		return 0;
	}

	SDE_DEBUG("\n");

	hdmi = hdmi_display->ctrl.ctrl;
	hdmi_ctrl = hdmi_read(hdmi, REG_HDMI_CTRL);
	hdmi_write(hdmi, REG_HDMI_CTRL, hdmi_ctrl | HDMI_CTRL_ENABLE);

	edid = drm_get_edid(connector, hdmi->i2c);

	hdmi_write(hdmi, REG_HDMI_CTRL, hdmi_ctrl);

	hdmi->hdmi_mode = drm_detect_hdmi_monitor(edid);
	drm_mode_connector_update_edid_property(connector, edid);

	if (edid) {
		ret = drm_add_edid_modes(connector, edid);
		kfree(edid);
	}

	return ret;
}

enum drm_mode_status sde_hdmi_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display)
{
	struct sde_hdmi *hdmi_display = (struct sde_hdmi *)display;
	struct hdmi *hdmi;
	struct msm_drm_private *priv;
	struct msm_kms *kms;
	long actual, requested;

	if (!connector || !display || !mode) {
		SDE_ERROR("connector=%p or display=%p or mode=%p is NULL\n",
			connector, display, mode);
		return 0;
	}

	SDE_DEBUG("\n");

	hdmi = hdmi_display->ctrl.ctrl;
	priv = connector->dev->dev_private;
	kms = priv->kms;
	requested = 1000 * mode->clock;
	actual = kms->funcs->round_pixclk(kms,
			requested, hdmi->encoder);

	SDE_DEBUG("requested=%ld, actual=%ld", requested, actual);

	if (actual != requested)
		return MODE_CLOCK_RANGE;

	return MODE_OK;
}

int sde_hdmi_dev_init(struct sde_hdmi *display)
{
	int rc = 0;

	if (!display) {
		SDE_ERROR("Invalid params\n");
		return -EINVAL;
	}

	return rc;
}

int sde_hdmi_dev_deinit(struct sde_hdmi *display)
{
	int rc = 0;

	if (!display) {
		SDE_ERROR("Invalid params\n");
		return -EINVAL;
	}

	return rc;
}

int sde_hdmi_bind(struct sde_hdmi *display, struct drm_device *dev)
{
	int rc = 0;
	struct sde_hdmi_ctrl *display_ctrl;
	struct msm_drm_private *priv = dev->dev_private;

	if (!display || !priv) {
		SDE_ERROR("Invalid params, display=%p, priv=%p", display, priv);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	rc = _sde_hdmi_debugfs_init(display);
	if (rc) {
		SDE_ERROR("[%s]Debugfs init failed, rc=%d\n",
				display->name, rc);
		goto error;
	}

	display_ctrl = &display->ctrl;
	display_ctrl->ctrl = priv->hdmi;
	display->drm_dev = dev;

error:
	mutex_unlock(&display->display_lock);
	return rc;
}


int sde_hdmi_unbind(struct sde_hdmi *display)
{
	if (!display) {
		SDE_ERROR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	(void)_sde_hdmi_debugfs_deinit(display);
	mutex_unlock(&display->display_lock);
	return 0;
}

static int _sde_hdmi_dev_probe(struct platform_device *pdev)
{
	struct sde_hdmi *display;

	DBG("");

	if (!pdev || !pdev->dev.of_node) {
		SDE_ERROR("pdev not found\n");
		return -ENODEV;
	}

	display = devm_kzalloc(&pdev->dev, sizeof(*display), GFP_KERNEL);
	if (!display)
		return -ENOMEM;

	DBG("");
	display->name = of_get_property(pdev->dev.of_node, "label", NULL);

	display->display_type = of_get_property(pdev->dev.of_node,
						"qcom,display-type", NULL);
	if (!display->display_type)
		display->display_type = "unknown";

	mutex_init(&display->display_lock);

	display->pdev = pdev;
	platform_set_drvdata(pdev, display);
	mutex_lock(&sde_hdmi_list_lock);
	list_add(&display->list, &sde_hdmi_list);
	mutex_unlock(&sde_hdmi_list_lock);
	return 0;
}

static int _sde_hdmi_dev_remove(struct platform_device *pdev)
{
	struct sde_hdmi *display;
	struct sde_hdmi *pos, *tmp;

	if (!pdev) {
		SDE_ERROR("Invalid device\n");
		return -EINVAL;
	}

	display = platform_get_drvdata(pdev);

	mutex_lock(&sde_hdmi_list_lock);
	list_for_each_entry_safe(pos, tmp, &sde_hdmi_list, list) {
		if (pos == display) {
			list_del(&display->list);
			break;
		}
	}
	mutex_unlock(&sde_hdmi_list_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, display);
	return 0;
}

static struct platform_driver sde_hdmi_driver = {
	.probe = _sde_hdmi_dev_probe,
	.remove = _sde_hdmi_dev_remove,
	.driver = {
		.name = "sde-hdmi",
		.of_match_table = sde_hdmi_dt_match,
	},
};

int sde_hdmi_drm_init(struct sde_hdmi *display, struct drm_encoder *enc)
{
	int rc = 0;
	struct msm_drm_private *priv = NULL;
	struct hdmi *hdmi;
	struct platform_device *pdev;

	DBG("");
	if (!display || !display->drm_dev || !enc) {
		SDE_ERROR("display=%p or enc=%p or drm_dev is NULL\n",
			display, enc);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	priv = display->drm_dev->dev_private;
	hdmi = display->ctrl.ctrl;

	if (!priv || !hdmi) {
		SDE_ERROR("priv=%p or hdmi=%p is NULL\n",
			priv, hdmi);
		mutex_unlock(&display->display_lock);
		return -EINVAL;
	}

	pdev = hdmi->pdev;
	hdmi->dev = display->drm_dev;
	hdmi->encoder = enc;

	hdmi_audio_infoframe_init(&hdmi->audio.infoframe);

	hdmi->bridge = hdmi_bridge_init(hdmi);
	if (IS_ERR(hdmi->bridge)) {
		rc = PTR_ERR(hdmi->bridge);
		SDE_ERROR("failed to create HDMI bridge: %d\n", rc);
		hdmi->bridge = NULL;
		goto error;
	}

	hdmi->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (hdmi->irq < 0) {
		rc = hdmi->irq;
		SDE_ERROR("failed to get irq: %d\n", rc);
		goto error;
	}

	rc = devm_request_irq(&pdev->dev, hdmi->irq,
			_sde_hdmi_irq, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
			"hdmi_isr", display);
	if (rc < 0) {
		SDE_ERROR("failed to request IRQ%u: %d\n",
				hdmi->irq, rc);
		goto error;
	}

	enc->bridge = hdmi->bridge;
	priv->bridges[priv->num_bridges++] = hdmi->bridge;

	mutex_unlock(&display->display_lock);
	return 0;

error:
	/* bridge is normally destroyed by drm: */
	if (hdmi->bridge) {
		hdmi_bridge_destroy(hdmi->bridge);
		hdmi->bridge = NULL;
	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int sde_hdmi_drm_deinit(struct sde_hdmi *display)
{
	int rc = 0;

	if (!display) {
		SDE_ERROR("Invalid params\n");
		return -EINVAL;
	}

	return rc;
}

void sde_hdmi_register(void)
{
	DBG("");
	platform_driver_register(&sde_hdmi_driver);
}

void sde_hdmi_unregister(void)
{
	platform_driver_unregister(&sde_hdmi_driver);
}

