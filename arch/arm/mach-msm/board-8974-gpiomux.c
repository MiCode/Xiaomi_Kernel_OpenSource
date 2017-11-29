/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017 XiaoMi, Inc.
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

static struct gpiomux_setting gpio_uart_config __initdata = {
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

static struct gpiomux_setting proximity_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir  = GPIOMUX_IN,
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
	{	.gpio	= 2,		/* BLSP1 QUP0 I2C_DAT */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 3,		/* BLSP1 QUP0 I2C_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 4,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 5,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 4,		/* BLSP2 UART TX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
	{	.gpio	= 5,		/* BLSP2 UART RX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
#endif
	{	.gpio	= 6,		/* BLSP1 QUP2 I2C_DAT */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 7,		/* BLSP1 QUP2 I2C_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 10,		/* Sensor I2C_DAT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 11,		/* Sensor I2C_CLK, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 12,		/* LCD_TE, todo */
		.settings = {								},							},
	{	.gpio	= 15,		/* CAM_MCLK0 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 16,		/* LCD_BL_EN, unused */
		.settings = {								},							},
	{	.gpio	= 17,		/* CAM_MCLK2 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 18,		/* WEBCAM1_RESET_N / CAM_MCLK3 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[4],			[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 19,		/* CCI_I2C_SDA0 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 20,		/* CCI_I2C_SCL0 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 21,		/* CCI_I2C_SDA1 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 22,		/* CCI_I2C_SCL1 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 24,		/* FLASH_LED_NOW */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 25,		/* LCD_ID_DET0, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 26,		/* LCD_ID_DET1, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 27,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 28,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{       .gpio   = 29,           /* BLSP6 QUP2 I2C_DAT */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,                    },                                                      },
	{       .gpio   = 30,           /* BLSP6 QUP2 I2C_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,                    },                                                      },
	{	.gpio	= 35,		/* WCSS_BT_SSBI, managed by WCN */
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
	{	.gpio	= 41,		/* BLSP2 UART7 TX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 42,		/* BLSP2 UART7 RX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 43,		/* BLSP2 UART7 CTS */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 44,		/* BLSP2 UART7 RFR */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 45,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 46,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 48,		/* Touch Power */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 49,		/* CAM_VDD_1P05_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 50,		/* CAM1_PWDN, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 52,		/* CAM0_PWDN, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},						},
	{	.gpio	= 54,		/* TOUCH IRQ */
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 55,		/* LCD_VSP_EN, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 56,		/* LCD_VSN_EN, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 58,		/* NFC_REG_PU, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 59,		/* NFC_I2C_REQ, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 60,		/* TOUCH RESET */
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,		[GPIOMUX_ACTIVE] = &atmel_resout_act_cfg, },		},
	{	.gpio	= 61,		/* NFC_CLK_REQ, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 62,		/* NFC_WAKE, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 63,		/* SYS_RST_N */
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_reset,			},							},
	{	.gpio	= 64,		/* SENSOR_RESET_N, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 65,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 66,		/* GYRO_INT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 67,		/* COMPASS_INT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 68,		/* AUDIO_SWITCH_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &hs_uart_sw_suspend_cfg,		},							},
	{	.gpio	= 70,		/* slimbus clk */
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 71,		/* slimbus data */
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 72,		/* CDC_INT */
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_int,				},							},
	{	.gpio	= 73,		/* SNS_SYNC, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 74,		/* PROXIMITY_INT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 75,		/* VP_RESET, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 76,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 77,		/* CAM1_RST_N */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 78,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 79,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 80,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 81,		/* SPKEN_EN, unused, X3P1:TPA6130_SD */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 83,		/* BLSP11 QUP I2C_DAT */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 84,		/* BLSP11 QUP I2C_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 85,		/* TPA6130_SD */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		[GPIOMUX_ACTIVE] = &tpa6130_sd_active_cfg, },		},
	{	.gpio	= 86,
		.settings = {[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,	[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config, },	},
	{	.gpio	= 87,		/* SENSOR_I2C_DAT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 88,		/* SENSOR_I2C_CLK, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 92,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 93,		/* CODEC_INT2, todo */
		.settings = {								},							},
	{	.gpio	= 117,		/* CAM1_STANDBY_N */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
};

static struct msm_gpiomux_config cancro_gpio_nowrite_configs[] __initdata = {
};

static struct msm_gpiomux_config cancro_v4_gpio_configs[] __initdata = {
	{	.gpio	= 0,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 1,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 2,		/* BLSP1 QUP0 I2C_DAT, HPH_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 3,		/* BLSP1 QUP0 I2C_CLK, HPH_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 4,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 5,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 4,		/* BLSP2 UART TX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
	{	.gpio	= 5,		/* BLSP2 UART RX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
#endif
	{	.gpio	= 6,		/* BLSP1 QUP2 I2C_DAT, TS_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 7,		/* BLSP1 QUP2 I2C_CLK, TS_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 8,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 9,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 10,		/* Sensor I2C_DAT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 11,		/* Sensor I2C_CLK, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 12,		/* LCD_TE, todo */
		.settings = {								},							},
	{	.gpio	= 13,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 14,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 15,		/* CAM_MCLK0 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 16,		/* LCD_BL_EN, unused */
		.settings = {								},							},
	{	.gpio	= 17,		/* CAM_MCLK2 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 18,		/* WEBCAM1_RESET_N / CAM_MCLK3 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[4],			[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 19,		/* CCI_I2C_SDA0, CAM0_I2C_SDA0 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 20,		/* CCI_I2C_SCL0, CAM0_I2C_SCL0 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 21,		/* CCI_I2C_SDA1, CAM1_I2C_SDA1 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 22,		/* CCI_I2C_SCL1, CAM1_I2C_SCL1 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 23,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 24,		/* FLASH_LED_NOW */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 25,		/* LCD_ID_DET0, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 26,		/* LCD_ID_DET1, todo */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		},							},
	{	.gpio	= 27,		/* LDO_5V_IN_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &tp_audio_pa_power_cfg,		},							},
	{	.gpio	= 28,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 29,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 30,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 31,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 32,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 33,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 34,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 35,		/* WCSS_BT_SSBI, managed by WCN */
		.settings = {								},							},
	{	.gpio	= 36,		/* WCSS_WLAN_DATA_2 */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 37,		/* WCSS_WLAN_DATA_1 */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 38,		/* WCSS_WLAN_DATA_0 */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 39,		/* WCSS_WLAN_SET */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 40,		/* WCSS_WLAN_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 41,		/* BLSP2 UART7 TX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 42,		/* BLSP2 UART7 RX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 43,		/* BLSP2 UART7 CTS */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 44,		/* BLSP2 UART7 RFR */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 45,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 46,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 47,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 48,		/* TP_PWDN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 49,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 50,		/* CAM1_PWDN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 51,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 52,		/* CAM0_PWDN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 53,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 54,		/* TS_INT */
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 55,		/* LCD_VSP_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 56,		/* LCD_VSN_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3], 		},							},
	{	.gpio	= 57,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 58,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 59,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 60,		/* TS_RESET_N */
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,		[GPIOMUX_ACTIVE] = &atmel_resout_act_cfg, },		},
	{	.gpio	= 61,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 62,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 63,		/* CODEC_RESET_N */
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_reset,			},							},
	{	.gpio	= 64,		/* SENSOR_RESET_N, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 65,		/* CAM_VDD_1P05_EN for X4 P2.5 afterward */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 66,		/* GYRO_INT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 67,		/* COMPASS_INT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 68,		/* AUDIO_SWITCH_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &hs_uart_sw_suspend_cfg,		},							},
	{	.gpio	= 69,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 70,		/* SLIMBUS_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 71,		/* SLIMBUS_DATA */
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 72,		/* CODEC_INT1_N */
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_int,				},							},
	{	.gpio	= 73,		/* SNS_SYNC, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 74,		/* PROXIMITY_INT, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 75,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 76,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 77,		/* CAM0_RST_N */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 78,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 79,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 80,		/* HALL_INTR1 */
		.settings = {[GPIOMUX_SUSPENDED] = &hall_gpio_suspend_config,    	[GPIOMUX_ACTIVE] = &hall_gpio_active_config },		},
	{	.gpio	= 81,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 82,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 83,		/* BLSP11 QUP I2C_DAT, HAPTICS_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 84,		/* BLSP11 QUP I2C_CLK, HAPTICS_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 85,		/* TPA6130_SD */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 86,		/* HAPTICS_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,	[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config, },	},
	{	.gpio	= 87,		/* SENSOR1_I2C_SDA, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 88,		/* SENSOR1_I2C_SCL, managed by DSPS */
		.settings = {								},							},
	{	.gpio	= 89,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 90,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 91,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 92,		/* HALL_INTR2 */
		.settings = {[GPIOMUX_SUSPENDED] = &hall_gpio_suspend_config,    	[GPIOMUX_ACTIVE] = &hall_gpio_active_config },		},
	{	.gpio	= 93,		/* CODEC_INT2_N */
		.settings = {								},							},
	{	.gpio	= 94,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 95,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 96,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 97,		/* UIM1_DATA */
		.settings = {								},							},
	{	.gpio	= 98,		/* UIM1_CLK */
		.settings = {								},							},
	{	.gpio	= 99,		/* UIM1_RST */
		.settings = {								},							},
	{	.gpio	= 100,		/* UIM1_DETECT */
		.settings = {								},							},
	{	.gpio	= 101,		/* BATT_REM_ALARM */
		.settings = {								},							},
	{	.gpio	= 102,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 103,		/* FORCE_USB_BOOT */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 104,		/* GRFC_0 */
		.settings = {								},							},
	{	.gpio	= 105,		/* GRFC_1 */
		.settings = {								},							},
	{	.gpio	= 106,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 107,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 108,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 109,		/* GRFC_5 */
		.settings = {								},							},
	{	.gpio	= 110,		/* RX_ON0 */
		.settings = {								},							},
	{	.gpio	= 111,		/* RF_ON0 */
		.settings = {								},							},
	{	.gpio	= 112,		/* PA0_R0 */
		.settings = {								},							},
	{	.gpio	= 113,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 114,		/* GRFC10_SW */
		.settings = {								},							},
	{	.gpio	= 115,		/* GRFC11_SW */
		.settings = {								},							},
	{	.gpio	= 116,		/* TX_GTR_THRES */
		.settings = {								},							},
	{	.gpio	= 117,		/* CAM_AF_PWDN_N */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 118,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 119,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 120,		/* GRFC16_SW */
		.settings = {								},							},
	{	.gpio	= 121,		/* GRFC17_SW */
		.settings = {								},							},
	{	.gpio	= 122,		/* GRFC18_SW */
		.settings = {								},							},
	{	.gpio	= 123,		/* GRFC19_SW */
		.settings = {								},							},
	{	.gpio	= 124,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 125,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 126,		/* GRFC22_SW */
		.settings = {								},							},
	{	.gpio	= 127,		/* GRFC23_SW */
		.settings = {								},							},
	{	.gpio	= 128,		/* GPS_EXT_LNA_EN */
		.settings = {								},							},
	{	.gpio	= 129,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 130,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 131,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 132,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 133,		/* SSBI1_TX_GNSS */
		.settings = {								},							},
	{	.gpio	= 134,		/* SSBI1_PRX_DRX */
		.settings = {								},							},
	{	.gpio	= 135,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 136,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 137,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 138,		/* GP_DATA1 */
		.settings = {								},							},
	{	.gpio	= 139,		/* GP_DATA0 */
		.settings = {								},							},
	{	.gpio	= 140,		/* RFFE1_CLK */
		.settings = {								},							},
	{	.gpio	= 141,		/* RFFE1_DATA */
		.settings = {								},							},
	{	.gpio	= 142,		/* RFFE2_CLK */
		.settings = {								},							},
	{	.gpio	= 143,		/* RFFE2_DATA */
		.settings = {								},							},
	{	.gpio	= 144,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 145,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
};

