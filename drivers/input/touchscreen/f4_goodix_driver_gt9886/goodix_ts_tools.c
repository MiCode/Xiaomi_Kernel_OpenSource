/*
 * Goodix GTX5 tools Dirver
 *
 * Copyright (C) 2015 - 2016 Goodix, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Authors:  Wang Yafei <wangyafei@goodix.com>
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include "goodix_ts_core.h"

#define GOODIX_TOOLS_NAME		"gtp_tools"
#define GOODIX_TS_IOC_MAGIC		'G'
#define NEGLECT_SIZE_MASK		(~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

#define GTP_IRQ_ENABLE	_IO(GOODIX_TS_IOC_MAGIC, 0)
#define GTP_DEV_RESET	_IO(GOODIX_TS_IOC_MAGIC, 1)
#define GTP_SEND_COMMAND (_IOW(GOODIX_TS_IOC_MAGIC, 2, u8) & NEGLECT_SIZE_MASK)
#define GTP_SEND_CONFIG	(_IOW(GOODIX_TS_IOC_MAGIC, 3, u8) & NEGLECT_SIZE_MASK)
#define GTP_ASYNC_READ	(_IOR(GOODIX_TS_IOC_MAGIC, 4, u8) & NEGLECT_SIZE_MASK)
#define GTP_SYNC_READ	(_IOR(GOODIX_TS_IOC_MAGIC, 5, u8) & NEGLECT_SIZE_MASK)
#define GTP_ASYNC_WRITE	(_IOW(GOODIX_TS_IOC_MAGIC, 6, u8) & NEGLECT_SIZE_MASK)
#define GTP_READ_CONFIG	(_IOW(GOODIX_TS_IOC_MAGIC, 7, u8) & NEGLECT_SIZE_MASK)
#define GTP_ESD_ENABLE	_IO(GOODIX_TS_IOC_MAGIC, 8)
#define GTP_DRV_VERSION     (_IOR(GOODIX_TS_IOC_MAGIC, 9, u8) & NEGLECT_SIZE_MASK)

#define GOODIX_TS_IOC_MAXNR		10

#define IRQ_FALG	(0x01 << 2)

#define I2C_MSG_HEAD_LEN	20
#define TS_REG_COORDS_BASE	0x4100

/*
 * struct goodix_tools_data - goodix tools data message used in sync read
 * @data: The buffer into which data is written
 * @reg_addr: Slave device register start address to start read data
 * @length: Number of data bytes in @data being read from slave device
 * @filled: When buffer @data be filled will set this flag with 1, outhrwise 0
 * @list_head:Eonnet every goodix_tools_data struct into a list
*/

struct goodix_tools_data {
	u32 reg_addr;
	u32 length;
	u8 *data;
	bool filled;
	struct list_head list;
};

/*
 * struct goodix_tools_dev - goodix tools device struct
 * @ts_core: The core data struct of ts driver
 * @ops_mode: represent device work mode
 * @rawdiffcmd: Set slave device into rawdata mode
 * @normalcmd: Set slave device into normal mode
 * @wq: Wait queue struct use in synchronous data read
 * @mutex: Protect goodix_tools_dev
*/
struct goodix_tools_dev {
	struct goodix_ts_core *ts_core;
	struct list_head head;
	unsigned int ops_mode;
	struct goodix_ts_cmd rawdiffcmd, normalcmd;
	wait_queue_head_t wq;
	struct mutex mutex;
	atomic_t t_count;
	struct goodix_ext_module module;
} *goodix_tools_dev;

