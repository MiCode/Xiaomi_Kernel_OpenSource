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
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include <drm/drm_notifier_odm.h>
#include "goodix_ts_core.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#include <linux/panel_notifier.h>

#define GOOIDX_INPUT_PHYS	"goodix_ts/input0"
#define PINCTRL_STATE_ACTIVE    "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND   "pmx_ts_suspend"

static int goodix_ts_remove(struct platform_device *pdev);
int goodix_start_later_init(struct goodix_ts_core *ts_core);
void goodix_ts_dev_release(void);
int goodix_fw_update_init(struct goodix_ts_core *core_data);
void goodix_fw_update_uninit(void);

#if TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
static int gtp_get_mode_value(int mode, int value_type);
static int gtp_get_mode_all(int mode, int *value);
static int gtp_reset_mode(int mode);
static int gtp_set_cur_value(int fts_mode, int fts_value);
static void gtp_init_touchmode_data(void);
#endif

struct goodix_module goodix_modules;
DEFINE_MUTEX(report_lock);

#define CORE_MODULE_UNPROBED     0
#define CORE_MODULE_PROB_SUCCESS 1
#define CORE_MODULE_PROB_FAILED -1
#define CORE_MODULE_REMOVED     -2
int core_module_prob_sate = CORE_MODULE_UNPROBED;

#define WAKEUP_OFF 4
#define WAKEUP_ON 5
static int gesture_switch_flag = 0;

int wt_gsx_tp_gesture_callback(bool flag)
{
	struct goodix_ts_core *cd = goodix_modules.core_data;

	if (flag) {
		cd->gesture_enable = 1;
		gesture_switch_flag = 1;
	} else {
		cd->gesture_enable = 0;
		gesture_switch_flag = 0;
	}

	return 0;
}

int gsx_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	ts_info("Enter. type = %u, code = %u, value = %d", type, code, value);
	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value == WAKEUP_OFF)
			wt_gsx_tp_gesture_callback(false);
		else if (value == WAKEUP_ON)
			wt_gsx_tp_gesture_callback(true);

	}
	ts_info("Exit");
	return 0;
}

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

	ts_info("Moudle [%s] unregistered",
		module->name ? module->name : " ");
	return 0;
}
EXPORT_SYMBOL_GPL(goodix_unregister_ext_module);

/**
 *	set screen global mode
 *
 *  rotation: screen rotation, -1 express default value
 *  edge: edge of inhibition, -1 express default value
 *  filter: track hand level, -1 express default value
 *  leave: leave level, -1 express default value
 *  game: game mode switch, -1 express default value
 */
static int goodix_set_global_mode(struct goodix_ts_core *core_data,
		int rotation, int edge, int filter, int leave, int game)
{
	struct goodix_ts_device *dev = core_data->ts_dev;
	struct goodix_ts_cmd tmp_cmd;
	unsigned int check_sum = 0;
	static u8 status[2];
	int ret;

	if (rotation >= 0) {
		status[0] &= 0x3F;
		status[0] |= (u8)rotation << 6;
	}

	if (edge >= 0) {
		status[1] &= 0x3F;
		status[1] |= (u8)edge << 6;
	}

	if (filter >= 0) {
		status[0] &= 0xC7;
		status[0] |= (u8)filter << 3;
	}

	if (leave >= 0) {
		status[0] &= 0xF8;
		status[0] |= (u8)leave;
	}

	tmp_cmd.initialized = true;
	tmp_cmd.cmd_reg = dev->reg.command;
	ts_info("&dev->reg.command = %d",dev->reg.command);
	tmp_cmd.length = 5;

	if (game >= 0)
		tmp_cmd.cmds[0] = GTP_GAME_CMD_ENTER;
	else
		tmp_cmd.cmds[0] = GTP_GAME_CMD_EXIT;

	tmp_cmd.cmds[1] = status[0];
	tmp_cmd.cmds[2] = status[1];
	check_sum = tmp_cmd.cmds[0] + tmp_cmd.cmds[1] + tmp_cmd.cmds[2];
	tmp_cmd.cmds[3] = (check_sum >> 8) & 0xFF;
	tmp_cmd.cmds[4] = check_sum & 0xFF;

	ts_info("tmp_cmd.cmds[0] = %x,tmp_cmd.cmds[1]= %x,tmp_cmd.cmds[2] = %x,tmp_cmd.cmds[3] = %x",tmp_cmd.cmds[0],tmp_cmd.cmds[1],tmp_cmd.cmds[2],tmp_cmd.cmds[3]);
	ret = dev->hw_ops->send_cmd(dev, &tmp_cmd);

	if (ret < 0)
		ts_err("failed set global mode");

	return ret;

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

	if (!goodix_dbg.dentry)
		return;
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
	struct goodix_ext_module *module, *next;
	size_t offset = 0;
	int r;

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(module, next,
					 &goodix_modules.head, list) {
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
static ssize_t goodix_ts_reset_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
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
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int ret, i, offset;
	char *cfg_buf;

	cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cfg_buf)
		return -ENOMEM;

	if (ts_dev->hw_ops->read_config)
		ret = ts_dev->hw_ops->read_config(ts_dev, cfg_buf, PAGE_SIZE);
	else
		ret = -EINVAL;

	if (ret <= 0) {
		kfree(cfg_buf);
		return -EINVAL;
	}

	offset = 0;
	for (i = 0; i < ret && offset < PAGE_SIZE - 5; i++) {
		if (i != 0 && i % 20 == 0)
			buf[offset++] = '\n';
		if (offset + 3 < PAGE_SIZE - 1) {
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					"%02x,", cfg_buf[i]);
		} else {
			offset += snprintf(&buf[offset], PAGE_SIZE - offset, "EOF");
			break;
		}
	}

	kfree(cfg_buf);
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

