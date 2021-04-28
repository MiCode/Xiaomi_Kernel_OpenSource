/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <mt-plat/mtk-mbox.h>

#include "adsp_reserved_mem.h"
#include "adsp_platform_driver.h"
#include "adsp_logger.h"
#include "adsp_excep.h"
#include "adsp_core.h"

/* ----------------------------- sys fs ------------------------------------ */
static inline ssize_t dev_dump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);
	int n = 0, i = 0;

	n += scnprintf(buf + n, PAGE_SIZE - n, "name:%s id = %d\n",
		       pdata->name, pdata->id);
	n += scnprintf(buf + n, PAGE_SIZE - n, "cfg = %p, size = %zu\n",
		       pdata->cfg, pdata->cfg_size);
	n += scnprintf(buf + n, PAGE_SIZE - n, "itcm = %p, size = %zu\n",
		       pdata->itcm, pdata->itcm_size);
	n += scnprintf(buf + n, PAGE_SIZE - n, "dtcm = %p, size = %zu\n",
		       pdata->dtcm, pdata->dtcm_size);
	n += scnprintf(buf + n, PAGE_SIZE - n, "sysram = %p, size = %zu\n",
		       pdata->sysram, pdata->sysram_size);

	for (i = 0; i < ADSP_IRQ_NUM; i++)
		n += scnprintf(buf + n, PAGE_SIZE - n, "%d ", pdata->irq[i].seq);
	n += scnprintf(buf + n, PAGE_SIZE - n, "\n");

	n += scnprintf(buf + n, PAGE_SIZE - n,
		       "status = %d, feature_set = %X\n",
		       pdata->state, pdata->feature_set);

	if (pdata->send_mbox)
		n += scnprintf(buf + n, PAGE_SIZE - n, "mailbox send = %d\n",
			pdata->send_mbox->mbox);
	if (pdata->recv_mbox)
		n += scnprintf(buf + n, PAGE_SIZE - n, "mailbox recv = %d\n",
			pdata->recv_mbox->mbox);

	return n;
}
DEVICE_ATTR_RO(dev_dump);

static inline ssize_t ipi_test_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);
	int value = 0;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	if (_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0) == 0) {
		adsp_push_message(ADSP_IPI_TEST1, &value, sizeof(value),
				  20, pdata->id);

		_adsp_deregister_feature(pdata->id, SYSTEM_FEATURE_ID, 0);
	}

	return count;
}

static inline ssize_t ipi_test_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);
	unsigned int value = 0x5A5A;
	int ret;

	if (!is_adsp_ready(pdata->id))
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");

	if (_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0) == 0) {
		ret = adsp_push_message(ADSP_IPI_TEST1, &value,
					sizeof(value), 20, pdata->id);
		_adsp_deregister_feature(pdata->id,
					 SYSTEM_FEATURE_ID, 0);
		return scnprintf(buf, PAGE_SIZE, "ADSP ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP register fail\n");
}
DEVICE_ATTR_RW(ipi_test);

static inline ssize_t suspend_cmd_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	return adsp_dump_feature_state(pdata->id, buf, PAGE_SIZE);
}

static inline ssize_t suspend_cmd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t fid = 0;
	char *temp = NULL, *token1 = NULL, *token2 = NULL;
	char *pin = NULL;
	char delim[] = " ,\t\n";
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	temp = kstrdup(buf, GFP_KERNEL);
	pin = temp;
	token1 = strsep(&pin, delim);
	token2 = strsep(&pin, delim);

	if (!token1 || !token2)
		goto EXIT;

	fid = adsp_get_feature_index(token2);

	if ((strcmp(token1, "regi") == 0) && (fid >= 0))
		_adsp_register_feature(pdata->id, fid, 0);

	if ((strcmp(token1, "deregi") == 0) && (fid >= 0))
		_adsp_deregister_feature(pdata->id, fid, 0);

EXIT:
	kfree(temp);
	return count;
}
DEVICE_ATTR_RW(suspend_cmd);

static inline ssize_t log_enable_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	return adsp_dump_log_state(pdata->log_ctrl, buf, PAGE_SIZE);
}

static inline ssize_t log_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int enable = 0;
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	adsp_log_enable(pdata->log_ctrl, pdata->id, enable);
	return count;
}
DEVICE_ATTR_RW(log_enable);

static inline ssize_t wakelock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int input = 0;
	int ret = 0;
	struct adsp_priv *pdata = container_of(dev_get_drvdata(dev),
					struct adsp_priv, mdev);

	if (kstrtouint(buf, 0, &input) != 0)
		return -EINVAL;

	if (input == 1)
		ret = adsp_awake_lock(pdata->id);
	else if (input == 0)
		ret = adsp_awake_unlock(pdata->id);

	pr_info("%s, input %u, ret = %d", __func__, input, ret);

	return count;
}
DEVICE_ATTR_WO(wakelock);

static struct attribute *adsp_default_attrs[] = {
	&dev_attr_dev_dump.attr,
	&dev_attr_ipi_test.attr,
	&dev_attr_suspend_cmd.attr,
	&dev_attr_log_enable.attr,
	&dev_attr_wakelock.attr,
	NULL,
};

