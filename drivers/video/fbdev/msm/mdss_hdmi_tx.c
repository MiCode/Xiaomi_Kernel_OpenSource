/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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
#include <linux/hdcp_qseecom.h>

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
#define DEFAULT_HDMI_PRIMARY_RESOLUTION HDMI_VFRMT_1920x1080p60_16_9

/* HDMI PHY/PLL bit field macros */
#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)

#define HPD_DISCONNECT_POLARITY 0
#define HPD_CONNECT_POLARITY    1

#define AUDIO_ACK_SET_ENABLE BIT(5)
#define AUDIO_ACK_ENABLE BIT(4)
#define AUDIO_ACK_CONNECT BIT(0)

/*
 * Audio engine may take 1 to 3 sec to shutdown
 * in normal cases. To handle worst cases, making
 * timeout for audio engine shutdown as 5 sec.
 */
#define AUDIO_POLL_SLEEP_US   (5 * 1000)
#define AUDIO_POLL_TIMEOUT_US (AUDIO_POLL_SLEEP_US * 1000)

#define LPA_DMA_IDLE_MAX 200

#define IFRAME_CHECKSUM_32(d)			\
	((d & 0xff) + ((d >> 8) & 0xff) +	\
	((d >> 16) & 0xff) + ((d >> 24) & 0xff))


/*
 * Pixel Clock to TMDS Character Rate Ratios.
 */
#define HDMI_TX_YUV420_24BPP_PCLK_TMDS_CH_RATE_RATIO 2
#define HDMI_TX_YUV422_24BPP_PCLK_TMDS_CH_RATE_RATIO 1
#define HDMI_TX_RGB_24BPP_PCLK_TMDS_CH_RATE_RATIO 1

#define HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ 340000
#define HDMI_TX_SCRAMBLER_TIMEOUT_MSEC 200

#define HDMI_TX_KHZ_TO_HZ 1000
#define HDMI_TX_MHZ_TO_HZ 1000000

/* Maximum pixel clock rates for hdmi tx */
#define HDMI_DEFAULT_MAX_PCLK_RATE         148500
#define HDMI_TX_3_MAX_PCLK_RATE            297000
#define HDMI_TX_4_MAX_PCLK_RATE            600000

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

/* AVI INFOFRAME DATA */
#define NUM_MODES_AVI 20
#define AVI_MAX_DATA_BYTES 13

/* Line numbers at which AVI Infoframe and Vendor Infoframe will be sent */
#define AVI_IFRAME_LINE_NUMBER 1
#define VENDOR_IFRAME_LINE_NUMBER 3
#define MAX_EDID_READ_RETRY	5

enum {
	DATA_BYTE_1,
	DATA_BYTE_2,
	DATA_BYTE_3,
	DATA_BYTE_4,
	DATA_BYTE_5,
	DATA_BYTE_6,
	DATA_BYTE_7,
	DATA_BYTE_8,
	DATA_BYTE_9,
	DATA_BYTE_10,
	DATA_BYTE_11,
	DATA_BYTE_12,
	DATA_BYTE_13,
};

#define IFRAME_PACKET_OFFSET 0x80
/*
 * InfoFrame Type Code:
 * 0x0 - Reserved
 * 0x1 - Vendor Specific
 * 0x2 - Auxiliary Video Information
 * 0x3 - Source Product Description
 * 0x4 - AUDIO
 * 0x5 - MPEG Source
 * 0x6 - NTSC VBI
 * 0x7 - 0xFF - Reserved
 */
#define AVI_IFRAME_TYPE 0x2
#define AVI_IFRAME_VERSION 0x2
#define LEFT_SHIFT_BYTE(x) ((x) << 8)
#define LEFT_SHIFT_WORD(x) ((x) << 16)
#define LEFT_SHIFT_24BITS(x) ((x) << 24)

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

enum hdmi_tx_hpd_states {
	HPD_OFF,
	HPD_ON,
	HPD_ON_CONDITIONAL_MTP,
	HPD_DISABLE,
	HPD_ENABLE
};

enum hdmi_tx_res_states {
	RESOLUTION_UNCHANGED,
	RESOLUTION_CHANGED
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
static int hdmi_tx_audio_setup(struct hdmi_tx_ctrl *hdmi_ctrl);
static int hdmi_tx_setup_tmds_clk_rate(struct hdmi_tx_ctrl *hdmi_ctrl);
static void hdmi_tx_set_vendor_specific_infoframe(
	struct hdmi_tx_ctrl *hdmi_ctrl);

static struct mdss_hw hdmi_tx_hw = {
	.hw_ndx = MDSS_HW_HDMI,
	.ptr = NULL,
	.irq_handler = hdmi_tx_isr,
};

static struct dss_gpio hpd_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-hpd"},
	{0, 1, COMPATIBLE_NAME "-mux-en"},
	{0, 0, COMPATIBLE_NAME "-mux-sel"},
	{0, 1, COMPATIBLE_NAME "-mux-lpm"}
};

static struct dss_gpio ddc_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-ddc-mux-sel"},
	{0, 1, COMPATIBLE_NAME "-ddc-clk"},
	{0, 1, COMPATIBLE_NAME "-ddc-data"}
};

static struct dss_gpio core_gpio_config[] = {
};

static struct dss_gpio cec_gpio_config[] = {
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
	{27027, {{4096, 27027}, {6272, 30030}, {6144, 27027}, {12544, 30030},
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
	/* 594.000MHz */
	{594000, {{3072, 445500}, {9408, 990000}, {6144, 594000},
		{18816, 990000}, {12288, 594000}, {37632, 990000},
		{24576, 594000} } },
};

static int hdmi_tx_get_version(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc;
	int reg_val;
	struct dss_io_data *io;

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, true);
	if (rc) {
		DEV_ERR("%s: Failed to read HDMI version\n", __func__);
		goto fail;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		rc = -EINVAL;
		goto fail;
	}

	reg_val = DSS_REG_R(io, HDMI_VERSION);
	reg_val = (reg_val & 0xF0000000) >> 28;
	hdmi_ctrl->hdmi_tx_ver = reg_val;

	switch (hdmi_ctrl->hdmi_tx_ver) {
	case (HDMI_TX_VERSION_3):
		hdmi_ctrl->max_pclk_khz = HDMI_TX_3_MAX_PCLK_RATE;
		break;
	case (HDMI_TX_VERSION_4):
		hdmi_ctrl->max_pclk_khz = HDMI_TX_4_MAX_PCLK_RATE;
		break;
	default:
		hdmi_ctrl->max_pclk_khz = HDMI_DEFAULT_MAX_PCLK_RATE;
		break;
	}

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, false);
	if (rc) {
		DEV_ERR("%s: FAILED to disable power\n", __func__);
		goto fail;
	}

fail:
	return rc;
}

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

static inline bool hdmi_tx_is_hdcp_enabled(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return hdmi_ctrl->hdcp_feature_on &&
		(hdmi_ctrl->hdcp14_present || hdmi_ctrl->hdcp22_present) &&
		hdmi_ctrl->hdcp_ops;
}

static const char *hdmi_tx_pm_name(enum hdmi_tx_power_module_type module)
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
	case HDMI_TX_QFPROM_IO:	return "qfprom_physical";
	case HDMI_TX_HDCP_IO:	return "hdcp_physical";
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
		struct msm_hdmi_mode_timing_info info = {0};
		u32 ret = hdmi_get_supported_mode(&info,
				&hdmi_ctrl->ds_data, pinfo->vic);
		u32 supported = info.supported;

		if (!ret && supported) {
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

		new_vic = hdmi_get_video_id_code(&timing, &hdmi_ctrl->ds_data);
	}

	return new_vic;
} /* hdmi_tx_get_vic_from_panel_info */

static inline u32 hdmi_tx_is_dvi_mode(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return hdmi_edid_get_sink_mode(
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]) ? 0 : 1;
} /* hdmi_tx_is_dvi_mode */

static inline bool hdmi_tx_is_panel_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return hdmi_ctrl->hpd_state && hdmi_ctrl->panel_power_on;
}

static inline void hdmi_tx_send_cable_notification(
	struct hdmi_tx_ctrl *hdmi_ctrl, int val)
{
	int state = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	state = hdmi_ctrl->sdev.state;

	switch_set_state(&hdmi_ctrl->sdev, val);

	DEV_INFO("%s: cable state %s %d\n", __func__,
		hdmi_ctrl->sdev.state == state ?
			"is same" : "switched to",
		hdmi_ctrl->sdev.state);

	/* Notify all registered modules of cable connection status */
	schedule_work(&hdmi_ctrl->cable_notify_work);
} /* hdmi_tx_send_cable_notification */

static inline void hdmi_tx_set_audio_switch_node(
	struct hdmi_tx_ctrl *hdmi_ctrl, int val)
{
	int state = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	state = hdmi_ctrl->audio_sdev.state;

	if (hdmi_ctrl->audio_ack_enabled &&
		atomic_read(&hdmi_ctrl->audio_ack_pending)) {
		DEV_ERR("%s: %s ack pending, not notifying %s\n", __func__,
			state ? "connect" : "disconnect",
			val ? "connect" : "disconnect");
		return;
	}

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl) &&
	    hdmi_tx_is_cea_format(hdmi_ctrl->vid_cfg.vic)) {
		bool switched;

		switch_set_state(&hdmi_ctrl->audio_sdev, val);
		switched = hdmi_ctrl->audio_sdev.state != state;

		if (hdmi_ctrl->audio_ack_enabled && switched)
			atomic_set(&hdmi_ctrl->audio_ack_pending, 1);

		DEV_INFO("%s: audio state %s %d\n", __func__,
			switched ? "switched to" : "is same",
			hdmi_ctrl->audio_sdev.state);
	}
} /* hdmi_tx_set_audio_switch_node */

static void hdmi_tx_wait_for_audio_engine(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u64 status = 0;
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

static ssize_t hdmi_tx_sysfs_wta_audio_cb(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ack, rc = 0;
	int ack_hpd;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &ack);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	if (ack & AUDIO_ACK_SET_ENABLE) {
		hdmi_ctrl->audio_ack_enabled = ack & AUDIO_ACK_ENABLE ?
			true : false;

		DEV_INFO("%s: audio ack feature %s\n", __func__,
			hdmi_ctrl->audio_ack_enabled ? "enabled" : "disabled");

		return ret;
	}

	if (!hdmi_ctrl->audio_ack_enabled)
		return ret;

	atomic_set(&hdmi_ctrl->audio_ack_pending, 0);

	ack_hpd = ack & AUDIO_ACK_CONNECT;

	if (ack_hpd != hdmi_ctrl->hpd_state) {
		DEV_INFO("%s: unbalanced audio state, ack %d, hpd %d\n",
			__func__, ack_hpd, hdmi_ctrl->hpd_state);

		hdmi_tx_set_audio_switch_node(hdmi_ctrl, hdmi_ctrl->hpd_state);
	}

	return ret;
}