static ssize_t goodix_ts_send_cfg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int en, r;
	const struct firmware *cfg_img;
	struct goodix_ts_config *config = NULL;

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
	if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
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
	int en;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	goodix_ts_irq_enable(core_data, en);
	return count;
}

/* open short test */
static ssize_t goodix_ts_tp_test_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	struct ts_rawdata_info *info;

	ts_info("test start!");
	info  = kzalloc(sizeof(struct ts_rawdata_info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory\n");
		 return -ENOMEM;
	}
	ret = gtx8_get_rawdata(dev,info);
	if (ret){
		ts_err("test error!");
		ret = 0;
	}

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

static ssize_t goodix_ts_tp_test_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int en = 0;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;
	set_test_flag(en);
	ts_info("save result in csv file:%d(0:disable,1:enable)!",en);

	return count;
}

/*reg read/write */
static u16 rw_addr;
static u32 rw_len;
static u8 rw_flag;
static u8 store_buf[32];
static u8 show_buf[PAGE_SIZE];
static ssize_t goodix_ts_reg_rw_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	if (!rw_addr || !rw_len) {
		ts_err("address(0x%x) and length(%d) cann't be null\n",
			rw_addr, rw_len);
		return -EINVAL;
	}

	if (rw_flag != 1) {
		ts_err("invalid rw flag %d, only support [1/2]", rw_flag);
		return -EINVAL;
	}

	ret = ts_dev->hw_ops->read(ts_dev, rw_addr, show_buf, rw_len);
	if (ret) {
		ts_err("failed read addr(%x) length(%d)\n", rw_addr, rw_len);
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
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	char *pos = NULL, *token = NULL;
	long result = 0;
	int ret, i;

	if (!buf || !count) {
		ts_err("invalid params\n");
		goto err_out;
	}

	if (buf[0] == 'r') {
		rw_flag = 1;
	} else if (buf[0] == 'w') {
		rw_flag = 2;
	} else {
		ts_err("string must start with 'r/w'\n");
		goto err_out;
	}

	/* get addr */
	pos = (char *)buf;
	pos += 2;
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid address info\n");
		goto err_out;
	} else {
		if (kstrtol(token, 16, &result)) {
			ts_err("failed get addr info\n");
			goto err_out;
		}
		rw_addr = (u16)result;
		ts_info("rw addr is 0x%x\n", rw_addr);
	}

	/* get length */
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid length info\n");
		goto err_out;
	} else {
		if (kstrtol(token, 0, &result)) {
			ts_err("failed get length info\n");
			goto err_out;
		}
		rw_len = (u32)result;
		ts_info("rw length info is %d\n", rw_len);
		if (rw_len > sizeof(store_buf)) {
			ts_err("data len > %lu\n", sizeof(store_buf));
			goto err_out;
		}
	}

	if (rw_flag == 1)
		return count;

	for (i = 0; i < rw_len; i++) {
		token = strsep(&pos, ":");
		if (!token) {
			ts_err("invalid data info\n");
			goto err_out;
		} else {
			if (kstrtol(token, 16, &result)) {
				ts_err("failed get data[%d] info\n", i);
				goto err_out;
			}
			store_buf[i] = (u8)result;
			ts_info("get data[%d]=0x%x\n", i, store_buf[i]);
		}
	}
	ret = ts_dev->hw_ops->write(ts_dev, rw_addr, store_buf, rw_len);
	if (ret) {
		ts_err("failed write addr(%x) data %*ph\n", rw_addr,
			rw_len, store_buf);
		goto err_out;
	}

	ts_info("%s write to addr (%x) with data %*ph\n",
		"success", rw_addr, rw_len, store_buf);

	return count;
err_out:
	snprintf(show_buf, PAGE_SIZE, "%s\n",
		"invalid params, format{r/w:4100:length:[41:21:31]}");
	return -EINVAL;
}

static DEVICE_ATTR(extmod_info, S_IRUGO, goodix_ts_extmod_show, NULL);
static DEVICE_ATTR(driver_info, S_IRUGO, goodix_ts_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, S_IRUGO, goodix_ts_chip_info_show, NULL);
static DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, S_IWUSR | S_IWGRP, NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, S_IRUGO, goodix_ts_read_cfg_show, NULL);
static DEVICE_ATTR(tp_test, S_IRUGO | S_IWUSR | S_IWGRP, goodix_ts_tp_test_show,
		goodix_ts_tp_test_store);
