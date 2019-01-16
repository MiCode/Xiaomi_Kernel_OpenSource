

#ifndef _SIHDMITX_902X_TPI_H_
#define _SIHDMITX_902X_TPI_H_

/* -------------------------------------------------------------------- */
/* typedef */
/* -------------------------------------------------------------------- */

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;

/* -------------------------------------------------------------------- */
/* System Macro definition */
/* -------------------------------------------------------------------- */
#define IIC_OK 0


#define DEV_SUPPORT_EDID  1
/* #define DEV_SUPPORT_HDCP */
/* #define DEV_SUPPORT_CEC */
/* #define DEV_SUPPORT_3D */

#define CLOCK_EDGE_RISING
/* #define CLOCK_EDGE_FALLING */

#define F_9022A_9334
/* #define HW_INT_ENABLE */



/* -------------------------------------------------------------------- */
/* TPI Firmware Version */
/* -------------------------------------------------------------------- */
static const char TPI_FW_VERSION[] = "TPI Firmware v6.6.3_APP v1.3";

/* Generic Constants */
/* ==================================================== */
/* #define FALSE                0 */
/* #define TRUE                  1 */

#define OFF                    0
#define ON                      1

#define LOW                   0
#define HIGH                  1

#define DISABLE	0x00
#define ENABLE	0xFF


#define MAX_V_DESCRIPTORS				20
#define MAX_A_DESCRIPTORS				10
#define MAX_SPEAKER_CONFIGURATIONS	4
#define AUDIO_DESCR_SIZE				3

#define RGB					0
#define YCBCR444			1
#define YCBCR422_16BITS		2
#define YCBCR422_8BITS		3
#define XVYCC444			4

#define EXTERNAL_HSVSDE	0
#define INTERNAL_DE			1
#define EMBEDDED_SYNC		2

#define COLORIMETRY_601	0
#define COLORIMETRY_709	1

/* ==================================================== */
#define MCLK128FS			0
#define MCLK256FS			1
#define MCLK384FS			2
#define MCLK512FS			3
#define MCLK768FS			4
#define MCLK1024FS			5
#define MCLK1152FS			6
#define MCLK192FS			7

#define SCK_SAMPLE_FALLING_EDGE	0x00
#define SCK_SAMPLE_RISING_EDGE	0x80

/* ==================================================== */
/* Video mode define */
#define HDMI_480I60_4X3	1
#define HDMI_576I50_4X3	2
#define HDMI_480P60_4X3	3
#define HDMI_576P50_4X3	4
#define HDMI_720P60			5
#define HDMI_720P50			6
#define HDMI_1080I60		7
#define HDMI_1080I50		8
#define HDMI_1080P60		9
#define HDMI_1080P50		10
#define HDMI_1024_768_60	11
#define HDMI_800_600_60		12
#define HDMI_1080P30		13
#define HDMI_1080P24		14





/* ==================================================== */
#define AMODE_I2S			0
#define AMODE_SPDIF		1
#define AMODE_HBR			2
#define AMODE_DSD			3

#define ACHANNEL_2CH		1
#define ACHANNEL_3CH		2
#define ACHANNEL_4CH		3
#define ACHANNEL_5CH		4
#define ACHANNEL_6CH		5
#define ACHANNEL_7CH		6
#define ACHANNEL_8CH		7

#define AFS_44K1			0x00
#define AFS_48K				0x02
#define AFS_32K				0x03
#define AFS_88K2			0x08
#define AFS_768K			0x09
#define AFS_96K				0x0a
#define AFS_176K4			0x0c
#define AFS_192K			0x0e

#define ALENGTH_16BITS		0x02
#define ALENGTH_17BITS		0x0c
#define ALENGTH_18BITS		0x04
#define ALENGTH_19BITS		0x08
#define ALENGTH_20BITS		0x0a
#define ALENGTH_21BITS		0x0d
#define ALENGTH_22BITS		0x05
#define ALENGTH_23BITS		0x09
#define ALENGTH_24BITS		0x0b

