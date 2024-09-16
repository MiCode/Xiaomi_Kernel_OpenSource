/*******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include
 *      /CFG_Wifi_File.h#1
 */

/*! \file   CFG_Wifi_File.h
 *    \brief  Collection of NVRAM structure used for YuSu project
 *
 *    In this file we collect all compiler flags and detail the driver behavior
 *    if enable/disable such switch or adjust numeric parameters.
 */

#ifndef _CFG_WIFI_FILE_H
#define _CFG_WIFI_FILE_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_typedef.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
/* Connac define */
struct WIFI_NVRAM_2G4_TX_POWER_T {
	uint8_t uc2G4TxPwrCck1M;
	uint8_t uc2G4TxPwrCck2M;
	uint8_t uc2G4TxPwrCck5M;
	uint8_t uc2G4TxPwrCck11M;
	uint8_t uc2G4TxPwrOfdm6M;
	uint8_t uc2G4TxPwrOfdm9M;
	uint8_t uc2G4TxPwrOfdm12M;
	uint8_t uc2G4TxPwrOfdm18M;
	uint8_t uc2G4TxPwrOfdm24M;
	uint8_t uc2G4TxPwrOfdm36M;
	uint8_t uc2G4TxPwrOfdm48M;
	uint8_t uc2G4TxPwrOfdm54M;
	uint8_t uc2G4TxPwrHt20Mcs0;
	uint8_t uc2G4TxPwrHt20Mcs1;
	uint8_t uc2G4TxPwrHt20Mcs2;
	uint8_t uc2G4TxPwrHt20Mcs3;
	uint8_t uc2G4TxPwrHt20Mcs4;
	uint8_t uc2G4TxPwrHt20Mcs5;
	uint8_t uc2G4TxPwrHt20Mcs6;
	uint8_t uc2G4TxPwrHt20Mcs7;
	uint8_t uc2G4TxPwrHt40Mcs0;
	uint8_t uc2G4TxPwrHt40Mcs1;
	uint8_t uc2G4TxPwrHt40Mcs2;
	uint8_t uc2G4TxPwrHt40Mcs3;
	uint8_t uc2G4TxPwrHt40Mcs4;
	uint8_t uc2G4TxPwrHt40Mcs5;
	uint8_t uc2G4TxPwrHt40Mcs6;
	uint8_t uc2G4TxPwrHt40Mcs7;
	uint8_t uc2G4TxPwrHt40Mcs32;
	uint8_t uc2G4TxPwrVht20Mcs0;
	uint8_t uc2G4TxPwrVht20Mcs1;
	uint8_t uc2G4TxPwrVht20Mcs2;
	uint8_t uc2G4TxPwrVht20Mcs3;
	uint8_t uc2G4TxPwrVht20Mcs4;
	uint8_t uc2G4TxPwrVht20Mcs5;
	uint8_t uc2G4TxPwrVht20Mcs6;
	uint8_t uc2G4TxPwrVht20Mcs7;
	uint8_t uc2G4TxPwrVht20Mcs8;
	uint8_t uc2G4TxPwrVht20Mcs9;
	uint8_t uc2G4TxPwrVht40Mcs0;
	uint8_t uc2G4TxPwrVht40Mcs1;
	uint8_t uc2G4TxPwrVht40Mcs2;
	uint8_t uc2G4TxPwrVht40Mcs3;
	uint8_t uc2G4TxPwrVht40Mcs4;
	uint8_t uc2G4TxPwrVht40Mcs5;
	uint8_t uc2G4TxPwrVht40Mcs6;
	uint8_t uc2G4TxPwrVht40Mcs7;
	uint8_t uc2G4TxPwrVht40Mcs8;
	uint8_t uc2G4TxPwrVht40Mcs9;
	uint8_t uc2G4TxPwrRU26Mcs0;
	uint8_t uc2G4TxPwrRU26Mcs1;
	uint8_t uc2G4TxPwrRU26Mcs2;
	uint8_t uc2G4TxPwrRU26Mcs3;
	uint8_t uc2G4TxPwrRU26Mcs4;
	uint8_t uc2G4TxPwrRU26Mcs5;
	uint8_t uc2G4TxPwrRU26Mcs6;
	uint8_t uc2G4TxPwrRU26Mcs7;
	uint8_t uc2G4TxPwrRU26Mcs8;
	uint8_t uc2G4TxPwrRU26Mcs9;
	uint8_t uc2G4TxPwrRU26Mcs10;
	uint8_t uc2G4TxPwrRU26Mcs11;

