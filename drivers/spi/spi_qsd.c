/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
/*
 * SPI driver for Qualcomm MSM platforms
 *
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <mach/msm_spi.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <mach/dma.h>
#include <asm/atomic.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/remote_spinlock.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include "spi_qsd.h"

static inline int msm_spi_configure_gsbi(struct msm_spi *dd,
					struct platform_device *pdev)
{
	struct resource *resource;
	unsigned long   gsbi_mem_phys_addr;
	size_t          gsbi_mem_size;
	void __iomem    *gsbi_base;

	resource  = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!resource)
		return 0;

	gsbi_mem_phys_addr = resource->start;
	gsbi_mem_size = resource_size(resource);
	if (!devm_request_mem_region(&pdev->dev, gsbi_mem_phys_addr,
					gsbi_mem_size, SPI_DRV_NAME))
		return -ENXIO;

	gsbi_base = devm_ioremap(&pdev->dev, gsbi_mem_phys_addr,
					gsbi_mem_size);
	if (!gsbi_base)
		return -ENXIO;

	/* Set GSBI to SPI mode */
	writel_relaxed(GSBI_SPI_CONFIG, gsbi_base + GSBI_CTRL_REG);

	return 0;
}

static inline void msm_spi_register_init(struct msm_spi *dd)
{
	writel_relaxed(0x00000001, dd->base + SPI_SW_RESET);
	msm_spi_set_state(dd, SPI_OP_STATE_RESET);
	writel_relaxed(0x00000000, dd->base + SPI_OPERATIONAL);
	writel_relaxed(0x00000000, dd->base + SPI_CONFIG);
	writel_relaxed(0x00000000, dd->base + SPI_IO_MODES);
	if (dd->qup_ver)
		writel_relaxed(0x00000000, dd->base + QUP_OPERATIONAL_MASK);
}

static inline int msm_spi_request_gpios(struct msm_spi *dd)
{
	int i;
	int result = 0;

	for (i = 0; i < ARRAY_SIZE(spi_rsrcs); ++i) {
		if (dd->spi_gpios[i] >= 0) {
			result = gpio_request(dd->spi_gpios[i], spi_rsrcs[i]);
			if (result) {
				dev_err(dd->dev, "%s: gpio_request for pin %d "
					"failed with error %d\n", __func__,
					dd->spi_gpios[i], result);
				goto error;
			}
		}
	}
	return 0;

error:
	for (; --i >= 0;) {
		if (dd->spi_gpios[i] >= 0)
			gpio_free(dd->spi_gpios[i]);
	}
	return result;
}

static inline void msm_spi_free_gpios(struct msm_spi *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(spi_rsrcs); ++i) {
		if (dd->spi_gpios[i] >= 0)
			gpio_free(dd->spi_gpios[i]);
	}

	for (i = 0; i < ARRAY_SIZE(spi_cs_rsrcs); ++i) {
		if (dd->cs_gpios[i].valid) {
			gpio_free(dd->cs_gpios[i].gpio_num);
			dd->cs_gpios[i].valid = 0;
		}
	}
}

static void msm_spi_clock_set(struct msm_spi *dd, int speed)
{
	int rc;

	rc = clk_set_rate(dd->clk, speed);
	if (!rc)
		dd->clock_speed = speed;
}

static int msm_spi_calculate_size(int *fifo_size,
				  int *block_size,
				  int block,
				  int mult)
{
	int words;

	switch (block) {
	case 0:
		words = 1; /* 4 bytes */
		break;
	case 1:
		words = 4; /* 16 bytes */
		break;
	case 2:
		words = 8; /* 32 bytes */
		break;
	default:
		return -EINVAL;
	}

	switch (mult) {
	case 0:
		*fifo_size = words * 2;
		break;
	case 1:
		*fifo_size = words * 4;
		break;
	case 2:
		*fifo_size = words * 8;
		break;
	case 3:
		*fifo_size = words * 16;
		break;
	default:
		return -EINVAL;
	}

	*block_size = words * sizeof(u32); /* in bytes */
	return 0;
}

static void get_next_transfer(struct msm_spi *dd)
{
	struct spi_transfer *t = dd->cur_transfer;

	if (t->transfer_list.next != &dd->cur_msg->transfers) {
		dd->cur_transfer = list_entry(t->transfer_list.next,
					      struct spi_transfer,
					      transfer_list);
		dd->write_buf          = dd->cur_transfer->tx_buf;
		dd->read_buf           = dd->cur_transfer->rx_buf;
	}
}

static void __init msm_spi_calculate_fifo_size(struct msm_spi *dd)
{
	u32 spi_iom;
	int block;
	int mult;

	spi_iom = readl_relaxed(dd->base + SPI_IO_MODES);

	block = (spi_iom & SPI_IO_M_INPUT_BLOCK_SIZE) >> INPUT_BLOCK_SZ_SHIFT;
	mult = (spi_iom & SPI_IO_M_INPUT_FIFO_SIZE) >> INPUT_FIFO_SZ_SHIFT;
	if (msm_spi_calculate_size(&dd->input_fifo_size, &dd->input_block_size,
				   block, mult)) {
		goto fifo_size_err;
	}

	block = (spi_iom & SPI_IO_M_OUTPUT_BLOCK_SIZE) >> OUTPUT_BLOCK_SZ_SHIFT;
	mult = (spi_iom & SPI_IO_M_OUTPUT_FIFO_SIZE) >> OUTPUT_FIFO_SZ_SHIFT;
	if (msm_spi_calculate_size(&dd->output_fifo_size,
				   &dd->output_block_size, block, mult)) {
		goto fifo_size_err;
	}
	/* DM mode is not available for this block size */
	if (dd->input_block_size == 4 || dd->output_block_size == 4)
		dd->use_dma = 0;

	/* DM mode is currently unsupported for different block sizes */
	if (dd->input_block_size != dd->output_block_size)
		dd->use_dma = 0;

	if (dd->use_dma)
		dd->burst_size = max(dd->input_block_size, DM_BURST_SIZE);

	return;

fifo_size_err:
	dd->use_dma = 0;
	pr_err("%s: invalid FIFO size, SPI_IO_MODES=0x%x\n", __func__, spi_iom);
	return;
}

static void msm_spi_read_word_from_fifo(struct msm_spi *dd)
{
	u32   data_in;
	int   i;
	int   shift;

	data_in = readl_relaxed(dd->base + SPI_INPUT_FIFO);
	if (dd->read_buf) {
		for (i = 0; (i < dd->bytes_per_word) &&
			     dd->rx_bytes_remaining; i++) {
			/* The data format depends on bytes_per_word:
			   4 bytes: 0x12345678
			   3 bytes: 0x00123456
			   2 bytes: 0x00001234
			   1 byte : 0x00000012
			*/
			shift = 8 * (dd->bytes_per_word - i - 1);
			*dd->read_buf++ = (data_in & (0xFF << shift)) >> shift;
			dd->rx_bytes_remaining--;
		}
	} else {
		if (dd->rx_bytes_remaining >= dd->bytes_per_word)
			dd->rx_bytes_remaining -= dd->bytes_per_word;
		else
			dd->rx_bytes_remaining = 0;
	}

	dd->read_xfr_cnt++;
	if (dd->multi_xfr) {
		if (!dd->rx_bytes_remaining)
			dd->read_xfr_cnt = 0;
		else if ((dd->read_xfr_cnt * dd->bytes_per_word) ==
						dd->read_len) {
			struct spi_transfer *t = dd->cur_rx_transfer;
			if (t->transfer_list.next != &dd->cur_msg->transfers) {
				t = list_entry(t->transfer_list.next,
						struct spi_transfer,
						transfer_list);
				dd->read_buf = t->rx_buf;
				dd->read_len = t->len;
				dd->read_xfr_cnt = 0;
				dd->cur_rx_transfer = t;
			}
		}
	}
}

static inline bool msm_spi_is_valid_state(struct msm_spi *dd)
{
	u32 spi_op = readl_relaxed(dd->base + SPI_STATE);

	return spi_op & SPI_OP_STATE_VALID;
}

static inline int msm_spi_wait_valid(struct msm_spi *dd)
{
	unsigned long delay = 0;
	unsigned long timeout = 0;

	if (dd->clock_speed == 0)
		return -EINVAL;
	/*
	 * Based on the SPI clock speed, sufficient time
	 * should be given for the SPI state transition
	 * to occur
	 */
	delay = (10 * USEC_PER_SEC) / dd->clock_speed;
	/*
	 * For small delay values, the default timeout would
	 * be one jiffy
	 */
	if (delay < SPI_DELAY_THRESHOLD)
		delay = SPI_DELAY_THRESHOLD;

	/* Adding one to round off to the nearest jiffy */
	timeout = jiffies + msecs_to_jiffies(delay * SPI_DEFAULT_TIMEOUT) + 1;
	while (!msm_spi_is_valid_state(dd)) {
		if (time_after(jiffies, timeout)) {
			if (!msm_spi_is_valid_state(dd)) {
				if (dd->cur_msg)
					dd->cur_msg->status = -EIO;
				dev_err(dd->dev, "%s: SPI operational state"
					"not valid\n", __func__);
				return -ETIMEDOUT;
			} else
				return 0;
		}
		/*
		 * For smaller values of delay, context switch time
		 * would negate the usage of usleep
		 */
		if (delay > 20)
			usleep(delay);
		else if (delay)
			udelay(delay);
	}
	return 0;
}

