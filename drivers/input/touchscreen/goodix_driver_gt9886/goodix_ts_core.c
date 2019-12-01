 /*
  * Goodix Touchscreen Driver
  * Core layer of touchdriver architecture.
  *
  * Copyright (C) 2015 - 2016 Goodix, Inc.
  * Copyright (C) 2019 XiaoMi, Inc.
  * Authors:  Yulong Cai <caiyulong@goodix.com>
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
#ifdef CONFIG_DRM
#include <drm/drm_notifier.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
//#include <linux/wakelock.h>
#include "goodix_ts_core.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif
#include <linux/backlight.h>
#include "../xiaomi/xiaomi_touch.h"
#include "test_core/test_param_init.h"

#define INPUT_EVENT_START						0
#define INPUT_EVENT_SENSITIVE_MODE_OFF			0
#define INPUT_EVENT_SENSITIVE_MODE_ON			1
#define INPUT_EVENT_STYLUS_MODE_OFF				2
#define INPUT_EVENT_STYLUS_MODE_ON				3
#define INPUT_EVENT_WAKUP_MODE_OFF				4
#define INPUT_EVENT_WAKUP_MODE_ON				5
#define INPUT_EVENT_COVER_MODE_OFF				6
#define INPUT_EVENT_COVER_MODE_ON				7
#define INPUT_EVENT_SLIDE_FOR_VOLUME			8
#define INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME		9
#define INPUT_EVENT_SINGLE_TAP_FOR_VOLUME		10
#define INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME	11
#define INPUT_EVENT_PALM_OFF					12
#define INPUT_EVENT_PALM_ON						13
#define INPUT_EVENT_END							13
#define IS_USB_EXIST							0x06
#define IS_USB_NOT_EXIST						0x07

#define GOOIDX_INPUT_PHYS						"goodix_ts/input0"
#define PINCTRL_STATE_ACTIVE					"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND					"pmx_ts_suspend"
extern int goodix_start_cfg_bin(struct goodix_ts_core *ts_core);
extern int goodix_i2c_write(struct goodix_ts_device *dev, unsigned int reg, unsigned char *data, unsigned int len);


struct goodix_module goodix_modules;
struct goodix_ts_core *goodix_core_data;
/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 */
static void  __do_register_ext_module(struct work_struct *work)
{
	struct goodix_ext_module *module =
			container_of(work, struct goodix_ext_module, work);
	struct goodix_ext_module *ext_module;
	struct list_head *insert_point = &goodix_modules.head;

	ts_info("__do_register_ext_module IN, goodix_modules.core_exit:%d", goodix_modules.core_exit);

	/* waitting for core layer */
	if (!wait_for_completion_timeout(&goodix_modules.core_comp, 25 * HZ)) {
		ts_err("Module [%s] timeout", module->name);
		return;
	}

	/* driver probe failed */
	if (goodix_modules.core_exit) {
		ts_err("Can't register ext_module, core exit");
		return;
	}
	ts_info("start register ext_module");
	/* prority level *must* be set */
	if (module->priority == EXTMOD_PRIO_RESERVED) {
		ts_err("Priority of module [%s] needs to be set",
			module->name);
		return;
	}

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry(ext_module, &goodix_modules.head, list) {
			if (ext_module == module) {
				ts_info("Module [%s] already exists",
						module->name);
				mutex_unlock(&goodix_modules.mutex);
				return;
			}
		}

		list_for_each_entry(ext_module, &goodix_modules.head, list) {
			/* small value of priority have
			 * higher priority level*/
			if (ext_module->priority >= module->priority) {
				insert_point = &ext_module->list;
				break;
			}
		} /* else module will be inserted
		 to goodix_modules->head */
	}

	if (module->funcs && module->funcs->init) {
		if (module->funcs->init(goodix_modules.core_data,
					module) < 0) {
			ts_err("Module [%s] init error",
					module->name ? module->name : " ");
			mutex_unlock(&goodix_modules.mutex);
			return;
		}
	}
	list_add(&module->list, insert_point->prev);
	goodix_modules.count++;
	mutex_unlock(&goodix_modules.mutex);
	ts_info("Module [%s] registered,priority:%u",
		module->name,
		module->priority);
	return;
}

/**
 * goodix_register_ext_module - interface for external module
 * to register into touch core modules structure
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;

	if (!goodix_modules.initilized) {
		goodix_modules.initilized = true;
		goodix_modules.core_exit = true;
		INIT_LIST_HEAD(&goodix_modules.head);
		mutex_init(&goodix_modules.mutex);
		init_completion(&goodix_modules.core_comp);
	}

/*if (goodix_modules.core_exit) {
		ts_err("Can't register ext_module, core exit");
		return -EFAULT;
	}
*/
	ts_info("goodix_register_ext_module IN");
	INIT_WORK(&module->work, __do_register_ext_module);
	schedule_work(&module->work);
	ts_info("goodix_register_ext_module OUT");
	return 0;
}
EXPORT_SYMBOL_GPL(goodix_register_ext_module);

/**
 * goodix_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module;
	bool found = false;
	if (!module)
		return -EINVAL;
	if (!goodix_modules.initilized)
		return -EINVAL;
	if (!goodix_modules.core_data)
		return -ENODEV;
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry(ext_module, &goodix_modules.head, list) {
			if (ext_module == module) {
				found = true;
				break;
			}
		}
	} else {
		mutex_unlock(&goodix_modules.mutex);
		return -EFAULT;
	}

	if (!found) {
		ts_err("Module [%s] never registed", module->name);
		mutex_unlock(&goodix_modules.mutex);
		return -EFAULT;
	}

	list_del(&module->list);
	mutex_unlock(&goodix_modules.mutex);
	if (module->funcs && module->funcs->exit)
		module->funcs->exit(goodix_modules.core_data, module);
	goodix_modules.count--;
	ts_info("Moudle [%s] unregistered",
		module->name ? module->name : " ");
	return 0;
}
EXPORT_SYMBOL_GPL(goodix_unregister_ext_module);

static void goodix_ext_sysfs_release(struct kobject *kobj)
{
	ts_info("Kobject released!");
}

#define to_ext_module(kobj)	container_of(kobj,\
				struct goodix_ext_module, kobj)
#define to_ext_attr(attr)	container_of(attr,\
				struct goodix_ext_attribute, attr)

static ssize_t goodix_ext_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->show)
		return ext_attr->show(module, buf);
	return -EIO;
}

static ssize_t goodix_ext_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->store)
		return ext_attr->store(module, buf, count);
	return -EIO;
}

static const struct sysfs_ops goodix_ext_ops = {
	.show = goodix_ext_sysfs_show,
	.store = goodix_ext_sysfs_store
};

static struct kobj_type goodix_ext_ktype = {
	.release = goodix_ext_sysfs_release,
	.sysfs_ops = &goodix_ext_ops,
};

struct kobj_type *goodix_get_default_ktype(void)
{
	return &goodix_ext_ktype;
}
EXPORT_SYMBOL_GPL(goodix_get_default_ktype);

struct kobject *goodix_get_default_kobj(void)
{
	struct kobject *kobj = NULL;

	if (goodix_modules.core_data && goodix_modules.core_data->pdev)
		kobj = &goodix_modules.core_data->pdev->dev.kobj;
	return kobj;
}
EXPORT_SYMBOL_GPL(goodix_get_default_kobj);

/* debug fs */
struct debugfs_buf {
	struct debugfs_blob_wrapper buf;
	int pos;
	struct dentry *dentry;
} goodix_dbg;

void goodix_msg_printf(const char *fmt, ...)
{
	va_list args;
	int r;

	if (goodix_dbg.pos < goodix_dbg.buf.size) {
		va_start(args, fmt);
		r = vscnprintf(goodix_dbg.buf.data + goodix_dbg.pos,
			goodix_dbg.buf.size - 1, fmt, args);
		goodix_dbg.pos += r;
		va_end(args);
	}
}
EXPORT_SYMBOL_GPL(goodix_msg_printf);

static int goodix_debugfs_init(void)
{
	struct dentry *r_b;
	goodix_dbg.buf.size = PAGE_SIZE;
	goodix_dbg.pos = 0;
	goodix_dbg.buf.data = kzalloc(goodix_dbg.buf.size, GFP_KERNEL);
	if (goodix_dbg.buf.data == NULL) {
		pr_err("Debugfs init failed\n");
		goto exit;
	}
	r_b = debugfs_create_blob("goodix_ts", 0644, NULL, &goodix_dbg.buf);
	if (!r_b) {
		pr_err("Debugfs create failed\n");
		return -ENOENT;
	}
	goodix_dbg.dentry = r_b;

exit:
	return 0;
}

static void goodix_debugfs_exit(void)
{
	debugfs_remove(goodix_dbg.dentry);
	goodix_dbg.dentry = NULL;
	pr_info("Debugfs module exit\n");
}

/* show external module infomation */
static ssize_t goodix_ts_extmod_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ext_module *module;
	size_t offset = 0;
	int r;

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry(module, &goodix_modules.head, list) {
			r = snprintf(&buf[offset], PAGE_SIZE,
					"priority:%u module:%s\n",
					module->priority, module->name);
			if (r < 0) {
				mutex_unlock(&goodix_modules.mutex);
				return -EINVAL;
			}
			offset += r;
		}
	}

	mutex_unlock(&goodix_modules.mutex);
	return offset;
}

