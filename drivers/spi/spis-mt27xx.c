/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Leilk Liu <leilk.liu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/spi/spi.h>

#include "spis-mt27xx.h"

#define SPIS_IRQ_EN_REG	0x0
#define DMA_DONE_EN	BIT(7)
#define TX_FIFO_EMPTY_EN BIT(6)
#define TX_FIFO_FULL_EN	BIT(5)
#define RX_FIFO_EMPTY_EN BIT(4)
#define RX_FIFO_FULL_EN	BIT(3)
#define DATA_DONE_EN BIT(2)
#define RSTA_DONE_EN BIT(1)
#define CMD_INVALID_EN BIT(0)

#define SPIS_IRQ_CLR_REG 0x4
#define DMA_DONE_CLR BIT(7)
#define TX_FIFO_EMPTY_CLR BIT(6)
#define TX_FIFO_FULL_CLR BIT(5)
#define RX_FIFO_EMPTY_CLR BIT(4)
#define RX_FIFO_FULL_CLR BIT(3)
#define DATA_DONE_CLR BIT(2)
#define RSTA_DONE_CLR BIT(1)
#define CMD_INVALID_CLR	BIT(0)

#define SPIS_IRQ_ST_REG	0x8
#define DMA_DONE_ST	BIT(7)
#define TX_FIFO_EMPTY_ST BIT(6)
#define TX_FIFO_FULL_ST	BIT(5)
#define RX_FIFO_EMPTY_ST BIT(4)
#define RX_FIFO_FULL_ST	BIT(3)
#define DATA_DONE_ST BIT(2)
#define RSTA_DONE_ST BIT(1)
#define CMD_INVALID_ST BIT(0)

#define SPIS_IRQ_MASK_REG 0xc
#define DMA_DONE_MASK BIT(7)
#define DATA_DONE_MASK BIT(2)
#define RSTA_DONE_MASK BIT(1)
#define CMD_INVALID_MASK BIT(0)

#define SPIS_CFG_REG 0x10
#define SPIS_TX_ENDIAN BIT(7)
#define SPIS_RX_ENDIAN BIT(6)
#define SPIS_TXMSBF	BIT(5)
#define SPIS_RXMSBF	BIT(4)
#define SPIS_CPHA BIT(3)
#define SPIS_CPOL BIT(2)
#define SPIS_TX_EN BIT(1)
#define SPIS_RX_EN BIT(0)

#define SPIS_RX_DATA_REG 0x14
#define SPIS_TX_DATA_REG 0x18
#define SPIS_RX_DST_REG	0x1c
#define SPIS_TX_SRC_REG	0x20
#define SPIS_RX_CMD_REG	0x24
#define SPIS_FIFO_ST_REG 0x28
#define SPIS_MON_SEL_REG 0x2c

#define SPIS_DMA_CFG_REG 0x30
#define TX_DMA_TRIG_EN BIT(31)
#define TX_DMA_EN BIT(30)
#define RX_DMA_EN BIT(29)
#define TX_DMA_LEN 0xfffff

#define SPIS_FIFO_THR_REG 0x34
#define SPIS_DEBUG_ST_REG 0x38
#define SPIS_BYTE_CNT_REG 0x3c

#define SPIS_SOFT_RST_REG 0x40
#define SPIS_SOFT_RST BIT(0)

#define SPIS_DUMMY_REG 0x44
#define SPIS_BF_ID 0xf0

#define MTK_SPIS_MAX_FIFO_SIZE 512

struct mtk_spis {
	void __iomem *base;
	struct spi_device *spi;
	struct clk *sel_clk, *spi_clk;
	struct completion xfer_completion;
	struct spi_transfer *cur_transfer;
	spinlock_t spi_lock;
};


void mtk_spis_enable_irq(struct mtk_spis *mdata)
{
	u32 reg_val;

	reg_val = DMA_DONE_EN | TX_FIFO_EMPTY_EN | TX_FIFO_FULL_EN
		  | RX_FIFO_EMPTY_EN | RX_FIFO_FULL_EN | DATA_DONE_EN
		  | RSTA_DONE_EN | CMD_INVALID_EN;
	writel(reg_val, mdata->base + SPIS_IRQ_EN_REG);

	reg_val = DMA_DONE_MASK | DATA_DONE_MASK
		  | RSTA_DONE_MASK | CMD_INVALID_MASK;
	writel(reg_val, mdata->base + SPIS_IRQ_MASK_REG);
}

