/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFA98XXPARAMETERS_H_
#define TFA98XXPARAMETERS_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "tfa_service.h"

#if (defined(WIN32) || defined(_X64))


#pragma warning(disable : 4200)
#pragma warning(disable : 4214)
#endif

/*
 * profiles & volumesteps
 *
 */
#define TFA_MAX_PROFILES (64)
#define TFA_MAX_VSTEPS (64)

#define TFA_MAX_VSTEP_MSG_MARKER (100)
#define TFA_MAX_MSGS (10)





#pragma pack(push, 1)

struct uint24 {
	uint8_t b[3];
};
/*
 * the generic header
 *   all char types are in ASCII
 */
struct nxpTfaHeader {
	uint16_t id;
	char version[2];
	char subversion[2];
	uint16_t size;
	uint32_t CRC;
	char customer[8];
	char application[8];
	char type[8];
};

enum nxpTfaSamplerate {
	fs_8k,
	fs_11k025,
	fs_12k,
	fs_16k,
	fs_22k05,
	fs_24k,
	fs_32k,
	fs_44k1,
	fs_48k,
	fs_96k,
	fs_count
};


static const int nxpTfaSamplerateHz[fs_count] = { 8000,  11025, 12000, 16000,
						  22050, 24000, 32000, 44100,
						  48000, 96000 };

/*
 * coolflux direct memory access
 */
struct nxpTfaDspMem {
	uint8_t type;     /* 0--3: p, x, y, iomem */
	uint16_t address; /* target address */
	uint8_t size;     /* data size in words */
	int words[]; /* payload  in signed 32bit integer (two's complement) */
};

/*
 * the biquad coefficients for the API together with index in filter
 *  the biquad_index is the actual index in the equalizer +1
 */
#define BIQUAD_COEFF_SIZE 6

/*
 * Output fixed point coeffs structure
 */
struct nxpTfaBiquad {
	int a2;
	int a1;
	int b2;
	int b1;
	int b0;
};

struct nxpTfaBiquadOld {
	uint8_t bytes[BIQUAD_COEFF_SIZE * sizeof(struct uint24)];
};

struct nxpTfaBiquadFloat {
	float headroom;
	float b0;
	float b1;
	float b2;
	float a1;
	float a2;
};

/*
 * EQ filter definitions
 * Note: This is not in line with smartstudio (JV: 12/12/2016)
 */
enum nxpTfaFilterType {
	fCustom,
	fFlat,
	fLowpass,
	fHighpass,
	fLowshelf,
	fHighshelf,
	fNotch,
	fPeak,
	fBandpass,
	f1stLP,
	f1stHP,
	fElliptic
};

/*
 * filter parameters for biquad (re-)calculation
 */
struct nxpTfaFilter {
	struct nxpTfaBiquadOld biquad;
	uint8_t enabled;
	uint8_t type;
	float frequency;
	float Q;
	float gain;
};

/*
 * biquad params for calculation
 */

#define TFA_BQ_EQ_INDEX 0
#define TFA_BQ_ANTI_ALIAS_INDEX 10
#define TFA_BQ_INTEGRATOR_INDEX 13

/*
 * Loudspeaker Compensation filter definitions
 */
struct nxpTfaLsCompensationFilter {
	struct nxpTfaBiquad biquad;
	uint8_t lsCompOn;


	uint8_t bwExtOn;
	float fRes;
	float Qt;
	float fBwExt;
	float samplingFreq;
};

/*
 * Anti Aliasing Elliptic filter definitions
 */
struct nxpTfaAntiAliasFilter {
	struct nxpTfaBiquad biquad; /**< Output results fixed point coeffs */
	uint8_t enabled;
	float cutOffFreq;
	float samplingFreq;
	float rippleDb;
	float rolloff;
};

/**
 * Integrator filter input definitions
 */
struct nxpTfaIntegratorFilter {
	struct nxpTfaBiquad biquad; /**< Output results fixed point coeffs */
	uint8_t type;       /**< Butterworth filter type: high or low pass */
	float cutOffFreq;
	float samplingFreq; /**< sampling frequency in Hertz */
	float leakage;      /**< leakage factor; range [0.0 1.0] */
};

struct nxpTfaEqFilter {
	struct nxpTfaBiquad biquad;
	uint8_t enabled;
	uint8_t type;
	float cutOffFreq;
	float samplingFreq;
	float Q;
	float gainDb;
};