/* ==================================================== */
typedef struct {
	byte HDMIVideoFormat;	/* 0 = CEA-861 VIC; 1 = HDMI_VIC; 2 = 3D */
	byte VIC;		/* VIC or the HDMI_VIC */
	byte AspectRatio;	/* 4x3 or 16x9 */
	byte ColorSpace;	/* 0 = RGB; 1 = YCbCr4:4:4; 2 = YCbCr4:2:2_16bits; 3 = YCbCr4:2:2_8bits; 4 = xvYCC4:4:4 */
	byte ColorDepth;	/* 0 = 8bits; 1 = 10bits; 2 = 12bits */
	byte Colorimetry;	/* 0 = 601; 1 = 709 */
	byte SyncMode;		/* 0 = external HS/VS/DE; 1 = external HS/VS and internal DE; 2 = embedded sync */
	byte TclkSel;		/* 0 = x0.5CLK; 1 = x1CLK; 2 = x2CLK; 3 = x4CLK */
	byte ThreeDStructure;	/* Valid when (HDMIVideoFormat == VMD_HDMIFORMAT_3D) */
	byte ThreeDExtData;	/* Valid when (HDMIVideoFormat == VMD_HDMIFORMAT_3D) && (ThreeDStructure == VMD_3D_SIDEBYSIDEHALF) */

	byte AudioMode;		/* 0 = I2S; 1 = S/PDIF; 2 = HBR; 3 = DSD; */
	byte AudioChannels;	/* 1 = 2chs; 2 = 3chs; 3 = 4chs; 4 = 5chs; 5 = 6chs; 6 = 7chs; 7 = 8chs; */
	byte AudioFs;		/* 0-44.1kHz; 2-48kHz; 3-32kHz; 8-88.2kHz; 9-768kHz; A-96kHz; C-176.4kHz; E-192kHz; 1/4/5/6/7/B/D/F-not indicated */
	byte AudioWordLength;	/* 0/1-not available; 2-16 bit; 4-18 bit; 8-19 bit; A-20 bit; C-17 bit; 5-22 bit; 9-23 bit; B-24 bit; D-21 bit */
	byte AudioI2SFormat;	/* Please refer to TPI reg0x20 for detailed. */
	/* [7]_SCK Sample Edge: 0 = Falling; 1 = Rising */
	/* [6:4]_MCLK Multiplier: 000:MCLK=128Fs; 001:MCLK=256Fs; 010:MCLK=384Fs; 011:MCLK=512Fs; 100:MCLK=768Fs; 101:MCLK=1024Fs; 110:MCLK=1152Fs; 111:MCLK=192Fs; */
	/* [3]_WS Polarity-Left when: 0 = WS is low when Left; 1 = WS is high when Left */
	/* [2]_SD Justify Data is justified: 0 = Left; 1 = Right */
	/* [1]_SD Direction Byte shifted first: 0 = MSB; 1 = LSB */
	/* [0]_WS to SD First Bit Shift: 0 = Yes; 1 = No */

} SIHDMITX_CONFIG;

/* ==================================================== */
typedef struct {
	byte txPowerState;
	byte tmdsPoweredUp;
	byte hdmiCableConnected;
	byte dsRxPoweredUp;

} GLOBAL_SYSTEM;

/* ==================================================== */
typedef struct {
	byte HDCP_TxSupports;
	byte HDCP_AksvValid;
	byte HDCP_Started;
	byte HDCP_LinkProtectionLevel;
	byte HDCP_Override;
	byte HDCPAuthenticated;

} GLOBAL_HDCP;

/* ==================================================== */
typedef struct {		/* for storing EDID parsed data */
	byte edidDataValid;
	byte VideoDescriptor[MAX_V_DESCRIPTORS];	/* maximum number of video descriptors */
	byte AudioDescriptor[MAX_A_DESCRIPTORS][3];	/* maximum number of audio descriptors */
	byte SpkrAlloc[MAX_SPEAKER_CONFIGURATIONS];	/* maximum number of speaker configurations */
	byte UnderScan;		/* "1" if DTV monitor underscans IT video formats by default */
	byte BasicAudio;	/* Sink supports Basic Audio */
	byte YCbCr_4_4_4;	/* Sink supports YCbCr 4:4:4 */
	byte YCbCr_4_2_2;	/* Sink supports YCbCr 4:2:2 */
	byte HDMI_Sink;		/* "1" if HDMI signature found */
	byte CEC_A_B;		/* CEC Physical address. See HDMI 1.3 Table 8-6 */
	byte CEC_C_D;
	byte ColorimetrySupportFlags;	/* IEC 61966-2-4 colorimetry support: 1 - xvYCC601; 2 - xvYCC709 */
	byte MetadataProfile;
	byte _3D_Supported;
	byte HDMI_compatible_VSDB;
} GLOBAL_EDID;

enum EDID_ErrorCodes {
	EDID_OK,
	EDID_INCORRECT_HEADER,
	EDID_CHECKSUM_ERROR,
	EDID_NO_861_EXTENSIONS,
	EDID_SHORT_DESCRIPTORS_OK,
	EDID_LONG_DESCRIPTORS_OK,
	EDID_EXT_TAG_ERROR,
	EDID_REV_ADDR_ERROR,
	EDID_V_DESCR_OVERFLOW,
	EDID_UNKNOWN_TAG_CODE,
	EDID_NO_DETAILED_DESCRIPTORS,
	EDID_DDC_BUS_REQ_FAILURE,
	EDID_DDC_BUS_RELEASE_FAILURE
};


#ifdef DEV_SUPPORT_EDID
#define IsHDMI_Sink()		(g_edid.HDMI_Sink)
#define IsCEC_DEVICE()		(((g_edid.CEC_A_B != 0xFF) && (g_edid.CEC_C_D != 0xFF)) ? 1 : 0)

#else
#define IsHDMI_Sink()		(1)
#define IsCEC_DEVICE()		(0)
#endif



