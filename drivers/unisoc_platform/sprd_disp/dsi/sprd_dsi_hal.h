/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DSI_HAL_H_
#define _SPRD_DSI_HAL_H_

#include <linux/delay.h>
#include <linux/types.h>

#include "sprd_dsi.h"

/*
 * Get DSI Host core version
 * @dsi pointer to structure holding the DSI Host core information
 * @return true or false
 */
static inline bool dsi_hal_check_version(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (!ops->check_version)
		return false;

	return ops->check_version(ctx);
}
/*
 * Modify power status of DSI Host core
 * @dsi pointer to structure holding the DSI Host core information
 * @enable power on(1) or off (0)
 */
static inline void dsi_hal_power_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->power_en)
		ops->power_en(ctx, enable);
}
/*
 * Enable DPI video mode
 * @dsi pointer to structure holding the DSI Host core information
 */
static inline void dsi_hal_video_mode(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->video_mode)
		ops->video_mode(ctx);
}
/*
 * Enable command mode (Generic interface)
 * @dsi pointer to structure holding the DSI Host core information
 */
static inline void dsi_hal_cmd_mode(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->cmd_mode)
		ops->cmd_mode(ctx);
}
static inline bool dsi_hal_is_cmd_mode(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_cmd_mode)
		return ops->is_cmd_mode(ctx);

	return true;
}
/*
 * Configure the read back virtual channel for the generic interface
 * @dsi pointer to structure holding the DSI Host core information
 * @vc to listen to on the line
 */
static inline void dsi_hal_rx_vcid(struct sprd_dsi *dsi, u8 vc)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->rx_vcid)
		ops->rx_vcid(ctx, vc);
}
/*
 * Write the DPI video virtual channel destination
 * @dsi pointer to structure holding the DSI Host core information
 * @vc virtual channel
 */
static inline void dsi_hal_video_vcid(struct sprd_dsi *dsi, u8 vc)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->video_vcid)
		ops->video_vcid(ctx, vc);
}
/*
 * Set DPI video mode type (burst/non-burst - with sync pulses or events)
 * @dsi pointer to structure holding the DSI Host core information
 * @type burst mode type: with sync pulse or events
 */
static inline
void dsi_hal_dpi_video_burst_mode(struct sprd_dsi *dsi, int type)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_video_burst_mode)
		ops->dpi_video_burst_mode(ctx, type);
}
/*
 * Set DPI video color coding
 * @dsi pointer to structure holding the DSI Host core information
 * @coding enum (configuration and color depth)
 */
static inline void dsi_hal_dpi_color_coding(struct sprd_dsi *dsi, int coding)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_color_coding)
		ops->dpi_color_coding(ctx, coding);
}
/*
 * Set DPI loosely packetisation video (used only when color depth = 18
 * @dsi pointer to structure holding the DSI Host core information
 * @enable 1 enable, 0 disable
 */
static inline
void dsi_hal_dpi_18_loosely_packet_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_18_loosely_packet_en)
		ops->dpi_18_loosely_packet_en(ctx, enable);
}
/*
 * Set DPI color mode pin polarity
 * @dsi pointer to structure holding the DSI Host core information
 * @active_low (1) or active high (0)
 */
static inline
void dsi_hal_dpi_color_mode_pol(struct sprd_dsi *dsi, int active_low)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_color_mode_pol)
		ops->dpi_color_mode_pol(ctx, active_low);
}
/*
 * Set DPI shut down pin polarity
 * @dsi pointer to structure holding the DSI Host core information
 * @active_low (1) or active high (0)
 */
static inline
void dsi_hal_dpi_shut_down_pol(struct sprd_dsi *dsi, int active_low)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_shut_down_pol)
		ops->dpi_shut_down_pol(ctx, active_low);
}
/*
 * Set DPI horizontal sync pin polarity
 * @dsi pointer to structure holding the DSI Host core information
 * @active_low (1) or active high (0)
 */