struct nxpTfaContAntiAlias {
	int8_t index;

	uint8_t type;
	float cutOffFreq;
	float samplingFreq;
	float rippleDb;
	float rolloff;
	uint8_t bytes[5 * 3];
};

struct nxpTfaContIntegrator {
	int8_t index;

	uint8_t type;
	float cutOffFreq;
	float samplingFreq;
	float leakage;
	float reserved;
	uint8_t bytes[5 * 3];
};

struct nxpTfaContEq {
	int8_t index;
	uint8_t type;
	float cutOffFreq;
	float samplingFreq;
	float Q;
	float gainDb;
	uint8_t bytes[5 * 3];
};

union nxpTfaContBiquad {
	struct nxpTfaContEq eq;
	struct nxpTfaContAntiAlias aa;
	struct nxpTfaContIntegrator in;
};

#define TFA_BQ_EQ_INDEX 0
#define TFA_BQ_ANTI_ALIAS_INDEX 10
#define TFA_BQ_INTEGRATOR_INDEX 13
#define TFA98XX_MAX_EQ 10

struct nxpTfaEqualizer {
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];
};

/*
 * files
 */
#define HDR(c1, c2) (c2 << 8 | c1)
enum nxpTfaHeaderType {
	paramsHdr = HDR('P', 'M'), /* containter file */
	volstepHdr = HDR('V', 'P'),
	patchHdr = HDR('P', 'A'),
	speakerHdr = HDR('S', 'P'),
	presetHdr = HDR('P', 'R'),
	configHdr = HDR('C', 'O'),
	equalizerHdr = HDR('E', 'Q'),
	drcHdr = HDR('D', 'R'),
	msgHdr = HDR('M', 'G'), /* generic message */
	infoHdr = HDR('I', 'N')
};

/*
 * equalizer file
 */
#define NXPTFA_EQ_VERSION '1'
#define NXPTFA_EQ_SUBVERSION "00"
struct nxpTfaEqualizerFile {
	struct nxpTfaHeader hdr;
	uint8_t samplerate;
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];

};

/*
 * patch file
 */
#define NXPTFA_PA_VERSION '1'
#define NXPTFA_PA_SUBVERSION "00"
struct nxpTfaPatchFile {
	struct nxpTfaHeader hdr;
	uint8_t data[];
};

/*
 * generic message file
 *   -  the payload of this file includes the opcode and is send straight to the
 * DSP
 */
#define NXPTFA_MG_VERSION '3'
#define NXPTFA_MG_SUBVERSION "00"
struct nxpTfaMsgFile {
	struct nxpTfaHeader hdr;
	uint8_t data[];
};

/*
 * NOTE the tfa98xx API defines the enum Tfa98xx_config_type that defines
 *          the subtypes as decribes below.
 *          tfa98xx_dsp_config_parameter_type() can be used to get the
 *           supported type for the active device..
 */
/*
 * config file V1 sub 1
 */
#define NXPTFA_CO_VERSION '1'
#define NXPTFA_CO3_VERSION '3'
#define NXPTFA_CO_SUBVERSION1 "01"
struct nxpTfaConfigS1File {
	struct nxpTfaHeader hdr;
	uint8_t data[55 * 3];
};

/*
 * config file V1 sub 2
 */
#define NXPTFA_CO_SUBVERSION2 "02"
struct nxpTfaConfigS2File {
	struct nxpTfaHeader hdr;
	uint8_t data[67 * 3];
};

/*
 * config file V1 sub 3
 */
#define NXPTFA_CO_SUBVERSION3 "03"
struct nxpTfaConfigS3File {
	struct nxpTfaHeader hdr;
	uint8_t data[67 * 3];
};

/*
 * config file V1.0
 */
#define NXPTFA_CO_SUBVERSION "00"
struct nxpTfaConfigFile {
	struct nxpTfaHeader hdr;
	uint8_t data[];
};

/*
 * preset file
 */
#define NXPTFA_PR_VERSION '1'
#define NXPTFA_PR_SUBVERSION "00"
struct nxpTfaPresetFile {
	struct nxpTfaHeader hdr;
	uint8_t data[];
};

/*
 * drc file
 */
