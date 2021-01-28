// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "tpd.h"
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

/* for magnify velocity******************************************** */
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC, 0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC, 1)
#define TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, 2, struct tpd_filter_t)
#ifdef CONFIG_COMPAT
#define COMPAT_TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, \
						2, struct tpd_filter_t)
#endif
struct tpd_filter_t tpd_filter;
struct tpd_dts_info tpd_dts_data;

const struct of_device_id touch_of_match[] = {
	{ .compatible = "goodix,touch", },
	{},
};

void tpd_get_dts_info(void)
{
	struct device_node *node1 = NULL;
	int key_dim_local[16], i;
	int convert_err = -EINVAL;

	node1 = of_find_matching_node(node1, touch_of_match);

	if (!node1) {
		TPD_ERR("can't find touch compatible custom node\n");
	} else {
		of_property_read_u32(node1,
			"tpd-max-touch-num", &tpd_dts_data.touch_max_num);

	of_property_read_u32_array(node1, "tpd-resolution",
		tpd_dts_data.tpd_resolution,
		ARRAY_SIZE(tpd_dts_data.tpd_resolution));
	TPD_DEBUG("[tpd] resulution is %d %d",
				tpd_dts_data.tpd_resolution[0],
				tpd_dts_data.tpd_resolution[1]);

#if defined(CONFIG_LCM_WIDTH) && defined(CONFIG_LCM_HEIGHT)
		convert_err = kstrtou32(CONFIG_LCM_WIDTH, 10,
			&tpd_dts_data.lcm_resolution[0]);
		if (convert_err)
			TPD_ERR("GET LCM WIDTH failed!\n");
		convert_err = kstrtou32(CONFIG_LCM_HEIGHT, 10,
			&tpd_dts_data.lcm_resolution[1]);
		if (convert_err)
			TPD_ERR("GET LCM HEIGHT failed!\n");
#else
		TPD_DEBUG("Set Default lcm-resolution!");
		of_property_read_u32_array(node1, "lcm-resolution",
			tpd_dts_data.lcm_resolution,
			ARRAY_SIZE(tpd_dts_data.lcm_resolution));
#endif

		TPD_DEBUG("[lcm] resulution is %d %d",
					tpd_dts_data.lcm_resolution[0],
					tpd_dts_data.lcm_resolution[1]);

		if (tpd_dts_data.use_tpd_button) {
			of_property_read_u32(node1,
				"tpd-key-num", &tpd_dts_data.tpd_key_num);
			if (of_property_read_u32_array(node1, "tpd-key-local",
				tpd_dts_data.tpd_key_local,
				ARRAY_SIZE(tpd_dts_data.tpd_key_local)))
				TPD_DEBUG("tpd-key-local: %d %d %d %d",
					tpd_dts_data.tpd_key_local[0],
					tpd_dts_data.tpd_key_local[1],
					tpd_dts_data.tpd_key_local[2],
					tpd_dts_data.tpd_key_local[3]);
			if (of_property_read_u32_array(node1,
				"tpd-key-dim-local",
				key_dim_local, ARRAY_SIZE(key_dim_local))) {
				memcpy(tpd_dts_data.tpd_key_dim_local,
					key_dim_local, sizeof(key_dim_local));
				for (i = 0; i < 4; i++) {
					pr_info("[tpd]key[%d].key_x = %d\n",
						i, tpd_dts_data
						.tpd_key_dim_local[i]
						.key_x);
					pr_info("[tpd]key[%d].key_y = %d\n",
						i, tpd_dts_data
						.tpd_key_dim_local[i]
						.key_y);
					pr_info("[tpd]key[%d].key_W = %d\n",
						i, tpd_dts_data
						.tpd_key_dim_local[i]
						.key_width);
					pr_info("[tpd]key[%d].key_H = %d\n",
						i, tpd_dts_data
						.tpd_key_dim_local[i]
						.key_height);
				}
			}
		}
		of_property_read_u32(node1, "tpd-filter-enable",
			&tpd_dts_data.touch_filter.enable);
		if (tpd_dts_data.touch_filter.enable) {
			of_property_read_u32(node1,
				"tpd-filter-pixel-density",
				&tpd_dts_data.touch_filter.pixel_density);
			if (of_property_read_u32_array(node1,
				"tpd-filter-custom-prameters",
				(u32 *)tpd_dts_data.touch_filter.W_W,
				ARRAY_SIZE(tpd_dts_data.touch_filter.W_W)))
				TPD_DEBUG("get tpd-filter-custom-parameters");
			if (of_property_read_u32_array(node1,
				"tpd-filter-custom-speed",
				tpd_dts_data.touch_filter.VECLOCITY_THRESHOLD,
				ARRAY_SIZE(tpd_dts_data
						.touch_filter
						.VECLOCITY_THRESHOLD)))
				TPD_DEBUG("get tpd-filter-custom-speed");
		}
		memcpy(&tpd_filter,
			&tpd_dts_data.touch_filter, sizeof(tpd_filter));
		TPD_DEBUG("[tpd]tpd-filter-enable = %d, pixel_density = %d\n",
				tpd_filter.enable, tpd_filter.pixel_density);
		tpd_dts_data.tpd_use_ext_gpio =
			of_property_read_bool(node1, "tpd-use-ext-gpio");
		of_property_read_u32(node1,
			"tpd-rst-ext-gpio-num",
			&tpd_dts_data.rst_ext_gpio_num);
		tpd_dts_data.eint_gpio_num = of_get_named_gpio(node1,
			"goodix,eint-gpio", 0);
		if (tpd_dts_data.eint_gpio_num < 0) {
			TPD_DEBUG("tpd Invalid eint-gpio in dt: %d\n",
				tpd_dts_data.eint_gpio_num);
		}
		tpd_dts_data.rst_gpio_num = of_get_named_gpio(node1,
			"goodix,reset-gpio", 0);
		if (tpd_dts_data.rst_gpio_num < 0) {
			TPD_DEBUG("tpd Invalid reset-gpio in dt: %d\n",
				tpd_dts_data.rst_gpio_num);
		}
		if (of_property_read_string(node1,
			"tpd-cfg-version", &tpd_dts_data.cfg_version))
			TPD_DEBUG("tpd-cfg-version: %s\n",
				tpd_dts_data.cfg_version);

		tpd_dts_data.x2x = of_property_read_bool(node1,
			"goodix,x2x");
		tpd_dts_data.y2y = of_property_read_bool(node1,
			"goodix,y2y");
	}
}

