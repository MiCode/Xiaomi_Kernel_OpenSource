/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <linux/msm_hdmi.h>
#include <mach/msm_hdmi_audio_codec.h>

#define REG_DUMP 0

#include "mdss_debug.h"
#include "mdss_fb.h"
#include "mdss_hdmi_cec.h"
#include "mdss_hdmi_edid.h"
#include "mdss_hdmi_hdcp.h"
#include "mdss_hdmi_tx.h"
#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_hdmi_mhl.h"

#define DRV_NAME "hdmi-tx"
#define COMPATIBLE_NAME "qcom,hdmi-tx"

#define DEFAULT_VIDEO_RESOLUTION HDMI_VFRMT_640x480p60_4_3

/* HDMI PHY/PLL bit field macros */
#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)

#define HPD_DISCONNECT_POLARITY 0
#define HPD_CONNECT_POLARITY    1

/*
 * Audio engine may take 1 to 3 sec to shutdown
 * in normal cases. To handle worst cases, making
 * timeout for audio engine shutdown as 5 sec.
 */
#define AUDIO_POLL_SLEEP_US   (5 * 1000)
#define AUDIO_POLL_TIMEOUT_US (AUDIO_POLL_SLEEP_US * 1000)

#define IFRAME_CHECKSUM_32(d)			\
	((d & 0xff) + ((d >> 8) & 0xff) +	\
	((d >> 16) & 0xff) + ((d >> 24) & 0xff))

/* Enable HDCP by default */
static bool hdcp_feature_on = true;

/* Supported HDMI Audio channels */
#define MSM_HDMI_AUDIO_CHANNEL_2	2
#define MSM_HDMI_AUDIO_CHANNEL_3	3
#define MSM_HDMI_AUDIO_CHANNEL_4	4
#define MSM_HDMI_AUDIO_CHANNEL_5	5
#define MSM_HDMI_AUDIO_CHANNEL_6	6
#define MSM_HDMI_AUDIO_CHANNEL_7	7
#define MSM_HDMI_AUDIO_CHANNEL_8	8

#define NUM_MODES_AVI 20

/* AVI Infoframe data byte 3, bit 7 (msb) represents ITC bit */
#define SET_ITC_BIT(byte)  (byte = (byte | BIT(7)))
#define CLR_ITC_BIT(byte)  (byte = (byte & ~BIT(7)))

/*
 * CN represents IT content type, if ITC bit in infoframe data byte 3
 * is set, CN bits will represent content type as below:
 * 0b00 Graphics
 * 0b01 Photo
 * 0b10 Cinema
 * 0b11 Game
*/
#define CONFIG_CN_BITS(bits, byte) \
		(byte = (byte & ~(BIT(4) | BIT(5))) |\
			((bits & (BIT(0) | BIT(1))) << 4))

enum msm_hdmi_supported_audio_sample_rates {
	AUDIO_SAMPLE_RATE_32KHZ,
	AUDIO_SAMPLE_RATE_44_1KHZ,
	AUDIO_SAMPLE_RATE_48KHZ,
	AUDIO_SAMPLE_RATE_88_2KHZ,
	AUDIO_SAMPLE_RATE_96KHZ,
	AUDIO_SAMPLE_RATE_176_4KHZ,
	AUDIO_SAMPLE_RATE_192KHZ,
	AUDIO_SAMPLE_RATE_MAX
};

/* parameters for clock regeneration */
struct hdmi_tx_audio_acr {
	u32 n;
	u32 cts;
};

struct hdmi_tx_audio_acr_arry {
	u32 pclk;
	struct hdmi_tx_audio_acr lut[AUDIO_SAMPLE_RATE_MAX];
};

static int hdmi_tx_set_mhl_hpd(struct platform_device *pdev, uint8_t on);
static int hdmi_tx_sysfs_enable_hpd(struct hdmi_tx_ctrl *hdmi_ctrl, int on);
static irqreturn_t hdmi_tx_isr(int irq, void *data);
static void hdmi_tx_hpd_off(struct hdmi_tx_ctrl *hdmi_ctrl);
static int hdmi_tx_enable_power(struct hdmi_tx_ctrl *hdmi_ctrl,
	enum hdmi_tx_power_module_type module, int enable);
static inline void hdmi_tx_set_audio_switch_node(struct hdmi_tx_ctrl *hdmi_ctrl,
	int val, bool force);
static int hdmi_tx_audio_setup(struct hdmi_tx_ctrl *hdmi_ctrl);
static void hdmi_tx_en_encryption(struct hdmi_tx_ctrl *hdmi_ctrl, u32 on);

struct mdss_hw hdmi_tx_hw = {
	.hw_ndx = MDSS_HW_HDMI,
	.ptr = NULL,
	.irq_handler = hdmi_tx_isr,
};

struct dss_gpio hpd_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-hpd"},
	{0, 1, COMPATIBLE_NAME "-mux-en"},
	{0, 0, COMPATIBLE_NAME "-mux-sel"}
};

struct dss_gpio ddc_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-ddc-mux-sel"},
	{0, 1, COMPATIBLE_NAME "-ddc-clk"},
	{0, 1, COMPATIBLE_NAME "-ddc-data"}
};

struct dss_gpio core_gpio_config[] = {
};

struct dss_gpio cec_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-cec"}
};

const char *hdmi_pm_name(enum hdmi_tx_power_module_type module)
{
	switch (module) {
	case HDMI_TX_HPD_PM:	return "HDMI_TX_HPD_PM";
	case HDMI_TX_DDC_PM:	return "HDMI_TX_DDC_PM";
	case HDMI_TX_CORE_PM:	return "HDMI_TX_CORE_PM";
	case HDMI_TX_CEC_PM:	return "HDMI_TX_CEC_PM";
	default: return "???";
	}
} /* hdmi_pm_name */

static u8 hdmi_tx_avi_iframe_lut[][NUM_MODES_AVI] = {
	{0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,
	 0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,
	 0x10,	0x10}, /*00*/
	{0x18,	0x18,	0x28,	0x28,	0x28,	0x28,	0x28,	0x28,	0x28,
	 0x28,	0x28,	0x28,	0x28,	0x18,	0x28,	0x18,	0x28,	0x28,
	 0x28,	0x28}, /*01*/
	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00}, /*02*/
	{0x02,	0x06,	0x11,	0x15,	0x04,	0x13,	0x10,	0x05,	0x1F,
	 0x14,	0x20,	0x22,	0x21,	0x01,	0x03,	0x11,	0x00,	0x00,
	 0x00,	0x00}, /*03*/
	{0x00,	0x01,	0x00,	0x01,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00}, /*04*/
	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00}, /*05*/
	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00}, /*06*/
	{0xE1,	0xE1,	0x41,	0x41,	0xD1,	0xd1,	0x39,	0x39,	0x39,
	 0x39,	0x39,	0x39,	0x39,	0xe1,	0xE1,	0x41,	0x71,	0x71,
	 0x71,	0x71}, /*07*/
	{0x01,	0x01,	0x02,	0x02,	0x02,	0x02,	0x04,	0x04,	0x04,
	 0x04,	0x04,	0x04,	0x04,	0x01,	0x01,	0x02,	0x08,	0x08,
	 0x08,	0x08}, /*08*/
	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00}, /*09*/
	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00}, /*10*/
	{0xD1,	0xD1,	0xD1,	0xD1,	0x01,	0x01,	0x81,	0x81,	0x81,
	 0x81,	0x81,	0x81,	0x81,	0x81,	0xD1,	0xD1,	0x01,	0x01,
	 0x01,	0x01}, /*11*/
	{0x02,	0x02,	0x02,	0x02,	0x05,	0x05,	0x07,	0x07,	0x07,
	 0x07,	0x07,	0x07,	0x07,	0x02,	0x02,	0x02,	0x0F,	0x0F,
	 0x0F,	0x10}  /*12*/
};

/* Audio constants lookup table for hdmi_tx_audio_acr_setup */
/* Valid Pixel-Clock rates: 25.2MHz, 27MHz, 27.03MHz, 74.25MHz, 148.5MHz */
static const struct hdmi_tx_audio_acr_arry hdmi_tx_audio_acr_lut[] = {
	/*  25.200MHz  */
	{25200, {{4096, 25200}, {6272, 28000}, {6144, 25200}, {12544, 28000},
		{12288, 25200}, {25088, 28000}, {24576, 25200} } },
	/*  27.000MHz  */
	{27000, {{4096, 27000}, {6272, 30000}, {6144, 27000}, {12544, 30000},
		{12288, 27000}, {25088, 30000}, {24576, 27000} } },
	/*  27.027MHz */
	{27030, {{4096, 27027}, {6272, 30030}, {6144, 27027}, {12544, 30030},
		{12288, 27027}, {25088, 30030}, {24576, 27027} } },
	/*  74.250MHz */
	{74250, {{4096, 74250}, {6272, 82500}, {6144, 74250}, {12544, 82500},
		{12288, 74250}, {25088, 82500}, {24576, 74250} } },
	/* 148.500MHz */
	{148500, {{4096, 148500}, {6272, 165000}, {6144, 148500},
		{12544, 165000}, {12288, 148500}, {25088, 165000},
		{24576, 148500} } },
	/* 297.000MHz */
	{297000, {{3072, 222750}, {4704, 247500}, {5120, 247500},
		{9408, 247500}, {10240, 247500}, {18816, 247500},
		{20480, 247500} } },
};

int register_hdmi_cable_notification(struct hdmi_cable_notify *handler)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct list_head *pos;

	if (!hdmi_tx_hw.ptr) {
		DEV_WARN("%s: HDMI Tx core not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!handler) {
		DEV_ERR("%s: Empty handler\n", __func__);
		return -ENODEV;
	}

	hdmi_ctrl = (struct hdmi_tx_ctrl *) hdmi_tx_hw.ptr;

	mutex_lock(&hdmi_ctrl->cable_notify_mutex);
	handler->status = hdmi_ctrl->hpd_state;
	list_for_each(pos, &hdmi_ctrl->cable_notify_handlers);
	list_add_tail(&handler->link, pos);
	mutex_unlock(&hdmi_ctrl->cable_notify_mutex);

	return handler->status;
} /* register_hdmi_cable_notification */

int unregister_hdmi_cable_notification(struct hdmi_cable_notify *handler)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (!hdmi_tx_hw.ptr) {
		DEV_WARN("%s: HDMI Tx core not ready\n", __func__);
		return -ENODEV;
	}

	if (!handler) {
		DEV_ERR("%s: Empty handler\n", __func__);
		return -ENODEV;
	}

	hdmi_ctrl = (struct hdmi_tx_ctrl *) hdmi_tx_hw.ptr;

	mutex_lock(&hdmi_ctrl->cable_notify_mutex);
	list_del(&handler->link);
	mutex_unlock(&hdmi_ctrl->cable_notify_mutex);

	return 0;
} /* unregister_hdmi_cable_notification */

static void hdmi_tx_cable_notify_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct hdmi_cable_notify *pos;

	hdmi_ctrl = container_of(work, struct hdmi_tx_ctrl, cable_notify_work);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi data\n", __func__);
		return;
	}

	mutex_lock(&hdmi_ctrl->cable_notify_mutex);
	list_for_each_entry(pos, &hdmi_ctrl->cable_notify_handlers, link) {
		if (pos->status != hdmi_ctrl->hpd_state) {
			pos->status = hdmi_ctrl->hpd_state;
			pos->hpd_notify(pos);
		}
	}
	mutex_unlock(&hdmi_ctrl->cable_notify_mutex);
} /* hdmi_tx_cable_notify_work */

static bool hdmi_tx_is_cea_format(int mode)
{
	bool cea_fmt;

	if ((mode > 0) && (mode <= HDMI_EVFRMT_END))
		cea_fmt = true;
	else
		cea_fmt = false;

	DEV_DBG("%s: %s\n", __func__, cea_fmt ? "Yes" : "No");

	return cea_fmt;
}

const char *hdmi_tx_pm_name(enum hdmi_tx_power_module_type module)
{
	switch (module) {
	case HDMI_TX_HPD_PM:	return "HDMI_TX_HPD_PM";
	case HDMI_TX_DDC_PM:	return "HDMI_TX_DDC_PM";
	case HDMI_TX_CORE_PM:	return "HDMI_TX_CORE_PM";
	case HDMI_TX_CEC_PM:	return "HDMI_TX_CEC_PM";
	default: return "???";
	}
} /* hdmi_tx_pm_name */

static const char *hdmi_tx_io_name(u32 type)
{
	switch (type) {
	case HDMI_TX_CORE_IO:	return "core_physical";
	case HDMI_TX_PHY_IO:	return "phy_physical";
	case HDMI_TX_QFPROM_IO:	return "qfprom_physical";
	default:		return NULL;
	}
} /* hdmi_tx_io_name */

static int hdmi_tx_get_vic_from_panel_info(struct hdmi_tx_ctrl *hdmi_ctrl,
	struct mdss_panel_info *pinfo)
{
	int new_vic = -1;
	u32 h_total, v_total;
	struct msm_hdmi_mode_timing_info timing;

