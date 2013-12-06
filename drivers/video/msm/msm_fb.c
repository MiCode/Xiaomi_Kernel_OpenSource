/* drivers/video/msm/msm_fb.c
 *
 * Core MSM framebuffer driver.
 *
 * Copyright (C) 2007 Google Incorporated
 * Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <mach/board.h>
#include <linux/uaccess.h>
#include <linux/msm_iommu_domains.h>

#include <linux/workqueue.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/console.h>
#include <linux/leds.h>
#include <linux/pm_runtime.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include <linux/file.h>

#define MSM_FB_C

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_NUM	3
#endif

static unsigned char *fbram;
static unsigned char *fbram_phys;
static int fbram_size;
static boolean bf_supported;
/* Set backlight on resume after 50 ms after first
 * pan display on the panel. This is to avoid panel specific
 * transients during resume.
 */
unsigned long backlight_duration = (HZ/20);

static struct platform_device *pdev_list[MSM_FB_MAX_DEV_LIST];
static int pdev_list_cnt;

int vsync_mode = 1;

#define MAX_BLIT_REQ 256

#define MAX_FBI_LIST 32
static struct fb_info *fbi_list[MAX_FBI_LIST];
static int fbi_list_index;

static struct msm_fb_data_type *mfd_list[MAX_FBI_LIST];
static int mfd_list_index;

static u32 msm_fb_pseudo_palette[16] = {
	0x00000000, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

static struct ion_client *iclient;

u32 msm_fb_debug_enabled;
/* Setting msm_fb_msg_level to 8 prints out ALL messages */
u32 msm_fb_msg_level = 7;

/* Setting mddi_msg_level to 8 prints out ALL messages */
u32 mddi_msg_level = 5;

extern int32 mdp_block_power_cnt[MDP_MAX_BLOCK];
extern unsigned long mdp_timer_duration;

static int msm_fb_register(struct msm_fb_data_type *mfd);
static int msm_fb_open(struct fb_info *info, int user);
static int msm_fb_release(struct fb_info *info, int user);
static int msm_fb_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info);
static int msm_fb_stop_sw_refresher(struct msm_fb_data_type *mfd);
int msm_fb_resume_sw_refresher(struct msm_fb_data_type *mfd);
static int msm_fb_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info);
static int msm_fb_set_par(struct fb_info *info);
static int msm_fb_blank_sub(int blank_mode, struct fb_info *info,
			    boolean op_enable);
static int msm_fb_suspend_sub(struct msm_fb_data_type *mfd);
static int msm_fb_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg);
static int msm_fb_mmap(struct fb_info *info, struct vm_area_struct * vma);
static int mdp_bl_scale_config(struct msm_fb_data_type *mfd,
						struct mdp_bl_scale_data *data);
static void msm_fb_scale_bl(__u32 *bl_lvl);
static void msm_fb_commit_wq_handler(struct work_struct *work);
static int msm_fb_pan_idle(struct msm_fb_data_type *mfd);

#ifdef MSM_FB_ENABLE_DBGFS

#define MSM_FB_MAX_DBGFS 1024
#define MAX_BACKLIGHT_BRIGHTNESS 255

/* 200 ms for time out */
#define WAIT_FENCE_TIMEOUT 200

int msm_fb_debugfs_file_index;
struct dentry *msm_fb_debugfs_root;
struct dentry *msm_fb_debugfs_file[MSM_FB_MAX_DBGFS];
static int bl_scale, bl_min_lvl;

DEFINE_MUTEX(msm_fb_notify_update_sem);
void msmfb_no_update_notify_timer_cb(unsigned long data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)data;
	if (!mfd)
		pr_err("%s mfd NULL\n", __func__);
	complete(&mfd->msmfb_no_update_notify);
}

struct dentry *msm_fb_get_debugfs_root(void)
{
	if (msm_fb_debugfs_root == NULL)
		msm_fb_debugfs_root = debugfs_create_dir("msm_fb", NULL);

	return msm_fb_debugfs_root;
}

void msm_fb_debugfs_file_create(struct dentry *root, const char *name,
				u32 *var)
{
	if (msm_fb_debugfs_file_index >= MSM_FB_MAX_DBGFS)
		return;

	msm_fb_debugfs_file[msm_fb_debugfs_file_index++] =
	    debugfs_create_u32(name, S_IRUGO | S_IWUSR, root, var);
}
#endif

int msm_fb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (!mfd->cursor_update)
		return -ENODEV;

	return mfd->cursor_update(info, cursor);
}

static int msm_fb_resource_initialized;

static int lcd_backlight_registered;

static void msm_fb_set_bl_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	struct msm_fb_data_type *mfd = dev_get_drvdata(led_cdev->dev->parent);
	int bl_lvl;

	if (value > MAX_BACKLIGHT_BRIGHTNESS)
		value = MAX_BACKLIGHT_BRIGHTNESS;

	/* This maps android backlight level 0 to 255 into
	   driver backlight level 0 to bl_max with rounding */
	bl_lvl = (2 * value * mfd->panel_info.bl_max + MAX_BACKLIGHT_BRIGHTNESS)
		/(2 * MAX_BACKLIGHT_BRIGHTNESS);

	if (!bl_lvl && value)
		bl_lvl = 1;
	down(&mfd->sem);
	msm_fb_set_backlight(mfd, bl_lvl);
	up(&mfd->sem);
}

static struct led_classdev backlight_led = {
	.name		= "lcd-backlight",
	.brightness	= MAX_BACKLIGHT_BRIGHTNESS,
	.brightness_set	= msm_fb_set_bl_brightness,
};

static struct msm_fb_platform_data *msm_fb_pdata;
unsigned char hdmi_prim_display;
unsigned char hdmi_prim_resolution;

int msm_fb_detect_client(const char *name)
{
	int ret = 0;
	u32 len;
#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
	u32 id;
#endif
	if (!msm_fb_pdata)
		return -EPERM;

	len = strnlen(name, PANEL_NAME_MAX_LEN);
	if (strnlen(msm_fb_pdata->prim_panel_name, PANEL_NAME_MAX_LEN)) {
		pr_err("\n name = %s, prim_display = %s",
			name, msm_fb_pdata->prim_panel_name);
		if (!strncmp((char *)msm_fb_pdata->prim_panel_name,
			name, len)) {
			if (!strncmp((char *)msm_fb_pdata->prim_panel_name,
				"hdmi_msm", len))
				hdmi_prim_display = 1;
				hdmi_prim_resolution =
					msm_fb_pdata->ext_resolution;
			return 0;
		} else {
			ret = -EPERM;
		}
	}

	if (strnlen(msm_fb_pdata->ext_panel_name, PANEL_NAME_MAX_LEN)) {
		pr_err("\n name = %s, ext_display = %s",
			name, msm_fb_pdata->ext_panel_name);
		if (!strncmp((char *)msm_fb_pdata->ext_panel_name, name, len))
			return 0;
		else
			ret = -EPERM;
	}

	if (ret)
		return ret;

	ret = -EPERM;
	if (msm_fb_pdata && msm_fb_pdata->detect_client) {
		ret = msm_fb_pdata->detect_client(name);

		/* if it's non mddi panel, we need to pre-scan
		   mddi client to see if we can disable mddi host */

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
		if (!ret && msm_fb_pdata->mddi_prescan)
			id = mddi_get_client_id();
#endif
	}

	return ret;
}

static ssize_t msm_fb_msm_fb_type(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct msm_fb_panel_data *pdata =
		(struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;

	switch (pdata->panel_info.type) {
	case NO_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "no panel\n");
		break;
	case MDDI_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "mddi panel\n");
		break;
	case EBI2_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "ebi2 panel\n");
		break;
	case LCDC_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "lcdc panel\n");
		break;
	case EXT_MDDI_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "ext mddi panel\n");
		break;
	case TV_PANEL:
		ret = snprintf(buf, PAGE_SIZE, "tv panel\n");
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
	default:
		ret = snprintf(buf, PAGE_SIZE, "unknown panel\n");
		break;
	}

	return ret;
}

static DEVICE_ATTR(msm_fb_type, S_IRUGO, msm_fb_msm_fb_type, NULL);
static struct attribute *msm_fb_attrs[] = {
	&dev_attr_msm_fb_type.attr,
	NULL,
};
static struct attribute_group msm_fb_attr_group = {
	.attrs = msm_fb_attrs,
};

static int msm_fb_create_sysfs(struct platform_device *pdev)
{
	int rc;
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	rc = sysfs_create_group(&mfd->fbi->dev->kobj, &msm_fb_attr_group);
	if (rc)
		MSM_FB_ERR("%s: sysfs group creation failed, rc=%d\n", __func__,
			rc);
	return rc;
}
static void msm_fb_remove_sysfs(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);
	sysfs_remove_group(&mfd->fbi->dev->kobj, &msm_fb_attr_group);
}

static void bl_workqueue_handler(struct work_struct *work);

static int msm_fb_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	int rc;
	int err = 0;

	MSM_FB_DEBUG("msm_fb_probe\n");

	if ((pdev->id == 0) && (pdev->num_resources > 0)) {
		msm_fb_pdata = pdev->dev.platform_data;
		fbram_size =
			pdev->resource[0].end - pdev->resource[0].start + 1;
		fbram_phys = (char *)pdev->resource[0].start;
		fbram = __va(fbram_phys);

		if (!fbram) {
			printk(KERN_ERR "fbram ioremap failed!\n");
			return -ENOMEM;
		}
		MSM_FB_DEBUG("msm_fb_probe:  phy_Addr = 0x%x virt = 0x%x\n",
			     (int)fbram_phys, (int)fbram);

		iclient = msm_ion_client_create(-1, pdev->name);
		if (IS_ERR_OR_NULL(iclient)) {
			pr_err("msm_ion_client_create() return"
				" error, val %p\n", iclient);
			iclient = NULL;
		}

		msm_fb_resource_initialized = 1;
		return 0;
	}

	if (!msm_fb_resource_initialized)
		return -EPERM;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	INIT_DELAYED_WORK(&mfd->backlight_worker, bl_workqueue_handler);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (pdev_list_cnt >= MSM_FB_MAX_DEV_LIST)
		return -ENOMEM;

	vsync_cntrl.dev = mfd->fbi->dev;
	mfd->panel_info.frame_count = 0;
	mfd->bl_level = 0;
	bl_scale = 1024;
	bl_min_lvl = 255;
#ifdef CONFIG_FB_MSM_OVERLAY
	mfd->overlay_play_enable = 1;
#endif

	bf_supported = mdp4_overlay_borderfill_supported();

	rc = msm_fb_register(mfd);
	if (rc)
		return rc;
	err = pm_runtime_set_active(mfd->fbi->dev);
	if (err < 0)
		printk(KERN_ERR "pm_runtime: fail to set active.\n");
	pm_runtime_enable(mfd->fbi->dev);
	/* android supports only one lcd-backlight/lcd for now */
	if (!lcd_backlight_registered) {
		if (led_classdev_register(&pdev->dev, &backlight_led))
			printk(KERN_ERR "led_classdev_register failed\n");
		else
			lcd_backlight_registered = 1;
	}

	pdev_list[pdev_list_cnt++] = pdev;
	msm_fb_create_sysfs(pdev);
	if (mfd->timeline == NULL) {
		mfd->timeline = sw_sync_timeline_create("mdp-timeline");
		if (mfd->timeline == NULL) {
			pr_err("%s: cannot create time line", __func__);
			return -ENOMEM;
		} else {
			mfd->timeline_value = 0;
		}
	}

	return 0;
}

static int msm_fb_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	MSM_FB_DEBUG("msm_fb_remove\n");

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	msm_fb_remove_sysfs(pdev);

	pm_runtime_disable(mfd->fbi->dev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (msm_fb_suspend_sub(mfd))
		printk(KERN_ERR "msm_fb_remove: can't stop the device %d\n", mfd->index);

	if (mfd->channel_irq != 0)
		free_irq(mfd->channel_irq, (void *)mfd);

	if (mfd->vsync_width_boundary)
		vfree(mfd->vsync_width_boundary);

	if (mfd->vsync_resync_timer.function)
		del_timer(&mfd->vsync_resync_timer);

	if (mfd->refresh_timer.function)
		del_timer(&mfd->refresh_timer);

	if (mfd->dma_hrtimer.function)
		hrtimer_cancel(&mfd->dma_hrtimer);

	if (mfd->msmfb_no_update_notify_timer.function)
		del_timer(&mfd->msmfb_no_update_notify_timer);
	complete(&mfd->msmfb_no_update_notify);
	complete(&mfd->msmfb_update_notify);

	/* remove /dev/fb* */
	unregister_framebuffer(mfd->fbi);

	if (lcd_backlight_registered) {
		lcd_backlight_registered = 0;
		led_classdev_unregister(&backlight_led);
	}

#ifdef MSM_FB_ENABLE_DBGFS
	if (mfd->sub_dir)
		debugfs_remove(mfd->sub_dir);
#endif

	return 0;
}

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int msm_fb_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct msm_fb_data_type *mfd;
	int ret = 0;

	MSM_FB_DEBUG("msm_fb_suspend\n");

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if ((!mfd) || (mfd->key != MFD_KEY))
		return 0;

	console_lock();
	fb_set_suspend(mfd->fbi, FBINFO_STATE_SUSPENDED);

	ret = msm_fb_suspend_sub(mfd);
	if (ret != 0) {
		printk(KERN_ERR "msm_fb: failed to suspend! %d\n", ret);
		fb_set_suspend(mfd->fbi, FBINFO_STATE_RUNNING);
	} else {
		pdev->dev.power.power_state = state;
	}

	console_unlock();
	return ret;
}
#else
#define msm_fb_suspend NULL
#endif

