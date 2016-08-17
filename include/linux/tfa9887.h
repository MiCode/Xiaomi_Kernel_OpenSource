/*
 * include/linux/tfa9887.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_TFA9887_H
#define __LINUX_TFA9887_H

#define IN_HAND_MODE 2
#define ON_DESK_MODE 1
#define DB_CUTOFF_INDEX 12
#define MAX_DB_INDEX 15
#define PRESET_DEFAULT 4
struct tfa9887_priv {
        struct regmap *regmap;
        int irq;
        bool deviceInit;
	struct mutex lock;
};

typedef enum Tfa9887_Mute {
        Tfa9887_Mute_Off,
        Tfa9887_Mute_Digital,
        Tfa9887_Mute_Amplifier
} Tfa9887_Mute_t;


int Tfa9887_Powerdown(int powerdown);

int Powerdown(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte, int powerdown);

int Tfa9887_WriteRegister(struct tfa9887_priv *tfa9887, unsigned int subaddress, unsigned int value);

int Tfa9887_ReadRegister(struct tfa9887_priv *tfa9887, unsigned int subaddress, unsigned int *pValue);

int Tfa9887_Init(int sRate);

int Init(struct tfa9887_priv *tfa9887,struct tfa9887_priv *tfa9887_byte, int sRate);

int Tfa9887_ReadRegister(struct tfa9887_priv *tfa9887, unsigned int subaddress, unsigned int *pValue);

int Tfa9887_WriteRegister(struct tfa9887_priv *tfa9887, unsigned int subaddress, unsigned int value);

int ProcessPatchFile(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte, int length, const unsigned char *bytes);

int DspSetParam(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte, unsigned char module_id, unsigned char param_id, int num_bytes, const unsigned char *data);

int DspGetParam(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte, unsigned char module_id, unsigned char param_id, int num_bytes, unsigned char *data);
int DspWriteMem(struct tfa9887_priv *tfa9887, unsigned int address, int value);

int DspReadMem(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte, unsigned short start_offset, int num_words, int *pValues);

int coldStartup(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte, int sRate);

int loadSettings(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte);

int stereoRouting(struct tfa9887_priv *tfa9887);

int Tfa9887_SetEq(void);

int SetEq(struct tfa9887_priv *tfa9887,struct tfa9887_priv *tfa9887_byte);

int Tfa9887_SetPreset(unsigned int preset);

int SetPreset(struct tfa9887_priv *tfa9887,struct tfa9887_priv *tfa9887_byte);

int SetMute(struct tfa9887_priv *tfa9887, Tfa9887_Mute_t mute);

void calibrate (struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte, char *calibdata);

void recalibrate(struct tfa9887_priv *tfa9887, struct tfa9887_priv *tfa9887_byte);

void resetMtpEx(struct tfa9887_priv *tfa9887);

int checkMTPEX(struct tfa9887_priv *tfa9887);

void setOtc(struct tfa9887_priv *tfa9887, unsigned short otcOn);

typedef enum Tfa9887_AmpInputSel {
	Tfa9887_AmpInputSel_I2SLeft,
	Tfa9887_AmpInputSel_I2SRight,
	Tfa9887_AmpInputSel_DSP
} Tfa9887_AmpInputSel_t;

typedef enum Tfa9887_OutputSel {
	Tfa9887_I2SOutputSel_CurrentSense,
	Tfa9887_I2SOutputSel_DSP_Gain,
	Tfa9887_I2SOutputSel_DSP_AEC,
	Tfa9887_I2SOutputSel_Amp,
	Tfa9887_I2SOutputSel_DataI3R,
	Tfa9887_I2SOutputSel_DataI3L,
	Tfa9887_I2SOutputSel_DcdcFFwdCur,
} Tfa9887_OutputSel_t;

typedef enum Tfa9887_StereoGainSel {
	Tfa9887_StereoGainSel_Left,
	Tfa9887_StereoGainSel_Right
} Tfa9887_StereoGainSel_t;

#define TFA9887_SPEAKERPARAMETER_LENGTH 423
typedef unsigned char Tfa9887_SpeakerParameters_t[TFA9887_SPEAKERPARAMETER_LENGTH];

#define TFA9887_CONFIG_LENGTH 165
typedef unsigned char Tfa9887_Config_t[TFA9887_CONFIG_LENGTH];

#define TFA9887_PRESET_LENGTH    87
typedef unsigned char Tfa9887_Preset_t[TFA9887_PRESET_LENGTH];

#define TFA9887_MAXPATCH_LENGTH (3*1024)


/* the number of biquads supported */
#define TFA9887_BIQUAD_NUM              10

#define Tfa9887_Error_Ok	0

