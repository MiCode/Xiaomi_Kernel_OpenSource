#ifndef __MT6627_FM_LIB_H__
#define __MT6627_FM_LIB_H__

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

enum IMG_TYPE {
	IMG_WRONG = 0,
	IMG_ROM,
	IMG_PATCH,
	IMG_COEFFICIENT,
	IMG_HW_COEFFICIENT
};

enum {
	mt6627_E1 = 0,
	mt6627_E2
};

struct mt6627_fm_cqi {
	fm_u16 ch;
	fm_u16 rssi;
	fm_u16 reserve;
};

struct adapt_fm_cqi {
	fm_s32 ch;
	fm_s32 rssi;
	fm_s32 reserve;
};

struct mt6627_full_cqi {
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