/* show driver infomation */
static ssize_t goodix_ts_driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DriverVersion:%s\n",
			GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
static ssize_t goodix_ts_chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_version chip_ver;
	int r, cnt = 0;

	cnt += snprintf(buf, PAGE_SIZE, "TouchDeviceName:%s\n", ts_dev->name);
	if (ts_dev->hw_ops->read_version) {
		r = ts_dev->hw_ops->read_version(ts_dev, &chip_ver);
		if (!r && chip_ver.valid) {
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
					"PID:%s\nVID:%02x %02x %02x %02x\nSensorID:%02x\n",
					chip_ver.pid, chip_ver.vid[0],
					chip_ver.vid[1], chip_ver.vid[2],
					chip_ver.vid[3], chip_ver.sensor_id);
		}
	}
	return cnt;
}

/* show chip configuration data */
static ssize_t goodix_ts_config_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_config *ncfg = ts_dev->normal_cfg;
	u8 *data;
	int i, r, offset = 0;

	if (ncfg && ncfg->initialized && ncfg->length < PAGE_SIZE) {
		data = kmalloc(ncfg->length, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		r = ts_dev->hw_ops->read(ts_dev, ncfg->reg_base, &data[0], ncfg->length);
		if (r < 0) {
			kfree(data);
			return -EINVAL;
		}

		for (i = 0; i < ncfg->length; i++) {
			if (i != 0 && i % 20 == 0)
				buf[offset++] = '\n';
			offset += snprintf(&buf[offset], PAGE_SIZE - offset, "%02x ", data[i]);
		}
		buf[offset++] = '\n';
		buf[offset++] = '\0';
		kfree(data);
		return offset;
	}
	return -EINVAL;
}

/* reset chip */
static ssize_t goodix_ts_reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;
	if (en != 1)
		return -EINVAL;
	if (ts_dev->hw_ops->reset)
		ts_dev->hw_ops->reset(ts_dev);
	return count;

}

static ssize_t goodix_ts_read_cfg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int ret, i, offset;
	char *cfg_buf;

	cfg_buf = kzalloc(4096, GFP_KERNEL);
	disable_irq(core_data->irq);
	if (ts_dev->hw_ops->read_config)
		ret = ts_dev->hw_ops->read_config(ts_dev, cfg_buf, 0);
	else
		ret = -EINVAL;
	enable_irq(core_data->irq);

	offset = 0;
	if (ret > 0) {
		for (i = 0; i < ret; i++) {
			if (i != 0 && i % 20 == 0)
				buf[offset++] = '\n';
			offset += snprintf(&buf[offset], 4096 - offset, "%02x ", cfg_buf[i]);
		}
	}
	kfree(cfg_buf);
	return ret;
}

static int goodix_ts_convert_0x_data(const u8 *buf,
		int buf_size, unsigned char *out_buf, int *out_buf_len)
{
	int i, m_size = 0;
	int temp_index = 0;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X')
			m_size++;
	}
	ts_info("***m_size:%d", m_size);
	if (m_size <= 1) {
		ts_err("cfg file ERROR, valid data count:%d\n", m_size);
		return -EINVAL;
	}
	*out_buf_len = m_size;
	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X') {
			if (temp_index >= m_size) {
				ts_err("exchange cfg data error, overflow, temp_index:%d,m_size:%d\n",
						temp_index, m_size);
				return -EINVAL;
			}
			if (buf[i + 1] >= '0' && buf[i + 1] <= '9')
				out_buf[temp_index] = (buf[i + 1] - '0') << 4;
			else if (buf[i + 1] >= 'a' && buf[i + 1] <= 'f')
				out_buf[temp_index] = (buf[i + 1] - 'a' + 10) << 4;
			else if (buf[i + 1] >= 'A' && buf[i + 1] <= 'F')
				out_buf[temp_index] = (buf[i + 1] - 'A' + 10) << 4;
			if (buf[i + 2] >= '0' && buf[i + 2] <= '9')
				out_buf[temp_index] += (buf[i + 2] - '0');
			else if (buf[i + 2] >= 'a' && buf[i + 2] <= 'f')
				out_buf[temp_index] += (buf[i + 2] - 'a' + 10);
			else if (buf[i + 2] >= 'A' && buf[i + 2] <= 'F')
				out_buf[temp_index] += (buf[i + 2] - 'A' + 10);
			temp_index++;
		}
	}
	return 0;
}



static ssize_t goodix_ts_send_cfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int en, r;
	const struct firmware *cfg_img;
	struct goodix_ts_config *config = NULL;

	ts_err("%s::enter\n",__func__);
	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	ts_err("en:%d", en);
	disable_irq(core_data->irq);

	/*request configuration*/
	r = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
	if (r < 0) {
		ts_err("cfg file [%s] not available,errno:%d", GOODIX_DEFAULT_CFG_NAME, r);
		goto exit;
	} else
		ts_info("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);

	config = kzalloc(sizeof(struct goodix_ts_config), GFP_KERNEL);
	if (config == NULL) {
		ts_err("Memory allco err");
		goto exit;
	}

	/*parse cfg data*/
	if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
				config->data, &config->length)) {
		ts_err("convert config data FAILED");
		goto exit;
	}

	config->reg_base = ts_dev->reg.cfg_addr;
	mutex_init(&config->lock);
	config->initialized = true;
	if (ts_dev->hw_ops->send_config)
		ts_dev->hw_ops->send_config(ts_dev, config);

exit:
	enable_irq(core_data->irq);
	if (config) {
		kfree(config);
		config = NULL;
	}
	if (cfg_img) {
		release_firmware(cfg_img);
		cfg_img = NULL;
	}
	ts_info("******OUT");
	return count;
}

/* show irq infomation */
static ssize_t goodix_ts_irq_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
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
static ssize_t goodix_ts_irq_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;
	goodix_ts_irq_enable(core_data, en);
	return count;
}

/* open short test */
static ssize_t goodix_ts_tp_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int r = 0;
	ret = goodix_tools_register();

	if (ret) {
		ret = 0;
		ts_err("tp_test prepare goodix_tools_register failed");
		r = snprintf(buf, sizeof(ret), "%d", ret);
		if (r < 0)
			return -EINVAL;
		return sizeof(ret);
	}
	ts_info("test start!");
	ret = test_process(dev);

	if (ret == 0) {
		ret = 1;
		ts_err("test PASS!");
	} else {
		ts_err("test FAILED. result:%x", ret);
		ret = 0;
	}
	goodix_tools_unregister();
	r = snprintf(buf, sizeof(ret), "%d\n", ret);
	if (r < 0)
		return -EINVAL;

	ts_info("test finish!");
	return sizeof(ret);

}

/* tp get rawdata */
static ssize_t goodix_ts_tp_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret = 0;
	int r = 0;
	int buf_size = 0;
	ret = goodix_tools_register();

	if (ret) {
		ret = 0;
		ts_err("tp_rawdata prepare goodix_tools_register failed");
		r = snprintf(buf, 6, "-EIO\t\n");
		if (r < 0)
			return -EINVAL;
		return 4;/*sizeof("-EIO")*/
	}
	ts_info("start get rawdata!");
	ret = get_tp_rawdata(dev, buf, &buf_size);
	goodix_tools_unregister();
	ts_info("test finish!");
	return ret;
}

static ssize_t goodix_ts_power_reset_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(core_data);

	int ret = 0;
	ts_info("ts power reset test!");

	goodix_ts_power_off(core_data);
	goodix_ts_power_on(core_data);
	if (hw_ops->reset)
		hw_ops->reset(core_data->ts_dev);

	return ret;
}

