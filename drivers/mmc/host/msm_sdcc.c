/*
 *  linux/drivers/mmc/host/msm_sdcc.c - Qualcomm MSM 7X00A SDCC Driver
 *
 *  Copyright (C) 2007 Google Inc,
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *  Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on mmci.c
 *
 * Author: San Mehat (san@android.com)
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/log2.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/clk.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/pm_runtime.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/mmc/mmc.h>

#include <asm/cacheflush.h>
#include <asm/div64.h>
#include <asm/sizes.h>

#include <asm/mach/mmc.h>
#include <mach/msm_iomap.h>
#include <mach/clk.h>
#include <mach/dma.h>
#include <mach/htc_pwrsink.h>
#include <mach/sdio_al.h>

#include "msm_sdcc.h"
#include "msm_sdcc_dml.h"

#define DRIVER_NAME "msm-sdcc"

#define DBG(host, fmt, args...)	\
	pr_debug("%s: %s: " fmt "\n", mmc_hostname(host->mmc), __func__ , args)

#define IRQ_DEBUG 0
#define SPS_SDCC_PRODUCER_PIPE_INDEX	1
#define SPS_SDCC_CONSUMER_PIPE_INDEX	2
#define SPS_CONS_PERIPHERAL		0
#define SPS_PROD_PERIPHERAL		1
/* 16 KB */
#define SPS_MAX_DESC_SIZE		(16 * 1024)

#if defined(CONFIG_DEBUG_FS)
static void msmsdcc_dbg_createhost(struct msmsdcc_host *);
static struct dentry *debugfs_dir;
static struct dentry *debugfs_file;
static int  msmsdcc_dbg_init(void);
#endif

static unsigned int msmsdcc_pwrsave = 1;

static struct mmc_command dummy52cmd;
static struct mmc_request dummy52mrq = {
	.cmd = &dummy52cmd,
	.data = NULL,
	.stop = NULL,
};
static struct mmc_command dummy52cmd = {
	.opcode = SD_IO_RW_DIRECT,
	.flags = MMC_RSP_PRESENT,
	.data = NULL,
	.mrq = &dummy52mrq,
};
/*
 * An array holding the Tuning pattern to compare with when
 * executing a tuning cycle.
 */
static const u32 cmd19_tuning_block[16] = {
	0x00FF0FFF, 0xCCC3CCFF, 0xFFCC3CC3, 0xEFFEFFFE,
	0xDDFFDFFF, 0xFBFFFBFF, 0xFF7FFFBF, 0xEFBDF777,
	0xF0FFF0FF, 0x3CCCFC0F, 0xCFCC33CC, 0xEEFFEFFF,
	0xFDFFFDFF, 0xFFBFFFDF, 0xFFF7FFBB, 0xDE7B7FF7
};

#if IRQ_DEBUG == 1
static char *irq_status_bits[] = { "cmdcrcfail", "datcrcfail", "cmdtimeout",
				   "dattimeout", "txunderrun", "rxoverrun",
				   "cmdrespend", "cmdsent", "dataend", NULL,
				   "datablkend", "cmdactive", "txactive",
				   "rxactive", "txhalfempty", "rxhalffull",
				   "txfifofull", "rxfifofull", "txfifoempty",
				   "rxfifoempty", "txdataavlbl", "rxdataavlbl",
				   "sdiointr", "progdone", "atacmdcompl",
				   "sdiointrope", "ccstimeout", NULL, NULL,
				   NULL, NULL, NULL };

static void
msmsdcc_print_status(struct msmsdcc_host *host, char *hdr, uint32_t status)
{
	int i;

	pr_debug("%s-%s ", mmc_hostname(host->mmc), hdr);
	for (i = 0; i < 32; i++) {
		if (status & (1 << i))
			pr_debug("%s ", irq_status_bits[i]);
	}
	pr_debug("\n");
}
#endif

static void
msmsdcc_start_command(struct msmsdcc_host *host, struct mmc_command *cmd,
		      u32 c);
static inline void msmsdcc_delay(struct msmsdcc_host *host);


#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
static int msmsdcc_sps_reset_ep(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep);
static int msmsdcc_sps_restore_ep(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep);
#else
static inline int msmsdcc_sps_init_ep_conn(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep,
				bool is_producer) { return 0; }
static inline void msmsdcc_sps_exit_ep_conn(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep) { }
static inline int msmsdcc_sps_reset_ep(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep)
{
	return 0;
}
static inline int msmsdcc_sps_restore_ep(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep)
{
	return 0;
}
static inline int msmsdcc_sps_init(struct msmsdcc_host *host) { return 0; }
static inline void msmsdcc_sps_exit(struct msmsdcc_host *host) {}
#endif /* CONFIG_MMC_MSM_SPS_SUPPORT */

/**
 * Apply soft reset to all SDCC BAM pipes
 *
 * This function applies soft reset to SDCC BAM pipe.
 *
 * This function should be called to recover from error
 * conditions encountered during CMD/DATA tranfsers with card.
 *
 * @host - Pointer to driver's host structure
 *
 */
static void msmsdcc_sps_pipes_reset_and_restore(struct msmsdcc_host *host)
{
	int rc;

	/* Reset all SDCC BAM pipes */
	rc = msmsdcc_sps_reset_ep(host, &host->sps.prod);
	if (rc)
		pr_err("%s:msmsdcc_sps_reset_ep(prod) error=%d\n",
				mmc_hostname(host->mmc), rc);
	rc = msmsdcc_sps_reset_ep(host, &host->sps.cons);
	if (rc)
		pr_err("%s:msmsdcc_sps_reset_ep(cons) error=%d\n",
				mmc_hostname(host->mmc), rc);

	/* Restore all BAM pipes connections */
	rc = msmsdcc_sps_restore_ep(host, &host->sps.prod);
	if (rc)
		pr_err("%s:msmsdcc_sps_restore_ep(prod) error=%d\n",
				mmc_hostname(host->mmc), rc);
	rc = msmsdcc_sps_restore_ep(host, &host->sps.cons);
	if (rc)
		pr_err("%s:msmsdcc_sps_restore_ep(cons) error=%d\n",
				mmc_hostname(host->mmc), rc);
}

/**
 * Apply soft reset
 *
 * This function applies soft reset to SDCC core and DML core.
 *
 * This function should be called to recover from error
 * conditions encountered with CMD/DATA tranfsers with card.
 *
 * Soft reset should only be used with SDCC controller v4.
 *
 * @host - Pointer to driver's host structure
 *
 */
static void msmsdcc_soft_reset(struct msmsdcc_host *host)
{
	/*
	 * Reset SDCC controller's DPSM (data path state machine
	 * and CPSM (command path state machine).
	 */
	writel_relaxed(0, host->base + MMCICOMMAND);
	msmsdcc_delay(host);
	writel_relaxed(0, host->base + MMCIDATACTRL);
	msmsdcc_delay(host);
}

static void msmsdcc_hard_reset(struct msmsdcc_host *host)
{
	int ret;

	/* Reset the controller */
	ret = clk_reset(host->clk, CLK_RESET_ASSERT);
	if (ret)
		pr_err("%s: Clock assert failed at %u Hz"
			" with err %d\n", mmc_hostname(host->mmc),
				host->clk_rate, ret);

	ret = clk_reset(host->clk, CLK_RESET_DEASSERT);
	if (ret)
		pr_err("%s: Clock deassert failed at %u Hz"
			" with err %d\n", mmc_hostname(host->mmc),
			host->clk_rate, ret);

	/* Give some delay for clock reset to propogate to controller */
	msmsdcc_delay(host);
}

static void msmsdcc_reset_and_restore(struct msmsdcc_host *host)
{
	if (host->sdcc_version) {
		if (host->is_sps_mode) {
			/* Reset DML first */
			msmsdcc_dml_reset(host);
			/*
			 * delay the SPS pipe reset in thread context as
			 * sps_connect/sps_disconnect APIs can be called
			 * only from non-atomic context.
			 */
			host->sps.pipe_reset_pending = true;
		}
		mb();
		msmsdcc_soft_reset(host);

		pr_debug("%s: Applied soft reset to Controller\n",
				mmc_hostname(host->mmc));

		if (host->is_sps_mode)
			msmsdcc_dml_init(host);
	} else {
		/* Give Clock reset (hard reset) to controller */
		u32	mci_clk = 0;
		u32	mci_mask0 = 0;

		/* Save the controller state */
		mci_clk = readl_relaxed(host->base + MMCICLOCK);
		mci_mask0 = readl_relaxed(host->base + MMCIMASK0);
		mb();

		msmsdcc_hard_reset(host);
		pr_debug("%s: Controller has been reinitialized\n",
				mmc_hostname(host->mmc));

		/* Restore the contoller state */
		writel_relaxed(host->pwr, host->base + MMCIPOWER);
		msmsdcc_delay(host);
		writel_relaxed(mci_clk, host->base + MMCICLOCK);
		msmsdcc_delay(host);
		writel_relaxed(mci_mask0, host->base + MMCIMASK0);
		mb(); /* no delay required after writing to MASK0 register */
	}

	if (host->dummy_52_needed)
		host->dummy_52_needed = 0;
}

static int
msmsdcc_request_end(struct msmsdcc_host *host, struct mmc_request *mrq)
{
	int retval = 0;

	BUG_ON(host->curr.data);

	host->curr.mrq = NULL;
	host->curr.cmd = NULL;

	del_timer(&host->req_tout_timer);

	if (mrq->data)
		mrq->data->bytes_xfered = host->curr.data_xfered;
	if (mrq->cmd->error == -ETIMEDOUT)
		mdelay(5);

	/*
	 * Need to drop the host lock here; mmc_request_done may call
	 * back into the driver...
	 */
	spin_unlock(&host->lock);
	mmc_request_done(host->mmc, mrq);
	spin_lock(&host->lock);

	return retval;
}

static void
msmsdcc_stop_data(struct msmsdcc_host *host)
{
	host->curr.data = NULL;
	host->curr.got_dataend = 0;
	host->curr.wait_for_auto_prog_done = 0;
	host->curr.got_auto_prog_done = 0;
	writel_relaxed(readl_relaxed(host->base + MMCIDATACTRL) &
			(~(MCI_DPSM_ENABLE)), host->base + MMCIDATACTRL);
	msmsdcc_delay(host);	/* Allow the DPSM to be reset */
}

static inline uint32_t msmsdcc_fifo_addr(struct msmsdcc_host *host)
{
	return host->core_memres->start + MMCIFIFO;
}

static inline unsigned int msmsdcc_get_min_sup_clk_rate(
					struct msmsdcc_host *host);

static inline void msmsdcc_delay(struct msmsdcc_host *host)
{
	ktime_t start, diff;

	mb();
	udelay(host->reg_write_delay);

	if (host->sdcc_version &&
		(readl_relaxed(host->base + MCI_STATUS2) &
			MCI_MCLK_REG_WR_ACTIVE)) {
		start = ktime_get();
		while (readl_relaxed(host->base + MCI_STATUS2) &
			MCI_MCLK_REG_WR_ACTIVE) {
			diff = ktime_sub(ktime_get(), start);
			/* poll for max. 1 ms */
			if (ktime_to_us(diff) > 1000) {
				pr_warning("%s: previous reg. write is"
					" still active\n",
					mmc_hostname(host->mmc));
				break;
			}
		}
	}
}

static inline void
msmsdcc_start_command_exec(struct msmsdcc_host *host, u32 arg, u32 c)
{
	writel_relaxed(arg, host->base + MMCIARGUMENT);
	writel_relaxed(c, host->base + MMCICOMMAND);
	/*
	 * As after sending the command, we don't write any of the
	 * controller registers and just wait for the
	 * CMD_RESPOND_END/CMD_SENT/Command failure notication
	 * from Controller.
	 */
	mb();
}

static void
msmsdcc_dma_exec_func(struct msm_dmov_cmd *cmd)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *)cmd->user;

	writel_relaxed(host->cmd_timeout, host->base + MMCIDATATIMER);
	writel_relaxed((unsigned int)host->curr.xfer_size,
			host->base + MMCIDATALENGTH);
	writel_relaxed(host->cmd_datactrl, host->base + MMCIDATACTRL);
	msmsdcc_delay(host);	/* Force delay prior to ADM or command */

	if (host->cmd_cmd) {
		msmsdcc_start_command_exec(host,
			(u32)host->cmd_cmd->arg, (u32)host->cmd_c);
	}
}

static void
msmsdcc_dma_complete_tlet(unsigned long data)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *)data;
	unsigned long		flags;
	struct mmc_request	*mrq;

	spin_lock_irqsave(&host->lock, flags);
	mrq = host->curr.mrq;
	BUG_ON(!mrq);

	if (!(host->dma.result & DMOV_RSLT_VALID)) {
		pr_err("msmsdcc: Invalid DataMover result\n");
		goto out;
	}

	if (host->dma.result & DMOV_RSLT_DONE) {
		host->curr.data_xfered = host->curr.xfer_size;
		host->curr.xfer_remain -= host->curr.xfer_size;
	} else {
		/* Error or flush  */
		if (host->dma.result & DMOV_RSLT_ERROR)
			pr_err("%s: DMA error (0x%.8x)\n",
			       mmc_hostname(host->mmc), host->dma.result);
		if (host->dma.result & DMOV_RSLT_FLUSH)
			pr_err("%s: DMA channel flushed (0x%.8x)\n",
			       mmc_hostname(host->mmc), host->dma.result);
		pr_err("Flush data: %.8x %.8x %.8x %.8x %.8x %.8x\n",
		       host->dma.err.flush[0], host->dma.err.flush[1],
		       host->dma.err.flush[2], host->dma.err.flush[3],
		       host->dma.err.flush[4],
		       host->dma.err.flush[5]);
		msmsdcc_reset_and_restore(host);
		if (!mrq->data->error)
			mrq->data->error = -EIO;
	}
	dma_unmap_sg(mmc_dev(host->mmc), host->dma.sg, host->dma.num_ents,
		     host->dma.dir);

	if (host->curr.user_pages) {
		struct scatterlist *sg = host->dma.sg;
		int i;

		for (i = 0; i < host->dma.num_ents; i++, sg++)
			flush_dcache_page(sg_page(sg));
	}

	host->dma.sg = NULL;
	host->dma.busy = 0;

	if ((host->curr.got_dataend && (!host->curr.wait_for_auto_prog_done ||
		(host->curr.wait_for_auto_prog_done &&
		host->curr.got_auto_prog_done))) || mrq->data->error) {
		/*
		 * If we've already gotten our DATAEND / DATABLKEND
		 * for this request, then complete it through here.
		 */

		if (!mrq->data->error) {
			host->curr.data_xfered = host->curr.xfer_size;
			host->curr.xfer_remain -= host->curr.xfer_size;
		}
		if (host->dummy_52_needed) {
			mrq->data->bytes_xfered = host->curr.data_xfered;
			host->dummy_52_sent = 1;
			msmsdcc_start_command(host, &dummy52cmd,
					      MCI_CPSM_PROGENA);
			goto out;
		}
		msmsdcc_stop_data(host);
		if (!mrq->data->stop || mrq->cmd->error ||
			(mrq->sbc && !mrq->data->error)) {
			host->curr.mrq = NULL;
			host->curr.cmd = NULL;
			mrq->data->bytes_xfered = host->curr.data_xfered;
			del_timer(&host->req_tout_timer);
			spin_unlock_irqrestore(&host->lock, flags);

			mmc_request_done(host->mmc, mrq);
			return;
		} else if (mrq->data->stop && ((mrq->sbc && mrq->data->error)
				|| !mrq->sbc)) {
			msmsdcc_start_command(host, mrq->data->stop, 0);
		}
	}

out:
	spin_unlock_irqrestore(&host->lock, flags);
	return;
}

#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
/**
 * Callback notification from SPS driver
 *
 * This callback function gets triggered called from
 * SPS driver when requested SPS data transfer is
 * completed.
 *
 * SPS driver invokes this callback in BAM irq context so
 * SDCC driver schedule a tasklet for further processing
 * this callback notification at later point of time in
 * tasklet context and immediately returns control back
 * to SPS driver.
 *
 * @nofity - Pointer to sps event notify sturcture
 *
 */
static void
msmsdcc_sps_complete_cb(struct sps_event_notify *notify)
{
	struct msmsdcc_host *host =
		(struct msmsdcc_host *)
		((struct sps_event_notify *)notify)->user;

	host->sps.notify = *notify;
	pr_debug("%s: %s: sps ev_id=%d, addr=0x%x, size=0x%x, flags=0x%x\n",
		mmc_hostname(host->mmc), __func__, notify->event_id,
		notify->data.transfer.iovec.addr,
		notify->data.transfer.iovec.size,
		notify->data.transfer.iovec.flags);
	/* Schedule a tasklet for completing data transfer */
	tasklet_schedule(&host->sps.tlet);
}

/**
 * Tasklet handler for processing SPS callback event
 *
 * This function processing SPS event notification and
 * checks if the SPS transfer is completed or not and
 * then accordingly notifies status to MMC core layer.
 *
 * This function is called in tasklet context.
 *
 * @data - Pointer to sdcc driver data
 *
 */
