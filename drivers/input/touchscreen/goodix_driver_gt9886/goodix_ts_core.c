 /*
  * Goodix Touchscreen Driver
  * Core layer of touchdriver architecture.
  *
  * Copyright (C) 2019 - 2020 Goodix, Inc.
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
#include <drm/drm_notifier_mi.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#include <linux/pm_wakeup.h>
#include "goodix_ts_core.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif
#include <linux/input/touch_common_info.h>
#include "../xiaomi/xiaomi_touch.h"

#define GOOIDX_INPUT_PHYS	"goodix_ts/input0"
#define PINCTRL_STATE_ACTIVE    "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND   "pmx_ts_suspend"

#define PANEL_ORIENTATION_DEGREE_0 	0	/* normal portrait orientation */
#define PANEL_ORIENTATION_DEGREE_90	1	/* anticlockwise 90 degrees */
#define PANEL_ORIENTATION_DEGREE_180	2	/* anticlockwise 180 degrees */
#define PANEL_ORIENTATION_DEGREE_270	3	/* anticlockwise 270 degrees */

extern int goodix_i2c_write(struct goodix_ts_device *dev, unsigned int reg, unsigned char *data, unsigned int len);

static int goodix_ts_remove(struct platform_device *pdev);
int goodix_start_later_init(struct goodix_ts_core *ts_core);
void goodix_ts_dev_release(void);

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
	struct goodix_ext_module *ext_module, *next;
	struct list_head *insert_point = &goodix_modules.head;

	ts_info("__do_register_ext_module IN");

	if (goodix_modules.core_data &&
	    !goodix_modules.core_data->initialized) {
		ts_err("core layer has exit");
		return;
	}

	if (!goodix_modules.core_data) {
		/* waitting for core layer */
		if (!wait_for_completion_timeout(&goodix_modules.core_comp,
						 25 * HZ)) {
			ts_err("Module [%s] timeout", module->name);
			return;
		}
	}

	/* driver probe failed */
	if (!goodix_modules.core_data ||
	    !goodix_modules.core_data->initialized) {
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

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (ext_module == module) {
				ts_info("Module [%s] already exists",
					module->name);
				mutex_unlock(&goodix_modules.mutex);
				return;
			}
		}

		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
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
		INIT_LIST_HEAD(&goodix_modules.head);
		mutex_init(&goodix_modules.mutex);
		init_completion(&goodix_modules.core_comp);
	}

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
	goodix_modules.count--;
	mutex_unlock(&goodix_modules.mutex);

	if (module->funcs && module->funcs->exit)
		module->funcs->exit(goodix_modules.core_data, module);

	ts_info("Moudle [%s] unregistered",
		module->name ? module->name : " ");
	return 0;
}
EXPORT_SYMBOL_GPL(goodix_unregister_ext_module);

static void goodix_remove_all_ext_modules(void)
{
	struct goodix_ext_module *ext_module, *next;

	if (!goodix_modules.initilized || !goodix_modules.core_data)
		return;

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			list_del(&ext_module->list);
			goodix_modules.count--;
			if (ext_module->funcs && ext_module->funcs->exit)
				ext_module->funcs->exit(goodix_modules.core_data,
							ext_module);
		}
	}

	mutex_unlock(&goodix_modules.mutex);
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
	int r = 0;
	struct ts_rawdata_info *info;
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
	info  = kzalloc(sizeof(struct ts_rawdata_info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory\n");
		 return -ENOMEM;
	}
	ret = test_process(dev,info);
	if (ret == 1) {
		ts_err("test PASS!");
	} else {
		ts_err("test FAILED. result:%x", ret);
	}
	goodix_tools_unregister();
	ret = snprintf(buf, sizeof(ret),
			"%d\n",ret);

	if (ret < 0){
		ts_err("print result info error!\n");
		ret =  -EINVAL;
		goto TEST_END;
	}
TEST_END:
	ts_info("test finish!");
	ts_info("info->result = %s\n",info->result);
	kfree(info);
	info = NULL;
	return ret;


}

/* tp get rawdata */
static ssize_t goodix_ts_tp_rawdata_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)

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

	ts_info("get rawdata test finish!");
	return ret;
}