static inline int msm_spi_set_state(struct msm_spi *dd,
				    enum msm_spi_state state)
{
	enum msm_spi_state cur_state;
	if (msm_spi_wait_valid(dd))
		return -EIO;
	cur_state = readl_relaxed(dd->base + SPI_STATE);
	/* Per spec:
	   For PAUSE_STATE to RESET_STATE, two writes of (10) are required */
	if (((cur_state & SPI_OP_STATE) == SPI_OP_STATE_PAUSE) &&
			(state == SPI_OP_STATE_RESET)) {
		writel_relaxed(SPI_OP_STATE_CLEAR_BITS, dd->base + SPI_STATE);
		writel_relaxed(SPI_OP_STATE_CLEAR_BITS, dd->base + SPI_STATE);
	} else {
		writel_relaxed((cur_state & ~SPI_OP_STATE) | state,
		       dd->base + SPI_STATE);
	}
	if (msm_spi_wait_valid(dd))
		return -EIO;

	return 0;
}

static inline void msm_spi_add_configs(struct msm_spi *dd, u32 *config, int n)
{
	*config &= ~(SPI_NO_INPUT|SPI_NO_OUTPUT);

	if (n != (*config & SPI_CFG_N))
		*config = (*config & ~SPI_CFG_N) | n;

	if ((dd->mode == SPI_DMOV_MODE) && (!dd->read_len)) {
		if (dd->read_buf == NULL)
			*config |= SPI_NO_INPUT;
		if (dd->write_buf == NULL)
			*config |= SPI_NO_OUTPUT;
	}
}

static void msm_spi_set_config(struct msm_spi *dd, int bpw)
{
	u32 spi_config;

	spi_config = readl_relaxed(dd->base + SPI_CONFIG);

	if (dd->cur_msg->spi->mode & SPI_CPHA)
		spi_config &= ~SPI_CFG_INPUT_FIRST;
	else
		spi_config |= SPI_CFG_INPUT_FIRST;
	if (dd->cur_msg->spi->mode & SPI_LOOP)
		spi_config |= SPI_CFG_LOOPBACK;
	else
		spi_config &= ~SPI_CFG_LOOPBACK;
	msm_spi_add_configs(dd, &spi_config, bpw-1);
	writel_relaxed(spi_config, dd->base + SPI_CONFIG);
	msm_spi_set_qup_config(dd, bpw);
}

static void msm_spi_setup_dm_transfer(struct msm_spi *dd)
{
	dmov_box *box;
	int bytes_to_send, num_rows, bytes_sent;
	u32 num_transfers;

	atomic_set(&dd->rx_irq_called, 0);
	atomic_set(&dd->tx_irq_called, 0);
	if (dd->write_len && !dd->read_len) {
		/* WR-WR transfer */
		bytes_sent = dd->cur_msg_len - dd->tx_bytes_remaining;
		dd->write_buf = dd->temp_buf;
	} else {
		bytes_sent = dd->cur_transfer->len - dd->tx_bytes_remaining;
		/* For WR-RD transfer, bytes_sent can be negative */
		if (bytes_sent < 0)
			bytes_sent = 0;
	}
	/* We'll send in chunks of SPI_MAX_LEN if larger than
	 * 4K bytes for targets that have only 12 bits in
	 * QUP_MAX_OUTPUT_CNT register. If the target supports
	 * more than 12bits then we send the data in chunks of
	 * the infinite_mode value that is defined in the
	 * corresponding board file.
	 */
	if (!dd->pdata->infinite_mode)
		dd->max_trfr_len = SPI_MAX_LEN;
	else
		dd->max_trfr_len = (dd->pdata->infinite_mode) *
			   (dd->bytes_per_word);

	bytes_to_send = min_t(u32, dd->tx_bytes_remaining,
			      dd->max_trfr_len);

	num_transfers = DIV_ROUND_UP(bytes_to_send, dd->bytes_per_word);
	dd->unaligned_len = bytes_to_send % dd->burst_size;
	num_rows = bytes_to_send / dd->burst_size;

	dd->mode = SPI_DMOV_MODE;

	if (num_rows) {
		/* src in 16 MSB, dst in 16 LSB */
		box = &dd->tx_dmov_cmd->box;
		box->src_row_addr = dd->cur_transfer->tx_dma + bytes_sent;
		box->src_dst_len = (dd->burst_size << 16) | dd->burst_size;
		box->num_rows = (num_rows << 16) | num_rows;
		box->row_offset = (dd->burst_size << 16) | 0;

		box = &dd->rx_dmov_cmd->box;
		box->dst_row_addr = dd->cur_transfer->rx_dma + bytes_sent;
		box->src_dst_len = (dd->burst_size << 16) | dd->burst_size;
		box->num_rows = (num_rows << 16) | num_rows;
		box->row_offset = (0 << 16) | dd->burst_size;

		dd->tx_dmov_cmd->cmd_ptr = CMD_PTR_LP |
				   DMOV_CMD_ADDR(dd->tx_dmov_cmd_dma +
				   offsetof(struct spi_dmov_cmd, box));
		dd->rx_dmov_cmd->cmd_ptr = CMD_PTR_LP |
				   DMOV_CMD_ADDR(dd->rx_dmov_cmd_dma +
				   offsetof(struct spi_dmov_cmd, box));
	} else {
		dd->tx_dmov_cmd->cmd_ptr = CMD_PTR_LP |
				   DMOV_CMD_ADDR(dd->tx_dmov_cmd_dma +
				   offsetof(struct spi_dmov_cmd, single_pad));
		dd->rx_dmov_cmd->cmd_ptr = CMD_PTR_LP |
				   DMOV_CMD_ADDR(dd->rx_dmov_cmd_dma +
				   offsetof(struct spi_dmov_cmd, single_pad));
	}

	if (!dd->unaligned_len) {
		dd->tx_dmov_cmd->box.cmd |= CMD_LC;
		dd->rx_dmov_cmd->box.cmd |= CMD_LC;
	} else {
		dmov_s *tx_cmd = &(dd->tx_dmov_cmd->single_pad);
		dmov_s *rx_cmd = &(dd->rx_dmov_cmd->single_pad);
		u32 offset = dd->cur_transfer->len - dd->unaligned_len;

		if ((dd->multi_xfr) && (dd->read_len <= 0))
			offset = dd->cur_msg_len - dd->unaligned_len;

		dd->tx_dmov_cmd->box.cmd &= ~CMD_LC;
		dd->rx_dmov_cmd->box.cmd &= ~CMD_LC;

		memset(dd->tx_padding, 0, dd->burst_size);
		memset(dd->rx_padding, 0, dd->burst_size);
		if (dd->write_buf)
			memcpy(dd->tx_padding, dd->write_buf + offset,
			       dd->unaligned_len);

		tx_cmd->src = dd->tx_padding_dma;
		rx_cmd->dst = dd->rx_padding_dma;
		tx_cmd->len = rx_cmd->len = dd->burst_size;
	}
	/* This also takes care of the padding dummy buf
	   Since this is set to the correct length, the
	   dummy bytes won't be actually sent */
	if (dd->multi_xfr) {
		u32 write_transfers = 0;
		u32 read_transfers = 0;

		if (dd->write_len > 0) {
			write_transfers = DIV_ROUND_UP(dd->write_len,
						       dd->bytes_per_word);
			writel_relaxed(write_transfers,
				       dd->base + SPI_MX_OUTPUT_COUNT);
		}
		if (dd->read_len > 0) {
			/*
			 *  The read following a write transfer must take
			 *  into account, that the bytes pertaining to
			 *  the write transfer needs to be discarded,
			 *  before the actual read begins.
			 */
			read_transfers = DIV_ROUND_UP(dd->read_len +
						      dd->write_len,
						      dd->bytes_per_word);
			writel_relaxed(read_transfers,
				       dd->base + SPI_MX_INPUT_COUNT);
		}
	} else {
		if (dd->write_buf)
			writel_relaxed(num_transfers,
				       dd->base + SPI_MX_OUTPUT_COUNT);
		if (dd->read_buf)
			writel_relaxed(num_transfers,
				       dd->base + SPI_MX_INPUT_COUNT);
	}
}

static void msm_spi_enqueue_dm_commands(struct msm_spi *dd)
{
	dma_coherent_pre_ops();
	if (dd->write_buf)
		msm_dmov_enqueue_cmd(dd->tx_dma_chan, &dd->tx_hdr);
	if (dd->read_buf)
		msm_dmov_enqueue_cmd(dd->rx_dma_chan, &dd->rx_hdr);
}

/* SPI core on targets that does not support infinite mode can send
   maximum of 4K transfers or 64K transfers depending up on size of
   MAX_OUTPUT_COUNT register, Therefore, we are sending in several
   chunks. Upon completion we send the next chunk, or complete the
   transfer if everything is finished. On targets that support
   infinite mode, we send all the bytes in as single chunk.
*/
static int msm_spi_dm_send_next(struct msm_spi *dd)
{
	/* By now we should have sent all the bytes in FIFO mode,
	 * However to make things right, we'll check anyway.
	 */
	if (dd->mode != SPI_DMOV_MODE)
		return 0;

	/* On targets which does not support infinite mode,
	   We need to send more chunks, if we sent max last time  */
	if (dd->tx_bytes_remaining > dd->max_trfr_len) {
		dd->tx_bytes_remaining -= dd->max_trfr_len;
		if (msm_spi_set_state(dd, SPI_OP_STATE_RESET))
			return 0;
		dd->read_len = dd->write_len = 0;
		msm_spi_setup_dm_transfer(dd);
		msm_spi_enqueue_dm_commands(dd);
		if (msm_spi_set_state(dd, SPI_OP_STATE_RUN))
			return 0;
		return 1;
	} else if (dd->read_len && dd->write_len) {
		dd->tx_bytes_remaining -= dd->cur_transfer->len;
		if (list_is_last(&dd->cur_transfer->transfer_list,
					    &dd->cur_msg->transfers))
			return 0;
		get_next_transfer(dd);
		if (msm_spi_set_state(dd, SPI_OP_STATE_PAUSE))
			return 0;
		dd->tx_bytes_remaining = dd->read_len + dd->write_len;
		dd->read_buf = dd->temp_buf;
		dd->read_len = dd->write_len = -1;
		msm_spi_setup_dm_transfer(dd);
		msm_spi_enqueue_dm_commands(dd);
		if (msm_spi_set_state(dd, SPI_OP_STATE_RUN))
			return 0;
		return 1;
	}
	return 0;
}

