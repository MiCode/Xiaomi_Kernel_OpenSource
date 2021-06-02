 /*
  * Goodix Touchscreen Driver
  * Core layer of touchdriver architecture.
  *
  * Copyright (C) 2019 - 2020 Goodix, Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be a reference
  * to you, when you are integrating the GOODiX's CTP IC into your system,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * General Public License for more details.
  *
  */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include "goodix_ts_core.h"
#include <linux/spi/spi.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#define GOOIDX_INPUT_PHYS	"goodix_ts/input0"

struct gt9896s_module gt9896s_modules;


/*put resume in workqueue to screen on*/
static struct gt9896s_ts_core *resume_core_data;
static struct work_struct touch_resume_work;
static struct workqueue_struct *touch_resume_workqueue;
static int touch_suspend_flag;

/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 */
static void  __do_register_ext_module(struct work_struct *work)
{
	struct gt9896s_ext_module *module =
			container_of(work, struct gt9896s_ext_module, work);
	struct gt9896s_ext_module *ext_module, *next;
	struct list_head *insert_point = &gt9896s_modules.head;

	ts_info("__do_register_ext_module IN");

	if (gt9896s_modules.core_data &&
	    !gt9896s_modules.core_data->initialized) {
		ts_err("core layer has exit");
		return;
	}

	if (!gt9896s_modules.core_data) {
		/* waitting for core layer */
		if (!wait_for_completion_timeout(&gt9896s_modules.core_comp,
						 25 * HZ)) {
			ts_err("Module [%s] timeout", module->name);
			return;
		}
	}

	/* driver probe failed */
	if (!gt9896s_modules.core_data ||
	    !gt9896s_modules.core_data->initialized) {
		ts_err("Can't register ext_module core error");
		return;
	}

	ts_info("start register ext_module");

	/* prority level *must* be set */
	if (module->priority == EXTMOD_PRIO_RESERVED) {
		ts_err("Priority of module [%s] needs to be set",
		       module->name);
		return;
	}

	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			if (ext_module == module) {
				ts_info("Module [%s] already exists",
					module->name);
				mutex_unlock(&gt9896s_modules.mutex);
				return;
			}
		}

		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			/* small value of priority have
			 * higher priority level
			 */
			if (ext_module->priority >= module->priority) {
				insert_point = &ext_module->list;
				break;
			}
		}
	}

	if (module->funcs && module->funcs->init) {
		if (module->funcs->init(gt9896s_modules.core_data,
					module) < 0) {
			ts_err("Module [%s] init error",
			       module->name ? module->name : " ");
			mutex_unlock(&gt9896s_modules.mutex);
			return;
		}
	}

	list_add(&module->list, insert_point->prev);
	gt9896s_modules.count++;
	mutex_unlock(&gt9896s_modules.mutex);

	ts_info("Module [%s] registered,priority:%u",
		module->name,
		module->priority);
}

/**
 * gt9896s_register_ext_module - interface for external module
 * to register into touch core modules structure
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int gt9896s_register_ext_module(struct gt9896s_ext_module *module)
{
	if (!module)
		return -EINVAL;

	if (!gt9896s_modules.initilized) {
		gt9896s_modules.initilized = true;
		INIT_LIST_HEAD(&gt9896s_modules.head);
		mutex_init(&gt9896s_modules.mutex);
		init_completion(&gt9896s_modules.core_comp);
	}

	ts_info("gt9896s_register_ext_module IN");

	INIT_WORK(&module->work, __do_register_ext_module);
	schedule_work(&module->work);

	ts_info("gt9896s_register_ext_module OUT");

	return 0;
}
EXPORT_SYMBOL_GPL(gt9896s_register_ext_module);

/**
 * gt9896s_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int gt9896s_unregister_ext_module(struct gt9896s_ext_module *module)
{
	struct gt9896s_ext_module *ext_module, *next;
	bool found = false;

	if (!module)
		return -EINVAL;

	if (!gt9896s_modules.initilized)
		return -EINVAL;

	if (!gt9896s_modules.core_data)
		return -ENODEV;

	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			if (ext_module == module) {
				found = true;
				break;
			}
		}
	} else {
		mutex_unlock(&gt9896s_modules.mutex);
		return -EFAULT;
	}

	if (!found) {
		ts_err("Module [%s] never registed",
				module->name);
		mutex_unlock(&gt9896s_modules.mutex);
		return -EFAULT;
	}

	list_del(&module->list);
	gt9896s_modules.count--;
	mutex_unlock(&gt9896s_modules.mutex);

	if (module->funcs && module->funcs->exit)
		module->funcs->exit(gt9896s_modules.core_data, module);

	ts_info("Moudle [%s] unregistered",
		module->name ? module->name : " ");
	return 0;
}
EXPORT_SYMBOL_GPL(gt9896s_unregister_ext_module);

static void gt9896s_remove_all_ext_modules(void)
{
	struct gt9896s_ext_module *ext_module, *next;

	if (!gt9896s_modules.initilized || !gt9896s_modules.core_data)
		return;

	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			list_del(&ext_module->list);
			gt9896s_modules.count--;
			if (ext_module->funcs && ext_module->funcs->exit)
				ext_module->funcs->exit(gt9896s_modules.core_data,
							ext_module);
		}
	}

	mutex_unlock(&gt9896s_modules.mutex);
}

static void gt9896s_ext_sysfs_release(struct kobject *kobj)
{
	ts_info("Kobject released!");
}

#define to_ext_module(kobj)	container_of(kobj,\
				struct gt9896s_ext_module, kobj)
#define to_ext_attr(attr)	container_of(attr,\
				struct gt9896s_ext_attribute, attr)

static ssize_t gt9896s_ext_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct gt9896s_ext_module *module = to_ext_module(kobj);
	struct gt9896s_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->show)
		return ext_attr->show(module, buf);

	return -EIO;
}

static ssize_t gt9896s_ext_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct gt9896s_ext_module *module = to_ext_module(kobj);
	struct gt9896s_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->store)
		return ext_attr->store(module, buf, count);

	return -EIO;
}

static const struct sysfs_ops gt9896s_ext_ops = {
	.show = gt9896s_ext_sysfs_show,
	.store = gt9896s_ext_sysfs_store
};

static struct kobj_type gt9896s_ext_ktype = {
	.release = gt9896s_ext_sysfs_release,
	.sysfs_ops = &gt9896s_ext_ops,
};

struct kobj_type *gt9896s_get_default_ktype(void)
{
	return &gt9896s_ext_ktype;
}
EXPORT_SYMBOL_GPL(gt9896s_get_default_ktype);

struct kobject *gt9896s_get_default_kobj(void)
{
	struct kobject *kobj = NULL;

	if (gt9896s_modules.core_data &&
			gt9896s_modules.core_data->pdev)
		kobj = &gt9896s_modules.core_data->pdev->dev.kobj;
	return kobj;
}
EXPORT_SYMBOL_GPL(gt9896s_get_default_kobj);

/* debug fs */
struct debugfs_buf {
	struct debugfs_blob_wrapper buf;
	int pos;
	struct dentry *dentry;
} gt9896s_dbg;

