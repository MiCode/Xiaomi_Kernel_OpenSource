/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/types.h>

/* #define DEBUG */

#include "mdss_fb.h"
#include "mdss_hdmi_tx.h"
#include "mdss.h"
#include "mdss_panel.h"

#define DRV_NAME "hdmi-tx"
#define COMPATIBLE_NAME "qcom,hdmi-tx"

/* HDMI PHY/PLL bit field macros */
#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)

static int hdmi_tx_sysfs_enable_hpd(struct hdmi_tx_ctrl *hdmi_ctrl, int on);
static irqreturn_t hdmi_tx_isr(int irq, void *data);

struct mdss_hw hdmi_tx_hw = {
	.hw_ndx = MDSS_HW_HDMI,
	.ptr = NULL,
	.irq_handler = hdmi_tx_isr,
};

const char *hdmi_pm_name(enum hdmi_tx_power_module_type module)
{
	switch (module) {
	case HDMI_TX_HPD_PM:	return "HDMI_TX_HPD_PM";
	case HDMI_TX_CORE_PM:	return "HDMI_TX_CORE_PM";
	case HDMI_TX_CEC_PM:	return "HDMI_TX_CEC_PM";
	default: return "???";
	}
} /* hdmi_pm_name */

static const char *hdmi_tx_clk_name(u32 clk)
{
	switch (clk) {
	case HDMI_TX_AHB_CLK:	return "hdmi_ahb_clk";
	case HDMI_TX_APP_CLK:	return "hdmi_app_clk";
	case HDMI_TX_EXTP_CLK:	return "hdmi_extp_clk";
	default:		return "???";
	}
} /* hdmi_tx_clk_name */

static const char *hdmi_tx_io_name(u32 io)
{
	switch (io) {
	case HDMI_TX_CORE_IO:	return "core_physical";
	case HDMI_TX_PHY_IO:	return "phy_physical";
	case HDMI_TX_QFPROM_IO:	return "qfprom_physical";
	default:		return NULL;
	}
} /* hdmi_tx_io_name */

static struct hdmi_tx_ctrl *hdmi_tx_get_drvdata_from_panel_data(
	struct mdss_panel_data *mpd)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (mpd) {
		hdmi_ctrl = container_of(mpd, struct hdmi_tx_ctrl, panel_data);
		if (hdmi_ctrl) {
			hdmi_ctrl->pixel_clk =
				mpd->panel_info.fbi->var.pixclock;
			hdmi_ctrl->xres = mpd->panel_info.fbi->var.xres;
			hdmi_ctrl->yres = mpd->panel_info.fbi->var.yres;
		} else {
			DEV_ERR("%s: hdmi_ctrl = NULL\n", __func__);
		}
	} else {
		DEV_ERR("%s: mdss_panel_data = NULL\n", __func__);
	}
	return hdmi_ctrl;
} /* hdmi_tx_get_drvdata_from_panel_data */

static struct hdmi_tx_ctrl *hdmi_tx_get_drvdata_from_sysfs_dev(
	struct device *device)
{
	struct msm_fb_data_type *mfd = NULL;
	struct mdss_panel_data *panel_data = NULL;
	struct fb_info *fbi = dev_get_drvdata(device);

	if (fbi) {
		mfd = (struct msm_fb_data_type *)fbi->par;
		panel_data = dev_get_platdata(&mfd->pdev->dev);

		return hdmi_tx_get_drvdata_from_panel_data(panel_data);
	} else {
		DEV_ERR("%s: fbi = NULL\n", __func__);
		return NULL;
	}
} /* hdmi_tx_get_drvdata_from_sysfs_dev */

static ssize_t hdmi_tx_sysfs_rda_connected(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->mutex);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->hpd_state);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_state);
	mutex_unlock(&hdmi_ctrl->mutex);

	return ret;
} /* hdmi_tx_sysfs_rda_connected */

static ssize_t hdmi_tx_sysfs_rda_fake_hpd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->mutex);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->hpd_state);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_state);
	mutex_unlock(&hdmi_ctrl->mutex);

	return ret;
} /* hdmi_tx_sysfs_rda_fake_hpd */

static ssize_t hdmi_tx_sysfs_wta_fake_hpd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int fake_hpd, rc = 0;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	DEV_DBG("%s:\n", __func__);
	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &fake_hpd);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	mutex_lock(&hdmi_ctrl->mutex);
	DEV_INFO("%s: fake_hpd=%d\n", __func__, fake_hpd);
	if (fake_hpd) {
		hdmi_ctrl->hpd_state = true;

		/* todo: Remove this once HPD line is available in HW */
		DEV_INFO("HDMI HPD: sense CONNECTED: send ONLINE\n");
		if (kobject_uevent(hdmi_ctrl->kobj, KOBJ_ONLINE))
			DEV_ERR("%s: failed sending online event\n", __func__);
		switch_set_state(&hdmi_ctrl->sdev, 1);
		DEV_INFO("%s: Hdmi state switch to %d\n", __func__,
			hdmi_ctrl->sdev.state);
	} else {
		hdmi_ctrl->hpd_state = false;

		/* todo: Remove this once HPD line is available in HW */
		DEV_INFO("HDMI HPD: sense CONNECTED: send ONLINE\n");
		if (kobject_uevent(hdmi_ctrl->kobj, KOBJ_OFFLINE))
			DEV_ERR("%s: failed sending online event\n", __func__);
		switch_set_state(&hdmi_ctrl->sdev, 0);
		DEV_INFO("%s: Hdmi state switch to %d\n", __func__,
			hdmi_ctrl->sdev.state);
	}
	mutex_unlock(&hdmi_ctrl->mutex);

	return ret;
} /* hdmi_tx_sysfs_wta_fake_hpd */

static ssize_t hdmi_tx_sysfs_rda_hpd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->hpd_feature_on);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_feature_on);

	return ret;
} /* hdmi_tx_sysfs_rda_hpd */

