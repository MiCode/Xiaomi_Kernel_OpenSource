/******************************************************************************
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
 *****************************************************************************/
/*
	Module Name:
	gl_ate_agent.c
*/
/*******************************************************************************
 *						C O M P I L E R	 F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *						E X T E R N A L	R E F E R E N C E S
 ********************************************************************************
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

/*******************************************************************************
*						C O N S T A N T S
********************************************************************************
*/

PARAM_RX_STAT_T g_HqaRxStat;
UINT_32 u4RxStatSeqNum;
BOOLEAN g_DBDCEnable = FALSE;
/* For SA Buffer Mode Temp Solution */
BOOLEAN	g_BufferDownload = FALSE;
UINT_32	u4EepromMode = 4;
UINT_32 g_u4Chip_ID;
UINT_8 g_ucEepromCurrentMode = EFUSE_MODE;

#if CFG_SUPPORT_BUFFER_MODE
UINT_8	uacEEPROMImage[MAX_EEPROM_BUFFER_SIZE] = {
	/* 0x000 ~ 0x00F */
	0xAE, 0x86, 0x06, 0x00, 0x18, 0x0D, 0x00, 0x00, 0xC0, 0x1F, 0xBD, 0x81, 0x3F, 0x01, 0x19, 0x00,
	/* 0x010 ~ 0x01F */
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
	/* 0x020 ~ 0x02F */
	0x80, 0x02, 0x00, 0x00, 0x32, 0x66, 0xC3, 0x14, 0x32, 0x66, 0xC3, 0x14, 0x03, 0x22, 0xFF, 0xFF,
	/* 0x030 ~ 0x03F */
	0x23, 0x04, 0x0D, 0xF2, 0x8F, 0x02, 0x00, 0x80, 0x0A, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x040 ~ 0x04F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x40, 0x00, 0x00,
	/* 0x050 ~ 0x05F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x08,
	/* 0x060 ~ 0x06F */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x08, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x08,
	/* 0x070 ~ 0x07F */
	0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x080 ~ 0x08F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x090 ~ 0x09F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0A0 ~ 0x0AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0B0 ~ 0x0BF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0x10, 0x10, 0x28, 0x00, 0x00, 0x00, 0x00,
	/* 0x0C0 ~ 0x0CF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0D0 ~ 0x0DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0E0 ~ 0x0EF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x0F0 ~ 0x0FF */
	0x0E, 0x05, 0x06, 0x06, 0x06, 0x0F, 0x00, 0x00, 0x0E, 0x05, 0x06, 0x05, 0x05, 0x09, 0xFF, 0x00,
	/* 0x100 ~ 0x10F */
	0x12, 0x34, 0x56, 0x78, 0x2C, 0x2C, 0x28, 0x28, 0x28, 0x26, 0x26, 0x28, 0x28, 0x28, 0x26, 0xFF,
	/* 0x110 ~ 0x11F */
	0x26, 0x25, 0x28, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x27, 0x27, 0x27, 0x25,
	/* 0x120 ~ 0x12F */
	0x25, 0x25, 0x25, 0x25, 0x23, 0x23, 0x23, 0x21, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00,
	/* 0x130 ~ 0x13F */
	0x40, 0x40, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	/* 0x140 ~ 0x14F */
	0x25, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x150 ~ 0x15F */
	0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0,
	/* 0x160 ~ 0x16F */
	0xD0, 0xD0, 0xD0, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x170 ~ 0x17F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC2, 0xC4, 0xC5, 0xC8,
	/* 0x180 ~ 0x18F */
	0x00, 0x26, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x190 ~ 0x19F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x1A0 ~ 0x1AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0xD0, 0xD0, 0x0E, 0x05, 0x06, 0x05, 0x09, 0x0E, 0x00,
	/* 0x1B0 ~ 0x1BF */
	0x05, 0x06, 0x05, 0x05, 0x09, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	/* 0x1C0 ~ 0x1CF */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00,
	/* 0x1D0 ~ 0x1DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x1E0 ~ 0x1EF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x1F0 ~ 0x1FF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x200 ~ 0x20F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x210 ~ 0x21F */
	0x48, 0xF5, 0x27, 0x49, 0x48, 0xF5, 0x57, 0x12, 0x4B, 0x71, 0x80, 0x50, 0x91, 0xF6, 0x87, 0x50,
	/* 0x220 ~ 0x22F */
	0x7D, 0x29, 0x09, 0x42, 0x7D, 0x29, 0x41, 0x44, 0x7D, 0x29, 0x41, 0x3C, 0x7D, 0x29, 0x31, 0x4D,
	/* 0x230 ~ 0x23F */
	0x49, 0x71, 0x24, 0x49, 0x49, 0x71, 0x54, 0x12, 0x4B, 0x71, 0x80, 0x50, 0x91, 0xF6, 0x87, 0x50,
	/* 0x240 ~ 0x24F */
	0x7D, 0x29, 0x09, 0x42, 0x7D, 0x29, 0x41, 0x04, 0x7D, 0x29, 0x41, 0x04, 0x7D, 0x29, 0x01, 0x40,
	/* 0x250 ~ 0x25F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x260 ~ 0x26F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x270 ~ 0x27F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x280 ~ 0x28F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x290 ~ 0x29F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2A0 ~ 0x2AF */
	0x7D, 0x29, 0xC9, 0x16, 0x7D, 0x29, 0xC9, 0x16, 0x44, 0x22, 0x32, 0x15, 0xEE, 0xEE, 0xEE, 0x08,
	/* 0x2B0 ~ 0x2BF */
	0x78, 0x90, 0x79, 0x1C, 0x78, 0x90, 0x79, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2C0 ~ 0x2CF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2D0 ~ 0x2DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2E0 ~ 0x2EF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x2F0 ~ 0x2FF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x300 ~ 0x30F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x310 ~ 0x31F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x42, 0x10, 0x42, 0x08, 0x21,
	/* 0x320 ~ 0x32F */
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21,
	/* 0x330 ~ 0x33F */
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21,
	/* 0x340 ~ 0x34F */
	0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x21, 0x10, 0x42, 0x08, 0x01,
	/* 0x350 ~ 0x35F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x360 ~ 0x36F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x370 ~ 0x37F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x380 ~ 0x38F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x390 ~ 0x39F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3A0 ~ 0x3AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3B0 ~ 0x3BF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3C0 ~ 0x3CF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3D0 ~ 0x3DF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x3E0 ~ 0x3EF */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	/* 0x3F0 ~ 0x3FF */
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	/* 0x400 ~ 0x40F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x410 ~ 0x41F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x420 ~ 0x42F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x430 ~ 0x43F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x440 ~ 0x44F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x450 ~ 0x45F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x460 ~ 0x46F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x470 ~ 0x47F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x480 ~ 0x48F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x490 ~ 0x49F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x4A0 ~ 0x4AF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Response ACK to QA Tool DLL.
*
* \param[in] HqaCmdFrame		Ethernet Frame Format respond to QA Tool DLL
* \param[in] prIwReqData
* \param[in] i4Length			Length of Ethernet Frame data field
* \param[in] i4Status			Status to respond
* \param[out] None
*
* \retval 0					On success.
* \retval -EFAULT				If copy_to_user fail
*/
/*----------------------------------------------------------------------------*/
static INT_32 ResponseToQA(HQA_CMD_FRAME *HqaCmdFrame,
			   IN union iwreq_data *prIwReqData, INT_32 i4Length, INT_32 i4Status)
{
	HqaCmdFrame->Length = ntohs((i4Length));

	i4Status = ntohs((i4Status));
	memcpy(HqaCmdFrame->Data, &i4Status, 2);

	prIwReqData->data.length = sizeof((HqaCmdFrame)->MagicNo) + sizeof((HqaCmdFrame)->Type)
	    + sizeof((HqaCmdFrame)->Id) + sizeof((HqaCmdFrame)->Length)
	    + sizeof((HqaCmdFrame)->Sequence) + ntohs((HqaCmdFrame)->Length);

	if (copy_to_user(prIwReqData->data.pointer, (UCHAR *) (HqaCmdFrame), prIwReqData->data.length)) {
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT copy_to_user() fail in %s\n", __func__);
		return -EFAULT;
	}
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA command(0x%04x)[Magic number(0x%08x)] is done\n",
	       ntohs(HqaCmdFrame->Id), ntohl(HqaCmdFrame->MagicNo));

	return 0;
}