static inline void msm_spi_ack_transfer(struct msm_spi *dd)
{
	writel_relaxed(SPI_OP_MAX_INPUT_DONE_FLAG |
		       SPI_OP_MAX_OUTPUT_DONE_FLAG,
		       dd->base + SPI_OPERATIONAL);
	/* Ensure done flag was cleared before proceeding further */
	mb();
}

/* Figure which irq occured and call the relevant functions */
static inline irqreturn_t msm_spi_qup_irq(int irq, void *dev_id)
{
	u32 op, ret = IRQ_NONE;
	struct msm_spi *dd = dev_id;

	if (readl_relaxed(dd->base + SPI_ERROR_FLAGS) ||
	    readl_relaxed(dd->base + QUP_ERROR_FLAGS)) {
		struct spi_master *master = dev_get_drvdata(dd->dev);
		ret |= msm_spi_error_irq(irq, master);
	}

	op = readl_relaxed(dd->base + SPI_OPERATIONAL);
	if (op & SPI_OP_INPUT_SERVICE_FLAG) {
		writel_relaxed(SPI_OP_INPUT_SERVICE_FLAG,
			       dd->base + SPI_OPERATIONAL);
		/*
		 * Ensure service flag was cleared before further
		 * processing of interrupt.
		 */
		mb();
		ret |= msm_spi_input_irq(irq, dev_id);
	}

	if (op & SPI_OP_OUTPUT_SERVICE_FLAG) {
		writel_relaxed(SPI_OP_OUTPUT_SERVICE_FLAG,
			       dd->base + SPI_OPERATIONAL);
		/*
		 * Ensure service flag was cleared before further
		 * processing of interrupt.
		 */
		mb();
		ret |= msm_spi_output_irq(irq, dev_id);
	}

	if (dd->done) {
		complete(&dd->transfer_complete);
		dd->done = 0;
	}
	return ret;
}

static irqreturn_t msm_spi_input_irq(int irq, void *dev_id)
{
	struct msm_spi	       *dd = dev_id;

	dd->stat_rx++;

	if (dd->mode == SPI_MODE_NONE)
		return IRQ_HANDLED;

	if (dd->mode == SPI_DMOV_MODE) {
		u32 op = readl_relaxed(dd->base + SPI_OPERATIONAL);
		if ((!dd->read_buf || op & SPI_OP_MAX_INPUT_DONE_FLAG) &&
		    (!dd->write_buf || op & SPI_OP_MAX_OUTPUT_DONE_FLAG)) {
			msm_spi_ack_transfer(dd);
			if (dd->unaligned_len == 0) {
				if (atomic_inc_return(&dd->rx_irq_called) == 1)
					return IRQ_HANDLED;
			}
			msm_spi_complete(dd);
			return IRQ_HANDLED;
		}
		return IRQ_NONE;
	}

	if (dd->mode == SPI_FIFO_MODE) {
		while ((readl_relaxed(dd->base + SPI_OPERATIONAL) &
			SPI_OP_IP_FIFO_NOT_EMPTY) &&
			(dd->rx_bytes_remaining > 0)) {
			msm_spi_read_word_from_fifo(dd);
		}
		if (dd->rx_bytes_remaining == 0)
			msm_spi_complete(dd);
	}

	return IRQ_HANDLED;
}

static void msm_spi_write_word_to_fifo(struct msm_spi *dd)
{
	u32    word;
	u8     byte;
	int    i;

	word = 0;
	if (dd->write_buf) {
		for (i = 0; (i < dd->bytes_per_word) &&
			     dd->tx_bytes_remaining; i++) {
			dd->tx_bytes_remaining--;
			byte = *dd->write_buf++;
			word |= (byte << (BITS_PER_BYTE * (3 - i)));
		}
	} else
		if (dd->tx_bytes_remaining > dd->bytes_per_word)
			dd->tx_bytes_remaining -= dd->bytes_per_word;
		else
			dd->tx_bytes_remaining = 0;
	dd->write_xfr_cnt++;
	if (dd->multi_xfr) {
		if (!dd->tx_bytes_remaining)
			dd->write_xfr_cnt = 0;
		else if ((dd->write_xfr_cnt * dd->bytes_per_word) ==
						dd->write_len) {
			struct spi_transfer *t = dd->cur_tx_transfer;
			if (t->transfer_list.next != &dd->cur_msg->transfers) {
				t = list_entry(t->transfer_list.next,
						struct spi_transfer,
						transfer_list);
				dd->write_buf = t->tx_buf;
				dd->write_len = t->len;
				dd->write_xfr_cnt = 0;
				dd->cur_tx_transfer = t;
			}
		}
	}
	writel_relaxed(word, dd->base + SPI_OUTPUT_FIFO);
}

static inline void msm_spi_write_rmn_to_fifo(struct msm_spi *dd)
{
	int count = 0;

	while ((dd->tx_bytes_remaining > 0) && (count < dd->input_fifo_size) &&
	       !(readl_relaxed(dd->base + SPI_OPERATIONAL) &
		SPI_OP_OUTPUT_FIFO_FULL)) {
		msm_spi_write_word_to_fifo(dd);
		count++;
	}
}

static irqreturn_t msm_spi_output_irq(int irq, void *dev_id)
{
	struct msm_spi	       *dd = dev_id;

	dd->stat_tx++;

	if (dd->mode == SPI_MODE_NONE)
		return IRQ_HANDLED;

	if (dd->mode == SPI_DMOV_MODE) {
		/* TX_ONLY transaction is handled here
		   This is the only place we send complete at tx and not rx */
		if (dd->read_buf == NULL &&
		    readl_relaxed(dd->base + SPI_OPERATIONAL) &
		    SPI_OP_MAX_OUTPUT_DONE_FLAG) {
			msm_spi_ack_transfer(dd);
			if (atomic_inc_return(&dd->tx_irq_called) == 1)
				return IRQ_HANDLED;
			msm_spi_complete(dd);
			return IRQ_HANDLED;
		}
		return IRQ_NONE;
	}

	/* Output FIFO is empty. Transmit any outstanding write data. */
	if (dd->mode == SPI_FIFO_MODE)
		msm_spi_write_rmn_to_fifo(dd);

	return IRQ_HANDLED;
}

static irqreturn_t msm_spi_error_irq(int irq, void *dev_id)
{
	struct spi_master	*master = dev_id;
	struct msm_spi          *dd = spi_master_get_devdata(master);
	u32                      spi_err;

	spi_err = readl_relaxed(dd->base + SPI_ERROR_FLAGS);
	if (spi_err & SPI_ERR_OUTPUT_OVER_RUN_ERR)
		dev_warn(master->dev.parent, "SPI output overrun error\n");
	if (spi_err & SPI_ERR_INPUT_UNDER_RUN_ERR)
		dev_warn(master->dev.parent, "SPI input underrun error\n");
	if (spi_err & SPI_ERR_OUTPUT_UNDER_RUN_ERR)
		dev_warn(master->dev.parent, "SPI output underrun error\n");
	msm_spi_get_clk_err(dd, &spi_err);
	if (spi_err & SPI_ERR_CLK_OVER_RUN_ERR)
		dev_warn(master->dev.parent, "SPI clock overrun error\n");
	if (spi_err & SPI_ERR_CLK_UNDER_RUN_ERR)
		dev_warn(master->dev.parent, "SPI clock underrun error\n");
	msm_spi_clear_error_flags(dd);
	msm_spi_ack_clk_err(dd);
	/* Ensure clearing of QUP_ERROR_FLAGS was completed */
	mb();
	return IRQ_HANDLED;
}

static int msm_spi_map_dma_buffers(struct msm_spi *dd)
{
	struct device *dev;
	struct spi_transfer *first_xfr;
	struct spi_transfer *nxt_xfr = NULL;
	void *tx_buf, *rx_buf;
	unsigned tx_len, rx_len;
	int ret = -EINVAL;

	dev = &dd->cur_msg->spi->dev;
	first_xfr = dd->cur_transfer;
	tx_buf = (void *)first_xfr->tx_buf;
	rx_buf = first_xfr->rx_buf;
	tx_len = rx_len = first_xfr->len;

	/*
	 * For WR-WR and WR-RD transfers, we allocate our own temporary
	 * buffer and copy the data to/from the client buffers.
	 */
	if (dd->multi_xfr) {
		dd->temp_buf = kzalloc(dd->cur_msg_len,
				       GFP_KERNEL | __GFP_DMA);
		if (!dd->temp_buf)
			return -ENOMEM;
		nxt_xfr = list_entry(first_xfr->transfer_list.next,
				     struct spi_transfer, transfer_list);

		if (dd->write_len && !dd->read_len) {
			if (!first_xfr->tx_buf || !nxt_xfr->tx_buf)
				goto error;

			memcpy(dd->temp_buf, first_xfr->tx_buf, first_xfr->len);
			memcpy(dd->temp_buf + first_xfr->len, nxt_xfr->tx_buf,
			       nxt_xfr->len);
			tx_buf = dd->temp_buf;
			tx_len = dd->cur_msg_len;
		} else {
			if (!first_xfr->tx_buf || !nxt_xfr->rx_buf)
				goto error;

			rx_buf = dd->temp_buf;
			rx_len = dd->cur_msg_len;
		}
	}
	if (tx_buf != NULL) {
		first_xfr->tx_dma = dma_map_single(dev, tx_buf,
						   tx_len, DMA_TO_DEVICE);
		if (dma_mapping_error(NULL, first_xfr->tx_dma)) {
			dev_err(dev, "dma %cX %d bytes error\n",
				'T', tx_len);
			ret = -ENOMEM;
			goto error;
		}
	}
	if (rx_buf != NULL) {
		dma_addr_t dma_handle;
		dma_handle = dma_map_single(dev, rx_buf,
					    rx_len, DMA_FROM_DEVICE);
		if (dma_mapping_error(NULL, dma_handle)) {
			dev_err(dev, "dma %cX %d bytes error\n",
				'R', rx_len);
			if (tx_buf != NULL)
				dma_unmap_single(NULL, first_xfr->tx_dma,
						 tx_len, DMA_TO_DEVICE);
			ret = -ENOMEM;
			goto error;
		}
		if (dd->multi_xfr)
			nxt_xfr->rx_dma = dma_handle;
		else
			first_xfr->rx_dma = dma_handle;
	}
	return 0;

error:
	kfree(dd->temp_buf);
	dd->temp_buf = NULL;
	return ret;
}

