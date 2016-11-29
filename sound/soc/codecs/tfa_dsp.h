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

#ifndef __TFA_DSP_H__
#define __TFA_DSP_H__


/*
 * Type containing all the possible errors that can occur
 */
enum DSP_ERRORS {
	ERROR_RPC_BASE = 100,
	ERROR_RPC_BUSY = 101,
	ERROR_RPC_MODID = 102,
	ERROR_RPC_PARAMID = 103,
	ERROR_RPC_INFOID = 104,
	ERROR_RPC_NOT_ALLOWED_SPEAKER = 105,
	ERROR_NOT_IMPLEMENTED = 106,
	ERROR_NOT_SUPPORTED = 107,
};

enum Tfa98xx_AgcGainInsert {
	Tfa98xx_AgcGainInsert_PreDrc = 0,
	Tfa98xx_AgcGainInsert_PostDrc,
};


/* bit8 set means tCoefA expected */
#define FEATURE1_TCOEF              0x100
/* bit9 NOT set means DRC expected */
#define FEATURE1_DRC                0x200


int tfa98xx_dsp_start(struct tfa98xx *tfa98xx, int profile, int vstep);
int tfa98xx_dsp_stop(struct tfa98xx *tfa98xx);
int tfaRunWriteBitfield(struct tfa98xx *tfa98xx, struct nxpTfaBitfield bf);
int tfaRunWriteRegister(struct tfa98xx *tfa98xx, struct nxpTfaRegpatch *reg);

int tfa98xx_dsp_patch(struct tfa98xx *tfa98xx, int patchLength, const unsigned char *patchBytes);
int tfa98xx_dsp_set_param(struct tfa98xx *tfa98xx,
		    unsigned char module_id,
		    unsigned char param_id, int num_bytes,
		    const unsigned char data[]);
int tfa98xx_dsp_biquad_disable(struct tfa98xx *tfa98xx, int biquad_index);
int tfa98xx_dsp_biquad_set_coeff(struct tfa98xx *tfa98xx, int biquad_index,
				 int len, u8 *data);
int tfa98xx_dsp_write_preset(struct tfa98xx *tfa98xx, int length,
		       const unsigned char *pPresetBytes);
int tfa98xx_dsp_power_up(struct tfa98xx *tfa98xx);
int tfa98xx_dsp_write_config(struct tfa98xx *tfa98xx, int length, const u8 *pConfigBytes);
int tfa98xx_dsp_write_speaker_parameters(struct tfa98xx *tfa98xx,
				  int length,
				  const unsigned char *pSpeakerBytes);
int tfa98xx_dsp_support_drc(struct tfa98xx *tfa98xx, int *has_drc);
int tfa98xx_dsp_set_agc_gain_insert(struct tfa98xx *tfa98xx,
				    enum Tfa98xx_AgcGainInsert
				    agcGainInsert);

int tfa98xx_set_volume(struct tfa98xx *tfa98xx, u32 voldB);
int tfa98xx_mute(struct tfa98xx *tfa98xx);
int tfa98xx_unmute(struct tfa98xx *tfa98xx);
int tfa98xx_is_pwdn(struct tfa98xx *tfa98xx);
int tfa98xx_is_amp_running(struct tfa98xx *tfa98xx);
int tfa98xx_select_mode(struct tfa98xx *tfa98xx, enum Tfa98xx_Mode mode);
int tfa98xx_write_dsp_mem(struct tfa98xx *tfa98xx, struct nxpTfaDspMem *cfmem);
int tfa98xx_write_filter(struct tfa98xx *tfa98xx, struct nxpTfaBiquadSettings *bq);
int tfa98xx_powerdown(struct tfa98xx *tfa98xx, int powerdown);

#ifdef CONFIG_SND_SOC_TFA98XX_DEBUG
int tfa98xx_dsp_msg_write(struct tfa98xx *tfa98xx, int length, const u8 *data);
int tfa98xx_dsp_msg_read(struct tfa98xx *tfa98xx, int length, u8 *data);
#endif

#endif
