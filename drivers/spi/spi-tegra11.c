/*
 * Driver for Nvidia TEGRA11 spi controller.
 *
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>

#include <linux/spi/spi.h>
#include <linux/spi-tegra.h>

#include <mach/dma.h>
#include <mach/clk.h>

#define SPI_COMMAND1			0x000
#define SPI_BIT_LENGTH(x)		(((x) & 0x1f) << 0)
#define SPI_PACKED			(1 << 5)
#define SPI_TX_EN			(1 << 11)
#define SPI_RX_EN			(1 << 12)
#define SPI_BOTH_EN_BYTE		(1 << 13)
#define SPI_BOTH_EN_BIT			(1 << 14)
#define SPI_LSBYTE_FE			(1 << 15)
#define SPI_LSBIT_FE			(1 << 16)
#define SPI_BIDIROE			(1 << 17)
#define SPI_IDLE_SDA_DRIVE_LOW		(0 << 18)
#define SPI_IDLE_SDA_DRIVE_HIGH		(1 << 18)
#define SPI_IDLE_SDA_PULL_LOW		(2 << 18)
#define SPI_IDLE_SDA_PULL_HIGH		(3 << 18)
#define SPI_IDLE_SDA_MASK		(3 << 18)
#define SPI_CS_SS_VAL			(1 << 20)
#define SPI_CS_SW_HW			(1 << 21)
/* SPI_CS_POL_INACTIVE bits are default high */
#define SPI_CS_POL_INACTIVE		22
#define SPI_CS_POL_INACTIVE_0		(1 << 22)
#define SPI_CS_POL_INACTIVE_1		(1 << 23)
#define SPI_CS_POL_INACTIVE_2		(1 << 24)
#define SPI_CS_POL_INACTIVE_3		(1 << 25)
#define SPI_CS_POL_INACTIVE_MASK	(0xF << 22)
#define SPI_CS_POL_INACTIVE_SET_LOW(reg, cs)	 \
				(reg &= ~((1 << cs) << 22))

#define SPI_CS_SEL_0			(0 << 26)
#define SPI_CS_SEL_1			(1 << 26)
#define SPI_CS_SEL_2			(2 << 26)
#define SPI_CS_SEL_3			(3 << 26)
#define SPI_CS_SEL_MASK			(3 << 26)
#define SPI_CS_SEL(x)			(((x) & 0x3) << 26)
#define SPI_CONTROL_MODE_0		(0 << 28)
#define SPI_CONTROL_MODE_1		(1 << 28)
#define SPI_CONTROL_MODE_2		(2 << 28)
#define SPI_CONTROL_MODE_3		(3 << 28)
#define SPI_CONTROL_MODE_MASK		(3 << 28)
#define SPI_MODE_SEL(x)			(((x) & 0x3) << 28)
#define SPI_M_S				(1 << 30)
#define SPI_PIO				(1 << 31)

#define SPI_COMMAND2			0x004
#define SPI_TX_TAP_DELAY(x)		(((x) & 0x3F) << 6)
#define SPI_RX_TAP_DELAY(x)		(((x) & 0x3F) << 0)

#define SPI_CS_TIMING1			0x008
#define SPI_SETUP_HOLD(setup, hold)	((setup << 4) | hold)
#define SPI_CS_SETUP_HOLD(reg, cs, val) (((val & 0xFFu) << (cs * 8)) |	\
					(reg & ~(0xFFu << (cs * 8))))

#define SPI_CS_TIMING2			0x00C
#define   CYCLES_BETWEEN_PACKETS_0(x)	(((x) & 0x1F) << 0)
#define   CS_ACTIVE_BETWEEN_PACKETS_0   (1 << 5)
#define   CYCLES_BETWEEN_PACKETS_1(x)	(((x) & 0x1F) << 8)
#define   CS_ACTIVE_BETWEEN_PACKETS_1   (1 << 13)
#define   CYCLES_BETWEEN_PACKETS_2(x)	(((x) & 0x1F) << 16)
#define   CS_ACTIVE_BETWEEN_PACKETS_2   (1 << 21)
#define   CYCLES_BETWEEN_PACKETS_3(x)	(((x) & 0x1F) << 24)
#define   CS_ACTIVE_BETWEEN_PACKETS_3   (1 << 29)
#define SPI_SET_CS_ACTIVE_BETWEEN_PACKETS(reg, cs, val)		\
		   (reg = ((val & 0x1) << (cs*8 + 5)) |	\
					(reg & ~(1 << (cs*8 + 5))))
#define SPI_SET_CYCLES_BETWEEN_PACKETS(reg, cs, val)		\
			(reg = ((val & 0xF) << (cs*8)) |	\
			(reg & ~(0xF << (cs*8))))

#define SPI_TRANS_STATUS		0x010
#define SPI_BLK_CNT(val)		(((val) >> 0) & 0xFFFF)
#define SPI_SLV_IDLE_COUNT(val)		((val >> 16) & 0xFF)
#define SPI_RDY				(1 << 30)

#define SPI_FIFO_STATUS			0x014
#define SPI_RX_FIFO_EMPTY		(1 << 0)
#define SPI_RX_FIFO_FULL		(1 << 1)
#define SPI_TX_FIFO_EMPTY		(1 << 2)
#define SPI_TX_FIFO_FULL		(1 << 3)
#define SPI_RX_FIFO_UNF			(1 << 4)
#define SPI_RX_FIFO_OVF			(1 << 5)
#define SPI_TX_FIFO_UNF			(1 << 6)
#define SPI_TX_FIFO_OVF			(1 << 7)
#define SPI_ERR				(1 << 8)
#define SPI_TX_FIFO_FLUSH		(1 << 14)
#define SPI_RX_FIFO_FLUSH		(1 << 15)
#define SPI_TX_FIFO_EMPTY_COUNT(val)	((val >> 16) & 0x7F)
#define SPI_RX_FIFO_FULL_COUNT(val)	((val >> 23) & 0x7F)
#define SPI_FRAME_END			(1 << 30)
#define SPI_CS_INACTIVE			(1 << 31)

#define SPI_TX_DATA			0x018
#define SPI_RX_DATA			0x01C

#define SPI_DMA_CTL			0x020
#define SPI_TX_TRIG_1			(0 << 15)
#define SPI_TX_TRIG_4			(1 << 15)
#define SPI_TX_TRIG_8			(2 << 15)
#define SPI_TX_TRIG_16			(3 << 15)
#define SPI_TX_TRIG_MASK		(3 << 15)
#define SPI_RX_TRIG_1			(0 << 19)
#define SPI_RX_TRIG_4			(1 << 19)
#define SPI_RX_TRIG_8			(2 << 19)
#define SPI_RX_TRIG_16			(3 << 19)
#define SPI_RX_TRIG_MASK		(3 << 19)
#define SPI_IE_TX			(1 << 28)
#define SPI_IE_RX			(1 << 29)
#define SPI_CONT			(1 << 30)
#define SPI_DMA				(1 << 31)
#define SPI_DMA_EN			SPI_DMA

#define SPI_DMA_BLK			0x024
#define SPI_DMA_BLK_SET(x)		(((x) & 0xFFFF) << 0)

#define SPI_TX_FIFO			0x108
#define SPI_RX_FIFO			0x188
#define MAX_CHIP_SELECT			4
#define SPI_FIFO_DEPTH			64
#define DATA_DIR_TX			(1 << 0)
#define DATA_DIR_RX			(1 << 1)