static int msm_fb_suspend_sub(struct msm_fb_data_type *mfd)
{
	int ret = 0;

	if ((!mfd) || (mfd->key != MFD_KEY))
		return 0;

	if (mfd->msmfb_no_update_notify_timer.function)
		del_timer(&mfd->msmfb_no_update_notify_timer);
	complete(&mfd->msmfb_no_update_notify);

	/*
	 * suspend this channel
	 */
	mfd->suspend.sw_refreshing_enable = mfd->sw_refreshing_enable;
	mfd->suspend.op_enable = mfd->op_enable;
	mfd->suspend.panel_power_on = mfd->panel_power_on;
	mfd->suspend.op_suspend = true;

	if (mfd->op_enable) {
		ret =
		     msm_fb_blank_sub(FB_BLANK_POWERDOWN, mfd->fbi,
				      mfd->suspend.op_enable);
		if (ret) {
			MSM_FB_INFO
			    ("msm_fb_suspend: can't turn off display!\n");
			return ret;
		}
		mfd->op_enable = FALSE;
	}
	/*
	 * try to power down
	 */
	mdp_pipe_ctrl(MDP_MASTER_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	/*
	 * detach display channel irq if there's any
	 * or wait until vsync-resync completes
	 */
	if ((mfd->dest == DISPLAY_LCD)) {
		if (mfd->panel_info.lcd.vsync_enable) {
			if (mfd->panel_info.lcd.hw_vsync_mode) {
				if (mfd->channel_irq != 0)
					disable_irq(mfd->channel_irq);
			} else {
				volatile boolean vh_pending;
				do {
					vh_pending = mfd->vsync_handler_pending;
				} while (vh_pending);
			}
		}
	}

	return 0;
}

#ifdef CONFIG_PM
static int msm_fb_resume_sub(struct msm_fb_data_type *mfd)
{
	int ret = 0;
	struct msm_fb_panel_data *pdata = NULL;

	if ((!mfd) || (mfd->key != MFD_KEY))
		return 0;

	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;

	/* attach display channel irq if there's any */
	if (mfd->channel_irq != 0)
		enable_irq(mfd->channel_irq);

	/* resume state var recover */
	mfd->sw_refreshing_enable = mfd->suspend.sw_refreshing_enable;
	mfd->op_enable = mfd->suspend.op_enable;

	if (mfd->suspend.panel_power_on) {
		ret =
		     msm_fb_blank_sub(FB_BLANK_UNBLANK, mfd->fbi,
				      mfd->op_enable);
		if (ret)
			MSM_FB_INFO("msm_fb_resume: can't turn on display!\n");
	}

	mfd->suspend.op_suspend = false;

	return ret;
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int msm_fb_resume(struct platform_device *pdev)
{
	/* This resume function is called when interrupt is enabled.
	 */
	int ret = 0;
	struct msm_fb_data_type *mfd;

	MSM_FB_DEBUG("msm_fb_resume\n");

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if ((!mfd) || (mfd->key != MFD_KEY))
		return 0;

	console_lock();
	ret = msm_fb_resume_sub(mfd);
	pdev->dev.power.power_state = PMSG_ON;
	fb_set_suspend(mfd->fbi, FBINFO_STATE_RUNNING);
	console_unlock();

	return ret;
}
#else
#define msm_fb_resume NULL
#endif

static int msm_fb_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int msm_fb_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static int msm_fb_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: idling...\n");
	return 0;
}


static struct dev_pm_ops msm_fb_dev_pm_ops = {
	.runtime_suspend = msm_fb_runtime_suspend,
	.runtime_resume = msm_fb_runtime_resume,
	.runtime_idle = msm_fb_runtime_idle,
};

static struct platform_driver msm_fb_driver = {
	.probe = msm_fb_probe,
	.remove = msm_fb_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = msm_fb_suspend,
	.resume = msm_fb_resume,
#endif
	.shutdown = NULL,
	.driver = {
		   /* Driver name must match the device name added in platform.c. */
		   .name = "msm_fb",
		   .pm = &msm_fb_dev_pm_ops,
		   },
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void msmfb_early_suspend(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd = container_of(h, struct msm_fb_data_type,
						    early_suspend);
	msm_fb_suspend_sub(mfd);
}

static void msmfb_early_resume(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd = container_of(h, struct msm_fb_data_type,
						    early_suspend);
	msm_fb_resume_sub(mfd);
}
#endif

static int unset_bl_level, bl_updated;
static int bl_level_old;
static int mdp_bl_scale_config(struct msm_fb_data_type *mfd,
						struct mdp_bl_scale_data *data)
{
	int ret = 0;
	int curr_bl;
	down(&mfd->sem);
	curr_bl = mfd->bl_level;
	bl_scale = data->scale;
	bl_min_lvl = data->min_lvl;
	pr_debug("%s: update scale = %d, min_lvl = %d\n", __func__, bl_scale,
								bl_min_lvl);

	/* update current backlight to use new scaling*/
	msm_fb_set_backlight(mfd, curr_bl);
	up(&mfd->sem);

	return ret;
}

static void msm_fb_scale_bl(__u32 *bl_lvl)
{
	__u32 temp = *bl_lvl;
	pr_debug("%s: input = %d, scale = %d", __func__, temp, bl_scale);
	if (temp >= bl_min_lvl) {
		/* bl_scale is the numerator of scaling fraction (x/1024)*/
		temp = ((*bl_lvl) * bl_scale) / 1024;

		/*if less than minimum level, use min level*/
		if (temp < bl_min_lvl)
			temp = bl_min_lvl;
	}
	pr_debug("%s: output = %d", __func__, temp);

	(*bl_lvl) = temp;
}

/*must call this function from within mfd->sem*/
void msm_fb_set_backlight(struct msm_fb_data_type *mfd, __u32 bkl_lvl)
{
	struct msm_fb_panel_data *pdata;
	__u32 temp = bkl_lvl;
	if (!mfd->panel_power_on || !bl_updated) {
		unset_bl_level = bkl_lvl;
		return;
	} else {
		unset_bl_level = 0;
	}

	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;

	if ((pdata) && (pdata->set_backlight)) {
		msm_fb_scale_bl(&temp);
		if (bl_level_old == temp) {
			return;
		}
		mfd->bl_level = temp;
		pdata->set_backlight(mfd);
		mfd->bl_level = bkl_lvl;
		bl_level_old = temp;
	}
}

static int msm_fb_blank_sub(int blank_mode, struct fb_info *info,
			    boolean op_enable)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct msm_fb_panel_data *pdata = NULL;
	int ret = 0;

	if (!op_enable)
		return -EPERM;

	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;
	if ((!pdata) || (!pdata->on) || (!pdata->off)) {
		printk(KERN_ERR "msm_fb_blank_sub: no panel operation detected!\n");
		return -ENODEV;
	}

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		if (!mfd->panel_power_on) {
			msleep(16);
			ret = pdata->on(mfd->pdev);
			if (ret == 0) {
				mfd->panel_power_on = TRUE;

/* ToDo: possible conflict with android which doesn't expect sw refresher */
/*
	  if (!mfd->hw_refresh)
	  {
	    if ((ret = msm_fb_resume_sw_refresher(mfd)) != 0)
	    {
	      MSM_FB_INFO("msm_fb_blank_sub: msm_fb_resume_sw_refresher failed = %d!\n",ret);
	    }
	  }
*/
			}
		}
		break;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
	case FB_BLANK_POWERDOWN:
	default:
		if (mfd->panel_power_on) {
			int curr_pwr_state;

			mfd->op_enable = FALSE;
			curr_pwr_state = mfd->panel_power_on;
			mfd->panel_power_on = FALSE;
			cancel_delayed_work_sync(&mfd->backlight_worker);
			bl_updated = 0;

			msleep(16);
			ret = pdata->off(mfd->pdev);
			if (ret)
				mfd->panel_power_on = curr_pwr_state;

			if (mfd->timeline) {
				/* Adding 1 is enough when pan_display is still
				 * a blocking call and with mutex protection.
				 * But if it is an async call, we will still
				 * need to add 2. Adding 2 can be safer in
				 * order to signal all existing fences, and it
				 * is harmless. */
				sw_sync_timeline_inc(mfd->timeline, 2);
				mfd->timeline_value += 2;
			}

			mfd->op_enable = TRUE;
		}
		break;
	}

	return ret;
}

int calc_fb_offset(struct msm_fb_data_type *mfd, struct fb_info *fbi, int bpp)
{
	struct msm_panel_info *panel_info = &mfd->panel_info;
	int remainder, yres, offset;

	if (panel_info->mode2_yres != 0) {
		yres = panel_info->mode2_yres;
		remainder = (fbi->fix.line_length*yres) & (PAGE_SIZE - 1);
	} else {
		yres = panel_info->yres;
		remainder = (fbi->fix.line_length*yres) & (PAGE_SIZE - 1);
	}

	if (!remainder)
		remainder = PAGE_SIZE;

	if (fbi->var.yoffset < yres) {
		offset = (fbi->var.xoffset * bpp);
				/* iBuf->buf +=	fbi->var.xoffset * bpp + 0 *
				yres * fbi->fix.line_length; */
	} else if (fbi->var.yoffset >= yres && fbi->var.yoffset < 2 * yres) {
		offset = (fbi->var.xoffset * bpp + yres *
		fbi->fix.line_length + PAGE_SIZE - remainder);
	} else {
		offset = (fbi->var.xoffset * bpp + 2 * yres *
		fbi->fix.line_length + 2 * (PAGE_SIZE - remainder));
	}
	return offset;
}

static void msm_fb_fillrect(struct fb_info *info,
			    const struct fb_fillrect *rect)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	msm_fb_pan_idle(mfd);
	cfb_fillrect(info, rect);
	if (!mfd->hw_refresh && (info->var.yoffset == 0) &&
		!mfd->sw_currently_refreshing) {
		struct fb_var_screeninfo var;

		var = info->var;
		var.reserved[0] = 0x54445055;
		var.reserved[1] = (rect->dy << 16) | (rect->dx);
		var.reserved[2] = ((rect->dy + rect->height) << 16) |
		    (rect->dx + rect->width);

		msm_fb_pan_display(&var, info);
	}
}

static void msm_fb_copyarea(struct fb_info *info,
			    const struct fb_copyarea *area)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	msm_fb_pan_idle(mfd);
	cfb_copyarea(info, area);
	if (!mfd->hw_refresh && (info->var.yoffset == 0) &&
		!mfd->sw_currently_refreshing) {
		struct fb_var_screeninfo var;

		var = info->var;
		var.reserved[0] = 0x54445055;
		var.reserved[1] = (area->dy << 16) | (area->dx);
		var.reserved[2] = ((area->dy + area->height) << 16) |
		    (area->dx + area->width);

		msm_fb_pan_display(&var, info);
	}
}

static void msm_fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	msm_fb_pan_idle(mfd);
	cfb_imageblit(info, image);
	if (!mfd->hw_refresh && (info->var.yoffset == 0) &&
		!mfd->sw_currently_refreshing) {
		struct fb_var_screeninfo var;

		var = info->var;
		var.reserved[0] = 0x54445055;
		var.reserved[1] = (image->dy << 16) | (image->dx);
		var.reserved[2] = ((image->dy + image->height) << 16) |
		    (image->dx + image->width);

		msm_fb_pan_display(&var, info);
	}
}

static int msm_fb_blank(int blank_mode, struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	msm_fb_pan_idle(mfd);
	return msm_fb_blank_sub(blank_mode, info, mfd->op_enable);
}

static int msm_fb_set_lut(struct fb_cmap *cmap, struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (!mfd->lut_update)
		return -ENODEV;

	mfd->lut_update(info, cmap);
	return 0;
}

/*
 * Custom Framebuffer mmap() function for MSM driver.
 * Differs from standard mmap() function by allowing for customized
 * page-protection.
 */
static int msm_fb_mmap(struct fb_info *info, struct vm_area_struct * vma)
{
	/* Get frame buffer memory range. */
	unsigned long start = info->fix.smem_start;
	u32 len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (!start)
		return -EINVAL;

	if ((vma->vm_end <= vma->vm_start) ||
	    (off >= len) ||
	    ((vma->vm_end - vma->vm_start) > (len - off)))
		return -EINVAL;

	msm_fb_pan_idle(mfd);
	/* Set VM flags. */
	start &= PAGE_MASK;
	off += start;
	if (off < start)
		return -EINVAL;

	vma->vm_pgoff = off >> PAGE_SHIFT;
	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO | VM_RESERVED;

	/* Set VM page protection */
	if (mfd->mdp_fb_page_protection == MDP_FB_PAGE_PROTECTION_WRITECOMBINE)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	else if (mfd->mdp_fb_page_protection ==
			MDP_FB_PAGE_PROTECTION_WRITETHROUGHCACHE)
		vma->vm_page_prot = pgprot_writethroughcache(vma->vm_page_prot);
	else if (mfd->mdp_fb_page_protection ==
			MDP_FB_PAGE_PROTECTION_WRITEBACKCACHE)
		vma->vm_page_prot = pgprot_writebackcache(vma->vm_page_prot);
	else if (mfd->mdp_fb_page_protection ==
			MDP_FB_PAGE_PROTECTION_WRITEBACKWACACHE)
		vma->vm_page_prot = pgprot_writebackwacache(vma->vm_page_prot);
	else
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	/* Remap the frame buffer I/O range */
	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static struct fb_ops msm_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = msm_fb_open,
	.fb_release = msm_fb_release,
	.fb_read = NULL,
	.fb_write = NULL,
	.fb_cursor = NULL,
	.fb_check_var = msm_fb_check_var,	/* vinfo check */
	.fb_set_par = msm_fb_set_par,	/* set the video mode according to info->var */
	.fb_setcolreg = NULL,	/* set color register */
	.fb_blank = msm_fb_blank,	/* blank display */
	.fb_pan_display = msm_fb_pan_display,	/* pan display */
	.fb_fillrect = msm_fb_fillrect,	/* Draws a rectangle */
	.fb_copyarea = msm_fb_copyarea,	/* Copy data from area to another */
	.fb_imageblit = msm_fb_imageblit,	/* Draws a image to the display */
	.fb_rotate = NULL,
	.fb_sync = NULL,	/* wait for blit idle, optional */
	.fb_ioctl = msm_fb_ioctl,	/* perform fb specific ioctl (optional) */
	.fb_mmap = msm_fb_mmap,
};

static __u32 msm_fb_line_length(__u32 fb_index, __u32 xres, int bpp)
{
	/* The adreno GPU hardware requires that the pitch be aligned to
	   32 pixels for color buffers, so for the cases where the GPU
	   is writing directly to fb0, the framebuffer pitch
	   also needs to be 32 pixel aligned */

	if (fb_index == 0)
		return ALIGN(xres, 32) * bpp;
	else
		return xres * bpp;
}