	if (!hdmi_ctrl || !pinfo) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (pinfo->vic) {
		if (hdmi_get_supported_mode(pinfo->vic)) {
			new_vic = pinfo->vic;
			DEV_DBG("%s: %s is supported\n", __func__,
				msm_hdmi_mode_2string(new_vic));
		} else {
			DEV_ERR("%s: invalid or not supported vic %d\n",
				__func__, pinfo->vic);
			return -EPERM;
		}
	} else {
		timing.active_h = pinfo->xres;
		timing.back_porch_h = pinfo->lcdc.h_back_porch;
		timing.front_porch_h = pinfo->lcdc.h_front_porch;
		timing.pulse_width_h = pinfo->lcdc.h_pulse_width;
		h_total = timing.active_h + timing.back_porch_h +
			timing.front_porch_h + timing.pulse_width_h;
		DEV_DBG("%s: ah=%d bph=%d fph=%d pwh=%d ht=%d\n", __func__,
			timing.active_h, timing.back_porch_h,
			timing.front_porch_h, timing.pulse_width_h, h_total);

		timing.active_v = pinfo->yres;
		timing.back_porch_v = pinfo->lcdc.v_back_porch;
		timing.front_porch_v = pinfo->lcdc.v_front_porch;
		timing.pulse_width_v = pinfo->lcdc.v_pulse_width;
		v_total = timing.active_v + timing.back_porch_v +
			timing.front_porch_v + timing.pulse_width_v;
		DEV_DBG("%s: av=%d bpv=%d fpv=%d pwv=%d vt=%d\n", __func__,
			timing.active_v, timing.back_porch_v,
			timing.front_porch_v, timing.pulse_width_v, v_total);

		timing.pixel_freq = pinfo->clk_rate / 1000;
		if (h_total && v_total) {
			timing.refresh_rate = ((timing.pixel_freq * 1000) /
				(h_total * v_total)) * 1000;
		} else {
			DEV_ERR("%s: cannot cal refresh rate\n", __func__);
			return -EPERM;
		}
		DEV_DBG("%s: pixel_freq=%d refresh_rate=%d\n", __func__,
			timing.pixel_freq, timing.refresh_rate);

		new_vic = hdmi_get_video_id_code(&timing);
	}

	return new_vic;
} /* hdmi_tx_get_vic_from_panel_info */

static inline void hdmi_tx_send_cable_notification(
	struct hdmi_tx_ctrl *hdmi_ctrl, int val)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (!hdmi_ctrl->pdata.primary && (hdmi_ctrl->sdev.state != val))
		switch_set_state(&hdmi_ctrl->sdev, val);

	/* Notify all registered modules of cable connection status */
	schedule_work(&hdmi_ctrl->cable_notify_work);
} /* hdmi_tx_send_cable_notification */

static inline u32 hdmi_tx_is_dvi_mode(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return hdmi_edid_get_sink_mode(
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]) ? 0 : 1;
} /* hdmi_tx_is_dvi_mode */

static void hdmi_tx_wait_for_audio_engine(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 status = 0;
	u32 wait_for_vote = 50;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return;
	}

	/*
	 * wait for 5 sec max for audio engine to acknowledge if hdmi tx core
	 * can be safely turned off. Sleep for a reasonable time to make sure
	 * vote_hdmi_core_on variable is updated properly by audio.
	 */
	while (hdmi_ctrl->vote_hdmi_core_on && --wait_for_vote)
		msleep(100);


	if (!wait_for_vote)
		DEV_ERR("%s: HDMI core still voted for power on\n", __func__);

	if (readl_poll_timeout(io->base + HDMI_AUDIO_PKT_CTRL, status,
				(status & BIT(0)) == 0, AUDIO_POLL_SLEEP_US,
				AUDIO_POLL_TIMEOUT_US))
		DEV_ERR("%s: Error turning off audio packet transmission.\n",
			__func__);

	if (readl_poll_timeout(io->base + HDMI_AUDIO_CFG, status,
				(status & BIT(0)) == 0, AUDIO_POLL_SLEEP_US,
				AUDIO_POLL_TIMEOUT_US))
		DEV_ERR("%s: Error turning off audio engine.\n", __func__);
}

static struct hdmi_tx_ctrl *hdmi_tx_get_drvdata_from_panel_data(
	struct mdss_panel_data *mpd)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (mpd) {
		hdmi_ctrl = container_of(mpd, struct hdmi_tx_ctrl, panel_data);
		if (!hdmi_ctrl)
			DEV_ERR("%s: hdmi_ctrl = NULL\n", __func__);
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

/* todo: Fix this. Right now this is declared in hdmi_util.h */
void *hdmi_get_featuredata_from_sysfs_dev(struct device *device,
	u32 feature_type)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (!device || feature_type >= HDMI_TX_FEAT_MAX) {
		DEV_ERR("%s: invalid input\n", __func__);
		return NULL;
	}

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(device);
	if (hdmi_ctrl)
		return hdmi_ctrl->feature_data[feature_type];
	else
		return NULL;

} /* hdmi_tx_get_featuredata_from_sysfs_dev */
EXPORT_SYMBOL(hdmi_get_featuredata_from_sysfs_dev);

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

	if (hdmi_ctrl->mhl_max_pclk && hpd &&
	    (!hdmi_ctrl->mhl_hpd_on || hdmi_ctrl->hpd_feature_on))
		return 0;

	if (0 == hpd && hdmi_ctrl->hpd_feature_on) {
		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, false);

		if (hdmi_ctrl->panel_power_on && hdmi_ctrl->hpd_state) {
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 0, false);
			hdmi_tx_wait_for_audio_engine(hdmi_ctrl);
		}

		hdmi_tx_send_cable_notification(hdmi_ctrl, 0);
		DEV_DBG("%s: Hdmi state switch to %d\n", __func__,
			hdmi_ctrl->sdev.state);
	} else if (1 == hpd && !hdmi_ctrl->hpd_feature_on) {
		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
	} else {
		DEV_DBG("%s: hpd is already '%s'. return\n", __func__,
			hdmi_ctrl->hpd_feature_on ? "enabled" : "disabled");
		return ret;
	}

	if (!rc) {
		hdmi_ctrl->hpd_feature_on =
			(~hdmi_ctrl->hpd_feature_on) & BIT(0);
		DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_feature_on);
	} else {
		DEV_ERR("%s: failed to '%s' hpd. rc = %d\n", __func__,
			hpd ? "enable" : "disable", rc);
		ret = rc;
	}

	return ret;
} /* hdmi_tx_sysfs_wta_hpd */

static ssize_t hdmi_tx_sysfs_wta_vendor_name(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret, sz;
	u8 *s = (u8 *) buf;
	u8 *d = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	d = hdmi_ctrl->spd_vendor_name;
	ret = strnlen(buf, PAGE_SIZE);
	ret = (ret > 8) ? 8 : ret;

	sz = sizeof(hdmi_ctrl->spd_vendor_name);
	memset(hdmi_ctrl->spd_vendor_name, 0, sz);
	while (*s) {
		if (*s & 0x60 && *s ^ 0x7f) {
			*d = *s;
		} else {
			/* stop copying if control character found */
			break;
		}

		if (++s > (u8 *) (buf + ret))
			break;

		d++;
	}
	hdmi_ctrl->spd_vendor_name[sz - 1] = 0;

	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_vendor_name);

	return ret;
} /* hdmi_tx_sysfs_wta_vendor_name */

static ssize_t hdmi_tx_sysfs_rda_vendor_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%s\n", hdmi_ctrl->spd_vendor_name);
	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_vendor_name);

	return ret;
} /* hdmi_tx_sysfs_rda_vendor_name */

static ssize_t hdmi_tx_sysfs_wta_product_description(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret, sz;
	u8 *s = (u8 *) buf;
	u8 *d = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	d = hdmi_ctrl->spd_product_description;
	ret = strnlen(buf, PAGE_SIZE);
	ret = (ret > 16) ? 16 : ret;

	sz = sizeof(hdmi_ctrl->spd_product_description);
	memset(hdmi_ctrl->spd_product_description, 0, sz);
	while (*s) {
		if (*s & 0x60 && *s ^ 0x7f) {
			*d = *s;
		} else {
			/* stop copying if control character found */
			break;
		}

		if (++s > (u8 *) (buf + ret))
			break;

		d++;
	}
	hdmi_ctrl->spd_product_description[sz - 1] = 0;

	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_product_description);

	return ret;
} /* hdmi_tx_sysfs_wta_product_description */

static ssize_t hdmi_tx_sysfs_rda_product_description(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%s\n",
		hdmi_ctrl->spd_product_description);
	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_product_description);

	return ret;
} /* hdmi_tx_sysfs_rda_product_description */

static ssize_t hdmi_tx_sysfs_wta_avi_itc(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	u8 *avi_byte3 = hdmi_tx_avi_iframe_lut[2];
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int loop = 0, itc = 0, rc = 0;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &itc);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc =%d\n", __func__, rc);
		return rc;
	}

	if (itc < 0 || itc > 1) {
		DEV_ERR("%s: Invalid ITC %d\n", __func__, itc);
		return ret;
	}

	if (mutex_lock_interruptible(&hdmi_ctrl->lut_lock))
		return -ERESTARTSYS;

	for (loop = 0; loop < NUM_MODES_AVI; loop++) {
		if (itc)
			SET_ITC_BIT(avi_byte3[loop]);
		else
			CLR_ITC_BIT(avi_byte3[loop]);
	}

	mutex_unlock(&hdmi_ctrl->lut_lock);

	return ret;
} /* hdmi_tx_sysfs_wta_avi_itc */

static ssize_t hdmi_tx_sysfs_wta_avi_cn_bits(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	u8 *avi_byte5 = hdmi_tx_avi_iframe_lut[4];
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int loop = 0, cn_bits = 0, rc = 0;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &cn_bits);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	/* As per CEA-861-E, CN is a positive number and can be max 3 */
	if (cn_bits < 0 || cn_bits > 3) {
		DEV_ERR("%s: Invalid CN %d\n", __func__, cn_bits);
		return ret;
	}

	if (mutex_lock_interruptible(&hdmi_ctrl->lut_lock))
		return -ERESTARTSYS;

	for (loop = 0; loop < NUM_MODES_AVI; loop++)
		CONFIG_CN_BITS(cn_bits, avi_byte5[loop]);

	mutex_unlock(&hdmi_ctrl->lut_lock);

	return ret;
} /* hdmi_tx_sysfs_wta_cn_bits */

static DEVICE_ATTR(connected, S_IRUGO, hdmi_tx_sysfs_rda_connected, NULL);
static DEVICE_ATTR(hpd, S_IRUGO | S_IWUSR, hdmi_tx_sysfs_rda_hpd,
	hdmi_tx_sysfs_wta_hpd);
static DEVICE_ATTR(vendor_name, S_IRUGO | S_IWUSR,
	hdmi_tx_sysfs_rda_vendor_name, hdmi_tx_sysfs_wta_vendor_name);
static DEVICE_ATTR(product_description, S_IRUGO | S_IWUSR,
	hdmi_tx_sysfs_rda_product_description,
	hdmi_tx_sysfs_wta_product_description);
static DEVICE_ATTR(avi_itc, S_IWUSR, NULL, hdmi_tx_sysfs_wta_avi_itc);
static DEVICE_ATTR(avi_cn0_1, S_IWUSR, NULL, hdmi_tx_sysfs_wta_avi_cn_bits);

static struct attribute *hdmi_tx_fs_attrs[] = {
	&dev_attr_connected.attr,
	&dev_attr_hpd.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_product_description.attr,
	&dev_attr_avi_itc.attr,
	&dev_attr_avi_cn0_1.attr,
	NULL,
};
static struct attribute_group hdmi_tx_fs_attrs_group = {
	.attrs = hdmi_tx_fs_attrs,
};

static int hdmi_tx_sysfs_create(struct hdmi_tx_ctrl *hdmi_ctrl,
	struct fb_info *fbi)
{
	int rc;

	if (!hdmi_ctrl || !fbi) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	rc = sysfs_create_group(&fbi->dev->kobj,
		&hdmi_tx_fs_attrs_group);
	if (rc) {
		DEV_ERR("%s: failed, rc=%d\n", __func__, rc);
		return rc;
	}
	hdmi_ctrl->kobj = &fbi->dev->kobj;
	DEV_DBG("%s: sysfs group %p\n", __func__, hdmi_ctrl->kobj);

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

static inline void hdmi_tx_set_audio_switch_node(struct hdmi_tx_ctrl *hdmi_ctrl,
	int val, bool force)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl) &&
		(force || (hdmi_ctrl->audio_sdev.state != val))) {
		switch_set_state(&hdmi_ctrl->audio_sdev, val);
		DEV_INFO("%s: hdmi_audio state switched to %d\n", __func__,
			hdmi_ctrl->audio_sdev.state);
	}
} /* hdmi_tx_set_audio_switch_node */

static int hdmi_tx_config_avmute(struct hdmi_tx_ctrl *hdmi_ctrl, int set)
{
	struct dss_io_data *io;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return -EINVAL;
	}

	if (set)
		DSS_REG_W(io, HDMI_GC,
			DSS_REG_R(io, HDMI_GC) | BIT(0));
	else
		DSS_REG_W(io, HDMI_GC,
			DSS_REG_R(io, HDMI_GC) & ~BIT(0));

	/* Enable AV Mute tranmission here */
	DSS_REG_W(io, HDMI_VBI_PKT_CTRL,
		DSS_REG_R(io, HDMI_VBI_PKT_CTRL) | (BIT(4) & BIT(5)));

	DEV_DBG("%s: AVMUTE %s\n", __func__, set ? "set" : "cleared");

	return 0;
} /* hdmi_tx_config_avmute */

