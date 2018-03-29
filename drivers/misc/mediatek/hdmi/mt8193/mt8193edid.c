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

#ifdef HDMI_MT8193_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/types.h>
#include <linux/types.h>

#include "mt8193_ctrl.h"

#include "mt8193ddc.h"
#include "mt8193edid.h"
#include "mt8193hdcp.h"

static HDMI_SINK_AV_CAP_T _HdmiSinkAvCap;
static unsigned char _fgHdmiNoEdidCheck = FALSE;
static unsigned int _u4i_3D_VIC;
static unsigned int _ui4First_16_NTSC_VIC;
static unsigned int _ui4First_16_PAL_VIC;
static unsigned int _ui4First_16_VIC[16];
static unsigned char _bEdidData[EDID_SIZE];	/* 4 block 512 Bytes */
static unsigned char aEDIDHeader[EDID_HEADER_LEN] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
static unsigned char aEDIDVSDBHeader[EDID_VSDB_LEN] = { 0x03, 0x0c, 0x00 };

static unsigned char _bEdidData2[256] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x4d, 0xd9, 0x02, 0xd7, 0x01, 0x01, 0x01, 0x01,
	0x20, 0x16, 0x01, 0x03, 0x80, 0xa0, 0x5a, 0x78,
	0x0a, 0x83, 0xad, 0xa2, 0x56, 0x49, 0x9b, 0x25,
	0x0f, 0x47, 0x4a, 0x20, 0x00, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1d,
	0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20, 0x6e, 0x28,
	0x55, 0x00, 0x40, 0x84, 0x63, 0x00, 0x00, 0x1e,
	0x01, 0x1d, 0x80, 0x18, 0x71, 0x1c, 0x16, 0x20,
	0x58, 0x2c, 0x25, 0x00, 0x40, 0x84, 0x63, 0x00,
	0x00, 0x9e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x53,
	0x4f, 0x4e, 0x59, 0x20, 0x54, 0x56, 0x0a, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
	0x00, 0x3a, 0x3e, 0x0f, 0x44, 0x0f, 0x00, 0x0a,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x6b,
	0x02, 0x03, 0x30, 0xf5, 0x45, 0x10, 0x04, 0x05,
	0x03, 0x20, 0x35, 0x0f, 0x7f, 0x07, 0x15, 0x07,
	0x55, 0x3d, 0x1f, 0xc0, 0x57, 0x07, 0x00, 0x67,
	0x54, 0x00, 0x5f, 0x7e, 0x01, 0x4d, 0x02, 0x00,
	0x83, 0x5f, 0x00, 0x00, 0x68, 0x03, 0x0c, 0x00,
	0x21, 0x00, 0x80, 0x1e, 0x0f, 0xe2, 0x00, 0x7b,
	0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40,
	0x58, 0x2c, 0x45, 0x00, 0x40, 0x84, 0x63, 0x00,
	0x00, 0x1e, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0,
	0x2d, 0x10, 0x10, 0x3e, 0x96, 0x00, 0x40, 0x84,
	0x63, 0x00, 0x00, 0x18, 0x8c, 0x0a, 0xd0, 0x8a,
	0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96, 0x00,
	0xb0, 0x84, 0x43, 0x00, 0x00, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c
};

static const unsigned char _cFsStr[][7] = { {"32khz  "},
{"44khz  "},
{"48khz  "},
{"88khz  "},
{"96khz  "},
{"176khz "},
{"192khz "}
};

static unsigned char cDstStr[50];

bool fgIsHdmiNoEDIDCheck(void)
{
	MT8193_EDID_FUNC();

	return _fgHdmiNoEdidCheck;
}

void vSetNoEdidChkInfo(void)
{
	unsigned char bInx;

	MT8193_EDID_FUNC();

	vSetSharedInfo(SI_EDID_PARSING_RESULT, TRUE);
	vSetSharedInfo(SI_EDID_VSDB_EXIST, TRUE);
	_HdmiSinkAvCap.b_sink_support_hdmi_mode = TRUE;
	_HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution = 0xffffffff;
	_HdmiSinkAvCap.ui4_sink_dtd_pal_resolution = 0xffffffff;
	_HdmiSinkAvCap.ui4_sink_1st_dtd_ntsc_resolution = 0xffffffff;
	_HdmiSinkAvCap.ui4_sink_1st_dtd_pal_resolution = 0xffffffff;
	_HdmiSinkAvCap.ui2_sink_colorimetry = 0xffff;
	_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution = 0xffffffff;
	_HdmiSinkAvCap.ui4_sink_cea_pal_resolution = 0xffffffff;
	_HdmiSinkAvCap.ui2_sink_aud_dec = 0xffff;
	_HdmiSinkAvCap.ui1_sink_dsd_ch_num = 5;
	for (bInx = 0; bInx < 7; bInx++) {
		_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[bInx] = 0xff;
		_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[bInx] = 0xff;
		_HdmiSinkAvCap.ui1_sink_dst_ch_sampling[bInx] = 0xff;
	}

	for (bInx = 0; bInx < 7; bInx++)
		_HdmiSinkAvCap.ui1_sink_pcm_bit_size[bInx] = 0xff;

	_HdmiSinkAvCap.ui1_sink_spk_allocation = 0xff;


	_HdmiSinkAvCap.e_sink_rgb_color_bit =
	    (HDMI_SINK_DEEP_COLOR_10_BIT | HDMI_SINK_DEEP_COLOR_12_BIT |
	     HDMI_SINK_DEEP_COLOR_16_BIT);
	_HdmiSinkAvCap.e_sink_ycbcr_color_bit =
	    (HDMI_SINK_DEEP_COLOR_10_BIT | HDMI_SINK_DEEP_COLOR_12_BIT |
	     HDMI_SINK_DEEP_COLOR_16_BIT);
	_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup = 0;
	_HdmiSinkAvCap.ui1_sink_support_ai = 1;

	_HdmiSinkAvCap.b_sink_edid_ready = TRUE;

	_HdmiSinkAvCap.b_sink_3D_present = TRUE;
	_HdmiSinkAvCap.ui4_sink_cea_3D_resolution = 0xFFFFFFFF;
	_HdmiSinkAvCap.ui1_sink_max_tmds_clock = 0xFFFF;
}

void vClearEdidInfo(void)
{
	unsigned char bInx;

	MT8193_EDID_FUNC();
	vSetSharedInfo(SI_EDID_PARSING_RESULT, FALSE);
	vSetSharedInfo(SI_EDID_VSDB_EXIST, FALSE);
	_HdmiSinkAvCap.b_sink_support_hdmi_mode = FALSE;
	_HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_dtd_pal_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_1st_dtd_ntsc_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_1st_dtd_pal_resolution = 0;
	_HdmiSinkAvCap.ui2_sink_colorimetry = 0;
	_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_cea_pal_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_native_pal_resolution = 0;
	_HdmiSinkAvCap.ui2_sink_vcdb_data = 0;
	_HdmiSinkAvCap.ui2_sink_aud_dec = 0;
	_HdmiSinkAvCap.ui1_sink_dsd_ch_num = 0;
	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[bInx] = 0x07;
		else
			_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[bInx] = 0;
		_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[bInx] = 0;
		_HdmiSinkAvCap.ui1_sink_dst_ch_sampling[bInx] = 0;
	}

	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			_HdmiSinkAvCap.ui1_sink_pcm_bit_size[bInx] = 0x07;
		else
			_HdmiSinkAvCap.ui1_sink_pcm_bit_size[bInx] = 0;
	}

	_HdmiSinkAvCap.ui1_sink_spk_allocation = 0;
	_HdmiSinkAvCap.ui1_sink_i_latency_present = 0;
	_HdmiSinkAvCap.ui1_sink_p_latency_present = 0;	/* kenny add 2010/4/25 */
	_HdmiSinkAvCap.ui1_sink_p_audio_latency = 0;
	_HdmiSinkAvCap.ui1_sink_p_video_latency = 0;
	_HdmiSinkAvCap.ui1_sink_i_audio_latency = 0;
	_HdmiSinkAvCap.ui1_sink_i_video_latency = 0;

	_HdmiSinkAvCap.e_sink_rgb_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	_HdmiSinkAvCap.e_sink_ycbcr_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup =
	    (SINK_BASIC_AUDIO_NO_SUP | SINK_SAD_NO_EXIST | SINK_BASE_BLK_CHKSUM_ERR |
	     SINK_EXT_BLK_CHKSUM_ERR);
	_HdmiSinkAvCap.ui2_sink_cec_address = 0xffff;

	_HdmiSinkAvCap.b_sink_edid_ready = FALSE;
	_HdmiSinkAvCap.ui1_sink_support_ai = 0;
	_HdmiSinkAvCap.ui1_Display_Horizontal_Size = 0;
	_HdmiSinkAvCap.ui1_Display_Vertical_Size = 0;

	_HdmiSinkAvCap.ui1_CNC = 0;

	if (fgIsHdmiNoEDIDCheck())
		vSetNoEdidChkInfo();


}

