/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define KS8851_IRQ_GPIO 75

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_UP,
	.drv = GPIOMUX_DRV_2MA,
	.func = GPIOMUX_FUNC_GPIO,
};

static struct msm_gpiomux_config msm_eth_config[] = {
	{
		.gpio = KS8851_IRQ_GPIO,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
		}
	},
};
#endif

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_uart_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_smb_stat_int_act_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_smb_stat_int_sus_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting gpio_uart_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_uart_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
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
		.gpio      = 10,	       /* BLSP1 I2C SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 11,	       /* BLSP1 I2C SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 24,	       /* SMB1357 STAT INT GPIO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_smb_stat_int_sus_config,
			[GPIOMUX_SUSPENDED] = &gpio_smb_stat_int_act_config,
		},
	},
	{
		.gpio      = 4,	/* BLSP1 UART2 TX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart_suspend_cfg,
		},
	},
	{
		.gpio      = 5,  /* BLSP1 UART2 RX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart_suspend_cfg,
		},
	},
	{
		.gpio      = 6,  /* BLSP1 UART2 CTS */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart_suspend_cfg,
		},
	},
	{
		.gpio      = 7,  /* BLSP1 UART2 RTS */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_uart_active_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_uart_suspend_cfg,
		},
	},
};

static struct gpiomux_setting gpio_sd_card_vreg_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir  = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config msm_sd_card_configs[] __initdata = {
	{
		.gpio      = 89,		/* SD CARD VREG EN GPIO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_sd_card_vreg_config,
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

static struct msm_gpiomux_config mdm9630_mi2s_configs[] __initdata = {
	{
		.gpio   = 12,		/* mi2s ws */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio   = 15,		/* mi2s sclk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio   = 14,		/* mi2s dout */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio   = 13,		/* mi2s din */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
	{
		.gpio   = 71,		/* mi2s mclk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_active_cfg,
		},
	},
};

static struct msm_gpiomux_config mdm9630_cdc_reset_config[] __initdata = {
	{
		.gpio   = 67,		/* SYS_RST_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &codec_reset,
		},
	}
};

static struct gpiomux_setting gpio_pcie_clkreq_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm_pcie_configs[] __initdata = {
	{
		.gpio = 64,    /* PCIE0_CLKREQ_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_pcie_clkreq_config,
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
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting qpic_lcdc_bl = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config msm9630_qpic_lcdc_configs[] __initdata = {
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
	{
		.gpio      = 84,	/* bl */
		.settings = {
			[GPIOMUX_SUSPENDED] = &qpic_lcdc_bl,
		},
	},
};

static void msm9630_disp_init_gpiomux(void)
{
	msm_gpiomux_install(msm9630_qpic_lcdc_configs,
			ARRAY_SIZE(msm9630_qpic_lcdc_configs));
}
#else
static void msm9630_disp_init_gpiomux(void)
{
}
#endif /* CONFIG_FB_MSM_QPIC */

void __init mdm9630_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));
	msm_gpiomux_install(msm_sd_card_configs,
			ARRAY_SIZE(msm_sd_card_configs));
	msm_gpiomux_install(mdm9630_mi2s_configs,
			ARRAY_SIZE(mdm9630_mi2s_configs));
	msm_gpiomux_install(mdm9630_cdc_reset_config,
			ARRAY_SIZE(mdm9630_cdc_reset_config));
	msm_gpiomux_install(msm_pcie_configs, ARRAY_SIZE(msm_pcie_configs));
#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	msm_gpiomux_install(msm_eth_config, ARRAY_SIZE(msm_eth_config));
#endif
	msm9630_disp_init_gpiomux();
}
