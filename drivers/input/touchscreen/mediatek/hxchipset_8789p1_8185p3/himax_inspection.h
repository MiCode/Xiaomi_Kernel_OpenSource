/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for inspection functions
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"

/*#define HX_GAP_TEST*/
/*#define HX_INSP_LP_TEST*/
/*#define HX_ACT_IDLE_TEST*/

/*#define HX_INSPT_DBG*/

#define HX_RSLT_OUT_PATH "/sdcard/"
#define HX_RSLT_OUT_FILE "hx_test_result.txt"
#define PCT(x...) pr_info(x)
#define HX_SZ_ICID 60

#if defined(HX_EXCP_RECOVERY)
extern u8 HX_EXCP_RESET_ACTIVATE;
#endif

#define BS_RAWDATA     10
#define BS_NOISE       10
#define BS_OPENSHORT   0
#define	BS_LPWUG       1
#define	BS_LP_dile  1
#define	BS_ACT_IDLE    1

/* skip notch & dummy */
#define SKIP_NOTCH_START    5
#define SKIP_NOTCH_END      10
/* TX+SKIP_NOTCH_START */
#define SKIP_DUMMY_START    23
/* TX+SKIP_NOTCH_END*/
#define SKIP_DUMMY_END      28


#define	NOISEFRAME                      (BS_NOISE+1)
#define NORMAL_IDLE_RAWDATA_NOISEFRAME  10
#define LP_RAWDATAFRAME              1
#define LP_NOISEFRAME                1
#define LP_IDLE_RAWDATAFRAME         1
#define LP_IDLE_NOISEFRAME           1

#define OTHERSFRAME		2

#define	UNIFMAX			500


/*Himax MP Password*/
#define	PWD_OPEN_START          0x77
#define	PWD_OPEN_END            0x88
#define	PWD_SHORT_START         0x11
#define	PWD_SHORT_END           0x33
#define	PWD_RAWDATA_START       0x00
#define	PWD_RAWDATA_END         0x99
#define	PWD_NOISE_START         0x00
#define	PWD_NOISE_END           0x99
#define	PWD_SORTING_START       0xAA
#define	PWD_SORTING_END         0xCC


#define PWD_ACT_IDLE_START      0x22
#define PWD_ACT_IDLE_END        0x44



#define PWD_LP_START         0x55
#define PWD_LP_END           0x66

#define PWD_LP_IDLE_START    0x50
#define PWD_LP_IDLE_END      0x60

#define PWD_TURN_ON_MPAP_OVL    0x107380

/*Himax DataType*/
#define DATA_SORTING            0x0A
#define DATA_OPEN               0x0B
#define DATA_MICRO_OPEN         0x0C
#define DATA_SHORT              0x0A
#define DATA_RAWDATA            0x0A
#define DATA_NOISE              0x0F
#define DATA_BACK_NORMAL        0x00
#define DATA_LP_RAWDATA      0x0C
#define DATA_LP_NOISE        0x0F
#define DATA_ACT_IDLE_RAWDATA   0x0A
#define DATA_ACT_IDLE_NOISE     0x0F
#define DATA_LP_IDLE_RAWDATA 0x0A
#define DATA_LP_IDLE_NOISE   0x0F

/*Himax Data Ready Password*/
#define	Data_PWD0       0xA5
#define	Data_PWD1       0x5A

/* ASCII format */
#define ASCII_LF        (0x0A)
#define ASCII_CR        (0x0D)
#define ASCII_COMMA     (0x2C)
#define ASCII_ZERO      (0x30)
#define CHAR_EL         '\0'
#define CHAR_NL         '\n'
#define ACSII_SPACE     (0x20)
/* INSOECTION Setting */

void himax_inspection_init(void);
extern int *g_test_item_flag;
extern int HX_CRITERIA_ITEM;
extern int *g_test_item_flag;
extern char *g_himax_inspection_mode[];

/*Inspection register*/
#define addr_normal_noise_thx   0x1000708C
#define addr_lpwug_noise_thx    0x10007090
#define addr_recal_thx          0x10007090
#define addr_palm_num           0x100070A8
#define addr_weight_sup         0x100072C8
#define addr_normal_weight_a    0x1000709C
#define addr_lpwug_weight_a     0x100070A0
#define addr_weight_b           0x10007094
#define addr_max_dc             0x10007FC8
#define addr_skip_frame         0x100070F4
#define addr_neg_noise_sup      0x10007FD8
#define data_neg_noise          0x7F0C0000
#define addr_ctrl_mpap_ovl      0x100073EC

/*Need to map *g_himax_inspection_mode[]*/
enum THP_INSPECTION_ENUM {
	HX_OPEN,
	HX_MICRO_OPEN,
	HX_SHORT,
	HX_SC,
	HX_WT_NOISE,
	HX_ABS_NOISE,
	HX_RAWDATA,
	HX_BPN_RAWDATA,
	HX_SORTING,

	HX_GAPTEST_RAW,
	/*HX_GAPTEST_RAW_X,*/
	/*HX_GAPTEST_RAW_Y,*/

	HX_ACT_IDLE_NOISE,
	HX_ACT_IDLE_RAWDATA,
	HX_ACT_IDLE_BPN_RAWDATA,
/*LPWUG test must put after Normal test*/
	HX_LP_WT_NOISE,
	HX_LP_ABS_NOISE,
	HX_LP_RAWDATA,
	HX_LP_BPN_RAWDATA,

	HX_LP_IDLE_NOISE,
	HX_LP_IDLE_RAWDATA,
	HX_LP_IDLE_BPN_RAWDATA,

