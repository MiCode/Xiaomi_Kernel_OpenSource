/*
 * mt6795_evb_fingerprint.c - fingerprint driver  for mtk6795 evb board
 *
 * Copyright (C) 2015 
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/module.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/types.h>


//#include <mach/mt_gpio.h>
#ifndef CONFIG_ARM64 //MT6795
#include <mach/mt_reg_base.h>
#include <mach/irqs.h>
#endif

#include <mach/mt_typedefs.h>
//#include <mach/board.h>
#include <mach/eint.h>


#include <cust_eint.h>
#include <cust_gpio_usage.h>
#include <mach/mt_spi.h>


#ifdef CONFIG_MTK_SPI
struct mt_chip_conf spi0_ctrdata[] = {
	[0] = {
		.setuptime = 10,
		.holdtime = 10,
		/*
		* These two determine the SPI_CLK frequency
		* SPI_CLK = 134300 / (high_time + low_time), unit KHz
		* To set 25MHz, set each to 3, then is 22.383 MHz
		* To set 10MHz, set each to 7, then is 9.592 MHz
		* To set 2MHz, set each to 33, then is 2.034 M
		*/
		.high_time = 50,
		.low_time = 50,
		.cs_idletime = 10,
		.ulthgh_thrsh = 0,
		/*
		* SPI_MODE_0
		*/
		.cpol = SPI_CPOL_0,
		.cpha = SPI_CPHA_0,
		/*
		* The following determines MSB
		*/
		.rx_mlsb = SPI_MSB,
		.tx_mlsb = SPI_MSB,
		/*
		* Not change the endian of data from mem
		*/
		.tx_endian = SPI_LENDIAN,
		.rx_endian = SPI_LENDIAN,

		//.com_mod = DMA_TRANSFER,
		.com_mod = FIFO_TRANSFER,
		
		.pause = 0,
		.finish_intr = 1,
		.deassert = 0,
		.ulthigh = 0,
		.tckdly = 0,
	},
	
	[1] = {
		.setuptime = 10,
		.holdtime = 10,
		.high_time = 50, //15--3m	25--2m	 50--1m  [ 100--0.5m]
		.low_time = 50,
		.cs_idletime = 10,
		.ulthgh_thrsh = 0,

		/*
		* SPI_MODE_0
		*/
		.cpol = SPI_CPOL_0,
		.cpha = SPI_CPHA_0,

		/*
		* The following determines MSB
		*/
		.rx_mlsb = SPI_MSB,
		.tx_mlsb = SPI_MSB,

		/*
		* Not change the endian of data from mem
		*/
		.tx_endian = SPI_LENDIAN,
		.rx_endian = SPI_LENDIAN,

		.com_mod = FIFO_TRANSFER,
		//.com_mod = DMA_TRANSFER,

		.pause = 0,
		.finish_intr = 1,
		.deassert = 0,
		.ulthigh = 0,
		.tckdly = 0,
	},
};

static struct spi_board_info spi_fp_board_info[] __initdata= {
	[0] = {
		.modalias		= "fp_spi",
		.platform_data		= NULL,
		.bus_num		= 0,
		.chip_select		=1,
		.controller_data	= &spi0_ctrdata[1],
	},
	[1] = {
        .modalias="fpc1020",
		.bus_num = 0,
		.chip_select=0,
		.mode = SPI_MODE_0,
	},
};
#endif

/*
* Init fingerprint subsystem gpios.
*/
static void __init mt6795_evb_fingerprint_init_gpio(void)
{
#if 0
	/*set spi gpio*/	
	mt_set_gpio_mode(GPIO_FP_SPICLK_PIN, GPIO_MODE_01);
	mt_set_gpio_pull_select(GPIO_FP_SPICLK_PIN, GPIO_PULL_UP);	
	mt_set_gpio_pull_enable(GPIO_FP_SPICLK_PIN, GPIO_PULL_ENABLE);
	
	mt_set_gpio_mode(GPIO_FP_SPIMISO_PIN, GPIO_MODE_01);
	mt_set_gpio_pull_select(GPIO_FP_SPIMISO_PIN, GPIO_PULL_UP);	
	mt_set_gpio_pull_enable(GPIO_FP_SPIMISO_PIN, GPIO_PULL_DISABLE);
	
	mt_set_gpio_mode(GPIO_FP_SPIMOSI_PIN, GPIO_MODE_01);
	mt_set_gpio_pull_select(GPIO_FP_SPIMOSI_PIN, GPIO_PULL_UP);	
	mt_set_gpio_pull_enable(GPIO_FP_SPIMOSI_PIN, GPIO_PULL_DISABLE);
	
	mt_set_gpio_mode(GPIO_FP_SPICS_PIN, GPIO_MODE_01);
	mt_set_gpio_pull_select(GPIO_FP_SPIMOSI_PIN, GPIO_PULL_UP);	
	mt_set_gpio_pull_enable(GPIO_FP_SPIMOSI_PIN, GPIO_PULL_ENABLE);
#endif	

}

/*
*
*/
static int __init mt6795_evb_init_fingerprint(void)
{	
	printk("%s(): ++++\n", __func__);

	//return 0;
	
	mt6795_evb_fingerprint_init_gpio();
	
	/* This configures the SPI controller clock. Not done here on MTK */
	spi_register_board_info(spi_fp_board_info, ARRAY_SIZE(spi_fp_board_info));
	
	printk("%s(): ----\n", __func__);
	return 0;
}

arch_initcall(mt6795_evb_init_fingerprint);

MODULE_DESCRIPTION("mt6795 evb board fingerprint driver");
MODULE_AUTHOR("");
MODULE_LICENSE("GPLV2");