/* tp get test config */
static ssize_t goodix_ts_tp_get_testcfg_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)

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

    ts_info("start get testcfg!");
    ret = get_tp_testcfg(dev, buf, &buf_size);

    goodix_tools_unregister();
    ts_info("get_testcfg finish!");
    return ret;
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
static DEVICE_ATTR(irq_info, S_IRUGO | S_IWUSR | S_IWGRP,
		   goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(reg_rw, S_IRUGO | S_IWUSR | S_IWGRP,
		   goodix_ts_reg_rw_show, goodix_ts_reg_rw_store);
static DEVICE_ATTR(tp_test, S_IRUGO, goodix_ts_tp_test_show, NULL);
static DEVICE_ATTR(tp_get_testcfg, S_IRUGO, goodix_ts_tp_get_testcfg_show, NULL);
static DEVICE_ATTR(tp_rawdata, S_IRUGO, goodix_ts_tp_rawdata_show, NULL);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_extmod_info.attr,
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_reg_rw.attr,
	&dev_attr_tp_test.attr,
	&dev_attr_tp_rawdata.attr,
	&dev_attr_tp_get_testcfg.attr,
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
		ret = ts_dev->hw_ops->read_config(ts_dev, buf);
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
	struct goodix_ts_core *core_data = input_get_drvdata(dev);
	unsigned int touch_num = touch_data->touch_num;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	int i;

	/*first touch down and last touch up condition*/
	if (touch_num)
		input_report_key(dev, BTN_TOUCH, 1);
	else if (!touch_num)
		input_report_key(dev, BTN_TOUCH, 0);

	mutex_lock(&ts_dev->report_mutex);
	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (!touch_data->coords[i].status)
			continue;
		if (touch_data->coords[i].status == TS_RELEASE) {
			input_mt_slot(dev, i);
			if (i == 0) {
				input_report_key(dev, BTN_INFO, 0);
				input_report_abs(dev, ABS_MT_WIDTH_MINOR, 0);
			}
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
 		if ((core_data->event_status & 0x88) != 0x88 || !core_data->fod_status)
			touch_data->coords[i].overlapping_area = 0;
		input_report_abs(dev, ABS_MT_WIDTH_MINOR,
				 touch_data->coords[i].overlapping_area);
		input_report_abs(dev, ABS_MT_WIDTH_MAJOR,
				 touch_data->coords[i].overlapping_area);

		ts_info("Report x=%d, y=%d, touch_major=%d, overlapping_area=%d",
                       touch_data->coords[i].x, touch_data->coords[i].y,
                       touch_data->coords[i].w, touch_data->coords[i].overlapping_area);
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
	if ((core_data->event_status & 0x88) == 0x88 && core_data->fod_status) {
		input_report_key(core_data->input_dev, BTN_INFO, 1);
		/*input_report_key(core_data->input_dev, KEY_INFO, 1); */
		core_data->fod_pressed = true;
		ts_info("BTN_INFO press");
	} else if (core_data->fod_pressed
		   && (core_data->event_status & 0x88) != 0x88) {
		if (unlikely(!core_data->fod_test)) {
			input_report_key(core_data->input_dev, BTN_INFO, 0);
			/*input_report_key(core_data->input_dev, KEY_INFO, 0); */
			ts_info("BTN_INFO release");
			core_data->fod_pressed = false;
		}
	}
	input_sync(dev);
	mutex_unlock(&ts_dev->report_mutex);
}

static void goodix_ts_sleep_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, sleep_work);
	struct goodix_ts_device *ts_dev =  core_data->ts_dev;
	struct goodix_ext_module *ext_module;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	u8 irq_flag = 0;
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
			goodix_ts_report_finger(core_data->input_dev,
					&ts_event->touch_data);
		}
		if (ts_dev->board_data.pen_enable &&
			ts_event->event_type == EVENT_PEN) {
			goodix_ts_report_pen(core_data->pen_dev,
					&ts_event->pen_data);
		}
	}
	/* clean irq flag */
	irq_flag = 0;
	ts_dev->hw_ops->write_trans(ts_dev, ts_dev->reg.coor, &irq_flag, 1);
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
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_device *ts_dev =  core_data->ts_dev;
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	u8 irq_flag = 0;
	int r;

	core_data->irq_trig_cnt++;

	if (core_data->tp_already_suspend) {
		ts_info("device in suspend, schedue to work");
		pm_wakeup_event(&core_data->pdev->dev, msecs_to_jiffies(300));
		queue_work(core_data->event_wq, &core_data->sleep_work);
		return IRQ_HANDLED;
	}

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

	/* clean irq flag */
	irq_flag = 0;
	ts_dev->hw_ops->write_trans(ts_dev, ts_dev->reg.coor, &irq_flag, 1);

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

	if (ts_bdata->avdd_name) {
		core_data->avdd = devm_regulator_get(dev, ts_bdata->avdd_name);
		if (IS_ERR_OR_NULL(core_data->avdd)) {
			r = PTR_ERR(core_data->avdd);
			ts_err("Failed to get regulator avdd:%d", r);
			core_data->avdd = NULL;
			return r;
		}
		core_data->avdd_load = ts_bdata->avdd_load;
	} else {
		ts_info("Avdd name is NULL");
	}

	if (ts_bdata->vddio_name) {
		core_data->vddio = devm_regulator_get(dev,
						      ts_bdata->vddio_name);
		if (IS_ERR_OR_NULL(core_data->vddio)) {
			r = PTR_ERR(core_data->vddio);
			ts_err("Failed to get regulator vddio:%d", r);
			core_data->vddio = NULL;
			return r;
		}
	} else {
		ts_info("vddio name is NULL");
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

	ts_err("enter::%s\n", __func__);
	if (core_data->power_on)
		return 0;

	if (core_data->avdd) {
		r = regulator_enable(core_data->avdd);
		if (!r) {
			/*ts_info("regulator enable avdd SUCCESS");*/
		} else {
			ts_err("Failed to enable avdd power:%d", r);
			return r;
		}
	}
	if (core_data->vddio) {
		r = regulator_enable(core_data->vddio);
		if (!r) {
			/*ts_info("regulator enable vddio SUCCESS");*/
		} else {
			ts_err("Failed to enable vddio power:%d", r);
			return r;
		}
	}

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
	/*struct goodix_ts_board_data *ts_bdata = board_data(core_data);*/
	int r = 0;

	ts_info("Device power off");
	if (!core_data->power_on)
		return 0;
	if (core_data->vddio) {
		r = regulator_disable(core_data->vddio);
		if (!r) {
			/*ts_info("regulator disable vddio SUCCESS");*/
		} else {
			ts_err("Failed to disable vddio power:%d", r);
			return r;
		}
	}

	if (core_data->avdd) {
		r = regulator_disable(core_data->avdd);
		if (!r) {
			/*ts_info("regulator disable avdd SUCCESS");*/
		} else {
			ts_err("Failed to disable avdd power:%d", r);
			return r;
		}

		if (core_data->avdd_load)
			regulator_set_load(core_data->avdd, 0);
	}

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

	ts_info("GPIO setup,reset-gpio:%d, irq-gpio:%d",
		ts_bdata->reset_gpio, ts_bdata->irq_gpio);

	if (ts_bdata->reset_gpio) {
		gpio_set_value(ts_bdata->reset_gpio, 0);
	}
	if (ts_bdata->irq_gpio) {
		gpio_direction_input(ts_bdata->irq_gpio);
	}

	return 0;
}