/* -------------------------------------------------------------------- */
/* Debug Definitions */
/* -------------------------------------------------------------------- */
/* Compile debug prints inline or not */
#define CONF__TPI_TRACE_PRINT		(DISABLE)
#define CONF__TPI_DEBUG_PRINT	(DISABLE)
#define CONF__TPI_EDID_PRINT	(DISABLE)
#define CONF__CPI_DEBUG_PRINT	(DISABLE)


/* Trace Print Macro */
/* Note: TPI_TRACE_PRINT Requires double parenthesis */
/* Example:  TPI_TRACE_PRINT(("hello, world!\n")); */
#if (CONF__TPI_TRACE_PRINT == ENABLE)
#define TPI_TRACE_PRINT(x)	pr_debug x;
#else
#define TPI_TRACE_PRINT(x)
#endif

/* Debug Print Macro */
/* Note: TPI_DEBUG_PRINT Requires double parenthesis */
/* Example:  TPI_DEBUG_PRINT(("hello, world!\n")); */
#if (CONF__TPI_DEBUG_PRINT == ENABLE)
#define TPI_DEBUG_PRINT(x)	pr_debug x;
#else
#define TPI_DEBUG_PRINT(x)
#endif

/* EDID Print Macro */
/* Note: To enable EDID description printing, both CONF__TPI_EDID_PRINT and CONF__TPI_DEBUG_PRINT must be enabled */
/* Note: TPI_EDID_PRINT Requires double parenthesis */
/* Example:  TPI_EDID_PRINT(("hello, world!\n")); */
#if (CONF__TPI_EDID_PRINT == ENABLE)
#define TPI_EDID_PRINT(x)		TPI_DEBUG_PRINT(x)
#else
#define TPI_EDID_PRINT(x)
#endif

/* CPI Debug Print Macro */
/* Note: To enable CPI description printing, both CONF__CPI_DEBUG_PRINT and CONF__TPI_DEBUG_PRINT must be enabled */
/* Note: CPI_DEBUG_PRINT Requires double parenthesis */
/* Example:  CPI_DEBUG_PRINT(("hello, world!\n")); */
#if (CONF__CPI_DEBUG_PRINT == ENABLE)
#define CPI_DEBUG_PRINT(x)	TPI_DEBUG_PRINT(x)
#else
#define CPI_DEBUG_PRINT(x)
#endif



enum AV_ConfigErrorCodes {
	DE_CANNOT_BE_SET_WITH_EMBEDDED_SYNC,
	V_MODE_NOT_SUPPORTED,
	SET_EMBEDDED_SYC_FAILURE,
	I2S_MAPPING_SUCCESSFUL,
	I2S_INPUT_CONFIG_SUCCESSFUL,
	I2S_HEADER_SET_SUCCESSFUL,
	EHDMI_ARC_SINGLE_SET_SUCCESSFUL,
	EHDMI_ARC_COMMON_SET_SUCCESSFUL,
	EHDMI_HEC_SET_SUCCESSFUL,
	EHDMI_ARC_CM_WITH_HEC_SET_SUCCESSFUL,
	AUD_MODE_NOT_SUPPORTED,
	I2S_NOT_SET,
	DE_SET_OK,
	VIDEO_MODE_SET_OK,
	AUDIO_MODE_SET_OK,
	GBD_SET_SUCCESSFULLY,
	DE_CANNOT_BE_SET_WITH_3D_MODE,
};


#define ClearInterrupt(x)       WriteByteTPI(0x3D, x)	/* write "1" to clear interrupt bit */

/* Generic Masks */
/* ==================================================== */
#define LOW_BYTE      0x00FF

#define LOW_NIBBLE	0x0F
#define HI_NIBBLE	0xF0

#define MSBIT		0x80
#define LSBIT		0x01

#define BIT_0                   0x01
#define BIT_1                   0x02
#define BIT_2                   0x04
#define BIT_3                   0x08
#define BIT_4                   0x10
#define BIT_5                   0x20
#define BIT_6                   0x40
#define BIT_7                   0x80

#define TWO_LSBITS		0x03
#define THREE_LSBITS	0x07
#define FOUR_LSBITS	0x0F
#define FIVE_LSBITS	0x1F
#define SEVEN_LSBITS	0x7F
#define TWO_MSBITS	0xC0
#define EIGHT_BITS	0xFF
#define BYTE_SIZE		0x08
#define BITS_1_0		0x03
#define BITS_2_1		0x06
#define BITS_2_1_0		0x07
#define BITS_3_2		0x0C
#define BITS_4_3_2		0x1C
#define BITS_5_4		0x30
#define BITS_5_4_3		0x38
#define BITS_6_5		0x60
#define BITS_6_5_4		0x70
#define BITS_7_6		0xC0

/* Interrupt Masks */
/* ==================================================== */
#define HOT_PLUG_EVENT          0x01
#define RX_SENSE_EVENT          0x02
#define HOT_PLUG_STATE          0x04
#define RX_SENSE_STATE          0x08

#define AUDIO_ERROR_EVENT       0x10
#define SECURITY_CHANGE_EVENT   0x20
#define V_READY_EVENT           0x40
#define HDCP_CHANGE_EVENT       0x80