static ssize_t hdmi_tx_sysfs_wta_hpd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int hpd, rc = 0;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	DEV_DBG("%s:\n", __func__);
	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &hpd);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	/* todo: Remove this once HPD line is available in HW */
	if (0) {
		if (0 == hpd && hdmi_ctrl->hpd_feature_on) {
			rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, false);
		} else if (1 == hpd && !hdmi_ctrl->hpd_feature_on) {
			rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		} else {
			rc = -EPERM;
			ret = rc;
		}
	}

	if (!rc) {
		hdmi_ctrl->hpd_feature_on =
			(~hdmi_ctrl->hpd_feature_on) & BIT(0);
		DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_feature_on);
	} else {
		DEV_DBG("%s: '%d' (unchanged)\n", __func__,
			hdmi_ctrl->hpd_feature_on);
	}

	return ret;
} /* hdmi_tx_sysfs_wta_hpd */

static DEVICE_ATTR(connected, S_IRUGO, hdmi_tx_sysfs_rda_connected, NULL);
static DEVICE_ATTR(hpd, S_IRUGO | S_IWUSR, hdmi_tx_sysfs_rda_hpd,
	hdmi_tx_sysfs_wta_hpd);
static DEVICE_ATTR(fake_hpd, S_IRUGO | S_IWUSR, hdmi_tx_sysfs_rda_fake_hpd,
	hdmi_tx_sysfs_wta_fake_hpd);

static struct attribute *hdmi_tx_fs_attrs[] = {
	&dev_attr_connected.attr,
	&dev_attr_hpd.attr,
	&dev_attr_fake_hpd.attr,
	NULL,
};
static struct attribute_group hdmi_tx_fs_attrs_group = {
	.attrs = hdmi_tx_fs_attrs,
};

static int hdmi_tx_sysfs_create(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc;
	struct mdss_panel_info *pinfo = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}
	pinfo = &hdmi_ctrl->panel_data.panel_info;

	rc = sysfs_create_group(&pinfo->fbi->dev->kobj,
		&hdmi_tx_fs_attrs_group);
	if (rc) {
		DEV_ERR("%s: failed, rc=%d\n", __func__, rc);
		return rc;
	}
	hdmi_ctrl->kobj = &pinfo->fbi->dev->kobj;
	DEV_DBG("%s: sysfs group %p\n", __func__, hdmi_ctrl->kobj);

	kobject_uevent(hdmi_ctrl->kobj, KOBJ_ADD);
	DEV_DBG("%s: kobject_uevent(KOBJ_ADD)\n", __func__);

	return 0;
} /* hdmi_tx_sysfs_create */

static void hdmi_tx_sysfs_remove(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	if (hdmi_ctrl->kobj)
		sysfs_remove_group(hdmi_ctrl->kobj, &hdmi_tx_fs_attrs_group);
	hdmi_ctrl->kobj = NULL;
} /* hdmi_tx_sysfs_remove */

static inline u32 hdmi_tx_is_controller_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return HDMI_REG_R_ND(hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO].base,
		HDMI_CTRL) & BIT(0);
} /* hdmi_tx_is_controller_on */

static inline u32 hdmi_tx_is_dvi_mode(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return !(HDMI_REG_R_ND(hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO].base,
		HDMI_CTRL) & BIT(1));
} /* hdmi_tx_is_dvi_mode */

static int hdmi_tx_init_panel_info(uint32_t resolution,
	struct mdss_panel_info *pinfo)
{
	const struct hdmi_disp_mode_timing_type *timing =
		hdmi_get_supported_mode(resolution);

	if (!timing || !pinfo) {
		DEV_ERR("%s: invalid input.\n", __func__);
		return -EINVAL;
	}

	pinfo->xres = timing->active_h;
	pinfo->yres = timing->active_v;
	pinfo->clk_rate = timing->pixel_freq*1000;

	pinfo->lcdc.h_back_porch = timing->back_porch_h;
	pinfo->lcdc.h_front_porch = timing->front_porch_h;
	pinfo->lcdc.h_pulse_width = timing->pulse_width_h;
	pinfo->lcdc.v_back_porch = timing->back_porch_v;
	pinfo->lcdc.v_front_porch = timing->front_porch_v;
	pinfo->lcdc.v_pulse_width = timing->pulse_width_v;

	pinfo->type = DTV_PANEL;
	pinfo->pdest = DISPLAY_2;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 1;

	pinfo->lcdc.border_clr = 0; /* blk */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;

	return 0;
} /* hdmi_tx_init_panel_info */

/* Table indicating the video format supported by the HDMI TX Core */
/* Valid Pixel-Clock rates: 25.2MHz, 27MHz, 27.03MHz, 74.25MHz, 148.5MHz */
static void hdmi_tx_setup_video_mode_lut(void)
{
	hdmi_set_supported_mode(HDMI_VFRMT_640x480p60_4_3);
	hdmi_set_supported_mode(HDMI_VFRMT_720x480p60_4_3);
	hdmi_set_supported_mode(HDMI_VFRMT_720x480p60_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_720x576p50_4_3);
	hdmi_set_supported_mode(HDMI_VFRMT_720x576p50_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1440x480i60_4_3);
	hdmi_set_supported_mode(HDMI_VFRMT_1440x480i60_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1440x576i50_4_3);
	hdmi_set_supported_mode(HDMI_VFRMT_1440x576i50_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1280x720p50_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1280x720p60_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1920x1080p24_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1920x1080p25_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1920x1080p30_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1920x1080p50_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1920x1080i60_16_9);
	hdmi_set_supported_mode(HDMI_VFRMT_1920x1080p60_16_9);
} /* hdmi_tx_setup_video_mode_lut */

