/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                        FTS API for Flashing the IC                     *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 *
 */

#ifndef __FTS_COMPENSATION_H
#define __FTS_COMPENSATION_H


#include "ftsCrossCompile.h"
#include "ftsSoftware.h"


#define COMP_DATA_READ_RETRY           2

//Bytes dimension of Compensation Data Format

#define COMP_DATA_HEADER               8
#define COMP_DATA_GLOBAL               8


#define HEADER_SIGNATURE               0xA5
#define INVALID_ERROR_OFFS             0xFFFF

//Possible Compensation/Frame Data Type
#define GENERAL_TUNING                 0x0100
#define MS_TOUCH_ACTIVE                0x0200
#define MS_TOUCH_LOW_POWER             0x0400
#define MS_TOUCH_ULTRA_LOW_POWER       0x0800
#define MS_KEY                         0x1000
#define SS_TOUCH                       0x2000
#define SS_KEY                         0x4000
#define SS_HOVER                       0x8000
#define SS_PROXIMITY                   0x0001
#define CHIP_INFO                      0xFFFF


#define TIMEOUT_REQU_COMP_DATA         1000 //ms

//CHIP INFO
#define CHIP_INFO_SIZE                 161
/*bytes to read from framebuffer (exclude the signature and the type*/
/*because already checked during the reading)*/
#define EXTERNAL_RELEASE_INFO_SIZE     8 //bytes

struct DataHeader {
	int force_node, sense_node;
	u16 type;
};


struct  MutualSenseData {
	struct DataHeader header;
	u8 tuning_ver;
	u8 cx1;
	u8 *node_data;
	int node_data_size;
};


struct SelfSenseData {
	struct DataHeader header;
	u8 tuning_ver;
	u8 f_ix1, s_ix1;
	u8 f_cx1, s_cx1;
	u8 f_max_n, s_max_n;

	u8 *ix2_fm;
	u8 *ix2_sn;
	u8 *cx2_fm;
	u8 *cx2_sn;
};


struct GeneralData {
	struct DataHeader header;
	u8 ftsd_lp_timer_cal0;
	u8 ftsd_lp_timer_cal1;
	u8 ftsd_lp_timer_cal2;

	u8 ftsd_lp_timer_cal3;
	u8 ftsa_lp_timer_cal0;
	u8 ftsa_lp_timer_cal1;
};

struct chipInfo {
	u8 u8_loadCnt;          ///< 03 - Load Counter
	u8 u8_infoVer;          ///< 04 - New chip info version
	u16 u16_ftsdId;         ///< 05 - FTSD ID
	u8 u8_ftsdVer;          ///< 07 - FTSD version
	u8 u8_ftsaId;           ///< 08 - FTSA ID
	u8 u8_ftsaVer;          ///< 09 - FTSA version
	u8 u8_tchRptVer;  ///< 0A - Touch report version (e.g. ST, Samsung etc)

	///< 0B - External release information
	u8 u8_extReleaseInfo[EXTERNAL_RELEASE_INFO_SIZE];
	u8 u8_custInfo[12];     ///< 13 - Customer information
	u16 u16_fwVer;          ///< 1F - Firmware version
	u16 u16_cfgId;          ///< 21 - Configuration ID
	u32 u32_projId;         ///< 23 - Project ID
	u16 u16_scrXRes;        ///< 27 - X resolution on main screen
	u16 u16_scrYRes;        ///< 29 - Y resolution on main screen
	u8 u8_scrForceLen;      ///< 2B - Number of force channel on main screen
	u8 u8_scrSenseLen;      ///< 2C - Number of sense channel on main screen
	u8 u64_scrForceEn[8];   ///< 2D - Force channel enabled on main screen
	u8 u64_scrSenseEn[8];   ///< 35 - Sense channel enabled on main screen
	u8 u8_msKeyLen;         ///< 3D - Number of MS Key channel
	u8 u64_msKeyForceEn[8]; ///< 3E - MS Key force channel enable
	u8 u64_msKeySenseEn[8]; ///< 46 - MS Key sense channel enable
	u8 u8_ssKeyLen;         ///< 4E - Number of SS Key channel
	u8 u64_ssKeyForceEn[8]; ///< 4F - SS Key force channel enable
	u8 u64_ssKeySenseEn[8]; ///< 57 - SS Key sense channel enable
	u8 u8_frcTchXLen;       ///< 5F - Number of force touch force channel
	u8 u8_frcTchYLen;       ///< 60 - Number of force touch sense channel
	u8 u64_frcTchForceEn[8];///< 61 - Force touch force channel enable
	u8 u64_frcTchSenseEn[8];///< 69 - Force touch sense channel enable
	u8 u8_msScrConfigTuneVer; ///< 71 - MS screen tuning version in config

