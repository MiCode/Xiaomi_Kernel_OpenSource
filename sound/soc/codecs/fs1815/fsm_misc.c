/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-16 File created.
 */

#if defined(CONFIG_FSM_MISC)
#include "fsm_public.h"
#ifdef CONFIG_FSM_NONDSP
#include "fsm_q6afe.h"
#endif
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
	uint8_t index;
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
	memset(fsm_misc, 0, sizeof(struct fsm_misc));
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
	if (fsm_misc->fsm_dev[index] == 0) {
		fsm_misc->fsm_dev[index] = fsm_get_fsm_dev(slave);
		if (fsm_misc->fsm_dev[index] == NULL) {
			pr_debug("not found device:%02X", slave);
			fsm_misc->addr = 0;
			fsm_misc->index = 0;
			return -EINVAL;
		}
	}
	fsm_misc->index = index;
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

#ifdef CONFIG_FSM_NONDSP
int fsm_misc_send_apr(void __user *buf)
{
	uint32_t header[FSM_AFE_HEADER_LEN];
	uint32_t *ptr_data;
	struct fsm_afe afe;
	int buf_size;
	void *buffer;
	int ret;

	if (!buf) {
		pr_err("bad parameter");
		return -EINVAL;
	}
	ret = copy_from_user(header, (void *)buf, sizeof(header));
	if (ret) {
		pr_err("Copy from user failed! buf = 0x%pK", buf);
		return ret;
	}
	/* buffer:
	 * [0]: buf_size
	 * [1]: cmd_size
	 * [2]: module_id
	 * [3]: param_id
	 * [4]: opcode
	 * [...]: data
	 */
	buf_size = header[0];
	if (buf_size <= 0) {
		pr_err("Invalid buffer size = %d", buf_size);
		return -EINVAL;
	}
	buffer = fsm_alloc_mem(buf_size);
	afe.module_id = header[2];
	if (afe.module_id == AFE_MODULE_ID_FSADSP_TX) {
		afe.port_id = fsm_afe_get_tx_port();
	} else {
		afe.port_id = fsm_afe_get_rx_port();
	}
	afe.param_id = header[3];
	ptr_data = (uint32_t *)buffer;
	if (header[4]) { // op_set = 1
		ret = copy_from_user(buffer, (void *)buf, buf_size);
		if (ret) {
			pr_err("copy from user fail:%d", ret);
			goto exit;
		}
		afe.op_set = true;
		ret = fsm_afe_send_apr(&afe, &ptr_data[5],
				(buf_size - FSM_AFE_HEADER_LEN * sizeof(uint32_t)));
		if (ret) {
			pr_err("set params fail:%d", ret);
			goto exit;
		}
	} else {
		afe.op_set = false;
		ret = fsm_afe_send_apr(&afe, &ptr_data[5],
				(buf_size - FSM_AFE_HEADER_LEN * sizeof(uint32_t)));
		if (ret) {
			pr_err("get params fail:%d", ret);
			goto exit;
		}
		// set ptr_data[0] = total size
		ptr_data[0] = afe.param_size + FSM_AFE_HEADER_LEN * sizeof(uint32_t);
		ret = copy_to_user((void __user *)buf, buffer, ptr_data[0]);
		if (ret) {
			pr_err("copy to user fail:%d", ret);
			goto exit;
		}
	}
exit:
	fsm_free_mem((void **)&buffer);

	return ret;
}
#endif

static long fsm_misc_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct fsm_misc_args misc_args;
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
		break;
#ifdef CONFIG_FSM_NONDSP
	case FSM_IOC_SEND_APR:
		ret = fsm_misc_send_apr((void __user *)arg);
		if (ret) {
			pr_err("send apr fail:%d", ret);
		}
		break;
#endif
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
#ifdef CONFIG_FSM_NONDSP
	case FSM_IOC_SEND_APR:
	case FSM_IOC_SEND_APR32:
		cmd = FSM_IOC_SEND_APR;
		break;
#endif
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
	do {
		fsm_dev = fsm_misc->fsm_dev[fsm_misc->index];
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
	do {
		fsm_dev = fsm_misc->fsm_dev[fsm_misc->index];
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
