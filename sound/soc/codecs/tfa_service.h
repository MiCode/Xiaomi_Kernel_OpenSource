/*
 *Copyright 2015 NXP Semiconductors
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

#ifndef TFA_SERVICE_H
#define TFA_SERVICE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TFA98XX_API_REV_MAJOR			(2)	/* major API rev */
#define TFA98XX_API_REV_MINOR			(9)	/* minor */
#define TFA98XX_API_REV_REVISION		(0)
#define TFA98XX_API_REV_STR			 "2.9.0"

/*
 * data previously defined in Tfa9888_dsp.h
 */
#define MEMTRACK_MAX_WORDS            50
#define LSMODEL_MAX_WORDS            150
#define TFA98XX_MAXTAG              (138)
#define FW_VAR_API_VERSION          (521)

#define fs_IDX                128
#define leakageFactor_IDX     130
#define ReCorrection_IDX      131
#define Bl_IDX                132
#define tCoef_IDX             138
#define ReZ_IDX               147

#define fs_SCALE              (double)1
#define leakageFactor_SCALE   (double)8388608
#define ReCorrection_SCALE    (double)8388608
#define Bl_SCALE              (double)2097152
#define tCoef_SCALE           (double)8388608

/* ---------------------------- Max1 ---------------------------- */
/* Headroom applied to the main input signal */
#define SPKRBST_HEADROOM		7
/* Exponent used for AGC Gain related variables */
#define SPKRBST_AGCGAIN_EXP		SPKRBST_HEADROOM
#define SPKRBST_TEMPERATURE_EXP		9
/* Exponent used for Gain Corection related variables */
#define SPKRBST_LIMGAIN_EXP		4
#define SPKRBST_TIMECTE_EXP		1
#define DSP_MAX_GAIN_EXP		7
/* -------------------------------------------------------------- */

/* speaker related parameters */
#define TFA2_SPEAKERPARAMETER_LENGTH		(3*151)	/* MAX2=450 */
#define TFA1_SPEAKERPARAMETER_LENGTH		(3*141)	/* MAX1=423 */

/* vstep related parameters */
#define TFA2_ALGOPARAMETER_LENGTH		(3*304)	/* N1B = (304) 305 is including the cmd-id */
#define TFA2_MBDRCPARAMETER_LENGTH		(3*152)	/* 154 is including the cmd-id */
#define TFA1_PRESET_LENGTH			87
#define TFA1_DRC_LENGTH				381	/* 127 words */
#define TFA2_FILTERCOEFSPARAMETER_LENGTH	(3*168) /* 170 is including the cmd-id */

/* Maximum number of retries for DSP result
 * Keep this value low!
 * If certain calls require longer wait conditions, the
 * application should poll, not the API
 * The total wait time depends on device settings. Those
 * are application specific.
 */
#define TFA98XX_WAITRESULT_NTRIES          40
#define TFA98XX_WAITRESULT_NTRIES_LONG   2000

/* following lengths are in bytes */
#define TFA98XX_PRESET_LENGTH              87

/* make full the default */
#if !(defined(TFA9887) || defined(TFA9890) || defined(TFA9887B) || defined(TFA9897))
#ifndef TFA98XX_FULL
	#define TFA98XX_FULL
#endif
#endif

#if (defined(TFA9887) || defined(TFA9890) || defined(TFA9897))
	#define TFA98XX_CONFIG_LENGTH           165
#else
#if (defined(TFA9887B) || defined(TFA98XX_FULL))
	#define TFA98XX_CONFIG_LENGTH           201
	#define TFA98XX_DRC_LENGTH              381	/* 127 words */
	typedef unsigned char Tfa98xx_DrcParameters_t[TFA98XX_DRC_LENGTH];
#endif
#endif

/*
MUST BE CONSISTANT: either one uses opaque arrays of bytes, or not!!!
*/
typedef unsigned char Tfa98xx_Config_t[TFA98XX_CONFIG_LENGTH];
typedef unsigned char Tfa98xx_Preset_t[TFA98XX_PRESET_LENGTH];

/* Type containing all the possible errors that can occur
 *
 */