static int goodix_ts_gpio_request(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r = 0;

	ts_info("GPIO setup,reset-gpio:%d, irq-gpio:%d",
		ts_bdata->reset_gpio, ts_bdata->irq_gpio);
	/*
	 * after kenerl3.13, gpio_ api is deprecated, new
	 * driver should use gpiod_ api.
	 */
	r = devm_gpio_request(&core_data->pdev->dev, ts_bdata->reset_gpio,
				 "ts_reset_gpio");
	if (r < 0) {
		ts_err("Failed to request reset gpio, r:%d", r);
		return r;
	}

	r = devm_gpio_request(&core_data->pdev->dev, ts_bdata->irq_gpio,
				  "ts_irq_gpio");
	if (r < 0) {
		ts_err("Failed to request irq gpio, r:%d", r);
		return r;
	}
	if (ts_bdata->reset_gpio) {
		gpio_direction_output(ts_bdata->reset_gpio, 0);
	}
	if (ts_bdata->irq_gpio) {
		gpio_direction_output(ts_bdata->irq_gpio, 0);
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
	goodix_ts_set_input_params(input_dev, ts_bdata);

#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH,
			    INPUT_MT_DIRECT);
#else
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH);
#endif
#endif
	input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
	input_set_capability(input_dev, EV_KEY, BTN_INFO);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);

	input_set_capability(input_dev, EV_KEY, KEY_POWER);

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
	input_unregister_device(core_data->input_dev);
	input_free_device(core_data->input_dev);
	core_data->input_dev = NULL;
}

void goodix_ts_pen_dev_remove(struct goodix_ts_core *core_data)
{
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

void goodix_sync_setup(struct goodix_ts_core *core_data)
{
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	u8 sync_enable_data[5] = {0x30, 0x02, 0x01, 0x00, 0x33};
	u8 sync_base_update[5] = {0x03, 0x00, 0x00, 0x00, 0x03};
	u8 sync_try_time = 3;

	ts_info("%s start sync setup", __func__);
	sync_try_time = 1;
	while (sync_try_time--) {
		ts_dev->hw_ops->write(core_data->ts_dev, 0x4160, sync_enable_data, 5);
		msleep(150);
		ts_dev->hw_ops->write(core_data->ts_dev, 0x4160, sync_base_update, 5);
	}
}

void goodix_USB_is_exist(struct goodix_ts_core *core_data)
{
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	u8 usb_enable_data[5] = {0x06, 0x00, 0x00, 0x00, 0x06};
	u8 usb_try_time = 3;

	ts_info("%s usb insert setup", __func__);
	usb_try_time = 1;
	while (usb_try_time--) {
		ts_dev->hw_ops->write(core_data->ts_dev, 0x4160, usb_enable_data, 5);
	}
}
void goodix_USB_is_not_exist(struct goodix_ts_core *core_data)
{
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	u8 usb_unplug_data[5] = {0x07, 0x00, 0x00, 0x00, 0x07};
	u8 usb_try_time = 3;

	ts_info("%s usb unplug setup", __func__);
	usb_try_time = 1;
	while (usb_try_time--) {
		ts_dev->hw_ops->write(core_data->ts_dev, 0x4160, usb_unplug_data, 5);
	}
}

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
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->before_suspend)
				continue;

			r = ext_module->funcs->before_suspend(core_data,
							      ext_module);
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
				if (!atomic_read(&core_data->suspend_stat))
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
#if 0
#ifdef CONFIG_PINCTRL
	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
				core_data->pin_sta_suspend);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif
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
#ifdef CONFIG_FACTORY_BUILD
	goodix_ts_power_off(core_data);