static void msm_spi_unmap_dma_buffers(struct msm_spi *dd)
{
	struct device *dev;
	u32 offset;

	dev = &dd->cur_msg->spi->dev;
	if (dd->cur_msg->is_dma_mapped)
		goto unmap_end;

	if (dd->multi_xfr) {
		if (dd->write_len && !dd->read_len) {
			dma_unmap_single(dev,
					 dd->cur_transfer->tx_dma,
					 dd->cur_msg_len,
					 DMA_TO_DEVICE);
		} else {
			struct spi_transfer *prev_xfr;
			prev_xfr = list_entry(
				   dd->cur_transfer->transfer_list.prev,
				   struct spi_transfer,
				   transfer_list);
			if (dd->cur_transfer->rx_buf) {
				dma_unmap_single(dev,
						 dd->cur_transfer->rx_dma,
						 dd->cur_msg_len,
						 DMA_FROM_DEVICE);
			}
			if (prev_xfr->tx_buf) {
				dma_unmap_single(dev,
						 prev_xfr->tx_dma,
						 prev_xfr->len,
						 DMA_TO_DEVICE);
			}
			if (dd->unaligned_len && dd->read_buf) {
				offset = dd->cur_msg_len - dd->unaligned_len;
				dma_coherent_post_ops();
				memcpy(dd->read_buf + offset, dd->rx_padding,
				       dd->unaligned_len);
				memcpy(dd->cur_transfer->rx_buf,
				       dd->read_buf + prev_xfr->len,
				       dd->cur_transfer->len);
			}
		}
		kfree(dd->temp_buf);
		dd->temp_buf = NULL;
		return;
	} else {
		if (dd->cur_transfer->rx_buf)
			dma_unmap_single(dev, dd->cur_transfer->rx_dma,
					 dd->cur_transfer->len,
					 DMA_FROM_DEVICE);
		if (dd->cur_transfer->tx_buf)
			dma_unmap_single(dev, dd->cur_transfer->tx_dma,
					 dd->cur_transfer->len,
					 DMA_TO_DEVICE);
	}

unmap_end:
	/* If we padded the transfer, we copy it from the padding buf */
	if (dd->unaligned_len && dd->read_buf) {
		offset = dd->cur_transfer->len - dd->unaligned_len;
		dma_coherent_post_ops();
		memcpy(dd->read_buf + offset, dd->rx_padding,
		       dd->unaligned_len);
	}
}

/**
 * msm_use_dm - decides whether to use data mover for this
 * 		transfer
 * @dd:       device
 * @tr:       transfer
 *
 * Start using DM if:
 * 1. Transfer is longer than 3*block size.
 * 2. Buffers should be aligned to cache line.
 * 3. For WR-RD or WR-WR transfers, if condition (1) and (2) above are met.
  */
static inline int msm_use_dm(struct msm_spi *dd, struct spi_transfer *tr,
			     u8 bpw)
{
	u32 cache_line = dma_get_cache_alignment();

	if (!dd->use_dma)
		return 0;

	if (dd->cur_msg_len < 3*dd->input_block_size)
		return 0;

	if (dd->multi_xfr && !dd->read_len && !dd->write_len)
		return 0;

	if (tr->tx_buf) {
		if (!IS_ALIGNED((size_t)tr->tx_buf, cache_line))
			return 0;
	}
	if (tr->rx_buf) {
		if (!IS_ALIGNED((size_t)tr->rx_buf, cache_line))
			return 0;
	}

	if (tr->cs_change &&
	   ((bpw != 8) || (bpw != 16) || (bpw != 32)))
		return 0;
	return 1;
}

static void msm_spi_process_transfer(struct msm_spi *dd)
{
	u8  bpw;
	u32 spi_ioc;
	u32 spi_iom;
	u32 spi_ioc_orig;
	u32 max_speed;
	u32 chip_select;
	u32 read_count;
	u32 timeout;
	u32 int_loopback = 0;

	dd->tx_bytes_remaining = dd->cur_msg_len;
	dd->rx_bytes_remaining = dd->cur_msg_len;
	dd->read_buf           = dd->cur_transfer->rx_buf;
	dd->write_buf          = dd->cur_transfer->tx_buf;
	init_completion(&dd->transfer_complete);
	if (dd->cur_transfer->bits_per_word)
		bpw = dd->cur_transfer->bits_per_word;
	else
		if (dd->cur_msg->spi->bits_per_word)
			bpw = dd->cur_msg->spi->bits_per_word;
		else
			bpw = 8;
	dd->bytes_per_word = (bpw + 7) / 8;

	if (dd->cur_transfer->speed_hz)
		max_speed = dd->cur_transfer->speed_hz;
	else
		max_speed = dd->cur_msg->spi->max_speed_hz;
	if (!dd->clock_speed || max_speed != dd->clock_speed)
		msm_spi_clock_set(dd, max_speed);

	read_count = DIV_ROUND_UP(dd->cur_msg_len, dd->bytes_per_word);
	if (dd->cur_msg->spi->mode & SPI_LOOP)
		int_loopback = 1;
	if (int_loopback && dd->multi_xfr &&
			(read_count > dd->input_fifo_size)) {
		if (dd->read_len && dd->write_len)
			pr_err(
			"%s:Internal Loopback does not support > fifo size"
			"for write-then-read transactions\n",
			__func__);
		else if (dd->write_len && !dd->read_len)
			pr_err(
			"%s:Internal Loopback does not support > fifo size"
			"for write-then-write transactions\n",
			__func__);
		return;
	}
	if (!msm_use_dm(dd, dd->cur_transfer, bpw)) {
		dd->mode = SPI_FIFO_MODE;
		if (dd->multi_xfr) {
			dd->read_len = dd->cur_transfer->len;
			dd->write_len = dd->cur_transfer->len;
		}
		/* read_count cannot exceed fifo_size, and only one READ COUNT
		   interrupt is generated per transaction, so for transactions
		   larger than fifo size READ COUNT must be disabled.
		   For those transactions we usually move to Data Mover mode.
		*/
		if (read_count <= dd->input_fifo_size) {
			writel_relaxed(read_count,
				       dd->base + SPI_MX_READ_COUNT);
			msm_spi_set_write_count(dd, read_count);
		} else {
			writel_relaxed(0, dd->base + SPI_MX_READ_COUNT);
			msm_spi_set_write_count(dd, 0);
		}
	} else {
		dd->mode = SPI_DMOV_MODE;
		if (dd->write_len && dd->read_len) {
			dd->tx_bytes_remaining = dd->write_len;
			dd->rx_bytes_remaining = dd->read_len;
		}
	}

	/* Write mode - fifo or data mover*/
	spi_iom = readl_relaxed(dd->base + SPI_IO_MODES);
	spi_iom &= ~(SPI_IO_M_INPUT_MODE | SPI_IO_M_OUTPUT_MODE);
	spi_iom = (spi_iom | (dd->mode << OUTPUT_MODE_SHIFT));
	spi_iom = (spi_iom | (dd->mode << INPUT_MODE_SHIFT));
	/* Turn on packing for data mover */
	if (dd->mode == SPI_DMOV_MODE)
		spi_iom |= SPI_IO_M_PACK_EN | SPI_IO_M_UNPACK_EN;
	else
		spi_iom &= ~(SPI_IO_M_PACK_EN | SPI_IO_M_UNPACK_EN);
	writel_relaxed(spi_iom, dd->base + SPI_IO_MODES);

	msm_spi_set_config(dd, bpw);

	spi_ioc = readl_relaxed(dd->base + SPI_IO_CONTROL);
	spi_ioc_orig = spi_ioc;
	if (dd->cur_msg->spi->mode & SPI_CPOL)
		spi_ioc |= SPI_IO_C_CLK_IDLE_HIGH;
	else
		spi_ioc &= ~SPI_IO_C_CLK_IDLE_HIGH;
	chip_select = dd->cur_msg->spi->chip_select << 2;
	if ((spi_ioc & SPI_IO_C_CS_SELECT) != chip_select)
		spi_ioc = (spi_ioc & ~SPI_IO_C_CS_SELECT) | chip_select;
	if (!dd->cur_transfer->cs_change)
		spi_ioc |= SPI_IO_C_MX_CS_MODE;
	if (spi_ioc != spi_ioc_orig)
		writel_relaxed(spi_ioc, dd->base + SPI_IO_CONTROL);

	if (dd->mode == SPI_DMOV_MODE) {
		msm_spi_setup_dm_transfer(dd);
		msm_spi_enqueue_dm_commands(dd);
	}
	/* The output fifo interrupt handler will handle all writes after
	   the first. Restricting this to one write avoids contention
	   issues and race conditions between this thread and the int handler
	*/
	else if (dd->mode == SPI_FIFO_MODE) {
		if (msm_spi_prepare_for_write(dd))
			goto transfer_end;
		msm_spi_start_write(dd, read_count);
	}

	/* Only enter the RUN state after the first word is written into
	   the output FIFO. Otherwise, the output FIFO EMPTY interrupt
	   might fire before the first word is written resulting in a
	   possible race condition.
	 */
	if (msm_spi_set_state(dd, SPI_OP_STATE_RUN))
		goto transfer_end;

	timeout = 100 * msecs_to_jiffies(
	      DIV_ROUND_UP(dd->cur_msg_len * 8,
		 DIV_ROUND_UP(max_speed, MSEC_PER_SEC)));

	/* Assume success, this might change later upon transaction result */
	dd->cur_msg->status = 0;
	do {
		if (!wait_for_completion_timeout(&dd->transfer_complete,
						 timeout)) {
				dev_err(dd->dev, "%s: SPI transaction "
						 "timeout\n", __func__);
				dd->cur_msg->status = -EIO;
				if (dd->mode == SPI_DMOV_MODE) {
					msm_dmov_flush(dd->tx_dma_chan, 1);
					msm_dmov_flush(dd->rx_dma_chan, 1);
				}
				break;
		}
	} while (msm_spi_dm_send_next(dd));

transfer_end:
	if (dd->mode == SPI_DMOV_MODE)
		msm_spi_unmap_dma_buffers(dd);
	dd->mode = SPI_MODE_NONE;

	msm_spi_set_state(dd, SPI_OP_STATE_RESET);
	writel_relaxed(spi_ioc & ~SPI_IO_C_MX_CS_MODE,
		       dd->base + SPI_IO_CONTROL);
}

