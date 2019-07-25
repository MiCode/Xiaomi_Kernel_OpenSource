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
#ifdef CONFIG_FB
#include <linux/msm_drm_notify.h>
#endif
#include "../xiaomi/xiaomi_touch.h"
#include "goodix_ts_core.h"
 #include "goodix_cfg_bin.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#define GOOIDX_INPUT_PHYS	"goodix_ts/input0"
#define PINCTRL_STATE_ACTIVE    "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND   "pmx_ts_suspend"

#define INPUT_EVENT_START			0
#define INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define INPUT_EVENT_SENSITIVE_MODE_ON		1
#define INPUT_EVENT_STYLUS_MODE_OFF		2
#define INPUT_EVENT_STYLUS_MODE_ON		3
#define INPUT_EVENT_WAKUP_MODE_OFF		4
#define INPUT_EVENT_WAKUP_MODE_ON		5
#define INPUT_EVENT_COVER_MODE_OFF		6
#define INPUT_EVENT_COVER_MODE_ON		7
#define INPUT_EVENT_SLIDE_FOR_VOLUME		8
#define INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME		9
#define INPUT_EVENT_SINGLE_TAP_FOR_VOLUME		10
#define INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME		11
#define INPUT_EVENT_PALM_OFF		12
#define INPUT_EVENT_PALM_ON		13
#define INPUT_EVENT_END				13
#define IS_USB_EXIST				0x06
#define IS_USB_NOT_EXIST			0x07

extern int goodix_start_cfg_bin(struct goodix_ts_core *ts_core);

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

/*	if (goodix_modules.core_exit) {
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
		ts_err("Module [%s] never registed",
				module->name);
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

	if (goodix_modules.core_data &&
			goodix_modules.core_data->pdev)
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
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_version chip_ver;
	int r, cnt = 0;

	cnt += snprintf(buf, PAGE_SIZE,
			"TouchDeviceName:%s\n", ts_dev->name);
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

		r = ts_dev->hw_ops->read(ts_dev, ncfg->reg_base,
				&data[0], ncfg->length);
		if (r < 0) {
			kfree(data);
			return -EINVAL;
		}

		for (i = 0; i < ncfg->length; i++) {
			if (i != 0 && i % 20 == 0)
				buf[offset++] = '\n';
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					"%02x ", data[i]);
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
				struct device_attribute *attr,
						char *buf)
{
	struct goodix_ts_core *core_data =
				dev_get_drvdata(dev);
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
			offset += snprintf(&buf[offset], 4096 - offset,
					"%02x ", cfg_buf[i]);
		}

	}
	kfree(cfg_buf);
	return ret;
}

static int goodix_ts_convert_0x_data(const u8 *buf,
				int buf_size,
				unsigned char *out_buf,
				int *out_buf_len)
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
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	struct goodix_ts_core *core_data =
				dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int en, r;
	const struct firmware *cfg_img;
	struct goodix_ts_config *config = NULL;

	ts_info("******IN");

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	ts_info("en:%d", en);

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
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	struct irq_desc *desc;
	size_t offset = 0;
	int r;

	r = snprintf(&buf[offset], PAGE_SIZE, "irq:%u\n",
			core_data->irq);
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
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	goodix_ts_irq_enable(core_data, en);
	return count;
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;

	if (core->ts_dev->reg.esd == 0)
		return;

	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on == false) {
		ts_esd->esd_on = true;
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
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

/* open short test */
static ssize_t goodix_ts_tp_test_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	struct ts_rawdata_info *info;
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);

	ret = goodix_tools_register();
	if (ret)
		return 0;
	ts_info("test start!");
	info  = kzalloc(sizeof(struct ts_rawdata_info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory\n");
		 return -ENOMEM;
	}
	core_data->ts_dev->board_data->tp_test_flag = TP_TEST_START;
	goodix_ts_esd_off(core_data);
	ret = gtx8_get_rawdata(dev,info);
	goodix_ts_esd_on(core_data);
	core_data->ts_dev->board_data->tp_test_flag = TP_TEST_END;
	if (ret <= 0)
		ts_err("test error!");

	goodix_tools_unregister();

	ret = snprintf(buf, PAGE_SIZE,
			"resultInfo:%s",info->result);
	if (ret < 0){
		ts_err("print result info error!\n");
		ret =  -EINVAL;
		goto TEST_END;
	}

TEST_END:
	kfree(info);
	info = NULL;
	return ret;
}

