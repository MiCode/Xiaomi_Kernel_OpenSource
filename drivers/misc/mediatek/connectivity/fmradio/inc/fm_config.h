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

#ifndef __FM_CONFIG_H__
#define __FM_CONFIG_H__

#include "fm_typedef.h"
#include "fm_rds.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"
#include "fm_interface.h"
/* band */
#define FM_BAND_UNKNOWN 0
#define FM_BAND_UE      1	/* US/Europe band  87.5MHz ~ 108MHz (DEFAULT) */
#define FM_BAND_JAPAN   2	/* Japan band      76MHz   ~ 90MHz */
#define FM_BAND_JAPANW  3	/* Japan wideband  76MHZ   ~ 108MHz */
#define FM_BAND_SPECIAL 4	/* special   band  between 76MHZ   and  108MHz */
#define FM_BAND_DEFAULT FM_BAND_UE
#define FM_RAIDO_BAND FM_BAND_UE
#define FM_FREQ_MIN  FM_RX_BAND_FREQ_L
#define FM_FREQ_MAX  FM_RX_BAND_FREQ_H

#define FM_UE_FREQ_MIN  8750
#define FM_UE_FREQ_MAX  10800
#define FM_JP_FREQ_MIN  7600
#define FM_JP_FREQ_MAX  10800

#define FM_RX_BAND_FREQ_L     8750	/* FM radio special band low freq(Default 87.5MHz) */
#define FM_RX_BAND_FREQ_H     10800	/* FM radio special band high freq(Default 108.0MHz) */
/* TX */
#define FM_TX_SCAN_HOLE_LOW  9230	/* 92.3MHz~95.4MHz should not show to user */
#define FM_TX_SCAN_HOLE_HIGH 9540	/* 92.3MHz~95.4MHz should not show to user */

/* space */
#define FM_SPACE_UNKNOWN    0
#define FM_SPACE_100K       1
#define FM_SPACE_200K       2
#define FM_SPACE_50K        5

#ifdef CONFIG_MTK_FM_50KHZ_SUPPORT
#define FM_SPACE_DEFAULT    FM_SPACE_50K
#else
#define FM_SPACE_DEFAULT    FM_SPACE_100K
#endif

#define FM_TX_SCAN_UP	(0)
#define FM_TX_SCAN_DOWN	(1)
#define FM_TX_SCAN_MAX 10
#define FM_TX_SCAN_MIN 1

/* seek direction */
#define FM_SEEK_UP          0
#define FM_SEEK_DOWN        1

/* ***************************************************************************************** */
/* ***************************FM default config for customer: start************************* */
/* ***************************************************************************************** */

/* RX */
#define FM_RX_RSSI_TH_LONG		-296	/* FM radio long antenna RSSI threshold(-4dBuV) */
#define FM_RX_RSSI_TH_SHORT		-296	/* FM radio short antenna RSSI threshold(-4dBuV) */
#define FM_RX_DESENSE_RSSI		-240
#define FM_RX_PAMD_TH			-12
#define FM_RX_MR_TH			-67
#define FM_RX_ATDC_TH			3496
#define FM_RX_PRX_TH			64
#define FM_RX_SMG_TH			16421	/* FM soft-mute gain threshold */
#define FM_RX_DEEMPHASIS		0	/* 0-50us, China Mainland; 1-75us China Taiwan */
#define FM_RX_OSC_FREQ			0	/* 0-26MHz; 1-19MHz; 2-24MHz; 3-38.4MHz; 4-40MHz; 5-52MHz */
#define FM_AUTO_HILO_OFF		0
#define FM_AUTO_HILO_ON			1

/* seek threshold */
#define FM_SEEKTH_LEVEL_DEFAULT	4

/* TX threshold*/
/* #define FM_TX_PWR_LEVEL_MAX	120 */
/* #define FM_TX_SCAN_HOLE_LOW	923         //92.3MHz~95.4MHz should not show to user */
/* #define FM_TX_SCAN_HOLE_HIGH	954         //92.3MHz~95.4MHz should not show to user */
#define FM_TX_PAMD_TH			-23
#define FM_TX_MR_TH			60
#define FM_TX_SMG_TH			8231

