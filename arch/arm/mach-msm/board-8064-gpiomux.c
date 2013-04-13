/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include "devices.h"
#include "board-8064.h"

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_NONE,
	.drv = GPIOMUX_DRV_8MA,
	.func = GPIOMUX_FUNC_GPIO,
};
#endif

/* The SPI configurations apply to GSBI 5*/
static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

/* The SPI configurations apply to GSBI 5 chip select 2*/
static struct gpiomux_setting gpio_spi_cs2_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

/* Chip selects for SPI clients */
static struct gpiomux_setting gpio_spi_cs_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_UP,
};

/* Chip selects for EPM SPI clients */
static struct gpiomux_setting gpio_epm_spi_cs_config = {
	.func = GPIOMUX_FUNC_6,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_UP,
};

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
struct msm_gpiomux_config apq8064_ethernet_configs[] = {
	{
		.gpio = 43,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
			[GPIOMUX_ACTIVE] = &gpio_eth_config,
		}
	},
};
#endif

#ifdef CONFIG_MSM_VCAP
static struct gpiomux_setting gpio_vcap_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

struct msm_gpiomux_config vcap_configs[] = {
	{
		.gpio = 20,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 23,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 19,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 22,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 21,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 12,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 11,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 10,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 9,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 26,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 8,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 7,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 6,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 86,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 84,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 5,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 4,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 2,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 87,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
	{
		.gpio = 13,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config,
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config,
		}
	},
};
#endif

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_i2c_2ma_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};


static struct gpiomux_setting gpio_i2c_config_sus = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct gpiomux_setting mbhc_hs_detect = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cdc_mclk = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting audio_auxpcm[] = {
/* Suspended state */
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
/* Active state */
	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
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


static struct gpiomux_setting slimbus = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct gpiomux_setting gsbi1_uart_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi2_uart_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi4_uart_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ext_regulator_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting gsbi7_func1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi7_func2_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi3_suspended_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct gpiomux_setting gsbi3_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
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

static struct gpiomux_setting hdmi_active_3_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hdmi_active_4_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting gsbi5_suspended_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5_uart_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi6_spi_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sx150x_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sx150x_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

#ifdef CONFIG_USB_EHCI_MSM_HSIC
static struct gpiomux_setting cyts_sleep_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cyts_sleep_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cyts_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting cyts_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config cyts_gpio_configs[] __initdata = {
	{	/* TS INTERRUPT */
		.gpio = 6,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cyts_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &cyts_int_sus_cfg,
		},
	},
	{	/* TS SLEEP */
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cyts_sleep_act_cfg,
			[GPIOMUX_SUSPENDED] = &cyts_sleep_sus_cfg,
		},
	},
};
static struct msm_gpiomux_config cyts_gpio_alt_config[] __initdata = {
	{	/* TS INTERRUPT */
		.gpio = 6,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cyts_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &cyts_int_sus_cfg,
		},
	},
	{	/* TS SLEEP */
		.gpio = 12,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cyts_sleep_act_cfg,
			[GPIOMUX_SUSPENDED] = &cyts_sleep_sus_cfg,
		},
	},
};

static struct gpiomux_setting hsic_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting hsic_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting hsic_wakeup_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hsic_wakeup_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config apq8064_hsic_configs[] = {
	{
		.gpio = 88,               /*HSIC_STROBE */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 89,               /* HSIC_DATA */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 47,              /* wake up */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_wakeup_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_wakeup_sus_cfg,
		},
	},
};
#endif

