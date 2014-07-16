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

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpiomux.h>

/*
 * The drive strength setting for MDIO pins
 * is different from the others
 */
#define MDIO_DRV_8MA	GPIOMUX_DRV_16MA

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

static struct gpiomux_setting mdm_grfc_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ctu_bridge_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ctu_grfc_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm_gpio_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting pdm_func1_config = {
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

struct msm_gpiomux_config fsm_gluon_grfc_configs[] = {
	{
		.gpio      = 82,       /* CTU_GPIO_BRIDGE[48] */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_bridge_config,
		},
	},
	{
		.gpio      = 83,       /* CTU_GPIO_BRIDGE[47] */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_bridge_config,
		},
	},
	{
		.gpio      = 88,       /* TX1_HB_PA_EN / TX1_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_grfc_config,
		},
	},
	{
		.gpio      = 90,       /* TX2_HB_PA_EN / FTR1_DPD_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_grfc_config,
		},
	},
	{
		.gpio      = 92,       /* TX2_LB_PA_EN  / FTR1_DPD_SEL1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_grfc_config,
		},
	},
	{
		.gpio      = 94,       /* TX3_HB_PA_EN / TX3_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_grfc_config,
		},
	},
	{
		.gpio      = 96,       /* TX3_LB_PA_EN / TX4_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_grfc_config,
		},
	},
	{
		.gpio      = 98,       /* TX4_HB_PA_EN / FTR2_DPD_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_grfc_config,
		},
	},
	{
		.gpio      = 101,       /* HB_HDD / SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &ctu_grfc_config,
		},
	},
	{
		.gpio      = 103,       /* FTR1_SEL0 / FTR2_TXRX_SW_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 104,       /* FTR1_SEL1 / FTR1_TXRX_SW_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 105,       /* FTR2_SEL0  / FTR1_RX_ON */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 106,       /* FTR2_SEL1 / FTR1_DPD_ON */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 112,       /* TX1_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 114,       /* TX1_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
};

struct msm_gpiomux_config fsm_mdm_grfc_configs[] = {
	{
		.gpio      = 88,       /* TX1_HB_PA_EN / TX1_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 89,       /* TX1_LB_PA_EN / TX2_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 90,       /* TX2_HB_PA_EN / FTR1_DPD_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 91,       /* TX2_LB_PA_EN  / FTR1_DPD_SEL1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 92,       /* TX3_HB_PA_EN / TX3_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 93,       /* TX3_LB_PA_EN / TX4_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 94,       /* TX4_HB_PA_EN / FTR2_DPD_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 95,       /* TX4_LB_PA_EN  / FTR2_DPD_SEL1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 96,       /* TX3_TX4_HB_PA_EN / FTR1_NL_SW1_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 97,       /* TX3_TX4_LB_PA_EN / FTR1_NL_SW2_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 98,       /* TX1_TX2_HB_PA_EN / FTR2_NL_SW1_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 99,       /* TX1_TX2_LB_PA_EN  / FTR2_NL_SW2_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 100,       /* ANT1_HBLB_FTR_SEL / SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 101,       /* HB_HDD / SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 103,       /* FTR1_SEL0 / FTR2_TXRX_SW_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 104,       /* FTR1_SEL1 / FTR1_TXRX_SW_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 105,       /* FTR2_SEL0  / FTR1_RX_ON */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 106,       /* FTR2_SEL1 / FTR1_DPD_ON */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_gpio_config,
		},
	},
	{
		.gpio      = 109,       /* WTR1605_RX_ON / FTR1_INTERRUPT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 110,       /* WTR1605_RF_ON / FTR2_INTERRUPT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 111,       /* WTR2605_RX_ON  / SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 112,       /* TX1_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 113 ,       /* TX2_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 114,       /* TX3_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 115,       /* TX4_VPA_EN  */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
};

static struct msm_gpiomux_config fsm_pdm_configs[] __initdata = {
	{
		.gpio      = 116,       /* TX1_VPA_CTL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pdm_func1_config,
		},
	},
	{
		.gpio      = 117,       /* TX2_VPA_CTL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pdm_func1_config,
		},
	},
	{
		.gpio      = 118,       /* TX3_VPA_CTL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pdm_func1_config,
		},
	},
	{
		.gpio      = 121,       /* TX4_VPA_CTL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pdm_func1_config,
		},
	},
};

static struct gpiomux_setting uim_config_data_clk = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting uim_config_reset_present = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting uim_config_reset_suspended = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIO_CFG_OUTPUT,
};

static struct msm_gpiomux_config fsm_uim_configs[] __initdata = {
	{
		.gpio      = 24,       /* UIM_DATA */
		.settings = {
			[GPIOMUX_ACTIVE] = &uim_config_data_clk,
		},
	},
	{
		.gpio      = 25,       /* UIM_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &uim_config_data_clk,
		},
	},
	{
		.gpio      = 26,       /* UIM_RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &uim_config_reset_present,
			[GPIOMUX_SUSPENDED] = &uim_config_reset_suspended,
		},
	},
	{
		.gpio      = 27,       /* UIM_PRESENT */
		.settings = {
			[GPIOMUX_ACTIVE] = &uim_config_reset_present,
		},
	},
};

