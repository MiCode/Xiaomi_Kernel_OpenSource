/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#if !defined(SI_INFOFRAME_H)
#define SI_INFOFRAME_H

struct __attribute__ ((__packed__)) info_frame_header_t {
	uint8_t type_code;
	uint8_t version_number;
	uint8_t length;
};

enum AviColorSpace_e {
	acsRGB,
	acsYCbCr422,
	acsYCbCr444,
	acsYCbCr420
};

enum avi_quant_range_e {
	aqr_default = 0,
	aqr_limited_range,
	aqr_full_range,
	aqr_reserved
};

/*
 * AVI Info Frame Structure
 */
struct __attribute__ ((__packed__)) avi_info_frame_data_byte_1_t {
	uint8_t ScanInfo:2;
	uint8_t BarInfo:2;
	uint8_t ActiveFormatInfoPresent:1;
	enum AviColorSpace_e colorSpace:2;
	uint8_t futureMustBeZero:1;
};

struct __attribute__ ((__packed__)) avi_info_frame_data_byte_2_t {
	uint8_t ActiveFormatAspectRatio:4;
	uint8_t PictureAspectRatio:2;
	uint8_t Colorimetry:2;
};

struct __attribute__ ((__packed__)) avi_info_frame_data_byte_3_t {
	uint8_t NonUniformPictureScaling:2;
	uint8_t RGBQuantizationRange:2;
	uint8_t ExtendedColorimetry:3;
	uint8_t ITContent:1;
};

struct __attribute__ ((__packed__)) avi_info_frame_data_byte_4_t {
	uint8_t VIC:7;
	uint8_t futureMustBeZero:1;
};

enum BitsContent_e {
	cnGraphics,
	cnPhoto,
	cnCinema,
	cnGame
};

enum AviQuantization_e {
	aqLimitedRange,
	aqFullRange,
	aqReserved0,
	aqReserved1
};

struct __attribute__ ((__packed__)) avi_info_frame_data_byte_5_t {
	uint8_t pixelRepetionFactor:4;
	enum BitsContent_e content:2;
	enum AviQuantization_e quantization:2;
};

struct __attribute__ ((__packed__)) hw_avi_named_payload_t {
	uint8_t checksum;
	union {
		struct __attribute__ ((__packed__)) {
			struct avi_info_frame_data_byte_1_t pb1;
			struct avi_info_frame_data_byte_2_t
				colorimetryAspectRatio;
			struct avi_info_frame_data_byte_3_t pb3;
			struct avi_info_frame_data_byte_4_t VIC;
			struct avi_info_frame_data_byte_5_t pb5;
			uint8_t LineNumEndTopBarLow;
			uint8_t LineNumEndTopBarHigh;
			uint8_t LineNumStartBottomBarLow;
			uint8_t LineNumStartBottomBarHigh;
			uint8_t LineNumEndLeftBarLow;
			uint8_t LineNumEndLeftBarHigh;
			uint8_t LineNumStartRightBarLow;
			uint8_t LineNumStartRightBarHigh;
		} bitFields;
		uint8_t infoFrameData[13];
	} ifData_u;
};

/* this union overlays the TPI HW for AVI InfoFrames,
 * starting at REG_TPI_AVI_CHSUM.
 */
union hw_avi_payload_t {
	struct hw_avi_named_payload_t namedIfData;
	uint8_t ifData[14];
};

struct __attribute__ ((__packed__)) avi_payload_t {
	union hw_avi_payload_t hwPayLoad;
	uint8_t byte_14;
	uint8_t byte_15;
};

struct __attribute__ ((__packed__)) avi_info_frame_t {
	struct info_frame_header_t header;
	struct avi_payload_t payLoad;
};

/* these values determine the interpretation of PB5 */
enum HDMI_Video_Format_e {
	hvfNoAdditionalHDMIVideoFormatPresent,
	hvfExtendedResolutionFormatPresent,
	hvf3DFormatIndicationPresent
};

enum _3D_structure_e {
	tdsFramePacking,
	tdsTopAndBottom = 0x06,
	tdsSideBySide = 0x08
};

enum ThreeDExtData_e {
	tdedHorizontalSubSampling,
	tdedQuincunxOddLeftOddRight = 0x04,
	tdedQuincunxOddLeftEvenRight,
	tdedQuincunxEvenLeftOddRight,
	tdedQuincunxEvenLeftEvenRight
};

enum ThreeDMetaDataType_e {
	tdmdParallaxIso23022_3Section6_x_2_2
};

struct __attribute__ ((__packed__)) hdmi_vendor_specific_payload_t {
	struct __attribute__ ((__packed__)) {
		unsigned reserved:5;
		enum HDMI_Video_Format_e HDMI_Video_Format:3;
	} pb4;
	union {
		uint8_t HDMI_VIC;
		struct __attribute__ ((__packed__)) _ThreeDStructure {
			unsigned reserved:3;
			unsigned ThreeDMetaPresent:1;
			enum _3D_structure_e threeDStructure:4;
		} ThreeDStructure;
	} pb5;
	struct __attribute__ ((__packed__)) {
		uint8_t reserved:4;
		uint8_t threeDExtData:4;	/* ThreeDExtData_e */
	} pb6;
	struct __attribute__ ((__packed__)) _PB7 {
		uint8_t threeDMetaDataLength:5;
		uint8_t threeDMetaDataType:3;	/* ThreeDMetaDataType_e */
	} pb7;
};

#define IEEE_OUI_HDMI 0x000C03
#define VSIF_COMMON_FIELDS \
	struct info_frame_header_t header; \
	uint8_t checksum;		   \
	uint8_t ieee_oui[3];

struct __attribute__ ((__packed__)) hdmi_vsif_t{
	VSIF_COMMON_FIELDS
	struct hdmi_vendor_specific_payload_t payLoad;
};

struct __attribute__ ((__packed__)) vsif_common_header_t {
	VSIF_COMMON_FIELDS
};
/*
 * MPEG Info Frame Structure
 * Table 8-11 on page 141 of HDMI Spec v1.4
 */
struct __attribute__ ((__packed__)) unr_info_frame_t {
	struct info_frame_header_t header;
	uint8_t checksum;
	uint8_t byte_1;
	uint8_t byte_2;
	uint8_t byte_3;
	uint8_t byte_4;
	uint8_t byte_5;
	uint8_t byte_6;
};

#ifdef ENABLE_DUMP_INFOFRAME

void DumpIncomingInfoFrameImpl(char *pszId, char *pszFile, int iLine,
	info_frame_t *pInfoFrame, uint8_t length);

#define DumpIncomingInfoFrame(pData, length) \
	DumpIncomingInfoFrameImpl(#pData, __FILE__, __LINE__, \
	(info_frame_t *)pData, length)
#else
#define DumpIncomingInfoFrame(pData, length)	/* do nothing */
#endif

#endif /* if !defined(SI_INFOFRAME_H) */
