 // SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.Lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/slab.h>

#define SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV		3400000
#define SPRD_BATTERY_OCV_CAP_TABLE_CHECK_VOLT_THRESHOLD	3000000

int sprd_battery_parse_cmdline_match_by_split(char *match_str, char* split, char *result, int size)
{
	struct device_node *cmdline_node = NULL;
	const char *cmdline;
	char *match, *match_end;
	int len, match_str_len, ret;

	if (!result || !match_str || !split)
		return -EINVAL;

	memset(result, '\0', size);
	match_str_len = strlen(match_str);

	cmdline_node = of_find_node_by_path("/chosen");
	if (!cmdline_node) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		pr_err("%s failed to read bootargs\n", __func__);
		return -EINVAL;
	}

	match = strstr(cmdline, match_str);
	if (!match) {
		pr_err("Mmatch: %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	match_end = strstr((match + match_str_len), split);
	if (!match_end) {
		pr_err("Match end of : %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	len = match_end - (match + match_str_len);
	if (len < 0 || len > size) {
		pr_err("Match cmdline :%s fail, len = %d\n", match_str, len);
		return -EINVAL;
	}

	memcpy(result, (match + match_str_len), len);

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_battery_parse_cmdline_match_by_split);

static int sprd_battery_parse_cmdline_match(struct power_supply *psy,
					    char *match_str, char *result, int size)
{
	struct device_node *cmdline_node = NULL;
	const char *cmdline;
	char *match, *match_end;
	int len, match_str_len, ret;

	if (!result || !match_str)
		return -EINVAL;

	memset(result, '\0', size);
	match_str_len = strlen(match_str);

	cmdline_node = of_find_node_by_path("/chosen");
	if (!cmdline_node) {
		dev_warn(&psy->dev, "%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		dev_warn(&psy->dev, "%s failed to read bootargs\n", __func__);
		return -EINVAL;
	}

	match = strstr(cmdline, match_str);
	if (!match) {
		dev_warn(&psy->dev, "Mmatch: %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	match_end = strstr((match + match_str_len), " ");
	if (!match_end) {
		dev_warn(&psy->dev, "Match end of : %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	len = match_end - (match + match_str_len);
	if (len < 0 || len > size) {
		dev_warn(&psy->dev, "Match cmdline :%s fail, len = %d\n", match_str, len);
		return -EINVAL;
	}

	memcpy(result, (match + match_str_len), len);

	return 0;
}

int sprd_battery_parse_battery_id(struct power_supply *psy)
{
	char *str = "bat.id=";
	char result[32] = {};
	int id = 0, ret;

	ret = sprd_battery_parse_cmdline_match(psy, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &id);
		if (ret) {
			id = 0;
			dev_err(&psy->dev, "Covert bat_id fail, ret = %d, result = %s\n",
				ret, result);
		}
	}
	//secret battery
	if (id == -1) {
		ret = sprd_battery_parse_cmdline_match_by_split("batt_id:", ";", result, sizeof(result));
		if (!ret) {
			ret = kstrtoint(result, 10, &id);
			if (ret) {
				id = 0;
				dev_err(&psy->dev, "Covert bat_id fail, ret = %d, result = %s\n",
					ret, result);
			}
		}
		//id=2 map to bat2, id=1 map to bat0, else map to bat0
		if (id != 2 )
			id = 0;
	}
	dev_info(&psy->dev, "Batteryid = %d\n", id);

	return id;
}
EXPORT_SYMBOL_GPL(sprd_battery_parse_battery_id);

static int sprd_battery_parse_charge_cycle(struct power_supply *psy)
{
	char result[32] = {};
	int ret, charge_cycle = 0;
	char *str;

	str = "charge.charge_cycle=";
	ret = sprd_battery_parse_cmdline_match(psy, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &charge_cycle);
		if (ret) {
			charge_cycle = -1;
			dev_err(&psy->dev, "Covert charge_cycle fail, ret = %d, result = %s\n",
				ret, result);
		}
	}

	dev_info(&psy->dev, "charge_cycle = %d\n", charge_cycle);

	return charge_cycle;
}

int sprd_battery_get_aging_bat_id(struct power_supply *psy, int charge_cycle)
{
	int aging_bat_id = 0, bat_id, i, charge_cycle_thres, len, ret;
	struct device_node *np;
	char *cycle_str;

	np = of_find_compatible_node(NULL, NULL, "sprd,sprd-fgu");
	bat_id = sprd_battery_parse_battery_id(psy);
	if (!np) {
		dev_err(&psy->dev, "failed to get fuel gauge device node!!!\n");
		return 0;
	}

	cycle_str = kasprintf(GFP_KERNEL, "bat-%d-aging-cycles", bat_id);
	if ((!of_property_read_bool(np, cycle_str)) ||
	    (!of_property_read_bool(np, "sprd,bat-aging"))) {
		dev_err(&psy->dev, "Do not support battery aging function!!!\n");
		return 0;
	}

	len = of_property_count_u32_elems(np, cycle_str);
	if (len < 0 && len != -EINVAL) {
		goto out;
	} else if (len > SPRD_BATTERY_CYCLES_FCC_COLS_MAX) {
		dev_err(&psy->dev, "too many cycles values\n");
		goto out;
	} else if (len > 0) {
		for (i = 1; i < len; i++) {
			ret = of_property_read_u32_index(np, cycle_str, i, &charge_cycle_thres);
			if (ret) {
				dev_err(&psy->dev, "failed to get charge_cycle_thres!!!\n");
				goto out;
			}

			if (i == len - 1 && charge_cycle >= charge_cycle_thres * 1000) {
				aging_bat_id = i;
				break;
			}

			if (charge_cycle < charge_cycle_thres * 1000) {
				aging_bat_id = i - 1;
				break;
			}
		}
	}

out:
	dev_info(&psy->dev, "aging_bat_id = %d\n", aging_bat_id);

	return aging_bat_id;
}
EXPORT_SYMBOL_GPL(sprd_battery_get_aging_bat_id);

static bool sprd_battery_ocv_cap_table_check(struct power_supply *psy,
					     struct sprd_battery_ocv_table *table,
					     int table_len)
{
	int i;

	if (table[table_len - 1].ocv / SPRD_BATTERY_OCV_CAP_TABLE_CHECK_VOLT_THRESHOLD != 1 ||
	    table[0].capacity != 100 || table[table_len - 1].capacity != 0)
		return false;
	for (i = 0; i < table_len - 2; i++) {
		if (table[i].ocv <= table[i + 1].ocv ||
		    table[i].capacity <= table[i + 1].capacity) {
			dev_info(&psy->dev, "the index = %d or %d ocv_cap table value is abnormal!\n",
				 i, i + 1);
			return false;
		}
	}

	return true;
}

static bool sprd_battery_resistance_ocv_table_check(int *ocv_table, int len)
{
	int i;

	if ((ocv_table[0] < SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV) ||
	    (ocv_table[len - 1] < SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV))
		return false;

	for (i = 0; i < len - 2; i++) {
		if (ocv_table[i] <= ocv_table[i + 1])
			return false;
	}

	return true;
}

static int sprd_battery_resistance_temp_table_check(int resist_temp_size,
						    int *resist_temp_table_size)
{
	int i, temp;

	temp = resist_temp_table_size[0];

	for (i = 1; i < resist_temp_size; i++) {
		if (temp != resist_temp_table_size[i]) {
			pr_err("battery_internal_resistance_table_len[%d] is wrong\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_battery_resistance_ocv_table_size_check(int resist_temp_table_size,
							int resist_ocv_table_size)
{
	if (resist_temp_table_size != resist_ocv_table_size) {
		pr_err("resist_ocv_table_size is wrong\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_factory_internal_resistance_check(struct sprd_battery_info *info)
{
	int ret;

	ret = sprd_battery_resistance_temp_table_check(
		info->battery_internal_resistance_temp_table_len,
		info->battery_internal_resistance_table_len);
	if (ret)
		return ret;

	ret = sprd_battery_resistance_ocv_table_size_check(
		info->battery_internal_resistance_table_len[0],
		info->battery_internal_resistance_ocv_table_len);

	return ret;
}

static int sprd_battery_parse_battery_ocv_capacity_table(struct sprd_battery_info *info,
							 struct device_node *battery_np,
							 struct power_supply *psy)
{
	int len, index;

	len = of_property_count_u32_elems(battery_np, "ocv-capacity-celsius");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_BATTERY_OCV_TEMP_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (len > 0) {
		of_property_read_u32_array(battery_np, "ocv-capacity-celsius",
					   info->battery_ocv_temp_table, len);
	}

	for (index = 0; index < len; index++) {
		struct sprd_battery_ocv_table *table;
		char *propname;
		const __be32 *list;
		int i, tab_len, size;

		propname = kasprintf(GFP_KERNEL, "ocv-capacity-table-%d", index);
		list = of_get_property(battery_np, propname, &size);
		if (!list || !size) {
			dev_err(&psy->dev, "failed to get %s\n", propname);
			kfree(propname);
			return -EINVAL;
		}

		kfree(propname);
		tab_len = size / (2 * sizeof(__be32));
		info->battery_ocv_table_len[index] = tab_len;

		table = info->battery_ocv_table[index] =
			devm_kcalloc(&psy->dev, tab_len, sizeof(*table), GFP_KERNEL);
		if (!info->battery_ocv_table[index])
			return -ENOMEM;

		for (i = 0; i < tab_len; i++) {
			table[i].ocv = be32_to_cpu(*list++);
			table[i].capacity = be32_to_cpu(*list++);
		}

		if (!sprd_battery_ocv_cap_table_check(psy, info->battery_ocv_table[index],
						      info->battery_ocv_table_len[index])) {
			dev_err(&psy->dev, "ocv_cap_table value is wrong, please check\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_battery_parse_battery_voltage_temp_table(struct sprd_battery_info *info,
							 struct device_node *battery_np,
							 struct power_supply *psy)
{
	struct sprd_battery_vol_temp_table *battery_vol_temp_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "voltage-temp-table", &len);
	if (!list || !len)
		return 0;

	info->battery_vol_temp_table_len = len / (2 * sizeof(__be32));
	battery_vol_temp_table = info->battery_vol_temp_table =
		devm_kcalloc(&psy->dev, info->battery_vol_temp_table_len,
			     sizeof(*battery_vol_temp_table), GFP_KERNEL);
	if (!info->battery_vol_temp_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_vol_temp_table_len; index++) {
		battery_vol_temp_table[index].vol = be32_to_cpu(*list++);
		battery_vol_temp_table[index].temp = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_temp_capacity_table(struct sprd_battery_info *info,
							  struct device_node *battery_np,
							  struct power_supply *psy)
{
	struct sprd_battery_temp_cap_table *battery_temp_cap_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "capacity-temp-table", &len);
	if (!list || !len)
		return 0;

	info->battery_temp_cap_table_len = len / (2 * sizeof(__be32));
	battery_temp_cap_table = info->battery_temp_cap_table =
		devm_kcalloc(&psy->dev, info->battery_temp_cap_table_len,
			     sizeof(*battery_temp_cap_table), GFP_KERNEL);
	if (!info->battery_temp_cap_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_temp_cap_table_len; index++) {
		battery_temp_cap_table[index].temp = be32_to_cpu(*list++);
		battery_temp_cap_table[index].cap = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_resistance_temp_table(struct sprd_battery_info *info,
							    struct device_node *battery_np,
							    struct power_supply *psy)
{
	struct sprd_battery_resistance_temp_table *resist_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "resistance-temp-table", &len);
	if (!list || !len)
		return 0;

	info->battery_temp_resist_table_len = len / (2 * sizeof(__be32));
	resist_table = info->battery_temp_resist_table = devm_kcalloc(&psy->dev,
							 info->battery_temp_resist_table_len,
							 sizeof(*resist_table),
							 GFP_KERNEL);
	if (!info->battery_temp_resist_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_temp_resist_table_len; index++) {
		resist_table[index].temp = be32_to_cpu(*list++);
		resist_table[index].resistance = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_remap_full_percent_table(struct sprd_battery_info *info,
							       struct device_node *battery_np,
							       struct power_supply *psy)
{
	struct sprd_battery_temp_fullcap_table *battery_temp_fullcap_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "temp-advance-fullcap-table", &len);
	if (!list || !len)
		return 0;

	info->battery_temp_fullcap_table_len = len / (2 * sizeof(__be32));
	battery_temp_fullcap_table = info->battery_temp_fullcap_table =
		devm_kcalloc(&psy->dev, info->battery_temp_fullcap_table_len,
			     sizeof(*battery_temp_fullcap_table), GFP_KERNEL);
	if (!info->battery_temp_fullcap_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_temp_fullcap_table_len; index++) {
		battery_temp_fullcap_table[index].temp = be32_to_cpu(*list++);
		battery_temp_fullcap_table[index].temp *= 10;
		battery_temp_fullcap_table[index].advance_fullcap = be32_to_cpu(*list++);
	}
	return 0;
}

static int sprd_battery_parse_battery_internal_resistance_table(struct sprd_battery_info *info,
								struct device_node *battery_np,
								struct power_supply *psy)
{
	int len, index;

	len = of_property_count_u32_elems(battery_np, "battery-internal-resistance-celsius");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (len > 0) {
		info->battery_internal_resistance_temp_table_len = len;
		of_property_read_u32_array(battery_np, "battery-internal-resistance-celsius",
					   info->battery_internal_resistance_temp_table, len);
		for (index = 0; index < len; index++)
			info->battery_internal_resistance_temp_table[index] *= 10;
	}

	for (index = 0; index < len; index++) {
		int *table;
		char *propname;
		const __be32 *list;
		int i, size;
		u32 tab_len;

		propname = kasprintf(GFP_KERNEL, "battery-internal-resistance-table-%d", index);
		list = of_get_property(battery_np, propname, &size);
		if (!list || !size) {
			dev_err(&psy->dev, "failed to get %s\n", propname);
			kfree(propname);
			return -EINVAL;
		}

		kfree(propname);
		tab_len = (u32)size / (sizeof(__be32));
		info->battery_internal_resistance_table_len[index] = tab_len;

		table = info->battery_internal_resistance_table[index] =
			devm_kzalloc(&psy->dev, tab_len * sizeof(*table), GFP_KERNEL);
		if (!info->battery_internal_resistance_table[index])
			return -ENOMEM;

		for (i = 0; i < tab_len; i++)
			table[i] = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_internal_resistance_ocv_table(struct sprd_battery_info *info,
								    struct device_node *battery_np,
								    struct power_supply *psy)
{
	const __be32 *list;
	int index, size;
	int *resistance_ocv_table;

	list = of_get_property(battery_np, "battery-internal-resistance-ocv-table", &size);
	if (!list || !size)
		return 0;

	info->battery_internal_resistance_ocv_table_len = (u32)size / (sizeof(__be32));
	resistance_ocv_table = info->battery_internal_resistance_ocv_table =
		devm_kcalloc(&psy->dev, info->battery_internal_resistance_ocv_table_len,
			     sizeof(*resistance_ocv_table), GFP_KERNEL);
	if (!info->battery_internal_resistance_ocv_table) {
		dev_err(&psy->dev, "battery_internal_resistance_ocv_table is null\n");
		return -ENOMEM;
	}

	for (index = 0; index < info->battery_internal_resistance_ocv_table_len; index++)
		resistance_ocv_table[index] = be32_to_cpu(*list++);

	if (!sprd_battery_resistance_ocv_table_check(resistance_ocv_table,
		info->battery_internal_resistance_ocv_table_len)) {
		dev_err(&psy->dev, "resistance_ocv_table value is wrong, please check\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_energy_density_ocv_table_check(density_ocv_table *table, int len)
{
	int i;

	for (i = 0; i < len - 1; i++) {
		if (table[i].engy_dens_ocv_lo >= table[i].engy_dens_ocv_hi)
			return false;
	}

	for (i = 0; i < len - 2; i++) {
		if ((table[i].engy_dens_ocv_lo >= table[i + 1].engy_dens_ocv_lo) ||
		    (table[i].engy_dens_ocv_hi >= table[i + 1].engy_dens_ocv_hi))
			return false;
	}

	return true;
}

static int sprd_battery_parse_energy_density_ocv_table(struct sprd_battery_info *info,
						       struct device_node *battery_np,
						       struct power_supply *psy,
						       char *name, int *table_len,
						       density_ocv_table **dens_ocv_table)
{
	density_ocv_table *table;
	const __be32 *list;
	int i, size;

	list = of_get_property(battery_np, name, &size);
	if (!list || !size)
		return 0;

	*table_len = size / (sizeof(density_ocv_table) / sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(&psy->dev, sizeof(density_ocv_table) * (*table_len + 1), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < *table_len; i++) {
		table[i].engy_dens_ocv_lo = be32_to_cpu(*list++);
		table[i].engy_dens_ocv_hi = be32_to_cpu(*list++);
		dev_info(&psy->dev, "%s: engy_dens_ocv_hi = %d, engy_dens_ocv_lo = %d\n",
			 name, table[i].engy_dens_ocv_hi, table[i].engy_dens_ocv_lo);
	}

	*dens_ocv_table = table;

	if (!sprd_battery_energy_density_ocv_table_check(*dens_ocv_table, *table_len)) {
		dev_err(&psy->dev, "%s:  density ocv table value is wrong, please check\n", name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_parse_full_alarm_table(struct sprd_battery_info *info,
					       struct device_node *battery_np,
					       struct power_supply *psy)
{
	struct sprd_battery_full_alarm_table *battery_full_alarm_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "full-alarm-table", &len);
	if (!list || !len)
		return 0;

	info->battery_full_alarm_table_len = len / (5 * sizeof(__be32));
	battery_full_alarm_table = info->battery_full_alarm_table =
		devm_kcalloc(&psy->dev, info->battery_full_alarm_table_len,
			sizeof(*battery_full_alarm_table), GFP_KERNEL);
	if (!info->battery_full_alarm_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_full_alarm_table_len; index++) {
		battery_full_alarm_table[index].full_alarm_cap = be32_to_cpu(*list++);
		battery_full_alarm_table[index].full_alarm_cur = be32_to_cpu(*list++);
		battery_full_alarm_table[index].full_alarm_cur_min = be32_to_cpu(*list++);
		battery_full_alarm_table[index].full_alarm_cur_max1 = be32_to_cpu(*list++);
		battery_full_alarm_table[index].full_alarm_cur_max2 = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_temp_fcc_lut(struct sprd_battery_info *info,
						   struct device_node *battery_np,
						   struct power_supply *psy)
{
	info->battery_temp_fcc_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_temp_fcc_lut), GFP_KERNEL);

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_TEMP_FCC_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		if (info->bat_temp_len != of_property_count_u32_elems(battery_np, "temp-fcc-fcc")) {
			dev_err(&psy->dev, "the temp and fcc table length is not equals\n");
			return -EINVAL;
		}
		info->battery_temp_fcc_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   info->battery_temp_fcc_lut->temp, info->bat_temp_len);
		of_property_read_u32_array(battery_np, "temp-fcc-fcc",
					   info->battery_temp_fcc_lut->fcc, info->bat_temp_len);
	}

	return 0;
}

static int sprd_battery_parse_battery_temp_hc_lut(struct sprd_battery_info *info,
						  struct device_node *battery_np,
						  struct power_supply *psy)
{
	info->battery_temp_hc_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_temp_hc_lut), GFP_KERNEL);

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_TEMP_FCC_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		if (info->bat_temp_len != of_property_count_u32_elems(battery_np, "temp-hc-hc")) {
			dev_err(&psy->dev, "the temp and hc table length is not equals\n");
			return -EINVAL;
		}
		info->battery_temp_hc_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					  info->battery_temp_hc_lut->temp, info->bat_temp_len);
		of_property_read_u32_array(battery_np, "temp-hc-hc",
					  info->battery_temp_hc_lut->hc, info->bat_temp_len);
	}

	return 0;
}


static int sprd_battery_parse_battery_fcc_cycle_ratio_lut(struct sprd_battery_info *info,
							  struct device_node *battery_np,
							  struct power_supply *psy)
{
	struct sprd_fcc_cycle_ratio_lut *fcc_cycle_ratio_lut;
	int len, i, j, total_len;

	info->battery_fcc_cycle_ratio_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_fcc_cycle_ratio_lut), GFP_KERNEL);
	fcc_cycle_ratio_lut = info->battery_fcc_cycle_ratio_lut;

	len = of_property_count_u32_elems(battery_np, "fcc-cycle-ratio-cycle");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_CAP_TEMP_ROWS_MAX) {
		dev_err(&psy->dev, "Too many capacity values\n");
		return -EINVAL;
	} else if (len > 0) {
		fcc_cycle_ratio_lut->rows = len;
		of_property_read_u32_array(battery_np, "fcc-cycle-ratio-cycle",
					   fcc_cycle_ratio_lut->cycle, len);
	}

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_CAP_TEMP_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		fcc_cycle_ratio_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   fcc_cycle_ratio_lut->temp,
					   info->bat_temp_len);
	}

	total_len = of_property_count_u32_elems(battery_np, "fcc-cycle-ratio-ratio");
	if (fcc_cycle_ratio_lut->rows * fcc_cycle_ratio_lut->cols !=
	    total_len) {
		fcc_cycle_ratio_lut->rows = 0;
		fcc_cycle_ratio_lut->cols = 0;
		dev_err(&psy->dev, "The number of cycle-temp-ratio lut does not match!!!\n");
		return -EINVAL;
	}

	for (i = 0; i < fcc_cycle_ratio_lut->rows; i++) {
		for (j = 0; j < fcc_cycle_ratio_lut->cols; j++) {
			of_property_read_u32_index(battery_np, "fcc-cycle-ratio-ratio",
						   j * fcc_cycle_ratio_lut->rows + i,
						   &(fcc_cycle_ratio_lut->ratio[i][j]));
		}
	}

	return 0;
}

static int sprd_battery_parse_battery_temp_zp_vol_lut(struct sprd_battery_info *info,
						      struct device_node *battery_np,
						      struct power_supply *psy)
{
	info->battery_temp_zp_vol_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_temp_zp_voltage_lut), GFP_KERNEL);

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_TEMP_ZP_VOL_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		if (info->bat_temp_len !=
		    of_property_count_u32_elems(battery_np, "temp-zpvol-zpvol")) {
			dev_err(&psy->dev, "the temp and zp vol table length is not equals\n");
			return -EINVAL;
		}
		info->battery_temp_zp_vol_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   info->battery_temp_zp_vol_lut->temp,
					   info->bat_temp_len);
		of_property_read_u32_array(battery_np, "temp-zpvol-zpvol",
					   info->battery_temp_zp_vol_lut->vol,
					   info->bat_temp_len);
	}

	return 0;
}

static int sprd_battery_parse_battery_temp_abs_zp_vol_lut(struct sprd_battery_info *info,
							  struct device_node *battery_np,
							  struct power_supply *psy)
{
	info->battery_temp_abs_zp_vol_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_temp_abs_zp_voltage_lut), GFP_KERNEL);

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_TEMP_ZP_VOL_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		if (info->bat_temp_len !=
		    of_property_count_u32_elems(battery_np, "temp-abs-zpvol-zpvol")) {
			dev_err(&psy->dev, "the temp and abs zp vol table length is not equals\n");
			return -EINVAL;
		}
		info->battery_temp_abs_zp_vol_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   info->battery_temp_abs_zp_vol_lut->temp,
					   info->bat_temp_len);
		of_property_read_u32_array(battery_np, "temp-abs-zpvol-zpvol",
					   info->battery_temp_abs_zp_vol_lut->vol,
					   info->bat_temp_len);
	}

	return 0;
}

static int sprd_battery_parse_battery_temp_bat_vol_lut(struct sprd_battery_info *info,
						       struct device_node *battery_np,
						       struct power_supply *psy)
{
	info->battery_temp_bat_vol_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_temp_bat_voltage_lut), GFP_KERNEL);

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_TEMP_BAT_VOL_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		if (info->bat_temp_len !=
		    of_property_count_u32_elems(battery_np, "temp-batvol-batvol")) {
			dev_err(&psy->dev, "the temp and bat vol table length is not equals\n");
			return -EINVAL;
		}
		info->battery_temp_bat_vol_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   info->battery_temp_bat_vol_lut->temp,
					   info->bat_temp_len);
		of_property_read_u32_array(battery_np, "temp-batvol-batvol",
					   info->battery_temp_bat_vol_lut->vol,
					   info->bat_temp_len);
	}

	return 0;
}

static bool sprd_battery_cap_temp_ocv_lut_check(struct sprd_battery_info *info,
						struct power_supply *psy)
{
	struct sprd_cap_temp_ocv_lut *cap_temp_ocv_lut;
	int i, j;

	cap_temp_ocv_lut = info->battery_cap_temp_ocv_lut;
	for (j = 0; j < cap_temp_ocv_lut->cols; j++) {
		if (cap_temp_ocv_lut->ocv[cap_temp_ocv_lut->rows - 1][j] /
		    SPRD_BATTERY_OCV_CAP_TABLE_CHECK_VOLT_THRESHOLD != 1 ||
		    cap_temp_ocv_lut->cap[0] != 100 ||
		    cap_temp_ocv_lut->cap[cap_temp_ocv_lut->rows - 1] != 0)
			return false;
		for (i = 0; i < cap_temp_ocv_lut->rows - 1; i++) {
			if (cap_temp_ocv_lut->ocv[i][j] <=
			    cap_temp_ocv_lut->ocv[i + 1][j] ||
			    cap_temp_ocv_lut->cap[i] <=
			    cap_temp_ocv_lut->cap[i + 1]) {
				dev_info(&psy->dev, "the %d or %d cap_temp_ocv lut error!\n",
					 i, i + 1);
				return false;
			}
		}
	}

	return true;
}

static bool sprd_battery_cap_temp_ocv_cycle_lut_check(struct sprd_battery_info *info,
						      struct power_supply *psy)
{
	struct sprd_cap_temp_ocv_cycle_lut *cap_temp_ocv_cycle_lut;
	int i, j;

	cap_temp_ocv_cycle_lut = info->battery_cap_temp_ocv_cycle_lut;
	for (j = 0; j < cap_temp_ocv_cycle_lut->cols; j++) {
		if (cap_temp_ocv_cycle_lut->cycle_ocv[cap_temp_ocv_cycle_lut->rows - 1][j] /
			SPRD_BATTERY_OCV_CAP_TABLE_CHECK_VOLT_THRESHOLD != 1 ||
			cap_temp_ocv_cycle_lut->cap[0] != 100 ||
			cap_temp_ocv_cycle_lut->cap[cap_temp_ocv_cycle_lut->rows - 1] != 0)
			return false;
		for (i = 0; i < cap_temp_ocv_cycle_lut->rows - 1; i++) {
			if (cap_temp_ocv_cycle_lut->cycle_ocv[i][j] <=
				cap_temp_ocv_cycle_lut->cycle_ocv[i + 1][j] ||
				cap_temp_ocv_cycle_lut->cap[i] <=
				cap_temp_ocv_cycle_lut->cap[i + 1]) {
				dev_info(&psy->dev, "the %d or %d cap_temp_ocv_cycle lut error!\n",
					 i, i + 1);
				return false;
			}
		}
	}

	return true;
}

static int sprd_battery_parse_battery_cap_temp_ocv_lut(struct sprd_battery_info *info,
						       struct device_node *battery_np,
						       struct power_supply *psy)
{
	int len, i, j, total_len;

	info->battery_cap_temp_ocv_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_cap_temp_ocv_lut), GFP_KERNEL);

	len = of_property_count_u32_elems(battery_np, "cap-temp-ocv-cap");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_CAP_TEMP_ROWS_MAX) {
		dev_err(&psy->dev, "Too many capacity values\n");
		return -EINVAL;
	} else if (len > 0) {
		info->battery_cap_temp_ocv_lut->rows = len;
		of_property_read_u32_array(battery_np, "cap-temp-ocv-cap",
					   info->battery_cap_temp_ocv_lut->cap, len);
	}

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_CAP_TEMP_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		info->battery_cap_temp_ocv_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   info->battery_cap_temp_ocv_lut->temp,
					   info->bat_temp_len);
	}

	total_len = of_property_count_u32_elems(battery_np, "cap-temp-ocv-ocv");
	if (info->battery_cap_temp_ocv_lut->rows * info->battery_cap_temp_ocv_lut->cols !=
	    total_len) {
		info->battery_cap_temp_ocv_lut->rows = 0;
		info->battery_cap_temp_ocv_lut->cols = 0;
		dev_err(&psy->dev, "The number of cap-temp-ocv lut does not match!!!\n");
		return -EINVAL;
	}

	for (i = 0; i < info->battery_cap_temp_ocv_lut->rows; i++) {
		for (j = 0; j < info->battery_cap_temp_ocv_lut->cols; j++) {
			of_property_read_u32_index(battery_np, "cap-temp-ocv-ocv",
						   j * info->battery_cap_temp_ocv_lut->rows + i,
						   &(info->battery_cap_temp_ocv_lut->ocv[i][j]));
		}
	}

	if (!sprd_battery_cap_temp_ocv_lut_check(info, psy)) {
		dev_err(&psy->dev, "cap_temp_ocv lut value is wrong, please check!!!\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_parse_battery_cap_temp_ocv_cycle_lut(struct sprd_battery_info *info,
							     struct device_node *battery_np,
							     struct power_supply *psy)
{
	struct sprd_cap_temp_ocv_cycle_lut *cap_temp_ocv_cycle_lut;
	int len, i, j, total_len;

	info->battery_cap_temp_ocv_cycle_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_cap_temp_ocv_cycle_lut), GFP_KERNEL);
	cap_temp_ocv_cycle_lut = info->battery_cap_temp_ocv_cycle_lut;

	total_len = of_property_count_u32_elems(battery_np, "cap-temp-ocv-cycle-ocv");
	if (total_len < 0) {
		dev_err(&psy->dev, "cap_temp_ocv_cycle lut value is null!!!\n");
		return 0;
	}

	len = of_property_count_u32_elems(battery_np, "cap-temp-ocv-cap");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_CAP_TEMP_ROWS_MAX) {
		dev_err(&psy->dev, "Too many capacity values\n");
		return -EINVAL;
	} else if (len > 0) {
		cap_temp_ocv_cycle_lut->rows = len;
		of_property_read_u32_array(battery_np, "cap-temp-ocv-cap",
					   cap_temp_ocv_cycle_lut->cap, len);
	}

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_CAP_TEMP_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		cap_temp_ocv_cycle_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   cap_temp_ocv_cycle_lut->temp,
					   info->bat_temp_len);
	}

	if (cap_temp_ocv_cycle_lut->rows *
	    cap_temp_ocv_cycle_lut->cols != total_len) {
		cap_temp_ocv_cycle_lut->rows = 0;
		cap_temp_ocv_cycle_lut->cols = 0;
		dev_err(&psy->dev, "The number of cap-temp-ocv cycle lut does not match!!!\n");
		return -EINVAL;
	}

	for (i = 0; i < cap_temp_ocv_cycle_lut->rows; i++) {
		for (j = 0; j < cap_temp_ocv_cycle_lut->cols; j++) {
			of_property_read_u32_index(battery_np, "cap-temp-ocv-cycle-ocv",
						   j * cap_temp_ocv_cycle_lut->rows + i,
						   &(cap_temp_ocv_cycle_lut->cycle_ocv[i][j]));
		}
	}

	if (!sprd_battery_cap_temp_ocv_cycle_lut_check(info, psy)) {
		dev_err(&psy->dev, "cap_temp_ocv_cycle lut value is wrong, please check!!!\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_parse_battery_cap_temp_ibat_lut(struct sprd_battery_info *info,
							struct device_node *battery_np,
							struct power_supply *psy)
{
	int len, i, j, total_len;

	info->battery_cap_temp_ibat_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_cap_temp_ibat_lut), GFP_KERNEL);

	len = of_property_count_u32_elems(battery_np, "cap-temp-ibat-cap");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_CAP_TEMP_ROWS_MAX) {
		dev_err(&psy->dev, "Too many capacity values\n");
		return -EINVAL;
	} else if (len > 0) {
		info->battery_cap_temp_ibat_lut->rows = len;
		of_property_read_u32_array(battery_np, "cap-temp-ibat-cap",
					   info->battery_cap_temp_ibat_lut->cap, len);
	}

	len = of_property_count_u32_elems(battery_np, "cap-temp-ibat-temp");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_CAP_TEMP_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (len > 0) {
		info->battery_cap_temp_ibat_lut->cols = len;
		of_property_read_u32_array(battery_np, "cap-temp-ibat-temp",
					   info->battery_cap_temp_ibat_lut->temp, len);
	}

	total_len = of_property_count_u32_elems(battery_np, "cap-temp-ibat-ibat");
	if (info->battery_cap_temp_ibat_lut->rows * info->battery_cap_temp_ibat_lut->cols !=
	    total_len) {
		info->battery_cap_temp_ibat_lut->rows = 0;
		info->battery_cap_temp_ibat_lut->cols = 0;
		dev_err(&psy->dev, "The number of cap-temp-ibat lut does not match!!!\n");
		return -EINVAL;
	}

	for (i = 0; i < info->battery_cap_temp_ibat_lut->rows; i++) {
		for (j = 0; j < info->battery_cap_temp_ibat_lut->cols; j++) {
			of_property_read_u32_index(battery_np, "cap-temp-ibat-ibat",
						   j * info->battery_cap_temp_ibat_lut->rows + i,
						   &(info->battery_cap_temp_ibat_lut->ibat[i][j]));
		}
	}

	return 0;
}