static void msmsdcc_sps_complete_tlet(unsigned long data)
{
	unsigned long flags;
	int i, rc;
	u32 data_xfered = 0;
	struct mmc_request *mrq;
	struct sps_iovec iovec;
	struct sps_pipe *sps_pipe_handle;
	struct msmsdcc_host *host = (struct msmsdcc_host *)data;
	struct sps_event_notify *notify = &host->sps.notify;

	spin_lock_irqsave(&host->lock, flags);
	if (host->sps.dir == DMA_FROM_DEVICE)
		sps_pipe_handle = host->sps.prod.pipe_handle;
	else
		sps_pipe_handle = host->sps.cons.pipe_handle;
	mrq = host->curr.mrq;

	if (!mrq) {
		spin_unlock_irqrestore(&host->lock, flags);
		return;
	}

	pr_debug("%s: %s: sps event_id=%d\n",
		mmc_hostname(host->mmc), __func__,
		notify->event_id);

	if (msmsdcc_is_dml_busy(host)) {
		/* oops !!! this should never happen. */
		pr_err("%s: %s: Received SPS EOT event"
			" but DML HW is still busy !!!\n",
			mmc_hostname(host->mmc), __func__);
	}
	/*
	 * Got End of transfer event!!! Check if all of the data
	 * has been transferred?
	 */
	for (i = 0; i < host->sps.xfer_req_cnt; i++) {
		rc = sps_get_iovec(sps_pipe_handle, &iovec);
		if (rc) {
			pr_err("%s: %s: sps_get_iovec() failed rc=%d, i=%d",
				mmc_hostname(host->mmc), __func__, rc, i);
			break;
		}
		data_xfered += iovec.size;
	}

	if (data_xfered == host->curr.xfer_size) {
		host->curr.data_xfered = host->curr.xfer_size;
		host->curr.xfer_remain -= host->curr.xfer_size;
		pr_debug("%s: Data xfer success. data_xfered=0x%x",
			mmc_hostname(host->mmc),
			host->curr.xfer_size);
	} else {
		pr_err("%s: Data xfer failed. data_xfered=0x%x,"
			" xfer_size=%d", mmc_hostname(host->mmc),
			data_xfered, host->curr.xfer_size);
		msmsdcc_reset_and_restore(host);
		if (!mrq->data->error)
			mrq->data->error = -EIO;
	}

	/* Unmap sg buffers */
	dma_unmap_sg(mmc_dev(host->mmc), host->sps.sg, host->sps.num_ents,
			 host->sps.dir);

	host->sps.sg = NULL;
	host->sps.busy = 0;

	if ((host->curr.got_dataend && (!host->curr.wait_for_auto_prog_done ||
		(host->curr.wait_for_auto_prog_done &&
		host->curr.got_auto_prog_done))) || mrq->data->error) {
		/*
		 * If we've already gotten our DATAEND / DATABLKEND
		 * for this request, then complete it through here.
		 */

		if (!mrq->data->error) {
			host->curr.data_xfered = host->curr.xfer_size;
			host->curr.xfer_remain -= host->curr.xfer_size;
		}
		if (host->dummy_52_needed) {
			mrq->data->bytes_xfered = host->curr.data_xfered;
			host->dummy_52_sent = 1;
			msmsdcc_start_command(host, &dummy52cmd,
					      MCI_CPSM_PROGENA);
			spin_unlock_irqrestore(&host->lock, flags);
			return;
		}
		msmsdcc_stop_data(host);
		if (!mrq->data->stop || mrq->cmd->error ||
			(mrq->sbc && !mrq->data->error)) {
			host->curr.mrq = NULL;
			host->curr.cmd = NULL;
			mrq->data->bytes_xfered = host->curr.data_xfered;
			del_timer(&host->req_tout_timer);
			spin_unlock_irqrestore(&host->lock, flags);

			mmc_request_done(host->mmc, mrq);
			return;
		} else if (mrq->data->stop && ((mrq->sbc && mrq->data->error)
				|| !mrq->sbc)) {
			msmsdcc_start_command(host, mrq->data->stop, 0);
		}
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

/**
 * Exit from current SPS data transfer
 *
 * This function exits from current SPS data transfer.
 *
 * This function should be called when error condition
 * is encountered during data transfer.
 *
 * @host - Pointer to sdcc host structure
 *
 */
static void msmsdcc_sps_exit_curr_xfer(struct msmsdcc_host *host)
{
	struct mmc_request *mrq;

	mrq = host->curr.mrq;
	BUG_ON(!mrq);

	msmsdcc_reset_and_restore(host);
	if (!mrq->data->error)
		mrq->data->error = -EIO;

	/* Unmap sg buffers */
	dma_unmap_sg(mmc_dev(host->mmc), host->sps.sg, host->sps.num_ents,
			 host->sps.dir);

	host->sps.sg = NULL;
	host->sps.busy = 0;
	if (host->curr.data)
		msmsdcc_stop_data(host);

	if (!mrq->data->stop || mrq->cmd->error ||
		(mrq->sbc && !mrq->data->error))
		msmsdcc_request_end(host, mrq);
	else if (mrq->data->stop && ((mrq->sbc && mrq->data->error)
			|| !mrq->sbc))
		msmsdcc_start_command(host, mrq->data->stop, 0);

}
#else
static inline void msmsdcc_sps_complete_cb(struct sps_event_notify *notify) { }
static inline void msmsdcc_sps_complete_tlet(unsigned long data) { }
static inline void msmsdcc_sps_exit_curr_xfer(struct msmsdcc_host *host) { }
#endif /* CONFIG_MMC_MSM_SPS_SUPPORT */

static void msmsdcc_enable_cdr_cm_sdc4_dll(struct msmsdcc_host *host);

static void
msmsdcc_dma_complete_func(struct msm_dmov_cmd *cmd,
			  unsigned int result,
			  struct msm_dmov_errdata *err)
{
	struct msmsdcc_dma_data	*dma_data =
		container_of(cmd, struct msmsdcc_dma_data, hdr);
	struct msmsdcc_host *host = dma_data->host;

	dma_data->result = result;
	if (err)
		memcpy(&dma_data->err, err, sizeof(struct msm_dmov_errdata));

	tasklet_schedule(&host->dma_tlet);
}

static int msmsdcc_check_dma_op_req(struct mmc_data *data)
{
	if (((data->blksz * data->blocks) < MCI_FIFOSIZE) ||
	     ((data->blksz * data->blocks) % MCI_FIFOSIZE))
		return -EINVAL;
	else
		return 0;
}

static int msmsdcc_config_dma(struct msmsdcc_host *host, struct mmc_data *data)
{
	struct msmsdcc_nc_dmadata *nc;
	dmov_box *box;
	uint32_t rows;
	unsigned int n;
	int i;
	struct scatterlist *sg = data->sg;

	if ((host->dma.channel == -1) || (host->dma.crci == -1))
		return -ENOENT;

	BUG_ON((host->pdev_id < 1) || (host->pdev_id > 5));

	host->dma.sg = data->sg;
	host->dma.num_ents = data->sg_len;

	BUG_ON(host->dma.num_ents > NR_SG); /* Prevent memory corruption */

	nc = host->dma.nc;

	if (data->flags & MMC_DATA_READ)
		host->dma.dir = DMA_FROM_DEVICE;
	else
		host->dma.dir = DMA_TO_DEVICE;

	/* host->curr.user_pages = (data->flags & MMC_DATA_USERPAGE); */
	host->curr.user_pages = 0;
	box = &nc->cmd[0];
	for (i = 0; i < host->dma.num_ents; i++) {
		box->cmd = CMD_MODE_BOX;

		/* Initialize sg dma address */
		sg->dma_address = pfn_to_dma(mmc_dev(host->mmc),
					      page_to_pfn(sg_page(sg)))
					      + sg->offset;

		if (i == (host->dma.num_ents - 1))
			box->cmd |= CMD_LC;
		rows = (sg_dma_len(sg) % MCI_FIFOSIZE) ?
			(sg_dma_len(sg) / MCI_FIFOSIZE) + 1 :
			(sg_dma_len(sg) / MCI_FIFOSIZE) ;

		if (data->flags & MMC_DATA_READ) {
			box->src_row_addr = msmsdcc_fifo_addr(host);
			box->dst_row_addr = sg_dma_address(sg);

			box->src_dst_len = (MCI_FIFOSIZE << 16) |
					   (MCI_FIFOSIZE);
			box->row_offset = MCI_FIFOSIZE;

			box->num_rows = rows * ((1 << 16) + 1);
			box->cmd |= CMD_SRC_CRCI(host->dma.crci);
		} else {
			box->src_row_addr = sg_dma_address(sg);
			box->dst_row_addr = msmsdcc_fifo_addr(host);

			box->src_dst_len = (MCI_FIFOSIZE << 16) |
					   (MCI_FIFOSIZE);
			box->row_offset = (MCI_FIFOSIZE << 16);

			box->num_rows = rows * ((1 << 16) + 1);
			box->cmd |= CMD_DST_CRCI(host->dma.crci);
		}
		box++;
		sg++;
	}

	/* location of command block must be 64 bit aligned */
	BUG_ON(host->dma.cmd_busaddr & 0x07);

	nc->cmdptr = (host->dma.cmd_busaddr >> 3) | CMD_PTR_LP;
	host->dma.hdr.cmdptr = DMOV_CMD_PTR_LIST |
			       DMOV_CMD_ADDR(host->dma.cmdptr_busaddr);
	host->dma.hdr.complete_func = msmsdcc_dma_complete_func;

	n = dma_map_sg(mmc_dev(host->mmc), host->dma.sg,
			host->dma.num_ents, host->dma.dir);
	/* dsb inside dma_map_sg will write nc out to mem as well */

	if (n != host->dma.num_ents) {
		pr_err("%s: Unable to map in all sg elements\n",
		       mmc_hostname(host->mmc));
		host->dma.sg = NULL;
		host->dma.num_ents = 0;
		return -ENOMEM;
	}

	return 0;
}

#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
/**
 * Submits data transfer request to SPS driver
 *
 * This function make sg (scatter gather) data buffers
 * DMA ready and then submits them to SPS driver for
 * transfer.
 *
 * @host - Pointer to sdcc host structure
 * @data - Pointer to mmc_data structure
 *
 * @return 0 if success else negative value
 */
static int msmsdcc_sps_start_xfer(struct msmsdcc_host *host,
				struct mmc_data *data)
{
	int rc = 0;
	u32 flags;
	int i;
	u32 addr, len, data_cnt;
	struct scatterlist *sg = data->sg;
	struct sps_pipe *sps_pipe_handle;

	BUG_ON(data->sg_len > NR_SG); /* Prevent memory corruption */

	host->sps.sg = data->sg;
	host->sps.num_ents = data->sg_len;
	host->sps.xfer_req_cnt = 0;
	if (data->flags & MMC_DATA_READ) {
		host->sps.dir = DMA_FROM_DEVICE;
		sps_pipe_handle = host->sps.prod.pipe_handle;
	} else {
		host->sps.dir = DMA_TO_DEVICE;
		sps_pipe_handle = host->sps.cons.pipe_handle;
	}

	/* Make sg buffers DMA ready */
	rc = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
			host->sps.dir);

	if (rc != data->sg_len) {
		pr_err("%s: Unable to map in all sg elements, rc=%d\n",
		       mmc_hostname(host->mmc), rc);
		host->sps.sg = NULL;
		host->sps.num_ents = 0;
		rc = -ENOMEM;
		goto dma_map_err;
	}

	pr_debug("%s: %s: %s: pipe=0x%x, total_xfer=0x%x, sg_len=%d\n",
		mmc_hostname(host->mmc), __func__,
		host->sps.dir == DMA_FROM_DEVICE ? "READ" : "WRITE",
		(u32)sps_pipe_handle, host->curr.xfer_size, data->sg_len);

	for (i = 0; i < data->sg_len; i++) {
		/*
		 * Check if this is the last buffer to transfer?
		 * If yes then set the INT and EOT flags.
		 */
		len = sg_dma_len(sg);
		addr = sg_dma_address(sg);
		flags = 0;
		while (len > 0) {
			if (len > SPS_MAX_DESC_SIZE) {
				data_cnt = SPS_MAX_DESC_SIZE;
			} else {
				data_cnt = len;
				if (i == data->sg_len - 1)
					flags = SPS_IOVEC_FLAG_INT |
						SPS_IOVEC_FLAG_EOT;
			}
			rc = sps_transfer_one(sps_pipe_handle, addr,
						data_cnt, host, flags);
			if (rc) {
				pr_err("%s: sps_transfer_one() error! rc=%d,"
					" pipe=0x%x, sg=0x%x, sg_buf_no=%d\n",
					mmc_hostname(host->mmc), rc,
					(u32)sps_pipe_handle, (u32)sg, i);
				goto dma_map_err;
			}
			addr += data_cnt;
			len -= data_cnt;
			host->sps.xfer_req_cnt++;
		}
		sg++;
	}
	goto out;

dma_map_err:
	/* unmap sg buffers */
	dma_unmap_sg(mmc_dev(host->mmc), host->sps.sg, host->sps.num_ents,
			host->sps.dir);
out:
	return rc;
}
#else
static int msmsdcc_sps_start_xfer(struct msmsdcc_host *host,
				struct mmc_data *data) { return 0; }
#endif /* CONFIG_MMC_MSM_SPS_SUPPORT */

static void
msmsdcc_start_command_deferred(struct msmsdcc_host *host,
				struct mmc_command *cmd, u32 *c)
{
	DBG(host, "op %02x arg %08x flags %08x\n",
	    cmd->opcode, cmd->arg, cmd->flags);

	*c |= (cmd->opcode | MCI_CPSM_ENABLE);

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136)
			*c |= MCI_CPSM_LONGRSP;
		*c |= MCI_CPSM_RESPONSE;
	}

	if (/*interrupt*/0)
		*c |= MCI_CPSM_INTERRUPT;

	if ((((cmd->opcode == 17) || (cmd->opcode == 18))  ||
	     ((cmd->opcode == 24) || (cmd->opcode == 25))) ||
	      (cmd->opcode == 53))
		*c |= MCI_CSPM_DATCMD;

	/* Check if AUTO CMD19 is required or not? */
	if (host->tuning_needed) {
		/*
		 * For open ended block read operation (without CMD23),
		 * AUTO_CMD19 bit should be set while sending the READ command.
		 * For close ended block read operation (with CMD23),
		 * AUTO_CMD19 bit should be set while sending CMD23.
		 */
		if ((cmd->opcode == 23 && (host->curr.mrq->cmd->opcode == 17 ||
			host->curr.mrq->cmd->opcode == 18)) ||
			(!host->curr.mrq->sbc &&
			(cmd->opcode == 17 || cmd->opcode == 18))) {
			msmsdcc_enable_cdr_cm_sdc4_dll(host);
			*c |= MCI_CSPM_AUTO_CMD19;
		}
	}

	if ((cmd->flags & MMC_RSP_R1B) == MMC_RSP_R1B) {
		*c |= MCI_CPSM_PROGENA;
		host->prog_enable = 1;
	}

	if (cmd == cmd->mrq->stop)
		*c |= MCI_CSPM_MCIABORT;

	if (host->curr.cmd != NULL) {
		pr_err("%s: Overlapping command requests\n",
		       mmc_hostname(host->mmc));
	}
	host->curr.cmd = cmd;
}

static void
msmsdcc_start_data(struct msmsdcc_host *host, struct mmc_data *data,
			struct mmc_command *cmd, u32 c)
{
	unsigned int datactrl = 0, timeout;
	unsigned long long clks;
	void __iomem *base = host->base;
	unsigned int pio_irqmask = 0;

	BUG_ON(!data->sg);
	BUG_ON(!data->sg_len);

	host->curr.data = data;
	host->curr.xfer_size = data->blksz * data->blocks;
	host->curr.xfer_remain = host->curr.xfer_size;
	host->curr.data_xfered = 0;
	host->curr.got_dataend = 0;
	host->curr.got_auto_prog_done = 0;

	memset(&host->pio, 0, sizeof(host->pio));

	datactrl = MCI_DPSM_ENABLE | (data->blksz << 4);

	if (host->curr.wait_for_auto_prog_done)
		datactrl |= MCI_AUTO_PROG_DONE;

	if (!msmsdcc_check_dma_op_req(data)) {
		if (host->is_dma_mode && !msmsdcc_config_dma(host, data)) {
			datactrl |= MCI_DPSM_DMAENABLE;
		} else if (host->is_sps_mode) {
			if (!msmsdcc_is_dml_busy(host)) {
				if (!msmsdcc_sps_start_xfer(host, data)) {
					/* Now kick start DML transfer */
					mb();
					msmsdcc_dml_start_xfer(host, data);
					datactrl |= MCI_DPSM_DMAENABLE;
					host->sps.busy = 1;
				}
			} else {
				/*
				 * Can't proceed with new transfer as
				 * previous trasnfer is already in progress.
				 * There is no point of going into PIO mode
				 * as well. Is this a time to do kernel panic?
				 */
				pr_err("%s: %s: DML HW is busy!!!"
					" Can't perform new SPS transfers"
					" now\n", mmc_hostname(host->mmc),
					__func__);
			}
		}
	}

	/* Is data transfer in PIO mode required? */
	if (!(datactrl & MCI_DPSM_DMAENABLE)) {
		host->pio.sg = data->sg;
		host->pio.sg_len = data->sg_len;
		host->pio.sg_off = 0;

		if (data->flags & MMC_DATA_READ) {
			pio_irqmask = MCI_RXFIFOHALFFULLMASK;
			if (host->curr.xfer_remain < MCI_FIFOSIZE)
				pio_irqmask |= MCI_RXDATAAVLBLMASK;
		} else
			pio_irqmask = MCI_TXFIFOHALFEMPTYMASK |
					MCI_TXFIFOEMPTYMASK;
	}

	if (data->flags & MMC_DATA_READ)
		datactrl |= (MCI_DPSM_DIRECTION | MCI_RX_DATA_PEND);

	clks = (unsigned long long)data->timeout_ns * host->clk_rate;
	do_div(clks, 1000000000UL);
	timeout = data->timeout_clks + (unsigned int)clks*2 ;

	if (host->is_dma_mode && (datactrl & MCI_DPSM_DMAENABLE)) {
		/* Use ADM (Application Data Mover) HW for Data transfer */
		/* Save parameters for the dma exec function */
		host->cmd_timeout = timeout;
		host->cmd_pio_irqmask = pio_irqmask;
		host->cmd_datactrl = datactrl;
		host->cmd_cmd = cmd;

		host->dma.hdr.exec_func = msmsdcc_dma_exec_func;
		host->dma.hdr.user = (void *)host;
		host->dma.busy = 1;

		if (cmd) {
			msmsdcc_start_command_deferred(host, cmd, &c);
			host->cmd_c = c;
		}
		writel_relaxed((readl_relaxed(host->base + MMCIMASK0) &
				(~(MCI_IRQ_PIO))) | host->cmd_pio_irqmask,
				host->base + MMCIMASK0);
		mb();
		msm_dmov_enqueue_cmd_ext(host->dma.channel, &host->dma.hdr);
	} else {
		/* SPS-BAM mode or PIO mode */
		writel_relaxed(timeout, base + MMCIDATATIMER);

		writel_relaxed(host->curr.xfer_size, base + MMCIDATALENGTH);

		writel_relaxed((readl_relaxed(host->base + MMCIMASK0) &
				(~(MCI_IRQ_PIO))) | pio_irqmask,
				host->base + MMCIMASK0);
		writel_relaxed(datactrl, base + MMCIDATACTRL);
		/*
		 * We don't need delay after writing to DATA_CTRL register
		 * if we are not writing to CMD register immediately after
		 * this. As we already have delay before sending the
		 * command, we just need mb() here.
		 */
		mb();

		if (cmd) {
			msmsdcc_delay(host); /* Delay between data/command */
			/* Daisy-chain the command if requested */
			msmsdcc_start_command(host, cmd, c);
		}
	}
}

static void
msmsdcc_start_command(struct msmsdcc_host *host, struct mmc_command *cmd, u32 c)
{
	msmsdcc_start_command_deferred(host, cmd, &c);
	msmsdcc_start_command_exec(host, cmd->arg, c);
}

static void
msmsdcc_data_err(struct msmsdcc_host *host, struct mmc_data *data,
		 unsigned int status)
{
	if (status & MCI_DATACRCFAIL) {
		if (!(data->mrq->cmd->opcode == MMC_BUS_TEST_W
			|| data->mrq->cmd->opcode == MMC_BUS_TEST_R)) {
			pr_err("%s: Data CRC error\n",
			       mmc_hostname(host->mmc));
			pr_err("%s: opcode 0x%.8x\n", __func__,
			       data->mrq->cmd->opcode);
			pr_err("%s: blksz %d, blocks %d\n", __func__,
			       data->blksz, data->blocks);
			data->error = -EILSEQ;
		}
	} else if (status & MCI_DATATIMEOUT) {
		/* CRC is optional for the bus test commands, not all
		 * cards respond back with CRC. However controller
		 * waits for the CRC and times out. Hence ignore the
		 * data timeouts during the Bustest.
		 */
		if (!(data->mrq->cmd->opcode == MMC_BUS_TEST_W
			|| data->mrq->cmd->opcode == MMC_BUS_TEST_R)) {
			pr_err("%s: Data timeout\n",
				 mmc_hostname(host->mmc));
			data->error = -ETIMEDOUT;
		}
	} else if (status & MCI_RXOVERRUN) {
		pr_err("%s: RX overrun\n", mmc_hostname(host->mmc));
		data->error = -EIO;
	} else if (status & MCI_TXUNDERRUN) {
		pr_err("%s: TX underrun\n", mmc_hostname(host->mmc));
		data->error = -EIO;
	} else {
		pr_err("%s: Unknown error (0x%.8x)\n",
		      mmc_hostname(host->mmc), status);
		data->error = -EIO;
	}

	/* Dummy CMD52 is not needed when CMD53 has errors */
	if (host->dummy_52_needed)
		host->dummy_52_needed = 0;
}

