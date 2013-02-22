/*
 * Core MDSS framebuffer driver.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/android_pmem.h>
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

#include <mach/board.h>
#include <mach/memory.h>
#include <mach/msm_memtypes.h>
#include <mach/iommu_domains.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MDSS_FB_NUM 3
#else
#define MDSS_FB_NUM 2
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

static int mdss_fb_register(struct msm_fb_data_type *mfd);
static int mdss_fb_open(struct fb_info *info, int user);
static int mdss_fb_release(struct fb_info *info, int user);
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
static int mdss_fb_mmap(struct fb_info *info, struct vm_area_struct *vma);
static void mdss_fb_release_fences(struct msm_fb_data_type *mfd);

static void mdss_fb_commit_wq_handler(struct work_struct *work);
static void mdss_fb_pan_idle(struct msm_fb_data_type *mfd);
static int mdss_fb_send_panel_event(struct msm_fb_data_type *mfd,
					int event, void *arg);
void mdss_fb_no_update_notify_timer_cb(unsigned long data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)data;
	if (!mfd)
		pr_err("%s mfd NULL\n", __func__);
	complete(&mfd->no_update.comp);
}

static int mdss_fb_notify_update(struct msm_fb_data_type *mfd,
							unsigned long *argp)
{
	int ret, notify;

	ret = copy_from_user(&notify, argp, sizeof(int));
	if (ret) {
		pr_err("%s:ioctl failed\n", __func__);
		return ret;
	}

	if (notify > NOTIFY_UPDATE_STOP)
		return -EINVAL;

	if (notify == NOTIFY_UPDATE_START) {
		INIT_COMPLETION(mfd->update.comp);
		ret = wait_for_completion_interruptible_timeout(
						&mfd->update.comp, 4 * HZ);
	} else {
		INIT_COMPLETION(mfd->no_update.comp);
		ret = wait_for_completion_interruptible_timeout(
						&mfd->no_update.comp, 4 * HZ);
	}
	if (ret == 0)
		ret = -ETIMEDOUT;
	return (ret > 0) ? 0 : ret;
}

#define MAX_BACKLIGHT_BRIGHTNESS 255
static int lcd_backlight_registered;

static void mdss_fb_set_bl_brightness(struct led_classdev *led_cdev,
				      enum led_brightness value)
{
	struct msm_fb_data_type *mfd = dev_get_drvdata(led_cdev->dev->parent);
	int bl_lvl;

	if (value > MAX_BACKLIGHT_BRIGHTNESS)
		value = MAX_BACKLIGHT_BRIGHTNESS;

	/* This maps android backlight level 0 to 255 into
	   driver backlight level 0 to bl_max with rounding */
	bl_lvl = (2 * value * mfd->panel_info->bl_max +
		  MAX_BACKLIGHT_BRIGHTNESS) / (2 * MAX_BACKLIGHT_BRIGHTNESS);

	if (!bl_lvl && value)
		bl_lvl = 1;

	mutex_lock(&mfd->lock);
	mdss_fb_set_backlight(mfd, bl_lvl);
	mutex_unlock(&mfd->lock);
}

static struct led_classdev backlight_led = {
	.name           = "lcd-backlight",
	.brightness     = MAX_BACKLIGHT_BRIGHTNESS,
	.brightness_set = mdss_fb_set_bl_brightness,
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

static DEVICE_ATTR(msm_fb_type, S_IRUGO, mdss_fb_get_type, NULL);
static struct attribute *mdss_fb_attrs[] = {
	&dev_attr_msm_fb_type.attr,
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

	mfd->bl_level = 0;
	mfd->bl_scale = 1024;
	mfd->bl_min_lvl = 30;
	mfd->fb_imgType = MDP_RGBA_8888;

	mfd->pdev = pdev;
	if (pdata->next)
		mfd->split_display = true;

	mutex_init(&mfd->lock);

	fbi_list[fbi_list_index++] = fbi;

	platform_set_drvdata(pdev, mfd);

	rc = mdss_fb_register(mfd);
	if (rc)
		return rc;

	rc = pm_runtime_set_active(mfd->fbi->dev);
	if (rc < 0)
		pr_err("pm_runtime: fail to set active.\n");
	pm_runtime_enable(mfd->fbi->dev);

