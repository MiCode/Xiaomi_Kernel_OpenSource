// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>

#include "sprd_dsi.h"
#include "dsi_ctrl_r1p0.h"

/**
 * Get DSI Host core version
 * @param instance pointer to structure holding the DSI Host core information
 * @return ascii number of the version
 */
static bool dsi_check_version(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	u32 version = readl(&reg->DSI_VERSION);

	if (version == 0x100)
		return true;
	else if (version == 0x200)
		return true;
	else if (version == 0x300)
		return true;
	else
		return false;
}
/**
 * Modify power status of DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param on (1) or off (0)
 */
static void dsi_power_enable(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(enable, &reg->SOFT_RESET);
}
/**
 * Enable/disable DPI video mode
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_video_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(0, &reg->DSI_MODE_CFG);
}
/**
 * Enable command mode (Generic interface)
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_cmd_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(1, &reg->DSI_MODE_CFG);
}

static bool dsi_is_cmd_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return readl(&reg->DSI_MODE_CFG) & BIT(0);
}
/**
 * Configure the read back virtual channel for the generic interface
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc to listen to on the line
 */
static void dsi_rx_vcid(struct dsi_context *ctx, u8 vc)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x1C virtual_channel_id;

	virtual_channel_id.val = readl(&reg->VIRTUAL_CHANNEL_ID);
	virtual_channel_id.bits.gen_rx_vcid = vc;

	writel(virtual_channel_id.val, &reg->VIRTUAL_CHANNEL_ID);
}
/**
 * Write the DPI video virtual channel destination
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc virtual channel
 */
static void dsi_video_vcid(struct dsi_context *ctx, u8 vc)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x1C virtual_channel_id;

	virtual_channel_id.val = readl(&reg->VIRTUAL_CHANNEL_ID);
	virtual_channel_id.bits.video_pkt_vcid = vc;

	writel(virtual_channel_id.val, &reg->VIRTUAL_CHANNEL_ID);
}
/**
 * Set DPI video mode type (burst/non-burst - with sync pulses or events)
 * @param instance pointer to structure holding the DSI Host core information
 * @param type
 * @return error code
 */
static void dsi_dpi_video_burst_mode(struct dsi_context *ctx, int mode)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = readl(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.vid_mode_type = mode;

	writel(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/**
 * Set DPI video color coding
 * @param instance pointer to structure holding the DSI Host core information
 * @param color_coding enum (configuration and color depth)
 * @return error code
 */
static void dsi_dpi_color_coding(struct dsi_context *ctx, int coding)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x20 dpi_video_format;

	dpi_video_format.val = readl(&reg->DPI_VIDEO_FORMAT);
	dpi_video_format.bits.dpi_video_mode_format = coding;

	writel(dpi_video_format.val, &reg->DPI_VIDEO_FORMAT);
}
/**
 * Set DPI loosely packetisation video (used only when color depth = 18
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_dpi_18_loosely_packet_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x20 dpi_video_format;

	dpi_video_format.val = readl(&reg->DPI_VIDEO_FORMAT);
	dpi_video_format.bits.loosely18_en = enable;

	writel(dpi_video_format.val, &reg->DPI_VIDEO_FORMAT);
}
/**
 * Configure the Horizontal Line time
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle taken to transmit the total of the horizontal line
 */
static void dsi_dpi_hline_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C video_line_time;

	video_line_time.val = readl(&reg->VIDEO_LINE_TIME);
	video_line_time.bits.video_line_time = byte_cycle;

	writel(video_line_time.val, &reg->VIDEO_LINE_TIME);
}
/**
 * Configure the Horizontal back porch time
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle taken to transmit the horizontal back porch
 */
static void dsi_dpi_hbp_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x28 video_line_hblk_time;

	video_line_hblk_time.val = readl(&reg->VIDEO_LINE_HBLK_TIME);
	video_line_hblk_time.bits.video_line_hbp_time = byte_cycle;

	writel(video_line_hblk_time.val, &reg->VIDEO_LINE_HBLK_TIME);
}
/**
 * Configure the Horizontal sync time
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle taken to transmit the horizontal sync
 */
