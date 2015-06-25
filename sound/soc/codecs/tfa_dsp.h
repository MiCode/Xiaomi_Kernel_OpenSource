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


int tfa98xx_dsp_set_agc_gain_insert(struct tfa98xx *tfa98xx,
				    enum Tfa98xx_AgcGainInsert
				    agcGainInsert);

int tfaRunstart(struct tfa98xx *tfa98xx, int profile, int vstep);
int tfaRunSpeakerBoost(struct tfa98xx *tfa98xx, int force);
int tfaRunstop(struct tfa98xx *tfa98xx);
int tfaRunWriteBitfield(struct tfa98xx *tfa98xx,  struct nxpTfaBitfield bf);
int tfaRunWriteRegister(struct tfa98xx *tfa98xx, struct nxpTfaRegpatch *reg);
int tfa98xx_dsp_patch(struct tfa98xx *tfa98xx, int patchLength, const unsigned char *patchBytes);
int tfa98xx_dsp_set_param(struct tfa98xx *tfa98xx,
		    unsigned char module_id,
		    unsigned char param_id, int num_bytes,
		    const unsigned char data[]);
int tfa98xx_dsp_biquad_disable(struct tfa98xx *tfa98xx, int biquad_index);
int tfa98xx_dsp_write_preset(struct tfa98xx *tfa98xx, int length,
		       const unsigned char *pPresetBytes);
int tfa98xx_set_mute(struct tfa98xx *tfa98xx, enum Tfa98xx_Mute mute);
int tfa98xx_set_dca(struct tfa98xx *tfa98xx);
int tfaRunIsCold(struct tfa98xx *tfa98xx);
int tfaRunIsPwdn(struct tfa98xx *tfa98xx);
int tfaIsCalibrated(struct tfa98xx *tfa98xx);
int tfa98xx_set_volume(struct tfa98xx *tfa98xx, u32 voldB);
int tfaRunCfPowerup(struct tfa98xx *tfa98xx);
int tfaRunUnmute(struct tfa98xx *tfa98xx);
int tfaRunMuteAmplifier(struct tfa98xx *tfa98xx);
int tfa98xx_dsp_write_config(struct tfa98xx *tfa98xx, int length, const u8 *pConfigBytes);
int tfa98xx_dsp_write_speaker_parameters(struct tfa98xx *tfa98xx,
				  int length,
				  const unsigned char *pSpeakerBytes);
int tfa98xx_dsp_biquad_set_coeff(struct tfa98xx *tfa98xx, int biquad_index,
				 int len, u8* data);

#endif
