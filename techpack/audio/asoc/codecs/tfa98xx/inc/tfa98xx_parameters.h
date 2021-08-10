/* 
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright 2020 GOODIX 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


/*
 * tfa98xx_parameters.h
 *
 *  Created on: Jul 22, 2013
 *      Author: NLV02095
 */

#ifndef TFA98XXPARAMETERS_H_
#define TFA98XXPARAMETERS_H_

//#include "config.h"
// workaround for Visual Studio: 
// fatal error C1083: Cannot open include file: 'config.h': No such file or directory
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "tfa_service.h"

#if (defined(WIN32) || defined(_X64))
/* These warnings are disabled because it is only given by Windows and there is no easy fix */
#pragma warning(disable:4200)
#pragma warning(disable:4214)
#endif

/*
 * profiles & volumesteps
 *
 */
#define TFA_MAX_PROFILES			(64)
#define TFA_MAX_VSTEPS				(64)
#define TFA_MAX_VSTEP_MSG_MARKER	(100) /* This marker  is used to indicate if all msgs need to be written to the device */
#define TFA_MAX_MSGS				(10)

// the pack pragma is required to make that the size in memory
// matches the actual variable lenghts
// This is to assure that the binary files can be transported between
// different platforms.
#pragma pack (push, 1)

/*
 * typedef for 24 bit value using 3 bytes
 */
typedef struct uint24 {
  uint8_t b[3];
} uint24_t;
/*
 * the generic header
 *   all char types are in ASCII
 */
typedef struct TfaHeader {
	uint16_t id;
    char version[2];     // "V_" : V=version, vv=subversion
    char subversion[2];  // "vv" : vv=subversion
    uint16_t size;       // data size in bytes following CRC
    uint32_t CRC;        // 32-bits CRC for following data
    char customer[8];    // “name of customer”
    char application[8]; // “application name”
    char type[8];		 // “application type name”
} TfaHeader_t;

typedef enum TfaSamplerate {
	fs_8k,       // 8kHz
	fs_11k025,   // 11.025kHz
	fs_12k,      // 12kHz
	fs_16k,      // 16kHz
	fs_22k05,    // 22.05kHz
	fs_24k,      // 24kHz
	fs_32k,      // 32kHz
	fs_44k1,     // 44.1kHz
	fs_48k,      // 48kHz
	fs_96k,      // 96kHz
	fs_count     // Should always be last item.
} TfaSamplerate_t;

// Keep in sync with TfaSamplerate_t !
static const int TfaSamplerateHz[fs_count] = { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 96000 };


/*
 * coolflux direct memory access
 */
typedef struct TfaDspMem {
	uint8_t  type;		/* 0--3: p, x, y, iomem */
	uint16_t address;	/* target address */
	uint8_t size;		/* data size in words */
	int words[];		/* payload  in signed 32bit integer (two's complement) */
} TfaDspMem_t;

/*
 * the biquad coefficients for the API together with index in filter
 *  the biquad_index is the actual index in the equalizer +1
 */
#define BIQUAD_COEFF_SIZE       6

/*
* Output fixed point coeffs structure
*/
typedef struct {
	int a2;
	int a1;	
	int b2;	
	int b1;	
	int b0;	
}TfaBiquad_t;

typedef struct TfaBiquadOld {
  uint8_t bytes[BIQUAD_COEFF_SIZE*sizeof(uint24_t)];
}TfaBiquadOld_t;

typedef struct TfaBiquadFloat {
  float headroom;
  float b0;
  float b1;
  float b2;
  float a1;
  float a2;
} TfaBiquadFloat_t;

/*
* EQ filter definitions
* Note: This is not in line with smartstudio (JV: 12/12/2016)
*/
typedef enum TfaFilterType {
	fCustom,		//User defined biquad coefficients
	fFlat,			//Vary only gain
	fLowpass,		//2nd order Butterworth low pass
	fHighpass,		//2nd order Butterworth high pass
	fLowshelf,
	fHighshelf,
	fNotch,
	fPeak,
	fBandpass,
	f1stLP,
	f1stHP,
	fElliptic
} TfaFilterType_t;

/*
 * filter parameters for biquad (re-)calculation
 */
typedef struct TfaFilter {
  TfaBiquadOld_t biquad;
  uint8_t enabled;
  uint8_t type; // (== enum FilterTypes, assure 8bits length)
  float frequency;
  float Q;
  float gain;
} TfaFilter_t ;  //8 * float + int32 + byte == 37

/* 
 * biquad params for calculation
*/