void vShowEdidRawData(void)
{
	unsigned short bTemp, i, j, k;

	MT8193_EDID_FUNC();

	for (bTemp = 0; bTemp < 2; bTemp++) {
		hdmi_print("===================================================================\n");
		hdmi_print("   EDID Block Number=#%d\n", bTemp);
		hdmi_print("   | 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f\n");
		hdmi_print("===================================================================\n");
		j = bTemp * EDID_BLOCK_LEN;
	for (i = 0; i < 8; i++) {
		if (((i * 16) + j) < 0x10)
			hdmi_print("0%x:  ", (i * 16) + j);
		else
			hdmi_print("%x:  ", (i * 16) + j);

		for (k = 0; k < 16; k++) {
			if (k == 15) {
				if ((j + (i * 16 + k)) < EDID_SIZE) {
					/* for Buffer overflow error */
					if (_bEdidData[j + (i * 16 + k)] > 0x0f)
						hdmi_print("%2x\n",
							   _bEdidData[j + (i * 16 + k)]);
					else
						hdmi_print("0%x\n",
							   _bEdidData[j + (i * 16 + k)]);
				}
			} else {
				if ((j + (i * 16 + k)) < EDID_SIZE) {
					/* for Buffer overflow error */
					if (_bEdidData[j + (i * 16 + k)] > 0x0f)
						hdmi_print("%2x  ",
							   _bEdidData[j + (i * 16 + k)]);
					else
						hdmi_print("0%x  ",
							   _bEdidData[j + (i * 16 + k)]);
				}
			}

		}

	}

	}
	hdmi_print("===================================================================\n");

}

unsigned char mt8193_fgreadedid(unsigned char i1noedid)
{
	unsigned char bIdx, bBlockIdx;
	unsigned char bExtBlockNo;
	unsigned short i;

	MT8193_EDID_FUNC();

	if (i1noedid == INTERNAL_EDID) {
		_fgHdmiNoEdidCheck = FALSE;
		vSetSharedInfo(SI_EDID_EXT_BLOCK_NO, 1);
		for (i = 0; i < 256; i++)
			_bEdidData[i] = _bEdidData2[i];
		return 1;
	} else if (i1noedid == NO_EDID) {
		vSetSharedInfo(SI_EDID_EXT_BLOCK_NO, 1);
		_fgHdmiNoEdidCheck = TRUE;
		for (i = 0; i < 256; i++)
			_bEdidData[i] = 0;
		; return 1;
	} else {
		_fgHdmiNoEdidCheck = FALSE;
	}

	/* block 0 : standard EDID block, address 0x00 ~ 0x7F w/ device ID=0xA0 */
	bExtBlockNo = 0xff;

	for (bBlockIdx = 0; bBlockIdx <= bExtBlockNo; bBlockIdx++) {
		for (bIdx = 0; bIdx < 5; bIdx++) {
			if ((bBlockIdx * EDID_BLOCK_LEN) < EDID_SIZE) {	/* for Buffer overflow error */
				if (fgDDCDataRead
				    (EDID_ID + (bBlockIdx >> 1),
				     0x00 + (bBlockIdx & 0x01) * EDID_BLOCK_LEN, EDID_BLOCK_LEN,
				     &_bEdidData[bBlockIdx * EDID_BLOCK_LEN]) == TRUE) {
					break;
				}
				if (bIdx == 4)
					return 0;


			}
		}

		bExtBlockNo = _bEdidData[EDID_ADDR_EXT_BLOCK_FLAG];
		vSetSharedInfo(SI_EDID_EXT_BLOCK_NO, bExtBlockNo);
	}

	return 1;
}

void vAnalyzeDTD(unsigned short ui2Active, unsigned short ui2HBlanking, unsigned char bFormat, unsigned char fgFirstDTD)
{
	unsigned int ui4NTSC = _HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution;
	unsigned int ui4PAL = _HdmiSinkAvCap.ui4_sink_dtd_pal_resolution;
	unsigned int ui41stNTSC = _HdmiSinkAvCap.ui4_sink_1st_dtd_ntsc_resolution;
	unsigned int ui41stPAL = _HdmiSinkAvCap.ui4_sink_1st_dtd_pal_resolution;

	MT8193_EDID_FUNC();

	switch (ui2Active) {
	case 0x5a0:		/* 480i */
		if (ui2HBlanking == 0x114) {	/* NTSC */
			if (bFormat == 0) {	/* p-scan */
				ui4NTSC |= SINK_480P_1440;
				if (fgFirstDTD)
					ui41stNTSC |= SINK_480P_1440;

			} else {
				ui4NTSC |= SINK_480I;
				if (fgFirstDTD)
					ui41stNTSC |= SINK_480I;

			}
		} else if (ui2HBlanking == 0x120) {	/* PAL */
			if (bFormat == 0) {	/* p-scan */
				ui4PAL |= SINK_576P_1440;
				if (fgFirstDTD)
					ui41stPAL |= SINK_576P_1440;

			} else {
				ui4PAL |= SINK_576I;
				if (fgFirstDTD)
					ui41stPAL |= SINK_576I;

			}
		}
		break;
	case 0x2d0:		/* 480p */
		if ((ui2HBlanking == 0x8a) && (bFormat == 0)) {	/* NTSC, p-scan */
			ui4NTSC |= SINK_480P;
			if (fgFirstDTD)
				ui41stNTSC |= SINK_480P;

		} else if ((ui2HBlanking == 0x90) && (bFormat == 0)) {	/* PAL, p-scan */
			ui4PAL |= SINK_576P;
			if (fgFirstDTD)
				ui41stPAL |= SINK_576P;

		}
		break;
	case 0x500:		/* 720p */
		if ((ui2HBlanking == 0x172) && (bFormat == 0)) {	/* NTSC, p-scan */
			ui4NTSC |= SINK_720P60;
			if (fgFirstDTD)
				ui41stNTSC |= SINK_720P60;

		} else if ((ui2HBlanking == 0x2bc) && (bFormat == 0)) {	/* PAL, p-scan */
			ui4PAL |= SINK_720P50;
			if (fgFirstDTD)
				ui41stPAL |= SINK_720P50;

		}
		break;
	case 0x780:		/* 1080i, 1080P */
		if ((ui2HBlanking == 0x118) && (bFormat == 1)) {	/* NTSC, interlace */
			ui4NTSC |= SINK_1080I60;
			if (fgFirstDTD)
				ui41stNTSC |= SINK_1080I60;

		} else if ((ui2HBlanking == 0x118) && (bFormat == 0)) {	/* NTSC, Progressive */
			ui4NTSC |= SINK_1080P60;
			if (fgFirstDTD)
				ui41stNTSC |= SINK_1080P60;

		} else if ((ui2HBlanking == 0x2d0) && (bFormat == 1)) {	/* PAL, interlace */
			ui4PAL |= SINK_1080I50;
			if (fgFirstDTD)
				ui41stPAL |= SINK_1080I50;

		} else if ((ui2HBlanking == 0x2d0) && (bFormat == 0)) {	/* PAL, Progressive */
			ui4PAL |= SINK_1080P50;
			if (fgFirstDTD)
				ui41stPAL |= SINK_1080P50;

		}
		break;
	}
	_HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution = ui4NTSC;
	_HdmiSinkAvCap.ui4_sink_dtd_pal_resolution = ui4PAL;
	_HdmiSinkAvCap.ui4_sink_1st_dtd_ntsc_resolution = ui41stNTSC;
	_HdmiSinkAvCap.ui4_sink_1st_dtd_pal_resolution = ui41stPAL;
}

unsigned char fgParserEDID(unsigned char *prbData)
{
	unsigned char bIdx;
	unsigned char bTemp = 0;
	unsigned short ui2HActive, ui2HBlanking;

	MT8193_EDID_FUNC();

	_HdmiSinkAvCap.ui1_Edid_Version = *(prbData + EDID_ADDR_VERSION);
	_HdmiSinkAvCap.ui1_Edid_Revision = *(prbData + EDID_ADDR_REVISION);
	_HdmiSinkAvCap.ui1_Display_Horizontal_Size = *(prbData + EDID_IMAGE_HORIZONTAL_SIZE);
	_HdmiSinkAvCap.ui1_Display_Vertical_Size = *(prbData + EDID_IMAGE_VERTICAL_SIZE);

	/* Step 1: check if EDID header pass */
	/* ie. EDID[0] ~ EDID[7] = specify header pattern */
	for (bIdx = EDID_ADDR_HEADER; bIdx < (EDID_ADDR_HEADER + EDID_HEADER_LEN); bIdx++) {
		if (*(prbData + bIdx) != aEDIDHeader[bIdx])
			return FALSE;

	}

	/* Step 2: Check if EDID checksume pass */
	/* ie. value of EDID[0] + ... + [0x7F] = 256*n */
	for (bIdx = 0; bIdx < EDID_BLOCK_LEN; bIdx++) {
		/* add the value into checksum */
		bTemp += *(prbData + bIdx);
	}

	/* check if EDID checksume pass */
	if (bTemp)
		return FALSE;

	_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup &= ~SINK_BASE_BLK_CHKSUM_ERR;


	/* [3.3] read-back H active line to define EDID resolution */
	for (bIdx = 0; bIdx < 2; bIdx++) {
		ui2HActive =
		    (unsigned short) (*(prbData + EDID_ADDR_TIMING_DSPR_1 + 18 * bIdx + OFST_H_ACT_BLA_HI) &
			   0xf0) << 4;
		ui2HActive |= *(prbData + EDID_ADDR_TIMING_DSPR_1 + 18 * bIdx + OFST_H_ACTIVE_LO);
		ui2HBlanking =
		    (unsigned short) (*(prbData + EDID_ADDR_TIMING_DSPR_1 + 18 * bIdx + OFST_H_ACT_BLA_HI) &
			   0x0f) << 8;
		ui2HBlanking |=
		    *(prbData + EDID_ADDR_TIMING_DSPR_1 + 18 * bIdx + OFST_H_BLANKING_LO);
		bTemp = (*(prbData + EDID_ADDR_TIMING_DSPR_1 + 18 * bIdx + OFST_FLAGS) & 0x80) >> 7;
		if (bIdx == 0)
			vAnalyzeDTD(ui2HActive, ui2HBlanking, bTemp, TRUE);
		else
			vAnalyzeDTD(ui2HActive, ui2HBlanking, bTemp, FALSE);

	}

	/* if go here, ie. parsing EDID data ok !! */
	return TRUE;
}

