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

/*
 * @file  si_8348_regs.h
 *
 * @brief Register descriptions for the Sil-8348
 * MHL transmitter registers.
 */

#include "sii_hal.h"

#if 0///SII_I2C_ADDR==(0x76)
typedef enum
{
	 TX_PAGE_L0		=	0x76
	,TX_PAGE_L1		=	0x7E
	,TX_PAGE_3		=	0x9E
	,TX_PAGE_TPI		=	0x96
	,TX_PAGE_CBUS		=	0xCC
	,TX_PAGE_DDC_EDID	=	0xA0
}slave_addr_t;

#else
typedef enum
{
	 TX_PAGE_L0		=	0x72
	,TX_PAGE_L1		=	0x7A
	,TX_PAGE_3		=	0x9A
	,TX_PAGE_TPI		=	0x92
	,TX_PAGE_CBUS		=	0xC8
	,TX_PAGE_DDC_EDID	=	0xA0
}slave_addr_t;

#endif

#define REG_DEV_IDL									TX_PAGE_L0, 0x02
#define REG_DEV_IDH									TX_PAGE_L0, 0x03
#define REG_DEV_REV									TX_PAGE_L0, 0x04

#define REG_SYS_CTRL1									TX_PAGE_L0, 0x08

#define REG_SYS_STAT									TX_PAGE_L0, 0x09
#define		BIT_STAT_RSEN							0x04

#define	REG_DCTL									TX_PAGE_L0, 0x0D
#define		BIT_DCTL_TCLK3X_PHASE						0x01				// TODO: FD, TBD, not used
#define		BIT_DCTL_EXT_DDC_SEL						0x10				// TODO: FD, TBD, not used

#define REG_HDCP_CTRL									TX_PAGE_L0, 0x0F	// TODO: FD, TBD, not used
#define		BIT_CP_RESETN_MASK						0x04
#define		BIT_CP_RESETN_RESET						0x00
#define		BIT_CP_RESETN_RELEASE						0x04

#define	TPI_HDCP_RI_LOW_REG								TX_PAGE_L0, 0x22	// TODO: FD, TBD, not used
#define	TPI_HDCP_RI_HIGH_REG								TX_PAGE_L0, 0x23	// TODO: FD, TBD, not used

#define REG_RI_CMD									TX_PAGE_L0, 0x27	// TODO: FD, TBD, not used

#define		BIT_RI_CMD_ENABLE_RI_CHECK_MASK					0x01
#define		BIT_RI_CMD_ENABLE_RI_CHECK_DISABLE				0x00
#define		BIT_RI_CMD_ENABLE_RI_CHECK_ENABLE				0x01

#define	REG_HRESL									TX_PAGE_L0, 0x3A
#define	REG_HRESH									TX_PAGE_L0, 0x3B
#define	REG_VRESL									TX_PAGE_L0, 0x3C
#define	REG_VRESH									TX_PAGE_L0, 0x3D

#define	REG_VID_MODE									TX_PAGE_L0, 0x4A
#define		REG_VID_MODE_DEFVAL						0x00
#define		BIT_VID_MODE_m1080p_MASK					0x40
#define		BIT_VID_MODE_m1080p_DISABLE					0x00
#define		BIT_VID_MODE_m1080p_ENABLE					0x40

#define	REG_INTR_STATE									TX_PAGE_L0, 0x70	// TODO: FD, TBD, not used

#define	REG_INTR1									TX_PAGE_L0, 0x71
#define		BIT_INTR1_RSEN_CHG						0x20
#define		BIT_INTR1_HPD_CHG						0x40

#define	REG_INTR2									TX_PAGE_L0, 0x72	// TODO: FD, TBD, not used
#define		BIT_INTR2_PSTABLE						0x02

#define	REG_INTR3									TX_PAGE_L0, 0x73
#define 	BIT_INTR3_DDC_FIFO_FULL						0x02
#define 	BIT_INTR3_DDC_CMD_DONE						0x08

#define	REG_INTR1_MASK									TX_PAGE_L0, 0x75	// TODO: FD, TBD, not used
#define	REG_INTR2_MASK									TX_PAGE_L0, 0x76	// TODO: FD, TBD, not used
#define	REG_INTR3_MASK									TX_PAGE_L0, 0x77
#define	REG_INTR5_MASK									TX_PAGE_L0, 0x78	// TODO: FD, TBD, not used

