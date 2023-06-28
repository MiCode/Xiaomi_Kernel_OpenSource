/*
 * Copyright (C) 2022, SI-IN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/current.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include "sipa_tuning_if.h"

/*
 up:     pc -> kernel -> hal -> dsp
    vdd/cmd -> kernel -> hal -> dsp

 down:   dsp -> hal -> kernel -> pc
*/

ssize_t sipa_turning_up_read(struct file *fl, char __user *buf, size_t len, loff_t *off)
{
    sipa_turning_t *priv = g_sipa_turning;
    int ret = 0;
    PARAM_CHECK(buf, len);

    ret = wait_event_interruptible(priv->up.wq, priv->up.flag);
    if (ret) {
        pr_err("[  err] %s: wait_event failed\n", __func__);
        return -ERESTART; 
    }

    if (copy_to_user(buf, priv->up.data, priv->up.len)) {
        pr_err("[  err] %s: copy to user failed\n", __func__);
        return -EFAULT;
    }
    priv->up.flag = false;
    pr_info("[ info] %s: read:%d\n", __func__, priv->up.len);
    ret = priv->up.len;

    return ret;
}

ssize_t sipa_turning_up_write(struct file *fl, const char __user *buf, size_t len, loff_t *off)
{
    sipa_turning_t *priv = g_sipa_turning;
    struct dev_comm_data *cmd = NULL;
    PARAM_CHECK(buf, len);

    mutex_lock(&priv->up.lock);
    if (copy_from_user(priv->up.data, buf, len)) {
        pr_err("[  err] %s: copy from user failed\n", __func__);
        mutex_unlock(&priv->up.lock);
        return -EFAULT;
    }

    cmd = (struct dev_comm_data *)priv->up.data;
    priv->up.len = DEV_COMM_DATA_LEN(cmd);
    priv->up.flag = true;
    pr_info("[ info] %s: datalen:%d payload len:%d\n", __func__, len, priv->up.len);
    wake_up_interruptible(&priv->up.wq);
    mutex_unlock(&priv->up.lock);

    return len; 
}

static long sipa_tuning_up_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
    sipa_turning_t *priv = g_sipa_turning;
    struct dev_comm_data *msg = NULL;
    int ret = 0;

	pr_info("[ info] %s: enter\n", __func__);

	switch (cmd) {
        case SIPA_CMD_TUNING_CTRL_WR: {
                pr_info("[ info] %s: write cmd\n", __func__);
                if (copy_from_user(priv->ctrlup.data,  (void __user *)arg, sizeof(dev_comm_data_t))) {
                    return -EFAULT;
                }
                msg = (struct dev_comm_data *)priv->ctrlup.data;
                priv->ctrlup.len = DEV_COMM_DATA_LEN(msg);
                if (copy_from_user(priv->ctrlup.data,  (void __user *)arg, priv->ctrlup.len)) {
                    return -EFAULT;
                }
                pr_info("[ info] %s: datalen:%d payload len:%d\n", __func__, priv->ctrlup.len, msg->payload_size);
                priv->ctrlup.flag = true;
                wake_up_interruptible(&priv->ctrlup.wq);
            }
            break;
        case SIPA_CMD_TUNING_CTRL_RD: {
                ret = wait_event_interruptible(priv->ctrlup.wq, priv->ctrlup.flag);
                if (ret) {
                    pr_err("[  err] %s: wait_event failed\n", __func__);
                    return -ERESTART; 
                }
                if (copy_to_user((void __user *)arg, priv->ctrlup.data, priv->ctrlup.len)) {
                    return -EFAULT;
                }
                priv->ctrlup.flag = false;
                pr_info("[ info] %s: read cmd, len:%d \n", __func__, priv->ctrlup.len);
            }
            break;
        default:
	        pr_info("[ info] %s: unsuport cmd:0x%x\n", __func__, cmd);
            return -EFAULT;
    }
	return 0;
}

