/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MHL_SPEC_DEFS_H__
#define __MHL_SPEC_DEFS_H__

enum DevCapOffset_e {
	DEVCAP_OFFSET_DEV_STATE         = 0x00,
	DEVCAP_OFFSET_MHL_VERSION	= 0x01,
	DEVCAP_OFFSET_DEV_CAT           = 0x02,
	DEVCAP_OFFSET_ADOPTER_ID_H      = 0x03,
	DEVCAP_OFFSET_ADOPTER_ID_L      = 0x04,
	DEVCAP_OFFSET_VID_LINK_MODE     = 0x05,
	DEVCAP_OFFSET_AUD_LINK_MODE     = 0x06,
	DEVCAP_OFFSET_VIDEO_TYPE        = 0x07,
	DEVCAP_OFFSET_LOG_DEV_MAP       = 0x08,
	DEVCAP_OFFSET_BANDWIDTH         = 0x09,
	DEVCAP_OFFSET_FEATURE_FLAG      = 0x0A,
	DEVCAP_OFFSET_DEVICE_ID_H       = 0x0B,
	DEVCAP_OFFSET_DEVICE_ID_L       = 0x0C,
	DEVCAP_OFFSET_SCRATCHPAD_SIZE   = 0x0D,
	DEVCAP_OFFSET_INT_STAT_SIZE     = 0x0E,
	DEVCAP_OFFSET_RESERVED          = 0x0F,
	/* this one must be last */
	DEVCAP_SIZE
};

#ifndef __MHL_MSM_8334_REGS_H__
#define __MHL_MSM_8334_REGS_H__

#define BIT0                    0x01
#define BIT1                    0x02
#define BIT2                    0x04
#define BIT3                    0x08
#define BIT4                    0x10
#define BIT5                    0x20
#define BIT6                    0x40
#define BIT7                    0x80

#define LOW                     0
#define HIGH                    1

#define MAX_PAGES               8
#endif


/* Version that this chip supports*/
/* bits 4..7 */
#define	MHL_VER_MAJOR           (0x01 << 4)
/* bits 0..3 */
#define	MHL_VER_MINOR		0x01
#define MHL_VERSION		(MHL_VER_MAJOR | MHL_VER_MINOR)

/*Device Category*/
#define	MHL_DEV_CATEGORY_OFFSET		DEVCAP_OFFSET_DEV_CAT
#define	MHL_DEV_CATEGORY_POW_BIT	(BIT4)

#define	MHL_DEV_CAT_SOURCE		0x02

/*Video Link Mode*/
#define	MHL_DEV_VID_LINK_SUPPRGB444		0x01
#define	MHL_DEV_VID_LINK_SUPPYCBCR444		0x02
#define	MHL_DEV_VID_LINK_SUPPYCBCR422		0x04
#define	MHL_DEV_VID_LINK_SUPP_PPIXEL		0x08
#define	MHL_DEV_VID_LINK_SUPP_ISLANDS		0x10

/*Audio Link Mode Support*/
#define	MHL_DEV_AUD_LINK_2CH				0x01
#define	MHL_DEV_AUD_LINK_8CH				0x02


/*Feature Flag in the devcap*/
#define	MHL_DEV_FEATURE_FLAG_OFFSET		DEVCAP_OFFSET_FEATURE_FLAG
/* Dongles have freedom to not support RCP */
#define	MHL_FEATURE_RCP_SUPPORT				BIT0
/* Dongles have freedom to not support RAP */
#define	MHL_FEATURE_RAP_SUPPORT				BIT1
/* Dongles have freedom to not support SCRATCHPAD */
#define	MHL_FEATURE_SP_SUPPORT				BIT2

/*Logical Dev Map*/
#define	MHL_DEV_LD_DISPLAY					(0x01 << 0)
#define	MHL_DEV_LD_VIDEO					(0x01 << 1)
#define	MHL_DEV_LD_AUDIO					(0x01 << 2)
#define	MHL_DEV_LD_MEDIA					(0x01 << 3)
#define	MHL_DEV_LD_TUNER					(0x01 << 4)
#define	MHL_DEV_LD_RECORD					(0x01 << 5)
#define	MHL_DEV_LD_SPEAKER					(0x01 << 6)
#define	MHL_DEV_LD_GUI						(0x01 << 7)

/*Bandwidth*/
/* 225 MHz */
#define	MHL_BANDWIDTH_LIMIT					22


#define MHL_STATUS_REG_CONNECTED_RDY        0x30
#define MHL_STATUS_REG_LINK_MODE            0x31

#define	MHL_STATUS_DCAP_RDY					BIT0

#define MHL_STATUS_CLK_MODE_MASK            0x07
#define MHL_STATUS_CLK_MODE_PACKED_PIXEL    0x02
#define MHL_STATUS_CLK_MODE_NORMAL          0x03
#define MHL_STATUS_PATH_EN_MASK             0x08
#define MHL_STATUS_PATH_ENABLED             0x08
#define MHL_STATUS_PATH_DISABLED            0x00
#define MHL_STATUS_MUTED_MASK               0x10