static INT_32 ToDoFunction(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT ToDoFunction\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Open Adapter (called when QA Tool UI Open).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_OpenAdapter(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_OpenAdapter\n");

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CloseAdapter(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CloseAdapter\n");

	i4Ret = MT_ATEStop(prNetDev, "ATESTOP");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Start TX.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StartTx(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 TxCount;
	UINT_16 TxLength;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartTx\n");

	memcpy((PUCHAR)&TxCount, HqaCmdFrame->Data + 4 * 0, 4);
	TxCount = ntohl(TxCount);
	memcpy((PUCHAR)&TxLength, HqaCmdFrame->Data + 4 * 1, 2);
	TxLength = ntohs(TxLength);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartTx TxCount = %d\n", TxCount);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartTx TxLength = %d\n", TxLength);

	i4Ret = MT_ATESetTxCount(prNetDev, TxCount);
	i4Ret = MT_ATESetTxLength(prNetDev, (UINT_32) TxLength);
	i4Ret = MT_ATEStartTX(prNetDev, "TXFRAME");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
/* 1 todo not support yet */
static INT_32 HQA_StartTxExt(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartTxExt\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Start Continuous TX.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
/* 1 todo not support yet */
static INT_32 HQA_StartTxContiTx(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartTxContiTx\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
/* 1 todo not support yets */
static INT_32 HQA_StartTxCarrier(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartTxCarrier\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Start RX (Legacy function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StartRx(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartRx\n");

	MT_ATESetDBDCBandIndex(prNetDev, 0);
	MT_ATEStartRX(prNetDev, "RXFRAME");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Stop TX (Legacy function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StopTx(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StopTx\n");

	MT_ATESetDBDCBandIndex(prNetDev, 0);
	MT_ATEStopRX(prNetDev, "RXSTOP");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Stop Continuous TX.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StopContiTx(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StopContiTx\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StopTxCarrier(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StopTxCarrier\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Stop RX (Legacy function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StopRx(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StopRx\n");

	MT_ATESetDBDCBandIndex(prNetDev, 0);
	MT_ATEStopRX(prNetDev, "RXSTOP");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set TX Path.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetTxPath(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxPath\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set RX Path.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetRxPath(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
/*	INT_16 Value = 0;
*	P_GLUE_INFO_T prGlueInfo = NULL;
*	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
*	UINT_32 u4BufLen = 0;
*/

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRxPath\n");

#if 0
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&Value, HqaCmdFrame->Data + 4 * 0, 2);
	Value = ntohs(Value);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRxPath Value : %d\n", Value);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RX_PATH;
	rRfATInfo.u4FuncData = (UINT_32) ((Value << 16) || (0 & BITS(0, 15)));

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
#endif

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set TX Inter-Packet Guard.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetTxIPG(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Aifs = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxIPG\n");

	memcpy(&u4Aifs, HqaCmdFrame->Data + 4 * 0, 4);
	u4Aifs = ntohs(u4Aifs);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxIPG u4Aifs : %d\n", u4Aifs);

	MT_ATESetTxIPG(prNetDev, u4Aifs);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set TX Power0 (Legacy Function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetTxPower0(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxPower0\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set TX Power1.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HAQ_SetTxPower1(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HAQ_SetTxPower1\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetTxPowerExt(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Power = 0;
	UINT_32 u4Channel = 0;
	UINT_32 u4Dbdc_idx = 0;
	UINT_32 u4Band_idx = 0;
	UINT_32 u4Ant_idx = 0;

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxPowerExt u4Power : %d\n", u4Power);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxPowerExt u4Dbdc_idx : %d\n", u4Dbdc_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxPowerExt u4Channel : %d\n", u4Channel);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxPowerExt u4Band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxPowerExt u4Ant_idx : %d\n", u4Ant_idx);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetOnOff(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetOnOff\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Antenna Selection.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_AntennaSel(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_AntennaSel\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_FWPacketCMD_ClockSwitchDisable(struct net_device *prNetDev,
						 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_FWPacketCMD_ClockSwitchDisable\n");

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetChannel(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 i4SetFreq = 0, i4SetChan = 0;
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetChannel\n");

	memcpy((PUCHAR)&i4SetChan, HqaCmdFrame->Data, 4);
	i4SetChan = ntohl(i4SetChan);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetChannel Channel = %d\n", i4SetChan);

	i4SetFreq = nicChannelNum2Freq(i4SetChan);
	i4Ret = MT_ATESetChannel(prNetDev, 0, i4SetFreq);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set Preamble (Legacy Function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetPreamble(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Mode = 0;
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetPreamble\n");

	memcpy((PUCHAR)&i4Mode, HqaCmdFrame->Data, 4);
	i4Mode = ntohl(i4Mode);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetPreamble Mode = %d\n", i4Mode);

	i4Ret = MT_ATESetPreamble(prNetDev, i4Mode);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set Rate (Legacy Function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetRate(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
/*	INT_32 i4Value = 0; */
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRate\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set Nss.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetNss(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetNss\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set System BW (Legacy Function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetSystemBW(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 i4BW;
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetSystemBW\n");

	memcpy((PUCHAR)&i4BW, HqaCmdFrame->Data, 4);
	i4BW = ntohl(i4BW);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetSystemBW BW = %d\n", i4BW);

	i4Ret = MT_ATESetSystemBW(prNetDev, i4BW);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set Data BW (Legacy Function).
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetPerPktBW(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Perpkt_bw;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetPerPktBW\n");

	memcpy((PUCHAR)&u4Perpkt_bw, HqaCmdFrame->Data, 4);
	u4Perpkt_bw = ntohl(u4Perpkt_bw);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetPerPktBW u4Perpkt_bw = %d\n", u4Perpkt_bw);

	i4Ret = MT_ATESetPerPacketBW(prNetDev, u4Perpkt_bw);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set Primary BW.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetPrimaryBW(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32               u4Pri_sel = 0;

	memcpy(&u4Pri_sel, HqaCmdFrame->Data, 4);
	u4Pri_sel = ntohl(u4Pri_sel);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetPrimaryBW u4Pri_sel : %d\n", u4Pri_sel);

	i4Ret = MT_ATEPrimarySetting(prNetDev, u4Pri_sel);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set Frequency Offset.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetFreqOffset(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4FreqOffset = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&u4FreqOffset, HqaCmdFrame->Data, 4);
	u4FreqOffset = ntohl(u4FreqOffset);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetFreqOffset u4FreqOffset : %d\n", u4FreqOffset);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_FRWQ_OFFSET;
	rRfATInfo.u4FuncData = (UINT_32) u4FreqOffset;

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetAutoResponder(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetAutoResponder\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetTssiOnOff(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTssiOnOff\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
/* 1 todo not support yet */

static INT_32 HQA_SetRxHighLowTemperatureCompensation(struct net_device *prNetDev,
						      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRxHighLowTemperatureCompensation\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_LowPower(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_LowPower\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

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
};

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Reset TRX Counter
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_ResetTxRxCounter(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 i4Status;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_ResetTxRxCounter\n");

	i4Status = MT_ATEResetTXRXCounter(prNetDev);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetStatistics(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetStatistics\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetRxOKData(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetRxOKData\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetRxOKOther(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetRxOKOther\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetRxAllPktCount(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetRxAllPktCount\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetTxTransmitted(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetTxTransmitted\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetHwCounter(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetHwCounter\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CalibrationOperation(struct net_device *prNetDev,
				       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CalibrationOperation\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CalibrationBypassExt(struct net_device *prNetDev,
				       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Status = 0;
	UINT_32 u4Item = 0;
	UINT_32 u4Band_idx = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&u4Item, HqaCmdFrame->Data, 4);
	u4Item = ntohl(u4Item);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CalibrationBypassExt u4Item : 0x%08x\n", u4Item);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CalibrationBypassExt u4Band_idx : %d\n", u4Band_idx);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetRXVectorIdx(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 band_idx = 0;
	UINT_32 Group_1 = 0, Group_2 = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	band_idx = ntohl(band_idx);
	memcpy(&Group_1, HqaCmdFrame->Data + 4 * 1, 4);
	Group_1 = ntohl(Group_1);
	memcpy(&Group_2, HqaCmdFrame->Data + 4 * 2, 4);
	Group_2 = ntohl(Group_2);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXVectorIdx band_idx : %d\n", band_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXVectorIdx Group_1 : %d\n", Group_1);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXVectorIdx Group_2 : %d\n", Group_2);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RXV_INDEX;
	rRfATInfo.u4FuncData = (UINT_32) (Group_1);
	rRfATInfo.u4FuncData |= (UINT_32) (Group_2 << 8);
	rRfATInfo.u4FuncData |= (UINT_32) (band_idx << 16);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXVectorIdx rRfATInfo.u4FuncData : 0x%08x\n",
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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetFAGCRssiPath(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4band_idx = 0;
	UINT_32 u4FAGC_Path = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&u4band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4band_idx = ntohl(u4band_idx);
	memcpy(&u4FAGC_Path, HqaCmdFrame->Data + 4 * 1, 4);
	u4FAGC_Path = ntohl(u4FAGC_Path);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetFAGCRssiPath u4band_idx : %d\n", u4band_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetFAGCRssiPath u4FAGC_Path : %d\n", u4FAGC_Path);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_FAGC_RSSI_PATH;
	rRfATInfo.u4FuncData = (UINT_32) ((u4band_idx << 16) || (u4FAGC_Path & BITS(0, 15)));

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MacBbpRegRead(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 u4Offset, u4Value;
	INT_32 i4Status;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MacBbpRegRead\n");

	memcpy(&u4Offset, HqaCmdFrame->Data, 4);
	u4Offset = ntohl(u4Offset);

	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_MacBbpRegRead Offset = 0x%08x\n",
	       u4Offset);

	rMcrInfo.u4McrOffset = u4Offset;
	rMcrInfo.u4McrData = 0;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo, wlanoidQueryMcrRead, &rMcrInfo, sizeof(rMcrInfo), TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Value = rMcrInfo.u4McrData;

		DBGLOG(RFTEST, INFO,
		       "MT6632 : QA_AGENT Address = 0x%08x, Result = 0x%08x\n",
		       u4Offset, u4Value);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MacBbpRegWrite(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{

/*	INT_32 i4Ret = 0; */
	UINT_32 u4Offset, u4Value;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MacBbpRegWrite\n");

	memcpy(&u4Offset, HqaCmdFrame->Data, 4);
	memcpy(&u4Value, HqaCmdFrame->Data + 4, 4);

	u4Offset = ntohl(u4Offset);
	u4Value = ntohl(u4Value);

	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_MacBbpRegWrite Offset = 0x%08x\n",
	       u4Offset);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_MacBbpRegWrite Value = 0x%08x\n",
	       u4Value);

	rMcrInfo.u4McrOffset = u4Offset;
	rMcrInfo.u4McrData = u4Value;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo, wlanoidSetMcrWrite, &rMcrInfo, sizeof(rMcrInfo), FALSE, FALSE, TRUE, &u4BufLen);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Read Bulk MAC CR.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MACBbpRegBulkRead(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 u4Index, u4Offset, u4Value;
	UINT_16 u2Len;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MACBbpRegBulkRead\n");

	memcpy(&u4Offset, HqaCmdFrame->Data, 4);
	u4Offset = ntohl(u4Offset);
	memcpy(&u2Len, HqaCmdFrame->Data + 4, 2);
	u2Len = ntohs(u2Len);

	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MACBbpRegBulkRead Offset = 0x%08x\n",
	       u4Offset);
	DBGLOG(RFTEST, INFO,
	       "QA_AGENT HQA_MACBbpRegBulkRead Len = 0x%08x\n",
	       u2Len);

	for (u4Index = 0; u4Index < u2Len; u4Index++) {
		rMcrInfo.u4McrOffset = u4Offset + u4Index * 4;
		rMcrInfo.u4McrData = 0;
		prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidQueryMcrRead, &rMcrInfo, sizeof(rMcrInfo), TRUE, TRUE, TRUE, &u4BufLen);

		if (i4Status == 0) {
			u4Value = rMcrInfo.u4McrData;

			DBGLOG(RFTEST, INFO,
			       "QA_AGENT Address = 0x%08x, Result = 0x%08x\n",
			       u4Offset + u4Index * 4, u4Value);

			u4Value = ntohl(u4Value);
			memcpy(HqaCmdFrame->Data + 2 + (u4Index * 4), &u4Value, 4);
		}
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + (u2Len * 4), i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Read Bulk RF CR.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_RfRegBulkRead(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 u4Index, u4WfSel, u4Offset, u4Length, u4Value;
	INT_32 i4Status = 0;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RfRegBulkRead\n");

	memcpy(&u4WfSel, HqaCmdFrame->Data, 4);
	u4WfSel = ntohl(u4WfSel);
	memcpy(&u4Offset, HqaCmdFrame->Data + 4, 4);
	u4Offset = ntohl(u4Offset);
	memcpy(&u4Length, HqaCmdFrame->Data + 8, 4);
	u4Length = ntohl(u4Length);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RfRegBulkRead WfSel  = %d\n", u4WfSel);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_RfRegBulkRead Offset = 0x%08x\n",
	       u4Offset);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RfRegBulkRead Length = %d\n", u4Length);

	if (u4WfSel == 0)
		u4Offset = u4Offset | 0x99900000;
	else if (u4WfSel == 1)
		u4Offset = u4Offset | 0x99910000;


	for (u4Index = 0; u4Index < u4Length; u4Index++) {
		rMcrInfo.u4McrOffset = u4Offset + u4Index * 4;
		rMcrInfo.u4McrData = 0;
		prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidQueryMcrRead, &rMcrInfo, sizeof(rMcrInfo), TRUE, TRUE, TRUE, &u4BufLen);

		if (i4Status == 0) {
			u4Value = rMcrInfo.u4McrData;

			DBGLOG(RFTEST, INFO,
			       "QA_AGENT Address = 0x%08x, Result = 0x%08x\n",
			       u4Offset + u4Index * 4, u4Value);

			u4Value = ntohl(u4Value);
			memcpy(HqaCmdFrame->Data + 2 + (u4Index * 4), &u4Value, 4);
		}
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + (u4Length * 4), i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Write RF CR.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_RfRegBulkWrite(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 u4WfSel, u4Offset, u4Length, u4Value;
	INT_32 i4Status;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RfRegBulkWrite\n");

	memcpy(&u4WfSel, HqaCmdFrame->Data, 4);
	u4WfSel = ntohl(u4WfSel);
	memcpy(&u4Offset, HqaCmdFrame->Data + 4, 4);
	u4Offset = ntohl(u4Offset);
	memcpy(&u4Length, HqaCmdFrame->Data + 8, 4);
	u4Length = ntohl(u4Length);
	memcpy(&u4Value, HqaCmdFrame->Data + 12, 4);
	u4Value = ntohl(u4Value);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RfRegBulkWrite WfSel  = %d\n", u4WfSel);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_RfRegBulkWrite Offset = 0x%08x\n",
	       u4Offset);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RfRegBulkWrite Length = %d\n", u4Length);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_RfRegBulkWrite Value  = 0x%08x\n",
	       u4Value);

	if (u4WfSel == 0)
		u4Offset = u4Offset | 0x99900000;
	else if (u4WfSel == 1)
		u4Offset = u4Offset | 0x99910000;


	rMcrInfo.u4McrOffset = u4Offset;
	rMcrInfo.u4McrData = u4Value;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo, wlanoidSetMcrWrite, &rMcrInfo, sizeof(rMcrInfo), FALSE, FALSE, TRUE, &u4BufLen);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_ReadEEPROM(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{

	UINT_16 Offset;
	UINT_16 Len;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	UINT_32 u4BufLen = 0;
	UINT_8  u4Index = 0;
	UINT_16  u4Value = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_ACCESS_EFUSE_T rAccessEfuseInfo;
#endif

	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadEEPROM\n");

	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&Len, HqaCmdFrame->Data + 2 * 1, 2);
	Len = ntohs(Len);

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	kalMemSet(&rAccessEfuseInfo, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));


	rAccessEfuseInfo.u4Address = (Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;


	rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryProcessAccessEfuseRead,
				&rAccessEfuseInfo,
				sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), TRUE, TRUE, TRUE, &u4BufLen);

	u4Index = Offset % EFUSE_BLOCK_SIZE;
	if (u4Index <= 14)
		u4Value = (prGlueInfo->prAdapter->aucEepromVaule[u4Index])
					| (prGlueInfo->prAdapter->aucEepromVaule[u4Index+1] << 8);


    /* isVaild = pResult->u4Valid; */

	if (rStatus == WLAN_STATUS_SUCCESS) {

		DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadEEPROM u4Value = %x\n", u4Value);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_WriteEEPROM(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;


#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	UINT_16  u4WriteData = 0;
	UINT_32 u4BufLen = 0;
	UINT_8  u4Index = 0;
	UINT_16 Offset;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_ACCESS_EFUSE_T rAccessEfuseInfoWrite;

	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&u4WriteData, HqaCmdFrame->Data + 2 * 1, 2);
	u4WriteData = ntohs(u4WriteData);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
#if 0
    /* Read */
	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadEEPROM\n");
	kalMemSet(&rAccessEfuseInfoRead, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
	rAccessEfuseInfoRead.u4Address = (Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
	rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryProcessAccessEfuseRead,
				&rAccessEfuseInfoRead,
				sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), FALSE, FALSE, TRUE, &u4BufLen);
#endif

    /* Write */
	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_WriteEEPROM\n");
	kalMemSet(&rAccessEfuseInfoWrite, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
	u4Index = Offset % EFUSE_BLOCK_SIZE;

	if (prGlueInfo->prAdapter->rWifiVar.ucEfuseBufferModeCal
		== LOAD_EEPROM_BIN) {
		uacEEPROMImage[Offset] = u4WriteData & 0xff;
		uacEEPROMImage[Offset + 1] = u4WriteData >> 8 & 0xff;
	} else {

		if (u4Index >= EFUSE_BLOCK_SIZE - 1) {
			DBGLOG(INIT, ERROR, "MT6632 : efuse Offset error\n");
			return -EINVAL;
		}

		prGlueInfo->prAdapter->aucEepromVaule[u4Index] = u4WriteData & 0xff; /* Note: u4WriteData is UINT_16 */
		prGlueInfo->prAdapter->aucEepromVaule[u4Index+1] = u4WriteData >> 8 & 0xff;

		kalMemCopy(rAccessEfuseInfoWrite.aucData, prGlueInfo->prAdapter->aucEepromVaule, 16);
		rAccessEfuseInfoWrite.u4Address = (Offset / EFUSE_BLOCK_SIZE)*EFUSE_BLOCK_SIZE;

		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryProcessAccessEfuseWrite,
					&rAccessEfuseInfoWrite,
					sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), FALSE, FALSE, TRUE, &u4BufLen);
	}
#endif

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_ReadBulkEEPROM(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_16 Offset;
	UINT_16 Len;
#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	PARAM_CUSTOM_ACCESS_EFUSE_T rAccessEfuseInfo;
	UINT_32 u4BufLen = 0;
	UINT_8  u4Loop = 0;

	UINT_16 Buffer;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_8 tmp = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	kalMemSet(&rAccessEfuseInfo, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
#endif

	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadBulkEEPROM\n");

	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&Len, HqaCmdFrame->Data + 2 * 1, 2);
	Len = ntohs(Len);
	tmp = Offset;
	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadBulkEEPROM Offset : %d\n", Offset);
	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadBulkEEPROM Len : %d\n", Len);

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	rAccessEfuseInfo.u4Address = (Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;

	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadBulkEEPROM Address : %d\n", rAccessEfuseInfo.u4Address);

	if	((g_ucEepromCurrentMode == EFUSE_MODE)
		&& (prGlueInfo->prAdapter->fgIsSupportQAAccessEfuse == TRUE)) {

		/* Read from Efuse */
		DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadBulkEEPROM Efuse Mode\n");
		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryProcessAccessEfuseRead,
					&rAccessEfuseInfo,
					sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadBulkEEPROM kal fail\n");

		Offset = Offset % EFUSE_BLOCK_SIZE;

#if 0
		for (u4Loop = 0; u4Loop < 16; u4Loop++) {
			DBGLOG(INIT, INFO, "MT6632:QA_AGENT HQA_ReadBulkEEPROM Efuse Offset=%x u4Loop=%d u4Value=%x\n",
			Offset, u4Loop, prGlueInfo->prAdapter->aucEepromVaule[u4Loop]);
		}
#endif
		for (u4Loop = 0; u4Loop < Len; u4Loop += 2) {
			memcpy(&Buffer, prGlueInfo->prAdapter->aucEepromVaule + Offset + u4Loop, 2);
			Buffer = ntohs(Buffer);
			DBGLOG(INIT, INFO, "MT6632 :From Efuse  u4Loop=%d  Buffer=%x\n", u4Loop, Buffer);
			memcpy(HqaCmdFrame->Data + 2 + u4Loop, &Buffer, 2);
		}

	} else {  /* Read from EEPROM */
		for (u4Loop = 0; u4Loop < Len; u4Loop += 2) {
			memcpy(&Buffer, uacEEPROMImage + Offset + u4Loop, 2);
			Buffer = ntohs(Buffer);
			memcpy(HqaCmdFrame->Data + 2 + u4Loop, &Buffer, 2);
			DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_ReadBulkEEPROM u4Loop=%d  u4Value=%x\n",
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

		UINT_16 u2InitAddr = 0x000;
		UINT_32 i = 0, j = 0;
		UINT_32 u4BufLen = 0;
		WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
		P_GLUE_INFO_T prGlueInfo = NULL;
		PARAM_CUSTOM_EFUSE_BUFFER_MODE_T rSetEfuseBufModeInfo;

		prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

		for (i = 0 ; i < MAX_EEPROM_BUFFER_SIZE/16 ; i++) {
			for (j = 0 ; j < 16 ; j++) {
				rSetEfuseBufModeInfo.aBinContent[j].u2Addr = u2InitAddr;
				rSetEfuseBufModeInfo.aBinContent[j].ucValue = uacEEPROMImage[u2InitAddr];
				u2InitAddr += 1;
			}

			rSetEfuseBufModeInfo.ucSourceMode = 1;
			rSetEfuseBufModeInfo.ucCount = EFUSE_CONTENT_SIZE;
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetEfusBufferMode,
					   &rSetEfuseBufModeInfo,
					   sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T), FALSE, FALSE, TRUE, &u4BufLen);
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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_WriteBulkEEPROM(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_16 Offset;
	UINT_16 Len;
	P_ADAPTER_T prAdapter = NULL;

	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_ACCESS_EFUSE_T rAccessEfuseInfoRead, rAccessEfuseInfoWrite;
	UINT_16 testBuffer1, testBuffer2, testBuffer;
	UINT_16	*Buffer = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_8  u4Loop = 0, u4Index = 0;
	UINT_16 ucTemp2;
	UINT_16 i = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemSet(&rAccessEfuseInfoRead, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
	kalMemSet(&rAccessEfuseInfoWrite, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));

	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_WriteBulkEEPROM\n");


	memcpy(&Offset, HqaCmdFrame->Data + 2 * 0, 2);
	Offset = ntohs(Offset);
	memcpy(&Len, HqaCmdFrame->Data + 2 * 1, 2);
	Len = ntohs(Len);

	memcpy(&testBuffer1, HqaCmdFrame->Data + 2 * 2, Len);
	testBuffer2 = ntohs(testBuffer1);
	testBuffer = ntohs(testBuffer1);


	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_WriteBulkEEPROM Offset : %x\n", Offset);
	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_WriteBulkEEPROM Len : %d\n", Len);


	/* Support Delay Calibraiton */
	if (prGlueInfo->prAdapter->fgIsSupportQAAccessEfuse == TRUE) {

		Buffer = kmalloc(sizeof(UINT_8)*(EFUSE_BLOCK_SIZE), GFP_KERNEL);
		if (!Buffer)
			return -ENOMEM;
		kalMemSet(Buffer, 0, sizeof(UINT_8)*(EFUSE_BLOCK_SIZE));

		kalMemCopy((UINT_8 *)Buffer, (UINT_8 *)HqaCmdFrame->Data + 4, Len);

#if 0
		for (u4Loop = 0; u4Loop < (Len)/2; u4Loop++) {

			DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_WriteBulkEEPROM u4Loop=%d  u4Value=%x\n",
				u4Loop, Buffer[u4Loop]);
		}
#endif

		if (g_ucEepromCurrentMode == BUFFER_BIN_MODE) {
			/* EEPROM */
			DBGLOG(INIT, INFO, "Direct EEPROM buffer, offset=%x, len=%x\n", Offset, Len);
#if 0
			for (i = 0; i < EFUSE_BLOCK_SIZE; i++)
				memcpy(uacEEPROMImage + Offset + i, Buffer + i, 1);

#endif
			if (Len > 2) {
				for (u4Loop = 0; u4Loop < EFUSE_BLOCK_SIZE/2 ; u4Loop++) {
					Buffer[u4Loop] = ntohs(Buffer[u4Loop]);
					uacEEPROMImage[Offset] = Buffer[u4Loop] & 0xff;
					uacEEPROMImage[Offset + 1] = Buffer[u4Loop] >> 8 & 0xff;
					Offset += 2;
				}
			} else {
				*Buffer = ntohs(*Buffer);
				uacEEPROMImage[Offset] = *Buffer & 0xff;
				uacEEPROMImage[Offset + 1] = *Buffer >> 8 & 0xff;
			}
		} else if (g_ucEepromCurrentMode == EFUSE_MODE) {
			/* EFUSE */
			/* Read */
			DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_WriteBulkEEPROM  Read\n");
			kalMemSet(&rAccessEfuseInfoRead, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
			rAccessEfuseInfoRead.u4Address = (Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
			rStatus = kalIoctl(prGlueInfo,
						wlanoidQueryProcessAccessEfuseRead,
						&rAccessEfuseInfoRead,
						sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), TRUE, TRUE, TRUE, &u4BufLen);

			/* Write */
			kalMemSet(&rAccessEfuseInfoWrite, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));

			if (Len > 2) {
				for (u4Loop = 0; u4Loop < 8 ; u4Loop++)
					Buffer[u4Loop] = ntohs(Buffer[u4Loop]);
				memcpy(rAccessEfuseInfoWrite.aucData, Buffer, 16);
			} else {
				u4Index = Offset % EFUSE_BLOCK_SIZE;
				DBGLOG(INIT, INFO, "MT6632:QA_AGENT HQA_WriteBulkEEPROM Wr,u4Index=%x,Buffer=%x\n",
						u4Index, testBuffer);

				if (u4Index >= EFUSE_BLOCK_SIZE - 1) {
					DBGLOG(INIT, ERROR, "MT6632 : efuse Offset error\n");
					i4Ret = -EINVAL;
					goto exit;
				}


				*Buffer = ntohs(*Buffer);
				DBGLOG(INIT, INFO, "MT6632 : Buffer[0]=%x, Buffer[0]&0xff=%x\n"
					, Buffer[0], Buffer[0]&0xff);
				DBGLOG(INIT, INFO, "MT6632 : Buffer[0] >> 8 & 0xff=%x\n"
					, Buffer[0] >> 8 & 0xff);

				prGlueInfo->prAdapter->aucEepromVaule[u4Index] = *Buffer & 0xff;
				prGlueInfo->prAdapter->aucEepromVaule[u4Index+1] = *Buffer >> 8 & 0xff;
				kalMemCopy(rAccessEfuseInfoWrite.aucData, prGlueInfo->prAdapter->aucEepromVaule, 16);
			}

			rAccessEfuseInfoWrite.u4Address = (Offset / EFUSE_BLOCK_SIZE)*EFUSE_BLOCK_SIZE;
			for (u4Loop = 0; u4Loop < (EFUSE_BLOCK_SIZE); u4Loop++) {

				DBGLOG(INIT, INFO, "MT6632 :  Loop=%d  aucData=%x\n",
				u4Loop, rAccessEfuseInfoWrite.aucData[u4Loop]);
			}

			DBGLOG(INIT, INFO, "Going for e-Fuse\n");

			rStatus = kalIoctl(prGlueInfo,
							wlanoidQueryProcessAccessEfuseWrite,
							&rAccessEfuseInfoWrite,
							sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T),
							FALSE, TRUE, TRUE, &u4BufLen);
		} else {
			DBGLOG(INIT, INFO, "Invalid ID!!\n");
		}
	} else {

		if (Len == 2) {
			memcpy(&ucTemp2, HqaCmdFrame->Data + 2 * 2, 2);
			ucTemp2 = ntohs(ucTemp2);
			memcpy(uacEEPROMImage + Offset, &ucTemp2, Len);
		} else {
			for (i = 0 ; i < 8 ; i++) {
				memcpy(&ucTemp2, HqaCmdFrame->Data + 2 * 2 + 2*i, 2);
				ucTemp2 = ntohs(ucTemp2);
				memcpy(uacEEPROMImage + Offset + 2*i, &ucTemp2, 2);
			}

			if (!g_BufferDownload) {
				UINT_16 u2InitAddr = Offset;
				UINT_32 j = 0;
				UINT_32 u4BufLen = 0;
				WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
				P_GLUE_INFO_T prGlueInfo = NULL;
				PARAM_CUSTOM_EFUSE_BUFFER_MODE_T *prSetEfuseBufModeInfo = NULL;
				BIN_CONTENT_T *pBinContent;

				prSetEfuseBufModeInfo = (PARAM_CUSTOM_EFUSE_BUFFER_MODE_T *)
					kalMemAlloc(sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T), VIR_MEM_TYPE);
				if (prSetEfuseBufModeInfo == NULL)
					return 0;
				kalMemZero(prSetEfuseBufModeInfo, sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T));

				prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
				pBinContent = (BIN_CONTENT_T *)prSetEfuseBufModeInfo->aBinContent;

				for (j = 0 ; j < 16 ; j++) {
					pBinContent->u2Addr = u2InitAddr;
					pBinContent->ucValue = uacEEPROMImage[u2InitAddr];

					pBinContent++;
				}

				prSetEfuseBufModeInfo->ucSourceMode = 1;
				prSetEfuseBufModeInfo->ucCount = EFUSE_CONTENT_SIZE;
				rStatus = kalIoctl(prGlueInfo,
							wlanoidSetEfusBufferMode,
							(PVOID)prSetEfuseBufModeInfo,
							sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T),
							FALSE, FALSE, TRUE, &u4BufLen);

				kalMemFree(prSetEfuseBufModeInfo, VIR_MEM_TYPE,
							sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T));

				if (Offset == 0x4A0)
					g_BufferDownload = TRUE;
			}
		}
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + Len, i4Ret);

exit:
	kfree(Buffer);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CheckEfuseMode(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		Value = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CheckEfuseMode\n");

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetFreeEfuseBlock(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{

	INT_32 i4Ret = 0, u4FreeBlockCount = 0;

#if (CFG_EEPROM_PAGE_ACCESS == 1)
	PARAM_CUSTOM_EFUSE_FREE_BLOCK_T rEfuseFreeBlock;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, INFO, "MT6632 : QA_AGENT HQA_GetFreeEfuseBlock\n");

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	if (prGlueInfo->prAdapter->fgIsSupportGetFreeEfuseBlockCount == TRUE) {
		kalMemSet(&rEfuseFreeBlock, 0, sizeof(PARAM_CUSTOM_EFUSE_FREE_BLOCK_T));


		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryEfuseFreeBlock,
					&rEfuseFreeBlock,
					sizeof(PARAM_CUSTOM_EFUSE_FREE_BLOCK_T), TRUE, TRUE, TRUE, &u4BufLen);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetEfuseBlockNr(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetEfuseBlockNr\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_WriteEFuseFromBuffer(struct net_device *prNetDev,
				       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_WriteEFuseFromBuffer\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetTxPower(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Channel = 0, u4Band = 0, u4Ch_Band = 0, u4TxTargetPower = 0;
/*	UINT_32 u4EfuseAddr = 0, u4Power = 0; */

#if (CFG_EEPROM_PAGE_ACCESS == 1)
	PARAM_CUSTOM_GET_TX_POWER_T rGetTxPower;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
#endif

	memcpy(&u4Channel, HqaCmdFrame->Data + 4 * 0, 4);
	u4Channel = ntohl(u4Channel);
	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 1, 4);
	u4Band = ntohl(u4Band);
	memcpy(&u4Ch_Band, HqaCmdFrame->Data + 4 * 2, 4);
	u4Ch_Band = ntohl(u4Ch_Band);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetTxPower u4Channel : %d\n", u4Channel);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetTxPower u4Band : %d\n", u4Band);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetTxPower u4Ch_Band : %d\n", u4Ch_Band);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prGlueInfo->prAdapter->fgIsSupportGetTxPower == TRUE) {
		kalMemSet(&rGetTxPower, 0, sizeof(PARAM_CUSTOM_GET_TX_POWER_T));

		rGetTxPower.ucCenterChannel = u4Channel;
		rGetTxPower.ucBand = u4Band;
		rGetTxPower.ucDbdcIdx = u4Ch_Band;

		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryGetTxPower,
					&rGetTxPower,
					sizeof(PARAM_CUSTOM_GET_TX_POWER_T), TRUE, TRUE, TRUE, &u4BufLen);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetCfgOnOff(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 Type, Enable, Band;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&Type, HqaCmdFrame->Data + 4 * 0, 4);
	Type = ntohl(Type);
	memcpy(&Enable, HqaCmdFrame->Data + 4 * 1, 4);
	Enable = ntohl(Enable);
	memcpy(&Band, HqaCmdFrame->Data + 4 * 2, 4);
	Band = ntohl(Band);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetCfgOnOff Type : %d\n", Type);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetCfgOnOff Enable : %d\n", Enable);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetCfgOnOff Band : %d\n", Band);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TSSI;
	rRfATInfo.u4FuncData = 0;

	if (Band == 0 && Enable == 1)
		rRfATInfo.u4FuncData |= BIT(0);
	else if (Band == 1 && Enable == 1)
		rRfATInfo.u4FuncData |= BIT(1);

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

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetFreqOffset(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4FreqOffset = 0;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_GET_FREQ_OFFSET;
	rRfATInfo.u4FuncData = 0;

	i4Ret = kalIoctl(prGlueInfo,
			 wlanoidRftestQueryAutoTest, &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Ret == 0) {
		u4FreqOffset = rRfATInfo.u4FuncData;

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetFreqOffset u4FreqOffset = %d\n", u4FreqOffset);

		u4FreqOffset = ntohl(u4FreqOffset);
		memcpy(HqaCmdFrame->Data + 2, &u4FreqOffset, sizeof(u4FreqOffset));
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_DBDCTXTone(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	INT_32 i4BandIdx = 0, i4Control = 0, i4AntIndex = 0, i4ToneType = 0, i4ToneFreq = 0;
	INT_32 i4DcOffsetI = 0, i4DcOffsetQ = 0, i4Band = 0, i4RF_Power = 0, i4Digi_Power = 0;

	memcpy(&i4BandIdx, HqaCmdFrame->Data + 4 * 0, 4);	/* DBDC Band Index : Band0, Band1 */
	i4BandIdx = ntohl(i4BandIdx);
	memcpy(&i4Control, HqaCmdFrame->Data + 4 * 1, 4);	/* Control TX Tone Start and Stop */
	i4Control = ntohl(i4Control);
	memcpy(&i4AntIndex, HqaCmdFrame->Data + 4 * 2, 4);	/* Select TX Antenna */
	i4AntIndex = ntohl(i4AntIndex);
	memcpy(&i4ToneType, HqaCmdFrame->Data + 4 * 3, 4);	/* ToneType : Single or Two */
	i4ToneType = ntohl(i4ToneType);
	memcpy(&i4ToneFreq, HqaCmdFrame->Data + 4 * 4, 4);	/* ToneFreq: DC/5M/10M/20M/40M */
	i4ToneFreq = ntohl(i4ToneFreq);
	memcpy(&i4DcOffsetI, HqaCmdFrame->Data + 4 * 5, 4);	/* DC Offset I : -512~1535 */
	i4DcOffsetI = ntohl(i4DcOffsetI);
	memcpy(&i4DcOffsetQ, HqaCmdFrame->Data + 4 * 6, 4);	/* DC Offset Q : -512~1535 */
	i4DcOffsetQ = ntohl(i4DcOffsetQ);
	memcpy(&i4Band, HqaCmdFrame->Data + 4 * 7, 4);	/* Band : 2.4G/5G */
	i4Band = ntohl(i4Band);
	memcpy(&i4RF_Power, HqaCmdFrame->Data + 4 * 8, 4);	/* RF_Power: (1db) 0~15 */
	i4RF_Power = ntohl(i4RF_Power);
	memcpy(&i4Digi_Power, HqaCmdFrame->Data + 4 * 9, 4);	/* Digi_Power: (0.25db) -32~31 */
	i4Digi_Power = ntohl(i4Digi_Power);

	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone BandIdx = 0x%08x\n",
	       i4BandIdx);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone Control = 0x%08x\n",
	       i4Control);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone AntIndex = 0x%08x\n",
	       i4AntIndex);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone ToneType = 0x%08x\n",
	       i4ToneType);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone ToneFreq = 0x%08x\n",
	       i4ToneFreq);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone DcOffsetI = 0x%08x\n",
	       i4DcOffsetI);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone DcOffsetQ = 0x%08x\n",
	       i4DcOffsetQ);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone Band = 0x%08x\n",
	       i4Band);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone RF_Power = 0x%08x\n",
	       i4RF_Power);
	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_DBDCTXTone Digi_Power = 0x%08x\n",
	       i4Digi_Power);

	/*
	 * Select TX Antenna
	 * RF_Power: (1db) 0~15
	 * Digi_Power: (0.25db) -32~31
	 */
	MT_ATESetDBDCTxTonePower(prNetDev, i4AntIndex, i4RF_Power, i4Digi_Power);

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

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_DBDCContinuousTX(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Band = 0, u4Control = 0, u4AntMask = 0, u4Phymode = 0, u4BW = 0;
	UINT_32 u4Pri_Ch = 0, u4Rate = 0, u4Central_Ch = 0, u4TxfdMode = 0, u4Freq = 0;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4Band : %d\n", u4Band);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4Control : %d\n", u4Control);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4AntMask : %d\n", u4AntMask);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4Phymode : %d\n", u4Phymode);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4BW : %d\n", u4BW);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4Pri_Ch : %d\n", u4Pri_Ch);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4Rate : %d\n", u4Rate);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4Central_Ch : %d\n", u4Central_Ch);	/* ok */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DBDCContinuousTX u4TxfdMode : %d\n", u4TxfdMode);	/* ok */

	if (u4Control) {
		MT_ATESetDBDCBandIndex(prNetDev, u4Band);
		u4Freq = nicChannelNum2Freq(u4Central_Ch);
		MT_ATESetChannel(prNetDev, 0, u4Freq);
		MT_ATEPrimarySetting(prNetDev, u4Pri_Ch);

		if (u4Phymode == 1) {
			u4Phymode = 0;
			u4Rate += 4;
		} else if ((u4Phymode == 0) && ((u4Rate == 9) || (u4Rate == 10) || (u4Rate == 11)))
			u4Phymode = 1;
		MT_ATESetPreamble(prNetDev, u4Phymode);

		if (u4Phymode == 0) {
			u4Rate |= 0x00000000;

			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT CCK/OFDM (normal preamble) rate : %d\n", u4Rate);

			MT_ATESetRate(prNetDev, u4Rate);
		} else if (u4Phymode == 1) {
			if (u4Rate == 9)
				u4Rate = 1;
			else if (u4Rate == 10)
				u4Rate = 2;
			else if (u4Rate == 11)
				u4Rate = 3;
			u4Rate |= 0x00000000;

			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT CCK (short preamble) rate : %d\n", u4Rate);

			MT_ATESetRate(prNetDev, u4Rate);
		} else if (u4Phymode >= 2 && u4Phymode <= 4) {
			u4Rate |= 0x80000000;

			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HT/VHT rate : %d\n", u4Rate);

			MT_ATESetRate(prNetDev, u4Rate);
		}

		MT_ATESetSystemBW(prNetDev, u4BW);

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_CW_MODE;
		rRfATInfo.u4FuncData = u4TxfdMode;

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

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ANTMASK;
		rRfATInfo.u4FuncData = u4AntMask;

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

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
		rRfATInfo.u4FuncData = RF_AT_COMMAND_CW;

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
	} else {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
		rRfATInfo.u4FuncData = RF_AT_COMMAND_STOPTEST;

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
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetRXFilterPktLen(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Band = 0, u4Control = 0, u4RxPktlen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band = ntohl(u4Band);
	memcpy(&u4Control, HqaCmdFrame->Data + 4 * 1, 4);
	u4Control = ntohl(u4Control);
	memcpy(&u4RxPktlen, HqaCmdFrame->Data + 4 * 2, 4);
	u4RxPktlen = ntohl(u4RxPktlen);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXFilterPktLen Band : %d\n", u4Band);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXFilterPktLen Control : %d\n", u4Control);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXFilterPktLen RxPktlen : %d\n", u4RxPktlen);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_RX_FILTER_PKT_LEN;
	rRfATInfo.u4FuncData = (UINT_32) (u4RxPktlen & BITS(0, 23));
	rRfATInfo.u4FuncData |= (UINT_32) (u4Band << 24);

	if (u4Control == 1)
		rRfATInfo.u4FuncData |= BIT(30);
	else
		rRfATInfo.u4FuncData &= ~BIT(30);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetRXFilterPktLen rRfATInfo.u4FuncData : 0x%08x\n",
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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetTXInfo(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 u4Txed_band0 = 0;
	UINT_32 u4Txed_band1 = 0;
	INT_32 i4Status;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetTXInfo\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_TXED_COUNT;
	rRfATInfo.u4FuncData = 0;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidRftestQueryAutoTest, &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Txed_band0 = rRfATInfo.u4FuncData;

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT u4Txed_band0 packet count = %d\n", u4Txed_band0);

		u4Txed_band0 = ntohl(u4Txed_band0);
		memcpy(HqaCmdFrame->Data + 2, &u4Txed_band0, sizeof(u4Txed_band0));
	}

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_TXED_COUNT;
	rRfATInfo.u4FuncIndex |= BIT(8);
	rRfATInfo.u4FuncData = 0;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidRftestQueryAutoTest, &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Txed_band1 = rRfATInfo.u4FuncData;

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT u4Txed_band1 packet count = %d\n", u4Txed_band1);

		u4Txed_band1 = ntohl(u4Txed_band1);
		memcpy(HqaCmdFrame->Data + 2 + 4, &u4Txed_band1, sizeof(u4Txed_band1));
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Txed_band0) + sizeof(u4Txed_band1), i4Status);

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetCfgOnOff(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetCfgOnOff\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetBufferBin(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 Ret = 0;
	UINT_32 data = 0;

	kalMemCopy(&data, HqaCmdFrame->Data, sizeof(data));
	data = ntohl(data);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetBufferBin data=%x\n", data);

	if (data == BUFFER_BIN_MODE) {
		/*Buffer mode*/
		g_ucEepromCurrentMode = BUFFER_BIN_MODE;
	} else if (data == EFUSE_MODE) {
		/*Efuse mode */
		g_ucEepromCurrentMode = EFUSE_MODE;
	} else {
		DBGLOG(RFTEST, INFO, "Invalid data!!\n");
	}

	DBGLOG(RFTEST, INFO, "MT6632 : ucEepromCurrentMode=%x\n", g_ucEepromCurrentMode);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, Ret);
	return Ret;
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
	NULL,					/* 0x1315 */
	HQA_SetBufferBin,		/* 0x1316 */
};

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_ReadTempReferenceValue(struct net_device *prNetDev,
					 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_ReadTempReferenceValue\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Get Thermal Value.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetThermalValue(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	UINT_32 u4Value;
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_TEMP_SENSOR;
	rRfATInfo.u4FuncData = 0;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidRftestQueryAutoTest, &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

	if (i4Status == 0) {
		u4Value = rRfATInfo.u4FuncData;
		u4Value = u4Value >> 16;

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetThermalValue Value = %d\n", u4Value);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetSideBandOption(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetSideBandOption\n");

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetFWInfo(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetFWInfo\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StartContinousTx(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartContinousTx\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetSTBC(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetSTBC\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Set short GI.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetShortGI(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4ShortGi;

	memcpy(&u4ShortGi, HqaCmdFrame->Data, 4);
	u4ShortGi = ntohl(u4ShortGi);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetShortGI u4ShortGi = %d\n", u4ShortGi);

	i4Ret = MT_ATESetTxGi(prNetDev, u4ShortGi);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetDPD(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetDPD\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For Get Rx Statistics.
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetRxStatisticsAll(struct net_device *prNetDev,
				     IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_ACCESS_RX_STAT rRxStatisticsTest;

	/* memset(&g_HqaRxStat, 0, sizeof(PARAM_RX_STAT_T)); */
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetRxStatisticsAll\n");

	rRxStatisticsTest.u4SeqNum = u4RxStatSeqNum;
	rRxStatisticsTest.u4TotalNum = HQA_RX_STATISTIC_NUM + 6;

	i4Ret = kalIoctl(prGlueInfo,
			 wlanoidQueryRxStatistics,
			 &rRxStatisticsTest, sizeof(rRxStatisticsTest), TRUE, TRUE, TRUE, &u4BufLen);

	/* ASSERT(rRxStatisticsTest.u4SeqNum == u4RxStatSeqNum); */

	u4RxStatSeqNum++;

	memcpy(HqaCmdFrame->Data + 2, &(g_HqaRxStat), sizeof(PARAM_RX_STAT_T));
	ResponseToQA(HqaCmdFrame, prIwReqData, (2 + sizeof(PARAM_RX_STAT_T)), i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StartContiTxTone(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StartContiTxTone\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_StopContiTxTone(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StopContiTxTone\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CalibrationTestMode(struct net_device *prNetDev,
				      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Mode = 0;
	UINT_32 u4IcapLen = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CalibrationTestMode\n");

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_DoCalibrationTestItem(struct net_device *prNetDev,
					IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Status = 0;
	UINT_32 u4Item = 0;
	UINT_32 u4Band_idx = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&u4Item, HqaCmdFrame->Data, 4);
	u4Item = ntohl(u4Item);

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DoCalibrationTestItem item : 0x%08x\n", u4Item);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DoCalibrationTestItem band_idx : %d\n", u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RECAL_CAL_STEP;
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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_eFusePhysicalWrite(struct net_device *prNetDev,
				     IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_eFusePhysicalWrite\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_eFusePhysicalRead(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_eFusePhysicalRead\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_eFuseLogicalRead(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_eFuseLogicalRead\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_eFuseLogicalWrite(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_eFuseLogicalWrite\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_TMRSetting(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Setting;
	UINT_32 u4Version;
	UINT_32 u4MPThres;
	UINT_32 u4MPIter;

	memcpy(&u4Setting, HqaCmdFrame->Data + 4 * 0, 4);
	u4Setting = ntohl(u4Setting);
	memcpy(&u4Version, HqaCmdFrame->Data + 4 * 1, 4);
	u4Version = ntohl(u4Version);
	memcpy(&u4MPThres, HqaCmdFrame->Data + 4 * 2, 4);
	u4MPThres = ntohl(u4MPThres);
	memcpy(&u4MPIter, HqaCmdFrame->Data + 4 * 3, 4);
	u4MPIter = ntohl(u4MPIter);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TMRSetting u4Setting : %d\n", u4Setting);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TMRSetting u4Version : %d\n", u4Version);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TMRSetting u4MPThres : %d\n", u4MPThres);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TMRSetting u4MPIter : %d\n", u4MPIter);

	i4Ret = MT_ATE_TMRSetting(prNetDev, u4Setting, u4Version, u4MPThres, u4MPIter);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetRxSNR(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetRxSNR\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_WriteBufferDone(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_16	u2InitAddr;
	UINT_32 Value;
/*	UINT_32 i = 0, j = 0;
*	UINT_32 u4BufLen = 0;
*/
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;
/*	PARAM_CUSTOM_EFUSE_BUFFER_MODE_T rSetEfuseBufModeInfo; */
	PARAM_CUSTOM_EFUSE_BUFFER_MODE_T *prSetEfuseBufModeInfo = NULL;
	PUINT_8 pucConfigBuf = NULL;
	UINT_32 u4ContentLen;
	UINT_32 u4BufLen = 0;
	P_ADAPTER_T prAdapter = NULL;


	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

#if (CFG_FW_Report_Efuse_Address)
	u2InitAddr = prAdapter->u4EfuseStartAddress;
#else
	u2InitAddr = EFUSE_CONTENT_BUFFER_START;
#endif


	memcpy(&Value, HqaCmdFrame->Data + 4 * 0, 4);
	Value = ntohl(Value);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_WriteBufferDone Value : %d\n", Value);

	u4EepromMode = Value;

	/* allocate memory for buffer mode info */
	prSetEfuseBufModeInfo = (PARAM_CUSTOM_EFUSE_BUFFER_MODE_T *)
		kalMemAlloc(sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T), VIR_MEM_TYPE);
	if (prSetEfuseBufModeInfo == NULL)
		goto label_exit;
	kalMemZero(prSetEfuseBufModeInfo, sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T));

	/* copy to the command buffer */
#if (CFG_FW_Report_Efuse_Address)
	u4ContentLen = (prAdapter->u4EfuseEndAddress)-(prAdapter->u4EfuseStartAddress)+1;
#else
	u4ContentLen = EFUSE_CONTENT_BUFFER_SIZE;
#endif
	if (u4ContentLen > MAX_EEPROM_BUFFER_SIZE)
		goto label_exit;
	kalMemCopy(prSetEfuseBufModeInfo->aBinContent, &uacEEPROMImage[u2InitAddr], u4ContentLen);

	prSetEfuseBufModeInfo->ucSourceMode = Value;

	prSetEfuseBufModeInfo->ucCmdType = 0x1 | (prAdapter->rWifiVar.ucCalTimingCtrl << 4);
	prSetEfuseBufModeInfo->ucCount   = 0xFF; /* ucCmdType 1 don't care the ucCount */

	rStatus = kalIoctl(prGlueInfo,
			wlanoidSetEfusBufferMode,
			(PVOID)prSetEfuseBufModeInfo,
			sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T),
			FALSE, TRUE, TRUE, &u4BufLen);

#if 0
	for (i = 0 ; i < MAX_EEPROM_BUFFER_SIZE/16 ; i++) {
		for (j = 0 ; j < 16 ; j++) {
			rSetEfuseBufModeInfo.aBinContent[j].u2Addr = u2InitAddr;
			rSetEfuseBufModeInfo.aBinContent[j].ucValue = uacEEPROMImage[u2InitAddr];
			DBGLOG(RFTEST, INFO, "u2Addr = %x\n", rSetEfuseBufModeInfo.aBinContent[j].u2Addr);
			DBGLOG(RFTEST, INFO, "ucValue = %x\n", rSetEfuseBufModeInfo.aBinContent[j].ucValue);
			u2InitAddr += 1;
		}

		rSetEfuseBufModeInfo.ucSourceMode = 1;
		rSetEfuseBufModeInfo.ucCount = EFUSE_CONTENT_SIZE;
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetEfusBufferMode,
				   &rSetEfuseBufModeInfo,
				   sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T), FALSE, FALSE, TRUE, &u4BufLen);
	}
#endif

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

label_exit:

	/* free memory */
	if (prSetEfuseBufModeInfo != NULL)
		kalMemFree(prSetEfuseBufModeInfo, VIR_MEM_TYPE, sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T));

	if (pucConfigBuf != NULL)
		kalMemFree(pucConfigBuf, VIR_MEM_TYPE, MAX_EEPROM_BUFFER_SIZE);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_FFT(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_FFT\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetTxTonePower(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetTxTonePower\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetChipID(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4ChipId;
	struct mt66xx_chip_info *prChipInfo;
	P_ADAPTER_T prAdapter = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
/*	UINT_32 u4BufLen = 0;
*	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
*/

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetChipID\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prChipInfo = prAdapter->chip_info;
	g_u4Chip_ID = prChipInfo->chip_id;
	u4ChipId = g_u4Chip_ID;

	DBGLOG(RFTEST, INFO,
	       "MT6632 : QA_AGENT HQA_GetChipID ChipId = 0x%08x\n", u4ChipId);

	u4ChipId = ntohl(u4ChipId);
	memcpy(HqaCmdFrame->Data + 2, &u4ChipId, 4);
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSSetSeqData(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		*mps_setting = NULL;
	UINT_32		u4Band_idx = 0;
	UINT_32		u4Offset = 0;
	UINT_32		u4Len = 0;
	UINT_32		i = 0;
	UINT_32		u4Value = 0;
	UINT_32		u4Mode = 0;
	UINT_32		u4TxPath = 0;
	UINT_32		u4Mcs = 0;

	u4Len = ntohs(HqaCmdFrame->Length)/sizeof(UINT_32) - 1;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetSeqData u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(UINT_32)*(u4Len), GFP_KERNEL);

	if (!mps_setting)
		return -ENOMEM;

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetSeqData u4Band_idx : %d\n", u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data)) /* Reserved at least 4 byte availbale data */
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4*i, 4);
		u4Value = ntohl(u4Value);

		u4Mode = (u4Value & BITS(24, 27)) >> 24;
		u4TxPath = (u4Value & BITS(8, 23)) >> 8;
		u4Mcs = (u4Value & BITS(0, 7));

		DBGLOG(RFTEST, INFO,
			"MT6632 : QA_AGENT HQA_MPSSetSeqData mps_setting Case %d (Mode : %d / TX Path : %d / MCS : %d)\n"
			, i, u4Mode, u4TxPath, u4Mcs);

		if (u4Mode == 1) {
			u4Mode = 0;
			u4Mcs += 4;
		} else if ((u4Mode == 0) && ((u4Mcs == 9) || (u4Mcs == 10) || (u4Mcs == 11)))
			u4Mode = 1;

		if (u4Mode == 0) {
			u4Mcs |= 0x00000000;

			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT CCK/OFDM (normal preamble) rate : %d\n", u4Mcs);
		} else if (u4Mode == 1) {
			if (u4Mcs == 9)
				u4Mcs = 1;
			else if (u4Mcs == 10)
				u4Mcs = 2;
			else if (u4Mcs == 11)
				u4Mcs = 3;
			u4Mcs |= 0x00000000;

			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT CCK (short preamble) rate : %d\n", u4Mcs);
		} else if (u4Mode >= 2 && u4Mode <= 4) {
			u4Mcs |= 0x80000000;

			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HT/VHT rate : %d\n", u4Mcs);
		}

		mps_setting[i] = (u4Mcs) | (u4TxPath << 8) | (u4Mode << 24);

		DBGLOG(RFTEST, INFO,
			"MT6632 : QA_AGENT HQA_MPSSetSeqData mps_setting Case %d (Mode : %d / TX Path : %d / MCS : %d)\n",
			i,
			(int)((mps_setting[i] & BITS(24, 27)) >> 24),
			(int)((mps_setting[i] & BITS(8, 23)) >> 8),
			(int)((mps_setting[i] & BITS(0, 7))));

	}

	i4Ret = MT_ATEMPSSetSeqData(prNetDev, u4Len, mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSSetPayloadLength(struct net_device *prNetDev,
				      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		*mps_setting = NULL;
	UINT_32		u4Band_idx = 0;
	UINT_32		u4Offset = 0;
	UINT_32		u4Len = 0;
	UINT_32		i = 0;
	UINT_32		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length)/sizeof(UINT_32) - 1;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPayloadLength u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(UINT_32)*(u4Len), GFP_KERNEL);

	if (!mps_setting)
		return -ENOMEM;

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPayloadLength u4Band_idx : %d\n", u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data)) /* Reserved at least 4 byte availbale data */
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4*i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO,
			"MT6632 : QA_AGENT HQA_MPSSetPayloadLength mps_setting Case %d (Payload Length : %d)\n",
			i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPayloadLength(prNetDev, u4Len, mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSSetPacketCount(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		*mps_setting = NULL;
	UINT_32		u4Band_idx = 0;
	UINT_32		u4Offset = 0;
	UINT_32		u4Len = 0;
	UINT_32		i = 0;
	UINT_32		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length)/sizeof(UINT_32) - 1;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPacketCount u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(UINT_32)*(u4Len), GFP_KERNEL);

	if (!mps_setting)
		return -ENOMEM;

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPacketCount u4Band_idx : %d\n", u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data)) /* Reserved at least 4 byte availbale data */
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4*i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO,
			"MT6632 : QA_AGENT HQA_MPSSetPacketCount mps_setting Case %d (Packet Count : %d)\n",
			i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPacketCount(prNetDev, u4Len, mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSSetPowerGain(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		*mps_setting = NULL;
	UINT_32		u4Band_idx = 0;
	UINT_32		u4Offset = 0;
	UINT_32		u4Len = 0;
	UINT_32		i = 0;
	UINT_32		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length)/sizeof(UINT_32) - 1;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPowerGain u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(UINT_32)*(u4Len), GFP_KERNEL);

	if (!mps_setting)
		return -ENOMEM;

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPowerGain u4Band_idx : %d\n", u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data)) /* Reserved at least 4 byte availbale data */
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4*i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPowerGain mps_setting Case %d (Power : %d)\n",
			i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPowerGain(prNetDev, u4Len, mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSStart(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		u4Band_idx = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSStart\n");

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSStop(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		u4Band_idx = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSStop\n");

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	/* To Do : MPS Stop for Specific Band. */

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSSetNss(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		*mps_setting = NULL;
	UINT_32		u4Band_idx = 0;
	UINT_32		u4Offset = 0;
	UINT_32		u4Len = 0;
	UINT_32		i = 0;
	UINT_32		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length)/sizeof(UINT_32) - 1;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetNss u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(UINT_32)*(u4Len), GFP_KERNEL);

	if (!mps_setting)
		return -ENOMEM;

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetNss u4Band_idx : %d\n", u4Band_idx);

	for (i = 0; i < u4Len; i++) {
		u4Offset = 4 + 4 * i;
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data)) /* Reserved at least 4 byte availbale data */
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4*i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetNss mps_setting Case %d (Nss : %d)\n",
			i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetNss(prNetDev, u4Len, mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_MPSSetPerpacketBW(
	struct net_device *prNetDev,
	IN union iwreq_data *prIwReqData,
	HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		*mps_setting = NULL;
	UINT_32		u4Band_idx = 0;
	UINT_32		u4Offset = 0;
	UINT_32		u4Len = 0;
	UINT_32		i = 0;
	UINT_32		u4Value = 0;

	u4Len = ntohs(HqaCmdFrame->Length)/sizeof(UINT_32) - 1;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPerpacketBW u4Len : %d\n", u4Len);

	mps_setting = kmalloc(sizeof(UINT_32)*(u4Len), GFP_KERNEL);

	if (!mps_setting)
		return -ENOMEM;

	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPerpacketBW u4Band_idx : %d\n", u4Band_idx);

	for (i = 0 ; i < u4Len ; i++) {
		u4Offset = 4 + 4 * i;
		if (u4Offset + 4 > sizeof(HqaCmdFrame->Data)) /* Reserved at least 4 byte availbale data */
			break;
		memcpy(&u4Value, HqaCmdFrame->Data + 4 + 4*i, 4);
		mps_setting[i] = ntohl(u4Value);

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MPSSetPerpacketBW mps_setting Case %d (BW : %d)\n",
			i, mps_setting[i]);
	}

	i4Ret = MT_ATEMPSSetPerpacketBW(prNetDev, u4Len, mps_setting, u4Band_idx);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(mps_setting);

	return i4Ret;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetAIFS(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 SlotTime = 0;
	UINT_32 SifsTime = 0;

	memcpy(&SlotTime, HqaCmdFrame->Data + 4 * 0, 4);
	SlotTime = ntohl(SlotTime);
	memcpy(&SifsTime, HqaCmdFrame->Data + 4 * 1, 4);
	SifsTime = ntohl(SifsTime);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetAIFS SlotTime = %d\n", SlotTime);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetAIFS SifsTime = %d\n", SifsTime);

	i4Ret = MT_ATESetTxIPG(prNetDev, SifsTime);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CheckEfuseModeType(struct net_device *prNetDev,
				     IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32		i4Ret = 0;
	UINT_32		Value = u4EepromMode;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CheckEfuseModeType\n");

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CheckEfuseNativeModeType(struct net_device *prNetDev,
					   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CheckEfuseNativeModeType\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_SetBandMode(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Band_mode = 0;
	UINT_32 u4Band_type = 0;

	memcpy((PUCHAR)&u4Band_mode, HqaCmdFrame->Data + 4 * 0, 4);
	u4Band_mode = ntohl(u4Band_mode);
	memcpy((PUCHAR)&u4Band_type, HqaCmdFrame->Data + 4 * 1, 4);
	u4Band_type = ntohl(u4Band_type);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetBandMode u4Band_mode : %d\n", u4Band_mode);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_SetBandMode u4Band_type : %d\n", u4Band_type);

	if (u4Band_mode == 2)
		g_DBDCEnable = TRUE;
	else if (u4Band_mode == 1)
		g_DBDCEnable = FALSE;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_GetBandMode(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Band_mode = 0;
	UINT_32 u4Band_idx = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy((PUCHAR)&u4Band_idx, HqaCmdFrame->Data, 4);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetBandMode u4Band_idx : %d\n", u4Band_idx);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_DBDC_ENABLE;
	if (g_DBDCEnable)
		rRfATInfo.u4FuncData = 1;
	else
		rRfATInfo.u4FuncData = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_GetBandMode g_DBDCEnable = %d\n", g_DBDCEnable);

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

	memcpy(HqaCmdFrame->Data + 2, &(u4Band_mode), sizeof(u4Band_mode));

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Band_mode), i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_RDDStartExt(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RDDStartExt\n");

	DBGLOG(RFTEST, INFO, "[RDD DUMP START]\n");

	i4Ret = MT_ATERDDStart(prNetDev, "RDDSTART");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_RDDStopExt(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_RDDStopExt\n");

	i4Ret = MT_ATERDDStop(prNetDev, "RDDSTOP");

	DBGLOG(RFTEST, INFO, "[RDD DUMP END]\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_BssInfoUpdate(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 OwnMacIdx = 0, BssIdx = 0;
	UINT_8 ucAddr1[MAC_ADDR_LEN];
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	memcpy(&OwnMacIdx, HqaCmdFrame->Data + 4 * 0, 4);
	OwnMacIdx = ntohl(OwnMacIdx);
	memcpy(&BssIdx, HqaCmdFrame->Data + 4 * 1, 4);
	BssIdx = ntohl(BssIdx);
	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 2, 6);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_BssInfoUpdate OwnMacIdx : %d\n", OwnMacIdx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_BssInfoUpdate BssIdx : %d\n", BssIdx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_BssInfoUpdate addr1:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5]);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   OwnMacIdx, BssIdx, ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5]);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_BssInfoUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_DevInfoUpdate(struct net_device *prNetDev,
				IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 Band = 0, OwnMacIdx = 0;
	UINT_8 ucAddr1[MAC_ADDR_LEN];
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	memcpy(&Band, HqaCmdFrame->Data + 4 * 0, 4);
	Band = ntohl(Band);
	memcpy(&OwnMacIdx, HqaCmdFrame->Data + 4 * 1, 4);
	OwnMacIdx = ntohl(OwnMacIdx);
	memcpy(ucAddr1, HqaCmdFrame->Data + 4 * 2, 6);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DevInfoUpdate Band : %d\n", Band);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DevInfoUpdate OwnMacIdx : %d\n", OwnMacIdx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_DevInfoUpdate addr1:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5]);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   OwnMacIdx, ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5], Band);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_DevInfoUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_LogOnOff(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Band_idx = 0;
	UINT_32 u4Log_type = 0;
	UINT_32 u4Log_ctrl = 0;
	UINT_32 u4Log_size = 200;

	memcpy(&u4Band_idx, HqaCmdFrame->Data, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(&u4Log_type, HqaCmdFrame->Data + 4, 4);
	u4Log_type = ntohl(u4Log_type);
	memcpy(&u4Log_ctrl, HqaCmdFrame->Data + 4 + 4, 4);
	u4Log_ctrl = ntohl(u4Log_ctrl);
	memcpy(&u4Log_size, HqaCmdFrame->Data + 4 + 4 + 4, 4);
	u4Log_size = ntohl(u4Log_size);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_LogOnOff band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_LogOnOff log_type : %d\n", u4Log_type);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_LogOnOff log_ctrl : %d\n", u4Log_ctrl);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_LogOnOff log_size : %d\n", u4Log_size);

	i4Ret = MT_ATELogOnOff(prNetDev, u4Log_type, u4Log_ctrl, u4Log_size);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

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
	ToDoFunction,		/* 0x151C */
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
static INT_32 HQA_TxBfProfileTagInValid(struct net_device *prNetDev,
					IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 invalid = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(invalid), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagInValid\n");

	memcpy(&invalid, HqaCmdFrame->Data, 4);
	invalid = ntohl(invalid);

	kalMemSet(prInBuf, 0, sizeof(invalid));
	kalSprintf(prInBuf, "%u", invalid);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_InValid(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagPfmuIdx(struct net_device *prNetDev,
					IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 pfmuidx = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(pfmuidx), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagPfmuIdx\n");

	memcpy(&pfmuidx, HqaCmdFrame->Data, 4);
	pfmuidx = ntohl(pfmuidx);

	kalMemSet(prInBuf, 0, sizeof(pfmuidx));
	kalSprintf(prInBuf, "%u", pfmuidx);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_PfmuIdx(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagBfType(struct net_device *prNetDev,
				       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 bftype = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(bftype), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagBfType\n");

	memcpy(&bftype, HqaCmdFrame->Data, 4);
	bftype = ntohl(bftype);

	kalMemSet(prInBuf, 0, sizeof(bftype));
	kalSprintf(prInBuf, "%u", bftype);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_BfType(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagBw(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 tag_bw = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(tag_bw), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagBw\n");

	memcpy(&tag_bw, HqaCmdFrame->Data, 4);
	tag_bw = ntohl(tag_bw);

	kalMemSet(prInBuf, 0, sizeof(tag_bw));
	kalSprintf(prInBuf, "%u", tag_bw);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DBW(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagSuMu(struct net_device *prNetDev,
				     IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 su_mu = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(su_mu), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagSuMu\n");

	memcpy(&su_mu, HqaCmdFrame->Data, 4);
	su_mu = ntohl(su_mu);

	kalMemSet(prInBuf, 0, sizeof(su_mu));
	kalSprintf(prInBuf, "%u", su_mu);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SuMu(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagMemAlloc(struct net_device *prNetDev,
					 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 col_idx0, row_idx0, col_idx1, row_idx1;
	UINT_32 col_idx2, row_idx2, col_idx3, row_idx3;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagMemAlloc\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   col_idx0, row_idx0, col_idx1, row_idx1, col_idx2, row_idx2, col_idx3, row_idx3);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_Mem(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagMatrix(struct net_device *prNetDev,
				       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 nrow, ncol, ngroup, LM, code_book, htc_exist;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagMatrix\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x", nrow, ncol, ngroup, LM, code_book, htc_exist);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_Matrix(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagSnr(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 snr_sts0, snr_sts1, snr_sts2, snr_sts3;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagSnr\n");

	memcpy(&snr_sts0, HqaCmdFrame->Data + 4 * 0, 4);
	snr_sts0 = ntohl(snr_sts0);
	memcpy(&snr_sts1, HqaCmdFrame->Data + 4 * 1, 4);
	snr_sts1 = ntohl(snr_sts1);
	memcpy(&snr_sts2, HqaCmdFrame->Data + 4 * 2, 4);
	snr_sts2 = ntohl(snr_sts2);
	memcpy(&snr_sts3, HqaCmdFrame->Data + 4 * 3, 4);
	snr_sts3 = ntohl(snr_sts3);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x", snr_sts0, snr_sts1, snr_sts2, snr_sts3);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SNR(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagSmtAnt(struct net_device *prNetDev,
				       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 smt_ant = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(smt_ant), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagSmtAnt\n");

	memcpy(&smt_ant, HqaCmdFrame->Data + 4 * 0, 4);
	smt_ant = ntohl(smt_ant);

	kalMemSet(prInBuf, 0, sizeof(smt_ant));
	kalSprintf(prInBuf, "%u", smt_ant);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SmartAnt(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagSeIdx(struct net_device *prNetDev,
				      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 se_idx = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(se_idx), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagSeIdx\n");

	memcpy(&se_idx, HqaCmdFrame->Data + 4 * 0, 4);
	se_idx = ntohl(se_idx);

	kalMemSet(prInBuf, 0, sizeof(se_idx));
	kalSprintf(prInBuf, "%u", se_idx);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_SeIdx(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagRmsdThrd(struct net_device *prNetDev,
					 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 rmsd_thrd = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(rmsd_thrd), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagRmsdThrd\n");

	memcpy(&rmsd_thrd, HqaCmdFrame->Data + 4 * 0, 4);
	rmsd_thrd = ntohl(rmsd_thrd);

	kalMemSet(prInBuf, 0, sizeof(rmsd_thrd));
	kalSprintf(prInBuf, "%u", rmsd_thrd);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_RmsdThrd(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagMcsThrd(struct net_device *prNetDev,
					IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 mcs_lss0, mcs_sss0, mcs_lss1, mcs_sss1, mcs_lss2, mcs_sss2;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagMcsThrd\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x", mcs_lss0, mcs_sss0, mcs_lss1, mcs_sss1, mcs_lss2,
		   mcs_sss2);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_McsThrd(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagTimeOut(struct net_device *prNetDev,
					IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 bf_tout = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(bf_tout), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagTimeOut\n");

	memcpy(&bf_tout, HqaCmdFrame->Data + 4 * 0, 4);
	bf_tout = ntohl(bf_tout);

	kalMemSet(prInBuf, 0, sizeof(bf_tout));
	kalSprintf(prInBuf, "%x", bf_tout);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_TimeOut(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagDesiredBw(struct net_device *prNetDev,
					  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 desire_bw = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(desire_bw), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagDesiredBw\n");

	memcpy(&desire_bw, HqaCmdFrame->Data + 4 * 0, 4);
	desire_bw = ntohl(desire_bw);

	kalMemSet(prInBuf, 0, sizeof(desire_bw));
	kalSprintf(prInBuf, "%u", desire_bw);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DesiredBW(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagDesiredNc(struct net_device *prNetDev,
					  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 desire_nc = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(desire_nc), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagDesiredNc\n");

	memcpy(&desire_nc, HqaCmdFrame->Data + 4 * 0, 4);
	desire_nc = ntohl(desire_nc);

	kalMemSet(prInBuf, 0, sizeof(desire_nc));
	kalSprintf(prInBuf, "%u", desire_nc);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DesiredNc(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagDesiredNr(struct net_device *prNetDev,
					  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 desire_nr = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(desire_nr), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagDesiredNr\n");

	memcpy(&desire_nr, HqaCmdFrame->Data + 4 * 0, 4);
	desire_nr = ntohl(desire_nr);

	kalMemSet(prInBuf, 0, sizeof(desire_nr));
	kalSprintf(prInBuf, "%u", desire_nr);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTag_DesiredNr(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagWrite(struct net_device *prNetDev,
				      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 idx = 0;	/* WLAN_IDX */
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(idx), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagWrite\n");

	memcpy(&idx, HqaCmdFrame->Data + 4 * 0, 4);
	idx = ntohl(idx);

	kalMemSet(prInBuf, 0, sizeof(idx));
	kalSprintf(prInBuf, "%u", idx);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTagWrite(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TxBfProfileTagRead(struct net_device *prNetDev,
				     IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 idx = 0, isBFer = 0;
	UINT_8 *prInBuf;
	PFMU_PROFILE_TAG1 rPfmuTag1;
	PFMU_PROFILE_TAG2 rPfmuTag2;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfProfileTagRead\n");

	memcpy(&idx, HqaCmdFrame->Data + 4 * 0, 4);
	idx = ntohl(idx);
	memcpy(&isBFer, HqaCmdFrame->Data + 4 * 1, 4);
	isBFer = ntohl(isBFer);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x", idx, isBFer);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileTagRead(prNetDev, prInBuf);

	rPfmuTag1.au4RawData[0] = ntohl(g_rPfmuTag1.au4RawData[0]);
	rPfmuTag1.au4RawData[1] = ntohl(g_rPfmuTag1.au4RawData[1]);
	rPfmuTag1.au4RawData[2] = ntohl(g_rPfmuTag1.au4RawData[2]);
	rPfmuTag1.au4RawData[3] = ntohl(g_rPfmuTag1.au4RawData[3]);

	rPfmuTag2.au4RawData[0] = ntohl(g_rPfmuTag2.au4RawData[0]);
	rPfmuTag2.au4RawData[1] = ntohl(g_rPfmuTag2.au4RawData[1]);
	rPfmuTag2.au4RawData[2] = ntohl(g_rPfmuTag2.au4RawData[2]);

	memcpy(HqaCmdFrame->Data + 2, &rPfmuTag1, sizeof(PFMU_PROFILE_TAG1));
	memcpy(HqaCmdFrame->Data + 2 + sizeof(PFMU_PROFILE_TAG1), &rPfmuTag2, sizeof(PFMU_PROFILE_TAG2));

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(PFMU_PROFILE_TAG1) + sizeof(PFMU_PROFILE_TAG2), i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_StaRecCmmUpdate(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 wlan_idx, bss_idx, aid;
	UINT_8 mac[MAC_ADDR_LEN];
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StaRecCmmUpdate\n");

	memcpy(&wlan_idx, HqaCmdFrame->Data + 4 * 0, 4);
	wlan_idx = ntohl(wlan_idx);
	memcpy(&bss_idx, HqaCmdFrame->Data + 4 * 1, 4);
	bss_idx = ntohl(bss_idx);
	memcpy(&aid, HqaCmdFrame->Data + 4 * 2, 4);
	aid = ntohl(aid);

	memcpy(mac, HqaCmdFrame->Data + 4 * 3, 6);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   wlan_idx, bss_idx, aid, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_StaRecCmmUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_StaRecBfUpdate(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 wlan_idx, bss_idx, PfmuId, su_mu, etxbf_cap, ndpa_rate, ndp_rate;
	UINT_32 report_poll_rate, tx_mode, nc, nr, cbw, spe_idx, tot_mem_req;
	UINT_32 mem_req_20m, mem_row0, mem_col0, mem_row1, mem_col1;
	UINT_32 mem_row2, mem_col2, mem_row3, mem_col3;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_StaRecBfUpdate\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02d:%02d:%02d:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   wlan_idx, bss_idx, PfmuId, su_mu, etxbf_cap, ndpa_rate, ndp_rate, report_poll_rate, tx_mode, nc, nr,
		   cbw, spe_idx, tot_mem_req, mem_req_20m, mem_row0, mem_col0, mem_row1, mem_col1, mem_row2, mem_col2,
		   mem_row3, mem_col3);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_StaRecBfUpdate(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_BFProfileDataRead(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 idx = 0, fgBFer = 0, subcarrIdx = 0, subcarr_start = 0, subcarr_end = 0;
	UINT_32 NumOfsub = 0;
	UINT_32 offset = 0;
	UINT_8 *SubIdx = NULL;
	UINT_8 *prInBuf;
	PFMU_DATA rPfmuData;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_BFProfileDataRead\n");

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

	for (subcarrIdx = subcarr_start; subcarrIdx <= subcarr_end; subcarrIdx++) {
		SubIdx = (UINT_8 *) &subcarrIdx;

		kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
		kalSprintf(prInBuf, "%02x:%02x:%02x:%02x", idx, fgBFer, SubIdx[1], SubIdx[0]);

		DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

		i4Ret = Set_TxBfProfileDataRead(prNetDev, prInBuf);

		rPfmuData.au4RawData[0] = ntohl(g_rPfmuData.au4RawData[0]);
		rPfmuData.au4RawData[1] = ntohl(g_rPfmuData.au4RawData[1]);
		rPfmuData.au4RawData[2] = ntohl(g_rPfmuData.au4RawData[2]);
		rPfmuData.au4RawData[3] = ntohl(g_rPfmuData.au4RawData[3]);
		rPfmuData.au4RawData[4] = ntohl(g_rPfmuData.au4RawData[4]);

		memcpy(HqaCmdFrame->Data + 2 + offset, &rPfmuData, sizeof(rPfmuData));
		offset += sizeof(rPfmuData);
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + offset, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_BFProfileDataWrite(struct net_device *prNetDev,
				     IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 pfmuid, subcarrier, phi11, psi21, phi21, psi31, phi31, psi41;
	UINT_32 phi22, psi32, phi32, psi42, phi33, psi43, snr00, snr01, snr02, snr03;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_BFProfileDataWrite\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%03x:%03x:%02x:%03x:%02x:%03x:%02x:%03x:%02x:%03x:%02x:%03x:%02x:%02x:%02x:%02x:%02x",
		   pfmuid, subcarrier, phi11, psi21, phi21, psi31, phi31, psi41,
		   phi22, psi32, phi32, psi42, phi33, psi43, snr00, snr01, snr02, snr03);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfProfileDataWrite(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_BFSounding(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 su_mu, mu_num, snd_interval, wlan_id0;
	UINT_32 wlan_id1, wlan_id2, wlan_id3, band_idx;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_BFSounding\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   su_mu, mu_num, snd_interval, wlan_id0, wlan_id1, wlan_id2, wlan_id3);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_Trigger_Sounding_Proc(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_TXBFSoundingStop(struct net_device *prNetDev,
				   IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TXBFSoundingStop\n");

	i4Ret = Set_Stop_Sounding_Proc(prNetDev, NULL);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_TXBFProfileDataWriteAllExt(struct net_device *prNetDev,
					     IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_TxBfTxApply(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 eBF_enable = 0;
	UINT_32 iBF_enable = 0;
	UINT_32 wlan_id = 0;
	UINT_32 MuTx_enable = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_TxBfTxApply\n");

	memcpy(&eBF_enable, HqaCmdFrame->Data + 4 * 0, 4);
	eBF_enable = ntohl(eBF_enable);
	memcpy(&iBF_enable, HqaCmdFrame->Data + 4 * 1, 4);
	iBF_enable = ntohl(iBF_enable);
	memcpy(&wlan_id, HqaCmdFrame->Data + 4 * 2, 4);
	wlan_id = ntohl(wlan_id);
	memcpy(&MuTx_enable, HqaCmdFrame->Data + 4 * 3, 4);
	MuTx_enable = ntohl(MuTx_enable);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x", wlan_id, eBF_enable, iBF_enable, MuTx_enable);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_TxBfTxApply(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_ManualAssoc(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 type;
	UINT_32 wtbl_idx;
	UINT_32 ownmac_idx;
	UINT_32 phymode;
	UINT_32 bw;
	UINT_32 pfmuid;
	UINT_32 marate_mode;
	UINT_32 marate_mcs;
	UINT_32 spe_idx;
	UINT_32 aid;
	UINT_8 ucAddr1[MAC_ADDR_LEN];
	UINT_32 nss = 1;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_ManualAssoc\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5], type, wtbl_idx, ownmac_idx,
		   phymode, bw, nss, pfmuid, marate_mode, marate_mcs, spe_idx, aid, 0);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

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
static INT_32 HQA_MUGetInitMCS(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Gid = 0;
	UINT_32 u4User0InitMCS = 0;
	UINT_32 u4User1InitMCS = 0;
	UINT_32 u4User2InitMCS = 0;
	UINT_32 u4User3InitMCS = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUGetInitMCS\n");

	memcpy(&u4Gid, HqaCmdFrame->Data, 4);
	u4Gid = ntohl(u4Gid);

	kalMemSet(prInBuf, 0, sizeof(u4Gid));
	kalSprintf(prInBuf, "%u", u4Gid);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUGetInitMCS(prNetDev, prInBuf);

	u4User0InitMCS = ntohl(u4User0InitMCS);
	u4User1InitMCS = ntohl(u4User1InitMCS);
	u4User2InitMCS = ntohl(u4User2InitMCS);
	u4User3InitMCS = ntohl(u4User3InitMCS);

	memcpy(HqaCmdFrame->Data + 2, &u4User0InitMCS, sizeof(UINT_32));
	memcpy(HqaCmdFrame->Data + 2 + 1 * sizeof(UINT_32), &u4User1InitMCS, sizeof(UINT_32));
	memcpy(HqaCmdFrame->Data + 2 + 2 * sizeof(UINT_32), &u4User2InitMCS, sizeof(UINT_32));
	memcpy(HqaCmdFrame->Data + 2 + 3 * sizeof(UINT_32), &u4User3InitMCS, sizeof(UINT_32));

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUCalInitMCS(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Num_of_user;
	UINT_32 u4Bandwidth;
	UINT_32 u4Nss_of_user0;
	UINT_32 u4Nss_of_user1;
	UINT_32 u4Nss_of_user2;
	UINT_32 u4Nss_of_user3;
	UINT_32 u4Pf_mu_id_of_user0;
	UINT_32 u4Pf_mu_id_of_user1;
	UINT_32 u4Pf_mu_id_of_user2;
	UINT_32 u4Pf_mu_id_of_user3;
	UINT_32 u4Num_of_txer;	/* number of antenna */
	UINT_32 u4Spe_index;
	UINT_32 u4Group_index;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUCalInitMCS\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4Num_of_user, u4Bandwidth, u4Nss_of_user0, u4Nss_of_user1, u4Pf_mu_id_of_user0, u4Pf_mu_id_of_user1,
		   u4Num_of_txer, u4Spe_index, u4Group_index);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUCalInitMCS(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUCalLQ(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Type = 0;
	UINT_32 u4Num_of_user;
	UINT_32 u4Bandwidth;
	UINT_32 u4Nss_of_user0;
	UINT_32 u4Nss_of_user1;
	UINT_32 u4Nss_of_user2;
	UINT_32 u4Nss_of_user3;
	UINT_32 u4Pf_mu_id_of_user0;
	UINT_32 u4Pf_mu_id_of_user1;
	UINT_32 u4Pf_mu_id_of_user2;
	UINT_32 u4Pf_mu_id_of_user3;
	UINT_32 u4Num_of_txer;	/* number of antenna */
	UINT_32 u4Spe_index;
	UINT_32 u4Group_index;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUCalLQ\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4Num_of_user, u4Bandwidth, u4Nss_of_user0, u4Nss_of_user1, u4Pf_mu_id_of_user0, u4Pf_mu_id_of_user1,
		   u4Num_of_txer, u4Spe_index, u4Group_index);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUCalLQ(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUGetLQ(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 i;
	UINT_8 u4LqReport[NUM_OF_USER * NUM_OF_MODUL];
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUGetLQ\n");

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	kalMemSet(u4LqReport, 0, (NUM_OF_USER * NUM_OF_MODUL));

	i4Ret = Set_MUGetLQ(prNetDev, prInBuf);

	for (i = 0; i < NUM_OF_USER * NUM_OF_MODUL; i++) {
		u4LqReport[i] = ntohl(u4LqReport[i]);
		memcpy(HqaCmdFrame->Data + 2 + i * sizeof(UINT_32), &u4LqReport[i], sizeof(UINT_32));
	}

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUSetSNROffset(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Offset = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUSetSNROffset\n");

	memcpy(&u4Offset, HqaCmdFrame->Data + 4 * 0, 4);
	u4Offset = ntohl(u4Offset);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4Offset);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetSNROffset(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUSetZeroNss(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Zero_nss = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUSetZeroNss\n");

	memcpy(&u4Zero_nss, HqaCmdFrame->Data + 4 * 0, 4);
	u4Zero_nss = ntohl(u4Zero_nss);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4Zero_nss);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetZeroNss(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUSetSpeedUpLQ(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4SpeedUpLq = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUSetSpeedUpLQ\n");

	memcpy(&u4SpeedUpLq, HqaCmdFrame->Data + 4 * 0, 4);
	u4SpeedUpLq = ntohl(u4SpeedUpLq);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4SpeedUpLq);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetSpeedUpLQ(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;

}

static INT_32 HQA_MUSetMUTable(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_8 *prTable;
	UINT_16 u2Len = 0;
	UINT_32 u4SuMu = 0;

	prTable = kmalloc_array(u2Len, sizeof(UINT_8), GFP_KERNEL);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUSetMUTable\n");

	u2Len = ntohl(HqaCmdFrame->Length) - sizeof(u4SuMu);

	memcpy(&u4SuMu, HqaCmdFrame->Data + 4 * 0, 4);
	u4SuMu = ntohl(u4SuMu);

	memcpy(prTable, HqaCmdFrame->Data + 4, u2Len);

	i4Ret = Set_MUSetMUTable(prNetDev, prTable);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_MUSetGroup(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4GroupIndex, u4NumOfUser, u4User0Ldpc, u4User1Ldpc, u4User2Ldpc, u4User3Ldpc;
	UINT_32 u4ShortGI, u4Bw, u4User0Nss, u4User1Nss, u4User2Nss, u4User3Nss;
	UINT_32 u4GroupId, u4User0UP, u4User1UP, u4User2UP, u4User3UP;
	UINT_32 u4User0MuPfId, u4User1MuPfId, u4User2MuPfId, u4User3MuPfId;
	UINT_32 u4User0InitMCS, u4User1InitMCS, u4User2InitMCS, u4User3InitMCS;
	UINT_8 ucAddr1[MAC_ADDR_LEN], ucAddr2[MAC_ADDR_LEN], ucAddr3[MAC_ADDR_LEN], ucAddr4[MAC_ADDR_LEN];
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUSetGroup\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf,
		   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4GroupIndex, u4NumOfUser, u4User0Ldpc, u4User1Ldpc, u4ShortGI, u4Bw, u4User0Nss, u4User1Nss,
		   u4GroupId, u4User0UP, u4User1UP, u4User0MuPfId, u4User1MuPfId, u4User0InitMCS, u4User1InitMCS,
		   ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5], ucAddr2[0], ucAddr2[1],
		   ucAddr2[2], ucAddr2[3], ucAddr2[4], ucAddr2[5]);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetGroup(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUGetQD(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4SubIdx = 0;

	/* TODO */
	UINT_32 u4User0InitMCS = 0;
	UINT_32 u4User1InitMCS = 0;
	UINT_32 u4User2InitMCS = 0;
	UINT_32 u4User3InitMCS = 0;

	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUGetQD\n");

	memcpy(&u4SubIdx, HqaCmdFrame->Data, 4);
	u4SubIdx = ntohl(u4SubIdx);

	kalMemSet(prInBuf, 0, sizeof(u4SubIdx));
	kalSprintf(prInBuf, "%u", u4SubIdx);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUGetQD(prNetDev, prInBuf);

	/* TODO */
	u4User0InitMCS = ntohl(u4User0InitMCS);
	u4User1InitMCS = ntohl(u4User1InitMCS);
	u4User2InitMCS = ntohl(u4User2InitMCS);
	u4User3InitMCS = ntohl(u4User3InitMCS);

	memcpy(HqaCmdFrame->Data + 2, &u4User0InitMCS, sizeof(UINT_32));
	memcpy(HqaCmdFrame->Data + 2 + 1 * sizeof(UINT_32), &u4User1InitMCS, sizeof(UINT_32));
	memcpy(HqaCmdFrame->Data + 2 + 2 * sizeof(UINT_32), &u4User2InitMCS, sizeof(UINT_32));
	memcpy(HqaCmdFrame->Data + 2 + 3 * sizeof(UINT_32), &u4User3InitMCS, sizeof(UINT_32));

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUSetEnable(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Enable = 0;
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUSetEnable\n");

	memcpy(&u4Enable, HqaCmdFrame->Data + 4 * 0, 4);
	u4Enable = ntohl(u4Enable);

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x", u4Enable);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetEnable(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUSetGID_UP(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 au4Gid[2];
	UINT_32 au4Up[4];
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUSetGID_UP\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x", au4Gid[0], au4Gid[1], au4Up[0], au4Up[1], au4Up[2],
		   au4Up[3]);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

	i4Ret = Set_MUSetGID_UP(prNetDev, prInBuf);

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	kfree(prInBuf);

	return i4Ret;
}

static INT_32 HQA_MUTriggerTx(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4BandIdx, u4IsRandomPattern;
	UINT_32 u4MsduPayloadLength0, u4MsduPayloadLength1, u4MsduPayloadLength2, u4MsduPayloadLength3;
	UINT_32 u4MuPacketCount, u4NumOfSTAs;
	UINT_8 ucAddr1[MAC_ADDR_LEN], ucAddr2[MAC_ADDR_LEN], ucAddr3[MAC_ADDR_LEN], ucAddr4[MAC_ADDR_LEN];
	UINT_8 *prInBuf;

	prInBuf = kmalloc(sizeof(UINT_8) * (HQA_BF_STR_SIZE), GFP_KERNEL);

	if (!prInBuf)
		return -ENOMEM;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_MUTriggerTx\n");

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

	kalMemSet(prInBuf, 0, sizeof(UINT_8) * (HQA_BF_STR_SIZE));
	kalSprintf(prInBuf, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		   u4IsRandomPattern, u4MsduPayloadLength0, u4MsduPayloadLength1, u4MuPacketCount, u4NumOfSTAs,
		   ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5],
		   ucAddr2[0], ucAddr2[1], ucAddr2[2], ucAddr2[3], ucAddr2[4], ucAddr2[5]);

	DBGLOG(RFTEST, ERROR, "MT6632 prInBuf = %s\n", prInBuf);

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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 HQA_CapWiFiSpectrum(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;
	UINT_32 u4Control = 0;
	UINT_32 u4Trigger = 0;
	UINT_32 u4RingCapEn = 0;
	UINT_32 u4TriggerEvent = 0;
	UINT_32 u4CaptureNode = 0;
	UINT_32 u4CaptureLen = 0;
	UINT_32 u4CapStopCycle = 0;
	UINT_32 u4BW = 0;
/*	UINT_32 u4MacTriggerEvent = 0; */	/* Temp unused */
/*	UINT_32 u4TriggerMac = 0; */	/* Temp unused */
	UINT_32 u4WFNum;
	UINT_32 u4IQ;
	UINT_32 u4TempLen = 0;
	UINT_32 u4DataLen;
	INT_32 i = 0, i4Ret = 0;
	INT_32 *prIQAry;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy((PUCHAR)&u4Control, HqaCmdFrame->Data + 4 * 0, 4);
	u4Control = ntohl(u4Control);
	memcpy((PUCHAR)&u4Trigger, HqaCmdFrame->Data + 4 * 1, 4);
	u4Trigger = ntohl(u4Trigger);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4Control = %d\n", u4Control);

	if (u4Control == 1 && u4Trigger == 1) {
		memcpy((PUCHAR)&u4RingCapEn, HqaCmdFrame->Data + 4 * 2, 4);
		u4RingCapEn = ntohl(u4RingCapEn);
		memcpy((PUCHAR)&u4TriggerEvent, HqaCmdFrame->Data + 4 * 3, 4);
		u4TriggerEvent = ntohl(u4TriggerEvent);
		memcpy((PUCHAR)&u4CaptureNode, HqaCmdFrame->Data + 4 * 4, 4);
		u4CaptureNode = ntohl(u4CaptureNode);
		memcpy((PUCHAR)&u4CaptureLen, HqaCmdFrame->Data + 4 * 5, 4);
		u4CaptureLen = ntohl(u4CaptureLen);
		memcpy((PUCHAR)&u4CapStopCycle, HqaCmdFrame->Data + 4 * 6, 4);
		u4CapStopCycle = ntohl(u4CapStopCycle);
		memcpy((PUCHAR)&u4BW, HqaCmdFrame->Data + 4 * 7, 4);
		u4BW = ntohl(u4BW);

		/* AT Command #1, Trigger always = 1 */
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4Trigger = %d\n", u4Trigger);
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4RingCapEn = %d\n", u4RingCapEn);
		/* AT Command #81 */
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4TriggerEvent = %d\n", u4TriggerEvent);
		/* AT Command #80 */
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4CaptureNode = %d\n", u4CaptureNode);
		/* AT Command #83 */
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4CaptureLen = %d\n", u4CaptureLen);
		/* AT Command #84 */
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4CapStopCycle = %d\n", u4CapStopCycle);
		/* AT Command #71 */
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4BW = %d\n", u4BW);

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

		/* iwpriv wlan205 set_test_cmd 84 18000   (Internal Capture Trigger Offset) */
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_TRIGGER_OFFSET;
		rRfATInfo.u4FuncData = u4CapStopCycle;

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

		if (u4CaptureLen == 0)
			u4CaptureLen = 196615;/* 24000; */
		/* iwpriv wlan205 set_test_cmd 83 24576   (Internal Capture Size) */
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_SIZE;
		rRfATInfo.u4FuncData = u4CaptureLen;

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
		if (u4CaptureNode == 0x6)
			u4CaptureNode = 0x10000006;
		else if (u4CaptureNode == 0x8)
			u4CaptureNode = 0x49;
		else if (u4CaptureNode == 0x9)
			u4CaptureNode = 0x48;

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_CONTENT;
		rRfATInfo.u4FuncData = u4CaptureNode;

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
		/* iwpriv wlan205 set_test_cmd 81 0   (Internal Capture  Trigger mode) */
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ICAP_MODE;
		rRfATInfo.u4FuncData = u4TriggerEvent;

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

		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	} else if (u4Control == 2) {
		if (g_bCaptureDone) {
			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum Done!!!!!!!!!!!!!!!!!\n");
			i4Ret = 0;
			/* Query whether ICAP Done */
			ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
		} else {
			DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum Wait!!!!!!!!!!!!!!!!!\n");
			i4Ret = 1;
			/* Query whether ICAP Done */
			ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
		}
	} else if (u4Control == 3) {
		memcpy((PUCHAR)&u4WFNum, HqaCmdFrame->Data + 4 * 1, 4);
		u4WFNum = ntohl(u4WFNum);
		memcpy((PUCHAR)&u4IQ, HqaCmdFrame->Data + 4 * 2, 4);
		u4IQ = ntohl(u4IQ);

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4WFNum = %d\n", u4WFNum);
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_CapWiFiSpectrum u4IQ = %d\n", u4IQ);

		if (u4WFNum <= 1) {
			u4DataLen = 0;
			GetIQData(&prIQAry, &u4DataLen, u4IQ, u4WFNum);
			u4TempLen = u4DataLen;
			u4DataLen /= 4;

			u4Control = ntohl(u4Control);
			memcpy(HqaCmdFrame->Data + 2 + 4 * 0, (UCHAR *) &u4Control, sizeof(u4Control));
			u4WFNum = ntohl(u4WFNum);
			memcpy(HqaCmdFrame->Data + 2 + 4 * 1, (UCHAR *) &u4WFNum, sizeof(u4WFNum));
			u4IQ = ntohl(u4IQ);
			memcpy(HqaCmdFrame->Data + 2 + 4 * 2, (UCHAR *) &u4IQ, sizeof(u4IQ));
			u4DataLen = ntohl(u4DataLen);
			memcpy(HqaCmdFrame->Data + 2 + 4 * 3, (UCHAR *) &u4DataLen, sizeof(u4DataLen));

			for (i = 0; i < u4TempLen / sizeof(UINT_32); i++)
				prIQAry[i] = ntohl(prIQAry[i]);

			memcpy(HqaCmdFrame->Data + 2 + 4 * 4, (UCHAR *) &prIQAry[0], u4TempLen);
		} else {
			u4TempLen = 0;
		}

		/* Get IQ Data and transmit them to UI DLL */
		ResponseToQA(HqaCmdFrame, prIwReqData, 2 + 4 * 4 + u4TempLen, i4Ret);
	} else {
		ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);
	}
	return rStatus;
}

static HQA_CMD_HANDLER HQA_ICAP_CMDS[] = {
	HQA_CapWiFiSpectrum,	/* 0x1580 */
};

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 hqa_set_channel_ext(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Ext_id = 0;
	UINT_32 u4Param_num = 0;
	UINT_32 u4Band_idx = 0;
	UINT_32 u4Central_ch0 = 0;
	UINT_32 u4Central_ch1 = 0;
	UINT_32 u4Sys_bw = 0;
	UINT_32 u4Perpkt_bw = 0;
	UINT_32 u4Pri_sel = 0;
	UINT_32 u4Reason = 0;
	UINT_32 u4Ch_band = 0;
	UINT_32 u4SetFreq = 0;

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext central_ch0 : %d\n", u4Central_ch0);
	/* for BW80+80 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext central_ch1 : %d\n", u4Central_ch1);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext sys_bw : %d\n", u4Sys_bw);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext perpkt_bw : %d\n", u4Perpkt_bw);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext pri_sel : %d\n", u4Pri_sel);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext reason : %d\n", u4Reason);
	/* 0:2.4G    1:5G */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_channel_ext ch_band : %d\n", u4Ch_band);

	/* BW Mapping in QA Tool
	 * 0: BW20
	 * 1: BW40
	 * 2: BW80
	 * 3: BW10
	 * 4: BW5
	 * 5: BW160C
	 * 6: BW160NC
	*/
	/* BW Mapping in MT6632 FW
	 * 0: BW20
	 * 1: BW40
	 * 2: BW80
	 * 3: BW160C
	 * 4: BW160NC
	 * 5: BW5
	 * 6: BW10
	*/
	/* For POR Cal Setting - 20160601 */
	if ((u4Central_ch0 == u4Central_ch1) && (u4Sys_bw == 6) && (u4Perpkt_bw == 6)) {
		DBGLOG(RFTEST, INFO, "MT6632 : Wrong Setting for POR Cal\n");
		goto exit;
	}

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	if ((u4Central_ch0 >= 7 && u4Central_ch0 <= 16) && u4Ch_band == 1) {
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
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));

	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);
	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 hqa_set_txcontent_ext(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Len = 0;
	UINT_32 u4Ext_id = 0;
	UINT_32 u4Param_num = 0;
	UINT_32 u4Band_idx = 0;
	UINT_32 u4FC = 0;
	UINT_32 u4Dur = 0;
	UINT_32 u4Seq = 0;
	UINT_32 u4Gen_payload_rule = 0;
	UINT_32 u4Txlen = 0;
	UINT_32 u4Payload_len = 0;
	UINT_8 ucAddr1[MAC_ADDR_LEN];
	UINT_8 ucAddr2[MAC_ADDR_LEN];
	UINT_8 ucAddr3[MAC_ADDR_LEN];
	UINT_32 ucPayload = 0;

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext band_idx : %d\n", u4Band_idx);
	/* Frame Control...0800 : Beacon */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext FC : 0x%x\n", u4FC);
	/* Duration....NAV */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext dur : 0x%x\n", u4Dur);
	/* Sequence Control */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext seq : 0x%x\n", u4Seq);
	/* Normal:0,Repeat:1,Random:2 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext gen_payload_rule : %d\n", u4Gen_payload_rule);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext txlen : %d\n", u4Txlen);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext payload_len : %d\n", u4Payload_len);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext addr1:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr1[0], ucAddr1[1], ucAddr1[2], ucAddr1[3], ucAddr1[4], ucAddr1[5]);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext addr2:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr2[0], ucAddr2[1], ucAddr2[2], ucAddr2[3], ucAddr2[4], ucAddr2[5]);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_set_txcontent_ext addr3:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucAddr3[0], ucAddr3[1], ucAddr3[2], ucAddr3[3], ucAddr3[4], ucAddr3[5]);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext payload : 0x%x\n", ucPayload);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATESetMacHeader(prNetDev, u4FC, u4Dur, u4Seq);
	MT_ATESetTxPayLoad(prNetDev, u4Gen_payload_rule, ucPayload);
	MT_ATESetTxLength(prNetDev, u4Txlen);
	MT_ATESetMACAddress(prNetDev, RF_AT_FUNCID_SET_MAC_ADDRESS, ucAddr1);
	MT_ATESetMACAddress(prNetDev, RF_AT_FUNCID_SET_TA, ucAddr2);
	/* PeiHsuan Memo : No Set Addr3 */

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Ext_id), i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 hqa_start_tx_ext(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Ext_id = 0;
	UINT_32 u4Param_num = 0;
	UINT_32 u4Band_idx = 0;
	UINT_32 u4Pkt_cnt = 0;
	UINT_32 u4Phymode = 0;
	UINT_32 u4Rate = 0;
	UINT_32 u4Pwr = 0;
	UINT_32 u4Stbc = 0;
	UINT_32 u4Ldpc = 0;
	UINT_32 u4iBF = 0;
	UINT_32 u4eBF = 0;
	UINT_32 u4Wlan_id = 0;
	UINT_32 u4Aifs = 0;
	UINT_32 u4Gi = 0;
	UINT_32 u4Tx_path = 0;
	UINT_32 u4Nss = 0;

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext pkt_cnt : %d\n", u4Pkt_cnt);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext phymode : %d\n", u4Phymode);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext rate : %d\n", u4Rate);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext pwr : %d\n", u4Pwr);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext stbc : %d\n", u4Stbc);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext ldpc : %d\n", u4Ldpc);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext ibf : %d\n", u4iBF);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext ebf : %d\n", u4eBF);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext wlan_id : %d\n", u4Wlan_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext aifs : %d\n", u4Aifs);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext gi : %d\n", u4Gi);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext tx_path : %d\n", u4Tx_path);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_tx_ext nss : %d\n", u4Nss);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATESetTxCount(prNetDev, u4Pkt_cnt);

#if 1
	if (u4Phymode == 1) {
		u4Phymode = 0;
		u4Rate += 4;
	} else if ((u4Phymode == 0) && ((u4Rate == 9) || (u4Rate == 10) || (u4Rate == 11)))
		u4Phymode = 1;
	MT_ATESetPreamble(prNetDev, u4Phymode);

	if (u4Phymode == 0) {
		u4Rate |= 0x00000000;

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT CCK/OFDM (normal preamble) rate : %d\n", u4Rate);

		MT_ATESetRate(prNetDev, u4Rate);
	} else if (u4Phymode == 1) {
		if (u4Rate == 9)
			u4Rate = 1;
		else if (u4Rate == 10)
			u4Rate = 2;
		else if (u4Rate == 11)
			u4Rate = 3;
		u4Rate |= 0x00000000;

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT CCK (short preamble) rate : %d\n", u4Rate);

		MT_ATESetRate(prNetDev, u4Rate);
	} else if (u4Phymode >= 2 && u4Phymode <= 4) {
		u4Rate |= 0x80000000;

		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HT/VHT rate : %d\n", u4Rate);

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
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Ext_id), i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 hqa_start_rx_ext(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Ext_id = 0;
	UINT_32 u4Param_num = 0;
	UINT_32 u4Band_idx = 0;
	UINT_32 u4Rx_path = 0;
	UCHAR ucOwn_mac[MAC_ADDR_LEN];
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memcpy(&u4Ext_id, HqaCmdFrame->Data, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);
	memcpy(ucOwn_mac, HqaCmdFrame->Data + 4 + 4 + 4, 6);
	memcpy(&u4Rx_path, HqaCmdFrame->Data + 4 + 4 + 4 + 6, 4);
	u4Rx_path = ntohl(u4Rx_path);

	memset(&g_HqaRxStat, 0, sizeof(PARAM_RX_STAT_T));

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_rx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_rx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_rx_ext band_idx : %d\n", u4Band_idx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_rx_ext own_mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ucOwn_mac[0], ucOwn_mac[1], ucOwn_mac[2], ucOwn_mac[3], ucOwn_mac[4], ucOwn_mac[5]);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_start_rx_ext rx_path : 0x%x\n", u4Rx_path);

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
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 2 + sizeof(u4Ext_id), i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 hqa_stop_tx_ext(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Ext_id = 0;
	UINT_32 u4Param_num = 0;
	UINT_32 u4Band_idx = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_stop_tx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_stop_tx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_stop_tx_ext band_idx : %d\n", u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATEStopTX(prNetDev, "TXSTOP");

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  QA Agent For
*
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 hqa_stop_rx_ext(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Ext_id = 0;
	UINT_32 u4Param_num = 0;
	UINT_32 u4Band_idx = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4Param_num, HqaCmdFrame->Data + 4, 4);
	u4Param_num = ntohl(u4Param_num);
	memcpy(&u4Band_idx, HqaCmdFrame->Data + 4 + 4, 4);
	u4Band_idx = ntohl(u4Band_idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_stop_rx_ext ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_stop_rx_ext param_num : %d\n", u4Param_num);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_stop_rx_ext band_idx : %d\n", u4Band_idx);

	MT_ATESetDBDCBandIndex(prNetDev, u4Band_idx);
	MT_ATEStopRX(prNetDev, "RXSTOP");

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static INT_32 HQA_iBFInit(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_iBFInit\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_iBFSetValue(struct net_device *prNetDev,
			      IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_iBFSetValue\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_iBFGetStatus(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_iBFGetStatus\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_iBFChanProfUpdate(struct net_device *prNetDev,
				    IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_iBFChanProfUpdate\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_iBFProfileRead(struct net_device *prNetDev,
				 IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_iBFProfileRead\n");

	ResponseToQA(HqaCmdFrame, prIwReqData, 2, i4Ret);

	return i4Ret;
}

static INT_32 HQA_IRRSetADC(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4WFIdx;
	UINT_32 u4ChFreq;
	UINT_32 u4BW;
	UINT_32 u4Sx;
	UINT_32 u4Band;
	UINT_32 u4Ext_id;
	UINT_32 u4RunType;
	UINT_32 u4FType;

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC ext_id : %d\n", u4Ext_id);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC u4WFIdx : %d\n", u4WFIdx);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC u4ChFreq : %d\n", u4ChFreq);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC u4BW : %d\n", u4BW);
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC u4Sx : %d\n", u4Sx);	/* SX : 0, 2 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC u4Band : %d\n", u4Band);
	/* RunType : 0 -> QA, 1 -> ATE */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC u4RunType : %d\n", u4RunType);
	/* FType : 0 -> FI, 1 -> FD */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetADC u4FType : %d\n", u4FType);

	i4Ret = MT_ATE_IRRSetADC(prNetDev, u4WFIdx, u4ChFreq, u4BW, u4Sx, u4Band, u4RunType, u4FType);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static INT_32 HQA_IRRSetRxGain(struct net_device *prNetDev,
			       IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4PgaLpfg;
	UINT_32 u4Lna;
	UINT_32 u4Band;
	UINT_32 u4WF_inx;
	UINT_32 u4Rfdgc;
	UINT_32 u4Ext_id;

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetRxGain ext_id : %d\n", u4Ext_id);
	/* PGA is for MT663, LPFG is for MT7615 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetRxGain u4PgaLpfg : %d\n", u4PgaLpfg);
	/* 5 : UH, 4 : H, 3 : M, 2 : L, 1 : UL */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetRxGain u4Lna : %d\n", u4Lna);
	/* DBDC band0 or band1 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetRxGain u4Band : %d\n", u4Band);
	/* (each bit for each WF) */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetRxGain u4WF_inx : 0x%x\n", u4WF_inx);
	/* only for MT6632 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetRxGain u4Rfdgc : %d\n", u4Rfdgc);

	i4Ret = MT_ATE_IRRSetRxGain(prNetDev, u4PgaLpfg, u4Lna, u4Band, u4WF_inx, u4Rfdgc);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static INT_32 HQA_IRRSetTTG(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Ext_id;
	UINT_32 u4TTGPwrIdx;
	UINT_32 u4ChFreq;
	UINT_32 u4FIToneFreq;
	UINT_32 u4Band;

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

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTTG ext_id : %d\n", u4Ext_id);
	/* TTG Power Index:   Power index value 0~15 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTTG u4TTGPwrIdx : %d\n", u4TTGPwrIdx);
	/* Ch Freq: channel frequency value */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTTG u4ChFreq : %d\n", u4ChFreq);
	/* FI Tone Freq(float): driver calculate TTG Freq(TTG Freq = Ch_freq + FI tone freq) */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTTG u4FIToneFreq : %d\n", u4FIToneFreq);
	/* Band: DBDC band0 or band1 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTTG u4Band : %d\n", u4Band);

	i4Ret = MT_ATE_IRRSetTTG(prNetDev, u4TTGPwrIdx, u4ChFreq, u4FIToneFreq, u4Band);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
	ResponseToQA(HqaCmdFrame, prIwReqData, 6, i4Ret);

	return i4Ret;
}

static INT_32 HQA_IRRSetTrunOnTTG(struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	UINT_32 u4Ext_id;
	UINT_32 u4TTGOnOff;
	UINT_32 u4Band;
	UINT_32 u4WF_inx = 0;

	memcpy(&u4Ext_id, HqaCmdFrame->Data + 4 * 0, 4);
	u4Ext_id = ntohl(u4Ext_id);
	memcpy(&u4TTGOnOff, HqaCmdFrame->Data + 4 * 1, 4);
	u4TTGOnOff = ntohl(u4TTGOnOff);
	memcpy(&u4Band, HqaCmdFrame->Data + 4 * 2, 4);
	u4Band = ntohl(u4Band);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTrunOnTTG ext_id : %d\n", u4Ext_id);
	/* TTG on/off:  0:off,   1: on */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTrunOnTTG u4TTGOnOff : %d\n", u4TTGOnOff);
	/* Band: DBDC band0 or band1 */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTrunOnTTG u4Band : %d\n", u4Band);
	/* (each bit for each WF) */
	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT HQA_IRRSetTrunOnTTG u4WF_inx : %d\n", u4WF_inx);

	i4Ret = MT_ATE_IRRSetTrunOnTTG(prNetDev, u4TTGOnOff, u4Band, u4WF_inx);

	u4Ext_id = ntohl(u4Ext_id);
	memcpy(HqaCmdFrame->Data + 2, (UCHAR *) &u4Ext_id, sizeof(u4Ext_id));
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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
static INT_32 hqa_ext_cmds(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Ret = 0;
	INT_32 i4Idx = 0;

	memmove((PUCHAR)&i4Idx, (PUCHAR)&HqaCmdFrame->Data, 4);
	i4Idx = ntohl(i4Idx);

	DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT hqa_ext_cmds index : %d\n", i4Idx);

	if (i4Idx < (sizeof(hqa_ext_cmd_set) / sizeof(HQA_CMD_HANDLER))) {
		if (hqa_ext_cmd_set[i4Idx] != NULL)
			i4Ret = (*hqa_ext_cmd_set[i4Idx]) (prNetDev, prIwReqData, HqaCmdFrame);
		else
			DBGLOG(RFTEST, INFO,
			"MT6632 : QA_AGENT hqa_ext_cmds cmd idx %d is NULL\n",
			i4Idx);
	} else
		DBGLOG(RFTEST, INFO,
		"MT6632 : QA_AGENT hqa_ext_cmds cmd idx %d is not supported\n",
		i4Idx);

	return i4Ret;
}

static HQA_CMD_HANDLER HQA_CMD_SET6[] = {
	/* cmd id start from 0x1600 */
	hqa_ext_cmds,		/* 0x1600 */
};

static HQA_CMD_TABLE HQA_CMD_TABLES[] = {
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
* \param[in] prNetDev				Pointer to the Net Device
* \param[in] prIwReqData
* \param[in] HqaCmdFrame			Ethernet Frame Format receive from QA Tool DLL
* \param[out] None
*
* \retval 0						On success.
*/
/*----------------------------------------------------------------------------*/
int HQA_CMDHandler(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame)
{
	INT_32 i4Status = 0;
	UINT_32 u4CmdId;
	UINT_32 u4TableIndex = 0;

	u4CmdId = ntohs(HqaCmdFrame->Id);

	while (u4TableIndex < (sizeof(HQA_CMD_TABLES) / sizeof(HQA_CMD_TABLE))) {
		int CmdIndex = 0;

		CmdIndex = u4CmdId - HQA_CMD_TABLES[u4TableIndex].CmdOffset;
		if ((CmdIndex >= 0) && (CmdIndex < HQA_CMD_TABLES[u4TableIndex].CmdSetSize)) {
			HQA_CMD_HANDLER *pCmdSet;

			pCmdSet = HQA_CMD_TABLES[u4TableIndex].CmdSet;

			if (pCmdSet[CmdIndex] != NULL)
				i4Status = (*pCmdSet[CmdIndex]) (prNetDev, prIwReqData, HqaCmdFrame);
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
		  IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	INT_32 i4Status = 0;
	HQA_CMD_FRAME *HqaCmdFrame;
	UINT_32 u4ATEMagicNum, u4ATEId, u4ATEData;

	HqaCmdFrame = kmalloc(sizeof(*HqaCmdFrame), GFP_KERNEL);

	if (!HqaCmdFrame) {
		i4Status = -ENOMEM;
		goto ERROR0;
	}

	memset(HqaCmdFrame, 0, sizeof(*HqaCmdFrame));

	if (copy_from_user(HqaCmdFrame, prIwReqData->data.pointer, prIwReqData->data.length)) {
		i4Status = -EFAULT;
		goto ERROR1;
	}

	u4ATEMagicNum = ntohl(HqaCmdFrame->MagicNo);
	u4ATEId = ntohs(HqaCmdFrame->Id);
	memcpy((PUCHAR)&u4ATEData, HqaCmdFrame->Data, 4);
	u4ATEData = ntohl(u4ATEData);

	switch (u4ATEMagicNum) {
	case HQA_CMD_MAGIC_NO:
		i4Status = HQA_CMDHandler(prNetDev, prIwReqData, HqaCmdFrame);
		break;
	default:
		i4Status = -EINVAL;
		DBGLOG(RFTEST, INFO, "MT6632 : QA_AGENT ATEMagicNum Error!!!\n");
		break;
	}

ERROR1:
	kfree(HqaCmdFrame);
ERROR0:
	return i4Status;
}

int priv_set_eeprom_mode(IN UINT_32 u4Mode)
{
	if ((u4Mode != EFUSE_MODE) && (u4Mode != BUFFER_BIN_MODE))
		return -EINVAL;

	g_ucEepromCurrentMode = u4Mode;
	return 0;
}
#endif
