/*
 * Driver for Nvidia TEGRA spi controller.
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *     Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2012, NVIDIA CORPORATION. All rights reserved.
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

/*#define DEBUG           1*/
/*#define VERBOSE_DEBUG   1*/

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

#define SLINK_COMMAND		0x000
#define   SLINK_BIT_LENGTH(x)		(((x) & 0x1f) << 0)
#define   SLINK_WORD_SIZE(x)		(((x) & 0x1f) << 5)
#define   SLINK_BOTH_EN			(1 << 10)
#define   SLINK_CS_SW			(1 << 11)
#define   SLINK_CS_VALUE		(1 << 12)
#define   SLINK_CS_POLARITY		(1 << 13)
#define   SLINK_IDLE_SDA_DRIVE_LOW	(0 << 16)
#define   SLINK_IDLE_SDA_DRIVE_HIGH	(1 << 16)
#define   SLINK_IDLE_SDA_PULL_LOW	(2 << 16)
#define   SLINK_IDLE_SDA_PULL_HIGH	(3 << 16)
#define   SLINK_IDLE_SDA_MASK		(3 << 16)
#define   SLINK_CS_POLARITY1		(1 << 20)
#define   SLINK_CK_SDA			(1 << 21)
#define   SLINK_CS_POLARITY2		(1 << 22)
#define   SLINK_CS_POLARITY3		(1 << 23)
#define   SLINK_IDLE_SCLK_DRIVE_LOW	(0 << 24)
#define   SLINK_IDLE_SCLK_DRIVE_HIGH	(1 << 24)
#define   SLINK_IDLE_SCLK_PULL_LOW	(2 << 24)
#define   SLINK_IDLE_SCLK_PULL_HIGH	(3 << 24)
#define   SLINK_IDLE_SCLK_MASK		(3 << 24)
#define   SLINK_M_S			(1 << 28)
#define   SLINK_WAIT			(1 << 29)
#define   SLINK_GO			(1 << 30)
#define   SLINK_ENB			(1 << 31)

#define SLINK_COMMAND2		0x004
#define   SLINK_LSBFE			(1 << 0)
#define   SLINK_SSOE			(1 << 1)
#define   SLINK_SPIE			(1 << 4)
#define   SLINK_BIDIROE			(1 << 6)
#define   SLINK_MODFEN			(1 << 7)
#define   SLINK_INT_SIZE(x)		(((x) & 0x1f) << 8)
#define   SLINK_CS_ACTIVE_BETWEEN	(1 << 17)
#define   SLINK_SS_EN_CS(x)		(((x) & 0x3) << 18)
#define   SLINK_SS_SETUP(x)		(((x) & 0x3) << 20)
#define   SLINK_FIFO_REFILLS_0		(0 << 22)
#define   SLINK_FIFO_REFILLS_1		(1 << 22)
#define   SLINK_FIFO_REFILLS_2		(2 << 22)
#define   SLINK_FIFO_REFILLS_3		(3 << 22)
#define   SLINK_FIFO_REFILLS_MASK	(3 << 22)
#define   SLINK_WAIT_PACK_INT(x)	(((x) & 0x7) << 26)
#define   SLINK_SPC0			(1 << 29)
#define   SLINK_TXEN			(1 << 30)
#define   SLINK_RXEN			(1 << 31)

#define SLINK_STATUS		0x008
#define   SLINK_COUNT(val)		(((val) >> 0) & 0x1f)
#define   SLINK_WORD(val)		(((val) >> 5) & 0x1f)
#define   SLINK_BLK_CNT(val)		(((val) >> 0) & 0xffff)
#define   SLINK_MODF			(1 << 16)
#define   SLINK_RX_UNF			(1 << 18)
#define   SLINK_TX_OVF			(1 << 19)
#define   SLINK_TX_FULL			(1 << 20)
#define   SLINK_TX_EMPTY		(1 << 21)
#define   SLINK_RX_FULL			(1 << 22)
#define   SLINK_RX_EMPTY		(1 << 23)
#define   SLINK_TX_UNF			(1 << 24)
#define   SLINK_RX_OVF			(1 << 25)
#define   SLINK_TX_FLUSH		(1 << 26)
#define   SLINK_RX_FLUSH		(1 << 27)
#define   SLINK_SCLK			(1 << 28)
#define   SLINK_ERR			(1 << 29)
#define   SLINK_RDY			(1 << 30)
#define   SLINK_BSY			(1 << 31)

