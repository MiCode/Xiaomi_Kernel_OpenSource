// SPDX-License-Identifier: GPL-2.0-only
/*
 * busmonitor.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __LINUX_SPRD_BUSMONITOR_H__
#define __LINUX_SPRD_BUSMONITOR_H__

enum ahb_busmon {
	AHB_CHN_INT = 8,
	AHB_CHN_CFG,
	AHB_ADDR_MIN,
	AHB_ADDR_MAX,
	AHB_ADDR_MASK,
	AHB_DATA_MIN_L32,
	AHB_DATA_MIN_H32,
	AHB_DATA_MAX_L32,
	AHB_DATA_MAX_H32,
	AHB_DATA_MASK_L32,
	AHB_DATA_MASK_H32,
	AHB_CNT_WIN_LEN,
	AHB_PEAK_WIN_LEN,
	AHB_MATCH_ADDR,
	AHB_MATCH_CMD,
	AHB_MATCH_DATA_L32,
	AHB_MATCH_DATA_H32,
	AHB_RTRANS_IN_WIN,
	AHB_RBW_IN_WIN,
	AHB_RLATENCE_IN_WIN,
	AHB_WTRANS_IN_WIN,
	AHB_WBW_IN_WIN,
	AHB_WLATENCE_IN_WIN,
	AHB_PEAKBW_IN_WIN,
	AHB_RESERVED,
	AHB_MON_TRANS_LEN,
	AHB_BUS_PEEK,
	AHB_ADDR_MIN_H32,
	AHB_ADDR_MAX_H32,
	AHB_ADDR_MASK_H32,
	AHB_MATCH_ADDR_H32,
};

enum axi_busmon {
	AXI_CHN_INT = 8,
	AXI_CHN_CFG,
	AXI_ID_CFG,
	AXI_ADDR_MIN,
	AXI_ADDR_MIN_H32,
	AXI_ADDR_MAX,
	AXI_ADDR_MAX_H32,
	AXI_ADDR_MASK,
	AXI_ADDR_MASK_H32,
	AXI_DATA_MIN_L32,
	AXI_DATA_MIN_H32,
	AXI_DATA_MIN_EXT_L32,
	AXI_DATA_MIN_EXT_H32,
	AXI_DATA_MAX_L32,
	AXI_DATA_MAX_H32,
	AXI_DATA_MAX_EXT_L32,
	AXI_DATA_MAX_EXT_H32,
	AXI_DATA_MASK_L32,
	AXI_DATA_MASK_H32,
	AXI_DATA_MASK_EXT_L32,
	AXI_DATA_MASK_EXT_H32,
	AXI_MON_TRANS_LEN,
	AXI_MATCH_ID,
	AXI_MATCH_ADDR,
	AXI_MATCH_ADDR_H32,
	AXI_MATCH_CMD,
	AXI_MATCH_DATA_L32,
	AXI_MATCH_DATA_H32,
	AXI_MATCH_DATA_EXT_L32,
	AXI_MATCH_DATA_EXT_H32,
	AXI_BUS_STATUS,
	AXI_USER_CFG,
	AXI_MATCH_USERID,
};

#endif /*__LINUX_SPRD_BUSMONITOR_H__*/