static DEVICE_ATTR(irq_info, S_IRUGO | S_IWUSR | S_IWGRP,
		   goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(reg_rw, S_IRUGO | S_IWUSR | S_IWGRP,
		   goodix_ts_reg_rw_show, goodix_ts_reg_rw_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_extmod_info.attr,
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_tp_test.attr,
	&dev_attr_reg_rw.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static ssize_t goodix_sysfs_config_write(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t count)
{
	struct platform_device *pdev = container_of(kobj_to_dev(kobj),
				struct platform_device, dev);
	struct goodix_ts_core *ts_core = platform_get_drvdata(pdev);
	struct goodix_ts_device *ts_dev = ts_core->ts_dev;
	struct goodix_ts_config *config = NULL;
	int ret;

	if (pos != 0 || count > GOODIX_CFG_MAX_SIZE) {
		ts_info("pos(%d) != 0, cfg size %zu", (int)pos, count);
		return -EINVAL;
	}

	config = kzalloc(sizeof(struct goodix_ts_config), GFP_KERNEL);
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

static ssize_t goodix_sysfs_config_read(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t size)
{
	struct platform_device *pdev = container_of(kobj_to_dev(kobj),
				struct platform_device, dev);
	struct goodix_ts_core *ts_core = platform_get_drvdata(pdev);
	struct goodix_ts_device *ts_dev = ts_core->ts_dev;
	int ret;

	ts_debug("pos = %d, size = %zu", (int)pos, size);

	if (pos != 0)
		return 0;

	if (ts_dev->hw_ops->read_config)
		ret = ts_dev->hw_ops->read_config(ts_dev, buf, PAGE_SIZE);
	else
		ret = -EINVAL;

	ts_debug("read config ret %d", ret);
	return ret;
}

static struct bin_attribute goodix_config_bin_attr = {
	.attr = {
		.name = "config_bin",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP,
	},
	.size = GOODIX_CFG_MAX_SIZE,
	.read = goodix_sysfs_config_read,
	.write = goodix_sysfs_config_write,
};

static int goodix_ts_sysfs_init(struct goodix_ts_core *core_data)
{
	int ret;

	ret = sysfs_create_bin_file(&core_data->pdev->dev.kobj,
				    &goodix_config_bin_attr);
	if (ret) {
		ts_err("failed create config bin attr");
		return ret;
	}

	ret = sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
	if (ret) {
		ts_err("failed create core sysfs group");
		sysfs_remove_bin_file(&core_data->pdev->dev.kobj,
				      &goodix_config_bin_attr);
		return ret;
	}

	return ret;
}

static void goodix_ts_sysfs_exit(struct goodix_ts_core *core_data)
{
	sysfs_remove_bin_file(&core_data->pdev->dev.kobj,
			      &goodix_config_bin_attr);
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

static BLOCKING_NOTIFIER_HEAD(notifier_head);

int earjack_notifier_chain_register(struct notifier_block *n)
{
        return blocking_notifier_chain_register(&notifier_head, n);
}

int earjack_notifier_call_chain(unsigned long val, void *v)
{
        return blocking_notifier_call_chain(&notifier_head, val, v);
}

int earjack_notifier_chain_unregister(struct notifier_block *n)
{
        return blocking_notifier_chain_unregister(&notifier_head, n);
}

/*add for charger notify*/
static BLOCKING_NOTIFIER_HEAD(charger_notifier);

int charger_notifier_chain_register(struct notifier_block *n)
{
        return blocking_notifier_chain_register(&charger_notifier, n);
}

int charger_notifier_call_chain(unsigned long val, void *v)
{
        return blocking_notifier_call_chain(&charger_notifier, val, v);
}

int charger_notifier_chain_unregister(struct notifier_block *n)
{
        return blocking_notifier_chain_unregister(&charger_notifier, n);
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
	int ret;

	ret = blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
	return ret;
}
EXPORT_SYMBOL_GPL(goodix_ts_blocking_notify);

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
	unsigned int touch_num = touch_data->touch_num;
	static u32 pre_fin;
	int i;

	mutex_lock(&report_lock);
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
#if WT_TP_PALM_EN
		/* palm event */
		if (touch_data->palm_flag && goodix_modules.core_data->palm_changed) {
			//goodix_modules.core_data->palm_changed = 0;
			input_report_key(dev, KEY_NUMERIC_POUND, 1);
			input_sync(dev);
			input_report_key(dev, KEY_NUMERIC_POUND, 0);
			input_sync(dev);
		}
#endif
		mutex_unlock(&report_lock);
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
	struct goodix_ts_device *ts_dev =  core_data->ts_dev;
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	u8 irq_flag = 0;
	int r;

	core_data->irq_trig_cnt++;
	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	list_for_each_entry_safe(ext_module, next,
				 &goodix_modules.head, list) {
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
			goodix_ts_report_finger(core_data->input_dev,
					&ts_event->touch_data);
		}
		if (ts_dev->board_data.pen_enable &&
			ts_event->event_type == EVENT_PEN) {
			goodix_ts_report_pen(core_data->pen_dev,
					&ts_event->pen_data);
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
	const struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r;

	/* if ts_bdata-> irq is invalid */
	if (ts_bdata->irq <= 0)
		core_data->irq = gpio_to_irq(ts_bdata->irq_gpio);
	else
		core_data->irq = ts_bdata->irq;

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
	struct goodix_ts_board_data *ts_bdata;
	struct device *dev = NULL;
	int r = 0;

	ts_info("Power init");
	/* dev:i2c client device or spi slave device*/
	dev =  core_data->ts_dev->dev;
	ts_bdata = board_data(core_data);

	if (strlen(ts_bdata->avdd_name)) {
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

	if (strlen(ts_bdata->iovdd_name)) {
		core_data->iovdd = devm_regulator_get(dev,
				 ts_bdata->iovdd_name);
		if (IS_ERR_OR_NULL(core_data->iovdd)) {
			r = PTR_ERR(core_data->iovdd);
			ts_err("Failed to get regulator iovdd:%d", r);
			core_data->iovdd = NULL;
		}
	} else {
		ts_info("iovdd name is NULL");
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
	int r = 0;
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	ts_info("Device power on");
	if (core_data->power_on)
		return 0;

	/*if (!core_data->avdd) {
		core_data->power_on = 1;
		return 0;
	}*/

	ts_err("Device power on  gpio_request");
	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->avdd_gpio,
				  GPIOF_OUT_INIT_HIGH, "avdd-gpio");
	if (r < 0) {
		ts_err("Failed to request avdd_gpio, r:%d", r);
		return r;
	}

	udelay(100);

	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->dvdd_gpio,
				  GPIOF_OUT_INIT_HIGH, "dvdd-gpio");
	if (r < 0) {
		ts_err("Failed to request dvdd_gpio, r:%d", r);
		gpio_free(ts_bdata->avdd_gpio);
		return r;
	}
	gpio_direction_output(core_data->ts_dev->board_data.avdd_gpio, 1);

	gpio_direction_output(core_data->ts_dev->board_data.dvdd_gpio, 1);

	core_data->power_on = 1;

	gpio_free(ts_bdata->avdd_gpio);
	gpio_free(ts_bdata->dvdd_gpio);

	return 0;
}

int goodix_ts_resume_power_on(struct goodix_ts_core *core_data)
{
	ts_info("Device resume power on");
	if (core_data->power_on)
		return 0;

	gpio_direction_output(core_data->ts_dev->board_data.avdd_gpio, 1);

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
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r = 0;

	ts_info("Device power off");
	if (!core_data->power_on)
		return 0;

	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->dvdd_gpio,
				  GPIOF_OUT_INIT_HIGH, "dvdd-gpio");
	if (r < 0) {
		ts_err("Failed to request dvdd_gpio, r:%d", r);
		return r;
	}
	gpio_direction_output(core_data->ts_dev->board_data.dvdd_gpio, 0);

	udelay(100);

	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->avdd_gpio,
				  GPIOF_OUT_INIT_HIGH, "avdd-gpio");
	if (r < 0) {
		ts_err("Failed to request avdd_gpio, r:%d", r);
		gpio_free(ts_bdata->dvdd_gpio);
		return r;
	}

	gpio_direction_output(core_data->ts_dev->board_data.avdd_gpio, 0);

	core_data->power_on = 0;

	gpio_free(ts_bdata->avdd_gpio);
	gpio_free(ts_bdata->dvdd_gpio);

	return r;
}

int goodix_ts_suspend_power_off(struct goodix_ts_core *core_data)
{
	ts_info("Device suspend power off");
	if (!core_data->power_on)
		return 0;

	gpio_direction_output(core_data->ts_dev->board_data.avdd_gpio, 0);

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
		ts_info("Failed to get pinctrl handler[need confirm]");
		core_data->pinctrl = NULL;
		return -EINVAL;
	}
	ts_debug("success get pinctrl");
	/* active state */
	core_data->pin_sta_active = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(core_data->pin_sta_active)) {
		r = PTR_ERR(core_data->pin_sta_active);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_ACTIVE, r);
		core_data->pin_sta_active = NULL;
		goto exit_pinctrl_put;
	}
	ts_debug("success get avtive pinctrl state");

	/* suspend state */
	core_data->pin_sta_suspend = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(core_data->pin_sta_suspend)) {
		r = PTR_ERR(core_data->pin_sta_suspend);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_SUSPEND, r);
		core_data->pin_sta_suspend = NULL;
		goto exit_pinctrl_put;
	}
	ts_debug("success get suspend pinctrl state");

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
	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->reset_gpio,
				  GPIOF_OUT_INIT_HIGH, "reset-gpio");
	if (r < 0) {
		ts_err("Failed to request reset gpio, r:%d", r);
		return r;
	}

	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->irq_gpio,
				  GPIOF_IN, "irq-gpio");
	if (r < 0) {
		ts_err("Failed to request irq gpio, r:%d", r);
		return r;
	}
