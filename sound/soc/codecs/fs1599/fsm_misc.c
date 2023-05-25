/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-16 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_MISC)
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

#define FSM_I2C_MAX_LEN     (8192)

/*
 * misc driver for foursemi devices.
 */

struct fsm_misc {
	struct fsm_dev *fsm_dev[FSM_DEV_MAX];
	uint8_t addr;
	uint8_t idx;
};

struct fsm_dev_info {
	int state; // 1: spk on, 0: spk off
	int ndev;  // ndev of preset
	int dev_count; // number of i2c devices
	int without_dsp; // with or without dsp
	int addr[FSM_DEV_MAX]; // addr of i2c devices
	int pos[FSM_DEV_MAX]; // position of i2c devices
	int re25[FSM_DEV_MAX];
};

static int g_misc_opened;

static int fsm_misc_check_params(unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (cmd == FSM_IOC_SET_SLAVE) {
		return 0;
	}
	if (_IOC_TYPE(cmd) != FSM_IOC_MAGIC || _IOC_NR(cmd) >= FSM_IOC_MAXNR) {
		return -ENOTTY;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	if (_IOC_DIR(cmd) & _IOC_READ) {
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}
#else
	ret = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
#endif

	return (ret ? -EFAULT : 0);
}

static int fsm_misc_open(struct inode *inode, struct file *filp)
{
	struct fsm_misc *fsm_misc;
	int dev;

	pr_debug("enter");
	if (xchg(&g_misc_opened, 1)) {
			pr_warning("device busy now");
			return -EBUSY;
	}
	fsm_misc = fsm_alloc_mem(sizeof(struct fsm_misc));
	if (fsm_misc == NULL) {
		pr_err("allocat memory fail");
		return -EINVAL;
	}
	fsm_misc->addr = 0;
	for (dev = 0; dev < FSM_DEV_MAX; dev++) {
		fsm_misc->fsm_dev[dev] = NULL;
	}
	filp->private_data = fsm_misc;

	return 0;
}

static int fsm_misc_release(struct inode *inode, struct file *filp)
{
	struct fsm_misc *fsm_misc;

	pr_debug("enter");
	if (!filp) {
		return -EINVAL;
	}
	fsm_misc = filp->private_data;
	g_misc_opened = 0;
	fsm_free_mem((void **)&fsm_misc);

	return 0;
}

static int fsm_misc_set_slave(struct fsm_misc *fsm_misc, uint8_t slave)
{
	int index;

	if (fsm_misc == NULL) {
		return -EINVAL;
	}
	if (slave < FSM_ADDR_BASE || slave >= FSM_ADDR_BASE + FSM_DEV_MAX) {
		pr_err("invalid address:%02X", slave);
		return -EINVAL;
	}
	index = slave - FSM_ADDR_BASE;
	if (fsm_misc->fsm_dev[index] == NULL) {
		fsm_misc->fsm_dev[index] = fsm_get_fsm_dev(slave);
		if (fsm_misc->fsm_dev[index] == NULL) {
			pr_debug("not found device:%02X", slave);
			fsm_misc->addr = 0;
			fsm_misc->idx = 0;
			return -EINVAL;
		}
	}
	fsm_misc->idx = index;
	fsm_misc->addr = slave;

	return 0;
}

static int fsm_misc_cmd_with_args(unsigned int cmd, uint8_t addr,
				struct fsm_misc_args *args)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg || !args) {
		pr_err("bad parameters");
		return -EINVAL;
	}
	switch (cmd) {
	case FSM_IOC_SET_SRATE:
		fsm_set_i2s_clocks(args->srate, args->bclk);
		break;
	case FSM_IOC_SET_SCENE:
		fsm_set_scene(args->scene);
		break;
	case FSM_IOC_INIT:
		cfg->force_init  = args->force_init;
		fsm_init();
		cfg->force_init  = 0;
		break;
	case FSM_IOC_CALIBRATE:
		break;
	case FSM_IOC_F0_TEST:
		break;
	default:
		pr_err("invalid cmd: %X", cmd);
		return -EINVAL;
	}

	return 0;
}