void gt9896s_msg_printf(const char *fmt, ...)
{
	va_list args;
	int r;

	if (!gt9896s_dbg.dentry)
		return;
	if (gt9896s_dbg.pos < gt9896s_dbg.buf.size) {
		va_start(args, fmt);
		r = vscnprintf(gt9896s_dbg.buf.data + gt9896s_dbg.pos,
			       gt9896s_dbg.buf.size - 1, fmt, args);
		gt9896s_dbg.pos += r;
		va_end(args);
	}
}
EXPORT_SYMBOL_GPL(gt9896s_msg_printf);

static int gt9896s_debugfs_init(void)
{
	struct dentry *r_b;

	gt9896s_dbg.buf.size = PAGE_SIZE;
	gt9896s_dbg.pos = 0;
	gt9896s_dbg.buf.data = kzalloc(gt9896s_dbg.buf.size, GFP_KERNEL);
	if (gt9896s_dbg.buf.data == NULL) {
		ts_err("Debugfs init failed\n");
		goto exit;
	}
	r_b = debugfs_create_blob("gt9896s_ts", 0644, NULL, &gt9896s_dbg.buf);
	if (!r_b) {
		ts_err("Debugfs create failed\n");
		return -ENOENT;
	}
	gt9896s_dbg.dentry = r_b;

exit:
	return 0;
}

static void gt9896s_debugfs_exit(void)
{
	debugfs_remove(gt9896s_dbg.dentry);
	gt9896s_dbg.dentry = NULL;
	pr_info("Debugfs module exit\n");
}

/* show external module infomation */
static ssize_t gt9896s_ts_extmod_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gt9896s_ext_module *module, *next;
	size_t offset = 0;
	int r;

	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(module, next,
					 &gt9896s_modules.head, list) {
			r = snprintf(&buf[offset], PAGE_SIZE,
				     "priority:%u module:%s\n",
				     module->priority, module->name);
			if (r < 0) {
				mutex_unlock(&gt9896s_modules.mutex);
				return -EINVAL;
			}
			offset += r;
		}
	}

	mutex_unlock(&gt9896s_modules.mutex);
	return offset;
}

/* show driver infomation */
static ssize_t gt9896s_ts_driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DriverVersion:%s\n",
			GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
static ssize_t gt9896s_ts_chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct gt9896s_ts_core *core_data =
		dev_get_drvdata(dev);
	struct gt9896s_ts_device *ts_dev = core_data->ts_dev;
	struct gt9896s_ts_version chip_ver;
	int r, cnt = 0;

	cnt += snprintf(buf, PAGE_SIZE, "TouchDeviceName:%s\n", ts_dev->name);
	if (ts_dev->hw_ops->read_version) {
		r = ts_dev->hw_ops->read_version(ts_dev, &chip_ver);
		if (!r && chip_ver.valid) {
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"PID:%s\nVID:%02x%02x%02x%02x\nSensID:%02x\n",
				chip_ver.pid, chip_ver.vid[0],
				chip_ver.vid[1], chip_ver.vid[2],
				chip_ver.vid[3], chip_ver.sensor_id);
		}
	}

	return cnt;
}

/* reset chip */
static ssize_t gt9896s_ts_reset_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct gt9896s_ts_core *core_data = dev_get_drvdata(dev);
	struct gt9896s_ts_device *ts_dev = core_data->ts_dev;
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	if (ts_dev->hw_ops->reset)
		ts_dev->hw_ops->reset(ts_dev);
	return count;

}

static ssize_t gt9896s_ts_read_cfg_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct gt9896s_ts_core *core_data = dev_get_drvdata(dev);
	struct gt9896s_ts_device *ts_dev = core_data->ts_dev;
	int ret, i, offset;
	char *cfg_buf;

	cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cfg_buf)
		return -ENOMEM;

	if (ts_dev->hw_ops->read_config)
		ret = ts_dev->hw_ops->read_config(ts_dev, cfg_buf);
	else
		ret = -EINVAL;

	if (ret > 0) {
		offset = 0;
		for (i = 0; i < ret; i++) {
			if (i != 0 && i % 20 == 0)
				buf[offset++] = '\n';
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					   "%02x,", cfg_buf[i]);
		}
	}
	kfree(cfg_buf);
	if (ret <= 0)
		return ret;

	return offset;
}

static u8 ascii2hex(u8 a)
{
	s8 value = 0;

	if (a >= '0' && a <= '9')
		value = a - '0';
	else if (a >= 'A' && a <= 'F')
		value = a - 'A' + 0x0A;
	else if (a >= 'a' && a <= 'f')
		value = a - 'a' + 0x0A;
	else
		value = 0xff;

	return value;
}

static int gt9896s_ts_convert_0x_data(const u8 *buf, int buf_size,
				     unsigned char *out_buf, int *out_buf_len)
{
	int i, m_size = 0;
	int temp_index = 0;
	u8 high, low;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X')
			m_size++;
	}

	if (m_size <= 1) {
		ts_err("cfg file ERROR, valid data count:%d\n", m_size);
		return -EINVAL;
	}
	*out_buf_len = m_size;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] != 'x' && buf[i] != 'X')
			continue;

		if (temp_index >= m_size) {
			ts_err("exchange cfg data error, overflow,"
			       "temp_index:%d,m_size:%d\n",
			       temp_index, m_size);
			return -EINVAL;
		}
		high = ascii2hex(buf[i + 1]);
		low = ascii2hex(buf[i + 2]);
		if (high == 0xff || low == 0xff) {
			ts_err("failed convert: 0x%x, 0x%x",
				buf[i + 1], buf[i + 2]);
			return -EINVAL;
		}
		out_buf[temp_index++] = (high << 4) + low;
	}
	return 0;
}

static ssize_t gt9896s_ts_send_cfg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct gt9896s_ts_core *core_data = dev_get_drvdata(dev);
	struct gt9896s_ts_device *ts_dev = core_data->ts_dev;
	int en, r;
	const struct firmware *cfg_img = NULL;
	struct gt9896s_ts_config *config = NULL;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	disable_irq(core_data->irq);

	/*request configuration*/
	r = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
	if (r < 0) {
		ts_err("cfg file [%s] not available,errno:%d",
		       GOODIX_DEFAULT_CFG_NAME, r);
		goto exit;
	} else
		ts_info("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);

	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (config == NULL)
		goto exit;

	/*parse cfg data*/
	if (gt9896s_ts_convert_0x_data(cfg_img->data, cfg_img->size,
				      config->data, &config->length)) {
		ts_err("convert config data FAILED");
		goto exit;
	}

	config->reg_base = ts_dev->reg.cfg_addr;
	mutex_init(&config->lock);
	config->initialized = TS_CFG_STABLE;

	if (ts_dev->hw_ops->send_config)
		ts_dev->hw_ops->send_config(ts_dev, config);

exit:
	enable_irq(core_data->irq);
	kfree(config);
	config = NULL;
	if (cfg_img) {
		release_firmware(cfg_img);
		cfg_img = NULL;
	}

	return count;
}