/*
	r = devm_gpio_request_one(&core_data->pdev->dev, ts_bdata->avdd_gpio,
				  GPIOF_OUT_INIT_HIGH, "avdd_gpio");
	if (r < 0) {
		ts_err("Failed to request avdd_gpio, r:%d", r);
		return r;
	}
*/
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
	core_data->input_dev->event = gsx_gesture_switch;
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
	__set_bit(KEY_GOTO,input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

	/* set input parameters */
	goodix_ts_set_input_params(input_dev, ts_bdata);

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
	input_set_capability(input_dev, EV_KEY, KEY_SLEEP);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);
	input_set_capability(input_dev, EV_KEY, KEY_NUMERIC_POUND);

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
			     GOODIX_PEN_MAX_PRESSURE, 0, 0);

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
	struct goodix_ts_core *core = container_of(ts_esd,
			struct goodix_ts_core, ts_esd);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(core);
	u8 data = GOODIX_ESD_TICK_WRITE_DATA;
	int r = 0;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (hw_ops->check_hw)
		r = hw_ops->check_hw(core->ts_dev);
	if (r < 0) {
		goodix_ts_power_off(core);
		goodix_ts_power_on(core);
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
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;

	if (core->ts_dev->reg.esd == 0)
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
static void goodix_ts_esd_off(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;
	int ret;

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
int goodix_ts_esd_init(struct goodix_ts_core *core)
{
	struct goodix_ts_esd *ts_esd = &core->ts_esd;
	struct goodix_ts_device *dev = core->ts_dev;
	u8 data = GOODIX_ESD_TICK_WRITE_DATA;
	int r;

	if (!dev->hw_ops->check_hw || !dev->reg.esd) {
		ts_info("missing key info for esd check");
		return 0;
	}

	INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
	ts_esd->ts_core = core;
	atomic_set(&ts_esd->esd_on, 0);
	ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
	goodix_ts_register_notifier(&ts_esd->esd_notifier);

	/*init dynamic esd*/
	r = dev->hw_ops->write_trans(core->ts_dev, core->ts_dev->reg.esd,
				     &data, 1);
	if (r < 0)
		ts_err("failed init dynamic esd[ignore]");

	goodix_ts_esd_on(core);

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

#if WT_TP_GRIP_AREA_EN
int wt_gsx_tp_get_screen_angle_callback(void)
{
	return -EPERM;
}

int wt_gsx_tp_set_screen_angle_callback(int angle)
{
	u8 val;
	int ret = -EIO;

	struct goodix_ts_core *cd = goodix_modules.core_data;
	ts_info("set_screen_angle IN");

	if(cd->gtp_game == 1){
		return 0;
	} else{
		if (angle == 90) {
			val = 1;
		} else if (angle == 270) {
			val = 2;
		} else {
			val = 0;
		}

		ts_info("val = %d",val);
		//ret = fts_write_reg(FTS_SET_ANGLE, val);
		ret = goodix_set_global_mode(cd,val,2,0,0,-1);

		if(ret < 0)
			ts_err("write grip_area err\n");

		ts_info("set_screen_angle out");
		return ret;

	}

}
#endif

/**
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
static int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
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
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->before_suspend)
				continue;

			r = ext_module->funcs->before_suspend(core_data,
							      ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
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
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->after_suspend)
				continue;

			r = ext_module->funcs->after_suspend(core_data,
							     ext_module);
			if (r == EVT_CANCEL_SUSPEND) {
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
	ts_info("goodix release connects out!");
	if((gesture_switch_flag == 0) && (core_data->aod_changed == 0) && (core_data->sleep_enable == 0)) {
		gpio_direction_output(ts_dev->board_data.reset_gpio, 0);
		goodix_ts_power_off(core_data);
		ts_info("Suspend power off end");
		return 0;
	} else {
		ts_info("Suspend end");
		return 0;
	}
}

/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int goodix_ts_resume(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_device *ts_dev =
				core_data->ts_dev;
	int r;

	ts_info("Resume start");
	if((gesture_switch_flag == 0) && (core_data->aod_changed == 0) &&  (core_data->sleep_enable == 0))
		goodix_ts_power_on(core_data);

	goodix_ts_release_connects(core_data);

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->before_resume)
				continue;

			r = ext_module->funcs->before_resume(core_data,
							     ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
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
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->after_resume)
				continue;

			r = ext_module->funcs->after_resume(core_data,
							    ext_module);
			if (r == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	goodix_ts_irq_enable(core_data, true);
	/*notify usb event ,inform IC go to charger in/out status*/
	if (core_data->charger_status)
		goodix_charger_in(core_data);
	else
		goodix_charger_out(core_data);

	/*
	 * notify resume event, inform the esd protector
	 * and charger detector to turn on the work
	 */
	ts_info("try notify resume");
	goodix_ts_blocking_notify(NOTIFY_RESUME, NULL);
out:
	ts_debug("Resume end");
	return 0;
}

#ifdef CONFIG_FB
/**
 * goodix_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
int goodix_ts_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct goodix_ts_core *core_data =
		container_of(self, struct goodix_ts_core, fb_notifier);
	struct fb_event *fb_event = data;

	if (fb_event && fb_event->data && core_data) {
		if (event == FB_EARLY_EVENT_BLANK) {
			/* before fb blank */
		} else if (event == FB_EVENT_BLANK) {
			int *blank = fb_event->data;
			if (*blank == FB_BLANK_UNBLANK)
				goodix_ts_resume(core_data);
			else if (*blank == FB_BLANK_POWERDOWN)
				goodix_ts_suspend(core_data);
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
static int goodix_generic_noti_callback(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct goodix_ts_core *ts_core = container_of(self,
			struct goodix_ts_core, ts_notifier);
	struct goodix_ts_device *ts_dev = ts_device(ts_core);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(ts_core);
	int r;

	ts_info("notify event type 0x%x", (unsigned int)action);
	switch (action) {
	case NOTIFY_FWUPDATE_START:
		goodix_ts_irq_enable(ts_core, 0);
		break;
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_FWUPDATE_FAILED:
		r = hw_ops->read_version(ts_dev, &ts_dev->chip_version);
		if (r < 0)
			ts_info("failed read fw version info[ignore]");
		goodix_ts_irq_enable(ts_core, 1);
		break;
	default:
		break;
	}
	return 0;
}

/* add xiaomi game mode start */
#if TOUCHSCREEN_XIAOMI_TOUCHFEATURE

static void gtp_init_touchmode_data(void)
{
	int i;
	ts_info("ENTER into");

	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;

	/* Active Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;

	/* sensivity */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 0;

	/*  Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 0;

	/* edge filter orientation */
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;

	/* edge filter area */
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;

	for (i = 0; i < Touch_Mode_NUM; i++) {
		ts_info("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d", i,
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}

	return;

}

static int gtp_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		ts_err("don't support\n");
	}

	ts_info("mode:%d, value:%d:%d:%d:%d\n", mode, value[0], value[1], value[2], value[3]);

	return 0;

}

static int gtp_reset_mode(int mode)
{
	struct goodix_ts_core *cd = goodix_modules.core_data;

	ts_info("gtp_reset_game_mode enter");

	goodix_set_global_mode(cd ,0,2,3,0,-1);

	mode = 0;
	cd->gtp_game = 0;
	ts_info("gtp_game mode = %d",cd->gtp_game);
	ts_info("mode:%d", mode);

	return 0;

}

static int gtp_set_cur_value(int gtp_mode, int gtp_value)
{
	struct goodix_ts_core *cd = goodix_modules.core_data;
	uint8_t gtp_game_value[4] = { 0  };
	uint8_t temp_value = 0;
	uint8_t ret = 0;
	uint8_t reg_value = 0;

	if (gtp_mode >= Touch_Mode_NUM && gtp_mode < 0) {
		ts_err("fts mode is error:%d", gtp_mode);
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

	ts_info("gtp_mode:%d,gtp_vlue:%d", gtp_mode, gtp_value);

	switch (gtp_mode) {
		case Touch_Game_Mode:
			break;
		case Touch_Active_MODE:
			break;
		case Touch_UP_THRESHOLD:
			/* level 0,1,2,3 for Tap sensivity,sensivity get stronger with 0 to 3 */
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
			gtp_game_value[0] = temp_value;
			cd->gtp_game_hold = gtp_game_value[0];
			break;
		case Touch_Tolerance:
			/* level 0,1,2,3 for Follow_hand_performance,get stronger with 0 to 3 */
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
			gtp_game_value[1] = temp_value;
			cd->gtp_game_tole = gtp_game_value[1];
			break;
		case Touch_Edge_Filter:
			/* filter 0,1,2,3,4,5,6,7,8 = default,1,2,3,4,5,6,7,8 level */
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
			gtp_game_value[2] = temp_value;
			cd->gtp_game_edge = gtp_game_value[2];
			break;
		case Touch_Panel_Orientation:
			/* 0,1,2,3 = 0, 90, 180,270 */
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
			if (temp_value == 0 || temp_value == 2) {
				reg_value = 0;
			} else if (temp_value == 1) {
				reg_value = 1;
			} else if (temp_value == 3) {
				reg_value = 2;
			}
			//gtp_game_value[0] = 0x8C;
			gtp_game_value[3] = reg_value;
			cd->gtp_game_direction = gtp_game_value[3];
			break;
		case Touch_Aod_Enable:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Aod_Enable][SET_CUR_VALUE];
			if (temp_value == 1) {
				cd->aod_changed = 1;
				ts_info("temp_value:%d,aod_changed:%d",temp_value,cd->aod_changed);
			} else {
				cd->aod_changed = 0;
				ts_info("temp_value:%d,aod_changed:%d",temp_value,cd->aod_changed);
			}
		default:
			/* Don't support */
			break;

	};

	ts_info("mode:%d, value:%d,temp_value:%d, game value:0x%x,0x%x,0x%x,0x%x", gtp_mode, gtp_value, temp_value, gtp_game_value[0],
			gtp_game_value[1],gtp_game_value[2],gtp_game_value[3]);
	ts_info("mode:%d, value:%d,temp_value:%d, game value2:0x%x,0x%x,0x%x,0x%x", gtp_mode, gtp_value, temp_value, cd->gtp_game_direction,
			cd->gtp_game_edge,cd->gtp_game_tole,cd->gtp_game_hold);

	xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_CUR_VALUE] =
		xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE];

	if (xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE]) {

		ret = goodix_set_global_mode(cd ,cd->gtp_game_direction,cd->gtp_game_edge,cd->gtp_game_tole,cd->gtp_game_hold,1);

		cd->gtp_game = 1;
		ts_info("set game mode  ret = %d",ret);
		ts_info("gtp_game mode = %d",cd->gtp_game);

		if (ret < 0) {
			ts_err("change game mode fail");
		}
	}

	return 0;
}

