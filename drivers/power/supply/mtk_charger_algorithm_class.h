/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#ifndef __MTK_CHARGER_ALGORITHM_CLASS_H__
#define __MTK_CHARGER_ALGORITHM_CLASS_H__

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>

#define PE_ID    1 << 0
#define PE2_ID   1 << 1
#define PDC_ID   1 << 2
#define PE4_ID   1 << 3
#define PE5_ID   1 << 4

struct chg_alg_properties {
	const char *alias_name;
};

/*
 * ALG_INIT_FAIL: hw init fail
 * ALG_TA_NOT_SUPPORT: TA does not support
 * ALG_TA_CHECKING: checking TA
 * ALG_NOT_READY: TA support & not meet the conditions
 * ALG_READY: TA support & meet the conditions
 * ALG_RUNNING: alg is running
 * ALG_DONE: alg done
 */
enum chg_alg_state {
	ALG_INIT_FAIL,
	ALG_TA_CHECKING,
	ALG_TA_NOT_SUPPORT,
	ALG_NOT_READY,
	ALG_READY,
	ALG_RUNNING,
	ALG_DONE,
};

enum chg_idx {
	CHG1,
	CHG2,
	DVCHG1,
	DVCHG2,
	CHG_MAX,
};

enum charger_configuration {
	SINGLE_CHARGER,
	DUAL_CHARGERS_IN_SERIES,
	DUAL_CHARGERS_IN_PARALLEL,
	DIVIDER_CHARGER,
	DUAL_DIVIDER_CHARGERS,
};

struct chg_alg_device {
	struct chg_alg_properties props;
	const struct chg_alg_ops *ops;
	enum charger_configuration config;
	struct mutex ops_lock;
	struct device dev;
	struct srcu_notifier_head evt_nh;
	void	*driver_data;
	void	*driver_hal_data;
	bool is_polling_mode;
	int alg_id;
};

enum chg_alg_notifier_events {
	EVT_PLUG_IN,
	EVT_PLUG_OUT,
	EVT_FULL,
	EVT_RECHARGE,
	EVT_DETACH,
	EVT_HARDRESET,
	EVT_VBUSOVP,
	EVT_IBUSOCP,
	EVT_IBUSUCP_FALL,
	EVT_VBATOVP,
	EVT_IBATOCP,
	EVT_VOUTOVP,
	EVT_VDROVP,
	EVT_VBATOVP_ALARM,
	EVT_VBUSOVP_ALARM,
	EVT_BATPRO_DONE,
	EVT_ALGO_STOP,
	EVT_MAX,
};

struct chg_alg_notify {
	enum chg_alg_notifier_events evt;
	int value;
};

struct chg_limit_setting {
	int cv;
	int input_current_limit1;
	int input_current_limit2;
	int input_current_limit_dvchg1;
	int charging_current_limit1;
	int charging_current_limit2;
	bool vbat_mon_en;
};

enum chg_alg_props {
	ALG_MAX_VBUS,
	ALG_LOG_LEVEL,
	ALG_REF_VBAT,
};

struct chg_alg_ops {
	int (*init_algo)(struct chg_alg_device *alg);
	int (*is_algo_ready)(struct chg_alg_device *alg);
	int (*start_algo)(struct chg_alg_device *alg);
	bool (*is_algo_running)(struct chg_alg_device *alg);
	int (*plugout_reset)(struct chg_alg_device *alg);
	int (*stop_algo)(struct chg_alg_device *alg);
	int (*notifier_call)(struct chg_alg_device *alg,
		struct chg_alg_notify *notify);
	int (*get_prop)(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value);
	int (*set_prop)(struct chg_alg_device *alg,
		enum chg_alg_props s, int value);
	int (*set_current_limit)(struct chg_alg_device *alg_dev,
		struct chg_limit_setting *setting);

};

#define to_chg_alg_dev(obj) container_of(obj, struct chg_alg_device, dev)

static inline void *chg_alg_dev_get_drvdata(
	const struct chg_alg_device *chg_alg)
{
	return chg_alg->driver_data;
}

static inline void chg_alg_dev_set_drvdata(
	struct chg_alg_device *chg_alg, void *data)
{
	chg_alg->driver_data = data;
}

static inline void *chg_alg_dev_get_drv_hal_data(
	const struct chg_alg_device *chg_alg)
{
	return chg_alg->driver_hal_data;
}

static inline void chg_alg_dev_set_drv_hal_data(
	struct chg_alg_device *chg_alg, void *data)
{
	chg_alg->driver_hal_data = data;
}

extern struct chg_alg_device *get_chg_alg_by_name(
	const char *name);
extern struct chg_alg_device *chg_alg_device_register(
	const char *name, struct device *parent,
	void *devdata, const struct chg_alg_ops *ops,
	const struct chg_alg_properties *props);
extern void chg_alg_device_unregister(
	struct chg_alg_device *charger_dev);
extern int register_chg_alg_notifier(struct chg_alg_device *alg_dev,
				struct notifier_block *nb);
extern int unregister_chg_alg_notifier(struct chg_alg_device *alg_dev,
				struct notifier_block *nb);

extern int chg_alg_init_algo(struct chg_alg_device *alg_dev);
extern int chg_alg_is_algo_ready(struct chg_alg_device *alg_dev);
extern int chg_alg_start_algo(struct chg_alg_device *alg_dev);
extern int chg_alg_is_algo_running(struct chg_alg_device *alg_dev);
extern int chg_alg_stop_algo(struct chg_alg_device *alg_dev);
extern int chg_alg_get_prop(struct chg_alg_device *alg_dev,
	enum chg_alg_props s, int *value);
extern int chg_alg_set_prop(struct chg_alg_device *alg_dev,
	enum chg_alg_props s, int value);
extern int chg_alg_set_current_limit(struct chg_alg_device *alg_dev,
	struct chg_limit_setting *setting);
extern int chg_alg_notifier_call(struct chg_alg_device *alg_dev,
	struct chg_alg_notify *notify);
extern char *chg_alg_state_to_str(int state);
extern const char *const
chg_alg_notify_evt_tostring(enum chg_alg_notifier_events evt);
#endif /* __MTK_CHARGER_ALGORITHM_CLASS_H__ */