#define SLINK_MAS_DATA		0x010
#define SLINK_SLAVE_DATA	0x014

#define SLINK_DMA_CTL		0x018
#define   SLINK_DMA_BLOCK_SIZE(x)	(((x) & 0xffff) << 0)
#define   SLINK_TX_TRIG_1		(0 << 16)
#define   SLINK_TX_TRIG_4		(1 << 16)
#define   SLINK_TX_TRIG_8		(2 << 16)
#define   SLINK_TX_TRIG_16		(3 << 16)
#define   SLINK_TX_TRIG_MASK		(3 << 16)
#define   SLINK_RX_TRIG_1		(0 << 18)
#define   SLINK_RX_TRIG_4		(1 << 18)
#define   SLINK_RX_TRIG_8		(2 << 18)
#define   SLINK_RX_TRIG_16		(3 << 18)
#define   SLINK_RX_TRIG_MASK		(3 << 18)
#define   SLINK_PACKED			(1 << 20)
#define   SLINK_PACK_SIZE_4		(0 << 21)
#define   SLINK_PACK_SIZE_8		(1 << 21)
#define   SLINK_PACK_SIZE_16		(2 << 21)
#define   SLINK_PACK_SIZE_32		(3 << 21)
#define   SLINK_PACK_SIZE_MASK		(3 << 21)
#define   SLINK_IE_TXC			(1 << 26)
#define   SLINK_IE_RXC			(1 << 27)
#define   SLINK_DMA_EN			(1 << 31)

#define SLINK_STATUS2		0x01c
#define   SLINK_TX_FIFO_EMPTY_COUNT(val)	(((val) & 0x3f) >> 0)
#define   SLINK_RX_FIFO_FULL_COUNT(val)		(((val) & 0x3f0000) >> 16)
#define   SLINK_SS_HOLD_TIME(val)		(((val) & 0xF) << 6)

#define SLINK_TX_FIFO		0x100
#define SLINK_RX_FIFO		0x180

#define DATA_DIR_TX		(1 << 0)
#define DATA_DIR_RX		(1 << 1)

#define SLINK_DMA_TIMEOUT (msecs_to_jiffies(1000))


static const unsigned long spi_tegra_req_sels[] = {
	TEGRA_DMA_REQ_SEL_SL2B1,
	TEGRA_DMA_REQ_SEL_SL2B2,
	TEGRA_DMA_REQ_SEL_SL2B3,
	TEGRA_DMA_REQ_SEL_SL2B4,
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	TEGRA_DMA_REQ_SEL_SL2B5,
	TEGRA_DMA_REQ_SEL_SL2B6,
#endif

};

#define DEFAULT_SPI_DMA_BUF_LEN		(16*1024)
#define TX_FIFO_EMPTY_COUNT_MAX		SLINK_TX_FIFO_EMPTY_COUNT(0x20)
#define RX_FIFO_FULL_COUNT_ZERO		SLINK_RX_FIFO_FULL_COUNT(0)

#define SLINK_STATUS2_RESET \
	(TX_FIFO_EMPTY_COUNT_MAX | \
	RX_FIFO_FULL_COUNT_ZERO << 16)

#define MAX_CHIP_SELECT		4
#define SLINK_FIFO_DEPTH	32

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
	unsigned long		packed_size;

	u32			command_reg;
	u32			command2_reg;
	u32			dma_control_reg;
	u32			def_command_reg;
	u32			def_command2_reg;

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
	readl(tspi->base + SLINK_MAS_DATA);
	spin_unlock_irqrestore(&tspi->reg_lock, flags);
}