#endif
out:
	goodix_ts_release_connects(core_data);
	core_data->ts_event.touch_data.touch_num = 0;

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
static int goodix_ts_resume(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_device *ts_dev =
				core_data->ts_dev;
	int r;

	ts_info("Resume start");

	/*goodix_ts_release_connects(core_data);*/
	mutex_lock(&core_data->work_stat);
	if (!atomic_read(&core_data->suspend_stat)) {
		ts_info("resumed, skip");
		/*goodix_ts_irq_enable(core_data, true);*/
		goto out;
	}
#ifdef CONFIG_FACTORY_BUILD
	goodix_ts_power_on(core_data);
#endif
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
#if 0
#ifdef CONFIG_PINCTRL
	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
					 core_data->pin_sta_active);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif
#endif
	atomic_set(&core_data->suspended, 0);
	/* resume device */
	if (ts_dev && ts_dev->hw_ops->resume)
		ts_dev->hw_ops->resume(ts_dev);

	atomic_set(&core_data->suspend_stat, TP_NO_SUSPEND);

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

	/*
	 * notify resume event, inform the esd protector
	 * and charger detector to turn on the work
	 */
	ts_info("try notify resume");
	goodix_ts_blocking_notify(NOTIFY_RESUME, NULL);
out:

	ts_err("core_data->fod_pressed = %d\n", core_data->fod_pressed);

	if (!core_data->fod_pressed) {
		ts_err("resume release all touch");
		goodix_ts_release_connects(core_data);
	}

	core_data->sleep_finger = 0;

	goodix_sync_setup(core_data);
	sysfs_notify(&core_data->gtp_touch_dev->kobj, NULL,
		     "touch_suspend_notify");

	mutex_unlock(&core_data->work_stat);

	ts_info("Resume end");
	return 0;
}

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
 * goodix_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
int goodix_ts_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct goodix_ts_core *core_data =
		container_of(self, struct goodix_ts_core, fb_notifier);
	struct mi_drm_notifier *fb_event = data;
	int blank;

	if (fb_event && fb_event->data && core_data) {
		blank = *(int *)(fb_event->data);
		flush_workqueue(core_data->event_wq);
		if (event == MI_DRM_EVENT_BLANK && blank == MI_DRM_BLANK_UNBLANK) {
			ts_info("touchpanel resume");
			queue_work(core_data->event_wq, &core_data->resume_work);
		} else if (event == MI_DRM_EVENT_BLANK && (blank == MI_DRM_BLANK_POWERDOWN ||
			blank == MI_DRM_BLANK_LP1 || blank == MI_DRM_BLANK_LP2)) {
			ts_info("touchpanel suspend by %s", blank == MI_DRM_BLANK_POWERDOWN ? "blank" : "doze");
			queue_work(core_data->event_wq, &core_data->suspend_work);
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
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);
	ts_info("enter");

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
	ts_info("enter");

	if (device_may_wakeup(dev) && core_data->gesture_enabled) {
		disable_irq_wake(core_data->irq);
	}
	core_data->tp_already_suspend = false;
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
	struct goodix_ts_core *ts_core = container_of(self,
			struct goodix_ts_core, ts_notifier);
	struct goodix_ts_device *ts_dev = ts_device(ts_core);
	const struct goodix_ts_hw_ops *hw_ops = ts_hw_ops(ts_core);
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

#ifdef CONFIG_DRM
	core_data->fb_notifier.notifier_call = goodix_ts_fb_notifier_callback;
	if (mi_drm_register_client(&core_data->fb_notifier))
		ts_err("Failed to register fb notifier client:%d", r);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	core_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	core_data->early_suspend.resume = goodix_ts_lateresume;
	core_data->early_suspend.suspend = goodix_ts_earlysuspend;
	register_early_suspend(&core_data->early_suspend);
#endif
	/*create sysfs files*/
	goodix_ts_sysfs_init(core_data);

	/* esd protector */
	goodix_ts_esd_init(core_data);
	return 0;
exit:
	if (ts_dev->board_data.pen_enable) {
		goodix_ts_pen_dev_remove(core_data);
	}
err_finger:
	goodix_ts_input_dev_remove(core_data);
	return r;
}

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

