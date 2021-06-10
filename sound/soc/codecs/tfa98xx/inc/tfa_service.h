/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
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
#include "NXP_I2C.h"
#endif

/* Linux kernel module defines TFA98XX_GIT_VERSIONS in the linux_driver/Makefile
 */
#if !defined(TFA98XX_GIT_VERSIONS)
#include "versions.h"
#endif
#ifdef TFA98XX_GIT_VERSIONS
#define TFA98XX_API_REV_STR "v6.5.5" /*TFA98XX_GIT_VERSIONS*/
#else
#define TFA98XX_API_REV_STR "v6.5.5"
#endif

#include "tfa_device.h"

/*
 * data previously defined in Tfa9888_dsp.h
 */
#define MEMTRACK_MAX_WORDS 150
#define LSMODEL_MAX_WORDS 150
#define TFA98XX_MAXTAG (150)
#define FW_VAR_API_VERSION (521)

/* Indexes and scaling factors of GetLSmodel */
#define tfa9888_fs_IDX 128
#define tfa9888_leakageFactor_IDX 130
#define tfa9888_ReCorrection_IDX 131
#define tfa9888_Bl_IDX 132
#define ReZ_IDX 147

#define tfa9872_leakageFactor_IDX 128
#define tfa9872_ReCorrection_IDX 129
#define tfa9872_Bl_IDX 130

#define fs_SCALE (double)1
#define leakageFactor_SCALE (double)8388608
#define ReCorrection_SCALE (double)8388608
#define Bl_SCALE (double)2097152
#define tCoef_SCALE (double)8388608

/* ---------------------------- Max1 ---------------------------- */
/* Headroom applied to the main input signal */
#define SPKRBST_HEADROOM 7
/* Exponent used for AGC Gain related variables */
#define SPKRBST_AGCGAIN_EXP SPKRBST_HEADROOM
#define SPKRBST_TEMPERATURE_EXP 9
/* Exponent used for Gain Corection related variables */
#define SPKRBST_LIMGAIN_EXP 4
#define SPKRBST_TIMECTE_EXP 1
#define DSP_MAX_GAIN_EXP 7
/* -------------------------------------------------------------- */

/* speaker related parameters */
#define TFA2_SPEAKERPARAMETER_LENGTH (3 * 151) /* MAX2=450 */
#define TFA1_SPEAKERPARAMETER_LENGTH (3 * 141) /* MAX1=423 */

/* vstep related parameters */
#define TFA2_ALGOPARAMETER_LENGTH \
	(3 * 304) /* N1B = (304) 305 is including the cmd-id */
#define TFA72_ALGOPARAMETER_LENGTH_MONO (3 * 183)
#define TFA72_ALGOPARAMETER_LENGTH_STEREO (3 * 356)
#define TFA2_MBDRCPARAMETER_LENGTH (3 * 152) /* 154 is including the cmd-id */
#define TFA72_MBDRCPARAMETER_LENGTH (3 * 98)
#define TFA1_PRESET_LENGTH 87
#define TFA1_DRC_LENGTH 381 /* 127 words */
#define TFA2_FILTERCOEFSPARAMETER_LENGTH \
	(3 * 168) /* 170 is including the cmd-id */
#define TFA72_FILTERCOEFSPARAMETER_LENGTH (3 * 156)

/* Maximum number of retries for DSP result
 * Keep this value low!
 * If certain calls require longer wait conditions, the
 * application should poll, not the API
 * The total wait time depends on device settings. Those
 * are application specific.
 */
#define TFA98XX_WAITRESULT_NTRIES 40
#define TFA98XX_WAITRESULT_NTRIES_LONG 2000

/* following lengths are in bytes */
#define TFA98XX_PRESET_LENGTH 87
#define TFA98XX_CONFIG_LENGTH 201
#define TFA98XX_DRC_LENGTH 381 /* 127 words */

/* Type containing all the possible errors that can occur
 *
 */
