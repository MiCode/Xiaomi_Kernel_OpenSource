/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_DAI_Q6_PDATA_H__

#define __MSM_DAI_Q6_PDATA_H__

#define MSM_MI2S_SD0 (1 << 0)
#define MSM_MI2S_SD1 (1 << 1)
#define MSM_MI2S_SD2 (1 << 2)
#define MSM_MI2S_SD3 (1 << 3)
#define MSM_MI2S_CAP_RX 0
#define MSM_MI2S_CAP_TX 1

#define MSM_PRIM_MI2S 0
#define MSM_SEC_MI2S  1
#define MSM_TERT_MI2S 2
#define MSM_QUAT_MI2S  3
#define MSM_SEC_MI2S_SD1  4
#define MSM_QUIN_MI2S  5
#define MSM_SENARY_MI2S  6
#define MSM_MI2S_MIN MSM_PRIM_MI2S
#define MSM_MI2S_MAX MSM_SENARY_MI2S

struct msm_dai_auxpcm_config {
	u16 mode;
	u16 sync;
	u16 frame;
	u16 quant;
	u16 num_slots;
	u16 *slot_mapping;
	u16 data;
	u32 pcm_clk_rate;
};

struct msm_dai_auxpcm_pdata {
	struct msm_dai_auxpcm_config mode_8k;
	struct msm_dai_auxpcm_config mode_16k;
};

struct msm_mi2s_pdata {
	u16 rx_sd_lines;
	u16 tx_sd_lines;
	u16 intf_id;
};

struct msm_i2s_data {
	u32 capability; /* RX or TX */
	u16 sd_lines;
};

struct msm_dai_tdm_group_config {
	u16 group_id;
	u16 num_ports;
	u16 *port_id;
	u32 clk_rate;
};

struct msm_dai_tdm_config {
	u16 sync_mode;
	u16 sync_src;
	u16 data_out;
	u16 invert_sync;
	u16 data_delay;
	u32 data_align;
	u16 header_start_offset;
	u16 header_width;
	u16 header_num_frame_repeat;
};

struct msm_dai_tdm_pdata {
	struct msm_dai_tdm_group_config group_config;
	struct msm_dai_tdm_config config;
};

#endif
