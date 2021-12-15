/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __LINUX_PROP_CHGALGO_CLASS_H
#define __LINUX_PROP_CHGALGO_CLASS_H

#include <linux/device.h>
#include <linux/notifier.h>

#define PCA_DBG_EN	1
#define PCA_INFO_EN	1
#define PCA_ERR_EN	1

#define PCA_DBG(fmt, ...) \
	do { \
		if (PCA_DBG_EN) \
			pr_info("[PCA]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define PCA_INFO(fmt, ...) \
	do { \
		if (PCA_INFO_EN) \
			pr_info("[PCA]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define PCA_ERR(fmt, ...) \
	do { \
		if (PCA_ERR_EN) \
			pr_info("[PCA]%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define SIMPLE_PCA_TA_DESC(_name, ops) \
const struct prop_chgalgo_desc _name##_desc = { \
	.name = #_name, \
	.type = PCA_DEVTYPE_TA, \
	.ta_ops = &ops, \
	.chg_ops = NULL, \
	.algo_ops = NULL, \
}

#define SIMPLE_PCA_CHG_DESC(_name, ops) \
const struct prop_chgalgo_desc _name##_desc = { \
	.name = #_name, \
	.type = PCA_DEVTYPE_CHARGER, \
	.ta_ops = NULL, \
	.chg_ops = &ops, \
	.algo_ops = NULL, \
}

#define SIMPLE_PCA_ALGO_DESC(_name, ops) \
const struct prop_chgalgo_desc _name##_desc = { \
	.name = #_name, \
	.type = PCA_DEVTYPE_ALGO, \
	.ta_ops = NULL, \
	.chg_ops = NULL, \
	.algo_ops = &ops, \
}

struct prop_chgalgo_ta_status {
	int temp1;
	int temp2;
	u8 temp_level;
	u8 present_input;
	u8 present_battery_input;
	bool ocp;
	bool otp;
	bool ovp;
};

struct prop_chgalgo_ta_auth_data {
	int vcap_min;
	int vcap_max;
	int icap_min;
	int vta_min;
	int vta_max;
	int ita_max;
	int ita_min;
	bool pwr_lmt;
	u8 pdp;
	bool support_meas_cap;
	bool support_status;
	bool support_cc;
	u32 vta_step;
	u32 ita_step;
	u32 ita_gap_per_vstep;
};

enum prop_chgalgo_notify_source {
	PCA_NOTISRC_TCP,
	PCA_NOTISRC_CHG,
	PCA_NOTISRC_ALGO,
	PCA_NOTISRC_MAX,
};

enum prop_chgalgo_notify_evt {
	PCA_NOTIEVT_DETACH,
	PCA_NOTIEVT_HARDRESET,
	PCA_NOTIEVT_VBUSOVP,
	PCA_NOTIEVT_IBUSOCP,
	PCA_NOTIEVT_IBUSUCP_FALL,
	PCA_NOTIEVT_VBATOVP,
	PCA_NOTIEVT_IBATOCP,
	PCA_NOTIEVT_VOUTOVP,
	PCA_NOTIEVT_VDROVP,
	PCA_NOTIEVT_VBATOVP_ALARM,
	PCA_NOTIEVT_VBUSOVP_ALARM,
	PCA_NOTIEVT_ALGO_STOP,
	PCA_NOTIEVT_MAX,
};

struct prop_chgalgo_notify {
	enum prop_chgalgo_notify_source src;
	enum prop_chgalgo_notify_evt evt;
};

enum prop_chgalgo_adc_channel {
	PCA_ADCCHAN_VBUS = 0,
	PCA_ADCCHAN_IBUS,
	PCA_ADCCHAN_VBAT,
	PCA_ADCCHAN_IBAT,
	PCA_ADCCHAN_TBAT,
	PCA_ADCCHAN_TCHG,
	PCA_ADCCHAN_VOUT,
	PCA_ADCCHAN_VSYS,
	PCA_ADCCHAN_MAX,
};

struct prop_chgalgo_device;

struct prop_chgalgo_ta_ops {
	int (*enable_charging)(struct prop_chgalgo_device *pca, bool en, u32 mV,
			       u32 mA);
	int (*set_cap)(struct prop_chgalgo_device *pca, u32 mV, u32 mA);
	int (*get_measure_cap)(struct prop_chgalgo_device *pca, u32 *mV,
			       u32 *mA);
	int (*get_temperature)(struct prop_chgalgo_device *pca, int *degree);
	int (*get_status)(struct prop_chgalgo_device *pca,
			  struct prop_chgalgo_ta_status *status);
	int (*is_cc)(struct prop_chgalgo_device *pca, bool *cc);
	int (*send_hardreset)(struct prop_chgalgo_device *pca);
	int (*authenticate_ta)(struct prop_chgalgo_device *pca,
			       struct prop_chgalgo_ta_auth_data *data);
	int (*enable_wdt)(struct prop_chgalgo_device *pca, bool en);
	int (*set_wdt)(struct prop_chgalgo_device *pca, u32 ms);
	int (*sync_vta)(struct prop_chgalgo_device *pca, u32 vta);
};

struct prop_chgalgo_chg_ops {
	int (*enable_power_path)(struct prop_chgalgo_device *pca, bool en);
	int (*enable_charging)(struct prop_chgalgo_device *pca, bool en);
	int (*enable_chip)(struct prop_chgalgo_device *pca, bool en);
	int (*enable_hz)(struct prop_chgalgo_device *pca, bool en);
	int (*set_vbusovp)(struct prop_chgalgo_device *pca, u32 mV);
	int (*set_ibusocp)(struct prop_chgalgo_device *pca, u32 mA);
	int (*set_vbatovp)(struct prop_chgalgo_device *pca, u32 mV);
	int (*set_ibatocp)(struct prop_chgalgo_device *pca, u32 mA);
	int (*set_vbatovp_alarm)(struct prop_chgalgo_device *pca, u32 mV);
	int (*reset_vbatovp_alarm)(struct prop_chgalgo_device *pca);
	int (*set_vbusovp_alarm)(struct prop_chgalgo_device *pca, u32 mV);
	int (*reset_vbusovp_alarm)(struct prop_chgalgo_device *pca);
	int (*set_aicr)(struct prop_chgalgo_device *pca, u32 mA);
	int (*set_ichg)(struct prop_chgalgo_device *pca, u32 mA);
	int (*get_adc)(struct prop_chgalgo_device *pca,
		       enum prop_chgalgo_adc_channel chan, int *min, int *max);
	int (*get_soc)(struct prop_chgalgo_device *pca, u32 *soc);
	int (*is_vbuslowerr)(struct prop_chgalgo_device *pca, bool *err);
	int (*is_charging_enabled)(struct prop_chgalgo_device *pca, bool *en);
	int (*get_adc_accuracy)(struct prop_chgalgo_device *pca,
				enum prop_chgalgo_adc_channel chan, int *min,
				int *max);
	int (*init_chip)(struct prop_chgalgo_device *pca);
	int (*enable_auto_trans)(struct prop_chgalgo_device *pca, bool en);
	int (*set_auto_trans)(struct prop_chgalgo_device *pca, u32 mV, bool en);
	int (*dump_registers)(struct prop_chgalgo_device *pca);
};

struct prop_chgalgo_algo_ops {
	int (*init_algo)(struct prop_chgalgo_device *pca);
	bool (*is_algo_ready)(struct prop_chgalgo_device *pca);
	int (*start_algo)(struct prop_chgalgo_device *pca);
	bool (*is_algo_running)(struct prop_chgalgo_device *pca);
	int (*plugout_reset)(struct prop_chgalgo_device *pca);
	int (*stop_algo)(struct prop_chgalgo_device *pca, bool rerun);
	int (*thermal_throttling)(struct prop_chgalgo_device *pca, int mA);
	int (*set_jeita_vbat_cv)(struct prop_chgalgo_device *pca, int mV);
	int (*notifier_call)(struct prop_chgalgo_device *pca,
			     struct prop_chgalgo_notify *notify);
};

enum prop_chgalgo_dev_type {
	PCA_DEVTYPE_TA = 0,
	PCA_DEVTYPE_CHARGER,
	PCA_DEVTYPE_ALGO,
	PCA_DEVTYPE_MAX,
};

struct prop_chgalgo_desc {
	const char *name;
	enum prop_chgalgo_dev_type type;
	const struct prop_chgalgo_ta_ops *ta_ops;
	const struct prop_chgalgo_chg_ops *chg_ops;
	const struct prop_chgalgo_algo_ops *algo_ops;
};

struct prop_chgalgo_device {
	struct device dev;
	const struct prop_chgalgo_desc *desc;
	struct srcu_notifier_head nh;
	void *drv_data;
	int (*suspend)(struct prop_chgalgo_device *pca);
	int (*resume)(struct prop_chgalgo_device *pca);
};

extern struct prop_chgalgo_device *
prop_chgalgo_device_register(struct device *parent,
			     const struct prop_chgalgo_desc *desc,
			     void *drv_data);
extern void prop_chgalgo_device_unregister(struct prop_chgalgo_device *pca);
extern struct prop_chgalgo_device *
prop_chgalgo_dev_get_by_name(const char *name);
extern const char *
prop_chgalgo_notify_evt_tostring(enum prop_chgalgo_notify_evt evt);

static inline int prop_chgalgo_get_devtype(struct prop_chgalgo_device *pca)
{
	return pca->desc->type;
}

static inline void *prop_chgalgo_get_drvdata(struct prop_chgalgo_device *pca)
{
	return pca->drv_data;
}

static inline int
prop_chgalgo_notifier_register(struct prop_chgalgo_device *pca,
			       struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&pca->nh, nb);
}

static inline int
prop_chgalgo_notifier_unregister(struct prop_chgalgo_device *pca,
				 struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&pca->nh, nb);
}