	/* android supports only one lcd-backlight/lcd for now */
	if (!lcd_backlight_registered) {
		if (led_classdev_register(&pdev->dev, &backlight_led))
			pr_err("led_classdev_register failed\n");
		else
			lcd_backlight_registered = 1;
	}

	mdss_fb_create_sysfs(mfd);
	mdss_fb_send_panel_event(mfd, MDSS_EVENT_FB_REGISTERED, fbi);

	if (mfd->timeline == NULL) {
		char timeline_name[16];
		snprintf(timeline_name, sizeof(timeline_name),
			"mdss_fb_%d", mfd->index);
		mfd->timeline = sw_sync_timeline_create(timeline_name);
		if (mfd->timeline == NULL) {
			pr_err("%s: cannot create time line", __func__);
			return -ENOMEM;
		} else {
			mfd->timeline_value = 0;
		}
	}

	rc = mdss_mdp_overlay_init(mfd);
	if (rc)
		pr_err("unable to init overlay\n");

	return 0;
}

static int mdss_fb_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	mdss_fb_remove_sysfs(mfd);

	pm_runtime_disable(mfd->fbi->dev);

	if (!mfd)
		return -ENODEV;

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
	}
	mfd->is_power_setting = false;
	complete_all(&mfd->power_set_comp);

	return ret;
}

int mdss_fb_suspend_all(void)
{
	struct fb_info *fbi;
	int ret, i;
	int result = 0;
	for (i = 0; i < fbi_list_index; i++) {
		fbi = fbi_list[i];
		fb_set_suspend(fbi, FBINFO_STATE_SUSPENDED);

		ret = mdss_fb_suspend_sub(fbi->par);
		if (ret != 0) {
			fb_set_suspend(fbi, FBINFO_STATE_RUNNING);
			result = ret;
		}
	}
	return result;
}

int mdss_fb_resume_all(void)
{
	struct fb_info *fbi;
	int ret, i;
	int result = 0;

	for (i = 0; i < fbi_list_index; i++) {
		fbi = fbi_list[i];

		ret = mdss_fb_resume_sub(fbi->par);
		if (ret == 0)
			fb_set_suspend(fbi, FBINFO_STATE_RUNNING);
	}
	return result;
}

static const struct of_device_id mdss_fb_dt_match[] = {
	{ .compatible = "qcom,mdss-fb",},
	{}
};
EXPORT_COMPAT("qcom,mdss-fb");

static struct platform_driver mdss_fb_driver = {
	.probe = mdss_fb_probe,
	.remove = mdss_fb_remove,
	.driver = {
		.name = "mdss_fb",
		.of_match_table = mdss_fb_dt_match,
	},
};

static int unset_bl_level, bl_updated;
static int bl_level_old;

static int mdss_bl_scale_config(struct msm_fb_data_type *mfd,
						struct mdp_bl_scale_data *data)
{
	int ret = 0;
	int curr_bl;
	mutex_lock(&mfd->lock);
	curr_bl = mfd->bl_level;
	mfd->bl_scale = data->scale;
	mfd->bl_min_lvl = data->min_lvl;
	pr_debug("update scale = %d, min_lvl = %d\n", mfd->bl_scale,
							mfd->bl_min_lvl);

	/* update current backlight to use new scaling*/
	mdss_fb_set_backlight(mfd, curr_bl);
	mutex_unlock(&mfd->lock);
	return ret;
}

static void mdss_fb_scale_bl(struct msm_fb_data_type *mfd, u32 *bl_lvl)
{
	u32 temp = *bl_lvl;
	pr_debug("input = %d, scale = %d", temp, mfd->bl_scale);
	if (temp >= mfd->bl_min_lvl) {
		/* bl_scale is the numerator of scaling fraction (x/1024)*/
		temp = (temp * mfd->bl_scale) / 1024;

		/*if less than minimum level, use min level*/
		if (temp < mfd->bl_min_lvl)
			temp = mfd->bl_min_lvl;
	}
	pr_debug("output = %d", temp);

	(*bl_lvl) = temp;
}