/* read data from i2c asynchronous,
** success return bytes read, else return <= 0
*/
static int async_read(struct goodix_tools_dev *dev, void __user *arg)
{
	u8 *databuf = NULL;
	int ret = 0;
	u32 reg_addr, length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];
	struct goodix_ts_device *ts_dev = dev->ts_core->ts_dev;
	const struct goodix_ts_hw_ops *hw_ops = ts_dev->hw_ops;

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ret = -EFAULT;
		goto err_out;
	}
	reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
	    + (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
	    + (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);

	databuf = kzalloc(length, GFP_KERNEL);
	if (!databuf) {
		ts_err("Alloc memory failed");
		return -ENOMEM;
	}

	if (!hw_ops->read_trans(ts_dev, reg_addr, databuf, length)) {
		if (copy_to_user
		    ((u8 *) arg + I2C_MSG_HEAD_LEN, databuf, length)) {
			ret = -EFAULT;
			ts_err("Copy_to_user failed");
		} else {
			ret = length;
		}
	} else {
		ret = -EBUSY;
		ts_err("Read i2c failed");
	}
err_out:
	kfree(databuf);
	return ret;
}

/* if success return config data length */
static int read_config_data(struct goodix_ts_device *ts_dev, void __user *arg)
{
	int ret = 0;
	u32 reg_addr, length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];
	u8 *tmp_buf;

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}
	reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
	    + (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
	    + (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);
	ts_info("read config,reg_addr=0x%x, length=%d", reg_addr, length);
	tmp_buf = kzalloc(length, GFP_KERNEL);
	if (!tmp_buf) {
		ts_err("failed alloc memory");
		return -ENOMEM;
	}
	/* if reg_addr == 0, read config data with specific flow */
	if (!reg_addr) {
		if (ts_dev->hw_ops->read_config)
			ret = ts_dev->hw_ops->read_config(ts_dev, tmp_buf, 0);
		else
			ret = -EINVAL;
	} else {
		ret =
		    ts_dev->hw_ops->read_trans(ts_dev, reg_addr, tmp_buf,
					       length);
		if (!ret)
			ret = length;
	}
	if (ret <= 0)
		goto err_out;

	if (copy_to_user((u8 *) arg + I2C_MSG_HEAD_LEN, tmp_buf, ret)) {
		ret = -EFAULT;
		ts_err("Copy_to_user failed");
	}

err_out:
	kfree(tmp_buf);
	return ret;
}

/* read data from i2c synchronous,
** success return bytes read, else return <= 0
*/
static int sync_read(struct goodix_tools_dev *dev, void __user *arg)
{
	int ret = 0;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];
	struct goodix_tools_data tools_data;

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}
	tools_data.reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
	    + (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	tools_data.length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
	    + (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);
	tools_data.filled = 0;

	tools_data.data = kzalloc(tools_data.length, GFP_KERNEL);
	if (!tools_data.data) {
		ts_err("Alloc memory failed");
		return -ENOMEM;
	}

	mutex_lock(&dev->mutex);
	list_add_tail(&tools_data.list, &dev->head);
	mutex_unlock(&dev->mutex);
	/* wait queue will timeout after 1 seconds */
	wait_event_interruptible_timeout(dev->wq, tools_data.filled == 1,
					 HZ * 3);

	mutex_lock(&dev->mutex);
	list_del(&tools_data.list);
	mutex_unlock(&dev->mutex);
	if (tools_data.filled == 1) {
		if (copy_to_user((u8 *) arg + I2C_MSG_HEAD_LEN, tools_data.data,
				 tools_data.length)) {
			ret = -EFAULT;
			ts_err("Copy_to_user failed");
		} else {
			ret = tools_data.length;
		}
	} else {
		ret = -EAGAIN;
		ts_err("Wait queue timeout");
	}

	kfree(tools_data.data);
	return ret;
}

/* write data to i2c asynchronous,
** success return bytes write, else return <= 0
*/
static int async_write(struct goodix_tools_dev *dev, void __user *arg)
{
	u8 *databuf;
	int ret = 0;
	u32 reg_addr, length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];
	struct goodix_ts_device *ts_dev = dev->ts_core->ts_dev;
	const struct goodix_ts_hw_ops *hw_ops = ts_dev->hw_ops;

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}
	reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
	    + (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
	    + (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);

	databuf = kzalloc(length, GFP_KERNEL);
	if (!databuf) {
		ts_err("Alloc memory failed");
		return -ENOMEM;
	}
	ret = copy_from_user(databuf, (u8 *) arg + I2C_MSG_HEAD_LEN, length);
	if (ret) {
		ret = -EFAULT;
		ts_err("Copy data from user failed");
		goto err_out;
	}

	if (hw_ops->write_trans(ts_dev, reg_addr, databuf, length)) {
		ret = -EBUSY;
		ts_err("Write data to device failed");
	} else {
		ret = length;
	}

err_out:
	kfree(databuf);
	return ret;
}

