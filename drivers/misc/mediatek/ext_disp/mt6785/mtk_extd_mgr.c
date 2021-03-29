/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
/* #include <linux/rtpm_prio.h> */
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/fb.h>

#include <linux/uaccess.h>
#include <linux/atomic.h>

#include "extd_log.h"
#include "extd_utils.h"
#include "extd_factory.h"
#include "mtk_extd_mgr.h"
#include <linux/suspend.h>
#include "extd_platform.h"


#define EXTD_DEVNAME		"hdmitx"
#define EXTD_DEV_ID(id)		(((id)>>16)&0x0ff)
#define EXTD_DEV_PARAM(id)	((id)&0x0ff)

static dev_t extd_devno;
static struct cdev *extd_cdev;
static struct class *extd_class;

static const struct EXTD_DRIVER *extd_driver[DEV_MAX_NUM];
static const struct EXTD_DRIVER *extd_factory_driver[DEV_MAX_NUM - 1];

static void external_display_enable(unsigned long param)
{
	enum EXTD_DEV_ID device_id = EXTD_DEV_ID(param);
	int enable = EXTD_DEV_PARAM(param);

	if (device_id >= DEV_MAX_NUM) {
		EXT_MGR_ERR("%s, device id is invalid!", __func__);
		return;
	}

	if (extd_driver[device_id] && extd_driver[device_id]->enable)
		extd_driver[device_id]->enable(enable);
}

static void external_display_power_enable(unsigned long param)
{
	enum EXTD_DEV_ID device_id = EXTD_DEV_ID(param);
	int enable = EXTD_DEV_PARAM(param);

	if (device_id >= DEV_MAX_NUM) {
		EXT_MGR_ERR("%s: device id is invalid!", __func__);
		return;
	}

	if (extd_driver[device_id] && extd_driver[device_id]->power_enable)
		extd_driver[device_id]->power_enable(enable);
}

static void external_display_set_resolution(unsigned long param)
{
	enum EXTD_DEV_ID device_id = EXTD_DEV_ID(param);
	int res = EXTD_DEV_PARAM(param);

	if (device_id >= DEV_MAX_NUM) {
		EXT_MGR_ERR("%s: device id is invalid!", __func__);
		return;
	}

	if (extd_driver[device_id] && extd_driver[device_id]->set_resolution)
		extd_driver[device_id]->set_resolution(res);
}

static int external_display_get_dev_info(unsigned long param, void *info)
{
	int ret = 0;
	enum EXTD_DEV_ID device_id = EXTD_DEV_ID(param);

	if (device_id >= DEV_MAX_NUM) {
		EXT_MGR_ERR("%s: device id is invalid!", __func__);
		return ret;
	}

	if (extd_driver[device_id] && extd_driver[device_id]->get_dev_info)
		ret = extd_driver[device_id]->get_dev_info(AP_GET_INFO, info);

	return ret;
}

static int external_display_get_capability(unsigned long param, void *info)
{
	int ret = 0;
	enum EXTD_DEV_ID device_id = EXTD_DEV_ID(param);

	device_id = DEV_MHL;

	if (device_id >= DEV_MAX_NUM) {
		EXT_MGR_ERR("%s: device id is invalid!", __func__);
		return ret;
	}

	if (extd_driver[device_id] && extd_driver[device_id]->get_capability)
		ret = extd_driver[device_id]->get_capability(info);

	return ret;
}

static long mtk_extd_mgr_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int r = 0;

#ifndef MTK_EXTENSION_MODE_SUPPORT
	EXT_MGR_LOG("[EXTD]ioctl= %s(%d), arg = %lu\n", _extd_ioctl_spy(cmd),
		    cmd & 0xff, arg);