static int msm_fb_register(struct msm_fb_data_type *mfd)
{
	int ret = -ENODEV;
	int bpp;
	struct msm_panel_info *panel_info = &mfd->panel_info;
	struct fb_info *fbi = mfd->fbi;
	struct fb_fix_screeninfo *fix;
	struct fb_var_screeninfo *var;
	int *id;
	int fbram_offset;
	int remainder, remainder_mode2;

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
	mfd->op_enable = FALSE;

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
		var->blue.offset = 0;
		var->green.offset = 8;
		var->red.offset = 16;
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

	case MDP_RGBA_8888:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;
		var->blue.offset = 8;
		var->green.offset = 16;
		var->red.offset = 24;
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

	case MDP_BGRA_8888:
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
		var->transp.offset = 24;
		var->transp.length = 8;
		bpp = 4;
		break;


	case MDP_YCRYCB_H2V1:
		/* ToDo: need to check TV-Out YUV422i framebuffer format */
		/*       we might need to create new type define */
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
		MSM_FB_ERR("msm_fb_init: fb %d unkown image type!\n",
			   mfd->index);
		return ret;
	}

	fix->type = panel_info->is_3d_panel;

	fix->line_length = msm_fb_line_length(mfd->index, panel_info->xres,
					      bpp);

	/* Make sure all buffers can be addressed on a page boundary by an x
	 * and y offset */

	remainder = (fix->line_length * panel_info->yres) & (PAGE_SIZE - 1);
					/* PAGE_SIZE is a power of 2 */
	if (!remainder)
		remainder = PAGE_SIZE;
	remainder_mode2 = (fix->line_length *
				panel_info->mode2_yres) & (PAGE_SIZE - 1);
	if (!remainder_mode2)
		remainder_mode2 = PAGE_SIZE;

	/*
	 * calculate smem_len based on max size of two supplied modes.
	 * Only fb0 has mem. fb1 and fb2 don't have mem.
	 */
	if (!bf_supported || mfd->index == 0)
		fix->smem_len = MAX((msm_fb_line_length(mfd->index,
							panel_info->xres,
							bpp) *
				     panel_info->yres + PAGE_SIZE -
				     remainder) * mfd->fb_page,
				    (msm_fb_line_length(mfd->index,
							panel_info->mode2_xres,
							bpp) *
				     panel_info->mode2_yres + PAGE_SIZE -
				     remainder_mode2) * mfd->fb_page);
	else if (mfd->index == 1 || mfd->index == 2) {
		pr_debug("%s:%d no memory is allocated for fb%d!\n",
			__func__, __LINE__, mfd->index);
		fix->smem_len = 0;
	}

	mfd->var_xres = panel_info->xres;
	mfd->var_yres = panel_info->yres;
	mfd->var_frame_rate = panel_info->frame_rate;

	var->pixclock = mfd->panel_info.clk_rate;
	mfd->var_pixclock = var->pixclock;

	var->xres = panel_info->xres;
	var->yres = panel_info->yres;
	var->xres_virtual = panel_info->xres;
	var->yres_virtual = panel_info->yres * mfd->fb_page +
		((PAGE_SIZE - remainder)/fix->line_length) * mfd->fb_page;
	var->bits_per_pixel = bpp * 8;	/* FrameBuffer color depth */

		/*
		 * id field for fb app
		 */
	id = (int *)&mfd->panel;

	switch (mdp_rev) {
	case MDP_REV_20:
		snprintf(fix->id, sizeof(fix->id), "msmfb20_%x", (__u32) *id);
		break;
	case MDP_REV_22:
		snprintf(fix->id, sizeof(fix->id), "msmfb22_%x", (__u32) *id);
		break;
	case MDP_REV_30:
		snprintf(fix->id, sizeof(fix->id), "msmfb30_%x", (__u32) *id);
		break;
	case MDP_REV_303:
		snprintf(fix->id, sizeof(fix->id), "msmfb303_%x", (__u32) *id);
		break;
	case MDP_REV_31:
		snprintf(fix->id, sizeof(fix->id), "msmfb31_%x", (__u32) *id);
		break;
	case MDP_REV_40:
		snprintf(fix->id, sizeof(fix->id), "msmfb40_%x", (__u32) *id);
		break;
	case MDP_REV_41:
		snprintf(fix->id, sizeof(fix->id), "msmfb41_%x", (__u32) *id);
		break;
	case MDP_REV_42:
		snprintf(fix->id, sizeof(fix->id), "msmfb42_%x", (__u32) *id);
		break;
	case MDP_REV_43:
		snprintf(fix->id, sizeof(fix->id), "msmfb43_%x", (__u32) *id);
		break;
	case MDP_REV_44:
		snprintf(fix->id, sizeof(fix->id), "msmfb44_%x", (__u32) *id);
		break;
	default:
		snprintf(fix->id, sizeof(fix->id), "msmfb0_%x", (__u32) *id);
		break;
	}

	fbi->fbops = &msm_fb_ops;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->pseudo_palette = msm_fb_pseudo_palette;

	mfd->ref_cnt = 0;
	mfd->sw_currently_refreshing = FALSE;
	mfd->sw_refreshing_enable = TRUE;
	mfd->panel_power_on = FALSE;

	mfd->pan_waiting = FALSE;
	init_completion(&mfd->pan_comp);
	init_completion(&mfd->refresher_comp);
	sema_init(&mfd->sem, 1);

	init_timer(&mfd->msmfb_no_update_notify_timer);
	mfd->msmfb_no_update_notify_timer.function =
			msmfb_no_update_notify_timer_cb;
	mfd->msmfb_no_update_notify_timer.data = (unsigned long)mfd;
	init_completion(&mfd->msmfb_update_notify);
	init_completion(&mfd->msmfb_no_update_notify);
	init_completion(&mfd->commit_comp);
	mutex_init(&mfd->sync_mutex);
	INIT_WORK(&mfd->commit_work, msm_fb_commit_wq_handler);
	mfd->msm_fb_backup = kzalloc(sizeof(struct msm_fb_backup_type),
		GFP_KERNEL);
	if (mfd->msm_fb_backup == 0) {
		pr_err("error: not enough memory!\n");
		return -ENOMEM;
	}
	fbram_offset = PAGE_ALIGN((int)fbram)-(int)fbram;
	fbram += fbram_offset;
	fbram_phys += fbram_offset;
	fbram_size -= fbram_offset;

	if (!bf_supported || mfd->index == 0)
		if (fbram_size < fix->smem_len) {
			pr_err("error: no more framebuffer memory!\n");
			return -ENOMEM;
		}

	fbi->screen_base = fbram;
	fbi->fix.smem_start = (unsigned long)fbram_phys;

	msm_iommu_map_contig_buffer(fbi->fix.smem_start,
					DISPLAY_WRITE_DOMAIN,
					GEN_POOL,
					fbi->fix.smem_len,
					SZ_4K,
					0,
					&(mfd->display_iova));

	msm_iommu_map_contig_buffer(fbi->fix.smem_start,
					DISPLAY_READ_DOMAIN,
					GEN_POOL,
					fbi->fix.smem_len,
					SZ_4K,
					0,
					&(mfd->display_iova));

	msm_iommu_map_contig_buffer(fbi->fix.smem_start,
					ROTATOR_SRC_DOMAIN,
					GEN_POOL,
					fbi->fix.smem_len,
					SZ_4K,
					0,
					&(mfd->rotator_iova));

	if (!bf_supported || mfd->index == 0)
		memset(fbi->screen_base, 0x0, fix->smem_len);

	mfd->op_enable = TRUE;
	mfd->panel_power_on = FALSE;

	/* cursor memory allocation */
	if (mfd->cursor_update) {
		unsigned long cursor_buf_iommu = 0;
		mfd->cursor_buf = dma_alloc_coherent(NULL,
					MDP_CURSOR_SIZE,
					(dma_addr_t *) &mfd->cursor_buf_phys,
					GFP_KERNEL);

		msm_iommu_map_contig_buffer((unsigned long)mfd->cursor_buf_phys,
					    DISPLAY_READ_DOMAIN,
					    GEN_POOL,
					    MDP_CURSOR_SIZE,
					    SZ_4K,
					    0,
					    (dma_addr_t *)&cursor_buf_iommu);
		if (cursor_buf_iommu)
			mfd->cursor_buf_phys = (void *)cursor_buf_iommu;

		if (!mfd->cursor_buf)
			mfd->cursor_update = 0;
	}

	if (mfd->lut_update) {
		ret = fb_alloc_cmap(&fbi->cmap, 256, 0);
		if (ret)
			printk(KERN_ERR "%s: fb_alloc_cmap() failed!\n",
					__func__);
	}

	if (register_framebuffer(fbi) < 0) {
		if (mfd->lut_update)
			fb_dealloc_cmap(&fbi->cmap);

		if (mfd->cursor_buf)
			dma_free_coherent(NULL,
				MDP_CURSOR_SIZE,
				mfd->cursor_buf,
				(dma_addr_t) mfd->cursor_buf_phys);

		mfd->op_enable = FALSE;
		return -EPERM;
	}

	fbram += fix->smem_len;
	fbram_phys += fix->smem_len;
	fbram_size -= fix->smem_len;

	MSM_FB_INFO
	    ("FrameBuffer[%d] %dx%d size=%d bytes is registered successfully!\n",
	     mfd->index, fbi->var.xres, fbi->var.yres, fbi->fix.smem_len);

	ret = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (mfd->panel_info.type != DTV_PANEL) {
		mfd->early_suspend.suspend = msmfb_early_suspend;
		mfd->early_suspend.resume = msmfb_early_resume;
		mfd->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 2;
		register_early_suspend(&mfd->early_suspend);
	}
#endif

#ifdef MSM_FB_ENABLE_DBGFS
	{
		struct dentry *root;
		struct dentry *sub_dir;
		char sub_name[2];

		root = msm_fb_get_debugfs_root();
		if (root != NULL) {
			sub_name[0] = (char)(mfd->index + 0x30);
			sub_name[1] = '\0';
			sub_dir = debugfs_create_dir(sub_name, root);
		} else {
			sub_dir = NULL;
		}

		mfd->sub_dir = sub_dir;

		if (sub_dir) {
			msm_fb_debugfs_file_create(sub_dir, "op_enable",
						   (u32 *) &mfd->op_enable);
			msm_fb_debugfs_file_create(sub_dir, "panel_power_on",
						   (u32 *) &mfd->
						   panel_power_on);
			msm_fb_debugfs_file_create(sub_dir, "ref_cnt",
						   (u32 *) &mfd->ref_cnt);
			msm_fb_debugfs_file_create(sub_dir, "fb_imgType",
						   (u32 *) &mfd->fb_imgType);
			msm_fb_debugfs_file_create(sub_dir,
						   "sw_currently_refreshing",
						   (u32 *) &mfd->
						   sw_currently_refreshing);
			msm_fb_debugfs_file_create(sub_dir,
						   "sw_refreshing_enable",
						   (u32 *) &mfd->
						   sw_refreshing_enable);

			msm_fb_debugfs_file_create(sub_dir, "xres",
						   (u32 *) &mfd->panel_info.
						   xres);
			msm_fb_debugfs_file_create(sub_dir, "yres",
						   (u32 *) &mfd->panel_info.
						   yres);
			msm_fb_debugfs_file_create(sub_dir, "bpp",
						   (u32 *) &mfd->panel_info.
						   bpp);
			msm_fb_debugfs_file_create(sub_dir, "type",
						   (u32 *) &mfd->panel_info.
						   type);
			msm_fb_debugfs_file_create(sub_dir, "wait_cycle",
						   (u32 *) &mfd->panel_info.
						   wait_cycle);
			msm_fb_debugfs_file_create(sub_dir, "pdest",
						   (u32 *) &mfd->panel_info.
						   pdest);
			msm_fb_debugfs_file_create(sub_dir, "backbuff",
						   (u32 *) &mfd->panel_info.
						   fb_num);
			msm_fb_debugfs_file_create(sub_dir, "clk_rate",
						   (u32 *) &mfd->panel_info.
						   clk_rate);
			msm_fb_debugfs_file_create(sub_dir, "frame_count",
						   (u32 *) &mfd->panel_info.
						   frame_count);


			switch (mfd->dest) {
			case DISPLAY_LCD:
				msm_fb_debugfs_file_create(sub_dir,
				"vsync_enable",
				(u32 *)&mfd->panel_info.lcd.vsync_enable);
				msm_fb_debugfs_file_create(sub_dir,
				"refx100",
				(u32 *) &mfd->panel_info.lcd. refx100);
				msm_fb_debugfs_file_create(sub_dir,
				"v_back_porch",
				(u32 *) &mfd->panel_info.lcd.v_back_porch);
				msm_fb_debugfs_file_create(sub_dir,
				"v_front_porch",
				(u32 *) &mfd->panel_info.lcd.v_front_porch);
				msm_fb_debugfs_file_create(sub_dir,
				"v_pulse_width",
				(u32 *) &mfd->panel_info.lcd.v_pulse_width);
				msm_fb_debugfs_file_create(sub_dir,
				"hw_vsync_mode",
				(u32 *) &mfd->panel_info.lcd.hw_vsync_mode);
				msm_fb_debugfs_file_create(sub_dir,
				"vsync_notifier_period", (u32 *)
				&mfd->panel_info.lcd.vsync_notifier_period);
				break;

			case DISPLAY_LCDC:
				msm_fb_debugfs_file_create(sub_dir,
				"h_back_porch",
				(u32 *) &mfd->panel_info.lcdc.h_back_porch);
				msm_fb_debugfs_file_create(sub_dir,
				"h_front_porch",
				(u32 *) &mfd->panel_info.lcdc.h_front_porch);
				msm_fb_debugfs_file_create(sub_dir,
				"h_pulse_width",
				(u32 *) &mfd->panel_info.lcdc.h_pulse_width);
				msm_fb_debugfs_file_create(sub_dir,
				"v_back_porch",
				(u32 *) &mfd->panel_info.lcdc.v_back_porch);
				msm_fb_debugfs_file_create(sub_dir,
				"v_front_porch",
				(u32 *) &mfd->panel_info.lcdc.v_front_porch);
				msm_fb_debugfs_file_create(sub_dir,
				"v_pulse_width",
				(u32 *) &mfd->panel_info.lcdc.v_pulse_width);
				msm_fb_debugfs_file_create(sub_dir,
				"border_clr",
				(u32 *) &mfd->panel_info.lcdc.border_clr);
				msm_fb_debugfs_file_create(sub_dir,
				"underflow_clr",
				(u32 *) &mfd->panel_info.lcdc.underflow_clr);
				msm_fb_debugfs_file_create(sub_dir,
				"hsync_skew",
				(u32 *) &mfd->panel_info.lcdc.hsync_skew);
				break;

			default:
				break;
			}
		}
	}
#endif /* MSM_FB_ENABLE_DBGFS */

	return ret;
}

static int msm_fb_open(struct fb_info *info, int user)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	bool unblank = true;
	int result;

	result = pm_runtime_get_sync(info->dev);

	if (result < 0) {
		printk(KERN_ERR "pm_runtime: fail to wake up\n");
	}

	if (info->node == 0 && !(mfd->cont_splash_done)) {	/* primary */
			mfd->ref_cnt++;
			return 0;
	}

	if (!mfd->ref_cnt) {
		if (!bf_supported ||
			(info->node != 1 && info->node != 2))
			mdp_set_dma_pan_info(info, NULL, TRUE);
		else
			pr_debug("%s:%d no mdp_set_dma_pan_info %d\n",
				__func__, __LINE__, info->node);

		if (mfd->is_panel_ready && !mfd->is_panel_ready())
			unblank = false;

		if (unblank) {
			if (msm_fb_blank_sub(FB_BLANK_UNBLANK,
				info, mfd->op_enable)) {
				MSM_FB_ERR("%s: can't turn on display!\n",
					__func__);
				return -EPERM;
			}
		}
	}

	mfd->ref_cnt++;
	return 0;
}

static int msm_fb_release(struct fb_info *info, int user)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int ret = 0;

	if (!mfd->ref_cnt) {
		MSM_FB_INFO("msm_fb_release: try to close unopened fb %d!\n",
			    mfd->index);
		return -EINVAL;
	}

	mfd->ref_cnt--;

	if (!mfd->ref_cnt) {
		if ((ret =
		     msm_fb_blank_sub(FB_BLANK_POWERDOWN, info,
				      mfd->op_enable)) != 0) {
			printk(KERN_ERR "msm_fb_release: can't turn off display!\n");
			return ret;
		}
	}

	pm_runtime_put(info->dev);
	return ret;
}

int msm_fb_wait_for_fence(struct msm_fb_data_type *mfd)
{
	int i, ret = 0;
	/* buf sync */
	for (i = 0; i < mfd->acq_fen_cnt; i++) {
		ret = sync_fence_wait(mfd->acq_fen[i], WAIT_FENCE_TIMEOUT);
		sync_fence_put(mfd->acq_fen[i]);
		if (ret < 0) {
			pr_err("%s: sync_fence_wait failed! ret = %x\n",
				__func__, ret);
			break;
		}
	}
	mfd->acq_fen_cnt = 0;
	return ret;
}
int msm_fb_signal_timeline(struct msm_fb_data_type *mfd)
{
	mutex_lock(&mfd->sync_mutex);
	if (mfd->timeline) {
		sw_sync_timeline_inc(mfd->timeline, 1);
		mfd->timeline_value++;
	}
	mfd->last_rel_fence = mfd->cur_rel_fence;
	mfd->cur_rel_fence = 0;
	mutex_unlock(&mfd->sync_mutex);
	return 0;
}

static void bl_workqueue_handler(struct work_struct *work)
{
	struct msm_fb_data_type *mfd = container_of(to_delayed_work(work),
				struct msm_fb_data_type, backlight_worker);
	struct msm_fb_panel_data *pdata = mfd->pdev->dev.platform_data;

	if ((pdata) && (pdata->set_backlight) && (!bl_updated)) {
		down(&mfd->sem);
		mfd->bl_level = unset_bl_level;
		pdata->set_backlight(mfd);
		bl_level_old = unset_bl_level;
		bl_updated = 1;
		up(&mfd->sem);
	}
}

DEFINE_SEMAPHORE(msm_fb_pan_sem);
static int msm_fb_pan_idle(struct msm_fb_data_type *mfd)
{
	int ret = 0;

	mutex_lock(&mfd->sync_mutex);
	if (mfd->is_committing) {
		mutex_unlock(&mfd->sync_mutex);
		ret = wait_for_completion_interruptible_timeout(
				&mfd->commit_comp,
			msecs_to_jiffies(WAIT_FENCE_TIMEOUT));
		if (ret <= 0)
			ret = -ERESTARTSYS;
		else if (!ret)
			pr_err("%s wait for commit_comp timeout %d %d",
				__func__, ret, mfd->is_committing);
	} else {
		mutex_unlock(&mfd->sync_mutex);
	}
	return ret;
}
static int msm_fb_pan_display_ex(struct fb_var_screeninfo *var,
			      struct fb_info *info, u32 wait_for_finish)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct msm_fb_backup_type *fb_backup;
	int ret = 0;
	/*
	 * If framebuffer is 2, io pen display is not allowed.
	 */
	if (bf_supported && info->node == 2) {
		pr_err("%s: no pan display for fb%d!",
		       __func__, info->node);
		return -EPERM;
	}

	if (info->node != 0 || mfd->cont_splash_done)	/* primary */
		if ((!mfd->op_enable) || (!mfd->panel_power_on))
			return -EPERM;

	if (var->xoffset > (info->var.xres_virtual - info->var.xres))
		return -EINVAL;

	if (var->yoffset > (info->var.yres_virtual - info->var.yres))
		return -EINVAL;
	msm_fb_pan_idle(mfd);

	mutex_lock(&mfd->sync_mutex);

	if (info->fix.xpanstep)
		info->var.xoffset =
		    (var->xoffset / info->fix.xpanstep) * info->fix.xpanstep;

	if (info->fix.ypanstep)
		info->var.yoffset =
		    (var->yoffset / info->fix.ypanstep) * info->fix.ypanstep;

	fb_backup = (struct msm_fb_backup_type *)mfd->msm_fb_backup;
	memcpy(&fb_backup->info, info, sizeof(struct fb_info));
	memcpy(&fb_backup->var, var, sizeof(struct fb_var_screeninfo));
	mfd->is_committing = 1;
	INIT_COMPLETION(mfd->commit_comp);
	schedule_work(&mfd->commit_work);
	mutex_unlock(&mfd->sync_mutex);
	if (wait_for_finish)
		msm_fb_pan_idle(mfd);
	return ret;
}