static void get_transfer_length(struct msm_spi *dd)
{
	struct spi_transfer *tr;
	int num_xfrs = 0;
	int readlen = 0;
	int writelen = 0;

	dd->cur_msg_len = 0;
	dd->multi_xfr = 0;
	dd->read_len = dd->write_len = 0;

	list_for_each_entry(tr, &dd->cur_msg->transfers, transfer_list) {
		if (tr->tx_buf)
			writelen += tr->len;
		if (tr->rx_buf)
			readlen += tr->len;
		dd->cur_msg_len += tr->len;
		num_xfrs++;
	}

	if (num_xfrs == 2) {
		struct spi_transfer *first_xfr = dd->cur_transfer;

		dd->multi_xfr = 1;
		tr = list_entry(first_xfr->transfer_list.next,
				struct spi_transfer,
				transfer_list);
		/*
		 * We update dd->read_len and dd->write_len only
		 * for WR-WR and WR-RD transfers.
		 */
		if ((first_xfr->tx_buf) && (!first_xfr->rx_buf)) {
			if (((tr->tx_buf) && (!tr->rx_buf)) ||
			    ((!tr->tx_buf) && (tr->rx_buf))) {
				dd->read_len = readlen;
				dd->write_len = writelen;
			}
		}
	} else if (num_xfrs > 1)
		dd->multi_xfr = 1;
}

static inline int combine_transfers(struct msm_spi *dd)
{
	struct spi_transfer *t = dd->cur_transfer;
	struct spi_transfer *nxt;
	int xfrs_grped = 1;

	dd->cur_msg_len = dd->cur_transfer->len;
	while (t->transfer_list.next != &dd->cur_msg->transfers) {
		nxt = list_entry(t->transfer_list.next,
				 struct spi_transfer,
				 transfer_list);
		if (t->cs_change != nxt->cs_change)
			return xfrs_grped;
		dd->cur_msg_len += nxt->len;
		xfrs_grped++;
		t = nxt;
	}
	return xfrs_grped;
}

static inline void write_force_cs(struct msm_spi *dd, bool set_flag)
{
	u32 spi_ioc;
	u32 spi_ioc_orig;

	spi_ioc = readl_relaxed(dd->base + SPI_IO_CONTROL);
	spi_ioc_orig = spi_ioc;
	if (set_flag)
		spi_ioc |= SPI_IO_C_FORCE_CS;
	else
		spi_ioc &= ~SPI_IO_C_FORCE_CS;

	if (spi_ioc != spi_ioc_orig)
		writel_relaxed(spi_ioc, dd->base + SPI_IO_CONTROL);
}

static void msm_spi_process_message(struct msm_spi *dd)
{
	int xfrs_grped = 0;
	int cs_num;
	int rc;

	dd->write_xfr_cnt = dd->read_xfr_cnt = 0;
	cs_num = dd->cur_msg->spi->chip_select;
	if ((!(dd->cur_msg->spi->mode & SPI_LOOP)) &&
		(!(dd->cs_gpios[cs_num].valid)) &&
		(dd->cs_gpios[cs_num].gpio_num >= 0)) {
		rc = gpio_request(dd->cs_gpios[cs_num].gpio_num,
				spi_cs_rsrcs[cs_num]);
		if (rc) {
			dev_err(dd->dev, "gpio_request for pin %d failed with "
				"error %d\n", dd->cs_gpios[cs_num].gpio_num,
				rc);
			return;
		}
		dd->cs_gpios[cs_num].valid = 1;
	}

	if (dd->qup_ver) {
		write_force_cs(dd, 0);
		list_for_each_entry(dd->cur_transfer,
				&dd->cur_msg->transfers,
				transfer_list) {
			struct spi_transfer *t = dd->cur_transfer;
			struct spi_transfer *nxt;

			if (t->transfer_list.next != &dd->cur_msg->transfers) {
				nxt = list_entry(t->transfer_list.next,
						struct spi_transfer,
						transfer_list);

				if (t->cs_change == nxt->cs_change)
					write_force_cs(dd, 1);
				else
					write_force_cs(dd, 0);
			}

			dd->cur_msg_len = dd->cur_transfer->len;
			msm_spi_process_transfer(dd);
		}
	} else {
		dd->cur_transfer = list_first_entry(&dd->cur_msg->transfers,
						    struct spi_transfer,
						    transfer_list);
		get_transfer_length(dd);
		if (dd->multi_xfr && !dd->read_len && !dd->write_len) {
			/*
			 * Handling of multi-transfers.
			 * FIFO mode is used by default
			 */
			list_for_each_entry(dd->cur_transfer,
					    &dd->cur_msg->transfers,
					    transfer_list) {
				if (!dd->cur_transfer->len)
					goto error;
				if (xfrs_grped) {
					xfrs_grped--;
					continue;
				} else {
					dd->read_len = dd->write_len = 0;
					xfrs_grped = combine_transfers(dd);
				}

				dd->cur_tx_transfer = dd->cur_transfer;
				dd->cur_rx_transfer = dd->cur_transfer;
				msm_spi_process_transfer(dd);
				xfrs_grped--;
			}
		} else {
			/* Handling of a single transfer or
			 * WR-WR or WR-RD transfers
			 */
			if ((!dd->cur_msg->is_dma_mapped) &&
			    (msm_use_dm(dd, dd->cur_transfer,
					dd->cur_transfer->bits_per_word))) {
				/* Mapping of DMA buffers */
				int ret = msm_spi_map_dma_buffers(dd);
				if (ret < 0) {
					dd->cur_msg->status = ret;
					goto error;
				}
			}

			dd->cur_tx_transfer = dd->cur_transfer;
			dd->cur_rx_transfer = dd->cur_transfer;
			msm_spi_process_transfer(dd);
		}
	}

	return;

error:
	if (dd->cs_gpios[cs_num].valid) {
		gpio_free(dd->cs_gpios[cs_num].gpio_num);
		dd->cs_gpios[cs_num].valid = 0;
	}
}

/* workqueue - pull messages from queue & process */
static void msm_spi_workq(struct work_struct *work)
{
	struct msm_spi      *dd =
		container_of(work, struct msm_spi, work_data);
	unsigned long        flags;
	u32                  status_error = 0;
	int                  rc = 0;

	mutex_lock(&dd->core_lock);

	/* Don't allow power collapse until we release mutex */
	if (pm_qos_request_active(&qos_req_list))
		pm_qos_update_request(&qos_req_list,
				  dd->pm_lat);
	if (dd->use_rlock)
		remote_mutex_lock(&dd->r_lock);

	/* Configure the spi clk, miso, mosi and cs gpio */
	if (dd->pdata->gpio_config) {
		rc = dd->pdata->gpio_config();
		if (rc) {
			dev_err(dd->dev,
					"%s: error configuring GPIOs\n",
					__func__);
			status_error = 1;
		}
	}

	rc = msm_spi_request_gpios(dd);
	if (rc)
		status_error = 1;

	clk_prepare_enable(dd->clk);
	clk_prepare_enable(dd->pclk);
	msm_spi_enable_irqs(dd);

	if (!msm_spi_is_valid_state(dd)) {
		dev_err(dd->dev, "%s: SPI operational state not valid\n",
			__func__);
		status_error = 1;
	}

	spin_lock_irqsave(&dd->queue_lock, flags);
	while (!list_empty(&dd->queue)) {
		dd->cur_msg = list_entry(dd->queue.next,
					 struct spi_message, queue);
		list_del_init(&dd->cur_msg->queue);
		spin_unlock_irqrestore(&dd->queue_lock, flags);
		if (status_error)
			dd->cur_msg->status = -EIO;
		else
			msm_spi_process_message(dd);
		if (dd->cur_msg->complete)
			dd->cur_msg->complete(dd->cur_msg->context);
		spin_lock_irqsave(&dd->queue_lock, flags);
	}
	dd->transfer_pending = 0;
	spin_unlock_irqrestore(&dd->queue_lock, flags);

	msm_spi_disable_irqs(dd);
	clk_disable_unprepare(dd->clk);
	clk_disable_unprepare(dd->pclk);

	/* Free  the spi clk, miso, mosi, cs gpio */
	if (!rc && dd->pdata && dd->pdata->gpio_release)
		dd->pdata->gpio_release();
	if (!rc)
		msm_spi_free_gpios(dd);

	if (dd->use_rlock)
		remote_mutex_unlock(&dd->r_lock);

	if (pm_qos_request_active(&qos_req_list))
		pm_qos_update_request(&qos_req_list,
				  PM_QOS_DEFAULT_VALUE);

	mutex_unlock(&dd->core_lock);
	/* If needed, this can be done after the current message is complete,
	   and work can be continued upon resume. No motivation for now. */
	if (dd->suspended)
		wake_up_interruptible(&dd->continue_suspend);
}