static void hdmi_tx_hpd_state_work(struct work_struct *work)
{
	u32 hpd_state = false;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct hdmi_tx_io_data *io = NULL;

	hdmi_ctrl = container_of(work, struct hdmi_tx_ctrl, hpd_state_work);
	if (!hdmi_ctrl || !hdmi_ctrl->hpd_initialized) {
		DEV_DBG("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	DEV_DBG("%s: Got HPD interrupt\n", __func__);

	hpd_state = (HDMI_REG_R(io->base, HDMI_HPD_INT_STATUS) & BIT(1)) >> 1;
	mutex_lock(&hdmi_ctrl->mutex);
	if ((hdmi_ctrl->hpd_prev_state != hdmi_ctrl->hpd_state) ||
		(hdmi_ctrl->hpd_state != hpd_state)) {

		hdmi_ctrl->hpd_state = hpd_state;
		hdmi_ctrl->hpd_prev_state = hdmi_ctrl->hpd_state;
		hdmi_ctrl->hpd_stable = 0;

		DEV_DBG("%s: state not stable yet, wait again (%d|%d|%d)\n",
			__func__, hdmi_ctrl->hpd_prev_state,
			hdmi_ctrl->hpd_state, hpd_state);

		mutex_unlock(&hdmi_ctrl->mutex);

		mod_timer(&hdmi_ctrl->hpd_state_timer, jiffies + HZ/2);

		return;
	}

	if (hdmi_ctrl->hpd_stable) {
		mutex_unlock(&hdmi_ctrl->mutex);
		DEV_DBG("%s: no more timer, depending on IRQ now\n",
			__func__);
		return;
	}

	hdmi_ctrl->hpd_stable = 1;
	DEV_INFO("HDMI HPD: event detected\n");

	/*
	 *todo: Revisit cable chg detected condition when HPD support is ready
	 */
	hdmi_ctrl->hpd_cable_chg_detected = false;
	mutex_unlock(&hdmi_ctrl->mutex);

	if (hpd_state) {
		DEV_INFO("HDMI HPD: sense CONNECTED: send ONLINE\n");
		kobject_uevent(hdmi_ctrl->kobj, KOBJ_ONLINE);
		switch_set_state(&hdmi_ctrl->sdev, 1);
		DEV_INFO("%s: Hdmi state switch to %d\n", __func__,
			hdmi_ctrl->sdev.state);
	} else {
		DEV_INFO("HDMI HPD: sense DISCONNECTED: send OFFLINE\n");
		kobject_uevent(hdmi_ctrl->kobj, KOBJ_OFFLINE);
		switch_set_state(&hdmi_ctrl->sdev, 0);
		DEV_INFO("%s: Hdmi state switch to %d\n", __func__,
			hdmi_ctrl->sdev.state);
	}

	/* Set IRQ for HPD */
	HDMI_REG_W(io->base, HDMI_HPD_INT_CTRL, 4 | (hpd_state ? 0 : 2));
} /* hdmi_tx_hpd_state_work */

static int hdmi_tx_check_capability(void __iomem *base)
{
	u32 hdmi_disabled, hdcp_disabled;

	if (!base) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* QFPROM_RAW_FEAT_CONFIG_ROW0_LSB */
	hdcp_disabled = HDMI_REG_R_ND(base, 0x000000F8) & BIT(31);
	/* QFPROM_RAW_FEAT_CONFIG_ROW0_MSB */
	hdmi_disabled = HDMI_REG_R_ND(base, 0x000000FC) & BIT(0);

	DEV_DBG("%s: Features <HDMI:%s, HDCP:%s>\n", __func__,
		hdmi_disabled ? "OFF" : "ON", hdcp_disabled ? "OFF" : "ON");

	if (hdmi_disabled) {
		DEV_ERR("%s: HDMI disabled\n", __func__);
		return -ENODEV;
	}

	if (hdcp_disabled)
		DEV_WARN("%s: HDCP disabled\n", __func__);

	return 0;
} /* hdmi_tx_check_capability */

/* todo: revisit when new HPD debouncing logic is available */
static void hdmi_tx_hpd_state_timer(unsigned long data)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = (struct hdmi_tx_ctrl *)data;

	if (hdmi_ctrl)
		queue_work(hdmi_ctrl->workq, &hdmi_ctrl->hpd_state_work);
	else
		DEV_ERR("%s: invalid input\n", __func__);
} /* hdmi_tx_hpd_state_timer */

static void hdmi_tx_set_mode(struct hdmi_tx_ctrl *hdmi_ctrl, u32 power_on)
{
	u32 reg_val = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (power_on) {
		/* ENABLE */
		reg_val |= BIT(0); /* Enable the block */
		if (hdmi_ctrl->present_hdcp)
			/* HDMI_Encryption_ON */
			reg_val |= BIT(1) | BIT(2);
		else
			reg_val |= BIT(1);
	} else {
		reg_val = BIT(1);
	}

	HDMI_REG_W(hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO].base, HDMI_CTRL,
		reg_val);

	DEV_DBG("HDMI Core: %s, HDMI_CTRL=0x%08x\n",
		power_on ? "Enable" : "Disable", reg_val);
} /* hdmi_tx_set_mode */

static inline struct clk *hdmi_tx_get_clk(struct hdmi_tx_platform_data *pdata,
	u32 clk_idx)
{
	if (!pdata || clk_idx > HDMI_TX_MAX_CLK) {
		DEV_ERR("%s: invalid input\n", __func__);
		return NULL;
	}

	return pdata->clk[clk_idx];
} /* hdmi_tx_get_clk */

static int hdmi_tx_clk_set_rate(struct hdmi_tx_platform_data *pdata,
	u32 clk_idx, unsigned long clk_rate)
{
	int rc = 0;
	struct clk *clk = NULL;

	if (!pdata) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	clk = hdmi_tx_get_clk(pdata, clk_idx);
	if (clk) {
		rc = clk_set_rate(clk, clk_rate);
		if (IS_ERR_VALUE(rc))
			DEV_ERR("%s: failed rc=%d\n", __func__, rc);
		else
			DEV_DBG("%s: clk=%d rate=%lu\n", __func__,
				clk_idx, clk_rate);
	} else {
		DEV_ERR("%s: FAILED: invalid clk_idx=%d\n", __func__, clk_idx);
		rc = -EINVAL;
	}

	return rc;
} /* hdmi_tx_clk_set_rate */

static int hdmi_tx_clk_update(struct hdmi_tx_platform_data *pdata, u32 clk_idx,
	u32 enable)
{
	int rc = 0;
	struct clk *clk = hdmi_tx_get_clk(pdata, clk_idx);

	if (clk) {
		DEV_DBG("%s: clk=%d en=%d\n", __func__, clk_idx, enable);
		if (enable) {
			rc = clk_prepare_enable(clk);
			if (rc)
				DEV_ERR("%s: clk=%d enable failed\n",
				__func__, clk_idx);
		} else {
			clk_disable_unprepare(clk);
		}
	} else {
		DEV_ERR("%s: FAILED: invalid input for clk='%s'\n", __func__,
			hdmi_tx_clk_name(clk_idx));
		rc = -EINVAL;
	}

	return rc;
} /* hdmi_tx_clk_update */