#endif

	switch (cmd) {
	case MTK_HDMI_AUDIO_VIDEO_ENABLE:
	{
		external_display_enable(arg);
		break;
	}
	case MTK_HDMI_POWER_ENABLE:
	{
		external_display_power_enable(arg);
		break;
	}
	case MTK_HDMI_VIDEO_CONFIG:
	{
		external_display_set_resolution(arg);
		break;
	}
	case MTK_HDMI_FORCE_FULLSCREEN_ON:
	{
		arg = arg | 0x1;
		external_display_power_enable(arg);
		break;
	}
	case MTK_HDMI_FORCE_FULLSCREEN_OFF:
	{
		arg = arg & 0x0FF0000;
		external_display_power_enable(arg);
		break;
	}
	case MTK_HDMI_GET_DEV_INFO:
	{
		int displayid = 0;

		if (copy_from_user(&displayid, argp, sizeof(displayid))) {
			HDMI_ERR(": copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EAGAIN;
		}
		r = external_display_get_dev_info(displayid, argp);
		break;
	}
	case MTK_HDMI_USBOTG_STATUS:
	{
		break;
	}
	case MTK_HDMI_AUDIO_ENABLE:
	{
		EXT_MGR_LOG("[EXTD]hdmi_set_audio_enable, arg = %lu\n", arg);
		if (extd_driver[DEV_MHL] &&
		    extd_driver[DEV_MHL]->set_audio_enable)
			extd_driver[DEV_MHL]->set_audio_enable((arg & 0x0FF));

		break;
	}
	case MTK_HDMI_VIDEO_ENABLE:
	{
		break;
	}
	case MTK_HDMI_AUDIO_CONFIG:
	{
		EXT_MGR_LOG("[EXTD]hdmi_audio_format, arg = %lu\n", arg);
		if (extd_driver[DEV_MHL] &&
		    extd_driver[DEV_MHL]->set_audio_format)
			extd_driver[DEV_MHL]->set_audio_format(arg);

		break;
	}
	case MTK_HDMI_IS_FORCE_AWAKE:
	{
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
		r = hdmi_is_force_awake(argp);
#endif
		break;
	}
	case MTK_HDMI_GET_EDID:
	{
		if (extd_driver[DEV_MHL] && extd_driver[DEV_MHL]->get_edid)
			r = extd_driver[DEV_MHL]->get_edid(argp);

		break;
	}
	case MTK_HDMI_GET_CAPABILITY:
	{
		r = external_display_get_capability(*((unsigned long *)argp),
						    argp);
		break;
	}
	case MTK_HDMI_SCREEN_CAPTURE:
	{
		break;
	}
	case MTK_HDMI_FACTORY_CHIP_INIT:
	{
		if (extd_factory_driver[DEV_MHL] &&
		    extd_factory_driver[DEV_MHL]->factory_mode_test)
			r = extd_factory_driver[DEV_MHL]->factory_mode_test(
							STEP1_CHIP_INIT, NULL);

		break;
	}
	case MTK_HDMI_FACTORY_JUDGE_CALLBACK:
	{
		if (extd_factory_driver[DEV_MHL] &&
		    extd_factory_driver[DEV_MHL]->factory_mode_test)
			r = extd_factory_driver[DEV_MHL]->factory_mode_test(
						STEP2_JUDGE_CALLBACK, argp);

		break;
	}
	case MTK_HDMI_FACTORY_START_DPI_AND_CONFIG:
	{
		if (extd_factory_driver[DEV_MHL] &&
		    extd_factory_driver[DEV_MHL]->factory_mode_test)
			r = extd_factory_driver[DEV_MHL]->factory_mode_test(
				STEP3_START_DPI_AND_CONFIG, (void *)arg);

		break;
	}
	case MTK_HDMI_FACTORY_DPI_STOP_AND_POWER_OFF:
	{
		if (extd_factory_driver[DEV_MHL] &&
		    extd_factory_driver[DEV_MHL]->factory_mode_test)
			r = extd_factory_driver[DEV_MHL]->factory_mode_test(
				STEP4_DPI_STOP_AND_POWER_OFF, (void *)arg);

		break;
	}
	case MTK_HDMI_AUDIO_SETTING:
	{
		if (extd_driver[DEV_MHL] && extd_driver[DEV_MHL]->audio_setting)
			r = extd_driver[DEV_MHL]->audio_setting(argp);
		break;
	}
	case MTK_HDMI_HDCP_KEY:
	{
		if (extd_driver[DEV_MHL] &&
		    extd_driver[DEV_MHL]->install_hdcpkey)
			r = extd_driver[DEV_MHL]->install_hdcpkey(argp);

		break;
	}
	default:
	{
		EXT_MGR_ERR("[EXTD]ioctl(%d) arguments is not support\n",
			    cmd & 0x0ff);
		r = -EFAULT;
		break;
	}
	}

#ifndef MTK_EXTENSION_MODE_SUPPORT
	EXT_MGR_LOG("[EXTD]ioctl = %s(%d) done\n", _extd_ioctl_spy(cmd),
		    cmd & 0x0ff);
#endif
	return r;
}

