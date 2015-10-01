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

#include <linux/io.h>
#include <linux/delay.h>
#include "mdss_hdmi_util.h"

#define RESOLUTION_NAME_STR_LEN 30

#define HDMI_SCDC_UNKNOWN_REGISTER        "Unknown register"

static char res_buf[RESOLUTION_NAME_STR_LEN];

static void hdmi_scrambler_status_timer_setup(struct hdmi_tx_ddc_ctrl *ctrl,
					     u32 to_in_num_lines)
{
	u32 reg_val;

	DSS_REG_W(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL,
						to_in_num_lines);
	DSS_REG_W(ctrl->io, HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL2,
						0xFFFF);
	reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL5);
	reg_val |= BIT(10);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL5, reg_val);

	reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL2);
	/* Trigger interrupt if scrambler status is 0 or DDC failure */
	reg_val |= BIT(10);
	reg_val &= 0x18000;
	reg_val |= (0x2 << 15);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL2, reg_val);

	/* Enable DDC access */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HW_DDC_CTRL);

	reg_val &= ~0x300;
	reg_val |= (0x1 << 8);
	DSS_REG_W(ctrl->io, HDMI_HW_DDC_CTRL, reg_val);
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
		DEV_ERR("%s: invalid mode %d\n", __func__, mode);
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
		DEV_ERR("%s: invalid input\n", __func__);
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

	if (vic < 0) {
		for (i = 0; i < HDMI_VFRMT_MAX; i++) {
			ret = hdmi_get_supported_mode(&supported_timing,
				ds_data, i);
			if (ret || !supported_timing.supported)
				continue;
			if (timing_in->active_h != supported_timing.active_h)
				continue;
			if (timing_in->active_v != supported_timing.active_v)
				continue;
			vic = (int)supported_timing.video_format;
			break;
		}
	}

	if (vic < 0) {
		DEV_ERR("%s: timing is not supported h=%d v=%d\n",
			__func__, timing_in->active_h, timing_in->active_v);
	}

exit:
	DEV_DBG("%s: vic = %d timing = %s\n", __func__, vic,
		msm_hdmi_mode_2string((u32)vic));

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

static void hdmi_ddc_print_data(struct hdmi_tx_ddc_data *ddc_data,
	const char *caller)
{
	if (!ddc_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	DEV_DBG("%s: buf=%p, d_len=0x%x, d_addr=0x%x, no_align=%d\n",
		caller, ddc_data->data_buf, ddc_data->data_len,
		ddc_data->dev_addr, ddc_data->no_align);
	DEV_DBG("%s: offset=0x%x, req_len=0x%x, retry=%d, what=%s\n",
		caller, ddc_data->offset, ddc_data->request_len,
		ddc_data->retry, ddc_data->what);
} /* hdmi_ddc_print_data */

static int hdmi_ddc_clear_irq(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	char *what)
{
	u32 reg_val, time_out_count;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* clear pending and enable interrupt */
	time_out_count = 0xFFFF;
	do {
		--time_out_count;
		/* Clear and Enable DDC interrupt */
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL,
			BIT(2) | BIT(1));
		reg_val = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL);
	} while ((reg_val & BIT(0)) && time_out_count);

	if (!time_out_count) {
		DEV_ERR("%s[%s]: timedout\n", __func__, what);
		return -ETIMEDOUT;
	}

	return 0;
} /*hdmi_ddc_clear_irq */