static ssize_t goodix_rawdata_info_read(struct file *file, char __user *buf,
					 size_t count, loff_t *pos)
{
	char *k_buf = NULL;
	int ret = 0;
	int cnt = 0;

	if (*pos != 0 || !goodix_core_data)
		return 0;
	k_buf = kzalloc(PAGE_SIZE * sizeof(char), GFP_KERNEL);
	if (k_buf == NULL) {
		ts_err("get memory to save raw data failed!");
		return 0;
	}
	ret = goodix_tools_register();
	if (ret) {
		ts_err("tp_rawdata prepare goodix_tools_register failed");
		cnt = snprintf(k_buf, 6, "-EIO\t\n");
		goto out;
	}
	ts_info("start get rawdata!");
	get_tp_rawdata(goodix_core_data->gtp_touch_dev, k_buf, &cnt);
	goodix_tools_unregister();

out:
	cnt = cnt > count ? count : cnt;
	ret = copy_to_user(buf, k_buf, cnt);
	*pos += cnt;
	if (k_buf) {
		kfree(k_buf);
		k_buf = NULL;
	}
	ts_info("get rawdata test finish!");
	if (ret != 0)
		return 0;
	else
		return cnt;

	return ret;
}

static const struct file_operations goodix_rawdata_info_ops = {
	.read = goodix_rawdata_info_read,
};

static ssize_t goodix_fw_version_info_read(struct file *file, char __user *buf,
					 size_t count, loff_t *pos)
{
	struct goodix_ts_version fw_ver;
	char k_buf[100] = {0};
	int cnt = 0;
	char str[5];
	int r = 0;

	if (*pos != 0 || !goodix_core_data)
		return 0;
	r = goodix_core_data->ts_dev->hw_ops->read_version(goodix_core_data->ts_dev, &fw_ver);
	if (r)
		return 0;
	memcpy(str, fw_ver.pid, 4);
	str[4] = '\0';
	cnt = snprintf(k_buf, 100,
			 "PID:%s VID:%02x %02x %02x %02x SENSOR_ID:%d\n",
			 str, fw_ver.vid[0], fw_ver.vid[1],
			 fw_ver.vid[2], fw_ver.vid[3],
			 fw_ver.sensor_id);
	cnt = cnt > count ? count : cnt;
	r = copy_to_user(buf, k_buf, cnt);
	*pos += cnt;
	if (r != 0)
		return 0;
	else
		return cnt;
}


static const struct file_operations goodix_fw_version_info_ops = {
	.read = goodix_fw_version_info_read,
};

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

static struct attribute *goodix_attr_group[] = {
	&dev_attr_lockdown_info.attr,
	NULL,
};

static int gtp_i2c_test(void)
{
	int ret = 0;
	u8 read_val = 0;
	struct goodix_ts_device *ts_device;

	ts_device = goodix_core_data->ts_dev;

	ret = ts_device->hw_ops->read_trans(ts_device, 0x3100, &read_val, 1);
	if (!ret) {
		ts_info("i2c test SUCCESS");
		return GTP_RESULT_PASS;
	} else {
		ts_err("i2c test FAILED");
		return GTP_RESULT_FAIL;
	}
}

static ssize_t gtp_selftest_read(struct file *file, char __user *buf,
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

static int gtp_short_open_test(void)
{
	int ret = 0;

	ret = goodix_tools_register();

	if (ret) {
		ts_err("tp_test prepare goodix_tools_register failed");
		return GTP_RESULT_INVALID;
	}
	ts_info("test start!");
	if (ret == 0) {
		ts_err("test PASS!");
		ret = GTP_RESULT_PASS;
	} else {
		ts_err("test FAILED. result:%x", ret);
		ret = GTP_RESULT_FAIL;
	}
	goodix_tools_unregister();
	ts_info("test finish!");
	return ret;
}

static ssize_t gtp_selftest_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
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


static void goodix_power_supply_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
	    container_of(work, struct goodix_ts_core, power_supply_work);
	/*struct goodix_ts_device *dev = core_data->ts_dev;*/
	/*u8 state_data[3];*/
	int is_usb_exist = 0;

	if (!atomic_read(&core_data->suspend_stat) && goodix_modules.core_data->initialized) {
		is_usb_exist = !!power_supply_is_system_supplied();
		if (is_usb_exist != core_data->is_usb_exist
		    || core_data->is_usb_exist < 0) {
			core_data->is_usb_exist = is_usb_exist;
			ts_info("Power_supply_event:%d", is_usb_exist);
			if (is_usb_exist) {
				goodix_USB_is_exist(core_data);
				ts_info("USB is exist");
			} else {
				goodix_USB_is_not_exist(core_data);
				ts_info("USB is not exist");
			}
		}
	}
}

