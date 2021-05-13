/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MD_COOLING_H
#define _MD_COOLING_H

#include <linux/thermal.h>

/*===========================================================
 *  Macro Definitions
 *===========================================================
 */
#define MAX_MD_COOLER_NAME_LEN		(20)

#define MAX_NUM_TX_PWR_STATE		(3)
#define MAX_NUM_SCG_OFF_STATE		(1)

#define MD_COOLING_UNLIMITED_STATE	(0)

#define DEFAULT_THROTTLE_TX_PWR_LV1	(4)
#define DEFAULT_THROTTLE_TX_PWR_LV2	(6)
#define DEFAULT_THROTTLE_TX_PWR_LV3	(8)

#define is_scg_off_enabled(status)	(status == MD_SCG_OFF)
#define is_mutt_enabled(status)		(status >= MD_LV_THROTTLE_ENABLED)
#define is_md_inactive(status)		(status == MD_OFF || status == MD_NO_IMS)
#define is_md_off(status)		(status == MD_OFF)

enum md_cooling_status {
	MD_LV_THROTTLE_DISABLED,
	MD_SCG_OFF,
	MD_LV_THROTTLE_ENABLED,
	MD_IMS_ONLY,
	MD_NO_IMS,
	MD_OFF,
};

enum md_cooling_type {
	MD_COOLING_TYPE_MUTT,
	MD_COOLING_TYPE_TX_PWR,
	MD_COOLING_TYPE_SCG_OFF,

	NR_MD_COOLING_TYPE
};

/*===========================================================
 * TMC message (must be align with MD site!)
 *===========================================================
 */
enum tmc_ctrl_cmd {
	TMC_CTRL_CMD_THROTTLING = 0,
	TMC_CTRL_CMD_CA_CTRL,
	TMC_CTRL_CMD_PA_CTRL,
	TMC_CTRL_CMD_COOLER_LV,
	/* 4~7 are for MD internal use */
	TMC_CTRL_CMD_SCG_OFF = 8,
	TMC_CTRL_CMD_SCG_ON,
	TMC_CTRL_CMD_TX_POWER,
	TMC_CTRL_CMD_DEFAULT,
};

enum tmc_throttle_ctrl {
	TMC_THROTTLE_ENABLE_IMS_ENABLE = 0,
	TMC_THROTTLE_ENABLE_IMS_DISABLE,
	TMC_THROTTLE_DISABLE,
};

enum tmc_ca_ctrl {
	TMC_CA_ON = 0, /* leave thermal control*/
	TMC_CA_OFF,
};

enum tmc_pa_ctrl {
	TMC_PA_ALL_ON = 0, /* leave thermal control*/
	TMC_PA_OFF_1PA,
};

enum tmc_cooler_lv_ctrl {
	TMC_COOLER_LV_ENABLE = 0,
	TMC_COOLER_LV_DISABLE
};

enum tmc_overheated_rat {
	TMC_OVERHEATED_LTE = 0,
	TMC_OVERHEATED_NR,
};

enum tmc_tx_pwr_event {
	TMC_TX_PWR_VOLTAGE_LOW_EVENT = 0,
	TMC_TX_PWR_LOW_BATTERY_EVENT,
	TMC_TX_PWR_OVER_CURRENT_EVENT,
	/* reserved for reduce 2G/3G/4G/C2K max TX power for certain value */
	TMC_TX_PWR_REDUCE_OTHER_MAX_TX_EVENT,
	/* reserved for reduce 5G max TX power for certain value */
	TMC_TX_PWR_REDUCE_NR_MAX_TX_EVENT,
};

#define TMC_THROTTLE_DISABLE_MSG \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROTTLE_DISABLE << 8)
#define TMC_IMS_ENABLE_MSG \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROTTLE_ENABLE_IMS_ENABLE << 8)
#define TMC_IMS_DISABLE_MSG \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROTTLE_ENABLE_IMS_DISABLE << 8)
#define TMC_CA_CTRL_CA_ON_MSG \
	(TMC_CTRL_CMD_CA_CTRL | TMC_CA_ON << 8)
#define TMC_CA_CTRL_CA_OFF_MSG \
	(TMC_CTRL_CMD_CA_CTRL | TMC_CA_OFF << 8)
#define TMC_PA_CTRL_PA_ALL_ON_MSG \
	(TMC_CTRL_CMD_PA_CTRL | TMC_PA_ALL_ON << 8)
