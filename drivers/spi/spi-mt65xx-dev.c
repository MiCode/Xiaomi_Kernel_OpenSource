/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/spi/spi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_data/spi-mt65xx.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/clk.h>

#if 0
/* #ifdef CONFIG_TRUSTONIC_TEE_SUPPORT */
#define SPI_TRUSTONIC_TEE_SUPPORT
#endif

#ifdef SPI_TRUSTONIC_TEE_SUPPORT
#include <mobicore_driver_api.h>
#include <tlspi_Api.h>
#endif

static struct spi_device *spi_test;
//u32 speed = 34500000;
u32 speed = 10000000;

struct mtk_spi {
	void __iomem *base;
	void __iomem *peri_regs;
	u32 state;
	int pad_num;
	u32 *pad_sel;
	struct clk *parent_clk, *sel_clk, *spi_clk;
	struct spi_transfer *cur_transfer;
	u32 xfer_len;
	struct scatterlist *tx_sgl, *rx_sgl;
	u32 tx_sgl_len, rx_sgl_len;
	const struct mtk_spi_compatible *dev_comp;
	u32 dram_8gb_offset;
};

static struct mtk_chip_config mtk_test_chip_info = {
	.rx_mlsb = 0,
	.tx_mlsb = 0,
	.cs_pol = 0,
	.sample_sel = 0,

	.cs_setuptime = 0,
	.cs_holdtime = 0,
	.cs_idletime = 0,
};

#define SPI_CFG1_REG                      0x0004
#define SPI_CFG1_CS_IDLE_OFFSET           0
#define SPI_CFG1_GET_TICK_DLY_OFFSET      29

#ifdef SPI_TRUSTONIC_TEE_SUPPORT
#define DEFAULT_HANDLES_NUM (64)
#define MAX_OPEN_SESSIONS (0xffffffff - 1)
/*
 * Trustlet UUID.
 */
u8 spi_uuid[10][16] = {
	{0x09, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00},
	{0x09, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00} };

static struct mc_session_handle secspi_session = { 0 };

static u32 secspi_session_ref;
static u32 secspi_devid = MC_DEVICE_ID_DEFAULT;
static tciSpiMessage_t *secspi_tci;

static DEFINE_MUTEX(secspi_lock);

int secspi_session_open(u32 spinum)
{
	enum mc_result mc_ret = MC_DRV_OK;

	mutex_lock(&secspi_lock);


	pr_info("%s() start\n", __func__);
	do {
		/* sessions reach max numbers ? */

		if (secspi_session_ref > MAX_OPEN_SESSIONS) {
			pr_notice("%s() err: secspi_session(0x%x)>MAX(0x%x)\n",
			__func__, secspi_session_ref, MAX_OPEN_SESSIONS);
			break;
		}

		if (secspi_session_ref > 0) {
			secspi_session_ref++;
			break;
		}

		/* open device */
		mc_ret = mc_open_device(secspi_devid);
		if (mc_ret != MC_DRV_OK) {
			pr_notice("%s() mc_open_device failed: %d\n", __func__,
				mc_ret);
			break;
		}

		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(secspi_devid, 0,
			sizeof(tciSpiMessage_t), (uint8_t **) &secspi_tci, 0);
		if (mc_ret != MC_DRV_OK) {
			pr_notice("%s() mc_malloc_wsm failed: %d\n", __func__,
				mc_ret);
			mc_close_device(secspi_devid);
			break;
		}

		/* open session */
		secspi_session.device_id = secspi_devid;
		mc_ret = mc_open_session(&secspi_session,
			(struct mc_uuid_t *)&spi_uuid[spinum][0],
			(uint8_t *)secspi_tci, sizeof(tciSpiMessage_t));
		if (mc_ret != MC_DRV_OK) {
			pr_notice("%s() mc_open_session fail: %d\n", __func__,
				mc_ret);
			mc_free_wsm(secspi_devid, (uint8_t *) secspi_tci);
			mc_close_device(secspi_devid);
			secspi_tci = NULL;
			break;
		}
		secspi_session_ref = 1;

	} while (0);

	mutex_unlock(&secspi_lock);

	if (mc_ret != MC_DRV_OK) {
		pr_notice("%s() Done. Fail! ret=%d, ref=%d\n", __func__,
			mc_ret, secspi_session_ref);
		return -ENXIO;
	}

	pr_info("%s() Done. Success! ret=%d, ref=%d\n", __func__, mc_ret,
				secspi_session_ref);
	return 0;
}