/* show irq infomation */
static ssize_t gt9896s_ts_irq_info_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct gt9896s_ts_core *core_data = dev_get_drvdata(dev);
	struct irq_desc *desc;
	size_t offset = 0;
	int r;

	r = snprintf(&buf[offset], PAGE_SIZE, "irq:%u\n", core_data->irq);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "state:%s\n",
		     atomic_read(&core_data->irq_enabled) ?
		     "enabled" : "disabled");
	if (r < 0)
		return -EINVAL;

	desc = irq_to_desc(core_data->irq);
	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "disable-depth:%d\n",
		     desc->depth);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "trigger-count:%zu\n",
		core_data->irq_trig_cnt);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset,
		     "echo 0/1 > irq_info to disable/enable irq");
	if (r < 0)
		return -EINVAL;

	offset += r;
	return offset;
}

/* enable/disable irq */
static ssize_t gt9896s_ts_irq_info_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct gt9896s_ts_core *core_data = dev_get_drvdata(dev);
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	gt9896s_ts_irq_enable(core_data, en);
	return count;
}

static DEVICE_ATTR(extmod_info, S_IRUGO, gt9896s_ts_extmod_show, NULL);
static DEVICE_ATTR(driver_info, S_IRUGO, gt9896s_ts_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, S_IRUGO, gt9896s_ts_chip_info_show, NULL);
static DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, gt9896s_ts_reset_store);
static DEVICE_ATTR(send_cfg, S_IWUSR | S_IWGRP, NULL, gt9896s_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, S_IRUGO, gt9896s_ts_read_cfg_show, NULL);
static DEVICE_ATTR(irq_info, S_IRUGO | S_IWUSR | S_IWGRP,
		   gt9896s_ts_irq_info_show, gt9896s_ts_irq_info_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_extmod_info.attr,
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_irq_info.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static ssize_t gt9896s_sysfs_config_write(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t count)
{
	struct platform_device *pdev = container_of(kobj_to_dev(kobj),
				struct platform_device, dev);
	struct gt9896s_ts_core *ts_core = platform_get_drvdata(pdev);
	struct gt9896s_ts_device *ts_dev = ts_core->ts_dev;
	struct gt9896s_ts_config *config = NULL;
	int ret;

	if (pos != 0 || count > GOODIX_CFG_MAX_SIZE) {
		ts_info("pos(%d) != 0, cfg size %zu", (int)pos, count);
		return -EINVAL;
	}

	config = kzalloc(sizeof(struct gt9896s_ts_config), GFP_KERNEL);
	if (config == NULL)
		return -ENOMEM;

	memcpy(config->data, buf, count);
	config->length = count;
	config->reg_base = ts_dev->reg.cfg_addr;
	mutex_init(&config->lock);
	config->initialized = true;

	ret = ts_dev->hw_ops->send_config(ts_dev, config);
	if (ret) {
		count = -EINVAL;
		ts_err("send config failed %d", ret);
	} else {
		ts_info("send config success");
	}

	kfree(config);
	return count;
}

static ssize_t gt9896s_sysfs_config_read(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t size)
{
	struct platform_device *pdev = container_of(kobj_to_dev(kobj),
				struct platform_device, dev);
	struct gt9896s_ts_core *ts_core = platform_get_drvdata(pdev);
	struct gt9896s_ts_device *ts_dev = ts_core->ts_dev;
	int ret;

	ts_debug("pos = %d, size = %zu", (int)pos, size);

	if (pos != 0)
		return 0;

	if (ts_dev->hw_ops->read_config)
		ret = ts_dev->hw_ops->read_config(ts_dev, buf);
	else
		ret = -EINVAL;

	ts_debug("read config ret %d", ret);
	return ret;
}

static struct bin_attribute gt9896s_config_bin_attr = {
	.attr = {
		.name = "config_bin",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP,
	},
	.size = GOODIX_CFG_MAX_SIZE,
	.read = gt9896s_sysfs_config_read,
	.write = gt9896s_sysfs_config_write,
};

static int gt9896s_ts_sysfs_init(struct gt9896s_ts_core *core_data)
{
	int ret;

	ret = sysfs_create_bin_file(&core_data->pdev->dev.kobj,
				    &gt9896s_config_bin_attr);
	if (ret) {
		ts_err("failed create config bin attr");
		return ret;
	}

	ret = sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
	if (ret) {
		ts_err("failed create core sysfs group");
		sysfs_remove_bin_file(&core_data->pdev->dev.kobj,
				      &gt9896s_config_bin_attr);
		return ret;
	}

	return ret;
}

static void gt9896s_ts_sysfs_exit(struct gt9896s_ts_core *core_data)
{
	sysfs_remove_bin_file(&core_data->pdev->dev.kobj,
			      &gt9896s_config_bin_attr);
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

/* event notifier */
static BLOCKING_NOTIFIER_HEAD(ts_notifier_list);
/**
 * gt9896s_ts_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *  see enum ts_notify_event in goodix_ts_core.h
 */
int gt9896s_ts_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ts_notifier_list, nb);
}
EXPORT_SYMBOL(gt9896s_ts_register_notifier);

/**
 * gt9896s_ts_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int gt9896s_ts_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_notifier_list, nb);
}
EXPORT_SYMBOL(gt9896s_ts_unregister_notifier);

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int gt9896s_ts_blocking_notify(enum ts_notify_event evt, void *v)
{
	int ret;

	ret = blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
	return ret;
}
EXPORT_SYMBOL_GPL(gt9896s_ts_blocking_notify);

static void gt9896s_ts_report_pen(struct input_dev *dev,
		struct gt9896s_pen_data *pen_data)
{
	int i;

	if (pen_data->coords.status == TS_TOUCH) {
		input_report_key(dev, BTN_TOUCH, 1);
		input_report_key(dev, pen_data->coords.tool_type, 1);
	} else if (pen_data->coords.status == TS_RELEASE) {
		input_report_key(dev, BTN_TOUCH, 0);
		input_report_key(dev, pen_data->coords.tool_type, 0);
	}
	if (pen_data->coords.status) {
		input_report_abs(dev, ABS_X, pen_data->coords.x);
		input_report_abs(dev, ABS_Y, pen_data->coords.y);
		input_report_abs(dev, ABS_PRESSURE, pen_data->coords.p);
	}
	/* report pen button */
	for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
		if (!pen_data->keys[i].status)
			continue;
		if (pen_data->keys[i].status == TS_TOUCH)
			input_report_key(dev, pen_data->keys[i].code, 1);
		else if (pen_data->keys[i].status == TS_RELEASE)
			input_report_key(dev, pen_data->keys[i].code, 0);
	}
	input_sync(dev);
}

static void gt9896s_ts_report_finger(struct input_dev *dev,
		struct gt9896s_touch_data *touch_data)
{
	unsigned int touch_num = touch_data->touch_num;
	static u32 pre_fin;
	int i;

	/*first touch down and last touch up condition*/
	if (touch_num && !pre_fin)
		input_report_key(dev, BTN_TOUCH, 1);
	else if (!touch_num && pre_fin)
		input_report_key(dev, BTN_TOUCH, 0);