/* must call this function from within mfd->lock */
void mdss_fb_set_backlight(struct msm_fb_data_type *mfd, u32 bkl_lvl)
{
	struct mdss_panel_data *pdata;
	u32 temp = bkl_lvl;

	if (!mfd->panel_power_on || !bl_updated) {
		unset_bl_level = bkl_lvl;
		return;
	} else {
		unset_bl_level = 0;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);

	if ((pdata) && (pdata->set_backlight)) {
		mdss_fb_scale_bl(mfd, &temp);
		/*
		 * Even though backlight has been scaled, want to show that
		 * backlight has been set to bkl_lvl to those that read from
		 * sysfs node. Thus, need to set bl_level even if it appears
		 * the backlight has already been set to the level it is at,
		 * as well as setting bl_level to bkl_lvl even though the
		 * backlight has been set to the scaled value.
		 */
		if (bl_level_old == temp) {
			mfd->bl_level = bkl_lvl;
			return;
		}
		pdata->set_backlight(pdata, temp);
		mfd->bl_level = bkl_lvl;
		bl_level_old = temp;
	}
}

void mdss_fb_update_backlight(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;

	if (unset_bl_level && !bl_updated) {
		pdata = dev_get_platdata(&mfd->pdev->dev);
		if ((pdata) && (pdata->set_backlight)) {
			mutex_lock(&mfd->lock);
			mfd->bl_level = unset_bl_level;
			pdata->set_backlight(pdata, mfd->bl_level);
			bl_level_old = unset_bl_level;
			mutex_unlock(&mfd->lock);
			bl_updated = 1;
		}
	}
}

static int mdss_fb_blank_sub(int blank_mode, struct fb_info *info,
			     int op_enable)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int ret = 0;

	if (!op_enable)
		return -EPERM;

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		if (!mfd->panel_power_on) {
			msleep(20);
			ret = mfd->on_fnc(mfd);
			if (ret == 0)
				mfd->panel_power_on = true;
		}
		break;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
	case FB_BLANK_POWERDOWN:
	default:
		if (mfd->panel_power_on) {
			int curr_pwr_state;

			del_timer(&mfd->no_update.timer);
			complete(&mfd->no_update.comp);

			mfd->op_enable = false;
			curr_pwr_state = mfd->panel_power_on;
			mfd->panel_power_on = false;
			bl_updated = 0;

			msleep(20);
			ret = mfd->off_fnc(mfd);
			if (ret)
				mfd->panel_power_on = curr_pwr_state;
			else
				mdss_fb_release_fences(mfd);
			mfd->op_enable = true;
		}
		break;
	}

	return ret;
}

static int mdss_fb_blank(int blank_mode, struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	if (blank_mode == FB_BLANK_POWERDOWN) {
		struct fb_event event;
		event.info = info;
		event.data = &blank_mode;
		fb_notifier_call_chain(FB_EVENT_BLANK, &event);
	}
	mdss_fb_pan_idle(mfd);
	if (mfd->op_enable == 0) {
		if (blank_mode == FB_BLANK_UNBLANK)
			mfd->suspend.panel_power_on = true;
		else
			mfd->suspend.panel_power_on = false;
		return 0;
	}
	return mdss_fb_blank_sub(blank_mode, info, mfd->op_enable);
}

/*
 * Custom Framebuffer mmap() function for MSM driver.
 * Differs from standard mmap() function by allowing for customized
 * page-protection.
 */
static int mdss_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	/* Get frame buffer memory range. */
	unsigned long start = info->fix.smem_start;
	u32 len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

	mdss_fb_pan_idle(mfd);
	if (off >= len) {
		/* memory mapped io */
		off -= len;
		if (info->var.accel_flags) {
			mutex_unlock(&info->lock);
			return -EINVAL;
		}
		start = info->fix.mmio_start;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.mmio_len);
	}

	/* Set VM flags. */
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;
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

static struct fb_ops mdss_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = mdss_fb_open,
	.fb_release = mdss_fb_release,
	.fb_check_var = mdss_fb_check_var,	/* vinfo check */
	.fb_set_par = mdss_fb_set_par,	/* set the video mode */
	.fb_blank = mdss_fb_blank,	/* blank display */
	.fb_pan_display = mdss_fb_pan_display,	/* pan display */
	.fb_ioctl = mdss_fb_ioctl,	/* perform fb specific ioctl */
	.fb_mmap = mdss_fb_mmap,
};

static u32 mdss_fb_line_length(u32 fb_index, u32 xres, int bpp)
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

