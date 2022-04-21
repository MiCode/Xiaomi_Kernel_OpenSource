// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "bus_tracer_interface.h"

#define DRIVER_ATTR(_name, _mode, _show, _store) \
	struct driver_attribute driver_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

static const struct of_device_id bus_tracer_of_ids[] = {
	{}
};

static int bus_tracer_probe(struct platform_device *pdev);
static int bus_tracer_remove(struct platform_device *pdev);
static int bus_tracer_suspend(struct platform_device *pdev, pm_message_t state);
static int bus_tracer_resume(struct platform_device *pdev);

static char *bus_tracer_dump_buf;

static struct bus_tracer bus_tracer_drv = {
	.plt_drv = {
		.driver = {
			.name = "bus_tracer",
			.bus = &platform_bus_type,
			.owner = THIS_MODULE,
			.of_match_table = bus_tracer_of_ids,
		},
		.probe = bus_tracer_probe,
		.remove = bus_tracer_remove,
		.suspend = bus_tracer_suspend,
		.resume = bus_tracer_resume,
	},
};

static int bus_tracer_probe(struct platform_device *pdev)
{
	struct bus_tracer_plt *plt = NULL;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	plt = bus_tracer_drv.cur_plt;

	if (plt && plt->ops && plt->ops->probe)
		return plt->ops->probe(plt, pdev);

	return 0;
}

static int bus_tracer_remove(struct platform_device *pdev)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->remove)
		return plt->ops->remove(plt, pdev);

	return 0;
}

static int bus_tracer_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->suspend)
		return plt->ops->suspend(plt, pdev, state);

	return 0;
}

static int bus_tracer_resume(struct platform_device *pdev)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	pr_debug("%s:%d: enter\n", __func__, __LINE__);

	if (plt && plt->ops && plt->ops->resume)
		return plt->ops->resume(plt, pdev);

	return 0;
}

int bus_tracer_dump(char *buf, int len)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!buf) {
		pr_notice("%s:%d: buf is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->dump)
		return plt->ops->dump(plt, buf, len);

	pr_notice("no dump function implemented\n");

	return 0;
}

int bus_tracer_enable(unsigned char force_enable, unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->enable)
		return plt->ops->enable(plt, force_enable, tracer_id);

	pr_notice("no enable function implemented\n");

	return 0;
}

int bus_tracer_disable(void)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->disable)
		return plt->ops->disable(plt);

	pr_notice("no disable function implemented\n");

	return 0;
}

int bus_tracer_set_watchpoint_filter(struct watchpoint_filter f,
		unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_watchpoint_filter)
		return plt->ops->set_watchpoint_filter(plt, f, tracer_id);

	pr_notice("no set_watchpoint_filter function implemented\n");

	return 0;
}

int bus_tracer_set_bypass_filter(struct bypass_filter f, unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_bypass_filter)
		return plt->ops->set_bypass_filter(plt, f, tracer_id);

	pr_notice("no set_bypass_filter function implemented\n");

	return 0;
}

int bus_tracer_set_id_filter(struct id_filter f, unsigned int tracer_id,
		unsigned int idf_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_id_filter)
		return plt->ops->set_id_filter(plt, f, tracer_id, idf_id);

	pr_notice("no set_id_filter function implemented\n");

	return 0;
}

int bus_tracer_set_rw_filter(struct rw_filter f, unsigned int tracer_id)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->set_rw_filter)
		return plt->ops->set_rw_filter(plt, f, tracer_id);

	pr_notice("no set_rw_filter function implemented\n");

	return 0;
}

int bus_tracer_dump_setting(char *buf, int len)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->num_tracer <= 0) {
		pr_notice("%s:%d: plt->num_tracer <= 0\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops && plt->ops->dump_setting)
		return plt->ops->dump_setting(plt, buf, len);

	pr_notice("no dump_setting function implemented\n");

	return 0;
}

int bus_tracer_dump_min_len(void)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!plt->min_buf_len)
		pr_notice("%s:%d: min_buf_len is 0\n", __func__, __LINE__);

	return plt->min_buf_len;
}

int mt_bus_tracer_dump(char *buf)
{
	strncpy(buf, bus_tracer_dump_buf, strlen(bus_tracer_dump_buf)+1);
	return 0;
}

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/dump_to_buf
 * dump traces to internal buffer
 */
static ssize_t bus_tracer_dump_to_buf_show(struct device_driver *driver,
		char *buf)
{
	if (!bus_tracer_dump_buf)
		bus_tracer_dump_buf = kzalloc(
			bus_tracer_drv.cur_plt->min_buf_len, GFP_KERNEL);

	if (!bus_tracer_dump_buf)
		return -ENOMEM;

	if (bus_tracer_drv.cur_plt)
		bus_tracer_dump(bus_tracer_dump_buf,
			bus_tracer_drv.cur_plt->min_buf_len);

	return snprintf(buf, PAGE_SIZE, "copy trace to internal buffer..\n"
			"using command \"cat /proc/bus_tracer/dump_db\"\n"
			"to see the trace\n");
}

static ssize_t bus_tracer_dump_to_buf_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	return count;
}