	///< 72 - MS screen LP mode tuning version in config
	u8 u8_msScrLpConfigTuneVer;

	///< 73 - MS screen ultra low power mode tuning version in config
	u8 u8_msScrHwulpConfigTuneVer;
	u8 u8_msKeyConfigTuneVer; ///< 74 - MS Key tuning version in config
	u8 u8_ssTchConfigTuneVer; ///< 75 - SS touch tuning version in config
	u8 u8_ssKeyConfigTuneVer; ///< 76 - SS Key tuning version in config
	u8 u8_ssHvrConfigTuneVer; ///< 77 - SS hover tuning version in config

	///< 78 - Force touch tuning version in config
	u8 u8_frcTchConfigTuneVer;
	u8 u8_msScrCxmemTuneVer; ///< 79 - MS screen tuning version in cxmem

	///< 7A - MS screen LP mode tuning version in cxmem
	u8 u8_msScrLpCxmemTuneVer;

	///< 7B - MS screen ultra low power mode tuning version in cxmem
	u8 u8_msScrHwulpCxmemTuneVer;
	u8 u8_msKeyCxmemTuneVer; ///< 7C - MS Key tuning version in cxmem
	u8 u8_ssTchCxmemTuneVer; ///< 7D - SS touch tuning version in cxmem
	u8 u8_ssKeyCxmemTuneVer; ///< 7E - SS Key tuning version in cxmem
	u8 u8_ssHvrCxmemTuneVer; ///< 7F - SS hover tuning version in cxmem

	///< 80 - Force touch tuning version in cxmem
	u8 u8_frcTchCxmemTuneVer;
	u32 u32_mpPassFlag;      ///< 81 - Mass production pass flag
	u32 u32_featEn;          ///< 85 - Supported features

	///< 89 - enable of particular features: first bit is Echo Enables
	u32 u32_echoEn;

	///< 8D - Side Touch tuning version in config
	u8 u8_sideTchConfigTuneVer;
	u8 u8_sideTchCxmemTuneVer; ///< 8E - Side Touch tuning version in cxmem
	u8 u8_sideTchForceLen;   ///< 8F - Number of force channel on side touch
	u8 u8_sideTchSenseLen;   ///< 90 - Number of sense channel on side touch
	u8 u64_sideTchForceEn[8];///< 91 - Side touch force channel enable
	u8 u64_sideTchSenseEn[8];///< 99 - Side touch sense channel enable
	u8 u8_errSign;           ///< A1 - Signature for error field
	u16 u16_errOffset;       ///< A2 - Error Offset
};

int requestCompensationData(u16 type);
int readCompensationDataHeader(u16 type, struct DataHeader *header,
	u16 *address);
int readMutualSenseGlobalData(u16 *address, struct MutualSenseData *global);
int readMutualSenseNodeData(u16 address, struct MutualSenseData *node);
int readMutualSenseCompensationData(u16 type, struct MutualSenseData *data);
int readSelfSenseGlobalData(u16 *address, struct SelfSenseData *global);
int readSelfSenseNodeData(u16 address, struct SelfSenseData *node);
int readSelfSenseCompensationData(u16 type, struct SelfSenseData *data);
int readGeneralGlobalData(u16 address, struct GeneralData *global);
int readGeneralCompensationData(u16 type, struct GeneralData *data);
int defaultChipInfo(int i2cError);
int readChipInfo(int doRequest);

#endif