enum Tfa98xx_Error {
	Tfa98xx_Error_Ok = 0,
	Tfa98xx_Error_Device,		/* Currently only used to keep in sync with tfa_error */
	Tfa98xx_Error_Bad_Parameter,
	Tfa98xx_Error_Fail,             /* generic failure, avoid mislead message */
	Tfa98xx_Error_NoClock,          /* no clock detected */
	Tfa98xx_Error_StateTimedOut,
	Tfa98xx_Error_DSP_not_running,	/* communication with the DSP failed */
	Tfa98xx_Error_AmpOn,            /* amp is still running */
	Tfa98xx_Error_NotOpen,	        /* the given handle is not open */
	Tfa98xx_Error_InUse,	        /* too many handles */
	Tfa98xx_Error_Buffer_too_small, /* if a buffer is too small */
	/* the expected response did not occur within the expected time */
	Tfa98xx_Error_RpcBase = 100,
	Tfa98xx_Error_RpcBusy = 101,
	Tfa98xx_Error_RpcModId = 102,
	Tfa98xx_Error_RpcParamId = 103,
	Tfa98xx_Error_RpcInfoId = 104,
	Tfa98xx_Error_RpcNotAllowedSpeaker = 105,

	Tfa98xx_Error_Not_Implemented,
	Tfa98xx_Error_Not_Supported,
	Tfa98xx_Error_I2C_Fatal,	/* Fatal I2C error occurred */
	/* Nonfatal I2C error, and retry count reached */
	Tfa98xx_Error_I2C_NonFatal,
	Tfa98xx_Error_Other = 1000
};

/*
 * Type containing all the possible msg returns DSP can give
 */
enum Tfa98xx_Status_ID {
	Tfa98xx_DSP_Not_Running = -1,           /* No response from DSP */
	Tfa98xx_I2C_Req_Done = 0,               /* Request executed correctly and result, if any, is available for download */
	Tfa98xx_I2C_Req_Busy = 1,               /* Request is being processed, just wait for result */
	Tfa98xx_I2C_Req_Invalid_M_ID = 2,       /* Provided M-ID does not fit in valid rang [0..2] */
	Tfa98xx_I2C_Req_Invalid_P_ID = 3,       /* Provided P-ID isn�t valid in the given M-ID context */
	Tfa98xx_I2C_Req_Invalid_CC = 4,         /* Invalid channel configuration bits (SC|DS|DP|DC) combination */
	Tfa98xx_I2C_Req_Invalid_Seq = 5,        /* Invalid sequence of commands, in case the DSP expects some commands in a specific order */
	Tfa98xx_I2C_Req_Invalid_Param = 6,      /* Generic error */
	Tfa98xx_I2C_Req_Buffer_Overflow = 7    /* I2C buffer has overflowed: host has sent too many parameters, memory integrity is not guaranteed */
};

/*
 * speaker as microphone
 */
enum Tfa98xx_saam {
	Tfa98xx_saam_none,	/*< SAAM feature not available */
	Tfa98xx_saam			/*< SAAM feature available */
};

/*
 * possible Digital Audio Interfaces bitmap
 */
enum Tfa98xx_DAI {
	Tfa98xx_DAI_I2S  =  0x01,
	Tfa98xx_DAI_TDM  =  0x02,
	Tfa98xx_DAI_PDM  =  0x04,
};

/*
 * config file subtypes
 */
enum Tfa98xx_config_type {
	Tfa98xx_config_generic,
	Tfa98xx_config_sub1,
	Tfa98xx_config_sub2,
	Tfa98xx_config_sub3,
};

enum Tfa98xx_AmpInputSel {
	Tfa98xx_AmpInputSel_I2SLeft,
	Tfa98xx_AmpInputSel_I2SRight,
	Tfa98xx_AmpInputSel_DSP
};

enum Tfa98xx_OutputSel {
	Tfa98xx_I2SOutputSel_CurrentSense,
	Tfa98xx_I2SOutputSel_DSP_Gain,
	Tfa98xx_I2SOutputSel_DSP_AEC,
	Tfa98xx_I2SOutputSel_Amp,
	Tfa98xx_I2SOutputSel_DataI3R,
	Tfa98xx_I2SOutputSel_DataI3L,
	Tfa98xx_I2SOutputSel_DcdcFFwdCur,
};

enum Tfa98xx_StereoGainSel {
	Tfa98xx_StereoGainSel_Left,
	Tfa98xx_StereoGainSel_Right
};

#define TFA98XX_MAXPATCH_LENGTH (3*1024)

/* the number of biquads supported */
#define TFA98XX_BIQUAD_NUM              10