static int gtp_get_mode_value(int mode, int value_type)
{

	int value = -1;
	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		ts_err("don't support\n");

	return value;

}

#endif
/* add xiaomi game mode end */

extern int goodix_get_lockdown_info(struct goodix_ts_core *core_data);
extern int proc_node_init(struct goodix_ts_core *core_data);
int goodix_ts_stage2_init(struct goodix_ts_core *core_data)
{
	int r;
	struct goodix_ts_device *ts_dev = ts_device(core_data);

	/* send normal-cfg to firmware */
	r = ts_dev->hw_ops->send_config(ts_dev, &(ts_dev->normal_cfg));
	if (r < 0) {
		ts_info("failed send normal config[ignore]");
	}

	r = ts_dev->hw_ops->read_version(ts_dev, &ts_dev->chip_version);
	if (r < 0)
		ts_info("failed read fw version info[ignore]");

	/* get lockdown info */
	goodix_get_lockdown_info(core_data);

	/* alloc/config/register input device */
	r = goodix_ts_input_dev_config(core_data);
	if (r < 0) {
		ts_err("failed set input device");
		return r;
	}

	if (ts_dev->board_data.pen_enable) {
		r = goodix_ts_pen_dev_config(core_data);
		if (r < 0) {
			ts_err("failed set pen device");
			goto err_finger;
		}
	}
	/* request irq line */
	r = goodix_ts_irq_setup(core_data);
	if (r < 0) {
		ts_info("failed set irq");
		goto exit;
	}
	ts_info("success register irq");

#ifdef CONFIG_FB
	/*add for tp aod*/
	core_data->aod_notif.notifier_call = gtp_aod_notifier_callback;
	aod_notifier_chain_register(&core_data->aod_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	core_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	core_data->early_suspend.resume = goodix_ts_lateresume;
	core_data->early_suspend.suspend = goodix_ts_earlysuspend;
	register_early_suspend(&core_data->early_suspend);
#endif
	/*create sysfs files*/
	goodix_ts_sysfs_init(core_data);

	/* create proc files */
	proc_node_init(core_data);

	/* esd protector */
	goodix_ts_esd_init(core_data);
	return 0;
exit:
	goodix_ts_pen_dev_remove(core_data);
err_finger:
	goodix_ts_input_dev_remove(core_data);
	return r;
}

static int gtp_panel_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct goodix_ts_core *core_data = container_of(self, struct goodix_ts_core, panel_notif);
	struct goodix_ts_cmd earjack_cmd;
	u8 earjack_status = 0;

	ts_info("earjack notifier callback in! earject flag is %d",event);

	earjack_cmd.initialized = true;
	earjack_cmd.cmd_reg = core_data->ts_dev->reg.command;
	earjack_cmd.length = 5;
	if (event) {
		earjack_status = 0x0;
		ts_info("earjack plug in!");
	} else {
		earjack_status = 0x01;
		ts_info("earjack plug out!");
	}
	earjack_cmd.cmds[0] = 0x06 + earjack_status;
	earjack_cmd.cmds[1] = 0x0;
	earjack_cmd.cmds[2] = 0x0;
	earjack_cmd.cmds[3] = 0x0;
	earjack_cmd.cmds[4] = earjack_cmd.cmds[0];
	ts_info("earjack notifier callback cmd is %d,cmd add is: %x",earjack_cmd.cmds[4],earjack_cmd.cmd_reg);
	return core_data->ts_dev->hw_ops->send_cmd(core_data->ts_dev, &earjack_cmd);
}

