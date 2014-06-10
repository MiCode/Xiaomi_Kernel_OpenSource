/*
 * Core MDSS framebuffer driver.
 *
 * Copyright (C) 2007 Google Incorporated
 * Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/bootmem.h>
#include <linux/console.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/msm_mdp.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include <linux/file.h>
#include <linux/kthread.h>

#include <linux/qcom_iommu.h>
#include <linux/msm_iommu_domains.h>

#include "mdss_fb.h"
#include "mdss_mdp_splash_logo.h"

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MDSS_FB_NUM 3
#else
#define MDSS_FB_NUM 2
#endif

#ifndef EXPORT_COMPAT
#define EXPORT_COMPAT(x)
#endif

#define MAX_FBI_LIST 32
static struct fb_info *fbi_list[MAX_FBI_LIST];
static int fbi_list_index;

static u32 mdss_fb_pseudo_palette[16] = {
	0x00000000, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

static struct msm_mdp_interface *mdp_instance;

static int mdss_fb_register(struct msm_fb_data_type *mfd);
static int mdss_fb_open(struct fb_info *info, int user);
static int mdss_fb_release(struct fb_info *info, int user);
static int mdss_fb_release_all(struct fb_info *info, bool release_all);
static int mdss_fb_pan_display(struct fb_var_screeninfo *var,
			       struct fb_info *info);
static int mdss_fb_check_var(struct fb_var_screeninfo *var,
			     struct fb_info *info);
static int mdss_fb_set_par(struct fb_info *info);
static int mdss_fb_blank_sub(int blank_mode, struct fb_info *info,
			     int op_enable);
static int mdss_fb_suspend_sub(struct msm_fb_data_type *mfd);
static int mdss_fb_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg);
static int mdss_fb_fbmem_ion_mmap(struct fb_info *info,
		struct vm_area_struct *vma);
static int mdss_fb_alloc_fb_ion_memory(struct msm_fb_data_type *mfd,
		size_t size);
static void mdss_fb_release_fences(struct msm_fb_data_type *mfd);
static int __mdss_fb_sync_buf_done_callback(struct notifier_block *p,
		unsigned long val, void *data);

static int __mdss_fb_display_thread(void *data);
static int mdss_fb_pan_idle(struct msm_fb_data_type *mfd);
static int mdss_fb_send_panel_event(struct msm_fb_data_type *mfd,
					int event, void *arg);
static void mdss_fb_set_mdp_sync_pt_threshold(struct msm_fb_data_type *mfd);
void mdss_fb_no_update_notify_timer_cb(unsigned long data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)data;
	if (!mfd) {
		pr_err("%s mfd NULL\n", __func__);
		return;
	}
	mfd->no_update.value = NOTIFY_TYPE_NO_UPDATE;
	complete(&mfd->no_update.comp);
}

void mdss_fb_bl_update_notify(struct msm_fb_data_type *mfd)
{
	if (!mfd) {
		pr_err("%s mfd NULL\n", __func__);
		return;
	}
	mutex_lock(&mfd->update.lock);
	if (mfd->update.ref_count > 0) {
		mutex_unlock(&mfd->update.lock);
		mfd->update.value = NOTIFY_TYPE_BL_UPDATE;
		complete(&mfd->update.comp);
		mutex_lock(&mfd->update.lock);
	}
	mutex_unlock(&mfd->update.lock);

	mutex_lock(&mfd->no_update.lock);
	if (mfd->no_update.ref_count > 0) {
		mutex_unlock(&mfd->no_update.lock);
		mfd->no_update.value = NOTIFY_TYPE_BL_UPDATE;
		complete(&mfd->no_update.comp);
		mutex_lock(&mfd->no_update.lock);
	}
	mutex_unlock(&mfd->no_update.lock);
}

static int mdss_fb_notify_update(struct msm_fb_data_type *mfd,
							unsigned long *argp)
{
	int ret;
	unsigned long notify = 0x0, to_user = 0x0;

	ret = copy_from_user(&notify, argp, sizeof(unsigned long));
	if (ret) {
		pr_err("%s:ioctl failed\n", __func__);
		return ret;
	}

	if (notify > NOTIFY_UPDATE_POWER_OFF)
		return -EINVAL;

	if (mfd->update.is_suspend) {
		to_user = NOTIFY_TYPE_SUSPEND;
		mfd->update.is_suspend = 0;
		ret = 1;
	} else if (notify == NOTIFY_UPDATE_START) {
		INIT_COMPLETION(mfd->update.comp);
		mutex_lock(&mfd->update.lock);
		mfd->update.ref_count++;
		mutex_unlock(&mfd->update.lock);
		ret = wait_for_completion_interruptible_timeout(
						&mfd->update.comp, 4 * HZ);
		mutex_lock(&mfd->update.lock);
		mfd->update.ref_count--;
		mutex_unlock(&mfd->update.lock);
		to_user = (unsigned int)mfd->update.value;
		if (mfd->update.type == NOTIFY_TYPE_SUSPEND) {
			to_user = (unsigned int)mfd->update.type;
			ret = 1;
		}
	} else if (notify == NOTIFY_UPDATE_STOP) {
		INIT_COMPLETION(mfd->no_update.comp);
		mutex_lock(&mfd->no_update.lock);
		mfd->no_update.ref_count++;
		mutex_unlock(&mfd->no_update.lock);
		ret = wait_for_completion_interruptible_timeout(
						&mfd->no_update.comp, 4 * HZ);
		mutex_lock(&mfd->no_update.lock);
		mfd->no_update.ref_count--;
		mutex_unlock(&mfd->no_update.lock);
		to_user = (unsigned int)mfd->no_update.value;
	} else {
		if (mfd->panel_power_on) {
			INIT_COMPLETION(mfd->power_off_comp);
			ret = wait_for_completion_interruptible_timeout(
						&mfd->power_off_comp, 1 * HZ);
		}
	}

	if (ret == 0)
		ret = -ETIMEDOUT;
	else if (ret > 0)
		ret = copy_to_user(argp, &to_user, sizeof(unsigned long));
	return ret;
}

static int lcd_backlight_registered;

static void mdss_fb_set_bl_brightness(struct led_classdev *led_cdev,
				      enum led_brightness value)
{
	struct msm_fb_data_type *mfd = dev_get_drvdata(led_cdev->dev->parent);
	int bl_lvl;

	if (value > mfd->panel_info->brightness_max)
		value = mfd->panel_info->brightness_max;

	/* This maps android backlight level 0 to 255 into
	   driver backlight level 0 to bl_max with rounding */
	MDSS_BRIGHT_TO_BL(bl_lvl, value, mfd->panel_info->bl_max,
				mfd->panel_info->brightness_max);

	if (!bl_lvl && value)
		bl_lvl = 1;

	if (!IS_CALIB_MODE_BL(mfd) && (!mfd->ext_bl_ctrl || !value ||
							!mfd->bl_level)) {
		mutex_lock(&mfd->bl_lock);
		mdss_fb_set_backlight(mfd, bl_lvl);
		mutex_unlock(&mfd->bl_lock);
	}
}

static struct led_classdev backlight_led = {
	.name           = "lcd-backlight",
	.brightness     = MDSS_MAX_BL_BRIGHTNESS,
	.brightness_set = mdss_fb_set_bl_brightness,
	.max_brightness = MDSS_MAX_BL_BRIGHTNESS,
};

static ssize_t mdss_fb_get_type(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	switch (mfd->panel.type) {
	case NO_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "no panel\n");
		break;
	case HDMI_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "hdmi panel\n");
		break;
	case LVDS_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "lvds panel\n");
		break;
	case DTV_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "dtv panel\n");
		break;
	case MIPI_VIDEO_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "mipi dsi video panel\n");
		break;
	case MIPI_CMD_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "mipi dsi cmd panel\n");
		break;
	case WRITEBACK_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "writeback panel\n");
		break;
	case EDP_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "edp panel\n");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "unknown panel\n");
		break;
	}

	return ret;
}

static inline int mdss_fb_validate_split(int left, int right,
			struct msm_fb_data_type *mfd)
{
	int rc = -EINVAL;
	u32 panel_xres = mfd->panel_info->xres;
	/* more validate condition could be added if needed */
	if (left && right) {
		if (is_panel_split(mfd))
			panel_xres *= 2;

		if (panel_xres == left + right) {
			mfd->split_fb_left = left;
			mfd->split_fb_right = right;
			rc = 0;
		}
	} else {
		if (is_panel_split(mfd)) {
			mfd->split_fb_left = mfd->split_fb_right = panel_xres;
			rc = 0;
		} else {
			mfd->split_fb_left = mfd->split_fb_right = 0;
		}
	}

	return rc;
}

static void mdss_fb_parse_dt_split(struct msm_fb_data_type *mfd)
{
	u32 data[2] = {0};
	struct platform_device *pdev = mfd->pdev;

	of_property_read_u32_array(pdev->dev.of_node,
		"qcom,mdss-fb-split", data, 2);

	if (!mdss_fb_validate_split(data[0], data[1], mfd))
		pr_debug("dt split_left=%d split_right=%d\n", data[0], data[1]);
}

static ssize_t mdss_fb_store_split(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u32 data[2] = {0};
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;

	if (2 != sscanf(buf, "%d %d", &data[0], &data[1])) {
		pr_debug("Not able to read split values\n");
	} else if (!mdss_fb_validate_split(data[0], data[1], mfd)) {
		mfd->mdss_fb_split_stored = 1;
		pr_debug("sys split_left=%d split_right=%d\n",
					data[0], data[1]);
	}

	return len;
}

static ssize_t mdss_fb_show_split(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	ret = snprintf(buf, PAGE_SIZE, "%d %d\n",
		       mfd->split_fb_left, mfd->split_fb_right);
	return ret;
}

static void mdss_fb_get_split(struct msm_fb_data_type *mfd)
{
	if (mfd->index != 0)
		return;

	if (!mfd->mdss_fb_split_stored)
		mdss_fb_parse_dt_split(mfd);

	if (mfd->split_fb_left || mfd->split_fb_right)
		pr_debug("split framebuffer left=%d right=%d\n",
			mfd->split_fb_left, mfd->split_fb_right);
}

static ssize_t mdss_mdp_show_blank_event(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	int ret;

	pr_debug("fb%d panel_power_on = %d\n", mfd->index, mfd->panel_power_on);
	ret = scnprintf(buf, PAGE_SIZE, "panel_power_on = %d\n",
						mfd->panel_power_on);

	return ret;
}

static void __mdss_fb_idle_notify_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct msm_fb_data_type *mfd = container_of(dw, struct msm_fb_data_type,
		idle_notify_work);

	/* Notify idle-ness here */
	pr_debug("Idle timeout %dms expired!\n", mfd->idle_time);
	sysfs_notify(&mfd->fbi->dev->kobj, NULL, "idle_notify");
}

static ssize_t mdss_fb_get_idle_time(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d", mfd->idle_time);

	return ret;
}

static ssize_t mdss_fb_set_idle_time(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	int rc = 0;
	int idle_time = 0;

	rc = kstrtoint(buf, 10, &idle_time);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_debug("Idle time = %d\n", idle_time);
	mfd->idle_time = idle_time;

	return count;
}

static ssize_t mdss_fb_get_idle_notify(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%s",
		work_busy(&mfd->idle_notify_work.work) ? "no" : "yes");

	return ret;
}

static ssize_t mdss_fb_get_panel_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	struct mdss_panel_info *pinfo = mfd->panel_info;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE,
			"pu_en=%d\nxstart=%d\nwalign=%d\nystart=%d\nhalign=%d\n"
			"min_w=%d\nmin_h=%d\nroi_merge=%d",
			pinfo->partial_update_enabled, pinfo->xstart_pix_align,
			pinfo->width_pix_align, pinfo->ystart_pix_align,
			pinfo->height_pix_align, pinfo->min_width,
			pinfo->min_height, pinfo->partial_update_roi_merge);

	return ret;
}
/*
 * mdss_fb_lpm_enable() - Function to Control LowPowerMode
 * @mfd:	Framebuffer data structure for display
 * @mode:	Enabled/Disable LowPowerMode
 *		1: Enter into LowPowerMode
 *		0: Exit from LowPowerMode
 *
 * This Function dynamically switches to and from LowPowerMode
 * based on the argument @mode.
 */