void vSetEdidChkError(void)
{
	unsigned char bInx;

	MT8193_EDID_FUNC();
	vSetSharedInfo(SI_EDID_PARSING_RESULT, TRUE);
	vSetSharedInfo(SI_EDID_VSDB_EXIST, FALSE);
	_HdmiSinkAvCap.b_sink_support_hdmi_mode = TRUE;
	_HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution = SINK_480P;	/* 0x1fffff; */
	_HdmiSinkAvCap.ui4_sink_dtd_pal_resolution = SINK_576P;	/* 0x1fffff; */
	_HdmiSinkAvCap.ui2_sink_colorimetry = 0;
	_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_cea_pal_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution = 0;
	_HdmiSinkAvCap.ui4_sink_native_pal_resolution = 0;
	_HdmiSinkAvCap.ui2_sink_vcdb_data = 0;
	_HdmiSinkAvCap.ui2_sink_aud_dec = 1;	/* PCM only */
	_HdmiSinkAvCap.ui1_sink_dsd_ch_num = 0;
	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[bInx] = 0x07;	/* 2ch max 48khz */
		else
			_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[bInx] = 0x0;
		_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[bInx] = 0;
		_HdmiSinkAvCap.ui1_sink_dst_ch_sampling[bInx] = 0;
	}

	for (bInx = 0; bInx < 7; bInx++) {
		if (bInx == 0)
			_HdmiSinkAvCap.ui1_sink_pcm_bit_size[bInx] = 0x07;	/* 2ch 24 bits */
		else
			_HdmiSinkAvCap.ui1_sink_pcm_bit_size[bInx] = 0;
	}

	_HdmiSinkAvCap.ui1_sink_spk_allocation = 0;
	_HdmiSinkAvCap.ui1_sink_i_latency_present = 0;
	_HdmiSinkAvCap.ui1_sink_p_latency_present = 0;	/* kenny add 2010/4/25 */
	_HdmiSinkAvCap.ui1_sink_p_audio_latency = 0;
	_HdmiSinkAvCap.ui1_sink_p_video_latency = 0;
	_HdmiSinkAvCap.ui1_sink_i_audio_latency = 0;
	_HdmiSinkAvCap.ui1_sink_i_video_latency = 0;

	_HdmiSinkAvCap.e_sink_rgb_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	_HdmiSinkAvCap.e_sink_ycbcr_color_bit = HDMI_SINK_NO_DEEP_COLOR;
	_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup =
	    (SINK_BASIC_AUDIO_NO_SUP | SINK_SAD_NO_EXIST | SINK_BASE_BLK_CHKSUM_ERR |
	     SINK_EXT_BLK_CHKSUM_ERR);
	_HdmiSinkAvCap.ui2_sink_cec_address = 0xffff;

	_HdmiSinkAvCap.b_sink_edid_ready = FALSE;
	_HdmiSinkAvCap.ui1_sink_support_ai = 0;
}