/* Richtek pca TA interface */
extern int prop_chgalgo_enable_ta_charging(struct prop_chgalgo_device *pca,
					   bool en, u32 mV, u32 mA);
extern int prop_chgalgo_set_ta_cap(struct prop_chgalgo_device *pca, u32 mV,
				   u32 mA);
extern int prop_chgalgo_get_ta_measure_cap(struct prop_chgalgo_device *pca,
					   u32 *mV, u32 *mA);
extern int prop_chgalgo_get_ta_temperature(struct prop_chgalgo_device *pca,
					   int *degree);
extern int prop_chgalgo_get_ta_status(struct prop_chgalgo_device *pca,
				      struct prop_chgalgo_ta_status *status);
extern int prop_chgalgo_is_ta_cc(struct prop_chgalgo_device *pca, bool *cc);
extern int prop_chgalgo_send_ta_hardreset(struct prop_chgalgo_device *pca);
extern int prop_chgalgo_authenticate_ta(struct prop_chgalgo_device *pca,
					struct prop_chgalgo_ta_auth_data *data);
extern int prop_chgalgo_enable_ta_wdt(struct prop_chgalgo_device *pca, bool en);
extern int prop_chgalgo_set_ta_wdt(struct prop_chgalgo_device *pca, u32 ms);
extern int prop_chgalgo_sync_ta_volt(struct prop_chgalgo_device *pca, u32 vta);

