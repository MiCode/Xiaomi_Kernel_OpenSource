 /*
  * Goodix Touchscreen Driver
  * Copyright (C) 2020 - 2021 Goodix, Inc.
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
/*N17 code for HQ-291100 by gaoxue at 2023/5/4 start*/
#include <linux/hqsysfs.h>
/*N17 code for HQ-291100 by gaoxue at 2023/5/4 end*/
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
#include <linux/power_supply.h>
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#include "goodix_ts_core.h"

/* N17 code for HQ-305336 by jiangyue at 2023/7/5 start */
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
#include "../../../gpu/drm/mediatek/mediatek_v2/mi_disp/mi_disp_notifier.h"
#endif
/* N17 code for HQ-305336 by jiangyue at 2023/7/5 end */

/*N17 code for HQ-291656 by gaoxue at 2023/5/9 start*/
#include "../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
/*N17 code for HQ-291656 by gaoxue at 2023/5/9 end*/

/* goodix fb test */
// #include "../../../video/fbdev/core/fb_firefly.h"

#define GOODIX_DEFAULT_CFG_NAME		"goodix_cfg_group.cfg"

/* N17 code for HQ-308381 by zhangzhijian5 at 2023/7/20 start */
#define INVALID_VALUE 0
/* N17 code for HQ-308381 by zhangzhijian5 at 2023/7/20 end */

struct goodix_module goodix_modules;
int core_module_prob_sate = CORE_MODULE_UNPROBED;

/*N17 code for HQ-292221 by gaoxue at 2023/4/19 start*/
struct goodix_ts_core *goodix_core_data;
/*N17 code for HQ-292221 by gaoxue at 2023/4/19 end*/

static int goodix_send_ic_config(struct goodix_ts_core *cd, int type);
/*N17 code for HQ-291100 by gaoxue at 2023/5/4 start*/
static char gt_hw_info[128] = "";
/*N17 code for HQ-291100 by gaoxue at 2023/5/4 end*/

/*N17 code for HQ-296326 by gaoxue at 2023/5/18 start*/
int goodix_ts_hw_info(struct goodix_ts_core *core_data);
/*N17 code for HQ-296326 by gaoxue at 2023/5/18 end*/

/* N17 code for HQ-291091 by jiangyue at 2023/6/2 start */
extern int gsx_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value);
/* N17 code for HQ-291091 by jiangyue at 2023/6/2 end */

/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
#include "../xiaomi/xiaomi_touch.h"
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
static void goodix_set_gesture_work(struct work_struct *work);
static int goodix_get_charging_status(void);
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */

/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 * return 0 on success, otherwise return < 0
 */
static int __do_register_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module, *next;
	struct list_head *insert_point = &goodix_modules.head;

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

	return 0;
}

static void goodix_register_ext_module_work(struct work_struct *work)
{
	struct goodix_ext_module *module =
			container_of(work, struct goodix_ext_module, work);

	ts_info("module register work IN");

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

	ts_info("IN");

	goodix_core_module_init();
	INIT_WORK(&module->work, goodix_register_ext_module_work);
	schedule_work(&module->work);

	ts_info("OUT");
	return 0;
}

/**
 * goodix_register_ext_module_no_wait
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module_no_wait(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;

	ts_info("IN");
	goodix_core_module_init();
	/* driver probe failed */
	if (core_module_prob_sate != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return -EINVAL;
	}
	return __do_register_ext_module(module);
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

struct kobject *goodix_get_default_kobj(void)
{
	struct kobject *kobj = NULL;

	if (goodix_modules.core_data &&
			goodix_modules.core_data->pdev)
		kobj = &goodix_modules.core_data->pdev->dev.kobj;
	return kobj;
}

/* show driver infomation */
static ssize_t driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DriverVersion:%s\n",
			GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
static ssize_t chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *cd = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_fw_version chip_ver;
	struct goodix_ic_info ic_info;
	u8 temp_pid[8] = {0};
	int ret;
	int cnt = -EINVAL;

	if (hw_ops->read_version) {
		ret = hw_ops->read_version(cd, &chip_ver);
		if (!ret) {
			memcpy(temp_pid, chip_ver.rom_pid,
					sizeof(chip_ver.rom_pid));
			cnt = snprintf(&buf[0], PAGE_SIZE,
				"rom_pid:%s\nrom_vid:%02x%02x%02x\n",
				temp_pid, chip_ver.rom_vid[0],
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
		ret = hw_ops->get_ic_info(cd, &ic_info);
		if (!ret) {
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
					"config_id:%x\n",
					ic_info.version.config_id);
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
					"config_version:%x\n",
					ic_info.version.config_version);
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
static ssize_t read_cfg_show(struct device *dev,
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
		for (i = 0; i < 200; i++) { // only print 200 bytes
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
			ts_err("exchange cfg data error, overflow, temp_index:%d,m_size:%d",
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
	int ret;

	if (buf[0] != '1')
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
		return snprintf(buf, PAGE_SIZE,
			"failed read addr(%x), len(%d)\n",
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
		     "echo 0/1 > irq_info to disable/enable irq\n");
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

/* debug level show */
static ssize_t goodix_ts_debug_log_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
		    debug_log_flag ?
		    "enabled" : "disabled");

	return r;
}

/* debug level store */
static ssize_t goodix_ts_debug_log_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		debug_log_flag = true;
	else
		debug_log_flag = false;
	return count;
}

/* show die package site and mcu fabs */
#define DIE_INFO_START_FLASH_ADDR 0x1F300
static ssize_t die_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *cd = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	u8 temp_buf[21];
	u8 pkg_site;
	u8 mcu_fab;
	int ret;

	ret = hw_ops->read_flash(cd, DIE_INFO_START_FLASH_ADDR, temp_buf, sizeof(temp_buf));
	if (ret < 0) {
		ts_err("read flash failed");
		return 0;
	}

	ts_info("die info:%*ph", (int)sizeof(temp_buf), temp_buf);

	pkg_site = temp_buf[1];
	mcu_fab = temp_buf[20];
	ret = snprintf(buf, PAGE_SIZE, "package_id:0x%02X mcu_fab:0x%02X\n", pkg_site, mcu_fab);

	return ret;
}

/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
/* charger show */
static ssize_t goodix_ts_charger_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r = 0;
	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
			goodix_core_data->charger_status ?
			"enabled" : "disabled");
	return r;
}
/* charger store */
static ssize_t goodix_ts_charger_info_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0') {
		goodix_core_data->charger_status = 1;
		ts_info("charger usb in");
		goodix_core_data->hw_ops->charger_on(goodix_core_data, true);
	} else {
		goodix_core_data->charger_status = 0;
		ts_info("charger usb exit");
		goodix_core_data->hw_ops->charger_on(goodix_core_data, false);
	}
	return count;
}

