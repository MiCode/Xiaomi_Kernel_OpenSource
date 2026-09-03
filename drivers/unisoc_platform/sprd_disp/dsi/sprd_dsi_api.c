// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>

#include "sprd_dsi_hal.h"

static int dsi_wait_tx_payload_fifo_empty(struct sprd_dsi *dsi)
{
	int timeout;

	for (timeout = 0; timeout < 20000; timeout++) {
		if (dsi_hal_is_tx_payload_fifo_empty(dsi))
			return 0;
		udelay(1);
	}

	pr_err("tx payload fifo is not empty\n");
	return -ETIMEDOUT;
}

static int dsi_wait_tx_cmd_fifo_empty(struct sprd_dsi *dsi)
{
	int timeout;

	for (timeout = 0; timeout < 20000; timeout++) {
		if (dsi_hal_is_tx_cmd_fifo_empty(dsi))
			return 0;
		udelay(1);
	}

	pr_err("tx cmd fifo is not empty\n");
	return -ETIMEDOUT;
}

static int dsi_wait_rd_resp_completed(struct sprd_dsi *dsi)
{
	int timeout;

	for (timeout = 0; timeout < 10000; timeout++) {
		udelay(10);
		if (dsi_hal_is_bta_returned(dsi))
			return 0;
	}

	pr_err("wait read response time out\n");
	return -ETIMEDOUT;
}

static u16 calc_bytes_per_pixel_x100(int coding)
{
	u16 Bpp_x100;

	switch (coding) {
	case COLOR_CODE_16BIT_CONFIG1:
	case COLOR_CODE_16BIT_CONFIG2:
	case COLOR_CODE_16BIT_CONFIG3:
		Bpp_x100 = 200;
		break;
	case COLOR_CODE_18BIT_CONFIG1:
	case COLOR_CODE_18BIT_CONFIG2:
		Bpp_x100 = 225;
		break;
	case COLOR_CODE_24BIT:
		Bpp_x100 = 300;
		break;
	case COLOR_CODE_COMPRESSTION:
		Bpp_x100 = 100;
		break;
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
		Bpp_x100 = 250;
		break;
	case COLOR_CODE_24BIT_YCC422:
		Bpp_x100 = 300;
		break;
	case COLOR_CODE_16BIT_YCC422:
		Bpp_x100 = 200;
		break;
	case COLOR_CODE_30BIT:
		Bpp_x100 = 375;
		break;
	case COLOR_CODE_36BIT:
		Bpp_x100 = 450;
		break;
	case COLOR_CODE_12BIT_YCC420:
		Bpp_x100 = 150;
		break;
	default:
		pr_err("invalid color coding");
		Bpp_x100 = 0;
		break;
	}

	return Bpp_x100;
}

static u8 calc_video_size_step(int coding)
{
	u8 video_size_step;

	switch (coding) {
	case COLOR_CODE_16BIT_CONFIG1:
	case COLOR_CODE_16BIT_CONFIG2:
	case COLOR_CODE_16BIT_CONFIG3:
	case COLOR_CODE_18BIT_CONFIG1:
	case COLOR_CODE_18BIT_CONFIG2:
	case COLOR_CODE_24BIT:
	case COLOR_CODE_COMPRESSTION:
		return video_size_step = 1;
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
	case COLOR_CODE_24BIT_YCC422:
	case COLOR_CODE_16BIT_YCC422:
	case COLOR_CODE_30BIT:
	case COLOR_CODE_36BIT:
	case COLOR_CODE_12BIT_YCC420:
		return video_size_step = 2;
	default:
		pr_err("invalid color coding");
		return 0;
	}
}

static u16 round_video_size(int coding, u16 video_size)
{
	switch (coding) {
	case COLOR_CODE_16BIT_YCC422:
	case COLOR_CODE_24BIT_YCC422:
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
	case COLOR_CODE_12BIT_YCC420:
		/* round up active H pixels to a multiple of 2 */
		if ((video_size % 2) != 0)
			video_size += 1;
		break;
	default:
		break;
	}

	return video_size;
}