/* Richtek pca charger interface */
extern int prop_chgalgo_enable_power_path(struct prop_chgalgo_device *pca,
					  bool en);
extern int prop_chgalgo_enable_charging(struct prop_chgalgo_device *pca,
					bool en);
extern int prop_chgalgo_enable_chip(struct prop_chgalgo_device *pca, bool en);
extern int prop_chgalgo_enable_hz(struct prop_chgalgo_device *pca, bool en);
extern int prop_chgalgo_set_vbusovp(struct prop_chgalgo_device *pca, u32 mV);
extern int prop_chgalgo_set_ibusocp(struct prop_chgalgo_device *pca, u32 mA);
extern int prop_chgalgo_set_vbatovp(struct prop_chgalgo_device *pca, u32 mV);
extern int prop_chgalgo_set_ibatocp(struct prop_chgalgo_device *pca, u32 mA);
extern int prop_chgalgo_set_vbatovp_alarm(struct prop_chgalgo_device *pca,
					  u32 mV);
extern int prop_chgalgo_reset_vbatovp_alarm(struct prop_chgalgo_device *pca);
extern int prop_chgalgo_set_vbusovp_alarm(struct prop_chgalgo_device *pca,
					  u32 mV);
extern int prop_chgalgo_reset_vbusovp_alarm(struct prop_chgalgo_device *pca);
extern int prop_chgalgo_get_adc(struct prop_chgalgo_device *pca,
				enum prop_chgalgo_adc_channel chan, int *min,
				int *max);