/* aod gesture show */
static ssize_t goodix_ts_aod_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r = 0;
	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
			goodix_core_data->aod_status ?
			"enabled" : "disabled");
	return r;
}
/* aod gesture_store */
static ssize_t goodix_ts_aod_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0') {
		goodix_core_data->aod_status = 1;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
	} else {
		goodix_core_data->aod_status = 0;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
	}
	return count;
}
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */

static DEVICE_ATTR(driver_info, 0440,
		driver_info_show, NULL);
static DEVICE_ATTR(chip_info, 0440,
		chip_info_show, NULL);
static DEVICE_ATTR(reset, 0220,
		NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, 0220,
		NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, 0440,
		read_cfg_show, NULL);
static DEVICE_ATTR(reg_rw, 0664,
		goodix_ts_reg_rw_show, goodix_ts_reg_rw_store);
static DEVICE_ATTR(irq_info, 0664,
		goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(esd_info, 0664,
		goodix_ts_esd_info_show, goodix_ts_esd_info_store);
static DEVICE_ATTR(debug_log, 0664,
		goodix_ts_debug_log_show, goodix_ts_debug_log_store);
static DEVICE_ATTR(die_info, 0440,
		die_info_show, NULL);
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
static DEVICE_ATTR(charger_info, 0664,
		goodix_ts_charger_info_show, goodix_ts_charger_info_store);
static DEVICE_ATTR(aod, 0664,
		goodix_ts_aod_show, goodix_ts_aod_store);
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */

static struct attribute *sysfs_attrs[] = {
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_reg_rw.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_esd_info.attr,
	&dev_attr_debug_log.attr,
	&dev_attr_die_info.attr,
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
	&dev_attr_charger_info.attr,
	&dev_attr_aod.attr,
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */
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

/* prosfs create */
static int rawdata_proc_show(struct seq_file *m, void *v)
{
	struct ts_rawdata_info *info;
	struct goodix_ts_core *cd;
	int tx;
	int rx;
	int ret;
	int i;
	int index;

	if (!m || !v)
		return -EIO;

	cd = m->private;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	ret = cd->hw_ops->get_capacitance_data(cd, info);
	if (ret < 0) {
		ts_err("failed to get_capacitance_data, exit!");
		goto exit;
	}

	rx = info->buff[0];
	tx = info->buff[1];
	seq_printf(m, "TX:%d  RX:%d\n", tx, rx);
	seq_puts(m, "mutual_rawdata:\n");
	index = 2;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%5d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}
	seq_puts(m, "mutual_diffdata:\n");
	index += tx * rx;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%3d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}

exit:
	kfree(info);
	return ret;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, rawdata_proc_show,
			PDE_DATA(inode), PAGE_SIZE * 10);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops rawdata_proc_fops = {
	.proc_open = rawdata_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations rawdata_proc_fops = {
	.open = rawdata_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

/*N17 code for HQ-292104 by gaoxue at 2023/5/8 start*/
static void goodix_ts_procfs_init(struct goodix_ts_core *core_data)
{
	struct proc_dir_entry *proc_entry;

	if (!proc_mkdir("goodix_ts", NULL))
		return;
	proc_entry = proc_create_data("tp_data_dump",
			0664, NULL, &rawdata_proc_fops, core_data);
	if (!proc_entry)
		ts_err("failed to create proc entry");
}

static void goodix_ts_procfs_exit(struct goodix_ts_core *core_data)
{
	remove_proc_entry("tp_data_dump", NULL);
	remove_proc_entry("goodix_ts", NULL);
}
/*N17 code for HQ-292104 by gaoxue at 2023/5/8 end*/

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

/**
 * goodix_ts_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_notifier_list, nb);
}

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v)
{
	int ret;

	ret = blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
	return ret;
}

#if IS_ENABLED(CONFIG_OF)
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

	memset(board_data->avdd_name, 0, sizeof(board_data->avdd_name));
	r = of_property_read_string(node, "goodix,avdd-name", &name_tmp);
	if (!r) {
		ts_info("avdd name from dt: %s", name_tmp);
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
		ts_info("iovdd name from dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->iovdd_name))
			strncpy(board_data->iovdd_name,
				name_tmp, sizeof(board_data->iovdd_name));
		else
			ts_info("invalied iovdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->iovdd_name));
	}

	/* get firmware file name */
	r = of_property_read_string(node, "goodix,firmware-name", &name_tmp);
	if (!r) {
		ts_info("firmware name from dt: %s", name_tmp);
		strncpy(board_data->fw_name,
				name_tmp, sizeof(board_data->fw_name));
	} else {
		ts_info("can't find firmware name, use default: %s",
				TS_DEFAULT_FIRMWARE);
		strncpy(board_data->fw_name,
				TS_DEFAULT_FIRMWARE,
				sizeof(board_data->fw_name));
	}

	/* get config file name */
	r = of_property_read_string(node, "goodix,config-name", &name_tmp);
	if (!r) {
		ts_info("config name from dt: %s", name_tmp);
		strncpy(board_data->cfg_bin_name, name_tmp,
				sizeof(board_data->cfg_bin_name));
	} else {
		ts_info("can't find config name, use default: %s",
				TS_DEFAULT_CFG_BIN);
		strncpy(board_data->cfg_bin_name,
				TS_DEFAULT_CFG_BIN,
				sizeof(board_data->cfg_bin_name));
	}

	/* get xyz resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	if (r) {
		ts_err("Failed to parse resolutions:%d", r);
		return r;
	}

/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
	/* get expert mode parameter */
	r = of_property_count_u32_elems(node, "goodix,touch-expert-array");
	if (r == GAME_ARRAY_LEN * GAME_ARRAY_SIZE) {
		of_property_read_u32_array(node,
						"goodix,touch-expert-array",
						board_data->touch_expert_array,
						r);
	} else {
		ts_err("Failed to parse touch-expert-array:%d", r);
	}
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */

	/* get sleep mode flag */
	board_data->sleep_enable = of_property_read_bool(node,
			"goodix,sleep-enable");

	/*get pen-enable switch and pen keys, must after "key map"*/
	board_data->pen_enable = of_property_read_bool(node,
			"goodix,pen-enable");

	ts_info("[DT]x:%d, y:%d, w:%d, p:%d sleep_enable:%d pen_enable:%d",
		board_data->panel_max_x, board_data->panel_max_y,
		board_data->panel_max_w, board_data->panel_max_p,
		board_data->sleep_enable, board_data->pen_enable);
	return 0;
}
#endif

static void goodix_ts_report_pen(struct input_dev *dev,
		struct goodix_pen_data *pen_data)
{
	int i;

	mutex_lock(&dev->mutex);

	if (pen_data->coords.status == TS_TOUCH) {
		input_report_key(dev, BTN_TOUCH, 1);
		input_report_key(dev, pen_data->coords.tool_type, 1);
		input_report_abs(dev, ABS_X, pen_data->coords.x);
		input_report_abs(dev, ABS_Y, pen_data->coords.y);
		input_report_abs(dev, ABS_PRESSURE, pen_data->coords.p);
		if (pen_data->coords.p == 0)
			input_report_abs(dev, ABS_DISTANCE, 1);
		else
			input_report_abs(dev, ABS_DISTANCE, 0);
		input_report_abs(dev, ABS_TILT_X, pen_data->coords.tilt_x);
		input_report_abs(dev, ABS_TILT_Y, pen_data->coords.tilt_y);
		ts_debug("pen_data:x %d, y %d, p %d, tilt_x %d tilt_y %d key[%d %d]",
				pen_data->coords.x, pen_data->coords.y,
				pen_data->coords.p, pen_data->coords.tilt_x,
				pen_data->coords.tilt_y,
				pen_data->keys[0].status == TS_TOUCH ? 1 : 0,
				pen_data->keys[1].status == TS_TOUCH ? 1 : 0);
	} else {
		input_report_key(dev, BTN_TOUCH, 0);
		input_report_key(dev, pen_data->coords.tool_type, 0);
	}
	/* report pen button */
	for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
		if (pen_data->keys[i].status == TS_TOUCH)
			input_report_key(dev, pen_data->keys[i].code, 1);
		else
			input_report_key(dev, pen_data->keys[i].code, 0);
	}

	input_sync(dev);
	mutex_unlock(&dev->mutex);
}

