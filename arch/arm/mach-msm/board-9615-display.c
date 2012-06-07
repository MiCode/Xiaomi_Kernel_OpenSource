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
#include <linux/ion.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/msm_bus_board.h>

#include "devices.h"
#include "board-9615.h"

/* prim = 240 x 320 x 4(bpp) x 2(pages) */
#define MSM_FB_PRIM_BUF_SIZE roundup(240 * 320 * 4 * 2, 0x10000)
#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE, 4096)

#define GPIO_PIN_EBI2_LCD_A_D	21
#define GPIO_PIN_EBI2_LCD_CS	22
#define GPIO_PIN_EBI2_LCD_RS	24


#ifdef CONFIG_FB_MSM

static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_MEM,
	}
};

static int msm_fb_detect_panel(const char *name)
{
	return 0;
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

void __init mdm9615_allocate_fb_region(void)
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


static bool ebi2_power_init;
static int ebi2_panel_power(int on)
{
	static struct regulator *panel_power;
	int rc;

	pr_debug("%s: on=%d\n", __func__, on);

	if (!ebi2_power_init) {
		panel_power = regulator_get(&msm_ebi2_lcdc_device.dev,
				"VDDI2");
		if (IS_ERR_OR_NULL(panel_power)) {
			pr_err("could not get L14, rc = %ld\n",
				PTR_ERR(panel_power));
			return -ENODEV;
		}

		rc = regulator_set_voltage(panel_power, 2800000, 3800000);
		if (rc) {
			pr_err("set_voltage L14 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		ebi2_power_init = true;
	}

	if (on) {
		rc = regulator_enable(panel_power);
		if (rc) {
			pr_err("enable L14 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = gpio_request(GPIO_PIN_EBI2_LCD_A_D, "disp_a_d");
		if (rc) {
			pr_err("request gpio EBI2_LCD_A_D failed, rc=%d\n", rc);
			goto error1;
		}
		rc = gpio_request(GPIO_PIN_EBI2_LCD_CS, "disp_cs");
		if (rc) {
			pr_err("request gpio EBI2_LCD_CS failed, rc=%d\n", rc);
			goto error2;
		}
		rc = gpio_request(GPIO_PIN_EBI2_LCD_RS, "disp_rs");
		if (rc) {
			pr_err("request gpio EBI2_LCD_RS failed, rc=%d\n", rc);
			goto error3;
		}
	} else {
		gpio_free(GPIO_PIN_EBI2_LCD_RS);
		gpio_free(GPIO_PIN_EBI2_LCD_CS);
		gpio_free(GPIO_PIN_EBI2_LCD_A_D);

		rc = regulator_disable(panel_power);
		if (rc) {
			pr_err("disable L14 failed, rc=%d\n", rc);
			return -ENODEV;
		}
	}

	return 0;
error3:
	gpio_free(GPIO_PIN_EBI2_LCD_CS);
error2:
	gpio_free(GPIO_PIN_EBI2_LCD_A_D);
error1:
	regulator_disable(panel_power);
	return rc;

}

static struct lcdc_platform_data ebi2_lcdc_pdata = {
	.lcdc_power_save = ebi2_panel_power,
};

static struct lvds_panel_platform_data ebi2_epson_s1d_pdata;

static struct platform_device ebi2_epson_s1d_panel_device = {
	.name = "ebi2_epson_s1d_qvga",
	.id = 0,
	.dev = {
		.platform_data = &ebi2_epson_s1d_pdata,
	}
};

void __init mdm9615_init_fb(void)
{
	platform_device_register(&msm_fb_device);
	platform_device_register(&ebi2_epson_s1d_panel_device);

	msm_fb_register_device("ebi2_lcd", &ebi2_lcdc_pdata);
}
#endif