/**

 * sync_read_rawdata - read device register through i2c bus

 * @dev: pointer to device data

 * @addr: register address

 * @data: read buffer

 * @len: bytes to read

 * return: 0 - read ok, < 0 - i2c transter error

*/

int sync_read_rawdata(unsigned int reg, unsigned char *data, unsigned int len)
{

	int ret = 0;

	struct goodix_tools_dev *dev = goodix_tools_dev;

	struct goodix_tools_data tools_data;

	tools_data.reg_addr = reg;

	tools_data.length = len;

	tools_data.filled = 0;

	tools_data.data = kzalloc(tools_data.length, GFP_KERNEL);

	if (!tools_data.data) {

		ts_err("Alloc memory failed");

		return -ENOMEM;

	}

	mutex_lock(&dev->mutex);

	list_add_tail(&tools_data.list, &dev->head);

	ts_info("add sync_read to list");

	mutex_unlock(&dev->mutex);

	/* wait queue will timeout after 1 seconds */

	wait_event_interruptible_timeout(dev->wq, tools_data.filled == 1,
					 HZ * 3);

	mutex_lock(&dev->mutex);

	list_del(&tools_data.list);

	mutex_unlock(&dev->mutex);

	if (tools_data.filled == 1) {

		memcpy(data, tools_data.data, tools_data.length);

	} else {

		ret = -EAGAIN;

		ts_err("Wait queue timeout");

	}

	kfree(tools_data.data);

	return ret;

}

int goodix_tools_register(void)
{

	int ret = 0;

	ts_info("register tools module");

	/* Only the first time open device need to register module */

	ret = goodix_register_ext_module(&goodix_tools_dev->module);

	if (ret) {

		ts_info("failed register to core module");

	}

	return ret;
}

int goodix_tools_unregister(void)
{

	int ret = 0;

	/* when the last close this dev node unregister the module */

	ret = goodix_unregister_ext_module(&goodix_tools_dev->module);

	return ret;

}

static int init_cfg_data(struct goodix_ts_config *cfg, void __user *arg)
{
	int ret = 0;
	u32 reg_addr, length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];

	cfg->initialized = 0;
	mutex_init(&cfg->lock);
	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}

	reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
	    + (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
	    + (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);

	ret = copy_from_user(cfg->data, (u8 *) arg + I2C_MSG_HEAD_LEN, length);
	if (ret) {
		ret = -EFAULT;
		ts_err("Copy data from user failed");
		goto err_out;
	}
	cfg->reg_base = reg_addr;
	cfg->length = length;
	strlcpy(cfg->name, "tools-send-cfg", sizeof(cfg->name));
	cfg->delay = 50;
	cfg->initialized = true;
	return 0;

err_out:
	return ret;
}

/**
 * goodix_tools_ioctl - ioctl implementation
 *
 * @filp: Pointer to file opened
 * @cmd: Ioctl opertion command
 * @arg: Command data
 * Returns >=0 - succeed, else failed
 */