/* tp get test config */
static ssize_t goodix_ts_tp_get_testcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret = 0;
	int r = 0;
	int buf_size = 0;
	ret = goodix_tools_register();

	if (ret) {
		ret = 0;
		ts_err("tp_rawdata prepare goodix_tools_register failed");
		r = snprintf(buf, 6, "-EIO\t\n");
		if (r < 0)
			return -EINVAL;
		return 4;/*sizeof("-EIO")*/
	}

	ts_info("start get rawdata!");
	ret = get_tp_testcfg(dev, buf, &buf_size);
	goodix_tools_unregister();
	ts_info("test finish!");
	return ret;
}
static DEVICE_ATTR(extmod_info, S_IRUGO, goodix_ts_extmod_show, NULL);
static DEVICE_ATTR(driver_info, S_IRUGO, goodix_ts_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, S_IRUGO, goodix_ts_chip_info_show, NULL);
static DEVICE_ATTR(config_data, S_IRUGO, goodix_ts_config_data_show, NULL);
static DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, S_IWUSR | S_IWGRP, NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, S_IRUGO, goodix_ts_read_cfg_show, NULL);
static DEVICE_ATTR(irq_info, S_IRUGO | S_IWUSR | S_IWGRP,
		goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(tp_test, S_IRUGO, goodix_ts_tp_test_show, NULL);
static DEVICE_ATTR(tp_rawdata, S_IRUGO, goodix_ts_tp_rawdata_show, NULL);
static DEVICE_ATTR(tp_get_testcfg, S_IRUGO, goodix_ts_tp_get_testcfg_show, NULL);
static DEVICE_ATTR(tp_power_reset, S_IRUGO, goodix_ts_power_reset_show, NULL);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_extmod_info.attr,
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_config_data.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_tp_test.attr,
	&dev_attr_tp_rawdata.attr,
	&dev_attr_tp_get_testcfg.attr,
	&dev_attr_tp_power_reset.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

int goodix_ts_sysfs_init(struct goodix_ts_core *core_data)
{
	return sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

static void goodix_ts_sysfs_exit(struct goodix_ts_core *core_data)
{
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

/* event notifier */
static BLOCKING_NOTIFIER_HEAD(ts_notifier_list);
/**
 * goodix_ts_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *  see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ts_notifier_list, nb);
}
EXPORT_SYMBOL(goodix_ts_register_notifier);

/**
 * goodix_ts_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_notifier_list, nb);
}
EXPORT_SYMBOL(goodix_ts_unregister_notifier);

/**
 * fb_notifier_call_chain - notify clients of fb_events
 * see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v)
{
	return blocking_notifier_call_chain(&ts_notifier_list, (unsigned long)evt, v);
}
EXPORT_SYMBOL_GPL(goodix_ts_blocking_notify);

static void release_all_touches(struct goodix_ts_core *core_data)
{
	unsigned int type = MT_TOOL_FINGER;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int i;
	mutex_lock(&ts_dev->report_mutex);

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		input_mt_slot(core_data->input_dev, i);
		input_mt_report_slot_state(core_data->input_dev, type, 0);
	}
	ts_err("enter:%s core_data->touch_id=%d\n", __func__, core_data->touch_id);
	core_data->sleep_finger = core_data->touch_id;
	core_data->touch_id = 0;
	input_sync(core_data->input_dev);
	mutex_unlock(&ts_dev->report_mutex);
}

/**
 * goodix_ts_input_report - report touch event to input subsystem
 *
 * @dev: input device pointer
 * @touch_data: touch data pointer
 * return: 0 ok, <0 failed
 */
static int goodix_ts_input_report(struct input_dev *dev,
		struct goodix_touch_data *touch_data)
{
	struct goodix_ts_coords *coords = &touch_data->coords[0];
	struct goodix_ts_core *core_data = input_get_drvdata(dev);
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	unsigned int touch_num = touch_data->touch_num;
	int i, id;

	if (core_data->fod_status){
		if ((core_data->event_status & 0x20) == 0x20){
			ts_info("%s:the data sended was error,return\n",__func__);
			return 0;
		}
	}

	mutex_lock(&ts_dev->report_mutex);
	id = coords->id;
	for (i = 0; i < ts_bdata->panel_max_id * 2; i++) {
		if (touch_num && i == id) { /* this is a valid touch down event */
			input_mt_slot(dev, id);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);

			/*first touch down event*/
			input_report_key(dev, BTN_TOUCH, !!touch_num);
			/*point position*/
			input_report_abs(dev, ABS_MT_POSITION_X, coords->x);
			input_report_abs(dev, ABS_MT_POSITION_Y, coords->y);
			/*
			input_report_abs(dev, ABS_MT_TOUCH_MAJOR, coords->w);
			input_report_abs(dev, ABS_MT_PRESSURE, coords->p);
			input_report_abs(dev, ABS_MT_TOUCH_MINOR, coords->area);
			*/

			if ((core_data->event_status & 0x88) != 0x88 || !core_data->fod_status)
				coords->overlapping_area = 0;
			input_report_abs(dev, ABS_MT_WIDTH_MINOR, coords->overlapping_area);
			input_report_abs(dev, ABS_MT_WIDTH_MAJOR, coords->overlapping_area);
			if (!__test_and_set_bit(i, &core_data->touch_id)) {
				ts_info("[GTP] %s report press:%d", __func__, i);
			}
			dev_dbg(core_data->ts_dev->dev, "[GTP] %s report:[%d](%d, %d, %d, %d)", __func__, id,
				touch_data->coords[0].x, touch_data->coords[0].y,
				touch_data->coords[0].area, touch_data->coords[0].overlapping_area);
			id = (++coords)->id;
		} else {
			if (__test_and_clear_bit(i, &core_data->touch_id)) {
				input_mt_slot(dev, i);
				input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
				if (!touch_num) {
					input_report_key(dev, BTN_TOUCH, !!touch_num);
					input_report_key(dev, BTN_TOOL_FINGER, 0);
					core_data->sleep_finger = 0;
				}
				ts_info("[GTP] %s report leave:%d", __func__, i);
			}
		}
	}

	/*report finger*/
	/*ts_info("get_event_now :0x%02x, pre_event : %d", get_event_now, pre_event);*/
	if ((core_data->event_status & 0x88) == 0x88 && core_data->fod_status) {
			input_report_key(core_data->input_dev, BTN_INFO, 1);
			/*input_report_key(core_data->input_dev, KEY_INFO, 1);*/
			core_data->fod_pressed = true;
			ts_info("BTN_INFO press");
		} else if (core_data->fod_pressed && (core_data->event_status & 0x88) != 0x88) {
		if (unlikely(!core_data->fod_test)) {
			input_report_key(core_data->input_dev, BTN_INFO, 0);
			/*input_report_key(core_data->input_dev, KEY_INFO, 0);*/
			ts_info("BTN_INFO release");
			core_data->fod_pressed = false;
		}
	}
	mutex_unlock(&ts_dev->report_mutex);
	input_sync(dev);
	/* check the ghost touch issue */
	if (!touch_num && core_data->touch_id) {
		ts_err("touch fw miss the up event");
		release_all_touches(core_data);
	}

	return 0;
}

static void goodix_ts_sleep_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, sleep_work);
	struct goodix_ts_device *ts_dev =  core_data->ts_dev;
	struct goodix_ext_module *ext_module;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	int r;

	ts_info("enter");

	if (core_data->tp_already_suspend) {
		r = wait_for_completion_timeout(&core_data->pm_resume_completion, msecs_to_jiffies(500));
		if (!r) {
			ts_info("pm_resume_completion timeout, i2c is closed");
			return;
		} else {
			ts_info("pm_resume_completion be completed, handling irq");
		}
	}

	mutex_lock(&goodix_modules.mutex);
	list_for_each_entry(ext_module, &goodix_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		r = ext_module->funcs->irq_event(core_data, ext_module);
		if (r == EVT_CANCEL_IRQEVT) {
			ts_info("irq exit");
			mutex_unlock(&goodix_modules.mutex);
			return;
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/* read touch data from touch device */
	r = ts_dev->hw_ops->event_handler(ts_dev, ts_event);
	if (likely(r >= 0)) {
		if (ts_event->event_type == EVENT_TOUCH) {
			/* report touch */
			goodix_ts_input_report(core_data->input_dev,
					&ts_event->event_data.touch_data);
		}
	}
	ts_info("exit");
}


/**
 * goodix_ts_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static irqreturn_t goodix_ts_threadirq_func(int irq, void *data)
{
	u8 irq_flag = 0;
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_device *ts_dev =  core_data->ts_dev;
	struct goodix_ext_module *ext_module;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	int r;
	struct i2c_client *client = NULL;

	client = to_i2c_client(ts_dev->dev);
	i2c_set_clientdata(client, core_data);

	core_data->irq_trig_cnt++;
	/* inform external module */
	/* ts_err("enter %s\n", __func__);*/
	if (core_data->tp_already_suspend) {
		ts_info("device in suspend noirq, schedue to work");
		pm_wakeup_event(&client->dev, msecs_to_jiffies(500));
		queue_work(core_data->event_wq, &core_data->sleep_work);
		return IRQ_HANDLED;
	}

	mutex_lock(&goodix_modules.mutex);
	list_for_each_entry(ext_module, &goodix_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		r = ext_module->funcs->irq_event(core_data, ext_module);
		/*ts_err("enter %s r=%d\n", __func__, r);*/
		if (r == EVT_CANCEL_IRQEVT) {
			ts_err("enter %s EVT_CANCEL_IRQEVT \n", __func__);
			mutex_unlock(&goodix_modules.mutex);
			return IRQ_HANDLED;
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/* read touch data from touch device */
	r = ts_dev->hw_ops->event_handler(ts_dev, ts_event);
	if (likely(r >= 0)) {
		if (ts_event->event_type == EVENT_TOUCH) {
			/* report touch */
			goodix_ts_input_report(core_data->input_dev,
					&ts_event->event_data.touch_data);
		}
	}
	/* clean irq flag */
	ts_dev->hw_ops->write_trans(ts_dev, ts_dev->reg.coor, &irq_flag, 1);/*TS_REG_COORDS_BASE*/
	return IRQ_HANDLED;
}

/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_irq_setup(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r;

	/* if ts_bdata-> irq is invalid */
	if (ts_bdata->irq <= 0) {
		core_data->irq = gpio_to_irq(ts_bdata->irq_gpio);
	} else {
		core_data->irq = ts_bdata->irq;
	}

	ts_info("IRQ:%u,flags:%d", core_data->irq, (int)ts_bdata->irq_flags);
	r = devm_request_threaded_irq(&core_data->pdev->dev,
			core_data->irq, NULL,
			goodix_ts_threadirq_func,
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
 * goodix_ts_irq_enable - Enable/Disable a irq
 * @core_data: pointer to touch core data
 * enable: enable or disable irq
 * return: 0 ok, <0 failed
 */
int goodix_ts_irq_enable(struct goodix_ts_core *core_data, bool enable)
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
EXPORT_SYMBOL(goodix_ts_irq_enable);
/**
 * goodix_ts_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_power_init(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata;
        ts_bdata = board_data(core_data);

        gpio_direction_output(ts_bdata->reset_gpio, 0);
        gpio_direction_output(ts_bdata->irq_gpio, 1);

	return 0;
}

/**
 * goodix_ts_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r = 0;

	ts_err("enter::%s\n",__func__);
	if (core_data->power_on)
		return 0;
	gpio_direction_output(ts_bdata->vdd_gpio, 1);
	gpio_direction_output(ts_bdata->avdd_gpio, 1);
	core_data->power_on = 1;
	return r;
}

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r = 0;

	ts_info("Device power off");
	if (!core_data->power_on)
		return 0;
	gpio_direction_output(ts_bdata->vdd_gpio, 0);
	gpio_direction_output(ts_bdata->avdd_gpio, 0);
	core_data->power_on = 0;
	return r;
}

#ifdef CONFIG_PINCTRL
/**
 * goodix_ts_pinctrl_init - Get pinctrl handler and pinctrl_state
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_pinctrl_init(struct goodix_ts_core *core_data)
{
	int r = 0;

	/* get pinctrl handler from of node */
	core_data->pinctrl = devm_pinctrl_get(core_data->ts_dev->dev);
	if (IS_ERR_OR_NULL(core_data->pinctrl)) {
		ts_err("Failed to get pinctrl handler");
		return PTR_ERR(core_data->pinctrl);
	}

	/* active state */
	core_data->pin_sta_active = pinctrl_lookup_state(core_data->pinctrl, PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(core_data->pin_sta_active)) {
		r = PTR_ERR(core_data->pin_sta_active);
		ts_err("Failed to get pinctrl state:%s, r:%d", PINCTRL_STATE_ACTIVE, r);
		goto exit_pinctrl_put;
	}

	/* suspend state */
	core_data->pin_sta_suspend = pinctrl_lookup_state(core_data->pinctrl, PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(core_data->pin_sta_suspend)) {
		r = PTR_ERR(core_data->pin_sta_suspend);
		ts_err("Failed to get pinctrl state:%s, r:%d", PINCTRL_STATE_SUSPEND, r);
		goto exit_pinctrl_put;
	}

	return 0;
exit_pinctrl_put:
	devm_pinctrl_put(core_data->pinctrl);
	core_data->pinctrl = NULL;
	return r;
}
#endif

/**
 * goodix_ts_gpio_setup - Request gpio resources from GPIO subsysten
 *	reset_gpio and irq_gpio number are obtained from goodix_ts_device
 *  which created in hardware layer driver. e.g.goodix_xx_i2c.c
 *	A goodix_ts_device should set those two fileds to right value
 *	before registed to touch core driver.
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_gpio_setup(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r = 0;

	ts_info("GPIO setup,reset-gpio:%d, irq-gpio:%d",
		ts_bdata->reset_gpio, ts_bdata->irq_gpio);
	/*
	 * after kenerl3.13, gpio_ api is deprecated, new
	 * driver should use gpiod_ api.
	 */
	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->reset_gpio,
			GPIOF_OUT_INIT_HIGH,
			"ts_reset_gpio");
	if (r < 0) {
		ts_err("Failed to request reset gpio, r:%d", r);
		return r;
	}

	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->irq_gpio,
			GPIOF_IN,
			"ts_irq_gpio");
	if (r < 0) {
		ts_err("Failed to request irq gpio, r:%d", r);
		return r;
	}
	return 0;
}

static ssize_t gtp_touch_suspend_notify_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", !!atomic_read(&goodix_core_data->suspend_stat));
}