static int msm_fb_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	return msm_fb_pan_display_ex(var, info, TRUE);
}

static int msm_fb_pan_display_sub(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct mdp_dirty_region dirty;
	struct mdp_dirty_region *dirtyPtr = NULL;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	/*
	 * If framebuffer is 2, io pen display is not allowed.
	 */
	if (bf_supported && info->node == 2) {
		pr_err("%s: no pan display for fb%d!",
		       __func__, info->node);
		return -EPERM;
	}

	if (info->node != 0 || mfd->cont_splash_done)	/* primary */
		if ((!mfd->op_enable) || (!mfd->panel_power_on))
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

	/* "UPDT" */
	if (var->reserved[0] == 0x54445055) {

		dirty.xoffset = var->reserved[1] & 0xffff;
		dirty.yoffset = (var->reserved[1] >> 16) & 0xffff;

		if ((var->reserved[2] & 0xffff) <= dirty.xoffset)
			return -EINVAL;
		if (((var->reserved[2] >> 16) & 0xffff) <= dirty.yoffset)
			return -EINVAL;

		dirty.width = (var->reserved[2] & 0xffff) - dirty.xoffset;
		dirty.height =
		    ((var->reserved[2] >> 16) & 0xffff) - dirty.yoffset;
		info->var.yoffset = var->yoffset;

		if (dirty.xoffset < 0)
			return -EINVAL;

		if (dirty.yoffset < 0)
			return -EINVAL;

		if ((dirty.xoffset + dirty.width) > info->var.xres)
			return -EINVAL;

		if ((dirty.yoffset + dirty.height) > info->var.yres)
			return -EINVAL;

		if ((dirty.width <= 0) || (dirty.height <= 0))
			return -EINVAL;

		dirtyPtr = &dirty;
	}
	complete(&mfd->msmfb_update_notify);
	mutex_lock(&msm_fb_notify_update_sem);
	if (mfd->msmfb_no_update_notify_timer.function)
		del_timer(&mfd->msmfb_no_update_notify_timer);

	mfd->msmfb_no_update_notify_timer.expires = jiffies + (2 * HZ);
	add_timer(&mfd->msmfb_no_update_notify_timer);
	mutex_unlock(&msm_fb_notify_update_sem);

	down(&msm_fb_pan_sem);
	msm_fb_wait_for_fence(mfd);
	if (info->node == 0 && !(mfd->cont_splash_done)) { /* primary */
		mdp_set_dma_pan_info(info, NULL, TRUE);
		if (msm_fb_blank_sub(FB_BLANK_UNBLANK, info, mfd->op_enable)) {
			pr_err("%s: can't turn on display!\n", __func__);
			if (mfd->timeline) {
				sw_sync_timeline_inc(mfd->timeline, 2);
				mfd->timeline_value += 2;
			}
			return -EINVAL;
		}
	}

	mdp_set_dma_pan_info(info, dirtyPtr,
			     (var->activate & FB_ACTIVATE_VBL));
	/* async call */
	mdp_dma_pan_update(info);
	msm_fb_signal_timeline(mfd);
	up(&msm_fb_pan_sem);

	if (unset_bl_level && !bl_updated)
		schedule_delayed_work(&mfd->backlight_worker,
				backlight_duration);

	if (info->node == 0 && (mfd->cont_splash_done)) /* primary */
		mdp_free_splash_buffer(mfd);

	++mfd->panel_info.frame_count;
	return 0;
}

static void msm_fb_commit_wq_handler(struct work_struct *work)
{
	struct msm_fb_data_type *mfd;
	struct fb_var_screeninfo *var;
	struct fb_info *info;
	struct msm_fb_backup_type *fb_backup;

	mfd = container_of(work, struct msm_fb_data_type, commit_work);
	fb_backup = (struct msm_fb_backup_type *)mfd->msm_fb_backup;
	var = &fb_backup->var;
	info = &fb_backup->info;
	msm_fb_pan_display_sub(var, info);
	mutex_lock(&mfd->sync_mutex);
	mfd->is_committing = 0;
	complete_all(&mfd->commit_comp);
	mutex_unlock(&mfd->sync_mutex);

}

static int msm_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	msm_fb_pan_idle(mfd);
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
		/* Figure out if the user meant RGBA or ARGB
		   and verify the position of the RGB components */

		if (var->transp.offset == 24) {
			if ((var->blue.offset != 0) ||
			    (var->green.offset != 8) ||
			    (var->red.offset != 16))
				return -EINVAL;
		} else if (var->transp.offset == 0) {
			if ((var->blue.offset != 8) ||
			    (var->green.offset != 16) ||
			    (var->red.offset != 24))
				return -EINVAL;
		} else
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

	if (!bf_supported ||
		(info->node != 1 && info->node != 2))
		if (info->fix.smem_len <
		    (var->xres_virtual*
		     var->yres_virtual*
		     (var->bits_per_pixel/8)))
			return -EINVAL;

	if ((var->xres == 0) || (var->yres == 0))
		return -EINVAL;

	if (var->xoffset > (var->xres_virtual - var->xres))
		return -EINVAL;

	if (var->yoffset > (var->yres_virtual - var->yres))
		return -EINVAL;

	return 0;
}

int msm_fb_check_frame_rate(struct msm_fb_data_type *mfd
						, struct fb_info *info)
{
	int panel_height, panel_width, var_frame_rate, fps_mod;
	struct fb_var_screeninfo *var = &info->var;
	fps_mod = 0;
	if ((mfd->panel_info.type == DTV_PANEL) ||
		(mfd->panel_info.type == HDMI_PANEL)) {
		panel_height = var->yres + var->upper_margin +
			var->vsync_len + var->lower_margin;
		panel_width = var->xres + var->right_margin +
			var->hsync_len + var->left_margin;
		var_frame_rate = ((var->pixclock)/(panel_height * panel_width));
		if (mfd->var_frame_rate != var_frame_rate) {
			fps_mod = 1;
			mfd->var_frame_rate = var_frame_rate;
		}
	}
	return fps_mod;
}

static int msm_fb_set_par(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_var_screeninfo *var = &info->var;
	int old_imgType;
	int blank = 0;
	msm_fb_pan_idle(mfd);
	old_imgType = mfd->fb_imgType;
	switch (var->bits_per_pixel) {
	case 16:
		if (var->red.offset == 0)
			mfd->fb_imgType = MDP_BGR_565;
		else
			mfd->fb_imgType = MDP_RGB_565;
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
		if ((var->transp.offset == 24) && (var->blue.offset == 0))
			mfd->fb_imgType = MDP_BGRA_8888;
		else if (var->transp.offset == 24)
			mfd->fb_imgType = MDP_ARGB_8888;
		else
			mfd->fb_imgType = MDP_RGBA_8888;
		break;

	default:
		return -EINVAL;
	}

	if ((mfd->var_pixclock != var->pixclock) ||
		(mfd->hw_refresh && ((mfd->fb_imgType != old_imgType) ||
				(mfd->var_pixclock != var->pixclock) ||
				(mfd->var_xres != var->xres) ||
				(mfd->var_yres != var->yres) ||
				(msm_fb_check_frame_rate(mfd, info))))) {
		mfd->var_xres = var->xres;
		mfd->var_yres = var->yres;
		mfd->var_pixclock = var->pixclock;
		blank = 1;
	}
	mfd->fbi->fix.line_length = msm_fb_line_length(mfd->index, var->xres,
						       var->bits_per_pixel/8);

	if (blank) {
		msm_fb_blank_sub(FB_BLANK_POWERDOWN, info, mfd->op_enable);

		if (mfd->update_panel_info)
			mfd->update_panel_info(mfd);

		msm_fb_blank_sub(FB_BLANK_UNBLANK, info, mfd->op_enable);
	}

	return 0;
}

static int msm_fb_stop_sw_refresher(struct msm_fb_data_type *mfd)
{
	if (mfd->hw_refresh)
		return -EPERM;

	if (mfd->sw_currently_refreshing) {
		down(&mfd->sem);
		mfd->sw_currently_refreshing = FALSE;
		up(&mfd->sem);

		/* wait until the refresher finishes the last job */
		wait_for_completion_killable(&mfd->refresher_comp);
	}

	return 0;
}

int msm_fb_resume_sw_refresher(struct msm_fb_data_type *mfd)
{
	boolean do_refresh;

	if (mfd->hw_refresh)
		return -EPERM;

	down(&mfd->sem);
	if ((!mfd->sw_currently_refreshing) && (mfd->sw_refreshing_enable)) {
		do_refresh = TRUE;
		mfd->sw_currently_refreshing = TRUE;
	} else {
		do_refresh = FALSE;
	}
	up(&mfd->sem);

	if (do_refresh)
		mdp_refresh_screen((unsigned long)mfd);

	return 0;
}

#if defined CONFIG_FB_MSM_MDP31
static int mdp_blit_split_height(struct fb_info *info,
				struct mdp_blit_req *req)
{
	int ret;
	struct mdp_blit_req splitreq;
	int s_x_0, s_x_1, s_w_0, s_w_1, s_y_0, s_y_1, s_h_0, s_h_1;
	int d_x_0, d_x_1, d_w_0, d_w_1, d_y_0, d_y_1, d_h_0, d_h_1;

	splitreq = *req;
	/* break dest roi at height*/
	d_x_0 = d_x_1 = req->dst_rect.x;
	d_w_0 = d_w_1 = req->dst_rect.w;
	d_y_0 = req->dst_rect.y;
	if (req->dst_rect.h % 32 == 3)
		d_h_1 = (req->dst_rect.h - 3) / 2 - 1;
	else if (req->dst_rect.h % 32 == 2)
		d_h_1 = (req->dst_rect.h - 2) / 2 - 6;
	else
		d_h_1 = (req->dst_rect.h - 1) / 2 - 1;
	d_h_0 = req->dst_rect.h - d_h_1;
	d_y_1 = d_y_0 + d_h_0;
	if (req->dst_rect.h == 3) {
		d_h_1 = 2;
		d_h_0 = 2;
		d_y_1 = d_y_0 + 1;
	}

	/* blit first region */
	if (((splitreq.flags & 0x07) == 0x04) ||
		((splitreq.flags & 0x07) == 0x0)) {

		if (splitreq.flags & MDP_ROT_90) {
			s_y_0 = s_y_1 = req->src_rect.y;
			s_h_0 = s_h_1 = req->src_rect.h;
			s_x_0 = req->src_rect.x;
			s_w_1 = (req->src_rect.w * d_h_1) / req->dst_rect.h;
			s_w_0 = req->src_rect.w - s_w_1;
			s_x_1 = s_x_0 + s_w_0;
			if (d_h_1 >= 8 * s_w_1) {
				s_w_1++;
				s_x_1--;
			}
		} else {
			s_x_0 = s_x_1 = req->src_rect.x;
			s_w_0 = s_w_1 = req->src_rect.w;
			s_y_0 = req->src_rect.y;
			s_h_1 = (req->src_rect.h * d_h_1) / req->dst_rect.h;
			s_h_0 = req->src_rect.h - s_h_1;
			s_y_1 = s_y_0 + s_h_0;
			if (d_h_1 >= 8 * s_h_1) {
				s_h_1++;
				s_y_1--;
			}
		}

		splitreq.src_rect.h = s_h_0;
		splitreq.src_rect.y = s_y_0;
		splitreq.dst_rect.h = d_h_0;
		splitreq.dst_rect.y = d_y_0;
		splitreq.src_rect.x = s_x_0;
		splitreq.src_rect.w = s_w_0;
		splitreq.dst_rect.x = d_x_0;
		splitreq.dst_rect.w = d_w_0;
	} else {

		if (splitreq.flags & MDP_ROT_90) {
			s_y_0 = s_y_1 = req->src_rect.y;
			s_h_0 = s_h_1 = req->src_rect.h;
			s_x_0 = req->src_rect.x;
			s_w_1 = (req->src_rect.w * d_h_0) / req->dst_rect.h;
			s_w_0 = req->src_rect.w - s_w_1;
			s_x_1 = s_x_0 + s_w_0;
			if (d_h_0 >= 8 * s_w_1) {
				s_w_1++;
				s_x_1--;
			}
		} else {
			s_x_0 = s_x_1 = req->src_rect.x;
			s_w_0 = s_w_1 = req->src_rect.w;
			s_y_0 = req->src_rect.y;
			s_h_1 = (req->src_rect.h * d_h_0) / req->dst_rect.h;
			s_h_0 = req->src_rect.h - s_h_1;
			s_y_1 = s_y_0 + s_h_0;
			if (d_h_0 >= 8 * s_h_1) {
				s_h_1++;
				s_y_1--;
			}
		}
		splitreq.src_rect.h = s_h_0;
		splitreq.src_rect.y = s_y_0;
		splitreq.dst_rect.h = d_h_1;
		splitreq.dst_rect.y = d_y_1;
		splitreq.src_rect.x = s_x_0;
		splitreq.src_rect.w = s_w_0;
		splitreq.dst_rect.x = d_x_1;
		splitreq.dst_rect.w = d_w_1;
	}
	ret = mdp_ppp_blit(info, &splitreq);
	if (ret)
		return ret;

	/* blit second region */
	if (((splitreq.flags & 0x07) == 0x04) ||
		((splitreq.flags & 0x07) == 0x0)) {
		splitreq.src_rect.h = s_h_1;
		splitreq.src_rect.y = s_y_1;
		splitreq.dst_rect.h = d_h_1;
		splitreq.dst_rect.y = d_y_1;
		splitreq.src_rect.x = s_x_1;
		splitreq.src_rect.w = s_w_1;
		splitreq.dst_rect.x = d_x_1;
		splitreq.dst_rect.w = d_w_1;
	} else {
		splitreq.src_rect.h = s_h_1;
		splitreq.src_rect.y = s_y_1;
		splitreq.dst_rect.h = d_h_0;
		splitreq.dst_rect.y = d_y_0;
		splitreq.src_rect.x = s_x_1;
		splitreq.src_rect.w = s_w_1;
		splitreq.dst_rect.x = d_x_0;
		splitreq.dst_rect.w = d_w_0;
	}
	ret = mdp_ppp_blit(info, &splitreq);
	return ret;
}
#endif