#define NON_MASKABLE_INT		0xFF

/* TPI Control Masks */
/* ==================================================== */

#define CS_HDMI_RGB         0x00
#define CS_DVI_RGB          0x03

#define ENABLE_AND_REPEAT	0xC0
#define EN_AND_RPT_MPEG	0xC3
#define DISABLE_MPEG		0x03	/* Also Vendor Specific InfoFrames */

/* Pixel Repetition Masks */
/* ==================================================== */
#define BIT_BUS_24          0x20
#define BIT_BUS_12          0x00

#define BIT_EDGE_RISE       0x10

/* Audio Maps */
/* ==================================================== */
#define BIT_AUDIO_MUTE      0x10

/* Input/Output Format Masks */
/* ==================================================== */
#define BITS_IN_RGB         0x00
#define BITS_IN_YCBCR444    0x01
#define BITS_IN_YCBCR422    0x02

#define BITS_IN_AUTO_RANGE  0x00
#define BITS_IN_FULL_RANGE  0x04
#define BITS_IN_LTD_RANGE   0x08

#define BIT_EN_DITHER_10_8  0x40
#define BIT_EXTENDED_MODE   0x80

#define BITS_OUT_RGB        0x00
#define BITS_OUT_YCBCR444   0x01
#define BITS_OUT_YCBCR422   0x02

#define BITS_OUT_AUTO_RANGE 0x00
#define BITS_OUT_FULL_RANGE 0x04
#define BITS_OUT_LTD_RANGE  0x08

#define BIT_BT_709          0x10


/* DE Generator Masks */
/* ==================================================== */
#define BIT_EN_DE_GEN       0x40
#define DE					0x00
#define DeDataNumBytes		12

/* Embedded Sync Masks */
/* ==================================================== */
#define BIT_EN_SYNC_EXTRACT 0x40
#define EMB                 0x80
#define EmbDataNumBytes     8


/* Audio Modes */
/* ==================================================== */
#define AUD_PASS_BASIC      0x00
#define AUD_PASS_ALL        0x01
#define AUD_DOWN_SAMPLE     0x02
#define AUD_DO_NOT_CHECK    0x03

#define REFER_TO_STREAM_HDR     0x00
#define TWO_CHANNELS		0x00
#define EIGHT_CHANNELS		0x01
#define AUD_IF_SPDIF			0x40
#define AUD_IF_I2S			0x80
#define AUD_IF_DSD				0xC0
#define AUD_IF_HBR				0x04

#define TWO_CHANNEL_LAYOUT      0x00
#define EIGHT_CHANNEL_LAYOUT    0x20


/* I2C Slave Addresses */
/* ==================================================== */
#define TX_SLAVE_ADDR		0x72
#define CBUS_SLAVE_ADDR		0xC8
#define HDCP_SLAVE_ADDR		0x74
#define EDID_ROM_ADDR		0xA0
#define EDID_SEG_ADDR		0x60

/* Indexed Register Offsets, Constants */
/* ==================================================== */
#define INDEXED_PAGE_0		0x01
#define INDEXED_PAGE_1		0x02
#define INDEXED_PAGE_2		0x03

/* DDC Bus Addresses */
/* ==================================================== */
#define DDC_BSTATUS_ADDR_L  0x41
#define DDC_BSTATUS_ADDR_H  0x42
#define DDC_KSV_FIFO_ADDR   0x43
#define KSV_ARRAY_SIZE      128

/* DDC Bus Bit Masks */
/* ==================================================== */
#define BIT_DDC_HDMI        0x80
#define BIT_DDC_REPEATER    0x40
#define BIT_DDC_FIFO_RDY    0x20
#define DEVICE_COUNT_MASK   0x7F

/* KSV Buffer Size */
/* ==================================================== */
#define DEVICE_COUNT         128	/* May be tweaked as needed */

/* InfoFrames */
/* ==================================================== */
#define SIZE_AVI_INFOFRAME      0x0E	/* including checksum byte */
#define BITS_OUT_FORMAT         0x60	/* Y1Y0 field */

#define _4_To_3                 0x10	/* Aspect ratio - 4:3  in InfoFrame DByte 1 */
#define _16_To_9                0x20	/* Aspect ratio - 16:9 in InfoFrame DByte 1 */
#define SAME_AS_AR              0x08	/* R3R2R1R0 - in AVI InfoFrame DByte 2 */

#define BT_601                  0x40
#define BT_709                  0x80

/* #define EN_AUDIO_INFOFRAMES         0xC2 */
#define TYPE_AUDIO_INFOFRAMES       0x84
#define AUDIO_INFOFRAMES_VERSION    0x01
#define AUDIO_INFOFRAMES_LENGTH     0x0A

#define TYPE_GBD_INFOFRAME		0x0A

#define ENABLE_AND_REPEAT			0xC0

#define EN_AND_RPT_MPEG				0xC3
#define DISABLE_MPEG				0x03	/* Also Vendor Specific InfoFrames */