static inline
void dsi_hal_dpi_hsync_pol(struct sprd_dsi *dsi, int active_low)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_hsync_pol)
		ops->dpi_hsync_pol(ctx, active_low);
}
/*
 * Set DPI vertical sync pin polarity
 * @dsi pointer to structure holding the DSI Host core information
 * @active_low (1) or active high (0)
 */
static inline
void dsi_hal_dpi_vsync_pol(struct sprd_dsi *dsi, int active_low)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_vsync_pol)
		ops->dpi_vsync_pol(ctx, active_low);
}
/*
 * Set DPI data enable pin polarity
 * @dsi pointer to structure holding the DSI Host core information
 * @active_low (1) or active high (0)
 */
static inline
void dsi_hal_dpi_data_en_pol(struct sprd_dsi *dsi, int active_low)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_data_en_pol)
		ops->dpi_data_en_pol(ctx, active_low);
}
/*
 * Configure the Horizontal Line time
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle taken to transmit the total of the horizontal line
 */
static inline
void dsi_hal_dpi_hline_time(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_hline_time)
		ops->dpi_hline_time(ctx, byte_cycle);
}
/*
 * Configure the Horizontal back porch time
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle taken to transmit the horizontal back porch
 */
static inline
void dsi_hal_dpi_hbp_time(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_hbp_time)
		ops->dpi_hbp_time(ctx, byte_cycle);
}
/*
 * Configure the Horizontal sync time
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle taken to transmit the horizontal sync
 */
static inline
void dsi_hal_dpi_hsync_time(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_hsync_time)
		ops->dpi_hsync_time(ctx, byte_cycle);
}
/*
 * Configure the DPI Interface signal delay time
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle delay time after receiving HSYNC from DPI interface
 *	  to retrieve data from pixel memory
 */
static inline
void dsi_hal_dpi_sig_delay(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_sig_delay)
		ops->dpi_sig_delay(ctx, byte_cycle);
}
/*
 * Configure the vertical active lines of the video stream
 * @dsi pointer to structure holding the DSI Host core information
 * @lines
 */
static inline void dsi_hal_dpi_vact(struct sprd_dsi *dsi, u16 lines)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_vact)
		ops->dpi_vact(ctx, lines);
}
/*
 * Configure the vertical front porch lines of the video stream
 * @dsi pointer to structure holding the DSI Host core information
 * @lines
 */
static inline void dsi_hal_dpi_vfp(struct sprd_dsi *dsi, u16 lines)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_vfp)
		ops->dpi_vfp(ctx, lines);
}
/*
 * Configure the vertical back porch lines of the video stream
 * @dsi pointer to structure holding the DSI Host core information
 * @lines
 */
static inline void dsi_hal_dpi_vbp(struct sprd_dsi *dsi, u16 lines)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_vbp)
		ops->dpi_vbp(ctx, lines);
}
/*
 * Configure the vertical sync lines of the video stream
 * @dsi pointer to structure holding the DSI Host core information
 * @lines
 */
static inline void dsi_hal_dpi_vsync(struct sprd_dsi *dsi, u16 lines)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_vsync)
		ops->dpi_vsync(ctx, lines);
}
/*
 * Enable return to low power mode inside horizontal front porch periods when
 *  timing allows
 * @dsi pointer to structure holding the DSI Host core information
 * @enable (1) - disable (0)
 */
static inline
void dsi_hal_dpi_hporch_lp_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_hporch_lp_en)
		ops->dpi_hporch_lp_en(ctx, enable);
}
/*
 * Enable return to low power mode inside vertical active lines periods when
 *  timing allows
 * @dsi pointer to structure holding the DSI Host core information
 * @enable (1) - disable (0)
 */
static inline
void dsi_hal_dpi_vporch_lp_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_vporch_lp_en)
		ops->dpi_vporch_lp_en(ctx, enable);
}
/*
 * Enable FRAME BTA ACK
 * @dsi pointer to structure holding the DSI Host core information
 * @enable (1) - disable (0)
 */
