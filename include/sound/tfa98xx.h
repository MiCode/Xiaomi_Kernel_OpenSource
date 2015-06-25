/*
 *                  Copyright (c), NXP Semiconductors
 *
 *                     (C)NXP Semiconductors
 *
 * NXP reserves the right to make changes without notice at any time.
 * This code is distributed in the hope that it will be useful,
 * but NXP makes NO WARRANTY, expressed, implied or statutory, including but
 * not limited to any implied warranty of MERCHANTABILITY or FITNESS FOR ANY
 * PARTICULAR PURPOSE, or that the use will not infringe any third party patent,
 * copyright or trademark. NXP must not be liable for any loss or damage
 * arising from its use. (c) PLMA, NXP Semiconductors.
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
	struct nxpTfaDevice 	*dev;
	char			*name;
};

struct tfa98xx {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct regulator *vdd;
	struct snd_soc_codec *codec;
	struct workqueue_struct *tfa98xx_wq;
	struct work_struct init_work;
	struct work_struct calib_work;
	struct work_struct mode_work;
	struct work_struct load_preset;
	struct delayed_work delay_work;
	struct mutex dsp_init_lock;
	struct mutex i2c_rw_lock;
	int dsp_init;
	int speaker_imp;
	int sysclk;
	int rst_gpio;
	int max_vol_steps;
	int mode;
	int mode_switched;
	int curr_mode;
	int vol_idx;
	int curr_vol_idx;
	int ic_version;
	u16 rev;
	int vstep;
	int profile;
	int profile_current;
	int profile_count;
	int has_drc;
	int rate;
	struct tfaprofile *profiles;
	struct tfa98xx_firmware fw;

	int (*info_profile)(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo);
	int (*set_profile)(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
	int (*get_profile)(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);

	int (*info_vstep)(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_info *uinfo);
	int (*set_vstep)(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol);
	int (*get_vstep)(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol);

	struct snd_kcontrol_new *(*build_profile_controls)(struct tfa98xx *tfa98xx, int *kcontrol_count);
};

#endif