static int hdmi_ddc_read_retry(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	struct hdmi_tx_ddc_data *ddc_data)
{
	u32 reg_val, ndx, time_out_count, wait_time;
	int status = 0;
	int log_retry_fail;

	if (!ddc_ctrl || !ddc_ctrl->io || !ddc_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		DEV_ERR("%s[%s]: invalid buf\n", __func__, ddc_data->what);
		goto error;
	}

	hdmi_ddc_print_data(ddc_data, __func__);

	log_retry_fail = ddc_data->retry != 1;
again:
	status = hdmi_ddc_clear_irq(ddc_ctrl, ddc_data->what);
	if (status)
		goto error;

	/* Ensure Device Address has LSB set to 0 to indicate Slave addr read */
	ddc_data->dev_addr &= 0xFE;

	/*
	 * 1. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #1
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress (primary link address and writing)
	 *    INDEX = 0x0 (initial offset into buffer)
	 *    INDEX_WRITE = 0x1 (setting initial offset)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		BIT(31) | (ddc_data->dev_addr << 8));

	/*
	 * 2. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #2
	 *    DATA_RW = 0x0 (write)
	 *    DATA = offsetAddress
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA, ddc_data->offset << 8);

	/*
	 * 3. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #3
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress + 1 (primary link address 0x74 and reading)
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		(ddc_data->dev_addr | BIT(0)) << 8);

	/* Data setup is complete, now setup the transaction characteristics */

	/*
	 * 4. Write to HDMI_I2C_TRANSACTION0 with the following fields set in
	 *    order to handle characteristics of portion #1 and portion #2
	 *    RW0 = 0x0 (write)
	 *    START0 = 0x1 (insert START bit)
	 *    STOP0 = 0x0 (do NOT insert STOP bit)
	 *    CNT0 = 0x1 (single byte transaction excluding address)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS0, BIT(12) | BIT(16));

	/*
	 * 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	 *    order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (it's 128 (0x80) for a blk read)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS1,
		BIT(0) | BIT(12) | BIT(13) | (ddc_data->request_len << 16));

	/* Trigger the I2C transfer */

	/*
	 * 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x1 (execute transaction0 followed by
	 *    transaction1)
	 *    SEND_RESET = Set to 1 to send reset sequence
	 *    GO = 0x1 (kicks off hardware)
	 */
	reinit_completion(&ddc_ctrl->ddc_sw_done);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(0) | BIT(20));

	if (ddc_data->hard_timeout) {
		DEV_DBG("%s: using hard_timeout %dms\n", __func__,
			ddc_data->hard_timeout);
		wait_time = msecs_to_jiffies(ddc_data->hard_timeout);
	} else {
		wait_time = HZ/2;
	}

	time_out_count = wait_for_completion_timeout(
		&ddc_ctrl->ddc_sw_done, wait_time);

	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL, BIT(1));
	if (!time_out_count) {
		if (ddc_data->retry-- > 0) {
			DEV_INFO("%s: failed timout, retry=%d\n", __func__,
				ddc_data->retry);
			goto again;
		}
		status = -ETIMEDOUT;
		DEV_ERR("%s: timedout(7), Int Ctrl=%08x\n", __func__,
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_INT_CTRL));
		DEV_ERR("%s: DDC SW Status=%08x, HW Status=%08x\n",
			__func__,
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_SW_STATUS),
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_HW_STATUS));
		goto error;
	}

	/* Read DDC status */
	reg_val = DSS_REG_R(ddc_ctrl->io, HDMI_DDC_SW_STATUS);
	reg_val &= BIT(12) | BIT(13) | BIT(14) | BIT(15);

	/* Check if any NACK occurred */
	if (reg_val) {
		/* SW_STATUS_RESET */
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(3));

		if (ddc_data->retry == 1)
			/* SOFT_RESET */
			DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(1));

		if (ddc_data->retry-- > 0) {
			DEV_DBG("%s(%s): failed NACK=0x%08x, retry=%d\n",
				__func__, ddc_data->what, reg_val,
				ddc_data->retry);
			DEV_DBG("%s: daddr=0x%02x,off=0x%02x,len=%d\n",
				__func__, ddc_data->dev_addr,
				ddc_data->offset, ddc_data->data_len);
			goto again;
		}
		status = -EIO;
		if (log_retry_fail) {
			DEV_ERR("%s(%s): failed NACK=0x%08x\n",
				__func__, ddc_data->what, reg_val);
			DEV_ERR("%s: daddr=0x%02x,off=0x%02x,len=%d\n",
				__func__, ddc_data->dev_addr,
				ddc_data->offset, ddc_data->data_len);
		}
		goto error;
	}

	/*
	 * 8. ALL data is now available and waiting in the DDC buffer.
	 *    Read HDMI_I2C_DATA with the following fields set
	 *    RW = 0x1 (read)
	 *    DATA = BCAPS (this is field where data is pulled from)
	 *    INDEX = 0x3 (where the data has been placed in buffer by hardware)
	 *    INDEX_WRITE = 0x1 (explicitly define offset)
	 */
	/* Write this data to DDC buffer */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		BIT(0) | (3 << 16) | BIT(31));

	/* Discard first byte */
	DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);
	for (ndx = 0; ndx < ddc_data->data_len; ++ndx) {
		reg_val = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);
		ddc_data->data_buf[ndx] = (u8)((reg_val & 0x0000FF00) >> 8);
	}

	DEV_DBG("%s[%s] success\n", __func__, ddc_data->what);