static inline
void dsi_hal_dpi_frame_ack_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_frame_ack_en)
		ops->dpi_frame_ack_en(ctx, enable);
}
/*
 * Enable null packets (value in null packet size will be taken in calculations)
 * @dsi pointer to structure holding the DSI Host core information
 * @enable (1) - disable (0)
 * note: function retained for backward compatibility (not used from 1.20a on)
 */
static inline
void dsi_hal_dpi_null_packet_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_null_packet_en)
		ops->dpi_null_packet_en(ctx, enable);
}
/*
 * Enable multi packets (value in no of chunks will be taken in calculations)
 * @dsi pointer to structure holding the DSI Host core information
 * @enable (1) - disable (0)
 */
static inline
void dsi_hal_dpi_multi_packet_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_multi_packet_en)
		ops->dpi_multi_packet_en(ctx, enable);
}
/*
 * Write no of chunks to core - taken into consideration only when multi packet
 * is enabled
 * @dsi pointer to structure holding the DSI Host core information
 * @num number of chunks
 */
static inline void dsi_hal_dpi_chunk_num(struct sprd_dsi *dsi, u16 num)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_chunk_num)
		ops->dpi_chunk_num(ctx, num);
}
/*
 * Write the null packet size - will only be taken into account when null
 * packets are enabled.
 * @dsi pointer to structure holding the DSI Host core information
 * @size of null packet
 */
static inline
void dsi_hal_dpi_null_packet_size(struct sprd_dsi *dsi, u16 size)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_null_packet_size)
		ops->dpi_null_packet_size(ctx, size);
}
/*
 * Write video packet size. obligatory for sending video
 * @dsi pointer to structure holding the DSI Host core information
 * @size of video packet - containing information
 */
static inline
void dsi_hal_dpi_video_packet_size(struct sprd_dsi *dsi, u16 size)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->dpi_video_packet_size)
		ops->dpi_video_packet_size(ctx, size);
}
/*
 * Specifiy the size of the packet memory write start/continue
 * @dsi pointer to structure holding the DSI Host core information
 * @size size of the packet
 * note when different than zero (0) eDPI is enabled
 */
static inline void dsi_hal_edpi_max_pkt_size(struct sprd_dsi *dsi, u16 size)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->edpi_max_pkt_size)
		ops->edpi_max_pkt_size(ctx, size);
}

static inline
void dsi_hal_edpi_video_hs_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->edpi_video_hs_en)
		ops->edpi_video_hs_en(ctx, enable);
}

/*
 * Enable tear effect acknowledge
 * @dsi pointer to structure holding the DSI Host core information
 * @enable (1) - disable (0)
 */
static inline
void dsi_hal_tear_effect_ack_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->tear_effect_ack_en)
		ops->tear_effect_ack_en(ctx, enable);
}
/*
 * Enable packets acknowledge request after each packet transmission
 * @dsi pointer to structure holding the DSI Host core information
 * @enable (1) - disable (0)
 */
static inline
void dsi_hal_cmd_ack_request_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->cmd_ack_request_en)
		ops->cmd_ack_request_en(ctx, enable);
}
/*
 * Set DCS command packet transmission to transmission type
 * @dsi pointer to structure holding the DSI Host core information
 * @enable lp transmit in low power
 */
static inline
void dsi_hal_cmd_mode_lp_cmd_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->cmd_mode_lp_cmd_en)
		ops->cmd_mode_lp_cmd_en(ctx, enable);
}
/*
 * Set DCS read command packet transmission to transmission type
 * @dsi pointer to structure holding the DSI Host core information
 * @enable lp transmit in low power
 */
static inline
void dsi_hal_video_mode_lp_cmd_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->video_mode_lp_cmd_en)
		ops->video_mode_lp_cmd_en(ctx, enable);
}

/*
 * Write command header in the generic interface
 * (which also sends DCS commands) as a subset
 * @dsi pointer to structure holding the DSI Host core information
 * @vc of destination
 * @type type of DCS command
 * @wc_lsb (if DCS, it is the DCS command)
 * @wc_msb (only parameter of short DCS packet)
 */