	HX_BACK_NORMAL,/*Must put in the end*/
};


enum HX_CRITERIA_ENUM {
	IDX_RAWMIN = 0,
	IDX_RAWMAX,
	IDX_BPN_RAWMIN,
	IDX_BPN_RAWMAX,
	IDX_SCMIN,
	IDX_SCMAX,
	IDX_SC_GOLDEN,
	IDX_SHORTMIN,
	IDX_SHORTMAX,
	IDX_OPENMIN,
	IDX_OPENMAX,
	IDX_M_OPENMIN,
	IDX_M_OPENMAX,
	IDX_WT_NOISEMIN,
	IDX_WT_NOISEMAX,
	IDX_ABS_NOISEMIN,
	IDX_ABS_NOISEMAX,
	IDX_SORTMIN,
	IDX_SORTMAX,

	IDX_GAP_HOR_RAWMAX,
	IDX_GAP_HOR_RAWMIN,
	IDX_GAP_VER_RAWMAX,
	IDX_GAP_VER_RAWMIN,

	IDX_ACT_IDLE_NOISE_MIN,
	IDX_ACT_IDLE_NOISE_MAX,
	IDX_ACT_IDLE_RAWDATA_MIN,
	IDX_ACT_IDLE_RAWDATA_MAX,
	IDX_ACT_IDLE_RAW_BPN_MIN,
	IDX_ACT_IDLE_RAW_BPN_MAX,

	IDX_LP_WT_NOISEMIN,
	IDX_LP_WT_NOISEMAX,
	IDX_LP_NOISE_ABS_MIN,
	IDX_LP_NOISE_ABS_MAX,
	IDX_LP_RAWDATA_MIN,
	IDX_LP_RAWDATA_MAX,
	IDX_LP_RAW_BPN_MIN,
	IDX_LP_RAW_BPN_MAX,

	IDX_LP_IDLE_NOISE_MIN,
	IDX_LP_IDLE_NOISE_MAX,
	IDX_LP_IDLE_RAWDATA_MIN,
	IDX_LP_IDLE_RAWDATA_MAX,
	IDX_LP_IDLE_RAW_BPN_MIN,
	IDX_LP_IDLE_RAW_BPN_MAX,
};

#define ERR_SFT 4
/* Error code of Inspection */
enum HX_INSP_ERR_ENUM {
	/* OK */
	HX_INSP_OK	= 0,

	/* Criteria file error*/
	HX_INSP_EFILE = 1,

	/* Get raw data errors */
	HX_INSP_EGETRAW = 1 << 1,

	/* Memory allocate errors */
	HX_INSP_MEMALLCTFAIL = 1 << 2,

	/* Switch mode error*/
	HX_INSP_ESWITCHMODE = 1 << 3,

	/* Sensor open error */
	HX_EOPEN = 1 << (HX_OPEN + ERR_SFT),

	/* Sensor micro open error */
	HX_EMOPEN = 1 << (HX_MICRO_OPEN + ERR_SFT),

	/* Sensor short error */
	HX_ESHORT = 1 << (HX_SHORT + ERR_SFT),

	/* Raw data error */
	HX_ERAW = 1 << (HX_RAWDATA + ERR_SFT),

	/* Raw data BPN error */
	HX_EBPNRAW = 1 << (HX_BPN_RAWDATA + ERR_SFT),

	/* Get SC errors */
	HX_ESC = 1 << (HX_SC + ERR_SFT),

	/* Noise error */
	HX_WT_ENOISE = 1 << (HX_WT_NOISE + ERR_SFT),

	/* Noise error */
	HX_ABS_ENOISE = 1 << (HX_ABS_NOISE + ERR_SFT),

	/* Sorting error*/
	HX_ESORT = 1 << (HX_SORTING + ERR_SFT),

	/* Raw Data GAP  */
	HX_EGAP_RAW = 1 << (HX_GAPTEST_RAW + ERR_SFT),

	/* ACT_IDLE RAW ERROR */
	HX_EACT_IDLE_RAW = 1 << (HX_ACT_IDLE_RAWDATA + ERR_SFT),

	/* ACT_IDLE NOISE ERROR */
	HX_EACT_IDLE_NOISE = 1 << (HX_ACT_IDLE_NOISE + ERR_SFT),

	/* LPWUG RAW ERROR */
	HX_ELP_RAW = 1 << (HX_LP_RAWDATA + ERR_SFT),

	/* LPWUG NOISE ERROR */
	HX_ELP_WT_NOISE = 1 << (HX_LP_WT_NOISE + ERR_SFT),

	/* LPWUG NOISE ERROR */
	HX_ELP_ABS_NOISE = 1 << (HX_LP_ABS_NOISE + ERR_SFT),

	/* LPWUG IDLE RAW ERROR */
	HX_ELP_IDLE_RAW = 1 << (HX_LP_IDLE_RAWDATA + ERR_SFT),

	/* LPWUG IDLE NOISE ERROR */
	HX_ELP_IDLE_NOISE = 1 << (HX_LP_IDLE_NOISE + ERR_SFT),
	HX_EACT_IDLE_BPNRAW = 1 << (HX_ACT_IDLE_BPN_RAWDATA + ERR_SFT),
	HX_ELP_BPNRAW = 1 << (HX_LP_BPN_RAWDATA + ERR_SFT),
	HX_ELP_IDLE_BPNRAW = 1 << (HX_LP_IDLE_BPN_RAWDATA + ERR_SFT),
};

extern void himax_inspect_data_clear(void);
