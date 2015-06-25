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

#ifndef __TFA_CONTAINER_H__
#define __TFA_CONTAINER_H__



/*
 * tfa98xx container firmware header
 */

struct nxpTfaDescPtr {
	u32 offset:24;
	u8  type; // (== enum nxpTfaDescriptorType, assure 8bits length)
};


struct nxpTfaDevice {
	u8 length;		// nr of items in the list
	u8 bus;		// bus
	u8 dev;		// device
	u8 func;		// subfunction or subdevice
	u32 devid;		// device  hw fw id
	struct nxpTfaDescPtr name;	// device name
	struct nxpTfaDescPtr list[];	// items list
} __attribute__((packed));


#define HDR(c1,c2) (c2<<8|c1) // little endian
enum tfa_cnt_header_type {
	params		= HDR('P','M'),
	volstep		= HDR('V','P'),
	patch		= HDR('P','A'),
	speaker		= HDR('S','P'),
	preset		= HDR('P','R'),
	config		= HDR('C','O'),
	equalizer	= HDR('E','Q'),
	drc		= HDR('D','R')
};

enum nxpTfaHeaderType {
    paramsHdr	= HDR('P','M'),
    volstepHdr	= HDR('V','P'),
    patchHdr	= HDR('P','A'),
    speakerHdr	= HDR('S','P'),
    presetHdr	= HDR('P','R'),
    configHdr	= HDR('C','O'),
    equalizerHdr = HDR('E','Q'),
    drcHdr	= HDR('D','R')
};


#define TFA_PM_VERSION '1'
#define TFA_PM_SUBVERSION "00"

enum nxpTfaDescriptorType {
	dscDevice,	// device list
	dscProfile,	// profile list
	dscRegister,	// register patch
	dscString,	// ascii, zero terminated string
	dscFile,	// filename + file contents
	dscPatch, 	// patch file
	dscMarker,	// marker to indicate end of a list
	dscBitfieldBase=0x80 // start of bitfield enums
};

/*
 * profile descriptor list
 */
struct nxpTfaProfile {
	u8 length;			// nr of items in the list
	u32 ID:24;			// profile ID
	struct nxpTfaDescPtr name;	// profile name
	struct nxpTfaDescPtr list[];	// items list
} __attribute__((packed));
#define TFA_PROFID 0x1234


//static char nostring[]="Undefined string";

/*
 * generic file descriptor
 */
struct nxpTfaFileDsc {
	struct nxpTfaDescPtr name;
	u32 size;	// file data length in bytes
	u8 data[];	//payload
} __attribute__((packed));


struct nxpTfaContainer {
	char id[2];          	// "XX" : XX=type
	char version[2];     	// "V_" : V=version, vv=subversion
	char subversion[2];  	// "vv" : vv=subversion
	u32 size;       	// data size in bytes following CRC
	u32 CRC;        	// 32-bits CRC for following data
	u16 rev;		// "extra chars for rev nr"
	char customer[8];    	// “name of customer”
	char application[8]; 	// “application name”
	char type[8];		// “application type name”
	u16 ndev;	 	// "nr of device lists"
	u16 nprof;	 	// "nr of profile lists"
	struct nxpTfaDescPtr index[]; // start of item index table
} __attribute__((packed));

struct tfa_cnt_header {
	u16	id;
	char	version[2];     /* "V_" : V=version */
	char	subversion[2];  // "vv" : vv=subversion
	u32	size;		// data size in bytes following CRC
	u32	CRC;        	// 32-bits CRC for following data
	u16	rev;
	char	customer[8];    // “name of customer”
	char	application[8]; // “application name”
	char	type[8];	// “application type name”
	u16	ndev;	 	// "nr of device lists"
	u16	nprof;	 	// "nr of profile lists"
	struct nxpTfaDescPtr index[]; // start of item index table
} __attribute__((packed));


/*
 * the generic header
 *   all char types are in ASCII
 */
struct nxpTfaHeader {
	u16 id;
	char version[2];     // "V_" : V=version, vv=subversion
	char subversion[2];  // "vv" : vv=subversion
	u16 size;       // data size in bytes following CRC
	u32 CRC;        // 32-bits CRC for following data
	char customer[8];    // “name of customer”
	char application[8]; // “application name”
	char type[8];		 // “application type name”
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
	fCustom,         //User defined biquad coefficients
	fFlat,           //Vary only gain
	fLowpass,        //2nd order Butterworth low pass
	fHighpass,       //2nd order Butterworth high pass
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
	u8 type; // (== enum FilterTypes, assure 8bits length)
	float frequency;
	float Q;
	float gain;
} __attribute__((packed));  //8 * float + int32 + byte == 37

#define TFA98XX_MAX_EQ 10

struct nxpTfaEqualizer {
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];// note: API index counts from 1..10
};




/*
 * equalizer file
 */
#define NXPTFA_EQ_VERSION    '1'
#define NXPTFA_EQ_SUBVERSION "00"

struct nxpTfaEqualizerFile {
	struct nxpTfaHeader hdr;
	u8 samplerate; 				 // ==enum samplerates, assure 8 bits
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];// note: API index counts from 1..10
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
	u32 attenuation;		// contain IEEE single float
	u8 preset[TFA98XX_PRESET_LENGTH];
} __attribute__((packed));