static int
msmsdcc_pio_read(struct msmsdcc_host *host, char *buffer, unsigned int remain)
{
	void __iomem	*base = host->base;
	uint32_t	*ptr = (uint32_t *) buffer;
	int		count = 0;

	if (remain % 4)
		remain = ((remain >> 2) + 1) << 2;

	while (readl_relaxed(base + MMCISTATUS) & MCI_RXDATAAVLBL) {

		*ptr = readl_relaxed(base + MMCIFIFO + (count % MCI_FIFOSIZE));
		ptr++;
		count += sizeof(uint32_t);

		remain -=  sizeof(uint32_t);
		if (remain == 0)
			break;
	}
	return count;
}

static int
msmsdcc_pio_write(struct msmsdcc_host *host, char *buffer,
		  unsigned int remain)
{
	void __iomem *base = host->base;
	char *ptr = buffer;
	unsigned int maxcnt = MCI_FIFOHALFSIZE;

	while (readl_relaxed(base + MMCISTATUS) &
		(MCI_TXFIFOEMPTY | MCI_TXFIFOHALFEMPTY)) {
		unsigned int count, sz;

		count = min(remain, maxcnt);

		sz = count % 4 ? (count >> 2) + 1 : (count >> 2);
		writesl(base + MMCIFIFO, ptr, sz);
		ptr += count;
		remain -= count;

		if (remain == 0)
			break;
	}
	mb();

	return ptr - buffer;
}

static irqreturn_t
msmsdcc_pio_irq(int irq, void *dev_id)
{
	struct msmsdcc_host	*host = dev_id;
	void __iomem		*base = host->base;
	uint32_t		status;

	spin_lock(&host->lock);

	status = readl_relaxed(base + MMCISTATUS);

	if (((readl_relaxed(host->base + MMCIMASK0) & status) &
				(MCI_IRQ_PIO)) == 0) {
		spin_unlock(&host->lock);
		return IRQ_NONE;
	}

#if IRQ_DEBUG
	msmsdcc_print_status(host, "irq1-r", status);
#endif

	do {
		unsigned long flags;
		unsigned int remain, len;
		char *buffer;

		if (!(status & (MCI_TXFIFOHALFEMPTY | MCI_TXFIFOEMPTY
				| MCI_RXDATAAVLBL)))
			break;

		/* Map the current scatter buffer */
		local_irq_save(flags);
		buffer = kmap_atomic(sg_page(host->pio.sg),
				     KM_BIO_SRC_IRQ) + host->pio.sg->offset;
		buffer += host->pio.sg_off;
		remain = host->pio.sg->length - host->pio.sg_off;

		len = 0;
		if (status & MCI_RXACTIVE)
			len = msmsdcc_pio_read(host, buffer, remain);
		if (status & MCI_TXACTIVE)
			len = msmsdcc_pio_write(host, buffer, remain);

		/* Unmap the buffer */
		kunmap_atomic(buffer, KM_BIO_SRC_IRQ);
		local_irq_restore(flags);

		host->pio.sg_off += len;
		host->curr.xfer_remain -= len;
		host->curr.data_xfered += len;
		remain -= len;

		if (remain) /* Done with this page? */
			break; /* Nope */

		if (status & MCI_RXACTIVE && host->curr.user_pages)
			flush_dcache_page(sg_page(host->pio.sg));

		if (!--host->pio.sg_len) {
			memset(&host->pio, 0, sizeof(host->pio));
			break;
		}

		/* Advance to next sg */
		host->pio.sg++;
		host->pio.sg_off = 0;

		status = readl_relaxed(base + MMCISTATUS);
	} while (1);

	if (status & MCI_RXACTIVE && host->curr.xfer_remain < MCI_FIFOSIZE) {
		writel_relaxed((readl_relaxed(host->base + MMCIMASK0) &
				(~(MCI_IRQ_PIO))) | MCI_RXDATAAVLBLMASK,
				host->base + MMCIMASK0);
		if (!host->curr.xfer_remain) {
			/*
			 * back to back write to MASK0 register don't need
			 * synchronization delay.
			 */
			writel_relaxed((readl_relaxed(host->base + MMCIMASK0) &
				(~(MCI_IRQ_PIO))) | 0, host->base + MMCIMASK0);
		}
		mb();
	} else if (!host->curr.xfer_remain) {
		writel_relaxed((readl_relaxed(host->base + MMCIMASK0) &
				(~(MCI_IRQ_PIO))) | 0, host->base + MMCIMASK0);
		mb();
	}

	spin_unlock(&host->lock);

	return IRQ_HANDLED;
}

static void
msmsdcc_request_start(struct msmsdcc_host *host, struct mmc_request *mrq);

static void msmsdcc_wait_for_rxdata(struct msmsdcc_host *host,
					struct mmc_data *data)
{
	u32 loop_cnt = 0;

	/*
	 * For read commands with data less than fifo size, it is possible to
	 * get DATAEND first and RXDATA_AVAIL might be set later because of
	 * synchronization delay through the asynchronous RX FIFO. Thus, for
	 * such cases, even after DATAEND interrupt is received software
	 * should poll for RXDATA_AVAIL until the requested data is read out
	 * of FIFO. This change is needed to get around this abnormal but
	 * sometimes expected behavior of SDCC3 controller.
	 *
	 * We can expect RXDATAAVAIL bit to be set after 6HCLK clock cycles
	 * after the data is loaded into RX FIFO. This would amount to less
	 * than a microsecond and thus looping for 1000 times is good enough
	 * for that delay.
	 */
	while (((int)host->curr.xfer_remain > 0) && (++loop_cnt < 1000)) {
		if (readl_relaxed(host->base + MMCISTATUS) & MCI_RXDATAAVLBL) {
			spin_unlock(&host->lock);
			msmsdcc_pio_irq(1, host);
			spin_lock(&host->lock);
		}
	}
	if (loop_cnt == 1000) {
		pr_info("%s: Timed out while polling for Rx Data\n",
				mmc_hostname(host->mmc));
		data->error = -ETIMEDOUT;
		msmsdcc_reset_and_restore(host);
	}
}

static void msmsdcc_do_cmdirq(struct msmsdcc_host *host, uint32_t status)
{
	struct mmc_command *cmd = host->curr.cmd;

	host->curr.cmd = NULL;
	cmd->resp[0] = readl_relaxed(host->base + MMCIRESPONSE0);
	cmd->resp[1] = readl_relaxed(host->base + MMCIRESPONSE1);
	cmd->resp[2] = readl_relaxed(host->base + MMCIRESPONSE2);
	cmd->resp[3] = readl_relaxed(host->base + MMCIRESPONSE3);

	if (status & (MCI_CMDTIMEOUT | MCI_AUTOCMD19TIMEOUT)) {
		pr_debug("%s: Command timeout\n", mmc_hostname(host->mmc));
		cmd->error = -ETIMEDOUT;
	} else if ((status & MCI_CMDCRCFAIL && cmd->flags & MMC_RSP_CRC) &&
			!host->cmd19_tuning_in_progress) {
		pr_err("%s: Command CRC error\n", mmc_hostname(host->mmc));
		cmd->error = -EILSEQ;
	}

	if (!cmd->data || cmd->error) {
		if (host->curr.data && host->dma.sg &&
			host->is_dma_mode)
			msm_dmov_stop_cmd(host->dma.channel,
					  &host->dma.hdr, 0);
		else if (host->curr.data && host->sps.sg &&
			host->is_sps_mode){
			/* Stop current SPS transfer */
			msmsdcc_sps_exit_curr_xfer(host);
		}
		else if (host->curr.data) { /* Non DMA */
			msmsdcc_reset_and_restore(host);
			msmsdcc_stop_data(host);
			msmsdcc_request_end(host, cmd->mrq);
		} else { /* host->data == NULL */
			if (!cmd->error && host->prog_enable) {
				if (status & MCI_PROGDONE) {
					host->prog_enable = 0;
					msmsdcc_request_end(host, cmd->mrq);
				} else
					host->curr.cmd = cmd;
			} else {
				host->prog_enable = 0;
				if (host->dummy_52_needed)
					host->dummy_52_needed = 0;
				if (cmd->data && cmd->error)
					msmsdcc_reset_and_restore(host);
				msmsdcc_request_end(host, cmd->mrq);
			}
		}
	} else if ((cmd == host->curr.mrq->sbc) && cmd->data) {
		if (cmd->data->flags & MMC_DATA_READ)
			msmsdcc_start_command(host, host->curr.mrq->cmd, 0);
		else
			msmsdcc_request_start(host, host->curr.mrq);
	} else if (cmd->data) {
		if (!(cmd->data->flags & MMC_DATA_READ))
			msmsdcc_start_data(host, cmd->data, NULL, 0);
	}
}

static irqreturn_t
msmsdcc_irq(int irq, void *dev_id)
{
	struct msmsdcc_host	*host = dev_id;
	u32			status;
	int			ret = 0;
	int			timer = 0;

	spin_lock(&host->lock);

	do {
		struct mmc_command *cmd;
		struct mmc_data *data;

		if (timer) {
			timer = 0;
			msmsdcc_delay(host);
		}

		if (!host->clks_on) {
			pr_debug("%s: %s: SDIO async irq received\n",
					mmc_hostname(host->mmc), __func__);
			host->mmc->ios.clock = host->clk_rate;
			spin_unlock(&host->lock);
			host->mmc->ops->set_ios(host->mmc, &host->mmc->ios);
			spin_lock(&host->lock);
			if (host->plat->cfg_mpm_sdiowakeup &&
				(host->mmc->pm_flags & MMC_PM_WAKE_SDIO_IRQ))
				wake_lock(&host->sdio_wlock);
			/* only ansyc interrupt can come when clocks are off */
			writel_relaxed(MCI_SDIOINTMASK, host->base + MMCICLEAR);
			if (host->clk_rate <=
					msmsdcc_get_min_sup_clk_rate(host))
				msmsdcc_delay(host);
		}

		status = readl_relaxed(host->base + MMCISTATUS);

		if (((readl_relaxed(host->base + MMCIMASK0) & status) &
						(~(MCI_IRQ_PIO))) == 0)
			break;

#if IRQ_DEBUG
		msmsdcc_print_status(host, "irq0-r", status);
#endif
		status &= readl_relaxed(host->base + MMCIMASK0);
		writel_relaxed(status, host->base + MMCICLEAR);
		/* Allow clear to take effect*/
		if (host->clk_rate <=
				msmsdcc_get_min_sup_clk_rate(host))
			msmsdcc_delay(host);
#if IRQ_DEBUG
		msmsdcc_print_status(host, "irq0-p", status);
#endif

#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
		if (status & MCI_SDIOINTROPE) {
			if (host->sdcc_suspending)
				wake_lock(&host->sdio_suspend_wlock);
			mmc_signal_sdio_irq(host->mmc);
		}
#endif
		data = host->curr.data;

		if (host->dummy_52_sent) {
			if (status & (MCI_PROGDONE | MCI_CMDCRCFAIL |
					  MCI_CMDTIMEOUT)) {
				if (status & MCI_CMDTIMEOUT)
					pr_debug("%s: dummy CMD52 timeout\n",
						mmc_hostname(host->mmc));
				if (status & MCI_CMDCRCFAIL)
					pr_debug("%s: dummy CMD52 CRC failed\n",
						mmc_hostname(host->mmc));
				host->dummy_52_sent = 0;
				host->dummy_52_needed = 0;
				if (data) {
					msmsdcc_stop_data(host);
					msmsdcc_request_end(host, data->mrq);
				}
				WARN(!data, "No data cmd for dummy CMD52\n");
				spin_unlock(&host->lock);
				return IRQ_HANDLED;
			}
			break;
		}

		/*
		 * Check for proper command response
		 */
		cmd = host->curr.cmd;
		if ((status & (MCI_CMDSENT | MCI_CMDRESPEND | MCI_CMDCRCFAIL |
			MCI_CMDTIMEOUT | MCI_PROGDONE |
			MCI_AUTOCMD19TIMEOUT)) && host->curr.cmd) {
			msmsdcc_do_cmdirq(host, status);
		}

		if (data) {
			/* Check for data errors */
			if (status & (MCI_DATACRCFAIL|MCI_DATATIMEOUT|
				      MCI_TXUNDERRUN|MCI_RXOVERRUN)) {
				msmsdcc_data_err(host, data, status);
				host->curr.data_xfered = 0;
				if (host->dma.sg && host->is_dma_mode)
					msm_dmov_stop_cmd(host->dma.channel,
							  &host->dma.hdr, 0);
				else if (host->sps.sg && host->is_sps_mode) {
					/* Stop current SPS transfer */
					msmsdcc_sps_exit_curr_xfer(host);
				} else {
					msmsdcc_reset_and_restore(host);
					if (host->curr.data)
						msmsdcc_stop_data(host);
					if (!data->stop || (host->curr.mrq->sbc
						&& !data->error))
						timer |=
						 msmsdcc_request_end(host,
								    data->mrq);
					else if ((host->curr.mrq->sbc
						&& data->error) ||
						!host->curr.mrq->sbc) {
						msmsdcc_start_command(host,
								     data->stop,
								     0);
						timer = 1;
					}
				}
			}

			/* Check for prog done */
			if (host->curr.wait_for_auto_prog_done &&
				(status & MCI_PROGDONE))
				host->curr.got_auto_prog_done = 1;

			/* Check for data done */
			if (!host->curr.got_dataend && (status & MCI_DATAEND))
				host->curr.got_dataend = 1;

			if (host->curr.got_dataend &&
				(!host->curr.wait_for_auto_prog_done ||
				(host->curr.wait_for_auto_prog_done &&
				host->curr.got_auto_prog_done))) {
				/*
				 * If DMA is still in progress, we complete
				 * via the completion handler
				 */
				if (!host->dma.busy && !host->sps.busy) {
					/*
					 * There appears to be an issue in the
					 * controller where if you request a
					 * small block transfer (< fifo size),
					 * you may get your DATAEND/DATABLKEND
					 * irq without the PIO data irq.
					 *
					 * Check to see if theres still data
					 * to be read, and simulate a PIO irq.
					 */
					if (data->flags & MMC_DATA_READ)
						msmsdcc_wait_for_rxdata(host,
								data);
					if (!data->error) {
						host->curr.data_xfered =
							host->curr.xfer_size;
						host->curr.xfer_remain -=
							host->curr.xfer_size;
					}

					if (!host->dummy_52_needed) {
						msmsdcc_stop_data(host);
						if (!data->stop ||
							(host->curr.mrq->sbc
							&& !data->error))
							msmsdcc_request_end(
								  host,
								  data->mrq);
						else if ((host->curr.mrq->sbc
							&& data->error) ||
							!host->curr.mrq->sbc) {
							msmsdcc_start_command(
								host,
								data->stop, 0);
							timer = 1;
						}
					} else {
						host->dummy_52_sent = 1;
						msmsdcc_start_command(host,
							&dummy52cmd,
							MCI_CPSM_PROGENA);
					}
				}
			}
		}

		ret = 1;
	} while (status);

	spin_unlock(&host->lock);

	return IRQ_RETVAL(ret);
}

static void
msmsdcc_request_start(struct msmsdcc_host *host, struct mmc_request *mrq)
{
	if (mrq->data && mrq->data->flags & MMC_DATA_READ) {
		/* Queue/read data, daisy-chain command when data starts */
		if (mrq->sbc)
			msmsdcc_start_data(host, mrq->data, mrq->sbc, 0);
		else
			msmsdcc_start_data(host, mrq->data, mrq->cmd, 0);
	} else {
		msmsdcc_start_command(host, mrq->cmd, 0);
	}
}

static void
msmsdcc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long		flags;

	/*
	 * Get the SDIO AL client out of LPM.
	 */
	WARN(host->dummy_52_sent, "Dummy CMD52 in progress\n");
	if (host->plat->is_sdio_al_client)
		msmsdcc_sdio_al_lpm(mmc, false);

	/* check if sps pipe reset is pending? */
	if (host->is_sps_mode && host->sps.pipe_reset_pending) {
		msmsdcc_sps_pipes_reset_and_restore(host);
		host->sps.pipe_reset_pending = false;
	}

	spin_lock_irqsave(&host->lock, flags);
	WARN(host->curr.mrq, "Request in progress\n");
	WARN(!host->pwr, "SDCC power is turned off\n");
	WARN(!host->clks_on, "SDCC clocks are turned off\n");
	WARN(host->sdcc_irq_disabled, "SDCC IRQ is disabled\n");

	if (host->eject) {
		if (mrq->data && !(mrq->data->flags & MMC_DATA_READ)) {
			mrq->cmd->error = 0;
			mrq->data->bytes_xfered = mrq->data->blksz *
						  mrq->data->blocks;
		} else
			mrq->cmd->error = -ENOMEDIUM;

		spin_unlock_irqrestore(&host->lock, flags);
		mmc_request_done(mmc, mrq);
		return;
	}

	/*
	 * Kick the software command timeout timer here.
	 * Timer expires in 10 secs.
	 */
	mod_timer(&host->req_tout_timer,
			(jiffies + msecs_to_jiffies(MSM_MMC_REQ_TIMEOUT)));

	host->curr.mrq = mrq;
	if (mrq->data && (mrq->data->flags & MMC_DATA_WRITE)) {
		if (mrq->cmd->opcode == SD_IO_RW_EXTENDED ||
			mrq->cmd->opcode == 54) {
			if (!host->sdcc_version)
				host->dummy_52_needed = 1;
			else
				/*
				 * SDCCv4 supports AUTO_PROG_DONE bit for SDIO
				 * write operations using CMD53 and CMD54.
				 * Setting this bit with CMD53 would
				 * automatically triggers PROG_DONE interrupt
				 * without the need of sending dummy CMD52.
				 */
				host->curr.wait_for_auto_prog_done = 1;
		}
	}

	if (mrq->data && mrq->sbc) {
		mrq->sbc->mrq = mrq;
		mrq->sbc->data = mrq->data;
		if (mrq->data->flags & MMC_DATA_WRITE) {
			host->curr.wait_for_auto_prog_done = 1;
			msmsdcc_start_command(host, mrq->sbc, 0);
		} else {
			msmsdcc_request_start(host, mrq);
		}
	} else {
		msmsdcc_request_start(host, mrq);
	}

	spin_unlock_irqrestore(&host->lock, flags);
}

static inline int msmsdcc_vreg_set_voltage(struct msm_mmc_reg_data *vreg,
					int min_uV, int max_uV)
{
	int rc = 0;

	if (vreg->set_voltage_sup) {
		rc = regulator_set_voltage(vreg->reg, min_uV, max_uV);
		if (rc) {
			pr_err("%s: regulator_set_voltage(%s) failed."
				" min_uV=%d, max_uV=%d, rc=%d\n",
				__func__, vreg->name, min_uV, max_uV, rc);
		}
	}

	return rc;
}