static void dsi_dpi_hsync_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x28 video_line_hblk_time;

	video_line_hblk_time.val = readl(&reg->VIDEO_LINE_HBLK_TIME);
	video_line_hblk_time.bits.video_line_hsa_time = byte_cycle;

	writel(video_line_hblk_time.val, &reg->VIDEO_LINE_HBLK_TIME);
}
/**
 * Configure the vertical active lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vact(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x34 video_active_lines;

	video_active_lines.val = readl(&reg->VIDEO_VACTIVE_LINES);
	video_active_lines.bits.vactive_lines = lines;

	writel(video_active_lines.val, &reg->VIDEO_VACTIVE_LINES);
}
/**
 * Configure the vertical front porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vfp(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x30 video_vblk_lines;

	video_vblk_lines.val = readl(&reg->VIDEO_VBLK_LINES);
	video_vblk_lines.bits.vfp_lines = lines;

	writel(video_vblk_lines.val, &reg->VIDEO_VBLK_LINES);
}
/**
 * Configure the vertical back porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vbp(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x30 video_vblk_lines;

	video_vblk_lines.val = readl(&reg->VIDEO_VBLK_LINES);
	video_vblk_lines.bits.vbp_lines = lines;

	writel(video_vblk_lines.val, &reg->VIDEO_VBLK_LINES);
}
/**
 * Configure the vertical sync lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vsync(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x30 video_vblk_lines;

	video_vblk_lines.val = readl(&reg->VIDEO_VBLK_LINES);
	video_vblk_lines.bits.vsa_lines = lines;

	writel(video_vblk_lines.val, &reg->VIDEO_VBLK_LINES);
}
/**
 * Enable return to low power mode inside horizontal front porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_dpi_hporch_lp_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = readl(&reg->VID_MODE_CFG);

	vid_mode_cfg.bits.lp_hfp_en = enable;
	vid_mode_cfg.bits.lp_hbp_en = enable;

	writel(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/**
 * Enable return to low power mode inside vertical active lines periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_dpi_vporch_lp_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = readl(&reg->VID_MODE_CFG);

	vid_mode_cfg.bits.lp_vact_en = enable;
	vid_mode_cfg.bits.lp_vfp_en = enable;
	vid_mode_cfg.bits.lp_vbp_en = enable;
	vid_mode_cfg.bits.lp_vsa_en = enable;

	writel(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/**
 * Enable FRAME BTA ACK
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_dpi_frame_ack_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = readl(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.frame_bta_ack_en = enable;

	writel(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/**
 * Write no of chunks to core - taken into consideration only when multi packet
 * is enabled
 * @param instance pointer to structure holding the DSI Host core information
 * @param no of chunks
 */
static void dsi_dpi_chunk_num(struct dsi_context *ctx, u16 num)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x24 video_pkt_config;

	video_pkt_config.val = readl(&reg->VIDEO_PKT_CONFIG);
	video_pkt_config.bits.video_line_chunk_num = num;

	writel(video_pkt_config.val, &reg->VIDEO_PKT_CONFIG);
}
/**
 * Write the null packet size - will only be taken into account when null
 * packets are enabled.
 * @param instance pointer to structure holding the DSI Host core information
 * @param size of null packet
 * @return error code
 */
static void dsi_dpi_null_packet_size(struct dsi_context *ctx, u16 size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xC0 video_nullpkt_size;

	video_nullpkt_size.val = readl(&reg->VIDEO_NULLPKT_SIZE);
	video_nullpkt_size.bits.video_nullpkt_size = size;

	writel(video_nullpkt_size.val, &reg->VIDEO_NULLPKT_SIZE);
}
/**
 * Write video packet size. obligatory for sending video
 * @param instance pointer to structure holding the DSI Host core information
 * @param size of video packet - containing information
 * @return error code
 */
