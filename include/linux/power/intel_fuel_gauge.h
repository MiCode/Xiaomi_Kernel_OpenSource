/*
 * intel_fuel_gauge.h - Intel MID PMIC Fuel Gauge Driver header
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Srinidhi Rao <srinidhi.rao@intel.com>
 *	   Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */

#ifndef __INTEL_FUEL_GAUGE__
#define __INTEL_FUEL_GAUGE__

#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/mutex.h>
#include <linux/power/battery_id.h>

struct set_cc_val {
	bool is_set;
	int val;
};

struct fg_algo_ip_params {
	int vbatt;
	int vavg;
	int vocv;
	int ibatt;
	int iavg;
	int bat_temp;
	int delta_q;

	/* Coulomb Counter I/P Params */
	int up_cc;
	int down_cc;
	int acc_err;
	int delta_thr;
	int long_avg;
	int long_avg_at_ocv;
	int ocv_accuracy;
};

struct fg_algo_op_params {
	int soc;
	int nac;
	int fcc;
	int cycle_count;
	bool calib_cc;

	/* Coulomb Counter O/P Params */
	int acc_err;
	int delta_thr;
	int ibat_avg_at_ocv;

	struct set_cc_val reset_acc_err;
	struct set_cc_val set_delta_thr;
	struct set_cc_val clr_latched_ibat_avg;
};

struct fg_batt_params {
	int v_ocv_bootup;
	int v_ocv_now;
	int vbatt_now;
	int i_bat_bootup;
	int i_batt_avg;
	int i_batt_now;
	int batt_temp_now;
	int capacity;
	int charge_now;
	int charge_full;
	int charge_counter;
	int status;
	bool boot_flag;
	bool is_valid_battery;
	char battid[BATTID_STR_LEN + 1];

	/* Coulomb Counter initial params */
	int up_cc;
	int down_cc;
	int acc_err;
	int delta_thr;
	int long_avg;
	int long_avg_ocv;
	int ocv_accuracy;
};

struct fg_algo_params {
	struct fg_algo_ip_params fg_ip;
	struct fg_algo_op_params fg_op;
};

struct intel_fg_batt_spec {
	int volt_min_design;
	int volt_max_design;
	int temp_min;
	int temp_max;
	int charge_full_design;
};

struct intel_fg_input {
	int (*get_batt_params)(int *vbat, int *ibat, int *bat_temp);
	int (*get_v_ocv)(int *v_ocv);
	int (*get_v_ocv_bootup)(int *v_ocv_bootup);
	int (*get_i_bat_bootup)(int *i_bat_bootup);
	int (*get_v_avg)(int *v_avg);
	int (*get_i_avg)(int *i_avg);
	int (*get_delta_q)(int *delta_q);
	int (*calibrate_cc)(void);

	/* Coulomb Counter APIs */
	bool (*wait_for_cc)(void);
	int (*get_up_cc)(int *up_cc);
	int (*get_down_cc)(int *down_cc);
	int (*get_acc_err)(int *acc_err);
	int (*get_delta_thr)(int *delta_thr);
	int (*get_long_avg)(int *long_avg);
	int (*get_long_avg_ocv)(int *long_avg_ocv);
	int (*get_ocv_accuracy)(int *ocv_acuracy);

	int (*reset_acc_err)(int acc_err);
	int (*set_delta_thr)(int delta_thr);
	int (*clr_latched_ibat_avg)(int ibat_avg_at_ocv);
};

enum intel_fg_algo_type {
	INTEL_FG_ALGO_PRIMARY,
	INTEL_FG_ALGO_SECONDARY,
};

struct intel_fg_algo {
	enum intel_fg_algo_type type;
	bool init_done;
	int (*fg_algo_init)(struct fg_batt_params *bat_params);
	int (*fg_algo_process)(struct fg_algo_ip_params *ip,
					struct fg_algo_op_params *op);
};

#ifdef CONFIG_INTEL_FUEL_GAUGE
int intel_fg_register_input(struct intel_fg_input *input);
int intel_fg_unregister_input(struct intel_fg_input *input);
int intel_fg_register_algo(struct intel_fg_algo *algo);
int intel_fg_unregister_algo(struct intel_fg_algo *algo);
#else
static int intel_fg_register_input(struct intel_fg_input *input)
{
	return 0;
}
static int intel_fg_unregister_input(struct intel_fg_input *input)
{
	return 0;
}
static int intel_fg_register_algo(struct intel_fg_algo *algo)
{
	return 0;
}
static int intel_fg_unregister_algo(struct intel_fg_algo *algo)
{
	return 0;
}
#endif

#endif