static int mdss_fb_alloc_fbmem(struct msm_fb_data_type *mfd)
{
	void *virt = NULL;
	unsigned long phys = 0;
	size_t size;
	u32 yres = mfd->fbi->var.yres_virtual;

	size = PAGE_ALIGN(mfd->fbi->fix.line_length * yres);

	if (mfd->index == 0) {
		int dom;
		virt = allocate_contiguous_memory(size, MEMTYPE_EBI1, SZ_1M, 0);
		if (!virt) {
			pr_err("unable to alloc fbmem size=%u\n", size);
			return -ENOMEM;
		}
		phys = memory_pool_node_paddr(virt);
		if (is_mdss_iommu_attached()) {
			dom = mdss_get_iommu_domain(MDSS_IOMMU_DOMAIN_UNSECURE);
			msm_iommu_map_contig_buffer(phys, dom, 0, size, SZ_4K,
						    0, &(mfd->iova));
		}
		pr_info("allocating %u bytes at %p (%lx phys) for fb %d\n",
			size, virt, phys, mfd->index);
	} else {
		pr_debug("no memory allocated for fb%d\n", mfd->index);
		size = 0;
	}

	mfd->fbi->screen_base = virt;
	mfd->fbi->fix.smem_start = phys;
	mfd->fbi->fix.smem_len = size;

	return 0;
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
	if (mfd->split_display)
		var->xres *= 2;

	fix->type = panel_info->is_3d_panel;
	fix->line_length = mdss_fb_line_length(mfd->index, var->xres, bpp);

	var->yres = panel_info->yres;
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

	/* id field for fb app  */

	id = (int *)&mfd->panel;

	snprintf(fix->id, sizeof(fix->id), "mdssfb_%x", (u32) *id);

	fbi->fbops = &mdss_fb_ops;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->pseudo_palette = mdss_fb_pseudo_palette;

	mfd->ref_cnt = 0;
	mfd->panel_power_on = false;

	if (mdss_fb_alloc_fbmem(mfd)) {
		pr_err("unable to allocate framebuffer memory\n");
		return -ENOMEM;
	}

	mfd->op_enable = true;

	mutex_init(&mfd->no_update.lock);
	mutex_init(&mfd->sync_mutex);
	init_timer(&mfd->no_update.timer);
	mfd->no_update.timer.function = mdss_fb_no_update_notify_timer_cb;
	mfd->no_update.timer.data = (unsigned long)mfd;
	init_completion(&mfd->update.comp);
	init_completion(&mfd->no_update.comp);
	init_completion(&mfd->commit_comp);
	init_completion(&mfd->power_set_comp);
	INIT_WORK(&mfd->commit_work, mdss_fb_commit_wq_handler);
	mfd->msm_fb_backup = kzalloc(sizeof(struct msm_fb_backup_type),
		GFP_KERNEL);
	if (mfd->msm_fb_backup == 0) {
		pr_err("error: not enough memory!\n");
		return -ENOMEM;
	}
	if (mfd->lut_update) {
		ret = fb_alloc_cmap(&fbi->cmap, 256, 0);
		if (ret)
			pr_err("fb_alloc_cmap() failed!\n");
	}

	if (register_framebuffer(fbi) < 0) {
		if (mfd->lut_update)
			fb_dealloc_cmap(&fbi->cmap);

		mfd->op_enable = false;
		return -EPERM;
	}

	pr_info("FrameBuffer[%d] %dx%d size=%d registered successfully!\n",
		     mfd->index, fbi->var.xres, fbi->var.yres,
		     fbi->fix.smem_len);

	ret = 0;

	return ret;
}

static int mdss_fb_open(struct fb_info *info, int user)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int result;

	result = pm_runtime_get_sync(info->dev);

	if (result < 0)
		pr_err("pm_runtime: fail to wake up\n");


	if (!mfd->ref_cnt) {
		result = mdss_fb_blank_sub(FB_BLANK_UNBLANK, info,
					   mfd->op_enable);
		if (result) {
			pr_err("mdss_fb_open: can't turn on display!\n");
			return result;
		}
	}

	mfd->ref_cnt++;
	return 0;
}