#define SPRD_MIPI_DSI_FMT_DSC 0xff
static u32 fmt_to_coding(u32 fmt)
{
	switch (fmt) {
	case MIPI_DSI_FMT_RGB565:
		return COLOR_CODE_16BIT_CONFIG1;
	case MIPI_DSI_FMT_RGB666_PACKED:
		return COLOR_CODE_18BIT_CONFIG1;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB888:
		return COLOR_CODE_24BIT;
	case SPRD_MIPI_DSI_FMT_DSC:
		return COLOR_CODE_COMPRESSTION;
	default:
		DRM_ERROR("Unsupported format (%d)\n", fmt);
		return COLOR_CODE_24BIT;
	}
}

#define ns_to_cycle(ns, byte_clk) \
	DIV_ROUND_UP((ns) * (byte_clk), 1000000)

void sprd_dsi_init(struct sprd_dsi *dsi)
{
	int div;
	struct dsi_context *ctx = &dsi->ctx;
	u64 max_rd_time;
	u16 data_hs2lp, data_lp2hs, clk_hs2lp, clk_lp2hs;

	dsi_hal_power_en(dsi, 0);
	dsi_hal_int0_mask(dsi, 0xffffffff);
	dsi_hal_int1_mask(dsi, 0xffffffff);
	dsi_hal_cmd_mode(dsi);
	dsi_hal_eotp_rx_en(dsi, 0);
	dsi_hal_eotp_tx_en(dsi, 0);
	dsi_hal_ecc_rx_en(dsi, 1);
	dsi_hal_crc_rx_en(dsi, 1);
	dsi_hal_bta_en(dsi, 1);
	dsi_hal_video_vcid(dsi, 0);
	dsi_hal_rx_vcid(dsi, 0);

	div = DIV_ROUND_UP(ctx->byte_clk, ctx->esc_clk);
	dsi_hal_tx_escape_division(dsi, div);
	pr_info("escape clock divider = %d\n", div);

	max_rd_time = ctx->max_rd_time * ctx->byte_clk;
	do_div(max_rd_time, 1000000);
	dsi_hal_max_read_time(dsi, max_rd_time);

	data_hs2lp = ns_to_cycle(ctx->data_hs2lp, ctx->byte_clk);
	data_lp2hs = ns_to_cycle(ctx->data_lp2hs, ctx->byte_clk);
	clk_hs2lp = ns_to_cycle(ctx->clk_hs2lp, ctx->byte_clk);
	clk_lp2hs = ns_to_cycle(ctx->clk_lp2hs, ctx->byte_clk);
	dsi_hal_datalane_hs2lp_config(dsi, data_hs2lp);
	dsi_hal_datalane_lp2hs_config(dsi, data_lp2hs);
	dsi_hal_clklane_hs2lp_config(dsi, clk_hs2lp);
	dsi_hal_clklane_lp2hs_config(dsi, clk_lp2hs);

	dsi_hal_power_en(dsi, 1);
}

/*
 * Close DSI Host driver
 * - Free up resources and shutdown host controller and PHY
 * @param dsi pointer to structure holding the DSI Host core information
 * @return int
 */
void sprd_dsi_fini(struct sprd_dsi *dsi)
{
	dsi_hal_int0_mask(dsi, 0xffffffff);
	dsi_hal_int1_mask(dsi, 0xffffffff);
	dsi_hal_power_en(dsi, 0);
}

int sprd_dsi_vrr_timing(struct sprd_dsi *dsi)
{
	u16 Bpp_x100;
	u32 ratio_x1000;
	u16 hline;
	u8 coding;
	struct dsi_context *ctx = &dsi->ctx;
	struct videomode *vm = &dsi->ctx.vm;

	coding = fmt_to_coding(ctx->format);
	Bpp_x100 = calc_bytes_per_pixel_x100(coding);
	ratio_x1000 = ctx->byte_clk * 1000 / (vm->pixelclock / 1000);
	hline = vm->hactive + vm->hsync_len + vm->hfront_porch +
		vm->hback_porch;
	dsi_hal_dpi_sig_delay(dsi, 95 * hline * ratio_x1000 / 100000);
	dsi_hal_dpi_hline_time(dsi, hline * ratio_x1000 / 1000);
	dsi_hal_dpi_hsync_time(dsi, vm->hsync_len * ratio_x1000 / 1000);
	dsi_hal_dpi_hbp_time(dsi, vm->hback_porch * ratio_x1000 / 1000);
	dsi_hal_dpi_vact(dsi, vm->vactive);
	dsi_hal_dpi_vfp(dsi, vm->vfront_porch);
	dsi_hal_dpi_vbp(dsi, vm->vback_porch);
	dsi_hal_dpi_vsync(dsi, vm->vsync_len);

	return 0;
}