typedef enum Tfa9887_SpeakerType {
	Tfa9887_Speaker_FreeSpeaker=0,
	Tfa9887_Speaker_RA11x15,
	Tfa9887_Speaker_RA13x18,
	Tfa9887_Speaker_RA9x13,

	Tfa9887_Speaker_Max

} Tfa9887_SpeakerType_t;


typedef enum Tfa9887_Channel {
	Tfa9887_Channel_L,
	Tfa9887_Channel_R,
	Tfa9887_Channel_L_R,
	Tfa9887_Channel_Stereo
} Tfa9887_Channel_t;



typedef enum Tfa9887_SpeakerBoostStatusFlags
{
	Tfa9887_SpeakerBoost_Activity=0		,		/* Input signal activity. */
	Tfa9887_SpeakerBoost_S_Ctrl				,		/* S Control triggers the limiter */
	Tfa9887_SpeakerBoost_Muted			  ,		/* 1 when signal is muted */
	Tfa9887_SpeakerBoost_X_Ctrl 			,		/* X Control triggers the limiter */
	Tfa9887_SpeakerBoost_T_Ctrl 			,		/* T Control triggers the limiter */
	Tfa9887_SpeakerBoost_NewModel			,		/* New model is available */
	Tfa9887_SpeakerBoost_VolumeRdy		,		/* 0 means stable volume, 1 means volume is still smoothing */
	Tfa9887_SpeakerBoost_Damaged			,		/* Speaker Damage detected  */
	Tfa9887_SpeakerBoost_SignalClipping		/* Input Signal clipping detected */
} Tfa9887_SpeakerBoostStatusFlags_t ;

typedef struct Tfa9887_SpeakerBoost_StateInfo
{
	float	agcGain;			/* Current AGC Gain value */
	float	limGain;			/* Current Limiter Gain value */
	float	sMax;				  /* Current Clip/Lim threshold */
	int		T;					  /* Current Speaker Temperature value */
	int	  statusFlag;		/* Masked bit word, see Tfa9887_SpeakerBoostStatusFlags */
	float	X1;					  /* Current estimated Excursion value caused by Speakerboost gain control */
	float	X2;					  /* Current estimated Excursion value caused by manual gain setting */
	float Re;           /* Current Loudspeaker blocked resistance */
} Tfa9887_SpeakerBoost_StateInfo_t;

typedef unsigned char subaddress_t;

#define TFA9887_I2S_CONTROL    (subaddress_t)0x04
#define TFA9887_AUDIO_CONTROL  (subaddress_t)0x06
#define TFA9887_SYSTEM_CONTROL (subaddress_t)0x09
#define TFA9887_I2S_SEL        (subaddress_t)0x0A
//#define TFA9887_CF_CONTROLS    (subaddress_t)0x70 //TODO cleanup reg defs
//#define TFA9887_CF_MAD         (subaddress_t)0x71
//#define TFA9887_CF_MEM         (subaddress_t)0x72
//#define TFA9887_CF_STATUS      (subaddress_t)0x73


/* REVISION values */
#define TFA9887_REV_N1C       0x11
#define TFA9887_REV_N1D       0x12


/* I2S_CONTROL bits */
#define TFA9887_I2SCTRL_RATE_SHIFT (12)
#define TFA9887_I2SCTRL_RATE_08000 (0<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_11025 (1<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_12000 (2<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_16000 (3<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_22050 (4<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_24000 (5<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_32000 (6<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_44100 (7<<TFA9887_I2SCTRL_RATE_SHIFT)
#define TFA9887_I2SCTRL_RATE_48000 (8<<TFA9887_I2SCTRL_RATE_SHIFT)

#define TFA9887_I2SCTRL_CHANSEL_SHIFT      3
#define TFA9887_I2SCTRL_INPUT_SEL_SHIFT    6

#define TFA9887_I2SCTRL_DATAI2_SHIFT      5

#define TFA9887_I2SSEL_I2SOUT_LEFT_SHIFT  0
#define TFA9887_I2SSEL_I2SOUT_RIGHT_SHIFT 3


/* SYSTEM CONTROL bits */
#define TFA9887_SYSCTRL_POWERDOWN    (1<<0)
#define TFA9887_SYSCTRL_RESETI2C     (1<<1)
#define TFA9887_SYSCTRL_ENBL_AMP     (1<<3)
#define TFA9887_SYSCTRL_CONFIGURED   (1<<5)
#define TFA9887_SYSCTRL_SEL_ENBL_AMP (1<<6)

/* Audio control bits */
#define TFA9887_AUDIOCTRL_MUTE       (1<<5)

/* modules */
#define MODULE_SPEAKERBOOST  1

/* RPC commands */
#define PARAM_SET_LSMODEL        0x06  // Load a full model into SpeakerBoost.
#define PARAM_SET_LSMODEL_SEL    0x07  // Select one of the default models present in Tfa9887 ROM.
#define PARAM_SET_EQ			 0x0A  // 2 Equaliser Filters.
#define PARAM_SET_PRESET         0x0D  // Load a preset
#define PARAM_SET_CONFIG		 0x0E  // Load a config

