/*
 * intel_em_config.h : Intel EM configuration setup code
 *
 * (C) Copyright 2014 Intel Corporation
 * Author: Kotakonda, Venkataramana <venkataramana.kotakonda@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _INTEL_EM_CONFIG_H
#define _INTEL_EM_CONFIG_H

#include <linux/types.h>
#include <linux/acpi.h>
#include <linux/power/battery_id.h>


struct em_config_oem0_data {
	char batt_id[BATTID_STR_LEN];
	 u8 turbo;
	 u8 batt_type;
	u16 capacity;
	u16 volt_max;
	u16 chrg_term_ma;
	u16 low_batt_thr;
	u8  safe_dischrg_ul;
	u8  safe_dischrg_ll;
	u16 temp_mon_ranges;
	struct ps_temp_chg_table temp_mon_range[BATT_TEMP_NR_RNG];
	/* Temperature lower limit */
	u16 temp_low_lim;
} __packed;


/********* OEM1 Table Structures ****************/
struct em_config_oem1_data {
	u8 fpo_0;
	u8 fpo_1;
	u8 dbiin_gpio;
	u8 dbiout_gpio;
	u8 batchptyp;
	u16 ia_apps_run_volt;
	u8 batid_dbibase;
	u8 batid_anlgbase;
	u8 ia_apps_cap;
	u16 vbatt_freq_lmt;
	u8 cap_freq_idx;
	u8 rsvd_1; /* reserved bit*/
	u8 batidx;
	u8 ia_apps_to_use;
	u8 turbo_chrg;
} __packed;

#ifdef CONFIG_ACPI
/*
 * em_config_get_oem0_data - This function fetches OEM0 table .
 * @data : Pointer to OEM0 data structure in which data should be filled.
 *
 * Returns number bytes fetched (+ve) on success or 0 on error.
 *
 */
int em_config_get_oem0_data(struct em_config_oem0_data *data);

/*
 * em_config_get_oem1_data - This function fetches OEM1 table .
 * @data : Pointer to OEM1 data structure in which data should be filled.
 *
 * Returns number bytes fetched (+ve) on success or 0 on error.
 *
 */
int em_config_get_oem1_data(struct em_config_oem1_data *data);

#else

static int em_config_get_oem1_data(struct em_config_oem1_data *data)
{
	return 0;
}

#endif /* CONFIG_ACPI */

#endif /*_INTEL_EM_CONFIG_H */