#define TFA_BQ_EQ_INDEX 0
#define TFA_BQ_ANTI_ALIAS_INDEX 10
#define TFA_BQ_INTEGRATOR_INDEX 13

/*
* Loudspeaker Compensation filter definitions
*/
typedef struct TfaLsCompensationFilter {
  TfaBiquad_t biquad;
  uint8_t lsCompOn;  // Loudspeaker compensation on/off; when 'off', the DSP code doesn't apply the bwExt => bwExtOn GUI flag should be gray to avoid confusion
  uint8_t bwExtOn;   // Bandwidth extension on/off
  float fRes;        // [Hz] speaker resonance frequency
  float Qt;          // Speaker resonance Q-factor
  float fBwExt;      // [Hz] Band width extension frequency
  float samplingFreq;// [Hz] Sampling frequency
} TfaLsCompensationFilter_t;

/*
* Anti Aliasing Elliptic filter definitions
*/
typedef struct TfaAntiAliasFilter {
  TfaBiquad_t biquad;	/**< Output results fixed point coeffs */
  uint8_t enabled;
  float cutOffFreq;   // cut off frequency
  float samplingFreq; // sampling frequency
  float rippleDb;     // range: [0.1 3.0]
  float rolloff;      // range: [-1.0 1.0]
} TfaAntiAliasFilter_t;

/**
* Integrator filter input definitions
*/
typedef struct TfaIntegratorFilter {
  TfaBiquad_t biquad;	/**< Output results fixed point coeffs */
  uint8_t type;             /**< Butterworth filter type: high or low pass */
  float  cutOffFreq;        /**< cut off frequency in Hertz; range: [100.0 4000.0] */
  float  samplingFreq;      /**< sampling frequency in Hertz */
  float  leakage;           /**< leakage factor; range [0.0 1.0] */
} TfaIntegratorFilter_t;


typedef struct TfaEqFilter {
  TfaBiquad_t biquad;
  uint8_t enabled;
  uint8_t type;       // (== enum FilterTypes, assure 8bits length)
  float cutOffFreq;   // cut off frequency, // range: [100.0 4000.0]
  float samplingFreq; // sampling frequency
  float Q;            // range: [0.5 5.0]
  float gainDb;       // range: [-10.0 10.0]
} TfaEqFilter_t ;  //8 * float + int32 + byte == 37

typedef struct TfaContAntiAlias {
	int8_t index; 	/**< index determines destination type; anti-alias, integrator,eq */
	uint8_t type;
	float cutOffFreq;   // cut off frequency
	float samplingFreq;
	float rippleDb;     // integrator leakage
	float rolloff;
	uint8_t bytes[5*3];	// payload 5*24buts coeffs
}TfaContAntiAlias_t;

typedef struct TfaContIntegrator {
	int8_t index; 	/**< index determines destination type; anti-alias, integrator,eq */
	uint8_t type;
	float cutOffFreq;   // cut off frequency
	float samplingFreq;
	float leakage;     // integrator leakage
	float reserved;
	uint8_t bytes[5*3];	// payload 5*24buts coeffs
}TfaContIntegrator_t;

typedef struct TfaContEq {
  int8_t index;
  uint8_t type;			// (== enum FilterTypes, assure 8bits length)
  float cutOffFreq;		// cut off frequency, // range: [100.0 4000.0]
  float samplingFreq;	// sampling frequency
  float Q;				// range: [0.5 5.0]
  float gainDb;			// range: [-10.0 10.0]
  uint8_t bytes[5*3];	// payload 5*24buts coeffs
} TfaContEq_t ;		//8 * float + int32 + byte == 37

typedef union TfaContBiquad {
	TfaContEq_t eq;
	TfaContAntiAlias_t aa;
	TfaContIntegrator_t in;
}TfaContBiquad_t;

#define TFA_BQ_EQ_INDEX			0
#define TFA_BQ_ANTI_ALIAS_INDEX	10
#define TFA_BQ_INTEGRATOR_INDEX 13
#define TFA98XX_MAX_EQ			10

typedef struct TfaEqualizer {
  TfaFilter_t filter[TFA98XX_MAX_EQ];
} TfaEqualizer_t;

/*
 * files
 */
#define HDR(c1,c2) (c2<<8|c1) // little endian
typedef enum TfaHeaderType {
    paramsHdr		= HDR('P','M'), /* containter file */
    volstepHdr	 	= HDR('V','P'),
    patchHdr	 	= HDR('P','A'),
    speakerHdr	 	= HDR('S','P'),
    presetHdr	 	= HDR('P','R'),
    configHdr	 	= HDR('C','O'),
    equalizerHdr	= HDR('E','Q'),
    drcHdr			= HDR('D','R'),
    msgHdr			= HDR('M','G'),	/* generic message */
    infoHdr			= HDR('I','N')
} TfaHeaderType_t;

