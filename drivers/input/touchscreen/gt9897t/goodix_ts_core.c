 /*
  * Goodix Touchscreen Driver
  * Copyright (C) 2020 - 2021 Goodix, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#include "goodix_ts_core.h"

#define GOODIX_DEFAULT_CFG_NAME		"goodix_cfg_group.cfg" /* this cfg file for tool debug */
#define GOOIDX_INPUT_PHYS				"goodix_ts/input0"
#define CORE_MODULE_UNPROBED			0
#define CORE_MODULE_PROB_SUCCESS		1
#define CORE_MODULE_PROB_FAILED			-1
#define CORE_MODULE_REMOVED				-2
int core_module_prob_sate = CORE_MODULE_UNPROBED;
struct goodix_module goodix_modules;
struct goodix_ts_core *goodix_core_data;
extern char *saved_command_line;


static int goodix_send_ic_config(struct goodix_ts_core *cd, int type);
/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 * return 0 on success, otherwise return < 0
 */
static int __do_register_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module, *next;
	struct list_head *insert_point = &goodix_modules.head;

	ts_info("__do_register_ext_module IN");
	/* prority level *must* be set */
	if (module->priority == EXTMOD_PRIO_RESERVED) {
		ts_err("Priority of module [%s] needs to be set",
				module->name);
		return -EINVAL;
	}
	mutex_lock(&goodix_modules.mutex);
	/* find insert point for the specified priority */
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (ext_module == module) {
				ts_info("Module [%s] already exists",
					module->name);
				mutex_unlock(&goodix_modules.mutex);
				return 0;
			}
		}

		/* smaller priority value with higher priority level */
		list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
			if (ext_module->priority >= module->priority) {
				insert_point = &ext_module->list;
				break;
			}
		}
	}

	if (module->funcs && module->funcs->init) {
		if (module->funcs->init(goodix_modules.core_data,
					module) < 0) {
			ts_err("Module [%s] init error",
					module->name ? module->name : " ");
			mutex_unlock(&goodix_modules.mutex);
			return -EFAULT;
		}
	}

	list_add(&module->list, insert_point->prev);
	mutex_unlock(&goodix_modules.mutex);

	ts_info("Module [%s] registered,priority:%u", module->name,
		module->priority);
	return 0;
}

static void goodix_register_ext_module_work(struct work_struct *work) {
	struct goodix_ext_module *module =
			container_of(work, struct goodix_ext_module, work);

	ts_info("module register work IN");
	if (core_module_prob_sate == CORE_MODULE_PROB_FAILED
		|| core_module_prob_sate == CORE_MODULE_REMOVED) {
		ts_err("core layer state error %d", core_module_prob_sate);
		return;
	}

	if (core_module_prob_sate == CORE_MODULE_UNPROBED) {
		/* waitting for core layer */
		if (!wait_for_completion_timeout(&goodix_modules.core_comp,
						25 * HZ)) {
			ts_err("Module [%s] timeout", module->name);
			return;
		}
	}

	/* driver probe failed */
	if (core_module_prob_sate != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return;
	}

	if (__do_register_ext_module(module))
		ts_err("failed register module: %s", module->name);
	else
		ts_info("success register module: %s", module->name);
}

static void goodix_core_module_init(void)
{
	if (goodix_modules.initilized)
		return;
	goodix_modules.initilized = true;
	INIT_LIST_HEAD(&goodix_modules.head);
	mutex_init(&goodix_modules.mutex);
	init_completion(&goodix_modules.core_comp);
}

/**
 * goodix_register_ext_module - interface for register external module
 * to the core. This will create a workqueue to finish the real register
 * work and return immediately. The user need to check the final result
 * to make sure registe is success or fail.
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;

	ts_info("goodix_register_ext_module IN");

	goodix_core_module_init();
	INIT_WORK(&module->work, goodix_register_ext_module_work);
	schedule_work(&module->work);

	ts_info("goodix_register_ext_module OUT");
	return 0;
}
EXPORT_SYMBOL_GPL(goodix_register_ext_module);

/**
 * goodix_register_ext_module_no_wait
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module_no_wait(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;
	ts_info("goodix_register_ext_module_no_wait IN");
	goodix_core_module_init();
	/* driver probe failed */
	if (core_module_prob_sate != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return -EINVAL;
	}
	return __do_register_ext_module(module);
}
EXPORT_SYMBOL_GPL(goodix_register_ext_module_no_wait);

/* remove all registered ext module
 * return 0 on success, otherwise return < 0
 */
int goodix_unregister_all_module(void)
{
	int ret = 0;
	struct goodix_ext_module *ext_module, *next;

	if (!goodix_modules.initilized)
		return 0;

	if (!goodix_modules.core_data)
		return 0;

	mutex_lock(&goodix_modules.mutex);
	if (list_empty(&goodix_modules.head)) {
		mutex_unlock(&goodix_modules.mutex);
		return 0;
	}

	list_for_each_entry_safe(ext_module, next,
				 &goodix_modules.head, list) {
		if (ext_module->funcs && ext_module->funcs->exit) {
			ret = ext_module->funcs->exit(goodix_modules.core_data,
					ext_module);
			if (ret) {
				ts_err("failed register ext module, %d:%s",
					ret, ext_module->name);
				break;
			}
		}
		ts_info("remove module: %s", ext_module->name);
		list_del(&ext_module->list);
	}
	mutex_unlock(&goodix_modules.mutex);
	return ret;
}

/**
 * goodix_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module, *next;
	bool found = false;

	if (!module)
		return -EINVAL;

	if (!goodix_modules.initilized)
		return -EINVAL;

	if (!goodix_modules.core_data)
		return -ENODEV;

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (ext_module == module) {
				found = true;
				break;
			}
		}
	} else {
		mutex_unlock(&goodix_modules.mutex);
		return 0;
	}

	if (!found) {
		ts_debug("Module [%s] never registed",
				module->name);
		mutex_unlock(&goodix_modules.mutex);
		return 0;
	}

	list_del(&module->list);
	mutex_unlock(&goodix_modules.mutex);

	if (module->funcs && module->funcs->exit)
		module->funcs->exit(goodix_modules.core_data, module);

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


/* show rawdata */
static ssize_t goodix_ts_get_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct ts_rawdata_info *info = NULL;
	int test_result;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory");
		return -ENOMEM;
	}

	if (goodix_get_rawdata(dev, info)) {
		ts_err("Factory_test FAIL");
		test_result = 1;
		goto exit;
	}

	if (80 == (*(info->result + 1))) {
		ts_info("test PASS!");
		test_result = 0;
	} else {
		ts_err("test FAILED!");
		test_result = 1;
	}

exit:
	ret = snprintf(buf, sizeof(test_result), "%d\n", test_result);
	ts_info("resultInfo: %s", info->result);
	/* ret = snprintf(buf, PAGE_SIZE, "resultInfo: %s", info->result); */

	kfree(info);
	return ret;
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
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_fw_version chip_ver;
	int ret;
	int cnt = -EINVAL;

	if (hw_ops->read_version) {
		ret = hw_ops->read_version(core_data, &chip_ver);
		if (!ret) {
			cnt = snprintf(&buf[0], PAGE_SIZE,
				"rom_pid:%s\nrom_vid:%02x%02x%02x\n",
				chip_ver.rom_pid, chip_ver.rom_vid[0],
				chip_ver.rom_vid[1], chip_ver.rom_vid[2]);
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"patch_pid:%s\npatch_vid:%02x%02x%02x%02x\n",
				chip_ver.patch_pid, chip_ver.patch_vid[0],
				chip_ver.patch_vid[1], chip_ver.patch_vid[2],
				chip_ver.patch_vid[3]);
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"sensorid:%d\n", chip_ver.sensor_id);
		}
	}

	if (hw_ops->get_ic_info) {
		ret = hw_ops->get_ic_info(core_data, &core_data->ic_info, false);
		if (!ret) {
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"config_id:%x\n", core_data->ic_info.version.config_id);
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"config_version:%x\n", core_data->ic_info.version.config_version);
		}
	}

	return cnt;
}

/* reset chip */
static ssize_t goodix_ts_reset_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0')
		hw_ops->reset(core_data, GOODIX_NORMAL_RESET_DELAY_MS);
	return count;
}

