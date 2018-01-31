/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _DP_CATALOG_H_
#define _DP_CATALOG_H_

#include <drm/msm_drm.h>

#include "dp_parser.h"

/* interrupts */
#define DP_INTR_HPD		BIT(0)
#define DP_INTR_AUX_I2C_DONE	BIT(3)
#define DP_INTR_WRONG_ADDR	BIT(6)
#define DP_INTR_TIMEOUT		BIT(9)
#define DP_INTR_NACK_DEFER	BIT(12)
#define DP_INTR_WRONG_DATA_CNT	BIT(15)
#define DP_INTR_I2C_NACK	BIT(18)
#define DP_INTR_I2C_DEFER	BIT(21)
#define DP_INTR_PLL_UNLOCKED	BIT(24)
#define DP_INTR_AUX_ERROR	BIT(27)

#define DP_INTR_READY_FOR_VIDEO		BIT(0)
#define DP_INTR_IDLE_PATTERN_SENT	BIT(3)
#define DP_INTR_FRAME_END		BIT(6)
#define DP_INTR_CRC_UPDATED		BIT(9)

struct dp_catalog_hdr_data {
	u32 ext_header_byte0;
	u32 ext_header_byte1;
	u32 ext_header_byte2;
	u32 ext_header_byte3;

	u32 vsc_header_byte0;
	u32 vsc_header_byte1;
	u32 vsc_header_byte2;
	u32 vsc_header_byte3;

	u32 vscext_header_byte0;
	u32 vscext_header_byte1;
	u32 vscext_header_byte2;
	u32 vscext_header_byte3;

	u32 bpc;

	u32 version;
	u32 length;
	u32 pixel_encoding;
	u32 colorimetry;
	u32 dynamic_range;
	u32 content_type;

	struct drm_msm_ext_hdr_metadata hdr_meta;
};

struct dp_catalog_aux {
	u32 data;
	u32 isr;

	u32 (*read_data)(struct dp_catalog_aux *aux);
	int (*write_data)(struct dp_catalog_aux *aux);
	int (*write_trans)(struct dp_catalog_aux *aux);
	int (*clear_trans)(struct dp_catalog_aux *aux, bool read);
	void (*reset)(struct dp_catalog_aux *aux);
	void (*enable)(struct dp_catalog_aux *aux, bool enable);
	void (*update_aux_cfg)(struct dp_catalog_aux *aux,
		struct dp_aux_cfg *cfg, enum dp_phy_aux_config_type type);
	void (*setup)(struct dp_catalog_aux *aux,
			struct dp_aux_cfg *aux_cfg);
	void (*get_irq)(struct dp_catalog_aux *aux, bool cmd_busy);
	void (*clear_hw_interrupts)(struct dp_catalog_aux *aux);
};

struct dp_catalog_ctrl {
	u32 dp_tu;
	u32 valid_boundary;
	u32 valid_boundary2;
	u32 isr;

	void (*state_ctrl)(struct dp_catalog_ctrl *ctrl, u32 state);
	void (*config_ctrl)(struct dp_catalog_ctrl *ctrl, u32 config);
	void (*lane_mapping)(struct dp_catalog_ctrl *ctrl);
	void (*mainlink_ctrl)(struct dp_catalog_ctrl *ctrl, bool enable);
	void (*config_misc)(struct dp_catalog_ctrl *ctrl, u32 cc, u32 tb);
	void (*config_msa)(struct dp_catalog_ctrl *ctrl, u32 rate,
				u32 stream_rate_khz, bool fixed_nvid);
	void (*set_pattern)(struct dp_catalog_ctrl *ctrl, u32 pattern);
	void (*reset)(struct dp_catalog_ctrl *ctrl);
	void (*usb_reset)(struct dp_catalog_ctrl *ctrl, bool flip);
	bool (*mainlink_ready)(struct dp_catalog_ctrl *ctrl);
	void (*enable_irq)(struct dp_catalog_ctrl *ctrl, bool enable);
	void (*hpd_config)(struct dp_catalog_ctrl *ctrl, bool enable);
	void (*phy_reset)(struct dp_catalog_ctrl *ctrl);
	void (*phy_lane_cfg)(struct dp_catalog_ctrl *ctrl, bool flipped,
				u8 lane_cnt);
	void (*update_vx_px)(struct dp_catalog_ctrl *ctrl, u8 v_level,
				u8 p_level);
	void (*get_interrupt)(struct dp_catalog_ctrl *ctrl);
	void (*update_transfer_unit)(struct dp_catalog_ctrl *ctrl);
	u32 (*read_hdcp_status)(struct dp_catalog_ctrl *ctrl);
	void (*send_phy_pattern)(struct dp_catalog_ctrl *ctrl,
			u32 pattern);
	u32 (*read_phy_pattern)(struct dp_catalog_ctrl *ctrl);
};