static int secspi_session_close(void)
{
	enum mc_result mc_ret = MC_DRV_OK;

	mutex_lock(&secspi_lock);

	do {
		/* session is already closed ? */
		if (secspi_session_ref == 0) {
			pr_notice("%s() spi_session already closed\n",
				__func__);
			break;
		}

		if (secspi_session_ref > 1) {
			secspi_session_ref--;
			break;
		}

		/* close session */
		mc_ret = mc_close_session(&secspi_session);
		if (mc_ret != MC_DRV_OK) {
			pr_notice("%s() mc_close_session failed: %d\n",
				__func__, mc_ret);
			break;
		}

		/* free WSM for DCI */
		mc_ret = mc_free_wsm(secspi_devid, (uint8_t *) secspi_tci);
		if (mc_ret != MC_DRV_OK) {
			pr_notice("%s() mc_free_wsm failed: %d\n", __func__,
				mc_ret);
			break;
		}
		secspi_tci = NULL;
		secspi_session_ref = 0;

		/* close device */
		mc_ret = mc_close_device(secspi_devid);
		if (mc_ret != MC_DRV_OK)
			pr_notice("%s() mc_close_device failed: %d\n",
				__func__, mc_ret);

	} while (0);

	mutex_unlock(&secspi_lock);

	if (mc_ret != MC_DRV_OK) {
		pr_notice("%s() Done. Fail! ret=%d, ref=%d\n", __func__,
			mc_ret, secspi_session_ref);
		return -ENXIO;
	}

	pr_info("%s() Done. Success! ret=%d, ref=%d\n", __func__, mc_ret,
			secspi_session_ref);
	return 0;

}

void secspi_enable_clk(struct spi_device *spidev)
{
	int ret;
	struct spi_master *master;
	struct mtk_spi *ms;

	master = spidev->master;
	ms = spi_master_get_devdata(master);
	/*
	 * prepare the clock source
	 */
	ret = clk_prepare_enable(ms->spi_clk);
}

int secspi_execute(u32 cmd, tciSpiMessage_t *param)
{
	enum mc_result mc_ret;

	pr_info("%s() start.\n", __func__);
	mutex_lock(&secspi_lock);

	if (secspi_tci == NULL) {
		mutex_unlock(&secspi_lock);
		pr_notice("%s() secspi_tci not exist\n", __func__);
		return -ENODEV;
	}

	/*set transfer data para */
	if (param == NULL) {
		pr_notice("%s() parameter is NULL !!\n", __func__);
	} else {
		secspi_tci->tx_buf = param->tx_buf;
		secspi_tci->rx_buf = param->rx_buf;
		secspi_tci->len = param->len;
		secspi_tci->is_dma_used = param->is_dma_used;
		secspi_tci->tx_dma = param->tx_dma;
		secspi_tci->rx_dma = param->rx_dma;
		secspi_tci->tl_chip_config = param->tl_chip_config;
	}

	secspi_tci->cmd_spi.header.commandId = (tciCommandId_t) cmd;
	secspi_tci->cmd_spi.len = 0;

	/* enable_clock(MT_CG_PERI_SPI0, "spi"); */
	/* enable_clk(ms); */

	mc_ret = mc_notify(&secspi_session);

	if (mc_ret != MC_DRV_OK) {
		pr_notice("%s() mc_notify failed: %d", __func__, mc_ret);
		goto exit;
	}

	mc_ret = mc_wait_notification(&secspi_session, -1);
	if (mc_ret != MC_DRV_OK) {
		pr_notice("%s() SPI mc_wait_notification failed: %d", __func__,
			mc_ret);
		goto exit;
	}

exit:
	mutex_unlock(&secspi_lock);

	if (mc_ret != MC_DRV_OK) {
		pr_notice("%s() Done. Fail. ret:%d\n", __func__, mc_ret);
		return -ENOSPC;
	}

	pr_info("%s() Done. Success. ret:%d\n", __func__, mc_ret);
	return 0;
}

