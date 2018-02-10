/* Copyright (c) 2010-2016, 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/io.h>
#include <linux/delay.h>
#include "mdss_hdmi_util.h"

#define RESOLUTION_NAME_STR_LEN 30
#define HDMI_SEC_TO_MS 1000
#define HDMI_MS_TO_US 1000
#define HDMI_SEC_TO_US (HDMI_SEC_TO_MS * HDMI_MS_TO_US)
#define HDMI_KHZ_TO_HZ 1000
#define HDMI_BUSY_WAIT_DELAY_US 100

#define HDMI_SCDC_UNKNOWN_REGISTER        "Unknown register"

static char res_buf[RESOLUTION_NAME_STR_LEN];

enum trigger_mode {
	TRIGGER_WRITE,
	TRIGGER_READ
};

int hdmi_utils_get_timeout_in_hysnc(struct msm_hdmi_mode_timing_info *timing,
	u32 timeout_ms)
{
	u32 fps, v_total;
	u32 time_taken_by_one_line_us, lines_needed_for_given_time;

	if (!timing || !timeout_ms) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	fps = timing->refresh_rate / HDMI_KHZ_TO_HZ;
	v_total = hdmi_tx_get_v_total(timing);

	/*
	 * pixel clock  = h_total * v_total * fps
	 * 1 sec = pixel clock number of pixels are transmitted.
	 * time taken by one line (h_total) = 1 / (v_total * fps).
	 */
	time_taken_by_one_line_us = HDMI_SEC_TO_US / (v_total * fps);
	lines_needed_for_given_time = (timeout_ms * HDMI_MS_TO_US) /
		time_taken_by_one_line_us;

	return lines_needed_for_given_time;
}

static int hdmi_ddc_clear_irq(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	char *what)
{
	u32 ddc_int_ctrl, ddc_status, in_use, timeout;
	u32 sw_done_mask = BIT(2);
	u32 sw_done_ack  = BIT(1);
	u32 in_use_by_sw = BIT(0);
	u32 in_use_by_hw = BIT(1);

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	/* clear and enable interrutps */
	ddc_int_ctrl = sw_done_mask | sw_done_ack;

	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL, ddc_int_ctrl);

	/* wait until DDC HW is free */
	timeout = 100;
	do {
		ddc_status = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_HW_STATUS);
		in_use = ddc_status & (in_use_by_sw | in_use_by_hw);
		if (in_use) {
			pr_debug("ddc is in use by %s, timeout(%d)\n",
				ddc_status & in_use_by_sw ? "sw" : "hw",
				timeout);
			udelay(100);
		}
	} while (in_use && --timeout);

	if (!timeout) {
		pr_err("%s: timedout\n", what);
		return -ETIMEDOUT;
	}

	return 0;
}

static void hdmi_scrambler_ddc_reset(struct hdmi_tx_ddc_ctrl *ctrl)
{
	u32 reg_val;

	if (!ctrl) {
		pr_err("Invalid parameters\n");
		return;
	}

	/* clear ack and disable interrupts */
	reg_val = BIT(14) | BIT(9) | BIT(5) | BIT(1);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL2, reg_val);

	/* Reset DDC timers */
	reg_val = BIT(0) | DSS_REG_R(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_CTRL);
	DSS_REG_W(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_CTRL, reg_val);

	reg_val = DSS_REG_R(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_CTRL);
	reg_val &= ~BIT(0);
	DSS_REG_W(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_CTRL, reg_val);
}

void hdmi_scrambler_ddc_disable(struct hdmi_tx_ddc_ctrl *ctrl)
{
	u32 reg_val;

	if (!ctrl) {
		pr_err("Invalid parameters\n");
		return;
	}

	hdmi_scrambler_ddc_reset(ctrl);

	/* Disable HW DDC access to RxStatus register */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(8) | BIT(9));

	DSS_REG_W(ctrl->io, HDMI_HW_DDC_CTRL, reg_val);
}

static int hdmi_scrambler_ddc_check_status(struct hdmi_tx_ddc_ctrl *ctrl)
{
	int rc = 0;
	u32 reg_val;

	if (!ctrl) {
		pr_err("invalid ddc ctrl\n");
		return -EINVAL;
	}

	/* check for errors and clear status */
	reg_val = DSS_REG_R(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_STATUS);

	if (reg_val & BIT(4)) {
		pr_err("ddc aborted\n");
		reg_val |= BIT(5);
		rc = -ECONNABORTED;
	}

	if (reg_val & BIT(8)) {
		pr_err("timed out\n");
		reg_val |= BIT(9);
		rc = -ETIMEDOUT;
	}

	if (reg_val & BIT(12)) {
		pr_err("NACK0\n");
		reg_val |= BIT(13);
		rc = -EIO;
	}

	if (reg_val & BIT(14)) {
		pr_err("NACK1\n");
		reg_val |= BIT(15);
		rc = -EIO;
	}

	DSS_REG_W(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_STATUS, reg_val);

	return rc;
}

