// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "lastbus.h"

struct plt_cfg_bus_latch *lastbus_ctrl;
static int lastbus_probe(struct platform_device *pdev);


#define NUM_INFRA_EVENT_REG (lastbus_ctrl->num_infra_event_reg)
#define NUM_PERI_EVENT_REG (lastbus_ctrl->num_peri_event_reg)

int lastbus_setup(struct plt_cfg_bus_latch *p)
{
	lastbus_ctrl = p;
	return 0;
}


static const struct of_device_id lastbus_of_ids[] = {
	{   .compatible = "mediatek,lastbus-v1", },
	{}
};

static struct platform_driver lastbus_drv = {
	.driver = {
		.name = "lastbus",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = lastbus_of_ids,
	},
	.probe = lastbus_probe,
};

static ssize_t lastbus_dump_show(struct device_driver *driver, char *buf)
{
	unsigned int wp = 0;

	if (lastbus_ctrl->perisys_ops.dump)
		wp += lastbus_ctrl->perisys_ops.dump(lastbus_ctrl, buf, &wp);

	if (lastbus_ctrl->infrasys_ops.dump)
		wp += lastbus_ctrl->infrasys_ops.dump(lastbus_ctrl, buf, &wp);

	return strlen(buf);
}

DRIVER_ATTR_RO(lastbus_dump);




static ssize_t
infra_event_store(struct device_driver *driver, const char *buf, size_t count)
{
	int ret = 0;

	if (lastbus_ctrl->infrasys_ops.set_event)
		ret = lastbus_ctrl->infrasys_ops.set_event(lastbus_ctrl, buf);

	if (ret < 0)
		pr_notice("lastbus: peri-event set error\n");

	return count;
}

static ssize_t infra_event_show(struct device_driver *driver, char *buf)
{
	ssize_t len = 0;

	if (lastbus_ctrl->infrasys_ops.get_event)
		len = lastbus_ctrl->infrasys_ops.get_event(lastbus_ctrl, buf);

	if (len < 0)
		pr_notice("lastbus: peri-event get event error\n");

	return len;
}


DRIVER_ATTR_RW(infra_event);


static ssize_t
lastbus_timeout_store(struct device_driver *driver,
	const char *buf, size_t count)
{
	int ret = 0;

	if (lastbus_ctrl->infrasys_ops.set_timeout)
		ret = lastbus_ctrl->infrasys_ops.set_timeout(lastbus_ctrl, buf);

	if (ret < 0)
		pr_notice("lastbus: set timeout error\n");

	return count;
}

static ssize_t lastbus_timeout_show(struct device_driver *driver, char *buf)
{
	ssize_t len = 0;

	if (lastbus_ctrl->infrasys_ops.get_timeout)
		len = lastbus_ctrl->infrasys_ops.get_timeout(lastbus_ctrl, buf);

	if (len < 0)
		pr_notice("lastbus: get timeout error\n");

	return len;
}


DRIVER_ATTR_RW(lastbus_timeout);


static ssize_t
peri_event_store(struct device_driver *driver,
	const char *buf, size_t count)
{
	int ret = 0;

	if (lastbus_ctrl->perisys_ops.set_event)
		ret = lastbus_ctrl->perisys_ops.set_event(lastbus_ctrl, buf);

	if (ret < 0)
		pr_notice("lastbus: peri-event set error\n");

	return count;
}

static ssize_t peri_event_show(struct device_driver *driver, char *buf)
{
	int len = 0;

	if (lastbus_ctrl->perisys_ops.get_event)
		len = lastbus_ctrl->perisys_ops.get_event(lastbus_ctrl, buf);

	if (len < 0)
		pr_notice("lastbus: peri-event get event error\n");

	return len;
}


DRIVER_ATTR_RW(peri_event);



static int lastbus_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);
	if (lastbus_ctrl->init)
		lastbus_ctrl->init(lastbus_ctrl);
	else {
		lastbus_ctrl->infra_base = of_iomap(pdev->dev.of_node, 0);
		if (!lastbus_ctrl->infra_base) {
			pr_info("can't of_iomap for infra lastbus!!\n");
			return -ENOMEM;
		}

		lastbus_ctrl->peri_base = of_iomap(pdev->dev.of_node, 1);
		if (!lastbus_ctrl->peri_base) {
			pr_info("can't of_iomap for peri lastbus!!\n");
			return -ENOMEM;
		}

		lastbus_ctrl->spm_flag_base = of_iomap(pdev->dev.of_node, 2);
		if (!lastbus_ctrl->peri_base) {
			pr_info("can't of_iomap for peri lastbus!!\n");
			return -ENOMEM;
		}
	}


	ret  = driver_create_file(&lastbus_drv.driver,
			&driver_attr_lastbus_dump);
	ret  |= driver_create_file(&lastbus_drv.driver,
			&driver_attr_infra_event);
	ret  |= driver_create_file(&lastbus_drv.driver,
			&driver_attr_peri_event);
	ret  |= driver_create_file(&lastbus_drv.driver,
			&driver_attr_lastbus_timeout);

	if (ret)
		pr_info("last bus create file failed\n");

	return 0;
}

static int __init lastbus_init(void)
{
	int err = 0;

	if (lastbus_ctrl == NULL) {
		pr_notice("kernel lastbus dump not support");
		return -1;
	}

	err = platform_driver_register(&lastbus_drv);
	if (err)
		return err;

	return 0;
}

late_initcall_sync(lastbus_init);