void goodix_charger_in(struct goodix_ts_core *core_data)
{
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	u8 charger_in_cmd[] = {0x06,0x00,0x00,0x00,0x06};
	int ret = -1, retry;

	ts_info("charger state IN");
	retry = 3;
	mutex_lock(&ts_dev->charger_mutex);
	do {
		ret = ts_dev->hw_ops->write(ts_dev, 0x4160,
				charger_in_cmd, sizeof(charger_in_cmd));
	} while (ret && retry--);
	mutex_unlock(&ts_dev->charger_mutex);
}

void goodix_charger_out(struct goodix_ts_core *core_data)
{
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	u8 charger_out_cmd[] = {0x07, 0x00, 0x00, 0x00, 0x07};
	int ret = -1, retry;

	ts_info("charger state OUT");
	retry = 3;
	mutex_lock(&ts_dev->charger_mutex);
	do {
		ret = ts_dev->hw_ops->write(ts_dev, 0x4160,
				charger_out_cmd, sizeof(charger_out_cmd));
	} while (ret && retry--);
	mutex_unlock(&ts_dev->charger_mutex);
}

static int charger_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct goodix_ts_core *core_data = container_of(self, struct goodix_ts_core, charger_notif);

	if (event) {
		goodix_charger_in(core_data);
		core_data->charger_status = 1;
	} else {
		goodix_charger_out(core_data);
		core_data->charger_status = 0;
	}

	return 0;
}