#define SPI_DMA_TIMEOUT (msecs_to_jiffies(1000))
#define DEFAULT_SPI_DMA_BUF_LEN		(16*1024)
#define TX_FIFO_EMPTY_COUNT_MAX		SPI_TX_FIFO_EMPTY_COUNT(0x20)
#define RX_FIFO_FULL_COUNT_ZERO		SPI_RX_FIFO_FULL_COUNT(0)
#define MAX_HOLD_CYCLES			16
#define SPI_DEFAULT_SPEED		25000000

static const unsigned long spi_tegra_req_sels[] = {
	TEGRA_DMA_REQ_SEL_SL2B1,
	TEGRA_DMA_REQ_SEL_SL2B2,
	TEGRA_DMA_REQ_SEL_SL2B3,
	TEGRA_DMA_REQ_SEL_SL2B4,
	TEGRA_DMA_REQ_SEL_SL2B5,
	TEGRA_DMA_REQ_SEL_SL2B6,
};

struct spi_tegra_data {
	struct spi_master	*master;
	struct platform_device	*pdev;
	spinlock_t		lock;
	spinlock_t		reg_lock;
	char			port_name[32];

	struct clk		*clk;
	struct clk		*sclk;
	void __iomem		*base;
	phys_addr_t		phys;
	unsigned		irq;

	u32			cur_speed;

	struct list_head	queue;
	struct spi_transfer	*cur;
	struct spi_device	*cur_spi;
	unsigned		cur_pos;
	unsigned		cur_len;
	unsigned		words_per_32bit;
	unsigned		bytes_per_word;
	unsigned		curr_dma_words;

	unsigned		cur_direction;

	bool			is_dma_allowed;

	struct tegra_dma_req	rx_dma_req;
	struct tegra_dma_channel *rx_dma;
	u32			*rx_buf;
	dma_addr_t		rx_buf_phys;
	unsigned		cur_rx_pos;

	struct tegra_dma_req	tx_dma_req;
	struct tegra_dma_channel *tx_dma;
	u32			*tx_buf;
	dma_addr_t		tx_buf_phys;
	unsigned		cur_tx_pos;

	unsigned		dma_buf_size;
	unsigned		max_buf_size;
	bool			is_curr_dma_xfer;

	bool			is_clkon_always;
	int			clk_state;
	bool			is_suspended;

	bool			is_hw_based_cs;

	struct completion	rx_dma_complete;
	struct completion	tx_dma_complete;
	bool			is_transfer_in_progress;

	u32			rx_complete;
	u32			tx_complete;
	u32			tx_status;
	u32			rx_status;
	u32			status_reg;
	bool			is_packed;

	u32			command1_reg;
	u32			dma_control_reg;
	u32			def_command1_reg;
	u32			def_command2_reg;
	u32			spi_cs_timing;

	struct spi_clk_parent	*parent_clk_list;
	int			parent_clk_count;
	unsigned long		max_rate;
	unsigned long		max_parent_rate;
	int			min_div;
	struct workqueue_struct *spi_workqueue;
	struct work_struct spi_transfer_work;
};

static inline unsigned long spi_tegra_readl(struct spi_tegra_data *tspi,
		    unsigned long reg)
{
	unsigned long flags;
	unsigned long val;

	spin_lock_irqsave(&tspi->reg_lock, flags);
	if (tspi->clk_state < 1)
		BUG();
	val = readl(tspi->base + reg);
	spin_unlock_irqrestore(&tspi->reg_lock, flags);
	return val;
}

static inline void spi_tegra_writel(struct spi_tegra_data *tspi,
		    unsigned long val, unsigned long reg)
{
	unsigned long flags;

	spin_lock_irqsave(&tspi->reg_lock, flags);
	if (tspi->clk_state < 1)
		BUG();
	writel(val, tspi->base + reg);

	/* Synchronize write by reading back the register */
	readl(tspi->base + SPI_COMMAND1);
	spin_unlock_irqrestore(&tspi->reg_lock, flags);
}

static int tegra_spi_clk_disable(struct spi_tegra_data *tspi)
{
	unsigned long flags;

	/* Flush all write which are in PPSB queue by reading back */
	spi_tegra_readl(tspi, SPI_COMMAND1);

	spin_lock_irqsave(&tspi->reg_lock, flags);
	tspi->clk_state--;
	spin_unlock_irqrestore(&tspi->reg_lock, flags);
	clk_disable(tspi->clk);
	clk_disable(tspi->sclk);
	return 0;
}

static int tegra_spi_clk_enable(struct spi_tegra_data *tspi)
{
	unsigned long flags;

	clk_enable(tspi->sclk);
	clk_enable(tspi->clk);
	spin_lock_irqsave(&tspi->reg_lock, flags);
	tspi->clk_state++;
	spin_unlock_irqrestore(&tspi->reg_lock, flags);
	return 0;
}

static void cancel_dma(struct tegra_dma_channel *dma_chan,
	struct tegra_dma_req *req)
{
	tegra_dma_cancel(dma_chan);
	if (req->status == -TEGRA_DMA_REQ_ERROR_ABORTED)
		req->complete(req);
}

static void spi_tegra_clear_status(struct spi_tegra_data *tspi)
{
	unsigned long val;
	unsigned long clear_err = 0;

	val = spi_tegra_readl(tspi, SPI_TRANS_STATUS);
	if (val & SPI_RDY)
		spi_tegra_writel(tspi, val, SPI_TRANS_STATUS);

	/* Save transfer fifo status and clear error bits */
	val = spi_tegra_readl(tspi, SPI_FIFO_STATUS);
	if (val & SPI_ERR) {
		clear_err |= SPI_ERR;
		if (val & SPI_TX_FIFO_UNF)
			clear_err |= SPI_TX_FIFO_UNF;
		if (val & SPI_TX_FIFO_OVF)
			clear_err |= SPI_TX_FIFO_OVF;
		if (val & SPI_RX_FIFO_UNF)
			clear_err |= SPI_RX_FIFO_UNF;
		if (val & SPI_RX_FIFO_OVF)
			clear_err |= SPI_RX_FIFO_OVF;
		spi_tegra_writel(tspi, clear_err, SPI_FIFO_STATUS);
	}
}

static unsigned spi_tegra_calculate_curr_xfer_param(
	struct spi_device *spi, struct spi_tegra_data *tspi,
	struct spi_transfer *t)
{
	unsigned remain_len = t->len - tspi->cur_pos;
	unsigned max_word;
	unsigned bits_per_word ;
	unsigned max_len;
	unsigned total_fifo_words;

	bits_per_word = t->bits_per_word ? t->bits_per_word :
						spi->bits_per_word;
	tspi->bytes_per_word = (bits_per_word - 1) / 8 + 1;

	if (bits_per_word == 8 || bits_per_word == 16) {
		tspi->is_packed = 1;
		tspi->words_per_32bit = 32/bits_per_word;
	} else {
		tspi->is_packed = 0;
		tspi->words_per_32bit = 1;
	}

	if (tspi->is_packed) {
		max_len = min(remain_len, tspi->max_buf_size);
		tspi->curr_dma_words = max_len/tspi->bytes_per_word;
		total_fifo_words = max_len/4;
	} else {
		max_word = (remain_len - 1) / tspi->bytes_per_word + 1;
		max_word = min(max_word, tspi->max_buf_size/4);
		tspi->curr_dma_words = max_word;
		total_fifo_words = max_word;
	}
	return total_fifo_words;
}