#define PARAM_GET_RE0            0x85  /* gets the speaker calibration impedance (@25 degrees celsius) */
#define PARAM_GET_LSMODEL        0x86  // Gets current LoudSpeaker Model.
#define PARAM_GET_STATE					 0xC0

/* RPC Status results */
#define STATUS_OK                  0
#define STATUS_INVALID_MODULE_ID   2
#define STATUS_INVALID_PARAM_ID    3
#define STATUS_INVALID_INFO_ID     4


/* the maximum message length in the communication with the DSP */
#define MAX_PARAM_SIZE (145*3)

#define MIN(a,b) ((a)<(b)?(a):(b))
#define ROUND_DOWN(a,n) (((a)/(n))*(n))

/* maximum number of bytes in 1 I2C write transaction */
#define MAX_I2C_LENGTH			254

#define TFA9887_CF_RESET  1
/* possible memory values for DMEM in CF_CONTROLs */
typedef enum {
	Tfa9887_DMEM_PMEM=0,
	Tfa9887_DMEM_XMEM=1,
	Tfa9887_DMEM_YMEM=2,
	Tfa9887_DMEM_IOMEM=3,
} Tfa9887_DMEM_e;


#define MODULE_BIQUADFILTERBANK 2
#define BIQUAD_PARAM_SET_COEFF  1
#define BIQUAD_COEFF_SIZE       6

#define EQ_COEFF_SIZE           7

/* the number of elements in Tfa9887_SpeakerBoost_StateInfo */
#define STATE_SIZE             8

#define SPKRBST_HEADROOM			 7										/* Headroom applied to the main input signal */
#define SPKRBST_AGCGAIN_EXP			SPKRBST_HEADROOM	  /* Exponent used for AGC Gain related variables */
#define SPKRBST_TEMPERATURE_EXP     9
#define SPKRBST_LIMGAIN_EXP			    4					     /* Exponent used for Gain Corection related variables */
#define SPKRBST_TIMECTE_EXP         1



//Tfa_Registers.h
#define TFA9887_STATUS         (unsigned int)0x00

#define TFA9887_MTP            (unsigned int)0x80

/* STATUS bits */
#define TFA9887_STATUS_VDDS       (1<<0) /*  */
#define TFA9887_STATUS_PLLS       (1<<1) /* plls locked */
#define TFA9887_STATUS_OTDS       (1<<2) /*  */
#define TFA9887_STATUS_OVDS       (1<<3) /*  */
#define TFA9887_STATUS_UVDS       (1<<4) /*  */
#define TFA9887_STATUS_OCDS       (1<<5) /*  */
#define TFA9887_STATUS_CLKS       (1<<6) /* clocks stable */
//
//
#define TFA9887_STATUS_MTPB	(1<<8) /*MTP busy operation*/
#define TFA9887_STATUS_DCCS       (1<<9) /*  */

#define TFA9887_STATUS_ACS        (1<<11) /* cold started */
#define TFA9887_STATUS_SWS        (1<<12) /* amplifier switching */

/* MTP bits */
#define TFA9887_MTP_MTPOTC        (1<<0)  /* one time calibration */
#define TFA9887_MTP_MTPEX         (1<<1)  /* one time calibration done */

/*
 * generated defines
 */
#define TFA9887_STATUSREG (0x00)
#define TFA9887_BATTERYVOLTAGE (0x01)
#define TFA9887_TEMPERATURE (0x02)
#define TFA9887_I2SREG (0x04)
#define TFA9887_BAT_PROT (0x05)
#define TFA9887_AUDIO_CTR (0x06)
#define TFA9887_DCDCBOOST (0x07)
#define TFA9887_SPKR_CALIBRATION (0x08)
#define TFA9887_SYS_CTRL (0x09)
#define TFA9887_I2S_SEL_REG (0x0a)
#define TFA9887_REVISIONNUMBER (0x03)
#define TFA9887_HIDE_UNHIDE_KEY (0x40)
#define TFA9887_PWM_CONTROL (0x41)
#define TFA9887_CURRENTSENSE1 (0x46)
#define TFA9887_CURRENTSENSE2 (0x47)
#define TFA9887_CURRENTSENSE3 (0x48)
#define TFA9887_CURRENTSENSE4 (0x49)
#define TFA9887_ABISTTEST (0x4c)
#define TFA9887_RESERVE1 (0x0c)
#define TFA9887_MTP_COPY (0x62)
#define TFA9887_CF_CONTROLS (0x70)
#define TFA9887_CF_MAD (0x71)
#define TFA9887_CF_MEM (0x72)
#define TFA9887_CF_STATUS (0x73)
#define TFA9887_RESERVE2 (0x0d)

#endif