static int msm_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct msm_spi	*dd;
	unsigned long    flags;
	struct spi_transfer *tr;

	dd = spi_master_get_devdata(spi->master);
	if (dd->suspended)
		return -EBUSY;

	if (list_empty(&msg->transfers) || !msg->complete)
		return -EINVAL;

	list_for_each_entry(tr, &msg->transfers, transfer_list) {
		/* Check message parameters */
		if (tr->speed_hz > dd->pdata->max_clock_speed ||
		    (tr->bits_per_word &&
		     (tr->bits_per_word < 4 || tr->bits_per_word > 32)) ||
		    (tr->tx_buf == NULL && tr->rx_buf == NULL)) {
			dev_err(&spi->dev, "Invalid transfer: %d Hz, %d bpw"
					   "tx=%p, rx=%p\n",
					    tr->speed_hz, tr->bits_per_word,
					    tr->tx_buf, tr->rx_buf);
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&dd->queue_lock, flags);
	if (dd->suspended) {
		spin_unlock_irqrestore(&dd->queue_lock, flags);
		return -EBUSY;
	}
	dd->transfer_pending = 1;
	list_add_tail(&msg->queue, &dd->queue);
	spin_unlock_irqrestore(&dd->queue_lock, flags);
	queue_work(dd->workqueue, &dd->work_data);
	return 0;
}

static int msm_spi_setup(struct spi_device *spi)
{
	struct msm_spi	*dd;
	int              rc = 0;
	u32              spi_ioc;
	u32              spi_config;
	u32              mask;

	if (spi->bits_per_word < 4 || spi->bits_per_word > 32) {
		dev_err(&spi->dev, "%s: invalid bits_per_word %d\n",
			__func__, spi->bits_per_word);
		rc = -EINVAL;
	}
	if (spi->chip_select > SPI_NUM_CHIPSELECTS-1) {
		dev_err(&spi->dev, "%s, chip select %d exceeds max value %d\n",
			__func__, spi->chip_select, SPI_NUM_CHIPSELECTS - 1);
		rc = -EINVAL;
	}

	if (rc)
		goto err_setup_exit;

	dd = spi_master_get_devdata(spi->master);

	mutex_lock(&dd->core_lock);
	if (dd->suspended) {
		mutex_unlock(&dd->core_lock);
		return -EBUSY;
	}

	if (dd->use_rlock)
		remote_mutex_lock(&dd->r_lock);

	/* Configure the spi clk, miso, mosi, cs gpio */
	if (dd->pdata->gpio_config) {
		rc = dd->pdata->gpio_config();
		if (rc) {
			dev_err(&spi->dev,
					"%s: error configuring GPIOs\n",
					__func__);
			rc = -ENXIO;
			goto err_setup_gpio;
		}
	}

	rc = msm_spi_request_gpios(dd);
	if (rc) {
		rc = -ENXIO;
		goto err_setup_gpio;
	}

	clk_prepare_enable(dd->clk);
	clk_prepare_enable(dd->pclk);

	spi_ioc = readl_relaxed(dd->base + SPI_IO_CONTROL);
	mask = SPI_IO_C_CS_N_POLARITY_0 << spi->chip_select;
	if (spi->mode & SPI_CS_HIGH)
		spi_ioc |= mask;
	else
		spi_ioc &= ~mask;
	if (spi->mode & SPI_CPOL)
		spi_ioc |= SPI_IO_C_CLK_IDLE_HIGH;
	else
		spi_ioc &= ~SPI_IO_C_CLK_IDLE_HIGH;

	writel_relaxed(spi_ioc, dd->base + SPI_IO_CONTROL);

	spi_config = readl_relaxed(dd->base + SPI_CONFIG);
	if (spi->mode & SPI_LOOP)
		spi_config |= SPI_CFG_LOOPBACK;
	else
		spi_config &= ~SPI_CFG_LOOPBACK;
	if (spi->mode & SPI_CPHA)
		spi_config &= ~SPI_CFG_INPUT_FIRST;
	else
		spi_config |= SPI_CFG_INPUT_FIRST;
	writel_relaxed(spi_config, dd->base + SPI_CONFIG);

	/* Ensure previous write completed before disabling the clocks */
	mb();
	clk_disable_unprepare(dd->clk);
	clk_disable_unprepare(dd->pclk);

	/* Free  the spi clk, miso, mosi, cs gpio */
	if (dd->pdata && dd->pdata->gpio_release)
		dd->pdata->gpio_release();
	msm_spi_free_gpios(dd);

err_setup_gpio:
	if (dd->use_rlock)
		remote_mutex_unlock(&dd->r_lock);
	mutex_unlock(&dd->core_lock);
err_setup_exit:
	return rc;
}

#ifdef CONFIG_DEBUG_FS
static int debugfs_iomem_x32_set(void *data, u64 val)
{
	writel_relaxed(val, data);
	/* Ensure the previous write completed. */
	mb();
	return 0;
}

static int debugfs_iomem_x32_get(void *data, u64 *val)
{
	*val = readl_relaxed(data);
	/* Ensure the previous read completed. */
	mb();
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x32, debugfs_iomem_x32_get,
			debugfs_iomem_x32_set, "0x%08llx\n");

static void spi_debugfs_init(struct msm_spi *dd)
{
	dd->dent_spi = debugfs_create_dir(dev_name(dd->dev), NULL);
	if (dd->dent_spi) {
		int i;

		for (i = 0; i < ARRAY_SIZE(debugfs_spi_regs); i++) {
			dd->debugfs_spi_regs[i] =
			   debugfs_create_file(
			       debugfs_spi_regs[i].name,
			       debugfs_spi_regs[i].mode,
			       dd->dent_spi,
			       dd->base + debugfs_spi_regs[i].offset,
			       &fops_iomem_x32);
		}
	}
}

static void spi_debugfs_exit(struct msm_spi *dd)
{
	if (dd->dent_spi) {
		int i;

		debugfs_remove_recursive(dd->dent_spi);
		dd->dent_spi = NULL;
		for (i = 0; i < ARRAY_SIZE(debugfs_spi_regs); i++)
			dd->debugfs_spi_regs[i] = NULL;
	}
}
#else
static void spi_debugfs_init(struct msm_spi *dd) {}
static void spi_debugfs_exit(struct msm_spi *dd) {}
#endif

/* ===Device attributes begin=== */
static ssize_t show_stats(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct msm_spi *dd =  spi_master_get_devdata(master);

	return snprintf(buf, PAGE_SIZE,
			"Device       %s\n"
			"rx fifo_size = %d spi words\n"
			"tx fifo_size = %d spi words\n"
			"use_dma ?    %s\n"
			"rx block size = %d bytes\n"
			"tx block size = %d bytes\n"
			"burst size = %d bytes\n"
			"DMA configuration:\n"
			"tx_ch=%d, rx_ch=%d, tx_crci= %d, rx_crci=%d\n"
			"--statistics--\n"
			"Rx isrs  = %d\n"
			"Tx isrs  = %d\n"
			"DMA error  = %d\n"
			"--debug--\n"
			"NA yet\n",
			dev_name(dev),
			dd->input_fifo_size,
			dd->output_fifo_size,
			dd->use_dma ? "yes" : "no",
			dd->input_block_size,
			dd->output_block_size,
			dd->burst_size,
			dd->tx_dma_chan,
			dd->rx_dma_chan,
			dd->tx_dma_crci,
			dd->rx_dma_crci,
			dd->stat_rx + dd->stat_dmov_rx,
			dd->stat_tx + dd->stat_dmov_tx,
			dd->stat_dmov_tx_err + dd->stat_dmov_rx_err
			);
}

/* Reset statistics on write */
static ssize_t set_stats(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct msm_spi *dd = dev_get_drvdata(dev);
	dd->stat_rx = 0;
	dd->stat_tx = 0;
	dd->stat_dmov_rx = 0;
	dd->stat_dmov_tx = 0;
	dd->stat_dmov_rx_err = 0;
	dd->stat_dmov_tx_err = 0;
	return count;
}

static DEVICE_ATTR(stats, S_IRUGO | S_IWUSR, show_stats, set_stats);

static struct attribute *dev_attrs[] = {
	&dev_attr_stats.attr,
	NULL,
};

static struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};
/* ===Device attributes end=== */

/**
 * spi_dmov_tx_complete_func - DataMover tx completion callback
 *
 * Executed in IRQ context (Data Mover's IRQ) DataMover's
 * spinlock @msm_dmov_lock held.
 */
static void spi_dmov_tx_complete_func(struct msm_dmov_cmd *cmd,
				      unsigned int result,
				      struct msm_dmov_errdata *err)
{
	struct msm_spi *dd;