static void dsi_dpi_video_packet_size(struct dsi_context *ctx, u16 size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x24 video_pkt_config;

	video_pkt_config.val = readl(&reg->VIDEO_PKT_CONFIG);
	video_pkt_config.bits.video_pkt_size = size;

	writel(video_pkt_config.val, &reg->VIDEO_PKT_CONFIG);
}
/**
 * Specifiy the size of the packet memory write start/continue
 * @param instance pointer to structure holding the DSI Host core information
 * @ size of the packet
 * @note when different than zero (0) eDPI is enabled
 */
static void dsi_edpi_max_pkt_size(struct dsi_context *ctx, u16 size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xC4 dcs_wm_pkt_size;

	dcs_wm_pkt_size.val = readl(&reg->DCS_WM_PKT_SIZE);
	dcs_wm_pkt_size.bits.dcs_wm_pkt_size = size;

	writel(dcs_wm_pkt_size.val, &reg->DCS_WM_PKT_SIZE);
}
/**
 * Enable tear effect acknowledge
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_tear_effect_ack_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = readl(&reg->CMD_MODE_CFG);
	cmd_mode_cfg.bits.tear_fx_en = enable;

	writel(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}
/**
 * Enable packets acknowledge request after each packet transmission
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_cmd_ack_request_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = readl(&reg->CMD_MODE_CFG);
	cmd_mode_cfg.bits.ack_rqst_en = enable;

	writel(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}
/**
 * Set DCS command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static void dsi_cmd_mode_lp_cmd_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = readl(&reg->CMD_MODE_CFG);

	cmd_mode_cfg.bits.gen_sw_0p_tx = enable;
	cmd_mode_cfg.bits.gen_sw_1p_tx = enable;
	cmd_mode_cfg.bits.gen_sw_2p_tx = enable;
	cmd_mode_cfg.bits.gen_lw_tx = enable;
	cmd_mode_cfg.bits.dcs_sw_0p_tx = enable;
	cmd_mode_cfg.bits.dcs_sw_1p_tx = enable;
	cmd_mode_cfg.bits.dcs_lw_tx = enable;
	cmd_mode_cfg.bits.max_rd_pkt_size = enable;

	cmd_mode_cfg.bits.gen_sr_0p_tx = enable;
	cmd_mode_cfg.bits.gen_sr_1p_tx = enable;
	cmd_mode_cfg.bits.gen_sr_2p_tx = enable;
	cmd_mode_cfg.bits.dcs_sr_0p_tx = enable;

	writel(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}
/**
 * Set DCS read command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static void dsi_video_mode_lp_cmd_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = readl(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_cmd_en = enable;

	writel(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Write command header in the generic interface
 * (which also sends DCS commands) as a subset
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc of destination
 * @param packet_type (or type of DCS command)
 * @param ls_byte (if DCS, it is the DCS command)
 * @param ms_byte (only parameter of short DCS packet)
 * @return error code
 */
static void dsi_set_packet_header(struct dsi_context *ctx,
				   u8 vc,
				   u8 type,
				   u8 wc_lsb,
				   u8 wc_msb)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x6C gen_hdr;

	gen_hdr.bits.gen_dt = type;
	gen_hdr.bits.gen_vc = vc;
	gen_hdr.bits.gen_wc_lsbyte = wc_lsb;
	gen_hdr.bits.gen_wc_msbyte = wc_msb;

	writel(gen_hdr.val, &reg->GEN_HDR);
}
/**
 * Write the payload of the long packet commands
 * @param instance pointer to structure holding the DSI Host core information
 * @param payload array of bytes of payload
 * @return error code
 */
static void dsi_set_packet_payload(struct dsi_context *ctx, u32 payload)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(payload, &reg->GEN_PLD_DATA);
}
/**
 * Write the payload of the long packet commands
 * @param instance pointer to structure holding the DSI Host core information
 * @param payload pointer to 32-bit array to hold read information
 * @return error code
 */
static u32 dsi_get_rx_payload(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return readl(&reg->GEN_PLD_DATA);
}