static int mdss_fb_lpm_enable(struct msm_fb_data_type *mfd, int mode)
{
	int ret = 0;
	u32 bl_lvl = 0;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_panel_data *pdata;

	if (!mfd || !mfd->panel_info)
		return -EINVAL;

	pinfo = mfd->panel_info;

	if (!pinfo->mipi.dynamic_switch_enabled) {
		pr_warn("Panel does not support dynamic switch!\n");
		return 0;
	}

	if (mode == pinfo->mipi.mode) {
		pr_debug("Already in requested mode!\n");
		return 0;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);

	pr_debug("Enter mode: %d\n", mode);
	pdata->panel_info.dynamic_switch_pending = true;

	mutex_lock(&mfd->bl_lock);
	bl_lvl = mfd->bl_level;
	mdss_fb_set_backlight(mfd, 0);
	mutex_unlock(&mfd->bl_lock);

	lock_fb_info(mfd->fbi);
	ret = mdss_fb_blank_sub(FB_BLANK_POWERDOWN, mfd->fbi,
						mfd->op_enable);
	if (ret) {
		pr_err("can't turn off display!\n");
		unlock_fb_info(mfd->fbi);
		return ret;
	}

	mfd->op_enable = false;

	ret = mfd->mdp.configure_panel(mfd, mode);
	mdss_fb_set_mdp_sync_pt_threshold(mfd);

	mfd->op_enable = true;

	ret = mdss_fb_blank_sub(FB_BLANK_UNBLANK, mfd->fbi,
					mfd->op_enable);
	if (ret) {
		pr_err("can't turn on display!\n");
		unlock_fb_info(mfd->fbi);
		return ret;
	}
	unlock_fb_info(mfd->fbi);

	mutex_lock(&mfd->bl_lock);
	mfd->bl_updated = true;
	mdss_fb_set_backlight(mfd, bl_lvl);
	mutex_unlock(&mfd->bl_lock);

	pdata->panel_info.dynamic_switch_pending = false;
	pdata->panel_info.is_lpm_mode = mode ? 1 : 0;

	if (ret) {
		pr_err("can't turn on display!\n");
		return ret;
	}

	pr_debug("Exit mode: %d\n", mode);

	return 0;
}

static ssize_t mdss_fb_get_src_split_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	int ret = 0;

	if ((mfd->split_mode == MDP_SPLIT_MODE_LM) &&
	    (fbi->var.yres > 2048) && (fbi->var.yres > fbi->var.xres))
		ret = scnprintf(buf, PAGE_SIZE,
			"src_split_always\n");

	return ret;
}

static DEVICE_ATTR(msm_fb_type, S_IRUGO, mdss_fb_get_type, NULL);
static DEVICE_ATTR(msm_fb_split, S_IRUGO | S_IWUSR, mdss_fb_show_split,
					mdss_fb_store_split);
static DEVICE_ATTR(show_blank_event, S_IRUGO, mdss_mdp_show_blank_event, NULL);
static DEVICE_ATTR(idle_time, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_fb_get_idle_time, mdss_fb_set_idle_time);
static DEVICE_ATTR(idle_notify, S_IRUGO, mdss_fb_get_idle_notify, NULL);
static DEVICE_ATTR(msm_fb_panel_info, S_IRUGO, mdss_fb_get_panel_info, NULL);
static DEVICE_ATTR(msm_fb_src_split_info, S_IRUGO, mdss_fb_get_src_split_info,
	NULL);

static struct attribute *mdss_fb_attrs[] = {
	&dev_attr_msm_fb_type.attr,
	&dev_attr_msm_fb_split.attr,
	&dev_attr_show_blank_event.attr,
	&dev_attr_idle_time.attr,
	&dev_attr_idle_notify.attr,
	&dev_attr_msm_fb_panel_info.attr,
	&dev_attr_msm_fb_src_split_info.attr,
	NULL,
};

static struct attribute_group mdss_fb_attr_group = {
	.attrs = mdss_fb_attrs,
};

static int mdss_fb_create_sysfs(struct msm_fb_data_type *mfd)
{
	int rc;

	rc = sysfs_create_group(&mfd->fbi->dev->kobj, &mdss_fb_attr_group);
	if (rc)
		pr_err("sysfs group creation failed, rc=%d\n", rc);
	return rc;
}

static void mdss_fb_remove_sysfs(struct msm_fb_data_type *mfd)
{
	sysfs_remove_group(&mfd->fbi->dev->kobj, &mdss_fb_attr_group);
}

static void mdss_fb_shutdown(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	mfd->shutdown_pending = true;
	lock_fb_info(mfd->fbi);
	mdss_fb_release_all(mfd->fbi, true);
	unlock_fb_info(mfd->fbi);
}

static int mdss_fb_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = NULL;
	struct mdss_panel_data *pdata;
	struct fb_info *fbi;
	int rc;

	if (fbi_list_index >= MAX_FBI_LIST)
		return -ENOMEM;

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata)
		return -EPROBE_DEFER;

	/*
	 * alloc framebuffer info + par data
	 */
	fbi = framebuffer_alloc(sizeof(struct msm_fb_data_type), NULL);
	if (fbi == NULL) {
		pr_err("can't allocate framebuffer info data!\n");
		return -ENOMEM;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	mfd->key = MFD_KEY;
	mfd->fbi = fbi;
	mfd->panel_info = &pdata->panel_info;
	mfd->panel.type = pdata->panel_info.type;
	mfd->panel.id = mfd->index;
	mfd->fb_page = MDSS_FB_NUM;
	mfd->index = fbi_list_index;
	mfd->mdp_fb_page_protection = MDP_FB_PAGE_PROTECTION_WRITECOMBINE;

	mfd->ext_ad_ctrl = -1;
	mfd->bl_level = 0;
	mfd->bl_level_prev_scaled = 0;
	mfd->bl_scale = 1024;
	mfd->bl_min_lvl = 30;
	mfd->fb_imgType = MDP_RGBA_8888;

	mfd->pdev = pdev;
	mfd->split_mode = MDP_SPLIT_MODE_NONE;
	if (pdata->next)
		mfd->split_mode = MDP_SPLIT_MODE_LM;
	mfd->mdp = *mdp_instance;
	INIT_LIST_HEAD(&mfd->proc_list);

	mutex_init(&mfd->bl_lock);

	fbi_list[fbi_list_index++] = fbi;

	platform_set_drvdata(pdev, mfd);

	rc = mdss_fb_register(mfd);
	if (rc)
		return rc;

	if (mfd->mdp.init_fnc) {
		rc = mfd->mdp.init_fnc(mfd);
		if (rc) {
			pr_err("init_fnc failed\n");
			return rc;
		}
	}

	rc = pm_runtime_set_active(mfd->fbi->dev);
	if (rc < 0)
		pr_err("pm_runtime: fail to set active.\n");
	pm_runtime_enable(mfd->fbi->dev);

	/* android supports only one lcd-backlight/lcd for now */
	if (!lcd_backlight_registered) {
		backlight_led.brightness = mfd->panel_info->brightness_max;
		backlight_led.max_brightness = mfd->panel_info->brightness_max;
		if (led_classdev_register(&pdev->dev, &backlight_led))
			pr_err("led_classdev_register failed\n");
		else
			lcd_backlight_registered = 1;
	}

	mdss_fb_create_sysfs(mfd);
	mdss_fb_send_panel_event(mfd, MDSS_EVENT_FB_REGISTERED, fbi);

	mfd->mdp_sync_pt_data.fence_name = "mdp-fence";
	if (mfd->mdp_sync_pt_data.timeline == NULL) {
		char timeline_name[16];
		snprintf(timeline_name, sizeof(timeline_name),
			"mdss_fb_%d", mfd->index);
		 mfd->mdp_sync_pt_data.timeline =
				sw_sync_timeline_create(timeline_name);
		if (mfd->mdp_sync_pt_data.timeline == NULL) {
			pr_err("cannot create release fence time line\n");
			return -ENOMEM;
		}
		mfd->mdp_sync_pt_data.notifier.notifier_call =
			__mdss_fb_sync_buf_done_callback;
	}

	mdss_fb_set_mdp_sync_pt_threshold(mfd);

	if (mfd->mdp.splash_init_fnc)
		mfd->mdp.splash_init_fnc(mfd);

	INIT_DELAYED_WORK(&mfd->idle_notify_work, __mdss_fb_idle_notify_work);

	return rc;
}

static void mdss_fb_set_mdp_sync_pt_threshold(struct msm_fb_data_type *mfd)
{
	if (!mfd)
		return;

	switch (mfd->panel.type) {
	case WRITEBACK_PANEL:
		mfd->mdp_sync_pt_data.threshold = 1;
		mfd->mdp_sync_pt_data.retire_threshold = 0;
		break;
	case MIPI_CMD_PANEL:
		mfd->mdp_sync_pt_data.threshold = 1;
		mfd->mdp_sync_pt_data.retire_threshold = 1;
		break;
	default:
		mfd->mdp_sync_pt_data.threshold = 2;
		mfd->mdp_sync_pt_data.retire_threshold = 0;
		break;
	}
}
static int mdss_fb_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	mdss_fb_remove_sysfs(mfd);

	pm_runtime_disable(mfd->fbi->dev);

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (mdss_fb_suspend_sub(mfd))
		pr_err("msm_fb_remove: can't stop the device %d\n",
			    mfd->index);

	/* remove /dev/fb* */
	unregister_framebuffer(mfd->fbi);

	if (lcd_backlight_registered) {
		lcd_backlight_registered = 0;
		led_classdev_unregister(&backlight_led);
	}

	return 0;
}

static int mdss_fb_send_panel_event(struct msm_fb_data_type *mfd,
					int event, void *arg)
{
	struct mdss_panel_data *pdata;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected\n");
		return -ENODEV;
	}

	pr_debug("sending event=%d for fb%d\n", event, mfd->index);

	if (pdata->event_handler)
		return pdata->event_handler(pdata, event, arg);

	return 0;
}

static int mdss_fb_suspend_sub(struct msm_fb_data_type *mfd)
{
	int ret = 0;

	if ((!mfd) || (mfd->key != MFD_KEY))
		return 0;

	pr_debug("mdss_fb suspend index=%d\n", mfd->index);

	mdss_fb_pan_idle(mfd);
	ret = mdss_fb_send_panel_event(mfd, MDSS_EVENT_SUSPEND, NULL);
	if (ret) {
		pr_warn("unable to suspend fb%d (%d)\n", mfd->index, ret);
		return ret;
	}

	mfd->suspend.op_enable = mfd->op_enable;
	mfd->suspend.panel_power_on = mfd->panel_power_on;

	if (mfd->op_enable) {
		ret = mdss_fb_blank_sub(FB_BLANK_POWERDOWN, mfd->fbi,
				mfd->suspend.op_enable);
		if (ret) {
			pr_warn("can't turn off display!\n");
			return ret;
		}
		mfd->op_enable = false;
		fb_set_suspend(mfd->fbi, FBINFO_STATE_SUSPENDED);
	}

	return 0;
}

static int mdss_fb_resume_sub(struct msm_fb_data_type *mfd)
{
	int ret = 0;

	if ((!mfd) || (mfd->key != MFD_KEY))
		return 0;

	INIT_COMPLETION(mfd->power_set_comp);
	mfd->is_power_setting = true;
	pr_debug("mdss_fb resume index=%d\n", mfd->index);

	mdss_fb_pan_idle(mfd);
	ret = mdss_fb_send_panel_event(mfd, MDSS_EVENT_RESUME, NULL);
	if (ret) {
		pr_warn("unable to resume fb%d (%d)\n", mfd->index, ret);
		return ret;
	}

	/* resume state var recover */
	mfd->op_enable = mfd->suspend.op_enable;

	if (mfd->suspend.panel_power_on) {
		ret = mdss_fb_blank_sub(FB_BLANK_UNBLANK, mfd->fbi,
					mfd->op_enable);
		if (ret)
			pr_warn("can't turn on display!\n");
		else
			fb_set_suspend(mfd->fbi, FBINFO_STATE_RUNNING);
	}
	mfd->is_power_setting = false;
	complete_all(&mfd->power_set_comp);

	return ret;
}

#if defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP)
static int mdss_fb_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;

	dev_dbg(&pdev->dev, "display suspend\n");

	return mdss_fb_suspend_sub(mfd);
}

static int mdss_fb_resume(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;

	dev_dbg(&pdev->dev, "display resume\n");

	return mdss_fb_resume_sub(mfd);
}
#else
#define mdss_fb_suspend NULL
#define mdss_fb_resume NULL
#endif

#ifdef CONFIG_PM_SLEEP
static int mdss_fb_pm_suspend(struct device *dev)
{
	struct msm_fb_data_type *mfd = dev_get_drvdata(dev);

	if (!mfd)
		return -ENODEV;

	dev_dbg(dev, "display pm suspend\n");

	return mdss_fb_suspend_sub(mfd);
}

