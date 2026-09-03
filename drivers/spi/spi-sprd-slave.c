// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/clk.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sprd-dma.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>

#define SPRD_SPI_TXD			0x0
#define SPRD_SPI_CLKD			0x4
#define SPRD_SPI_CTL0			0x8
#define SPRD_SPI_CTL1			0xc
#define SPRD_SPI_CTL2			0x10
#define SPRD_SPI_CTL3			0x14
#define SPRD_SPI_CTL4			0x18
#define SPRD_SPI_CTL5			0x1c
#define SPRD_SPI_INT_EN			0x20
#define SPRD_SPI_INT_CLR		0x24
#define SPRD_SPI_INT_RAW_STS		0x28
#define SPRD_SPI_INT_MASK_STS		0x2c
#define SPRD_SPI_STS1			0x30
#define SPRD_SPI_STS2			0x34
#define SPRD_SPI_DSP_WAIT		0x38
#define SPRD_SPI_STS3			0x3c
#define SPRD_SPI_CTL6			0x40
#define SPRD_SPI_STS4			0x44
#define SPRD_SPI_FIFO_RST		0x48
#define SPRD_SPI_CTL7			0x4c
#define SPRD_SPI_STS5			0x50
#define SPRD_SPI_CTL8			0x54
#define SPRD_SPI_CTL9			0x58
#define SPRD_SPI_CTL10			0x5c
#define SPRD_SPI_CTL11			0x60
#define SPRD_SPI_CTL12			0x64
#define SPRD_SPI_STS6			0x68
#define SPRD_SPI_STS7			0x6c
#define SPRD_SPI_STS8			0x70
#define SPRD_SPI_STS9			0x74

/* Bits & mask definition for register CTL0 */
#define SPRD_SPI_SCK_REV		BIT(13)
#define SPRD_SPI_NG_TX			BIT(1)
#define SPRD_SPI_NG_RX			BIT(0)
#define SPRD_SPI_CHNL_LEN_MASK		GENMASK(4, 0)
#define SPRD_SPI_CSN_MASK		GENMASK(11, 8)
#define SPRD_SPI_CS0_VALID		BIT(8)

/* Bits & mask definition for register SPI_INT_EN */
#define SPRD_SPI_ALL_INT_CLR		0x33f
#define SPRD_SPI_TIMEOUT_INT_EN		BIT(5)
#define SPRD_SPI_RX_FULL_INT_EN		BIT(6)
#define SPRD_SPI_TX_EMPTY_INT_EN        BIT(7)
#define SPRD_SPI_TX_END_INT_EN		BIT(8)
#define SPRD_SPI_RX_END_INT_EN		BIT(9)

/* Bits & mask definition for register SPI_INT_RAW_STS */
#define SPRD_SPI_TX_END_RAW		BIT(8)
#define SPRD_SPI_RX_END_RAW		BIT(9)

/* Bits & mask definition for register SPI_INT_CLR */
#define SPRD_SPI_TX_END_CLR		BIT(8)
#define SPRD_SPI_RX_END_CLR		BIT(9)

/* Bits & mask definition for register INT_MASK_STS */
#define SPRD_SPI_MASK_TIME_OUT		BIT(5)
#define SPRD_SPI_MASK_RX_FULL           BIT(6)
#define SPRD_SPI_MASK_TX_EMPTY          BIT(7)
#define SPRD_SPI_MASK_RX_END		BIT(9)
#define SPRD_SPI_MASK_TX_END		BIT(8)

/* Bits & mask definition for register STS1 */
#define SPRD_SPI_RXF_WADDR		GENMASK(12, 8)
#define SPRD_SPI_RXF_RADDR		GENMASK(4, 0)

/* Bits & mask definition for register STS2 */
#define SPRD_SPI_TX_BUSY		BIT(8)
#define SPRD_SPI_RX_REAL_EMPTY		BIT(5)
#define SPRD_SPI_TX_REAL_FULL		BIT(6)
#define SPRD_SPI_TX_REAL_EMPTY		BIT(7)

/* Bits & mask definition for register STS4 */
#define SPRD_SPI_TXF_WADDR		GENMASK(12, 8)
#define SPRD_SPI_TXF_RADDR		GENMASK(4, 0)

/* Bits & mask definition for register CTL1 */
#define SPRD_SPI_RX_MODE		BIT(12)
#define SPRD_SPI_TX_MODE		BIT(13)
#define SPRD_SPI_RTX_MD_MASK		GENMASK(13, 12)

/* Bits & mask definition for register CTL2 */
#define SPRD_SPI_IS_SLVD		BIT(5)
#define SPRD_SPI_DMA_EN			BIT(6)

/* Bits & mask definition for register CTL4 */
#define SPRD_SPI_IS_FAST		BIT(14)
#define SPRD_SPI_START_RX		BIT(9)
#define SPRD_SPI_ONLY_RECV_MASK		GENMASK(8, 0)

/* Bits & mask definition for register CTL5 */
#define SPRD_SPI_ITVL_NUM		0x4000

/* Bits & mask definition for register SPI_INT_CLR */
#define SPRD_SPI_RX_END_INT_CLR		BIT(9)
#define SPRD_SPI_TX_END_INT_CLR		BIT(8)

