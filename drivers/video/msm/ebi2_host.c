/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/clk.h>
#include <mach/hardware.h>
#include "msm_fb.h"

struct mdp_ccs mdp_ccs_rgb2yuv;
struct mdp_ccs mdp_ccs_yuv2rgb;

static struct platform_device *pdev_list[MSM_FB_MAX_DEV_LIST];
static int pdev_list_cnt;
static int ebi2_host_resource_initialized;
static struct msm_panel_common_pdata *ebi2_host_pdata;

static int ebi2_host_probe(struct platform_device *pdev);
static int ebi2_host_remove(struct platform_device *pdev);

static int ebi2_host_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int ebi2_host_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops ebi2_host_dev_pm_ops = {
	.runtime_suspend = ebi2_host_runtime_suspend,
	.runtime_resume = ebi2_host_runtime_resume,
};


static struct platform_driver ebi2_host_driver = {
	.probe = ebi2_host_probe,
	.remove = ebi2_host_remove,
	.shutdown = NULL,
	.driver = {
		/*
		 * Simulate mdp hw
		 */
		.name = "mdp",
		.pm = &ebi2_host_dev_pm_ops,
	},
};

void mdp_pipe_ctrl(MDP_BLOCK_TYPE block, MDP_BLOCK_POWER_STATE state,
		   boolean isr)
{
	return;
}
int mdp_ppp_blit(struct fb_info *info, struct mdp_blit_req *req)
{
	return 0;
}
int mdp_start_histogram(struct fb_info *info)
{
	return 0;
}
int mdp_stop_histogram(struct fb_info *info)
{
	return 0;
}
void mdp_refresh_screen(unsigned long data)
{
	return;
}

static int ebi2_host_off(struct platform_device *pdev)
{
	int ret;
	ret = panel_next_off(pdev);
	return ret;
}

static int ebi2_host_on(struct platform_device *pdev)
{
	int ret;
	ret = panel_next_on(pdev);
	return ret;
}


static int ebi2_host_probe(struct platform_device *pdev)
{
	struct platform_device *msm_fb_dev = NULL;
	struct msm_fb_data_type *mfd;
	struct msm_fb_panel_data *pdata = NULL;
	int rc;

	if ((pdev->id == 0) && (pdev->num_resources > 0)) {

		ebi2_host_pdata = pdev->dev.platform_data;

		ebi2_host_resource_initialized = 1;
		return 0;
	}

	ebi2_host_resource_initialized = 1;
	if (!ebi2_host_resource_initialized)
		return -EPERM;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (pdev_list_cnt >= MSM_FB_MAX_DEV_LIST)
		return -ENOMEM;

	msm_fb_dev = platform_device_alloc("msm_fb", pdev->id);
	if (!msm_fb_dev)
		return -ENOMEM;

	/* link to the latest pdev */
	mfd->pdev = msm_fb_dev;

	if (ebi2_host_pdata) {
		mfd->mdp_rev = ebi2_host_pdata->mdp_rev;
		mfd->mem_hid = ebi2_host_pdata->mem_hid;
	}

	/* add panel data */
	if (platform_device_add_data
	    (msm_fb_dev, pdev->dev.platform_data,
	     sizeof(struct msm_fb_panel_data))) {
		pr_err("ebi2_host_probe: platform_device_add_data failed!\n");
		rc = -ENOMEM;
		goto ebi2_host_probe_err;
	}
	/* data chain */
	pdata = msm_fb_dev->dev.platform_data;
	pdata->on = ebi2_host_on;
	pdata->off = ebi2_host_off;
	pdata->next = pdev;

	/* set driver data */
	platform_set_drvdata(msm_fb_dev, mfd);

	rc = platform_device_add(msm_fb_dev);
	if (rc)
		goto ebi2_host_probe_err;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	pdev_list[pdev_list_cnt++] = pdev;
	return 0;

ebi2_host_probe_err:
	platform_device_put(msm_fb_dev);
	return rc;
}

