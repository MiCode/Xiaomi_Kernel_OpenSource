/*
 * platform_byt_audio.h: Baytrail audio platform data header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Omair Md Abdullah <omair.m.abdullah@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_BYT_AUDIO_H_
#define _PLATFORM_BYT_AUDIO_H_

enum {
	BYT_DPCM_AUDIO = 0,
	BYT_DPCM_VOIP,
};

/* LPE viewpoint addresses */
/* TODO: move to DSDT */
#define SST_BYT_IRAM_PHY_START	0xff2c0000
#define SST_BYT_IRAM_PHY_END	0xff2d4000
#define SST_BYT_DRAM_PHY_START	0xff300000
#define SST_BYT_DRAM_PHY_END	0xff320000
#define SST_BYT_IMR_VIRT_START	0xc0000000 /* virtual addr in LPE */
#define SST_BYT_IMR_VIRT_END	0xc01fffff
#define SST_BYT_SHIM_PHY_ADDR	0xff340000
#define SST_BYT_MBOX_PHY_ADDR	0xff344000
#define SST_BYT_DMA0_PHY_ADDR	0xff298000
#define SST_BYT_DMA1_PHY_ADDR	0xff29c000
#define SST_BYT_SSP0_PHY_ADDR	0xff2a0000
#define SST_BYT_SSP2_PHY_ADDR	0xff2a2000

#define BYT_FW_MOD_TABLE_OFFSET 0x80000
#define BYT_FW_MOD_TABLE_SIZE   0x100

#endif