static int mtk_extd_mgr_open(struct inode *inode, struct file *file)
{
	/* EXT_MGR_FUNC(); */
	return 0;
}

static int mtk_extd_mgr_release(struct inode *inode, struct file *file)
{
	/* EXT_MGR_FUNC(); */
	return 0;
}

#ifdef CONFIG_COMPAT
static long mtk_extd_mgr_compat_ioctl(struct file *file, unsigned int cmd,
				      unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	switch (cmd) {
		/* add cases here for 32bit/64bit conversion */
		/* ... */

	default:
		ret = mtk_extd_mgr_ioctl(file, cmd, arg);
		break;
	}

	return ret;
}
#endif

const struct file_operations external_display_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mtk_extd_mgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mtk_extd_mgr_compat_ioctl,
#endif
	.open = mtk_extd_mgr_open,
	.release = mtk_extd_mgr_release,
};

static const struct of_device_id extd_of_ids[] = {
	{.compatible = "mediatek,extd_dev",},
	{}
};

struct device *ext_dev_context;

static int mtk_extd_mgr_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct class_device *class_dev = NULL;

	EXT_MGR_LOG("%s+\n", __func__);

	/* Allocate device number for hdmi driver */
	ret = alloc_chrdev_region(&extd_devno, 0, 1, EXTD_DEVNAME);
	if (ret) {
		EXT_MGR_LOG("alloc_chrdev_region fail\n");
		return -1;
	}

	extd_cdev = cdev_alloc();
	extd_cdev->owner = THIS_MODULE;
	extd_cdev->ops = &external_display_fops;
	ret = cdev_add(extd_cdev, extd_devno, 1);

	extd_class = class_create(THIS_MODULE, EXTD_DEVNAME);
	/* mknod /dev/hdmitx */
	class_dev = (struct class_device *)
		device_create(extd_class, NULL, extd_devno, NULL, EXTD_DEVNAME);
	ext_dev_context = (struct device *)&(pdev->dev);

	for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
		if (extd_driver[i] && extd_driver[i]->post_init)
			extd_driver[i]->post_init();
	}

	EXT_MGR_LOG("%s-\n", __func__);
	return 0;
}

static int mtk_extd_mgr_remove(struct platform_device *pdev)
{
	EXT_MGR_FUNC();
	return 0;
}

#ifdef CONFIG_PM
int extd_pm_suspend(struct device *device)
{
	EXT_MGR_FUNC();
	return 0;
}

int extd_pm_resume(struct device *device)
{
	EXT_MGR_FUNC();
	return 0;
}

const struct dev_pm_ops extd_pm_ops = {
	.suspend = extd_pm_suspend,
	.resume = extd_pm_resume,
};
#endif