static ssize_t gtp_fod_test_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int value = 0;
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);

	ts_info("buf:%c,count:%zu\n", buf[0], count);
	sscanf(buf, "%u", &value);
	if (value) {
		input_report_key(core_data->input_dev, BTN_INFO, 1);
		/*input_report_key(core_data->input_dev, KEY_INFO, 1);*/
		input_sync(core_data->input_dev);
		input_mt_slot(core_data->input_dev, 0);
		input_mt_report_slot_state(core_data->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(core_data->input_dev, BTN_TOUCH, 1);
		input_report_key(core_data->input_dev, BTN_TOOL_FINGER, 1);
		input_report_abs(core_data->input_dev, ABS_MT_TRACKING_ID, 0);
		input_report_abs(core_data->input_dev, ABS_MT_POSITION_X, CENTER_X);
		input_report_abs(core_data->input_dev, ABS_MT_POSITION_Y, CENTER_Y);
		input_report_abs(core_data->input_dev, ABS_MT_WIDTH_MINOR, 8);
		input_report_abs(core_data->input_dev, ABS_MT_WIDTH_MAJOR, 8);
		input_sync(core_data->input_dev);
	} else {
		input_mt_slot(core_data->input_dev, 0);
		input_mt_report_slot_state(core_data->input_dev, MT_TOOL_FINGER, 0);
		input_report_abs(core_data->input_dev, ABS_MT_TRACKING_ID, -1);
		input_report_key(core_data->input_dev, BTN_INFO, 0);
		/*input_report_key(core_data->input_dev, KEY_INFO, 0);*/
		input_sync(core_data->input_dev);
	}
	return count;
}


static ssize_t gtp_fod_status_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);

	return snprintf(buf, 10, "%d\n", core_data->fod_status);
}

static ssize_t gtp_fod_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	//struct goodix_ts_event *ts_event = &core_data->ts_event;
	ts_info("buf:%s, count:%zu\n", buf, count);
	sscanf(buf, "%u", &core_data->fod_status);

	//goodix_ts_input_report(core_data->input_dev,&ts_event->event_data.touch_data);
	core_data->gesture_enabled = core_data->double_wakeup | core_data->fod_status;
	goodix_check_gesture_stat(!!core_data->fod_status);

	return count;
}
static DEVICE_ATTR(fod_status, (S_IRUGO | S_IWUSR | S_IWGRP),
			gtp_fod_status_show, gtp_fod_status_store);

static DEVICE_ATTR(fod_test, (S_IRUGO | S_IWUSR | S_IWGRP),
		NULL, gtp_fod_test_store);

static DEVICE_ATTR(touch_suspend_notify, (S_IRUGO | S_IRGRP),
			gtp_touch_suspend_notify_show, NULL);

static void goodix_switch_mode_work(struct work_struct *work)
{
	struct goodix_mode_switch *ms =
		container_of(work, struct goodix_mode_switch, switch_mode_work);

	struct goodix_ts_core *info = ms->info;
	unsigned char value = ms->mode;

#ifdef CONFIG_GOODIX_HWINFO
	char ch[16] = { 0x0, };
#endif
	if (value >= INPUT_EVENT_WAKUP_MODE_OFF
		&& value <= INPUT_EVENT_WAKUP_MODE_ON) {
		info->double_wakeup = value - INPUT_EVENT_WAKUP_MODE_OFF;
		info->gesture_enabled = info->double_wakeup | info->fod_status;
		/*goodix_gesture_enable(!!info->gesture_enabled);*/
#ifdef CONFIG_GOODIX_HWINFO
		snprintf(ch, sizeof(ch), "%s", info->gesture_enabled ? "enabled" : "disabled");
#endif
	}
}

static int goodix_input_event(struct input_dev *dev, unsigned int type,
		unsigned int code, int value)
{
	struct goodix_ts_core *core_data = input_get_drvdata(dev);
	struct goodix_mode_switch *ms;

	if (!core_data) {
		ts_err("core_data is NULL");
		return 0;
	}

	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value >= INPUT_EVENT_START && value <= INPUT_EVENT_END) {
			ms = (struct goodix_mode_switch *)
				kmalloc(sizeof(struct goodix_mode_switch), GFP_ATOMIC);
			if (ms != NULL) {
				ms->info = core_data;
				ms->mode = (unsigned char)value;
				INIT_WORK(&ms->switch_mode_work,
					goodix_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				ts_err("failed in allocating memory for switching mode");
				return -ENOMEM;
			}
		} else {
			ts_err("Invalid event value");
			return -EINVAL;
		}
	}
	return 0;
}


/**
 * goodix_input_set_params - set input parameters
 */
static void goodix_ts_set_input_params(struct input_dev *input_dev,
		struct goodix_ts_board_data *ts_bdata)
{
	int i;

