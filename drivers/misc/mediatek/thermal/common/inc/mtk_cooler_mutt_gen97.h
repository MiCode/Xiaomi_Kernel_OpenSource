/*
 * Copyright (C) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MTK_COOLER_MUTT_GEN97_H
#define _MTK_COOLER_MUTT_GEN97_H

#define MAX_NUM_INSTANCE_MTK_COOLER_MUTT  8

#define TMC_CA_CTRL_CA_ON \
	(TMC_CTRL_CMD_CA_CTRL | TMC_CA_ON << 8)
#define TMC_CA_CTRL_CA_OFF \
	(TMC_CTRL_CMD_CA_CTRL | TMC_CA_OFF << 8)
#define TMC_PA_CTRL_PA_ALL_ON \
	(TMC_CTRL_CMD_PA_CTRL | TMC_PA_ALL_ON << 8)
#define TMC_PA_CTRL_PA_OFF_1PA \
	(TMC_CTRL_CMD_PA_CTRL | TMC_PA_OFF_1PA << 8)
#define TMC_THROTTLING_THROT_DISABLE \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROT_DISABLE << 8)
#define MUTT_THROTTLING_IMS_ENABLE \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROT_ENABLE_IMS_ENABLE << 8)
#define MUTT_THROTTLING_IMS_DISABLE \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROT_ENABLE_IMS_DISABLE << 8)
#define MUTT_TMC_COOLER_LV_ENABLE \
	(TMC_CTRL_CMD_COOLER_LV | TMC_COOLER_LV_ENABLE << 8)
#define MUTT_TMC_COOLER_LV_DISABLE \
	(TMC_CTRL_CMD_COOLER_LV | TMC_COOLER_LV_DISABLE << 8)
#define TMC_COOLER_LV_CTRL00 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV0 << 16)
#define TMC_COOLER_LV_CTRL01 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV1 << 16)
#define TMC_COOLER_LV_CTRL02 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV2 << 16)
#define TMC_COOLER_LV_CTRL03 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV3 << 16)
#define TMC_COOLER_LV_CTRL04 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV4 << 16)
#define TMC_COOLER_LV_CTRL05 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV5 << 16)
#define TMC_COOLER_LV_CTRL06 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV6 << 16)
#define TMC_COOLER_LV_CTRL07 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV7 << 16)
#define TMC_COOLER_LV_CTRL08 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV8 << 16)
#define TMC_COOLER_LV_RAT_LTE	(TMC_OVERHEATED_LTE << 24)
#define TMC_COOLER_LV_RAT_NR	(TMC_OVERHEATED_NR << 24)
#define TMC_REDUCE_OTHER_MAX_TX_POWER(pwr) \
	(TMC_CTRL_CMD_TX_POWER \
	| (TMC_TW_PWR_REDUCE_OTHER_MAX_TX_EVENT << 16)	\
	| (pwr << 24))
#define TMC_REDUCE_NR_MAX_TX_POWER(pwr) \
	(TMC_CTRL_CMD_TX_POWER \
	| (TMC_TW_PWR_REDUCE_NR_MAX_TX_EVENT << 16)	\
	| (pwr << 24))

enum mutt_type {
	MUTT_LTE,
	MUTT_NR,

	NR_MUTT_TYPE,
};

/*enum mapping must be align with MD site*/
enum tmc_ctrl_cmd_enum {
	TMC_CTRL_CMD_THROTTLING = 0,
	TMC_CTRL_CMD_CA_CTRL,
	TMC_CTRL_CMD_PA_CTRL,
	TMC_CTRL_CMD_COOLER_LV,
	/* MD internal use start */
	TMC_CTRL_CMD_CELL,            /* refer as del_cell */
	TMC_CTRL_CMD_BAND,            /* refer as del_band */
	TMC_CTRL_CMD_INTER_BAND_OFF,  /* similar to PA_OFF on Gen95 */
	TMC_CTRL_CMD_CA_OFF,          /* similar to CA_OFF on Gen95 */
	/* MD internal use end */
	TMC_CTRL_CMD_SCG_OFF,         /* Fall back to 4G */
	TMC_CTRL_CMD_SCG_ON,          /* Enabled 5G */
	TMC_CTRL_CMD_TX_POWER,
	TMC_CTRL_CMD_DEFAULT,
};

enum tmc_throt_ctrl_enum {
	TMC_THROT_ENABLE_IMS_ENABLE = 0,
	TMC_THROT_ENABLE_IMS_DISABLE,
	TMC_THROT_DISABLE,
};

enum tmc_ca_ctrl_enum {
	TMC_CA_ON = 0, /* leave thermal control*/
	TMC_CA_OFF,
};

enum tmc_pa_ctrl_enum {
	TMC_PA_ALL_ON = 0, /* leave thermal control*/
	TMC_PA_OFF_1PA,
};

enum tmc_cooler_lv_ctrl_enum {
	TMC_COOLER_LV_ENABLE = 0,
	TMC_COOLER_LV_DISABLE
};

enum tmc_cooler_lv_enum {
	TMC_COOLER_LV0 = 0,
	TMC_COOLER_LV1,
	TMC_COOLER_LV2,
	TMC_COOLER_LV3,
	TMC_COOLER_LV4,
	TMC_COOLER_LV5,
	TMC_COOLER_LV6,
	TMC_COOLER_LV7,
	TMC_COOLER_LV8,
};

enum tmc_overheated_rat_enum {
	TMC_OVERHEATED_LTE = 0,
	TMC_OVERHEATED_NR,
};

enum tmc_tx_pwr_event_enum {
	TMC_TW_PWR_VOLTAGE_LOW_EVENT = 0,
	TMC_TW_PWR_LOW_BATTERY_EVENT,
	TMC_TW_PWR_OVER_CURRENT_EVENT,
	/* reserved for reduce 2G/3G/4G/C2K max TX power for certain value */
	TMC_TW_PWR_REDUCE_OTHER_MAX_TX_EVENT,
	/* reserved for reduce 5G max TX power for certain value */
	TMC_TW_PWR_REDUCE_NR_MAX_TX_EVENT,
	TMC_TW_PWR_EVENT_MAX_NUM,
};

extern unsigned int clmutt_level_selection(int lv, unsigned int type);

#endif

