/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <mt-plat/sync_write.h>

#include "n3d.h"
#include "n3d_if.h"
#include "n3d_util.h"
#include "kd_seninf_n3d.h"
#include "vsync_recorder.h"
#include "frame-sync/frame_sync_camsys.h"

#include "kd_imgsensor_errcode.h"
#include <linux/delay.h>

static const char * const n3d_names[] = {
	SENINF_N3D_NAMES
};

static struct SENINF_N3D gn3d;

static int fs_callback_fl_result(void *p_ctx,
				 unsigned int sensor_idx,
				 unsigned int framelength)
{
	struct SENINF_N3D *pn3d = (struct SENINF_N3D *)p_ctx;

	mutex_lock(&pn3d->n3d_mutex);

	if (sensor_idx < MAX_NUM_OF_SUPPORT_SENSOR) {
		LOG_D("fl result sensor idx = %u, fl = %u\n",
		      sensor_idx, framelength);
		pn3d->fl_result[sensor_idx] = framelength;
	}

	mutex_unlock(&pn3d->n3d_mutex);

	return 0;
}

static int n3d_power_on(void)
{
	struct SENINF_N3D *pn3d = &gn3d;

	LOG_D("E\n");

	//mutex_lock(&pn3d->n3d_mutex);
	if (atomic_inc_return(&pn3d->n3d_open_cnt) == 1)
		n3d_clk_open(&pn3d->clk);

	LOG_D("%d\n", atomic_read(&pn3d->n3d_open_cnt));

	//mutex_unlock(&pn3d->n3d_mutex);

	return 0;
}

static int n3d_power_off(void)
{
	struct SENINF_N3D *pn3d = &gn3d;
	//int i;

	//mutex_lock(&pn3d->n3d_mutex);
	if (atomic_dec_and_test(&pn3d->n3d_open_cnt)) {
		n3d_clk_release(&pn3d->clk);
		//for (i = 0; i < ARRAY_SIZE(pn3d->sync_sensors); i++) {
		//	if (pn3d->sync_sensors[i])
		//		kfree(pn3d->sync_sensors[i]);
		//	pn3d->sync_sensors[i] = NULL;
		//}
	}

	LOG_D("%d\n", atomic_read(&pn3d->n3d_open_cnt));
	//mutex_unlock(&pn3d->n3d_mutex);

	return 0;
}

static void sensor_info_converting(struct fs_streaming_st *st,
				   struct sensor_info *info)
{
	st->sensor_id = info->sensor_id;
	st->sensor_idx = info->sensor_idx;
	st->tg = info->cammux_id + 1;

	st->fl_active_delay = info->fl_active_delay;
	st->def_fl_lc = info->def_fl_lc;
	st->max_fl_lc = info->max_fl_lc;
	st->def_shutter_lc = info->def_shutter_lc;

	/* callback function */
	st->func_ptr = fs_callback_fl_result;
	st->p_ctx = (void *)&gn3d;
}

static int register_sensor(struct sensor_info *psensor)
{
	struct fs_streaming_st st;
	struct SENINF_N3D *pn3d = &gn3d;
	struct sensor_info *info;

	mutex_lock(&pn3d->n3d_mutex);

	if (psensor->sensor_idx < MAX_NUM_OF_SUPPORT_SENSOR) {
		if (pn3d->sync_sensors[psensor->sensor_idx] != NULL)
			kfree(pn3d->sync_sensors[psensor->sensor_idx]);
		info = kmalloc(sizeof(struct sensor_info), GFP_KERNEL);
		if (!info)
			return -1;
		memcpy(info, psensor, sizeof(struct sensor_info));
		pn3d->sync_sensors[psensor->sensor_idx] = info;
		pn3d->fl_result[psensor->sensor_idx] = 0;

		sensor_info_converting(&st, psensor);
		pn3d->fsync_mgr->fs_streaming(1, &st);

		LOG_D("register sensor index = %u, cammux = %u\n",
		       info->sensor_idx, info->cammux_id);
	}

	mutex_unlock(&pn3d->n3d_mutex);

	return 0;
}

