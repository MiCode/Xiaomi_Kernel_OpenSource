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
#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include <mach/rpc_pmapp.h>
#include "devices.h"
#include "board-msm7627a.h"

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_SIZE	     0x261000
#define MSM7x25A_MSM_FB_SIZE    0xE1000
#else
#define MSM_FB_SIZE	     0x196000
#define MSM7x25A_MSM_FB_SIZE    0x96000
#endif

static unsigned fb_size = MSM_FB_SIZE;
static int __init fb_size_setup(char *p)
{
	fb_size = memparse(p, NULL);
	return 0;
}

early_param("fb_size", fb_size_setup);

static struct regulator_bulk_data regs_lcdc[] = {
	{ .supply = "gp2",   .min_uV = 2850000, .max_uV = 2850000 },
	{ .supply = "msme1", .min_uV = 1800000, .max_uV = 1800000 },
};
static uint32_t lcdc_gpio_initialized;

static void lcdc_toshiba_gpio_init(void)
{
	int rc = 0;
	if (!lcdc_gpio_initialized) {
		if (gpio_request(GPIO_SPI_CLK, "spi_clk")) {
			pr_err("failed to request gpio spi_clk\n");
			return;
		}
		if (gpio_request(GPIO_SPI_CS0_N, "spi_cs")) {
			pr_err("failed to request gpio spi_cs0_N\n");
			goto fail_gpio6;
		}
		if (gpio_request(GPIO_SPI_MOSI, "spi_mosi")) {
			pr_err("failed to request gpio spi_mosi\n");
			goto fail_gpio5;
		}
		if (gpio_request(GPIO_SPI_MISO, "spi_miso")) {
			pr_err("failed to request gpio spi_miso\n");
			goto fail_gpio4;
		}
		if (gpio_request(GPIO_DISPLAY_PWR_EN, "gpio_disp_pwr")) {
			pr_err("failed to request gpio_disp_pwr\n");
			goto fail_gpio3;
		}
		if (gpio_request(GPIO_BACKLIGHT_EN, "gpio_bkl_en")) {
			pr_err("failed to request gpio_bkl_en\n");
			goto fail_gpio2;
		}
		pmapp_disp_backlight_init();

		rc = regulator_bulk_get(NULL, ARRAY_SIZE(regs_lcdc),
					regs_lcdc);
		if (rc) {
			pr_err("%s: could not get regulators: %d\n",
					__func__, rc);
			goto fail_gpio1;
		}

		rc = regulator_bulk_set_voltage(ARRAY_SIZE(regs_lcdc),
				regs_lcdc);
		if (rc) {
			pr_err("%s: could not set voltages: %d\n",
					__func__, rc);
			goto fail_vreg;
		}
		lcdc_gpio_initialized = 1;
	}
	return;
fail_vreg:
	regulator_bulk_free(ARRAY_SIZE(regs_lcdc), regs_lcdc);
fail_gpio1:
	gpio_free(GPIO_BACKLIGHT_EN);
fail_gpio2:
	gpio_free(GPIO_DISPLAY_PWR_EN);
fail_gpio3:
	gpio_free(GPIO_SPI_MISO);
fail_gpio4:
	gpio_free(GPIO_SPI_MOSI);
fail_gpio5:
	gpio_free(GPIO_SPI_CS0_N);
fail_gpio6:
	gpio_free(GPIO_SPI_CLK);
	lcdc_gpio_initialized = 0;
}

static uint32_t lcdc_gpio_table[] = {
	GPIO_SPI_CLK,
	GPIO_SPI_CS0_N,
	GPIO_SPI_MOSI,
	GPIO_DISPLAY_PWR_EN,
	GPIO_BACKLIGHT_EN,
	GPIO_SPI_MISO,
};

static void config_lcdc_gpio_table(uint32_t *table, int len, unsigned enable)
{
	int n;

	if (lcdc_gpio_initialized) {
		/* All are IO Expander GPIOs */
		for (n = 0; n < (len - 1); n++)
			gpio_direction_output(table[n], 1);
	}
}