int mdp_blit(struct fb_info *info, struct mdp_blit_req *req)
{
	int ret;
#if defined CONFIG_FB_MSM_MDP31 || defined CONFIG_FB_MSM_MDP30
	unsigned int remainder = 0, is_bpp_4 = 0;
	struct mdp_blit_req splitreq;
	int s_x_0, s_x_1, s_w_0, s_w_1, s_y_0, s_y_1, s_h_0, s_h_1;
	int d_x_0, d_x_1, d_w_0, d_w_1, d_y_0, d_y_1, d_h_0, d_h_1;

	if (req->flags & MDP_ROT_90) {
		if (((req->dst_rect.h == 1) && ((req->src_rect.w != 1) ||
			(req->dst_rect.w != req->src_rect.h))) ||
			((req->dst_rect.w == 1) && ((req->src_rect.h != 1) ||
			(req->dst_rect.h != req->src_rect.w)))) {
			printk(KERN_ERR "mpd_ppp: error scaling when size is 1!\n");
			return -EINVAL;
		}
	} else {
		if (((req->dst_rect.w == 1) && ((req->src_rect.w != 1) ||
			(req->dst_rect.h != req->src_rect.h))) ||
			((req->dst_rect.h == 1) && ((req->src_rect.h != 1) ||
			(req->dst_rect.w != req->src_rect.w)))) {
			printk(KERN_ERR "mpd_ppp: error scaling when size is 1!\n");
			return -EINVAL;
		}
	}
#endif
	if (unlikely(req->src_rect.h == 0 || req->src_rect.w == 0)) {
		printk(KERN_ERR "mpd_ppp: src img of zero size!\n");
		return -EINVAL;
	}
	if (unlikely(req->dst_rect.h == 0 || req->dst_rect.w == 0))
		return 0;

#if defined CONFIG_FB_MSM_MDP31
	/* MDP width split workaround */
	remainder = (req->dst_rect.w)%32;
	ret = mdp_get_bytes_per_pixel(req->dst.format,
					(struct msm_fb_data_type *)info->par);
	if (ret <= 0) {
		printk(KERN_ERR "mdp_ppp: incorrect bpp!\n");
		return -EINVAL;
	}
	is_bpp_4 = (ret == 4) ? 1 : 0;

	if ((is_bpp_4 && (remainder == 6 || remainder == 14 ||
	remainder == 22 || remainder == 30)) || remainder == 3 ||
	(remainder == 1 && req->dst_rect.w != 1) ||
	(remainder == 2 && req->dst_rect.w != 2)) {
		/* make new request as provide by user */
		splitreq = *req;

		/* break dest roi at width*/
		d_y_0 = d_y_1 = req->dst_rect.y;
		d_h_0 = d_h_1 = req->dst_rect.h;
		d_x_0 = req->dst_rect.x;

		if (remainder == 14)
			d_w_1 = (req->dst_rect.w - 14) / 2 + 4;
		else if (remainder == 22)
			d_w_1 = (req->dst_rect.w - 22) / 2 + 10;
		else if (remainder == 30)
			d_w_1 = (req->dst_rect.w - 30) / 2 + 10;
		else if (remainder == 6)
			d_w_1 = req->dst_rect.w / 2 - 1;
		else if (remainder == 3)
			d_w_1 = (req->dst_rect.w - 3) / 2 - 1;
		else if (remainder == 2)
			d_w_1 = (req->dst_rect.w - 2) / 2 - 6;
		else
			d_w_1 = (req->dst_rect.w - 1) / 2 - 1;
		d_w_0 = req->dst_rect.w - d_w_1;
		d_x_1 = d_x_0 + d_w_0;
		if (req->dst_rect.w == 3) {
			d_w_1 = 2;
			d_w_0 = 2;
			d_x_1 = d_x_0 + 1;
		}

		/* blit first region */
		if (((splitreq.flags & 0x07) == 0x07) ||
			((splitreq.flags & 0x07) == 0x0)) {

			if (splitreq.flags & MDP_ROT_90) {
				s_x_0 = s_x_1 = req->src_rect.x;
				s_w_0 = s_w_1 = req->src_rect.w;
				s_y_0 = req->src_rect.y;
				s_h_1 = (req->src_rect.h * d_w_1) /
					req->dst_rect.w;
				s_h_0 = req->src_rect.h - s_h_1;
				s_y_1 = s_y_0 + s_h_0;
				if (d_w_1 >= 8 * s_h_1) {
					s_h_1++;
					s_y_1--;
				}
			} else {
				s_y_0 = s_y_1 = req->src_rect.y;
				s_h_0 = s_h_1 = req->src_rect.h;
				s_x_0 = req->src_rect.x;
				s_w_1 = (req->src_rect.w * d_w_1) /
					req->dst_rect.w;
				s_w_0 = req->src_rect.w - s_w_1;
				s_x_1 = s_x_0 + s_w_0;
				if (d_w_1 >= 8 * s_w_1) {
					s_w_1++;
					s_x_1--;
				}
			}

			splitreq.src_rect.h = s_h_0;
			splitreq.src_rect.y = s_y_0;
			splitreq.dst_rect.h = d_h_0;
			splitreq.dst_rect.y = d_y_0;
			splitreq.src_rect.x = s_x_0;
			splitreq.src_rect.w = s_w_0;
			splitreq.dst_rect.x = d_x_0;
			splitreq.dst_rect.w = d_w_0;
		} else {
			if (splitreq.flags & MDP_ROT_90) {
				s_x_0 = s_x_1 = req->src_rect.x;
				s_w_0 = s_w_1 = req->src_rect.w;
				s_y_0 = req->src_rect.y;
				s_h_1 = (req->src_rect.h * d_w_0) /
					req->dst_rect.w;
				s_h_0 = req->src_rect.h - s_h_1;
				s_y_1 = s_y_0 + s_h_0;
				if (d_w_0 >= 8 * s_h_1) {
					s_h_1++;
					s_y_1--;
				}
			} else {
				s_y_0 = s_y_1 = req->src_rect.y;
				s_h_0 = s_h_1 = req->src_rect.h;
				s_x_0 = req->src_rect.x;
				s_w_1 = (req->src_rect.w * d_w_0) /
					req->dst_rect.w;
				s_w_0 = req->src_rect.w - s_w_1;
				s_x_1 = s_x_0 + s_w_0;
				if (d_w_0 >= 8 * s_w_1) {
					s_w_1++;
					s_x_1--;
				}
			}
			splitreq.src_rect.h = s_h_0;
			splitreq.src_rect.y = s_y_0;
			splitreq.dst_rect.h = d_h_1;
			splitreq.dst_rect.y = d_y_1;
			splitreq.src_rect.x = s_x_0;
			splitreq.src_rect.w = s_w_0;
			splitreq.dst_rect.x = d_x_1;
			splitreq.dst_rect.w = d_w_1;
		}

		if ((splitreq.dst_rect.h % 32 == 3) ||
			((req->dst_rect.h % 32) == 1 && req->dst_rect.h != 1) ||
			((req->dst_rect.h % 32) == 2 && req->dst_rect.h != 2))
			ret = mdp_blit_split_height(info, &splitreq);
		else
			ret = mdp_ppp_blit(info, &splitreq);
		if (ret)
			return ret;
		/* blit second region */
		if (((splitreq.flags & 0x07) == 0x07) ||
			((splitreq.flags & 0x07) == 0x0)) {
			splitreq.src_rect.h = s_h_1;
			splitreq.src_rect.y = s_y_1;
			splitreq.dst_rect.h = d_h_1;
			splitreq.dst_rect.y = d_y_1;
			splitreq.src_rect.x = s_x_1;
			splitreq.src_rect.w = s_w_1;
			splitreq.dst_rect.x = d_x_1;
			splitreq.dst_rect.w = d_w_1;
		} else {
			splitreq.src_rect.h = s_h_1;
			splitreq.src_rect.y = s_y_1;
			splitreq.dst_rect.h = d_h_0;
			splitreq.dst_rect.y = d_y_0;
			splitreq.src_rect.x = s_x_1;
			splitreq.src_rect.w = s_w_1;
			splitreq.dst_rect.x = d_x_0;
			splitreq.dst_rect.w = d_w_0;
		}
		if (((splitreq.dst_rect.h % 32) == 3) ||
			((req->dst_rect.h % 32) == 1 && req->dst_rect.h != 1) ||
			((req->dst_rect.h % 32) == 2 && req->dst_rect.h != 2))
			ret = mdp_blit_split_height(info, &splitreq);
		else
			ret = mdp_ppp_blit(info, &splitreq);
		if (ret)
			return ret;
	} else if ((req->dst_rect.h % 32) == 3 ||
		((req->dst_rect.h % 32) == 1 && req->dst_rect.h != 1) ||
		((req->dst_rect.h % 32) == 2 && req->dst_rect.h != 2))
		ret = mdp_blit_split_height(info, req);
	else
		ret = mdp_ppp_blit(info, req);
	return ret;
#elif defined CONFIG_FB_MSM_MDP30
	/* MDP width split workaround */
	remainder = (req->dst_rect.w)%16;
	ret = mdp_get_bytes_per_pixel(req->dst.format,
					(struct msm_fb_data_type *)info->par);
	if (ret <= 0) {
		printk(KERN_ERR "mdp_ppp: incorrect bpp!\n");
		return -EINVAL;
	}
	is_bpp_4 = (ret == 4) ? 1 : 0;

	if ((is_bpp_4 && (remainder == 6 || remainder == 14))) {

		/* make new request as provide by user */
		splitreq = *req;

		/* break dest roi at width*/
		d_y_0 = d_y_1 = req->dst_rect.y;
		d_h_0 = d_h_1 = req->dst_rect.h;
		d_x_0 = req->dst_rect.x;

		if (remainder == 14 || remainder == 6)
			d_w_1 = req->dst_rect.w / 2;
		else
			d_w_1 = (req->dst_rect.w - 1) / 2 - 1;

		d_w_0 = req->dst_rect.w - d_w_1;
		d_x_1 = d_x_0 + d_w_0;

		/* blit first region */
		if (((splitreq.flags & 0x07) == 0x07) ||
			((splitreq.flags & 0x07) == 0x05) ||
			((splitreq.flags & 0x07) == 0x02) ||
			((splitreq.flags & 0x07) == 0x0)) {

			if (splitreq.flags & MDP_ROT_90) {
				s_x_0 = s_x_1 = req->src_rect.x;
				s_w_0 = s_w_1 = req->src_rect.w;
				s_y_0 = req->src_rect.y;
				s_h_1 = (req->src_rect.h * d_w_1) /
					req->dst_rect.w;
				s_h_0 = req->src_rect.h - s_h_1;
				s_y_1 = s_y_0 + s_h_0;
				if (d_w_1 >= 8 * s_h_1) {
					s_h_1++;
					s_y_1--;
				}
			} else {
				s_y_0 = s_y_1 = req->src_rect.y;
				s_h_0 = s_h_1 = req->src_rect.h;
				s_x_0 = req->src_rect.x;
				s_w_1 = (req->src_rect.w * d_w_1) /
					req->dst_rect.w;
				s_w_0 = req->src_rect.w - s_w_1;
				s_x_1 = s_x_0 + s_w_0;
				if (d_w_1 >= 8 * s_w_1) {
					s_w_1++;
					s_x_1--;
				}
			}

			splitreq.src_rect.h = s_h_0;
			splitreq.src_rect.y = s_y_0;
			splitreq.dst_rect.h = d_h_0;
			splitreq.dst_rect.y = d_y_0;
			splitreq.src_rect.x = s_x_0;
			splitreq.src_rect.w = s_w_0;
			splitreq.dst_rect.x = d_x_0;
			splitreq.dst_rect.w = d_w_0;
		} else {
			if (splitreq.flags & MDP_ROT_90) {
				s_x_0 = s_x_1 = req->src_rect.x;
				s_w_0 = s_w_1 = req->src_rect.w;
				s_y_0 = req->src_rect.y;
				s_h_1 = (req->src_rect.h * d_w_0) /
					req->dst_rect.w;
				s_h_0 = req->src_rect.h - s_h_1;
				s_y_1 = s_y_0 + s_h_0;
				if (d_w_0 >= 8 * s_h_1) {
					s_h_1++;
					s_y_1--;
				}
			} else {
				s_y_0 = s_y_1 = req->src_rect.y;
				s_h_0 = s_h_1 = req->src_rect.h;
				s_x_0 = req->src_rect.x;
				s_w_1 = (req->src_rect.w * d_w_0) /
					req->dst_rect.w;
				s_w_0 = req->src_rect.w - s_w_1;
				s_x_1 = s_x_0 + s_w_0;
				if (d_w_0 >= 8 * s_w_1) {
					s_w_1++;
					s_x_1--;
				}
			}
			splitreq.src_rect.h = s_h_0;
			splitreq.src_rect.y = s_y_0;
			splitreq.dst_rect.h = d_h_1;
			splitreq.dst_rect.y = d_y_1;
			splitreq.src_rect.x = s_x_0;
			splitreq.src_rect.w = s_w_0;
			splitreq.dst_rect.x = d_x_1;
			splitreq.dst_rect.w = d_w_1;
		}

		/* No need to split in height */
		ret = mdp_ppp_blit(info, &splitreq);

		if (ret)
			return ret;

		/* blit second region */
		if (((splitreq.flags & 0x07) == 0x07) ||
			((splitreq.flags & 0x07) == 0x05) ||
			((splitreq.flags & 0x07) == 0x02) ||
			((splitreq.flags & 0x07) == 0x0)) {
			splitreq.src_rect.h = s_h_1;
			splitreq.src_rect.y = s_y_1;
			splitreq.dst_rect.h = d_h_1;
			splitreq.dst_rect.y = d_y_1;
			splitreq.src_rect.x = s_x_1;
			splitreq.src_rect.w = s_w_1;
			splitreq.dst_rect.x = d_x_1;
			splitreq.dst_rect.w = d_w_1;
		} else {
			splitreq.src_rect.h = s_h_1;
			splitreq.src_rect.y = s_y_1;
			splitreq.dst_rect.h = d_h_0;
			splitreq.dst_rect.y = d_y_0;
			splitreq.src_rect.x = s_x_1;
			splitreq.src_rect.w = s_w_1;
			splitreq.dst_rect.x = d_x_0;
			splitreq.dst_rect.w = d_w_0;
		}

		/* No need to split in height ... just width */
		ret = mdp_ppp_blit(info, &splitreq);

		if (ret)
			return ret;

	} else
		ret = mdp_ppp_blit(info, req);
	return ret;
#else
	ret = mdp_ppp_blit(info, req);
	return ret;
#endif
}

typedef void (*msm_dma_barrier_function_pointer) (void *, size_t);

static inline void msm_fb_dma_barrier_for_rect(struct fb_info *info,
			struct mdp_img *img, struct mdp_rect *rect,
			msm_dma_barrier_function_pointer dma_barrier_fp
			)
{
	/*
	 * Compute the start and end addresses of the rectangles.
	 * NOTE: As currently implemented, the data between
	 *       the end of one row and the start of the next is
	 *       included in the address range rather than
	 *       doing multiple calls for each row.
	 */
	unsigned long start;
	size_t size;
	char * const pmem_start = info->screen_base;
	int bytes_per_pixel = mdp_get_bytes_per_pixel(img->format,
					(struct msm_fb_data_type *)info->par);
	if (bytes_per_pixel <= 0) {
		printk(KERN_ERR "%s incorrect bpp!\n", __func__);
		return;
	}
	start = (unsigned long)pmem_start + img->offset +
		(img->width * rect->y + rect->x) * bytes_per_pixel;
	size  = (rect->h * img->width + rect->w) * bytes_per_pixel;
	(*dma_barrier_fp) ((void *) start, size);

}

static inline void msm_dma_nc_pre(void)
{
	dmb();
}
static inline void msm_dma_wt_pre(void)
{
	dmb();
}
static inline void msm_dma_todevice_wb_pre(void *start, size_t size)
{
	dma_cache_pre_ops(start, size, DMA_TO_DEVICE);
}

static inline void msm_dma_fromdevice_wb_pre(void *start, size_t size)
{
	dma_cache_pre_ops(start, size, DMA_FROM_DEVICE);
}

static inline void msm_dma_nc_post(void)
{
	dmb();
}

static inline void msm_dma_fromdevice_wt_post(void *start, size_t size)
{
	dma_cache_post_ops(start, size, DMA_FROM_DEVICE);
}

static inline void msm_dma_todevice_wb_post(void *start, size_t size)
{
	dma_cache_post_ops(start, size, DMA_TO_DEVICE);
}

static inline void msm_dma_fromdevice_wb_post(void *start, size_t size)
{
	dma_cache_post_ops(start, size, DMA_FROM_DEVICE);
}