static void goodix_ts_report_finger(struct input_dev *dev,
		struct goodix_touch_data *touch_data)
{
	unsigned int touch_num = touch_data->touch_num;
	int i;

	mutex_lock(&dev->mutex);

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (touch_data->coords[i].status == TS_TOUCH) {
			ts_debug("report: id[%d], x %d, y %d, w %d", i,
				touch_data->coords[i].x,
				touch_data->coords[i].y,
				touch_data->coords[i].w);
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
			input_report_abs(dev, ABS_MT_POSITION_X,
					touch_data->coords[i].x);
			input_report_abs(dev, ABS_MT_POSITION_Y,
					touch_data->coords[i].y);
/* N17 code for HQ-308381 by zhangzhijian5 at 2023/7/20 start */
			input_report_abs(dev, ABS_MT_TOUCH_MAJOR,
					INVALID_VALUE);
/* N17 code for HQ-308381 by zhangzhijian5 at 2023/7/20 end */
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
			last_touch_events_collect(i, 1);
		} else {
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
			last_touch_events_collect(i, 0);
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */
		}
	}

	input_report_key(dev, BTN_TOUCH, touch_num > 0 ? 1 : 0);
	input_sync(dev);

	mutex_unlock(&dev->mutex);
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
	int ret;

	disable_irq_nosync(core_data->irq);

	ts_esd->irq_status = true;
	core_data->irq_trig_cnt++;
	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	list_for_each_entry_safe(ext_module, next,
				 &goodix_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		ret = ext_module->funcs->irq_event(core_data, ext_module);
		if (ret == EVT_CANCEL_IRQEVT) {
			mutex_unlock(&goodix_modules.mutex);
			enable_irq(core_data->irq);
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
		if (core_data->board_data.pen_enable &&
				ts_event->event_type == EVENT_PEN) {
			goodix_ts_report_pen(core_data->pen_dev,
					&ts_event->pen_data);
		}
		if (ts_event->event_type == EVENT_REQUEST)
			goodix_ts_request_handle(core_data, ts_event);
	}

	enable_irq(core_data->irq);
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

	ts_info("Device power on");
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
	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->reset_gpio,
			GPIOF_OUT_INIT_LOW, "ts_reset_gpio");
	if (r < 0) {
		ts_err("Failed to request reset gpio, r:%d", r);
		return r;
	}

	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->irq_gpio,
			GPIOF_IN, "ts_irq_gpio");
	if (r < 0) {
		ts_err("Failed to request irq gpio, r:%d", r);
		return r;
	}

	if (ts_bdata->avdd_gpio > 0) {
		r = devm_gpio_request_one(&core_data->pdev->dev,
				ts_bdata->avdd_gpio,
				GPIOF_OUT_INIT_LOW, "ts_avdd_gpio");
		if (r < 0) {
			ts_err("Failed to request avdd-gpio, r:%d", r);
			return r;
		}
	}

	if (ts_bdata->iovdd_gpio > 0) {
		r = devm_gpio_request_one(&core_data->pdev->dev,
				ts_bdata->iovdd_gpio,
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
	static char ts_phys[32];
	int r;

	input_dev = input_allocate_device();
	if (!input_dev) {
		ts_err("Failed to allocated input device");
		return -ENOMEM;
	}

	core_data->input_dev = input_dev;
/* N17 code for HQ-291091 by jiangyue at 2023/6/2 start */
	core_data->input_dev->event = gsx_gesture_switch;
/* N17 code for HQ-291091 by jiangyue at 2023/6/2 end */
	input_set_drvdata(input_dev, core_data);

	input_dev->name = GOODIX_CORE_DRIVER_NAME;
	sprintf(ts_phys, "%s/input0", input_dev->name);
	input_dev->phys = ts_phys;
	input_dev->id.bustype = core_data->bus->bus_type;
	input_dev->id.vendor = 0x27C6;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	/* set input parameters */
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, ts_bdata->panel_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, ts_bdata->panel_max_w, 0, 0);
#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH,
			    INPUT_MT_DIRECT);
