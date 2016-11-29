/*
 * intel_em_config.c : Intel EM configuration setup code
 *
 * (C) Copyright 2014 Intel Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
 * Author: Kotakonda, Venkataramana <venkataramana.kotakonda@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <asm/intel_em_config.h>

#define EM_CONFIG_OEM0_NAME "OEM0"
#define EM_CONFIG_OEM1_NAME "OEM1"

#ifdef DEBUG
static void dump_chrg_profile(const struct ps_pse_mod_prof *chrg_prof);
#endif


static char battery_vendor[BATTID_STR_LEN + 1] = "";
module_param_string(battery_vendor, battery_vendor, sizeof(battery_vendor), 0444);
MODULE_PARM_DESC(battery_vendor, "Battery vendor");

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
		pr_err("EM:%s  table found, size=%d\n", name, (int)tbl_size);
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

/* ************************ NOTE  ******************************
 * Now the default is for Xiaomi battery according to the SPEC of
 * X6 -ATL-2014-2-13-TENTATIVE.pdf and
 * X6 -LG-2014-2-13-TENTATIVE.pdf
 */
struct ps_pse_mod_prof default_chrg_profile = {
	"LG", /* Battery ID */
	0,          /* TurboChargingSupport */
	2,          /* BatteryType */
	6190,       /* Capacity */
	4400,       /* MaxVoltage */
	256,        /* ChargeTermiationCurrent */
	3000,       /* LowBatteryThreshold */
	60,         /* SafeDischargeTemperatureUL */
	-20,        /* SafeDischargeTemperatureLL */
	4,          /* TempMonitoringRanges */
	{
		{
			60,     /* UpperTemperatureLimit */
			4100,   /* FastChargeVoltageUpperThreshold */
			3000,   /* FastChargeCurrentLimit */
			4100,   /* MaintenanceChargeVoltageLowerThreshold */
			4100,   /* MaintenanceChargeVoltageUpperThreshold */
			3000    /* MaintenanceChargeCurrentLimit */
		},
		{
			45,     /* UpperTemperatureLimit */
			4400,   /* FastChargeVoltageUpperThreshold */
			3000,   /* FastChargeCurrentLimit */
			4400,   /* MaintenanceChargeVoltageLowerThreshold */
			4400,   /* MaintenanceChargeVoltageUpperThreshold */
			3000    /* MaintenanceChargeCurrentLimit */
		},
		{
			10,     /* UpperTemperatureLimit */
			4400,   /* FastChargeVoltageUpperThreshold */
			1800,   /* FastChargeCurrentLimit */
			4400,   /* MaintenanceChargeVoltageLowerThreshold */
			4400,   /* MaintenanceChargeVoltageUpperThreshold */
			3000    /*MaintenanceChargeCurrentLimit */
		},
		{
			0, /* UpperTemperatureLimit */
			0, /* FastChargeVoltageUpperThreshold */
			0, /* FastChargeCurrentLimit */
			0, /* MaintenanceChargeVoltageLowerThreshold */
			0, /* MaintenanceChargeVoltageUpperThreshold */
			0  /* MaintenanceChargeCurrentLimit */
		},
	},
	0         /* Temp Lower Limit */
};

static int em_config_get_charge_profile(struct ps_pse_mod_prof *chrg_prof)
{
	struct em_config_oem0_data chrg_prof_temp;
	int ret = 0;

	if (chrg_prof == NULL)
		return 0;
#if !defined  DEBUG_CHRG_PROF
	ret = em_config_get_oem0_data(&chrg_prof_temp);
	if (ret > 0) {
		pr_info("EM: get acpi data");
		memcpy(chrg_prof, &chrg_prof_temp, BATTID_STR_LEN);
		chrg_prof->batt_id[BATTID_STR_LEN] = '\0';
		memcpy(&(chrg_prof->turbo),
			((char *)&chrg_prof_temp) + BATTID_STR_LEN,
			sizeof(struct em_config_oem0_data) - BATTID_STR_LEN);
	} else {
		memcpy(chrg_prof, &default_chrg_profile, BATTID_STR_LEN);
		chrg_prof->batt_id[BATTID_STR_LEN] = '\0';
		memcpy(&(chrg_prof->turbo),
			((char *)&default_chrg_profile) + BATTID_STR_LEN + 1,
			sizeof(struct em_config_oem0_data) - BATTID_STR_LEN);
		pr_err("EM:default chrg profile table is available!!\n");
	}
#else
	memcpy(chrg_prof, &default_chrg_profile, BATTID_STR_LEN);
	chrg_prof->batt_id[BATTID_STR_LEN] = '\0';
	memcpy(&(chrg_prof->turbo),
		((char *)&default_chrg_profile) + BATTID_STR_LEN + 1,
		sizeof(struct em_config_oem0_data) - BATTID_STR_LEN);
	pr_err("EM:default chrg profile table is available!!\n");

#endif

	memcpy(battery_vendor, chrg_prof->batt_id, BATTID_STR_LEN);
#ifdef DEBUG
		dump_chrg_profile(chrg_prof);
#endif
	return ret;
}

#ifdef DEBUG
static void dump_chrg_profile(const struct ps_pse_mod_prof *chrg_prof)
{
	u16 i = 0;

	pr_err("OEM0:batt_id = %s\n", chrg_prof->batt_id);
	pr_err("OEM0:battery_type = %d\n", chrg_prof->battery_type);
	pr_err("OEM0:capacity = %d\n", chrg_prof->capacity);
	pr_err("OEM0:voltage_max = %d\n", chrg_prof->voltage_max);
	pr_err("OEM0:chrg_term_ma = %d\n", chrg_prof->chrg_term_ma);
	pr_err("OEM0:low_batt_mV = %d\n", chrg_prof->low_batt_mV);
	pr_err("OEM0:disch_tmp_ul = %d\n", chrg_prof->disch_tmp_ul);
	pr_err("OEM0:disch_tmp_ll = %d\n", chrg_prof->disch_tmp_ll);
	pr_err("OEM0:temp_mon_ranges = %d\n", chrg_prof->temp_mon_ranges);
	for (i = 0; i < chrg_prof->temp_mon_ranges; i++) {
		pr_err("OEM0:temp_mon_range[%d].up_lim = %d\n",
				i, chrg_prof->temp_mon_range[i].temp_up_lim);
		pr_err("OEM0:temp_mon_range[%d].full_chrg_vol = %d\n",
				i, chrg_prof->temp_mon_range[i].full_chrg_vol);
		pr_err("OEM0:temp_mon_range[%d].full_chrg_cur = %d\n",
				i, chrg_prof->temp_mon_range[i].full_chrg_cur);
		pr_err("OEM0:temp_mon_range[%d].maint_chrg_vol_ll = %d\n", i,
				chrg_prof->temp_mon_range[i].maint_chrg_vol_ll);
		pr_err("OEM0:temp_mon_range[%d].main_chrg_vol_ul = %d\n", i,
				chrg_prof->temp_mon_range[i].maint_chrg_vol_ul);
		pr_err("OEM0:temp_mon_range[%d].main_chrg_cur = %d\n",
				i, chrg_prof->temp_mon_range[i].maint_chrg_cur);
	}
	pr_err("OEM0:temp_low_lim = %d\n", chrg_prof->temp_low_lim);
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