/*
 * Configure DPI video interface
 * - If not in burst mode, it will compute the video and null packet sizes
 * according to necessity
 * - Configure timers for data lanes and/or clock lane to return to LP when
 * bandwidth is not filled by data
 * @param dsi pointer to structure holding the DSI Host core information
 * @param param pointer to video stream-to-send information
 * @return error code
 */
int sprd_dsi_dpi_video(struct sprd_dsi *dsi)
{
	u16 Bpp_x100;
	u16 video_size;
	u32 ratio_x1000;
	u16 null_pkt_size = 0;
	u8 video_size_step;
	u32 hs_to;
	u32 total_bytes;
	u32 bytes_per_chunk;
	u32 chunks = 0;
	u32 bytes_left = 0;
	u32 chunk_overhead;
	const u8 pkt_header = 6;
	u8 coding;
	int div;
	u16 hline;
	struct dsi_context *ctx = &dsi->ctx;
	struct videomode *vm = &dsi->ctx.vm;

	coding = fmt_to_coding(ctx->format);
	video_size = round_video_size(coding, vm->hactive);
	Bpp_x100 = calc_bytes_per_pixel_x100(coding);
	video_size_step = calc_video_size_step(coding);
	ratio_x1000 = ctx->byte_clk * 1000 / (vm->pixelclock / 1000);
	hline = vm->hactive + vm->hsync_len + vm->hfront_porch +
		vm->hback_porch;

	dsi_hal_power_en(dsi, 0);
	dsi_hal_dpi_frame_ack_en(dsi, ctx->frame_ack_en);
	dsi_hal_dpi_color_coding(dsi, coding);
	dsi_hal_dpi_video_burst_mode(dsi, ctx->burst_mode);
	dsi_hal_dpi_sig_delay(dsi, 95 * hline * ratio_x1000 / 100000);
	dsi_hal_dpi_hline_time(dsi, hline * ratio_x1000 / 1000);
	dsi_hal_dpi_hsync_time(dsi, vm->hsync_len * ratio_x1000 / 1000);
	dsi_hal_dpi_hbp_time(dsi, vm->hback_porch * ratio_x1000 / 1000);
	dsi_hal_dpi_vact(dsi, vm->vactive);
	dsi_hal_dpi_vfp(dsi, vm->vfront_porch);
	dsi_hal_dpi_vbp(dsi, vm->vback_porch);
	dsi_hal_dpi_vsync(dsi, vm->vsync_len);
	dsi_hal_vblk_cmd_trans_limit(dsi, 0x80);
	if (!ctx->hporch_lp_disable)
		dsi_hal_dpi_hporch_lp_en(dsi, 1);
	dsi_hal_dpi_vporch_lp_en(dsi, 1);
	dsi_hal_dpi_hsync_pol(dsi, 0);
	dsi_hal_dpi_vsync_pol(dsi, 0);
	dsi_hal_dpi_data_en_pol(dsi, 0);
	dsi_hal_dpi_color_mode_pol(dsi, 0);
	dsi_hal_dpi_shut_down_pol(dsi, 0);

	hs_to = (hline * vm->vactive) + (2 * Bpp_x100) / 100;
	for (div = 0x80; (div < hs_to) && (div > 2); div--) {
		if ((hs_to % div) == 0) {
			dsi_hal_timeout_clock_division(dsi, div);
			dsi_hal_lp_rx_timeout(dsi, hs_to / div);
			dsi_hal_hs_tx_timeout(dsi, hs_to / div);
			break;
		}
	}

	if (ctx->burst_mode == VIDEO_BURST_WITH_SYNC_PULSES) {
		dsi_hal_dpi_video_packet_size(dsi, video_size);
		dsi_hal_dpi_null_packet_size(dsi, 0);
		dsi_hal_dpi_chunk_num(dsi, 0);
	} else {
		/* non burst transmission */
		null_pkt_size = 0;

		/* bytes to be sent - first as one chunk */
		bytes_per_chunk = vm->hactive * Bpp_x100 / 100 + pkt_header;

		/* hline total bytes from the DPI interface */
		total_bytes = (vm->hactive + vm->hfront_porch) *
				ratio_x1000 / ctx->lanes / 1000;

		/* check if the pixels actually fit on the DSI link */
		if (total_bytes < bytes_per_chunk) {
			pr_err("current resolution can not be set\n");
			return -EINVAL;
		}

		chunk_overhead = total_bytes - bytes_per_chunk;

		/* overhead higher than 1 -> enable multi packets */
		if (chunk_overhead > 1) {

			/* multi packets */
			for (video_size = video_size_step;
			     video_size < vm->hactive;
			     video_size += video_size_step) {

				if (vm->hactive * 1000 / video_size % 1000)
					continue;

				chunks = vm->hactive / video_size;
				bytes_per_chunk = Bpp_x100 * video_size / 100
						  + pkt_header;
				if (total_bytes >= (bytes_per_chunk * chunks)) {
					bytes_left = total_bytes -
						     bytes_per_chunk * chunks;
					break;
				}
			}

			/* prevent overflow (unsigned - unsigned) */
			if (bytes_left > (pkt_header * chunks)) {
				null_pkt_size = (bytes_left -
						pkt_header * chunks) / chunks;
				/* avoid register overflow */
				if (null_pkt_size > 1023)
					null_pkt_size = 1023;
			}

		} else {

			/* single packet */
			chunks = 1;

			/* must be a multiple of 4 except 18 loosely */
			for (video_size = vm->hactive;
			    (video_size % video_size_step) != 0;
			     video_size++)
				;
		}

		dsi_hal_dpi_video_packet_size(dsi, video_size);
		dsi_hal_dpi_null_packet_size(dsi, null_pkt_size);
		dsi_hal_dpi_chunk_num(dsi, chunks);
	}

	dsi_hal_int0_mask(dsi, ctx->int0_mask);
	dsi_hal_int1_mask(dsi, ctx->int1_mask);
	dsi_hal_power_en(dsi, 1);

	return 0;
}