static int mdss_fb_pm_resume(struct device *dev)
{
	struct msm_fb_data_type *mfd = dev_get_drvdata(dev);
	if (!mfd)
		return -ENODEV;

	dev_dbg(dev, "display pm resume\n");

	return mdss_fb_resume_sub(mfd);
}
#endif

static const struct dev_pm_ops mdss_fb_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mdss_fb_pm_suspend, mdss_fb_pm_resume)
};

static const struct of_device_id mdss_fb_dt_match[] = {
	{ .compatible = "qcom,mdss-fb",},
	{}
};
EXPORT_COMPAT("qcom,mdss-fb");

static struct platform_driver mdss_fb_driver = {
	.probe = mdss_fb_probe,
	.remove = mdss_fb_remove,
	.suspend = mdss_fb_suspend,
	.resume = mdss_fb_resume,
	.shutdown = mdss_fb_shutdown,
	.driver = {
		.name = "mdss_fb",
		.of_match_table = mdss_fb_dt_match,
		.pm = &mdss_fb_pm_ops,
	},
};

static void mdss_fb_scale_bl(struct msm_fb_data_type *mfd, u32 *bl_lvl)
{
	u32 temp = *bl_lvl;

	pr_debug("input = %d, scale = %d", temp, mfd->bl_scale);
	if (temp >= mfd->bl_min_lvl) {
		if (temp > mfd->panel_info->bl_max) {
			pr_warn("%s: invalid bl level\n",
				__func__);
			temp = mfd->panel_info->bl_max;
		}
		if (mfd->bl_scale > 1024) {
			pr_warn("%s: invalid bl scale\n",
				__func__);
			mfd->bl_scale = 1024;
		}
		/*
		 * bl_scale is the numerator of
		 * scaling fraction (x/1024)
		 */
		temp = (temp * mfd->bl_scale) / 1024;

		/*if less than minimum level, use min level*/
		if (temp < mfd->bl_min_lvl)
			temp = mfd->bl_min_lvl;
	}
	pr_debug("output = %d", temp);

	(*bl_lvl) = temp;
}

/* must call this function from within mfd->bl_lock */
void mdss_fb_set_backlight(struct msm_fb_data_type *mfd, u32 bkl_lvl)
{
	struct mdss_panel_data *pdata;
	int (*update_ad_input)(struct msm_fb_data_type *mfd);
	u32 temp = bkl_lvl;
	int ret = -EINVAL;
	bool is_bl_changed = (bkl_lvl != mfd->bl_level);

	if (((!mfd->panel_power_on && mfd->dcm_state != DCM_ENTER)
		|| !mfd->bl_updated) && !IS_CALIB_MODE_BL(mfd)) {
		mfd->unset_bl_level = bkl_lvl;
		return;
	} else {
		mfd->unset_bl_level = 0;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if ((pdata) && (pdata->set_backlight)) {
		if (mfd->mdp.ad_attenuate_bl && is_bl_changed) {
			ret = (*mfd->mdp.ad_attenuate_bl)(bkl_lvl, &temp, mfd);
			if (ret)
				pr_err("Failed to attenuate BL\n");
		}

		mfd->bl_level_prev_scaled = mfd->bl_level_scaled;
		if (!IS_CALIB_MODE_BL(mfd))
			mdss_fb_scale_bl(mfd, &temp);
		/*
		 * Even though backlight has been scaled, want to show that
		 * backlight has been set to bkl_lvl to those that read from
		 * sysfs node. Thus, need to set bl_level even if it appears
		 * the backlight has already been set to the level it is at,
		 * as well as setting bl_level to bkl_lvl even though the
		 * backlight has been set to the scaled value.
		 */
		if (mfd->bl_level_scaled == temp) {
			mfd->bl_level = bkl_lvl;
			return;
		}
		pdata->set_backlight(pdata, temp);
		mfd->bl_level = bkl_lvl;
		mfd->bl_level_scaled = temp;

		if (mfd->mdp.update_ad_input && is_bl_changed) {
			update_ad_input = mfd->mdp.update_ad_input;
			mutex_unlock(&mfd->bl_lock);
			/* Will trigger ad_setup which will grab bl_lock */
			update_ad_input(mfd);
			mutex_lock(&mfd->bl_lock);
		}
		mdss_fb_bl_update_notify(mfd);
	}
}

void mdss_fb_update_backlight(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	int ret = 0;
	u32 temp;

	if (!mfd->unset_bl_level)
		return;
	mutex_lock(&mfd->bl_lock);
	if (!mfd->bl_updated) {
		pdata = dev_get_platdata(&mfd->pdev->dev);
		if ((pdata) && (pdata->set_backlight)) {
			mfd->bl_level = mfd->unset_bl_level;
			temp = mfd->bl_level;
			if (mfd->mdp.ad_attenuate_bl) {
				ret = (*mfd->mdp.ad_attenuate_bl)(temp,
						&temp, mfd);
				if (ret)
					pr_err("Failed to attenuate BL\n");
			}

			pdata->set_backlight(pdata, temp);
			mfd->bl_level_scaled = mfd->unset_bl_level;
			mfd->bl_updated = 1;
		}
	}
	mutex_unlock(&mfd->bl_lock);
}

static int mdss_fb_blank_sub(int blank_mode, struct fb_info *info,
			     int op_enable)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int ret = 0;

	if (!op_enable)
		return -EPERM;

	if (mfd->dcm_state == DCM_ENTER)
		return -EPERM;

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		if (!mfd->panel_power_on && mfd->mdp.on_fnc) {
			ret = mfd->mdp.on_fnc(mfd);
			if (ret == 0) {
				mfd->panel_power_on = true;
				mfd->panel_info->panel_dead = false;
			}
			mutex_lock(&mfd->update.lock);
			mfd->update.type = NOTIFY_TYPE_UPDATE;
			mfd->update.is_suspend = 0;
			mutex_unlock(&mfd->update.lock);

			/* Start the work thread to signal idle time */
			if (mfd->idle_time)
				schedule_delayed_work(&mfd->idle_notify_work,
					msecs_to_jiffies(mfd->idle_time));
		}

		mutex_lock(&mfd->bl_lock);
		if (!mfd->bl_updated) {
			mfd->bl_updated = 1;
			mdss_fb_set_backlight(mfd, mfd->bl_level_prev_scaled);
		}
		mutex_unlock(&mfd->bl_lock);
		break;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
	case FB_BLANK_POWERDOWN:
	default:
		if (mfd->panel_power_on && mfd->mdp.off_fnc) {
			int curr_pwr_state;

			mutex_lock(&mfd->update.lock);
			mfd->update.type = NOTIFY_TYPE_SUSPEND;
			mfd->update.is_suspend = 1;
			mutex_unlock(&mfd->update.lock);
			complete(&mfd->update.comp);
			del_timer(&mfd->no_update.timer);
			mfd->no_update.value = NOTIFY_TYPE_SUSPEND;
			complete(&mfd->no_update.comp);

			mfd->op_enable = false;
			curr_pwr_state = mfd->panel_power_on;
			mutex_lock(&mfd->bl_lock);
			mdss_fb_set_backlight(mfd, 0);
			mfd->panel_power_on = false;
			mfd->bl_updated = 0;
			mutex_unlock(&mfd->bl_lock);

			ret = mfd->mdp.off_fnc(mfd);
			if (ret)
				mfd->panel_power_on = curr_pwr_state;
			else
				mdss_fb_release_fences(mfd);
			mfd->op_enable = true;
			complete(&mfd->power_off_comp);
		}
		break;
	}
	/* Notify listeners */
	sysfs_notify(&mfd->fbi->dev->kobj, NULL, "show_blank_event");

	return ret;
}

static int mdss_fb_blank(int blank_mode, struct fb_info *info)
{
	struct mdss_panel_data *pdata;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	mdss_fb_pan_idle(mfd);
	if (mfd->op_enable == 0) {
		if (blank_mode == FB_BLANK_UNBLANK)
			mfd->suspend.panel_power_on = true;
		else
			mfd->suspend.panel_power_on = false;
		return 0;
	}
	pr_debug("mode: %d\n", blank_mode);

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if (pdata->panel_info.is_lpm_mode &&
			blank_mode == FB_BLANK_UNBLANK) {
		pr_debug("panel is in lpm mode\n");
		mfd->mdp.configure_panel(mfd, 0);
		mdss_fb_set_mdp_sync_pt_threshold(mfd);
		pdata->panel_info.is_lpm_mode = false;
	}

	return mdss_fb_blank_sub(blank_mode, info, mfd->op_enable);
}

static inline int mdss_fb_create_ion_client(struct msm_fb_data_type *mfd)
{
	mfd->fb_ion_client  = msm_ion_client_create("mdss_fb_iclient");
	if (IS_ERR_OR_NULL(mfd->fb_ion_client)) {
		pr_err("Err:client not created, val %d\n",
				PTR_RET(mfd->fb_ion_client));
		mfd->fb_ion_client = NULL;
		return PTR_RET(mfd->fb_ion_client);
	}
	return 0;
}

void mdss_fb_free_fb_ion_memory(struct msm_fb_data_type *mfd)
{
	if (!mfd) {
		pr_err("no mfd\n");
		return;
	}

	if (!mfd->fbi->screen_base)
		return;

	if (!mfd->fb_ion_client || !mfd->fb_ion_handle) {
		pr_err("invalid input parameters for fb%d\n", mfd->index);
		return;
	}

	mfd->fbi->screen_base = NULL;
	mfd->fbi->fix.smem_start = 0;

	ion_unmap_kernel(mfd->fb_ion_client, mfd->fb_ion_handle);

	if (mfd->mdp.fb_mem_get_iommu_domain) {
		ion_unmap_iommu(mfd->fb_ion_client, mfd->fb_ion_handle,
				mfd->mdp.fb_mem_get_iommu_domain(), 0);
	}

	ion_free(mfd->fb_ion_client, mfd->fb_ion_handle);
	mfd->fb_ion_handle = NULL;
}