static struct gpiomux_setting mxt_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mxt_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting mxt_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mxt_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config apq8064_hdmi_configs[] __initdata = {
	{
		.gpio = 69,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 70,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 71,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 72,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config apq8064_mhl_configs[] __initdata = {
	{
		.gpio = 30,
		.settings = {
		[GPIOMUX_ACTIVE]    = &hdmi_active_3_cfg,
		[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 35,
		.settings = {
		[GPIOMUX_ACTIVE]    = &hdmi_active_4_cfg,
		[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config apq8064_gsbi_configs[] __initdata = {
	{
		.gpio      = 8,			/* GSBI3 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi3_active_cfg,
		},
	},
	{
		.gpio      = 9,			/* GSBI3 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi3_active_cfg,
		},
	},
	{
		.gpio      = 18,		/* GSBI1 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi1_uart_config,
		},
	},
	{
		.gpio      = 19,		/* GSBI1 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi1_uart_config,
		},
	},
	{
		.gpio      = 51,		/* GSBI5 QUP SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 52,		/* GSBI5 QUP SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 53,		/* Funny CS0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 31,		/* GSBI5 QUP SPI_CS2_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_cs2_config,
		},
	},
	{
		.gpio      = 54,		/* GSBI5 QUP SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 30,		/* FP CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_cs_config,
		},
	},
	{
		.gpio      = 53,		/* NOR CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_cs_config,
		},
	},
	{
		.gpio      = 82,	/* GSBI7 UART2 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi7_func2_cfg,
		},
	},
	{
		.gpio      = 83,	/* GSBI7 UART2 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi7_func1_cfg,
		},
	},
};

static struct msm_gpiomux_config fsm8064_ep_gsbi_configs[] __initdata = {
	{
		.gpio      = 10,	/* GSBI4 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4_uart_config,
		},
	},
	{
		.gpio      = 11,	/* GSBI4 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4_uart_config,
		},
	},
	{
		.gpio      = 18,	/* GSBI1 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi1_uart_config,
		},
	},
	{
		.gpio      = 19,	/* GSBI1 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi1_uart_config,
		},
	},
	{
		.gpio      = 22,	/* GSBI2 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi2_uart_config,
		},
	},
	{
		.gpio      = 23,	/* GSBI7 UART2 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi2_uart_config,
		},
	},
	{
		.gpio      = 51,	/* GSBI5 QUP SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 52,	/* GSBI5 QUP SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 53,	/* Funny CS0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 54,	/* GSBI5 QUP SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 53,	/* NOR CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_cs_config,
		},
	},
};

static struct msm_gpiomux_config apq8064_non_mi2s_gsbi_configs[] __initdata = {
	{
		.gpio      = 32,		/* EPM CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_epm_spi_cs_config,
		},
	},
};

static struct msm_gpiomux_config apq8064_gsbi1_i2c_2ma_configs[] __initdata = {
	{
		.gpio      = 21,		/* GSBI1 QUP I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config_sus,
			[GPIOMUX_ACTIVE] = &gpio_i2c_2ma_config,
		},
	},
	{
		.gpio      = 20,		/* GSBI1 QUP I2C_DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config_sus,
			[GPIOMUX_ACTIVE] = &gpio_i2c_2ma_config,
		},
	},
};

static struct msm_gpiomux_config apq8064_gsbi1_i2c_8ma_configs[] __initdata = {
	{
		.gpio      = 21,		/* GSBI1 QUP I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config_sus,
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 20,		/* GSBI1 QUP I2C_DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config_sus,
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
		},
	},
};

static struct msm_gpiomux_config apq8064_slimbus_config[] __initdata = {
	{
		.gpio   = 40,           /* slimbus clk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
	{
		.gpio   = 41,           /* slimbus data */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
};

static struct gpiomux_setting spkr_i2s = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct msm_gpiomux_config mpq8064_spkr_i2s_config[] __initdata = {
	{
		.gpio   = 47,           /* spkr i2c sck */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spkr_i2s,
		},
	},
	{
		.gpio   = 48,           /* spkr_i2s_ws */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spkr_i2s,
		},
	},
	{
		.gpio   = 49,           /* spkr_i2s_dout */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spkr_i2s,
		},
	},
	{
		.gpio   = 50,           /* spkr_i2s_mclk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spkr_i2s,
		},
	},
};

static struct msm_gpiomux_config apq8064_audio_codec_configs[] __initdata = {
	{
		.gpio = 38,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mbhc_hs_detect,
		},
	},
	{
		.gpio = 39,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_mclk,
		},
	},
};

static struct msm_gpiomux_config mpq8064_audio_auxpcm_configs[] __initdata = {
	{
		.gpio = 43,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
	{
		.gpio = 44,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
	{
		.gpio = 45,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
	{
		.gpio = 46,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
};

/* External 3.3 V regulator enable */
static struct msm_gpiomux_config apq8064_ext_regulator_configs[] __initdata = {
	{
		.gpio = APQ8064_EXT_3P3V_REG_EN_GPIO,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ext_regulator_config,
		},
	},
};

static struct gpiomux_setting ap2mdm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_errfatal_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_pblrdy = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ap2mdm_soft_reset_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ap2mdm_wakeup = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config mdm_configs[] __initdata = {
	/* AP2MDM_STATUS */
	{
		.gpio = 48,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* MDM2AP_STATUS */
	{
		.gpio = 49,
		.settings = {
			[GPIOMUX_ACTIVE] = &mdm2ap_status_cfg,
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	/* MDM2AP_ERRFATAL */
	{
		.gpio = 19,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	/* AP2MDM_ERRFATAL */
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_SOFT_RESET, aka AP2MDM_PON_RESET_N */
	{
		.gpio = 27,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_soft_reset_cfg,
		}
	},
	/* AP2MDM_WAKEUP */
	{
		.gpio = 35,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_wakeup,
		}
	},
	/* MDM2AP_PBL_READY*/
	{
		.gpio = 46,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_pblrdy,
		}
	},
};

static struct msm_gpiomux_config mdm_i2s_configs[] __initdata = {
	/* AP2MDM_STATUS */
	{
		.gpio = 48,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* MDM2AP_STATUS */
	{
		.gpio = 49,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	/* MDM2AP_ERRFATAL */
	{
		.gpio = 19,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	/* AP2MDM_ERRFATAL */
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_SOFT_RESET, aka AP2MDM_PON_RESET_N */
	{
		.gpio = 0,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_soft_reset_cfg,
		}
	},
	/* AP2MDM_WAKEUP */
	{
		.gpio = 44,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_wakeup,
		}
	},
	/* MDM2AP_PBL_READY*/
	{
		.gpio = 81,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_pblrdy,
		}
	},
};

static struct gpiomux_setting i2s_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting i2s_act_func_2_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting i2s_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config mpq8064_mi2s_configs[] __initdata = {
	{
		.gpio	= 27,		/* mi2s ws */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 28,		/* mi2s sclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 29,		/* mi2s dout3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 30,		/* mi2s dout2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},

	{
		.gpio	= 31,		/* mi2s dout1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 32,		/* mi2s dout0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},

	{
		.gpio	= 33,		/* mi2s mclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config apq8064_mi2s_configs[] __initdata = {
	{
		.gpio	= 27,		/* mi2s ws */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 28,		/* mi2s sclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 29,		/* mi2s dout3 - TX*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 32,		/* mi2s dout0 - RX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},

	{
		.gpio	= 33,		/* mi2s mclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config apq8064_mic_i2s_configs[] __initdata = {
	{
		.gpio	= 35,		/* mic i2s sclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 36,		/* mic i2s ws */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 37,		/* mic i2s din0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
};


static struct msm_gpiomux_config apq8064_spkr_i2s_configs[] __initdata = {
	{
		.gpio	= 40,		/* spkr i2s sclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_func_2_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 41,		/* spkr i2s dout */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_func_2_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 42,		/* spkr i2s ws */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
};


static struct msm_gpiomux_config apq8064_mxt_configs[] __initdata = {
	{	/* TS INTERRUPT */
		.gpio = 6,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mxt_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &mxt_int_sus_cfg,
		},
	},
	{	/* TS RESET */
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mxt_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &mxt_reset_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config wcnss_5wire_interface[] = {
	{
		.gpio = 64,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 65,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 66,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 67,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 68,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config mpq8064_gsbi5_i2c_configs[] __initdata = {
	{
		.gpio      = 53,			/* GSBI5 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi5_active_cfg,
		},
	},
	{
		.gpio      = 54,			/* GSBI5 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi5_active_cfg,
		},
	},
};

static struct gpiomux_setting ir_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ir_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config mpq8064_ir_configs[] __initdata = {
	{
		.gpio      = 88,			/* GPIO IR */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ir_suspended_cfg,
			[GPIOMUX_ACTIVE] = &ir_active_cfg,
		},
	},
};

static struct msm_gpiomux_config sx150x_int_configs[] __initdata = {
	{
		.gpio      = 81,
		.settings = {
			[GPIOMUX_SUSPENDED] = &sx150x_suspended_cfg,
			[GPIOMUX_ACTIVE] = &sx150x_active_cfg,
		},
	},
};

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static struct gpiomux_setting sdc2_clk_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc2_cmd_data_0_3_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc2_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc2_data_1_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config apq8064_sdc2_configs[] __initdata = {
	{
		.gpio      = 59,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc2_clk_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc2_suspended_cfg,
		},
	},
	{
		.gpio      = 57,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc2_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc2_suspended_cfg,
		},

	},
	{
		.gpio      = 62,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc2_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc2_suspended_cfg,
		},
	},
	{
		.gpio      = 61,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc2_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc2_data_1_suspended_cfg,
		},
	},
	{
		.gpio      = 60,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc2_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc2_suspended_cfg,
		},
	},
	{
		.gpio      = 58,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc2_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc2_suspended_cfg,
		},
	},
};
#endif


#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct gpiomux_setting sdc4_clk_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc4_cmd_data_0_3_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc4_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc4_data_1_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config apq8064_sdc4_configs[] __initdata = {
	{
		.gpio      = 68,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc4_clk_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspended_cfg,
		},
	},
	{
		.gpio      = 67,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc4_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspended_cfg,
		},

	},
	{
		.gpio      = 66,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc4_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspended_cfg,
		},
	},
	{
		.gpio      = 65,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc4_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_data_1_suspended_cfg,
		},
	},
	{
		.gpio      = 64,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc4_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspended_cfg,
		},
	},
	{
		.gpio      = 63,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc4_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspended_cfg,
		},
	},
};
#endif