/* read config */
static ssize_t goodix_ts_read_cfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;
	int i;
	int offset;
	char *cfg_buf = NULL;

	cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cfg_buf)
		return -ENOMEM;

	if (hw_ops->read_config)
		ret = hw_ops->read_config(core_data, cfg_buf, PAGE_SIZE);
	else
		ret = -EINVAL;

	if (ret > 0) {
		offset = 0;
		for (i = 0; i < 200; i++) { /* only print 200 bytes */
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					"%02x,", cfg_buf[i]);
			if ((i + 1) % 20 == 0)
				buf[offset++] = '\n';
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

static int goodix_ts_convert_0x_data(const u8 *buf, int buf_size,
					u8 *out_buf, int *out_buf_len)
{
	int i, m_size = 0;
	int temp_index = 0;
	u8 high, low;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X')
			m_size++;
	}

	if (m_size <= 1) {
		ts_err("cfg file ERROR, valid data count:%d", m_size);
		return -EINVAL;
	}
	*out_buf_len = m_size;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] != 'x' && buf[i] != 'X')
			continue;

		if (temp_index >= m_size) {
			ts_err("exchange cfg data error, overflow,"
				"temp_index:%d,m_size:%d",
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

/* send config */
static ssize_t goodix_ts_send_cfg_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ic_config *config = NULL;
	const struct firmware *cfg_img = NULL;
	int en;
	int ret;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	hw_ops->irq_enable(core_data, false);

	ret = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
	if (ret < 0) {
		ts_err("cfg file [%s] not available,errno:%d",
			GOODIX_DEFAULT_CFG_NAME, ret);
		goto exit;
	} else {
		ts_info("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);
	}

	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		goto exit;

	if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
			config->data, &config->len)) {
		ts_err("convert config data FAILED");
		goto exit;
	}

	if (hw_ops->send_config) {
		ret = hw_ops->send_config(core_data, config->data, config->len);
		if (ret < 0)
			ts_err("send config failed");
	}

exit:
	hw_ops->irq_enable(core_data, true);
	kfree(config);
	if (cfg_img)
		release_firmware(cfg_img);

	return count;
}

/* reg read/write */
static u32 rw_addr;
static u32 rw_len;
static u8 rw_flag;
static u8 store_buf[32];
static u8 show_buf[PAGE_SIZE];
static ssize_t goodix_ts_reg_rw_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (!rw_addr || !rw_len) {
		ts_err("address(0x%x) and length(%d) can't be null",
			rw_addr, rw_len);
		return -EINVAL;
	}

	if (rw_flag != 1) {
		ts_err("invalid rw flag %d, only support [1/2]", rw_flag);
		return -EINVAL;
	}

	ret = hw_ops->read(core_data, rw_addr, show_buf, rw_len);
	if (ret < 0) {
		ts_err("failed read addr(%x) length(%d)", rw_addr, rw_len);
		return snprintf(buf, PAGE_SIZE, "failed read addr(%x), len(%d)\n",
			rw_addr, rw_len);
	}

	return snprintf(buf, PAGE_SIZE, "0x%x,%d {%*ph}\n",
		rw_addr, rw_len, rw_len, show_buf);
}

static ssize_t goodix_ts_reg_rw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	char *pos = NULL;
	char *token = NULL;
	long result = 0;
	int ret;
	int i;

	if (!buf || !count) {
		ts_err("invalid parame");
		goto err_out;
	}

	if (buf[0] == 'r') {
		rw_flag = 1;
	} else if (buf[0] == 'w') {
		rw_flag = 2;
	} else {
		ts_err("string must start with 'r/w'");
		goto err_out;
	}

	/* get addr */
	pos = (char *)buf;
	pos += 2;
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid address info");
		goto err_out;
	} else {
		if (kstrtol(token, 16, &result)) {
			ts_err("failed get addr info");
			goto err_out;
		}
		rw_addr = (u32)result;
		ts_info("rw addr is 0x%x", rw_addr);
	}

	/* get length */
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid length info");
		goto err_out;
	} else {
		if (kstrtol(token, 0, &result)) {
			ts_err("failed get length info");
			goto err_out;
		}
		rw_len = (u32)result;
		ts_info("rw length info is %d", rw_len);
		if (rw_len > sizeof(store_buf)) {
			ts_err("data len > %lu", sizeof(store_buf));
			goto err_out;
		}
	}

	if (rw_flag == 1)
		return count;

	for (i = 0; i < rw_len; i++) {
		token = strsep(&pos, ":");
		if (!token) {
			ts_err("invalid data info");
			goto err_out;
		} else {
			if (kstrtol(token, 16, &result)) {
				ts_err("failed get data[%d] info", i);
				goto err_out;
			}
			store_buf[i] = (u8)result;
			ts_info("get data[%d]=0x%x", i, store_buf[i]);
		}
	}
	ret = hw_ops->write(core_data, rw_addr, store_buf, rw_len);
	if (ret < 0) {
		ts_err("failed write addr(%x) data %*ph", rw_addr,
			rw_len, store_buf);
		goto err_out;
	}

	ts_info("%s write to addr (%x) with data %*ph",
		"success", rw_addr, rw_len, store_buf);

	return count;
err_out:
	snprintf(show_buf, PAGE_SIZE, "%s\n",
		"invalid params, format{r/w:4100:length:[41:21:31]}");
	return -EINVAL;

}

/* show irq infomation */
static ssize_t goodix_ts_irq_info_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
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
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		hw_ops->irq_enable(core_data, true);
	else
		hw_ops->irq_enable(core_data, false);
	return count;
}

/* show esd status */
static ssize_t goodix_ts_esd_info_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
		     atomic_read(&ts_esd->esd_on) ?
		     "enabled" : "disabled");

	return r;
}

/* enable/disable esd */
static ssize_t goodix_ts_esd_info_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	else
		goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	return count;
}

static DEVICE_ATTR(get_rawdata, S_IRUGO, goodix_ts_get_rawdata_show, NULL);
static DEVICE_ATTR(driver_info, S_IRUGO, goodix_ts_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, S_IRUGO, goodix_ts_chip_info_show, NULL);
static DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, S_IWUSR | S_IWGRP, NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, S_IRUGO, goodix_ts_read_cfg_show, NULL);
static DEVICE_ATTR(reg_rw, S_IRUGO | S_IWUSR | S_IWGRP,
					goodix_ts_reg_rw_show, goodix_ts_reg_rw_store);
static DEVICE_ATTR(irq_info, S_IRUGO | S_IWUSR | S_IWGRP,
					goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(esd_info, S_IRUGO | S_IWUSR | S_IWGRP,
					goodix_ts_esd_info_show, goodix_ts_esd_info_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_get_rawdata.attr,
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_reg_rw.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_esd_info.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static int goodix_ts_sysfs_init(struct goodix_ts_core *core_data)
{
	int ret;

	ret = sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
	if (ret) {
		ts_err("failed create core sysfs group");
		return ret;
	}

	return ret;
}

static void goodix_ts_sysfs_exit(struct goodix_ts_core *core_data)
{
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

static void goodix_ts_proc_exit(struct goodix_ts_core *core_data)
{
	proc_remove(core_data->tp_lockdown_info_proc);
	proc_remove(core_data->tp_fw_version_proc);
	proc_remove(core_data->tp_selftest_proc);
}

static void goodix_ts_wq_exit(struct goodix_ts_core *core_data)
{
	destroy_workqueue(core_data->event_wq);
	destroy_workqueue(core_data->gesture_wq);
}
#ifdef CONFIG_DEBUG_FS
static void goodix_ts_debugfs_exit(struct goodix_ts_core *core_data)
{
	debugfs_remove_recursive(core_data->debugfs);
}
#endif
/* prosfs create */
static int rawdata_proc_show(struct seq_file *m, void *v)
{
	struct ts_rawdata_info *info;
	struct goodix_ts_core *cd = goodix_modules.core_data;
	int tx;
	int rx;
	int ret;
	int i;
	int index;

	if (!m || !v || !cd) {
		ts_err("rawdata_proc_show, input null ptr");
		return -EIO;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory");
		return -ENOMEM;
	}

	ret = cd->hw_ops->get_capacitance_data(cd, info);
	if (ret < 0) {
		ts_err("failed to get_capacitance_data, exit!");
		goto exit;
	}

	rx = info->buff[0];
	tx = info->buff[1];
	seq_printf(m, "TX:%d  RX:%d\n", tx, rx);

	seq_printf(m, "mutual_rawdata:\n");
	index = 2;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%4d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}

	seq_printf(m, "mutual_diffdata:\n");
	index += tx * rx;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%4d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}

	seq_printf(m, "mutual_refdata:\n");
	index += tx * rx;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%4d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}

exit:
	kfree(info);
	return ret;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rawdata_proc_show, NULL);
}