/* Bits & mask definition for register SPI_INT_RAW */
#define SPRD_SPI_RX_END_IRQ		BIT(9)
#define SPRD_SPI_TX_END_IRQ		BIT(8)

/* Bits & mask definition for register CTL12 */
#define SPRD_SPI_SW_RX_REQ		BIT(0)
#define SPRD_SPI_SW_TX_REQ		BIT(1)

/* Bit & mask rx/tx fifo thld*/
#define FIFO_RX_EMPTY			0x3
#define FIFO_RX_FULL			0x6
#define RXF_FULL_THLD_MASK		GENMASK(4, 0)
#define RXF_THLD_OFFSET			8

#define FIFO_TX_EMPTY			0x0
#define FIFO_TX_FULL			0x10
#define TXF_FULL_THLD_MASK		GENMASK(4, 0)
#define TXF_THLD_OFFSET			8

/* Bits & mask definition for register CTL7 */
#define SPRD_SPI_CSN_INPUT		BIT(0)
#define SPRD_SPI_DATA_LINE2_EN		BIT(15)
#define SPRD_SPI_MODE_MASK		GENMASK(5, 3)
#define SPRD_SPI_MODE_OFFSET		3
#define SPRD_SPI_3WIRE_MODE		4
#define SPRD_SPI_4WIRE_MODE		0
#define SPRD_SPI_SLV_SEL		BIT(11)
#define SPRD_SPI_SLV_EN			BIT(10)

/* Bits & mask definition for register CTL8 */
#define SPRD_SPI_TX_MAX_LEN_MASK	GENMASK(19, 0)
#define SPRD_SPI_TX_LEN_H_MASK		GENMASK(3, 0)
#define SPRD_SPI_TX_LEN_H_OFFSET	16

/* Bits & mask definition for register CTL9 */
#define SPRD_SPI_TX_LEN_L_MASK		GENMASK(15, 0)

/* Bits & mask definition for register CTL10 */
#define SPRD_SPI_RX_MAX_LEN_MASK	GENMASK(19, 0)
#define SPRD_SPI_RX_LEN_H_MASK		GENMASK(3, 0)
#define SPRD_SPI_RX_LEN_H_OFFSET	16

/* Bits & mask definition for register CTL11 */
#define SPRD_SPI_RX_LEN_L_MASK		GENMASK(15, 0)

/* Default & maximum word delay cycles */
#define SPRD_SPI_MIN_DELAY_CYCLE	14
#define SPRD_SPI_MAX_DELAY_CYCLE	130

#define SPRD_SPI_FIFO_SIZE		32
#define SPRD_SPI_CHIP_CS_NUM		0x4
#define SPRD_SPI_CHNL_LEN		2
#define SPRD_SPI_DEFAULT_SOURCE		26000000
#define SPRD_SPI_MAX_SPEED_HZ		48000000
#define SPRD_SPI_AUTOSUSPEND_DELAY	100
#define SPI_XFER_TIMEOUT		100
#define SPRD_SPI_DMA_STEP		8

enum sprd_spi_dma_channel {
	SPRD_SPI_RX,
	SPRD_SPI_TX,
	SPRD_SPI_MAX,
};

struct sprd_spi_dma {
	bool enable;
	struct dma_chan *dma_chan[SPRD_SPI_MAX];
	dma_addr_t dma_phys_addr;
	enum dma_slave_buswidth width;
	u32 fragmens_len;
};

struct sprd_spi_slave {
	void __iomem *base;
	struct device *dev;
	struct clk *clk;
	phys_addr_t phy_base;
	int irq;
	u32 src_clk;
	u32 hw_mode;
	u32 trans_len;
	u32 trans_mode;
	u32 hw_speed_hz;
	int status;
	int datawidth;
	u32 dma_trans_len;
	struct sprd_spi_dma dma;
	struct spi_transfer *cur_transfer;
	const void *tx_buf;
	void *rx_buf;
	int (*read_bufs)(struct sprd_spi_slave *ss, u32 len);
	int (*write_bufs)(struct sprd_spi_slave *ss, u32 len);
	struct completion xfer_done;
	bool slave_aborted;
	int not_first_flag;
};

static void sprd_spi_set_rx_fifo_thld(struct sprd_spi_slave *ss,
				      unsigned int len)
{
	u32 val = readl_relaxed(ss->base + SPRD_SPI_CTL3);

	val &= ~RXF_FULL_THLD_MASK;
	writel_relaxed(val | (len >> ss->datawidth), ss->base + SPRD_SPI_CTL3);
}

static void sprd_spi_set_tx_fifo_thld(struct sprd_spi_slave *ss,
				      unsigned int len)
{
	u32 val = readl_relaxed(ss->base + SPRD_SPI_CTL6);

	val &= ~TXF_FULL_THLD_MASK;
	writel_relaxed(val | (len >> ss->datawidth), ss->base + SPRD_SPI_CTL6);
}

