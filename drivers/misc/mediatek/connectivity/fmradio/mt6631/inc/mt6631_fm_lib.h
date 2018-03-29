/*
* Copyright (C) 2016 MediaTek Inc.
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
#ifndef __MT6631_FM_LIB_H__
#define __MT6631_FM_LIB_H__

#include "fm_typedef.h"

enum {
	DSPPATCH = 0xFFF9,
	USDELAY = 0xFFFA,
	MSDELAY = 0xFFFB,
	HW_VER = 0xFFFD,
	POLL_N = 0xFFFE,	/* poling check if bit(n) is '0' */
	POLL_P = 0xFFFF,	/* polling check if bit(n) is '1' */
};

enum {
	FM_PUS_DSPPATCH = DSPPATCH,
	FM_PUS_USDELAY = USDELAY,
	FM_PUS_MSDELAY = MSDELAY,
	FM_PUS_HW_VER = HW_VER,
	FM_PUS_POLL_N = POLL_N,	/* poling check if bit(n) is '0' */
	FM_PUS_POLL_P = POLL_P,	/* polling check if bit(n) is '1' */
	FM_PUS_MAX
};

enum {
	DSP_PATH = 0x02,
	DSP_COEFF = 0x03,
	DSP_HW_COEFF = 0x04
};

enum {
	mt6631_E1 = 0,
	mt6631_E2
};

struct mt6631_fm_cqi {
	fm_u16 ch;
	fm_u16 rssi;
	fm_u16 reserve;
};

struct adapt_fm_cqi {
	fm_s32 ch;
	fm_s32 rssi;
	fm_s32 reserve;
};

struct mt6631_full_cqi {
	fm_u16 ch;
	fm_u16 rssi;
	fm_u16 pamd;
	fm_u16 pr;
	fm_u16 fpamd;
	fm_u16 mr;
	fm_u16 atdc;
	fm_u16 prx;
	fm_u16 atdev;
	fm_u16 smg;		/* soft-mute gain */
	fm_u16 drssi;		/* delta rssi */
};

#endif