/**
 * Enable Bus Turn-around request
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_bta_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(enable, &reg->TA_EN);
}
/**
 * Enable EOTp reception
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_eotp_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xBC eotp_en;

	eotp_en.val = readl(&reg->EOTP_EN);
	eotp_en.bits.rx_eotp_en = enable;

	writel(eotp_en.val, &reg->EOTP_EN);
}
/**
 * Enable EOTp transmission
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_eotp_tx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xBC eotp_en;

	eotp_en.val = readl(&reg->EOTP_EN);
	eotp_en.bits.tx_eotp_en = enable;

	writel(eotp_en.val, &reg->EOTP_EN);
}
/**
 * Enable ECC reception, error correction and reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_ecc_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB4 rx_pkt_check_config;

	rx_pkt_check_config.val = readl(&reg->RX_PKT_CHECK_CONFIG);
	rx_pkt_check_config.bits.rx_pkt_ecc_en = enable;

	writel(rx_pkt_check_config.val, &reg->RX_PKT_CHECK_CONFIG);
}
/**
 * Enable CRC reception, error reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_crc_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xB4 rx_pkt_check_config;

	rx_pkt_check_config.val = readl(&reg->RX_PKT_CHECK_CONFIG);
	rx_pkt_check_config.bits.rx_pkt_crc_en = enable;

	writel(rx_pkt_check_config.val, &reg->RX_PKT_CHECK_CONFIG);
}
/**
 * NOTE: dsi-ctrl-r1p0 only
 *
 * Get status of read command
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if busy
 */
static bool dsi_is_rdcmd_done(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 cmd_mode_status;

	cmd_mode_status.val = readl(&reg->CMD_MODE_STATUS);

	return cmd_mode_status.bits.gen_cmd_rdcmd_done;
}
/**
 * Get the FULL status of generic read payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static bool dsi_is_rx_payload_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 cmd_mode_status;

	cmd_mode_status.val = readl(&reg->CMD_MODE_STATUS);

	return cmd_mode_status.bits.gen_cmd_rdata_fifo_full;
}
/**
 * Get the EMPTY status of generic read payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static bool dsi_is_rx_payload_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 cmd_mode_status;

	cmd_mode_status.val = readl(&reg->CMD_MODE_STATUS);

	return cmd_mode_status.bits.gen_cmd_rdata_fifo_empty;
}
/**
 * Get the FULL status of generic write payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static bool dsi_is_tx_payload_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 cmd_mode_status;

	cmd_mode_status.val = readl(&reg->CMD_MODE_STATUS);

	return cmd_mode_status.bits.gen_cmd_wdata_fifo_full;
}
/**
 * Get the EMPTY status of generic write payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static bool dsi_is_tx_payload_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 cmd_mode_status;

	cmd_mode_status.val = readl(&reg->CMD_MODE_STATUS);

	return cmd_mode_status.bits.gen_cmd_wdata_fifo_empty;
}
/**
 * Get the FULL status of generic command fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static bool dsi_is_tx_cmd_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 cmd_mode_status;

	cmd_mode_status.val = readl(&reg->CMD_MODE_STATUS);

	return cmd_mode_status.bits.gen_cmd_cmd_fifo_full;
}
/**
 * Get the EMPTY status of generic command fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static bool dsi_is_tx_cmd_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 cmd_mode_status;

	cmd_mode_status.val = readl(&reg->CMD_MODE_STATUS);

	return cmd_mode_status.bits.gen_cmd_cmd_fifo_empty;
}

/* only if DPI */
/**
 * DPI interface signal delay config
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle period for waiting after controller receiving HSYNC from
 *       DPI interface to start read pixel data from memory.
 */