static int gtp_aod_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct goodix_ts_core *core_data =
		container_of(self, struct goodix_ts_core, aod_notif);
	struct fb_event *panel_aod_event = data;

	ts_info("gtp_aod_notifier_callback");
	if (panel_aod_event && panel_aod_event->data && core_data) {
		if (event == PANEL_AOD_EARLY_EVENT_BLANK) {
			/* before fb blank */
		} else if (event == PANEL_AOD_EVENT_BLANK) {
			int *blank = panel_aod_event->data;
			if (*blank == PANEL_AOD_BLANK_UNBLANK) {
				ts_info("gtp_aod_notifier_callback resume");
				goodix_ts_resume(core_data);
			}
			else if (*blank == PANEL_AOD_BLANK_POWERDOWN){
				ts_info("gtp_aod_notifier_callback suspend");
				goodix_ts_suspend(core_data);
			}
		}
	}

	return 0;
}

/**
 * goodix_ts_probe - called by kernel when Goodix touch
 *  platform driver is added.
 */
static int goodix_ts_probe(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = NULL;
	struct goodix_ts_device *ts_device;
	int r;

	ts_info("goodix_ts_probe IN");

	ts_device = pdev->dev.platform_data;
	if (!ts_device || !ts_device->hw_ops) {
		ts_err("Invalid touch device");
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -ENODEV;
	}

	core_data = kzalloc(sizeof(struct goodix_ts_core), GFP_KERNEL);
	if (!core_data) {
		ts_err("Failed to allocate memory for core data");
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -ENOMEM;
	}

	goodix_core_module_init();
	/* touch core layer is a platform driver */
	core_data->pdev = pdev;
	core_data->ts_dev = ts_device;
	platform_set_drvdata(pdev, core_data);
	core_data->sleep_enable = 0;

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
		goto gpio_err;

	/* confirm it's goodix touch dev or not */
	r = ts_device->hw_ops->dev_confirm(ts_device);
	if (r) {
		ts_err("goodix device confirm failed");
		goto gpio_err;
	}

	/* add xiaomi game mode start */
	if (core_data->gtp_tp_class == NULL) {
#if TOUCHSCREEN_XIAOMI_TOUCHFEATURE
		core_data->gtp_tp_class = get_xiaomi_touch_class();
#endif
		if (core_data->gtp_tp_class) {
			core_data->gtp_touch_dev = device_create(core_data->gtp_tp_class, NULL, 0x38, core_data, "tp_dev");
			if (IS_ERR(core_data->gtp_touch_dev)) {
				ts_err("Failed to create device !");
				goto err_class_create;
			}

			dev_set_drvdata(core_data->gtp_touch_dev, core_data);

#ifdef TOUCHSCREEN_XIAOMI_TOUCHFEATURE
			memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
			xiaomi_touch_interfaces.getModeValue = gtp_get_mode_value;
			xiaomi_touch_interfaces.setModeValue = gtp_set_cur_value;
			xiaomi_touch_interfaces.resetMode = gtp_reset_mode;
			xiaomi_touch_interfaces.getModeAll = gtp_get_mode_all;
			gtp_init_touchmode_data();
			xiaomitouch_register_modedata(&xiaomi_touch_interfaces);

#endif
		}
	}
	/* add xiaomi game mode end */

	/*add for tp_earjack*/
	core_data->panel_notif.notifier_call = gtp_panel_notifier_callback;
	earjack_notifier_chain_register(&core_data->panel_notif);

	/*add for tp_charger*/
	core_data->charger_notif.notifier_call = charger_notifier_callback;
	charger_notifier_chain_register(&core_data->charger_notif);

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&core_data->ts_notifier);

	core_data->initialized = 1;
	goodix_modules.core_data = core_data;
	core_data->init_stage = CORE_INIT_STAGE1;
	core_module_prob_sate = CORE_MODULE_PROB_SUCCESS;

	r = goodix_fw_update_init(core_data);
	if (r) {
		ts_err("failed init fwupdate module, %d", r);
		goto gpio_err;
	}
	/* Try start a thread to get config-bin info */
	r = goodix_start_later_init(core_data);
	if (r) {
		ts_err("Failed start cfg_bin_proc, %d", r);
		goto later_thread_err;
	}

	complete_all(&goodix_modules.core_comp);
	goodix_debugfs_init();
	ts_info("goodix_ts_core probe success");
	return 0;