static void lcdc_toshiba_config_gpios(int enable)
{
	config_lcdc_gpio_table(lcdc_gpio_table,
		ARRAY_SIZE(lcdc_gpio_table), enable);
}

static int msm_fb_lcdc_power_save(int on)
{
	int rc = 0;
	/* Doing the init of the LCDC GPIOs very late as they are from
		an I2C-controlled IO Expander */
	lcdc_toshiba_gpio_init();

	if (lcdc_gpio_initialized) {
		gpio_set_value_cansleep(GPIO_DISPLAY_PWR_EN, on);
		gpio_set_value_cansleep(GPIO_BACKLIGHT_EN, on);

		rc = on ? regulator_bulk_enable(
				ARRAY_SIZE(regs_lcdc), regs_lcdc) :
			  regulator_bulk_disable(
				ARRAY_SIZE(regs_lcdc), regs_lcdc);

		if (rc)
			pr_err("%s: could not %sable regulators: %d\n",
					__func__, on ? "en" : "dis", rc);
	}

	return rc;
}

static int lcdc_toshiba_set_bl(int level)
{
	int ret;

	ret = pmapp_disp_backlight_set_brightness(level);
	if (ret)
		pr_err("%s: can't set lcd backlight!\n", __func__);

	return ret;
}

static struct lcdc_platform_data lcdc_pdata = {
	.lcdc_gpio_config = NULL,
	.lcdc_power_save   = msm_fb_lcdc_power_save,
};

static int lcd_panel_spi_gpio_num[] = {
		GPIO_SPI_MOSI,  /* spi_sdi */
		GPIO_SPI_MISO,  /* spi_sdoi */
		GPIO_SPI_CLK,   /* spi_clk */
		GPIO_SPI_CS0_N, /* spi_cs  */
};

static struct msm_panel_common_pdata lcdc_toshiba_panel_data = {
	.panel_config_gpio = lcdc_toshiba_config_gpios,
	.pmic_backlight = lcdc_toshiba_set_bl,
	.gpio_num	 = lcd_panel_spi_gpio_num,
};

static struct platform_device lcdc_toshiba_panel_device = {
	.name   = "lcdc_toshiba_fwvga_pt",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_toshiba_panel_data,
	}
};

static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

#define PANEL_NAME_MAX_LEN      30
#define LCDC_TOSHIBA_FWVGA_PANEL_NAME   "lcdc_toshiba_fwvga_pt"
#define MIPI_CMD_RENESAS_FWVGA_PANEL_NAME       "mipi_cmd_renesas_fwvga"