static inline void dsi_hal_set_packet_header(struct sprd_dsi *dsi,
				   u8 vc,
				   u8 type,
				   u8 wc_lsb,
				   u8 wc_msb)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->set_packet_header)
		ops->set_packet_header(ctx, vc, type, wc_lsb, wc_msb);
}
/*
 * Write the payload of the long packet commands
 * @dsi pointer to structure holding the DSI Host core information
 * @payload array of bytes of payload
 */
static inline
void dsi_hal_set_packet_payload(struct sprd_dsi *dsi, u32 payload)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->set_packet_payload)
		ops->set_packet_payload(ctx, payload);
}
/*
 * Write the payload of the long packet commands
 * @dsi pointer to structure holding the DSI Host core information
 * @return 32-bit data to hold read information
 */
static inline u32 dsi_hal_get_rx_payload(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->get_rx_payload)
		return ops->get_rx_payload(ctx);

	return 0;
}

/*
 * Enable Bus Turn-around request
 * @dsi pointer to structure holding the DSI Host core information
 * @enable
 */
static inline void dsi_hal_bta_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->bta_en)
		ops->bta_en(ctx, enable);
}
/*
 * Enable EOTp reception
 * @dsi pointer to structure holding the DSI Host core information
 * @enable
 */
static inline void dsi_hal_eotp_rx_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->eotp_rx_en)
		ops->eotp_rx_en(ctx, enable);
}
/*
 * Enable EOTp transmission
 * @dsi pointer to structure holding the DSI Host core information
 * @enable
 */
static inline void dsi_hal_eotp_tx_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->eotp_tx_en)
		ops->eotp_tx_en(ctx, enable);
}
/*
 * Enable ECC reception, error correction and reporting
 * @dsi pointer to structure holding the DSI Host core information
 * @enable
 */
static inline void dsi_hal_ecc_rx_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->ecc_rx_en)
		ops->ecc_rx_en(ctx, enable);
}
/*
 * Enable CRC reception, error reporting
 * @dsi pointer to structure holding the DSI Host core information
 * @enable
 */
static inline void dsi_hal_crc_rx_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->crc_rx_en)
		ops->crc_rx_en(ctx, enable);
}
/*
 * Get status of read command
 * @dsi pointer to structure holding the DSI Host core information
 * @return 1 if bta was returned
 */
static inline bool dsi_hal_is_bta_returned(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_bta_returned)
		return ops->is_bta_returned(ctx);

	return true;
}
/*
 * Get the FULL status of generic read payload fifo
 * @dsi pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static inline bool dsi_hal_is_rx_payload_fifo_full(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_rx_payload_fifo_full)
		return ops->is_rx_payload_fifo_full(ctx);

	return false;
}
/*
 * Get the EMPTY status of generic read payload fifo
 * @dsi pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static inline bool dsi_hal_is_rx_payload_fifo_empty(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_rx_payload_fifo_empty)
		return ops->is_rx_payload_fifo_empty(ctx);

	return true;
}
/*
 * Get the FULL status of generic write payload fifo
 * @dsi pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static inline bool dsi_hal_is_tx_payload_fifo_full(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_tx_payload_fifo_full)
		return ops->is_tx_payload_fifo_full(ctx);

	return false;
}
/*
 * Get the EMPTY status of generic write payload fifo
 * @dsi pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static inline bool dsi_hal_is_tx_payload_fifo_empty(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_tx_payload_fifo_empty)
		return ops->is_tx_payload_fifo_empty(ctx);

	return true;
}
/*
 * Get the FULL status of generic command fifo
 * @dsi pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static inline bool dsi_hal_is_tx_cmd_fifo_full(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_tx_cmd_fifo_full)
		return ops->is_tx_cmd_fifo_full(ctx);

	return false;
}
/*
 * Get the EMPTY status of generic command fifo
 * @dsi pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static inline bool dsi_hal_is_tx_cmd_fifo_empty(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->is_tx_cmd_fifo_empty)
		return ops->is_tx_cmd_fifo_empty(ctx);

	return true;
}

/* only if DPI */
/*
 * Configure how many cycles of byte clock would the PHY module take
 * to switch data lane from high speed to low power
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle
 */