static unsigned spi_tegra_fill_tx_fifo_from_client_txbuf(
	struct spi_tegra_data *tspi, struct spi_transfer *t)
{
	unsigned nbytes;
	unsigned tx_empty_count;
	unsigned long fifo_status;
	u8 *tx_buf = (u8 *)t->tx_buf + tspi->cur_tx_pos;
	unsigned max_n_32bit;
	unsigned i, count;
	unsigned long x;
	unsigned int written_words;

	fifo_status = spi_tegra_readl(tspi, SPI_FIFO_STATUS);
	tx_empty_count = SPI_TX_FIFO_EMPTY_COUNT(fifo_status);

	if (tspi->is_packed) {
		nbytes = tspi->curr_dma_words * tspi->bytes_per_word;
		max_n_32bit = (min(nbytes,  tx_empty_count*4) - 1)/4 + 1;
		for (count = 0; count < max_n_32bit; ++count) {
			x = 0;
			for (i = 0; (i < 4) && nbytes; i++, nbytes--)
				x |= (*tx_buf++) << (i*8);
			spi_tegra_writel(tspi, x, SPI_TX_FIFO);
		}
		written_words =  min(max_n_32bit * tspi->words_per_32bit,
					tspi->curr_dma_words);
	} else {
		max_n_32bit = min(tspi->curr_dma_words,  tx_empty_count);
		nbytes = max_n_32bit * tspi->bytes_per_word;
		for (count = 0; count < max_n_32bit; ++count) {
			x = 0;
			for (i = 0; nbytes && (i < tspi->bytes_per_word);
							++i, nbytes--)
				x |= ((*tx_buf++) << i*8);
			spi_tegra_writel(tspi, x, SPI_TX_FIFO);
		}
		written_words = max_n_32bit;
	}
	tspi->cur_tx_pos += written_words * tspi->bytes_per_word;
	return written_words;
}

static unsigned int spi_tegra_read_rx_fifo_to_client_rxbuf(
		struct spi_tegra_data *tspi, struct spi_transfer *t)
{
	unsigned rx_full_count;
	unsigned long fifo_status;
	u8 *rx_buf = (u8 *)t->rx_buf + tspi->cur_rx_pos;
	unsigned i, count;
	unsigned long x;
	unsigned int read_words = 0;
	unsigned len;

	fifo_status = spi_tegra_readl(tspi, SPI_FIFO_STATUS);
	rx_full_count = SPI_RX_FIFO_FULL_COUNT(fifo_status);
	dev_dbg(&tspi->pdev->dev, "Rx fifo count %d\n", rx_full_count);
	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		for (count = 0; count < rx_full_count; ++count) {
			x = spi_tegra_readl(tspi, SPI_RX_FIFO);
			for (i = 0; len && (i < 4); ++i, len--)
				*rx_buf++ = (x >> i*8) & 0xFF;
		}
		tspi->cur_rx_pos += tspi->curr_dma_words * tspi->bytes_per_word;
		read_words += tspi->curr_dma_words;
	} else {
		unsigned int rx_mask, bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;
		rx_mask = (1 << bits_per_word) - 1;
		for (count = 0; count < rx_full_count; ++count) {
			x = spi_tegra_readl(tspi, SPI_RX_FIFO);
			x &= rx_mask;
			for (i = 0; (i < tspi->bytes_per_word); ++i)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
		tspi->cur_rx_pos += rx_full_count * tspi->bytes_per_word;
		read_words += rx_full_count;
	}
	return read_words;
}

static void spi_tegra_copy_client_txbuf_to_spi_txbuf(
		struct spi_tegra_data *tspi, struct spi_transfer *t)
{
	unsigned len;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(&tspi->pdev->dev, tspi->tx_buf_phys,
				tspi->dma_buf_size, DMA_TO_DEVICE);

	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		if (t->tx_buf)
			memcpy(tspi->tx_buf, t->tx_buf + tspi->cur_pos, len);
	} else {
		unsigned int i;
		unsigned int count;
		u8 *tx_buf = (u8 *)t->tx_buf + tspi->cur_tx_pos;
		unsigned consume = tspi->curr_dma_words * tspi->bytes_per_word;
		unsigned int x;

		for (count = 0; count < tspi->curr_dma_words; ++count) {
			x = 0;
			for (i = 0; consume && (i < tspi->bytes_per_word);
							++i, consume--)
				x |= ((*tx_buf++) << i*8);
			tspi->tx_buf[count] = x;
		}
	}
	tspi->cur_tx_pos += tspi->curr_dma_words * tspi->bytes_per_word;

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(&tspi->pdev->dev, tspi->tx_buf_phys,
				tspi->dma_buf_size, DMA_TO_DEVICE);
}

static void spi_tegra_copy_spi_rxbuf_to_client_rxbuf(
		struct spi_tegra_data *tspi, struct spi_transfer *t)
{
	unsigned len;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(&tspi->pdev->dev, tspi->rx_buf_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);

	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		memcpy(t->rx_buf + tspi->cur_rx_pos, tspi->rx_buf, len);
	} else {
		unsigned int i;
		unsigned int count;
		unsigned char *rx_buf = t->rx_buf + tspi->cur_rx_pos;
		unsigned int x;
		unsigned int rx_mask, bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;
		rx_mask = (1 << bits_per_word) - 1;
		for (count = 0; count < tspi->curr_dma_words; ++count) {
			x = tspi->rx_buf[count];
			x &= rx_mask;
			for (i = 0; (i < tspi->bytes_per_word); ++i)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
	}
	tspi->cur_rx_pos += tspi->curr_dma_words * tspi->bytes_per_word;

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(&tspi->pdev->dev, tspi->rx_buf_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);
}

static int spi_tegra_start_dma_based_transfer(
		struct spi_tegra_data *tspi, struct spi_transfer *t)
{
	unsigned long val;
	unsigned long test_val;
	unsigned int len;
	int ret = 0;

	INIT_COMPLETION(tspi->rx_dma_complete);
	INIT_COMPLETION(tspi->tx_dma_complete);

	/* Make sure that Rx and Tx fifo are empty */
	test_val = spi_tegra_readl(tspi, SPI_FIFO_STATUS);
	if (((test_val >> 16) & 0x3FFF) != 0x40)
		dev_err(&tspi->pdev->dev,
			"The Rx and Tx fifo are not empty status 0x%08lx\n",
				test_val);

	val = SPI_DMA_BLK_SET(tspi->curr_dma_words - 1);
	spi_tegra_writel(tspi, val, SPI_DMA_BLK);

	if (tspi->is_packed)
		len = DIV_ROUND_UP(tspi->curr_dma_words * tspi->bytes_per_word,
					4) * 4;
	else
		len = tspi->curr_dma_words * 4;


	if (len & 0xF)
		val = SPI_TX_TRIG_1 | SPI_RX_TRIG_1;
	else if (((len) >> 4) & 0x1)
		val = SPI_TX_TRIG_4 | SPI_RX_TRIG_4;
	else
		val = SPI_TX_TRIG_8 | SPI_RX_TRIG_8;

	if (tspi->cur_direction & DATA_DIR_TX)
		val |= SPI_IE_TX;

	if (tspi->cur_direction & DATA_DIR_RX)
		val |= SPI_IE_RX;