static inline int msmsdcc_vreg_set_optimum_mode(struct msm_mmc_reg_data *vreg,
						int uA_load)
{
	int rc = 0;

	/* regulators that do not support regulator_set_voltage also
	   do not support regulator_set_optimum_mode */
	if (vreg->set_voltage_sup) {
		rc = regulator_set_optimum_mode(vreg->reg, uA_load);
		if (rc < 0)
			pr_err("%s: regulator_set_optimum_mode(reg=%s, "
				"uA_load=%d) failed. rc=%d\n", __func__,
				vreg->name, uA_load, rc);
		else
			/* regulator_set_optimum_mode() can return non zero
			 * value even for success case.
			 */
			rc = 0;
	}

	return rc;
}

static inline int msmsdcc_vreg_init_reg(struct msm_mmc_reg_data *vreg,
				struct device *dev)
{
	int rc = 0;

	/* check if regulator is already initialized? */
	if (vreg->reg)
		goto out;

	/* Get the regulator handle */
	vreg->reg = regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		rc = PTR_ERR(vreg->reg);
		pr_err("%s: regulator_get(%s) failed. rc=%d\n",
			__func__, vreg->name, rc);
		goto out;
	}

	if (regulator_count_voltages(vreg->reg) > 0)
		vreg->set_voltage_sup = 1;

out:
	return rc;
}

static inline void msmsdcc_vreg_deinit_reg(struct msm_mmc_reg_data *vreg)
{
	if (vreg->reg)
		regulator_put(vreg->reg);
}

/* This init function should be called only once for each SDCC slot */
static int msmsdcc_vreg_init(struct msmsdcc_host *host, bool is_init)
{
	int rc = 0;
	struct msm_mmc_slot_reg_data *curr_slot;
	struct msm_mmc_reg_data *curr_vdd_reg, *curr_vccq_reg, *curr_vddp_reg;
	struct device *dev = mmc_dev(host->mmc);

	curr_slot = host->plat->vreg_data;
	if (!curr_slot)
		goto out;

	curr_vdd_reg = curr_slot->vdd_data;
	curr_vccq_reg = curr_slot->vccq_data;
	curr_vddp_reg = curr_slot->vddp_data;

	if (is_init) {
		/*
		 * Get the regulator handle from voltage regulator framework
		 * and then try to set the voltage level for the regulator
		 */
		if (curr_vdd_reg) {
			rc = msmsdcc_vreg_init_reg(curr_vdd_reg, dev);
			if (rc)
				goto out;
		}
		if (curr_vccq_reg) {
			rc = msmsdcc_vreg_init_reg(curr_vccq_reg, dev);
			if (rc)
				goto vdd_reg_deinit;
		}
		if (curr_vddp_reg) {
			rc = msmsdcc_vreg_init_reg(curr_vddp_reg, dev);
			if (rc)
				goto vccq_reg_deinit;
		}
		goto out;
	} else {
		/* Deregister all regulators from regulator framework */
		goto vddp_reg_deinit;
	}
vddp_reg_deinit:
	if (curr_vddp_reg)
		msmsdcc_vreg_deinit_reg(curr_vddp_reg);
vccq_reg_deinit:
	if (curr_vccq_reg)
		msmsdcc_vreg_deinit_reg(curr_vccq_reg);
vdd_reg_deinit:
	if (curr_vdd_reg)
		msmsdcc_vreg_deinit_reg(curr_vdd_reg);
out:
	return rc;
}

static int msmsdcc_vreg_enable(struct msm_mmc_reg_data *vreg)
{
	int rc = 0;

	/* Put regulator in HPM (high power mode) */
	rc = msmsdcc_vreg_set_optimum_mode(vreg, vreg->hpm_uA);
	if (rc < 0)
		goto out;

	if (!vreg->is_enabled) {
		/* Set voltage level */
		rc = msmsdcc_vreg_set_voltage(vreg, vreg->high_vol_level,
						vreg->high_vol_level);
		if (rc)
			goto out;

		rc = regulator_enable(vreg->reg);
		if (rc) {
			pr_err("%s: regulator_enable(%s) failed. rc=%d\n",
			__func__, vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = true;
	}

out:
	return rc;
}

static int msmsdcc_vreg_disable(struct msm_mmc_reg_data *vreg)
{
	int rc = 0;

	/* Never disable regulator marked as always_on */
	if (vreg->is_enabled && !vreg->always_on) {
		rc = regulator_disable(vreg->reg);
		if (rc) {
			pr_err("%s: regulator_disable(%s) failed. rc=%d\n",
				__func__, vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = false;

		rc = msmsdcc_vreg_set_optimum_mode(vreg, 0);
		if (rc < 0)
			goto out;

		/* Set min. voltage level to 0 */
		rc = msmsdcc_vreg_set_voltage(vreg, 0, vreg->high_vol_level);
		if (rc)
			goto out;
	} else if (vreg->is_enabled && vreg->always_on && vreg->lpm_sup) {
		/* Put always_on regulator in LPM (low power mode) */
		rc = msmsdcc_vreg_set_optimum_mode(vreg, vreg->lpm_uA);
		if (rc < 0)
			goto out;
	}
out:
	return rc;
}

static int msmsdcc_setup_vreg(struct msmsdcc_host *host, bool enable)
{
	int rc = 0, i;
	struct msm_mmc_slot_reg_data *curr_slot;
	struct msm_mmc_reg_data *curr_vdd_reg, *curr_vccq_reg, *curr_vddp_reg;
	struct msm_mmc_reg_data *vreg_table[3];

	curr_slot = host->plat->vreg_data;
	if (!curr_slot)
		goto out;

	curr_vdd_reg = vreg_table[0] = curr_slot->vdd_data;
	curr_vccq_reg = vreg_table[1] = curr_slot->vccq_data;
	curr_vddp_reg = vreg_table[2] = curr_slot->vddp_data;

	for (i = 0; i < ARRAY_SIZE(vreg_table); i++) {
		if (vreg_table[i]) {
			if (enable)
				rc = msmsdcc_vreg_enable(vreg_table[i]);
			else
				rc = msmsdcc_vreg_disable(vreg_table[i]);
			if (rc)
				goto out;
		}
	}
out:
	return rc;
}

static int msmsdcc_set_vddp_level(struct msmsdcc_host *host, int level)
{
	int rc = 0;

	if (host->plat->vreg_data) {
		struct msm_mmc_reg_data *vddp_reg =
			host->plat->vreg_data->vddp_data;

		if (vddp_reg && vddp_reg->is_enabled)
			rc = msmsdcc_vreg_set_voltage(vddp_reg, level, level);
	}

	return rc;
}

static inline int msmsdcc_set_vddp_low_vol(struct msmsdcc_host *host)
{
	struct msm_mmc_slot_reg_data *curr_slot = host->plat->vreg_data;
	int rc = 0;

	if (curr_slot && curr_slot->vddp_data) {
		rc = msmsdcc_set_vddp_level(host,
			curr_slot->vddp_data->low_vol_level);

		if (rc)
			pr_err("%s: %s: failed to change vddp level to %d",
				mmc_hostname(host->mmc), __func__,
				curr_slot->vddp_data->low_vol_level);
	}

	return rc;
}

static inline int msmsdcc_set_vddp_high_vol(struct msmsdcc_host *host)
{
	struct msm_mmc_slot_reg_data *curr_slot = host->plat->vreg_data;
	int rc = 0;

	if (curr_slot && curr_slot->vddp_data) {
		rc = msmsdcc_set_vddp_level(host,
			curr_slot->vddp_data->high_vol_level);

		if (rc)
			pr_err("%s: %s: failed to change vddp level to %d",
				mmc_hostname(host->mmc), __func__,
				curr_slot->vddp_data->high_vol_level);
	}

	return rc;
}

static inline int msmsdcc_is_pwrsave(struct msmsdcc_host *host)
{
	if (host->clk_rate > 400000 && msmsdcc_pwrsave)
		return 1;
	return 0;
}

static inline void msmsdcc_setup_clocks(struct msmsdcc_host *host, bool enable)
{
	if (enable) {
		if (!IS_ERR_OR_NULL(host->dfab_pclk))
			clk_enable(host->dfab_pclk);
		if (!IS_ERR(host->pclk))
			clk_enable(host->pclk);
		clk_enable(host->clk);
		msmsdcc_delay(host);
	} else {
		msmsdcc_delay(host);
		clk_disable(host->clk);
		if (!IS_ERR(host->pclk))
			clk_disable(host->pclk);
		if (!IS_ERR_OR_NULL(host->dfab_pclk))
			clk_disable(host->dfab_pclk);
	}
}

static inline unsigned int msmsdcc_get_sup_clk_rate(struct msmsdcc_host *host,
						unsigned int req_clk)
{
	unsigned int sel_clk = -1;

	if (host->plat->sup_clk_table && host->plat->sup_clk_cnt) {
		unsigned char cnt;

		for (cnt = 0; cnt < host->plat->sup_clk_cnt; cnt++) {
			if (host->plat->sup_clk_table[cnt] > req_clk)
				break;
			else if (host->plat->sup_clk_table[cnt] == req_clk) {
				sel_clk = host->plat->sup_clk_table[cnt];
				break;
			} else
				sel_clk = host->plat->sup_clk_table[cnt];
		}
	} else {
		if ((req_clk < host->plat->msmsdcc_fmax) &&
			(req_clk > host->plat->msmsdcc_fmid))
			sel_clk = host->plat->msmsdcc_fmid;
		else
			sel_clk = req_clk;
	}

	return sel_clk;
}

static inline unsigned int msmsdcc_get_min_sup_clk_rate(
				struct msmsdcc_host *host)
{
	if (host->plat->sup_clk_table && host->plat->sup_clk_cnt)
		return host->plat->sup_clk_table[0];
	else
		return host->plat->msmsdcc_fmin;
}

static inline unsigned int msmsdcc_get_max_sup_clk_rate(
				struct msmsdcc_host *host)
{
	if (host->plat->sup_clk_table && host->plat->sup_clk_cnt)
		return host->plat->sup_clk_table[host->plat->sup_clk_cnt - 1];
	else
		return host->plat->msmsdcc_fmax;
}

static int msmsdcc_setup_gpio(struct msmsdcc_host *host, bool enable)
{
	struct msm_mmc_gpio_data *curr;
	int i, rc = 0;

	curr = host->plat->pin_data->gpio_data;
	for (i = 0; i < curr->size; i++) {
		if (enable) {
			if (curr->gpio[i].is_always_on &&
				curr->gpio[i].is_enabled)
				continue;
			rc = gpio_request(curr->gpio[i].no,
						curr->gpio[i].name);
			if (rc) {
				pr_err("%s: gpio_request(%d, %s) failed %d\n",
					mmc_hostname(host->mmc),
					curr->gpio[i].no,
					curr->gpio[i].name, rc);
				goto free_gpios;
			}
			curr->gpio[i].is_enabled = true;
		} else {
			if (curr->gpio[i].is_always_on)
				continue;
			gpio_free(curr->gpio[i].no);
			curr->gpio[i].is_enabled = false;
		}
	}
	goto out;

free_gpios:
	for (; i >= 0; i--) {
		gpio_free(curr->gpio[i].no);
		curr->gpio[i].is_enabled = false;
	}
out:
	return rc;
}

static int msmsdcc_setup_pad(struct msmsdcc_host *host, bool enable)
{
	struct msm_mmc_pad_data *curr;
	int i;

	curr = host->plat->pin_data->pad_data;
	for (i = 0; i < curr->drv->size; i++) {
		if (enable)
			msm_tlmm_set_hdrive(curr->drv->on[i].no,
				curr->drv->on[i].val);
		else
			msm_tlmm_set_hdrive(curr->drv->off[i].no,
				curr->drv->off[i].val);
	}

	for (i = 0; i < curr->pull->size; i++) {
		if (enable)
			msm_tlmm_set_pull(curr->pull->on[i].no,
				curr->pull->on[i].val);
		else
			msm_tlmm_set_pull(curr->pull->off[i].no,
				curr->pull->off[i].val);
	}

	return 0;
}

static u32 msmsdcc_setup_pins(struct msmsdcc_host *host, bool enable)
{
	int rc = 0;

	if (!host->plat->pin_data || host->plat->pin_data->cfg_sts == enable)
		return 0;

	if (host->plat->pin_data->is_gpio)
		rc = msmsdcc_setup_gpio(host, enable);
	else
		rc = msmsdcc_setup_pad(host, enable);

	if (!rc)
		host->plat->pin_data->cfg_sts = enable;

	return rc;
}

static void msmsdcc_enable_irq_wake(struct msmsdcc_host *host)
{
	unsigned int wakeup_irq;

	wakeup_irq = (host->plat->sdiowakeup_irq) ?
			host->plat->sdiowakeup_irq :
			host->core_irqres->start;

	if (!host->irq_wake_enabled) {
		enable_irq_wake(wakeup_irq);
		host->irq_wake_enabled = true;
	}
}

static void msmsdcc_disable_irq_wake(struct msmsdcc_host *host)
{
	unsigned int wakeup_irq;

	wakeup_irq = (host->plat->sdiowakeup_irq) ?
			host->plat->sdiowakeup_irq :
			host->core_irqres->start;

	if (host->irq_wake_enabled) {
		disable_irq_wake(wakeup_irq);
		host->irq_wake_enabled = false;
	}
}

static void
msmsdcc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	u32 clk = 0, pwr = 0;
	int rc;
	unsigned long flags;
	unsigned int clock;

	DBG(host, "ios->clock = %u\n", ios->clock);

	if (ios->clock) {
		spin_lock_irqsave(&host->lock, flags);
		if (!host->clks_on) {
			msmsdcc_setup_clocks(host, true);
			host->clks_on = 1;
			if (mmc->card && mmc->card->type == MMC_TYPE_SDIO) {
				if (!host->plat->sdiowakeup_irq) {
					writel_relaxed(host->mci_irqenable,
							host->base + MMCIMASK0);
					mb();
					if (host->plat->cfg_mpm_sdiowakeup &&
					(mmc->pm_flags & MMC_PM_WAKE_SDIO_IRQ))
						host->plat->cfg_mpm_sdiowakeup(
						mmc_dev(mmc), SDC_DAT1_DISWAKE);
					msmsdcc_disable_irq_wake(host);
				} else if (!(mmc->pm_flags &
							MMC_PM_WAKE_SDIO_IRQ)) {
					writel_relaxed(host->mci_irqenable,
							host->base + MMCIMASK0);
				}
			} else {
				writel_relaxed(host->mci_irqenable,
						host->base + MMCIMASK0);
				mb();
			}
		}
		spin_unlock_irqrestore(&host->lock, flags);

		clock = msmsdcc_get_sup_clk_rate(host, ios->clock);
		/*
		 * For DDR50 mode, controller needs clock rate to be
		 * double than what is required on the SD card CLK pin.
		 */
		if (ios->timing == MMC_TIMING_UHS_DDR50) {
			/*
			 * Make sure that we don't double the clock if
			 * doubled clock rate is already set
			 */
			if (!host->ddr_doubled_clk_rate ||
				(host->ddr_doubled_clk_rate &&
				(host->ddr_doubled_clk_rate != ios->clock))) {
				host->ddr_doubled_clk_rate =
					msmsdcc_get_sup_clk_rate(
						host, (ios->clock * 2));
				clock = host->ddr_doubled_clk_rate;
			}
		} else {
			host->ddr_doubled_clk_rate = 0;
		}

		if (clock != host->clk_rate) {
			rc = clk_set_rate(host->clk, clock);
			if (rc < 0)
				pr_debug("%s: failed to set clk rate %u\n",
						mmc_hostname(mmc), clock);
			host->clk_rate = clock;
			host->reg_write_delay =
				(1 + ((3 * USEC_PER_SEC) /
				      (host->clk_rate ? host->clk_rate :
				       msmsdcc_get_min_sup_clk_rate(host))));
		}
		/*
		 * give atleast 2 MCLK cycles delay for clocks
		 * and SDCC core to stabilize
		 */
		msmsdcc_delay(host);
		clk |= MCI_CLK_ENABLE;
	}

	if (ios->bus_width == MMC_BUS_WIDTH_8)
		clk |= MCI_CLK_WIDEBUS_8;
	else if (ios->bus_width == MMC_BUS_WIDTH_4)
		clk |= MCI_CLK_WIDEBUS_4;
	else
		clk |= MCI_CLK_WIDEBUS_1;

	if (msmsdcc_is_pwrsave(host))
		clk |= MCI_CLK_PWRSAVE;

	clk |= MCI_CLK_FLOWENA;

	host->tuning_needed = 0;
	/*
	 * Select the controller timing mode according
	 * to current bus speed mode
	 */
	if ((ios->timing == MMC_TIMING_UHS_SDR104) ||
		(ios->timing == MMC_TIMING_UHS_SDR50)) {
		clk |= (4 << 14);
		host->tuning_needed = 1;
	} else if (ios->timing == MMC_TIMING_UHS_DDR50) {
		clk |= (3 << 14);
	} else {
		clk |= (2 << 14); /* feedback clock */
	}

	/* Select free running MCLK as input clock of cm_dll_sdc4 */
	clk |= (2 << 23);

	if (host->io_pad_pwr_switch)
		clk |= IO_PAD_PWR_SWITCH;

	if (host->plat->translate_vdd && !host->sdio_gpio_lpm)
		pwr |= host->plat->translate_vdd(mmc_dev(mmc), ios->vdd);
	else if (!host->plat->translate_vdd && !host->sdio_gpio_lpm)
		pwr |= msmsdcc_setup_vreg(host, !!ios->vdd);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		htc_pwrsink_set(PWRSINK_SDCARD, 0);
		if (!host->sdcc_irq_disabled) {
			if (host->plat->cfg_mpm_sdiowakeup)
				host->plat->cfg_mpm_sdiowakeup(
					mmc_dev(mmc), SDC_DAT1_DISABLE);
			disable_irq(host->core_irqres->start);
			host->sdcc_irq_disabled = 1;
		}
		/*
		 * As VDD pad rail is always on, set low voltage for VDD
		 * pad rail when slot is unused (when card is not present
		 * or during system suspend).
		 */
		msmsdcc_set_vddp_low_vol(host);
		msmsdcc_setup_pins(host, false);
		break;
	case MMC_POWER_UP:
		/* writing PWR_UP bit is redundant */
		pwr |= MCI_PWR_UP;
		if (host->sdcc_irq_disabled) {
			if (host->plat->cfg_mpm_sdiowakeup)
				host->plat->cfg_mpm_sdiowakeup(
					mmc_dev(mmc), SDC_DAT1_ENABLE);
			enable_irq(host->core_irqres->start);
			host->sdcc_irq_disabled = 0;
		}
		msmsdcc_set_vddp_high_vol(host);
		msmsdcc_setup_pins(host, true);
		break;
	case MMC_POWER_ON:
		htc_pwrsink_set(PWRSINK_SDCARD, 100);
		pwr |= MCI_PWR_ON;
		break;
	}

	spin_lock_irqsave(&host->lock, flags);
	if (!host->clks_on) {
		/* force the clocks to be on */
		msmsdcc_setup_clocks(host, true);
		/*
		 * give atleast 2 MCLK cycles delay for clocks
		 * and SDCC core to stabilize
		 */
		msmsdcc_delay(host);
	}
	writel_relaxed(clk, host->base + MMCICLOCK);
	msmsdcc_delay(host);

	if (host->pwr != pwr) {
		host->pwr = pwr;
		writel_relaxed(pwr, host->base + MMCIPOWER);
		msmsdcc_delay(host);
	}
	if (!host->clks_on) {
		/* force the clocks to be off */
		msmsdcc_setup_clocks(host, false);
	}

	if (!(clk & MCI_CLK_ENABLE) && host->clks_on) {
		if (mmc->card && mmc->card->type == MMC_TYPE_SDIO) {
			if (!host->plat->sdiowakeup_irq) {
				writel_relaxed(MCI_SDIOINTMASK,
						host->base + MMCIMASK0);
				mb();
				if (host->plat->cfg_mpm_sdiowakeup &&
					(mmc->pm_flags & MMC_PM_WAKE_SDIO_IRQ))
					host->plat->cfg_mpm_sdiowakeup(
						mmc_dev(mmc), SDC_DAT1_ENWAKE);
				msmsdcc_enable_irq_wake(host);
			} else if (mmc->pm_flags & MMC_PM_WAKE_SDIO_IRQ) {
				writel_relaxed(0, host->base + MMCIMASK0);
			} else {
				writel_relaxed(MCI_SDIOINTMASK,
						host->base + MMCIMASK0);
			}
			mb();
		}
		msmsdcc_setup_clocks(host, false);
		host->clks_on = 0;
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

int msmsdcc_set_pwrsave(struct mmc_host *mmc, int pwrsave)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	u32 clk;

	clk = readl_relaxed(host->base + MMCICLOCK);
	pr_debug("Changing to pwr_save=%d", pwrsave);
	if (pwrsave && msmsdcc_is_pwrsave(host))
		clk |= MCI_CLK_PWRSAVE;
	else
		clk &= ~MCI_CLK_PWRSAVE;
	writel_relaxed(clk, host->base + MMCICLOCK);
	msmsdcc_delay(host);

	return 0;
}

static int msmsdcc_get_ro(struct mmc_host *mmc)
{
	int status = -ENOSYS;
	struct msmsdcc_host *host = mmc_priv(mmc);

	if (host->plat->wpswitch) {
		status = host->plat->wpswitch(mmc_dev(mmc));
	} else if (host->plat->wpswitch_gpio) {
		status = gpio_request(host->plat->wpswitch_gpio,
					"SD_WP_Switch");
		if (status) {
			pr_err("%s: %s: Failed to request GPIO %d\n",
				mmc_hostname(mmc), __func__,
				host->plat->wpswitch_gpio);
		} else {
			status = gpio_direction_input(
					host->plat->wpswitch_gpio);
			if (!status) {
				/*
				 * Wait for atleast 300ms as debounce
				 * time for GPIO input to stabilize.
				 */
				msleep(300);
				status = gpio_get_value_cansleep(
						host->plat->wpswitch_gpio);
				status ^= !host->plat->wpswitch_polarity;
			}
			gpio_free(host->plat->wpswitch_gpio);
		}
	}

	if (status < 0)
		status = -ENOSYS;
	pr_debug("%s: Card read-only status %d\n", __func__, status);

	return status;
}

#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
static void msmsdcc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;

	if (enable) {
		spin_lock_irqsave(&host->lock, flags);
		host->mci_irqenable |= MCI_SDIOINTOPERMASK;
		writel_relaxed(readl_relaxed(host->base + MMCIMASK0) |
				MCI_SDIOINTOPERMASK, host->base + MMCIMASK0);
		spin_unlock_irqrestore(&host->lock, flags);
	} else {
		host->mci_irqenable &= ~MCI_SDIOINTOPERMASK;
		writel_relaxed(readl_relaxed(host->base + MMCIMASK0) &
				~MCI_SDIOINTOPERMASK, host->base + MMCIMASK0);
	}
	mb();
}
#endif /* CONFIG_MMC_MSM_SDIO_SUPPORT */

#ifdef CONFIG_PM_RUNTIME
static int msmsdcc_enable(struct mmc_host *mmc)
{
	int rc;
	struct device *dev = mmc->parent;

	if (dev->power.runtime_status == RPM_SUSPENDING) {
		if (mmc->suspend_task == current) {
			pm_runtime_get_noresume(dev);
			goto out;
		}
	}

	rc = pm_runtime_get_sync(dev);

	if (rc < 0) {
		pr_info("%s: %s: failed with error %d", mmc_hostname(mmc),
				__func__, rc);
		return rc;
	}
out:
	return 0;
}

static int msmsdcc_disable(struct mmc_host *mmc, int lazy)
{
	int rc;
	struct msmsdcc_host *host = mmc_priv(mmc);

	if (host->plat->disable_runtime_pm)
		return -ENOTSUPP;
	if (mmc->card && mmc->card->type == MMC_TYPE_SDIO)
		return -ENOTSUPP;

	rc = pm_runtime_put_sync(mmc->parent);

	if (rc < 0)
		pr_info("%s: %s: failed with error %d", mmc_hostname(mmc),
				__func__, rc);
	return rc;
}
#else
#define msmsdcc_enable NULL
#define msmsdcc_disable NULL
#endif

static int msmsdcc_start_signal_voltage_switch(struct mmc_host *mmc,
						struct mmc_ios *ios)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;
	int rc = 0;

	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		/* Change voltage level of VDDPX to high voltage */
		rc = msmsdcc_set_vddp_high_vol(host);
		goto out;
	} else if (ios->signal_voltage != MMC_SIGNAL_VOLTAGE_180) {
		/* invalid selection. don't do anything */
		rc = -EINVAL;
		goto out;
	}

	spin_lock_irqsave(&host->lock, flags);
	/*
	 * If we are here means voltage switch from high voltage to
	 * low voltage is required
	 */

	/*
	 * Poll on MCIDATIN_3_0 and MCICMDIN bits of MCI_TEST_INPUT
	 * register until they become all zeros.
	 */
	if (readl_relaxed(host->base + MCI_TEST_INPUT) & (0xF << 1)) {
		rc = -EAGAIN;
		pr_err("%s: %s: MCIDATIN_3_0 is still not all zeros",
			mmc_hostname(mmc), __func__);
		goto out_unlock;
	}

	/* Stop SD CLK output. */
	writel_relaxed((readl_relaxed(host->base + MMCICLOCK) |
			MCI_CLK_PWRSAVE), host->base + MMCICLOCK);
	msmsdcc_delay(host);
	spin_unlock_irqrestore(&host->lock, flags);

	/*
	 * Switch VDDPX from high voltage to low voltage
	 * to change the VDD of the SD IO pads.
	 */
	rc = msmsdcc_set_vddp_low_vol(host);
	if (rc)
		goto out;

	spin_lock_irqsave(&host->lock, flags);
	writel_relaxed((readl_relaxed(host->base + MMCICLOCK) |
			IO_PAD_PWR_SWITCH), host->base + MMCICLOCK);
	msmsdcc_delay(host);
	host->io_pad_pwr_switch = 1;
	spin_unlock_irqrestore(&host->lock, flags);

	/* Wait 5 ms for the voltage regulater in the card to become stable. */
	usleep_range(5000, 5500);

	spin_lock_irqsave(&host->lock, flags);
	/* Disable PWRSAVE would make sure that SD CLK is always running */
	writel_relaxed((readl_relaxed(host->base + MMCICLOCK)
			& ~MCI_CLK_PWRSAVE), host->base + MMCICLOCK);
	msmsdcc_delay(host);
	spin_unlock_irqrestore(&host->lock, flags);

	/*
	 * If MCIDATIN_3_0 and MCICMDIN bits of MCI_TEST_INPUT register
	 * don't become all ones within 1 ms then a Voltage Switch
	 * sequence has failed and a power cycle to the card is required.
	 * Otherwise Voltage Switch sequence is completed successfully.
	 */
	usleep_range(1000, 1500);

	spin_lock_irqsave(&host->lock, flags);
	if ((readl_relaxed(host->base + MCI_TEST_INPUT) & (0xF << 1))
				!= (0xF << 1)) {
		pr_err("%s: %s: MCIDATIN_3_0 are still not all ones",
			mmc_hostname(mmc), __func__);
		rc = -EAGAIN;
		goto out_unlock;
	}

out_unlock:
	/* Enable PWRSAVE */
	writel_relaxed((readl_relaxed(host->base + MMCICLOCK) |
			MCI_CLK_PWRSAVE), host->base + MMCICLOCK);
	spin_unlock_irqrestore(&host->lock, flags);
out:
	return rc;
}