static const struct file_operations rawdata_proc_fops = {
	.open = rawdata_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void goodix_ts_procfs_init(void)
{
	if (!proc_mkdir("goodix_ts", NULL))
		return;
	proc_create("goodix_ts/tp_capacitance_data",
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, NULL,
		&rawdata_proc_fops);
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
	int ret;

	ret = blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
	return ret;
}
EXPORT_SYMBOL_GPL(goodix_ts_blocking_notify);

#ifdef CONFIG_OF
/**
 * goodix_parse_dt_resolution - parse resolution from dt
 * @node: devicetree node
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt_resolution(struct device_node *node,
		struct goodix_ts_board_data *board_data)
{
	int ret;

	ret = of_property_read_u32(node, "goodix,panel-max-x",
				&board_data->panel_max_x);
	if (ret) {
		ts_err("failed get panel-max-x");
		return ret;
	}

	ret = of_property_read_u32(node, "goodix,panel-max-y",
				&board_data->panel_max_y);
	if (ret) {
		ts_err("failed get panel-max-y");
		return ret;
	}

	ret = of_property_read_u32(node, "goodix,panel-max-w",
				&board_data->panel_max_w);
	if (ret) {
		ts_err("failed get panel-max-w");
		return ret;
	}

	ret = of_property_read_u32(node, "goodix,panel-max-p",
				 &board_data->panel_max_p);
	if (ret) {
		ts_err("failed get panel-max-p, use default");
		board_data->panel_max_p = GOODIX_PEN_MAX_PRESSURE;
	}

	return 0;
}

/**
 * goodix_parse_dt - parse board data from dt
 * @dev: pointer to device
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt(struct device_node *node,
	struct goodix_ts_board_data *board_data)
{
	const char *name_tmp;
	int r;

	if (!board_data) {
		ts_err("invalid board data");
		return -EINVAL;
	}

	r = of_get_named_gpio(node, "goodix,avdd-gpio", 0);
	if (r < 0) {
		ts_info("can't find avdd-gpio, use other power supply");
		board_data->avdd_gpio = 0;
	} else {
		ts_info("get avdd-gpio[%d] from dt", r);
		board_data->avdd_gpio = r;
	}

	r = of_get_named_gpio(node, "goodix,iovdd-gpio", 0);
	if (r < 0) {
		ts_info("can't find iovdd-gpio, use other power supply");
		board_data->iovdd_gpio = 0;
	} else {
		ts_info("get iovdd-gpio[%d] from dt", r);
		board_data->iovdd_gpio = r;
	}

	r = of_get_named_gpio(node, "goodix,reset-gpio", 0);
	if (r < 0) {
		ts_err("invalid reset-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get reset-gpio[%d] from dt", r);
	board_data->reset_gpio = r;

	r = of_get_named_gpio(node, "goodix,irq-gpio", 0);
	if (r < 0) {
		ts_err("invalid irq-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get irq-gpio[%d] from dt", r);
	board_data->irq_gpio = r;

	r = of_property_read_u32(node, "goodix,irq-flags",
			&board_data->irq_flags);
	if (r) {
		ts_err("invalid irq-flags");
		return -EINVAL;
	}

	memset(board_data->pmavdd_name, 0, sizeof(board_data->pmavdd_name));
	r = of_property_read_string(node, "goodix,pmavdd-name", &name_tmp);
	if (!r) {
		ts_info("pmavdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->pmavdd_name))
			strncpy(board_data->pmavdd_name,
				name_tmp, sizeof(board_data->pmavdd_name));
		else
			ts_info("invalied pmavdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->pmavdd_name));
	}

	memset(board_data->avdd_name, 0, sizeof(board_data->avdd_name));
	r = of_property_read_string(node, "goodix,avdd-name", &name_tmp);
	if (!r) {
		ts_info("avdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->avdd_name))
			strncpy(board_data->avdd_name,
				name_tmp, sizeof(board_data->avdd_name));
		else
			ts_info("invalied avdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->avdd_name));
	}

	memset(board_data->iovdd_name, 0, sizeof(board_data->iovdd_name));
	r = of_property_read_string(node, "goodix,iovdd-name", &name_tmp);
	if (!r) {
		ts_info("iovdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->iovdd_name))
			strncpy(board_data->iovdd_name,
				name_tmp, sizeof(board_data->iovdd_name));
		else
			ts_info("invalied iovdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->iovdd_name));
	}

	/* get xyz resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	if (r) {
		ts_err("Failed to parse resolutions:%d", r);
		return r;
	}


	/*get pen-enable switch and pen keys, must after "key map"*/
	board_data->pen_enable = of_property_read_bool(node,
					"goodix,pen-enable");
	if (board_data->pen_enable)
		ts_info("goodix pen enabled");

	ts_debug("[DT]x:%d, y:%d, w:%d, p:%d", board_data->panel_max_x,
		board_data->panel_max_y, board_data->panel_max_w,
		board_data->panel_max_p);
	return 0;
}
#endif

static void goodix_ts_report_pen(struct input_dev *dev,
		struct goodix_pen_data *pen_data)
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

static void goodix_ts_report_finger(struct input_dev *dev,
		struct goodix_touch_data *touch_data)
{
	struct goodix_ts_core *core_data = input_get_drvdata(dev);
	unsigned int touch_num = touch_data->touch_num;
	static u32 pre_fin;
	int i;

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (!touch_data->coords[i].status)
			continue;
		if (touch_data->coords[i].status == TS_RELEASE) {
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
			if (__test_and_clear_bit(i, &core_data->touch_id)) {
				ts_info("finger report leave:%d", i);
			}
			continue;
		}
		/* ts_debug("report: id %d, x %d, y %d, w %d", i,
			touch_data->coords[i].x, touch_data->coords[i].y,
			touch_data->coords[i].p); */
		input_mt_slot(dev, i);
		input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
		input_report_abs(dev, ABS_MT_POSITION_X,
				touch_data->coords[i].x);
		input_report_abs(dev, ABS_MT_POSITION_Y,
				touch_data->coords[i].y);
		if (!__test_and_set_bit(i, &core_data->touch_id)) {
			ts_info("finger report press:%d", i);
		}
	}

	/*first touch down and last touch up condition*/
	if (touch_num && !pre_fin)
		input_report_key(dev, BTN_TOUCH, 1);
	else if (!touch_num && pre_fin)
		input_report_key(dev, BTN_TOUCH, 0);

	pre_fin = touch_num;

	input_sync(dev);
}

static int goodix_ts_request_handle(struct goodix_ts_core *cd,
	struct goodix_ts_event *ts_event)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	int ret = -1;

	if (ts_event->request_code == REQUEST_TYPE_CONFIG)
		ret = goodix_send_ic_config(cd, CONFIG_TYPE_NORMAL);
	else if (ts_event->request_code == REQUEST_TYPE_RESET)
		ret = hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS);
	else
		ts_info("can not handle request type 0x%x",
			ts_event->request_code);
	if (ret)
		ts_err("failed handle request 0x%x",
			ts_event->request_code);
	else
		ts_info("success handle ic request 0x%x",
			ts_event->request_code);
	return ret;
}