	spi_tegra_writel(tspi, val, SPI_DMA_CTL);
	tspi->dma_control_reg = val;

	if (tspi->cur_direction & DATA_DIR_TX) {
		spi_tegra_copy_client_txbuf_to_spi_txbuf(tspi, t);
		wmb();
		tspi->tx_dma_req.size = len;
		ret = tegra_dma_enqueue_req(tspi->tx_dma, &tspi->tx_dma_req);
		if (ret < 0) {
			dev_err(&tspi->pdev->dev,
				"Error in starting tx dma error = %d\n", ret);
			return ret;
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		/* Make the dma buffer to read by dma */
		dma_sync_single_for_device(&tspi->pdev->dev, tspi->rx_buf_phys,
				tspi->dma_buf_size, DMA_FROM_DEVICE);

		tspi->rx_dma_req.size = len;
		ret = tegra_dma_enqueue_req(tspi->rx_dma, &tspi->rx_dma_req);
		if (ret < 0) {
			dev_err(&tspi->pdev->dev,
				"Error in starting rx dma error = %d\n", ret);
			if (tspi->cur_direction & DATA_DIR_TX)
				cancel_dma(tspi->tx_dma, &tspi->tx_dma_req);
			return ret;
		}
	}
	tspi->is_curr_dma_xfer = true;
	tspi->dma_control_reg = val;

	val |= SPI_DMA;
	spi_tegra_writel(tspi, val, SPI_DMA_CTL);
	return ret;
}

static int spi_tegra_start_cpu_based_transfer(
		struct spi_tegra_data *tspi, struct spi_transfer *t)
{
	unsigned long val;
	unsigned curr_words;

	if (tspi->cur_direction & DATA_DIR_TX)
		curr_words = spi_tegra_fill_tx_fifo_from_client_txbuf(tspi, t);
	else
		curr_words = tspi->curr_dma_words;
	val = SPI_DMA_BLK_SET(curr_words - 1);
	spi_tegra_writel(tspi, val, SPI_DMA_BLK);

	val = 0;
	if (tspi->cur_direction & DATA_DIR_TX)
		val |= SPI_IE_TX;

	if (tspi->cur_direction & DATA_DIR_RX)
		val |= SPI_IE_RX;

	spi_tegra_writel(tspi, val, SPI_DMA_CTL);
	tspi->dma_control_reg = val;

	tspi->is_curr_dma_xfer = false;
	val = tspi->command1_reg;
	val |= SPI_PIO;
	spi_tegra_writel(tspi, val, SPI_COMMAND1);
	return 0;
}

static void set_best_clk_source(struct spi_tegra_data *tspi,
		unsigned long speed)
{
	long new_rate;
	unsigned long err_rate;
	int rate = speed;
	unsigned int fin_err = speed;
	int final_index = -1;
	int count;
	int ret;
	struct clk *pclk;
	unsigned long prate, crate, nrate;
	unsigned long cdiv;

	if (!tspi->parent_clk_count || !tspi->parent_clk_list)
		return;

	/* make sure divisor is more than min_div */
	pclk = clk_get_parent(tspi->clk);
	prate = clk_get_rate(pclk);
	crate = clk_get_rate(tspi->clk);
	cdiv = DIV_ROUND_UP(prate, crate);
	if (cdiv < tspi->min_div) {
		nrate = DIV_ROUND_UP(prate, tspi->min_div);
		clk_set_rate(tspi->clk, nrate);
	}

	for (count = 0; count < tspi->parent_clk_count; ++count) {
		if (!tspi->parent_clk_list[count].parent_clk)
			continue;
		ret = clk_set_parent(tspi->clk,
			tspi->parent_clk_list[count].parent_clk);
		if (ret < 0) {
			dev_warn(&tspi->pdev->dev,
				"Error in setting parent clk src %s\n",
				tspi->parent_clk_list[count].name);
			continue;
		}

		new_rate = clk_round_rate(tspi->clk, rate);
		if (new_rate < 0)
			continue;

		err_rate = abs(new_rate - rate);
		if (err_rate < fin_err) {
			final_index = count;
			fin_err = err_rate;
		}
	}

	if (final_index >= 0) {
		dev_dbg(&tspi->pdev->dev, "Setting clk_src %s\n",
				tspi->parent_clk_list[final_index].name);
		clk_set_parent(tspi->clk,
			tspi->parent_clk_list[final_index].parent_clk);
	}
}

static void spi_tegra_start_transfer(struct spi_device *spi,
		    struct spi_transfer *t, bool is_first_of_msg,
		    bool is_single_xfer)
{
	struct spi_tegra_data *tspi = spi_master_get_devdata(spi->master);
	u32 speed;
	u8 bits_per_word;
	unsigned total_fifo_words;
	int ret;
	unsigned long command1;
	int req_mode;
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;

	bits_per_word = t->bits_per_word ? t->bits_per_word :
					spi->bits_per_word;

	speed = t->speed_hz ? t->speed_hz : spi->max_speed_hz;
	if (!speed)
		speed = tspi->max_rate;
	if (!speed)
		speed = SPI_DEFAULT_SPEED;
	if (speed != tspi->cur_speed) {
		set_best_clk_source(tspi, speed);
		clk_set_rate(tspi->clk, speed);
		tspi->cur_speed = speed;
	}

	tspi->cur = t;
	tspi->cur_spi = spi;
	tspi->cur_pos = 0;
	tspi->cur_rx_pos = 0;
	tspi->cur_tx_pos = 0;
	tspi->rx_complete = 0;
	tspi->tx_complete = 0;
	total_fifo_words = spi_tegra_calculate_curr_xfer_param(spi, tspi, t);

	if (is_first_of_msg) {
		pm_runtime_get_sync(&tspi->pdev->dev);
		tegra_spi_clk_enable(tspi);

		spi_tegra_clear_status(tspi);

		command1 = tspi->def_command1_reg;
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);

		command1 &= ~SPI_CONTROL_MODE_MASK;
		req_mode = spi->mode & 0x3;
		if (req_mode == SPI_MODE_0)
			command1 |= SPI_CONTROL_MODE_0;
		else if (req_mode == SPI_MODE_1)
			command1 |= SPI_CONTROL_MODE_1;
		else if (req_mode == SPI_MODE_2)
			command1 |= SPI_CONTROL_MODE_2;
		else if (req_mode == SPI_MODE_3)
			command1 |= SPI_CONTROL_MODE_3;

		spi_tegra_writel(tspi, command1, SPI_COMMAND1);

		/* possibly use the hw based chip select */
		tspi->is_hw_based_cs = false;
		if (cdata && cdata->is_hw_based_cs && is_single_xfer) {
			if ((tspi->curr_dma_words * tspi->bytes_per_word) ==
						(t->len - tspi->cur_pos)) {
				u32 set_count;
				u32 hold_count;
				u32 spi_cs_timing;
				u32 spi_cs_setup;

				set_count = min(cdata->cs_setup_clk_count, 16);
				if (set_count)
					set_count--;

				hold_count = min(cdata->cs_hold_clk_count, 16);
				if (hold_count)
					hold_count--;

				spi_cs_setup = SPI_SETUP_HOLD(set_count,
						hold_count);
				spi_cs_timing = tspi->spi_cs_timing;
				spi_cs_timing = SPI_CS_SETUP_HOLD(spi_cs_timing,
							spi->chip_select,
							spi_cs_setup);
				tspi->spi_cs_timing = spi_cs_timing;
				spi_tegra_writel(tspi, spi_cs_timing,
							SPI_CS_TIMING1);
				tspi->is_hw_based_cs = true;
			}
		}
		if (!tspi->is_hw_based_cs) {
			command1 |= SPI_CS_SW_HW;
			if (spi->mode & SPI_CS_HIGH)
				command1 |= SPI_CS_SS_VAL;
			else
				command1 &= ~SPI_CS_SS_VAL;
		} else {
			command1 &= ~SPI_CS_SW_HW;
			command1 &= ~SPI_CS_SS_VAL;
		}

		if (cdata) {
			u32 command2_reg;
			u32 rx_tap_delay;
			u32 tx_tap_delay;

			rx_tap_delay = min(cdata->rx_clk_tap_delay, 63);
			tx_tap_delay = min(cdata->tx_clk_tap_delay, 63);
			command2_reg = SPI_TX_TAP_DELAY(tx_tap_delay) |
					SPI_RX_TAP_DELAY(tx_tap_delay);
			spi_tegra_writel(tspi, command2_reg, SPI_COMMAND2);
		} else {
			spi_tegra_writel(tspi, tspi->def_command2_reg, SPI_COMMAND2);
		}

	} else {
		command1 = tspi->command1_reg;
		command1 &= ~SPI_BIT_LENGTH(~0);
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);
	}

	if (bits_per_word == 8)
		command1 |= 0; //SPI_LSBYTE_FE;
	if (tspi->is_packed)
		command1 |= SPI_PACKED;

	command1 &= ~(SPI_CS_SEL_MASK | SPI_TX_EN | SPI_RX_EN);
	tspi->cur_direction = 0;
	if (t->rx_buf) {
		command1 |= SPI_RX_EN;
		tspi->cur_direction |= DATA_DIR_RX;
	}
	if (t->tx_buf) {
		command1 |= SPI_TX_EN;
		tspi->cur_direction |= DATA_DIR_TX;
	}
	command1 |= SPI_CS_SEL(spi->chip_select);
	spi_tegra_writel(tspi, command1, SPI_COMMAND1);
	tspi->command1_reg = command1;

	dev_dbg(&tspi->pdev->dev, "The def 0x%x and written 0x%lx\n",
				tspi->def_command1_reg, command1);

	if (total_fifo_words > SPI_FIFO_DEPTH)
		ret = spi_tegra_start_dma_based_transfer(tspi, t);
	else
		ret = spi_tegra_start_cpu_based_transfer(tspi, t);
	WARN_ON(ret < 0);
}