static int goodix_ts_resume(struct goodix_ts_core *core_data);
static int goodix_ts_suspend(struct goodix_ts_core *core_data);
static ssize_t gtp_fod_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);

	goodix_ts_resume(core_data);
	return snprintf(buf, 10, "%d\n", core_data->fod_status);
}

static ssize_t gtp_fod_status_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);

	ts_info("buf:%s, count:%zu", buf, count);
	sscanf(buf, "%u", &core_data->fod_status);

	goodix_ts_suspend(core_data);
	core_data->gesture_enabled = core_data->double_wakeup || core_data->fod_status ||
		core_data->tid->aod_status;


	return count;
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
static DEVICE_ATTR(fod_status, (S_IRUGO | S_IWUSR | S_IWGRP),
		gtp_fod_status_show, gtp_fod_status_store);

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
	&dev_attr_fod_status.attr,
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
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v)
{
	return blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
}
EXPORT_SYMBOL_GPL(goodix_ts_blocking_notify);

extern int event_sta_sign;
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
	unsigned int touch_num = touch_data->touch_num;
	static u32 pre_fin = 0x0000;
	static u8 pre_key = 0x00;
	int i, id;

	/*first touch down and last touch up condition*/
	if (touch_num != 0 && pre_fin == 0x0000) {
		/*first touch down event*/
		input_report_key(dev, BTN_TOUCH, 1);
	} else if (touch_num == 0 && pre_fin != 0x0000) {
		/*no finger exist*/
		input_report_key(dev, BTN_TOUCH, 0);
	}

	/*report key, include tp's key and pen's key */
	if (unlikely(touch_data->have_key)) {
		for (i = 0; i < ts_bdata->panel_max_key; i++) {
			input_report_key(dev, ts_bdata->panel_key_map[i],
							touch_data->key_value & (1 << i));
		}
		pre_key = touch_data->key_value;
		/*ts_info("$$$$$$pre_key:0x%02x",pre_key);*/
	} else if (pre_key != 0x00) {
		/*ts_info("******no key, by pre_key is not ZERO! pre_key:0x%02x", pre_key);*/
		for (i = 0; i < ts_bdata->panel_max_key; i++) {
			if (pre_key & (1 << i)) {
				input_report_key(dev, ts_bdata->panel_key_map[i], 0);
				pre_key &= ~(1 << i);
				/*ts_info("******report i:%d, key:%d leave", i, ts_bdata->panel_key_map[i]);*/
			}
		}
		/*ts_info("******after, pre_key:0x%02x", pre_key);*/
	}