#else
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH);
#endif
#endif

	input_set_capability(input_dev, EV_KEY, KEY_POWER);
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
	static char ts_phys[32];
	int r;

	pen_dev = input_allocate_device();
	if (!pen_dev) {
		ts_err("Failed to allocated pen device");
		return -ENOMEM;
	}

	core_data->pen_dev = pen_dev;
	input_set_drvdata(pen_dev, core_data);

	pen_dev->name = GOODIX_PEN_DRIVER_NAME;
	sprintf(ts_phys, "%s/input0", pen_dev->name);
	pen_dev->phys = ts_phys;
	pen_dev->id.bustype = core_data->bus->bus_type;
	pen_dev->id.vendor = 0x27C6;
	pen_dev->id.product = 0x0002;
	pen_dev->id.version = 0x0100;

	pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(ABS_X, pen_dev->absbit);
	set_bit(ABS_Y, pen_dev->absbit);
	set_bit(ABS_TILT_X, pen_dev->absbit);
	set_bit(ABS_TILT_Y, pen_dev->absbit);
	set_bit(BTN_STYLUS, pen_dev->keybit);
	set_bit(BTN_STYLUS2, pen_dev->keybit);
	set_bit(BTN_TOUCH, pen_dev->keybit);
	set_bit(BTN_TOOL_PEN, pen_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
	input_set_abs_params(pen_dev, ABS_X, 0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(pen_dev, ABS_Y, 0, ts_bdata->panel_max_y, 0, 0);
	input_set_abs_params(pen_dev, ABS_PRESSURE, 0,
			     ts_bdata->panel_max_p, 0, 0);
	input_set_abs_params(pen_dev, ABS_DISTANCE, 0, 255, 0, 0);
	input_set_abs_params(pen_dev, ABS_TILT_X,
			-GOODIX_PEN_MAX_TILT, GOODIX_PEN_MAX_TILT, 0, 0);
	input_set_abs_params(pen_dev, ABS_TILT_Y,
			-GOODIX_PEN_MAX_TILT, GOODIX_PEN_MAX_TILT, 0, 0);

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

	if (ts_esd->irq_status)
		goto exit;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (!hw_ops->esd_check)
		return;

	ret = hw_ops->esd_check(cd);
	if (ret) {
		ts_err("esd check failed");
		goodix_ts_power_off(cd);
		usleep_range(5000, 5100);
		goodix_ts_power_on(cd);
	}

exit:
	ts_esd->irq_status = false;
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
	if (!schedule_delayed_work(&ts_esd->esd_work, 2 * HZ))
		ts_info("esd work already in workqueue");

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
	case NOTIFY_ESD_OFF:
		goodix_ts_esd_off(ts_esd->ts_core);
		break;
	case NOTIFY_FWUPDATE_FAILED:
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_RESUME:
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
	int i;

	mutex_lock(&input_dev->mutex);

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		input_mt_slot(input_dev, i);
		input_mt_report_slot_state(input_dev,
				MT_TOOL_FINGER,
				false);
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
		last_touch_events_collect(i, 0);
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */
	}
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_mt_sync_frame(input_dev);
	input_sync(input_dev);

	mutex_unlock(&input_dev->mutex);
	if (core_data->gesture_type)
		core_data->hw_ops->after_event_handler(core_data);
}

/**
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to sleep
 */
static int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (core_data->init_stage < CORE_INIT_STAGE2 ||
			atomic_read(&core_data->suspended))
		return 0;

	ts_info("Suspend start");
	atomic_set(&core_data->suspended, 1);
	/* disable irq */
	hw_ops->irq_enable(core_data, false);

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

/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
	core_data->work_status = TP_SLEEP;
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */
	/* enter sleep mode or power off */
	if (core_data->board_data.sleep_enable)
		hw_ops->suspend(core_data);
	else
		goodix_ts_power_off(core_data);

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

	if (core_data->init_stage < CORE_INIT_STAGE2 ||
			!atomic_read(&core_data->suspended))
		return 0;

	ts_info("Resume start");
	atomic_set(&core_data->suspended, 0);
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
	hw_ops->irq_enable(core_data, true);
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */
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

	/* reset device or power on*/
	if (core_data->board_data.sleep_enable)
		hw_ops->resume(core_data);
	else
		goodix_ts_power_on(core_data);
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
	core_data->work_status = TP_NORMAL;
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */
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

out:
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
	/* enable charger mode */
	core_data->work_status = TP_NORMAL;
	if (core_data->charger_status)
		hw_ops->charger_on(core_data, true);
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
	/* enable palm sensor */
	if (core_data->palm_status)
		ret = hw_ops->palm_on(core_data, core_data->palm_status);
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */
	/* enable irq */
	hw_ops->irq_enable(core_data, true);
	/* open esd */
	goodix_ts_blocking_notify(NOTIFY_RESUME, NULL);
	ts_info("Resume end");
	return 0;
}

/*N17 code for HQ-291656 by gaoxue at 2023/5/9 start*/
/**
 * goodix_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
/* N17 code for HQ-305336 by jiangyue at 2023/7/5 start */
static int goodix_ts_fb_notifier_callback(struct notifier_block *nb,
                unsigned long val, void *data)
{
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	struct goodix_ts_core *core_data =
		container_of(nb, struct goodix_ts_core, fb_notifier);
	//struct fb_event *evdata = data;
	struct mi_disp_notifier *evdata = data;
	int blank;

    if (!(val == MI_DISP_DPMS_EARLY_EVENT ||
          val == MI_DISP_DPMS_EVENT)) {
        ts_err("event(%lu) do not need process", val);
        return 0;
    }

	if (evdata && evdata->data && core_data) {
		blank = *(int *)(evdata->data);
		ts_info("notifier tp event:%d, code:%d.", val, blank);
		if (val == MI_DISP_DPMS_EVENT && (blank == MI_DISP_DPMS_POWERDOWN || blank == MI_DISP_DPMS_LP1 || blank == MI_DISP_DPMS_LP2)) {
			ts_info("event:%lu,blank:%d", val, blank);
/*N17 code for HQ-301563 by jiangyue at 2023/7/12 start*/
			flush_workqueue(core_data->event_wq);
			queue_work(core_data->event_wq, &core_data->suspend_work);
		} else if (val == MI_DISP_DPMS_EVENT && blank == MI_DISP_DPMS_ON) {
			ts_info("touchpanel resume, event:%lu,blank:%d", val, blank);
			flush_workqueue(core_data->event_wq);
			queue_work(core_data->event_wq, &core_data->resume_work);
/*N17 code for HQ-301563 by jiangyue at 2023/7/12 end*/
		}
	}
#endif
	return 0;
}
/* N17 code for HQ-305336 by jiangyue at 2023/7/5 end */

/*N17 code for HQ-301563 by jiangyue at 2023/7/12 start*/
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
/*N17 code for HQ-301563 by jiangyue at 2023/7/12 end*/