static int hdmi_scrambler_status_timer_setup(struct hdmi_tx_ddc_ctrl *ctrl,
		u32 timeout_hsync)
{
	u32 reg_val;
	int rc;
	struct mdss_io_data *io = NULL;

	if (!ctrl || !ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	io = ctrl->io;

	hdmi_ddc_clear_irq(ctrl, "scrambler");

	DSS_REG_W(io, HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL, timeout_hsync);
	DSS_REG_W(io, HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL2, timeout_hsync);

	reg_val = DSS_REG_R(io, HDMI_DDC_INT_CTRL5);
	reg_val |= BIT(10);
	DSS_REG_W(io, HDMI_DDC_INT_CTRL5, reg_val);

	reg_val = DSS_REG_R(io, HDMI_DDC_INT_CTRL2);
	/* Trigger interrupt if scrambler status is 0 or DDC failure */
	reg_val |= BIT(10);
	reg_val &= ~(BIT(15) | BIT(16));
	reg_val |= BIT(16);
	DSS_REG_W(io, HDMI_DDC_INT_CTRL2, reg_val);

	/* Enable DDC access */
	reg_val = DSS_REG_R(io, HDMI_HW_DDC_CTRL);

	reg_val &= ~(BIT(8) | BIT(9));
	reg_val |= BIT(8);
	DSS_REG_W(io, HDMI_HW_DDC_CTRL, reg_val);

	/* WAIT for 200ms as per HDMI 2.0 standard for sink to respond */
	msleep(200);

	/* clear the scrambler status */
	rc = hdmi_scrambler_ddc_check_status(ctrl);
	if (rc)
		pr_err("scrambling ddc error %d\n", rc);

	hdmi_scrambler_ddc_disable(ctrl);

	return rc;
}

static inline char *hdmi_scdc_reg2string(u32 type)
{
	switch (type) {
	case HDMI_TX_SCDC_SCRAMBLING_STATUS:
		return "HDMI_TX_SCDC_SCRAMBLING_STATUS";
	case HDMI_TX_SCDC_SCRAMBLING_ENABLE:
		return "HDMI_TX_SCDC_SCRAMBLING_ENABLE";
	case HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE:
		return "HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE";
	case HDMI_TX_SCDC_CLOCK_DET_STATUS:
		return "HDMI_TX_SCDC_CLOCK_DET_STATUS";
	case HDMI_TX_SCDC_CH0_LOCK_STATUS:
		return "HDMI_TX_SCDC_CH0_LOCK_STATUS";
	case HDMI_TX_SCDC_CH1_LOCK_STATUS:
		return "HDMI_TX_SCDC_CH1_LOCK_STATUS";
	case HDMI_TX_SCDC_CH2_LOCK_STATUS:
		return "HDMI_TX_SCDC_CH2_LOCK_STATUS";
	case HDMI_TX_SCDC_CH0_ERROR_COUNT:
		return "HDMI_TX_SCDC_CH0_ERROR_COUNT";
	case HDMI_TX_SCDC_CH1_ERROR_COUNT:
		return "HDMI_TX_SCDC_CH1_ERROR_COUNT";
	case HDMI_TX_SCDC_CH2_ERROR_COUNT:
		return "HDMI_TX_SCDC_CH2_ERROR_COUNT";
	case HDMI_TX_SCDC_READ_ENABLE:
		return"HDMI_TX_SCDC_READ_ENABLE";
	default:
		return HDMI_SCDC_UNKNOWN_REGISTER;
	}
}

static struct msm_hdmi_mode_timing_info hdmi_resv_timings[
		RESERVE_VFRMT_END - HDMI_VFRMT_RESERVE1 + 1];

static int hdmi_get_resv_timing_info(
	struct msm_hdmi_mode_timing_info *mode, int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_resv_timings); i++) {
		struct msm_hdmi_mode_timing_info *info = &hdmi_resv_timings[i];

		if (info->video_format == id) {
			*mode = *info;
			return 0;
		}
	}

	return -EINVAL;
}

int hdmi_set_resv_timing_info(struct msm_hdmi_mode_timing_info *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_resv_timings); i++) {
		struct msm_hdmi_mode_timing_info *info = &hdmi_resv_timings[i];

		if (info->video_format == 0) {
			*info = *mode;
			info->video_format = HDMI_VFRMT_RESERVE1 + i;
			return info->video_format;
		}
	}

	return -ENOMEM;
}

bool hdmi_is_valid_resv_timing(int mode)
{
	struct msm_hdmi_mode_timing_info *info;

	if (mode < HDMI_VFRMT_RESERVE1 || mode > RESERVE_VFRMT_END) {
		pr_err("invalid mode %d\n", mode);
		return false;
	}

	info = &hdmi_resv_timings[mode - HDMI_VFRMT_RESERVE1];

	return info->video_format >= HDMI_VFRMT_RESERVE1 &&
		info->video_format <= RESERVE_VFRMT_END;
}

void hdmi_reset_resv_timing_info(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_resv_timings); i++) {
		struct msm_hdmi_mode_timing_info *info = &hdmi_resv_timings[i];

		info->video_format = 0;
	}
}

int msm_hdmi_get_timing_info(
	struct msm_hdmi_mode_timing_info *mode, int id)
{
	int ret = 0;

	switch (id) {
	case HDMI_VFRMT_640x480p60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_640x480p60_4_3);
		break;
	case HDMI_VFRMT_720x480p60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_720x480p60_4_3);
		break;
	case HDMI_VFRMT_720x480p60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_720x480p60_16_9);
		break;
	case HDMI_VFRMT_1280x720p60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1280x720p60_16_9);
		break;
	case HDMI_VFRMT_1920x1080i60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1920x1080i60_16_9);
		break;
	case HDMI_VFRMT_1440x480i60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1440x480i60_4_3);
		break;
	case HDMI_VFRMT_1440x480i60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1440x480i60_16_9);
		break;
	case HDMI_VFRMT_1920x1080p60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1920x1080p60_16_9);
		break;
	case HDMI_VFRMT_720x576p50_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_720x576p50_4_3);
		break;
	case HDMI_VFRMT_720x576p50_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_720x576p50_16_9);
		break;
	case HDMI_VFRMT_1280x720p50_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1280x720p50_16_9);
		break;
	case HDMI_VFRMT_1440x576i50_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1440x576i50_4_3);
		break;
	case HDMI_VFRMT_1440x576i50_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1440x576i50_16_9);
		break;
	case HDMI_VFRMT_1920x1080p50_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1920x1080p50_16_9);
		break;
	case HDMI_VFRMT_1920x1080p24_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1920x1080p24_16_9);
		break;
	case HDMI_VFRMT_1920x1080p25_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1920x1080p25_16_9);
		break;
	case HDMI_VFRMT_1920x1080p30_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1920x1080p30_16_9);
		break;
	case HDMI_EVFRMT_3840x2160p30_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_EVFRMT_3840x2160p30_16_9);
		break;
	case HDMI_EVFRMT_3840x2160p25_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_EVFRMT_3840x2160p25_16_9);
		break;
	case HDMI_EVFRMT_3840x2160p24_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_EVFRMT_3840x2160p24_16_9);
		break;
	case HDMI_EVFRMT_4096x2160p24_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_EVFRMT_4096x2160p24_16_9);
		break;
	case HDMI_VFRMT_1024x768p60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1024x768p60_4_3);
		break;
	case HDMI_VFRMT_1280x1024p60_5_4:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1280x1024p60_5_4);
		break;
	case HDMI_VFRMT_2560x1600p60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_2560x1600p60_16_9);
		break;
	case HDMI_VFRMT_800x600p60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_800x600p60_4_3);
		break;
	case HDMI_VFRMT_848x480p60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_848x480p60_16_9);
		break;
	case HDMI_VFRMT_1280x960p60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1280x960p60_4_3);
		break;
	case HDMI_VFRMT_1360x768p60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1360x768p60_16_9);
		break;
	case HDMI_VFRMT_1440x900p60_16_10:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1440x900p60_16_10);
		break;
	case HDMI_VFRMT_1400x1050p60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1400x1050p60_4_3);
		break;
	case HDMI_VFRMT_1680x1050p60_16_10:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1680x1050p60_16_10);
		break;
	case HDMI_VFRMT_1600x1200p60_4_3:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1600x1200p60_4_3);
		break;
	case HDMI_VFRMT_1920x1200p60_16_10:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1920x1200p60_16_10);
		break;
	case HDMI_VFRMT_1366x768p60_16_10:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1366x768p60_16_10);
		break;
	case HDMI_VFRMT_1280x800p60_16_10:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_1280x800p60_16_10);
		break;
	case HDMI_VFRMT_3840x2160p24_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p24_16_9);
		break;
	case HDMI_VFRMT_3840x2160p25_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p25_16_9);
		break;
	case HDMI_VFRMT_3840x2160p30_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p30_16_9);
		break;
	case HDMI_VFRMT_3840x2160p50_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p50_16_9);
		break;
	case HDMI_VFRMT_3840x2160p60_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p60_16_9);
		break;
	case HDMI_VFRMT_4096x2160p24_256_135:
		MSM_HDMI_MODES_GET_DETAILS(mode,
					HDMI_VFRMT_4096x2160p24_256_135);
		break;
	case HDMI_VFRMT_4096x2160p25_256_135:
		MSM_HDMI_MODES_GET_DETAILS(mode,
					HDMI_VFRMT_4096x2160p25_256_135);
		break;
	case HDMI_VFRMT_4096x2160p30_256_135:
		MSM_HDMI_MODES_GET_DETAILS(mode,
					HDMI_VFRMT_4096x2160p30_256_135);
		break;
	case HDMI_VFRMT_4096x2160p50_256_135:
		MSM_HDMI_MODES_GET_DETAILS(mode,
					HDMI_VFRMT_4096x2160p50_256_135);
		break;
	case HDMI_VFRMT_4096x2160p60_256_135:
		MSM_HDMI_MODES_GET_DETAILS(mode,
					HDMI_VFRMT_4096x2160p60_256_135);
		break;
	case HDMI_VFRMT_3840x2160p24_64_27:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p24_64_27);
		break;
	case HDMI_VFRMT_3840x2160p25_64_27:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p25_64_27);
		break;
	case HDMI_VFRMT_3840x2160p30_64_27:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p30_64_27);
		break;
	case HDMI_VFRMT_3840x2160p50_64_27:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p50_64_27);
		break;
	case HDMI_VFRMT_3840x2160p60_64_27:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p60_64_27);
		break;
	default:
		ret = hdmi_get_resv_timing_info(mode, id);
	}

	return ret;
}