static int msmsdcc_config_cm_sdc4_dll_phase(struct msmsdcc_host *host,
						u8 phase);
/* Initialize the DLL (Programmable Delay Line ) */
static int msmsdcc_init_cm_sdc4_dll(struct msmsdcc_host *host)
{
	int rc = 0;
	u32 wait_timeout;

	/* Write 0 to DLL_PDN bit of MCI_DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~MCI_DLL_PDN), host->base + MCI_DLL_CONFIG);

	/* Write 1 to DLL_RST bit of MCI_DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_DLL_RST), host->base + MCI_DLL_CONFIG);

	msmsdcc_delay(host);

	/* Write 0 to DLL_RST bit of MCI_DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~MCI_DLL_RST), host->base + MCI_DLL_CONFIG);

	/* Initialize the phase to 0 */
	rc = msmsdcc_config_cm_sdc4_dll_phase(host, 0);
	if (rc)
		goto out;

	wait_timeout = 1000;
	/* Wait until DLL_LOCK bit of MCI_DLL_STATUS register becomes '1' */
	while (!(readl_relaxed(host->base + MCI_DLL_STATUS) & MCI_DLL_LOCK)) {
		/* max. wait for 1 sec for LOCK bit to be set */
		if (--wait_timeout == 0) {
			pr_err("%s: %s: DLL failed to lock at phase: %d",
				mmc_hostname(host->mmc), __func__, 0);
			rc = -1;
			goto out;
		}
		/* wait for 1ms */
		usleep_range(1000, 1500);
	}
out:
	return rc;
}

/*
 * Enable a CDR circuit in CM_SDC4_DLL block to enable automatic
 * calibration sequence. This function should be called before
 * enabling AUTO_CMD19 bit in MCI_CMD register for block read
 * commands (CMD17/CMD18).
 */
static void msmsdcc_enable_cdr_cm_sdc4_dll(struct msmsdcc_host *host)
{
	/* Set CDR_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG) |
			MCI_CDR_EN), host->base + MCI_DLL_CONFIG);

	/* Set CDR_EXT_EN bit to 0. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~MCI_CDR_EXT_EN), host->base + MCI_DLL_CONFIG);

	/* Set CK_OUT_EN bit to 0. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~MCI_CK_OUT_EN), host->base + MCI_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register becomes '0' */
	while (readl_relaxed(host->base + MCI_DLL_CONFIG) & MCI_CK_OUT_EN)
		;

	/* Set CK_OUT_EN bit of MCI_DLL_CONFIG register to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_CK_OUT_EN), host->base + MCI_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register is 1. */
	while (!(readl_relaxed(host->base + MCI_DLL_CONFIG) & MCI_CK_OUT_EN))
		;
}

static int msmsdcc_config_cm_sdc4_dll_phase(struct msmsdcc_host *host,
						u8 phase)
{
	int rc = 0;
	u32 mclk_freq = 0;
	u32 wait_timeout;

	/* Set CDR_EN bit to 0. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~MCI_CDR_EN), host->base + MCI_DLL_CONFIG);

	/* Set CDR_EXT_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_CDR_EXT_EN), host->base + MCI_DLL_CONFIG);

	/* Program the MCLK value to MCLK_FREQ bit field */
	if (host->clk_rate <= 112000000)
		mclk_freq = 0;
	else if (host->clk_rate <= 125000000)
		mclk_freq = 1;
	else if (host->clk_rate <= 137000000)
		mclk_freq = 2;
	else if (host->clk_rate <= 150000000)
		mclk_freq = 3;
	else if (host->clk_rate <= 162000000)
		mclk_freq = 4;
	else if (host->clk_rate <= 175000000)
		mclk_freq = 5;
	else if (host->clk_rate <= 187000000)
		mclk_freq = 6;
	else if (host->clk_rate <= 200000000)
		mclk_freq = 7;

	writel_relaxed(((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~(7 << 24)) | (mclk_freq << 24)),
			host->base + MCI_DLL_CONFIG);

	/* Set CK_OUT_EN bit to 0. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
		& ~MCI_CK_OUT_EN), host->base + MCI_DLL_CONFIG);

	/* Set DLL_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_DLL_EN), host->base + MCI_DLL_CONFIG);

	wait_timeout = 1000;
	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register becomes '0' */
	while (readl_relaxed(host->base + MCI_DLL_CONFIG) & MCI_CK_OUT_EN) {
		/* max. wait for 1 sec for LOCK bit for be set */
		if (--wait_timeout == 0) {
			pr_err("%s: %s: Failed to set DLL phase: %d, CK_OUT_EN bit is not 0",
				mmc_hostname(host->mmc), __func__, phase);
			rc = -1;
			goto out;
		}
		/* wait for 1ms */
		usleep_range(1000, 1500);
	}

	/*
	 * Write the selected DLL clock output phase (0 ... 15)
	 * to CDR_SELEXT bit field of MCI_DLL_CONFIG register.
	 */
	writel_relaxed(((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~(0xF << 20)) | (phase << 20)),
			host->base + MCI_DLL_CONFIG);

	/* Set CK_OUT_EN bit of MCI_DLL_CONFIG register to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_CK_OUT_EN), host->base + MCI_DLL_CONFIG);

	wait_timeout = 1000;
	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register becomes '1' */
	while (!(readl_relaxed(host->base + MCI_DLL_CONFIG) & MCI_CK_OUT_EN)) {
		/* max. wait for 1 sec for LOCK bit for be set */
		if (--wait_timeout == 0) {
			pr_err("%s: %s: Failed to set DLL phase: %d, CK_OUT_EN bit is not 1",
				mmc_hostname(host->mmc), __func__, phase);
			rc = -1;
			goto out;
		}
		/* wait for 1ms */
		usleep_range(1000, 1500);
	}
out:
	return rc;
}

static int msmsdcc_execute_tuning(struct mmc_host *mmc)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	u8 phase;
	u8 *data_buf;
	u8 tuned_phases[16], tuned_phase_cnt = 0;
	int rc = 0;

	/* Tuning is only required for SDR50 & SDR104 modes */
	if (!host->tuning_needed) {
		rc = 0;
		goto out;
	}

	host->cmd19_tuning_in_progress = 1;
	/*
	 * Make sure that clock is always enabled when DLL
	 * tuning is in progress. Keeping PWRSAVE ON may
	 * turn off the clock. So let's disable the PWRSAVE
	 * here and re-enable it once tuning is completed.
	 */
	writel_relaxed((readl_relaxed(host->base + MMCICLOCK)
			& ~MCI_CLK_PWRSAVE), host->base + MMCICLOCK);
	msmsdcc_delay(host);
	/* first of all reset the tuning block */
	rc = msmsdcc_init_cm_sdc4_dll(host);
	if (rc)
		goto out;

	data_buf = kmalloc(64, GFP_KERNEL);
	if (!data_buf) {
		rc = -ENOMEM;
		goto out;
	}

	phase = 0;
	do {
		struct mmc_command cmd = {0};
		struct mmc_data data = {0};
		struct mmc_request mrq = {
			.cmd = &cmd,
			.data = &data
		};
		struct scatterlist sg;

		/* set the phase in delay line hw block */
		rc = msmsdcc_config_cm_sdc4_dll_phase(host, phase);
		if (rc)
			goto kfree;

		cmd.opcode = MMC_SEND_TUNING_BLOCK;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		data.blksz = 64;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.timeout_ns = 1000 * 1000 * 1000; /* 1 sec */

		data.sg = &sg;
		data.sg_len = 1;
		sg_init_one(&sg, data_buf, 64);
		memset(data_buf, 0, 64);
		mmc_wait_for_req(mmc, &mrq);

		if (!cmd.error && !data.error &&
			!memcmp(data_buf, cmd19_tuning_block, 64)) {
			/* tuning is successful with this tuning point */
			tuned_phases[tuned_phase_cnt++] = phase;
		}
	} while (++phase < 16);

	kfree(data_buf);

	if (tuned_phase_cnt) {
		tuned_phase_cnt--;
		tuned_phase_cnt = (tuned_phase_cnt * 3) / 4;
		phase = tuned_phases[tuned_phase_cnt];
		/*
		 * Finally set the selected phase in delay
		 * line hw block.
		 */
		rc = msmsdcc_config_cm_sdc4_dll_phase(host, phase);
		if (rc)
			goto out;
	} else {
		/* tuning failed */
		rc = -EAGAIN;
		pr_err("%s: %s: no tuning point found",
			mmc_hostname(mmc), __func__);
	}
	goto out;

kfree:
	kfree(data_buf);
out:
	/* re-enable PWESAVE */
	writel_relaxed((readl_relaxed(host->base + MMCICLOCK) |
			MCI_CLK_PWRSAVE), host->base + MMCICLOCK);
	msmsdcc_delay(host);
	host->cmd19_tuning_in_progress = 0;
	return rc;
}

static const struct mmc_host_ops msmsdcc_ops = {
	.enable		= msmsdcc_enable,
	.disable	= msmsdcc_disable,
	.request	= msmsdcc_request,
	.set_ios	= msmsdcc_set_ios,
	.get_ro		= msmsdcc_get_ro,
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
	.enable_sdio_irq = msmsdcc_enable_sdio_irq,
#endif
	.start_signal_voltage_switch = msmsdcc_start_signal_voltage_switch,
	.execute_tuning = msmsdcc_execute_tuning
};

static unsigned int
msmsdcc_slot_status(struct msmsdcc_host *host)
{
	int status;
	unsigned int gpio_no = host->plat->status_gpio;

	status = gpio_request(gpio_no, "SD_HW_Detect");
	if (status) {
		pr_err("%s: %s: Failed to request GPIO %d\n",
			mmc_hostname(host->mmc), __func__, gpio_no);
	} else {
		status = gpio_direction_input(gpio_no);
		if (!status)
			status = !gpio_get_value_cansleep(gpio_no);
		gpio_free(gpio_no);
	}
	return status;
}

static void
msmsdcc_check_status(unsigned long data)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *)data;
	unsigned int status;

	if (host->plat->status || host->plat->status_gpio) {
		if (host->plat->status)
			status = host->plat->status(mmc_dev(host->mmc));
		else
			status = msmsdcc_slot_status(host);

		host->eject = !status;
		if (status ^ host->oldstat) {
			pr_info("%s: Slot status change detected (%d -> %d)\n",
			       mmc_hostname(host->mmc), host->oldstat, status);
			mmc_detect_change(host->mmc, 0);
		}
		host->oldstat = status;
	} else {
		mmc_detect_change(host->mmc, 0);
	}
}

static irqreturn_t
msmsdcc_platform_status_irq(int irq, void *dev_id)
{
	struct msmsdcc_host *host = dev_id;

	pr_debug("%s: %d\n", __func__, irq);
	msmsdcc_check_status((unsigned long) host);
	return IRQ_HANDLED;
}

static irqreturn_t
msmsdcc_platform_sdiowakeup_irq(int irq, void *dev_id)
{
	struct msmsdcc_host	*host = dev_id;

	pr_debug("%s: SDIO Wake up IRQ : %d\n", mmc_hostname(host->mmc), irq);
	spin_lock(&host->lock);
	if (!host->sdio_irq_disabled) {
		disable_irq_nosync(irq);
		if (host->mmc->pm_flags & MMC_PM_WAKE_SDIO_IRQ) {
			wake_lock(&host->sdio_wlock);
			msmsdcc_disable_irq_wake(host);
		}
		host->sdio_irq_disabled = 1;
	}
	if (host->plat->is_sdio_al_client) {
		if (!host->clks_on) {
			msmsdcc_setup_clocks(host, true);
			host->clks_on = 1;
		}
		if (host->sdcc_irq_disabled) {
			writel_relaxed(host->mci_irqenable,
				       host->base + MMCIMASK0);
			mb();
			enable_irq(host->core_irqres->start);
			host->sdcc_irq_disabled = 0;
		}
		wake_lock(&host->sdio_wlock);
	}
	spin_unlock(&host->lock);

	return IRQ_HANDLED;
}

