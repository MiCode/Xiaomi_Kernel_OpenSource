/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/err.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/batterydata-lib.h>
#include <linux/power_supply.h>

static int of_batterydata_read_lut(const struct device_node *np,
			int max_cols, int max_rows, int *ncols, int *nrows,
			int *col_legend_data, int *row_legend_data,
			int *lut_data)
{
	struct property *prop;
	const __be32 *data;
	int cols, rows, size, i, j, *out_values;

	prop = of_find_property(np, "qcom,lut-col-legend", NULL);
	if (!prop) {
		pr_err("%s: No col legend found\n", np->name);
		return -EINVAL;
	} else if (!prop->value) {
		pr_err("%s: No col legend value found, np->name\n", np->name);
		return -ENODATA;
	} else if (prop->length > max_cols * sizeof(int)) {
		pr_err("%s: Too many columns\n", np->name);
		return -EINVAL;
	}

	cols = prop->length/sizeof(int);
	*ncols = cols;
	data = prop->value;
	for (i = 0; i < cols; i++)
		*col_legend_data++ = be32_to_cpup(data++);

	prop = of_find_property(np, "qcom,lut-row-legend", NULL);
	if (!prop || row_legend_data == NULL) {
		/* single row lut */
		rows = 1;
	} else if (!prop->value) {
		pr_err("%s: No row legend value found\n", np->name);
		return -ENODATA;
	} else if (prop->length > max_rows * sizeof(int)) {
		pr_err("%s: Too many rows\n", np->name);
		return -EINVAL;
	} else {
		rows = prop->length/sizeof(int);
		*nrows = rows;
		data = prop->value;
		for (i = 0; i < rows; i++)
			*row_legend_data++ = be32_to_cpup(data++);
	}

	prop = of_find_property(np, "qcom,lut-data", NULL);
	if (!prop) {
		pr_err("prop 'qcom,lut-data' not found\n");
		return -EINVAL;
	}
	data = prop->value;
	size = prop->length/sizeof(int);
	if (size != cols * rows) {
		pr_err("%s: data size mismatch, %dx%d != %d\n",
				np->name, cols, rows, size);
		return -EINVAL;
	}
	for (i = 0; i < rows; i++) {
		out_values = lut_data + (max_cols * i);
		for (j = 0; j < cols; j++) {
			*out_values++ = be32_to_cpup(data++);
			pr_debug("Value = %d\n", *(out_values-1));
		}
	}

	return 0;
}

static int of_batterydata_read_sf_lut(struct device_node *data_node,
				const char *name, struct sf_lut *lut)
{
	struct device_node *node = of_find_node_by_name(data_node, name);
	int rc;

	if (!lut) {
		pr_debug("No lut provided, skipping\n");
		return 0;
	} else if (!node) {
		pr_err("Couldn't find %s node.\n", name);
		return -EINVAL;
	}

	rc = of_batterydata_read_lut(node, PC_CC_COLS, PC_CC_ROWS,
			&lut->cols, &lut->rows, lut->row_entries,
			lut->percent, *lut->sf);
	if (rc) {
		pr_err("Failed to read %s node.\n", name);
		return rc;
	}

	return 0;
}

static int of_batterydata_read_pc_temp_ocv_lut(struct device_node *data_node,
				const char *name, struct pc_temp_ocv_lut *lut)
{
	struct device_node *node = of_find_node_by_name(data_node, name);
	int rc;

	if (!lut) {
		pr_debug("No lut provided, skipping\n");
		return 0;
	} else if (!node) {
		pr_err("Couldn't find %s node.\n", name);
		return -EINVAL;
	}
	rc = of_batterydata_read_lut(node, PC_TEMP_COLS, PC_TEMP_ROWS,
			&lut->cols, &lut->rows, lut->temp, lut->percent,
			*lut->ocv);
	if (rc) {
		pr_err("Failed to read %s node.\n", name);
		return rc;
	}

	return 0;
}

static int of_batterydata_read_ibat_temp_acc_lut(struct device_node *data_node,
			const char *name, struct ibat_temp_acc_lut *lut)
{
	struct device_node *node = of_find_node_by_name(data_node, name);
	int rc;

	if (!lut) {
		pr_debug("No lut provided, skipping\n");
		return 0;
	} else if (!node) {
		pr_debug("Couldn't find %s node.\n", name);
		return 0;
	}
	rc = of_batterydata_read_lut(node, ACC_TEMP_COLS, ACC_IBAT_ROWS,
			&lut->cols, &lut->rows, lut->temp, lut->ibat,
			*lut->acc);
	if (rc) {
		pr_err("Failed to read %s node.\n", name);
		return rc;
	}

	return 0;
}