#define NXPTFA_DR_VERSION '1'
#define NXPTFA_DR_SUBVERSION "00"
struct nxpTfaDrcFile {
	struct nxpTfaHeader hdr;
	uint8_t data[];
};

/*
 * drc file
 * for tfa 2 there is also a xml-version
 */
#define NXPTFA_DR3_VERSION '3'
#define NXPTFA_DR3_SUBVERSION "00"
struct nxpTfaDrcFile2 {
	struct nxpTfaHeader hdr;
	uint8_t version[3];
	uint8_t data[];
};

/*
 * volume step structures
 */

#define NXPTFA_VP1_VERSION '1'
#define NXPTFA_VP1_SUBVERSION "01"
struct nxpTfaVolumeStep1 {
	float attenuation;
	uint8_t preset[TFA98XX_PRESET_LENGTH];
};


#define NXPTFA_VP2_VERSION '2'
#define NXPTFA_VP2_SUBVERSION "01"
struct nxpTfaVolumeStep2 {
	float attenuation;
	uint8_t preset[TFA98XX_PRESET_LENGTH];
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];

};

/*
 * volumestep file
 */
#define NXPTFA_VP_VERSION '1'
#define NXPTFA_VP_SUBVERSION "00"
struct nxpTfaVolumeStepFile {
	struct nxpTfaHeader hdr;
	uint8_t vsteps;
	uint8_t samplerate;
	uint8_t payload;
};
/*
 * volumestep2 file
 */
struct nxpTfaVolumeStep2File {
	struct nxpTfaHeader hdr;
	uint8_t vsteps;
	uint8_t samplerate;
	struct nxpTfaVolumeStep2 vstep[];

};

/*
 * volumestepMax2 file
 */
struct nxpTfaVolumeStepMax2File {
	struct nxpTfaHeader hdr;
	uint8_t version[3];
	uint8_t NrOfVsteps;
	uint8_t vstepsBin[];
};

/*
 * volumestepMax2 file
 * This volumestep should ONLY be used for the use of bin2hdr!
 * This can only be used to find the messagetype of the vstep (without header)
 */
struct nxpTfaVolumeStepMax2_1File {
	uint8_t version[3];
	uint8_t NrOfVsteps;
	uint8_t vstepsBin[];
};

struct nxpTfaVolumeStepRegisterInfo {
	uint8_t NrOfRegisters;
	uint16_t registerInfo[];
};

struct nxpTfaVolumeStepMessageInfo {
	uint8_t NrOfMessages;
	uint8_t MessageType;
	struct uint24 MessageLength;
	uint8_t CmdId[3];
	uint8_t ParameterData[];
};
/**************************old v2
 * *************************************************/

/*
 * subv 00 volumestep file
 */
struct nxpTfaOldHeader {
	uint16_t id;
	char version[2];
	char subversion[2];
	uint16_t size;
	uint32_t CRC;
};

struct nxpOldTfaFilter {
	double bq[5];
	int32_t type;
	double frequency;
	double Q;
	double gain;
	uint8_t enabled;
};

struct nxpTfaOldVolumeStep2 {
	float attenuation;
	uint8_t preset[TFA98XX_PRESET_LENGTH];
	struct nxpOldTfaFilter eq[10];
};

struct nxpTfaOldVolumeStepFile {
	struct nxpTfaOldHeader hdr;
	struct nxpTfaOldVolumeStep2 step[];
};
/**************************end old v2
 * *************************************************/

/*
 * speaker file header
 */
struct nxpTfaSpkHeader {
	struct nxpTfaHeader hdr;
	char name[8];
	char vendor[16];
	char type[8];

	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint16_t ohm;
};

/*
 * speaker file
 */
#define NXPTFA_SP_VERSION '1'
#define NXPTFA_SP_SUBVERSION "00"
struct nxpTfaSpeakerFile {
	struct nxpTfaHeader hdr;
	char name[8];
	char vendor[16];
	char type[8];

	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint8_t ohm_primary;
	uint8_t ohm_secondary;
	uint8_t data[];
};

#define NXPTFA_VP3_VERSION '3'
#define NXPTFA_VP3_SUBVERSION "00"

struct nxpTfaFWVer {
	uint8_t Major;
	uint8_t minor;
	uint8_t minor_update : 6;
	uint8_t Update : 2;
};