static int spi_tegra_setup(struct spi_device *spi)
{
	struct spi_tegra_data *tspi = spi_master_get_devdata(spi->master);
	unsigned long cs_bit;
	unsigned long val;
	unsigned long flags;

	dev_info(&spi->dev, "setup %d bpw, %scpol, %scpha, %dHz\n",
		spi->bits_per_word,
		spi->mode & SPI_CPOL ? "" : "~",
		spi->mode & SPI_CPHA ? "" : "~",
		spi->max_speed_hz);

	BUG_ON(spi->chip_select >= MAX_CHIP_SELECT);
	switch (spi->chip_select) {
	case 0:
		cs_bit = SPI_CS_POL_INACTIVE_0;
		break;

	case 1:
		cs_bit = SPI_CS_POL_INACTIVE_1;
		break;

	case 2:
		cs_bit = SPI_CS_POL_INACTIVE_2;
		break;

	case 3:
		cs_bit = SPI_CS_POL_INACTIVE_3;
		break;

	default:
		return -EINVAL;
	}

	pm_runtime_get_sync(&tspi->pdev->dev);
	tegra_spi_clk_enable(tspi);

	spin_lock_irqsave(&tspi->lock, flags);
	val = tspi->def_command1_reg;
	if (spi->mode & SPI_CS_HIGH)
		val &= ~cs_bit;
	else
		val |= cs_bit;
	tspi->def_command1_reg = val;
	spi_tegra_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	spin_unlock_irqrestore(&tspi->lock, flags);

	tegra_spi_clk_disable(tspi);
	pm_runtime_put_sync(&tspi->pdev->dev);
	return 0;
}

static void tegra_spi_transfer_work(struct work_struct *work)
{
	struct spi_tegra_data *tspi;
	struct spi_device *spi;
	struct spi_message *m;
	struct spi_transfer *t;
	int single_xfer = 0;
	unsigned long flags;

	tspi = container_of(work, struct spi_tegra_data, spi_transfer_work);

	spin_lock_irqsave(&tspi->lock, flags);

	if (tspi->is_transfer_in_progress || tspi->is_suspended) {
		spin_unlock_irqrestore(&tspi->lock, flags);
		return;
	}
	if (list_empty(&tspi->queue)) {
		spin_unlock_irqrestore(&tspi->lock, flags);
		return;
	}

	m = list_first_entry(&tspi->queue, struct spi_message, queue);
	spi = m->state;
	single_xfer = list_is_singular(&m->transfers);
	m->actual_length = 0;
	m->status = 0;
	t = list_first_entry(&m->transfers, struct spi_transfer, transfer_list);
	tspi->is_transfer_in_progress = true;

	spin_unlock_irqrestore(&tspi->lock, flags);
	spi_tegra_start_transfer(spi, t, true, single_xfer);
}

static int spi_tegra_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct spi_tegra_data *tspi = spi_master_get_devdata(spi->master);
	struct spi_transfer *t;
	unsigned long flags;
	int was_empty;
	int bytes_per_word;

	if (list_empty(&m->transfers) || !m->complete)
		return -EINVAL;

	list_for_each_entry(t, &m->transfers, transfer_list) {
		if (t->bits_per_word < 0 || t->bits_per_word > 32)
			return -EINVAL;

		if (t->len == 0)
			return -EINVAL;

		/* Check that the all words are available */
		if (t->bits_per_word)
			bytes_per_word = (t->bits_per_word + 7)/8;
		else
			bytes_per_word = (spi->bits_per_word + 7)/8;

		if (t->len % bytes_per_word != 0)
			return -EINVAL;

		if (!t->rx_buf && !t->tx_buf)
			return -EINVAL;
	}

	spin_lock_irqsave(&tspi->lock, flags);

	if (WARN_ON(tspi->is_suspended)) {
		spin_unlock_irqrestore(&tspi->lock, flags);
		return -EBUSY;
	}

	m->state = spi;
	was_empty = list_empty(&tspi->queue);
	list_add_tail(&m->queue, &tspi->queue);
	if (was_empty)
		queue_work(tspi->spi_workqueue, &tspi->spi_transfer_work);

	spin_unlock_irqrestore(&tspi->lock, flags);
	return 0;
}

