// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"QBG-K: %s: " fmt, __func__

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <uapi/linux/qbg-profile.h>

#include "qbg-battery-profile.h"

static int table_temperatures[] = { -20, -10, 0, 10, 25, 40, 50 };

static int qbg_battery_data_open(struct inode *inode, struct file *file)
{
	struct qbg_battery_data *battery = container_of(inode->i_cdev,
				struct qbg_battery_data, battery_cdev);

	pr_debug("battery_data device opened\n");
	file->private_data = battery;

	return 0;
}

static long qbg_battery_data_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct qbg_battery_data *battery = file->private_data;
	struct battery_config __user *profile_user;
	struct battery_profile_table bp_table;
	struct battery_profile_table __user *bp_table_user;
	struct battery_data_table *table;
	int rc = 0, table_index, table_type;

	if (!battery->profile_node) {
		pr_err("Invalid battery profile node\n");
		return -EINVAL;
	}

	if (!arg) {
		pr_err("Invalid user pointer\n");
		return -EINVAL;
	}

	switch (cmd) {
	case BPIOCXBP:
		profile_user = (struct battery_config __user *)arg;

		if (copy_to_user(profile_user, &battery->bp, sizeof(battery->bp))) {
			pr_err("Failed to copy battery profile to user\n");
			return -EFAULT;
		}

		break;
	case BPIOCXBPTABLE:
		bp_table_user = (struct battery_profile_table __user *)arg;

		if (copy_from_user(&bp_table, bp_table_user, sizeof(bp_table))) {
			pr_err("Failed to copy battery_profile_table from user\n");
			return -EFAULT;
		}

		table_index = bp_table.table_index;
		table_type = bp_table.table_type;

		if ((table_type != CHARGE_TABLE) &&
		    (table_type != DISCHARGE_TABLE))
			return -EFAULT;

		if (((table_type == CHARGE_TABLE) &&
		     (table_index >= battery->num_ctables)) ||
		     ((table_type == DISCHARGE_TABLE) &&
		     (table_index >= battery->num_dtables)))
			return -EFAULT;

		table = (table_type == CHARGE_TABLE) ?
				battery->bp_charge_tables[table_index] :
				battery->bp_discharge_tables[table_index];

		if (copy_to_user(bp_table.table, table, sizeof(*table))) {
			pr_err("Failed to copy battery profile table to user\n");
			return -EFAULT;
		}

		pr_debug("Copied %s table %d to user\n",
			table_type == CHARGE_TABLE ? "Charge" : "Discharge", table_index);
		break;
	default:
		pr_err_ratelimited("IOCTL %u not supported\n", cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int qbg_battery_data_release(struct inode *inode, struct file *file)
{
	pr_debug("battery_data device closed\n");

	return 0;
}

static const struct file_operations qbg_battery_data_fops = {
	.owner = THIS_MODULE,
	.open = qbg_battery_data_open,
	.unlocked_ioctl = qbg_battery_data_ioctl,
	.compat_ioctl = qbg_battery_data_ioctl,
	.release = qbg_battery_data_release,
};

#define QBG_PON_TEMPERATURE	25
static int qbg_parse_table0(struct device_node *profile_node,
		char *table_name, struct battery_data_table0 *table)
{
	struct device_node *node;
	int rc = 0, temperature;

	node = of_find_node_by_name(profile_node, table_name);
	if (!node) {
		pr_err("%s not found\n", table_name);
		return -ENODEV;
	}

	rc = of_property_read_s32(node, "qcom,temperature", &temperature);
	if (rc < 0) {
		pr_err("Failed to read %s temperature\n", table_name);
		goto out;
	}

	if (temperature != QBG_PON_TEMPERATURE) {
		pr_err("Invalid table0 found, temperature:%d\n",
			temperature);
		rc = -EINVAL;
		goto out;
	}

	rc = of_property_count_elems_of_size(node, "qcom,soc", sizeof(int));
	if (rc < 0) {
		pr_err("Failed to get soc-length for %s, rc=%d\n",
			table_name, rc);
		goto out;
	}
	table->soc_length = rc;

	table->soc = kcalloc(table->soc_length, sizeof(*table->soc), GFP_KERNEL);
	if (!table->soc) {
		rc = -ENOMEM;
		goto out;
	}

	rc = of_property_read_u32_array(node, "qcom,soc", table->soc,
						table->soc_length);
	if (rc < 0) {
		pr_err("Failed to read qcom,soc\n");
		rc = -EINVAL;
		goto cleanup_soc;
	}

	rc = of_property_count_elems_of_size(node, "qcom,ocv", sizeof(int));
	if (rc < 0) {
		pr_err("Failed to get ocv-length for %s, rc=%d\n",
			table_name, rc);
		goto cleanup_soc;
	}
	table->ocv_length = rc;

	table->ocv = kcalloc(table->ocv_length, sizeof(*table->ocv), GFP_KERNEL);
	if (!table->ocv) {
		rc = -ENOMEM;
		goto cleanup_soc;
	}

	rc = of_property_read_u32_array(node, "qcom,ocv", table->ocv,
						table->ocv_length);
	if (rc < 0) {
		pr_err("Failed to read qcom,ocv\n");
		rc = -EINVAL;
		goto cleanup_ocv;
	}

	return 0;

cleanup_ocv:
	kfree(table->ocv);
cleanup_soc:
	kfree(table->soc);
out:
	of_node_put(node);

	return rc;
}

static int qbg_parse_table(struct device_node *profile_node,
		int index, char *table_name, struct battery_data_table *bp_table)
{
	struct device_node *node;
	struct property *prop;
	const __be32 *data;
	int rc = 0, j, k, temperature;
	u32 rows, cols;

	node = of_find_node_by_name(profile_node, table_name);
	if (!node) {
		pr_err("%s not found\n", table_name);
		return -ENODEV;
	}

	rc = of_property_read_s32(node, "qcom,temperature", &temperature);
	if (rc < 0) {
		pr_err("Failed to read %s temperature\n", table_name);
		goto out;
	}

	if (temperature != table_temperatures[index]) {
		pr_err("Invalid table at wrong index %d temperature:%d\n",
			index, temperature);
		rc = -EINVAL;
		goto out;
	}

	rc = of_property_read_u32(node, "qcom,nrows", &rows);
	if (rc < 0) {
		pr_err("Failed to read %s\n", table_name);
		goto out;
	}
	bp_table->nrows = rows;

	rc = of_property_read_u32(node, "qcom,ncols", &cols);
	if (rc < 0) {
		pr_err("Failed to read %s\n", table_name);
		goto out;
	}
	bp_table->ncols = cols;

	rc = of_property_read_u32_array(node, "qcom,conv-factor",
					bp_table->unit_conv_factor,
					MAX_BP_LUT_COLS);
	if (rc < 0) {
		pr_err("Failed to read conv-factor\n");
		rc = -EINVAL;
		goto out;
	}

	prop = of_find_property(node, "qcom,data", NULL);
	if (!prop) {
		pr_err("Failed to find lut-data\n");
		rc = -EINVAL;
		goto out;
	}

	data = prop->value;

	for (j = 0; j < bp_table->nrows; j++) {
		for (k = 0; k < bp_table->ncols; k++)
			bp_table->table[j][k] = be32_to_cpup(data++);
	}

	pr_debug("Profile %s parsed rows=%d cols=%d\n", table_name,
		bp_table->nrows, bp_table->ncols);
out:
	of_node_put(node);

	return rc;
}

static int qbg_parse_u32_dt_array(struct device_node *node,
				const char *prop_name, int *buf, int len)
{
	int rc;

	rc = of_property_count_elems_of_size(node, prop_name, sizeof(u32));
	if (rc < 0) {
		pr_err("Property %s not found, rc=%d\n", prop_name, rc);
		return rc;
	} else if (rc != len) {
		pr_err("Incorrect length %d for %s, rc=%d\n", len, prop_name,
			rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_name, buf, len);
	if (rc < 0) {
		pr_err("Error in reading %s, rc=%d\n", prop_name, rc);
		return rc;
	}

	return 0;
}

static int qbg_parse_battery_profile(struct qbg_battery_data *battery)
{
	struct device_node *node = battery->profile_node;
	struct device_node *child;
	struct battery_config *bp = &battery->bp;
	char buf[32];
	const char *battery_name = NULL;
	int rc, i = 0;
	u32 temp[2];

	rc = of_property_read_string(node, "qcom,battery-type", &battery_name);
	if (rc < 0) {
		pr_err("Failed to get battery type, rc=%d\n", rc);
		return rc;
	}
	strlcpy(bp->bp_profile_name, battery_name, MAX_PROFILE_NAME_LENGTH);

	rc = of_property_read_u32(node, "qcom,batt-id-kohm", &bp->bp_batt_id);
	if (rc < 0) {
		pr_err("Failed to get battery id, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,capacity", &bp->capacity);
	if (rc < 0) {
		pr_err("Failed to get battery capacity, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,checksum", &bp->bp_checksum);
	if (rc < 0) {
		pr_err("Failed to get checksum, rc=%d\n", rc);
		return rc;
	}

	rc = qbg_parse_u32_dt_array(node, "qcom,soh-range", temp, 2);
	if (rc < 0)
		return rc;

	if (temp[0] > 100 || temp[1] > 100 || (temp[0] > temp[1])) {
		pr_err("Incorrect SOH range [%d %d]\n", temp[0],
			temp[1]);
		return -ERANGE;
	}
	bp->soh_range_low = temp[0];
	bp->soh_range_high = temp[1];

	rc = qbg_parse_u32_dt_array(node, "qcom,battery-impedance", temp, 2);
	if (rc < 0)
		return rc;

	bp->normal_impedance = temp[0];
	bp->aged_impedance = temp[1];

	rc = qbg_parse_u32_dt_array(node, "qcom,battery-capacity", temp, 2);
	if (rc < 0)
		return rc;

	bp->normal_capacity = temp[0];
	bp->aged_capacity = temp[1];

	rc = of_property_read_u32(node, "qcom,recharge-soc-delta",
					&bp->recharge_soc_delta);
	if (rc < 0) {
		pr_err("Failed to get recharege soc delta, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,recharge-vflt-delta",
					&bp->recharge_vflt_delta);
	if (rc < 0) {
		pr_err("Failed to get recharge vflt delta, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,recharge-iterm-ma",
					&bp->recharge_iterm);
	if (rc < 0) {
		pr_err("Failed to get recharge iterm, rc=%d\n", rc);
		return rc;
	}

	for_each_available_child_of_node(battery->profile_node, child) {
		if (of_node_name_prefix(child, "qcom,bp-c-table"))
			battery->num_ctables++;
		else if (of_node_name_prefix(child, "qcom,bp-d-table"))
			battery->num_dtables++;
	}

	if (!battery->num_ctables || !battery->num_dtables) {
		pr_err("ctable or dtable missing\n");
		return -EINVAL;
	}

	/* Battery profile contains additional table (table0) */
	if (battery->num_ctables)
		battery->num_ctables--;
	if (battery->num_dtables)
		battery->num_dtables--;

	battery->bp_discharge_tables = kcalloc(battery->num_dtables,
				sizeof(*battery->bp_discharge_tables), GFP_KERNEL);
	if (!battery->bp_discharge_tables)
		return -ENOMEM;

	battery->bp_charge_tables = kcalloc(battery->num_ctables,
				sizeof(*battery->bp_charge_tables), GFP_KERNEL);
	if (!battery->bp_charge_tables)
		return -ENOMEM;

	/* Parse c-table-0 */
	scnprintf(buf, sizeof(buf), "qcom,bp-c-table-0");
	rc = qbg_parse_table0(battery->profile_node, buf, &battery->table0[0]);
	if (rc < 0) {
		pr_err("Failed to parse %s, rc=%d\n", buf, rc);
		return rc;
	}

	/* Parse d-table-0 */
	scnprintf(buf, sizeof(buf), "qcom,bp-d-table-0");
	rc = qbg_parse_table0(battery->profile_node, buf, &battery->table0[1]);
	if (rc < 0) {
		pr_err("Failed to parse %s, rc=%d\n", buf, rc);
		goto cleanup_ctable;
	}

	for (i = 0; i < battery->num_dtables; i++) {
		battery->bp_discharge_tables[i] = kzalloc(
			sizeof(*battery->bp_discharge_tables[i]), GFP_KERNEL);
		if (!battery->bp_discharge_tables[i]) {
			rc = -ENOMEM;
			goto cleanup_ctable;
		}

		scnprintf(buf, sizeof(buf), "qcom,bp-d-table-%d", i + 1);

		rc = qbg_parse_table(battery->profile_node, i, buf,
					battery->bp_discharge_tables[i]);
		if (rc < 0) {
			pr_err("Failed to parse %s, rc=%d\n", buf, rc);
			goto cleanup_dtable;
		}
	}

	for (i = 0; i < battery->num_ctables; i++) {
		battery->bp_charge_tables[i] = kzalloc(
				sizeof(*battery->bp_charge_tables[i]), GFP_KERNEL);
		if (!battery->bp_charge_tables[i]) {
			rc = -ENOMEM;
			goto cleanup_ctable;
		}

		scnprintf(buf, sizeof(buf), "qcom,bp-c-table-%d", i + 1);

		rc = qbg_parse_table(battery->profile_node, i, buf,
					battery->bp_charge_tables[i]);
		if (rc < 0) {
			pr_err("Failed to parse %s, rc=%d\n", buf, rc);
			goto cleanup_ctable;
		}
	}

	return 0;

cleanup_ctable:
	for (; i > 0; i--)
		kfree(battery->bp_charge_tables[i]);
	i = battery->num_dtables;
cleanup_dtable:
	for (; i > 0; i--)
		kfree(battery->bp_discharge_tables[i]);

	return rc;
}

int qbg_batterydata_init(struct device_node *profile_node,
	struct qbg_battery_data *battery)
{
	int rc = 0;

	/* char device to access battery-profile data */
	rc = alloc_chrdev_region(&battery->dev_no, 0, 1, "qbg_battery");
	if (rc < 0) {
		pr_err("Failed to allocate chrdev, rc=%d\n", rc);
		return rc;
	}

	cdev_init(&battery->battery_cdev, &qbg_battery_data_fops);
	rc = cdev_add(&battery->battery_cdev, battery->dev_no, 1);
	if (rc) {
		pr_err("Failed to add battery_cdev, rc=%d\n", rc);
		goto unregister_chrdev;
	}

	battery->battery_class = class_create(THIS_MODULE, "qbg_battery");
	if (IS_ERR_OR_NULL(battery->battery_class)) {
		pr_err("Failed to create qbg-battery class (%d)\n",
			PTR_ERR(battery->battery_class));
		rc = -ENODEV;
		goto delete_cdev;
	}

	battery->battery_device = device_create(battery->battery_class, NULL,
						battery->dev_no, NULL,
						"qbg_battery");
	if (IS_ERR_OR_NULL(battery->battery_device)) {
		pr_err("Failed to create battery_device device (%d)\n",
			PTR_ERR(battery->battery_device));
		rc = -ENODEV;
		goto destroy_class;
	}

	battery->profile_node = profile_node;

	/* parse the battery profile */
	rc = qbg_parse_battery_profile(battery);
	if (rc < 0) {
		pr_err("Failed to parse battery profile, rc=%d\n", rc);
		goto destroy_device;
	}

	pr_info("QBG Battery-profile loaded, id:%d name:%s\n",
		battery->bp.bp_batt_id, battery->bp.bp_profile_name);

	return 0;

destroy_device:
	device_destroy(battery->battery_class, battery->dev_no);
destroy_class:
	class_destroy(battery->battery_class);
delete_cdev:
	cdev_del(&battery->battery_cdev);
unregister_chrdev:
	unregister_chrdev_region(battery->dev_no, 1);
	return rc;
}

void qbg_batterydata_exit(struct qbg_battery_data *battery)
{
	int i;

	if (!battery) {
		pr_err("Battery cannot be null\n");
		return;
	}

	/* unregister the device node */
	device_destroy(battery->battery_class, battery->dev_no);
	class_destroy(battery->battery_class);
	cdev_del(&battery->battery_cdev);
	unregister_chrdev_region(battery->dev_no, 1);

	/* delete all the battery profile memory */
	for (i = 0; i < battery->num_ctables; i++)
		kfree(battery->bp_charge_tables[i]);

	for (i = 0; i < battery->num_dtables; i++)
		kfree(battery->bp_discharge_tables[i]);
}

int qbg_lookup_soc_ocv(struct qbg_battery_data *battery, int *pon_soc, int ocv, bool charging)
{
	struct battery_data_table0 *lut;
	int i;

	*pon_soc = -EINVAL;

	lut = charging ? &battery->table0[0] : &battery->table0[1];
	for (i = 0; i < lut->ocv_length; i++) {
		if (ocv == lut->ocv[i]) {
			*pon_soc = lut->soc[i];
			break;
		} else if (is_between(lut->ocv[i], lut->ocv[i+1], ocv)) {
			*pon_soc = (lut->soc[i] + lut->soc[i+1]) / 2;
			break;
		}
	}

	if (*pon_soc == -EINVAL) {
		pr_debug("%d ocv wasn't found in the LUT returning 100%\n", ocv);
		*pon_soc = 10000;
	}

	return 0;
}
