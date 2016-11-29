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

#ifndef __TFA_CONTAINER_H__
#define __TFA_CONTAINER_H__



/*
 * tfa98xx container firmware header
 */

struct nxpTfaDescPtr {
	u32 offset:24;
	u8  type;
};


struct nxpTfaDevice {
	u8 length;
	u8 bus;
	u8 dev;
	u8 func;
	u32 devid;
	struct nxpTfaDescPtr name;
	struct nxpTfaDescPtr list[];
} __attribute__((packed));


#define HDR(c1, c2) (c2<<8|c1)
enum tfa_cnt_header_type {
	params		= HDR('P', 'M'),
	volstep		= HDR('V', 'P'),
	patch		= HDR('P', 'A'),
	speaker		= HDR('S', 'P'),
	preset		= HDR('P', 'R'),
	config		= HDR('C', 'O'),
	equalizer	= HDR('E', 'Q'),
	drc		= HDR('D', 'R')
};

enum nxpTfaHeaderType {
	paramsHdr	= HDR('P', 'M'),
	volstepHdr	= HDR('V', 'P'),
	patchHdr	= HDR('P', 'A'),
	speakerHdr	= HDR('S', 'P'),
	presetHdr	= HDR('P', 'R'),
	configHdr	= HDR('C', 'O'),
	equalizerHdr = HDR('E', 'Q'),
	drcHdr	= HDR('D', 'R')
};


#define TFA_PM_VERSION '1'
#define TFA_PM_SUBVERSION "00"

enum nxpTfaDescriptorType {
	dscDevice,
	dscProfile,
	dscRegister,
	dscString,
	dscFile,
	dscPatch,
	dscMarker,
	dscMode,
	dscCfMem,
	dscFilter,
	dscBitfieldBase = 0x80
};

/*
 * profile descriptor list
 */
struct nxpTfaProfile {
	u8 length;
	u32 ID:24;
	struct nxpTfaDescPtr name;
	struct nxpTfaDescPtr list[];
} __attribute__((packed));
#define TFA_PROFID 0x1234




/*
 * generic file descriptor
 */
struct nxpTfaFileDsc {
	struct nxpTfaDescPtr name;
	u32 size;
	u8 data[];
} __attribute__((packed));


struct nxpTfaContainer {
	char id[2];
	char version[2];
	char subversion[2];
	u32 size;
	u32 CRC;
	u16 rev;
	char customer[8];
	char application[8];
	char type[8];
	u16 ndev;
	u16 nprof;
	struct nxpTfaDescPtr index[];
} __attribute__((packed));

struct tfa_cnt_header {
	u16	id;
	char	version[2];     /* "V_" : V=version */
	char	subversion[2];
	u32	size;
	u32	CRC;
	u16	rev;
	char	customer[8];
	char	application[8];
	char	type[8];
	u16	ndev;
	u16	nprof;
	struct nxpTfaDescPtr index[];
} __attribute__((packed));


/*
 * the generic header
 *   all char types are in ASCII
 */
struct nxpTfaHeader {
	u16 id;
	char version[2];
	char subversion[2];
	u16 size;
	u32 CRC;
	char customer[8];
	char application[8];
	char type[8];
} __attribute__((packed));

#define NXPTFA_PA_VERSION    '1'
#define NXPTFA_PA_SUBVERSION "00"

struct nxpTfaPatch {
	struct nxpTfaHeader hdr;
	u8 data[];
} __attribute__((packed));


/*
 * typedef for 24 bit value using 3 bytes
 */
typedef struct uint24 {
	u8 b[3];
} u24;

/*
 * coolflux direct memory access
 */
struct nxpTfaDspMem {
	u8  type;         /* 0--3: p, x, y, iomem */
	u16 address;      /* target address */
	u8  size;	  /* data size in words */
	s32 words[]; 	  /* payload  in signed 32bit integer (two's complement) */
} __attribute__((packed));

/*
 * the biquad coefficients for the API together with index in filter
 *  the biquad_index is the actual index in the equalizer +1
 */
struct nxpTfaBiquad {
	u8 bytes[6*sizeof(u24)];
} __attribute__((packed));

struct nxpTfaBiquadFloat {
	float headroom;
	float b0;
	float b1;
	float b2;
	float a1;
	float a2;
};

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
	fCount
};

/*
 * filter parameters for biquad (re-)calculation
 */
struct nxpTfaFilter {
	struct nxpTfaBiquad biquad;
	u8 enabled;
	u8 type;
	float frequency;
	float Q;
	float gain;
} __attribute__((packed));

/**
 * standalone filter data in container file
 *  contains data for recalculation and payload data
 */
struct nxpTfaBiquadSettings {
	int8_t index; 	/**< index determines destination type; anti-alias, integrator,eq */
	uint8_t type;
	float cutOffFreq;
	float samplingFreq;
	float rippleDb_leakage;
	float rolloff;
	uint8_t bytes[5*3];
} __attribute__((packed));



#define TFA98XX_MAX_EQ 10

struct nxpTfaEqualizer {
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];
};




/*
 * equalizer file
 */
#define NXPTFA_EQ_VERSION    '1'
#define NXPTFA_EQ_SUBVERSION "00"

struct nxpTfaEqualizerFile {
	struct nxpTfaHeader hdr;
	u8 samplerate;
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];
} __attribute__((packed));

/*
 * config file
 */
#define NXPTFA_CO_VERSION    '1'
#define NXPTFA_CO_SUBVERSION "00"

struct nxpTfaConfigFile {
	struct nxpTfaHeader hdr;
	u8 data[];
} __attribute__((packed));

