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
#include "sipa_common.h"

/*
 up:     pc -> kernel -> hal -> dsp
 down:   dsp -> hal -> kernel -> pc
*/

static int sipa_tuning_write(void __user * arg, sipa_sync_t *pdata)
{
    sipa_turning_t *priv = g_sipa_turning;
    struct dev_comm_data *msg = NULL;
    mutex_lock(&priv->lock);
    if (copy_from_user(pdata->data, arg, sizeof(dev_comm_data_t))) {
        mutex_unlock(&priv->lock);
        return -EFAULT;
    }
    msg = (struct dev_comm_data *)pdata->data;
    pdata->len = DEV_COMM_DATA_LEN(msg);
    if (copy_from_user(pdata->data, arg, pdata->len)) {
        mutex_unlock(&priv->lock);
        return -EFAULT;
    }
    pr_info("[ info] %s: datalen:%d payload len:%d\n", __func__, pdata->len, msg->payload_size);
    pdata->flag = true;
    wake_up_interruptible(&pdata->wq);
    mutex_unlock(&priv->lock);
    return 0;
}

static int sipa_tuning_read(void __user *arg, sipa_sync_t *pdata)
{
    int ret = wait_event_interruptible(pdata->wq, pdata->flag);
    if (ret) {
        pr_info("%s: wait_event return\n", __func__);
        return -ERESTART; 
    }
    if (copy_to_user(arg, pdata->data, pdata->len)) {
        return -EFAULT;
    }
    pdata->flag = false;
    return 0;
}

static long sipa_tuning_comm_ioctl(unsigned int cmd, unsigned long arg, sipa_sync_t *up, sipa_sync_t *down)
{
    int ret = 0;

    switch (cmd) {
        case SIPA_TUNING_CTRL_WR_UP: {
                pr_info("[ info] %s: up write cmd\n", __func__);
                ret = sipa_tuning_write((void __user *)arg, up);
            }
            break;
        case SIPA_TUNING_CTRL_RD_UP: {
                ret = sipa_tuning_read((void __user *)arg, up);
                pr_info("[ info] %s: up read cmd, len:%d \n", __func__, up->len);
            }
            break;
        case SIPA_TUNING_CTRL_WR_DOWN: {
                pr_info("[ info] %s: down write cmd\n", __func__);
                ret = sipa_tuning_write((void __user *)arg, down);
            }
            break;
        case SIPA_TUNING_CTRL_RD_DOWN: {
                ret = sipa_tuning_read((void __user *)arg, down);
                pr_info("[ info] %s: down read cmd, len:%d \n", __func__, down->len);
            }
            break;
        default: {
            pr_info("[ info] %s: unsuport cmd:0x%x\n", __func__, cmd);
            ret = -EFAULT;
            break;
        }
    }

    return ret;
}

static long sipa_tuning_cmd_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
    sipa_turning_t *priv = g_sipa_turning;
    scene_data_t scene_data;
    box_name_t fwname;
    char *pstr = NULL;
    int ret = 0;

    pr_info("[ info] %s: enter cmd:0x%x\n", __func__, cmd);
    switch (cmd) {
        case SIPA_TUNING_CTRL_WR_UP:
        case SIPA_TUNING_CTRL_RD_UP:
        case SIPA_TUNING_CTRL_WR_DOWN:
        case SIPA_TUNING_CTRL_RD_DOWN: {
            ret = sipa_tuning_comm_ioctl(cmd, arg, &(priv->cmdup), &(priv->cmddown));
            break;
        }
        case SIPA_IOCTL_LOAD_FIRMWARE: {
            if (copy_from_user(&fwname.len, (void __user *)arg, sizeof(uint8_t))) {
                pr_err("%s: power_on copy from user failed\n", __func__);
                return -EFAULT;
            }
            if (fwname.len > sizeof(fwname.boxname)) {
                pr_err("%s: input too long, len:%d, maxlen:%d\n", __func__, fwname.len, sizeof(fwname.boxname));
                return -EFAULT;
            }
			pstr = (char*)arg + 1;
            if (copy_from_user(&fwname.boxname, (void __user *)pstr, sizeof(fwname.boxname))) {
                pr_err("%s: power_on copy from user failed\n", __func__);
                return -EFAULT;
            }
            sipa_multi_channel_load_fw(fwname.boxname);
            break;
        }
        case SIPA_IOCTL_POWER_ON: {
            if (copy_from_user(&scene_data, (void __user *)arg, sizeof(scene_data_t))) {
                pr_err("%s: power_on copy from user failed\n", __func__);
                return -EFAULT;
            }
            sipa_multi_channel_power_on_and_set_scene(scene_data.scene, scene_data.pa_idx);
            break;
        }
        case SIPA_IOCTL_POWER_OFF: {
            if (copy_from_user(&scene_data, (void __user *)arg, sizeof(scene_data_t))) {
                pr_err("%s: power_off copy from user failed\n", __func__);
                return -EFAULT;
            }
            sipa_multi_channel_power_off(scene_data.pa_idx);
            break;
        }
        case SIPA_IOCTL_GET_CHANNEL: {
            int channel_num = sipa_get_channels();
            if (copy_to_user((void __user *)arg, &channel_num, sizeof(int))) {
                pr_err("%s: copy channel_num to user failed\n", __func__);
                return -EFAULT;
            }
            break;
        }
        case SIPA_IOCTL_REG_DUMP: {
            sipa_multi_channel_reg_dump();
            break;
        }
        default: {
            pr_err("%s: not support ioctl:0x%x\n", __func__, cmd);
            break;
        }
    }
    return ret;
}

struct file_operations sipa_turning_cmd_fops = {
    .owner = THIS_MODULE,
	.unlocked_ioctl = sipa_tuning_cmd_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_cmd_unlocked_ioctl,
};

static long sipa_tuning_tool_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
    sipa_turning_t *priv = g_sipa_turning;

	pr_info("[ info] %s: enter cmd:0x%x\n", __func__, cmd);
    return sipa_tuning_comm_ioctl(cmd, arg, &(priv->toolup), &(priv->tooldown));
}

struct file_operations sipa_turning_tool_fops = {
    .owner = THIS_MODULE,
	.unlocked_ioctl = sipa_tuning_tool_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_tool_unlocked_ioctl,
};