static void dsi_dpi_sig_delay(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xD0 video_sig_delay_config;

	video_sig_delay_config.val = readl(&reg->VIDEO_SIG_DELAY_CONFIG);
	video_sig_delay_config.bits.video_sig_delay = byte_cycle;

	writel(video_sig_delay_config.val, &reg->VIDEO_SIG_DELAY_CONFIG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch data lane from high speed to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_datalane_hs2lp_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xAC phy_datalane_time_config;

	phy_datalane_time_config.val = readl(&reg->PHY_DATALANE_TIME_CONFIG);
	phy_datalane_time_config.bits.phy_datalane_hs_to_lp_time = byte_cycle;

	writel(phy_datalane_time_config.val, &reg->PHY_DATALANE_TIME_CONFIG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch the data lane from to low power high speed
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_datalane_lp2hs_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xAC phy_datalane_time_config;

	phy_datalane_time_config.val = readl(&reg->PHY_DATALANE_TIME_CONFIG);
	phy_datalane_time_config.bits.phy_datalane_lp_to_hs_time = byte_cycle;

	writel(phy_datalane_time_config.val, &reg->PHY_DATALANE_TIME_CONFIG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch clock lane from high speed to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_clklane_hs2lp_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA8 phy_clklane_time_config;

	phy_clklane_time_config.val = readl(&reg->PHY_CLKLANE_TIME_CONFIG);
	phy_clklane_time_config.bits.phy_clklane_hs_to_lp_time = byte_cycle;

	writel(phy_clklane_time_config.val, &reg->PHY_CLKLANE_TIME_CONFIG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch clock lane from to low power high speed
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_clklane_lp2hs_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xA8 phy_clklane_time_config;

	phy_clklane_time_config.val = readl(&reg->PHY_CLKLANE_TIME_CONFIG);
	phy_clklane_time_config.bits.phy_clklane_lp_to_hs_time = byte_cycle;

	writel(phy_clklane_time_config.val, &reg->PHY_CLKLANE_TIME_CONFIG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to turn the bus around to start receiving
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_max_read_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(byte_cycle, &reg->MAX_READ_TIME);
}

/**
 * Configure the largest packet that can fit in a line during vlank region
 * when	the transmission of commands in lp mode.
 * @param instance pointer to structure holding the DSI Host core information
 * @param size, in bytes
 * @return error code
 */
static void dsi_vblk_cmd_trans_limit(struct dsi_context *ctx, u16 size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(size, &reg->VBLK_CMD_TRANS_LIMIT);
}

/**
 * Enable the automatic mechanism to stop providing clock in the clock
 * lane when time allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 * @return error code
 */
static void dsi_nc_clk_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 phy_clk_lane_lp_ctrl;

	phy_clk_lane_lp_ctrl.val = readl(&reg->PHY_CLK_LANE_LP_CTRL);
	phy_clk_lane_lp_ctrl.bits.auto_clklane_ctrl_en = enable;

	writel(phy_clk_lane_lp_ctrl.val, &reg->PHY_CLK_LANE_LP_CTRL);
}
/**
 * Write transmission escape timeout
 * a safe guard so that the state machine would reset if transmission
 * takes too long
 * @param instance pointer to structure holding the DSI Host core information
 * @param div
 */
static void dsi_tx_escape_division(struct dsi_context *ctx, u8 div)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(div, &reg->TX_ESC_CLK_CONFIG);
}
/* PRESP Time outs */
/**
 * configure timeout divisions (so they would have more clock ticks)
 * @param instance pointer to structure holding the DSI Host core information
 * @param div no of hs cycles before transiting back to LP in
 *  (lane_clk / div)
 */
static void dsi_timeout_clock_division(struct dsi_context *ctx, u8 div)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(div, &reg->TIMEOUT_CNT_CLK_CONFIG);
}
/**
 * Configure the Low power receive time out
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle (of byte cycles)
 */
static void dsi_lp_rx_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(byte_cycle, &reg->LRX_H_TO_CONFIG);
}
/**
 * Configure a high speed transmission time out
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle (byte cycles)
 */
static void dsi_hs_tx_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(byte_cycle, &reg->HTX_TO_CONFIG);
}
/**
 * Timeout for peripheral (for controller to stay still) after bus turn around
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a BTA operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_bta_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(byte_cycle, &reg->BTA_PRESP_TO_CONFIG);
}
/**
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission write requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a low power write operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_lp_write_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(byte_cycle, &reg->LPWR_PRESP_TO_CONFIG);
}
/**
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission read requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a low power read operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_lp_read_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x4C rd_presp_to_config;

	rd_presp_to_config.val = readl(&reg->RD_PRESP_TO_CONFIG);
	rd_presp_to_config.bits.lprd_presp_to_cnt_limit = byte_cycle;

	writel(rd_presp_to_config.val, &reg->RD_PRESP_TO_CONFIG);
}
/**
 * Timeout for peripheral (for controller to stay still) after HS data
 * transmission write requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a high-speed write operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_hs_write_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x50 hswr_presp_to_config;

	hswr_presp_to_config.val = readl(&reg->HSWR_PRESP_TO_CONFIG);
	hswr_presp_to_config.bits.hswr_presp_to_cnt_limit = byte_cycle;

	writel(hswr_presp_to_config.val, &reg->HSWR_PRESP_TO_CONFIG);
}
/**
 * Timeout for peripheral between HS data transmission read requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a high-speed read operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_hs_read_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x4C rd_presp_to_config;

	rd_presp_to_config.val = readl(&reg->RD_PRESP_TO_CONFIG);
	rd_presp_to_config.bits.hsrd_presp_to_cnt_limit = byte_cycle;

	writel(rd_presp_to_config.val, &reg->RD_PRESP_TO_CONFIG);
}
/**
 * Get the error 0 interrupt register status
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be read from the register
 * @return error status 0 value
 */
