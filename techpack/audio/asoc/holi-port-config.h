/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _HOLI_PORT_CONFIG
#define _HOLI_PORT_CONFIG

#include <soc/swr-common.h>

#define WSA_MSTR_PORT_MASK 0xFF
/*
 * Add port configuration in the format
 *{ si, off1, off2, hstart, hstop, wd_len, bp_mode, bgp_ctrl, lane_ctrl, dir,
 *  stream_type}
 */

static struct port_params rx_frame_params_dsd[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 1,    0xFF, 0xFF, 1, 0x00, 0x00},
	{31, 0,  0,  3,    6,    7,    0,    0xFF, 0, 0x00, 0x00},
	{31, 11, 11, 0xFF, 0xFF, 4,    1,    0xFF, 0, 0x00, 0x00},
	{7,  9,  0,  0xFF, 0xFF, 0xFF, 0xFF, 1,    0, 0x00, 0x00},
	{3,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 3,    0, 0x00, 0x00},
};

/* Headset + PCM Haptics */
static struct port_params rx_frame_params_default[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 1,    0xFF, 0xFF, 1, 0x00, 0x00}, /* HPH/EAR */
	{31, 0,  0,  3,    6,    7,    0,    0xFF, 0, 0x00, 0x00}, /* HPH_CLH */
	{31, 11, 11, 0xFF, 0xFF, 4,    1,    0xFF, 0, 0x00, 0x00}, /* HPH_CMP */
	{7,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0x00, 0x00}, /* LO/AUX */
	{0,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0,    0, 0x00, 0x00}, /* DSD */
	{0x18F, 0, 0, 0x8, 0x8, 0x0F,  0x00, 0,    0, 0x00, 0x01}, /* PCM_OUT */
};

/* Headset(44.1K) + PCM Haptics */
static struct port_params rx_frame_params_44p1KHz[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 1,    0xFF, 0xFF, 1, 0x00, 0x00}, /* HPH/EAR */
	{63, 0,  0,  3,    6,    7,    0,    0xFF, 0, 0x00, 0x00}, /* HPH_CLH */
	{31, 11, 11, 0xFF, 0xFF, 4,    1,    0xFF, 0, 0x00, 0x00}, /* HPH_CMP */
	{3,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0, 0x00, 0x00}, /* LO/AUX */
	{0,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0,    0, 0x00, 0x00}, /* DSD */
	{0x1FF, 0, 0, 0x8, 0x8, 0x0F, 0, 0,    0, 0x00, 0x01}, /* PCM_OUT */
};

/* TX UC1: TX1: 1ch, TX2: 2chs, TX3: 1ch(MBHC) */
static struct port_params tx_frame_params_4p8MHz[SWR_MSTR_PORT_LEN] = {
	{3,  0,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1, 0x00, 0x00}, /* TX1 */
	{3,  2,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0x00, 0x00}, /* TX2 */
	{7,  1,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0x00, 0x00}, /* TX3 */
};

static struct port_params tx_frame_params_wcd937x[SWR_MSTR_PORT_LEN] = {
	{3, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1, 0x00, 0x00}, /* TX1 */
	{3, 1, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0x00, 0x00}, /* TX2 */
	{3,  2,  0,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0x00, 0x00}, /* TX3 */
};

static struct swr_mstr_port_map sm_port_map[] = {
	{VA_MACRO, SWR_UC0, tx_frame_params_4p8MHz},
	{RX_MACRO, SWR_UC0, rx_frame_params_default},
	{RX_MACRO, SWR_UC1, rx_frame_params_dsd},
	{RX_MACRO, SWR_UC2, rx_frame_params_44p1KHz},
};

static struct swr_mstr_port_map sm_port_map_wcd937x[] = {
	{VA_MACRO, SWR_UC0, tx_frame_params_wcd937x},
	{RX_MACRO, SWR_UC0, rx_frame_params_default},
	{RX_MACRO, SWR_UC1, rx_frame_params_dsd},
	{RX_MACRO, SWR_UC2, rx_frame_params_44p1KHz},
};

#endif /* _HOLI_PORT_CONFIG */
