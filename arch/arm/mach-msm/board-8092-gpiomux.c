/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_uart_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting slimbus = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct gpiomux_setting spkr_mi2s_mclk_act_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting spkr_mi2s_mclk_sus_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting spdif_opt_act_cfg = {
	.func = GPIOMUX_FUNC_2,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting spdif_opt_sus_cfg = {
	.func = GPIOMUX_FUNC_2,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ioexp_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ioexp_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config ioexpander_configs[] __initdata = {
	{
		.gpio      = 37,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ioexp_suspend_config,
			[GPIOMUX_ACTIVE] = &ioexp_active_config,
		},
	},
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
	{
		.gpio      = 6,		/* BLSP1 QUP2 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 7,		/* BLSP1 QUP2 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 28,	/* BLSP1 UART5 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 29,	/* BLSP1 UART5 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 12,	/* BLSP2 UART0 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 13,	/* BLSP2 UART0 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 59,	/* BLSP2 UART3 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 60,	/* BLSP2 UART3 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio	   = 14,	/* BLSP2 QUP1 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio	   = 15,	/* BLSP2 QUP1 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 61,	/* BLSP2 QUP4 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 62,	/* BLSP2 QUP4 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 81,	/* BLSP2 QUP5 I2C_SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 82,	/* BLSP2 QUP5 I2C_SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 87,		/* BLSP2 QUP6 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 88,		/* BLSP2 QUP6 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 90,		/* BLSP2 QUP6 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
	{
		.gpio      = 89,		/* BLSP2 QUP6 SPI_CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_spi_config,
		},
	},
};

static struct gpiomux_setting ehci_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ehci_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config msm_ehci_configs[] = {
	{
		.gpio = 0,               /* EHCI HUB RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &ehci_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &ehci_reset_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config mpq8092_slimbus_config[] __initdata = {
	{
		.gpio	= 43,		/* slimbus clk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
	{
		.gpio	= 44,		/* slimbus data */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
};

static struct msm_gpiomux_config mpq8092_spkr_mi2s_mclk_configs[] __initdata = {
	{
		.gpio = 42,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spkr_mi2s_mclk_sus_cfg,
			[GPIOMUX_ACTIVE] = &spkr_mi2s_mclk_act_cfg,
		}
	}
};

static struct msm_gpiomux_config mpq8092_spdif_config[] __initdata = {
	{
		.gpio = 41,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spdif_opt_sus_cfg,
			[GPIOMUX_ACTIVE] = &spdif_opt_act_cfg,
		},
	},
	{
		.gpio = 46,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spdif_opt_sus_cfg,
			[GPIOMUX_ACTIVE] = &spdif_opt_act_cfg,
		},
	},
};

static struct gpiomux_setting hdmi_mux_int_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hdmi_mux_int_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config mpq_hdmi_mux_configs[] __initdata = {
	{
		.gpio = 22, /* hdmi_mux interrupt */
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_mux_int_active_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_mux_int_suspend_cfg,
		},
	},
};

static struct gpiomux_setting hdmi_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_active_cec_cfg = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_ddc_cfg = {
	.func = GPIOMUX_FUNC_5,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_hpd_cfg = {
	.func = GPIOMUX_FUNC_5,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_hdmi_configs[] __initdata = {
	{
		.gpio = 26, /* cec */
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_cec_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 24, /* ddc clk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_ddc_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 27, /* ddc data */
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_ddc_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 25, /* hpd */
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_hpd_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};

static struct gpiomux_setting geni_ir_tx_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting geni_ir_rx_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_geni_ir_configs[] __initdata = {
	{
		.gpio      = 8,       /* GENI_IR_TX */
		.settings = {
			[GPIOMUX_ACTIVE] = &geni_ir_tx_config,
			[GPIOMUX_SUSPENDED] = &geni_ir_tx_config,
		},
	},
	{
		.gpio      = 9,       /* GENI_IR_RX */
		.settings = {
			[GPIOMUX_ACTIVE] = &geni_ir_rx_config,
			[GPIOMUX_SUSPENDED] = &geni_ir_rx_config,
		},
	},
};

void __init mpq8092_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));
	msm_gpiomux_install(msm_ehci_configs, ARRAY_SIZE(msm_ehci_configs));
	msm_gpiomux_install(ioexpander_configs, ARRAY_SIZE(ioexpander_configs));
	msm_gpiomux_install(mpq8092_slimbus_config,
			ARRAY_SIZE(mpq8092_slimbus_config));
	msm_gpiomux_install(mpq8092_spkr_mi2s_mclk_configs,
			ARRAY_SIZE(mpq8092_spkr_mi2s_mclk_configs));
	msm_gpiomux_install(mpq8092_spdif_config,
			ARRAY_SIZE(mpq8092_spdif_config));
	msm_gpiomux_install(mpq_hdmi_mux_configs,
			ARRAY_SIZE(mpq_hdmi_mux_configs));
	msm_gpiomux_install(msm_hdmi_configs, ARRAY_SIZE(msm_hdmi_configs));
	msm_gpiomux_install(msm_geni_ir_configs,
			    ARRAY_SIZE(msm_geni_ir_configs));
}