extern int prop_chgalgo_get_soc(struct prop_chgalgo_device *pca, u32 *soc);
extern int prop_chgalgo_set_ichg(struct prop_chgalgo_device *pca, u32 mA);
extern int prop_chgalgo_set_aicr(struct prop_chgalgo_device *pca, u32 mA);
extern int prop_chgalgo_is_vbuslowerr(struct prop_chgalgo_device *pca,
				      bool *err);
extern int prop_chgalgo_is_charging_enabled(struct prop_chgalgo_device *pca,
					    bool *en);
extern int prop_chgalgo_get_adc_accuracy(struct prop_chgalgo_device *pca,
					 enum prop_chgalgo_adc_channel chan,
					 int *min, int *max);
extern int prop_chgalgo_init_chip(struct prop_chgalgo_device *pca);
extern int prop_chgalgo_enable_auto_trans(struct prop_chgalgo_device *pca,
					  bool en);
extern int prop_chgalgo_set_auto_trans(struct prop_chgalgo_device *pca, u32 mV, bool en);
extern int prop_chgalgo_dump_registers(struct prop_chgalgo_device *pca);

/* Richtek pca algorithm interface */
#ifdef CONFIG_RT_PROP_CHGALGO
extern int prop_chgalgo_init_algo(struct prop_chgalgo_device *pca);
extern bool prop_chgalgo_is_algo_ready(struct prop_chgalgo_device *pca);
extern int prop_chgalgo_start_algo(struct prop_chgalgo_device *pca);
extern bool prop_chgalgo_is_algo_running(struct prop_chgalgo_device *pca);
extern int prop_chgalgo_plugout_reset(struct prop_chgalgo_device *pca);
extern int prop_chgalgo_stop_algo(struct prop_chgalgo_device *pca, bool rerun);
extern int prop_chgalgo_notifier_call(struct prop_chgalgo_device *pca,
				      struct prop_chgalgo_notify *notify);
extern int prop_chgalgo_thermal_throttling(struct prop_chgalgo_device *pca,
					   int mA);
extern int prop_chgalgo_set_jeita_vbat_cv(struct prop_chgalgo_device *pca,
					  int mV);
#else
static inline int prop_chgalgo_init_algo(struct prop_chgalgo_device *pca)
{
	return -ENOTSUPP;
}

static inline bool prop_chgalgo_is_algo_ready(struct prop_chgalgo_device *pca)
{
	return false;
}

static inline int prop_chgalgo_start_algo(struct prop_chgalgo_device *pca)
{
	return -ENOTSUPP;
}

static inline bool prop_chgalgo_is_algo_running(struct prop_chgalgo_device *pca)
{
	return false;
}

static inline int prop_chgalgo_plugout_reset(struct prop_chgalgo_device *pca)
{
	return -ENOTSUPP;
}

static inline int prop_chgalgo_stop_algo(struct prop_chgalgo_device *pca,
					 bool rerun)
{
	return -ENOTSUPP;
}

static inline int prop_chgalgo_notifier_call(struct prop_chgalgo_device *pca,
					     struct prop_chgalgo_notify *notify)
{
	return -ENOTSUPP;
}

static inline int
prop_chgalgo_thermal_throttling(struct prop_chgalgo_device *pca, int mA)
{
	return -ENOTSUPP;
}

static inline int
prop_chgalgo_set_jeita_vbat_cv(struct prop_chgalgo_device *pca, int mV)
{
	return -ENOTSUPP;
}
#endif /* CONFIG_RT_PROP_CHGALGO */
#endif /* __LINUX_PROP_CHGALGO_CLASS_H */