static ssize_t hdmi_tx_sysfs_rda_video_mode(struct device *dev,
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
	ret = snprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->vid_cfg.vic);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->vid_cfg.vic);
	mutex_unlock(&hdmi_ctrl->mutex);

	return ret;
} /* hdmi_tx_sysfs_rda_video_mode */

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

	if (hdmi_ctrl->ds_registered && hpd &&
	    (!hdmi_ctrl->mhl_hpd_on || hdmi_ctrl->hpd_feature_on))
		return 0;

	switch (hpd) {
	case HPD_OFF:
	case HPD_DISABLE:
		if (hpd == HPD_DISABLE)
			hdmi_ctrl->hpd_disabled = true;

		if (!hdmi_ctrl->hpd_feature_on) {
			DEV_DBG("%s: HPD is already off\n", __func__);
			return ret;
		}

		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, false);

		mutex_lock(&hdmi_ctrl->power_mutex);
		if (hdmi_tx_is_panel_on(hdmi_ctrl)) {
			mutex_unlock(&hdmi_ctrl->power_mutex);
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 0);
			hdmi_tx_wait_for_audio_engine(hdmi_ctrl);
		} else {
			mutex_unlock(&hdmi_ctrl->power_mutex);
		}

		hdmi_tx_send_cable_notification(hdmi_ctrl, 0);
		break;
	case HPD_ON:
		if (hdmi_ctrl->hpd_disabled == true) {
			DEV_ERR("%s: hpd is disabled, state %d not allowed\n",
				__func__, hpd);
			return ret;
		}

		if (hdmi_ctrl->pdata.cond_power_on) {
			DEV_ERR("%s: hpd state %d not allowed w/ cond. hpd\n",
				__func__, hpd);
			return ret;
		}

		if (hdmi_ctrl->hpd_feature_on) {
			DEV_DBG("%s: HPD is already on\n", __func__);
			return ret;
		}

		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		break;
	case HPD_ON_CONDITIONAL_MTP:
		if (hdmi_ctrl->hpd_disabled == true) {
			DEV_ERR("%s: hpd is disabled, state %d not allowed\n",
				__func__, hpd);
			return ret;
		}

		if (!hdmi_ctrl->pdata.cond_power_on) {
			DEV_ERR("%s: hpd state %d not allowed w/o cond. hpd\n",
				__func__, hpd);
			return ret;
		}

		if (hdmi_ctrl->hpd_feature_on) {
			DEV_DBG("%s: HPD is already on\n", __func__);
			return ret;
		}

		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		break;
	case HPD_ENABLE:
		hdmi_ctrl->hpd_disabled = false;

		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		break;
	default:
		DEV_ERR("%s: Invalid HPD state requested\n", __func__);
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
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int itc = 0, rc = 0;

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

	hdmi_ctrl->vid_cfg.avi_iframe.is_it_content =
		itc ? true : false;

	mutex_unlock(&hdmi_ctrl->lut_lock);

	return ret;
} /* hdmi_tx_sysfs_wta_avi_itc */

static ssize_t hdmi_tx_sysfs_wta_avi_cn_bits(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int cn_bits = 0, rc = 0;

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

	hdmi_ctrl->vid_cfg.avi_iframe.content_type = cn_bits;

	mutex_unlock(&hdmi_ctrl->lut_lock);

	return ret;
} /* hdmi_tx_sysfs_wta_cn_bits */

static ssize_t hdmi_tx_sysfs_wta_s3d_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc, s3d_mode;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &s3d_mode);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	if (s3d_mode < HDMI_S3D_NONE || s3d_mode >= HDMI_S3D_MAX) {
		DEV_ERR("%s: invalid s3d mode = %d\n", __func__, s3d_mode);
		return -EINVAL;
	}

	if (s3d_mode > HDMI_S3D_NONE &&
		!hdmi_edid_is_s3d_mode_supported(
			hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID],
			hdmi_ctrl->vid_cfg.vic,
			s3d_mode)) {
		DEV_ERR("%s: s3d mode not supported in current video mode\n",
			__func__);
		return -EPERM;
	}

	hdmi_ctrl->s3d_mode = s3d_mode;
	hdmi_tx_set_vendor_specific_infoframe(hdmi_ctrl);

	DEV_DBG("%s: %d\n", __func__, hdmi_ctrl->s3d_mode);
	return ret;
}

static ssize_t hdmi_tx_sysfs_rda_s3d_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->s3d_mode);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->s3d_mode);

	return ret;
}

static DEVICE_ATTR(connected, S_IRUGO, hdmi_tx_sysfs_rda_connected, NULL);
static DEVICE_ATTR(hdmi_audio_cb, S_IWUSR, NULL, hdmi_tx_sysfs_wta_audio_cb);
static DEVICE_ATTR(video_mode, S_IRUGO, hdmi_tx_sysfs_rda_video_mode, NULL);
static DEVICE_ATTR(hpd, S_IRUGO | S_IWUSR, hdmi_tx_sysfs_rda_hpd,
	hdmi_tx_sysfs_wta_hpd);
static DEVICE_ATTR(vendor_name, S_IRUGO | S_IWUSR,
	hdmi_tx_sysfs_rda_vendor_name, hdmi_tx_sysfs_wta_vendor_name);
static DEVICE_ATTR(product_description, S_IRUGO | S_IWUSR,
	hdmi_tx_sysfs_rda_product_description,
	hdmi_tx_sysfs_wta_product_description);
static DEVICE_ATTR(avi_itc, S_IWUSR, NULL, hdmi_tx_sysfs_wta_avi_itc);
static DEVICE_ATTR(avi_cn0_1, S_IWUSR, NULL, hdmi_tx_sysfs_wta_avi_cn_bits);
static DEVICE_ATTR(s3d_mode, S_IRUGO | S_IWUSR, hdmi_tx_sysfs_rda_s3d_mode,
	hdmi_tx_sysfs_wta_s3d_mode);

static struct attribute *hdmi_tx_fs_attrs[] = {
	&dev_attr_connected.attr,
	&dev_attr_hdmi_audio_cb.attr,
	&dev_attr_video_mode.attr,
	&dev_attr_hpd.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_product_description.attr,
	&dev_attr_avi_itc.attr,
	&dev_attr_avi_cn0_1.attr,
	&dev_attr_s3d_mode.attr,
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

static int hdmi_tx_config_avmute(struct hdmi_tx_ctrl *hdmi_ctrl, bool set)
{
	struct dss_io_data *io;
	u32 av_mute_status;
	bool av_pkt_en = false;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return -EINVAL;
	}

	av_mute_status = DSS_REG_R(io, HDMI_GC);

	if (set) {
		if (!(av_mute_status & BIT(0))) {
			DSS_REG_W(io, HDMI_GC, av_mute_status | BIT(0));
			av_pkt_en = true;
		}
	} else {
		if (av_mute_status & BIT(0)) {
			DSS_REG_W(io, HDMI_GC, av_mute_status & ~BIT(0));
			av_pkt_en = true;
		}
	}

	/* Enable AV Mute tranmission here */
	if (av_pkt_en)
		DSS_REG_W(io, HDMI_VBI_PKT_CTRL,
			DSS_REG_R(io, HDMI_VBI_PKT_CTRL) | (BIT(4) & BIT(5)));

	DEV_DBG("%s: AVMUTE %s\n", __func__, set ? "set" : "cleared");

	return 0;
} /* hdmi_tx_config_avmute */

static bool hdmi_tx_is_encryption_set(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io;
	bool enc_en = true;
	u32 reg_val;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto end;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		goto end;
	}

	reg_val = DSS_REG_R_ND(io, HDMI_HDCP_CTRL2);
	if ((reg_val & BIT(0)) && (reg_val & BIT(1)))
		goto end;

	if (DSS_REG_R_ND(io, HDMI_CTRL) & BIT(2))
		goto end;

	return false;

end:
	return enc_en;
} /* hdmi_tx_is_encryption_set */

static void hdmi_tx_hdcp_cb(void *ptr, enum hdmi_hdcp_state status)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = (struct hdmi_tx_ctrl *)ptr;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	hdmi_ctrl->hdcp_status = status;

	queue_delayed_work(hdmi_ctrl->workq, &hdmi_ctrl->hdcp_cb_work, HZ/4);
}

static inline bool hdmi_tx_is_stream_shareable(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	bool ret;

	switch (hdmi_ctrl->enc_lvl) {
	case HDCP_STATE_AUTH_ENC_NONE:
		ret = true;
		break;
	case HDCP_STATE_AUTH_ENC_1X:
		ret = hdmi_tx_is_hdcp_enabled(hdmi_ctrl) &&
			hdmi_ctrl->auth_state;
		break;
	case HDCP_STATE_AUTH_ENC_2P2:
		ret = hdmi_ctrl->hdcp_feature_on &&
			hdmi_ctrl->hdcp22_present &&
			hdmi_ctrl->auth_state;
		break;
	default:
		ret = false;
	}

	return ret;
}

static void hdmi_tx_hdcp_cb_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct delayed_work *dw = to_delayed_work(work);
	int rc = 0;

	hdmi_ctrl = container_of(dw, struct hdmi_tx_ctrl, hdcp_cb_work);
	if (!hdmi_ctrl) {
		DEV_DBG("%s: invalid input\n", __func__);
		return;
	}

	DEV_DBG("%s: HDCP status=%s hpd_state=%d\n", __func__,
		hdcp_state_name(hdmi_ctrl->hdcp_status), hdmi_ctrl->hpd_state);

	switch (hdmi_ctrl->hdcp_status) {
	case HDCP_STATE_AUTHENTICATED:
		hdmi_ctrl->auth_state = true;

		if (hdmi_tx_is_panel_on(hdmi_ctrl) &&
			hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
			rc = hdmi_tx_config_avmute(hdmi_ctrl, false);
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 1);
		}
		break;
	case HDCP_STATE_AUTH_FAIL:
		hdmi_ctrl->auth_state = false;

		if (hdmi_tx_is_encryption_set(hdmi_ctrl) ||
			!hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 0);
			rc = hdmi_tx_config_avmute(hdmi_ctrl, true);
		}

		if (hdmi_tx_is_panel_on(hdmi_ctrl)) {
			DEV_DBG("%s: Reauthenticating\n", __func__);
			rc = hdmi_ctrl->hdcp_ops->hdmi_hdcp_reauthenticate(
				hdmi_ctrl->hdcp_data);
			if (rc)
				DEV_ERR("%s: HDCP reauth failed. rc=%d\n",
					__func__, rc);
		} else {
			DEV_DBG("%s: Not reauthenticating. Cable not conn\n",
				__func__);
		}

		break;
	case HDCP_STATE_AUTH_ENC_NONE:
		hdmi_ctrl->enc_lvl = HDCP_STATE_AUTH_ENC_NONE;

		if (hdmi_tx_is_panel_on(hdmi_ctrl)) {
			rc = hdmi_tx_config_avmute(hdmi_ctrl, false);
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 1);
		}
		break;
	case HDCP_STATE_AUTH_ENC_1X:
	case HDCP_STATE_AUTH_ENC_2P2:
		hdmi_ctrl->enc_lvl = hdmi_ctrl->hdcp_status;

		if (hdmi_tx_is_panel_on(hdmi_ctrl) &&
			hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
			rc = hdmi_tx_config_avmute(hdmi_ctrl, false);
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 1);
		} else {
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 0);
			rc = hdmi_tx_config_avmute(hdmi_ctrl, true);
		}
		break;
	default:
		break;
		/* do nothing */
	}
}

static u32 hdmi_tx_ddc_read(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	u32 block, u8 *edid_buf)
{
	u32 block_size = EDID_BLOCK_SIZE;
	struct hdmi_tx_ddc_data ddc_data;
	u32 status = 0, retry_cnt = 0, i;

	if (!ddc_ctrl || !edid_buf) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	do {
		DEV_DBG("EDID: reading block(%d) with block-size=%d\n",
				block, block_size);

		for (i = 0; i < EDID_BLOCK_SIZE; i += block_size) {
			memset(&ddc_data, 0, sizeof(ddc_data));

			ddc_data.dev_addr    = EDID_BLOCK_ADDR;
			ddc_data.offset      = block * EDID_BLOCK_SIZE + i;
			ddc_data.data_buf    = edid_buf + i;
			ddc_data.data_len    = block_size;
			ddc_data.request_len = block_size;
			ddc_data.retry       = 1;
			ddc_data.what        = "EDID";
			ddc_data.no_align    = false;

			ddc_ctrl->ddc_data = ddc_data;

			/* Read EDID twice with 32bit alighnment too */
			if (block < 2)
				status = hdmi_ddc_read(ddc_ctrl);
			else
				status = hdmi_ddc_read_seg(ddc_ctrl);

			if (status)
				break;
		}
		if (retry_cnt++ >= MAX_EDID_READ_RETRY)
			block_size /= 2;

	} while (status && (block_size >= 16));

	return status;
}