/**
 * goodix_ts_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static irqreturn_t goodix_ts_threadirq_func(int irq, void *data)
{
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
	unsigned long  us;
	int ret;

	core_data->irq_trig_cnt++;

#ifdef CONFIG_PM
	if (core_data->tp_pm_suspend) {
		ts_info("device in suspend, wait to resume");
		ret = wait_for_completion_timeout(&core_data->pm_resume_completion, msecs_to_jiffies(300));
		if (!ret) {
			ts_err("system can't finished resuming procedure");
			return IRQ_HANDLED;
		}
	}
#endif

	if (atomic_read(&ts_esd->esd_on)) {
		cancel_delayed_work_sync(&ts_esd->esd_work);
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
	}

	if (core_data->tp_qos_on) {
		us = 20*1000;
		pm_qos_update_request_timeout(&core_data->tp_qos_request, 500, us);
	}

	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	list_for_each_entry_safe(ext_module, next,
				&goodix_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		ret = ext_module->funcs->irq_event(core_data, ext_module);
		if (ret == EVT_CANCEL_IRQEVT) {
			mutex_unlock(&goodix_modules.mutex);
			return IRQ_HANDLED;
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/* read touch data from touch device */
	ret = hw_ops->event_handler(core_data, ts_event);
	if (likely(!ret)) {
		if (ts_event->event_type == EVENT_TOUCH) {
			/* report touch */
			goodix_ts_report_finger(core_data->input_dev,
					&ts_event->touch_data);
		}
		if (ts_event->event_type == EVENT_PEN) {
			goodix_ts_report_pen(core_data->pen_dev,
					&ts_event->pen_data);
		}
		if (ts_event->event_type == EVENT_REQUEST) {
			goodix_ts_request_handle(core_data, ts_event);
		}
	}

	if (!core_data->tools_ctrl_sync)
		hw_ops->after_event_handler(core_data);
	return IRQ_HANDLED;
}

/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_irq_setup(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int ret;

	/* if ts_bdata-> irq is invalid */
	core_data->irq = gpio_to_irq(ts_bdata->irq_gpio);
	if (core_data->irq < 0) {
		ts_err("failed get irq num %d", core_data->irq);
		return -EINVAL;
	}

	ts_info("IRQ:%u,flags:%d", core_data->irq, (int)ts_bdata->irq_flags);
	ret = devm_request_threaded_irq(&core_data->pdev->dev,
					core_data->irq, NULL,
					goodix_ts_threadirq_func,
					ts_bdata->irq_flags | IRQF_ONESHOT,
					GOODIX_CORE_DRIVER_NAME,
					core_data);
	if (ret < 0)
		ts_err("Failed to requeset threaded irq:%d", ret);
	else
		atomic_set(&core_data->irq_enabled, 1);

	return ret;
}

/**
 * goodix_ts_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_power_init(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct device *dev = core_data->bus->dev;
	int ret = 0;

	ts_info("Power init");
	if (strlen(ts_bdata->avdd_name)) {
		core_data->avdd = devm_regulator_get(dev,
				ts_bdata->avdd_name);
		if (IS_ERR_OR_NULL(core_data->avdd)) {
			ret = PTR_ERR(core_data->avdd);
			ts_err("Failed to get regulator avdd:%d", ret);
			core_data->avdd = NULL;
			return ret;
		}
	} else {
		ts_info("Avdd name is NULL");
	}

	if (strlen(ts_bdata->pmavdd_name)) {
		core_data->pmavdd = devm_regulator_get(dev,
				ts_bdata->pmavdd_name);
		if (IS_ERR_OR_NULL(core_data->pmavdd)) {
			ret = PTR_ERR(core_data->pmavdd);
			ts_err("Failed to get regulator pmavdd:%d", ret);
			core_data->pmavdd = NULL;
			return ret;
		}
	} else {
		ts_info("pmAvdd name is NULL");
	}

	if (strlen(ts_bdata->iovdd_name)) {
		core_data->iovdd = devm_regulator_get(dev,
				ts_bdata->iovdd_name);
		if (IS_ERR_OR_NULL(core_data->iovdd)) {
			ret = PTR_ERR(core_data->iovdd);
			ts_err("Failed to get regulator iovdd:%d", ret);
			core_data->iovdd = NULL;
		}
	} else {
		ts_info("iovdd name is NULL");
	}

	return ret;
}

/**
 * goodix_ts_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on(struct goodix_ts_core *cd)
{
	int ret = 0;

	ts_info("power on");
	if (cd->power_on)
		return 0;

	ret = cd->hw_ops->power_on(cd, true);
	if (!ret)
		cd->power_on = 1;
	else
		ts_err("failed power on, %d", ret);
	return ret;
}

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_core *cd)
{
	int ret;

	ts_info("Device power off");
	if (!cd->power_on)
		return 0;

	ret = cd->hw_ops->power_on(cd, false);
	if (!ret)
		cd->power_on = 0;
	else
		ts_err("failed power off, %d", ret);

	return ret;
}

/**
 * goodix_ts_gpio_setup - Request gpio resources from GPIO subsysten
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
	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->reset_gpio,
				GPIOF_OUT_INIT_LOW, "ts_reset_gpio");
	if (r < 0) {
		ts_err("Failed to request reset gpio, r:%d", r);
		return r;
	}

	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->irq_gpio,
				GPIOF_IN, "ts_irq_gpio");
	if (r < 0) {
		ts_err("Failed to request irq gpio, r:%d", r);
		return r;
	}

	if (ts_bdata->avdd_gpio > 0) {
		r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->avdd_gpio,
				GPIOF_OUT_INIT_LOW, "ts_avdd_gpio");
		if (r < 0) {
			ts_err("Failed to request avdd-gpio, r:%d", r);
			return r;
		}
	}

	if (ts_bdata->iovdd_gpio > 0) {
		r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->iovdd_gpio,
				GPIOF_OUT_INIT_LOW, "ts_iovdd_gpio");
		if (r < 0) {
			ts_err("Failed to request iovdd-gpio, r:%d", r);
			return r;
		}
	}

	return 0;
}

/**
 * goodix_ts_input_dev_config - Requset and config a input device
 *  then register it to input sybsystem.
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_input_dev_config(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
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
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
				0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				0, ts_bdata->panel_max_y, 0, 0);
#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH,
				INPUT_MT_DIRECT);
#else
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH);
#endif
#endif

	input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);

	r = input_register_device(input_dev);
	if (r < 0) {
		ts_err("Unable to register input device");
		input_free_device(input_dev);
		return r;
	}

	return 0;
}

static int goodix_ts_pen_dev_config(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
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
	input_set_abs_params(pen_dev, ABS_X, 0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(pen_dev, ABS_Y, 0, ts_bdata->panel_max_y, 0, 0);
	input_set_abs_params(pen_dev, ABS_PRESSURE, 0,
				ts_bdata->panel_max_w, 0, 0);

	r = input_register_device(pen_dev);
	if (r < 0) {
		ts_err("Unable to register pen device");
		input_free_device(pen_dev);
		return r;
	}

	return 0;
}

void goodix_ts_input_dev_remove(struct goodix_ts_core *core_data)
{
	if (!core_data->input_dev)
		return;
	input_unregister_device(core_data->input_dev);
	input_free_device(core_data->input_dev);
	core_data->input_dev = NULL;
}

void goodix_ts_pen_dev_remove(struct goodix_ts_core *core_data)
{
	if (!core_data->pen_dev)
		return;
	input_unregister_device(core_data->pen_dev);
	input_free_device(core_data->pen_dev);
	core_data->pen_dev = NULL;
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
	struct goodix_ts_core *cd = container_of(ts_esd,
			struct goodix_ts_core, ts_esd);
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	int ret = 0;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (!hw_ops->esd_check)
		return;

	ret = hw_ops->esd_check(cd);
	if (ret) {
		ts_err("esd check failed");
		goodix_ts_power_off(cd);
		goodix_ts_power_on(cd);
		/* if (hw_ops->reset) */
		/* 	hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS); */
	}
	if (atomic_read(&ts_esd->esd_on))
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *cd)
{
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;

	if (!misc->esd_addr)
		return;

	if (atomic_read(&ts_esd->esd_on))
		return;

	atomic_set(&ts_esd->esd_on, 1);
	if (!schedule_delayed_work(&ts_esd->esd_work, 2 * HZ)) {
		ts_info("esd work already in workqueue");
	}
	ts_info("esd on");
}

/**
 * goodix_ts_esd_off - turn off esd protection
 */