#if 1
	/*protocol B*/

	/*report pen*/
	if (touch_num >= 1 && touch_data->pen_down) {
		touch_num -= 1;

		input_mt_slot(dev, ts_bdata->panel_max_id * 2);
		input_mt_report_slot_state(dev, MT_TOOL_PEN, true);

		input_report_abs(dev, ABS_MT_POSITION_X, touch_data->pen_coords[0].x);
		input_report_abs(dev, ABS_MT_POSITION_Y, touch_data->pen_coords[0].y);
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR, touch_data->pen_coords[0].w);
		input_report_abs(dev, ABS_MT_PRESSURE, touch_data->pen_coords[0].p);

		pre_fin |= 1 << 20;
		/*ts_info("!!!!!!report pen  DOWN,%d,%d,%d",
				touch_data->pen_coords[0].x,
				touch_data->pen_coords[0].y,
				touch_data->pen_coords[0].p);*/
	} else {
		if (pre_fin & (1 << 20)) {
			input_mt_slot(dev, ts_bdata->panel_max_id * 2);
			input_mt_report_slot_state(dev, MT_TOOL_PEN, false);
			pre_fin &= ~(1 << 20);
			/*ts_info("!!!!!!report pen LEAVE");*/
		}
	}

	/*report finger*/
	id = coords->id;
	for (i = 0; i < ts_bdata->panel_max_id * 2; i++) {
		if (touch_num && i == id) { /* this is a valid touch down event */
			input_mt_slot(dev, id);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
			input_report_key(dev, BTN_TOUCH, !!touch_num);
			input_report_abs(dev, ABS_MT_POSITION_X, coords->x);
			input_report_abs(dev, ABS_MT_POSITION_Y, coords->y);
			input_report_abs(dev, ABS_MT_TOUCH_MAJOR, coords->w);
			input_report_abs(dev, ABS_MT_PRESSURE, coords->p);

			if ((event_sta_sign & 0x88) != 0x88 || !core_data->fod_status)
				coords->overlapping_area = 0;

			input_report_abs(dev, ABS_MT_WIDTH_MINOR, coords->overlapping_area);
			input_report_abs(dev, ABS_MT_WIDTH_MAJOR, coords->overlapping_area);

			__set_bit(i, &core_data->touch_id);
			pre_fin |= 1 << i;
			id = (++coords)->id;
		} else {
			if (pre_fin & (1 << i)) {/* release touch */
				input_mt_slot(dev, i);
				input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
				pre_fin &= ~(1 << i);
				/*ts_info("report leave:%d", i);*/
			}
		}
	}