/* ***************************************************************************************** */
/* ***************************FM default config for customer: end*************************** */
/* ***************************************************************************************** */

enum fm_cfg_parser_state {
	FM_CFG_STAT_NONE = 0,
	FM_CFG_STAT_GROUP,
	FM_CFG_STAT_KEY,
	FM_CFG_STAT_VALUE,
	FM_CFG_STAT_COMMENT
};

typedef enum fm_cfg_parser_state fm_cfg_parser_state_t;

#define COMMENT_CHAR '#'
#define DELIMIT_CHAR '='

#define isspace(a) ((a) == 0x20)

struct fm_rx_cust_cfg {
	fm_s32 desene_rssi_th;
	fm_s32 pamd_th;
	fm_s32 mr_th;
	fm_s32 atdc_th;
	fm_s32 prx_th;
	fm_s32 atdev_th;
	fm_s32 short_ana_rssi_th;
	fm_s32 long_ana_rssi_th;
	fm_s32 cqi_th;
	fm_s32 smg_th;
	fm_s32 deemphasis;
	fm_s32 osc_freq;
};

struct fm_tx_cust_cfg {
	fm_s32 scan_hole_low;
	fm_s32 scan_hole_high;
	fm_s32 power_level;
	fm_s32 pamd_th;
	fm_s32 mr_th;
	fm_s32 smg_th;
};
typedef struct {
	struct fm_rx_cust_cfg rx_cfg;
	struct fm_tx_cust_cfg tx_cfg;
	fm_audio_info_t aud_cfg;
} fm_cust_cfg;

enum fm_cust_cfg_op {
	FM_CFG_RX_RSSI_TH_LONG = 0,
	FM_CFG_RX_RSSI_TH_SHORT,
	FM_CFG_RX_CQI_TH,
	FM_CFG_RX_MR_TH,
	FM_CFG_RX_SMG_TH,
	FM_CFG_RX_DEEMPHASIS,
	FM_CFG_RX_OSC_FREQ,
	FM_CFG_RX_DESENSE_RSSI_TH,
	FM_CFG_RX_PAMD_TH,
	FM_CFG_RX_ATDC_TH,
	FM_CFG_RX_PRX_TH,
	FM_CFG_RX_ATDEV_TH,

	FM_CFG_TX_SCAN_HOLE_LOW,
	FM_CFG_TX_SCAN_HOLE_HIGH,
	FM_CFG_TX_PWR_LEVEL,
	FM_CFG_TX_PAMD_TH,
	FM_CFG_TX_DEEMPHASIS,
	FM_CFG_TX_SMG_TH,

	FM_CFG_MAX
};

enum fm_cfg_chip_type {
	FM_COMBO_CHIP = 0,
	FM_AD_DIE_CHIP,
	FM_SOC_CHIP,
	FM_CHIP_TYPE_MAX
};

typedef fm_s32(*CFG_HANDLER) (fm_s8 *grp, fm_s8 *key, fm_s8 *val, fm_cust_cfg *cfg);
extern fm_s32 to_upper_n(fm_s8 *str, fm_s32 len);
extern fm_s32 check_hex_str(fm_s8 *str, fm_s32 len);
extern fm_s32 check_dec_str(fm_s8 *str, fm_s32 len);
extern fm_s32 ascii_to_hex(fm_s8 *in_ascii, fm_u16 *out_hex);
extern fm_s32 ascii_to_dec(fm_s8 *in_ascii, fm_s32 *out_dec);
extern fm_s32 trim_string(fm_s8 **start);
extern fm_s32 trim_path(fm_s8 **start);
extern fm_s32 cfg_parser(fm_s8 *buffer, CFG_HANDLER handler, fm_cust_cfg *cfg);
extern fm_s32 cfg_item_match(fm_s8 *src_key, fm_s8 *src_val, fm_s8 *dst_key, fm_s32 *dst_val);

extern fm_s32 fm_cust_config_setup(const fm_s8 *filename);
extern fm_u16 fm_cust_config_fetch(enum fm_cust_cfg_op op_code);
extern fm_u16 fm_cust_config_chip(fm_u16 chipid, enum fm_cfg_chip_type type);

extern fm_cust_cfg fm_config;
#endif /* __FM_CONFIG_H__ */