static void
msmsdcc_status_notify_cb(int card_present, void *dev_id)
{
	struct msmsdcc_host *host = dev_id;

	pr_debug("%s: card_present %d\n", mmc_hostname(host->mmc),
	       card_present);
	msmsdcc_check_status((unsigned long) host);
}

static int
msmsdcc_init_dma(struct msmsdcc_host *host)
{
	memset(&host->dma, 0, sizeof(struct msmsdcc_dma_data));
	host->dma.host = host;
	host->dma.channel = -1;
	host->dma.crci = -1;

	if (!host->dmares)
		return -ENODEV;

	host->dma.nc = dma_alloc_coherent(NULL,
					  sizeof(struct msmsdcc_nc_dmadata),
					  &host->dma.nc_busaddr,
					  GFP_KERNEL);
	if (host->dma.nc == NULL) {
		pr_err("Unable to allocate DMA buffer\n");
		return -ENOMEM;
	}
	memset(host->dma.nc, 0x00, sizeof(struct msmsdcc_nc_dmadata));
	host->dma.cmd_busaddr = host->dma.nc_busaddr;
	host->dma.cmdptr_busaddr = host->dma.nc_busaddr +
				offsetof(struct msmsdcc_nc_dmadata, cmdptr);
	host->dma.channel = host->dmares->start;
	host->dma.crci = host->dma_crci_res->start;

	return 0;
}

#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
/**
 * Allocate and Connect a SDCC peripheral's SPS endpoint
 *
 * This function allocates endpoint context and
 * connect it with memory endpoint by calling
 * appropriate SPS driver APIs.
 *
 * Also registers a SPS callback function with
 * SPS driver
 *
 * This function should only be called once typically
 * during driver probe.
 *
 * @host - Pointer to sdcc host structure
 * @ep   - Pointer to sps endpoint data structure
 * @is_produce - 1 means Producer endpoint
 *		 0 means Consumer endpoint
 *
 * @return - 0 if successful else negative value.
 *
 */
static int msmsdcc_sps_init_ep_conn(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep,
				bool is_producer)
{
	int rc = 0;
	struct sps_pipe *sps_pipe_handle;
	struct sps_connect *sps_config = &ep->config;
	struct sps_register_event *sps_event = &ep->event;

	/* Allocate endpoint context */
	sps_pipe_handle = sps_alloc_endpoint();
	if (!sps_pipe_handle) {
		pr_err("%s: sps_alloc_endpoint() failed!!! is_producer=%d",
			   mmc_hostname(host->mmc), is_producer);
		rc = -ENOMEM;
		goto out;
	}

	/* Get default connection configuration for an endpoint */
	rc = sps_get_config(sps_pipe_handle, sps_config);
	if (rc) {
		pr_err("%s: sps_get_config() failed!!! pipe_handle=0x%x,"
			" rc=%d", mmc_hostname(host->mmc),
			(u32)sps_pipe_handle, rc);
		goto get_config_err;
	}

	/* Modify the default connection configuration */
	if (is_producer) {
		/*
		 * For SDCC producer transfer, source should be
		 * SDCC peripheral where as destination should
		 * be system memory.
		 */
		sps_config->source = host->sps.bam_handle;
		sps_config->destination = SPS_DEV_HANDLE_MEM;
		/* Producer pipe will handle this connection */
		sps_config->mode = SPS_MODE_SRC;
		sps_config->options =
			SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_ACK_TRANSFERS;
	} else {
		/*
		 * For SDCC consumer transfer, source should be
		 * system memory where as destination should
		 * SDCC peripheral
		 */
		sps_config->source = SPS_DEV_HANDLE_MEM;
		sps_config->destination = host->sps.bam_handle;
		sps_config->mode = SPS_MODE_DEST;
		sps_config->options =
			SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_ACK_TRANSFERS;
	}

	/* Producer pipe index */
	sps_config->src_pipe_index = host->sps.src_pipe_index;
	/* Consumer pipe index */
	sps_config->dest_pipe_index = host->sps.dest_pipe_index;
	/*
	 * This event thresold value is only significant for BAM-to-BAM
	 * transfer. It's ignored for BAM-to-System mode transfer.
	 */
	sps_config->event_thresh = 0x10;
	/*
	 * Max. no of scatter/gather buffers that can
	 * be passed by block layer = 32 (NR_SG).
	 * Each BAM descritor needs 64 bits (8 bytes).
	 * One BAM descriptor is required per buffer transfer.
	 * So we would require total 256 (32 * 8) bytes of descriptor FIFO.
	 * But due to HW limitation we need to allocate atleast one extra
	 * descriptor memory (256 bytes + 8 bytes). But in order to be
	 * in power of 2, we are allocating 512 bytes of memory.
	 */
	sps_config->desc.size = 512;
	sps_config->desc.base = dma_alloc_coherent(mmc_dev(host->mmc),
						sps_config->desc.size,
						&sps_config->desc.phys_base,
						GFP_KERNEL);

	if (!sps_config->desc.base) {
		rc = -ENOMEM;
		pr_err("%s: dma_alloc_coherent() failed!!! Can't allocate buffer\n"
			, mmc_hostname(host->mmc));
		goto get_config_err;
	}
	memset(sps_config->desc.base, 0x00, sps_config->desc.size);

	/* Establish connection between peripheral and memory endpoint */
	rc = sps_connect(sps_pipe_handle, sps_config);
	if (rc) {
		pr_err("%s: sps_connect() failed!!! pipe_handle=0x%x,"
			" rc=%d", mmc_hostname(host->mmc),
			(u32)sps_pipe_handle, rc);
		goto sps_connect_err;
	}

	sps_event->mode = SPS_TRIGGER_CALLBACK;
	sps_event->options = SPS_O_EOT;
	sps_event->callback = msmsdcc_sps_complete_cb;
	sps_event->xfer_done = NULL;
	sps_event->user = (void *)host;

	/* Register callback event for EOT (End of transfer) event. */
	rc = sps_register_event(sps_pipe_handle, sps_event);
	if (rc) {
		pr_err("%s: sps_connect() failed!!! pipe_handle=0x%x,"
			" rc=%d", mmc_hostname(host->mmc),
			(u32)sps_pipe_handle, rc);
		goto reg_event_err;
	}
	/* Now save the sps pipe handle */
	ep->pipe_handle = sps_pipe_handle;
	pr_debug("%s: %s, success !!! %s: pipe_handle=0x%x,"
		" desc_fifo.phys_base=0x%x\n", mmc_hostname(host->mmc),
		__func__, is_producer ? "READ" : "WRITE",
		(u32)sps_pipe_handle, sps_config->desc.phys_base);
	goto out;

reg_event_err:
	sps_disconnect(sps_pipe_handle);
sps_connect_err:
	dma_free_coherent(mmc_dev(host->mmc),
			sps_config->desc.size,
			sps_config->desc.base,
			sps_config->desc.phys_base);
get_config_err:
	sps_free_endpoint(sps_pipe_handle);
out:
	return rc;
}

/**
 * Disconnect and Deallocate a SDCC peripheral's SPS endpoint
 *
 * This function disconnect endpoint and deallocates
 * endpoint context.
 *
 * This function should only be called once typically
 * during driver remove.
 *
 * @host - Pointer to sdcc host structure
 * @ep   - Pointer to sps endpoint data structure
 *
 */
static void msmsdcc_sps_exit_ep_conn(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep)
{
	struct sps_pipe *sps_pipe_handle = ep->pipe_handle;
	struct sps_connect *sps_config = &ep->config;
	struct sps_register_event *sps_event = &ep->event;

	sps_event->xfer_done = NULL;
	sps_event->callback = NULL;
	sps_register_event(sps_pipe_handle, sps_event);
	sps_disconnect(sps_pipe_handle);
	dma_free_coherent(mmc_dev(host->mmc),
			sps_config->desc.size,
			sps_config->desc.base,
			sps_config->desc.phys_base);
	sps_free_endpoint(sps_pipe_handle);
}

/**
 * Reset SDCC peripheral's SPS endpoint
 *
 * This function disconnects an endpoint.
 *
 * This function should be called for reseting
 * SPS endpoint when data transfer error is
 * encountered during data transfer. This
 * can be considered as soft reset to endpoint.
 *
 * This function should only be called if
 * msmsdcc_sps_init() is already called.
 *
 * @host - Pointer to sdcc host structure
 * @ep   - Pointer to sps endpoint data structure
 *
 * @return - 0 if successful else negative value.
 */
static int msmsdcc_sps_reset_ep(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep)
{
	int rc = 0;
	struct sps_pipe *sps_pipe_handle = ep->pipe_handle;

	rc = sps_disconnect(sps_pipe_handle);
	if (rc) {
		pr_err("%s: %s: sps_disconnect() failed!!! pipe_handle=0x%x,"
			" rc=%d", mmc_hostname(host->mmc), __func__,
			(u32)sps_pipe_handle, rc);
		goto out;
	}
 out:
	return rc;
}

/**
 * Restore SDCC peripheral's SPS endpoint
 *
 * This function connects an endpoint.
 *
 * This function should be called for restoring
 * SPS endpoint after data transfer error is
 * encountered during data transfer. This
 * can be considered as soft reset to endpoint.
 *
 * This function should only be called if
 * msmsdcc_sps_reset_ep() is called before.
 *
 * @host - Pointer to sdcc host structure
 * @ep   - Pointer to sps endpoint data structure
 *
 * @return - 0 if successful else negative value.
 */
static int msmsdcc_sps_restore_ep(struct msmsdcc_host *host,
				struct msmsdcc_sps_ep_conn_data *ep)
{
	int rc = 0;
	struct sps_pipe *sps_pipe_handle = ep->pipe_handle;
	struct sps_connect *sps_config = &ep->config;
	struct sps_register_event *sps_event = &ep->event;

	/* Establish connection between peripheral and memory endpoint */
	rc = sps_connect(sps_pipe_handle, sps_config);
	if (rc) {
		pr_err("%s: %s: sps_connect() failed!!! pipe_handle=0x%x,"
			" rc=%d", mmc_hostname(host->mmc), __func__,
			(u32)sps_pipe_handle, rc);
		goto out;
	}

	/* Register callback event for EOT (End of transfer) event. */
	rc = sps_register_event(sps_pipe_handle, sps_event);
	if (rc) {
		pr_err("%s: %s: sps_register_event() failed!!!"
			" pipe_handle=0x%x, rc=%d",
			mmc_hostname(host->mmc), __func__,
			(u32)sps_pipe_handle, rc);
		goto reg_event_err;
	}
	goto out;

reg_event_err:
	sps_disconnect(sps_pipe_handle);
out:
	return rc;
}

/**
 * Initialize SPS HW connected with SDCC core
 *
 * This function register BAM HW resources with
 * SPS driver and then initialize 2 SPS endpoints
 *
 * This function should only be called once typically
 * during driver probe.
 *
 * @host - Pointer to sdcc host structure
 *
 * @return - 0 if successful else negative value.
 *
 */
static int msmsdcc_sps_init(struct msmsdcc_host *host)
{
	int rc = 0;
	struct sps_bam_props bam = {0};

	host->bam_base = ioremap(host->bam_memres->start,
				resource_size(host->bam_memres));
	if (!host->bam_base) {
		pr_err("%s: BAM ioremap() failed!!! phys_addr=0x%x,"
			" size=0x%x", mmc_hostname(host->mmc),
			host->bam_memres->start,
			(host->bam_memres->end -
			host->bam_memres->start));
		rc = -ENOMEM;
		goto out;
	}

	bam.phys_addr = host->bam_memres->start;
	bam.virt_addr = host->bam_base;
	/*
	 * This event thresold value is only significant for BAM-to-BAM
	 * transfer. It's ignored for BAM-to-System mode transfer.
	 */
	bam.event_threshold = 0x10;	/* Pipe event threshold */
	/*
	 * This threshold controls when the BAM publish
	 * the descriptor size on the sideband interface.
	 * SPS HW will only be used when
	 * data transfer size >  MCI_FIFOSIZE (64 bytes).
	 * PIO mode will be used when
	 * data transfer size < MCI_FIFOSIZE (64 bytes).
	 * So set this thresold value to 64 bytes.
	 */
	bam.summing_threshold = 64;
	/* SPS driver wll handle the SDCC BAM IRQ */
	bam.irq = (u32)host->bam_irqres->start;
	bam.manage = SPS_BAM_MGR_LOCAL;

	pr_info("%s: bam physical base=0x%x\n", mmc_hostname(host->mmc),
			(u32)bam.phys_addr);
	pr_info("%s: bam virtual base=0x%x\n", mmc_hostname(host->mmc),
			(u32)bam.virt_addr);

	/* Register SDCC Peripheral BAM device to SPS driver */
	rc = sps_register_bam_device(&bam, &host->sps.bam_handle);
	if (rc) {
		pr_err("%s: sps_register_bam_device() failed!!! err=%d",
			   mmc_hostname(host->mmc), rc);
		goto reg_bam_err;
	}
	pr_info("%s: BAM device registered. bam_handle=0x%x",
		mmc_hostname(host->mmc), host->sps.bam_handle);

	host->sps.src_pipe_index = SPS_SDCC_PRODUCER_PIPE_INDEX;
	host->sps.dest_pipe_index = SPS_SDCC_CONSUMER_PIPE_INDEX;

	rc = msmsdcc_sps_init_ep_conn(host, &host->sps.prod,
					SPS_PROD_PERIPHERAL);
	if (rc)
		goto sps_reset_err;
	rc = msmsdcc_sps_init_ep_conn(host, &host->sps.cons,
					SPS_CONS_PERIPHERAL);
	if (rc)
		goto cons_conn_err;

	pr_info("%s: Qualcomm MSM SDCC-BAM at 0x%016llx irq %d\n",
		mmc_hostname(host->mmc),
		(unsigned long long)host->bam_memres->start,
		(unsigned int)host->bam_irqres->start);
	goto out;

cons_conn_err:
	msmsdcc_sps_exit_ep_conn(host, &host->sps.prod);
sps_reset_err:
	sps_deregister_bam_device(host->sps.bam_handle);
reg_bam_err:
	iounmap(host->bam_base);
out:
	return rc;
}

/**
 * De-initialize SPS HW connected with SDCC core
 *
 * This function deinitialize SPS endpoints and then
 * deregisters BAM resources from SPS driver.
 *
 * This function should only be called once typically
 * during driver remove.
 *
 * @host - Pointer to sdcc host structure
 *
 */
static void msmsdcc_sps_exit(struct msmsdcc_host *host)
{
	msmsdcc_sps_exit_ep_conn(host, &host->sps.cons);
	msmsdcc_sps_exit_ep_conn(host, &host->sps.prod);
	sps_deregister_bam_device(host->sps.bam_handle);
	iounmap(host->bam_base);
}
#endif /* CONFIG_MMC_MSM_SPS_SUPPORT */

static ssize_t
show_polling(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int poll;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	poll = !!(mmc->caps & MMC_CAP_NEEDS_POLL);
	spin_unlock_irqrestore(&host->lock, flags);

	return snprintf(buf, PAGE_SIZE, "%d\n", poll);
}

