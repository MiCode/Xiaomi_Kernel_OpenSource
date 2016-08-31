/*
 * arch/arm/mach-tegra/nct_sysfs.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/crc32.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <mach/nct.h>
#include "board.h"

static struct kobject *nct_kobj;
static ssize_t nct_item_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);


static const struct kobj_attribute serial_number_attr =
	__ATTR(serial_number, 0444, nct_item_show, 0);
static const struct kobj_attribute wifi_mac_addr_attr =
	__ATTR(wifi_mac_addr, 0444, nct_item_show, 0);
static const struct kobj_attribute bt_addr_attr =
	__ATTR(bt_addr, 0444, nct_item_show, 0);
static const struct kobj_attribute cm_id_attr =
	__ATTR(cm_id, 0444, nct_item_show, 0);
static const struct kobj_attribute lbh_id_attr =
	__ATTR(lbh_id, 0444, nct_item_show, 0);
static const struct kobj_attribute boardinfo_id_attr =
	__ATTR(boardinfo_id, 0444, nct_item_show, 0);
static const struct kobj_attribute gps_id_attr =
	__ATTR(gps_id, 0444, nct_item_show, 0);
static const struct kobj_attribute lcd_id_attr =
	__ATTR(lcd_id, 0444, nct_item_show, 0);
static const struct kobj_attribute accelerometer_id_attr =
	__ATTR(accelerometer_id, 0444, nct_item_show, 0);
static const struct kobj_attribute compass_id_attr =
	__ATTR(compass_id, 0444, nct_item_show, 0);
static const struct kobj_attribute gyroscope_id_attr =
	__ATTR(gyroscope_id, 0444, nct_item_show, 0);
static const struct kobj_attribute light_id_attr =
	__ATTR(light_id, 0444, nct_item_show, 0);
static const struct kobj_attribute charger_id_attr =
	__ATTR(charger_id, 0444, nct_item_show, 0);
static const struct kobj_attribute touch_id_attr =
	__ATTR(touch_id, 0444, nct_item_show, 0);

static const struct attribute *nct_item_attrs[] = {
	&serial_number_attr.attr,
	&wifi_mac_addr_attr.attr,
	&bt_addr_attr.attr,
	&cm_id_attr.attr,
	&lbh_id_attr.attr,
	&boardinfo_id_attr.attr,
	&gps_id_attr.attr,
	&lcd_id_attr.attr,
	&accelerometer_id_attr.attr,
	&compass_id_attr.attr,
	&gyroscope_id_attr.attr,
	&light_id_attr.attr,
	&charger_id_attr.attr,
	&touch_id_attr.attr,
	NULL
};

static ssize_t nct_item_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	ssize_t rval = 0;
	union nct_item_type item;
	int err;

	if (attr == &serial_number_attr) {
		err = tegra_nct_read_item(NCT_ID_SERIAL_NUMBER, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%s\n", item.serial_number.sn);
	} else if (attr == &wifi_mac_addr_attr) {
		err = tegra_nct_read_item(NCT_ID_WIFI_MAC_ADDR, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			item.wifi_mac_addr.addr[0],
			item.wifi_mac_addr.addr[1],
			item.wifi_mac_addr.addr[2],
			item.wifi_mac_addr.addr[3],
			item.wifi_mac_addr.addr[4],
			item.wifi_mac_addr.addr[5]);
	} else if (attr == &bt_addr_attr) {
		err = tegra_nct_read_item(NCT_ID_BT_ADDR, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			item.bt_addr.addr[0],
			item.bt_addr.addr[1],
			item.bt_addr.addr[2],
			item.bt_addr.addr[3],
			item.bt_addr.addr[4],
			item.bt_addr.addr[5]);
	} else if (attr == &cm_id_attr) {
		err = tegra_nct_read_item(NCT_ID_CM_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.cm_id.id);
	} else if (attr == &lbh_id_attr) {
		err = tegra_nct_read_item(NCT_ID_LBH_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.lbh_id.id);
	} else if (attr == &boardinfo_id_attr) {
		err = tegra_nct_read_item(NCT_ID_BOARD_INFO, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf,
			"Proc: %4u (sku: %u, fab: %u)\n"
			"PMU : %4u (sku: %u, fab: %u)\n"
			"Disp: %4u (sku: %u, fab: %u)\n",
			item.board_info.proc_board_id,
			item.board_info.proc_sku,
			item.board_info.proc_fab,
			item.board_info.pmu_board_id,
			item.board_info.pmu_sku,
			item.board_info.pmu_fab,
			item.board_info.display_board_id,
			item.board_info.display_sku,
			item.board_info.display_fab);
	} else if (attr == &gps_id_attr) {
		err = tegra_nct_read_item(NCT_ID_GPS_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.gps_id.id);
	} else if (attr == &lcd_id_attr) {
		err = tegra_nct_read_item(NCT_ID_LCD_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.lcd_id.id);
	} else if (attr == &accelerometer_id_attr) {
		err = tegra_nct_read_item(NCT_ID_ACCELEROMETER_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.accelerometer_id.id);
	} else if (attr == &compass_id_attr) {
		err = tegra_nct_read_item(NCT_ID_COMPASS_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.compass_id.id);
	} else if (attr == &gyroscope_id_attr) {
		err = tegra_nct_read_item(NCT_ID_GYROSCOPE_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.gyroscope_id.id);
	} else if (attr == &light_id_attr) {
		err = tegra_nct_read_item(NCT_ID_LIGHT_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.light_id.id);
	} else if (attr == &charger_id_attr) {
		err = tegra_nct_read_item(NCT_ID_CHARGER_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.charger_id.id);
	} else if (attr == &touch_id_attr) {
		err = tegra_nct_read_item(NCT_ID_TOUCH_ID, &item);
		if (err < 0)
			return 0;
		rval = sprintf(buf, "%04d\n", item.touch_id.id);
	}

	return rval;
}

static int __init tegra_nct_sysfs_init(void)
{
	if (!tegra_nct_is_init()) {
		pr_err("tegra_nct: not initialized\n");
		return 0;
	}

	nct_kobj = kobject_create_and_add("tegra_nct", kernel_kobj);
	if (!nct_kobj) {
		pr_err("tegra_nct: failed to create sysfs nct object\n");
		return 0;
	}

	if (sysfs_create_files(nct_kobj, nct_item_attrs)) {
		pr_err("%s: failed to create nct item sysfs files\n", __func__);
		kobject_del(nct_kobj);
		nct_kobj = 0;
	}

	return 0;
}

late_initcall(tegra_nct_sysfs_init);