static int tpd_misc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int tpd_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long tpd_compat_ioctl(
			struct file *file, unsigned int cmd,
			unsigned long arg)
{
	long ret;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	switch (cmd) {
	case COMPAT_TPD_GET_FILTER_PARA:
		if (arg32 == NULL) {
			TPD_DEBUG("invalid argument.");
			return -EINVAL;
		}
		ret = file->f_op->unlocked_ioctl(file, TPD_GET_FILTER_PARA,
					   (unsigned long)arg32);
		if (ret) {
			TPD_DEBUG("TPD_GET_FILTER_PARA unlocked_ioctl failed.");
			return ret;
		}
		break;
	default:
		TPD_DEBUG("tpd: unknown IOCTL: 0x%08x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
static long tpd_unlocked_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	/* char strbuf[256]; */
	void __user *data;

	long err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
			(void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
			(void __user *)arg, _IOC_SIZE(cmd));
	if (err) {
		TPD_DEBUG("tpd: access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case TPD_GET_VELOCITY_CUSTOM_X:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd_v_magnify_x, sizeof(tpd_v_magnify_x))) {
			err = -EFAULT;
			break;
		}

		break;

	case TPD_GET_VELOCITY_CUSTOM_Y:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd_v_magnify_y, sizeof(tpd_v_magnify_y))) {
			err = -EFAULT;
			break;
		}

		break;
	case TPD_GET_FILTER_PARA:
			data = (void __user *) arg;

			if (data == NULL) {
				err = -EINVAL;
				TPD_ERR("GET_FILTER_PARA: data is null\n");
				break;
			}

			if (copy_to_user(data, &tpd_filter,
					sizeof(struct tpd_filter_t))) {
				TPD_ERR("GET_FILTER_PARA: copy data error\n");
				err = -EFAULT;
				break;
			}
			break;
	default:
		TPD_DEBUG("tpd: unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}
static struct work_struct touch_resume_work;
static struct workqueue_struct *touch_resume_workqueue;
static const struct file_operations tpd_fops = {
/* .owner = THIS_MODULE, */
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tpd_compat_ioctl,
#endif
};

/*---------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &tpd_fops,
};

/* ********************************************** */
/* #endif */


/* function definitions */
static int __init tpd_device_init(void);
static void __exit tpd_device_exit(void);
static int tpd_probe(struct platform_device *pdev);
static int tpd_remove(struct platform_device *pdev);
static struct work_struct tpd_init_work;
static struct workqueue_struct *tpd_init_workqueue;
static int tpd_suspend_flag;
int tpd_register_flag;
/* global variable definitions */
struct tpd_device *tpd;
static struct tpd_driver_t tpd_driver_list[TP_DRV_MAX_COUNT];	/* = {0}; */

struct platform_device tpd_device = {
	.name		= TPD_DEVICE,
	.id			= -1,
};
const struct dev_pm_ops tpd_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
};
static struct platform_driver tpd_driver = {
	.remove = tpd_remove,
	.shutdown = NULL,
	.probe = tpd_probe,
	.driver = {
			.name = TPD_DEVICE,
			.pm = &tpd_pm_ops,
			.owner = THIS_MODULE,
			.of_match_table = touch_of_match,
	},
};
static struct tpd_driver_t *g_tpd_drv;
/* hh: use fb_notifier */
static struct notifier_block tpd_fb_notifier;
/* use fb_notifier */
static void touch_resume_workqueue_callback(struct work_struct *work)
{
	TPD_DEBUG("GTP %s\n", __func__);
	g_tpd_drv->resume(NULL);
	tpd_suspend_flag = 0;
}
static int tpd_fb_notifier_callback(
			struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = NULL;
	int blank;
	int err = 0;

	TPD_DEBUG("%s\n", __func__);
	evdata = data;
	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	TPD_ERR("fb_notify(blank=%d)\n", blank);
	switch (blank) {
	case FB_BLANK_UNBLANK:
		TPD_DEBUG("LCD ON Notify\n");
		if (g_tpd_drv && tpd_suspend_flag) {
			#if defined(CONFIG_MACH_MT6779)
			//err = queue_work(touch_resume_workqueue,
			//			&touch_resume_work);
			#else
			  err = queue_work(touch_resume_workqueue,
						&touch_resume_work);
			#endif
			if (!err) {
				TPD_ERR("start resume_workqueue failed\n");
				return err;
			}
		}
		break;
	case FB_BLANK_POWERDOWN:
		TPD_DEBUG("LCD OFF Notify\n");
		if (g_tpd_drv && !tpd_suspend_flag) {
			#if defined(CONFIG_MACH_MT6779)
			//err = cancel_work_sync(&touch_resume_work);
			if (!err)
				TPD_ERR("cancel resume_workqueue failed\n");
			//g_tpd_drv->suspend(NULL);
			#else
			err = cancel_work_sync(&touch_resume_work);
			if (!err)
				TPD_ERR("cancel resume_workqueue failed\n");
			g_tpd_drv->suspend(NULL);
			#endif
		}
		tpd_suspend_flag = 1;
		break;
	default:
		break;
	}
	return 0;
}
/* Add driver: if find TPD_TYPE_CAPACITIVE driver successfully, loading it */
int tpd_driver_add(struct tpd_driver_t *tpd_drv)
{
	int i;

	if (g_tpd_drv != NULL) {
		TPD_ERR("touch driver exist\n");
		return -1;
	}
	/* check parameter */
	if (tpd_drv == NULL)
		return -1;
	tpd_drv->tpd_have_button = tpd_dts_data.use_tpd_button;
	/* R-touch */
	if (strcmp(tpd_drv->tpd_device_name, "generic") == 0) {
		tpd_driver_list[0].tpd_device_name = tpd_drv->tpd_device_name;
		tpd_driver_list[0].tpd_local_init = tpd_drv->tpd_local_init;
		tpd_driver_list[0].suspend = tpd_drv->suspend;
		tpd_driver_list[0].resume = tpd_drv->resume;
		tpd_driver_list[0].tpd_have_button = tpd_drv->tpd_have_button;
		return 0;
	}
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd driver into list */
		if (tpd_driver_list[i].tpd_device_name == NULL) {
			tpd_driver_list[i].tpd_device_name =
				tpd_drv->tpd_device_name;
			tpd_driver_list[i].tpd_local_init =
				tpd_drv->tpd_local_init;
			tpd_driver_list[i].suspend = tpd_drv->suspend;
			tpd_driver_list[i].resume = tpd_drv->resume;
			tpd_driver_list[i].tpd_have_button =
				tpd_drv->tpd_have_button;
			tpd_driver_list[i].attrs = tpd_drv->attrs;
			break;
		}
		if (strcmp(tpd_driver_list[i].tpd_device_name,
			tpd_drv->tpd_device_name) == 0)
			return 1;	/* driver exist */
	}

	return 0;
}

int tpd_driver_remove(struct tpd_driver_t *tpd_drv)
{
	int i = 0;
	/* check parameter */
	if (tpd_drv == NULL)
		return -1;
	for (i = 0; i < TP_DRV_MAX_COUNT; i++) {
		/* find it */
		if (strcmp(tpd_driver_list[i].tpd_device_name,
				tpd_drv->tpd_device_name) == 0) {
			memset(&tpd_driver_list[i], 0,
				sizeof(struct tpd_driver_t));
			break;
		}
	}
	return 0;
}

static void tpd_create_attributes(struct device *dev, struct tpd_attrs *attrs)
{
	int num = attrs->num;

	for (; num > 0;) {
		if (device_create_file(dev, attrs->attr[--num]))
			TPD_DEBUG("tpd create attributes file failed\n");
	}
}

/* touch panel probe */
static int tpd_probe(struct platform_device *pdev)
{
	int touch_type = 1;	/* 0:R-touch, 1: Cap-touch */
	int i = 0;

	TPD_DEBUG("enter %s, %d\n", __func__, __LINE__);

	tpd = kmalloc(sizeof(struct tpd_device), GFP_KERNEL);
	if (tpd == NULL)
		return -ENOMEM;
	memset(tpd, 0, sizeof(struct tpd_device));

	/* allocate input device */
	tpd->dev = input_allocate_device();
	if (tpd->dev == NULL) {
		kfree(tpd);
		return -ENOMEM;
	}

	tpd_mode = TPD_MODE_NORMAL;
	tpd_mode_axis = 0;
	tpd_mode_min = tpd_dts_data.lcm_resolution[1] / 2;
	tpd_mode_max = tpd_dts_data.lcm_resolution[1];
	tpd_mode_keypad_tolerance = tpd_dts_data.lcm_resolution[0] *
		tpd_dts_data.lcm_resolution[0] / 1600;
	/* save dev for regulator_get() before tpd_local_init() */
	tpd->tpd_dev = &pdev->dev;
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd driver into list */
		if (tpd_driver_list[i].tpd_device_name != NULL) {
			tpd_driver_list[i].tpd_local_init();
			/* msleep(1); */
			if (tpd_load_status == 1) {
				TPD_DEBUG("%s, tpd_driver_name=%s\n", __func__,
					  tpd_driver_list[i].tpd_device_name);
				g_tpd_drv = &tpd_driver_list[i];
				break;
			}
		}
	}
	if (g_tpd_drv == NULL) {
		if (tpd_driver_list[0].tpd_device_name != NULL) {
			g_tpd_drv = &tpd_driver_list[0];
			/* touch_type:0: r-touch, 1: C-touch */
			touch_type = 0;
			g_tpd_drv->tpd_local_init();
			TPD_DEBUG("Generic touch panel driver\n");
		} else {
			TPD_ERR("no touch driver is loaded!!\n");
			return 0;
		}
	}
	touch_resume_workqueue = create_singlethread_workqueue("touch_resume");
	INIT_WORK(&touch_resume_work, touch_resume_workqueue_callback);
	/* use fb_notifier */
	tpd_fb_notifier.notifier_call = tpd_fb_notifier_callback;
	if (fb_register_client(&tpd_fb_notifier))
		TPD_ERR("register fb_notifier fail!\n");

	/* TPD_TYPE_CAPACITIVE handle */
	if (touch_type == 1) {
		/* struct input_dev dev initialization and registration */
		tpd->dev->name = TPD_INPUT_DEVICE;
		set_bit(EV_SYN, tpd->dev->evbit);
		set_bit(EV_ABS, tpd->dev->evbit);
		set_bit(EV_KEY, tpd->dev->evbit);
		set_bit(BTN_TOUCH, tpd->dev->keybit);
		set_bit(BTN_TOOL_FINGER, tpd->dev->keybit);
		set_bit(ABS_MT_TRACKING_ID, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MAJOR, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MINOR, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_X, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_Y, tpd->dev->absbit);
		set_bit(INPUT_PROP_DIRECT, tpd->dev->propbit);
		input_set_abs_params(tpd->dev,
			ABS_MT_POSITION_X, 0,
			tpd_dts_data.lcm_resolution[0], 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_POSITION_Y, 0,
			tpd_dts_data.lcm_resolution[1], 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
		TPD_DEBUG("Cap touch panel driver\n");
	}

	if (input_register_device(tpd->dev))
		TPD_ERR("input_register_device failed.(tpd)\n");
	else
		tpd_register_flag = 1;

	if (g_tpd_drv->tpd_have_button)
		tpd_button_init();

	if (g_tpd_drv->attrs.num)
		tpd_create_attributes(&pdev->dev, &g_tpd_drv->attrs);

	if (misc_register(&tpd_misc_device))
		TPD_ERR("mtk_tpd: tpd_misc_device register failed\n");

	return 0;
}
static int tpd_remove(struct platform_device *pdev)
{
	input_unregister_device(tpd->dev);
	return 0;
}

/* called when loaded into kernel */
static void tpd_init_work_callback(struct work_struct *work)
{
	TPD_DEBUG("MediaTek touch panel driver init\n");
	if (platform_driver_register(&tpd_driver) != 0)
		TPD_ERR("unable to register touch panel driver.\n");
}
static int __init tpd_device_init(void)
{
	int res = 0;
	/* load touch driver first  */
	gt1x_driver_init();
	tpd_log_init();

	tpd_init_workqueue = create_singlethread_workqueue("mtk-tpd");
	INIT_WORK(&tpd_init_work, tpd_init_work_callback);

	res = queue_work(tpd_init_workqueue, &tpd_init_work);
	if (!res)
		TPD_ERR("tpd : touch device init failed res:%d\n", res);
	return 0;
}

static void __exit tpd_device_exit(void)
{
	tpd_log_exit();
	gt1x_generic_exit();
	gt1x_tool_exit();
	misc_deregister(&tpd_misc_device);
	fb_unregister_client(&tpd_fb_notifier);
	gt1x_driver_exit();
	cancel_work_sync(&tpd_init_work);
	destroy_workqueue(tpd_init_workqueue);
	platform_driver_unregister(&tpd_driver);
	TPD_DEBUG("Touch exit done\n");
}

late_initcall(tpd_device_init);
module_exit(tpd_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch panel driver");