#endif

	if (event_sta_sign == 0x88 && (core_data->fod_status || core_data->tid->aod_status)) {
		input_report_key(core_data->input_dev, BTN_INFO, 1);
		input_report_key(core_data->input_dev, KEY_INFO, 1);
		core_data->fod_pressed = true;
		ts_info("BTN_INFO press");
	} else if (core_data->fod_pressed) {
		input_report_key(core_data->input_dev, BTN_INFO, 0);
		input_report_key(core_data->input_dev, KEY_INFO, 0);
		ts_info("BTN_INFO release");
		core_data->fod_pressed = false;
	}

	if (!touch_num && core_data->touch_id) {
		for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
		}
		core_data->touch_id = 0;
	}
	input_sync(dev);
	return 0;
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
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_device *ts_dev =  core_data->ts_dev;
	struct goodix_ext_module *ext_module;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	int r;

	core_data->irq_trig_cnt++;
	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	list_for_each_entry(ext_module, &goodix_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		r = ext_module->funcs->irq_event(core_data, ext_module);
		if (r == EVT_CANCEL_IRQEVT) {
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

	return IRQ_HANDLED;
}

/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_irq_setup(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_board_data *ts_bdata =
			board_data(core_data);
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
int goodix_ts_irq_enable(struct goodix_ts_core *core_data,
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
EXPORT_SYMBOL(goodix_ts_irq_enable);
/**
 * goodix_ts_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_power_init(struct goodix_ts_core *core_data)
{
	struct device *dev = NULL;
	struct goodix_ts_board_data *ts_bdata;
	int r = 0;

	ts_info("Power init");
	/* dev:i2c client device or spi slave device*/
	dev =  core_data->ts_dev->dev;
	ts_bdata = board_data(core_data);

	if (ts_bdata->avdd_name) {
		core_data->avdd = devm_regulator_get(dev,
				 ts_bdata->avdd_name);
		if (IS_ERR_OR_NULL(core_data->avdd)) {
			r = PTR_ERR(core_data->avdd);
			ts_err("Failed to get regulator avdd:%d", r);
			core_data->avdd = NULL;
			return r;
		}
	} else {
		ts_info("Avdd name is NULL");
	}

	return r;
}

/**
 * goodix_ts_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata =
			board_data(core_data);
	int r;

	ts_info("Device power on");
	if (core_data->power_on)
		return 0;

	if (core_data->avdd) {
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
	}

	core_data->power_on = 1;
	return 0;
}

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata =
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
	core_data->pin_sta_active = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(core_data->pin_sta_active)) {
		r = PTR_ERR(core_data->pin_sta_active);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_ACTIVE, r);
		goto exit_pinctrl_put;
	}

	/* suspend state */
	core_data->pin_sta_suspend = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(core_data->pin_sta_suspend)) {
		r = PTR_ERR(core_data->pin_sta_suspend);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_SUSPEND, r);
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
	struct goodix_ts_board_data *ts_bdata =
			 board_data(core_data);
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

static void goodix_switch_mode_work(struct work_struct *work)
{
	struct goodix_mode_switch *ms =
	    container_of(work, struct goodix_mode_switch, switch_mode_work);

	struct goodix_ts_core *info = ms->info;
	unsigned char value = ms->mode;

	if (value >= INPUT_EVENT_WAKUP_MODE_OFF
		&& value <= INPUT_EVENT_WAKUP_MODE_ON) {
		info->double_wakeup = value - INPUT_EVENT_WAKUP_MODE_OFF;
		info->gesture_enabled = info->double_wakeup || info->fod_status ||
			info->tid->aod_status;


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
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			0, ts_bdata->panel_max_w, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			0, ts_bdata->panel_max_p, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,
			0, 100, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MINOR,
			0, 100, 0, 0);

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
	__set_bit(BTN_TOOL_PEN, input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

	/* set input parameters */
	goodix_ts_set_input_params(input_dev, ts_bdata);

#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	/*input_mt_init_slots(input_dev, ts_bdata->panel_max_id,
			INPUT_MT_DIRECT);*/
	input_mt_init_slots(input_dev,
			ts_bdata->panel_max_id * 2 + 1,
			INPUT_MT_DIRECT);
#else
	/*input_mt_init_slots(input_dev, ts_bdata->panel_max_id);*/
	input_mt_init_slots(input_dev,
			ts_bdata->panel_max_id * 2 + 1);
#endif
#endif

	input_set_capability(input_dev, EV_KEY, KEY_POWER);
	input_set_capability(input_dev, EV_KEY, BTN_INFO);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);
	input_set_capability(input_dev, EV_KEY, KEY_INFO);
	input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
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
	const struct goodix_ts_hw_ops *hw_ops =
		ts_hw_ops(core_data);
	int r = 0;

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
			r = hw_ops->write(core->ts_dev,
					0x8043, &data, 1);
			if (r < 0)
				ts_err("nanjing esd reset, init static esd FAILED, i2c wirte ERROR");
		}

		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev,
				core->ts_dev->reg.esd,
				&data, 1);
		if (r < 0)
			ts_err("esd reset, init dynamic esd FAILED, i2c write ERROR");

	} else {
		/*init dynamic esd*/
		r = hw_ops->write_trans(core->ts_dev,
				core->ts_dev->reg.esd,
				&data, 1);
		if (r < 0)
			ts_err("esd init watch dog FAILED, i2c write ERROR");
	}

	mutex_lock(&ts_esd->esd_mutex);
	if (ts_esd->esd_on)
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
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
			r = dev->hw_ops->write_trans(core->ts_dev,
				0x8043, &data, 1);
			if (r < 0)
				ts_err("static ESD init ERROR, i2c write failed");
		}

		/*init dynamic esd*/
		r = dev->hw_ops->write_trans(core->ts_dev,
				core->ts_dev->reg.esd,
				&data, 1);
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
static int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int r;

	ts_info("Suspend start");

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
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s", ext_module->name);
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
	core_data->ts_event.event_data.touch_data.touch_num = 0;
	goodix_ts_input_report(core_data->input_dev,
			&core_data->ts_event.event_data.touch_data);
	ts_info("Suspend end");
	return 0;
}

