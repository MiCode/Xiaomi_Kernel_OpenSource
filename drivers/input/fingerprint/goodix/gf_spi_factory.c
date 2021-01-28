/* Goodix's GF316M/GF318M/GF3118M/GF518M/GF5118M/GF516M/GF816M/GF3208/
 *  GF5206/GF5216/GF5208
 *  fingerprint sensor linux driver for factory test
 *
 * 2010 - 2015 Goodix Technology.
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
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <net/sock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

/* MTK header */
#ifndef CONFIG_SPI_MT65XX
#include "mtk_spi.h"
#include "mtk_spi_hal.h"
#endif

/* there is no this file on standardized GPIO platform */
#ifdef CONFIG_MTK_GPIO
#include "mach/gpio_const.h"
#endif

#include "gf_spi_tee.h"

#define SPI_CLK_TOTAL_TIME  107

#ifdef CONFIG_SPI_MT65XX
static u32 gf_factory_SPIClk = 1*1000000;
#endif

#ifndef CONFIG_SPI_MT65XX
int  gf_ioctl_spi_init_cfg_cmd(struct mt_chip_conf *mcc, unsigned long arg)
{
	int retval = 0;
#if 0
	gf_spi_cfg_t gf_spi_cfg;
	u32 spi_clk_total_cycle_time;
	u32 spi_duty_cycle;

	/* gf_debug(ERR_LOG, "%s:enter\n", __func__); */
	do {
		if (copy_from_user(&gf_spi_cfg, (gf_spi_cfg_t *)arg,
			sizeof(gf_spi_cfg_t))) {
			gf_debug(ERR_LOG,
				"%s: Failed to copy gf_spi_cfg_t data struct\n",
					__func__);
			retval = -EFAULT;
			break;
		}
		spi_clk_total_cycle_time =
		(uint32_t)(SPI_CLK_TOTAL_TIME / gf_spi_cfg.speed_hz + 1);
		mcc->setuptime = gf_spi_cfg.cs_setuptime;
		mcc->holdtime = gf_spi_cfg.cs_holdtime;
		mcc->cs_idletime = gf_spi_cfg.cs_idletime;
		spi_duty_cycle = gf_spi_cfg.duty_cycle;
		mcc->high_time =
		(uint32_t) (spi_clk_total_cycle_time * spi_duty_cycle / 100);
		mcc->low_time = spi_clk_total_cycle_time - mcc->high_time;
		mcc->cpol = gf_spi_cfg.cpol;
		mcc->cpha = gf_spi_cfg.cpha;
	} while (0);
#endif
	/* gf_debug(ERR_LOG, "%s:exit:%d\n", __func__,retval); */
	return retval;
}
#endif

#if 0
static void gf_spi_setup_conf_factory(struct gf_device *gf_dev, u32 high_time,
	u32 low_time, enum spi_transfer_mode mode)
{
	struct mt_chip_conf *mcc = &gf_dev->spi_mcc;

	/* default set to 1MHz clock */
	mcc->high_time = high_time;
	mcc->low_time = low_time;

	if ((mode == DMA_TRANSFER) || (mode == FIFO_TRANSFER)) {
		mcc->com_mod = mode;
	} else {
		/* default set to FIFO mode */
		mcc->com_mod = FIFO_TRANSFER;
	}

	if (spi_setup(gf_dev->spi))
		gf_debug(ERR_LOG, "%s, failed to setup spi conf\n", __func__);
}
#else

/* gf_spi_setup_conf_ree, configure spi speed and transfer mode in REE mode
 *
 * speed: 1, 4, 6, 8 unit:MHz
 * mode: DMA mode or FIFO mode
 */
 /* non-upstream SPI driver */
#ifndef CONFIG_SPI_MT65XX
void gf_spi_setup_conf_factory(struct gf_device *gf_dev, u32 speed,
					enum spi_transfer_mode mode)
{
	struct mt_chip_conf *mcc = &gf_dev->spi_mcc;

	switch (speed) {
	case 1:
		/* set to 1MHz clock */
		mcc->high_time = 50;
		mcc->low_time = 50;
		break;
	case 4:
		/* set to 4MHz clock */
		mcc->high_time = 15;
		mcc->low_time = 15;
		break;
	case 6:
		/* set to 6MHz clock */
		mcc->high_time = 10;
		mcc->low_time = 10;
		break;
	case 8:
		/* set to 8MHz clock */
		mcc->high_time = 8;
		mcc->low_time = 8;
		break;
	default:
		/* default set to 1MHz clock */
		mcc->high_time = 50;
		mcc->low_time = 50;
	}

	if ((mode == DMA_TRANSFER) || (mode == FIFO_TRANSFER)) {
		mcc->com_mod = mode;
	} else {
		/* default set to FIFO mode */
		mcc->com_mod = FIFO_TRANSFER;
	}

	if (spi_setup(gf_dev->spi))
		gf_debug(ERR_LOG, "%s, failed to setup spi conf\n", __func__);

}
#endif
#endif