/*
 * Do the write barriers required to guarantee data is committed to RAM
 * (from CPU cache or internal buffers) before a DMA operation starts.
 * NOTE: As currently implemented, the data between
 *       the end of one row and the start of the next is
 *       included in the address range rather than
 *       doing multiple calls for each row.
*/
static void msm_fb_ensure_memory_coherency_before_dma(struct fb_info *info,
		struct mdp_blit_req *req_list,
		int req_list_count)
{
#ifdef CONFIG_ARCH_QSD8X50
	int i;

	/*
	 * Normally, do the requested barriers for each address
	 * range that corresponds to a rectangle.
	 *
	 * But if at least one write barrier is requested for data
	 * going to or from the device but no address range is
	 * needed for that barrier, then do the barrier, but do it
	 * only once, no matter how many requests there are.
	 */
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	switch (mfd->mdp_fb_page_protection)	{
	default:
	case MDP_FB_PAGE_PROTECTION_NONCACHED:
	case MDP_FB_PAGE_PROTECTION_WRITECOMBINE:
		/*
		 * The following barrier is only done at most once,
		 * since further calls would be redundant.
		 */
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags
				& MDP_NO_DMA_BARRIER_START)) {
				msm_dma_nc_pre();
				break;
			}
		}
		break;

	case MDP_FB_PAGE_PROTECTION_WRITETHROUGHCACHE:
		/*
		 * The following barrier is only done at most once,
		 * since further calls would be redundant.
		 */
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags
				& MDP_NO_DMA_BARRIER_START)) {
				msm_dma_wt_pre();
				break;
			}
		}
		break;

	case MDP_FB_PAGE_PROTECTION_WRITEBACKCACHE:
	case MDP_FB_PAGE_PROTECTION_WRITEBACKWACACHE:
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags &
					MDP_NO_DMA_BARRIER_START)) {

				msm_fb_dma_barrier_for_rect(info,
						&(req_list[i].src),
						&(req_list[i].src_rect),
						msm_dma_todevice_wb_pre
						);

				msm_fb_dma_barrier_for_rect(info,
						&(req_list[i].dst),
						&(req_list[i].dst_rect),
						msm_dma_todevice_wb_pre
						);
			}
		}
		break;
	}
#else
	dmb();
#endif
}


/*
 * Do the write barriers required to guarantee data will be re-read from RAM by
 * the CPU after a DMA operation ends.
 * NOTE: As currently implemented, the data between
 *       the end of one row and the start of the next is
 *       included in the address range rather than
 *       doing multiple calls for each row.
*/
static void msm_fb_ensure_memory_coherency_after_dma(struct fb_info *info,
		struct mdp_blit_req *req_list,
		int req_list_count)
{
#ifdef CONFIG_ARCH_QSD8X50
	int i;

	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	switch (mfd->mdp_fb_page_protection)	{
	default:
	case MDP_FB_PAGE_PROTECTION_NONCACHED:
	case MDP_FB_PAGE_PROTECTION_WRITECOMBINE:
		/*
		 * The following barrier is only done at most once,
		 * since further calls would be redundant.
		 */
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags
				& MDP_NO_DMA_BARRIER_END)) {
				msm_dma_nc_post();
				break;
			}
		}
		break;

	case MDP_FB_PAGE_PROTECTION_WRITETHROUGHCACHE:
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags &
					MDP_NO_DMA_BARRIER_END)) {

				msm_fb_dma_barrier_for_rect(info,
						&(req_list[i].dst),
						&(req_list[i].dst_rect),
						msm_dma_fromdevice_wt_post
						);
			}
		}
		break;
	case MDP_FB_PAGE_PROTECTION_WRITEBACKCACHE:
	case MDP_FB_PAGE_PROTECTION_WRITEBACKWACACHE:
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags &
					MDP_NO_DMA_BARRIER_END)) {

				msm_fb_dma_barrier_for_rect(info,
						&(req_list[i].dst),
						&(req_list[i].dst_rect),
						msm_dma_fromdevice_wb_post
						);
			}
		}
		break;
	}
#else
	dmb();
#endif
}

/*
 * NOTE: The userspace issues blit operations in a sequence, the sequence
 * start with a operation marked START and ends in an operation marked
 * END. It is guranteed by the userspace that all the blit operations
 * between START and END are only within the regions of areas designated
 * by the START and END operations and that the userspace doesnt modify
 * those areas. Hence it would be enough to perform barrier/cache operations
 * only on the START and END operations.
 */
static int msmfb_blit(struct fb_info *info, void __user *p)
{
	/*
	 * CAUTION: The names of the struct types intentionally *DON'T* match
	 * the names of the variables declared -- they appear to be swapped.
	 * Read the code carefully and you should see that the variable names
	 * make sense.
	 */
	const int MAX_LIST_WINDOW = 16;
	struct mdp_blit_req req_list[MAX_LIST_WINDOW];
	struct mdp_blit_req_list req_list_header;

	int count, i, req_list_count;
	if (bf_supported &&
		(info->node == 1 || info->node == 2)) {
		pr_err("%s: no pan display for fb%d.",
		       __func__, info->node);
		return -EPERM;
	}
	/* Get the count size for the total BLIT request. */
	if (copy_from_user(&req_list_header, p, sizeof(req_list_header)))
		return -EFAULT;
	p += sizeof(req_list_header);
	count = req_list_header.count;
	if (count < 0 || count >= MAX_BLIT_REQ)
		return -EINVAL;
	while (count > 0) {
		/*
		 * Access the requests through a narrow window to decrease copy
		 * overhead and make larger requests accessible to the
		 * coherency management code.
		 * NOTE: The window size is intended to be larger than the
		 *       typical request size, but not require more than 2
		 *       kbytes of stack storage.
		 */
		req_list_count = count;
		if (req_list_count > MAX_LIST_WINDOW)
			req_list_count = MAX_LIST_WINDOW;
		if (copy_from_user(&req_list, p,
				sizeof(struct mdp_blit_req)*req_list_count))
			return -EFAULT;

		/*
		 * Ensure that any data CPU may have previously written to
		 * internal state (but not yet committed to memory) is
		 * guaranteed to be committed to memory now.
		 */
		msm_fb_ensure_memory_coherency_before_dma(info,
				req_list, req_list_count);

		/*
		 * Do the blit DMA, if required -- returning early only if
		 * there is a failure.
		 */
		for (i = 0; i < req_list_count; i++) {
			if (!(req_list[i].flags & MDP_NO_BLIT)) {
				/* Do the actual blit. */
				int ret = mdp_blit(info, &(req_list[i]));

				/*
				 * Note that early returns don't guarantee
				 * memory coherency.
				 */
				if (ret)
					return ret;
			}
		}

		/*
		 * Ensure that CPU cache and other internal CPU state is
		 * updated to reflect any change in memory modified by MDP blit
		 * DMA.
		 */
		msm_fb_ensure_memory_coherency_after_dma(info,
				req_list,
				req_list_count);

		/* Go to next window of requests. */
		count -= req_list_count;
		p += sizeof(struct mdp_blit_req)*req_list_count;
	}
	return 0;
}

