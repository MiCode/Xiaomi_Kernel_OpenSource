/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_CATALOG_H_
#define _DP_CATALOG_H_

#include <drm/drm_dp_helper.h>
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

#define DP_INTR_MST_DP0_VCPF_SENT	BIT(0)
#define DP_INTR_MST_DP1_VCPF_SENT	BIT(3)

#define DP_MAX_TIME_SLOTS	64

/* stream id */
enum dp_stream_id {
	DP_STREAM_0,
	DP_STREAM_1,
	DP_STREAM_MAX,
};

struct dp_catalog_vsc_sdp_colorimetry {
	struct dp_sdp_header header;
	u8 data[32];
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
	u32 isr;
	u32 isr5;

	void (*state_ctrl)(struct dp_catalog_ctrl *ctrl, u32 state);
	void (*config_ctrl)(struct dp_catalog_ctrl *ctrl, u8 ln_cnt);
	void (*lane_mapping)(struct dp_catalog_ctrl *ctrl, bool flipped,
				char *lane_map);
	void (*lane_pnswap)(struct dp_catalog_ctrl *ctrl, u8 ln_pnswap);
	void (*mainlink_ctrl)(struct dp_catalog_ctrl *ctrl, bool enable);
	void (*set_pattern)(struct dp_catalog_ctrl *ctrl, u32 pattern);
	void (*reset)(struct dp_catalog_ctrl *ctrl);
	void (*usb_reset)(struct dp_catalog_ctrl *ctrl, bool flip);
	bool (*mainlink_ready)(struct dp_catalog_ctrl *ctrl);
	void (*enable_irq)(struct dp_catalog_ctrl *ctrl, bool enable);
	void (*phy_reset)(struct dp_catalog_ctrl *ctrl);
	void (*phy_lane_cfg)(struct dp_catalog_ctrl *ctrl, bool flipped,
				u8 lane_cnt);
	void (*update_vx_px)(struct dp_catalog_ctrl *ctrl, u8 v_level,
				u8 p_level, bool high);
	void (*get_interrupt)(struct dp_catalog_ctrl *ctrl);
	u32 (*read_hdcp_status)(struct dp_catalog_ctrl *ctrl);
	void (*send_phy_pattern)(struct dp_catalog_ctrl *ctrl,
			u32 pattern);
	u32 (*read_phy_pattern)(struct dp_catalog_ctrl *ctrl);
	void (*mst_config)(struct dp_catalog_ctrl *ctrl, bool enable);
	void (*trigger_act)(struct dp_catalog_ctrl *ctrl);
	void (*read_act_complete_sts)(struct dp_catalog_ctrl *ctrl, bool *sts);
	void (*channel_alloc)(struct dp_catalog_ctrl *ctrl,
			u32 ch, u32 ch_start_timeslot, u32 tot_ch_cnt);
	void (*update_rg)(struct dp_catalog_ctrl *ctrl, u32 ch, u32 x_int,
			u32 y_frac_enum);
	void (*channel_dealloc)(struct dp_catalog_ctrl *ctrl,
			u32 ch, u32 ch_start_timeslot, u32 tot_ch_cnt);
	void (*fec_config)(struct dp_catalog_ctrl *ctrl, bool enable);
	void (*mainlink_levels)(struct dp_catalog_ctrl *ctrl, u8 lane_cnt);

	int (*late_phy_init)(struct dp_catalog_ctrl *ctrl,
					u8 lane_cnt, bool flipped);
};

struct dp_catalog_hpd {
	void (*config_hpd)(struct dp_catalog_hpd *hpd, bool en);
	u32 (*get_interrupt)(struct dp_catalog_hpd *hpd);
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

	enum dp_stream_id stream_id;

	void (*init)(struct dp_catalog_audio *audio);
	void (*enable)(struct dp_catalog_audio *audio);
	void (*config_acr)(struct dp_catalog_audio *audio);
	void (*config_sdp)(struct dp_catalog_audio *audio);
	void (*set_header)(struct dp_catalog_audio *audio);
	void (*get_header)(struct dp_catalog_audio *audio);
};

struct dp_dsc_cfg_data {
	bool dsc_en;
	char pps[128];
	u32 pps_len;
	u32 pps_word[32];
	u32 pps_word_len;
	u8 parity[32];
	u8 parity_len;
	u32 parity_word[8];
	u32 parity_word_len;
	u32 slice_per_pkt;
	u32 bytes_per_pkt;
	u32 eol_byte_num;
	u32 be_in_lane;
	u32 dto_en;
	u32 dto_n;
	u32 dto_d;
	u32 dto_count;
};