static struct gpiomux_setting pcie_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting pcie_perst_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIO_CFG_OUTPUT,
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
	{
		.gpio      = 29,       /* PCIE1_PERST */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pcie_perst_config,
		},
	},
	{
		.gpio      = 33,       /* PCIE0_PERST */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pcie_perst_config,
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

static struct gpiomux_setting rf_detect_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
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

static struct gpiomux_setting mdio_clk_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = MDIO_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting mdio_data_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = MDIO_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_mdio_configs[] __initdata = {
	{
		.gpio      = 123,       /* GMAC0_MDIO_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdio_clk_config,
		},
	},
	{
		.gpio      = 124,      /* GMAC0_MDIO_DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdio_data_config,
		},
	},
	{
		.gpio      = 125,       /* GMAC1_MDIO_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdio_clk_config,
		},
	},
	{
		.gpio      = 126,      /* GMAC1_MDIO_DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdio_data_config,
		},
	},
};

static struct msm_gpiomux_config fsm_rf_configs[] __initdata = {
	{
		.gpio      = 96,       /* FTR2_HB_NL_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 97,      /* FTR2_LB_NL_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 98,       /* FTR1_HB_NL_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 99,       /* FTR1_LB_NL_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 103,       /* FTR1_SBI_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 104,       /* FTR1_SBI_SEL1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 105,       /* FTR2_SBI_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 106,       /* FTR2_SBI_SEL1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
};

struct msm_gpiomux_config fsm_mtr_configs[] = {
	{
		.gpio      = 88,       /* TX1_HB_PA_EN / TX1_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 89,       /* TX1_LB_PA_EN / TX2_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 90,       /* TX2_HB_PA_EN / FTR1_DPD_SEL1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 91,       /* TX2_LB_PA_EN  / FTR1_DPD_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 92,       /* TX3_HB_PA_EN / TX3_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 93,       /* TX3_LB_PA_EN / TX4_PA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 94,       /* TX4_HB_PA_EN / FTR2_DPD_SEL1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 95,       /* TX4_LB_PA_EN  / FTR2_DPD_SEL0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 96,       /* TX3_TX4_HB_PA_EN / FTR2_NL_SW1_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 97,       /* TX3_TX4_LB_PA_EN / FTR2_NL_SW2_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 98,       /* TX1_TX2_HB_PA_EN / FTR1_NL_SW1_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 99,       /* TX1_TX2_LB_PA_EN  / FTR1_NL_SW2_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 100,       /* ANT1_HBLB_FTR_SEL / SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 101,       /* FTR2_TXRX_SW_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 102,       /* FTR1_TXRX_SW_SEL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 103,       /* HB_TDD / FTR1_RX_ON */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 104,       /* SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 105,       /* FTR2_SEL0  / FTR2_RX_ON */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 106,       /* SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 109,       /* WTR1605_RX_ON / FTR1_INTERRUPT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 110,       /* WTR1605_RF_ON / FTR2_INTERRUPT */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rf_detect_config,
		},
	},
	{
		.gpio      = 111,       /* WTR2605_RX_ON  / SPARE */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 112,       /* TX1_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 113 ,       /* TX2_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 114,       /* TX3_VPA_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
		},
	},
	{
		.gpio      = 115,       /* TX4_VPA_EN  */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm_grfc_config,
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
	msm_gpiomux_install(fsm_mdio_configs, ARRAY_SIZE(fsm_mdio_configs));
	msm_gpiomux_install(fsm_pdm_configs, ARRAY_SIZE(fsm_pdm_configs));
	msm_gpiomux_install(fsm_rf_configs, ARRAY_SIZE(fsm_rf_configs));
}

void fsm9900_gluon_init(void)
{
	msm_gpiomux_install(fsm_gluon_grfc_configs,
			ARRAY_SIZE(fsm_gluon_grfc_configs));
}

void fsm9900_mtr_init(void)
{
	msm_gpiomux_install(fsm_mtr_configs,
			ARRAY_SIZE(fsm_mtr_configs));
}

void fsm9900_rfic_init(void)
{
	msm_gpiomux_install(fsm_mdm_grfc_configs,
			ARRAY_SIZE(fsm_mdm_grfc_configs));
}