void mtk_spis_disable_dma(struct mtk_spis *mdata)
{
	int reg_val;

	/* disable dma tx/rx */
	reg_val = readl(mdata->base + SPIS_DMA_CFG_REG);
	reg_val &= ~RX_DMA_EN;
	reg_val &= ~TX_DMA_EN;
	writel(reg_val, mdata->base + SPIS_DMA_CFG_REG);
}

void mtk_spis_disable_xfer(struct mtk_spis *mdata)
{
	int reg_val;

	/* disable config reg tx rx_enable */
	reg_val = readl(mdata->base + SPIS_CFG_REG);
	reg_val &= ~SPIS_TX_EN;
	reg_val &= ~SPIS_RX_EN;
	writel(reg_val, mdata->base + SPIS_CFG_REG);
}

void mtk_spis_prepare_hw(struct spi_device *spi)
{
	u16 cpha, cpol;
	int reg_val;
	struct mtk_spis *mdata = spi_get_drvdata(spi);

	cpha = spi->mode & SPI_CPHA ? 1 : 0;
	cpol = spi->mode & SPI_CPOL ? 1 : 0;

	reg_val = readl(mdata->base + SPIS_CFG_REG);
	if (cpha)
		reg_val |= SPIS_CPHA;
	else
		reg_val &= ~SPIS_CPHA;
	if (cpol)
		reg_val |= SPIS_CPOL;
	else
		reg_val &= ~SPIS_CPOL;

	if (spi->mode & SPI_LSB_FIRST)
		reg_val &= ~(SPIS_TXMSBF | SPIS_RXMSBF);
	else
		reg_val |= SPIS_TXMSBF | SPIS_RXMSBF;

	writel(reg_val, mdata->base + SPIS_CFG_REG);
}

int mtk_spis_dma_transfer(struct spi_device *spi,
			  struct spi_transfer *xfer)
{
	int reg_val;
	struct spi_master *master = spi->master;
	struct mtk_spis *mdata = spi_get_drvdata(spi);

	/* soft reset for dma */
	writel(SPIS_SOFT_RST, mdata->base + SPIS_SOFT_RST_REG);

	if (xfer->tx_buf) {
		xfer->tx_dma = dma_map_single(master->dev.parent, (void *)xfer->tx_buf,
					      xfer->len, DMA_TO_DEVICE);
		if (dma_mapping_error(master->dev.parent, xfer->tx_dma)) {
			dev_err(&spi->dev, "dma mapping txbuf err\n");
			return -1;
		}
	}

	if (xfer->rx_buf) {
		xfer->rx_dma = dma_map_single(master->dev.parent, (void *)xfer->rx_buf,
					      xfer->len, DMA_FROM_DEVICE);
		if (dma_mapping_error(master->dev.parent, xfer->rx_dma)) {
			dev_err(&spi->dev, "dma mapping rxbuf err\n");
			return -1;
		}
	}

	/* write addr */
	writel(xfer->tx_dma, mdata->base + SPIS_TX_SRC_REG);
	writel(xfer->rx_dma, mdata->base + SPIS_RX_DST_REG);

	writel(BIT(1), mdata->base + SPIS_SOFT_RST_REG);

	/* enable config reg tx rx_enable */
	reg_val = readl(mdata->base + SPIS_CFG_REG);
	if (xfer->tx_buf)
		reg_val |= SPIS_TX_EN;
	if (xfer->rx_buf)
		reg_val |= SPIS_RX_EN;
	writel(reg_val, mdata->base + SPIS_CFG_REG);

	/* config dma */
	reg_val = 0;
	reg_val &= ~TX_DMA_LEN;
	reg_val |= (xfer->len - 1) & TX_DMA_LEN;
	writel(reg_val, mdata->base + SPIS_DMA_CFG_REG);

	reg_val = readl(mdata->base + SPIS_DMA_CFG_REG);
	if (xfer->tx_buf)
		reg_val |= TX_DMA_EN;
	if (xfer->rx_buf)
		reg_val |= RX_DMA_EN;
	reg_val |= TX_DMA_TRIG_EN;
	writel(reg_val, mdata->base + SPIS_DMA_CFG_REG);

	return 0;
}