static int msmfb_vsync_ctrl(struct fb_info *info, void __user *argp)
{
	int enable, ret;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	ret = copy_from_user(&enable, argp, sizeof(enable));
	if (ret) {
		pr_err("%s:msmfb_overlay_vsync ioctl failed", __func__);
		return ret;
	}

	if (mfd->vsync_ctrl)
		mfd->vsync_ctrl(enable);
	else {
		pr_err("%s: Vsync IOCTL not supported", __func__);
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_FB_MSM_OVERLAY
static int msmfb_overlay_get(struct fb_info *info, void __user *p)
{
	struct mdp_overlay req;
	int ret;

	if (copy_from_user(&req, p, sizeof(req)))
		return -EFAULT;

	ret = mdp4_overlay_get(info, &req);
	if (ret) {
		printk(KERN_ERR "%s: ioctl failed \n",
			__func__);
		return ret;
	}
	if (copy_to_user(p, &req, sizeof(req))) {
		printk(KERN_ERR "%s: copy2user failed \n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int msmfb_overlay_set(struct fb_info *info, void __user *p)
{
	struct mdp_overlay req;
	int ret;

	if (copy_from_user(&req, p, sizeof(req)))
		return -EFAULT;

	ret = mdp4_overlay_set(info, &req);
	if (ret) {
		printk(KERN_ERR "%s: ioctl failed, rc=%d\n",
			__func__, ret);
		return ret;
	}

	if (copy_to_user(p, &req, sizeof(req))) {
		printk(KERN_ERR "%s: copy2user failed \n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int msmfb_overlay_unset(struct fb_info *info, unsigned long *argp)
{
	int ret, ndx;

	ret = copy_from_user(&ndx, argp, sizeof(ndx));
	if (ret) {
		printk(KERN_ERR "%s:msmfb_overlay_unset ioctl failed \n",
			__func__);
		return ret;
	}

	return mdp4_overlay_unset(info, ndx);
}

static int msmfb_overlay_vsync_ctrl(struct fb_info *info, void __user *argp)
{
	int ret;
	int enable;

	ret = copy_from_user(&enable, argp, sizeof(enable));
	if (ret) {
		pr_err("%s:msmfb_overlay_vsync ioctl failed", __func__);
		return ret;
	}

	ret = mdp4_overlay_vsync_ctrl(info, enable);

	return ret;
}

static int msmfb_overlay_play_wait(struct fb_info *info, unsigned long *argp)
{
	int ret;
	struct msmfb_overlay_data req;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (mfd->overlay_play_enable == 0)      /* nothing to do */
		return 0;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("%s:msmfb_overlay_wait ioctl failed", __func__);
		return ret;
	}

	ret = mdp4_overlay_play_wait(info, &req);

	return ret;
}

static int msmfb_overlay_commit(struct fb_info *info)
{
	return mdp4_overlay_commit(info);
}

static int msmfb_overlay_play(struct fb_info *info, unsigned long *argp)
{
	int	ret;
	struct msmfb_overlay_data req;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	if (mfd->overlay_play_enable == 0)	/* nothing to do */
		return 0;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		printk(KERN_ERR "%s:msmfb_overlay_play ioctl failed \n",
			__func__);
		return ret;
	}

	complete(&mfd->msmfb_update_notify);
	mutex_lock(&msm_fb_notify_update_sem);
	if (mfd->msmfb_no_update_notify_timer.function)
		del_timer(&mfd->msmfb_no_update_notify_timer);

	mfd->msmfb_no_update_notify_timer.expires = jiffies + (2 * HZ);
	add_timer(&mfd->msmfb_no_update_notify_timer);
	mutex_unlock(&msm_fb_notify_update_sem);

	if (info->node == 0 && !(mfd->cont_splash_done)) { /* primary */
		mdp_set_dma_pan_info(info, NULL, TRUE);
		if (msm_fb_blank_sub(FB_BLANK_UNBLANK, info, mfd->op_enable)) {
			pr_err("%s: can't turn on display!\n", __func__);
			return -EINVAL;
		}
	}

	ret = mdp4_overlay_play(info, &req);

	if (unset_bl_level && !bl_updated)
		schedule_delayed_work(&mfd->backlight_worker,
				backlight_duration);

	if (info->node == 0 && (mfd->cont_splash_done)) /* primary */
		mdp_free_splash_buffer(mfd);

	return ret;
}

static int msmfb_overlay_play_enable(struct fb_info *info, unsigned long *argp)
{
	int	ret, enable;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	ret = copy_from_user(&enable, argp, sizeof(enable));
	if (ret) {
		printk(KERN_ERR "%s:msmfb_overlay_play_enable ioctl failed \n",
			__func__);
		return ret;
	}

	mfd->overlay_play_enable = enable;

	return 0;
}

static int msmfb_overlay_blt(struct fb_info *info, unsigned long *argp)
{
	int     ret;
	struct msmfb_overlay_blt req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("%s: failed\n", __func__);
		return ret;
	}

	ret = mdp4_overlay_blt(info, &req);

	return ret;
}

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static int msmfb_overlay_ioctl_writeback_init(struct fb_info *info)
{
	return mdp4_writeback_init(info);
}
static int msmfb_overlay_ioctl_writeback_start(
		struct fb_info *info)
{
	int ret = 0;
	ret = mdp4_writeback_start(info);
	if (ret)
		goto error;
error:
	if (ret)
		pr_err("%s:msmfb_writeback_start "
				" ioctl failed\n", __func__);
	return ret;
}

static int msmfb_overlay_ioctl_writeback_stop(
		struct fb_info *info)
{
	int ret = 0;
	ret = mdp4_writeback_stop(info);
	if (ret)
		goto error;

error:
	if (ret)
		pr_err("%s:msmfb_writeback_stop ioctl failed\n",
				__func__);
	return ret;
}

static int msmfb_overlay_ioctl_writeback_queue_buffer(
		struct fb_info *info, unsigned long *argp)
{
	int ret = 0;
	struct msmfb_data data;

	ret = copy_from_user(&data, argp, sizeof(data));
	if (ret)
		goto error;

	ret = mdp4_writeback_queue_buffer(info, &data);
	if (ret)
		goto error;

error:
	if (ret)
		pr_err("%s:msmfb_writeback_queue_buffer ioctl failed\n",
				__func__);
	return ret;
}

static int msmfb_overlay_ioctl_writeback_dequeue_buffer(
		struct fb_info *info, unsigned long *argp)
{
	int ret = 0;
	struct msmfb_data data;

	ret = copy_from_user(&data, argp, sizeof(data));
	if (ret)
		goto error;

	ret = mdp4_writeback_dequeue_buffer(info, &data);
	if (ret)
		goto error;

	ret = copy_to_user(argp, &data, sizeof(data));
	if (ret)
		goto error;

error:
	if (ret)
		pr_err("%s:msmfb_writeback_dequeue_buffer ioctl failed\n",
				__func__);
	return ret;
}
static int msmfb_overlay_ioctl_writeback_terminate(struct fb_info *info)
{
	return mdp4_writeback_terminate(info);
}

static int msmfb_overlay_ioctl_writeback_set_mirr_hint(struct fb_info *
		info, void *argp)
{
	int ret = 0, hint;

	if (!info) {
		ret = -EINVAL;
		goto error;
	}

	ret = copy_from_user(&hint, argp, sizeof(hint));
	if (ret)
		goto error;

	ret = mdp4_writeback_set_mirroring_hint(info, hint);
	if (ret)
		goto error;
error:
	if (ret)
		pr_err("%s: ioctl failed\n", __func__);
	return ret;
}

#else
static int msmfb_overlay_ioctl_writeback_init(struct fb_info *info)
{
	return -ENOTSUPP;
}
static int msmfb_overlay_ioctl_writeback_start(
		struct fb_info *info)
{
	return -ENOTSUPP;
}

static int msmfb_overlay_ioctl_writeback_stop(
		struct fb_info *info)
{
	return -ENOTSUPP;
}

static int msmfb_overlay_ioctl_writeback_queue_buffer(
		struct fb_info *info, unsigned long *argp)
{
	return -ENOTSUPP;
}

static int msmfb_overlay_ioctl_writeback_dequeue_buffer(
		struct fb_info *info, unsigned long *argp)
{
	return -ENOTSUPP;
}
static int msmfb_overlay_ioctl_writeback_terminate(struct fb_info *info)
{
	return -ENOTSUPP;
}

static int msmfb_overlay_ioctl_writeback_set_mirr_hint(struct fb_info *
		info, void *argp)
{
	return -ENOTSUPP;
}
#endif

static int msmfb_overlay_3d_sbys(struct fb_info *info, unsigned long *argp)
{
	int	ret;
	struct msmfb_overlay_3d req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("%s:msmfb_overlay_3d_ctrl ioctl failed\n",
			__func__);
		return ret;
	}

	ret = mdp4_overlay_3d_sbys(info, &req);

	return ret;
}

static int msmfb_mixer_info(struct fb_info *info, unsigned long *argp)
{
	int     ret, cnt;
	struct msmfb_mixer_info_req req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("%s: failed\n", __func__);
		return ret;
	}

	cnt = mdp4_mixer_info(req.mixer_num, req.info);
	req.cnt = cnt;
	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret)
		pr_err("%s:msmfb_overlay_blt_off ioctl failed\n",
		__func__);

	return cnt;
}

#endif

DEFINE_SEMAPHORE(msm_fb_ioctl_ppp_sem);
DEFINE_SEMAPHORE(msm_fb_ioctl_vsync_sem);
DEFINE_MUTEX(msm_fb_ioctl_lut_sem);

/* Set color conversion matrix from user space */

#ifndef CONFIG_FB_MSM_MDP40
static void msmfb_set_color_conv(struct mdp_ccs *p)
{
	int i;

	if (p->direction == MDP_CCS_RGB2YUV) {
		/* MDP cmd block enable */
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

		/* RGB->YUV primary forward matrix */
		for (i = 0; i < MDP_CCS_SIZE; i++)
			writel(p->ccs[i], MDP_CSC_PFMVn(i));

		#ifdef CONFIG_FB_MSM_MDP31
		for (i = 0; i < MDP_BV_SIZE; i++)
			writel(p->bv[i], MDP_CSC_POST_BV2n(i));
		#endif

		/* MDP cmd block disable */
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	} else {
		/* MDP cmd block enable */
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

		/* YUV->RGB primary reverse matrix */
		for (i = 0; i < MDP_CCS_SIZE; i++)
			writel(p->ccs[i], MDP_CSC_PRMVn(i));
		for (i = 0; i < MDP_BV_SIZE; i++)
			writel(p->bv[i], MDP_CSC_PRE_BV1n(i));

		/* MDP cmd block disable */
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	}
}
#else
static void msmfb_set_color_conv(struct mdp_csc *p)
{
	mdp4_vg_csc_update(p);
}
#endif

static int msmfb_notify_update(struct fb_info *info, unsigned long *argp)
{
	int ret, notify;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	ret = copy_from_user(&notify, argp, sizeof(int));
	if (ret) {
		pr_err("%s:ioctl failed\n", __func__);
		return ret;
	}

	if (notify > NOTIFY_UPDATE_STOP)
		return -EINVAL;

	if (notify == NOTIFY_UPDATE_START) {
		INIT_COMPLETION(mfd->msmfb_update_notify);
		ret = wait_for_completion_interruptible_timeout(
		&mfd->msmfb_update_notify, 4*HZ);
	} else {
		INIT_COMPLETION(mfd->msmfb_no_update_notify);
		ret = wait_for_completion_interruptible_timeout(
		&mfd->msmfb_no_update_notify, 4*HZ);
	}
	if (ret == 0)
		ret = -ETIMEDOUT;
	return (ret > 0) ? 0 : ret;
}

static int msmfb_handle_pp_ioctl(struct msm_fb_data_type *mfd,
						struct msmfb_mdp_pp *pp_ptr)
{
	int ret = -1;
#ifdef CONFIG_FB_MSM_MDP40
	int i = 0;
#endif
	if (!pp_ptr)
		return ret;

	switch (pp_ptr->op) {
#ifdef CONFIG_FB_MSM_MDP40
	case mdp_op_csc_cfg:
		ret = mdp4_csc_config(&(pp_ptr->data.csc_cfg_data));
		for (i = 0; i < CSC_MAX_BLOCKS; i++) {
			if (pp_ptr->data.csc_cfg_data.block ==
					csc_cfg_matrix[i].block) {
				memcpy(&csc_cfg_matrix[i].csc_data,
				&(pp_ptr->data.csc_cfg_data.csc_data),
				sizeof(struct mdp_csc_cfg));
				break;
			}
		}
		break;

	case mdp_op_pcc_cfg:
		ret = mdp4_pcc_cfg(&(pp_ptr->data.pcc_cfg_data));
		break;

	case mdp_op_lut_cfg:
		switch (pp_ptr->data.lut_cfg_data.lut_type) {
		case mdp_lut_igc:
			ret = mdp4_igc_lut_config(
					(struct mdp_igc_lut_data *)
					&pp_ptr->data.lut_cfg_data.data);
			break;

		case mdp_lut_pgc:
			ret = mdp4_argc_cfg(
				&pp_ptr->data.lut_cfg_data.data.pgc_lut_data);
			break;

		case mdp_lut_hist:
			ret = mdp_hist_lut_config(
					(struct mdp_hist_lut_data *)
					&pp_ptr->data.lut_cfg_data.data);
			break;

		default:
			break;
		}
		break;
	case mdp_op_qseed_cfg:
		ret = mdp4_qseed_cfg((struct mdp_qseed_cfg_data *)
						&pp_ptr->data.qseed_cfg_data);
		break;
	case mdp_op_calib_cfg:
		ret = mdp4_calib_config((struct mdp_calib_config_data *)
						&pp_ptr->data.calib_cfg);
		break;
#endif
	case mdp_bl_scale_cfg:
		ret = mdp_bl_scale_config(mfd, (struct mdp_bl_scale_data *)
				&pp_ptr->data.bl_scale_data);
		break;

	default:
		pr_warn("Unsupported request to MDP_PP IOCTL.\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}
static int msmfb_handle_metadata_ioctl(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata_ptr)
{
	int ret;
	switch (metadata_ptr->op) {
#ifdef CONFIG_FB_MSM_MDP40
	case metadata_op_base_blend:
		ret = mdp4_update_base_blend(mfd,
						&metadata_ptr->data.blend_cfg);
		break;
	case metadata_op_wb_format:
		ret = mdp4_update_writeback_format(mfd,
					&metadata_ptr->data.mixer_cfg);
		break;
#endif
	default:
		pr_warn("Unsupported request to MDP META IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int msmfb_get_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata_ptr)
{
	int ret = 0;
	switch (metadata_ptr->op) {
	case metadata_op_frame_rate:
		metadata_ptr->data.panel_frame_rate =
			mdp_get_panel_framerate(mfd);
		break;
	default:
		pr_warn("Unsupported request to MDP META IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int msmfb_handle_buf_sync_ioctl(struct msm_fb_data_type *mfd,
						struct mdp_buf_sync *buf_sync)
{
	int i, fence_cnt = 0, ret = 0;
	int acq_fen_fd[MDP_MAX_FENCE_FD];
	struct sync_fence *fence;

	if ((buf_sync->acq_fen_fd_cnt > MDP_MAX_FENCE_FD) ||
		(mfd->timeline == NULL))
		return -EINVAL;

	if (buf_sync->acq_fen_fd_cnt)
		ret = copy_from_user(acq_fen_fd, buf_sync->acq_fen_fd,
				buf_sync->acq_fen_fd_cnt * sizeof(int));
	if (ret) {
		pr_err("%s:copy_from_user failed", __func__);
		return ret;
	}
	mutex_lock(&mfd->sync_mutex);
	for (i = 0; i < buf_sync->acq_fen_fd_cnt; i++) {
		fence = sync_fence_fdget(acq_fen_fd[i]);
		if (fence == NULL) {
			pr_info("%s: null fence! i=%d fd=%d\n", __func__, i,
				acq_fen_fd[i]);
			ret = -EINVAL;
			break;
		}
		mfd->acq_fen[i] = fence;
	}
	fence_cnt = i;
	if (ret)
		goto buf_sync_err_1;
	mfd->acq_fen_cnt = fence_cnt;
	if (buf_sync->flags & MDP_BUF_SYNC_FLAG_WAIT)
		msm_fb_wait_for_fence(mfd);

	mfd->cur_rel_sync_pt = sw_sync_pt_create(mfd->timeline,
			mfd->timeline_value + 2);
	if (mfd->cur_rel_sync_pt == NULL) {
		pr_err("%s: cannot create sync point", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}
	/* create fence */
	mfd->cur_rel_fence = sync_fence_create("mdp-fence",
			mfd->cur_rel_sync_pt);
	if (mfd->cur_rel_fence == NULL) {
		sync_pt_free(mfd->cur_rel_sync_pt);
		mfd->cur_rel_sync_pt = NULL;
		pr_err("%s: cannot create fence", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}
	/* create fd */
	mfd->cur_rel_fen_fd = get_unused_fd_flags(0);
	sync_fence_install(mfd->cur_rel_fence, mfd->cur_rel_fen_fd);
	ret = copy_to_user(buf_sync->rel_fen_fd,
		&mfd->cur_rel_fen_fd, sizeof(int));
	if (ret) {
		pr_err("%s:copy_to_user failed", __func__);
		goto buf_sync_err_2;
	}
	mutex_unlock(&mfd->sync_mutex);
	return ret;
buf_sync_err_2:
	sync_fence_put(mfd->cur_rel_fence);
	put_unused_fd(mfd->cur_rel_fen_fd);
	mfd->cur_rel_fence = NULL;
	mfd->cur_rel_fen_fd = 0;
buf_sync_err_1:
	for (i = 0; i < fence_cnt; i++)
		sync_fence_put(mfd->acq_fen[i]);
	mfd->acq_fen_cnt = 0;
	mutex_unlock(&mfd->sync_mutex);
	return ret;
}

static int buf_fence_process(struct msm_fb_data_type *mfd,
						struct mdp_buf_fence *buf_fence)
{
	int i, fence_cnt = 0, ret;
	struct sync_fence *fence;

	if ((buf_fence->acq_fen_fd_cnt == 0) ||
		(buf_fence->acq_fen_fd_cnt > MDP_MAX_FENCE_FD) ||
		(mfd->timeline == NULL))
		return -EINVAL;

	mutex_lock(&mfd->sync_mutex);
	for (i = 0; i < buf_fence->acq_fen_fd_cnt; i++) {
		fence = sync_fence_fdget(buf_fence->acq_fen_fd[i]);
		if (fence == NULL) {
			pr_info("%s: null fence! i=%d fd=%d\n", __func__, i,
				buf_fence->acq_fen_fd[i]);
			ret = -EINVAL;
			break;
		}
		mfd->acq_fen[i] = fence;
	}
	fence_cnt = i;
	if (ret)
		goto buf_fence_err_1;
	mfd->cur_rel_sync_pt = sw_sync_pt_create(mfd->timeline,
			mfd->timeline_value + 2);
	if (mfd->cur_rel_sync_pt == NULL) {
		pr_err("%s: cannot create sync point", __func__);
		ret = -ENOMEM;
		goto buf_fence_err_1;
	}
	/* create fence */
	mfd->cur_rel_fence = sync_fence_create("mdp-fence",
			mfd->cur_rel_sync_pt);
	if (mfd->cur_rel_fence == NULL) {
		sync_pt_free(mfd->cur_rel_sync_pt);
		mfd->cur_rel_sync_pt = NULL;
		pr_err("%s: cannot create fence", __func__);
		ret = -ENOMEM;
		goto buf_fence_err_1;
	}
	/* create fd */
	mfd->cur_rel_fen_fd = get_unused_fd_flags(0);
	sync_fence_install(mfd->cur_rel_fence, mfd->cur_rel_fen_fd);
	buf_fence->rel_fen_fd[0] = mfd->cur_rel_fen_fd;
	/* Only one released fd for now, -1 indicates an end */
	buf_fence->rel_fen_fd[1] = -1;
	mfd->acq_fen_cnt = buf_fence->acq_fen_fd_cnt;
	mutex_unlock(&mfd->sync_mutex);
	return ret;
buf_fence_err_1:
	for (i = 0; i < fence_cnt; i++)
		sync_fence_put(mfd->acq_fen[i]);
	mfd->acq_fen_cnt = 0;
	mutex_unlock(&mfd->sync_mutex);
	return ret;
}
static int msmfb_display_commit(struct fb_info *info,
						unsigned long *argp)
{
	int ret;
	u32 copy_back = FALSE;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdp_display_commit disp_commit;
	struct mdp_buf_fence *buf_fence;
	ret = copy_from_user(&disp_commit, argp,
			sizeof(disp_commit));
	if (ret) {
		pr_err("%s:copy_from_user failed", __func__);
		return ret;
	}
	buf_fence = &disp_commit.buf_fence;
	if (buf_fence->acq_fen_fd_cnt > 0)
		ret = buf_fence_process(mfd, buf_fence);
	if ((!ret) && (buf_fence->rel_fen_fd[0] > 0))
		copy_back = TRUE;

	ret = msm_fb_pan_display_ex(&disp_commit.var,
			      info, disp_commit.wait_for_finish);

	if (copy_back) {
		ret = copy_to_user(argp,
			&disp_commit, sizeof(disp_commit));
		if (ret)
			pr_err("%s:copy_to_user failed", __func__);
	}
	return ret;
}
static int msm_fb_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	void __user *argp = (void __user *)arg;
	struct fb_cursor cursor;
	struct fb_cmap cmap;
	struct mdp_histogram_data hist;
	struct mdp_histogram_start_req hist_req;
	uint32_t block;
#ifndef CONFIG_FB_MSM_MDP40
	struct mdp_ccs ccs_matrix;
#else
	struct mdp_csc csc_matrix;
#endif
	struct mdp_page_protection fb_page_protection;
	struct msmfb_mdp_pp mdp_pp;
	struct msmfb_metadata mdp_metadata;
	struct mdp_buf_sync buf_sync;
	int ret = 0;
	msm_fb_pan_idle(mfd);

	switch (cmd) {
#ifdef CONFIG_FB_MSM_OVERLAY
	case MSMFB_OVERLAY_GET:
		ret = msmfb_overlay_get(info, argp);
		break;
	case MSMFB_OVERLAY_SET:
		ret = msmfb_overlay_set(info, argp);
		break;
	case MSMFB_OVERLAY_UNSET:
		ret = msmfb_overlay_unset(info, argp);
		break;
	case MSMFB_OVERLAY_COMMIT:
		down(&msm_fb_ioctl_ppp_sem);
		ret = msmfb_overlay_commit(info);
		up(&msm_fb_ioctl_ppp_sem);
		break;
	case MSMFB_OVERLAY_PLAY:
		ret = msmfb_overlay_play(info, argp);
		break;
	case MSMFB_OVERLAY_PLAY_ENABLE:
		ret = msmfb_overlay_play_enable(info, argp);
		break;
	case MSMFB_OVERLAY_PLAY_WAIT:
		ret = msmfb_overlay_play_wait(info, argp);
		break;
	case MSMFB_OVERLAY_BLT:
		ret = msmfb_overlay_blt(info, argp);
		break;
	case MSMFB_OVERLAY_3D:
		ret = msmfb_overlay_3d_sbys(info, argp);
		break;
	case MSMFB_MIXER_INFO:
		ret = msmfb_mixer_info(info, argp);
		break;
	case MSMFB_WRITEBACK_INIT:
		ret = msmfb_overlay_ioctl_writeback_init(info);
		break;
	case MSMFB_WRITEBACK_START:
		ret = msmfb_overlay_ioctl_writeback_start(
				info);
		break;
	case MSMFB_WRITEBACK_STOP:
		ret = msmfb_overlay_ioctl_writeback_stop(
				info);
		break;
	case MSMFB_WRITEBACK_QUEUE_BUFFER:
		ret = msmfb_overlay_ioctl_writeback_queue_buffer(
				info, argp);
		break;
	case MSMFB_WRITEBACK_DEQUEUE_BUFFER:
		ret = msmfb_overlay_ioctl_writeback_dequeue_buffer(
				info, argp);
		break;
	case MSMFB_WRITEBACK_TERMINATE:
		ret = msmfb_overlay_ioctl_writeback_terminate(info);
		break;
	case MSMFB_WRITEBACK_SET_MIRRORING_HINT:
		ret = msmfb_overlay_ioctl_writeback_set_mirr_hint(
				info, argp);
		break;
#endif
	case MSMFB_VSYNC_CTRL:
	case MSMFB_OVERLAY_VSYNC_CTRL:
		down(&msm_fb_ioctl_vsync_sem);
		if (mdp_rev >= MDP_REV_40)
			ret = msmfb_overlay_vsync_ctrl(info, argp);
		else
			ret = msmfb_vsync_ctrl(info, argp);
		up(&msm_fb_ioctl_vsync_sem);
		break;
	case MSMFB_BLIT:
		down(&msm_fb_ioctl_ppp_sem);
		ret = msmfb_blit(info, argp);
		up(&msm_fb_ioctl_ppp_sem);

		break;

	/* Ioctl for setting ccs matrix from user space */
	case MSMFB_SET_CCS_MATRIX:
#ifndef CONFIG_FB_MSM_MDP40
		ret = copy_from_user(&ccs_matrix, argp, sizeof(ccs_matrix));
		if (ret) {
			printk(KERN_ERR
				"%s:MSMFB_SET_CCS_MATRIX ioctl failed \n",
				__func__);
			return ret;
		}

		down(&msm_fb_ioctl_ppp_sem);
		if (ccs_matrix.direction == MDP_CCS_RGB2YUV)
			mdp_ccs_rgb2yuv = ccs_matrix;
		else
			mdp_ccs_yuv2rgb = ccs_matrix;

		msmfb_set_color_conv(&ccs_matrix) ;
		up(&msm_fb_ioctl_ppp_sem);
#else
		ret = copy_from_user(&csc_matrix, argp, sizeof(csc_matrix));
		if (ret) {
			pr_err("%s:MSMFB_SET_CSC_MATRIX ioctl failed\n",
				__func__);
			return ret;
		}
		down(&msm_fb_ioctl_ppp_sem);
		msmfb_set_color_conv(&csc_matrix);
		up(&msm_fb_ioctl_ppp_sem);

#endif

		break;

	/* Ioctl for getting ccs matrix to user space */
	case MSMFB_GET_CCS_MATRIX:
#ifndef CONFIG_FB_MSM_MDP40
		ret = copy_from_user(&ccs_matrix, argp, sizeof(ccs_matrix)) ;
		if (ret) {
			printk(KERN_ERR
				"%s:MSMFB_GET_CCS_MATRIX ioctl failed \n",
				 __func__);
			return ret;
		}

		down(&msm_fb_ioctl_ppp_sem);
		if (ccs_matrix.direction == MDP_CCS_RGB2YUV)
			ccs_matrix = mdp_ccs_rgb2yuv;
		 else
			ccs_matrix =  mdp_ccs_yuv2rgb;

		ret = copy_to_user(argp, &ccs_matrix, sizeof(ccs_matrix));

		if (ret)	{
			printk(KERN_ERR
				"%s:MSMFB_GET_CCS_MATRIX ioctl failed \n",
				 __func__);
			return ret ;
		}
		up(&msm_fb_ioctl_ppp_sem);
#else
		ret = -EINVAL;
#endif

		break;

	case MSMFB_GRP_DISP:
#ifdef CONFIG_FB_MSM_MDP22
		{
			unsigned long grp_id;

			ret = copy_from_user(&grp_id, argp, sizeof(grp_id));
			if (ret)
				return ret;

			mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
			writel(grp_id, MDP_FULL_BYPASS_WORD43);
			mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF,
				      FALSE);
			break;
		}
#else
		return -EFAULT;
#endif
	case MSMFB_SUSPEND_SW_REFRESHER:
		if (!mfd->panel_power_on)
			return -EPERM;

		mfd->sw_refreshing_enable = FALSE;
		ret = msm_fb_stop_sw_refresher(mfd);
		break;

	case MSMFB_RESUME_SW_REFRESHER:
		if (!mfd->panel_power_on)
			return -EPERM;

		mfd->sw_refreshing_enable = TRUE;
		ret = msm_fb_resume_sw_refresher(mfd);
		break;

	case MSMFB_CURSOR:
		ret = copy_from_user(&cursor, argp, sizeof(cursor));
		if (ret)
			return ret;

		ret = msm_fb_cursor(info, &cursor);
		break;

	case MSMFB_SET_LUT:
		ret = copy_from_user(&cmap, argp, sizeof(cmap));
		if (ret)
			return ret;

		mutex_lock(&msm_fb_ioctl_lut_sem);
		ret = msm_fb_set_lut(&cmap, info);
		mutex_unlock(&msm_fb_ioctl_lut_sem);
		break;

	case MSMFB_HISTOGRAM:
		if (!mfd->panel_power_on)
			return -EPERM;

		if (!mfd->do_histogram)
			return -ENODEV;

		ret = copy_from_user(&hist, argp, sizeof(hist));
		if (ret)
			return ret;

		ret = mfd->do_histogram(info, &hist);
		break;

	case MSMFB_HISTOGRAM_START:
		if (!mfd->panel_power_on)
			return -EPERM;

		if (!mfd->start_histogram)
			return -ENODEV;

		ret = copy_from_user(&hist_req, argp, sizeof(hist_req));
		if (ret)
			return ret;

		ret = mfd->start_histogram(&hist_req);
		break;

	case MSMFB_HISTOGRAM_STOP:
		if (!mfd->stop_histogram)
			return -ENODEV;

		ret = copy_from_user(&block, argp, sizeof(int));
		if (ret)
			return ret;

		ret = mfd->stop_histogram(info, block);
		break;


	case MSMFB_GET_PAGE_PROTECTION:
		fb_page_protection.page_protection
			= mfd->mdp_fb_page_protection;
		ret = copy_to_user(argp, &fb_page_protection,
				sizeof(fb_page_protection));
		if (ret)
				return ret;
		break;

	case MSMFB_NOTIFY_UPDATE:
		ret = msmfb_notify_update(info, argp);
		break;

	case MSMFB_SET_PAGE_PROTECTION:
#if defined CONFIG_ARCH_QSD8X50 || defined CONFIG_ARCH_MSM8X60
		ret = copy_from_user(&fb_page_protection, argp,
				sizeof(fb_page_protection));
		if (ret)
				return ret;

		/* Validate the proposed page protection settings. */
		switch (fb_page_protection.page_protection)	{
		case MDP_FB_PAGE_PROTECTION_NONCACHED:
		case MDP_FB_PAGE_PROTECTION_WRITECOMBINE:
		case MDP_FB_PAGE_PROTECTION_WRITETHROUGHCACHE:
		/* Write-back cache (read allocate)  */
		case MDP_FB_PAGE_PROTECTION_WRITEBACKCACHE:
		/* Write-back cache (write allocate) */
		case MDP_FB_PAGE_PROTECTION_WRITEBACKWACACHE:
			mfd->mdp_fb_page_protection =
				fb_page_protection.page_protection;
			break;
		default:
			ret = -EINVAL;
			break;
		}
#else
		/*
		 * Don't allow caching until 7k DMA cache operations are
		 * available.
		 */
		ret = -EINVAL;
#endif
		break;

	case MSMFB_MDP_PP:
		ret = copy_from_user(&mdp_pp, argp, sizeof(mdp_pp));
		if (ret)
			return ret;

		ret = msmfb_handle_pp_ioctl(mfd, &mdp_pp);
		if (ret == 1)
			ret = copy_to_user(argp, &mdp_pp, sizeof(mdp_pp));
		break;
	case MSMFB_BUFFER_SYNC:
		ret = copy_from_user(&buf_sync, argp, sizeof(buf_sync));
		if (ret)
			return ret;

		ret = msmfb_handle_buf_sync_ioctl(mfd, &buf_sync);

		if (!ret)
			ret = copy_to_user(argp, &buf_sync, sizeof(buf_sync));
		break;

	case MSMFB_METADATA_SET:
		ret = copy_from_user(&mdp_metadata, argp, sizeof(mdp_metadata));
		if (ret)
			return ret;
		ret = msmfb_handle_metadata_ioctl(mfd, &mdp_metadata);
	case MSMFB_DISPLAY_COMMIT:
		ret = msmfb_display_commit(info, argp);
		break;

	case MSMFB_METADATA_GET:
		ret = copy_from_user(&mdp_metadata, argp, sizeof(mdp_metadata));
		if (ret)
			return ret;
		ret = msmfb_get_metadata(mfd, &mdp_metadata);
		if (!ret)
			ret = copy_to_user(argp, &mdp_metadata,
				sizeof(mdp_metadata));
		break;

	default:
		MSM_FB_INFO("MDP: unknown ioctl (cmd=%x) received!\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int msm_fb_register_driver(void)
{
	return platform_driver_register(&msm_fb_driver);
}

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
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

int msm_fb_writeback_start(struct fb_info *info)
{
	return mdp4_writeback_start(info);
}
EXPORT_SYMBOL(msm_fb_writeback_start);

int msm_fb_writeback_queue_buffer(struct fb_info *info,
		struct msmfb_data *data)
{
	return mdp4_writeback_queue_buffer(info, data);
}
EXPORT_SYMBOL(msm_fb_writeback_queue_buffer);

int msm_fb_writeback_dequeue_buffer(struct fb_info *info,
		struct msmfb_data *data)
{
	return mdp4_writeback_dequeue_buffer(info, data);
}
EXPORT_SYMBOL(msm_fb_writeback_dequeue_buffer);

int msm_fb_writeback_stop(struct fb_info *info)
{
	return mdp4_writeback_stop(info);
}
EXPORT_SYMBOL(msm_fb_writeback_stop);
int msm_fb_writeback_init(struct fb_info *info)
{
	return mdp4_writeback_init(info);
}
EXPORT_SYMBOL(msm_fb_writeback_init);
int msm_fb_writeback_terminate(struct fb_info *info)
{
	return mdp4_writeback_terminate(info);
}
EXPORT_SYMBOL(msm_fb_writeback_terminate);
#endif

struct platform_device *msm_fb_add_device(struct platform_device *pdev)
{
	struct msm_fb_panel_data *pdata;
	struct platform_device *this_dev = NULL;
	struct fb_info *fbi;
	struct msm_fb_data_type *mfd = NULL;
	u32 type, id, fb_num;

	if (!pdev)
		return NULL;
	id = pdev->id;

	pdata = pdev->dev.platform_data;
	if (!pdata)
		return NULL;
	type = pdata->panel_info.type;

#if defined MSM_FB_NUM
	/*
	 * over written fb_num which defined
	 * at panel_info
	 *
	 */
	if (type == HDMI_PANEL || type == DTV_PANEL ||
		type == TV_PANEL || type == WRITEBACK_PANEL) {
		if (hdmi_prim_display)
			pdata->panel_info.fb_num = 2;
		else
			pdata->panel_info.fb_num = 1;
	}
	else
		pdata->panel_info.fb_num = MSM_FB_NUM;

	MSM_FB_INFO("setting pdata->panel_info.fb_num to %d. type: %d\n",
			pdata->panel_info.fb_num, type);
#endif
	fb_num = pdata->panel_info.fb_num;

	if (fb_num <= 0)
		return NULL;

	if (fbi_list_index >= MAX_FBI_LIST) {
		printk(KERN_ERR "msm_fb: no more framebuffer info list!\n");
		return NULL;
	}
	/*
	 * alloc panel device data
	 */
	this_dev = msm_fb_device_alloc(pdata, type, id);

	if (!this_dev) {
		printk(KERN_ERR
		"%s: msm_fb_device_alloc failed!\n", __func__);
		return NULL;
	}

	/*
	 * alloc framebuffer info + par data
	 */
	fbi = framebuffer_alloc(sizeof(struct msm_fb_data_type), NULL);
	if (fbi == NULL) {
		platform_device_put(this_dev);
		printk(KERN_ERR "msm_fb: can't alloca framebuffer info data!\n");
		return NULL;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	mfd->key = MFD_KEY;
	mfd->fbi = fbi;
	mfd->panel.type = type;
	mfd->panel.id = id;
	mfd->fb_page = fb_num;
	mfd->index = fbi_list_index;
	mfd->mdp_fb_page_protection = MDP_FB_PAGE_PROTECTION_WRITECOMBINE;
	mfd->iclient = iclient;
	/* link to the latest pdev */
	mfd->pdev = this_dev;

	mfd_list[mfd_list_index++] = mfd;
	fbi_list[fbi_list_index++] = fbi;

	/*
	 * set driver data
	 */
	platform_set_drvdata(this_dev, mfd);

	if (platform_device_add(this_dev)) {
		printk(KERN_ERR "msm_fb: platform_device_add failed!\n");
		platform_device_put(this_dev);
		framebuffer_release(fbi);
		fbi_list_index--;
		return NULL;
	}
	return this_dev;
}
EXPORT_SYMBOL(msm_fb_add_device);

int get_fb_phys_info(unsigned long *start, unsigned long *len, int fb_num,
	int subsys_id)
{
	struct fb_info *info;
	struct msm_fb_data_type *mfd;

	if (fb_num > MAX_FBI_LIST ||
		(subsys_id != DISPLAY_SUBSYSTEM_ID &&
		 subsys_id != ROTATOR_SUBSYSTEM_ID)) {
		pr_err("%s(): Invalid parameters\n", __func__);
		return -1;
	}

	info = fbi_list[fb_num];
	if (!info) {
		pr_err("%s(): info is NULL\n", __func__);
		return -1;
	}

	mfd = (struct msm_fb_data_type *)info->par;

	if (subsys_id == DISPLAY_SUBSYSTEM_ID) {
		if (mfd->display_iova)
			*start = mfd->display_iova;
		else
			*start = info->fix.smem_start;
	} else {
		if (mfd->rotator_iova)
			*start = mfd->rotator_iova;
		else
			*start = info->fix.smem_start;
	}

	*len = info->fix.smem_len;

	return 0;
}
EXPORT_SYMBOL(get_fb_phys_info);

int __init msm_fb_init(void)
{
	int rc = -ENODEV;

	if (msm_fb_register_driver())
		return rc;

#ifdef MSM_FB_ENABLE_DBGFS
	{
		struct dentry *root;

		if ((root = msm_fb_get_debugfs_root()) != NULL) {
			msm_fb_debugfs_file_create(root,
						   "msm_fb_msg_printing_level",
						   (u32 *) &msm_fb_msg_level);
			msm_fb_debugfs_file_create(root,
						   "mddi_msg_printing_level",
						   (u32 *) &mddi_msg_level);
			msm_fb_debugfs_file_create(root, "msm_fb_debug_enabled",
						   (u32 *) &msm_fb_debug_enabled);
		}
	}
#endif

	return 0;
}

/* Called by v4l2 driver to enable/disable overlay pipe */
int msm_fb_v4l2_enable(struct mdp_overlay *req, bool enable, void **par)
{
	int err = 0;
#ifdef CONFIG_FB_MSM_MDP40
	struct mdp4_overlay_pipe *pipe;
	if (enable) {

		err = mdp4_v4l2_overlay_set(fbi_list[0], req, &pipe);

		*(struct mdp4_overlay_pipe **)par = pipe;

	} else {
		pipe = *(struct mdp4_overlay_pipe **)par;
		mdp4_v4l2_overlay_clear(pipe);
	}
#else
#ifdef CONFIG_FB_MSM_MDP30
	if (enable)
		err = mdp_ppp_v4l2_overlay_set(fbi_list[0], req);
	else
		err = mdp_ppp_v4l2_overlay_clear();
#else
	err = -EINVAL;
#endif
#endif

	return err;
}
EXPORT_SYMBOL(msm_fb_v4l2_enable);

/* Called by v4l2 driver to provide a frame for display */
int msm_fb_v4l2_update(void *par, bool bUserPtr,
	unsigned long srcp0_addr, unsigned long srcp0_size,
	unsigned long srcp1_addr, unsigned long srcp1_size,
	unsigned long srcp2_addr, unsigned long srcp2_size)
{
#ifdef CONFIG_FB_MSM_MDP40
	struct mdp4_overlay_pipe *pipe = (struct mdp4_overlay_pipe *)par;
	return mdp4_v4l2_overlay_play(fbi_list[0], pipe,
		srcp0_addr, srcp1_addr,
		srcp2_addr);
#else
#ifdef CONFIG_FB_MSM_MDP30
	if (bUserPtr)
		return mdp_ppp_v4l2_overlay_play(fbi_list[0], true,
				srcp0_addr, srcp0_size,
				srcp1_addr, srcp1_size);
	else
		return mdp_ppp_v4l2_overlay_play(fbi_list[0], false,
			srcp0_addr, srcp0_size,
			srcp1_addr, srcp1_size);
#else
	return -EINVAL;
#endif
#endif
}
EXPORT_SYMBOL(msm_fb_v4l2_update);

module_init(msm_fb_init);