static int hdmi_tx_read_edid_retry(struct hdmi_tx_ctrl *hdmi_ctrl, u8 block)
{
	u32 checksum_retry = 0;
	u8 *ebuf;
	int ret = 0;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ebuf = hdmi_ctrl->edid_buf;
	if (!ebuf) {
		DEV_ERR("%s: invalid edid buf\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ddc_ctrl = &hdmi_ctrl->ddc_ctrl;

	while (checksum_retry++ < MAX_EDID_READ_RETRY) {
		ret = hdmi_tx_ddc_read(ddc_ctrl, block,
			ebuf + (block * EDID_BLOCK_SIZE));
		if (ret)
			continue;
		else
			break;
	}
end:
	return ret;
}

static int hdmi_tx_read_edid(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int ndx, check_sum;
	int cea_blks = 0, block = 0, total_blocks = 0;
	int ret = 0;
	u8 *ebuf;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ebuf = hdmi_ctrl->edid_buf;
	if (!ebuf) {
		DEV_ERR("%s: invalid edid buf\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	memset(ebuf, 0, hdmi_ctrl->edid_buf_size);

	ddc_ctrl = &hdmi_ctrl->ddc_ctrl;

	do {
		if (block * EDID_BLOCK_SIZE > hdmi_ctrl->edid_buf_size) {
			DEV_ERR("%s: no mem for block %d, max mem %d\n",
				__func__, block, hdmi_ctrl->edid_buf_size);
			ret = -ENOMEM;
			goto end;
		}

		ret = hdmi_tx_read_edid_retry(hdmi_ctrl, block);
		if (ret) {
			DEV_ERR("%s: edid read failed\n", __func__);
			goto end;
		}

		/* verify checksum to validate edid block */
		check_sum = 0;
		for (ndx = 0; ndx < EDID_BLOCK_SIZE; ++ndx)
			check_sum += ebuf[ndx];

		if (check_sum & 0xFF) {
			DEV_ERR("%s: checksome mismatch\n", __func__);
			ret = -EINVAL;
			goto end;
		}

		/* get number of cea extension blocks as given in block 0*/
		if (block == 0) {
			cea_blks = ebuf[EDID_BLOCK_SIZE - 2];
			if (cea_blks < 0 || cea_blks >= MAX_EDID_BLOCKS) {
				cea_blks = 0;
				DEV_ERR("%s: invalid cea blocks %d\n",
					__func__, cea_blks);
				ret = -EINVAL;
				goto end;
			}

			total_blocks = cea_blks + 1;
		}
	} while ((cea_blks-- > 0) && (block++ < MAX_EDID_BLOCKS));
end:

	return ret;
}

/* Enable HDMI features */
static int hdmi_tx_init_features(struct hdmi_tx_ctrl *hdmi_ctrl,
	struct fb_info *fbi)
{
	struct hdmi_edid_init_data edid_init_data;
	struct hdmi_hdcp_init_data hdcp_init_data;
	struct hdmi_cec_init_data cec_init_data;
	struct resource *res = NULL;
	void *fd = NULL;

	if (!hdmi_ctrl || !fbi) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* Initialize EDID feature */
	edid_init_data.kobj = hdmi_ctrl->kobj;
	edid_init_data.ds_data = &hdmi_ctrl->ds_data;
	edid_init_data.max_pclk_khz = hdmi_ctrl->max_pclk_khz;

	fd = hdmi_edid_init(&edid_init_data);
	if (!fd) {
		DEV_ERR("%s: hdmi_edid_init failed\n", __func__);
		return -EPERM;
	}

	hdmi_ctrl->panel_data.panel_info.edid_data = fd;

	/* get edid buffer from edid parser */
	hdmi_ctrl->edid_buf = edid_init_data.buf;
	hdmi_ctrl->edid_buf_size = edid_init_data.buf_size;

	hdmi_edid_set_video_resolution(fd, hdmi_ctrl->vid_cfg.vic, true);

	hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID] = fd;

	/* Initialize HDCP features */
	res = platform_get_resource_byname(hdmi_ctrl->pdev,
		IORESOURCE_MEM, hdmi_tx_io_name(HDMI_TX_CORE_IO));
	if (!res) {
		DEV_ERR("%s: Error getting HDMI tx core resource\n",
			__func__);
		return -ENODEV;
	}

	hdcp_init_data.phy_addr      = res->start;
	hdcp_init_data.core_io       = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	hdcp_init_data.qfprom_io     = &hdmi_ctrl->pdata.io[HDMI_TX_QFPROM_IO];
	hdcp_init_data.hdcp_io       = &hdmi_ctrl->pdata.io[HDMI_TX_HDCP_IO];
	hdcp_init_data.mutex         = &hdmi_ctrl->mutex;
	hdcp_init_data.sysfs_kobj    = hdmi_ctrl->kobj;
	hdcp_init_data.ddc_ctrl      = &hdmi_ctrl->ddc_ctrl;
	hdcp_init_data.workq         = hdmi_ctrl->workq;
	hdcp_init_data.notify_status = hdmi_tx_hdcp_cb;
	hdcp_init_data.cb_data       = (void *)hdmi_ctrl;
	hdcp_init_data.hdmi_tx_ver   = hdmi_ctrl->hdmi_tx_ver;
	hdcp_init_data.timing        = &hdmi_ctrl->vid_cfg.timing;

	if (hdmi_ctrl->hdcp14_present) {
		fd = hdmi_hdcp_init(&hdcp_init_data);

		if (IS_ERR_OR_NULL(fd)) {
			DEV_WARN("%s: hdmi_hdcp_init failed\n", __func__);
		} else {
			hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP] = fd;
			DEV_DBG("%s: HDCP 1.4 configured\n", __func__);
		}
	}

	fd = hdmi_hdcp2p2_init(&hdcp_init_data);

	if (IS_ERR_OR_NULL(fd)) {
		DEV_WARN("%s: hdmi_hdcp2p2_init failed\n", __func__);
	} else {
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP2P2] = fd;
		DEV_DBG("%s: HDCP 2.2 configured\n", __func__);
	}

	/* Initialize CEC feature */
	cec_init_data.io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	cec_init_data.sysfs_kobj = hdmi_ctrl->kobj;
	cec_init_data.workq = hdmi_ctrl->workq;

	fd = hdmi_cec_init(&cec_init_data);
	if (!fd)
		DEV_WARN("%s: hdmi_cec_init failed\n", __func__);
	else
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC] = fd;

	return 0;
} /* hdmi_tx_init_features */

static inline u32 hdmi_tx_is_controller_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	return DSS_REG_R_ND(io, HDMI_CTRL) & BIT(0);
} /* hdmi_tx_is_controller_on */

static int hdmi_tx_init_panel_info(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct mdss_panel_info *pinfo;
	struct msm_hdmi_mode_timing_info timing = {0};
	u32 ret;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = hdmi_get_supported_mode(&timing, &hdmi_ctrl->ds_data,
		hdmi_ctrl->vid_cfg.vic);
	pinfo = &hdmi_ctrl->panel_data.panel_info;

	if (ret || !timing.supported || !pinfo) {
		DEV_ERR("%s: invalid timing data\n", __func__);
		return -EINVAL;
	}

	pinfo->xres = timing.active_h;
	pinfo->yres = timing.active_v;
	pinfo->clk_rate = timing.pixel_freq * 1000;

	pinfo->lcdc.h_back_porch = timing.back_porch_h;
	pinfo->lcdc.h_front_porch = timing.front_porch_h;
	pinfo->lcdc.h_pulse_width = timing.pulse_width_h;
	pinfo->lcdc.v_back_porch = timing.back_porch_v;
	pinfo->lcdc.v_front_porch = timing.front_porch_v;
	pinfo->lcdc.v_pulse_width = timing.pulse_width_v;

	pinfo->type = DTV_PANEL;
	pinfo->pdest = DISPLAY_2;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 1;

	pinfo->lcdc.border_clr = 0; /* blk */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;

	pinfo->cont_splash_enabled = hdmi_ctrl->pdata.cont_splash_enabled;
	pinfo->is_pluggable = hdmi_ctrl->pdata.pluggable;

	return 0;
} /* hdmi_tx_init_panel_info */

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

	status = hdmi_tx_read_edid(hdmi_ctrl);
	if (status) {
		DEV_ERR("%s: error reading edid\n", __func__);
		goto error;
	}

	status = hdmi_edid_parser(hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]);
	if (status)
		DEV_ERR("%s: edid parse failed\n", __func__);

error:
	return status;
} /* hdmi_tx_read_sink_info */

static void hdmi_tx_update_hdcp_info(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	void *fd = NULL;
	struct hdmi_hdcp_ops *ops = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	/* check first if hdcp2p2 is supported */
	fd = hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP2P2];
	if (fd)
		ops = hdmi_hdcp2p2_start(fd);

	if (ops && ops->feature_supported)
		hdmi_ctrl->hdcp22_present = ops->feature_supported(fd);
	else
		hdmi_ctrl->hdcp22_present = false;

	if (!hdmi_ctrl->hdcp22_present) {
		if (hdmi_ctrl->hdcp1_use_sw_keys)
			hdmi_ctrl->hdcp14_present =
				hdcp1_check_if_supported_load_app();

		if (hdmi_ctrl->hdcp14_present) {
			fd = hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP];
			ops = hdmi_hdcp_start(fd);
		}
	}

	/* update internal data about hdcp */
	hdmi_ctrl->hdcp_data = fd;
	hdmi_ctrl->hdcp_ops = ops;
}

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
	DEV_DBG("%s: Got HPD %s interrupt\n", __func__,
		hdmi_ctrl->hpd_state ? "CONNECT" : "DISCONNECT");

	if (hdmi_ctrl->hpd_state) {
		if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, true)) {
			DEV_ERR("%s: Failed to enable ddc power\n", __func__);
			return;
		}

		/* Enable SW DDC before EDID read */
		DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION ,
			DSS_REG_R(io, HDMI_DDC_ARBITRATION) & ~(BIT(4)));

		hdmi_tx_read_sink_info(hdmi_ctrl);

		if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, false))
			DEV_ERR("%s: Failed to disable ddc power\n", __func__);

		hdmi_tx_update_hdcp_info(hdmi_ctrl);

		hdmi_tx_send_cable_notification(hdmi_ctrl, true);
	} else {
		hdmi_tx_set_audio_switch_node(hdmi_ctrl, 0);
		hdmi_tx_wait_for_audio_engine(hdmi_ctrl);

		hdmi_tx_send_cable_notification(hdmi_ctrl, false);
	}

	if (!completion_done(&hdmi_ctrl->hpd_int_done))
		complete_all(&hdmi_ctrl->hpd_int_done);
} /* hdmi_tx_hpd_int_work */