static int mdss_fb_release(struct fb_info *info, int user)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	int ret = 0;

	if (!mfd->ref_cnt) {
		pr_info("try to close unopened fb %d!\n", mfd->index);
		return -EINVAL;
	}

	mdss_fb_pan_idle(mfd);
	mfd->ref_cnt--;

	if (!mfd->ref_cnt) {
		ret = mdss_fb_blank_sub(FB_BLANK_POWERDOWN, info,
				       mfd->op_enable);
		if (ret) {
			pr_err("can't turn off display!\n");
			return ret;
		}
	}

	pm_runtime_put(info->dev);
	return ret;
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

void mdss_fb_wait_for_fence(struct msm_fb_data_type *mfd)
{
	int i, ret = 0;
	/* buf sync */
	for (i = 0; i < mfd->acq_fen_cnt; i++) {
		ret = sync_fence_wait(mfd->acq_fen[i], WAIT_FENCE_TIMEOUT);
		if (ret < 0) {
			pr_err("%s: sync_fence_wait failed! ret = %x\n",
				__func__, ret);
			break;
		}
		sync_fence_put(mfd->acq_fen[i]);
	}

	if (ret < 0) {
		while (i < mfd->acq_fen_cnt) {
			sync_fence_put(mfd->acq_fen[i]);
			i++;
		}
	}
	mfd->acq_fen_cnt = 0;
}

static void mdss_fb_signal_timeline_locked(struct msm_fb_data_type *mfd)
{
	if (mfd->timeline) {
		sw_sync_timeline_inc(mfd->timeline, 1);
		mfd->timeline_value++;
	}
	mfd->last_rel_fence = mfd->cur_rel_fence;
	mfd->cur_rel_fence = 0;
}

void mdss_fb_signal_timeline(struct msm_fb_data_type *mfd)
{
	mutex_lock(&mfd->sync_mutex);
	mdss_fb_signal_timeline_locked(mfd);
	mutex_unlock(&mfd->sync_mutex);
}

static void mdss_fb_release_fences(struct msm_fb_data_type *mfd)
{
	mutex_lock(&mfd->sync_mutex);
	if (mfd->timeline) {
		sw_sync_timeline_inc(mfd->timeline, 2);
		mfd->timeline_value += 2;
	}
	mfd->last_rel_fence = 0;
	mfd->cur_rel_fence = 0;
	mutex_unlock(&mfd->sync_mutex);
}

static void mdss_fb_pan_idle(struct msm_fb_data_type *mfd)
{
	int ret;

	if (mfd->is_committing) {
		ret = wait_for_completion_timeout(
				&mfd->commit_comp,
			msecs_to_jiffies(WAIT_DISP_OP_TIMEOUT));
		if (ret < 0)
			ret = -ERESTARTSYS;
		else if (!ret)
			pr_err("%s wait for commit_comp timeout %d %d",
				__func__, ret, mfd->is_committing);
		if (ret <= 0) {
			mutex_lock(&mfd->sync_mutex);
			mdss_fb_signal_timeline_locked(mfd);
			mfd->is_committing = 0;
			complete_all(&mfd->commit_comp);
			mutex_unlock(&mfd->sync_mutex);
		}
	}
}

static int mdss_fb_pan_display_ex(struct fb_info *info,
		struct mdp_display_commit *disp_commit)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct msm_fb_backup_type *fb_backup;
	struct fb_var_screeninfo *var = &disp_commit->var;
	u32 wait_for_finish = disp_commit->wait_for_finish;
	int ret = 0;

	if ((!mfd->op_enable) || (!mfd->panel_power_on))
		return -EPERM;

	if (var->xoffset > (info->var.xres_virtual - info->var.xres))
		return -EINVAL;

	if (var->yoffset > (info->var.yres_virtual - info->var.yres))
		return -EINVAL;

	mdss_fb_pan_idle(mfd);

	mutex_lock(&mfd->sync_mutex);
	if (info->fix.xpanstep)
		info->var.xoffset =
		(var->xoffset / info->fix.xpanstep) * info->fix.xpanstep;

	if (info->fix.ypanstep)
		info->var.yoffset =
		(var->yoffset / info->fix.ypanstep) * info->fix.ypanstep;

	fb_backup = (struct msm_fb_backup_type *)mfd->msm_fb_backup;
	memcpy(&fb_backup->info, info, sizeof(struct fb_info));
	memcpy(&fb_backup->disp_commit, disp_commit,
		sizeof(struct mdp_display_commit));
	INIT_COMPLETION(mfd->commit_comp);
	mfd->is_committing = 1;
	schedule_work(&mfd->commit_work);
	mutex_unlock(&mfd->sync_mutex);
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
	return mdss_fb_pan_display_ex(info, &disp_commit);
}