static void sprd_spi_irq_enable(struct sprd_spi_slave *ss)
{
	u32 val;

	/* Clear interrupt status before enabling interrupt. */
	writel_relaxed(SPRD_SPI_ALL_INT_CLR, ss->base + SPRD_SPI_INT_CLR);
	/* Enable SPI interrupt. */
	val = readl_relaxed(ss->base + SPRD_SPI_INT_EN);
	if (ss->trans_mode & SPRD_SPI_RX_MODE)
		val = val | SPRD_SPI_TIMEOUT_INT_EN | SPRD_SPI_RX_FULL_INT_EN;
	if (ss->trans_mode & SPRD_SPI_TX_MODE)
		val = val | SPRD_SPI_TX_EMPTY_INT_EN;

	writel_relaxed(val, ss->base + SPRD_SPI_INT_EN);
}

static void sprd_spi_irq_disable(struct sprd_spi_slave *ss)
{
	u32 val;

	/* Clear interrupt status before enabling interrupt. */
	writel_relaxed(SPRD_SPI_ALL_INT_CLR, ss->base + SPRD_SPI_INT_CLR);
	/* disable SPI interrupt. */
	val = readl_relaxed(ss->base + SPRD_SPI_INT_EN);
	writel_relaxed(0, ss->base + SPRD_SPI_INT_EN);
}

static void sprd_spi_enter_idle(struct sprd_spi_slave *ss)
{
	u32 val = readl_relaxed(ss->base + SPRD_SPI_CTL1);

	val &= ~SPRD_SPI_RTX_MD_MASK;
	writel_relaxed(val, ss->base + SPRD_SPI_CTL1);
}

static int sprd_spi_slv_rxf_cnt(struct sprd_spi_slave *ss)
{
	int rxf_waddr, rxf_raddr;

	if (readl_relaxed(ss->base + SPRD_SPI_STS2) & SPRD_SPI_RX_REAL_EMPTY)
		return 0;

	rxf_waddr = (readl_relaxed(ss->base + SPRD_SPI_STS1) &
		     SPRD_SPI_RXF_WADDR) >> 8;
	rxf_raddr = readl_relaxed(ss->base + SPRD_SPI_STS1) &
		SPRD_SPI_RXF_RADDR;

	if (rxf_waddr > rxf_raddr)
		return (rxf_waddr - rxf_raddr);
	else
		return (rxf_waddr + 32 - rxf_raddr);

}

static int sprd_spi_slv_txf_cnt(struct sprd_spi_slave *ss)
{
	int txf_waddr, txf_raddr;

	if (readl_relaxed(ss->base + SPRD_SPI_STS2) & SPRD_SPI_TX_REAL_EMPTY)
		return 0;

	txf_waddr = (readl_relaxed(ss->base + SPRD_SPI_STS4) &
		     SPRD_SPI_TXF_WADDR) >> 8;
	txf_raddr = readl_relaxed(ss->base + SPRD_SPI_STS4) &
		SPRD_SPI_TXF_RADDR;
	if (txf_waddr > txf_raddr)
		return (txf_waddr - txf_raddr);
	else
		return (txf_waddr + 32 - txf_raddr);
}
static int sprd_spi_slv_write_bufs_u8(struct sprd_spi_slave *ss, u32 len)
{
	u8 *tx_p = (u8 *)ss->tx_buf;
	int i;

	for (i = 0; i < len; i++)
		writeb_relaxed(tx_p[i], ss->base + SPRD_SPI_TXD);

	ss->tx_buf += i;
	return i;
}

static int sprd_spi_slv_write_bufs_u16(struct sprd_spi_slave *ss, u32 len)
{
	u16 *tx_p = (u16 *)ss->tx_buf;
	int i;

	for (i = 0; i < len; i++)
		writew_relaxed(tx_p[i], ss->base + SPRD_SPI_TXD);

	ss->tx_buf += i << 1;
	return i << 1;
}

static int sprd_spi_slv_write_bufs_u32(struct sprd_spi_slave *ss, u32 len)
{
	u32 *tx_p = (u32 *)ss->tx_buf;
	int i;

	for (i = 0; i < len; i++)
		writel_relaxed(tx_p[i], ss->base + SPRD_SPI_TXD);

	ss->tx_buf += i << 2;
	return i << 2;
}

static int sprd_spi_slv_read_bufs_u8(struct sprd_spi_slave *ss, u32 len)
{
	u8 *rx_p = (u8 *)ss->rx_buf;
	int i;

	for (i = 0; i < len; i++)
		rx_p[i] = readb_relaxed(ss->base + SPRD_SPI_TXD);

	ss->rx_buf += i;
	return i;
}

static int sprd_spi_slv_read_bufs_u16(struct sprd_spi_slave *ss, u32 len)
{
	u16 *rx_p = (u16 *)ss->rx_buf;
	int i;

	for (i = 0; i < len; i++)
		rx_p[i] = readw_relaxed(ss->base + SPRD_SPI_TXD);

	ss->rx_buf += i << 1;
	return i << 1;
}

static int sprd_spi_slv_read_bufs_u32(struct sprd_spi_slave *ss, u32 len)
{
	u32 *rx_p = (u32 *)ss->rx_buf;
	int i;

	for (i = 0; i < len; i++)
		rx_p[i] = readl_relaxed(ss->base + SPRD_SPI_TXD);

	ss->rx_buf += i << 2;
	return i << 2;
}