error:
	return status;
} /* hdmi_ddc_read_retry */

void hdmi_ddc_config(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	if (!ddc_ctrl || !ddc_ctrl->io) {
		DEV_ERR("%s: invalid input\n", __func__);
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

int hdmi_ddc_isr(struct hdmi_tx_ddc_ctrl *ddc_ctrl, u32 version)
{
	u32 ddc_int_ctrl;
	u32 ddc_timer_int;
	bool scrambler_timer_off = false;

	if (!ddc_ctrl || !ddc_ctrl->io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ddc_int_ctrl = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL);
	if ((ddc_int_ctrl & BIT(2)) && (ddc_int_ctrl & BIT(0))) {
		/* SW_DONE INT occured, clr it */
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL,
			ddc_int_ctrl | BIT(1));
		complete(&ddc_ctrl->ddc_sw_done);
	}

	DEV_DBG("%s: ddc_int_ctrl=%04x\n", __func__, ddc_int_ctrl);
	if (version < HDMI_TX_SCRAMBLER_MIN_TX_VERSION)
		goto end;

	ddc_timer_int = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL2);
	if (ddc_timer_int & BIT(12)) {
		/* DDC_INT_CTRL2.SCRAMBLER_STATUS_NOT is set */
		DEV_ERR("%s: Sink cannot descramble the signal\n",
							__func__);
		/* Clear interrupt */
		ddc_timer_int |= BIT(14);
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL2,
							ddc_timer_int);
		scrambler_timer_off = true;
	}

	ddc_timer_int = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL5);
	if (ddc_timer_int & BIT(8)) {
		/*
		 * DDC_INT_CTRL5.SCRAMBLER_STATUS_DDC_REQ_TIMEOUT
		 * is set
		 */
		DEV_ERR(
		"%s: DDC timeout while reading SCRAMBLER STATUS\n",
						__func__);
		ddc_timer_int |= BIT(13);
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL5,
							ddc_timer_int);
		scrambler_timer_off = true;
	}

	/* Disable scrambler status timer if it has been acknowledged */
	if (scrambler_timer_off) {
		u32 regval = DSS_REG_R_ND(ddc_ctrl->io,
						HDMI_HW_DDC_CTRL);
		regval &= 0x300;
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_HW_DDC_CTRL, regval);
	}
end:
	return 0;
} /* hdmi_ddc_isr */