int fsm_get_dev_info(int *arg)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsm_dev_info dev_info;
	struct preset_file *pfile;
	struct fsm_dev *fsm_dev;
	int index;
	int dev;
	int ret;

	pfile = fsm_get_presets();
	if (cfg == NULL || arg == NULL || pfile == NULL) {
		pr_err("bad parameter");
		return -EINVAL;
	}

	memset(&dev_info, 0, sizeof(struct fsm_dev_info));
	dev_info.state = cfg->speaker_on;
	dev_info.ndev = pfile->hdr.ndev;
	dev_info.dev_count = fsm_dev_count();
	dev_info.without_dsp = cfg->nondsp_mode;
	for (dev = 0; dev < dev_info.dev_count; dev++) {
		fsm_dev = fsm_get_fsm_dev_by_id(dev);
		if (fsm_dev == NULL) {
			continue;
		}
		index = fsm_get_index_by_position(fsm_dev->pos_mask);
		if (index < 0) {
			pr_addr(err, "get index fail:%d", index);
			continue;
		}
		dev_info.addr[index] = fsm_dev->addr;
		dev_info.pos[index]  = fsm_dev->pos_mask;
		dev_info.re25[index] = fsm_dev->re25;
	}
	ret = copy_to_user((int *)arg, &dev_info, sizeof(dev_info));
	if (ret) {
		pr_err("get dev info fail:%d", ret);
		return -EFAULT;
	}

	return 0;
}

static long fsm_misc_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct fsm_misc_args misc_args;
	struct fsm_re25_data re25_data;
	struct fsm_livedata livedata;
	struct fsm_misc *fsm_misc;
	int count;
	int ret;

	if (filp == NULL || filp->private_data == NULL) {
		pr_err("invalid parameter");
		return -EINVAL;
	}
	fsm_misc = filp->private_data;
	ret = fsm_misc_check_params(cmd, arg);
	if (ret) {
		pr_err("invalid params: %X, %lX", cmd, arg);
		return ret;
	}

	pr_debug("cmd:%X, arg:%lX", cmd, arg);
	switch (cmd) {
	case FSM_IOC_SET_SLAVE:
	case FSM_IOC_SET_ADDR:
		ret = fsm_misc_set_slave(fsm_misc, arg & 0xFF);
		break;
	case FSM_IOC_GET_DEVICE:
		count = fsm_dev_count();
		ret = copy_to_user((int *)arg, &count, sizeof(count));
		if (ret) {
			pr_err("cmd:%X, copy to user fail:%d", cmd, ret);
			return -EFAULT;
		}
		break;
	case FSM_IOC_SET_SRATE:
	case FSM_IOC_SET_SCENE:
	case FSM_IOC_INIT:
	case FSM_IOC_CALIBRATE:
	case FSM_IOC_F0_TEST:
		ret = copy_from_user(&misc_args, (void *)arg, sizeof(misc_args));
		if (ret) {
			pr_err("cmd:%X, copy from user fail:%d", cmd, ret);
			return -EFAULT;
		}
		ret = fsm_misc_cmd_with_args(cmd, fsm_misc->addr, &misc_args);
		if (ret) {
			pr_err("cmd:%X, error:%d", cmd, ret);
		}
		break;
	case FSM_IOC_SPEAKER_ON:
		fsm_speaker_onn();
		break;
	case FSM_IOC_SPEAKER_OFF:
		fsm_speaker_off();
		break;
	case FSM_IOC_GET_RESULT:
		memset(&livedata, 0, sizeof(livedata));
		fsm_get_livedata(&livedata);
		ret = copy_to_user((int *)arg, &livedata, sizeof(livedata));
		if (ret) {
			pr_err("cmd:%X, copy to user fail:%d", cmd, ret);
			return -EFAULT;
		}
		break;
	case FSM_IOC_SEND_APR:
		ret = -EINVAL;
		break;
	case FSM_IOC_GET_INFO:
		ret = fsm_get_dev_info((int *)arg);
		break;
	case FSM_IOC_SET_RE:
		ret = copy_from_user(&re25_data, (void *)arg, sizeof(re25_data));
		if (ret) {
			pr_err("cmd:%X, copy from user fail:%d", cmd, ret);
			return -EFAULT;
		}
		ret = fsm_set_re25_data(&re25_data);
		break;
	default:
		pr_err("unknown cmd:%X", cmd);
		ret = -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_COMPAT)