#if IS_ENABLED(CONFIG_PM)
#if 0
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
/*N17 code for HQ-291656 by gaoxue at 2023/5/9 end*/

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

	ts_info("notify event type 0x%x", (unsigned int)action);
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

/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
static int goodix_get_charging_status(void)
{
	struct power_supply *usb_psy;
	struct power_supply *dc_psy;
	union power_supply_propval val;
	int rc = 0;
	int is_charging = 0;
	is_charging = !!power_supply_is_system_supplied();
	if (!is_charging)
		return 0;
	dc_psy = power_supply_get_by_name("wireless");
	if (dc_psy) {
		rc = power_supply_get_property(dc_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (rc < 0)
			ts_err("Couldn't get DC online status, rc=%d\n", rc);
		else if (val.intval == 1)
			return 1;
	}
	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		rc = power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (rc < 0)
			ts_err("Couldn't get usb online status, rc=%d\n", rc);
		else if (val.intval == 1)
			return 1;
	}
	return 0;
}
static void charger_power_supply_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, power_supply_work);
	const struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int charge_status = -1;
	if (core_data->init_stage < CORE_INIT_STAGE2 || atomic_read(&core_data->suspended)) {
		ts_debug("Init stage,forbid changing charger status");
		return;
	}
	charge_status = !!goodix_get_charging_status();
	ts_debug("power supply changed,Power_supply_event:%d", charge_status);
	if (charge_status != core_data->charger_status || core_data->charger_status < 0) {
		core_data->charger_status = charge_status;
		if (charge_status) {
			ts_info("charger usb in");
			hw_ops->charger_on(core_data, true);
		} else {
			ts_info("charger usb exit");
			hw_ops->charger_on(core_data, false);
		}
	}
}
static int charger_status_event_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct goodix_ts_core *core_data = container_of(nb, struct goodix_ts_core, charger_notifier);
	if (!core_data)
		return 0;
	queue_work(core_data->event_wq, &core_data->power_supply_work);
	return 0;
}
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */

static void goodix_self_check(struct work_struct *work)
{
	struct goodix_ts_core *cd =
			container_of(work, struct goodix_ts_core, self_check_work);
	u32 fw_state_addr = cd->ic_info.misc.fw_state_addr;
	int update_flag = UPDATE_MODE_BLOCK | UPDATE_MODE_SRC_REQUEST | UPDATE_MODE_FORCE;
	u8 cur_cycle_cnt = 0;
	u8 pre_cycle_cnt = 0;
	int err_cnt = 0;
	int retry = 5;

	while (retry--) {
		cd->hw_ops->read(cd, fw_state_addr, &cur_cycle_cnt, 1);
		if (cur_cycle_cnt == pre_cycle_cnt)
			err_cnt++;
		pre_cycle_cnt = cur_cycle_cnt;
		msleep(20);
	}
	if (err_cnt > 1) {
		ts_err("Warning! The firmware maybe running abnormal, need upgrade.");
		goodix_do_fw_update(cd->ic_configs[CONFIG_TYPE_NORMAL],
				update_flag);
	}
}