int hdmi_ddc_read(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	struct hdmi_tx_ddc_data *ddc_data)
{
	int rc = 0;

	if (!ddc_ctrl || !ddc_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = hdmi_ddc_read_retry(ddc_ctrl, ddc_data);
	if (!rc)
		return rc;

	if (ddc_data->no_align) {
		rc = hdmi_ddc_read_retry(ddc_ctrl, ddc_data);
	} else {
		ddc_data->request_len = 32 * ((ddc_data->data_len + 31) / 32);
		rc = hdmi_ddc_read_retry(ddc_ctrl, ddc_data);
	}

	return rc;
} /* hdmi_ddc_read */

int hdmi_ddc_read_seg(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	struct hdmi_tx_ddc_data *ddc_data)
{
	int status = 0;
	u32 reg_val, ndx, time_out_count;
	int log_retry_fail;
	int seg_addr = 0x60, seg_num = 0x01;

	if (!ddc_ctrl || !ddc_ctrl->io || !ddc_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		DEV_ERR("%s[%s]: invalid buf\n", __func__, ddc_data->what);
		goto error;
	}

	log_retry_fail = ddc_data->retry != 1;

again:
	status = hdmi_ddc_clear_irq(ddc_ctrl, ddc_data->what);
	if (status)
		goto error;

	/* Ensure Device Address has LSB set to 0 to indicate Slave addr read */
	ddc_data->dev_addr &= 0xFE;

	/*
	 * 1. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #1
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress (primary link address and writing)
	 *    INDEX = 0x0 (initial offset into buffer)
	 *    INDEX_WRITE = 0x1 (setting initial offset)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA, BIT(31) | (seg_addr << 8));

	/*
	 * 2. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #2
	 *    DATA_RW = 0x0 (write)
	 *    DATA = offsetAddress
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA, seg_num << 8);

	/*
	 * 3. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #3
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress + 1 (primary link address 0x74 and reading)
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA, ddc_data->dev_addr << 8);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA, ddc_data->offset << 8);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		(ddc_data->dev_addr | BIT(0)) << 8);

	/* Data setup is complete, now setup the transaction characteristics */

	/*
	 * 4. Write to HDMI_I2C_TRANSACTION0 with the following fields set in
	 *    order to handle characteristics of portion #1 and portion #2
	 *    RW0 = 0x0 (write)
	 *    START0 = 0x1 (insert START bit)
	 *    STOP0 = 0x0 (do NOT insert STOP bit)
	 *    CNT0 = 0x1 (single byte transaction excluding address)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS0, BIT(12) | BIT(16));

	/*
	 * 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	 *    order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (it's 128 (0x80) for a blk read)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS1, BIT(12) | BIT(16));

	/*
	 * 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	 *    order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (it's 128 (0x80) for a blk read)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS2,
		BIT(0) | BIT(12) | BIT(13) | (ddc_data->request_len << 16));

	/* Trigger the I2C transfer */

	/*
	 * 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x2 (execute transaction0 followed by
	 *    transaction1)
	 *    GO = 0x1 (kicks off hardware)
	 */
	reinit_completion(&ddc_ctrl->ddc_sw_done);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(0) | BIT(21));

	time_out_count = wait_for_completion_timeout(
		&ddc_ctrl->ddc_sw_done, HZ/2);

	reg_val = DSS_REG_R(ddc_ctrl->io, HDMI_DDC_INT_CTRL);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL, reg_val & (~BIT(2)));
	if (!time_out_count) {
		if (ddc_data->retry-- > 0) {
			DEV_INFO("%s: failed timout, retry=%d\n", __func__,
				ddc_data->retry);
			goto again;
		}
		status = -ETIMEDOUT;
		DEV_ERR("%s: timedout(7), Int Ctrl=%08x\n", __func__,
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_INT_CTRL));
		DEV_ERR("%s: DDC SW Status=%08x, HW Status=%08x\n",
			__func__,
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_SW_STATUS),
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_HW_STATUS));
		goto error;
	}

	/* Read DDC status */
	reg_val = DSS_REG_R(ddc_ctrl->io, HDMI_DDC_SW_STATUS);
	reg_val &= BIT(12) | BIT(13) | BIT(14) | BIT(15);

	/* Check if any NACK occurred */
	if (reg_val) {
		/* SW_STATUS_RESET */
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(3));
		if (ddc_data->retry == 1)
			/* SOFT_RESET */
			DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(1));
		if (ddc_data->retry-- > 0) {
			DEV_DBG("%s(%s): failed NACK=0x%08x, retry=%d\n",
				__func__, ddc_data->what, reg_val,
				ddc_data->retry);
			DEV_DBG("%s: daddr=0x%02x,off=0x%02x,len=%d\n",
				__func__, ddc_data->dev_addr,
				ddc_data->offset, ddc_data->data_len);
			goto again;
		}
		status = -EIO;
		if (log_retry_fail) {
			DEV_ERR("%s(%s): failed NACK=0x%08x\n",
				__func__, ddc_data->what, reg_val);
			DEV_ERR("%s: daddr=0x%02x,off=0x%02x,len=%d\n",
				__func__, ddc_data->dev_addr,
				ddc_data->offset, ddc_data->data_len);
		}
		goto error;
	}

	/*
	 * 8. ALL data is now available and waiting in the DDC buffer.
	 *    Read HDMI_I2C_DATA with the following fields set
	 *    RW = 0x1 (read)
	 *    DATA = BCAPS (this is field where data is pulled from)
	 *    INDEX = 0x5 (where the data has been placed in buffer by hardware)
	 *    INDEX_WRITE = 0x1 (explicitly define offset)
	 */
	/* Write this data to DDC buffer */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		BIT(0) | (5 << 16) | BIT(31));

	/* Discard first byte */
	DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);

	for (ndx = 0; ndx < ddc_data->data_len; ++ndx) {
		reg_val = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_DATA);
		ddc_data->data_buf[ndx] = (u8) ((reg_val & 0x0000FF00) >> 8);
	}

	DEV_DBG("%s[%s] success\n", __func__, ddc_data->what);