static void release_all_touches(struct goodix_ts_core *core_data)
{
	unsigned int type = MT_TOOL_FINGER;
	int i;

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		input_mt_slot(core_data->input_dev, i);
		input_mt_report_slot_state(core_data->input_dev, type, 0);
	}

	core_data->touch_id = 0;
	input_sync(core_data->input_dev);
}

/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int goodix_ts_resume(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module;
	struct goodix_ts_device *ts_dev =
				core_data->ts_dev;
	int r;

	ts_info("Resume start");
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
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_sta_active);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

	atomic_set(&core_data->suspended, 0);
	/* resume device */
	if (ts_dev && ts_dev->hw_ops->resume)
		ts_dev->hw_ops->resume(ts_dev);

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

	ts_info("fod_pressed:%d", core_data->fod_pressed);
	if (!core_data->fod_pressed) {
		release_all_touches(core_data);
	}

	mutex_unlock(&goodix_modules.mutex);
	ts_debug("Resume end");
	return 0;
}

#ifdef CONFIG_FB
/**
 * goodix_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
static void fb_notify_resume_work(struct work_struct  *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, fb_notify_work);
	ts_info("Try resume in workqueue!\n");
	goodix_ts_resume(core_data);
}
int goodix_ts_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct goodix_ts_core *core_data =
		container_of(self, struct goodix_ts_core, fb_notifier);
	struct msm_drm_notifier *evdata = data;

	if (evdata && evdata->data && core_data) {
		if (event == MSM_DRM_EVENT_BLANK) {
			/* before fb blank */
		} else if (event == MSM_DRM_EARLY_EVENT_BLANK) {
			int *blank = evdata->data;
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				if (!atomic_read(&core_data->suspended)) {
					ts_info("Already in resume state");
					return 0;
				}
				queue_work(system_unbound_wq, &core_data->fb_notify_work);
			} else if (*blank == MSM_DRM_BLANK_POWERDOWN) {
				if (atomic_read(&core_data->suspended)) {
					ts_info("Already in suspend state");
					return 0;
				}
				cancel_work_sync(&core_data->fb_notify_work);
				goodix_ts_suspend(core_data);
			}
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
		container_of(h, struct goodix_ts_core,
			 early_suspend);

	goodix_ts_suspend(core_data);
}
/**
 * goodix_ts_lateresume - Late resume function
 * Called by kernel during system wakeup
 */
static void goodix_ts_lateresume(struct early_suspend *h)
{
	struct goodix_ts_core *core_data =
		container_of(h, struct goodix_ts_core,
			 early_suspend);

	goodix_ts_resume(core_data);
}
#endif