	pre_fin = touch_num;

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (!touch_data->coords[i].status)
			continue;
		if (touch_data->coords[i].status == TS_RELEASE) {
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
			continue;
		}

		input_mt_slot(dev, i);
		input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
		input_report_abs(dev, ABS_MT_POSITION_X,
				 touch_data->coords[i].x);
		input_report_abs(dev, ABS_MT_POSITION_Y,
				 touch_data->coords[i].y);
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR,
				 touch_data->coords[i].w);
	}

	/* report panel key */
	for (i = 0; i < GOODIX_MAX_TP_KEY; i++) {
		if (!touch_data->keys[i].status)
			continue;
		if (touch_data->keys[i].status == TS_TOUCH)
			input_report_key(dev, touch_data->keys[i].code, 1);
		else if (touch_data->keys[i].status == TS_RELEASE)
			input_report_key(dev, touch_data->keys[i].code, 0);
	}
	input_sync(dev);
}

/**
 * gt9896s_ts_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static irqreturn_t gt9896s_ts_threadirq_func(int irq, void *data)
{
	struct gt9896s_ts_core *core_data = data;
	struct gt9896s_ts_device *ts_dev =  core_data->ts_dev;
	struct gt9896s_ext_module *ext_module, *next;
	struct gt9896s_ts_event *ts_event = &core_data->ts_event;
	u8 irq_flag = 0;
	int r;

	core_data->irq_trig_cnt++;
	/* inform external module */
	mutex_lock(&gt9896s_modules.mutex);
	list_for_each_entry_safe(ext_module, next,
				 &gt9896s_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		r = ext_module->funcs->irq_event(core_data, ext_module);
		if (r == EVT_CANCEL_IRQEVT) {
			mutex_unlock(&gt9896s_modules.mutex);
			return IRQ_HANDLED;
		}
	}
	mutex_unlock(&gt9896s_modules.mutex);

	/* read touch data from touch device */
	r = ts_dev->hw_ops->event_handler(ts_dev, ts_event);
	if (likely(r >= 0)) {
		if (ts_event->event_type == EVENT_TOUCH) {
			/* report touch */
			gt9896s_ts_report_finger(core_data->input_dev,
					&ts_event->touch_data);
		}
		if (ts_dev->board_data.pen_enable &&
			ts_event->event_type == EVENT_PEN) {
			gt9896s_ts_report_pen(core_data->pen_dev,
					&ts_event->pen_data);
		}
	}

	/* clean irq flag */
	irq_flag = 0;
	ts_dev->hw_ops->write_trans(ts_dev, ts_dev->reg.coor, &irq_flag, 1);

	return IRQ_HANDLED;
}

/**
 * gt9896s_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int gt9896s_ts_irq_setup(struct gt9896s_ts_core *core_data)
{
	const struct gt9896s_ts_board_data *ts_bdata = board_data(core_data);
	int r;

	/* if ts_bdata-> irq is invalid */
	if (ts_bdata->irq <= 0)
		core_data->irq = gpio_to_irq(ts_bdata->irq_gpio);
	else
		core_data->irq = ts_bdata->irq;

	ts_info("IRQ:%u,flags:%d", core_data->irq, (int)ts_bdata->irq_flags);
	r = devm_request_threaded_irq(&core_data->pdev->dev,
				      core_data->irq, NULL,
				      gt9896s_ts_threadirq_func,
				      ts_bdata->irq_flags | IRQF_ONESHOT,
				      GOODIX_CORE_DRIVER_NAME,
				      core_data);
	if (r < 0)
		ts_err("Failed to requeset threaded irq:%d", r);
	else
		atomic_set(&core_data->irq_enabled, 1);

	return r;
}

/**
 * gt9896s_ts_irq_enable - Enable/Disable a irq
 * @core_data: pointer to touch core data
 * enable: enable or disable irq
 * return: 0 ok, <0 failed
 */
int gt9896s_ts_irq_enable(struct gt9896s_ts_core *core_data,
			bool enable)
{
	if (enable) {
		if (!atomic_cmpxchg(&core_data->irq_enabled, 0, 1)) {
			enable_irq(core_data->irq);
			ts_debug("Irq enabled");
		}
	} else {
		if (atomic_cmpxchg(&core_data->irq_enabled, 1, 0)) {
			disable_irq(core_data->irq);
			ts_debug("Irq disabled");
		}
	}

	return 0;
}
EXPORT_SYMBOL(gt9896s_ts_irq_enable);

/**
 * gt9896s_ts_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int gt9896s_ts_power_init(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ts_board_data *ts_bdata;
	struct device *dev = NULL;
	int r = 0;

	ts_info("Power init");
	/* dev:i2c client device or spi slave device*/
	dev =  core_data->ts_dev->dev;
	ts_bdata = board_data(core_data);

	if (strlen(ts_bdata->avdd_name)) {
		ts_info("avdd name is %s!\n", ts_bdata->avdd_name);
		core_data->avdd = devm_regulator_get(dev,
				 ts_bdata->avdd_name);
		if (IS_ERR_OR_NULL(core_data->avdd)) {
			r = PTR_ERR(core_data->avdd);
			ts_err("Failed to get regulator avdd:%d", r);
			core_data->avdd = NULL;
			return r;
		}
		r = regulator_set_voltage(core_data->avdd, 3000000, 3000000);
		if (r) {
			ts_err("regulator_set_voltage failed %d\n", r);
			return r;
		}
	} else {
		ts_info("Avdd name is NULL");
	}

	return r;
}