#define	REG_HPD_CTRL									TX_PAGE_L0, 0x79
#define		BIT_HPD_CTRL_HPD_OUT_OVR_EN_MASK				0x10
#define		BIT_HPD_CTRL_HPD_OUT_OVR_EN_OFF					0x00
#define		BIT_HPD_CTRL_HPD_OUT_OVR_EN_ON					0x10
#define		BIT_HPD_CTRL_HPD_OUT_OVR_VAL_MASK				0x20
#define		BIT_HPD_CTRL_HPD_OUT_OVR_VAL_OFF				0x00
#define		BIT_HPD_CTRL_HPD_OUT_OVR_VAL_ON					0x20
#define		BIT_HPD_CTRL_HPD_OUT_OD_EN_MASK					0x40
#define		BIT_HPD_CTRL_HPD_OUT_OD_EN_DISABLED				0x00
#define		BIT_HPD_CTRL_HPD_OUT_OD_EN_ENABLED				0x40

#define	REG_TMDS_CCTRL									TX_PAGE_L0, 0x80
#define		BIT_TMDS_CCTRL_BGRCTL_MASK					0x07
#define		BIT_TMDS_CCTRL_SEL_BGR_MASK					0x08
#define		BIT_TMDS_CCTRL_SEL_BGR						0x08
#define		BIT_TMDS_CCTRL_TMDS_OE_MASK					0x10
#define		BIT_TMDS_CCTRL_TMDS_OE						0x10

#define	REG_TXMZ_CTRL2									TX_PAGE_L0, 0xB1

#define	REG_TPI_SEL									TX_PAGE_L0, 0xC7
#define		BIT_TPI_SEL_SW_TPI_EN_MASK					0x80
#define		BIT_TPI_SEL_SW_TPI_EN_HW_TPI					0x00
#define		BIT_TPI_SEL_SW_TPI_EN_NON_HW_TPI				0x80

#define	REG_DDC_ADDR									TX_PAGE_L0, 0xED
#define	REG_DDC_SEGM									TX_PAGE_L0, 0xEE
#define	REG_DDC_OFFSET									TX_PAGE_L0, 0xEF

#define	REG_DDC_DIN_CNT1								TX_PAGE_L0, 0xF0
#define	REG_DDC_DIN_CNT2								TX_PAGE_L0, 0xF1

#define	REG_DDC_STATUS									TX_PAGE_L0, 0xF2
#define		BIT_DDC_STATUS_DDC_NO_ACK					0x20

#define	REG_DDC_CMD									TX_PAGE_L0, 0xF3
#define		BIT_DDC_CMD_COMMAND_MASK					0x0F
#define		BIT_DDC_CMD_COMMAND_ABPRT					0x0F
#define		BIT_DDC_CMD_COMMAND_CLEAR_FIFO					0x09
#define		BIT_DDC_CMD_COMMAND_ENHANCED_READ_NO_ACK			0x04

#define	REG_DDC_DATA									TX_PAGE_L0, 0xF4

#define	REG_USB_CHARGE_PUMP_MHL								TX_PAGE_L0, 0xF7
#define		BIT_USE_CHARGE_PUMP_MHL_DEFAULT					0x03

#define	REG_USB_CHARGE_PUMP								TX_PAGE_L0, 0xF8
#define		BIT_USE_CHARGE_PUMP_DEFAULT					0x8C

#define	REG_EPCM									TX_PAGE_L0, 0xFA	// TODO: FD, TBD, not used
#define		BIT_EPCM_LD_KSV_MASK						0x20
#define		BIT_EPCM_LD_KSV_DISABLE						0x00
#define		BIT_EPCM_LD_KSV_ENABLE						0x20

#define REG_DPD										TX_PAGE_L1, 0x3D

#define	REG_INF_CTRL1									TX_PAGE_L1, 0x3E	// TODO: FD, TBD, not used
#define	REG_INF_CTRL2									TX_PAGE_L1, 0x3F	// TODO: FD, TBD, not used