/*
 * equalizer file
 */
#define TFA_EQ_VERSION    '1'
#define TFA_EQ_SUBVERSION "00"
typedef struct TfaEqualizerFile {
	TfaHeader_t hdr;
	uint8_t samplerate; 				 // ==enum samplerates, assure 8 bits
    TfaFilter_t filter[TFA98XX_MAX_EQ];// note: API index counts from 1..10
} TfaEqualizerFile_t;

/*
 * patch file
 */
#define TFA_PA_VERSION    '1'
#define TFA_PA_SUBVERSION "00"
typedef struct TfaPatchFile {
	TfaHeader_t hdr;
	uint8_t data[];
} TfaPatch_t;

/*
 * generic message file
 *   -  the payload of this file includes the opcode and is send straight to the DSP
 */
#define TFA_MG_VERSION    '3'
#define TFA_MG_SUBVERSION "00"
typedef struct TfaMsgFile {
	TfaHeader_t hdr;
	uint8_t data[];
} TfaMsgFile_t;

/*
 * NOTE the tfa98xx API defines the enum Tfa98xx_config_type that defines
 *          the subtypes as decribes below.
 *          tfa98xx_dsp_config_parameter_type() can be used to get the
 *           supported type for the active device..
 */
/*
 * config file V1 sub 1
 */
#define TFA_CO_VERSION		'1'
#define TFA_CO3_VERSION		'3'
#define TFA_CO_SUBVERSION1	"01"
typedef struct TfaConfigS1File {
	TfaHeader_t hdr;
	uint8_t data[55*3];
} TfaConfigS1_t;

/*
 * config file V1 sub 2
 */
#define TFA_CO_SUBVERSION2 "02"
typedef struct TfaConfigS2File {
	TfaHeader_t hdr;
	uint8_t data[67*3];
} TfaConfigS2_t;

/*
 * config file V1 sub 3
 */
#define TFA_CO_SUBVERSION3 "03"
typedef struct TfaConfigS3File {
	TfaHeader_t hdr;
	uint8_t data[67*3];
} TfaConfigS3_t;

/*
 * config file V1.0
 */
#define TFA_CO_SUBVERSION "00"
typedef struct TfaConfigFile {
	TfaHeader_t hdr;
	uint8_t data[];
} TfaConfig_t;

/*
 * preset file
 */
#define TFA_PR_VERSION    '1'
#define TFA_PR_SUBVERSION "00"
typedef struct TfaPresetFile {
	TfaHeader_t hdr;
	uint8_t data[];
} TfaPreset_t;

/*
 * drc file
 */
#define TFA_DR_VERSION    '1'
#define TFA_DR_SUBVERSION "00"
typedef struct TfaDrcFile {
	TfaHeader_t hdr;
	uint8_t data[];
} TfaDrc_t;

/*
 * drc file
 * for tfa 2 there is also a xml-version
 */
#define TFA_DR3_VERSION    '3'
#define TFA_DR3_SUBVERSION "00"
typedef struct TfaDrcFile2 {
	TfaHeader_t hdr;
	uint8_t version[3];
	uint8_t data[];
} TfaDrc2_t;

/*
 * volume step structures
 */
// VP01
#define TFA_VP1_VERSION    '1'
#define TFA_VP1_SUBVERSION "01"
typedef struct TfaVolumeStep1 {
    float attenuation;              // IEEE single float
    uint8_t preset[TFA98XX_PRESET_LENGTH];
} TfaVolumeStep1_t;

// VP02
#define TFA_VP2_VERSION    '2'
#define TFA_VP2_SUBVERSION "01"
typedef struct TfaVolumeStep2 {
    float attenuation;              // IEEE single float
    uint8_t preset[TFA98XX_PRESET_LENGTH];
    TfaFilter_t filter[TFA98XX_MAX_EQ];// note: API index counts from 1..10
} TfaVolumeStep2_t;

/*
 * volumestep file
 */
#define TFA_VP_VERSION    '1'
#define TFA_VP_SUBVERSION "00"
typedef struct TfaVolumeStepFile {
	TfaHeader_t hdr;
	uint8_t vsteps;  	// can also be calulated from size+type
	uint8_t samplerate; // ==enum samplerates, assure 8 bits
	uint8_t payload; 	//start of variable length contents:N times volsteps
}TfaVolumeStepFile_t;
/*
 * volumestep2 file
 */