static int hdmi_tx_check_capability(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 hdmi_disabled, hdcp_disabled, reg_val;
	struct dss_io_data *io = NULL;
	int ret = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_QFPROM_IO];
	if (!io->base) {
		DEV_ERR("%s: QFPROM io is not initialized\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	/* check if hdmi and hdcp are disabled */
	if (hdmi_ctrl->hdmi_tx_ver < HDMI_TX_VERSION_4) {
		hdcp_disabled = DSS_REG_R_ND(io,
			QFPROM_RAW_FEAT_CONFIG_ROW0_LSB) & BIT(31);

		hdmi_disabled = DSS_REG_R_ND(io,
			QFPROM_RAW_FEAT_CONFIG_ROW0_MSB) & BIT(0);
	} else {
		reg_val = DSS_REG_R_ND(io,
			QFPROM_RAW_FEAT_CONFIG_ROW0_LSB + QFPROM_RAW_VERSION_4);
		hdcp_disabled = reg_val & BIT(12);
		hdmi_disabled = reg_val & BIT(13);

		reg_val = DSS_REG_R_ND(io, SEC_CTRL_HW_VERSION);
		/*
		 * With HDCP enabled on capable hardware, check if HW
		 * or SW keys should be used.
		 */
		if (!hdcp_disabled && (reg_val >= HDCP_SEL_MIN_SEC_VERSION)) {
			reg_val = DSS_REG_R_ND(io,
				QFPROM_RAW_FEAT_CONFIG_ROW0_MSB +
				QFPROM_RAW_VERSION_4);
			if (!(reg_val & BIT(23)))
				hdmi_ctrl->hdcp1_use_sw_keys = true;
		}
	}

	DEV_DBG("%s: Features <HDMI:%s, HDCP:%s>\n", __func__,
		hdmi_disabled ? "OFF" : "ON", hdcp_disabled ? "OFF" : "ON");

	if (hdmi_disabled) {
		DEV_ERR("%s: HDMI disabled\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	hdmi_ctrl->hdcp14_present = !hdcp_disabled;
end:
	return ret;
} /* hdmi_tx_check_capability */

static int hdmi_tx_set_video_fmt(struct hdmi_tx_ctrl *hdmi_ctrl,
	struct mdss_panel_info *pinfo)
{
	int new_vic = -1;
	int res_changed = RESOLUTION_UNCHANGED;
	struct hdmi_video_config *vid_cfg = NULL;
	u32 ret;
	u32 div = 0;

	if (!hdmi_ctrl || !pinfo) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	vid_cfg = &hdmi_ctrl->vid_cfg;
	new_vic = hdmi_tx_get_vic_from_panel_info(hdmi_ctrl, pinfo);
	if ((new_vic < 0) || (new_vic > HDMI_VFRMT_MAX)) {
		DEV_ERR("%s: invalid or not supported vic\n", __func__);
		return -EPERM;
	}

	if (vid_cfg->vic != new_vic) {
		res_changed = RESOLUTION_CHANGED;
		DEV_DBG("%s: switching from %s => %s", __func__,
			msm_hdmi_mode_2string(vid_cfg->vic),
			msm_hdmi_mode_2string(new_vic));
	}

	vid_cfg->vic = (u32)new_vic;

	ret = hdmi_get_supported_mode(&vid_cfg->timing, &hdmi_ctrl->ds_data,
				      vid_cfg->vic);

	if (ret || !vid_cfg->timing.supported) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* Setup AVI Infoframe content */
	vid_cfg->vic = new_vic;
	vid_cfg->avi_iframe.pixel_format = pinfo->out_format;
	vid_cfg->avi_iframe.scan_info = hdmi_edid_get_sink_scaninfo(
				hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID],
				hdmi_ctrl->vid_cfg.vic);

	vid_cfg->avi_iframe.bar_info.end_of_top_bar = 0x0;
	vid_cfg->avi_iframe.bar_info.start_of_bottom_bar =
					vid_cfg->timing.active_v + 1;
	vid_cfg->avi_iframe.bar_info.end_of_left_bar = 0;
	vid_cfg->avi_iframe.bar_info.start_of_right_bar =
					vid_cfg->timing.active_h + 1;

	vid_cfg->avi_iframe.act_fmt_info_present = true;
	vid_cfg->avi_iframe.rgb_quantization_range = HDMI_QUANTIZATION_DEFAULT;
	vid_cfg->avi_iframe.yuv_quantization_range = HDMI_QUANTIZATION_DEFAULT;

	vid_cfg->avi_iframe.scaling_info = HDMI_SCALING_NONE;

	vid_cfg->avi_iframe.colorimetry_info = 0;
	vid_cfg->avi_iframe.ext_colorimetry_info = 0;

	vid_cfg->avi_iframe.pixel_rpt_factor = 0;

	/*
	 * If output format is yuv420, pixel clock rate should be half of the
	 * rate that is used for rgb888. MDP timing engine is programmed at half
	 * rate because the bits per pixel for yuv420 is only half that of
	 * rgb888
	 */
	if (pinfo->out_format  == MDP_Y_CBCR_H2V2)
		div = 1;

	hdmi_ctrl->pdata.power_data[HDMI_TX_CORE_PM].clk_config[0].rate =
		(vid_cfg->timing.pixel_freq * 1000) >> div;

	hdmi_edid_set_video_resolution(
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID],
		vid_cfg->vic, false);

	return res_changed;
} /* hdmi_tx_set_video_fmt */

static int hdmi_tx_video_setup(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 total_v   = 0;
	u32 total_h   = 0;
	u32 start_h   = 0;
	u32 end_h     = 0;
	u32 start_v   = 0;
	u32 end_v     = 0;
	u32 div       = 0;
	struct dss_io_data *io = NULL;
	struct msm_hdmi_mode_timing_info *timing = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	timing = &hdmi_ctrl->vid_cfg.timing;

	if (timing == NULL) {
		DEV_ERR("%s: video format not supported: %d\n", __func__,
			hdmi_ctrl->vid_cfg.vic);
		return -EPERM;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return -EPERM;
	}
	/*
	 * In case of YUV420 output, Horizontal timing parameters should be
	 * reduced by half
	 */
	if (hdmi_ctrl->vid_cfg.avi_iframe.pixel_format == MDP_Y_CBCR_H2V2)
		div = 1;

	total_h = (hdmi_tx_get_h_total(timing) >> div) - 1;
	total_v = hdmi_tx_get_v_total(timing) - 1;

	if (((total_v << 16) & 0xE0000000) || (total_h & 0xFFFFE000)) {
		DEV_ERR("%s: total v=%d or h=%d is larger than supported\n",
			__func__, total_v, total_h);
		return -EPERM;
	}
	DSS_REG_W(io, HDMI_TOTAL, (total_v << 16) | (total_h << 0));

	start_h = (timing->back_porch_h >> div) +
		  (timing->pulse_width_h >> div);
	end_h   = (total_h + 1) - (timing->front_porch_h >> div);
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
	int i;
	u8  avi_iframe[AVI_MAX_DATA_BYTES] = {0};
	u8 checksum;
	u32 sum, reg_val;
	struct dss_io_data *io = NULL;
	struct hdmi_avi_infoframe_config *avi_info;
	struct msm_hdmi_mode_timing_info *timing;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	avi_info = &hdmi_ctrl->vid_cfg.avi_iframe;
	timing = &hdmi_ctrl->vid_cfg.timing;

	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	/*
	 * BYTE - 1:
	 *	0:1 - Scan Information
	 *	2:3 - Bar Info
	 *	4   - Active Format Info present
	 *	5:6 - Pixel format type;
	 *	7   - Reserved;
	 */
	avi_iframe[0] = (avi_info->scan_info & 0x3) |
			(avi_info->bar_info.vert_binfo_present ? BIT(2) : 0) |
			(avi_info->bar_info.horz_binfo_present ? BIT(3) : 0) |
			(avi_info->act_fmt_info_present ? BIT(4) : 0);
	if (avi_info->pixel_format == MDP_Y_CBCR_H2V2)
		avi_iframe[0] |= (0x3 << 5);
	else if (avi_info->pixel_format == MDP_Y_CBCR_H2V1)
		avi_iframe[0] |= (0x1 << 5);
	else if (avi_info->pixel_format == MDP_Y_CBCR_H1V1)
		avi_iframe[0] |= (0x2 << 5);

	/*
	 * BYTE - 2:
	 *	0:3 - Active format info
	 *	4:5 - Picture aspect ratio
	 *	6:7 - Colorimetry info
	 */
	avi_iframe[1] |= 0x08;
	if (timing->ar == HDMI_RES_AR_4_3)
		avi_iframe[1] |= (0x1 << 4);
	else if (timing->ar == HDMI_RES_AR_16_9)
		avi_iframe[1] |= (0x2 << 4);

	avi_iframe[1] |= (avi_info->colorimetry_info & 0x3) << 6;

	/*
	 * BYTE - 3:
	 *	0:1 - Scaling info
	 *	2:3 - Quantization range
	 *	4:6 - Extended Colorimetry
	 *	7   - IT content
	 */
	avi_iframe[2] |= (avi_info->scaling_info & 0x3) |
			 ((avi_info->rgb_quantization_range & 0x3) << 2) |
			 ((avi_info->ext_colorimetry_info & 0x7) << 4) |
			 ((avi_info->is_it_content ? 0x1 : 0x0) << 7);
	/*
	 * BYTE - 4:
	 *	0:7 - VIC
	 */
	if (timing->video_format < HDMI_VFRMT_END)
		avi_iframe[3] = timing->video_format;

	/*
	 * BYTE - 5:
	 *	0:3 - Pixel Repeat factor
	 *	4:5 - Content type
	 *	6:7 - YCC Quantization range
	 */
	avi_iframe[4] = (avi_info->pixel_rpt_factor & 0xF) |
			((avi_info->content_type & 0x3) << 4) |
			((avi_info->yuv_quantization_range & 0x3) << 6);

	/* BYTE - 6,7: End of top bar */
	avi_iframe[5] = avi_info->bar_info.end_of_top_bar & 0xFF;
	avi_iframe[6] = ((avi_info->bar_info.end_of_top_bar & 0xFF00) >> 8);

	/* BYTE - 8,9: Start of bottom bar */
	avi_iframe[7] = avi_info->bar_info.start_of_bottom_bar & 0xFF;
	avi_iframe[8] = ((avi_info->bar_info.start_of_bottom_bar & 0xFF00) >>
			 8);

	/* BYTE - 10,11: Endof of left bar */
	avi_iframe[9] = avi_info->bar_info.end_of_left_bar & 0xFF;
	avi_iframe[10] = ((avi_info->bar_info.end_of_left_bar & 0xFF00) >> 8);

	/* BYTE - 12,13: Start of right bar */
	avi_iframe[11] = avi_info->bar_info.start_of_right_bar & 0xFF;
	avi_iframe[12] = ((avi_info->bar_info.start_of_right_bar & 0xFF00) >>
			  8);

	sum = IFRAME_PACKET_OFFSET + AVI_IFRAME_TYPE +
		AVI_IFRAME_VERSION + AVI_MAX_DATA_BYTES;

	for (i = 0; i < AVI_MAX_DATA_BYTES; i++)
		sum += avi_iframe[i];
	sum &= 0xFF;
	sum = 256 - sum;
	checksum = (u8) sum;

	reg_val = checksum |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_1]) |
		LEFT_SHIFT_WORD(avi_iframe[DATA_BYTE_2]) |
		LEFT_SHIFT_24BITS(avi_iframe[DATA_BYTE_3]);
	DSS_REG_W(io, HDMI_AVI_INFO0, reg_val);

	reg_val = avi_iframe[DATA_BYTE_4] |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_5]) |
		LEFT_SHIFT_WORD(avi_iframe[DATA_BYTE_6]) |
		LEFT_SHIFT_24BITS(avi_iframe[DATA_BYTE_7]);
	DSS_REG_W(io, HDMI_AVI_INFO1, reg_val);

	reg_val = avi_iframe[DATA_BYTE_8] |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_9]) |
		LEFT_SHIFT_WORD(avi_iframe[DATA_BYTE_10]) |
		LEFT_SHIFT_24BITS(avi_iframe[DATA_BYTE_11]);
	DSS_REG_W(io, HDMI_AVI_INFO2, reg_val);

	reg_val = avi_iframe[DATA_BYTE_12] |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_13]) |
		LEFT_SHIFT_24BITS(AVI_IFRAME_VERSION);
	DSS_REG_W(io, HDMI_AVI_INFO3, reg_val);

	/* AVI InfFrame enable (every frame) */
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0,
		DSS_REG_R(io, HDMI_INFOFRAME_CTRL0) | BIT(1) | BIT(0));

	reg_val = DSS_REG_R(io, HDMI_INFOFRAME_CTRL1);
	reg_val &= ~0x3F;
	reg_val |= AVI_IFRAME_LINE_NUMBER;
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL1, reg_val);
} /* hdmi_tx_set_avi_infoframe */