#define EN_AND_RPT_AUDIO			0xC2
#define DISABLE_AUDIO				0x02

#define EN_AND_RPT_AVI				0xC0	/* Not normally used.  Write to TPI 0x19 instead */
#define DISABLE_AVI					0x00	/* But this is used to Disable */

#define NEXT_FIELD					0x80
#define GBD_PROFILE					0x00
#define AFFECTED_GAMUT_SEQ_NUM		0x01

#define ONLY_PACKET					0x30
#define CURRENT_GAMUT_SEQ_NUM		0x01

/* FPLL Multipliers: */
/* ==================================================== */

#define X0d5							0x00
#define X1							0x01
#define X2							0x02
#define X4							0x03

/* 3D Constants */
/* ==================================================== */

#define _3D_STRUC_PRESENT				0x02

/* 3D_Stucture Constants */
/* ==================================================== */
#define FRAME_PACKING					0x00
#define FIELD_ALTERNATIVE				0x01
#define LINE_ALTERNATIVE				0x02
#define SIDE_BY_SIDE_FULL				0x03
#define L_PLUS_DEPTH					0x04
#define L_PLUS_DEPTH_PLUS_GRAPHICS	0x05
#define SIDE_BY_SIDE_HALF				0x08

/* 3D_Ext_Data Constants */
/* ==================================================== */
#define HORIZ_ODD_LEFT_ODD_RIGHT		0x00
#define HORIZ_ODD_LEFT_EVEN_RIGHT		0x01
#define HORIZ_EVEN_LEFT_ODD_RIGHT		0x02
#define HORIZ_EVEN_LEFT_EVEN_RIGHT		0x03

#define QUINCUNX_ODD_LEFT_EVEN_RIGHT	0x04
#define QUINCUNX_ODD_LEFT_ODD_RIGHT		0x05
#define QUINCUNX_EVEN_LEFT_ODD_RIGHT	0x06
#define QUINCUNX_EVEN_LEFT_EVEN_RIGHT	0x07

#define NO_3D_SUPPORT					0x0F

/* InfoFrame Type Code */
/* ==================================================== */
#define AVI						0x00
#define SPD						0x01
#define AUDIO						0x02
#define MPEG						0x03
#define GEN_1						0x04
#define GEN_2						0x05
#define HDMI_VISF					0x06
#define GBD						0x07

/* Size of InfoFrame Data types */
#define MAX_SIZE_INFOFRAME_DATA     0x22
#define SIZE_AVI_INFOFRAME			0x0E	/* 14 bytes */
#define SIZE_SPD_INFOFRAME			0x19	/* 25 bytes */
#define SISE_AUDIO_INFOFRAME_IFORM  0x0A	/* 10 bytes */
#define SIZE_AUDIO_INFOFRAME		0x0F	/* 15 bytes */
#define SIZE_MPRG_HDMI_INFOFRAME    0x1B	/* 27 bytes */
#define SIZE_MPEG_INFOFRAME		0x0A	/* 10 bytes */
#define SIZE_GEN_1_INFOFRAME	0x1F	/* 31 bytes */
#define SIZE_GEN_2_INFOFRAME	0x1F	/* 31 bytes */
#define SIZE_HDMI_VISF_INFOFRAME    0x1E	/* 31 bytes */
#define SIZE_GBD_INFOFRAME		0x1C	/* 28 bytes */

#define AVI_INFOFRM_OFFSET          0x0C
#define OTHER_INFOFRM_OFFSET        0xC4
#define TPI_INFOFRAME_ACCESS_REG    0xBF

/* Serial Communication Buffer constants */
#define MAX_COMMAND_ARGUMENTS       50
#define GLOBAL_BYTE_BUF_BLOCK_SIZE  131


/* Video Mode Constants */
/* ==================================================== */
#define VMD_ASPECT_RATIO_4x3			0x01
#define VMD_ASPECT_RATIO_16x9			0x02

#define VMD_COLOR_SPACE_RGB			0x00
#define VMD_COLOR_SPACE_YCBCR422		0x01
#define VMD_COLOR_SPACE_YCBCR444		0x02

#define VMD_COLOR_DEPTH_8BIT			0x00
#define VMD_COLOR_DEPTH_10BIT			0x01
#define VMD_COLOR_DEPTH_12BIT			0x02
#define VMD_COLOR_DEPTH_16BIT			0x03

#define VMD_HDCP_NOT_AUTHENTICATED	0x00
#define VMD_HDCP_AUTHENTICATED		0x01

#define VMD_HDMIFORMAT_CEA_VIC		0x00
#define VMD_HDMIFORMAT_HDMI_VIC		0x01
#define VMD_HDMIFORMAT_3D		0x02
#define VMD_HDMIFORMAT_PC		0x03

/* These values are from HDMI Spec 1.4 Table H-2 */
#define VMD_3D_FRAMEPACKING			0
#define VMD_3D_FIELDALTERNATIVE		1
#define VMD_3D_LINEALTERNATIVE		2
#define VMD_3D_SIDEBYSIDEFULL			3
#define VMD_3D_LDEPTH					4
#define VMD_3D_LDEPTHGRAPHICS			5
#define VMD_3D_SIDEBYSIDEHALF			8