enum Tfa98xx_Channel {
	Tfa98xx_Channel_L,
	Tfa98xx_Channel_R,
	Tfa98xx_Channel_L_R,
	Tfa98xx_Channel_Stereo
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

struct Tfa98xx_DrcStateInfo {
	float GRhighDrc1[2];
	float GRhighDrc2[2];
	float GRmidDrc1[2];
	float GRmidDrc2[2];
	float GRlowDrc1[2];
	float GRlowDrc2[2];
	float GRpostDrc1[2];
	float GRpostDrc2[2];
	float GRblDrc[2];
};
struct Tfa98xx_StateInfo {
	/* SpeakerBoost State */
	float agcGain;	/* Current AGC Gain value */
	float limGain;	/* Current Limiter Gain value */
	float sMax;	/* Current Clip/Lim threshold */
	int T;		/* Current Speaker Temperature value */
	int statusFlag;	/* Masked bit word */
	float X1;	/* estimated excursion caused by Spkrboost gain ctrl */
	float X2;	/* estimated excursion caused by manual gain setting */
	float Re;	/* Loudspeaker blocked resistance */
	/* Framework state */
	/* increments each time a MIPS problem is detected on the DSP */
	int shortOnMips;
	/* DRC state, when enabled */
	struct Tfa98xx_DrcStateInfo drcState;
};

typedef struct nxpTfaMsg {
	uint8_t msg_size;
	unsigned char cmdId[3];
	int data[9];
} nxpTfaMsg_t;

typedef struct nxpTfaGroup {
	uint8_t msg_size;
	uint8_t profileId[64];
} nxpTfaGroup_t;


struct nxpTfa98xx_Memtrack_data {
	int length;
	float mValues[MEMTRACK_MAX_WORDS];
	int mAdresses[MEMTRACK_MAX_WORDS];
	int scalingFactor[MEMTRACK_MAX_WORDS];
	int trackers[MEMTRACK_MAX_WORDS];
};

/* possible memory values for DMEM in CF_CONTROLs */
enum Tfa98xx_DMEM {
	Tfa98xx_DMEM_PMEM = 0,
	Tfa98xx_DMEM_XMEM = 1,
	Tfa98xx_DMEM_YMEM = 2,
	Tfa98xx_DMEM_IOMEM = 3,
};

/**
 * lookup the device type and return the family type
 */
int tfa98xx_dev2family(int dev_type);

/**
 *  register definition structure
 */
struct regdef {
	unsigned char offset; /**< subaddress offset */
	unsigned short pwronDefault;
			      /**< register contents after poweron */
	unsigned short pwronTestmask;
			      /**< mask of bits not test */
	char *name;	      /**< short register name */
};

#define Tfa98xx_handle_t int

/**
 * Open the instance handle
 */
enum Tfa98xx_Error tfa98xx_open(Tfa98xx_handle_t handle);

/**
 * Load the default HW settings in the device
 */
enum Tfa98xx_Error tfa98xx_init(Tfa98xx_handle_t handle);

/**
 * Return the tfa revision
 */
void tfa98xx_rev(int *major, int *minor, int *revision);

enum Tfa98xx_DMEM tfa98xx_filter_mem(Tfa98xx_handle_t dev,
	int filter_index, unsigned short *address, int channel);

/**
 * Return the maximum nr of devices
 */
int tfa98xx_max_devices(void);

/**
 * If needed, this function can be used to get a text version of the status ID code
 * @param the given status ID code
 * @return the I2C status ID string
 */
const char *tfa98xx_get_i2c_status_id_string(int status);

/**
 * Close the instance handle
 */
enum Tfa98xx_Error tfa98xx_close(Tfa98xx_handle_t handle);

/* control the powerdown bit of the TFA9887
 * @param powerdown must be 1 or 0
 */
enum Tfa98xx_Error tfa98xx_powerdown(Tfa98xx_handle_t handle,
	int powerdown);

/* control the input_sel bits of the TFA9887, to indicate */
/* what is sent to the amplfier and speaker
 * @param input_sel, see Tfa98xx_AmpInputSel_t
 */
enum Tfa98xx_Error tfa98xx_select_amplifier_input(Tfa98xx_handle_t handle,
	enum Tfa98xx_AmpInputSel input_sel);

/* control the I2S left output of the TFA9887
 * @param output_sel, see Tfa98xx_OutputSel_t
 */
enum Tfa98xx_Error tfa98xx_select_i2s_output_left(Tfa98xx_handle_t handle,
	enum Tfa98xx_OutputSel output_sel);

/* control the I2S right output of the TFA9887
 * @param output_sel, see Tfa98xx_OutputSel_t
 */
enum Tfa98xx_Error tfa98xx_select_i2s_output_right(Tfa98xx_handle_t handle,
	enum Tfa98xx_OutputSel output_sel);

/* indicates on which channel of DATAI2 the gain from the IC is set
 * @param gain_sel, see Tfa98xx_StereoGainSel_t
 */
enum Tfa98xx_Error tfa98xx_select_stereo_gain_channel(Tfa98xx_handle_t handle,
	enum Tfa98xx_StereoGainSel gain_sel);

/* TODO cleanup calibration support */

/**
 * set the mtp with user controllable values
 * @param value to be written
 * @param mask to be applied toi the bits affected
 */
enum Tfa98xx_Error tfa98xx_set_mtp(Tfa98xx_handle_t handle, uint16_t value, uint16_t mask);

enum Tfa98xx_Error tfa98xx_get_mtp(Tfa98xx_handle_t handle, uint16_t *value);

/**
 * lock or unlock KEY2
 * lock = 1 will lock
 * lock = 0 will unlock
 * note that on return all the hidden key will be off
 */
void tfa98xx_key2(Tfa98xx_handle_t handle, int lock);

int tfa_calibrate(Tfa98xx_handle_t handle) ;
void tfa98xx_set_exttemp(Tfa98xx_handle_t handle, short ext_temp);
short tfa98xx_get_exttemp(Tfa98xx_handle_t handle);

/* control the volume of the DSP
 * @param vol volume in bit field. It must be between 0 and 255
 */
enum Tfa98xx_Error tfa98xx_set_volume_level(Tfa98xx_handle_t handle,
	unsigned short vol);

/* read the TFA9887 of the sample rate of the I2S bus that will be used.
 * @param pRate pointer to rate in Hz i.e 32000, 44100 or 48000
 */
enum Tfa98xx_Error tfa98xx_get_sample_rate(Tfa98xx_handle_t handle,
	int *pRate);

/* set the input channel to use
 * @param channel see Tfa98xx_Channel_t enumeration
 */
enum Tfa98xx_Error tfa98xx_select_channel(Tfa98xx_handle_t handle,
	enum Tfa98xx_Channel channel);

/* set the mode for normal or receiver mode
 * @param mode see Tfa98xx_Mode enumeration
 */
enum Tfa98xx_Error tfa98xx_select_mode(Tfa98xx_handle_t handle, enum Tfa98xx_Mode mode);

/* mute/unmute the audio
 * @param mute see Tfa98xx_Mute_t enumeration
 */
enum Tfa98xx_Error tfa98xx_set_mute(Tfa98xx_handle_t handle,
	enum Tfa98xx_Mute mute);

/*
 * tfa98xx_supported_speakers - required for SmartStudio initialization
 *  returns the number of the supported speaker count
 */
enum Tfa98xx_Error tfa98xx_supported_speakers(Tfa98xx_handle_t handle, int *spkr_count);

/*
 * Return the feature bits from MTP and cnt file for comparison
 */
enum Tfa98xx_Error
tfa98xx_compare_features(Tfa98xx_handle_t handle, int features_from_MTP[3], int features_from_cnt[3]);

/*
 * return feature bits
 */
enum Tfa98xx_Error
tfa98xx_dsp_get_sw_feature_bits(Tfa98xx_handle_t handle, int features[2]);
enum Tfa98xx_Error
tfa98xx_dsp_get_hw_feature_bits(Tfa98xx_handle_t handle, int *features);
/*
 * tfa98xx_supported_dai
 *  returns the bitmap of the supported Digital Audio Interfaces
 * @param dai bitmap enum pointer
 *  @return error code
 */
enum Tfa98xx_Error tfa98xx_supported_dai(Tfa98xx_handle_t handle, enum Tfa98xx_DAI *daimap);

/*
 * tfa98xx_supported_saam
 *  returns the speaker as microphone feature
 * @param saam enum pointer
 *  @return error code
 */
enum Tfa98xx_Error tfa98xx_supported_saam(Tfa98xx_handle_t handle, enum Tfa98xx_saam *saam);

/* load the tables to the DSP
 *   called after patch load is done
 *   @return error code
 */
enum Tfa98xx_Error tfa98xx_dsp_write_tables(Tfa98xx_handle_t handle, int sample_rate);

/* set or clear DSP reset signal
 * @param new state
 * @return error code
 */
enum Tfa98xx_Error tfa98xx_dsp_reset(Tfa98xx_handle_t handle, int state);

/* check the state of the DSP subsystem
 * return ready = 1 when clocks are stable to allow safe DSP subsystem access
 * @param pointer to state flag, non-zero if clocks are not stable
 * @return error code
 */
enum Tfa98xx_Error tfa98xx_dsp_system_stable(Tfa98xx_handle_t handle,
	int *ready);

/**
 * check the state of the DSP coolflux
 * returns the value of CFE
 */
int tfa98xx_cf_enabled(Tfa98xx_handle_t dev_idx);

/* The following functions can only be called when the DSP is running
 * - I2S clock must be active,
 * - IC must be in operating mode
 */

/**
 * patch the ROM code of the DSP
 * @param handle to opened instance
 * @param patchLength the number of bytes of patchBytes
 * @param patchBytes pointer to the bytes to patch
 */
enum Tfa98xx_Error tfa_dsp_patch(Tfa98xx_handle_t handle,
	int patchLength, const unsigned char *patchBytes);

/* Check whether the DSP expects tCoef or tCoefA as last parameter in
 * the speaker parameters
 * *pbSupporttCoef=1 when DSP expects tCoef,
 * *pbSupporttCoef=0 when it expects tCoefA (and the elaborate workaround
 * to calculate tCoefA from tCoef on the host)
 */
enum Tfa98xx_Error tfa98xx_dsp_support_tcoef(Tfa98xx_handle_t handle,
	int *pbSupporttCoef);

/**
 * return the tfa device family id
 */
int tfa98xx_dev_family(Tfa98xx_handle_t dev_idx);

/**
 * return the device revision id
 */
unsigned short tfa98xx_dev_revision(Tfa98xx_handle_t dev_idx);

/**
 * load explicitly the speaker parameters in case of free speaker,
 * or when using a saved speaker model
 */
enum Tfa98xx_Error tfa98xx_dsp_write_speaker_parameters(
	Tfa98xx_handle_t handle, int length, const unsigned char *pSpeakerBytes);

/**
 * read the speaker parameters as used by the SpeakerBoost processing
 */
enum Tfa98xx_Error tfa98xx_dsp_read_speaker_parameters(
	Tfa98xx_handle_t handle, int length,
	unsigned char *pSpeakerBytes);

/**
 * read the current status of the DSP, typically used for development,
 * not essential to be used in a product
 */
enum Tfa98xx_Error tfa98xx_dsp_get_state_info(
	Tfa98xx_handle_t handle, unsigned char bytes[],
	unsigned int *statesize);

/**
 * Check whether the DSP supports DRC
 * pbSupportDrc=1 when DSP supports DRC,
 * pbSupportDrc=0 when DSP doesn't support it
 */
enum Tfa98xx_Error tfa98xx_dsp_support_drc(Tfa98xx_handle_t handle,
	int *pbSupportDrc);

enum Tfa98xx_Error
tfa98xx_dsp_support_framework(Tfa98xx_handle_t handle, int *pbSupportFramework);

/**
 * read the speaker excursion model as used by SpeakerBoost processing
 */
enum Tfa98xx_Error tfa98xx_dsp_read_excursion_model(
	Tfa98xx_handle_t handle, int length,
	unsigned char *pSpeakerBytes);

/**
 * load all the parameters for a preset from a file
 */
enum Tfa98xx_Error tfa98xx_dsp_write_preset(Tfa98xx_handle_t handle,
	int length, const unsigned char *pPresetBytes);

/**
 * wrapper for dsp_msg that adds opcode and only writes
 */
enum Tfa98xx_Error tfa_dsp_cmd_id_write(Tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
			   const unsigned char data[]);

/**
 * wrapper for dsp_msg that writes opcode and reads back the data
 */
enum Tfa98xx_Error tfa_dsp_cmd_id_write_read(Tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
			   unsigned char data[]);

/**
 * wrapper for dsp_msg that adds opcode and 3 bytes required for coefs
 */
enum Tfa98xx_Error tfa_dsp_cmd_id_coefs(Tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int num_bytes,
			   unsigned char data[]);

/**
 * wrapper for dsp_msg that adds opcode and 3 bytes required for MBDrcDynamics
 */
enum Tfa98xx_Error tfa_dsp_cmd_id_MBDrc_dynamics(Tfa98xx_handle_t handle,
			   unsigned char module_id,
			   unsigned char param_id, int index_subband,
			   int num_bytes, unsigned char data[]);

/**
 * Disable a certain biquad.
 * @param handle to opened instance
 * @param biquad_index: 1-10 of the biquad that needs to be adressed
*/
enum Tfa98xx_Error Tfa98xx_DspBiquad_Disable(Tfa98xx_handle_t handle,
	int biquad_index);

/**
 * fill the calibration value as milli ohms in the struct
 * assume that the device has been calibrated
 */
enum Tfa98xx_Error
tfa_dsp_get_calibration_impedance(Tfa98xx_handle_t handle);

/*
 * return the mohm value
 */
int tfa_get_calibration_info(Tfa98xx_handle_t handle, int channel);

/**
 * Reads a number of words from dsp memory
 * @param handle to opened instance
 * @param subaddress write address to set in address register
 * @param pValue pointer to read data
*/
enum Tfa98xx_Error tfa98xx_read_register16(Tfa98xx_handle_t handle,
				       unsigned char subaddress,
				       unsigned short *pValue);

/**
 * Reads a number of words from dsp memory
 * @param handle to opened instance
 * @param subaddress write address to set in address register
 * @param value value to write int the memory
*/
enum Tfa98xx_Error tfa98xx_write_register16(Tfa98xx_handle_t handle,
					unsigned char subaddress,
					unsigned short value);

/**
 * Reads a number of words from dsp memory
 * @param handle to opened instance
 * @param start_offset offset from where to start reading
 * @param num_words number of words to read
 * @param pValues pointer to read data
*/
enum Tfa98xx_Error tfa98xx_dsp_read_mem(Tfa98xx_handle_t handle,
				   unsigned int start_offset,
				   int num_words, int *pValues);

/**
 * Write a value to dsp memory
 * @param handle to opened instance
 * @param address write address to set in address register
 * @param value value to write int the memory
 * @param memtype type of memory to write to
*/
enum Tfa98xx_Error tfa98xx_dsp_write_mem_word(Tfa98xx_handle_t handle,
	unsigned short address, int value, int memtype);

/**
 * Read data from dsp memory
 * @param handle to opened instance
 * @param subaddress write address to set in address register
 * @param num_bytes number of bytes to read from dsp
 * @param data the unsigned char buffer to read data into
*/
enum Tfa98xx_Error tfa98xx_read_data(Tfa98xx_handle_t handle,
			unsigned char subaddress,
			int num_bytes, unsigned char data[]);

/**
 * Write all the bytes specified by num_bytes and data to dsp memory
 * @param handle to opened instance
 * @param subaddress the subaddress to write to
 * @param num_bytes number of bytes to write
 * @param data actual data to write
*/
enum Tfa98xx_Error tfa98xx_write_data(Tfa98xx_handle_t handle,
			unsigned char subaddress,
			int num_bytes,
			const unsigned char data[]);

enum Tfa98xx_Error tfa98xx_write_raw(Tfa98xx_handle_t handle,
				int num_bytes,
				const unsigned char data[]);

/* support for converting error codes into text */
const char *tfa98xx_get_error_string(enum Tfa98xx_Error error);

/**
 * convert signed 24 bit integers to 32bit aligned bytes
 * input:   data contains "num_bytes/3" int24 elements
 * output:  bytes contains "num_bytes" byte elements
 * @param num_data length of the input data array
 * @param data input data as integer array
 * @param bytes output data as unsigned char array
*/
void tfa98xx_convert_data2bytes(int num_data, const int data[],
			unsigned char bytes[]);

/* return the device revision id
 */
unsigned short tfa98xx_get_device_revision(Tfa98xx_handle_t handle);

/**
 * return the device digital audio interface (DAI) type bitmap
 */
enum Tfa98xx_DAI tfa98xx_get_device_dai(Tfa98xx_handle_t handle);

/**
 * convert memory bytes to signed 24 bit integers
 * input:  bytes contains "num_bytes" byte elements
 * output: data contains "num_bytes/3" int24 elements
 * @param num_bytes length of the input data array
 * @param bytes input data as unsigned char array
 * @param data output data as integer array
*/
void tfa98xx_convert_bytes2data(int num_bytes, const unsigned char bytes[],
			int data[]);

/**
 * Read a part of the dsp memory
 * @param handle to opened instance
 * @param memoryType indicator to the memory type
 * @param offset from where to start reading
 * @param length the number of bytes to read
 * @param bytes output data as unsigned char array
*/
enum Tfa98xx_Error tfa98xx_dsp_get_memory(Tfa98xx_handle_t handle, int memoryType,
	int offset, int length, unsigned char bytes[]);

/**
 * Write a value to the dsp memory
 * @param handle to opened instance
 * @param memoryType indicator to the memory type
 * @param offset from where to start writing
 * @param length the number of bytes to write
 * @param value the value to write to the dsp
*/
enum Tfa98xx_Error tfa98xx_dsp_set_memory(Tfa98xx_handle_t handle, int memoryType,
	int offset, int length, int value);

enum Tfa98xx_Error tfa98xx_dsp_write_config(Tfa98xx_handle_t handle, int length, const unsigned char *p_config_bytes);
enum Tfa98xx_Error tfa98xx_dsp_write_drc(Tfa98xx_handle_t handle, int length, const unsigned char *p_drc_bytes);

/**
 * write/read raw msg functions :
 * the buffer is provided in little endian format, each word occupying 3 bytes, length is in bytes.
 * The functions will return immediately and do not not wait for DSP reponse.
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buf character buffer to write
*/
enum Tfa98xx_Error tfa_dsp_msg(Tfa98xx_handle_t handle, int length, const char *buf);

/**
 * write/read raw msg functions:
 * the buffer is provided in little endian format, each word occupying 3 bytes, length is in bytes.
 * The functions will return immediately and do not not wait for DSP reponse.
 * An ID is added to modify the command-ID
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buf character buffer to write
 * @param cmdid command identifier
*/
enum Tfa98xx_Error tfa_dsp_msg_id(Tfa98xx_handle_t handle, int length, const char *buf, uint8_t cmdid[3]);

/**
 * write raw dsp msg functions
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buffer character buffer to write
*/
enum Tfa98xx_Error tfa_dsp_msg_write(Tfa98xx_handle_t handle, int length, const char *buffer);

/**
 * write raw dsp msg functions
 * @param handle to opened instance
 * @param length length of the character buffer to write
 * @param buf character buffer to write
 * @param cmdid command identifier
*/
enum Tfa98xx_Error tfa_dsp_msg_write_id(Tfa98xx_handle_t handle, int length, const char *buffer, uint8_t cmdid[3]);

/**
 * status function used by tfa_dsp_msg() to retrieve command/msg status:
 * return a <0 status of the DSP did not ACK.
 * @param handle to opened instance
 * @param pRpcStatus status for remote processor communication
*/
enum Tfa98xx_Error tfa_dsp_msg_status(Tfa98xx_handle_t handle, int *pRpcStatus);

/**
 * Read a message from dsp
 * @param length number of bytes of the message
 * @param bytes pointer to unsigned char buffer
*/
enum Tfa98xx_Error tfa_dsp_msg_read(Tfa98xx_handle_t handle, int length, unsigned char *bytes);

void create_dsp_buffer_msg(nxpTfaMsg_t *msg, char *buffer, int *size);

int tfa_set_bf(Tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t value);

/**
 * Get the value of a given bitfield
 * @param dev_idx this is the device index
 * @param bf the value indicating which bitfield
 */
int tfa_get_bf(Tfa98xx_handle_t dev_idx, const uint16_t bf);

/**
 * Set the value of a given bitfield
 * @param bf the value indicating which bitfield
 * @param bf_value the value of the bitfield
 * @param p_reg_value a pointer to the register where to write the bitfield value
 */
int tfa_set_bf_value(const uint16_t bf, const uint16_t bf_value, uint16_t *p_reg_value);

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value);
int tfa_write_reg(Tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t reg_value);
int tfa_read_reg(Tfa98xx_handle_t dev_idx, const uint16_t bf);