static void goodix_ts_esd_off(struct goodix_ts_core *cd)
{
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;
	int ret;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	atomic_set(&ts_esd->esd_on, 0);
	ret = cancel_delayed_work_sync(&ts_esd->esd_work);
	ts_info("Esd off, esd work state %d", ret);
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
		goodix_core_data->hw_ops->palm_on(goodix_core_data, false);
	case NOTIFY_ESD_OFF:
		goodix_ts_esd_off(ts_esd->ts_core);
		break;
	case NOTIFY_FWUPDATE_FAILED:
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_RESUME:
		if (goodix_core_data->charger_status)
			goodix_core_data->hw_ops->charger_on(goodix_core_data, true);
		if (goodix_core_data->palm_status)
			goodix_core_data->hw_ops->palm_on(goodix_core_data, true);
	case NOTIFY_ESD_ON:
		goodix_ts_esd_on(ts_esd->ts_core);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * goodix_ts_esd_init - initialize esd protection
 */
int goodix_ts_esd_init(struct goodix_ts_core *cd)
{
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;

	if (!cd->hw_ops->esd_check || !misc->esd_addr) {
		ts_info("missing key info for esd check");
		return 0;
	}

	INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
	ts_esd->ts_core = cd;
	atomic_set(&ts_esd->esd_on, 0);
	ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
	goodix_ts_register_notifier(&ts_esd->esd_notifier);
	goodix_ts_esd_on(cd);

	return 0;
}

static void goodix_ts_release_connects(struct goodix_ts_core *core_data)
{
	struct input_dev *input_dev = core_data->input_dev;
	struct input_mt *mt = input_dev->mt;
	int i;

	memset(&core_data->ts_event, 0, sizeof(core_data->ts_event));
	goodix_ts_report_finger(core_data->input_dev,
			&core_data->ts_event.touch_data);

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
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
static int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (atomic_read(&core_data->suspended))
		return 0;

	ts_info("Suspend start");

	/*
	 * notify suspend event, inform the esd protector
	 * and charger detector to turn off the work
	 */
	goodix_ts_blocking_notify(NOTIFY_SUSPEND, NULL);

	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
			if (!ext_module->funcs->before_suspend)
				continue;

			ret = ext_module->funcs->before_suspend(core_data,
							      ext_module);
			if (ret == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/* disable irq */
	hw_ops->irq_enable(core_data, false);

	/* let touch ic work in sleep mode */
	if (hw_ops->suspend)
		hw_ops->suspend(core_data);
	atomic_set(&core_data->suspended, 1);
	core_data->work_status = TP_SLEEP;

	/* inform exteranl modules */
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
			if (!ext_module->funcs->after_suspend)
				continue;

			ret = ext_module->funcs->after_suspend(core_data,
								ext_module);
			if (ret == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);
#ifdef CONFIG_FACTORY_BUILD
	goodix_ts_power_off(core_data);
#endif

out:
	goodix_ts_release_connects(core_data);
	ts_info("Suspend end");
	return 0;
}

/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int goodix_ts_resume(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (!atomic_read(&core_data->suspended))
		return 0;

	ts_info("Resume start");
#ifdef CONFIG_FACTORY_BUILD
	goodix_ts_power_on(core_data);
#endif

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
			if (!ext_module->funcs->before_resume)
				continue;

			ret = ext_module->funcs->before_resume(core_data,
								ext_module);
			if (ret == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	atomic_set(&core_data->suspended, 0);
	/* resume device */
	if (hw_ops->resume)
		hw_ops->resume(core_data);
	core_data->work_status = TP_NORMAL;

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
			if (!ext_module->funcs->after_resume)
				continue;

			ret = ext_module->funcs->after_resume(core_data,
								ext_module);
			if (ret == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	hw_ops->irq_enable(core_data, true);

	/*
	 * notify resume event, inform the esd protector
	 * and charger detector to turn on the work
	 */
	ts_info("try notify resume");
	goodix_ts_blocking_notify(NOTIFY_RESUME, NULL);
out:
	ts_info("Resume end");
	return 0;
}

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
		/* ts_info("notifier tp event:%d, code:%d.", event, blank); */
		flush_workqueue(core_data->event_wq);
		if (event == DRM_EARLY_EVENT_BLANK && (blank == DRM_BLANK_POWERDOWN ||
			blank == DRM_BLANK_LP1 || blank == DRM_BLANK_LP2)) {
			ts_info("touchpanel suspend by %s", blank == DRM_BLANK_POWERDOWN ? "blank" : "doze");
			queue_work(core_data->event_wq, &core_data->suspend_work);
		} else if (event == DRM_EVENT_BLANK && blank == DRM_BLANK_UNBLANK) {
			ts_info("touchpanel resume");
			queue_work(core_data->event_wq, &core_data->resume_work);
		}
	}

	return 0;
}
#endif

static void goodix_charger_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, charger_work);
	const struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int status = 0;

	if (core_data->init_stage >= CORE_INIT_STAGE2) {
		status = !!power_supply_is_system_supplied();
		if (status != core_data->charger_status
			|| core_data->charger_status < 0) {
			core_data->charger_status = status;
			ts_debug("Power_supply_event:%d", status);
			if (status) {
				ts_info("charger usb in");
				hw_ops->charger_on(core_data, true);
			} else {
				ts_info("charger usb exit");
				hw_ops->charger_on(core_data, false);
			}
		}
	}
}


static int goodix_ts_charger_notifier_callback(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	struct goodix_ts_core *ts_core =
		container_of(nb, struct goodix_ts_core, charger_notifier);

	queue_work(ts_core->event_wq, &ts_core->charger_work);

	return 0;
}


#ifdef CONFIG_PM
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	ts_info("%s enter.", __func__);
	if (!core_data || core_data->init_stage != CORE_INIT_STAGE2) {
		ts_err("initialization not completed, return");
		return 0;
	}

	if (device_may_wakeup(dev) && core_data->gesture_enabled) {
		enable_irq_wake(core_data->irq);
	}
	core_data->tp_pm_suspend = true;
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

	ts_info("%s enter.", __func__);
	if (!core_data || core_data->init_stage != CORE_INIT_STAGE2) {
		ts_err("initialization not completed, return");
		return 0;
	}

	if (device_may_wakeup(dev) && core_data->gesture_enabled) {
		disable_irq_wake(core_data->irq);
	}
	core_data->tp_pm_suspend = false;
	complete(&core_data->pm_resume_completion);
	return 0;
}
#endif

/**
 * goodix_generic_noti_callback - generic notifier callback
 *  for goodix touch notification event.
 */
static int goodix_generic_noti_callback(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct goodix_ts_core *cd = container_of(self,
			struct goodix_ts_core, ts_notifier);
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->init_stage < CORE_INIT_STAGE2)
		return 0;

	ts_debug("notify event type 0x%x", (unsigned int)action);
	switch (action) {
	case NOTIFY_FWUPDATE_START:
		hw_ops->irq_enable(cd, 0);
		break;
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_FWUPDATE_FAILED:
		if (hw_ops->read_version(cd, &cd->fw_version))
			ts_info("failed read fw version info[ignore]");
		hw_ops->irq_enable(cd, 1);
		break;
	default:
		break;
	}
	return 0;
}

int goodix_ts_get_lockdowninfo(struct goodix_ts_core *cd)
{
	int ret = 0;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	ret = hw_ops->read(cd, TS_LOCKDOWN_REG,
				cd->lockdown_info, GOODIX_LOCKDOWN_SIZE);
	if (ret) {
		ts_err("can't get lockdown");
		return -EINVAL;
	}

	ts_info("lockdown is:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",
			cd->lockdown_info[0], cd->lockdown_info[1],
			cd->lockdown_info[2], cd->lockdown_info[3],
			cd->lockdown_info[4], cd->lockdown_info[5],
			cd->lockdown_info[6], cd->lockdown_info[7]);
	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
/*
 * bit 0: double tap
 * bit 1: single tap
 */
static void goodix_set_gesture_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, gesture_work);

	if(core_data->double_wakeup)
		core_data->gesture_enabled |= (1 << 0);
	else
		core_data->gesture_enabled &= ~(1 << 0);

	if(core_data->aod_status)
		core_data->gesture_enabled |= (1 << 1);
	else
		core_data->gesture_enabled &= ~(1 << 1);

	ts_info("set gesture_enabled:%d", core_data->gesture_enabled);

	return;
}