static inline
void dsi_hal_datalane_hs2lp_config(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->datalane_hs2lp_config)
		ops->datalane_hs2lp_config(ctx, byte_cycle);
}
/*
 * Configure how many cycles of byte clock would the PHY module take
 * to switch the data lane from to low power high speed
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle
 */
static inline
void dsi_hal_datalane_lp2hs_config(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->datalane_lp2hs_config)
		ops->datalane_lp2hs_config(ctx, byte_cycle);
}
/*
 * Configure how many cycles of byte clock would the PHY module take
 * to switch clock lane from high speed to low power
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle
 */
static inline
void dsi_hal_clklane_hs2lp_config(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->clklane_hs2lp_config)
		ops->clklane_hs2lp_config(ctx, byte_cycle);
}
/*
 * Configure how many cycles of byte clock would the PHY module take
 * to switch clock lane from to low power high speed
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle
 */
static inline
void dsi_hal_clklane_lp2hs_config(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->clklane_lp2hs_config)
		ops->clklane_lp2hs_config(ctx, byte_cycle);
}
/*
 * Configure how many cycles of byte clock would the PHY module take
 * to turn the bus around to start receiving
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle
 */
static inline
void dsi_hal_max_read_time(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->max_read_time)
		ops->max_read_time(ctx, byte_cycle);
}
/*
 * Enable the automatic mechanism to stop providing clock in the clock
 * lane when time allows
 * @dsi pointer to structure holding the DSI Host core information
 * @enable
 */
static inline void dsi_hal_nc_clk_en(struct sprd_dsi *dsi, int enable)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->nc_clk_en)
		ops->nc_clk_en(ctx, enable);
}
/*
 * Write transmission escape timeout
 * a safe guard so that the state machine would reset if transmission
 * takes too long
 * @dsi pointer to structure holding the DSI Host core information
 * @div escape clk division
 */
static inline void dsi_hal_tx_escape_division(struct sprd_dsi *dsi, u8 div)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->tx_escape_division)
		ops->tx_escape_division(ctx, div);
}
/* PRESP Time outs */
/*
 * configure timeout divisions (so they would have more clock ticks)
 * @dsi pointer to structure holding the DSI Host core information
 * @div no of hs cycles before transiting back to LP in
 * (lane_clk / byte_clk_division_factor)
 */
static inline
void dsi_hal_timeout_clock_division(struct sprd_dsi *dsi, u8 div)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->timeout_clock_division)
		ops->timeout_clock_division(ctx, div);
}
/*
 * Configure the Low power receive time out
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle count of byte cycles
 */
static inline
void dsi_hal_lp_rx_timeout(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->lp_rx_timeout)
		ops->lp_rx_timeout(ctx, byte_cycle);
}
/*
 * Configure a high speed transmission time out
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle
 */
static inline
void dsi_hal_hs_tx_timeout(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->hs_tx_timeout)
		ops->hs_tx_timeout(ctx, byte_cycle);
}
/**
 * Configure the largest packet that can fit in a line during vlank region
 * when the transmission of commands in lp mode.
 * @dsi pointer to structure holding the DSI Host core information
 * @size, in bytes
 */
static inline
void dsi_hal_vblk_cmd_trans_limit(struct sprd_dsi *dsi, u16 size)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->vblk_cmd_trans_limit)
		ops->vblk_cmd_trans_limit(ctx, size);
}
/*
 * Timeout for peripheral (for controller to stay still) after bus turn around
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle period for which the DWC_mipi_dsi_hal_host keeps
 * the link still, after sending a BTA operation. This period is
 * measured in cycles of lanebyteclk
 */