static long fsm_misc_compat_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	switch (cmd) {
	case FSM_IOC_GET_RESULT32:
		cmd = FSM_IOC_GET_RESULT;
		break;
	case FSM_IOC_SEND_APR32:
		cmd = FSM_IOC_SEND_APR;
		break;
	case FSM_IOC_GET_INFO32:
		cmd = FSM_IOC_GET_INFO;
		break;
	case FSM_IOC_SET_RE32:
		cmd = FSM_IOC_SET_RE;
		break;
	default:
		break;
	}
	return fsm_misc_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static ssize_t fsm_misc_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsm_misc *fsm_misc;
	struct fsm_dev *fsm_dev;
	int retries = FSM_I2C_RETRY;
	uint8_t *tmp;
	int ret;

	if (cfg == NULL || filp == NULL || filp->private_data == NULL) {
		pr_err("invalid parameter");
		return -EINVAL;
	}
	if (count > FSM_I2C_MAX_LEN) {
		count = FSM_I2C_MAX_LEN;
	}
	tmp = fsm_alloc_mem(count);
	if (tmp == NULL) {
		return -ENOMEM;
	}

	if (cfg->skip_monitor == false) {
		cfg->skip_monitor = true;
	}
	fsm_misc = filp->private_data;
	if (fsm_misc->idx >= FSM_DEV_MAX || fsm_misc->addr == 0) {
		pr_err("invalid misc parameter");
		return -EINVAL;
	}
	do {
		fsm_dev = fsm_misc->fsm_dev[fsm_misc->idx];
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
	} while((ret != count) && (--retries >= 0));

	if (ret == count) {
		ret = (copy_to_user(buf, tmp, count) ? -EFAULT : ret);
	} else {
		pr_err("%02X: reading %zu bytes failed:%d",
				fsm_misc->addr, count, ret);
	}
	fsm_free_mem((void **)&tmp);

	return ret;
}

static ssize_t fsm_misc_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsm_misc *fsm_misc;
	struct fsm_dev *fsm_dev;
	int retries = FSM_I2C_RETRY;
	uint8_t *tmp;
	int ret;

	if (cfg == NULL || filp == NULL || filp->private_data == NULL) {
		pr_err("invalid parameter");
		return -EINVAL;
	}
	if (count > FSM_I2C_MAX_LEN) {
		count = FSM_I2C_MAX_LEN;
	}
	tmp = memdup_user(buf, count);
	if (IS_ERR(tmp)) {
		return PTR_ERR(tmp);
	}

	if (cfg->skip_monitor == false) {
		cfg->skip_monitor = true;
	}
	fsm_misc = filp->private_data;
	if (fsm_misc->idx >= FSM_DEV_MAX || fsm_misc->addr == 0) {
		pr_err("invalid misc parameter");
		return -EINVAL;
	}
	do {
		fsm_dev = fsm_misc->fsm_dev[fsm_misc->idx];
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
	} while((ret != count) && (--retries >= 0));

	if (ret != count) {
		pr_err("%02X: writing %zu bytes failed:%d",
				fsm_misc->addr, count, ret);
	}
	kfree(tmp);

	return ret;
}

static const struct file_operations g_fsm_misc_ops = {
	.owner   = THIS_MODULE,
	.open    = fsm_misc_open,
	.read    = fsm_misc_read,
	.write   = fsm_misc_write,
	.release = fsm_misc_release,
	.llseek  = no_llseek,
	.unlocked_ioctl = fsm_misc_ioctl,
#if defined(CONFIG_COMPAT)
	.compat_ioctl = fsm_misc_compat_ioctl,
#endif
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