static void hdmi_tx_set_vendor_specific_infoframe(
	struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int i;
	u8 vs_iframe[9]; /* two header + length + 6 data */
	u32 sum, reg_val;
	u32 hdmi_vic, hdmi_video_format, s3d_struct = 0;
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

	if ((hdmi_ctrl->s3d_mode != HDMI_S3D_NONE) &&
		hdmi_edid_is_s3d_mode_supported(
			hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID],
			hdmi_ctrl->vid_cfg.vic,
			hdmi_ctrl->s3d_mode)) {
		switch (hdmi_ctrl->s3d_mode) {
		case HDMI_S3D_SIDE_BY_SIDE:
			s3d_struct = 0x8;
			break;
		case HDMI_S3D_TOP_AND_BOTTOM:
			s3d_struct = 0x6;
			break;
		default:
			s3d_struct = 0;
		}
		hdmi_video_format = 0x2;
		hdmi_vic = 0;
		/* PB5: 3D_Structure[7:4], Reserved[3:0] */
		vs_iframe[8] = s3d_struct << 4;
	} else {
		hdmi_video_format = 0x1;
		switch (hdmi_ctrl->vid_cfg.vic) {
		case HDMI_EVFRMT_3840x2160p30_16_9:
			hdmi_vic = 0x1;
			break;
		case HDMI_EVFRMT_3840x2160p25_16_9:
			hdmi_vic = 0x2;
			break;
		case HDMI_EVFRMT_3840x2160p24_16_9:
			hdmi_vic = 0x3;
			break;
		case HDMI_EVFRMT_4096x2160p24_16_9:
			hdmi_vic = 0x4;
			break;
		default:
			hdmi_video_format = 0x0;
			hdmi_vic = 0x0;
		}
		/* PB5: HDMI_VIC */
		vs_iframe[8] = hdmi_vic;
	}
	/* PB4: HDMI Video Format[7:5],  Reserved[4:0] */
	vs_iframe[7] = (hdmi_video_format << 5) & 0xE0;

	/* compute checksum */
	sum = 0;
	for (i = 0; i < 9; i++)
		sum += vs_iframe[i];

	sum &= 0xFF;
	sum = 256 - sum;
	vs_iframe[3] = (u8)sum;

	reg_val = (s3d_struct << 24) | (hdmi_vic << 16) | (vs_iframe[3] << 8) |
		(hdmi_video_format << 5) | vs_iframe[2];
	DSS_REG_W(io, HDMI_VENSPEC_INFO0, reg_val);

	/* vendor specific info-frame enable (every frame) */
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0,
		DSS_REG_R(io, HDMI_INFOFRAME_CTRL0) | BIT(13) | BIT(12));

	reg_val = DSS_REG_R(io, HDMI_INFOFRAME_CTRL1);
	reg_val &= ~0x3F000000;
	reg_val |= (VENDOR_IFRAME_LINE_NUMBER << 24);
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL1, reg_val);
} /* hdmi_tx_set_vendor_specific_infoframe */

void hdmi_tx_set_spd_infoframe(struct hdmi_tx_ctrl *hdmi_ctrl)
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

		/**
		 * HDMI Encryption, if HDCP is enabled
		 * The ENC_REQUIRED bit is only available on HDMI Tx major
		 * version less than 4. From 4 onwards, this bit is controlled
		 * by TZ
		 */
		if (hdmi_ctrl->hdmi_tx_ver < 4 &&
			hdmi_tx_is_hdcp_enabled(hdmi_ctrl) &&
			!hdmi_ctrl->pdata.primary)
			reg_val |= BIT(2);

		/* Set transmission mode to DVI based in EDID info */
		if (hdmi_edid_get_sink_mode(
			hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]) == 0)
			reg_val &= ~BIT(1); /* DVI mode */

		/*
		 * Use DATAPATH_MODE as 1 always, the new mode that also
		 * supports scrambler and HDCP 2.2. The legacy mode should no
		 * longer be used
		 */
		reg_val |= BIT(31);
	}

	DSS_REG_W(io, HDMI_CTRL, reg_val);
	mutex_unlock(&hdmi_ctrl->mutex);

	DEV_DBG("HDMI Core: %s, HDMI_CTRL=0x%08x\n",
		power_on ? "Enable" : "Disable", reg_val);
} /* hdmi_tx_set_mode */

static int hdmi_tx_pinctrl_set_state(struct hdmi_tx_ctrl *hdmi_ctrl,
			enum hdmi_tx_power_module_type module, bool active)
{
	struct pinctrl_state *pin_state = NULL;
	int rc = -EFAULT;
	struct dss_module_power *power_data = NULL;
	u64 cur_pin_states;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.pinctrl))
		return 0;

	power_data = &hdmi_ctrl->pdata.power_data[module];

	cur_pin_states = active ? (hdmi_ctrl->pdata.pin_states | BIT(module))
				: (hdmi_ctrl->pdata.pin_states & ~BIT(module));

	if (cur_pin_states & BIT(HDMI_TX_HPD_PM)) {
		if (cur_pin_states & BIT(HDMI_TX_DDC_PM)) {
			if (cur_pin_states & BIT(HDMI_TX_CEC_PM))
				pin_state = hdmi_ctrl->pin_res.state_active;
			else
				pin_state =
					hdmi_ctrl->pin_res.state_ddc_active;
		} else if (cur_pin_states & BIT(HDMI_TX_CEC_PM)) {
			pin_state = hdmi_ctrl->pin_res.state_cec_active;
		} else {
			pin_state = hdmi_ctrl->pin_res.state_hpd_active;
		}
	} else {
		pin_state = hdmi_ctrl->pin_res.state_suspend;
	}

	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(hdmi_ctrl->pin_res.pinctrl,
				pin_state);
		if (rc)
			pr_err("%s: cannot set pins\n", __func__);
		else
			hdmi_ctrl->pdata.pin_states = cur_pin_states;
	} else {
		pr_err("%s: pinstate not found\n", __func__);
	}

	return rc;
}

static int hdmi_tx_pinctrl_init(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl;

	hdmi_ctrl = platform_get_drvdata(pdev);
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	hdmi_ctrl->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(hdmi_ctrl->pin_res.pinctrl);
	}

	hdmi_ctrl->pin_res.state_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl, "hdmi_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_active))
		pr_debug("%s: cannot get active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_hpd_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl,
							"hdmi_hpd_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_hpd_active))
		pr_debug("%s: cannot get hpd active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_cec_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl,
							"hdmi_cec_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_cec_active))
		pr_debug("%s: cannot get cec active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_ddc_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl,
							"hdmi_ddc_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_ddc_active))
		pr_debug("%s: cannot get ddc active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_suspend =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl, "hdmi_sleep");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_suspend))
		pr_debug("%s: cannot get sleep pinstate\n", __func__);

	return 0;
}

static int hdmi_tx_config_power(struct hdmi_tx_ctrl *hdmi_ctrl,
	enum hdmi_tx_power_module_type module, int config)
{
	int rc = 0;
	struct dss_module_power *power_data = NULL;
	char name[MAX_CLIENT_NAME_LEN];

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

		snprintf(name, MAX_CLIENT_NAME_LEN, "hdmi:%u", module);
		hdmi_ctrl->pdata.reg_bus_clt[module] =
			mdss_reg_bus_vote_client_create(name);
		if (IS_ERR_OR_NULL(hdmi_ctrl->pdata.reg_bus_clt[module])) {
			pr_err("reg bus client create failed\n");
			msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 0);
			rc = PTR_ERR(hdmi_ctrl->pdata.reg_bus_clt[module]);
			goto exit;
		}

		rc = msm_dss_get_clk(&hdmi_ctrl->pdev->dev,
			power_data->clk_config, power_data->num_clk);
		if (rc) {
			DEV_ERR("%s: Failed to get %s clk. Err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);

			mdss_reg_bus_vote_client_destroy(
				hdmi_ctrl->pdata.reg_bus_clt[module]);
			hdmi_ctrl->pdata.reg_bus_clt[module] = NULL;
			msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 0);
		}
	} else {
		msm_dss_put_clk(power_data->clk_config, power_data->num_clk);
		mdss_reg_bus_vote_client_destroy(
			hdmi_ctrl->pdata.reg_bus_clt[module]);
		hdmi_ctrl->pdata.reg_bus_clt[module] = NULL;

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
		if (hdmi_ctrl->panel_data.panel_info.cont_splash_enabled) {
			DEV_DBG("%s: %s already eanbled by splash\n",
				__func__, hdmi_pm_name(module));
			return 0;
		}

		rc = msm_dss_enable_vreg(power_data->vreg_config,
			power_data->num_vreg, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s vreg. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto error;
		}

		rc = hdmi_tx_pinctrl_set_state(hdmi_ctrl, module, enable);
		if (rc) {
			DEV_ERR("%s: Failed to set %s pinctrl state\n",
				__func__, hdmi_tx_pm_name(module));
			goto error;
		}

		rc = msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s gpio. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto disable_vreg;
		}
		mdss_update_reg_bus_vote(hdmi_ctrl->pdata.reg_bus_clt[module],
			VOTE_INDEX_19_MHZ);

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
		mdss_update_reg_bus_vote(hdmi_ctrl->pdata.reg_bus_clt[module],
			VOTE_INDEX_DISABLE);
		msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, 0);
		hdmi_tx_pinctrl_set_state(hdmi_ctrl, module, 0);
		msm_dss_enable_vreg(power_data->vreg_config,
			power_data->num_vreg, 0);
	}

	return rc;

disable_gpio:
	mdss_update_reg_bus_vote(hdmi_ctrl->pdata.reg_bus_clt[module],
		VOTE_INDEX_DISABLE);
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