/*used for REE to detach IRQ of TEE*/
void spi_detach_irq_tee(u32 spinum)
{
	secspi_session_open(spinum);
	secspi_execute(2, NULL);
	pr_info("%s() Done.\n", __func__);
}
#endif

void mt_spi_disable_master_clk(struct spi_device *spidev)
{
	struct mtk_spi *ms;

	ms = spi_master_get_devdata(spidev->master);

	clk_disable_unprepare(ms->spi_clk);
}
EXPORT_SYMBOL(mt_spi_disable_master_clk);

void mt_spi_enable_master_clk(struct spi_device *spidev)
{
	int ret;
	struct mtk_spi *ms;

	ms = spi_master_get_devdata(spidev->master);

	ret = clk_prepare_enable(ms->spi_clk);
}
EXPORT_SYMBOL(mt_spi_enable_master_clk);

static int spi_setup_xfer(struct device *dev, struct spi_transfer *xfer,
		u32 len, u32 flag)
{
	u32 tx_buffer = 0x12345678;
	u32 cnt, i;

	xfer->speed_hz = speed;
	pr_info("%s()  flag:%d speed:%d\n", __func__, flag, xfer->speed_hz);

	/* Instead of using kzalloc, if using below
	 * dma_alloc_coherent to allocate tx_dma/rx_dma, remember:
	 * 1. remove kfree in spi_recv_check_all, otherwise KE reboot
	 * 2. set msg.is_dma_mapped = 1 before calling spi_sync();
	 */

	if ((xfer->tx_buf == NULL) || (xfer->rx_buf == NULL))
		return -1;

	cnt = (len%4)?(len/4 + 1):(len/4);

	if (flag == 0)
		for (i = 0; i < cnt; i++)
			*((u32 *)xfer->tx_buf + i) = tx_buffer;
	else if (flag == 1)
		for (i = 0; i < cnt; i++)
			*((u32 *)xfer->tx_buf + i) = tx_buffer + i;
	else
		return -EINVAL;

	return 0;

}

static int spi_recv_check(struct spi_device *spi, struct spi_message *msg)
{
	struct mtk_chip_config *chip_config;
	struct spi_transfer *xfer;
	u32 i, err = 0;
	int j;
	u8 t, rec_cac;

	chip_config = (struct mtk_chip_config *) spi->controller_data;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (!xfer) {
			pr_notice("%s() rv msg is NULL.\n", __func__);
			return -1;
		}

		pr_info("%s xfer->len:%d chip_config->tx_mlsb:%d rx_mlsb:%d\n",
				__func__, xfer->len, chip_config->tx_mlsb,
				chip_config->rx_mlsb);
		for (i = 0; i < xfer->len; i++) {
			if (chip_config->tx_mlsb ^ chip_config->rx_mlsb) {
				rec_cac = 0;
				for (j = 7; j >= 0; j--) {
					t = *((u8 *)xfer->tx_buf + i) & (1<<j);
					t = (t >> j) << (7-j);
					rec_cac |= t;
				}
			} else
				rec_cac = *((u8 *) xfer->tx_buf + i);

			if (*((u8 *) xfer->rx_buf + i) != rec_cac) {
				pr_notice("%s() tx xfer %d is:%x\n", __func__,
					i, *((u8 *) xfer->tx_buf + i));
				pr_notice("%s() rx xfer %d is:%x\n", __func__,
					i, *((u8 *) xfer->rx_buf + i));
				err++;
			}
		}
	}

	pr_info("%s() Done. error %d,actual xfer len:%d\n", __func__, err,
		msg->actual_length);

	return err;
}

static ssize_t spi_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct spi_device *spi;
	struct mtk_spi *mdata = NULL;
	struct mtk_chip_config *chip_config;
	u32 cs_idletime, pad_sel;
	int cpol, cpha, tx_mlsb, rx_mlsb;
	int sample_sel, tckdly, cs_pol;
	u32 reg_val;
