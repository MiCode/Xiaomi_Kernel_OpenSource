/*
 * mcd_acpi.c : retrieving modem control ACPI data.
 *
 * (C) Copyright 2008 Intel Corporation
 * Author: Ben Zoubeir Mustapha <mustaphax.ben.zoubeir@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>

#include <linux/gpio.h>
#include <asm/intel-mid.h>

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include "mcd_acpi.h"


/* Modem data */
static struct mdm_ctrl_mdm_data mdm_6260 = {
	.pre_on_delay = 200,
	.on_duration = 60,
	.pre_wflash_delay = 30,
	.pre_cflash_delay = 60,
	.flash_duration = 60,
	.warm_rst_duration = 60,
	.pre_pwr_down_delay = 60,
};

static struct mdm_ctrl_mdm_data mdm_2230 = {
	.pre_on_delay = 3000,
	.on_duration = 35000,
	.pre_wflash_delay = 30,
	.pre_cflash_delay = 60,
	.flash_duration = 60,
	.warm_rst_duration = 60,
	.pre_pwr_down_delay = 60,
};

static struct mdm_ctrl_mdm_data mdm_generic = {
	.pre_on_delay = 200,
	.on_duration = 60,
	.pre_wflash_delay = 30,
	.pre_cflash_delay = 60,
	.flash_duration = 60,
	.warm_rst_duration = 60,
	.pre_pwr_down_delay = 650,
};

void *modem_data[] = {
	NULL,			/* MODEM_UNSUP */
	&mdm_2230,		/* MODEM_2230 */
	&mdm_6260,		/* MODEM_6260 */
	&mdm_generic,		/* MODEM_6268 */
	&mdm_generic,		/* MODEM_6360 */
	&mdm_generic,		/* MODEM_7160 */
	&mdm_generic		/* MODEM_7260 */
};

/*
 * Element to be read through sysfs entry
 */
static char config_name[NAME_LEN];

/*
 * config name accessor
 */
static ssize_t config_name_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", config_name);
}

/* Read-only element */
static struct kobj_attribute config_name_attribute = __ATTR_RO(config_name);

static struct attribute *mdm_attrs[] = {
	&config_name_attribute.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group mdm_attr_group = {
	.attrs = mdm_attrs,
};

static struct kobject *telephony_kobj;
static int nb_mdms;


int create_sysfs_telephony_entry(void *pdata)
{
	int retval;

	/* Creating telephony directory */
	telephony_kobj = kobject_create_and_add("telephony", kernel_kobj);
	if (!telephony_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(telephony_kobj, &mdm_attr_group);
	if (retval)
		kobject_put(telephony_kobj);

	return retval;
}

void mcd_set_mdm(struct mcd_base_info *info, int mdm_ver)
{
	if (!info) {
		pr_err("%s: info is NULL\n", __func__);
	} else {
		info->mdm_ver = mdm_ver;
		info->modem_data = modem_data[mdm_ver];
	}
}
#ifdef CONFIG_ACPI
acpi_status get_acpi_param(acpi_handle handle, int type, char *id,
			   union acpi_object **result)
{
	acpi_status status = AE_OK;
	struct acpi_buffer obj_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *out_obj;

	status = acpi_evaluate_object(handle, id, NULL, &obj_buffer);
	pr_err("%s: acpi_evaluate_object, status:%d\n", __func__, status);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR %d evaluating ID:%s\n", __func__, status, id);
		*result = NULL;
		goto error;
	}

	out_obj = obj_buffer.pointer;
	if (!out_obj || out_obj->type != type) {
		pr_err("%s: Invalid type:%d for Id:%s\n", __func__, type, id);
		status = AE_BAD_PARAMETER;
		kfree(out_obj);
		*result = NULL;
		goto error;
	} else {
		*result = out_obj;
	}

 error:
	return status;
}
#endif

/*
 * Access ACPI resources/data to populate global object mcd_reg_info
 *
 * @pdev : The platform device object to identify ACPI data.
 */
void *retrieve_acpi_modem_data(struct platform_device *pdev)
{
#ifdef CONFIG_ACPI
	struct mcd_base_info *mcd_reg_info;
	acpi_status status = AE_OK;
	acpi_handle handle;
	union acpi_object *out_obj;
	union acpi_object *item;
	struct mdm_ctrl_cpu_data *cpu_data;
	struct mdm_ctrl_pmic_data *pmic_data;
	int i;

	if (!pdev) {
		pr_err("%s: platform device is NULL.", __func__);
		return NULL;
	}

	cpu_data = kzalloc(sizeof(struct mdm_ctrl_cpu_data), GFP_KERNEL);
	if (!cpu_data) {
		pr_err("%s: can't allocate cpu_data memory\n", __func__);
		return NULL;
	}

	pmic_data = kzalloc(sizeof(struct mdm_ctrl_pmic_data), GFP_KERNEL);
	if (!pmic_data) {
		pr_err("%s: can't allocate pmic_data memory\n", __func__);
		kfree(cpu_data);
		return NULL;
	}

	/* Get ACPI handle */
	handle = ACPI_HANDLE(&pdev->dev);
	mcd_reg_info = kzalloc(sizeof(struct mcd_base_info), GFP_KERNEL);
	if (!mcd_reg_info) {
		pr_err("%s: can't allocate mcd_reg_tmp_info memory\n",
				__func__);
		goto free_mdm_info;
	}

	pr_info("%s: Getting ACPI data...\n", __func__);

	/* CPU name */
	status = get_acpi_param(handle, ACPI_TYPE_STRING, "CPU", &out_obj);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR evaluating CPU Name\n", __func__);
		goto free_mdm_info;
	}