void hdmi_tx_hdcp_cb(void *ptr, enum hdmi_hdcp_state status)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = (struct hdmi_tx_ctrl *)ptr;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	DEV_DBG("%s: HDCP status=%s hpd_state=%d\n", __func__,
		hdcp_state_name(status), hdmi_ctrl->hpd_state);

	switch (status) {
	case HDCP_STATE_AUTHENTICATED:
		if (hdmi_ctrl->hpd_state) {
			if (hdmi_ctrl->pdata.primary)
				hdmi_tx_en_encryption(hdmi_ctrl, true);
			else
				/* Clear AV Mute */
				rc = hdmi_tx_config_avmute(hdmi_ctrl, 0);
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 1, false);
		}
		break;
	case HDCP_STATE_AUTH_FAIL:
		hdmi_tx_set_audio_switch_node(hdmi_ctrl, 0, false);

		if (hdmi_ctrl->hpd_state) {
			if (hdmi_ctrl->pdata.primary)
				hdmi_tx_en_encryption(hdmi_ctrl, false);
			else
				/* Set AV Mute */
				rc = hdmi_tx_config_avmute(hdmi_ctrl, 1);

			DEV_DBG("%s: Reauthenticating\n", __func__);
			rc = hdmi_hdcp_reauthenticate(
				hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP]);
			if (rc)
				DEV_ERR("%s: HDCP reauth failed. rc=%d\n",
					__func__, rc);
		} else {
			DEV_DBG("%s: Not reauthenticating. Cable not conn\n",
				__func__);
		}

		break;
	case HDCP_STATE_AUTHENTICATING:
	case HDCP_STATE_INACTIVE:
	default:
		break;
		/* do nothing */
	}
}

/* Enable HDMI features */
static int hdmi_tx_init_features(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdmi_edid_init_data edid_init_data;
	struct hdmi_hdcp_init_data hdcp_init_data;
	struct hdmi_cec_init_data cec_init_data;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* Initialize EDID feature */
	edid_init_data.io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	edid_init_data.mutex = &hdmi_ctrl->mutex;
	edid_init_data.sysfs_kobj = hdmi_ctrl->kobj;
	edid_init_data.ddc_ctrl = &hdmi_ctrl->ddc_ctrl;

	hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID] =
		hdmi_edid_init(&edid_init_data);
	if (!hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]) {
		DEV_ERR("%s: hdmi_edid_init failed\n", __func__);
		return -EPERM;
	}
	hdmi_edid_set_video_resolution(
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID],
		hdmi_ctrl->video_resolution);

	/* Initialize HDCP feature */
	if (hdmi_ctrl->present_hdcp) {
		hdcp_init_data.core_io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
		hdcp_init_data.qfprom_io =
			&hdmi_ctrl->pdata.io[HDMI_TX_QFPROM_IO];
		hdcp_init_data.mutex = &hdmi_ctrl->mutex;
		hdcp_init_data.sysfs_kobj = hdmi_ctrl->kobj;
		hdcp_init_data.ddc_ctrl = &hdmi_ctrl->ddc_ctrl;
		hdcp_init_data.workq = hdmi_ctrl->workq;
		hdcp_init_data.notify_status = hdmi_tx_hdcp_cb;
		hdcp_init_data.cb_data = (void *)hdmi_ctrl;

		hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP] =
			hdmi_hdcp_init(&hdcp_init_data);
		if (!hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP]) {
			DEV_ERR("%s: hdmi_hdcp_init failed\n", __func__);
			hdmi_edid_deinit(hdmi_ctrl->feature_data[
				HDMI_TX_FEAT_EDID]);
			hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID] = NULL;
			return -EPERM;
		}

		DEV_DBG("%s: HDCP feature initialized\n", __func__);
	}

	/* Initialize CEC feature */
	cec_init_data.io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	cec_init_data.sysfs_kobj = hdmi_ctrl->kobj;
	cec_init_data.workq = hdmi_ctrl->workq;

	hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC] =
		hdmi_cec_init(&cec_init_data);
	if (!hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC])
		DEV_WARN("%s: hdmi_cec_init failed\n", __func__);

	return 0;
} /* hdmi_tx_init_features */

static inline u32 hdmi_tx_is_controller_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	return DSS_REG_R_ND(io, HDMI_CTRL) & BIT(0);
} /* hdmi_tx_is_controller_on */

static int hdmi_tx_init_panel_info(uint32_t resolution,
	struct mdss_panel_info *pinfo)
{
	const struct msm_hdmi_mode_timing_info *timing =
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

/* Table tuned to indicate video formats supported by the MHL Tx */
/* Valid pclk rates (Mhz): 25.2, 27, 27.03, 74.25 */
static void hdmi_tx_setup_mhl_video_mode_lut(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 i;
	struct msm_hdmi_mode_timing_info *temp_timing;

	if (!hdmi_ctrl->mhl_max_pclk) {
		DEV_WARN("%s: mhl max pclk not set!\n", __func__);
		return;
	}
	DEV_DBG("%s: max mode set to [%u]\n",
		__func__, hdmi_ctrl->mhl_max_pclk);
	for (i = 0; i < HDMI_VFRMT_MAX; i++) {
		temp_timing =
		(struct msm_hdmi_mode_timing_info *)hdmi_get_supported_mode(i);
		if (!temp_timing)
			continue;
		/* formats that exceed max mhl line clk bw */
		if (temp_timing->pixel_freq > hdmi_ctrl->mhl_max_pclk)
			hdmi_del_supported_mode(i);
	}
} /* hdmi_tx_setup_mhl_video_mode_lut */

static int hdmi_tx_read_sink_info(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int status;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!hdmi_tx_is_controller_on(hdmi_ctrl)) {
		DEV_ERR("%s: failed: HDMI controller is off", __func__);
		status = -ENXIO;
		goto error;
	}

	hdmi_ddc_config(&hdmi_ctrl->ddc_ctrl);

	status = hdmi_edid_read(hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]);
	if (!status)
		DEV_DBG("%s: hdmi_edid_read success\n", __func__);
	else
		DEV_ERR("%s: hdmi_edid_read failed\n", __func__);

error:
	return status;
} /* hdmi_tx_read_sink_info */

static void hdmi_tx_hpd_int_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct dss_io_data *io;

	hdmi_ctrl = container_of(work, struct hdmi_tx_ctrl, hpd_int_work);
	if (!hdmi_ctrl || !hdmi_ctrl->hpd_initialized) {
		DEV_DBG("%s: invalid input\n", __func__);
		return;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	DEV_DBG("%s: Got HPD interrupt\n", __func__);

	if (hdmi_ctrl->hpd_state) {
		if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, true)) {
			DEV_ERR("%s: Failed to enable ddc power\n", __func__);
			return;
		}
		/* Enable SW DDC before EDID read */
		DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION ,
			DSS_REG_R(io, HDMI_DDC_ARBITRATION) & ~(BIT(4)));

		hdmi_tx_read_sink_info(hdmi_ctrl);
		hdmi_tx_send_cable_notification(hdmi_ctrl, 1);
		DEV_INFO("%s: sense cable CONNECTED: state switch to %d\n",
			__func__, hdmi_ctrl->sdev.state);
	} else {
		hdmi_tx_set_audio_switch_node(hdmi_ctrl, 0, false);
		hdmi_tx_wait_for_audio_engine(hdmi_ctrl);

		hdmi_tx_send_cable_notification(hdmi_ctrl, 0);
		DEV_INFO("%s: sense cable DISCONNECTED: state switch to %d\n",
			__func__, hdmi_ctrl->sdev.state);
	}

	if (!completion_done(&hdmi_ctrl->hpd_done))
		complete_all(&hdmi_ctrl->hpd_done);
} /* hdmi_tx_hpd_int_work */

static int hdmi_tx_check_capability(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 hdmi_disabled, hdcp_disabled;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_QFPROM_IO];
	if (!io->base) {
		DEV_ERR("%s: QFPROM io is not initialized\n", __func__);
		return -EINVAL;
	}

	hdcp_disabled = DSS_REG_R_ND(io,
		QFPROM_RAW_FEAT_CONFIG_ROW0_LSB) & BIT(31);

	hdmi_disabled = DSS_REG_R_ND(io,
		QFPROM_RAW_FEAT_CONFIG_ROW0_MSB) & BIT(0);

	DEV_DBG("%s: Features <HDMI:%s, HDCP:%s>\n", __func__,
		hdmi_disabled ? "OFF" : "ON", hdcp_disabled ? "OFF" : "ON");

	if (hdmi_disabled) {
		DEV_ERR("%s: HDMI disabled\n", __func__);
		return -ENODEV;
	}

	if (hdcp_disabled) {
		hdmi_ctrl->present_hdcp = 0;
		DEV_WARN("%s: HDCP disabled\n", __func__);
	} else {
		hdmi_ctrl->present_hdcp = 1;
		DEV_DBG("%s: Device is HDCP enabled\n", __func__);
	}

	return 0;
} /* hdmi_tx_check_capability */

static int hdmi_tx_set_video_fmt(struct hdmi_tx_ctrl *hdmi_ctrl,
	struct mdss_panel_info *pinfo)
{
	int new_vic = -1;
	const struct msm_hdmi_mode_timing_info *timing = NULL;

	if (!hdmi_ctrl || !pinfo) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	new_vic = hdmi_tx_get_vic_from_panel_info(hdmi_ctrl, pinfo);
	if ((new_vic < 0) || (new_vic > HDMI_VFRMT_MAX)) {
		DEV_ERR("%s: invalid or not supported vic\n", __func__);
		return -EPERM;
	}

	DEV_DBG("%s: switching from %s => %s", __func__,
		msm_hdmi_mode_2string(hdmi_ctrl->video_resolution),
		msm_hdmi_mode_2string(new_vic));

	hdmi_ctrl->video_resolution = (u32)new_vic;

	timing = hdmi_get_supported_mode(hdmi_ctrl->video_resolution);

	/* todo: find a better way */
	hdmi_ctrl->pdata.power_data[HDMI_TX_CORE_PM].clk_config[0].rate =
		timing->pixel_freq * 1000;

	hdmi_edid_set_video_resolution(
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID],
		hdmi_ctrl->video_resolution);

	return 0;
} /* hdmi_tx_set_video_fmt */

static int hdmi_tx_video_setup(struct hdmi_tx_ctrl *hdmi_ctrl,
	int video_format)
{
	u32 total_v   = 0;
	u32 total_h   = 0;
	u32 start_h   = 0;
	u32 end_h     = 0;
	u32 start_v   = 0;
	u32 end_v     = 0;
	struct dss_io_data *io = NULL;

	const struct msm_hdmi_mode_timing_info *timing =
		hdmi_get_supported_mode(video_format);
	if (timing == NULL) {
		DEV_ERR("%s: video format not supported: %d\n", __func__,
			video_format);
		return -EPERM;
	}

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return -EPERM;
	}

	total_h = timing->active_h + timing->front_porch_h +
		timing->back_porch_h + timing->pulse_width_h - 1;
	total_v = timing->active_v + timing->front_porch_v +
		timing->back_porch_v + timing->pulse_width_v - 1;
	if (((total_v << 16) & 0xE0000000) || (total_h & 0xFFFFE000)) {
		DEV_ERR("%s: total v=%d or h=%d is larger than supported\n",
			__func__, total_v, total_h);
		return -EPERM;
	}
	DSS_REG_W(io, HDMI_TOTAL, (total_v << 16) | (total_h << 0));

	start_h = timing->back_porch_h + timing->pulse_width_h;
	end_h   = (total_h + 1) - timing->front_porch_h;
	if (((end_h << 16) & 0xE0000000) || (start_h & 0xFFFFE000)) {
		DEV_ERR("%s: end_h=%d or start_h=%d is larger than supported\n",
			__func__, end_h, start_h);
		return -EPERM;
	}
	DSS_REG_W(io, HDMI_ACTIVE_H, (end_h << 16) | (start_h << 0));

	start_v = timing->back_porch_v + timing->pulse_width_v - 1;
	end_v   = total_v - timing->front_porch_v;
	if (((end_v << 16) & 0xE0000000) || (start_v & 0xFFFFE000)) {
		DEV_ERR("%s: end_v=%d or start_v=%d is larger than supported\n",
			__func__, end_v, start_v);
		return -EPERM;
	}
	DSS_REG_W(io, HDMI_ACTIVE_V, (end_v << 16) | (start_v << 0));

	if (timing->interlaced) {
		DSS_REG_W(io, HDMI_V_TOTAL_F2, (total_v + 1) << 0);
		DSS_REG_W(io, HDMI_ACTIVE_V_F2,
			((end_v + 1) << 16) | ((start_v + 1) << 0));
	} else {
		DSS_REG_W(io, HDMI_V_TOTAL_F2, 0);
		DSS_REG_W(io, HDMI_ACTIVE_V_F2, 0);
	}

	DSS_REG_W(io, HDMI_FRAME_CTRL,
		((timing->interlaced << 31) & 0x80000000) |
		((timing->active_low_h << 29) & 0x20000000) |
		((timing->active_low_v << 28) & 0x10000000));

	return 0;
} /* hdmi_tx_video_setup */