static void goodix_set_game_work(struct work_struct *work)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_core_data->hw_ops;
	u8 status = 0;
	bool on = false;
	u8 temp_value = 0;
	int ret = 0;
	int i = 0;

	if (goodix_core_data->work_status == TP_SLEEP) {
		ts_info("suspended, skip");
		return;
	}

	for (i = 0; i < Touch_Mode_NUM; i++) {
		switch (i) {
		case Touch_Game_Mode:
			temp_value =
			xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE];
			on = !!temp_value;
			break;
		case Touch_Active_MODE:
				break;
		case Touch_UP_THRESHOLD:
			temp_value =
			xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
			status &= 0xFC;
			status |= temp_value;
			break;
		case Touch_Tolerance:
			temp_value =
			xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
			temp_value = 3 - temp_value;
			status &= 0xF3;
			status |= (temp_value << 2);
			break;
		case Touch_Edge_Filter:
			temp_value =
			xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
			status &= 0xCF;
			status |= (temp_value << 4);
			break;
		case Touch_Panel_Orientation:
			temp_value =
				xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
			if (PANEL_ORIENTATION_DEGREE_90 == temp_value)
				temp_value = 1;
			else if (PANEL_ORIENTATION_DEGREE_270 == temp_value)
				temp_value = 2;
			else
				temp_value = 0;
			status &= 0x3F;
			status |= (temp_value << 6);
			break;
		default:
			/* Don't support */
			break;

		};
	}

	ret = hw_ops->game(goodix_core_data, status, !!on);

	if (ret < 0) {
		ts_err("send game mode fail");
	}

	return;
}


static int goodix_set_cur_value(int gtp_mode, int gtp_value)
{
	int ret = 0;
	ts_info("mode:%d, value:%d", gtp_mode, gtp_value);

	if (!goodix_core_data || goodix_core_data->init_stage != CORE_INIT_STAGE2) {
		ts_err("initialization not completed, return");
		return 0;
	}

	if (gtp_mode == Touch_Doubletap_Mode && goodix_core_data && gtp_value >= 0) {
		goodix_core_data->double_wakeup = gtp_value;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
		return 0;
	}
	if (gtp_mode == Touch_Aod_Enable && goodix_core_data && gtp_value >= 0) {
		goodix_core_data->aod_status = gtp_value;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
		return 0;
	}

	xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] = gtp_value;
	if (gtp_mode >= Touch_Mode_NUM) {
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

	xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE];

	queue_work(goodix_core_data->game_wq, &goodix_core_data->game_work);


	return ret;
}

static int goodix_get_mode_value(int mode, int value_type)
{
	int value = -1;

	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		ts_err("don't support");

	return value;
}

static int goodix_get_mode_all(int mode, int *value)
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

static int goodix_reset_mode(int mode)
{
	int i = 0;

	ts_info("mode:%d", mode);
	if (mode < Touch_Mode_NUM && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		queue_work(goodix_core_data->game_wq, &goodix_core_data->game_work);
	} else if (mode == 0) {
		for (i = 0; i < Touch_Mode_NUM; i++) {
			if (i == Touch_Panel_Orientation) {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE];
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
			} else {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
			}
		}
		queue_work(goodix_core_data->game_wq, &goodix_core_data->game_work);
	} else {
		ts_err("don't support");
	}

	return 0;
}

static void goodix_init_touchmode_data(void)
{
	int i;

	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
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
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
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
		ts_info("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}

	return;
}


static char goodix_panel_color_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[2];
}

static char goodix_panel_vendor_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[0];
}

static char goodix_panel_display_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[1];
}

static char goodix_touch_vendor_read(void)
{
	return '2';
}

static int goodix_palm_sensor_write(int value)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_core_data->hw_ops;
	int ret = 0;

	ts_info("palm sensor value : %d", value);
	if (!goodix_core_data) {
		ts_err("goodix core data os NULL");
		return -EINVAL;
	}

	goodix_core_data->palm_status = value;
	if (goodix_core_data->work_status == TP_NORMAL)
		ret = hw_ops->palm_on(goodix_core_data, !!value);

	return ret;
}

#endif

int goodix_ts_stage2_init(struct goodix_ts_core *cd)
{
	int ret;

	/*init lock stat*/
	mutex_init(&cd->report_mutex);

	/* alloc/config/register input device */
	ret = goodix_ts_input_dev_config(cd);
	if (ret < 0) {
		ts_err("failed set input device");
		return ret;
	}

	if (cd->board_data.pen_enable) {
		ret = goodix_ts_pen_dev_config(cd);
		if (ret < 0) {
			ts_err("failed set pen device");
			goto err_finger;
		}
	}
	/* request irq line */
	ret = goodix_ts_irq_setup(cd);
	if (ret < 0) {
		ts_info("failed set irq");
		goto exit;
	}
	ts_info("success register irq");

	cd->event_wq = alloc_workqueue("gtp-event-queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!cd->event_wq) {
		ts_err("goodix cannot create event work thread");
		ret = -ENOMEM;
		goto exit;
	}
	cd->gesture_wq = alloc_workqueue("gtp-gesture-queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!cd->gesture_wq) {
		ts_err("goodix cannot create gesture work thread");
		ret = -ENOMEM;
		goto exit;
	}

	INIT_WORK(&cd->resume_work, goodix_ts_resume_work);
	INIT_WORK(&cd->suspend_work, goodix_ts_suspend_work);
	INIT_WORK(&cd->gesture_work, goodix_set_gesture_work);
	INIT_WORK(&cd->charger_work, goodix_charger_work);
	pm_qos_add_request(&cd->tp_qos_request, PM_QOS_CPU_DMA_LATENCY,
						PM_QOS_DEFAULT_VALUE);
	cd->tp_qos_on = true;
	cd->tp_pm_suspend = false;
	init_completion(&cd->pm_resume_completion);
	device_init_wakeup(&cd->pdev->dev, 1);

#ifdef CONFIG_DRM
	cd->fb_notifier.notifier_call = goodix_ts_fb_notifier_callback;
	ret = drm_register_client(&cd->fb_notifier);
	if (ret)
		ts_err("Failed to register fb notifier client:%d", ret);
#endif

	cd->charger_notifier.notifier_call = goodix_ts_charger_notifier_callback;
	ret = power_supply_reg_notifier(&cd->charger_notifier);
	if (ret)
		ts_err("Failed to register charger notifier client:%d", ret);

	/* get ts lockdown info */
	goodix_ts_get_lockdowninfo(cd);

	/* create sysfs files */
	goodix_ts_sysfs_init(cd);
	/* create procfs files */
	goodix_ts_procfs_init();

	/* esd protector */
	goodix_ts_esd_init(cd);

	return 0;
exit:
	goodix_ts_pen_dev_remove(cd);
err_finger:
	goodix_ts_input_dev_remove(cd);
	return ret;
}

/* try send the config specified with type */
static int goodix_send_ic_config(struct goodix_ts_core *cd, int type)
{
	u32 config_id;
	struct goodix_ic_config *cfg;

	if (type >= GOODIX_MAX_CONFIG_GROUP) {
		ts_err("unsupproted config type %d", type);
		return -EINVAL;
	}

	cfg = cd->ic_configs[type];
	if (!cfg || cfg->len <= 0) {
		ts_info("no valid normal config found");
		return -EINVAL;
	}

	config_id = goodix_get_file_config_id(cfg->data);
	if (cd->ic_info.version.config_id == config_id) {
		ts_info("config id is equal 0x%x, skiped", config_id);
		return 0;
	}

	ts_info("try send config, id=0x%x", config_id);
	return cd->hw_ops->send_config(cd, cfg->data, cfg->len);
}

/**
 * goodix_later_init_thread - init IC fw and config
 * @data: point to goodix_ts_core
 *
 * This function respond for get fw version and try upgrade fw and config.
 * Note: when init encounter error, need release all resource allocated here.
 */
