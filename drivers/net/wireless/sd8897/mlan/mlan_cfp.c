/**
 * @file mlan_cfp.c
 *
 *  @brief This file contains WLAN client mode channel, frequency and power
 *  related code
 *
 *  Copyright (C) 2009-2012, Marvell International Ltd.
 *
 *  This software file (the "File") is distributed by Marvell International
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991
 *  (the "License").  You may use, redistribute and/or modify this File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available by writing to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 *  worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
 */

/*************************************************************
Change Log:
    04/16/2009: initial version
************************************************************/

#include "mlan.h"
#include "mlan_util.h"
#include "mlan_fw.h"
#include "mlan_join.h"
#include "mlan_main.h"

/********************************************************
    Local Variables
********************************************************/

/** 100mW */
#define WLAN_TX_PWR_DEFAULT     20
/** 100mW */
#define WLAN_TX_PWR_US_DEFAULT      20
/** 100mW */
#define WLAN_TX_PWR_JP_BG_DEFAULT   20
/** 200mW */
#define WLAN_TX_PWR_JP_A_DEFAULT    23
/** 100mW */
#define WLAN_TX_PWR_FR_100MW        20
/** 10mW */
#define WLAN_TX_PWR_FR_10MW         10
/** 100mW */
#define WLAN_TX_PWR_EMEA_DEFAULT    20
/** 2000mW */
#define WLAN_TX_PWR_CN_2000MW       33
/** 200mW */
#define WLAN_TX_PWR_200MW   23
/** 1000mW */
#define WLAN_TX_PWR_1000MW   30
/** 30mW */
#define WLAN_TX_PWR_SP_30MW   14
/** 60mW */
#define WLAN_TX_PWR_SP_60MW   17
/** 25mW */
#define WLAN_TX_PWR_25MW   14
/** 250mW */
#define WLAN_TX_PWR_250MW   24

/** Region code mapping */
typedef struct _country_code_mapping
{
    /** Region */
	t_u8 country_code[COUNTRY_CODE_LEN];
    /** Code for B/G CFP table */
	t_u8 cfp_code_bg;
    /** Code for A CFP table */
	t_u8 cfp_code_a;
} country_code_mapping_t;

/** Region code mapping table */
static country_code_mapping_t country_code_mapping[] = {
	{"US", 0x10, 0x10},	/* US FCC */
	{"CA", 0x10, 0x20},	/* IC Canada */
	{"SG", 0x10, 0x10},	/* Singapore */
	{"EU", 0x30, 0x30},	/* ETSI */
	{"AU", 0x30, 0x30},	/* Australia */
	{"KR", 0x30, 0x30},	/* Republic Of Korea */
	{"FR", 0x32, 0x32},	/* France */
	{"JP", 0xFF, 0x40},	/* Japan */
	{"CN", 0x30, 0x50},	/* China */
	{"DE", 0x30, 0x06},	/* Germany */
	{"ES", 0x30, 0x07},	/* Spain */
	{"AT", 0x30, 0x09},	/* Austria */
	{"BR", 0x01, 0x09},	/* Brazil */
	{"RU", 0x30, 0x0f},	/* Russia */
};

/**
 * The structure for Channel-Frequency-Power table
 */
typedef struct _cfp_table
{
    /** Region or Code */
	t_u8 code;
    /** Frequency/Power */
	chan_freq_power_t *cfp;
    /** No of CFP flag */
	int cfp_no;
} cfp_table_t;