struct dp_catalog_panel {
	u32 total;
	u32 sync_start;
	u32 width_blanking;
	u32 dp_active;
	u8 *spd_vendor_name;
	u8 *spd_product_description;

	struct dp_catalog_vsc_sdp_colorimetry vsc_colorimetry;
	struct dp_sdp_header dhdr_vsif_sdp;
	struct dp_sdp_header shdr_if_sdp;
	struct drm_msm_ext_hdr_metadata hdr_meta;

	/* TPG */
	u32 hsync_period;
	u32 vsync_period;
	u32 display_v_start;
	u32 display_v_end;
	u32 v_sync_width;
	u32 hsync_ctl;
	u32 display_hctl;

	/* TU */
	u32 dp_tu;
	u32 valid_boundary;
	u32 valid_boundary2;

	u32 misc_val;

	enum dp_stream_id stream_id;

	bool widebus_en;
	struct dp_dsc_cfg_data dsc;

	int (*timing_cfg)(struct dp_catalog_panel *panel);
	void (*config_hdr)(struct dp_catalog_panel *panel, bool en,
		u32 dhdr_max_pkts, bool flush);
	void (*config_sdp)(struct dp_catalog_panel *panel, bool en);
	int (*set_colorspace)(struct dp_catalog_panel *panel,
		 bool vsc_supported);
	void (*tpg_config)(struct dp_catalog_panel *panel, bool enable);
	void (*config_spd)(struct dp_catalog_panel *panel);
	void (*config_misc)(struct dp_catalog_panel *panel);
	void (*config_msa)(struct dp_catalog_panel *panel,
			u32 rate, u32 stream_rate_khz);
	void (*update_transfer_unit)(struct dp_catalog_panel *panel);
	void (*config_ctrl)(struct dp_catalog_panel *panel, u32 cfg);
	void (*config_dto)(struct dp_catalog_panel *panel, bool ack);
	void (*dsc_cfg)(struct dp_catalog_panel *panel);
	void (*pps_flush)(struct dp_catalog_panel *panel);
	void (*dhdr_flush)(struct dp_catalog_panel *panel);
	bool (*dhdr_busy)(struct dp_catalog_panel *panel);
};

struct dp_catalog;
struct dp_catalog_sub {
	u32 (*read)(struct dp_catalog *dp_catalog,
		struct dp_io_data *io_data, u32 offset);
	void (*write)(struct dp_catalog *dp_catalog,
		struct dp_io_data *io_data, u32 offset, u32 data);

	void (*put)(struct dp_catalog *catalog);
};

struct dp_catalog_io {
	struct dp_io_data *dp_ahb;
	struct dp_io_data *dp_aux;
	struct dp_io_data *dp_link;
	struct dp_io_data *dp_p0;
	struct dp_io_data *dp_phy;
	struct dp_io_data *dp_ln_tx0;
	struct dp_io_data *dp_ln_tx1;
	struct dp_io_data *dp_mmss_cc;
	struct dp_io_data *dp_pll;
	struct dp_io_data *usb3_dp_com;
	struct dp_io_data *hdcp_physical;
	struct dp_io_data *dp_p1;
	struct dp_io_data *dp_tcsr;
};

struct dp_catalog {
	struct dp_catalog_aux aux;
	struct dp_catalog_ctrl ctrl;
	struct dp_catalog_audio audio;
	struct dp_catalog_panel panel;
	struct dp_catalog_hpd hpd;

	struct dp_catalog_sub *sub;

	void (*set_exe_mode)(struct dp_catalog *dp_catalog, char *mode);
	int (*get_reg_dump)(struct dp_catalog *dp_catalog,
		char *mode, u8 **out_buf, u32 *out_buf_len);
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
	u8 num_byte = (data > 0xFF) ? 8 : 2;

	for (i = 0; i < num_byte; i++) {
		iData = (data >> i*4) & 0xF;

		ci = iData ^ x1;
		x1 = x0 ^ dp_ecc_get_g1_value(ci);
		x0 = dp_ecc_get_g0_value(ci);
	}

	parity_byte = x1 | (x0 << 4);

	return parity_byte;
}

struct dp_catalog *dp_catalog_get(struct device *dev, struct dp_parser *parser);
void dp_catalog_put(struct dp_catalog *catalog);

struct dp_catalog_sub *dp_catalog_get_v420(struct device *dev,
			struct dp_catalog *catalog, struct dp_catalog_io *io);

struct dp_catalog_sub *dp_catalog_get_v200(struct device *dev,
			struct dp_catalog *catalog, struct dp_catalog_io *io);

#endif /* _DP_CATALOG_H_ */