static long goodix_tools_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	int ret = 0;
	struct goodix_tools_dev *dev = filp->private_data;
	struct goodix_ts_device *ts_dev;
	const struct goodix_ts_hw_ops *hw_ops;
	struct goodix_ts_cmd temp_cmd;
	struct goodix_ts_config *temp_cfg = NULL;

	if (dev->ts_core == NULL) {
		ts_err("Tools module not register");
		return -EINVAL;
	}
	ts_dev = dev->ts_core->ts_dev;
	hw_ops = ts_dev->hw_ops;

	if (_IOC_TYPE(cmd) != GOODIX_TS_IOC_MAGIC) {
		ts_err("Bad magic num:%c", _IOC_TYPE(cmd));
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > GOODIX_TS_IOC_MAXNR) {
		ts_err("Bad cmd num:%d > %d",
		       _IOC_NR(cmd), GOODIX_TS_IOC_MAXNR);
		return -ENOTTY;
	}

	switch (cmd & NEGLECT_SIZE_MASK) {
	case GTP_IRQ_ENABLE:
		if (arg == 1) {
			goodix_ts_irq_enable(dev->ts_core, true);
			mutex_lock(&dev->mutex);
			dev->ops_mode |= IRQ_FALG;
			mutex_unlock(&dev->mutex);
			ts_info("IRQ enabled");
		} else if (arg == 0) {
			goodix_ts_irq_enable(dev->ts_core, false);
			mutex_lock(&dev->mutex);
			dev->ops_mode &= ~IRQ_FALG;
			mutex_unlock(&dev->mutex);
			ts_info("IRQ disabled");
		} else {
			ts_info("Irq aready set with, arg = %ld", arg);
		}
		ret = 0;
		break;
	case GTP_ESD_ENABLE:
		if (arg == 0)
			goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
		else
			goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
		break;
	case GTP_DEV_RESET:
		hw_ops->reset(ts_dev);
		break;
	case GTP_SEND_COMMAND:
		ret = copy_from_user(&temp_cmd, (void __user *)arg,
				     sizeof(struct goodix_ts_cmd));
		if (ret) {
			ret = -EINVAL;
			goto err_out;
		}

		ret = hw_ops->send_cmd(ts_dev, &temp_cmd);
		if (ret) {
			ts_err("Send command failed");
			ret = -EAGAIN;
		}
		break;
	case GTP_SEND_CONFIG:
		temp_cfg = kzalloc(sizeof(struct goodix_ts_config), GFP_KERNEL);
		if (temp_cfg == NULL) {
			ts_err("Memory allco err");
			ret = -ENOMEM;
			goto err_out;
		}

		ret = init_cfg_data(temp_cfg, (void __user *)arg);

		if (!ret && hw_ops->send_config) {
			ret = hw_ops->send_config(ts_dev, temp_cfg);
			if (ret) {
				ts_err("Failed send config");
				ret = -EAGAIN;
			} else {
				ts_info("Send config success");
				ret = 0;
			}
		}
		break;
	case GTP_READ_CONFIG:
		ret = read_config_data(ts_dev, (void __user *)arg);
		if (ret > 0)
			ts_info("success read config:len=%d", ret);
		else
			ts_err("failed read config:ret=0x%x", ret);
		break;
	case GTP_ASYNC_READ:
		ret = async_read(dev, (void __user *)arg);
		if (ret < 0)
			ts_err("Async data read failed");
		break;
	case GTP_SYNC_READ:
		if (filp->f_flags & O_NONBLOCK) {
			ts_err("Goodix tools now worked in sync_bus mode");
			ret = -EAGAIN;
			goto err_out;
		}
		ret = sync_read(dev, (void __user *)arg);
		if (ret < 0)
			ts_err("Sync data read failed");
		break;
	case GTP_ASYNC_WRITE:
		ret = async_write(dev, (void __user *)arg);
		if (ret < 0)
			ts_err("Async data write failed");
		break;
	case GTP_DRV_VERSION:
		ret = copy_to_user((u8 *) arg, GOODIX_DRIVER_VERSION,
				   sizeof(GOODIX_DRIVER_VERSION));
		if (ret)
			ts_err("failed copy driver version info to user");
		break;
	default:
		ts_info("Invalid cmd");
		ret = -ENOTTY;
		break;
	}

err_out:
	if (!temp_cfg) {
		kfree(temp_cfg);
		temp_cfg = NULL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long goodix_tools_compat_ioctl(struct file *file, unsigned int cmd,
				      unsigned long arg)
{
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)arg32);
}
#endif

static int goodix_tools_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	filp->private_data = goodix_tools_dev;
	ts_info("tools open");
	/* Only the first time open device need to register module */
	ret = goodix_register_ext_module(&goodix_tools_dev->module);
	if (ret) {
		ts_info("failed register to core module");
	}

	return ret;
}