#ifdef CONFIG_PM
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	return goodix_ts_suspend(core_data);
}
/**
 * goodix_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int goodix_ts_pm_resume(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	return goodix_ts_resume(core_data);
}
#endif
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

			goodix_ts_set_input_params(ts_core->input_dev,
					ts_core->ts_dev->board_data);
		}
		break;
	}

exit:
	return 0;

}

static int tid_open_short_test(struct device *dev, struct seq_file *seq,
			       const struct firmware *fw)
{
	int ret = 0;
	struct ts_rawdata_info *info;
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	ret = goodix_tools_register();
	if (ret)
		return 0;
	ts_info("test start!");
	info  = kzalloc(sizeof(struct ts_rawdata_info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory\n");
		 return -ENOMEM;
	}
	core_data->ts_dev->board_data->tp_test_flag = TP_TEST_START;
	goodix_ts_esd_off(core_data);
	ret = gtx8_get_rawdata(dev,info);
	goodix_ts_esd_on(core_data);
	core_data->ts_dev->board_data->tp_test_flag = TP_TEST_END;
	if (ret <= 0)
		ts_err("test error!");

	seq_printf(seq, "buff_Info1:\n");
	for (int i = 0; i < (info->used_size * 2); i++) {
		if (i % 30 == 2)
			seq_putc(seq, '\n');

		seq_printf(seq, "%8d",info->buff[i]);
	}

	seq_putc(seq, '\n');
	seq_printf(seq, "resultInfo:%s\n",info->result);
	goodix_tools_unregister();
	kfree(info);
	info = NULL;
	return ret;
}

static int tid_get_lockdown_info(struct device *dev, char *out_values)
{
	int ret;
	ret = goodix_get_lockdowninfo(goodix_core_data, out_values);
	if (ret) {
		ts_err("get lockdown info error");
		return 1;
	}
	return 0;
}

static int tid_fod_status_set(struct device *dev, int fod_values)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);

	ts_info("fod_values:%d", fod_values);
	core_data->fod_status = fod_values;
 	core_data->gesture_enabled = core_data->double_wakeup || fod_values ||
 		core_data->tid->aod_status;

	return 0;
}

static int tid_grip_area_set(struct device *dev, int grip_value)
{
	unsigned char regul_val[3];

	switch (grip_value) {
	case 0:
		regul_val[0] = 0xf2;
		regul_val[1] = 0x00;
		regul_val[2] = 0x0e;
		break;
	case 1:
		regul_val[0] = 0xec;
		regul_val[1] = 0x06;
		regul_val[2] = 0x0e;
		break;
	case 90:
		regul_val[0] = 0xe9;
		regul_val[1] = 0x09;
		regul_val[2] = 0x0e;
		break;
	case 180:
		regul_val[0] = 0xec;
		regul_val[1] = 0x06;
		regul_val[2] = 0x0e;
		break;
	case 270:
		regul_val[0] = 0xe9;
		regul_val[1] = 0x09;
		regul_val[2] = 0x0e;
		break;
	default: return -EPERM;
	}

	return goodix_i2c_write(dev->platform_data, TS_REG_CMD, regul_val, 2);
}

static int tid_get_version(struct device *dev, unsigned int *major,
			   unsigned int *minor)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_version chip_ver;
	int r;

	if (ts_dev->hw_ops->read_version) {
		r = ts_dev->hw_ops->read_version(ts_dev, &chip_ver);
		if (!r && chip_ver.valid) {
			*major = chip_ver.vid[0] << 8 | chip_ver.vid[1];
			*minor = chip_ver.vid[2] << 8 | chip_ver.vid[3];
			return 0;
		}
	}
	return -EPERM;
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
				if (temp_value >= 35 && temp_value < 40)
					goodix_game_value |= 0x01;
				else if (temp_value >= 40 && temp_value < 45)
					goodix_game_value |= 0x10;
				else if (temp_value >= 45 && temp_value <= 50)
					goodix_game_value |= 0x11;
				break;
		case Touch_Tolerance:
				temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
				goodix_game_value &= 0xF3;
				if (temp_value >= 60 && temp_value < 150)
					goodix_game_value |= (0x00 << 2);
				else if (temp_value >= 150 && temp_value < 200)
					goodix_game_value |= (0x01 << 2);
				else if (temp_value >= 200 && temp_value < 220)
					goodix_game_value |= (0x10 << 2);
				else if (temp_value >= 220 && temp_value <= 255)
					goodix_game_value |= (0x11 << 2);
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
				if (temp_value == 0 || temp_value == 2) {
					goodix_game_value &= 0x3F;
				} else if (temp_value == 1 || temp_value == 3) {
					goodix_game_value &= 0x3F;
					goodix_game_value |= ((temp_value + 1) << 5);
				}
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
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 50;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 35;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 35;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 35;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 35;

	/*  Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 255;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 64;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 200;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 200;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 200;

	/*	edge filter */
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;

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