static void hdmi_tx_set_avi_infoframe(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int i, mode = 0;
	u8 avi_iframe[16]; /* two header + length + 13 data */
	u8 checksum;
	u32 sum, regVal;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	switch (hdmi_ctrl->video_resolution) {
	case HDMI_VFRMT_720x480p60_4_3:
		mode = 0;
		break;
	case HDMI_VFRMT_720x480i60_16_9:
		mode = 1;
		break;
	case HDMI_VFRMT_720x576p50_16_9:
		mode = 2;
		break;
	case HDMI_VFRMT_720x576i50_16_9:
		mode = 3;
		break;
	case HDMI_VFRMT_1280x720p60_16_9:
		mode = 4;
		break;
	case HDMI_VFRMT_1280x720p50_16_9:
		mode = 5;
		break;
	case HDMI_VFRMT_1920x1080p60_16_9:
		mode = 6;
		break;
	case HDMI_VFRMT_1920x1080i60_16_9:
		mode = 7;
		break;
	case HDMI_VFRMT_1920x1080p50_16_9:
		mode = 8;
		break;
	case HDMI_VFRMT_1920x1080i50_16_9:
		mode = 9;
		break;
	case HDMI_VFRMT_1920x1080p24_16_9:
		mode = 10;
		break;
	case HDMI_VFRMT_1920x1080p30_16_9:
		mode = 11;
		break;
	case HDMI_VFRMT_1920x1080p25_16_9:
		mode = 12;
		break;
	case HDMI_VFRMT_640x480p60_4_3:
		mode = 13;
		break;
	case HDMI_VFRMT_720x480p60_16_9:
		mode = 14;
		break;
	case HDMI_VFRMT_720x576p50_4_3:
		mode = 15;
		break;
	case HDMI_VFRMT_3840x2160p30_16_9:
		mode = 16;
		break;
	case HDMI_VFRMT_3840x2160p25_16_9:
		mode = 17;
		break;
	case HDMI_VFRMT_3840x2160p24_16_9:
		mode = 18;
		break;
	case HDMI_VFRMT_4096x2160p24_16_9:
		mode = 19;
		break;
	default:
		DEV_INFO("%s: mode %d not supported\n", __func__,
			hdmi_ctrl->video_resolution);
		return;
	}

	/* InfoFrame Type = 82 */
	avi_iframe[0]  = 0x82;
	/* Version = 2 */
	avi_iframe[1]  = 2;
	/* Length of AVI InfoFrame = 13 */
	avi_iframe[2]  = 13;

	/* Data Byte 01: 0 Y1 Y0 A0 B1 B0 S1 S0 */
	avi_iframe[3]  = hdmi_tx_avi_iframe_lut[0][mode];
	avi_iframe[3] |= hdmi_edid_get_sink_scaninfo(
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID],
		hdmi_ctrl->video_resolution);

	/* Data Byte 02: C1 C0 M1 M0 R3 R2 R1 R0 */
	avi_iframe[4]  = hdmi_tx_avi_iframe_lut[1][mode];
	/* Data Byte 03: ITC EC2 EC1 EC0 Q1 Q0 SC1 SC0 */
	avi_iframe[5]  = hdmi_tx_avi_iframe_lut[2][mode];
	/* Data Byte 04: 0 VIC6 VIC5 VIC4 VIC3 VIC2 VIC1 VIC0 */
	avi_iframe[6]  = hdmi_tx_avi_iframe_lut[3][mode];
	/* Data Byte 05: 0 0 0 0 PR3 PR2 PR1 PR0 */
	avi_iframe[7]  = hdmi_tx_avi_iframe_lut[4][mode];
	/* Data Byte 06: LSB Line No of End of Top Bar */
	avi_iframe[8]  = hdmi_tx_avi_iframe_lut[5][mode];
	/* Data Byte 07: MSB Line No of End of Top Bar */
	avi_iframe[9]  = hdmi_tx_avi_iframe_lut[6][mode];
	/* Data Byte 08: LSB Line No of Start of Bottom Bar */
	avi_iframe[10] = hdmi_tx_avi_iframe_lut[7][mode];
	/* Data Byte 09: MSB Line No of Start of Bottom Bar */
	avi_iframe[11] = hdmi_tx_avi_iframe_lut[8][mode];
	/* Data Byte 10: LSB Pixel Number of End of Left Bar */
	avi_iframe[12] = hdmi_tx_avi_iframe_lut[9][mode];
	/* Data Byte 11: MSB Pixel Number of End of Left Bar */
	avi_iframe[13] = hdmi_tx_avi_iframe_lut[10][mode];
	/* Data Byte 12: LSB Pixel Number of Start of Right Bar */
	avi_iframe[14] = hdmi_tx_avi_iframe_lut[11][mode];
	/* Data Byte 13: MSB Pixel Number of Start of Right Bar */
	avi_iframe[15] = hdmi_tx_avi_iframe_lut[12][mode];

	sum = 0;
	for (i = 0; i < 16; i++)
		sum += avi_iframe[i];
	sum &= 0xFF;
	sum = 256 - sum;
	checksum = (u8) sum;

	regVal = avi_iframe[5];
	regVal = regVal << 8 | avi_iframe[4];
	regVal = regVal << 8 | avi_iframe[3];
	regVal = regVal << 8 | checksum;
	DSS_REG_W(io, HDMI_AVI_INFO0, regVal);

	regVal = avi_iframe[9];
	regVal = regVal << 8 | avi_iframe[8];
	regVal = regVal << 8 | avi_iframe[7];
	regVal = regVal << 8 | avi_iframe[6];
	DSS_REG_W(io, HDMI_AVI_INFO1, regVal);

	regVal = avi_iframe[13];
	regVal = regVal << 8 | avi_iframe[12];
	regVal = regVal << 8 | avi_iframe[11];
	regVal = regVal << 8 | avi_iframe[10];
	DSS_REG_W(io, HDMI_AVI_INFO2, regVal);

	regVal = avi_iframe[1];
	regVal = regVal << 16 | avi_iframe[15];
	regVal = regVal << 8 | avi_iframe[14];
	DSS_REG_W(io, HDMI_AVI_INFO3, regVal);

	/* AVI InfFrame enable (every frame) */
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0,
		DSS_REG_R(io, HDMI_INFOFRAME_CTRL0) | BIT(1) | BIT(0));
} /* hdmi_tx_set_avi_infoframe */

/* todo: add 3D support */
static void hdmi_tx_set_vendor_specific_infoframe(
	struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int i;
	u8 vs_iframe[9]; /* two header + length + 6 data */
	u32 sum, reg_val;
	u32 hdmi_vic, hdmi_video_format;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	/* HDMI Spec 1.4a Table 8-10 */
	vs_iframe[0] = 0x81; /* type */
	vs_iframe[1] = 0x1;  /* version */
	vs_iframe[2] = 0x8;  /* length */

	vs_iframe[3] = 0x0; /* PB0: checksum */

	/* PB1..PB3: 24 Bit IEEE Registration Code 00_0C_03 */
	vs_iframe[4] = 0x03;
	vs_iframe[5] = 0x0C;
	vs_iframe[6] = 0x00;

	hdmi_video_format = 0x1;
	switch (hdmi_ctrl->video_resolution) {
	case HDMI_VFRMT_3840x2160p30_16_9:
		hdmi_vic = 0x1;
		break;
	case HDMI_VFRMT_3840x2160p25_16_9:
		hdmi_vic = 0x2;
		break;
	case HDMI_VFRMT_3840x2160p24_16_9:
		hdmi_vic = 0x3;
		break;
	case HDMI_VFRMT_4096x2160p24_16_9:
		hdmi_vic = 0x4;
		break;
	default:
		hdmi_video_format = 0x0;
		hdmi_vic = 0x0;
	}

	/* PB4: HDMI Video Format[7:5],  Reserved[4:0] */
	vs_iframe[7] = (hdmi_video_format << 5) & 0xE0;

	/* PB5: HDMI_VIC or 3D_Structure[7:4], Reserved[3:0] */
	vs_iframe[8] = hdmi_vic;

	/* compute checksum */
	sum = 0;
	for (i = 0; i < 9; i++)
		sum += vs_iframe[i];

	sum &= 0xFF;
	sum = 256 - sum;
	vs_iframe[3] = (u8)sum;

	reg_val = (hdmi_vic << 16) | (vs_iframe[3] << 8) |
		(hdmi_video_format << 5) | vs_iframe[2];
	DSS_REG_W(io, HDMI_VENSPEC_INFO0, reg_val);

	/* vendor specific info-frame enable (every frame) */
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0,
		DSS_REG_R(io, HDMI_INFOFRAME_CTRL0) | BIT(13) | BIT(12));
} /* hdmi_tx_set_vendor_specific_infoframe */

static void hdmi_tx_set_spd_infoframe(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 packet_header  = 0;
	u32 check_sum      = 0;
	u32 packet_payload = 0;
	u32 packet_control = 0;

	u8 *vendor_name = NULL;
	u8 *product_description = NULL;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	vendor_name = hdmi_ctrl->spd_vendor_name;
	product_description = hdmi_ctrl->spd_product_description;

	/* Setup Packet header and payload */
	/*
	 * 0x83 InfoFrame Type Code
	 * 0x01 InfoFrame Version Number
	 * 0x19 Length of Source Product Description InfoFrame
	 */
	packet_header  = 0x83 | (0x01 << 8) | (0x19 << 16);
	DSS_REG_W(io, HDMI_GENERIC1_HDR, packet_header);
	check_sum += IFRAME_CHECKSUM_32(packet_header);

	packet_payload = (vendor_name[3] & 0x7f)
		| ((vendor_name[4] & 0x7f) << 8)
		| ((vendor_name[5] & 0x7f) << 16)
		| ((vendor_name[6] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_1, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	/* Product Description (7-bit ASCII code) */
	packet_payload = (vendor_name[7] & 0x7f)
		| ((product_description[0] & 0x7f) << 8)
		| ((product_description[1] & 0x7f) << 16)
		| ((product_description[2] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_2, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	packet_payload = (product_description[3] & 0x7f)
		| ((product_description[4] & 0x7f) << 8)
		| ((product_description[5] & 0x7f) << 16)
		| ((product_description[6] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_3, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	packet_payload = (product_description[7] & 0x7f)
		| ((product_description[8] & 0x7f) << 8)
		| ((product_description[9] & 0x7f) << 16)
		| ((product_description[10] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_4, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	packet_payload = (product_description[11] & 0x7f)
		| ((product_description[12] & 0x7f) << 8)
		| ((product_description[13] & 0x7f) << 16)
		| ((product_description[14] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_5, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	/*
	 * Source Device Information
	 * 00h unknown
	 * 01h Digital STB
	 * 02h DVD
	 * 03h D-VHS
	 * 04h HDD Video
	 * 05h DVC
	 * 06h DSC
	 * 07h Video CD
	 * 08h Game
	 * 09h PC general
	 */
	packet_payload = (product_description[15] & 0x7f) | 0x00 << 8;
	DSS_REG_W(io, HDMI_GENERIC1_6, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	/* Vendor Name (7bit ASCII code) */
	packet_payload = ((vendor_name[0] & 0x7f) << 8)
		| ((vendor_name[1] & 0x7f) << 16)
		| ((vendor_name[2] & 0x7f) << 24);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);
	packet_payload |= ((0x100 - (0xff & check_sum)) & 0xff);
	DSS_REG_W(io, HDMI_GENERIC1_0, packet_payload);

	/*
	 * GENERIC1_LINE | GENERIC1_CONT | GENERIC1_SEND
	 * Setup HDMI TX generic packet control
	 * Enable this packet to transmit every frame
	 * Enable HDMI TX engine to transmit Generic packet 1
	 */
	packet_control = DSS_REG_R_ND(io, HDMI_GEN_PKT_CTRL);
	packet_control |= ((0x1 << 24) | (1 << 5) | (1 << 4));
	DSS_REG_W(io, HDMI_GEN_PKT_CTRL, packet_control);
} /* hdmi_tx_set_spd_infoframe */

static void hdmi_tx_en_encryption(struct hdmi_tx_ctrl *hdmi_ctrl, u32 on)
{
	u32 reg_val;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl->hdcp_feature_on || !hdmi_ctrl->present_hdcp)
		return;

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];

	mutex_lock(&hdmi_ctrl->mutex);
	reg_val = DSS_REG_R_ND(io, HDMI_CTRL);

	if (on)
		reg_val |= BIT(2);
	else
		reg_val &= ~BIT(2);
	DSS_REG_W(io, HDMI_CTRL, reg_val);

	mutex_unlock(&hdmi_ctrl->mutex);
} /* hdmi_tx_en_encryption */

static void hdmi_tx_set_mode(struct hdmi_tx_ctrl *hdmi_ctrl, u32 power_on)
{
	struct dss_io_data *io = NULL;
	/* Defaults: Disable block, HDMI mode */
	u32 reg_val = BIT(1);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	mutex_lock(&hdmi_ctrl->mutex);
	if (power_on) {
		/* Enable the block */
		reg_val |= BIT(0);

		/* HDMI Encryption, if HDCP is enabled */
		if (hdmi_ctrl->hdcp_feature_on &&
			hdmi_ctrl->present_hdcp && !hdmi_ctrl->pdata.primary)
			reg_val |= BIT(2);

		/* Set transmission mode to DVI based in EDID info */
		if (hdmi_edid_get_sink_mode(
			hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]) == 0)
			reg_val &= ~BIT(1); /* DVI mode */
	}

	DSS_REG_W(io, HDMI_CTRL, reg_val);
	mutex_unlock(&hdmi_ctrl->mutex);

	DEV_DBG("HDMI Core: %s, HDMI_CTRL=0x%08x\n",
		power_on ? "Enable" : "Disable", reg_val);
} /* hdmi_tx_set_mode */

static int hdmi_tx_config_power(struct hdmi_tx_ctrl *hdmi_ctrl,
	enum hdmi_tx_power_module_type module, int config)
{
	int rc = 0;
	struct dss_module_power *power_data = NULL;

	if (!hdmi_ctrl || module >= HDMI_TX_MAX_PM) {
		DEV_ERR("%s: Error: invalid input\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	power_data = &hdmi_ctrl->pdata.power_data[module];
	if (!power_data) {
		DEV_ERR("%s: Error: invalid power data\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if (config) {
		rc = msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 1);
		if (rc) {
			DEV_ERR("%s: Failed to config %s vreg. Err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto exit;
		}

		rc = msm_dss_get_clk(&hdmi_ctrl->pdev->dev,
			power_data->clk_config, power_data->num_clk);
		if (rc) {
			DEV_ERR("%s: Failed to get %s clk. Err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);

			msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 0);
		}
	} else {
		msm_dss_put_clk(power_data->clk_config, power_data->num_clk);

		rc = msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 0);
		if (rc)
			DEV_ERR("%s: Fail to deconfig %s vreg. Err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
	}

exit:
	return rc;
} /* hdmi_tx_config_power */

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
		rc = msm_dss_enable_vreg(power_data->vreg_config,
			power_data->num_vreg, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s vreg. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto error;
		}

		rc = msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s gpio. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto disable_vreg;
		}

		rc = msm_dss_clk_set_rate(power_data->clk_config,
			power_data->num_clk);
		if (rc) {
			DEV_ERR("%s: failed to set clks rate for %s. err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto disable_gpio;
		}

		rc = msm_dss_enable_clk(power_data->clk_config,
			power_data->num_clk, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable clks for %s. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto disable_gpio;
		}
	} else {
		msm_dss_enable_clk(power_data->clk_config,
			power_data->num_clk, 0);
		msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, 0);
		msm_dss_enable_vreg(power_data->vreg_config,
			power_data->num_vreg, 0);
	}

	return rc;

disable_gpio:
	msm_dss_enable_gpio(power_data->gpio_config, power_data->num_gpio, 0);
disable_vreg:
	msm_dss_enable_vreg(power_data->vreg_config, power_data->num_vreg, 0);
error:
	return rc;
} /* hdmi_tx_enable_power */

static void hdmi_tx_core_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CEC_PM, 0);
	hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CORE_PM, 0);
} /* hdmi_tx_core_off */

static int hdmi_tx_core_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CORE_PM, 1);
	if (rc) {
		DEV_ERR("%s: core hdmi_msm_enable_power failed rc = %d\n",
			__func__, rc);
		return rc;
	}
	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CEC_PM, 1);
	if (rc) {
		DEV_ERR("%s: cec hdmi_msm_enable_power failed rc = %d\n",
			__func__, rc);
		goto disable_core_power;
	}

	return rc;
disable_core_power:
	hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CORE_PM, 0);
	return rc;
} /* hdmi_tx_core_on */

