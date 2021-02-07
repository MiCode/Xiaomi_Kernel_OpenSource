/*
 * drivers/scsi/ufs/ufs_monitor.c
 *
 * Copyright (C) 2018 Xiaomi Ltd.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Author:
 * Shane Gao<gaoshan3@xiaomi.com>
 * DongSheng <donsheng@xiaomi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <asm/hwconf_manager.h>
#include "ufs-statistics.h"
#include "ufshcd.h"

#define LIFE_STRING_SIZE 128
#define ASCII_STD true

static  struct ufs_hba *phba;

static int ufshcd_get_device_lifetime(char *tmp, int buffer_len)
{
	int err = 0;
	u8 desc_buf[QUERY_DESC_HEALTH_MAX_SIZE];

	pm_runtime_get_sync(phba->dev);
	err = ufshcd_read_health_desc(phba, desc_buf, QUERY_DESC_HEALTH_MAX_SIZE);
	pm_runtime_put_sync(phba->dev);

	if (err) {
		pr_err("ufshcd_read_health_desc fail to read lifetime\n ");
	} else {
		memset(tmp, 0, buffer_len);
		snprintf(tmp, buffer_len, "A:0x%02x;B:0x%02x", desc_buf[3], desc_buf[4]);
	}
	return err;
}

static int ufsmonitor_update(struct notifier_block *nb,
					unsigned long val, void *data)
{
	int ret = 0;
	char lifetime[LIFE_STRING_SIZE] = {0};

	ret = ufshcd_get_device_lifetime(lifetime, LIFE_STRING_SIZE);
	if (!ret) {
		update_hw_monitor_info("ufs_monitor", "LifeTime", lifetime);
		return NOTIFY_OK;
	} else {
		return ret;
	}
}

static struct notifier_block ufsmonitor_notifier = {
	.notifier_call  = ufsmonitor_update,
};



static int  ufs_export_hwmonitor(struct ufs_hba *hba)
{
	char lifetime[LIFE_STRING_SIZE] = {0};
	int err = -1;

	if (register_hw_monitor_info("ufs_monitor"))
		return err;

	ufshcd_get_device_lifetime(lifetime, LIFE_STRING_SIZE);

	add_hw_monitor_info("ufs_monitor", "LifeTime", lifetime);

	if (hw_monitor_notifier_register(&ufsmonitor_notifier))
		return err;

	return 0;
}

static void  ufs_unexport_hwmonitor(struct ufs_hba *hba)
{
	hw_monitor_notifier_unregister(&ufsmonitor_notifier);
	unregister_hw_monitor_info("ufs_monitor");
}


static int ufs_export_hwinfo(struct ufs_hba *hba)
{
	int err;
	u8 model_index;
	u8 str_desc_buf[QUERY_DESC_MAX_SIZE + 1] = {};
	u8 desc_buf[QUERY_DESC_DEVICE_DEF_SIZE] = {};
	u8 tmp[QUERY_DESC_MAX_SIZE] = {0};
	u16 device_val;

	err = ufshcd_read_device_desc(hba, desc_buf,
			QUERY_DESC_DEVICE_DEF_SIZE);
	if (err) {
		pr_err("read device_desc failed\n");
		goto out;
	}
	 if (register_hw_component_info("UFS")) {
		pr_err("register_hw_component_info failed\n");
		 goto out;
	 }

	/*ManufacturerName*/
	model_index = desc_buf[DEVICE_DESC_PARAM_MANF_NAME];
	memset(str_desc_buf, 0, QUERY_DESC_MAX_SIZE);
	err = ufshcd_read_string_desc(hba, model_index, str_desc_buf,
			QUERY_DESC_MAX_SIZE, ASCII_STD);
	if (err)
		goto out;

	str_desc_buf[QUERY_DESC_MAX_SIZE] = '\0';
	strlcpy(tmp, (str_desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
			  8));
	tmp[8] = '\0';
	add_hw_component_info("UFS", "ManufacturerName", tmp);

	/*big endian format  wmanufacturerid*/
	device_val = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];
	snprintf(tmp, QUERY_DESC_MAX_SIZE, "0x%x", device_val);
	add_hw_component_info("UFS", "wmanufacturerid", tmp);

	/*ProductName*/
	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];
	memset(str_desc_buf, 0, QUERY_DESC_MAX_SIZE);
	err = ufshcd_read_string_desc(hba, model_index, str_desc_buf,
			QUERY_DESC_MAX_SIZE, ASCII_STD);
	if (err)
		goto out;

	str_desc_buf[QUERY_DESC_MAX_SIZE] = '\0';
	strlcpy(tmp, (str_desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
		      MAX_MODEL_LEN));
	tmp[MAX_MODEL_LEN] = '\0';
	add_hw_component_info("UFS", "ProductName", tmp);


	add_hw_component_info("UFS", "SerialNumber", ufs_get_serial());
	pr_info("MMinfo:USN:%s\n", ufs_get_serial());

	/*FwVersion*/
	model_index = desc_buf[0x2A];

	memset(str_desc_buf, 0, QUERY_DESC_MAX_SIZE);
	err = ufshcd_read_string_desc(hba, model_index, str_desc_buf,
			QUERY_DESC_MAX_SIZE, ASCII_STD);
	if (err)
		goto out;

	str_desc_buf[QUERY_DESC_MAX_SIZE] = '\0';
	strlcpy(tmp, (str_desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
		      4));
	tmp[4] = '\0';
	add_hw_component_info("UFS", "FwVersion", tmp);

	/*big endian format  BCD_version*/
	device_val = desc_buf[DEVICE_DESC_PARAM_MANF_DATE] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_DATE + 1];
	memset(tmp, 0, QUERY_DESC_MAX_SIZE);
	snprintf(tmp, QUERY_DESC_MAX_SIZE, "%d", device_val);
	add_hw_component_info("UFS", "ManufactureDate", tmp);
	pr_info("MMinfo:MDT:%s\n", tmp);

	/*lifetime info*/
	ufshcd_read_health_desc(hba, desc_buf, QUERY_DESC_HEALTH_MAX_SIZE);
	snprintf(tmp, QUERY_DESC_MAX_SIZE, "A:0x%02x;B:0x%02x", desc_buf[3], desc_buf[4]);
	add_hw_component_info("UFS", "LifeTime", tmp);
	pr_info("MMinfo:A:0x%02x,B:0x%02x\n", desc_buf[3], desc_buf[4]);




out:
	return err;
}


static int ufs_unexport_hwinfo(struct ufs_hba *hba)
{
	return unregister_hw_component_info("UFS");
}


void ufs_add_statistics(struct ufs_hba *hba)
{
	if (!hba) {
		pr_err("%s: NULL hba, exiting", __func__);
		return;
	}
	phba = hba;

	ufs_export_hwinfo(hba);
	ufs_export_hwmonitor(hba);
}

void ufs_remove_statistics(struct ufs_hba *hba)
{
	if (!hba) {
		pr_err("%s: NULL hba, exiting", __func__);
		return;
	}
	phba = NULL;

	ufs_unexport_hwmonitor(hba);
	ufs_unexport_hwinfo(hba);
}