void mdp_set_dma_pan_info(struct fb_info *info, struct mdp_dirty_region *dirty,
			  boolean sync)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct fb_info *fbi = mfd->fbi;
	struct msm_panel_info *panel_info = &mfd->panel_info;
	MDPIBUF *iBuf;
	int bpp = info->var.bits_per_pixel / 8;
	int yres, remainder;

	if (panel_info->mode2_yres != 0) {
		yres = panel_info->mode2_yres;
		remainder = (fbi->fix.line_length*yres)%PAGE_SIZE;
	} else {
		yres = panel_info->yres;
		remainder = (fbi->fix.line_length*yres)%PAGE_SIZE;
	}

	if (!remainder)
		remainder = PAGE_SIZE;

	down(&mfd->sem);

	iBuf = &mfd->ibuf;
	/* use virtual address */
	iBuf->buf = (uint8 *) fbi->screen_base;

	if (fbi->var.yoffset < yres) {
		iBuf->buf += fbi->var.xoffset * bpp;
	} else if (fbi->var.yoffset >= yres && fbi->var.yoffset < 2 * yres) {
		iBuf->buf += fbi->var.xoffset * bpp + yres *
		fbi->fix.line_length + PAGE_SIZE - remainder;
	} else {
		iBuf->buf += fbi->var.xoffset * bpp + 2 * yres *
		fbi->fix.line_length + 2 * (PAGE_SIZE - remainder);
	}

	iBuf->ibuf_width = info->var.xres_virtual;
	iBuf->bpp = bpp;

	iBuf->vsync_enable = sync;

	if (dirty) {
		/*
		 * ToDo: dirty region check inside var.xoffset+xres
		 * <-> var.yoffset+yres
		 */
		iBuf->dma_x = dirty->xoffset % info->var.xres;
		iBuf->dma_y = dirty->yoffset % info->var.yres;
		iBuf->dma_w = dirty->width;
		iBuf->dma_h = dirty->height;
	} else {
		iBuf->dma_x = 0;
		iBuf->dma_y = 0;
		iBuf->dma_w = info->var.xres;
		iBuf->dma_h = info->var.yres;
	}
	mfd->ibuf_flushed = FALSE;
	up(&mfd->sem);
}

void mdp_dma_pan_update(struct fb_info *info)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	MDPIBUF *iBuf;
	int i, j;
	uint32 data;
	uint8 *src;
	struct msm_fb_panel_data *pdata =
	    (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;
	struct fb_info *fbi = mfd->fbi;

	iBuf = &mfd->ibuf;

	invalidate_caches((unsigned long)fbi->screen_base,
		(unsigned long)info->fix.smem_len,
		(unsigned long)info->fix.smem_start);

	pdata->set_rect(iBuf->dma_x, iBuf->dma_y, iBuf->dma_w,
			iBuf->dma_h);
	for (i = 0; i < iBuf->dma_h; i++) {
		src = iBuf->buf + (fbi->fix.line_length * (iBuf->dma_y + i))
			+ (iBuf->dma_x * iBuf->bpp);
		for (j = 0; j < iBuf->dma_w; j++) {
			data = (uint32)(*src++ >> 2) << 12;
			data |= (uint32)(*src++ >> 2) << 6;
			data |= (uint32)(*src++ >> 2);
			data = ((data&0x1FF)<<16) | ((data&0x3FE00)>>9);
			outpdw(mfd->data_port, data);
		}
	}
}

static int ebi2_host_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static int ebi2_host_register_driver(void)
{
	return platform_driver_register(&ebi2_host_driver);
}

static int __init ebi2_host_driver_init(void)
{
	int ret;

	ret = ebi2_host_register_driver();
	if (ret) {
		pr_err("ebi2_host_register_driver() failed!\n");
		return ret;
	}

	return 0;
}

module_init(ebi2_host_driver_init);