#define TMC_PA_CTRL_PA_OFF_1PA_MSG \
	(TMC_CTRL_CMD_PA_CTRL | TMC_PA_OFF_1PA << 8)
#define TMC_COOLER_LV_ENABLE_MSG \
	(TMC_CTRL_CMD_COOLER_LV | TMC_COOLER_LV_ENABLE << 8)
#define TMC_COOLER_LV_DISABLE_MSG \
	(TMC_CTRL_CMD_COOLER_LV | TMC_COOLER_LV_DISABLE << 8)

#define mutt_lv_to_tmc_msg(id, lv)	\
	((TMC_COOLER_LV_ENABLE_MSG | (lv) << 16) | ((id) << 24))
#define duty_ctrl_to_tmc_msg(active, suspend, ims)	\
	((ims)	\
	? ((active << 16) | (suspend << 24) | TMC_IMS_ENABLE_MSG)	\
	: ((1 << 16) | (255 << 24) | TMC_IMS_DISABLE_MSG)	\
	)
#define ca_ctrl_to_tmc_msg(ca_ctrl)	\
	((ca_ctrl) ? TMC_CA_CTRL_CA_OFF_MSG : TMC_CA_CTRL_CA_ON_MSG)
#define pa_ctrl_to_tmc_msg(pa_ctrl)	\
	((pa_ctrl) ? TMC_PA_CTRL_PA_OFF_1PA_MSG : TMC_PA_CTRL_PA_ALL_ON_MSG)
#define scg_off_to_tmc_msg(off)	\
	((off) ? TMC_CTRL_CMD_SCG_OFF : TMC_CTRL_CMD_SCG_ON)
#define reduce_tx_pwr_to_tmc_msg(id, pwr)	\
	(((id) == TMC_OVERHEATED_NR)	\
	? (TMC_CTRL_CMD_TX_POWER |	\
		(TMC_TX_PWR_REDUCE_NR_MAX_TX_EVENT << 16) |	\
		((pwr) << 24)) \
	: (TMC_CTRL_CMD_TX_POWER |	\
		(TMC_TX_PWR_REDUCE_OTHER_MAX_TX_EVENT << 16) |	\
		((pwr) << 24)) \
	)

/*==================================================
 * Type Definitions
 *==================================================
 */
/**
 * struct md_cooling_device - data for MD cooling device
 * @name: naming string for this cooling device
 * @type: type of cooling device with different throttle method
 * @pa_id: hint to MD to know the heat source
 * @target_state: target cooling state which is set in set_cur_state()
 *	callback.
 * @max_state: maximum state supported for this cooling device
 * @cdev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @throttle_tx_power: array of throttle TX power from device tree.
 * @throttle: callback function to handle throttle request
 * @node: list_head to link all md_cooling_device.
 * @dev: device node pointer
 */
struct md_cooling_device {
	char name[MAX_MD_COOLER_NAME_LEN];
	enum md_cooling_type type;
	unsigned int pa_id;
	unsigned long target_state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	unsigned int throttle_tx_power[MAX_NUM_TX_PWR_STATE];
	int (*throttle)(struct md_cooling_device *md_cdev, unsigned long state);
	struct list_head node;
	struct device *dev;
};

/**
 * struct md_cooling_platform_data - MD cooler platform dependent data
 * @state_to_mutt_lv: callback function to transfer cooling state
 *			to mutt LV defined by MD
 * @max_lv: max cooler LV supported by MD
 */
struct md_cooling_platform_data {
	unsigned long (*state_to_mutt_lv)(unsigned long state);
	unsigned long max_lv;
};

/**
 * struct md_cooling_data - data for MD cooling driver
 * @pa_num: number of PA from device tree
 * @mutt_lv: current MUTT state (maximum of all PA's target state)
 * @status: current MD throttle status
 * @pdata: platform data of MD cooling driver
 */
struct md_cooling_data {
	unsigned int pa_num;
	unsigned long mutt_state;
	enum md_cooling_status status;
	const struct md_cooling_platform_data *pdata;
};

extern enum md_cooling_status get_md_cooling_status(void);
extern int send_throttle_msg(unsigned int msg);
extern void update_throttle_power(unsigned int pa_id, unsigned int *pwr);
extern struct md_cooling_device *get_md_cdev(enum md_cooling_type type, unsigned int pa_id);
extern unsigned int get_pa_num(void);
#if IS_ENABLED(CONFIG_DEBUG_FS)
extern int md_cooling_debugfs_init(void);
extern void md_cooling_debugfs_exit(void);
#endif

#endif