void vParserCEADataBlock(unsigned char *prData, unsigned char bLen)
{
	unsigned int ui4CEA_NTSC = 0, ui4CEA_PAL = 0, ui4OrgCEA_NTSC = 0, ui4OrgCEA_PAL =
	    0, ui4NativeCEA_NTSC = 0, ui4NativeCEA_PAL = 0;
	unsigned char bTemp, bIdx;
	unsigned char bLengthSum;
	unsigned char bType, bNo, bAudCode, bPcmChNum;
	unsigned char bTemp13, bTemp8, bLatency_offset = 0;
	unsigned char b3D_Multi_present = 0, b3D_Structure_7_0 = 1, b3D_MASK_15_8 = 1, b3D_MASK_7_0 =
	    1, b2D_VIC_order_Index = 0;
	unsigned char i, bTemp14 = 1, bDataTemp = 1;
	unsigned int ui4Temp = 0;
	unsigned int u23D_MASK_ALL;

	MT8193_EDID_FUNC();

	while (bLen) {

		if (bLen > 0x80)
			break;
		/* Step 1: get 1st data block type & total number of this data type */
		bTemp = *prData;
		bType = bTemp >> 5;	/* bit[7:5] */
		bNo = bTemp & 0x1F;	/* bit[4:0] */

		if (bType == 0x02) {	/* Video data block */
			ui4CEA_NTSC = 0;
			ui4CEA_PAL = 0;

			for (bIdx = 0; bIdx < bNo; bIdx++) {
				if (*(prData + 1 + bIdx) & 0x80) {	/* Native bit */
					ui4OrgCEA_NTSC = ui4CEA_NTSC;
					ui4OrgCEA_PAL = ui4CEA_PAL;
				}
				switch (*(prData + 1 + bIdx) & 0x7f) {
				case 6:
					ui4CEA_NTSC |= SINK_480I;
					ui4OrgCEA_NTSC |= SINK_480I;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_480I;
					break;

				case 7:
					ui4CEA_NTSC |= SINK_480I;
					ui4OrgCEA_NTSC |= SINK_480I;	/* 16:9 */
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_480I;

					break;
				case 2:
					ui4CEA_NTSC |= SINK_480P;
					ui4OrgCEA_NTSC |= SINK_480P;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_480P;

					break;
				case 3:
					ui4CEA_NTSC |= SINK_480P;
					ui4OrgCEA_NTSC |= SINK_480P;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_480P;

					break;
				case 14:
				case 15:
					ui4CEA_NTSC |= SINK_480P_1440;
					ui4OrgCEA_NTSC |= SINK_480P_1440;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_480P_1440;

					break;
				case 4:
					ui4CEA_NTSC |= SINK_720P60;
					ui4OrgCEA_NTSC |= SINK_720P60;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_720P60;

					break;
				case 5:
					ui4CEA_NTSC |= SINK_1080I60;
					ui4OrgCEA_NTSC |= SINK_1080I60;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_1080I60;
					break;
				case 21:
					ui4CEA_PAL |= SINK_576I;
					ui4OrgCEA_PAL |= SINK_576I;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_576I;
					break;

				case 22:
					ui4CEA_PAL |= SINK_576I;
					ui4OrgCEA_PAL |= SINK_576I;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_576I;

					break;
				case 16:
					ui4CEA_NTSC |= SINK_1080P60;
					ui4OrgCEA_NTSC |= SINK_1080P60;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_NTSC |= SINK_1080P60;
					break;

				case 17:
					ui4CEA_PAL |= SINK_576P;
					ui4OrgCEA_PAL |= SINK_576P;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_576P;
					break;

				case 18:
					ui4CEA_PAL |= SINK_576P;
					ui4OrgCEA_PAL |= SINK_576P;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_576P;

					break;

				case 29:
				case 30:
					ui4CEA_PAL |= SINK_576P_1440;
					ui4OrgCEA_PAL |= SINK_576P_1440;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_576P_1440;
					break;

				case 19:
					ui4CEA_PAL |= SINK_720P50;
					ui4OrgCEA_PAL |= SINK_720P50;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_720P50;

					break;
				case 20:
					ui4CEA_PAL |= SINK_1080I50;
					ui4OrgCEA_PAL |= SINK_1080I50;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_1080I50;

					break;

				case 31:
					ui4CEA_PAL |= SINK_1080P50;
					ui4OrgCEA_PAL |= SINK_1080P50;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_1080P50;
					break;

				case 32:
					ui4CEA_NTSC |= SINK_1080P24;
					ui4CEA_PAL |= SINK_1080P24;
					ui4CEA_NTSC |= SINK_1080P23976;
					ui4CEA_PAL |= SINK_1080P23976;
					ui4OrgCEA_PAL |= SINK_1080P24;
					ui4OrgCEA_NTSC |= SINK_1080P23976;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_1080P24;

					break;

				case 33:
					/* ui4CEA_NTSC |= SINK_1080P25; */
					ui4CEA_PAL |= SINK_1080P25;
					ui4OrgCEA_PAL |= SINK_1080P25;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_1080P25;

					break;

				case 34:
					ui4CEA_NTSC |= SINK_1080P30;
					ui4CEA_NTSC |= SINK_1080P2997;
					ui4CEA_PAL |= SINK_1080P30;
					ui4CEA_PAL |= SINK_1080P2997;
					ui4OrgCEA_PAL |= SINK_1080P30;
					ui4OrgCEA_NTSC |= SINK_1080P2997;
					if (*(prData + 1 + bIdx) & 0x80)	/* Native bit */
						ui4NativeCEA_PAL |= SINK_1080P30;
					break;

				default:
					break;
				}

				if (bIdx < 0x10) {

					switch (*(prData + 1 + bIdx) & 0x7f) {
					case 6:
					case 7:
						ui4Temp = SINK_480I;
						break;
					case 2:
					case 3:
						ui4Temp = SINK_480P;
						break;
					case 14:
					case 15:
						ui4Temp = SINK_480P_1440;
						break;
					case 4:
						ui4Temp = SINK_720P60;
						break;
					case 5:
						ui4Temp = SINK_1080I60;
						break;
					case 21:
					case 22:
						ui4Temp = SINK_576I;
						break;
					case 16:
						ui4Temp = SINK_1080P60;
						break;

					case 17:
					case 18:
						ui4Temp = SINK_576P;
						break;
					case 29:
					case 30:
						ui4Temp = SINK_576P_1440;
						break;
					case 19:
						ui4Temp = SINK_720P50;
						break;
					case 20:
						ui4Temp = SINK_1080I50;
						break;

					case 31:
						ui4Temp = SINK_1080P50;
						break;

					case 32:
						ui4Temp |= SINK_1080P24;
						ui4Temp |= SINK_1080P23976;
						break;

					case 33:
						/* ui4CEA_NTSC |= SINK_1080P25; */
						ui4Temp = SINK_1080P25;
						break;

					case 34:
						ui4Temp |= SINK_1080P30;
						ui4Temp |= SINK_1080P2997;

						break;

					default:
						break;


					}

					_ui4First_16_NTSC_VIC |= ui4CEA_NTSC;
					_ui4First_16_PAL_VIC |= ui4CEA_PAL;
					_ui4First_16_VIC[bIdx] = ui4Temp;
				}

				if (*(prData + 1 + bIdx) & 0x80) {
					ui4OrgCEA_NTSC = ui4CEA_NTSC & (~ui4OrgCEA_NTSC);
					ui4OrgCEA_PAL = ui4CEA_PAL & (~ui4OrgCEA_PAL);

					if (ui4OrgCEA_NTSC) {
						_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution =
						    ui4OrgCEA_NTSC;
					} else if (ui4OrgCEA_PAL) {
						_HdmiSinkAvCap.ui4_sink_native_pal_resolution =
						    ui4OrgCEA_PAL;
					} else {
						_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution = 0;
						_HdmiSinkAvCap.ui4_sink_native_pal_resolution = 0;
					}
				}
			}	/* for(bIdx = 0; bIdx < bNo; bIdx++) */

			_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution |= ui4CEA_NTSC;
			_HdmiSinkAvCap.ui4_sink_cea_pal_resolution |= ui4CEA_PAL;
			_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution |= ui4NativeCEA_NTSC;
			_HdmiSinkAvCap.ui4_sink_native_pal_resolution |= ui4NativeCEA_PAL;

		} else if (bType == 0x01) {	/* Audio data block */
			_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup &= ~(SINK_SAD_NO_EXIST);
			for (bIdx = 0; bIdx < (bNo / 3); bIdx++) {
				bLengthSum = bIdx * 3;
				bAudCode = (*(prData + bLengthSum + 1) & 0x78) >> 3;	/* get audio code */

				if ((bAudCode >= AVD_LPCM) && bAudCode <= AVD_WMA)
					_HdmiSinkAvCap.ui2_sink_aud_dec |= (1 << (bAudCode - 1));
				/* PCM:1 HDMI_SINK_AUDIO_DEC_LPCM AC3:2 HDMI_SINK_AUDIO_DEC_AC3 */

				if (bAudCode == AVD_LPCM) {	/* LPCM */
					bPcmChNum = (*(prData + bLengthSum + 1) & 0x07) + 1;
					if (bPcmChNum == 2) {
						_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[bPcmChNum -
											2] =
						    (*(prData + bLengthSum + 2) & 0x7f);
						_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[bPcmChNum -
											2] &= 0x07;
						_HdmiSinkAvCap.ui1_sink_pcm_bit_size[bPcmChNum -
										     2] =
						    (*(prData + bLengthSum + 3) & 0x07);
					}

				}

				if (bAudCode == AVD_DST) {	/* DST */
					bPcmChNum = (*(prData + bLengthSum + 1) & 0x07) + 1;
					if (bPcmChNum >= 2) {
						_HdmiSinkAvCap.ui1_sink_dst_ch_sampling[bPcmChNum -
											2] =
						    (*(prData + bLengthSum + 2) & 0x7f);

					}

				}

				if (bAudCode == AVD_DSD) {	/* DSD */
					bPcmChNum = (*(prData + bLengthSum + 1) & 0x07) + 1;
					if (bPcmChNum >= 2) {
						_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[bPcmChNum -
											2] =
						    (*(prData + bLengthSum + 2) & 0x7f);

					}

				}
			}	/* for(bIdx = 0; bIdx < bNo/3; bIdx++) */
		} else if (bType == 0x04) {	/* speaker allocation tag code, 0x04 */
			_HdmiSinkAvCap.ui1_sink_spk_allocation = *(prData + 1) & 0x7f;
	} else if (bType == 0x03) {	/* VDSB exit */
		for (bTemp = 0; bTemp < EDID_VSDB_LEN; bTemp++) {
			if (*(prData + bTemp + 1) != aEDIDVSDBHeader[bTemp])
				break;

		}
		/* for loop to end, is. VSDB header match */
	if (bTemp == EDID_VSDB_LEN) {
		vSetSharedInfo(SI_EDID_VSDB_EXIST, TRUE);
		_HdmiSinkAvCap.b_sink_support_hdmi_mode = TRUE;
		/* Read CEC physis address */
		if (bNo >= 5) {
			_HdmiSinkAvCap.ui2_sink_cec_address =
			    (*(prData + 4) << 8) | (*(prData + 5));

		} else {
			_HdmiSinkAvCap.ui2_sink_cec_address = 0xFFFF;
		}

		/* Read Support AI */
		if (bNo >= 6) {
			bTemp = *(prData + 6);
			if (bTemp & 0x80) {
				_HdmiSinkAvCap.ui1_sink_support_ai = 1;
				vSetSharedInfo(SI_HDMI_SUPPORTS_AI, 1);

			} else {
				_HdmiSinkAvCap.ui1_sink_support_ai = 0;
				vSetSharedInfo(SI_HDMI_SUPPORTS_AI, 0);
			}

			/* kenny add 2010/4/25 for repeater EDID check */
			_HdmiSinkAvCap.u1_sink_support_ai =
			    i4SharedInfo(SI_HDMI_SUPPORTS_AI);
			_HdmiSinkAvCap.e_sink_rgb_color_bit = ((bTemp >> 4) & 0x07);

			_HdmiSinkAvCap.u1_sink_max_tmds = *(prData + 7);

			if (bTemp & 0x08) {	/* support YCbCr Deep Color */
				_HdmiSinkAvCap.e_sink_ycbcr_color_bit =
				    ((bTemp >> 4) & 0x07);
			}

		} else {
			_HdmiSinkAvCap.ui1_sink_support_ai = 0;
			vSetSharedInfo(SI_HDMI_SUPPORTS_AI, 0);
		}

		/* max tmds clock */
		if (bNo >= 7) {
			bTemp = *(prData + 7);
			_HdmiSinkAvCap.ui1_sink_max_tmds_clock = ((unsigned short) bTemp) * 5;
			/* _HdmiSinkAvCap.ui1_sink_max_tmds_clock = 190; */
		} else {
			_HdmiSinkAvCap.ui1_sink_max_tmds_clock = 0;
		}

		/* Read Latency data */
		if (bNo >= 8) {
			bTemp = *(prData + 8);
			if (bTemp & 0x20)
				_HdmiSinkAvCap.b_sink_hdmi_video_present = 1;
			else
				_HdmiSinkAvCap.b_sink_hdmi_video_present = 0;
			_HdmiSinkAvCap.ui1_sink_content_cnc = bTemp & 0x0f;

			if (bTemp & 0x80) {
				/* Latency Present */
				_HdmiSinkAvCap.ui1_sink_p_latency_present = TRUE;
				_HdmiSinkAvCap.ui1_sink_p_video_latency =
				    *(prData + 9);
				_HdmiSinkAvCap.ui1_sink_p_audio_latency =
				    *(prData + 10);

				if (bTemp & 0x40) {
					/* Interlace Latency present */
					_HdmiSinkAvCap.ui1_sink_i_latency_present =
					    TRUE;
					_HdmiSinkAvCap.ui1_sink_i_video_latency =
					    *(prData + 11);
					_HdmiSinkAvCap.ui1_sink_i_audio_latency =
					    *(prData + 12);
				}

			}

			_HdmiSinkAvCap.ui1_CNC = bTemp & 0x0F;

		}



		if (bNo >= 8) {
			bTemp = *(prData + 8);

			if (!(bTemp & 0x80)) {
				/* Latency Present */
				bLatency_offset = bLatency_offset + 2;
			}
			if (!(bTemp & 0x40)) {
				/* Interlace Latency present */
				bLatency_offset = bLatency_offset + 2;
			}

		}
		if (bNo >= 13) {
			/* kenny add */
			bTemp = *(prData + 13);
			if (bTemp & 0x80)
				_HdmiSinkAvCap.b_sink_3D_present = 1;
			else
				_HdmiSinkAvCap.b_sink_3D_present = 0;

		}
		if (bNo >= 8)
			bTemp8 = *(prData + 8);

		if (bNo >= (13 - bLatency_offset)) {
			bTemp13 = *(prData + 13 - bLatency_offset);

			if (bTemp13 & 0x80) {
				_u4i_3D_VIC |= SINK_720P50;
				_u4i_3D_VIC |= SINK_720P60;
				_u4i_3D_VIC |= SINK_1080P23976;
				_u4i_3D_VIC |= SINK_1080P24;
				_HdmiSinkAvCap.b_sink_3D_present = TRUE;
			} else
				_HdmiSinkAvCap.b_sink_3D_present = FALSE;
		} else
			_HdmiSinkAvCap.b_sink_3D_present = FALSE;

		if (bNo >= (13 - bLatency_offset)) {
			bTemp13 = *(prData + 13 - bLatency_offset);

			if ((bTemp13 & 0x60) == 0x20)
				b3D_Multi_present = 0x20;
			else if ((bTemp13 & 0x60) == 0x40)
				b3D_Multi_present = 0x40;
			else
				b3D_Multi_present = 0x00;

		}

		if (bNo >= (14 - bLatency_offset))
			bTemp14 = *(prData + 14 - bLatency_offset);

		if (bNo > (14 - bLatency_offset + ((bTemp14 & 0xE0) >> 5))) {
			if (b3D_Multi_present == 0x20) {
				if (((15 - bLatency_offset +
				      ((bTemp14 & 0xE0) >> 5)) +
				     (bTemp14 & 0x1F)) >=
				    (15 - bLatency_offset +
				     ((bTemp14 & 0xE0) >> 5) + 2))
					b3D_Structure_7_0 =
					    *(prData + 15 - bLatency_offset +
					      ((bTemp14 & 0xE0) >> 5) + 1);

				if ((b3D_Structure_7_0 & 0x01) == 0x01) {
					for (i = 0; i < 0x10; i++)
						_u4i_3D_VIC |= _ui4First_16_VIC[i];
				}
				while (((15 - bLatency_offset +
					 ((bTemp14 & 0xE0) >> 5)) +
					(bTemp14 & 0x1F)) >
				       ((15 - bLatency_offset +
					 ((bTemp14 & 0xE0) >> 5)) + 2 +
					b2D_VIC_order_Index)) {
					/* 2 is 3D_structure */
					bDataTemp =
					    *(prData + 15 - bLatency_offset +
					      ((bTemp14 & 0xE0) >> 5) + 2 +
					      b2D_VIC_order_Index);
					if ((bDataTemp & 0x0F) < 0x08) {
						b2D_VIC_order_Index =
						    b2D_VIC_order_Index + 1;

					if ((bDataTemp & 0x0F) == 0x00) {
						/* 3D_Structure=0,  support frame packet */
						_u4i_3D_VIC |=
						    _ui4First_16_VIC[((bDataTemp & 0xF0) >> 4)];
					}

					} else {
						b2D_VIC_order_Index =
						    b2D_VIC_order_Index + 2;
					}
				}
			} else if (b3D_Multi_present == 0x40) {
				if (((15 - bLatency_offset +
				      ((bTemp14 & 0xE0) >> 5)) +
				     (bTemp14 & 0x1F)) >=
				    ((15 - bLatency_offset +
				      ((bTemp14 & 0xE0) >> 5)) + 4)) {
					/* 4 is 3D_structure+3D_MASK */
					/* b3D_Structure_15_8=*(prData+15+((bTemp&0xE0)>>5)); */
					b3D_Structure_7_0 =
					    *(prData + 15 - bLatency_offset +
					      ((bTemp14 & 0xE0) >> 5) + 1);
					b3D_MASK_15_8 =
					    *(prData + 15 - bLatency_offset +
					      ((bTemp14 & 0xE0) >> 5) + 2);
					b3D_MASK_7_0 =
					    *(prData + 15 - bLatency_offset +
					      ((bTemp14 & 0xE0) >> 5) + 3);

				if ((b3D_Structure_7_0 & 0x01) == 0x01) {	/* support frame packet */
					u23D_MASK_ALL =
					    (((unsigned short) (b3D_MASK_15_8)) << 8) |
					    ((unsigned short) (b3D_MASK_7_0));
				for (i = 0; i < 0x10; i++) {
					if (u23D_MASK_ALL & 0x0001) {
						_u4i_3D_VIC |=
						    _ui4First_16_VIC[i];
					}
					u23D_MASK_ALL =
					    u23D_MASK_ALL >> 1;
				}
				}

				}
				while (((15 - bLatency_offset +
					 ((bTemp14 & 0xE0) >> 5)) +
					(bTemp14 & 0x1F)) >
				       (15 - bLatency_offset +
					((bTemp14 & 0xE0) >> 5) + 4 +
					b2D_VIC_order_Index)) {
					bDataTemp =
					    *(prData + 15 - bLatency_offset +
					      ((bTemp14 & 0xE0) >> 5) + 4 +
					      b2D_VIC_order_Index);
					if ((bDataTemp & 0x0F) < 0x08) {
						b2D_VIC_order_Index =
						    b2D_VIC_order_Index + 1;

					if ((bDataTemp & 0x0F) == 0x00) {	/* 3D_Structure=0 */
						_u4i_3D_VIC |=
						    _ui4First_16_VIC[((bDataTemp & 0xF0) >> 4)];
					}

					} else {
						b2D_VIC_order_Index =
						    b2D_VIC_order_Index + 2;
					}
				}

			} else {
				b3D_Structure_7_0 = 0;

				while (((15 - bLatency_offset +
					 ((bTemp14 & 0xE0) >> 5)) +
					(bTemp14 & 0x1F)) >
				       ((15 - bLatency_offset +
					 ((bTemp14 & 0xE0) >> 5)) +
					b2D_VIC_order_Index)) {
					bDataTemp =
					    *(prData + 15 - bLatency_offset +
					      ((bTemp14 & 0xE0) >> 5) +
					      b2D_VIC_order_Index);
					if ((bDataTemp & 0x0F) < 0x08) {
						b2D_VIC_order_Index =
						    b2D_VIC_order_Index + 1;

					if ((bDataTemp & 0x0F) == 0x00) {	/* 3D_Structure=0 */
						_u4i_3D_VIC |=
						    _ui4First_16_VIC[((bDataTemp & 0xF0) >> 4)];
					}

					} else {
						b2D_VIC_order_Index =
						    b2D_VIC_order_Index + 2;
					}
				}
			}

		}
		_HdmiSinkAvCap.ui4_sink_cea_3D_resolution = _u4i_3D_VIC;


	} else {
			/* if(bTemp==EDID_VSDB_LEN) */
			vSetSharedInfo(SI_EDID_VSDB_EXIST, FALSE);
			_HdmiSinkAvCap.b_sink_support_hdmi_mode = FALSE;

		}

	} else if (bType == 0x07) {	/* Use Extended Tag */
			/* if(bType == 0x03) // VDSB exit */
			if (*(prData + 1) == 0x05) {	/* Extend Tag code ==0x05 */
				if (*(prData + 2) & 0x1) {
					/* Suppot xvYcc601 */
					_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_XV_YCC601;
				}

				if (*(prData + 2) & 0x2) {
					/* Suppot xvYcc709 */
					_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_XV_YCC709;
				}

				if (*(prData + 3) & 0x1) {
					/* support Gamut data P0 */
					_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_METADATA0;
				}

				if (*(prData + 3) & 0x2) {
					/* support Gamut data P1 */
					_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_METADATA1;
				}

				if (*(prData + 3) & 0x4) {
					/* support Gamut data P1 */
					_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_METADATA2;
				}
			} else if (*(prData + 1) == 0x0) {
				/* Extend Tag code ==0x0 */
				if (*(prData + 2) & 0x40) {
					/* support selectable, QS=1 */
					_HdmiSinkAvCap.ui2_sink_vcdb_data |= SINK_RGB_SELECTABLE;
				}
			}
		}
		/* re-assign the next data block address */
		prData += (bNo + 1);	/* '1' means the tag byte */

		bLen -= (bNo + 1);

	}			/* while(bLen) */


	_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[5] |= _HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[6];
	_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[4] |= _HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[5];
	_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[3] |= _HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[4];
	_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[2] |= _HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[3];
	_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[1] |= _HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[2];
	_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[0] |= _HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[1];

	_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[5] |= _HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[6];
	_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[4] |= _HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[5];
	_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[3] |= _HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[4];
	_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[2] |= _HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[3];
	_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[1] |= _HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[2];
	_HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[0] |= _HdmiSinkAvCap.ui1_sink_dsd_ch_sampling[1];


	if (_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup & SINK_EXT_BLK_CHKSUM_ERR) {
		/* 2007/2/12 for av output chksum error */
		vSetSharedInfo(SI_EDID_VSDB_EXIST, FALSE);
		_HdmiSinkAvCap.b_sink_support_hdmi_mode = FALSE;
	}

}