int goodix_ts_stage2_init(struct goodix_ts_core *cd)
{
	int ret;

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

/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
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
	INIT_WORK(&cd->gesture_work, goodix_set_gesture_work);
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */
/*N17 code for HQ-301563 by jiangyue at 2023/7/12 start*/
	INIT_WORK(&cd->suspend_work, goodix_ts_suspend_work);
	INIT_WORK(&cd->resume_work, goodix_ts_resume_work);
/*N17 code for HQ-301563 by jiangyue at 2023/7/12 end*/
/*N17 code for HQ-291656 by gaoxue at 2023/5/9 start*/
/* N17 code for HQ-305336 by jiangyue at 2023/7/5 start */
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	cd->fb_notifier.notifier_call = goodix_ts_fb_notifier_callback;
	ret = mi_disp_register_client(&cd->fb_notifier);
	if (ret) {
		ts_err("[FB]Unable to register fb_notifier: %d", ret);
	}
#endif
/* N17 code for HQ-305336 by jiangyue at 2023/7/5 end */
/*N17 code for HQ-291656 by gaoxue at 2023/5/9 end*/

/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
	/* register charger status change notifier */
	INIT_WORK(&cd->power_supply_work, charger_power_supply_work);
	cd->charger_notifier.notifier_call = charger_status_event_callback;
	if (power_supply_reg_notifier(&cd->charger_notifier))
		ts_err("failed to register charger notifier client");
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */

	/*N17 code for HQ-291116 by gaoxue at 2023/4/24 start*/
	/* get ts lockdown info */
	goodix_ts_get_lockdown_info(cd);
	/*N17 code for HQ-291116 by gaoxue at 2023/4/24 end*/

	/* create sysfs files */
	goodix_ts_sysfs_init(cd);

	/* create procfs files */
	goodix_ts_procfs_init(cd);

	/* esd protector */
	goodix_ts_esd_init(cd);

	/* gesture init */
	gesture_module_init();

	/* inspect init */
	inspect_module_init(cd);

	/* Do self check on first boot */
	INIT_WORK(&cd->self_check_work, goodix_self_check);
	schedule_work(&cd->self_check_work);

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
	int update_flag = UPDATE_MODE_BLOCK | UPDATE_MODE_SRC_REQUEST;
	struct goodix_ts_core *cd = data;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	/* step 1: read version */
	ret = hw_ops->read_version(cd, &cd->fw_version);
	if (ret < 0) {
		ts_err("failed to get version info, try to upgrade");
		update_flag |= UPDATE_MODE_FORCE;
	}

	/* step 2: read ic info */
	ret = hw_ops->get_ic_info(cd, &cd->ic_info);
	if (ret < 0) {
		ts_err("failed to get ic info, try to upgrade");
		update_flag |= UPDATE_MODE_FORCE;
	}

	/* step 3: get config data from config bin */
	ret = goodix_get_config_proc(cd);
	if (ret)
		ts_info("no valid ic config found");
	else
		ts_info("success get valid ic config");

	/* step 4: init fw struct add try do fw upgrade */
	ret = goodix_fw_update_init(cd);
	if (ret) {
		ts_err("failed init fw update module");
		goto err_out;
	}

	/* step 5: do upgrade */
	ts_info("update flag: 0x%X", update_flag);
	ret = goodix_do_fw_update(cd->ic_configs[CONFIG_TYPE_NORMAL],
			update_flag);
	if (ret)
		ts_err("failed do fw update");

	print_ic_info(&cd->ic_info);

	/*N17 code for HQ-296326 by gaoxue at 2023/5/18 start*/
	goodix_ts_hw_info(cd);
	/*N17 code for HQ-296326 by gaoxue at 2023/5/18 end*/

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

/* goodix fb test */
// static void test_suspend(void)
// {
// 	goodix_ts_suspend(goodix_modules.core_data);
// }

// static void test_resume(void)
// {
// 	goodix_ts_resume(goodix_modules.core_data);
// }

/*N17 code for HQ-292221 by gaoxue at 2023/4/19 start*/
static ssize_t goodix_fw_version_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_core_data->hw_ops;
	struct goodix_fw_version chip_ver;
	char k_buf[100] = {0};
	int ret = 0;
	int cnt = -EINVAL;
	ts_info("%s",  __func__);

	if (*pos != 0 || !hw_ops)
		return 0;
	if (hw_ops->read_version) {
		ret = hw_ops->read_version(goodix_core_data, &chip_ver);
		if (!ret) {
			cnt = snprintf(&k_buf[0], sizeof(k_buf),
					"patch_pid:%s\n",
					chip_ver.patch_pid);
			cnt += snprintf(&k_buf[cnt], sizeof(k_buf),
					"patch_vid:%02x%02x%02x%02x\n",
					chip_ver.patch_vid[0], chip_ver.patch_vid[1],
					chip_ver.patch_vid[2], chip_ver.patch_vid[3]);
		}
	}

	if (hw_ops->get_ic_info) {
		ret = hw_ops->get_ic_info(goodix_core_data, &goodix_core_data->ic_info);
		if (!ret) {
			cnt += snprintf(&k_buf[cnt], sizeof(k_buf),
					"config_version:%x\n", goodix_core_data->ic_info.version.config_version);
		}
	}
	/*N17 code for HQ-291656 by gaoxue at 2023/5/9 start*/
	if (strlen(k_buf) == 0) {
		strcpy(k_buf, "please resume tp");
		cnt = strlen("please resume tp");
	}
	ts_info("k_buf =%s, cnt =%d, count =%d", k_buf, cnt ,count);
	/*N17 code for HQ-291656 by gaoxue at 2023/5/9 end*/
	cnt = cnt > count ? count : cnt;
	ret = copy_to_user(buf, k_buf, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct proc_ops goodix_fw_version_info_ops = {
	.proc_read = goodix_fw_version_info_read,
};
/*N17 code for HQ-292221 by gaoxue at 2023/4/19 end*/

/*N17 code for HQ-291116 by gaoxue at 2023/4/24 start*/
static ssize_t goodix_lockdown_info_read(struct file *file, char __user *buf,
							 size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
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
static const struct proc_ops goodix_lockdown_info_ops = {
	.proc_read = goodix_lockdown_info_read,
};

int goodix_ts_get_lockdown_info(struct goodix_ts_core *cd)
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
/*N17 code for HQ-291116 by gaoxue at 2023/4/24 end*/

/*N17 code for HQ-296326 by gaoxue at 2023/5/18 start*/
int goodix_ts_hw_info(struct goodix_ts_core *core_data)
{
	snprintf(gt_hw_info, sizeof(gt_hw_info),
			"[Vendor]:Tianma [TP-IC]:GT%s [FW]:%02x%02x%02x%02x [Config]:%x\n",
			core_data->fw_version.patch_pid,
			core_data->fw_version.patch_vid[0], core_data->fw_version.patch_vid[1],
			core_data->fw_version.patch_vid[2], core_data->fw_version.patch_vid[3],
			core_data->ic_info.version.config_version);

	hq_regiser_hw_info(HWID_CTP, gt_hw_info);
	return 0;
}
/*N17 code for HQ-296326 by gaoxue at 2023/5/18 end*/

/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
/*
 * bit 0: double tap
 * bit 1: single tap
 */
 /* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
 /* N17 code for HQ-290808 by jiangyue at 2023/6/19 start */
static void goodix_set_gesture_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, gesture_work);
	ts_info("aod is 0x%x", core_data->aod_status);
	ts_info("enable is 0x%x", core_data->gesture_type);
	if (core_data->aod_status)
		core_data->gesture_type |= GESTURE_SINGLE_TAP;
	else
		core_data->gesture_type &= ~GESTURE_SINGLE_TAP;
	ts_info("set gesture_enabled:%d", core_data->gesture_type);

	if(core_data->gesture_type != 0)
		goodix_gesture_enable(core_data->gesture_type != 0);
}
/* N17 code for HQ-290808 by jiangyue at 2023/6/19 end */
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */

static void goodix_set_game_work(struct work_struct *work)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_core_data->hw_ops;
	u8 data0 = 0;
	u8 data1 = 0;
	bool on = false;
	u8 temp_value = 0;
	int ret = 0;
	int i = 0;
	bool update = false;
	static bool expert_mode = false;

	if (goodix_core_data->work_status == TP_SLEEP) {
		ts_info("suspended, skip");
		return;
	}

	mutex_lock(&goodix_core_data->core_mutex);
	for (i = 0; i <= Touch_Panel_Orientation; i++) {
		if (xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] !=
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]) {

			update = true;
			if (Touch_Expert_Mode == i) {
				expert_mode = true;
				ts_info("expert mode set");
			}
			else if ((i == Touch_Tolerance)
				|| (i == Touch_UP_THRESHOLD)
				|| (i == Touch_Aim_Sensitivity)
				|| (i == Touch_Tap_Stability))
				expert_mode = false;

			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
		}
	}

	if (!update) {
		ts_info("no need update mode value");
		mutex_unlock(&goodix_core_data->core_mutex);
		return;
	}

	for (i = 0; i <= Touch_Panel_Orientation; i++) {
		switch (i) {
		case Touch_Game_Mode:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE];
			on = !!temp_value;
			break;
		case Touch_Active_MODE:
			break;
		case Touch_UP_THRESHOLD:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
			data0 &= 0xF8;
			data0 |= temp_value;
			break;
		case Touch_Tolerance:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
			data0 &= 0xC7;
			data0 |= (temp_value << 3);
			break;
		case Touch_Panel_Orientation:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
			if (temp_value == PANEL_ORIENTATION_DEGREE_90)
				temp_value = 1;
			else if (temp_value == PANEL_ORIENTATION_DEGREE_270)
				temp_value = 2;
			else
				temp_value = 0;
			data0 &= 0x3F;
			data0 |= (temp_value << 6);
			break;
		case Touch_Aim_Sensitivity:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE];
			data1 &= 0xC7;
			data1 |= (temp_value << 3);
			break;
		case Touch_Tap_Stability:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE];
			data1 &= 0xF8;
			data1 |= temp_value;
			break;
		case Touch_Edge_Filter:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
			data1 &= 0x3F;
			data1 |= (temp_value << 6);
			break;
		case Touch_Expert_Mode:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE];
			temp_value = temp_value - 1;
			if (expert_mode) {
				data0 &= 0xF8;
				data0 |= (u8)goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN + 1];
				data0 &= 0xC7;
				data0 |= (u8)(goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN] << 3);
				data1 &= 0xC7;
				data1 |= (u8)(goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN + 2] << 3);
				data1 &= 0xF8;
				data1 |= (u8)goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN + 3];
			}
			break;
		default:
			/* Don't support */
			break;

		};
	}

	ret = hw_ops->game(goodix_core_data, data0, data1, !!on);
	/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 start */
	if (ret < 0) {
		ts_err("send game mode fail");
        } else {
		ret = hw_ops->hdle_mode_set(goodix_core_data, !!on);
		if (ret < 0)
			ts_err("send hdle mode fail");
        }
	/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 end */
	mutex_unlock(&goodix_core_data->core_mutex);
}