int hdmi_get_supported_mode(struct msm_hdmi_mode_timing_info *info,
	struct hdmi_util_ds_data *ds_data, u32 mode)
{
	int ret;

	if (!info)
		return -EINVAL;

	if (mode >= HDMI_VFRMT_MAX)
		return -EINVAL;

	ret = msm_hdmi_get_timing_info(info, mode);

	if (!ret && ds_data && ds_data->ds_registered && ds_data->ds_max_clk) {
		if (info->pixel_freq > ds_data->ds_max_clk)
			info->supported = false;
	}

	return ret;
} /* hdmi_get_supported_mode */

const char *msm_hdmi_mode_2string(u32 mode)
{
	static struct msm_hdmi_mode_timing_info ri = {0};
	char *aspect_ratio;

	if (mode >= HDMI_VFRMT_MAX)
		return "???";

	if (hdmi_get_supported_mode(&ri, NULL, mode))
		return "???";

	memset(res_buf, 0, sizeof(res_buf));

	if (!ri.supported) {
		snprintf(res_buf, RESOLUTION_NAME_STR_LEN, "%d", mode);
		return res_buf;
	}

	switch (ri.ar) {
	case HDMI_RES_AR_4_3:
		aspect_ratio = "4/3";
		break;
	case HDMI_RES_AR_5_4:
		aspect_ratio = "5/4";
		break;
	case HDMI_RES_AR_16_9:
		aspect_ratio = "16/9";
		break;
	case HDMI_RES_AR_16_10:
		aspect_ratio = "16/10";
		break;
	default:
		aspect_ratio = "???";
	};

	snprintf(res_buf, RESOLUTION_NAME_STR_LEN, "%dx%d %s%dHz %s",
		ri.active_h, ri.active_v, ri.interlaced ? "i" : "p",
		ri.refresh_rate / 1000, aspect_ratio);

	return res_buf;
}

int hdmi_get_video_id_code(struct msm_hdmi_mode_timing_info *timing_in,
	struct hdmi_util_ds_data *ds_data)
{
	int i, vic = -1;
	struct msm_hdmi_mode_timing_info supported_timing = {0};
	u32 ret;

	if (!timing_in) {
		pr_err("invalid input\n");
		goto exit;
	}

	/* active_low_h, active_low_v and interlaced are not checked against */
	for (i = 0; i < HDMI_VFRMT_MAX; i++) {
		ret = hdmi_get_supported_mode(&supported_timing, ds_data, i);

		if (ret || !supported_timing.supported)
			continue;
		if (timing_in->active_h != supported_timing.active_h)
			continue;
		if (timing_in->front_porch_h != supported_timing.front_porch_h)
			continue;
		if (timing_in->pulse_width_h != supported_timing.pulse_width_h)
			continue;
		if (timing_in->back_porch_h != supported_timing.back_porch_h)
			continue;
		if (timing_in->active_v != supported_timing.active_v)
			continue;
		if (timing_in->front_porch_v != supported_timing.front_porch_v)
			continue;
		if (timing_in->pulse_width_v != supported_timing.pulse_width_v)
			continue;
		if (timing_in->back_porch_v != supported_timing.back_porch_v)
			continue;
		if (timing_in->pixel_freq != supported_timing.pixel_freq)
			continue;
		if (timing_in->refresh_rate != supported_timing.refresh_rate)
			continue;

		vic = (int)supported_timing.video_format;
		break;
	}

	if (vic < 0)
		pr_err("timing is not supported h=%d v=%d\n",
			timing_in->active_h, timing_in->active_v);
	else
		pr_debug("vic = %d timing = %s\n", vic,
			msm_hdmi_mode_2string((u32)vic));
exit:

	return vic;
} /* hdmi_get_video_id_code */

static const char *hdmi_get_single_video_3d_fmt_2string(u32 format)
{
	switch (format) {
	case TOP_AND_BOTTOM:	return "TAB";
	case FRAME_PACKING:	return "FP";
	case SIDE_BY_SIDE_HALF: return "SSH";
	}
	return "";
} /* hdmi_get_single_video_3d_fmt_2string */

