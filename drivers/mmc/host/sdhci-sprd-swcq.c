// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2021 UNISOC, Inc.
// Author: Zhongwu Zhu <zhongwu.zhu@unisoc.com>

#include "sdhci-sprd-debug.h"
#ifndef CONFIG_SPRD_DEBUG
#include "sdhci-sprd-debug.c"
#endif

#define _DRIVER_NAME "sprd-sdhci-swcq"
#define DBG(f, x...) \
	pr_debug("%s: " _DRIVER_NAME ": " f, mmc_hostname(host->mmc), ## x)

#define SDHCI_DUMP(f, x...) \
	pr_err("%s: " _DRIVER_NAME ": " f, mmc_hostname(host->mmc), ## x)

#define POLL_TIMEOUT             (1000*1000)     /* 1ms */
#define CMD_TIMEOUT             (5*1000*1000*1000UL)     /* 5s */
static unsigned long long pre_time, second_time, now;
static bool sdhci_sprd_send_command(struct sdhci_host *host, struct mmc_command *cmd);
static void sdhci_sprd_finish_command(struct sdhci_host *host);

static void sdhci_sprd_dumpregs(struct sdhci_host *host)
{
	SDHCI_DUMP("============ SPRD SDHCI REGISTER DUMP ===========\n");

	SDHCI_DUMP("Sys addr:  0x%08x | Version:  0x%08x\n",
		   sdhci_readl(host, SDHCI_DMA_ADDRESS),
		   sdhci_readw(host, SDHCI_HOST_VERSION));
	SDHCI_DUMP("Blk size:  0x%08x | Blk cnt:  0x%08x\n",
		   sdhci_readw(host, SDHCI_BLOCK_SIZE),
		   sdhci_readw(host, SDHCI_BLOCK_COUNT));
	SDHCI_DUMP("Argument:  0x%08x | Trn mode: 0x%08x\n",
		   sdhci_readl(host, SDHCI_ARGUMENT),
		   sdhci_readw(host, SDHCI_TRANSFER_MODE));
	SDHCI_DUMP("Present:   0x%08x | Host ctl: 0x%08x\n",
		   sdhci_readl(host, SDHCI_PRESENT_STATE),
		   sdhci_readb(host, SDHCI_HOST_CONTROL));
	SDHCI_DUMP("Power:     0x%08x | Blk gap:  0x%08x\n",
		   sdhci_readb(host, SDHCI_POWER_CONTROL),
		   sdhci_readb(host, SDHCI_BLOCK_GAP_CONTROL));
	SDHCI_DUMP("Wake-up:   0x%08x | Clock:    0x%08x\n",
		   sdhci_readb(host, SDHCI_WAKE_UP_CONTROL),
		   sdhci_readw(host, SDHCI_CLOCK_CONTROL));
	SDHCI_DUMP("Timeout:   0x%08x | Int stat: 0x%08x\n",
		   sdhci_readb(host, SDHCI_TIMEOUT_CONTROL),
		   sdhci_readl(host, SDHCI_INT_STATUS));
	SDHCI_DUMP("Int enab:  0x%08x | Sig enab: 0x%08x\n",
		   sdhci_readl(host, SDHCI_INT_ENABLE),
		   sdhci_readl(host, SDHCI_SIGNAL_ENABLE));
	SDHCI_DUMP("ACmd stat: 0x%08x | Slot int: 0x%08x\n",
		   sdhci_readw(host, SDHCI_AUTO_CMD_STATUS),
		   sdhci_readw(host, SDHCI_SLOT_INT_STATUS));
	SDHCI_DUMP("Caps:      0x%08x | Caps_1:   0x%08x\n",
		   sdhci_readl(host, SDHCI_CAPABILITIES),
		   sdhci_readl(host, SDHCI_CAPABILITIES_1));
	SDHCI_DUMP("Cmd:       0x%08x | Max curr: 0x%08x\n",
		   sdhci_readw(host, SDHCI_COMMAND),
		   sdhci_readl(host, SDHCI_MAX_CURRENT));
	SDHCI_DUMP("Resp[0]:   0x%08x | Resp[1]:  0x%08x\n",
		   sdhci_readl(host, SDHCI_RESPONSE),
		   sdhci_readl(host, SDHCI_RESPONSE + 4));
	SDHCI_DUMP("Resp[2]:   0x%08x | Resp[3]:  0x%08x\n",
		   sdhci_readl(host, SDHCI_RESPONSE + 8),
		   sdhci_readl(host, SDHCI_RESPONSE + 12));
	SDHCI_DUMP("Host ctl2: 0x%08x\n",
		   sdhci_readw(host, SDHCI_HOST_CONTROL2));

	if (host->flags & SDHCI_USE_ADMA) {
		if (host->flags & SDHCI_USE_64_BIT_DMA) {
			SDHCI_DUMP("ADMA Err:  0x%08x | ADMA Ptr: 0x%08x%08x\n",
				   sdhci_readl(host, SDHCI_ADMA_ERROR),
				   sdhci_readl(host, SDHCI_ADMA_ADDRESS_HI),
				   sdhci_readl(host, SDHCI_ADMA_ADDRESS));
		} else {
			SDHCI_DUMP("ADMA Err:  0x%08x | ADMA Ptr: 0x%08x\n",
				   sdhci_readl(host, SDHCI_ADMA_ERROR),
				   sdhci_readl(host, SDHCI_ADMA_ADDRESS));
		}
	}

	SDHCI_DUMP("============================================\n");
}

static void sdhci_sprd_set_adma_addr(struct sdhci_host *host, dma_addr_t addr)
{
	sdhci_writel(host, lower_32_bits(addr), SDHCI_ADMA_ADDRESS);
	if (host->flags & SDHCI_USE_64_BIT_DMA)
		sdhci_writel(host, upper_32_bits(addr), SDHCI_ADMA_ADDRESS_HI);
}

static dma_addr_t sdhci_sprd_sdma_address(struct sdhci_host *host)
{
	if (host->bounce_buffer)
		return host->bounce_addr;
	else
		return sg_dma_address(host->data->sg);
}

static void sdhci_sprd_set_sdma_addr(struct sdhci_host *host, dma_addr_t addr)
{
	if (host->v4_mode)
		sdhci_sprd_set_adma_addr(host, addr);
	else
		sdhci_writel(host, addr, SDHCI_DMA_ADDRESS);
}

static void sdhci_sprd_read_block_pio(struct sdhci_host *host)
{
	unsigned long flags;
	size_t blksize, len, chunk;
	u32 scratch;
	u8 *buf;

	DBG("PIO reading\n");

	blksize = host->data->blksz;
	chunk = 0;

	local_irq_save(flags);

	while (blksize) {
		BUG_ON(!sg_miter_next(&host->sg_miter));

		len = min(host->sg_miter.length, blksize);

		blksize -= len;
		host->sg_miter.consumed = len;

		buf = host->sg_miter.addr;

		while (len) {
			if (chunk == 0) {
				scratch = sdhci_readl(host, SDHCI_BUFFER);
				chunk = 4;
			}

			*buf = scratch & 0xFF;

			buf++;
			scratch >>= 8;
			chunk--;
			len--;
		}
	}

	sg_miter_stop(&host->sg_miter);

	local_irq_restore(flags);
}

static void sdhci_sprd_write_block_pio(struct sdhci_host *host)
{
	unsigned long flags;
	size_t blksize, len, chunk;
	u32 scratch;
	u8 *buf;


	DBG("PIO writing\n");

	blksize = host->data->blksz;
	chunk = 0;
	scratch = 0;

	local_irq_save(flags);

	while (blksize) {
		BUG_ON(!sg_miter_next(&host->sg_miter));

		len = min(host->sg_miter.length, blksize);

		blksize -= len;
		host->sg_miter.consumed = len;

		buf = host->sg_miter.addr;

		while (len) {
			scratch |= (u32)*buf << (chunk * 8);

			buf++;
			chunk++;
			len--;

			if ((chunk == 4) || ((len == 0) && (blksize == 0))) {
				sdhci_writel(host, scratch, SDHCI_BUFFER);
				chunk = 0;
				scratch = 0;
			}
		}
	}

	sg_miter_stop(&host->sg_miter);

	local_irq_restore(flags);
}

static void sdhci_sprd_transfer_pio(struct sdhci_host *host)
{
	u32 mask;

	if (host->blocks == 0)
		return;

	if (host->data->flags & MMC_DATA_READ)
		mask = SDHCI_DATA_AVAILABLE;
	else
		mask = SDHCI_SPACE_AVAILABLE;

	/*
	 * Some controllers (JMicron JMB38x) mess up the buffer bits
	 * for transfers < 4 bytes. As long as it is just one block,
	 * we can ignore the bits.
	 */
	if ((host->quirks & SDHCI_QUIRK_BROKEN_SMALL_PIO) &&
		(host->data->blocks == 1))
		mask = ~0;

	while (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) {
		if (host->quirks & SDHCI_QUIRK_PIO_NEEDS_DELAY)
			udelay(100);

		if (host->data->flags & MMC_DATA_READ)
			sdhci_sprd_read_block_pio(host);
		else
			sdhci_sprd_write_block_pio(host);

		host->blocks--;
		if (host->blocks == 0)
			break;
	}

	DBG("PIO transfer complete.\n");
}

static inline bool sdhci_sprd_has_requests(struct sdhci_host *host)
{
	return host->cmd || host->data_cmd;
}

static inline bool sdhci_sprd_data_line_cmd(struct mmc_command *cmd)
{
	return cmd->data || cmd->flags & MMC_RSP_BUSY;
}

static void sdhci_sprd_mod_timer(struct sdhci_host *host, struct mmc_request *mrq,
			    unsigned long timeout)
{
	if (sdhci_sprd_data_line_cmd(mrq->cmd))
		mod_timer(&host->data_timer, timeout);
	else
		mod_timer(&host->timer, timeout);

	if (sdhci_sprd_mmc_debug_judge() && HOST_IS_EMMC_TYPE(host->mmc) &&
		sdhci_sprd_data_line_cmd(mrq->cmd))
		sdhci_sprd_mod_debug_timer(host, jiffies + 256);
}

static void sdhci_sprd_del_timer(struct sdhci_host *host, struct mmc_request *mrq)
{
	if (sdhci_sprd_data_line_cmd(mrq->cmd))
		del_timer(&host->data_timer);
	else
		del_timer(&host->timer);

	if (sdhci_sprd_mmc_debug_judge() && HOST_IS_EMMC_TYPE(host->mmc) &&
		sdhci_sprd_data_line_cmd(mrq->cmd))
		sdhci_sprd_del_debug_timer(host);
}

static inline bool sdhci_sprd_auto_cmd12(struct sdhci_host *host,
				    struct mmc_request *mrq)
{
	return !mrq->sbc && (host->flags & SDHCI_AUTO_CMD12) &&
	       !mrq->cap_cmd_during_tfr;
}

static inline bool sdhci_sprd_auto_cmd23(struct sdhci_host *host,
				    struct mmc_request *mrq)
{
	return mrq->sbc && (host->flags & SDHCI_AUTO_CMD23);
}

static inline bool sdhci_manual_cmd23(struct sdhci_host *host,
				      struct mmc_request *mrq)
{
	return mrq->sbc && !(host->flags & SDHCI_AUTO_CMD23);
}

static inline void sdhci_sprd_auto_cmd_select(struct sdhci_host *host,
					 struct mmc_command *cmd,
					 u16 *mode)
{
	bool use_cmd12 = sdhci_sprd_auto_cmd12(host, cmd->mrq) &&
			 (cmd->opcode != SD_IO_RW_EXTENDED);
	bool use_cmd23 = sdhci_sprd_auto_cmd23(host, cmd->mrq);
	u16 ctrl2;

	/*
	 * In case of Version 4.10 or later, use of 'Auto CMD Auto
	 * Select' is recommended rather than use of 'Auto CMD12
	 * Enable' or 'Auto CMD23 Enable'. We require Version 4 Mode
	 * here because some controllers (e.g sdhci-of-dwmshc) expect it.
	 */
	if (host->version >= SDHCI_SPEC_410 && host->v4_mode &&
	    (use_cmd12 || use_cmd23)) {
		*mode |= SDHCI_TRNS_AUTO_SEL;

		ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		if (use_cmd23)
			ctrl2 |= SDHCI_CMD23_ENABLE;
		else
			ctrl2 &= ~SDHCI_CMD23_ENABLE;
		sdhci_writew(host, ctrl2, SDHCI_HOST_CONTROL2);

		return;
	}

	/*
	 * If we are sending CMD23, CMD12 never gets sent
	 * on successful completion (so no Auto-CMD12).
	 */
	if (use_cmd12)
		*mode |= SDHCI_TRNS_AUTO_CMD12;
	else if (use_cmd23)
		*mode |= SDHCI_TRNS_AUTO_CMD23;
}

static void sdhci_sprd_set_transfer_mode(struct sdhci_host *host,
	struct mmc_command *cmd)
{
	u16 mode = 0;
	struct mmc_data *data = cmd->data;

	if (data == NULL) {
		if (host->quirks2 &
			SDHCI_QUIRK2_CLEAR_TRANSFERMODE_REG_BEFORE_CMD) {
			/* must not clear SDHCI_TRANSFER_MODE when tuning */
			if (cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200)
				sdhci_writew(host, 0x0, SDHCI_TRANSFER_MODE);
		} else {
		/* clear Auto CMD settings for no data CMDs */
			mode = sdhci_readw(host, SDHCI_TRANSFER_MODE);
			sdhci_writew(host, mode & ~(SDHCI_TRNS_AUTO_CMD12 |
				SDHCI_TRNS_AUTO_CMD23), SDHCI_TRANSFER_MODE);
		}
		return;
	}

	WARN_ON(!host->data);

	if (!(host->quirks2 & SDHCI_QUIRK2_SUPPORT_SINGLE))
		mode = SDHCI_TRNS_BLK_CNT_EN;

	if (mmc_op_multi(cmd->opcode) || data->blocks > 1) {
		mode = SDHCI_TRNS_BLK_CNT_EN | SDHCI_TRNS_MULTI;
		sdhci_sprd_auto_cmd_select(host, cmd, &mode);
		if (sdhci_sprd_auto_cmd23(host, cmd->mrq))
			sdhci_writel(host, cmd->mrq->sbc->arg, SDHCI_ARGUMENT2);
	}

	if (data->flags & MMC_DATA_READ)
		mode |= SDHCI_TRNS_READ;
	if (host->flags & SDHCI_REQ_USE_DMA)
		mode |= SDHCI_TRNS_DMA;

	sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
}

static bool sdhci_sprd_needs_reset(struct sdhci_host *host, struct mmc_request *mrq)
{
	return (!(host->flags & SDHCI_DEVICE_DEAD) &&
		((mrq->cmd && mrq->cmd->error) ||
		 (mrq->sbc && mrq->sbc->error) ||
		 (mrq->data && mrq->data->stop && mrq->data->stop->error) ||
		 (host->quirks & SDHCI_QUIRK_RESET_AFTER_REQUEST)));
}

static void __sdhci_sprd_finish_mrq(struct sdhci_host *host, struct mmc_request *mrq)
{
	int i;

	if (host->cmd && host->cmd->mrq == mrq)
		host->cmd = NULL;

	if (host->data_cmd && host->data_cmd->mrq == mrq)
		host->data_cmd = NULL;

	if (host->deferred_cmd && host->deferred_cmd->mrq == mrq)
		host->deferred_cmd = NULL;

	if (host->data && host->data->mrq == mrq)
		host->data = NULL;

	if (sdhci_sprd_needs_reset(host, mrq))
		host->pending_reset = true;

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (host->mrqs_done[i] == mrq) {
			WARN_ON(1);
			return;
		}
	}

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (!host->mrqs_done[i]) {
			host->mrqs_done[i] = mrq;
			break;
		}
	}

	WARN_ON(i >= SDHCI_MAX_MRQS);

	sdhci_sprd_del_timer(host, mrq);

}