	if (!(result & DMOV_RSLT_VALID)) {
		pr_err("Invalid DMOV result: rc=0x%08x, cmd = %p", result, cmd);
		return;
	}
	/* restore original context */
	dd = container_of(cmd, struct msm_spi, tx_hdr);
	if (result & DMOV_RSLT_DONE) {
		dd->stat_dmov_tx++;
		if ((atomic_inc_return(&dd->tx_irq_called) == 1))
			return;
		complete(&dd->transfer_complete);
	} else {
		/* Error or flush */
		if (result & DMOV_RSLT_ERROR) {
			dev_err(dd->dev, "DMA error (0x%08x)\n", result);
			dd->stat_dmov_tx_err++;
		}
		if (result & DMOV_RSLT_FLUSH) {
			/*
			 * Flushing normally happens in process of
			 * removing, when we are waiting for outstanding
			 * DMA commands to be flushed.
			 */
			dev_info(dd->dev,
				 "DMA channel flushed (0x%08x)\n", result);
		}
		if (err)
			dev_err(dd->dev,
				"Flush data(%08x %08x %08x %08x %08x %08x)\n",
				err->flush[0], err->flush[1], err->flush[2],
				err->flush[3], err->flush[4], err->flush[5]);
		dd->cur_msg->status = -EIO;
		complete(&dd->transfer_complete);
	}
}

/**
 * spi_dmov_rx_complete_func - DataMover rx completion callback
 *
 * Executed in IRQ context (Data Mover's IRQ)
 * DataMover's spinlock @msm_dmov_lock held.
 */
static void spi_dmov_rx_complete_func(struct msm_dmov_cmd *cmd,
				      unsigned int result,
				      struct msm_dmov_errdata *err)
{
	struct msm_spi *dd;

	if (!(result & DMOV_RSLT_VALID)) {
		pr_err("Invalid DMOV result(rc = 0x%08x, cmd = %p)",
		       result, cmd);
		return;
	}
	/* restore original context */
	dd = container_of(cmd, struct msm_spi, rx_hdr);
	if (result & DMOV_RSLT_DONE) {
		dd->stat_dmov_rx++;
		if (atomic_inc_return(&dd->rx_irq_called) == 1)
			return;
		complete(&dd->transfer_complete);
	} else {
		/** Error or flush  */
		if (result & DMOV_RSLT_ERROR) {
			dev_err(dd->dev, "DMA error(0x%08x)\n", result);
			dd->stat_dmov_rx_err++;
		}
		if (result & DMOV_RSLT_FLUSH) {
			dev_info(dd->dev,
				"DMA channel flushed(0x%08x)\n", result);
		}
		if (err)
			dev_err(dd->dev,
				"Flush data(%08x %08x %08x %08x %08x %08x)\n",
				err->flush[0], err->flush[1], err->flush[2],
				err->flush[3], err->flush[4], err->flush[5]);
		dd->cur_msg->status = -EIO;
		complete(&dd->transfer_complete);
	}
}

static inline u32 get_chunk_size(struct msm_spi *dd)
{
	u32 cache_line = dma_get_cache_alignment();

	return (roundup(sizeof(struct spi_dmov_cmd), DM_BYTE_ALIGN) +
			  roundup(dd->burst_size, cache_line))*2;
}

static void msm_spi_teardown_dma(struct msm_spi *dd)
{
	int limit = 0;

	if (!dd->use_dma)
		return;

	while (dd->mode == SPI_DMOV_MODE && limit++ < 50) {
		msm_dmov_flush(dd->tx_dma_chan, 1);
		msm_dmov_flush(dd->rx_dma_chan, 1);
		msleep(10);
	}

	dma_free_coherent(NULL, get_chunk_size(dd), dd->tx_dmov_cmd,
			  dd->tx_dmov_cmd_dma);
	dd->tx_dmov_cmd = dd->rx_dmov_cmd = NULL;
	dd->tx_padding = dd->rx_padding = NULL;
}

static __init int msm_spi_init_dma(struct msm_spi *dd)
{
	dmov_box *box;
	u32 cache_line = dma_get_cache_alignment();

	/* Allocate all as one chunk, since all is smaller than page size */

	/* We send NULL device, since it requires coherent_dma_mask id
	   device definition, we're okay with using system pool */
	dd->tx_dmov_cmd = dma_alloc_coherent(NULL, get_chunk_size(dd),
					     &dd->tx_dmov_cmd_dma, GFP_KERNEL);
	if (dd->tx_dmov_cmd == NULL)
		return -ENOMEM;

	/* DMA addresses should be 64 bit aligned aligned */
	dd->rx_dmov_cmd = (struct spi_dmov_cmd *)
			  ALIGN((size_t)&dd->tx_dmov_cmd[1], DM_BYTE_ALIGN);
	dd->rx_dmov_cmd_dma = ALIGN(dd->tx_dmov_cmd_dma +
			      sizeof(struct spi_dmov_cmd), DM_BYTE_ALIGN);

	/* Buffers should be aligned to cache line */
	dd->tx_padding = (u8 *)ALIGN((size_t)&dd->rx_dmov_cmd[1], cache_line);
	dd->tx_padding_dma = ALIGN(dd->rx_dmov_cmd_dma +
			      sizeof(struct spi_dmov_cmd), cache_line);
	dd->rx_padding = (u8 *)ALIGN((size_t)(dd->tx_padding + dd->burst_size),
				     cache_line);
	dd->rx_padding_dma = ALIGN(dd->tx_padding_dma + dd->burst_size,
				      cache_line);

	/* Setup DM commands */
	box = &(dd->rx_dmov_cmd->box);
	box->cmd = CMD_MODE_BOX | CMD_SRC_CRCI(dd->rx_dma_crci);
	box->src_row_addr = (uint32_t)dd->mem_phys_addr + SPI_INPUT_FIFO;
	dd->rx_hdr.cmdptr = DMOV_CMD_PTR_LIST |
				   DMOV_CMD_ADDR(dd->rx_dmov_cmd_dma +
				   offsetof(struct spi_dmov_cmd, cmd_ptr));
	dd->rx_hdr.complete_func = spi_dmov_rx_complete_func;

	box = &(dd->tx_dmov_cmd->box);
	box->cmd = CMD_MODE_BOX | CMD_DST_CRCI(dd->tx_dma_crci);
	box->dst_row_addr = (uint32_t)dd->mem_phys_addr + SPI_OUTPUT_FIFO;
	dd->tx_hdr.cmdptr = DMOV_CMD_PTR_LIST |
			    DMOV_CMD_ADDR(dd->tx_dmov_cmd_dma +
			    offsetof(struct spi_dmov_cmd, cmd_ptr));
	dd->tx_hdr.complete_func = spi_dmov_tx_complete_func;

	dd->tx_dmov_cmd->single_pad.cmd = CMD_MODE_SINGLE | CMD_LC |
					  CMD_DST_CRCI(dd->tx_dma_crci);
	dd->tx_dmov_cmd->single_pad.dst = (uint32_t)dd->mem_phys_addr +
					   SPI_OUTPUT_FIFO;
	dd->rx_dmov_cmd->single_pad.cmd = CMD_MODE_SINGLE | CMD_LC |
					  CMD_SRC_CRCI(dd->rx_dma_crci);
	dd->rx_dmov_cmd->single_pad.src = (uint32_t)dd->mem_phys_addr +
					  SPI_INPUT_FIFO;

	/* Clear remaining activities on channel */
	msm_dmov_flush(dd->tx_dma_chan, 1);
	msm_dmov_flush(dd->rx_dma_chan, 1);

	return 0;
}

struct msm_spi_platform_data *msm_spi_dt_to_pdata(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct msm_spi_platform_data *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("Unable to allocate platform data\n");
		return NULL;
	}

	of_property_read_u32(node, "spi-max-frequency",
			&pdata->max_clock_speed);
	of_property_read_u32(node, "infinite_mode",
			&pdata->infinite_mode);

	return pdata;
}

static int __init msm_spi_probe(struct platform_device *pdev)
{
	struct spi_master      *master;
	struct msm_spi	       *dd;
	struct resource	       *resource;
	int                     rc = -ENXIO;
	int                     locked = 0;
	int                     i = 0;
	int                     clk_enabled = 0;
	int                     pclk_enabled = 0;
	struct msm_spi_platform_data *pdata;
	enum of_gpio_flags flags;

	master = spi_alloc_master(&pdev->dev, sizeof(struct msm_spi));
	if (!master) {
		rc = -ENOMEM;
		dev_err(&pdev->dev, "master allocation failed\n");
		goto err_probe_exit;
	}

	master->bus_num        = pdev->id;
	master->mode_bits      = SPI_SUPPORTED_MODES;
	master->num_chipselect = SPI_NUM_CHIPSELECTS;
	master->setup          = msm_spi_setup;
	master->transfer       = msm_spi_transfer;
	platform_set_drvdata(pdev, master);
	dd = spi_master_get_devdata(master);

	if (pdev->dev.of_node) {
		dd->qup_ver = SPI_QUP_VERSION_BFAM;
		master->dev.of_node = pdev->dev.of_node;
		pdata = msm_spi_dt_to_pdata(pdev);
		if (!pdata) {
			rc = -ENOMEM;
			goto err_probe_exit;
		}

		rc = of_property_read_u32(pdev->dev.of_node,
				"cell-index", &pdev->id);
		if (rc)
			dev_warn(&pdev->dev,
				"using default bus_num %d\n", pdev->id);
		else
			master->bus_num = pdev->id;

		for (i = 0; i < ARRAY_SIZE(spi_rsrcs); ++i) {
			dd->spi_gpios[i] = of_get_gpio_flags(pdev->dev.of_node,
								i, &flags);
		}

		for (i = 0; i < ARRAY_SIZE(spi_cs_rsrcs); ++i) {
			dd->cs_gpios[i].gpio_num = of_get_named_gpio_flags(
						pdev->dev.of_node, "cs-gpios",
						i, &flags);
			dd->cs_gpios[i].valid = 0;
		}
	} else {
		pdata = pdev->dev.platform_data;
		dd->qup_ver = SPI_QUP_VERSION_NONE;

		for (i = 0; i < ARRAY_SIZE(spi_rsrcs); ++i) {
			resource = platform_get_resource(pdev, IORESOURCE_IO,
							i);
			dd->spi_gpios[i] = resource ? resource->start : -1;
		}

		for (i = 0; i < ARRAY_SIZE(spi_cs_rsrcs); ++i) {
			resource = platform_get_resource(pdev, IORESOURCE_IO,
						i + ARRAY_SIZE(spi_rsrcs));
			dd->cs_gpios[i].gpio_num = resource ?
							resource->start : -1;
			dd->cs_gpios[i].valid = 0;
		}
	}

	dd->pdata = pdata;
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		rc = -ENXIO;
		goto err_probe_res;
	}

	dd->mem_phys_addr = resource->start;
	dd->mem_size = resource_size(resource);

	if (pdata) {
		if (pdata->dma_config) {
			rc = pdata->dma_config();
			if (rc) {
				dev_warn(&pdev->dev,
					"%s: DM mode not supported\n",
					__func__);
				dd->use_dma = 0;
				goto skip_dma_resources;
			}
		}
		resource = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		if (resource) {
			dd->rx_dma_chan = resource->start;
			dd->tx_dma_chan = resource->end;
			resource = platform_get_resource(pdev, IORESOURCE_DMA,
							1);
			if (!resource) {
				rc = -ENXIO;
				goto err_probe_res;
			}

			dd->rx_dma_crci = resource->start;
			dd->tx_dma_crci = resource->end;
			dd->use_dma = 1;
			master->dma_alignment =	dma_get_cache_alignment();
		}
	}