	/* CPU Id */
	if (strstr(out_obj->string.pointer, "ValleyView2")) {
		mcd_reg_info->cpu_ver = CPU_VVIEW2;
	} else if (strstr(out_obj->string.pointer, "CherryView")) {
		mcd_reg_info->cpu_ver = CPU_CHERRYVIEW;
	} else {
		pr_err("%s: ERROR CPU name %s Not supported!\n", __func__,
		       out_obj->string.pointer);
		goto free_mdm_info;
	}
	kfree(out_obj);

	mcd_reg_info->mdm_ver = MODEM_UNSUP;

	/* Retrieve Telephony configuration name from ACPI */
	status = get_acpi_param(handle, ACPI_TYPE_STRING, "CONF", &out_obj);
	if (ACPI_FAILURE(status)) {
		/* This is a workaround to handle Baytrail FFRD8 wrong ACPI
		 * table.
		 * @TODO: remove this code once this board is no more
		 * supported */
		pr_info("CONF field is empty. Might be a Baytrail FFRD8 board");
		strncpy(config_name, "XMM7160_CONF_3", NAME_LEN - 1);
	} else {
		strncpy(config_name, out_obj->string.pointer, NAME_LEN - 1);
	}
	kfree(out_obj);

	/* PMIC */
	switch (mcd_reg_info->cpu_ver) {
	case CPU_VVIEW2:
		mcd_reg_info->pmic_ver = PMIC_BYT;
		break;
	case CPU_CHERRYVIEW:
		mcd_reg_info->pmic_ver = PMIC_CHT;
		break;
	default:
		mcd_reg_info->pmic_ver = PMIC_UNSUP;
		break;
	}

	status = get_acpi_param(handle, ACPI_TYPE_PACKAGE, "PMIC", &out_obj);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR evaluating PMIC info\n", __func__);
		goto free_mdm_info;
	}

	item = &(out_obj->package.elements[0]);
	pmic_data->chipctrl = (int)item->integer.value;
	item = &(out_obj->package.elements[1]);
	pmic_data->chipctrlon = (int)item->integer.value;
	item = &(out_obj->package.elements[2]);
	pmic_data->chipctrloff = (int)item->integer.value;
	item = &(out_obj->package.elements[3]);
	pmic_data->chipctrl_mask = (int)item->integer.value;
	pmic_data->pwr_down_duration = 20000;
	pr_info("%s: Retrieved PMIC values:Reg:%x, On:%x, Off:%x, Mask:%x\n",
		__func__, pmic_data->chipctrl, pmic_data->chipctrlon,
		pmic_data->chipctrloff, pmic_data->chipctrl_mask);
	kfree(out_obj);

	pr_info("%s: cpu info setup\n", __func__);
	/*CPU Data*/
	/* set GPIOs Names */
	cpu_data->gpio_rst_out_name = GPIO_RST_OUT;
	cpu_data->gpio_pwr_on_name = GPIO_PWR_ON;
	cpu_data->gpio_rst_bbn_name = GPIO_RST_BBN;
	cpu_data->gpio_cdump_name = GPIO_CDUMP;

	for (i = 0; i < ARRAY_SIZE(cpu_data->entries); i++)
		cpu_data->entries[i] =
			devm_gpiod_get_index(&pdev->dev, NULL, i);

	status = get_acpi_param(handle, ACPI_TYPE_PACKAGE, "EPWR", &out_obj);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR evaluating Early PWR info\n", __func__);
		goto free_mdm_info;
	}

	item = &(out_obj->package.elements[0]);
	pmic_data->early_pwr_on = (int)item->integer.value;
	item = &(out_obj->package.elements[1]);
	pmic_data->early_pwr_off = (int)item->integer.value;
	kfree(out_obj);

	mcd_reg_info->cpu_data = cpu_data;
	mcd_reg_info->pmic_data = pmic_data;

	nb_mdms = 1;

	return mcd_reg_info;