static void sdhci_sprd_finish_mrq(struct sdhci_host *host, struct mmc_request *mrq)
{
	__sdhci_sprd_finish_mrq(host, mrq);

	queue_work(host->complete_wq, &host->complete_work);
}

static int sdhci_sprd_pre_dma_transfer(struct sdhci_host *host,
				  struct mmc_data *data, int cookie)
{
	int sg_count;

	/*
	 * If the data buffers are already mapped, return the previous
	 * dma_map_sg() result.
	 */
	if (data->host_cookie == COOKIE_PRE_MAPPED)
		return data->sg_count;

	/* Bounce write requests to the bounce buffer */
	if (host->bounce_buffer) {
		unsigned int length = data->blksz * data->blocks;

		if (length > host->bounce_buffer_size) {
			pr_err("%s: asked for transfer of %u bytes exceeds bounce buffer %u bytes\n",
			       mmc_hostname(host->mmc), length,
			       host->bounce_buffer_size);
			return -EIO;
		}
		if (mmc_get_dma_dir(data) == DMA_TO_DEVICE) {
			/* Copy the data to the bounce buffer */
			sg_copy_to_buffer(data->sg, data->sg_len,
					  host->bounce_buffer,
					  length);
		}
		/* Switch ownership to the DMA */
		dma_sync_single_for_device(host->mmc->parent,
					   host->bounce_addr,
					   host->bounce_buffer_size,
					   mmc_get_dma_dir(data));
		/* Just a dummy value */
		sg_count = 1;
	} else {
		/* Just access the data directly from memory */
		sg_count = dma_map_sg(mmc_dev(host->mmc),
				      data->sg, data->sg_len,
				      mmc_get_dma_dir(data));
	}

	if (sg_count == 0)
		return -ENOSPC;

	data->sg_count = sg_count;
	data->host_cookie = cookie;

	return sg_count;
}

static char *sdhci_sprd_kmap_atomic(struct scatterlist *sg, unsigned long *flags)
{
	local_irq_save(*flags);
	return kmap_atomic(sg_page(sg)) + sg->offset;
}

static void sdhci_sprd_kunmap_atomic(void *buffer, unsigned long *flags)
{
	kunmap_atomic(buffer);
	local_irq_restore(*flags);
}

static void sdhci_sprd_adma_write_desc(struct sdhci_host *host, void **desc,
			   dma_addr_t addr, int len, unsigned int cmd)
{
	struct sdhci_adma2_64_desc *dma_desc = *desc;

	/* 32-bit and 64-bit descriptors have these members in same position */
	dma_desc->cmd = cpu_to_le16(cmd);
	dma_desc->len = cpu_to_le16(len);
	dma_desc->addr_lo = cpu_to_le32(lower_32_bits(addr));

	if (host->flags & SDHCI_USE_64_BIT_DMA)
		dma_desc->addr_hi = cpu_to_le32(upper_32_bits(addr));

	*desc += host->desc_sz;
}

static inline void __sdhci_sprd_adma_write_desc(struct sdhci_host *host,
					   void **desc, dma_addr_t addr,
					   int len, unsigned int cmd)
{
	if (host->ops->adma_write_desc)
		host->ops->adma_write_desc(host, desc, addr, len, cmd);
	else
		sdhci_sprd_adma_write_desc(host, desc, addr, len, cmd);
}

static void sdhci_sprd_adma_mark_end(void *desc)
{
	struct sdhci_adma2_64_desc *dma_desc = desc;

	/* 32-bit and 64-bit descriptors have 'cmd' in same position */
	dma_desc->cmd |= cpu_to_le16(ADMA2_END);
}