/* bitfield */
/**
 * get the datasheet name corresponding to the bitfield number
 * @param num is the number for which to get the bitfield name
 * @param rev is the device type
 */
char *tfaContBfName(uint16_t num, unsigned short rev);

/**
 * get the bitfield name corresponding to the bitfield number
 * @param num is the number for which to get the bitfield name
 * @param rev is the device type
 */
char *tfaContBitName(uint16_t num, unsigned short rev);

/**
 * get the bitfield number corresponding to the bitfield name
 * @param name is the bitfield name for which to get the bitfield number
 * @param rev is the device type
 */
uint16_t tfaContBfEnum(const char *name, unsigned short rev);

/**
* get the bitfield number corresponding to the bitfield name, checks for all devices
* @param name is the bitfield name for which to get the bitfield number
 */
uint16_t tfaContBfEnumAny(const char *name);

#define TFA_FAM(dev_idx, fieldname) ((tfa98xx_dev_family(dev_idx) == 1) ? TFA1_BF_##fieldname :  TFA2_BF_##fieldname)
#define TFA_FAM_FW(dev_idx, fwname) ((tfa98xx_dev_family(dev_idx) == 1) ? TFA1_FW_##fwname :  TFA2_FW_##fwname)

/* set/get bit fields to HW register*/
#define TFA_SET_BF(dev_idx, fieldname, value) tfa_set_bf(dev_idx, TFA_FAM(dev_idx, fieldname), value)
#define TFA_GET_BF(dev_idx, fieldname) tfa_get_bf(dev_idx, TFA_FAM(dev_idx, fieldname))