static bool sprd_battery_cap_temp_resist_lut_check(struct sprd_battery_info *info,
						   struct power_supply *psy)
{
	struct sprd_cap_temp_resist_lut *cap_temp_resist_lut;
	int i;

	cap_temp_resist_lut = info->battery_cap_temp_resist_lut;
	if (cap_temp_resist_lut->cap[0] != 100 ||
	    cap_temp_resist_lut->cap[cap_temp_resist_lut->rows - 1] != 0)
		return false;

	for (i = 0; i < cap_temp_resist_lut->rows - 1; i++) {
		if (cap_temp_resist_lut->cap[i] <=
		    cap_temp_resist_lut->cap[i + 1]) {
			dev_info(&psy->dev, "the %d or %d cap_temp_resist lut value error!\n",
				 i, i + 1);
			return false;
		}
	}

	return true;
}

static int sprd_battery_parse_battery_cap_temp_resist_lut(struct sprd_battery_info *info,
							  struct device_node *battery_np,
							  struct power_supply *psy)
{
	struct sprd_cap_temp_resist_lut *cap_temp_resist_lut;
	int len, i, j, total_len;

	info->battery_cap_temp_resist_lut =
		devm_kzalloc(&psy->dev, sizeof(struct sprd_cap_temp_resist_lut), GFP_KERNEL);
	cap_temp_resist_lut = info->battery_cap_temp_resist_lut;