int mdss_fb_alloc_fb_ion_memory(struct msm_fb_data_type *mfd, size_t fb_size)
{
	unsigned long buf_size;
	int rc;
	void *vaddr;

	if (!mfd) {
		pr_err("Invalid input param - no mfd");
		return -EINVAL;
	}

	if (!mfd->fb_ion_client) {
		rc = mdss_fb_create_ion_client(mfd);
		if (rc < 0) {
			pr_err("fb ion client couldn't be created - %d\n", rc);
			return rc;
		}
	}

	pr_debug("size for mmap = %zu", fb_size);
	mfd->fb_ion_handle = ion_alloc(mfd->fb_ion_client, fb_size, SZ_4K,
			ION_HEAP(ION_SYSTEM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(mfd->fb_ion_handle)) {
		pr_err("unable to alloc fbmem from ion - %ld\n",
				PTR_ERR(mfd->fb_ion_handle));
		return PTR_ERR(mfd->fb_ion_handle);
	}

	if (mfd->mdp.fb_mem_get_iommu_domain) {
		rc = ion_map_iommu(mfd->fb_ion_client, mfd->fb_ion_handle,
				mfd->mdp.fb_mem_get_iommu_domain(), 0, SZ_4K, 0,
				&mfd->iova, &buf_size, 0, 0);
		if (rc) {
			pr_err("Cannot map fb_mem to IOMMU. rc=%d\n", rc);
			goto fb_mmap_failed;
		}
	} else {
		pr_err("No IOMMU Domain");
		goto fb_mmap_failed;

	}

	vaddr  = ion_map_kernel(mfd->fb_ion_client, mfd->fb_ion_handle);
	if (IS_ERR_OR_NULL(vaddr)) {
		pr_err("ION memory mapping failed - %ld\n", PTR_ERR(vaddr));
		rc = PTR_ERR(vaddr);
		if (mfd->mdp.fb_mem_get_iommu_domain) {
			ion_unmap_iommu(mfd->fb_ion_client, mfd->fb_ion_handle,
					mfd->mdp.fb_mem_get_iommu_domain(), 0);
		}
		goto fb_mmap_failed;
	}

	pr_debug("alloc 0x%zuB vaddr = %p (%pa iova) for fb%d\n", fb_size,
			vaddr, &mfd->iova, mfd->index);

	mfd->fbi->screen_base = (char *) vaddr;
	mfd->fbi->fix.smem_start = (unsigned int) mfd->iova;
	mfd->fbi->fix.smem_len = fb_size;

	return rc;

fb_mmap_failed:
	ion_free(mfd->fb_ion_client, mfd->fb_ion_handle);
	return rc;
}

/**
 * mdss_fb_fbmem_ion_mmap() -  Custom fb  mmap() function for MSM driver.
 *
 * @info -  Framebuffer info.
 * @vma  -  VM area which is part of the process virtual memory.
 *
 * This framebuffer mmap function differs from standard mmap() function by
 * allowing for customized page-protection and dynamically allocate framebuffer
 * memory from system heap and map to iommu virtual address.
 *
 * Return: virtual address is returned through vma
 */
static int mdss_fb_fbmem_ion_mmap(struct fb_info *info,
		struct vm_area_struct *vma)
{
	int rc = 0;
	size_t req_size, fb_size;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct sg_table *table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	unsigned int i;
	struct page *page;

	if (!mfd || !mfd->pdev || !mfd->pdev->dev.of_node) {
		pr_err("Invalid device node\n");
		return -ENODEV;
	}

	req_size = vma->vm_end - vma->vm_start;
	fb_size = mfd->fbi->fix.line_length * mfd->fbi->var.yres * MDSS_FB_NUM;
	if (req_size > fb_size) {
		pr_warn("requested map is greater than framebuffer");
		return -EOVERFLOW;
	}

	if (!mfd->fbi->screen_base) {
		rc = mdss_fb_alloc_fb_ion_memory(mfd, fb_size);
		if (rc < 0) {
			pr_err("fb mmap failed!!!!");
			return rc;
		}
	}

	table = ion_sg_table(mfd->fb_ion_client, mfd->fb_ion_handle);
	if (IS_ERR(table)) {
		pr_err("Unable to get sg_table from ion:%ld\n", PTR_ERR(table));
		mfd->fbi->screen_base = NULL;
		mfd->fbi->fix.smem_len = 0;
		return PTR_ERR(table);
	} else if (!table) {
		pr_err("sg_list is NULL\n");
		mfd->fbi->screen_base = NULL;
		mfd->fbi->fix.smem_len = 0;
		return -EINVAL;
	}

	page = sg_page(table->sgl);
	if (page) {
		for_each_sg(table->sgl, sg, table->nents, i) {
			unsigned long remainder = vma->vm_end - addr;
			unsigned long len = sg_dma_len(sg);

			page = sg_page(sg);

			if (offset >= sg_dma_len(sg)) {
				offset -= sg_dma_len(sg);
				continue;
			} else if (offset) {
				page += offset / PAGE_SIZE;
				len = sg_dma_len(sg) - offset;
				offset = 0;
			}
			len = min(len, remainder);

			if (mfd->mdp_fb_page_protection ==
					MDP_FB_PAGE_PROTECTION_WRITECOMBINE)
				vma->vm_page_prot =
					pgprot_writecombine(vma->vm_page_prot);

			pr_debug("vma=%p, addr=%x len=%ld",
					vma, (unsigned int)addr, len);
			pr_cont("vm_start=%x vm_end=%x vm_page_prot=%ld\n",
					(unsigned int)vma->vm_start,
					(unsigned int)vma->vm_end,
					(unsigned long int)vma->vm_page_prot);

			io_remap_pfn_range(vma, addr, page_to_pfn(page), len,
					vma->vm_page_prot);
			addr += len;
			if (addr >= vma->vm_end)
				break;
		}
	} else {
		pr_err("PAGE is null\n");
		mdss_fb_free_fb_ion_memory(mfd);
		return -ENOMEM;
	}

	return rc;
}

/*
 * mdss_fb_physical_mmap() - Custom fb mmap() function for MSM driver.
 *
 * @info -  Framebuffer info.
 * @vma  -  VM area which is part of the process virtual memory.
 *
 * This framebuffer mmap function differs from standard mmap() function as
 * map to framebuffer memory from the CMA memory which is allocated during
 * bootup.
 *
 * Return: virtual address is returned through vma
 */
static int mdss_fb_physical_mmap(struct fb_info *info,
		struct vm_area_struct *vma)
{
	/* Get frame buffer memory range. */
	unsigned long start = info->fix.smem_start;
	u32 len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (!start) {
		pr_warn("No framebuffer memory is allocated\n");
		return -ENOMEM;
	}

	/* Set VM flags. */
	start &= PAGE_MASK;
	if ((vma->vm_end <= vma->vm_start) ||
			(off >= len) ||
			((vma->vm_end - vma->vm_start) > (len - off)))
		return -EINVAL;
	off += start;
	if (off < start)
		return -EINVAL;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO;

	if (mfd->mdp_fb_page_protection == MDP_FB_PAGE_PROTECTION_WRITECOMBINE)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* Remap the frame buffer I/O range */
	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static int mdss_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int rc = 0;

	if (!info->fix.smem_start && !mfd->fb_ion_handle)
		rc = mdss_fb_fbmem_ion_mmap(info, vma);
	else
		rc = mdss_fb_physical_mmap(info, vma);

	if (rc < 0)
		pr_err("fb mmap failed with rc = %d", rc);

	return rc;
}

static struct fb_ops mdss_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = mdss_fb_open,
	.fb_release = mdss_fb_release,
	.fb_check_var = mdss_fb_check_var,	/* vinfo check */
	.fb_set_par = mdss_fb_set_par,	/* set the video mode */
	.fb_blank = mdss_fb_blank,	/* blank display */
	.fb_pan_display = mdss_fb_pan_display,	/* pan display */
	.fb_ioctl = mdss_fb_ioctl,	/* perform fb specific ioctl */
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = mdss_fb_compat_ioctl,
#endif
	.fb_mmap = mdss_fb_mmap,
};

static int mdss_fb_alloc_fbmem_iommu(struct msm_fb_data_type *mfd, int dom)
{
	void *virt = NULL;
	phys_addr_t phys = 0;
	size_t size = 0;
	struct platform_device *pdev = mfd->pdev;
	int rc = 0;
	struct device_node *fbmem_pnode = NULL;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid device node\n");
		return -ENODEV;
	}

	fbmem_pnode = of_parse_phandle(pdev->dev.of_node,
		"linux,contiguous-region", 0);
	if (!fbmem_pnode) {
		pr_debug("fbmem is not reserved for %s\n", pdev->name);
		mfd->fbi->screen_base = NULL;
		mfd->fbi->fix.smem_start = 0;
		mfd->fbi->fix.smem_len = 0;
		return 0;
	} else {
		const u32 *addr;
		u64 len;

		addr = of_get_address(fbmem_pnode, 0, &len, NULL);
		if (!addr) {
			pr_err("fbmem size is not specified\n");
			of_node_put(fbmem_pnode);
			return -EINVAL;
		}
		size = (size_t)len;
		of_node_put(fbmem_pnode);
	}

	pr_debug("%s frame buffer reserve_size=0x%zx\n", __func__, size);

	if (size < PAGE_ALIGN(mfd->fbi->fix.line_length *
			      mfd->fbi->var.yres_virtual))
		pr_warn("reserve size is smaller than framebuffer size\n");

	virt = dma_alloc_coherent(&pdev->dev, size, &phys, GFP_KERNEL);
	if (!virt) {
		pr_err("unable to alloc fbmem size=%zx\n", size);
		return -ENOMEM;
	}

	if (MDSS_LPAE_CHECK(phys)) {
		pr_warn("fb mem phys %pa > 4GB is not supported.\n", &phys);
		dma_free_coherent(&pdev->dev, size, &virt, GFP_KERNEL);
		return -ERANGE;
	}

	rc = msm_iommu_map_contig_buffer(phys, dom, 0, size, SZ_4K, 0,
					    &mfd->iova);
	if (rc)
		pr_warn("Cannot map fb_mem %pa to IOMMU. rc=%d\n", &phys, rc);

	pr_debug("alloc 0x%zxB @ (%pa phys) (0x%p virt) (%pa iova) for fb%d\n",
		 size, &phys, virt, &mfd->iova, mfd->index);

	mfd->fbi->screen_base = virt;
	mfd->fbi->fix.smem_start = phys;
	mfd->fbi->fix.smem_len = size;

	return 0;
}

static int mdss_fb_alloc_fbmem(struct msm_fb_data_type *mfd)
{

	if (mfd->mdp.fb_mem_alloc_fnc) {
		return mfd->mdp.fb_mem_alloc_fnc(mfd);
	} else if (mfd->mdp.fb_mem_get_iommu_domain) {
		int dom = mfd->mdp.fb_mem_get_iommu_domain();
		if (dom >= 0)
			return mdss_fb_alloc_fbmem_iommu(mfd, dom);
		else
			return -ENOMEM;
	} else {
		pr_err("no fb memory allocator function defined\n");
		return -ENOMEM;
	}
}

static int mdss_fb_register(struct msm_fb_data_type *mfd)
{
	int ret = -ENODEV;
	int bpp;
	struct mdss_panel_info *panel_info = mfd->panel_info;
	struct fb_info *fbi = mfd->fbi;
	struct fb_fix_screeninfo *fix;
	struct fb_var_screeninfo *var;
	int *id;

	/*
	 * fb info initialization
	 */
	fix = &fbi->fix;
	var = &fbi->var;

	fix->type_aux = 0;	/* if type == FB_TYPE_INTERLEAVED_PLANES */
	fix->visual = FB_VISUAL_TRUECOLOR;	/* True Color */
	fix->ywrapstep = 0;	/* No support */
	fix->mmio_start = 0;	/* No MMIO Address */
	fix->mmio_len = 0;	/* No MMIO Address */
	fix->accel = FB_ACCEL_NONE;/* FB_ACCEL_MSM needes to be added in fb.h */

	var->xoffset = 0,	/* Offset from virtual to visible */
	var->yoffset = 0,	/* resolution */
	var->grayscale = 0,	/* No graylevels */
	var->nonstd = 0,	/* standard pixel format */
	var->activate = FB_ACTIVATE_VBL,	/* activate it at vsync */
	var->height = -1,	/* height of picture in mm */
	var->width = -1,	/* width of picture in mm */
	var->accel_flags = 0,	/* acceleration flags */
	var->sync = 0,	/* see FB_SYNC_* */
	var->rotate = 0,	/* angle we rotate counter clockwise */
	mfd->op_enable = false;

	switch (mfd->fb_imgType) {
	case MDP_RGB_565:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;
		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 11;
		var->blue.length = 5;
		var->green.length = 6;
		var->red.length = 5;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.offset = 0;
		var->transp.length = 0;
		bpp = 2;
		break;

	case MDP_RGB_888:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;
		var->blue.offset = 0;
		var->green.offset = 8;
		var->red.offset = 16;
		var->blue.length = 8;
		var->green.length = 8;
		var->red.length = 8;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.offset = 0;
		var->transp.length = 0;
		bpp = 3;
		break;

	case MDP_ARGB_8888:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;
		var->blue.offset = 24;
		var->green.offset = 16;
		var->red.offset = 8;
		var->blue.length = 8;
		var->green.length = 8;
		var->red.length = 8;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.offset = 0;
		var->transp.length = 8;
		bpp = 4;
		break;

	case MDP_RGBA_8888:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;
		var->blue.offset = 16;
		var->green.offset = 8;
		var->red.offset = 0;
		var->blue.length = 8;
		var->green.length = 8;
		var->red.length = 8;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.offset = 24;
		var->transp.length = 8;
		bpp = 4;
		break;

	case MDP_YCRYCB_H2V1:
		fix->type = FB_TYPE_INTERLEAVED_PLANES;
		fix->xpanstep = 2;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		/* how about R/G/B offset? */
		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 11;
		var->blue.length = 5;
		var->green.length = 6;
		var->red.length = 5;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.offset = 0;
		var->transp.length = 0;
		bpp = 2;
		break;

	default:
		pr_err("msm_fb_init: fb %d unkown image type!\n",
			    mfd->index);
		return ret;
	}

	var->xres = panel_info->xres;
	if (is_panel_split(mfd))
		var->xres *= 2;

	fix->type = panel_info->is_3d_panel;
	if (mfd->mdp.fb_stride)
		fix->line_length = mfd->mdp.fb_stride(mfd->index, var->xres,
							bpp);
	else
		fix->line_length = var->xres * bpp;

	var->yres = panel_info->yres;
	if (panel_info->physical_width)
		var->width = panel_info->physical_width;
	if (panel_info->physical_height)
		var->height = panel_info->physical_height;
	var->xres_virtual = var->xres;
	var->yres_virtual = panel_info->yres * mfd->fb_page;
	var->bits_per_pixel = bpp * 8;	/* FrameBuffer color depth */
	var->upper_margin = panel_info->lcdc.v_back_porch;
	var->lower_margin = panel_info->lcdc.v_front_porch;
	var->vsync_len = panel_info->lcdc.v_pulse_width;
	var->left_margin = panel_info->lcdc.h_back_porch;
	var->right_margin = panel_info->lcdc.h_front_porch;
	var->hsync_len = panel_info->lcdc.h_pulse_width;
	var->pixclock = panel_info->clk_rate / 1000;

	/*
	 * Populate smem length here for uspace to get the
	 * Franebuffer size when FBIO_FSCREENINFO ioctl is
	 * called.
	 */
	fix->smem_len = fix->line_length * var->yres_virtual;

	/* id field for fb app  */
	id = (int *)&mfd->panel;

	snprintf(fix->id, sizeof(fix->id), "mdssfb_%x", (u32) *id);

	fbi->fbops = &mdss_fb_ops;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->pseudo_palette = mdss_fb_pseudo_palette;

	mfd->ref_cnt = 0;
	mfd->panel_power_on = false;
	mfd->dcm_state = DCM_UNINIT;

	if (mdss_fb_alloc_fbmem(mfd))
		pr_warn("unable to allocate fb memory in fb register\n");

	mfd->op_enable = true;

	mutex_init(&mfd->update.lock);
	mutex_init(&mfd->no_update.lock);
	mutex_init(&mfd->mdp_sync_pt_data.sync_mutex);
	atomic_set(&mfd->mdp_sync_pt_data.commit_cnt, 0);
	atomic_set(&mfd->commits_pending, 0);
	atomic_set(&mfd->ioctl_ref_cnt, 0);
	atomic_set(&mfd->kickoff_pending, 0);

	init_timer(&mfd->no_update.timer);
	mfd->no_update.timer.function = mdss_fb_no_update_notify_timer_cb;
	mfd->no_update.timer.data = (unsigned long)mfd;
	mfd->update.ref_count = 0;
	mfd->no_update.ref_count = 0;
	init_completion(&mfd->update.comp);
	init_completion(&mfd->no_update.comp);
	init_completion(&mfd->power_off_comp);
	init_completion(&mfd->power_set_comp);
	init_waitqueue_head(&mfd->commit_wait_q);
	init_waitqueue_head(&mfd->idle_wait_q);
	init_waitqueue_head(&mfd->ioctl_q);
	init_waitqueue_head(&mfd->kickoff_wait_q);

	ret = fb_alloc_cmap(&fbi->cmap, 256, 0);
	if (ret)
		pr_err("fb_alloc_cmap() failed!\n");

	if (register_framebuffer(fbi) < 0) {
		fb_dealloc_cmap(&fbi->cmap);

		mfd->op_enable = false;
		return -EPERM;
	}

	pr_info("FrameBuffer[%d] %dx%d registered successfully!\n", mfd->index,
					fbi->var.xres, fbi->var.yres);

	return 0;
}