static void sdhci_sprd_adma_table_pre(struct sdhci_host *host,
	struct mmc_data *data, int sg_count)
{
	struct scatterlist *sg;
	unsigned long flags;
	dma_addr_t addr, align_addr;
	void *desc, *align;
	char *buffer;
	int len, offset, i;

	/*
	 * The spec does not specify endianness of descriptor table.
	 * We currently guess that it is LE.
	 */

	host->sg_count = sg_count;

	desc = host->adma_table;
	align = host->align_buffer;

	align_addr = host->align_addr;

	for_each_sg(data->sg, sg, host->sg_count, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);

		/*
		 * The SDHCI specification states that ADMA addresses must
		 * be 32-bit aligned. If they aren't, then we use a bounce
		 * buffer for the (up to three) bytes that screw up the
		 * alignment.
		 */
		offset = (SDHCI_ADMA2_ALIGN - (addr & SDHCI_ADMA2_MASK)) &
			 SDHCI_ADMA2_MASK;
		if (offset) {
			if (data->flags & MMC_DATA_WRITE) {
				buffer = sdhci_sprd_kmap_atomic(sg, &flags);
				memcpy(align, buffer, offset);
				sdhci_sprd_kunmap_atomic(buffer, &flags);
			}

			/* tran, valid */
			__sdhci_sprd_adma_write_desc(host, &desc, align_addr,
						offset, ADMA2_TRAN_VALID);

			BUG_ON(offset > 65536);

			align += SDHCI_ADMA2_ALIGN;
			align_addr += SDHCI_ADMA2_ALIGN;

			addr += offset;
			len -= offset;
		}

		BUG_ON(len > 65536);

		/* tran, valid */
		if (len)
			__sdhci_sprd_adma_write_desc(host, &desc, addr, len,
						ADMA2_TRAN_VALID);

		/*
		 * If this triggers then we have a calculation bug
		 * somewhere. :/
		 */
		WARN_ON((desc - host->adma_table) >= host->adma_table_sz);
	}

	if (host->quirks & SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC) {
		/* Mark the last descriptor as the terminating descriptor */
		if (desc != host->adma_table) {
			desc -= host->desc_sz;
			sdhci_sprd_adma_mark_end(desc);
		}
	} else {
		/* Add a terminating entry - nop, end, valid */
		__sdhci_sprd_adma_write_desc(host, &desc, 0, 0, ADMA2_NOP_END_VALID);
	}
}

static void sdhci_sprd_adma_table_post(struct sdhci_host *host,
	struct mmc_data *data)
{
	struct scatterlist *sg;
	int i, size;
	void *align;
	char *buffer;
	unsigned long flags;

	if (data->flags & MMC_DATA_READ) {
		bool has_unaligned = false;

		/* Do a quick scan of the SG list for any unaligned mappings */
		for_each_sg(data->sg, sg, host->sg_count, i)
			if (sg_dma_address(sg) & SDHCI_ADMA2_MASK) {
				has_unaligned = true;
				break;
			}

		if (has_unaligned) {
			dma_sync_sg_for_cpu(mmc_dev(host->mmc), data->sg,
					    data->sg_len, DMA_FROM_DEVICE);

			align = host->align_buffer;

			for_each_sg(data->sg, sg, host->sg_count, i) {
				if (sg_dma_address(sg) & SDHCI_ADMA2_MASK) {
					size = SDHCI_ADMA2_ALIGN -
					       (sg_dma_address(sg) & SDHCI_ADMA2_MASK);

					buffer = sdhci_sprd_kmap_atomic(sg, &flags);
					memcpy(buffer, align, size);
					sdhci_sprd_kunmap_atomic(buffer, &flags);

					align += SDHCI_ADMA2_ALIGN;
				}
			}
		}
	}
}

static void sdhci_sprd_do_reset(struct sdhci_host *host, u8 mask)
{
	if (host->quirks & SDHCI_QUIRK_NO_CARD_NO_RESET) {
		struct mmc_host *mmc = host->mmc;

		if (!mmc->ops->get_cd(mmc))
			return;
	}

	host->ops->reset(host, mask);

	if (mask & SDHCI_RESET_ALL) {
		if (host->flags & (SDHCI_USE_SDMA | SDHCI_USE_ADMA)) {
			if (host->ops->enable_dma)
				host->ops->enable_dma(host);
		}

		/* Resetting the controller clears many */
		host->preset_enabled = false;
	}
}

static void __sdhci_sprd_finish_data(struct sdhci_host *host, bool sw_data_timeout)
{
	struct mmc_command *data_cmd = host->data_cmd;
	struct mmc_data *data = host->data;

	host->data = NULL;
	host->data_cmd = NULL;

	/*
	 * The controller needs a reset of internal state machines upon
	 * conditions.
	 */
	if (data->error) {
		if (!host->cmd || host->cmd == data_cmd)
			sdhci_sprd_do_reset(host, SDHCI_RESET_CMD);
		sdhci_sprd_do_reset(host, SDHCI_RESET_DATA);
	}

	if ((host->flags & (SDHCI_REQ_USE_DMA | SDHCI_USE_ADMA)) ==
	    (SDHCI_REQ_USE_DMA | SDHCI_USE_ADMA))
		sdhci_sprd_adma_table_post(host, data);

	/*
	 * The specification states that the block count register must
	 * be updated, but it does not specify at what point in the
	 * data flow. That makes the register entirely useless to read
	 * back so we have to assume that nothing made it to the card
	 * in the event of an error.
	 */
	if (data->error)
		data->bytes_xfered = 0;
	else
		data->bytes_xfered = data->blksz * data->blocks;

	/*
	 * Need to send CMD12 if -
	 * a) open-ended multiblock transfer (no CMD23)
	 * b) error in multiblock transfer
	 */
	if (data->stop &&
	    (data->error ||
	     !data->mrq->sbc) && !(host->mmc->card
				&& mmc_card_cmdq(host->mmc->card))) {
		/*
		 * 'cap_cmd_during_tfr' request must not use the command line
		 * after mmc_command_done() has been called. It is upper layer's
		 * responsibility to send the stop command if required.
		 */
		if (data->mrq->cap_cmd_during_tfr) {
			__sdhci_sprd_finish_mrq(host, data->mrq);
		} else {
			/* Avoid triggering warning in sdhci_send_command() */
			host->cmd = NULL;
			if (!sdhci_sprd_send_command(host, data->stop)) {
				if (sw_data_timeout) {
					/*
					 * This is anyway a sw data timeout, so
					 * give up now.
					 */
					data->stop->error = -EIO;
					__sdhci_sprd_finish_mrq(host, data->mrq);
				} else {
					WARN_ON(host->deferred_cmd);
					host->deferred_cmd = data->stop;
				}
			}
		}
	} else {
		__sdhci_sprd_finish_mrq(host, data->mrq);
	}
}
static void sdhci_sprd_finish_data(struct sdhci_host *host)
{
	__sdhci_sprd_finish_data(host, false);
}

static bool _sdhci_sprd_request_done(struct sdhci_host *host)
{
	struct mmc_request *mrq;
	int i;

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		mrq = host->mrqs_done[i];
		if (mrq)
			break;
	}

	if (!mrq)
		return true;

	/*
	 * The controller needs a reset of internal state machines
	 * upon error conditions.
	 */
	if (sdhci_sprd_needs_reset(host, mrq)) {
		/*
		 * Do not finish until command and data lines are available for
		 * reset. Note there can only be one other mrq, so it cannot
		 * also be in mrqs_done, otherwise host->cmd and host->data_cmd
		 * would both be null.
		 */
		if (host->cmd || host->data_cmd) {
			//spin_unlock_irqrestore(&host->lock, flags);
			return true;
		}

		/* Some controllers need this kick or reset won't work here */
		if (host->quirks & SDHCI_QUIRK_CLOCK_BEFORE_RESET)
			/* This is to force an update */
			host->ops->set_clock(host, host->clock);

		/*
		 * Spec says we should do both at the same time, but Ricoh
		 * controllers do not like that.
		 */
		sdhci_sprd_do_reset(host, SDHCI_RESET_CMD);
		sdhci_sprd_do_reset(host, SDHCI_RESET_DATA);

		host->pending_reset = false;
	}

	/*
	 * Always unmap the data buffers if they were mapped by
	 * sdhci_prepare_data() whenever we finish with a request.
	 * This avoids leaking DMA mappings on error.
	 */
	if (host->flags & SDHCI_REQ_USE_DMA) {
		struct mmc_data *data = mrq->data;

		if (data && data->host_cookie == COOKIE_MAPPED) {
			if (host->bounce_buffer) {
				/*
				 * On reads, copy the bounced data into the
				 * sglist
				 */
				if (mmc_get_dma_dir(data) == DMA_FROM_DEVICE) {
					unsigned int length = data->bytes_xfered;

					if (length > host->bounce_buffer_size) {
						pr_err("%s: bounce buffer is %u bytes but DMA claims to have transferred %u bytes\n",
						       mmc_hostname(host->mmc),
						       host->bounce_buffer_size,
						       data->bytes_xfered);
						/* Cap it down and continue */
						length = host->bounce_buffer_size;
					}
					dma_sync_single_for_cpu(
						host->mmc->parent,
						host->bounce_addr,
						host->bounce_buffer_size,
						DMA_FROM_DEVICE);
					sg_copy_from_buffer(data->sg,
						data->sg_len,
						host->bounce_buffer,
						length);
				} else {
					/* No copying, just switch ownership */
					dma_sync_single_for_cpu(
						host->mmc->parent,
						host->bounce_addr,
						host->bounce_buffer_size,
						mmc_get_dma_dir(data));
				}
			} else {
				/* Unmap the raw data */
				dma_unmap_sg(mmc_dev(host->mmc), data->sg,
					     data->sg_len,
					     mmc_get_dma_dir(data));
			}
			data->host_cookie = COOKIE_UNMAPPED;
		}
	}

	host->mrqs_done[i] = NULL;

	//spin_unlock_irqrestore(&host->lock, flags);

	if (host->ops->request_done)
		host->ops->request_done(host, mrq);
	else
		mmc_request_done(host->mmc, mrq);

	return false;
}