/* Note: Before accessing extpclk, always make sure that hdmi_ahb_clk is on */
static int hdmi_tx_clk_ctrl_update(struct hdmi_tx_platform_data *pdata, int on)
{
	int  rc = 0;
	DEV_DBG("%s: HDMI Clk: %s\n", __func__, on ? "Enable" : "Disable");

	rc = hdmi_tx_clk_update(pdata, HDMI_TX_APP_CLK, on);
	if (on && rc) {
		DEV_ERR("%s: '%s' on failed\n", __func__,
			hdmi_tx_clk_name(HDMI_TX_APP_CLK));
		goto fail_hdmi_app_clk;
	}
	if (on) {
		rc = hdmi_tx_clk_update(pdata, HDMI_TX_AHB_CLK, on);
		if (rc) {
			DEV_ERR("%s: '%s' on failed\n", __func__,
				hdmi_tx_clk_name(HDMI_TX_AHB_CLK));
			goto fail_hdmi_ahb_clk;
		}
		rc = hdmi_tx_clk_update(pdata, HDMI_TX_EXTP_CLK, on);
		if (rc) {
			DEV_ERR("%s: '%s' on failed\n", __func__,
				hdmi_tx_clk_name(HDMI_TX_EXTP_CLK));
			goto fail_hdmi_extp_clk;
		}
	} else {
		hdmi_tx_clk_update(pdata, HDMI_TX_EXTP_CLK, on);
		hdmi_tx_clk_update(pdata, HDMI_TX_AHB_CLK, on);
	}
	return rc;

fail_hdmi_extp_clk:
	hdmi_tx_clk_update(pdata, HDMI_TX_AHB_CLK, 0);
fail_hdmi_ahb_clk:
	hdmi_tx_clk_update(pdata, HDMI_TX_APP_CLK, 0);
fail_hdmi_app_clk:
	return rc;
} /* hdmi_tx_clk_ctrl_update */

static int hdmi_tx_enable_power(struct hdmi_tx_ctrl *hdmi_ctrl,
	enum hdmi_tx_power_module_type module, int enable)
{
	int rc = 0;
	struct dss_module_power *power_data = NULL;

	if (!hdmi_ctrl || module >= HDMI_TX_MAX_PM) {
		DEV_ERR("%s: Error: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	power_data = &hdmi_ctrl->pdata.power_data[module];
	if (!power_data) {
		DEV_ERR("%s: Error: invalid power data\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	if (enable) {
		rc = msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 1);
		if (rc) {
			DEV_ERR("%s: Failed to config %s vreg. Error=%d\n",
				__func__, hdmi_pm_name(module), rc);
			return rc;
		}

		rc = msm_dss_enable_vreg(power_data->vreg_config,
			power_data->num_vreg, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s vreg. Error=%d\n",
				__func__, hdmi_pm_name(module), rc);
			goto deconfig_vreg;
		}

		rc = msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, enable);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s gpio. Error=%d\n",
				__func__, hdmi_pm_name(module), rc);
			goto disable_vreg;
		}
	} else {
		msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, 0);
	}

	return rc;

disable_vreg:
	msm_dss_enable_vreg(power_data->vreg_config, power_data->num_vreg, 0);
deconfig_vreg:
	msm_dss_config_vreg(&hdmi_ctrl->pdev->dev, power_data->vreg_config,
		power_data->num_vreg, 0);
error:
	return rc;
} /* hdmi_tx_enable_power */

static void hdmi_tx_phy_reset(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	unsigned int phy_reset_polarity = 0x0;
	unsigned int pll_reset_polarity = 0x0;
	unsigned int val;
	struct hdmi_tx_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: io not inititalized\n", __func__);
		return;
	}

	val = HDMI_REG_R_ND(io->base, HDMI_PHY_CTRL);

	phy_reset_polarity = val >> 3 & 0x1;
	pll_reset_polarity = val >> 1 & 0x1;

	if (phy_reset_polarity == 0)
		HDMI_REG_W_ND(io->base, HDMI_PHY_CTRL, val | SW_RESET);
	else
		HDMI_REG_W_ND(io->base, HDMI_PHY_CTRL, val & (~SW_RESET));

	if (pll_reset_polarity == 0)
		HDMI_REG_W_ND(io->base, HDMI_PHY_CTRL, val | SW_RESET_PLL);
	else
		HDMI_REG_W_ND(io->base,
			HDMI_PHY_CTRL, val & (~SW_RESET_PLL));

	if (phy_reset_polarity == 0)
		HDMI_REG_W_ND(io->base, HDMI_PHY_CTRL, val & (~SW_RESET));
	else
		HDMI_REG_W_ND(io->base, HDMI_PHY_CTRL, val | SW_RESET);

	if (pll_reset_polarity == 0)
		HDMI_REG_W_ND(io->base,
			HDMI_PHY_CTRL, val & (~SW_RESET_PLL));
	else
		HDMI_REG_W_ND(io->base, HDMI_PHY_CTRL, val | SW_RESET_PLL);
} /* hdmi_tx_phy_reset */

static int hdmi_tx_power_on(struct mdss_panel_data *panel_data)
{
	return 0;
} /* hdmi_tx_power_on */

static int hdmi_tx_power_off(struct mdss_panel_data *panel_data)
{
	return 0;
} /* hdmi_tx_power_off */

static void hdmi_tx_hpd_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (!hdmi_ctrl->hpd_initialized) {
		DEV_DBG("%s: HPD is already OFF, returning\n", __func__);
		return;
	}

	DEV_DBG("%s: (timer, 5V, IRQ off)\n", __func__);
	del_timer_sync(&hdmi_ctrl->hpd_state_timer);
	mdss_disable_irq(&hdmi_tx_hw);

	hdmi_tx_set_mode(hdmi_ctrl, false);
	rc = hdmi_tx_clk_ctrl_update(&hdmi_ctrl->pdata, 0);
	if (rc)
		DEV_INFO("%s: Failed to disable clock. Error=%d\n",
			__func__, rc);
	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, 0);
	if (rc)
		DEV_INFO("%s: Failed to disable hpd power. Error=%d\n",
			__func__, rc);

	hdmi_ctrl->hpd_initialized = false;
} /* hdmi_tx_hpd_off */