int mtk_spis_fifo_transfer(struct spi_device *spi,
			   struct spi_transfer *xfer)
{
	int reg_val, cnt, remainder;
	struct mtk_spis *mdata = spi_get_drvdata(spi);

	reg_val = readl(mdata->base + SPIS_CFG_REG);
	if (xfer->rx_buf)
		reg_val |= SPIS_RX_EN;
	if (xfer->tx_buf)
		reg_val |= SPIS_TX_EN;
	writel(reg_val, mdata->base + SPIS_CFG_REG);

	cnt = xfer->len / 4;
	if (xfer->tx_buf)
		iowrite32_rep(mdata->base + SPIS_TX_DATA_REG,
			      xfer->tx_buf, cnt);

	remainder = xfer->len % 4;
	if (xfer->tx_buf && remainder > 0) {
		reg_val = 0;
		memcpy(&reg_val, xfer->tx_buf + (cnt * 4), remainder);
		writel(reg_val, mdata->base + SPIS_TX_DATA_REG);
	}

	return 0;
}

int mtk_spis_transfer_one(struct spi_device *spi,
			  struct spi_transfer *xfer)
{
	struct mtk_spis *mdata = spi_get_drvdata(spi);

	reinit_completion(&mdata->xfer_completion);
	mdata->cur_transfer = xfer;

	mtk_spis_prepare_hw(spi);

	if (xfer->len > MTK_SPIS_MAX_FIFO_SIZE)
		return mtk_spis_dma_transfer(spi, xfer);
	else
		return mtk_spis_fifo_transfer(spi, xfer);
}

void mtk_spis_finalize_current_transfer(struct spi_device *spi)
{
	struct mtk_spis *mdata = spi_get_drvdata(spi);

	complete(&mdata->xfer_completion);
}

void mtk_spis_wait_for_transfer_done(struct spi_device *spi)
{
	unsigned long ms = 1;
	struct mtk_spis *mdata = spi_get_drvdata(spi);

	ms += ms + 100; /* some tolerance */

	ms = wait_for_completion_timeout(&mdata->xfer_completion,
					 msecs_to_jiffies(ms));
	if (!ms)
		pr_err("mtk-spis transfer timed out\n");
}

void mtk_spis_setup(struct spi_device *spi)
{
	int reg_val;
	struct mtk_spis *mdata = spi_get_drvdata(spi);

	mtk_spis_enable_irq(mdata);

	reg_val = readl(mdata->base + SPIS_CFG_REG);
	if (spi->mode & SPI_LSB_FIRST)
		reg_val &= ~(SPIS_TXMSBF | SPIS_RXMSBF);
	else
		reg_val |= SPIS_TXMSBF | SPIS_RXMSBF;
	writel(reg_val, mdata->base + SPIS_CFG_REG);

	mtk_spis_disable_dma(mdata);
	mtk_spis_disable_xfer(mdata);
}

irqreturn_t mtk_spis_interrupt(int irq, void *dev_id)
{
	int int_status, reg_val, cnt, remainder;
	struct spi_device *spi = dev_id;
	struct spi_master *master = spi->master;
	struct mtk_spis *mdata = spi_get_drvdata(spi);
	struct spi_transfer *trans = mdata->cur_transfer;

	int_status = readl(mdata->base + SPIS_IRQ_ST_REG);
	writel(int_status, mdata->base + SPIS_IRQ_CLR_REG);

	if (!trans) {
		pr_err("[%s]trans is NULL, int_status(0x%x)\n", __func__, int_status);
		return IRQ_HANDLED;
	}
	do {
		if ((int_status & DMA_DONE_ST) &&
			((int_status & DATA_DONE_ST) || (int_status & RSTA_DONE_ST))) {
			writel(SPIS_SOFT_RST, mdata->base + SPIS_SOFT_RST_REG);

			mtk_spis_disable_dma(mdata);
			mtk_spis_disable_xfer(mdata);

			if (trans->tx_buf)
				dma_unmap_single(master->dev.parent, trans->tx_dma,
						 trans->len, DMA_TO_DEVICE);
			if (trans->rx_buf)
				dma_unmap_single(master->dev.parent, trans->rx_dma,
						 trans->len, DMA_FROM_DEVICE);
		}

		if ((!(int_status & DMA_DONE_ST)) &&
		    ((int_status & DATA_DONE_ST) || (int_status & RSTA_DONE_ST))) {
			cnt = trans->len / 4;
			if (trans->rx_buf)
				ioread32_rep(mdata->base + SPIS_RX_DATA_REG,
					     trans->rx_buf, cnt);
			remainder = trans->len % 4;
			if (trans->rx_buf && remainder > 0) {
				reg_val = readl(mdata->base + SPIS_RX_DATA_REG);
				memcpy(trans->rx_buf + (cnt * 4),
				       &reg_val, remainder);
			}

			mtk_spis_disable_xfer(mdata);
		}

		if (int_status & CMD_INVALID_ST)
			pr_err("mtk-spis cmd invalid\n");

		int_status = readl(mdata->base + SPIS_IRQ_ST_REG);
		writel(int_status, mdata->base + SPIS_IRQ_CLR_REG);
	} while (int_status & (DMA_DONE_ST | DATA_DONE_ST | CMD_INVALID_ST));

	mdata->cur_transfer = NULL;
	mtk_spis_finalize_current_transfer(spi);

	return IRQ_HANDLED;
}