static void sprd_spi_set_transfer_bits(struct sprd_spi_slave *ss, u32 bits)
{
	u32 val = readl_relaxed(ss->base + SPRD_SPI_CTL0);

	/* Set the valid bits for every transaction */
	val &= ~(SPRD_SPI_CHNL_LEN_MASK << SPRD_SPI_CHNL_LEN);
	val |= bits << SPRD_SPI_CHNL_LEN;
	writel_relaxed(val, ss->base + SPRD_SPI_CTL0);
}

static void sprd_spi_dma_enable(struct sprd_spi_slave *ss, bool enable)
{
	u32 val = readl_relaxed(ss->base + SPRD_SPI_CTL2);

	if (enable)
		val |= SPRD_SPI_DMA_EN;
	else
		val &= ~SPRD_SPI_DMA_EN;

	writel_relaxed(val, ss->base + SPRD_SPI_CTL2);
}

static void sprd_complete_tx_dma(void *data)
{
	int i;
	struct sprd_spi_slave *ss = (struct sprd_spi_slave *)data;
	int len = ss->cur_transfer->len >= ss->dma_trans_len;

	for (i = 0; i < len; i++)
		iowrite8_rep(ss->base + SPRD_SPI_TXD,
			     ss->cur_transfer->tx_buf + ss->dma_trans_len, len);

	if (ss->trans_mode == SPRD_SPI_TX_MODE)
		complete(&ss->xfer_done);
}

static void sprd_complete_rx_dma(void *data)
{
	int i;
	struct sprd_spi_slave *ss = (struct sprd_spi_slave *)data;
	int len = ss->cur_transfer->len >= ss->dma_trans_len;

	for (i = 0; i < len; i++)
		//ss->rx_buf[len+i] = readb_relaxed(ss->base + SPRD_SPI_TXD);
		ioread8_rep(ss->base + SPRD_SPI_TXD,
			    ss->cur_transfer->rx_buf + ss->dma_trans_len, len);

	complete(&ss->xfer_done);
}

static int sprd_spi_dma_submit(struct sprd_spi_slave *ss,
			       struct dma_chan *dma_chan,
			       struct dma_slave_config *c,
			       struct sg_table *sg,
			       enum dma_transfer_direction dir,
			       dma_async_tx_callback callback)
{
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	unsigned long flags;
	int ret;

	ret = dmaengine_slave_config(dma_chan, c);
	if (ret < 0)
		return ret;

	flags = SPRD_DMA_FLAGS(SPRD_DMA_CHN_MODE_NONE, SPRD_DMA_NO_TRG,
			       SPRD_DMA_FRAG_REQ, SPRD_DMA_TRANS_INT);

	if (sg != NULL) {
		sg_dma_len(sg->sgl) = ss->dma_trans_len;
		desc = dmaengine_prep_slave_sg(dma_chan, sg->sgl,
					       sg->nents, dir, flags);
	} else {
		desc = dmaengine_prep_slave_single(dma_chan,
						   ss->dma.dma_phys_addr,
						   ss->dma_trans_len,
						   dir, flags);
	}

	if (!desc)
		return  -ENODEV;

	desc->callback = callback;
	desc->callback_param = ss;

	cookie = dmaengine_submit(desc);
	if (dma_submit_error(cookie))
		return dma_submit_error(cookie);

	dma_async_issue_pending(dma_chan);

	return 0;
}

static int sprd_spi_dma_rx_config(struct sprd_spi_slave *ss,
				  struct spi_transfer *t)
{
	struct dma_chan *dma_chan = ss->dma.dma_chan[SPRD_SPI_RX];
	struct dma_slave_config config = {
		.src_addr = ss->phy_base,
		.src_addr_width = ss->dma.width,
		.dst_addr_width = ss->dma.width,
		.src_maxburst = ss->dma.fragmens_len,
		.dst_maxburst = ss->dma.fragmens_len,
		.direction = DMA_DEV_TO_MEM,
		.slave_id = ss->dma.dma_chan[SPRD_SPI_RX]->chan_id + 1,
	};
	int ret;

	ret = sprd_spi_dma_submit(ss, dma_chan, &config,
				  &t->rx_sg, DMA_DEV_TO_MEM,
				  sprd_complete_rx_dma);
	if (ret)
		return ret;

	sprd_spi_set_rx_fifo_thld(ss, ss->dma.fragmens_len);

	return 0;
}

static int sprd_spi_dma_tx_config(struct sprd_spi_slave *ss,
				  struct spi_transfer *t)
{
	struct dma_chan *dma_chan = ss->dma.dma_chan[SPRD_SPI_TX];
	struct dma_slave_config config = {
		.dst_addr = ss->phy_base,
		.src_addr_width = ss->dma.width,
		.dst_addr_width = ss->dma.width,
		.src_maxburst = ss->dma.fragmens_len,
		.direction = DMA_MEM_TO_DEV,
		.slave_id = ss->dma.dma_chan[SPRD_SPI_TX]->chan_id + 1,
	};
	int ret;