struct nxpTfaFWMsg {
	struct nxpTfaFWVer fwVersion;
	struct nxpTfaMsg payload;
};

struct nxpTfaLiveData {
	char name[25];
	char addrs[25];
	int tracker;
	int scalefactor;
};

#define NXPTFA_SP3_VERSION '3'
#define NXPTFA_SP3_SUBVERSION "00"
struct nxpTfaSpeakerFileMax2 {
	struct nxpTfaHeader hdr;
	char name[8];
	char vendor[16];
	char type[8];

	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint8_t ohm_primary;
	uint8_t ohm_secondary;
	struct nxpTfaFWMsg FWmsg;
};

/*
 * parameter container file
 */
/*
 * descriptors
 * Note 1: append new DescriptorType at the end
 * Note 2: add new descriptors to dsc_name[] in tfaContUtil.c
 */
enum nxpTfaDescriptorType {
	dscDevice,
	dscProfile,
	dscRegister,
	dscString,
	dscFile,
	dscPatch,
	dscMarker,
	dscMode,
	dscSetInputSelect,
	dscSetOutputSelect,
	dscSetProgramConfig,
	dscSetLagW,
	dscSetGains,
	dscSetvBatFactors,
	dscSetSensesCal,
	dscSetSensesDelay,
	dscBitfield,
	dscDefault,
	dscLiveData,
	dscLiveDataString,
	dscGroup,
	dscCmd,
	dscSetMBDrc,
	dscFilter,
	dscNoInit,
	dscFeatures,
	dscCfMem,
	dscSetFwkUseCase,
	dscSetVddpConfig,
	dsc_last
};

#define TFA_BITFIELDDSCMSK 0x7fffffff
struct nxpTfaDescPtr {
	uint32_t offset : 24;
	uint32_t type : 8;

};

/*
 * generic file descriptor
 */
struct nxpTfaFileDsc {
	struct nxpTfaDescPtr name;
	uint32_t size;
	uint8_t data[];
};

/*
 * device descriptor list
 */
struct nxpTfaDeviceList {
	uint8_t length;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
	uint32_t devid;
	struct nxpTfaDescPtr name;
	struct nxpTfaDescPtr list[];
};

/*
 * profile descriptor list
 */
struct nxpTfaProfileList {
	uint32_t length : 8;
	uint32_t group : 8;
	uint32_t ID : 16;
	struct nxpTfaDescPtr name;
	struct nxpTfaDescPtr list[];
};
#define TFA_PROFID 0x1234

/*
 * livedata descriptor list
 */
struct nxpTfaLiveDataList {
	uint32_t length : 8;
	uint32_t ID : 24;
	struct nxpTfaDescPtr name;
	struct nxpTfaDescPtr list[];
};
#define TFA_LIVEDATAID 0x5678

/*
 * Bitfield descriptor
 */
struct nxpTfaBitfield {
	uint16_t value;
	uint16_t field;
};

/*
 * Bitfield enumuration bits descriptor
 */
struct nxpTfaBfEnum {
	unsigned int len : 4;
	unsigned int pos : 4;
	unsigned int address : 8;
};

/*
 * Register patch descriptor
 */
struct nxpTfaRegpatch {
	uint8_t address;
	uint16_t value;
	uint16_t mask;
};

/*
 * Mode descriptor
 */
struct nxpTfaUseCase {
	int value;
};

/*
 * NoInit descriptor
 */
struct nxpTfaNoInit {
	uint8_t value;
};

/*
 * Features descriptor
 */
struct nxpTfaFeatures {
	uint16_t value[3];
};

/*
 * the container file
 *   - the size field is 32bits long (generic=16)
 *   - all char types are in ASCII
 */
#define NXPTFA_PM_VERSION '1'
#define NXPTFA_PM3_VERSION '3'
#define NXPTFA_PM_SUBVERSION '1'
struct nxpTfaContainer {
	char id[2];
	char version[2];
	char subversion[2];
	uint32_t size;
	uint32_t CRC;
	uint16_t rev;
	char customer[8];
	char application[8];
	char type[8];
	uint16_t ndev;
	uint16_t nprof;
	uint16_t nliveData;
	struct nxpTfaDescPtr index[];
};

#pragma pack(pop)

#endif /* TFA98XXPARAMETERS_H_ */
