/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

// ===================================================== //

#define REG_TPI_HDCP_TIMER_1_SEC		TX_PAGE_TPI , 0x0002

#define REG_TPI_INPUT				TX_PAGE_TPI , 0x0009
#define REG_TPI_INPUT_DEFVAL			0x00 
#define BIT_TPI_INPUT_FORMAT_MASK		0x03
#define BIT_TPI_INPUT_FORMAT_RGB		0x00
#define BIT_TPI_INPUT_FORMAT_YCbCr444		0x01
#define BIT_TPI_INPUT_FORMAT_YCbCr422		0x02
#define BIT_TPI_INPUT_FORMAT_INTERNAL_RGB	0x03

#define BIT_TPI_INPUT_QUAN_RANGE_MASK		0x0C
#define BIT_TPI_INPUT_QUAN_RANGE_AUTO		0x00
#define BIT_TPI_INPUT_QUAN_RANGE_FULL		0x04	// Design: expand input to full color space range	// TODO: FD, TBD, not used
#define BIT_TPI_INPUT_QUAN_RANGE_LIMITED	0x08	// Design: leave input color space range as is		// TODO: FD, TBD, not used
#define BIT_TPI_INPUT_QUAN_RANGE_RSVD		0x0C

#define REG_TPI_OUTPUT				TX_PAGE_TPI , 0x000A
#define REG_TPI_OUTPUT_DEFVAL			0x00
#define BIT_TPI_OUTPUT_FORMAT_MASK		0x03
#define BIT_TPI_OUTPUT_FORMAT_HDMI_TO_RGB	0x00
#define BIT_TPI_OUTPUT_FORMAT_YCbCr444		0x01
#define BIT_TPI_OUTPUT_FORMAT_YCbCr422		0x02
#define BIT_TPI_OUTPUT_FORMAT_DVI_TO_RGB	0x03

#define BIT_TPI_OUTPUT_QUAN_RANGE_MASK		0x0C
#define BIT_TPI_OUTPUT_QUAN_RANGE_AUTO		0x00
#define BIT_TPI_OUTPUT_QUAN_RANGE_FULL		0x04	// Design: compress output to limited color space range	// TODO: FD, TBD, not used
#define BIT_TPI_OUTPUT_QUAN_RANGE_LIMITED	0x08	// Design: leave output color space range as is		// TODO: FD, TBD, not used
#define BIT_TPI_OUTPUT_QUAN_RANGE_RSVD		0x0C

#define REG_TPI_AVI_CHSUM			TX_PAGE_TPI , 0x000C

#define REG_TPI_AVI_BYTE13			TX_PAGE_TPI , 0x0019

#define TPI_SYSTEM_CONTROL_DATA_REG		TX_PAGE_TPI , 0x001A

#define TPI_1A_AUTO_REAUTHENTICATION_MASK	(0x40)	// TODO: FD, TBD, not used, not in PR, should be removed
#define TPI_1A_AUTO_REAUTHENTICATION_OFF	(0x00)	// TODO: FD, TBD, not used, not in PR, should be removed
#define TPI_1A_AUTO_REAUTHENTICATION_ENABLE	(0x40)	// TODO: FD, TBD, not used, not in PR, should be removed

#define TMDS_OUTPUT_CONTROL_MASK		(0x10)
#define TMDS_OUTPUT_CONTROL_ACTIVE		(0x00)
#define TMDS_OUTPUT_CONTROL_POWER_DOWN		(0x10)

#define AV_MUTE_MASK				(0x08)
#define AV_MUTE_NORMAL				(0x00)
#define AV_MUTE_MUTED				(0x08)

#define DDC_BUS_REQUEST_MASK			(0x04)	// TODO: FD, TBD, not used, not in PR, should be removed
#define DDC_BUS_REQUEST_NOT_USING		(0x00)	// TODO: FD, TBD, not used, not in PR, should be removed
#define DDC_BUS_REQUEST_REQUESTED		(0x04)	// TODO: FD, TBD, not used, not in PR, should be removed