#if 0
/* #ifdef CONFIG_TRUSTONIC_TEE_SUPPORT */
	u32 spinum;
#endif

	spi = container_of(dev, struct spi_device, dev);
	pr_info("%s() SPIDEV name is:%s\n", __func__, spi->modalias);

	chip_config = (struct mtk_chip_config *) spi->controller_data;

	if (!chip_config) {
		pr_notice("%s() chip_config is NULL.\n", __func__);
		chip_config = kzalloc(sizeof(struct mtk_chip_config),
			GFP_KERNEL);
		if (!chip_config)
			return -ENOMEM;
	}

	if (!buf) {
		pr_notice("%s() buf is NULL.Exit!\n", __func__);
		goto out;
	}

#if 0
/* #ifdef CONFIG_TRUSTONIC_TEE_SUPPORT */
	if (!strncmp(buf, "send", 4)) {
		if (sscanf(buf + 4, "%d", &spinum) == 1) {
			pr_info("%s() start to access TL SPI driver.\n",
				__func__);
			secspi_session_open(spinum);
			secspi_enable_clk(spi);
			secspi_execute(1, NULL);
			secspi_session_close();
			pr_info("%s() secspi_execute 1 finished!\n", __func__);
		}
	} else if (!strncmp(buf, "config", 6)) {
		if (sscanf(buf + 6, "%d", &spinum) == 1) {
			pr_info("start to access TL SPI driver.\n");
			secspi_session_open(spinum);
			secspi_execute(2, NULL);
			secspi_session_close();
			pr_info("secspi_execute 2 finished!!!\n");
		}
	} else if (!strncmp(buf, "debug", 5)) {
		if (sscanf(buf + 5, "%d", &spinum) == 1) {
			pr_info("start to access TL SPI driver.\n");
			secspi_session_open(spinum);
			secspi_execute(3, NULL);
			secspi_session_close();
			pr_info("secspi_execute 3 finished!!!\n");
		}
	} else if (!strncmp(buf, "test", 4)) {
		if (sscanf(buf + 4, "%d", &spinum) == 1) {
			pr_info("start to access TL SPI driver.\n");
			secspi_session_open(spinum);
			secspi_execute(4, NULL);
			secspi_session_close();
			pr_info("secspi_execute 4 finished!!!\n");
		}
	}
#endif
	else if (!strncmp(buf, "-w", 2))
		goto set;
	else
		goto out;