	len = of_property_count_u32_elems(battery_np, "cap-temp-resist-cap");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_CAP_TEMP_ROWS_MAX) {
		dev_err(&psy->dev, "Too many cap values\n");
		return -EINVAL;
	} else if (len > 0) {
		cap_temp_resist_lut->rows = len;
		of_property_read_u32_array(battery_np, "cap-temp-resist-cap",
					   cap_temp_resist_lut->cap, len);
	}

	if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
		return info->bat_temp_len;
	} else if (info->bat_temp_len > SPRD_OCV_TEMP_COLS_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (info->bat_temp_len > 0) {
		cap_temp_resist_lut->cols = info->bat_temp_len;
		of_property_read_u32_array(battery_np, "bat-temp",
					   cap_temp_resist_lut->temp,
					   info->bat_temp_len);
	}

	total_len = of_property_count_u32_elems(battery_np, "cap-temp-resist-resist");
	if (cap_temp_resist_lut->rows * cap_temp_resist_lut->cols !=
	    total_len) {
		cap_temp_resist_lut->rows = 0;
		cap_temp_resist_lut->cols = 0;
		dev_err(&psy->dev, "The number of cap-temp-resist lut does not match!!!\n");
		return -EINVAL;
	}

	for (i = 0; i < cap_temp_resist_lut->rows; i++) {
		for (j = 0; j < cap_temp_resist_lut->cols; j++) {
			of_property_read_u32_index(battery_np, "cap-temp-resist-resist",
						   j * cap_temp_resist_lut->rows + i,
						   &(cap_temp_resist_lut->resist[i][j]));
		}
	}

	if (!sprd_battery_cap_temp_resist_lut_check(info, psy)) {
		dev_err(&psy->dev, "cap_temp_resist lut value is wrong, please check!!!\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_parse_battery_cap_temp_resist_ratio_lut(struct sprd_battery_info *info,
								struct device_node *battery_np,
								struct power_supply *psy)
{
	struct sprd_cap_temp_resist_ratio_lut *table;
	int len, i, j, total_len, cycle_len, index;

	cycle_len = of_property_count_u32_elems(battery_np, "cap-temp-resist-cycle");
	if (cycle_len < 0 && cycle_len != -EINVAL) {
		return cycle_len;
	} else if (cycle_len > SPRD_BATTERY_OCV_TEMP_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (cycle_len > 0) {
		info->battery_cap_temp_resist_ratio_lut_len = cycle_len;
		of_property_read_u32_array(battery_np, "cap-temp-resist-cycle",
					   info->battery_cap_temp_resist_ratio_table, cycle_len);
	}

	for (index = 0; index < cycle_len; index++) {
		char *propname;

		info->battery_cap_temp_resist_ratio_lut[index] =
			devm_kzalloc(&psy->dev, sizeof(struct sprd_cap_temp_resist_ratio_lut),
				     GFP_KERNEL);
		table = info->battery_cap_temp_resist_ratio_lut[index];

		len = of_property_count_u32_elems(battery_np, "cap-temp-resist-cap");
		if (len < 0 && len != -EINVAL) {
			dev_err(&psy->dev, "%s:len = %d\n", __func__, len);
			return len;
		} else if (len > SPRD_CAP_TEMP_ROWS_MAX) {
			dev_err(&psy->dev, "Too many cap values\n");
			return -EINVAL;
		} else if (len > 0) {
			table->rows = len;
			of_property_read_u32_array(battery_np, "cap-temp-resist-cap",
						   table->cap, len);
		}

		if (info->bat_temp_len < 0 && info->bat_temp_len != -EINVAL) {
			return info->bat_temp_len;
		} else if (info->bat_temp_len > SPRD_OCV_TEMP_COLS_MAX) {
			dev_err(&psy->dev, "Too many temperature values\n");
			return -EINVAL;
		} else if (info->bat_temp_len > 0) {
			table->cols = info->bat_temp_len;
			of_property_read_u32_array(battery_np, "bat-temp",
						   table->temp,
						   info->bat_temp_len);
		}

		propname = kasprintf(GFP_KERNEL, "cap-temp-resist-ratio-%d", index);

		total_len = of_property_count_u32_elems(battery_np, propname);
		if (table->rows * table->cols != total_len) {
			dev_err(&psy->dev, "The num of cap_temp_resist_ratio lut not match!!!\n");
			kfree(propname);
			propname = NULL;
			return -EINVAL;
		}

		for (i = 0; i < table->rows; i++) {
			for (j = 0; j < table->cols; j++) {
				of_property_read_u32_index(battery_np, propname,
							   j * table->rows + i,
							   &(table->res_ratio[i][j]));
			}
		}

		kfree(propname);
		propname = NULL;
	}

	return 0;
}

static int sprd_battery_init_jeita_table(struct sprd_battery_info *info,
					 struct device_node *battery_np,
					 struct device *dev,
					 int jeita_num)
{
	struct sprd_battery_jeita_table *table;
	const __be32 *list;
	const char *np_name = sprd_battery_jeita_type_names[jeita_num];
	struct sprd_battery_jeita_table **cur_table = &info->jeita_table[jeita_num];
	int i, size, max_current_ua = 0;

	list = of_get_property(battery_np, np_name, &size);
	if (!list || !size)
		return 0;

	info->sprd_battery_jeita_size[jeita_num] =
		size / (sizeof(struct sprd_battery_jeita_table) /
			sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(dev, sizeof(struct sprd_battery_jeita_table) *
			     (info->sprd_battery_jeita_size[jeita_num] + 1), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < info->sprd_battery_jeita_size[jeita_num]; i++) {
		table[i].temp = be32_to_cpu(*list++) - 1000;
		table[i].recovery_temp = be32_to_cpu(*list++) - 1000;
		table[i].current_ua = be32_to_cpu(*list++);
		table[i].term_volt = be32_to_cpu(*list++);
		if (max_current_ua < table[i].current_ua) {
			info->max_current_jeita_index[jeita_num] = i;
			max_current_ua = table[i].current_ua;
		}
	}

	*cur_table = table;

	return 0;
}

static int sprd_battery_parse_jeita_table(struct sprd_battery_info *info,
					  struct device_node *battery_np,
					  struct power_supply *psy)
{
	int ret, i;

	for (i = SPRD_BATTERY_JEITA_DCP; i < SPRD_BATTERY_JEITA_MAX; i++) {
		ret = sprd_battery_init_jeita_table(info, battery_np, &psy->dev, i);
		if (ret)
			return ret;
	}

	return 0;
}

static void sprd_battery_parse_step_chg_table(struct sprd_battery_info *info,
					      struct device_node *battery_np,
					      struct power_supply *psy)
{
	struct sprd_battery_step_chg_table *table;
	const __be32 *list;
	int i, size;

	list = of_get_property(battery_np, SPRD_BATTERY_STEP_CHG_TABLE_NAME, &size);
	if (!list || !size) {
		dev_err(&psy->dev, "%s, failed to get %s\n",
			__func__, SPRD_BATTERY_STEP_CHG_TABLE_NAME);
		return;
	}

	info->sprd_battery_step_chg_size =
		size / (sizeof(struct sprd_battery_step_chg_table) /
			sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(&psy->dev, sizeof(struct sprd_battery_step_chg_table) *
		     (info->sprd_battery_step_chg_size + 1), GFP_KERNEL);
	if (!table)
		return;

	for (i = 0; i < info->sprd_battery_step_chg_size; i++) {
		table[i].jeita_inr = be32_to_cpu(*list++);
		table[i].current_ua = be32_to_cpu(*list++);
		table[i].term_volt = be32_to_cpu(*list++);
	}

	info->step_chg_table = table;
}

struct sprd_battery_ocv_table *sprd_battery_find_ocv2cap_table(struct sprd_battery_info *info,
							       int temp, int *table_len)
{
	int best_temp_diff = INT_MAX, temp_diff;
	u8 i, best_index = 0;

	if (!info->battery_ocv_table[0])
		return NULL;

	for (i = 0; i < SPRD_BATTERY_OCV_TEMP_MAX; i++) {
		temp_diff = abs(info->battery_ocv_temp_table[i] - temp);

		if (temp_diff < best_temp_diff) {
			best_temp_diff = temp_diff;
			best_index = i;
		}
	}

	*table_len = info->battery_ocv_table_len[best_index];
	return info->battery_ocv_table[best_index];
}
EXPORT_SYMBOL_GPL(sprd_battery_find_ocv2cap_table);

int sprd_battery_get_battery_para(struct power_supply *psy,
				  struct sprd_battery_info *info,
				  struct device_node *battery_np)
{
	int err;

	of_property_read_string(battery_np, "vendor-cycle-range",
                              &info->vendor_cycle_range);
	of_property_read_u32(battery_np, "charge-full-design-microamp-hours",
			     &info->charge_full_design_uah);
	of_property_read_u32(battery_np, "charge-full-microamp-hours",
			     &info->charge_full_uah);
	of_property_read_u32(battery_np, "voltage-min-design-microvolt",
			     &info->voltage_min_design_uv);
	of_property_read_u32(battery_np, "batt-ovp-threshold-microvolt",
			     &info->batt_ovp_threshold_uv);
	of_property_read_u32(battery_np, "precharge-current-microamp",
			     &info->precharge_current_ua);
	of_property_read_u32(battery_np, "charge-term-current-microamp",
			     &info->charge_term_current_ua);
	of_property_read_u32(battery_np, "constant-charge-current-max-microamp",
			     &info->constant_charge_current_max_ua);
	of_property_read_u32(battery_np, "constant-charge-voltage-max-microvolt",
			     &info->constant_charge_voltage_max_uv);
	of_property_read_u32(battery_np, "fullbatt-voltage-offset-microvolt",
			     &info->fullbatt_voltage_offset_uv);
	of_property_read_u32(battery_np, "factory-internal-resistance-micro-ohms",
			     &info->factory_internal_resistance_uohm);
	of_property_read_u32(battery_np, "fullbatt-track-end-vol",
			     &info->fullbatt_track_end_voltage_uv);
	of_property_read_u32(battery_np, "fullbatt-track-end-cur",
			     &info->fullbatt_track_end_current_uA);
	of_property_read_u32(battery_np, "first-calib-voltage",
			     &info->first_capacity_calibration_voltage_uv);
	of_property_read_u32(battery_np, "first-calib-capacity",
			     &info->first_capacity_calibration_capacity);
	of_property_read_u32(battery_np, "fullbatt-voltage",
			     &info->fullbatt_voltage_uv);
	of_property_read_u32(battery_np, "fullbatt-current",
			     &info->fullbatt_current_uA);
	of_property_read_u32(battery_np, "first-fullbatt-current",
			     &info->first_fullbatt_current_uA);
	of_property_read_u32(battery_np, "shutdown-voltage",
			     &info->shutdown_voltage_uv);
	info->bat_temp_len = of_property_count_u32_elems(battery_np, "bat-temp");
	of_property_read_u32(battery_np, "fast-charge-threshold-microvolt",
			     &info->fast_charge_ocv_threshold_uv);
	of_property_read_u32(battery_np, "ir-rc-micro-ohms", &info->ir.rc_uohm);
	of_property_read_u32(battery_np, "ir-us-upper-limit-microvolt",
			     &info->ir.us_upper_limit_uv);
	of_property_read_u32(battery_np, "ir-cv-offset-microvolt",
			     &info->ir.cv_upper_limit_offset_uv);

	of_property_read_u32_index(battery_np, "charge-sdp-current-microamp", 0,
				   &info->cur.sdp_cur);
	of_property_read_u32_index(battery_np, "charge-sdp-current-microamp", 1,
				   &info->cur.sdp_limit);
	of_property_read_u32_index(battery_np, "charge-dcp-current-microamp", 0,
				   &info->cur.dcp_cur);
	of_property_read_u32_index(battery_np, "charge-dcp-current-microamp", 1,
				   &info->cur.dcp_limit);
	of_property_read_u32_index(battery_np, "charge-cdp-current-microamp", 0,
				   &info->cur.cdp_cur);
	of_property_read_u32_index(battery_np, "charge-cdp-current-microamp", 1,
				   &info->cur.cdp_limit);
	of_property_read_u32_index(battery_np, "charge-unknown-current-microamp", 0,
				   &info->cur.unknown_cur);
	of_property_read_u32_index(battery_np, "charge-unknown-current-microamp", 1,
				   &info->cur.unknown_limit);
	of_property_read_u32_index(battery_np, "charge-fchg-current-microamp", 0,
				   &info->cur.fchg_cur);
	of_property_read_u32_index(battery_np, "charge-fchg-current-microamp", 1,
				   &info->cur.fchg_limit);
	of_property_read_u32_index(battery_np, "charge-flash-current-microamp", 0,
				   &info->cur.flash_cur);
	of_property_read_u32_index(battery_np, "charge-flash-current-microamp", 1,
				   &info->cur.flash_limit);
	of_property_read_u32_index(battery_np, "charge-wl-bpp-current-microamp", 0,
				   &info->cur.wl_bpp_cur);
	of_property_read_u32_index(battery_np, "charge-wl-bpp-current-microamp", 1,
				   &info->cur.wl_bpp_limit);
	of_property_read_u32_index(battery_np, "charge-wl-epp-current-microamp", 0,
				   &info->cur.wl_epp_cur);
	of_property_read_u32_index(battery_np, "charge-wl-epp-current-microamp", 1,
				   &info->cur.wl_epp_limit);

	dev_info(&psy->dev,"vendor-cycle-range = %s\n", info->vendor_cycle_range);
	err = sprd_battery_parse_battery_ocv_capacity_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery ocv capacity table, ret = %d\n", err);

	err = sprd_battery_parse_battery_voltage_temp_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery voltage temp table, ret = %d\n", err);

	err = sprd_battery_parse_battery_temp_capacity_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery temp capacity table, ret = %d\n", err);

	err = sprd_battery_parse_battery_resistance_temp_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery resist temp table, ret = %d\n", err);

	err = sprd_battery_parse_battery_internal_resistance_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get factory internal resist table, ret = %d\n", err);

	err = sprd_battery_parse_battery_temp_fcc_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_temp_fcc_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_temp_hc_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_temp_hc_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_fcc_cycle_ratio_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_fcc_cycle_ratio_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_temp_zp_vol_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_temp_zpvol_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_temp_abs_zp_vol_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_temp_abs_zp_vol_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_temp_bat_vol_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_temp_bat_vol_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_cap_temp_ocv_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_cap_temp_ocv_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_cap_temp_ocv_cycle_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_cap_temp_ocv_cycle_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_cap_temp_ibat_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_cap_temp_ibat_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_cap_temp_resist_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_cap_temp_resist_lut, ret = %d\n", err);

	err = sprd_battery_parse_battery_cap_temp_resist_ratio_lut(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get battery_cap_temp_resist_ratio_lut, ret = %d\n",
			err);

	err = sprd_battery_parse_battery_remap_full_percent_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get remap full percent table, ret = %d\n", err);

	err = sprd_battery_parse_battery_internal_resistance_ocv_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get factory internal resist ocv table, ret = %d\n",
			err);

	err = sprd_battery_factory_internal_resistance_check(info);
	if (err)
		dev_err(&psy->dev, "factory_internal_resistance is not right, err = %d\n", err);

	err = sprd_battery_parse_energy_density_ocv_table(info, battery_np, psy,
							  "cap-calib-energy-density-ocv-table",
							  &info->cap_calib_dens_ocv_table_len,
							  &info->cap_calib_dens_ocv_table);
	if (err)
		dev_err(&psy->dev, "Fail to parse cap cali density ocv table, ret = %d\n", err);

	err = sprd_battery_parse_energy_density_ocv_table(info, battery_np, psy,
							  "cap-track-energy-density-ocv-table",
							  &info->cap_track_dens_ocv_table_len,
							  &info->cap_track_dens_ocv_table);
	if (err)
		dev_err(&psy->dev, "Fail to parse cap track density ocv table, ret = %d\n", err);

	err = sprd_battery_parse_full_alarm_table(info, battery_np, psy);
	if (err)
		dev_err(&psy->dev, "Fail to get full alarm percent table, ret = %d\n", err);

	err = sprd_battery_parse_jeita_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to parse jeita table, err = %d\n", err);
		return err;
	}

	sprd_battery_parse_step_chg_table(info, battery_np, psy);

	return 0;
}

static struct device_node *of_parse_battery_np(struct power_supply *psy, int dynamic_aging_bat_id)
{
	char *propname;
	const char *value;
	struct device_node *battery_np;
	int err, battery_id, aging_bat_id, charge_cycle;

	if (!psy->of_node) {
		dev_warn(&psy->dev, "%s currently only supports devicetree\n", __func__);
		return NULL;
	}

	battery_id = sprd_battery_parse_battery_id(psy);
	propname = kasprintf(GFP_KERNEL, "monitored-battery-%d", battery_id);
	if (!dynamic_aging_bat_id) {
		charge_cycle = sprd_battery_parse_charge_cycle(psy);
		aging_bat_id = sprd_battery_get_aging_bat_id(psy, charge_cycle);
	} else if (dynamic_aging_bat_id == SPRD_BAT_AGING_ID_DEFAULT) {
		aging_bat_id = 0;
	} else {
		aging_bat_id = dynamic_aging_bat_id;
	}

	battery_np = of_parse_phandle(psy->of_node, propname, aging_bat_id);
	if (!battery_np) {
		dev_warn(&psy->dev, "Fail to get monitored-battery-%d, aging-bat-id-%d\n",
			 battery_id, aging_bat_id);
		return NULL;
	}

	err = of_property_read_string(battery_np, "compatible", &value);
	if (err)
		return NULL;

	if (strcmp("simple-battery", value))
		return NULL;

	return battery_np;
}

int sprd_battery_get_battery_info(struct power_supply *psy,
				  struct sprd_battery_info *info,
				  int dynamic_aging_bat_id)
{
	struct device_node *battery_np, *new_battery_np;
	int err, index;

	info->charge_full_design_uah         = -EINVAL;
	info->voltage_min_design_uv          = -EINVAL;
	info->batt_ovp_threshold_uv          = -EINVAL;
	info->precharge_current_ua           = -EINVAL;
	info->charge_term_current_ua         = -EINVAL;
	info->constant_charge_current_max_ua = -EINVAL;
	info->constant_charge_voltage_max_uv = -EINVAL;
	info->fullbatt_voltage_offset_uv = 0;
	info->factory_internal_resistance_uohm  = -EINVAL;
	info->battery_internal_resistance_ocv_table = NULL;
	info->battery_internal_resistance_temp_table_len = -EINVAL;
	info->battery_internal_resistance_ocv_table_len = -EINVAL;
	info->fullbatt_track_end_voltage_uv = -EINVAL;
	info->fullbatt_track_end_current_uA = -EINVAL;
	info->first_capacity_calibration_voltage_uv = -EINVAL;
	info->first_capacity_calibration_capacity = -EINVAL;
	info->fast_charge_ocv_threshold_uv = -EINVAL;
	info->cur.sdp_cur = -EINVAL;
	info->cur.sdp_limit = -EINVAL;
	info->cur.dcp_cur = -EINVAL;
	info->cur.dcp_limit = -EINVAL;
	info->cur.cdp_cur = -EINVAL;
	info->cur.cdp_limit = -EINVAL;
	info->cur.unknown_cur = -EINVAL;
	info->cur.unknown_limit = -EINVAL;
	info->cur.fchg_cur = -EINVAL;
	info->cur.fchg_limit = -EINVAL;
	info->cur.flash_cur = -EINVAL;
	info->cur.flash_limit = -EINVAL;
	info->cur.wl_bpp_cur = -EINVAL;
	info->cur.wl_bpp_limit = -EINVAL;
	info->cur.wl_epp_cur = -EINVAL;
	info->cur.wl_epp_limit = -EINVAL;

	for (index = 0; index < SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX; index++) {
		info->battery_internal_resistance_temp_table[index] = -EINVAL;
		info->battery_internal_resistance_table[index] = NULL;
		info->battery_internal_resistance_table_len[index] = -EINVAL;
	}

	battery_np = of_parse_battery_np(psy, SPRD_BAT_AGING_ID_DEFAULT);
	if (!battery_np)
		return -ENODEV;

	err = sprd_battery_get_battery_para(psy, info, battery_np);
	if (err)
		return err;

	new_battery_np = of_parse_battery_np(psy, dynamic_aging_bat_id);
	if (battery_np == new_battery_np)
		return 0;

	battery_np = new_battery_np;

	err = sprd_battery_get_battery_para(psy, info, battery_np);
	if (err)
		return err;

	return 0;
}

EXPORT_SYMBOL_GPL(sprd_battery_get_battery_info);

void sprd_battery_put_battery_info(struct power_supply *psy, struct sprd_battery_info *info)
{
	int i;

	for (i = 0; i < SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX; i++) {
		if (info->battery_internal_resistance_table[i])
			devm_kfree(&psy->dev, info->battery_internal_resistance_table[i]);
	}

	if (info->battery_internal_resistance_ocv_table)
		devm_kfree(&psy->dev, info->battery_internal_resistance_ocv_table);
}
EXPORT_SYMBOL_GPL(sprd_battery_put_battery_info);

void sprd_battery_find_resistance_table(struct power_supply *psy,
					int **resistance_table, int table_len,
					int *temp_table, int temp_len, int temp,
					int *target_table)
{
	int temp_index, i, delta_res;

	if (!resistance_table[0] || !temp_table || !target_table)
		return;

	for (i = temp_len - 1; i >= 0; i--) {
		if (temp > temp_table[i])
			break;
	}

	temp_index = i;

	if (temp_index == temp_len - 1) {
		memcpy(target_table, resistance_table[temp_len - 1], table_len);
		return;
	} else if (temp_index == -1) {
		memcpy(target_table, resistance_table[0], table_len);
		return;
	}

	for (i = 0; i < table_len; i++) {
		delta_res = DIV_ROUND_CLOSEST((resistance_table[temp_index + 1][i] -
					       resistance_table[temp_index][i]) *
					      (temp - temp_table[temp_index]),
					      (temp_table[temp_index + 1] -
					       temp_table[temp_index]));

		target_table[i] = resistance_table[temp_index][i] + delta_res;
	}
}
EXPORT_SYMBOL_GPL(sprd_battery_find_resistance_table);

MODULE_LICENSE("GPL v2");