static int hdmi_tx_hpd_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 reg_val;
	int rc = 0;
	struct hdmi_tx_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: io not inititalized\n", __func__);
		return -EINVAL;
	}

	if (hdmi_ctrl->hpd_initialized) {
		DEV_DBG("%s: HPD is already ON\n", __func__);
	} else {
		rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, true);
		if (rc) {
			DEV_ERR("%s: Failed to enable hpd power. rc=%d\n",
				__func__, rc);
			return rc;
		}

		rc = hdmi_tx_clk_ctrl_update(&hdmi_ctrl->pdata, true);
		if (rc) {
			DEV_ERR("%s: Failed to enable clocks. rc=%d\n",
				__func__, rc);
			goto disable_hpd_power;
		}

		hdmi_reg_dump(io->base, io->len, "HDMI-INIT: ");

		hdmi_tx_set_mode(hdmi_ctrl, false);
		hdmi_tx_phy_reset(hdmi_ctrl);
		hdmi_tx_set_mode(hdmi_ctrl, true);

		HDMI_REG_W(io->base, HDMI_USEC_REFTIMER, 0x0001001B);

		/* set timeout to 4.1ms (max) for hardware debounce */
		reg_val = HDMI_REG_R(io->base, HDMI_HPD_CTRL) | 0x1FFF;

		/* Toggle HPD circuit to trigger HPD sense */
		HDMI_REG_W(io->base, HDMI_HPD_CTRL,
			~(1 << 28) & reg_val);
		HDMI_REG_W(io->base, HDMI_HPD_CTRL, (1 << 28) | reg_val);

		hdmi_ctrl->hpd_initialized = true;

		/* Check HPD State */
		mdss_enable_irq(&hdmi_tx_hw);
	}

	/* Set HPD state machine: ensure at least 2 readouts */
	mutex_lock(&hdmi_ctrl->mutex);
	hdmi_ctrl->hpd_stable = 0;
	hdmi_ctrl->hpd_prev_state = true;
	hdmi_ctrl->hpd_state = false;
	hdmi_ctrl->hpd_cable_chg_detected = true;
	mutex_unlock(&hdmi_ctrl->mutex);
	mod_timer(&hdmi_ctrl->hpd_state_timer, jiffies + HZ/2);

	return 0;

disable_hpd_power:
	hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, false);

	return rc;
} /* hdmi_tx_hpd_on */

static int hdmi_tx_sysfs_enable_hpd(struct hdmi_tx_ctrl *hdmi_ctrl, int on)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	DEV_INFO("%s: %d\n", __func__, on);
	if (on) {
		rc = hdmi_tx_hpd_on(hdmi_ctrl);
	} else {
		hdmi_tx_hpd_off(hdmi_ctrl);
		/* Set HDMI switch node to 0 on HPD feature disable */
		switch_set_state(&hdmi_ctrl->sdev, 0);
		DEV_INFO("%s: Hdmi state switch to %d\n", __func__,
			hdmi_ctrl->sdev.state);
	}

	return rc;
} /* hdmi_tx_sysfs_enable_hpd */

static irqreturn_t hdmi_tx_isr(int irq, void *data)
{
	u32 hpd_int_status;
	u32 hpd_int_ctrl;
	struct hdmi_tx_io_data *io = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl = (struct hdmi_tx_ctrl *)data;

	if (!hdmi_ctrl || !hdmi_ctrl->hpd_initialized) {
		DEV_WARN("%s: invalid input data, ISR ignored\n", __func__);
		return IRQ_HANDLED;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_WARN("%s: io not initialized, ISR ignored\n", __func__);
		return IRQ_HANDLED;
	}

	/* Process HPD Interrupt */
	hpd_int_status = HDMI_REG_R(io->base, HDMI_HPD_INT_STATUS);
	hpd_int_ctrl = HDMI_REG_R(io->base, HDMI_HPD_INT_CTRL);
	if ((hpd_int_ctrl & BIT(2)) && (hpd_int_status & BIT(0))) {
		u32 cable_detected = hpd_int_status & BIT(1);

		/*
		 * Clear all interrupts, timer will turn IRQ back on
		 * Leaving the bit[2] on, else core goes off
		 * on getting HPD during power off.
		 */
		HDMI_REG_W(io->base, HDMI_HPD_INT_CTRL, BIT(2) | BIT(0));

		DEV_DBG("%s: HPD IRQ, Ctrl=%04x, State=%04x\n", __func__,
			hpd_int_ctrl, hpd_int_status);

		mutex_lock(&hdmi_ctrl->mutex);
		hdmi_ctrl->hpd_cable_chg_detected = true;
		hdmi_ctrl->hpd_prev_state = cable_detected ? 0 : 1;
		hdmi_ctrl->hpd_stable = 0;
		mutex_unlock(&hdmi_ctrl->mutex);

		mod_timer(&hdmi_ctrl->hpd_state_timer, jiffies + HZ/2);

		return IRQ_HANDLED;
	}

	DEV_DBG("%s: HPD<Ctrl=%04x, State=%04x>\n", __func__, hpd_int_ctrl,
		hpd_int_status);

	return IRQ_HANDLED;
} /* hdmi_tx_isr */

static void hdmi_tx_clk_deinit(struct hdmi_tx_platform_data *pdata)
{
	int i;
	if (!pdata) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	for (i = HDMI_TX_MAX_CLK - 1; i >= 0; i--) {
		if (pdata->clk[i])
			clk_put(pdata->clk[i]);
		pdata->clk[i] = NULL;
	}
} /* hdmi_tx_clk_deinit */

static int hdmi_tx_clk_init(struct platform_device *pdev,
	struct hdmi_tx_platform_data *pdata)
{
	int rc = 0;
	struct device *dev = NULL;
	struct clk *clk = NULL;

	if (!pdev || !pdata) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	dev = &pdev->dev;

	clk = clk_get(dev, "iface_clk");
	rc = IS_ERR(clk);
	if (rc) {
		DEV_ERR("%s: ERROR: '%s' clk not found\n", __func__,
			hdmi_tx_clk_name(HDMI_TX_AHB_CLK));
		goto error;
	}
	pdata->clk[HDMI_TX_AHB_CLK] = clk;

	clk = clk_get(dev, "core_clk");
	rc = IS_ERR(clk);
	if (rc) {
		DEV_ERR("%s: ERROR: '%s' clk not found\n", __func__,
			hdmi_tx_clk_name(HDMI_TX_APP_CLK));
		goto error;
	}
	pdata->clk[HDMI_TX_APP_CLK] = clk;

