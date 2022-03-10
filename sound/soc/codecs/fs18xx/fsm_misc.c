// SPDX-License-Identifier: GPL-2.0

/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-16 File created.
 */

#if defined(CONFIG_FSM_MISC)
#include "fsm_public.h"
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

/* i2s clock ratio = fbclk/flrclk */
#if defined(CONFIG_FSM_MTK)
#define FSM_BCLK_RATIO      (64)
#else
#define FSM_BCLK_RATIO      (32)
#endif
#define FSM_I2C_MAX_LEN     (8192)

/* ioctl magic number and cmd. */
#define FSM_IOC_MAGIC       (0x7C)
#define FSM_IOC_GET_DEVICE  _IOR(FSM_IOC_MAGIC, 1, int)
#define FSM_IOC_SET_SRATE   _IOW(FSM_IOC_MAGIC, 2, int)
#define FSM_IOC_SET_SCENE   _IOW(FSM_IOC_MAGIC, 3, int)
#define FSM_IOC_INIT        _IOW(FSM_IOC_MAGIC, 4, int)
#define FSM_IOC_SPEAKER_ON  _IOW(FSM_IOC_MAGIC, 5, int)
#define FSM_IOC_SPEAKER_OFF _IOW(FSM_IOC_MAGIC, 6, int)
#define FSM_IOC_CALIBRATE   _IOW(FSM_IOC_MAGIC, 7, int)
#define FSM_IOC_F0_TEST     _IOW(FSM_IOC_MAGIC, 8, int)
// #define FSM_IOC_SET_SLAVE   _IOW(FSM_IOC_MAGIC, 8, int)
#define FSM_IOC_SET_SLAVE   (0x0706)
#define FSM_IOC_MAXNR       (8)

/*
 * misc driver for foursemi devices.
 */

struct fsm_i2c_cfg {
	uint8_t addr;
};

static struct fsm_i2c_cfg g_i2c_cfg;

static int fsm_misc_check_params(unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (cmd == FSM_IOC_SET_SLAVE) {
		return 0;
	}
	if (_IOC_TYPE(cmd) != FSM_IOC_MAGIC || _IOC_NR(cmd) > FSM_IOC_MAXNR) {
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	return (ret ? -EFAULT : 0);
}

static int fsm_misc_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	pr_debug("enter");
	return ret;
}

static int fsm_misc_set_slave(uint8_t slave)
{
	struct fsm_i2c_cfg *i2c_cfg = &g_i2c_cfg;

	if (i2c_cfg == NULL) {
		return -EINVAL;
	}
	if (slave < FSM_ADDR_BASE || slave >= FSM_ADDR_BASE + FSM_DEV_MAX) {
		return -EINVAL;
	}
	fsm_mutex_lock();
	i2c_cfg->addr = slave;
	if (fsm_get_fsm_dev(slave) == NULL) {
		fsm_mutex_unlock();
		return -EINVAL;
	}
	fsm_mutex_unlock();

	return 0;
}

static long fsm_misc_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int value;
	int ret;

	ret = fsm_misc_check_params(cmd, arg);
	if (ret) {
		pr_info("invalid params: %X, %lX", cmd, arg);
		return ret;
	}

	pr_debug("cmd:%X, arg:%lX", cmd, arg);
	switch (cmd) {
	case FSM_IOC_SET_SLAVE:
		ret = fsm_misc_set_slave(arg & 0xFF);
		break;
	case FSM_IOC_GET_DEVICE:
		value = fsm_dev_count();
		if (copy_to_user((int *)arg, &value, sizeof(value))) {
			return -EFAULT;
		}
		break;
	case FSM_IOC_SET_SRATE:
		fsm_set_i2s_clocks(arg, (FSM_BCLK_RATIO * arg));
		break;
	case FSM_IOC_SET_SCENE:
		fsm_set_scene(arg);
		break;
	case FSM_IOC_SPEAKER_ON:
		fsm_speaker_onn();
		break;
	case FSM_IOC_SPEAKER_OFF:
		fsm_speaker_off();
		break;
	case FSM_IOC_INIT:
		fsm_init();
		break;
	case FSM_IOC_CALIBRATE:
		fsm_re25_test((arg != 0));
		break;
	case FSM_IOC_F0_TEST:
		fsm_f0_test();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_COMPAT)
