/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "sprd-gnss:" fmt

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>
#include "gnss.h"
#include "sprd_wcn.h"

#define GNSS_CALI_DONE_FLAG 0x1314520

struct gnss_cali {
	bool cali_done;
	u32 *cali_data;
};
static struct gnss_cali gnss_cali_data;
static u32 *gnss_efuse_data;

int gnss_data_init(void)
{
	unsigned int gnss_cali_data_size;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		gnss_cali_data_size = M3L_GNSS_CALI_DATA_SIZE;
	else
		gnss_cali_data_size = GNSS_CALI_DATA_SIZE;

	gnss_cali_data.cali_done = false;

	pr_info("%s cali_done=%d", __func__, gnss_cali_data.cali_done);

	gnss_cali_data.cali_data = kzalloc(gnss_cali_data_size, GFP_KERNEL);
	if (gnss_cali_data.cali_data == NULL) {
		pr_err("%s malloc fail\n", __func__);
		return -ENOMEM;
	}

	gnss_efuse_data = kzalloc(GNSS_EFUSE_DATA_SIZE, GFP_KERNEL);
	if (gnss_efuse_data == NULL) {
		pr_err("%s malloc efuse data fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int gnss_write_cali_data(void)
{
	unsigned int gnss_cali_addr, gnss_cali_data_size;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3lite) {
		gnss_cali_addr = M3L_GNSS_CALI_ADDRESS;
		gnss_cali_data_size = M3L_GNSS_CALI_DATA_SIZE;
	} else {
		gnss_cali_addr = GNSS_CALI_ADDRESS;
		gnss_cali_data_size = GNSS_CALI_DATA_SIZE;
	}
	pr_info("%s flag %d\n", __func__,
		gnss_cali_data.cali_done);
	if (gnss_cali_data.cali_done) {
		sprdwcn_bus_direct_write(gnss_cali_addr,
					 gnss_cali_data.cali_data,
					 gnss_cali_data_size);
	}
	return 0;
}

static int gnss_write_efuse_data(void)
{
	unsigned int gnss_efuse_addr;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		gnss_efuse_addr = M3L_GNSS_EFUSE_ADDRESS;
	else
		gnss_efuse_addr = GNSS_EFUSE_ADDRESS;

	pr_info("%s flag %d\n", __func__, gnss_cali_data.cali_done);
	if (gnss_cali_data.cali_done && (gnss_efuse_data != NULL))
		sprdwcn_bus_direct_write(gnss_efuse_addr,
					 gnss_efuse_data,
					 GNSS_EFUSE_DATA_SIZE);

	return 0;
}

int gnss_write_data(void)
{
	int ret = 0;

	gnss_write_cali_data();
	ret = gnss_write_efuse_data();

	return ret;
}

static int gnss_backup_cali(void)
{
	int i = 15;
	int tempvalue = 0;
	unsigned int gnss_cali_addr, gnss_cali_data_size;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3lite) {
		gnss_cali_addr = M3L_GNSS_CALI_ADDRESS;
		gnss_cali_data_size = M3L_GNSS_CALI_DATA_SIZE;
	} else {
		gnss_cali_addr = GNSS_CALI_ADDRESS;
		gnss_cali_data_size = GNSS_CALI_DATA_SIZE;
	}
	pr_info("%s entry\n", __func__);

	if (!gnss_cali_data.cali_done) {
		pr_info("%s begin\n", __func__);
		if (gnss_cali_data.cali_data != NULL) {
			while (i--) {
				sprdwcn_bus_direct_read(gnss_cali_addr,
					gnss_cali_data.cali_data,
					gnss_cali_data_size);
				tempvalue = *(gnss_cali_data.cali_data);
				pr_err("cali %d time, value is 0x%x\n",
				       i, tempvalue);
				if (tempvalue != GNSS_CALI_DONE_FLAG) {
					msleep(100);
					continue;
				}
				pr_info("cali success\n");
				gnss_cali_data.cali_done = true;
				break;
			}
		}
	} else
		pr_err("no need back again\n");

	return 0;
}

static int gnss_backup_efuse(void)
{
	int ret = 1;
	unsigned int gnss_efuse_addr;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		gnss_efuse_addr = M3L_GNSS_EFUSE_ADDRESS;
	else
		gnss_efuse_addr = GNSS_EFUSE_ADDRESS;

	pr_info("%s entry\n", __func__);

	/* efuse data is ok when cali done */
	if (gnss_cali_data.cali_done && (gnss_efuse_data != NULL)) {
		sprdwcn_bus_direct_read(gnss_efuse_addr, gnss_efuse_data,
					GNSS_EFUSE_DATA_SIZE);
		ret = 0;
		pr_info("%s 0x%x\n", __func__, *gnss_efuse_data);
	} else
		pr_err("%s no need back again\n", __func__);

	return ret;
}

int gnss_backup_data(void)
{
	int ret;

	gnss_backup_cali();
	ret = gnss_backup_efuse();

	return ret;
}

int gnss_boot_wait(void)
{
	int ret = -1;
	int i = 125;
	u32 *buffer = NULL;
	unsigned int gnss_bootsts_addr;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		gnss_bootsts_addr = M3L_GNSS_BOOTSTATUS_ADDRESS;
	else
		gnss_bootsts_addr = GNSS_BOOTSTATUS_ADDRESS;

	pr_info("%s entry\n", __func__);

	buffer = kzalloc(GNSS_BOOTSTATUS_SIZE, GFP_KERNEL);
	if (buffer == NULL) {
		pr_err("%s malloc fail\n", __func__);
		return -ENOMEM;
	}
	while (i--) {
		sprdwcn_bus_direct_read(gnss_bootsts_addr, buffer,
					GNSS_BOOTSTATUS_SIZE);
		pr_err("boot read %d time,val=0x%x\n", i, *buffer);
		if (*buffer != GNSS_BOOTSTATUS_MAGIC) {
			msleep(20);
			continue;
		}
		ret = 0;
		pr_info("boot read success\n");
		break;
	}
	kfree(buffer);

	return ret;
}

void gnss_file_path_set(char *buf)
{
	pr_info("%s by wcn\n", __func__);
}