error:
	return status;
} /* hdmi_ddc_read_seg */

int hdmi_ddc_write(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	struct hdmi_tx_ddc_data *ddc_data)
{
	u32 reg_val, ndx;
	int status = 0, retry = 10;
	u32 time_out_count;

	if (!ddc_ctrl || !ddc_ctrl->io || !ddc_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		DEV_ERR("%s[%s]: invalid buf\n", __func__, ddc_data->what);
		goto error;
	}

again:
	status = hdmi_ddc_clear_irq(ddc_ctrl, ddc_data->what);
	if (status)
		goto error;

	/* Ensure Device Address has LSB set to 0 to indicate Slave addr read */
	ddc_data->dev_addr &= 0xFE;

	/*
	 * 1. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #1
	 *    DATA_RW = 0x1 (write)
	 *    DATA = linkAddress (primary link address and writing)
	 *    INDEX = 0x0 (initial offset into buffer)
	 *    INDEX_WRITE = 0x1 (setting initial offset)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
		BIT(31) | (ddc_data->dev_addr << 8));

	/*
	 * 2. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #2
	 *    DATA_RW = 0x0 (write)
	 *    DATA = offsetAddress
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA, ddc_data->offset << 8);

	/*
	 * 3. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #3
	 *    DATA_RW = 0x0 (write)
	 *    DATA = data_buf[ndx]
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware)
	 */
	for (ndx = 0; ndx < ddc_data->data_len; ++ndx)
		DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_DATA,
			((u32)ddc_data->data_buf[ndx]) << 8);

	/* Data setup is complete, now setup the transaction characteristics */

	/*
	 * 4. Write to HDMI_I2C_TRANSACTION0 with the following fields set in
	 *    order to handle characteristics of portion #1 and portion #2
	 *    RW0 = 0x0 (write)
	 *    START0 = 0x1 (insert START bit)
	 *    STOP0 = 0x0 (do NOT insert STOP bit)
	 *    CNT0 = 0x1 (single byte transaction excluding address)
	 */
	reg_val = (ddc_data->data_len + 1) << 16;
	reg_val |= BIT(12);
	reg_val |= BIT(13);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS0, reg_val);

	/* Trigger the I2C transfer */
	/*
	 * 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x1 (execute transaction0 followed by
	 *    transaction1)
	 *    GO = 0x1 (kicks off hardware)
	 */
	reinit_completion(&ddc_ctrl->ddc_sw_done);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(0));

	time_out_count = wait_for_completion_timeout(
		&ddc_ctrl->ddc_sw_done, HZ/2);

	reg_val = DSS_REG_R(ddc_ctrl->io, HDMI_DDC_INT_CTRL);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_INT_CTRL, reg_val & (~BIT(2)));
	if (!time_out_count) {
		if (retry-- > 0) {
			DEV_INFO("%s[%s]: failed timout, retry=%d\n", __func__,
				ddc_data->what, retry);
			goto again;
		}
		status = -ETIMEDOUT;
		DEV_ERR("%s[%s]: timedout, Int Ctrl=%08x\n",
			__func__, ddc_data->what,
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_INT_CTRL));
		DEV_ERR("%s: DDC SW Status=%08x, HW Status=%08x\n",
			__func__,
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_SW_STATUS),
			DSS_REG_R(ddc_ctrl->io, HDMI_DDC_HW_STATUS));
		goto error;
	}

	/* Read DDC status */
	reg_val = DSS_REG_R_ND(ddc_ctrl->io, HDMI_DDC_SW_STATUS);
	reg_val &= 0x00001000 | 0x00002000 | 0x00004000 | 0x00008000;

	/* Check if any NACK occurred */
	if (reg_val) {
		if (retry > 1)
			/* SW_STATUS_RESET */
			DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(3));
		else
			/* SOFT_RESET */
			DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(1));

		if (retry-- > 0) {
			DEV_DBG("%s[%s]: failed NACK=%08x, retry=%d\n",
				__func__, ddc_data->what, reg_val, retry);
			msleep(100);
			goto again;
		}
		status = -EIO;
		DEV_ERR("%s[%s]: failed NACK: %08x\n", __func__,
			ddc_data->what, reg_val);
		goto error;
	}

	DEV_DBG("%s[%s] success\n", __func__, ddc_data->what);