void sprd_dsi_edpi_video(struct sprd_dsi *dsi)
{
	const u32 fifo_depth = 1096;
	const u32 word_length = 4;
	struct dsi_context *ctx = &dsi->ctx;
	u32 hactive = ctx->vm.hactive;
	u32 Bpp_x100;
	u32 max_fifo_len;
	u8 coding;

	coding = fmt_to_coding(ctx->format);
	Bpp_x100 = calc_bytes_per_pixel_x100(coding);
	max_fifo_len = word_length * fifo_depth * 100 / Bpp_x100;

	dsi_hal_power_en(dsi, 0);
	dsi_hal_dpi_color_coding(dsi, coding);
	dsi_hal_tear_effect_ack_en(dsi, ctx->te_ack_en);

	if (max_fifo_len > hactive)
		dsi_hal_edpi_max_pkt_size(dsi, hactive);
	else
		dsi_hal_edpi_max_pkt_size(dsi, max_fifo_len);

	dsi_hal_int0_mask(dsi, ctx->int0_mask);
	dsi_hal_int1_mask(dsi, ctx->int1_mask);
	dsi_hal_power_en(dsi, 1);
}

/*
 * Send a packet on the generic interface
 * @param dsi pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type type of command, inserted in first byte of header
 * @param params byte array of command parameters
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to:
 * (param_length / 4) x DSIH_FIFO_ACTIVE_WAIT x register access time
 * @note the controller restricts the sending of .
 * This function will not be able to send Null and Blanking packets due to
 *  controller restriction
 */
int sprd_dsi_wr_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			const u8 *param, u16 len)
{
	u32 payload;
	u8 wc_lsbyte, wc_msbyte;
	int i, j, ret;

	if (vc > 3)
		return -EINVAL;

	/* 1st: for long packet, must config payload first */
	ret = dsi_wait_tx_payload_fifo_empty(dsi);
	if (ret)
		return ret;

	if (len > 2) {
		for (i = 0; i < len; i += j) {
			payload = 0;
			for (j = 0; (j < 4) && ((j + i) < (len)); j++)
				payload |= param[i + j] << (j * 8);

			dsi_hal_set_packet_payload(dsi, payload);
		}
		wc_lsbyte = len & 0xff;
		wc_msbyte = len >> 8;
	} else {
		wc_lsbyte = (len > 0) ? param[0] : 0;
		wc_msbyte = (len > 1) ? param[1] : 0;
	}

	/* 2nd: then set packet header */
	ret = dsi_wait_tx_cmd_fifo_empty(dsi);
	if (ret)
		return ret;

	dsi_hal_set_packet_header(dsi, vc, type, wc_lsbyte, wc_msbyte);

	return 0;
}