static int msm_fb_detect_panel(const char *name)
{
	int ret = -ENODEV;

	if (machine_is_msm7x27a_surf() || machine_is_msm7625a_surf()) {
		if (!strncmp(name, "lcdc_toshiba_fwvga_pt", 21) ||
				!strncmp(name, "mipi_cmd_renesas_fwvga", 22))
			ret = 0;
	} else if (machine_is_msm7x27a_ffa() || machine_is_msm7625a_ffa()) {
		if (!strncmp(name, "mipi_cmd_renesas_fwvga", 22))
			ret = 0;
	} else if (machine_is_msm7627a_qrd1()) {
		if (!strncmp(name, "mipi_video_truly_wvga", 21))
			ret = 0;
	} else if (machine_is_msm7627a_evb()) {
		if (!strncmp(name, "mipi_cmd_nt35510_wvga", 21))
			ret = 0;
	}

#if !defined(CONFIG_FB_MSM_LCDC_AUTO_DETECT) && \
	!defined(CONFIG_FB_MSM_MIPI_PANEL_AUTO_DETECT) && \
	!defined(CONFIG_FB_MSM_LCDC_MIPI_PANEL_AUTO_DETECT)
		if (machine_is_msm7x27a_surf() ||
			machine_is_msm7625a_surf()) {
			if (!strncmp(name, LCDC_TOSHIBA_FWVGA_PANEL_NAME,
				strnlen(LCDC_TOSHIBA_FWVGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
				return 0;
		}
#endif

	return ret;
}

static int mipi_truly_set_bl(int on)
{
	gpio_set_value_cansleep(QRD_GPIO_BACKLIGHT_EN, !!on);

	return 1;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

#ifdef CONFIG_FB_MSM_MIPI_DSI
static int mipi_renesas_set_bl(int level)
{
	int ret;

	ret = pmapp_disp_backlight_set_brightness(level);

	if (ret)
		pr_err("%s: can't set lcd backlight!\n", __func__);

	return ret;
}

static struct msm_panel_common_pdata mipi_renesas_pdata = {
	.pmic_backlight = mipi_renesas_set_bl,
};


static struct platform_device mipi_dsi_renesas_panel_device = {
	.name = "mipi_renesas",
	.id = 0,
	.dev    = {
		.platform_data = &mipi_renesas_pdata,
	}
};
#endif

static struct msm_panel_common_pdata mipi_truly_pdata = {
	.pmic_backlight = mipi_truly_set_bl,
};

static struct platform_device mipi_dsi_truly_panel_device = {
	.name   = "mipi_truly",
	.id     = 0,
	.dev    = {
		.platform_data = &mipi_truly_pdata,
	}
};

static struct msm_panel_common_pdata mipi_NT35510_pdata = {
	.pmic_backlight = NULL,/*mipi_NT35510_set_bl,*/
};

static struct platform_device mipi_dsi_NT35510_panel_device = {
	.name = "mipi_NT35510",
	.id   = 0,
	.dev  = {
		.platform_data = &mipi_NT35510_pdata,
	}
};

static struct platform_device *msm_fb_devices[] __initdata = {
	&msm_fb_device,
	&lcdc_toshiba_panel_device,
#ifdef CONFIG_FB_MSM_MIPI_DSI
	&mipi_dsi_renesas_panel_device,
#endif
};

static struct platform_device *qrd_fb_devices[] __initdata = {
	&msm_fb_device,
	&mipi_dsi_truly_panel_device,
};

static struct platform_device *evb_fb_devices[] __initdata = {
	&msm_fb_device,
	&mipi_dsi_NT35510_panel_device,
};

void __init msm_msm7627a_allocate_memory_regions(void)
{
	void *addr;
	unsigned long fb_size;

	if (machine_is_msm7625a_surf() || machine_is_msm7625a_ffa())
		fb_size = MSM7x25A_MSM_FB_SIZE;
	else
		fb_size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(fb_size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + fb_size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n", fb_size,
						addr, __pa(addr));
}

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
	.mdp_rev = MDP_REV_303,
};

#define GPIO_LCDC_BRDG_PD	128
#define GPIO_LCDC_BRDG_RESET_N	129
#define GPIO_LCD_DSI_SEL	125
#define LCDC_RESET_PHYS		0x90008014

static  void __iomem *lcdc_reset_ptr;

static unsigned mipi_dsi_gpio[] = {
		GPIO_CFG(GPIO_LCDC_BRDG_RESET_N, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), /* LCDC_BRDG_RESET_N */
		GPIO_CFG(GPIO_LCDC_BRDG_PD, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), /* LCDC_BRDG_PD */
};

static unsigned lcd_dsi_sel_gpio[] = {
	GPIO_CFG(GPIO_LCD_DSI_SEL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
			GPIO_CFG_2MA),
};

enum {
	DSI_SINGLE_LANE = 1,
	DSI_TWO_LANES,
};

static int msm_fb_get_lane_config(void)
{
	/* For MSM7627A SURF/FFA and QRD */
	int rc = DSI_TWO_LANES;
	if (machine_is_msm7625a_surf() || machine_is_msm7625a_ffa()) {
		rc = DSI_SINGLE_LANE;
		pr_info("DSI_SINGLE_LANES\n");
	} else {
		pr_info("DSI_TWO_LANES\n");
	}
	return rc;
}

static int msm_fb_dsi_client_msm_reset(void)
{
	int rc = 0;

	rc = gpio_request(GPIO_LCDC_BRDG_RESET_N, "lcdc_brdg_reset_n");
	if (rc < 0) {
		pr_err("failed to request lcd brdg reset_n\n");
		return rc;
	}

	rc = gpio_request(GPIO_LCDC_BRDG_PD, "lcdc_brdg_pd");
	if (rc < 0) {
		pr_err("failed to request lcd brdg pd\n");
		return rc;
	}

	rc = gpio_tlmm_config(mipi_dsi_gpio[0], GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("Failed to enable LCDC Bridge reset enable\n");
		goto gpio_error;
	}

	rc = gpio_tlmm_config(mipi_dsi_gpio[1], GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("Failed to enable LCDC Bridge pd enable\n");
		goto gpio_error2;
	}

	rc = gpio_direction_output(GPIO_LCDC_BRDG_RESET_N, 1);
	rc |= gpio_direction_output(GPIO_LCDC_BRDG_PD, 1);
	gpio_set_value_cansleep(GPIO_LCDC_BRDG_PD, 0);

	if (!rc) {
		if (machine_is_msm7x27a_surf() || machine_is_msm7625a_surf()) {
			lcdc_reset_ptr = ioremap_nocache(LCDC_RESET_PHYS,
				sizeof(uint32_t));

			if (!lcdc_reset_ptr)
				return 0;
		}
		return rc;
	} else {
		goto gpio_error;
	}

gpio_error2:
	pr_err("Failed GPIO bridge pd\n");
	gpio_free(GPIO_LCDC_BRDG_PD);

gpio_error:
	pr_err("Failed GPIO bridge reset\n");
	gpio_free(GPIO_LCDC_BRDG_RESET_N);
	return rc;
}

static int mipi_truly_sel_mode(int video_mode)
{
	int rc = 0;

	rc = gpio_request(GPIO_LCD_DSI_SEL, "lcd_dsi_sel");
	if (rc < 0)
		goto gpio_error;

	rc = gpio_tlmm_config(lcd_dsi_sel_gpio[0], GPIO_CFG_ENABLE);
	if (rc)
		goto gpio_error;

	rc = gpio_direction_output(GPIO_LCD_DSI_SEL, 1);
	if (!rc) {
		gpio_set_value_cansleep(GPIO_LCD_DSI_SEL, video_mode);
		return rc;
	} else {
		goto gpio_error;
	}

gpio_error:
	pr_err("mipi_truly_sel_mode failed\n");
	gpio_free(GPIO_LCD_DSI_SEL);
	return rc;
}

static int msm_fb_dsi_client_qrd1_reset(void)
{
	int rc = 0;

	rc = gpio_request(GPIO_LCDC_BRDG_RESET_N, "lcdc_brdg_reset_n");
	if (rc < 0) {
		pr_err("failed to request lcd brdg reset_n\n");
		return rc;
	}

	rc = gpio_tlmm_config(mipi_dsi_gpio[0], GPIO_CFG_ENABLE);
	if (rc < 0) {
		pr_err("Failed to enable LCDC Bridge reset enable\n");
		return rc;
	}

	rc = gpio_direction_output(GPIO_LCDC_BRDG_RESET_N, 1);
	if (rc < 0) {
		pr_err("Failed GPIO bridge pd\n");
		gpio_free(GPIO_LCDC_BRDG_RESET_N);
		return rc;
	}

	mipi_truly_sel_mode(1);

	return rc;
}

#define GPIO_QRD3_LCD_BRDG_RESET_N	85
#define GPIO_QRD3_LCD_BACKLIGHT_EN	96
#define GPIO_QRD3_LCD_EXT_2V85_EN	35
#define GPIO_QRD3_LCD_EXT_1V8_EN	40

static unsigned qrd3_mipi_dsi_gpio[] = {
	GPIO_CFG(GPIO_QRD3_LCD_BRDG_RESET_N, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), /* GPIO_QRD3_LCD_BRDG_RESET_N */
	GPIO_CFG(GPIO_QRD3_LCD_BACKLIGHT_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), /* GPIO_QRD3_LCD_BACKLIGHT_EN */
	GPIO_CFG(GPIO_QRD3_LCD_EXT_2V85_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), /* GPIO_QRD3_LCD_EXT_2V85_EN */
	GPIO_CFG(GPIO_QRD3_LCD_EXT_1V8_EN, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), /* GPIO_QRD3_LCD_EXT_1V8_EN */
};

static int msm_fb_dsi_client_qrd3_reset(void)
{
	int rc = 0;

	rc = gpio_request(GPIO_QRD3_LCD_BRDG_RESET_N, "qrd3_lcd_brdg_reset_n");
	if (rc < 0) {
		pr_err("failed to request qrd3 lcd brdg reset_n\n");
		return rc;
	}

	rc = gpio_tlmm_config(qrd3_mipi_dsi_gpio[0], GPIO_CFG_ENABLE);
	if (rc < 0) {
		pr_err("Failed to enable LCD Bridge reset enable\n");
		return rc;
	}

	rc = gpio_direction_output(GPIO_QRD3_LCD_BRDG_RESET_N, 1);
	if (rc < 0) {
		pr_err("Failed GPIO bridge Reset\n");
		gpio_free(GPIO_QRD3_LCD_BRDG_RESET_N);
		return rc;
	}

	return rc;
}

static int msm_fb_dsi_client_reset(void)
{
	int rc = 0;

	if (machine_is_msm7627a_qrd1())
		rc = msm_fb_dsi_client_qrd1_reset();
	else if (machine_is_msm7627a_evb())
		rc = msm_fb_dsi_client_qrd3_reset();
	else
		rc = msm_fb_dsi_client_msm_reset();

	return rc;

}

static struct regulator_bulk_data regs_dsi[] = {
	{ .supply = "gp2",   .min_uV = 2850000, .max_uV = 2850000 },
	{ .supply = "msme1", .min_uV = 1800000, .max_uV = 1800000 },
};

static int dsi_gpio_initialized;

static int mipi_dsi_panel_msm_power(int on)
{
	int rc = 0;
	uint32_t lcdc_reset_cfg;

	/* I2C-controlled GPIO Expander -init of the GPIOs very late */
	if (unlikely(!dsi_gpio_initialized)) {
		pmapp_disp_backlight_init();

		rc = gpio_request(GPIO_DISPLAY_PWR_EN, "gpio_disp_pwr");
		if (rc < 0) {
			pr_err("failed to request gpio_disp_pwr\n");
			return rc;
		}

		if (machine_is_msm7x27a_surf() || machine_is_msm7625a_surf()) {
			rc = gpio_direction_output(GPIO_DISPLAY_PWR_EN, 1);
			if (rc < 0) {
				pr_err("failed to enable display pwr\n");
				goto fail_gpio1;
			}

			rc = gpio_request(GPIO_BACKLIGHT_EN, "gpio_bkl_en");
			if (rc < 0) {
				pr_err("failed to request gpio_bkl_en\n");
				goto fail_gpio1;
			}

			rc = gpio_direction_output(GPIO_BACKLIGHT_EN, 1);
			if (rc < 0) {
				pr_err("failed to enable backlight\n");
				goto fail_gpio2;
			}
		}

		rc = regulator_bulk_get(NULL, ARRAY_SIZE(regs_dsi), regs_dsi);
		if (rc) {
			pr_err("%s: could not get regulators: %d\n",
					__func__, rc);
			goto fail_gpio2;
		}

		rc = regulator_bulk_set_voltage(ARRAY_SIZE(regs_dsi),
						regs_dsi);
		if (rc) {
			pr_err("%s: could not set voltages: %d\n",
					__func__, rc);
			goto fail_vreg;
		}
		if (pmapp_disp_backlight_set_brightness(100))
			pr_err("backlight set brightness failed\n");

		dsi_gpio_initialized = 1;
	}
	if (machine_is_msm7x27a_surf() || machine_is_msm7625a_surf()) {
		gpio_set_value_cansleep(GPIO_DISPLAY_PWR_EN, on);
		gpio_set_value_cansleep(GPIO_BACKLIGHT_EN, on);
	} else if (machine_is_msm7x27a_ffa() ||
			 machine_is_msm7625a_ffa()) {
		if (on) {
			/* This line drives an active low pin on FFA */
			rc = gpio_direction_output(GPIO_DISPLAY_PWR_EN, !on);
			if (rc < 0)
				pr_err("failed to set direction for "
					"display pwr\n");
		} else {
			gpio_set_value_cansleep(GPIO_DISPLAY_PWR_EN, !on);
			rc = gpio_direction_input(GPIO_DISPLAY_PWR_EN);
			if (rc < 0)
				pr_err("failed to set direction for "
					"display pwr\n");
		}
	}

	if (on) {
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_PD, 0);

		if (machine_is_msm7x27a_surf() ||
				 machine_is_msm7625a_surf()) {
			lcdc_reset_cfg = readl_relaxed(lcdc_reset_ptr);
			rmb();
			lcdc_reset_cfg &= ~1;

			writel_relaxed(lcdc_reset_cfg, lcdc_reset_ptr);
			msleep(20);
			wmb();
			lcdc_reset_cfg |= 1;
			writel_relaxed(lcdc_reset_cfg, lcdc_reset_ptr);
		} else {
			gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 0);
			msleep(20);
			gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 1);
		}
	} else {
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_PD, 1);
	}

	rc = on ? regulator_bulk_enable(ARRAY_SIZE(regs_dsi), regs_dsi) :
		  regulator_bulk_disable(ARRAY_SIZE(regs_dsi), regs_dsi);

	if (rc)
		pr_err("%s: could not %sable regulators: %d\n",
				__func__, on ? "en" : "dis", rc);

	return rc;