static int hdmi_tx_audio_acr_setup(struct hdmi_tx_ctrl *hdmi_ctrl,
	bool enabled)
{
	/* Read first before writing */
	u32 acr_pck_ctrl_reg;
	u32 sample_rate_hz;
	u32 pixel_freq;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	sample_rate_hz = hdmi_ctrl->audio_data.sample_rate_hz;

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return -EINVAL;
	}

	acr_pck_ctrl_reg = DSS_REG_R(io, HDMI_ACR_PKT_CTRL);

	if (enabled) {
		struct msm_hdmi_mode_timing_info *timing =
			&hdmi_ctrl->vid_cfg.timing;
		const struct hdmi_tx_audio_acr_arry *audio_acr =
			&hdmi_tx_audio_acr_lut[0];
		const int lut_size = sizeof(hdmi_tx_audio_acr_lut)
			/ sizeof(*hdmi_tx_audio_acr_lut);
		u32 i, n, cts, layout, multiplier, aud_pck_ctrl_2_reg;

		if (timing == NULL) {
			DEV_WARN("%s: video format %d not supported\n",
				__func__, hdmi_ctrl->vid_cfg.vic);
			return -EPERM;
		}
		pixel_freq = hdmi_tx_setup_tmds_clk_rate(hdmi_ctrl);

		for (i = 0; i < lut_size;
			audio_acr = &hdmi_tx_audio_acr_lut[++i]) {
			if (audio_acr->pclk == pixel_freq)
				break;
		}
		if (i >= lut_size) {
			DEV_WARN("%s: pixel clk %d not supported\n", __func__,
				pixel_freq);
			return -EPERM;
		}

		n = audio_acr->lut[sample_rate_hz].n;
		cts = audio_acr->lut[sample_rate_hz].cts;
		layout = (MSM_HDMI_AUDIO_CHANNEL_2 ==
			hdmi_ctrl->audio_data.num_of_channels) ? 0 : 1;

		if (
		(AUDIO_SAMPLE_RATE_192KHZ == sample_rate_hz) ||
		(AUDIO_SAMPLE_RATE_176_4KHZ == sample_rate_hz)) {
			multiplier = 4;
			n >>= 2; /* divide N by 4 and use multiplier */
		} else if (
		(AUDIO_SAMPLE_RATE_96KHZ == sample_rate_hz) ||
		(AUDIO_SAMPLE_RATE_88_2KHZ == sample_rate_hz)) {
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

		if ((AUDIO_SAMPLE_RATE_48KHZ == sample_rate_hz) ||
		(AUDIO_SAMPLE_RATE_96KHZ == sample_rate_hz) ||
		(AUDIO_SAMPLE_RATE_192KHZ == sample_rate_hz)) {
			/* SELECT(3) */
			acr_pck_ctrl_reg |= 3 << 4;
			/* CTS_48 */
			cts <<= 12;

			/* CTS: need to determine how many fractional bits */
			DSS_REG_W(io, HDMI_ACR_48_0, cts);
			/* N */
			DSS_REG_W(io, HDMI_ACR_48_1, n);
		} else if (
		(AUDIO_SAMPLE_RATE_44_1KHZ == sample_rate_hz) ||
		(AUDIO_SAMPLE_RATE_88_2KHZ == sample_rate_hz) ||
		(AUDIO_SAMPLE_RATE_176_4KHZ == sample_rate_hz)) {
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

	u32 hdmi_debug_reg = 0;
	u32 channel_count = 1; /* Def to 2 channels -> Table 17 in CEA-D */
	u32 num_of_channels;
	u32 channel_allocation;
	u32 level_shift;
	u32 down_mix;
	u32 check_sum, audio_info_0_reg, audio_info_1_reg;
	u32 audio_info_ctrl_reg;
	u32 aud_pck_ctrl_2_reg;
	u32 layout;
	u32 sample_present;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	num_of_channels    = hdmi_ctrl->audio_data.num_of_channels;
	channel_allocation = hdmi_ctrl->audio_data.channel_allocation;
	level_shift        = hdmi_ctrl->audio_data.level_shift;
	down_mix           = hdmi_ctrl->audio_data.down_mix;
	sample_present     = hdmi_ctrl->audio_data.sample_present;

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
			channel_count = 2;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_4:
			channel_count = 3;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_5:
			channel_count = 4;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_6:
			channel_count = 5;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_7:
			channel_count = 6;
			break;
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

		/*
		 * Program the Sample Present into the debug register so that
		 * the HDMI transmitter core can add the sample present to
		 * Audio Sample Packet once tranmission starts.
		 */
		if (layout) {
			/* Set the Layout bit */
			hdmi_debug_reg |= BIT(4);
			/* Set the Sample Present bits */
			hdmi_debug_reg |= sample_present & 0xF;
			DSS_REG_W(io, HDMI_DEBUG, hdmi_debug_reg);
		}
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

static int hdmi_tx_get_audio_sample_rate(u32 *sample_rate_hz)
{
	int ret = 0;
	u32 rate = *sample_rate_hz;

	switch (rate) {
	case 32000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_32KHZ;
		break;
	case 44100:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_44_1KHZ;
		break;
	case 48000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_48KHZ;
		break;
	case 88200:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_88_2KHZ;
		break;
	case 96000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_96KHZ;
		break;
	case 176400:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_176_4KHZ;
		break;
	case 192000:
		*sample_rate_hz = AUDIO_SAMPLE_RATE_192KHZ;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
} /* hdmi_tx_get_audio_sample_rate */

static int hdmi_tx_audio_info_setup(struct platform_device *pdev,
	struct msm_hdmi_audio_setup_params *params)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);
	u32 is_mode_dvi;
	u32 *sample_rate_hz;

	if (!hdmi_ctrl || !params) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -ENODEV;
	}

	is_mode_dvi = hdmi_tx_is_dvi_mode(hdmi_ctrl);

	mutex_lock(&hdmi_ctrl->power_mutex);
	if (!is_mode_dvi && hdmi_tx_is_panel_on(hdmi_ctrl)) {
		mutex_unlock(&hdmi_ctrl->power_mutex);
		memcpy(&hdmi_ctrl->audio_data, params,
			sizeof(struct msm_hdmi_audio_setup_params));

		sample_rate_hz = &hdmi_ctrl->audio_data.sample_rate_hz;
		rc = hdmi_tx_get_audio_sample_rate(sample_rate_hz);
		if (rc) {
			DEV_ERR("%s: invalid sample rate = %d\n",
				__func__, hdmi_ctrl->audio_data.sample_rate_hz);
			goto exit;
		}

		rc = hdmi_tx_audio_setup(hdmi_ctrl);
		if (rc)
			DEV_ERR("%s: hdmi_tx_audio_iframe_setup failed.rc=%d\n",
				__func__, rc);
	} else {
		mutex_unlock(&hdmi_ctrl->power_mutex);
		rc = -EPERM;
	}

	if (rc)
		dev_err_ratelimited(&hdmi_ctrl->pdev->dev,
			"%s: hpd %d, ack %d, switch %d, mode %s, power %d\n",
			__func__, hdmi_ctrl->hpd_state,
			atomic_read(&hdmi_ctrl->audio_ack_pending),
			hdmi_ctrl->audio_sdev.state,
			is_mode_dvi ? "dvi" : "hdmi",
			hdmi_ctrl->panel_power_on);

exit:
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

	if (!hdmi_ctrl->audio_sdev.state)
		return -EPERM;

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
		hdmi_ctrl->ds_data.ds_max_clk = max_val;
		hdmi_ctrl->ds_data.ds_registered = true;
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

	hdmi_ctrl->ds_registered = true;

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
	hpd = hdmi_tx_is_panel_on(hdmi_ctrl);
	spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

	hdmi_ctrl->vote_hdmi_core_on = false;

	if (vote && hpd)
		hdmi_ctrl->vote_hdmi_core_on = true;

	/*
	 * if cable is not connected and audio calls this function,
	 * consider this as an error as it will result in whole
	 * audio path to fail.
	 */
	if (!hpd)
		dev_err_ratelimited(&hdmi_ctrl->pdev->dev,
			"%s: hpd %d, ack %d, switch %d, power %d\n",
			__func__, hdmi_ctrl->hpd_state,
			atomic_read(&hdmi_ctrl->audio_ack_pending),
			hdmi_ctrl->audio_sdev.state,
			hdmi_ctrl->panel_power_on);

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

	hdmi_ctrl->audio_data.sample_rate_hz = AUDIO_SAMPLE_RATE_48KHZ;
	hdmi_ctrl->audio_data.num_of_channels = MSM_HDMI_AUDIO_CHANNEL_2;

	DEV_INFO("HDMI Audio: Disabled\n");
} /* hdmi_tx_audio_off */

static int hdmi_tx_setup_tmds_clk_rate(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 rate = 0;
	struct msm_hdmi_mode_timing_info *timing = NULL;
	u32 rate_ratio;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: Bad input parameters\n", __func__);
		goto end;
	}

	timing = &hdmi_ctrl->vid_cfg.timing;
	if (!timing) {
		DEV_ERR("%s: Invalid timing info\n", __func__);
		goto end;
	}

	switch (hdmi_ctrl->vid_cfg.avi_iframe.pixel_format) {
	case MDP_Y_CBCR_H2V2:
		rate_ratio = HDMI_TX_YUV420_24BPP_PCLK_TMDS_CH_RATE_RATIO;
		break;
	case MDP_Y_CBCR_H2V1:
		rate_ratio = HDMI_TX_YUV422_24BPP_PCLK_TMDS_CH_RATE_RATIO;
		break;
	default:
		rate_ratio = HDMI_TX_RGB_24BPP_PCLK_TMDS_CH_RATE_RATIO;
		break;
	}

	rate = timing->pixel_freq / rate_ratio;

end:
	return rate;
}

int hdmi_tx_setup_scrambler(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;
	u32 rate = 0;
	u32 reg_val = 0;
	u32 tmds_clock_ratio = 0;
	bool scrambler_on = false;
	struct dss_io_data *io = NULL;
	struct msm_hdmi_mode_timing_info *timing = NULL;
	void *edid_data = NULL;
	int timeout_hsync;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: Bad input parameters\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io is not initialized\n", __func__);
		return -EINVAL;
	}

	edid_data = hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID];

	timing = &hdmi_ctrl->vid_cfg.timing;
	if (!timing) {
		DEV_ERR("%s: Invalid timing info\n", __func__);
		return -EINVAL;
	}

	/* Scrambling is supported from HDMI TX 4.0 */
	if (hdmi_ctrl->hdmi_tx_ver < HDMI_TX_SCRAMBLER_MIN_TX_VERSION) {
		DEV_DBG("%s: HDMI TX does not support scrambling\n", __func__);
		return 0;
	}

	rate = hdmi_tx_setup_tmds_clk_rate(hdmi_ctrl);

	if (rate > HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ) {
		scrambler_on = true;
		tmds_clock_ratio = 1;
	} else {
		if (hdmi_edid_get_sink_scrambler_support(edid_data))
			scrambler_on = true;
	}

	if (scrambler_on) {
		rc = hdmi_scdc_write(&hdmi_ctrl->ddc_ctrl,
			HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE,
			tmds_clock_ratio);
		if (rc) {
			DEV_ERR("%s: TMDS CLK RATIO ERR\n", __func__);
			return rc;
		}

		reg_val = DSS_REG_R(io, HDMI_CTRL);
		reg_val |= BIT(31); /* Enable Update DATAPATH_MODE */
		reg_val |= BIT(28); /* Set SCRAMBLER_EN bit */

		DSS_REG_W(io, HDMI_CTRL, reg_val);

		rc = hdmi_scdc_write(&hdmi_ctrl->ddc_ctrl,
			HDMI_TX_SCDC_SCRAMBLING_ENABLE, 0x1);
		if (!rc) {
			hdmi_ctrl->scrambler_enabled = true;
		} else {
			DEV_ERR("%s: failed to enable scrambling\n",
				__func__);
			return rc;
		}

		/*
		 * Setup hardware to periodically check for scrambler
		 * status bit on the sink. Sink should set this bit
		 * with in 200ms after scrambler is enabled.
		 */
		timeout_hsync = hdmi_utils_get_timeout_in_hysnc(
					&hdmi_ctrl->vid_cfg.timing,
					HDMI_TX_SCRAMBLER_TIMEOUT_MSEC);

		if (timeout_hsync <= 0) {
			DEV_ERR("%s: err in timeout hsync calc\n", __func__);
			timeout_hsync = HDMI_DEFAULT_TIMEOUT_HSYNC;
		}

		pr_debug("timeout for scrambling en: %d hsyncs\n",
			timeout_hsync);

		rc = hdmi_setup_ddc_timers(&hdmi_ctrl->ddc_ctrl,
			HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS, timeout_hsync);
	} else {
		hdmi_scdc_write(&hdmi_ctrl->ddc_ctrl,
			HDMI_TX_SCDC_SCRAMBLING_ENABLE, 0x0);

		hdmi_ctrl->scrambler_enabled = false;
	}

	return rc;
}

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

	DSS_REG_W(io, HDMI_USEC_REFTIMER, 0x0001001B);

	rc = hdmi_tx_video_setup(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: hdmi_tx_video_setup failed. rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl) &&
	    hdmi_tx_is_cea_format(hdmi_ctrl->vid_cfg.vic)) {
		hdmi_tx_audio_setup(hdmi_ctrl);

		if (!hdmi_tx_is_encryption_set(hdmi_ctrl) &&
			hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
			hdmi_tx_set_audio_switch_node(hdmi_ctrl, 1);
			hdmi_tx_config_avmute(hdmi_ctrl, false);
		}

		hdmi_tx_set_avi_infoframe(hdmi_ctrl);
		hdmi_tx_set_vendor_specific_infoframe(hdmi_ctrl);
		hdmi_tx_set_spd_infoframe(hdmi_ctrl);
	}

	hdmi_tx_set_mode(hdmi_ctrl, true);

	if (hdmi_tx_setup_scrambler(hdmi_ctrl))
		DEV_WARN("%s: Scrambler setup failed\n", __func__);

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

static int hdmi_tx_power_off(struct mdss_panel_data *panel_data)
{
	struct dss_io_data *io = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_panel_data(panel_data);

	mutex_lock(&hdmi_ctrl->power_mutex);
	if (!hdmi_ctrl ||
		(!panel_data->panel_info.cont_splash_enabled &&
		!hdmi_ctrl->panel_power_on)) {
		mutex_unlock(&hdmi_ctrl->power_mutex);
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	} else {
		mutex_unlock(&hdmi_ctrl->power_mutex);
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return -EINVAL;
	}

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl))
		hdmi_tx_audio_off(hdmi_ctrl);

	hdmi_cec_deconfig(hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]);

	hdmi_tx_core_off(hdmi_ctrl);

	mutex_lock(&hdmi_ctrl->power_mutex);
	hdmi_ctrl->panel_power_on = false;
	mutex_unlock(&hdmi_ctrl->power_mutex);

	mutex_lock(&hdmi_ctrl->mutex);
	if (hdmi_ctrl->hpd_off_pending) {
		hdmi_ctrl->hpd_off_pending = false;
		mutex_unlock(&hdmi_ctrl->mutex);
		if (!hdmi_ctrl->hpd_state)
			hdmi_tx_hpd_off(hdmi_ctrl);
	} else {
		mutex_unlock(&hdmi_ctrl->mutex);
	}

	if (hdmi_ctrl->hdmi_tx_hpd_done)
		hdmi_ctrl->hdmi_tx_hpd_done(
			hdmi_ctrl->downstream_data);

	DEV_INFO("%s: HDMI Core: OFF\n", __func__);

	return 0;
} /* hdmi_tx_power_off */