#define	REG_SRST									TX_PAGE_3, 0x00
#define		BIT_MHL_FIFO_AUTO_RST						0x80
#define		BIT_AUDIO_FIFO_RST_SET						0x02
#define		BIT_AUDIO_FIFO_RST_CLR						0x00
#define		BIT_AUDIO_FIFO_RST_MASK						0x02

#define	REG_POWER_CTRL									TX_PAGE_3, 0x01
#define	BIT_MASTER_POWER_CTRL							(1 << 0)
#define	BIT_PCLK_EN								(1 << 2)
#define	BIT_ISO_EN								(1 << 3)
#define	BIT_OSC_EN								(1 << 4)
#define	BIT_PDNRX12								(1 << 5)
#define	BIT_PDNTX12								(1 << 6)

#define	REG_DISC_CTRL1									TX_PAGE_3, 0x10
#define		VAL_DISC_CTRL1_DEFAULT						0x24
#define		BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE				0x01
#define		BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE_MASK			0x01
#define		BIT_DISC_CTRL1_STROBE_OFF					0x02

#define	REG_DISC_CTRL2									TX_PAGE_3, 0x11
#define		REG_DISC_CTRL2_DEFVAL						0xAD

#define	REG_DISC_CTRL3									TX_PAGE_3, 0x12
#define	BIT_DC6_USB_OVERRIDE_USBID_VALUE						0x08
#define	BIT_FORCE_USB                                    0x10
#define		BIT_DC3_DEFAULT								0x8A

#define	REG_DISC_CTRL4									TX_PAGE_3, 0x13
#define		BIT_DC6_USB_force						0x08
#define		REG_DISC_CTRL4_DEFVAL						0x84

#define	REG_DISC_CTRL5									TX_PAGE_3, 0x14
#define		BIT_DC6_USB_OVERRIDE_VALUE						0x08
#define		REG_DISC_CTRL5_DEFVAL						0x57

#define	REG_DISC_CTRL6									TX_PAGE_3, 0x15		// TODO: FD, TBD, not used
#define		BIT_DC6_USB_OVERRIDE_MASK					0x40
#define		BIT_DC6_USB_OVERRIDE_OFF					0x00
#define		BIT_DC6_USB_OVERRIDE_ON						0x40
#define		BIT_DC6_USB_SWforce_MASK					0x80
#define		BIT_DC6_USB_SWforce_ON						0x80
#define		BIT_DC6_USB_D_OVERRIDE_ON						0x80


#define	REG_DISC_CTRL7									TX_PAGE_3, 0x16		// TODO: FD, TBD, not used
#define	REG_DISC_CTRL8									TX_PAGE_3, 0x17

#define	REG_DISC_CTRL9									TX_PAGE_3, 0x18
#define		BIT_DC9_VBUS_OUTPUT_CAPABILITY_SRC				0x01
#define		BIT_DC9_WAKE_PULSE_BYPASS					0x02
#define		BIT_DC9_DISC_PULSE_PROCEED					0x04
#define		BIT_DC9_USB_EST							0x08
#define		BIT_DC9_WAKE_DRVFLT						0x10
#define		BIT_DC9_CBUS_LOW_TO_DISCONNECT					0x20
#define		BIT_DC9_VBUS_EN_OVERRIDE					0x40
#define		BIT_DC9_VBUS_EN_OVERRIDE_VAL					0x80

#define	REG_DISC_CTRL10									TX_PAGE_3, 0x19		// TODO: FD, TBD, not used
#define	REG_DISC_CTRL11									TX_PAGE_3, 0x1A		// TODO: FD, TBD, not used
#define	REG_DISC_STAT									TX_PAGE_3, 0x1B		// TODO: FD, TBD, not used
#define	REG_DISC_STAT2									TX_PAGE_3, 0x1C

#define	REG_INT_CTRL									TX_PAGE_3, 0x20
#define		BIT_INT_CTRL_POLARITY_LEVEL_HIGH				0x00
#define		BIT_INT_CTRL_POLARITY_LEVEL_LOW					0x02
#define		BIT_INT_CTRL_PUSH_PULL						0x00
#define		BIT_INT_CTRL_OPEN_DRAIN						0x04