static bool raw_sdhci_request_done(struct sdhci_host *host)
{
	unsigned long flags;
	struct mmc_request *mrq;
	int i;

	spin_lock_irqsave(&host->lock, flags);

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		mrq = host->mrqs_done[i];
		if (mrq)
			break;
	}

	if (!mrq) {
		spin_unlock_irqrestore(&host->lock, flags);
		return true;
	}

	/*
	 * The controller needs a reset of internal state machines
	 * upon error conditions.
	 */
	if (sdhci_sprd_needs_reset(host, mrq)) {
		/*
		 * Do not finish until command and data lines are available for
		 * reset. Note there can only be one other mrq, so it cannot
		 * also be in mrqs_done, otherwise host->cmd and host->data_cmd
		 * would both be null.
		 */
		if (host->cmd || host->data_cmd) {
			spin_unlock_irqrestore(&host->lock, flags);
			return true;
		}

		/* Some controllers need this kick or reset won't work here */
		if (host->quirks & SDHCI_QUIRK_CLOCK_BEFORE_RESET)
			/* This is to force an update */
			host->ops->set_clock(host, host->clock);

		/*
		 * Spec says we should do both at the same time, but Ricoh
		 * controllers do not like that.
		 */
		sdhci_sprd_do_reset(host, SDHCI_RESET_CMD);
		sdhci_sprd_do_reset(host, SDHCI_RESET_DATA);

		host->pending_reset = false;
	}

	/*
	 * Always unmap the data buffers if they were mapped by
	 * sdhci_prepare_data() whenever we finish with a request.
	 * This avoids leaking DMA mappings on error.
	 */
	if (host->flags & SDHCI_REQ_USE_DMA) {
		struct mmc_data *data = mrq->data;

		if (data && data->host_cookie == COOKIE_MAPPED) {
			if (host->bounce_buffer) {
				/*
				 * On reads, copy the bounced data into the
				 * sglist
				 */
				if (mmc_get_dma_dir(data) == DMA_FROM_DEVICE) {
					unsigned int length = data->bytes_xfered;

					if (length > host->bounce_buffer_size) {
						pr_err("%s: bounce buffer is %u bytes but DMA claims to have transferred %u bytes\n",
						       mmc_hostname(host->mmc),
						       host->bounce_buffer_size,
						       data->bytes_xfered);
						/* Cap it down and continue */
						length = host->bounce_buffer_size;
					}
					dma_sync_single_for_cpu(
						host->mmc->parent,
						host->bounce_addr,
						host->bounce_buffer_size,
						DMA_FROM_DEVICE);
					sg_copy_from_buffer(data->sg,
						data->sg_len,
						host->bounce_buffer,
						length);
				} else {
					/* No copying, just switch ownership */
					dma_sync_single_for_cpu(
						host->mmc->parent,
						host->bounce_addr,
						host->bounce_buffer_size,
						mmc_get_dma_dir(data));
				}
			} else {
				/* Unmap the raw data */
				dma_unmap_sg(mmc_dev(host->mmc), data->sg,
					     data->sg_len,
					     mmc_get_dma_dir(data));
			}
			data->host_cookie = COOKIE_UNMAPPED;
		}
	}

	host->mrqs_done[i] = NULL;

	spin_unlock_irqrestore(&host->lock, flags);

	if (host->ops->request_done)
		host->ops->request_done(host, mrq);
	else
		mmc_request_done(host->mmc, mrq);

	return false;
}

/*****************************************************************************\
 *                                                                           *
 * Interrupt handling                                                        *
 *                                                                           *
\*****************************************************************************/

static void sdhci_sprd_cmd_irq(struct sdhci_host *host, u32 intmask, u32 *intmask_p)
{
	/* Handle auto-CMD12 error */
	if (intmask & SDHCI_INT_AUTO_CMD_ERR && host->data_cmd) {
		struct mmc_request *mrq = host->data_cmd->mrq;
		u16 auto_cmd_status = sdhci_readw(host, SDHCI_AUTO_CMD_STATUS);
		int data_err_bit = (auto_cmd_status & SDHCI_AUTO_CMD_TIMEOUT) ?
				   SDHCI_INT_DATA_TIMEOUT :
				   SDHCI_INT_DATA_CRC;

		/* Treat auto-CMD12 error the same as data error */
		if (!mrq->sbc && (host->flags & SDHCI_AUTO_CMD12)) {
			*intmask_p |= data_err_bit;
			return;
		}
	}

	if (!host->cmd) {
		/*
		 * SDHCI recovers from errors by resetting the cmd and data
		 * circuits.  Until that is done, there very well might be more
		 * interrupts, so ignore them in that case.
		 */
		if (host->pending_reset)
			return;
		pr_err("%s: Got command interrupt 0x%08x even though no command operation was in progress.\n",
		       mmc_hostname(host->mmc), (unsigned int)intmask);
		sdhci_sprd_dumpregs(host);
		return;
	}

	if (intmask & (SDHCI_INT_TIMEOUT | SDHCI_INT_CRC |
		       SDHCI_INT_END_BIT | SDHCI_INT_INDEX)) {
		if (intmask & SDHCI_INT_TIMEOUT)
			host->cmd->error = -ETIMEDOUT;
		else
			host->cmd->error = -EILSEQ;

		/* Treat data command CRC error the same as data CRC error */
		if (host->cmd->data &&
		    (intmask & (SDHCI_INT_CRC | SDHCI_INT_TIMEOUT)) ==
		     SDHCI_INT_CRC) {
			host->cmd = NULL;
			*intmask_p |= SDHCI_INT_DATA_CRC;
			return;
		}

		__sdhci_sprd_finish_mrq(host, host->cmd->mrq);
		return;
	}

	/* Handle auto-CMD23 error */
	if (intmask & SDHCI_INT_AUTO_CMD_ERR) {
		struct mmc_request *mrq = host->cmd->mrq;
		u16 auto_cmd_status = sdhci_readw(host, SDHCI_AUTO_CMD_STATUS);
		int err = (auto_cmd_status & SDHCI_AUTO_CMD_TIMEOUT) ?
			  -ETIMEDOUT :
			  -EILSEQ;

		if (mrq->sbc && (host->flags & SDHCI_AUTO_CMD23)) {
			mrq->sbc->error = err;
			__sdhci_sprd_finish_mrq(host, mrq);
			return;
		}
	}

	if (intmask & SDHCI_INT_RESPONSE)
		sdhci_sprd_finish_command(host);
}

static void sdhci_sprd_adma_show_error(struct sdhci_host *host)
{
	void *desc = host->adma_table;
	dma_addr_t dma = host->adma_addr;

	sdhci_sprd_dumpregs(host);

	while (true) {
		struct sdhci_adma2_64_desc *dma_desc = desc;

		if (host->flags & SDHCI_USE_64_BIT_DMA)
			SDHCI_DUMP("%08llx: DMA 0x%08x%08x, LEN 0x%04x, Attr=0x%02x\n",
			    (unsigned long long)dma,
			    le32_to_cpu(dma_desc->addr_hi),
			    le32_to_cpu(dma_desc->addr_lo),
			    le16_to_cpu(dma_desc->len),
			    le16_to_cpu(dma_desc->cmd));
		else
			SDHCI_DUMP("%08llx: DMA 0x%08x, LEN 0x%04x, Attr=0x%02x\n",
			    (unsigned long long)dma,
			    le32_to_cpu(dma_desc->addr_lo),
			    le16_to_cpu(dma_desc->len),
			    le16_to_cpu(dma_desc->cmd));

		desc += host->desc_sz;
		dma += host->desc_sz;

		if (dma_desc->cmd & cpu_to_le16(ADMA2_END))
			break;
	}
}