error:
	return status;
} /* hdmi_ddc_write */


int hdmi_ddc_abort_transaction(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
		struct hdmi_tx_ddc_data *ddc_data)
{
	int status;

	if (!ddc_ctrl || !ddc_ctrl->io || !ddc_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

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
		DEV_ERR("%s: Bad Parameters\n", __func__);
		return -EINVAL;
	}

	if (data_type >= HDMI_TX_SCDC_MAX) {
		DEV_ERR("%s: Unsupported data type\n", __func__);
		return -EINVAL;
	}

	data.what = hdmi_scdc_reg2string(data_type);
	data.dev_addr = 0xA8;
	data.no_align = true;
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

	rc = hdmi_ddc_read(ctrl, &data);
	if (rc) {
		DEV_ERR("%s: DDC Read failed for %s\n", __func__, data.what);
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
		DEV_ERR("%s: Bad Parameters\n", __func__);
		return -EINVAL;
	}

	if (data_type >= HDMI_TX_SCDC_MAX) {
		DEV_ERR("%s: Unsupported data type\n", __func__);
		return -EINVAL;
	}

	data.what = hdmi_scdc_reg2string(data_type);
	data.dev_addr = 0xA8;
	data.no_align = true;
	data.retry = 1;
	data.data_buf = data_buf;

