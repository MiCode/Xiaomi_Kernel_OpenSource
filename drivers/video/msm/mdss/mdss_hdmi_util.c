/* Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
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

static char res_buf[RESOLUTION_NAME_STR_LEN];

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
	case HDMI_VFRMT_3840x2160p30_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p30_16_9);
		break;
	case HDMI_VFRMT_3840x2160p25_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p25_16_9);
		break;
	case HDMI_VFRMT_3840x2160p24_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_3840x2160p24_16_9);
		break;
	case HDMI_VFRMT_4096x2160p24_16_9:
		MSM_HDMI_MODES_GET_DETAILS(mode, HDMI_VFRMT_4096x2160p24_16_9);
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
	default:
		ret = hdmi_get_resv_timing_info(mode, id);
	}

	return ret;
}

int hdmi_get_supported_mode(struct msm_hdmi_mode_timing_info *info,
	struct hdmi_util_ds_data *ds_data, u32 mode)
{
	int ret;

	if (!info || !ds_data)
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

	DEV_DBG("%s: buf=%pK, d_len=0x%x, d_addr=0x%x, no_align=%d\n",
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
	u32 reg_val, ndx, time_out_count;
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
	INIT_COMPLETION(ddc_ctrl->ddc_sw_done);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(0) | BIT(20));

	time_out_count = wait_for_completion_timeout(
		&ddc_ctrl->ddc_sw_done, HZ/2);
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

int hdmi_ddc_isr(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	u32 ddc_int_ctrl;

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
	INIT_COMPLETION(ddc_ctrl->ddc_sw_done);
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
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS0, BIT(12) | BIT(16));

	/*
	 * 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	 *    order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (0xN (write N bytes of data))
	 *    Byte count for second transition (excluding the first
	 *    Byte which is usually the address)
	 */
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_TRANS1,
		BIT(13) | ((ddc_data->data_len-1) << 16));

	/* Trigger the I2C transfer */
	/*
	 * 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x1 (execute transaction0 followed by
	 *    transaction1)
	 *    GO = 0x1 (kicks off hardware)
	 */
	INIT_COMPLETION(ddc_ctrl->ddc_sw_done);
	DSS_REG_W_ND(ddc_ctrl->io, HDMI_DDC_CTRL, BIT(0) | BIT(20));

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