ssize_t hdmi_get_video_3d_fmt_2string(u32 format, char *buf, u32 size)
{
	ssize_t ret, len = 0;

	ret = scnprintf(buf, size, "%s",
		hdmi_get_single_video_3d_fmt_2string(
			format & FRAME_PACKING));
	len += ret;

	if (len && (format & TOP_AND_BOTTOM))
		ret = scnprintf(buf + len, size - len, ":%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & TOP_AND_BOTTOM));
	else
		ret = scnprintf(buf + len, size - len, "%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & TOP_AND_BOTTOM));
	len += ret;

	if (len && (format & SIDE_BY_SIDE_HALF))
		ret = scnprintf(buf + len, size - len, ":%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & SIDE_BY_SIDE_HALF));
	else
		ret = scnprintf(buf + len, size - len, "%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & SIDE_BY_SIDE_HALF));
	len += ret;

	return len;
} /* hdmi_get_video_3d_fmt_2string */

static void hdmi_ddc_trigger(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
		enum trigger_mode mode, bool seg)
{
	struct hdmi_tx_ddc_data *ddc_data = &ddc_ctrl->ddc_data;
	struct mdss_io_data *io = ddc_ctrl->io;
	u32 const seg_addr = 0x60, seg_num = 0x01;
	u32 ddc_ctrl_reg_val;

	ddc_data->dev_addr &= 0xFE;

	if (mode == TRIGGER_READ && seg) {
		DSS_REG_W_ND(io, HDMI_DDC_DATA, BIT(31) | (seg_addr << 8));
		DSS_REG_W_ND(io, HDMI_DDC_DATA, seg_num << 8);
	}

	/* handle portion #1 */
	DSS_REG_W_ND(io, HDMI_DDC_DATA, BIT(31) | (ddc_data->dev_addr << 8));

	/* handle portion #2 */
	DSS_REG_W_ND(io, HDMI_DDC_DATA, ddc_data->offset << 8);

	if (mode == TRIGGER_READ) {
		/* handle portion #3 */
		DSS_REG_W_ND(io, HDMI_DDC_DATA,
			(ddc_data->dev_addr | BIT(0)) << 8);

		/* HDMI_I2C_TRANSACTION0 */
		DSS_REG_W_ND(io, HDMI_DDC_TRANS0, BIT(12) | BIT(16));

		/* Write to HDMI_I2C_TRANSACTION1 */
		if (seg) {
			DSS_REG_W_ND(io, HDMI_DDC_TRANS1, BIT(12) | BIT(16));
			DSS_REG_W_ND(io, HDMI_DDC_TRANS2,
				BIT(0) | BIT(12) | BIT(13) |
				(ddc_data->request_len << 16));

			ddc_ctrl_reg_val = BIT(0) | BIT(21);
		} else {
			DSS_REG_W_ND(io, HDMI_DDC_TRANS1,
				BIT(0) | BIT(12) | BIT(13) |
				(ddc_data->request_len << 16));

			ddc_ctrl_reg_val = BIT(0) | BIT(20);
		}
	} else {
		int ndx;

		/* write buffer */
		for (ndx = 0; ndx < ddc_data->data_len; ++ndx)
			DSS_REG_W_ND(io, HDMI_DDC_DATA,
				((u32)ddc_data->data_buf[ndx]) << 8);

		DSS_REG_W_ND(io, HDMI_DDC_TRANS0,
			(ddc_data->data_len + 1) << 16 | BIT(12) | BIT(13));

		ddc_ctrl_reg_val = BIT(0);
	}

	/* Trigger the I2C transfer */
	DSS_REG_W_ND(io, HDMI_DDC_CTRL, ddc_ctrl_reg_val);
}

static void hdmi_ddc_clear_status(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	u32 reg_val;

	/* Read DDC status */
	reg_val = DSS_REG_R(ddc_ctrl->io, HDMI_DDC_SW_STATUS);
	reg_val &= BIT(12) | BIT(13) | BIT(14) | BIT(15);

	/* Check if any NACK occurred */
	if (reg_val) {
		pr_debug("%s: NACK: HDMI_DDC_SW_STATUS 0x%x\n",
			ddc_ctrl->ddc_data.what, reg_val);

		/* SW_STATUS_RESET, SOFT_RESET */
		reg_val = BIT(3) | BIT(1);

		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, reg_val);
	}
}

static int hdmi_ddc_read_retry(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	u32 reg_val, ndx, time_out_count, wait_time;
	struct hdmi_tx_ddc_data *ddc_data;
	int status;
	int busy_wait_us = 0;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ddc_data  = &ddc_ctrl->ddc_data;

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		pr_err("%s: invalid buf\n", ddc_data->what);
		goto error;
	}

	if (ddc_data->retry < 0) {
		pr_err("invalid no. of retries %d\n", ddc_data->retry);
		status = -EINVAL;
		goto error;
	}

	do {
		status = hdmi_ddc_clear_irq(ddc_ctrl, ddc_data->what);
		if (status)
			continue;

		if (ddc_data->hard_timeout) {
			pr_debug("using hard_timeout %dms\n",
				ddc_data->hard_timeout);

			busy_wait_us = ddc_data->hard_timeout * HDMI_MS_TO_US;
			atomic_set(&ddc_ctrl->read_busy_wait_done, 0);
		} else {
			reinit_completion(&ddc_ctrl->ddc_sw_done);
			wait_time = HZ / 2;
		}

		hdmi_ddc_trigger(ddc_ctrl, TRIGGER_READ, false);

		if (ddc_data->hard_timeout) {
			while (busy_wait_us > 0 &&
				!atomic_read(&ddc_ctrl->read_busy_wait_done)) {
				udelay(HDMI_BUSY_WAIT_DELAY_US);
				busy_wait_us -= HDMI_BUSY_WAIT_DELAY_US;
			};

			if (busy_wait_us < 0)
				busy_wait_us = 0;

			time_out_count = busy_wait_us / HDMI_MS_TO_US;

			ddc_data->timeout_left = time_out_count;
		} else {
			time_out_count = wait_for_completion_timeout(
				&ddc_ctrl->ddc_sw_done, wait_time);

			ddc_data->timeout_left =
				jiffies_to_msecs(time_out_count);
		}

		pr_debug("ddc read done at %dms\n", jiffies_to_msecs(jiffies));

		if (!time_out_count) {
			pr_debug("%s: timedout\n", ddc_data->what);

			status = -ETIMEDOUT;
		}

		hdmi_ddc_clear_status(ddc_ctrl);
	} while (status && ddc_data->retry--);

	if (status)
		goto error;

	/* Write data to DDC buffer */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		BIT(0) | (3 << 16) | BIT(31));

	/* Discard first byte */
	DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);
	for (ndx = 0; ndx < ddc_data->data_len; ++ndx) {
		reg_val = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);
		ddc_data->data_buf[ndx] = (u8)((reg_val & 0x0000FF00) >> 8);
	}

	pr_debug("%s: success\n",  ddc_data->what);