struct file_operations sipa_turning_up_fops = {
    .owner = THIS_MODULE,
    .read = sipa_turning_up_read,
    .write = sipa_turning_up_write,
	.unlocked_ioctl = sipa_tuning_up_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_up_unlocked_ioctl,
};

ssize_t sipa_turning_down_read(struct file *fl, char __user *buf, size_t len, loff_t *off)
{
    sipa_turning_t *priv = g_sipa_turning;
    int ret = 0;
    PARAM_CHECK(buf, len);

    ret = wait_event_interruptible(priv->down.wq, priv->down.flag);
    if (ret) {
        pr_err("[  err] %s: wait_event failed\n", __func__);
        return -ERESTART; 
    }

    if (copy_to_user(buf, priv->down.data, priv->down.len)) {
        pr_err("[  err] %s: copy to user failed\n", __func__);
        return -EFAULT;
    }
    priv->down.flag = false;
    pr_info("[ info] %s: read:%d\n", __func__, priv->down.len);
    ret = priv->down.len;

    return ret;
}

ssize_t sipa_turning_down_write(struct file *fl, const char __user *buf, size_t len, loff_t *off)
{
    sipa_turning_t *priv = g_sipa_turning;
    struct dev_comm_data *cmd = NULL;
    PARAM_CHECK(buf, len);

    if (copy_from_user(priv->down.data, buf, len)) {
        pr_err("[  err] %s: copy from user failed\n", __func__);
        return -EFAULT;
    }

    cmd = (struct dev_comm_data *)priv->down.data;
    priv->down.len = DEV_COMM_DATA_LEN(cmd);
    priv->down.flag = true;
    pr_info("[ info] %s: datalen:%d payload len:%d\n", __func__, len, priv->up.len);
    wake_up_interruptible(&priv->down.wq);

    return len; 
}

static long sipa_tuning_down_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
    sipa_turning_t *priv = g_sipa_turning;
    struct dev_comm_data *msg = NULL;
    int ret = 0;

	pr_info("[ info] %s: enter\n", __func__);

	switch (cmd) {
        case SIPA_CMD_TUNING_CTRL_WR: {
                pr_info("[ info] %s: write cmd\n", __func__);
                if (copy_from_user(priv->ctrldown.data,  (void __user *)arg, sizeof(dev_comm_data_t))) {
                    return -EFAULT;
                }
                msg = (struct dev_comm_data *)priv->ctrldown.data;
                priv->ctrldown.len = DEV_COMM_DATA_LEN(msg);
                if (copy_from_user(priv->ctrldown.data,  (void __user *)arg, priv->ctrldown.len)) {
                    return -EFAULT;
                }
                pr_info("[ info] %s: datalen:%d payload len:%d\n", __func__, priv->ctrldown.len, msg->payload_size);
                priv->ctrldown.flag = true;
                wake_up_interruptible(&priv->ctrldown.wq);
            }
            break;
        case SIPA_CMD_TUNING_CTRL_RD: {
                ret = wait_event_interruptible(priv->ctrldown.wq, priv->ctrldown.flag);
                if (ret) {
                    pr_err("[  err] %s: wait_event failed\n", __func__);
                    return -ERESTART; 
                }
                if (copy_to_user((void __user *)arg, priv->ctrldown.data, priv->ctrldown.len)) {
                    return -EFAULT;
                }
                priv->ctrldown.flag = false;
                pr_info("[ info] %s: read cmd, len:%d \n", __func__, priv->ctrldown.len);
            }
            break;
        default:
	        pr_info("[ info] %s: unsuport cmd:0x%x\n", __func__, cmd);
            return -EFAULT;
    }
	return 0;
}

struct file_operations sipa_turning_down_fops = {
    .owner = THIS_MODULE,
    .read = sipa_turning_down_read,
    .write = sipa_turning_down_write,
	.unlocked_ioctl = sipa_tuning_down_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_down_unlocked_ioctl,
};