#define MHL_RCHANGE_INT                     0x20
#define MHL_DCHANGE_INT                     0x21

#define	MHL_INT_DCAP_CHG					BIT0
#define MHL_INT_DSCR_CHG                    BIT1
#define MHL_INT_REQ_WRT                     BIT2
#define MHL_INT_GRT_WRT                     BIT3

/* On INTR_1 the EDID_CHG is located at BIT 0*/
#define	MHL_INT_EDID_CHG					BIT1

/* This contains one nibble each - max offset */
#define		MHL_INT_AND_STATUS_SIZE			0x33
#define		MHL_SCRATCHPAD_SIZE			16
/* manually define highest number */
#define		MHL_MAX_BUFFER_SIZE			MHL_SCRATCHPAD_SIZE



enum {
	/* RCP sub-command  */
	MHL_MSC_MSG_RCP             = 0x10,
	/* RCP Acknowledge sub-command  */
	MHL_MSC_MSG_RCPK            = 0x11,
	/* RCP Error sub-command  */
	MHL_MSC_MSG_RCPE            = 0x12,
	/* Mode Change Warning sub-command  */
	MHL_MSC_MSG_RAP             = 0x20,
	/* MCW Acknowledge sub-command  */
	MHL_MSC_MSG_RAPK            = 0x21,
};

#define MHL_RCPE_NO_ERROR			0x00
#define MHL_RCPE_UNSUPPORTED_KEY_CODE		0x01
#define MHL_RCPE_BUSY				0x02

#define MHL_RAPK_NO_ERROR			0x00
#define MHL_RAPK_UNRECOGNIZED_ACTION_CODE	0x01
#define MHL_RAPK_UNSUPPORTED_ACTION_CODE	0x02
#define MHL_RAPK_BUSY				0x03

/* MHL spec related defines*/
enum {
	/* Command or Data byte acknowledge */
	MHL_ACK						= 0x33,
	/* Command or Data byte not acknowledge */
	MHL_NACK					= 0x34,
	/* Transaction abort */
	MHL_ABORT					= 0x35,
	/* 0xE0 - Write one status register strip top bit */
	MHL_WRITE_STAT				= 0x60 | 0x80,
	/* Write one interrupt register */
	MHL_SET_INT					= 0x60,
	/* Read one register */
	MHL_READ_DEVCAP				= 0x61,
	/* Read CBUS revision level from follower */
	MHL_GET_STATE				= 0x62,
	/* Read vendor ID value from follower. */
	MHL_GET_VENDOR_ID			= 0x63,
	/* Set Hot Plug Detect in follower */
	MHL_SET_HPD					= 0x64,
	/* Clear Hot Plug Detect in follower */
	MHL_CLR_HPD					= 0x65,
	/* Set Capture ID for downstream device. */
	MHL_SET_CAP_ID				= 0x66,
	/* Get Capture ID from downstream device. */
	MHL_GET_CAP_ID				= 0x67,
	/* VS command to send RCP sub-commands */
	MHL_MSC_MSG					= 0x68,
	/* Get Vendor-Specific command error code. */
	MHL_GET_SC1_ERRORCODE		= 0x69,
	/* Get DDC channel command error code. */
	MHL_GET_DDC_ERRORCODE		= 0x6A,
	/* Get MSC command error code. */
	MHL_GET_MSC_ERRORCODE		= 0x6B,
	/* Write 1-16 bytes to responder's scratchpad. */
	MHL_WRITE_BURST				= 0x6C,
	/* Get channel 3 command error code. */
	MHL_GET_SC3_ERRORCODE		= 0x6D,
};

/* Turn content streaming ON. */
#define	MHL_RAP_CONTENT_ON		0x10
/* Turn content streaming OFF. */
#define	MHL_RAP_CONTENT_OFF		0x11

/*
 *
 * MHL Timings applicable to this driver.
 *
 */
/* 100 - 1000 milliseconds. Per MHL 1.0 Specs */
#define	T_SRC_VBUS_CBUS_TO_STABLE	(200)
/* 20 milliseconds. Per MHL 1.0 Specs */
#define	T_SRC_WAKE_PULSE_WIDTH_1	(20)
/* 60 milliseconds. Per MHL 1.0 Specs */
#define	T_SRC_WAKE_PULSE_WIDTH_2	(60)

/* 100 - 1000 milliseconds. Per MHL 1.0 Specs */
#define	T_SRC_WAKE_TO_DISCOVER		(500)

#define T_SRC_VBUS_CBUS_T0_STABLE	(500)

/* Allow RSEN to stay low this much before reacting.*/
#define	T_SRC_RSEN_DEGLITCH			(100)

/* Wait this much after connection before reacting to RSEN (300-500ms)*/
/* Per specs between 300 to 500 ms*/
#define	T_SRC_RXSENSE_CHK			(400)

#endif /* __MHL_SPEC_DEFS_H__ */