int goodix_palm_sensor_write(int value)
{
	int ret = 0;
	goodix_core_data->palm_sensor_switch = !!value;

	return ret;
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
	struct touch_info_dev *tid;
	struct touch_info_dev_operations *tid_ops;
	int r;
	u8 read_val = 0;

	ts_info("goodix_ts_probe IN");

	ts_device = pdev->dev.platform_data;
	if (!ts_device || !ts_device->hw_ops ||
			!ts_device->board_data) {
		ts_err("Invalid touch device");
		return -ENODEV;
	}

	core_data = devm_kzalloc(&pdev->dev, sizeof(struct goodix_ts_core),
						GFP_KERNEL);
	if (!core_data) {
		ts_err("Failed to allocate memory for core data");
		return -ENOMEM;
	}
	goodix_core_data = core_data;
	/* touch core layer is a platform driver */
	core_data->pdev = pdev;
	pdev->dev.of_node = ts_device->dev->of_node;
	core_data->ts_dev = ts_device;
	ts_device->board_data->tp_test_flag = TP_TEST_END;
	platform_set_drvdata(pdev, core_data);
	core_data->cfg_group_parsed = false;

	tid = devm_tid_and_ops_allocate(&pdev->dev);
	if (unlikely(!tid))
		return -ENOMEM;

	core_data->tid = tid;
	tid_ops = tid->tid_ops;
	tid_ops->open_short_test = tid_open_short_test;
	tid_ops->get_lockdown_info = tid_get_lockdown_info;
	tid_ops->fod_status_set = tid_fod_status_set;
	tid_ops->grip_area_set = tid_grip_area_set;
	tid_ops->get_version = tid_get_version;

	r = goodix_ts_power_init(core_data);
	if (r < 0)
		goto out;

	r = goodix_ts_power_on(core_data);
	if (r < 0)
		goto out;

#ifdef CONFIG_PINCTRL
	/* Pinctrl handle is optional. */
	r = goodix_ts_pinctrl_init(core_data);
	if (!r && core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_sta_active);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

	/* get GPIO resource */
	r = goodix_ts_gpio_setup(core_data);
	if (r < 0)
		goto out;
#if CONFIG_FB
	INIT_WORK(&core_data->fb_notify_work, fb_notify_resume_work);
#endif
	/*create sysfs files*/
	goodix_ts_sysfs_init(core_data);

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	core_data->gtp_tp_class = get_xiaomi_touch_class();
#endif

	r = ts_device->hw_ops->reset(ts_device);
	if (r < 0)
		goto out;

	/*i2c test*/
	r = ts_device->hw_ops->read_trans(ts_device, 0x3100,
			&read_val, 1);
	if (!r)
		ts_info("i2c test SUCCESS");
	else {
		ts_err("i2c test FAILED");
		goto out;
	}

	/*unified protocl
	 * start a thread to parse cfg_bin and init IC*/
	r = goodix_start_cfg_bin(core_data);
	if (!r) {
		ts_info("***start cfg_bin_proc SUCCESS");
	} else {
		ts_err("***start cfg_bin_proc FAILED");
		goto out;
	}

	r = devm_tid_register(&pdev->dev, tid);
    	if (unlikely(r))
		return r;

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	xiaomi_touch_interfaces.getModeValue = gtp_get_mode_value;
	xiaomi_touch_interfaces.setModeValue = gtp_set_cur_value;
	xiaomi_touch_interfaces.resetMode = gtp_reset_mode;
	xiaomi_touch_interfaces.getModeAll = gtp_get_mode_all;
	xiaomi_touch_interfaces.palm_sensor_write = goodix_palm_sensor_write;
	xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
	gtp_init_touchmode_data();
#endif
out:
	ts_info("goodix_ts_probe OUT, r:%d", r);
	return r;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data =
		platform_get_drvdata(pdev);

	goodix_ts_power_off(core_data);
	goodix_debugfs_exit();
	goodix_ts_sysfs_exit(core_data);
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops dev_pm_ops = {
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = goodix_ts_pm_suspend,
	.resume = goodix_ts_pm_resume,
#endif
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
	ts_info("Core layer init");

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