struct attribute_group adsp_default_attr_group = {
	.attrs = adsp_default_attrs,
};

/* ---------------------------- debug fs ----------------------------------- */

static ssize_t adsp_debug_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *pos)
{
	char *buffer = NULL;
	size_t n = 0, max_size;
	struct adsp_priv *pdata = filp->private_data;
	u32 memid = pdata->id + ADSP_A_DEBUG_DUMP_MEM_ID;

	buffer = adsp_get_reserve_mem_virt(memid);
	max_size = adsp_get_reserve_mem_size(memid);

	if (buffer)
		n = strnlen(buffer, max_size);

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static ssize_t adsp_debug_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[64];
	struct adsp_priv *pdata = filp->private_data;

	if (copy_from_user(buf, buffer, min(count, sizeof(buf))))
		return -EFAULT;

	if (_adsp_register_feature(pdata->id, SYSTEM_FEATURE_ID, 0) == 0) {
		adsp_push_message(ADSP_IPI_ADSP_TIMER,
				buf, min(count, sizeof(buf)), 0, pdata->id);

		_adsp_deregister_feature(pdata->id, SYSTEM_FEATURE_ID, 0);
	}

	return count;
}

const struct file_operations adsp_debug_ops = {
	.open = simple_open,
	.read = adsp_debug_read,
	.write = adsp_debug_write,
};

/* ------------------------------ misc device ----------------------------- */
/*==============================================================================
 *                     ioctl
 *==============================================================================
 */
#define AUDIO_DSP_IOC_MAGIC 'a'
#define AUDIO_DSP_IOCTL_ADSP_REG_FEATURE  \
	_IOW(AUDIO_DSP_IOC_MAGIC, 0, unsigned int)
#define AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS \
	_IOR(AUDIO_DSP_IOC_MAGIC, 1, unsigned int)
#define AUDIO_DSP_IOCTL_ADSP_RESET_CBK \
	_IOR(AUDIO_DSP_IOC_MAGIC, 2, unsigned int)

union ioctl_param {
	struct {
	    uint16_t enable;
	    uint16_t fid;
	} cmd0;
	struct {
	    int16_t flag;
	    uint16_t cid;
	} cmd1;
};

/* user-space feature reset */
static int adsp_hal_feature_table[ADSP_NUM_FEATURE_ID];
void reset_hal_feature_table(void)
{
	int i;

	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++) {
		while (adsp_hal_feature_table[i] > 0) {
			adsp_deregister_feature(i);
			adsp_hal_feature_table[i]--;
		}
	}
}

/* file operations */
static ssize_t adsp_driver_read(struct file *filp, char __user *data,
				  size_t len, loff_t *ppos)
{
	struct adsp_priv *pdata = container_of(filp->private_data,
					struct adsp_priv, mdev);

	return adsp_log_read(pdata->log_ctrl, data, len);
}
static int adsp_driver_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static unsigned int adsp_driver_poll(struct file *filp,
				     struct poll_table_struct *poll)
{
	struct adsp_priv *pdata = container_of(filp->private_data,
					struct adsp_priv, mdev);

	if (!pdata->log_ctrl || !pdata->log_ctrl->inited)
		return 0;

	if (!(filp->f_mode & FMODE_READ))
		return 0;

	return adsp_log_poll(pdata->log_ctrl);
}

static long adsp_driver_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	union ioctl_param t;

	switch (cmd) {
	case AUDIO_DSP_IOCTL_ADSP_REG_FEATURE: {
		if (copy_from_user(&t, (void *)arg, sizeof(t))) {
			ret = -EFAULT;
			break;
		}

		if (t.cmd0.enable)
			ret = adsp_register_feature(t.cmd0.fid);
		else
			ret = adsp_deregister_feature(t.cmd0.fid);

		if (ret == 0) {
			if (t.cmd0.enable)
				adsp_hal_feature_table[t.cmd0.fid]++;
			else
				adsp_hal_feature_table[t.cmd0.fid]--;

			if (adsp_hal_feature_table[t.cmd0.fid] < 0)
				adsp_hal_feature_table[t.cmd0.fid] = 0;
		}
		break;
	}
	case AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS: {
		if (copy_from_user(&t, (void *)arg, sizeof(t))) {
			ret = -EFAULT;
			break;
		}

		t.cmd1.flag = is_adsp_ready(t.cmd1.cid);

		if (copy_to_user((void __user *)arg, &t, sizeof(t))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	default:
		pr_debug("%s(), invalid ioctl cmd\n", __func__);
	}

	if (ret < 0)
		pr_info("%s(), ioctl error %d\n", __func__, ret);

	return ret;
}

static long adsp_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		pr_notice("op null\n");
		return -ENOTTY;
	}
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}

const struct file_operations adsp_common_file_ops = {
	.owner = THIS_MODULE,
	.open = adsp_driver_open,
	.unlocked_ioctl = adsp_driver_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = adsp_driver_compat_ioctl,
#endif
};

const struct file_operations adsp_core_file_ops = {
	.owner = THIS_MODULE,
	.read = adsp_driver_read,
	.open = adsp_driver_open,
	.poll = adsp_driver_poll,
};