static void spi_tegra_curr_transfer_complete(struct spi_tegra_data *tspi,
	unsigned err, unsigned cur_xfer_size, unsigned long *irq_flags)
{
	struct spi_message *m;
	struct spi_device *spi;
	struct spi_transfer *t;
	int single_xfer = 0;

	/* Check if CS need to be toggele here */
	if (tspi->cur && tspi->cur->cs_change &&
				tspi->cur->delay_usecs) {
		udelay(tspi->cur->delay_usecs);
	}

	m = list_first_entry(&tspi->queue, struct spi_message, queue);
	if (err)
		m->status = -EIO;
	spi = m->state;

	m->actual_length += cur_xfer_size;

	if (tspi->cur &&
		!list_is_last(&tspi->cur->transfer_list, &m->transfers)) {
		tspi->cur = list_first_entry(&tspi->cur->transfer_list,
			struct spi_transfer, transfer_list);
		spin_unlock_irqrestore(&tspi->lock, *irq_flags);
		spi_tegra_start_transfer(spi, tspi->cur, false, 0);
		spin_lock_irqsave(&tspi->lock, *irq_flags);
	} else {
		list_del(&m->queue);
		m->complete(m->context);
		if (!list_empty(&tspi->queue)) {
			if (tspi->is_suspended) {
				spi_tegra_writel(tspi, tspi->def_command1_reg,
						SPI_COMMAND1);
				tspi->is_transfer_in_progress = false;
				return;
			}
			m = list_first_entry(&tspi->queue, struct spi_message,
				queue);
			spi = m->state;
			single_xfer = list_is_singular(&m->transfers);
			m->actual_length = 0;
			m->status = 0;

			t = list_first_entry(&m->transfers, struct spi_transfer,
						transfer_list);
			spin_unlock_irqrestore(&tspi->lock, *irq_flags);
			spi_tegra_start_transfer(spi, t, true, single_xfer);
			spin_lock_irqsave(&tspi->lock, *irq_flags);
		} else {
			spi_tegra_writel(tspi, tspi->def_command1_reg,
								SPI_COMMAND1);
			/* Provide delay to stablize the signal state */
			spin_unlock_irqrestore(&tspi->lock, *irq_flags);
			udelay(10);
			tegra_spi_clk_disable(tspi);
			pm_runtime_put_sync(&tspi->pdev->dev);
			spin_lock_irqsave(&tspi->lock, *irq_flags);
			tspi->is_transfer_in_progress = false;
			/* Check if any new request has come between
			 * clock disable */
			queue_work(tspi->spi_workqueue,
					&tspi->spi_transfer_work);
		}
	}
	return;
}

static void tegra_spi_tx_dma_complete(struct tegra_dma_req *req)
{
	struct spi_tegra_data *tspi = req->dev;
	complete(&tspi->tx_dma_complete);
}

static void tegra_spi_rx_dma_complete(struct tegra_dma_req *req)
{
	struct spi_tegra_data *tspi = req->dev;
	complete(&tspi->rx_dma_complete);
}

static void handle_cpu_based_xfer(void *context_data)
{
	struct spi_tegra_data *tspi = context_data;
	struct spi_transfer *t = tspi->cur;
	unsigned long flags;

	spin_lock_irqsave(&tspi->lock, flags);
	if (tspi->tx_status ||  tspi->rx_status) {
		dev_err(&tspi->pdev->dev, "%s ERROR bit set 0x%x\n",
					 __func__, tspi->status_reg);
		dev_err(&tspi->pdev->dev, "%s 0x%08x:0x%08x\n",
				__func__, tspi->command1_reg,
				tspi->dma_control_reg);
		tegra_periph_reset_assert(tspi->clk);
		udelay(2);
		tegra_periph_reset_deassert(tspi->clk);
		WARN_ON(1);
		spi_tegra_curr_transfer_complete(tspi,
			tspi->tx_status ||  tspi->rx_status, t->len, &flags);
		goto exit;
	}

	dev_vdbg(&tspi->pdev->dev, "Current direction %x\n",
					tspi->cur_direction);
	if (tspi->cur_direction & DATA_DIR_RX)
		spi_tegra_read_rx_fifo_to_client_rxbuf(tspi, t);

	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->cur_pos = tspi->cur_tx_pos;
	else if (tspi->cur_direction & DATA_DIR_RX)
		tspi->cur_pos = tspi->cur_rx_pos;
	else
		WARN_ON(1);

	dev_vdbg(&tspi->pdev->dev,
		"current position %d and length of the transfer %d\n",
			tspi->cur_pos, t->len);
	if (tspi->cur_pos == t->len) {
		spi_tegra_curr_transfer_complete(tspi,
			tspi->tx_status || tspi->rx_status, t->len, &flags);
		goto exit;
	}

	spi_tegra_calculate_curr_xfer_param(tspi->cur_spi, tspi, t);
	spi_tegra_start_cpu_based_transfer(tspi, t);
exit:
	spin_unlock_irqrestore(&tspi->lock, flags);
	return;
}