static void hdmi_tx_phy_reset(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	unsigned int phy_reset_polarity = 0x0;
	unsigned int pll_reset_polarity = 0x0;
	unsigned int val;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return;
	}

	val = DSS_REG_R_ND(io, HDMI_PHY_CTRL);

	phy_reset_polarity = val >> 3 & 0x1;
	pll_reset_polarity = val >> 1 & 0x1;

	if (phy_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET);
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET));

	if (pll_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET_PLL);
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET_PLL));

	if (phy_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET));
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET);

	if (pll_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET_PLL));
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET_PLL);
} /* hdmi_tx_phy_reset */

static void hdmi_tx_init_phy(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_PHY_IO];
	if (!io->base) {
		DEV_ERR("%s: phy io is not initialized\n", __func__);
		return;
	}

	DSS_REG_W_ND(io, HDMI_PHY_ANA_CFG0, 0x1B);
	DSS_REG_W_ND(io, HDMI_PHY_ANA_CFG1, 0xF2);
	DSS_REG_W_ND(io, HDMI_PHY_BIST_CFG0, 0x0);
	DSS_REG_W_ND(io, HDMI_PHY_BIST_PATN0, 0x0);
	DSS_REG_W_ND(io, HDMI_PHY_BIST_PATN1, 0x0);
	DSS_REG_W_ND(io, HDMI_PHY_BIST_PATN2, 0x0);
	DSS_REG_W_ND(io, HDMI_PHY_BIST_PATN3, 0x0);

	DSS_REG_W_ND(io, HDMI_PHY_PD_CTRL1, 0x20);
} /* hdmi_tx_init_phy */

static void hdmi_tx_powerdown_phy(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_PHY_IO];
	if (!io->base) {
		DEV_ERR("%s: phy io is not initialized\n", __func__);
		return;
	}

	DSS_REG_W_ND(io, HDMI_PHY_PD_CTRL0, 0x7F);
} /* hdmi_tx_powerdown_phy */

static int hdmi_tx_audio_acr_setup(struct hdmi_tx_ctrl *hdmi_ctrl,
	bool enabled)
{
	/* Read first before writing */
	u32 acr_pck_ctrl_reg;
	u32 sample_rate;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	sample_rate = hdmi_ctrl->audio_data.sample_rate;

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return -EINVAL;
	}

	acr_pck_ctrl_reg = DSS_REG_R(io, HDMI_ACR_PKT_CTRL);

	if (enabled) {
		const struct msm_hdmi_mode_timing_info *timing =
			hdmi_get_supported_mode(hdmi_ctrl->video_resolution);
		const struct hdmi_tx_audio_acr_arry *audio_acr =
			&hdmi_tx_audio_acr_lut[0];
		const int lut_size = sizeof(hdmi_tx_audio_acr_lut)
			/ sizeof(*hdmi_tx_audio_acr_lut);
		u32 i, n, cts, layout, multiplier, aud_pck_ctrl_2_reg;

		if (timing == NULL) {
			DEV_WARN("%s: video format %d not supported\n",
				__func__, hdmi_ctrl->video_resolution);
			return -EPERM;
		}

		for (i = 0; i < lut_size;
			audio_acr = &hdmi_tx_audio_acr_lut[++i]) {
			if (audio_acr->pclk == timing->pixel_freq)
				break;
		}
		if (i >= lut_size) {
			DEV_WARN("%s: pixel clk %d not supported\n", __func__,
				timing->pixel_freq);
			return -EPERM;
		}

		n = audio_acr->lut[sample_rate].n;
		cts = audio_acr->lut[sample_rate].cts;
		layout = (MSM_HDMI_AUDIO_CHANNEL_2 ==
			hdmi_ctrl->audio_data.channel_num) ? 0 : 1;

		if (
		(AUDIO_SAMPLE_RATE_192KHZ == sample_rate) ||
		(AUDIO_SAMPLE_RATE_176_4KHZ == sample_rate)) {
			multiplier = 4;
			n >>= 2; /* divide N by 4 and use multiplier */
		} else if (
		(AUDIO_SAMPLE_RATE_96KHZ == sample_rate) ||
		(AUDIO_SAMPLE_RATE_88_2KHZ == sample_rate)) {
			multiplier = 2;
			n >>= 1; /* divide N by 2 and use multiplier */
		} else {
			multiplier = 1;
		}
		DEV_DBG("%s: n=%u, cts=%u, layout=%u\n", __func__, n, cts,
			layout);

		/* AUDIO_PRIORITY | SOURCE */
		acr_pck_ctrl_reg |= 0x80000100;

		/* Reset multiplier bits */
		acr_pck_ctrl_reg &= ~(7 << 16);

		/* N_MULTIPLE(multiplier) */
		acr_pck_ctrl_reg |= (multiplier & 7) << 16;

		if ((AUDIO_SAMPLE_RATE_48KHZ == sample_rate) ||
		(AUDIO_SAMPLE_RATE_96KHZ == sample_rate) ||
		(AUDIO_SAMPLE_RATE_192KHZ == sample_rate)) {
			/* SELECT(3) */
			acr_pck_ctrl_reg |= 3 << 4;
			/* CTS_48 */
			cts <<= 12;

			/* CTS: need to determine how many fractional bits */
			DSS_REG_W(io, HDMI_ACR_48_0, cts);
			/* N */
			DSS_REG_W(io, HDMI_ACR_48_1, n);
		} else if (
		(AUDIO_SAMPLE_RATE_44_1KHZ == sample_rate) ||
		(AUDIO_SAMPLE_RATE_88_2KHZ == sample_rate) ||
		(AUDIO_SAMPLE_RATE_176_4KHZ == sample_rate)) {
			/* SELECT(2) */
			acr_pck_ctrl_reg |= 2 << 4;
			/* CTS_44 */
			cts <<= 12;

			/* CTS: need to determine how many fractional bits */
			DSS_REG_W(io, HDMI_ACR_44_0, cts);
			/* N */
			DSS_REG_W(io, HDMI_ACR_44_1, n);
		} else {	/* default to 32k */
			/* SELECT(1) */
			acr_pck_ctrl_reg |= 1 << 4;
			/* CTS_32 */
			cts <<= 12;

			/* CTS: need to determine how many fractional bits */
			DSS_REG_W(io, HDMI_ACR_32_0, cts);
			/* N */
			DSS_REG_W(io, HDMI_ACR_32_1, n);
		}
		/* Payload layout depends on number of audio channels */
		/* LAYOUT_SEL(layout) */
		aud_pck_ctrl_2_reg = 1 | (layout << 1);
		/* override | layout */
		DSS_REG_W(io, HDMI_AUDIO_PKT_CTRL2, aud_pck_ctrl_2_reg);

		/* SEND | CONT */
		acr_pck_ctrl_reg |= 0x00000003;
	} else {
		/* ~(SEND | CONT) */
		acr_pck_ctrl_reg &= ~0x00000003;
	}
	DSS_REG_W(io, HDMI_ACR_PKT_CTRL, acr_pck_ctrl_reg);

	return 0;
} /* hdmi_tx_audio_acr_setup */

static int hdmi_tx_audio_iframe_setup(struct hdmi_tx_ctrl *hdmi_ctrl,
	bool enabled)
{
	struct dss_io_data *io = NULL;

	u32 channel_count = 1; /* Def to 2 channels -> Table 17 in CEA-D */
	u32 num_of_channels;
	u32 channel_allocation;
	u32 level_shift;
	u32 down_mix;
	u32 check_sum, audio_info_0_reg, audio_info_1_reg;
	u32 audio_info_ctrl_reg;
	u32 aud_pck_ctrl_2_reg;
	u32 layout;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	num_of_channels    = hdmi_ctrl->audio_data.channel_num;
	channel_allocation = hdmi_ctrl->audio_data.spkr_alloc;
	level_shift        = hdmi_ctrl->audio_data.level_shift;
	down_mix           = hdmi_ctrl->audio_data.down_mix;

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return -EINVAL;
	}

	layout = (MSM_HDMI_AUDIO_CHANNEL_2 == num_of_channels) ? 0 : 1;
	aud_pck_ctrl_2_reg = 1 | (layout << 1);
	DSS_REG_W(io, HDMI_AUDIO_PKT_CTRL2, aud_pck_ctrl_2_reg);

	/*
	 * Please see table 20 Audio InfoFrame in HDMI spec
	 * FL  = front left
	 * FC  = front Center
	 * FR  = front right
	 * FLC = front left center
	 * FRC = front right center
	 * RL  = rear left
	 * RC  = rear center
	 * RR  = rear right
	 * RLC = rear left center
	 * RRC = rear right center
	 * LFE = low frequency effect
	 */

	/* Read first then write because it is bundled with other controls */
	audio_info_ctrl_reg = DSS_REG_R(io, HDMI_INFOFRAME_CTRL0);

	if (enabled) {
		switch (num_of_channels) {
		case MSM_HDMI_AUDIO_CHANNEL_2:
			break;
		case MSM_HDMI_AUDIO_CHANNEL_3:
		case MSM_HDMI_AUDIO_CHANNEL_4:
			channel_count = 3;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_5:
		case MSM_HDMI_AUDIO_CHANNEL_6:
			channel_count = 5;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_7:
		case MSM_HDMI_AUDIO_CHANNEL_8:
			channel_count = 7;
			break;
		default:
			DEV_ERR("%s: Unsupported num_of_channels = %u\n",
				__func__, num_of_channels);
			return -EINVAL;
		}

		/* Program the Channel-Speaker allocation */
		audio_info_1_reg = 0;
		/* CA(channel_allocation) */
		audio_info_1_reg |= channel_allocation & 0xff;
		/* Program the Level shifter */
		audio_info_1_reg |= (level_shift << 11) & 0x00007800;
		/* Program the Down-mix Inhibit Flag */
		audio_info_1_reg |= (down_mix << 15) & 0x00008000;

		DSS_REG_W(io, HDMI_AUDIO_INFO1, audio_info_1_reg);

		/*
		 * Calculate CheckSum: Sum of all the bytes in the
		 * Audio Info Packet (See table 8.4 in HDMI spec)
		 */
		check_sum = 0;
		/* HDMI_AUDIO_INFO_FRAME_PACKET_HEADER_TYPE[0x84] */
		check_sum += 0x84;
		/* HDMI_AUDIO_INFO_FRAME_PACKET_HEADER_VERSION[0x01] */
		check_sum += 1;
		/* HDMI_AUDIO_INFO_FRAME_PACKET_LENGTH[0x0A] */
		check_sum += 0x0A;
		check_sum += channel_count;
		check_sum += channel_allocation;
		/* See Table 8.5 in HDMI spec */
		check_sum += (level_shift & 0xF) << 3 | (down_mix & 0x1) << 7;
		check_sum &= 0xFF;
		check_sum = (u8) (256 - check_sum);

		audio_info_0_reg = 0;
		/* CHECKSUM(check_sum) */
		audio_info_0_reg |= check_sum & 0xff;
		/* CC(channel_count) */
		audio_info_0_reg |= (channel_count << 8) & 0x00000700;

		DSS_REG_W(io, HDMI_AUDIO_INFO0, audio_info_0_reg);

		/*
		 * Set these flags
		 * AUDIO_INFO_UPDATE |
		 * AUDIO_INFO_SOURCE |
		 * AUDIO_INFO_CONT   |
		 * AUDIO_INFO_SEND
		 */
		audio_info_ctrl_reg |= 0x000000F0;
	} else {
		/*Clear these flags
		 * ~(AUDIO_INFO_UPDATE |
		 *   AUDIO_INFO_SOURCE |
		 *   AUDIO_INFO_CONT   |
		 *   AUDIO_INFO_SEND)
		 */
		audio_info_ctrl_reg &= ~0x000000F0;
	}
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0, audio_info_ctrl_reg);

	dss_reg_dump(io->base, io->len,
		enabled ? "HDMI-AUDIO-ON: " : "HDMI-AUDIO-OFF: ", REG_DUMP);

	return 0;
} /* hdmi_tx_audio_iframe_setup */