static int mdss_fb_pan_display_sub(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;

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

	mdss_fb_wait_for_fence(mfd);
	if (mfd->dma_fnc)
		mfd->dma_fnc(mfd);
	else
		pr_warn("dma function not set for panel type=%d\n",
				mfd->panel.type);
	mdss_fb_signal_timeline(mfd);
	mdss_fb_update_backlight(mfd);
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

static void mdss_fb_commit_wq_handler(struct work_struct *work)
{
	struct msm_fb_data_type *mfd;
	struct fb_var_screeninfo *var;
	struct fb_info *info;
	struct msm_fb_backup_type *fb_backup;
	int ret;

	mfd = container_of(work, struct msm_fb_data_type, commit_work);
	fb_backup = (struct msm_fb_backup_type *)mfd->msm_fb_backup;
	info = &fb_backup->info;
	if (fb_backup->disp_commit.flags &
		MDP_DISPLAY_COMMIT_OVERLAY) {
		mdss_fb_wait_for_fence(mfd);
		mdss_mdp_overlay_kickoff(mfd->ctl);
		mdss_fb_signal_timeline(mfd);
	} else {
		var = &fb_backup->disp_commit.var;
		ret = mdss_fb_pan_display_sub(var, info);
		if (ret)
			pr_err("%s fails: ret = %x", __func__, ret);
	}
	mutex_lock(&mfd->sync_mutex);
	mfd->is_committing = 0;
	complete_all(&mfd->commit_comp);
	mutex_unlock(&mfd->sync_mutex);
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

	mdss_fb_pan_idle(mfd);
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
		if (var->transp.offset == 24)
			mfd->fb_imgType = MDP_ARGB_8888;
		else
			mfd->fb_imgType	= MDP_RGBA_8888;
		break;

	default:
		return -EINVAL;
	}

	mfd->fbi->fix.line_length = mdss_fb_line_length(mfd->index, var->xres,
						var->bits_per_pixel / 8);

	if (mfd->panel_reconfig || (mfd->fb_imgType != old_imgType)) {
		mdss_fb_blank_sub(FB_BLANK_POWERDOWN, info, mfd->op_enable);
		mdss_fb_var_to_panelinfo(var, mfd->panel_info);
		mdss_fb_blank_sub(FB_BLANK_UNBLANK, info, mfd->op_enable);
		mfd->panel_reconfig = false;
	}

	return 0;
}

static int mdss_fb_cursor(struct fb_info *info, void __user *p)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_cursor cursor;
	int ret;

	if (!mfd->cursor_update)
		return -ENODEV;

	ret = copy_from_user(&cursor, p, sizeof(cursor));
	if (ret)
		return ret;

	return mfd->cursor_update(mfd, &cursor);
}

static int mdss_fb_set_lut(struct fb_info *info, void __user *p)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_cmap cmap;
	int ret;

	if (!mfd->lut_update)
		return -ENODEV;

	ret = copy_from_user(&cmap, p, sizeof(cmap));
	if (ret)
		return ret;

	mfd->lut_update(mfd, &cmap);
	return 0;
}

