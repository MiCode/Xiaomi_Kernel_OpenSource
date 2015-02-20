/*
 * intel_em_config.c : Intel EM configuration setup code
 *
 * (C) Copyright 2014 Intel Corporation
 * Author: Kotakonda, Venkataramana <venkataramana.kotakonda@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <asm/intel_em_config.h>

#define EM_CONFIG_OEM0_NAME "OEM0"
#define EM_CONFIG_OEM1_NAME "OEM1"

#ifdef DEBUG
static void dump_chrg_profile(const struct ps_pse_mod_prof *chrg_prof);
#endif


static struct ps_pse_mod_prof chrg_prof;
static struct ps_batt_chg_prof batt_chrg_prof;

static int em_config_get_acpi_table(char *name, void *data, int data_size)
{
	struct acpi_table_header *acpi_tbl = NULL;
	acpi_size tbl_size;
	acpi_status status;
	int ret = 0;
	int hdr_size = sizeof(struct acpi_table_header);

	status = acpi_get_table_with_size(name , 0,
					&acpi_tbl, &tbl_size);
	if (ACPI_SUCCESS(status)) {
		pr_info("EM:%s  table found, size=%d\n", name, (int)tbl_size);
		if (tbl_size < (data_size + hdr_size)) {
			pr_err("EM:%s table incomplete!!\n", name);
		} else {
			memcpy(data, ((char *)acpi_tbl) + hdr_size, data_size);
			ret = data_size;
		}
	} else {
		pr_err("EM:%s table not found!!\n", name);
	}

	return ret;
}

int em_config_get_oem0_data(struct em_config_oem0_data *data)
{
	return em_config_get_acpi_table(EM_CONFIG_OEM0_NAME,
				data, sizeof(struct em_config_oem0_data));
}

int em_config_get_oem1_data(struct em_config_oem1_data *data)
{
	return em_config_get_acpi_table(EM_CONFIG_OEM1_NAME,
				data, sizeof(struct em_config_oem1_data));
}
EXPORT_SYMBOL(em_config_get_oem1_data);

static int em_config_get_charge_profile(struct ps_pse_mod_prof *chrg_prof)
{
	int ret = 0;

	if (chrg_prof == NULL)
		return 0;
	ret = em_config_get_oem0_data((struct em_config_oem0_data *)chrg_prof);
	if (ret > 0) {
		/*
		 * battery_type field contains 2 bytes, and upper byte
		 * contains * battery_type & lower byte used for turbo,
		 * which is discarded
		 */
		chrg_prof->battery_type = chrg_prof->battery_type >> 8;
#ifdef DEBUG
		dump_chrg_profile(chrg_prof);
#endif
	}
	return ret;
}

#ifdef DEBUG
static void dump_chrg_profile(const struct ps_pse_mod_prof *chrg_prof)
{
	u16 i = 0;

	pr_info("OEM0:batt_id = %s\n", chrg_prof->batt_id);
	pr_info("OEM0:battery_type = %d\n", chrg_prof->battery_type);
	pr_info("OEM0:capacity = %d\n", chrg_prof->capacity);
	pr_info("OEM0:voltage_max = %d\n", chrg_prof->voltage_max);
	pr_info("OEM0:chrg_term_ma = %d\n", chrg_prof->chrg_term_ma);
	pr_info("OEM0:low_batt_mV = %d\n", chrg_prof->low_batt_mV);
	pr_info("OEM0:disch_tmp_ul = %d\n", chrg_prof->disch_tmp_ul);
	pr_info("OEM0:disch_tmp_ll = %d\n", chrg_prof->disch_tmp_ll);
	pr_info("OEM0:temp_mon_ranges = %d\n", chrg_prof->temp_mon_ranges);
	for (i = 0; i < chrg_prof->temp_mon_ranges; i++) {
		pr_info("OEM0:temp_mon_range[%d].up_lim = %d\n",
				i, chrg_prof->temp_mon_range[i].temp_up_lim);
		pr_info("OEM0:temp_mon_range[%d].full_chrg_vol = %d\n",
				i, chrg_prof->temp_mon_range[i].full_chrg_vol);
		pr_info("OEM0:temp_mon_range[%d].full_chrg_cur = %d\n",
				i, chrg_prof->temp_mon_range[i].full_chrg_cur);
		pr_info("OEM0:temp_mon_range[%d].maint_chrg_vol_ll = %d\n", i,
				chrg_prof->temp_mon_range[i].maint_chrg_vol_ll);
		pr_info("OEM0:temp_mon_range[%d].main_chrg_vol_ul = %d\n", i,
				chrg_prof->temp_mon_range[i].maint_chrg_vol_ul);
		pr_info("OEM0:temp_mon_range[%d].main_chrg_cur = %d\n",
				i, chrg_prof->temp_mon_range[i].maint_chrg_cur);
	}
	pr_info("OEM0:temp_low_lim = %d\n", chrg_prof->temp_low_lim);
}
#endif

static int __init em_config_init(void)
{
	int ret;

	ret = em_config_get_charge_profile(&chrg_prof);

	if (ret)
		batt_chrg_prof.chrg_prof_type = PSE_MOD_CHRG_PROF;
	else
		batt_chrg_prof.chrg_prof_type = CHRG_PROF_NONE;

	batt_chrg_prof.batt_prof = &chrg_prof;

#ifdef CONFIG_POWER_SUPPLY_BATTID
	battery_prop_changed(POWER_SUPPLY_BATTERY_INSERTED, &batt_chrg_prof);
#endif
	return 0;
}
early_initcall(em_config_init);

static void __exit em_config_exit(void)
{
#ifdef CONFIG_POWER_SUPPLY_BATTID
	batt_chrg_prof.chrg_prof_type = CHRG_PROF_NONE;
	battery_prop_changed(POWER_SUPPLY_BATTERY_INSERTED, &batt_chrg_prof);
#endif
}
module_exit(em_config_exit);