	switch (data_type) {
	case HDMI_TX_SCDC_SCRAMBLING_ENABLE:
	case HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE:
		rdata.what = "TMDS CONFIG";
		rdata.dev_addr = 0xA8;
		rdata.no_align = true;
		rdata.retry = 2;
		rdata.data_buf = &read_val;
		rdata.data_len = 1;
		rdata.offset = HDMI_SCDC_TMDS_CONFIG;
		rdata.request_len = 1;
		rc = hdmi_ddc_read(ctrl, &rdata);
		if (rc) {
			DEV_ERR("%s: scdc read failed\n", __func__);
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
		DEV_ERR("%s Cannot write to read only reg (%d)\n", __func__,
			data_type);
		return -EINVAL;
	}

	rc = hdmi_ddc_write(ctrl, &data);
	if (rc) {
		DEV_ERR("%s: DDC Read failed for %s\n", __func__, data.what);
		return rc;
	}

	return 0;
}

int hdmi_setup_ddc_timers(struct hdmi_tx_ddc_ctrl *ctrl,
			  u32 type, u32 to_in_num_lines)
{
	if (!ctrl) {
		DEV_ERR("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (type >= HDMI_TX_DDC_TIMER_MAX) {
		DEV_ERR("%s: Invalid timer type %d\n", __func__, type);
		return -EINVAL;
	}

	switch (type) {
	case HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS:
		hdmi_scrambler_status_timer_setup(ctrl, to_in_num_lines);
		break;
	default:
		DEV_ERR("%s: %d type not supported\n", __func__, type);
		return -EINVAL;
	}

	return 0;
}

void hdmi_hdcp2p2_ddc_reset(struct hdmi_tx_ddc_ctrl *ctrl)
{
	u32 reg_val;

	if (!ctrl) {
		DEV_ERR("%s: Invalid parameters\n", __func__);
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
	u32 retry_read = 100;
	bool ddc_hw_not_ready;

	if (!ctrl) {
		DEV_ERR("%s: Invalid parameters\n", __func__);
		return;
	}

	/* Clear RXSTATUS_DDC_DONE interrupt */
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL0, BIT(5));
	ddc_hw_not_ready = true;

	/* Make sure the interrupt is clear */
	do {
		reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL0);
		ddc_hw_not_ready = reg_val & BIT(5);
		if (ddc_hw_not_ready)
			msleep(20);
	} while (ddc_hw_not_ready && --retry_read);

	/* Disable HW DDC access to RxStatus register */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(1) | BIT(0));
	DSS_REG_W(ctrl->io, HDMI_HW_DDC_CTRL, reg_val);
}

int hdmi_hdcp2p2_ddc_read_rxstatus(struct hdmi_tx_ddc_ctrl *ctrl,
	struct hdmi_tx_hdcp2p2_ddc_data *hdcp2p2_ddc_data,
	struct completion *rxstatus_completion)
{
	u32 reg_val;
	u32 reg_field_shift;
	u32 reg_field_mask;
	u32 reg_intr_ack_shift;
	u32 reg_intr_mask_shift;
	u32 timeout;
	u32 rxstatus_read_method;
	u32 rxstatus_bytes;
	bool poll_sink;

	if (!hdcp2p2_ddc_data)
		return -EINVAL;

	/* We return a u32 for all RxStatus bits being read */
	if (hdcp2p2_ddc_data->ddc_data.data_len < sizeof(u32))
		return -EINVAL;

	poll_sink = hdcp2p2_ddc_data->poll_sink;