/**
 * mdss_fb_release_file_entry() - Releases file node entry from list
 * @info:	Frame buffer info
 * @pinfo:	Process list node in which file node entry is going to
 *		be removed
 * @release_all: Releases all file node entries from list if this parameter
 *		is true
 *
 * This function is called to remove the file node entry/entries from main
 * list. It also helps to find the process id if fb_open and fb_close
 * callers are different.
 */
static struct mdss_fb_proc_info *mdss_fb_release_file_entry(
		struct fb_info *info,
		struct mdss_fb_proc_info *pinfo, bool release_all)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdss_fb_file_info *file_info = NULL, *temp_file_info = NULL;
	struct mdss_fb_proc_info *proc_info = NULL, *temp_proc_info = NULL;
	struct file *file = info->file;
	bool node_found = false;

	if (!pinfo && release_all) {
		pr_err("process node not provided for release all case\n");
		goto end;
	}

	if (pinfo) {
		proc_info = pinfo;
		list_for_each_entry_safe(file_info, temp_file_info,
						&pinfo->file_list, list) {
			if (!release_all && file_info->file != file)
				continue;

			list_del(&file_info->list);
			kfree(file_info);

			node_found = true;

			if (!release_all)
				break;
		}
	}

	if (!node_found) {
		list_for_each_entry_safe(proc_info, temp_proc_info,
						&mfd->proc_list, list) {
			list_for_each_entry_safe(file_info, temp_file_info,
						&proc_info->file_list, list) {
				if (file_info->file == file) {
					list_del(&file_info->list);
					kfree(file_info);
					goto end;
				}
			}
		}
	}

end:
	return proc_info;
}

static int mdss_fb_open(struct fb_info *info, int user)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdss_fb_proc_info *pinfo = NULL;
	struct mdss_fb_file_info *file_info = NULL;
	int result;
	int pid = current->tgid;
	struct task_struct *task = current->group_leader;

	if (mfd->shutdown_pending) {
		pr_err("Shutdown pending. Aborting operation. Request from pid:%d name=%s\n",
				pid, task->comm);
		return -EPERM;
	}

	file_info = kmalloc(sizeof(*file_info), GFP_KERNEL);
	if (!file_info) {
		pr_err("unable to alloc file info\n");
		return -ENOMEM;
	}

	list_for_each_entry(pinfo, &mfd->proc_list, list) {
		if (pinfo->pid == pid)
			break;
	}

	if ((pinfo == NULL) || (pinfo->pid != pid)) {
		pinfo = kmalloc(sizeof(*pinfo), GFP_KERNEL);
		if (!pinfo) {
			pr_err("unable to alloc process info\n");
			kfree(file_info);
			return -ENOMEM;
		}
		pinfo->pid = pid;
		pinfo->ref_cnt = 0;
		list_add(&pinfo->list, &mfd->proc_list);
		INIT_LIST_HEAD(&pinfo->file_list);
		pr_debug("new process entry pid=%d\n", pinfo->pid);
	}

	file_info->file = info->file;
	list_add(&file_info->list, &pinfo->file_list);

	result = pm_runtime_get_sync(info->dev);

	if (result < 0) {
		pr_err("pm_runtime: fail to wake up\n");
		goto pm_error;
	}

	if (!mfd->ref_cnt) {
		mdss_fb_get_split(mfd);
		mfd->disp_thread = kthread_run(__mdss_fb_display_thread, mfd,
				"mdss_fb%d", mfd->index);
		if (IS_ERR(mfd->disp_thread)) {
			pr_err("unable to start display thread %d\n",
				mfd->index);
			result = PTR_ERR(mfd->disp_thread);
			mfd->disp_thread = NULL;
			goto thread_error;
		}

		result = mdss_fb_blank_sub(FB_BLANK_UNBLANK, info,
					   mfd->op_enable);
		if (result) {
			pr_err("can't turn on fb%d! rc=%d\n", mfd->index,
				result);
			goto blank_error;
		}
	}

	pinfo->ref_cnt++;
	mfd->ref_cnt++;

	return 0;

blank_error:
	kthread_stop(mfd->disp_thread);
	mfd->disp_thread = NULL;

thread_error:
	pm_runtime_put(info->dev);

pm_error:
	if (pinfo && !pinfo->ref_cnt) {
		list_del(&pinfo->list);
		kfree(pinfo);
	}
	list_del(&file_info->list);
	kfree(file_info);
	return result;
}

static int mdss_fb_release_all(struct fb_info *info, bool release_all)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdss_fb_proc_info *pinfo = NULL, *temp_pinfo = NULL;
	struct mdss_fb_proc_info *proc_info = NULL;
	int ret = 0;
	int pid = current->tgid;
	bool unknown_pid = true, release_needed = false;
	struct task_struct *task = current->group_leader;

	if (!mfd->ref_cnt) {
		pr_info("try to close unopened fb %d! from %s\n", mfd->index,
			task->comm);
		return -EINVAL;
	}

	if (!wait_event_timeout(mfd->ioctl_q,
		!atomic_read(&mfd->ioctl_ref_cnt) || !release_all,
		msecs_to_jiffies(1000)))
		pr_warn("fb%d ioctl could not finish. waited 1 sec.\n",
			mfd->index);

	mdss_fb_pan_idle(mfd);

	pr_debug("release_all = %s\n", release_all ? "true" : "false");

	list_for_each_entry_safe(pinfo, temp_pinfo, &mfd->proc_list, list) {
		if (!release_all && (pinfo->pid != pid))
			continue;

		unknown_pid = false;

		pr_debug("found process %s pid=%d mfd->ref=%d pinfo->ref=%d\n",
			task->comm, mfd->ref_cnt, pinfo->pid, pinfo->ref_cnt);

		proc_info = mdss_fb_release_file_entry(info, pinfo,
								release_all);
		/*
		 * if fb_release is called from different known process then
		 * release the ref_count of original proc_info instead of
		 * current process.
		 */
		if (!release_all && proc_info && proc_info != pinfo) {
			pr_info("fb_release called from different process for current file node\n");
			pinfo = proc_info;
		}

		do {
			if (mfd->ref_cnt < pinfo->ref_cnt)
				pr_warn("WARN:mfd->ref=%d < pinfo->ref=%d\n",
					mfd->ref_cnt, pinfo->ref_cnt);
			else
				mfd->ref_cnt--;

			pinfo->ref_cnt--;
			pm_runtime_put(info->dev);
		} while (release_all && pinfo->ref_cnt);

		if (release_all && mfd->disp_thread) {
			kthread_stop(mfd->disp_thread);
			mfd->disp_thread = NULL;
		}

		if (pinfo->ref_cnt == 0) {
			list_del(&pinfo->list);
			kfree(pinfo);
			release_needed = !release_all;
		}

		if (!release_all)
			break;
	}

	if (unknown_pid) {
		pinfo = mdss_fb_release_file_entry(info, NULL, false);
		if (pinfo) {
			mfd->ref_cnt--;
			pinfo->ref_cnt--;
			pm_runtime_put(info->dev);
			if (!pinfo->ref_cnt) {
				list_del(&pinfo->list);
				kfree(pinfo);
				release_needed = true;
			}
		} else {
			WARN("unknown caller:: process %s mfd->ref=%d\n",
				task->comm, mfd->ref_cnt);
		}
	}

	if (release_needed) {
		pr_debug("known process %s pid=%d mfd->ref=%d\n",
			task->comm, pid, mfd->ref_cnt);

		if (mfd->mdp.release_fnc) {
			ret = mfd->mdp.release_fnc(mfd, false);
			if (ret)
				pr_err("error releasing fb%d pid=%d\n",
					mfd->index, pid);
		}
	} else if (release_all && mfd->ref_cnt) {
		pr_err("reference count mismatch with proc list entries\n");
	}

	if (!mfd->ref_cnt) {
		if (mfd->disp_thread) {
			kthread_stop(mfd->disp_thread);
			mfd->disp_thread = NULL;
		}

		if (mfd->mdp.release_fnc) {
			ret = mfd->mdp.release_fnc(mfd, true);
			if (ret)
				pr_err("error fb%d release process %s pid=%d\n",
					mfd->index, task->comm, pid);
		}

		if (mfd->fb_ion_handle)
			mdss_fb_free_fb_ion_memory(mfd);

		ret = mdss_fb_blank_sub(FB_BLANK_POWERDOWN, info,
			mfd->op_enable);
		if (ret) {
			pr_err("can't turn off fb%d! rc=%d process %s pid=%d\n",
				mfd->index, ret, task->comm, pid);
			return ret;
		}
		atomic_set(&mfd->ioctl_ref_cnt, 0);
	}

	return ret;
}

static int mdss_fb_release(struct fb_info *info, int user)
{
	return mdss_fb_release_all(info, false);
}

static void mdss_fb_power_setting_idle(struct msm_fb_data_type *mfd)
{
	int ret;

	if (mfd->is_power_setting) {
		ret = wait_for_completion_timeout(
				&mfd->power_set_comp,
			msecs_to_jiffies(WAIT_DISP_OP_TIMEOUT));
		if (ret < 0)
			ret = -ERESTARTSYS;
		else if (!ret)
			pr_err("%s wait for power_set_comp timeout %d %d",
				__func__, ret, mfd->is_power_setting);
		if (ret <= 0) {
			mfd->is_power_setting = false;
			complete_all(&mfd->power_set_comp);
		}
	}
}