#define HEADER_BYTE_2_BIT	 0
#define PARITY_BYTE_2_BIT	 8
#define HEADER_BYTE_1_BIT	16
#define PARITY_BYTE_1_BIT	24
#define HEADER_BYTE_3_BIT	16
#define PARITY_BYTE_3_BIT	24

enum dp_catalog_audio_sdp_type {
	DP_AUDIO_SDP_STREAM,
	DP_AUDIO_SDP_TIMESTAMP,
	DP_AUDIO_SDP_INFOFRAME,
	DP_AUDIO_SDP_COPYMANAGEMENT,
	DP_AUDIO_SDP_ISRC,
	DP_AUDIO_SDP_MAX,
};

enum dp_catalog_audio_header_type {
	DP_AUDIO_SDP_HEADER_1,
	DP_AUDIO_SDP_HEADER_2,
	DP_AUDIO_SDP_HEADER_3,
	DP_AUDIO_SDP_HEADER_MAX,
};

struct dp_catalog_audio {
	enum dp_catalog_audio_sdp_type sdp_type;
	enum dp_catalog_audio_header_type sdp_header;
	u32 data;

	void (*init)(struct dp_catalog_audio *audio);
	void (*enable)(struct dp_catalog_audio *audio);
	void (*config_acr)(struct dp_catalog_audio *audio);
	void (*config_sdp)(struct dp_catalog_audio *audio);
	void (*set_header)(struct dp_catalog_audio *audio);
	void (*get_header)(struct dp_catalog_audio *audio);
	void (*safe_to_exit_level)(struct dp_catalog_audio *audio);
};

struct dp_catalog_panel {
	u32 total;
	u32 sync_start;
	u32 width_blanking;
	u32 dp_active;
	u8 *spd_vendor_name;
	u8 *spd_product_description;

	struct dp_catalog_hdr_data hdr_data;

	/* TPG */
	u32 hsync_period;
	u32 vsync_period;
	u32 display_v_start;
	u32 display_v_end;
	u32 v_sync_width;
	u32 hsync_ctl;
	u32 display_hctl;

	int (*timing_cfg)(struct dp_catalog_panel *panel);
	void (*config_hdr)(struct dp_catalog_panel *panel, bool en);
	void (*tpg_config)(struct dp_catalog_panel *panel, bool enable);
	void (*config_spd)(struct dp_catalog_panel *panel);
};

struct dp_catalog {
	struct dp_catalog_aux aux;
	struct dp_catalog_ctrl ctrl;
	struct dp_catalog_audio audio;
	struct dp_catalog_panel panel;
};

static inline u8 dp_ecc_get_g0_value(u8 data)
{
	u8 c[4];
	u8 g[4];
	u8 ret_data = 0;
	u8 i;

	for (i = 0; i < 4; i++)
		c[i] = (data >> i) & 0x01;

	g[0] = c[3];
	g[1] = c[0] ^ c[3];
	g[2] = c[1];
	g[3] = c[2];

	for (i = 0; i < 4; i++)
		ret_data = ((g[i] & 0x01) << i) | ret_data;

	return ret_data;
}

static inline u8 dp_ecc_get_g1_value(u8 data)
{
	u8 c[4];
	u8 g[4];
	u8 ret_data = 0;
	u8 i;

	for (i = 0; i < 4; i++)
		c[i] = (data >> i) & 0x01;

	g[0] = c[0] ^ c[3];
	g[1] = c[0] ^ c[1] ^ c[3];
	g[2] = c[1] ^ c[2];
	g[3] = c[2] ^ c[3];

	for (i = 0; i < 4; i++)
		ret_data = ((g[i] & 0x01) << i) | ret_data;

	return ret_data;
}

static inline u8 dp_header_get_parity(u32 data)
{
	u8 x0 = 0;
	u8 x1 = 0;
	u8 ci = 0;
	u8 iData = 0;
	u8 i = 0;
	u8 parity_byte;
	u8 num_byte = (data & 0xFF00) > 0 ? 8 : 2;

	for (i = 0; i < num_byte; i++) {
		iData = (data >> i*4) & 0xF;

		ci = iData ^ x1;
		x1 = x0 ^ dp_ecc_get_g1_value(ci);
		x0 = dp_ecc_get_g0_value(ci);
	}

	parity_byte = x1 | (x0 << 4);

	return parity_byte;
}

struct dp_catalog *dp_catalog_get(struct device *dev, struct dp_io *io);
void dp_catalog_put(struct dp_catalog *catalog);

#endif /* _DP_CATALOG_H_ */