static ssize_t
set_polling(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int value;
	unsigned long flags;

	sscanf(buf, "%d", &value);

	spin_lock_irqsave(&host->lock, flags);
	if (value) {
		mmc->caps |= MMC_CAP_NEEDS_POLL;
		mmc_detect_change(host->mmc, 0);
	} else {
		mmc->caps &= ~MMC_CAP_NEEDS_POLL;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	host->polling_enabled = mmc->caps & MMC_CAP_NEEDS_POLL;
#endif
	spin_unlock_irqrestore(&host->lock, flags);
	return count;
}

static DEVICE_ATTR(polling, S_IRUGO | S_IWUSR,
		show_polling, set_polling);
static struct attribute *dev_attrs[] = {
	&dev_attr_polling.attr,
	NULL,
};
static struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void msmsdcc_early_suspend(struct early_suspend *h)
{
	struct msmsdcc_host *host =
		container_of(h, struct msmsdcc_host, early_suspend);
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->polling_enabled = host->mmc->caps & MMC_CAP_NEEDS_POLL;
	host->mmc->caps &= ~MMC_CAP_NEEDS_POLL;
	spin_unlock_irqrestore(&host->lock, flags);
};
static void msmsdcc_late_resume(struct early_suspend *h)
{
	struct msmsdcc_host *host =
		container_of(h, struct msmsdcc_host, early_suspend);
	unsigned long flags;

	if (host->polling_enabled) {
		spin_lock_irqsave(&host->lock, flags);
		host->mmc->caps |= MMC_CAP_NEEDS_POLL;
		mmc_detect_change(host->mmc, 0);
		spin_unlock_irqrestore(&host->lock, flags);
	}
};
#endif

void msmsdcc_print_regs(const char *name, void __iomem *base,
			unsigned int no_of_regs)
{
	unsigned int i;

	if (!base)
		return;
	pr_info("===== %s: Register Dumps @base=0x%x =====\n",
		name, (u32)base);
	for (i = 0; i < no_of_regs; i = i + 4) {
		pr_info("Reg=0x%.2x: 0x%.8x, 0x%.8x, 0x%.8x, 0x%.8x.\n", i*4,
				(u32)readl_relaxed(base + i*4),
				(u32)readl_relaxed(base + ((i+1)*4)),
				(u32)readl_relaxed(base + ((i+2)*4)),
				(u32)readl_relaxed(base + ((i+3)*4)));
	}
}

static void msmsdcc_dump_sdcc_state(struct msmsdcc_host *host)
{
	/* Dump current state of SDCC clocks, power and irq */
	pr_info("%s: SDCC PWR is %s\n", mmc_hostname(host->mmc),
			(host->pwr ? "ON" : "OFF"));
	pr_info("%s: SDCC clks are %s, MCLK rate=%d\n",
			mmc_hostname(host->mmc),
			(host->clks_on ? "ON" : "OFF"),
			(u32)clk_get_rate(host->clk));
	pr_info("%s: SDCC irq is %s\n", mmc_hostname(host->mmc),
		(host->sdcc_irq_disabled ? "disabled" : "enabled"));

	/* Now dump SDCC registers. Don't print FIFO registers */
	if (host->clks_on)
		msmsdcc_print_regs("SDCC-CORE", host->base, 28);

	if (host->curr.data) {
		if (msmsdcc_check_dma_op_req(host->curr.data))
			pr_info("%s: PIO mode\n", mmc_hostname(host->mmc));
		else if (host->is_dma_mode)
			pr_info("%s: ADM mode: busy=%d, chnl=%d, crci=%d\n",
				mmc_hostname(host->mmc), host->dma.busy,
				host->dma.channel, host->dma.crci);
		else if (host->is_sps_mode)
			pr_info("%s: SPS mode: busy=%d\n",
				mmc_hostname(host->mmc), host->sps.busy);

		pr_info("%s: xfer_size=%d, data_xfered=%d, xfer_remain=%d\n",
			mmc_hostname(host->mmc), host->curr.xfer_size,
			host->curr.data_xfered, host->curr.xfer_remain);
		pr_info("%s: got_dataend=%d, prog_enable=%d,"
			" wait_for_auto_prog_done=%d,"
			" got_auto_prog_done=%d\n",
			mmc_hostname(host->mmc), host->curr.got_dataend,
			host->prog_enable, host->curr.wait_for_auto_prog_done,
			host->curr.got_auto_prog_done);
	}

}
static void msmsdcc_req_tout_timer_hdlr(unsigned long data)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *)data;
	struct mmc_request *mrq;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	if (host->dummy_52_sent) {
		pr_info("%s: %s: dummy CMD52 timeout\n",
				mmc_hostname(host->mmc), __func__);
		host->dummy_52_sent = 0;
	}

	mrq = host->curr.mrq;

	if (mrq && mrq->cmd) {
		pr_info("%s: CMD%d: Request timeout\n", mmc_hostname(host->mmc),
				mrq->cmd->opcode);
		msmsdcc_dump_sdcc_state(host);

		if (!mrq->cmd->error)
			mrq->cmd->error = -ETIMEDOUT;
		host->dummy_52_needed = 0;
		if (host->curr.data) {
			if (mrq->data && !mrq->data->error)
				mrq->data->error = -ETIMEDOUT;
			host->curr.data_xfered = 0;
			if (host->dma.sg && host->is_dma_mode) {
				msm_dmov_stop_cmd(host->dma.channel,
						&host->dma.hdr, 0);
			} else if (host->sps.sg && host->is_sps_mode) {
				/* Stop current SPS transfer */
				msmsdcc_sps_exit_curr_xfer(host);
			} else {
				msmsdcc_reset_and_restore(host);
				msmsdcc_stop_data(host);
				if (mrq->data && mrq->data->stop)
					msmsdcc_start_command(host,
							mrq->data->stop, 0);
				else
					msmsdcc_request_end(host, mrq);
			}
		} else {
			host->prog_enable = 0;
			msmsdcc_reset_and_restore(host);
			msmsdcc_request_end(host, mrq);
		}
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

static struct mmc_platform_data *msmsdcc_populate_pdata(struct device *dev)
{
	int i, ret;
	struct mmc_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 bus_width = 0;
	u32 *clk_table;
	int clk_table_len;
	u32 *sup_voltages;
	int sup_volt_len;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		goto err;
	}

	of_property_read_u32(np, "qcom,sdcc-bus-width", &bus_width);
	if (bus_width == 8) {
		pdata->mmc_bus_width = MMC_CAP_8_BIT_DATA;
	} else if (bus_width == 4) {
		pdata->mmc_bus_width = MMC_CAP_4_BIT_DATA;
	} else {
		dev_notice(dev, "Invalid bus width, default to 1 bit mode\n");
		pdata->mmc_bus_width = 0;
	}

	if (of_get_property(np, "qcom,sdcc-sup-voltages", &sup_volt_len)) {
		size_t sz;
		sz = sup_volt_len / sizeof(*sup_voltages);
		if (sz > 0) {
			sup_voltages = devm_kzalloc(dev,
					sz * sizeof(*sup_voltages), GFP_KERNEL);
			if (!sup_voltages) {
				dev_err(dev, "No memory for supported voltage\n");
				goto err;
			}

			ret = of_property_read_u32_array(np,
				"qcom,sdcc-sup-voltages", sup_voltages, sz);
			if (ret < 0) {
				dev_err(dev, "error while reading voltage"
						"ranges %d\n", ret);
				goto err;
			}
		} else {
			dev_err(dev, "No supported voltages\n");
			goto err;
		}
		for (i = 0; i < sz; i += 2) {
			u32 mask;

			mask = mmc_vddrange_to_ocrmask(sup_voltages[i],
					sup_voltages[i + 1]);
			if (!mask)
				dev_err(dev, "Invalide voltage range %d\n", i);
			pdata->ocr_mask |= mask;
		}
		dev_dbg(dev, "OCR mask=0x%x\n", pdata->ocr_mask);
	} else {
		dev_err(dev, "Supported voltage range not specified\n");
	}

	if (of_get_property(np, "qcom,sdcc-clk-rates", &clk_table_len)) {
		size_t sz;
		sz = clk_table_len / sizeof(*clk_table);

		if (sz > 0) {
			clk_table = devm_kzalloc(dev, sz * sizeof(*clk_table),
					GFP_KERNEL);
			if (!clk_table) {
				dev_err(dev, "No memory for clock table\n");
				goto err;
			}

			ret = of_property_read_u32_array(np,
				"qcom,sdcc-clk-rates", clk_table, sz);
			if (ret < 0) {
				dev_err(dev, "error while reading clk"
						"table %d\n", ret);
				goto err;
			}
		} else {
			dev_err(dev, "clk_table not specified\n");
			goto err;
		}
		pdata->sup_clk_table = clk_table;
		pdata->sup_clk_cnt = sz;
	} else {
		dev_err(dev, "Supported clock rates not specified\n");
	}

	if (of_get_property(np, "qcom,sdcc-nonremovable", NULL))
		pdata->nonremovable = true;
	if (of_get_property(np, "qcom,sdcc-disable_cmd23", NULL))
		pdata->disable_cmd23 = true;

	return pdata;
err:
	return NULL;
}

static int
msmsdcc_probe(struct platform_device *pdev)
{
	struct mmc_platform_data *plat;
	struct msmsdcc_host *host;
	struct mmc_host *mmc;
	unsigned long flags;
	struct resource *core_irqres = NULL;
	struct resource *bam_irqres = NULL;
	struct resource *core_memres = NULL;
	struct resource *dml_memres = NULL;
	struct resource *bam_memres = NULL;
	struct resource *dmares = NULL;
	struct resource *dma_crci_res = NULL;
	int ret = 0;
	int i;

	if (pdev->dev.of_node) {
		plat = msmsdcc_populate_pdata(&pdev->dev);
		of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &pdev->id);
	} else {
		plat = pdev->dev.platform_data;
	}

	/* must have platform data */
	if (!plat) {
		pr_err("%s: Platform data not available\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (pdev->id < 1 || pdev->id > 5)
		return -EINVAL;

	if (plat->is_sdio_al_client && !plat->sdiowakeup_irq) {
		pr_err("%s: No wakeup IRQ for sdio_al client\n", __func__);
		return -EINVAL;
	}

	if (pdev->resource == NULL || pdev->num_resources < 2) {
		pr_err("%s: Invalid resource\n", __func__);
		return -ENXIO;
	}
	if (pdev->dev.of_node) {
		/*
		 * Device tree iomem resources are only accessible by index.
		 * index = 0 -> SDCC register interface
		 * index = 1 -> DML register interface
		 * index = 2 -> BAM register interface
		 * IRQ resources:
		 * index = 0 -> SDCC IRQ
		 * index = 1 -> BAM IRQ
		 */
		core_memres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		dml_memres = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		bam_memres = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		core_irqres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
		bam_irqres = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	} else {
		for (i = 0; i < pdev->num_resources; i++) {
			if (pdev->resource[i].flags & IORESOURCE_MEM) {
				if (!strncmp(pdev->resource[i].name,
						"sdcc_dml_addr",
						sizeof("sdcc_dml_addr")))
					dml_memres = &pdev->resource[i];
				else if (!strncmp(pdev->resource[i].name,
						"sdcc_bam_addr",
						sizeof("sdcc_bam_addr")))
					bam_memres = &pdev->resource[i];
				else
					core_memres = &pdev->resource[i];

			}
			if (pdev->resource[i].flags & IORESOURCE_IRQ) {
				if (!strncmp(pdev->resource[i].name,
						"sdcc_bam_irq",
						sizeof("sdcc_bam_irq")))
					bam_irqres = &pdev->resource[i];
				else
					core_irqres = &pdev->resource[i];
			}
			if (pdev->resource[i].flags & IORESOURCE_DMA) {
				if (!strncmp(pdev->resource[i].name,
						"sdcc_dma_chnl",
						sizeof("sdcc_dma_chnl")))
					dmares = &pdev->resource[i];
				else if (!strncmp(pdev->resource[i].name,
						"sdcc_dma_crci",
						sizeof("sdcc_dma_crci")))
					dma_crci_res = &pdev->resource[i];
			}
		}
	}

	if (!core_irqres || !core_memres) {
		pr_err("%s: Invalid sdcc core resource\n", __func__);
		return -ENXIO;
	}

	/*
	 * Both BAM and DML memory resource should be preset.
	 * BAM IRQ resource should also be present.
	 */
	if ((bam_memres && !dml_memres) ||
		(!bam_memres && dml_memres) ||
		((bam_memres && dml_memres) && !bam_irqres)) {
		pr_err("%s: Invalid sdcc BAM/DML resource\n", __func__);
		return -ENXIO;
	}

	/*
	 * Setup our host structure
	 */
	mmc = mmc_alloc_host(sizeof(struct msmsdcc_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto out;
	}

	host = mmc_priv(mmc);
	host->pdev_id = pdev->id;
	host->plat = plat;
	host->mmc = mmc;
	host->curr.cmd = NULL;

	if (!plat->disable_bam && bam_memres && dml_memres && bam_irqres)
		host->is_sps_mode = 1;
	else if (dmares)
		host->is_dma_mode = 1;

	host->base = ioremap(core_memres->start,
			resource_size(core_memres));
	if (!host->base) {
		ret = -ENOMEM;
		goto host_free;
	}

	host->core_irqres = core_irqres;
	host->bam_irqres = bam_irqres;
	host->core_memres = core_memres;
	host->dml_memres = dml_memres;
	host->bam_memres = bam_memres;
	host->dmares = dmares;
	host->dma_crci_res = dma_crci_res;
	spin_lock_init(&host->lock);

#ifdef CONFIG_MMC_EMBEDDED_SDIO
	if (plat->embedded_sdio)
		mmc_set_embedded_sdio_data(mmc,
					   &plat->embedded_sdio->cis,
					   &plat->embedded_sdio->cccr,
					   plat->embedded_sdio->funcs,
					   plat->embedded_sdio->num_funcs);
#endif

	tasklet_init(&host->dma_tlet, msmsdcc_dma_complete_tlet,
			(unsigned long)host);

	tasklet_init(&host->sps.tlet, msmsdcc_sps_complete_tlet,
			(unsigned long)host);
	if (host->is_dma_mode) {
		/* Setup DMA */
		ret = msmsdcc_init_dma(host);
		if (ret)
			goto ioremap_free;
	} else {
		host->dma.channel = -1;
		host->dma.crci = -1;
	}

	/*
	 * Setup SDCC clock if derived from Dayatona
	 * fabric core clock.
	 */
	if (plat->pclk_src_dfab) {
		host->dfab_pclk = clk_get(&pdev->dev, "bus_clk");
		if (!IS_ERR(host->dfab_pclk)) {
			/* Set the clock rate to 64MHz for max. performance */
			ret = clk_set_rate(host->dfab_pclk, 64000000);
			if (ret)
				goto dfab_pclk_put;
			ret = clk_enable(host->dfab_pclk);
			if (ret)
				goto dfab_pclk_put;
		} else
			goto dma_free;
	}

	/*
	 * Setup main peripheral bus clock
	 */
	host->pclk = clk_get(&pdev->dev, "iface_clk");
	if (!IS_ERR(host->pclk)) {
		ret = clk_enable(host->pclk);
		if (ret)
			goto pclk_put;

		host->pclk_rate = clk_get_rate(host->pclk);
	}

	/*
	 * Setup SDC MMC clock
	 */
	host->clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(host->clk)) {
		ret = PTR_ERR(host->clk);
		goto pclk_disable;
	}

	ret = clk_set_rate(host->clk, msmsdcc_get_min_sup_clk_rate(host));
	if (ret) {
		pr_err("%s: Clock rate set failed (%d)\n", __func__, ret);
		goto clk_put;
	}

	ret = clk_enable(host->clk);
	if (ret)
		goto clk_put;

	host->clk_rate = clk_get_rate(host->clk);
	if (!host->clk_rate)
		dev_err(&pdev->dev, "Failed to read MCLK\n");

	/*
	* Lookup the Controller Version, to identify the supported features
	* Version number read as 0 would indicate SDCC3 or earlier versions
	*/
	host->sdcc_version = readl_relaxed(host->base + MCI_VERSION);
	pr_info("%s: mci-version: %x\n", mmc_hostname(host->mmc),
		host->sdcc_version);
	/*
	 * Set the register write delay according to min. clock frequency
	 * supported and update later when the host->clk_rate changes.
	 */
	host->reg_write_delay =
		(1 + ((3 * USEC_PER_SEC) /
		      msmsdcc_get_min_sup_clk_rate(host)));

	host->clks_on = 1;
	/* Apply Hard reset to SDCC to put it in power on default state */
	msmsdcc_hard_reset(host);

	ret = msmsdcc_vreg_init(host, true);
	if (ret) {
		pr_err("%s: msmsdcc_vreg_init() failed (%d)\n", __func__, ret);
		goto clk_disable;
	}


	/* Clocks has to be running before accessing SPS/DML HW blocks */
	if (host->is_sps_mode) {
		/* Initialize SPS */
		ret = msmsdcc_sps_init(host);
		if (ret)
			goto vreg_deinit;
		/* Initialize DML */
		ret = msmsdcc_dml_init(host);
		if (ret)
			goto sps_exit;
	}

	/*
	 * Setup MMC host structure
	 */
	mmc->ops = &msmsdcc_ops;
	mmc->f_min = msmsdcc_get_min_sup_clk_rate(host);
	mmc->f_max = msmsdcc_get_max_sup_clk_rate(host);
	mmc->ocr_avail = plat->ocr_mask;
	mmc->pm_caps |= MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;
	mmc->caps |= plat->mmc_bus_width;

	mmc->caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;
	mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;

	/*
	 * If we send the CMD23 before multi block write/read command
	 * then we need not to send CMD12 at the end of the transfer.
	 * If we don't send the CMD12 then only way to detect the PROG_DONE
	 * status is to use the AUTO_PROG_DONE status provided by SDCC4
	 * controller. So let's enable the CMD23 for SDCC4 only.
	 */
	if (!plat->disable_cmd23 && host->sdcc_version)
		mmc->caps |= MMC_CAP_CMD23;

	mmc->caps |= plat->uhs_caps;
	/*
	 * XPC controls the maximum current in the default speed mode of SDXC
	 * card. XPC=0 means 100mA (max.) but speed class is not supported.
	 * XPC=1 means 150mA (max.) and speed class is supported.
	 */
	if (plat->xpc_cap)
		mmc->caps |= (MMC_CAP_SET_XPC_330 | MMC_CAP_SET_XPC_300 |
				MMC_CAP_SET_XPC_180);

	if (plat->nonremovable)
		mmc->caps |= MMC_CAP_NONREMOVABLE;
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
	mmc->caps |= MMC_CAP_SDIO_IRQ;
#endif

	if (plat->is_sdio_al_client)
		mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;

	mmc->max_segs = NR_SG;
	mmc->max_blk_size = 4096;	/* MCI_DATA_CTL BLOCKSIZE up to 4096 */
	mmc->max_blk_count = 65535;

	mmc->max_req_size = 33554432;	/* MCI_DATA_LENGTH is 25 bits */
	mmc->max_seg_size = mmc->max_req_size;

	writel_relaxed(0, host->base + MMCIMASK0);
	writel_relaxed(MCI_CLEAR_STATIC_MASK, host->base + MMCICLEAR);

	writel_relaxed(MCI_IRQENABLE, host->base + MMCIMASK0);
	mb();
	host->mci_irqenable = MCI_IRQENABLE;

	ret = request_irq(core_irqres->start, msmsdcc_irq, IRQF_SHARED,
			  DRIVER_NAME " (cmd)", host);
	if (ret)
		goto dml_exit;

	ret = request_irq(core_irqres->start, msmsdcc_pio_irq, IRQF_SHARED,
			  DRIVER_NAME " (pio)", host);
	if (ret)
		goto irq_free;

	/*
	 * Enable SDCC IRQ only when host is powered on. Otherwise, this
	 * IRQ is un-necessarily being monitored by MPM (Modem power
	 * management block) during idle-power collapse.  The MPM will be
	 * configured to monitor the DATA1 GPIO line with level-low trigger
	 * and thus depending on the GPIO status, it prevents TCXO shutdown
	 * during idle-power collapse.
	 */
	disable_irq(core_irqres->start);
	host->sdcc_irq_disabled = 1;

	if (plat->sdiowakeup_irq) {
		wake_lock_init(&host->sdio_wlock, WAKE_LOCK_SUSPEND,
				mmc_hostname(mmc));
		ret = request_irq(plat->sdiowakeup_irq,
			msmsdcc_platform_sdiowakeup_irq,
			IRQF_SHARED | IRQF_TRIGGER_LOW,
			DRIVER_NAME "sdiowakeup", host);
		if (ret) {
			pr_err("Unable to get sdio wakeup IRQ %d (%d)\n",
				plat->sdiowakeup_irq, ret);
			goto pio_irq_free;
		} else {
			spin_lock_irqsave(&host->lock, flags);
			if (!host->sdio_irq_disabled) {
				disable_irq_nosync(plat->sdiowakeup_irq);
				host->sdio_irq_disabled = 1;
			}
			spin_unlock_irqrestore(&host->lock, flags);
		}
	}

	if (plat->cfg_mpm_sdiowakeup) {
		wake_lock_init(&host->sdio_wlock, WAKE_LOCK_SUSPEND,
				mmc_hostname(mmc));
	}

	wake_lock_init(&host->sdio_suspend_wlock, WAKE_LOCK_SUSPEND,
			mmc_hostname(mmc));
	/*
	 * Setup card detect change
	 */

	if (plat->status || plat->status_gpio) {
		if (plat->status)
			host->oldstat = plat->status(mmc_dev(host->mmc));
		else
			host->oldstat = msmsdcc_slot_status(host);
		host->eject = !host->oldstat;
	}

	if (plat->status_irq) {
		ret = request_threaded_irq(plat->status_irq, NULL,
				  msmsdcc_platform_status_irq,
				  plat->irq_flags,
				  DRIVER_NAME " (slot)",
				  host);
		if (ret) {
			pr_err("Unable to get slot IRQ %d (%d)\n",
			       plat->status_irq, ret);
			goto sdiowakeup_irq_free;
		}
	} else if (plat->register_status_notify) {
		plat->register_status_notify(msmsdcc_status_notify_cb, host);
	} else if (!plat->status)
		pr_err("%s: No card detect facilities available\n",
		       mmc_hostname(mmc));

	mmc_set_drvdata(pdev, mmc);

	ret = pm_runtime_set_active(&(pdev)->dev);
	if (ret < 0)
		pr_info("%s: %s: failed with error %d", mmc_hostname(mmc),
				__func__, ret);
	/*
	 * There is no notion of suspend/resume for SD/MMC/SDIO
	 * cards. So host can be suspended/resumed with out
	 * worrying about its children.
	 */
	pm_suspend_ignore_children(&(pdev)->dev, true);

	/*
	 * MMC/SD/SDIO bus suspend/resume operations are defined
	 * only for the slots that will be used for non-removable
	 * media or for all slots when CONFIG_MMC_UNSAFE_RESUME is
	 * defined. Otherwise, they simply become card removal and
	 * insertion events during suspend and resume respectively.
	 * Hence, enable run-time PM only for slots for which bus
	 * suspend/resume operations are defined.
	 */
#ifdef CONFIG_MMC_UNSAFE_RESUME
	/*
	 * If this capability is set, MMC core will enable/disable host
	 * for every claim/release operation on a host. We use this
	 * notification to increment/decrement runtime pm usage count.
	 */
	mmc->caps |= MMC_CAP_DISABLE;
	pm_runtime_enable(&(pdev)->dev);
#else
	if (mmc->caps & MMC_CAP_NONREMOVABLE) {
		mmc->caps |= MMC_CAP_DISABLE;
		pm_runtime_enable(&(pdev)->dev);
	}
#endif
	setup_timer(&host->req_tout_timer, msmsdcc_req_tout_timer_hdlr,
			(unsigned long)host);

	mmc_add_host(mmc);

#ifdef CONFIG_HAS_EARLYSUSPEND
	host->early_suspend.suspend = msmsdcc_early_suspend;
	host->early_suspend.resume  = msmsdcc_late_resume;
	host->early_suspend.level   = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&host->early_suspend);
#endif

	pr_info("%s: Qualcomm MSM SDCC-core at 0x%016llx irq %d,%d dma %d"
		" dmacrcri %d\n", mmc_hostname(mmc),
		(unsigned long long)core_memres->start,
		(unsigned int) core_irqres->start,
		(unsigned int) plat->status_irq, host->dma.channel,
		host->dma.crci);

	pr_info("%s: 8 bit data mode %s\n", mmc_hostname(mmc),
		(mmc->caps & MMC_CAP_8_BIT_DATA ? "enabled" : "disabled"));
	pr_info("%s: 4 bit data mode %s\n", mmc_hostname(mmc),
	       (mmc->caps & MMC_CAP_4_BIT_DATA ? "enabled" : "disabled"));
	pr_info("%s: polling status mode %s\n", mmc_hostname(mmc),
	       (mmc->caps & MMC_CAP_NEEDS_POLL ? "enabled" : "disabled"));
	pr_info("%s: MMC clock %u -> %u Hz, PCLK %u Hz\n",
	       mmc_hostname(mmc), msmsdcc_get_min_sup_clk_rate(host),
		msmsdcc_get_max_sup_clk_rate(host), host->pclk_rate);
	pr_info("%s: Slot eject status = %d\n", mmc_hostname(mmc),
	       host->eject);
	pr_info("%s: Power save feature enable = %d\n",
	       mmc_hostname(mmc), msmsdcc_pwrsave);

	if (host->is_dma_mode && host->dma.channel != -1
			&& host->dma.crci != -1) {
		pr_info("%s: DM non-cached buffer at %p, dma_addr 0x%.8x\n",
		       mmc_hostname(mmc), host->dma.nc, host->dma.nc_busaddr);
		pr_info("%s: DM cmd busaddr 0x%.8x, cmdptr busaddr 0x%.8x\n",
		       mmc_hostname(mmc), host->dma.cmd_busaddr,
		       host->dma.cmdptr_busaddr);
	} else if (host->is_sps_mode) {
		pr_info("%s: SPS-BAM data transfer mode available\n",
			mmc_hostname(mmc));
	} else
		pr_info("%s: PIO transfer enabled\n", mmc_hostname(mmc));

#if defined(CONFIG_DEBUG_FS)
	msmsdcc_dbg_createhost(host);
#endif
	if (!plat->status_irq) {
		ret = sysfs_create_group(&pdev->dev.kobj, &dev_attr_grp);
		if (ret)
			goto platform_irq_free;
	}
	return 0;

 platform_irq_free:
	del_timer_sync(&host->req_tout_timer);
	pm_runtime_disable(&(pdev)->dev);
	pm_runtime_set_suspended(&(pdev)->dev);

	if (plat->status_irq)
		free_irq(plat->status_irq, host);
 sdiowakeup_irq_free:
	wake_lock_destroy(&host->sdio_suspend_wlock);
	if (plat->sdiowakeup_irq)
		free_irq(plat->sdiowakeup_irq, host);
 pio_irq_free:
	if (plat->sdiowakeup_irq)
		wake_lock_destroy(&host->sdio_wlock);
	free_irq(core_irqres->start, host);
 irq_free:
	free_irq(core_irqres->start, host);
 dml_exit:
	if (host->is_sps_mode)
		msmsdcc_dml_exit(host);
 sps_exit:
	if (host->is_sps_mode)
		msmsdcc_sps_exit(host);
 vreg_deinit:
	msmsdcc_vreg_init(host, false);
 clk_disable:
	clk_disable(host->clk);
 clk_put:
	clk_put(host->clk);
 pclk_disable:
	if (!IS_ERR(host->pclk))
		clk_disable(host->pclk);
 pclk_put:
	if (!IS_ERR(host->pclk))
		clk_put(host->pclk);
	if (!IS_ERR_OR_NULL(host->dfab_pclk))
		clk_disable(host->dfab_pclk);
 dfab_pclk_put:
	if (!IS_ERR_OR_NULL(host->dfab_pclk))
		clk_put(host->dfab_pclk);
 dma_free:
	if (host->is_dma_mode) {
		if (host->dmares)
			dma_free_coherent(NULL,
				sizeof(struct msmsdcc_nc_dmadata),
				host->dma.nc, host->dma.nc_busaddr);
	}
 ioremap_free:
	iounmap(host->base);
 host_free:
	mmc_free_host(mmc);
 out:
	return ret;
}