	if (ss->dma_trans_len > t->len)
		ret = sprd_spi_dma_submit(ss, dma_chan, &config,
					  NULL, DMA_MEM_TO_DEV,
					  sprd_complete_tx_dma);
	else
		ret = sprd_spi_dma_submit(ss, dma_chan, &config,
					  &t->tx_sg, DMA_MEM_TO_DEV,
					  sprd_complete_tx_dma);

	if (ret)
		return ret;

	sprd_spi_set_tx_fifo_thld(ss, ss->dma.fragmens_len);

	return 0;
}

static int sprd_spi_dma_request(struct sprd_spi_slave *ss)
{
	ss->dma.dma_chan[SPRD_SPI_RX] = dma_request_chan(ss->dev, "rx_chn");
	if (IS_ERR_OR_NULL(ss->dma.dma_chan[SPRD_SPI_RX])) {
		if (PTR_ERR(ss->dma.dma_chan[SPRD_SPI_RX]) == -EPROBE_DEFER)
			return PTR_ERR(ss->dma.dma_chan[SPRD_SPI_RX]);

		dev_err(ss->dev, "request RX DMA channel failed!\n");
		return PTR_ERR(ss->dma.dma_chan[SPRD_SPI_RX]);
	}

	ss->dma.dma_chan[SPRD_SPI_TX] = dma_request_chan(ss->dev, "tx_chn");
	if (IS_ERR_OR_NULL(ss->dma.dma_chan[SPRD_SPI_TX])) {
		if (PTR_ERR(ss->dma.dma_chan[SPRD_SPI_TX]) == -EPROBE_DEFER)
			return PTR_ERR(ss->dma.dma_chan[SPRD_SPI_TX]);

		dev_err(ss->dev, "request TX DMA channel failed!\n");
		dma_release_channel(ss->dma.dma_chan[SPRD_SPI_RX]);
		return PTR_ERR(ss->dma.dma_chan[SPRD_SPI_TX]);
	}

	return 0;
}

static void sprd_spi_dma_release(struct sprd_spi_slave *ss)
{
	if (ss->dma.dma_chan[SPRD_SPI_RX])
		dma_release_channel(ss->dma.dma_chan[SPRD_SPI_RX]);

	if (ss->dma.dma_chan[SPRD_SPI_TX])
		dma_release_channel(ss->dma.dma_chan[SPRD_SPI_TX]);
}

static int sprd_spi_slave_wait_for_completion(struct sprd_spi_slave *ss)
{

	if (wait_for_completion_interruptible(&ss->xfer_done) ||
	    ss->slave_aborted) {
		dev_err(ss->dev, "interrupted\n");
		return -EINTR;
	}
	return 0;
}

static void sprd_spi_init_hw(struct sprd_spi_slave *ss, struct spi_transfer *t)
{
	u16 word_delay, interval;
	u32 val;

	val = readl_relaxed(ss->base + SPRD_SPI_CTL0);
	val &= ~(SPRD_SPI_SCK_REV | SPRD_SPI_NG_TX | SPRD_SPI_NG_RX);
	/* Set default chip selection, clock phase and clock polarity */
	val |= ss->hw_mode & SPI_CPHA ? SPRD_SPI_NG_RX : SPRD_SPI_NG_TX;
	val |= ss->hw_mode & SPI_CPOL ? SPRD_SPI_SCK_REV : 0;
	writel_relaxed(val, ss->base + SPRD_SPI_CTL0);

	/*
	 * Set the intervals of two SPI frames, and the inteval calculation
	 * formula as below per	 datasheet:
	 */
	writel_relaxed(0x10, ss->base + SPRD_SPI_CTL5);

	/* Reset SPI fifo */
	writel_relaxed(1, ss->base + SPRD_SPI_FIFO_RST);
	writel_relaxed(0, ss->base + SPRD_SPI_FIFO_RST);

	/* Set SPI work mode */
	/* enable slave mode */
	val = readl_relaxed(ss->base + SPRD_SPI_CTL2);
	val |= SPRD_SPI_IS_SLVD;
	writel_relaxed(val, ss->base + SPRD_SPI_CTL2);

	val = readl_relaxed(ss->base + SPRD_SPI_CTL7);
	val &= ~(SPRD_SPI_MODE_MASK | SPRD_SPI_SLV_SEL | SPRD_SPI_SLV_EN);
	val |= SPRD_SPI_SLV_SEL;
	writel_relaxed(val, ss->base + SPRD_SPI_CTL7);

	/* Clear all interrupt status*/
	writel_relaxed(SPRD_SPI_ALL_INT_CLR, ss->base + SPRD_SPI_INT_CLR);

	/* Set SPI default fifo threshold*/
	writel_relaxed((FIFO_TX_EMPTY << TXF_THLD_OFFSET) | FIFO_TX_FULL,
		       ss->base + SPRD_SPI_CTL6);
	writel_relaxed((FIFO_RX_EMPTY << RXF_THLD_OFFSET) | FIFO_RX_FULL,
		       ss->base + SPRD_SPI_CTL3);
}