unsigned char fgParserExtEDID(unsigned char *prData)
{
	unsigned char bIdx;
	unsigned char bTemp = 0;
	unsigned short ui2HActive, ui2HBlanking, ui2VBlanking;
	unsigned char bOfst, *prCEAaddr;

	MT8193_EDID_FUNC();

	_HdmiSinkAvCap.ui1_ExtEdid_Revision = *(prData + EXTEDID_ADDR_REVISION);

	for (bIdx = 0; bIdx < EDID_BLOCK_LEN; bIdx++) {
		/* add the value into checksum */
		bTemp += *(prData + bIdx);	/* i4SharedInfo(wPos+bIdx); */
	}


	bTemp = 0;
	/* check if EDID checksume pass */
	if (bTemp) {
		; return FALSE;
	} else {
		_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup &= ~SINK_EXT_BLK_CHKSUM_ERR;
	}

	/* Step 1: get the offset value of 1st detail timing description within extension block */
	bOfst = *(prData + EXTEDID_ADDR_OFST_TIME_DSPR);

	if (*(prData + EDID_ADDR_EXTEND_BYTE3) & 0x40)	/* Support basic audio */
		_HdmiSinkAvCap.ui2_edid_chksum_and_audio_sup &= ~SINK_BASIC_AUDIO_NO_SUP;

	/* Max'0528'04, move to here, after read 0x80 ~ 0xFF because it is 0x83... */
	if (*(prData + EDID_ADDR_EXTEND_BYTE3) & 0x20) {	/* receiver support YCbCr 4:4:4 */
		_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_YCBCR_444;
	}

	if (*(prData + EDID_ADDR_EXTEND_BYTE3) & 0x10)	/* receiver support YCbCr 4:2:2 */
		_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_YCBCR_422;

	_HdmiSinkAvCap.ui2_sink_colorimetry |= SINK_RGB;
	/* Step 3: read-back the pixel clock of each timing descriptor */

	/* Step 4: read-back V active line to define EDID resolution */
	for (bIdx = 0; bIdx < 6; bIdx++) {
		if (((bOfst + 18 * bIdx) > 109) || (*(prData + bOfst + 18 * bIdx) == 0))
			break;

		ui2HActive = (unsigned short) (*(prData + bOfst + 18 * bIdx + OFST_H_ACT_BLA_HI) & 0xf0) << 4;
		ui2HActive |= *(prData + bOfst + 18 * bIdx + OFST_H_ACTIVE_LO);
		ui2HBlanking =
		    (unsigned short) (*(prData + bOfst + 18 * bIdx + OFST_H_ACT_BLA_HI) & 0x0f) << 8;
		ui2HBlanking |= *(prData + bOfst + 18 * bIdx + OFST_H_BLANKING_LO);
		ui2VBlanking = (unsigned short) (*(prData + bOfst + 18 * bIdx + OFST_V_ACTIVE_HI) & 0x0f) << 8;
		ui2VBlanking |= *(prData + bOfst + 18 * bIdx + OFST_V_BLANKING_LO);
		bTemp = (*(prData + bOfst + 18 * bIdx + OFST_FLAGS) & 0x80) >> 7;
		vAnalyzeDTD(ui2HActive, ui2HBlanking, bTemp, FALSE);
	}

	if (*(prData + EXTEDID_ADDR_REVISION) >= 0x03) {	/* for simplay #7-37, #7-36 */

		prCEAaddr = prData + 4;
		vParserCEADataBlock(prCEAaddr, bOfst - 4);
	}
	/* if go here, ie. parsing EDID data ok !! */
	return TRUE;
}