err_class_create:
	class_destroy(core_data->gtp_tp_class);
	core_data->gtp_tp_class = NULL;

later_thread_err:
	goodix_fw_update_uninit();
gpio_err:
	goodix_ts_power_off(core_data);
out:
	kfree(core_data);
	core_data = NULL;
	core_module_prob_sate = CORE_MODULE_PROB_FAILED;
	ts_err("goodix_ts_core failed, r:%d", r);
	/* wakeup ext module register work */
	complete_all(&goodix_modules.core_comp);
	return r;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;

	if (goodix_unregister_all_module())
		return -EBUSY;

	goodix_fw_update_uninit();
	goodix_ts_unregister_notifier(&core_data->ts_notifier);
	earjack_notifier_chain_unregister(&core_data->panel_notif);
	aod_notifier_chain_unregister(&core_data->aod_notif);
	charger_notifier_chain_unregister(&core_data->charger_notif);
#ifdef CONFIG_FB
	aod_notifier_chain_unregister(&core_data->aod_notif);
#endif
	core_data->initialized = 0;
	core_module_prob_sate = CORE_MODULE_REMOVED;
	if (atomic_read(&core_data->ts_esd.esd_on))
		goodix_ts_esd_off(core_data);
	goodix_ts_unregister_notifier(&ts_esd->esd_notifier);

	goodix_ts_input_dev_remove(core_data);
	goodix_ts_pen_dev_remove(core_data);
	goodix_ts_irq_enable(core_data, false);

	goodix_ts_power_off(core_data);
	goodix_debugfs_exit();
	goodix_ts_sysfs_exit(core_data);
	kfree(core_data);
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
	int ret;

	ts_info("Core layer init:%s", GOODIX_DRIVER_VERSION);
	ret = goodix_bus_init();
	if (ret) {
		ts_err("failed add bus driver");
		return ret;
	}
	return platform_driver_register(&goodix_ts_driver);
}

static void __exit goodix_ts_core_exit(void)
{
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