static u32 dsi_int0_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x08 protocol_int_sts;

	protocol_int_sts.val = readl(&reg->PROTOCOL_INT_STS);
	writel(protocol_int_sts.val, &reg->PROTOCOL_INT_CLR);

	if (protocol_int_sts.bits.dphy_errors_0)
		pr_err("dphy_err: escape entry error\n");

	if (protocol_int_sts.bits.dphy_errors_1)
		pr_err("dphy_err: lp data transmission sync error\n");

	if (protocol_int_sts.bits.dphy_errors_2)
		pr_err("dphy_err: control error\n");

	if (protocol_int_sts.bits.dphy_errors_3)
		pr_err("dphy_err: LP0 contention error\n");

	if (protocol_int_sts.bits.dphy_errors_4)
		pr_err("dphy_err: LP1 contention error\n");

	if (protocol_int_sts.bits.ack_with_err_0)
		pr_err("ack_err: SoT error\n");

	if (protocol_int_sts.bits.ack_with_err_1)
		pr_err("ack_err: SoT Sync error\n");

	if (protocol_int_sts.bits.ack_with_err_2)
		pr_err("ack_err: EoT Sync error\n");

	if (protocol_int_sts.bits.ack_with_err_3)
		pr_err("ack_err: Escape Mode Entry Command error\n");

	if (protocol_int_sts.bits.ack_with_err_4)
		pr_err("ack_err: LP Transmit Sync error\n");

	if (protocol_int_sts.bits.ack_with_err_5)
		pr_err("ack_err: Peripheral Timeout error\n");

	if (protocol_int_sts.bits.ack_with_err_6)
		pr_err("ack_err: False Control error\n");

	if (protocol_int_sts.bits.ack_with_err_7)
		pr_err("ack_err: reserved (specific to device)\n");

	if (protocol_int_sts.bits.ack_with_err_8)
		pr_err("ack_err: ECC error, single-bit (corrected)\n");

	if (protocol_int_sts.bits.ack_with_err_9)
		pr_err("ack_err: ECC error, multi-bit (not corrected)\n");

	if (protocol_int_sts.bits.ack_with_err_10)
		pr_err("ack_err: checksum error (long packet only)\n");

	if (protocol_int_sts.bits.ack_with_err_11)
		pr_err("ack_err: not recognized DSI data type\n");

	if (protocol_int_sts.bits.ack_with_err_12)
		pr_err("ack_err: DSI VC ID Invalid\n");

	if (protocol_int_sts.bits.ack_with_err_13)
		pr_err("ack_err: invalid transmission length\n");

	if (protocol_int_sts.bits.ack_with_err_14)
		pr_err("ack_err: reserved (specific to device)\n");

	if (protocol_int_sts.bits.ack_with_err_15)
		pr_err("ack_err: DSI protocol violation\n");

	return 0;
}
/**
 * Get the error 1 interrupt register status
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be read from the register
 * @return error status 1 value
 */