#define DDC_BUS_GRANT_MASK			(0x02)	// TODO: FD, TBD, not used, not in PR, should be removed
#define DDC_BUS_GRANT_NOT_AVAILABLE		(0x00)	// TODO: FD, TBD, not used, not in PR, should be removed
#define DDC_BUS_GRANT_GRANTED			(0x02)	// TODO: FD, TBD, not used, not in PR, should be removed

#define TMDS_OUTPUT_MODE_MASK			(0x01)	// confirmed with design
#define TMDS_OUTPUT_MODE_DVI			(0x00)
#define TMDS_OUTPUT_MODE_HDMI			(0x01)

// ===================================================== //

#define TPI_DEVICE_POWER_STATE_CTRL_REG		TX_PAGE_TPI , 0x001E

#define TX_POWER_STATE_MASK			(0x03)
#define TX_POWER_STATE_D0			(0x00)
#define TX_POWER_STATE_D2			(0x02)
#define TX_POWER_STATE_D3			(0x03)


#define REG_TPI_AUDIO_MAPPING_CONFIG		TX_PAGE_TPI , 0x001F
#define BIT_TPI_AUDIO_SD_ENABLE			(0x80)
#define BIT_TPI_AUDIO_SD_DISABLE		(0x00)
#define BIT_TPI_AUDIO_FIFO_MAP_3		(0x30)
#define BIT_TPI_AUDIO_FIFO_MAP_2		(0x20)
#define BIT_TPI_AUDIO_FIFO_MAP_1		(0x10)
#define BIT_TPI_AUDIO_FIFO_MAP_0		(0x00)
#define BIT_TPI_AUDIO_SD_SEL_3			(0x03)
#define BIT_TPI_AUDIO_SD_SEL_2			(0x02)
#define BIT_TPI_AUDIO_SD_SEL_1			(0x01)
#define BIT_TPI_AUDIO_SD_SEL_0			(0x00)

#define REG_TPI_CONFIG1                     TX_PAGE_TPI , 0x0024
typedef enum{
     BIT_TPI_CONFIG1_AUDIO_FREQUENCY_192K	= 0x0E
	 ,BIT_TPI_CONFIG1_AUDIO_FREQUENCY_96K	= 0x0A
    ,BIT_TPI_CONFIG1_AUDIO_FREQUENCY_48K	= 0x02
	,BIT_TPI_CONFIG1_AUDIO_FREQUENCY_44K	 = 0x00
	,BIT_TPI_CONFIG1_AUDIO_FREQUENCY_32K	  = 0x03
}TpiConfig1Bits_e;

#define REG_TPI_CONFIG2                     TX_PAGE_TPI , 0x0025	// TODO: FD, TBD, not used
typedef enum{
     BIT_TPI_AUDIO_HANDLING_MASK                         = 0x03
    ,BIT_TPI_AUDIO_HANDLING_PASS_BASIC_AUDIO_ONLY        = 0x00
    ,BIT_TPI_AUDIO_HANDLING_PASS_ALL_AUDIO_MODES         = 0x01
    ,BIT_TPI_AUDIO_HANDLING_DOWNSAMPLE_INCOMING_AS_NEEDED= 0x02
    ,BIT_TPI_AUDIO_HANDLING_DO_NOT_CHECK_AUDIO_STREAM    = 0x03
}TpiConfig2Bits_e;

