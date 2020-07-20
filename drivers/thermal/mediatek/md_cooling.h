/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MD_COOLING_H
#define _MD_COOLING_H

#include <linux/thermal.h>
#include <linux/types.h>

/*===========================================================
 *  Macro Definitions
 *===========================================================
 */
#define MAX_MD_COOLER_NAME_LEN	(20)
#define MAX_NUM_TX_PWR_LV	(3)
#define MD_COOLING_UNLIMITED_LV	(0)

#define is_mutt_enabled(status)	(status >= MD_LV_THROTTLE_ENABLED)
#define is_md_inactive(status)	(status == MD_OFF || status == MD_NO_IMS)
#define is_md_off(status)	(status == MD_OFF)


enum md_status {
	MD_LV_THROTTLE_DISABLED,
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
 * @target_level: target cooling level which is set in set_cur_state()
 *	callback.
 * @max_level: maximum level supported for this cooling device
 * @cdev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @throttle_tx_power: array of throttle TX power from device tree.
 * @node: list_head to link all md_cooling_device.
 * @dev_data: device private data
 */
struct md_cooling_device {
	char name[MAX_MD_COOLER_NAME_LEN];
	enum md_cooling_type type;
	unsigned int pa_id;
	unsigned long target_level;
	unsigned long max_level;
	struct thermal_cooling_device *cdev;
	unsigned int throttle_tx_power[MAX_NUM_TX_PWR_LV];
	struct list_head node;
	void *dev_data;
};

enum md_status get_md_status(void);
void set_md_status(enum md_status status);
int send_throttle_msg(unsigned int msg);
void update_throttle_power(unsigned int pa_id, unsigned int *pwr);
struct md_cooling_device*
get_md_cdev(enum md_cooling_type type, unsigned int pa_id);
unsigned int get_pa_num(void);
int md_cooling_register(struct device_node *np, enum md_cooling_type type,
		unsigned long max_level, unsigned int *throttle_pwr,
		struct thermal_cooling_device_ops *cooling_ops, void *data);
void md_cooling_unregister(enum md_cooling_type type);

#endif