void vParserExtEDIDState(unsigned char *prEdid)
{
	unsigned char bTemp;
	unsigned char *prData;

	MT8193_EDID_FUNC();

	if (i4SharedInfo(SI_EDID_PARSING_RESULT) == TRUE) {
		/* parsing EDID extension block if it exist */
		for (bTemp = 0; bTemp < i4SharedInfo(SI_EDID_EXT_BLOCK_NO); bTemp++) {
			if ((EDID_BLOCK_LEN + bTemp * EDID_BLOCK_LEN) < EDID_SIZE) {	/* for Buffer Overflow error */
				if (*(prEdid + EDID_BLOCK_LEN + bTemp * EDID_BLOCK_LEN) == 0x02) {
					prData = (prEdid + EDID_BLOCK_LEN + bTemp * EDID_BLOCK_LEN);
					fgParserExtEDID(prData);
				} else if (*(prEdid + EDID_BLOCK_LEN + bTemp * EDID_BLOCK_LEN) ==
					   0xF0) {
						/****/
				}
			} else {
				/****/
			}
		}
	}
}

void mt8193_checkedid(unsigned char i1noedid)
{
	unsigned char bTemp;
	unsigned char bRetryCount = 2;
	unsigned char i;

	MT8193_EDID_FUNC();

	vClearEdidInfo();

	for (i = 0; i < 0x10; i++)
		_ui4First_16_VIC[i] = 0;

	_ui4First_16_NTSC_VIC = 0;
	_ui4First_16_PAL_VIC = 0;
	_u4i_3D_VIC = 0;
	_HdmiSinkAvCap.b_sink_hdmi_video_present = FALSE;
	_HdmiSinkAvCap.b_sink_3D_present = FALSE;
	_HdmiSinkAvCap.ui4_sink_cea_3D_resolution = 0;


	for (bTemp = 0; bTemp < bRetryCount; bTemp++) {
		if (mt8193_fgreadedid(i1noedid) == TRUE) {
			if (fgParserEDID(&_bEdidData[0]) == TRUE) {
				vSetSharedInfo(SI_EDID_PARSING_RESULT, TRUE);
				_HdmiSinkAvCap.b_sink_edid_ready = TRUE;
				HDMI_DEF_LOG("[hdmi][edid]parser ok\n");
				break;
			}

			if (bTemp == bRetryCount - 1)
				vSetSharedInfo(SI_EDID_PARSING_RESULT, TRUE);

			if (bTemp == bRetryCount - 1)
				break;
		} else {
			if (bTemp == bRetryCount - 1) {
				if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_ONLY)
					vClearEdidInfo();
				else if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) ==
					 HDMI_PLUG_IN_AND_SINK_POWER_ON)
					vSetEdidChkError();

				if (fgIsHdmiNoEDIDCheck())
					vSetNoEdidChkInfo();

				return;
			}
		}
		udelay(5);
	}

	if ((i4SharedInfo(SI_EDID_EXT_BLOCK_NO) * EDID_BLOCK_LEN) < EDID_SIZE)	/* for Buffer Overflow error */
		vParserExtEDIDState(&_bEdidData[0]);

	if (fgIsHdmiNoEDIDCheck())
		vSetNoEdidChkInfo();

#ifdef CONFIG_SINGLE_PANEL_OUTPUT
	_HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution &= (SINK_480P | SINK_720P60);
	_HdmiSinkAvCap.ui4_sink_dtd_pal_resolution &= (SINK_480P | SINK_720P60);
	_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution &= (SINK_480P | SINK_720P60);
	_HdmiSinkAvCap.ui4_sink_cea_pal_resolution &= (SINK_480P | SINK_720P60);
	_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution &= (SINK_480P | SINK_720P60);
	_HdmiSinkAvCap.ui4_sink_native_pal_resolution &= (SINK_480P | SINK_720P60);
#else
	_HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution &= (SINK_480P | SINK_720P60 | SINK_1080P30);
	_HdmiSinkAvCap.ui4_sink_dtd_pal_resolution &= (SINK_480P | SINK_720P60 | SINK_1080P30);
	_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution &= (SINK_480P | SINK_720P60 | SINK_1080P30);
	_HdmiSinkAvCap.ui4_sink_cea_pal_resolution &= (SINK_480P | SINK_720P60 | SINK_1080P30);
	_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution &= (SINK_480P | SINK_720P60 | SINK_1080P30);
	_HdmiSinkAvCap.ui4_sink_native_pal_resolution &= (SINK_480P | SINK_720P60 | SINK_1080P30);
#endif

	if (mt8193_log_on & hdmiedidlog) {
		vShowEdidRawData();
		vShowEdidInformation();
	} else
		hdmi_show_def_info();

}

unsigned char vCheckPcmBitSize(unsigned char ui1ChNumInx)
{
	unsigned char ui1Data, u1MaxBit;
	int i;

	u1MaxBit = PCM_16BIT;
	for (i = 6; i >= ui1ChNumInx; i--) {
		ui1Data = _HdmiSinkAvCap.ui1_sink_pcm_bit_size[i];

		if (ui1Data & (1 << PCM_24BIT)) {
			if (u1MaxBit < PCM_24BIT)
				u1MaxBit = PCM_24BIT;
		} else if (ui1Data & (1 << PCM_20BIT)) {
			if (u1MaxBit < PCM_20BIT)
				u1MaxBit = PCM_20BIT;
		}
	}

	return u1MaxBit;
}