static int unregister_sensor(struct sensor_info *psensor)
{
	struct SENINF_N3D *pn3d = &gn3d;
	struct fs_streaming_st st;

	mutex_lock(&pn3d->n3d_mutex);

	if (psensor->sensor_idx < MAX_NUM_OF_SUPPORT_SENSOR) {
		if (pn3d->sync_sensors[psensor->sensor_idx] != NULL) {
			sensor_info_converting(&st, psensor);
			pn3d->fsync_mgr->fs_streaming(0, &st);
			pn3d->fl_result[psensor->sensor_idx] = 0;
			kfree(pn3d->sync_sensors[psensor->sensor_idx]);
		}
		LOG_D("unregister sensor index = %u\n",
		      psensor->sensor_idx);
	}

	mutex_unlock(&pn3d->n3d_mutex);

	return 0;
}

static int start_sync(void)
{
#define SYNC_NUM 2
	struct SENINF_N3D *pn3d = &gn3d;
	unsigned int i, sync_num;
	unsigned int sync_idx[SYNC_NUM];

	LOG_D("start sync\n");

	for (i = 0, sync_num = 0;
	     (i < ARRAY_SIZE(pn3d->sync_sensors)) && (sync_num < SYNC_NUM);
	     i++) {
		if (pn3d->sync_sensors[i] && pn3d->sensor_streaming[i]) {
			sync_idx[sync_num] = i;
			sync_num++;
		}
	}

	if (sync_num == SYNC_NUM) {
		//enable_irq(pn3d->irq_id);
		n3d_power_on();
		reset_recorder(pn3d->sync_sensors[sync_idx[0]]->cammux_id,
			       pn3d->sync_sensors[sync_idx[1]]->cammux_id);
		set_n3d_source(&pn3d->regs,
			       pn3d->sync_sensors[sync_idx[0]],
			       pn3d->sync_sensors[sync_idx[1]]);
		if (pn3d->fsync_mgr != NULL) {
			pn3d->fsync_mgr->fs_set_sync(sync_idx[0], 1);
			pn3d->fsync_mgr->fs_set_sync(sync_idx[1], 1);
		}
	} else
		LOG_W("fail to start sync due to sync_num is %d\n", sync_num);

	return 0;
}

static int stop_sync(void)
{
	struct SENINF_N3D *pn3d = &gn3d;
	int i;

	for (i = 0;
	     i < ARRAY_SIZE(pn3d->sync_sensors);
	     i++) {
		if (pn3d->sync_sensors[i])
			pn3d->fsync_mgr->fs_set_sync(i, 0);
	}

	//disable_irq(pn3d->irq_id);
	n3d_power_off();
	disable_n3d(&pn3d->regs);

	return 0;
}

static void copy_from_perframe(struct fs_perframe_st *t,
				struct n3d_perframe *s)
{
	t->sensor_id = s->sensor_id;
	t->sensor_idx = s->sensor_idx;

	t->min_fl_lc = s->min_fl_lc;
	t->shutter_lc = s->shutter_lc;
	t->margin_lc = s->margin_lc;
	t->flicker_en = s->flicker_en;
	t->out_fl_lc = s->out_fl_lc;

	/* for on-the-fly mode change */
	t->pclk = s->pclk;
	t->linelength = s->linelength;
	/* lineTimeInNs ~= 10^9 * (linelength/pclk) */
	t->lineTimeInNs = s->lineTimeInNs;

	/* callback function using */
	t->cmd_id = s->sensor_idx;
}

static void copy_to_perframe(struct n3d_perframe *t,
			     struct fs_perframe_st *s)
{
	t->sensor_id = s->sensor_id;
	t->sensor_idx = s->sensor_idx;

	t->min_fl_lc = s->min_fl_lc;
	t->shutter_lc = s->shutter_lc;
	t->margin_lc = s->margin_lc;
	t->flicker_en = s->flicker_en;
	t->out_fl_lc = s->out_fl_lc;

	/* for on-the-fly mode change */
	t->pclk = s->pclk;
	t->linelength = s->linelength;
	/* lineTimeInNs ~= 10^9 * (linelength/pclk) */
	t->lineTimeInNs = s->lineTimeInNs;
}

static int update_ae_info(struct n3d_perframe *perframe)
{
	struct SENINF_N3D *pn3d = &gn3d;
	struct fs_perframe_st per;

	copy_from_perframe(&per, perframe);

	pn3d->fsync_mgr->fs_set_shutter(&per);

	copy_to_perframe(perframe, &per);

	/* update output frame length */
	if (per.sensor_idx < MAX_NUM_OF_SUPPORT_SENSOR)
		perframe->out_fl_lc = pn3d->fl_result[per.sensor_idx];

	return 0;
}

