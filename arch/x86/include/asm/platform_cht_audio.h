/*
 * platform_cht_audio.h: Cherrytrail audio platform data header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Mythri P K<mythri.p.k@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_CHT_AUDIO_H_
#define _PLATFORM_CHT_AUDIO_H_

enum {
	CHT_DPCM_AUDIO = 0,
	CHT_DPCM_DB,
	CHT_DPCM_COMPR,
	CHT_DPCM_VOIP,
};

/* LPE viewpoint addresses */
/* TODO: move to DSDT */
#define CHT_FW_LSP_DDR_BASE 0xC0000000
#define CHT_FW_MOD_END (CHT_FW_LSP_DDR_BASE + 0x1FFFFF)
#define CHT_FW_MOD_TABLE_OFFSET 0x3000
#define CHT_FW_MOD_OFFSET 0x100000
#define CHT_FW_MOD_TABLE_SIZE 0x100

#endif