/* Format { Channel, Frequency (MHz), MaxTxPower } */
/** Band: 'B/G', Region: USA FCC/Canada IC */
static chan_freq_power_t channel_freq_power_US_BG[] = {
	{1, 2412, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{2, 2417, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{3, 2422, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{4, 2427, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{5, 2432, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{6, 2437, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{7, 2442, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{8, 2447, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{9, 2452, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{10, 2457, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{11, 2462, WLAN_TX_PWR_US_DEFAULT, MFALSE}
};

/** Band: 'B/G', Region: Europe ETSI/China */
static chan_freq_power_t channel_freq_power_EU_BG[] = {
	{1, 2412, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{2, 2417, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{3, 2422, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{4, 2427, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{5, 2432, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{6, 2437, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{7, 2442, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{8, 2447, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{9, 2452, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{10, 2457, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{11, 2462, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{12, 2467, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{13, 2472, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE}
};

/** Band: 'B/G', Region: France */
static chan_freq_power_t channel_freq_power_FR_BG[] = {
	{1, 2412, WLAN_TX_PWR_FR_100MW, MFALSE},
	{2, 2417, WLAN_TX_PWR_FR_100MW, MFALSE},
	{3, 2422, WLAN_TX_PWR_FR_100MW, MFALSE},
	{4, 2427, WLAN_TX_PWR_FR_100MW, MFALSE},
	{5, 2432, WLAN_TX_PWR_FR_100MW, MFALSE},
	{6, 2437, WLAN_TX_PWR_FR_100MW, MFALSE},
	{7, 2442, WLAN_TX_PWR_FR_100MW, MFALSE},
	{8, 2447, WLAN_TX_PWR_FR_100MW, MFALSE},
	{9, 2452, WLAN_TX_PWR_FR_100MW, MFALSE},
	{10, 2457, WLAN_TX_PWR_FR_10MW, MFALSE},
	{11, 2462, WLAN_TX_PWR_FR_10MW, MFALSE},
	{12, 2467, WLAN_TX_PWR_FR_10MW, MFALSE},
	{13, 2472, WLAN_TX_PWR_FR_10MW, MFALSE}
};

/** Band: 'B/G', Region: Japan */
static chan_freq_power_t channel_freq_power_JPN41_BG[] = {
	{1, 2412, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{2, 2417, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{3, 2422, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{4, 2427, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{5, 2432, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{6, 2437, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{7, 2442, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{8, 2447, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{9, 2452, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{10, 2457, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{11, 2462, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{12, 2467, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{13, 2472, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE}
};

/** Band: 'B/G', Region: Japan */
static chan_freq_power_t channel_freq_power_JPN40_BG[] = {
	{14, 2484, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE}
};

/** Band: 'B/G', Region: Japan */
static chan_freq_power_t channel_freq_power_JPNFE_BG[] = {
	{1, 2412, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{2, 2417, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{3, 2422, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{4, 2427, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{5, 2432, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{6, 2437, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{7, 2442, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{8, 2447, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{9, 2452, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{10, 2457, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{11, 2462, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{12, 2467, WLAN_TX_PWR_JP_BG_DEFAULT, MTRUE},
	{13, 2472, WLAN_TX_PWR_JP_BG_DEFAULT, MTRUE}
};

/** Band : 'B/G', Region: Brazil */
static chan_freq_power_t channel_freq_power_BR_BG[] = {
	{1, 2412, WLAN_TX_PWR_1000MW, MFALSE},
	{2, 2417, WLAN_TX_PWR_1000MW, MFALSE},
	{3, 2422, WLAN_TX_PWR_1000MW, MFALSE},
	{4, 2427, WLAN_TX_PWR_1000MW, MFALSE},
	{5, 2432, WLAN_TX_PWR_1000MW, MFALSE},
	{6, 2437, WLAN_TX_PWR_1000MW, MFALSE},
	{7, 2442, WLAN_TX_PWR_1000MW, MFALSE},
	{8, 2447, WLAN_TX_PWR_1000MW, MFALSE},
	{9, 2452, WLAN_TX_PWR_1000MW, MFALSE},
	{10, 2457, WLAN_TX_PWR_1000MW, MFALSE},
	{11, 2462, WLAN_TX_PWR_1000MW, MFALSE},
	{12, 2467, WLAN_TX_PWR_1000MW, MFALSE},
	{13, 2472, WLAN_TX_PWR_1000MW, MFALSE},
};

/** Band : 'B/G', Region: Special */
static chan_freq_power_t channel_freq_power_SPECIAL_BG[] = {
	{1, 2412, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{2, 2417, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{3, 2422, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{4, 2427, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{5, 2432, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{6, 2437, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{7, 2442, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{8, 2447, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{9, 2452, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{10, 2457, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{11, 2462, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{12, 2467, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{13, 2472, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE},
	{14, 2484, WLAN_TX_PWR_JP_BG_DEFAULT, MFALSE}
};

/**
 * The 2.4GHz CFP tables
 */
static cfp_table_t cfp_table_BG[] = {
	{
	 0x01,			/* Brazil */
	 channel_freq_power_BR_BG,
	 sizeof(channel_freq_power_BR_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0x10,			/* US FCC */
	 channel_freq_power_US_BG,
	 sizeof(channel_freq_power_US_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0x20,			/* CANADA IC */
	 channel_freq_power_US_BG,
	 sizeof(channel_freq_power_US_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0x30,			/* EU */
	 channel_freq_power_EU_BG,
	 sizeof(channel_freq_power_EU_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0x32,			/* FRANCE */
	 channel_freq_power_FR_BG,
	 sizeof(channel_freq_power_FR_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0x40,			/* JAPAN */
	 channel_freq_power_JPN40_BG,
	 sizeof(channel_freq_power_JPN40_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0x41,			/* JAPAN */
	 channel_freq_power_JPN41_BG,
	 sizeof(channel_freq_power_JPN41_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0x50,			/* China */
	 channel_freq_power_EU_BG,
	 sizeof(channel_freq_power_EU_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{
	 0xfe,			/* JAPAN */
	 channel_freq_power_JPNFE_BG,
	 sizeof(channel_freq_power_JPNFE_BG) / sizeof(chan_freq_power_t),
	 }
	,
	{0xff,			/* Special */
	 channel_freq_power_SPECIAL_BG,
	 sizeof(channel_freq_power_SPECIAL_BG) / sizeof(chan_freq_power_t),
	 }
	,
/* Add new region here */
};

/** Number of the CFP tables for 2.4GHz */
#define MLAN_CFP_TABLE_SIZE_BG  (sizeof(cfp_table_BG)/sizeof(cfp_table_t))

/* Format { Channel, Frequency (MHz), MaxTxPower, DFS } */
/** Band: 'A', Region: USA FCC, Spain, France */
static chan_freq_power_t channel_freq_power_A[] = {
	{36, 5180, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{40, 5200, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{44, 5220, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{48, 5240, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{52, 5260, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{56, 5280, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{60, 5300, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{64, 5320, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{100, 5500, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{104, 5520, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{108, 5540, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{112, 5560, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{116, 5580, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{120, 5600, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{124, 5620, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{128, 5640, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{132, 5660, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{136, 5680, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{140, 5700, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{144, 5720, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{149, 5745, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{153, 5765, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{157, 5785, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{161, 5805, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{165, 5825, WLAN_TX_PWR_US_DEFAULT, MFALSE}
};

/** Band: 'A', Region: Canada IC */
static chan_freq_power_t channel_freq_power_CAN_A[] = {
	{36, 5180, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{40, 5200, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{44, 5220, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{48, 5240, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{52, 5260, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{56, 5280, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{60, 5300, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{64, 5320, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{100, 5500, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{104, 5520, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{108, 5540, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{112, 5560, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{116, 5580, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{132, 5660, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{136, 5680, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{140, 5700, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{144, 5720, WLAN_TX_PWR_US_DEFAULT, MTRUE},
	{149, 5745, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{153, 5765, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{157, 5785, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{161, 5805, WLAN_TX_PWR_US_DEFAULT, MFALSE},
	{165, 5825, WLAN_TX_PWR_US_DEFAULT, MFALSE}
};

/** Band: 'A', Region: Europe ETSI */
static chan_freq_power_t channel_freq_power_EU_A[] = {
	{36, 5180, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{40, 5200, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{44, 5220, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{48, 5240, WLAN_TX_PWR_EMEA_DEFAULT, MFALSE},
	{52, 5260, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{56, 5280, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{60, 5300, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{64, 5320, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{100, 5500, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{104, 5520, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{108, 5540, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{112, 5560, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{116, 5580, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{120, 5600, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{124, 5620, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{128, 5640, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{132, 5660, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{136, 5680, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE},
	{140, 5700, WLAN_TX_PWR_EMEA_DEFAULT, MTRUE}
};

/** Band: 'A', Region: Japan */
static chan_freq_power_t channel_freq_power_JPN_A[] = {
	{36, 5180, WLAN_TX_PWR_JP_A_DEFAULT, MFALSE},
	{40, 5200, WLAN_TX_PWR_JP_A_DEFAULT, MFALSE},
	{44, 5220, WLAN_TX_PWR_JP_A_DEFAULT, MFALSE},
	{48, 5240, WLAN_TX_PWR_JP_A_DEFAULT, MFALSE},
	{52, 5260, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{56, 5280, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{60, 5300, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{64, 5320, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{100, 5500, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{104, 5520, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{108, 5540, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{112, 5560, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{116, 5580, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{120, 5600, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{124, 5620, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{128, 5640, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{132, 5660, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{136, 5680, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE},
	{140, 5700, WLAN_TX_PWR_JP_A_DEFAULT, MTRUE}
};

/** Band: 'A', Region: China */
static chan_freq_power_t channel_freq_power_CN_A[] = {
	{149, 5745, WLAN_TX_PWR_CN_2000MW, MFALSE},
	{153, 5765, WLAN_TX_PWR_CN_2000MW, MFALSE},
	{157, 5785, WLAN_TX_PWR_CN_2000MW, MFALSE},
	{161, 5805, WLAN_TX_PWR_CN_2000MW, MFALSE},
	{165, 5825, WLAN_TX_PWR_CN_2000MW, MFALSE}
};

/** Band: 'A', NULL */
static chan_freq_power_t channel_freq_power_NULL_A[] = {
};

/** Band: 'A', Region: Germany */
static chan_freq_power_t channel_freq_power_GRM_A[] = {
	{36, 5180, WLAN_TX_PWR_200MW, MFALSE},
	{40, 5200, WLAN_TX_PWR_200MW, MFALSE},
	{44, 5220, WLAN_TX_PWR_200MW, MFALSE},
	{48, 5240, WLAN_TX_PWR_200MW, MFALSE},
	{52, 5260, WLAN_TX_PWR_200MW, MTRUE},
	{56, 5280, WLAN_TX_PWR_200MW, MTRUE},
	{60, 5300, WLAN_TX_PWR_200MW, MTRUE},
	{64, 5320, WLAN_TX_PWR_200MW, MTRUE},
	{100, 5500, WLAN_TX_PWR_1000MW, MTRUE},
	{104, 5520, WLAN_TX_PWR_1000MW, MTRUE},
	{108, 5540, WLAN_TX_PWR_1000MW, MTRUE},
	{112, 5560, WLAN_TX_PWR_1000MW, MTRUE},
	{116, 5580, WLAN_TX_PWR_1000MW, MTRUE},
	{120, 5600, WLAN_TX_PWR_1000MW, MTRUE},
	{124, 5620, WLAN_TX_PWR_1000MW, MTRUE},
	{128, 5640, WLAN_TX_PWR_1000MW, MTRUE},
	{132, 5660, WLAN_TX_PWR_1000MW, MTRUE},
	{136, 5680, WLAN_TX_PWR_1000MW, MTRUE},
	{140, 5700, WLAN_TX_PWR_1000MW, MTRUE},
};

/** Band: 'A', Region: Spain */
static chan_freq_power_t channel_freq_power_SPN0_A[] = {
	{36, 5180, WLAN_TX_PWR_SP_30MW, MFALSE},
	{40, 5200, WLAN_TX_PWR_SP_30MW, MFALSE},
	{44, 5220, WLAN_TX_PWR_SP_30MW, MFALSE},
	{48, 5240, WLAN_TX_PWR_SP_30MW, MFALSE},
};

/** Band: 'A', Region: Spain */
static chan_freq_power_t channel_freq_power_SPN1_A[] = {
	{36, 5180, WLAN_TX_PWR_SP_60MW, MFALSE},
	{40, 5200, WLAN_TX_PWR_SP_60MW, MFALSE},
	{44, 5220, WLAN_TX_PWR_SP_60MW, MFALSE},
	{48, 5240, WLAN_TX_PWR_SP_60MW, MFALSE},
};

/** Band: 'A', Region: Spain/Austria/Brazil */
static chan_freq_power_t channel_freq_power_SPN2_A[] = {
	{36, 5180, WLAN_TX_PWR_200MW, MFALSE},
	{40, 5200, WLAN_TX_PWR_200MW, MFALSE},
	{44, 5220, WLAN_TX_PWR_200MW, MFALSE},
	{48, 5240, WLAN_TX_PWR_200MW, MFALSE},
	{52, 5260, WLAN_TX_PWR_200MW, MTRUE},
	{56, 5280, WLAN_TX_PWR_200MW, MTRUE},
	{60, 5300, WLAN_TX_PWR_200MW, MTRUE},
	{64, 5320, WLAN_TX_PWR_200MW, MTRUE},
};

/** Band: 'A', Region: Spain/Austria */
static chan_freq_power_t channel_freq_power_SPN3_A[] = {
	{100, 5500, WLAN_TX_PWR_1000MW, MTRUE},
	{104, 5520, WLAN_TX_PWR_1000MW, MTRUE},
	{108, 5540, WLAN_TX_PWR_1000MW, MTRUE},
	{112, 5560, WLAN_TX_PWR_1000MW, MTRUE},
	{116, 5580, WLAN_TX_PWR_1000MW, MTRUE},
	{120, 5600, WLAN_TX_PWR_1000MW, MTRUE},
	{124, 5620, WLAN_TX_PWR_1000MW, MTRUE},
	{128, 5640, WLAN_TX_PWR_1000MW, MTRUE},
	{132, 5660, WLAN_TX_PWR_1000MW, MTRUE},
	{136, 5680, WLAN_TX_PWR_1000MW, MTRUE},
	{140, 5700, WLAN_TX_PWR_1000MW, MTRUE},
};

/** Band: 'A', Region: Austria */
static chan_freq_power_t channel_freq_power_AT_A[] = {
	{149, 5745, WLAN_TX_PWR_25MW, MFALSE},
	{153, 5765, WLAN_TX_PWR_25MW, MFALSE},
	{157, 5785, WLAN_TX_PWR_25MW, MFALSE},
	{161, 5805, WLAN_TX_PWR_25MW, MFALSE},
	{165, 5825, WLAN_TX_PWR_25MW, MFALSE}
};

/** Band: 'A', Region: Brazil */
static chan_freq_power_t channel_freq_power_BR1_A[] = {
	{100, 5500, WLAN_TX_PWR_250MW, MTRUE},
	{104, 5520, WLAN_TX_PWR_250MW, MTRUE},
	{108, 5540, WLAN_TX_PWR_250MW, MTRUE},
	{112, 5560, WLAN_TX_PWR_250MW, MTRUE},
	{116, 5580, WLAN_TX_PWR_250MW, MTRUE},
	{120, 5600, WLAN_TX_PWR_250MW, MTRUE},
	{124, 5620, WLAN_TX_PWR_250MW, MTRUE},
	{128, 5640, WLAN_TX_PWR_250MW, MTRUE},
	{132, 5660, WLAN_TX_PWR_250MW, MTRUE},
	{136, 5680, WLAN_TX_PWR_250MW, MTRUE},
	{140, 5700, WLAN_TX_PWR_250MW, MTRUE},
};

/** Band: 'A', Region: Brazil */
static chan_freq_power_t channel_freq_power_BR2_A[] = {
	{149, 5745, WLAN_TX_PWR_1000MW, MFALSE},
	{153, 5765, WLAN_TX_PWR_1000MW, MFALSE},
	{157, 5785, WLAN_TX_PWR_1000MW, MFALSE},
	{161, 5805, WLAN_TX_PWR_1000MW, MFALSE},
	{165, 5825, WLAN_TX_PWR_1000MW, MFALSE}
};

/** Band: 'A', Region: Russia */
static chan_freq_power_t channel_freq_power_RU_A[] = {
	{149, 5745, WLAN_TX_PWR_DEFAULT, MFALSE},
	{153, 5765, WLAN_TX_PWR_DEFAULT, MFALSE},
	{157, 5785, WLAN_TX_PWR_DEFAULT, MFALSE},
	{161, 5805, WLAN_TX_PWR_DEFAULT, MFALSE},
};

/** Band: 'A', Code: 1, Low band (5150-5250 MHz) channels */
static chan_freq_power_t channel_freq_power_low_band[] = {
	{36, 5180, WLAN_TX_PWR_DEFAULT, MFALSE},
	{40, 5200, WLAN_TX_PWR_DEFAULT, MFALSE},
	{44, 5220, WLAN_TX_PWR_DEFAULT, MFALSE},
	{48, 5240, WLAN_TX_PWR_DEFAULT, MFALSE},
};

/** Band: 'A', Code: 2, Lower middle band (5250-5350 MHz) channels */
static chan_freq_power_t channel_freq_power_lower_middle_band[] = {
	{52, 5260, WLAN_TX_PWR_DEFAULT, MTRUE},
	{56, 5280, WLAN_TX_PWR_DEFAULT, MTRUE},
	{60, 5300, WLAN_TX_PWR_DEFAULT, MTRUE},
	{64, 5320, WLAN_TX_PWR_DEFAULT, MTRUE},
};

/** Band: 'A', Code: 3, Upper middle band (5470-5725 MHz) channels */
static chan_freq_power_t channel_freq_power_upper_middle_band[] = {
	{100, 5500, WLAN_TX_PWR_DEFAULT, MTRUE},
	{104, 5520, WLAN_TX_PWR_DEFAULT, MTRUE},
	{108, 5540, WLAN_TX_PWR_DEFAULT, MTRUE},
	{112, 5560, WLAN_TX_PWR_DEFAULT, MTRUE},
	{116, 5580, WLAN_TX_PWR_DEFAULT, MTRUE},
	{120, 5600, WLAN_TX_PWR_DEFAULT, MTRUE},
	{124, 5620, WLAN_TX_PWR_DEFAULT, MTRUE},
	{128, 5640, WLAN_TX_PWR_DEFAULT, MTRUE},
	{132, 5660, WLAN_TX_PWR_DEFAULT, MTRUE},
	{136, 5680, WLAN_TX_PWR_DEFAULT, MTRUE},
	{140, 5700, WLAN_TX_PWR_DEFAULT, MTRUE},
};

/** Band: 'A', Code: 4, High band (5725-5850 MHz) channels */
static chan_freq_power_t channel_freq_power_high_band[] = {
	{149, 5745, WLAN_TX_PWR_DEFAULT, MFALSE},
	{153, 5765, WLAN_TX_PWR_DEFAULT, MFALSE},
	{157, 5785, WLAN_TX_PWR_DEFAULT, MFALSE},
	{161, 5805, WLAN_TX_PWR_DEFAULT, MFALSE},
	{165, 5825, WLAN_TX_PWR_DEFAULT, MFALSE}
};

/** Band: 'A', Code: 5, Low band (5150-5250 MHz) and
 * High band (5725-5850 MHz) channels */
static chan_freq_power_t channel_freq_power_low_high_band[] = {
	{36, 5180, WLAN_TX_PWR_DEFAULT, MFALSE},
	{40, 5200, WLAN_TX_PWR_DEFAULT, MFALSE},
	{44, 5220, WLAN_TX_PWR_DEFAULT, MFALSE},
	{48, 5240, WLAN_TX_PWR_DEFAULT, MFALSE},
	{149, 5745, WLAN_TX_PWR_DEFAULT, MFALSE},
	{153, 5765, WLAN_TX_PWR_DEFAULT, MFALSE},
	{157, 5785, WLAN_TX_PWR_DEFAULT, MFALSE},
	{161, 5805, WLAN_TX_PWR_DEFAULT, MFALSE},
	{165, 5825, WLAN_TX_PWR_DEFAULT, MFALSE}
};

/**
 * The 5GHz CFP tables
 */
static cfp_table_t cfp_table_A[] = {
	{0x1,			/* Low band (5150-5250 MHz) channels */
	 channel_freq_power_low_band,
	 sizeof(channel_freq_power_low_band) / sizeof(chan_freq_power_t)
	 }
	,
	{0x2,			/* Lower middle band (5250-5350 MHz) channels */
	 channel_freq_power_lower_middle_band,
	 sizeof(channel_freq_power_lower_middle_band) /
	 sizeof(chan_freq_power_t)
	 }
	,
	{0x3,			/* Upper middle band (5470-5725 MHz) channels */
	 channel_freq_power_upper_middle_band,
	 sizeof(channel_freq_power_upper_middle_band) /
	 sizeof(chan_freq_power_t)
	 }
	,
	{0x4,			/* High band (5725-5850 MHz) channels */
	 channel_freq_power_high_band,
	 sizeof(channel_freq_power_high_band) / sizeof(chan_freq_power_t)
	 }
	,
	{0x5,			/* Low band (5150-5250 MHz) and High band
				   (5725-5850 MHz) channels */
	 channel_freq_power_low_high_band,
	 sizeof(channel_freq_power_low_high_band) / sizeof(chan_freq_power_t)
	 }
	,
	{0x06,			/* GERMANY */
	 channel_freq_power_GRM_A,
	 sizeof(channel_freq_power_GRM_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x07,			/* SPAIN */
	 channel_freq_power_SPN0_A,
	 sizeof(channel_freq_power_SPN0_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x08,			/* SPAIN */
	 channel_freq_power_SPN1_A,
	 sizeof(channel_freq_power_SPN1_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x09,			/* SPAIN/Austria/Brazil */
	 channel_freq_power_SPN2_A,
	 sizeof(channel_freq_power_SPN2_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x0a,			/* SPAIN/Austria */
	 channel_freq_power_SPN3_A,
	 sizeof(channel_freq_power_SPN3_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x0b,			/* Austria */
	 channel_freq_power_AT_A,
	 sizeof(channel_freq_power_AT_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x0c,			/* Brazil */
	 channel_freq_power_BR1_A,
	 sizeof(channel_freq_power_BR1_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x0e,			/* Brazil */
	 channel_freq_power_BR2_A,
	 sizeof(channel_freq_power_BR2_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x0f,			/* Russia */
	 channel_freq_power_RU_A,
	 sizeof(channel_freq_power_RU_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x10,			/* US FCC */
	 channel_freq_power_A,
	 sizeof(channel_freq_power_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x20,			/* CANADA IC */
	 channel_freq_power_CAN_A,
	 sizeof(channel_freq_power_CAN_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x30,			/* EU */
	 channel_freq_power_EU_A,
	 sizeof(channel_freq_power_EU_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x32,			/* FRANCE */
	 channel_freq_power_A,
	 sizeof(channel_freq_power_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x40,			/* JAPAN */
	 channel_freq_power_JPN_A,
	 sizeof(channel_freq_power_JPN_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x41,			/* JAPAN */
	 channel_freq_power_JPN_A,
	 sizeof(channel_freq_power_JPN_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0x50,			/* China */
	 channel_freq_power_CN_A,
	 sizeof(channel_freq_power_CN_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0xfe,			/* JAPAN */
	 channel_freq_power_NULL_A,
	 sizeof(channel_freq_power_NULL_A) / sizeof(chan_freq_power_t),
	 }
	,
	{0xff,			/* Special */
	 channel_freq_power_JPN_A,
	 sizeof(channel_freq_power_JPN_A) / sizeof(chan_freq_power_t),
	 }
	,
/* Add new region here */
};

/** Number of the CFP tables for 5GHz */
#define MLAN_CFP_TABLE_SIZE_A   (sizeof(cfp_table_A)/sizeof(cfp_table_t))

/********************************************************
    Global Variables
********************************************************/
/**
 * The table to keep region code
 */
t_u16 region_code_index[MRVDRV_MAX_REGION_CODE] =
	{ 0x10, 0x20, 0x30, 0x32, 0x40, 0x41, 0x50, 0xfe, 0xff };

/** The table to keep CFP code for BG */
t_u16 cfp_code_index_bg[MRVDRV_MAX_CFP_CODE_BG] = { };

/** The table to keep CFP code for A */
t_u16 cfp_code_index_a[MRVDRV_MAX_CFP_CODE_A] = { 0x1, 0x2, 0x3, 0x4, 0x5 };

/**
 * The rates supported for ad-hoc B mode
 */
t_u8 AdhocRates_B[B_SUPPORTED_RATES] = { 0x82, 0x84, 0x8b, 0x96, 0 };

/**
 * The rates supported for ad-hoc G mode
 */
t_u8 AdhocRates_G[G_SUPPORTED_RATES] =
	{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0 };

/**
 * The rates supported for ad-hoc BG mode
 */
t_u8 AdhocRates_BG[BG_SUPPORTED_RATES] =
	{ 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48,
	0x60, 0x6c, 0
};

/**
 * The rates supported in A mode for ad-hoc
 */
t_u8 AdhocRates_A[A_SUPPORTED_RATES] =
	{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0 };

/**
 * The rates supported in A mode (used for BAND_A)
 */
t_u8 SupportedRates_A[A_SUPPORTED_RATES] =
	{ 0x0c, 0x12, 0x18, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0 };

/**
 * The rates supported by the card
 */
t_u16 WlanDataRates[WLAN_SUPPORTED_RATES_EXT] =
	{ 0x02, 0x04, 0x0B, 0x16, 0x00, 0x0C, 0x12,
	0x18, 0x24, 0x30, 0x48, 0x60, 0x6C, 0x90,
	0x0D, 0x1A, 0x27, 0x34, 0x4E, 0x68, 0x75,
	0x82, 0x0C, 0x1B, 0x36, 0x51, 0x6C, 0xA2,
	0xD8, 0xF3, 0x10E, 0x00
};

/**
 * The rates supported in B mode
 */
t_u8 SupportedRates_B[B_SUPPORTED_RATES] = { 0x02, 0x04, 0x0b, 0x16, 0 };

/**
 * The rates supported in G mode (BAND_G, BAND_G|BAND_GN)
 */
t_u8 SupportedRates_G[G_SUPPORTED_RATES] =
	{ 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c, 0 };

/**
 * The rates supported in BG mode (BAND_B|BAND_G, BAND_B|BAND_G|BAND_GN)
 */
t_u8 SupportedRates_BG[BG_SUPPORTED_RATES] =
	{ 0x02, 0x04, 0x0b, 0x0c, 0x12, 0x16, 0x18, 0x24, 0x30, 0x48,
	0x60, 0x6c, 0
};

/**
 * The rates supported in N mode
 */
t_u8 SupportedRates_N[N_SUPPORTED_RATES] = { 0x02, 0x04, 0 };

/********************************************************
    Local Functions
********************************************************/
/**
 *  @brief Find a character in a string.
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param s            A pointer to string
 *  @param c            Character to be located
 *  @param n            The length of string
 *
 *  @return        A pointer to the first occurrence of c in string, or MNULL if c is not found.
 */
static void *
wlan_memchr(pmlan_adapter pmadapter, void *s, int c, int n)
{
	const t_u8 *p = (t_u8 *) s;

	ENTER();

	while (n--) {
		if ((t_u8) c == *p++) {
			LEAVE();
			return (void *)(p - 1);
		}
	}

	LEAVE();
	return MNULL;
}

/**
 *  @brief This function finds the CFP in
 *  		cfp_table_BG/A based on region/code and band parameter.
 *
 *  @param pmadapter  A pointer to mlan_adapter structure
 *  @param region     The region code
 *  @param band       The band
 *  @param cfp_no     A pointer to CFP number
 *
 *  @return           A pointer to CFP
 */
static chan_freq_power_t *
wlan_get_region_cfp_table(pmlan_adapter pmadapter, t_u8 region, t_u8 band,
			  int *cfp_no)
{
	t_u32 i;
	t_u8 cfp_bg, cfp_a;

	ENTER();

	cfp_bg = cfp_a = region;
	if (!region) {
		/* Invalid region code, use CFP code */
		cfp_bg = pmadapter->cfp_code_bg;
		cfp_a = pmadapter->cfp_code_a;
	}

	if (band & (BAND_B | BAND_G | BAND_GN | BAND_GAC)) {
		for (i = 0; i < MLAN_CFP_TABLE_SIZE_BG; i++) {
			PRINTM(MINFO, "cfp_table_BG[%d].code=%d\n", i,
			       cfp_table_BG[i].code);
			/* Check if region/code matches for BG bands */
			if (cfp_table_BG[i].code == cfp_bg) {
				/* Select by band */
				*cfp_no = cfp_table_BG[i].cfp_no;
				LEAVE();
				return cfp_table_BG[i].cfp;
			}
		}
	}
	if (band & (BAND_A | BAND_AN | BAND_AAC)) {
		for (i = 0; i < MLAN_CFP_TABLE_SIZE_A; i++) {
			PRINTM(MINFO, "cfp_table_A[%d].code=%d\n", i,
			       cfp_table_A[i].code);
			/* Check if region/code matches for A bands */
			if (cfp_table_A[i].code == cfp_a) {
				/* Select by band */
				*cfp_no = cfp_table_A[i].cfp_no;
				LEAVE();
				return cfp_table_A[i].cfp;
			}
		}
	}

	if (!region)
		PRINTM(MERROR, "Error Band[0x%x] or code[BG:%#x, A:%#x]\n",
		       band, cfp_bg, cfp_a);
	else
		PRINTM(MERROR, "Error Band[0x%x] or region[%#x]\n", band,
		       region);

	LEAVE();
	return MNULL;
}

/**
 *  @brief This function copies dynamic CFP elements from one table to another.
 *         Only copy elements where channel numbers match.
 *
 *  @param pmadapter   A pointer to mlan_adapter structure
 *  @param cfp         Destination table
 *  @param num_cfp     Number of elements in dest table
 *  @param cfp_src     Source table
 *  @param num_cfp_src Number of elements in source table
 */
static t_void
wlan_cfp_copy_dynamic(pmlan_adapter pmadapter,
		      chan_freq_power_t * cfp, t_u8 num_cfp,
		      chan_freq_power_t * cfp_src, t_u8 num_cfp_src)
{
	int i, j;
	ENTER();

	/* first clear dest dynamic entries */
	for (i = 0; i < num_cfp; i++)
		memset(pmadapter, &cfp[i].dynamic, 0x00, sizeof(cfp_dyn_t));

	/* copy dynamic entries from source where channels match */
	if (cfp_src) {
		for (i = 0; i < num_cfp; i++)
			for (j = 0; j < num_cfp_src; j++)
				if (cfp[i].channel == cfp_src[j].channel) {
					cfp[i].dynamic = cfp_src[j].dynamic;
					break;
				}
	}

	LEAVE();
}

/********************************************************
    Global Functions
********************************************************/
/**
 *  @brief This function converts region string to integer code
 *
 *  @param pmadapter        A pointer to mlan_adapter structure
 *  @param country_code     Country string
 *  @param cfp_bg           Pointer to buffer
 *  @param cfp_a            Pointer to buffer
 *
 *  @return                 MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_misc_country_2_cfp_table_code(pmlan_adapter pmadapter, t_u8 * country_code,
				   t_u8 * cfp_bg, t_u8 * cfp_a)
{
	t_u8 i;

	ENTER();

	/* Look for code in mapping table */
	for (i = 0; i < NELEMENTS(country_code_mapping); i++) {
		if (!memcmp(pmadapter, country_code_mapping[i].country_code,
			    country_code, COUNTRY_CODE_LEN - 1)) {
			*cfp_bg = country_code_mapping[i].cfp_code_bg;
			*cfp_a = country_code_mapping[i].cfp_code_a;
			LEAVE();
			return MLAN_STATUS_SUCCESS;
		}
	}

	LEAVE();
	return MLAN_STATUS_FAILURE;
}

#ifdef STA_SUPPORT
#endif /* STA_SUPPORT */

/**
 *  @brief Use index to get the data rate
 *
 *  @param pmadapter        A pointer to mlan_adapter structure
 *  @param index            The index of data rate
 *  @param tx_rate_info     Tx rate info
 *
 *  @return                 Data rate or 0
 */
t_u32
wlan_index_to_data_rate(pmlan_adapter pmadapter, t_u8 index, t_u8 tx_rate_info)
{
#define MCS_NUM_SUPP    16
	t_u16 mcs_rate[4][MCS_NUM_SUPP] =
		{ {0x1b, 0x36, 0x51, 0x6c, 0xa2, 0xd8, 0xf3, 0x10e, 0x36, 0x6c,
		   0xa2, 0xd8, 0x144, 0x1b0, 0x1e6, 0x21c}
	,			/* LG 40M */
	{0x1e, 0x3c, 0x5a, 0x78, 0xb4, 0xf0, 0x10e, 0x12c, 0x3c, 0x78, 0xb4,
	 0xf0, 0x168, 0x1e0, 0x21c, 0x258}
	,			/* SG 40M */
	{0x0d, 0x1a, 0x27, 0x34, 0x4e, 0x68, 0x75, 0x82, 0x1a, 0x34, 0x4e, 0x68,
	 0x9c, 0xd0, 0xea, 0x104}
	,			/* LG 20M */
	{0x0e, 0x1c, 0x2b, 0x39, 0x56, 0x73, 0x82, 0x90, 0x1c, 0x39, 0x56, 0x73,
	 0xad, 0xe7, 0x104, 0x120}
	};			/* SG 20M */

#define MCS_NUM_AC    10
	/* NSS 1. note: the value in the table is 2 multiplier of the actual
	   rate in other words, it is in the unit of 500 Kbs */
	t_u16 ac_mcs_rate_nss1[8][MCS_NUM_AC] = {
		{0x75, 0xEA, 0x15F, 0x1D4, 0x2BE, 0x3A8, 0x41D, 0x492, 0x57C,
		 0x618}
		,		/* LG 160M */
		{0x82, 0x104, 0x186, 0x208, 0x30C, 0x410, 0x492, 0x514, 0x618,
		 0x6C6}
		,		/* SG 160M */
		{0x3B, 0x75, 0xB0, 0xEA, 0x15F, 0x1D4, 0x20F, 0x249, 0x2BE,
		 0x30C}
		,		/* LG 80M */
		{0x41, 0x82, 0xC3, 0x104, 0x186, 0x208, 0x249, 0x28A, 0x30C,
		 0x363}
		,		/* SG 80M */
		{0x1B, 0x36, 0x51, 0x6C, 0xA2, 0xD8, 0xF3, 0x10E, 0x144, 0x168}
		,		/* LG 40M */
		{0x1E, 0x3C, 0x5A, 0x78, 0xB4, 0xF0, 0x10E, 0x12C, 0x168, 0x190}
		,		/* SG 40M */
		{0xD, 0x1A, 0x27, 0x34, 0x4E, 0x68, 0x75, 0x82, 0x9C, 0x00}
		,		/* LG 20M */
		{0xF, 0x1D, 0x2C, 0x3A, 0x57, 0x74, 0x82, 0x91, 0xAE, 0x00}
		,		/* SG 20M */
	};
	/* NSS 2. note: the value in the table is 2 multiplier of the actual
	   rate */
	t_u16 ac_mcs_rate_nss2[8][MCS_NUM_AC] = {
		{0xEA, 0x1D4, 0x2BE, 0x3A8, 0x57C, 0x750, 0x83A, 0x924, 0xAF8,
		 0xC30}
		,		/* LG 160M */
		{0x104, 0x208, 0x30C, 0x410, 0x618, 0x820, 0x924, 0xA28, 0xC30,
		 0xD8B}
		,		/* SG 160M */

		{0x75, 0xEA, 0x15F, 0x1D4, 0x2BE, 0x3A8, 0x41D, 0x492, 0x57C,
		 0x618}
		,		/* LG 80M */
		{0x82, 0x104, 0x186, 0x208, 0x30C, 0x410, 0x492, 0x514, 0x618,
		 0x6C6}
		,		/* SG 80M */
		{0x36, 0x6C, 0xA2, 0xD8, 0x144, 0x1B0, 0x1E6, 0x21C, 0x288,
		 0x2D0}
		,		/* LG 40M */
		{0x3C, 0x78, 0xB4, 0xF0, 0x168, 0x1E0, 0x21C, 0x258, 0x2D0,
		 0x320}
		,		/* SG 40M */
		{0x1A, 0x34, 0x4A, 0x68, 0x9C, 0xD0, 0xEA, 0x104, 0x138, 0x00}
		,		/* LG 20M */
		{0x1D, 0x3A, 0x57, 0x74, 0xAE, 0xE6, 0x104, 0x121, 0x15B, 0x00}
		,		/* SG 20M */
	};

	t_u32 rate = 0;
	t_u8 mcs_index = 0;
	t_u8 bw = 0;
	t_u8 gi = 0;
	ENTER();

	if ((tx_rate_info & 0x3) == MLAN_RATE_FORMAT_VHT) {
		/* VHT rate */
		mcs_index = MIN(index & 0xF, 9);
		/* 20M: bw=0, 40M: bw=1, 80M: bw=2, 160M: bw=3 */
		bw = (tx_rate_info & 0xC) >> 2;
		/* LGI: gi =0, SGI: gi = 1 */
		gi = (tx_rate_info & 0x10) >> 4;
		if ((index >> 4) == 1) {
			/* NSS = 2 */
			rate = ac_mcs_rate_nss2[2 * (3 - bw) + gi][mcs_index];
		} else
			/* NSS = 1 */
			rate = ac_mcs_rate_nss1[2 * (3 - bw) + gi][mcs_index];
	} else if ((tx_rate_info & 0x3) == MLAN_RATE_FORMAT_HT) {
		/* HT rate */
		/* 20M: bw=0, 40M: bw=1 */
		bw = (tx_rate_info & 0xC) >> 2;
		/* LGI: gi =0, SGI: gi = 1 */
		gi = (tx_rate_info & 0x10) >> 4;
		if (index == MLAN_RATE_BITMAP_MCS0) {
			if (gi == 1)
				rate = 0x0D;	/* MCS 32 SGI rate */
			else
				rate = 0x0C;	/* MCS 32 LGI rate */
		} else if (index < MCS_NUM_SUPP) {
			if (bw <= 1)
				rate = mcs_rate[2 * (1 - bw) + gi][index];
			else
				rate = WlanDataRates[0];
		} else
			rate = WlanDataRates[0];
	} else {
		/* 11n non HT rates */
		if (index >= WLAN_SUPPORTED_RATES_EXT)
			index = 0;
		rate = WlanDataRates[index];
	}
	LEAVE();
	return rate;
}

/**
 *  @brief Use rate to get the index
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param rate         Data rate
 *
 *  @return                     Index or 0
 */
t_u8
wlan_data_rate_to_index(pmlan_adapter pmadapter, t_u32 rate)
{
	t_u16 *ptr;

	ENTER();
	if (rate)
		if ((ptr = wlan_memchr(pmadapter, WlanDataRates, (t_u8) rate,
				       sizeof(WlanDataRates)))) {
			LEAVE();
			return (t_u8) (ptr - WlanDataRates);
		}
	LEAVE();
	return 0;
}

/**
 *  @brief Get active data rates
 *
 *  @param pmpriv           A pointer to mlan_private structure
 *  @param bss_mode         The specified BSS mode (Infra/IBSS)
 *  @param config_bands     The specified band configuration
 *  @param rates            The buf to return the active rates
 *
 *  @return                 The number of Rates
 */
t_u32
wlan_get_active_data_rates(mlan_private * pmpriv, t_u32 bss_mode,
			   t_u8 config_bands, WLAN_802_11_RATES rates)
{
	t_u32 k;

	ENTER();

	if (pmpriv->media_connected != MTRUE) {
		k = wlan_get_supported_rates(pmpriv, bss_mode, config_bands,
					     rates);
	} else {
		k = wlan_copy_rates(rates, 0,
				    pmpriv->curr_bss_params.data_rates,
				    pmpriv->curr_bss_params.num_of_rates);
	}

	LEAVE();
	return k;
}

#ifdef STA_SUPPORT
/**
 *  @brief This function search through all the regions cfp table to find the channel,
 *            if the channel is found then gets the MIN txpower of the channel
 *            present in all the regions.
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param channel      Channel number.
 *
 *  @return             The Tx power
 */
t_u8
wlan_get_txpwr_of_chan_from_cfp(mlan_private * pmpriv, t_u8 channel)
{
	t_u8 i = 0;
	t_u8 j = 0;
	t_u8 tx_power = 0;
	t_u32 cfp_no;
	chan_freq_power_t *cfp = MNULL;
	chan_freq_power_t *cfp_a = MNULL;
	t_u32 cfp_no_a;

	ENTER();

	for (i = 0; i < MLAN_CFP_TABLE_SIZE_BG; i++) {
		/* Get CFP */
		cfp = cfp_table_BG[i].cfp;
		cfp_no = cfp_table_BG[i].cfp_no;
		/* Find matching channel and get Tx power */
		for (j = 0; j < cfp_no; j++) {
			if ((cfp + j)->channel == channel) {
				if (tx_power != 0)
					tx_power =
						MIN(tx_power,
						    (cfp + j)->max_tx_power);
				else
					tx_power =
						(t_u8) (cfp + j)->max_tx_power;
				break;
			}
		}
	}

	for (i = 0; i < MLAN_CFP_TABLE_SIZE_A; i++) {
		/* Get CFP */
		cfp_a = cfp_table_A[i].cfp;
		cfp_no_a = cfp_table_A[i].cfp_no;
		for (j = 0; j < cfp_no_a; j++) {
			if ((cfp_a + j)->channel == channel) {
				if (tx_power != 0)
					tx_power =
						MIN(tx_power,
						    (cfp_a + j)->max_tx_power);
				else
					tx_power =
						(t_u8) ((cfp_a +
							 j)->max_tx_power);
				break;
			}
		}
	}

	LEAVE();
	return tx_power;
}

/**
 *  @brief Get the channel frequency power info for a specific channel
 *
 *  @param pmadapter            A pointer to mlan_adapter structure
 *  @param band                 It can be BAND_A, BAND_G or BAND_B
 *  @param channel              The channel to search for
 *  @param region_channel       A pointer to region_chan_t structure
 *
 *  @return                     A pointer to chan_freq_power_t structure or MNULL if not found.
 */

chan_freq_power_t *
wlan_get_cfp_by_band_and_channel(pmlan_adapter pmadapter,
				 t_u8 band,
				 t_u16 channel, region_chan_t * region_channel)
{
	region_chan_t *rc;
	chan_freq_power_t *cfp = MNULL;
	int i, j;

	ENTER();

	for (j = 0; !cfp && (j < MAX_REGION_CHANNEL_NUM); j++) {
		rc = &region_channel[j];

		if (!rc->valid || !rc->pcfp)
			continue;
		switch (rc->band) {
		case BAND_A:
			switch (band) {
			case BAND_AN:
			case BAND_A | BAND_AN:
			case BAND_A | BAND_AN | BAND_AAC:
			case BAND_A:	/* Matching BAND_A */
				break;

			default:
				continue;
			}
			break;
		case BAND_B:
		case BAND_G:
			switch (band) {
			case BAND_GN:
			case BAND_B | BAND_G | BAND_GN:
			case BAND_G | BAND_GN:
			case BAND_GN | BAND_GAC:
			case BAND_B | BAND_G | BAND_GN | BAND_GAC:
			case BAND_G | BAND_GN | BAND_GAC:
			case BAND_B | BAND_G:
			case BAND_B:	/* Matching BAND_B/G */
			case BAND_G:
			case 0:
				break;
			default:
				continue;
			}
			break;
		default:
			continue;
		}
		if (channel == FIRST_VALID_CHANNEL)
			cfp = &rc->pcfp[0];
		else {
			for (i = 0; i < rc->num_cfp; i++) {
				if (rc->pcfp[i].channel == channel) {
					cfp = &rc->pcfp[i];
					break;
				}
			}
		}
	}

	if (!cfp && channel)
		PRINTM(MCMND, "wlan_get_cfp_by_band_and_channel(): cannot find "
		       "cfp by band %d & channel %d\n", band, channel);

	LEAVE();
	return cfp;
}

/**
 *  @brief Find the channel frequency power info for a specific channel
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param band         It can be BAND_A, BAND_G or BAND_B
 *  @param channel      The channel to search for
 *
 *  @return             A pointer to chan_freq_power_t structure or MNULL if not found.
 */
chan_freq_power_t *
wlan_find_cfp_by_band_and_channel(mlan_adapter * pmadapter,
				  t_u8 band, t_u16 channel)
{
	chan_freq_power_t *cfp = MNULL;

	ENTER();

	/* Any station(s) with 11D enabled */
	if (wlan_count_priv_cond(pmadapter, wlan_11d_is_enabled,
				 wlan_is_station) > 0)
		cfp = wlan_get_cfp_by_band_and_channel(pmadapter, band, channel,
						       pmadapter->
						       universal_channel);
	else
		cfp = wlan_get_cfp_by_band_and_channel(pmadapter, band, channel,
						       pmadapter->
						       region_channel);

	LEAVE();
	return cfp;
}

/**
 *  @brief Find the channel frequency power info for a specific frequency
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param band         It can be BAND_A, BAND_G or BAND_B
 *  @param freq         The frequency to search for
 *
 *  @return         Pointer to chan_freq_power_t structure; MNULL if not found
 */
chan_freq_power_t *
wlan_find_cfp_by_band_and_freq(mlan_adapter * pmadapter, t_u8 band, t_u32 freq)
{
	chan_freq_power_t *cfp = MNULL;
	region_chan_t *rc;
	int i, j;

	ENTER();

	for (j = 0; !cfp && (j < MAX_REGION_CHANNEL_NUM); j++) {
		rc = &pmadapter->region_channel[j];

		/* Any station(s) with 11D enabled */
		if (wlan_count_priv_cond(pmadapter, wlan_11d_is_enabled,
					 wlan_is_station) > 0)
			rc = &pmadapter->universal_channel[j];

		if (!rc->valid || !rc->pcfp)
			continue;
		switch (rc->band) {
		case BAND_A:
			switch (band) {
			case BAND_AN:
			case BAND_A | BAND_AN:
			case BAND_A | BAND_AN | BAND_AAC:
			case BAND_A:	/* Matching BAND_A */
				break;
			default:
				continue;
			}
			break;
		case BAND_B:
		case BAND_G:
			switch (band) {
			case BAND_GN:
			case BAND_B | BAND_G | BAND_GN:
			case BAND_G | BAND_GN:
			case BAND_GN | BAND_GAC:
			case BAND_B | BAND_G | BAND_GN | BAND_GAC:
			case BAND_G | BAND_GN | BAND_GAC:
			case BAND_B | BAND_G:
			case BAND_B:
			case BAND_G:
			case 0:
				break;
			default:
				continue;
			}
			break;
		default:
			continue;
		}
		for (i = 0; i < rc->num_cfp; i++) {
			if (rc->pcfp[i].freq == freq) {
				cfp = &rc->pcfp[i];
				break;
			}
		}
	}

	if (!cfp && freq)
		PRINTM(MERROR,
		       "wlan_find_cfp_by_band_and_freq(): cannot find cfp by "
		       "band %d & freq %d\n", band, freq);

	LEAVE();
	return cfp;
}
#endif /* STA_SUPPORT */

/**
 *  @brief Check if Rate Auto
 *
 *  @param pmpriv               A pointer to mlan_private structure
 *
 *  @return                     MTRUE or MFALSE
 */
t_u8
wlan_is_rate_auto(mlan_private * pmpriv)
{
	t_u32 i;
	int rate_num = 0;

	ENTER();

	for (i = 0; i < NELEMENTS(pmpriv->bitmap_rates); i++)
		if (pmpriv->bitmap_rates[i])
			rate_num++;

	LEAVE();
	if (rate_num > 1)
		return MTRUE;
	else
		return MFALSE;
}

/**
 *  @brief Covert Rate Bitmap to Rate index
 *
 *  @param pmadapter    Pointer to mlan_adapter structure
 *  @param rate_bitmap  Pointer to rate bitmap
 *  @param size         Size of the bitmap array
 *
 *  @return             Rate index
 */
int
wlan_get_rate_index(pmlan_adapter pmadapter, t_u16 * rate_bitmap, int size)
{
	int i;

	ENTER();

	for (i = 0; i < size * 8; i++) {
		if (rate_bitmap[i / 16] & (1 << (i % 16))) {
			LEAVE();
			return i;
		}
	}

	LEAVE();
	return -1;
}

/**
 *  @brief Get supported data rates
 *
 *  @param pmpriv           A pointer to mlan_private structure
 *  @param bss_mode         The specified BSS mode (Infra/IBSS)
 *  @param config_bands     The specified band configuration
 *  @param rates            The buf to return the supported rates
 *
 *  @return                 The number of Rates
 */
t_u32
wlan_get_supported_rates(mlan_private * pmpriv, t_u32 bss_mode,
			 t_u8 config_bands, WLAN_802_11_RATES rates)
{
	t_u32 k = 0;

	ENTER();

	if (bss_mode == MLAN_BSS_MODE_INFRA) {
		/* Infra. mode */
		switch (config_bands) {
		case BAND_B:
			PRINTM(MINFO, "Infra Band=%d SupportedRates_B\n",
			       config_bands);
			k = wlan_copy_rates(rates, k, SupportedRates_B,
					    sizeof(SupportedRates_B));
			break;
		case BAND_G:
		case BAND_G | BAND_GN:
		case BAND_G | BAND_GN | BAND_GAC:
			PRINTM(MINFO, "Infra band=%d SupportedRates_G\n",
			       config_bands);
			k = wlan_copy_rates(rates, k, SupportedRates_G,
					    sizeof(SupportedRates_G));
			break;
		case BAND_B | BAND_G:
		case BAND_A | BAND_B | BAND_G:
		case BAND_A | BAND_B:
		case BAND_A | BAND_B | BAND_G | BAND_GN | BAND_AN:
		case BAND_A | BAND_B | BAND_G | BAND_GN | BAND_AN | BAND_AAC:
		case BAND_A | BAND_B | BAND_G | BAND_GN | BAND_AN | BAND_AAC | BAND_GAC:
		case BAND_B | BAND_G | BAND_GN:
		case BAND_B | BAND_G | BAND_GN | BAND_GAC:
			PRINTM(MINFO, "Infra band=%d SupportedRates_BG\n",
			       config_bands);
#ifdef WIFI_DIRECT_SUPPORT
			if (pmpriv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT)
				k = wlan_copy_rates(rates, k, SupportedRates_G,
						    sizeof(SupportedRates_G));
			else
				k = wlan_copy_rates(rates, k, SupportedRates_BG,
						    sizeof(SupportedRates_BG));
#else
			k = wlan_copy_rates(rates, k, SupportedRates_BG,
					    sizeof(SupportedRates_BG));
#endif
			break;
		case BAND_A:
		case BAND_A | BAND_G:
			PRINTM(MINFO, "Infra band=%d SupportedRates_A\n",
			       config_bands);
			k = wlan_copy_rates(rates, k, SupportedRates_A,
					    sizeof(SupportedRates_A));
			break;
		case BAND_AN:
		case BAND_A | BAND_AN:
		case BAND_A | BAND_G | BAND_AN | BAND_GN:
		case BAND_A | BAND_G | BAND_AN | BAND_GN | BAND_AAC:
			PRINTM(MINFO, "Infra band=%d SupportedRates_A\n",
			       config_bands);
			k = wlan_copy_rates(rates, k, SupportedRates_A,
					    sizeof(SupportedRates_A));
			break;
		case BAND_GN:
		case BAND_GN | BAND_GAC:
			PRINTM(MINFO, "Infra band=%d SupportedRates_N\n",
			       config_bands);
			k = wlan_copy_rates(rates, k, SupportedRates_N,
					    sizeof(SupportedRates_N));
			break;
		}
	} else {
		/* Ad-hoc mode */
		switch (config_bands) {
		case BAND_B:
			PRINTM(MINFO, "Band: Adhoc B\n");
			k = wlan_copy_rates(rates, k, AdhocRates_B,
					    sizeof(AdhocRates_B));
			break;
		case BAND_G:
		case BAND_G | BAND_GN:
		case BAND_G | BAND_GN | BAND_GAC:
			PRINTM(MINFO, "Band: Adhoc G only\n");
			k = wlan_copy_rates(rates, k, AdhocRates_G,
					    sizeof(AdhocRates_G));
			break;
		case BAND_B | BAND_G:
		case BAND_B | BAND_G | BAND_GN:
		case BAND_B | BAND_G | BAND_GN | BAND_GAC:
			PRINTM(MINFO, "Band: Adhoc BG\n");
			k = wlan_copy_rates(rates, k, AdhocRates_BG,
					    sizeof(AdhocRates_BG));
			break;
		case BAND_A:
		case BAND_AN:
		case BAND_A | BAND_AN:
		case BAND_A | BAND_AN | BAND_AAC:
			PRINTM(MINFO, "Band: Adhoc A\n");
			k = wlan_copy_rates(rates, k, AdhocRates_A,
					    sizeof(AdhocRates_A));
			break;
		}
	}

	LEAVE();
	return k;
}

/**
 *  @brief This function sets region table.
 *
 *  @param pmpriv  A pointer to mlan_private structure
 *  @param region  The region code
 *  @param band    The band
 *
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_set_regiontable(mlan_private * pmpriv, t_u8 region, t_u8 band)
{
	mlan_adapter *pmadapter = pmpriv->adapter;
	int i = 0, j;
	chan_freq_power_t *cfp;
	int cfp_no;
	region_chan_t region_chan_old[MAX_REGION_CHANNEL_NUM];

	ENTER();

	memcpy(pmadapter, region_chan_old, pmadapter->region_channel,
	       sizeof(pmadapter->region_channel));
	memset(pmadapter, pmadapter->region_channel, 0,
	       sizeof(pmadapter->region_channel));

	if (band & (BAND_B | BAND_G | BAND_GN)) {
		cfp = wlan_get_region_cfp_table(pmadapter, region,
						BAND_G | BAND_B | BAND_GN,
						&cfp_no);
		if (cfp) {
			pmadapter->region_channel[i].num_cfp = (t_u8) cfp_no;
			pmadapter->region_channel[i].pcfp = cfp;
		} else {
			PRINTM(MERROR, "wrong region code %#x in Band B-G\n",
			       region);
			LEAVE();
			return MLAN_STATUS_FAILURE;
		}
		pmadapter->region_channel[i].valid = MTRUE;
		pmadapter->region_channel[i].region = region;
		if (band & BAND_GN)
			pmadapter->region_channel[i].band = BAND_G;
		else
			pmadapter->region_channel[i].band =
				(band & BAND_G) ? BAND_G : BAND_B;

		for (j = 0; j < MAX_REGION_CHANNEL_NUM; j++) {
			if (region_chan_old[j].band & (BAND_B | BAND_G))
				break;
		}
		if ((j < MAX_REGION_CHANNEL_NUM) && region_chan_old[j].valid)
			wlan_cfp_copy_dynamic(pmadapter, cfp, cfp_no,
					      region_chan_old[j].pcfp,
					      region_chan_old[j].num_cfp);
		else
			wlan_cfp_copy_dynamic(pmadapter, cfp, cfp_no, MNULL, 0);

		i++;
	}
	if (band & (BAND_A | BAND_AN | BAND_AAC)) {
		cfp = wlan_get_region_cfp_table(pmadapter, region, BAND_A,
						&cfp_no);
		if (cfp) {
			pmadapter->region_channel[i].num_cfp = (t_u8) cfp_no;
			pmadapter->region_channel[i].pcfp = cfp;
		} else {
			PRINTM(MERROR, "wrong region code %#x in Band A\n",
			       region);
			LEAVE();
			return MLAN_STATUS_FAILURE;
		}
		pmadapter->region_channel[i].valid = MTRUE;
		pmadapter->region_channel[i].region = region;
		pmadapter->region_channel[i].band = BAND_A;

		for (j = 0; j < MAX_REGION_CHANNEL_NUM; j++) {
			if (region_chan_old[j].band & BAND_A)
				break;
		}
		if ((j < MAX_REGION_CHANNEL_NUM) && region_chan_old[j].valid)
			wlan_cfp_copy_dynamic(pmadapter, cfp, cfp_no,
					      region_chan_old[j].pcfp,
					      region_chan_old[j].num_cfp);
		else
			wlan_cfp_copy_dynamic(pmadapter, cfp, cfp_no, MNULL, 0);
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Get if radar detection is enabled or not on a certain channel
 *
 *  @param priv    Private driver information structure
 *  @param chnl Channel to determine radar detection requirements
 *
 *  @return
 *    - MTRUE if radar detection is required
 *    - MFALSE otherwise
 */
t_bool
wlan_get_cfp_radar_detect(mlan_private * priv, t_u8 chnl)
{
	int i, j;
	t_bool required = MFALSE;
	chan_freq_power_t *pcfp = MNULL;

	ENTER();

	/* get the cfp table first */
	for (i = 0; i < MAX_REGION_CHANNEL_NUM; i++) {
		if (priv->adapter->region_channel[i].band == BAND_A) {
			pcfp = priv->adapter->region_channel[i].pcfp;
			break;
		}
	}

	if (!pcfp) {
		/* This means operation in BAND-A is not support, we can just
		   return false here, it's harmless */
		goto done;
	}

	/* get the radar detection requirements according to chan num */
	for (j = 0; j < priv->adapter->region_channel[i].num_cfp; j++) {
		if (pcfp[j].channel == chnl) {
			required = pcfp[j].passive_scan_or_radar_detect;
			break;
		}
	}

done:
	LEAVE();
	return required;
}

/**
 *  @brief Get if scan type is passive or not on a certain channel for b/g band
 *
 *  @param priv    Private driver information structure
 *  @param chnl Channel to determine scan type
 *
 *  @return
 *    - MTRUE if scan type is passive
 *    - MFALSE otherwise
 */

t_bool
wlan_bg_scan_type_is_passive(mlan_private * priv, t_u8 chnl)
{
	int i, j;
	t_bool passive = MFALSE;
	chan_freq_power_t *pcfp = MNULL;

	ENTER();

	/* get the cfp table first */
	for (i = 0; i < MAX_REGION_CHANNEL_NUM; i++) {
		if (priv->adapter->region_channel[i].band & (BAND_B | BAND_G)) {
			pcfp = priv->adapter->region_channel[i].pcfp;
			break;
		}
	}

	if (!pcfp) {
		/* This means operation in BAND-B or BAND_G is not support, we
		   can just return false here */
		goto done;
	}

	/* get the bg scan type according to chan num */
	for (j = 0; j < priv->adapter->region_channel[i].num_cfp; j++) {
		if (pcfp[j].channel == chnl) {
			passive = pcfp[j].passive_scan_or_radar_detect;
			break;
		}
	}

done:
	LEAVE();
	return passive;
}

/**
 *  @brief Get if a channel is blacklisted or not
 *
 *  @param priv     Private driver information structure
 *  @param band     Band to check
 *  @param chan     Channel to check
 *
 *  @return
 *    - MTRUE if channel is blacklisted
 *    - MFALSE otherwise
 */

t_bool
wlan_is_chan_blacklisted(mlan_private * priv, t_u8 band, t_u8 chan)
{
	int i, j;
	t_bool blacklist = MFALSE;
	chan_freq_power_t *pcfp = MNULL;

	ENTER();

	/* get the cfp table first */
	for (i = 0; i < MAX_REGION_CHANNEL_NUM; i++) {
		if (priv->adapter->region_channel[i].band & band) {
			pcfp = priv->adapter->region_channel[i].pcfp;
			break;
		}
	}

	if (pcfp) {
		/* check table according to chan num */
		for (j = 0; j < priv->adapter->region_channel[i].num_cfp; j++) {
			if (pcfp[j].channel == chan) {
				blacklist = pcfp[j].dynamic.blacklist;
				break;
			}
		}
	}

	LEAVE();
	return blacklist;
}

/**
 *  @brief Set a channel as blacklisted or not
 *
 *  @param priv     Private driver information structure
 *  @param band     Band to check
 *  @param chan     Channel to check
 *  @param bl       Blacklist if MTRUE
 *
 *  @return
 *    - MTRUE if channel setting is updated
 *    - MFALSE otherwise
 */

t_bool
wlan_set_chan_blacklist(mlan_private * priv, t_u8 band, t_u8 chan, t_bool bl)
{
	int i, j;
	t_bool set_bl = MFALSE;
	chan_freq_power_t *pcfp = MNULL;

	ENTER();

	/* get the cfp table first */
	for (i = 0; i < MAX_REGION_CHANNEL_NUM; i++) {
		if (priv->adapter->region_channel[i].band & band) {
			pcfp = priv->adapter->region_channel[i].pcfp;
			break;
		}
	}

	if (pcfp) {
		/* check table according to chan num */
		for (j = 0; j < priv->adapter->region_channel[i].num_cfp; j++) {
			if (pcfp[j].channel == chan) {
				pcfp[j].dynamic.blacklist = bl;
				set_bl = MTRUE;
				break;
			}
		}
	}

	LEAVE();
	return set_bl;
}