static u32 dsi_int1_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x10 internal_int_sts;
	u32 status = 0;

	internal_int_sts.val = readl(&reg->INTERNAL_INT_STS);
	writel(internal_int_sts.val, &reg->INTERNAL_INT_CLR);

	if (internal_int_sts.bits.receive_pkt_size_err)
		pr_err("receive packet size error\n");

	if (internal_int_sts.bits.eotp_not_receive_err)
		pr_err("EoTp packet is not received\n");

	if (internal_int_sts.bits.gen_cmd_cmd_fifo_wr_err)
		pr_err("cmd header-fifo is full\n");

	if (internal_int_sts.bits.gen_cmd_rdata_fifo_rd_err)
		pr_err("cmd read-payload-fifo is empty\n");

	if (internal_int_sts.bits.gen_cmd_rdata_fifo_wr_err)
		pr_err("cmd read-payload-fifo is full\n");

	if (internal_int_sts.bits.gen_cmd_wdata_fifo_wr_err)
		pr_err("cmd write-payload-fifo is full\n");

	if (internal_int_sts.bits.gen_cmd_wdata_fifo_rd_err)
		pr_err("cmd write-payload-fifo is empty\n");

	if (internal_int_sts.bits.dpi_pix_fifo_wr_err) {
		pr_err("DPI pixel-fifo is full\n");
		status |= DSI_INT_STS_NEED_SOFT_RESET;
	}

	if (internal_int_sts.bits.ecc_single_err)
		pr_err("ECC single error in a received packet\n");

	if (internal_int_sts.bits.ecc_multi_err)
		pr_err("ECC multiple error in a received packet\n");

	if (internal_int_sts.bits.crc_err)
		pr_err("CRC error in the received packet payload\n");

	if (internal_int_sts.bits.hs_tx_timeout)
		pr_err("high-speed transmission timeout\n");

	if (internal_int_sts.bits.lp_rx_timeout)
		pr_err("low-power reception timeout\n");

	return status;
}
/**
 * Get the error 1 interrupt register status
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be read from the register
 * @return error status 1 value
 */
static u32 dsi_int2_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	if (readl(&reg->INT_PLL_STS))
		pr_err("pll interrupt\n");

	return 0;
}
/**
 * Configure MASK (hiding) of interrupts coming from error 0 source
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask to be written to the register
 */
static void dsi_int0_mask(struct dsi_context *ctx, u32 mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(mask, &reg->MASK_PROTOCOL_INT);
}
/**
 * Configure MASK (hiding) of interrupts coming from error 1 source
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be written to the register
 */
static void dsi_int1_mask(struct dsi_context *ctx, u32 mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(mask, &reg->MASK_INTERNAL_INT);
}
/**
 * Configure MASK (hiding) of interrupts coming from error 2 source
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be written to the register
 */
static void dsi_int2_mask(struct dsi_context *ctx, u32 mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	writel(mask, &reg->INT_PLL_MSK);
}