static int mdss_fb_handle_pp_ioctl(struct msm_fb_data_type *mfd,
							void __user *argp)
{
	int ret;
	struct msmfb_mdp_pp mdp_pp;
	u32 copyback = 0;

	ret = copy_from_user(&mdp_pp, argp, sizeof(mdp_pp));
	if (ret)
		return ret;

	switch (mdp_pp.op) {
	case mdp_op_pa_cfg:
		ret = mdss_mdp_pa_config(&mdp_pp.data.pa_cfg_data,
				&copyback);
		break;

	case mdp_op_pcc_cfg:
		ret = mdss_mdp_pcc_config(&mdp_pp.data.pcc_cfg_data,
			   &copyback);
		break;

	case mdp_op_lut_cfg:
		switch (mdp_pp.data.lut_cfg_data.lut_type) {
		case mdp_lut_igc:
			ret = mdss_mdp_igc_lut_config(
					(struct mdp_igc_lut_data *)
					&mdp_pp.data.lut_cfg_data.data,
					&copyback);
			break;

		case mdp_lut_pgc:
			ret = mdss_mdp_argc_config(
				&mdp_pp.data.lut_cfg_data.data.pgc_lut_data,
				&copyback);
			break;

		case mdp_lut_hist:
			ret = mdss_mdp_hist_lut_config(
				(struct mdp_hist_lut_data *)
				&mdp_pp.data.lut_cfg_data.data, &copyback);
			break;

		default:
			ret = -ENOTSUPP;
			break;
		}
		break;
	case mdp_op_dither_cfg:
		ret = mdss_mdp_dither_config(&mdp_pp.data.dither_cfg_data,
				&copyback);
		break;
	case mdp_op_gamut_cfg:
		ret = mdss_mdp_gamut_config(&mdp_pp.data.gamut_cfg_data,
				&copyback);
		break;
	case mdp_bl_scale_cfg:
		ret = mdss_bl_scale_config(mfd, (struct mdp_bl_scale_data *)
						&mdp_pp.data.bl_scale_data);
		break;
	default:
		pr_err("Unsupported request to MDP_PP IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	if ((ret == 0) && copyback)
		ret = copy_to_user(argp, &mdp_pp, sizeof(struct msmfb_mdp_pp));
	return ret;
}
static int mdss_fb_handle_buf_sync_ioctl(struct msm_fb_data_type *mfd,
						struct mdp_buf_sync *buf_sync)
{
	int i, fence_cnt = 0, ret = 0;
	int acq_fen_fd[MDP_MAX_FENCE_FD];
	struct sync_fence *fence;

	if ((buf_sync->acq_fen_fd_cnt > MDP_MAX_FENCE_FD) ||
		(mfd->timeline == NULL))
		return -EINVAL;

	if ((!mfd->op_enable) || (!mfd->panel_power_on))
		return -EPERM;

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
		mdss_fb_wait_for_fence(mfd);

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
	if (mfd->cur_rel_fen_fd < 0) {
		pr_err("%s: get_unused_fd_flags failed", __func__);
		ret  = -EIO;
		goto buf_sync_err_2;
	}
	sync_fence_install(mfd->cur_rel_fence, mfd->cur_rel_fen_fd);
	ret = copy_to_user(buf_sync->rel_fen_fd,
		&mfd->cur_rel_fen_fd, sizeof(int));
	if (ret) {
		pr_err("%s:copy_to_user failed", __func__);
		goto buf_sync_err_3;
	}
	mutex_unlock(&mfd->sync_mutex);
	return ret;
buf_sync_err_3:
	put_unused_fd(mfd->cur_rel_fen_fd);
buf_sync_err_2:
	sync_fence_put(mfd->cur_rel_fence);
	mfd->cur_rel_fence = NULL;
	mfd->cur_rel_fen_fd = 0;
buf_sync_err_1:
	for (i = 0; i < fence_cnt; i++)
		sync_fence_put(mfd->acq_fen[i]);
	mfd->acq_fen_cnt = 0;
	mutex_unlock(&mfd->sync_mutex);
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

static int mdss_fb_set_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata)
{
	int ret = 0;
	switch (metadata->op) {
	case metadata_op_vic:
		if (mfd->panel_info)
			mfd->panel_info->vic =
				metadata->data.video_info_code;
		else
			ret = -EINVAL;
		break;
	default:
		pr_warn("unsupported request to MDP META IOCTL\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mdss_fb_get_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata)
{
	int ret = 0;
	switch (metadata->op) {
	case metadata_op_frame_rate:
		metadata->data.panel_frame_rate =
			mdss_get_panel_framerate(mfd);
		break;
	default:
		pr_warn("Unsupported request to MDP META IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mdss_fb_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	void __user *argp = (void __user *)arg;
	struct mdp_histogram_data hist;
	struct mdp_histogram_start_req hist_req;
	u32 block, hist_data_addr = 0;
	struct mdp_page_protection fb_page_protection;
	int ret = -ENOSYS;
	struct mdp_buf_sync buf_sync;
	struct msmfb_metadata metadata;

	mdss_fb_power_setting_idle(mfd);

	mdss_fb_pan_idle(mfd);

	switch (cmd) {
	case MSMFB_CURSOR:
		ret = mdss_fb_cursor(info, argp);
		break;

	case MSMFB_SET_LUT:
		ret = mdss_fb_set_lut(info, argp);
		break;

	case MSMFB_HISTOGRAM:
		if (!mfd->panel_power_on)
			return -EPERM;

		ret = copy_from_user(&hist, argp, sizeof(hist));
		if (ret)
			return ret;

		ret = mdss_mdp_hist_collect(info, &hist, &hist_data_addr);
		if ((ret == 0) && hist_data_addr) {
			ret = copy_to_user(hist.c0, (u32 *)hist_data_addr,
				sizeof(u32) * hist.bin_cnt);
			if (ret == 0)
				ret = copy_to_user(argp, &hist,
						   sizeof(hist));
		}
		break;

	case MSMFB_HISTOGRAM_START:
		if (!mfd->panel_power_on)
			return -EPERM;

		ret = copy_from_user(&hist_req, argp, sizeof(hist_req));
		if (ret)
			return ret;

		ret = mdss_mdp_histogram_start(&hist_req);
		break;

	case MSMFB_HISTOGRAM_STOP:
		ret = copy_from_user(&block, argp, sizeof(int));
		if (ret)
			return ret;

		ret = mdss_mdp_histogram_stop(block);
		break;

	case MSMFB_GET_PAGE_PROTECTION:
		fb_page_protection.page_protection =
			mfd->mdp_fb_page_protection;
		ret = copy_to_user(argp, &fb_page_protection,
				   sizeof(fb_page_protection));
		if (ret)
			return ret;
		break;

	case MSMFB_MDP_PP:
		ret = mdss_fb_handle_pp_ioctl(mfd, argp);
		break;

	case MSMFB_BUFFER_SYNC:
		ret = copy_from_user(&buf_sync, argp, sizeof(buf_sync));
		if (ret)
			return ret;

		ret = mdss_fb_handle_buf_sync_ioctl(mfd, &buf_sync);

		if (!ret)
			ret = copy_to_user(argp, &buf_sync, sizeof(buf_sync));
		break;

	case MSMFB_NOTIFY_UPDATE:
		ret = mdss_fb_notify_update(mfd, argp);
		break;

	case MSMFB_DISPLAY_COMMIT:
		ret = mdss_fb_display_commit(info, argp);
		break;

	case MSMFB_METADATA_SET:
		ret = copy_from_user(&metadata, argp, sizeof(metadata));
		if (ret)
			return ret;
		ret = mdss_fb_set_metadata(mfd, &metadata);
		break;

	case MSMFB_METADATA_GET:
		ret = copy_from_user(&metadata, argp, sizeof(metadata));
		if (ret)
			return ret;
		ret = mdss_fb_get_metadata(mfd, &metadata);
		if (!ret)
			ret = copy_to_user(argp, &metadata, sizeof(metadata));
		break;

	default:
		if (mfd->ioctl_handler)
			ret = mfd->ioctl_handler(mfd, cmd, argp);
		break;
	}

	if (ret == -ENOSYS)
		pr_err("unsupported ioctl (%x)\n", cmd);

	return ret;
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

	if ((fb_pdata->panel_info.type != MIPI_VIDEO_PANEL) ||
			(pdata->panel_info.type != MIPI_VIDEO_PANEL)) {
		pr_err("Split panel not supported for panel type %d\n",
				pdata->panel_info.type);
		return -EINVAL;
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

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid device node\n");
		return -ENODEV;
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
	} else {
		pr_info("adding framebuffer device %s\n", dev_name(&pdev->dev));
		fb_pdev = of_platform_device_create(node, NULL,
				&mdss_pdev->dev);
		fb_pdev->dev.platform_data = pdata;
	}

mdss_notfound:
	of_node_put(node);

	return rc;
}
EXPORT_SYMBOL(mdss_register_panel);

int mdss_fb_get_phys_info(unsigned long *start, unsigned long *len, int fb_num)
{
	struct fb_info *info;
	struct msm_fb_data_type *mfd;

	if (fb_num > MAX_FBI_LIST)
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

device_initcall_sync(mdss_fb_init);