/*
 * preset file
 */
#define NXPTFA_PR_VERSION    '1'
#define NXPTFA_PR_SUBVERSION "00"

struct nxpTfaPresetFile {
	struct nxpTfaHeader hdr;
	u8 data[];
} __attribute__((packed));

/*
 * drc file
 * TODO: Add DRC filter data, treshold ...
 */
#define NXPTFA_DR_VERSION    '1'
#define NXPTFA_DR_SUBVERSION "00"

struct nxpTfaDrcFile {
	struct nxpTfaHeader hdr;
	u8 data[];
}  __attribute__((packed));

/*
 * volume step structures
 */

/* VP01 */
#define NXPTFA_VP1_VERSION    '1'
#define NXPTFA_VP1_SUBVERSION "01"

struct nxpTfaVolumeStep1 {
	u32 attenuation;
	u8 preset[TFA98XX_PRESET_LENGTH];
} __attribute__((packed));

/* VP02 */
#define NXPTFA_VP2_VERSION    '2'
#define NXPTFA_VP2_SUBVERSION "01"

struct nxpTfaVolumeStep2 {
	u32 attenuation;
	u8 preset[TFA98XX_PRESET_LENGTH];
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];
} __attribute__((packed));


/*
 * Register patch descriptor
 */
struct nxpTfaRegpatch {
	u8   address;
	u16  value;
	u16  mask;
} __attribute__((packed));

/*
 * Mode descriptor
 */
struct nxpTfaMode {
	u32  value;
} __attribute__((packed));


/*
 * volumestep file
 */
struct nxpTfaVolumeStepFile {
	struct nxpTfaHeader hdr;
	u8 vsteps;
	u8 samplerate;
	u8 payload;
} __attribute__((packed));

/*
 * volumestep2 file
 */
struct nxpTfaVolumeStep2File {
	struct nxpTfaHeader hdr;
	u8 vsteps;
	u8 samplerate;
	struct nxpTfaVolumeStep2 vstep[];
} __attribute__((packed));

/*
 * speaker file
 */
#define NXPTFA_SP_VERSION    '1'
#define NXPTFA_SP_SUBVERSION "00"

struct nxpTfaSpeakerFile {
	struct nxpTfaHeader hdr;
	char name[8];
	char vendor[16];
	char type[8];
	/* dimensions (mm) */
	u8 height;
	u8 width;
	u8 depth;
	u16 ohm;
	u8 data[];
} __attribute__((packed));

/*
 * Bitfield descriptor
 */
struct nxpTfaBitfield {
	u16  value;
	u16  field;
} __attribute__((packed));

/*
 * Bitfield enumuration bits descriptor
 */
struct nxpTfaBfEnum {
	u16  len:4;
	u16  pos:4;
	u16  address:8;
} __attribute__((packed));


struct drcBiquad {
	u24 freq;
	u24 Q;
	u24 gain;
	u24 type;
};

struct drc {
	u24 enabled;
	u24 sidechain;
	u24 kneetype;
	u24 env;
	u24 attack;
	u24 release;
	u24 thresDb;
	u24 ratio;
	u24 makeupGain;
};

struct drcBandLimited {
	u24 enabled;
	struct drcBiquad biquad;
	struct drc limiter;
};

struct drcParamBlock {
	u24 drcOn;
	struct drcBiquad hi1bq;
	struct drcBiquad hi2bq;
	struct drcBiquad mi1bq;
	struct drcBiquad mi2bq;
	struct drcBiquad mi3bq;
	struct drcBiquad mi4bq;
	struct drcBiquad lo1bq;
	struct drcBiquad lo2bq;
	struct drcBiquad po1bq;
	struct drcBiquad po2bq;
	struct drc hi1drc;
	struct drc hi2drc;
	struct drc mi1drc;
	struct drc mi2drc;
	struct drc lo1drc;
	struct drc lo2drc;
	struct drc po1drc;
	struct drc po2drc;
	struct drcBandLimited bl;
};



int tfa98xx_cnt_loadfile(struct tfa98xx *tfa98xx, int index);
char *tfaContBfName(u16 num);
int tfaContWriteRegsProf(struct tfa98xx *tfa98xx, int profile);
int tfaContWriteRegsDev(struct tfa98xx *tfa98xx);
void tfaContShowHeader(struct tfa98xx *tfa98xx, struct nxpTfaHeader *hdr);
int tfaContWritePatch(struct tfa98xx *tfa98xx);
int tfaContWriteFilterbank(struct tfa98xx *tfa98xx, struct nxpTfaFilter *filter);
int tfaContWriteEq(struct tfa98xx *tfa98xx, struct nxpTfaEqualizerFile *eqf);
int tfaContWriteVstep(struct tfa98xx *tfa98xx, struct nxpTfaVolumeStep2File *vp);
int tfaContWriteFile(struct tfa98xx *tfa98xx, struct nxpTfaFileDsc *file);
int tfaContWriteItem(struct tfa98xx *tfa98xx, struct nxpTfaDescPtr *dsc);
int tfaContWriteProfile(struct tfa98xx *tfa98xx, int profile, int vstep);
int tfaContWriteFilesProf(struct tfa98xx *tfa98xx, int profile, int vstep);
int tfaContWriteFiles(struct tfa98xx *tfa98xx);
int tfa98xx_dsp_write_drc(struct tfa98xx *tfa98xx, int len, const u8 *data);
int tfaContWriteFilesVstep(struct tfa98xx *tfa98xx, int profile, int vstep);

#endif