	if (ts_bdata->swap_axis)
		swap(ts_bdata->panel_max_x, ts_bdata->panel_max_y);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			0, ts_bdata->panel_max_y, 0, 0);
	/*input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			0, ts_bdata->panel_max_w, 0, 0);*/
	/*input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			0, ts_bdata->panel_max_p, 0, 0);*/
	/*input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR,
			0, ts_bdata->panel_max_w, 0, 0);*/
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MINOR,
			0, ts_bdata->panel_max_overlapping_area, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,
			0, ts_bdata->panel_max_overlapping_area, 0, 0);
	if (ts_bdata->panel_max_key) {
		for (i = 0; i < ts_bdata->panel_max_key; i++)
			input_set_capability(input_dev, EV_KEY,
					ts_bdata->panel_key_map[i]);
	}
}

/**
 * goodix_ts_input_dev_config - Requset and config a input device
 *  then register it to input sybsystem.
 *  NOTE that some hardware layer may provide a input device
 *  (ts_dev->input_dev not NULL).
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_input_dev_config(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct input_dev *input_dev = NULL;
	int r;

	input_dev = devm_input_allocate_device(&core_data->pdev->dev);
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
	input_dev->event = goodix_input_event;

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, input_dev->keybit);


#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

	/* set input parameters */
	goodix_ts_set_input_params(input_dev, ts_bdata);

#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	input_mt_init_slots(input_dev, ts_bdata->panel_max_id * 2 + 1, INPUT_MT_DIRECT);
#else
	input_mt_init_slots(input_dev, ts_bdata->panel_max_id * 2 + 1);
#endif
#endif

	input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
	input_set_capability(input_dev, EV_KEY, BTN_INFO);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);
	/*input_set_capability(input_dev, EV_KEY, KEY_INFO);*/

	r = input_register_device(input_dev);
	if (r < 0) {
		ts_err("Unable to register input device");
		return r;
	}
	return 0;
}

/**
 * goodix_ts_hw_init - Hardware initilize
 *  poweron - hardware reset - sendconfig
 * @core_data: pointer to touch core data
 * return: 0 intilize ok, <0 failed
 */
int goodix_ts_hw_init(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(core_data);
	int r;

	/* reset touch device */
	if (hw_ops->reset) {
		r = hw_ops->reset(core_data->ts_dev);
		if (r < 0)
			goto exit;
	}

	/* init */
	if (hw_ops->init) {
		r = hw_ops->init(core_data->ts_dev);
		if (r < 0)
			goto exit;
	}

exit:
	/* if bus communication error occured then
	 * exit driver binding, other errors will
	 * be ignored */
	if (r != -EBUS)
		r = 0;
	return r;
}

/**
 * goodix_ts_esd_work - check hardware status and recovery
 *  the hardware if needed.
 */
static void goodix_ts_esd_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct goodix_ts_esd *ts_esd = container_of(dwork,
			struct goodix_ts_esd, esd_work);
	struct goodix_ts_core *core = container_of(ts_esd,
			struct goodix_ts_core, ts_esd);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(core);
	int r = 0;
	u8 data = GOODIX_ESD_TICK_WRITE_DATA;

	if (ts_esd->esd_on == false)
		return;

	if (hw_ops->check_hw)
		r = hw_ops->check_hw(core->ts_dev);
	if (r < 0) {
		goodix_ts_power_off(core);
		goodix_ts_power_on(core);
		if (hw_ops->reset)
			hw_ops->reset(core->ts_dev);

		/*init static esd*/
		if (core->ts_dev->ic_type == IC_TYPE_NANJING) {
			r = hw_ops->write(core->ts_dev, 0x8043, &data, 1);
			if (r < 0)
				ts_err("nanjing esd reset, init static esd FAILED, i2c wirte ERROR");
		}

		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev, core->ts_dev->reg.esd, &data, 1);
		if (r < 0)
			ts_err("esd reset, init dynamic esd FAILED, i2c write ERROR");
	} else {
		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev, core->ts_dev->reg.esd, &data, 1);
		if (r < 0)
			ts_err("esd init watch dog FAILED, i2c write ERROR");
	}
	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on)
		schedule_delayed_work(&ts_esd->esd_work, GOODIX_ESD_CHECK_INTERVAL * HZ);
	mutex_unlock(&ts_esd->esd_mutex);
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;

	if(core->ts_dev->reg.esd == 0)
		return;

	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on == false) {
		ts_esd->esd_on = true;
		schedule_delayed_work(&ts_esd->esd_work, GOODIX_ESD_CHECK_INTERVAL * HZ);
		mutex_unlock(&ts_esd->esd_mutex);
		ts_info("Esd on");
		return;
	}
	mutex_unlock(&ts_esd->esd_mutex);
}

/**
 * goodix_ts_esd_off - turn off esd protection
 */
static void goodix_ts_esd_off(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;

	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on == true) {
		ts_esd->esd_on = false;
		cancel_delayed_work(&ts_esd->esd_work);
		mutex_unlock(&ts_esd->esd_mutex);
		ts_info("Esd off");
		return;
	}
	mutex_unlock(&ts_esd->esd_mutex);
}

/**
 * goodix_esd_notifier_callback - notification callback
 *  under certain condition, we need to turn off/on the esd
 *  protector, we use kernel notify call chain to achieve this.
 *
 *  for example: before firmware update we need to turn off the
 *  esd protector and after firmware update finished, we should
 *  turn on the esd protector.
 */
static int goodix_esd_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct goodix_ts_esd *ts_esd = container_of(nb,
			struct goodix_ts_esd, esd_notifier);

	switch (action) {
	case NOTIFY_FWUPDATE_START:
	case NOTIFY_SUSPEND:
	case NOTIFY_ESD_OFF:
		goodix_ts_esd_off(ts_esd->ts_core);
		break;
	case NOTIFY_FWUPDATE_END:
	case NOTIFY_RESUME:
	case NOTIFY_ESD_ON:
		goodix_ts_esd_on(ts_esd->ts_core);
		break;
	}
	return 0;
}

/**
 * goodix_ts_esd_init - initialize esd protection
 */
int goodix_ts_esd_init(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;
	struct goodix_ts_device *dev = core->ts_dev;
	u8 data = GOODIX_ESD_TICK_WRITE_DATA;
	int r;

	INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
	mutex_init(&ts_esd->esd_mutex);
	ts_esd->ts_core = core;
	ts_esd->esd_on = false;
	ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
	goodix_ts_register_notifier(&ts_esd->esd_notifier);

	if (dev->hw_ops->check_hw && dev->reg.esd != 0) {
		/*init static esd*/
		if (dev->ic_type == IC_TYPE_NANJING) {
			r = dev->hw_ops->write_trans(core->ts_dev, 0x8043, &data, 1);
			if (r < 0)
				ts_err("static ESD init ERROR, i2c write failed");
		}

		/*init dynamic esd*/
		r = dev->hw_ops->write_trans(core->ts_dev, core->ts_dev->reg.esd, &data, 1);
		if (r < 0)
			ts_err("dynamic ESD init ERROR, i2c write failed");
		goodix_ts_esd_on(core);
	}
	return 0;
}