error:
	return status;
} /* hdmi_ddc_read_retry */

void hdmi_ddc_config(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return;
	}

	/* Configure Pre-Scale multiplier & Threshold */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_SPEED, (10 << 16) | (2 << 0));

	/*
	 * Setting 31:24 bits : Time units to wait before timeout
	 * when clock is being stalled by external sink device
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_SETUP, 0xFF000000);

	/* Enable reference timer to 19 micro-seconds */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_REF, (1 << 16) | (19 << 0));
} /* hdmi_ddc_config */

static void hdmi_hdcp2p2_ddc_clear_status(struct hdmi_tx_ddc_ctrl *ctrl)
{
	u32 reg_val;

	if (!ctrl) {
		pr_err("invalid ddc ctrl\n");
		return;
	}

	/* check for errors and clear status */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HDCP2P2_DDC_STATUS);

	if (reg_val & BIT(4)) {
		pr_debug("ddc aborted\n");
		reg_val |= BIT(5);
	}

	if (reg_val & BIT(8)) {
		pr_debug("timed out\n");
		reg_val |= BIT(9);
	}

	if (reg_val & BIT(12)) {
		pr_debug("NACK0\n");
		reg_val |= BIT(13);
	}

	if (reg_val & BIT(14)) {
		pr_debug("NACK1\n");
		reg_val |= BIT(15);
	}

	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_STATUS, reg_val);
}

static int hdmi_ddc_hdcp2p2_isr(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	struct mdss_io_data *io = NULL;
	struct hdmi_tx_hdcp2p2_ddc_data *data;
	u32 intr0, intr2, intr5;
	u32 msg_size;
	int rc = 0;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	io = ddc_ctrl->io;

	data = &ddc_ctrl->hdcp2p2_ddc_data;

	intr0 = DSS_REG_R(io, HDMI_DDC_INT_CTRL0);
	intr2 = DSS_REG_R(io, HDMI_HDCP_INT_CTRL2);
	intr5 = DSS_REG_R_ND(io, HDMI_DDC_INT_CTRL5);

	pr_debug("intr0: 0x%x, intr2: 0x%x, intr5: 0x%x\n",
			intr0, intr2, intr5);

	/* check if encryption is enabled */
	if (intr2 & BIT(0)) {
		/*
		 * ack encryption ready interrupt.
		 * disable encryption ready interrupt.
		 * enable encryption not ready interrupt.
		 */
		intr2 &= ~BIT(2);
		intr2 |= BIT(1) | BIT(6);

		pr_debug("HDCP 2.2 Encryption enabled\n");
		data->encryption_ready = true;
	}

	/* check if encryption is disabled */
	if (intr2 & BIT(4)) {
		/*
		 * ack encryption not ready interrupt.
		 * disable encryption not ready interrupt.
		 * enable encryption ready interrupt.
		 */
		intr2  &= ~BIT(6);
		intr2  |= BIT(5) | BIT(2);

		pr_debug("HDCP 2.2 Encryption disabled\n");
		data->encryption_ready = false;
	}

	DSS_REG_W_ND(io, HDMI_HDCP_INT_CTRL2, intr2);

	/* get the message size bits 29:20 */
	msg_size = (intr0 & (0x3FF << 20)) >> 20;

	if (msg_size) {
		/* ack and disable message size interrupt */
		intr0 |= BIT(30);
		intr0 &= ~BIT(31);

		data->message_size = msg_size;
	}

	/* check and disable ready interrupt */
	if (intr0 & BIT(16)) {
		/* ack ready/not ready interrupt */
		intr0 |= BIT(17);

		intr0 &= ~BIT(18);
		data->ready = true;
	}

	/* check for reauth req interrupt */
	if (intr0 & BIT(12)) {
		/* ack and disable reauth req interrupt */
		intr0 |= BIT(13);
		intr0 &= ~BIT(14);

		data->reauth_req = true;
	}

	/* check for ddc fail interrupt */
	if (intr0 & BIT(8)) {
		/* ack ddc fail interrupt */
		intr0 |= BIT(9);

		data->ddc_max_retries_fail = true;
	}

	/* check for ddc done interrupt */
	if (intr0 & BIT(4)) {
		/* ack ddc done interrupt */
		intr0 |= BIT(5);

		data->ddc_done = true;
	}

	/* check for ddc read req interrupt */
	if (intr0 & BIT(0)) {
		/* ack read req interrupt */
		intr0 |= BIT(1);

		data->ddc_read_req = true;
	}

	DSS_REG_W_ND(io, HDMI_DDC_INT_CTRL0, intr0);

	if (intr5 & BIT(0)) {
		pr_err("RXSTATUS_DDC_REQ_TIMEOUT\n");

		/* ack and disable timeout interrupt */
		intr5 |= BIT(1);
		intr5 &= ~BIT(2);

		data->ddc_timeout = true;
	}
	DSS_REG_W_ND(io, HDMI_DDC_INT_CTRL5, intr5);

	if (data->message_size || data->ready || data->reauth_req) {
		if (data->wait) {
			atomic_set(&ddc_ctrl->rxstatus_busy_wait_done, 1);
		} else if (data->link_cb && data->link_data) {
			data->link_cb(data->link_data);
		} else {
			pr_err("new msg/reauth not handled\n");
			rc = -EINVAL;
		}
	}

	hdmi_hdcp2p2_ddc_clear_status(ddc_ctrl);

	return rc;
}

static int hdmi_ddc_scrambling_isr(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	struct mdss_io_data *io;
	bool scrambler_timer_off = false;
	u32 intr2, intr5;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	io = ddc_ctrl->io;

	intr2 = DSS_REG_R_ND(io, HDMI_DDC_INT_CTRL2);
	intr5 = DSS_REG_R_ND(io, HDMI_DDC_INT_CTRL5);

	pr_debug("intr2: 0x%x, intr5: 0x%x\n", intr2, intr5);

	if (intr2 & BIT(12)) {
		pr_err("SCRAMBLER_STATUS_NOT\n");

		intr2 |= BIT(14);

		scrambler_timer_off = true;
	}

	if (intr2 & BIT(8)) {
		pr_err("SCRAMBLER_STATUS_DDC_FAILED\n");

		intr2 |= BIT(9);

		scrambler_timer_off = true;
	}
	DSS_REG_W_ND(io, HDMI_DDC_INT_CTRL2, intr2);

	if (intr5 & BIT(8)) {
		pr_err("SCRAMBLER_STATUS_DDC_REQ_TIMEOUT\n");

		intr5 |= BIT(9);
		intr5 &= ~BIT(10);

		scrambler_timer_off = true;
	}
	DSS_REG_W_ND(io, HDMI_DDC_INT_CTRL5, intr5);

	if (scrambler_timer_off)
		hdmi_scrambler_ddc_disable(ddc_ctrl);

	return 0;
}

