/*
 * Copyright (C) NXP Semiconductors (PLMA)
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TFA98XX_CORE_H__
#define __TFA98XX_CORE_H__

#include <linux/ioctl.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/tfa98xx.h>


#define TFA98XX_MAX_I2C_SIZE	252


/* DSP Write/Read */
#define TFA98XX_DSP_WRITE			0
#define TFA98XX_DSP_READ			1

/* RPC Status results */
#define TFA98XX_STATUS_OK			0
#define TFA98XX_INVALID_MODULE_ID	2
#define TFA98XX_INVALID_PARAM_ID	3
#define TFA98XX_INVALID_INFO_ID		4

/* Params masks */
#define TFA98XX_FORMAT_MASK		(0x7)
#define TFA98XX_FORMAT_LSB		(0x4)
#define TFA98XX_FORMAT_MSB		(0x2)

#define TFA98XX_POWER_DOWN		(0x1)

/* Mute States */
#define TFA98XX_MUTE_OFF	0
#define TFA98XX_DIGITAL_MUTE	1
#define TFA98XX_AMP_MUTE	2


/* DSP init status */
#define TFA98XX_DSP_INIT_PENDING	0
#define TFA98XX_DSP_INIT_DONE		1
#define TFA98XX_DSP_INIT_FAIL		-1
#define TFA98XX_DSP_INIT_RECOVER	-2



#define TFA_BITFIELDDSCMSK 0x7fffffff

/* retry values */
#define AREFS_TRIES 100
#define CFSTABLE_TRIES 10

int tfa98xx_i2c_read(struct i2c_client *tfa98xx_client,	u8 reg, u8 *value, int len);
int tfa98xx_bulk_write_raw(struct snd_soc_codec *codec, const u8 *data, u8 count);

int tfa98xx_get_vstep(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol);
int tfa98xx_set_vstep(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol);
int tfa98xx_info_vstep(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_info *uinfo);

int tfa98xx_get_profile(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int tfa98xx_set_profile(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int tfa98xx_info_profile(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo);

/* the maximum message length in the communication with the DSP */
#define MAX_PARAM_SIZE (145*3)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ROUND_DOWN(a, n) (((a)/(n))*(n))

#define TFA98XX_PRESET_LENGTH              87



/* possible memory values for DMEM in CF_CONTROLs */
enum Tfa98xx_DMEM {
	Tfa98xx_DMEM_PMEM = 0,
	Tfa98xx_DMEM_XMEM = 1,
	Tfa98xx_DMEM_YMEM = 2,
	Tfa98xx_DMEM_IOMEM = 3,
};

enum Tfa98xx_Mode {
	Tfa98xx_Mode_Normal = 0,
	Tfa98xx_Mode_RCV
};

enum Tfa98xx_Mute {
	Tfa98xx_Mute_Off,
	Tfa98xx_Mute_Digital,
	Tfa98xx_Mute_Amplifier
};

enum Tfa98xx_SpeakerBoostStatusFlags {
	Tfa98xx_SpeakerBoost_Activity = 0,	/* Input signal activity. */
	Tfa98xx_SpeakerBoost_S_Ctrl,	/* S Control triggers the limiter */
	Tfa98xx_SpeakerBoost_Muted,	/* 1 when signal is muted */
	Tfa98xx_SpeakerBoost_X_Ctrl,	/* X Control triggers the limiter */
	Tfa98xx_SpeakerBoost_T_Ctrl,	/* T Control triggers the limiter */
	Tfa98xx_SpeakerBoost_NewModel,	/* New model is available */
	Tfa98xx_SpeakerBoost_VolumeRdy,	/* 0:stable vol, 1:still smoothing */
	Tfa98xx_SpeakerBoost_Damaged,	/* Speaker Damage detected  */
	Tfa98xx_SpeakerBoost_SignalClipping	/* input clipping detected */
};



#endif