/**
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int r = 0;

	ts_info("Suspend start");

	mutex_lock(&core_data->work_stat);
	if (atomic_read(&core_data->suspend_stat)) {
		ts_info("suspended, skip");
		goto out;
	}

	/*
	 * notify suspend event, inform the esd protector
	 * and charger detector to turn off the work
	 */
	goodix_ts_blocking_notify(NOTIFY_SUSPEND, NULL);

	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry(ext_module, &goodix_modules.head, list) {
			if (!ext_module->funcs->before_suspend)
				continue;

			r = ext_module->funcs->before_suspend(core_data, ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
				if (core_data->double_wakeup && core_data->fod_status) {
					atomic_set(&core_data->suspend_stat, TP_GESTURE_DBCLK_FOD);
				} else if (core_data->double_wakeup) {
					atomic_set(&core_data->suspend_stat, TP_GESTURE_DBCLK);
				} else if (core_data->fod_status) {
					atomic_set(&core_data->suspend_stat, TP_GESTURE_FOD);
				}
				mutex_unlock(&goodix_modules.mutex);
				ts_info("suspend_stat[%d]", atomic_read(&core_data->suspend_stat));
				ts_info("Canceled by module:%s", ext_module->name);
				if(!atomic_read(&core_data->suspend_stat))
					ts_info("go suspend remaind work\n");
				else
					goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/* disable irq */
	goodix_ts_irq_enable(core_data, false);

	/* let touch ic work in sleep mode */
	if (ts_dev && ts_dev->hw_ops->suspend)
		ts_dev->hw_ops->suspend(ts_dev);
	atomic_set(&core_data->suspended, 1);
	atomic_set(&core_data->suspend_stat, TP_SLEEP);

#ifdef CONFIG_PINCTRL
	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_sta_suspend);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

	/* inform exteranl modules */
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry(ext_module, &goodix_modules.head, list) {
			if (!ext_module->funcs->after_suspend)
				continue;

			r = ext_module->funcs->after_suspend(core_data, ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s", ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

out:
	/* release all the touch IDs */
	release_all_touches(core_data);
	core_data->ts_event.event_data.touch_data.touch_num = 0;

	sysfs_notify(&core_data->gtp_touch_dev->kobj, NULL,
		     "touch_suspend_notify");

	mutex_unlock(&core_data->work_stat);

	ts_info("Suspend end");
	return 0;
}

/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
int goodix_ts_resume(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int r = 0;

	ts_info("Resume start");
	/*goodix_ts_irq_enable(core_data, false);*/
	mutex_lock(&core_data->work_stat);
	if (!atomic_read(&core_data->suspend_stat)) {
		ts_info("resumed, skip");
		/*goodix_ts_irq_enable(core_data, true);*/
		goto out;
	}

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry(ext_module, &goodix_modules.head, list) {
			if (!ext_module->funcs->before_resume)
				continue;

			r = ext_module->funcs->before_resume(core_data, ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s", ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

#ifdef CONFIG_PINCTRL
	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl, core_data->pin_sta_active);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

	atomic_set(&core_data->suspended, 0);
	/* resume device */
	if (ts_dev && ts_dev->hw_ops->resume)
		ts_dev->hw_ops->resume(ts_dev);

	atomic_set(&core_data->suspend_stat, TP_NO_SUSPEND);

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry(ext_module, &goodix_modules.head, list) {
			if (!ext_module->funcs->after_resume)
				continue;

			r = ext_module->funcs->after_resume(core_data, ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s", ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);
	goodix_ts_irq_enable(core_data, true);
out:
	/*
	 * notify resume event, inform the esd protector
	 * and charger detector to turn on the work
	 */
	goodix_ts_blocking_notify(NOTIFY_RESUME, NULL);

	ts_err("core_data->fod_pressed = %d\n",core_data->fod_pressed);

	if (!core_data->fod_pressed) {
		ts_err("resume release all touch");
		release_all_touches(core_data);
	}
	core_data->sleep_finger = 0;

	sysfs_notify(&core_data->gtp_touch_dev->kobj, NULL,
		     "touch_suspend_notify");

	mutex_unlock(&core_data->work_stat);

	ts_info("Resume end");
	return 0;
}

static int goodix_bl_state_chg_callback(struct notifier_block *nb, unsigned long val, void *data)
{
	struct goodix_ts_core *core_data = container_of(nb, struct goodix_ts_core, bl_notifier);
	unsigned int blank;
	if (val != BACKLIGHT_UPDATED)
		return NOTIFY_OK;
	if (data && core_data) {
		blank = *(int *)(data);
		ts_info("%s val:%lu, blank:%u\n", __func__, val, blank);
		if (blank == BACKLIGHT_OFF && (atomic_read(&core_data->suspend_stat) && core_data->fod_status)) {
			ts_info("%s BACKLIGHT OFF, disable irq\n", __func__);
			goodix_ts_irq_enable(core_data, false);
		} else
			goodix_ts_irq_enable(core_data, true);
	}
	return NOTIFY_OK;
}

#ifdef CONFIG_DRM
/**
 * goodix_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
int goodix_ts_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct goodix_ts_core *core_data =
		container_of(self, struct goodix_ts_core, fb_notifier);
	struct drm_notify_data *fb_event = data;
	int blank;

	if (fb_event && fb_event->data && core_data) {
		blank = *(int *)(fb_event->data);
		flush_workqueue(core_data->event_wq);
		if (event == DRM_EVENT_BLANK && (blank == DRM_BLANK_POWERDOWN ||
			blank == DRM_BLANK_LP1 || blank == DRM_BLANK_LP2)) {
			ts_info("touchpanel suspend .....blank=%d\n",blank);
			ts_info("touchpanel suspend .....suspend_stat=%d\n", atomic_read(&core_data->suspend_stat));
			if (atomic_read(&core_data->suspend_stat))
				return 0;
			ts_info("touchpanel suspend by %s", blank == DRM_BLANK_POWERDOWN ? "blank" : "doze");
			queue_work(core_data->event_wq, &core_data->suspend_work);
		} else if (event == DRM_EVENT_BLANK && blank == DRM_BLANK_UNBLANK) {
			//if (!atomic_read(&core_data->suspend_stat))
			ts_info("core_data->suspend_stat = %d\n",atomic_read(&core_data->suspend_stat));
			ts_info("touchpanel resume");
			queue_work(core_data->event_wq, &core_data->resume_work);
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/**
 * goodix_ts_earlysuspend - Early suspend function
 * Called by kernel during system suspend phrase
 */
static void goodix_ts_earlysuspend(struct early_suspend *h)
{
	struct goodix_ts_core *core_data =
		container_of(h, struct goodix_ts_core, early_suspend);

	goodix_ts_suspend(core_data);
}
/**
 * goodix_ts_lateresume - Late resume function
 * Called by kernel during system wakeup
 */
static void goodix_ts_lateresume(struct early_suspend *h)
{
	struct goodix_ts_core *core_data =
		container_of(h, struct goodix_ts_core, early_suspend);
	goodix_ts_resume(core_data);
}
#endif

#ifdef CONFIG_PM
#ifdef CONFIG_DRM
static void goodix_ts_resume_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, resume_work);
	goodix_ts_resume(core_data);
}
static void goodix_ts_suspend_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, suspend_work);
	goodix_ts_suspend(core_data);
}

/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	if (device_may_wakeup(dev) && core_data->gesture_enabled) {
		enable_irq_wake(core_data->irq);
	}

	core_data->tp_already_suspend = true;
	reinit_completion(&core_data->pm_resume_completion);

	return 0;
}
/**
 * goodix_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int goodix_ts_pm_resume(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	if (device_may_wakeup(dev) && core_data->gesture_enabled) {
		disable_irq_wake(core_data->irq);
	}

	core_data->tp_already_suspend = false;
	complete(&core_data->pm_resume_completion);

	return 0;
}

#endif
#endif


#ifdef CONFIG_TOUCHSCREEN_GOODIX_DEBUG_FS
/*
static void tpdbg_shutdown(struct goodix_ts_core *core_data, bool sleep)
{

}
*/

static void tpdbg_suspend(struct goodix_ts_core *core_data, bool enable)
{
	if (enable)
		queue_work(core_data->event_wq, &core_data->suspend_work);
	else
		queue_work(core_data->event_wq, &core_data->resume_work);
}

static int tpdbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t tpdbg_read(struct file *file, char __user *buf, size_t size,
			loff_t *ppos)
{
	const char *str = "cmd support as below:\n \
				echo \"irq-disable\" or \"irq-enable\" to ctrl irq\n \
				echo \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n";

	loff_t pos = *ppos;
	int len = strlen(str);

	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;

	if (copy_to_user(buf, str, len))
		return -EFAULT;

	*ppos = pos + len;

	return len;
}

static ssize_t tpdbg_write(struct file *file, const char __user *buf,
			size_t size, loff_t *ppos)
{
	struct goodix_ts_core *core_data = file->private_data;
	char *cmd = kzalloc(size + 1, GFP_KERNEL);
	int ret = size;

	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(cmd, buf, size)) {
		ret = -EFAULT;
		goto out;
	}

	cmd[size] = '\0';

	if (!strncmp(cmd, "irq-disable", 11))
		goodix_ts_irq_enable(core_data, false);
	else if (!strncmp(cmd, "irq-enable", 10))
		goodix_ts_irq_enable(core_data, true);
/*
	else if (!strncmp(cmd, "tp-sd-en", 8))
		tpdbg_shutdown(core_data, true);
	else if (!strncmp(cmd, "tp-sd-off", 9))
		tpdbg_shutdown(core_data, false);
*/
	else if (!strncmp(cmd, "tp-suspend-en", 13))
		tpdbg_suspend(core_data, true);
	else if (!strncmp(cmd, "tp-suspend-off", 14))
		tpdbg_suspend(core_data, false);
out:
	kfree(cmd);

	return ret;
}

static int tpdbg_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations tpdbg_operations = {
	.owner = THIS_MODULE,
	.open = tpdbg_open,
	.read = tpdbg_read,
	.write = tpdbg_write,
	.release = tpdbg_release,
};
#endif

/**
 * goodix_generic_noti_callback - generic notifier callback
 *  for goodix touch notification event.
 */
int goodix_generic_noti_callback(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct goodix_ts_core *ts_core = container_of(self,
			struct goodix_ts_core, ts_notifier);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(ts_core);
	int r;

	switch (action) {
	case NOTIFY_FWUPDATE_END:
		if (hw_ops->init) {
			/* Firmware has been updated, we need to reinit
			 * the chip, read the sensor ID and send the
			 * correct config data based on sensor ID.
			 * The input parameters also needs to be updated.*/
			r = hw_ops->init(ts_core->ts_dev);
			if (r < 0)
				goto exit;
			goodix_ts_set_input_params(ts_core->input_dev, ts_core->ts_dev->board_data);
		}
		break;
	}
exit:
	return 0;

}

static ssize_t goodix_lockdown_info_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	#define TP_INFO_MAX_LENGTH 50
	char tmp[TP_INFO_MAX_LENGTH];

	if (*pos != 0 || !goodix_core_data)
		return 0;

	cnt = snprintf(tmp, TP_INFO_MAX_LENGTH,
			"0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			goodix_core_data->lockdown_info[0], goodix_core_data->lockdown_info[1],
			goodix_core_data->lockdown_info[2], goodix_core_data->lockdown_info[3],
			goodix_core_data->lockdown_info[4], goodix_core_data->lockdown_info[5],
			goodix_core_data->lockdown_info[6], goodix_core_data->lockdown_info[7]);
	ret = copy_to_user(buf, tmp, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations goodix_lockdown_info_ops = {
	.read = goodix_lockdown_info_read,
};

static ssize_t goodix_panel_color_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	if (!goodix_core_data)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%c\n", goodix_core_data->lockdown_info[2]);
}