typedef struct TfaVolumeStep2File {
	TfaHeader_t hdr;
	uint8_t vsteps;  	// can also be calulated from size+type
	uint8_t samplerate; // ==enum samplerates, assure 8 bits
	TfaVolumeStep2_t vstep[]; 	//start of variable length contents:N times volsteps
}TfaVolumeStep2File_t;

/*
 * volumestepMax2 file
 */
typedef struct TfaVolumeStepMax2File {
	TfaHeader_t hdr;
	uint8_t version[3]; 
	uint8_t NrOfVsteps;
	uint8_t vstepsBin[]; 
}TfaVolumeStepMax2File_t;

/*
 * volumestepMax2 file
 * This volumestep should ONLY be used for the use of bin2hdr!
 * This can only be used to find the messagetype of the vstep (without header)
 */
typedef struct TfaVolumeStepMax2_1File {
	uint8_t version[3]; 
	uint8_t NrOfVsteps;
	uint8_t vstepsBin[]; 
}TfaVolumeStepMax2_1File_t;

struct TfaVolumeStepRegisterInfo {
	uint8_t NrOfRegisters;
	uint16_t registerInfo[];
};

struct TfaVolumeStepMessageInfo {
	uint8_t NrOfMessages;
	uint8_t MessageType;
	uint24_t MessageLength;
	uint8_t CmdId[3];
	uint8_t ParameterData[];
};
/**************************old v2 *************************************************/

/*
 * subv 00 volumestep file
 */
typedef struct TfaOldHeader {
	uint16_t id;
	char version[2];     // "V_" : V=version, vv=subversion
	char subversion[2];  // "vv" : vv=subversion
	uint16_t size;       // data size in bytes following CRC
	uint32_t CRC;        // 32-bits CRC for following data
} TfaOldHeader_t;

typedef struct TfaOldFilter {
  double bq[5];
  int32_t type;
  double frequency;
  double Q;
  double gain;
  uint8_t enabled;
} TfaOldFilter_t ;

typedef struct TfaOldVolumeStep2 {
    float attenuation;              // IEEE single float
    uint8_t preset[TFA98XX_PRESET_LENGTH];
    TfaOldFilter_t eq[10];
} TfaOldVolumeStep2_t;

typedef struct TfaOldVolumeStepFile {
	TfaOldHeader_t hdr;
	TfaOldVolumeStep2_t step[];
}TfaOldVolumeStep2File_t;
/**************************end old v2 *************************************************/

/*
 * speaker file header
 */
struct TfaSpkHeader {
	struct TfaHeader hdr;
	char name[8];	// speaker nick name (e.g. “dumbo”)
	char vendor[16];
	char type[8];
	//	dimensions (mm)
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint16_t ohm;
};

/*
 * speaker file
 */
#define TFA_SP_VERSION    '1'
#define TFA_SP_SUBVERSION "00"
typedef struct TfaSpeakerFile {
	TfaHeader_t hdr;
	char name[8];	// speaker nick name (e.g. “dumbo”)
	char vendor[16];
	char type[8];
	//	dimensions (mm)
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint8_t ohm_primary;
	uint8_t ohm_secondary;
	uint8_t data[]; //payload TFA98XX_SPEAKERPARAMETER_LENGTH
} TfaSpeakerFile_t;

#define TFA_VP3_VERSION    '3'
#define TFA_VP3_SUBVERSION "00"

struct TfaFWVer {
	uint8_t Major;
	uint8_t minor;
	uint8_t minor_update:6;
	uint8_t Update:2;
};

struct TfaFWMsg {
	struct TfaFWVer fwVersion;
	struct TfaMsg payload;
};

typedef struct TfaLiveData {
	char name[25];
	char addrs[25];
	int tracker;
	int scalefactor;
} TfaLiveData_t;

#define TFA_SP3_VERSION  '3'
#define TFA_SP3_SUBVERSION "00"
struct TfaSpeakerFileMax2 {
	TfaHeader_t hdr;
	char name[8];	// speaker nick name (e.g. “dumbo”)
	char vendor[16];
	char type[8];
	//	dimensions (mm)
	uint8_t height;
	uint8_t width;
	uint8_t depth;
	uint8_t ohm_primary;
	uint8_t ohm_secondary;
	struct TfaFWMsg FWmsg; //payload including FW ver and Cmd ID
};

/*
 * parameter container file
 */
/*
 * descriptors
 * Note 1: append new DescriptorType at the end
 * Note 2: add new descriptors to dsc_name[] in tfaContUtil.c
 */