static int goodix_power_supply_event(struct notifier_block *nb,
				  unsigned long event, void *ptr)
{
	struct goodix_ts_core *ts_core =
	    container_of(nb, struct goodix_ts_core, power_supply_notifier);

	if (ts_core)
		queue_work(ts_core->event_wq, &ts_core->power_supply_work);

	return 0;
}

static ssize_t goodix_touch_suspend_notify_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
			!!atomic_read(&goodix_core_data->suspend_stat));
}

static ssize_t goodix_fod_test_store(struct device *dev,
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

static ssize_t goodix_fod_status_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);

	return snprintf(buf, 10, "%d\n", core_data->fod_status);
}

static DEVICE_ATTR(fod_status, (S_IRUGO | S_IRGRP),
			goodix_fod_status_show, NULL);


static DEVICE_ATTR(fod_test, (S_IRUGO | S_IWUSR | S_IWGRP),
		NULL, goodix_fod_test_store);

static DEVICE_ATTR(touch_suspend_notify, (S_IRUGO | S_IRGRP),
		goodix_touch_suspend_notify_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;

static void gtp_aod_set_work(struct work_struct *work)
{
	goodix_check_gesture_stat(!!goodix_core_data->aod_status);
}

static void gtp_fod_set_work(struct work_struct *work)
{
	goodix_check_gesture_stat(!!goodix_core_data->fod_status);
}

static void gtp_set_cur_value_work(struct work_struct *work)
{
	struct goodix_ts_device *dev = goodix_core_data->ts_dev;
	u8 state_data[5] = {0};
	u8 goodix_game_value = 0;
	u8 temp_value = 0;
	unsigned int check_sum = 0;
	int ret = 0;
	int i = 0;

	if (atomic_read(&goodix_core_data->suspend_stat) == TP_SLEEP) {
		ts_info("suspended, skip");
		return;
	}

	if (!xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE]) {
		ts_info("exit touch game mode");
		state_data[0] = GTP_EXIT_GAME_CMD;
		state_data[1] = 0x00;
		state_data[2] = 0x00;
		check_sum = state_data[0] + state_data[1] + state_data[2];
		state_data[3] = (check_sum >> 8) & 0xFF;
		state_data[4] = check_sum & 0xFF;
		ts_info("write state_data[0]=0x%02X, state_data[1]=0x%02X, state_data[2]=0x%02X, state_data[3]=0x%02X, state_data[4]=0x%02X",
			state_data[0], state_data[1], state_data[2],
			state_data[3], state_data[4]);
		ret = goodix_i2c_write(dev, GTP_GAME_CMD_ADD, state_data, 5);
		if (ret < 0) {
			ts_err("exit game mode fail");
		}
		goodix_game_value = 0;
		return;
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
				temp_value =
					xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
				if (PANEL_ORIENTATION_DEGREE_90 == temp_value)
					temp_value = 1;
				else if (PANEL_ORIENTATION_DEGREE_270 == temp_value)
					temp_value = 2;
 				else
					temp_value = 0;
				goodix_game_value &= 0x3F;
				goodix_game_value |= (temp_value << 6);
				break;
		default:
				/* Don't support */
				break;

		};
	}
	ts_info("write value:0x%x", goodix_game_value);

	if (xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE]) {
		state_data[0] = GTP_GAME_CMD;
		state_data[1] = goodix_game_value;
		state_data[2] = 0x00;
		check_sum = state_data[0] + state_data[1] + state_data[2];
		state_data[3] = (check_sum >> 8) & 0xFF;
		state_data[4] = check_sum & 0xFF;
		ts_info("write state_data[0]=0x%02X, state_data[1]=0x%02X, state_data[2]=0x%02X, state_data[3]=0x%02X, state_data[4]=0x%02X",
			state_data[0], state_data[1], state_data[2],
			state_data[3], state_data[4]);
		ret = goodix_i2c_write(dev, GTP_GAME_CMD_ADD, state_data, 5);

		if (ret < 0) {
			ts_err("change game mode fail");
		}
	}
	return;
}

