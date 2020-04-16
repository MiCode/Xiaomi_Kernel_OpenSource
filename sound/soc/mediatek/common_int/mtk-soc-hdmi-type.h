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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
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
 ******************************************************************************
 */

#ifndef _AUDIO_HDMI_TYPE_H
#define _AUDIO_HDMI_TYPE_H

#include <linux/list.h>

/*****************************************************************************
 *                ENUM DEFINITION
 ******************************************************************************/
enum hdmi_display_type {
	HDMI_DISPLAY_MHL = 0,
	HDMI_DISPLAY_SILMPORT = 1,
};

struct audio_hdmi_format {
	unsigned char mHDMI_DisplayType;
	unsigned int mI2Snum;
	unsigned int mI2S_MCKDIV;
	unsigned int mI2S_BCKDIV;

	unsigned int mHDMI_Samplerate;
	unsigned int mHDMI_Channels; /* channel number HDMI transmitted */
	unsigned int mHDMI_Data_Lens;
	unsigned int mTDM_Data_Lens;
	unsigned int mClock_Data_Lens;
	unsigned int mTDM_LRCK;
	unsigned int msDATA_Channels; /* channel number per sdata */
	unsigned int mMemIfFetchFormatPerSample;
	bool mSdata0;
	bool mSdata1;
	bool mSdata2;
	bool mSdata3;
};

#endif