static struct gpiomux_setting apq8064_sdc3_card_det_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config apq8064_sdc3_configs[] __initdata = {
	{
		.gpio      = 26,
		.settings = {
			[GPIOMUX_SUSPENDED] = &apq8064_sdc3_card_det_cfg,
			[GPIOMUX_ACTIVE] = &apq8064_sdc3_card_det_cfg,
		},
	},
};

static struct gpiomux_setting gsbi6_uartdm_active = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi6_uartdm_suspended = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config mpq8064_uartdm_configs[] __initdata = {
	{ /* UARTDM_TX */
		.gpio      = 14,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gsbi6_uartdm_active,
			[GPIOMUX_SUSPENDED] = &gsbi6_uartdm_suspended,
		},
	},
	{ /* UARTDM_RX */
		.gpio      = 15,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gsbi6_uartdm_active,
			[GPIOMUX_SUSPENDED] = &gsbi6_uartdm_suspended,
		},
	},
	{ /* UARTDM_CTS */
		.gpio      = 16,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gsbi6_uartdm_active,
			[GPIOMUX_SUSPENDED] = &gsbi6_uartdm_suspended,
		},
	},
	{ /* UARTDM_RFR */
		.gpio      = 17,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gsbi6_uartdm_active,
			[GPIOMUX_SUSPENDED] = &gsbi6_uartdm_suspended,
		},
	},
};