const struct dsi_core_ops dsi_ctrl_r1p0_ops = {
	.check_version                  = dsi_check_version,
	.power_en                       = dsi_power_enable,
	.video_mode                     = dsi_video_mode,
	.cmd_mode                       = dsi_cmd_mode,
	.is_cmd_mode                    = dsi_is_cmd_mode,
	.rx_vcid                        = dsi_rx_vcid,
	.video_vcid                     = dsi_video_vcid,
	.dpi_video_burst_mode           = dsi_dpi_video_burst_mode,
	.dpi_color_coding               = dsi_dpi_color_coding,
	.dpi_18_loosely_packet_en       = dsi_dpi_18_loosely_packet_en,
	.dpi_sig_delay                  = dsi_dpi_sig_delay,
	.dpi_hline_time                 = dsi_dpi_hline_time,
	.dpi_hsync_time                 = dsi_dpi_hsync_time,
	.dpi_hbp_time                   = dsi_dpi_hbp_time,
	.dpi_vact                       = dsi_dpi_vact,
	.dpi_vfp                        = dsi_dpi_vfp,
	.dpi_vbp                        = dsi_dpi_vbp,
	.dpi_vsync                      = dsi_dpi_vsync,
	.dpi_hporch_lp_en               = dsi_dpi_hporch_lp_en,
	.dpi_vporch_lp_en               = dsi_dpi_vporch_lp_en,
	.dpi_frame_ack_en               = dsi_dpi_frame_ack_en,
	.dpi_chunk_num                  = dsi_dpi_chunk_num,
	.dpi_null_packet_size           = dsi_dpi_null_packet_size,
	.dpi_video_packet_size          = dsi_dpi_video_packet_size,
	.edpi_max_pkt_size              = dsi_edpi_max_pkt_size,
	.tear_effect_ack_en             = dsi_tear_effect_ack_en,
	.cmd_ack_request_en             = dsi_cmd_ack_request_en,
	.cmd_mode_lp_cmd_en             = dsi_cmd_mode_lp_cmd_en,
	.video_mode_lp_cmd_en           = dsi_video_mode_lp_cmd_en,
	.set_packet_header              = dsi_set_packet_header,
	.set_packet_payload             = dsi_set_packet_payload,
	.get_rx_payload                 = dsi_get_rx_payload,
	.bta_en                         = dsi_bta_en,
	.eotp_rx_en                     = dsi_eotp_rx_en,
	.eotp_tx_en                     = dsi_eotp_tx_en,
	.ecc_rx_en                      = dsi_ecc_rx_en,
	.crc_rx_en                      = dsi_crc_rx_en,
	.is_bta_returned                = dsi_is_rdcmd_done,
	.is_rx_payload_fifo_full        = dsi_is_rx_payload_fifo_full,
	.is_rx_payload_fifo_empty       = dsi_is_rx_payload_fifo_empty,
	.is_tx_payload_fifo_full        = dsi_is_tx_payload_fifo_full,
	.is_tx_payload_fifo_empty       = dsi_is_tx_payload_fifo_empty,
	.is_tx_cmd_fifo_full            = dsi_is_tx_cmd_fifo_full,
	.is_tx_cmd_fifo_empty           = dsi_is_tx_cmd_fifo_empty,
	.datalane_hs2lp_config          = dsi_datalane_hs2lp_config,
	.datalane_lp2hs_config          = dsi_datalane_lp2hs_config,
	.clklane_hs2lp_config           = dsi_clklane_hs2lp_config,
	.clklane_lp2hs_config           = dsi_clklane_lp2hs_config,
	.max_read_time                  = dsi_max_read_time,
	.vblk_cmd_trans_limit           = dsi_vblk_cmd_trans_limit,
	.nc_clk_en                      = dsi_nc_clk_en,
	.tx_escape_division             = dsi_tx_escape_division,
	.timeout_clock_division         = dsi_timeout_clock_division,
	.lp_rx_timeout                  = dsi_lp_rx_timeout,
	.hs_tx_timeout                  = dsi_hs_tx_timeout,
	.bta_presp_timeout              = dsi_bta_presp_timeout,
	.lp_write_presp_timeout         = dsi_lp_write_presp_timeout,
	.lp_read_presp_timeout          = dsi_lp_read_presp_timeout,
	.hs_write_presp_timeout         = dsi_hs_write_presp_timeout,
	.hs_read_presp_timeout          = dsi_hs_read_presp_timeout,
	.int0_status                    = dsi_int0_status,
	.int1_status                    = dsi_int1_status,
	.int2_status                    = dsi_int2_status,
	.int0_mask                      = dsi_int0_mask,
	.int1_mask                      = dsi_int1_mask,
	.int2_mask                      = dsi_int2_mask,
};