/**
 * gt9896s_ts_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int gt9896s_ts_power_on(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ts_board_data *ts_bdata =
			board_data(core_data);
	int r;

	ts_info("Device power on");
	if (core_data->power_on)
		return 0;

	if (!core_data->avdd) {
		core_data->power_on = 1;
		return 0;
	}

	r = regulator_enable(core_data->avdd);
	if (!r) {
		ts_info("regulator enable SUCCESS");
		if (ts_bdata->power_on_delay_us)
			usleep_range(ts_bdata->power_on_delay_us,
				     ts_bdata->power_on_delay_us);
	} else {
		ts_err("Failed to enable analog power:%d", r);
		return r;
	}

	core_data->power_on = 1;
	return 0;
}

/**
 * gt9896s_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int gt9896s_ts_power_off(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ts_board_data *ts_bdata =
			board_data(core_data);
	int r;

	ts_info("Device power off");
	if (!core_data->power_on)
		return 0;

	if (core_data->avdd) {
		r = regulator_disable(core_data->avdd);
		if (!r) {
			ts_info("regulator disable SUCCESS");
			if (ts_bdata->power_off_delay_us)
				usleep_range(ts_bdata->power_off_delay_us,
					     ts_bdata->power_off_delay_us);
		} else {
			ts_err("Failed to disable analog power:%d", r);
			return r;
		}
	}

	core_data->power_on = 0;
	return 0;
}

#ifdef CONFIG_PINCTRL
/**
 * gt9896s_ts_pinctrl_init - Get pinctrl handler and pinctrl_state
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int gt9896s_ts_pinctrl_init(struct gt9896s_ts_core *core_data)
{
	int r = 0;
	ts_debug("enter %s", __func__);

	/* get pinctrl handler from of node */
	core_data->pinctrl = devm_pinctrl_get(
		core_data->ts_dev->spi_dev->controller->dev.parent);
	if (IS_ERR_OR_NULL(core_data->pinctrl)) {
		ts_info("Failed to get pinctrl handler[need confirm]");
		core_data->pinctrl = NULL;
		return -EINVAL;
	}

	/* default spi mode */
	core_data->pin_spi_mode_default = pinctrl_lookup_state(
				core_data->pinctrl, PINCTRL_STATE_SPI_DEFAULT);
	if (IS_ERR_OR_NULL(core_data->pin_spi_mode_default)) {
		r = PTR_ERR(core_data->pin_spi_mode_default);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_SPI_DEFAULT, r);
		core_data->pin_spi_mode_default = NULL;
		goto exit_pinctrl_put;
	}

	/* int active state */
	core_data->pin_int_sta_active = pinctrl_lookup_state(
				core_data->pinctrl, PINCTRL_STATE_INT_ACTIVE);
	if (IS_ERR_OR_NULL(core_data->pin_int_sta_active)) {
		r = PTR_ERR(core_data->pin_int_sta_active);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_INT_ACTIVE, r);
		core_data->pin_int_sta_active = NULL;
		goto exit_pinctrl_put;
	}

	/* rst active state */
	core_data->pin_rst_sta_active = pinctrl_lookup_state(
				core_data->pinctrl, PINCTRL_STATE_RST_ACTIVE);
	if (IS_ERR_OR_NULL(core_data->pin_rst_sta_active)) {
		r = PTR_ERR(core_data->pin_rst_sta_active);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_RST_ACTIVE, r);
		core_data->pin_rst_sta_active = NULL;
		goto exit_pinctrl_put;
	}
	ts_info("success get avtive pinctrl state");

	/* int suspend state */
	core_data->pin_int_sta_suspend = pinctrl_lookup_state(
				core_data->pinctrl, PINCTRL_STATE_INT_SUSPEND);
	if (IS_ERR_OR_NULL(core_data->pin_int_sta_suspend)) {
		r = PTR_ERR(core_data->pin_int_sta_suspend);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_INT_SUSPEND, r);
		core_data->pin_int_sta_suspend = NULL;
		goto exit_pinctrl_put;
	}

	/* int suspend state */
	core_data->pin_rst_sta_suspend = pinctrl_lookup_state(
				core_data->pinctrl, PINCTRL_STATE_RST_SUSPEND);
	if (IS_ERR_OR_NULL(core_data->pin_rst_sta_suspend)) {
		r = PTR_ERR(core_data->pin_rst_sta_suspend);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_RST_SUSPEND, r);
		core_data->pin_rst_sta_suspend = NULL;
		goto exit_pinctrl_put;
	}
	ts_debug("success get suspend pinctrl state");

	return 0;
exit_pinctrl_put:
	pinctrl_put(core_data->pinctrl);
	core_data->pinctrl = NULL;
	return r;
}

int gt9896s_ts_gpio_suspend(struct gt9896s_ts_core *core_data)
{
	int r = 0;

	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_int_sta_suspend);
		if (r < 0) {
			ts_err("Failed to select int suspend state, r:%d", r);
			goto err_suspend_pinctrl;
		}
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_rst_sta_suspend);
		if (r < 0) {
			ts_err("Failed to select rst suspend state, r:%d", r);
			goto err_suspend_pinctrl;
		}
	}
err_suspend_pinctrl:
	return r;
}

int gt9896s_ts_gpio_resume(struct gt9896s_ts_core *core_data)
{
	int r = 0;

	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_int_sta_active);
		if (r < 0) {
			ts_err("Failed to select int active state, r:%d", r);
			goto err_resume_pinctrl;
		}
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_rst_sta_active);
		if (r < 0) {
			ts_err("Failed to select rst active state, r:%d", r);
			goto err_resume_pinctrl;
		}
	}
err_resume_pinctrl:
		return r;
}
#endif

/**
 * gt9896s_ts_gpio_setup - Request gpio resources from GPIO subsysten
 *	reset_gpio and irq_gpio number are obtained from gt9896s_ts_device
 *  which created in hardware layer driver. e.g.gt9896s_xx_i2c.c
 *	A gt9896s_ts_device should set those two fileds to right value
 *	before registed to touch core driver.
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int gt9896s_ts_gpio_setup(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ts_board_data *ts_bdata =
			 board_data(core_data);
	int r = 0;

	ts_info("GPIO setup,reset-gpio:%d, irq-gpio:%d",
		ts_bdata->reset_gpio, ts_bdata->irq_gpio);
	/*
	 * after kenerl3.13, gpio_ api is deprecated, new
	 * driver should use gpiod_ api.
	 */
	if (gpio_is_valid(ts_bdata->reset_gpio)) {
		r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->reset_gpio,
			GPIOF_OUT_INIT_HIGH,
			"gt9896s_reset_gpio");
		if (r < 0) {
			ts_err("Failed to request reset gpio, r:%d", r);
			goto err_request_reset_gpio;
		}
	}
	if (gpio_is_valid(ts_bdata->irq_gpio)) {
		r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->irq_gpio,
			GPIOF_IN,
			"gt9896s_irq_gpio");
		if (r < 0) {
			ts_err("Failed to request irq gpio, r:%d", r);
			goto err_request_eint_gpio;
		}
	}
	return r;

err_request_eint_gpio:
	gpio_free(ts_bdata->reset_gpio);
err_request_reset_gpio:
	return r;
}

/**
 * gt9896s_input_set_params - set input parameters
 */
static void gt9896s_ts_set_input_params(struct input_dev *input_dev,
		struct gt9896s_ts_board_data *ts_bdata)
{
	int i;

	if (ts_bdata->swap_axis)
		swap(ts_bdata->input_max_x, ts_bdata->input_max_y);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, ts_bdata->input_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, ts_bdata->input_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, ts_bdata->panel_max_w, 0, 0);
}

/**
 * gt9896s_ts_input_dev_config - Requset and config a input device
 *  then register it to input sybsystem.
 *  NOTE that some hardware layer may provide a input device
 *  (ts_dev->input_dev not NULL).
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int gt9896s_ts_input_dev_config(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ts_board_data *ts_bdata = board_data(core_data);
	struct input_dev *input_dev = NULL;
	int r;

	input_dev = input_allocate_device();
	if (!input_dev) {
		ts_err("Failed to allocated input device");
		return -ENOMEM;
	}

	core_data->input_dev = input_dev;
	input_set_drvdata(input_dev, core_data);

	input_dev->name = GOODIX_CORE_DRIVER_NAME;
	input_dev->phys = GOOIDX_INPUT_PHYS;
	input_dev->id.product = 0xDEAD;
	input_dev->id.vendor = 0xBEEF;
	input_dev->id.version = 10427;

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

	/* set input parameters */
	gt9896s_ts_set_input_params(input_dev, ts_bdata);