free_mdm_info:
	pr_err("%s: ERROR retrieving data from ACPI!!!\n", __func__);
	kfree(cpu_data);
	kfree(pmic_data);
	kfree(mcd_reg_info);
#endif
	return NULL;
}

int mcd_finalize_cpu_data(struct mcd_base_info *mcd_reg_info)
{
#ifdef CONFIG_ACPI
	struct mdm_ctrl_cpu_data *cpu_data = mcd_reg_info->cpu_data;
	int ret = 0;

	/* finalize cpu data */
	if (mcd_reg_info->board_type == BOARD_NGFF) {
		cpu_data->gpio_wwan_disable = cpu_data->entries[0];
		cpu_data->gpio_rst_bbn = cpu_data->entries[2];
		cpu_data->gpio_rst_usbhub = cpu_data->entries[4];

		pr_info("%s: Setup GPIOs(WWAN_Disable:%d, RB:%d, rst_usbhub:%d)\n",
				__func__,
				desc_to_gpio(cpu_data->gpio_wwan_disable),
				desc_to_gpio(cpu_data->gpio_rst_bbn),
				desc_to_gpio(cpu_data->gpio_rst_usbhub));

	} else if (mcd_reg_info->board_type == BOARD_AOB) {
		cpu_data->gpio_pwr_on = cpu_data->entries[0];
		cpu_data->gpio_cdump = cpu_data->entries[1];
		cpu_data->gpio_rst_out = cpu_data->entries[2];
		cpu_data->gpio_rst_bbn = cpu_data->entries[3];

		pr_info("%s: Setup GPIOs(PO:%d, RO:%d, RB:%d, CD:%d)\n",
				__func__,
				desc_to_gpio(cpu_data->gpio_pwr_on),
				desc_to_gpio(cpu_data->gpio_rst_out),
				desc_to_gpio(cpu_data->gpio_rst_bbn),
				desc_to_gpio(cpu_data->gpio_cdump));
	} else
		ret = -1;

	return ret;
#else
	return 0;
#endif
}

/*
 * Entry point from MCD to populate modem parameters.
 *
 * @pdev : The platform device object to identify ACPI data.
 */

int get_modem_acpi_data(struct platform_device *pdev)
{
	int ret = -ENODEV;

	struct mcd_base_info *info;
	if (!pdev) {
		pr_err("%s: platform device is NULL, aborting\n", __func__);
		return ret;
	}

	if (!ACPI_HANDLE(&pdev->dev)) {
		pr_err("%s: platform device is NOT ACPI, aborting\n", __func__);
		return ret;
	}

	pr_err("%s: Retrieving modem info from ACPI for device %s\n",
	       __func__, pdev->name);
	info = retrieve_acpi_modem_data(pdev);

	if (!info)
		return ret;

	/* Store modem parameters in platform device */
	pdev->dev.platform_data = info;

	if (telephony_kobj) {
		pr_err("%s: Unexpected entry for device named %s\n",
		       __func__, pdev->name);
		return ret;
	}

	pr_err("%s: creates sysfs entry for device named %s\n",
	       __func__, pdev->name);
	ret = create_sysfs_telephony_entry(pdev->dev.platform_data);

	return ret;
}

int get_nb_mdms(void)
{
	return nb_mdms;
}
