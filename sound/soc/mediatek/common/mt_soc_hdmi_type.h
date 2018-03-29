/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_digital_type.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/

#ifndef _AUDIO_HDMI_TYPE_H
#define _AUDIO_HDMI_TYPE_H

#include <linux/list.h>

/*****************************************************************************
 *                ENUM DEFINITION
 ******************************************************************************/
enum HDMI_DISPLAY_TYPE {
	HDMI_DISPLAY_MHL = 0,
	HDMI_DISPLAY_SILMPORT = 1,
};

typedef struct {
	uint8 mHDMI_DisplayType;
	uint32 mI2Snum;
	uint32 mI2S_MCKDIV;
	uint32 mI2S_BCKDIV;

	uint32 mHDMI_Samplerate;
	uint32 mHDMI_Channels; /* channel number HDMI transmitted */
	uint32 mHDMI_Data_Lens;
	uint32 mTDM_Data_Lens;
	uint32 mClock_Data_Lens;
	uint32 mTDM_LRCK;
	uint32 msDATA_Channels; /* channel number per sdata */
	uint32 mMemIfFetchFormatPerSample;
	bool mSdata0;
	bool mSdata1;
	bool mSdata2;
	bool mSdata3;
} AudioHDMIFormat;


#endif