static int hdmi_tx_audio_info_setup(struct platform_device *pdev,
	u32 sample_rate, u32 num_of_channels, u32 channel_allocation,
	u32 level_shift, bool down_mix)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl) && hdmi_ctrl->panel_power_on) {

		/* Map given sample rate to Enum */
		if (sample_rate == 32000)
			sample_rate = AUDIO_SAMPLE_RATE_32KHZ;
		else if (sample_rate == 44100)
			sample_rate = AUDIO_SAMPLE_RATE_44_1KHZ;
		else if (sample_rate == 48000)
			sample_rate = AUDIO_SAMPLE_RATE_48KHZ;
		else if (sample_rate == 88200)
			sample_rate = AUDIO_SAMPLE_RATE_88_2KHZ;
		else if (sample_rate == 96000)
			sample_rate = AUDIO_SAMPLE_RATE_96KHZ;
		else if (sample_rate == 176400)
			sample_rate = AUDIO_SAMPLE_RATE_176_4KHZ;
		else if (sample_rate == 192000)
			sample_rate = AUDIO_SAMPLE_RATE_192KHZ;

		hdmi_ctrl->audio_data.sample_rate = sample_rate;
		hdmi_ctrl->audio_data.channel_num = num_of_channels;
		hdmi_ctrl->audio_data.spkr_alloc  = channel_allocation;
		hdmi_ctrl->audio_data.level_shift = level_shift;
		hdmi_ctrl->audio_data.down_mix    = down_mix;

		rc = hdmi_tx_audio_setup(hdmi_ctrl);
		if (rc)
			DEV_ERR("%s: hdmi_tx_audio_iframe_setup failed.rc=%d\n",
				__func__, rc);
	} else {
		DEV_ERR("%s: Error. panel is not on.\n", __func__);
		rc = -EPERM;
	}

	return rc;
} /* hdmi_tx_audio_info_setup */

static int hdmi_tx_get_audio_edid_blk(struct platform_device *pdev,
	struct msm_hdmi_audio_edid_blk *blk)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	if (!hdmi_ctrl->audio_sdev.state) {
		DEV_ERR("%s: failed. HDMI is not connected/ready for audio\n",
			__func__);
		return -EPERM;
	}

	return hdmi_edid_get_audio_blk(
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID], blk);
} /* hdmi_tx_get_audio_edid_blk */

static u8 hdmi_tx_tmds_enabled(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	/* status of tmds */
	return (hdmi_ctrl->timing_gen_on == true);
}

static int hdmi_tx_set_mhl_max_pclk(struct platform_device *pdev, u32 max_val)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}
	if (max_val) {
		hdmi_ctrl->mhl_max_pclk = max_val;
		hdmi_tx_setup_mhl_video_mode_lut(hdmi_ctrl);
	} else {
		DEV_ERR("%s: invalid max pclk val\n", __func__);
		return -EINVAL;
	}
	return 0;
}

int msm_hdmi_register_mhl(struct platform_device *pdev,
			  struct msm_hdmi_mhl_ops *ops, void *data)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid pdev\n", __func__);
		return -ENODEV;
	}

	if (!ops) {
		DEV_ERR("%s: invalid ops\n", __func__);
		return -EINVAL;
	}

	ops->tmds_enabled = hdmi_tx_tmds_enabled;
	ops->set_mhl_max_pclk = hdmi_tx_set_mhl_max_pclk;
	ops->set_upstream_hpd = hdmi_tx_set_mhl_hpd;

	return 0;
}

static int hdmi_tx_get_cable_status(struct platform_device *pdev, u32 vote)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);
	unsigned long flags;
	u32 hpd;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
	hpd = hdmi_ctrl->hpd_state;
	spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

	hdmi_ctrl->vote_hdmi_core_on = false;

	if (vote && hpd)
		hdmi_ctrl->vote_hdmi_core_on = true;

	return hpd;
}

int msm_hdmi_register_audio_codec(struct platform_device *pdev,
	struct msm_hdmi_audio_codec_ops *ops)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl || !ops) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	ops->audio_info_setup = hdmi_tx_audio_info_setup;
	ops->get_audio_edid_blk = hdmi_tx_get_audio_edid_blk;
	ops->hdmi_cable_status = hdmi_tx_get_cable_status;

	return 0;
} /* hdmi_tx_audio_register */
EXPORT_SYMBOL(msm_hdmi_register_audio_codec);

static int hdmi_tx_audio_setup(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return -EINVAL;
	}

	rc = hdmi_tx_audio_acr_setup(hdmi_ctrl, true);
	if (rc) {
		DEV_ERR("%s: hdmi_tx_audio_acr_setup failed. rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = hdmi_tx_audio_iframe_setup(hdmi_ctrl, true);
	if (rc) {
		DEV_ERR("%s: hdmi_tx_audio_iframe_setup failed. rc=%d\n",
			__func__, rc);
		return rc;
	}

	DEV_INFO("HDMI Audio: Enabled\n");

	return 0;
} /* hdmi_tx_audio_setup */

static void hdmi_tx_audio_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return;
	}

	if (hdmi_tx_audio_iframe_setup(hdmi_ctrl, false))
		DEV_ERR("%s: hdmi_tx_audio_iframe_setup failed.\n", __func__);

	if (hdmi_tx_audio_acr_setup(hdmi_ctrl, false))
		DEV_ERR("%s: hdmi_tx_audio_acr_setup failed.\n", __func__);

	hdmi_ctrl->audio_data.sample_rate = AUDIO_SAMPLE_RATE_48KHZ;
	hdmi_ctrl->audio_data.channel_num = MSM_HDMI_AUDIO_CHANNEL_2;

	DEV_INFO("HDMI Audio: Disabled\n");
} /* hdmi_tx_audio_off */

static int hdmi_tx_start(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io is not initialized\n", __func__);
		return -EINVAL;
	}

	hdmi_tx_set_mode(hdmi_ctrl, false);
	hdmi_tx_init_phy(hdmi_ctrl);
	DSS_REG_W(io, HDMI_USEC_REFTIMER, 0x0001001B);

	hdmi_tx_set_mode(hdmi_ctrl, true);

	rc = hdmi_tx_video_setup(hdmi_ctrl, hdmi_ctrl->video_resolution);
	if (rc) {
		DEV_ERR("%s: hdmi_tx_video_setup failed. rc=%d\n",
			__func__, rc);
		hdmi_tx_set_mode(hdmi_ctrl, false);
		return rc;
	}

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl) &&
	    hdmi_tx_is_cea_format(hdmi_ctrl->video_resolution)) {
		rc = hdmi_tx_audio_setup(hdmi_ctrl);
		if (rc) {
			DEV_ERR("%s: hdmi_msm_audio_setup failed. rc=%d\n",
				__func__, rc);
			hdmi_tx_set_mode(hdmi_ctrl, false);
			return rc;
		}

		if (!hdmi_ctrl->hdcp_feature_on || !hdmi_ctrl->present_hdcp)
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 1, false);

		hdmi_tx_set_avi_infoframe(hdmi_ctrl);
		hdmi_tx_set_vendor_specific_infoframe(hdmi_ctrl);
		hdmi_tx_set_spd_infoframe(hdmi_ctrl);
	}

	/* todo: CEC */

	DEV_INFO("%s: HDMI Core: Initialized\n", __func__);

	return rc;
} /* hdmi_tx_start */

static void hdmi_tx_hpd_polarity_setup(struct hdmi_tx_ctrl *hdmi_ctrl,
	bool polarity)
{
	struct dss_io_data *io = NULL;
	u32 cable_sense;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io is not initialized\n", __func__);
		return;
	}

	if (polarity)
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(2) | BIT(1));
	else
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(2));

	cable_sense = (DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(1)) >> 1;
	DEV_DBG("%s: listen = %s, sense = %s\n", __func__,
		polarity ? "connect" : "disconnect",
		cable_sense ? "connect" : "disconnect");

	if (cable_sense == polarity) {
		u32 reg_val = DSS_REG_R(io, HDMI_HPD_CTRL);

		/* Toggle HPD circuit to trigger HPD sense */
		DSS_REG_W(io, HDMI_HPD_CTRL, reg_val & ~BIT(28));
		DSS_REG_W(io, HDMI_HPD_CTRL, reg_val | BIT(28));
	}
} /* hdmi_tx_hpd_polarity_setup */

static void hdmi_tx_power_off_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct dss_io_data *io = NULL;

	hdmi_ctrl = container_of(work, struct hdmi_tx_ctrl, power_off_work);
	if (!hdmi_ctrl) {
		DEV_DBG("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	if (hdmi_ctrl->hdcp_feature_on && hdmi_ctrl->present_hdcp) {
		DEV_DBG("%s: Turning off HDCP\n", __func__);
		hdmi_hdcp_off(hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP]);
	}

	if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, false))
		DEV_WARN("%s: Failed to disable ddc power\n", __func__);

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl))
		hdmi_tx_audio_off(hdmi_ctrl);

	hdmi_tx_powerdown_phy(hdmi_ctrl);

	hdmi_cec_deconfig(hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]);

	hdmi_tx_core_off(hdmi_ctrl);

	if (hdmi_ctrl->hpd_off_pending) {
		hdmi_tx_hpd_off(hdmi_ctrl);
		hdmi_ctrl->hpd_off_pending = false;
	}

	mutex_lock(&hdmi_ctrl->mutex);
	hdmi_ctrl->panel_power_on = false;
	mutex_unlock(&hdmi_ctrl->mutex);

	DEV_INFO("%s: HDMI Core: OFF\n", __func__);

	if (hdmi_ctrl->hdmi_tx_hpd_done)
		hdmi_ctrl->hdmi_tx_hpd_done(
			hdmi_ctrl->downstream_data);
} /* hdmi_tx_power_off_work */

static int hdmi_tx_power_off(struct mdss_panel_data *panel_data)
{
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_panel_data(panel_data);

	if (!hdmi_ctrl || !hdmi_ctrl->panel_power_on) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/*
	 * Queue work item to handle power down sequence.
	 * This is needed since we need to wait for the audio engine
	 * to shutdown first before we shutdown the HDMI core.
	 */
	DEV_DBG("%s: Queuing work to power off HDMI core\n", __func__);
	queue_work(hdmi_ctrl->workq, &hdmi_ctrl->power_off_work);

	return 0;
} /* hdmi_tx_power_off */

static int hdmi_tx_power_on(struct mdss_panel_data *panel_data)
{
	u32 timeout;
	int rc = 0;
	struct dss_io_data *io = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_panel_data(panel_data);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io is not initialized\n", __func__);
		return -EINVAL;
	}

	if (!hdmi_ctrl->hpd_initialized) {
		DEV_ERR("%s: HDMI on is not possible w/o cable detection.\n",
			__func__);
		return -EPERM;
	}

	/* If a power down is already underway, wait for it to finish */
	flush_work_sync(&hdmi_ctrl->power_off_work);

	if (hdmi_ctrl->pdata.primary) {
		timeout = wait_for_completion_timeout(
			&hdmi_ctrl->hpd_done, HZ);
		if (!timeout) {
			DEV_ERR("%s: cable connection hasn't happened yet\n",
				__func__);
			return -ETIMEDOUT;
		}
	}

	rc = hdmi_tx_set_video_fmt(hdmi_ctrl, &panel_data->panel_info);
	if (rc) {
		DEV_ERR("%s: cannot set video_fmt.rc=%d\n", __func__, rc);
		return rc;
	}

	hdmi_ctrl->hdcp_feature_on = hdcp_feature_on;

	DEV_INFO("power: ON (%s)\n", msm_hdmi_mode_2string(
		hdmi_ctrl->video_resolution));

	rc = hdmi_tx_core_on(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: hdmi_msm_core_on failed\n", __func__);
		return rc;
	}

	mutex_lock(&hdmi_ctrl->mutex);
	hdmi_ctrl->panel_power_on = true;
	mutex_unlock(&hdmi_ctrl->mutex);

	if (hdmi_ctrl->pdata.primary) {
		if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, true))
			DEV_ERR("%s: Failed to enable ddc power\n", __func__);
	}

	hdmi_cec_config(hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]);

	if (hdmi_ctrl->hpd_state) {
		DEV_DBG("%s: Turning HDMI on\n", __func__);
		rc = hdmi_tx_start(hdmi_ctrl);
		if (rc) {
			DEV_ERR("%s: hdmi_tx_start failed. rc=%d\n",
				__func__, rc);
			hdmi_tx_power_off(panel_data);
			return rc;
		}
	}

	dss_reg_dump(io->base, io->len, "HDMI-ON: ", REG_DUMP);

	DEV_INFO("%s: HDMI=%s DVI= %s\n", __func__,
		hdmi_tx_is_controller_on(hdmi_ctrl) ? "ON" : "OFF" ,
		hdmi_tx_is_dvi_mode(hdmi_ctrl) ? "ON" : "OFF");

	hdmi_tx_hpd_polarity_setup(hdmi_ctrl, HPD_DISCONNECT_POLARITY);

	if (hdmi_ctrl->hdmi_tx_hpd_done)
		hdmi_ctrl->hdmi_tx_hpd_done(hdmi_ctrl->downstream_data);

	return 0;
} /* hdmi_tx_power_on */