	uint8_t uc2G4TxPwrRU52Mcs0;
	uint8_t uc2G4TxPwrRU52Mcs1;
	uint8_t uc2G4TxPwrRU52Mcs2;
	uint8_t uc2G4TxPwrRU52Mcs3;
	uint8_t uc2G4TxPwrRU52Mcs4;
	uint8_t uc2G4TxPwrRU52Mcs5;
	uint8_t uc2G4TxPwrRU52Mcs6;
	uint8_t uc2G4TxPwrRU52Mcs7;
	uint8_t uc2G4TxPwrRU52Mcs8;
	uint8_t uc2G4TxPwrRU52Mcs9;
	uint8_t uc2G4TxPwrRU52Mcs10;
	uint8_t uc2G4TxPwrRU52Mcs11;

	uint8_t uc2G4TxPwrRU106Mcs0;
	uint8_t uc2G4TxPwrRU106Mcs1;
	uint8_t uc2G4TxPwrRU106Mcs2;
	uint8_t uc2G4TxPwrRU106Mcs3;
	uint8_t uc2G4TxPwrRU106Mcs4;
	uint8_t uc2G4TxPwrRU106Mcs5;
	uint8_t uc2G4TxPwrRU106Mcs6;
	uint8_t uc2G4TxPwrRU106Mcs7;
	uint8_t uc2G4TxPwrRU106Mcs8;
	uint8_t uc2G4TxPwrRU106Mcs9;
	uint8_t uc2G4TxPwrRU106Mcs10;
	uint8_t uc2G4TxPwrRU106Mcs11;

	uint8_t uc2G4TxPwrRU242Mcs0;
	uint8_t uc2G4TxPwrRU242Mcs1;
	uint8_t uc2G4TxPwrRU242Mcs2;
	uint8_t uc2G4TxPwrRU242Mcs3;
	uint8_t uc2G4TxPwrRU242Mcs4;
	uint8_t uc2G4TxPwrRU242Mcs5;
	uint8_t uc2G4TxPwrRU242Mcs6;
	uint8_t uc2G4TxPwrRU242Mcs7;
	uint8_t uc2G4TxPwrRU242Mcs8;
	uint8_t uc2G4TxPwrRU242Mcs9;
	uint8_t uc2G4TxPwrRU242Mcs10;
	uint8_t uc2G4TxPwrRU242Mcs11;

	uint8_t uc2G4TxPwrRU484Mcs0;
	uint8_t uc2G4TxPwrRU484Mcs1;
	uint8_t uc2G4TxPwrRU484Mcs2;
	uint8_t uc2G4TxPwrRU484Mcs3;
	uint8_t uc2G4TxPwrRU484Mcs4;
	uint8_t uc2G4TxPwrRU484Mcs5;
	uint8_t uc2G4TxPwrRU484Mcs6;
	uint8_t uc2G4TxPwrRU484Mcs7;
	uint8_t uc2G4TxPwrRU484Mcs8;
	uint8_t uc2G4TxPwrRU484Mcs9;
	uint8_t uc2G4TxPwrRU484Mcs10;
	uint8_t uc2G4TxPwrRU484Mcs11;

	uint8_t uc2G4TxPwrLGBW40DuplucateMode;

};

struct WIFI_NVRAM_5G_TX_POWER_T {
	uint8_t uc5GTxPwrOfdm6M;
	uint8_t uc5GTxPwrOfdm9M;
	uint8_t uc5GTxPwrOfdm12M;
	uint8_t uc5GTxPwrOfdm18M;
	uint8_t uc5GTxPwrOfdm24M;
	uint8_t uc5GTxPwrOfdm36M;
	uint8_t uc5GTxPwrOfdm48M;
	uint8_t uc5GTxPwrOfdm54M;
	uint8_t uc5GTxPwrHt20Mcs0;
	uint8_t uc5GTxPwrHt20Mcs1;
	uint8_t uc5GTxPwrHt20Mcs2;
	uint8_t uc5GTxPwrHt20Mcs3;
	uint8_t uc5GTxPwrHt20Mcs4;
	uint8_t uc5GTxPwrHt20Mcs5;
	uint8_t uc5GTxPwrHt20Mcs6;
	uint8_t uc5GTxPwrHt20Mcs7;
	uint8_t uc5GTxPwrHt40Mcs0;
	uint8_t uc5GTxPwrHt40Mcs1;
	uint8_t uc5GTxPwrHt40Mcs2;
	uint8_t uc5GTxPwrHt40Mcs3;
	uint8_t uc5GTxPwrHt40Mcs4;
	uint8_t uc5GTxPwrHt40Mcs5;
	uint8_t uc5GTxPwrHt40Mcs6;
	uint8_t uc5GTxPwrHt40Mcs7;
	uint8_t uc5GTxPwrHt40Mcs32;