#define	REG_INTR4									TX_PAGE_3, 0x21
#define		BIT_INTR4_VBUS_CHG						0x01				// TODO: FD, TBI, not actually used, not in PR, to be deleted?
#define		BIT_INTR4_MHL_EST						0x04
#define		BIT_INTR4_NON_MHL_EST						0x08
#define		BIT_INTR4_CBUS_LKOUT						0x10				// TODO: FD, TBD, not used, rvsd in PR
#define		BIT_INTR4_CBUS_DISCONNECT					0x20
#define		BIT_INTR4_RGND_DETECTION					0x40

#define	REG_INTR4_MASK									TX_PAGE_3, 0x22
#define		BIT_MHL_EST_INT_MASK						0x04
#define		BIT_NON_MHL_EST_INT_MASK					0x08
#define		BIT_CBUS_DISCON_INT_MASK					0x20
#define		BIT_CBUS_RID_DONE_INT_MASK					0x40
#define		BIT_SOFT_INT_MASK						0x80

#define	REG_MHLTX_CTL1									TX_PAGE_3, 0x30
#define		BIT_MHLTX_CTL1_VCO_FMAX_CTL_MASK				0x0F
#define		BIT_MHLTX_CTL1_DISC_OVRIDE_MASK					0x10
#define		BIT_MHLTX_CTL1_DISC_OVRIDE_OFF					0x00
#define		BIT_MHLTX_CTL1_DISC_OVRIDE_ON					0x10
#define		BIT_MHLTX_CTL1_TX_TERM_MODE_MASK				0xC0
#define		BIT_MHLTX_CTL1_TX_TERM_MODE_100DIFF				0x00
#define		BIT_MHLTX_CTL1_TX_TERM_MODE_150DIFF				0x40
#define		BIT_MHLTX_CTL1_TX_TERM_MODE_300DIFF				0x80
#define		BIT_MHLTX_CTL1_TX_TERM_MODE_OFF					0xC0

#define	REG_MHLTX_CTL2									TX_PAGE_3, 0x31
#define		REG_MHLTX_CTL2_DEFVAL						0x00
#define		BIT_MHL_TX_CTL2_TX_OE_MASK					0x3F

#define	REG_MHLTX_CTL3									TX_PAGE_3, 0x32
#define		REG_MHLTX_CTL3_DEFVAL						0x00
#define		BIT_MHLTX_CTL3_DAMPING_SEL_MASK					0x30
#define		BIT_MHLTX_CTL3_DAMPING_SEL_OFF					0x00
#define		BIT_MHLTX_CTL3_DAMPING_SEL_300_OHM				0x10
#define		BIT_MHLTX_CTL3_DAMPING_SEL_150_OHM				0x20
#define		BIT_MHLTX_CTL3_DAMPING_SEL_75_OHM				0x30

#define	REG_MHLTX_CTL4									TX_PAGE_3, 0x33
#define		REG_MHLTX_CTL4_SWING_DEFVAL					0x30
#define		BIT_DATA_SWING_CTL_MASK						0x07
#define		BIT_CLK_SWING_CTL_MASK						0x38
#define		BIT_MHLTX_CTL4_MHL_CLK_RATIO_MASK				0x40
#define		BIT_MHLTX_CTL4_MHL_CLK_RATIO_2X					0x00
#define		BIT_MHLTX_CTL4_MHL_CLK_RATIO_3X					0x40
#define		BIT_MHLTX_CTL4_AUDIO_CLK_EN					0x80	// TODO: FD, TBD, rvsd in register map, why system provided value contain this???
#define		REG_MHLTX_CTL4_DEFVAL						(BIT_MHLTX_CTL4_AUDIO_CLK_EN | BIT_MHLTX_CTL4_MHL_CLK_RATIO_3X | REG_MHLTX_CTL4_SWING_DEFVAL)

#define	REG_MHLTX_CTL5									TX_PAGE_3, 0x34		// TODO: FD, TBD, not used, why 0x35-Bit1:0 used but this one not???

#define	REG_MHLTX_CTL6									TX_PAGE_3, 0x35
#define		REG_MHLTX_CTL6_DEFVAL						0xBC
#define		BIT_MHLTX_CTL6_CLK_MASK						0xE0
#define		BIT_MHLTX_CTL6_CLK_PP						0x60
#define		BIT_MHLTX_CTL6_CLK_NPP						0xA0

