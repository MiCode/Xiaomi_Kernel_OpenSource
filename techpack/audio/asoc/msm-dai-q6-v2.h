/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2012-2017, 2019, 2021 The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_DAI_Q6_PDATA_H__

#define __MSM_DAI_Q6_PDATA_H__

#define MSM_MI2S_SD0 (1 << 0)
#define MSM_MI2S_SD1 (1 << 1)
#define MSM_MI2S_SD2 (1 << 2)
#define MSM_MI2S_SD3 (1 << 3)
#define MSM_MI2S_SD4 (1 << 4)
#define MSM_MI2S_SD5 (1 << 5)
#define MSM_MI2S_SD6 (1 << 6)
#define MSM_MI2S_SD7 (1 << 7)

#define MSM_MI2S_CAP_RX 0
#define MSM_MI2S_CAP_TX 1

#define MSM_PRIM_MI2S_RX 0
#define MSM_PRIM_MI2S_TX 1
#define MSM_SEC_MI2S_RX  2
#define MSM_SEC_MI2S_TX  3
#define MSM_TERT_MI2S_RX 4
#define MSM_TERT_MI2S_TX 5
#define MSM_QUAT_MI2S_RX  6
#define MSM_QUAT_MI2S_TX  7
#define MSM_QUIN_MI2S_RX  8
#define MSM_QUIN_MI2S_TX  9
#define MSM_SENARY_MI2S_RX  10
#define MSM_SENARY_MI2S_TX  11
#define MSM_SEC_MI2S_SD1  12
#define MSM_INT0_MI2S_RX  13
#define MSM_INT0_MI2S_TX  14
#define MSM_INT1_MI2S_RX  15
#define MSM_INT1_MI2S_TX  16
#define MSM_INT2_MI2S_RX  17
#define MSM_INT2_MI2S_TX  18
#define MSM_INT3_MI2S_RX  19
#define MSM_INT3_MI2S_TX  20
#define MSM_INT4_MI2S_RX  21
#define MSM_INT4_MI2S_TX  22
#define MSM_INT5_MI2S_RX  23
#define MSM_INT5_MI2S_TX  24
#define MSM_INT6_MI2S_RX  25
#define MSM_INT6_MI2S_TX  26
#define MSM_MI2S_MIN MSM_PRIM_MI2S_RX
#define MSM_MI2S_MAX MSM_INT6_MI2S_TX

#define MSM_DISPLAY_PORT	0
#define MSM_DISPLAY_PORT1	1

#define MSM_PRIM_META_MI2S 0
#define MSM_SEC_META_MI2S  1
#define MSM_META_MI2S_MIN  MSM_PRIM_META_MI2S
#define MSM_META_MI2S_MAX  MSM_SEC_META_MI2S

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
	u16 sd_lines;
	u16 intf_id;
};

struct msm_meta_mi2s_pdata {
	u32 num_member_ports;
	u32 member_port[MAX_NUM_I2S_META_PORT_MEMBER_PORTS];
	u32 sd_lines[MAX_NUM_I2S_META_PORT_MEMBER_PORTS];
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