#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	r = input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH,
			    INPUT_MT_DIRECT);
	if (r < 0) {
		ts_err("input_mt_init_slots err0");
		return r;
	}
#else
	r = input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH);
	if (r < 0) {
		ts_err("input_mt_init_slots err0");
		return r;
	}
#endif
#endif

	input_set_capability(input_dev, EV_KEY, KEY_POWER);

	r = input_register_device(input_dev);
	if (r < 0) {
		ts_err("Unable to register input device");
		input_free_device(input_dev);
		return r;
	}

	return 0;
}

static int gt9896s_ts_pen_dev_config(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ts_board_data *ts_bdata = board_data(core_data);
	struct input_dev *pen_dev = NULL;
	int r;

	pen_dev = input_allocate_device();
	if (!pen_dev) {
		ts_err("Failed to allocated pen device");
		return -ENOMEM;
	}
	core_data->pen_dev = pen_dev;
	input_set_drvdata(pen_dev, core_data);

	pen_dev->name = GOODIX_PEN_DRIVER_NAME;
	pen_dev->id.product = 0xDEAD;
	pen_dev->id.vendor = 0xBEEF;
	pen_dev->id.version = 10427;

	pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	__set_bit(ABS_X, pen_dev->absbit);
	__set_bit(ABS_Y, pen_dev->absbit);
	__set_bit(BTN_STYLUS, pen_dev->keybit);
	__set_bit(BTN_STYLUS2, pen_dev->keybit);
	__set_bit(BTN_TOUCH, pen_dev->keybit);
	__set_bit(BTN_TOOL_PEN, pen_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
	input_set_abs_params(pen_dev, ABS_X, 0, ts_bdata->input_max_x, 0, 0);
	input_set_abs_params(pen_dev, ABS_Y, 0, ts_bdata->input_max_y, 0, 0);
	input_set_abs_params(pen_dev, ABS_PRESSURE, 0,
			     GOODIX_PEN_MAX_PRESSURE, 0, 0);

	r = input_register_device(pen_dev);
	if (r < 0) {
		ts_err("Unable to register pen device");
		input_free_device(pen_dev);
		return r;
	}

	return 0;
}

void gt9896s_ts_input_dev_remove(struct gt9896s_ts_core *core_data)
{
	input_unregister_device(core_data->input_dev);
	input_free_device(core_data->input_dev);
	core_data->input_dev = NULL;
}

void gt9896s_ts_pen_dev_remove(struct gt9896s_ts_core *core_data)
{
	input_unregister_device(core_data->pen_dev);
	input_free_device(core_data->pen_dev);
	core_data->pen_dev = NULL;
}

/**
 * gt9896s_ts_esd_work - check hardware status and recovery
 *  the hardware if needed.
 */
static void gt9896s_ts_esd_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct gt9896s_ts_esd *ts_esd = container_of(dwork,
			struct gt9896s_ts_esd, esd_work);
	struct gt9896s_ts_core *core = container_of(ts_esd,
			struct gt9896s_ts_core, ts_esd);
	const struct gt9896s_ts_hw_ops *hw_ops = ts_hw_ops(core);
	u8 data = GOODIX_ESD_TICK_WRITE_DATA;
	int r = 0;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (hw_ops->check_hw)
		r = hw_ops->check_hw(core->ts_dev);
	if (r < 0) {
		gt9896s_ts_power_off(core);
		gt9896s_ts_power_on(core);
		if (hw_ops->reset)
			hw_ops->reset(core->ts_dev);

		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev, core->ts_dev->reg.esd,
					&data, 1);
		if (r < 0)
			ts_err("failed init dynamic esd");
	} else {
		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev,
				core->ts_dev->reg.esd,
				&data, 1);
		if (r < 0)
			ts_err("failed init watch dog");
	}

	if (atomic_read(&ts_esd->esd_on))
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
}

/**
 * gt9896s_ts_esd_on - turn on esd protection
 */
static void gt9896s_ts_esd_on(struct gt9896s_ts_core *core)
{
	struct gt9896s_ts_esd *ts_esd = &core->ts_esd;

	if (core->ts_dev->reg.esd == 0)
		return;

	atomic_set(&ts_esd->esd_on, 1);
	if (!schedule_delayed_work(&ts_esd->esd_work, 2 * HZ)) {
		ts_info("esd work already in workqueue");
	}
	ts_info("esd on");
}

/**
 * gt9896s_ts_esd_off - turn off esd protection
 */
static void gt9896s_ts_esd_off(struct gt9896s_ts_core *core)
{
	struct gt9896s_ts_esd *ts_esd = &core->ts_esd;
	int ret;

	atomic_set(&ts_esd->esd_on, 0);
	ret = cancel_delayed_work_sync(&ts_esd->esd_work);
	ts_info("Esd off, esd work state %d", ret);
}

/**
 * gt9896s_esd_notifier_callback - notification callback
 *  under certain condition, we need to turn off/on the esd
 *  protector, we use kernel notify call chain to achieve this.
 *
 *  for example: before firmware update we need to turn off the
 *  esd protector and after firmware update finished, we should
 *  turn on the esd protector.
 */