static struct msm_gpiomux_config cancro_v4_gpio_nowrite_configs[] __initdata = {
};

static struct msm_gpiomux_config cancro_v5_gpio_configs[] __initdata = {
	{	.gpio	= 0,		/* HIFI_49M_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 1,		/* HIFI_RESET_N */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 2,		/* BLSP1 QUP0 I2C_DAT, HIFI_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 3,		/* BLSP1 QUP0 I2C_CLK, HIFI_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 4,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 5,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 4,		/* BLSP2 UART TX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
	{	.gpio	= 5,		/* BLSP2 UART RX */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart_config,			},							},
#endif
	{	.gpio	= 6,		/* BLSP1 QUP2 I2C_DAT, TS_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config, },			},
	{	.gpio	= 7,		/* BLSP1 QUP2 I2C_CLK, TS_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config, },			},
	{	.gpio	= 8,		/* HIFI_1P8_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 9,		/* HIFI_3P3_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 10,		/* SENSOR2_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 11,		/* SENSOR2_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 12,		/* LCD_TE */
		.settings = {								},							},
	{	.gpio	= 13,		/* HIFI_+5P0_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 14,		/* HIFI_-5P0_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 15,		/* CAM_MCLK0 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 16,		/* TP_LDO_IN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 17,		/* CAM1_MCLK2 */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[1],			[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 18,		/* CAM1_RESET_N */
		.settings = {[GPIOMUX_SUSPENDED] = &cam_settings[4],			[GPIOMUX_ACTIVE] = &cam_settings[3], },			},
	{	.gpio	= 19,		/* CCI_I2C_SDA0, CAM0_I2C_SDA0 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 20,		/* CCI_I2C_SCL0, CAM0_I2C_SCL0 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 21,		/* CCI_I2C_SDA1, CAM1_I2C_SDA1 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 22,		/* CCI_I2C_SCL1, CAM1_I2C_SCL1 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[0],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 23,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 24,		/* FLASH_LED_NOW */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &cam_settings[0], },			},
	{	.gpio	= 25,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 26,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 27,		/* FLS_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 28,		/* FLS_STROBE */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 29,		/* FLS_TORCH */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 30,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 31,		/* SENSOR_HUB_RST */
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_rst_config,		[GPIOMUX_ACTIVE] = &sensor_hub_rst_config, },		},
	{	.gpio	= 32,		/* AUDIO_SWTICH_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &hs_uart_sw_suspend_cfg,		},							},
#ifdef CONFIG_MSM_UART_HS_USE_HS
	{	.gpio	= 33,		/* HIFI_SW_MUTE */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 34,		/* HIFI_SW_SEL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
#else
	{	.gpio	= 33,		/* HIFI_SW_MUTE */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		},							},
	{	.gpio	= 34,		/* HIFI_SW_SEL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		},							},
#endif
	{	.gpio	= 35,		/* WCSS_BT_SSBI, managed by WCN */
		.settings = {								},							},
	{	.gpio	= 36,		/* WCSS_WLAN_DATA_2 */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 37,		/* WCSS_WLAN_DATA_1 */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 38,		/* WCSS_WLAN_DATA_0 */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 39,		/* WCSS_WLAN_SET */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 40,		/* WCSS_WLAN_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,		[GPIOMUX_ACTIVE] = &wcnss_5wire_active_cfg, },		},
	{	.gpio	= 41,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 42,		/* SNR_HUB_RST */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 43,		/* BLSP2 UART7 CTS, BT_CTL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 44,		/* BLSP2 UART7 RFR, BT_DATA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_uart7_suspend_cfg,		[GPIOMUX_ACTIVE] = &gpio_uart7_active_cfg, },		},
	{	.gpio	= 45,		/* CAM_VDD_1P05_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 46,		/* CAM1_PWDN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 47,		/* CAM0_ID */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 48,		/* CAM0_RST_N */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 49,		/* UIM2_DATA */
		.settings = {								},							},
	{	.gpio	= 50,		/* UIM2_CLK */
		.settings = {								},							},
	{	.gpio	= 51,		/* UIM2_RST */
		.settings = {								},							},
	{	.gpio	= 52,		/* UIM2_DETECT */
		.settings = {								},							},
	{	.gpio	= 53,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 54,		/* TS_INT */
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 55,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 56,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 57,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			},							},
	{	.gpio	= 58,		/* qua mi2s sck */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 59,		/* qua mi2s wck */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 60,		/* qua mi2s data0 */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 61,		/* unsued */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 62,		/* TS_RESET_N */
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,		[GPIOMUX_ACTIVE] = &atmel_resout_act_cfg, },		},
	{	.gpio	= 63,		/* CODEC_RESET_N */
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_reset,			},							},
	{	.gpio	= 64,		/* HAPTICS_HEN */
		.settings = {[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,      [GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config, },      },
	{	.gpio	= 65,		/* TP_KEY_INT */
		.settings = {[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg, },		},
	{	.gpio	= 66,		/* SENSOR_HUB_INT */
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_int_config,		[GPIOMUX_ACTIVE] = &sensor_hub_int_config, },		},
	{	.gpio	= 67,		/* SENSOR_HUB_BOOT */
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_boot_config,		[GPIOMUX_ACTIVE] = &sensor_hub_boot_config, },		},
	{	.gpio	= 68,		/* LCD_1V8_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 69,		/* LCD_VSN_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 70,		/* SLIMBUS_CLK */
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 71,		/* SLIMBUS_DATA */
		.settings = {[GPIOMUX_SUSPENDED] = &slimbus,				},							},
	{	.gpio	= 72,		/* CODEC_INT1_N */
		.settings = {[GPIOMUX_SUSPENDED] = &taiko_int,				},							},
	{	.gpio	= 73,		/* SENSOR_HUB_WAKE */
		.settings = {[GPIOMUX_SUSPENDED] = &sensor_hub_wake_config,		[GPIOMUX_ACTIVE] = &sensor_hub_wake_config, },		},
	{	.gpio	= 74,		/* PROXIMITY_INT, managed by DSPS */
		.settings = {[GPIOMUX_SUSPENDED] = &proximity_int_config,		[GPIOMUX_ACTIVE] = &proximity_int_config, 	},	},
	{	.gpio	= 75,		/* LCD_ID_DET0 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 76,		/* SPKR_PA_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 77,		/* TS_PWDN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 78,		/* SPKR_I2S_MCLK */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 79,		/* SPKR_I2S_BCK */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 80,		/* SPKR_I2S_WS */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 81,		/* SPKR_I2S_DOUT */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 82,		/* SPKR_I2S_DIN */
		.settings = {[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,			[GPIOMUX_ACTIVE] = &mi2s_act_cfg, },			},
	{	.gpio	= 83,		/* BLSP11 QUP I2C_DAT, HAPTICS_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 84,		/* BLSP11 QUP I2C_CLK, HAPTICS_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			},							},
	{	.gpio	= 85,		/* LCD_VSP_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[3],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[3], },		},
	{	.gpio	= 86,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 87,		/* SENSOR1_I2C_SDA */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config,},		},
	{	.gpio	= 88,		/* SENSOR1_I2C_SCL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_i2c_config,			[GPIOMUX_ACTIVE] = &gpio_i2c_config,},		},
	{	.gpio	= 89,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 90,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 91,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 92,		/* HALL_INTR2 */
		.settings = {[GPIOMUX_SUSPENDED] = &hall_gpio_suspend_config,		[GPIOMUX_ACTIVE] = &hall_gpio_active_config, },		},
	{	.gpio	= 93,		/* CODEC_INT2_N */
		.settings = {								},							},
	{	.gpio	= 94,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 95,		/* HALL_INTR1 */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 96,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 97,		/* UIM1_DATA */
		.settings = {								},							},
	{	.gpio	= 98,		/* UIM1_CLK */
		.settings = {								},							},
	{	.gpio	= 99,		/* UIM1_RST */
		.settings = {								},							},
	{	.gpio	= 100,		/* UIM1_DETECT */
		.settings = {								},							},
	{	.gpio	= 101,		/* BATT_REM_ALARM */
		.settings = {								},							},
	{	.gpio	= 102,		/* HIFI_45M_EN */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 103,		/* FORCE_USB_BOOT */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 104,		/* GRFC_0 */
		.settings = {								},							},
	{	.gpio	= 105,		/* GRFC_1 */
		.settings = {								},							},
	{	.gpio	= 106,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 107,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 108,		/* PA_ON3_B34_B39 */
		.settings = {								},							},
	{	.gpio	= 109,		/* GRFC_5 */
		.settings = {								},							},
	{	.gpio	= 110,		/* RX_ON0 */
		.settings = {								},							},
	{	.gpio	= 111,		/* RF_ON0 */
		.settings = {								},							},
	{	.gpio	= 112,		/* TDS_PA0_R0 */
		.settings = {								},							},
	{	.gpio	= 113,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 114,		/* TM8_SEL0 */
		.settings = {								},							},
	{	.gpio	= 115,		/* TM8_SEL1 */
		.settings = {								},							},
	{	.gpio	= 116,		/* TX_GTR_THRES */
		.settings = {								},							},
	{	.gpio	= 117,		/* RGB_SEL */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[1],		[GPIOMUX_ACTIVE] = &gpio_suspend_config[1], },		},
	{	.gpio	= 118,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 119,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 120,		/* TX4_B7_B41_SEL */
		.settings = {								},							},
	{	.gpio	= 121,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 122,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 123,		/* SP4T_CTRL_2 */
		.settings = {								},							},
	{	.gpio	= 124,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 125,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 126,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 127,		/* SP4T_CTRL_1 */
		.settings = {								},							},
	{	.gpio	= 128,		/* GPS_EXT_LNA_EN */
		.settings = {								},							},
	{	.gpio	= 129,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 130,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 131,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 132,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 133,		/* SSBI1_TX_GNSS */
		.settings = {								},							},
	{	.gpio	= 134,		/* SSBI1_PRX_DRX */
		.settings = {								},							},
	{	.gpio	= 135,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 136,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 137,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 138,		/* GP_DATA1 */
		.settings = {								},							},
	{	.gpio	= 139,		/* GP_DATA0 */
		.settings = {								},							},
	{	.gpio	= 140,		/* RFFE1_CLK */
		.settings = {								},							},
	{	.gpio	= 141,		/* RFFE1_DATA */
		.settings = {								},							},
	{	.gpio	= 142,		/* RFFE2_CLK */
		.settings = {								},							},
	{	.gpio	= 143,		/* RFFE2_DATA */
		.settings = {								},							},
	{	.gpio	= 144,		/* unused */
		.settings = {[GPIOMUX_SUSPENDED] = &gpio_suspend_config[2],		},							},
	{	.gpio	= 145,		/* unused */
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
			/* Handle V5 minor version different here */
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

		msm_gpiomux_debug_init();
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