fail_vreg:
	regulator_bulk_free(ARRAY_SIZE(regs_dsi), regs_dsi);
fail_gpio2:
	gpio_free(GPIO_BACKLIGHT_EN);
fail_gpio1:
	gpio_free(GPIO_DISPLAY_PWR_EN);
	dsi_gpio_initialized = 0;
	return rc;
}

static int mipi_dsi_panel_qrd1_power(int on)
{
	int rc = 0;

	if (!dsi_gpio_initialized) {
		rc = gpio_request(QRD_GPIO_BACKLIGHT_EN, "gpio_bkl_en");
		if (rc < 0)
			return rc;

		rc = gpio_tlmm_config(GPIO_CFG(QRD_GPIO_BACKLIGHT_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		if (rc < 0) {
			pr_err("failed GPIO_BACKLIGHT_EN tlmm config\n");
			return rc;
		}

		rc = gpio_direction_output(QRD_GPIO_BACKLIGHT_EN, 1);
		if (rc < 0) {
			pr_err("failed to enable backlight\n");
			gpio_free(QRD_GPIO_BACKLIGHT_EN);
			return rc;
		}
		dsi_gpio_initialized = 1;
	}

	gpio_set_value_cansleep(QRD_GPIO_BACKLIGHT_EN, !!on);

	if (on) {
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 1);
		msleep(20);
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 0);
		msleep(20);
		gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N, 1);

	}

	return rc;
}