set:
	buf += 3;
	mdata = spi_master_get_devdata(spi->master);
	mt_spi_enable_master_clk(spi);
	if (!strncmp(buf, "setuptime=", 10))
		pr_info("%s() setuptime cannot be set by cmd.\n", __func__);
	else if (!strncmp(buf, "holdtime=", 9))
		pr_info("%s() holdtime cannot be set by cmd.\n", __func__);
	else if (!strncmp(buf, "high_time=", 10))
		pr_info("%s() high_time cannot be set by cmd.\n", __func__);
	else if (!strncmp(buf, "low_time=", 9))
		pr_info("%s() low_time cannot be set by cmd.\n", __func__);
	else if (!strncmp(buf, "tckdly=", 7)) {
		if (sscanf(buf+7, "%d", &tckdly) == 1) {
			pr_info("%s() set tckdly=%d to SPI_CFG1_REG.\n",
				__func__, tckdly);
			reg_val = readl(mdata->base + SPI_CFG1_REG);
			reg_val &= 0x1FFFFFFF;
			reg_val |= (tckdly << SPI_CFG1_GET_TICK_DLY_OFFSET);
			writel(reg_val, mdata->base + SPI_CFG1_REG);
			reg_val = readl(mdata->base + SPI_CFG1_REG);
			pr_info("%s() now SPI_CFG1_REG=0x%x.\n", __func__,
				reg_val);
		}
	} else if (!strncmp(buf, "cs_idletime=", 12)) {
		if (sscanf(buf+12, "%d", &cs_idletime) == 1) {
			pr_info("%s() set cs_idletime=%d to SPI_CFG1_REG\n",
				__func__, cs_idletime);
			reg_val = readl(mdata->base + SPI_CFG1_REG);
			reg_val &= 0xFFFFFF00;
			reg_val |= (cs_idletime << SPI_CFG1_CS_IDLE_OFFSET);
			writel(reg_val, mdata->base + SPI_CFG1_REG);
			reg_val = readl(mdata->base + SPI_CFG1_REG);
			pr_info("%s() now SPI_CFG1_REG=0x%x.\n", __func__,
				reg_val);
		}
	} else if (!strncmp(buf, "cpol=", 5)) {
		if (sscanf(buf+5, "%d", &cpol) == 1) {
			pr_info("%s() Set cpol=%d to spi->mode.\n", __func__,
				cpol);
			spi->mode &= 0xFD;
			spi->mode |= cpol<<1;
			pr_info("%s() mow spi->mode:0x%.4x\n", __func__,
				spi->mode);
		}
	} else if (!strncmp(buf, "cpha=", 5)) {
		if (sscanf(buf+5, "%d", &cpha) == 1) {
			pr_info("%s() Set cpha=%d to spi->mode.\n", __func__,
				cpha);
			spi->mode &= 0xFE;
			spi->mode |= cpha;
			pr_info("%s() mow spi->mode:0x%.4x\n", __func__,
				spi->mode);
		}
	} else if (!strncmp(buf, "tx_mlsb=", 8)) {
		if (sscanf(buf+8, "%d", &tx_mlsb) == 1) {
			pr_info("%s() Set tx_mlsb=%d to chip_config\n",
				__func__, tx_mlsb);
			chip_config->tx_mlsb = tx_mlsb;
		}
	} else if (!strncmp(buf, "rx_mlsb=", 8)) {
		if (sscanf(buf+8, "%d", &rx_mlsb) == 1) {
			pr_info("%s() Set rx_mlsb=%d to chip_config\n",
				__func__, rx_mlsb);
			chip_config->rx_mlsb = rx_mlsb;
		}
	} else if (!strncmp(buf, "tx_endian=", 10))
		pr_info("%s() tx_endian cannot be set.See __LTTTLE_ENDIAN\n",
			__func__);
	else if (!strncmp(buf, "rx_endian=", 10))
		pr_info("%s() rx_endian cannot be set.See __LTTTLE_ENDIAN\n",
			__func__);
	else if (!strncmp(buf, "pause=", 6))
		pr_info("%s() pause cannot be set due to sw auto select\n",
			__func__);
	else if (!strncmp(buf, "deassert=", 9))
		pr_info("%s() deassert cannot be set, sw default disable\n",
			__func__);
	else if (!strncmp(buf, "sample_sel=", 11)) {
		if (sscanf(buf + 11, "%d", &sample_sel) == 1) {
			pr_info("%s() Set sample_sel=%d to chip_config\n",
				__func__, sample_sel);
			chip_config->sample_sel = sample_sel;
		}
	} else if (!strncmp(buf, "cs_pol=", 7)) {
		if (sscanf(buf + 7, "%d", &cs_pol) == 1) {
			pr_info("%s() Set cs_pol=%d to chip_config\n",
				__func__, cs_pol);
			chip_config->cs_pol = cs_pol;
		}
	} else if (!strncmp(buf, "pad_sel=", 8)) {
		if (sscanf(buf + 8, "%d", &pad_sel) == 1) {
			pr_info("%s() Set pad select=%d to mdata->pad_sel.\n",
				__func__, pad_sel);
			mdata->pad_sel[spi->chip_select] = pad_sel;
		}
	} else if (!strncmp(buf, "speed=", 6)) {
		if (sscanf(buf + 6, "%d", &speed) == 1)
			pr_info("%s() Set speed=%d as global parameter.\n",
				__func__, speed);
	} else {
		pr_notice("%s() Wrong parameters.Do nothing.\n", __func__);
		mt_spi_disable_master_clk(spi);
		goto out;
	}
	mt_spi_disable_master_clk(spi);
out:
	if (!spi->controller_data)
		kfree(chip_config);
	return count;
}