static void sdhci_sprd_data_irq(struct sdhci_host *host, u32 intmask)
{
	u32 command;

	/* CMD19 generates _only_ Buffer Read Ready interrupt */
	if (intmask & SDHCI_INT_DATA_AVAIL) {
		command = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
		if (command == MMC_SEND_TUNING_BLOCK ||
		    command == MMC_SEND_TUNING_BLOCK_HS200) {
			host->tuning_done = 1;
			wake_up(&host->buf_ready_int);
			return;
		}
	}

	if (!host->data) {
		struct mmc_command *data_cmd = host->data_cmd;

		/*
		 * The "data complete" interrupt is also used to
		 * indicate that a busy state has ended. See comment
		 * above in sdhci_sprd_cmd_irq().
		 */
		if (data_cmd && (data_cmd->flags & MMC_RSP_BUSY)) {
			if (intmask & SDHCI_INT_DATA_TIMEOUT) {
				host->data_cmd = NULL;
				data_cmd->error = -ETIMEDOUT;
				__sdhci_sprd_finish_mrq(host, data_cmd->mrq);
				return;
			}
			if (intmask & SDHCI_INT_DATA_END) {
				host->data_cmd = NULL;
				/*
				 * Some cards handle busy-end interrupt
				 * before the command completed, so make
				 * sure we do things in the proper order.
				 */
				if (host->cmd == data_cmd)
					return;

				__sdhci_sprd_finish_mrq(host, data_cmd->mrq);
				return;
			}
		}

		/*
		 * SDHCI recovers from errors by resetting the cmd and data
		 * circuits. Until that is done, there very well might be more
		 * interrupts, so ignore them in that case.
		 */
		if (host->pending_reset)
			return;

		pr_err("%s: Got data interrupt 0x%08x even though no data operation was in progress.\n",
		       mmc_hostname(host->mmc), (unsigned int)intmask);
		sdhci_sprd_dumpregs(host);

		return;
	}

	if (intmask & SDHCI_INT_DATA_TIMEOUT)
		host->data->error = -ETIMEDOUT;
	else if (intmask & SDHCI_INT_DATA_END_BIT)
		host->data->error = -EILSEQ;
	else if ((intmask & SDHCI_INT_DATA_CRC) &&
		SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND))
			!= MMC_BUS_TEST_R)
		host->data->error = -EILSEQ;
	else if (intmask & SDHCI_INT_ADMA_ERROR) {
		pr_err("%s: ADMA error: 0x%08x\n", mmc_hostname(host->mmc),
		       intmask);
		sdhci_sprd_adma_show_error(host);
		host->data->error = -EIO;
		if (host->ops->adma_workaround)
			host->ops->adma_workaround(host, intmask);
	}

	if (host->data->error)
		sdhci_sprd_finish_data(host);
	else {
		if (intmask & (SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL))
			sdhci_sprd_transfer_pio(host);

		/*
		 * We currently don't do anything fancy with DMA
		 * boundaries, but as we can't disable the feature
		 * we need to at least restart the transfer.
		 *
		 * According to the spec sdhci_readl(host, SDHCI_DMA_ADDRESS)
		 * should return a valid address to continue from, but as
		 * some controllers are faulty, don't trust them.
		 */
		if (intmask & SDHCI_INT_DMA_END) {
			dma_addr_t dmastart, dmanow;

			dmastart = sdhci_sprd_sdma_address(host);
			dmanow = dmastart + host->data->bytes_xfered;
			/*
			 * Force update to the next DMA block boundary.
			 */
			dmanow = (dmanow &
				~((dma_addr_t)SDHCI_DEFAULT_BOUNDARY_SIZE - 1)) +
				SDHCI_DEFAULT_BOUNDARY_SIZE;
			host->data->bytes_xfered = dmanow - dmastart;
			DBG("DMA base %pad, transferred 0x%06x bytes, next %pad\n",
			    &dmastart, host->data->bytes_xfered, &dmanow);
			sdhci_sprd_set_sdma_addr(host, dmanow);
		}

		if (intmask & SDHCI_INT_DATA_END) {
			if (host->cmd == host->data_cmd) {
				/*
				 * Data managed to finish before the
				 * command completed. Make sure we do
				 * things in the proper order.
				 */
				host->data_early = 1;
			} else {
				sdhci_sprd_finish_data(host);
			}
		}
	}
}

static inline bool sdhci_sprd_defer_done(struct sdhci_host *host,
				    struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	return host->pending_reset || host->always_defer_done ||
	       ((host->flags & SDHCI_REQ_USE_DMA) && data &&
		data->host_cookie == COOKIE_MAPPED);
}

static irqreturn_t sdhci_sprd_thread_irq(int irq, void *dev_id)
{
	struct sdhci_host *host = dev_id;
	struct mmc_command *cmd;
	u32 isr;

	while (!_sdhci_sprd_request_done(host))
		;

	isr = host->thread_isr;
	host->thread_isr = 0;

	cmd = host->deferred_cmd;
	if (cmd && !sdhci_sprd_send_command(host, cmd))
		sdhci_sprd_finish_mrq(host, cmd->mrq);



	if (isr & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) {
		struct mmc_host *mmc = host->mmc;

		mmc->ops->card_event(mmc);
		mmc_detect_change(mmc, msecs_to_jiffies(200));
	}

	return IRQ_HANDLED;
}

static bool sdhci_sprd_present_error(struct sdhci_host *host,
				struct mmc_command *cmd, bool present)
{
	if (!present || host->flags & SDHCI_DEVICE_DEAD) {
		cmd->error = -ENOMEDIUM;
		return true;
	}

	return false;
}


static bool sdhci_sprd_send_command_retry(struct sdhci_host *host,
				     struct mmc_command *cmd,
				     unsigned long flags)
	__releases(host->lock)
	__acquires(host->lock)
{
	struct mmc_command *deferred_cmd = host->deferred_cmd;
	int timeout = 10; /* Approx. 10 ms */
	bool present;

	while (!sdhci_sprd_send_command(host, cmd)) {
		if (!timeout--) {
			pr_err("%s: Controller never released inhibit bit(s).\n",
			       mmc_hostname(host->mmc));
			sdhci_sprd_dumpregs(host);
			cmd->error = -EIO;
			return false;
		}

		spin_unlock_irqrestore(&host->lock, flags);

		usleep_range_state(1000, 1250, TASK_UNINTERRUPTIBLE);

		present = host->mmc->ops->get_cd(host->mmc);

		spin_lock_irqsave(&host->lock, flags);

		/* A deferred command might disappear, handle that */
		if (cmd == deferred_cmd && cmd != host->deferred_cmd)
			return true;

		if (sdhci_sprd_present_error(host, cmd, present))
			return false;
	}

	if (cmd == host->deferred_cmd)
		host->deferred_cmd = NULL;

	return true;
}

static irqreturn_t raw_sdhci_thread_irq(int irq, void *dev_id)
{
	struct sdhci_host *host = dev_id;
	struct mmc_command *cmd;
	unsigned long flags;
	u32 isr;

	while (!raw_sdhci_request_done(host))
		;

	spin_lock_irqsave(&host->lock, flags);

	isr = host->thread_isr;
	host->thread_isr = 0;

	cmd = host->deferred_cmd;
	if (cmd && !sdhci_sprd_send_command_retry(host, cmd, flags))
		sdhci_sprd_finish_mrq(host, cmd->mrq);

	spin_unlock_irqrestore(&host->lock, flags);

	if (isr & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) {
		struct mmc_host *mmc = host->mmc;

		mmc->ops->card_event(mmc);
		mmc_detect_change(mmc, msecs_to_jiffies(200));
	}

	return IRQ_HANDLED;
}

static irqreturn_t sdhci_sprd_irq(int irq, void *dev_id)
{
	struct mmc_request *mrqs_done[SDHCI_MAX_MRQS] = {0};
	irqreturn_t result = IRQ_NONE;
	struct sdhci_host *host = dev_id;
	struct mmc_host *mmc = host->mmc;
	struct mmc_command *cmd;
	struct mmc_swcq *swcq = mmc->cqe_private;
	//struct mmc_command *cmd = host->cmd;
	u32 intmask, mask, unexpected = 0;
	int max_loops = 16;
	int i;

	cmd = host->cmd;
	if (cmd && cmd->opcode != MMC_QUE_TASK_ADDR)
		spin_lock(&host->lock);

	if (host->runtime_suspended) {
		if (cmd && cmd->opcode != MMC_QUE_TASK_ADDR)
			spin_unlock(&host->lock);
		return IRQ_NONE;
	}

	intmask = sdhci_readl(host, SDHCI_INT_STATUS);
	if (!intmask || intmask == 0xffffffff) {
		result = IRQ_NONE;
		goto out;
	}

	do {
		DBG("IRQ status 0x%08x comm=%s\n", intmask, current->comm);

		if (host->ops->irq) {
			intmask = host->ops->irq(host, intmask);
			if (!intmask)
				goto cont;
		}

		/* Clear selected interrupts. */
		mask = intmask & (SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK |
				  SDHCI_INT_BUS_POWER);
		sdhci_writel(host, mask, SDHCI_INT_STATUS);

		if (intmask & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) {
			u32 present = sdhci_readl(host, SDHCI_PRESENT_STATE) &
				      SDHCI_CARD_PRESENT;

			/*
			 * There is a observation on i.mx esdhc.  INSERT
			 * bit will be immediately set again when it gets
			 * cleared, if a card is inserted.  We have to mask
			 * the irq to prevent interrupt storm which will
			 * freeze the system.  And the REMOVE gets the
			 * same situation.
			 *
			 * More testing are needed here to ensure it works
			 * for other platforms though.
			 */
			host->ier &= ~(SDHCI_INT_CARD_INSERT |
				       SDHCI_INT_CARD_REMOVE);
			host->ier |= present ? SDHCI_INT_CARD_REMOVE :
					       SDHCI_INT_CARD_INSERT;
			sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
			sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);

			sdhci_writel(host, intmask & (SDHCI_INT_CARD_INSERT |
				     SDHCI_INT_CARD_REMOVE), SDHCI_INT_STATUS);

			host->thread_isr |= intmask & (SDHCI_INT_CARD_INSERT |
						       SDHCI_INT_CARD_REMOVE);
			result = IRQ_WAKE_THREAD;
		}

		if (intmask & SDHCI_INT_CMD_MASK)
			sdhci_sprd_cmd_irq(host, intmask & SDHCI_INT_CMD_MASK, &intmask);

		if (intmask & SDHCI_INT_DATA_MASK)
			sdhci_sprd_data_irq(host, intmask & SDHCI_INT_DATA_MASK);

		if (sdhci_sprd_mmc_debug_judge())
			mmc_debug_update(host, NULL, intmask);

		if (intmask & SDHCI_INT_BUS_POWER)
			pr_err("%s: Card is consuming too much power!\n",
				mmc_hostname(host->mmc));

		if (intmask & SDHCI_INT_RETUNE)
			mmc_retune_needed(host->mmc);

		intmask &= ~(SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE |
			     SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK |
			     SDHCI_INT_ERROR | SDHCI_INT_BUS_POWER |
			     SDHCI_INT_RETUNE | SDHCI_INT_CARD_INT);

		if (intmask) {
			unexpected |= intmask;
			sdhci_writel(host, intmask, SDHCI_INT_STATUS);
		}
cont:
		if (result == IRQ_NONE)
			result = IRQ_HANDLED;

		intmask = sdhci_readl(host, SDHCI_INT_STATUS);
	} while (intmask && --max_loops);

	/* Determine if mrqs can be completed immediately */
	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		struct mmc_request *mrq = host->mrqs_done[i];

		if (!mrq)
			continue;

		if (sdhci_sprd_defer_done(host, mrq)) {
			if (swcq->need_polling) {
				sdhci_sprd_thread_irq(0, host);
				result = IRQ_HANDLED;
			} else
				result = IRQ_WAKE_THREAD;
		} else {
			mrqs_done[i] = mrq;
			host->mrqs_done[i] = NULL;
		}
	}