static int goodix_set_cur_value(int gtp_mode, int gtp_value)
{
	int ret = 0;

	ts_info("mode:%d, value:%d", gtp_mode, gtp_value);
	if (!goodix_core_data || goodix_core_data->init_stage != CORE_INIT_STAGE2) {
		ts_err("initialization not completed, return");
		return 0;
	}
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 start */
	if (gtp_mode == Touch_Aod_Enable && goodix_core_data && gtp_value >= 0) {
		goodix_core_data->aod_status = gtp_value;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
		return 0;
	}
/* N17 code for HQ-290598 by jiangyue at 2023/6/6 end */
/* N17 code for HQ-322938 by zhangzhijian5 at 2023/8/28 start */
	if (gtp_mode >= Touch_Mode_NUM) {
		ts_err("gtp mode is error:%d", gtp_mode);
		return -EINVAL;
	}

	xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] = gtp_value;
	if (xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MAX_VALUE]) {

		xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MAX_VALUE];

	} else if (xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MIN_VALUE]) {

		xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MIN_VALUE];
	}
/* N17 code for HQ-322938 by zhangzhijian5 at 2023/8/28 end */

	if (gtp_mode <= Touch_Panel_Orientation) {/*power state no need call game work*/
		queue_work(goodix_core_data->game_wq, &goodix_core_data->game_work);
	} else {
		xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE];
	}
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
		for (i = 0; i <= Touch_Panel_Orientation; i++) {
			if (i == Touch_Panel_Orientation)
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
			else {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
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

	/* tap sensitivity */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 3;

	/* latency */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 2;

	/* aim sensitivity */
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_CUR_VALUE] = 2;
	/* tap stability */
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_CUR_VALUE] = 2;
	/* edge filter */
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;
	/* Expert Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MAX_VALUE] = GAME_ARRAY_SIZE;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MIN_VALUE] = 1;
	/*Orientation */
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
}


static u8 goodix_panel_color_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[2];
}

static u8 goodix_panel_vendor_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[0];
}

static u8 goodix_panel_display_read(void)
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
	/* N17 code for HQ-299577 by liunianliang at 2023/6/19 start */
	struct goodix_ts_hw_ops *hw_ops;
	int ret = 0;

	ts_info("palm sensor value : %d", value);
	if (!goodix_core_data) {
		ts_err("goodix core data os NULL");
		return -EINVAL;
	}

	hw_ops = goodix_core_data->hw_ops;
	/* N17 code for HQ-299577 by liunianliang at 2023/6/19 end */

	goodix_core_data->palm_status = value;
	if (goodix_core_data->work_status == TP_NORMAL)
		ret = hw_ops->palm_on(goodix_core_data, !!value);

	return ret;
}
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */

/* N17 code for HQ-307700 by p-xionglei6 at 2023.07.24 start */
static int goodix_touch_edge_mode_set(int value)
{
	struct goodix_ts_hw_ops *hw_ops = NULL;
	int ret = 0;
	u8 data0 = 0;
	u8 data1 = 0;

	ts_info("edge value : %d", value);

	if ((!goodix_core_data) || (!goodix_core_data->hw_ops)) {
		ts_err("goodix_core_data or hw_ops is NULL");
		return -EINVAL;
	}

	hw_ops = goodix_core_data->hw_ops;
	ret = hw_ops->edge_mode_set(goodix_core_data, data0, data1, value);

	return ret;
}
/* N17 code for HQ-307700 by p-xionglei6 at 2023.07.24 end */

/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 start */
static int goodix_touch_hdle_mode_set(bool value)
{
	struct goodix_ts_hw_ops *hw_ops = NULL;
	int ret = 0;

	ts_info("hdle value : %d", value);

	if ((!goodix_core_data) || (!goodix_core_data->hw_ops)) {
		ts_err("goodix_core_data or hw_ops is NULL");
		return -EINVAL;
	}

	hw_ops = goodix_core_data->hw_ops;
	ret = hw_ops->hdle_mode_set(goodix_core_data, value);

	return ret;
}
/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 end */

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
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -ENOMEM;
	}
	/*N17 code for HQ-292221 by gaoxue at 2023/4/19 start*/
	goodix_core_data = core_data;
	/*N17 code for HQ-292221 by gaoxue at 2023/4/19 end*/
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

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&core_data->ts_notifier);

	device_init_wakeup(core_data->bus->dev, 1);

	/*N17 code for HQ-291116 by gaoxue at 2023/4/24 start*/
	core_data->tp_lockdown_info_proc =
		proc_create("tp_lockdown_info", 0664, NULL, &goodix_lockdown_info_ops);
	/*N17 code for HQ-291116 by gaoxue at 2023/4/24 end*/

	/*N17 code for HQ-292221 by gaoxue at 2023/4/19 start*/
	core_data->tp_fw_version_proc =
		proc_create("tp_info", 0664, NULL, &goodix_fw_version_info_ops);
	/*N17 code for HQ-292221 by gaoxue at 2023/4/19 end*/