	uint8_t uc5GTxPwrVht20Mcs0;
	uint8_t uc5GTxPwrVht20Mcs1;
	uint8_t uc5GTxPwrVht20Mcs2;
	uint8_t uc5GTxPwrVht20Mcs3;
	uint8_t uc5GTxPwrVht20Mcs4;
	uint8_t uc5GTxPwrVht20Mcs5;
	uint8_t uc5GTxPwrVht20Mcs6;
	uint8_t uc5GTxPwrVht20Mcs7;
	uint8_t uc5GTxPwrVht20Mcs8;
	uint8_t uc5GTxPwrVht20Mcs9;
	uint8_t uc5GTxPwrVht40Mcs0;
	uint8_t uc5GTxPwrVht40Mcs1;
	uint8_t uc5GTxPwrVht40Mcs2;
	uint8_t uc5GTxPwrVht40Mcs3;
	uint8_t uc5GTxPwrVht40Mcs4;
	uint8_t uc5GTxPwrVht40Mcs5;
	uint8_t uc5GTxPwrVht40Mcs6;
	uint8_t uc5GTxPwrVht40Mcs7;
	uint8_t uc5GTxPwrVht40Mcs8;
	uint8_t uc5GTxPwrVht40Mcs9;
	uint8_t uc5GTxPwrVht80Mcs0;
	uint8_t uc5GTxPwrVht80Mcs1;
	uint8_t uc5GTxPwrVht80Mcs2;
	uint8_t uc5GTxPwrVht80Mcs3;
	uint8_t uc5GTxPwrVht80Mcs4;
	uint8_t uc5GTxPwrVht80Mcs5;
	uint8_t uc5GTxPwrVht80Mcs6;
	uint8_t uc5GTxPwrVht80Mcs7;
	uint8_t uc5GTxPwrVht80Mcs8;
	uint8_t uc5GTxPwrVht80Mcs9;
	uint8_t uc5GTxPwrVht160Mcs0;
	uint8_t uc5GTxPwrVht160Mcs1;
	uint8_t uc5GTxPwrVht160Mcs2;
	uint8_t uc5GTxPwrVht160Mcs3;
	uint8_t uc5GTxPwrVht160Mcs4;
	uint8_t uc5GTxPwrVht160Mcs5;
	uint8_t uc5GTxPwrVht160Mcs6;
	uint8_t uc5GTxPwrVht160Mcs7;
	uint8_t uc5GTxPwrVht160Mcs8;
	uint8_t uc5GTxPwrVht160Mcs9;

	uint8_t uc5GTxPwrRU26Mcs0;
	uint8_t uc5GTxPwrRU26Mcs1;
	uint8_t uc5GTxPwrRU26Mcs2;
	uint8_t uc5GTxPwrRU26Mcs3;
	uint8_t uc5GTxPwrRU26Mcs4;
	uint8_t uc5GTxPwrRU26Mcs5;
	uint8_t uc5GTxPwrRU26Mcs6;
	uint8_t uc5GTxPwrRU26Mcs7;
	uint8_t uc5GTxPwrRU26Mcs8;
	uint8_t uc5GTxPwrRU26Mcs9;
	uint8_t uc5GTxPwrRU26Mcs10;
	uint8_t uc5GTxPwrRU26Mcs11;