static int qrd3_dsi_gpio_initialized;

static int mipi_dsi_panel_qrd3_power(int on)
{
	int rc = 0;

	if (!qrd3_dsi_gpio_initialized) {
		rc = gpio_request(GPIO_QRD3_LCD_BACKLIGHT_EN,
			"qrd3_gpio_bkl_en");
		if (rc < 0)
			return rc;

		rc = gpio_tlmm_config(GPIO_CFG(GPIO_QRD3_LCD_BACKLIGHT_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		if (rc < 0) {
			pr_err("failed QRD3 GPIO_BACKLIGHT_EN tlmm config\n");
			return rc;
		}
		rc = gpio_direction_output(GPIO_QRD3_LCD_BACKLIGHT_EN, 1);
		if (rc < 0) {
			pr_err("failed to enable backlight\n");
			gpio_free(GPIO_QRD3_LCD_BACKLIGHT_EN);
			return rc;
		}

		rc = gpio_request(GPIO_QRD3_LCD_EXT_2V85_EN,
			"qrd3_gpio_ext_2v85_en");
		if (rc < 0)
			return rc;

		rc = gpio_tlmm_config(GPIO_CFG(GPIO_QRD3_LCD_EXT_2V85_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		if (rc < 0) {
			pr_err("failed QRD3 GPIO_QRD3_LCD_EXT_2V85_EN tlmm config\n");
			return rc;
		}

		rc = gpio_direction_output(GPIO_QRD3_LCD_EXT_2V85_EN, 1);
		if (rc < 0) {
			pr_err("failed to enable external 2V85\n");
			gpio_free(GPIO_QRD3_LCD_EXT_2V85_EN);
			return rc;
		}

		rc = gpio_request(GPIO_QRD3_LCD_EXT_1V8_EN,
			"qrd3_gpio_ext_1v8_en");
		if (rc < 0)
			return rc;

		rc = gpio_tlmm_config(GPIO_CFG(GPIO_QRD3_LCD_EXT_1V8_EN, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		if (rc < 0) {
			pr_err("failed QRD3 GPIO_QRD3_LCD_EXT_1V8_EN tlmm config\n");
			return rc;
		}

		rc = gpio_direction_output(GPIO_QRD3_LCD_EXT_1V8_EN, 1);
		if (rc < 0) {
			pr_err("failed to enable external 1v8\n");
			gpio_free(GPIO_QRD3_LCD_EXT_1V8_EN);
			return rc;
		}

			qrd3_dsi_gpio_initialized = 1;
	}

	gpio_set_value_cansleep(GPIO_QRD3_LCD_BACKLIGHT_EN, !!on);
	gpio_set_value_cansleep(GPIO_QRD3_LCD_EXT_2V85_EN, !!on);
	gpio_set_value_cansleep(GPIO_QRD3_LCD_EXT_1V8_EN, !!on);

	if (on) {
		gpio_set_value_cansleep(GPIO_QRD3_LCD_BRDG_RESET_N, 1);
		msleep(20);
		gpio_set_value_cansleep(GPIO_QRD3_LCD_BRDG_RESET_N, 0);
		msleep(20);
		gpio_set_value_cansleep(GPIO_QRD3_LCD_BRDG_RESET_N, 1);
		msleep(20);
	}

		return rc;
}

static int mipi_dsi_panel_power(int on)
{
	int rc = 0;

	if (machine_is_msm7627a_qrd1())
		rc = mipi_dsi_panel_qrd1_power(on);
	else if (machine_is_msm7627a_evb())
		rc = mipi_dsi_panel_qrd3_power(on);
	else
		rc = mipi_dsi_panel_msm_power(on);
	return rc;
}

#define MDP_303_VSYNC_GPIO 97

#ifdef CONFIG_FB_MSM_MDP303
static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio		= MDP_303_VSYNC_GPIO,
	.dsi_power_save		= mipi_dsi_panel_power,
	.dsi_client_reset       = msm_fb_dsi_client_reset,
	.get_lane_config	= msm_fb_get_lane_config,
};
#endif

void __init msm_fb_add_devices(void)
{
	if (machine_is_msm7627a_qrd1())
		platform_add_devices(qrd_fb_devices,
				ARRAY_SIZE(qrd_fb_devices));
	else if (machine_is_msm7627a_evb())
		platform_add_devices(evb_fb_devices,
				ARRAY_SIZE(evb_fb_devices));
	else if (machine_is_msm7627a_qrd3())
		return;
	else
		platform_add_devices(msm_fb_devices,
				ARRAY_SIZE(msm_fb_devices));

	msm_fb_register_device("mdp", &mdp_pdata);
	if (machine_is_msm7625a_surf() || machine_is_msm7x27a_surf())
		msm_fb_register_device("lcdc", &lcdc_pdata);
#ifdef CONFIG_FB_MSM_MDP303
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#endif
}