static ssize_t
spi_msg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;
	struct spi_device *spi;

	struct spi_transfer transfer = {0,};
	struct spi_message msg;
	u32 len = 4;

	spi = container_of(dev, struct spi_device, dev);

	if (unlikely(!spi)) {
		pr_notice("%s() spi device is invalid\n", __func__);
		goto out;
	}
	if (unlikely(!buf)) {
		pr_notice("%s() buf is invalid\n", __func__);
		goto out;
	}
	spi_message_init(&msg);

	if (strncmp(buf, "-func", 5)) {
		pr_notice("%s() Wrong parameters.Do nothing.\n", __func__);
		ret = -1;
		goto out;
	}

	buf += 6;
	if (!strncmp(buf, "len=", 4) && 1 == sscanf(buf + 4, "%d", &len)) {
		pr_info("%s() start transfer,dataLen=%d.-----------------\n",
				__func__, len);
		transfer.len = len;
		transfer.tx_buf = kzalloc(len, GFP_KERNEL);
		transfer.rx_buf = kzalloc(len, GFP_KERNEL);
		pr_info("%s() transfer.tx_buf:%p, transfer.rx_buf:%p\n",
				__func__, transfer.tx_buf, transfer.rx_buf);

		spi_setup_xfer(&spi->dev, &transfer, len, 1);
		spi_message_add_tail(&transfer, &msg);
		ret = spi_sync(spi, &msg);
		if (ret < 0) {
			pr_notice("%s() spi_sync err:%d\n", __func__, ret);
		} else {
			ret = spi_recv_check(spi, &msg);
			kfree(transfer.tx_buf);
			kfree(transfer.rx_buf);
			if (ret != 0) {
				ret = -ret;
				pr_notice("%s Message transfer err:%d\n",
					__func__, ret);
				goto out;
			}
		}
		pr_info("%s() spi transfer Done.-----------------\n",
				__func__);
	}

	ret = count;

out:
	return count;
	//return ret;

}

static DEVICE_ATTR(spi, 0200, NULL, spi_store);
static DEVICE_ATTR(spi_msg, 0200, NULL, spi_msg_store);

static struct device_attribute *spi_attribute[] = {
	&dev_attr_spi,
	&dev_attr_spi_msg,
};

static int spi_create_attribute(struct device *dev)
{
	int num, idx;
	int err = 0;

	num = (int)ARRAY_SIZE(spi_attribute);
	for (idx = 0; idx < num; idx++) {
		err = device_create_file(dev, spi_attribute[idx]);
		if (err)
			break;
	}
	return err;

}

static void spi_remove_attribute(struct device *dev)
{
	int num, idx;

	num = (int)ARRAY_SIZE(spi_attribute);
	for (idx = 0; idx < num; idx++)
		device_remove_file(dev, spi_attribute[idx]);
}

static int spi_test_remove(struct spi_device *spi)
{

	pr_info("%s().\n", __func__);
	spi_remove_attribute(&spi->dev);
	return 0;
}

static int spi_test_probe(struct spi_device *spi)
{
	pr_info("%s() enter.\n", __func__);
	spi_test = spi;
	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 8;
	spi->controller_data = (void *)&mtk_test_chip_info;
	return spi_create_attribute(&spi->dev);
}

struct spi_device_id spi_id_table[] = {
	{"spi-ut", 0},
	{},
};

static const struct of_device_id spidev_dt_ids[] = {
	{ .compatible = "mediatek,spi-mt65xx-test" },
	{},
};
MODULE_DEVICE_TABLE(of, spidev_dt_ids);

static struct spi_driver spi_test_driver = {
	.driver = {
		.name = "test_spi",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = spidev_dt_ids,
	},
	.probe = spi_test_probe,
	.remove = spi_test_remove,
	.id_table = spi_id_table,
};

static int __init spi_dev_init(void)
{
	pr_info("%s().\n", __func__);
	return spi_register_driver(&spi_test_driver);
}

static void __exit spi_test_exit(void)
{
	pr_info("%s().\n", __func__);
	spi_unregister_driver(&spi_test_driver);
}

module_init(spi_dev_init);
module_exit(spi_test_exit);

MODULE_DESCRIPTION("mt SPI test device driver");
MODULE_AUTHOR("ZH Chen <zh.chen@mediatek.com>");
MODULE_LICENSE("GPL");