/* set/get bit field in variable */
#define TFA_SET_BF_VALUE(dev_idx, fieldname, bf_value, p_reg_value) tfa_set_bf_value(TFA_FAM(dev_idx, fieldname), bf_value, p_reg_value)
#define TFA_GET_BF_VALUE(dev_idx, fieldname, reg_value) tfa_get_bf_value(TFA_FAM(dev_idx, fieldname), reg_value)

/* write/read registers using a bit field name to determine the register address */
#define TFA_WRITE_REG(dev_idx, fieldname, value) tfa_write_reg(dev_idx, TFA_FAM(dev_idx, fieldname), value)
#define TFA_READ_REG(dev_idx, fieldname) tfa_read_reg(dev_idx, TFA_FAM(dev_idx, fieldname))

/* FOR CALIBRATION RETRIES */
#define TFA98XX_API_WAITRESULT_NTRIES 3000

/**
 * run the startup/init sequence and set ACS bit
 * @param state the cold start state that is requested
 */
enum Tfa98xx_Error tfaRunColdboot(Tfa98xx_handle_t handle, int state);
enum Tfa98xx_Error tfaRunMute(Tfa98xx_handle_t handle);
enum Tfa98xx_Error tfaRunUnmute(Tfa98xx_handle_t handle);

/**
 * wait for calibrateDone
 * @param calibrateDone pointer to status of calibration
 */