static int hdmi_tx_power_on(struct mdss_panel_data *panel_data)
{
	int rc = 0;
	int res_changed = RESOLUTION_UNCHANGED;
	struct dss_io_data *io = NULL;
	struct mdss_panel_info *panel_info = NULL;
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
		DEV_ERR("%s: hpd not initialized\n", __func__);
		return -EPERM;
	}

	panel_info = &panel_data->panel_info;
	hdmi_ctrl->hdcp_feature_on = hdcp_feature_on;

	res_changed = hdmi_tx_set_video_fmt(hdmi_ctrl, panel_info);

	DEV_DBG("%s: %dx%d%s\n", __func__,
		panel_info->xres, panel_info->yres,
		panel_info->cont_splash_enabled ? " (handoff underway)" : "");

	if (hdmi_ctrl->pdata.cont_splash_enabled) {
		hdmi_ctrl->pdata.cont_splash_enabled = false;
		panel_data->panel_info.cont_splash_enabled = false;

		if (res_changed == RESOLUTION_UNCHANGED) {
			mutex_lock(&hdmi_ctrl->power_mutex);
			hdmi_ctrl->panel_power_on = true;
			mutex_unlock(&hdmi_ctrl->power_mutex);

			hdmi_cec_config(
				hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]);

			hdmi_tx_set_vendor_specific_infoframe(hdmi_ctrl);
			hdmi_tx_set_spd_infoframe(hdmi_ctrl);

			if (!hdmi_tx_is_hdcp_enabled(hdmi_ctrl))
				hdmi_tx_set_audio_switch_node(hdmi_ctrl, 1);

			goto end;
		}
	}

	rc = hdmi_tx_core_on(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: hdmi_msm_core_on failed\n", __func__);
		return rc;
	}

	hdmi_cec_config(hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]);

	if (hdmi_ctrl->hpd_state) {
		rc = hdmi_tx_start(hdmi_ctrl);
		if (rc) {
			DEV_ERR("%s: hdmi_tx_start failed. rc=%d\n",
				__func__, rc);
			hdmi_tx_power_off(panel_data);
			return rc;
		}
	}

	mutex_lock(&hdmi_ctrl->power_mutex);
	hdmi_ctrl->panel_power_on = true;
	mutex_unlock(&hdmi_ctrl->power_mutex);
end:
	dss_reg_dump(io->base, io->len, "HDMI-ON: ", REG_DUMP);

	DEV_DBG("%s: Tx: %s (%s mode)\n", __func__,
		hdmi_tx_is_controller_on(hdmi_ctrl) ? "ON" : "OFF" ,
		hdmi_tx_is_dvi_mode(hdmi_ctrl) ? "DVI" : "HDMI");

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
	if (!hdmi_ctrl->panel_suspend)
		flush_work(&hdmi_ctrl->hpd_int_work);

	/* Turn off HPD interrupts */
	DSS_REG_W(io, HDMI_HPD_INT_CTRL, 0);

	hdmi_ctrl->mdss_util->disable_irq(&hdmi_tx_hw);

	hdmi_tx_set_mode(hdmi_ctrl, false);

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, 0);
	if (rc)
		DEV_INFO("%s: Failed to disable hpd power. Error=%d\n",
			__func__, rc);

	spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
	hdmi_ctrl->hpd_state = false;
	spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

	hdmi_ctrl->hpd_initialized = false;

	if (!completion_done(&hdmi_ctrl->hpd_off_done))
		complete_all(&hdmi_ctrl->hpd_off_done);

	DEV_DBG("%s: HPD is now OFF\n", __func__);
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

		if (!hdmi_ctrl->panel_data.panel_info.cont_splash_enabled) {
			hdmi_tx_set_mode(hdmi_ctrl, false);
			hdmi_tx_phy_reset(hdmi_ctrl);
			hdmi_tx_set_mode(hdmi_ctrl, true);
		}

		DSS_REG_W(io, HDMI_USEC_REFTIMER, 0x0001001B);

		hdmi_ctrl->mdss_util->enable_irq(&hdmi_tx_hw);

		hdmi_ctrl->hpd_initialized = true;

		DEV_INFO("%s: HDMI HW version = 0x%x\n", __func__,
			DSS_REG_R_ND(&hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO],
				HDMI_VERSION));

		/* set timeout to 4.1ms (max) for hardware debounce */
		reg_val = DSS_REG_R(io, HDMI_HPD_CTRL) | 0x1FFF;

		/* Turn on HPD HW circuit */
		DSS_REG_W(io, HDMI_HPD_CTRL, reg_val | BIT(28));

		atomic_set(&hdmi_ctrl->audio_ack_pending, 0);

		hdmi_tx_hpd_polarity_setup(hdmi_ctrl, HPD_CONNECT_POLARITY);
		DEV_DBG("%s: HPD is now ON\n", __func__);
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
		if (hdmi_ctrl->hpd_off_pending) {
			u32 timeout;

			reinit_completion(&hdmi_ctrl->hpd_off_done);
			timeout = wait_for_completion_timeout(
				&hdmi_ctrl->hpd_off_done, HZ);
			if (!timeout) {
				hdmi_ctrl->hpd_off_pending = false;
				DEV_ERR("%s: hpd off still pending\n",
					__func__);
				return 0;
			}
		}

		rc = hdmi_tx_hpd_on(hdmi_ctrl);
	} else {
		mutex_lock(&hdmi_ctrl->power_mutex);
		if (!hdmi_ctrl->panel_power_on &&
			!hdmi_ctrl->hpd_off_pending) {
			mutex_unlock(&hdmi_ctrl->power_mutex);
			hdmi_tx_hpd_off(hdmi_ctrl);
		} else {
			mutex_unlock(&hdmi_ctrl->power_mutex);
			hdmi_ctrl->hpd_off_pending = true;
		}
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
	u32 hpd_current_state;
	u32 reg_val = 0;

	if (!hdmi_ctrl) {
		DEV_WARN("%s: invalid input data, ISR ignored\n", __func__);
		goto end;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_WARN("%s: core io not initialized, ISR ignored\n",
			__func__);
		goto end;
	}

	if (DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(0)) {
		spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
		hpd_current_state = hdmi_ctrl->hpd_state;
		hdmi_ctrl->hpd_state =
			(DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(1)) >> 1;
		spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

		/*
		 * check if this is a spurious interrupt, if yes, reset
		 * interrupts and return
		 */
		if (hpd_current_state == hdmi_ctrl->hpd_state) {
			DEV_DBG("%s: spurious interrupt %d\n", __func__,
				hpd_current_state);

			/* enable interrupts */
			reg_val |= BIT(2);

			/* set polarity, reverse of current state */
			reg_val |= (~hpd_current_state << 1) & BIT(1);

			/* ack interrupt */
			reg_val |= BIT(0);

			DSS_REG_W(io, HDMI_HPD_INT_CTRL, reg_val);
			goto end;
		}

		/*
		 * Ack the current hpd interrupt and stop listening to
		 * new hpd interrupt.
		 */
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(0));

		/*
		 * If suspend has already triggered, don't start the hpd work
		 * to avoid a possible deadlock during suspend where hpd off
		 * waits for hpd interrupt to finish. Suspend thread will
		 * eventually reset the HPD module.
		 */
		if (hdmi_ctrl->panel_suspend)
			hdmi_ctrl->hpd_state = 0;
		else
			queue_work(hdmi_ctrl->workq, &hdmi_ctrl->hpd_int_work);
	}

	if (hdmi_ddc_isr(&hdmi_ctrl->ddc_ctrl,
		hdmi_ctrl->hdmi_tx_ver))
		DEV_ERR("%s: hdmi_ddc_isr failed\n", __func__);

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC])
		if (hdmi_cec_isr(hdmi_ctrl->feature_data[HDMI_TX_FEAT_CEC]))
			DEV_ERR("%s: hdmi_cec_isr failed\n", __func__);

	if (hdmi_ctrl->hdcp_ops && hdmi_ctrl->hdcp_data) {
		if (hdmi_ctrl->hdcp_ops->hdmi_hdcp_isr) {
			if (hdmi_ctrl->hdcp_ops->hdmi_hdcp_isr(
				hdmi_ctrl->hdcp_data))
				DEV_ERR("%s: hdmi_hdcp_isr failed\n",
					 __func__);
		}
	}
end:
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

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP2P2]) {
		hdmi_hdcp2p2_deinit(
				hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP2P2]);
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_HDCP2P2] = NULL;
	}

	hdmi_ctrl->hdcp_ops = NULL;
	hdmi_ctrl->hdcp_data = NULL;

	if (hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]) {
		hdmi_edid_deinit(hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID]);
		hdmi_ctrl->feature_data[HDMI_TX_FEAT_EDID] = NULL;
	}

	switch_dev_unregister(&hdmi_ctrl->audio_sdev);
	switch_dev_unregister(&hdmi_ctrl->sdev);
	if (hdmi_ctrl->workq)
		destroy_workqueue(hdmi_ctrl->workq);
	mutex_destroy(&hdmi_ctrl->lut_lock);
	mutex_destroy(&hdmi_ctrl->power_mutex);
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

	mutex_init(&hdmi_ctrl->mutex);
	mutex_init(&hdmi_ctrl->lut_lock);
	mutex_init(&hdmi_ctrl->cable_notify_mutex);
	mutex_init(&hdmi_ctrl->power_mutex);

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
	init_completion(&hdmi_ctrl->hpd_int_done);
	init_completion(&hdmi_ctrl->hpd_off_done);

	INIT_WORK(&hdmi_ctrl->hpd_int_work, hdmi_tx_hpd_int_work);
	INIT_WORK(&hdmi_ctrl->cable_notify_work, hdmi_tx_cable_notify_work);
	INIT_DELAYED_WORK(&hdmi_ctrl->hdcp_cb_work, hdmi_tx_hdcp_cb_work);

	spin_lock_init(&hdmi_ctrl->hpd_state_lock);

	hdmi_ctrl->audio_data.sample_rate_hz = AUDIO_SAMPLE_RATE_48KHZ;
	hdmi_ctrl->audio_data.num_of_channels = MSM_HDMI_AUDIO_CHANNEL_2;

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