static int of_batterydata_read_single_row_lut(struct device_node *data_node,
				const char *name, struct single_row_lut *lut)
{
	struct device_node *node = of_find_node_by_name(data_node, name);
	int rc;

	if (!lut) {
		pr_debug("No lut provided, skipping\n");
		return 0;
	} else if (!node) {
		pr_err("Couldn't find %s node.\n", name);
		return -EINVAL;
	}

	rc = of_batterydata_read_lut(node, MAX_SINGLE_LUT_COLS, 1,
			&lut->cols, NULL, lut->x, NULL, lut->y);
	if (rc) {
		pr_err("Failed to read %s node.\n", name);
		return rc;
	}

	return 0;
}

static int of_batterydata_read_batt_id_kohm(const struct device_node *np,
				const char *propname, struct batt_ids *batt_ids)
{
	struct property *prop;
	const __be32 *data;
	int num, i, *id_kohm = batt_ids->kohm;

	prop = of_find_property(np, "qcom,batt-id-kohm", NULL);
	if (!prop) {
		pr_err("%s: No battery id resistor found\n", np->name);
		return -EINVAL;
	} else if (!prop->value) {
		pr_err("%s: No battery id resistor value found, np->name\n",
						np->name);
		return -ENODATA;
	} else if (prop->length > MAX_BATT_ID_NUM * sizeof(__be32)) {
		pr_err("%s: Too many battery id resistors\n", np->name);
		return -EINVAL;
	}

	num = prop->length/sizeof(__be32);
	batt_ids->num = num;
	data = prop->value;
	for (i = 0; i < num; i++)
		*id_kohm++ = be32_to_cpup(data++);

	return 0;
}

#define OF_PROP_READ(property, qpnp_dt_property, node, rc, optional)	\
do {									\
	if (rc)								\
		break;							\
	rc = of_property_read_u32(node, "qcom," qpnp_dt_property,	\
					&property);			\
									\
	if ((rc == -EINVAL) && optional) {				\
		property = -EINVAL;					\
		rc = 0;							\
	} else if (rc) {						\
		pr_err("Error reading " #qpnp_dt_property		\
				" property rc = %d\n", rc);		\
	}								\
} while (0)

static int of_batterydata_load_battery_data(struct device_node *node,
				int best_id_kohm,
				struct bms_battery_data *batt_data)
{
	int rc;

	rc = of_batterydata_read_single_row_lut(node, "qcom,fcc-temp-lut",
			batt_data->fcc_temp_lut);
	if (rc)
		return rc;

	rc = of_batterydata_read_pc_temp_ocv_lut(node,
			"qcom,pc-temp-ocv-lut",
			batt_data->pc_temp_ocv_lut);
	if (rc)
		return rc;

	rc = of_batterydata_read_sf_lut(node, "qcom,rbatt-sf-lut",
			batt_data->rbatt_sf_lut);
	if (rc)
		return rc;

	rc = of_batterydata_read_ibat_temp_acc_lut(node, "qcom,ibat-acc-lut",
						batt_data->ibat_acc_lut);
	if (rc)
		return rc;

	rc = of_property_read_string(node, "qcom,battery-type",
					&batt_data->battery_type);
	if (rc) {
		pr_err("Error reading qcom,battery-type property rc=%d\n", rc);
		batt_data->battery_type = NULL;
		return rc;
	}

	OF_PROP_READ(batt_data->fcc, "fcc-mah", node, rc, false);
	OF_PROP_READ(batt_data->default_rbatt_mohm,
			"default-rbatt-mohm", node, rc, false);
	OF_PROP_READ(batt_data->rbatt_capacitive_mohm,
			"rbatt-capacitive-mohm", node, rc, false);
	OF_PROP_READ(batt_data->flat_ocv_threshold_uv,
			"flat-ocv-threshold-uv", node, rc, true);
	OF_PROP_READ(batt_data->max_voltage_uv,
			"max-voltage-uv", node, rc, true);
	OF_PROP_READ(batt_data->cutoff_uv, "v-cutoff-uv", node, rc, true);
	OF_PROP_READ(batt_data->iterm_ua, "chg-term-ua", node, rc, true);

	batt_data->batt_id_kohm = best_id_kohm;

	return rc;
}

static int64_t of_batterydata_convert_battery_id_kohm(int batt_id_uv,
				int rpull_up, int vadc_vdd)
{
	int64_t resistor_value_kohm, denom;

	if (batt_id_uv == 0) {
		/* vadc not correct or batt id line grounded, report 0 kohms */
		return 0;
	}
	/* calculate the battery id resistance reported via ADC */
	denom = div64_s64(vadc_vdd * 1000000LL, batt_id_uv) - 1000000LL;

	if (denom == 0) {
		/* batt id connector might be open, return 0 kohms */
		return 0;
	}
	resistor_value_kohm = div64_s64(rpull_up * 1000000LL + denom/2, denom);

	pr_debug("batt id voltage = %d, resistor value = %lld\n",
			batt_id_uv, resistor_value_kohm);

	return resistor_value_kohm;
}

