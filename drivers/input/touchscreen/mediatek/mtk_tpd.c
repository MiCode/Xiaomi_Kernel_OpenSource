/******************************************************************************
 * mtk_tpd.c - MTK Android Linux Touch Panel Device Driver               *
 *                                                                            *
 * Copyright 2008-2009 MediaTek Co.,Ltd.                                      *
 *                                                                            *
 * DESCRIPTION:                                                               *
 *     this file provide basic touch panel event to input sub system          *
 *                                                                            *
 * AUTHOR:                                                                    *
 *     Kirby.Wu (mtk02247)                                                    *
 *                                                                            *
 * NOTE:                                                                      *
 * 1. Sensitivity for touch screen should be set to edge-sensitive.           *
 *    But in this driver it is assumed to be done by interrupt core,          *
 *    though not done yet. Interrupt core may provide interface to            *
 *    let drivers set the sensitivity in the future. In this case,            *
 *    this driver should set the sensitivity of the corresponding IRQ         *
 *    line itself.                                                            *
 ******************************************************************************/

#include "tpd.h"
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fb.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_50) \
	|| defined(CONFIG_MTK_S3320_47) || defined(CONFIG_MTK_MIT200) \
	|| defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) || defined(CONFIG_MTK_S7020) \
	|| defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
#include <linux/input/mt.h>
#endif /* CONFIG_MTK_S3320 */
/* for magnify velocity******************************************** */
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC, 0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC, 1)
#define TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, 2, struct tpd_filter_t)
#ifdef CONFIG_COMPAT
#define COMPAT_TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, 2, struct tpd_filter_t)
#endif
struct tpd_filter_t tpd_filter;
struct tpd_dts_info tpd_dts_data;
EXPORT_SYMBOL(tpd_dts_data);

struct pinctrl *pinctrl1;
struct pinctrl_state *pins_default;
struct pinctrl_state *eint_as_int, *eint_output0, *eint_output1, *rst_output0, *rst_output1;
struct of_device_id touch_of_match[] = {
	{ .compatible = "mediatek,mt6570-touch", },
	{ .compatible = "mediatek,mt6735-touch", },
	{ .compatible = "mediatek,mt6580-touch", },
	{ .compatible = "mediatek,mt8173-touch", },
	{ .compatible = "mediatek,mt6755-touch", },
	{ .compatible = "mediatek,mt6757-touch", },
	{ .compatible = "mediatek,mt6797-touch", },
	{ .compatible = "mediatek,mt8163-touch", },
	{ .compatible = "mediatek,mt8127-touch", },
	{ .compatible = "mediatek,mt2701-touch", },
	{ .compatible = "mediatek,mt7623-touch", },
	{},
};
EXPORT_SYMBOL(touch_of_match);

void tpd_get_dts_info(void)
{
	struct device_node *node1 = NULL;
	int key_dim_local[16], i;

	node1 = of_find_matching_node(node1, touch_of_match);
	if (node1) {
		of_property_read_u32(node1, "tpd-max-touch-num", &tpd_dts_data.touch_max_num);
		of_property_read_u32(node1, "use-tpd-button", &tpd_dts_data.use_tpd_button);
		pr_debug("[tpd]use-tpd-button = %d\n", tpd_dts_data.use_tpd_button);
		of_property_read_u32_array(node1, "tpd-resolution",
			tpd_dts_data.tpd_resolution, ARRAY_SIZE(tpd_dts_data.tpd_resolution));
		if (tpd_dts_data.use_tpd_button) {
			of_property_read_u32(node1, "tpd-key-num", &tpd_dts_data.tpd_key_num);
			of_property_read_u32_array(node1, "tpd-key-local",
				tpd_dts_data.tpd_key_local, ARRAY_SIZE(tpd_dts_data.tpd_key_local));
			of_property_read_u32_array(node1, "tpd-key-dim-local",
				key_dim_local, ARRAY_SIZE(key_dim_local));
			memcpy(tpd_dts_data.tpd_key_dim_local, key_dim_local, sizeof(key_dim_local));
			for (i = 0; i < 4; i++) {
				pr_debug("[tpd]key[%d].key_x = %d\n", i, tpd_dts_data.tpd_key_dim_local[i].key_x);
				pr_debug("[tpd]key[%d].key_y = %d\n", i, tpd_dts_data.tpd_key_dim_local[i].key_y);
				pr_debug("[tpd]key[%d].key_W = %d\n", i, tpd_dts_data.tpd_key_dim_local[i].key_width);
				pr_debug("[tpd]key[%d].key_H = %d\n", i, tpd_dts_data.tpd_key_dim_local[i].key_height);
			}
		}
		of_property_read_u32(node1, "tpd-filter-enable", &tpd_dts_data.touch_filter.enable);
		if (tpd_dts_data.touch_filter.enable) {
			of_property_read_u32(node1, "tpd-filter-pixel-density",
						&tpd_dts_data.touch_filter.pixel_density);
			of_property_read_u32_array(node1, "tpd-filter-custom-prameters",
				(u32 *)tpd_dts_data.touch_filter.W_W, ARRAY_SIZE(tpd_dts_data.touch_filter.W_W));
			of_property_read_u32_array(node1, "tpd-filter-custom-speed",
				tpd_dts_data.touch_filter.VECLOCITY_THRESHOLD,
				ARRAY_SIZE(tpd_dts_data.touch_filter.VECLOCITY_THRESHOLD));
		}
		memcpy(&tpd_filter, &tpd_dts_data.touch_filter, sizeof(tpd_filter));
		pr_debug("[tpd]tpd-filter-enable = %d, pixel_density = %d\n",
					tpd_filter.enable, tpd_filter.pixel_density);
	} else {
		pr_err("[tpd]%s can't find touch compatible custom node\n", __func__);
	}
}
EXPORT_SYMBOL(tpd_get_dts_info);