static ssize_t goodix_panel_vendor_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	if (!goodix_core_data)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%c\n", goodix_core_data->lockdown_info[6]);
}

static ssize_t goodix_panel_display_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	if (!goodix_core_data)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%c\n", goodix_core_data->lockdown_info[1]);
}

static ssize_t goodix_lockdown_info_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	if (!goodix_core_data)
		return 0;

	return snprintf(buf, PAGE_SIZE,
			"0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			goodix_core_data->lockdown_info[0], goodix_core_data->lockdown_info[1],
			goodix_core_data->lockdown_info[2], goodix_core_data->lockdown_info[3],
			goodix_core_data->lockdown_info[4], goodix_core_data->lockdown_info[5],
			goodix_core_data->lockdown_info[6], goodix_core_data->lockdown_info[7]);
}

static DEVICE_ATTR(lockdown_info, (S_IRUGO), goodix_lockdown_info_show, NULL);
static DEVICE_ATTR(panel_vendor, (S_IRUGO), goodix_panel_vendor_show, NULL);
static DEVICE_ATTR(panel_color, (S_IRUGO), goodix_panel_color_show, NULL);
static DEVICE_ATTR(panel_display, (S_IRUGO), goodix_panel_display_show, NULL);

static struct attribute *goodix_attr_group[] = {
	&dev_attr_panel_vendor.attr,
	&dev_attr_panel_color.attr,
	&dev_attr_panel_display.attr,
	&dev_attr_lockdown_info.attr,
	NULL,
};

static int gtp_i2c_test(void)
{
	int ret = 0;
	u8 read_val = 0;
	struct goodix_ts_device *ts_device;
	ts_device = goodix_core_data->ts_dev;

	ret = ts_device->hw_ops->read_trans(ts_device, 0x3100,
			&read_val, 1);
	if (!ret) {
		ts_info("i2c test SUCCESS");
	} else {
		ts_err("i2c test FAILED");
		return GTP_RESULT_FAIL;
	}

	return GTP_RESULT_PASS;
}

static ssize_t gtp_selftest_read(struct file *file, char __user * buf,
				size_t count, loff_t * pos)
{
	char tmp[5] = { 0 };
	int cnt;

	if (*pos != 0 || !goodix_core_data)
		return 0;
	cnt =
		snprintf(tmp, sizeof(goodix_core_data->result_type), "%d\n",
			goodix_core_data->result_type);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}
	*pos += cnt;
	return cnt;
}

static int gtp_short_open_test(void)
{
	int ret = 0;

	ret = goodix_tools_register();

	if (ret) {
		ts_err("tp_test prepare goodix_tools_register failed");
		return GTP_RESULT_INVALID;
	}
	ts_info("test start!");
	ret = test_process((void *)(&goodix_core_data->pdev->dev));

	if (ret == 0) {
		ts_err("test PASS!");
		return GTP_RESULT_PASS;
	} else {
		ts_err("test FAILED. result:%x", ret);
		return GTP_RESULT_FAIL;
	}
	goodix_tools_unregister();

	ts_info("test finish!");
	return GTP_RESULT_FAIL;
}
static ssize_t gtp_selftest_write(struct file *file, const char __user * buf,
				size_t count, loff_t * pos)
{
	int retval = 0;
	char tmp[6];

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}
	if (!goodix_core_data)
		return GTP_RESULT_INVALID;

	if (!strncmp("short", tmp, 5) || !strncmp("open", tmp, 4)) {
		retval = gtp_short_open_test();
	} else if (!strncmp("i2c", tmp, 3))
		retval = gtp_i2c_test();

	goodix_core_data->result_type = retval;
out:
	if (retval >= 0)
		retval = count;

	return retval;
}

static const struct file_operations gtp_selftest_ops = {
	.read = gtp_selftest_read,
	.write = gtp_selftest_write,
};

static void gtp_power_supply_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, power_supply_work);
	struct goodix_ts_device *dev = core_data->ts_dev;
	u8 state_data[3];
	int is_usb_exist = 0;

	if (!atomic_read(&core_data->suspend_stat) && !goodix_modules.core_exit) {
		is_usb_exist = !!power_supply_is_system_supplied();
		if (is_usb_exist != core_data->is_usb_exist || core_data->is_usb_exist < 0) {
			core_data->is_usb_exist = is_usb_exist;
			ts_info("Power_supply_event:%d", is_usb_exist);
			if (is_usb_exist) {
				state_data[0] = IS_USB_EXIST;
				state_data[1] = 0x00;
				state_data[2] = 0xFA;
				goodix_i2c_write(dev, 0x6F68, state_data, 3);
				ts_info("USB is exist");
			} else {
				state_data[0] = IS_USB_NOT_EXIST;
				state_data[1] = 0x00;
				state_data[2] = 0xF9;
				goodix_i2c_write(dev, 0x6F68, state_data, 3);
				ts_info("USB is not exist");
			}
		}
	}
}
static int gtp_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct goodix_ts_core *ts_core = container_of(nb, struct goodix_ts_core, power_supply_notifier);

	queue_work(ts_core->event_wq, &ts_core->power_supply_work);

	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;

static int gtp_set_cur_value(int gtp_mode, int gtp_value)
{
	u8 state_data[3] = {0};
	u8 goodix_game_value = 0;
	u8 temp_value = 0;
	int ret = 0;
	int i = 0;

	struct goodix_ts_device *dev = goodix_core_data->ts_dev;
/*
	if (gtp_mode == Touch_Fod_Enable && goodix_core_data && gtp_value >= 0) {
		ts_info("set fod status");
		goodix_core_data->fod_status = gtp_value;
		goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup |
			goodix_core_data->fod_status | goodix_core_data->aod_status;
		goodix_check_gesture_stat(!!goodix_core_data->fod_status);
		return 0;
	}
	if (gtp_mode == Touch_Aod_Enable && goodix_core_data && gtp_value >= 0) {
		ts_info("set aod status");
		goodix_core_data->aod_status = gtp_value;
		goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup |
			goodix_core_data->fod_status | goodix_core_data->aod_status;
		goodix_check_gesture_stat(!!goodix_core_data->aod_status);
		return 0;
	}
*/
	if (gtp_mode >= Touch_Mode_NUM && gtp_mode < 0) {
		ts_err("gtp mode is error:%d", gtp_mode);
		return -EINVAL;
	} else if (xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MAX_VALUE]) {

		xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MAX_VALUE];

	} else if (xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MIN_VALUE]) {

		xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MIN_VALUE];
	}

	xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] = gtp_value;

	if (gtp_mode == Touch_Game_Mode && gtp_value == 0) {
		ts_info("exit touch game mode");
		state_data[0] = GTP_EXIT_GAME_CMD;
		state_data[1] = 0x00;
		state_data[2] = 0xF1;
		ret = goodix_i2c_write(dev, GTP_GAME_CMD_ADD, state_data, 3);
		if (ret < 0) {
			ts_err("exit game mode fail");
		}
		goodix_game_value = 0;
		return ret;
	}

	for (i = 0; i < Touch_Mode_NUM; i++) {
		switch (i) {
		case Touch_Game_Mode:
				break;
		case Touch_Active_MODE:
				break;
		case Touch_UP_THRESHOLD:
				temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
				goodix_game_value &= 0xFC;
				goodix_game_value |= temp_value;
				break;
		case Touch_Tolerance:
				temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
				temp_value = 3 - temp_value;
				goodix_game_value &= 0xF3;
				goodix_game_value |= (temp_value << 2);
				break;
		case Touch_Edge_Filter:
				temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
				goodix_game_value &= 0xCF;
				goodix_game_value |= (temp_value << 4);
				break;
		case Touch_Panel_Orientation:
				/* 0,1,2,3 = 0, 90, 180,270 */
				temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
				if (temp_value == 3)
					temp_value = 2;
				else if (temp_value == 2)
					temp_value = 3;
				goodix_game_value &= 0x3F;
				goodix_game_value |= (temp_value << 6);
				break;
		default:
				/* Don't support */
				break;

		};
	}
	ts_info("mode:%d, value:%d, write value:0x%x", gtp_mode, gtp_value, goodix_game_value);

	xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE];

	if (xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE]) {
		state_data[0] = GTP_GAME_CMD;
		state_data[1] = goodix_game_value;
		state_data[2] = 0xFF & (0 - state_data[0] - state_data[1]);

		ret = goodix_i2c_write(dev, GTP_GAME_CMD_ADD, state_data, 3);

		if (ret < 0) {
			ts_err("change game mode fail");
		}
	}
	return ret;
}

static int gtp_get_mode_value(int mode, int value_type)
{
	int value = -1;

	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		ts_err("don't support");

	return value;
}

static int gtp_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		ts_err("don't support");
	}
	ts_info("mode:%d, value:%d:%d:%d:%d", mode, value[0], value[1], value[2], value[3]);

	return 0;
}