typedef enum TfaDescriptorType {
	dscDevice,		// device list
	dscProfile,		// profile list
	dscRegister,	// register patch
	dscString,		// ascii, zero terminated string
	dscFile,		// filename + file contents
	dscPatch,		// patch file
	dscMarker,		// marker to indicate end of a list
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
	dscDefault,		// used to reset bitfields to there default values
	dscLiveData,
	dscLiveDataString,
	dscGroup,
	dscCmd,
	dscSetMBDrc,
	dscFilter,
	dscNoInit,
	dscFeatures,
	dscCfMem,		// coolflux memory x,y,io
	dscSetFwkUseCase,
	dscSetVddpConfig,
	dsc_last		// trailer
} TfaDescriptorType_t;

#define TFA_BITFIELDDSCMSK 0x7fffffff
typedef struct TfaDescPtr {
	uint32_t offset:24;
	uint32_t  type:8; // (== enum TfaDescriptorType, assure 8bits length)
}TfaDescPtr_t;

/*
 * generic file descriptor
 */
typedef struct TfaFileDsc {
	TfaDescPtr_t name;
	uint32_t size;	// file data length in bytes
	uint8_t data[]; //payload
} TfaFileDsc_t;


/*
 * device descriptor list
 */
typedef struct TfaDeviceList {
	uint8_t length;			// nr of items in the list
	uint8_t bus;			// bus
	uint8_t dev;			// device
	uint8_t func;			// subfunction or subdevice
	uint32_t devid;			// device  hw fw id
	TfaDescPtr_t name;	// device name
	TfaDescPtr_t list[];	// items list
} TfaDeviceList_t;

/*
 * profile descriptor list
 */
typedef struct TfaProfileList {
	uint32_t length:8;		// nr of items in the list + name
	uint32_t group:8;		// profile group number
	uint32_t ID:16;			// profile ID
	TfaDescPtr_t name;	// profile name
	TfaDescPtr_t list[];	// items list (lenght-1 items)
} TfaProfileList_t;
#define TFA_PROFID 0x1234

/*
 * livedata descriptor list
 */
typedef struct TfaLiveDataList {
	uint32_t length:8;		// nr of items in the list
	uint32_t ID:24;			// profile ID
	TfaDescPtr_t name;	        // livedata name
	TfaDescPtr_t list[];	        // items list
} TfaLiveDataList_t;
#define TFA_LIVEDATAID 0x5678

/*
 * Bitfield descriptor
 */
typedef struct TfaBitfield {
	uint16_t  value;
	uint16_t  field; // ==datasheet defined, 16 bits
} TfaBitfield_t;

/*
 * Bitfield enumuration bits descriptor
 */
typedef struct TfaBfEnum {
	unsigned int  len:4;		// this is the actual length-1
	unsigned int  pos:4;
	unsigned int  address:8;
} TfaBfEnum_t;

/*
 * Register patch descriptor
 */
typedef struct TfaRegpatch {
	uint8_t   address;	// register address
	uint16_t  value;	// value to write
	uint16_t  mask;		// mask of bits to write
} TfaRegpatch_t;

/*
 * Mode descriptor
 */
typedef struct TfaUseCase {
	int value;	// mode value, maps to enum Tfa98xx_Mode
} TfaMode_t;

/*
 * NoInit descriptor
 */
typedef struct TfaNoInit {
	uint8_t value;	// noInit value
} TfaNoInit_t;

/*
 * Features descriptor
 */
typedef struct TfaFeatures {
	uint16_t value[3];	// features value
} TfaFeatures_t;


/*
 * the container file
 *   - the size field is 32bits long (generic=16)
 *   - all char types are in ASCII
 */
#define TFA_PM_VERSION  '1'
#define TFA_PM3_VERSION '3'
#define TFA_PM_SUBVERSION '1'
typedef struct TfaContainer {
    char id[2];					// "XX" : XX=type
    char version[2];			// "V_" : V=version, vv=subversion
    char subversion[2];			// "vv" : vv=subversion
    uint32_t size;				// data size in bytes following CRC
    uint32_t CRC;				// 32-bits CRC for following data
    uint16_t rev;				// "extra chars for rev nr"
    char customer[8];			// “name of customer”
    char application[8];		// “application name”
    char type[8];				// “application type name”
    uint16_t ndev;	 			// "nr of device lists"
    uint16_t nprof;	 			// "nr of profile lists"
    uint16_t nliveData;			// "nr of livedata lists"
    TfaDescPtr_t index[];	// start of item index table
} TfaContainer_t;

#pragma pack (pop)

#endif /* TFA98XXPARAMETERS_H_ */