static inline
void dsi_hal_bta_presp_timeout(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->bta_presp_timeout)
		ops->bta_presp_timeout(ctx, byte_cycle);
}
/*
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission write requests
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle period for which the DWC_mipi_dsi_hal_host keeps
 * the link still, after sending a low power write operation. This period is
 * measured in cycles of lanebyteclk
 */
static inline
void dsi_hal_lp_write_presp_timeout(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->lp_write_presp_timeout)
		ops->lp_write_presp_timeout(ctx, byte_cycle);
}
/*
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission read requests
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle period for which the DWC_mipi_dsi_hal_host keeps
 * the link still, after sending a low power read operation. This period is
 * measured in cycles of lanebyteclk
 */
static inline
void dsi_hal_lp_read_presp_timeout(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->lp_read_presp_timeout)
		ops->lp_read_presp_timeout(ctx, byte_cycle);
}
/*
 * Timeout for peripheral (for controller to stay still) after HS data
 * transmission write requests
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle period for which the DWC_mipi_dsi_hal_host keeps
 * the link still, after sending a high-speed write operation. This period is
 * measured in cycles of lanebyteclk
 */
static inline
void dsi_hal_hs_write_presp_timeout(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->hs_write_presp_timeout)
		ops->hs_write_presp_timeout(ctx, byte_cycle);
}
/*
 * Timeout for peripheral between HS data transmission read requests
 * @dsi pointer to structure holding the DSI Host core information
 * @byte_cycle period for which the DWC_mipi_dsi_hal_host keeps
 * the link still, after sending a high-speed read operation. This period is
 * measured in cycles of lanebyteclk
 */
static inline
void dsi_hal_hs_read_presp_timeout(struct sprd_dsi *dsi, u16 byte_cycle)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->hs_read_presp_timeout)
		ops->hs_read_presp_timeout(ctx, byte_cycle);
}
/*
 * Get the error 0 interrupt register status
 * @dsi pointer to structure holding the DSI Host core information
 * @return error status 0 value
 */
static inline u32 dsi_hal_int0_status(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->int0_status)
		return ops->int0_status(ctx);

	return 0;
}
/*
 * Get the error 1 interrupt register status
 * @dsi pointer to structure holding the DSI Host core information
 * @return error status 1 value
 */
static inline u32 dsi_hal_int1_status(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->int1_status)
		return ops->int1_status(ctx);

	return 0;
}
/*
 * Get the error 2 interrupt register status
 * @dsi pointer to structure holding the DSI Host core information
 * @return error status 2 value
 */
static inline u32 dsi_hal_int2_status(struct sprd_dsi *dsi)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->int2_status)
		return ops->int2_status(ctx);

	return 0;
}
/*
 * Configure MASK (hiding) of interrupts coming from error 0 source
 * @dsi pointer to structure holding the DSI Host core information
 * @mask to be written to the register
 */
static inline void dsi_hal_int0_mask(struct sprd_dsi *dsi, u32 mask)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->int0_mask)
		ops->int0_mask(ctx, mask);
}
/*
 * Configure MASK (hiding) of interrupts coming from error 0 source
 * @dsi pointer to structure holding the DSI Host core information
 * @mask the mask to be written to the register
 */
static inline void dsi_hal_int1_mask(struct sprd_dsi *dsi, u32 mask)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->int1_mask)
		ops->int1_mask(ctx, mask);
}
/*
 * Configure MASK (hiding) of interrupts coming from error 0 source
 * @dsi pointer to structure holding the DSI Host core information
 * @mask the mask to be written to the register
 */
static inline void dsi_hal_int2_mask(struct sprd_dsi *dsi, u32 mask)
{
	const struct dsi_core_ops *ops = dsi->core;
	struct dsi_context *ctx = &dsi->ctx;

	if (ops->int2_mask)
		ops->int2_mask(ctx, mask);
}

#endif /* _SPRD_DSI_HAL_H_ */