static int gf_spi_transfer_raw_ree(struct gf_device *gf_dev, u8 *tx_buf,
					u8 *rx_buf, u32 len)
{
	struct spi_message msg;
	struct spi_transfer xfer;

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(struct spi_transfer));

	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;
	xfer.len = len;
#ifdef CONFIG_SPI_MT65XX
	xfer.speed_hz = gf_factory_SPIClk;
#endif
	spi_message_add_tail(&xfer, &msg);
	spi_sync(gf_dev->spi, &msg);

	return 0;
}

int gf_ioctl_transfer_raw_cmd(struct gf_device *gf_dev, unsigned long arg,
					unsigned int bufsiz)
{
	struct gf_ioc_transfer_raw ioc_xraw;
	int retval = 0;

	do {
		u8 *tx_buf;
		u8 *rx_buf;
		uint32_t len;

		/* gf_debug(ERR_LOG, "%s:enter\n", __func__); */

		if (copy_from_user(&ioc_xraw,
					(struct gf_ioc_transfer_raw *)arg,
					sizeof(struct gf_ioc_transfer_raw))) {
			gf_debug(ERR_LOG,
				"%s: copy_from_user failed\n", __func__);
			retval = -EFAULT;
			break;
		}

	/* gf_debug(ERR_LOG,
	 * "%s:len:%d,read_buf:0x%p,write_buf:%p,high_time:%d,low_time:%d\n",
	 * __func__,ioc_xraw.len,ioc_xraw.read_buf,ioc_xraw.write_buf,
	 * ioc_xraw.high_time,ioc_xraw.low_time);
	 */
		if ((ioc_xraw.len > bufsiz) || (ioc_xraw.len == 0)) {
			gf_debug(ERR_LOG,
				"%s:transfer len larger than max buffer\n",
				__func__);
			retval = -EINVAL;
			break;
		}

		if (ioc_xraw.read_buf == NULL || ioc_xraw.write_buf == NULL) {
			gf_debug(ERR_LOG,
				"%s:buf can't equal NULL simultaneously\n",
				__func__);
			retval = -EINVAL;
			break;
		}

		/* change speed and set transfer mode */
#ifndef CONFIG_SPI_MT65XX
		if (ioc_xraw.len > 32)
			gf_spi_setup_conf_factory(gf_dev,
				ioc_xraw.high_time, DMA_TRANSFER);
		else
			gf_spi_setup_conf_factory(gf_dev,
				ioc_xraw.high_time, FIFO_TRANSFER);
#else
		gf_factory_SPIClk = ioc_xraw.high_time*1000000;
		/* gf_debug(INFO_LOG, "%s, %d, now spi clock:%d\n",
		 * __func__, __LINE__, gf_factory_SPIClk);
		 */
#endif

		len = ioc_xraw.len;
		if (len % 1024 != 0 && len > 1024)
			len = ((ioc_xraw.len / 1024) + 1) * 1024;

		tx_buf = kzalloc(len, GFP_KERNEL);
		if (tx_buf == NULL) {
			/* gf_debug(ERR_LOG,
			 * "%s: failed to allocate raw tx buffer\n", __func__);
			 */
			retval = -EMSGSIZE;
			break;
		}

		rx_buf = kzalloc(len, GFP_KERNEL);
		if (rx_buf == NULL) {
			kfree(tx_buf);
			/* gf_debug(ERR_LOG,
			 * "%s: failed to allocate raw rx buffer\n", __func__);
			 */
			retval = -EMSGSIZE;
			break;
		}

		if (copy_from_user(tx_buf, ioc_xraw.write_buf, ioc_xraw.len)) {
			kfree(tx_buf);
			kfree(rx_buf);
			gf_debug(ERR_LOG,
				"gf_ioc_transfer copy_from_user failed\n");
			/* gf_debug(ERR_LOG,
			 * "%s:copy gf_ioc_transfer from user to kernel failed
			 * tx_buf:0x%p,write_buf:0x%p,len:%d\n",
			 *  __func__,tx_buf, ioc_xraw.write_buf, ioc_xraw.len);
			 */
			retval = -EFAULT;
			break;
		}

		gf_spi_transfer_raw_ree(gf_dev, tx_buf, rx_buf, len);

		if (copy_to_user(ioc_xraw.read_buf, rx_buf, ioc_xraw.len)) {
			gf_debug(ERR_LOG,
				"gf_ioc_transfer_raw copy_to_user failed\n");
			/* gf_debug(ERR_LOG,
			 * "gf_ioc_transfer_raw copy_to_user:0x%p\n",
			 * ioc_xraw.read_buf);
			 */
			retval = -EFAULT;
		}

		kfree(tx_buf);
		kfree(rx_buf);
	} while (0);

	/* gf_debug(ERR_LOG, "%s:exit:%d\n", __func__,retval); */
	return retval;
}