#define	REG_MHLTX_CTL7									TX_PAGE_3, 0x36
#define		REG_MHLTX_CTL7_DEFVAL						0x0C

#define	REG_MHLTX_CTL8									TX_PAGE_3, 0x37
#define		REG_MHLTX_CTL8_DEFVAL						0x32
#define		BIT_MHLTX_CTL8_PLL_BW_CTL_MASK					0x07


/*
 * CBUS register definitions
 */
#define	REG_CBUS_DEVICE_CAP_0								TX_PAGE_CBUS, 0x00
#define	REG_CBUS_DEVICE_CAP_1								TX_PAGE_CBUS, 0x01
#define	REG_CBUS_DEVICE_CAP_2								TX_PAGE_CBUS, 0x02
#define	REG_CBUS_DEVICE_CAP_3								TX_PAGE_CBUS, 0x03
#define	REG_CBUS_DEVICE_CAP_4								TX_PAGE_CBUS, 0x04
#define	REG_CBUS_DEVICE_CAP_5								TX_PAGE_CBUS, 0x05
#define	REG_CBUS_DEVICE_CAP_6								TX_PAGE_CBUS, 0x06
#define	REG_CBUS_DEVICE_CAP_7								TX_PAGE_CBUS, 0x07
#define	REG_CBUS_DEVICE_CAP_8								TX_PAGE_CBUS, 0x08
#define	REG_CBUS_DEVICE_CAP_9								TX_PAGE_CBUS, 0x09
#define	REG_CBUS_DEVICE_CAP_A								TX_PAGE_CBUS, 0x0A
#define	REG_CBUS_DEVICE_CAP_B								TX_PAGE_CBUS, 0x0B
#define	REG_CBUS_DEVICE_CAP_C								TX_PAGE_CBUS, 0x0C
#define	REG_CBUS_DEVICE_CAP_D								TX_PAGE_CBUS, 0x0D
#define	REG_CBUS_DEVICE_CAP_E								TX_PAGE_CBUS, 0x0E
#define	REG_CBUS_DEVICE_CAP_F								TX_PAGE_CBUS, 0x0F

#define	REG_CBUS_SET_INT_0								TX_PAGE_CBUS, 0x20
#define	REG_CBUS_SET_INT_1								TX_PAGE_CBUS, 0x21
#define	REG_CBUS_SET_INT_2								TX_PAGE_CBUS, 0x22
#define	REG_CBUS_SET_INT_3								TX_PAGE_CBUS, 0x23

#define	REG_CBUS_WRITE_STAT_0								TX_PAGE_CBUS, 0x30
#define	REG_CBUS_WRITE_STAT_1								TX_PAGE_CBUS, 0x31
#define	REG_CBUS_WRITE_STAT_2								TX_PAGE_CBUS, 0x32
#define	REG_CBUS_WRITE_STAT_3								TX_PAGE_CBUS, 0x33

#define	REG_CBUS_MHL_SCRPAD_BASE						0x40
#define	REG_CBUS_MHL_SCRPAD_0								TX_PAGE_CBUS, 0x40
#define	REG_CBUS_WB_XMIT_DATA_0								TX_PAGE_CBUS, 0x60

#define	REG_CBUS_SET_INT_ENABLE_0							TX_PAGE_CBUS, 0x80
#define	REG_CBUS_SET_INT_ENABLE_1							TX_PAGE_CBUS, 0x81
#define	REG_CBUS_SET_INT_ENABLE_2							TX_PAGE_CBUS, 0x82
#define	REG_CBUS_SET_INT_ENABLE_3							TX_PAGE_CBUS, 0x83

#define	REG_CBUS_MDT_RCV_TIMEOUT							TX_PAGE_CBUS, 0x84
#define	REG_CBUS_MDT_XMIT_TIMEOUT							TX_PAGE_CBUS, 0x85