void vShowEdidInformation(void)
{
	unsigned int u4Res = 0;
	unsigned char bInx = 0;

	MT8193_EDID_FUNC();

	hdmi_print("[HDMI]EDID ver:%d/rev:%d\n", _HdmiSinkAvCap.ui1_Edid_Version,
		   _HdmiSinkAvCap.ui1_Edid_Revision);
	hdmi_print("[HDMI]EDID Extend Rev:%d\n", _HdmiSinkAvCap.ui1_ExtEdid_Revision);
	if (_HdmiSinkAvCap.b_sink_support_hdmi_mode)
		hdmi_print("[HDMI]SINK Device is HDMI\n");
	else
		hdmi_print("[HDMI]SINK Device is DVI\n");

	if (_HdmiSinkAvCap.b_sink_support_hdmi_mode)
		hdmi_print("[HDMI]CEC ADDRESS:%x\n", _HdmiSinkAvCap.ui2_sink_cec_address);

	hdmi_print("[HDMI]max clock limit : %d\n", _HdmiSinkAvCap.ui1_sink_max_tmds_clock);

	u4Res = (_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution |
		 _HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution |
		 _HdmiSinkAvCap.ui4_sink_cea_pal_resolution |
		 _HdmiSinkAvCap.ui4_sink_dtd_pal_resolution);

	if (u4Res & SINK_480I)
		hdmi_print("[HDMI]SUPPORT 1440x480I 59.94hz\n");
	if (u4Res & SINK_480I_1440)
		hdmi_print("[HDMI]SUPPORT 2880x480I 59.94hz\n");
	if (u4Res & SINK_480P)
		hdmi_print("[HDMI]SUPPORT 720x480P 59.94hz\n");
	if (u4Res & SINK_480P_1440)
		hdmi_print("[HDMI]SUPPORT 1440x480P 59.94hz\n");
	if (u4Res & SINK_480P_2880)
		hdmi_print("[HDMI]SUPPORT 2880x480P 59.94hz\n");
	if (u4Res & SINK_720P60)
		hdmi_print("[HDMI]SUPPORT 1280x720P 59.94hz\n");
	if (u4Res & SINK_1080I60)
		hdmi_print("[HDMI]SUPPORT 1920x1080I 59.94hz\n");
	if (u4Res & SINK_1080P60)
		hdmi_print("[HDMI]SUPPORT 1920x1080P 59.94hz\n");

	if (u4Res & SINK_576I)
		hdmi_print("[HDMI]SUPPORT 1440x576I 50hz\n");
	if (u4Res & SINK_576I_1440)
		hdmi_print("[HDMI]SUPPORT 2880x576I 50hz\n");
	if (u4Res & SINK_576P)
		hdmi_print("[HDMI]SUPPORT 720x576P 50hz\n");
	if (u4Res & SINK_576P_1440)
		hdmi_print("[HDMI]SUPPORT 1440x576P 50hz\n");
	if (u4Res & SINK_576P_2880)
		hdmi_print("[HDMI]SUPPORT 2880x576P 50hz\n");
	if (u4Res & SINK_720P50)
		hdmi_print("[HDMI]SUPPORT 1280x720P 50hz\n");
	if (u4Res & SINK_1080I50)
		hdmi_print("[HDMI]SUPPORT 1920x1080I 50hz\n");
	if (u4Res & SINK_1080P50)
		hdmi_print("[HDMI]SUPPORT 1920x1080P 50hz\n");
	if (u4Res & SINK_1080P30)
		hdmi_print("[HDMI]SUPPORT 1920x1080P 30hz\n");
	if (u4Res & SINK_1080P24)
		hdmi_print("[HDMI]SUPPORT 1920x1080P 24hz\n");
	if (u4Res & SINK_1080P25)
		hdmi_print("[HDMI]SUPPORT 1920x1080P 25hz\n");

	u4Res =
	    (_HdmiSinkAvCap.ui4_sink_native_ntsc_resolution |
	     _HdmiSinkAvCap.ui4_sink_native_pal_resolution);
	hdmi_print("[HDMI]NTSC Native =%x\n", _HdmiSinkAvCap.ui4_sink_native_ntsc_resolution);
	hdmi_print("[HDMI]PAL Native =%x\n", _HdmiSinkAvCap.ui4_sink_native_pal_resolution);
	if (u4Res & SINK_480I)
		hdmi_print("[HDMI]Native resolution is 1440x480I 59.94hz\n");
	if (u4Res & SINK_480I_1440)
		hdmi_print("[HDMI]Native resolution is 2880x480I 59.94hz\n");
	if (u4Res & SINK_480P)
		hdmi_print("[HDMI]Native resolution is 720x480P 59.94hz\n");
	if (u4Res & SINK_480P_1440)
		hdmi_print("[HDMI]Native resolution is 1440x480P 59.94hz\n");
	if (u4Res & SINK_480P_2880)
		hdmi_print("[HDMI]Native resolution is 2880x480P 59.94hz\n");
	if (u4Res & SINK_720P60)
		hdmi_print("[HDMI]Native resolution is 1280x720P 59.94hz\n");
	if (u4Res & SINK_1080I60)
		hdmi_print("[HDMI]Native resolution is 1920x1080I 59.94hz\n");
	if (u4Res & SINK_1080P60)
		hdmi_print("[HDMI]Native resolution is 1920x1080P 59.94hz\n");
	if (u4Res & SINK_576I)
		hdmi_print("[HDMI]Native resolution is 1440x576I 50hz\n");
	if (u4Res & SINK_576I_1440)
		hdmi_print("[HDMI]Native resolution is 2880x576I 50hz\n");
	if (u4Res & SINK_576P)
		hdmi_print("[HDMI]Native resolution is 720x576P 50hz\n");
	if (u4Res & SINK_576P_1440)
		hdmi_print("[HDMI]Native resolution is 1440x576P 50hz\n");
	if (u4Res & SINK_576P_2880)
		hdmi_print("[HDMI]Native resolution is 2880x576P 50hz\n");
	if (u4Res & SINK_720P50)
		hdmi_print("[HDMI]Native resolution is 1280x720P 50hz\n");
	if (u4Res & SINK_1080I50)
		hdmi_print("[HDMI]Native resolution is 1920x1080I 50hz\n");
	if (u4Res & SINK_1080P50)
		hdmi_print("[HDMI]Native resolution is 1920x1080P 50hz\n");
	if (u4Res & SINK_1080P30)
		hdmi_print("[HDMI]Native resolution is 1920x1080P 30hz\n");
	if (u4Res & SINK_1080P24)
		hdmi_print("[HDMI]Native resolution is 1920x1080P 24hz\n");
	if (u4Res & SINK_1080P25)
		hdmi_print("[HDMI]Native resolution is 1920x1080P 25hz\n");


	hdmi_print("[HDMI]SUPPORT RGB\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_YCBCR_444)
		hdmi_print("[HDMI]SUPPORT YCBCR 444\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_YCBCR_422)
		hdmi_print("[HDMI]SUPPORT YCBCR 422\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_XV_YCC709)
		hdmi_print("[HDMI]SUPPORT xvYCC 709\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_XV_YCC601)
		hdmi_print("[HDMI]SUPPORT xvYCC 601\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_METADATA0)
		hdmi_print("[HDMI]SUPPORT metadata P0\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_METADATA1)
		hdmi_print("[HDMI]SUPPORT metadata P1\n");
	if (_HdmiSinkAvCap.ui2_sink_colorimetry & SINK_METADATA2)
		hdmi_print("[HDMI]SUPPORT metadata P2\n");

	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit & HDMI_SINK_DEEP_COLOR_10_BIT)
		hdmi_print("[HDMI]SUPPORT YCBCR 30 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit & HDMI_SINK_DEEP_COLOR_12_BIT)
		hdmi_print("[HDMI]SUPPORT YCBCR 36 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit & HDMI_SINK_DEEP_COLOR_16_BIT)
		hdmi_print("[HDMI]SUPPORT YCBCR 48 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_ycbcr_color_bit == HDMI_SINK_NO_DEEP_COLOR)
		hdmi_print("[HDMI]Not SUPPORT YCBCR Deep Color\n");

	if (_HdmiSinkAvCap.e_sink_rgb_color_bit & HDMI_SINK_DEEP_COLOR_10_BIT)
		hdmi_print("[HDMI]SUPPORT RGB 30 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_rgb_color_bit & HDMI_SINK_DEEP_COLOR_12_BIT)
		hdmi_print("[HDMI]SUPPORT RGB 36 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_rgb_color_bit & HDMI_SINK_DEEP_COLOR_16_BIT)
		hdmi_print("[HDMI]SUPPORT RGB 48 Bits Deep Color\n");
	if (_HdmiSinkAvCap.e_sink_rgb_color_bit == HDMI_SINK_NO_DEEP_COLOR)
		hdmi_print("[HDMI]Not SUPPORT RGB Deep Color\n");

	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_LPCM)
		hdmi_print("[HDMI]SUPPORT LPCM\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_AC3)
		hdmi_print("[HDMI]SUPPORT AC3 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MPEG1)
		hdmi_print("[HDMI]SUPPORT MPEG1 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MP3)
		hdmi_print("[HDMI]SUPPORT AC3 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MPEG2)
		hdmi_print("[HDMI]SUPPORT MPEG2 Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_AAC)
		hdmi_print("[HDMI]SUPPORT AAC Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DTS)
		hdmi_print("[HDMI]SUPPORT DTS Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_ATRAC)
		hdmi_print("[HDMI]SUPPORT ATRAC Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DSD)
		hdmi_print("[HDMI]SUPPORT SACD DSD Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DOLBY_PLUS)
		hdmi_print("[HDMI]SUPPORT Dolby Plus Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DTS_HD)
		hdmi_print("[HDMI]SUPPORT DTS HD Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_MAT_MLP)
		hdmi_print("[HDMI]SUPPORT MAT MLP Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_DST)
		hdmi_print("[HDMI]SUPPORT SACD DST Decode\n");
	if (_HdmiSinkAvCap.ui2_sink_aud_dec & HDMI_SINK_AUDIO_DEC_WMA)
		hdmi_print("[HDMI]SUPPORT  WMA Decode\n");

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[0] != 0) {
		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);

		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[0] >> bInx) & 0x01)
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
		}
		hdmi_print("[HDMI]SUPPORT PCM Max 2CH, Fs is: %s\n", &cDstStr[0]);
	}

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[4] != 0) {

		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);

		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[4] >> bInx) & 0x01)
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
		}
		hdmi_print("[HDMI]SUPPORT PCM Max 6CH Fs is: %s\n", &cDstStr[0]);

	}

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[5] != 0) {

		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);

		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[5] >> bInx) & 0x01)
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
		}
		hdmi_print("[HDMI]SUPPORT PCM Max 7CH Fs is: %s\n", &cDstStr[0]);

	}

	if (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[6] != 0) {

		for (bInx = 0; bInx < 50; bInx++)
			memcpy(&cDstStr[0 + bInx], " ", 1);

		for (bInx = 0; bInx < 7; bInx++) {
			if ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[6] >> bInx) & 0x01)
				memcpy(&cDstStr[0 + bInx * 7], &_cFsStr[bInx][0], 7);
		}
		hdmi_print("[HDMI]SUPPORT PCM Max 8CH, FS is: %s\n", &cDstStr[0]);

	}

	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_FL_FR)
		hdmi_print("[HDMI]Speaker FL/FR allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_LFE)
		hdmi_print("[HDMI]Speaker LFE allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_FC)
		hdmi_print("[HDMI]Speaker FC allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_RL_RR)
		hdmi_print("[HDMI]Speaker RL/RR allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_RC)
		hdmi_print("[HDMI]Speaker RC allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_FLC_FRC)
		hdmi_print("[HDMI]Speaker FLC/FRC allocated\n");
	if (_HdmiSinkAvCap.ui1_sink_spk_allocation & SINK_AUDIO_RLC_RRC)
		hdmi_print("[HDMI]Speaker RLC/RRC allocated\n");

	hdmi_print("[HDMI]HDMI edid support content type =%x\n",
		   _HdmiSinkAvCap.ui1_sink_content_cnc);
	hdmi_print("[HDMI]Lip Sync Progressive audio latency = %d\n",
		   _HdmiSinkAvCap.ui1_sink_p_audio_latency);
	hdmi_print("[HDMI]Lip Sync Progressive video latency = %d\n",
		   _HdmiSinkAvCap.ui1_sink_p_video_latency);
	if (_HdmiSinkAvCap.ui1_sink_i_latency_present) {
		hdmi_print("[HDMI]Lip Sync Interlace audio latency = %d\n",
			   _HdmiSinkAvCap.ui1_sink_i_audio_latency);
		hdmi_print("[HDMI]Lip Sync Interlace video latency = %d\n",
			   _HdmiSinkAvCap.ui1_sink_i_video_latency);
	}

	if (_HdmiSinkAvCap.ui1_sink_support_ai == 1)
		hdmi_print("[HDMI]Support AI\n");
	else
		hdmi_print("[HDMI]Not Support AI\n");

	hdmi_print("[HDMI]Monitor Max horizontal size = %d\n",
		   _HdmiSinkAvCap.ui1_Display_Horizontal_Size);
	hdmi_print("[HDMI]Monitor Max vertical size = %d\n",
		   _HdmiSinkAvCap.ui1_Display_Vertical_Size);


	if (_HdmiSinkAvCap.b_sink_hdmi_video_present == TRUE)
		hdmi_print("[HDMI]HDMI_Video_Present\n");
	else
		hdmi_print("[HDMI]No HDMI_Video_Present\n");

	if (_HdmiSinkAvCap.b_sink_3D_present == TRUE)
		hdmi_print("[HDMI]3D_present\n");
	else
		hdmi_print("[HDMI]No 3D_present\n");

}

void hdmi_show_def_info(void)
{
	unsigned int u4Res = 0;

	if (_HdmiSinkAvCap.b_sink_support_hdmi_mode)
		HDMI_DEF_LOG("[hdmi][edid]is hdmi\n");
	else
		HDMI_DEF_LOG("[hdmi][edid]is dvi\n");

	u4Res = (_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution |
		 _HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution |
		 _HdmiSinkAvCap.ui4_sink_cea_pal_resolution |
		 _HdmiSinkAvCap.ui4_sink_dtd_pal_resolution);

	HDMI_DEF_LOG("[hdmi][edid]res:%x,aud_dec:%x,pa:%x\n", u4Res,
		     _HdmiSinkAvCap.ui2_sink_aud_dec, _HdmiSinkAvCap.ui2_sink_cec_address);

}

void mt8193_AppGetEdidInfo(HDMI_EDID_T *pv_get_info)
{
	MT8193_EDID_FUNC();

	pv_get_info->ui4_ntsc_resolution =
	    (_HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution
	     | _HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution);
	pv_get_info->ui4_pal_resolution =
	    (_HdmiSinkAvCap.
	     ui4_sink_cea_pal_resolution | _HdmiSinkAvCap.ui4_sink_dtd_pal_resolution);
	pv_get_info->ui4_sink_native_ntsc_resolution =
	    _HdmiSinkAvCap.ui4_sink_native_ntsc_resolution;
	pv_get_info->ui4_sink_native_pal_resolution = _HdmiSinkAvCap.ui4_sink_native_pal_resolution;
	pv_get_info->ui4_sink_cea_ntsc_resolution = _HdmiSinkAvCap.ui4_sink_cea_ntsc_resolution;
	pv_get_info->ui4_sink_cea_pal_resolution = _HdmiSinkAvCap.ui4_sink_cea_pal_resolution;
	pv_get_info->ui4_sink_dtd_ntsc_resolution = _HdmiSinkAvCap.ui4_sink_dtd_ntsc_resolution;
	pv_get_info->ui4_sink_dtd_pal_resolution = _HdmiSinkAvCap.ui4_sink_dtd_pal_resolution;
	pv_get_info->ui4_sink_1st_dtd_ntsc_resolution =
	    _HdmiSinkAvCap.ui4_sink_1st_dtd_ntsc_resolution;
	pv_get_info->ui4_sink_1st_dtd_pal_resolution =
	    _HdmiSinkAvCap.ui4_sink_1st_dtd_pal_resolution;
	pv_get_info->ui2_sink_colorimetry = _HdmiSinkAvCap.ui2_sink_colorimetry;
	pv_get_info->ui1_sink_rgb_color_bit = _HdmiSinkAvCap.e_sink_rgb_color_bit;
	pv_get_info->ui1_sink_ycbcr_color_bit = _HdmiSinkAvCap.e_sink_ycbcr_color_bit;
	pv_get_info->ui2_sink_aud_dec = _HdmiSinkAvCap.ui2_sink_aud_dec;
	pv_get_info->ui1_sink_is_plug_in = _stAvdAVInfo.b_hotplug_state;
	pv_get_info->ui4_hdmi_pcm_ch_type = ((_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[0]) |
					     (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[4] << 8) |
					     (_HdmiSinkAvCap.ui1_sink_pcm_ch_sampling[6] << 16));
	pv_get_info->ui1_sink_i_latency_present = _HdmiSinkAvCap.ui1_sink_i_latency_present;
	pv_get_info->ui1_sink_p_audio_latency = _HdmiSinkAvCap.ui1_sink_p_audio_latency;
	pv_get_info->ui1_sink_p_video_latency = _HdmiSinkAvCap.ui1_sink_p_video_latency;
	pv_get_info->ui1_sink_i_audio_latency = _HdmiSinkAvCap.ui1_sink_i_audio_latency;
	pv_get_info->ui1_sink_i_video_latency = _HdmiSinkAvCap.ui1_sink_i_video_latency;

	pv_get_info->ui1ExtEdid_Revision = _HdmiSinkAvCap.ui1_ExtEdid_Revision;
	pv_get_info->ui1Edid_Version = _HdmiSinkAvCap.ui1_Edid_Version;
	pv_get_info->ui1Edid_Revision = _HdmiSinkAvCap.ui1_Edid_Revision;
	pv_get_info->ui1_Display_Horizontal_Size = _HdmiSinkAvCap.ui1_Display_Horizontal_Size;
	pv_get_info->ui1_Display_Vertical_Size = _HdmiSinkAvCap.ui1_Display_Vertical_Size;
	pv_get_info->ui2_sink_cec_address = _HdmiSinkAvCap.ui2_sink_cec_address;

}


unsigned char mt8193_Check_EdidHeader(void)
{
	unsigned char bIdx;
	unsigned char *prbData;

	prbData = &_bEdidData[0];

	/* check if EDID header pass, ie. EDID[0] ~ EDID[7] == specify header pattern */
	for (bIdx = EDID_ADDR_HEADER; bIdx < (EDID_ADDR_HEADER + EDID_HEADER_LEN); bIdx++) {
		if (*(prbData + bIdx) != aEDIDHeader[bIdx])
			return FALSE;
	}

	/* EDID Header Check OK Here */
	return TRUE;
}

#endif