static int per_frame_ctrl(struct n3d_perframe *perframe1,
			  struct n3d_perframe *perframe2)
{
	struct SENINF_N3D *pn3d = &gn3d;
	struct fs_perframe_st per1, per2;

	copy_from_perframe(&per1, perframe1);
	copy_from_perframe(&per2, perframe2);

	fs_sync_frame(1);

	pn3d->fsync_mgr->fs_set_shutter(&per1);
	pn3d->fsync_mgr->fs_set_shutter(&per2);

	fs_sync_frame(0);

	copy_to_perframe(perframe1, &per1);
	copy_to_perframe(perframe2, &per2);

	/* update output frame length */
	if (per1.sensor_idx < MAX_NUM_OF_SUPPORT_SENSOR)
		perframe1->out_fl_lc = pn3d->fl_result[per1.sensor_idx];
	if (per2.sensor_idx < MAX_NUM_OF_SUPPORT_SENSOR)
		perframe2->out_fl_lc = pn3d->fl_result[per2.sensor_idx];

	LOG_D("fl_result1(%d) = %u, fl_result2(%d) = %u\n",
	      per1.sensor_idx, perframe1->out_fl_lc,
	      per2.sensor_idx, perframe2->out_fl_lc);

	return 0;
}

static irqreturn_t n3d_irq(int Irq, void *DeviceId)
{
	struct SENINF_N3D *pn3d = &gn3d;

	if (read_status(&pn3d->regs))
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static int n3d_open(struct inode *pInode, struct file *pFile)
{
	return 0;
}

static int n3d_release(struct inode *pInode, struct file *pFile)
{
	return 0;
}

static long n3d_ioctl(struct file *pfile,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void *pbuff = NULL;
	struct SENINF_N3D *pn3d = &gn3d;

	if (_IOC_DIR(cmd) != _IOC_NONE) {
		pbuff = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
		if (pbuff == NULL) {
			LOG_E("ioctl allocate mem failed\n");
			ret = -ENOMEM;
			goto N3D_IOCTL_EXIT;
		}

		if (_IOC_WRITE & _IOC_DIR(cmd)) {
			if (copy_from_user(pbuff,
						(void *)arg, _IOC_SIZE(cmd))) {
				kfree(pbuff);
				LOG_E("ioctl copy from user failed\n");
				ret = -EFAULT;
				goto N3D_IOCTL_EXIT;
			}
		} else
			memset(pbuff, 0, _IOC_SIZE(cmd));
	} else if (cmd == KDSENINFN3DIOC_X_START_SYNC) {
		/* skip copy data due to no arguments */
	} else if (cmd == KDSENINFN3DIOC_X_STOP_SYNC) {
		/* skip copy data due to no arguments */
	} else {
		ret = -EFAULT;
		goto N3D_IOCTL_EXIT;
	}

	switch (cmd) {
	case KDSENINFN3DIOC_X_REGISTER_SENSOR:
		register_sensor(&((struct KD_REGISTER_SENSOR *) pbuff)->sensor);
		mutex_lock(&pn3d->n3d_mutex);
		if (pn3d->sync_state) {
			LOG_D("start sync by reg ctrl\n");
			start_sync();
		}
		mutex_unlock(&pn3d->n3d_mutex);
		break;

	case KDSENINFN3DIOC_X_UNREGISTER_SENSOR:
		unregister_sensor(&((struct KD_REGISTER_SENSOR *) pbuff)->sensor);
		break;

	case KDSENINFN3DIOC_X_START_SYNC:
		mutex_lock(&pn3d->n3d_mutex);
		LOG_D("start sync by start ctrl\n");
		if (!pn3d->sync_state) {
			start_sync();
			pn3d->sync_state = 1;
		}
		mutex_unlock(&pn3d->n3d_mutex);
		break;

	case KDSENINFN3DIOC_X_STOP_SYNC:
		mutex_lock(&pn3d->n3d_mutex);
		if (pn3d->sync_state) {
			stop_sync();
			pn3d->sync_state = 0;
		}
		mutex_unlock(&pn3d->n3d_mutex);
		break;

	case KDSENINFN3DIOC_X_UPDATE_AE_INFO:
		update_ae_info(&((struct KD_N3D_AE_INFO *) pbuff)->ae_info);
		break;

	case KDSENINFN3DIOC_X_PERFRAME_CTRL:
		per_frame_ctrl(&((struct KD_N3D_PERFRAME *) pbuff)->per1,
			       &((struct KD_N3D_PERFRAME *) pbuff)->per2);
		break;

	default:
		LOG_W("No such command %d\n", cmd);
		ret = -EPERM;
		break;
	}

	if ((_IOC_READ & _IOC_DIR(cmd)) && copy_to_user((void __user *)arg,
			pbuff, _IOC_SIZE(cmd))) {
		kfree(pbuff);
		LOG_E("[CAMERA SENSOR] ioctl copy to user failed\n");
		ret = -EFAULT;
		goto N3D_IOCTL_EXIT;
	}

	kfree(pbuff);

N3D_IOCTL_EXIT:

	return ret;
}

void set_sensor_streaming_state(int sensor_idx, int state)
{
	struct SENINF_N3D *pn3d = &gn3d;

	mutex_lock(&pn3d->n3d_mutex);

	LOG_D("sidx = %d, state = %d\n", sensor_idx, state);

	if (sensor_idx < MAX_NUM_OF_SUPPORT_SENSOR)
		pn3d->sensor_streaming[sensor_idx] = state;

	if (pn3d->sync_state) {
		if (state) {
			LOG_D("start sync by stream on\n");
			start_sync();
		} else
			stop_sync();
	}

	mutex_unlock(&pn3d->n3d_mutex);
}

#ifdef CONFIG_COMPAT
static long n3d_ioctl_compat(struct file *pfile,
	unsigned int cmd, unsigned long arg)
{
	if (!pfile->f_op || !pfile->f_op->unlocked_ioctl)
		return -ENOTTY;

	return pfile->f_op->unlocked_ioctl(pfile, cmd, arg);
}
#endif

static const struct file_operations gn3d_file_operations = {
	.owner          = THIS_MODULE,
	.open           = n3d_open,
	.release        = n3d_release,
	.unlocked_ioctl = n3d_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = n3d_ioctl_compat,
#endif
};

static inline void n3d_unreg_char_dev(struct SENINF_N3D *pn3d)
{
	LOG_D("- E.");

	/* Release char driver */
	if (pn3d->pchar_dev != NULL) {
		cdev_del(pn3d->pchar_dev);
		pn3d->pchar_dev = NULL;
	}

	unregister_chrdev_region(pn3d->dev_no, 1);
}

static inline int n3d_reg_char_dev(struct SENINF_N3D *pn3d)
{
	int ret = 0;

#ifdef CONFIG_OF
	struct device *dev = NULL;
#endif

	LOG_D("- E.\n");

	ret = alloc_chrdev_region(&pn3d->dev_no, 0, 1, N3D_DEV_NAME);
	if (ret < 0) {
		LOG_E("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}
	/* Allocate driver */
	pn3d->pchar_dev = cdev_alloc();
	if (pn3d->pchar_dev == NULL) {
		LOG_E("cdev_alloc failed\n");
		ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pn3d->pchar_dev, &gn3d_file_operations);

	pn3d->pchar_dev->owner = THIS_MODULE;
	/* Add to system */
	if (cdev_add(pn3d->pchar_dev, pn3d->dev_no, 1) < 0) {
		LOG_E("Attatch file operation failed, %d\n", ret);
		goto EXIT;
	}

	/* Create class register */
	pn3d->pclass = class_create(THIS_MODULE, N3D_DEV_NAME);
	if (IS_ERR(pn3d->pclass)) {
		ret = PTR_ERR(pn3d->pclass);
		LOG_E("Unable to create class, err = %d\n", ret);
		goto EXIT;
	}

	dev = device_create(pn3d->pclass,
				NULL,
				pn3d->dev_no,
				NULL,
				N3D_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		dev_err(dev, "Failed to create device: /dev/%s, err = %d",
			N3D_DEV_NAME, ret);
		goto EXIT;
	}

EXIT:
	if (ret < 0)
		n3d_unreg_char_dev(pn3d);

	LOG_D("- X.\n");
	return ret;
}

static int n3d_probe(struct platform_device *pDev)
{
	struct SENINF_N3D *pn3d = &gn3d;
	struct resource *res;
	int ret = 0;
	unsigned int irq_info[3];	/* Record interrupts info from device tree */
	int irq, i;

	n3d_reg_char_dev(pn3d);

	mutex_init(&pn3d->n3d_mutex);
	atomic_set(&pn3d->n3d_open_cnt, 0);

	pn3d->clk.pplatform_device = pDev;
	n3d_clk_init(&pn3d->clk);

	pn3d->sync_state = 0;
	for (i = 0; i < ARRAY_SIZE(pn3d->sync_sensors); i++) {
		pn3d->sync_sensors[i] = NULL;
		pn3d->sensor_streaming[i] = 0;
	}

	ret = FrameSyncInit(&pn3d->fsync_mgr);
	if (ret != 0) {
		dev_err(&pDev->dev, "frame-sync init failed !\n");
		pn3d->fsync_mgr = NULL;
	}

	/* reg base address */
	res = platform_get_resource_byname(pDev, IORESOURCE_MEM, "seninf_top");
	pn3d->regs.pseninf_top_base = devm_ioremap_resource(&pDev->dev, res);
	if (IS_ERR(pn3d->regs.pseninf_top_base)) {
		dev_err(&pDev->dev, "get seninf_top base address error\n");
		return PTR_ERR(pn3d->regs.pseninf_top_base);
	}

	for (i = 0; i < ARRAY_SIZE(n3d_names); i++) {
		res = platform_get_resource_byname(pDev, IORESOURCE_MEM, n3d_names[i]);
		pn3d->regs.pseninf_n3d_base[i] = devm_ioremap_resource(&pDev->dev, res);
		if (IS_ERR(pn3d->regs.pseninf_n3d_base[i])) {
			dev_err(&pDev->dev, "get n3d base address error\n");
			return PTR_ERR(pn3d->regs.pseninf_n3d_base[i]);
		}
	}
	mutex_init(&pn3d->regs.reg_mutex);

	/* get IRQ ID and request IRQ */
	irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array
				(pDev->dev.of_node,
				"interrupts",
				irq_info,
				ARRAY_SIZE(irq_info))) {
			dev_err(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		ret = request_irq(irq,
				(irq_handler_t) n3d_irq,
				irq_info[2],
				"SENINF_N3D",
				NULL);
		if (ret) {
			dev_err(&pDev->dev, "request_irq fail\n");
			return ret;
		}

		pn3d->irq_id = irq;
		LOG_D("devnode(%s), irq=%d\n", pDev->dev.of_node->name, irq);
	} else {
		LOG_D("No IRQ!!\n");
	}

	return ret;
}

static int n3d_remove(struct platform_device *pDev)
{
	struct SENINF_N3D *pn3d = &gn3d;

	LOG_D("- E.");
	/* unregister char driver. */
	n3d_unreg_char_dev(pn3d);

	/* Release IRQ */
	free_irq(platform_get_irq(pDev, 0), NULL);

	device_destroy(pn3d->pclass, pn3d->dev_no);

	class_destroy(pn3d->pclass);
	pn3d->pclass = NULL;

	return 0;
}

static int n3d_suspend(struct platform_device *pDev, pm_message_t mesg)
{
	return 0;
}

static int n3d_resume(struct platform_device *pDev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gn3d_of_device_id[] = {
	{.compatible = "mediatek,seninf_n3d_top",},
	{}
};
#endif

static struct platform_driver gn3d_platform_driver = {
	.probe = n3d_probe,
	.remove = n3d_remove,
	.suspend = n3d_suspend,
	.resume = n3d_resume,
	.driver = {
			.name = N3D_DEV_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = gn3d_of_device_id,
#endif
			}
};

static int __init n3d_init(void)
{
	if (platform_driver_register(&gn3d_platform_driver) < 0) {
		LOG_E("platform_driver_register fail");
		return -ENODEV;
	}

	return 0;
}

static void __exit n3d_exit(void)
{
	platform_driver_unregister(&gn3d_platform_driver);
}

module_init(n3d_init);
module_exit(n3d_exit);

MODULE_DESCRIPTION("n3d fsync driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");