	clk = clk_get(dev, "extp_clk");
	rc = IS_ERR(clk);
	if (rc) {
		DEV_ERR("%s: ERROR: '%s' clk not found\n", __func__,
			hdmi_tx_clk_name(HDMI_TX_EXTP_CLK));
		goto error;
	}
	pdata->clk[HDMI_TX_EXTP_CLK] = clk;

	/*
	 * extpclk src is hdmi phy pll. This phy pll programming requires
	 * hdmi_ahb_clk. So enable it and then disable.
	 */
	rc = clk_prepare_enable(pdata->clk[HDMI_TX_AHB_CLK]);
	if (rc) {
		DEV_ERR("%s: failed to enable '%s' clk\n", __func__,
			hdmi_tx_clk_name(HDMI_TX_AHB_CLK));
		goto error;
	}
	rc = hdmi_tx_clk_set_rate(pdata, HDMI_TX_EXTP_CLK,
		HDMI_TX_EXTP_CLK_DEFAULT);
	if (rc) {
		DEV_ERR("%s: FAILED: '%s' clk set rate\n", __func__,
			hdmi_tx_clk_name(HDMI_TX_EXTP_CLK));
		clk_disable_unprepare(pdata->clk[HDMI_TX_AHB_CLK]);
		goto error;
	}
	clk_disable_unprepare(pdata->clk[HDMI_TX_AHB_CLK]);

	return rc;

error:
	hdmi_tx_clk_deinit(pdata);
	return rc;
} /* hdmi_tx_clk_init */

static void hdmi_tx_dev_deinit(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	switch_dev_unregister(&hdmi_ctrl->sdev);
	del_timer_sync(&hdmi_ctrl->hpd_state_timer);
	if (hdmi_ctrl->workq)
		destroy_workqueue(hdmi_ctrl->workq);
	mutex_destroy(&hdmi_ctrl->mutex);

	hdmi_tx_hw.ptr = NULL;
} /* hdmi_tx_dev_deinit */

static int hdmi_tx_dev_init(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;
	struct hdmi_tx_platform_data *pdata = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pdata = &hdmi_ctrl->pdata;

	rc = hdmi_tx_check_capability(pdata->io[HDMI_TX_QFPROM_IO].base);
	if (rc) {
		DEV_ERR("%s: no HDMI device\n", __func__);
		goto fail_no_hdmi;
	}

	/* irq enable/disable will be handled in hpd on/off */
	hdmi_tx_hw.ptr = (void *)hdmi_ctrl;

	hdmi_tx_setup_video_mode_lut();
	mutex_init(&hdmi_ctrl->mutex);
	hdmi_ctrl->workq = create_workqueue("hdmi_tx_workq");
	if (!hdmi_ctrl->workq) {
		DEV_ERR("%s: hdmi_tx_workq creation failed.\n", __func__);
		goto fail_create_workq;
	}

	INIT_WORK(&hdmi_ctrl->hpd_state_work, hdmi_tx_hpd_state_work);
	init_timer(&hdmi_ctrl->hpd_state_timer);
	hdmi_ctrl->hpd_state_timer.function = hdmi_tx_hpd_state_timer;
	hdmi_ctrl->hpd_state_timer.data = (u32)hdmi_ctrl;
	hdmi_ctrl->hpd_state_timer.expires = 0xffffffffL;

	hdmi_ctrl->sdev.name = "hdmi";
	if (switch_dev_register(&hdmi_ctrl->sdev) < 0) {
		DEV_ERR("%s: Hdmi switch registration failed\n", __func__);
		goto fail_switch_dev;
	}

	return 0;

fail_switch_dev:
	del_timer_sync(&hdmi_ctrl->hpd_state_timer);
fail_create_workq:
	if (hdmi_ctrl->workq)
		destroy_workqueue(hdmi_ctrl->workq);
	mutex_destroy(&hdmi_ctrl->mutex);
fail_no_hdmi:
	return rc;
} /* hdmi_tx_dev_init */

static int hdmi_tx_register_panel(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	hdmi_ctrl->panel_data.on = hdmi_tx_power_on;
	hdmi_ctrl->panel_data.off = hdmi_tx_power_off;

	hdmi_ctrl->video_resolution = HDMI_VFRMT_1920x1080p60_16_9;
	rc = hdmi_tx_init_panel_info(hdmi_ctrl->video_resolution,
		&hdmi_ctrl->panel_data.panel_info);
	if (rc) {
		DEV_ERR("%s: hdmi_init_panel_info failed\n", __func__);
		return rc;
	}

	rc = mdss_register_panel(&hdmi_ctrl->panel_data);
	if (rc) {
		DEV_ERR("%s: FAILED: to register HDMI panel\n", __func__);
		return rc;
	}

	return rc;
} /* hdmi_tx_register_panel */

static void hdmi_tx_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
} /* hdmi_tx_put_dt_vreg_data */