int hdmi_ddc_isr(struct hdmi_tx_ddc_ctrl *ddc_ctrl, u32 version)
{
	u32 ddc_int_ctrl, ret = 0;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ddc_int_ctrl = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL);
	pr_debug("intr: 0x%x\n", ddc_int_ctrl);

	if (ddc_int_ctrl & BIT(0)) {
		pr_debug("sw done\n");

		ddc_int_ctrl |= BIT(1);
		if (ddc_ctrl->ddc_data.hard_timeout) {
			atomic_set(&ddc_ctrl->read_busy_wait_done, 1);
			atomic_set(&ddc_ctrl->write_busy_wait_done, 1);
		} else {
			complete(&ddc_ctrl->ddc_sw_done);
		}
	}

	if (ddc_int_ctrl & BIT(4)) {
		pr_debug("hw done\n");
		ddc_int_ctrl |= BIT(5);
	}

	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL, ddc_int_ctrl);

	if (version >= HDMI_TX_SCRAMBLER_MIN_TX_VERSION) {
		ret = hdmi_ddc_scrambling_isr(ddc_ctrl);
		if (ret)
			pr_err("err in scrambling isr\n");
	}

	ret = hdmi_ddc_hdcp2p2_isr(ddc_ctrl);
	if (ret)
		pr_err("err in hdcp2p2 isr\n");

	return ret;
} /* hdmi_ddc_isr */

int hdmi_ddc_read(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	int rc = 0;
	int retry;
	struct hdmi_tx_ddc_data *ddc_data;

	if (!ddc_ctrl) {
		pr_err("invalid ddc ctrl\n");
		return -EINVAL;
	}

	ddc_data = &ddc_ctrl->ddc_data;
	retry = ddc_data->retry;

	rc = hdmi_ddc_read_retry(ddc_ctrl);
	if (!rc)
		return rc;

	if (ddc_data->retry_align) {
		ddc_data->retry = retry;

		ddc_data->request_len = 32 * ((ddc_data->data_len + 31) / 32);
		rc = hdmi_ddc_read_retry(ddc_ctrl);
	}

	return rc;
} /* hdmi_ddc_read */

int hdmi_ddc_read_seg(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	int status;
	u32 reg_val, ndx, time_out_count;
	struct hdmi_tx_ddc_data *ddc_data;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ddc_data = &ddc_ctrl->ddc_data;

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		pr_err("%s: invalid buf\n", ddc_data->what);
		goto error;
	}

	if (ddc_data->retry < 0) {
		pr_err("invalid no. of retries %d\n", ddc_data->retry);
		status = -EINVAL;
		goto error;
	}

	do {
		status = hdmi_ddc_clear_irq(ddc_ctrl, ddc_data->what);
		if (status)
			continue;

		reinit_completion(&ddc_ctrl->ddc_sw_done);

		hdmi_ddc_trigger(ddc_ctrl, TRIGGER_READ, true);

		time_out_count = wait_for_completion_timeout(
			&ddc_ctrl->ddc_sw_done, HZ / 2);

		if (!time_out_count) {
			pr_debug("%s: timedout\n", ddc_data->what);

			status = -ETIMEDOUT;
		}

		hdmi_ddc_clear_status(ddc_ctrl);
	} while (status && ddc_data->retry--);

	if (status)
		goto error;

	/* Write data to DDC buffer */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		BIT(0) | (5 << 16) | BIT(31));

	/* Discard first byte */
	DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);

	for (ndx = 0; ndx < ddc_data->data_len; ++ndx) {
		reg_val = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);
		ddc_data->data_buf[ndx] = (u8) ((reg_val & 0x0000FF00) >> 8);
	}

	pr_debug("%s: success\n", ddc_data->what);

error:
	return status;
} /* hdmi_ddc_read_seg */

int hdmi_ddc_write(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	int status;
	u32 time_out_count;
	struct hdmi_tx_ddc_data *ddc_data;
	u32 wait_time;
	int busy_wait_us = 0;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ddc_data = &ddc_ctrl->ddc_data;

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		pr_err("%s: invalid buf\n", ddc_data->what);
		goto error;
	}

	if (ddc_data->retry < 0) {
		pr_err("invalid no. of retries %d\n", ddc_data->retry);
		status = -EINVAL;
		goto error;
	}

	do {
		status = hdmi_ddc_clear_irq(ddc_ctrl, ddc_data->what);
		if (status)
			continue;

		if (ddc_data->hard_timeout) {
			pr_debug("using hard_timeout %dms\n",
				ddc_data->hard_timeout);

			busy_wait_us = ddc_data->hard_timeout * HDMI_MS_TO_US;
			atomic_set(&ddc_ctrl->write_busy_wait_done, 0);
		} else {
			reinit_completion(&ddc_ctrl->ddc_sw_done);
			wait_time = HZ / 2;
		}

		hdmi_ddc_trigger(ddc_ctrl, TRIGGER_WRITE, false);

		if (ddc_data->hard_timeout) {
			while (busy_wait_us > 0 &&
				!atomic_read(&ddc_ctrl->write_busy_wait_done)) {
				udelay(HDMI_BUSY_WAIT_DELAY_US);
				busy_wait_us -= HDMI_BUSY_WAIT_DELAY_US;
			};

			if (busy_wait_us < 0)
				busy_wait_us = 0;

			time_out_count = busy_wait_us / HDMI_MS_TO_US;

			ddc_data->timeout_left = time_out_count;
		} else {
			time_out_count = wait_for_completion_timeout(
				&ddc_ctrl->ddc_sw_done, wait_time);

			ddc_data->timeout_left =
				jiffies_to_msecs(time_out_count);
		}

		pr_debug("DDC write done at %dms\n", jiffies_to_msecs(jiffies));

		if (!time_out_count) {
			pr_debug("%s timout\n",  ddc_data->what);

			status = -ETIMEDOUT;
		}

		hdmi_ddc_clear_status(ddc_ctrl);
	} while (status && ddc_data->retry--);

	if (status)
		goto error;

	pr_debug("%s: success\n", ddc_data->what);
error:
	return status;
} /* hdmi_ddc_write */


int hdmi_ddc_abort_transaction(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	int status;
	struct hdmi_tx_ddc_data *ddc_data;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ddc_data = &ddc_ctrl->ddc_data;

	status = hdmi_ddc_clear_irq(ddc_ctrl, ddc_data->what);
	if (status)
		goto error;

	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_ARBITRATION, BIT(12)|BIT(8));

error:
	return status;

}