out:
	if (host->deferred_cmd)
		result = IRQ_WAKE_THREAD;
	if (cmd && cmd->opcode != MMC_QUE_TASK_ADDR)
		spin_unlock(&host->lock);

	/* Process mrqs ready for immediate completion */
	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (!mrqs_done[i])
			continue;

		if (host->ops->request_done)
			host->ops->request_done(host, mrqs_done[i]);
		else
			mmc_request_done(host->mmc, mrqs_done[i]);
	}

	if (unexpected) {
		pr_err("%s: Unexpected interrupt 0x%08x.\n",
			   mmc_hostname(host->mmc), unexpected);
		sdhci_sprd_dumpregs(host);
	}

	return result;
}

static irqreturn_t raw_sdhci_irq(int irq, void *dev_id)
{
	struct mmc_request *mrqs_done[SDHCI_MAX_MRQS] = {0};
	irqreturn_t result = IRQ_NONE;
	struct sdhci_host *host = dev_id;
	u32 intmask, mask, unexpected = 0;
	int max_loops = 16;
	int i;

	spin_lock(&host->lock);

	if (host->runtime_suspended) {
		spin_unlock(&host->lock);
		return IRQ_NONE;
	}

	intmask = sdhci_readl(host, SDHCI_INT_STATUS);
	if (!intmask || intmask == 0xffffffff) {
		result = IRQ_NONE;
		goto out;
	}

	do {
		DBG("IRQ status 0x%08x\n", intmask);

		if (host->ops->irq) {
			intmask = host->ops->irq(host, intmask);
			if (!intmask)
				goto cont;
		}

		/* Clear selected interrupts. */
		mask = intmask & (SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK |
				  SDHCI_INT_BUS_POWER);
		sdhci_writel(host, mask, SDHCI_INT_STATUS);

		if (intmask & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) {
			u32 present = sdhci_readl(host, SDHCI_PRESENT_STATE) &
				      SDHCI_CARD_PRESENT;

			/*
			 * There is a observation on i.mx esdhc.  INSERT
			 * bit will be immediately set again when it gets
			 * cleared, if a card is inserted.  We have to mask
			 * the irq to prevent interrupt storm which will
			 * freeze the system.  And the REMOVE gets the
			 * same situation.
			 *
			 * More testing are needed here to ensure it works
			 * for other platforms though.
			 */
			host->ier &= ~(SDHCI_INT_CARD_INSERT |
				       SDHCI_INT_CARD_REMOVE);
			host->ier |= present ? SDHCI_INT_CARD_REMOVE :
					       SDHCI_INT_CARD_INSERT;
			sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
			sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);

			sdhci_writel(host, intmask & (SDHCI_INT_CARD_INSERT |
				     SDHCI_INT_CARD_REMOVE), SDHCI_INT_STATUS);

			host->thread_isr |= intmask & (SDHCI_INT_CARD_INSERT |
						       SDHCI_INT_CARD_REMOVE);
			result = IRQ_WAKE_THREAD;
		}

		if (intmask & SDHCI_INT_CMD_MASK)
			sdhci_sprd_cmd_irq(host, intmask & SDHCI_INT_CMD_MASK, &intmask);

		if (intmask & SDHCI_INT_DATA_MASK)
			sdhci_sprd_data_irq(host, intmask & SDHCI_INT_DATA_MASK);

		if (sdhci_sprd_mmc_debug_judge())
			mmc_debug_update(host, NULL, intmask);

		if (intmask & SDHCI_INT_BUS_POWER)
			pr_err("%s: Card is consuming too much power!\n",
				mmc_hostname(host->mmc));

		if (intmask & SDHCI_INT_RETUNE)
			mmc_retune_needed(host->mmc);

		intmask &= ~(SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE |
			     SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK |
			     SDHCI_INT_ERROR | SDHCI_INT_BUS_POWER |
			     SDHCI_INT_RETUNE | SDHCI_INT_CARD_INT);

		if (intmask) {
			unexpected |= intmask;
			sdhci_writel(host, intmask, SDHCI_INT_STATUS);
		}
cont:
		if (result == IRQ_NONE)
			result = IRQ_HANDLED;

		intmask = sdhci_readl(host, SDHCI_INT_STATUS);
	} while (intmask && --max_loops);

	/* Determine if mrqs can be completed immediately */
	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		struct mmc_request *mrq = host->mrqs_done[i];

		if (!mrq)
			continue;

		if (sdhci_sprd_defer_done(host, mrq)) {
			result = IRQ_WAKE_THREAD;
		} else {
			mrqs_done[i] = mrq;
			host->mrqs_done[i] = NULL;
		}
	}
out:
	if (host->deferred_cmd)
		result = IRQ_WAKE_THREAD;

	spin_unlock(&host->lock);

	/* Process mrqs ready for immediate completion */
	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (!mrqs_done[i])
			continue;

		if (host->ops->request_done)
			host->ops->request_done(host, mrqs_done[i]);
		else
			mmc_request_done(host->mmc, mrqs_done[i]);
	}

	if (unexpected) {
		pr_err("%s: Unexpected interrupt 0x%08x.\n",
			   mmc_hostname(host->mmc), unexpected);
		sdhci_sprd_dumpregs(host);
	}

	return result;
}


static unsigned int sdhci_sprd_target_timeout(struct sdhci_host *host,
					 struct mmc_command *cmd,
					 struct mmc_data *data)
{
	unsigned int target_timeout;

	/* timeout in us */
	if (!data) {
		target_timeout = cmd->busy_timeout * 1000;
	} else {
		target_timeout = DIV_ROUND_UP(data->timeout_ns, 1000);
		if (host->clock && data->timeout_clks) {
			unsigned long long val;

			/*
			 * data->timeout_clks is in units of clock cycles.
			 * host->clock is in Hz.  target_timeout is in us.
			 * Hence, us = 1000000 * cycles / Hz.  Round up.
			 */
			val = 1000000ULL * data->timeout_clks;
			if (do_div(val, host->clock))
				target_timeout++;
			target_timeout += val;
		}
	}

	return target_timeout;
}

static void sdhci_sprd_calc_sw_timeout(struct sdhci_host *host,
				  struct mmc_command *cmd)
{
	struct mmc_data *data = cmd->data;
	struct mmc_host *mmc = host->mmc;
	struct mmc_ios *ios = &mmc->ios;
	unsigned char bus_width = 1 << ios->bus_width;
	unsigned int blksz;
	unsigned int freq;
	u64 target_timeout;
	u64 transfer_time;

	target_timeout = sdhci_sprd_target_timeout(host, cmd, data);
	target_timeout *= NSEC_PER_USEC;

	if (data) {
		blksz = data->blksz;
		freq = host->mmc->actual_clock ? : host->clock;
		transfer_time = (u64)blksz * NSEC_PER_SEC * (8 / bus_width);
		do_div(transfer_time, freq);
		/* multiply by '2' to account for any unknowns */
		transfer_time = transfer_time * 2;
		/* calculate timeout for the entire data */
		host->data_timeout = data->blocks * target_timeout +
				     transfer_time;
	} else {
		host->data_timeout = target_timeout;
	}

	if (host->data_timeout)
		host->data_timeout += MMC_CMD_TRANSFER_TIME;
}

static u8 sdhci_sprd_calc_timeout(struct sdhci_host *host, struct mmc_command *cmd,
			     bool *too_big)
{
	u8 count;
	struct mmc_data *data;
	unsigned int target_timeout, current_timeout;

	*too_big = true;

	/*
	 * If the host controller provides us with an incorrect timeout
	 * value, just skip the check and use 0xE.  The hardware may take
	 * longer to time out, but that's much better than having a too-short
	 * timeout value.
	 */
	if (host->quirks & SDHCI_QUIRK_BROKEN_TIMEOUT_VAL)
		return 0xE;

	/* Unspecified command, asume max */
	if (cmd == NULL)
		return 0xE;

	data = cmd->data;
	/* Unspecified timeout, assume max */
	if (!data && !cmd->busy_timeout)
		return 0xE;

	/* timeout in us */
	target_timeout = sdhci_sprd_target_timeout(host, cmd, data);

	/*
	 * Figure out needed cycles.
	 * We do this in steps in order to fit inside a 32 bit int.
	 * The first step is the minimum timeout, which will have a
	 * minimum resolution of 6 bits:
	 * (1) 2^13*1000 > 2^22,
	 * (2) host->timeout_clk < 2^16
	 *     =>
	 *     (1) / (2) > 2^6
	 */
	count = 0;
	current_timeout = (1 << 13) * 1000 / host->timeout_clk;
	while (current_timeout < target_timeout) {
		count++;
		current_timeout <<= 1;
		if (count >= 0xF)
			break;
	}

	if (count >= 0xF) {
		if (!(host->quirks2 & SDHCI_QUIRK2_DISABLE_HW_TIMEOUT))
			DBG("Too large timeout 0x%x requested for CMD%d!\n",
			    count, cmd->opcode);
		count = 0xE;
	} else {
		*too_big = false;
	}

	return count;
}