static irqreturn_t spi_tegra_isr_thread(int irq, void *context_data)
{
	struct spi_tegra_data *tspi = context_data;
	struct spi_transfer *t = tspi->cur;
	long wait_status;
	int err = 0;
	unsigned total_fifo_words;
	unsigned long flags;

	if (!tspi->is_curr_dma_xfer) {
		handle_cpu_based_xfer(context_data);
		return IRQ_HANDLED;
	}

	/* Abort dmas if any error */
	if (tspi->cur_direction & DATA_DIR_TX) {
		if (tspi->tx_status) {
			cancel_dma(tspi->tx_dma, &tspi->tx_dma_req);
			err += 1;
		} else {
			wait_status = wait_for_completion_interruptible_timeout(
				&tspi->tx_dma_complete, SPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				cancel_dma(tspi->tx_dma, &tspi->tx_dma_req);
				dev_err(&tspi->pdev->dev,
					"Error in Dma Tx transfer\n");
				err += 1;
			}
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		if (tspi->rx_status) {
			cancel_dma(tspi->rx_dma, &tspi->rx_dma_req);
			err += 2;
		} else {
			wait_status = wait_for_completion_interruptible_timeout(
				&tspi->rx_dma_complete, SPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				cancel_dma(tspi->rx_dma, &tspi->rx_dma_req);
				dev_err(&tspi->pdev->dev,
					"Error in Dma Rx transfer\n");
				err += 2;
			}
		}
	}

	spin_lock_irqsave(&tspi->lock, flags);
	if (err) {
		dev_err(&tspi->pdev->dev, "%s ERROR bit set 0x%x\n",
					 __func__, tspi->status_reg);
		dev_err(&tspi->pdev->dev, "%s 0x%08x:0x%08x\n",
				__func__, tspi->command1_reg,
				tspi->dma_control_reg);
		tegra_periph_reset_assert(tspi->clk);
		udelay(2);
		tegra_periph_reset_deassert(tspi->clk);
		WARN_ON(1);
		spi_tegra_curr_transfer_complete(tspi, err, t->len, &flags);
		spin_unlock_irqrestore(&tspi->lock, flags);
		return IRQ_HANDLED;
	}

	if (tspi->cur_direction & DATA_DIR_RX)
		spi_tegra_copy_spi_rxbuf_to_client_rxbuf(tspi, t);

	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->cur_pos = tspi->cur_tx_pos;
	else if (tspi->cur_direction & DATA_DIR_RX)
		tspi->cur_pos = tspi->cur_rx_pos;
	else
		WARN_ON(1);

	if (tspi->cur_pos == t->len) {
		spi_tegra_curr_transfer_complete(tspi,
			tspi->tx_status || tspi->rx_status, t->len, &flags);
		spin_unlock_irqrestore(&tspi->lock, flags);
		return IRQ_HANDLED;
	}

	/* Continue transfer in current message */
	total_fifo_words = spi_tegra_calculate_curr_xfer_param(tspi->cur_spi,
							tspi, t);
	if (total_fifo_words > SPI_FIFO_DEPTH)
		err = spi_tegra_start_dma_based_transfer(tspi, t);
	else
		err = spi_tegra_start_cpu_based_transfer(tspi, t);

	spin_unlock_irqrestore(&tspi->lock, flags);
	WARN_ON(err < 0);
	return IRQ_HANDLED;
}

static irqreturn_t spi_tegra_isr(int irq, void *context_data)
{
	struct spi_tegra_data *tspi = context_data;

	tspi->status_reg = spi_tegra_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg &
					(SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg &
					(SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF);
	spi_tegra_clear_status(tspi);

	return IRQ_WAKE_THREAD;
}

static void spi_tegra_deinit_dma_param(struct spi_tegra_data *tspi,
	bool dma_to_memory)
{
	struct tegra_dma_channel *tdc;
	u32 *dma_buf;
	dma_addr_t dma_phys;

	if (dma_to_memory) {
		dma_buf = tspi->rx_buf;
		tdc = tspi->rx_dma;
		dma_phys = tspi->rx_buf_phys;
		tspi->rx_dma = NULL;
		tspi->rx_buf = NULL;
	} else {
		dma_buf = tspi->tx_buf;
		tdc = tspi->tx_dma;
		dma_phys = tspi->tx_buf_phys;
		tspi->tx_buf = NULL;
		tspi->tx_dma = NULL;
	}

	dma_free_coherent(&tspi->pdev->dev, tspi->dma_buf_size,
			dma_buf, dma_phys);
	tegra_dma_free_channel(tdc);
}

static int __devinit spi_tegra_init_dma_param(struct spi_tegra_data *tspi,
			bool dma_to_memory)
{
	struct tegra_dma_req *dma_req;
	struct tegra_dma_channel *tdc;
	u32 *dma_buf;
	dma_addr_t dma_phys;

	tdc = tegra_dma_allocate_channel(TEGRA_DMA_MODE_ONESHOT, "spi_%s_%d",
			(dma_to_memory) ? "rx" : "tx", tspi->pdev->id);
	if (!tdc) {
		dev_err(&tspi->pdev->dev, "can not allocate rx dma channel\n");
		return -ENODEV;
	}

	dma_buf = dma_alloc_coherent(&tspi->pdev->dev, tspi->dma_buf_size,
				&dma_phys, GFP_KERNEL);
	if (!dma_buf) {
		dev_err(&tspi->pdev->dev, "can not allocate rx bounce buffer");
		tegra_dma_free_channel(tdc);
		return -ENOMEM;
	}

	dma_req = (dma_to_memory) ? &tspi->rx_dma_req : &tspi->tx_dma_req;
	memset(dma_req, 0, sizeof(*dma_req));

	dma_req->req_sel = spi_tegra_req_sels[tspi->pdev->id];
	dma_req->dev = tspi;
	dma_req->dest_bus_width = 32;
	dma_req->source_bus_width = 32;
	dma_req->to_memory = (dma_to_memory) ? 1 : 0;
	dma_req->virt_addr = dma_buf;
	dma_req->dest_wrap = 0;
	dma_req->source_wrap = 0;

	if (dma_to_memory) {
		dma_req->complete = tegra_spi_rx_dma_complete;
		dma_req->dest_addr = dma_phys;
		dma_req->source_addr = tspi->phys + SPI_RX_FIFO;
		dma_req->source_wrap = 4;
		tspi->rx_buf_phys = dma_phys;
		tspi->rx_buf = dma_buf;
		tspi->rx_dma = tdc;
	} else {
		dma_req->complete = tegra_spi_tx_dma_complete;
		dma_req->dest_addr = tspi->phys + SPI_TX_FIFO;
		dma_req->source_addr = dma_phys;
		dma_req->dest_wrap = 4;
		tspi->tx_buf = dma_buf;
		tspi->tx_buf_phys = dma_phys;
		tspi->tx_dma = tdc;
	}
	return 0;
}

static int __devinit spi_tegra_probe(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct spi_tegra_data	*tspi;
	struct resource		*r;
	struct tegra_spi_platform_data *pdata = pdev->dev.platform_data;
	int ret, spi_irq;
	int i;
	char spi_wq_name[20];

	master = spi_alloc_master(&pdev->dev, sizeof *tspi);
	if (master == NULL) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;

	if (pdev->id != -1)
		master->bus_num = pdev->id;

	master->setup = spi_tegra_setup;
	master->transfer = spi_tegra_transfer;
	master->num_chipselect = MAX_CHIP_SELECT;

	dev_set_drvdata(&pdev->dev, master);
	tspi = spi_master_get_devdata(master);
	tspi->master = master;
	tspi->pdev = pdev;
	tspi->is_transfer_in_progress = false;
	tspi->is_suspended = false;
	spin_lock_init(&tspi->lock);
	spin_lock_init(&tspi->reg_lock);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "No IO memory resource\n");
		ret = -ENODEV;
		goto exit_free_master;
	}
	tspi->phys = r->start;
	tspi->base = devm_request_and_ioremap(&pdev->dev, r);
	if (!tspi->base) {
		dev_err(&pdev->dev,
			"Cannot request memregion/iomap dma address\n");
		ret = -EADDRNOTAVAIL;
		goto exit_free_master;
	}

	spi_irq = platform_get_irq(pdev, 0);
	if (unlikely(spi_irq < 0)) {
		dev_err(&pdev->dev, "can't find irq resource\n");
		ret = -ENXIO;
		goto exit_free_master;
	}
	tspi->irq = spi_irq;

	sprintf(tspi->port_name, "tegra_spi_%d", pdev->id);
	ret = devm_request_threaded_irq(&pdev->dev, tspi->irq,
			spi_tegra_isr, spi_tegra_isr_thread, IRQF_ONESHOT,
			tspi->port_name, tspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
					tspi->irq);
		goto exit_free_master;
	}

	tspi->clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		ret = PTR_ERR(tspi->clk);
		goto exit_free_master;
	}

	tspi->sclk = devm_clk_get(&pdev->dev, "sclk");
	if (IS_ERR(tspi->sclk)) {
		dev_err(&pdev->dev, "can not get sclock\n");
		ret = PTR_ERR(tspi->sclk);
		goto exit_free_master;
	}

	INIT_LIST_HEAD(&tspi->queue);

	if (pdata) {
		tspi->is_clkon_always = pdata->is_clkon_always;
		tspi->is_dma_allowed = pdata->is_dma_based;
		tspi->dma_buf_size = (pdata->max_dma_buffer) ?
				pdata->max_dma_buffer : DEFAULT_SPI_DMA_BUF_LEN;
		tspi->parent_clk_count = pdata->parent_clk_count;
		tspi->parent_clk_list = pdata->parent_clk_list;
		tspi->max_rate = pdata->max_rate;
	} else {
		tspi->is_clkon_always = false;
		tspi->is_dma_allowed = true;
		tspi->dma_buf_size = DEFAULT_SPI_DMA_BUF_LEN;
		tspi->parent_clk_count = 0;
		tspi->parent_clk_list = NULL;
		tspi->max_rate = SPI_DEFAULT_SPEED;
	}

	tspi->max_parent_rate = 0;
	tspi->min_div = 0;

	if (tspi->parent_clk_count) {
		tspi->max_parent_rate = tspi->parent_clk_list[0].fixed_clk_rate;
		for (i = 1; i < tspi->parent_clk_count; ++i) {
			tspi->max_parent_rate = max(tspi->max_parent_rate,
				tspi->parent_clk_list[i].fixed_clk_rate);
		}
		if (tspi->max_rate)
			tspi->min_div = DIV_ROUND_UP(tspi->max_parent_rate,
						tspi->max_rate);
	}
	tspi->max_buf_size = SPI_FIFO_DEPTH << 2;

	if (!tspi->is_dma_allowed)
		goto skip_dma_alloc;

	init_completion(&tspi->tx_dma_complete);
	init_completion(&tspi->rx_dma_complete);

	ret = spi_tegra_init_dma_param(tspi, true);
	if (ret < 0) {
		dev_err(&pdev->dev, "Error in rx dma init\n");
		goto exit_free_master;
	}

	ret = spi_tegra_init_dma_param(tspi, false);
	if (ret < 0) {
		dev_err(&pdev->dev, "Error in tx dma init\n");
		goto exit_rx_dma_free;
	}

	tspi->max_buf_size = tspi->dma_buf_size;

