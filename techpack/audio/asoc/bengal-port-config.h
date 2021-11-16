/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _BENGAL_PORT_CONFIG
#define _BENGAL_PORT_CONFIG

#include <soc/swr-common.h>

/*
 * Add port configuration in the format
 *{ si, off1, off2, hstart, hstop, wd_len, bp_mode, bgp_ctrl, lane_ctrl}
 */

static struct port_params rx_frame_params_default[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 1,    0xFF, 0xFF, 1},
	{31, 0,  0,  3,    6,    7,    0,    0xFF, 0},
	{31, 11, 11, 0xFF, 0xFF, 4,    1,    0xFF, 0},
	{7,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},
	{0,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0,    0},
};

static struct port_params rx_frame_params_rouleur[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 1,    0xFF, 0xFF, 1},
	{31, 0,  0,  3,    6,    7,    0,    0xFF, 0},
	{31, 1,  0,  0xFF, 0xFF, 4,    1,    0xFF, 0},
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
	{3,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},  /* TX1 */
	{3,  2,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},  /* TX2 */
	{3,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0},  /* TX3 */
};

static struct swr_mstr_port_map sm_port_map[] = {
	{VA_MACRO, SWR_UC0, tx_frame_params_default},
	{RX_MACRO, SWR_UC0, rx_frame_params_default},
	{RX_MACRO, SWR_UC1, rx_frame_params_dsd},
};

static struct swr_mstr_port_map sm_port_map_rouleur[] = {
	{VA_MACRO, SWR_UC0, tx_frame_params_default},
	{RX_MACRO, SWR_UC0, rx_frame_params_rouleur},
	{RX_MACRO, SWR_UC1, rx_frame_params_dsd},
};
#endif /* _BENGAL_PORT_CONFIG */