static int goodix_later_init_thread(void *data)
{
	int ret, i;
	struct goodix_ts_core *cd = data;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	/* setp 1: get config data from config bin */
	if (goodix_get_config_proc(cd))
		ts_info("no valid ic config found");
	else
		ts_info("success get valid ic config");

	/* setp 2: init fw struct add try do fw upgrade */
	ret = goodix_fw_update_init(cd);
	if (ret) {
		ts_err("failed init fw update module");
		goto err_out;
	}

	ret = goodix_do_fw_update(cd->ic_configs[CONFIG_TYPE_NORMAL],
			UPDATE_MODE_BLOCK | UPDATE_MODE_SRC_REQUEST);
	if (ret)
		ts_err("failed do fw update");
	/* setp3: get fw version and ic_info
	 * at this step we believe that the ic is in normal mode,
	 * if the version info is invalid there must have some
	 * problem we cann't cover so exit init directly.
	 */
	ret = hw_ops->read_version(cd, &cd->fw_version);
	if (ret) {
		ts_err("invalid fw version, abort");
		goto uninit_fw;
	}
	ret = hw_ops->get_ic_info(cd, &cd->ic_info, true);
	if (ret) {
		ts_err("invalid ic info, abort");
		goto uninit_fw;
	}

	/* the recomend way to update ic config is throuth ISP,
	 * if not we will send config with interactive mode
	 */
	goodix_send_ic_config(cd, CONFIG_TYPE_NORMAL);

	/* init other resources */
	ret = goodix_ts_stage2_init(cd);
	if (ret) {
		ts_err("stage2 init failed");
		goto uninit_fw;
	}
	cd->init_stage = CORE_INIT_STAGE2;
	return 0;

uninit_fw:
	goodix_fw_update_uninit();
err_out:
	ts_err("stage2 init failed");
	cd->init_stage = CORE_INIT_FAIL;
	for (i = 0; i < GOODIX_MAX_CONFIG_GROUP; i++) {
		if (cd->ic_configs[i])
			kfree(cd->ic_configs[i]);
		cd->ic_configs[i] = NULL;
	}
	return ret;
}

static int goodix_start_later_init(struct goodix_ts_core *ts_core)
{
	struct task_struct *init_thrd;
	/* create and run update thread */
	init_thrd = kthread_run(goodix_later_init_thread,
				ts_core, "goodix_init_thread");
	if (IS_ERR_OR_NULL(init_thrd)) {
		ts_err("Failed to create update thread:%ld",
				PTR_ERR(init_thrd));
		return -EFAULT;
	}
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

static ssize_t goodix_fw_version_info_read(struct file *file, char __user *buf,
					 size_t count, loff_t *pos)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_core_data->hw_ops;
	struct goodix_fw_version chip_ver;
	char k_buf[100] = {0};
	int ret = 0;
	int cnt = -EINVAL;

	if (*pos != 0 || !hw_ops)
		return 0;
	if (hw_ops->read_version) {
		ret = hw_ops->read_version(goodix_core_data, &chip_ver);
		if (!ret) {
			cnt = snprintf(&k_buf[0], PAGE_SIZE,
				"patch_pid:%s\n",
				chip_ver.patch_pid);
			cnt += snprintf(&k_buf[cnt], PAGE_SIZE,
				"patch_vid:%02x%02x%02x%02x\n",
				chip_ver.patch_vid[0], chip_ver.patch_vid[1],
				chip_ver.patch_vid[2], chip_ver.patch_vid[3]);
		}
	}

	if (hw_ops->get_ic_info) {
		ret = hw_ops->get_ic_info(goodix_core_data, &goodix_core_data->ic_info, false);
		if (!ret) {
			cnt += snprintf(&k_buf[cnt], PAGE_SIZE,
				"config_version:%x\n", goodix_core_data->ic_info.version.config_version);
		}
	}
	cnt = cnt > count ? count : cnt;
	ret = copy_to_user(buf, k_buf, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}
static const struct file_operations goodix_fw_version_info_ops = {
	.read = goodix_fw_version_info_read,
};

static ssize_t goodix_selftest_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	char tmp[5] = { 0 };
	int cnt;

	if (*pos != 0 || !goodix_core_data)
		return 0;
	cnt = snprintf(tmp, sizeof(goodix_core_data->result_type), "%d\n",
			goodix_core_data->result_type);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}
	*pos += cnt;
	return cnt;
}

static int goodix_short_open_test(void)
{
	struct ts_rawdata_info *info = NULL;
	int test_result;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory");
		return GTP_RESULT_INVALID;
	}

	if (goodix_get_rawdata(&goodix_core_data->pdev->dev, info)) {
		ts_err("Factory_test FAIL");
		test_result = GTP_RESULT_INVALID;
		goto exit;
	}

	if (80 == (*(info->result + 1))) {
		ts_info("test PASS!");
		test_result = GTP_RESULT_PASS;
	} else {
		ts_err("test FAILED!");
		test_result = GTP_RESULT_FAIL;
	}

exit:
	ts_info("resultInfo: %s", info->result);
	/* ret = snprintf(buf, PAGE_SIZE, "resultInfo: %s", info->result); */

	kfree(info);
	return test_result;
}

static ssize_t goodix_selftest_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	struct goodix_fw_version chip_ver;
	struct goodix_ts_hw_ops *hw_ops;
	int retval = 0;
	char tmp[6];

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}
	if (!goodix_core_data)
		return GTP_RESULT_INVALID;
	else
		hw_ops = goodix_core_data->hw_ops;

	if (!strncmp("short", tmp, 5) || !strncmp("open", tmp, 4)) {
		retval = goodix_short_open_test();
	} else if (!strncmp("i2c", tmp, 3)) {
		hw_ops->read_version(goodix_core_data, &chip_ver);
		if (chip_ver.sensor_id == 255)
			retval = GTP_RESULT_PASS;
		else
			retval = GTP_RESULT_FAIL;
	}

	goodix_core_data->result_type = retval;
out:
	if (retval >= 0)
		retval = count;

	return retval;
}
static const struct file_operations goodix_selftest_ops = {
	.read = goodix_selftest_read,
	.write = goodix_selftest_write,
};
#ifdef CONFIG_DEBUG_FS
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
				\necho \"irq-disable\" or \"irq-enable\" to ctrl irq\n \
				\necho \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n \
				\necho \"tp-sd-en\" or \"tp-sd-off\" to ctrl panel in or off sleep status\n";

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
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
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
		hw_ops->irq_enable(core_data, false);
	else if (!strncmp(cmd, "irq-enable", 10))
		hw_ops->irq_enable(core_data, true);
	else if (!strncmp(cmd, "tp-sd-en", 8))
		tpdbg_suspend(core_data, true);
	else if (!strncmp(cmd, "tp-sd-off", 9))
		tpdbg_suspend(core_data, false);
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
static int get_hardware_level(u8 *hwlevel, u8 *hwc)
{
	char *ptr_hwlevel = (char *)strnstr(saved_command_line, "androidboot.hwlevel=", strlen(saved_command_line));
	char *ptr_hwc = (char *)strnstr(saved_command_line, "androidboot.hwc=", strlen(saved_command_line));

	if (ptr_hwlevel && ptr_hwc) {
		memcpy(hwlevel, (ptr_hwlevel + strlen("androidboot.hwlevel=")), 4);
		memcpy(hwc, (ptr_hwc + strlen("androidboot.hwc=")), 7);
		ts_info("hwlevel is %s", hwlevel);
		ts_info("hwc is %s", hwc);
		return 0;
	}
	else
		return -EINVAL;
}


static int check_update_confirm(void)
{
	char hwlevel[5] = {'\0'};
	char hwc[8] = {'\0'};
	int ret = -1;
	ret =  get_hardware_level(hwlevel, hwc);
	if (ret) {
		ts_err("get hwlevel failed!");
		return -EINVAL;
	}
	if (!strncmp(hwlevel, "P0.1", strlen("P0.1")) ||
			!strncmp(hwlevel, "P1 ", strlen("P1 ")))
		goodix_core_data->update_confirm = false;
	else if (!strncmp(hwlevel, "P1.1", strlen("P1.1")) &&
			!strncmp(hwc, "INDIA", strlen("INDIA")))
		goodix_core_data->update_confirm = false;
	else
		goodix_core_data->update_confirm = true;
	ts_info("update_confirm is %d", goodix_core_data->update_confirm);

    return 0;
}


