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

struct dp_catalog_aux {
	u32 data;
	u32 isr;

	u32 (*read_data)(struct dp_catalog_aux *aux);
	int (*write_data)(struct dp_catalog_aux *aux);
	int (*write_trans)(struct dp_catalog_aux *aux);
	void (*reset)(struct dp_catalog_aux *aux);
	void (*enable)(struct dp_catalog_aux *aux, bool enable);
	void (*setup)(struct dp_catalog_aux *aux, u32 *aux_cfg);
	void (*get_irq)(struct dp_catalog_aux *aux, bool cmd_busy);
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
	void (*config_msa)(struct dp_catalog_ctrl *ctrl, u32 rate);
	void (*set_pattern)(struct dp_catalog_ctrl *ctrl, u32 pattern);
	void (*reset)(struct dp_catalog_ctrl *ctrl);
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
};

struct dp_catalog_audio {
	u32 data;

	int (*acr_ctrl)(struct dp_catalog_audio *audio);
	int (*stream_sdp)(struct dp_catalog_audio *audio);
	int (*timestamp_sdp)(struct dp_catalog_audio *audio);
	int (*infoframe_sdp)(struct dp_catalog_audio *audio);
	int (*copy_mgmt_sdp)(struct dp_catalog_audio *audio);
	int (*isrc_sdp)(struct dp_catalog_audio *audio);
	int (*setup_sdp)(struct dp_catalog_audio *audio);
};

struct dp_catalog_panel {
	u32 total;
	u32 sync_start;
	u32 width_blanking;
	u32 dp_active;

	int (*timing_cfg)(struct dp_catalog_panel *panel);
};

struct dp_catalog {
	struct dp_catalog_aux aux;
	struct dp_catalog_ctrl ctrl;
	struct dp_catalog_audio audio;
	struct dp_catalog_panel panel;
};

struct dp_catalog *dp_catalog_get(struct device *dev, struct dp_io *io);
void dp_catalog_put(struct dp_catalog *catalog);

#endif /* _DP_CATALOG_H_ */
