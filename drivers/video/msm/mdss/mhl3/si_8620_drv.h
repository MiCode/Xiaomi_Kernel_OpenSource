/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#if !defined(SI_8620_DRV_H)
#define SI_8620_DRV_H

extern uint8_t dev_cap_values[16];

enum pp_16bpp_override_t {
	pp_16bpp_automatic	= 0x00,
	pp_16bpp_override_24bpp	= 0x01,
	pp_16bpp_override_16bpp	= 0x02
};
struct drv_hw_context {
	struct interrupt_info *intr_info;
	uint8_t chip_rev_id;
	uint16_t chip_device_id;
	uint8_t cbus_status;
	uint8_t gen2_write_burst_rcv;
	uint8_t gen2_write_burst_xmit;
	uint8_t hawb_write_pending;
	enum {
		CBUS1_IDLE_RCV_DISABLED,
		CBUS1_IDLE_RCV_ENABLED,
		CBUS1_IDLE_RCV_PEND,
		CBUS1_MSC_PEND_DLY_RCV_EN,
		CBUS1_MSC_PEND_DLY_RCV_DIS,
		CBUS1_XMIT_PEND_XMIT_RCV_EN,
		CBUS1_XMIT_PEND_XMIT_RCV_PEND
	} cbus1_state;
	uint8_t delayed_hawb_enable_reg_val;
	uint8_t video_path;
	uint8_t video_ready;
	uint8_t mhl_peer_version_stat;
	enum cbus_mode_e cbus_mode;
	uint8_t current_edid_req_blk;
	uint8_t edid_fifo_block_number;
	uint8_t valid_vsif;
#ifdef NEVER_USED
	uint8_t valid_avif;
#endif
	uint8_t rx_hdmi_ctrl2_defval;
	uint8_t aksv[5];
	struct avi_info_frame_t current_avi_info_frame;
	union SI_PACK_THIS_STRUCT vsif_mhl3_or_hdmi_u current_vsif;
	union hw_avi_payload_t outgoingAviPayLoad;
	struct mhl3_vsif_t outgoing_mhl3_vsif;
	uint8_t write_burst_data[MHL_SCRATCHPAD_SIZE];
	struct cbus_req current_cbus_req;
	uint8_t tdm_virt_chan_slot_counts[VC_MAX];
	struct {
		uint16_t received_byte_count;
		unsigned long peer_blk_rx_buf_avail;
		unsigned long peer_blk_rx_buf_max;

#define NUM_BLOCK_INPUT_BUFFERS 8
		uint8_t input_buffers[NUM_BLOCK_INPUT_BUFFERS][256];
		int input_buffer_lengths[NUM_BLOCK_INPUT_BUFFERS];
		uint16_t head;
		uint16_t tail;
	} block_protocol;
	struct si_mhl_callback_api_t callbacks;
	union avif_or_cea_861_dtd_u avif_or_dtd_from_callback;
	union vsif_mhl3_or_hdmi_u vsif_mhl3_or_hdmi_from_callback;
	int hpd_high_callback_status;
#ifdef MANUAL_EDID_FETCH
	uint8_t edid_block[EDID_BLOCK_SIZE];
#endif
	void *input_field_rate_measurement_timer;
	uint8_t idx_pixel_clock_history;
	uint32_t pixel_clock_history[16];
	bool hdcp2_started;
	enum pp_16bpp_override_t pp_16bpp_override;
	uint8_t prev_bist_coc_status[6];
};

bool si_mhl_tx_set_status(struct mhl_dev_context *dev_context,
	bool xstat, uint8_t reg_to_write, uint8_t value);
void *si_mhl_tx_get_sub_payload_buffer(struct mhl_dev_context *dev_context,
	uint8_t size);
bool si_mhl_tx_send_write_burst(struct mhl_dev_context *dev_context,
	void *buffer);
int si_mhl_tx_drv_cbus_ready_for_edid(struct mhl_dev_context *dev_context);
int si_mhl_tx_drv_set_display_mode(struct mhl_dev_context *dev_context,
	enum hpd_high_callback_status status);
int si_mhl_tx_drv_sample_edid_buffer(struct drv_hw_context *hw_context,
	uint8_t *edid_buffer);

void si_set_cbus_mode_leds_impl(enum cbus_mode_e mode_sel,
		const char *func_name, int line_num);
#define si_set_cbus_mode_leds(mode_sel) \
	si_set_cbus_mode_leds_impl(mode_sel, \
		__func__, __LINE__)
void si_dump_important_regs(struct drv_hw_context *hw_context);

int si_mhl_tx_drv_get_pp_16bpp_override(struct mhl_dev_context *dev_context);
void si_mhl_tx_drv_set_pp_16bpp_override(struct mhl_dev_context *dev_context,
	int override);
int si_mhl_tx_drv_get_hpd_status(struct mhl_dev_context *dev_context);
uint32_t si_mhl_tx_drv_get_hdcp2_status(struct mhl_dev_context *dev_context);
#endif /* if !defined(SI_8620_DRV_H) */
