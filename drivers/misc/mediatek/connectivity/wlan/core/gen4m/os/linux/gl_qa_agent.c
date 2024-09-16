/*******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
/*
	Module Name:
	gl_ate_agent.c
*/
/*******************************************************************************
 *				C O M P I L E R	 F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *				E X T E R N A L	R E F E R E N C E S
 *******************************************************************************
 */

#include "precomp.h"
#if (CFG_SUPPORT_QA_TOOL == 1)
#include "gl_wext.h"
#include "gl_cfg80211.h"
#include "gl_ate_agent.h"
#include "gl_qa_agent.h"
#include "gl_hook_api.h"
#if KERNEL_VERSION(3, 8, 0) <= CFG80211_VERSION_CODE
#include <uapi/linux/nl80211.h>
#endif
#if (CONFIG_WLAN_SERVICE == 1)
#include "agent.h"
#endif

/*******************************************************************************
 *				C O N S T A N T S
 *******************************************************************************
 */

struct PARAM_RX_STAT g_HqaRxStat;
uint32_t u4RxStatSeqNum;
u_int8_t g_DBDCEnable = FALSE;
/* For SA Buffer Mode Temp Solution */
u_int8_t	g_BufferDownload = FALSE;
uint32_t	u4EepromMode = 4;
uint32_t g_u4Chip_ID;

static struct hqa_rx_stat_band_format g_backup_band0_info;
static struct hqa_rx_stat_band_format g_backup_band1_info;

#if CFG_SUPPORT_BUFFER_MODE
uint8_t	uacEEPROMImage[MAX_EEPROM_BUFFER_SIZE] = {
	/* 0x000 ~ 0x00F */
	0xAE, 0x86, 0x06, 0x00, 0x18, 0x0D, 0x00, 0x00,
	0xC0, 0x1F, 0xBD, 0x81, 0x3F, 0x01, 0x19, 0x00,
	/* 0x010 ~ 0x01F */
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
	/* 0x020 ~ 0x02F */
	0x80, 0x02, 0x00, 0x00, 0x32, 0x66, 0xC3, 0x14,
	0x32, 0x66, 0xC3, 0x14, 0x03, 0x22, 0xFF, 0xFF,
	/* 0x030 ~ 0x03F */
	0x23, 0x04, 0x0D, 0xF2, 0x8F, 0x02, 0x00, 0x80,
	0x0A, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x040 ~ 0x04F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x33, 0x40, 0x00, 0x00,
	/* 0x050 ~ 0x05F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x08,
	/* 0x060 ~ 0x06F */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x08,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x08,
	/* 0x070 ~ 0x07F */
	0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0xE0, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x080 ~ 0x08F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x090 ~ 0x09F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0A0 ~ 0x0AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0B0 ~ 0x0BF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x92, 0x10, 0x10, 0x28, 0x00, 0x00, 0x00, 0x00,
	/* 0x0C0 ~ 0x0CF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0D0 ~ 0x0DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0E0 ~ 0x0EF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0F0 ~ 0x0FF */
	0x0E, 0x05, 0x06, 0x06, 0x06, 0x0F, 0x00, 0x00,
	0x0E, 0x05, 0x06, 0x05, 0x05, 0x09, 0xFF, 0x00,
	/* 0x100 ~ 0x10F */
	0x12, 0x34, 0x56, 0x78, 0x2C, 0x2C, 0x28, 0x28,
	0x28, 0x26, 0x26, 0x28, 0x28, 0x28, 0x26, 0xFF,
	/* 0x110 ~ 0x11F */
	0x26, 0x25, 0x28, 0x21, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x27, 0x27, 0x27, 0x25,
	/* 0x120 ~ 0x12F */
	0x25, 0x25, 0x25, 0x25, 0x23, 0x23, 0x23, 0x21,
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00,
	/* 0x130 ~ 0x13F */
	0x40, 0x40, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0,
	0xD0, 0xD0, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	/* 0x140 ~ 0x14F */
	0x25, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x150 ~ 0x15F */
	0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0,
	/* 0x160 ~ 0x16F */
	0xD0, 0xD0, 0xD0, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x170 ~ 0x17F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xC2, 0xC4, 0xC5, 0xC8,
	/* 0x180 ~ 0x18F */
	0x00, 0x26, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x190 ~ 0x19F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x1A0 ~ 0x1AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0xD0,
	0xD0, 0x0E, 0x05, 0x06, 0x05, 0x09, 0x0E, 0x00,
	/* 0x1B0 ~ 0x1BF */
	0x05, 0x06, 0x05, 0x05, 0x09, 0x00, 0x00, 0x00,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	/* 0x1C0 ~ 0x1CF */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00,
	/* 0x1D0 ~ 0x1DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x1E0 ~ 0x1EF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x1F0 ~ 0x1FF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x200 ~ 0x20F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x210 ~ 0x21F */
	0x48, 0xF5, 0x27, 0x49, 0x48, 0xF5, 0x57, 0x12,
	0x4B, 0x71, 0x80, 0x50, 0x91, 0xF6, 0x87, 0x50,
	/* 0x220 ~ 0x22F */
	0x7D, 0x29, 0x09, 0x42, 0x7D, 0x29, 0x41, 0x44,
	0x7D, 0x29, 0x41, 0x3C, 0x7D, 0x29, 0x31, 0x4D,
	/* 0x230 ~ 0x23F */
	0x49, 0x71, 0x24, 0x49, 0x49, 0x71, 0x54, 0x12,
	0x4B, 0x71, 0x80, 0x50, 0x91, 0xF6, 0x87, 0x50,
	/* 0x240 ~ 0x24F */
	0x7D, 0x29, 0x09, 0x42, 0x7D, 0x29, 0x41, 0x04,
	0x7D, 0x29, 0x41, 0x04, 0x7D, 0x29, 0x01, 0x40,
	/* 0x250 ~ 0x25F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x260 ~ 0x26F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x270 ~ 0x27F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x280 ~ 0x28F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x290 ~ 0x29F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2A0 ~ 0x2AF */
	0x7D, 0x29, 0xC9, 0x16, 0x7D, 0x29, 0xC9, 0x16,
	0x44, 0x22, 0x32, 0x15, 0xEE, 0xEE, 0xEE, 0x08,
	/* 0x2B0 ~ 0x2BF */
	0x78, 0x90, 0x79, 0x1C, 0x78, 0x90, 0x79, 0x1C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2C0 ~ 0x2CF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2D0 ~ 0x2DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2E0 ~ 0x2EF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2F0 ~ 0x2FF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x300 ~ 0x30F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x310 ~ 0x31F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10, 0x42, 0x10, 0x42, 0x08, 0x21,
	/* 0x320 ~ 0x32F */
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21,
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21,
	/* 0x330 ~ 0x33F */
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21,
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21,
	/* 0x340 ~ 0x34F */
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21,
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x01,
	/* 0x350 ~ 0x35F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x360 ~ 0x36F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x370 ~ 0x37F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x380 ~ 0x38F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x390 ~ 0x39F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3A0 ~ 0x3AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3B0 ~ 0x3BF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3C0 ~ 0x3CF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3D0 ~ 0x3DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3E0 ~ 0x3EF */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	/* 0x3F0 ~ 0x3FF */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	/* 0x400 ~ 0x40F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x410 ~ 0x41F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x420 ~ 0x42F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x430 ~ 0x43F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x440 ~ 0x44F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x450 ~ 0x45F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x460 ~ 0x46F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x470 ~ 0x47F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x480 ~ 0x48F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x490 ~ 0x49F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x4A0 ~ 0x4AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Response ACK to QA Tool DLL.
 *
 * \param[in] HqaCmdFrame	Ethernet Frame Format respond to QA Tool DLL
 * \param[in] prIwReqData
 * \param[in] i4Length		Length of Ethernet Frame data field
 * \param[in] i4Status		Status to respond
 * \param[out] None
 *
 * \retval 0			On success.
 * \retval -EFAULT		If copy_to_user fail
 */
/*----------------------------------------------------------------------------*/
static int32_t ResponseToQA(struct HQA_CMD_FRAME
			    *HqaCmdFrame,
			    IN union iwreq_data *prIwReqData, int32_t i4Length,
			    int32_t i4Status)
{
	if (!prIwReqData)
		return -EINVAL;

	HqaCmdFrame->Length = ntohs((i4Length));
	i4Status = ntohs((i4Status));
	memcpy(HqaCmdFrame->Data, &i4Status, 2);

	prIwReqData->data.length = sizeof((HqaCmdFrame)->MagicNo) +
				   sizeof((HqaCmdFrame)->Type) +
				   sizeof((HqaCmdFrame)->Id) +
				   sizeof((HqaCmdFrame)->Length) +
				   sizeof((HqaCmdFrame)->Sequence) +
				   ntohs((HqaCmdFrame)->Length);

	if (prIwReqData->data.length == 0)
		return -EFAULT;

	if (copy_to_user(prIwReqData->data.pointer,
			 (uint8_t *) (HqaCmdFrame), prIwReqData->data.length)) {
		DBGLOG(RFTEST, INFO, "QA_AGENT copy_to_user() fail in %s\n",
		       __func__);
		return -EFAULT;
	}
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA command(0x%04x)[Magic number(0x%08x)] is done\n",
	       ntohs(HqaCmdFrame->Id), ntohl(HqaCmdFrame->MagicNo));

	return 0;
}