#define	REG_CBUS_MDT_RCV_CONTROL							TX_PAGE_CBUS, 0x86
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_CLR_CUR_MASK			0x01
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_CLR_CUR_NOP			0x00
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_CLR_CUR_CLEAR			0x01
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_CLR_ALL_MASK			0x02
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_CLR_ALL_NOP			0x00
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_CLR_ALL_CLEAR			0x02
#define		BIT_CBUS_MDT_RCV_CONTROL_MDT_DISABLE_MASK			0x04
#define		BIT_CBUS_MDT_RCV_CONTROL_MDT_DISABLE_ENABLED			0x00
#define		BIT_CBUS_MDT_RCV_CONTROL_MDT_DISABLE_RESET			0x04
#define		BIT_CBUS_MDT_RCV_CONTROL_XFIFO_OVRWR_EN_MASK			0x08
#define		BIT_CBUS_MDT_RCV_CONTROL_XFIFO_OVRWR_EN_STALL			0x00
#define		BIT_CBUS_MDT_RCV_CONTROL_XFIFO_OVRWR_EN_OVRWR			0x08
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_OVRWR_EN_MASK			0x10
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_OVRWR_EN_STALL			0x00
#define		BIT_CBUS_MDT_RCV_CONTROL_RFIFO_OVRWR_EN_OVRWR			0x10
#define		BIT_CBUS_MDT_RCV_CONTROL_RCV_EN_MASK				0x80
#define		BIT_CBUS_MDT_RCV_CONTROL_RCV_EN_DISABLE				0x00
#define		BIT_CBUS_MDT_RCV_CONTROL_RCV_EN_ENABLE				0x80

#define	REG_CBUS_MDT_RCV_READ_PORT							TX_PAGE_CBUS, 0x87
#define	REG_CBUS_MDT_XMIT_CONTROL							TX_PAGE_CBUS, 0x88
#define	REG_CBUS_MDT_XMIT_WRITE_PORT							TX_PAGE_CBUS, 0x89
#define	REG_CBUS_MDT_RFIFO_STAT								TX_PAGE_CBUS, 0x8A
#define	REG_CBUS_MDT_XFIFO_STAT								TX_PAGE_CBUS, 0x8B

#define	REG_CBUS_MDT_INT_0								TX_PAGE_CBUS, 0x8C
#define		BIT_MDT_RXFIFO_DATA_RDY						0x01	// TODO: FD, TBD, not actually used
#define		BIT_MDT_MSC_XFIFO_FULL						0x02	// TODO: FD, TBD, not used
#define		BIT_MDT_STATE_MACH_IDLE						0x04	// TODO: FD, TBD, not used
#define		BIT_MDT_XFIFO_EMPTY						0x08	// TODO: FD, TBD, not used

#define	REG_CBUS_MDT_INT_0_MASK								TX_PAGE_CBUS, 0x8D
#define	REG_CBUS_MDT_INT_1								TX_PAGE_CBUS, 0x8E
#define	REG_CBUS_MDT_INT_1_MASK								TX_PAGE_CBUS, 0x8F
#define	SPAD_XFIFO_STAT									TX_PAGE_CBUS, 0x8B	// TODO: FD, TBD, not used

#define	REG_CBUS_STATUS									TX_PAGE_CBUS, 0x91
#define		BIT_CBUS_CONNECTED						0x01
#define		BIT_MHL_MODE							0x02
#define		BIT_CBUS_HPD							0x04
#define		BIT_MSC_HB_SUCCESS						0x08
#define		BIT_MHL_CABLE_PRESENT						0x10

#define	REG_CBUS_INT_0									TX_PAGE_CBUS, 0x92
#define		BIT_CBUS_CNX_CHG						0x01
#define		BIT_CBUS_MSC_MT_DONE						0x02
#define		BIT_CBUS_HPD_RCVD						0x04
#define		BIT_CBUS_MSC_MR_WRITE_STAT					0x08
#define		BIT_CBUS_MSC_MR_MSC_MSG						0x10
#define		BIT_CBUS_MSC_MR_WRITE_BURST					0x20
#define		BIT_CBUS_MSC_MR_SET_INT						0x40
#define		BIT_CBUS_MSC_MT_DONE_NACK					0x80

#define	REG_CBUS_INT_0_MASK								TX_PAGE_CBUS, 0x93