/* -------------------------------------------------------------------- */
/* System Macro Definitions */
/* -------------------------------------------------------------------- */
#define TX_HW_RESET_PERIOD      200
#define SII902XA_DEVICE_ID         0xB0

#define T_HPD_DELAY			10

/* -------------------------------------------------------------------- */
/* HDCP Macro Definitions */
/* -------------------------------------------------------------------- */
#define AKSV_SIZE			5
#define NUM_OF_ONES_IN_KSV	20

/* -------------------------------------------------------------------- */
/* EDID Constants Definition */
/* -------------------------------------------------------------------- */
#define EDID_BLOCK_0_OFFSET 0x00
#define EDID_BLOCK_1_OFFSET 0x80

#define EDID_BLOCK_SIZE      128
#define EDID_HDR_NO_OF_FF   0x06
#define NUM_OF_EXTEN_ADDR   0x7E

#define EDID_TAG_ADDR       0x00
#define EDID_REV_ADDR       0x01
#define EDID_TAG_IDX        0x02
#define LONG_DESCR_PTR_IDX  0x02
#define MISC_SUPPORT_IDX    0x03

#define ESTABLISHED_TIMING_INDEX        35	/* Offset of Established Timing in EDID block */
#define NUM_OF_STANDARD_TIMINGS          8
#define STANDARD_TIMING_OFFSET          38
#define LONG_DESCR_LEN                  18
#define NUM_OF_DETAILED_DESCRIPTORS      4

#define DETAILED_TIMING_OFFSET        0x36

/* Offsets within a Long Descriptors Block */
/* ==================================================== */
#define PIX_CLK_OFFSET				0
#define H_ACTIVE_OFFSET			2
#define H_BLANKING_OFFSET		3
#define V_ACTIVE_OFFSET				5
#define V_BLANKING_OFFSET                6
#define H_SYNC_OFFSET				8
#define H_SYNC_PW_OFFSET                 9
#define V_SYNC_OFFSET			10
#define V_SYNC_PW_OFFSET		10
#define H_IMAGE_SIZE_OFFSET		12
#define V_IMAGE_SIZE_OFFSET		13
#define H_BORDER_OFFSET			15
#define V_BORDER_OFFSET			16
#define FLAGS_OFFSET			17

#define AR16_10						0
#define AR4_3						1
#define AR5_4						2
#define AR16_9						3

/* Data Block Tag Codes */
/* ==================================================== */
#define AUDIO_D_BLOCK       0x01
#define VIDEO_D_BLOCK       0x02
#define VENDOR_SPEC_D_BLOCK 0x03
#define SPKR_ALLOC_D_BLOCK  0x04
#define USE_EXTENDED_TAG    0x07

/* Extended Data Block Tag Codes */
/* ==================================================== */
#define COLORIMETRY_D_BLOCK 0x05

#define HDMI_SIGNATURE_LEN  0x03

#define CEC_PHYS_ADDR_LEN   0x02
#define EDID_EXTENSION_TAG  0x02
#define EDID_REV_THREE      0x03
#define EDID_DATA_START     0x04

#define EDID_BLOCK_0        0x00
#define EDID_BLOCK_2_3      0x01

#define VIDEO_CAPABILITY_D_BLOCK 0x00





/* -------------------------------------------------------------------- */
/* TPI Register Definition */
/* -------------------------------------------------------------------- */


/* TPI AVI Input and Output Format Data */
/* / AVI Input Format Data */
#define INPUT_COLOR_SPACE_MASK				(BIT_1 | BIT_0)
#define INPUT_COLOR_SPACE_RGB					(0x00)
#define INPUT_COLOR_SPACE_YCBCR444			(0x01)
#define INPUT_COLOR_SPACE_YCBCR422			(0x02)
#define INPUT_COLOR_SPACE_BLACK_MODE			(0x03)


#define LINK_INTEGRITY_MODE_MASK				(BIT_6)
#define LINK_INTEGRITY_STATIC					(0x00)
#define LINK_INTEGRITY_DYNAMIC					(0x40)

#define TMDS_OUTPUT_CONTROL_MASK				(BIT_4)
#define TMDS_OUTPUT_CONTROL_ACTIVE			(0x00)
#define TMDS_OUTPUT_CONTROL_POWER_DOWN	(0x10)

#define AV_MUTE_MASK							(BIT_3)
#define AV_MUTE_NORMAL						(0x00)
#define AV_MUTE_MUTED							(0x08)

#define DDC_BUS_REQUEST_MASK					(BIT_2)
#define DDC_BUS_REQUEST_NOT_USING			(0x00)
#define DDC_BUS_REQUEST_REQUESTED			(0x04)

#define DDC_BUS_GRANT_MASK					(BIT_1)
#define DDC_BUS_GRANT_NOT_AVAILABLE			(0x00)
#define DDC_BUS_GRANT_GRANTED				(0x02)