	uint8_t uc5GTxPwrRU52Mcs0;
	uint8_t uc5GTxPwrRU52Mcs1;
	uint8_t uc5GTxPwrRU52Mcs2;
	uint8_t uc5GTxPwrRU52Mcs3;
	uint8_t uc5GTxPwrRU52Mcs4;
	uint8_t uc5GTxPwrRU52Mcs5;
	uint8_t uc5GTxPwrRU52Mcs6;
	uint8_t uc5GTxPwrRU52Mcs7;
	uint8_t uc5GTxPwrRU52Mcs8;
	uint8_t uc5GTxPwrRU52Mcs9;
	uint8_t uc5GTxPwrRU52Mcs10;
	uint8_t uc5GTxPwrRU52Mcs11;

	uint8_t uc5GTxPwrRU106Mcs0;
	uint8_t uc5GTxPwrRU106Mcs1;
	uint8_t uc5GTxPwrRU106Mcs2;
	uint8_t uc5GTxPwrRU106Mcs3;
	uint8_t uc5GTxPwrRU106Mcs4;
	uint8_t uc5GTxPwrRU106Mcs5;
	uint8_t uc5GTxPwrRU106Mcs6;
	uint8_t uc5GTxPwrRU106Mcs7;
	uint8_t uc5GTxPwrRU106Mcs8;
	uint8_t uc5GTxPwrRU106Mcs9;
	uint8_t uc5GTxPwrRU106Mcs10;
	uint8_t uc5GTxPwrRU106Mcs11;

	uint8_t uc5GTxPwrRU242Mcs0;
	uint8_t uc5GTxPwrRU242Mcs1;
	uint8_t uc5GTxPwrRU242Mcs2;
	uint8_t uc5GTxPwrRU242Mcs3;
	uint8_t uc5GTxPwrRU242Mcs4;
	uint8_t uc5GTxPwrRU242Mcs5;
	uint8_t uc5GTxPwrRU242Mcs6;
	uint8_t uc5GTxPwrRU242Mcs7;
	uint8_t uc5GTxPwrRU242Mcs8;
	uint8_t uc5GTxPwrRU242Mcs9;
	uint8_t uc5GTxPwrRU242Mcs10;
	uint8_t uc5GTxPwrRU242Mcs11;

	uint8_t uc5GTxPwrRU484Mcs0;
	uint8_t uc5GTxPwrRU484Mcs1;
	uint8_t uc5GTxPwrRU484Mcs2;
	uint8_t uc5GTxPwrRU484Mcs3;
	uint8_t uc5GTxPwrRU484Mcs4;
	uint8_t uc5GTxPwrRU484Mcs5;
	uint8_t uc5GTxPwrRU484Mcs6;
	uint8_t uc5GTxPwrRU484Mcs7;
	uint8_t uc5GTxPwrRU484Mcs8;
	uint8_t uc5GTxPwrRU484Mcs9;
	uint8_t uc5GTxPwrRU484Mcs10;
	uint8_t uc5GTxPwrRU484Mcs11;

	uint8_t uc5GTxPwrRU996Mcs0;
	uint8_t uc5GTxPwrRU996Mcs1;
	uint8_t uc5GTxPwrRU996Mcs2;
	uint8_t uc5GTxPwrRU996Mcs3;
	uint8_t uc5GTxPwrRU996Mcs4;
	uint8_t uc5GTxPwrRU996Mcs5;
	uint8_t uc5GTxPwrRU996Mcs6;
	uint8_t uc5GTxPwrRU996Mcs7;
	uint8_t uc5GTxPwrRU996Mcs8;
	uint8_t uc5GTxPwrRU996Mcs9;
	uint8_t uc5GTxPwrRU996Mcs10;
	uint8_t uc5GTxPwrRU996Mcs11;

	uint8_t uc5GTxPwrLGBW40DuplucateMode;
	uint8_t uc5GTxPwrLGBW80DuplucateMode;
	uint8_t uc5GTxPwrLGBW1600DuplucateMode;
	uint8_t uc5GBw5MTxPwrDelta;
	uint8_t uc5GBw10MTxPwrDelta;
};

/* end Connac TX Power define */

/* duplicated from nic_cmd_event.h to avoid header dependency */
struct TX_PWR_PARAM {
	int8_t cTxPwr2G4Cck;	/* signed, in unit of 0.5dBm */
	int8_t cTxPwr2G4Dsss;	/* signed, in unit of 0.5dBm */
	int8_t acReserved[2];