#define REG_TPI_CONFIG3                     TX_PAGE_TPI , 0x0026	// TODO: FD, TBD, not used
typedef enum{
     BIT_TPI_AUDIO_CODING_TYPE_MASK             = 0x0F
    ,BIT_TPI_AUDIO_CODING_TYPE_STREAM_HEADER    = 0x00
    ,BIT_TPI_AUDIO_CODING_TYPE_PCM              = 0x01
    ,BIT_TPI_AUDIO_CODING_TYPE_AC3              = 0x02
    ,BIT_TPI_AUDIO_CODING_TYPE_MPEG1            = 0x03
    ,BIT_TPI_AUDIO_CODING_TYPE_MP3              = 0x04
    ,BIT_TPI_AUDIO_CODING_TYPE_MPEG2            = 0x05
    ,BIT_TPI_AUDIO_CODING_TYPE_AAC              = 0x06
    ,BIT_TPI_AUDIO_CODING_TYPE_DTS              = 0x07
    ,BIT_TPI_AUDIO_CODING_TYPE_ATRAC            = 0x08

    ,BIT_TPI_CONFIG3_MUTE_MASK                  = 0x10
    ,BIT_TPI_CONFIG3_MUTE_NORMAL                = 0x00
    ,BIT_TPI_CONFIG3_MUTE_MUTED                 = 0x10

    ,BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_MASK    = 0x20
    ,BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH     = 0x00
    ,BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_8CH_MAX = 0x20

	,BIT_TPI_CONFIG3_AUDIO_TDM_MSB_No_DELAY	 = 0x00
	,BIT_TPI_CONFIG3_AUDIO_TDM_MSB_1CLK_DEALY	 = 0x01
	,BIT_TPI_CONFIG3_AUDIO_TDM_MSB_2CLK_DELAY	 = 0x02

	,BIT_TPI_CONFIG3_AUDIO_TDM_FS_POL	 = 0x00
	,BIT_TPI_CONFIG3_AUDIO_TDM_FS_NEG	 = 0x04
 
	,BIT_TPI_CONFIG3_AUDIO_TDM_32B_CH	 = 0x00
	,BIT_TPI_CONFIG3_AUDIO_TDM_16B_CH	 = 0x08

    ,BIT_TPI_CONFIG_3_AUDIO_INTERFACE_MASK      = 0xC0
    ,BIT_TPI_CONFIG_3_AUDIO_INTERFACE_DISABLED  = 0x00
    ,BIT_TPI_CONFIG_3_AUDIO_INTERFACE_SPDIF     = 0x40
    ,BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S       = 0x80
    ,BIT_TPI_CONFIG_3_AUDIO_INTERFACE_HD_AUDIO  = 0xC0
}TpiConfig3Bits_e;

#define REG_TPI_CONFIG4                     TX_PAGE_TPI , 0x0027
typedef enum{
	 BIT_TPI_CONFIG4_AUDIO_Refer_To_Stream_Header 	= 0x00
	,BIT_TPI_CONFIG4_AUDIO_WIDTH_16_BIT	 	= 0x40
	,BIT_TPI_CONFIG4_AUDIO_WIDTH_20_BIT	 	= 0x80
    ,BIT_TPI_CONFIG4_AUDIO_WIDTH_24_BIT		= 0xC0
    ,BIT_TPI_CONFIG4_AUDIO_FREQUENCY_192K		= 0x38
    ,BIT_TPI_CONFIG4_AUDIO_FREQUENCY_48K		= 0x18
        ,BIT_TPI_CONFIG4_AUDIO_FREQUENCY_96K		= 0x28
	,BIT_TPI_CONFIG4_AUDIO_FREQUENCY_44K	 	= 0x10
	,BIT_TPI_CONFIG4_AUDIO_FREQUENCY_32K		= 0x08
    ,BIT_TPI_CONFIG4_AUDIO_CHANNEL_8CH		= 0x07
    ,BIT_TPI_CONFIG4_AUDIO_CHANNEL_2CH		= 0x01
}TpiConfig4Bits_e;
/*\
 HDCP Implementation

 HDCP link security logic is implemented in certain transmitters; unique
   keys are embedded in each chip as part of the solution. The security
   scheme is fully automatic and handled completely by the hardware.
\*/

/// HDCP Query Data Register ============================================== ///

#define TPI_HDCP_QUERY_DATA_REG				TX_PAGE_TPI , 0x0029

#define EXTENDED_LINK_PROTECTION_MASK			(0x80)
#define EXTENDED_LINK_PROTECTION_NONE			(0x00)
#define EXTENDED_LINK_PROTECTION_SECURE			(0x80)

#define LOCAL_LINK_PROTECTION_MASK			(0x40)
#define LOCAL_LINK_PROTECTION_NONE			(0x00)
#define LOCAL_LINK_PROTECTION_SECURE			(0x40)