static int tegra_spi_clk_disable(struct spi_tegra_data *tspi)
{
	unsigned long flags;

	/* Flush all write which are in PPSB queue by reading back */
	spi_tegra_readl(tspi, SLINK_MAS_DATA);

	spin_lock_irqsave(&tspi->reg_lock, flags);
	tspi->clk_state--;
	spin_unlock_irqrestore(&tspi->reg_lock, flags);
	clk_disable_unprepare(tspi->clk);
	clk_disable_unprepare(tspi->sclk);
	return 0;
}

static int tegra_spi_clk_enable(struct spi_tegra_data *tspi)
{
	unsigned long flags;

	clk_prepare_enable(tspi->sclk);
	clk_prepare_enable(tspi->clk);
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
	unsigned long val_write = 0;

	val = spi_tegra_readl(tspi, SLINK_STATUS);

	val_write = SLINK_RDY;
	if (val & SLINK_TX_OVF)
		val_write |= SLINK_TX_OVF;
	if (val & SLINK_RX_OVF)
		val_write |= SLINK_RX_OVF;
	if (val & SLINK_RX_UNF)
		val_write |= SLINK_RX_UNF;
	if (val & SLINK_TX_UNF)
		val_write |= SLINK_TX_UNF;

	spi_tegra_writel(tspi, val_write, SLINK_STATUS);
}

static unsigned long spi_tegra_get_packed_size(struct spi_tegra_data *tspi,
				  struct spi_transfer *t)
{
	unsigned long val;

	switch (tspi->bytes_per_word) {
	case 0:
		val = SLINK_PACK_SIZE_4;
		break;
	case 1:
		val = SLINK_PACK_SIZE_8;
		break;
	case 2:
		val = SLINK_PACK_SIZE_16;
		break;
	case 4:
		val = SLINK_PACK_SIZE_32;
		break;
	default:
		val = 0;
	}
	return val;
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
	tspi->packed_size = spi_tegra_get_packed_size(tspi, t);