static int hdmi_tx_get_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp, u32 module_type)
{
	int i, j, rc = 0;
	int dt_vreg_total = 0, mod_vreg_total = 0;
	u32 ndx_mask = 0;
	u32 *val_array = NULL;
	const char *mod_name = NULL;
	struct device_node *of_node = NULL;
	char prop_name[32];

	if (!dev || !mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	switch (module_type) {
	case HDMI_TX_HPD_PM:
		mod_name = "hpd";
		break;
	case HDMI_TX_CORE_PM:
		mod_name = "core";
		break;
	case HDMI_TX_CEC_PM:
		mod_name = "cec";
		break;
	default:
		DEV_ERR("%s: invalid module type=%d\n", __func__,
			module_type);
		return -EINVAL;
	}

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_pm_name(module_type));

	of_node = dev->of_node;

	memset(prop_name, 0, sizeof(prop_name));
	snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME, "supply-names");
	dt_vreg_total = of_property_count_strings(of_node, prop_name);
	if (dt_vreg_total < 0) {
		DEV_ERR("%s: vreg not found. rc=%d\n", __func__,
			dt_vreg_total);
		rc = dt_vreg_total;
		goto error;
	}

	/* count how many vreg for particular hdmi module */
	for (i = 0; i < dt_vreg_total; i++) {
		const char *st = NULL;
		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME,
			"supply-names");
		rc = of_property_read_string_index(of_node,
			prop_name, i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}

		if (strnstr(st, mod_name, strlen(st))) {
			ndx_mask |= BIT(i);
			mod_vreg_total++;
		}
	}

	if (mod_vreg_total > 0) {
		mp->num_vreg = mod_vreg_total;
		mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
			mod_vreg_total, GFP_KERNEL);
		if (!mp->vreg_config) {
			DEV_ERR("%s: can't alloc '%s' vreg mem\n", __func__,
				hdmi_pm_name(module_type));
			goto error;
		}
	} else {
		DEV_DBG("%s: no vreg\n", __func__);
		return 0;
	}

	val_array = devm_kzalloc(dev, sizeof(u32) * dt_vreg_total, GFP_KERNEL);
	if (!val_array) {
		DEV_ERR("%s: can't allocate vreg scratch mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0, j = 0; (i < dt_vreg_total) && (j < mod_vreg_total); i++) {
		const char *st = NULL;

		if (!(ndx_mask & BIT(0))) {
			ndx_mask >>= 1;
			continue;
		}

		/* vreg-name */
		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME,
			"supply-names");
		rc = of_property_read_string_index(of_node,
			prop_name, i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}
		snprintf(mp->vreg_config[j].vreg_name, 32, "%s", st);

		/* vreg-type */
		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME,
			"supply-type");
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			prop_name, val_array, dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' vreg type. rc=%d\n",
				__func__, hdmi_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].type = val_array[i];

		/* vreg-min-voltage */
		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME,
			"min-voltage-level");
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			prop_name, val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' min volt. rc=%d\n",
				__func__, hdmi_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].min_voltage = val_array[i];

		/* vreg-max-voltage */
		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME,
			"max-voltage-level");
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			prop_name, val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' max volt. rc=%d\n",
				__func__, hdmi_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].max_voltage = val_array[i];

		/* vreg-op-mode */
		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME,
			"op-mode");
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			prop_name, val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' min volt. rc=%d\n",
				__func__, hdmi_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].optimum_voltage = val_array[i];

		DEV_DBG("%s: %s type=%d, min=%d, max=%d, op=%d\n",
			__func__, mp->vreg_config[j].vreg_name,
			mp->vreg_config[j].type,
			mp->vreg_config[j].min_voltage,
			mp->vreg_config[j].max_voltage,
			mp->vreg_config[j].optimum_voltage);

		ndx_mask >>= 1;
		j++;
	}

	devm_kfree(dev, val_array);

	return rc;

error:
	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_vreg_data(dev, mp);
	if (val_array)
		devm_kfree(dev, val_array);
	return rc;
} /* hdmi_tx_get_dt_vreg_data */

static void hdmi_tx_put_dt_gpio_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->gpio_config) {
		devm_kfree(dev, module_power->gpio_config);
		module_power->gpio_config = NULL;
	}
	module_power->num_gpio = 0;
} /* hdmi_tx_put_dt_gpio_data */

static int hdmi_tx_get_dt_gpio_data(struct device *dev,
	struct dss_module_power *mp, u32 module_type)
{
	int i, j, rc = 0;
	int dt_gpio_total = 0, mod_gpio_total = 0;
	u32 ndx_mask = 0;
	const char *mod_name = NULL;
	struct device_node *of_node = NULL;
	char prop_name[32];
	snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME, "gpio-names");

	if (!dev || !mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	switch (module_type) {
	case HDMI_TX_HPD_PM:
		mod_name = "hpd";
		break;
	case HDMI_TX_CORE_PM:
		mod_name = "core";
		break;
	case HDMI_TX_CEC_PM:
		mod_name = "cec";
		break;
	default:
		DEV_ERR("%s: invalid module type=%d\n", __func__,
			module_type);
		return -EINVAL;
	}

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_pm_name(module_type));

	of_node = dev->of_node;

	dt_gpio_total = of_gpio_count(of_node);
	if (dt_gpio_total < 0) {
		DEV_ERR("%s: gpio not found. rc=%d\n", __func__,
			dt_gpio_total);
		rc = dt_gpio_total;
		goto error;
	}

	/* count how many gpio for particular hdmi module */
	for (i = 0; i < dt_gpio_total; i++) {
		const char *st = NULL;

		rc = of_property_read_string_index(of_node,
			prop_name, i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}

		if (strnstr(st, mod_name, strlen(st))) {
			ndx_mask |= BIT(i);
			mod_gpio_total++;
			continue;
		}
	}

	if (mod_gpio_total > 0) {
		mp->num_gpio = mod_gpio_total;
		mp->gpio_config = devm_kzalloc(dev, sizeof(struct dss_gpio) *
			mod_gpio_total, GFP_KERNEL);
		if (!mp->gpio_config) {
			DEV_ERR("%s: can't alloc '%s' gpio mem\n", __func__,
				hdmi_pm_name(module_type));
			goto error;
		}
	} else {
		DEV_DBG("%s: no gpio\n", __func__);
		return 0;
	}


	for (i = 0, j = 0; (i < dt_gpio_total) && (j < mod_gpio_total); i++) {
		const char *st = NULL;

		if (!(ndx_mask & BIT(0))) {
			ndx_mask >>= 1;
			continue;
		}

		/* gpio-name */
		rc = of_property_read_string_index(of_node,
			prop_name, i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}
		snprintf(mp->gpio_config[j].gpio_name, 32, "%s", st);

		/* gpio-number */
		mp->gpio_config[j].gpio = of_get_gpio(of_node, i);

		DEV_DBG("%s: gpio num=%d, name=%s\n", __func__,
			mp->gpio_config[j].gpio,
			mp->gpio_config[j].gpio_name);

		ndx_mask >>= 1;
		j++;
	}

	return rc;

error:
	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_gpio_data(dev, mp);

	return rc;
} /* hdmi_tx_get_dt_gpio_data */

static struct resource *hdmi_tx_get_res_byname(struct platform_device *pdev,
	unsigned int type, const char *name)
{
	struct resource *res = NULL;

	res = platform_get_resource_byname(pdev, type, name);
	if (!res)
		DEV_ERR("%s: '%s' resource not found\n", __func__, name);

	return res;
} /* hdmi_tx_get_res_byname */

static int hdmi_tx_ioremap_byname(struct platform_device *pdev,
	struct hdmi_tx_io_data *io_data, u32 io_type)
{
	struct resource *res = NULL;