static struct msm_gpiomux_config mpq8064_gsbi6_spi_configs[] __initdata = {
	{
		.gpio      = 17,        /* GSBI6_0 SPI CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi6_spi_cfg,
		},
	},
	{
		.gpio      = 16,        /* GSBI6_1 SPI CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi6_spi_cfg,
		},
	},
	{
		.gpio      = 15,        /* GSBI6_2 SPI MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi6_spi_cfg,
		},
	},
	{
		.gpio      = 14,        /* GSBI6_3 SPI_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi6_spi_cfg,
		},
	},
};

static struct msm_gpiomux_config mpq8064_gsbi5_uart_configs[] __initdata = {
	{
		.gpio      = 51,        /* GSBI5 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5_uart_cfg,
		},
	},
	{
		.gpio      = 52,        /* GSBI5 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5_uart_cfg,
		},
	},
};

static struct gpiomux_setting boot_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm8064_ep_boot_configs[] __initdata = {
	{
		.gpio      = 2,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 3,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 4,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 5,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 33,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 34,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 39,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 50,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
	{
		.gpio      = 87,
		.settings = {
			[GPIOMUX_SUSPENDED] = &boot_cfg,
		},
	},
};

static struct gpiomux_setting fsm8064_ep_backup_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config fsm8064_ep_backup_configs[] __initdata = {
	{
		.gpio      = 45,
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_backup_suspended_cfg,
		},
	},
	{
		.gpio      = 46,
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_backup_suspended_cfg,
		},
	},
	{
		.gpio      = 47,
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_backup_suspended_cfg,
		},
	},
	{
		.gpio      = 62,
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_backup_suspended_cfg,
		},
	},
	{
		.gpio      = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_backup_suspended_cfg,
		},
	},
};

static struct gpiomux_setting fsm8064_ep_uim_rst_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting fsm8064_ep_uim_pwr_sel_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config fsm8064_ep_uim_configs[] __initdata = {
	{
		.gpio      = 49,	/* UIM_RST */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_uim_rst_cfg,
		},
	},
	{
		.gpio      = 55,	/* UIM_PWR_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_uim_pwr_sel_cfg,
		},
	},
};

static struct gpiomux_setting fsm8064_ep_sync_input_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm8064_ep_sync_configs[] __initdata = {
	{
		.gpio      = 6,		/* GPSPPSIN_DRSYNC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_sync_input_cfg,
		},
	},
	{
		.gpio      = 7,		/* KRAIT_PPS_INPUT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_sync_input_cfg,
		},
	},
	{
		.gpio      = 8,		/* QDSP_PPS_INPUT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_sync_input_cfg,
		},
	},
	{
		.gpio      = 9,		/* DAN_TTI_INPUT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_sync_input_cfg,
		},
	},
};

static struct gpiomux_setting fsm8064_ep_led_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config fsm8064_ep_led_configs[] __initdata = {
	{
		.gpio      = 58,		/* RED1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 59,		/* GREEN1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 60,		/* RED2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 61,		/* GREEN2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 69,		/* RED3 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 70,		/* GREEN3 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 71,		/* RED4 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 72,		/* GREEN4 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 77,		/* RED5 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
	{
		.gpio      = 80,		/* GREEN5 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsm8064_ep_led_cfg,
		},
	},
};

void __init apq8064_init_gpiomux(void)
{
	int rc;
	int platform_version = socinfo_get_platform_version();

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return;
	}
	if (!(machine_is_mpq8064_hrd()))
		msm_gpiomux_install(wcnss_5wire_interface,
				ARRAY_SIZE(wcnss_5wire_interface));

	if (machine_is_mpq8064_cdp() || machine_is_mpq8064_hrd() ||
		 machine_is_mpq8064_dtv()) {
		msm_gpiomux_install(mpq8064_gsbi5_i2c_configs,
				ARRAY_SIZE(mpq8064_gsbi5_i2c_configs));
		msm_gpiomux_install(mpq8064_gsbi5_uart_configs,
				ARRAY_SIZE(mpq8064_gsbi5_uart_configs));
#ifdef CONFIG_MSM_VCAP
		msm_gpiomux_install(vcap_configs,
				ARRAY_SIZE(vcap_configs));
#endif
		msm_gpiomux_install(sx150x_int_configs,
				ARRAY_SIZE(sx150x_int_configs));
	} else {
		#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
		msm_gpiomux_install(apq8064_ethernet_configs,
				ARRAY_SIZE(apq8064_ethernet_configs));
		#endif

		if (machine_is_fsm8064_ep())
			msm_gpiomux_install(fsm8064_ep_gsbi_configs,
					ARRAY_SIZE(fsm8064_ep_gsbi_configs));
		else
			msm_gpiomux_install(apq8064_gsbi_configs,
					ARRAY_SIZE(apq8064_gsbi_configs));

		if (!(machine_is_apq8064_mtp() &&
		(SOCINFO_VERSION_MINOR(platform_version) == 1)))
			msm_gpiomux_install(apq8064_non_mi2s_gsbi_configs,
				ARRAY_SIZE(apq8064_non_mi2s_gsbi_configs));
	}
	if (machine_is_apq8064_mtp() &&
	(SOCINFO_VERSION_MINOR(platform_version) == 1)) {
			msm_gpiomux_install(apq8064_mic_i2s_configs,
				ARRAY_SIZE(apq8064_mic_i2s_configs));
			msm_gpiomux_install(apq8064_spkr_i2s_configs,
				ARRAY_SIZE(apq8064_spkr_i2s_configs));
			msm_gpiomux_install(apq8064_mi2s_configs,
				ARRAY_SIZE(apq8064_mi2s_configs));
			msm_gpiomux_install(apq8064_gsbi1_i2c_2ma_configs,
				ARRAY_SIZE(apq8064_gsbi1_i2c_2ma_configs));
	} else {
		if (!machine_is_fsm8064_ep()) {
			msm_gpiomux_install(apq8064_slimbus_config,
				ARRAY_SIZE(apq8064_slimbus_config));
		}
	}

	if (!(machine_is_mpq8064_cdp() || machine_is_mpq8064_hrd() ||
		 machine_is_mpq8064_dtv()) && !machine_is_apq8064_mtp()) {
		msm_gpiomux_install(apq8064_gsbi1_i2c_8ma_configs,
				ARRAY_SIZE(apq8064_gsbi1_i2c_8ma_configs));
	}

	msm_gpiomux_install(apq8064_audio_codec_configs,
			ARRAY_SIZE(apq8064_audio_codec_configs));

	if (machine_is_mpq8064_cdp() || machine_is_mpq8064_hrd() ||
		machine_is_mpq8064_dtv()) {

		msm_gpiomux_install(mpq8064_audio_auxpcm_configs,
			ARRAY_SIZE(mpq8064_audio_auxpcm_configs));

		msm_gpiomux_install(mpq8064_spkr_i2s_config,
			ARRAY_SIZE(mpq8064_spkr_i2s_config));
	}

	pr_debug("%s(): audio-auxpcm: Include GPIO configs"
		" as audio is not the primary user"
		" for these GPIO Pins\n", __func__);

	if (machine_is_mpq8064_cdp() || machine_is_mpq8064_hrd() ||
		machine_is_mpq8064_dtv())
		msm_gpiomux_install(mpq8064_mi2s_configs,
			ARRAY_SIZE(mpq8064_mi2s_configs));

	if (!machine_is_mpq8064_hrd() && !machine_is_fsm8064_ep())
		msm_gpiomux_install(apq8064_ext_regulator_configs,
			ARRAY_SIZE(apq8064_ext_regulator_configs));

	if (machine_is_apq8064_mtp()) {
		if (SOCINFO_VERSION_MINOR(platform_version) == 1)
			msm_gpiomux_install(mdm_i2s_configs,
					ARRAY_SIZE(mdm_i2s_configs));
		else
			msm_gpiomux_install(mdm_configs,
					ARRAY_SIZE(mdm_configs));
	}

	if (machine_is_apq8064_mtp()) {
		if (SOCINFO_VERSION_MINOR(platform_version) == 1) {
			msm_gpiomux_install(cyts_gpio_alt_config,
					ARRAY_SIZE(cyts_gpio_alt_config));
		} else {
			msm_gpiomux_install(cyts_gpio_configs,
					ARRAY_SIZE(cyts_gpio_configs));
		}
	}

#ifdef CONFIG_USB_EHCI_MSM_HSIC
	if (machine_is_apq8064_mtp())
		msm_gpiomux_install(apq8064_hsic_configs,
				ARRAY_SIZE(apq8064_hsic_configs));
#endif

	if (machine_is_apq8064_cdp() || machine_is_apq8064_liquid())
		msm_gpiomux_install(apq8064_mxt_configs,
			ARRAY_SIZE(apq8064_mxt_configs));

	if (!machine_is_fsm8064_ep())
		msm_gpiomux_install(apq8064_hdmi_configs,
				ARRAY_SIZE(apq8064_hdmi_configs));

	if (apq8064_mhl_display_enabled())
		msm_gpiomux_install(apq8064_mhl_configs,
				ARRAY_SIZE(apq8064_mhl_configs));

	if (machine_is_mpq8064_cdp()) {
		msm_gpiomux_install(mpq8064_ir_configs,
				ARRAY_SIZE(mpq8064_ir_configs));

		msm_gpiomux_install(mpq8064_gsbi6_spi_configs,
				ARRAY_SIZE(mpq8064_gsbi6_spi_configs));
	}

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	 msm_gpiomux_install(apq8064_sdc2_configs,
			     ARRAY_SIZE(apq8064_sdc2_configs));
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	 msm_gpiomux_install(apq8064_sdc4_configs,
			     ARRAY_SIZE(apq8064_sdc4_configs));
#endif

	if (!(machine_is_mpq8064_cdp() || machine_is_mpq8064_hrd() ||
		 machine_is_mpq8064_dtv())) {
		msm_gpiomux_install(apq8064_sdc3_configs,
				ARRAY_SIZE(apq8064_sdc3_configs));
	}
	 if (machine_is_mpq8064_hrd() || machine_is_mpq8064_dtv())
		msm_gpiomux_install(mpq8064_uartdm_configs,
				ARRAY_SIZE(mpq8064_uartdm_configs));

	if (machine_is_fsm8064_ep()) {
		msm_gpiomux_install(fsm8064_ep_boot_configs,
				    ARRAY_SIZE(fsm8064_ep_boot_configs));
		msm_gpiomux_install(fsm8064_ep_backup_configs,
				    ARRAY_SIZE(fsm8064_ep_backup_configs));
		msm_gpiomux_install(fsm8064_ep_uim_configs,
				    ARRAY_SIZE(fsm8064_ep_uim_configs));
		msm_gpiomux_install(fsm8064_ep_sync_configs,
				    ARRAY_SIZE(fsm8064_ep_sync_configs));
		msm_gpiomux_install(fsm8064_ep_led_configs,
				    ARRAY_SIZE(fsm8064_ep_led_configs));
	}
}