#define OUTPUT_MODE_MASK						(BIT_0)
#define OUTPUT_MODE_DVI						(0x00)
#define OUTPUT_MODE_HDMI						(0x01)

#define CTRL_PIN_CONTROL_MASK					(BIT_4)
#define CTRL_PIN_TRISTATE						(0x00)
#define CTRL_PIN_DRIVEN_TX_BRIDGE				(0x10)

#define TX_POWER_STATE_MASK					(BIT_1 | BIT_0)
#define TX_POWER_STATE_D0						(0x00)
#define TX_POWER_STATE_D1						(0x01)
#define TX_POWER_STATE_D2						(0x02)
#define TX_POWER_STATE_D3						(0x03)

/* Configuration of I2S Interface */
#define SCK_SAMPLE_EDGE						(BIT_7)


#define AUDIO_MUTE_MASK						(BIT_4)
#define AUDIO_MUTE_NORMAL						(0x00)
#define AUDIO_MUTE_MUTED						(0x10)

#define AUDIO_SEL_MASK							(BITS_7_6)

/* -------------------------------------------------------------------- */
/* HDCP Implementation */
/* HDCP link security logic is implemented in certain transmitters; unique */
/* keys are embedded in each chip as part of the solution. The security */
/* scheme is fully automatic and handled completely by the hardware. */
/* -------------------------------------------------------------------- */

/* / HDCP Query Data Register */
#define EXTENDED_LINK_PROTECTION_MASK		(BIT_7)
#define EXTENDED_LINK_PROTECTION_NONE		(0x00)
#define EXTENDED_LINK_PROTECTION_SECURE		(0x80)

#define LOCAL_LINK_PROTECTION_MASK			(BIT_6)
#define LOCAL_LINK_PROTECTION_NONE			(0x00)
#define LOCAL_LINK_PROTECTION_SECURE			(0x40)

#define LINK_STATUS_MASK						(BIT_5 | BIT_4)
#define LINK_STATUS_NORMAL					(0x00)
#define LINK_STATUS_LINK_LOST					(0x10)
#define LINK_STATUS_RENEGOTIATION_REQ		(0x20)
#define LINK_STATUS_LINK_SUSPENDED			(0x30)

#define HDCP_REPEATER_MASK					(BIT_3)
#define HDCP_REPEATER_NO						(0x00)
#define HDCP_REPEATER_YES						(0x08)

#define CONNECTOR_TYPE_MASK					(BIT_2 | BIT_0)
#define CONNECTOR_TYPE_DVI						(0x00)
#define CONNECTOR_TYPE_RSVD					(0x01)
#define CONNECTOR_TYPE_HDMI					(0x04)
#define CONNECTOR_TYPE_FUTURE					(0x05)

#define PROTECTION_TYPE_MASK					(BIT_1)
#define PROTECTION_TYPE_NONE					(0x00)
#define PROTECTION_TYPE_HDCP					(0x02)

#define PROTECTION_LEVEL_MASK					(BIT_0)
#define PROTECTION_LEVEL_MIN					(0x00)
#define PROTECTION_LEVEL_MAX					(0x01)

#define KSV_FORWARD_MASK						(BIT_4)
#define KSV_FORWARD_ENABLE					(0x10)
#define KSV_FORWARD_DISABLE					(0x00)


/* / HDCP Revision Data Register */
#define HDCP_MAJOR_REVISION_MASK				(BIT_7 | BIT_6 | BIT_5 | BIT_4)
#define HDCP_MAJOR_REVISION_VALUE				(0x10)

#define HDCP_MINOR_REVISION_MASK				(BIT_3 | BIT_2 | BIT_1 | BIT_0)
#define HDCP_MINOR_REVISION_VALUE				(0x02)


#define HDCP_AUTH_STATUS_CHANGE_EN_MASK	(BIT_7)
#define HDCP_AUTH_STATUS_CHANGE_DISABLE		(0x00)
#define HDCP_AUTH_STATUS_CHANGE_ENABLE		(0x80)

#define HDCP_VPRIME_VALUE_READY_EN_MASK		(BIT_6)
#define HDCP_VPRIME_VALUE_READY_DISABLE		(0x00)
#define HDCP_VPRIME_VALUE_READY_ENABLE		(0x40)

#define HDCP_SECURITY_CHANGE_EN_MASK		(BIT_5)
#define HDCP_SECURITY_CHANGE_DISABLE			(0x00)
#define HDCP_SECURITY_CHANGE_ENABLE			(0x20)

#define AUDIO_ERROR_EVENT_EN_MASK			(BIT_4)
#define AUDIO_ERROR_EVENT_DISABLE				(0x00)
#define AUDIO_ERROR_EVENT_ENABLE				(0x10)

#define CPI_EVENT_NO_RX_SENSE_MASK			(BIT_3)
#define CPI_EVENT_NO_RX_SENSE_DISABLE		(0x00)
#define CPI_EVENT_NO_RX_SENSE_ENABLE			(0x08)