static DEFINE_MUTEX(tpd_set_gpio_mutex);
void tpd_gpio_as_int(int pin)
{
	mutex_lock(&tpd_set_gpio_mutex);
	TPD_DEBUG("[tpd]tpd_gpio_as_int\n");
	if (pin == 1)
		pinctrl_select_state(pinctrl1, eint_as_int);
	mutex_unlock(&tpd_set_gpio_mutex);
}
EXPORT_SYMBOL(tpd_gpio_as_int);

void tpd_gpio_output(int pin, int level)
{
	mutex_lock(&tpd_set_gpio_mutex);
	TPD_DEBUG("[tpd]tpd_gpio_output pin = %d, level = %d\n", pin, level);
	if (pin == 1) {
		if (level)
			pinctrl_select_state(pinctrl1, eint_output1);
		else
			pinctrl_select_state(pinctrl1, eint_output0);
	} else {
		if (level)
			pinctrl_select_state(pinctrl1, rst_output1);
		else
			pinctrl_select_state(pinctrl1, rst_output0);
	}
	mutex_unlock(&tpd_set_gpio_mutex);
}
EXPORT_SYMBOL(tpd_gpio_output);

int tpd_get_gpio_info(struct platform_device *pdev)
{
	int ret;

	TPD_DEBUG("[tpd %d] mt_tpd_pinctrl+++++++++++++++++\n", pdev->id);
	pinctrl1 = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl1)) {
		ret = PTR_ERR(pinctrl1);
		dev_err(&pdev->dev, "fwq Cannot find touch pinctrl1!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		/* dev_err(&pdev->dev, "fwq Cannot find touch pinctrl default %d!\n", ret);*/
	}
	eint_as_int = pinctrl_lookup_state(pinctrl1, "state_eint_as_int");
	if (IS_ERR(eint_as_int)) {
		ret = PTR_ERR(eint_as_int);
		dev_err(&pdev->dev, "fwq Cannot find touch pinctrl state_eint_as_int!\n");
		return ret;
	}
	eint_output0 = pinctrl_lookup_state(pinctrl1, "state_eint_output0");
	if (IS_ERR(eint_output0)) {
		ret = PTR_ERR(eint_output0);
		dev_err(&pdev->dev, "fwq Cannot find touch pinctrl state_eint_output0!\n");
		return ret;
	}
	eint_output1 = pinctrl_lookup_state(pinctrl1, "state_eint_output1");
	if (IS_ERR(eint_output1)) {
		ret = PTR_ERR(eint_output1);
		dev_err(&pdev->dev, "fwq Cannot find touch pinctrl state_eint_output1!\n");
		return ret;
	}
	rst_output0 = pinctrl_lookup_state(pinctrl1, "state_rst_output0");
	if (IS_ERR(rst_output0)) {
		ret = PTR_ERR(rst_output0);
		dev_err(&pdev->dev, "fwq Cannot find touch pinctrl state_rst_output0!\n");
		return ret;
	}
	rst_output1 = pinctrl_lookup_state(pinctrl1, "state_rst_output1");
	if (IS_ERR(rst_output1)) {
		ret = PTR_ERR(rst_output1);
		dev_err(&pdev->dev, "fwq Cannot find touch pinctrl state_rst_output1!\n");
		return ret;
	}
	TPD_DEBUG("[tpd%d] mt_tpd_pinctrl----------\n", pdev->id);
	return 0;
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
static long tpd_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	switch (cmd) {
	case COMPAT_TPD_GET_FILTER_PARA:
		if (arg32 == NULL) {
			pr_err("invalid argument.");
			return -EINVAL;
		}
		ret = file->f_op->unlocked_ioctl(file, TPD_GET_FILTER_PARA,
					   (unsigned long)arg32);
		if (ret) {
			pr_err("TPD_GET_FILTER_PARA unlocked_ioctl failed.");
			return ret;
		}
		break;
	default:
		pr_err("tpd: unknown IOCTL: 0x%08x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* char strbuf[256]; */
	void __user *data;

	long err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) {
		pr_err("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case TPD_GET_VELOCITY_CUSTOM_X:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data, &tpd_v_magnify_x, sizeof(tpd_v_magnify_x))) {
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

		if (copy_to_user(data, &tpd_v_magnify_y, sizeof(tpd_v_magnify_y))) {
			err = -EFAULT;
			break;
		}

		break;
	case TPD_GET_FILTER_PARA:
			data = (void __user *) arg;

			if (data == NULL) {
				err = -EINVAL;
				pr_err("tpd: TPD_GET_FILTER_PARA IOCTL CMD: data is null\n");
				break;
			}

			if (copy_to_user(data, &tpd_filter, sizeof(struct tpd_filter_t))) {
				pr_err("tpd: TPD_GET_FILTER_PARA IOCTL CMD: copy data error\n");
				err = -EFAULT;
				break;
			}
			break;
	default:
		pr_err("tpd: unknown IOCTL: 0x%08x\n", cmd);
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

/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &tpd_fops,
};

/* ********************************************** */
/* #endif */


/* function definitions */
static int tpd_probe(struct platform_device *pdev);
static int tpd_remove(struct platform_device *pdev);
static struct work_struct tpd_init_work;
static struct workqueue_struct *tpd_init_workqueue;
static int tpd_suspend_flag;
int tpd_register_flag = 0;
/* global variable definitions */
struct tpd_device *tpd = 0;
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
	TPD_DEBUG("GTP touch_resume_workqueue_callback\n");
	g_tpd_drv->resume(NULL);
	tpd_suspend_flag = 0;
}
static int tpd_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = NULL;
	int blank;
	int err = 0;

	TPD_DEBUG("tpd_fb_notifier_callback\n");

	evdata = data;
	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	TPD_DMESG("fb_notify(blank=%d)\n", blank);
	switch (blank) {
	case FB_BLANK_UNBLANK:
		TPD_DMESG("LCD ON Notify\n");
		if (g_tpd_drv && tpd_suspend_flag) {
			err = queue_work(touch_resume_workqueue, &touch_resume_work);
			if (!err) {
				TPD_DMESG("start touch_resume_workqueue failed\n");
				return err;
			}
		}
		break;
	case FB_BLANK_POWERDOWN:
		TPD_DMESG("LCD OFF Notify\n");
		if (g_tpd_drv && !tpd_suspend_flag) {
			err = cancel_work_sync(&touch_resume_work);
			if (!err)
				TPD_DMESG("cancel touch_resume_workqueue err = %d\n", err);
			g_tpd_drv->suspend(NULL);
		}
		tpd_suspend_flag = 1;
		break;
	default:
		break;
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
		if (strcmp(tpd_driver_list[i].tpd_device_name, tpd_drv->tpd_device_name) == 0) {
			memset(&tpd_driver_list[i], 0, sizeof(struct tpd_driver_t));
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(tpd_driver_remove);

static void tpd_create_attributes(struct device *dev, struct tpd_attrs *attrs)
{
	int num = attrs->num;

	for (; num > 0;)
		device_create_file(dev, attrs->attr[--num]);
}

/* touch panel probe */
static int tpd_probe(struct platform_device *pdev)
{
	int touch_type = 1;	/* 0:R-touch, 1: Cap-touch */
	int i = 0;
#ifndef CONFIG_CUSTOM_LCM_X
#ifdef CONFIG_LCM_WIDTH
	unsigned long tpd_res_x = 0, tpd_res_y = 0;
	int ret = 0;
#endif
#endif

	TPD_DMESG("enter %s, %d\n", __func__, __LINE__);

	if (misc_register(&tpd_misc_device))
		pr_err("mtk_tpd: tpd_misc_device register failed\n");
	tpd_get_gpio_info(pdev);
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
	/* TPD_RES_X = simple_strtoul(LCM_WIDTH, NULL, 0); */
	/* TPD_RES_Y = simple_strtoul(LCM_HEIGHT, NULL, 0); */

	#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION
	if (0 == strncmp(CONFIG_MTK_LCM_PHYSICAL_ROTATION, "90", 2)
		|| 0 == strncmp(CONFIG_MTK_LCM_PHYSICAL_ROTATION, "270", 3)) {
#ifdef CONFIG_MTK_FB	/*Fix build errors,as some projects  cannot support these apis while bring up*/
		TPD_RES_Y = DISP_GetScreenWidth();
		TPD_RES_X = DISP_GetScreenHeight();
#endif
	} else
    #endif
	{
#ifdef CONFIG_CUSTOM_LCM_X
#ifndef CONFIG_MTK_FPGA
#ifdef CONFIG_MTK_FB	/*Fix build errors,as some projects  cannot support these apis while bring up*/
		TPD_RES_X = DISP_GetScreenWidth();
		TPD_RES_Y = DISP_GetScreenHeight();
#else/*for some projects, we do not use mtk framebuffer*/
	TPD_RES_X = tpd_dts_data.tpd_resolution[0];
	TPD_RES_Y = tpd_dts_data.tpd_resolution[1];
#endif
#endif
#else
#ifdef CONFIG_LCM_WIDTH
		ret = kstrtoul(CONFIG_LCM_WIDTH, 0, &tpd_res_x);
		if (ret < 0) {
			pr_err("Touch down get lcm_x failed");
			return ret;
		}
		TPD_RES_X = tpd_res_x;
		ret = kstrtoul(CONFIG_LCM_HEIGHT, 0, &tpd_res_y);
		if (ret < 0) {
			pr_err("Touch down get lcm_y failed");
			return ret;
		}
		TPD_RES_Y = tpd_res_y;
#endif
#endif
	}

	if (2560 == TPD_RES_X)
		TPD_RES_X = 2048;
	if (1600 == TPD_RES_Y)
		TPD_RES_Y = 1536;
	pr_debug("mtk_tpd: TPD_RES_X = %lu, TPD_RES_Y = %lu\n", TPD_RES_X, TPD_RES_Y);

	tpd_mode = TPD_MODE_NORMAL;
	tpd_mode_axis = 0;
	tpd_mode_min = TPD_RES_Y / 2;
	tpd_mode_max = TPD_RES_Y;
	tpd_mode_keypad_tolerance = TPD_RES_X * TPD_RES_X / 1600;
	/* struct input_dev dev initialization and registration */
	tpd->dev->name = TPD_DEVICE;
	set_bit(EV_ABS, tpd->dev->evbit);
	set_bit(EV_KEY, tpd->dev->evbit);
	set_bit(ABS_X, tpd->dev->absbit);
	set_bit(ABS_Y, tpd->dev->absbit);
	set_bit(ABS_PRESSURE, tpd->dev->absbit);
#if !defined(CONFIG_MTK_S3320) && !defined(CONFIG_MTK_S3320_47)\
	&& !defined(CONFIG_MTK_S3320_50) && !defined(CONFIG_MTK_MIT200) \
	&& !defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) && !defined(CONFIG_MTK_S7020) \
	&& !defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
	set_bit(BTN_TOUCH, tpd->dev->keybit);
#endif /* CONFIG_MTK_S3320 */
	set_bit(INPUT_PROP_DIRECT, tpd->dev->propbit);

	/* save dev for regulator_get() before tpd_local_init() */
	tpd->tpd_dev = &pdev->dev;
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd driver into list */
		if (tpd_driver_list[i].tpd_device_name != NULL) {
			tpd_driver_list[i].tpd_local_init();
			/* msleep(1); */
			if (tpd_load_status == 1) {
				TPD_DMESG("[mtk-tpd]tpd_probe, tpd_driver_name=%s\n",
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
			TPD_DMESG("[mtk-tpd]Generic touch panel driver\n");
		} else {
			TPD_DMESG("[mtk-tpd]cap touch and Generic touch both are not loaded!!\n");
			return 0;
		}
	}
	touch_resume_workqueue = create_singlethread_workqueue("touch_resume");
	INIT_WORK(&touch_resume_work, touch_resume_workqueue_callback);
	/* use fb_notifier */
	tpd_fb_notifier.notifier_call = tpd_fb_notifier_callback;
	if (fb_register_client(&tpd_fb_notifier))
		TPD_DMESG("register fb_notifier fail!\n");
	/* TPD_TYPE_CAPACITIVE handle */
	if (touch_type == 1) {

		set_bit(ABS_MT_TRACKING_ID, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MAJOR, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MINOR, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_X, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_Y, tpd->dev->absbit);
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_47) \
	|| defined(CONFIG_MTK_S3320_50) || defined(CONFIG_MTK_MIT200) \
	|| defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) || defined(CONFIG_MTK_S7020) \
	|| defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
		input_set_abs_params(tpd->dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_WIDTH_MINOR, 0, 15, 0, 0);
		input_mt_init_slots(tpd->dev, 10, 0);
#else
		input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
#endif /* CONFIG_MTK_S3320 */
		TPD_DMESG("Cap touch panel driver\n");
	}
	input_set_abs_params(tpd->dev, ABS_X, 0, TPD_RES_X, 0, 0);
	input_set_abs_params(tpd->dev, ABS_Y, 0, TPD_RES_Y, 0, 0);
	input_abs_set_res(tpd->dev, ABS_X, TPD_RES_X);
	input_abs_set_res(tpd->dev, ABS_Y, TPD_RES_Y);
	input_set_abs_params(tpd->dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	if (input_register_device(tpd->dev))
		TPD_DMESG("input_register_device failed.(tpd)\n");
	else
		tpd_register_flag = 1;
	if (g_tpd_drv->tpd_have_button)
		tpd_button_init();

	if (g_tpd_drv->attrs.num)
		tpd_create_attributes(&pdev->dev, &g_tpd_drv->attrs);

	return 0;
}
EXPORT_SYMBOL(tpd_probe);
EXPORT_SYMBOL(tpd);

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
		TPD_DMESG("unable to register touch panel driver.\n");
}
/* Add driver: if find TPD_TYPE_CAPACITIVE driver successfully, loading it */
int tpd_driver_add(struct tpd_driver_t *tpd_drv)
{
	int i;

	if (g_tpd_drv != NULL) {
		TPD_DMESG("touch driver exist\n");
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
			tpd_driver_list[i].tpd_device_name = tpd_drv->tpd_device_name;
			tpd_driver_list[i].tpd_local_init = tpd_drv->tpd_local_init;
			tpd_driver_list[i].suspend = tpd_drv->suspend;
			tpd_driver_list[i].resume = tpd_drv->resume;
			tpd_driver_list[i].tpd_have_button = tpd_drv->tpd_have_button;
			tpd_driver_list[i].attrs = tpd_drv->attrs;
			break;
		}
		if (strcmp(tpd_driver_list[i].tpd_device_name, tpd_drv->tpd_device_name) == 0)
			return 1;	/* driver exist */
	}

	return 0;
}
EXPORT_SYMBOL(tpd_driver_add);


int tpd_device_init(void)
{
	int res = 0;

	tpd_init_workqueue = create_singlethread_workqueue("mtk-tpd");
	INIT_WORK(&tpd_init_work, tpd_init_work_callback);

	res = queue_work(tpd_init_workqueue, &tpd_init_work);
	if (!res)
		pr_err("tpd : touch device init failed res:%d\n", res);
	return 0;
}
EXPORT_SYMBOL(tpd_device_init);
/* should never be called */
void  tpd_device_exit(void)
{
	TPD_DMESG("MediaTek touch panel driver exit\n");
	/* input_unregister_device(tpd->dev); */
	platform_driver_unregister(&tpd_driver);
}
EXPORT_SYMBOL(tpd_device_exit);