static int sprd_spi_setup_transfer(struct spi_device *sdev,
				   struct spi_transfer *t)
{
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sdev->controller);
	u8 bits_per_word = t->bits_per_word;
	u32 val, mode = 0;

	ss->tx_buf = t->tx_buf;
	ss->rx_buf = t->rx_buf;
	ss->hw_mode = sdev->mode;

	/* Set tansfer speed and valid bits */
	sprd_spi_set_transfer_bits(ss, bits_per_word);

	if (bits_per_word > 16)
		bits_per_word = round_up(bits_per_word, 16);
	else
		bits_per_word = round_up(bits_per_word, 8);

	switch (bits_per_word) {
	case 8:
		ss->trans_len = t->len;
		ss->read_bufs = sprd_spi_slv_read_bufs_u8;
		ss->write_bufs = sprd_spi_slv_write_bufs_u8;
		ss->dma.width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		ss->dma.fragmens_len = SPRD_SPI_DMA_STEP;
		ss->datawidth = 0;
		break;
	case 16:
		ss->trans_len = t->len >> 1;
		t->len = ss->trans_len;
		ss->read_bufs = sprd_spi_slv_read_bufs_u16;
		ss->write_bufs = sprd_spi_slv_write_bufs_u16;
		ss->dma.width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		ss->dma.fragmens_len = SPRD_SPI_DMA_STEP << 1;
		ss->datawidth = 1;
		break;
	case 32:
		ss->trans_len = t->len >> 2;
		t->len = ss->trans_len;
		ss->read_bufs = sprd_spi_slv_read_bufs_u32;
		ss->write_bufs = sprd_spi_slv_write_bufs_u32;
		ss->dma.width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		ss->dma.fragmens_len = SPRD_SPI_DMA_STEP << 2;
		ss->datawidth = 2;
		break;
	default:
		return -EINVAL;
	}

	sprd_spi_init_hw(ss, t);

	/* Set transfer read or write mode */
	val = readl_relaxed(ss->base + SPRD_SPI_CTL1);
	val &= ~SPRD_SPI_RTX_MD_MASK;
	if (t->tx_buf)
		mode |= SPRD_SPI_TX_MODE;
	if (t->rx_buf)
		mode |= SPRD_SPI_RX_MODE;

	writel_relaxed(val | mode, ss->base + SPRD_SPI_CTL1);

	ss->trans_mode = mode;

	return 0;
}

static int sprd_spi_slave_fifo_transfer(struct spi_controller *sctlr,
				       struct spi_device *sdev,
				       struct spi_transfer *t)
{
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sdev->controller);
	u32 len;
	int ret = 0;

	//set rxfifo_full_water_mark to read data
	writel_relaxed(t->len, ss->base + SPRD_SPI_CTL3);

	if (ss->trans_mode & SPRD_SPI_TX_MODE)
		ret = ss->write_bufs(ss, t->len);

	sprd_spi_irq_enable(ss);
	ret = sprd_spi_slave_wait_for_completion(ss);
	if (ss->trans_len)
		dev_err(ss->dev, "slave didn't get enough data, received: %d\n",
			ss->trans_len);

	return ret;
}

static int sprd_spi_dma_transfer(struct spi_device *sdev, struct spi_transfer *t)
{
	struct sprd_spi_slave *ss = spi_master_get_devdata(sdev->master);
	u32 trans_len = ss->trans_len, *dma_tmp_txbuf = NULL, val;
	int ret = 0;

	if (ss->trans_mode & SPRD_SPI_TX_MODE) {
		/* The SPI device is used for TX mode.*/

		if (ss->dma_trans_len > t->len) {
			dma_tmp_txbuf = kzalloc(ss->dma_trans_len, GFP_ATOMIC);
			if (!dma_tmp_txbuf) {
				ret = -ENOMEM;
				dev_err(ss->dev,
					"failed to alloc dma_tmp_txbuf, ret = %d\n",
					ret);
				goto trans_complete;
			}

			memcpy(dma_tmp_txbuf, t->tx_buf, t->len);
			ss->dma.dma_phys_addr = dma_map_single(ss->dev,
							(void *)dma_tmp_txbuf,
							ss->dma_trans_len,
							DMA_TO_DEVICE);
		}

		ret = sprd_spi_dma_tx_config(ss, t);
		if (ret < 0) {
			dev_err(ss->dev,
				"failed to config tx DMA, ret = %d\n",
				ret);
			goto trans_complete;
		}
	}

	if (ss->trans_mode & SPRD_SPI_RX_MODE) {
		/* The SPI device is used for RX mode.*/

		ret = sprd_spi_dma_rx_config(ss, t);
		if (ret < 0) {
			dev_err(ss->dev,
				"failed to configure rx DMA, ret = %d\n",
				ret);
			goto trans_complete;
		}
	}

	sprd_spi_dma_enable(ss, true);
	wait_for_completion_interruptible(&ss->xfer_done);

	if (dma_tmp_txbuf != NULL) {
		dma_unmap_single(ss->dev,
				 ss->dma.dma_phys_addr,
				 ss->dma_trans_len,
				 DMA_TO_DEVICE);
		kfree(dma_tmp_txbuf);
	}

trans_complete:
	sprd_spi_dma_enable(ss, false);
	sprd_spi_enter_idle(ss);

	return ret;

}