struct device_node *of_batterydata_get_best_profile(
		const struct device_node *batterydata_container_node,
		const char *psy_name,  const char  *batt_type)
{
	struct batt_ids batt_ids;
	struct device_node *node, *best_node = NULL;
	struct power_supply *psy;
	const char *battery_type = NULL;
	union power_supply_propval ret = {0, };
	int delta = 0, best_delta = 0, best_id_kohm = 0, id_range_pct,
		batt_id_kohm = 0, i = 0, rc = 0;

	psy = power_supply_get_by_name(psy_name);
	if (!psy) {
		pr_err("%s supply not found. defer\n", psy_name);
		return ERR_PTR(-EPROBE_DEFER);
	}

	rc = psy->get_property(psy, POWER_SUPPLY_PROP_RESISTANCE_ID, &ret);
	if (rc) {
		pr_err("failed to retrieve resistance value rc=%d\n", rc);
		return ERR_PTR(-ENOSYS);
	}

	batt_id_kohm = ret.intval / 1000;

	/*
	 * Find the battery data with a battery id resistor closest to this one
	 */
	for_each_child_of_node(batterydata_container_node, node) {
		if (batt_type != NULL) {
			rc = of_property_read_string(node, "qcom,battery-type",
							&battery_type);
			if (!rc && strcmp(battery_type, batt_type) == 0) {
				best_node = node;
				best_id_kohm = batt_id_kohm;
				break;
			}
		} else {
			rc = of_batterydata_read_batt_id_kohm(node,
							"qcom,batt-id-kohm",
							&batt_ids);
			if (rc)
				continue;
			for (i = 0; i < batt_ids.num; i++) {
				delta = abs(batt_ids.kohm[i] - batt_id_kohm);
				if (delta < best_delta || !best_node) {
					best_node = node;
					best_delta = delta;
					best_id_kohm = batt_ids.kohm[i];
				}
			}
		}
	}

	if (best_node == NULL) {
		pr_err("No battery data found\n");
		return best_node;
	}

	/* read battery id value for best profile */
	rc = of_property_read_u32(batterydata_container_node,
			"qcom,batt-id-range-pct", &id_range_pct);
	if (!rc) {
		/* check that profile id is in range of the measured batt_id */
		if (abs(best_id_kohm - batt_id_kohm) >
				((best_id_kohm * id_range_pct) / 100)) {
			pr_err("out of range: profile id %d batt id %d pct %d",
				best_id_kohm, batt_id_kohm, id_range_pct);
			return NULL;
		}
	} else if (rc == -EINVAL) {
		rc = 0;
	} else {
		pr_err("failed to read battery id range\n");
		return ERR_PTR(-ENXIO);
	}

	rc = of_property_read_string(best_node, "qcom,battery-type",
							&battery_type);
	if (!rc)
		pr_info("%s found\n", battery_type);
	else
		pr_info("%s found\n", best_node->name);

	return best_node;
}

int of_batterydata_read_data(struct device_node *batterydata_container_node,
				struct bms_battery_data *batt_data,
				int batt_id_uv)
{
	struct device_node *node, *best_node;
	struct batt_ids batt_ids;
	const char *battery_type = NULL;
	int delta, best_delta, batt_id_kohm, rpull_up_kohm,
		vadc_vdd_uv, best_id_kohm, i, rc = 0;

	node = batterydata_container_node;
	OF_PROP_READ(rpull_up_kohm, "rpull-up-kohm", node, rc, false);
	OF_PROP_READ(vadc_vdd_uv, "vref-batt-therm", node, rc, false);
	if (rc)
		return rc;

	batt_id_kohm = of_batterydata_convert_battery_id_kohm(batt_id_uv,
					rpull_up_kohm, vadc_vdd_uv);
	best_node = NULL;
	best_delta = 0;
	best_id_kohm = 0;

	/*
	 * Find the battery data with a battery id resistor closest to this one
	 */
	for_each_child_of_node(batterydata_container_node, node) {
		rc = of_batterydata_read_batt_id_kohm(node,
						"qcom,batt-id-kohm",
						&batt_ids);
		if (rc)
			continue;
		for (i = 0; i < batt_ids.num; i++) {
			delta = abs(batt_ids.kohm[i] - batt_id_kohm);
			if (delta < best_delta || !best_node) {
				best_node = node;
				best_delta = delta;
				best_id_kohm = batt_ids.kohm[i];
			}
		}
	}

	if (best_node == NULL) {
		pr_err("No battery data found\n");
		return -ENODATA;
	}
	rc = of_property_read_string(best_node, "qcom,battery-type",
							&battery_type);
	if (!rc)
		pr_info("%s loaded\n", battery_type);
	else
		pr_info("%s loaded\n", best_node->name);

	return of_batterydata_load_battery_data(best_node,
					best_id_kohm, batt_data);
}

MODULE_LICENSE("GPL v2");