skip_dma_resources:

	spin_lock_init(&dd->queue_lock);
	mutex_init(&dd->core_lock);
	INIT_LIST_HEAD(&dd->queue);
	INIT_WORK(&dd->work_data, msm_spi_workq);
	init_waitqueue_head(&dd->continue_suspend);
	dd->workqueue = create_singlethread_workqueue(
			dev_name(master->dev.parent));
	if (!dd->workqueue)
		goto err_probe_workq;

	if (!devm_request_mem_region(&pdev->dev, dd->mem_phys_addr,
					dd->mem_size, SPI_DRV_NAME)) {
		rc = -ENXIO;
		goto err_probe_reqmem;
	}

	dd->base = devm_ioremap(&pdev->dev, dd->mem_phys_addr, dd->mem_size);
	if (!dd->base) {
		rc = -ENOMEM;
		goto err_probe_reqmem;
	}

	if (pdata && pdata->rsl_id) {
		struct remote_mutex_id rmid;
		rmid.r_spinlock_id = pdata->rsl_id;
		rmid.delay_us = SPI_TRYLOCK_DELAY;

		rc = remote_mutex_init(&dd->r_lock, &rmid);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to init remote_mutex "
				"(%s), (rc=%d)\n", rmid.r_spinlock_id,
				__func__, rc);
			goto err_probe_rlock_init;
		}

		dd->use_rlock = 1;
		dd->pm_lat = pdata->pm_lat;
		pm_qos_add_request(&qos_req_list, PM_QOS_CPU_DMA_LATENCY,
					PM_QOS_DEFAULT_VALUE);
	}

	mutex_lock(&dd->core_lock);
	if (dd->use_rlock)
		remote_mutex_lock(&dd->r_lock);

	locked = 1;
	dd->dev = &pdev->dev;
	dd->clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(dd->clk)) {
		dev_err(&pdev->dev, "%s: unable to get core_clk\n", __func__);
		rc = PTR_ERR(dd->clk);
		goto err_probe_clk_get;
	}

	dd->pclk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(dd->pclk)) {
		dev_err(&pdev->dev, "%s: unable to get iface_clk\n", __func__);
		rc = PTR_ERR(dd->pclk);
		goto err_probe_pclk_get;
	}

	if (pdata && pdata->max_clock_speed)
		msm_spi_clock_set(dd, dd->pdata->max_clock_speed);

	rc = clk_prepare_enable(dd->clk);
	if (rc) {
		dev_err(&pdev->dev, "%s: unable to enable core_clk\n",
			__func__);
		goto err_probe_clk_enable;
	}

	clk_enabled = 1;
	rc = clk_prepare_enable(dd->pclk);
	if (rc) {
		dev_err(&pdev->dev, "%s: unable to enable iface_clk\n",
		__func__);
		goto err_probe_pclk_enable;
	}

	pclk_enabled = 1;
	rc = msm_spi_configure_gsbi(dd, pdev);
	if (rc)
		goto err_probe_gsbi;

	msm_spi_calculate_fifo_size(dd);
	if (dd->use_dma) {
		rc = msm_spi_init_dma(dd);
		if (rc)
			goto err_probe_dma;
	}

	msm_spi_register_init(dd);
	/*
	 * The SPI core generates a bogus input overrun error on some targets,
	 * when a transition from run to reset state occurs and if the FIFO has
	 * an odd number of entries. Hence we disable the INPUT_OVER_RUN_ERR_EN
	 * bit.
	 */
	msm_spi_enable_error_flags(dd);

	writel_relaxed(SPI_IO_C_NO_TRI_STATE, dd->base + SPI_IO_CONTROL);
	rc = msm_spi_set_state(dd, SPI_OP_STATE_RESET);
	if (rc)
		goto err_probe_state;

	clk_disable_unprepare(dd->clk);
	clk_disable_unprepare(dd->pclk);
	clk_enabled = 0;
	pclk_enabled = 0;

	dd->suspended = 0;
	dd->transfer_pending = 0;
	dd->multi_xfr = 0;
	dd->mode = SPI_MODE_NONE;

	rc = msm_spi_request_irq(dd, pdev, master);
	if (rc)
		goto err_probe_irq;

	msm_spi_disable_irqs(dd);
	if (dd->use_rlock)
		remote_mutex_unlock(&dd->r_lock);

	mutex_unlock(&dd->core_lock);
	locked = 0;

	rc = spi_register_master(master);
	if (rc)
		goto err_probe_reg_master;

	rc = sysfs_create_group(&(dd->dev->kobj), &dev_attr_grp);
	if (rc) {
		dev_err(&pdev->dev, "failed to create dev. attrs : %d\n", rc);
		goto err_attrs;
	}

	spi_debugfs_init(dd);

	return 0;

err_attrs:
	spi_unregister_master(master);
err_probe_reg_master:
err_probe_irq:
err_probe_state:
	msm_spi_teardown_dma(dd);
err_probe_dma:
err_probe_gsbi:
	if (pclk_enabled)
		clk_disable_unprepare(dd->pclk);
err_probe_pclk_enable:
	if (clk_enabled)
		clk_disable_unprepare(dd->clk);
err_probe_clk_enable:
	clk_put(dd->pclk);
err_probe_pclk_get:
	clk_put(dd->clk);
err_probe_clk_get:
	if (locked) {
		if (dd->use_rlock)
			remote_mutex_unlock(&dd->r_lock);

		mutex_unlock(&dd->core_lock);
	}
err_probe_rlock_init:
err_probe_reqmem:
	destroy_workqueue(dd->workqueue);
err_probe_workq:
err_probe_res:
	spi_master_put(master);
err_probe_exit:
	return rc;
}

#ifdef CONFIG_PM
static int msm_spi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct msm_spi    *dd;
	unsigned long      flags;

	if (!master)
		goto suspend_exit;
	dd = spi_master_get_devdata(master);
	if (!dd)
		goto suspend_exit;

	/* Make sure nothing is added to the queue while we're suspending */
	spin_lock_irqsave(&dd->queue_lock, flags);
	dd->suspended = 1;
	spin_unlock_irqrestore(&dd->queue_lock, flags);

	/* Wait for transactions to end, or time out */
	wait_event_interruptible(dd->continue_suspend, !dd->transfer_pending);

suspend_exit:
	return 0;
}

static int msm_spi_resume(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct msm_spi    *dd;

	if (!master)
		goto resume_exit;
	dd = spi_master_get_devdata(master);
	if (!dd)
		goto resume_exit;

	dd->suspended = 0;
resume_exit:
	return 0;
}
#else
#define msm_spi_suspend NULL
#define msm_spi_resume NULL
#endif /* CONFIG_PM */

static int __devexit msm_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct msm_spi    *dd = spi_master_get_devdata(master);

	pm_qos_remove_request(&qos_req_list);
	spi_debugfs_exit(dd);
	sysfs_remove_group(&pdev->dev.kobj, &dev_attr_grp);

	msm_spi_teardown_dma(dd);

	clk_put(dd->clk);
	clk_put(dd->pclk);
	destroy_workqueue(dd->workqueue);
	platform_set_drvdata(pdev, 0);
	spi_unregister_master(master);
	spi_master_put(master);

	return 0;
}

static struct of_device_id msm_spi_dt_match[] = {
	{
		.compatible = "qcom,spi-qup-v2",
	},
	{}
};

static struct platform_driver msm_spi_driver = {
	.driver		= {
		.name	= SPI_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = msm_spi_dt_match,
	},
	.suspend        = msm_spi_suspend,
	.resume         = msm_spi_resume,
	.remove		= __exit_p(msm_spi_remove),
};

static int __init msm_spi_init(void)
{
	return platform_driver_probe(&msm_spi_driver, msm_spi_probe);
}
module_init(msm_spi_init);

static void __exit msm_spi_exit(void)
{
	platform_driver_unregister(&msm_spi_driver);
}
module_exit(msm_spi_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.4");
MODULE_ALIAS("platform:"SPI_DRV_NAME);