	if (tspi->is_packed) {
		max_len = min(remain_len, tspi->max_buf_size);
		tspi->curr_dma_words = max_len/tspi->bytes_per_word;
		total_fifo_words = (max_len + 3)/4;
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

	fifo_status = spi_tegra_readl(tspi, SLINK_STATUS2);
	tx_empty_count = SLINK_TX_FIFO_EMPTY_COUNT(fifo_status);

	if (tspi->is_packed) {
		nbytes = tspi->curr_dma_words * tspi->bytes_per_word;
		max_n_32bit = (min(nbytes,  tx_empty_count*4) - 1)/4 + 1;
		for (count = 0; count < max_n_32bit; ++count) {
			x = 0;
			for (i = 0; (i < 4) && nbytes; i++, nbytes--)
				x |= (*tx_buf++) << (i*8);
			spi_tegra_writel(tspi, x, SLINK_TX_FIFO);
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
			spi_tegra_writel(tspi, x, SLINK_TX_FIFO);
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

	fifo_status = spi_tegra_readl(tspi, SLINK_STATUS2);
	rx_full_count = SLINK_RX_FIFO_FULL_COUNT(fifo_status);
	dev_dbg(&tspi->pdev->dev, "Rx fifo count %d\n", rx_full_count);
	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		for (count = 0; count < rx_full_count; ++count) {
			x = spi_tegra_readl(tspi, SLINK_RX_FIFO);
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
			x = spi_tegra_readl(tspi, SLINK_RX_FIFO);
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
	test_val = spi_tegra_readl(tspi, SLINK_STATUS);
	if (((test_val >> 20) & 0xF) != 0xA)
		dev_err(&tspi->pdev->dev,
			"The Rx and Tx fifo are not empty status 0x%08lx\n",
				test_val);

	val = SLINK_DMA_BLOCK_SIZE(tspi->curr_dma_words - 1);
	val |= tspi->packed_size;
	if (tspi->is_packed)
		len = DIV_ROUND_UP(tspi->curr_dma_words * tspi->bytes_per_word,
					4) * 4;
	else
		len = tspi->curr_dma_words * 4;

	if (len & 0xF)
		val |= SLINK_TX_TRIG_1 | SLINK_RX_TRIG_1;
	else if (((len) >> 4) & 0x1)
		val |= SLINK_TX_TRIG_4 | SLINK_RX_TRIG_4;
	else
		val |= SLINK_TX_TRIG_8 | SLINK_RX_TRIG_8;

	if (tspi->cur_direction & DATA_DIR_TX)
		val |= SLINK_IE_TXC;

	if (tspi->cur_direction & DATA_DIR_RX)
		val |= SLINK_IE_RXC;

	spi_tegra_writel(tspi, val, SLINK_DMA_CTL);
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

		/* Wait for tx fifo to be fill before starting slink */
		test_val = spi_tegra_readl(tspi, SLINK_STATUS);
		while (!(test_val & SLINK_TX_FULL))
			test_val = spi_tegra_readl(tspi, SLINK_STATUS);
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
	if (tspi->is_packed) {
		val |= SLINK_PACKED;
		spi_tegra_writel(tspi, val, SLINK_DMA_CTL);
		udelay(1);
		wmb();
	}
	tspi->dma_control_reg = val;

	val |= SLINK_DMA_EN;
	spi_tegra_writel(tspi, val, SLINK_DMA_CTL);
	return ret;
}

static int spi_tegra_start_cpu_based_transfer(
		struct spi_tegra_data *tspi, struct spi_transfer *t)
{
	unsigned long val;
	unsigned curr_words;

	val = tspi->packed_size;
	if (tspi->cur_direction & DATA_DIR_TX)
		val |= SLINK_IE_TXC;

	if (tspi->cur_direction & DATA_DIR_RX)
		val |= SLINK_IE_RXC;

	spi_tegra_writel(tspi, val, SLINK_DMA_CTL);
	tspi->dma_control_reg = val;

	if (tspi->cur_direction & DATA_DIR_TX)
		curr_words = spi_tegra_fill_tx_fifo_from_client_txbuf(tspi, t);
	else
		curr_words = tspi->curr_dma_words;
	val |= SLINK_DMA_BLOCK_SIZE(curr_words - 1);
	spi_tegra_writel(tspi, val, SLINK_DMA_CTL);
	tspi->dma_control_reg = val;

	tspi->is_curr_dma_xfer = false;
	if (tspi->is_packed) {
		val |= SLINK_PACKED;
		spi_tegra_writel(tspi, val, SLINK_DMA_CTL);
		udelay(1);
		wmb();
	}
	tspi->dma_control_reg = val;
	val |= SLINK_DMA_EN;
	spi_tegra_writel(tspi, val, SLINK_DMA_CTL);
	return 0;
}

static void set_best_clk_source(struct spi_tegra_data *tspi,
		unsigned long speed)
{
	long new_rate;
	unsigned long err_rate;
	int rate = speed * 4;
	unsigned int fin_err = speed * 4;
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
		dev_info(&tspi->pdev->dev, "Setting clk_src %s\n",
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
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	unsigned long command;
	unsigned long command2;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	unsigned long status2;
#endif
	int cs_setup_count;
	int cs_hold_count;

	unsigned int cs_pol_bit[] = {
			SLINK_CS_POLARITY,
			SLINK_CS_POLARITY1,
			SLINK_CS_POLARITY2,
			SLINK_CS_POLARITY3,
	};

	bits_per_word = t->bits_per_word ? t->bits_per_word :
					spi->bits_per_word;

	speed = t->speed_hz ? t->speed_hz : spi->max_speed_hz;
	if (speed != tspi->cur_speed) {
		set_best_clk_source(tspi, speed);
		clk_set_rate(tspi->clk, speed * 4);
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

	command2 = tspi->def_command2_reg;
	if (is_first_of_msg) {
		pm_runtime_get_sync(&tspi->pdev->dev);
		tegra_spi_clk_enable(tspi);

		spi_tegra_clear_status(tspi);

		command = tspi->def_command_reg;
		command |= SLINK_BIT_LENGTH(bits_per_word - 1);

		/* possibly use the hw based chip select */
		tspi->is_hw_based_cs = false;
		if (cdata && cdata->is_hw_based_cs && is_single_xfer) {
			if ((tspi->curr_dma_words * tspi->bytes_per_word) ==
						(t->len - tspi->cur_pos)) {
				cs_setup_count = cdata->cs_setup_clk_count >> 1;
				if (cs_setup_count > 3)
					cs_setup_count = 3;
				cs_hold_count = cdata->cs_hold_clk_count;
				if (cs_hold_count > 0xF)
					cs_hold_count = 0xF;
				tspi->is_hw_based_cs = true;

				command &= ~SLINK_CS_SW;
				command2 &= ~SLINK_SS_SETUP(3);
				command2 |= SLINK_SS_SETUP(cs_setup_count);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
				status2 = spi_tegra_readl(tspi, SLINK_STATUS2);
				status2 &= ~SLINK_SS_HOLD_TIME(0xF);
				status2 |= SLINK_SS_HOLD_TIME(cs_hold_count);
				spi_tegra_writel(tspi, status2, SLINK_STATUS2);
#endif
			}
		}
		if (!tspi->is_hw_based_cs) {
			command |= SLINK_CS_SW;
			command ^= cs_pol_bit[spi->chip_select];
		}

		command &= ~SLINK_IDLE_SCLK_MASK & ~SLINK_CK_SDA;
		if (spi->mode & SPI_CPHA)
			command |= SLINK_CK_SDA;

		if (spi->mode & SPI_CPOL)
			command |= SLINK_IDLE_SCLK_DRIVE_HIGH;
		else
			command |= SLINK_IDLE_SCLK_DRIVE_LOW;
	} else {
		command = tspi->command_reg;
		command &= ~SLINK_BIT_LENGTH(~0);
		command |= SLINK_BIT_LENGTH(bits_per_word - 1);
	}

	spi_tegra_writel(tspi, command, SLINK_COMMAND);
	tspi->command_reg = command;

	dev_dbg(&tspi->pdev->dev, "The def 0x%x and written 0x%lx\n",
				tspi->def_command_reg, command);

	command2 &= ~(SLINK_SS_EN_CS(~0) | SLINK_RXEN | SLINK_TXEN
		| SLINK_LSBFE);
	tspi->cur_direction = 0;
	if (t->rx_buf) {
		command2 |= SLINK_RXEN;
		tspi->cur_direction |= DATA_DIR_RX;
	}
	if (t->tx_buf) {
		command2 |= SLINK_TXEN;
		tspi->cur_direction |= DATA_DIR_TX;
	}
	command2 |= SLINK_SS_EN_CS(spi->chip_select);

	if (spi->mode & SPI_LSB_FIRST)
		command2 |= SLINK_LSBFE;

	spi_tegra_writel(tspi, command2, SLINK_COMMAND2);
	tspi->command2_reg = command2;

	if (total_fifo_words > SLINK_FIFO_DEPTH)
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

	dev_dbg(&spi->dev, "setup %d bpw, %scpol, %scpha, %dHz\n",
		spi->bits_per_word,
		spi->mode & SPI_CPOL ? "" : "~",
		spi->mode & SPI_CPHA ? "" : "~",
		spi->max_speed_hz);

	BUG_ON(spi->chip_select >= MAX_CHIP_SELECT);
	switch (spi->chip_select) {
	case 0:
		cs_bit = SLINK_CS_POLARITY;
		break;

	case 1:
		cs_bit = SLINK_CS_POLARITY1;
		break;

	case 2:
		cs_bit = SLINK_CS_POLARITY2;
		break;

	case 3:
		cs_bit = SLINK_CS_POLARITY3;
		break;

	default:
		return -EINVAL;
	}

	pm_runtime_get_sync(&tspi->pdev->dev);
	tegra_spi_clk_enable(tspi);

	spin_lock_irqsave(&tspi->lock, flags);
	val = tspi->def_command_reg;
	if (spi->mode & SPI_CS_HIGH)
		val |= cs_bit;
	else
		val &= ~cs_bit;
	tspi->def_command_reg = val;
	spi_tegra_writel(tspi, tspi->def_command_reg, SLINK_COMMAND);
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
				spi_tegra_writel(tspi, tspi->def_command_reg,
						SLINK_COMMAND);
				spi_tegra_writel(tspi, tspi->def_command2_reg,
						SLINK_COMMAND2);
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
			spi_tegra_writel(tspi, tspi->def_command_reg,
								SLINK_COMMAND);
			spi_tegra_writel(tspi, tspi->def_command2_reg,
								SLINK_COMMAND2);
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
	if (tspi->tx_status ||  tspi->rx_status ||
				(tspi->status_reg & SLINK_BSY)) {
		dev_err(&tspi->pdev->dev, "%s ERROR bit set 0x%x\n",
					 __func__, tspi->status_reg);
		dev_err(&tspi->pdev->dev, "%s 0x%08x:0x%08x:0x%08x\n",
				__func__, tspi->command_reg, tspi->command2_reg,
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
				&tspi->tx_dma_complete, SLINK_DMA_TIMEOUT);
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
				&tspi->rx_dma_complete, SLINK_DMA_TIMEOUT);
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
		dev_err(&tspi->pdev->dev, "%s 0x%08x:0x%08x:0x%08x\n",
				__func__, tspi->command_reg, tspi->command2_reg,
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
	if (total_fifo_words > SLINK_FIFO_DEPTH)
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

	tspi->status_reg = spi_tegra_readl(tspi, SLINK_STATUS);
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg &
					(SLINK_TX_OVF | SLINK_TX_UNF);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg &
					(SLINK_RX_OVF | SLINK_RX_UNF);
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
		dma_req->source_addr = tspi->phys + SLINK_RX_FIFO;
		dma_req->source_wrap = 4;
		tspi->rx_buf_phys = dma_phys;
		tspi->rx_buf = dma_buf;
		tspi->rx_dma = tdc;
	} else {
		dma_req->complete = tegra_spi_tx_dma_complete;
		dma_req->dest_addr = tspi->phys + SLINK_TX_FIFO;
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
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST;

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
		tspi->max_rate = 0;
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
	tspi->max_buf_size = SLINK_FIFO_DEPTH << 2;

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
	tspi->def_command_reg  = SLINK_CS_SW | SLINK_M_S;
	tspi->def_command2_reg = SLINK_CS_ACTIVE_BETWEEN;

skip_dma_alloc:
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

#ifdef CONFIG_PM
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
	spi_tegra_writel(tspi, tspi->command_reg, SLINK_COMMAND);
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
#ifdef CONFIG_PM
	.suspend = spi_tegra_suspend,
	.resume = spi_tegra_resume,
#endif
};

MODULE_ALIAS("platform:spi_tegra");

#ifdef CONFIG_OF
static struct of_device_id spi_tegra_of_match_table[] __devinitdata = {
	{ .compatible = "nvidia,tegra20-spi", },
	{}
};
MODULE_DEVICE_TABLE(of, spi_tegra_of_match_table);
#else /* CONFIG_OF */
#define spi_tegra_of_match_table NULL
#endif /* CONFIG_OF */

static struct platform_driver spi_tegra_driver = {
	.driver = {
		.name =		"spi_tegra",
		.owner =	THIS_MODULE,
		.pm =		&tegra_spi_dev_pm_ops,
		.of_match_table = spi_tegra_of_match_table,
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