enum Tfa98xx_Error tfaRunWaitCalibration(Tfa98xx_handle_t handle, int *calibrateDone);

/**
 * run the startup/init sequence and set ACS bit
 * @param profile the profile that should be loaded
 */
enum Tfa98xx_Error tfaRunColdStartup(Tfa98xx_handle_t handle, int profile);

/**
 *  this will load the patch witch will implicitly start the DSP
 *   if no patch is available the DPS is started immediately
 */
enum Tfa98xx_Error tfaRunStartDSP(Tfa98xx_handle_t handle);

/**
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 * @param profile the profile that should be loaded on startup
 */
enum Tfa98xx_Error tfaRunStartup(Tfa98xx_handle_t handle, int profile);

/**
 * start the maximus speakerboost algorithm
 * this implies a full system startup when the system was not already started
 * @param force indicates wether a full system startup should be allowed
 * @param profile the profile that should be loaded
 */
enum Tfa98xx_Error tfaRunSpeakerBoost(Tfa98xx_handle_t handle, int force, int profile);

/**
 * Startup the device and write all files from device and profile section
 * @param force indicates wether a full system startup should be allowed
 * @param profile the profile that should be loaded on speaker startup
 */
enum Tfa98xx_Error tfaRunSpeakerStartup(Tfa98xx_handle_t handle, int force, int profile);