static int msmsdcc_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = mmc_get_drvdata(pdev);
	struct mmc_platform_data *plat;
	struct msmsdcc_host *host;

	if (!mmc)
		return -ENXIO;

	if (pm_runtime_suspended(&(pdev)->dev))
		pm_runtime_resume(&(pdev)->dev);

	host = mmc_priv(mmc);

	DBG(host, "Removing SDCC device = %d\n", pdev->id);
	plat = host->plat;

	if (!plat->status_irq)
		sysfs_remove_group(&pdev->dev.kobj, &dev_attr_grp);

	del_timer_sync(&host->req_tout_timer);
	tasklet_kill(&host->dma_tlet);
	tasklet_kill(&host->sps.tlet);
	mmc_remove_host(mmc);

	if (plat->status_irq)
		free_irq(plat->status_irq, host);

	wake_lock_destroy(&host->sdio_suspend_wlock);
	if (plat->sdiowakeup_irq) {
		wake_lock_destroy(&host->sdio_wlock);
		irq_set_irq_wake(plat->sdiowakeup_irq, 0);
		free_irq(plat->sdiowakeup_irq, host);
	}

	free_irq(host->core_irqres->start, host);
	free_irq(host->core_irqres->start, host);

	clk_put(host->clk);
	if (!IS_ERR(host->pclk))
		clk_put(host->pclk);
	if (!IS_ERR_OR_NULL(host->dfab_pclk))
		clk_put(host->dfab_pclk);

	msmsdcc_vreg_init(host, false);

	if (host->is_dma_mode) {
		if (host->dmares)
			dma_free_coherent(NULL,
					sizeof(struct msmsdcc_nc_dmadata),
					host->dma.nc, host->dma.nc_busaddr);
	}

	if (host->is_sps_mode) {
		msmsdcc_dml_exit(host);
		msmsdcc_sps_exit(host);
	}

	iounmap(host->base);
	mmc_free_host(mmc);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&host->early_suspend);
#endif
	pm_runtime_disable(&(pdev)->dev);
	pm_runtime_set_suspended(&(pdev)->dev);

	return 0;
}

#ifdef CONFIG_MSM_SDIO_AL
int msmsdcc_sdio_al_lpm(struct mmc_host *mmc, bool enable)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	pr_debug("%s: %sabling LPM\n", mmc_hostname(mmc),
			enable ? "En" : "Dis");

	if (enable) {
		if (!host->sdcc_irq_disabled) {
			writel_relaxed(0, host->base + MMCIMASK0);
			disable_irq_nosync(host->core_irqres->start);
			host->sdcc_irq_disabled = 1;
		}

		if (host->clks_on) {
			msmsdcc_setup_clocks(host, false);
			host->clks_on = 0;
		}

		if (host->plat->sdio_lpm_gpio_setup &&
				!host->sdio_gpio_lpm) {
			spin_unlock_irqrestore(&host->lock, flags);
			host->plat->sdio_lpm_gpio_setup(mmc_dev(mmc), 0);
			spin_lock_irqsave(&host->lock, flags);
			host->sdio_gpio_lpm = 1;
		}

		if (host->sdio_irq_disabled) {
			msmsdcc_enable_irq_wake(host);
			enable_irq(host->plat->sdiowakeup_irq);
			host->sdio_irq_disabled = 0;
		}
	} else {
		if (!host->sdio_irq_disabled) {
			disable_irq_nosync(host->plat->sdiowakeup_irq);
			host->sdio_irq_disabled = 1;
			msmsdcc_disable_irq_wake(host);
		}

		if (host->plat->sdio_lpm_gpio_setup &&
				host->sdio_gpio_lpm) {
			spin_unlock_irqrestore(&host->lock, flags);
			host->plat->sdio_lpm_gpio_setup(mmc_dev(mmc), 1);
			spin_lock_irqsave(&host->lock, flags);
			host->sdio_gpio_lpm = 0;
		}

		if (!host->clks_on) {
			msmsdcc_setup_clocks(host, true);
			host->clks_on = 1;
		}

		if (host->sdcc_irq_disabled) {
			writel_relaxed(host->mci_irqenable,
				       host->base + MMCIMASK0);
			mb();
			enable_irq(host->core_irqres->start);
			host->sdcc_irq_disabled = 0;
		}
		wake_lock_timeout(&host->sdio_wlock, 1);
	}
	spin_unlock_irqrestore(&host->lock, flags);
	return 0;
}
#else
int msmsdcc_sdio_al_lpm(struct mmc_host *mmc, bool enable)
{
	return 0;
}
#endif

#ifdef CONFIG_PM
static int
msmsdcc_runtime_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;

	if (host->plat->is_sdio_al_client)
		return 0;

	pr_debug("%s: %s: start\n", mmc_hostname(mmc), __func__);
	if (mmc) {
		host->sdcc_suspending = 1;
		mmc->suspend_task = current;

		/*
		 * If the clocks are already turned off by SDIO clients (as
		 * part of LPM), then clocks should be turned on before
		 * calling mmc_suspend_host() because mmc_suspend_host might
		 * send some commands to the card. The clocks will be turned
		 * off again after mmc_suspend_host. Thus for SDIO
		 * cards, clocks will be turned on before mmc_suspend_host
		 * and turned off after mmc_suspend_host.
		 */
		if (mmc->card && mmc_card_sdio(mmc->card)) {
			mmc->ios.clock = host->clk_rate;
			mmc->ops->set_ios(host->mmc, &host->mmc->ios);
		}

		/*
		 * MMC core thinks that host is disabled by now since
		 * runtime suspend is scheduled after msmsdcc_disable()
		 * is called. Thus, MMC core will try to enable the host
		 * while suspending it. This results in a synchronous
		 * runtime resume request while in runtime suspending
		 * context and hence inorder to complete this resume
		 * requet, it will wait for suspend to be complete,
		 * but runtime suspend also can not proceed further
		 * until the host is resumed. Thus, it leads to a hang.
		 * Hence, increase the pm usage count before suspending
		 * the host so that any resume requests after this will
		 * simple become pm usage counter increment operations.
		 */
		pm_runtime_get_noresume(dev);
		rc = mmc_suspend_host(mmc);
		pm_runtime_put_noidle(dev);

		if (!rc) {
			if (mmc->card && (mmc->card->type == MMC_TYPE_SDIO) &&
				(mmc->pm_flags & MMC_PM_WAKE_SDIO_IRQ)) {
				disable_irq(host->core_irqres->start);
				host->sdcc_irq_disabled = 1;

				/*
				 * If MMC core level suspend is not supported,
				 * turn off clocks to allow deep sleep (TCXO
				 * shutdown).
				 */
				mmc->ios.clock = 0;
				mmc->ops->set_ios(host->mmc, &host->mmc->ios);
				enable_irq(host->core_irqres->start);
				host->sdcc_irq_disabled = 0;

				if (host->plat->sdiowakeup_irq) {
					host->sdio_irq_disabled = 0;
					msmsdcc_enable_irq_wake(host);
					enable_irq(host->plat->sdiowakeup_irq);
				}
			}
		}
		host->sdcc_suspending = 0;
		mmc->suspend_task = NULL;
		if (rc && wake_lock_active(&host->sdio_suspend_wlock))
			wake_unlock(&host->sdio_suspend_wlock);
	}
	pr_debug("%s: %s: end\n", mmc_hostname(mmc), __func__);
	return rc;
}

static int
msmsdcc_runtime_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;

	if (host->plat->is_sdio_al_client)
		return 0;

	pr_debug("%s: %s: start\n", mmc_hostname(mmc), __func__);
	if (mmc) {
		if (mmc->card && mmc->card->type == MMC_TYPE_SDIO) {
			if (host->sdcc_irq_disabled) {
				enable_irq(host->core_irqres->start);
				host->sdcc_irq_disabled = 0;
			}
			mmc->ios.clock = host->clk_rate;
			mmc->ops->set_ios(host->mmc, &host->mmc->ios);

			spin_lock_irqsave(&host->lock, flags);
			writel_relaxed(host->mci_irqenable,
					host->base + MMCIMASK0);
			mb();

			if ((mmc->pm_flags & MMC_PM_WAKE_SDIO_IRQ) &&
					!host->sdio_irq_disabled) {
				if (host->plat->sdiowakeup_irq) {
					disable_irq_nosync(
						host->plat->sdiowakeup_irq);
					msmsdcc_disable_irq_wake(host);
					host->sdio_irq_disabled = 1;
				}
			}

			spin_unlock_irqrestore(&host->lock, flags);
		}

		mmc_resume_host(mmc);

		/*
		 * FIXME: Clearing of flags must be handled in clients
		 * resume handler.
		 */
		spin_lock_irqsave(&host->lock, flags);
		mmc->pm_flags = 0;
		spin_unlock_irqrestore(&host->lock, flags);

		/*
		 * After resuming the host wait for sometime so that
		 * the SDIO work will be processed.
		 */
		if (mmc->card && (mmc->card->type == MMC_TYPE_SDIO)) {
			if ((host->plat->cfg_mpm_sdiowakeup ||
					host->plat->sdiowakeup_irq) &&
					wake_lock_active(&host->sdio_wlock))
				wake_lock_timeout(&host->sdio_wlock, 1);
		}

		wake_unlock(&host->sdio_suspend_wlock);
	}
	pr_debug("%s: %s: end\n", mmc_hostname(mmc), __func__);
	return 0;
}

static int msmsdcc_runtime_idle(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);

	if (host->plat->is_sdio_al_client)
		return 0;

	/* Idle timeout is not configurable for now */
	pm_schedule_suspend(dev, MSM_MMC_IDLE_TIMEOUT);

	return -EAGAIN;
}

static int msmsdcc_pm_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;

	if (host->plat->is_sdio_al_client)
		return 0;


	if (host->plat->status_irq)
		disable_irq(host->plat->status_irq);

	if (!pm_runtime_suspended(dev))
		rc = msmsdcc_runtime_suspend(dev);

	return rc;
}

static int msmsdcc_pm_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;

	if (host->plat->is_sdio_al_client)
		return 0;

	if (!pm_runtime_suspended(dev))
		rc = msmsdcc_runtime_resume(dev);
	if (host->plat->status_irq) {
		msmsdcc_check_status((unsigned long)host);
		enable_irq(host->plat->status_irq);
	}

	return rc;
}

#else
#define msmsdcc_runtime_suspend NULL
#define msmsdcc_runtime_resume NULL
#define msmsdcc_runtime_idle NULL
#define msmsdcc_pm_suspend NULL
#define msmsdcc_pm_resume NULL
#endif

static const struct dev_pm_ops msmsdcc_dev_pm_ops = {
	.runtime_suspend = msmsdcc_runtime_suspend,
	.runtime_resume  = msmsdcc_runtime_resume,
	.runtime_idle    = msmsdcc_runtime_idle,
	.suspend 	 = msmsdcc_pm_suspend,
	.resume		 = msmsdcc_pm_resume,
};

static const struct of_device_id msmsdcc_dt_match[] = {
	{.compatible = "qcom,msm-sdcc"},

};
MODULE_DEVICE_TABLE(of, msmsdcc_dt_match);

static struct platform_driver msmsdcc_driver = {
	.probe		= msmsdcc_probe,
	.remove		= msmsdcc_remove,
	.driver		= {
		.name	= "msm_sdcc",
		.pm	= &msmsdcc_dev_pm_ops,
		.of_match_table = msmsdcc_dt_match,
	},
};

static int __init msmsdcc_init(void)
{
#if defined(CONFIG_DEBUG_FS)
	int ret = 0;
	ret = msmsdcc_dbg_init();
	if (ret) {
		pr_err("Failed to create debug fs dir \n");
		return ret;
	}
#endif
	return platform_driver_register(&msmsdcc_driver);
}

static void __exit msmsdcc_exit(void)
{
	platform_driver_unregister(&msmsdcc_driver);

#if defined(CONFIG_DEBUG_FS)
	debugfs_remove(debugfs_file);
	debugfs_remove(debugfs_dir);
#endif
}

module_init(msmsdcc_init);
module_exit(msmsdcc_exit);

MODULE_DESCRIPTION("Qualcomm Multimedia Card Interface driver");
MODULE_LICENSE("GPL");

#if defined(CONFIG_DEBUG_FS)

static int
msmsdcc_dbg_state_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t
msmsdcc_dbg_state_read(struct file *file, char __user *ubuf,
		       size_t count, loff_t *ppos)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *) file->private_data;
	char buf[1024];
	int max, i;

	i = 0;
	max = sizeof(buf) - 1;

	i += scnprintf(buf + i, max - i, "STAT: %p %p %p\n", host->curr.mrq,
		       host->curr.cmd, host->curr.data);
	if (host->curr.cmd) {
		struct mmc_command *cmd = host->curr.cmd;

		i += scnprintf(buf + i, max - i, "CMD : %.8x %.8x %.8x\n",
			      cmd->opcode, cmd->arg, cmd->flags);
	}
	if (host->curr.data) {
		struct mmc_data *data = host->curr.data;
		i += scnprintf(buf + i, max - i,
			      "DAT0: %.8x %.8x %.8x %.8x %.8x %.8x\n",
			      data->timeout_ns, data->timeout_clks,
			      data->blksz, data->blocks, data->error,
			      data->flags);
		i += scnprintf(buf + i, max - i, "DAT1: %.8x %.8x %.8x %p\n",
			      host->curr.xfer_size, host->curr.xfer_remain,
			      host->curr.data_xfered, host->dma.sg);
	}

	return simple_read_from_buffer(ubuf, count, ppos, buf, i);
}

static const struct file_operations msmsdcc_dbg_state_ops = {
	.read	= msmsdcc_dbg_state_read,
	.open	= msmsdcc_dbg_state_open,
};

static void msmsdcc_dbg_createhost(struct msmsdcc_host *host)
{
	if (debugfs_dir) {
		debugfs_file = debugfs_create_file(mmc_hostname(host->mmc),
							0644, debugfs_dir, host,
							&msmsdcc_dbg_state_ops);
	}
}

static int __init msmsdcc_dbg_init(void)
{
	int err;

	debugfs_dir = debugfs_create_dir("msmsdcc", 0);
	if (IS_ERR(debugfs_dir)) {
		err = PTR_ERR(debugfs_dir);
		debugfs_dir = NULL;
		return err;
	}

	return 0;
}
#endif