/*
 * Send READ packet to peripheral using the generic interface
 * This will force command mode and stop video mode (because of BTA)
 * @param dsi pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type generic command type
 * @param lsb_byte first command parameter
 * @param msb_byte second command parameter
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
int sprd_dsi_rd_pkt(struct sprd_dsi *dsi, u8 vc, u8 type,
			u8 msb_byte, u8 lsb_byte,
			u8 *buffer, u8 bytes_to_read)
{
	int i, ret;
	int count = 0;
	u32 temp;

	if (vc > 3)
		return -EINVAL;

	/* 1st: send read command to peripheral */
	ret = dsi_wait_tx_cmd_fifo_empty(dsi);
	if (ret)
		return ret;

	dsi_hal_set_packet_header(dsi, vc, type, lsb_byte, msb_byte);

	/* 2nd: wait peripheral response completed */
	dsi_wait_rd_resp_completed(dsi);

	/* 3rd: get data from rx payload fifo */
	if (dsi_hal_is_rx_payload_fifo_empty(dsi)) {
		pr_err("rx payload fifo empty\n");
		return -EINVAL;
	}

	for (i = 0; i < 100; i++) {
		temp = dsi_hal_get_rx_payload(dsi);

		if (count < bytes_to_read)
			buffer[count++] = temp & 0xff;
		if (count < bytes_to_read)
			buffer[count++] = (temp >> 8) & 0xff;
		if (count < bytes_to_read)
			buffer[count++] = (temp >> 16) & 0xff;
		if (count < bytes_to_read)
			buffer[count++] = (temp >> 24) & 0xff;

		if (dsi_hal_is_rx_payload_fifo_empty(dsi))
			return count;
	}

	pr_err("read too many buffers\n");
	return -EINVAL;
}

void sprd_dsi_set_work_mode(struct sprd_dsi *dsi, u8 mode)
{
	if (mode == DSI_MODE_CMD)
		dsi_hal_cmd_mode(dsi);
	else
		dsi_hal_video_mode(dsi);
}

int sprd_dsi_get_work_mode(struct sprd_dsi *dsi)
{
	if (dsi_hal_is_cmd_mode(dsi))
		return DSI_MODE_CMD;
	else
		return DSI_MODE_VIDEO;
}

void sprd_dsi_lp_cmd_enable(struct sprd_dsi *dsi, bool enable)
{
	if (dsi_hal_is_cmd_mode(dsi))
		dsi_hal_cmd_mode_lp_cmd_en(dsi, enable);
	else
		dsi_hal_video_mode_lp_cmd_en(dsi, enable);
}

void sprd_dsi_nc_clk_en(struct sprd_dsi *dsi, bool enable)
{
	dsi_hal_nc_clk_en(dsi, enable);
}

void sprd_dsi_state_reset(struct sprd_dsi *dsi)
{
	dsi_hal_power_en(dsi, 0);
	udelay(100);
	dsi_hal_power_en(dsi, 1);
}

u32 sprd_dsi_int_status(struct sprd_dsi *dsi, int index)
{
	u32 status = 0;

	switch (index) {
	case 0:
		status = dsi_hal_int0_status(dsi);
		break;

	case 1:
		status = dsi_hal_int1_status(dsi);
		break;

	case 2:
		status = dsi_hal_int2_status(dsi);
		break;

	default:
		break;
	}

	return status;
}

void sprd_dsi_int_mask(struct sprd_dsi *dsi, int index)
{
	switch (index) {
	case 0:
		dsi_hal_int0_mask(dsi, dsi->ctx.int0_mask);
		break;

	case 1:
		dsi_hal_int1_mask(dsi, dsi->ctx.int1_mask);
		break;

	default:
		break;
	}
}
