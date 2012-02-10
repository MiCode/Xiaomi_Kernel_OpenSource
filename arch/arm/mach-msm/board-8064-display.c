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
#define MSM_FB_PRIM_BUF_SIZE roundup(1376 * 768 * 4 * 3, 0x10000)
#else
/* prim = 1366 x 768 x 3(bpp) x 2(pages) */
#define MSM_FB_PRIM_BUF_SIZE roundup(1376 * 768 * 4 * 2, 0x10000)
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

#define PANEL_NAME_MAX_LEN 30
#define LVDS_CHIMEI_PANEL_NAME "lvds_chimei_wxga"
#define MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME "mipi_video_toshiba_wsvga"
#define MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME "mipi_video_chimei_wxga"
#define HDMI_PANEL_NAME "hdmi_msm"
#define TVOUT_PANEL_NAME "tvout_msm"

static int msm_fb_detect_panel(const char *name)
{
	if (machine_is_apq8064_liquid()) {
		if (!strncmp(name, LVDS_CHIMEI_PANEL_NAME,
			strnlen(LVDS_CHIMEI_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
			return 0;

#if !defined(CONFIG_FB_MSM_LVDS_MIPI_PANEL_DETECT) && \
	!defined(CONFIG_FB_MSM_MIPI_PANEL_DETECT)
		if (!strncmp(name, MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME,
			strnlen(MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
			return 0;
#endif
	} else if (machine_is_apq8064_mtp()) {
		if (!strncmp(name, MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
			strnlen(MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
			return 0;
	} else if (machine_is_apq8064_cdp()) {
		if (!strncmp(name, LVDS_CHIMEI_PANEL_NAME,
			strnlen(LVDS_CHIMEI_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
			return 0;
	}

	if (!strncmp(name, HDMI_PANEL_NAME,
		strnlen(HDMI_PANEL_NAME,
			PANEL_NAME_MAX_LEN)))
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
	200000000,
	200000000,
	200000000,
	200000000,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_core_clk_rate = 200000000,
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

static bool dsi_power_on;
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_lvs7, *reg_l2, *reg_l11, *reg_ext_3p3v;
	static int gpio36, gpio25, gpio26, mpp3;
	int rc;

	pr_debug("%s: on=%d\n", __func__, on);

	if (!dsi_power_on) {
		reg_lvs7 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi1_vddio");
		if (IS_ERR_OR_NULL(reg_lvs7)) {
			pr_err("could not get 8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_lvs7));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi1_pll_vdda");
		if (IS_ERR_OR_NULL(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		reg_l11 = regulator_get(&msm_mipi_dsi1_device.dev,
						"dsi1_avdd");
		if (IS_ERR(reg_l11)) {
				pr_err("could not get 8921_l11, rc = %ld\n",
						PTR_ERR(reg_l11));
				return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l11, 3000000, 3000000);
		if (rc) {
				pr_err("set_voltage l11 failed, rc=%d\n", rc);
				return -EINVAL;
		}

		if (machine_is_apq8064_liquid()) {
			reg_ext_3p3v = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi1_vccs_3p3v");
			if (IS_ERR_OR_NULL(reg_ext_3p3v)) {
				pr_err("could not get reg_ext_3p3v, rc = %ld\n",
					PTR_ERR(reg_ext_3p3v));
				reg_ext_3p3v = NULL;
				return -ENODEV;
			}
			mpp3 = PM8921_MPP_PM_TO_SYS(3);
			rc = gpio_request(mpp3, "backlight_en");
			if (rc) {
				pr_err("request mpp3 failed, rc=%d\n", rc);
				return -ENODEV;
			}
		}

		gpio25 = PM8921_GPIO_PM_TO_SYS(25);
		rc = gpio_request(gpio25, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 25 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio26 = PM8921_GPIO_PM_TO_SYS(26);
		rc = gpio_request(gpio26, "pwm_backlight_ctrl");
		if (rc) {
			pr_err("request gpio 26 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio36 = PM8921_GPIO_PM_TO_SYS(36); /* lcd1_pwr_en_n */
		rc = gpio_request(gpio36, "lcd1_pwr_en_n");
		if (rc) {
			pr_err("request gpio 36 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		dsi_power_on = true;
	}

	if (on) {
		rc = regulator_enable(reg_lvs7);
		if (rc) {
			pr_err("enable lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l11, 110000);
		if (rc < 0) {
			pr_err("set_optimum_mode l11 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l11);
		if (rc) {
			pr_err("enable l11 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		if (machine_is_apq8064_liquid()) {
			rc = regulator_enable(reg_ext_3p3v);
			if (rc) {
				pr_err("enable reg_ext_3p3v failed, rc=%d\n",
					rc);
				return -ENODEV;
			}
			gpio_set_value_cansleep(mpp3, 1);
		}

		gpio_set_value_cansleep(gpio36, 0);
		gpio_set_value_cansleep(gpio25, 1);
	} else {
		gpio_set_value_cansleep(gpio25, 0);
		gpio_set_value_cansleep(gpio36, 1);

		if (machine_is_apq8064_liquid()) {
			gpio_set_value_cansleep(mpp3, 0);

			rc = regulator_disable(reg_ext_3p3v);
			if (rc) {
				pr_err("disable reg_ext_3p3v failed, rc=%d\n",
					rc);
				return -ENODEV;
			}
		}

		rc = regulator_disable(reg_lvs7);
		if (rc) {
			pr_err("disable reg_lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
	}

	return 0;
}

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.dsi_power_save = mipi_dsi_panel_power,
};

static bool lvds_power_on;
static int lvds_panel_power(int on)
{
	static struct regulator *reg_lvs7, *reg_l2, *reg_ext_3p3v;
	static int gpio36, gpio26, mpp3;
	int rc;

	pr_debug("%s: on=%d\n", __func__, on);

	if (!lvds_power_on) {
		reg_lvs7 = regulator_get(&msm_lvds_device.dev,
				"lvds_vdda");
		if (IS_ERR_OR_NULL(reg_lvs7)) {
			pr_err("could not get 8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_lvs7));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_lvds_device.dev,
				"lvds_pll_vdda");
		if (IS_ERR_OR_NULL(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		reg_ext_3p3v = regulator_get(&msm_lvds_device.dev,
			"lvds_vccs_3p3v");
		if (IS_ERR_OR_NULL(reg_ext_3p3v)) {
			pr_err("could not get reg_ext_3p3v, rc = %ld\n",
			       PTR_ERR(reg_ext_3p3v));
		    return -ENODEV;
		}

		gpio26 = PM8921_GPIO_PM_TO_SYS(26);
		rc = gpio_request(gpio26, "pwm_backlight_ctrl");
		if (rc) {
			pr_err("request gpio 26 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio36 = PM8921_GPIO_PM_TO_SYS(36); /* lcd1_pwr_en_n */
		rc = gpio_request(gpio36, "lcd1_pwr_en_n");
		if (rc) {
			pr_err("request gpio 36 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		mpp3 = PM8921_MPP_PM_TO_SYS(3);
		rc = gpio_request(mpp3, "backlight_en");
		if (rc) {
			pr_err("request mpp3 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		lvds_power_on = true;
	}

	if (on) {
		rc = regulator_enable(reg_lvs7);
		if (rc) {
			pr_err("enable lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_enable(reg_ext_3p3v);
		if (rc) {
			pr_err("enable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio_set_value_cansleep(gpio36, 0);
		gpio_set_value_cansleep(mpp3, 1);
	} else {
		gpio_set_value_cansleep(mpp3, 0);
		gpio_set_value_cansleep(gpio36, 1);

		rc = regulator_disable(reg_lvs7);
		if (rc) {
			pr_err("disable reg_lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_ext_3p3v);
		if (rc) {
			pr_err("disable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}
	}

	return 0;
}

static struct lcdc_platform_data lvds_pdata = {
	.lcdc_power_save = lvds_panel_power,
};

#define LPM_CHANNEL 2
static int lvds_chimei_gpio[] = {LPM_CHANNEL};

static struct lvds_panel_platform_data lvds_chimei_pdata = {
	.gpio = lvds_chimei_gpio,
};

static struct platform_device lvds_chimei_panel_device = {
	.name = "lvds_chimei_wxga",
	.id = 0,
	.dev = {
		.platform_data = &lvds_chimei_pdata,
	}
};

static int dsi2lvds_gpio[2] = {
	LPM_CHANNEL,/* Backlight PWM-ID=0 for PMIC-GPIO#24 */
	0x1F08 /* DSI2LVDS Bridge GPIO Output, mask=0x1f, out=0x08 */
};
static struct msm_panel_common_pdata mipi_dsi2lvds_pdata = {
	.gpio_num = dsi2lvds_gpio,
};

static struct platform_device mipi_dsi2lvds_bridge_device = {
	.name = "mipi_tc358764",
	.id = 0,
	.dev.platform_data = &mipi_dsi2lvds_pdata,
};

static int toshiba_gpio[] = {LPM_CHANNEL};
static struct mipi_dsi_panel_platform_data toshiba_pdata = {
	.gpio = toshiba_gpio,
};

static struct platform_device mipi_dsi_toshiba_panel_device = {
	.name = "mipi_toshiba",
	.id = 0,
	.dev = {
			.platform_data = &toshiba_pdata,
	}
};

void __init apq8064_init_fb(void)
{
	platform_device_register(&msm_fb_device);
	platform_device_register(&lvds_chimei_panel_device);

	if (machine_is_apq8064_liquid())
		platform_device_register(&mipi_dsi2lvds_bridge_device);
	if (machine_is_apq8064_mtp())
		platform_device_register(&mipi_dsi_toshiba_panel_device);

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("lvds", &lvds_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
}