	int8_t cTxPwr2G4OFDM_BPSK;
	int8_t cTxPwr2G4OFDM_QPSK;
	int8_t cTxPwr2G4OFDM_16QAM;
	int8_t cTxPwr2G4OFDM_Reserved;
	int8_t cTxPwr2G4OFDM_48Mbps;
	int8_t cTxPwr2G4OFDM_54Mbps;

	int8_t cTxPwr2G4HT20_BPSK;
	int8_t cTxPwr2G4HT20_QPSK;
	int8_t cTxPwr2G4HT20_16QAM;
	int8_t cTxPwr2G4HT20_MCS5;
	int8_t cTxPwr2G4HT20_MCS6;
	int8_t cTxPwr2G4HT20_MCS7;

	int8_t cTxPwr2G4HT40_BPSK;
	int8_t cTxPwr2G4HT40_QPSK;
	int8_t cTxPwr2G4HT40_16QAM;
	int8_t cTxPwr2G4HT40_MCS5;
	int8_t cTxPwr2G4HT40_MCS6;
	int8_t cTxPwr2G4HT40_MCS7;

	int8_t cTxPwr5GOFDM_BPSK;
	int8_t cTxPwr5GOFDM_QPSK;
	int8_t cTxPwr5GOFDM_16QAM;
	int8_t cTxPwr5GOFDM_Reserved;
	int8_t cTxPwr5GOFDM_48Mbps;
	int8_t cTxPwr5GOFDM_54Mbps;

	int8_t cTxPwr5GHT20_BPSK;
	int8_t cTxPwr5GHT20_QPSK;
	int8_t cTxPwr5GHT20_16QAM;
	int8_t cTxPwr5GHT20_MCS5;
	int8_t cTxPwr5GHT20_MCS6;
	int8_t cTxPwr5GHT20_MCS7;

	int8_t cTxPwr5GHT40_BPSK;
	int8_t cTxPwr5GHT40_QPSK;
	int8_t cTxPwr5GHT40_16QAM;
	int8_t cTxPwr5GHT40_MCS5;
	int8_t cTxPwr5GHT40_MCS6;
	int8_t cTxPwr5GHT40_MCS7;
};

struct TX_AC_PWR {
	int8_t c11AcTxPwr_BPSK;
	int8_t c11AcTxPwr_QPSK;
	int8_t c11AcTxPwr_16QAM;
	int8_t c11AcTxPwr_MCS5_MCS6;
	int8_t c11AcTxPwr_MCS7;
	int8_t c11AcTxPwr_MCS8;
	int8_t c11AcTxPwr_MCS9;
	int8_t c11AcTxPwrVht40_OFFSET;
	int8_t c11AcTxPwrVht80_OFFSET;
	int8_t c11AcTxPwrVht160_OFFSET;
	int8_t acReverse[2];
};

struct RSSI_PATH_COMPASATION {
	int8_t c2GRssiCompensation;
	int8_t c5GRssiCompensation;
};

struct PWR_5G_OFFSET {
	int8_t cOffsetBand0;	/* 4.915-4.980G */
	int8_t cOffsetBand1;	/* 5.000-5.080G */
	int8_t cOffsetBand2;	/* 5.160-5.180G */
	int8_t cOffsetBand3;	/* 5.200-5.280G */
	int8_t cOffsetBand4;	/* 5.300-5.340G */
	int8_t cOffsetBand5;	/* 5.500-5.580G */
	int8_t cOffsetBand6;	/* 5.600-5.680G */
	int8_t cOffsetBand7;	/* 5.700-5.825G */
};

struct PWR_PARAM {
	uint32_t au4Data[28];
	uint32_t u4RefValue1;
	uint32_t u4RefValue2;
};

struct AC_PWR_SETTING_STRUCT {
	uint8_t c11AcTxPwr_BPSK;
	uint8_t c11AcTxPwr_QPSK;
	uint8_t c11AcTxPwr_16QAM;
	uint8_t c11AcTxPwr_MCS5_MCS6;
	uint8_t c11AcTxPwr_MCS7;
	uint8_t c11AcTxPwr_MCS8;
	uint8_t c11AcTxPwr_MCS9;
	uint8_t c11AcTxPwr_Reserved;
	uint8_t c11AcTxPwrVht40_OFFSET;
	uint8_t c11AcTxPwrVht80_OFFSET;
	uint8_t c11AcTxPwrVht160_OFFSET;
};