static int goodix_tools_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	/* when the last close this dev node unregister the module */
	ret = goodix_unregister_ext_module(&goodix_tools_dev->module);
	return ret;
}

/**
 * goodix_tools_module_irq - goodix tools Irq handle
 * This functions is excuted when interrupt happended
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: EVT_CONTINUE let other module handle this irq
 */
static int goodix_tools_module_irq(struct goodix_ts_core *core_data,
				   struct goodix_ext_module *module)
{
	struct goodix_tools_dev *dev = module->priv_data;
	struct goodix_ts_device *ts_dev = dev->ts_core->ts_dev;
	const struct goodix_ts_hw_ops *hw_ops = ts_dev->hw_ops;
	struct goodix_tools_data *tools_data;
	int r = 0;
	u8 evt_sta = 0;

	mutex_lock(&dev->mutex);
	if (!list_empty(&dev->head)) {
		r = hw_ops->read_trans(ts_dev,
				       ts_dev->
				       reg.coor,
				       &evt_sta,
				       1);
		if (r < 0 || ((evt_sta & GOODIX_TOUCH_EVENT) == 0)) {
			ts_err("data not ready:0x%x, read ret =%d", evt_sta, r);
			mutex_unlock(&dev->mutex);
			return EVT_CONTINUE;
		}

		list_for_each_entry(tools_data, &dev->head, list) {
			if (!hw_ops->read_trans(ts_dev, tools_data->reg_addr,
						tools_data->data,
						tools_data->length)) {
				tools_data->filled = 1;
			}
		}
		wake_up(&dev->wq);
	}
	mutex_unlock(&dev->mutex);
	return EVT_CONTINUE;
}

static int goodix_tools_module_init(struct goodix_ts_core *core_data,
				    struct goodix_ext_module *module)
{
	struct goodix_tools_dev *tools_dev = module->priv_data;

	if (core_data)
		tools_dev->ts_core = core_data;
	else
		return -ENODEV;

	return 0;
}

static const struct file_operations goodix_tools_fops = {
	.owner = THIS_MODULE,
	.open = goodix_tools_open,
	.release = goodix_tools_release,
	.unlocked_ioctl = goodix_tools_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = goodix_tools_compat_ioctl,
#endif
};

static struct miscdevice goodix_tools_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = GOODIX_TOOLS_NAME,
	.fops = &goodix_tools_fops,
};

static struct goodix_ext_module_funcs goodix_tools_module_funcs = {
	.irq_event = goodix_tools_module_irq,
	.init = goodix_tools_module_init,
};

/**
 * goodix_tools_init - init goodix tools device and register a miscdevice
 *
 * return: 0 success, else failed
 */
static int __init goodix_tools_init(void)
{
	int ret;

	goodix_tools_dev = kzalloc(sizeof(struct goodix_tools_dev), GFP_KERNEL);
	if (goodix_tools_dev == NULL) {
		ts_err("Memory allco err");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&goodix_tools_dev->head);
	goodix_tools_dev->ops_mode = 0;
	goodix_tools_dev->ops_mode |= IRQ_FALG;
	init_waitqueue_head(&goodix_tools_dev->wq);
	mutex_init(&goodix_tools_dev->mutex);
	atomic_set(&goodix_tools_dev->t_count, 0);

	goodix_tools_dev->module.funcs = &goodix_tools_module_funcs;
	goodix_tools_dev->module.name = GOODIX_TOOLS_NAME;
	goodix_tools_dev->module.priv_data = goodix_tools_dev;
	goodix_tools_dev->module.priority = EXTMOD_PRIO_DBGTOOL;

	ret = misc_register(&goodix_tools_miscdev);
	if (ret)
		ts_err("Debug tools miscdev register failed");

	return ret;
}

static void __exit goodix_tools_exit(void)
{
	misc_deregister(&goodix_tools_miscdev);
	kfree(goodix_tools_dev);
	ts_info("Goodix tools miscdev exit");
}

module_init(goodix_tools_init);
module_exit(goodix_tools_exit);

MODULE_DESCRIPTION("Goodix tools Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