static int sprd_spi_transfer_one(struct spi_controller *sctlr,
				 struct spi_device *sdev,
				 struct spi_transfer *t)
{
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sctlr);
	int ret;

	ss->slave_aborted = false;
	ss->cur_transfer = t;

	reinit_completion(&ss->xfer_done);
	ret = sprd_spi_setup_transfer(sdev, t);
	if (ret)
		return ret; //error

	if (sctlr->can_dma(sctlr, sdev, t)) {
		/* Align trans_len to fragmens_len */
		ss->dma_trans_len = round_down(t->len, ss->dma.fragmens_len);
		ss->trans_len = round_down(ss->trans_len, ss->dma.fragmens_len);
		ret = sprd_spi_dma_transfer(sdev, t);
	} else if (t->len < SPRD_SPI_FIFO_SIZE) {
		ret = sprd_spi_slave_fifo_transfer(sctlr, sdev, t);
	} else {
		ret = -EREMOTEIO;
		dev_err(ss->dev, "data len is larger then fifo depth, please use dma_mode!\n");
	}

	return ret;
}

static int sprd_slave_abort(struct spi_controller *sctlr)
{
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sctlr);

	/*Clear all interrupt status*/
	writel_relaxed(SPRD_SPI_ALL_INT_CLR, ss->base + SPRD_SPI_INT_CLR);
	ss->slave_aborted = true;
	complete(&ss->xfer_done);

	return 0;
}

static void sprd_spi_slv_read(struct sprd_spi_slave *ss)
{
	int len = 0, ret = 0;

	/*how long should i read*/
	len = sprd_spi_slv_rxf_cnt(ss);
	ret = ss->read_bufs(ss, len);
	ss->trans_len -= len;
}

static irqreturn_t sprd_spi_slave_interrupt(int irq, void *dev_id)
{
	struct sprd_spi_slave *ss = (struct sprd_spi_slave *)dev_id;
	struct spi_transfer *trans = ss->cur_transfer;
	u32 int_status, len, cnt;
	int val = 0;

	int_status = readl_relaxed(ss->base + SPRD_SPI_INT_MASK_STS);
	sprd_spi_irq_disable(ss);
	writel(int_status, ss->base + SPRD_SPI_INT_CLR);

	if (!int_status)
		return IRQ_NONE;

	if (int_status & (SPRD_SPI_MASK_RX_FULL | SPRD_SPI_MASK_TIME_OUT))
		sprd_spi_slv_read(ss);

	if (ss->trans_len <= 0)
		ss->cur_transfer = NULL;

	complete(&ss->xfer_done);

	return IRQ_HANDLED;
}

static int sprd_spi_clk_init(struct platform_device *pdev,
			     struct sprd_spi_slave *ss)
{
	struct clk *clk_spi, *clk_parent, *clk_pad;
	int ret;

	clk_spi = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(clk_spi)) {
		dev_warn(&pdev->dev, "can't get the spi clock\n");
		clk_spi = NULL;
	}

	clk_parent = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(clk_parent)) {
		dev_warn(&pdev->dev, "can't get the source clock\n");
		clk_parent = NULL;
	}

	ss->clk = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(ss->clk)) {
		dev_err(&pdev->dev, "can't get the enable clock\n");
		return PTR_ERR(ss->clk);
	}

	if (!clk_set_parent(clk_spi, clk_parent))
		ss->src_clk = clk_get_rate(clk_spi);
	else
		ss->src_clk = SPRD_SPI_DEFAULT_SOURCE;

	return 0;
}

static int sprd_spi_irq_init(struct platform_device *pdev,
			     struct sprd_spi_slave *ss)
{
	int ret;

	ss->irq = platform_get_irq(pdev, 0);
	if (ss->irq < 0)
		return ss->irq;

	ret = devm_request_irq(&pdev->dev, ss->irq, sprd_spi_slave_interrupt,
			       IRQF_SHARED, pdev->name, ss);
	if (ret)
		dev_err(&pdev->dev, "failed to request spi irq %d, ret = %d\n",
			ss->irq, ret);
	return ret;
}

static bool sprd_spi_can_dma(struct spi_controller *sctlr,
			     struct spi_device *spi, struct spi_transfer *t)
{
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sctlr);

	return ss->dma.enable && (t->len > SPRD_SPI_FIFO_SIZE);
}

static int sprd_spi_dma_init(struct platform_device *pdev,
			     struct sprd_spi_slave *ss)
{
	int ret;

	ret = sprd_spi_dma_request(ss);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			return ret;

		dev_warn(&pdev->dev,
			 "failed to request dma, enter no dma mode, ret = %d\n",
			 ret);

		return 0;
	}

	ss->dma.enable = true;

	return 0;
}