static int mtk_spis_probe(struct spi_device *spi)
{
	int ret;
	struct mtk_spis	*mdata;
	struct device_node *np;
	struct device *dev = &spi->dev;

	np = of_find_compatible_node(NULL, NULL, "mediatek,2712-spis");
	if (!np) {
		pr_err("%s, fail to find node\n", __func__);
		return -1;
	}

	mdata = devm_kzalloc(dev, sizeof(*mdata), GFP_KERNEL);
	if (!mdata)
		return -ENOMEM;

	mdata->base = ioremap(0x10013000, 0x100);
	if (!mdata->base) {
		pr_err("%s, fail to get spis base\n", __func__);
		return -1;
	}

	mdata->spi = spi;
	init_completion(&mdata->xfer_completion);
	spi_set_drvdata(spi, mdata);

	mdata->sel_clk = devm_clk_get(&spi->dev, "selclk");
	if (IS_ERR(mdata->sel_clk)) {
		ret = PTR_ERR(mdata->sel_clk);
		dev_err(&spi->dev, "failed to get sel-clk: %d\n", ret);
		return -ENOMEM;
	}

	ret = clk_prepare_enable(mdata->sel_clk);
	if (ret < 0) {
		dev_err(&spi->dev, "failed to enable sel_clk (%d)\n", ret);
		return -ENOMEM;
	}

	mdata->spi_clk = devm_clk_get(&spi->dev, "spiclk");
	if (IS_ERR(mdata->spi_clk)) {
		ret = PTR_ERR(mdata->spi_clk);
		dev_err(&spi->dev, "failed to get spi-clk: %d\n", ret);
		return -ENOMEM;
	}

	ret = clk_prepare_enable(mdata->spi_clk);
	if (ret < 0) {
		dev_err(&spi->dev, "failed to enable spi_clk (%d)\n", ret);
		return -ENOMEM;
	}

	mtk_spis_setup(spi);

	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;

	ret = devm_request_irq(dev, spi->irq, mtk_spis_interrupt,
			       IRQF_TRIGGER_NONE, dev_name(dev), spi);
	if (ret < 0) {
		dev_err(dev, "request spis irq %d fail\n", spi->irq);
		return ret;
	}

	mtk_spis_create_attribute(&spi->dev);

	return 0;
}

static int mtk_spis_remove(struct spi_device *spi)
{
	struct mtk_spis	*mdata = spi_get_drvdata(spi);

	spin_lock_irq(&mdata->spi_lock);
	mdata->spi = NULL;
	spin_unlock_irq(&mdata->spi_lock);

	return 0;
}

static const struct of_device_id mtk_spis_dt_ids[] = {
	{ .compatible = "mediatek,2712-spis" },
	{},
};

MODULE_DEVICE_TABLE(of, mtk_spis_dt_ids);

static struct spi_driver mtk_spis_driver = {
	.driver = {
		.name = "mtk-spis",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_spis_dt_ids),
	},
	.probe = mtk_spis_probe,
	.remove = mtk_spis_remove,
};

static int __init mtk_spis_init(void)
{
	return spi_register_driver(&mtk_spis_driver);
}
module_init(mtk_spis_init);

static void __exit mtk_spis_exit(void)
{
	spi_unregister_driver(&mtk_spis_driver);
}
module_exit(mtk_spis_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:mtk-spis");
MODULE_AUTHOR("Leilk Liu <leilk.liu@mediatek.com>");

