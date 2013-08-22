/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <mach/gpiomux.h>

static struct gpiomux_setting blsp_uart_no_pull_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting blsp_uart_pull_up_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting blsp_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_blsp_configs[] __initdata = {
	{
		.gpio      = 0,	       /* BLSP UART1 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_no_pull_config,
		},
	},
	{
		.gpio      = 1,	       /* BLSP UART1 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_pull_up_config,
		},
	},
	{
		.gpio      = 2,	       /* BLSP I2C SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 3,	       /* BLSP I2C SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 6,	       /* BLSP I2C SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 7,	       /* BLSP I2C SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 36,       /* BLSP UART10 TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_no_pull_config,
		},
	},
	{
		.gpio      = 37,       /* BLSP UART10 RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_pull_up_config,
		},
	},
	{
		.gpio      = 38,       /* BLSP I2C10 SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 39,       /* BLSP I2C10 SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},

};

static struct gpiomux_setting geni_func4_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting geni_func5_config = {
	.func = GPIOMUX_FUNC_5,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config fsm_geni_configs[] __initdata = {
	{
		.gpio      = 8,	       /* GENI7 DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 9,	       /* GENI1 DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 10,       /* GENI2 DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 11,       /* GENI7 CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 20,       /* GENI3 DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 21,       /* GENI4 DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 22,       /* GENI6 DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 23,       /* GENI6 CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 30,       /* GENI5 DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 31,       /* GENI5 CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},

};

static struct gpiomux_setting dan_spi_func4_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting dan_spi_func1_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_dan_spi_configs[] __initdata = {
	{
		.gpio      = 12,       /* BLSP DAN0 SPI_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 13,       /* BLSP DAN0 SPI_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 14,       /* BLSP DAN0 SPI_CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 15,       /* BLSP DAN0 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 16,       /* BLSP DAN1 SPI_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 17,       /* BLSP DAN1 SPI_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 18,       /* BLSP DAN1 SPI_CS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 19,       /* BLSP DAN1 SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 81,       /* BLSP DAN1 SPI_CS0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func1_config,
		},
	},
	{
		.gpio      = 82,       /* BLSP DAN1 SPI_CS1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func1_config,
		},
	},
};

static struct gpiomux_setting uim_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_uim_configs[] __initdata = {
	{
		.gpio      = 24,       /* UIM_DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
	{
		.gpio      = 25,       /* UIM_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
	{
		.gpio      = 26,       /* UIM_RESET */
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
	{
		.gpio      = 27,       /* UIM_PRESENT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
};

static struct gpiomux_setting pcie_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_pcie_configs[] __initdata = {
	{
		.gpio      = 28,       /* BLSP PCIE1_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pcie_config,
		},
	},
	{
		.gpio      = 32,       /* BLSP PCIE0_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pcie_config,
		},
	},
};

static struct gpiomux_setting pps_out_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting pps_in_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gps_clk_in_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gps_nav_tlmm_blank_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct msm_gpiomux_config fsm_gps_configs[] __initdata = {
	{
		.gpio      = 40,       /* GPS_PPS_OUT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pps_out_config,
		},
	},
	{
		.gpio      = 41,       /* GPS_PPS_IN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pps_in_config,
		},
	},
	{
		.gpio      = 43,       /* GPS_CLK_IN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gps_clk_in_config,
		},
	},
	{
		.gpio      = 120,      /* GPS_NAV_TLMM_BLANK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gps_nav_tlmm_blank_config,
		},
	},
};

static struct gpiomux_setting sd_detect_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sd_wp_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_sd_configs[] __initdata = {
	{
		.gpio      = 42,       /* SD_CARD_DET */
		.settings = {
			[GPIOMUX_SUSPENDED] = &sd_detect_config,
		},
	},
	{
		.gpio      = 122,      /* BLSP SD WRITE PROTECT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &sd_wp_config,
		},
	},
};

void __init fsm9900_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	msm_gpiomux_install(fsm_blsp_configs, ARRAY_SIZE(fsm_blsp_configs));
	msm_gpiomux_install(fsm_geni_configs, ARRAY_SIZE(fsm_geni_configs));
	msm_gpiomux_install(fsm_dan_spi_configs,
			    ARRAY_SIZE(fsm_dan_spi_configs));
	msm_gpiomux_install(fsm_uim_configs, ARRAY_SIZE(fsm_uim_configs));
	msm_gpiomux_install(fsm_pcie_configs, ARRAY_SIZE(fsm_pcie_configs));
	msm_gpiomux_install(fsm_gps_configs, ARRAY_SIZE(fsm_gps_configs));
	msm_gpiomux_install(fsm_sd_configs, ARRAY_SIZE(fsm_sd_configs));
}