/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
	if (core_data->goodix_tp_class == NULL) {
		core_data->goodix_tp_class = get_xiaomi_touch_class();
		if (core_data->goodix_tp_class) {
			core_data->goodix_touch_dev = device_create(core_data->goodix_tp_class, NULL, 0x38, core_data, "tp_dev");
			if (IS_ERR(core_data->goodix_touch_dev)) {
				ts_err("Failed to create device !\n");
				goto err_class_create;
			}
			dev_set_drvdata(core_data->goodix_touch_dev, core_data);
		}
	}

	core_data->game_wq = alloc_workqueue("gtp-game-queue",
				WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!core_data->game_wq)
		ts_err("goodix cannot create game work thread");
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
	/* N17 code for HQ-307700 by p-xionglei6 at 2023.07.24 start */
	xiaomi_touch_interfaces.touch_edge_mode_set = goodix_touch_edge_mode_set;
	/* N17 code for HQ-307700 by p-xionglei6 at 2023.07.24 end */
	/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 start */
	xiaomi_touch_interfaces.touch_hdle_mode_set = goodix_touch_hdle_mode_set;
	/* N17 code for HQ-310258 by zhangzhijian5 at 2023/7/29 end */

	xiaomitouch_register_modedata(0, &xiaomi_touch_interfaces);
	goodix_init_touchmode_data();
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */

	/* debug node init */
	goodix_tools_init();

	/* goodix fb test */
	// fb_firefly_register(test_suspend, test_resume);

	core_data->init_stage = CORE_INIT_STAGE1;
	goodix_modules.core_data = core_data;
	core_module_prob_sate = CORE_MODULE_PROB_SUCCESS;

	/* Try start a thread to get config-bin info */
	goodix_start_later_init(core_data);

	ts_info("goodix_ts_core probe success");
	return 0;

/* N17 code for HQ-296762 by jiangyue at 2023/6/2 start */
err_class_create:
	class_destroy(core_data->goodix_tp_class);
	core_data->goodix_tp_class = NULL;
/* N17 code for HQ-296762 by jiangyue at 2023/6/2 end */

err_out:
	core_data->init_stage = CORE_INIT_FAIL;
	core_module_prob_sate = CORE_MODULE_PROB_FAILED;
	ts_err("goodix_ts_core failed, ret:%d", ret);
	return ret;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;

	goodix_ts_unregister_notifier(&core_data->ts_notifier);
	goodix_tools_exit();

	if (core_data->init_stage >= CORE_INIT_STAGE2) {
		gesture_module_exit();
		inspect_module_exit();
		hw_ops->irq_enable(core_data, false);
	/*N17 code for HQ-291656 by gaoxue at 2023/5/9 start*/
	/* N17 code for HQ-305336 by jiangyue at 2023/7/5 start */
	#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
		mi_disp_unregister_client(&core_data->fb_notifier);
	#endif
	/* N17 code for HQ-305336 by jiangyue at 2023/7/5 end */
	/*N17 code for HQ-291656 by gaoxue at 2023/5/9 end*/
		core_module_prob_sate = CORE_MODULE_REMOVED;
		if (atomic_read(&core_data->ts_esd.esd_on))
			goodix_ts_esd_off(core_data);
		goodix_ts_unregister_notifier(&ts_esd->esd_notifier);

		goodix_fw_update_uninit();
		goodix_ts_input_dev_remove(core_data);
		goodix_ts_pen_dev_remove(core_data);
		goodix_ts_sysfs_exit(core_data);
		goodix_ts_procfs_exit(core_data);
		goodix_ts_power_off(core_data);
	}

	return 0;
}

/*N17 code for HQ-291656 by gaoxue at 2023/5/9 start*/
#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops dev_pm_ops = {
#if 0
	.suspend = goodix_ts_pm_suspend,
	.resume = goodix_ts_pm_resume,
#endif
};
#endif
/*N17 code for HQ-291656 by gaoxue at 2023/5/9 end*/

static const struct platform_device_id ts_core_ids[] = {
	{.name = GOODIX_CORE_DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(platform, ts_core_ids);

static struct platform_driver goodix_ts_driver = {
	.driver = {
		.name = GOODIX_CORE_DRIVER_NAME,
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &dev_pm_ops,
#endif
	},
	.probe = goodix_ts_probe,
	.remove = goodix_ts_remove,
	.id_table = ts_core_ids,
};

/* N17 code for HQ-291087 by liunianliang at 2023/5/29 start */
static int skip_load = 0;
module_param(skip_load, int, S_IRUSR);

static int force_load = 0;
module_param(force_load, int, S_IRUSR);

struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1];
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

static int __init goodix_ts_core_init(void)
{
	int ret;
	struct device_node *lcm_name;
	struct tag_videolfb *videolfb_tag = NULL;
	struct tag_bootmode *tag_boot = NULL;
	unsigned long size = 0;

	/* for debug, you can use: 'insmod gt9916r.ko skip_load=1' */
	if (skip_load) {
		ts_err("get a skip flag, don't load gt9916r ko!");
		return 0;
	}

	if (force_load)
		goto force_load_ko;

	lcm_name = of_find_node_by_path("/chosen");

	if (lcm_name) {
		tag_boot = (struct tag_bootmode *)of_get_property(lcm_name, "atag,boot", NULL);
		if (tag_boot && (tag_boot->bootmode == 8 || tag_boot->bootmode == 9)) {
			ts_err("in kpoc mode, don't load focaltech_tp ko!");
			return 0;
		}

		videolfb_tag = (struct tag_videolfb *)of_get_property(lcm_name,
				"atag,videolfb",(int *)&size);
		if (!videolfb_tag) {
			ts_err("Invalid lcm name!!!");
			return 0;
		}
		ts_err("goodix_ts_core_init read lcm name : %s", videolfb_tag->lcmname);
		if (strcmp("n17_36_02_0a_dsc_vdo_lcm_drv",
				videolfb_tag->lcmname) == 0) {
			ts_err("goodix tp!!!");
		} else {
			ts_err("not goodix tp!!!");
			return 0;
		}
	}

force_load_ko:

	ts_info("Core layer init:%s", GOODIX_DRIVER_VERSION);
#ifdef CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI
	ret = goodix_spi_bus_init();
#else
	ret = goodix_i2c_bus_init();
#endif
	if (ret) {
		ts_err("failed add bus driver");
		return ret;
	}
	return platform_driver_register(&goodix_ts_driver);
}
/* N17 code for HQ-291087 by liunianliang at 2023/5/29 end */

static void __exit goodix_ts_core_exit(void)
{
	ts_info("Core layer exit");
	platform_driver_unregister(&goodix_ts_driver);
#ifdef CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI
	goodix_spi_bus_exit();
#else
	goodix_i2c_bus_exit();
#endif
}

late_initcall(goodix_ts_core_init);
module_exit(goodix_ts_core_exit);

/*N17 code for HQ-292221 by gaoxue at 2023/4/19 start*/
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
/*N17 code for HQ-292221 by gaoxue at 2023/4/19 end*/
MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
