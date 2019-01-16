/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

#if !defined(SI_TIMING_DEFS_H)
#define SI_TIMING_DEFS_H


enum
{
	VIDEO_INITIAL		= 0xFF
};
			
//must align to extd_drv.h
enum
{
	HDMI_CHANNEL_2 = 0x2,
	HDMI_CHANNEL_3 = 0x3,
	HDMI_CHANNEL_4 = 0x4,
	HDMI_CHANNEL_5 = 0x5,
	HDMI_CHANNEL_6 = 0x6,
	HDMI_CHANNEL_7 = 0x7,
	HDMI_CHANNEL_8 = 0x8,
};

enum
{
	HDMI_SAMPLERATE_32  = 0x1,
	HDMI_SAMPLERATE_44  = 0x2,
	HDMI_SAMPLERATE_48  = 0x3,
	HDMI_SAMPLERATE_96  = 0x4,
	HDMI_SAMPLERATE_192 = 0x5,
};

enum
{
	HDMI_BITWIDTH_16  = 0x1,
	HDMI_BITWIDTH_24  = 0x2,
	HDMI_BITWIDTH_20  = 0x3,
};

enum
{
	AUDIO_32K_2CH		= 0x01,
	AUDIO_44K_2CH		= 0x02,
	AUDIO_48K_2CH		= 0x03,
	AUDIO_96K_2CH		= 0x05,
	AUDIO_192K_2CH	= 0x07,
	AUDIO_32K_8CH		= 0x81,
	AUDIO_44K_8CH		= 0x82,
	AUDIO_48K_8CH		= 0x83,
	AUDIO_96K_8CH		= 0x85,
	AUDIO_192K_8CH	= 0x87,
	AUDIO_INITIAL		= 0xFF
};

enum
{
	VIDEO_3D_NONE		= 0x00,
	VIDEO_3D_FS		= 0x01,
	VIDEO_3D_TB		= 0x02,
	VIDEO_3D_SS		= 0x03,
	VIDEO_3D_INITIAL	= 0xFF
};

// Video mode define ( = VIC code, please see CEA-861 spec)
#define HDMI_640X480P		1
#define HDMI_480I60_4X3	6
#define HDMI_480I60_16X9	7
#define HDMI_576I50_4X3	21
#define HDMI_576I50_16X9	22
#define HDMI_480P60_4X3	2
#define HDMI_480P60_16X9	3
#define HDMI_576P50_4X3	17
#define HDMI_576P50_16X9	18
#define HDMI_720P60			4
#define HDMI_720P50			19
#define HDMI_1080I60		5
#define HDMI_1080I50		20
#define HDMI_1080P24		32
#define HDMI_1080P25		33
#define HDMI_1080P30		34
#define HDMI_1080P60		16 //MHL doesn't supported
#define HDMI_1080P50		31 //MHL doesn't supported


#endif /* if !defined(SI_TIMING_DEFS_H) */
