/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include <asm/bootinfo.h>

#define KS8851_IRQ_GPIO 94

#define WLAN_CLK	40
#define WLAN_SET	39
#define WLAN_DATA0	38
#define WLAN_DATA1	37
#define WLAN_DATA2	36

static struct gpiomux_setting ap2mdm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting mdm2ap_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting mdm2ap_errfatal_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting mdm2ap_pblrdy = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};


static struct gpiomux_setting ap2mdm_soft_reset_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting ap2mdm_wakeup = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config mdm_configs[] __initdata = {
	/* AP2MDM_STATUS */
	{
		.gpio = 105,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* MDM2AP_STATUS */
	{
		.gpio = 46,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	/* MDM2AP_ERRFATAL */
	{
		.gpio = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	/* AP2MDM_ERRFATAL */
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_SOFT_RESET, aka AP2MDM_PON_RESET_N */
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_soft_reset_cfg,
		}
	},
	/* AP2MDM_WAKEUP */
	{
		.gpio = 104,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_wakeup,
		}
	},
	/* MDM2AP_PBL_READY*/
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_pblrdy,
		}
	},
};

static struct gpiomux_setting gpio_uart_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting slimbus = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_UP,
	.drv = GPIOMUX_DRV_2MA,
	.func = GPIOMUX_FUNC_GPIO,
};

static struct gpiomux_setting gpio_spi_cs2_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_spi_susp_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_spi_cs1_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm_eth_configs[] = {
	{
		.gpio = KS8851_IRQ_GPIO,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
		}
	},
};
#endif

static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_suspend_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* O-LOW */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-PD */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* O-HIGH */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_HIGH,
	},
};

static struct gpiomux_setting gpio_epm_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting gpio_epm_marker_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting wcnss_5wire_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5wire_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting wcnss_5gpio_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5gpio_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ath_gpio_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ath_gpio_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sensor_hub_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir  = GPIOMUX_IN,
};

static struct gpiomux_setting sensor_hub_wake_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir  = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting sensor_hub_rst_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir  = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting sensor_hub_boot_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir  = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	/*
	 * Please keep I2C GPIOs drive-strength at minimum (2ma). It is a
	 * workaround for HW issue of glitches caused by rapid GPIO current-
	 * change.
	 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_i2c_act_config = {
	.func = GPIOMUX_FUNC_3,
	/*
	 * Please keep I2C GPIOs drive-strength at minimum (2ma). It is a
	 * workaround for HW issue of glitches caused by rapid GPIO current-
	 * change.
	 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting lcd_en_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting lcd_en_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting atmel_resout_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting atmel_resout_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting taiko_reset = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting taiko_int = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting hap_lvl_shft_suspended_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hap_lvl_shft_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct msm_gpiomux_config hap_lvl_shft_config[] __initdata = {
	{
		.gpio = 86,
		.settings = {
			[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,
			[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config,
		},
	},
};

static struct msm_gpiomux_config msm_touch_configs[] __initdata = {
	{
		.gpio      = 60,		/* TOUCH RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &atmel_resout_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,
		},
	},
	{
		.gpio      = 61,		/* TOUCH IRQ */
		.settings = {
			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,
		},
	},

};

static struct gpiomux_setting hsic_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hsic_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting hsic_hub_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hsic_resume_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting hsic_resume_susp_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_hsic_configs[] = {
	{
		.gpio = 144,               /*HSIC_STROBE */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 145,               /* HSIC_DATA */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_resume_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_resume_susp_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_hsic_hub_configs[] = {
	{
		.gpio = 50,               /* HSIC_HUB_INT_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_hub_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
};

static struct gpiomux_setting mhl_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mhl_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting hdmi_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_2_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_mhl_configs[] __initdata = {
	{
		/* mhl-sii8334 pwr */
		.gpio = 12,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mhl_suspend_config,
			[GPIOMUX_ACTIVE]    = &mhl_active_1_cfg,
		},
	},
	{
		/* mhl-sii8334 intr */
		.gpio = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mhl_suspend_config,
			[GPIOMUX_ACTIVE]    = &mhl_active_1_cfg,
		},
	},
};