#define LINK_STATUS_MASK				(0x30)
#define LINK_STATUS_NORMAL				(0x00)
#define LINK_STATUS_LINK_LOST				(0x10)
#define LINK_STATUS_RENEGOTIATION_REQ			(0x20)
#define LINK_STATUS_LINK_SUSPENDED			(0x30)

#define HDCP_REPEATER_MASK				(0x08)
#define HDCP_REPEATER_NO				(0x00)
#define HDCP_REPEATER_YES				(0x08)

#define CONNECTOR_TYPE_MASK				(0x05)
#define CONNECTOR_TYPE_DVI				(0x00)	// confirmed with design
#define CONNECTOR_TYPE_RSVD				(0x01)
#define CONNECTOR_TYPE_HDMI				(0x04)	// confirmed with design
#define CONNECTOR_TYPE_FUTURE				(0x05)

#define PROTECTION_TYPE_MASK				(0x02)
#define PROTECTION_TYPE_NONE				(0x00)
#define PROTECTION_TYPE_HDCP				(0x02)

/// HDCP Control Data Register ============================================ ///

#define TPI_HDCP_CONTROL_DATA_REG			TX_PAGE_TPI , 0x002A

typedef enum
{
     BIT_TPI_HDCP_CONTROL_DATA_COPP_PROTLEVEL_MASK    = 0x01
    ,BIT_TPI_HDCP_CONTROL_DATA_COPP_PROTLEVEL_MIN     = 0x00
    ,BIT_TPI_HDCP_CONTROL_DATA_COPP_PROTLEVEL_MAX     = 0x01

    ,BIT_TPI_HDCP_CONTROL_DATA_DOUBLE_RI_CHECK_MASK   = 0x04
    ,BIT_TPI_HDCP_CONTROL_DATA_DOUBLE_RI_CHECK_DISABLE= 0x00
    ,BIT_TPI_HDCP_CONTROL_DATA_DOUBLE_RI_CHECK_ENABLE = 0x04

}TpiHdcpControlDataBits_e;

#define PROTECTION_LEVEL_MASK				(0x01)
#define PROTECTION_LEVEL_MIN				(0x00)
#define PROTECTION_LEVEL_MAX				(0x01)

#define KSV_FORWARD_MASK				(0x10)
#define KSV_FORWARD_ENABLE				(0x10)
#define KSV_FORWARD_DISABLE				(0x00)

/// HDCP BKSV Registers =================================================== ///

#define TPI_BKSV_1_REG					TX_PAGE_TPI , 0x002B
#define TPI_BKSV_2_REG					TX_PAGE_TPI , 0x002C
#define TPI_BKSV_3_REG					TX_PAGE_TPI , 0x002D
#define TPI_BKSV_4_REG					TX_PAGE_TPI , 0x002E
#define TPI_BKSV_5_REG					TX_PAGE_TPI , 0x002F

/// HDCP Revision Data Register =========================================== ///

#define TPI_HDCP_REVISION_DATA_REG			TX_PAGE_TPI , 0x0030	// TODO: FD, TBD, not used

#define HDCP_MAJOR_REVISION_MASK			(0xF0)
#define HDCP_MAJOR_REVISION_VALUE			(0x10)

#define HDCP_MINOR_REVISION_MASK			(0x0F)
#define HDCP_MINOR_REVISION_VALUE			(0x02)

/// HDCP KSV and V' Value Data Register =================================== ///


#define TPI_V_PRIME_SELECTOR_REG			TX_PAGE_TPI , 0x0031

/// V' Value Readback Registers =========================================== ///

#define TPI_V_PRIME_7_0_REG				TX_PAGE_TPI , 0x0032
#define TPI_V_PRIME_15_9_REG				TX_PAGE_TPI , 0x0033
#define TPI_V_PRIME_23_16_REG				TX_PAGE_TPI , 0x0034
#define TPI_V_PRIME_31_24_REG				TX_PAGE_TPI , 0x0035

/// HDCP AKSV Registers =================================================== ///

#define TPI_AKSV_1_REG					TX_PAGE_TPI , 0x0036
#define TPI_AKSV_2_REG					TX_PAGE_TPI , 0x0037
#define TPI_AKSV_3_REG					TX_PAGE_TPI , 0x0038
#define TPI_AKSV_4_REG					TX_PAGE_TPI , 0x0039
#define TPI_AKSV_5_REG					TX_PAGE_TPI , 0x003A