static void sdhci_sprd_set_transfer_irqs(struct sdhci_host *host)
{
	u32 pio_irqs = SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL;
	u32 dma_irqs = SDHCI_INT_DMA_END | SDHCI_INT_ADMA_ERROR;
	struct mmc_host *mmc = host->mmc;
	struct mmc_swcq *swcq = mmc->cqe_private;

	if (host->flags & SDHCI_REQ_USE_DMA)
		host->ier = (host->ier & ~pio_irqs) | dma_irqs;
	else
		host->ier = (host->ier & ~dma_irqs) | pio_irqs;

	if (host->flags & (SDHCI_AUTO_CMD23 | SDHCI_AUTO_CMD12))
		host->ier |= SDHCI_INT_AUTO_CMD_ERR;
	else
		host->ier &= ~SDHCI_INT_AUTO_CMD_ERR;

	sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
	if (swcq->need_polling)
		sdhci_writel(host, 0, SDHCI_SIGNAL_ENABLE);
	else
		sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
}

static void sdhci_sprd_set_data_timeout_irq(struct sdhci_host *host, bool enable)
{
	if (enable)
		host->ier |= SDHCI_INT_DATA_TIMEOUT;
	else
		host->ier &= ~SDHCI_INT_DATA_TIMEOUT;
	sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
	sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
}

static void __sdhci_sprd_set_timeout(struct sdhci_host *host, struct mmc_command *cmd)
{
	bool too_big = false;
	u8 count = sdhci_sprd_calc_timeout(host, cmd, &too_big);

	if (too_big &&
	    host->quirks2 & SDHCI_QUIRK2_DISABLE_HW_TIMEOUT) {
		sdhci_sprd_calc_sw_timeout(host, cmd);
		sdhci_sprd_set_data_timeout_irq(host, false);
	} else if (!(host->ier & SDHCI_INT_DATA_TIMEOUT)) {
		sdhci_sprd_set_data_timeout_irq(host, true);
	}

	sdhci_writeb(host, count, SDHCI_TIMEOUT_CONTROL);
}

static void sdhci_sprd_set_timeout(struct sdhci_host *host, struct mmc_command *cmd)
{
	if (host->ops->set_timeout)
		host->ops->set_timeout(host, cmd);
	else
		__sdhci_sprd_set_timeout(host, cmd);
}

static void sdhci_sprd_config_dma(struct sdhci_host *host)
{
	u8 ctrl;
	u16 ctrl2;

	if (host->version < SDHCI_SPEC_200)
		return;

	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);

	/*
	 * Always adjust the DMA selection as some controllers
	 * (e.g. JMicron) can't do PIO properly when the selection
	 * is ADMA.
	 */
	ctrl &= ~SDHCI_CTRL_DMA_MASK;
	if (!(host->flags & SDHCI_REQ_USE_DMA))
		goto out;

	/* Note if DMA Select is zero then SDMA is selected */
	if (host->flags & SDHCI_USE_ADMA)
		ctrl |= SDHCI_CTRL_ADMA32;

	if (host->flags & SDHCI_USE_64_BIT_DMA) {
		/*
		 * If v4 mode, all supported DMA can be 64-bit addressing if
		 * controller supports 64-bit system address, otherwise only
		 * ADMA can support 64-bit addressing.
		 */
		if (host->v4_mode) {
			ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
			ctrl2 |= SDHCI_CTRL_64BIT_ADDR;
			sdhci_writew(host, ctrl2, SDHCI_HOST_CONTROL2);
		} else if (host->flags & SDHCI_USE_ADMA) {
			/*
			 * Don't need to undo SDHCI_CTRL_ADMA32 in order to
			 * set SDHCI_CTRL_ADMA64.
			 */
			ctrl |= SDHCI_CTRL_ADMA64;
		}
	}

out:
	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
}

static void sdhci_sprd_prepare_data(struct sdhci_host *host, struct mmc_command *cmd)
{
	struct mmc_data *data = cmd->data;

	host->data_timeout = 0;

	if (sdhci_sprd_data_line_cmd(cmd))
		sdhci_sprd_set_timeout(host, cmd);

	if (!data) {
		sdhci_sprd_set_transfer_irqs(host);
		return;
	}
	WARN_ON(host->data);

	/* Sanity checks */
	BUG_ON(data->blksz * data->blocks > 524288);
	BUG_ON(data->blksz > host->mmc->max_blk_size);
	BUG_ON(data->blocks > 65535);

	host->data = data;
	host->data_early = 0;
	host->data->bytes_xfered = 0;

	if (host->flags & (SDHCI_USE_SDMA | SDHCI_USE_ADMA)) {
		struct scatterlist *sg;
		unsigned int length_mask, offset_mask;
		int i;

		host->flags |= SDHCI_REQ_USE_DMA;

		/*
		 * FIXME: This doesn't account for merging when mapping the
		 * scatterlist.
		 *
		 * The assumption here being that alignment and lengths are
		 * the same after DMA mapping to device address space.
		 */
		length_mask = 0;
		offset_mask = 0;
		if (host->flags & SDHCI_USE_ADMA) {
			if (host->quirks & SDHCI_QUIRK_32BIT_ADMA_SIZE) {
				length_mask = 3;
				/*
				 * As we use up to 3 byte chunks to work
				 * around alignment problems, we need to
				 * check the offset as well.
				 */
				offset_mask = 3;
			}
		} else {
			if (host->quirks & SDHCI_QUIRK_32BIT_DMA_SIZE)
				length_mask = 3;
			if (host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR)
				offset_mask = 3;
		}

		if (unlikely(length_mask | offset_mask)) {
			for_each_sg(data->sg, sg, data->sg_len, i) {
				if (sg->length & length_mask) {
					DBG("Reverting to PIO because of transfer size (%d)\n",
					    sg->length);
					host->flags &= ~SDHCI_REQ_USE_DMA;
					break;
				}
				if (sg->offset & offset_mask) {
					DBG("Reverting to PIO because of bad alignment\n");
					host->flags &= ~SDHCI_REQ_USE_DMA;
					break;
				}
			}
		}
	}

	if (host->flags & SDHCI_REQ_USE_DMA) {
		int sg_cnt = sdhci_sprd_pre_dma_transfer(host, data, COOKIE_MAPPED);

		if (sg_cnt <= 0) {
			/*
			 * This only happens when someone fed
			 * us an invalid request.
			 */
			WARN_ON(1);
			host->flags &= ~SDHCI_REQ_USE_DMA;
		} else if (host->flags & SDHCI_USE_ADMA) {
			sdhci_sprd_adma_table_pre(host, data, sg_cnt);
			sdhci_sprd_set_adma_addr(host, host->adma_addr);
		} else {
			WARN_ON(sg_cnt != 1);
			sdhci_sprd_set_sdma_addr(host, sdhci_sprd_sdma_address(host));
		}
	}

	sdhci_sprd_config_dma(host);

	if (!(host->flags & SDHCI_REQ_USE_DMA)) {
		int flags;

		flags = SG_MITER_ATOMIC;
		if (host->data->flags & MMC_DATA_READ)
			flags |= SG_MITER_TO_SG;
		else
			flags |= SG_MITER_FROM_SG;
		sg_miter_start(&host->sg_miter, data->sg, data->sg_len, flags);
		host->blocks = data->blocks;
	}

	sdhci_sprd_set_transfer_irqs(host);

	/* Set the DMA boundary value and block size */
	sdhci_writew(host, SDHCI_MAKE_BLKSZ(host->sdma_boundary, data->blksz),
		     SDHCI_BLOCK_SIZE);

	/*
	 * For Version 4.10 onwards, if v4 mode is enabled, 32-bit Block Count
	 * can be supported, in that case 16-bit block count register must be 0.
	 */
	if (host->version >= SDHCI_SPEC_410 && host->v4_mode &&
	    (host->quirks2 & SDHCI_QUIRK2_USE_32BIT_BLK_CNT)) {
		if (sdhci_readw(host, SDHCI_BLOCK_COUNT))
			sdhci_writew(host, 0, SDHCI_BLOCK_COUNT);
		sdhci_writew(host, data->blocks, SDHCI_32BIT_BLK_CNT);
	} else {
		sdhci_writew(host, data->blocks, SDHCI_BLOCK_COUNT);
	}
}