enum Tfa98xx_Error {
	Tfa98xx_Error_Ok = 0,
	Tfa98xx_Error_Device, // 1. Currently only used to keep in sync with
			      // tfa_error
	Tfa98xx_Error_Bad_Parameter, /* 2. */
	Tfa98xx_Error_Fail,    /* 3. generic failure, avoid mislead message */
	Tfa98xx_Error_NoClock, /* 4. no clock detected */
	Tfa98xx_Error_StateTimedOut,    /* 5. */
	Tfa98xx_Error_DSP_not_running,  // 6. communication with the DSP failed
	Tfa98xx_Error_AmpOn,		/* 7. amp is still running */
	Tfa98xx_Error_NotOpen,		/* 8. the given handle is not open */
	Tfa98xx_Error_InUse,		/* 9. too many handles */
	Tfa98xx_Error_Buffer_too_small, /* 10. if a buffer is too small */
	/* the expected response did not occur within the expected time */
	Tfa98xx_Error_RpcBase = 100,
	Tfa98xx_Error_RpcBusy = 101,
	Tfa98xx_Error_RpcModId = 102,
	Tfa98xx_Error_RpcParamId = 103,
	Tfa98xx_Error_RpcInvalidCC = 104,
	Tfa98xx_Error_RpcInvalidSeq = 105,
	Tfa98xx_Error_RpcInvalidParam = 106,
	Tfa98xx_Error_RpcBufferOverflow = 107,
	Tfa98xx_Error_RpcCalibBusy = 108,
	Tfa98xx_Error_RpcCalibFailed = 109,
	Tfa98xx_Error_Not_Implemented,
	Tfa98xx_Error_Not_Supported,
	Tfa98xx_Error_I2C_Fatal, /* Fatal I2C error occurred */
	/* Nonfatal I2C error, and retry count reached */
	Tfa98xx_Error_I2C_NonFatal,
	Tfa98xx_Error_Other = 1000
};

/*
 * Type containing all the possible msg returns DSP can give
 *  //TODO move to tfa_dsp_fw.h
 */
enum Tfa98xx_Status_ID {
	Tfa98xx_DSP_Not_Running = -1, /* No response from DSP */
	Tfa98xx_I2C_Req_Done = 0, // Request executed correctly and result, if
				  // any, is available for download
	Tfa98xx_I2C_Req_Busy = 1, // Request is being processed, just wait for
				  // result
	Tfa98xx_I2C_Req_Invalid_M_ID = 2, // Provided M-ID does not fit in valid
					  // rang [0..2]
	Tfa98xx_I2C_Req_Invalid_P_ID = 3, // Provided P-ID isn�t valid in the
					  // given M-ID context
	Tfa98xx_I2C_Req_Invalid_CC = 4,   // Invalid channel configuration bits
					  // (SC|DS|DP|DC) combination
	Tfa98xx_I2C_Req_Invalid_Seq = 5,  // Invalid sequence of commands, in
					  // case the DSP expects some commands
					  // in a specific order
	Tfa98xx_I2C_Req_Invalid_Param = 6,   /* Generic error */
	Tfa98xx_I2C_Req_Buffer_Overflow = 7, // I2C buffer has overflowed: host
					     // has sent too many parameters,
					     // memory integrity is not
					     // guaranteed
	Tfa98xx_I2C_Req_Calib_Busy = 8,      /* Calibration not finished */
	Tfa98xx_I2C_Req_Calib_Failed = 9     /* Calibration failed */
};

/*
 * speaker as microphone
 */