static int sprd_spi_slave_probe(struct platform_device *pdev)
{
	struct spi_controller *sctlr;
	struct resource *res;
	struct sprd_spi_slave *ss;
	u32 spi_is_slave;
	int ret, val;

	pdev->id = of_alias_get_id(pdev->dev.of_node, "spi");
	sctlr = spi_alloc_slave(&pdev->dev, sizeof(*ss));
	if (!sctlr) {
		dev_err(&pdev->dev, "failed to alloc spi slave\n");
		return -ENOMEM;
	}

	ss = spi_controller_get_devdata(sctlr);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ss->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ss->base)) {
		ret = PTR_ERR(ss->base);
		goto free_controller;
	}

	ss->phy_base = res->start;
	ss->dev = &pdev->dev;
	sctlr->dev.of_node = pdev->dev.of_node;
	sctlr->mode_bits = SPI_CPOL | SPI_CPHA | SPI_3WIRE | SPI_TX_DUAL;
	sctlr->bus_num = pdev->id;
	sctlr->auto_runtime_pm = true;
	sctlr->max_speed_hz = min_t(u32, ss->src_clk >> 1,
				    SPRD_SPI_MAX_SPEED_HZ);

	sctlr->transfer_one = sprd_spi_transfer_one;
	sctlr->slave_abort = sprd_slave_abort;
	sctlr->can_dma = sprd_spi_can_dma;

	init_completion(&ss->xfer_done);
	platform_set_drvdata(pdev, sctlr);

	ret = sprd_spi_clk_init(pdev, ss);
	if (ret)
		goto free_controller;

	ret = sprd_spi_irq_init(pdev, ss);
	if (ret)
		goto free_controller;

	ret = sprd_spi_dma_init(pdev, ss);
	if (ret)
		goto release_dma;

	ret = clk_prepare_enable(ss->clk);
	if (ret)
		goto free_controller;

	ret = pm_runtime_set_active(&pdev->dev);
	if (ret < 0)
		goto disable_clk;

	pm_runtime_set_autosuspend_delay(&pdev->dev,
					 SPRD_SPI_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to resume SPI controller\n");
		goto err_rpm_put;
	}

	ret = devm_spi_register_controller(&pdev->dev, sctlr);
	if (ret)
		goto err_rpm_put;

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	return 0;

err_rpm_put:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
release_dma:
	sprd_spi_dma_release(ss);
disable_clk:
	clk_disable_unprepare(ss->clk);
free_controller:
	spi_controller_put(sctlr);

	return ret;
}

static int __exit sprd_spi_slave_remove(struct platform_device *pdev)
{
	struct spi_controller *sctlr = platform_get_drvdata(pdev);
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sctlr);
	int ret;

	ret = pm_runtime_get_sync(ss->dev);
	if (ret < 0) {
		dev_err(ss->dev, "failed to resume SPI controller\n");
		return ret;
	}

	if (ss->dma.enable)
		sprd_spi_dma_release(ss);

	clk_disable_unprepare(ss->clk);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static int __maybe_unused sprd_spi_slave_runtime_suspend(struct device *dev)
{
	struct spi_controller *sctlr = dev_get_drvdata(dev);
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sctlr);

	if (ss->dma.enable)
		sprd_spi_dma_release(ss);
	clk_disable_unprepare(ss->clk);

	return 0;
}

static int __maybe_unused sprd_spi_slave_runtime_resume(struct device *dev)
{
	struct spi_controller *sctlr = dev_get_drvdata(dev);
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sctlr);
	int ret;

	ret = clk_prepare_enable(ss->clk);
	if (!ss->dma.enable)
		return 0;

	ret = sprd_spi_dma_request(ss);
	if (ret)
		return ret;

	return 0;
}

static int __maybe_unused sprd_spi_slave_suspend(struct device *dev)
{
	struct spi_controller *sctlr = dev_get_drvdata(dev);
	int ret;

	ret =  spi_controller_suspend(sctlr);
	if (ret)
		return ret;

	if (pm_runtime_status_suspended(dev))
		return 0;

	return sprd_spi_slave_runtime_suspend(dev);
}

static int __maybe_unused sprd_spi_slave_resume(struct device *dev)
{
	struct spi_controller *sctlr = dev_get_drvdata(dev);
	struct sprd_spi_slave *ss = spi_controller_get_devdata(sctlr);
	int ret;

	if (!pm_runtime_status_suspended(dev)) {
		ret = sprd_spi_slave_runtime_resume(dev);
		if (ret) {
			dev_err(ss->dev, "enable spi failed\n");
			return ret;
		}
	}

	ret = spi_controller_resume(sctlr);
	if (ret)
		clk_disable_unprepare(ss->clk);

	return ret;
}


static const struct dev_pm_ops sprd_spi_slave_pm_ops = {
	SET_RUNTIME_PM_OPS(sprd_spi_slave_runtime_suspend,
			   sprd_spi_slave_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(sprd_spi_slave_suspend,
				sprd_spi_slave_resume)
};

static const struct of_device_id sprd_spi_slave_of_match[] = {
	{ .compatible = "sprd,sc9860-spi-slave", },
	{ /* sentinel */ }
};

static struct platform_driver sprd_spi_slave_driver = {
	.driver = {
		.name = "sprd-spi_slave",
		.of_match_table = sprd_spi_slave_of_match,
		.pm = &sprd_spi_slave_pm_ops,
	},
	.probe = sprd_spi_slave_probe,
	.remove  = sprd_spi_slave_remove,
};

module_platform_driver(sprd_spi_slave_driver);

MODULE_DESCRIPTION("Spreadtrum SPI Controller driver");
MODULE_AUTHOR("Yanbin Xiong <yanbin.xiong@unisoc.com>");
MODULE_LICENSE("GPL v2");
