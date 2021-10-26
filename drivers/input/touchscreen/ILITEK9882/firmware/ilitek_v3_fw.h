/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __ILITEK_V3_FW_H
#define __ILITEK_V3_FW_H

/* define names and paths for the variety of tp modules */
#define DEF_INI_NAME_PATH		"/sdcard/mp.ini"
#define DEF_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define DEF_INI_REQUEST_PATH		"mp.ini"
#define DEF_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_DEF[] = {
	#include "9882N_MTK_90Hz_V0x00.0x00_AP_0x01_MP_0x01_SPI_SLAVE20200818.ili"
};

#define CSOT_INI_NAME_PATH		"/sdcard/mp_csot.ini"
#define CSOT_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define CSOT_INI_REQUEST_PATH		"mp_csot.ini"
#define CSOT_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_CSOT[] = {
	0xFF,
};

#define AUO_INI_NAME_PATH		"/sdcard/mp_auo.ini"
#define AUO_FW_FILP_PATH		"/sdcard/ILITEK_FW_AUO"
#define AUO_INI_REQUEST_PATH		"mp_auo.ini"
#define AUO_FW_REQUEST_PATH		"ILITEK_FW_AUO"
static unsigned char CTPM_FW_AUO[] = {
	0xFF,
};

#define BOE_INI_NAME_PATH		"/sdcard/mp_boe.ini"
#define BOE_FW_FILP_PATH		"/sdcard/ILITEK_FW_BOE"
#define BOE_INI_REQUEST_PATH		"mp_boe.ini"
#define BOE_FW_REQUEST_PATH		"ILITEK_FW_BOE"
static unsigned char CTPM_FW_BOE[] = {
	0xFF,
};

#define INX_INI_NAME_PATH		"/sdcard/mp_inx.ini"
#define INX_FW_FILP_PATH		"/sdcard/ILITEK_FW_INX"
#define INX_INI_REQUEST_PATH		"mp_inx.ini"
#define INX_FW_REQUEST_PATH		"ILITEK_FW_INX"
static unsigned char CTPM_FW_INX[] = {
	0xFF,
};

#define DJ_INI_NAME_PATH		"/sdcard/mp_dj.ini"
#define DJ_FW_FILP_PATH                 "/sdcard/ILITEK_FW_DJ"
#define DJ_INI_REQUEST_PATH		"mp_dj.ini"
#define DJ_FW_REQUEST_PATH		"ILITEK_FW_DJ"
static unsigned char CTPM_FW_DJ[] = {
	0xFF,
};

#define TXD_INI_NAME_PATH		"/sdcard/mp_txd.ini"
#define TXD_FW_FILP_PATH		"/sdcard/ILITEK_FW_TXD"
#define TXD_INI_REQUEST_PATH		"mp_txd.ini"
#define TXD_FW_REQUEST_PATH		"ILITEK_FW_TXD"
static unsigned char CTPM_FW_TXD[] = {
	0xFF,
};

#define TM_INI_NAME_PATH		"/sdcard/mp_tm.ini"
#define TM_FW_FILP_PATH                 "/sdcard/ILITEK_FW_TM"
#define TM_INI_REQUEST_PATH		"mp_tm.ini"
#define TM_FW_REQUEST_PATH		"ILITEK_FW_TM"
static unsigned char CTPM_FW_TM[] = {
	0xFF,
};

#endif