static void hdmi_tx_hpd_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;
	struct dss_io_data *io = NULL;
	unsigned long flags;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (!hdmi_ctrl->hpd_initialized) {
		DEV_DBG("%s: HPD is already OFF, returning\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return;
	}

	/* finish the ongoing hpd work if any */
	flush_work_sync(&hdmi_ctrl->hpd_int_work);

	/* Turn off HPD interrupts */
	DSS_REG_W(io, HDMI_HPD_INT_CTRL, 0);

	mdss_disable_irq(&hdmi_tx_hw);

	hdmi_tx_set_mode(hdmi_ctrl, false);

	if (hdmi_ctrl->hpd_state) {
		rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, 0);
		if (rc)
			DEV_INFO("%s: Failed to disable ddc power. Error=%d\n",
				__func__, rc);
	}

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, 0);
	if (rc)
		DEV_INFO("%s: Failed to disable hpd power. Error=%d\n",
			__func__, rc);

	spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
	hdmi_ctrl->hpd_state = false;
	spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

	hdmi_ctrl->hpd_initialized = false;
} /* hdmi_tx_hpd_off */

static int hdmi_tx_hpd_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 reg_val;
	int rc = 0;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
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

		dss_reg_dump(io->base, io->len, "HDMI-INIT: ", REG_DUMP);

		hdmi_tx_set_mode(hdmi_ctrl, false);
		hdmi_tx_phy_reset(hdmi_ctrl);
		hdmi_tx_set_mode(hdmi_ctrl, true);

		DSS_REG_W(io, HDMI_USEC_REFTIMER, 0x0001001B);

		mdss_enable_irq(&hdmi_tx_hw);

		hdmi_ctrl->hpd_initialized = true;

		DEV_INFO("%s: HDMI HW version = 0x%x\n", __func__,
			DSS_REG_R_ND(&hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO],
				HDMI_VERSION));

		/* set timeout to 4.1ms (max) for hardware debounce */
		reg_val = DSS_REG_R(io, HDMI_HPD_CTRL) | 0x1FFF;

		/* Turn on HPD HW circuit */
		DSS_REG_W(io, HDMI_HPD_CTRL, reg_val | BIT(28));

		hdmi_tx_hpd_polarity_setup(hdmi_ctrl, HPD_CONNECT_POLARITY);
	}

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
		/* If power down is already underway, wait for it to finish */
		flush_work_sync(&hdmi_ctrl->power_off_work);

		if (!hdmi_ctrl->panel_power_on)
			hdmi_tx_hpd_off(hdmi_ctrl);
		else
			hdmi_ctrl->hpd_off_pending = true;
	}

	return rc;
} /* hdmi_tx_sysfs_enable_hpd */

static int hdmi_tx_set_mhl_hpd(struct platform_device *pdev, uint8_t on)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* mhl status should override */
	hdmi_ctrl->mhl_hpd_on = on;

	if (!on && hdmi_ctrl->hpd_feature_on) {
		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, false);
	} else if (on && !hdmi_ctrl->hpd_feature_on) {
		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
	} else {
		DEV_DBG("%s: hpd is already '%s'. return\n", __func__,
			hdmi_ctrl->hpd_feature_on ? "enabled" : "disabled");
		return rc;
	}

	if (!rc) {
		hdmi_ctrl->hpd_feature_on =
			(~hdmi_ctrl->hpd_feature_on) & BIT(0);
		DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_feature_on);
	} else {
		DEV_ERR("%s: failed to '%s' hpd. rc = %d\n", __func__,
			on ? "enable" : "disable", rc);
	}

	return rc;

}

static irqreturn_t hdmi_tx_isr(int irq, void *data)
{
	struct dss_io_data *io = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl = (struct hdmi_tx_ctrl *)data;
	unsigned long flags;

	if (!hdmi_ctrl) {
		DEV_WARN("%s: invalid input data, ISR ignored\n", __func__);
		return IRQ_HANDLED;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_WARN("%s: core io not initialized, ISR ignored\n",
			__func__);
		return IRQ_HANDLED;
	}

	if (DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(0)) {
		spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
		hdmi_ctrl->hpd_state =
			(DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(1)) >> 1;
		spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

		/*
		 * Ack the current hpd interrupt and stop listening to
		 * new hpd interrupt.
		 */
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(0));
		queue_work(hdmi_ctrl->workq, &hdmi_ctrl->hpd_int_work);
	}

	if (hdmi_ddc_isr(&hdmi_ctrl->ddc_ctrl))
		DEV_ERR("%s: hdmi_ddc_isr failed\n", __func__);

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC])
		if (hdmi_cec_isr(hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]))
			DEV_ERR("%s: hdmi_cec_isr failed\n", __func__);

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP])
		if (hdmi_hdcp_isr(hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP]))
			DEV_ERR("%s: hdmi_hdcp_isr failed\n", __func__);

	return IRQ_HANDLED;
} /* hdmi_tx_isr */

static void hdmi_tx_dev_deinit(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]) {
		hdmi_cec_deinit(hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]);
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC] = NULL;
	}

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP]) {
		hdmi_hdcp_deinit(hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP]);
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP] = NULL;
	}

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]) {
		hdmi_edid_deinit(hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]);
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID] = NULL;
	}

	switch_dev_unregister(&hdmi_ctrl->audio_sdev);
	switch_dev_unregister(&hdmi_ctrl->sdev);
	if (hdmi_ctrl->workq)
		destroy_workqueue(hdmi_ctrl->workq);
	mutex_destroy(&hdmi_ctrl->lut_lock);
	mutex_destroy(&hdmi_ctrl->cable_notify_mutex);
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

	rc = hdmi_tx_check_capability(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: no HDMI device\n", __func__);
		goto fail_no_hdmi;
	}

	/* irq enable/disable will be handled in hpd on/off */
	hdmi_tx_hw.ptr = (void *)hdmi_ctrl;

	hdmi_setup_video_mode_lut();
	mutex_init(&hdmi_ctrl->mutex);
	mutex_init(&hdmi_ctrl->lut_lock);
	mutex_init(&hdmi_ctrl->cable_notify_mutex);

	INIT_LIST_HEAD(&hdmi_ctrl->cable_notify_handlers);

	hdmi_ctrl->workq = create_workqueue("hdmi_tx_workq");
	if (!hdmi_ctrl->workq) {
		DEV_ERR("%s: hdmi_tx_workq creation failed.\n", __func__);
		rc = -EPERM;
		goto fail_create_workq;
	}

	hdmi_ctrl->ddc_ctrl.io = &pdata->io[HDMI_TX_CORE_IO];
	init_completion(&hdmi_ctrl->ddc_ctrl.ddc_sw_done);

	hdmi_ctrl->panel_power_on = false;
	hdmi_ctrl->panel_suspend = false;

	hdmi_ctrl->hpd_state = false;
	hdmi_ctrl->hpd_initialized = false;
	hdmi_ctrl->hpd_off_pending = false;
	init_completion(&hdmi_ctrl->hpd_done);

	INIT_WORK(&hdmi_ctrl->hpd_int_work, hdmi_tx_hpd_int_work);
	INIT_WORK(&hdmi_ctrl->cable_notify_work, hdmi_tx_cable_notify_work);
	INIT_WORK(&hdmi_ctrl->power_off_work, hdmi_tx_power_off_work);

	spin_lock_init(&hdmi_ctrl->hpd_state_lock);

	hdmi_ctrl->audio_data.sample_rate = AUDIO_SAMPLE_RATE_48KHZ;
	hdmi_ctrl->audio_data.channel_num = MSM_HDMI_AUDIO_CHANNEL_2;

	hdmi_ctrl->sdev.name = "hdmi";
	if (switch_dev_register(&hdmi_ctrl->sdev) < 0) {
		DEV_ERR("%s: Hdmi switch registration failed\n", __func__);
		rc = -ENODEV;
		goto fail_create_workq;
	}

	hdmi_ctrl->audio_sdev.name = "hdmi_audio";
	if (switch_dev_register(&hdmi_ctrl->audio_sdev) < 0) {
		DEV_ERR("%s: hdmi_audio switch registration failed\n",
			__func__);
		rc = -ENODEV;
		goto fail_audio_switch_dev;
	}

	return 0;

fail_audio_switch_dev:
	switch_dev_unregister(&hdmi_ctrl->sdev);
fail_create_workq:
	if (hdmi_ctrl->workq)
		destroy_workqueue(hdmi_ctrl->workq);
	mutex_destroy(&hdmi_ctrl->lut_lock);
	mutex_destroy(&hdmi_ctrl->mutex);
fail_no_hdmi:
	return rc;
} /* hdmi_tx_dev_init */

static int hdmi_tx_panel_event_handler(struct mdss_panel_data *panel_data,
	int event, void *arg)
{
	int rc = 0, new_vic = -1;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_panel_data(panel_data);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	DEV_DBG("%s: event = %d suspend=%d, hpd_feature=%d\n", __func__,
		event, hdmi_ctrl->panel_suspend, hdmi_ctrl->hpd_feature_on);

	switch (event) {
	case MDSS_EVENT_FB_REGISTERED:
		rc = hdmi_tx_sysfs_create(hdmi_ctrl, arg);
		if (rc) {
			DEV_ERR("%s: hdmi_tx_sysfs_create failed.rc=%d\n",
					__func__, rc);
			return rc;
		}
		rc = hdmi_tx_init_features(hdmi_ctrl);
		if (rc) {
			DEV_ERR("%s: init_features failed.rc=%d\n",
					__func__, rc);
			hdmi_tx_sysfs_remove(hdmi_ctrl);
			return rc;
		}

		if (hdmi_ctrl->pdata.primary) {
			INIT_COMPLETION(hdmi_ctrl->hpd_done);
			rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
			if (rc) {
				DEV_ERR("%s: hpd_enable failed. rc=%d\n",
					__func__, rc);
				hdmi_tx_sysfs_remove(hdmi_ctrl);
				return rc;
			} else {
				hdmi_ctrl->hpd_feature_on = true;
			}
		}

		break;

	case MDSS_EVENT_CHECK_PARAMS:
		new_vic = hdmi_tx_get_vic_from_panel_info(hdmi_ctrl,
			(struct mdss_panel_info *)arg);
		if ((new_vic < 0) || (new_vic > HDMI_VFRMT_MAX)) {
			DEV_ERR("%s: invalid or not supported vic\n", __func__);
			return -EPERM;
		}

		/*
		 * return value of 1 lets mdss know that panel
		 * needs a reconfig due to new resolution and
		 * it will issue close and open subsequently.
		 */
		if (new_vic != hdmi_ctrl->video_resolution)
			rc = 1;
		else
			DEV_DBG("%s: no res change.\n", __func__);
		break;

	case MDSS_EVENT_RESUME:
		/* If a suspend is already underway, wait for it to finish */
		if (hdmi_ctrl->panel_suspend && hdmi_ctrl->panel_power_on)
			flush_work(&hdmi_ctrl->power_off_work);

		if (hdmi_ctrl->hpd_feature_on) {
			INIT_COMPLETION(hdmi_ctrl->hpd_done);

			rc = hdmi_tx_hpd_on(hdmi_ctrl);
			if (rc)
				DEV_ERR("%s: hdmi_tx_hpd_on failed. rc=%d\n",
					__func__, rc);
		}
		break;

	case MDSS_EVENT_RESET:
		if (hdmi_ctrl->panel_suspend) {
			u32 timeout;
			hdmi_ctrl->panel_suspend = false;

			timeout = wait_for_completion_timeout(
				&hdmi_ctrl->hpd_done, HZ/10);
			if (!timeout & !hdmi_ctrl->hpd_state) {
				DEV_INFO("%s: cable removed during suspend\n",
					__func__);
				hdmi_tx_send_cable_notification(hdmi_ctrl, 0);
				rc = -EPERM;
			} else {
				DEV_DBG("%s: cable present after resume\n",
					__func__);
			}
		}
		break;

	case MDSS_EVENT_UNBLANK:
		rc = hdmi_tx_power_on(panel_data);
		if (rc)
			DEV_ERR("%s: hdmi_tx_power_on failed. rc=%d\n",
				__func__, rc);
		break;

	case MDSS_EVENT_PANEL_ON:
		if (hdmi_ctrl->hdcp_feature_on && hdmi_ctrl->present_hdcp) {
			/* Set AV Mute before starting authentication */
			if (hdmi_ctrl->pdata.primary)
				hdmi_tx_en_encryption(hdmi_ctrl, false);
			else
				rc = hdmi_tx_config_avmute(hdmi_ctrl, 1);

			DEV_DBG("%s: Starting HDCP authentication\n", __func__);
			rc = hdmi_hdcp_authenticate(
				hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP]);
			if (rc)
				DEV_ERR("%s: hdcp auth failed. rc=%d\n",
					__func__, rc);
		}
		hdmi_ctrl->timing_gen_on = true;
		break;

	case MDSS_EVENT_SUSPEND:
		if (!hdmi_ctrl->panel_power_on) {
			if (hdmi_ctrl->hpd_feature_on)
				hdmi_tx_hpd_off(hdmi_ctrl);

			hdmi_ctrl->panel_suspend = false;
		} else {
			hdmi_ctrl->hpd_off_pending = true;
			hdmi_ctrl->panel_suspend = true;
		}
		break;

	case MDSS_EVENT_BLANK:
		if (hdmi_ctrl->panel_power_on) {
			rc = hdmi_tx_power_off(panel_data);
			if (rc)
				DEV_ERR("%s: hdmi_tx_power_off failed.rc=%d\n",
					__func__, rc);

		} else {
			DEV_DBG("%s: hdmi is already powered off\n", __func__);
		}
		break;

	case MDSS_EVENT_PANEL_OFF:
		hdmi_ctrl->timing_gen_on = false;
		break;

	case MDSS_EVENT_CLOSE:
		if (hdmi_ctrl->hpd_feature_on)
			hdmi_tx_hpd_polarity_setup(hdmi_ctrl,
				HPD_CONNECT_POLARITY);
		break;
	}

	return rc;
} /* hdmi_tx_panel_event_handler */