/**
 * goodix_ts_probe - called by kernel when Goodix touch
 *  platform driver is added.
 */
static int goodix_ts_probe(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = NULL;
	struct goodix_bus_interface *bus_interface;
	int ret;

	ts_info("goodix_ts_probe IN");

	bus_interface = pdev->dev.platform_data;
	if (!bus_interface) {
		ts_err("Invalid touch device");
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -ENODEV;
	}

	core_data = devm_kzalloc(&pdev->dev,
			sizeof(struct goodix_ts_core), GFP_KERNEL);
	if (!core_data) {
		ts_err("Failed to allocate memory for core data");
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -ENOMEM;
	}
	goodix_core_data = core_data;

	if (IS_ENABLED(CONFIG_OF) && bus_interface->dev->of_node) {
		/* parse devicetree property */
		ret = goodix_parse_dt(bus_interface->dev->of_node,
					&core_data->board_data);
		if (ret) {
			ts_err("failed parse device info form dts, %d", ret);
			return -EINVAL;
		}
	} else {
		ts_err("no valid device tree node found");
		return -ENODEV;
	}

	core_data->hw_ops = goodix_get_hw_ops();
	if (!core_data->hw_ops) {
		ts_err("hw ops is NULL");
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -EINVAL;
	}
	goodix_core_module_init();
	/* touch core layer is a platform driver */
	core_data->pdev = pdev;
	core_data->bus = bus_interface;
	platform_set_drvdata(pdev, core_data);

	/* get GPIO resource */
	ret = goodix_ts_gpio_setup(core_data);
	if (ret) {
		ts_err("failed init gpio");
		goto err_out;
	}

	ret = goodix_ts_power_init(core_data);
	if (ret) {
		ts_err("failed init power");
		goto err_out;
	}

	ret = goodix_ts_power_on(core_data);
	if (ret) {
		ts_err("failed power on");
		goto err_out;
	}

	/* core_data->hw_ops->reset(core_data, GOODIX_NORMAL_RESET_DELAY_MS); */
	/* confirm it's goodix touch dev or not */
	ret = core_data->hw_ops->dev_confirm(core_data);
	if (ret) {
		ts_err("goodix device confirm failed");
		goto err_out;
	}

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&core_data->ts_notifier);

	ret = check_update_confirm();
	if (ret) {
		ts_err("failed get update confirm, default on");
		core_data->update_confirm = true;
	}

	/* Try start a thread to get config-bin info */
	ret = goodix_start_later_init(core_data);
	if (ret) {
		ts_err("Failed start cfg_bin_proc, %d", ret);
		goto err_out;
	}

	core_data->tp_lockdown_info_proc =
		proc_create("tp_lockdown_info", 0664, NULL, &goodix_lockdown_info_ops);
	core_data->tp_fw_version_proc =
		proc_create("tp_fw_version", 0664, NULL, &goodix_fw_version_info_ops);
	core_data->tp_selftest_proc =
		proc_create("tp_selftest", 0664, NULL, &goodix_selftest_ops);
#ifdef CONFIG_DEBUG_FS
	core_data->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (core_data->debugfs) {
		debugfs_create_file("switch_state", 0660, core_data->debugfs, core_data,
					&tpdbg_operations);
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	core_data->game_wq = alloc_workqueue("gtp-game-queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!core_data->game_wq) {
		ts_err("goodix cannot create game work thread");
	}
	INIT_WORK(&core_data->game_work, goodix_set_game_work);

	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	xiaomi_touch_interfaces.setModeValue = goodix_set_cur_value;
	xiaomi_touch_interfaces.getModeValue = goodix_get_mode_value;
	xiaomi_touch_interfaces.resetMode = goodix_reset_mode;
	xiaomi_touch_interfaces.getModeAll = goodix_get_mode_all;
	xiaomi_touch_interfaces.panel_display_read = goodix_panel_display_read;
	xiaomi_touch_interfaces.panel_vendor_read = goodix_panel_vendor_read;
	xiaomi_touch_interfaces.panel_color_read = goodix_panel_color_read;
	xiaomi_touch_interfaces.touch_vendor_read = goodix_touch_vendor_read;
	xiaomi_touch_interfaces.palm_sensor_write = goodix_palm_sensor_write;

	xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
	goodix_init_touchmode_data();
#endif

	core_data->init_stage = CORE_INIT_STAGE1;
	goodix_modules.core_data = core_data;
	core_module_prob_sate = CORE_MODULE_PROB_SUCCESS;

	complete_all(&goodix_modules.core_comp);
	ts_info("goodix_ts_core probe success");
	return 0;

err_out:
	core_data->init_stage = CORE_INIT_FAIL;
	core_module_prob_sate = CORE_MODULE_PROB_FAILED;
	ts_err("goodix_ts_core failed, ret:%d", ret);
	/* wakeup ext module register work */
	complete_all(&goodix_modules.core_comp);
	return ret;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;

	if (goodix_unregister_all_module())
		return -EBUSY;

	goodix_ts_unregister_notifier(&core_data->ts_notifier);

	if (core_data->init_stage >= CORE_INIT_STAGE2) {
		hw_ops->irq_enable(core_data, false);
	#ifdef CONFIG_DRM
		fb_unregister_client(&core_data->fb_notifier);
	#endif
		core_module_prob_sate = CORE_MODULE_REMOVED;
		if (atomic_read(&core_data->ts_esd.esd_on))
			goodix_ts_esd_off(core_data);
		goodix_ts_unregister_notifier(&ts_esd->esd_notifier);

		if (core_data->tp_qos_on) {
			pm_qos_remove_request(&core_data->tp_qos_request);
			core_data->tp_qos_on = false;
		}

		goodix_fw_update_uninit();
		goodix_ts_input_dev_remove(core_data);
		goodix_ts_pen_dev_remove(core_data);
		goodix_ts_sysfs_exit(core_data);
		goodix_ts_proc_exit(core_data);
		goodix_ts_power_off(core_data);
		goodix_ts_wq_exit(core_data);
#ifdef CONFIG_DEBUG_FS
		goodix_ts_debugfs_exit(core_data);
#endif
	}

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

static int get_dsi_display0(u8 *display0)
{
	char *ptr_display0 = (char *)strnstr(saved_command_line, "msm_drm.dsi_display0=mdss_dsi_k9a_", strlen(saved_command_line));

	if (ptr_display0) {
		memcpy(display0, (ptr_display0 + strlen("msm_drm.dsi_display0=mdss_dsi_k9a_")), 2);
		ts_info("display0 is %s", display0);
		return 0;
	} else
		return -EINVAL;
}

static int check_touch_ic(void)
{
	char display0[5] = {'\0'};
	int ret = -1;
	ret =  get_dsi_display0(display0);
	if (ret) {
		ts_err("get dsi_display0 failed!");
		return -EINVAL;
	}

	if (!strncmp(display0, "36", strlen("36"))) {
		ts_info("goodix ic");
		return 36;
	} else if (!strncmp(display0, "42", strlen("42"))) {
		ts_info("focal ic");
		return 42;
	} else {
		ts_info("unknow ic");
		return 0;
	}
}

static int __init goodix_ts_core_init(void)
{
	int ret = 0;
	int touch_ic = -1;

	ts_info("Core layer init:%s", GOODIX_DRIVER_VERSION);
	touch_ic = check_touch_ic();
	if (touch_ic != 36 && touch_ic != 0)
		return 0;

	ret = goodix_bus_init();
	if (ret) {
		ts_err("failed add bus driver");
		return ret;
	}
	return platform_driver_register(&goodix_ts_driver);
}

static void __exit goodix_ts_core_exit(void)
{
	int touch_ic = -1;

	touch_ic = check_touch_ic();
	if (touch_ic != 36 && touch_ic != 0)
		return;
	ts_info("Core layer exit");
	platform_driver_unregister(&goodix_ts_driver);
	goodix_bus_exit();
	return;
}
module_init(goodix_ts_core_init);
module_exit(goodix_ts_core_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
