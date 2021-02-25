/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _KONA_PORT_CONFIG
#define _KONA_PORT_CONFIG

#include <soc/swr-common.h>

#define WSA_MSTR_PORT_MASK 0xFF
/*
 * Add port configuration in the format
 *{ si, off1, off2, hstart, hstop, wd_len, bp_mode, bgp_ctrl, lane_ctrl}
 */

static struct port_params wsa_frame_params_default[SWR_MSTR_PORT_LEN] = {
	{7,  1,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{31, 2,  0,    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{63, 12, 31,   0xFF, 0xFF, 0xFF,  0x1, 0xFF, 0xFF},
	{7,  6,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{31, 18, 0,    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{63, 13, 31,   0xFF, 0xFF, 0xFF,  0x1, 0xFF, 0xFF},
	{15, 7,  0,    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{15, 10, 0,    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
};

static struct port_params rx_frame_params_default[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 1,    0xFF, 0xFF, 1},
	{31, 0,  0,  3,    6,    7,    0,    0xFF, 0},
	{31, 11, 11, 0xFF, 0xFF, 4,    1,    0xFF, 0},
	{7,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},
	{0,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0,    0},
};

static struct port_params rx_frame_params_dsd[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 1,    0xFF, 0xFF, 1},
	{31, 0,  0,  3,    6,    7,    0,    0xFF, 0},
	{31, 11, 11, 0xFF, 0xFF, 4,    1,    0xFF, 0},
	{7,  9,  0,  0xFF, 0xFF, 0xFF, 0xFF, 1,    0},
	{3,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 3,    0},
};

/* TX UC1: TX1: 1ch, TX2: 2chs, TX3: 1ch(MBHC) */
static struct port_params tx_frame_params_default[SWR_MSTR_PORT_LEN] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},/* PCM OUT */
	{1,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},  /* TX1 */
	{1,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1},  /* TX2 */
	{3,  2,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},  /* TX3 */
	{3,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1},  /* TX4 */
};

/* TX UC1: TX1: 1ch, TX2: 2chs, TX3: 1ch(MBHC) */
static struct port_params tx_frame_params_v2[SWR_MSTR_PORT_LEN] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},/* PCM OUT */
	{1,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1},  /* TX1 */
	{1,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 2},  /* TX2 */
	{3,  2,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},  /* TX3 */
	{3,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 2},  /* TX4 */
};

static struct port_params tx_frame_params_wcd937x[SWR_MSTR_PORT_LEN] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{3,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},
	{3,  2,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},
	{3,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},
	{3,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},
};

static struct swr_mstr_port_map sm_port_map[] = {
	{TX_MACRO, SWR_UC0, tx_frame_params_default},
	{RX_MACRO, SWR_UC0, rx_frame_params_default},
	{RX_MACRO, SWR_UC1, rx_frame_params_dsd},
	{WSA_MACRO, SWR_UC0, wsa_frame_params_default},
};

static struct swr_mstr_port_map sm_port_map_v2[] = {
	{TX_MACRO, SWR_UC0, tx_frame_params_v2},
	{RX_MACRO, SWR_UC0, rx_frame_params_default},
	{RX_MACRO, SWR_UC1, rx_frame_params_dsd},
	{WSA_MACRO, SWR_UC0, wsa_frame_params_default},
};

static struct swr_mstr_port_map sm_port_map_wcd937x[] = {
	{TX_MACRO, SWR_UC0, tx_frame_params_wcd937x},
	{RX_MACRO, SWR_UC0, rx_frame_params_default},
	{RX_MACRO, SWR_UC1, rx_frame_params_dsd},
	{WSA_MACRO, SWR_UC0, wsa_frame_params_default},
};

#endif /* _KONA_PORT_CONFIG */