#define	REG_CBUS_INT_1									TX_PAGE_CBUS, 0x94
#define		BIT_CBUS_DDC_ABRT						0x04
#define		BIT_CBUS_MSC_ABORT_RCVD						0x08
#define		BIT_CBUS_CMD_ABORT						0x40

#define	REG_CBUS_INT_1_MASK								TX_PAGE_CBUS, 0x95

#define	REG_CBUS_DDC_ABORT_INT								TX_PAGE_CBUS, 0x98
#define		BIT_CBUS_DDC_MAX_FAIL						0x01
#define		BIT_CBUS_DDC_PROTO_ERR						0x02
#define		BIT_CBUS_DDC_TIMEOUT						0x04
#define		BIT_CBUS_DDC_PEER_ABORT						0x80

#define	REG_CBUS_MSC_MT_ABORT_INT							TX_PAGE_CBUS, 0x9A
#define		BIT_CBUS_MSC_MT_ABORT_INT_MAX_FAIL				0x01
#define		BIT_CBUS_MSC_MT_ABORT_INT_PROTO_ERR				0x02
#define		BIT_CBUS_MSC_MT_ABORT_INT_TIMEOUT				0x04
#define		BIT_CBUS_MSC_MT_ABORT_INT_UNDEF_CMD				0x08
#define		BIT_CBUS_MSC_MT_ABORT_INT_MSC_MT_PEER_ABORT			0x80

#define	REG_MSC_RCV_ERROR								TX_PAGE_CBUS, 0x9C
#define		BIT_CBUS_MSC_MT_ABORT_INT_MAX_FAIL				0x01
#define		BIT_CBUS_MSC_MT_ABORT_INT_PROTO_ERR				0x02
#define		BIT_CBUS_MSC_MT_ABORT_INT_TIMEOUT				0x04
#define		BIT_CBUS_MSC_MT_ABORT_INT_UNDEF_CMD				0x08
#define		BIT_CBUS_MSC_MT_ABORT_INT_MSC_MT_PEER_ABORT			0x80

#define	REG_CBUS_LINK_CHECK_HIGH_LIMIT							TX_PAGE_CBUS, 0xA5
#define		REG_CBUS_LINK_CHECK_HIGH_LIMIT_DEFVAL				0x13

#define	REG_CBUS_LINK_XMIT_BIT_TIME							TX_PAGE_CBUS, 0xA7
#define		REG_CBUS_LINK_XMIT_BIT_TIME_DEFVAL				0x1D

#define	REG_CBUS_MSC_COMMAND_START							TX_PAGE_CBUS, 0xB8
#define		BIT_CBUS_MSC_PEER_CMD						0x01
#define		BIT_CBUS_MSC_MSG						0x02
#define		BIT_CBUS_MSC_READ_DEVCAP					0x04
#define		BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT				0x08
#define		BIT_CBUS_MSC_WRITE_BURST					0x10

#define	REG_CBUS_MSC_CMD_OR_OFFSET							TX_PAGE_CBUS, 0xB9
#define	REG_CBUS_MSC_1ST_TRANSMIT_DATA							TX_PAGE_CBUS, 0xBA
#define	REG_CBUS_MSC_2ND_TRANSMIT_DATA							TX_PAGE_CBUS, 0xBB
#define	REG_CBUS_PRI_RD_DATA_1ST							TX_PAGE_CBUS, 0xBC
#define	REG_CBUS_PRI_RD_DATA_2ND							TX_PAGE_CBUS, 0xBD
#define	REG_CBUS_MSC_MR_MSC_MSG_RCVD_1ST_DATA						TX_PAGE_CBUS, 0xBF
#define	REG_CBUS_MSC_MR_MSC_MSG_RCVD_2ND_DATA						TX_PAGE_CBUS, 0xC0
#define	REG_WBURST_RCVD_DATA_CNT							TX_PAGE_CBUS, 0xC3

#define	REG_CBUS_MSC_WRITE_BURST_DATA_LEN						TX_PAGE_CBUS, 0xC6
#define		MSC_WRITE_BURST_LEN_MASK					0x0F

#define	REG_CBUS_DDC_TIMEOUT								TX_PAGE_CBUS, 0xD1
#define	REG_CBUS_MISC_CONTROL								TX_PAGE_CBUS, 0xD8
