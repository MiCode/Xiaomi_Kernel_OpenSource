/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <linux/ion.h>
#include <mach/ion.h>

#include "devices.h"
#include "board-8064.h"

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
/* prim = 1366 x 768 x 3(bpp) x 3(pages) */
#define MSM_FB_PRIM_BUF_SIZE roundup(1366 * 768 * 3 * 3, 0x10000)
#else
/* prim = 1366 x 768 x 3(bpp) x 2(pages) */
#define MSM_FB_PRIM_BUF_SIZE roundup(1366 * 768 * 3 * 2, 0x10000)
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
/* hdmi = 1920 x 1088 x 2(bpp) x 1(page) */
#define MSM_FB_EXT_BUF_SIZE 0x3FC000
#elif defined(CONFIG_FB_MSM_TVOUT)
/* tvout = 720 x 576 x 2(bpp) x 2(pages) */
#define MSM_FB_EXT_BUF_SIZE 0x195000
#else /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
#define MSM_FB_EXT_BUF_SIZE 0
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE + MSM_FB_EXT_BUF_SIZE, 4096)

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((1376 * 768 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */

static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

#define SIMULATOR_PANAL_NAME "mipi_video_simulator"
#define SIMULATOR_PANAL_NAME_LEN 20
#define LVDS_CHIMEI_PANEL_NAME "lvds_chimei_wxga"
#define LVDS_CHIMEI_PANEL_NAME_LEN 16

static int msm_fb_detect_panel(const char *name)
{
	if (!strncmp(name, LVDS_CHIMEI_PANEL_NAME,
		LVDS_CHIMEI_PANEL_NAME_LEN))
		return 0;
	return -ENODEV;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name              = "msm_fb",
	.id                = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

void __init apq8064_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

	size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));
}

#define MDP_VSYNC_GPIO 0

static int mdp_core_clk_rate_table[] = {
	266667000,
	266667000,
	266667000,
	266667000,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_core_clk_rate = 266667000,
	.mdp_core_clk_table = mdp_core_clk_rate_table,
	.num_mdp_clk = ARRAY_SIZE(mdp_core_clk_rate_table),
	.mdp_rev = MDP_REV_44,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = ION_CP_MM_HEAP_ID,
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
};

void __init apq8064_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}

void __init apq8064_init_fb(void)
{
	platform_device_register(&msm_fb_device);
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("lvds", NULL);
}
