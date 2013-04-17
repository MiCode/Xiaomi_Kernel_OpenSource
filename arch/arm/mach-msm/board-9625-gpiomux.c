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

#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>

static struct gpiomux_setting gpio_uart_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting gpio_spi_cs_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
	{
		.gpio      = 4,		/* BLSP1 QUP2 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 5,		/* BLSP1 QUP2 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 6,		/* BLSP1 QUP2 SPI_CS_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_cs_config,
		},
	},
	{
		.gpio      = 7,		/* BLSP1 QUP2 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 8,	       /* BLSP1 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 9,	       /* BLSP1 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 10,		/* BLSP1 QUP3 I2C_DAT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 11,		/* BLSP1 QUP3 I2C_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
};

static struct gpiomux_setting  mi2s_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting  mi2s_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting codec_reset = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config mdm9625_mi2s_configs[] __initdata = {
	{
		.gpio	= 12,		/* mi2s ws */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio	= 15,		/* mi2s sclk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio	= 14,		/* mi2s dout */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio	= 13,		/* mi2s din */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio	= 71,		/* mi2s mclk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
};

static struct msm_gpiomux_config mdm9625_cdc_reset_config[] __initdata = {
	{
		.gpio   = 22,           /* SYS_RST_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &codec_reset,
		},
	}
};

static struct gpiomux_setting sdc3_clk_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc3_cmd_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc3_data_0_3_active_cfg = {
	.func = GPIOMUX_FUNC_6,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc3_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc3_data_1_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config sdc3_configs[] __initdata = {
	{
		.gpio      = 25,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc3_clk_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspended_cfg,
		},
	},
	{
		.gpio      = 24,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc3_cmd_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspended_cfg,
		},

	},
	{
		.gpio      = 16,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc3_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspended_cfg,
		},
	},
	{
		.gpio      = 17,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc3_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_data_1_suspended_cfg,
		},
	},
	{
		.gpio      = 18,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc3_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspended_cfg,
		},
	},
	{
		.gpio      = 19,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc3_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspended_cfg,
		},
	},
};

static struct gpiomux_setting wlan_ath6kl_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting wlan_ath6kl_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config wlan_ath6kl_configs[] __initdata = {
	{
		.gpio      = 62,/* CHIP_PWD_L */
		.settings = {
			[GPIOMUX_ACTIVE] = &wlan_ath6kl_active_config,
			[GPIOMUX_SUSPENDED] = &wlan_ath6kl_suspend_config,
		},
	},
};

static struct gpiomux_setting sdc2_card_det_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

struct msm_gpiomux_config sdc2_card_det_config[] __initdata = {
	{
		.gpio = 66,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc2_card_det_cfg,
			[GPIOMUX_SUSPENDED] = &sdc2_card_det_cfg,
		},
	},
};

#ifdef CONFIG_FB_MSM_QPIC
static struct gpiomux_setting qpic_lcdc_a_d = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting qpic_lcdc_cs = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting qpic_lcdc_rs = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting qpic_lcdc_te = {
	.func = GPIOMUX_FUNC_7,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm9625_qpic_lcdc_configs[] __initdata = {
	{
		.gpio      = 20,	/* a_d */
		.settings = {
			[GPIOMUX_SUSPENDED] = &qpic_lcdc_a_d,
		},
	},
	{
		.gpio      = 21,	/* cs */
		.settings = {
			[GPIOMUX_SUSPENDED] = &qpic_lcdc_cs,
		},
	},
	{
		.gpio      = 22,	/* te */
		.settings = {
			[GPIOMUX_SUSPENDED] = &qpic_lcdc_te,
		},
	},
	{
		.gpio      = 23,	/* rs */
		.settings = {
			[GPIOMUX_SUSPENDED] = &qpic_lcdc_rs,
		},
	},
};

static void msm9625_disp_init_gpiomux(void)
{
	msm_gpiomux_install(msm9625_qpic_lcdc_configs,
			ARRAY_SIZE(msm9625_qpic_lcdc_configs));
}
#else
static void msm9625_disp_init_gpiomux(void)
{
}
#endif /* CONFIG_FB_MSM_QPIC */

void __init msm9625_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));
	msm_gpiomux_install(sdc3_configs, ARRAY_SIZE(sdc3_configs));
	msm_gpiomux_install(wlan_ath6kl_configs,
		ARRAY_SIZE(wlan_ath6kl_configs));
	msm_gpiomux_install(mdm9625_mi2s_configs,
			ARRAY_SIZE(mdm9625_mi2s_configs));
	msm_gpiomux_install(mdm9625_cdc_reset_config,
			ARRAY_SIZE(mdm9625_cdc_reset_config));
	msm_gpiomux_install(sdc2_card_det_config,
		ARRAY_SIZE(sdc2_card_det_config));
	msm9625_disp_init_gpiomux();
}