struct BANDEDGE_5G {
	uint8_t uc5GBandEdgePwrUsed;
	uint8_t c5GBandEdgeMaxPwrOFDM20;
	uint8_t c5GBandEdgeMaxPwrOFDM40;
	uint8_t c5GBandEdgeMaxPwrOFDM80;

};

struct NEW_EFUSE_MAPPING2NVRAM {
	uint8_t ucReverse1[8];
	uint16_t u2Signature;
	struct BANDEDGE_5G r5GBandEdgePwr;
	uint8_t ucReverse2[14];

	/* 0x50 */
	uint8_t aucChOffset[3];
	uint8_t ucChannelOffsetVaild;
	uint8_t acAllChannelOffset;
	uint8_t aucChOffset3[11];

	/* 0x60 */
	uint8_t auc5GChOffset[8];
	uint8_t uc5GChannelOffsetVaild;
	uint8_t aucChOffset4[7];

	/* 0x70 */
	struct AC_PWR_SETTING_STRUCT r11AcTxPwr;
	uint8_t uc11AcTxPwrValid;

	uint8_t ucReverse4[20];

	/* 0x90 */
	struct AC_PWR_SETTING_STRUCT r11AcTxPwr2G;
	uint8_t uc11AcTxPwrValid2G;

	uint8_t ucReverse5[40];
};

struct WIFI_NVRAM_CONTROL_T {
	uint8_t ucControl; /*0: disable, 1: enable*/
	uint8_t ucTotalSizeLSB;
	uint8_t ucTotalSizeMSB;
};


struct WIFI_CFG_PARAM_STRUCT {
	/* NVRAM offset[0] ~ offset[255] */
	uint16_t u2Part1OwnVersion;
	uint16_t u2Part1PeerVersion;
	uint8_t aucMacAddress[6];
	uint8_t aucCountryCode[2];
	uint8_t aucOldTxPwr0[185];

	uint8_t ucSupport5GBand;
	uint8_t aucOldTxPwr1[4];

	uint8_t ucRegChannelListMap;
	uint8_t ucRegChannelListIndex;
	uint8_t aucRegSubbandInfo[36];
	uint8_t ucEnable5GBand;	/* move from 256+ offset to here */
	uint8_t ucNeedCheckLDO;
	uint8_t ucDefaultTestMode;
	uint8_t ucSupportCoAnt;
	uint8_t aucReserved0[12];
	/* NVRAM offset[256] ~ offset[255] */
	/* uint8_t aucReserved0[256 - 241]; */

	uint8_t ucTypeID0;
	uint8_t ucTypeLen0LSB;
	uint8_t ucTypeLen0MSB;
	struct WIFI_NVRAM_CONTROL_T rCtrl;
	uint8_t ucTypeID1;
	uint8_t ucTypeLen1LSB;
	uint8_t ucTypeLen1MSB;
	struct WIFI_NVRAM_2G4_TX_POWER_T r2G4Pwr;
	uint8_t ucTypeID2;
	uint8_t ucTypeLen2LSB;
	uint8_t ucTypeLen2MSB;
	struct WIFI_NVRAM_5G_TX_POWER_T r5GPwr;
	uint8_t aucReserved1[1528];
};

struct WIFI_NVRAM_TAG_FORMAT {
	uint8_t u1NvramTypeID;
	uint8_t u1NvramTypeLenLsb;
	uint8_t u1NvramTypeLenMsb;
};
/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#ifndef DATA_STRUCT_INSPECTING_ASSERT
#define DATA_STRUCT_INSPECTING_ASSERT(expr) \
		{switch (0) {case 0: case (expr): default:; } }
#endif

#define MAX_CFG_FILE_WIFI_REC_SIZE    (1024*8)

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#ifndef _lint
/* We don't have to call following function to inspect the data structure.
 * It will check automatically while at compile time.
 * We'll need this to guarantee the same member order in different structures
 * to simply handling effort in some functions.
 */
static __KAL_INLINE__ void nvramOffsetCheck(void)
{
	DATA_STRUCT_INSPECTING_ASSERT(
		OFFSET_OF(struct WIFI_CFG_PARAM_STRUCT, ucTypeID0) == 256);
	DATA_STRUCT_INSPECTING_ASSERT(
		sizeof(struct WIFI_CFG_PARAM_STRUCT) == 2048);
}
#endif

#endif /* _CFG_WIFI_FILE_H */
