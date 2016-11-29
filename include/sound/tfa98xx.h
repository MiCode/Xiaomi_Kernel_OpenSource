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

#ifndef _TFA98XX_H
#define _TFA98XX_H

#include <sound/core.h>
#include <sound/soc.h>

/* Revision IDs for tfa98xx variants */
#define REV_TFA9887	0x12
#define REV_TFA9890	0x80
#define REV_TFA9895	0x12
#define REV_TFA9897	0x97


struct tfaprofile;
struct nxpTfaDevice;
struct nxpTfaProfile;
struct nxpTfaVolumeStep2File;

struct tfaprofile {
	struct nxpTfaProfile *profile;
	struct nxpTfaVolumeStep2File *vp;
	int vsteps;
	int vstep;
	int index;
	int state;
	char *name;
};

struct tfa98xx_firmware {
	void			*base;
	struct nxpTfaDevice	*dev;
	char			*name;
};

struct tfa98xx {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct snd_soc_codec *codec;
	struct workqueue_struct *tfa98xx_wq;
	struct work_struct init_work;
	struct delayed_work delay_work;
	struct mutex dsp_init_lock;
	int dsp_init;
	int sysclk;
	u8 rev;
	u8 subrev;
	int vstep_ctl;
	int vstep_current;
	int profile_ctl;
	int profile_current;
	int profile_count;
	int has_drc;
	int rate;
	struct tfaprofile *profiles;
	struct tfa98xx_firmware fw;
	u8 reg;
#ifdef CONFIG_SND_SOC_TFA98XX_DEBUG
	int dsp_msg_retries;
#endif
	int monitor_status;
};

#endif