static int32_t ToDoFunction(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT ToDoFunction\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Open Adapter (called when QA Tool UI Open).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_OpenAdapter(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_OpenAdapter\n");

	i4Ret = MT_ATEStart(prNetDev, "ATESTART");

	/* For SA Buffer Mode Temp Solution */
	g_BufferDownload = FALSE;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Close Adapter (called when QA Tool UI Close).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CloseAdapter(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_CloseAdapter\n");

	i4Ret = MT_ATEStop(prNetDev, "ATESTOP");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Start TX.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StartTx(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t TxCount;
	uint16_t TxLength;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartTx\n");

	memcpy((uint8_t *)&TxCount, HqaCmdFrame->Data + 4 * 0, 4);
	TxCount = ntohl(TxCount);
	memcpy((uint8_t *)&TxLength, HqaCmdFrame->Data + 4 * 1, 2);
	TxLength = ntohs(TxLength);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartTx TxCount = %d\n",
	       TxCount);
	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartTx TxLength = %d\n",
	       TxLength);

	i4Ret = MT_ATESetTxCount(prNetDev, TxCount);
	i4Ret = MT_ATESetTxLength(prNetDev, (uint32_t) TxLength);
	i4Ret = MT_ATEStartTX(prNetDev, "TXFRAME");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
/* 1 todo not support yet */
static int32_t HQA_StartTxExt(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartTxExt\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Start Continuous TX.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
/* 1 todo not support yet */
static int32_t HQA_StartTxContiTx(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartTxContiTx\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
/* 1 todo not support yets */
static int32_t HQA_StartTxCarrier(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartTxCarrier\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Start RX (Legacy function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StartRx(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartRx\n");

	MT_ATESetDBDCBandIndex(prNetDev, 0);
	MT_ATEStartRX(prNetDev, "RXFRAME");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Stop TX (Legacy function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StopTx(struct net_device *prNetDev,
			  IN union iwreq_data *prIwReqData,
			  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StopTx\n");

	MT_ATESetDBDCBandIndex(prNetDev, 0);
	MT_ATEStopRX(prNetDev, "RXSTOP");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Stop Continuous TX.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StopContiTx(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StopContiTx\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StopTxCarrier(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StopTxCarrier\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Stop RX (Legacy function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StopRx(struct net_device *prNetDev,
			  IN union iwreq_data *prIwReqData,
			  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StopRx\n");

	MT_ATESetDBDCBandIndex(prNetDev, 0);
	MT_ATEStopRX(prNetDev, "RXSTOP");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set TX Path.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetTxPath(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0, value = 0;
	uint8_t	band_idx = 0;
	uint16_t tx_ant = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetTxPath\n");

	if (HqaCmdFrame->Length > 2) {
		memcpy(&value, HqaCmdFrame->Data + 4 * 0, 4);
		tx_ant = ntohl(value);
		memcpy(&value, HqaCmdFrame->Data + 4 * 1, 4);
		band_idx = ntohl(value);

		if (band_idx && tx_ant > 0x3)
			tx_ant >>= 2;
		DBGLOG(RFTEST, INFO, "tx_path:%d, band:%d\n", tx_ant, band_idx);
	} else {
		memcpy(&tx_ant, HqaCmdFrame->Data + 2 * 0, 2);
		tx_ant = ntohs(tx_ant);
		DBGLOG(RFTEST, INFO, "tx_path:%d, ", tx_ant);
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_DBDC_BAND_IDX;
	rRfATInfo.u4FuncData = band_idx;

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TX_PATH;
	rRfATInfo.u4FuncData = tx_ant;

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set RX Path.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetRxPath(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0, value = 0;
	uint8_t	band_idx = 0;
	uint32_t rx_ant = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetRxPath\n");

	if (HqaCmdFrame->Length > 2) {
		memcpy(&value, HqaCmdFrame->Data + 4 * 0, 4);
		rx_ant = ntohl(value);
		memcpy(&value, HqaCmdFrame->Data + 4 * 1, 4);
		band_idx = ntohl(value);

		if (band_idx && rx_ant > 0x3)
			rx_ant >>= 2;
		DBGLOG(RFTEST, INFO, "rx_path:%d, band:%d\n", rx_ant, band_idx);
	} else {
		memcpy(&rx_ant, HqaCmdFrame->Data + 2 * 0, 2);
		rx_ant = ntohs(rx_ant);
		DBGLOG(RFTEST, INFO, "rx_path:%d, ", rx_ant);
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_DBDC_BAND_IDX;
	rRfATInfo.u4FuncData = band_idx;

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RX_PATH;
	rRfATInfo.u4FuncData = (uint32_t) ((rx_ant << 16)
					   || (0 & BITS(0, 15)));

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set TX Inter-Packet Guard.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetTxIPG(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Aifs = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetTxIPG\n");

	memcpy(&u4Aifs, HqaCmdFrame->Data + 4 * 0, 4);
	u4Aifs = ntohs(u4Aifs);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetTxIPG u4Aifs : %d\n",
	       u4Aifs);

	MT_ATESetTxIPG(prNetDev, u4Aifs);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set TX Power0 (Legacy Function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetTxPower0(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetTxPower0\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set TX Power1.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HAQ_SetTxPower1(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HAQ_SetTxPower1\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetTxPowerExt(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Power = 0;
	uint32_t u4Channel = 0;
	uint32_t u4Dbdc_idx = 0;
	uint32_t u4Band_idx = 0;
	uint32_t u4Ant_idx = 0;

	memcpy(&u4Power, HqaCmdFrame->Data + 4 * 0, 4);
	u4Power = ntohl(u4Power);
	memcpy(&u4Dbdc_idx, HqaCmdFrame->Data + 4 * 1, 4);
	u4Dbdc_idx = ntohl(u4Dbdc_idx);
	memcpy(&u4Channel, HqaCmdFrame->Data + 4 * 2, 4);
	u4Channel = ntohl(u4Channel);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 3, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(&u4Ant_idx, HqaCmdFrame->Data + 4 * 4, 4);
	u4Ant_idx = ntohl(u4Ant_idx);

	DBGLOG(RFTEST, INFO,
		" QA_AGENT HQA_SetTxPowerExt u4Power : %u,u4Dbdc_idx:%u, u4Channel:%u,u4Band_idx:%u, u4Ant_idx:%u\n",
		u4Power, u4Dbdc_idx, u4Channel, u4Band_idx, u4Ant_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Dbdc_idx);
	MT_ATESetTxPower0(prNetDev, u4Power);
	/* u4Freq = nicChannelNum2Freq(u4Channel); */
	/* i4Ret = MT_ATESetChannel(prNetDev, 0, u4Freq); */
	/* MT_ATESetBand(prNetDev, u4Band_idx); */
	/* Antenna?? */

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetOnOff(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetOnOff\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Antenna Selection.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_AntennaSel(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_AntennaSel\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_FWPacketCMD_ClockSwitchDisable(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_FWPacketCMD_ClockSwitchDisable\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_CMD_SET0[] = {
	/* cmd id start from 0x1000 */
	HQA_OpenAdapter,	/* 0x1000 */
	HQA_CloseAdapter,	/* 0x1001 */
	HQA_StartTx,		/* 0x1002 */
	HQA_StartTxExt,		/* 0x1003 */
	HQA_StartTxContiTx,	/* 0x1004 */
	HQA_StartTxCarrier,	/* 0x1005 */
	HQA_StartRx,		/* 0x1006 */
	HQA_StopTx,		/* 0x1007 */
	HQA_StopContiTx,	/* 0x1008 */
	HQA_StopTxCarrier,	/* 0x1009 */
	HQA_StopRx,		/* 0x100A */
	HQA_SetTxPath,		/* 0x100B */
	HQA_SetRxPath,		/* 0x100C */
	HQA_SetTxIPG,		/* 0x100D */
	HQA_SetTxPower0,	/* 0x100E */
	HAQ_SetTxPower1,	/* 0x100F */
	ToDoFunction,		/* 0x1010 */
	HQA_SetTxPowerExt,	/* 0x1011 */
	HQA_SetOnOff,		/* 0x1012 */
	HQA_AntennaSel,		/* 0x1013 */
	HQA_FWPacketCMD_ClockSwitchDisable,	/* 0x1014 */
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set Channel Frequency (Legacy Function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetChannel(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t i4SetFreq = 0, i4SetChan = 0;
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetChannel\n");

	memcpy((uint8_t *)&i4SetChan, HqaCmdFrame->Data, 4);
	i4SetChan = ntohl(i4SetChan);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetChannel Channel = %d\n", i4SetChan);

	i4SetFreq = nicChannelNum2Freq(i4SetChan);
	i4Ret = MT_ATESetChannel(prNetDev, 0, i4SetFreq);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set Preamble (Legacy Function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetPreamble(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Mode = 0;
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetPreamble\n");

	memcpy((uint8_t *)&i4Mode, HqaCmdFrame->Data, 4);
	i4Mode = ntohl(i4Mode);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetPreamble Mode = %d\n",
	       i4Mode);

	i4Ret = MT_ATESetPreamble(prNetDev, i4Mode);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set Rate (Legacy Function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetRate(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	/*	INT_32 i4Value = 0; */
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetRate\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set Nss.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetNss(struct net_device *prNetDev,
			  IN union iwreq_data *prIwReqData,
			  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetNss\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set System BW (Legacy Function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetSystemBW(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t i4BW;
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetSystemBW\n");

	memcpy((uint8_t *)&i4BW, HqaCmdFrame->Data, 4);
	i4BW = ntohl(i4BW);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetSystemBW BW = %d\n",
	       i4BW);

	i4Ret = MT_ATESetSystemBW(prNetDev, i4BW);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set Data BW (Legacy Function).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetPerPktBW(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Perpkt_bw;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetPerPktBW\n");

	memcpy((uint8_t *)&u4Perpkt_bw, HqaCmdFrame->Data, 4);
	u4Perpkt_bw = ntohl(u4Perpkt_bw);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetPerPktBW u4Perpkt_bw = %d\n", u4Perpkt_bw);

	i4Ret = MT_ATESetPerPacketBW(prNetDev, u4Perpkt_bw);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set Primary BW.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetPrimaryBW(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t               u4Pri_sel = 0;

	memcpy(&u4Pri_sel, HqaCmdFrame->Data, 4);
	u4Pri_sel = ntohl(u4Pri_sel);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetPrimaryBW u4Pri_sel : %d\n", u4Pri_sel);

	i4Ret = MT_ATEPrimarySetting(prNetDev, u4Pri_sel);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set Frequency Offset.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetFreqOffset(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4FreqOffset = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&u4FreqOffset, HqaCmdFrame->Data, 4);
	u4FreqOffset = ntohl(u4FreqOffset);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetFreqOffset u4FreqOffset : %d\n",
	       u4FreqOffset);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_FRWQ_OFFSET;
	rRfATInfo.u4FuncData = (uint32_t) u4FreqOffset;

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetAutoResponder(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetAutoResponder\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetTssiOnOff(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetTssiOnOff\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
/* 1 todo not support yet */

static int32_t HQA_SetRxHighLowTemperatureCompensation(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRxHighLowTemperatureCompensation\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_LowPower(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_LowPower\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

#if CFG_SUPPORT_ANT_SWAP
/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For query ant swap capablity
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetAntSwapCapability(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t value = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo || !prGlueInfo->prAdapter) {
		DBGLOG(RFTEST, ERROR, "prGlueInfo or prAdapter is NULL\n");
		return -EFAULT;
	}

	prChipInfo = prGlueInfo->prAdapter->chip_info;
	if (!prChipInfo) {
		DBGLOG(RFTEST, ERROR, "prChipInfo is NULL\n");
		return -EFAULT;
	}

	DBGLOG(RFTEST, INFO, "HQA_GetAntSwapCapability [%d]\n",
				prGlueInfo->prAdapter->fgIsSupportAntSwp);

	DBGLOG(RFTEST, INFO, "ucMaxSwapAntenna = [%d]\n",
				prChipInfo->ucMaxSwapAntenna);

	if (prGlueInfo->prAdapter->fgIsSupportAntSwp)
		value = ntohl(prChipInfo->ucMaxSwapAntenna);
	else
		value = 0;

	memcpy(HqaCmdFrame->Data + 2, &value, sizeof(value));
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(value), i4Ret);
	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For setting antenna swap
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetAntSwap(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ant = 0, u4Band = 0;

	memcpy(&u4Band, HqaCmdFrame->Data, sizeof(uint32_t));
	memcpy(&u4Ant, HqaCmdFrame->Data +  sizeof(uint32_t), sizeof(uint32_t));
	u4Ant = ntohl(u4Ant);

	DBGLOG(RFTEST, INFO, "Band = %d, Ant = %d\n", u4Band, u4Ant);

	i4Ret = MT_ATESetAntSwap(prNetDev, u4Ant);
	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	return i4Ret;

}
#endif


static HQA_CMD_HANDLER HQA_CMD_SET1[] = {
	/* cmd id start from 0x1100 */
	HQA_SetChannel,		/* 0x1100 */
	HQA_SetPreamble,	/* 0x1101 */
	HQA_SetRate,		/* 0x1102 */
	HQA_SetNss,		/* 0x1103 */
	HQA_SetSystemBW,	/* 0x1104 */
	HQA_SetPerPktBW,	/* 0x1105 */
	HQA_SetPrimaryBW,	/* 0x1106 */
	HQA_SetFreqOffset,	/* 0x1107 */
	HQA_SetAutoResponder,	/* 0x1108 */
	HQA_SetTssiOnOff,	/* 0x1109 */
	HQA_SetRxHighLowTemperatureCompensation,	/* 0x110A */
	HQA_LowPower,		/* 0x110B */
	NULL,			/* 0x110C */
#if CFG_SUPPORT_ANT_SWAP
	HQA_GetAntSwapCapability,	/* 0x110D */
	HQA_SetAntSwap,		/* 0x110E */
#endif
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Reset TRX Counter
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_ResetTxRxCounter(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t i4Status;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_ResetTxRxCounter\n");

	i4Status = MT_ATEResetTXRXCounter(prNetDev);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetStatistics(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetStatistics\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetRxOKData(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetRxOKData\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetRxOKOther(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetRxOKOther\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetRxAllPktCount(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetRxAllPktCount\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetTxTransmitted(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetTxTransmitted\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetHwCounter(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetHwCounter\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CalibrationOperation(struct net_device
					*prNetDev,
					IN union iwreq_data *prIwReqData,
					struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_CalibrationOperation\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CalibrationBypassExt(struct net_device
					*prNetDev,
					IN union iwreq_data *prIwReqData,
					struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Status = 0;
	uint32_t u4Item = 0;
	uint32_t u4Band_idx = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&u4Item, HqaCmdFrame->Data, 4);
	u4Item = ntohl(u4Item);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_CalibrationBypassExt u4Item : 0x%08x\n",
	       u4Item);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_CalibrationBypassExt u4Band_idx : %d\n",
	       u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_BYPASS_CAL_STEP;
	rRfATInfo.u4FuncData = u4Item;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetRXVectorIdx(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t band_idx = 0;
	uint32_t Group_1 = 0, Group_2 = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	band_idx = ntohl(band_idx);
	memcpy(&Group_1, HqaCmdFrame->Data + 4 * 1, 4);
	Group_1 = ntohl(Group_1);
	memcpy(&Group_2, HqaCmdFrame->Data + 4 * 2, 4);
	Group_2 = ntohl(Group_2);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXVectorIdx band_idx : %d\n", band_idx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXVectorIdx Group_1 : %d\n", Group_1);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXVectorIdx Group_2 : %d\n", Group_2);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RXV_INDEX;
	rRfATInfo.u4FuncData = (uint32_t) (Group_1);
	rRfATInfo.u4FuncData |= (uint32_t) (Group_2 << 8);
	rRfATInfo.u4FuncData |= (uint32_t) (band_idx << 16);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXVectorIdx rRfATInfo.u4FuncData : 0x%08x\n",
	       rRfATInfo.u4FuncData);

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set FAGC Rssi Path
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetFAGCRssiPath(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4band_idx = 0;
	uint32_t u4FAGC_Path = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&u4band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4band_idx = ntohl(u4band_idx);
	memcpy(&u4FAGC_Path, HqaCmdFrame->Data + 4 * 1, 4);
	u4FAGC_Path = ntohl(u4FAGC_Path);

	DBGLOG(RFTEST, INFO, "u4band_idx : %d, u4FAGC_Path : %d\n",
			     u4band_idx, u4FAGC_Path);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_FAGC_RSSI_PATH;
	rRfATInfo.u4FuncData = (uint32_t) ((u4band_idx << 16) |
					   (u4FAGC_Path & BITS(0, 15)));

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_CMD_SET2[] = {
	/* cmd id start from 0x1200 */
	HQA_ResetTxRxCounter,	/* 0x1200 */
	HQA_GetStatistics,	/* 0x1201 */
	HQA_GetRxOKData,	/* 0x1202 */
	HQA_GetRxOKOther,	/* 0x1203 */
	HQA_GetRxAllPktCount,	/* 0x1204 */
	HQA_GetTxTransmitted,	/* 0x1205 */
	HQA_GetHwCounter,	/* 0x1206 */
	HQA_CalibrationOperation,	/* 0x1207 */
	HQA_CalibrationBypassExt,	/* 0x1208 */
	HQA_SetRXVectorIdx,	/* 0x1209 */
	HQA_SetFAGCRssiPath,	/* 0x120A */
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For MAC CR Read.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MacBbpRegRead(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t u4Offset, u4Value;
	int32_t i4Status;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_MCR_RW_STRUCT rMcrInfo;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MacBbpRegRead\n");

	memcpy(&u4Offset, HqaCmdFrame->Data, 4);
	u4Offset = ntohl(u4Offset);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MacBbpRegRead Offset = 0x%08x\n", u4Offset);

	rMcrInfo.u4McrOffset = u4Offset;
	rMcrInfo.u4McrData = 0;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo, wlanoidQueryMcrRead,
			    &rMcrInfo, sizeof(rMcrInfo),
			    TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Value = rMcrInfo.u4McrData;

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT Address = 0x%08x, Result = 0x%08x\n", u4Offset,
		       u4Value);

		u4Value = ntohl(u4Value);
		memcpy(HqaCmdFrame->Data + 2, &u4Value, 4);
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For MAC CR Write.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MacBbpRegWrite(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{

	/*	INT_32 i4Ret = 0; */
	uint32_t u4Offset, u4Value;
	int32_t i4Status = 0;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_MCR_RW_STRUCT rMcrInfo;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MacBbpRegWrite\n");

	memcpy(&u4Offset, HqaCmdFrame->Data, 4);
	memcpy(&u4Value, HqaCmdFrame->Data + 4, 4);

	u4Offset = ntohl(u4Offset);
	u4Value = ntohl(u4Value);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MacBbpRegWrite Offset = 0x%08x\n", u4Offset);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MacBbpRegWrite Value = 0x%08x\n", u4Value);

	rMcrInfo.u4McrOffset = u4Offset;
	rMcrInfo.u4McrData = u4Value;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo, wlanoidSetMcrWrite, &rMcrInfo,
			sizeof(rMcrInfo), FALSE, FALSE, TRUE, &u4BufLen);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Read Bulk MAC CR.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MACBbpRegBulkRead(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t u4Index, u4Offset, u4Value;
	uint16_t u2Len;
	int32_t i4Status = 0;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_MCR_RW_STRUCT rMcrInfo;

	memcpy(&u4Offset, HqaCmdFrame->Data, 4);
	u4Offset = ntohl(u4Offset);
	memcpy(&u2Len, HqaCmdFrame->Data + 4, 2);
	u2Len = ntohs(u2Len);

	DBGLOG(RFTEST, INFO, "Offset = 0x%08x, Len = 0x%08x\n",
				u4Offset, u2Len);

	for (u4Index = 0; u4Index < u2Len; u4Index++) {
		rMcrInfo.u4McrOffset = u4Offset + u4Index * 4;
		rMcrInfo.u4McrData = 0;
		prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

		i4Status = kalIoctl(prGlueInfo, wlanoidQueryMcrRead,
				&rMcrInfo, sizeof(rMcrInfo),
				TRUE, TRUE, TRUE, &u4BufLen);

		if (i4Status == 0) {
			u4Value = rMcrInfo.u4McrData;

			DBGLOG(RFTEST, INFO,
			       "Address = 0x%08x, Result = 0x%08x\n",
			       u4Offset + u4Index * 4, u4Value);

			u4Value = ntohl(u4Value);
			memcpy(HqaCmdFrame->Data + 2 + (u4Index * 4), &u4Value,
			       4);
		}
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + (u2Len * 4),
		     i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Read Bulk RF CR.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_RfRegBulkRead(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t u4Index, u4WfSel, u4Offset, u4Length, u4Value;
	int32_t i4Status = 0;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_MCR_RW_STRUCT rMcrInfo;

	memcpy(&u4WfSel, HqaCmdFrame->Data, 4);
	u4WfSel = ntohl(u4WfSel);
	memcpy(&u4Offset, HqaCmdFrame->Data + 4, 4);
	u4Offset = ntohl(u4Offset);
	memcpy(&u4Length, HqaCmdFrame->Data + 8, 4);
	u4Length = ntohl(u4Length);

	DBGLOG(RFTEST, INFO, " WfSel  = %u, Offset = 0x%08x, Length = %u\n",
		u4WfSel, u4Offset, u4Length);

	if (u4WfSel == 0)
		u4Offset = u4Offset | 0x99900000;
	else if (u4WfSel == 1)
		u4Offset = u4Offset | 0x99910000;
	else if (u4WfSel == 15)
		u4Offset = u4Offset | 0x999F0000;


	for (u4Index = 0; u4Index < u4Length; u4Index++) {
		rMcrInfo.u4McrOffset = u4Offset + u4Index * 4;
		rMcrInfo.u4McrData = 0;
		prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

		i4Status = kalIoctl(prGlueInfo, wlanoidQueryMcrRead,
				    &rMcrInfo, sizeof(rMcrInfo),
				    TRUE, TRUE, TRUE, &u4BufLen);

		if (i4Status == 0) {
			u4Value = rMcrInfo.u4McrData;

			DBGLOG(RFTEST, INFO,
			       "Address = 0x%08x, Result = 0x%08x\n",
			       u4Offset + u4Index * 4, u4Value);

			u4Value = ntohl(u4Value);
			memcpy(HqaCmdFrame->Data + 2 + (u4Index * 4), &u4Value,
			       4);
		}
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + (u4Length * 4),
		     i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Write RF CR.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_RfRegBulkWrite(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t u4WfSel, u4Offset, u4Length, u4Value;
	int32_t i4Status;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_MCR_RW_STRUCT rMcrInfo;

	memcpy(&u4WfSel, HqaCmdFrame->Data, 4);
	u4WfSel = ntohl(u4WfSel);
	memcpy(&u4Offset, HqaCmdFrame->Data + 4, 4);
	u4Offset = ntohl(u4Offset);
	memcpy(&u4Length, HqaCmdFrame->Data + 8, 4);
	u4Length = ntohl(u4Length);
	memcpy(&u4Value, HqaCmdFrame->Data + 12, 4);
	u4Value = ntohl(u4Value);

	DBGLOG(RFTEST, INFO,
		"WfSel  = %u, Offset = 0x%08x, Length = %u, Value  = 0x%08x\n",
		u4WfSel, u4Offset, u4Length, u4Value);

	if (u4WfSel == 0)
		u4Offset = u4Offset | 0x99900000;
	else if (u4WfSel == 1)
		u4Offset = u4Offset | 0x99910000;


	rMcrInfo.u4McrOffset = u4Offset;
	rMcrInfo.u4McrData = u4Value;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo, wlanoidSetMcrWrite,
			    &rMcrInfo, sizeof(rMcrInfo),
			    FALSE, FALSE, TRUE, &u4BufLen);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_ReadEEPROM(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{

	uint16_t Offset;
	uint16_t Len;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	uint32_t u4BufLen = 0;
	uint8_t  u4Index = 0;
	uint16_t  u4Value = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_ACCESS_EFUSE rAccessEfuseInfo;
#endif

	DBGLOG(INIT, INFO, "QA_AGENT HQA_ReadEEPROM\n");

	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&Len, HqaCmdFrame->Data + 2 * 1, 2);
	Len = ntohs(Len);

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo) {
		log_dbg(RFTEST, ERROR, "prGlueInfo is NULL\n");
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, rStatus);
		return rStatus;
	}

	if (prGlueInfo->prAdapter &&
	    prGlueInfo->prAdapter->chip_info &&
	    !prGlueInfo->prAdapter->chip_info->is_support_efuse) {
		rStatus = WLAN_STATUS_NOT_SUPPORTED;
		log_dbg(RFTEST, WARN, "Efuse not support\n");
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, rStatus);
		return rStatus;
	}

	kalMemSet(&rAccessEfuseInfo, 0,
		sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));

	rAccessEfuseInfo.u4Address =
		(Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;


	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryProcessAccessEfuseRead,
			   &rAccessEfuseInfo,
			   sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
			   TRUE, TRUE, TRUE, &u4BufLen);

	u4Index = Offset % EFUSE_BLOCK_SIZE;
	if (u4Index <= 14)
		u4Value =
		    (prGlueInfo->prAdapter->aucEepromVaule[u4Index]) |
		    (prGlueInfo->prAdapter->aucEepromVaule[u4Index + 1] << 8);


	/* isVaild = pResult->u4Valid; */

	if (rStatus == WLAN_STATUS_SUCCESS) {

		DBGLOG(INIT, INFO, "QA_AGENT HQA_ReadEEPROM u4Value = %x\n",
		       u4Value);

		u4Value = ntohl(u4Value);
		memcpy(HqaCmdFrame->Data + 2, &u4Value, sizeof(u4Value));
	}
#endif

	ResponseToQA(HqaCmdFrame, prIwReqData, 4, rStatus);

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_WriteEEPROM(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;


#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	uint16_t  u4WriteData = 0;
	uint32_t u4BufLen = 0;
	uint8_t  u4Index = 0;
	uint16_t Offset;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_ACCESS_EFUSE rAccessEfuseInfoWrite;

	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&u4WriteData, HqaCmdFrame->Data + 2 * 1, 2);
	u4WriteData = ntohs(u4WriteData);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
#if 0
	/* Read */
	DBGLOG(INIT, INFO, "QA_AGENT HQA_ReadEEPROM\n");
	kalMemSet(&rAccessEfuseInfoRead, 0,
		  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
	rAccessEfuseInfoRead.u4Address = (Offset / EFUSE_BLOCK_SIZE)
					 * EFUSE_BLOCK_SIZE;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryProcessAccessEfuseRead,
			   &rAccessEfuseInfoRead,
			   sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
			   FALSE, FALSE, TRUE, &u4BufLen);
#endif

	/* Write */
	DBGLOG(INIT, INFO, "QA_AGENT HQA_WriteEEPROM\n");
	kalMemSet(&rAccessEfuseInfoWrite, 0,
		  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
	u4Index = Offset % EFUSE_BLOCK_SIZE;

	if (prGlueInfo->prAdapter->rWifiVar.ucEfuseBufferModeCal ==
	    TRUE && Offset >= 0 && Offset < MAX_EEPROM_BUFFER_SIZE - 1) {
		uacEEPROMImage[Offset] = u4WriteData & 0xff;
		uacEEPROMImage[Offset + 1] = u4WriteData >> 8 & 0xff;
	} else if (u4Index >= EFUSE_BLOCK_SIZE - 1) {
		DBGLOG(INIT, ERROR, "u4Index [%d] overrun\n", u4Index);
	} else {
		prGlueInfo->prAdapter->aucEepromVaule[u4Index] = u4WriteData
				& 0xff; /* Note: u4WriteData is UINT_16 */
		prGlueInfo->prAdapter->aucEepromVaule[u4Index + 1] =
			u4WriteData >> 8 & 0xff;

		kalMemCopy(rAccessEfuseInfoWrite.aucData,
			   prGlueInfo->prAdapter->aucEepromVaule, 16);
		rAccessEfuseInfoWrite.u4Address =
				(Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryProcessAccessEfuseWrite,
				   &rAccessEfuseInfoWrite,
				   sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
				   FALSE, FALSE, TRUE, &u4BufLen);
	}
#endif

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_ReadBulkEEPROM(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint16_t Offset;
	uint16_t Len;
#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	struct PARAM_CUSTOM_ACCESS_EFUSE rAccessEfuseInfo;
	uint32_t u4BufLen = 0;
	uint8_t  u4Loop = 0;

	uint16_t Buffer;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t tmp = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemSet(&rAccessEfuseInfo, 0,
		  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
#endif

	DBGLOG(INIT, INFO, "QA_AGENT HQA_ReadBulkEEPROM\n");
	if (prAdapter->chip_info &&
	    !prAdapter->chip_info->is_support_efuse) {
		log_dbg(RFTEST, WARN, "Efuse not support\n");
		rStatus = WLAN_STATUS_NOT_SUPPORTED;
		ResponseToQA(HqaCmdFrame, prIwReqData,
			     2, rStatus);
		return rStatus;
	}

	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&Len, HqaCmdFrame->Data + 2 * 1, 2);
	Len = ntohs(Len);
	tmp = Offset;
	DBGLOG(INIT, INFO,
	       "QA_AGENT HQA_ReadBulkEEPROM Offset : %d\n", Offset);
	DBGLOG(INIT, INFO, "QA_AGENT HQA_ReadBulkEEPROM Len : %d\n",
	       Len);

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	rAccessEfuseInfo.u4Address = (Offset / EFUSE_BLOCK_SIZE) *
				     EFUSE_BLOCK_SIZE;

	DBGLOG(INIT, INFO,
	       "QA_AGENT HQA_ReadBulkEEPROM Address : %d\n",
	       rAccessEfuseInfo.u4Address);

	if	((prGlueInfo->prAdapter->rWifiVar.ucEfuseBufferModeCal !=
		  TRUE)
		 && (prGlueInfo->prAdapter->fgIsSupportQAAccessEfuse ==
		     TRUE)) {

		/* Read from Efuse */
		DBGLOG(INIT, INFO,
		       "QA_AGENT HQA_ReadBulkEEPROM Efuse Mode\n");
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryProcessAccessEfuseRead,
				   &rAccessEfuseInfo,
				   sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, INFO,
			       "QA_AGENT HQA_ReadBulkEEPROM kal fail\n");

		Offset = Offset % EFUSE_BLOCK_SIZE;

#if 0
		for (u4Loop = 0; u4Loop < 16; u4Loop++) {
			DBGLOG(INIT, INFO,
			       "MT6632:QA_AGENT HQA_ReadBulkEEPROM Efuse Offset=%x u4Loop=%d u4Value=%x\n",
			       Offset, u4Loop,
			       prGlueInfo->prAdapter->aucEepromVaule[u4Loop]);
		}
#endif
		for (u4Loop = 0; u4Loop < Len; u4Loop += 2) {
			memcpy(&Buffer, prGlueInfo->prAdapter->aucEepromVaule +
			       Offset + u4Loop, 2);
			Buffer = ntohs(Buffer);
			DBGLOG(INIT, INFO,
			       ":From Efuse  u4Loop=%d  Buffer=%x\n",
			       u4Loop, Buffer);
			memcpy(HqaCmdFrame->Data + 2 + u4Loop, &Buffer, 2);
		}

	} else {  /* Read from EEPROM */
		for (u4Loop = 0; u4Loop < Len; u4Loop += 2) {
			memcpy(&Buffer, uacEEPROMImage + Offset + u4Loop, 2);
			Buffer = ntohs(Buffer);
			memcpy(HqaCmdFrame->Data + 2 + u4Loop, &Buffer, 2);
			DBGLOG(INIT, INFO,
			       "QA_AGENT HQA_ReadBulkEEPROM u4Loop=%d  u4Value=%x\n",
			       u4Loop, uacEEPROMImage[Offset + u4Loop]);
		}
	}
#endif

	/*kfree(Buffer);*/

	/* Read from buffer array in driver */
	/* Pass these data to FW also */
#if 0
	for (i = 0 ; i < Len ; i += 2) {
		memcpy(&u2Temp, uacEEPROMImage + Offset + i, 2);
		u2Temp = ntohs(u2Temp);
		memcpy(HqaCmdFrame->Data + 2 + i, &u2Temp, 2);
	}
#endif
	/* For SA Buffer Mode Temp Solution */
#if 0
	if (Offset == 0x4A0 && !g_BufferDownload) {

		uint16_t u2InitAddr = 0x000;
		uint32_t i = 0, j = 0;
		uint32_t u4BufLen = 0;
		uint32_t rStatus = WLAN_STATUS_SUCCESS;
		struct GLUE_INFO *prGlueInfo = NULL;
		struct PARAM_CUSTOM_EFUSE_BUFFER_MODE rSetEfuseBufModeInfo;

		prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

		for (i = 0 ; i < MAX_EEPROM_BUFFER_SIZE / 16 ; i++) {
			for (j = 0 ; j < 16 ; j++) {
				rSetEfuseBufModeInfo.aBinContent[j].u2Addr =
					u2InitAddr;
				rSetEfuseBufModeInfo.aBinContent[j].ucValue =
					uacEEPROMImage[u2InitAddr];
				u2InitAddr += 1;
			}

			rSetEfuseBufModeInfo.ucSourceMode = 1;
			rSetEfuseBufModeInfo.ucCount = EFUSE_CONTENT_SIZE;
			rStatus = kalIoctl(prGlueInfo, wlanoidSetEfusBufferMode,
				&rSetEfuseBufModeInfo,
				sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
				FALSE, FALSE, TRUE, &u4BufLen);
		}

		g_BufferDownload = TRUE;
	}
#endif
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + Len, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_WriteBulkEEPROM(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint16_t Offset;
	uint16_t Len;
	struct ADAPTER *prAdapter = NULL;

	uint32_t u4BufLen = 0;
	struct PARAM_CUSTOM_ACCESS_EFUSE rAccessEfuseInfoRead,
		       rAccessEfuseInfoWrite;
	uint16_t testBuffer1, testBuffer2, testBuffer;
	uint16_t	*Buffer = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t  u4Loop = 0, u4Index = 0;
	uint16_t ucTemp2;
	uint16_t i = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemSet(&rAccessEfuseInfoRead, 0,
		  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
	kalMemSet(&rAccessEfuseInfoWrite, 0,
		  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));

	DBGLOG(INIT, INFO, "QA_AGENT HQA_WriteBulkEEPROM\n");


	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&Len, HqaCmdFrame->Data + 2 * 1, 2);
	Len = ntohs(Len);

	memcpy(&testBuffer1, HqaCmdFrame->Data + 2 * 2, Len);
	testBuffer2 = ntohs(testBuffer1);
	testBuffer = ntohs(testBuffer1);

	DBGLOG(INIT, INFO, "Offset : %x, Len : %u\n", Offset, Len);

	/* Support Delay Calibraiton */
	if (prGlueInfo->prAdapter->fgIsSupportQAAccessEfuse ==
	    TRUE) {

		Buffer = kmalloc(sizeof(uint8_t) * (EFUSE_BLOCK_SIZE),
				 GFP_KERNEL);
		ASSERT(Buffer);
		kalMemSet(Buffer, 0, sizeof(uint8_t) * (EFUSE_BLOCK_SIZE));

		kalMemCopy((uint8_t *)Buffer,
			   (uint8_t *)HqaCmdFrame->Data + 4, Len);

		for (u4Loop = 0; u4Loop < (Len); u4Loop++) {

			DBGLOG(INIT, INFO,
			       "QA_AGENT HQA_WriteBulkEEPROM u4Loop=%d  u4Value=%x\n",
			       u4Loop, Buffer[u4Loop]);
		}

		if (prGlueInfo->prAdapter->rWifiVar.ucEfuseBufferModeCal ==
		    TRUE &&
		    Offset >= 0 && Offset < MAX_EEPROM_BUFFER_SIZE - 1) {
			/* EEPROM */
			DBGLOG(INIT, INFO, "Direct EEPROM buffer, offset=%x\n",
			       Offset);
#if 0
			for (i = 0; i < EFUSE_BLOCK_SIZE; i++)
				memcpy(uacEEPROMImage + Offset + i, Buffer + i,
				       1);

#endif
			*Buffer = ntohs(*Buffer);
			uacEEPROMImage[Offset] = *Buffer & 0xff;
			uacEEPROMImage[Offset + 1] = *Buffer >> 8 & 0xff;
		} else {
			/* EFUSE */
			/* Read */
			DBGLOG(INIT, INFO,
			       "QA_AGENT HQA_WriteBulkEEPROM  Read\n");
			if (prAdapter->chip_info &&
			    !prAdapter->chip_info->is_support_efuse) {
				log_dbg(RFTEST, WARN, "Efuse not support\n");
				rStatus = WLAN_STATUS_NOT_SUPPORTED;
				ResponseToQA(HqaCmdFrame, prIwReqData,
					     2, rStatus);
				kfree(Buffer);
				return rStatus;
			}
			kalMemSet(&rAccessEfuseInfoRead, 0,
				  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
			rAccessEfuseInfoRead.u4Address =
				(Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
			rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryProcessAccessEfuseRead,
				&rAccessEfuseInfoRead,
				sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
				TRUE, TRUE, TRUE, &u4BufLen);

			/* Write */
			kalMemSet(&rAccessEfuseInfoWrite, 0,
				  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));

			if (Len > 2) {
				for (u4Loop = 0; u4Loop < 8 ; u4Loop++)
					Buffer[u4Loop] = ntohs(Buffer[u4Loop]);
				memcpy(rAccessEfuseInfoWrite.aucData, Buffer,
				       16);
			} else {
				u4Index = Offset % EFUSE_BLOCK_SIZE;
				DBGLOG(INIT, INFO,
				       "MT6632:QA_AGENT HQA_WriteBulkEEPROM Wr,u4Index=%x,Buffer=%x\n",
				       u4Index, testBuffer);

				*Buffer = ntohs(*Buffer);
				DBGLOG(INIT, INFO,
				       "Buffer[0]=%x, Buffer[0]&0xff=%x\n",
				       Buffer[0], Buffer[0] & 0xff);
				DBGLOG(INIT, INFO, "Buffer[0] >> 8 & 0xff=%x\n"
				       , Buffer[0] >> 8 & 0xff);

				if (u4Index < EFUSE_BLOCK_SIZE - 1) {
					prGlueInfo->prAdapter
					->aucEepromVaule[u4Index] =
							*Buffer & 0xff;
					prGlueInfo->prAdapter
					->aucEepromVaule[u4Index + 1] =
							*Buffer	>> 8 & 0xff;
					kalMemCopy(
						rAccessEfuseInfoWrite.aucData,
						prGlueInfo->prAdapter
						->aucEepromVaule, 16);
				} else {
					DBGLOG(INIT, ERROR,
						"u4Index [%d] overrun\n",
						u4Index);
					goto end;
				}
			}

			rAccessEfuseInfoWrite.u4Address =
				(Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
			for (u4Loop = 0; u4Loop < (EFUSE_BLOCK_SIZE);
			     u4Loop++) {
				DBGLOG(INIT, INFO, " Loop=%d  aucData=%x\n",
				       u4Loop,
				       rAccessEfuseInfoWrite.aucData[u4Loop]);
			}

			DBGLOG(INIT, INFO, "Going for e-Fuse\n");

			rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryProcessAccessEfuseWrite,
				&rAccessEfuseInfoWrite,
				sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
				FALSE, TRUE, TRUE, &u4BufLen);
		}

	} else {

		if (Len == 2) {
			memcpy(&ucTemp2, HqaCmdFrame->Data + 2 * 2, 2);
			ucTemp2 = ntohs(ucTemp2);
			memcpy(uacEEPROMImage + Offset, &ucTemp2, Len);
		} else {
			for (i = 0 ; i < 8 ; i++) {
				memcpy(&ucTemp2,
				       HqaCmdFrame->Data + 2 * 2 + 2 * i, 2);
				ucTemp2 = ntohs(ucTemp2);
				memcpy(uacEEPROMImage + Offset + 2 * i,
				       &ucTemp2, 2);
			}

			if (!g_BufferDownload) {
				uint16_t u2InitAddr = Offset;
				uint32_t j = 0;
				uint32_t u4BufLen = 0;
				uint32_t rStatus = WLAN_STATUS_SUCCESS;
				struct GLUE_INFO *prGlueInfo = NULL;
				struct PARAM_CUSTOM_EFUSE_BUFFER_MODE
					*prSetEfuseBufModeInfo = NULL;
				struct BIN_CONTENT *pBinContent;

				prSetEfuseBufModeInfo =
					(
					struct PARAM_CUSTOM_EFUSE_BUFFER_MODE *)
					kalMemAlloc(sizeof(
					struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
					VIR_MEM_TYPE);
				if (prSetEfuseBufModeInfo == NULL)
					return 0;
				kalMemZero(prSetEfuseBufModeInfo,
					sizeof(
					struct PARAM_CUSTOM_EFUSE_BUFFER_MODE));

				prGlueInfo =
					*((struct GLUE_INFO **)
							netdev_priv(prNetDev));
				pBinContent = (struct BIN_CONTENT *)
					prSetEfuseBufModeInfo->aBinContent;

				for (j = 0 ; j < 16 ; j++) {
					pBinContent->u2Addr = u2InitAddr;
					pBinContent->ucValue =
						uacEEPROMImage[u2InitAddr];

					pBinContent++;
				}

				prSetEfuseBufModeInfo->ucSourceMode = 1;
				prSetEfuseBufModeInfo->ucCount =
							EFUSE_CONTENT_SIZE;
				rStatus = kalIoctl(prGlueInfo,
					wlanoidSetEfusBufferMode,
					(void *)prSetEfuseBufModeInfo, sizeof(
					struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
					FALSE, FALSE, TRUE, &u4BufLen);

				kalMemFree(prSetEfuseBufModeInfo, VIR_MEM_TYPE,
					sizeof(
					struct PARAM_CUSTOM_EFUSE_BUFFER_MODE));

				if (Offset == 0x4A0)
					g_BufferDownload = TRUE;
			}
		}
	}

end:
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + Len, i4Ret);
	kfree(Buffer);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CheckEfuseMode(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		Value = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_CheckEfuseMode\n");

	/* Value: 0:eeprom mode, 1:eFuse mode */
	Value = ntohl(Value);
	memcpy(HqaCmdFrame->Data + 2, &(Value), sizeof(Value));

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetFreeEfuseBlock(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{

	int32_t i4Ret = 0, u4FreeBlockCount = 0;

#if (CFG_EEPROM_PAGE_ACCESS == 1)
	struct PARAM_CUSTOM_EFUSE_FREE_BLOCK rEfuseFreeBlock;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
#endif

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, INFO, "QA_AGENT HQA_GetFreeEfuseBlock\n");

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	if (prGlueInfo->prAdapter->fgIsSupportGetFreeEfuseBlockCount
	    == TRUE) {
		kalMemSet(&rEfuseFreeBlock, 0,
			  sizeof(struct PARAM_CUSTOM_EFUSE_FREE_BLOCK));


		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryEfuseFreeBlock,
				   &rEfuseFreeBlock,
				   sizeof(struct PARAM_CUSTOM_EFUSE_FREE_BLOCK),
				   TRUE, TRUE, TRUE, &u4BufLen);

		u4FreeBlockCount = prGlueInfo->prAdapter->u4FreeBlockNum;
		u4FreeBlockCount = ntohl(u4FreeBlockCount);
		kalMemCopy(HqaCmdFrame->Data + 2, &u4FreeBlockCount, 4);
	}
#endif

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetEfuseBlockNr(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetEfuseBlockNr\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_WriteEFuseFromBuffer(struct net_device
					*prNetDev,
					IN union iwreq_data *prIwReqData,
					struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_WriteEFuseFromBuffer\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetTxPower(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Channel = 0, u4Band = 0, u4Ch_Band = 0,
		 u4TxTargetPower = 0;
	/*	UINT_32 u4EfuseAddr = 0, u4Power = 0; */

#if (CFG_EEPROM_PAGE_ACCESS == 1)
	struct PARAM_CUSTOM_GET_TX_POWER rGetTxPower;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
#endif

	memcpy(&u4Channel, HqaCmdFrame->Data + 4 * 0, 4);
	u4Channel = ntohl(u4Channel);
	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 1, 4);
	u4Band = ntohl(u4Band);
	memcpy(&u4Ch_Band, HqaCmdFrame->Data + 4 * 2, 4);
	u4Ch_Band = ntohl(u4Ch_Band);

	DBGLOG(RFTEST, INFO, "u4Channel : %u, u4Band : %u, u4Ch_Band : %u\n",
		u4Channel, u4Band, u4Ch_Band);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (prGlueInfo->prAdapter->fgIsSupportGetTxPower == TRUE) {
		kalMemSet(&rGetTxPower, 0,
			  sizeof(struct PARAM_CUSTOM_GET_TX_POWER));

		rGetTxPower.ucCenterChannel = u4Channel;
		rGetTxPower.ucBand = u4Band;
		rGetTxPower.ucDbdcIdx = u4Ch_Band;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryGetTxPower,
				   &rGetTxPower,
				   sizeof(struct PARAM_CUSTOM_GET_TX_POWER),
				   TRUE, TRUE, TRUE, &u4BufLen);

		u4TxTargetPower = prGlueInfo->prAdapter->u4GetTxPower;
		u4TxTargetPower = ntohl(u4TxTargetPower);
		kalMemCopy(HqaCmdFrame->Data + 6, &u4TxTargetPower, 4);
	}
	ResponseToQA(HqaCmdFrame, prIwReqData, 10, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetCfgOnOff(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t Type, Enable, Band;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&Type, HqaCmdFrame->Data + 4 * 0, 4);
	Type = ntohl(Type);
	memcpy(&Enable, HqaCmdFrame->Data + 4 * 1, 4);
	Enable = ntohl(Enable);
	memcpy(&Band, HqaCmdFrame->Data + 4 * 2, 4);
	Band = ntohl(Band);

	DBGLOG(RFTEST, INFO, "Type : %u, Enable : %u, Band : %u\n",
						 Type, Enable, Band);

	switch (Type) {
	case 0: /* TSSI */
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TSSI;
		break;
	case 1: /* DPD */
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_DPD_MODE;
		break;
	default:
		DBGLOG(RFTEST, WARN, "Type [%d] not support\n", Type);
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
		return i4Ret;
	}



	rRfATInfo.u4FuncData = 0;

	if (Enable == 0)
		rRfATInfo.u4FuncData &= ~BIT(0);
	else
		rRfATInfo.u4FuncData |= BIT(0);

	if (Band == 0)
		rRfATInfo.u4FuncData &= ~BIT(1);
	else
		rRfATInfo.u4FuncData |= BIT(1);

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest, /* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo), /* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetFreqOffset(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4FreqOffset = 0;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo) {
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
		return i4Ret;
	}

	if (prGlueInfo->prAdapter)
		prChipInfo = prGlueInfo->prAdapter->chip_info;

	/* Mobile chips don't support GetFreqOffset */
	if (prChipInfo && prChipInfo->u4ChipIpVersion
						== CONNAC_CHIP_IP_VERSION) {
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
		return i4Ret;
	}

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_GET_FREQ_OFFSET;
	rRfATInfo.u4FuncData = 0;

	i4Ret = kalIoctl(prGlueInfo,
			 wlanoidRftestQueryAutoTest, &rRfATInfo,
			 sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Ret == 0) {
		u4FreqOffset = rRfATInfo.u4FuncData;

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_GetFreqOffset u4FreqOffset = %d\n",
		       u4FreqOffset);

		u4FreqOffset = ntohl(u4FreqOffset);
		memcpy(HqaCmdFrame->Data + 2, &u4FreqOffset,
		       sizeof(u4FreqOffset));
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_DBDCTXTone(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	int32_t i4BandIdx = 0, i4Control = 0, i4AntIndex = 0,
		i4ToneType = 0, i4ToneFreq = 0;
	int32_t i4DcOffsetI = 0, i4DcOffsetQ = 0, i4Band = 0,
		i4RF_Power = 0, i4Digi_Power = 0;

	memcpy(&i4BandIdx, HqaCmdFrame->Data + 4 * 0,
	       4);	/* DBDC Band Index : Band0, Band1 */
	i4BandIdx = ntohl(i4BandIdx);
	memcpy(&i4Control, HqaCmdFrame->Data + 4 * 1,
	       4);	/* Control TX Tone Start and Stop */
	i4Control = ntohl(i4Control);
	memcpy(&i4AntIndex, HqaCmdFrame->Data + 4 * 2,
	       4);	/* Select TX Antenna */
	i4AntIndex = ntohl(i4AntIndex);
	memcpy(&i4ToneType, HqaCmdFrame->Data + 4 * 3,
	       4);	/* ToneType : Single or Two */
	i4ToneType = ntohl(i4ToneType);
	memcpy(&i4ToneFreq, HqaCmdFrame->Data + 4 * 4,
	       4);	/* ToneFreq: DC/5M/10M/20M/40M */
	i4ToneFreq = ntohl(i4ToneFreq);
	memcpy(&i4DcOffsetI, HqaCmdFrame->Data + 4 * 5,
	       4);	/* DC Offset I : -512~1535 */
	i4DcOffsetI = ntohl(i4DcOffsetI);
	memcpy(&i4DcOffsetQ, HqaCmdFrame->Data + 4 * 6,
	       4);	/* DC Offset Q : -512~1535 */
	i4DcOffsetQ = ntohl(i4DcOffsetQ);
	memcpy(&i4Band, HqaCmdFrame->Data + 4 * 7,
	       4);	/* Band : 2.4G/5G */
	i4Band = ntohl(i4Band);
	memcpy(&i4RF_Power, HqaCmdFrame->Data + 4 * 8,
	       4);	/* RF_Power: (1db) 0~15 */
	i4RF_Power = ntohl(i4RF_Power);
	memcpy(&i4Digi_Power, HqaCmdFrame->Data + 4 * 9,
	       4);	/* Digi_Power: (0.25db) -32~31 */
	i4Digi_Power = ntohl(i4Digi_Power);

	DBGLOG(RFTEST, INFO, "BandIdx = 0x%08x\n", i4BandIdx);
	DBGLOG(RFTEST, INFO, "Control = 0x%08x\n", i4Control);
	DBGLOG(RFTEST, INFO, "AntIndex = 0x%08x\n", i4AntIndex);
	DBGLOG(RFTEST, INFO, "ToneType = 0x%08x\n", i4ToneType);
	DBGLOG(RFTEST, INFO, "ToneFreq = 0x%08x\n", i4ToneFreq);
	DBGLOG(RFTEST, INFO, "DcOffsetI = 0x%08x\n", i4DcOffsetI);
	DBGLOG(RFTEST, INFO, "DcOffsetQ = 0x%08x\n", i4DcOffsetQ);
	DBGLOG(RFTEST, INFO, "Band = 0x%08x\n", i4Band);
	DBGLOG(RFTEST, INFO, "RF_Power = 0x%08x\n", i4RF_Power);
	DBGLOG(RFTEST, INFO, "Digi_Power = 0x%08x\n", i4Digi_Power);

	/*
	 * Select TX Antenna
	 * RF_Power: (1db) 0~15
	 * Digi_Power: (0.25db) -32~31
	 */
	MT_ATESetDBDCTxTonePower(prNetDev, i4AntIndex, i4RF_Power,
				 i4Digi_Power);

	/* DBDC Band Index : Band0, Band1 */
	MT_ATESetDBDCBandIndex(prNetDev, i4BandIdx);

	if (i4Control) {
		/* Band : 2.4G/5G */
		MT_ATESetBand(prNetDev, i4Band);

		/* ToneType : Single or Two */
		MT_ATESetTxToneType(prNetDev, i4ToneType);

		/* ToneFreq: DC/5M/10M/20M/40M */
		MT_ATESetTxToneBW(prNetDev, i4ToneFreq);

		/* DC Offset I, DC Offset Q */
		MT_ATESetTxToneDCOffset(prNetDev, i4DcOffsetI, i4DcOffsetQ);

		/* Control TX Tone Start and Stop */
		MT_ATEDBDCTxTone(prNetDev, i4Control);
	} else {
		/* Control TX Tone Start and Stop */
		MT_ATEDBDCTxTone(prNetDev, i4Control);
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static uint8_t _whPhyGetPrimChOffset(uint32_t u4BW,
						   uint32_t u4Pri_Ch,
						   uint32_t u4Cen_ch)
{
	 uint8_t ucPrimChOffset = 0;

	/* BW Mapping in QA Tool
	 * 0: BW20
	 * 1: BW40
	 * 2: BW80
	 * 3: BW10
	 * 4: BW5
	 * 5: BW160C
	 * 6: BW160NC
	 */
	u4Pri_Ch &= 0xFF;
	u4Cen_ch &= 0xFF;
	switch (u4BW) {
	case 1:
		ucPrimChOffset = (u4Pri_Ch < u4Cen_ch) ? 0 : 1;
		break;
	case 2:
		ucPrimChOffset = (((u4Pri_Ch - u4Cen_ch) + 6) >> 2);
		break;
	default:
		break;
	}
	return ucPrimChOffset;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_DBDCContinuousTX(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Band = 0, u4Control = 0, u4AntMask = 0,
		 u4Phymode = 0, u4BW = 0;
	uint32_t u4Pri_Ch = 0, u4Rate = 0, u4Central_Ch = 0,
		 u4TxfdMode = 0, u4Freq = 0;
	uint32_t u4BufLen = 0;
	uint8_t ucPriChOffset = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band = ntohl(u4Band);
	memcpy(&u4Control, HqaCmdFrame->Data + 4 * 1, 4);
	u4Control = ntohl(u4Control);
	memcpy(&u4AntMask, HqaCmdFrame->Data + 4 * 2, 4);
	u4AntMask = ntohl(u4AntMask);
	memcpy(&u4Phymode, HqaCmdFrame->Data + 4 * 3, 4);
	u4Phymode = ntohl(u4Phymode);
	memcpy(&u4BW, HqaCmdFrame->Data + 4 * 4, 4);
	u4BW = ntohl(u4BW);
	memcpy(&u4Pri_Ch, HqaCmdFrame->Data + 4 * 5, 4);
	u4Pri_Ch = ntohl(u4Pri_Ch);
	memcpy(&u4Rate, HqaCmdFrame->Data + 4 * 6, 4);
	u4Rate = ntohl(u4Rate);
	memcpy(&u4Central_Ch, HqaCmdFrame->Data + 4 * 7, 4);
	u4Central_Ch = ntohl(u4Central_Ch);
	memcpy(&u4TxfdMode, HqaCmdFrame->Data + 4 * 8, 4);
	u4TxfdMode = ntohl(u4TxfdMode);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4Band : %d\n",
	       u4Band);	/* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4Control : %d\n",
	       u4Control);	/* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4AntMask : %d\n",
	       u4AntMask);	/* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4Phymode : %d\n",
	       u4Phymode);	/* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4BW : %d\n", u4BW); /* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4Pri_Ch : %d\n",
	       u4Pri_Ch);	/* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4Rate : %d\n",
	       u4Rate);	/* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4Central_Ch : %d\n",
	       u4Central_Ch);	/* ok */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DBDCContinuousTX u4TxfdMode : %d\n",
	       u4TxfdMode);	/* ok */

	if (u4Control) {
		MT_ATESetDBDCBandIndex(prNetDev, u4Band);
		u4Freq = nicChannelNum2Freq(u4Central_Ch);
		MT_ATESetChannel(prNetDev, 0, u4Freq);
		ucPriChOffset = _whPhyGetPrimChOffset(u4BW,
						      u4Pri_Ch,
						      u4Central_Ch);
		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_DBDCContinuousTX ucPriChOffset : %d\n",
		       ucPriChOffset);	/* ok */
		MT_ATEPrimarySetting(prNetDev, ucPriChOffset);

		if (u4Phymode == 1) {
			u4Phymode = 0;
			u4Rate += 4;
		} else if ((u4Phymode == 0) &&
			   ((u4Rate == 9) || (u4Rate == 10) || (u4Rate == 11)))
			u4Phymode = 1;
		MT_ATESetPreamble(prNetDev, u4Phymode);

		if (u4Phymode == 0) {
			u4Rate |= 0x00000000;

			DBGLOG(RFTEST, INFO,
			       "QA_AGENT CCK/OFDM (normal preamble) rate : %d\n",
			       u4Rate);

			MT_ATESetRate(prNetDev, u4Rate);
		} else if (u4Phymode == 1) {
			if (u4Rate == 9)
				u4Rate = 1;
			else if (u4Rate == 10)
				u4Rate = 2;
			else if (u4Rate == 11)
				u4Rate = 3;
			u4Rate |= 0x00000000;

			DBGLOG(RFTEST, INFO,
			       "QA_AGENT CCK (short preamble) rate : %d\n",
			       u4Rate);

			MT_ATESetRate(prNetDev, u4Rate);
		} else if (u4Phymode >= 2 && u4Phymode <= 4) {
			u4Rate |= 0x80000000;

			DBGLOG(RFTEST, INFO, "QA_AGENT HT/VHT rate : %d\n",
			       u4Rate);

			MT_ATESetRate(prNetDev, u4Rate);
		}

		MT_ATESetSystemBW(prNetDev, u4BW);

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_CW_MODE;
		rRfATInfo.u4FuncData = u4TxfdMode;

		rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
				   wlanoidRftestSetAutoTest, /* pfnOidHandler */
				   &rRfATInfo,	/* pvInfoBuf */
				   sizeof(rRfATInfo), /* u4InfoBufLen */
				   FALSE,	/* fgRead */
				   FALSE,	/* fgWaitResp */
				   TRUE,	/* fgCmd */
				   &u4BufLen);	/* pu4QryInfoLen */

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ANTMASK;
		rRfATInfo.u4FuncData = u4AntMask;

		rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
				   wlanoidRftestSetAutoTest, /* pfnOidHandler */
				   &rRfATInfo,	/* pvInfoBuf */
				   sizeof(rRfATInfo), /* u4InfoBufLen */
				   FALSE,	/* fgRead */
				   FALSE,	/* fgWaitResp */
				   TRUE,	/* fgCmd */
				   &u4BufLen);	/* pu4QryInfoLen */

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
		rRfATInfo.u4FuncData = RF_AT_COMMAND_CW;

		rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
				   wlanoidRftestSetAutoTest, /* pfnOidHandler */
				   &rRfATInfo,	/* pvInfoBuf */
				   sizeof(rRfATInfo), /* u4InfoBufLen */
				   FALSE,	/* fgRead */
				   FALSE,	/* fgWaitResp */
				   TRUE,	/* fgCmd */
				   &u4BufLen);	/* pu4QryInfoLen */

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	} else {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
		rRfATInfo.u4FuncData = RF_AT_COMMAND_STOPTEST;

		rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
				   wlanoidRftestSetAutoTest, /* pfnOidHandler */
				   &rRfATInfo,	/* pvInfoBuf */
				   sizeof(rRfATInfo), /* u4InfoBufLen */
				   FALSE,	/* fgRead */
				   FALSE,	/* fgWaitResp */
				   TRUE,	/* fgCmd */
				   &u4BufLen);	/* pu4QryInfoLen */

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetRXFilterPktLen(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Band = 0, u4Control = 0, u4RxPktlen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band = ntohl(u4Band);
	memcpy(&u4Control, HqaCmdFrame->Data + 4 * 1, 4);
	u4Control = ntohl(u4Control);
	memcpy(&u4RxPktlen, HqaCmdFrame->Data + 4 * 2, 4);
	u4RxPktlen = ntohl(u4RxPktlen);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXFilterPktLen Band : %d\n", u4Band);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXFilterPktLen Control : %d\n", u4Control);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXFilterPktLen RxPktlen : %d\n",
	       u4RxPktlen);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_RX_FILTER_PKT_LEN;
	rRfATInfo.u4FuncData = (uint32_t) (u4RxPktlen & BITS(0,
					   23));
	rRfATInfo.u4FuncData |= (uint32_t) (u4Band << 24);

	if (u4Control == 1)
		rRfATInfo.u4FuncData |= BIT(30);
	else
		rRfATInfo.u4FuncData &= ~BIT(30);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetRXFilterPktLen rRfATInfo.u4FuncData : 0x%08x\n",
	       rRfATInfo.u4FuncData);

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetTXInfo(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t u4Txed_band0 = 0;
	uint32_t u4Txed_band1 = 0;
	int32_t i4Status;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetTXInfo\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_TXED_COUNT;
	rRfATInfo.u4FuncData = 0;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidRftestQueryAutoTest, &rRfATInfo,
			    sizeof(rRfATInfo),
			    TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Txed_band0 = rRfATInfo.u4FuncData;

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT u4Txed_band0 packet count = %d\n",
		       u4Txed_band0);

		u4Txed_band0 = ntohl(u4Txed_band0);
		memcpy(HqaCmdFrame->Data + 2, &u4Txed_band0,
		       sizeof(u4Txed_band0));
	}

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_TXED_COUNT;
	rRfATInfo.u4FuncIndex |= BIT(8);
	rRfATInfo.u4FuncData = 0;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidRftestQueryAutoTest, &rRfATInfo,
			    sizeof(rRfATInfo),
			    TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Txed_band1 = rRfATInfo.u4FuncData;

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT u4Txed_band1 packet count = %d\n",
		       u4Txed_band1);

		u4Txed_band1 = ntohl(u4Txed_band1);
		memcpy(HqaCmdFrame->Data + 2 + 4, &u4Txed_band1,
		       sizeof(u4Txed_band1));
	}

	ResponseToQA(HqaCmdFrame, prIwReqData,
		     2 + sizeof(u4Txed_band0) + sizeof(u4Txed_band1), i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetCfgOnOff(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetCfgOnOff\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_CMD_SET3[] = {
	/* cmd id start from 0x1300 */
	HQA_MacBbpRegRead,	/* 0x1300 */
	HQA_MacBbpRegWrite,	/* 0x1301 */
	HQA_MACBbpRegBulkRead,	/* 0x1302 */
	HQA_RfRegBulkRead,	/* 0x1303 */
	HQA_RfRegBulkWrite,	/* 0x1304 */
	HQA_ReadEEPROM,		/* 0x1305 */
	HQA_WriteEEPROM,	/* 0x1306 */
	HQA_ReadBulkEEPROM,	/* 0x1307 */
	HQA_WriteBulkEEPROM,	/* 0x1308 */
	HQA_CheckEfuseMode,	/* 0x1309 */
	HQA_GetFreeEfuseBlock,	/* 0x130A */
	HQA_GetEfuseBlockNr,	/* 0x130B */
	HQA_WriteEFuseFromBuffer,	/* 0x130C */
	HQA_GetTxPower,		/* 0x130D */
	HQA_SetCfgOnOff,	/* 0x130E */
	HQA_GetFreqOffset,	/* 0x130F */
	HQA_DBDCTXTone,		/* 0x1310 */
	HQA_DBDCContinuousTX,	/* 0x1311 */
	HQA_SetRXFilterPktLen,	/* 0x1312 */
	HQA_GetTXInfo,		/* 0x1313 */
	HQA_GetCfgOnOff,	/* 0x1314 */
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_ReadTempReferenceValue(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_ReadTempReferenceValue\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Get Thermal Value.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetThermalValue(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	uint32_t u4Value;
	uint32_t u4BufLen = 0;
	int32_t i4Status;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_TEMP_SENSOR;
	rRfATInfo.u4FuncData = 0;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidRftestQueryAutoTest, &rRfATInfo,
			    sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Value = rRfATInfo.u4FuncData;
		u4Value = u4Value >> 16;

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_GetThermalValue Value = %d\n", u4Value);

		u4Value = ntohl(u4Value);
		memcpy(HqaCmdFrame->Data + 2, &u4Value, 4);
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetSideBandOption(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetSideBandOption\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_CMD_SET4[] = {
	/* cmd id start from 0x1400 */
	HQA_ReadTempReferenceValue,	/* 0x1400 */
	HQA_GetThermalValue,	/* 0x1401 */
	HQA_SetSideBandOption,	/* 0x1402 */
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetFWInfo(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetFWInfo\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StartContinousTx(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartContinousTx\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetSTBC(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetSTBC\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Set short GI.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetShortGI(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4ShortGi;

	memcpy(&u4ShortGi, HqaCmdFrame->Data, 4);
	u4ShortGi = ntohl(u4ShortGi);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetShortGI u4ShortGi = %d\n", u4ShortGi);

	i4Ret = MT_ATESetTxGi(prNetDev, u4ShortGi);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetDPD(struct net_device *prNetDev,
			  IN union iwreq_data *prIwReqData,
			  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetDPD\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Get Rx Statistics.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetRxStatisticsAll(struct net_device
				      *prNetDev,
				      IN union iwreq_data *prIwReqData,
				      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen = 0;
	struct PARAM_CUSTOM_ACCESS_RX_STAT rRxStatisticsTest;

	/* memset(&g_HqaRxStat, 0, sizeof(PARAM_RX_STAT_T)); */
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetRxStatisticsAll\n");

	rRxStatisticsTest.u4SeqNum = u4RxStatSeqNum;
	rRxStatisticsTest.u4TotalNum = HQA_RX_STATISTIC_NUM + 6;

	i4Ret = kalIoctl(prGlueInfo,
			 wlanoidQueryRxStatistics,
			 &rRxStatisticsTest, sizeof(rRxStatisticsTest),
			 TRUE, TRUE, TRUE, &u4BufLen);

	/* ASSERT(rRxStatisticsTest.u4SeqNum == u4RxStatSeqNum); */

	u4RxStatSeqNum++;

	memcpy(HqaCmdFrame->Data + 2, &(g_HqaRxStat),
	       sizeof(struct PARAM_RX_STAT));
	ResponseToQA(HqaCmdFrame, prIwReqData,
		     (2 + sizeof(struct PARAM_RX_STAT)), i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StartContiTxTone(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StartContiTxTone\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_StopContiTxTone(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StopContiTxTone\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CalibrationTestMode(struct net_device
				       *prNetDev,
				       IN union iwreq_data *prIwReqData,
				       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Mode = 0;
	uint32_t u4IcapLen = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_CalibrationTestMode\n");

	memcpy(&u4Mode, HqaCmdFrame->Data + 4 * 0, 4);
	u4Mode = ntohl(u4Mode);
	memcpy(&u4IcapLen, HqaCmdFrame->Data + 4 * 1, 4);
	u4IcapLen = ntohl(u4IcapLen);

	if (u4Mode == 2)
		i4Ret = MT_ICAPStart(prNetDev, "ICAPSTART");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_DoCalibrationTestItem(struct net_device
		*prNetDev,
		IN union iwreq_data *prIwReqData,
		struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Status = 0;
	uint32_t u4Item = 0;
	uint32_t u4Band_idx = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	struct ADAPTER *prAdapter;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	memcpy(&u4Item, HqaCmdFrame->Data, 4);
	u4Item = ntohl(u4Item);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DoCalibrationTestItem item : 0x%08x\n",
	       u4Item);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_DoCalibrationTestItem band_idx : %d\n",
	       u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RECAL_CAL_STEP;
	rRfATInfo.u4FuncData = u4Item;

	kalMemSet((void *)&prAdapter->rReCalInfo,
			  0,
			  sizeof(struct RECAL_INFO_T));
	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_eFusePhysicalWrite(struct net_device
				      *prNetDev,
				      IN union iwreq_data *prIwReqData,
				      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_eFusePhysicalWrite\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_eFusePhysicalRead(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_eFusePhysicalRead\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_eFuseLogicalRead(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_eFuseLogicalRead\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_eFuseLogicalWrite(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_eFuseLogicalWrite\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_TMRSetting(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Setting;
	uint32_t u4Version;
	uint32_t u4MPThres;
	uint32_t u4MPIter;

	memcpy(&u4Setting, HqaCmdFrame->Data + 4 * 0, 4);
	u4Setting = ntohl(u4Setting);
	memcpy(&u4Version, HqaCmdFrame->Data + 4 * 1, 4);
	u4Version = ntohl(u4Version);
	memcpy(&u4MPThres, HqaCmdFrame->Data + 4 * 2, 4);
	u4MPThres = ntohl(u4MPThres);
	memcpy(&u4MPIter, HqaCmdFrame->Data + 4 * 3, 4);
	u4MPIter = ntohl(u4MPIter);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TMRSetting u4Setting : %d\n", u4Setting);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TMRSetting u4Version : %d\n", u4Version);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TMRSetting u4MPThres : %d\n", u4MPThres);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TMRSetting u4MPIter : %d\n", u4MPIter);

	i4Ret = MT_ATE_TMRSetting(prNetDev, u4Setting, u4Version,
				  u4MPThres, u4MPIter);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetRxSNR(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetRxSNR\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_WriteBufferDone(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	/* UINT_16 u2InitAddr = 0x000; */
	uint32_t Value;
	/* UINT_32 i = 0, j = 0;
	 * UINT_32 u4BufLen = 0;
	 */
	/* WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS; */
	struct GLUE_INFO *prGlueInfo = NULL;
	/* PARAM_CUSTOM_EFUSE_BUFFER_MODE_T rSetEfuseBufModeInfo; */

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&Value, HqaCmdFrame->Data + 4 * 0, 4);
	Value = ntohl(Value);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_WriteBufferDone Value : %d\n", Value);

	u4EepromMode = Value;

#if 0
	for (i = 0 ; i < MAX_EEPROM_BUFFER_SIZE / 16 ; i++) {
		for (j = 0 ; j < 16 ; j++) {
			rSetEfuseBufModeInfo.aBinContent[j].u2Addr = u2InitAddr;
			rSetEfuseBufModeInfo.aBinContent[j].ucValue =
				uacEEPROMImage[u2InitAddr];
			DBGLOG(RFTEST, INFO, "u2Addr = %x\n",
			       rSetEfuseBufModeInfo.aBinContent[j].u2Addr);
			DBGLOG(RFTEST, INFO, "ucValue = %x\n",
			       rSetEfuseBufModeInfo.aBinContent[j].ucValue);
			u2InitAddr += 1;
		}

		rSetEfuseBufModeInfo.ucSourceMode = 1;
		rSetEfuseBufModeInfo.ucCount = EFUSE_CONTENT_SIZE;
		rStatus = kalIoctl(prGlueInfo,
				wlanoidSetEfusBufferMode,
				&rSetEfuseBufModeInfo,
				sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
				FALSE, FALSE, TRUE, &u4BufLen);
	}
#endif

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_FFT(struct net_device *prNetDev,
		       IN union iwreq_data *prIwReqData,
		       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_FFT\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetTxTonePower(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetTxTonePower\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetChipID(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4ChipId;
	struct mt66xx_chip_info *prChipInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	/* UINT_32 u4BufLen = 0;
	 * PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	 */

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prChipInfo = prAdapter->chip_info;
	g_u4Chip_ID = prChipInfo->chip_id;
	DBGLOG(RFTEST, INFO,
		"QA_AGENT IPVer= 0x%08x, Adie = 0x%08x\n",
		prChipInfo->u4ChipIpVersion,
		prChipInfo->u2ADieChipVersion);

	/* Check A-Die information for mobile solution */
	switch (prChipInfo->u2ADieChipVersion) {
	case 0x6631:
		u4ChipId = 0x00066310;	/* use 66310 to diff from gen3 6631 */
		break;
	case 0x6635:
		u4ChipId = 0x0006635;	/* return A die directly */
		break;
	default:
		u4ChipId = g_u4Chip_ID;
		break;
	}

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_GetChipID ChipId = 0x%08x\n", u4ChipId);

	u4ChipId = ntohl(u4ChipId);
	memcpy(HqaCmdFrame->Data + 2, &u4ChipId, 4);
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSSetSeqData(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		*mps_setting = NULL;
	uint32_t		u4Band_idx = 0;
	uint32_t		u4Offset = 0;
	uint32_t		u4Len = 0;
	uint32_t		i = 0;
	uint32_t		u4Value = 0;
	uint32_t		u4Mode = 0;
	uint32_t		u4TxPath = 0;
	uint32_t		u4Mcs = 0;

	u4Len = ntohs(HqaCmdFrame->Length) / sizeof(uint32_t) - 1;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetSeqData u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(uint32_t) * (u4Len),
			      GFP_KERNEL);
	ASSERT(mps_setting);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetSeqData u4Band_idx : %d\n", u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		/* Reserved at least 4 byte availbale data */
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data))
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4 * i, 4);
		u4Value = ntohl(u4Value);

		u4Mode = (u4Value & BITS(24, 27)) >> 24;
		u4TxPath = (u4Value & BITS(8, 23)) >> 8;
		u4Mcs = (u4Value & BITS(0, 7));

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_MPSSetSeqData mps_setting Case %d (Mode : %d / TX Path : %d / MCS : %d)\n"
		       , i, u4Mode, u4TxPath, u4Mcs);

		if (u4Mode == 1) {
			u4Mode = 0;
			u4Mcs += 4;
		} else if ((u4Mode == 0) && ((u4Mcs == 9) || (u4Mcs == 10)
					     || (u4Mcs == 11)))
			u4Mode = 1;

		if (u4Mode == 0) {
			u4Mcs |= 0x00000000;

			DBGLOG(RFTEST, INFO,
			       "QA_AGENT CCK/OFDM (normal preamble) rate : %d\n",
			       u4Mcs);
		} else if (u4Mode == 1) {
			if (u4Mcs == 9)
				u4Mcs = 1;
			else if (u4Mcs == 10)
				u4Mcs = 2;
			else if (u4Mcs == 11)
				u4Mcs = 3;
			u4Mcs |= 0x00000000;

			DBGLOG(RFTEST, INFO,
			       "QA_AGENT CCK (short preamble) rate : %d\n",
			       u4Mcs);
		} else if (u4Mode >= 2 && u4Mode <= 4) {
			u4Mcs |= 0x80000000;

			DBGLOG(RFTEST, INFO, "QA_AGENT HT/VHT rate : %d\n",
			       u4Mcs);
		}

		mps_setting[i] = (u4Mcs) | (u4TxPath << 8) | (u4Mode << 24);

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_MPSSetSeqData mps_setting Case %d (Mode : %d / TX Path : %d / MCS : %d)\n",
		       i,
		       (int)((mps_setting[i] & BITS(24, 27)) >> 24),
		       (int)((mps_setting[i] & BITS(8, 23)) >> 8),
		       (int)((mps_setting[i] & BITS(0, 7))));

	}

	i4Ret = MT_ATEMPSSetSeqData(prNetDev, u4Len, mps_setting,
				    u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSSetPayloadLength(struct net_device
				       *prNetDev,
				       IN union iwreq_data *prIwReqData,
				       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		*mps_setting = NULL;
	uint32_t		u4Band_idx = 0;
	uint32_t		u4Offset = 0;
	uint32_t		u4Len = 0;
	uint32_t		i = 0;
	uint32_t		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length) / sizeof(uint32_t) - 1;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPayloadLength u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(uint32_t) * (u4Len),
			      GFP_KERNEL);
	ASSERT(mps_setting);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPayloadLength u4Band_idx : %d\n",
	       u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		/* Reserved at least 4 byte availbale data */
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data))
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4 * i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_MPSSetPayloadLength mps_setting Case %d (Payload Length : %d)\n",
		       i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPayloadLength(prNetDev, u4Len,
					  mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSSetPacketCount(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		*mps_setting = NULL;
	uint32_t		u4Band_idx = 0;
	uint32_t		u4Offset = 0;
	uint32_t		u4Len = 0;
	uint32_t		i = 0;
	uint32_t		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length) / sizeof(uint32_t) - 1;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPacketCount u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(uint32_t) * (u4Len),
			      GFP_KERNEL);
	ASSERT(mps_setting);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPacketCount u4Band_idx : %d\n",
	       u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		/* Reserved at least 4 byte availbale data */
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data))
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4 * i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_MPSSetPacketCount mps_setting Case %d (Packet Count : %d)\n",
		       i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPacketCount(prNetDev, u4Len,
					mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSSetPowerGain(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		*mps_setting = NULL;
	uint32_t		u4Band_idx = 0;
	uint32_t		u4Offset = 0;
	uint32_t		u4Len = 0;
	uint32_t		i = 0;
	uint32_t		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length) / sizeof(uint32_t) - 1;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPowerGain u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(uint32_t) * (u4Len),
			      GFP_KERNEL);
	ASSERT(mps_setting);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPowerGain u4Band_idx : %d\n",
	       u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		/* Reserved at least 4 byte availbale data */
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data))
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4 * i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_MPSSetPowerGain mps_setting Case %d (Power : %d)\n",
		       i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPowerGain(prNetDev, u4Len, mps_setting,
				      u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSStart(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		u4Band_idx = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MPSStart\n");

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATEStartTX(prNetDev, "TXFRAME");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSStop(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		u4Band_idx = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MPSStop\n");

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	/* To Do : MPS Stop for Specific Band. */

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief  internal function used by HQA_GetRxStatisticsAllV2.
 */
/*----------------------------------------------------------------------------*/
int32_t hqa_genStatBandReport(
	u_int8_t band_idx,
	u_int8_t blk_idx,
	struct hqa_rx_stat_band_format *rx_st_band)
{

	int32_t ret = 0;

	if (band_idx == HQA_M_BAND_0) {
		rx_st_band->mac_rx_fcs_err_cnt =
			ntohl(g_HqaRxStat.MAC_FCS_Err) +
			ntohl(g_backup_band0_info.mac_rx_fcs_err_cnt);
		rx_st_band->mac_rx_mdrdy_cnt =
			ntohl(g_HqaRxStat.MAC_Mdrdy) +
			ntohl(g_backup_band0_info.mac_rx_mdrdy_cnt);
		rx_st_band->mac_rx_len_mismatch =
			ntohl(g_HqaRxStat.LengthMismatchCount_B0) +
			ntohl(g_backup_band0_info.mac_rx_len_mismatch);
		rx_st_band->mac_rx_fcs_ok_cnt = 0;
		rx_st_band->phy_rx_fcs_err_cnt_cck =
			ntohl(g_HqaRxStat.FCSErr_CCK);
		rx_st_band->phy_rx_fcs_err_cnt_ofdm =
			ntohl(g_HqaRxStat.FCSErr_OFDM);
		rx_st_band->phy_rx_pd_cck =
			ntohl(g_HqaRxStat.CCK_PD);
		rx_st_band->phy_rx_pd_ofdm =
			ntohl(g_HqaRxStat.OFDM_PD);
		rx_st_band->phy_rx_sig_err_cck =
			ntohl(g_HqaRxStat.CCK_SIG_Err);
		rx_st_band->phy_rx_sfd_err_cck =
			ntohl(g_HqaRxStat.CCK_SFD_Err);
		rx_st_band->phy_rx_sig_err_ofdm =
			ntohl(g_HqaRxStat.OFDM_SIG_Err);
		rx_st_band->phy_rx_tag_err_ofdm =
			ntohl(g_HqaRxStat.OFDM_TAG_Err);
		rx_st_band->phy_rx_mdrdy_cnt_cck =
			ntohl(g_HqaRxStat.PhyMdrdyCCK);
		rx_st_band->phy_rx_mdrdy_cnt_ofdm =
			ntohl(g_HqaRxStat.PhyMdrdyOFDM);

		/* Backup Band1 info */
		g_backup_band1_info.mac_rx_fcs_err_cnt +=
			g_HqaRxStat.MAC_FCS_Err1;

		g_backup_band1_info.mac_rx_mdrdy_cnt +=
			g_HqaRxStat.MAC_Mdrdy1;

		g_backup_band1_info.mac_rx_len_mismatch +=
			g_HqaRxStat.LengthMismatchCount_B1;

		/* Reset Band0 backup info */
		kalMemZero(&g_backup_band0_info,
			sizeof(struct hqa_rx_stat_band_format));
	} else {
		rx_st_band->mac_rx_fcs_err_cnt =
			ntohl(
			g_HqaRxStat.MAC_FCS_Err1) +
			ntohl(
			g_backup_band1_info.mac_rx_fcs_err_cnt);
		rx_st_band->mac_rx_mdrdy_cnt =
			ntohl(
			g_HqaRxStat.MAC_Mdrdy1) +
			ntohl(
			g_backup_band1_info.mac_rx_mdrdy_cnt);
		rx_st_band->mac_rx_len_mismatch =
			ntohl(
			g_HqaRxStat.LengthMismatchCount_B1) +
			ntohl(
			g_backup_band1_info.mac_rx_len_mismatch);
		rx_st_band->mac_rx_fcs_ok_cnt = 0;
		rx_st_band->phy_rx_fcs_err_cnt_cck =
			ntohl(
			g_HqaRxStat.CCK_FCS_Err_Band1);
		rx_st_band->phy_rx_fcs_err_cnt_ofdm =
			ntohl(
			g_HqaRxStat.OFDM_FCS_Err_Band1);
		rx_st_band->phy_rx_pd_cck =
			ntohl(
			g_HqaRxStat.CCK_PD_Band1);
		rx_st_band->phy_rx_pd_ofdm =
			ntohl(
			g_HqaRxStat.OFDM_PD_Band1);
		rx_st_band->phy_rx_sig_err_cck =
			ntohl(
			g_HqaRxStat.CCK_SIG_Err_Band1);
		rx_st_band->phy_rx_sfd_err_cck =
			ntohl(
			g_HqaRxStat.CCK_SFD_Err_Band1);
		rx_st_band->phy_rx_sig_err_ofdm =
			ntohl(
			g_HqaRxStat.OFDM_SIG_Err_Band1);
		rx_st_band->phy_rx_tag_err_ofdm =
			ntohl(
			g_HqaRxStat.OFDM_TAG_Err_Band1);
		rx_st_band->phy_rx_mdrdy_cnt_cck =
			ntohl(
			g_HqaRxStat.PHY_CCK_MDRDY_Band1);
		rx_st_band->phy_rx_mdrdy_cnt_ofdm =
			ntohl(
			g_HqaRxStat.PHY_OFDM_MDRDY_Band1);


		/* Backup Band0 info */
		g_backup_band0_info.mac_rx_fcs_err_cnt +=
			g_HqaRxStat.MAC_FCS_Err;

		g_backup_band0_info.mac_rx_mdrdy_cnt +=
			g_HqaRxStat.MAC_Mdrdy;

		g_backup_band0_info.mac_rx_len_mismatch +=
			g_HqaRxStat.LengthMismatchCount_B0;

		/* Reset Band1 backup info */
		kalMemZero(&g_backup_band1_info,
			sizeof(struct hqa_rx_stat_band_format));
	}

	return ret;
}

int32_t hqa_genStatPathReport(
	u_int8_t band_idx,
	u_int8_t blk_idx,
	struct hqa_rx_stat_path_format *rx_st_path)
{
	int32_t ret = 0;

	switch (blk_idx) {
	case HQA_ANT_WF0:
		rx_st_path->rcpi =
			ntohl(g_HqaRxStat.RCPI0);
		rx_st_path->rssi =
			ntohl(g_HqaRxStat.RSSI0);
		rx_st_path->fagc_ib_rssi =
			ntohl(g_HqaRxStat.FAGCRssiIBR0);
		rx_st_path->fagc_wb_rssi =
			ntohl(g_HqaRxStat.FAGCRssiWBR0);
		rx_st_path->inst_ib_rssi =
			ntohl(g_HqaRxStat.InstRssiIBR0);
		rx_st_path->inst_wb_rssi =
			ntohl(g_HqaRxStat.InstRssiWBR0);
		break;
	case HQA_ANT_WF1:
		rx_st_path->rcpi =
			ntohl(g_HqaRxStat.RCPI1);
		rx_st_path->rssi =
			ntohl(g_HqaRxStat.RSSI1);
		rx_st_path->fagc_ib_rssi =
			ntohl(g_HqaRxStat.FAGCRssiIBR1);
		rx_st_path->fagc_wb_rssi =
			ntohl(g_HqaRxStat.FAGCRssiWBR1);
		rx_st_path->inst_ib_rssi =
			ntohl(g_HqaRxStat.InstRssiIBR1);
		rx_st_path->inst_wb_rssi =
			ntohl(g_HqaRxStat.InstRssiWBR1);
		break;

	default:
		ret = WLAN_STATUS_INVALID_DATA;
		break;
	}

	return ret;
}

int32_t hqa_genStatUserReport(
	u_int8_t band_idx,
	u_int8_t blk_idx,
	struct hqa_rx_stat_user_format *rx_st_user)
{
	int32_t ret = WLAN_STATUS_SUCCESS;

	rx_st_user->freq_offset_from_rx =
		ntohl(g_HqaRxStat.FreqOffsetFromRX);
	if (band_idx == HQA_M_BAND_0)
		rx_st_user->snr = ntohl(g_HqaRxStat.SNR0);
	else
		rx_st_user->snr = ntohl(g_HqaRxStat.SNR1);

	rx_st_user->fcs_error_cnt =
		ntohl(g_HqaRxStat.MAC_FCS_Err);

	return ret;
}

int32_t hqa_genStatCommReport(
	u_int8_t band_idx,
	u_int8_t blk_idx,
	struct hqa_rx_stat_comm_format *rx_st_comm)
{
	int32_t ret = WLAN_STATUS_SUCCESS;

	rx_st_comm->rx_fifo_full =
		ntohl(g_HqaRxStat.OutOfResource);
	rx_st_comm->aci_hit_low =
		ntohl(g_HqaRxStat.ACIHitLower);
	rx_st_comm->aci_hit_high =
		ntohl(g_HqaRxStat.ACIHitUpper);
	rx_st_comm->mu_pkt_count =
		ntohl(g_HqaRxStat.MRURxCount);
	rx_st_comm->sig_mcs =
		ntohl(g_HqaRxStat.SIGMCS);
	rx_st_comm->sinr =
		ntohl(g_HqaRxStat.SINR);
	if (band_idx == HQA_M_BAND_0) {
		rx_st_comm->driver_rx_count =
		ntohl(g_HqaRxStat.DriverRxCount);
	} else {
		rx_st_comm->driver_rx_count =
		ntohl(g_HqaRxStat.DriverRxCount1);
	}
	return ret;
}


int32_t hqa_getRxStatisticsByType(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t band_idx,
	u_int8_t blk_idx,
	u_int8_t test_rx_stat_cat,
	struct hqa_rx_stat_u *st)
{
	int32_t i4Ret = 0;
	uint32_t u4BufLen = 0;
	struct PARAM_CUSTOM_ACCESS_RX_STAT rx_stat_test;

	rx_stat_test.u4SeqNum = 0;
	rx_stat_test.u4TotalNum = 72;

	/* only TEST_RX_STAT_BAND send query command to FW. */
	if (test_rx_stat_cat == HQA_RX_STAT_BAND) {
		i4Ret = kalIoctl(prGlueInfo,
			 wlanoidQueryRxStatistics,
			 &rx_stat_test, sizeof(rx_stat_test),
			 TRUE, TRUE, TRUE, &u4BufLen);
	}

	switch (test_rx_stat_cat) {
	case HQA_RX_STAT_BAND:
		i4Ret = hqa_genStatBandReport(
		band_idx,
		blk_idx,
		&(st->u.rx_st_band));
		break;
	case HQA_RX_STAT_PATH:
		i4Ret = hqa_genStatPathReport(
		band_idx,
		blk_idx,
		&(st->u.rx_st_path));
		break;
	case HQA_RX_STAT_USER:
		i4Ret = hqa_genStatUserReport(
		band_idx,
		blk_idx,
		&(st->u.rx_st_user));
		break;
	case HQA_RX_STAT_COMM:
		i4Ret = hqa_genStatCommReport(
		band_idx,
		blk_idx,
		&(st->u.rx_st_comm));
		break;
	default:
		break;
	}

	if (i4Ret)
		DBGLOG(RFTEST, INFO, "err=0x%08x\n.", i4Ret);

	return i4Ret;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Get Rx Statistics.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetRxStatisticsAllV2(struct net_device
				      *prNetDev,
				      IN union iwreq_data *prIwReqData,
				      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t	i4Ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	u_int32_t buf_size = 0;
	u_int32_t type_mask = 0, band_idx = 0, type_num = 0, length;
	u_int32_t blk_idx = 0, type_idx = 0, buf = 0;
	u_int32_t dw_idx = 0, dw_cnt = 0;
	u_int32_t *ptr2 = NULL;
	struct hqa_rx_stat_u *rx_stat = NULL;
	u_int8_t path[HQA_ANT_NUM] = {0};
	u_int8_t path_len = 0;
	u_int8_t *ptr = NULL;
	u_int8_t i = 0;

	struct hqa_rx_stat_resp_field st_form[HQA_SERV_RX_STAT_TYPE_NUM] = {
	 {HQA_SERV_RX_STAT_TYPE_BAND, 0, 0, 0,
		 sizeof(struct hqa_rx_stat_band_format)},
	 {HQA_SERV_RX_STAT_TYPE_PATH, 0, 0, 0,
		 sizeof(struct hqa_rx_stat_path_format)},
	 {HQA_SERV_RX_STAT_TYPE_USER, 0, 0, 0,
		 sizeof(struct hqa_rx_stat_user_format)},
	 {HQA_SERV_RX_STAT_TYPE_COMM, 0, 0, 0,
		 sizeof(struct hqa_rx_stat_comm_format)}
	};

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_GetRxStatisticsAllV2\n");


	/* Request format type */
	memcpy(&type_mask, HqaCmdFrame->Data + 4 * 0, 4);
	type_mask = ntohl(type_mask);

	memcpy(&band_idx, HqaCmdFrame->Data + 4 * 1, 4);
	band_idx = ntohl(band_idx);

	DBGLOG(RFTEST, INFO, "type_mask = %d, band_idx = %d\n",
		type_mask, band_idx);

	/* sanity check for band index param */
	if ((!g_DBDCEnable) && (band_idx != HQA_M_BAND_0))
		goto error2;

	/* check wifi path combination for specific band */
	/* check with Yenchih */
	if (g_DBDCEnable) {
		path_len = 1;
		if (band_idx == HQA_M_BAND_0)
			path[0] = 0;
		else
			path[0] = 1;
	} else {
		path_len = 2;
		for (i = 0; i < path_len; i++)
			path[i] = i;
	}

	/* update item mask for each type */
	st_form[HQA_SERV_RX_STAT_TYPE_BAND].item_mask = BIT(band_idx);
	for (blk_idx = 0; blk_idx < path_len; blk_idx++)
		st_form[HQA_SERV_RX_STAT_TYPE_PATH].item_mask |=
			BIT(path[blk_idx]);
	for (blk_idx = 0; blk_idx < HQA_USER_NUM; blk_idx++)
		st_form[HQA_SERV_RX_STAT_TYPE_USER].item_mask |=
			BIT(blk_idx);
	st_form[HQA_SERV_RX_STAT_TYPE_COMM].item_mask = BIT(0);

	/* update block count for each type */
	for (type_idx = HQA_SERV_RX_STAT_TYPE_BAND;
		type_idx < HQA_SERV_RX_STAT_TYPE_NUM; type_idx++) {
		for (blk_idx = 0; blk_idx < 32; blk_idx++) {
			if (st_form[type_idx].item_mask & BIT(blk_idx))
				st_form[type_idx].blk_cnt++;
		}
	}

	ptr = HqaCmdFrame->Data + 2 + sizeof(type_num);

	/* allocate dynamic memory for rx stat info */
	rx_stat = kalMemAlloc(sizeof(struct hqa_rx_stat_u), VIR_MEM_TYPE);
	if (!rx_stat) {
		i4Ret = WLAN_STATUS_RESOURCES;
		goto error1;
	}

	for (type_idx = HQA_SERV_RX_STAT_TYPE_BAND;
			type_idx < HQA_SERV_RX_STAT_TYPE_NUM; type_idx++) {
		if (type_mask & BIT(type_idx)) {
			type_num++;
			length = st_form[type_idx].blk_cnt *
				st_form[type_idx].blk_size;

			/* fill in type */
			buf = htonl(st_form[type_idx].type);
			kalMemMove(ptr, &buf, sizeof(buf));
			ptr += sizeof(st_form[type_idx].type);
			buf_size += sizeof(st_form[type_idx].type);

			/* fill in version */
			buf = htonl(st_form[type_idx].version);
			kalMemMove(ptr, &buf, sizeof(buf));
			ptr += sizeof(st_form[type_idx].version);
			buf_size += sizeof(st_form[type_idx].version);

			/* fill in item mask */
			buf = htonl(st_form[type_idx].item_mask);
			kalMemMove(ptr, &buf, sizeof(buf));
			ptr += sizeof(st_form[type_idx].item_mask);
			buf_size += sizeof(st_form[type_idx].item_mask);

			/* fill in length */
			buf = htonl(length);
			kalMemMove(ptr, &buf, sizeof(buf));
			ptr += sizeof(length);
			buf_size += sizeof(length);

			for (blk_idx = 0; blk_idx < 32; blk_idx++) {
				if (st_form[type_idx].item_mask
						& BIT(blk_idx)) {
					/* service handle for rx stat info */
					hqa_getRxStatisticsByType(prGlueInfo,
						band_idx,
						blk_idx,
						type_idx,
						rx_stat);
					ptr2 = (u_int32_t *) rx_stat;
					dw_cnt = st_form[type_idx].blk_size
						>> 2;
					for (dw_idx = 0; dw_idx < dw_cnt;
							dw_idx++, ptr2++,
							ptr += 4) {
						/* endian transform */
						buf = htonl(*ptr2);
						/* fill in block content */
						kalMemMove(ptr, &buf,
								sizeof(buf));
					}

					buf_size += st_form[type_idx].blk_size;
				}
			}
		}
	}

	/* free allocated memory */
	kalMemFree(rx_stat, VIR_MEM_TYPE, sizeof(struct hqa_rx_stat_u));

	/* fill in type num */
	ptr = HqaCmdFrame->Data + 2;
	buf = htonl(type_num);
	kalMemMove(ptr, &buf, sizeof(buf));
	buf_size += sizeof(type_num);

	ResponseToQA(HqaCmdFrame, prIwReqData,
		     (2 + buf_size), i4Ret);

	return i4Ret;

error1:
	DBGLOG(RFTEST, INFO, "memory allocation fail for rx stat.");
	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	return i4Ret;

error2:
	DBGLOG(RFTEST, INFO, "invalid band index for non-dbdc mode\n");
	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSSetNss(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		*mps_setting = NULL;
	uint32_t		u4Band_idx = 0;
	uint32_t		u4Offset = 0;
	uint32_t		u4Len = 0;
	uint32_t		i = 0;
	uint32_t		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length) / sizeof(uint32_t) - 1;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MPSSetNss u4Len : %d\n",
	       u4Len);

	mps_setting = kmalloc(sizeof(uint32_t) * (u4Len),
			      GFP_KERNEL);
	ASSERT(mps_setting);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetNss u4Band_idx : %d\n", u4Band_idx);

	for (i = 0; i < u4Len; i++) {
		u4Offset = 4 + 4 * i;
		/* Reserved at least 4 byte availbale data */
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data))
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4 * i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_MPSSetNss mps_setting Case %d (Nss : %d)\n",
		       i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetNss(prNetDev, u4Len, mps_setting,
				u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_MPSSetPerpacketBW(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		*mps_setting = NULL;
	uint32_t		u4Band_idx = 0;
	uint32_t		u4Offset = 0;
	uint32_t		u4Len = 0;
	uint32_t		i = 0;
	uint32_t		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length) / sizeof(uint32_t) - 1;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPerpacketBW u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(uint32_t) * (u4Len),
			      GFP_KERNEL);
	ASSERT(mps_setting);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MPSSetPerpacketBW u4Band_idx : %d\n",
	       u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		/* Reserved at least 4 byte availbale data */
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data))
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4 * i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_MPSSetPerpacketBW mps_setting Case %d (BW : %d)\n",
		       i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPerpacketBW(prNetDev, u4Len,
					mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetAIFS(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t SlotTime = 0;
	uint32_t SifsTime = 0;

	memcpy(&SlotTime, HqaCmdFrame->Data + 4 * 0, 4);
	SlotTime = ntohl(SlotTime);
	memcpy(&SifsTime, HqaCmdFrame->Data + 4 * 1, 4);
	SifsTime = ntohl(SifsTime);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetAIFS SlotTime = %d\n",
	       SlotTime);
	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_SetAIFS SifsTime = %d\n",
	       SifsTime);

	i4Ret = MT_ATESetTxIPG(prNetDev, SifsTime);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CheckEfuseModeType(struct net_device
				      *prNetDev,
				      IN union iwreq_data *prIwReqData,
				      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t		i4Ret = 0;
	uint32_t		Value = u4EepromMode;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_CheckEfuseModeType\n");

	/*
	 * Value:
	 * 1 -> efuse Mode
	 * 2 -> flash Mode
	 * 3 -> eeprom Mode
	 * 4 -> bin Mode
	 */
	Value = ntohl(Value);
	memcpy(HqaCmdFrame->Data + 2, &(Value), sizeof(Value));

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CheckEfuseNativeModeType(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_CheckEfuseNativeModeType\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_SetBandMode(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Band_mode = 0;
	uint32_t u4Band_type = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	memcpy((uint8_t *)&u4Band_mode, HqaCmdFrame->Data + 4 * 0,
	       4);
	u4Band_mode = ntohl(u4Band_mode);
	memcpy((uint8_t *)&u4Band_type, HqaCmdFrame->Data + 4 * 1,
	       4);
	u4Band_type = ntohl(u4Band_type);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetBandMode u4Band_mode : %d\n", u4Band_mode);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_SetBandMode u4Band_type : %d\n", u4Band_type);

	if (u4Band_mode == 2)
		g_DBDCEnable = TRUE;
	else if (u4Band_mode == 1)
		g_DBDCEnable = FALSE;

	/* notifiy FW */
	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_DBDC_ENABLE;
	if (g_DBDCEnable)
		rRfATInfo.u4FuncData = 1;
	else
		rRfATInfo.u4FuncData = 0;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_GetBandMode g_DBDCEnable = %d\n",
	       g_DBDCEnable);

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_GetBandMode(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Band_mode = 0;
	uint32_t u4Band_idx = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy((uint8_t *)&u4Band_idx, HqaCmdFrame->Data, 4);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_GetBandMode u4Band_idx : %d\n", u4Band_idx);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_DBDC_ENABLE;
	if (g_DBDCEnable)
		rRfATInfo.u4FuncData = 1;
	else
		rRfATInfo.u4FuncData = 0;

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_GetBandMode g_DBDCEnable = %d\n",
	       g_DBDCEnable);

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	if (u4Band_idx == 0) {
		u4Band_mode = 3;
	} else {
		if (g_DBDCEnable)
			u4Band_mode = 3;
		else
			u4Band_mode = 0;
	}

	u4Band_mode = ntohl(u4Band_mode);

	memcpy(HqaCmdFrame->Data + 2, &(u4Band_mode),
	       sizeof(u4Band_mode));

	ResponseToQA(HqaCmdFrame, prIwReqData,
		     2 + sizeof(u4Band_mode), i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_RDDStartExt(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_RDDStartExt\n");

	DBGLOG(RFTEST, INFO, "[RDD DUMP START]\n");

	i4Ret = MT_ATERDDStart(prNetDev, "RDDSTART");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_RDDStopExt(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_RDDStopExt\n");

	i4Ret = MT_ATERDDStop(prNetDev, "RDDSTOP");

	DBGLOG(RFTEST, INFO, "[RDD DUMP END]\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_BssInfoUpdate(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t OwnMacIdx = 0, BssIdx = 0;
	uint8_t ucAddr1[MAC_ADDR_LEN];
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	memcpy(&OwnMacIdx, HqaCmdFrame->Data + 4 * 0, 4);
	OwnMacIdx = ntohl(OwnMacIdx);
	memcpy(&BssIdx, HqaCmdFrame->Data + 4 * 1, 4);
	BssIdx = ntohl(BssIdx);
	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 2, 6);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_BssInfoUpdate OwnMacIdx : %d\n", OwnMacIdx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_BssInfoUpdate BssIdx : %d\n", BssIdx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_BssInfoUpdate addr1:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4],
	       ucAddr1[5]);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   OwnMacIdx, BssIdx, ucAddr1[0], ucAddr1[1], ucAddr1[2],
		   ucAddr1[3], ucAddr1[4], ucAddr1[5]);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_BssInfoUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_DevInfoUpdate(struct net_device
				 *prNetDev,
				 IN union iwreq_data *prIwReqData,
				 struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t Band = 0, OwnMacIdx = 0;
	uint8_t ucAddr1[MAC_ADDR_LEN];
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	memcpy(&Band, HqaCmdFrame->Data + 4 * 0, 4);
	Band = ntohl(Band);
	memcpy(&OwnMacIdx, HqaCmdFrame->Data + 4 * 1, 4);
	OwnMacIdx = ntohl(OwnMacIdx);
	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 2, 6);

	DBGLOG(RFTEST, INFO, "Band : %d, OwnMacIdx : %d\n", Band, OwnMacIdx);
	DBGLOG(RFTEST, INFO, "addr1:%02x:%02x:%02x:%02x:%02x:%02x\n",
			ucAddr1[0], ucAddr1[1], ucAddr1[2],
			ucAddr1[3], ucAddr1[4], ucAddr1[5]);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   OwnMacIdx, ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3],
		   ucAddr1[4], ucAddr1[5], Band);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_DevInfoUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_LogOnOff(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Band_idx = 0;
	uint32_t u4Log_type = 0;
	uint32_t u4Log_ctrl = 0;
	uint32_t u4Log_size = 200;

	memcpy(&u4Band_idx, HqaCmdFrame->Data, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(&u4Log_type, HqaCmdFrame->Data + 4, 4);
	u4Log_type = ntohl(u4Log_type);
	memcpy(&u4Log_ctrl, HqaCmdFrame->Data + 4 + 4, 4);
	u4Log_ctrl = ntohl(u4Log_ctrl);
	memcpy(&u4Log_size, HqaCmdFrame->Data + 4 + 4 + 4, 4);
	u4Log_size = ntohl(u4Log_size);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_LogOnOff band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_LogOnOff log_type : %d\n", u4Log_type);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_LogOnOff log_ctrl : %d\n", u4Log_ctrl);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_LogOnOff log_size : %d\n", u4Log_size);

	i4Ret = MT_ATELogOnOff(prNetDev, u4Log_type, u4Log_ctrl,
			       u4Log_size);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}


static int32_t HQA_GetDumpRecal(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct RECAL_INFO_T *prReCalInfo = NULL;
	struct RECAL_DATA_T *prCalArray = NULL;
	uint32_t i = 0, u4Value = 0, u4RespLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);
	prReCalInfo = &prAdapter->rReCalInfo;
	prCalArray = prReCalInfo->prCalArray;

	DBGLOG(RFTEST, INFO, "prReCalInfo->u4Count = [%d]\n",
						 prReCalInfo->u4Count);
	if (prReCalInfo->u4Count > 0) {
		for (i = 0; i < prReCalInfo->u4Count; i++) {
			u4Value = ntohl(prCalArray[i].u4CalId);
			kalMemCopy(HqaCmdFrame->Data + 6 + u4RespLen,
					   &u4Value,
					   sizeof(u4Value));
			u4RespLen += sizeof(u4Value);

			u4Value = ntohl(prCalArray[i].u4CalAddr);
			DBGLOG(RFTEST, INFO, "CalAddr[%d] = [0x%08x]\n",
					     i, prCalArray[i].u4CalAddr);
			kalMemCopy(HqaCmdFrame->Data + 6 + u4RespLen,
					   &u4Value,
					   sizeof(u4Value));
			u4RespLen += sizeof(u4Value);

			u4Value = ntohl(prCalArray[i].u4CalValue);
			kalMemCopy(HqaCmdFrame->Data + 6 + u4RespLen,
					   &u4Value,
					   sizeof(u4Value));
			u4RespLen += sizeof(u4Value);
		}

		u4Value = ntohl(prReCalInfo->u4Count);
		kalMemCopy(HqaCmdFrame->Data + 2, &u4Value, sizeof(u4Value));
		ResponseToQA(HqaCmdFrame,
			     prIwReqData,
			     6 + prReCalInfo->u4Count * 12,
			     0);
	} else {
		kalMemCopy(HqaCmdFrame->Data + 2, &prReCalInfo->u4Count, 4);
		ResponseToQA(HqaCmdFrame, prIwReqData, 6, 0);
	}

	/* free resources */
	if (prReCalInfo->prCalArray != NULL) {
		kalMemFree(prReCalInfo->prCalArray,
			       VIR_MEM_TYPE,
			       2048 * sizeof(struct RECAL_DATA_T));
		prReCalInfo->prCalArray = 0;
		prReCalInfo->u4Count = 0;
	}

	return 0;
}

static int32_t HQA_GetDumpRXV(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Count = 0, i4Ret = 0;

	i4Ret = MT_ATEGetDumpRXV(prNetDev, HqaCmdFrame->Data, &i4Count);
	ResponseToQA(HqaCmdFrame, prIwReqData, 6 + i4Count, i4Ret);
	return i4Ret;
}

static HQA_CMD_HANDLER HQA_ReCal_CMDS[] = {
	HQA_GetDumpRecal,			/*0x1581 */
};

static HQA_CMD_HANDLER HQA_RXV_CMDS[] = {
	HQA_GetDumpRXV,		/* 0x1581 */
};


static HQA_CMD_HANDLER HQA_CMD_SET5[] = {
	/* cmd id start from 0x1500 */
	HQA_GetFWInfo,		/* 0x1500 */
	HQA_StartContinousTx,	/* 0x1501 */
	HQA_SetSTBC,		/* 0x1502 */
	HQA_SetShortGI,		/* 0x1503 */
	HQA_SetDPD,		/* 0x1504 */
	HQA_SetTssiOnOff,	/* 0x1505 */
	HQA_GetRxStatisticsAll,	/* 0x1506 */
	HQA_StartContiTxTone,	/* 0x1507 */
	HQA_StopContiTxTone,	/* 0x1508 */
	HQA_CalibrationTestMode,	/* 0x1509 */
	HQA_DoCalibrationTestItem,	/* 0x150A */
	HQA_eFusePhysicalWrite,	/* 0x150B */
	HQA_eFusePhysicalRead,	/* 0x150C */
	HQA_eFuseLogicalRead,	/* 0x150D */
	HQA_eFuseLogicalWrite,	/* 0x150E */
	HQA_TMRSetting,		/* 0x150F */
	HQA_GetRxSNR,		/* 0x1510 */
	HQA_WriteBufferDone,	/* 0x1511 */
	HQA_FFT,		/* 0x1512 */
	HQA_SetTxTonePower,	/* 0x1513 */
	HQA_GetChipID,		/* 0x1514 */
	HQA_MPSSetSeqData,	/* 0x1515 */
	HQA_MPSSetPayloadLength,	/* 0x1516 */
	HQA_MPSSetPacketCount,	/* 0x1517 */
	HQA_MPSSetPowerGain,	/* 0x1518 */
	HQA_MPSStart,		/* 0x1519 */
	HQA_MPSStop,		/* 0x151A */
	ToDoFunction,		/* 0x151B */
	HQA_GetRxStatisticsAllV2,	/* 0x151C */
	ToDoFunction,		/* 0x151D */
	ToDoFunction,		/* 0x151E */
	ToDoFunction,		/* 0x151F */
	ToDoFunction,		/* 0x1520 */
	HQA_SetAIFS,		/* 0x1521 */
	HQA_CheckEfuseModeType,	/* 0x1522 */
	HQA_CheckEfuseNativeModeType,	/* 0x1523 */
	ToDoFunction,		/* 0x1524 */
	ToDoFunction,		/* 0x1525 */
	ToDoFunction,		/* 0x1526 */
	ToDoFunction,		/* 0x1527 */
	ToDoFunction,		/* 0x1528 */
	ToDoFunction,		/* 0x1529 */
	ToDoFunction,		/* 0x152A */
	ToDoFunction,		/* 0x152B */
	HQA_SetBandMode,	/* 0x152C */
	HQA_GetBandMode,	/* 0x152D */
	HQA_RDDStartExt,	/* 0x152E */
	HQA_RDDStopExt,		/* 0x152F */
	ToDoFunction,		/* 0x1530 */
	HQA_BssInfoUpdate,	/* 0x1531 */
	HQA_DevInfoUpdate,	/* 0x1532 */
	HQA_LogOnOff,		/* 0x1533 */
	ToDoFunction,		/* 0x1534 */
	ToDoFunction,		/* 0x1535 */
	HQA_MPSSetNss,		/* 0x1536 */
	HQA_MPSSetPerpacketBW,	/* 0x1537 */
};

#if CFG_SUPPORT_TX_BF
static int32_t HQA_TxBfProfileTagInValid(struct net_device
		*prNetDev,
		IN union iwreq_data *prIwReqData,
		struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t invalid = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(invalid), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagInValid\n");

	memcpy(&invalid, HqaCmdFrame->Data, 4);
	invalid = ntohl(invalid);

	kalMemSet(prInBuf, 0, sizeof(invalid));
	kalSprintf(prInBuf, "%u", invalid);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_InValid(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagPfmuIdx(struct net_device
		*prNetDev,
		IN union iwreq_data *prIwReqData,
		struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t pfmuidx = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(pfmuidx), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagPfmuIdx\n");

	memcpy(&pfmuidx, HqaCmdFrame->Data, 4);
	pfmuidx = ntohl(pfmuidx);

	kalMemSet(prInBuf, 0, sizeof(pfmuidx));
	kalSprintf(prInBuf, "%u", pfmuidx);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_PfmuIdx(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagBfType(struct net_device
					*prNetDev,
					IN union iwreq_data *prIwReqData,
					struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t bftype = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(bftype), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagBfType\n");

	memcpy(&bftype, HqaCmdFrame->Data, 4);
	bftype = ntohl(bftype);

	kalMemSet(prInBuf, 0, sizeof(bftype));
	kalSprintf(prInBuf, "%u", bftype);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_BfType(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagBw(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t tag_bw = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(tag_bw), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagBw\n");

	memcpy(&tag_bw, HqaCmdFrame->Data, 4);
	tag_bw = ntohl(tag_bw);

	kalMemSet(prInBuf, 0, sizeof(tag_bw));
	kalSprintf(prInBuf, "%u", tag_bw);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DBW(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagSuMu(struct net_device
				      *prNetDev,
				      IN union iwreq_data *prIwReqData,
				      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t su_mu = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(su_mu), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagSuMu\n");

	memcpy(&su_mu, HqaCmdFrame->Data, 4);
	su_mu = ntohl(su_mu);

	kalMemSet(prInBuf, 0, sizeof(su_mu));
	kalSprintf(prInBuf, "%u", su_mu);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SuMu(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagMemAlloc(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t col_idx0, row_idx0, col_idx1, row_idx1;
	uint32_t col_idx2, row_idx2, col_idx3, row_idx3;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagMemAlloc\n");

	memcpy(&col_idx0, HqaCmdFrame->Data + 4 * 0, 4);
	col_idx0 = ntohl(col_idx0);
	memcpy(&row_idx0, HqaCmdFrame->Data + 4 * 1, 4);
	row_idx0 = ntohl(row_idx0);
	memcpy(&col_idx1, HqaCmdFrame->Data + 4 * 2, 4);
	col_idx1 = ntohl(col_idx1);
	memcpy(&row_idx1, HqaCmdFrame->Data + 4 * 3, 4);
	row_idx1 = ntohl(row_idx1);
	memcpy(&col_idx2, HqaCmdFrame->Data + 4 * 4, 4);
	col_idx2 = ntohl(col_idx2);
	memcpy(&row_idx2, HqaCmdFrame->Data + 4 * 5, 4);
	row_idx2 = ntohl(row_idx2);
	memcpy(&col_idx3, HqaCmdFrame->Data + 4 * 6, 4);
	col_idx3 = ntohl(col_idx3);
	memcpy(&row_idx3, HqaCmdFrame->Data + 4 * 7, 4);
	row_idx3 = ntohl(row_idx3);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   col_idx0, row_idx0, col_idx1, row_idx1, col_idx2, row_idx2,
		   col_idx3, row_idx3);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_Mem(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagMatrix(struct net_device
					*prNetDev,
					IN union iwreq_data *prIwReqData,
					struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t nrow, ncol, ngroup, LM, code_book, htc_exist;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagMatrix\n");

	memcpy(&nrow, HqaCmdFrame->Data + 4 * 0, 4);
	nrow = ntohl(nrow);
	memcpy(&ncol, HqaCmdFrame->Data + 4 * 1, 4);
	ncol = ntohl(ncol);
	memcpy(&ngroup, HqaCmdFrame->Data + 4 * 2, 4);
	ngroup = ntohl(ngroup);
	memcpy(&LM, HqaCmdFrame->Data + 4 * 3, 4);
	LM = ntohl(LM);
	memcpy(&code_book, HqaCmdFrame->Data + 4 * 4, 4);
	code_book = ntohl(code_book);
	memcpy(&htc_exist, HqaCmdFrame->Data + 4 * 5, 4);
	htc_exist = ntohl(htc_exist);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x", nrow,
		   ncol, ngroup, LM, code_book, htc_exist);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_Matrix(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagSnr(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t snr_sts0, snr_sts1, snr_sts2, snr_sts3;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagSnr\n");

	memcpy(&snr_sts0, HqaCmdFrame->Data + 4 * 0, 4);
	snr_sts0 = ntohl(snr_sts0);
	memcpy(&snr_sts1, HqaCmdFrame->Data + 4 * 1, 4);
	snr_sts1 = ntohl(snr_sts1);
	memcpy(&snr_sts2, HqaCmdFrame->Data + 4 * 2, 4);
	snr_sts2 = ntohl(snr_sts2);
	memcpy(&snr_sts3, HqaCmdFrame->Data + 4 * 3, 4);
	snr_sts3 = ntohl(snr_sts3);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x", snr_sts0,
		   snr_sts1, snr_sts2, snr_sts3);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SNR(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagSmtAnt(struct net_device
					*prNetDev,
					IN union iwreq_data *prIwReqData,
					struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t smt_ant = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(smt_ant), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagSmtAnt\n");

	memcpy(&smt_ant, HqaCmdFrame->Data + 4 * 0, 4);
	smt_ant = ntohl(smt_ant);

	kalMemSet(prInBuf, 0, sizeof(smt_ant));
	kalSprintf(prInBuf, "%u", smt_ant);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SmartAnt(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagSeIdx(struct net_device
				       *prNetDev,
				       IN union iwreq_data *prIwReqData,
				       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t se_idx = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(se_idx), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagSeIdx\n");

	memcpy(&se_idx, HqaCmdFrame->Data + 4 * 0, 4);
	se_idx = ntohl(se_idx);

	kalMemSet(prInBuf, 0, sizeof(se_idx));
	kalSprintf(prInBuf, "%u", se_idx);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SeIdx(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagRmsdThrd(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t rmsd_thrd = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(rmsd_thrd), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagRmsdThrd\n");

	memcpy(&rmsd_thrd, HqaCmdFrame->Data + 4 * 0, 4);
	rmsd_thrd = ntohl(rmsd_thrd);

	kalMemSet(prInBuf, 0, sizeof(rmsd_thrd));
	kalSprintf(prInBuf, "%u", rmsd_thrd);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_RmsdThrd(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagMcsThrd(struct net_device
		*prNetDev,
		IN union iwreq_data *prIwReqData,
		struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t mcs_lss0, mcs_sss0, mcs_lss1, mcs_sss1, mcs_lss2,
		 mcs_sss2;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagMcsThrd\n");

	memcpy(&mcs_lss0, HqaCmdFrame->Data + 4 * 0, 4);
	mcs_lss0 = ntohl(mcs_lss0);
	memcpy(&mcs_sss0, HqaCmdFrame->Data + 4 * 1, 4);
	mcs_sss0 = ntohl(mcs_sss0);
	memcpy(&mcs_lss1, HqaCmdFrame->Data + 4 * 2, 4);
	mcs_lss1 = ntohl(mcs_lss1);
	memcpy(&mcs_sss1, HqaCmdFrame->Data + 4 * 3, 4);
	mcs_sss1 = ntohl(mcs_sss1);
	memcpy(&mcs_lss2, HqaCmdFrame->Data + 4 * 4, 4);
	mcs_lss2 = ntohl(mcs_lss2);
	memcpy(&mcs_sss2, HqaCmdFrame->Data + 4 * 5, 4);
	mcs_sss2 = ntohl(mcs_sss2);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x",
		   mcs_lss0, mcs_sss0, mcs_lss1, mcs_sss1, mcs_lss2,
		   mcs_sss2);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_McsThrd(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagTimeOut(struct net_device
		*prNetDev,
		IN union iwreq_data *prIwReqData,
		struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t bf_tout = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(bf_tout), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagTimeOut\n");

	memcpy(&bf_tout, HqaCmdFrame->Data + 4 * 0, 4);
	bf_tout = ntohl(bf_tout);

	kalMemSet(prInBuf, 0, sizeof(bf_tout));
	kalSprintf(prInBuf, "%x", bf_tout);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_TimeOut(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagDesiredBw(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t desire_bw = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(desire_bw), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagDesiredBw\n");

	memcpy(&desire_bw, HqaCmdFrame->Data + 4 * 0, 4);
	desire_bw = ntohl(desire_bw);

	kalMemSet(prInBuf, 0, sizeof(desire_bw));
	kalSprintf(prInBuf, "%u", desire_bw);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DesiredBW(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagDesiredNc(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t desire_nc = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(desire_nc), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagDesiredNc\n");

	memcpy(&desire_nc, HqaCmdFrame->Data + 4 * 0, 4);
	desire_nc = ntohl(desire_nc);

	kalMemSet(prInBuf, 0, sizeof(desire_nc));
	kalSprintf(prInBuf, "%u", desire_nc);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DesiredNc(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagDesiredNr(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t desire_nr = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(desire_nr), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_TxBfProfileTagDesiredNr\n");

	memcpy(&desire_nr, HqaCmdFrame->Data + 4 * 0, 4);
	desire_nr = ntohl(desire_nr);

	kalMemSet(prInBuf, 0, sizeof(desire_nr));
	kalSprintf(prInBuf, "%u", desire_nr);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DesiredNr(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagWrite(struct net_device
				       *prNetDev,
				       IN union iwreq_data *prIwReqData,
				       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t idx = 0;	/* WLAN_IDX */
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(idx), GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagWrite\n");

	memcpy(&idx, HqaCmdFrame->Data + 4 * 0, 4);
	idx = ntohl(idx);

	kalMemSet(prInBuf, 0, sizeof(idx));
	kalSprintf(prInBuf, "%u", idx);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTagWrite(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TxBfProfileTagRead(struct net_device
				      *prNetDev,
				      IN union iwreq_data *prIwReqData,
				      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t idx = 0, isBFer = 0;
	uint8_t *prInBuf;
	union PFMU_PROFILE_TAG1 rPfmuTag1;
	union PFMU_PROFILE_TAG2 rPfmuTag2;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfProfileTagRead\n");

	memcpy(&idx, HqaCmdFrame->Data + 4 * 0, 4);
	idx = ntohl(idx);
	memcpy(&isBFer, HqaCmdFrame->Data + 4 * 1, 4);
	isBFer = ntohl(isBFer);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x", idx, isBFer);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTagRead(prNetDev, prInBuf);

	rPfmuTag1.au4RawData[0] = ntohl(g_rPfmuTag1.au4RawData[0]);
	rPfmuTag1.au4RawData[1] = ntohl(g_rPfmuTag1.au4RawData[1]);
	rPfmuTag1.au4RawData[2] = ntohl(g_rPfmuTag1.au4RawData[2]);
	rPfmuTag1.au4RawData[3] = ntohl(g_rPfmuTag1.au4RawData[3]);

	rPfmuTag2.au4RawData[0] = ntohl(g_rPfmuTag2.au4RawData[0]);
	rPfmuTag2.au4RawData[1] = ntohl(g_rPfmuTag2.au4RawData[1]);
	rPfmuTag2.au4RawData[2] = ntohl(g_rPfmuTag2.au4RawData[2]);

	memcpy(HqaCmdFrame->Data + 2, &rPfmuTag1,
	       sizeof(union PFMU_PROFILE_TAG1));
	memcpy(HqaCmdFrame->Data + 2 + sizeof(union
					      PFMU_PROFILE_TAG1), &rPfmuTag2,
	       sizeof(union PFMU_PROFILE_TAG2));

	ResponseToQA(HqaCmdFrame, prIwReqData,
		     2 + sizeof(union PFMU_PROFILE_TAG1) + sizeof(
			     union PFMU_PROFILE_TAG2), i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_StaRecCmmUpdate(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t wlan_idx, bss_idx, aid;
	uint8_t mac[MAC_ADDR_LEN];
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StaRecCmmUpdate\n");

	memcpy(&wlan_idx, HqaCmdFrame->Data + 4 * 0, 4);
	wlan_idx = ntohl(wlan_idx);
	memcpy(&bss_idx, HqaCmdFrame->Data + 4 * 1, 4);
	bss_idx = ntohl(bss_idx);
	memcpy(&aid, HqaCmdFrame->Data + 4 * 2, 4);
	aid = ntohl(aid);

	memcpy(mac, HqaCmdFrame->Data + 4 * 3, 6);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   wlan_idx, bss_idx, aid, mac[0], mac[1], mac[2], mac[3],
		   mac[4], mac[5]);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_StaRecCmmUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_StaRecBfUpdate(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t wlan_idx, bss_idx, PfmuId, su_mu, etxbf_cap,
		 ndpa_rate, ndp_rate;
	uint32_t report_poll_rate, tx_mode, nc, nr, cbw, spe_idx,
		 tot_mem_req;
	uint32_t mem_req_20m, mem_row0, mem_col0, mem_row1,
		 mem_col1;
	uint32_t mem_row2, mem_col2, mem_row3, mem_col3;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_StaRecBfUpdate\n");

	memcpy(&wlan_idx, HqaCmdFrame->Data + 4 * 0, 4);
	wlan_idx = ntohl(wlan_idx);
	memcpy(&bss_idx, HqaCmdFrame->Data + 4 * 1, 4);
	bss_idx = ntohl(bss_idx);
	memcpy(&PfmuId, HqaCmdFrame->Data + 4 * 2, 4);
	PfmuId = ntohl(PfmuId);
	memcpy(&su_mu, HqaCmdFrame->Data + 4 * 3, 4);
	su_mu = ntohl(su_mu);
	memcpy(&etxbf_cap, HqaCmdFrame->Data + 4 * 4, 4);
	etxbf_cap = ntohl(etxbf_cap);
	memcpy(&ndpa_rate, HqaCmdFrame->Data + 4 * 5, 4);
	ndpa_rate = ntohl(ndpa_rate);
	memcpy(&ndp_rate, HqaCmdFrame->Data + 4 * 6, 4);
	ndp_rate = ntohl(ndp_rate);
	memcpy(&report_poll_rate, HqaCmdFrame->Data + 4 * 7, 4);
	report_poll_rate = ntohl(report_poll_rate);
	memcpy(&tx_mode, HqaCmdFrame->Data + 4 * 8, 4);
	tx_mode = ntohl(tx_mode);
	memcpy(&nc, HqaCmdFrame->Data + 4 * 9, 4);
	nc = ntohl(nc);
	memcpy(&nr, HqaCmdFrame->Data + 4 * 10, 4);
	nr = ntohl(nr);
	memcpy(&cbw, HqaCmdFrame->Data + 4 * 11, 4);
	cbw = ntohl(cbw);
	memcpy(&spe_idx, HqaCmdFrame->Data + 4 * 12, 4);
	spe_idx = ntohl(spe_idx);
	memcpy(&tot_mem_req, HqaCmdFrame->Data + 4 * 13, 4);
	tot_mem_req = ntohl(tot_mem_req);
	memcpy(&mem_req_20m, HqaCmdFrame->Data + 4 * 14, 4);
	mem_req_20m = ntohl(mem_req_20m);
	memcpy(&mem_row0, HqaCmdFrame->Data + 4 * 15, 4);
	mem_row0 = ntohl(mem_row0);
	memcpy(&mem_col0, HqaCmdFrame->Data + 4 * 16, 4);
	mem_col0 = ntohl(mem_col0);
	memcpy(&mem_row1, HqaCmdFrame->Data + 4 * 17, 4);
	mem_row1 = ntohl(mem_row1);
	memcpy(&mem_col1, HqaCmdFrame->Data + 4 * 18, 4);
	mem_col1 = ntohl(mem_col1);
	memcpy(&mem_row2, HqaCmdFrame->Data + 4 * 19, 4);
	mem_row2 = ntohl(mem_row2);
	memcpy(&mem_col2, HqaCmdFrame->Data + 4 * 20, 4);
	mem_col2 = ntohl(mem_col2);
	memcpy(&mem_row3, HqaCmdFrame->Data + 4 * 21, 4);
	mem_row3 = ntohl(mem_row3);
	memcpy(&mem_col3, HqaCmdFrame->Data + 4 * 22, 4);
	mem_col3 = ntohl(mem_col3);

	/* For Tool wrong memory row and col num 20160501 */
	if (PfmuId == 0) {
		mem_row0 = 0;
		mem_col0 = 0;
		mem_row1 = 1;
		mem_col1 = 0;
		mem_row2 = 2;
		mem_col2 = 0;
		mem_row3 = 3;
		mem_col3 = 0;
	} else if (PfmuId == 1) {
		mem_row0 = 0;
		mem_col0 = 2;
		mem_row1 = 1;
		mem_col1 = 2;
		mem_row2 = 2;
		mem_col2 = 2;
		mem_row3 = 3;
		mem_col3 = 2;
	}

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02d:%02d:%02d:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   wlan_idx, bss_idx, PfmuId, su_mu, etxbf_cap, ndpa_rate,
		   ndp_rate, report_poll_rate, tx_mode, nc, nr,
		   cbw, spe_idx, tot_mem_req, mem_req_20m, mem_row0, mem_col0,
		   mem_row1, mem_col1, mem_row2, mem_col2,
		   mem_row3, mem_col3);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_StaRecBfUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_BFProfileDataRead(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t idx = 0, fgBFer = 0, subcarrIdx = 0,
		 subcarr_start = 0, subcarr_end = 0;
	uint32_t NumOfsub = 0;
	uint32_t offset = 0;
	uint8_t *SubIdx = NULL;
	uint8_t *prInBuf;
	union PFMU_DATA rPfmuData;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_BFProfileDataRead\n");

	memcpy(&idx, HqaCmdFrame->Data + 4 * 0, 4);
	idx = ntohl(idx);
	memcpy(&fgBFer, HqaCmdFrame->Data + 4 * 1, 4);
	fgBFer = ntohl(fgBFer);
	memcpy(&subcarr_start, HqaCmdFrame->Data + 4 * 2, 4);
	subcarr_start = ntohl(subcarr_start);
	memcpy(&subcarr_end, HqaCmdFrame->Data + 4 * 3, 4);
	subcarr_end = ntohl(subcarr_end);

	NumOfsub = subcarr_end - subcarr_start + 1;
	NumOfsub = ntohl(NumOfsub);

	memcpy(HqaCmdFrame->Data + 2, &NumOfsub, sizeof(NumOfsub));
	offset += sizeof(NumOfsub);

	for (subcarrIdx = subcarr_start; subcarrIdx <= subcarr_end;
	     subcarrIdx++) {
		SubIdx = (uint8_t *) &subcarrIdx;

		kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
		kalSprintf(prInBuf, "%02x:%02x:%02x:%02x", idx, fgBFer,
			   SubIdx[1], SubIdx[0]);

		DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

		i4Ret = Set_TxBfProfileDataRead(prNetDev, prInBuf);

		rPfmuData.au4RawData[0] = ntohl(g_rPfmuData.au4RawData[0]);
		rPfmuData.au4RawData[1] = ntohl(g_rPfmuData.au4RawData[1]);
		rPfmuData.au4RawData[2] = ntohl(g_rPfmuData.au4RawData[2]);
		rPfmuData.au4RawData[3] = ntohl(g_rPfmuData.au4RawData[3]);
		rPfmuData.au4RawData[4] = ntohl(g_rPfmuData.au4RawData[4]);

		memcpy(HqaCmdFrame->Data + 2 + offset, &rPfmuData,
		       sizeof(rPfmuData));
		offset += sizeof(rPfmuData);
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + offset, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_BFProfileDataWrite(struct net_device
				      *prNetDev,
				      IN union iwreq_data *prIwReqData,
				      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t pfmuid, subcarrier, phi11, psi21, phi21, psi31,
		 phi31, psi41;
	uint32_t phi22, psi32, phi32, psi42, phi33, psi43, snr00,
		 snr01, snr02, snr03;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_BFProfileDataWrite\n");

	memcpy(&pfmuid, HqaCmdFrame->Data + 4 * 0, 4);
	pfmuid = ntohl(pfmuid);
	memcpy(&subcarrier, HqaCmdFrame->Data + 4 * 1, 4);
	subcarrier = ntohl(subcarrier);
	memcpy(&phi11, HqaCmdFrame->Data + 4 * 2, 4);
	phi11 = ntohl(phi11);
	memcpy(&psi21, HqaCmdFrame->Data + 4 * 3, 4);
	psi21 = ntohl(psi21);
	memcpy(&phi21, HqaCmdFrame->Data + 4 * 4, 4);
	phi21 = ntohl(phi21);
	memcpy(&psi31, HqaCmdFrame->Data + 4 * 5, 4);
	psi31 = ntohl(psi31);
	memcpy(&phi31, HqaCmdFrame->Data + 4 * 6, 4);
	phi31 = ntohl(phi31);
	memcpy(&psi41, HqaCmdFrame->Data + 4 * 7, 4);
	psi41 = ntohl(psi41);
	memcpy(&phi22, HqaCmdFrame->Data + 4 * 8, 4);
	phi22 = ntohl(phi22);
	memcpy(&psi32, HqaCmdFrame->Data + 4 * 9, 4);
	psi32 = ntohl(psi32);
	memcpy(&phi32, HqaCmdFrame->Data + 4 * 10, 4);
	phi32 = ntohl(phi32);
	memcpy(&psi42, HqaCmdFrame->Data + 4 * 11, 4);
	psi42 = ntohl(psi42);
	memcpy(&phi33, HqaCmdFrame->Data + 4 * 12, 4);
	phi33 = ntohl(phi33);
	memcpy(&psi43, HqaCmdFrame->Data + 4 * 13, 4);
	psi43 = ntohl(psi43);
	memcpy(&snr00, HqaCmdFrame->Data + 4 * 14, 4);
	snr00 = ntohl(snr00);
	memcpy(&snr01, HqaCmdFrame->Data + 4 * 15, 4);
	snr01 = ntohl(snr01);
	memcpy(&snr02, HqaCmdFrame->Data + 4 * 16, 4);
	snr02 = ntohl(snr02);
	memcpy(&snr03, HqaCmdFrame->Data + 4 * 17, 4);
	snr03 = ntohl(snr03);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%03x:%03x:%02x:%03x:%02x:%03x:%02x:%03x:%02x:%03x:%02x:%03x:%02x:%02x:%02x:%02x:%02x",
		   pfmuid, subcarrier, phi11, psi21, phi21, psi31, phi31,
		   psi41,
		   phi22, psi32, phi32, psi42, phi33, psi43, snr00, snr01,
		   snr02, snr03);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileDataWrite(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_BFSounding(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t su_mu, mu_num, snd_interval, wlan_id0;
	uint32_t wlan_id1, wlan_id2, wlan_id3, band_idx;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_BFSounding\n");

	memcpy(&su_mu, HqaCmdFrame->Data + 4 * 0, 4);
	su_mu = ntohl(su_mu);
	memcpy(&mu_num, HqaCmdFrame->Data + 4 * 1, 4);
	mu_num = ntohl(mu_num);
	memcpy(&snd_interval, HqaCmdFrame->Data + 4 * 2, 4);
	snd_interval = ntohl(snd_interval);
	memcpy(&wlan_id0, HqaCmdFrame->Data + 4 * 3, 4);
	wlan_id0 = ntohl(wlan_id0);
	memcpy(&wlan_id1, HqaCmdFrame->Data + 4 * 4, 4);
	wlan_id1 = ntohl(wlan_id1);
	memcpy(&wlan_id2, HqaCmdFrame->Data + 4 * 5, 4);
	wlan_id2 = ntohl(wlan_id2);
	memcpy(&wlan_id3, HqaCmdFrame->Data + 4 * 6, 4);
	wlan_id3 = ntohl(wlan_id3);
	memcpy(&band_idx, HqaCmdFrame->Data + 4 * 7, 4);
	band_idx = ntohl(band_idx);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   su_mu, mu_num, snd_interval, wlan_id0, wlan_id1, wlan_id2,
		   wlan_id3);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_Trigger_Sounding_Proc(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_TXBFSoundingStop(struct net_device
				    *prNetDev,
				    IN union iwreq_data *prIwReqData,
				    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TXBFSoundingStop\n");

	i4Ret = Set_Stop_Sounding_Proc(prNetDev, NULL);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_TXBFProfileDataWriteAllExt(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_TxBfTxApply(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t eBF_enable = 0;
	uint32_t iBF_enable = 0;
	uint32_t wlan_id = 0;
	uint32_t MuTx_enable = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_TxBfTxApply\n");

	memcpy(&eBF_enable, HqaCmdFrame->Data + 4 * 0, 4);
	eBF_enable = ntohl(eBF_enable);
	memcpy(&iBF_enable, HqaCmdFrame->Data + 4 * 1, 4);
	iBF_enable = ntohl(iBF_enable);
	memcpy(&wlan_id, HqaCmdFrame->Data + 4 * 2, 4);
	wlan_id = ntohl(wlan_id);
	memcpy(&MuTx_enable, HqaCmdFrame->Data + 4 * 3, 4);
	MuTx_enable = ntohl(MuTx_enable);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x", wlan_id,
		   eBF_enable, iBF_enable, MuTx_enable);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfTxApply(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_ManualAssoc(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t type;
	uint32_t wtbl_idx;
	uint32_t ownmac_idx;
	uint32_t phymode;
	uint32_t bw;
	uint32_t pfmuid;
	uint32_t marate_mode;
	uint32_t marate_mcs;
	uint32_t spe_idx;
	uint32_t aid;
	uint8_t ucAddr1[MAC_ADDR_LEN];
	uint32_t nss = 1;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_ManualAssoc\n");

	memcpy(&type, HqaCmdFrame->Data + 4 * 0, 4);
	type = ntohl(type);
	memcpy(&wtbl_idx, HqaCmdFrame->Data + 4 * 1, 4);
	wtbl_idx = ntohl(wtbl_idx);
	memcpy(&ownmac_idx, HqaCmdFrame->Data + 4 * 2, 4);
	ownmac_idx = ntohl(ownmac_idx);
	memcpy(&phymode, HqaCmdFrame->Data + 4 * 3, 4);
	phymode = ntohl(phymode);
	memcpy(&bw, HqaCmdFrame->Data + 4 * 4, 4);
	bw = ntohl(bw);
	memcpy(&pfmuid, HqaCmdFrame->Data + 4 * 5, 4);
	pfmuid = ntohl(pfmuid);
	memcpy(&marate_mode, HqaCmdFrame->Data + 4 * 6, 4);
	marate_mode = ntohl(marate_mode);
	memcpy(&marate_mcs, HqaCmdFrame->Data + 4 * 7, 4);
	marate_mcs = ntohl(marate_mcs);
	memcpy(&spe_idx, HqaCmdFrame->Data + 4 * 8, 4);
	spe_idx = ntohl(spe_idx);
	memcpy(&aid, HqaCmdFrame->Data + 4 * 9, 4);
	aid = ntohl(aid);
	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 10, 6);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4],
		   ucAddr1[5], type, wtbl_idx, ownmac_idx,
		   phymode, bw, nss, pfmuid, marate_mode, marate_mcs, spe_idx,
		   aid, 0);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfManualAssoc(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_TXBF_CMDS[] = {
	HQA_TxBfProfileTagInValid,	/* 0x1540 */
	HQA_TxBfProfileTagPfmuIdx,	/* 0x1541 */
	HQA_TxBfProfileTagBfType,	/* 0x1542 */
	HQA_TxBfProfileTagBw,	/* 0x1543 */
	HQA_TxBfProfileTagSuMu,	/* 0x1544 */
	HQA_TxBfProfileTagMemAlloc,	/* 0x1545 */
	HQA_TxBfProfileTagMatrix,	/* 0x1546 */
	HQA_TxBfProfileTagSnr,	/* 0x1547 */
	HQA_TxBfProfileTagSmtAnt,	/* 0x1548 */
	HQA_TxBfProfileTagSeIdx,	/* 0x1549 */
	HQA_TxBfProfileTagRmsdThrd,	/* 0x154A */
	HQA_TxBfProfileTagMcsThrd,	/* 0x154B */
	HQA_TxBfProfileTagTimeOut,	/* 0x154C */
	HQA_TxBfProfileTagDesiredBw,	/* 0x154D */
	HQA_TxBfProfileTagDesiredNc,	/* 0x154E */
	HQA_TxBfProfileTagDesiredNr,	/* 0x154F */
	HQA_TxBfProfileTagWrite,	/* 0x1550 */
	HQA_TxBfProfileTagRead,	/* 0x1551 */
	HQA_StaRecCmmUpdate,	/* 0x1552 */
	HQA_StaRecBfUpdate,	/* 0x1553 */
	HQA_BFProfileDataRead,	/* 0x1554 */
	HQA_BFProfileDataWrite,	/* 0x1555 */
	HQA_BFSounding,		/* 0x1556 */
	HQA_TXBFSoundingStop,	/* 0x1557 */
	HQA_TXBFProfileDataWriteAllExt,	/* 0x1558 */
	HQA_TxBfTxApply,	/* 0x1559 */
	HQA_ManualAssoc,	/* 0x155A */
};

#if CFG_SUPPORT_MU_MIMO
static int32_t HQA_MUGetInitMCS(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Gid = 0;
	uint32_t u4User0InitMCS = 0;
	uint32_t u4User1InitMCS = 0;
	uint32_t u4User2InitMCS = 0;
	uint32_t u4User3InitMCS = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUGetInitMCS\n");

	memcpy(&u4Gid, HqaCmdFrame->Data, 4);
	u4Gid = ntohl(u4Gid);

	kalMemSet(prInBuf, 0, sizeof(u4Gid));
	kalSprintf(prInBuf, "%u", u4Gid);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUGetInitMCS(prNetDev, prInBuf);

	u4User0InitMCS = ntohl(u4User0InitMCS);
	u4User1InitMCS = ntohl(u4User1InitMCS);
	u4User2InitMCS = ntohl(u4User2InitMCS);
	u4User3InitMCS = ntohl(u4User3InitMCS);

	memcpy(HqaCmdFrame->Data + 2, &u4User0InitMCS,
	       sizeof(uint32_t));
	memcpy(HqaCmdFrame->Data + 2 + 1 * sizeof(uint32_t),
	       &u4User1InitMCS, sizeof(uint32_t));
	memcpy(HqaCmdFrame->Data + 2 + 2 * sizeof(uint32_t),
	       &u4User2InitMCS, sizeof(uint32_t));
	memcpy(HqaCmdFrame->Data + 2 + 3 * sizeof(uint32_t),
	       &u4User3InitMCS, sizeof(uint32_t));

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUCalInitMCS(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Num_of_user;
	uint32_t u4Bandwidth;
	uint32_t u4Nss_of_user0;
	uint32_t u4Nss_of_user1;
	uint32_t u4Nss_of_user2;
	uint32_t u4Nss_of_user3;
	uint32_t u4Pf_mu_id_of_user0;
	uint32_t u4Pf_mu_id_of_user1;
	uint32_t u4Pf_mu_id_of_user2;
	uint32_t u4Pf_mu_id_of_user3;
	uint32_t u4Num_of_txer;	/* number of antenna */
	uint32_t u4Spe_index;
	uint32_t u4Group_index;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUCalInitMCS\n");

	memcpy(&u4Num_of_user, HqaCmdFrame->Data + 4 * 0, 4);
	u4Num_of_user = ntohl(u4Num_of_user);
	memcpy(&u4Bandwidth, HqaCmdFrame->Data + 4 * 1, 4);
	u4Bandwidth = ntohl(u4Bandwidth);
	memcpy(&u4Nss_of_user0, HqaCmdFrame->Data + 4 * 2, 4);
	u4Nss_of_user0 = ntohl(u4Nss_of_user0);
	memcpy(&u4Nss_of_user1, HqaCmdFrame->Data + 4 * 3, 4);
	u4Nss_of_user1 = ntohl(u4Nss_of_user1);
	memcpy(&u4Nss_of_user2, HqaCmdFrame->Data + 4 * 4, 4);
	u4Nss_of_user2 = ntohl(u4Nss_of_user2);
	memcpy(&u4Nss_of_user3, HqaCmdFrame->Data + 4 * 5, 4);
	u4Nss_of_user3 = ntohl(u4Nss_of_user3);
	memcpy(&u4Pf_mu_id_of_user0, HqaCmdFrame->Data + 4 * 6, 4);
	u4Pf_mu_id_of_user0 = ntohl(u4Pf_mu_id_of_user0);
	memcpy(&u4Pf_mu_id_of_user1, HqaCmdFrame->Data + 4 * 7, 4);
	u4Pf_mu_id_of_user1 = ntohl(u4Pf_mu_id_of_user1);
	memcpy(&u4Pf_mu_id_of_user2, HqaCmdFrame->Data + 4 * 8, 4);
	u4Pf_mu_id_of_user2 = ntohl(u4Pf_mu_id_of_user2);
	memcpy(&u4Pf_mu_id_of_user3, HqaCmdFrame->Data + 4 * 9, 4);
	u4Pf_mu_id_of_user3 = ntohl(u4Pf_mu_id_of_user3);
	memcpy(&u4Num_of_txer, HqaCmdFrame->Data + 4 * 10, 4);
	u4Num_of_txer = ntohl(u4Num_of_txer);
	memcpy(&u4Spe_index, HqaCmdFrame->Data + 4 * 11, 4);
	u4Spe_index = ntohl(u4Spe_index);
	memcpy(&u4Group_index, HqaCmdFrame->Data + 4 * 12, 4);
	u4Group_index = ntohl(u4Group_index);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4Num_of_user, u4Bandwidth, u4Nss_of_user0, u4Nss_of_user1,
		   u4Pf_mu_id_of_user0, u4Pf_mu_id_of_user1,
		   u4Num_of_txer, u4Spe_index, u4Group_index);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUCalInitMCS(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUCalLQ(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Type = 0;
	uint32_t u4Num_of_user;
	uint32_t u4Bandwidth;
	uint32_t u4Nss_of_user0;
	uint32_t u4Nss_of_user1;
	uint32_t u4Nss_of_user2;
	uint32_t u4Nss_of_user3;
	uint32_t u4Pf_mu_id_of_user0;
	uint32_t u4Pf_mu_id_of_user1;
	uint32_t u4Pf_mu_id_of_user2;
	uint32_t u4Pf_mu_id_of_user3;
	uint32_t u4Num_of_txer;	/* number of antenna */
	uint32_t u4Spe_index;
	uint32_t u4Group_index;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUCalLQ\n");

	memcpy(&u4Type, HqaCmdFrame->Data + 4 * 0, 4);
	u4Type = ntohl(u4Type);
	memcpy(&u4Num_of_user, HqaCmdFrame->Data + 4 * 1, 4);
	u4Num_of_user = ntohl(u4Num_of_user);
	memcpy(&u4Bandwidth, HqaCmdFrame->Data + 4 * 2, 4);
	u4Bandwidth = ntohl(u4Bandwidth);
	memcpy(&u4Nss_of_user0, HqaCmdFrame->Data + 4 * 3, 4);
	u4Nss_of_user0 = ntohl(u4Nss_of_user0);
	memcpy(&u4Nss_of_user1, HqaCmdFrame->Data + 4 * 4, 4);
	u4Nss_of_user1 = ntohl(u4Nss_of_user1);
	memcpy(&u4Nss_of_user2, HqaCmdFrame->Data + 4 * 5, 4);
	u4Nss_of_user2 = ntohl(u4Nss_of_user2);
	memcpy(&u4Nss_of_user3, HqaCmdFrame->Data + 4 * 6, 4);
	u4Nss_of_user3 = ntohl(u4Nss_of_user3);
	memcpy(&u4Pf_mu_id_of_user0, HqaCmdFrame->Data + 4 * 7, 4);
	u4Pf_mu_id_of_user0 = ntohl(u4Pf_mu_id_of_user0);
	memcpy(&u4Pf_mu_id_of_user1, HqaCmdFrame->Data + 4 * 8, 4);
	u4Pf_mu_id_of_user1 = ntohl(u4Pf_mu_id_of_user1);
	memcpy(&u4Pf_mu_id_of_user2, HqaCmdFrame->Data + 4 * 9, 4);
	u4Pf_mu_id_of_user2 = ntohl(u4Pf_mu_id_of_user2);
	memcpy(&u4Pf_mu_id_of_user3, HqaCmdFrame->Data + 4 * 10, 4);
	u4Pf_mu_id_of_user3 = ntohl(u4Pf_mu_id_of_user3);
	memcpy(&u4Num_of_txer, HqaCmdFrame->Data + 4 * 11, 4);
	u4Num_of_txer = ntohl(u4Num_of_txer);
	memcpy(&u4Spe_index, HqaCmdFrame->Data + 4 * 12, 4);
	u4Spe_index = ntohl(u4Spe_index);
	memcpy(&u4Group_index, HqaCmdFrame->Data + 4 * 13, 4);
	u4Group_index = ntohl(u4Group_index);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4Num_of_user, u4Bandwidth, u4Nss_of_user0, u4Nss_of_user1,
		   u4Pf_mu_id_of_user0, u4Pf_mu_id_of_user1,
		   u4Num_of_txer, u4Spe_index, u4Group_index);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUCalLQ(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUGetLQ(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t i;
	uint8_t u4LqReport[NUM_OF_USER * NUM_OF_MODUL] = {0};
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUGetLQ\n");

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUGetLQ(prNetDev, prInBuf);

	for (i = 0; i < NUM_OF_USER * NUM_OF_MODUL; i++) {
		u4LqReport[i] = ntohl(u4LqReport[i]);
		memcpy(HqaCmdFrame->Data + 2 + i * sizeof(uint32_t),
		       &u4LqReport[i], sizeof(uint32_t));
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUSetSNROffset(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Offset = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUSetSNROffset\n");

	memcpy(&u4Offset, HqaCmdFrame->Data + 4 * 0, 4);
	u4Offset = ntohl(u4Offset);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4Offset);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetSNROffset(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUSetZeroNss(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Zero_nss = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUSetZeroNss\n");

	memcpy(&u4Zero_nss, HqaCmdFrame->Data + 4 * 0, 4);
	u4Zero_nss = ntohl(u4Zero_nss);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4Zero_nss);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetZeroNss(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUSetSpeedUpLQ(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4SpeedUpLq = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUSetSpeedUpLQ\n");

	memcpy(&u4SpeedUpLq, HqaCmdFrame->Data + 4 * 0, 4);
	u4SpeedUpLq = ntohl(u4SpeedUpLq);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4SpeedUpLq);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetSpeedUpLQ(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;

}

static int32_t HQA_MUSetMUTable(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint8_t *prTable;
	uint16_t u2Len = 0;
	uint32_t u4SuMu = 0;

	prTable = kmalloc_array(u2Len, sizeof(uint8_t), GFP_KERNEL);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUSetMUTable\n");

	u2Len = ntohl(HqaCmdFrame->Length) - sizeof(u4SuMu);

	memcpy(&u4SuMu, HqaCmdFrame->Data + 4 * 0, 4);
	u4SuMu = ntohl(u4SuMu);

	memcpy(prTable, HqaCmdFrame->Data + 4, u2Len);

	i4Ret = Set_MUSetMUTable(prNetDev, prTable);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_MUSetGroup(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData,
			      struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4GroupIndex, u4NumOfUser, u4User0Ldpc,
		 u4User1Ldpc, u4User2Ldpc, u4User3Ldpc;
	uint32_t u4ShortGI, u4Bw, u4User0Nss, u4User1Nss,
		 u4User2Nss, u4User3Nss;
	uint32_t u4GroupId, u4User0UP, u4User1UP, u4User2UP,
		 u4User3UP;
	uint32_t u4User0MuPfId, u4User1MuPfId, u4User2MuPfId,
		 u4User3MuPfId;
	uint32_t u4User0InitMCS, u4User1InitMCS, u4User2InitMCS,
		 u4User3InitMCS;
	uint8_t ucAddr1[MAC_ADDR_LEN], ucAddr2[MAC_ADDR_LEN],
		ucAddr3[MAC_ADDR_LEN], ucAddr4[MAC_ADDR_LEN];
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUSetGroup\n");

	memcpy(&u4GroupIndex, HqaCmdFrame->Data + 4 * 0, 4);
	u4GroupIndex = ntohl(u4GroupIndex);
	memcpy(&u4NumOfUser, HqaCmdFrame->Data + 4 * 1, 4);
	u4NumOfUser = ntohl(u4NumOfUser);
	memcpy(&u4User0Ldpc, HqaCmdFrame->Data + 4 * 2, 4);
	u4User0Ldpc = ntohl(u4User0Ldpc);
	memcpy(&u4User1Ldpc, HqaCmdFrame->Data + 4 * 3, 4);
	u4User1Ldpc = ntohl(u4User1Ldpc);
	memcpy(&u4User2Ldpc, HqaCmdFrame->Data + 4 * 4, 4);
	u4User2Ldpc = ntohl(u4User2Ldpc);
	memcpy(&u4User3Ldpc, HqaCmdFrame->Data + 4 * 5, 4);
	u4User3Ldpc = ntohl(u4User3Ldpc);
	memcpy(&u4ShortGI, HqaCmdFrame->Data + 4 * 6, 4);
	u4ShortGI = ntohl(u4ShortGI);
	memcpy(&u4Bw, HqaCmdFrame->Data + 4 * 7, 4);
	u4Bw = ntohl(u4Bw);
	memcpy(&u4User0Nss, HqaCmdFrame->Data + 4 * 8, 4);
	u4User0Nss = ntohl(u4User0Nss);
	memcpy(&u4User1Nss, HqaCmdFrame->Data + 4 * 9, 4);
	u4User1Nss = ntohl(u4User1Nss);
	memcpy(&u4User2Nss, HqaCmdFrame->Data + 4 * 10, 4);
	u4User2Nss = ntohl(u4User2Nss);
	memcpy(&u4User3Nss, HqaCmdFrame->Data + 4 * 11, 4);
	u4User3Nss = ntohl(u4User3Nss);
	memcpy(&u4GroupId, HqaCmdFrame->Data + 4 * 12, 4);
	u4GroupId = ntohl(u4GroupId);
	memcpy(&u4User0UP, HqaCmdFrame->Data + 4 * 13, 4);
	u4User0UP = ntohl(u4User0UP);
	memcpy(&u4User1UP, HqaCmdFrame->Data + 4 * 14, 4);
	u4User1UP = ntohl(u4User1UP);
	memcpy(&u4User2UP, HqaCmdFrame->Data + 4 * 15, 4);
	u4User2UP = ntohl(u4User2UP);
	memcpy(&u4User3UP, HqaCmdFrame->Data + 4 * 16, 4);
	u4User3UP = ntohl(u4User3UP);
	memcpy(&u4User0MuPfId, HqaCmdFrame->Data + 4 * 17, 4);
	u4User0MuPfId = ntohl(u4User0MuPfId);
	memcpy(&u4User1MuPfId, HqaCmdFrame->Data + 4 * 18, 4);
	u4User1MuPfId = ntohl(u4User1MuPfId);
	memcpy(&u4User2MuPfId, HqaCmdFrame->Data + 4 * 19, 4);
	u4User2MuPfId = ntohl(u4User2MuPfId);
	memcpy(&u4User3MuPfId, HqaCmdFrame->Data + 4 * 20, 4);
	u4User3MuPfId = ntohl(u4User3MuPfId);
	memcpy(&u4User0InitMCS, HqaCmdFrame->Data + 4 * 21, 4);
	u4User0InitMCS = ntohl(u4User0InitMCS);
	memcpy(&u4User1InitMCS, HqaCmdFrame->Data + 4 * 22, 4);
	u4User1InitMCS = ntohl(u4User1InitMCS);
	memcpy(&u4User2InitMCS, HqaCmdFrame->Data + 4 * 23, 4);
	u4User2InitMCS = ntohl(u4User2InitMCS);
	memcpy(&u4User3InitMCS, HqaCmdFrame->Data + 4 * 24, 4);
	u4User3InitMCS = ntohl(u4User3InitMCS);

	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 25, 6);
	memcpy(ucAddr2, HqaCmdFrame->Data + 4 * 25 + 6 * 1, 6);
	memcpy(ucAddr3, HqaCmdFrame->Data + 4 * 25 + 6 * 2, 6);
	memcpy(ucAddr4, HqaCmdFrame->Data + 4 * 25 + 6 * 3, 6);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4GroupIndex, u4NumOfUser, u4User0Ldpc, u4User1Ldpc,
		   u4ShortGI, u4Bw, u4User0Nss, u4User1Nss,
		   u4GroupId, u4User0UP, u4User1UP, u4User0MuPfId,
		   u4User1MuPfId, u4User0InitMCS, u4User1InitMCS,
		   ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4],
		   ucAddr1[5], ucAddr2[0], ucAddr2[1],
		   ucAddr2[2], ucAddr2[3], ucAddr2[4], ucAddr2[5]);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetGroup(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUGetQD(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4SubIdx = 0;

	/* TODO */
	uint32_t u4User0InitMCS = 0;
	uint32_t u4User1InitMCS = 0;
	uint32_t u4User2InitMCS = 0;
	uint32_t u4User3InitMCS = 0;

	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUGetQD\n");

	memcpy(&u4SubIdx, HqaCmdFrame->Data, 4);
	u4SubIdx = ntohl(u4SubIdx);

	kalMemSet(prInBuf, 0, sizeof(u4SubIdx));
	kalSprintf(prInBuf, "%u", u4SubIdx);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUGetQD(prNetDev, prInBuf);

	/* TODO */
	u4User0InitMCS = ntohl(u4User0InitMCS);
	u4User1InitMCS = ntohl(u4User1InitMCS);
	u4User2InitMCS = ntohl(u4User2InitMCS);
	u4User3InitMCS = ntohl(u4User3InitMCS);

	memcpy(HqaCmdFrame->Data + 2, &u4User0InitMCS,
	       sizeof(uint32_t));
	memcpy(HqaCmdFrame->Data + 2 + 1 * sizeof(uint32_t),
	       &u4User1InitMCS, sizeof(uint32_t));
	memcpy(HqaCmdFrame->Data + 2 + 2 * sizeof(uint32_t),
	       &u4User2InitMCS, sizeof(uint32_t));
	memcpy(HqaCmdFrame->Data + 2 + 3 * sizeof(uint32_t),
	       &u4User3InitMCS, sizeof(uint32_t));

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUSetEnable(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Enable = 0;
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUSetEnable\n");

	memcpy(&u4Enable, HqaCmdFrame->Data + 4 * 0, 4);
	u4Enable = ntohl(u4Enable);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4Enable);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetEnable(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUSetGID_UP(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t au4Gid[2];
	uint32_t au4Up[4];
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUSetGID_UP\n");

	memcpy(&au4Gid[0], HqaCmdFrame->Data + 4 * 0, 4);
	au4Gid[0] = ntohl(au4Gid[0]);
	memcpy(&au4Gid[1], HqaCmdFrame->Data + 4 * 1, 4);
	au4Gid[1] = ntohl(au4Gid[1]);
	memcpy(&au4Up[0], HqaCmdFrame->Data + 4 * 2, 4);
	au4Up[0] = ntohl(au4Up[0]);
	memcpy(&au4Up[1], HqaCmdFrame->Data + 4 * 3, 4);
	au4Up[1] = ntohl(au4Up[1]);
	memcpy(&au4Up[2], HqaCmdFrame->Data + 4 * 4, 4);
	au4Up[2] = ntohl(au4Up[2]);
	memcpy(&au4Up[3], HqaCmdFrame->Data + 4 * 5, 4);
	au4Up[3] = ntohl(au4Up[3]);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x",
		   au4Gid[0], au4Gid[1], au4Up[0], au4Up[1], au4Up[2],
		   au4Up[3]);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetGID_UP(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static int32_t HQA_MUTriggerTx(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4BandIdx, u4IsRandomPattern;
	uint32_t u4MsduPayloadLength0, u4MsduPayloadLength1,
		 u4MsduPayloadLength2, u4MsduPayloadLength3;
	uint32_t u4MuPacketCount, u4NumOfSTAs;
	uint8_t ucAddr1[MAC_ADDR_LEN], ucAddr2[MAC_ADDR_LEN],
		ucAddr3[MAC_ADDR_LEN], ucAddr4[MAC_ADDR_LEN];
	uint8_t *prInBuf;

	prInBuf = kmalloc(sizeof(uint8_t) * (HQA_BF_STR_SIZE),
			  GFP_KERNEL);
	ASSERT(prInBuf);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_MUTriggerTx\n");

	memcpy(&u4BandIdx, HqaCmdFrame->Data + 4 * 0, 4);
	u4BandIdx = ntohl(u4BandIdx);
	memcpy(&u4IsRandomPattern, HqaCmdFrame->Data + 4 * 1, 4);
	u4IsRandomPattern = ntohl(u4IsRandomPattern);
	memcpy(&u4MsduPayloadLength0, HqaCmdFrame->Data + 4 * 2, 4);
	u4MsduPayloadLength0 = ntohl(u4MsduPayloadLength0);
	memcpy(&u4MsduPayloadLength1, HqaCmdFrame->Data + 4 * 3, 4);
	u4MsduPayloadLength1 = ntohl(u4MsduPayloadLength1);
	memcpy(&u4MsduPayloadLength2, HqaCmdFrame->Data + 4 * 4, 4);
	u4MsduPayloadLength2 = ntohl(u4MsduPayloadLength2);
	memcpy(&u4MsduPayloadLength3, HqaCmdFrame->Data + 4 * 5, 4);
	u4MsduPayloadLength3 = ntohl(u4MsduPayloadLength3);
	memcpy(&u4MuPacketCount, HqaCmdFrame->Data + 4 * 6, 4);
	u4MuPacketCount = ntohl(u4MuPacketCount);
	memcpy(&u4NumOfSTAs, HqaCmdFrame->Data + 4 * 7, 4);
	u4NumOfSTAs = ntohl(u4NumOfSTAs);
	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 8, 6);
	memcpy(ucAddr2, HqaCmdFrame->Data + 4 * 8 + 6 * 1, 6);
	memcpy(ucAddr3, HqaCmdFrame->Data + 4 * 8 + 6 * 2, 6);
	memcpy(ucAddr4, HqaCmdFrame->Data + 4 * 8 + 6 * 3, 6);

	kalMemSet(prInBuf, 0, sizeof(uint8_t) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4IsRandomPattern, u4MsduPayloadLength0,
		   u4MsduPayloadLength1, u4MuPacketCount, u4NumOfSTAs,
		   ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4],
		   ucAddr1[5],
		   ucAddr2[0], ucAddr2[1], ucAddr2[2], ucAddr2[3], ucAddr2[4],
		   ucAddr2[5]);

	DBGLOG(RFTEST, ERROR, "prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUTriggerTx(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	kfree(prInBuf);

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_TXMU_CMDS[] = {
	HQA_MUGetInitMCS,	/* 0x1560 */
	HQA_MUCalInitMCS,	/* 0x1561 */
	HQA_MUCalLQ,		/* 0x1562 */
	HQA_MUGetLQ,		/* 0x1563 */
	HQA_MUSetSNROffset,	/* 0x1564 */
	HQA_MUSetZeroNss,	/* 0x1565 */
	HQA_MUSetSpeedUpLQ,	/* 0x1566 */
	HQA_MUSetMUTable,	/* 0x1567 */
	HQA_MUSetGroup,		/* 0x1568 */
	HQA_MUGetQD,		/* 0x1569 */
	HQA_MUSetEnable,	/* 0x156A */
	HQA_MUSetGID_UP,	/* 0x156B */
	HQA_MUTriggerTx,	/* 0x156C */
};
#endif
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For ICAP
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t HQA_CapWiFiSpectrum(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	struct ATE_OPS_T *prAteOps = NULL;
	uint32_t u4Control = 0;
	uint32_t u4Trigger = 0;
	uint32_t u4RingCapEn = 0;
	uint32_t u4Event = 0;
	uint32_t u4Node = 0;
	uint32_t u4Len = 0;
	uint32_t u4StopCycle = 0;
	uint32_t u4BW = 0;
	uint32_t u4MacTriggerEvent = 0;
	uint32_t u4SourceAddrLSB = 0;
	uint32_t u4SourceAddrMSB = 0;
	uint8_t aucSourceAddress[MAC_ADDR_LEN];
	uint32_t u4Band = 0;
	uint32_t u4WFNum;
	uint32_t u4IQ;
	uint32_t u4DataLen = 0, u4TempLen = 0;
	int32_t i4Ret = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prChipInfo = prGlueInfo->prAdapter->chip_info;
	prAteOps = prChipInfo->prAteOps;

	memcpy((uint8_t *)&u4Control, HqaCmdFrame->Data + 4 * 0, 4);
	u4Control = ntohl(u4Control);

	DBGLOG(RFTEST, INFO, "u4Control = %d\n", u4Control);

	if (u4Control == 1) {
		if (prAteOps->setICapStart) {
			memcpy((uint8_t *)&u4Trigger,
			       HqaCmdFrame->Data + 4 * 1, 4);
			u4Trigger = ntohl(u4Trigger);
			memcpy((uint8_t *)&u4RingCapEn,
			       HqaCmdFrame->Data + 4 * 2, 4);
			u4RingCapEn = ntohl(u4RingCapEn);
			memcpy((uint8_t *)&u4Event,
			       HqaCmdFrame->Data + 4 * 3, 4);
			u4Event = ntohl(u4Event);
			memcpy((uint8_t *)&u4Node,
			       HqaCmdFrame->Data + 4 * 4, 4);
			u4Node = ntohl(u4Node);
			memcpy((uint8_t *)&u4Len, HqaCmdFrame->Data + 4 * 5, 4);
			u4Len = ntohl(u4Len);
			memcpy((uint8_t *)&u4StopCycle,
			       HqaCmdFrame->Data + 4 * 6,
			       4);
			u4StopCycle = ntohl(u4StopCycle);
			memcpy((uint8_t *)&u4BW, HqaCmdFrame->Data + 4 * 7, 4);
			u4BW = ntohl(u4BW);
			memcpy((uint8_t *)&u4MacTriggerEvent,
			       HqaCmdFrame->Data + 4 * 8, 4);
			u4MacTriggerEvent = ntohl(u4MacTriggerEvent);
			memcpy((uint8_t *)&aucSourceAddress,
			       HqaCmdFrame->Data + 4 * 9, MAC_ADDR_LEN);
			memcpy((uint8_t *)&u4Band,
			       HqaCmdFrame->Data + 4 * 9 + MAC_ADDR_LEN, 4);
			u4Band = ntohl(u4Band);

			/* AT Command #1, Trigger always = 1 */
			DBGLOG(RFTEST, INFO,
				"u4Trigger=%u, u4RingCapEn=%u, u4TriggerEvent=%u\n",
				u4Trigger, u4RingCapEn, u4Trigger);
			/* AT Command #81 */
			DBGLOG(RFTEST, INFO,
				"u4Node=%u, u4Len=%u, u4topCycle=%u, u4BW=%u, u4Band=%d",
				u4Node, u4Len, u4StopCycle, u4BW, u4Band);

			u4SourceAddrLSB = ((aucSourceAddress[0]) |
					   (aucSourceAddress[1] << 8) |
					   (aucSourceAddress[2]) << 16 |
					   (aucSourceAddress[3]) << 24);
			u4SourceAddrMSB = ((aucSourceAddress[4]) |
					   (aucSourceAddress[5] << 8) |
					   (0x1 << 16));

			prGlueInfo->prAdapter->rIcapInfo.u4CapNode = u4Node;
			i4Ret = prAteOps->setICapStart(prGlueInfo, u4Trigger,
					       u4RingCapEn, u4Event, u4Node,
					       u4Len, u4StopCycle, u4BW,
					       u4MacTriggerEvent,
					       u4SourceAddrLSB,
					       u4SourceAddrMSB, u4Band);
		} else
			i4Ret = 1;

		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	} else if (u4Control == 2) {
		if (prAteOps->getICapStatus)
			i4Ret = prAteOps->getICapStatus(prGlueInfo);
		else
			i4Ret = 1;
		/* Query whether ICAP Done */
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	} else if (u4Control == 3) {
		if (prAteOps->getICapIQData) {
			kalMemCopy((uint8_t *)&u4WFNum,
				   HqaCmdFrame->Data + 4 * 1,
				   4);
			u4WFNum = ntohl(u4WFNum);
			kalMemCopy((uint8_t *)&u4IQ,
				   HqaCmdFrame->Data + 4 * 2,
				   4);
			u4IQ = ntohl(u4IQ);

			DBGLOG(RFTEST, INFO,
			       "u4WFNum = %d, u4IQ = %d\n", u4WFNum, u4IQ);

			u4DataLen = prAteOps->getICapIQData(
						prGlueInfo,
						&HqaCmdFrame->Data[2 + 4 * 4],
						u4IQ,
						u4WFNum);
			/* tool want data count instead of buff length */
			u4TempLen = u4DataLen / 4;
			u4Control = ntohl(u4Control);
			kalMemCopy(HqaCmdFrame->Data + 2 + 4 * 0,
					   (uint8_t *)&u4Control,
					   sizeof(u4Control));
			u4WFNum = ntohl(u4WFNum);
			kalMemCopy(HqaCmdFrame->Data + 2 + 4 * 1,
					   (uint8_t *)&u4WFNum,
					   sizeof(u4WFNum));
			u4IQ = ntohl(u4IQ);
			kalMemCopy(HqaCmdFrame->Data + 2 + 4 * 2,
					   (uint8_t *)&u4IQ,
					   sizeof(u4IQ));
			u4TempLen = ntohl(u4TempLen);
			kalMemCopy(HqaCmdFrame->Data + 2 + 4 * 3,
					   (uint8_t *)&u4TempLen,
					   sizeof(u4TempLen));

		}

		i4Ret = 0;
		/* Get IQ Data and transmit them to UI DLL */
		ResponseToQA(HqaCmdFrame,
			     prIwReqData,
			     2 + 4 * 4 + u4DataLen,
			     i4Ret);
	} else {
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	}
	return 0;
}

int32_t mt6632SetICapStart(struct GLUE_INFO *prGlueInfo,
			   uint32_t u4Trigger, uint32_t u4RingCapEn,
			   uint32_t u4Event, uint32_t u4Node, uint32_t u4Len,
			   uint32_t u4StopCycle,
			   uint32_t u4BW, uint32_t u4MacTriggerEvent,
			   uint32_t u4SourceAddrLSB,
			   uint32_t u4SourceAddrMSB, uint32_t u4Band)
{
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	/* iwpriv wlan205 set_test_cmd 75 0   (J mode Setting) */
	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_J_MODE;
	rRfATInfo.u4FuncData = 0;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	/* iwpriv wlan205 set_test_cmd 71 0   (Channel Bandwidth) */
	if (u4BW == 4)
		u4BW = 3;
	else if (u4BW == 3)
		u4BW = 4;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_CBW;
	rRfATInfo.u4FuncData = u4BW;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	/* iwpriv wlan205 set_test_cmd 24 0     (ADC clock mode) */
	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_ADC_CLK_MODE;
	rRfATInfo.u4FuncData = 0;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	/* iwpriv wlan205 set_test_cmd 84 18000
	 * (Internal Capture Trigger Offset)
	 */
	rRfATInfo.u4FuncIndex =
		RF_AT_FUNCID_SET_ICAP_TRIGGER_OFFSET;
	rRfATInfo.u4FuncData = u4StopCycle;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	if (u4Len == 0)
		u4Len = 196615;/* 24000; */
	/* iwpriv wlan205 set_test_cmd 83 24576   (Internal Capture Size) */
	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_SIZE;
	rRfATInfo.u4FuncData = u4Len;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;
	/* iwpriv wlan205 set_test_cmd 80 0   (Internal Capture Content) */
	if (u4Node == 0x6)
		u4Node = 0x10000006;
	else if (u4Node == 0x8)
		u4Node = 0x49;
	else if (u4Node == 0x9)
		u4Node = 0x48;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_CONTENT;
	rRfATInfo.u4FuncData = u4Node;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	/* iwpriv wlan205 set_test_cmd 81 0 (Internal Capture Trigger mode) */
	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_MODE;
	rRfATInfo.u4FuncData = u4Trigger;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_RING;
	rRfATInfo.u4FuncData = u4RingCapEn;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;
	/* iwpriv wlan205 set_test_cmd 1 13 */
	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_CH_SWITCH_FOR_ICAP;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;
	/* iwpriv wlan205 set_test_cmd 1 11 */
	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_ICAP;

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;
	return 0;
}

int32_t mt6632GetICapStatus(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	if (prAdapter->rIcapInfo.eIcapState == ICAP_STATE_FW_DUMP_DONE) {
		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_CapWiFiSpectrum Done!!!!!!!!!!!!!!!!!\n");
		return 0;
	}
	return 1;
}

int32_t connacSetICapStart(struct GLUE_INFO *prGlueInfo,
			   uint32_t u4Trigger, uint32_t u4RingCapEn,
			   uint32_t u4Event, uint32_t u4Node, uint32_t u4Len,
			   uint32_t u4StopCycle,
			   uint32_t u4BW, uint32_t u4MacTriggerEvent,
			   uint32_t u4SourceAddrLSB,
			   uint32_t u4SourceAddrMSB, uint32_t u4Band)
{
	struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T rRfATInfo;
	struct RBIST_CAP_START_T *prICapInfo = NULL;
	uint32_t u4BufLen = 0, u4IQArrayLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	if (u4Trigger) {
		if (prGlueInfo->prAdapter->rIcapInfo.eIcapState
			!= ICAP_STATE_INIT) {
			log_dbg(RFTEST, ERROR, "Already starting, ignore\n");
			return 1;
		}
	} else {
		log_dbg(RFTEST, INFO, "Shutdown Icap\n");
		prGlueInfo->prAdapter->rIcapInfo.eIcapState = ICAP_STATE_INIT;
		if (prGlueInfo->prAdapter->rIcapInfo.prIQArray != NULL)
			kalMemFree(prGlueInfo->prAdapter->rIcapInfo.prIQArray,
				   VIR_MEM_TYPE,
				   u4IQArrayLen);
		prGlueInfo->prAdapter->rIcapInfo.u4IQArrayIndex = 0;
		prGlueInfo->prAdapter->rIcapInfo.u4ICapEventCnt = 0;
		prGlueInfo->prAdapter->rIcapInfo.prIQArray = NULL;
		return 0;
	}

	prICapInfo = &(rRfATInfo.Data.rICapInfo);
	prICapInfo->u4Trigger = u4Trigger;
	prICapInfo->u4TriggerEvent = u4Event;

	u4IQArrayLen = MAX_ICAP_IQ_DATA_CNT * sizeof(struct _RBIST_IQ_DATA_T);
#if 0
	if (prGlueInfo->prAdapter->rIcapInfo.prIQArray != NULL)
		kalMemFree(prGlueInfo->prAdapter->rIcapInfo.prIQArray,
			   VIR_MEM_TYPE,
			   u4IQArrayLen);
#endif

	if (!prGlueInfo->prAdapter->rIcapInfo.prIQArray) {
		prGlueInfo->prAdapter->rIcapInfo.prIQArray =
				kalMemAlloc(u4IQArrayLen, VIR_MEM_TYPE);
		if (!prGlueInfo->prAdapter->rIcapInfo.prIQArray) {
			DBGLOG(RFTEST, ERROR,
				"Not enough memory for IQ_Array\n");
			return 0;
		}
	}

	prGlueInfo->prAdapter->rIcapInfo.u4IQArrayIndex = 0;
	prGlueInfo->prAdapter->rIcapInfo.u4ICapEventCnt = 0;
	kalMemZero(prGlueInfo->prAdapter->rIcapInfo.au4ICapDumpIndex,
		sizeof(prGlueInfo->prAdapter->rIcapInfo.au4ICapDumpIndex));
	kalMemZero(prGlueInfo->prAdapter->rIcapInfo.prIQArray, u4IQArrayLen);

	if (prICapInfo->u4TriggerEvent == CAP_FREE_RUN)
		prICapInfo->u4RingCapEn = CAP_RING_MODE_DISABLE;
	else
		prICapInfo->u4RingCapEn = CAP_RING_MODE_ENABLE;

	prICapInfo->u4CaptureNode = u4Node;
	prICapInfo->u4CaptureLen = u4Len;
	prICapInfo->u4CapStopCycle = u4StopCycle;
	prICapInfo->u4BW = u4BW;
	prICapInfo->u4MacTriggerEvent = u4MacTriggerEvent;
	prICapInfo->u4SourceAddressLSB = u4SourceAddrLSB;
	prICapInfo->u4SourceAddressMSB = u4SourceAddrMSB;
	prICapInfo->u4BandIdx = u4Band;
	prICapInfo->u4EnBitWidth = 0;
	prICapInfo->u4Architech = 1;
	prICapInfo->u4PhyIdx = 0;
#if (CFG_MTK_ANDROID_EMI == 1)
	prICapInfo->u4EmiStartAddress =
		(uint32_t) (gConEmiPhyBase & 0xFFFFFFFF);
	prICapInfo->u4EmiEndAddress =
		(uint32_t) ((gConEmiPhyBase + gConEmiSize) & 0xFFFFFFFF);
	prICapInfo->u4EmiMsbAddress =
		(uint32_t) ((((uint64_t) gConEmiPhyBase) >> 32) & 0xFFFFFFFF);

	DBGLOG(RFTEST, INFO,
		"startAddr = 0x%08x, endAddress = 0x%08x, MsbAddr = 0x%08x\n",
		  prICapInfo->u4EmiStartAddress,
		  prICapInfo->u4EmiEndAddress,
		  prICapInfo->u4EmiMsbAddress);
#else
	DBGLOG(RFTEST, WARN, "Platform doesn't support WMT, no EMI address\n");
#endif

	DBGLOG(RFTEST, INFO,
	       "%s :\n prICapInfo->u4Trigger = 0x%08x\n prICapInfo->u4RingCapEn = 0x%08x\n"
	       "prICapInfo->u4TriggerEvent = 0x%08x\n prICapInfo->u4CaptureNode = 0x%08x\n"
	       "prICapInfo->u4CaptureLen = 0x%08x\n prICapInfo->u4CapStopCycle = 0x%08x\n"
	       "prICapInfo->ucBW = 0x%08x\n prICapInfo->u4MacTriggerEvent = 0x%08x\n"
	       "prICapInfo->u4SourceAddressLSB = 0x%08x\n prICapInfo->u4SourceAddressMSB = 0x%08x\n"
	       "prICapInfo->u4BandIdx = 0x%08x\n",
	       __func__,
	       prICapInfo->u4Trigger, prICapInfo->u4RingCapEn,
	       prICapInfo->u4TriggerEvent, prICapInfo->u4CaptureNode,
	       prICapInfo->u4CaptureLen,
	       prICapInfo->u4CapStopCycle, prICapInfo->u4BW,
	       prICapInfo->u4MacTriggerEvent,
	       prICapInfo->u4SourceAddressLSB,
	       prICapInfo->u4SourceAddressMSB, prICapInfo->u4BandIdx);

	rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
			   wlanoidExtRfTestICapStart,	/* pfnOidHandler */
			   &rRfATInfo,	/* pvInfoBuf */
			   sizeof(rRfATInfo),	/* u4InfoBufLen */
			   FALSE,	/* fgRead */
			   FALSE,	/* fgWaitResp */
			   TRUE,	/* fgCmd */
			   &u4BufLen);	/* pu4QryInfoLen */
	return 0;
}

int32_t connacGetICapStatus(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;
	struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T rRfATInfo;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	enum ENUM_ICAP_STATE eIcapState = ICAP_STATE_INIT;


	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	eIcapState = prGlueInfo->prAdapter->rIcapInfo.eIcapState;

	/*FW dump IQ data done*/
	if (eIcapState == ICAP_STATE_FW_DUMP_DONE) {
		DBGLOG(RFTEST, INFO,
		       "QA_AGENT HQA_CapWiFiSpectrum Done!!!!!!!!!!!!!!!!!\n");
		return 0;
	}

	if (eIcapState != ICAP_STATE_FW_DUMPING) {
		rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
				   wlanoidExtRfTestICapStatus,
				   &rRfATInfo,	/* pvInfoBuf */
				   sizeof(rRfATInfo),	/* u4InfoBufLen */
				   FALSE,	/* fgRead */
				   FALSE,	/* fgWaitResp */
				   TRUE,	/* fgCmd */
				   &u4BufLen);	/* pu4QryInfoLen */
	}

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_CapWiFiSpectrum Wait!!!!!!!!!!!!\n");
	return 1;
}

int32_t commonGetICapIQData(struct GLUE_INFO *prGlueInfo,
			    uint8_t *pData, uint32_t u4IQType, uint32_t u4WFNum)
{
	struct ADAPTER *prAdapter;
	uint32_t u4TempLen = 0;
	uint32_t u4DataLen = 0;
	int32_t *prIQAry;
	int32_t i = 0;

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	if (u4WFNum <= 1) {
		GetIQData(prAdapter, &prIQAry, &u4DataLen, u4IQType,
			  u4WFNum);
		u4TempLen = u4DataLen;
		u4DataLen /= 4;

		u4DataLen = ntohl(u4DataLen);
		memcpy(pData + 2 + 4 * 3, (uint8_t *) &u4DataLen,
		       sizeof(u4DataLen));

		for (i = 0; i < u4TempLen / sizeof(uint32_t); i++)
			prIQAry[i] = ntohl(prIQAry[i]);

		memcpy(pData + 2 + 4 * 4, (uint8_t *) &prIQAry[0],
		       u4TempLen);
	}
	return u4TempLen;
}

int32_t connacGetICapIQData(struct GLUE_INFO *prGlueInfo,
			    uint8_t *pData, uint32_t u4IQType, uint32_t u4WFNum)
{
	struct RBIST_DUMP_IQ_T rRbistDump;
	struct ADAPTER *prAdapter;
	struct ICAP_INFO_T *prICapInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint32_t i = 0;
	uint32_t u4Value = 0;

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);
	prICapInfo = &prAdapter->rIcapInfo;

	rRbistDump.u4IQType = u4IQType;
	rRbistDump.u4WfNum = u4WFNum;
	rRbistDump.u4IcapCnt = 0;
	rRbistDump.u4IcapDataLen = 0;
	rRbistDump.pucIcapData = pData;

	if ((prICapInfo->eIcapState == ICAP_STATE_FW_DUMP_DONE) ||
		(prICapInfo->eIcapState == ICAP_STATE_QA_TOOL_CAPTURE)) {
		rStatus = kalIoctl(prGlueInfo,	/* prGlueInfo */
				   wlanoidRfTestICapGetIQData,
				   &rRbistDump,	/* pvInfoBuf */
				   sizeof(rRbistDump),	/* u4InfoBufLen */
				   TRUE,	/* fgRead */
				   TRUE,	/* fgWaitResp */
				   FALSE,	/* fgCmd */
				   &u4BufLen);	/* pu4QryInfoLen */

	} else
		DBGLOG(RFTEST, ERROR, "ICAP IQ Dump fail in State = %d\n",
			prICapInfo->eIcapState);

	/*IQ data network byte oder transfer to host byte order*/
	/*each (I or Q) data size is 4Byte*/
	if (rStatus == WLAN_STATUS_SUCCESS) {
		for (i = 0; i < rRbistDump.u4IcapDataLen;
		  i += sizeof(uint32_t)) {
			u4Value = *(pData + i);
			u4Value = ntohl(u4Value);
			kalMemCopy((pData + i),
				(uint8_t *) &u4Value,
				sizeof(u4Value));
		}
	}

	return rRbistDump.u4IcapDataLen;
}


static HQA_CMD_HANDLER HQA_ICAP_CMDS[] = {
	HQA_CapWiFiSpectrum,	/* 0x1580 */
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t hqa_set_channel_ext(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ext_id = 0;
	uint32_t u4Param_num = 0;
	uint32_t u4Band_idx = 0;
	uint32_t u4Central_ch0 = 0;
	uint32_t u4Central_ch1 = 0;
	uint32_t u4Sys_bw = 0;
	uint32_t u4Perpkt_bw = 0;
	uint32_t u4Pri_sel = 0;
	uint32_t u4Reason = 0;
	uint32_t u4Ch_band = 0;
	uint32_t u4SetFreq = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4 * 1, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 2, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(&u4Central_ch0, HqaCmdFrame->Data + 4 * 3, 4);
	u4Central_ch0 = ntohl(u4Central_ch0);
	memcpy(&u4Central_ch1, HqaCmdFrame->Data + 4 * 4, 4);
	u4Central_ch1 = ntohl(u4Central_ch1);
	memcpy(&u4Sys_bw, HqaCmdFrame->Data + 4 * 5, 4);
	u4Sys_bw = ntohl(u4Sys_bw);
	memcpy(&u4Perpkt_bw, HqaCmdFrame->Data + 4 * 6, 4);
	u4Perpkt_bw = ntohl(u4Perpkt_bw);
	memcpy(&u4Pri_sel, HqaCmdFrame->Data + 4 * 7, 4);
	u4Pri_sel = ntohl(u4Pri_sel);
	memcpy(&u4Reason, HqaCmdFrame->Data + 4 * 8, 4);
	u4Reason = ntohl(u4Reason);
	memcpy(&u4Ch_band, HqaCmdFrame->Data + 4 * 9, 4);
	u4Ch_band = ntohl(u4Ch_band);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext param_num : %d\n",
	       u4Param_num);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext central_ch0 : %d\n",
	       u4Central_ch0);
	/* for BW80+80 */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext central_ch1 : %d\n",
	       u4Central_ch1);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext sys_bw : %d\n", u4Sys_bw);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext perpkt_bw : %d\n",
	       u4Perpkt_bw);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext pri_sel : %d\n", u4Pri_sel);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext reason : %d\n", u4Reason);
	/* 0:2.4G    1:5G */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_channel_ext ch_band : %d\n", u4Ch_band);

	/* BW Mapping in QA Tool
	 * 0: BW20
	 * 1: BW40
	 * 2: BW80
	 * 3: BW10
	 * 4: BW5
	 * 5: BW160C
	 * 6: BW160NC
	 */
	/* BW Mapping in FW
	 * 0: BW20
	 * 1: BW40
	 * 2: BW80
	 * 3: BW160C
	 * 4: BW160NC
	 * 5: BW5
	 * 6: BW10
	 */
	/* For POR Cal Setting - 20160601 */
	if ((u4Central_ch0 == u4Central_ch1) && (u4Sys_bw == 6)
	    && (u4Perpkt_bw == 6)) {
		DBGLOG(RFTEST, INFO, "Wrong Setting for POR Cal\n");
		goto exit;
	}

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	if ((u4Central_ch0 >= 7 && u4Central_ch0 <= 16)
	    && u4Ch_band == 1) {
		/*Ch7 - Ch12, 5G (5035-5060)*/
		u4SetFreq = 1000 * (5000 + u4Central_ch0 * 5);
	} else if (u4Central_ch0 == 6 && u4Ch_band == 1) {
		u4SetFreq = 1000 * 5032;
	} else {
		u4SetFreq = nicChannelNum2Freq(u4Central_ch0);
	}
	MT_ATESetChannel(prNetDev, 0, u4SetFreq);

	if (u4Sys_bw == 6) {
		u4SetFreq = nicChannelNum2Freq(u4Central_ch1);
		MT_ATESetChannel(prNetDev, 1, u4SetFreq);
	}

	MT_ATESetSystemBW(prNetDev, u4Sys_bw);

	/* For POR Cal Setting - 20160601 */
	if ((u4Sys_bw == 6) && (u4Perpkt_bw == 6))
		MT_ATESetPerPacketBW(prNetDev, 5);
	else
		MT_ATESetPerPacketBW(prNetDev, u4Perpkt_bw);

	MT_ATEPrimarySetting(prNetDev, u4Pri_sel);
	/* PeiHsuan Memo : No Set Reason ? */
	MT_ATESetBand(prNetDev, u4Ch_band);

exit:
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);
	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t hqa_set_txcontent_ext(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Len = 0;
	uint32_t u4Ext_id = 0;
	uint32_t u4Param_num = 0;
	uint32_t u4Band_idx = 0;
	uint32_t u4FC = 0;
	uint32_t u4Dur = 0;
	uint32_t u4Seq = 0;
	uint32_t u4Gen_payload_rule = 0;
	uint32_t u4Txlen = 0;
	uint32_t u4Payload_len = 0;
	uint8_t ucAddr1[MAC_ADDR_LEN];
	uint8_t ucAddr2[MAC_ADDR_LEN];
	uint8_t ucAddr3[MAC_ADDR_LEN];
	uint32_t ucPayload = 0;

	u4Len = ntohs(HqaCmdFrame->Length);

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4 * 1, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 2, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(&u4FC, HqaCmdFrame->Data + 4 * 3, 4);
	u4FC = ntohl(u4FC);
	memcpy(&u4Dur, HqaCmdFrame->Data + 4 * 4, 4);
	u4Dur = ntohl(u4Dur);
	memcpy(&u4Seq, HqaCmdFrame->Data + 4 * 5, 4);
	u4Seq = ntohl(u4Seq);
	memcpy(&u4Gen_payload_rule, HqaCmdFrame->Data + 4 * 6, 4);
	u4Gen_payload_rule = ntohl(u4Gen_payload_rule);
	memcpy(&u4Txlen, HqaCmdFrame->Data + 4 * 7, 4);
	u4Txlen = ntohl(u4Txlen);
	memcpy(&u4Payload_len, HqaCmdFrame->Data + 4 * 8, 4);
	u4Payload_len = ntohl(u4Payload_len);
	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 9, 6);
	memcpy(ucAddr2, HqaCmdFrame->Data + 4 * 9 + 6 * 1, 6);
	memcpy(ucAddr3, HqaCmdFrame->Data + 4 * 9 + 6 * 2, 6);
	memcpy(&ucPayload, HqaCmdFrame->Data + 4 * 9 + 6 * 3, 1);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext param_num : %d\n",
	       u4Param_num);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext band_idx : %d\n",
	       u4Band_idx);
	/* Frame Control...0800 : Beacon */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext FC : 0x%x\n", u4FC);
	/* Duration....NAV */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext dur : 0x%x\n", u4Dur);
	/* Sequence Control */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext seq : 0x%x\n", u4Seq);
	/* Normal:0,Repeat:1,Random:2 */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext gen_payload_rule : %d\n",
	       u4Gen_payload_rule);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext txlen : %d\n", u4Txlen);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext payload_len : %d\n",
	       u4Payload_len);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext addr1:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4],
	       ucAddr1[5]);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext addr2:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr2[0], ucAddr2[1], ucAddr2[2], ucAddr2[3], ucAddr2[4],
	       ucAddr2[5]);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_set_txcontent_ext addr3:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr3[0], ucAddr3[1], ucAddr3[2], ucAddr3[3], ucAddr3[4],
	       ucAddr3[5]);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext payload : 0x%x\n", ucPayload);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATESetMacHeader(prNetDev, u4FC, u4Dur, u4Seq);
	MT_ATESetTxPayLoad(prNetDev, u4Gen_payload_rule, ucPayload);
	MT_ATESetTxLength(prNetDev, u4Txlen);
	MT_ATESetMACAddress(prNetDev, RF_AT_FUNCID_SET_MAC_ADDRESS,
			    ucAddr1);
	MT_ATESetMACAddress(prNetDev, RF_AT_FUNCID_SET_TA, ucAddr2);
	/* PeiHsuan Memo : No Set Addr3 */

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Ext_id),
		     i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t hqa_start_tx_ext(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ext_id = 0;
	uint32_t u4Param_num = 0;
	uint32_t u4Band_idx = 0;
	uint32_t u4Pkt_cnt = 0;
	uint32_t u4Phymode = 0;
	uint32_t u4Rate = 0;
	uint32_t u4Pwr = 0;
	uint32_t u4Stbc = 0;
	uint32_t u4Ldpc = 0;
	uint32_t u4iBF = 0;
	uint32_t u4eBF = 0;
	uint32_t u4Wlan_id = 0;
	uint32_t u4Aifs = 0;
	uint32_t u4Gi = 0;
	uint32_t u4Tx_path = 0;
	uint32_t u4Nss = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4 * 1, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 2, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(&u4Pkt_cnt, HqaCmdFrame->Data + 4 * 3, 4);
	u4Pkt_cnt = ntohl(u4Pkt_cnt);
	memcpy(&u4Phymode, HqaCmdFrame->Data + 4 * 4, 4);
	u4Phymode = ntohl(u4Phymode);
	memcpy(&u4Rate, HqaCmdFrame->Data + 4 * 5, 4);
	u4Rate = ntohl(u4Rate);
	memcpy(&u4Pwr, HqaCmdFrame->Data + 4 * 6, 4);
	u4Pwr = ntohl(u4Pwr);
	memcpy(&u4Stbc, HqaCmdFrame->Data + 4 * 7, 4);
	u4Stbc = ntohl(u4Stbc);
	memcpy(&u4Ldpc, HqaCmdFrame->Data + 4 * 8, 4);
	u4Ldpc = ntohl(u4Ldpc);
	memcpy(&u4iBF, HqaCmdFrame->Data + 4 * 9, 4);
	u4iBF = ntohl(u4iBF);
	memcpy(&u4eBF, HqaCmdFrame->Data + 4 * 10, 4);
	u4eBF = ntohl(u4eBF);
	memcpy(&u4Wlan_id, HqaCmdFrame->Data + 4 * 11, 4);
	u4Wlan_id = ntohl(u4Wlan_id);
	memcpy(&u4Aifs, HqaCmdFrame->Data + 4 * 12, 4);
	u4Aifs = ntohl(u4Aifs);
	memcpy(&u4Gi, HqaCmdFrame->Data + 4 * 13, 4);
	u4Gi = ntohl(u4Gi);
	memcpy(&u4Tx_path, HqaCmdFrame->Data + 4 * 14, 4);
	u4Tx_path = ntohl(u4Tx_path);
	memcpy(&u4Nss, HqaCmdFrame->Data + 4 * 15, 4);
	u4Nss = ntohl(u4Nss);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext pkt_cnt : %d\n", u4Pkt_cnt);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext phymode : %d\n", u4Phymode);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext rate : %d\n", u4Rate);
	DBGLOG(RFTEST, INFO, "QA_AGENT hqa_start_tx_ext pwr : %d\n",
	       u4Pwr);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext stbc : %d\n", u4Stbc);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext ldpc : %d\n", u4Ldpc);
	DBGLOG(RFTEST, INFO, "QA_AGENT hqa_start_tx_ext ibf : %d\n",
	       u4iBF);
	DBGLOG(RFTEST, INFO, "QA_AGENT hqa_start_tx_ext ebf : %d\n",
	       u4eBF);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext wlan_id : %d\n", u4Wlan_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext aifs : %d\n", u4Aifs);
	DBGLOG(RFTEST, INFO, "QA_AGENT hqa_start_tx_ext gi : %d\n",
	       u4Gi);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_tx_ext tx_path : %d\n", u4Tx_path);
	DBGLOG(RFTEST, INFO, "QA_AGENT hqa_start_tx_ext nss : %d\n",
	       u4Nss);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATESetTxCount(prNetDev, u4Pkt_cnt);

#if 1
	if (u4Phymode == 1) {
		u4Phymode = 0;
		u4Rate += 4;
	} else if ((u4Phymode == 0) && ((u4Rate == 9)
					|| (u4Rate == 10) || (u4Rate == 11)))
		u4Phymode = 1;
	MT_ATESetPreamble(prNetDev, u4Phymode);

	if (u4Phymode == 0) {
		u4Rate |= 0x00000000;

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT CCK/OFDM (normal preamble) rate : %d\n",
		       u4Rate);

		MT_ATESetRate(prNetDev, u4Rate);
	} else if (u4Phymode == 1) {
		if (u4Rate == 9)
			u4Rate = 1;
		else if (u4Rate == 10)
			u4Rate = 2;
		else if (u4Rate == 11)
			u4Rate = 3;
		u4Rate |= 0x00000000;

		DBGLOG(RFTEST, INFO,
		       "QA_AGENT CCK (short preamble) rate : %d\n", u4Rate);

		MT_ATESetRate(prNetDev, u4Rate);
	} else if (u4Phymode >= 2 && u4Phymode <= 4) {
		u4Rate |= 0x80000000;

		DBGLOG(RFTEST, INFO, "QA_AGENT HT/VHT rate : %d\n", u4Rate);

		MT_ATESetRate(prNetDev, u4Rate);
	}
#endif

	MT_ATESetTxPower0(prNetDev, u4Pwr);
	MT_ATESetTxSTBC(prNetDev, u4Stbc);
	MT_ATESetEncodeMode(prNetDev, u4Ldpc);
	MT_ATESetiBFEnable(prNetDev, u4iBF);
	MT_ATESeteBFEnable(prNetDev, u4eBF);
	/* PeiHsuan Memo : No Set Wlan ID */
	MT_ATESetTxIPG(prNetDev, u4Aifs);
	MT_ATESetTxGi(prNetDev, u4Gi);
	MT_ATESetTxVhtNss(prNetDev, u4Nss);
	MT_ATESetTxPath(prNetDev, u4Tx_path);
	MT_ATEStartTX(prNetDev, "TXFRAME");

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Ext_id),
		     i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t hqa_start_rx_ext(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ext_id = 0;
	uint32_t u4Param_num = 0;
	uint32_t u4Band_idx = 0;
	uint32_t u4Rx_path = 0;
	uint8_t ucOwn_mac[MAC_ADDR_LEN];
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	uint32_t u4BufLen = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	memcpy(&u4Ext_id, HqaCmdFrame->Data, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(ucOwn_mac, HqaCmdFrame->Data + 4 + 4 + 4, 6);
	memcpy(&u4Rx_path, HqaCmdFrame->Data + 4 + 4 + 4 + 6, 4);
	u4Rx_path = ntohl(u4Rx_path);

	memset(&g_HqaRxStat, 0, sizeof(struct PARAM_RX_STAT));

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_rx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_rx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_rx_ext band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_rx_ext own_mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucOwn_mac[0], ucOwn_mac[1], ucOwn_mac[2], ucOwn_mac[3],
	       ucOwn_mac[4], ucOwn_mac[5]);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_start_rx_ext rx_path : 0x%x\n", u4Rx_path);

	u4RxStatSeqNum = 0;

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RX_PATH;
	rRfATInfo.u4FuncData = u4Rx_path << 16 | u4Band_idx;

	i4Ret = kalIoctl(prGlueInfo,	/* prGlueInfo */
			 wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			 &rRfATInfo,	/* pvInfoBuf */
			 sizeof(rRfATInfo),	/* u4InfoBufLen */
			 FALSE,	/* fgRead */
			 FALSE,	/* fgWaitResp */
			 TRUE,	/* fgCmd */
			 &u4BufLen);	/* pu4QryInfoLen */

	if (i4Ret != WLAN_STATUS_SUCCESS)
		return -EFAULT;
	/* PeiHsuan Memo : No Set Own MAC Address */
	MT_ATEStartRX(prNetDev, "RXFRAME");

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Ext_id),
		     i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t hqa_stop_tx_ext(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ext_id = 0;
	uint32_t u4Param_num = 0;
	uint32_t u4Band_idx = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_stop_tx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_stop_tx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_stop_tx_ext band_idx : %d\n", u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATEStopTX(prNetDev, "TXSTOP");

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t hqa_stop_rx_ext(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ext_id = 0;
	uint32_t u4Param_num = 0;
	uint32_t u4Band_idx = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_stop_rx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_stop_rx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT hqa_stop_rx_ext band_idx : %d\n", u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATEStopRX(prNetDev, "RXSTOP");

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static int32_t HQA_iBFInit(struct net_device *prNetDev,
			   IN union iwreq_data *prIwReqData,
			   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_iBFInit\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_iBFSetValue(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData,
			       struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_iBFSetValue\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_iBFGetStatus(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_iBFGetStatus\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_iBFChanProfUpdate(struct net_device
				     *prNetDev,
				     IN union iwreq_data *prIwReqData,
				     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_iBFChanProfUpdate\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_iBFProfileRead(struct net_device
				  *prNetDev,
				  IN union iwreq_data *prIwReqData,
				  struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_iBFProfileRead\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static int32_t HQA_IRRSetADC(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4WFIdx;
	uint32_t u4ChFreq;
	uint32_t u4BW;
	uint32_t u4Sx;
	uint32_t u4Band;
	uint32_t u4Ext_id;
	uint32_t u4RunType;
	uint32_t u4FType;

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4WFIdx, HqaCmdFrame->Data + 4 * 1, 4);
	u4WFIdx = ntohl(u4WFIdx);
	memcpy(&u4ChFreq, HqaCmdFrame->Data + 4 * 2, 4);
	u4ChFreq = ntohl(u4ChFreq);
	memcpy(&u4BW, HqaCmdFrame->Data + 4 * 3, 4);
	u4BW = ntohl(u4BW);
	memcpy(&u4Sx, HqaCmdFrame->Data + 4 * 4, 4);
	u4Sx = ntohl(u4Sx);
	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 5, 4);
	u4Band = ntohl(u4Band);
	memcpy(&u4RunType, HqaCmdFrame->Data + 4 * 6, 4);
	u4RunType = ntohl(u4RunType);
	memcpy(&u4FType, HqaCmdFrame->Data + 4 * 7, 4);
	u4FType = ntohl(u4FType);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_IRRSetADC ext_id : %d\n",
	       u4Ext_id);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetADC u4WFIdx : %d\n", u4WFIdx);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetADC u4ChFreq : %d\n", u4ChFreq);
	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_IRRSetADC u4BW : %d\n",
	       u4BW);
	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_IRRSetADC u4Sx : %d\n",
	       u4Sx);	/* SX : 0, 2 */
	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_IRRSetADC u4Band : %d\n",
	       u4Band);
	/* RunType : 0 -> QA, 1 -> ATE */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetADC u4RunType : %d\n", u4RunType);
	/* FType : 0 -> FI, 1 -> FD */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetADC u4FType : %d\n", u4FType);

	i4Ret = MT_ATE_IRRSetADC(prNetDev, u4WFIdx, u4ChFreq, u4BW,
				 u4Sx, u4Band, u4RunType, u4FType);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static int32_t HQA_IRRSetRxGain(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData,
				struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4PgaLpfg;
	uint32_t u4Lna;
	uint32_t u4Band;
	uint32_t u4WF_inx;
	uint32_t u4Rfdgc;
	uint32_t u4Ext_id;

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4PgaLpfg, HqaCmdFrame->Data + 4 * 1, 4);
	u4PgaLpfg = ntohl(u4PgaLpfg);
	memcpy(&u4Lna, HqaCmdFrame->Data + 4 * 2, 4);
	u4Lna = ntohl(u4Lna);
	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 3, 4);
	u4Band = ntohl(u4Band);
	memcpy(&u4WF_inx, HqaCmdFrame->Data + 4 * 4, 4);
	u4WF_inx = ntohl(u4WF_inx);
	memcpy(&u4Rfdgc, HqaCmdFrame->Data + 4 * 5, 4);
	u4Rfdgc = ntohl(u4Rfdgc);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetRxGain ext_id : %d\n", u4Ext_id);
	/* PGA is for MT663, LPFG is for MT7615 */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetRxGain u4PgaLpfg : %d\n", u4PgaLpfg);
	/* 5 : UH, 4 : H, 3 : M, 2 : L, 1 : UL */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetRxGain u4Lna : %d\n", u4Lna);
	/* DBDC band0 or band1 */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetRxGain u4Band : %d\n", u4Band);
	/* (each bit for each WF) */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetRxGain u4WF_inx : 0x%x\n", u4WF_inx);
	/* only for */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetRxGain u4Rfdgc : %d\n", u4Rfdgc);

	i4Ret = MT_ATE_IRRSetRxGain(prNetDev, u4PgaLpfg, u4Lna,
				    u4Band, u4WF_inx, u4Rfdgc);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static int32_t HQA_IRRSetTTG(struct net_device *prNetDev,
			     IN union iwreq_data *prIwReqData,
			     struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ext_id;
	uint32_t u4TTGPwrIdx;
	uint32_t u4ChFreq;
	uint32_t u4FIToneFreq;
	uint32_t u4Band;

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4TTGPwrIdx, HqaCmdFrame->Data + 4 * 1, 4);
	u4TTGPwrIdx = ntohl(u4TTGPwrIdx);
	memcpy(&u4ChFreq, HqaCmdFrame->Data + 4 * 2, 4);
	u4ChFreq = ntohl(u4ChFreq);
	memcpy(&u4FIToneFreq, HqaCmdFrame->Data + 4 * 3, 4);
	u4FIToneFreq = ntohl(u4FIToneFreq);
	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 4, 4);
	u4Band = ntohl(u4Band);

	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_IRRSetTTG ext_id : %d\n",
	       u4Ext_id);
	/* TTG Power Index:   Power index value 0~15 */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetTTG u4TTGPwrIdx : %d\n", u4TTGPwrIdx);
	/* Ch Freq: channel frequency value */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetTTG u4ChFreq : %d\n", u4ChFreq);
	/* FI Tone Freq(float): driver calculate TTG Freq(TTG Freq = Ch_freq +
	 *                                                FI tone freq)
	 */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetTTG u4FIToneFreq : %d\n", u4FIToneFreq);
	/* Band: DBDC band0 or band1 */
	DBGLOG(RFTEST, INFO, "QA_AGENT HQA_IRRSetTTG u4Band : %d\n",
	       u4Band);

	i4Ret = MT_ATE_IRRSetTTG(prNetDev, u4TTGPwrIdx, u4ChFreq,
				 u4FIToneFreq, u4Band);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static int32_t HQA_IRRSetTrunOnTTG(struct net_device
				   *prNetDev,
				   IN union iwreq_data *prIwReqData,
				   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Ext_id;
	uint32_t u4TTGOnOff;
	uint32_t u4Band;
	uint32_t u4WF_inx = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4TTGOnOff, HqaCmdFrame->Data + 4 * 1, 4);
	u4TTGOnOff = ntohl(u4TTGOnOff);
	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 2, 4);
	u4Band = ntohl(u4Band);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetTrunOnTTG ext_id : %d\n", u4Ext_id);
	/* TTG on/off:  0:off,   1: on */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetTrunOnTTG u4TTGOnOff : %d\n",
	       u4TTGOnOff);
	/* Band: DBDC band0 or band1 */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetTrunOnTTG u4Band : %d\n", u4Band);
	/* (each bit for each WF) */
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_IRRSetTrunOnTTG u4WF_inx : %d\n", u4WF_inx);

	i4Ret = MT_ATE_IRRSetTrunOnTTG(prNetDev, u4TTGOnOff, u4Band,
				       u4WF_inx);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (uint8_t *) &u4Ext_id,
	       sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static HQA_CMD_HANDLER hqa_ext_cmd_set[] = {
	NULL,
	hqa_set_channel_ext,	/* 0x00000001 */
	hqa_set_txcontent_ext,	/* 0x00000002 */
	hqa_start_tx_ext,	/* 0x00000003 */
	hqa_start_rx_ext,	/* 0x00000004 */
	hqa_stop_tx_ext,	/* 0x00000005 */
	hqa_stop_rx_ext,	/* 0x00000006 */
	HQA_iBFInit,		/* 0x00000007 */
	HQA_iBFSetValue,	/* 0x00000008 */
	HQA_iBFGetStatus,	/* 0x00000009 */
	HQA_iBFChanProfUpdate,	/* 0x0000000A */
	HQA_iBFProfileRead,	/* 0x0000000B */
	ToDoFunction,		/* 0x0000000C */
	ToDoFunction,		/* 0x0000000D */
	ToDoFunction,		/* 0x0000000E */
	ToDoFunction,		/* 0x0000000F */
	ToDoFunction,		/* 0x00000010 */
	ToDoFunction,		/* 0x00000011 */
	ToDoFunction,		/* 0x00000012 */
	ToDoFunction,		/* 0x00000013 */
	ToDoFunction,		/* 0x00000014 */
	ToDoFunction,		/* 0x00000015 */
	ToDoFunction,		/* 0x00000016 */
	ToDoFunction,		/* 0x00000017 */
	ToDoFunction,		/* 0x00000018 */
	ToDoFunction,		/* 0x00000019 */
	ToDoFunction,		/* 0x0000001A */
	ToDoFunction,		/* 0x0000001B */
	ToDoFunction,		/* 0x0000001C */
	ToDoFunction,		/* 0x0000001D */
	ToDoFunction,		/* 0x0000001E */
	ToDoFunction,		/* 0x0000001F */
	HQA_IRRSetADC,		/* 0x00000020 */
	HQA_IRRSetRxGain,	/* 0x00000021 */
	HQA_IRRSetTTG,		/* 0x00000022 */
	HQA_IRRSetTrunOnTTG,	/* 0x00000023 */
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Extension Commands (For MT7615).
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
static int32_t hqa_ext_cmds(struct net_device *prNetDev,
			    IN union iwreq_data *prIwReqData,
			    struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Ret = 0;
	uint32_t u4Idx = 0;

	memmove((uint8_t *)&u4Idx, (uint8_t *)&HqaCmdFrame->Data,
		4);
	u4Idx = ntohl(u4Idx);

	DBGLOG(RFTEST, INFO, "QA_AGENT hqa_ext_cmds index : %d\n",
	       u4Idx);

	if (u4Idx < (sizeof(hqa_ext_cmd_set) / sizeof(HQA_CMD_HANDLER))) {
		if (hqa_ext_cmd_set[u4Idx] != NULL) {
			/* valid command */
			i4Ret = (*hqa_ext_cmd_set[u4Idx])(prNetDev,
				prIwReqData, HqaCmdFrame);
		} else {
			/* invalid command */
			DBGLOG(RFTEST, INFO,
			       "QA_AGENT hqa_ext_cmds cmd idx is NULL: %d\n",
			       u4Idx);
		}
	} else {
		/* invalid command */
		DBGLOG(RFTEST, INFO,
		"QA_AGENT hqa_ext_cmds cmd idx is not supported: %d\n", u4Idx);
	}

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_CMD_SET6[] = {
	/* cmd id start from 0x1600 */
	hqa_ext_cmds,		/* 0x1600 */
};

static struct HQA_CMD_TABLE HQA_CMD_TABLES[] = {
	{
		HQA_CMD_SET0,
		sizeof(HQA_CMD_SET0) / sizeof(HQA_CMD_HANDLER),
		0x1000,
	}
	,
	{
		HQA_CMD_SET1,
		sizeof(HQA_CMD_SET1) / sizeof(HQA_CMD_HANDLER),
		0x1100,
	}
	,
	{
		HQA_CMD_SET2,
		sizeof(HQA_CMD_SET2) / sizeof(HQA_CMD_HANDLER),
		0x1200,
	}
	,
	{
		HQA_CMD_SET3,
		sizeof(HQA_CMD_SET3) / sizeof(HQA_CMD_HANDLER),
		0x1300,
	}
	,
	{
		HQA_CMD_SET4,
		sizeof(HQA_CMD_SET4) / sizeof(HQA_CMD_HANDLER),
		0x1400,
	}
	,
	{
		HQA_CMD_SET5,
		sizeof(HQA_CMD_SET5) / sizeof(HQA_CMD_HANDLER),
		0x1500,
	}
	,
#if CFG_SUPPORT_TX_BF
	{
		HQA_TXBF_CMDS,
		sizeof(HQA_TXBF_CMDS) / sizeof(HQA_CMD_HANDLER),
		0x1540,
	}
	,
#if CFG_SUPPORT_MU_MIMO
	{
		HQA_TXMU_CMDS,
		sizeof(HQA_TXMU_CMDS) / sizeof(HQA_CMD_HANDLER),
		0x1560,
	}
	,
#endif
#endif
	{
		HQA_ICAP_CMDS,
		sizeof(HQA_ICAP_CMDS) / sizeof(HQA_CMD_HANDLER),
		0x1580,
	}
	,
	{
	 HQA_ReCal_CMDS,
	 sizeof(HQA_ReCal_CMDS) / sizeof(HQA_CMD_HANDLER),
	 0x1581,
	 }
	,
	{
	 HQA_RXV_CMDS,
	 sizeof(HQA_RXV_CMDS) / sizeof(HQA_CMD_HANDLER),
	 0x1582,
	 }
	,
	{
		HQA_CMD_SET6,
		sizeof(HQA_CMD_SET6) / sizeof(HQA_CMD_HANDLER),
		0x1600,
	}
	,
};

/*----------------------------------------------------------------------------*/
/*!
 * \brief  QA Agent For Handle Ethernet command by Command Idx.
 *
 * \param[in] prNetDev		Pointer to the Net Device
 * \param[in] prIwReqData
 * \param[in] HqaCmdFrame	Ethernet Frame Format receive from QA Tool DLL
 * \param[out] None
 *
 * \retval 0			On success.
 */
/*----------------------------------------------------------------------------*/
int HQA_CMDHandler(struct net_device *prNetDev,
		   IN union iwreq_data *prIwReqData,
		   struct HQA_CMD_FRAME *HqaCmdFrame)
{
	int32_t i4Status = 0;
	uint32_t u4CmdId;
	uint32_t u4TableIndex = 0;

	u4CmdId = ntohs(HqaCmdFrame->Id);

	while (u4TableIndex < (sizeof(HQA_CMD_TABLES) / sizeof(
				       struct HQA_CMD_TABLE))) {
		int CmdIndex = 0;

		CmdIndex = u4CmdId - HQA_CMD_TABLES[u4TableIndex].CmdOffset;
		if ((CmdIndex >= 0)
		    && (CmdIndex < HQA_CMD_TABLES[u4TableIndex].CmdSetSize)) {
			HQA_CMD_HANDLER *pCmdSet;

			pCmdSet = HQA_CMD_TABLES[u4TableIndex].CmdSet;

			if (pCmdSet[CmdIndex] != NULL)
				i4Status = (*pCmdSet[CmdIndex])(prNetDev,
						prIwReqData, HqaCmdFrame);
			break;
		}
		u4TableIndex++;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Ioctl entry from ATE Daemon.
 *
 * \param[in] prNetDev				Pointer to the Net Device
 * \param[in] prIwReqInfo
 * \param[in] prIwReqData
 * \param[in] pcExtra
 * \param[out] None
 *
 * \retval 0						On success.
 */
/*----------------------------------------------------------------------------*/
int priv_qa_agent(IN struct net_device *prNetDev,
		  IN struct iw_request_info *prIwReqInfo,
		  IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	int32_t i4Status = 0;
	struct HQA_CMD_FRAME *HqaCmdFrame;
	uint32_t u4ATEMagicNum, u4ATEId, u4ATEData;
	struct GLUE_INFO *prGlueInfo = NULL;
#if (CONFIG_WLAN_SERVICE == 1)
	struct hqa_frame_ctrl local_hqa;
#endif

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);

	/* workaroud for meta tool */
	if (prGlueInfo->prAdapter->fgTestMode == FALSE)
		MT_ATEStart(prNetDev, "ATESTART");

	if (!prIwReqData || prIwReqData->data.length == 0) {
		i4Status = -EINVAL;
		goto ERROR0;
	}

	HqaCmdFrame = kmalloc(sizeof(*HqaCmdFrame), GFP_KERNEL);

	if (!HqaCmdFrame) {
		i4Status = -ENOMEM;
		goto ERROR0;
	}

	memset(HqaCmdFrame, 0, sizeof(*HqaCmdFrame));
	if (copy_from_user(HqaCmdFrame, prIwReqData->data.pointer,
			   prIwReqData->data.length)) {
		i4Status = -EFAULT;
		goto ERROR1;
	}

	u4ATEMagicNum = ntohl(HqaCmdFrame->MagicNo);
	u4ATEId = ntohs(HqaCmdFrame->Id);
	memcpy((uint8_t *)&u4ATEData, HqaCmdFrame->Data, 4);
	u4ATEData = ntohl(u4ATEData);

	switch (u4ATEMagicNum) {
	case HQA_CMD_MAGIC_NO:
#if (CONFIG_WLAN_SERVICE == 1)
	{
		local_hqa.type = 0;
		local_hqa.hqa_frame_comm.hqa_frame_eth =
		(struct hqa_frame *)HqaCmdFrame;

		i4Status = mt_agent_hqa_cmd_handler(&prGlueInfo->rService,
			(struct hqa_frame_ctrl *)&local_hqa);

		if (i4Status == WLAN_STATUS_SUCCESS) {
			/*Response to QA */
			prIwReqData->data.length
				= sizeof((HqaCmdFrame)->MagicNo)
				+ sizeof((HqaCmdFrame)->Type)
				+ sizeof((HqaCmdFrame)->Id)
				+ sizeof((HqaCmdFrame)->Length)
				+ sizeof((HqaCmdFrame)->Sequence)
				+ ntohs((HqaCmdFrame)->Length);

			if (copy_to_user(prIwReqData->data.pointer
				, (uint8_t *) (HqaCmdFrame)
				, prIwReqData->data.length)) {
				DBGLOG(RFTEST, INFO
					, "QA_AGENT copy_to_user() fail in %s\n"
					, __func__);
				goto ERROR1;
			}
			DBGLOG(RFTEST, INFO,
			 "QA_AGENT HQA cmd(0x%04x)Magic num(0x%08x) is done\n",
			 ntohs(HqaCmdFrame->Id),
			 ntohl(HqaCmdFrame->MagicNo));
		}
	}
#else
		i4Status = HQA_CMDHandler(prNetDev, prIwReqData, HqaCmdFrame);
#endif
		break;
	default:
		i4Status = -EINVAL;
		DBGLOG(RFTEST, INFO, "QA_AGENT ATEMagicNum Error!!!\n");
		break;
	}



ERROR1:
	kfree(HqaCmdFrame);
ERROR0:
	return i4Status;
}
#endif
