/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef _AUDIO_STREAM_ATTRIBUTE_H_
#define _AUDIO_STREAM_ATTRIBUTE_H_


/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/
enum STREAMSTATUS {
	STREAMSTATUS_STATE_FREE = -1,	/* memory is not allocate */
	STREAMSTATUS_STATE_STANDBY,	/* memory allocate and ready */
	STREAMSTATUS_STATE_EXECUTING,	/* stream is running */
};

/* use in modem pcm and DAI */
enum MEMIFDUPWRITE {
	MEMIFDUPWRITE_DUP_WR_DISABLE = 0x0,
	MEMIFDUPWRITE_DUP_WR_ENABLE = 0x1
};

/* Used when AWB and VUL and data is mono */
enum MEMIFMONOSEL {
	MEMIFMONOSEL_AFE_MONO_USE_L = 0x0,
	MEMIFMONOSEL_AFE_MONO_USE_R = 0x1
};

enum SAMPLINGRATE {
	SAMPLINGRATE_AFE_8000HZ = 0x0,
	SAMPLINGRATE_AFE_11025HZ = 0x1,
	SAMPLINGRATE_AFE_12000HZ = 0x2,
	SAMPLINGRATE_AFE_16000HZ = 0x4,
	SAMPLINGRATE_AFE_22050HZ = 0x5,
	SAMPLINGRATE_AFE_24000HZ = 0x6,
	SAMPLINGRATE_AFE_32000HZ = 0x8,
	SAMPLINGRATE_AFE_44100HZ = 0x9,
	SAMPLINGRATE_AFE_48000HZ = 0xa
};

enum FETCHFORMATPERSAMPLE {
	FETCHFORMATPERSAMPLEb_AFE_WLEN_16_BIT = 0,
	FETCHFORMATPERSAMPLE_AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA = 1,
	FETCHFORMATPERSAMPLE_AFE_WLEN_32_BIT_ALIGN_24BIT_DATA_8BIT_0 = 3,
};


#endif