static struct platform_driver external_display_driver = {
	.probe = mtk_extd_mgr_probe,
	.remove = mtk_extd_mgr_remove,
	.driver = {
		.name = EXTD_DEVNAME,
#ifdef CONFIG_PM
		.pm = &extd_pm_ops,
#endif
		.of_match_table = extd_of_ids,
	}
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void extd_early_suspend(struct early_suspend *h)
{
	EXT_MGR_FUNC();
	int i = 0;

	for (i = DEV_MHL; i < DEV_MAX_NUM - 1; i++) {
		if (i != DEV_EINK && extd_driver[i] &&
		    extd_driver[i]->power_enable)
			extd_driver[i]->power_enable(0);
	}
}

static void extd_late_resume(struct early_suspend *h)
{
	EXT_MGR_FUNC();
	int i = 0;

	for (i = DEV_MHL; i < DEV_MAX_NUM - 1; i++) {
		if (i != DEV_EINK && extd_driver[i] &&
		    extd_driver[i]->power_enable)
			extd_driver[i]->power_enable(1);
	}
}

static struct early_suspend extd_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = extd_early_suspend,
	.resume = extd_late_resume,
};
#endif

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT != 2)
static int fb_notifier_callback(struct notifier_block *p,
				unsigned long event, void *data)
{
	int i = 0;
	int blank_mode = 0;
	struct fb_event *evdata = data;

	if (event != FB_EARLY_EVENT_BLANK)
		return 0;

	blank_mode = *(int *)evdata->data;
	EXT_MGR_LOG("[%s] - blank_mode:%d\n", __func__, blank_mode);

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
			if (i != DEV_EINK && extd_driver[i] &&
			    extd_driver[i]->power_enable)
				extd_driver[i]->power_enable(1);
		}
		break;
	case FB_BLANK_POWERDOWN:
		for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
			if (i != DEV_EINK && extd_driver[i] &&
			    extd_driver[i]->power_enable)
				extd_driver[i]->power_enable(0);
		}
		break;
	default:
		EXT_MGR_ERR("[%s] - unknown blank mode!\n", __func__);
		break;
	}

	return 0;
}
#endif

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT != 2)
static struct notifier_block notifier;
#endif

static int __init mtk_extd_mgr_init(void)
{
	int i = 0;
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT != 2)
	int ret = 0;
#endif
	/* struct notifier_block notifier; */

	EXT_MGR_LOG("%s+\n", __func__);

	extd_driver[DEV_MHL] = EXTD_HDMI_Driver();
	extd_driver[DEV_EINK] = EXTD_EPD_Driver();
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	extd_driver[DEV_LCM] = EXTD_LCM_Driver();
#endif
	extd_factory_driver[DEV_MHL] = EXTD_Factory_HDMI_Driver();

	for (i = DEV_MHL; i < DEV_MAX_NUM; i++) {
		DISPMSG("%s: extd driver init(%d)\n", __func__, i);
		if (extd_driver[i] && extd_driver[i]->init)
			extd_driver[i]->init();
		else
			DISPMSG("%s:%d:NO init func\n", __func__, i);
	}

	if (platform_driver_register(&external_display_driver)) {
		EXT_MGR_ERR("failed to register mtkfb driver\n");
		return -1;
	}

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT != 2)
	notifier.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&notifier);
	if (ret)
		EXT_MGR_ERR("unable to register fb callback!\n");
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&extd_early_suspend_handler);
#endif
	EXT_MGR_LOG("%s-\n", __func__);
	return 0;
}

static void __exit mtk_extd_mgr_exit(void)
{
	device_destroy(extd_class, extd_devno);
	class_destroy(extd_class);
	cdev_del(extd_cdev);
	unregister_chrdev_region(extd_devno, 1);
}

late_initcall(mtk_extd_mgr_init);
module_exit(mtk_extd_mgr_exit);
MODULE_AUTHOR("www.mediatek.com>");
MODULE_DESCRIPTION("External Display Driver");
MODULE_LICENSE("GPL");