static int gtp_set_cur_value(int gtp_mode, int gtp_value)
{
	int ret = 0;

	ts_info("mode:%d, value:%d", gtp_mode, gtp_value);
	if (gtp_mode == Touch_Fod_Enable && goodix_core_data && gtp_value >= 0) {
		ts_info("set fod status:%d", gtp_value);
		goodix_core_data->fod_status = gtp_value;
		if (goodix_core_data->fod_status == -1 || goodix_core_data->fod_status == 100)
			goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup | goodix_core_data->aod_status;
		else
			goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup |
				goodix_core_data->fod_status | goodix_core_data->aod_status;

		queue_work(goodix_core_data->touch_feature_wq, &goodix_core_data->fod_set_work);
		return 0;
	}
	if (gtp_mode == Touch_Aod_Enable && goodix_core_data && gtp_value >= 0) {
		ts_info("set aod status %d", gtp_value);
		goodix_core_data->aod_status = gtp_value;
		if (goodix_core_data->fod_status == -1 || goodix_core_data->fod_status == 100)
			goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup | goodix_core_data->aod_status;
		else
			goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup |
				goodix_core_data->fod_status | 	goodix_core_data->aod_status;
		queue_work(goodix_core_data->touch_feature_wq, &goodix_core_data->aod_set_work);
		return 0;
	}
	if (gtp_mode == Touch_Doubletap_Mode && goodix_core_data && gtp_value >= 0) {
		ts_info("set double tap %d", gtp_value);
		goodix_core_data->double_wakeup = gtp_value;
		if (goodix_core_data->fod_status == -1 || goodix_core_data->fod_status == 100)
			goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup | goodix_core_data->aod_status;
		else
			goodix_core_data->gesture_enabled = goodix_core_data->double_wakeup |
				goodix_core_data->fod_status | 	goodix_core_data->aod_status;
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

	queue_work(goodix_core_data->touch_feature_wq, &goodix_core_data->cmd_update_work);

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

	ts_info("mode:%d", mode);
	if (mode < Touch_Mode_NUM && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		queue_work(goodix_core_data->touch_feature_wq, &goodix_core_data->cmd_update_work);
	} else if (mode == 0) {
		for (i = 0; i < Touch_Mode_NUM; i++) {
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
		}
		queue_work(goodix_core_data->touch_feature_wq, &goodix_core_data->cmd_update_work);
	} else {
		ts_err("don't support");
	}

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

int goodix_palm_sensor_write(int value)
{
	int ret = 0;
	goodix_core_data->palm_sensor_switch = !!value;

	return ret;
}

static u8 goodix_panel_color_read()
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[2];
}

static u8 goodix_panel_vendor_read()
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[0];
}

static u8 goodix_panel_display_read()
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[1];
}

static char goodix_touch_vendor_read(void)
{
	return '2';
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

	ts_info("goodix_ts_probe IN");

	ts_device = pdev->dev.platform_data;
	if (!ts_device || !ts_device->hw_ops) {
		ts_err("Invalid touch device");
		return -ENODEV;
	}

	core_data = kzalloc(sizeof(struct goodix_ts_core),
				 GFP_KERNEL);
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

	r = goodix_ts_gpio_request(core_data);
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
	} else {
		ts_err("Failed to get pinctrl");
		goto out;
	}
#endif

	r = goodix_ts_power_init(core_data);
	if (r < 0)
		goto out;

	r = goodix_ts_power_on(core_data);
	if (r < 0)
		goto out;

	/* get GPIO resource */
	r = goodix_ts_gpio_setup(core_data);
	if (r < 0)
		goto out;

	/*init lock to protect suspend_stat*/
	mutex_init(&core_data->work_stat);
	mutex_init(&ts_device->report_mutex);
	/*init complete and pm status*/
	core_data->tp_already_suspend = false;
	init_completion(&core_data->pm_resume_completion);

	/* confirm it's goodix touch dev or not */
	r = ts_device->hw_ops->dev_confirm(ts_device);
	if (r) {
		ts_err("goodix device confirm failed[skip]");
		goto out;
	}
	msleep(100);

	/* Try start a thread to get config-bin info */
	r = goodix_start_later_init(core_data);
	if (r) {
		ts_info("Failed start cfg_bin_proc");
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

	core_data->power_supply_notifier.notifier_call = goodix_power_supply_event;
	power_supply_reg_notifier(&core_data->power_supply_notifier);
	INIT_WORK(&core_data->power_supply_work, goodix_power_supply_work);

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&core_data->ts_notifier);

	core_data->tp_lockdown_info_proc =
		proc_create("tp_lockdown_info", 0664, NULL, &goodix_lockdown_info_ops);
	core_data->tp_data_dump_proc =
		proc_create("tp_data_dump", 0444, NULL, &goodix_rawdata_info_ops);
	core_data->tp_fw_version_proc =
		proc_create("tp_fw_version", 0444, NULL, &goodix_fw_version_info_ops);

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
	core_data->gtp_touch_dev = get_xiaomi_touch_dev();
	if (core_data->gtp_touch_dev == NULL)
			ts_err("ERROR: gtp_touch_dev is NULL!\n");
#else
	core_data->gtp_tp_class = class_create(THIS_MODULE, "touch");
	core_data->gtp_touch_dev = device_create(core_data->gtp_tp_class, NULL, 0x5d, core_data, "touch_dev");
