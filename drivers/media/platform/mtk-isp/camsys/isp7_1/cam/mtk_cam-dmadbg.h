/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_DMADBG_H
#define __MTK_CAM_RAW_DMADBG_H

#include "mtk_cam-raw_debug.h"

static __maybe_unused struct dma_debug_item dbg_RAWI_R2[] = {
	{0x00000000, "32’h0000"},
	{0x00000100, "state_checksum"},
	{0x00000200, "line_pix_cnt_tmp"},
	{0x00000300, "line_pix_cnt"},
	{0x00000500, "smi_debug_data (case 0)"},
	{0x00010600, "aff(fifo)_debug_data (case 1)"},
	{0x00030600, "aff(fifo)_debug_data (case 3)"},
	{0x01000040, "rawi_r2_smi_port / plane-0 / data-crc"},
	{0x01000041, "rawi_r2_smi_port / plane-0 / addr-crc"},
	{0x00000080, "rawi_r2_smi_port / smi_latency_mon output"},
	{0x000000A0, "rawi_r2_smi_port / plane-0 / { len-cnt, dle-cnt }"},
	{0x000000C0, "rawi_r2_smi_port / plane-0 / maddr_max record"},
	{0x000000C1, "rawi_r2_smi_port / plane-0 / maddr_min record"},
};

static __maybe_unused struct dma_debug_item dbg_RAWI_R2_UFD[] = {
	{0x00000001, "32’h0000"},
	{0x00000101, "state_checksum"},
	{0x00000201, "line_pix_cnt_tmp"},
	{0x00000301, "line_pix_cnt"},
};

static __maybe_unused struct dma_debug_item dbg_IMGO_R1[] = {
	{0x00000019, "32’h0000"},
	{0x00000119, "state_checksum"},
	{0x00000219, "line_pix_cnt_tmp"},
	{0x00000319, "line_pix_cnt"},
	{0x00000819, "smi_debug_data (case 0)"},
	{0x00010719, "aff(fifo)_debug_data (case 1)"},
	{0x00030719, "aff(fifo)_debug_data (case 3)"},

	{0x01000059, "imgo_r1_smi_port / plane-0 (i.e. imgo_r1) / data-crc"},

	{0x0000008B, "imgo_r1_smi_port / smi_latency_mon output"},

	{0x000000AB, "imgo_r1_smi_port / plane-0 / {len-cnt, dle-cnt}"},
	{0x000000AC, "imgo_r1_smi_port / plane-0 / {load_com-cnt, bvalid-cnt}"},

	{0x000013C0, "imgo_r1_smi_port / plane-0 / maddr_max record"},
	{0x000013C1, "imgo_r1_smi_port / plane-0 / maddr_min record"},
};

#endif /*__MTK_CAM_RAW_DMADBG_H*/