/* VP02 */
#define NXPTFA_VP2_VERSION    '2'
#define NXPTFA_VP2_SUBVERSION "01"

struct nxpTfaVolumeStep2 {
	u32 attenuation;		// contain IEEE single float
	u8 preset[TFA98XX_PRESET_LENGTH];
	struct nxpTfaFilter filter[TFA98XX_MAX_EQ];// note: API index counts from 1..10
} __attribute__((packed));


/*
 * Register patch descriptor
 */
struct nxpTfaRegpatch {
	u8   address;	// register address
	u16  value;	// value to write
	u16  mask;	// mask of bits to write
} __attribute__((packed));

/*
 * volumestep file
 */
struct nxpTfaVolumeStepFile {
	struct nxpTfaHeader hdr;
	u8 vsteps;  	// can also be calulated from size+type
	u8 samplerate;	// ==enum samplerates, assure 8 bits
	u8 payload; 	//start of variable length contents:N times volsteps
} __attribute__((packed));

/*
 * volumestep2 file
 */
struct nxpTfaVolumeStep2File {
	struct nxpTfaHeader hdr;
	u8 vsteps;  	// can also be calulated from size+type
	u8 samplerate; 	// ==enum samplerates, assure 8 bits
	struct nxpTfaVolumeStep2 vstep[]; 	//start of variable length contents:N times volsteps
} __attribute__((packed));

/*
 * speaker file
 */
#define NXPTFA_SP_VERSION    '1'
#define NXPTFA_SP_SUBVERSION "00"

struct nxpTfaSpeakerFile {
	struct nxpTfaHeader hdr;
	char name[8];	// speaker nick name (e.g. “dumbo”)
	char vendor[16];
	char type[8];
	/* dimensions (mm) */
	u8 height;
	u8 width;
	u8 depth;
	u16 ohm;
	u8 data[]; //payload TFA98XX_SPEAKERPARAMETER_LENGTH
} __attribute__((packed));

/*
 * Bitfield descriptor
 */
struct nxpTfaBitfield {
	u16  value;
	u16  field; // ==datasheet defined, 16 bits
} __attribute__((packed));

/*
 * Bitfield enumuration bits descriptor
 */
struct nxpTfaBfEnum {
	u16  len:4;		// this is the actual length-1
	u16  pos:4;
	u16  address:8;
} __attribute__((packed));


struct drcBiquad {
	u24 freq;		// center frequency
	u24 Q;			// Q factor
	u24 gain;		// gain in dB
	u24 type;		// filter type (= enum dspBqFiltType_t)
};

struct drc {
	u24 enabled;	// drc enabled
	u24 sidechain;	// side chain usage
	u24 kneetype;	// knee type enum
	u24 env;		// envelope
	u24 attack;		// attack time in ms
	u24 release;	// release time in ms;
	u24 thresDb;	// threshold in dB;
	u24 ratio;
	u24 makeupGain;	// make up gain in dB
};

struct drcBandLimited {
	u24 enabled;			// band limted drc enabled (0 = off)
	struct drcBiquad biquad;	// biquad for band
	struct drc limiter;			// band limiter
};

struct drcParamBlock {
	u24 drcOn;			// drc module enabled. 0 == off
	struct drcBiquad hi1bq;	// high band biquads
	struct drcBiquad hi2bq;
	struct drcBiquad mi1bq;	// mid band biqauds
	struct drcBiquad mi2bq;
	struct drcBiquad mi3bq;
	struct drcBiquad mi4bq;
	struct drcBiquad lo1bq;	// low band biquads
	struct drcBiquad lo2bq;
	struct drcBiquad po1bq;	// post biquads
	struct drcBiquad po2bq;
	struct drc hi1drc;		// high compressors
	struct drc hi2drc;
	struct drc mi1drc;		// mid compressors
	struct drc mi2drc;
	struct drc lo1drc;		// low compressors
	struct drc lo2drc;
	struct drc po1drc;		// post compressors
	struct drc po2drc;
	struct drcBandLimited bl;// band limited compressor
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
int tfaContWriteVstepAttenuation(struct tfa98xx *tfa98xx, struct nxpTfaVolumeStep2File *vp);
int tfaContWriteFile(struct tfa98xx *tfa98xx, struct nxpTfaFileDsc *file);
int tfaContWriteAttenuation(struct tfa98xx *tfa98xx, struct nxpTfaFileDsc *file);
int tfaContWriteItem(struct tfa98xx *tfa98xx, struct nxpTfaDescPtr * dsc);
int tfaContWriteProfile(struct tfa98xx *tfa98xx, int profile, int vstep);
int tfaContWriteFilesProf(struct tfa98xx *tfa98xx, int profile, int vstep);
int tfaContWriteVolumeAttenuation(struct tfa98xx *tfa98xx, int profile, int vstep);
int tfaContWriteFiles(struct tfa98xx *tfa98xx);
struct snd_kcontrol_new *tfa_build_profile_controls(struct tfa98xx *tfa98xx, int* kcontrol_count);

#endif