int mdss_fb_wait_for_fence(struct msm_sync_pt_data *sync_pt_data)
{
	struct sync_fence *fences[MDP_MAX_FENCE_FD];
	int fence_cnt;
	int i, ret = 0;

	pr_debug("%s: wait for fences\n", sync_pt_data->fence_name);

	mutex_lock(&sync_pt_data->sync_mutex);
	/*
	 * Assuming that acq_fen_cnt is sanitized in bufsync ioctl
	 * to check for sync_pt_data->acq_fen_cnt <= MDP_MAX_FENCE_FD
	 */
	fence_cnt = sync_pt_data->acq_fen_cnt;
	sync_pt_data->acq_fen_cnt = 0;
	if (fence_cnt)
		memcpy(fences, sync_pt_data->acq_fen,
				fence_cnt * sizeof(struct sync_fence *));
	mutex_unlock(&sync_pt_data->sync_mutex);

	/* buf sync */
	for (i = 0; i < fence_cnt && !ret; i++) {
		ret = sync_fence_wait(fences[i],
				WAIT_FENCE_FIRST_TIMEOUT);
		if (ret == -ETIME) {
			pr_warn("%s: sync_fence_wait timed out! ",
					sync_pt_data->fence_name);
			pr_cont("Waiting %ld more seconds\n",
					WAIT_FENCE_FINAL_TIMEOUT/MSEC_PER_SEC);
			ret = sync_fence_wait(fences[i],
					WAIT_FENCE_FINAL_TIMEOUT);
		}
		sync_fence_put(fences[i]);
	}

	if (ret < 0) {
		pr_err("%s: sync_fence_wait failed! ret = %x\n",
				sync_pt_data->fence_name, ret);
		for (; i < fence_cnt; i++)
			sync_fence_put(fences[i]);
	}

	return fence_cnt;
}

/**
 * mdss_fb_signal_timeline() - signal a single release fence
 * @sync_pt_data:	Sync point data structure for the timeline which
 *			should be signaled.
 *
 * This is called after a frame has been pushed to display. This signals the
 * timeline to release the fences associated with this frame.
 */
void mdss_fb_signal_timeline(struct msm_sync_pt_data *sync_pt_data)
{
	mutex_lock(&sync_pt_data->sync_mutex);
	if (atomic_add_unless(&sync_pt_data->commit_cnt, -1, 0) &&
			sync_pt_data->timeline) {
		sw_sync_timeline_inc(sync_pt_data->timeline, 1);
		sync_pt_data->timeline_value++;

		pr_debug("%s: buffer signaled! timeline val=%d remaining=%d\n",
			sync_pt_data->fence_name, sync_pt_data->timeline_value,
			atomic_read(&sync_pt_data->commit_cnt));
	} else {
		pr_debug("%s timeline signaled without commits val=%d\n",
			sync_pt_data->fence_name, sync_pt_data->timeline_value);
	}
	mutex_unlock(&sync_pt_data->sync_mutex);
}

/**
 * mdss_fb_release_fences() - signal all pending release fences
 * @mfd:	Framebuffer data structure for display
 *
 * Release all currently pending release fences, including those that are in
 * the process to be commited.
 *
 * Note: this should only be called during close or suspend sequence.
 */
static void mdss_fb_release_fences(struct msm_fb_data_type *mfd)
{
	struct msm_sync_pt_data *sync_pt_data = &mfd->mdp_sync_pt_data;
	int val;

	mutex_lock(&sync_pt_data->sync_mutex);
	if (sync_pt_data->timeline) {
		val = sync_pt_data->threshold +
			atomic_read(&sync_pt_data->commit_cnt);
		sw_sync_timeline_inc(sync_pt_data->timeline, val);
		sync_pt_data->timeline_value += val;
		atomic_set(&sync_pt_data->commit_cnt, 0);
	}
	mutex_unlock(&sync_pt_data->sync_mutex);
}

static void mdss_fb_release_kickoff(struct msm_fb_data_type *mfd)
{
	if (mfd->wait_for_kickoff) {
		atomic_set(&mfd->kickoff_pending, 0);
		wake_up_all(&mfd->kickoff_wait_q);
	}
}

/**
 * __mdss_fb_sync_buf_done_callback() - process async display events
 * @p:		Notifier block registered for async events.
 * @event:	Event enum to identify the event.
 * @data:	Optional argument provided with the event.
 *
 * See enum mdp_notify_event for events handled.
 */
static int __mdss_fb_sync_buf_done_callback(struct notifier_block *p,
		unsigned long event, void *data)
{
	struct msm_sync_pt_data *sync_pt_data;
	struct msm_fb_data_type *mfd;

	sync_pt_data = container_of(p, struct msm_sync_pt_data, notifier);
	mfd = container_of(sync_pt_data, struct msm_fb_data_type,
		mdp_sync_pt_data);

	switch (event) {
	case MDP_NOTIFY_FRAME_BEGIN:
		if (mfd->idle_time)
			mod_delayed_work(system_wq, &mfd->idle_notify_work,
				msecs_to_jiffies(mfd->idle_time));
		break;
	case MDP_NOTIFY_FRAME_READY:
		if (sync_pt_data->async_wait_fences)
			mdss_fb_wait_for_fence(sync_pt_data);
		break;
	case MDP_NOTIFY_FRAME_FLUSHED:
		pr_debug("%s: frame flushed\n", sync_pt_data->fence_name);
		sync_pt_data->flushed = true;
		break;
	case MDP_NOTIFY_FRAME_TIMEOUT:
		pr_err("%s: frame timeout\n", sync_pt_data->fence_name);
		mdss_fb_signal_timeline(sync_pt_data);
		break;
	case MDP_NOTIFY_FRAME_DONE:
		pr_debug("%s: frame done\n", sync_pt_data->fence_name);
		mdss_fb_signal_timeline(sync_pt_data);
		break;
	case MDP_NOTIFY_FRAME_START:
		mdss_fb_release_kickoff(mfd);
		break;
	}

	return NOTIFY_OK;
}

/**
 * mdss_fb_pan_idle() - wait for panel programming to be idle
 * @mfd:	Framebuffer data structure for display
 *
 * Wait for any pending programming to be done if in the process of programming
 * hardware configuration. After this function returns it is safe to perform
 * software updates for next frame.
 */
static int mdss_fb_pan_idle(struct msm_fb_data_type *mfd)
{
	int ret = 0;

	ret = wait_event_timeout(mfd->idle_wait_q,
			(!atomic_read(&mfd->commits_pending) ||
			 mfd->shutdown_pending),
			msecs_to_jiffies(WAIT_DISP_OP_TIMEOUT));
	if (!ret) {
		pr_err("wait for idle timeout %d pending=%d\n",
				ret, atomic_read(&mfd->commits_pending));

		mdss_fb_signal_timeline(&mfd->mdp_sync_pt_data);
	} else if (mfd->shutdown_pending) {
		pr_debug("Shutdown signalled\n");
		return -EPERM;
	}

	return 0;
}

static int mdss_fb_wait_for_kickoff(struct msm_fb_data_type *mfd)
{
	int ret = 0;

	ret = wait_event_timeout(mfd->kickoff_wait_q,
			(!atomic_read(&mfd->kickoff_pending) ||
			 mfd->shutdown_pending),
			msecs_to_jiffies(WAIT_DISP_OP_TIMEOUT / 2));
	if (!ret) {
		pr_err("wait for kickoff timeout %d pending=%d\n",
				ret, atomic_read(&mfd->kickoff_pending));

	} else if (mfd->shutdown_pending) {
		pr_debug("Shutdown signalled\n");
		return -EPERM;
	}

	return 0;
}

static int mdss_fb_pan_display_ex(struct fb_info *info,
		struct mdp_display_commit *disp_commit)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_var_screeninfo *var = &disp_commit->var;
	u32 wait_for_finish = disp_commit->wait_for_finish;
	int ret = 0;

	if (!mfd || (!mfd->op_enable))
		return -EPERM;

	if ((!mfd->panel_power_on) && !((mfd->dcm_state == DCM_ENTER) &&
				(mfd->panel.type == MIPI_CMD_PANEL)))
		return -EPERM;

	if (var->xoffset > (info->var.xres_virtual - info->var.xres))
		return -EINVAL;

	if (var->yoffset > (info->var.yres_virtual - info->var.yres))
		return -EINVAL;

	ret = mdss_fb_pan_idle(mfd);
	if (ret) {
		pr_err("Shutdown pending. Aborting operation\n");
		return ret;
	}

	mutex_lock(&mfd->mdp_sync_pt_data.sync_mutex);
	if (info->fix.xpanstep)
		info->var.xoffset =
		(var->xoffset / info->fix.xpanstep) * info->fix.xpanstep;

	if (info->fix.ypanstep)
		info->var.yoffset =
		(var->yoffset / info->fix.ypanstep) * info->fix.ypanstep;

	mfd->msm_fb_backup.info = *info;
	mfd->msm_fb_backup.disp_commit = *disp_commit;

	atomic_inc(&mfd->mdp_sync_pt_data.commit_cnt);
	atomic_inc(&mfd->commits_pending);
	atomic_inc(&mfd->kickoff_pending);
	wake_up_all(&mfd->commit_wait_q);
	mutex_unlock(&mfd->mdp_sync_pt_data.sync_mutex);
	if (wait_for_finish)
		mdss_fb_pan_idle(mfd);
	return ret;
}

static int mdss_fb_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	struct mdp_display_commit disp_commit;
	memset(&disp_commit, 0, sizeof(disp_commit));
	disp_commit.wait_for_finish = true;
	memcpy(&disp_commit.var, var, sizeof(struct fb_var_screeninfo));
	return mdss_fb_pan_display_ex(info, &disp_commit);
}

static int mdss_fb_pan_display_sub(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (!mfd->op_enable)
		return -EPERM;

	if ((!mfd->panel_power_on) && !((mfd->dcm_state == DCM_ENTER) &&
				(mfd->panel.type == MIPI_CMD_PANEL)))
		return -EPERM;

	if (var->xoffset > (info->var.xres_virtual - info->var.xres))
		return -EINVAL;

	if (var->yoffset > (info->var.yres_virtual - info->var.yres))
		return -EINVAL;

	if (info->fix.xpanstep)
		info->var.xoffset =
		(var->xoffset / info->fix.xpanstep) * info->fix.xpanstep;

	if (info->fix.ypanstep)
		info->var.yoffset =
		(var->yoffset / info->fix.ypanstep) * info->fix.ypanstep;

	if (mfd->mdp.dma_fnc)
		mfd->mdp.dma_fnc(mfd);
	else
		pr_warn("dma function not set for panel type=%d\n",
				mfd->panel.type);

	return 0;
}

static void mdss_fb_var_to_panelinfo(struct fb_var_screeninfo *var,
	struct mdss_panel_info *pinfo)
{
	pinfo->xres = var->xres;
	pinfo->yres = var->yres;
	pinfo->lcdc.v_front_porch = var->lower_margin;
	pinfo->lcdc.v_back_porch = var->upper_margin;
	pinfo->lcdc.v_pulse_width = var->vsync_len;
	pinfo->lcdc.h_front_porch = var->right_margin;
	pinfo->lcdc.h_back_porch = var->left_margin;
	pinfo->lcdc.h_pulse_width = var->hsync_len;
	pinfo->clk_rate = var->pixclock;
}

/**
 * __mdss_fb_perform_commit() - process a frame to display
 * @mfd:	Framebuffer data structure for display
 *
 * Processes all layers and buffers programmed and ensures all pending release
 * fences are signaled once the buffer is transfered to display.
 */
static int __mdss_fb_perform_commit(struct msm_fb_data_type *mfd)
{
	struct msm_sync_pt_data *sync_pt_data = &mfd->mdp_sync_pt_data;
	struct msm_fb_backup_type *fb_backup = &mfd->msm_fb_backup;
	int ret = -ENOSYS;

	if (!sync_pt_data->async_wait_fences)
		mdss_fb_wait_for_fence(sync_pt_data);
	sync_pt_data->flushed = false;

	if (fb_backup->disp_commit.flags & MDP_DISPLAY_COMMIT_OVERLAY) {
		if (mfd->mdp.kickoff_fnc)
			ret = mfd->mdp.kickoff_fnc(mfd,
					&fb_backup->disp_commit);
		else
			pr_warn("no kickoff function setup for fb%d\n",
					mfd->index);
	} else {
		ret = mdss_fb_pan_display_sub(&fb_backup->disp_commit.var,
				&fb_backup->info);
		if (ret)
			pr_err("pan display failed %x on fb%d\n", ret,
					mfd->index);
	}
	if (!ret)
		mdss_fb_update_backlight(mfd);

	if (IS_ERR_VALUE(ret) || !sync_pt_data->flushed) {
		mdss_fb_release_kickoff(mfd);
		mdss_fb_signal_timeline(sync_pt_data);
	}
	return ret;
}

