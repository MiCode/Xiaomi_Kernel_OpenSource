/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __mt8193ddc_h__
#define __mt8193ddc_h__
#ifdef HDMI_MT8193_SUPPORT

#define EDID_BLOCK_LEN      128
#define EDID_SIZE 512

#define SIF1_CLOK 260		/* 27M/432 = 62.5Khz */

#define EDID_ID     0x50	/* 0xA0 */
#define EDID_ID1    0x51	/* 0xA2 */

#define EDID_ADDR_HEADER                      0x00
#define EDID_ADDR_VERSION                     0x12
#define EDID_ADDR_REVISION                    0x13
#define EDID_IMAGE_HORIZONTAL_SIZE            0x15
#define EDID_IMAGE_VERTICAL_SIZE              0x16
#define EDID_ADDR_FEATURE_SUPPORT             0x18
#define EDID_ADDR_TIMING_DSPR_1               0x36
#define EDID_ADDR_TIMING_DSPR_2               0x48
#define EDID_ADDR_MONITOR_DSPR_1              0x5A
#define EDID_ADDR_MONITOR_DSPR_2              0x6C
#define EDID_ADDR_EXT_BLOCK_FLAG              0x7E
#define EDID_ADDR_EXTEND_BYTE3                0x03	/* EDID address: 0x83 */
						   /* for ID receiver if RGB, YCbCr 4:2:2 or 4:4:4 */
/* Extension Block 1: */
#define EXTEDID_ADDR_TAG                      0x00
#define EXTEDID_ADDR_REVISION                 0x01
#define EXTEDID_ADDR_OFST_TIME_DSPR           0x02

/* ddcci master */
#define DDC_DDCMCTL0           ((unsigned int)0x0)
#define DDCM_ODRAIN          ((unsigned int)0x1<<31)
#define DDCM_CLK_DIV_OFFSET  ((unsigned int)16)
#define DDCM_CLK_DIV_MASK    ((unsigned int)0xFFF<<16)
#define DDCM_CS_STATUS       ((unsigned int)0x1<<4)
#define DDCM_SCL_STATE       ((unsigned int)0x1<<3)
#define DDCM_SDA_STATE       ((unsigned int)0x1<<2)
#define DDCM_SM0EN           ((unsigned int)0x1<<1)
#define DDCM_SCL_STRECH      ((unsigned int)0x1<<0)

#define DDC_DDCMCTL1           ((unsigned int)0x4)
#define DDCM_ACK_OFFSET      ((unsigned int)16)
#define DDCM_ACK_MASK        ((unsigned int)0xFF<<16)
#define DDCM_PGLEN_OFFSET    ((unsigned int)8)
#define DDCM_PGLEN_MASK      ((unsigned int)0x7<<8)
#define DDCM_SIF_MODE_OFFSET ((unsigned int)4)
#define DDCM_SIF_MODE_MASK   ((unsigned int)0x7<<4)
#define DDCM_START            ((unsigned int)0x1)
#define DDCM_WRITE_DATA       ((unsigned int)0x2)
#define DDCM_STOP             ((unsigned int)0x3)
#define DDCM_READ_DATA_NO_ACK ((unsigned int)0x4)
#define DDCM_READ_DATA_ACK    ((unsigned int)0x5)
#define DDCM_TRI             ((unsigned int)0x1<<0)

#define DDC_DDCMD0             ((unsigned int)0x8)
#define DDCM_DATA3           ((unsigned int)0xFF<<24)
#define DDCM_DATA2           ((unsigned int)0xFF<<16)
#define DDCM_DATA1           ((unsigned int)0xFF<<8)
#define DDCM_DATA0           ((unsigned int)0xFF<<0)

#define DDC_DDCMD1             ((unsigned int)0xC)
#define DDCM_DATA7           ((unsigned int)0xFF<<24)
#define DDCM_DATA6           ((unsigned int)0xFF<<16)
#define DDCM_DATA5           ((unsigned int)0xFF<<8)
#define DDCM_DATA4           ((unsigned int)0xFF<<0)

enum _SIF_BIT_T {
	SIF_8_BIT,		/* /< [8 bits data address.] */
	SIF_16_BIT,		/* /< [16 bits data address.] */
};
#define SIF_BIT_T enum _SIF_BIT_T

enum _SIF_TYPE_T {
	SIF_NORMAL,		/* /< [Normal, always select this.] */
	SIF_OTHER,		/* /< [Other.] */
};
#define SIF_TYPE_T enum _SIF_TYPE_T

  /* / [Sif control mode select.] */
struct _SIF_MODE_T {
	SIF_BIT_T eBit;		/* /< [The data address type. ] */
	SIF_TYPE_T eType;	/* /< [The control mode.] */
};
#define SIF_MODE_T struct _SIF_MODE_T

extern unsigned char fgDDCDataRead(unsigned char bDevice,
	unsigned char bData_Addr, unsigned char bDataCount, unsigned char *prData);
extern unsigned char fgDDCDataWrite(unsigned char bDevice,
	unsigned char bData_Addr, unsigned char bDataCount, unsigned char *prData);

#endif
#endif