static bool sdhci_sprd_send_command(struct sdhci_host *host, struct mmc_command *cmd)
{
	int flags;
	u32 mask;
	unsigned long timeout;
	struct mmc_data *data = cmd->data;
	struct mmc_host *mmc = host->mmc;
	struct mmc_swcq *swcq = mmc->cqe_private;
	u32 intmask;

#ifdef CONFIG_SPRD_DEBUG
	if (host->cmd) {
		pr_err("%s: host->cmd%d\n", mmc_hostname(host->mmc), host->cmd->opcode);

		if (cmd)
			pr_err("%s: now cmd%d\n", mmc_hostname(host->mmc), cmd->opcode);
	}
#endif

	WARN_ON(host->cmd);

	dbg_add_host_log(host->mmc, MMC_SEND_CMD, cmd->opcode, cmd->arg, cmd->mrq);

	/* Initially, a command has no error */
	cmd->error = 0;

	if ((host->quirks2 & SDHCI_QUIRK2_STOP_WITH_TC) &&
	    cmd->opcode == MMC_STOP_TRANSMISSION)
		cmd->flags |= MMC_RSP_BUSY;

	mask = SDHCI_CMD_INHIBIT;
	if (sdhci_sprd_data_line_cmd(cmd))
		mask |= SDHCI_DATA_INHIBIT;

	/* We shouldn't wait for data inihibit for stop commands, even
	 * though they might use busy signaling
	 */
	if (cmd->mrq->data && (cmd == cmd->mrq->data->stop))
		mask &= ~SDHCI_DATA_INHIBIT;

	if (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask)
		return false;

	host->cmd = cmd;
	if (sdhci_sprd_data_line_cmd(cmd)) {
		WARN_ON(host->data_cmd);
		host->data_cmd = cmd;
	}

	if (host->mmc->card
	  && mmc_card_cmdq(host->mmc->card)
	  && (cmd->opcode == MMC_SEND_STATUS
	  || cmd->opcode == MMC_CMDQ_TASK_MGMT
	  || cmd->opcode == MMC_STOP_TRANSMISSION
	  || cmd->opcode == MMC_QUE_TASK_PARAMS
	  || cmd->opcode == MMC_QUE_TASK_ADDR
	  || cmd->opcode == MMC_EXECUTE_READ_TASK
	  || cmd->opcode == MMC_EXECUTE_WRITE_TASK)) {
		swcq->need_polling = true;
		swcq->need_intr = false;
	} else
		swcq->need_polling = false;

	sdhci_sprd_prepare_data(host, cmd);

	sdhci_writel(host, cmd->arg, SDHCI_ARGUMENT);

	sdhci_sprd_set_transfer_mode(host, cmd);

	if ((cmd->flags & MMC_RSP_136) && (cmd->flags & MMC_RSP_BUSY)) {
		WARN_ONCE(1, "Unsupported response type!\n");
		/*
		 * This does not happen in practice because 136-bit response
		 * commands never have busy waiting, so rather than complicate
		 * the error path, just remove busy waiting and continue.
		 */
		cmd->flags &= ~MMC_RSP_BUSY;
	}

	if (!(cmd->flags & MMC_RSP_PRESENT))
		flags = SDHCI_CMD_RESP_NONE;
	else if (cmd->flags & MMC_RSP_136)
		flags = SDHCI_CMD_RESP_LONG;
	else if (cmd->flags & MMC_RSP_BUSY)
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
	else
		flags = SDHCI_CMD_RESP_SHORT;

	if (cmd->flags & MMC_RSP_CRC)
		flags |= SDHCI_CMD_CRC;
	if (cmd->flags & MMC_RSP_OPCODE)
		flags |= SDHCI_CMD_INDEX;

	/* CMD19 is special in that the Data Present Select should be set */
	if (cmd->data || cmd->opcode == MMC_SEND_TUNING_BLOCK ||
	    cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200)
		flags |= SDHCI_CMD_DATA;

	timeout = jiffies;
	if (host->data_timeout)
		timeout += nsecs_to_jiffies(host->data_timeout);
	else if (!cmd->data && cmd->busy_timeout > 9000)
		timeout += DIV_ROUND_UP(cmd->busy_timeout, 1000) * HZ + HZ;
	else
		timeout += 4 * HZ;
	sdhci_sprd_mod_timer(host, cmd->mrq, timeout);

	if (sdhci_sprd_mmc_debug_judge())
		mmc_debug_update(host, cmd, 0);

	sdhci_writew(host, SDHCI_MAKE_CMD(cmd->opcode, flags), SDHCI_COMMAND);

	/*polling mode*/
	if (swcq->need_polling) {
		second_time = sched_clock();
		pre_time = second_time;
		while (1) {
			intmask = sdhci_readl(host, SDHCI_INT_STATUS);
			if (((intmask & SDHCI_INT_CMD_MASK) &&
			  !(cmd->opcode == MMC_EXECUTE_READ_TASK
			  || cmd->opcode == MMC_EXECUTE_WRITE_TASK))
			  || ((intmask & SDHCI_INT_DATA_MASK)
			  && (cmd->opcode == MMC_EXECUTE_READ_TASK
			  || cmd->opcode == MMC_EXECUTE_WRITE_TASK))) {
				now = sched_clock();
				sdhci_sprd_irq(0, host);
				return true;
			}
			now = sched_clock();
			if (now - second_time > swcq->poll_timeout) {
				if (data)
					break;
				else {
					if (now - pre_time > CMD_TIMEOUT) {
						pr_err("%s [SWCQ] cmd%d polling timeout.",
							mmc_hostname(host->mmc), cmd->opcode);
						return false;
					}
					if (cmd->opcode == MMC_QUE_TASK_ADDR)
						spin_unlock(&host->lock);

					usleep_range_state(50, 100, TASK_UNINTERRUPTIBLE);

					if (cmd->opcode == MMC_QUE_TASK_ADDR)
						spin_lock(&host->lock);
					second_time = sched_clock();
				}
			}
		}

		/*switch to interrupt mode*/
		swcq->need_intr = true;
		sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
	}

	wmb();

	return true;
}

static void sdhci_sprd_read_rsp_136(struct sdhci_host *host, struct mmc_command *cmd)
{
	int i, reg;

	for (i = 0; i < 4; i++) {
		reg = SDHCI_RESPONSE + (3 - i) * 4;
		cmd->resp[i] = sdhci_readl(host, reg);
	}

	if (host->quirks2 & SDHCI_QUIRK2_RSP_136_HAS_CRC)
		return;

	/* CRC is stripped so we need to do some shifting */
	for (i = 0; i < 4; i++) {
		cmd->resp[i] <<= 8;
		if (i != 3)
			cmd->resp[i] |= cmd->resp[i + 1] >> 24;
	}
}

static void sdhci_sprd_finish_command(struct sdhci_host *host)
{
	struct mmc_command *cmd = host->cmd;

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			sdhci_sprd_read_rsp_136(host, cmd);
		} else {
			cmd->resp[0] = sdhci_readl(host, SDHCI_RESPONSE);
			dbg_add_host_log(host->mmc, MMC_CMD_RSP, cmd->opcode,
				cmd->resp[0], cmd->mrq);
		}
	}

	host->cmd = NULL;

	//if (cmd->mrq->cap_cmd_during_tfr && cmd == cmd->mrq->cmd)
	//	mmc_command_done(host->mmc, cmd->mrq);

	/*
	 * The host can send and interrupt when the busy state has
	 * ended, allowing us to wait without wasting CPU cycles.
	 * The busy signal uses DAT0 so this is similar to waiting
	 * for data to complete.
	 *
	 * Note: The 1.0 specification is a bit ambiguous about this
	 *       feature so there might be some problems with older
	 *       controllers.
	 */
	if (cmd->flags & MMC_RSP_BUSY) {
		if (cmd->data) {
			DBG("Cannot wait for busy signal when also doing a data transfer");
		} else if (!(host->quirks & SDHCI_QUIRK_NO_BUSY_IRQ) &&
			   cmd == host->data_cmd) {
			/* Command complete before busy is ended */
			return;
		}
	}

	/* Finished CMD23, now send actual command. */
	if (cmd == cmd->mrq->sbc) {
		if (!sdhci_sprd_send_command(host, cmd->mrq->cmd)) {
			WARN_ON(host->deferred_cmd);
			host->deferred_cmd = cmd->mrq->cmd;
		}
	} else {

		/* Processed actual command. */
		if (host->data && host->data_early)
			sdhci_sprd_finish_data(host);

		if (!cmd->data)
			__sdhci_sprd_finish_mrq(host, cmd->mrq);
	}
}


int sdhci_sprd_request_sync(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	int ret = 0;

	if (sdhci_sprd_present_error(host, mrq->cmd, true)) {
		sdhci_sprd_finish_mrq(host, mrq);
		goto out_finish;
	}

	cmd = sdhci_manual_cmd23(host, mrq) ? mrq->sbc : mrq->cmd;

	/*
	 * The HSQ may send a command in interrupt context without polling
	 * the busy signaling, which means we should return BUSY if controller
	 * has not released inhibit bits to allow HSQ trying to send request
	 * again in non-atomic context. So we should not finish this request
	 * here.
	 */
	if (!sdhci_sprd_send_command(host, cmd))
		ret = -EBUSY;

out_finish:
	return ret;
}

int _sdhci_request_atomic(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&host->lock, flags);

	if (sdhci_sprd_present_error(host, mrq->cmd, true)) {
		sdhci_sprd_finish_mrq(host, mrq);
		goto out_finish;
	}

	cmd = sdhci_manual_cmd23(host, mrq) ? mrq->sbc : mrq->cmd;

	/*
	 * The HSQ may send a command in interrupt context without polling
	 * the busy signaling, which means we should return BUSY if controller
	 * has not released inhibit bits to allow HSQ trying to send request
	 * again in non-atomic context. So we should not finish this request
	 * here.
	 */
	if (!sdhci_sprd_send_command(host, cmd))
		ret = -EBUSY;

out_finish:
	spin_unlock_irqrestore(&host->lock, flags);
	return ret;
}

int mmc_hsq_swcq_init(struct sdhci_host *host,
					struct platform_device *pdev)
{
	struct mmc_swcq *swcq;
	struct mmc_hsq *hsq;
	int ret = 0;

	if (HOST_IS_EMMC_TYPE(host->mmc)) {
		swcq = devm_kzalloc(&pdev->dev, sizeof(*swcq), GFP_KERNEL);
		if (!swcq) {
			ret = -ENOMEM;
			goto out;
		}
		ret = mmc_swcq_init(swcq, host->mmc);
		swcq->poll_timeout = POLL_TIMEOUT;
	} else {
		hsq = devm_kzalloc(&pdev->dev, sizeof(*hsq), GFP_KERNEL);
		if (!hsq) {
			ret = -ENOMEM;
			goto out;
		}

		ret = mmc_hsq_init(hsq, host->mmc);
	}
out:
	return ret;
}

int sdhci_sprd_irq_request_swcq(struct sdhci_host *host)
{
	int ret = 0;

	if (HOST_IS_EMMC_TYPE(host->mmc)) {
		free_irq(host->irq, host);
		ret = request_threaded_irq(host->irq, raw_sdhci_irq, raw_sdhci_thread_irq,
			IRQF_SHARED, mmc_hostname(host->mmc), host);
	}

	return ret;
}

