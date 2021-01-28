/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PDC_H
#define __MTK_PDC_H

#define ADAPTER_CAP_MAX_NR 10

struct pd_cap {
	uint8_t selected_cap_idx;
	uint8_t nr;
	uint8_t pdp;
	uint8_t pwr_limit[ADAPTER_CAP_MAX_NR];
	int max_mv[ADAPTER_CAP_MAX_NR];
	int min_mv[ADAPTER_CAP_MAX_NR];
	int ma[ADAPTER_CAP_MAX_NR];
	int maxwatt[ADAPTER_CAP_MAX_NR];
	int minwatt[ADAPTER_CAP_MAX_NR];
	uint8_t type[ADAPTER_CAP_MAX_NR];
	int info[ADAPTER_CAP_MAX_NR];
};

struct pdc_data {
	int input_current_limit;
	int charging_current_limit;
	int battery_cv;
	int min_charger_voltage;
	int pd_vbus_low_bound;
	int pd_vbus_upper_bound;
	int ibus_err;
	int vsys_watt;
};

struct pdc {
	struct pd_cap cap;
	struct pdc_data data;

	int pdc_max_watt;
	int pdc_max_watt_setting;

	int pd_cap_max_watt;
	int pd_idx;
	int pd_reset_idx;
	int pd_boost_idx;
	int pd_buck_idx;
	int vbus_l;
	int vbus_h;

	struct mutex access_lock;
	struct mutex pmic_sync_lock;
	struct wakeup_source suspend_lock;
	int ta_vchr_org;
	bool to_check_chr_type;
	bool to_tune_ta_vchr;
	bool is_cable_out_occur;

	int pdc_input_current_limit_setting;	/* TA */
};

extern int pdc_init(void);
extern bool pdc_is_ready(void);
extern int pdc_stop(void);
extern int pdc_run(void);
extern int pdc_set_data(struct pdc_data data);
extern struct pdc_data *pdc_get_data(void);

#endif /* __MTK_PDC_H */