static int hdmi_tx_start_hdcp(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (hdmi_ctrl->panel_data.panel_info.cont_splash_enabled ||
		!hdmi_tx_is_hdcp_enabled(hdmi_ctrl))
		return 0;

	if (hdmi_tx_is_encryption_set(hdmi_ctrl))
		hdmi_tx_config_avmute(hdmi_ctrl, true);

	if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, true)) {
		DEV_ERR("%s: Failed to enable ddc power\n", __func__);
		return -ENODEV;
	}

	rc = hdmi_ctrl->hdcp_ops->hdmi_hdcp_authenticate(hdmi_ctrl->hdcp_data);
	if (rc)
		DEV_ERR("%s: hdcp auth failed. rc=%d\n", __func__, rc);

	return rc;
}

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
		rc = hdmi_tx_init_features(hdmi_ctrl, arg);
		if (rc) {
			DEV_ERR("%s: init_features failed.rc=%d\n",
					__func__, rc);
			hdmi_tx_sysfs_remove(hdmi_ctrl);
			return rc;
		}

		if (hdmi_ctrl->pdata.primary) {
			reinit_completion(&hdmi_ctrl->hpd_int_done);
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
		if (new_vic != hdmi_ctrl->vid_cfg.vic)
			rc = 1;
		else
			DEV_DBG("%s: no res change.\n", __func__);
		break;

	case MDSS_EVENT_RESUME:
		if (hdmi_ctrl->hpd_feature_on) {
			reinit_completion(&hdmi_ctrl->hpd_int_done);

			rc = hdmi_tx_hpd_on(hdmi_ctrl);
			if (rc)
				DEV_ERR("%s: hdmi_tx_hpd_on failed. rc=%d\n",
					__func__, rc);
		}
		break;

	case MDSS_EVENT_RESET:
		if (hdmi_ctrl->hpd_initialized) {
			hdmi_tx_set_mode(hdmi_ctrl, false);
			hdmi_tx_phy_reset(hdmi_ctrl);
			hdmi_tx_set_mode(hdmi_ctrl, true);
		}

		if (hdmi_ctrl->panel_suspend) {
			u32 timeout;
			hdmi_ctrl->panel_suspend = false;

			timeout = wait_for_completion_timeout(
				&hdmi_ctrl->hpd_int_done, HZ/10);
			if (!timeout && !hdmi_ctrl->hpd_state) {
				DEV_INFO("%s: cable removed during suspend\n",
					__func__);
				hdmi_tx_send_cable_notification(hdmi_ctrl,
					false);
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
		rc = hdmi_tx_start_hdcp(hdmi_ctrl);
		if (rc)
			DEV_ERR("%s: hdcp start failed rc=%d\n", __func__, rc);

		hdmi_ctrl->timing_gen_on = true;
		break;

	case MDSS_EVENT_SUSPEND:
		mutex_lock(&hdmi_ctrl->power_mutex);
		if (!hdmi_ctrl->panel_power_on &&
			!hdmi_ctrl->hpd_off_pending && !hdmi_ctrl->hpd_state) {
			mutex_unlock(&hdmi_ctrl->power_mutex);
			if (hdmi_ctrl->hpd_feature_on)
				hdmi_tx_hpd_off(hdmi_ctrl);

			hdmi_ctrl->panel_suspend = false;
		} else {
			hdmi_ctrl->hpd_state = 0;
			mutex_unlock(&hdmi_ctrl->power_mutex);
			hdmi_ctrl->hpd_off_pending = true;
			hdmi_ctrl->panel_suspend = true;
		}
		break;

	case MDSS_EVENT_BLANK:
		if (hdmi_tx_is_hdcp_enabled(hdmi_ctrl)) {
			DEV_DBG("%s: Turning off HDCP\n", __func__);
			hdmi_ctrl->hdcp_ops->hdmi_hdcp_off(
				hdmi_ctrl->hdcp_data);

			hdmi_ctrl->hdcp_ops = NULL;

			rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM,
				false);
			if (rc)
				DEV_ERR("%s: Failed to disable ddc power\n",
					__func__);
		}
		break;

	case MDSS_EVENT_PANEL_OFF:
		mutex_lock(&hdmi_ctrl->power_mutex);
		if (hdmi_ctrl->panel_power_on) {
			mutex_unlock(&hdmi_ctrl->power_mutex);
			hdmi_tx_config_avmute(hdmi_ctrl, 1);
			rc = hdmi_tx_power_off(panel_data);
			if (rc)
				DEV_ERR("%s: hdmi_tx_power_off failed.rc=%d\n",
					__func__, rc);
		} else {
			mutex_unlock(&hdmi_ctrl->power_mutex);
			DEV_DBG("%s: hdmi is already powered off\n", __func__);
		}

		hdmi_ctrl->timing_gen_on = false;
		break;

	case MDSS_EVENT_CLOSE:
		if (panel_data->panel_info.cont_splash_enabled) {
			hdmi_tx_power_off(panel_data);
			panel_data->panel_info.cont_splash_enabled = false;
		} else {
			if (hdmi_ctrl->hpd_feature_on &&
				hdmi_ctrl->hpd_initialized &&
				!hdmi_ctrl->hpd_state)
				hdmi_tx_hpd_polarity_setup(hdmi_ctrl,
					HPD_CONNECT_POLARITY);
		}
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

	if (!hdmi_ctrl->pdata.primary)
		hdmi_ctrl->vid_cfg.vic = DEFAULT_VIDEO_RESOLUTION;

	rc = hdmi_tx_init_panel_info(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: hdmi_init_panel_info failed\n", __func__);
		return rc;
	}

	rc = mdss_register_panel(hdmi_ctrl->pdev, &hdmi_ctrl->panel_data);
	if (rc) {
		DEV_ERR("%s: FAILED: to register HDMI panel\n", __func__);
		return rc;
	}

	rc = hdmi_ctrl->mdss_util->register_irq(&hdmi_tx_hw);
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
	for (i = HDMI_TX_MAX_IO - 1; i >= 0; i--) {
		if (hdmi_ctrl->pdata.io[i].base)
			msm_dss_iounmap(&hdmi_ctrl->pdata.io[i]);
	}
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

	hdmi_tx_pinctrl_init(hdmi_ctrl->pdev);

	/* IO */
	for (i = 0; i < HDMI_TX_MAX_IO; i++) {
		rc = msm_dss_ioremap_byname(hdmi_ctrl->pdev, &pdata->io[i],
			hdmi_tx_io_name(i));
		if (rc) {
			DEV_DBG("%s: '%s' remap failed or not available\n",
				__func__, hdmi_tx_io_name(i));
		}
		DEV_INFO("%s: '%s': start = 0x%p, len=0x%x\n", __func__,
			hdmi_tx_io_name(i), pdata->io[i].base,
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
		return -EINVAL;
	}

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_tx_pm_name(module_type));

	switch (module_type) {
	case HDMI_TX_HPD_PM:
		mp->num_clk = 4;
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

		snprintf(mp->clk_config[3].clk_name, 32, "%s", "alt_iface_clk");
		mp->clk_config[3].type = DSS_CLK_AHB;
		mp->clk_config[3].rate = 0;
		break;

	case HDMI_TX_CORE_PM:
		mp->num_clk = 1;
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

	if (!dev || !mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
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

	dt_vreg_total = of_property_count_strings(of_node, "qcom,supply-names");
	if (dt_vreg_total < 0) {
		DEV_ERR("%s: vreg not found. rc=%d\n", __func__,
			dt_vreg_total);
		rc = dt_vreg_total;
		goto error;
	}

	/* count how many vreg for particular hdmi module */
	for (i = 0; i < dt_vreg_total; i++) {
		const char *st = NULL;
		rc = of_property_read_string_index(of_node,
			"qcom,supply-names", i, &st);
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
		rc = of_property_read_string_index(of_node,
			"qcom,supply-names", i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}
		snprintf(mp->vreg_config[j].vreg_name, 32, "%s", st);

		/* vreg-min-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,min-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' min volt. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].min_voltage = val_array[i];

		/* vreg-max-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,max-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' max volt. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].max_voltage = val_array[i];

		/* vreg-op-mode */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,enable-load", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' enable load. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].enable_load = val_array[i];

		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,disable-load", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' disable load. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].disable_load = val_array[i];

		DEV_DBG("%s: %s min=%d, max=%d, enable=%d disable=%d\n",
			__func__,
			mp->vreg_config[j].vreg_name,
			mp->vreg_config[j].min_voltage,
			mp->vreg_config[j].max_voltage,
			mp->vreg_config[j].enable_load,
			mp->vreg_config[j].disable_load);

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
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

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

	if (!hdmi_ctrl->pdata.primary)
		hdmi_ctrl->pdata.primary = of_property_read_bool(
			pdev->dev.of_node, "qcom,primary_panel");

	pdata->cond_power_on = of_property_read_bool(pdev->dev.of_node,
		"qcom,conditional-power-on");

	if (!pdata->cont_splash_enabled)
		pdata->cont_splash_enabled =
			hdmi_ctrl->mdss_util->panel_intf_status(DISPLAY_2,
			MDSS_PANEL_INTF_HDMI) ? true : false;

	pdata->pluggable = of_property_read_bool(pdev->dev.of_node,
		"qcom,pluggable");

	return rc;

error:
	hdmi_tx_put_dt_data(&pdev->dev, pdata);
	return rc;
} /* hdmi_tx_get_dt_data */

static void hdmi_tx_audio_tear_down(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io;
	u32 audio_pkt_ctrl;
	u32 audio_eng_cfg;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	audio_pkt_ctrl = DSS_REG_R(io, HDMI_AUDIO_PKT_CTRL);
	audio_eng_cfg  = DSS_REG_R(io, HDMI_AUDIO_CFG);

	if ((audio_pkt_ctrl & BIT(0)) || (audio_eng_cfg & BIT(0))) {
		u32 lpa_dma, i = 0;

		void __iomem *lpa_base = ioremap(LPASS_LPAIF_RDDMA_CTL0, 0xFF);

		lpa_dma = readl_relaxed(lpa_base + LPASS_LPAIF_RDDMA_PER_CNT0);

		/* Disable audio packet transmission */
		DSS_REG_W(io, HDMI_AUDIO_PKT_CTRL,
			DSS_REG_R(io, HDMI_AUDIO_PKT_CTRL) & ~BIT(0));

		/* Wait for LPA DMA Engine to be idle */
		while (i < LPA_DMA_IDLE_MAX) {
			u32 val;

			/*
			 * sleep for minimum HW recommended time
			 * for HW status to update.
			 */
			msleep(20);

			val = readl_relaxed(lpa_base +
				LPASS_LPAIF_RDDMA_PER_CNT0);
			if (val == lpa_dma)
				break;

			lpa_dma = val;
			i++;
		}

		DEV_DBG("%s: LPA DMA idle after %d ms\n", __func__, i * 20);

		/* Disable audio engine */
		DSS_REG_W(io, HDMI_AUDIO_CFG,
			DSS_REG_R(io, HDMI_AUDIO_CFG) & ~BIT(0));

		/* Disable LPA DMA Engine */
		writel_relaxed(readl_relaxed(lpa_base) & ~BIT(0), lpa_base);

		iounmap(lpa_base);
	}
}

static int hdmi_tx_probe(struct platform_device *pdev)
{
	int rc = 0, i;
	struct device_node *of_node = pdev->dev.of_node;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct mdss_panel_cfg *pan_cfg = NULL;
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
	hdmi_ctrl->enc_lvl = HDCP_STATE_AUTH_ENC_NONE;

	pan_cfg = mdss_panel_intf_type(MDSS_PANEL_INTF_HDMI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (pan_cfg) {
		int vic;

		if (kstrtoint(pan_cfg->arg_cfg, 10, &vic) ||
			vic <= HDMI_VFRMT_UNKNOWN || vic >= HDMI_VFRMT_MAX)
			vic = DEFAULT_HDMI_PRIMARY_RESOLUTION;

		hdmi_ctrl->pdata.primary = true;
		hdmi_ctrl->vid_cfg.vic = vic;
		hdmi_ctrl->panel_data.panel_info.is_prim_panel = true;
	}

	hdmi_ctrl->mdss_util = mdss_get_util_intf();
	if (hdmi_ctrl->mdss_util == NULL) {
		pr_err("Failed to get mdss utility functions\n");
		rc = -ENODEV;
		goto failed_res_init;
	}

	hdmi_tx_hw.irq_info = mdss_intr_line();
	if (hdmi_tx_hw.irq_info == NULL) {
		pr_err("Failed to get mdss irq information\n");
		return -ENODEV;
	}

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

	rc = hdmi_tx_get_version(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: hdmi_tx_get_version. rc=%d\n",
			__func__, rc);
		goto failed_reg_panel;
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

	if (mdss_debug_register_io("hdmi",
		&hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO], NULL))
		DEV_WARN("%s: hdmi_tx debugfs register failed\n", __func__);

	if (hdmi_ctrl->panel_data.panel_info.cont_splash_enabled) {
		for (i = 0; i < HDMI_TX_MAX_PM; i++) {
			msm_dss_enable_vreg(
				hdmi_ctrl->pdata.power_data[i].vreg_config,
				hdmi_ctrl->pdata.power_data[i].num_vreg, 1);

			msm_dss_enable_gpio(
				hdmi_ctrl->pdata.power_data[i].gpio_config,
				hdmi_ctrl->pdata.power_data[i].num_gpio, 1);

			msm_dss_enable_clk(
				hdmi_ctrl->pdata.power_data[i].clk_config,
				hdmi_ctrl->pdata.power_data[i].num_clk, 1);
		}

		hdmi_tx_audio_tear_down(hdmi_ctrl);
	}

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

static int hdmi_tx_remove(struct platform_device *pdev)
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