	/*
	 * Setup shifts and masks based on which RxStatus bits we are dealing
	 * with in this call
	 */
	switch (hdcp2p2_ddc_data->rxstatus_field) {
	case RXSTATUS_MESSAGE_SIZE:
		reg_field_shift = HDCP2P2_RXSTATUS_MESSAGE_SIZE_SHIFT;
		reg_field_mask = HDCP2P2_RXSTATUS_MESSAGE_SIZE_MASK;
		reg_intr_ack_shift =
			HDCP2P2_RXSTATUS_MESSAGE_SIZE_ACK_SHIFT;
		reg_intr_mask_shift =
			HDCP2P2_RXSTATUS_MESSAGE_SIZE_INTR_SHIFT;
		break;
	case RXSTATUS_REAUTH_REQ:
		reg_field_shift = HDCP2P2_RXSTATUS_REAUTH_REQ_SHIFT;
		reg_field_mask = HDCP2P2_RXSTATUS_REAUTH_REQ_MASK;
		reg_intr_ack_shift =
			HDCP2P2_RXSTATUS_REAUTH_REQ_ACK_SHIFT;
		reg_intr_mask_shift =
			HDCP2P2_RXSTATUS_REAUTH_REQ_INTR_SHIFT;
		break;
	case RXSTATUS_READY:
		reg_field_shift = HDCP2P2_RXSTATUS_READY_SHIFT;
		reg_field_mask = HDCP2P2_RXSTATUS_READY_MASK;
		reg_intr_ack_shift =
			HDCP2P2_RXSTATUS_READY_ACK_SHIFT;
		reg_intr_mask_shift =
			HDCP2P2_RXSTATUS_READY_INTR_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	DEV_DBG("%s: Requested msg size, shift %u mask %u ack %u mask %u\n",
			__func__, reg_field_shift, reg_field_mask,
			reg_intr_ack_shift, reg_intr_mask_shift);
	/* Disable short read for now, sinks don't support it */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HDCP2P2_DDC_CTRL);
	reg_val |= BIT(4);
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_CTRL, reg_val);

	/* Clear interrupt status bits */
	reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL0);
	reg_val &= ~BIT(reg_intr_ack_shift);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL0, reg_val);

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
	timeout = hdcp2p2_ddc_data->timer_delay_lines & 0xffff;
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_TIMER_CTRL, timeout);
	/* Set both urgent and hw-timeout fields to the same value */
	DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_TIMER_CTRL2,
			(timeout << 16 | timeout));

	/* Enable the interrupt for the requested field, and enable timeouts */
	reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL0);
	reg_val |= BIT(reg_intr_mask_shift);
	reg_val |= BIT(HDCP2P2_RXSTATUS_DDC_FAILED_INTR_MASK);
	DEV_DBG("%s: Enabling interrupts for req field\n", __func__);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL0, reg_val);

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

	rxstatus_read_method =
		poll_sink ? HDCP2P2_RXSTATUS_HW_DDC_AUTOMATIC_LOOP
			  : HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER;

	if (poll_sink)
		reg_val |= BIT(0);
	else
		reg_val |= (BIT(1) | BIT(0));

	DEV_DBG("%s: Enabling hardware access to rxstatus\n", __func__);
	DSS_REG_W(ctrl->io, HDMI_HW_DDC_CTRL, reg_val);

	if (rxstatus_read_method == HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER) {
		/* If we are using SW_TRIGGER, then go ahead and trigger it */
		DSS_REG_W(ctrl->io, HDMI_HDCP2P2_DDC_SW_TRIGGER, 1);

		/*
		 * Now wait for interrupt bits to be set to indicate that the
		 * register is available to read
		 */
		DEV_DBG("%s: HDMI_DDC_INT_CTRL0 is 0x%x, waiting for ISR\n",
			__func__, DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL0));
		reinit_completion(rxstatus_completion);
		wait_for_completion_timeout(rxstatus_completion,
						msecs_to_jiffies(200));
	}

	/* Make sure no errors occurred during DDC transaction */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HDCP2P2_DDC_STATUS);

	/* Check for NACK0, NACK1, TIMEOUT, ABORT bits */
	reg_val &= (BIT(12) | BIT(14) | BIT(4) | BIT(8));

	if (reg_val) {
		DEV_ERR("%s: DDC transaction error, hdcp2p2_ddc_status 0x%x\n",
			__func__, reg_val);
		return -EIO;
	}

	/* Read the RxStatus field that was requested */
	reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL0);
	rxstatus_bytes = reg_val;
	rxstatus_bytes &= reg_field_mask;
	rxstatus_bytes >>= reg_field_shift;
	memcpy(hdcp2p2_ddc_data->ddc_data.data_buf,
			&rxstatus_bytes, sizeof(rxstatus_bytes));

	/* Read the RxStatus field that was requested */
	/* Write the interrupt ack back */
	reg_val |= BIT(reg_intr_ack_shift);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL0, reg_val);

	/* Clear the ack bits and the DDC_FAILED bit next */
	reg_val = DSS_REG_R(ctrl->io, HDMI_DDC_INT_CTRL0);
	reg_val &= ~BIT(reg_intr_ack_shift);
	reg_val &= ~BIT(HDCP2P2_RXSTATUS_DDC_FAILED_INTR_MASK);
	DSS_REG_W(ctrl->io, HDMI_DDC_INT_CTRL0, reg_val);

	/* Disable hardware access to RxStatus register */
	reg_val = DSS_REG_R(ctrl->io, HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(1) | BIT(0));
	DSS_REG_W(ctrl->io, HDMI_HW_DDC_CTRL, reg_val);

	return 0;
}