static DRIVER_ATTR(dump_to_buf, 0664, bus_tracer_dump_to_buf_show,
		bus_tracer_dump_to_buf_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/tracer_enable
 * enable tracer from sysfs in runtime
 */
static ssize_t bus_tracer_enable_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\n"
		"echo $tracer_id $enabled  >\n"
		"/sys/bus/platform/drivers/bus_tracer/tracer_enable\n");
}

static ssize_t bus_tracer_enable_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	unsigned int tracer_id = 0, enable = 0;
	unsigned long input;
	char *p = (char *)buf, *arg;
	int ret = 0, i = 0;

	while ((arg = strsep(&p, " ")) && (i <= 1)) {
		if (kstrtoul(arg, 16, &input) != 0) {
			pr_notice("%s: kstrtoul fail for %s\n", __func__, p);
			return 0;
		}
		switch (i) {
		case 0:
			tracer_id = (unsigned int) input & 1;
			break;
		case 1:
			enable = (unsigned char) input & 1;
			break;
		default:
			break;
		}
		i++;
	}

	if (plt && plt->ops) {
		if (enable)
			ret = plt->ops->enable(plt, 1, tracer_id);
		else
			pr_notice("%s: not support for runtime disabling\n",
					__func__);

		if (ret)
			pr_notice("%s: enable failed\n", __func__);
	}

	return count;
}

static DRIVER_ATTR(tracer_enable, 0664, bus_tracer_enable_show,
		bus_tracer_enable_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/trace_recording
 * pause/resume trace recording from sysfs in runtime
 */
static ssize_t bus_tracer_recording_show(struct device_driver *driver,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage:\ninput 0 to pause recording,\n"
					"input 1 to resume.\n");
}

static ssize_t bus_tracer_recording_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	char *p = (char *)buf;
	unsigned long arg = 0;
	int ret = -1;

	if (kstrtoul(p, 10, &arg) != 0) {
		pr_notice("%s: kstrtoul fail for %s\n", __func__, p);
		return 0;
	}

	/* 0 to pause recording, 1 or other ints to resume */
	if (plt && plt->ops) {
		if (plt->ops->set_recording)
			ret = plt->ops->set_recording(plt, !(arg & 1));

		if (ret)
			pr_notice("%s: recording failed\n", __func__);
	}

	return count;
}

static DRIVER_ATTR(tracer_recording, 0664, bus_tracer_recording_show,
		bus_tracer_recording_store);

/*
 * interface: /sys/bus/platform/drivers/bus_tracer/dump_setting
 * dump current setting of bus tracers
 */
static ssize_t bus_dump_setting_show(struct device_driver *driver, char *buf)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;
	int ret = 0;

	ret = plt->ops->dump_setting(plt, buf, -1);
	if (ret)
		pr_notice("%s:%d: dump failed\n", __func__, __LINE__);

	return strlen(buf);
}

static ssize_t bus_dump_setting_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	return count;
}

static DRIVER_ATTR(dump_setting, 0664, bus_dump_setting_show,
		bus_dump_setting_store);

static int bus_tracer_start(void)
{
	struct bus_tracer_plt *plt = bus_tracer_drv.cur_plt;

	if (!plt) {
		pr_notice("%s:%d: plt is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!plt->ops) {
		pr_notice("%s:%d: ops not installed\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (plt->ops->start)
		return plt->ops->start(plt);

	return 0;
}

extern struct bus_tracer_plt *plt;
static int __init bus_tracer_init(void)
{
	int ret;
	//static struct proc_dir_entry *root_dir;

	if (!plt) {
		pr_notice("%s%d: plt is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	bus_tracer_drv.cur_plt = plt;

	ret = bus_tracer_start();
	if (ret) {
		pr_notice("%s: bus_tracer_start failed\n", __func__);
		return -ENODEV;
	}

	/* our probe would be callback after this registration */
	ret = platform_driver_register(&bus_tracer_drv.plt_drv);
	if (ret) {
		pr_notice("%s: platform_driver_register failed\n", __func__);
		return -ENODEV;
	}

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver,
			&driver_attr_dump_to_buf);
	if (ret)
		pr_notice("%s: driver_create_file failed.\n", __func__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver,
			&driver_attr_tracer_enable);
	if (ret)
		pr_notice("%s: driver_create_file failed.\n", __func__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver,
			&driver_attr_tracer_recording);
	if (ret)
		pr_notice("%s: driver_create_file failed.\n", __func__);

	ret = driver_create_file(&bus_tracer_drv.plt_drv.driver,
			&driver_attr_dump_setting);
	if (ret)
		pr_notice("%s: driver_create_file failed.\n", __func__);

	bus_tracer_dump_buf = kzalloc(bus_tracer_drv.cur_plt->min_buf_len,
			GFP_KERNEL);
	if (!bus_tracer_dump_buf)
		return -ENOMEM;

	/* force_enable=0 to enable all the tracers with enabled=1 */
	bus_tracer_enable(0, -1);
	pr_notice("%s: running...\n", __func__);

	return 0;
}

static void __exit bus_tracer_exit(void)
{
	pr_notice("%s%d: done\n", __func__, __LINE__);
}

module_init(bus_tracer_init);
module_exit(bus_tracer_exit);

MODULE_LICENSE("GPL v2");