static int gtp_reset_mode(int mode)
{
	int i = 0;

	if (mode < Touch_Mode_NUM && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		gtp_set_cur_value(mode, xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE]);
	} else if (mode == 0) {
		for (i = 0; i < Touch_Mode_NUM; i++) {
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			gtp_set_cur_value(mode, xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE]);
		}
	} else {
		ts_err("don't support");
	}

	ts_info("mode:%d", mode);

	return 0;
}

static void gtp_init_touchmode_data(void)
{
	int i;

	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;

	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;

	/* finger hysteresis */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 0;

	/*  Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 0;

	/*	edge filter */
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 1;

	/*	Orientation */
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;

	for (i = 0; i < Touch_Mode_NUM; i++) {
		ts_info("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}

	return;
}
#endif

/**
 * goodix_ts_probe - called by kernel when a Goodix touch
 *  platform driver is added.
 */
static int goodix_ts_probe(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = NULL;
	struct goodix_ts_device *ts_device;
	struct i2c_client *client = NULL;
	int r;
	u8 read_val = 0;

	ts_err("enter::%s\n",__func__);
	ts_device = pdev->dev.platform_data;
	if (!ts_device || !ts_device->hw_ops ||
			!ts_device->board_data) {
		ts_err("Invalid touch device");
		return -ENODEV;
	}

	core_data = devm_kzalloc(&pdev->dev, sizeof(struct goodix_ts_core), GFP_KERNEL);
	if (!core_data) {
		ts_err("Failed to allocate memory for core data");
		return -ENOMEM;
	}
	goodix_core_data = core_data;

	/* touch core layer is a platform driver */
	core_data->pdev = pdev;
	core_data->ts_dev = ts_device;
	platform_set_drvdata(pdev, core_data);

	client = to_i2c_client(ts_device->dev);
	i2c_set_clientdata(client, core_data);
	core_data->cfg_group_parsed = false;

	r = goodix_ts_power_init(core_data);
	if (r < 0) {
		pr_err("goodix_ts_power_init fail \n");
		goto out;
	}

	r = goodix_ts_power_on(core_data);
	if (r < 0) {
		pr_err("goodix_ts_power_on fail \n");
		goto out;
	}

#ifdef CONFIG_PINCTRL
	/* Pinctrl handle is optional. */
	r = goodix_ts_pinctrl_init(core_data);
	if (!r && core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl, core_data->pin_sta_active);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

	/* get GPIO resource */
	r = goodix_ts_gpio_setup(core_data);
	if (r < 0) {
		pr_err("goodix_ts_gpio_setup fail \n");
		goto out;
	}
	/*init lock to protect suspend_stat*/
	mutex_init(&core_data->work_stat);
	mutex_init(&ts_device->report_mutex);
	/*init complete and pm status*/
	core_data->tp_already_suspend = false;
	init_completion(&core_data->pm_resume_completion);

	/*create sysfs files*/
	goodix_ts_sysfs_init(core_data);

	r = ts_device->hw_ops->reset(ts_device);
	if (r < 0) {
		pr_err("goodix_hw_ops->reset fail \n");
		goto out;
	}

	core_data->event_wq = alloc_workqueue("gdt-event-queue",
				WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!core_data->event_wq) {
		ts_err("goodix cannot create work thread");
		r = -ENOMEM;
		goto out;
	}

	INIT_WORK(&core_data->resume_work, goodix_ts_resume_work);
	INIT_WORK(&core_data->suspend_work, goodix_ts_suspend_work);
	INIT_WORK(&core_data->sleep_work, goodix_ts_sleep_work);
	device_init_wakeup(&pdev->dev, 1);

	core_data->is_usb_exist = -1;
	/*i2c test*/
	r = ts_device->hw_ops->read_trans(ts_device, 0x3100, &read_val, 1);
	if (!r)
		ts_info("i2c test SUCCESS");
	else {
		ts_err("i2c test FAILED");
		goto out;
	}

	core_data->tp_lockdown_info_proc =
	    proc_create("tp_lockdown_info", 0664, NULL, &goodix_lockdown_info_ops);

	/*unified protocl
	 * start a thread to parse cfg_bin and init IC*/
	r = goodix_start_cfg_bin(core_data);
	if (!r) {
		ts_info("***start cfg_bin_proc SUCCESS");
	} else {
		ts_err("***start cfg_bin_proc FAILED");
		goto out;
	}

	core_data->power_supply_notifier.notifier_call = gtp_power_supply_event;
	power_supply_reg_notifier(&core_data->power_supply_notifier);
	INIT_WORK(&core_data->power_supply_work, gtp_power_supply_work);

	core_data->bl_notifier.notifier_call = goodix_bl_state_chg_callback;
	if (backlight_register_notifier(&core_data->bl_notifier) < 0) {
		ts_err("ERROR:register bl_notifier failed\n");
		goto out;
	}
	core_data->attrs.attrs = goodix_attr_group;
	r = sysfs_create_group(&client->dev.kobj, &core_data->attrs);
	if (r) {
		ts_err("ERROR: Cannot create sysfs structure!\n");
		r = -ENODEV;
		goto out;
	}

	if (core_data->gtp_tp_class == NULL)
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
		core_data->gtp_tp_class = get_xiaomi_touch_class();
		if (core_data->gtp_tp_class == NULL)
			ts_err("ERROR: gtp_tp_class is NULL!\n");
#else
		core_data->gtp_tp_class = class_create(THIS_MODULE, "touch");
#endif
	core_data->gtp_touch_dev = device_create(core_data->gtp_tp_class, NULL, 0x5d, core_data, "tp_dev");
	if (IS_ERR(core_data->gtp_touch_dev)) {
		ts_err("ERROR: Cannot create device for the sysfs!\n");
		goto out;
	}
	dev_set_drvdata(core_data->gtp_touch_dev, core_data);

	if (sysfs_create_file(&core_data->gtp_touch_dev->kobj,
			&dev_attr_touch_suspend_notify.attr)) {
		ts_err("Failed to create sysfs group!\n");
		goto out;
	}

	if (sysfs_create_file(&core_data->gtp_touch_dev->kobj,
				&dev_attr_fod_status.attr)) {
		ts_err("Failed to create fod_status sysfs group!\n");
		goto out;
	}

	if (sysfs_create_file(&core_data->gtp_touch_dev->kobj,
				  &dev_attr_fod_test.attr)) {
		ts_err("Failed to create fod_test sysfs group!");
		goto out;
	}

	core_data->tp_selftest_proc =
		proc_create("tp_selftest", 0644, NULL, &gtp_selftest_ops);

#ifdef CONFIG_GOODIX_HWINFO
	core_data->dbclick_count = 0;
#endif

	/*core_data->fod_status = -1;*/
	//wake_lock_init(&core_data->tp_wakelock, WAKE_LOCK_SUSPEND, "touch_locker");
#ifdef CONFIG_TOUCHSCREEN_GOODIX_DEBUG_FS
	core_data->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (core_data->debugfs) {
		debugfs_create_file("switch_state", 0660, core_data->debugfs, core_data,
					&tpdbg_operations);
	}
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
		memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
		xiaomi_touch_interfaces.getModeValue = gtp_get_mode_value;
		xiaomi_touch_interfaces.setModeValue = gtp_set_cur_value;
		xiaomi_touch_interfaces.resetMode = gtp_reset_mode;
		xiaomi_touch_interfaces.getModeAll = gtp_get_mode_all;
		xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
		gtp_init_touchmode_data();
#endif

out:
	backlight_unregister_notifier(&core_data->bl_notifier);
	ts_info("goodix_ts_probe OUT, r:%d", r);
	return r;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
#ifdef CONFIG_DRM
	drm_unregister_client(&core_data->fb_notifier);
#endif
	//wake_lock_destroy(&core_data->tp_wakelock);
	power_supply_unreg_notifier(&core_data->power_supply_notifier);
	goodix_ts_power_off(core_data);
	goodix_debugfs_exit();
	goodix_ts_sysfs_exit(core_data);
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops dev_pm_ops = {
	.suspend = goodix_ts_pm_suspend,
	.resume = goodix_ts_pm_resume,
};
#endif

static const struct platform_device_id ts_core_ids[] = {
	{.name = GOODIX_CORE_DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(platform, ts_core_ids);

static struct platform_driver goodix_ts_driver = {
	.driver = {
		.name = GOODIX_CORE_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &dev_pm_ops,
#endif
	},
	.probe = goodix_ts_probe,
	.remove = goodix_ts_remove,
	.id_table = ts_core_ids,
};


static int __init goodix_ts_core_init(void)
{
	ts_info("goodix9886 Core layer init");
	if (!goodix_modules.initilized) {
		goodix_modules.initilized = true;
		goodix_modules.core_exit = true;
		INIT_LIST_HEAD(&goodix_modules.head);
		mutex_init(&goodix_modules.mutex);
		init_completion(&goodix_modules.core_comp);
	}
	goodix_debugfs_init();
	return platform_driver_register(&goodix_ts_driver);
}


static void __exit goodix_ts_core_exit(void)
{
	ts_info("Core layer exit");
	platform_driver_unregister(&goodix_ts_driver);
	return;
}

module_init(goodix_ts_core_init);
module_exit(goodix_ts_core_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