static int __mdss_fb_display_thread(void *data)
{
	struct msm_fb_data_type *mfd = data;
	int ret;
	struct sched_param param;

	/*
	 * this priority was found during empiric testing to have appropriate
	 * realtime scheduling to process display updates and interact with
	 * other real time and normal priority tasks
	 */
	param.sched_priority = 16;
	ret = sched_setscheduler(current, SCHED_FIFO, &param);
	if (ret)
		pr_warn("set priority failed for fb%d display thread\n",
				mfd->index);

	while (1) {
		wait_event(mfd->commit_wait_q,
				(atomic_read(&mfd->commits_pending) ||
				 kthread_should_stop()));

		if (kthread_should_stop())
			break;

		ret = __mdss_fb_perform_commit(mfd);
		atomic_dec(&mfd->commits_pending);
		wake_up_all(&mfd->idle_wait_q);
	}

	mdss_fb_release_kickoff(mfd);
	atomic_set(&mfd->commits_pending, 0);
	wake_up_all(&mfd->idle_wait_q);

	return ret;
}

static int mdss_fb_check_var(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (var->rotate != FB_ROTATE_UR)
		return -EINVAL;
	if (var->grayscale != info->var.grayscale)
		return -EINVAL;

	switch (var->bits_per_pixel) {
	case 16:
		if ((var->green.offset != 5) ||
		    !((var->blue.offset == 11)
		      || (var->blue.offset == 0)) ||
		    !((var->red.offset == 11)
		      || (var->red.offset == 0)) ||
		    (var->blue.length != 5) ||
		    (var->green.length != 6) ||
		    (var->red.length != 5) ||
		    (var->blue.msb_right != 0) ||
		    (var->green.msb_right != 0) ||
		    (var->red.msb_right != 0) ||
		    (var->transp.offset != 0) ||
		    (var->transp.length != 0))
			return -EINVAL;
		break;

	case 24:
		if ((var->blue.offset != 0) ||
		    (var->green.offset != 8) ||
		    (var->red.offset != 16) ||
		    (var->blue.length != 8) ||
		    (var->green.length != 8) ||
		    (var->red.length != 8) ||
		    (var->blue.msb_right != 0) ||
		    (var->green.msb_right != 0) ||
		    (var->red.msb_right != 0) ||
		    !(((var->transp.offset == 0) &&
		       (var->transp.length == 0)) ||
		      ((var->transp.offset == 24) &&
		       (var->transp.length == 8))))
			return -EINVAL;
		break;

	case 32:
		/* Check user specified color format BGRA/ARGB/RGBA
		   and verify the position of the RGB components */

		if (!((var->transp.offset == 24) &&
			(var->blue.offset == 0) &&
			(var->green.offset == 8) &&
			(var->red.offset == 16)) &&
		    !((var->transp.offset == 24) &&
			(var->blue.offset == 16) &&
			(var->green.offset == 8) &&
			(var->red.offset == 0)))
				return -EINVAL;

		/* Check the common values for both RGBA and ARGB */

		if ((var->blue.length != 8) ||
		    (var->green.length != 8) ||
		    (var->red.length != 8) ||
		    (var->transp.length != 8) ||
		    (var->blue.msb_right != 0) ||
		    (var->green.msb_right != 0) ||
		    (var->red.msb_right != 0))
			return -EINVAL;

		break;

	default:
		return -EINVAL;
	}

	if ((var->xres_virtual <= 0) || (var->yres_virtual <= 0))
		return -EINVAL;

	if (info->fix.smem_start) {
		u32 len = var->xres_virtual * var->yres_virtual *
			(var->bits_per_pixel / 8);
		if (len > info->fix.smem_len)
			return -EINVAL;
	}

	if ((var->xres == 0) || (var->yres == 0))
		return -EINVAL;

	if (var->xoffset > (var->xres_virtual - var->xres))
		return -EINVAL;

	if (var->yoffset > (var->yres_virtual - var->yres))
		return -EINVAL;

	if (mfd->panel_info) {
		struct mdss_panel_info panel_info;
		int rc;

		memcpy(&panel_info, mfd->panel_info, sizeof(panel_info));
		mdss_fb_var_to_panelinfo(var, &panel_info);
		rc = mdss_fb_send_panel_event(mfd, MDSS_EVENT_CHECK_PARAMS,
			&panel_info);
		if (IS_ERR_VALUE(rc))
			return rc;
		mfd->panel_reconfig = rc;
	}

	return 0;
}

static int mdss_fb_set_par(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_var_screeninfo *var = &info->var;
	int old_imgType;
	int ret = 0;

	ret = mdss_fb_pan_idle(mfd);
	if (ret) {
		pr_err("Shutdown pending. Aborting operation\n");
		return ret;
	}

	old_imgType = mfd->fb_imgType;
	switch (var->bits_per_pixel) {
	case 16:
		if (var->red.offset == 0)
			mfd->fb_imgType = MDP_BGR_565;
		else
			mfd->fb_imgType	= MDP_RGB_565;
		break;

	case 24:
		if ((var->transp.offset == 0) && (var->transp.length == 0))
			mfd->fb_imgType = MDP_RGB_888;
		else if ((var->transp.offset == 24) &&
			 (var->transp.length == 8)) {
			mfd->fb_imgType = MDP_ARGB_8888;
			info->var.bits_per_pixel = 32;
		}
		break;

	case 32:
		if ((var->red.offset == 0) &&
		    (var->green.offset == 8) &&
		    (var->blue.offset == 16) &&
		    (var->transp.offset == 24))
			mfd->fb_imgType = MDP_RGBA_8888;
		else if ((var->red.offset == 16) &&
		    (var->green.offset == 8) &&
		    (var->blue.offset == 0) &&
		    (var->transp.offset == 24))
			mfd->fb_imgType = MDP_BGRA_8888;
		else if ((var->red.offset == 8) &&
		    (var->green.offset == 16) &&
		    (var->blue.offset == 24) &&
		    (var->transp.offset == 0))
			mfd->fb_imgType = MDP_ARGB_8888;
		else
			mfd->fb_imgType = MDP_RGBA_8888;
		break;

	default:
		return -EINVAL;
	}


	if (mfd->mdp.fb_stride)
		mfd->fbi->fix.line_length = mfd->mdp.fb_stride(mfd->index,
						var->xres,
						var->bits_per_pixel / 8);
	else
		mfd->fbi->fix.line_length = var->xres * var->bits_per_pixel / 8;


	if (mfd->panel_reconfig || (mfd->fb_imgType != old_imgType)) {
		mdss_fb_blank_sub(FB_BLANK_POWERDOWN, info, mfd->op_enable);
		mdss_fb_var_to_panelinfo(var, mfd->panel_info);
		mdss_fb_blank_sub(FB_BLANK_UNBLANK, info, mfd->op_enable);
		mfd->panel_reconfig = false;
	}

	return ret;
}

int mdss_fb_dcm(struct msm_fb_data_type *mfd, int req_state)
{
	int ret = 0;

	if (req_state == mfd->dcm_state) {
		pr_warn("Already in correct DCM/DTM state");
		return ret;
	}

	switch (req_state) {
	case DCM_UNBLANK:
		if (mfd->dcm_state == DCM_UNINIT &&
			!mfd->panel_power_on && mfd->mdp.on_fnc) {
			ret = mfd->mdp.on_fnc(mfd);
			if (ret == 0) {
				mfd->panel_power_on = true;
				mfd->dcm_state = DCM_UNBLANK;
			}
		}
		break;
	case DCM_ENTER:
		if (mfd->dcm_state == DCM_UNBLANK) {
			/*
			 * Keep unblank path available for only
			 * DCM operation
			 */
			mfd->panel_power_on = false;
			mfd->dcm_state = DCM_ENTER;
		}
		break;
	case DCM_EXIT:
		if (mfd->dcm_state == DCM_ENTER) {
			/* Release the unblank path for exit */
			mfd->panel_power_on = true;
			mfd->dcm_state = DCM_EXIT;
		}
		break;
	case DCM_BLANK:
		if ((mfd->dcm_state == DCM_EXIT ||
			mfd->dcm_state == DCM_UNBLANK) &&
			mfd->panel_power_on && mfd->mdp.off_fnc) {
			ret = mfd->mdp.off_fnc(mfd);
			if (ret == 0) {
				mfd->panel_power_on = false;
				mfd->dcm_state = DCM_UNINIT;
			}
		}
		break;
	case DTM_ENTER:
		if (mfd->dcm_state == DCM_UNINIT)
			mfd->dcm_state = DTM_ENTER;
		break;
	case DTM_EXIT:
		if (mfd->dcm_state == DTM_ENTER)
			mfd->dcm_state = DCM_UNINIT;
		break;
	}

	return ret;
}

static int mdss_fb_cursor(struct fb_info *info, void __user *p)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_cursor cursor;
	int ret;

	if (!mfd->mdp.cursor_update)
		return -ENODEV;

	ret = copy_from_user(&cursor, p, sizeof(cursor));
	if (ret)
		return ret;

	return mfd->mdp.cursor_update(mfd, &cursor);
}

static int mdss_fb_set_lut(struct fb_info *info, void __user *p)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_cmap cmap;
	int ret;

	if (!mfd->mdp.lut_update)
		return -ENODEV;

	ret = copy_from_user(&cmap, p, sizeof(cmap));
	if (ret)
		return ret;

	mfd->mdp.lut_update(mfd, &cmap);
	return 0;
}

/**
 * mdss_fb_sync_get_fence() - get fence from timeline
 * @timeline:	Timeline to create the fence on
 * @fence_name:	Name of the fence that will be created for debugging
 * @val:	Timeline value at which the fence will be signaled
 *
 * Function returns a fence on the timeline given with the name provided.
 * The fence created will be signaled when the timeline is advanced.
 */
struct sync_fence *mdss_fb_sync_get_fence(struct sw_sync_timeline *timeline,
		const char *fence_name, int val)
{
	struct sync_pt *sync_pt;
	struct sync_fence *fence;

	pr_debug("%s: buf sync fence timeline=%d\n", fence_name, val);

	sync_pt = sw_sync_pt_create(timeline, val);
	if (sync_pt == NULL) {
		pr_err("%s: cannot create sync point\n", fence_name);
		return NULL;
	}

	/* create fence */
	fence = sync_fence_create(fence_name, sync_pt);
	if (fence == NULL) {
		sync_pt_free(sync_pt);
		pr_err("%s: cannot create fence\n", fence_name);
		return NULL;
	}

	return fence;
}

static int mdss_fb_handle_buf_sync_ioctl(struct msm_sync_pt_data *sync_pt_data,
				 struct mdp_buf_sync *buf_sync)
{
	int i, ret = 0;
	int acq_fen_fd[MDP_MAX_FENCE_FD];
	struct sync_fence *fence, *rel_fence, *retire_fence;
	int rel_fen_fd;
	int retire_fen_fd;
	int val;

	if ((buf_sync->acq_fen_fd_cnt > MDP_MAX_FENCE_FD) ||
				(sync_pt_data->timeline == NULL))
		return -EINVAL;

	if (buf_sync->acq_fen_fd_cnt)
		ret = copy_from_user(acq_fen_fd, buf_sync->acq_fen_fd,
				buf_sync->acq_fen_fd_cnt * sizeof(int));
	if (ret) {
		pr_err("%s: copy_from_user failed", sync_pt_data->fence_name);
		return ret;
	}

	i = mdss_fb_wait_for_fence(sync_pt_data);
	if (i > 0)
		pr_warn("%s: waited on %d active fences\n",
				sync_pt_data->fence_name, i);

	mutex_lock(&sync_pt_data->sync_mutex);
	for (i = 0; i < buf_sync->acq_fen_fd_cnt; i++) {
		fence = sync_fence_fdget(acq_fen_fd[i]);
		if (fence == NULL) {
			pr_err("%s: null fence! i=%d fd=%d\n",
					sync_pt_data->fence_name, i,
					acq_fen_fd[i]);
			ret = -EINVAL;
			break;
		}
		sync_pt_data->acq_fen[i] = fence;
	}
	sync_pt_data->acq_fen_cnt = i;
	if (ret)
		goto buf_sync_err_1;

	val = sync_pt_data->timeline_value + sync_pt_data->threshold +
			atomic_read(&sync_pt_data->commit_cnt);

	/* Set release fence */
	rel_fence = mdss_fb_sync_get_fence(sync_pt_data->timeline,
			sync_pt_data->fence_name, val);
	if (IS_ERR_OR_NULL(rel_fence)) {
		pr_err("%s: unable to retrieve release fence\n",
				sync_pt_data->fence_name);
		ret = rel_fence ? PTR_ERR(rel_fence) : -ENOMEM;
		goto buf_sync_err_1;
	}