/// Interrupt Enable Register ============================================= ///

#define REG_TPI_INTR_ST0_ENABLE			        TX_PAGE_TPI , 0x003C
#define REG_TPI_INTR_ST1_ENABLE			        TX_PAGE_TPI , 0x003F


/// Interrupt Status Register ============================================= ///

#define REG_TPI_INTR_ST0			TX_PAGE_TPI , 0x003D
typedef enum{
     BIT_TPI_INTR_ST0_AUDIO_ERROR_EVENT             = 0x10
    ,BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT    = 0x20
    ,BIT_TPI_INTR_ST0_HDCP_VPRIME_VALUE_READY_EVENT = 0x40
    ,BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT = 0x80
}TpiIntrSt0Bits_e;

#define REG_TPI_INTR_ST1			TX_PAGE_TPI , 0x003E
typedef enum{
     BIT_TPI_INTR_ST1_BKSV_ERR                      = 0x02
    ,BIT_TPI_INTR_ST1_BKSV_DONE                     = 0x04
    ,BIT_TPI_INTR_ST1_KSV_FIFO_FIRST                = 0x08
}TpiIntrSt1Bits_e;

#define REG_TPI_BSTATUS2			TX_PAGE_TPI , 0x46
typedef enum{
	BIT_DS_CASC_EXCEEDED			= 0x08
}tpi_bstatus2_e;

#define REG_TPI_HW_DBG1             TX_PAGE_TPI , 0x79
#define REG_TPI_HW_DBG2             TX_PAGE_TPI , 0x7A
#define REG_TPI_HW_DBG3             TX_PAGE_TPI , 0x7B
#define REG_TPI_HW_DBG4             TX_PAGE_TPI , 0x7C
#define REG_TPI_HW_DBG5             TX_PAGE_TPI , 0x7D
#define REG_TPI_HW_DBG6             TX_PAGE_TPI , 0x7E
#define REG_TPI_HW_DBG7             TX_PAGE_TPI , 0x7F

// Define the rest here when needed.

#define TPI_REG_HW_OPT1_B9            	TX_PAGE_TPI , 0x00B9	// TODO: FD, TBD, not used

#define REG_TPI_HW_OPT3			TX_PAGE_TPI , 0x00BB

#define REG_TPI_INFO_FSEL		TX_PAGE_TPI , 0x00BF
typedef enum{
     BIT_TPI_INFO_SEL_MASK                  = 0x07
    ,BIT_TPI_INFO_SEL_AVI                   = 0x00
    ,BIT_TPI_INFO_SEL_SPD                   = 0x01
    ,BIT_TPI_INFO_SEL_Audio                 = 0x02
    ,BIT_TPI_INFO_SEL_MPEG                  = 0x03
    ,BIT_TPI_INFO_SEL_GENERIC               = 0x04
    ,BIT_TPI_INFO_SEL_GENERIC2              = 0x05
    ,BIT_TPI_INFO_SEL_3D_VSIF               = 0x06

    ,BIT_TPI_INFO_READ_FLAG_MASK            = 0x20
    ,BIT_TPI_INFO_READ_FLAG_NO_READ         = 0x00
    ,BIT_TPI_INFO_READ_FLAG_READ            = 0x20
    ,BIT_TPI_INFO_RPT                       = 0x40
    ,BIT_TPI_INFO_EN                        = 0x80
}TpiInfoFSelBits_e;