static long fsm_misc_compat_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	return fsm_misc_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static ssize_t fsm_misc_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct fsm_i2c_cfg *i2c_cfg = &g_i2c_cfg;
	struct fsm_dev *fsm_dev;
	int retries = FSM_I2C_RETRY;
	uint8_t *tmp;
	int ret;

	pr_debug("enter");
	if (i2c_cfg == NULL) {
		return -EINVAL;
	}
	if (count > FSM_I2C_MAX_LEN) {
		count = FSM_I2C_MAX_LEN;
	}
	tmp = fsm_alloc_mem(count);
	if (tmp == NULL) {
		return -ENOMEM;
	}

	fsm_dev = fsm_get_fsm_dev(i2c_cfg->addr);
	do {
		if (fsm_dev == NULL || fsm_dev->i2c == NULL) {
			ret = -EINVAL;
			break;
		}
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_master_recv(fsm_dev->i2c, tmp, count);
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != count) {
			fsm_delay_ms(5);
		}
		// pr_debug("data: 0x%02x", tmp[0]);
	} while ((ret != count) && (--retries > 0));

	if (ret == count) {
		ret = (copy_to_user(buf, tmp, count) ? -EFAULT : ret);
	} else {
		pr_info("%02X: reading %zu bytes failed:%d",
				i2c_cfg->addr, count, ret);
	}
	fsm_free_mem(tmp);

	return ret;
}

static ssize_t fsm_misc_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct fsm_i2c_cfg *i2c_cfg = &g_i2c_cfg;
	struct fsm_dev *fsm_dev;
	int retries = FSM_I2C_RETRY;
	uint8_t *tmp;
	int ret;

	pr_debug("enter");
	if (i2c_cfg == NULL) {
		return -EINVAL;
	}
	if (count > FSM_I2C_MAX_LEN) {
		count = FSM_I2C_MAX_LEN;
	}
	tmp = memdup_user(buf, count);
	if (IS_ERR(tmp)) {
		return PTR_ERR(tmp);
	}

	fsm_dev = fsm_get_fsm_dev(i2c_cfg->addr);
	do {
		if (fsm_dev == NULL || fsm_dev->i2c == NULL) {
			ret = -EINVAL;
			break;
		}
		// pr_debug("data: 0x%02x-0x%02x", buf[0], tmp[0]);
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_master_send(fsm_dev->i2c, tmp, count);
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != count) {
			fsm_delay_ms(5);
		}
	} while ((ret != count) && (--retries > 0));

	if (ret != count) {
		pr_info("%02X: writing %zu bytes failed:%d",
				i2c_cfg->addr, count, ret);
	}
	fsm_free_mem(tmp);

	return ret;
}

static const struct file_operations g_fsm_misc_ops = {
	.owner	= THIS_MODULE,
	.open	= fsm_misc_open,
	.unlocked_ioctl = fsm_misc_ioctl,
#if defined(CONFIG_COMPAT)
	.compat_ioctl = fsm_misc_compat_ioctl,
#endif
	.llseek	= no_llseek,
	.read	= fsm_misc_read,
	.write	= fsm_misc_write,
};

struct miscdevice g_fsm_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = FSM_DRV_NAME,
	.fops  = &g_fsm_misc_ops,
	.this_device = NULL,
};

int fsm_misc_init(void)
{
	struct miscdevice *misc = &g_fsm_misc;
	int ret;

	pr_debug("enter");
	if (misc->this_device) {
		return 0;
	}
	ret = misc_register(misc);
	if (ret) {
		misc->this_device = NULL;
	}

	FSM_FUNC_EXIT(ret);
	return ret;
}

void fsm_misc_deinit(void)
{
	struct miscdevice *misc = &g_fsm_misc;

	pr_debug("enter");
	if (misc->this_device == NULL) {
		return;
	}
	misc_deregister(misc);
}
#endif