	/* create fd */
	rel_fen_fd = get_unused_fd_flags(0);
	if (rel_fen_fd < 0) {
		pr_err("%s: get_unused_fd_flags failed error:0x%x\n",
				sync_pt_data->fence_name, rel_fen_fd);
		ret = rel_fen_fd;
		goto buf_sync_err_2;
	}

	sync_fence_install(rel_fence, rel_fen_fd);

	ret = copy_to_user(buf_sync->rel_fen_fd, &rel_fen_fd, sizeof(int));
	if (ret) {
		pr_err("%s: copy_to_user failed\n", sync_pt_data->fence_name);
		goto buf_sync_err_3;
	}

	if (!(buf_sync->flags & MDP_BUF_SYNC_FLAG_RETIRE_FENCE))
		goto skip_retire_fence;

	if (sync_pt_data->get_retire_fence)
		retire_fence = sync_pt_data->get_retire_fence(sync_pt_data);
	else
		retire_fence = NULL;

	if (IS_ERR_OR_NULL(retire_fence)) {
		val += sync_pt_data->retire_threshold;
		retire_fence = mdss_fb_sync_get_fence(
			sync_pt_data->timeline, "mdp-retire", val);
	}

	if (IS_ERR_OR_NULL(retire_fence)) {
		pr_err("%s: unable to retrieve retire fence\n",
				sync_pt_data->fence_name);
		ret = retire_fence ? PTR_ERR(rel_fence) : -ENOMEM;
		goto buf_sync_err_3;
	}
	retire_fen_fd = get_unused_fd_flags(0);

	if (retire_fen_fd < 0) {
		pr_err("%s: get_unused_fd_flags failed for retire fence error:0x%x\n",
				sync_pt_data->fence_name, retire_fen_fd);
		ret = retire_fen_fd;
		sync_fence_put(retire_fence);
		goto buf_sync_err_3;
	}

	sync_fence_install(retire_fence, retire_fen_fd);

	ret = copy_to_user(buf_sync->retire_fen_fd, &retire_fen_fd,
			sizeof(int));
	if (ret) {
		pr_err("%s: copy_to_user failed for retire fence\n",
				sync_pt_data->fence_name);
		put_unused_fd(retire_fen_fd);
		sync_fence_put(retire_fence);
		goto buf_sync_err_3;
	}

skip_retire_fence:
	mutex_unlock(&sync_pt_data->sync_mutex);

	if (buf_sync->flags & MDP_BUF_SYNC_FLAG_WAIT)
		mdss_fb_wait_for_fence(sync_pt_data);

	return ret;
buf_sync_err_3:
	put_unused_fd(rel_fen_fd);
buf_sync_err_2:
	sync_fence_put(rel_fence);
buf_sync_err_1:
	for (i = 0; i < sync_pt_data->acq_fen_cnt; i++)
		sync_fence_put(sync_pt_data->acq_fen[i]);
	sync_pt_data->acq_fen_cnt = 0;
	mutex_unlock(&sync_pt_data->sync_mutex);
	return ret;
}
static int mdss_fb_display_commit(struct fb_info *info,
						unsigned long *argp)
{
	int ret;
	struct mdp_display_commit disp_commit;
	ret = copy_from_user(&disp_commit, argp,
			sizeof(disp_commit));
	if (ret) {
		pr_err("%s:copy_from_user failed", __func__);
		return ret;
	}
	ret = mdss_fb_pan_display_ex(info, &disp_commit);
	return ret;
}

static int __ioctl_wait_idle(struct msm_fb_data_type *mfd, u32 cmd)
{
	int ret = 0;

	if (mfd->wait_for_kickoff &&
		((cmd == MSMFB_OVERLAY_PREPARE) ||
		(cmd == MSMFB_BUFFER_SYNC) ||
		(cmd == MSMFB_OVERLAY_SET))) {
		ret = mdss_fb_wait_for_kickoff(mfd);
	} else if ((cmd != MSMFB_VSYNC_CTRL) &&
		(cmd != MSMFB_OVERLAY_VSYNC_CTRL) &&
		(cmd != MSMFB_ASYNC_BLIT) &&
		(cmd != MSMFB_BLIT) &&
		(cmd != MSMFB_NOTIFY_UPDATE) &&
		(cmd != MSMFB_OVERLAY_PREPARE)) {
		ret = mdss_fb_pan_idle(mfd);
	}

	if (ret)
		pr_debug("Shutdown pending. Aborting operation %x\n", cmd);
	return ret;
}

/*
 * mdss_fb_do_ioctl() - MDSS Framebuffer ioctl function
 * @info:	pointer to framebuffer info
 * @cmd:	ioctl command
 * @arg:	argument to ioctl
 *
 * This function provides an architecture agnostic implementation
 * of the mdss framebuffer ioctl. This function can be called
 * by compat ioctl or regular ioctl to handle the supported commands.
 */
int mdss_fb_do_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	struct msm_fb_data_type *mfd;
	void __user *argp = (void __user *)arg;
	struct mdp_page_protection fb_page_protection;
	int ret = -ENOSYS;
	struct mdp_buf_sync buf_sync;
	struct msm_sync_pt_data *sync_pt_data = NULL;
	unsigned int dsi_mode = 0;

	if (!info || !info->par)
		return -EINVAL;

	mfd = (struct msm_fb_data_type *)info->par;
	if (!mfd)
		return -EINVAL;

	if (mfd->shutdown_pending)
		return -EPERM;

	atomic_inc(&mfd->ioctl_ref_cnt);

	mdss_fb_power_setting_idle(mfd);

	ret = __ioctl_wait_idle(mfd, cmd);
	if (ret)
		goto exit;

	switch (cmd) {
	case MSMFB_CURSOR:
		ret = mdss_fb_cursor(info, argp);
		break;

	case MSMFB_SET_LUT:
		ret = mdss_fb_set_lut(info, argp);
		break;

	case MSMFB_GET_PAGE_PROTECTION:
		fb_page_protection.page_protection =
			mfd->mdp_fb_page_protection;
		ret = copy_to_user(argp, &fb_page_protection,
				   sizeof(fb_page_protection));
		if (ret)
			goto exit;
		break;

	case MSMFB_BUFFER_SYNC:
		ret = copy_from_user(&buf_sync, argp, sizeof(buf_sync));
		if (ret)
			goto exit;

		if ((!mfd->op_enable) || (!mfd->panel_power_on)) {
			ret = -EPERM;
			goto exit;
		}

		if (mfd->mdp.get_sync_fnc)
			sync_pt_data = mfd->mdp.get_sync_fnc(mfd, &buf_sync);
		if (!sync_pt_data)
			sync_pt_data = &mfd->mdp_sync_pt_data;

		ret = mdss_fb_handle_buf_sync_ioctl(sync_pt_data, &buf_sync);

		if (!ret)
			ret = copy_to_user(argp, &buf_sync, sizeof(buf_sync));
		break;

	case MSMFB_NOTIFY_UPDATE:
		ret = mdss_fb_notify_update(mfd, argp);
		break;

	case MSMFB_DISPLAY_COMMIT:
		ret = mdss_fb_display_commit(info, argp);
		break;

	case MSMFB_LPM_ENABLE:
		ret = copy_from_user(&dsi_mode, argp, sizeof(dsi_mode));
		if (ret) {
			pr_err("%s: MSMFB_LPM_ENABLE ioctl failed\n", __func__);
			goto exit;
		}

		ret = mdss_fb_lpm_enable(mfd, dsi_mode);
		break;

	default:
		if (mfd->mdp.ioctl_handler)
			ret = mfd->mdp.ioctl_handler(mfd, cmd, argp);
		break;
	}

	if (ret == -ENOSYS)
		pr_err("unsupported ioctl (%x)\n", cmd);

exit:
	if (!atomic_dec_return(&mfd->ioctl_ref_cnt))
		wake_up_all(&mfd->ioctl_q);

	return ret;
}

static int mdss_fb_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	if (!info || !info->par)
		return -EINVAL;

	return mdss_fb_do_ioctl(info, cmd, arg);
}

struct fb_info *msm_fb_get_writeback_fb(void)
{
	int c = 0;
	for (c = 0; c < fbi_list_index; ++c) {
		struct msm_fb_data_type *mfd;
		mfd = (struct msm_fb_data_type *)fbi_list[c]->par;
		if (mfd->panel.type == WRITEBACK_PANEL)
			return fbi_list[c];
	}

	return NULL;
}
EXPORT_SYMBOL(msm_fb_get_writeback_fb);

static int mdss_fb_register_extra_panel(struct platform_device *pdev,
	struct mdss_panel_data *pdata)
{
	struct mdss_panel_data *fb_pdata;

	fb_pdata = dev_get_platdata(&pdev->dev);
	if (!fb_pdata) {
		pr_err("framebuffer device %s contains invalid panel data\n",
				dev_name(&pdev->dev));
		return -EINVAL;
	}

	if (fb_pdata->next) {
		pr_err("split panel already setup for framebuffer device %s\n",
				dev_name(&pdev->dev));
		return -EEXIST;
	}

	fb_pdata->next = pdata;

	return 0;
}

int mdss_register_panel(struct platform_device *pdev,
	struct mdss_panel_data *pdata)
{
	struct platform_device *fb_pdev, *mdss_pdev;
	struct device_node *node;
	int rc = 0;
	bool master_panel = true;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid device node\n");
		return -ENODEV;
	}

	if (!mdp_instance) {
		pr_err("mdss mdp resource not initialized yet\n");
		return -EPROBE_DEFER;
	}

	node = of_parse_phandle(pdev->dev.of_node, "qcom,mdss-fb-map", 0);
	if (!node) {
		pr_err("Unable to find fb node for device: %s\n",
				pdev->name);
		return -ENODEV;
	}
	mdss_pdev = of_find_device_by_node(node->parent);
	if (!mdss_pdev) {
		pr_err("Unable to find mdss for node: %s\n", node->full_name);
		rc = -ENODEV;
		goto mdss_notfound;
	}

	fb_pdev = of_find_device_by_node(node);
	if (fb_pdev) {
		rc = mdss_fb_register_extra_panel(fb_pdev, pdata);
		if (rc == 0)
			master_panel = false;
	} else {
		pr_info("adding framebuffer device %s\n", dev_name(&pdev->dev));
		fb_pdev = of_platform_device_create(node, NULL,
				&mdss_pdev->dev);
		if (fb_pdev)
			fb_pdev->dev.platform_data = pdata;
	}

	if (master_panel && mdp_instance->panel_register_done)
		mdp_instance->panel_register_done(pdata);

mdss_notfound:
	of_node_put(node);
	return rc;
}
EXPORT_SYMBOL(mdss_register_panel);

int mdss_fb_register_mdp_instance(struct msm_mdp_interface *mdp)
{
	if (mdp_instance) {
		pr_err("multiple MDP instance registration");
		return -EINVAL;
	}

	mdp_instance = mdp;
	return 0;
}
EXPORT_SYMBOL(mdss_fb_register_mdp_instance);

int mdss_fb_get_phys_info(dma_addr_t *start, unsigned long *len, int fb_num)
{
	struct fb_info *info;
	struct msm_fb_data_type *mfd;

	if (fb_num >= MAX_FBI_LIST)
		return -EINVAL;

	info = fbi_list[fb_num];
	if (!info)
		return -ENOENT;

	mfd = (struct msm_fb_data_type *)info->par;
	if (!mfd)
		return -ENODEV;

	if (mfd->iova)
		*start = mfd->iova;
	else
		*start = info->fix.smem_start;
	*len = info->fix.smem_len;

	return 0;
}
EXPORT_SYMBOL(mdss_fb_get_phys_info);

int __init mdss_fb_init(void)
{
	int rc = -ENODEV;

	if (platform_driver_register(&mdss_fb_driver))
		return rc;

	return 0;
}

module_init(mdss_fb_init);

int mdss_fb_suspres_panel(struct device *dev, void *data)
{
	struct msm_fb_data_type *mfd;
	int rc;
	u32 event;

	if (!data) {
		pr_err("Device state not defined\n");
		return -EINVAL;
	}
	mfd = dev_get_drvdata(dev);
	if (!mfd)
		return 0;

	event = *((bool *) data) ? MDSS_EVENT_RESUME : MDSS_EVENT_SUSPEND;

	rc = mdss_fb_send_panel_event(mfd, event, NULL);
	if (rc)
		pr_warn("unable to %s fb%d (%d)\n",
			event == MDSS_EVENT_RESUME ? "resume" : "suspend",
			mfd->index, rc);
	return rc;
}