#endif
	if (IS_ERR(core_data->gtp_touch_dev)) {
		ts_err("ERROR: Cannot create device for the sysfs!\n");
		r = -ENOMEM;
		goto out;
	}

	if (sysfs_create_file(&core_data->gtp_touch_dev->kobj,
			&dev_attr_touch_suspend_notify.attr)) {
		ts_err("Failed to create sysfs group!\n");
		r = -ENOMEM;
		goto out;
	}

	if (sysfs_create_file(&core_data->gtp_touch_dev->kobj,
				&dev_attr_fod_status.attr)) {
		ts_err("Failed to create fod_status sysfs group!\n");
		r = -ENOMEM;
		goto out;
	}

	if (sysfs_create_file(&core_data->gtp_touch_dev->kobj,
				  &dev_attr_fod_test.attr)) {
		ts_err("Failed to create fod_test sysfs group!");
		r = -ENOMEM;
		goto out;
	}

	core_data->tp_selftest_proc =
		proc_create("tp_selftest", 0644, NULL, &gtp_selftest_ops);


	core_data->fod_status = -1;

	core_data->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (core_data->debugfs) {
		debugfs_create_file("switch_state", 0660, core_data->debugfs, core_data,
					&tpdbg_operations);
	}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
		core_data->touch_feature_wq =
			alloc_workqueue("gtp-touch-feature",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
		if (!core_data->touch_feature_wq) {
			ts_info("Cannot create touch feature work thread");
			r = -ENOMEM;
			goto out;
		}
		INIT_WORK(&core_data->cmd_update_work, gtp_set_cur_value_work);
		INIT_WORK(&core_data->aod_set_work, gtp_aod_set_work);
		INIT_WORK(&core_data->fod_set_work, gtp_fod_set_work);
		memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
		xiaomi_touch_interfaces.getModeValue = gtp_get_mode_value;
		xiaomi_touch_interfaces.setModeValue = gtp_set_cur_value;
		xiaomi_touch_interfaces.resetMode = gtp_reset_mode;
		xiaomi_touch_interfaces.getModeAll = gtp_get_mode_all;
		xiaomi_touch_interfaces.palm_sensor_write = goodix_palm_sensor_write;
		xiaomi_touch_interfaces.panel_display_read = goodix_panel_display_read;
		xiaomi_touch_interfaces.panel_vendor_read = goodix_panel_vendor_read;
		xiaomi_touch_interfaces.panel_color_read = goodix_panel_color_read;
		xiaomi_touch_interfaces.touch_vendor_read = goodix_touch_vendor_read;

		xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
		gtp_init_touchmode_data();
#endif
	core_data->fod_status = 0;

#ifdef CONFIG_FACTORY_BUILD
	core_data->fod_status = 1;
#endif

out:
	if (r) {
		if (core_data->pinctrl) {
			pinctrl_select_state(core_data->pinctrl,
					 core_data->pin_sta_suspend);
			devm_pinctrl_put(core_data->pinctrl);
		}
		core_data->pinctrl = NULL;
		if (ts_device->board_data.reset_gpio) {
			gpio_set_value(ts_device->board_data.reset_gpio, 0);
		}

		if (ts_device->board_data.irq_gpio) {
			gpio_direction_output(ts_device->board_data.irq_gpio, 0);
		}
		goodix_ts_power_off(core_data);
		if (core_data->fb_notifier.notifier_call)
			mi_drm_unregister_client(&core_data->fb_notifier);
		if (core_data->power_supply_notifier.notifier_call)
			power_supply_unreg_notifier(&core_data->power_supply_notifier);
		core_data->initialized = 0;
	} else
		core_data->initialized = 1;

	goodix_modules.core_data = core_data;
	ts_info("goodix_ts_probe OUT, r:%d", r);
	/* wakeup ext module register work */
	complete_all(&goodix_modules.core_comp);
	return r;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);

	core_data->initialized = 0;
	if (atomic_read(&core_data->ts_esd.esd_on))
		goodix_ts_esd_off(core_data);
#ifdef CONFIG_DRM
	mi_drm_unregister_client(&core_data->fb_notifier);
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	destroy_workqueue(core_data->touch_feature_wq);
#endif
	power_supply_unreg_notifier(&core_data->power_supply_notifier);
	goodix_remove_all_ext_modules();
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

int goodix_ts_core_init(void)
{
	ts_info("Core layer init");
	if (!goodix_modules.initilized) {
		/* this may init by outer modules register event */
		ts_info("init modules struct");
		goodix_modules.initilized = true;
		INIT_LIST_HEAD(&goodix_modules.head);
		mutex_init(&goodix_modules.mutex);
		init_completion(&goodix_modules.core_comp);
	}

	goodix_debugfs_init();
	return platform_driver_register(&goodix_ts_driver);
}

/* uninit module manually */
int goodix_ts_core_release(struct goodix_ts_core *core_data)
{
	ts_info("goodix core module removed");

	platform_driver_unregister(&goodix_ts_driver);
	goodix_ts_dev_release();
	return 0;
}