static int gt9896s_esd_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct gt9896s_ts_esd *ts_esd = container_of(nb,
			struct gt9896s_ts_esd, esd_notifier);

	switch (action) {
	case NOTIFY_FWUPDATE_START:
	case NOTIFY_SUSPEND:
	case NOTIFY_ESD_OFF:
		gt9896s_ts_esd_off(ts_esd->ts_core);
		break;
	case NOTIFY_FWUPDATE_FAILED:
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_RESUME:
	case NOTIFY_ESD_ON:
		gt9896s_ts_esd_on(ts_esd->ts_core);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * gt9896s_ts_esd_init - initialize esd protection
 */
int gt9896s_ts_esd_init(struct gt9896s_ts_core *core)
{
	struct gt9896s_ts_esd *ts_esd = &core->ts_esd;
	struct gt9896s_ts_device *dev = core->ts_dev;
	u8 data = GOODIX_ESD_TICK_WRITE_DATA;
	int r;

	if (!dev->hw_ops->check_hw || !dev->reg.esd) {
		ts_info("missing key info for esd check");
		return 0;
	}

	INIT_DELAYED_WORK(&ts_esd->esd_work, gt9896s_ts_esd_work);
	ts_esd->ts_core = core;
	atomic_set(&ts_esd->esd_on, 0);
	ts_esd->esd_notifier.notifier_call = gt9896s_esd_notifier_callback;
	gt9896s_ts_register_notifier(&ts_esd->esd_notifier);

	/*init dynamic esd*/
	r = dev->hw_ops->write_trans(core->ts_dev, core->ts_dev->reg.esd,
				     &data, 1);
	if (r < 0)
		ts_err("failed init dynamic esd[ignore]");

	gt9896s_ts_esd_on(core);

	return 0;
}

void gt9896s_ts_release_connects(struct gt9896s_ts_core *core_data)
{
	struct input_dev *input_dev = core_data->input_dev;
	struct input_mt *mt = input_dev->mt;
	int i;

	if (mt) {
		for (i = 0; i < mt->num_slots; i++) {
			input_mt_slot(input_dev, i);
			input_mt_report_slot_state(input_dev,
					MT_TOOL_FINGER,
					false);
		}
		input_report_key(input_dev, BTN_TOUCH, 0);
		input_mt_sync_frame(input_dev);
		input_sync(input_dev);
	}
}

/**
 * gt9896s_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
static int gt9896s_ts_suspend(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ext_module *ext_module, *next;
	struct gt9896s_ts_device *ts_dev = core_data->ts_dev;
	int r;

	ts_info("Suspend start");

	/*
	 * notify suspend event, inform the esd protector
	 * and charger detector to turn off the work
	 */
	gt9896s_ts_blocking_notify(NOTIFY_SUSPEND, NULL);

	/* inform external module */
	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			if (!ext_module->funcs->before_suspend)
				continue;

			r = ext_module->funcs->before_suspend(core_data,
							      ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&gt9896s_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&gt9896s_modules.mutex);

	/* disable irq */
	gt9896s_ts_irq_enable(core_data, false);

	/* let touch ic work in sleep mode */
	if (ts_dev && ts_dev->hw_ops->suspend)
		ts_dev->hw_ops->suspend(ts_dev);
	atomic_set(&core_data->suspended, 1);

#ifdef CONFIG_PINCTRL
	r = gt9896s_ts_gpio_suspend(core_data);
	if (r < 0) {
		ts_err("Failed to select rst/eint suspend state: %d", r);
		goto out;
	}
#endif

	/* inform exteranl modules */
	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			if (!ext_module->funcs->after_suspend)
				continue;

			r = ext_module->funcs->after_suspend(core_data,
							     ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&gt9896s_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	gt9896s_ts_power_off(core_data);
	mutex_unlock(&gt9896s_modules.mutex);

out:
	core_data->ts_event.touch_data.touch_num = 0;
	gt9896s_ts_report_finger(core_data->input_dev,
		&core_data->ts_event.touch_data);
	ts_info("Suspend end");
	return 0;
}

/**
 * gt9896s_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int gt9896s_ts_resume(struct gt9896s_ts_core *core_data)
{
	struct gt9896s_ext_module *ext_module, *next;
	struct gt9896s_ts_device *ts_dev =
				core_data->ts_dev;
	int r;

	ts_info("Resume start");
	r = gt9896s_ts_power_on(core_data);
	if (r < 0) {
		ts_err("Failed to enable analog power: %d", r);
		goto out;
	}

	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			if (!ext_module->funcs->before_resume)
				continue;

			r = ext_module->funcs->before_resume(core_data,
							     ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&gt9896s_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&gt9896s_modules.mutex);

#ifdef CONFIG_PINCTRL
	r = gt9896s_ts_gpio_resume(core_data);
	if (r < 0) {
		ts_err("Failed to select rst/eint resume state: %d", r);
		goto out;
	}
#endif

	atomic_set(&core_data->suspended, 0);
	/* resume device */
	if (ts_dev && ts_dev->hw_ops->resume)
		ts_dev->hw_ops->resume(ts_dev);

	mutex_lock(&gt9896s_modules.mutex);
	if (!list_empty(&gt9896s_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &gt9896s_modules.head, list) {
			if (!ext_module->funcs->after_resume)
				continue;

			r = ext_module->funcs->after_resume(core_data,
							    ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&gt9896s_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&gt9896s_modules.mutex);

	gt9896s_ts_irq_enable(core_data, true);

	/*
	 * notify resume event, inform the esd protector
	 * and charger detector to turn on the work
	 */
	ts_info("try notify resume");
	gt9896s_ts_blocking_notify(NOTIFY_RESUME, NULL);
out:
	ts_debug("Resume end");
	return 0;
}

/* resume work queue callback */
static void resume_workqueue_callback(struct work_struct *work)
{
	gt9896s_ts_resume(resume_core_data);
}

#ifdef CONFIG_FB
/**
 * gt9896s_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
int gt9896s_ts_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct gt9896s_ts_core *core_data =
		container_of(self, struct gt9896s_ts_core, fb_notifier);
	struct fb_event *fb_event = data;
	int err = 0;

	if (fb_event && fb_event->data && core_data) {
		if (event == FB_EARLY_EVENT_BLANK) {
			/* before fb blank */
		} else if (event == FB_EVENT_BLANK) {
			int *blank = fb_event->data;
			if (*blank == FB_BLANK_UNBLANK) {
				if (touch_suspend_flag) {
					queue_work(touch_resume_workqueue, &touch_resume_work);
					touch_suspend_flag = 0;
				}
			} else if (*blank == FB_BLANK_POWERDOWN) {
				if (!touch_suspend_flag) {
					err = cancel_work_sync(
						&touch_resume_work);
					if (!err)
						ts_err("cancel resume_workqueue failed\n");
					gt9896s_ts_suspend(core_data);
				}
				touch_suspend_flag = 1;
			}
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/**
 * gt9896s_ts_earlysuspend - Early suspend function
 * Called by kernel during system suspend phrase
 */
static void gt9896s_ts_earlysuspend(struct early_suspend *h)
{
	struct gt9896s_ts_core *core_data =
		container_of(h, struct gt9896s_ts_core,
			 early_suspend);

	gt9896s_ts_suspend(core_data);
}
/**
 * gt9896s_ts_lateresume - Late resume function
 * Called by kernel during system wakeup
 */
static void gt9896s_ts_lateresume(struct early_suspend *h)
{
	struct gt9896s_ts_core *core_data =
		container_of(h, struct gt9896s_ts_core,
			 early_suspend);

	gt9896s_ts_resume(core_data);
}
#endif

#ifdef CONFIG_PM
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
 * gt9896s_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int gt9896s_ts_pm_suspend(struct device *dev)
{
	struct gt9896s_ts_core *core_data =
		dev_get_drvdata(dev);

	return gt9896s_ts_suspend(core_data);
}
/**
 * gt9896s_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int gt9896s_ts_pm_resume(struct device *dev)
{
	struct gt9896s_ts_core *core_data =
		dev_get_drvdata(dev);

	return gt9896s_ts_resume(core_data);
}
#endif
#endif

/**
 * gt9896s_generic_noti_callback - generic notifier callback
 *  for gt9896s touch notification event.
 */
static int gt9896s_generic_noti_callback(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct gt9896s_ts_core *ts_core = container_of(self,
			struct gt9896s_ts_core, ts_notifier);
	struct gt9896s_ts_device *ts_dev = ts_device(ts_core);
	const struct gt9896s_ts_hw_ops *hw_ops = ts_hw_ops(ts_core);
	int r;

	ts_info("notify event type 0x%x", (unsigned int)action);
	switch (action) {
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_FWUPDATE_FAILED:
		r = hw_ops->read_version(ts_dev, &ts_dev->chip_version);
		if (r < 0)
			ts_info("failed read fw version info[ignore]");
		break;
	default:
		break;
	}

	return 0;
}

int gt9896s_ts_stage2_init(struct gt9896s_ts_core *core_data)
{
	int r;
	struct gt9896s_ts_device *ts_dev = ts_device(core_data);

	/* send normal-cfg to firmware */
	r = ts_dev->hw_ops->send_config(ts_dev, &(ts_dev->normal_cfg));
	if (r < 0) {
		ts_info("failed send normal config[ignore]");
	}

	r = ts_dev->hw_ops->read_version(ts_dev, &ts_dev->chip_version);
	if (r < 0)
		ts_info("failed read fw version info[ignore]");

	/* alloc/config/register input device */
	r = gt9896s_ts_input_dev_config(core_data);
	if (r < 0) {
		ts_err("failed set input device");
		return r;
	}

	if (ts_dev->board_data.pen_enable) {
		r = gt9896s_ts_pen_dev_config(core_data);
		if (r < 0) {
			ts_err("failed set pen device");
			goto err_finger;
		}
	}
	/* request irq line */
	r = gt9896s_ts_irq_setup(core_data);
	if (r < 0) {
		ts_info("failed set irq");
		goto exit;
	}
	ts_info("success register irq");

#ifdef CONFIG_FB
	core_data->fb_notifier.notifier_call = gt9896s_ts_fb_notifier_callback;
	if (fb_register_client(&core_data->fb_notifier))
		ts_err("Failed to register fb notifier client:%d", r);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	core_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	core_data->early_suspend.resume = gt9896s_ts_lateresume;
	core_data->early_suspend.suspend = gt9896s_ts_earlysuspend;
	register_early_suspend(&core_data->early_suspend);
#endif
	/*create sysfs files*/
	gt9896s_ts_sysfs_init(core_data);

	/* esd protector */
	gt9896s_ts_esd_init(core_data);
	return 0;
exit:
	if (ts_dev->board_data.pen_enable) {
		gt9896s_ts_pen_dev_remove(core_data);
	}
err_finger:
	gt9896s_ts_input_dev_remove(core_data);
	return r;
}

/**
 * gt9896s_ts_probe - called by kernel when a Goodix touch
 *  platform driver is added.
 */
static int gt9896s_ts_probe(struct platform_device *pdev)
{
	struct gt9896s_ts_core *core_data = NULL;
	struct gt9896s_ts_device *ts_device;

	int r;

	ts_info("%s IN", __func__);

	ts_device = pdev->dev.platform_data;
	if (!ts_device || !ts_device->hw_ops) {
		ts_err("Invalid touch device");
		return -ENODEV;
	}

	core_data = devm_kzalloc(&pdev->dev, sizeof(struct gt9896s_ts_core),
				 GFP_KERNEL);
	if (!core_data) {
		ts_err("Failed to allocate memory for core data");
		return -ENOMEM;
	}

	/* touch core layer is a platform driver */
	core_data->pdev = pdev;
	core_data->ts_dev = ts_device;
	platform_set_drvdata(pdev, core_data);

	resume_core_data = core_data;

	r = gt9896s_ts_power_init(core_data);
	if (r < 0)
		goto out;

	r = gt9896s_ts_power_on(core_data);
	if (r < 0)
		goto regulator_err;

	/* get GPIO resource */
	r = gt9896s_ts_gpio_setup(core_data);
	if (r < 0)
		goto regulator_err;

#ifdef CONFIG_PINCTRL
	/* Pinctrl handle is optional. */
	r = gt9896s_ts_pinctrl_init(core_data);
	if (!r && core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_spi_mode_default);
		if (r < 0)
			ts_err("Failed to select default pinstate, r:%d", r);
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_int_sta_active);
		if (r < 0)
			ts_err("Failed to select int active pinstate, r:%d", r);
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_rst_sta_active);
		if (r < 0)
			ts_err("Failed to select rst active pinstate, r:%d", r);
	}