skip_dma_alloc:
	tspi->def_command1_reg  = SPI_CS_SW_HW | SPI_M_S | SPI_CS_POL_INACTIVE_0 |
					SPI_CS_POL_INACTIVE_1 | SPI_CS_POL_INACTIVE_2 |
					SPI_CS_POL_INACTIVE_3 | SPI_CS_SS_VAL | SPI_LSBYTE_FE;
	tegra_spi_clk_enable(tspi);
	spi_tegra_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	tspi->def_command2_reg = spi_tegra_readl(tspi, SPI_COMMAND2);
	tegra_spi_clk_disable(tspi);

	pm_runtime_enable(&pdev->dev);

	/* Enable clock if it is require to be enable always */
	if (tspi->is_clkon_always)
		tegra_spi_clk_enable(tspi);

	/* create the workqueue for the spi transfer */
	snprintf(spi_wq_name, sizeof(spi_wq_name), "spi_tegra-%d", pdev->id);
	tspi->spi_workqueue = create_singlethread_workqueue(spi_wq_name);
	if (!tspi->spi_workqueue) {
		dev_err(&pdev->dev, "Failed to create work queue\n");
		ret = -ENODEV;
		goto exit_fail_wq;
	}

	INIT_WORK(&tspi->spi_transfer_work, tegra_spi_transfer_work);

	master->dev.of_node = pdev->dev.of_node;
	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not register to master err %d\n", ret);
		goto exit_destry_wq;
	}

	return ret;

exit_destry_wq:
	destroy_workqueue(tspi->spi_workqueue);

exit_fail_wq:
	if (tspi->is_clkon_always)
		tegra_spi_clk_disable(tspi);

	pm_runtime_disable(&pdev->dev);

	spi_tegra_deinit_dma_param(tspi, false);

exit_rx_dma_free:
	spi_tegra_deinit_dma_param(tspi, true);

exit_free_master:
	spi_master_put(master);
	return ret;
}

static int __devexit spi_tegra_remove(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct spi_tegra_data	*tspi;

	master = dev_get_drvdata(&pdev->dev);
	tspi = spi_master_get_devdata(master);

	spi_unregister_master(master);

	if (tspi->tx_dma)
		spi_tegra_deinit_dma_param(tspi, false);

	if (tspi->rx_dma)
		spi_tegra_deinit_dma_param(tspi, true);

	/* Disable clock if it is always enabled */
	if (tspi->is_clkon_always)
		tegra_spi_clk_disable(tspi);

	pm_runtime_disable(&pdev->dev);

	destroy_workqueue(tspi->spi_workqueue);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int spi_tegra_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct spi_tegra_data *tspi = spi_master_get_devdata(master);
	unsigned limit = 50;
	unsigned long flags;

	spin_lock_irqsave(&tspi->lock, flags);

	/* Wait for all transfer completes */
	if (!list_empty(&tspi->queue))
		dev_warn(dev, "The transfer list is not empty "
			"Waiting for time %d ms to complete transfer\n",
			limit * 20);

	while (!list_empty(&tspi->queue) && limit--) {
		spin_unlock_irqrestore(&tspi->lock, flags);
		msleep(20);
		spin_lock_irqsave(&tspi->lock, flags);
	}

	/* Wait for current transfer completes only */
	tspi->is_suspended = true;
	if (!list_empty(&tspi->queue)) {
		limit = 50;
		dev_err(dev, "All transfer has not completed, "
			"Waiting for %d ms current transfer to complete\n",
			limit * 20);
		while (tspi->is_transfer_in_progress && limit--) {
			spin_unlock_irqrestore(&tspi->lock, flags);
			msleep(20);
			spin_lock_irqsave(&tspi->lock, flags);
		}
	}

	if (tspi->is_transfer_in_progress) {
		dev_err(dev,
			"Spi transfer is in progress Avoiding suspend\n");
		tspi->is_suspended = false;
		spin_unlock_irqrestore(&tspi->lock, flags);
		return -EBUSY;
	}

	spin_unlock_irqrestore(&tspi->lock, flags);

	/* Disable clock if it is always enabled */
	if (tspi->is_clkon_always)
		tegra_spi_clk_disable(tspi);

	return 0;
}

static int spi_tegra_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct spi_tegra_data *tspi = spi_master_get_devdata(master);
	struct spi_message *m;
	struct spi_device *spi = NULL;
	struct spi_transfer *t = NULL;
	int single_xfer = 0;
	unsigned long flags;

	/* Enable clock if it is always enabled */
	if (tspi->is_clkon_always)
		tegra_spi_clk_enable(tspi);

	pm_runtime_get_sync(dev);
	tegra_spi_clk_enable(tspi);
	spi_tegra_writel(tspi, tspi->command1_reg, SPI_COMMAND1);
	spi_tegra_writel(tspi, tspi->def_command2_reg, SPI_COMMAND2);
	tegra_spi_clk_disable(tspi);
	pm_runtime_put_sync(dev);

	spin_lock_irqsave(&tspi->lock, flags);

	tspi->cur_speed = 0;
	tspi->is_suspended = false;
	if (!list_empty(&tspi->queue)) {
		m = list_first_entry(&tspi->queue, struct spi_message, queue);
		spi = m->state;
		single_xfer = list_is_singular(&m->transfers);
		m->actual_length = 0;
		m->status = 0;
		t = list_first_entry(&m->transfers, struct spi_transfer,
						transfer_list);
		tspi->is_transfer_in_progress = true;
	}
	spin_unlock_irqrestore(&tspi->lock, flags);
	if (t)
		spi_tegra_start_transfer(spi, t, true, single_xfer);
	return 0;
}
#endif

static const struct dev_pm_ops tegra_spi_dev_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = spi_tegra_suspend,
	.resume = spi_tegra_resume,
#endif
};

static struct platform_driver spi_tegra_driver = {
	.driver = {
		.name =		"tegra11-spi",
		.owner =	THIS_MODULE,
		.pm =		&tegra_spi_dev_pm_ops,
	},
	.probe =	spi_tegra_probe,
	.remove =	__devexit_p(spi_tegra_remove),
};

static int __init spi_tegra_init(void)
{
	return platform_driver_probe(&spi_tegra_driver, spi_tegra_probe);
}
subsys_initcall(spi_tegra_init);

static void __exit spi_tegra_exit(void)
{
	platform_driver_unregister(&spi_tegra_driver);
}
module_exit(spi_tegra_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:spi_tegra");