#define REG_TPI_INFO_BYTE00                    TX_PAGE_TPI , 0x00C0
#define REG_TPI_INFO_BYTE01                    TX_PAGE_TPI , 0x00C1
#define REG_TPI_INFO_BYTE02                    TX_PAGE_TPI , 0x00C2
#define REG_TPI_INFO_BYTE03                    TX_PAGE_TPI , 0x00C3
#define REG_TPI_INFO_BYTE04                    TX_PAGE_TPI , 0x00C4
#define REG_TPI_INFO_BYTE05                    TX_PAGE_TPI , 0x00C5
#define REG_TPI_INFO_BYTE06                    TX_PAGE_TPI , 0x00C6
#define REG_TPI_INFO_BYTE07                    TX_PAGE_TPI , 0x00C7
#define REG_TPI_INFO_BYTE08                    TX_PAGE_TPI , 0x00C8
#define REG_TPI_INFO_BYTE09                    TX_PAGE_TPI , 0x00C9
#define REG_TPI_INFO_BYTE10                    TX_PAGE_TPI , 0x00CA
#define REG_TPI_INFO_BYTE11                    TX_PAGE_TPI , 0x00CB
#define REG_TPI_INFO_BYTE12                    TX_PAGE_TPI , 0x00CC
#define REG_TPI_INFO_BYTE13                    TX_PAGE_TPI , 0x00CD
#define REG_TPI_INFO_BYTE14                    TX_PAGE_TPI , 0x00CE
#define REG_TPI_INFO_BYTE15                    TX_PAGE_TPI , 0x00CF
#define REG_TPI_INFO_BYTE16                    TX_PAGE_TPI , 0x00D0
#define REG_TPI_INFO_BYTE17                    TX_PAGE_TPI , 0x00D1
#define REG_TPI_INFO_BYTE18                    TX_PAGE_TPI , 0x00D2
#define REG_TPI_INFO_BYTE19                    TX_PAGE_TPI , 0x00D3
#define REG_TPI_INFO_BYTE20                    TX_PAGE_TPI , 0x00D4
#define REG_TPI_INFO_BYTE21                    TX_PAGE_TPI , 0x00D5
#define REG_TPI_INFO_BYTE22                    TX_PAGE_TPI , 0x00D6
#define REG_TPI_INFO_BYTE23                    TX_PAGE_TPI , 0x00D7
#define REG_TPI_INFO_BYTE24                    TX_PAGE_TPI , 0x00D8
#define REG_TPI_INFO_BYTE25                    TX_PAGE_TPI , 0x00D9
#define REG_TPI_INFO_BYTE26                    TX_PAGE_TPI , 0x00DA
#define REG_TPI_INFO_BYTE27                    TX_PAGE_TPI , 0x00DB
#define REG_TPI_INFO_BYTE28                    TX_PAGE_TPI , 0x00DC
#define REG_TPI_INFO_BYTE29                    TX_PAGE_TPI , 0x00DD
#define REG_TPI_INFO_BYTE30                    TX_PAGE_TPI , 0x00DE
#define REG_TPI_INFO_BYTE31                    TX_PAGE_TPI , 0x00DF
#define REG_TPI_INFO_BYTE32                    TX_PAGE_TPI , 0x00E0
#define REG_TPI_INFO_BYTE33                    TX_PAGE_TPI , 0x00E1
#define REG_TPI_INFO_BYTE34                    TX_PAGE_TPI , 0x00E2
#define REG_TPI_INFO_BYTE35                    TX_PAGE_TPI , 0x00E3
#define REG_TPI_INFO_BYTE36                    TX_PAGE_TPI , 0x00E4
#define REG_TPI_INFO_BYTE37                    TX_PAGE_TPI , 0x00E5
#define REG_TPI_INFO_BYTE38                    TX_PAGE_TPI , 0x00E6
#define REG_TPI_INFO_BYTE39                    TX_PAGE_TPI , 0x00E7
#define REG_TPI_INFO_BYTE40                    TX_PAGE_TPI , 0x00E8
#define REG_TPI_INFO_BYTE41                    TX_PAGE_TPI , 0x00E9
#define REG_TPI_INFO_BYTE42                    TX_PAGE_TPI , 0x00EA
#define REG_TPI_INFO_BYTE43                    TX_PAGE_TPI , 0x00EB
#define REG_TPI_INFO_BYTE44                    TX_PAGE_TPI , 0x00EC
#define REG_TPI_INFO_BYTE45                    TX_PAGE_TPI , 0x00ED
#define REG_TPI_INFO_BYTE46                    TX_PAGE_TPI , 0x00EE
#define REG_TPI_INFO_BYTE47                    TX_PAGE_TPI , 0x00EF