/**
 * Run calibration
 * @param profile the profile that should be loaded
 */
enum Tfa98xx_Error tfaRunSpeakerCalibration(Tfa98xx_handle_t handle);

/**
 * startup all devices. all step until patch loading is handled
 */
int tfaRunStartupAll(Tfa98xx_handle_t *handles);

/**
 * powerup the coolflux subsystem and wait for it
 */
enum Tfa98xx_Error tfa_cf_powerup(Tfa98xx_handle_t handle);

/*
 * print the current device manager state
 */
enum Tfa98xx_Error show_current_state(Tfa98xx_handle_t handle);

/**
 * set verbosity level
 */
void tfa_verbose(int level);

/**
 * Init registers and coldboot dsp
 */
int tfa98xx_reset(Tfa98xx_handle_t handle);

/**
 *
 * @param dev_idx is the device index
 * @param revid is the revision id
 */
enum Tfa98xx_Error tfa_soft_probe(int dev_idx, int revid);

/**
 * Get profile from a register
 */
int tfa_get_swprof(Tfa98xx_handle_t handle);

/**
 * Save profile in a register
 */
int tfa_set_swprof(Tfa98xx_handle_t handle, unsigned short new_value);

int tfa_get_swvstep(Tfa98xx_handle_t handle);

int tfa_set_swvstep(Tfa98xx_handle_t handle, unsigned short new_value);

int tfa98xx_is_amp_running(Tfa98xx_handle_t handle);

#ifdef __cplusplus
}
#endif
#endif				/* TFA_SERVICE_H */