int hdmi_scdc_read(struct hdmi_tx_ddc_ctrl *ctrl, u32 data_type, u32 *val)
{
	struct hdmi_tx_ddc_data data = {0};
	int rc = 0;
	u8 data_buf[2] = {0};

	if (!ctrl || !ctrl->io || !val) {
		pr_err("Bad Parameters\n");
		return -EINVAL;
	}

	if (data_type >= HDMI_TX_SCDC_MAX) {
		pr_err("Unsupported data type\n");
		return -EINVAL;
	}

	data.what = hdmi_scdc_reg2string(data_type);
	data.dev_addr = 0xA8;
	data.retry = 1;
	data.data_buf = data_buf;

	switch (data_type) {
	case HDMI_TX_SCDC_SCRAMBLING_STATUS:
		data.data_len = 1;
		data.request_len = 1;
		data.offset = HDMI_SCDC_SCRAMBLER_STATUS;
		break;
	case HDMI_TX_SCDC_SCRAMBLING_ENABLE:
	case HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE:
		data.data_len = 1;
		data.request_len = 1;
		data.offset = HDMI_SCDC_TMDS_CONFIG;
		break;
	case HDMI_TX_SCDC_CLOCK_DET_STATUS:
	case HDMI_TX_SCDC_CH0_LOCK_STATUS:
	case HDMI_TX_SCDC_CH1_LOCK_STATUS:
	case HDMI_TX_SCDC_CH2_LOCK_STATUS:
		data.data_len = 1;
		data.request_len = 1;
		data.offset = HDMI_SCDC_STATUS_FLAGS_0;
		break;
	case HDMI_TX_SCDC_CH0_ERROR_COUNT:
		data.data_len = 2;
		data.request_len = 2;
		data.offset = HDMI_SCDC_ERR_DET_0_L;
		break;
	case HDMI_TX_SCDC_CH1_ERROR_COUNT:
		data.data_len = 2;
		data.request_len = 2;
		data.offset = HDMI_SCDC_ERR_DET_1_L;
		break;
	case HDMI_TX_SCDC_CH2_ERROR_COUNT:
		data.data_len = 2;
		data.request_len = 2;
		data.offset = HDMI_SCDC_ERR_DET_2_L;
		break;
	case HDMI_TX_SCDC_READ_ENABLE:
		data.data_len = 1;
		data.request_len = 1;
		data.offset = HDMI_SCDC_CONFIG_0;
		break;
	default:
		break;
	}

	ctrl->ddc_data = data;

	rc = hdmi_ddc_read(ctrl);
	if (rc) {
		pr_err("DDC Read failed for %s\n", data.what);
		return rc;
	}

	switch (data_type) {
	case HDMI_TX_SCDC_SCRAMBLING_STATUS:
		*val = (data_buf[0] & BIT(0)) ? 1 : 0;
		break;
	case HDMI_TX_SCDC_SCRAMBLING_ENABLE:
		*val = (data_buf[0] & BIT(0)) ? 1 : 0;
		break;
	case HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE:
		*val = (data_buf[0] & BIT(1)) ? 1 : 0;
		break;
	case HDMI_TX_SCDC_CLOCK_DET_STATUS:
		*val = (data_buf[0] & BIT(0)) ? 1 : 0;
		break;
	case HDMI_TX_SCDC_CH0_LOCK_STATUS:
		*val = (data_buf[0] & BIT(1)) ? 1 : 0;
		break;
	case HDMI_TX_SCDC_CH1_LOCK_STATUS:
		*val = (data_buf[0] & BIT(2)) ? 1 : 0;
		break;
	case HDMI_TX_SCDC_CH2_LOCK_STATUS:
		*val = (data_buf[0] & BIT(3)) ? 1 : 0;
		break;
	case HDMI_TX_SCDC_CH0_ERROR_COUNT:
	case HDMI_TX_SCDC_CH1_ERROR_COUNT:
	case HDMI_TX_SCDC_CH2_ERROR_COUNT:
		if (data_buf[1] & BIT(7))
			*val = (data_buf[0] | ((data_buf[1] & 0x7F) << 8));
		else
			*val = 0;
		break;
	case HDMI_TX_SCDC_READ_ENABLE:
		*val = (data_buf[0] & BIT(0)) ? 1 : 0;
		break;
	default:
		break;
	}

	return 0;
}

int hdmi_scdc_write(struct hdmi_tx_ddc_ctrl *ctrl, u32 data_type, u32 val)
{
	struct hdmi_tx_ddc_data data = {0};
	struct hdmi_tx_ddc_data rdata = {0};
	int rc = 0;
	u8 data_buf[2] = {0};
	u8 read_val = 0;

	if (!ctrl || !ctrl->io) {
		pr_err("Bad Parameters\n");
		return -EINVAL;
	}

	if (data_type >= HDMI_TX_SCDC_MAX) {
		pr_err("Unsupported data type\n");
		return -EINVAL;
	}

	data.what = hdmi_scdc_reg2string(data_type);
	data.dev_addr = 0xA8;
	data.retry = 1;
	data.data_buf = data_buf;

	switch (data_type) {
	case HDMI_TX_SCDC_SCRAMBLING_ENABLE:
	case HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE:
		rdata.what = "TMDS CONFIG";
		rdata.dev_addr = 0xA8;
		rdata.retry = 2;
		rdata.data_buf = &read_val;
		rdata.data_len = 1;
		rdata.offset = HDMI_SCDC_TMDS_CONFIG;
		rdata.request_len = 1;
		ctrl->ddc_data = rdata;
		rc = hdmi_ddc_read(ctrl);
		if (rc) {
			pr_err("scdc read failed\n");
			return rc;
		}
		if (data_type == HDMI_TX_SCDC_SCRAMBLING_ENABLE) {
			data_buf[0] = ((((u8)(read_val & 0xFF)) & (~BIT(0))) |
				       ((u8)(val & BIT(0))));
		} else {
			data_buf[0] = ((((u8)(read_val & 0xFF)) & (~BIT(1))) |
				       (((u8)(val & BIT(0))) << 1));
		}
		data.data_len = 1;
		data.request_len = 1;
		data.offset = HDMI_SCDC_TMDS_CONFIG;
		break;
	case HDMI_TX_SCDC_READ_ENABLE:
		data.data_len = 1;
		data.request_len = 1;
		data.offset = HDMI_SCDC_CONFIG_0;
		data_buf[0] = (u8)(val & 0x1);
		break;
	default:
		pr_err("Cannot write to read only reg (%d)\n",
			data_type);
		return -EINVAL;
	}

	ctrl->ddc_data = data;

	rc = hdmi_ddc_write(ctrl);
	if (rc) {
		pr_err("DDC Read failed for %s\n", data.what);
		return rc;
	}

	return 0;
}

