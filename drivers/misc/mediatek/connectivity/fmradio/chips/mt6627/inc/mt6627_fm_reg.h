/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT6627_FM_REG_H__
#define __MT6627_FM_REG_H__

/* RDS_BDGRP_ABD_CTRL_REG */
enum {
	BDGRP_ABD_EN = 0x0001,
	BER_RUN = 0x2000
};
#define FM_DAC_CON1 0x83
#define FM_DAC_CON2 0x84
#define FM_FT_CON0 0x86
enum {
	FT_EN = 0x0001
};

#define FM_I2S_CON0 0x90
enum {
	I2S_EN = 0x0001,
	FORMAT = 0x0002,
	WLEN = 0x0004,
	I2S_SRC = 0x0008
};

/* FM_MAIN_CTRL */
enum {
	TUNE = 0x0001,
	SEEK = 0x0002,
	SCAN = 0x0004,
	CQI_READ = 0x0008,
	RDS_MASK = 0x0010,
	MUTE = 0x0020,
	RDS_BRST = 0x0040,
	RAMP_DOWN = 0x0100,
};

enum {
	ANTENNA_TYPE = 0x0010,	/* 0x61 D4, 0:long,  1:short */
	ANALOG_I2S = 0x0080,	/* 0x61 D7, 0:lineout,  1:I2S */
	DE_EMPHASIS = 0x1000,	/* 0x61 D12,0:50us,  1:75 us */
};

#define OSC_FREQ_BITS 0x0070	/* 0x60 bit4~6 */
#define OSC_FREQ_MASK (~OSC_FREQ_BITS)

#endif /* __MT6627_FM_REG_H__ */