static struct msm_gpiomux_config msm_hdmi_configs[] __initdata = {
	{
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 34,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};

static struct gpiomux_setting gpio_uart7_active_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_uart7_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_blsp2_uart7_configs[] __initdata = {
	{
		.gpio	= 41,	/* BLSP2 UART7 TX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
	{
		.gpio	= 42,	/* BLSP2 UART7 RX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
	{
		.gpio	= 43,	/* BLSP2 UART7 CTS */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
	{
		.gpio	= 44,	/* BLSP2 UART7 RFR */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart7_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_rumi_blsp_configs[] __initdata = {
	{
		.gpio      = 45,	/* BLSP2 UART8 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 46,	/* BLSP2 UART8 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
};

static struct msm_gpiomux_config msm_lcd_configs[] __initdata = {
	{
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_epm_configs[] __initdata = {
	{
		.gpio      = 81,		/* EPM enable */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_epm_config,
		},
	},
	{
		.gpio      = 85,		/* EPM MARKER2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_epm_marker_config,
		},
	},
	{
		.gpio      = 96,		/* EPM MARKER1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_epm_marker_config,
		},
	},
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	{
		.gpio      = 0,		/* BLSP1 QUP SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 1,		/* BLSP1 QUP SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 3,		/* BLSP1 QUP SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 9,		/* BLSP1 QUP SPI_CS2A_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_cs2_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 8,		/* BLSP1 QUP SPI_CS1_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_cs1_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
#endif
	{
		.gpio      = 6,		/* BLSP1 QUP2 I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
			[GPIOMUX_ACTIVE] = &gpio_i2c_act_config,
		},
	},
	{
		.gpio      = 7,		/* BLSP1 QUP2 I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
			[GPIOMUX_ACTIVE] = &gpio_i2c_act_config,
		},
	},
	{
		.gpio      = 83,		/* BLSP11 QUP I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 84,		/* BLSP11 QUP I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 4,			/* BLSP2 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 5,			/* BLSP2 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{                           /* NFC */
		.gpio      = 29,		/* BLSP1 QUP6 I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{                           /* NFC */
		.gpio      = 30,		/* BLSP1 QUP6 I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 53,		/* BLSP2 QUP4 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio      = 54,		/* BLSP2 QUP4 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio      = 56,		/* BLSP2 QUP4 SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio      = 55,		/* BLSP2 QUP4 SPI_CS0_N */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
};

static struct msm_gpiomux_config msm8974_slimbus_config[] __initdata = {
	{
		.gpio	= 70,		/* slimbus clk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
	{
		.gpio	= 71,		/* slimbus data */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
};

static struct gpiomux_setting  mi2s_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting  mi2s_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_1, /*active 1*/ /* 0 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*suspend*/ /* 1 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*i2c suspend*/ /* 2 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 0*/ /* 3 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 0*/ /* 4 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
};

static struct gpiomux_setting sd_card_det_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_sleep_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config sd_card_det __initdata = {
	.gpio = 62,
	.settings = {
		[GPIOMUX_ACTIVE]    = &sd_card_det_active_config,
		[GPIOMUX_SUSPENDED] = &sd_card_det_sleep_config,
	},
};

static struct msm_gpiomux_config msm_sensor_configs[] __initdata = {
	{
		.gpio = 15, /* CAM_MCLK0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 16, /* CAM_MCLK1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 17, /* CAM_MCLK2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 18, /* WEBCAM1_RESET_N / CAM_MCLK3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
	{
		.gpio = 19, /* CCI_I2C_SDA0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 20, /* CCI_I2C_SCL0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 21, /* CCI_I2C_SDA1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 22, /* CCI_I2C_SCL1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 23, /* FLASH_LED_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 24, /* FLASH_LED_NOW */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 25, /* WEBCAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 26, /* CAM_IRQ */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 27, /* OIS_SYNC */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 28, /* WEBCAM1_STANDBY */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 89, /* CAM1_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 90, /* CAM1_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 91, /* CAM2_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 92, /* CAM2_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
};

static struct msm_gpiomux_config msm_sensor_configs_dragonboard[] __initdata = {
	{
		.gpio = 15, /* CAM_MCLK0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 16, /* CAM_MCLK1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 17, /* CAM_MCLK2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 18, /* WEBCAM1_RESET_N / CAM_MCLK3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[4],
		},
	},
	{
		.gpio = 19, /* CCI_I2C_SDA0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 20, /* CCI_I2C_SCL0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 21, /* CCI_I2C_SDA1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 22, /* CCI_I2C_SCL1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],
		},
	},
	{
		.gpio = 23, /* FLASH_LED_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 24, /* FLASH_LED_NOW */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 25, /* WEBCAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 26, /* CAM_IRQ */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[1],
		},
	},
	{
		.gpio = 27, /* OIS_SYNC */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 28, /* WEBCAM1_STANDBY */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 89, /* CAM1_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 90, /* CAM1_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 91, /* CAM2_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
	{
		.gpio = 94, /* CAM2_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],
		},
	},
};

static struct gpiomux_setting auxpcm_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};


static struct gpiomux_setting auxpcm_sus_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

/* Primary AUXPCM port sharing GPIO lines with Primary MI2S */
static struct msm_gpiomux_config msm8974_pri_pri_auxpcm_configs[] __initdata = {
	{
		.gpio = 65,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 66,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 67,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 68,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
};

/* Primary AUXPCM port sharing GPIO lines with Tertiary MI2S */
static struct msm_gpiomux_config msm8974_pri_ter_auxpcm_configs[] __initdata = {
	{
		.gpio = 74,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 75,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 76,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 77,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8974_sec_auxpcm_configs[] __initdata = {
	{
		.gpio = 79,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 81,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
};

static struct msm_gpiomux_config wcnss_5wire_interface[] = {
	{
		.gpio = 36,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 37,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 38,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 39,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 40,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config wcnss_5gpio_interface[] = {
	{
		.gpio = 36,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 37,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 38,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 39,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 40,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config ath_gpio_configs[] = {
	{
		.gpio = 51,
		.settings = {
			[GPIOMUX_ACTIVE]    = &ath_gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &ath_gpio_suspend_cfg,
		},
	},
	{
		.gpio = 79,
		.settings = {
			[GPIOMUX_ACTIVE]    = &ath_gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &ath_gpio_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_taiko_config[] __initdata = {
	{
		.gpio	= 63,		/* SYS_RST_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &taiko_reset,
		},
	},
	{
		.gpio	= 72,		/* CDC_INT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &taiko_int,
		},
	},
};

static struct gpiomux_setting sdc3_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc3_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc3_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc3_data_1_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm8974_sdc3_configs[] __initdata = {
	{
		/* DAT3 */
		.gpio      = 35,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* DAT2 */
		.gpio      = 36,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* DAT1 */
		.gpio      = 37,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_data_1_suspend_cfg,
		},
	},
	{
		/* DAT0 */
		.gpio      = 38,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* CMD */
		.gpio      = 39,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* CLK */
		.gpio      = 40,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
};

static void msm_gpiomux_sdc3_install(void)
{
	msm_gpiomux_install(msm8974_sdc3_configs,
			    ARRAY_SIZE(msm8974_sdc3_configs));
}

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct gpiomux_setting sdc4_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc4_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc4_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc4_data_1_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm8974_sdc4_configs[] __initdata = {
	{
		/* DAT3 */
		.gpio      = 92,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		/* DAT2 */
		.gpio      = 94,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		/* DAT1 */
		.gpio      = 95,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_data_1_suspend_cfg,
		},
	},
	{
		/* DAT0 */
		.gpio      = 96,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		/* CMD */
		.gpio      = 91,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		/* CLK */
		.gpio      = 93,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
};

static void msm_gpiomux_sdc4_install(void)
{
	msm_gpiomux_install(msm8974_sdc4_configs,
			    ARRAY_SIZE(msm8974_sdc4_configs));
}
#else
static void msm_gpiomux_sdc4_install(void) {}
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

static struct msm_gpiomux_config apq8074_dragonboard_ts_config[] __initdata = {
	{
		/* BLSP1 QUP I2C_DATA */
		.gpio      = 2,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		/* BLSP1 QUP I2C_CLK */
		.gpio      = 3,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
};

static struct gpiomux_setting hs_uart_sw_suspend_cfg = {
        .func = GPIOMUX_FUNC_GPIO,
        .drv = GPIOMUX_DRV_2MA,
#ifdef CONFIG_MSM_UART_HS_USE_HS
        .dir = GPIOMUX_OUT_LOW,
#else
        .dir = GPIOMUX_OUT_HIGH,
#endif
};

static struct gpiomux_setting hall_gpio_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir  = GPIOMUX_IN,
};

static struct gpiomux_setting hall_gpio_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir  = GPIOMUX_IN,
};

static struct gpiomux_setting tpa6130_sd_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting max97220_sd_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting tp_audio_pa_power_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config cancro_gpio_configs[] __initdata = {
	{	.gpio	= 2,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 3,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 4,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 5,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 4,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
	{	.gpio	= 5,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
#endif
	{	.gpio	= 6,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 7,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 10,
		.settings = {								},							},
	{	.gpio	= 11,
		.settings = {								},							},
	{	.gpio	= 12,
		.settings = {								},							},
	{	.gpio	= 15,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 16,
		.settings = {								},							},
	{	.gpio	= 17,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 18,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[4],			[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 19,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 20,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 21,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 22,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 24,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 25,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 26,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 27,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 28,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{       .gpio   = 29,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,                    },                                                      },
	{       .gpio   = 30,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,                    },                                                      },
	{	.gpio	= 35,
		.settings = {								},							},
	{	.gpio	= 36,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 37,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 38,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 39,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 40,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 41,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 42,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 43,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 44,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 45,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 46,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 48,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 49,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 50,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 52,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},						},
	{	.gpio	= 54,
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 55,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 56,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 58,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 59,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 60,
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,		[GPIOMUX_ACTIVE] = &atmel_resout_act_cfg, },		},
	{	.gpio	= 61,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 62,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 63,
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_reset,			},							},
	{	.gpio	= 64,
		.settings = {								},							},
	{	.gpio	= 65,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 66,
		.settings = {								},							},
	{	.gpio	= 67,
		.settings = {								},							},
	{	.gpio	= 68,
		.settings = {[GPIOMUX_SUSPENDED] = &hs_uart_sw_suspend_cfg,		},							},
	{	.gpio	= 70,
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 71,
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 72,
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_int,				},							},
	{	.gpio	= 73,
		.settings = {								},							},
	{	.gpio	= 74,
		.settings = {								},							},
	{	.gpio	= 75,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 76,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 77,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 78,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 79,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 80,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 81,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 83,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 84,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 85,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		[GPIOMUX_ACTIVE] = &tpa6130_sd_active_cfg, },		},
	{	.gpio	= 86,
		.settings = {[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,	[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config, },	},
	{	.gpio	= 87,
		.settings = {								},							},
	{	.gpio	= 88,
		.settings = {								},							},
	{	.gpio	= 92,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 93,
		.settings = {								},							},
	{	.gpio	= 117,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
};

static struct msm_gpiomux_config cancro_gpio_nowrite_configs[] __initdata = {
};

static struct msm_gpiomux_config cancro_v4_gpio_configs[] __initdata = {
	{	.gpio	= 0,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 1,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 2,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 3,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 4,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 5,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 4,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
	{	.gpio	= 5,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
#endif
	{	.gpio	= 6,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 7,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 8,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 9,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 10,
		.settings = {								},							},
	{	.gpio	= 11,
		.settings = {								},							},
	{	.gpio	= 12,
		.settings = {								},							},
	{	.gpio	= 13,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 14,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 15,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 16,
		.settings = {								},							},
	{	.gpio	= 17,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 18,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[4],			[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 19,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 20,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 21,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 22,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 23,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 24,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 25,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 26,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 27,
		.settings = {[GPIOMUX_SUSPENDED] = &tp_audio_pa_power_cfg,		},							},
	{	.gpio	= 28,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 29,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 30,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 31,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 32,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 33,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 34,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 35,
		.settings = {								},							},
	{	.gpio	= 36,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 37,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 38,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 39,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 40,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 41,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 42,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 43,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 44,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 45,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 46,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 47,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 48,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 49,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 50,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 51,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 52,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 53,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 54,
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 55,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 56,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 57,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 58,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 59,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 60,
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,		[GPIOMUX_ACTIVE] = &atmel_resout_act_cfg, },		},
	{	.gpio	= 61,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 62,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 63,
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_reset,			},							},
	{	.gpio	= 64,
		.settings = {								},							},
	{	.gpio	= 65,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 66,
		.settings = {								},							},
	{	.gpio	= 67,
		.settings = {								},							},
	{	.gpio	= 68,
		.settings = {[GPIOMUX_SUSPENDED] = &hs_uart_sw_suspend_cfg,		},							},
	{	.gpio	= 69,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 70,
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 71,
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 72,
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_int,				},							},
	{	.gpio	= 73,
		.settings = {								},							},
	{	.gpio	= 74,
		.settings = {								},							},
	{	.gpio	= 75,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 76,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 77,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 78,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 79,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 80,
		.settings = {[GPIOMUX_SUSPENDED] = &hall_gpio_suspend_config,    	[GPIOMUX_ACTIVE] = &hall_gpio_active_config },		},
	{	.gpio	= 81,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 82,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 83,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 84,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 85,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 86,
		.settings = {[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,	[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config, },	},
	{	.gpio	= 87,
		.settings = {								},							},
	{	.gpio	= 88,
		.settings = {								},							},
	{	.gpio	= 89,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 90,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 91,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 92,
		.settings = {[GPIOMUX_SUSPENDED] = &hall_gpio_suspend_config,    	[GPIOMUX_ACTIVE] = &hall_gpio_active_config },		},
	{	.gpio	= 93,
		.settings = {								},							},
	{	.gpio	= 94,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 95,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 96,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 97,
		.settings = {								},							},
	{	.gpio	= 98,
		.settings = {								},							},
	{	.gpio	= 99,
		.settings = {								},							},
	{	.gpio	= 100,
		.settings = {								},							},
	{	.gpio	= 101,
		.settings = {								},							},
	{	.gpio	= 102,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 103,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 104,
		.settings = {								},							},
	{	.gpio	= 105,
		.settings = {								},							},
	{	.gpio	= 106,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 107,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 108,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 109,
		.settings = {								},							},
	{	.gpio	= 110,
		.settings = {								},							},
	{	.gpio	= 111,
		.settings = {								},							},
	{	.gpio	= 112,
		.settings = {								},							},
	{	.gpio	= 113,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 114,
		.settings = {								},							},
	{	.gpio	= 115,
		.settings = {								},							},
	{	.gpio	= 116,
		.settings = {								},							},
	{	.gpio	= 117,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 118,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 119,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 120,
		.settings = {								},							},
	{	.gpio	= 121,
		.settings = {								},							},
	{	.gpio	= 122,
		.settings = {								},							},
	{	.gpio	= 123,
		.settings = {								},							},
	{	.gpio	= 124,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 125,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 126,
		.settings = {								},							},
	{	.gpio	= 127,
		.settings = {								},							},
	{	.gpio	= 128,
		.settings = {								},							},
	{	.gpio	= 129,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 130,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 131,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 132,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 133,
		.settings = {								},							},
	{	.gpio	= 134,
		.settings = {								},							},
	{	.gpio	= 135,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 136,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 137,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 138,
		.settings = {								},							},
	{	.gpio	= 139,
		.settings = {								},							},
	{	.gpio	= 140,
		.settings = {								},							},
	{	.gpio	= 141,
		.settings = {								},							},
	{	.gpio	= 142,
		.settings = {								},							},
	{	.gpio	= 143,
		.settings = {								},							},
	{	.gpio	= 144,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 145,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
};

static struct msm_gpiomux_config cancro_v4_gpio_nowrite_configs[] __initdata = {
};

static struct msm_gpiomux_config cancro_v5_gpio_configs[] __initdata = {
	{	.gpio	= 0,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 1,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 2,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 3,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 4,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 5,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 4,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
	{	.gpio	= 5,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
#endif
	{	.gpio	= 6,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config, },			},
	{	.gpio	= 7,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config, },			},
	{	.gpio	= 8,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 9,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 10,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 11,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 12,
		.settings = {								},							},
	{	.gpio	= 13,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 14,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 15,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 16,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 17,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 18,
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[4],			[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 19,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 20,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 21,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 22,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 23,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 24,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 25,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 26,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 27,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 28,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 29,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 30,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 31,
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_rst_config,		[GPIOMUX_ACTIVE] = &sensor_hub_rst_config, },		},
	{	.gpio	= 32,
		.settings = {[GPIOMUX_SUSPENDED] = &hs_uart_sw_suspend_cfg,		},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 33,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 34,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 33,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		},							},
	{	.gpio	= 34,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		},							},
#endif
	{	.gpio	= 35,
		.settings = {								},							},
	{	.gpio	= 36,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 37,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 38,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 39,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 40,
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 41,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 42,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 43,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 44,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 45,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 46,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 47,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 48,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 49,
		.settings = {								},							},
	{	.gpio	= 50,
		.settings = {								},							},
	{	.gpio	= 51,
		.settings = {								},							},
	{	.gpio	= 52,
		.settings = {								},							},
	{	.gpio	= 53,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 54,
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 55,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 56,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 57,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			},							},
	{	.gpio	= 58,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 59,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 60,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 61,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 62,
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,		[GPIOMUX_ACTIVE] = &atmel_resout_act_cfg, },		},
	{	.gpio	= 63,
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_reset,			},							},
	{	.gpio	= 64,
		.settings = {[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,      [GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config, },      },
	{	.gpio	= 65,
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 66,
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_int_config,		[GPIOMUX_ACTIVE] = &sensor_hub_int_config, },		},
	{	.gpio	= 67,
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_boot_config,		[GPIOMUX_ACTIVE] = &sensor_hub_boot_config, },		},
	{	.gpio	= 68,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 69,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 70,
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 71,
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 72,
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_int,				},							},
	{	.gpio	= 73,
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_wake_config,		[GPIOMUX_ACTIVE] = &sensor_hub_wake_config, },		},
	{	.gpio	= 74,
		.settings = {								},							},
	{	.gpio	= 75,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 76,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 77,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 78,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 79,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 80,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 81,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 82,
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 83,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 84,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 85,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 86,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 87,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config,},		},
	{	.gpio	= 88,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config,},		},
	{	.gpio	= 89,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 90,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 91,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 92,
		.settings = {[GPIOMUX_SUSPENDED] = &hall_gpio_suspend_config,		[GPIOMUX_ACTIVE] = &hall_gpio_active_config, },		},
	{	.gpio	= 93,
		.settings = {								},							},
	{	.gpio	= 94,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 95,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 96,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 97,
		.settings = {								},							},
	{	.gpio	= 98,
		.settings = {								},							},
	{	.gpio	= 99,
		.settings = {								},							},
	{	.gpio	= 100,
		.settings = {								},							},
	{	.gpio	= 101,
		.settings = {								},							},
	{	.gpio	= 102,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 103,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 104,
		.settings = {								},							},
	{	.gpio	= 105,
		.settings = {								},							},
	{	.gpio	= 106,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 107,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 108,
		.settings = {								},							},
	{	.gpio	= 109,
		.settings = {								},							},
	{	.gpio	= 110,
		.settings = {								},							},
	{	.gpio	= 111,
		.settings = {								},							},
	{	.gpio	= 112,
		.settings = {								},							},
	{	.gpio	= 113,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 114,
		.settings = {								},							},
	{	.gpio	= 115,
		.settings = {								},							},
	{	.gpio	= 116,
		.settings = {								},							},
	{	.gpio	= 117,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[1], },		},
	{	.gpio	= 118,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 119,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 120,
		.settings = {								},							},
	{	.gpio	= 121,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 122,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 123,
		.settings = {								},							},
	{	.gpio	= 124,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 125,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 126,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 127,
		.settings = {								},							},
	{	.gpio	= 128,
		.settings = {								},							},
	{	.gpio	= 129,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 130,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 131,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 132,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 133,
		.settings = {								},							},
	{	.gpio	= 134,
		.settings = {								},							},
	{	.gpio	= 135,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 136,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 137,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 138,
		.settings = {								},							},
	{	.gpio	= 139,
		.settings = {								},							},
	{	.gpio	= 140,
		.settings = {								},							},
	{	.gpio	= 141,
		.settings = {								},							},
	{	.gpio	= 142,
		.settings = {								},							},
	{	.gpio	= 143,
		.settings = {								},							},
	{	.gpio	= 144,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 145,
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
};

static struct msm_gpiomux_config cancro_v5_gpio_nowrite_configs[] __initdata = {
};

void __init msm_8974_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	pr_err("%s:%d socinfo_get_version %x\n", __func__, __LINE__,
		socinfo_get_version());
	if (socinfo_get_version() >= 0x20000)
		msm_tlmm_misc_reg_write(TLMM_SPARE_REG, 0xf);

	if (of_machine_is_compatible("qcom,msm8974")) {
		if (get_hw_version_major() == 5) {
			msm_gpiomux_install(cancro_v5_gpio_configs,
					ARRAY_SIZE(cancro_v5_gpio_configs));
			msm_gpiomux_install_nowrite(cancro_v5_gpio_nowrite_configs,
					ARRAY_SIZE(cancro_v5_gpio_nowrite_configs));

		} else if (get_hw_version_major() == 4) {
			/* Handle V4 minor version different here */
			if (get_hw_version_minor() == 2) {
				/* This will break P2.5 */
				cancro_v4_gpio_configs[145].settings[GPIOMUX_SUSPENDED] =
					&tp_audio_pa_power_cfg;
			}
			if (get_hw_version_minor() == 1 || get_hw_version_minor() == 2)
				cancro_v4_gpio_configs[81].settings[GPIOMUX_ACTIVE] =
					&max97220_sd_active_cfg;
			msm_gpiomux_install(cancro_v4_gpio_configs,
					ARRAY_SIZE(cancro_v4_gpio_configs));
			msm_gpiomux_install_nowrite(cancro_v4_gpio_nowrite_configs,
					ARRAY_SIZE(cancro_v4_gpio_nowrite_configs));

		} else {
			msm_gpiomux_install(cancro_gpio_configs,
					ARRAY_SIZE(cancro_gpio_configs));
			msm_gpiomux_install_nowrite(cancro_gpio_nowrite_configs,
					ARRAY_SIZE(cancro_gpio_nowrite_configs));
		}

		return;
	}
#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	if (!(of_board_is_dragonboard() && machine_is_apq8074()))
		msm_gpiomux_install(msm_eth_configs, \
			ARRAY_SIZE(msm_eth_configs));
#endif
	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));
	msm_gpiomux_install(msm_blsp2_uart7_configs,
			 ARRAY_SIZE(msm_blsp2_uart7_configs));
	msm_gpiomux_install(wcnss_5wire_interface,
				ARRAY_SIZE(wcnss_5wire_interface));
	if (of_board_is_liquid())
		msm_gpiomux_install_nowrite(ath_gpio_configs,
					ARRAY_SIZE(ath_gpio_configs));
	msm_gpiomux_install(msm8974_slimbus_config,
			ARRAY_SIZE(msm8974_slimbus_config));

	msm_gpiomux_install(msm_touch_configs, ARRAY_SIZE(msm_touch_configs));
		msm_gpiomux_install(hap_lvl_shft_config,
				ARRAY_SIZE(hap_lvl_shft_config));

	if (of_board_is_dragonboard() && machine_is_apq8074())
		msm_gpiomux_install(msm_sensor_configs_dragonboard, \
				ARRAY_SIZE(msm_sensor_configs_dragonboard));
	else
		msm_gpiomux_install(msm_sensor_configs, \
				ARRAY_SIZE(msm_sensor_configs));

	msm_gpiomux_install(&sd_card_det, 1);

	if (machine_is_apq8074() && (of_board_is_liquid() || \
	    of_board_is_dragonboard()))
		msm_gpiomux_sdc3_install();

	if (!(of_board_is_dragonboard() && machine_is_apq8074()))
		msm_gpiomux_sdc4_install();

	msm_gpiomux_install(msm_taiko_config, ARRAY_SIZE(msm_taiko_config));

	msm_gpiomux_install(msm_hsic_configs, ARRAY_SIZE(msm_hsic_configs));
	msm_gpiomux_install(msm_hsic_hub_configs,
				ARRAY_SIZE(msm_hsic_hub_configs));

	msm_gpiomux_install(msm_hdmi_configs, ARRAY_SIZE(msm_hdmi_configs));
	if (of_board_is_fluid())
		msm_gpiomux_install(msm_mhl_configs,
				    ARRAY_SIZE(msm_mhl_configs));

	if (of_board_is_liquid() ||
	    (of_board_is_dragonboard() && machine_is_apq8074()))
		msm_gpiomux_install(msm8974_pri_ter_auxpcm_configs,
				 ARRAY_SIZE(msm8974_pri_ter_auxpcm_configs));
	else
		msm_gpiomux_install(msm8974_pri_pri_auxpcm_configs,
				 ARRAY_SIZE(msm8974_pri_pri_auxpcm_configs));

	if (of_board_is_cdp())
		msm_gpiomux_install(msm8974_sec_auxpcm_configs,
				 ARRAY_SIZE(msm8974_sec_auxpcm_configs));
	else if (of_board_is_liquid() || of_board_is_fluid() ||
						of_board_is_mtp())
		msm_gpiomux_install(msm_epm_configs,
				ARRAY_SIZE(msm_epm_configs));

	msm_gpiomux_install_nowrite(msm_lcd_configs,
			ARRAY_SIZE(msm_lcd_configs));

	if (of_board_is_rumi())
		msm_gpiomux_install(msm_rumi_blsp_configs,
				    ARRAY_SIZE(msm_rumi_blsp_configs));

	if (socinfo_get_platform_subtype() == PLATFORM_SUBTYPE_MDM)
		msm_gpiomux_install(mdm_configs,
			ARRAY_SIZE(mdm_configs));

	if (of_board_is_dragonboard() && machine_is_apq8074())
		msm_gpiomux_install(apq8074_dragonboard_ts_config,
			ARRAY_SIZE(apq8074_dragonboard_ts_config));
}

static void wcnss_switch_to_gpio(void)
{
	/* Switch MUX to GPIO */
	msm_gpiomux_install(wcnss_5gpio_interface,
			ARRAY_SIZE(wcnss_5gpio_interface));

	/* Ensure GPIO config */
	gpio_direction_input(WLAN_DATA2);
	gpio_direction_input(WLAN_DATA1);
	gpio_direction_input(WLAN_DATA0);
	gpio_direction_output(WLAN_SET, 0);
	gpio_direction_output(WLAN_CLK, 0);
}

static void wcnss_switch_to_5wire(void)
{
	msm_gpiomux_install(wcnss_5wire_interface,
			ARRAY_SIZE(wcnss_5wire_interface));
}

u32 wcnss_rf_read_reg(u32 rf_reg_addr)
{
	int count = 0;
	u32 rf_cmd_and_addr = 0;
	u32 rf_data_received = 0;
	u32 rf_bit = 0;

	wcnss_switch_to_gpio();

	/* Reset the signal if it is already being used. */
	gpio_set_value(WLAN_SET, 0);
	gpio_set_value(WLAN_CLK, 0);

	/* We start with cmd_set high WLAN_SET = 1. */
	gpio_set_value(WLAN_SET, 1);

	gpio_direction_output(WLAN_DATA0, 1);
	gpio_direction_output(WLAN_DATA1, 1);
	gpio_direction_output(WLAN_DATA2, 1);

	gpio_set_value(WLAN_DATA0, 0);
	gpio_set_value(WLAN_DATA1, 0);
	gpio_set_value(WLAN_DATA2, 0);

	/* Prepare command and RF register address that need to sent out.
	 * Make sure that we send only 14 bits from LSB.
	 */
	rf_cmd_and_addr  = (((WLAN_RF_READ_REG_CMD) |
		(rf_reg_addr << WLAN_RF_REG_ADDR_START_OFFSET)) &
		WLAN_RF_READ_CMD_MASK);

	for (count = 0; count < 5; count++) {
		gpio_set_value(WLAN_CLK, 0);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(WLAN_DATA0, rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(WLAN_DATA1, rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(WLAN_DATA2, rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		/* Send the data out WLAN_CLK = 1 */
		gpio_set_value(WLAN_CLK, 1);
	}

	/* Pull down the clock signal */
	gpio_set_value(WLAN_CLK, 0);

	/* Configure data pins to input IO pins */
	gpio_direction_input(WLAN_DATA0);
	gpio_direction_input(WLAN_DATA1);
	gpio_direction_input(WLAN_DATA2);

	for (count = 0; count < 2; count++) {
		gpio_set_value(WLAN_CLK, 1);
		gpio_set_value(WLAN_CLK, 0);
	}

	rf_bit = 0;
	for (count = 0; count < 6; count++) {
		gpio_set_value(WLAN_CLK, 1);
		gpio_set_value(WLAN_CLK, 0);

		rf_bit = gpio_get_value(WLAN_DATA0);
		rf_data_received |= (rf_bit << (count * 3 + 0));

		if (count != 5) {
			rf_bit = gpio_get_value(WLAN_DATA1);
			rf_data_received |= (rf_bit << (count * 3 + 1));

			rf_bit = gpio_get_value(WLAN_DATA2);
			rf_data_received |= (rf_bit << (count * 3 + 2));
		}
	}

	gpio_set_value(WLAN_SET, 0);
	wcnss_switch_to_5wire();

	return rf_data_received;
}