#define RECEIVER_SENSE_EVENT_EN_MASK			(BIT_1)
#define RECEIVER_SENSE_EVENT_DISABLE			(0x00)
#define RECEIVER_SENSE_EVENT_ENABLE			(0x02)

#define HOT_PLUG_EVENT_EN_MASK				(BIT_0)
#define HOT_PLUG_EVENT_DISABLE				(0x00)
#define HOT_PLUG_EVENT_ENABLE					(0x01)

#define HDCP_AUTH_STATUS_CHANGE_EVENT_MASK	(BIT_7)
#define HDCP_AUTH_STATUS_CHANGE_EVENT_NO	(0x00)
#define HDCP_AUTH_STATUS_CHANGE_EVENT_YES	(0x80)

#define HDCP_VPRIME_VALUE_READY_EVENT_MASK	(BIT_6)
#define HDCP_VPRIME_VALUE_READY_EVENT_NO	(0x00)
#define HDCP_VPRIME_VALUE_READY_EVENT_YES	(0x40)

#define HDCP_SECURITY_CHANGE_EVENT_MASK		(BIT_5)
#define HDCP_SECURITY_CHANGE_EVENT_NO		(0x00)
#define HDCP_SECURITY_CHANGE_EVENT_YES		(0x20)

#define AUDIO_ERROR_EVENT_MASK				(BIT_4)
#define AUDIO_ERROR_EVENT_NO					(0x00)
#define AUDIO_ERROR_EVENT_YES					(0x10)

#define CPI_EVENT_MASK							(BIT_3)
#define CPI_EVENT_NO							(0x00)
#define CPI_EVENT_YES							(0x08)
#define RX_SENSE_MASK							(BIT_3)	/* This bit is dual purpose depending on the value of 0x3C[3] */
#define RX_SENSE_NOT_ATTACHED					(0x00)
#define RX_SENSE_ATTACHED						(0x08)

#define HOT_PLUG_PIN_STATE_MASK				(BIT_2)
#define HOT_PLUG_PIN_STATE_LOW				(0x00)
#define HOT_PLUG_PIN_STATE_HIGH				(0x04)

#define RECEIVER_SENSE_EVENT_MASK				(BIT_1)
#define RECEIVER_SENSE_EVENT_NO				(0x00)
#define RECEIVER_SENSE_EVENT_YES				(0x02)

#define HOT_PLUG_EVENT_MASK					(BIT_0)
#define HOT_PLUG_EVENT_NO						(0x00)
#define HOT_PLUG_EVENT_YES					(0x01)



#define KSV_FIFO_READY_MASK					(BIT_1)
#define KSV_FIFO_READY_NO						(0x00)
#define KSV_FIFO_READY_YES						(0x02)


#define KSV_FIFO_READY_EN_MASK				(BIT_1)
#define KSV_FIFO_READY_DISABLE				(0x00)
#define KSV_FIFO_READY_ENABLE					(0x02)


#define KSV_FIFO_LAST_MASK						(BIT_7)
#define KSV_FIFO_LAST_NO						(0x00)
#define KSV_FIFO_LAST_YES						(0x80)

#define KSV_FIFO_COUNT_MASK					(BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0)


/* H/W Optimization Control Register #3 Set */
#define DDC_DELAY_BIT9_MASK					(BIT_7)
#define DDC_DELAY_BIT9_NO						(0x00)
#define DDC_DELAY_BIT9_YES						(0x80)
#define RI_CHECK_SKIP_MASK						(BIT_3)
#define RI_CHECK_SKIP_NO						(0x00)
#define RI_CHECK_SKIP_YES						(0x08)

/* Misc InfoFrames */
#define MISC_INFO_FRAMES_CTRL					(0xBF)
#define MISC_INFO_FRAMES_TYPE					(0xC0)
#define MISC_INFO_FRAMES_VER					(0xC1)
#define MISC_INFO_FRAMES_LEN					(0xC2)
#define MISC_INFO_FRAMES_CHKSUM				(0xC3)
/* -------------------------------------------------------------------- */
void DelayMS(word MS);

byte I2CReadBlock(struct i2c_client *client, byte RegAddr, byte NBytes, byte *Data);
byte I2CWriteBlock(struct i2c_client *client, byte RegAddr, byte NBytes, byte *Data);
byte siiReadSegmentBlockEDID(struct i2c_client *client, byte Segment, byte Offset, byte *Buffer,
			     byte Length);

void WriteByteTPI(byte RegOffset, byte Data);
byte ReadByteTPI(byte RegOffset);
void siHdmiTx_PowerStateD2(void);
void siHdmiTx_PowerStateD0fromD2(void);
void siHdmiTx_PowerStateD3(void);
void siHdmiTx_Init(void);
byte siHdmiTx_VideoSet(void);
byte siHdmiTx_AudioSet(void);
byte siHdmiTx_TPI_Init(void);
void siHdmiTx_TPI_Poll(void);
void siHdmiTx_VideoSel(byte vmode);
void siHdmiTx_AudioSel(byte Afs);
#endif