	if (!pdev) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	res = hdmi_tx_get_res_byname(pdev, IORESOURCE_MEM,
		hdmi_tx_io_name(io_type));
	if (!res) {
		DEV_ERR("%s: '%s' hdmi_tx_get_res_byname failed\n", __func__,
			hdmi_tx_io_name(io_type));
		return -ENODEV;
	}

	io_data->len = resource_size(res);
	io_data->base = ioremap(res->start, io_data->len);
	if (!io_data->base) {
		DEV_ERR("%s: '%s' ioremap failed\n", __func__,
			hdmi_tx_io_name(io_type));
		return -EIO;
	}

	return 0;
} /* hdmi_tx_ioremap_byname */

static void hdmi_tx_put_dt_data(struct device *dev,
	struct hdmi_tx_platform_data *pdata)
{
	int i;
	if (!dev || !pdata) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	hdmi_tx_clk_deinit(pdata);

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_vreg_data(dev, &pdata->power_data[i]);

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_gpio_data(dev, &pdata->power_data[i]);

	for (i = HDMI_TX_MAX_IO - 1; i >= 0; i--) {
		if (pdata->io[i].base)
			iounmap(pdata->io[i].base);
		pdata->io[i].base = NULL;
		pdata->io[i].len = 0;
	}
} /* hdmi_tx_put_dt_data */

static int hdmi_tx_get_dt_data(struct platform_device *pdev,
	struct hdmi_tx_platform_data *pdata)
{
	int i, rc = 0;
	struct device_node *of_node = NULL;

	if (!pdev || !pdata) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;

	rc = of_property_read_u32(of_node, "cell-index", &pdev->id);
	if (rc) {
		DEV_ERR("%s: dev id from dt not found.rc=%d\n",
			__func__, rc);
		goto error;
	}
	DEV_DBG("%s: id=%d\n", __func__, pdev->id);

	/* IO */
	for (i = 0; i < HDMI_TX_MAX_IO; i++) {
		rc = hdmi_tx_ioremap_byname(pdev, &pdata->io[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' remap failed\n", __func__,
				hdmi_tx_io_name(i));
			goto error;
		}
		DEV_INFO("%s: '%s': start = 0x%x, len=0x%x\n", __func__,
			hdmi_tx_io_name(i), (u32)pdata->io[i].base,
			pdata->io[i].len);
	}

	/* GPIO */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_get_dt_gpio_data(&pdev->dev,
			&pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_gpio_data failed.rc=%d\n",
				__func__, hdmi_pm_name(i), rc);
			goto error;
		}
	}

	/* VREG */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_get_dt_vreg_data(&pdev->dev,
			&pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
				__func__, hdmi_pm_name(i), rc);
			goto error;
		}
	}

	/* CLK */
	rc = hdmi_tx_clk_init(pdev, pdata);
	if (rc) {
		DEV_ERR("%s: FAILED: clk init. rc=%d\n", __func__, rc);
		goto error;
	}

	return rc;

error:
	hdmi_tx_put_dt_data(&pdev->dev, pdata);
	return rc;
} /* hdmi_tx_get_dt_data */

static int hdmi_tx_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device_node *of_node = pdev->dev.of_node;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (!of_node) {
		DEV_ERR("%s: FAILED: of_node not found\n", __func__);
		rc = -ENODEV;
		return rc;
	}

	hdmi_ctrl = devm_kzalloc(&pdev->dev, sizeof(*hdmi_ctrl), GFP_KERNEL);
	if (!hdmi_ctrl) {
		DEV_ERR("%s: FAILED: cannot alloc hdmi tx ctrl\n", __func__);
		rc = -ENOMEM;
		goto failed_no_mem;
	}

	platform_set_drvdata(pdev, hdmi_ctrl);
	hdmi_ctrl->pdev = pdev;

	rc = hdmi_tx_get_dt_data(pdev, &hdmi_ctrl->pdata);
	if (rc) {
		DEV_ERR("%s: FAILED: parsing device tree data. rc=%d\n",
			__func__, rc);
		goto failed_dt_data;
	}

	rc = hdmi_tx_dev_init(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: hdmi_tx_dev_init. rc=%d\n", __func__, rc);
		goto failed_dev_init;
	}

	rc = hdmi_tx_register_panel(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: register_panel. rc=%d\n", __func__, rc);
		goto failed_reg_panel;
	}

	rc = hdmi_tx_sysfs_create(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: hdmi_tx_sysfs_create failed.rc=%d\n",
			__func__, rc);
		goto failed_sysfs_create;
	}

	return rc;

failed_sysfs_create:
failed_reg_panel:
	hdmi_tx_dev_deinit(hdmi_ctrl);
failed_dev_init:
	hdmi_tx_put_dt_data(&pdev->dev, &hdmi_ctrl->pdata);
failed_dt_data:
	devm_kfree(&pdev->dev, hdmi_ctrl);
failed_no_mem:
	return rc;
} /* hdmi_tx_probe */

static int hdmi_tx_remove(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);
	if (!hdmi_ctrl) {
		DEV_ERR("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	hdmi_tx_sysfs_remove(hdmi_ctrl);
	hdmi_tx_dev_deinit(hdmi_ctrl);
	hdmi_tx_put_dt_data(&pdev->dev, &hdmi_ctrl->pdata);
	devm_kfree(&hdmi_ctrl->pdev->dev, hdmi_ctrl);

	return 0;
} /* hdmi_tx_remove */

static const struct of_device_id hdmi_tx_dt_match[] = {
	{.compatible = COMPATIBLE_NAME,},
};
MODULE_DEVICE_TABLE(of, hdmi_tx_dt_match);

static struct platform_driver this_driver = {
	.probe = hdmi_tx_probe,
	.remove = hdmi_tx_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = hdmi_tx_dt_match,
	},
};

static int __init hdmi_tx_drv_init(void)
{
	int rc;

	rc = platform_driver_register(&this_driver);
	if (rc)
		DEV_ERR("%s: FAILED: rc=%d\n", __func__, rc);

	return rc;
} /* hdmi_tx_drv_init */

static void __exit hdmi_tx_drv_exit(void)
{
	platform_driver_unregister(&this_driver);
} /* hdmi_tx_drv_exit */

module_init(hdmi_tx_drv_init);
module_exit(hdmi_tx_drv_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.3");
MODULE_DESCRIPTION("HDMI MSM TX driver");