int hdmi_setup_ddc_timers(struct hdmi_tx_ddc_ctrl *ctrl,
			  u32 type, u32 to_in_num_lines)
{
	if (!ctrl) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	if (type >= HDMI_TX_DDC_TIMER_MAX) {
		pr_err("Invalid timer type %d\n", type);
		return -EINVAL;
	}

	switch (type) {
	case HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS:
		hdmi_scrambler_status_timer_setup(ctrl, to_in_num_lines);
		break;
	default:
		pr_err("%d type not supported\n", type);
		return -EINVAL;
	}

	return 0;
}

static void hdmi_hdcp2p2_ddc_reset(struct hdmi_tx_ddc_ctrl *ctrl)
{
	u32 reg_val;

	if (!ctrl) {
		pr_err("Invalid parameters\n");
		return;
	}

	/*
	 * Clear acks for DDC_REQ, DDC_DONE, DDC_FAILED, RXSTATUS_READY,
	 * RXSTATUS_MSG_SIZE
	 */
	reg_val = BIT(30) | BIT(17) | BIT(13) | BIT(9) | BIT(5) | BIT(1);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL0, reg_val);

	/* Reset DDC timers */
	reg_val = BIT(0) | DSS_REG_R(ctrl->io, HDMI_HDCP2P2_DDC_CTRL);
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_CTRL, reg_val);
	reg_val = DSS_REG_R(ctrl->io, HDMI_HDCP2P2_DDC_CTRL);
	reg_val &= ~BIT(0);
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_CTRL, reg_val);
}

void hdmi_hdcp2p2_ddc_disable(struct hdmi_tx_ddc_ctrl *ctrl)
{
	u32 reg_val;

	if (!ctrl) {
		pr_err("Invalid parameters\n");
		return;
	}

	hdmi_hdcp2p2_ddc_reset(ctrl);

	/* Disable HW DDC access to RxStatus register */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(1) | BIT(0));

	DSS_REG_W(ctrl->io, HDMI_HW_DDC_CTRL, reg_val);
}

int hdmi_hdcp2p2_ddc_read_rxstatus(struct hdmi_tx_ddc_ctrl *ctrl)
{
	u32 reg_val;
	u32 intr_en_mask;
	u32 timeout;
	u32 timer;
	int rc = 0;
	struct hdmi_tx_hdcp2p2_ddc_data *data;
	int busy_wait_us;

	if (!ctrl) {
		pr_err("Invalid ctrl data\n");
		return -EINVAL;
	}

	data = &ctrl->hdcp2p2_ddc_data;
	if (!data) {
		pr_err("Invalid ddc data\n");
		return -EINVAL;
	}

	rc = hdmi_ddc_clear_irq(ctrl, "rxstatus");
	if (rc)
		return rc;

	intr_en_mask = data->intr_mask;
	intr_en_mask |= BIT(HDCP2P2_RXSTATUS_DDC_FAILED_INTR_MASK);

	/* Disable short read for now, sinks don't support it */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HDCP2P2_DDC_CTRL);
	reg_val |= BIT(4);
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_CTRL, reg_val);

	/*
	 * Setup the DDC timers for HDMI_HDCP2P2_DDC_TIMER_CTRL1 and
	 *  HDMI_HDCP2P2_DDC_TIMER_CTRL2.
	 * Following are the timers:
	 * 1. DDC_REQUEST_TIMER: Timeout in hsyncs in which to wait for the
	 *	HDCP 2.2 sink to respond to an RxStatus request
	 * 2. DDC_URGENT_TIMER: Time period in hsyncs to issue an urgent flag
	 *	when an RxStatus DDC request is made but not accepted by I2C
	 *	engine
	 * 3. DDC_TIMEOUT_TIMER: Timeout in hsyncs which starts counting when
	 *	a request is made and stops when it is accepted by DDC arbiter
	 */
	timeout = data->timeout_hsync;
	timer = data->periodic_timer_hsync;
	pr_debug("timeout: %d hsyncs, timer %d hsync\n", timeout, timer);

	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_TIMER_CTRL, timer);

	/* Set both urgent and hw-timeout fields to the same value */
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_TIMER_CTRL2,
		(timeout << 16 | timeout));

	/* enable interrupts */
	reg_val = intr_en_mask;
	/* Clear interrupt status bits */
	reg_val |= intr_en_mask >> 1;

	pr_debug("writing HDMI_DDC_INT_CTRL0 0x%x\n", reg_val);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL0, reg_val);

	reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL5);
	/* clear and enable RxStatus read timeout */
	reg_val |= BIT(2) | BIT(1);

	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL5, reg_val);

	/*
	 * Enable hardware DDC access to RxStatus register
	 *
	 * HDMI_HW_DDC_CTRL:Bits 1:0 (RXSTATUS_DDC_ENABLE) read like this:
	 *
	 * 0 = disable HW controlled DDC access to RxStatus
	 * 1 = automatic on when HDCP 2.2 is authenticated and loop based on
	 * request timer (i.e. the hardware will loop automatically)
	 * 2 = force on and loop based on request timer (hardware will loop)
	 * 3 = enable by sw trigger and loop until interrupt is generated for
	 * RxStatus.reauth_req, RxStatus.ready or RxStatus.message_Size.
	 *
	 * Depending on the value of ddc_data::poll_sink, we make the decision
	 * to use either SW_TRIGGER(3) (poll_sink = false) which means that the
	 * hardware will poll sink and generate interrupt when sink responds,
	 * or use AUTOMATIC_LOOP(1) (poll_sink = true) which will poll the sink
	 * based on request timer
	 */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(1) | BIT(0));

	busy_wait_us = data->timeout_ms * HDMI_MS_TO_US;
	atomic_set(&ctrl->rxstatus_busy_wait_done, 0);

	/* read method: HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER */
	reg_val |= BIT(1) | BIT(0);
	DSS_REG_W(ctrl->io, HDMI_HW_DDC_CTRL, reg_val);
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_SW_TRIGGER, 1);

	if (data->wait) {
		while (busy_wait_us > 0 &&
			!atomic_read(&ctrl->rxstatus_busy_wait_done)) {
			udelay(HDMI_BUSY_WAIT_DELAY_US);
			busy_wait_us -= HDMI_BUSY_WAIT_DELAY_US;
		};

		if (busy_wait_us < 0)
			busy_wait_us = 0;

		data->timeout_left = busy_wait_us / HDMI_MS_TO_US;

		if (!data->timeout_left) {
			pr_err("sw ddc rxstatus timeout\n");
			rc = -ETIMEDOUT;
		}

		hdmi_hdcp2p2_ddc_disable(ctrl);
	}

	return rc;
}