static int hdmi_tx_register_panel(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	hdmi_ctrl->panel_data.event_handler = hdmi_tx_panel_event_handler;

	hdmi_ctrl->video_resolution = DEFAULT_VIDEO_RESOLUTION;
	rc = hdmi_tx_init_panel_info(hdmi_ctrl->video_resolution,
		&hdmi_ctrl->panel_data.panel_info);
	if (rc) {
		DEV_ERR("%s: hdmi_init_panel_info failed\n", __func__);
		return rc;
	}

	rc = mdss_register_panel(hdmi_ctrl->pdev, &hdmi_ctrl->panel_data);
	if (rc) {
		DEV_ERR("%s: FAILED: to register HDMI panel\n", __func__);
		return rc;
	}

	rc = mdss_register_irq(&hdmi_tx_hw);
	if (rc)
		DEV_ERR("%s: mdss_register_irq failed.\n", __func__);

	return rc;
} /* hdmi_tx_register_panel */

static void hdmi_tx_deinit_resource(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int i;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	/* VREG & CLK */
	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--) {
		if (hdmi_tx_config_power(hdmi_ctrl, i, 0))
			DEV_ERR("%s: '%s' power deconfig fail\n",
				__func__, hdmi_tx_pm_name(i));
	}

	/* IO */
	for (i = HDMI_TX_MAX_IO - 1; i >= 0; i--)
		msm_dss_iounmap(&hdmi_ctrl->pdata.io[i]);
} /* hdmi_tx_deinit_resource */

static int hdmi_tx_init_resource(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int i, rc = 0;
	struct hdmi_tx_platform_data *pdata = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pdata = &hdmi_ctrl->pdata;

	/* IO */
	for (i = 0; i < HDMI_TX_MAX_IO; i++) {
		rc = msm_dss_ioremap_byname(hdmi_ctrl->pdev, &pdata->io[i],
			hdmi_tx_io_name(i));
		if (rc) {
			DEV_ERR("%s: '%s' remap failed\n", __func__,
				hdmi_tx_io_name(i));
			goto error;
		}
		DEV_INFO("%s: '%s': start = 0x%x, len=0x%x\n", __func__,
			hdmi_tx_io_name(i), (u32)pdata->io[i].base,
			pdata->io[i].len);
	}

	/* VREG & CLK */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_config_power(hdmi_ctrl, i, 1);
		if (rc) {
			DEV_ERR("%s: '%s' power config failed.rc=%d\n",
				__func__, hdmi_tx_pm_name(i), rc);
			goto error;
		}
	}

	return rc;

error:
	hdmi_tx_deinit_resource(hdmi_ctrl);
	return rc;
} /* hdmi_tx_init_resource */

static void hdmi_tx_put_dt_clk_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->clk_config) {
		devm_kfree(dev, module_power->clk_config);
		module_power->clk_config = NULL;
	}
	module_power->num_clk = 0;
} /* hdmi_tx_put_dt_clk_data */

/* todo: once clk are moved to device tree then change this implementation */
static int hdmi_tx_get_dt_clk_data(struct device *dev,
	struct dss_module_power *mp, u32 module_type)
{
	int rc = 0;

	if (!dev || !mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_tx_pm_name(module_type));

	switch (module_type) {
	case HDMI_TX_HPD_PM:
		mp->num_clk = 3;
		mp->clk_config = devm_kzalloc(dev, sizeof(struct dss_clk) *
			mp->num_clk, GFP_KERNEL);
		if (!mp->clk_config) {
			DEV_ERR("%s: can't alloc '%s' clk mem\n", __func__,
				hdmi_tx_pm_name(module_type));
			goto error;
		}

		snprintf(mp->clk_config[0].clk_name, 32, "%s", "iface_clk");
		mp->clk_config[0].type = DSS_CLK_AHB;
		mp->clk_config[0].rate = 0;

		snprintf(mp->clk_config[1].clk_name, 32, "%s", "core_clk");
		mp->clk_config[1].type = DSS_CLK_OTHER;
		mp->clk_config[1].rate = 19200000;

		/*
		 * This clock is required to clock MDSS interrupt registers
		 * when HDMI is the only block turned on within MDSS. Since
		 * rate for this clock is controlled by MDP driver, treat this
		 * similar to AHB clock and do not set rate for it.
		 */
		snprintf(mp->clk_config[2].clk_name, 32, "%s", "mdp_core_clk");
		mp->clk_config[2].type = DSS_CLK_AHB;
		mp->clk_config[2].rate = 0;
		break;

	case HDMI_TX_CORE_PM:
		mp->num_clk = 2;
		mp->clk_config = devm_kzalloc(dev, sizeof(struct dss_clk) *
			mp->num_clk, GFP_KERNEL);
		if (!mp->clk_config) {
			DEV_ERR("%s: can't alloc '%s' clk mem\n", __func__,
				hdmi_tx_pm_name(module_type));
			goto error;
		}

		snprintf(mp->clk_config[0].clk_name, 32, "%s", "extp_clk");
		mp->clk_config[0].type = DSS_CLK_PCLK;
		/* This rate will be overwritten when core is powered on */
		mp->clk_config[0].rate = 148500000;

		snprintf(mp->clk_config[1].clk_name, 32, "%s", "alt_iface_clk");
		mp->clk_config[1].type = DSS_CLK_AHB;
		mp->clk_config[1].rate = 0;
		break;

	case HDMI_TX_DDC_PM:
	case HDMI_TX_CEC_PM:
		mp->num_clk = 0;
		DEV_DBG("%s: no clk\n", __func__);
		break;

	default:
		DEV_ERR("%s: invalid module type=%d\n", __func__,
			module_type);
		return -EINVAL;
	}

	return rc;

error:
	if (mp->clk_config) {
		devm_kfree(dev, mp->clk_config);
		mp->clk_config = NULL;
	}
	mp->num_clk = 0;

	return rc;
} /* hdmi_tx_get_dt_clk_data */

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
	case HDMI_TX_DDC_PM:
		mod_name = "ddc";
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

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_tx_pm_name(module_type));

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
				hdmi_tx_pm_name(module_type));
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
				__func__, hdmi_tx_pm_name(module_type), rc);
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
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].max_voltage = val_array[i];

		/* vreg-op-mode */
		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 32, "%s-%s", COMPATIBLE_NAME,
			"peak-current");
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			prop_name, val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' peak current. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].enable_load = val_array[i];

		DEV_DBG("%s: %s min=%d, max=%d, pc=%d\n", __func__,
			mp->vreg_config[j].vreg_name,
			mp->vreg_config[j].min_voltage,
			mp->vreg_config[j].max_voltage,
			mp->vreg_config[j].enable_load);

		ndx_mask >>= 1;
		j++;
	}

	devm_kfree(dev, val_array);

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
	mp->num_vreg = 0;

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
	int i, j;
	int mp_gpio_cnt = 0, gpio_list_size = 0;
	struct dss_gpio *gpio_list = NULL;
	struct device_node *of_node = NULL;

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_tx_pm_name(module_type));

	if (!dev || !mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	of_node = dev->of_node;

	switch (module_type) {
	case HDMI_TX_HPD_PM:
		gpio_list_size = ARRAY_SIZE(hpd_gpio_config);
		gpio_list = hpd_gpio_config;
		break;
	case HDMI_TX_DDC_PM:
		gpio_list_size = ARRAY_SIZE(ddc_gpio_config);
		gpio_list = ddc_gpio_config;
		break;
	case HDMI_TX_CORE_PM:
		gpio_list_size = ARRAY_SIZE(core_gpio_config);
		gpio_list = core_gpio_config;
		break;
	case HDMI_TX_CEC_PM:
		gpio_list_size = ARRAY_SIZE(cec_gpio_config);
		gpio_list = cec_gpio_config;
		break;
	default:
		DEV_ERR("%s: invalid module type=%d\n", __func__,
			module_type);
		return -EINVAL;
	}

	for (i = 0; i < gpio_list_size; i++)
		if (of_find_property(of_node, gpio_list[i].gpio_name, NULL))
			mp_gpio_cnt++;

	if (!mp_gpio_cnt) {
		DEV_DBG("%s: no gpio\n", __func__);
		return 0;
	}

	DEV_DBG("%s: mp_gpio_cnt = %d\n", __func__, mp_gpio_cnt);
	mp->num_gpio = mp_gpio_cnt;

	mp->gpio_config = devm_kzalloc(dev, sizeof(struct dss_gpio) *
		mp_gpio_cnt, GFP_KERNEL);
	if (!mp->gpio_config) {
		DEV_ERR("%s: can't alloc '%s' gpio mem\n", __func__,
			hdmi_tx_pm_name(module_type));

		mp->num_gpio = 0;
		return -ENOMEM;
	}

	for (i = 0, j = 0; i < gpio_list_size; i++) {
		int gpio = of_get_named_gpio(of_node,
			gpio_list[i].gpio_name, 0);
		if (gpio < 0) {
			DEV_DBG("%s: no gpio named %s\n", __func__,
				gpio_list[i].gpio_name);
			continue;
		}
		memcpy(&mp->gpio_config[j], &gpio_list[i],
			sizeof(struct dss_gpio));

		mp->gpio_config[j].gpio = (unsigned)gpio;

		DEV_DBG("%s: gpio num=%d, name=%s, value=%d\n",
			__func__, mp->gpio_config[j].gpio,
			mp->gpio_config[j].gpio_name,
			mp->gpio_config[j].value);
		j++;
	}

	return 0;
} /* hdmi_tx_get_dt_gpio_data */

static void hdmi_tx_put_dt_data(struct device *dev,
	struct hdmi_tx_platform_data *pdata)
{
	int i;
	if (!dev || !pdata) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_clk_data(dev, &pdata->power_data[i]);

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_vreg_data(dev, &pdata->power_data[i]);

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_gpio_data(dev, &pdata->power_data[i]);
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

	/* GPIO */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_get_dt_gpio_data(&pdev->dev,
			&pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_gpio_data failed.rc=%d\n",
				__func__, hdmi_tx_pm_name(i), rc);
			goto error;
		}
	}

	/* VREG */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_get_dt_vreg_data(&pdev->dev,
			&pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
				__func__, hdmi_tx_pm_name(i), rc);
			goto error;
		}
	}

	/* CLK */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_get_dt_clk_data(&pdev->dev,
			&pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_clk_data failed.rc=%d\n",
				__func__, hdmi_tx_pm_name(i), rc);
			goto error;
		}
	}

	if (of_find_property(pdev->dev.of_node, "qcom,primary_panel", NULL)) {
		u32 tmp;
		of_property_read_u32(pdev->dev.of_node, "qcom,primary_panel",
			&tmp);
		pdata->primary = tmp ? true : false;
	}

	return rc;

error:
	hdmi_tx_put_dt_data(&pdev->dev, pdata);
	return rc;
} /* hdmi_tx_get_dt_data */

static int __devinit hdmi_tx_probe(struct platform_device *pdev)
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

	rc = hdmi_tx_init_resource(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: resource init. rc=%d\n",
			__func__, rc);
		goto failed_res_init;
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

	rc = of_platform_populate(of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		DEV_ERR("%s: Failed to add child devices. rc=%d\n",
			__func__, rc);
		goto failed_reg_panel;
	} else {
		DEV_DBG("%s: Add child devices.\n", __func__);
	}

	if (mdss_debug_register_base("hdmi",
			hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO].base,
			hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO].len))
		DEV_WARN("%s: hdmi_tx debugfs register failed\n", __func__);

	return rc;

failed_reg_panel:
	hdmi_tx_dev_deinit(hdmi_ctrl);
failed_dev_init:
	hdmi_tx_deinit_resource(hdmi_ctrl);
failed_res_init:
	hdmi_tx_put_dt_data(&pdev->dev, &hdmi_ctrl->pdata);
failed_dt_data:
	devm_kfree(&pdev->dev, hdmi_ctrl);
failed_no_mem:
	return rc;
} /* hdmi_tx_probe */

static int __devexit hdmi_tx_remove(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);
	if (!hdmi_ctrl) {
		DEV_ERR("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	hdmi_tx_sysfs_remove(hdmi_ctrl);
	hdmi_tx_dev_deinit(hdmi_ctrl);
	hdmi_tx_deinit_resource(hdmi_ctrl);
	hdmi_tx_put_dt_data(&pdev->dev, &hdmi_ctrl->pdata);
	devm_kfree(&hdmi_ctrl->pdev->dev, hdmi_ctrl);

	return 0;
} /* hdmi_tx_remove */

static const struct of_device_id hdmi_tx_dt_match[] = {
	{.compatible = COMPATIBLE_NAME,},
	{ /* Sentinel */ },
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

static int set_hdcp_feature_on(const char *val, const struct kernel_param *kp)
{
	int rc = 0;

	rc = param_set_bool(val, kp);
	if (!rc)
		pr_debug("%s: HDCP feature = %d\n", __func__, hdcp_feature_on);

	return rc;
}

static struct kernel_param_ops hdcp_feature_on_param_ops = {
	.set = set_hdcp_feature_on,
	.get = param_get_bool,
};

module_param_cb(hdcp, &hdcp_feature_on_param_ops, &hdcp_feature_on,
	S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hdcp, "Enable or Disable HDCP");

module_init(hdmi_tx_drv_init);
module_exit(hdmi_tx_drv_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.3");
MODULE_DESCRIPTION("HDMI MSM TX driver");