#endif

	/* confirm it's gt9896s touch dev or not */
	r = ts_device->hw_ops->dev_prepare(ts_device);
	if (r) {
		ts_err("gt9896s device confirm failed[skip]");
		goto err;
	}

	/* create work queue for resume */
	touch_resume_workqueue = create_singlethread_workqueue("touch_resume");
	INIT_WORK(&touch_resume_work, resume_workqueue_callback);

	/* Try start a thread to get config-bin info */
	r = gt9896s_start_later_init(core_data);
	if (r) {
		ts_info("Failed start cfg_bin_proc");
		goto err;
	}

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = gt9896s_generic_noti_callback;
	gt9896s_ts_register_notifier(&core_data->ts_notifier);
	goto out;

err:
	if (core_data->pinctrl)
		pinctrl_put(core_data->pinctrl);
	gpio_free(ts_device->board_data.reset_gpio);
	gpio_free(ts_device->board_data.reset_gpio);
regulator_err:
	gt9896s_ts_power_off(core_data);
	regulator_put(core_data->avdd);
out:
	if (r) {
		core_data->initialized = 0;
		core_data = NULL;
	} else
		core_data->initialized = 1;
	gt9896s_modules.core_data = core_data;
	ts_info("gt9896s_ts_probe OUT, r:%d", r);
	/* wakeup ext module register work */
	complete_all(&gt9896s_modules.core_comp);
	return r;
}

int gt9896s_ts_remove(struct platform_device *pdev)
{
	struct gt9896s_ts_core *core_data = platform_get_drvdata(pdev);

	core_data->initialized = 0;
	if (atomic_read(&core_data->ts_esd.esd_on))
		gt9896s_ts_esd_off(core_data);
	gt9896s_remove_all_ext_modules();
	gt9896s_ts_power_off(core_data);
	gt9896s_debugfs_exit();
	gt9896s_ts_sysfs_exit(core_data);
	// can't free the memory for tools or gesture module
	//kfree(core_data);
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops dev_pm_ops = {
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = gt9896s_ts_pm_suspend,
	.resume = gt9896s_ts_pm_resume,
#endif
};
#endif

static const struct platform_device_id ts_core_ids[] = {
	{.name = GOODIX_CORE_DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(platform, ts_core_ids);

static struct platform_driver gt9896s_ts_driver = {
	.driver = {
		.name = GOODIX_CORE_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &dev_pm_ops,
#endif
	},
	.probe = gt9896s_ts_probe,
	.remove = gt9896s_ts_remove,
	.id_table = ts_core_ids,
};

int gt9896s_ts_core_init(void)
{
	ts_info("GT9896S Core layer init");
	if (!gt9896s_modules.initilized) {
		/* this may init by outer modules register event */
		ts_info("init modules struct");
		gt9896s_modules.initilized = true;
		INIT_LIST_HEAD(&gt9896s_modules.head);
		mutex_init(&gt9896s_modules.mutex);
		init_completion(&gt9896s_modules.core_comp);
	}

	gt9896s_debugfs_init();
	return platform_driver_register(&gt9896s_ts_driver);
}

/* uninit module manually */
int gt9896s_ts_core_release(struct gt9896s_ts_core *core_data)
{
	ts_info("gt9896s core module removed");

	platform_driver_unregister(&gt9896s_ts_driver);
	gt9896s_ts_dev_release();
	return 0;
}