enum Tfa98xx_saam {
	Tfa98xx_saam_none, /*< SAAM feature not available */
	Tfa98xx_saam       /*< SAAM feature available */
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

#define TFA98XX_MAXPATCH_LENGTH (3 * 1024)

/* the number of biquads supported */
#define TFA98XX_BIQUAD_NUM 10

enum Tfa98xx_Channel {
	Tfa98xx_Channel_L,
	Tfa98xx_Channel_R,
	Tfa98xx_Channel_L_R,
	Tfa98xx_Channel_Stereo
};

enum Tfa98xx_Mode { Tfa98xx_Mode_Normal = 0, Tfa98xx_Mode_RCV };

enum Tfa98xx_Mute {
	Tfa98xx_Mute_Off,
	Tfa98xx_Mute_Digital,
	Tfa98xx_Mute_Amplifier
};

enum Tfa98xx_SpeakerBoostStatusFlags {
	Tfa98xx_SpeakerBoost_Activity = 0, /* Input signal activity. */
	Tfa98xx_SpeakerBoost_S_Ctrl,       /* S Control triggers the limiter */
	Tfa98xx_SpeakerBoost_Muted,	/* 1 when signal is muted */
	Tfa98xx_SpeakerBoost_X_Ctrl,       /* X Control triggers the limiter */
	Tfa98xx_SpeakerBoost_T_Ctrl,       /* T Control triggers the limiter */
	Tfa98xx_SpeakerBoost_NewModel,     /* New model is available */
	Tfa98xx_SpeakerBoost_VolumeRdy,    /* 0:stable vol, 1:still smoothing */
	Tfa98xx_SpeakerBoost_Damaged,      /* Speaker Damage detected  */
	Tfa98xx_SpeakerBoost_SignalClipping /* input clipping detected */
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
	float agcGain;  /* Current AGC Gain value */
	float limGain;  /* Current Limiter Gain value */
	float sMax;     /* Current Clip/Lim threshold */
	int T;		/* Current Speaker Temperature value */
	int statusFlag; /* Masked bit word */
	float X1;       /* estimated excursion caused by Spkrboost gain ctrl */
	float X2;       /* estimated excursion caused by manual gain setting */
	float Re;       /* Loudspeaker blocked resistance */
	/* Framework state */
	/* increments each time a MIPS problem is detected on the DSP */
	int shortOnMips;
	struct Tfa98xx_DrcStateInfo drcState; /* DRC state, when enabled */
};

struct nxpTfaMsg {
	uint8_t msg_size;
	unsigned char cmdId[3];
	int data[9];
};

struct nxp_vstep_msg {
	int fw_version;
	uint8_t no_of_vsteps;
	uint16_t reg_no;
	uint8_t *msg_reg;
	uint8_t msg_no;
	uint32_t algo_param_length;
	uint8_t *msg_algo_param;
	uint32_t filter_coef_length;
	uint8_t *msg_filter_coef;
	uint32_t mbdrc_length;
	uint8_t *msg_mbdrc;
};

struct nxpTfaGroup {
	uint8_t msg_size;
	uint8_t profileId[64];
};

struct nxpTfa98xx_Memtrack_data {
	int length;
	float mValues[MEMTRACK_MAX_WORDS];
	int mAdresses[MEMTRACK_MAX_WORDS];
	int trackers[MEMTRACK_MAX_WORDS];
	int scalingFactor[MEMTRACK_MAX_WORDS];
};

/* possible memory values for DMEM in CF_CONTROLs */
enum Tfa98xx_DMEM {
	Tfa98xx_DMEM_ERR = -1,
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
	unsigned char offset;	 /**< subaddress offset */
	unsigned short pwronDefault;  /**< register contents after poweron */
	unsigned short pwronTestmask; /**< mask of bits not test */
	char *name;		      /**< short register name */
};

enum Tfa98xx_DMEM tfa98xx_filter_mem(struct tfa_device *tfa, int filter_index,
				     unsigned short *address, int channel);

/**
 * Load the default HW settings in the device
 * @param tfa the device struct pointer
 */
enum Tfa98xx_Error tfa98xx_init(struct tfa_device *tfa);

/**
 * If needed, this function can be used to get a text version of the status ID
 * code
 * @param status the given status ID code
 * @return the I2C status ID string
 */
const char *tfa98xx_get_i2c_status_id_string(int status);

/* control the powerdown bit
 * @param tfa the device struct pointer
 * @param powerdown must be 1 or 0
 */
enum Tfa98xx_Error tfa98xx_powerdown(struct tfa_device *tfa, int powerdown);

/* indicates on which channel of DATAI2 the gain from the IC is set
 * @param tfa the device struct pointer
 * @param gain_sel, see Tfa98xx_StereoGainSel_t
 */
enum Tfa98xx_Error
tfa98xx_select_stereo_gain_channel(struct tfa_device *tfa,
				   enum Tfa98xx_StereoGainSel gain_sel);

/**
 * set the mtp with user controllable values
 * @param tfa the device struct pointer
 * @param value to be written
 * @param mask to be applied toi the bits affected
 */
enum Tfa98xx_Error tfa98xx_set_mtp(struct tfa_device *tfa, uint16_t value,
				   uint16_t mask);
enum Tfa98xx_Error tfa98xx_get_mtp(struct tfa_device *tfa, uint16_t *value);

/**
 * lock or unlock KEY2
 * lock = 1 will lock
 * lock = 0 will unlock
 * note that on return all the hidden key will be off
 */
void tfa98xx_key2(struct tfa_device *tfa, int lock);

int tfa_calibrate(struct tfa_device *tfa);
void tfa98xx_set_exttemp(struct tfa_device *tfa, short ext_temp);
short tfa98xx_get_exttemp(struct tfa_device *tfa);

/* control the volume of the DSP
 * @param vol volume in bit field. It must be between 0 and 255
 */
enum Tfa98xx_Error tfa98xx_set_volume_level(struct tfa_device *tfa,
					    unsigned short vol);

/* set the input channel to use
 * @param channel see Tfa98xx_Channel_t enumeration
 */
enum Tfa98xx_Error tfa98xx_select_channel(struct tfa_device *tfa,
					  enum Tfa98xx_Channel channel);

/* set the mode for normal or receiver mode
 * @param mode see Tfa98xx_Mode enumeration
 */
enum Tfa98xx_Error tfa98xx_select_mode(struct tfa_device *tfa,
				       enum Tfa98xx_Mode mode);

/* mute/unmute the audio
 * @param mute see Tfa98xx_Mute_t enumeration
 */
enum Tfa98xx_Error tfa98xx_set_mute(struct tfa_device *tfa,
				    enum Tfa98xx_Mute mute);

/*
 * tfa_supported_speakers - required for SmartStudio initialization
 *  returns the number of the supported speaker count
 */
enum Tfa98xx_Error tfa_supported_speakers(struct tfa_device *tfa,
					  int *spkr_count);

/**
 * Return the tfa revision
 */
void tfa98xx_rev(int *major, int *minor, int *revision);

/*
 * Return the feature bits from MTP and cnt file for comparison
 */
enum Tfa98xx_Error tfa98xx_compare_features(struct tfa_device *tfa,
					    int features_from_MTP[3],
					    int features_from_cnt[3]);

/*
 * return feature bits
 */
enum Tfa98xx_Error tfa98xx_dsp_get_sw_feature_bits(struct tfa_device *tfa,
						   int features[2]);
enum Tfa98xx_Error tfa98xx_dsp_get_hw_feature_bits(struct tfa_device *tfa,
						   int *features);

/*
 * tfa98xx_supported_saam
 *  returns the speaker as microphone feature
 * @param saam enum pointer
 *  @return error code
 */
enum Tfa98xx_Error tfa98xx_supported_saam(struct tfa_device *tfa,
					  enum Tfa98xx_saam *saam);

/* load the tables to the DSP
 *   called after patch load is done
 *   @return error code
 */
enum Tfa98xx_Error tfa98xx_dsp_write_tables(struct tfa_device *tfa,
					    int sample_rate);

/* set or clear DSP reset signal
 * @param new state
 * @return error code
 */
enum Tfa98xx_Error tfa98xx_dsp_reset(struct tfa_device *tfa, int state);

/* check the state of the DSP subsystem
 * return ready = 1 when clocks are stable to allow safe DSP subsystem access
 * @param tfa the device struct pointer
 * @param ready pointer to state flag, non-zero if clocks are not stable
 * @return error code
 */
enum Tfa98xx_Error tfa98xx_dsp_system_stable(struct tfa_device *tfa,
					     int *ready);

enum Tfa98xx_Error tfa98xx_auto_copy_mtp_to_iic(struct tfa_device *tfa);

/**
 * check the state of the DSP coolflux
 * @param tfa the device struct pointer
 * @return the value of CFE
 */
int tfa_cf_enabled(struct tfa_device *tfa);

/* The following functions can only be called when the DSP is running
 * - I2S clock must be active,
 * - IC must be in operating mode
 */

/**
 * patch the ROM code of the DSP
 * @param tfa the device struct pointer
 * @param patchLength the number of bytes of patchBytes
 * @param patchBytes pointer to the bytes to patch
 */
enum Tfa98xx_Error tfa_dsp_patch(struct tfa_device *tfa, int patchLength,
				 const unsigned char *patchBytes);

/**
 * load explicitly the speaker parameters in case of free speaker,
 * or when using a saved speaker model
 */
enum Tfa98xx_Error
tfa98xx_dsp_write_speaker_parameters(struct tfa_device *tfa, int length,
				     const unsigned char *pSpeakerBytes);

/**
 * read the speaker parameters as used by the SpeakerBoost processing
 */
enum Tfa98xx_Error
tfa98xx_dsp_read_speaker_parameters(struct tfa_device *tfa, int length,
				    unsigned char *pSpeakerBytes);

/**
 * read the current status of the DSP, typically used for development,
 * not essential to be used in a product
 */
enum Tfa98xx_Error tfa98xx_dsp_get_state_info(struct tfa_device *tfa,
					      unsigned char bytes[],
					      unsigned int *statesize);

/**
 * Check whether the DSP supports DRC
 * pbSupportDrc=1 when DSP supports DRC,
 * pbSupportDrc=0 when DSP doesn't support it
 */
enum Tfa98xx_Error tfa98xx_dsp_support_drc(struct tfa_device *tfa,
					   int *pbSupportDrc);

enum Tfa98xx_Error tfa98xx_dsp_support_framework(struct tfa_device *tfa,
						 int *pbSupportFramework);

/**
 * read the speaker excursion model as used by SpeakerBoost processing
 */
enum Tfa98xx_Error
tfa98xx_dsp_read_excursion_model(struct tfa_device *tfa, int length,
				 unsigned char *pSpeakerBytes);

/**
 * load all the parameters for a preset from a file
 */
enum Tfa98xx_Error tfa98xx_dsp_write_preset(struct tfa_device *tfa, int length,
					    const unsigned char *pPresetBytes);

/**
 * wrapper for dsp_msg that adds opcode and only writes
 */
enum Tfa98xx_Error tfa_dsp_cmd_id_write(struct tfa_device *tfa,
					unsigned char module_id,
					unsigned char param_id, int num_bytes,
					const unsigned char data[]);

/**
 * wrapper for dsp_msg that writes opcode and reads back the data
 */
enum Tfa98xx_Error tfa_dsp_cmd_id_write_read(struct tfa_device *tfa,
					     unsigned char module_id,
					     unsigned char param_id,
					     int num_bytes,
					     unsigned char data[]);

/**
 * wrapper for dsp_msg that adds opcode and 3 bytes required for coefs
 */
enum Tfa98xx_Error tfa_dsp_cmd_id_coefs(struct tfa_device *tfa,
					unsigned char module_id,
					unsigned char param_id, int num_bytes,
					unsigned char data[]);

/**
 * wrapper for dsp_msg that adds opcode and 3 bytes required for MBDrcDynamics
 */
enum Tfa98xx_Error
tfa_dsp_cmd_id_MBDrc_dynamics(struct tfa_device *tfa, unsigned char module_id,
			      unsigned char param_id, int index_subband,
			      int num_bytes, unsigned char data[]);

/**
 * Disable a certain biquad.
 * @param tfa the device struct pointer
 * @param biquad_index: 1-10 of the biquad that needs to be adressed
 */
enum Tfa98xx_Error Tfa98xx_DspBiquad_Disable(struct tfa_device *tfa,
					     int biquad_index);

/**
 * fill the calibration value as milli ohms in the struct
 * assume that the device has been calibrated
 */
enum Tfa98xx_Error tfa_dsp_get_calibration_impedance(struct tfa_device *tfa);

/*
 * return the mohm value
 */
int tfa_get_calibration_info(struct tfa_device *tfa, int channel);

/*
 * return sign extended tap pattern
 */
int tfa_get_tap_pattern(struct tfa_device *tfa);

/**
 * Reads a number of words from dsp memory
 * @param tfa the device struct pointer
 * @param subaddress write address to set in address register
 * @param pValue pointer to read data
 */
enum Tfa98xx_Error tfa98xx_read_register16(struct tfa_device *tfa,
					   unsigned char subaddress,
					   unsigned short *pValue);

/**
 * Reads a number of words from dsp memory
 * @param tfa the device struct pointer
 * @param subaddress write address to set in address register
 * @param value value to write int the memory
 */
enum Tfa98xx_Error tfa98xx_write_register16(struct tfa_device *tfa,
					    unsigned char subaddress,
					    unsigned short value);

/**
 * Initialise the dsp
 * @param tfa the device struct pointer
 * @return tfa error enum
 */
enum Tfa98xx_Error tfa98xx_init_dsp(struct tfa_device *tfa);

/**
 * Get the status of the external DSP
 * @param tfa the device struct pointer
 * @return status
 */
int tfa98xx_get_dsp_status(struct tfa_device *tfa);

/**
 * Write a command message (RPC) to the dsp
 * @param tfa the device struct pointer
 * @param num_bytes command buffer size in bytes
 * @param command_buffer
 * @return tfa error enum
 */
enum Tfa98xx_Error tfa98xx_write_dsp(struct tfa_device *tfa, int num_bytes,
				     const char *command_buffer);

/**
 * Read the result from the last message from the dsp
 * @param tfa the device struct pointer
 * @param num_bytes result buffer size in bytes
 * @param result_buffer
 * @return tfa error enum
 */
enum Tfa98xx_Error tfa98xx_read_dsp(struct tfa_device *tfa, int num_bytes,
				    unsigned char *result_buffer);

/**
 * Write a command message (RPC) to the dsp and return the result
 * @param tfa the device struct pointer
 * @param command_length command buffer size in bytes
 * @param command_buffer command buffer
 * @param result_length result buffer size in bytes
 * @param result_buffer result buffer
 * @return tfa error enum
 */
enum Tfa98xx_Error tfa98xx_writeread_dsp(struct tfa_device *tfa,
					 int command_length,
					 void *command_buffer,
					 int result_length,
					 void *result_buffer);

/**
 * Reads a number of words from dsp memory
 * @param tfa the device struct pointer
 * @param start_offset offset from where to start reading
 * @param num_words number of words to read
 * @param pValues pointer to read data
 */
enum Tfa98xx_Error tfa98xx_dsp_read_mem(struct tfa_device *tfa,
					unsigned int start_offset,
					int num_words, int *pValues);
/**
 * Write a value to dsp memory
 * @param tfa the device struct pointer
 * @param address write address to set in address register
 * @param value value to write int the memory
 * @param memtype type of memory to write to
 */
enum Tfa98xx_Error tfa98xx_dsp_write_mem_word(struct tfa_device *tfa,
					      unsigned short address, int value,
					      int memtype);

/**
 * Read data from dsp memory
 * @param tfa the device struct pointer
 * @param subaddress write address to set in address register
 * @param num_bytes number of bytes to read from dsp
 * @param data the unsigned char buffer to read data into
 */
enum Tfa98xx_Error tfa98xx_read_data(struct tfa_device *tfa,
				     unsigned char subaddress, int num_bytes,
				     unsigned char data[]);

/**
 * Write all the bytes specified by num_bytes and data to dsp memory
 * @param tfa the device struct pointer
 * @param subaddress the subaddress to write to
 * @param num_bytes number of bytes to write
 * @param data actual data to write
 */
enum Tfa98xx_Error tfa98xx_write_data(struct tfa_device *tfa,
				      unsigned char subaddress, int num_bytes,
				      const unsigned char data[]);

enum Tfa98xx_Error tfa98xx_write_raw(struct tfa_device *tfa, int num_bytes,
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
 * @param tfa the device struct pointer
 * @param memoryType indicator to the memory type
 * @param offset from where to start reading
 * @param length the number of bytes to read
 * @param bytes output data as unsigned char array
 */
enum Tfa98xx_Error tfa98xx_dsp_get_memory(struct tfa_device *tfa,
					  int memoryType, int offset,
					  int length, unsigned char bytes[]);

/**
 * Write a value to the dsp memory
 * @param tfa the device struct pointer
 * @param memoryType indicator to the memory type
 * @param offset from where to start writing
 * @param length the number of bytes to write
 * @param value the value to write to the dsp
 */
enum Tfa98xx_Error tfa98xx_dsp_set_memory(struct tfa_device *tfa,
					  int memoryType, int offset,
					  int length, int value);

enum Tfa98xx_Error
tfa98xx_dsp_write_config(struct tfa_device *tfa, int length,
			 const unsigned char *p_config_bytes);
enum Tfa98xx_Error tfa98xx_dsp_write_drc(struct tfa_device *tfa, int length,
					 const unsigned char *p_drc_bytes);

/**
 * write/read raw msg functions :
 * the buffer is provided in little endian format, each word occupying 3 bytes,
 * length is in bytes. The functions will return immediately and do not not wait
 * for DSP response.
 * @param tfa the device struct pointer
 * @param length length of the character buffer to write
 * @param buf character buffer to write
 */
enum Tfa98xx_Error tfa_dsp_msg(struct tfa_device *tfa, int length,
			       const char *buf);

/**
 * The wrapper functions to call the dsp msg, register and memory function for
 * tfa or probus
 */
enum Tfa98xx_Error dsp_msg(struct tfa_device *tfa, int length, const char *buf);
enum Tfa98xx_Error dsp_msg_read(struct tfa_device *tfa, int length,
				unsigned char *bytes);
enum Tfa98xx_Error tfa_reg_write(struct tfa_device *tfa,
				 unsigned char subaddress,
				 unsigned short value);
enum Tfa98xx_Error tfa_reg_read(struct tfa_device *tfa,
				unsigned char subaddress,
				unsigned short *value);
enum Tfa98xx_Error mem_write(struct tfa_device *tfa, unsigned short address,
			     int value, int memtype);
enum Tfa98xx_Error mem_read(struct tfa_device *tfa, unsigned int start_offset,
			    int num_words, int *pValues);

enum Tfa98xx_Error dsp_partial_coefficients(struct tfa_device *tfa,
					    uint8_t *prev, uint8_t *next);
int is_94_N2_device(struct tfa_device *tfa);
/**
 * write/read raw msg functions:
 * the buffer is provided in little endian format, each word occupying 3 bytes,
 * length is in bytes. The functions will return immediately and do not not wait
 * for DSP response. An ID is added to modify the command-ID
 * @param tfa the device struct pointer
 * @param length length of the character buffer to write
 * @param buf character buffer to write
 * @param cmdid command identifier
 */
enum Tfa98xx_Error tfa_dsp_msg_id(struct tfa_device *tfa, int length,
				  const char *buf, uint8_t cmdid[3]);

/**
 * write raw dsp msg functions
 * @param tfa the device struct pointer
 * @param length length of the character buffer to write
 * @param buffer character buffer to write
 */
enum Tfa98xx_Error tfa_dsp_msg_write(struct tfa_device *tfa, int length,
				     const char *buffer);

/**
 * write raw dsp msg functions
 * @param tfa the device struct pointer
 * @param length length of the character buffer to write
 * @param buffer character buffer to write
 * @param cmdid command identifier
 */
enum Tfa98xx_Error tfa_dsp_msg_write_id(struct tfa_device *tfa, int length,
					const char *buffer, uint8_t cmdid[3]);

/**
 * status function used by tfa_dsp_msg() to retrieve command/msg status:
 * return a <0 status of the DSP did not ACK.
 * @param tfa the device struct pointer
 * @param pRpcStatus status for remote processor communication
 */
enum Tfa98xx_Error tfa_dsp_msg_status(struct tfa_device *tfa, int *pRpcStatus);

/**
 * Read a message from dsp
 * @param tfa the device struct pointer
 * @param length number of bytes of the message
 * @param bytes pointer to unsigned char buffer
 */
enum Tfa98xx_Error tfa_dsp_msg_read(struct tfa_device *tfa, int length,
				    unsigned char *bytes);

int tfa_set_bf(struct tfa_device *tfa, const uint16_t bf, const uint16_t value);
int tfa_set_bf_volatile(struct tfa_device *tfa, const uint16_t bf,
			const uint16_t value);

/**
 * Get the value of a given bitfield
 * @param tfa the device struct pointer
 * @param bf the value indicating which bitfield
 */
int tfa_get_bf(struct tfa_device *tfa, const uint16_t bf);

/**
 * Set the value of a given bitfield
 * @param bf the value indicating which bitfield
 * @param bf_value the value of the bitfield
 * @param p_reg_value a pointer to the register where to write the bitfield
 * value
 */
int tfa_set_bf_value(const uint16_t bf, const uint16_t bf_value,
		     uint16_t *p_reg_value);

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value);
int tfa_write_reg(struct tfa_device *tfa, const uint16_t bf,
		  const uint16_t reg_value);
int tfa_read_reg(struct tfa_device *tfa, const uint16_t bf);

/* bitfield */
/**
 * get the datasheet or bitfield name corresponding to the bitfield number
 * @param num is the number for which to get the bitfield name
 * @param rev is the device type
 */
char *tfaContBfName(uint16_t num, unsigned short rev);

/**
 * get the datasheet name corresponding to the bitfield number
 * @param num is the number for which to get the bitfield name
 * @param rev is the device type
 */
char *tfaContDsName(uint16_t num, unsigned short rev);

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
 * get the bitfield number corresponding to the bitfield name, checks for all
 * devices
 * @param name is the bitfield name for which to get the bitfield number
 */
uint16_t tfaContBfEnumAny(const char *name);

#define TFA_FAM(tfa, fieldname) \
	((tfa->tfa_family == 1) ? TFA1_BF_##fieldname : TFA2_BF_##fieldname)
#define TFA_FAM_FW(tfa, fwname) \
	((tfa->tfa_family == 1) ? TFA1_FW_##fwname : TFA2_FW_##fwname)

/* set/get bit fields to HW register*/
#define TFA_SET_BF(tfa, fieldname, value) \
	tfa_set_bf(tfa, TFA_FAM(tfa, fieldname), value)
#define TFA_SET_BF_VOLATILE(tfa, fieldname, value) \
	tfa_set_bf_volatile(tfa, TFA_FAM(tfa, fieldname), value)
#define TFA_GET_BF(tfa, fieldname) tfa_get_bf(tfa, TFA_FAM(tfa, fieldname))

/* set/get bit field in variable */
#define TFA_SET_BF_VALUE(tfa, fieldname, bf_value, p_reg_value) \
	tfa_set_bf_value(TFA_FAM(tfa, fieldname), bf_value, p_reg_value)
#define TFA_GET_BF_VALUE(tfa, fieldname, reg_value) \
	tfa_get_bf_value(TFA_FAM(tfa, fieldname), reg_value)

/* write/read registers using a bit field name to determine the register address
 */
#define TFA_WRITE_REG(tfa, fieldname, value) \
	tfa_write_reg(tfa, TFA_FAM(tfa, fieldname), value)
#define TFA_READ_REG(tfa, fieldname) tfa_read_reg(tfa, TFA_FAM(tfa, fieldname))

/* FOR CALIBRATION RETRIES */
#define TFA98XX_API_WAITRESULT_NTRIES 3000 // defined in API

/**
 * run the startup/init sequence and set ACS bit
 * @param tfa the device struct pointer
 * @param state the cold start state that is requested
 */
enum Tfa98xx_Error tfaRunColdboot(struct tfa_device *tfa, int state);
enum Tfa98xx_Error tfaRunMute(struct tfa_device *tfa);
enum Tfa98xx_Error tfaRunUnmute(struct tfa_device *tfa);

/**
 * wait for calibrateDone
 * @param tfa the device struct pointer
 * @param calibrateDone pointer to status of calibration
 */
enum Tfa98xx_Error tfaRunWaitCalibration(struct tfa_device *tfa,
					 int *calibrateDone);

/**
 * run the startup/init sequence and set ACS bit
 * @param tfa the device struct pointer
 * @param profile the profile that should be loaded
 */
enum Tfa98xx_Error tfaRunColdStartup(struct tfa_device *tfa, int profile);

/**
 *  this will load the patch witch will implicitly start the DSP
 *   if no patch is available the DPS is started immediately
 * @param tfa the device struct pointer
 */
enum Tfa98xx_Error tfaRunStartDSP(struct tfa_device *tfa);

/**
 * start the clocks and wait until the AMP is switching
 * on return the DSP sub system will be ready for loading
 * @param tfa the device struct pointer
 * @param profile the profile that should be loaded on startup
 */
enum Tfa98xx_Error tfaRunStartup(struct tfa_device *tfa, int profile);

/**
 * start the maximus speakerboost algorithm
 * this implies a full system startup when the system was not already started
 * @param tfa the device struct pointer
 * @param force indicates whether a full system startup should be allowed
 * @param profile the profile that should be loaded
 */
enum Tfa98xx_Error tfaRunSpeakerBoost(struct tfa_device *tfa, int force,
				      int profile);

/**
 * Startup the device and write all files from device and profile section
 * @param tfa the device struct pointer
 * @param force indicates whether a full system startup should be allowed
 * @param profile the profile that should be loaded on speaker startup
 */
enum Tfa98xx_Error tfaRunSpeakerStartup(struct tfa_device *tfa, int force,
					int profile);

/**
 * Run calibration
 * @param tfa the device struct pointer
 */
enum Tfa98xx_Error tfaRunSpeakerCalibration(struct tfa_device *tfa);

/**
 * startup all devices. all step until patch loading is handled
 * @param tfa the device struct pointer
 */
int tfaRunStartupAll(struct tfa_device *tfa);

/**
 * powerup the coolflux subsystem and wait for it
 * @param tfa the device struct pointer
 */
enum Tfa98xx_Error tfa_cf_powerup(struct tfa_device *tfa);

/*
 * print the current device manager state
 * @param tfa the device struct pointer
 */
enum Tfa98xx_Error show_current_state(struct tfa_device *tfa);

/**
 * Init registers and coldboot dsp
 * @param tfa the device struct pointer
 */
int tfa_reset(struct tfa_device *tfa);

/**
 * Get profile from a register
 * @param tfa the device struct pointer
 */
int tfa_dev_get_swprof(struct tfa_device *tfa);

/**
 * Save profile in a register
 */
int tfa_dev_set_swprof(struct tfa_device *tfa, unsigned short new_value);

int tfa_dev_get_swvstep(struct tfa_device *tfa);

int tfa_dev_set_swvstep(struct tfa_device *tfa, unsigned short new_value);

int tfa_needs_reset(struct tfa_device *tfa);

int tfa_is_cold(struct tfa_device *tfa);

void tfa_set_query_info(struct tfa_device *tfa);

int tfa_get_pga_gain(struct tfa_device *tfa);
int tfa_set_pga_gain(struct tfa_device *tfa, uint16_t value);
int tfa_get_noclk(struct tfa_device *tfa);

/**
 * Status of used for monitoring
 * @param tfa the device struct pointer
 * @return tfa error enum
 */

enum Tfa98xx_Error tfa_status(struct tfa_device *tfa);

/*
 * function overload for flag_mtp_busy
 */
int tfa_dev_get_mtpb(struct tfa_device *tfa);

#ifdef __cplusplus
}
#endif
#endif /* TFA_SERVICE_H */
