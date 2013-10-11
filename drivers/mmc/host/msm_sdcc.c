/*
 *  linux/drivers/mmc/host/msm_sdcc.c - Qualcomm MSM 7X00A SDCC Driver
 *
 *  Copyright (C) 2007 Google Inc,
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *  Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 *
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
#include <linux/of_gpio.h>
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
#include <linux/pm_qos.h>
#include <linux/iopoll.h>

#include <asm/cacheflush.h>
#include <asm/div64.h>
#include <asm/sizes.h>

#include <asm/mach/mmc.h>
#include <mach/msm_iomap.h>
#include <mach/clk.h>
#include <mach/dma.h>
#include <mach/sdio_al.h>
#include <mach/mpm.h>
#include <mach/msm_bus.h>

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
/* Use SPS only if transfer size is more than this macro */
#define SPS_MIN_XFER_SIZE		MCI_FIFOSIZE

#define MSM_MMC_BUS_VOTING_DELAY	200 /* msecs */
#define INVALID_TUNING_PHASE		-1

#if defined(CONFIG_DEBUG_FS)
static void msmsdcc_dbg_createhost(struct msmsdcc_host *);
static struct dentry *debugfs_dir;
static int  msmsdcc_dbg_init(void);
#endif

static int msmsdcc_prep_xfer(struct msmsdcc_host *host, struct mmc_data
			     *data);
static void msmsdcc_msm_bus_cancel_work_and_set_vote(struct msmsdcc_host *host,
						struct mmc_ios *ios);
static void msmsdcc_msm_bus_queue_work(struct msmsdcc_host *host);


static u64 dma_mask = DMA_BIT_MASK(32);
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
static const u32 tuning_block_64[] = {
	0x00FF0FFF, 0xCCC3CCFF, 0xFFCC3CC3, 0xEFFEFFFE,
	0xDDFFDFFF, 0xFBFFFBFF, 0xFF7FFFBF, 0xEFBDF777,
	0xF0FFF0FF, 0x3CCCFC0F, 0xCFCC33CC, 0xEEFFEFFF,
	0xFDFFFDFF, 0xFFBFFFDF, 0xFFF7FFBB, 0xDE7B7FF7
};

static const u32 tuning_block_128[] = {
	0xFF00FFFF, 0x0000FFFF, 0xCCCCFFFF, 0xCCCC33CC,
	0xCC3333CC, 0xFFFFCCCC, 0xFFFFEEFF, 0xFFEEEEFF,
	0xFFDDFFFF, 0xDDDDFFFF, 0xBBFFFFFF, 0xBBFFFFFF,
	0xFFFFFFBB, 0xFFFFFF77, 0x77FF7777, 0xFFEEDDBB,
	0x00FFFFFF, 0x00FFFFFF, 0xCCFFFF00, 0xCC33CCCC,
	0x3333CCCC, 0xFFCCCCCC, 0xFFEEFFFF, 0xEEEEFFFF,
	0xDDFFFFFF, 0xDDFFFFFF, 0xFFFFFFDD, 0xFFFFFFBB,
	0xFFFFBBBB, 0xFFFF77FF, 0xFF7777FF, 0xEEDDBB77
};

static int disable_slots;
module_param(disable_slots, int, 0);

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
static inline void msmsdcc_sync_reg_wr(struct msmsdcc_host *host);
static inline void msmsdcc_delay(struct msmsdcc_host *host);
static void msmsdcc_dump_sdcc_state(struct msmsdcc_host *host);
static void msmsdcc_sg_start(struct msmsdcc_host *host);
static int msmsdcc_vreg_reset(struct msmsdcc_host *host);
static int msmsdcc_runtime_resume(struct device *dev);
static int msmsdcc_dt_get_array(struct device *dev, const char *prop_name,
		u32 **out_array, int *len, int size);
static int msmsdcc_execute_tuning(struct mmc_host *mmc, u32 opcode);
static bool msmsdcc_is_wait_for_auto_prog_done(struct msmsdcc_host *host,
					       struct mmc_request *mrq);
static bool msmsdcc_is_wait_for_prog_done(struct msmsdcc_host *host,
					  struct mmc_request *mrq);

static inline unsigned short msmsdcc_get_nr_sg(struct msmsdcc_host *host)
{
	unsigned short ret = NR_SG;

	if (is_sps_mode(host)) {
		ret = SPS_MAX_DESCS;
	} else { /* DMA or PIO mode */
		if (NR_SG > MAX_NR_SG_DMA_PIO)
			ret = MAX_NR_SG_DMA_PIO;
	}

	return ret;
}

/* Prevent idle power collapse(pc) while operating in peripheral mode */
static void msmsdcc_pm_qos_update_latency(struct msmsdcc_host *host, int vote)
{
	if (!host->cpu_dma_latency)
		return;

	if (vote)
		pm_qos_update_request(&host->pm_qos_req_dma,
				host->cpu_dma_latency);
	else
		pm_qos_update_request(&host->pm_qos_req_dma,
					PM_QOS_DEFAULT_VALUE);
}

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
 * Apply reset
 *
 * This function resets SPS BAM and DML cores.
 *
 * This function should be called to recover from error
 * conditions encountered during CMD/DATA tranfsers with card.
 *
 * @host - Pointer to driver's host structure
 *
 */
static int msmsdcc_bam_dml_reset_and_restore(struct msmsdcc_host *host)
{
	int rc;

	/* Reset all SDCC BAM pipes */
	rc = msmsdcc_sps_reset_ep(host, &host->sps.prod);
	if (rc) {
		pr_err("%s: msmsdcc_sps_reset_ep(prod) error=%d\n",
				mmc_hostname(host->mmc), rc);
		goto out;
	}

	rc = msmsdcc_sps_reset_ep(host, &host->sps.cons);
	if (rc) {
		pr_err("%s: msmsdcc_sps_reset_ep(cons) error=%d\n",
				mmc_hostname(host->mmc), rc);
		goto out;
	}

	/* Reset BAM */
	rc = sps_device_reset(host->sps.bam_handle);
	if (rc) {
		pr_err("%s: sps_device_reset error=%d\n",
				mmc_hostname(host->mmc), rc);
		goto out;
	}

	memset(host->sps.prod.config.desc.base, 0x00,
			host->sps.prod.config.desc.size);
	memset(host->sps.cons.config.desc.base, 0x00,
			host->sps.cons.config.desc.size);

	/* Restore all BAM pipes connections */
	rc = msmsdcc_sps_restore_ep(host, &host->sps.prod);
	if (rc) {
		pr_err("%s: msmsdcc_sps_restore_ep(prod) error=%d\n",
				mmc_hostname(host->mmc), rc);
		goto out;
	}

	rc = msmsdcc_sps_restore_ep(host, &host->sps.cons);
	if (rc) {
		pr_err("%s: msmsdcc_sps_restore_ep(cons) error=%d\n",
				mmc_hostname(host->mmc), rc);
		goto out;
	}

	/* Reset and init DML */
	rc = msmsdcc_dml_init(host);
	if (rc)
		pr_err("%s: msmsdcc_dml_init error=%d\n",
				mmc_hostname(host->mmc), rc);

out:
	if (!rc)
		host->sps.reset_bam = false;
	return rc;
}

/**
 * Apply soft reset
 *
 * This function applies soft reset to SDCC core.
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
	 * Reset controller state machines without resetting
	 * configuration registers (MCI_POWER, MCI_CLK, MCI_INT_MASKn).
	 */
	if (is_sw_reset_save_config(host)) {
		ktime_t start;
		uint32_t dll_config = 0;


		if (is_sw_reset_save_config_broken(host))
			dll_config = readl_relaxed(host->base + MCI_DLL_CONFIG);

		writel_relaxed(readl_relaxed(host->base + MMCIPOWER)
				| MCI_SW_RST_CFG, host->base + MMCIPOWER);
		msmsdcc_sync_reg_wr(host);

		start = ktime_get();
		while (readl_relaxed(host->base + MMCIPOWER) & MCI_SW_RST_CFG) {
			/*
			 * SW reset can take upto 10HCLK + 15MCLK cycles.
			 * Calculating based on min clk rates (hclk = 27MHz,
			 * mclk = 400KHz) it comes to ~40us. Let's poll for
			 * max. 1ms for reset completion.
			 */
			if (ktime_to_us(ktime_sub(ktime_get(), start)) > 1000) {
				pr_err("%s: %s failed\n",
					mmc_hostname(host->mmc), __func__);
				BUG();
			}
		}

		if (is_sw_reset_save_config_broken(host)) {
			writel_relaxed(dll_config, host->base + MCI_DLL_CONFIG);
			mb();
		}
	} else {
		writel_relaxed(0, host->base + MMCICOMMAND);
		msmsdcc_sync_reg_wr(host);
		writel_relaxed(0, host->base + MMCIDATACTRL);
		msmsdcc_sync_reg_wr(host);
	}
}

static void msmsdcc_hard_reset(struct msmsdcc_host *host)
{
	int ret;

	/*
	 * Reset SDCC controller to power on default state.
	 * Don't issue a reset request to clock control block if
	 * SDCC controller itself can support hard reset.
	 */
	if (is_sw_hard_reset(host)) {
		u32 pwr;

		writel_relaxed(readl_relaxed(host->base + MMCIPOWER)
				| MCI_SW_RST, host->base + MMCIPOWER);
		msmsdcc_sync_reg_wr(host);

		/*
		 * See comment in msmsdcc_soft_reset() on choosing 1ms
		 * poll timeout.
		 */
		ret = readl_poll_timeout_noirq(host->base + MMCIPOWER,
				pwr, !(pwr & MCI_SW_RST), 100, 10);

		if (ret) {
			pr_err("%s: %s failed (%d)\n",
			mmc_hostname(host->mmc), __func__, ret);
			BUG();
		}
	} else {
		ret = clk_reset(host->clk, CLK_RESET_ASSERT);
		if (ret)
			pr_err("%s: Clock assert failed at %u Hz" \
				" with err %d\n", mmc_hostname(host->mmc),
				host->clk_rate, ret);

		ret = clk_reset(host->clk, CLK_RESET_DEASSERT);
		if (ret)
			pr_err("%s: Clock deassert failed at %u Hz" \
				" with err %d\n", mmc_hostname(host->mmc),
				host->clk_rate, ret);

		mb();
		/* Give some delay for clock reset to propogate to controller */
		msmsdcc_delay(host);
	}
}

static void msmsdcc_reset_and_restore(struct msmsdcc_host *host)
{
	if (is_soft_reset(host)) {
		msmsdcc_soft_reset(host);

		pr_debug("%s: Applied soft reset to Controller\n",
				mmc_hostname(host->mmc));
	} else {
		/* Give Clock reset (hard reset) to controller */
		u32	mci_clk = 0;
		u32	mci_mask0 = 0;
		u32	dll_config = 0;

		/* Save the controller state */
		mci_clk = readl_relaxed(host->base + MMCICLOCK);
		mci_mask0 = readl_relaxed(host->base + MMCIMASK0);
		host->pwr = readl_relaxed(host->base + MMCIPOWER);
		if (host->tuning_needed)
			dll_config = readl_relaxed(host->base + MCI_DLL_CONFIG);
		mb();

		msmsdcc_hard_reset(host);
		pr_debug("%s: Applied hard reset to controller\n",
				mmc_hostname(host->mmc));

		/* Restore the contoller state */
		writel_relaxed(host->pwr, host->base + MMCIPOWER);
		msmsdcc_sync_reg_wr(host);
		writel_relaxed(mci_clk, host->base + MMCICLOCK);
		msmsdcc_sync_reg_wr(host);
		writel_relaxed(mci_mask0, host->base + MMCIMASK0);
		if (host->tuning_needed)
			writel_relaxed(dll_config, host->base + MCI_DLL_CONFIG);
		mb(); /* no delay required after writing to MASK0 register */
	}

	if (is_sps_mode(host))
		/*
		 * delay the SPS BAM reset in thread context as
		 * sps_connect/sps_disconnect APIs can be called
		 * only from non-atomic context.
		 */
		host->sps.reset_bam = true;

	if (host->dummy_52_needed)
		host->dummy_52_needed = 0;
}

static void msmsdcc_reset_dpsm(struct msmsdcc_host *host)
{
	struct mmc_request *mrq = host->curr.mrq;

	if (!mrq || !mrq->cmd || !mrq->data)
		goto out;

	/*
	 * If we have not waited for the prog done for write transfer then
	 * perform the DPSM reset without polling for TXACTIVE.
	 * Otherwise, we poll here unnecessarily as TXACTIVE will not be
	 * deasserted until DAT0 (Busy line) goes high.
	 */
	if (mrq->data->flags & MMC_DATA_WRITE) {
		if (!msmsdcc_is_wait_for_prog_done(host, mrq)) {
			if (is_wait_for_tx_rx_active(host) &&
			    !is_auto_prog_done(host))
				pr_warning("%s: %s: AUTO_PROG_DONE capability is must\n",
					   mmc_hostname(host->mmc), __func__);
			goto no_polling;
		}
	}

	/* Make sure h/w (TX/RX) is inactive before resetting DPSM */
	if (is_wait_for_tx_rx_active(host)) {
		ktime_t start = ktime_get();

		while (readl_relaxed(host->base + MMCISTATUS) &
				(MCI_TXACTIVE | MCI_RXACTIVE)) {
			/*
			 * TX/RX active bits may be asserted for 4HCLK + 4MCLK
			 * cycles (~11us) after data transfer due to clock mux
			 * switching delays. Let's poll for 1ms and panic if
			 * still active.
			 */
			if (ktime_to_us(ktime_sub(ktime_get(), start)) > 1000) {
				pr_err("%s: %s still active\n",
					mmc_hostname(host->mmc),
					readl_relaxed(host->base + MMCISTATUS)
					& MCI_TXACTIVE ? "TX" : "RX");
				msmsdcc_dump_sdcc_state(host);
				msmsdcc_reset_and_restore(host);
				goto out;
			}
		}
	}

no_polling:
	writel_relaxed(0, host->base + MMCIDATACTRL);
	msmsdcc_sync_reg_wr(host); /* Allow the DPSM to be reset */
out:
	return;
}

static int
msmsdcc_request_end(struct msmsdcc_host *host, struct mmc_request *mrq)
{
	int retval = 0;

	BUG_ON(host->curr.data);

	del_timer(&host->req_tout_timer);

	if (mrq->data)
		mrq->data->bytes_xfered = host->curr.data_xfered;
	if (mrq->cmd->error == -ETIMEDOUT)
		mdelay(5);

	msmsdcc_reset_dpsm(host);

	/* Clear current request information as current request has ended */
	memset(&host->curr, 0, sizeof(struct msmsdcc_curr_req));

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
	host->curr.wait_for_auto_prog_done = false;
	host->curr.got_auto_prog_done = false;
}

static inline uint32_t msmsdcc_fifo_addr(struct msmsdcc_host *host)
{
	return host->core_memres->start + MMCIFIFO;
}

static inline unsigned int msmsdcc_get_min_sup_clk_rate(
					struct msmsdcc_host *host);

static inline void msmsdcc_sync_reg_wr(struct msmsdcc_host *host)
{
	mb();
	if (!is_wait_for_reg_write(host))
		udelay(host->reg_write_delay);
	else if (readl_relaxed(host->base + MCI_STATUS2) &
			MCI_MCLK_REG_WR_ACTIVE) {
		ktime_t start, diff;

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

static inline void msmsdcc_delay(struct msmsdcc_host *host)
{
	udelay(host->reg_write_delay);

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
	msmsdcc_sync_reg_wr(host); /* Force delay prior to ADM or command */

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
	if (!mrq->data->host_cookie)
		dma_unmap_sg(mmc_dev(host->mmc), host->dma.sg,
			     host->dma.num_ents, host->dma.dir);

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
			mrq->data->bytes_xfered = host->curr.data_xfered;
			msmsdcc_reset_dpsm(host);
			del_timer(&host->req_tout_timer);
			/*
			 * Clear current request information as current
			 * request has ended
			 */
			memset(&host->curr, 0, sizeof(struct msmsdcc_curr_req));
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
	if (!mrq->data->host_cookie)
		dma_unmap_sg(mmc_dev(host->mmc), host->sps.sg,
			     host->sps.num_ents, host->sps.dir);
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
			mrq->data->bytes_xfered = host->curr.data_xfered;
			msmsdcc_reset_dpsm(host);
			del_timer(&host->req_tout_timer);
			/*
			 * Clear current request information as current
			 * request has ended
			 */
			memset(&host->curr, 0, sizeof(struct msmsdcc_curr_req));
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
	if (!mrq->data->host_cookie)
		dma_unmap_sg(mmc_dev(host->mmc), host->sps.sg,
			     host->sps.num_ents, host->sps.dir);

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

static int msmsdcc_enable_cdr_cm_sdc4_dll(struct msmsdcc_host *host);

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

static bool msmsdcc_is_dma_possible(struct msmsdcc_host *host,
				    struct mmc_data *data)
{
	bool ret = true;
	u32 xfer_size = data->blksz * data->blocks;

	if (host->enforce_pio_mode) {
		ret = false;
		goto out;
	}
	if (is_sps_mode(host)) {
		/*
		 * BAM Mode: Fall back on PIO if size is less
		 * than or equal to SPS_MIN_XFER_SIZE bytes.
		 */
		if (xfer_size <= SPS_MIN_XFER_SIZE)
			ret = false;
	} else if (is_dma_mode(host)) {
		/*
		 * ADM Mode: Fall back on PIO if size is less than FIFO size
		 * or not integer multiple of FIFO size
		 */
		if (xfer_size % MCI_FIFOSIZE)
			ret = false;
	} else {
		/* PIO Mode */
		ret = false;
	}
 out:
	return ret;
}

static int msmsdcc_config_dma(struct msmsdcc_host *host, struct mmc_data *data)
{
	struct msmsdcc_nc_dmadata *nc;
	dmov_box *box;
	uint32_t rows;
	int n;
	int i, err = 0, box_cmd_cnt = 0;
	struct scatterlist *sg = data->sg;
	unsigned int len, offset;

	if ((host->dma.channel == -1) || (host->dma.crci == -1))
		return -ENOENT;

	BUG_ON((host->pdev->id < 1) || (host->pdev->id > 5));

	host->dma.sg = data->sg;
	host->dma.num_ents = data->sg_len;

	/* Prevent memory corruption */
	BUG_ON(host->dma.num_ents > msmsdcc_get_nr_sg(host));

	nc = host->dma.nc;

	if (data->flags & MMC_DATA_READ)
		host->dma.dir = DMA_FROM_DEVICE;
	else
		host->dma.dir = DMA_TO_DEVICE;

	if (!data->host_cookie) {
		n = msmsdcc_prep_xfer(host, data);
		if (unlikely(n < 0)) {
			host->dma.sg = NULL;
			host->dma.num_ents = 0;
			return -ENOMEM;
		}
	}

	/* host->curr.user_pages = (data->flags & MMC_DATA_USERPAGE); */
	host->curr.user_pages = 0;
	box = &nc->cmd[0];
	for (i = 0; i < host->dma.num_ents; i++) {
		len = sg_dma_len(sg);
		offset = 0;

		do {
			/* Check if we can do DMA */
			if (!len || (box_cmd_cnt >= MMC_MAX_DMA_CMDS)) {
				err = -ENOTSUPP;
				goto unmap;
			}

			box->cmd = CMD_MODE_BOX;

			if (len >= MMC_MAX_DMA_BOX_LENGTH) {
				len = MMC_MAX_DMA_BOX_LENGTH;
				len -= len % data->blksz;
			}
			rows = (len % MCI_FIFOSIZE) ?
				(len / MCI_FIFOSIZE) + 1 :
				(len / MCI_FIFOSIZE);

			if (data->flags & MMC_DATA_READ) {
				box->src_row_addr = msmsdcc_fifo_addr(host);
				box->dst_row_addr = sg_dma_address(sg) + offset;
				box->src_dst_len = (MCI_FIFOSIZE << 16) |
						(MCI_FIFOSIZE);
				box->row_offset = MCI_FIFOSIZE;
				box->num_rows = rows * ((1 << 16) + 1);
				box->cmd |= CMD_SRC_CRCI(host->dma.crci);
			} else {
				box->src_row_addr = sg_dma_address(sg) + offset;
				box->dst_row_addr = msmsdcc_fifo_addr(host);
				box->src_dst_len = (MCI_FIFOSIZE << 16) |
						(MCI_FIFOSIZE);
				box->row_offset = (MCI_FIFOSIZE << 16);
				box->num_rows = rows * ((1 << 16) + 1);
				box->cmd |= CMD_DST_CRCI(host->dma.crci);
			}

			offset += len;
			len = sg_dma_len(sg) - offset;
			box++;
			box_cmd_cnt++;
		} while (len);
		sg++;
	}
	/* Mark last command */
	box--;
	box->cmd |= CMD_LC;

	/* location of command block must be 64 bit aligned */
	BUG_ON(host->dma.cmd_busaddr & 0x07);

	nc->cmdptr = (host->dma.cmd_busaddr >> 3) | CMD_PTR_LP;
	host->dma.hdr.cmdptr = DMOV_CMD_PTR_LIST |
			       DMOV_CMD_ADDR(host->dma.cmdptr_busaddr);
	host->dma.hdr.complete_func = msmsdcc_dma_complete_func;

	/* Flush all data to memory before starting dma */
	mb();

unmap:
	if (err) {
		if (!data->host_cookie)
			dma_unmap_sg(mmc_dev(host->mmc), host->dma.sg,
				     host->dma.num_ents, host->dma.dir);
		pr_err("%s: cannot do DMA, fall back to PIO mode err=%d\n",
				mmc_hostname(host->mmc), err);
	}

	return err;
}

static int msmsdcc_prep_xfer(struct msmsdcc_host *host,
			     struct mmc_data *data)
{
	int rc = 0;
	unsigned int dir;

	/* Prevent memory corruption */
	BUG_ON(data->sg_len > msmsdcc_get_nr_sg(host));

	if (data->flags & MMC_DATA_READ)
		dir = DMA_FROM_DEVICE;
	else
		dir = DMA_TO_DEVICE;

	/* Make sg buffers DMA ready */
	rc = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
			dir);

	if (unlikely(rc != data->sg_len)) {
		pr_err("%s: Unable to map in all sg elements, rc=%d\n",
		       mmc_hostname(host->mmc), rc);
		rc = -ENOMEM;
		goto dma_map_err;
	}

	pr_debug("%s: %s: %s: sg_len=%d\n",
		mmc_hostname(host->mmc), __func__,
		dir == DMA_FROM_DEVICE ? "READ" : "WRITE",
		data->sg_len);

	goto out;

dma_map_err:
	dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
		     data->flags);
out:
	return rc;
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

	if (!data->host_cookie) {
		rc = msmsdcc_prep_xfer(host, data);
		if (unlikely(rc < 0)) {
			host->dma.sg = NULL;
			host->dma.num_ents = 0;
			goto out;
		}
	}

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
				if ((i == data->sg_len - 1) &&
						(sps_pipe_handle ==
						host->sps.cons.pipe_handle)) {
					/*
					 * set EOT only for consumer pipe, for
					 * producer pipe h/w will set it.
					 */
					flags = SPS_IOVEC_FLAG_INT |
						SPS_IOVEC_FLAG_EOT;
				}
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
	if (!data->host_cookie)
		dma_unmap_sg(mmc_dev(host->mmc), host->sps.sg,
			     host->sps.num_ents, host->sps.dir);
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

	/* DAT_CMD bit should be set for all ADTC */
	if (mmc_cmd_type(cmd) == MMC_CMD_ADTC)
		*c |= MCI_CSPM_DATCMD;

	/* Check if AUTO CMD19/CMD21 is required or not? */
	if (host->tuning_needed && (cmd->mrq->data &&
	    (cmd->mrq->data->flags & MMC_DATA_READ)) &&
	    (host->en_auto_cmd19 || host->en_auto_cmd21)) {
		/*
		 * For open ended block read operation (without CMD23),
		 * AUTO_CMD19/AUTO_CMD21 bit should be set while sending
		 * the READ command.
		 * For close ended block read operation (with CMD23),
		 * AUTO_CMD19/AUTO_CMD21 bit should be set while sending
		 * CMD23.
		 */
		if ((cmd->opcode == MMC_SET_BLOCK_COUNT &&
			host->curr.mrq->cmd->opcode ==
				MMC_READ_MULTIPLE_BLOCK) ||
			(!host->curr.mrq->sbc &&
			(cmd->opcode == MMC_READ_SINGLE_BLOCK ||
			cmd->opcode == MMC_READ_MULTIPLE_BLOCK ||
			cmd->opcode == SD_IO_RW_EXTENDED))) {
			msmsdcc_enable_cdr_cm_sdc4_dll(host);
			if (host->en_auto_cmd19 &&
			    host->mmc->ios.timing == MMC_TIMING_UHS_SDR104)
				*c |= MCI_CSPM_AUTO_CMD19;
			else if (host->en_auto_cmd21 &&
			    host->mmc->ios.timing == MMC_TIMING_MMC_HS200)
				*c |= MCI_CSPM_AUTO_CMD21;
		}
	}

	if (cmd->mrq->data && (cmd->mrq->data->flags & MMC_DATA_READ))
		writel_relaxed((readl_relaxed(host->base +
				MCI_DLL_CONFIG) | MCI_CDR_EN),
				host->base + MCI_DLL_CONFIG);
	else
		/* Clear CDR_EN bit for non read operations */
		writel_relaxed((readl_relaxed(host->base +
				MCI_DLL_CONFIG) & ~MCI_CDR_EN),
				host->base + MCI_DLL_CONFIG);

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
	host->curr.got_auto_prog_done = false;

	datactrl = MCI_DPSM_ENABLE | (data->blksz << 4);

	if (host->curr.wait_for_auto_prog_done)
		datactrl |= MCI_AUTO_PROG_DONE;

	if (msmsdcc_is_dma_possible(host, data)) {
		if (is_dma_mode(host) && !msmsdcc_config_dma(host, data)) {
			datactrl |= MCI_DPSM_DMAENABLE;
		} else if (is_sps_mode(host)) {
			if (!msmsdcc_sps_start_xfer(host, data)) {
				/* Now kick start DML transfer */
				mb();
				msmsdcc_dml_start_xfer(host, data);
				datactrl |= MCI_DPSM_DMAENABLE;
				host->sps.busy = 1;
			}
		}
	}

	/* Is data transfer in PIO mode required? */
	if (!(datactrl & MCI_DPSM_DMAENABLE)) {
		if (data->flags & MMC_DATA_READ) {
			pio_irqmask = MCI_RXFIFOHALFFULLMASK;
			if (host->curr.xfer_remain < MCI_FIFOSIZE)
				pio_irqmask |= MCI_RXDATAAVLBLMASK;
		} else
			pio_irqmask = MCI_TXFIFOHALFEMPTYMASK |
					MCI_TXFIFOEMPTYMASK;

		msmsdcc_sg_start(host);
	}

	if (data->flags & MMC_DATA_READ)
		datactrl |= (MCI_DPSM_DIRECTION | MCI_RX_DATA_PEND);
	else if (host->curr.use_wr_data_pend)
		datactrl |= MCI_DATA_PEND;

	if (host->mmc->ios.timing == MMC_TIMING_UHS_DDR50)
		clks = (unsigned long long)data->timeout_ns *
					   (host->clk_rate / 2);
	else
		clks = (unsigned long long)data->timeout_ns * host->clk_rate;

	do_div(clks, 1000000000UL);
	timeout = data->timeout_clks + (unsigned int)clks*2 ;
	WARN(!timeout,
	     "%s: data timeout is zero. timeout_ns=0x%x, timeout_clks=0x%x\n",
	     mmc_hostname(host->mmc), data->timeout_ns, data->timeout_clks);

	if (is_dma_mode(host) && (datactrl & MCI_DPSM_DMAENABLE)) {
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

		if (cmd) {
			/* Delay between data/command */
			msmsdcc_sync_reg_wr(host);
			/* Daisy-chain the command if requested */
			msmsdcc_start_command(host, cmd, c);
		} else {
			/*
			 * We don't need delay after writing to DATA_CTRL
			 * register if we are not writing to CMD register
			 * immediately after this. As we already have delay
			 * before sending the command, we just need mb() here.
			 */
			mb();
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
	if ((status & MCI_DATACRCFAIL) || (status & MCI_DATATIMEOUT)) {
		u32 opcode = data->mrq->cmd->opcode;

		if (!((!host->tuning_in_progress && opcode == MMC_BUS_TEST_W)
		    || (opcode == MMC_BUS_TEST_R) ||
		    (host->tuning_in_progress &&
		    (opcode == MMC_SEND_TUNING_BLOCK_HS200 ||
		     opcode == MMC_SEND_TUNING_BLOCK)))) {
			/* Execute full tuning in case of CRC/timeout errors */
			host->saved_tuning_phase = INVALID_TUNING_PHASE;

			if (status & MCI_DATACRCFAIL) {
				pr_err("%s: Data CRC error\n",
				       mmc_hostname(host->mmc));
				pr_err("%s: opcode 0x%.8x\n", __func__, opcode);
				pr_err("%s: blksz %d, blocks %d\n", __func__,
				       data->blksz, data->blocks);
			} else {
				pr_err("%s: CMD%d: Data timeout. DAT0 => %d\n",
					 mmc_hostname(host->mmc), opcode,
					 (readl_relaxed(host->base
					 + MCI_TEST_INPUT) & 0x2) ? 1 : 0);
				msmsdcc_dump_sdcc_state(host);
			}
		}

		/*
		 * CRC is optional for the bus test commands, not all
		 * cards respond back with CRC. However controller
		 * waits for the CRC and times out. Hence ignore the
		 * data timeouts during the Bustest.
		 */
		if (!((!host->tuning_in_progress && opcode == MMC_BUS_TEST_W)
		    || (opcode == MMC_BUS_TEST_R))) {
			if (status & MCI_DATACRCFAIL)
				data->error = -EILSEQ;
			else
				data->error = -ETIMEDOUT;
		}
		/* In case of DATA CRC/timeout error, execute tuning again */
		if (host->tuning_needed && !host->tuning_in_progress)
			host->tuning_done = false;

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

/*
 * Copy up to a word (4 bytes) between a scatterlist
 * and a temporary bounce buffer when the word lies across
 * two pages. The temporary buffer can then be read to/
 * written from the FIFO once.
 */
static void _msmsdcc_sg_consume_word(struct msmsdcc_host *host)
{
	struct msmsdcc_pio_data *pio = &host->pio;
	unsigned int bytes_avail;

	if (host->curr.data->flags & MMC_DATA_READ)
		memcpy(pio->sg_miter.addr, pio->bounce_buf,
		       pio->bounce_buf_len);
	else
		memcpy(pio->bounce_buf, pio->sg_miter.addr,
		       pio->bounce_buf_len);

	while (pio->bounce_buf_len != 4) {
		if (!sg_miter_next(&pio->sg_miter))
			break;
		bytes_avail = min_t(unsigned int, pio->sg_miter.length,
			4 - pio->bounce_buf_len);
		if (host->curr.data->flags & MMC_DATA_READ)
			memcpy(pio->sg_miter.addr,
			       &pio->bounce_buf[pio->bounce_buf_len],
			       bytes_avail);
		else
			memcpy(&pio->bounce_buf[pio->bounce_buf_len],
			       pio->sg_miter.addr, bytes_avail);

		pio->sg_miter.consumed = bytes_avail;
		pio->bounce_buf_len += bytes_avail;
	}
}

/*
 * Use sg_miter_next to return as many 4-byte aligned
 * chunks as possible, using a temporary 4 byte buffer
 * for alignment if necessary
 */
static int msmsdcc_sg_next(struct msmsdcc_host *host, char **buf, int *len)
{
	struct msmsdcc_pio_data *pio = &host->pio;
	unsigned int length, rlength;
	char *buffer;

	if (!sg_miter_next(&pio->sg_miter))
		return 0;

	buffer = pio->sg_miter.addr;
	length = pio->sg_miter.length;

	if (length < host->curr.xfer_remain) {
		rlength = round_down(length, 4);
		if (rlength) {
			/*
			 * We have a 4-byte aligned chunk.
			 * The rounding will be reflected by
			 * a call to msmsdcc_sg_consumed
			 */
			length = rlength;
			goto sg_next_end;
		}
		/*
		 * We have a length less than 4 bytes. Check to
		 * see if more buffer is available, and combine
		 * to make 4 bytes if possible.
		 */
		pio->bounce_buf_len = length;
		memset(pio->bounce_buf, 0, 4);

		/*
		 * On a read, get 4 bytes from FIFO, and distribute
		 * (4-bouce_buf_len) bytes into consecutive
		 * sgl buffers when msmsdcc_sg_consumed is called
		 */
		if (host->curr.data->flags & MMC_DATA_READ) {
			buffer = pio->bounce_buf;
			length = 4;
			goto sg_next_end;
		} else {
			_msmsdcc_sg_consume_word(host);
			buffer = pio->bounce_buf;
			length = pio->bounce_buf_len;
		}
	}

sg_next_end:
	*buf = buffer;
	*len = length;
	return 1;
}

/*
 * Update sg_miter.consumed based on how many bytes were
 * consumed. If the bounce buffer was used to read from FIFO,
 * redistribute into sgls.
 */
static void msmsdcc_sg_consumed(struct msmsdcc_host *host,
				unsigned int length)
{
	struct msmsdcc_pio_data *pio = &host->pio;

	if (host->curr.data->flags & MMC_DATA_READ) {
		if (length > pio->sg_miter.consumed)
			/*
			 * consumed 4 bytes, but sgl
			 * describes < 4 bytes
			 */
			_msmsdcc_sg_consume_word(host);
		else
			pio->sg_miter.consumed = length;
	} else
		if (length < pio->sg_miter.consumed)
			pio->sg_miter.consumed = length;
}

static void msmsdcc_sg_start(struct msmsdcc_host *host)
{
	unsigned int sg_miter_flags = SG_MITER_ATOMIC;

	host->pio.bounce_buf_len = 0;

	if (host->curr.data->flags & MMC_DATA_READ)
		sg_miter_flags |= SG_MITER_TO_SG;
	else
		sg_miter_flags |= SG_MITER_FROM_SG;

	sg_miter_start(&host->pio.sg_miter, host->curr.data->sg,
		       host->curr.data->sg_len, sg_miter_flags);
}

static void msmsdcc_sg_stop(struct msmsdcc_host *host)
{
	sg_miter_stop(&host->pio.sg_miter);
}

static inline void msmsdcc_clear_pio_irq_mask(struct msmsdcc_host *host)
{
	writel_relaxed(readl_relaxed(host->base + MMCIMASK0) & ~MCI_IRQ_PIO,
			host->base + MMCIMASK0);
	mb();
}

static irqreturn_t
msmsdcc_pio_irq(int irq, void *dev_id)
{
	struct msmsdcc_host	*host = dev_id;
	void __iomem		*base = host->base;
	uint32_t		status;
	unsigned long flags;
	unsigned int remain;
	char *buffer;

	spin_lock(&host->lock);

	if (!atomic_read(&host->clks_on) || !host->curr.data) {
		spin_unlock(&host->lock);
		return IRQ_NONE;
	}

	status = readl_relaxed(base + MMCISTATUS);

	if (((readl_relaxed(host->base + MMCIMASK0) & status) &
				(MCI_IRQ_PIO)) == 0) {
		spin_unlock(&host->lock);
		return IRQ_NONE;
	}
#if IRQ_DEBUG
	msmsdcc_print_status(host, "irq1-r", status);
#endif
	local_irq_save(flags);

	do {
		unsigned int len;

		if (!(status & (MCI_TXFIFOHALFEMPTY | MCI_TXFIFOEMPTY
				| MCI_RXDATAAVLBL)))
			break;

		if (!msmsdcc_sg_next(host, &buffer, &remain))
			break;

		len = 0;
		if (status & MCI_RXACTIVE)
			len = msmsdcc_pio_read(host, buffer, remain);
		if (status & MCI_TXACTIVE)
			len = msmsdcc_pio_write(host, buffer, remain);

		/* len might have aligned to 32bits above */
		if (len > remain)
			len = remain;

		host->curr.xfer_remain -= len;
		host->curr.data_xfered += len;
		remain -= len;
		msmsdcc_sg_consumed(host, len);

		if (remain) /* Done with this page? */
			break; /* Nope */

		status = readl_relaxed(base + MMCISTATUS);
	} while (1);

	msmsdcc_sg_stop(host);
	local_irq_restore(flags);

	if (!host->curr.xfer_remain) {
		msmsdcc_clear_pio_irq_mask(host);
		goto out_unlock;
	}

	if (status & MCI_RXACTIVE && host->curr.xfer_remain < MCI_FIFOSIZE) {
		writel_relaxed((readl_relaxed(host->base + MMCIMASK0) &
					~MCI_IRQ_PIO) | MCI_RXDATAAVLBLMASK,
					host->base + MMCIMASK0);
		mb();
	}

out_unlock:
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
	if (mmc_resp_type(cmd))
		cmd->resp[0] = readl_relaxed(host->base + MMCIRESPONSE0);
	/*
	 * Read rest of the response registers only if
	 * long response is expected for this command
	 */
	if (mmc_resp_type(cmd) & MMC_RSP_136) {
		cmd->resp[1] = readl_relaxed(host->base + MMCIRESPONSE1);
		cmd->resp[2] = readl_relaxed(host->base + MMCIRESPONSE2);
		cmd->resp[3] = readl_relaxed(host->base + MMCIRESPONSE3);
	}

	if (status & (MCI_CMDTIMEOUT | MCI_AUTOCMD19TIMEOUT)) {
		pr_debug("%s: CMD%d: Command timeout\n",
				mmc_hostname(host->mmc), cmd->opcode);
		cmd->error = -ETIMEDOUT;
	} else if ((status & MCI_CMDCRCFAIL && cmd->flags & MMC_RSP_CRC) &&
			!host->tuning_in_progress) {
		pr_err("%s: CMD%d: Command CRC error\n",
			mmc_hostname(host->mmc), cmd->opcode);
		msmsdcc_dump_sdcc_state(host);
		/* Execute full tuning in case of CRC errors */
		host->saved_tuning_phase = INVALID_TUNING_PHASE;
		if (host->tuning_needed)
			host->tuning_done = false;
		cmd->error = -EILSEQ;
	}

	if (!cmd->error) {
		if (cmd->cmd_timeout_ms > host->curr.req_tout_ms) {
			host->curr.req_tout_ms = cmd->cmd_timeout_ms;
			mod_timer(&host->req_tout_timer, (jiffies +
				  msecs_to_jiffies(host->curr.req_tout_ms)));
		}
	}

	if (!cmd->data || cmd->error) {
		if (host->curr.data && host->dma.sg &&
			is_dma_mode(host))
			msm_dmov_flush(host->dma.channel, 0);
		else if (host->curr.data && host->sps.sg &&
			is_sps_mode(host)) {
			/* Stop current SPS transfer */
			msmsdcc_sps_exit_curr_xfer(host);
		}
		else if (host->curr.data) { /* Non DMA */
			msmsdcc_clear_pio_irq_mask(host);
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
				host->curr.wait_for_auto_prog_done = false;
				if (host->dummy_52_needed)
					host->dummy_52_needed = 0;
				if (cmd->data && cmd->error)
					msmsdcc_reset_and_restore(host);
				msmsdcc_request_end(host, cmd->mrq);
			}
		}
	} else if (cmd->data) {
		if (cmd == host->curr.mrq->sbc)
			msmsdcc_start_command(host, host->curr.mrq->cmd, 0);
		else if ((cmd->data->flags & MMC_DATA_WRITE) &&
			   !host->curr.use_wr_data_pend)
			msmsdcc_start_data(host, cmd->data, NULL, 0);
	}
}

static irqreturn_t
msmsdcc_irq(int irq, void *dev_id)
{
	struct msmsdcc_host	*host = dev_id;
	struct mmc_host		*mmc = host->mmc;
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

		if (!atomic_read(&host->clks_on)) {
			pr_debug("%s: %s: SDIO async irq received\n",
					mmc_hostname(host->mmc), __func__);

			/*
			 * Only async interrupt can come when clocks are off,
			 * disable further interrupts and enable them when
			 * clocks are on.
			 */
			if (!host->sdcc_irq_disabled) {
				disable_irq_nosync(irq);
				host->sdcc_irq_disabled = 1;
			}

			/*
			 * If mmc_card_wake_sdio_irq() is set, mmc core layer
			 * will take care of signaling sdio irq during
			 * mmc_sdio_resume().
			 */
			if (host->sdcc_suspended &&
					(host->plat->mpm_sdiowakeup_int ||
					 host->plat->sdiowakeup_irq)) {
				/*
				 * This is a wakeup interrupt so hold wakelock
				 * until SDCC resume is handled.
				 */
				wake_lock(&host->sdio_wlock);
			} else {
				if (!mmc->card || (mmc->card &&
				    !mmc_card_sdio(mmc->card))) {
					pr_warning("%s: SDCC core interrupt received for non-SDIO cards when SDCC clocks are off\n",
					   mmc_hostname(mmc));
					ret = 1;
					break;
				}
				spin_unlock(&host->lock);
				mmc_signal_sdio_irq(host->mmc);
				spin_lock(&host->lock);
			}
			ret = 1;
			break;
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
			msmsdcc_sync_reg_wr(host);
#if IRQ_DEBUG
		msmsdcc_print_status(host, "irq0-p", status);
#endif

		if (status & MCI_SDIOINTROPE) {
			if (!mmc->card || (mmc->card &&
			    !mmc_card_sdio(mmc->card))) {
				pr_warning("%s: SDIO interrupt (SDIOINTROPE) received for non-SDIO card\n",
					   mmc_hostname(mmc));
				ret = 1;
				break;
			}
			if (host->sdcc_suspending)
				wake_lock(&host->sdio_suspend_wlock);
			spin_unlock(&host->lock);
			mmc_signal_sdio_irq(host->mmc);
			spin_lock(&host->lock);
		}
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
				if (host->dma.sg && is_dma_mode(host))
					msm_dmov_flush(host->dma.channel, 0);
				else if (host->sps.sg && is_sps_mode(host)) {
					/* Stop current SPS transfer */
					msmsdcc_sps_exit_curr_xfer(host);
				} else {
					msmsdcc_clear_pio_irq_mask(host);
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
				host->curr.got_auto_prog_done = true;

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
msmsdcc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq,
		bool is_first_request)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;
	int rc = 0;

	if (unlikely(!data)) {
		pr_err("%s: %s cannot prepare null data\n", mmc_hostname(mmc),
		       __func__);
		return;
	}
	if (unlikely(data->host_cookie)) {
		/* Very wrong */
		data->host_cookie = 0;
		pr_err("%s: %s Request reposted for prepare\n",
		       mmc_hostname(mmc), __func__);
		return;
	}

	if (!msmsdcc_is_dma_possible(host, data))
		return;

	rc = msmsdcc_prep_xfer(host, data);
	if (unlikely(rc < 0)) {
		data->host_cookie = 0;
		return;
	}

	data->host_cookie = 1;
}

static void
msmsdcc_post_req(struct mmc_host *mmc, struct mmc_request *mrq, int err)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned int dir;
	struct mmc_data *data = mrq->data;

	if (unlikely(!data)) {
		pr_err("%s: %s cannot cleanup null data\n", mmc_hostname(mmc),
		       __func__);
		return;
	}
	if (data->flags & MMC_DATA_READ)
		dir = DMA_FROM_DEVICE;
	else
		dir = DMA_TO_DEVICE;

	if (data->host_cookie)
		dma_unmap_sg(mmc_dev(host->mmc), data->sg,
			     data->sg_len, dir);

	data->host_cookie = 0;
}

static void
msmsdcc_request_start(struct msmsdcc_host *host, struct mmc_request *mrq)
{
	if (mrq->data) {
		/* Queue/read data, daisy-chain command when data starts */
		if ((mrq->data->flags & MMC_DATA_READ) ||
		    host->curr.use_wr_data_pend)
			msmsdcc_start_data(host, mrq->data,
					   mrq->sbc ? mrq->sbc : mrq->cmd,
					   0);
		else
			msmsdcc_start_command(host,
					      mrq->sbc ? mrq->sbc : mrq->cmd,
					      0);
	} else {
		msmsdcc_start_command(host, mrq->cmd, 0);
	}
}

/*
 * This function returns true if AUTO_PROG_DONE feature of host is
 * applicable for current request, returns "false" otherwise.
 *
 * NOTE: Caller should call this function only for data write operations.
 */
static bool msmsdcc_is_wait_for_auto_prog_done(struct msmsdcc_host *host,
					       struct mmc_request *mrq)
{
	/*
	 * Auto-prog done will be enabled for following cases:
	 * mrq->sbc	|	mrq->stop
	 * _____________|________________
	 *	True	|	Don't care
	 *	False	|	False (CMD24, ACMD25 use case)
	 */
	if (is_auto_prog_done(host) && (mrq->sbc || !mrq->stop))
		return true;

	return false;
}

/*
 * This function returns true if controller can wait for prog done
 * for current request, returns "false" otherwise.
 *
 * NOTE: Caller should call this function only for data write operations.
 */
static bool msmsdcc_is_wait_for_prog_done(struct msmsdcc_host *host,
					  struct mmc_request *mrq)
{
	if (msmsdcc_is_wait_for_auto_prog_done(host, mrq) || mrq->stop)
		return true;

	return false;
}

static void
msmsdcc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long		flags;
	unsigned int error = 0;
	int retries = 5;

	/*
	 * Get the SDIO AL client out of LPM.
	 */
	WARN(host->dummy_52_sent, "Dummy CMD52 in progress\n");
	if (host->plat->is_sdio_al_client)
		msmsdcc_sdio_al_lpm(mmc, false);

	/*
	 * Don't start the request if SDCC is not in proper state to handle it
	 * BAM state is checked below if applicable
	 */
	if (!host->pwr || !atomic_read(&host->clks_on) ||
			host->sdcc_irq_disabled) {
		WARN(1, "%s: %s: SDCC is in bad state. don't process new request (CMD%d)\n",
			mmc_hostname(host->mmc), __func__, mrq->cmd->opcode);
		error = EIO;
		goto bad_state;
	}

	/* check if sps bam needs to be reset */
	if (is_sps_mode(host) && host->sps.reset_bam) {
		while (retries) {
			if (!msmsdcc_bam_dml_reset_and_restore(host))
				break;
			pr_err("%s: msmsdcc_bam_dml_reset_and_restore returned error. %d attempts left.\n",
					mmc_hostname(host->mmc), --retries);
		}

		/* check if BAM reset succeeded or not */
		if (host->sps.reset_bam) {
			pr_err("%s: bam reset failed. Not processing the new request (CMD%d)\n",
				mmc_hostname(host->mmc), mrq->cmd->opcode);
			error = EAGAIN;
			goto bad_state;
		}
	}

	/*
	 * Check if DLL retuning is required? If yes, perform it here before
	 * starting new request.
	 */
	if (host->tuning_needed && !host->tuning_in_progress &&
	    !host->tuning_done) {
		pr_debug("%s: %s: execute_tuning for timing mode = %d\n",
			 mmc_hostname(mmc), __func__, host->mmc->ios.timing);
		if (host->mmc->ios.timing == MMC_TIMING_UHS_SDR104)
			msmsdcc_execute_tuning(mmc,
					       MMC_SEND_TUNING_BLOCK);
		else if (host->mmc->ios.timing == MMC_TIMING_MMC_HS200)
			msmsdcc_execute_tuning(mmc,
					       MMC_SEND_TUNING_BLOCK_HS200);
	}

	if (host->eject) {
		error = ENOMEDIUM;
		goto card_ejected;
	}

	WARN(host->curr.mrq, "%s: %s: New request (CMD%d) received while"
	     " other request (CMD%d) is in progress\n",
	     mmc_hostname(host->mmc), __func__,
	     mrq->cmd->opcode, host->curr.mrq->cmd->opcode);

	spin_lock_irqsave(&host->lock, flags);

	/*
	 * Set timeout value to 10 secs (or more in case of buggy cards)
	 */
	if ((mmc->card) && (mmc->card->quirks & MMC_QUIRK_INAND_DATA_TIMEOUT))
		host->curr.req_tout_ms = 20000;
	else
		host->curr.req_tout_ms = MSM_MMC_REQ_TIMEOUT;
	/*
	 * Kick the software request timeout timer here with the timeout
	 * value identified above
	 */
	mod_timer(&host->req_tout_timer,
			(jiffies +
			 msecs_to_jiffies(host->curr.req_tout_ms)));

	host->curr.mrq = mrq;
	if (mrq->sbc) {
		mrq->sbc->mrq = mrq;
		mrq->sbc->data = mrq->data;
	}

	if (mrq->data && (mrq->data->flags & MMC_DATA_WRITE)) {
		if (msmsdcc_is_wait_for_auto_prog_done(host, mrq)) {
			host->curr.wait_for_auto_prog_done = true;
		} else {
			if ((mrq->cmd->opcode == SD_IO_RW_EXTENDED) ||
			    (mrq->cmd->opcode == 54))
				host->dummy_52_needed = 1;
		}

		if ((mrq->cmd->opcode == MMC_WRITE_BLOCK) ||
		    (mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) ||
		    ((mrq->cmd->opcode == SD_IO_RW_EXTENDED) &&
		     is_data_pend_for_cmd53(host)))
			host->curr.use_wr_data_pend = true;
	}

	msmsdcc_request_start(host, mrq);
	spin_unlock_irqrestore(&host->lock, flags);
	return;

bad_state:
	msmsdcc_dump_sdcc_state(host);
card_ejected:
	mrq->cmd->error = -error;
	if (mrq->data) {
		mrq->data->error = -error;
		mrq->data->bytes_xfered = 0;
	}
	mmc_request_done(mmc, mrq);
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

static inline int msmsdcc_vreg_get_voltage(struct msm_mmc_reg_data *vreg)
{
	int rc = 0;

	rc = regulator_get_voltage(vreg->reg);
	if (rc < 0)
		pr_err("%s: regulator_get_voltage(%s) failed. rc=%d\n",
			__func__, vreg->name, rc);

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

	if (regulator_count_voltages(vreg->reg) > 0) {
		vreg->set_voltage_sup = 1;
		/* sanity check */
		if (!vreg->high_vol_level || !vreg->hpm_uA) {
			pr_err("%s: %s invalid constraints specified\n",
					__func__, vreg->name);
			rc = -EINVAL;
		}
	}

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
	struct msm_mmc_reg_data *curr_vdd_reg, *curr_vdd_io_reg;
	struct device *dev = mmc_dev(host->mmc);

	curr_slot = host->plat->vreg_data;
	if (!curr_slot)
		goto out;

	curr_vdd_reg = curr_slot->vdd_data;
	curr_vdd_io_reg = curr_slot->vdd_io_data;

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
		if (curr_vdd_io_reg) {
			rc = msmsdcc_vreg_init_reg(curr_vdd_io_reg, dev);
			if (rc)
				goto vdd_reg_deinit;
		}
		rc = msmsdcc_vreg_reset(host);
		if (rc)
			pr_err("msmsdcc.%d vreg reset failed (%d)\n",
			       host->pdev->id, rc);
		goto out;
	} else {
		/* Deregister all regulators from regulator framework */
		goto vdd_io_reg_deinit;
	}
vdd_io_reg_deinit:
	if (curr_vdd_io_reg)
		msmsdcc_vreg_deinit_reg(curr_vdd_io_reg);
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

static int msmsdcc_vreg_disable(struct msm_mmc_reg_data *vreg, bool is_init)
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
	} else if (vreg->is_enabled && vreg->always_on) {
		if (!is_init && vreg->lpm_sup) {
			/* Put always_on regulator in LPM (low power mode) */
			rc = msmsdcc_vreg_set_optimum_mode(vreg, vreg->lpm_uA);
			if (rc < 0)
				goto out;
		} else if (is_init && vreg->reset_at_init) {
			/**
			 * The regulator might not actually be disabled if it
			 * is shared and in use by other drivers.
			 */
			rc = regulator_disable(vreg->reg);
			if (rc) {
				pr_err("%s: regulator_disable(%s) failed at " \
					"bootup. rc=%d\n", __func__,
					vreg->name, rc);
				goto out;
			}
			vreg->is_enabled = false;
		}
	}
out:
	return rc;
}

static int msmsdcc_setup_vreg(struct msmsdcc_host *host, bool enable,
		bool is_init)
{
	int rc = 0, i;
	struct msm_mmc_slot_reg_data *curr_slot;
	struct msm_mmc_reg_data *vreg_table[2];

	curr_slot = host->plat->vreg_data;
	if (!curr_slot) {
		pr_debug("%s: vreg info unavailable, assuming the slot is powered by always on domain\n",
			 mmc_hostname(host->mmc));
		goto out;
	}

	vreg_table[0] = curr_slot->vdd_data;
	vreg_table[1] = curr_slot->vdd_io_data;

	for (i = 0; i < ARRAY_SIZE(vreg_table); i++) {
		if (vreg_table[i]) {
			if (enable)
				rc = msmsdcc_vreg_enable(vreg_table[i]);
			else
				rc = msmsdcc_vreg_disable(vreg_table[i],
						is_init);
			if (rc)
				goto out;
		}
	}
out:
	return rc;
}

/* This function returns the max. current supported by VDD rail in mA */
static unsigned int msmsdcc_get_vreg_vdd_max_current(struct msmsdcc_host *host)
{
	struct msm_mmc_slot_reg_data *curr_slot = host->plat->vreg_data;

	if (!curr_slot)
		return 0;

	if (curr_slot->vdd_data)
		return curr_slot->vdd_data->hpm_uA / 1000;
	else
		return 0;
}

/*
 * Reset vreg by ensuring it is off during probe. A call
 * to enable vreg is needed to balance disable vreg
 */
static int msmsdcc_vreg_reset(struct msmsdcc_host *host)
{
	int rc;

	rc = msmsdcc_setup_vreg(host, 1, true);
	if (rc)
		return rc;
	rc = msmsdcc_setup_vreg(host, 0, true);
	return rc;
}

enum vdd_io_level {
	/* set vdd_io_data->low_vol_level */
	VDD_IO_LOW,
	/* set vdd_io_data->high_vol_level */
	VDD_IO_HIGH,
	/*
	 * set whatever there in voltage_level (third argument) of
	 * msmsdcc_set_vdd_io_vol() function.
	 */
	VDD_IO_SET_LEVEL,
};

/*
 * This function returns the current VDD IO voltage level.
 * Returns negative value if it fails to read the voltage level
 * Returns 0 if regulator was disabled or if VDD_IO (and VDD)
 * regulator were not defined for host.
 */
static int msmsdcc_get_vdd_io_vol(struct msmsdcc_host *host)
{
	int rc = 0;

	if (host->plat->vreg_data) {
		struct msm_mmc_reg_data *io_reg =
			host->plat->vreg_data->vdd_io_data;

		/*
		 * If vdd_io is not defined, then we can consider that
		 * IO voltage is same as VDD.
		 */
		if (!io_reg)
			io_reg = host->plat->vreg_data->vdd_data;

		if (io_reg && io_reg->is_enabled)
			rc = msmsdcc_vreg_get_voltage(io_reg);
	}

	return rc;
}

/*
 * This function updates the IO pad power switch bit in MCI_CLK register
 * based on currrent IO pad voltage level.
 * NOTE: This function assumes that host lock was not taken by caller.
 */
static void msmsdcc_update_io_pad_pwr_switch(struct msmsdcc_host *host)
{
	int rc = 0;
	unsigned long flags;

	if (!is_io_pad_pwr_switch(host))
		return;

	rc = msmsdcc_get_vdd_io_vol(host);

	spin_lock_irqsave(&host->lock, flags);
	/*
	 * Dual voltage pad is the SDCC's (chipset) functionality and not all
	 * the SDCC instances support the dual voltage pads.
	 * For dual-voltage pad (1.8v/3.3v), SW should set IO_PAD_PWR_SWITCH
	 * bit before using the pads in 1.8V mode.
	 * For regular, not dual-voltage pads (including eMMC 1.2v/1.8v pads),
	 * IO_PAD_PWR_SWITCH bit is a don't care.
	 * But we don't have an option to know (by reading some SDCC register)
	 * that a particular SDCC instance supports dual voltage pads or not,
	 * so we simply set the IO_PAD_PWR_SWITCH bit for low voltage IO
	 * (1.8v/1.2v). For regular (not dual-voltage pads), this bit value
	 * is anyway ignored.
	 */
	if (rc > 0 && rc < 2700000)
		host->io_pad_pwr_switch = 1;
	else
		host->io_pad_pwr_switch = 0;

	if (atomic_read(&host->clks_on)) {
		if (host->io_pad_pwr_switch)
			writel_relaxed((readl_relaxed(host->base + MMCICLOCK) |
					IO_PAD_PWR_SWITCH),
					host->base + MMCICLOCK);
		else
			writel_relaxed((readl_relaxed(host->base + MMCICLOCK) &
					~IO_PAD_PWR_SWITCH),
					host->base + MMCICLOCK);
		msmsdcc_sync_reg_wr(host);
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

static int msmsdcc_set_vdd_io_vol(struct msmsdcc_host *host,
				  enum vdd_io_level level,
				  unsigned int voltage_level)
{
	int rc = 0;
	int set_level;

	if (host->plat->vreg_data) {
		struct msm_mmc_reg_data *vdd_io_reg =
			host->plat->vreg_data->vdd_io_data;

		if (vdd_io_reg && vdd_io_reg->is_enabled) {
			switch (level) {
			case VDD_IO_LOW:
				set_level = vdd_io_reg->low_vol_level;
				break;
			case VDD_IO_HIGH:
				set_level = vdd_io_reg->high_vol_level;
				break;
			case VDD_IO_SET_LEVEL:
				set_level = voltage_level;
				break;
			default:
				pr_err("%s: %s: invalid argument level = %d",
				       mmc_hostname(host->mmc), __func__,
				       level);
				rc = -EINVAL;
				goto out;
			}
			rc = msmsdcc_vreg_set_voltage(vdd_io_reg,
						      set_level, set_level);
		}
	}

out:
	return rc;
}

static inline int msmsdcc_is_pwrsave(struct msmsdcc_host *host)
{
	if (host->clk_rate > 400000 && msmsdcc_pwrsave)
		return 1;
	return 0;
}

/*
 * Any function calling msmsdcc_setup_clocks must
 * acquire clk_mutex. May sleep.
 */
static int msmsdcc_setup_clocks(struct msmsdcc_host *host, bool enable)
{
	int rc = 0;

	if (enable && !atomic_read(&host->clks_on)) {
		msmsdcc_msm_bus_cancel_work_and_set_vote(host, &host->mmc->ios);

		if (!IS_ERR_OR_NULL(host->bus_clk)) {
			rc = clk_prepare_enable(host->bus_clk);
			if (rc) {
				pr_err("%s: %s: failed to enable the bus-clock with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto remove_vote;
			}
		}
		if (!IS_ERR(host->pclk)) {
			rc = clk_prepare_enable(host->pclk);
			if (rc) {
				pr_err("%s: %s: failed to enable the pclk with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto disable_bus;
			}
		}
		rc = clk_prepare_enable(host->clk);
		if (rc) {
			pr_err("%s: %s: failed to enable the host-clk with error %d\n",
				mmc_hostname(host->mmc), __func__, rc);
			goto disable_pclk;
		}
		mb();
		msmsdcc_delay(host);
		atomic_set(&host->clks_on, 1);
	} else if (!enable && atomic_read(&host->clks_on)) {
		mb();
		msmsdcc_delay(host);
		clk_disable_unprepare(host->clk);
		if (!IS_ERR(host->pclk))
			clk_disable_unprepare(host->pclk);
		if (!IS_ERR_OR_NULL(host->bus_clk))
			clk_disable_unprepare(host->bus_clk);

		/*
		 * If clock gating is enabled, then remove the vote
		 * immediately because clocks will be disabled only
		 * after MSM_MMC_CLK_GATE_DELAY and thus no additional
		 * delay is required to remove the bus vote.
		 */
		 if (host->mmc->clkgate_delay)
			msmsdcc_msm_bus_cancel_work_and_set_vote(host, NULL);
		 else
			 msmsdcc_msm_bus_queue_work(host);

		atomic_set(&host->clks_on, 0);
	}
	goto out;

disable_pclk:
	if (!IS_ERR_OR_NULL(host->pclk))
		clk_disable_unprepare(host->pclk);
disable_bus:
	if (!IS_ERR_OR_NULL(host->bus_clk))
		clk_disable_unprepare(host->bus_clk);
remove_vote:
	msmsdcc_msm_bus_cancel_work_and_set_vote(host, NULL);
out:
	return rc;
}

static inline unsigned int msmsdcc_get_sup_clk_rate(struct msmsdcc_host *host,
						unsigned int req_clk)
{
	unsigned int sel_clk = -1;

	if (req_clk < msmsdcc_get_min_sup_clk_rate(host)) {
		sel_clk = msmsdcc_get_min_sup_clk_rate(host);
		goto out;
	}

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

out:
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
		if (!gpio_is_valid(curr->gpio[i].no)) {
			rc = -EINVAL;
			pr_err("%s: Invalid gpio = %d\n",
				mmc_hostname(host->mmc), curr->gpio[i].no);
			goto free_gpios;
		}
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
	for (i--; i >= 0; i--) {
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

static int msmsdcc_cfg_mpm_sdiowakeup(struct msmsdcc_host *host,
				      unsigned mode)
{
	int ret = 0;
	unsigned int pin = host->plat->mpm_sdiowakeup_int;

	if (!pin)
		return 0;

	switch (mode) {
	case SDC_DAT1_DISABLE:
		ret = msm_mpm_enable_pin(pin, 0);
		break;
	case SDC_DAT1_ENABLE:
		ret = msm_mpm_set_pin_type(pin, IRQ_TYPE_LEVEL_LOW);
		ret = msm_mpm_enable_pin(pin, 1);
		break;
	case SDC_DAT1_ENWAKE:
		ret = msm_mpm_set_pin_wake(pin, 1);
		break;
	case SDC_DAT1_DISWAKE:
		ret = msm_mpm_set_pin_wake(pin, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static u32 msmsdcc_setup_pwr(struct msmsdcc_host *host, struct mmc_ios *ios)
{
	u32 pwr = 0;
	int ret = 0;
	struct mmc_host *mmc = host->mmc;

	if (host->plat->translate_vdd && !host->sdio_gpio_lpm)
		ret = host->plat->translate_vdd(mmc_dev(mmc), ios->vdd);
	else if (!host->plat->translate_vdd && !host->sdio_gpio_lpm)
		ret = msmsdcc_setup_vreg(host, !!ios->vdd, false);

	if (ret) {
		pr_err("%s: Failed to setup voltage regulators\n",
				mmc_hostname(host->mmc));
		goto out;
	}

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		pwr = MCI_PWR_OFF;
		msmsdcc_cfg_mpm_sdiowakeup(host, SDC_DAT1_DISABLE);
		/*
		 * If VDD IO rail is always on, set low voltage for VDD
		 * IO rail when slot is not in use (like when card is not
		 * present or during system suspend).
		 */
		msmsdcc_set_vdd_io_vol(host, VDD_IO_LOW, 0);
		msmsdcc_update_io_pad_pwr_switch(host);
		msmsdcc_setup_pins(host, false);
		/*
		 * Reset the mask to prevent hitting any pending interrupts
		 * after powering up the card again.
		 */
		if (atomic_read(&host->clks_on)) {
			writel_relaxed(0, host->base + MMCIMASK0);
			mb();
		}
		break;
	case MMC_POWER_UP:
		/* writing PWR_UP bit is redundant */
		pwr = MCI_PWR_UP;
		msmsdcc_cfg_mpm_sdiowakeup(host, SDC_DAT1_ENABLE);

		msmsdcc_set_vdd_io_vol(host, VDD_IO_HIGH, 0);
		msmsdcc_update_io_pad_pwr_switch(host);
		msmsdcc_setup_pins(host, true);
		break;
	case MMC_POWER_ON:
		pwr = MCI_PWR_ON;
		break;
	}

out:
	return pwr;
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

/* Returns required bandwidth in Bytes per Sec */
static unsigned int msmsdcc_get_bw_required(struct msmsdcc_host *host,
					    struct mmc_ios *ios)
{
	unsigned int bw;

	bw = host->clk_rate;
	/*
	 * For DDR mode, SDCC controller clock will be at
	 * the double rate than the actual clock that goes to card.
	 */
	if (ios->bus_width == MMC_BUS_WIDTH_4)
		bw /= 2;
	else if (ios->bus_width == MMC_BUS_WIDTH_1)
		bw /= 8;

	return bw;
}

static int msmsdcc_msm_bus_get_vote_for_bw(struct msmsdcc_host *host,
					   unsigned int bw)
{
	unsigned int *table = host->plat->msm_bus_voting_data->bw_vecs;
	unsigned int size = host->plat->msm_bus_voting_data->bw_vecs_size;
	int i;

	if (host->msm_bus_vote.is_max_bw_needed && bw)
		return host->msm_bus_vote.max_bw_vote;

	for (i = 0; i < size; i++) {
		if (bw <= table[i])
			break;
	}

	if (i && (i == size))
		i--;

	return i;
}

static int msmsdcc_msm_bus_register(struct msmsdcc_host *host)
{
	int rc = 0;
	struct msm_bus_scale_pdata *use_cases;

	if (host->pdev->dev.of_node) {
		struct msm_mmc_bus_voting_data *data;
		struct device *dev = &host->pdev->dev;

		data = devm_kzalloc(dev,
			sizeof(struct msm_mmc_bus_voting_data), GFP_KERNEL);
		if (!data) {
			dev_err(&host->pdev->dev,
				"%s: failed to allocate memory\n", __func__);
			rc = -ENOMEM;
			goto out;
		}

		rc = msmsdcc_dt_get_array(dev, "qcom,bus-bw-vectors-bps",
				&data->bw_vecs, &data->bw_vecs_size, 0);
		if (!rc) {
			data->use_cases = msm_bus_cl_get_pdata(host->pdev);
			host->plat->msm_bus_voting_data = data;
		}
	}

	if (host->plat->msm_bus_voting_data &&
	    host->plat->msm_bus_voting_data->use_cases &&
	    host->plat->msm_bus_voting_data->bw_vecs &&
	    host->plat->msm_bus_voting_data->bw_vecs_size) {
		use_cases = host->plat->msm_bus_voting_data->use_cases;
		host->msm_bus_vote.client_handle =
				msm_bus_scale_register_client(use_cases);
	} else {
		return 0;
	}

	if (!host->msm_bus_vote.client_handle) {
		pr_err("%s: msm_bus_scale_register_client() failed\n",
		       mmc_hostname(host->mmc));
		rc = -EFAULT;
	} else {
		/* cache the vote index for minimum and maximum bandwidth */
		host->msm_bus_vote.min_bw_vote =
				msmsdcc_msm_bus_get_vote_for_bw(host, 0);
		host->msm_bus_vote.max_bw_vote =
				msmsdcc_msm_bus_get_vote_for_bw(host, UINT_MAX);
	}
out:
	return rc;
}

static void msmsdcc_msm_bus_unregister(struct msmsdcc_host *host)
{
	if (host->msm_bus_vote.client_handle)
		msm_bus_scale_unregister_client(
			host->msm_bus_vote.client_handle);
}

/*
 * This function must be called with host lock acquired.
 * Caller of this function should also ensure that msm bus client
 * handle is not null.
 */
static inline int msmsdcc_msm_bus_set_vote(struct msmsdcc_host *host,
					     int vote,
					     unsigned long flags)
{
	int rc = 0;

	if (vote != host->msm_bus_vote.curr_vote) {
		spin_unlock_irqrestore(&host->lock, flags);
		rc = msm_bus_scale_client_update_request(
				host->msm_bus_vote.client_handle, vote);
		if (rc)
			pr_err("%s: msm_bus_scale_client_update_request() failed."
			       " bus_client_handle=0x%x, vote=%d, err=%d\n",
			       mmc_hostname(host->mmc),
			       host->msm_bus_vote.client_handle, vote, rc);
		spin_lock_irqsave(&host->lock, flags);
		if (!rc)
			host->msm_bus_vote.curr_vote = vote;
	}

	return rc;
}

/*
 * Internal work. Work to set 0 bandwidth for msm bus.
 */
static void msmsdcc_msm_bus_work(struct work_struct *work)
{
	struct msmsdcc_host *host = container_of(work,
					struct msmsdcc_host,
					msm_bus_vote.vote_work.work);
	unsigned long flags;

	if (!host->msm_bus_vote.client_handle)
		return;

	spin_lock_irqsave(&host->lock, flags);
	/* don't vote for 0 bandwidth if any request is in progress */
	if (!host->curr.mrq)
		msmsdcc_msm_bus_set_vote(host,
			host->msm_bus_vote.min_bw_vote, flags);
	else
		pr_warning("%s: %s: SDCC transfer in progress. skipping"
			   " bus voting to 0 bandwidth\n",
			   mmc_hostname(host->mmc), __func__);
	spin_unlock_irqrestore(&host->lock, flags);
}

/*
 * This function cancels any scheduled delayed work
 * and sets the bus vote based on ios argument.
 * If "ios" argument is NULL, bandwidth required is 0 else
 * calculate the bandwidth based on ios parameters.
 */
static void msmsdcc_msm_bus_cancel_work_and_set_vote(
					struct msmsdcc_host *host,
					struct mmc_ios *ios)
{
	unsigned long flags;
	unsigned int bw;
	int vote;

	if (!host->msm_bus_vote.client_handle)
		return;

	bw = ios ? msmsdcc_get_bw_required(host, ios) : 0;

	cancel_delayed_work_sync(&host->msm_bus_vote.vote_work);
	spin_lock_irqsave(&host->lock, flags);
	vote = msmsdcc_msm_bus_get_vote_for_bw(host, bw);
	msmsdcc_msm_bus_set_vote(host, vote, flags);
	spin_unlock_irqrestore(&host->lock, flags);
}

/* This function queues a work which will set the bandwidth requiement to 0 */
static void msmsdcc_msm_bus_queue_work(struct msmsdcc_host *host)
{
	unsigned long flags;

	if (!host->msm_bus_vote.client_handle)
		return;

	spin_lock_irqsave(&host->lock, flags);
	if (host->msm_bus_vote.min_bw_vote != host->msm_bus_vote.curr_vote)
		queue_delayed_work(system_nrt_wq,
				   &host->msm_bus_vote.vote_work,
				   msecs_to_jiffies(MSM_MMC_BUS_VOTING_DELAY));
	spin_unlock_irqrestore(&host->lock, flags);
}

static void
msmsdcc_cfg_sdio_wakeup(struct msmsdcc_host *host, bool enable_wakeup_irq)
{
	struct mmc_host *mmc = host->mmc;

	/*
	 * SDIO_AL clients has different mechanism of handling LPM through
	 * sdio_al driver itself. The sdio wakeup interrupt is configured as
	 * part of that. Here, we are interested only in clients like WLAN.
	 */
	if (!(mmc->card && mmc_card_sdio(mmc->card))
			|| host->plat->is_sdio_al_client)
		goto out;

	if (!host->sdcc_suspended) {
		/*
		 * When MSM is not in power collapse and we
		 * are disabling clocks, enable bit 22 in MASK0
		 * to handle asynchronous SDIO interrupts.
		 */
		if (enable_wakeup_irq) {
			writel_relaxed(MCI_SDIOINTMASK, host->base + MMCIMASK0);
			mb();
		} else {
			writel_relaxed(MCI_SDIOINTMASK, host->base + MMCICLEAR);
			msmsdcc_sync_reg_wr(host);
		}
		goto out;
	} else if (!mmc_card_wake_sdio_irq(mmc)) {
		/*
		 * Wakeup MSM only if SDIO function drivers set
		 * MMC_PM_WAKE_SDIO_IRQ flag in their suspend call.
		 */
		goto out;
	}

	if (enable_wakeup_irq) {
		if (!host->plat->sdiowakeup_irq) {
			/*
			 * When there is no gpio line that can be configured
			 * as wakeup interrupt handle it by configuring
			 * asynchronous sdio interrupts and DAT1 line.
			 */
			writel_relaxed(MCI_SDIOINTMASK,
					host->base + MMCIMASK0);
			mb();
			msmsdcc_cfg_mpm_sdiowakeup(host, SDC_DAT1_ENWAKE);
			/* configure sdcc core interrupt as wakeup interrupt */
			msmsdcc_enable_irq_wake(host);
		} else {
			/* Let gpio line handle wakeup interrupt */
			writel_relaxed(0, host->base + MMCIMASK0);
			mb();
			if (host->sdio_wakeupirq_disabled) {
				host->sdio_wakeupirq_disabled = 0;
				/* configure gpio line as wakeup interrupt */
				msmsdcc_enable_irq_wake(host);
				enable_irq(host->plat->sdiowakeup_irq);
			}
		}
	} else {
		if (!host->plat->sdiowakeup_irq) {
			/*
			 * We may not have cleared bit 22 in the interrupt
			 * handler as the clocks might be off at that time.
			 */
			writel_relaxed(MCI_SDIOINTMASK, host->base + MMCICLEAR);
			msmsdcc_sync_reg_wr(host);
			msmsdcc_cfg_mpm_sdiowakeup(host, SDC_DAT1_DISWAKE);
			msmsdcc_disable_irq_wake(host);
		} else if (!host->sdio_wakeupirq_disabled) {
			disable_irq_nosync(host->plat->sdiowakeup_irq);
			msmsdcc_disable_irq_wake(host);
			host->sdio_wakeupirq_disabled = 1;
		}
	}
out:
	return;
}

static void
msmsdcc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	u32 clk = 0, pwr = 0;
	int rc;
	unsigned long flags;
	unsigned int clock;


	/*
	 * Disable SDCC core interrupt until set_ios is completed.
	 * This avoids any race conditions with interrupt raised
	 * when turning on/off the clocks. One possible
	 * scenario is SDIO operational interrupt while the clock
	 * is turned off.
	 * host->lock is being released intermittently below.
	 * Thus, prevent concurrent access to host.
	 */

	mutex_lock(&host->clk_mutex);
	DBG(host, "ios->clock = %u\n", ios->clock);
	spin_lock_irqsave(&host->lock, flags);
	if (!host->sdcc_irq_disabled) {
		disable_irq_nosync(host->core_irqres->start);
		host->sdcc_irq_disabled = 1;
	}
	spin_unlock_irqrestore(&host->lock, flags);

	/* Make sure sdcc core irq is synchronized */
	synchronize_irq(host->core_irqres->start);

	pwr = msmsdcc_setup_pwr(host, ios);

	spin_lock_irqsave(&host->lock, flags);
	if (ios->clock) {
		spin_unlock_irqrestore(&host->lock, flags);
		rc = msmsdcc_setup_clocks(host, true);
		if (rc)
			goto out;
		spin_lock_irqsave(&host->lock, flags);
		writel_relaxed(host->mci_irqenable, host->base + MMCIMASK0);
		mb();
		msmsdcc_cfg_sdio_wakeup(host, false);
		clock = msmsdcc_get_sup_clk_rate(host, ios->clock);

		/*
		 * For DDR50 mode, controller needs clock rate to be
		 * double than what is required on the SD card CLK pin.
		 *
		 * Setting DDR timing mode in controller before setting the
		 * clock rate will make sure that card don't see the double
		 * clock rate even for very small duration. Some eMMC
		 * cards seems to lock up if they see clock frequency > 52MHz.
		 */
		if (ios->timing == MMC_TIMING_UHS_DDR50) {
			u32 clk;

			clk = readl_relaxed(host->base + MMCICLOCK);
			clk &= ~(0x7 << 14); /* clear SELECT_IN field */
			clk |= (3 << 14); /* set DDR timing mode */
			writel_relaxed(clk, host->base + MMCICLOCK);
			msmsdcc_sync_reg_wr(host);

			clock = msmsdcc_get_sup_clk_rate(host, ios->clock * 2);
		}

		if (clock != host->clk_rate) {
			spin_unlock_irqrestore(&host->lock, flags);
			rc = clk_set_rate(host->clk, clock);
			spin_lock_irqsave(&host->lock, flags);
			if (rc < 0)
				pr_err("%s: failed to set clk rate %u\n",
						mmc_hostname(mmc), clock);
			host->clk_rate = clock;
			host->reg_write_delay =
				(1 + ((3 * USEC_PER_SEC) /
				      (host->clk_rate ? host->clk_rate :
				       msmsdcc_get_min_sup_clk_rate(host))));
			spin_unlock_irqrestore(&host->lock, flags);
			/*
			 * Update bus vote incase of frequency change due to
			 * clock scaling.
			 */
			msmsdcc_msm_bus_cancel_work_and_set_vote(host,
								&mmc->ios);
			spin_lock_irqsave(&host->lock, flags);
		}
		/*
		 * give atleast 2 MCLK cycles delay for clocks
		 * and SDCC core to stabilize
		 */
		mb();
		msmsdcc_delay(host);
		clk |= MCI_CLK_ENABLE;
	}
	if (ios->bus_width == MMC_BUS_WIDTH_8)
		clk |= MCI_CLK_WIDEBUS_8;
	else if (ios->bus_width == MMC_BUS_WIDTH_4)
		clk |= MCI_CLK_WIDEBUS_4;
	else
		clk |= MCI_CLK_WIDEBUS_1;

	if (msmsdcc_is_pwrsave(host) && mmc_host_may_gate_card(host->mmc->card))
		clk |= MCI_CLK_PWRSAVE;

	clk |= MCI_CLK_FLOWENA;

	host->tuning_needed = 0;
	/*
	 * Select the controller timing mode according
	 * to current bus speed mode
	 */
	if (host->clk_rate > (100 * 1000 * 1000) &&
	    (ios->timing == MMC_TIMING_UHS_SDR104 ||
	    ios->timing == MMC_TIMING_MMC_HS200)) {
		/* Card clock frequency must be > 100MHz to enable tuning */
		clk |= (4 << 14);
		host->tuning_needed = 1;
	} else {
		if (ios->timing == MMC_TIMING_UHS_DDR50)
			clk |= (3 << 14);
		else
			clk |= (2 << 14); /* feedback clock */

		host->tuning_done = false;
		if (atomic_read(&host->clks_on)) {
			/* Write 1 to DLL_RST bit of MCI_DLL_CONFIG register */
			writel_relaxed((readl_relaxed(host->base +
					MCI_DLL_CONFIG) | MCI_DLL_RST),
					host->base + MCI_DLL_CONFIG);

			/* Write 1 to DLL_PDN bit of MCI_DLL_CONFIG register */
			writel_relaxed((readl_relaxed(host->base +
					MCI_DLL_CONFIG) | MCI_DLL_PDN),
					host->base + MCI_DLL_CONFIG);
		}
	}

	/* Select free running MCLK as input clock of cm_dll_sdc4 */
	clk |= (2 << 23);

	if (host->io_pad_pwr_switch)
		clk |= IO_PAD_PWR_SWITCH;

	/* Don't write into registers if clocks are disabled */
	if (atomic_read(&host->clks_on)) {
		if (readl_relaxed(host->base + MMCICLOCK) != clk) {
			writel_relaxed(clk, host->base + MMCICLOCK);
			msmsdcc_sync_reg_wr(host);
		}
		if (readl_relaxed(host->base + MMCIPOWER) != pwr) {
			host->pwr = pwr;
			writel_relaxed(pwr, host->base + MMCIPOWER);
			msmsdcc_sync_reg_wr(host);
		}
	}

	if (!(clk & MCI_CLK_ENABLE) && atomic_read(&host->clks_on)) {
		msmsdcc_cfg_sdio_wakeup(host, true);
		spin_unlock_irqrestore(&host->lock, flags);
		/*
		 * May get a wake-up interrupt the instant we disable the
		 * clocks. This would disable the wake-up interrupt.
		 */
		msmsdcc_setup_clocks(host, false);
		spin_lock_irqsave(&host->lock, flags);
	}

	if (host->tuning_in_progress)
		WARN(!atomic_read(&host->clks_on),
			"tuning_in_progress but SDCC clocks are OFF\n");

	/* Let interrupts be disabled if the host is powered off */
	if (ios->power_mode != MMC_POWER_OFF && host->sdcc_irq_disabled) {
		enable_irq(host->core_irqres->start);
		host->sdcc_irq_disabled = 0;
	}
	spin_unlock_irqrestore(&host->lock, flags);
out:
	mutex_unlock(&host->clk_mutex);
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
	msmsdcc_sync_reg_wr(host);

	return 0;
}

static int msmsdcc_get_ro(struct mmc_host *mmc)
{
	int status = -ENOSYS;
	struct msmsdcc_host *host = mmc_priv(mmc);

	if (host->plat->wpswitch) {
		status = host->plat->wpswitch(mmc_dev(mmc));
	} else if (gpio_is_valid(host->plat->wpswitch_gpio)) {
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
				status ^= !host->plat->is_wpswitch_active_low;
			}
			gpio_free(host->plat->wpswitch_gpio);
		}
	}

	if (status < 0)
		status = -ENOSYS;
	pr_debug("%s: Card read-only status %d\n", __func__, status);

	return status;
}

static void msmsdcc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;

	/*
	 * We may come here with clocks turned off in that case don't
	 * attempt to write into MASK0 register. While turning on the
	 * clocks mci_irqenable will be written to MASK0 register.
	 */

	spin_lock_irqsave(&host->lock, flags);
	if (enable) {
		host->mci_irqenable |= MCI_SDIOINTOPERMASK;
		if (atomic_read(&host->clks_on)) {
			writel_relaxed(readl_relaxed(host->base + MMCIMASK0) |
				MCI_SDIOINTOPERMASK, host->base + MMCIMASK0);
			mb();
		}
	} else {
		host->mci_irqenable &= ~MCI_SDIOINTOPERMASK;
		if (atomic_read(&host->clks_on)) {
			writel_relaxed(readl_relaxed(host->base + MMCIMASK0) &
				~MCI_SDIOINTOPERMASK, host->base + MMCIMASK0);
			mb();
		}
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

#ifdef CONFIG_PM_RUNTIME
static void msmsdcc_print_rpm_info(struct msmsdcc_host *host)
{
	struct device *dev = mmc_dev(host->mmc);

	pr_err("%s: PM: sdcc_suspended=%d, pending_resume=%d, sdcc_suspending=%d\n",
		mmc_hostname(host->mmc), host->sdcc_suspended,
		host->pending_resume, host->sdcc_suspending);
	pr_err("%s: RPM: runtime_status=%d, usage_count=%d,"
		" is_suspended=%d, disable_depth=%d, runtime_error=%d,"
		" request_pending=%d, request=%d\n",
		mmc_hostname(host->mmc), dev->power.runtime_status,
		atomic_read(&dev->power.usage_count),
		dev->power.is_suspended, dev->power.disable_depth,
		dev->power.runtime_error, dev->power.request_pending,
		dev->power.request);
}

static int msmsdcc_enable(struct mmc_host *mmc)
{
	int rc = 0;
	struct device *dev = mmc->parent;
	struct msmsdcc_host *host = mmc_priv(mmc);

	msmsdcc_pm_qos_update_latency(host, 1);

	if (mmc->card && mmc_card_sdio(mmc->card))
		goto out;

	if (host->sdcc_suspended && host->pending_resume) {
		host->pending_resume = false;
		pm_runtime_get_noresume(dev);
		rc = msmsdcc_runtime_resume(dev);
		goto skip_get_sync;
	}

	if (dev->power.runtime_status == RPM_SUSPENDING) {
		if (mmc->suspend_task == current) {
			pm_runtime_get_noresume(dev);
			goto out;
		}
	} else if (dev->power.runtime_status == RPM_RESUMING) {
		pm_runtime_get_noresume(dev);
		goto out;
	}

	rc = pm_runtime_get_sync(dev);

skip_get_sync:
	if (rc < 0) {
		WARN(1, "%s: %s: failed with error %d\n", mmc_hostname(mmc),
		     __func__, rc);
		msmsdcc_print_rpm_info(host);
		return rc;
	}
out:
	return 0;
}

static int msmsdcc_disable(struct mmc_host *mmc)
{
	int rc;
	struct msmsdcc_host *host = mmc_priv(mmc);

	msmsdcc_pm_qos_update_latency(host, 0);

	if (mmc->card && mmc_card_sdio(mmc->card)) {
		rc = 0;
		goto out;
	}

	if (host->plat->disable_runtime_pm)
		return -ENOTSUPP;

	rc = pm_runtime_put_sync(mmc->parent);

	if (rc < 0) {
		WARN(1, "%s: %s: failed with error %d\n", mmc_hostname(mmc),
		     __func__, rc);
		msmsdcc_print_rpm_info(host);
		return rc;
	}

out:
	return rc;
}
#else
static void msmsdcc_print_rpm_info(struct msmsdcc_host *host) {}

static int msmsdcc_enable(struct mmc_host *mmc)
{
	struct device *dev = mmc->parent;
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;

	msmsdcc_pm_qos_update_latency(host, 1);

	if (mmc->card && mmc_card_sdio(mmc->card)) {
		rc = 0;
		goto out;
	}

	if (host->sdcc_suspended && host->pending_resume) {
		host->pending_resume = false;
		rc = msmsdcc_runtime_resume(dev);
		goto out;
	}

	mutex_lock(&host->clk_mutex);
	rc = msmsdcc_setup_clocks(host, true);
	mutex_unlock(&host->clk_mutex);

out:
	if (rc < 0) {
		pr_info("%s: %s: failed with error %d", mmc_hostname(mmc),
				__func__, rc);
		msmsdcc_pm_qos_update_latency(host, 0);
		return rc;
	}
	return 0;
}

static int msmsdcc_disable(struct mmc_host *mmc)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;

	msmsdcc_pm_qos_update_latency(host, 0);

	if (mmc->card && mmc_card_sdio(mmc->card))
		goto out;

	mutex_lock(&host->clk_mutex);
	rc = msmsdcc_setup_clocks(host, false);
	mutex_unlock(&host->clk_mutex);

	if (rc) {
		msmsdcc_pm_qos_update_latency(host, 1);
		return rc;
	}
out:
	return rc;
}
#endif

static int msmsdcc_switch_io_voltage(struct mmc_host *mmc,
				     struct mmc_ios *ios)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;
	enum vdd_io_level io_level;
	unsigned int vreg_level = 0;

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		io_level = VDD_IO_HIGH;
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		io_level = VDD_IO_LOW;
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		io_level = VDD_IO_SET_LEVEL;
		vreg_level = 1200000;
		break;
	default:
		/* invalid selection. don't do anything */
		rc = -EINVAL;
		goto out;
	}
	rc = msmsdcc_set_vdd_io_vol(host, io_level, vreg_level);
	if (!rc)
		msmsdcc_update_io_pad_pwr_switch(host);
out:
	return rc;
}

/*
 * Returns 1 if any of the data lines [0:3] state is low (card busy).
 * Returns 0 if all of the data lines [0:3] state is high (card not busy).
 */
static int msmsdcc_is_card_busy(struct mmc_host *mmc)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	u32 data_lines_mask = (0xF << 1);

	if (atomic_read(&host->clks_on))
		return !((readl_relaxed(host->base + MCI_TEST_INPUT) &
			  data_lines_mask) == data_lines_mask);

	/*
	 * Clock should ideally be running when this function is called but
	 * in case if its not running then return 0 to indicate that card is
	 * not busy.
	 */
	return 0;
}

static inline void msmsdcc_cm_sdc4_dll_set_freq(struct msmsdcc_host *host)
{
	u32 mclk_freq = 0;

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
}

/* Initialize the DLL (Programmable Delay Line ) */
static int msmsdcc_init_cm_sdc4_dll(struct msmsdcc_host *host)
{
	int rc = 0;
	unsigned long flags;
	u32 wait_cnt;
	bool prev_pwrsave, curr_pwrsave;

	spin_lock_irqsave(&host->lock, flags);
	prev_pwrsave = !!(readl_relaxed(host->base + MMCICLOCK) &
			MCI_CLK_PWRSAVE);
	curr_pwrsave = prev_pwrsave;
	/*
	 * Make sure that clock is always enabled when DLL
	 * tuning is in progress. Keeping PWRSAVE ON may
	 * turn off the clock. So let's disable the PWRSAVE
	 * here and re-enable it once tuning is completed.
	 */
	if (prev_pwrsave) {
		writel_relaxed((readl_relaxed(host->base + MMCICLOCK)
				& ~MCI_CLK_PWRSAVE), host->base + MMCICLOCK);
		msmsdcc_sync_reg_wr(host);
		curr_pwrsave = false;
	}

	/* Write 1 to DLL_RST bit of MCI_DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_DLL_RST), host->base + MCI_DLL_CONFIG);

	/* Write 1 to DLL_PDN bit of MCI_DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_DLL_PDN), host->base + MCI_DLL_CONFIG);

	msmsdcc_cm_sdc4_dll_set_freq(host);

	/* Write 0 to DLL_RST bit of MCI_DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~MCI_DLL_RST), host->base + MCI_DLL_CONFIG);

	/* Write 0 to DLL_PDN bit of MCI_DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~MCI_DLL_PDN), host->base + MCI_DLL_CONFIG);

	/* Set DLL_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_DLL_EN), host->base + MCI_DLL_CONFIG);

	/* Set CK_OUT_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_CK_OUT_EN), host->base + MCI_DLL_CONFIG);

	wait_cnt = 50;
	/* Wait until DLL_LOCK bit of MCI_DLL_STATUS register becomes '1' */
	while (!(readl_relaxed(host->base + MCI_DLL_STATUS) & MCI_DLL_LOCK)) {
		/* max. wait for 50us sec for LOCK bit to be set */
		if (--wait_cnt == 0) {
			pr_err("%s: %s: DLL failed to LOCK\n",
				mmc_hostname(host->mmc), __func__);
			rc = -ETIMEDOUT;
			goto out;
		}
		/* wait for 1us before polling again */
		udelay(1);
	}

out:
	/* Restore the correct PWRSAVE state */
	if (prev_pwrsave ^ curr_pwrsave)
		msmsdcc_set_pwrsave(host->mmc, prev_pwrsave);
	spin_unlock_irqrestore(&host->lock, flags);

	return rc;
}

static inline int msmsdcc_dll_poll_ck_out_en(struct msmsdcc_host *host,
						u8 poll)
{
	int rc = 0;
	u32 wait_cnt = 50;
	u8 ck_out_en = 0;

	/* poll for MCI_CK_OUT_EN bit.  max. poll time = 50us */
	ck_out_en = !!(readl_relaxed(host->base + MCI_DLL_CONFIG) &
			MCI_CK_OUT_EN);

	while (ck_out_en != poll) {
		if (--wait_cnt == 0) {
			pr_err("%s: %s: CK_OUT_EN bit is not %d\n",
				mmc_hostname(host->mmc), __func__, poll);
			rc = -ETIMEDOUT;
			goto out;
		}
		udelay(1);

		ck_out_en = !!(readl_relaxed(host->base + MCI_DLL_CONFIG) &
			MCI_CK_OUT_EN);
	}
out:
	return rc;
}

/*
 * Enable a CDR circuit in CM_SDC4_DLL block to enable automatic
 * calibration sequence. This function should be called before
 * enabling AUTO_CMD19 bit in MCI_CMD register for block read
 * commands (CMD17/CMD18).
 *
 * This function gets called when host spinlock acquired.
 */
static int msmsdcc_enable_cdr_cm_sdc4_dll(struct msmsdcc_host *host)
{
	int rc = 0;
	u32 config;

	config = readl_relaxed(host->base + MCI_DLL_CONFIG);
	config |= MCI_CDR_EN;
	config &= ~(MCI_CDR_EXT_EN | MCI_CK_OUT_EN);
	writel_relaxed(config, host->base + MCI_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register becomes '0' */
	rc = msmsdcc_dll_poll_ck_out_en(host, 0);
	if (rc)
		goto err_out;

	/* Set CK_OUT_EN bit of MCI_DLL_CONFIG register to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_CK_OUT_EN), host->base + MCI_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register becomes '1' */
	rc = msmsdcc_dll_poll_ck_out_en(host, 1);
	if (rc)
		goto err_out;

	goto out;

err_out:
	pr_err("%s: %s: Failed\n", mmc_hostname(host->mmc), __func__);
out:
	return rc;
}

static int msmsdcc_config_cm_sdc4_dll_phase(struct msmsdcc_host *host,
						u8 phase)
{
	int rc = 0;
	u8 grey_coded_phase_table[] = {0x0, 0x1, 0x3, 0x2, 0x6, 0x7, 0x5, 0x4,
					0xC, 0xD, 0xF, 0xE, 0xA, 0xB, 0x9,
					0x8};
	unsigned long flags;
	u32 config;

	spin_lock_irqsave(&host->lock, flags);

	config = readl_relaxed(host->base + MCI_DLL_CONFIG);
	config &= ~(MCI_CDR_EN | MCI_CK_OUT_EN);
	config |= (MCI_CDR_EXT_EN | MCI_DLL_EN);
	writel_relaxed(config, host->base + MCI_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register becomes '0' */
	rc = msmsdcc_dll_poll_ck_out_en(host, 0);
	if (rc)
		goto err_out;

	/*
	 * Write the selected DLL clock output phase (0 ... 15)
	 * to CDR_SELEXT bit field of MCI_DLL_CONFIG register.
	 */
	writel_relaxed(((readl_relaxed(host->base + MCI_DLL_CONFIG)
			& ~(0xF << 20))
			| (grey_coded_phase_table[phase] << 20)),
			host->base + MCI_DLL_CONFIG);

	/* Set CK_OUT_EN bit of MCI_DLL_CONFIG register to 1. */
	writel_relaxed((readl_relaxed(host->base + MCI_DLL_CONFIG)
			| MCI_CK_OUT_EN), host->base + MCI_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of MCI_DLL_CONFIG register becomes '1' */
	rc = msmsdcc_dll_poll_ck_out_en(host, 1);
	if (rc)
		goto err_out;

	config = readl_relaxed(host->base + MCI_DLL_CONFIG);
	config |= MCI_CDR_EN;
	config &= ~MCI_CDR_EXT_EN;
	writel_relaxed(config, host->base + MCI_DLL_CONFIG);
	goto out;

err_out:
	pr_err("%s: %s: Failed to set DLL phase: %d\n",
		mmc_hostname(host->mmc), __func__, phase);
out:
	spin_unlock_irqrestore(&host->lock, flags);
	return rc;
}

/*
 * Find out the greatest range of consecuitive selected
 * DLL clock output phases that can be used as sampling
 * setting for SD3.0 UHS-I card read operation (in SDR104
 * timing mode) or for eMMC4.5 card read operation (in HS200
 * timing mode).
 * Select the 3/4 of the range and configure the DLL with the
 * selected DLL clock output phase.
*/
static int find_most_appropriate_phase(struct msmsdcc_host *host,
				u8 *phase_table, u8 total_phases)
{
	#define MAX_PHASES 16
	int ret;
	u8 ranges[MAX_PHASES][MAX_PHASES] = { {0}, {0} };
	u8 phases_per_row[MAX_PHASES] = {0};
	int row_index = 0, col_index = 0, selected_row_index = 0, curr_max = 0;
	int i, cnt, phase_0_raw_index = 0, phase_15_raw_index = 0;
	bool phase_0_found = false, phase_15_found = false;

	if (!total_phases || (total_phases > MAX_PHASES)) {
		pr_err("%s: %s: invalid argument: total_phases=%d\n",
			mmc_hostname(host->mmc), __func__, total_phases);
		return -EINVAL;
	}

	for (cnt = 0; cnt < total_phases; cnt++) {
		ranges[row_index][col_index] = phase_table[cnt];
		phases_per_row[row_index] += 1;
		col_index++;

		if ((cnt + 1) == total_phases) {
			continue;
		/* check if next phase in phase_table is consecutive or not */
		} else if ((phase_table[cnt] + 1) != phase_table[cnt + 1]) {
			row_index++;
			col_index = 0;
		}
	}

	if (row_index >= MAX_PHASES)
		return -EINVAL;

	/* Check if phase-0 is present in first valid window? */
	if (!ranges[0][0]) {
		phase_0_found = true;
		phase_0_raw_index = 0;
		/* Check if cycle exist between 2 valid windows */
		for (cnt = 1; cnt <= row_index; cnt++) {
			if (phases_per_row[cnt]) {
				for (i = 0; i < phases_per_row[cnt]; i++) {
					if (ranges[cnt][i] == 15) {
						phase_15_found = true;
						phase_15_raw_index = cnt;
						break;
					}
				}
			}
		}
	}

	/* If 2 valid windows form cycle then merge them as single window */
	if (phase_0_found && phase_15_found) {
		/* number of phases in raw where phase 0 is present */
		u8 phases_0 = phases_per_row[phase_0_raw_index];
		/* number of phases in raw where phase 15 is present */
		u8 phases_15 = phases_per_row[phase_15_raw_index];

		if (phases_0 + phases_15 >= MAX_PHASES)
			/*
			 * If there are more than 1 phase windows then total
			 * number of phases in both the windows should not be
			 * more than or equal to MAX_PHASES.
			 */
			return -EINVAL;

		/* Merge 2 cyclic windows */
		i = phases_15;
		for (cnt = 0; cnt < phases_0; cnt++) {
			ranges[phase_15_raw_index][i] =
				ranges[phase_0_raw_index][cnt];
			if (++i >= MAX_PHASES)
				break;
		}

		phases_per_row[phase_0_raw_index] = 0;
		phases_per_row[phase_15_raw_index] = phases_15 + phases_0;
	}

	for (cnt = 0; cnt <= row_index; cnt++) {
		if (phases_per_row[cnt] > curr_max) {
			curr_max = phases_per_row[cnt];
			selected_row_index = cnt;
		}
	}

	i = ((curr_max * 3) / 4);
	if (i)
		i--;

	ret = (int)ranges[selected_row_index][i];

	if (ret >= MAX_PHASES) {
		ret = -EINVAL;
		pr_err("%s: %s: invalid phase selected=%d\n",
			mmc_hostname(host->mmc), __func__, ret);
	}

	return ret;
}

static int msmsdcc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	int rc = 0;
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long	flags;
	u8 phase, *data_buf, tuned_phases[16], tuned_phase_cnt = 0;
	const u32 *tuning_block_pattern = tuning_block_64;
	int size = sizeof(tuning_block_64); /* Tuning pattern size in bytes */
	bool is_tuning_all_phases;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);

	/* Tuning is only required for SDR104 modes */
	if (!host->tuning_needed) {
		rc = 0;
		goto exit;
	}

	spin_lock_irqsave(&host->lock, flags);
	WARN(!host->pwr, "SDCC power is turned off\n");
	WARN(!atomic_read(&host->clks_on), "SDCC clocks are turned off\n");
	WARN(host->sdcc_irq_disabled, "SDCC IRQ is disabled\n");

	host->tuning_in_progress = 1;
	if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) &&
		(mmc->ios.bus_width == MMC_BUS_WIDTH_8)) {
		tuning_block_pattern = tuning_block_128;
		size = sizeof(tuning_block_128);
	}
	spin_unlock_irqrestore(&host->lock, flags);

	/* first of all reset the tuning block */
	rc = msmsdcc_init_cm_sdc4_dll(host);
	if (rc)
		goto out;

	data_buf = kmalloc(size, GFP_KERNEL);
	if (!data_buf) {
		rc = -ENOMEM;
		goto out;
	}

	is_tuning_all_phases = !(host->mmc->card &&
		(host->saved_tuning_phase != INVALID_TUNING_PHASE));
retry:
	if (is_tuning_all_phases)
		phase = 0; /* start from phase 0 during init */
	else
		phase = (u8)host->saved_tuning_phase;
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

		cmd.opcode = opcode;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		data.blksz = size;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.timeout_ns = 1000 * 1000 * 1000; /* 1 sec */

		data.sg = &sg;
		data.sg_len = 1;
		sg_init_one(&sg, data_buf, size);
		memset(data_buf, 0, size);
		mmc_wait_for_req(mmc, &mrq);

		if (!cmd.error && !data.error &&
			!memcmp(data_buf, tuning_block_pattern, size)) {
			/* tuning is successful at this tuning point */
			if (!is_tuning_all_phases)
				goto kfree;
			tuned_phases[tuned_phase_cnt++] = phase;
			pr_debug("%s: %s: found good phase = %d\n",
				mmc_hostname(mmc), __func__, phase);
		} else if (!is_tuning_all_phases) {
			pr_debug("%s: tuning failed at saved phase (%d), retrying\n",
					mmc_hostname(mmc), (u32)phase);
			is_tuning_all_phases = true;
			goto retry;
		}
	} while (++phase < 16);

	if (tuned_phase_cnt) {
		rc = find_most_appropriate_phase(host, tuned_phases,
							tuned_phase_cnt);
		if (rc < 0)
			goto kfree;
		else
			phase = (u8)rc;

		/*
		 * Finally set the selected phase in delay
		 * line hw block.
		 */
		rc = msmsdcc_config_cm_sdc4_dll_phase(host, phase);
		if (rc)
			goto kfree;
		else
			host->saved_tuning_phase = phase;
		pr_debug("%s: %s: finally setting the tuning phase to %d\n",
				mmc_hostname(mmc), __func__, phase);
	} else {
		/* tuning failed */
		pr_err("%s: %s: no tuning point found\n",
			mmc_hostname(mmc), __func__);
		msmsdcc_dump_sdcc_state(host);
		rc = -EAGAIN;
	}

kfree:
	kfree(data_buf);
out:
	spin_lock_irqsave(&host->lock, flags);
	host->tuning_in_progress = 0;
	if (!rc)
		host->tuning_done = true;
	spin_unlock_irqrestore(&host->lock, flags);
exit:
	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return rc;
}

/**
 *	msmsdcc_stop_request - stops ongoing request
 *	@mmc: MMC host, running the request
 *
 *	Stops currently running request synchronously. All relevant request
 *	information is cleared.
 */
int msmsdcc_stop_request(struct mmc_host *mmc)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	struct mmc_request *mrq;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&host->lock, flags);
	mrq = host->curr.mrq;
	if (mrq) {
		msmsdcc_reset_and_restore(host);
		/*
		 * Note: We are just taking care of SPS. We may also
		 * need to think about ADM (and PIO?) later if required.
		 */
		if (host->sps.sg && is_sps_mode(host)) {
			if (!mrq->data->host_cookie)
				dma_unmap_sg(mmc_dev(host->mmc), host->sps.sg,
					host->sps.num_ents, host->sps.dir);
			host->sps.sg = NULL;
			host->sps.busy = 0;
		}

		/*
		 * Clear current request information as current
		 * request has ended
		 */
		memset(&host->curr, 0, sizeof(struct msmsdcc_curr_req));
		del_timer(&host->req_tout_timer);
	} else {
		rc = -EINVAL;
	}
	spin_unlock_irqrestore(&host->lock, flags);

	return rc;
}

/**
 *	msmsdcc_get_xfer_remain - returns number of bytes passed on bus
 *	@mmc: MMC host, running the request
 *
 *	Returns the number of bytes passed for SPS transfer. 0 - for non-SPS
 *	transfer.
 */
unsigned int msmsdcc_get_xfer_remain(struct mmc_host *mmc)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	u32 data_cnt = 0;

	/* Currently, we don't support to stop the non-SPS transfer */
	if (host->sps.busy && atomic_read(&host->clks_on))
		data_cnt = readl_relaxed(host->base + MMCIDATACNT);

	return data_cnt;
}

static int msmsdcc_notify_load(struct mmc_host *mmc, enum mmc_load state)
{
	int err = 0;
	unsigned long rate;
	struct msmsdcc_host *host = mmc_priv(mmc);

	if (IS_ERR_OR_NULL(host->bus_clk))
		goto out;

	switch (state) {
	case MMC_LOAD_HIGH:
		rate = MSMSDCC_BUS_VOTE_MAX_RATE;
		break;
	case MMC_LOAD_LOW:
		rate = MSMSDCC_BUS_VOTE_MIN_RATE;
		break;
	default:
		err = -EINVAL;
		goto out;
	}

	if (rate != host->bus_clk_rate) {
		err = clk_set_rate(host->bus_clk, rate);
		if (err)
			pr_err("%s: %s: bus clk set rate %lu Hz err %d\n",
					mmc_hostname(mmc), __func__, rate, err);
		else
			host->bus_clk_rate = rate;
	}
out:
	return err;
}

static const struct mmc_host_ops msmsdcc_ops = {
	.enable		= msmsdcc_enable,
	.disable	= msmsdcc_disable,
	.pre_req        = msmsdcc_pre_req,
	.post_req       = msmsdcc_post_req,
	.request	= msmsdcc_request,
	.set_ios	= msmsdcc_set_ios,
	.get_ro		= msmsdcc_get_ro,
	.enable_sdio_irq = msmsdcc_enable_sdio_irq,
	.start_signal_voltage_switch = msmsdcc_switch_io_voltage,
	.card_busy = msmsdcc_is_card_busy,
	.execute_tuning = msmsdcc_execute_tuning,
	.stop_request = msmsdcc_stop_request,
	.get_xfer_remain = msmsdcc_get_xfer_remain,
	.notify_load = msmsdcc_notify_load,
};

static void msmsdcc_enable_status_gpio(struct msmsdcc_host *host)
{
	unsigned int gpio_no = host->plat->status_gpio;
	int status;

	if (!gpio_is_valid(gpio_no))
		return;

	status = gpio_request(gpio_no, "SD_HW_Detect");
	if (status)
		pr_err("%s: %s: gpio_request(%d) failed\n",
			mmc_hostname(host->mmc), __func__, gpio_no);
}

static void msmsdcc_disable_status_gpio(struct msmsdcc_host *host)
{
	if (gpio_is_valid(host->plat->status_gpio))
		gpio_free(host->plat->status_gpio);
}

static unsigned int
msmsdcc_slot_status(struct msmsdcc_host *host)
{
	int status;

	status = gpio_get_value_cansleep(host->plat->status_gpio);
	if (host->plat->is_status_gpio_active_low)
		status = !status;

	return status;
}

static void
msmsdcc_check_status(unsigned long data)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *)data;
	unsigned int status;

	if (host->plat->status || gpio_is_valid(host->plat->status_gpio)) {
		if (host->plat->status)
			status = host->plat->status(mmc_dev(host->mmc));
		else
			status = msmsdcc_slot_status(host);

		host->eject = !status;

		if (status ^ host->oldstat) {
			if (host->plat->status)
				pr_info("%s: Slot status change detected "
					"(%d -> %d)\n",
					mmc_hostname(host->mmc),
					host->oldstat, status);
			else if (host->plat->is_status_gpio_active_low)
				pr_info("%s: Slot status change detected "
					"(%d -> %d) and the card detect GPIO"
					" is ACTIVE_LOW\n",
					mmc_hostname(host->mmc),
					host->oldstat, status);
			else
				pr_info("%s: Slot status change detected "
					"(%d -> %d) and the card detect GPIO"
					" is ACTIVE_HIGH\n",
					mmc_hostname(host->mmc),
					host->oldstat, status);
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
	if (!host->sdio_wakeupirq_disabled) {
		disable_irq_nosync(irq);
		if (host->sdcc_suspended) {
			wake_lock(&host->sdio_wlock);
			msmsdcc_disable_irq_wake(host);
		}
		host->sdio_wakeupirq_disabled = 1;
	}
	if (host->plat->is_sdio_al_client) {
		wake_lock(&host->sdio_wlock);
		spin_unlock(&host->lock);
		mmc_signal_sdio_irq(host->mmc);
		goto out_unlocked;
	}
	spin_unlock(&host->lock);

out_unlocked:
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

	/* Allocate maximum descriptor fifo size */
	sps_config->desc.size = SPS_MAX_DESC_FIFO_SIZE -
		(SPS_MAX_DESC_FIFO_SIZE % SPS_MAX_DESC_LENGTH);
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
	pr_debug("%s: %s, success !!! %s: pipe_handle=0x%x,"\
		" desc_fifo.phys_base=%pa\n", mmc_hostname(host->mmc),
		__func__, is_producer ? "READ" : "WRITE",
		(u32)sps_pipe_handle, &sps_config->desc.phys_base);
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
 * Handle BAM device's global error condition
 *
 * This is an error handler for the SDCC bam device
 *
 * This function is registered as a callback with SPS-BAM
 * driver and will called in case there are an errors for
 * the SDCC BAM deivce. Any error conditions in the BAM
 * device are global and will be result in this function
 * being called once per device.
 *
 * This function will be called from the sps driver's
 * interrupt context.
 *
 * @sps_cb_case - indicates what error it is
 * @user - Pointer to sdcc host structure
 */
static void
msmsdcc_sps_bam_global_irq_cb(enum sps_callback_case sps_cb_case, void *user)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *)user;
	struct mmc_request *mrq;
	unsigned long flags;
	int32_t error = 0;

	BUG_ON(!host);
	BUG_ON(!is_sps_mode(host));

	if (sps_cb_case == SPS_CALLBACK_BAM_ERROR_IRQ) {
		/* Reset all endpoints along with resetting bam. */
		host->sps.reset_bam = true;

		pr_err("%s: BAM Global ERROR IRQ happened\n",
			mmc_hostname(host->mmc));
		error = EAGAIN;
	} else if (sps_cb_case == SPS_CALLBACK_BAM_HRESP_ERR_IRQ) {
		/**
		 *  This means that there was an AHB access error and
		 *  the address we are trying to read/write is something
		 *  we dont have priviliges to do so.
		 */
		pr_err("%s: BAM HRESP_ERR_IRQ happened\n",
			mmc_hostname(host->mmc));
		error = EACCES;
	} else {
		/**
		 * This should not have happened ideally. If this happens
		 * there is some seriously wrong.
		 */
		pr_err("%s: BAM global IRQ callback received, type:%d\n",
			mmc_hostname(host->mmc), (u32) sps_cb_case);
		error = EIO;
	}

	spin_lock_irqsave(&host->lock, flags);

	mrq = host->curr.mrq;

	if (mrq && mrq->cmd) {
		msmsdcc_dump_sdcc_state(host);

		if (!mrq->cmd->error)
			mrq->cmd->error = -error;
		if (host->curr.data) {
			if (mrq->data && !mrq->data->error)
				mrq->data->error = -error;
			host->curr.data_xfered = 0;
			if (host->sps.sg && is_sps_mode(host)) {
				/* Stop current SPS transfer */
				msmsdcc_sps_exit_curr_xfer(host);
			} else {
				/* this condition should not have happened */
				pr_err("%s: something is seriously wrong. "\
					"Funtion: %s, line: %d\n",
					mmc_hostname(host->mmc),
					__func__, __LINE__);
			}
		} else {
			/* this condition should not have happened */
			pr_err("%s: something is seriously wrong. Funtion: "\
				"%s, line: %d\n", mmc_hostname(host->mmc),
				__func__, __LINE__);
		}
	}
	spin_unlock_irqrestore(&host->lock, flags);
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
		pr_err("%s: BAM ioremap() failed!!! resource: %pr\n",
			mmc_hostname(host->mmc), host->bam_memres);
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
	 * SPS HW will be used for data transfer size even
	 * less than SDCC FIFO size. So let's set BAM summing
	 * thresold to SPS_MIN_XFER_SIZE bytes.
	 */
	bam.summing_threshold = SPS_MIN_XFER_SIZE;
	/* SPS driver wll handle the SDCC BAM IRQ */
	bam.irq = host->bam_irqres->start;
	bam.manage = SPS_BAM_MGR_LOCAL;
	bam.callback = msmsdcc_sps_bam_global_irq_cb;
	bam.user = (void *)host;

	/* bam reset messages will be limited to 5 times */
	bam.constrained_logging = true;
	bam.logging_number = 5;

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

	pr_info("%s: Qualcomm MSM SDCC-BAM at %pr %pr\n",
		mmc_hostname(host->mmc), host->bam_memres, host->bam_irqres);
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
store_polling(struct device *dev, struct device_attribute *attr,
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
	spin_unlock_irqrestore(&host->lock, flags);
	return count;
}

static ssize_t
show_sdcc_to_mem_max_bus_bw(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			host->msm_bus_vote.is_max_bw_needed);
}

static ssize_t
store_sdcc_to_mem_max_bus_bw(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	uint32_t value;
	unsigned long flags;

	if (!kstrtou32(buf, 0, &value)) {
		spin_lock_irqsave(&host->lock, flags);
		host->msm_bus_vote.is_max_bw_needed = !!value;
		spin_unlock_irqrestore(&host->lock, flags);
	}

	return count;
}

static ssize_t
show_idle_timeout(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);

	return snprintf(buf, PAGE_SIZE, "%u (Min 5 sec)\n",
		host->idle_tout / 1000);
}

static ssize_t
store_idle_timeout(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned int long flags;
	int timeout; /* in secs */

	if (!kstrtou32(buf, 0, &timeout)
			&& (timeout > MSM_MMC_DEFAULT_IDLE_TIMEOUT / 1000)) {
		spin_lock_irqsave(&host->lock, flags);
		host->idle_tout = timeout * 1000;
		spin_unlock_irqrestore(&host->lock, flags);
	}
	return count;
}

static inline void set_auto_cmd_setting(struct device *dev,
					 const char *buf,
					 bool is_cmd19)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned int long flags;
	int temp;

	if (!kstrtou32(buf, 0, &temp)) {
		spin_lock_irqsave(&host->lock, flags);
		if (is_cmd19)
			host->en_auto_cmd19 = !!temp;
		else
			host->en_auto_cmd21 = !!temp;
		spin_unlock_irqrestore(&host->lock, flags);
	}
}

static ssize_t
show_enable_auto_cmd19(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);

	return snprintf(buf, PAGE_SIZE, "%d\n", host->en_auto_cmd19);
}

static ssize_t
store_enable_auto_cmd19(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	set_auto_cmd_setting(dev, buf, true);

	return count;
}

static ssize_t
show_enable_auto_cmd21(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);

	return snprintf(buf, PAGE_SIZE, "%d\n", host->en_auto_cmd21);
}

static ssize_t
store_enable_auto_cmd21(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	set_auto_cmd_setting(dev, buf, false);

	return count;
}

static void msmsdcc_print_regs(const char *name, void __iomem *base,
				resource_size_t phys_base,
				unsigned int no_of_regs)
{
	unsigned int i;

	if (!base)
		return;

	pr_err("===== %s: Register Dumps @phys_base=%pa, @virt_base=0x%x"\
		" =====\n", name, &phys_base, (u32)base);
	for (i = 0; i < no_of_regs; i = i + 4) {
		pr_err("Reg=0x%.2x: 0x%.8x, 0x%.8x, 0x%.8x, 0x%.8x\n", i*4,
			(u32)readl_relaxed(base + i*4),
			(u32)readl_relaxed(base + ((i+1)*4)),
			(u32)readl_relaxed(base + ((i+2)*4)),
			(u32)readl_relaxed(base + ((i+3)*4)));
	}
}

/*
 * This function prints the testbus debug output for all the
 * available SDCC controller test bus.
 *
 * Note: This function should only be called if the SDCC is clocked.
 */
static void msmsdcc_print_testbus_info(struct msmsdcc_host *host)
{
	int testbus_num;

	if (!is_testbus_debug(host))
		return;

	pr_err("== SDCC Test Bus Debug ==");
	for (testbus_num = 0; testbus_num < MAX_TESTBUS; testbus_num++) {
		writel_relaxed(((testbus_num & MCI_TESTBUS_SEL_MASK)
			       | MCI_TESTBUS_ENA),
			       host->base + MCI_TESTBUS_CONFIG);
		pr_err("TestBus(%d) = 0x%.8x\n", testbus_num,
			(u32)readl_relaxed(host->base + MCI_SDCC_DEBUG_REG));
	}
	/* Disable the test bus output */
	writel_relaxed(~MCI_TESTBUS_ENA, host->base + MCI_TESTBUS_CONFIG);
}

static void msmsdcc_dump_sdcc_state(struct msmsdcc_host *host)
{
	/* Dump current state of SDCC clocks, power and irq */
	pr_err("%s: SDCC PWR is %s\n", mmc_hostname(host->mmc),
		(host->pwr ? "ON" : "OFF"));
	pr_err("%s: SDCC clks are %s, MCLK rate=%d\n",
		mmc_hostname(host->mmc),
		(atomic_read(&host->clks_on) ? "ON" : "OFF"),
		(u32)clk_get_rate(host->clk));
	pr_err("%s: SDCC irq is %s\n", mmc_hostname(host->mmc),
		(host->sdcc_irq_disabled ? "disabled" : "enabled"));

	/* Now dump SDCC registers. Don't print FIFO registers */
	if (atomic_read(&host->clks_on)) {
		msmsdcc_print_regs("SDCC-CORE", host->base,
				   host->core_memres->start, 28);
		pr_err("%s: MCI_TEST_INPUT = 0x%.8x\n",
			mmc_hostname(host->mmc),
			readl_relaxed(host->base + MCI_TEST_INPUT));
		msmsdcc_print_testbus_info(host);
	}

	if (host->curr.data) {
		if (!msmsdcc_is_dma_possible(host, host->curr.data))
			pr_err("%s: PIO mode\n", mmc_hostname(host->mmc));
		else if (is_dma_mode(host))
			pr_err("%s: ADM mode: busy=%d, chnl=%d, crci=%d\n",
				mmc_hostname(host->mmc), host->dma.busy,
				host->dma.channel, host->dma.crci);
		else if (is_sps_mode(host)) {
			if (host->sps.busy && atomic_read(&host->clks_on))
				msmsdcc_print_regs("SDCC-DML", host->dml_base,
						   host->dml_memres->start,
						   16);
			pr_err("%s: SPS mode: busy=%d\n",
				mmc_hostname(host->mmc), host->sps.busy);
		}

		pr_err("%s: xfer_size=%d, data_xfered=%d, xfer_remain=%d\n",
			mmc_hostname(host->mmc), host->curr.xfer_size,
			host->curr.data_xfered, host->curr.xfer_remain);
	}

	if (host->sps.reset_bam)
		pr_err("%s: SPS BAM reset failed: sps reset_bam=%d\n",
			mmc_hostname(host->mmc), host->sps.reset_bam);

	pr_err("%s: got_dataend=%d, prog_enable=%d,"
		" wait_for_auto_prog_done=%d, got_auto_prog_done=%d,"
		" req_tout_ms=%d\n", mmc_hostname(host->mmc),
		host->curr.got_dataend, host->prog_enable,
		host->curr.wait_for_auto_prog_done,
		host->curr.got_auto_prog_done, host->curr.req_tout_ms);
	msmsdcc_print_rpm_info(host);
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
		if (!mrq->cmd->ignore_timeout) {
			pr_info("%s: CMD%d: Request timeout\n",
				mmc_hostname(host->mmc), mrq->cmd->opcode);
			msmsdcc_dump_sdcc_state(host);
		}

		if (!mrq->cmd->error)
			mrq->cmd->error = -ETIMEDOUT;
		host->dummy_52_needed = 0;
		if (host->curr.data) {
			if (mrq->data && !mrq->data->error)
				mrq->data->error = -ETIMEDOUT;
			host->curr.data_xfered = 0;
			if (host->dma.sg && is_dma_mode(host)) {
				msm_dmov_flush(host->dma.channel, 0);
			} else if (host->sps.sg && is_sps_mode(host)) {
				/* Stop current SPS transfer */
				msmsdcc_sps_exit_curr_xfer(host);
			} else {
				msmsdcc_clear_pio_irq_mask(host);
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
			host->curr.wait_for_auto_prog_done = false;
			msmsdcc_reset_and_restore(host);
			msmsdcc_request_end(host, mrq);
		}
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

/*
 * msmsdcc_dt_get_array - Wrapper fn to read an array of 32 bit integers
 *
 * @dev:	device node from which the property value is to be read.
 * @prop_name:	name of the property to be searched.
 * @out_array:	filled array returned to caller
 * @len:	filled array size returned to caller
 * @size:	expected size of the array
 *
 * If expected "size" doesn't match with "len" an error is returned. If
 * expected size is zero, the length of actual array is returned provided
 * return value is zero.
 *
 * RETURNS:
 * zero on success, negative error if failed.
 */
static int msmsdcc_dt_get_array(struct device *dev, const char *prop_name,
		u32 **out_array, int *len, int size)
{
	int ret = 0;
	u32 *array = NULL;
	struct device_node *np = dev->of_node;

	if (of_get_property(np, prop_name, len)) {
		size_t sz;
		sz = *len = *len / sizeof(*array);

		if (sz > 0 && !(size > 0 && (sz != size))) {
			array = devm_kzalloc(dev, sz * sizeof(*array),
					GFP_KERNEL);
			if (!array) {
				dev_err(dev, "%s: no memory\n", prop_name);
				ret = -ENOMEM;
				goto out;
			}

			ret = of_property_read_u32_array(np, prop_name,
					array, sz);
			if (ret < 0) {
				dev_err(dev, "%s: error reading array %d\n",
						prop_name, ret);
				goto out;
			}
		} else {
			dev_err(dev, "%s invalid size\n", prop_name);
			ret = -EINVAL;
			goto out;
		}
	} else {
		dev_err(dev, "%s not specified\n", prop_name);
		ret = -EINVAL;
		goto out;
	}
	*out_array = array;
out:
	if (ret)
		*len = 0;
	return ret;
}

static int msmsdcc_dt_get_pad_pull_info(struct device *dev, int id,
		struct msm_mmc_pad_pull_data **pad_pull_data)
{
	int ret = 0, base = 0, len, i;
	u32 *tmp;
	struct msm_mmc_pad_pull_data *pull_data;
	struct msm_mmc_pad_pull *pull;

	switch (id) {
	case 1:
		base = TLMM_PULL_SDC1_CLK;
		break;
	case 2:
		base = TLMM_PULL_SDC2_CLK;
		break;
	case 3:
		base = TLMM_PULL_SDC3_CLK;
		break;
	case 4:
		base = TLMM_PULL_SDC4_CLK;
		break;
	default:
		dev_err(dev, "%s: Invalid slot id\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pull_data = devm_kzalloc(dev, sizeof(struct msm_mmc_pad_pull_data),
			GFP_KERNEL);
	if (!pull_data) {
		dev_err(dev, "No memory msm_mmc_pad_pull_data\n");
		ret = -ENOMEM;
		goto err;
	}
	pull_data->size = 3; /* array size for clk, cmd, data */

	/* Allocate on, off configs for clk, cmd, data */
	pull = devm_kzalloc(dev, 2 * pull_data->size *\
			sizeof(struct msm_mmc_pad_pull), GFP_KERNEL);
	if (!pull) {
		dev_err(dev, "No memory for msm_mmc_pad_pull\n");
		ret = -ENOMEM;
		goto err;
	}
	pull_data->on = pull;
	pull_data->off = pull + pull_data->size;

	ret = msmsdcc_dt_get_array(dev, "qcom,pad-pull-on",
			&tmp, &len, pull_data->size);
	if (!ret) {
		for (i = 0; i < len; i++) {
			pull_data->on[i].no = base + i;
			pull_data->on[i].val = tmp[i];
			dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
					i, pull_data->on[i].val);
		}
	} else {
		goto err;
	}

	ret = msmsdcc_dt_get_array(dev, "qcom,pad-pull-off",
			&tmp, &len, pull_data->size);
	if (!ret) {
		for (i = 0; i < len; i++) {
			pull_data->off[i].no = base + i;
			pull_data->off[i].val = tmp[i];
			dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
					i, pull_data->off[i].val);
		}
	} else {
		goto err;
	}

	*pad_pull_data = pull_data;
err:
	return ret;
}

static int msmsdcc_dt_get_pad_drv_info(struct device *dev, int id,
		struct msm_mmc_pad_drv_data **pad_drv_data)
{
	int ret = 0, base = 0, len, i;
	u32 *tmp;
	struct msm_mmc_pad_drv_data *drv_data;
	struct msm_mmc_pad_drv *drv;

	switch (id) {
	case 1:
		base = TLMM_HDRV_SDC1_CLK;
		break;
	case 2:
		base = TLMM_HDRV_SDC2_CLK;
		break;
	case 3:
		base = TLMM_HDRV_SDC3_CLK;
		break;
	case 4:
		base = TLMM_HDRV_SDC4_CLK;
		break;
	default:
		dev_err(dev, "%s: Invalid slot id\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	drv_data = devm_kzalloc(dev, sizeof(struct msm_mmc_pad_drv_data),
			GFP_KERNEL);
	if (!drv_data) {
		dev_err(dev, "No memory for msm_mmc_pad_drv_data\n");
		ret = -ENOMEM;
		goto err;
	}
	drv_data->size = 3; /* array size for clk, cmd, data */

	/* Allocate on, off configs for clk, cmd, data */
	drv = devm_kzalloc(dev, 2 * drv_data->size *\
			sizeof(struct msm_mmc_pad_drv), GFP_KERNEL);
	if (!drv) {
		dev_err(dev, "No memory msm_mmc_pad_drv\n");
		ret = -ENOMEM;
		goto err;
	}
	drv_data->on = drv;
	drv_data->off = drv + drv_data->size;

	ret = msmsdcc_dt_get_array(dev, "qcom,pad-drv-on",
			&tmp, &len, drv_data->size);
	if (!ret) {
		for (i = 0; i < len; i++) {
			drv_data->on[i].no = base + i;
			drv_data->on[i].val = tmp[i];
			dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
					i, drv_data->on[i].val);
		}
	} else {
		goto err;
	}

	ret = msmsdcc_dt_get_array(dev, "qcom,pad-drv-off",
			&tmp, &len, drv_data->size);
	if (!ret) {
		for (i = 0; i < len; i++) {
			drv_data->off[i].no = base + i;
			drv_data->off[i].val = tmp[i];
			dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
					i, drv_data->off[i].val);
		}
	} else {
		goto err;
	}

	*pad_drv_data = drv_data;
err:
	return ret;
}

static void msmsdcc_dt_get_cd_wp_gpio(struct device *dev,
		struct mmc_platform_data *pdata)
{
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	struct device_node *np = dev->of_node;

	pdata->status_gpio = of_get_named_gpio_flags(np,
			"cd-gpios", 0, &flags);
	if (gpio_is_valid(pdata->status_gpio)) {
		struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
		pdata->status_irq = platform_get_irq_byname(pdev, "status_irq");
		pdata->is_status_gpio_active_low = flags & OF_GPIO_ACTIVE_LOW;
	}

	pdata->wpswitch_gpio = of_get_named_gpio_flags(np,
			"wp-gpios", 0, &flags);
	if (gpio_is_valid(pdata->wpswitch_gpio))
		pdata->is_wpswitch_active_low = flags & OF_GPIO_ACTIVE_LOW;
}

static int msmsdcc_dt_parse_gpio_info(struct device *dev,
		struct mmc_platform_data *pdata)
{
	int ret = 0, id = 0, cnt, i;
	struct msm_mmc_pin_data *pin_data;
	struct device_node *np = dev->of_node;

	msmsdcc_dt_get_cd_wp_gpio(dev, pdata);

	pin_data = devm_kzalloc(dev, sizeof(*pin_data), GFP_KERNEL);
	if (!pin_data) {
		dev_err(dev, "No memory for pin_data\n");
		ret = -ENOMEM;
		goto err;
	}

	cnt = of_gpio_count(np);
	if (cnt > 0) {
		pin_data->is_gpio = true;

		pin_data->gpio_data = devm_kzalloc(dev,
				sizeof(struct msm_mmc_gpio_data), GFP_KERNEL);
		if (!pin_data->gpio_data) {
			dev_err(dev, "No memory for gpio_data\n");
			ret = -ENOMEM;
			goto err;
		}
		pin_data->gpio_data->size = cnt;
		pin_data->gpio_data->gpio = devm_kzalloc(dev,
				cnt * sizeof(struct msm_mmc_gpio), GFP_KERNEL);
		if (!pin_data->gpio_data->gpio) {
			dev_err(dev, "No memory for gpio\n");
			ret = -ENOMEM;
			goto err;
		}

		for (i = 0; i < cnt; i++) {
			const char *name = NULL;
			char result[32];
			pin_data->gpio_data->gpio[i].no = of_get_gpio(np, i);
			of_property_read_string_index(np,
					"qcom,gpio-names", i, &name);

			snprintf(result, 32, "%s-%s",
					dev_name(dev), name ? name : "?");
			pin_data->gpio_data->gpio[i].name = result;
			dev_dbg(dev, "%s: gpio[%s] = %d\n", __func__,
					pin_data->gpio_data->gpio[i].name,
					pin_data->gpio_data->gpio[i].no);
		}
	} else {
		pin_data->pad_data = devm_kzalloc(dev,
				sizeof(struct msm_mmc_pad_data), GFP_KERNEL);
		if (!pin_data->pad_data) {
			dev_err(dev, "No memory for pin_data->pad_data\n");
			ret = -ENOMEM;
			goto err;
		}

		of_property_read_u32(np, "cell-index", &id);

		ret = msmsdcc_dt_get_pad_pull_info(dev, id,
				&pin_data->pad_data->pull);
		if (ret)
			goto err;
		ret = msmsdcc_dt_get_pad_drv_info(dev, id,
				&pin_data->pad_data->drv);
		if (ret)
			goto err;
	}

	pdata->pin_data = pin_data;
err:
	if (ret)
		dev_err(dev, "%s failed with err %d\n", __func__, ret);
	return ret;
}

#define MAX_PROP_SIZE 32
static int msmsdcc_dt_parse_vreg_info(struct device *dev,
		struct msm_mmc_reg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct msm_mmc_reg_data *vreg;
	struct device_node *np = dev->of_node;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (of_parse_phandle(np, prop_name, 0)) {
		vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
		if (!vreg) {
			dev_err(dev, "No memory for vreg: %s\n", vreg_name);
			ret = -ENOMEM;
			goto err;
		}

		vreg->name = vreg_name;

		snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-always-on", vreg_name);
		if (of_get_property(np, prop_name, NULL))
			vreg->always_on = true;

		snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-lpm-sup", vreg_name);
		if (of_get_property(np, prop_name, NULL))
			vreg->lpm_sup = true;

		snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-voltage-level", vreg_name);
		prop = of_get_property(np, prop_name, &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_warn(dev, "%s %s property\n",
				prop ? "invalid format" : "no", prop_name);
		} else {
			vreg->low_vol_level = be32_to_cpup(&prop[0]);
			vreg->high_vol_level = be32_to_cpup(&prop[1]);
		}

		snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-current-level", vreg_name);
		prop = of_get_property(np, prop_name, &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_warn(dev, "%s %s property\n",
				prop ? "invalid format" : "no", prop_name);
		} else {
			vreg->lpm_uA = be32_to_cpup(&prop[0]);
			vreg->hpm_uA = be32_to_cpup(&prop[1]);
		}

		*vreg_data = vreg;
		dev_dbg(dev, "%s: %s %s vol=[%d %d]uV, curr=[%d %d]uA\n",
			vreg->name, vreg->always_on ? "always_on," : "",
			vreg->lpm_sup ? "lpm_sup," : "", vreg->low_vol_level,
			vreg->high_vol_level, vreg->lpm_uA, vreg->hpm_uA);
	}

err:
	return ret;
}

static struct mmc_platform_data *msmsdcc_populate_pdata(struct device *dev)
{
	int i, ret;
	struct mmc_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 bus_width = 0;
	u32 *clk_table = NULL, *sup_voltages = NULL;
	int clk_table_len, sup_volt_len, len;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		goto err;
	}

	of_property_read_u32(np, "qcom,bus-width", &bus_width);
	if (bus_width == 8) {
		pdata->mmc_bus_width = MMC_CAP_8_BIT_DATA;
	} else if (bus_width == 4) {
		pdata->mmc_bus_width = MMC_CAP_4_BIT_DATA;
	} else {
		dev_notice(dev, "Invalid bus width, default to 1 bit mode\n");
		pdata->mmc_bus_width = 0;
	}

	ret = msmsdcc_dt_get_array(dev, "qcom,sup-voltages",
			&sup_voltages, &sup_volt_len, 0);
	if (!ret) {
		for (i = 0; i < sup_volt_len; i += 2) {
			u32 mask;

			mask = mmc_vddrange_to_ocrmask(sup_voltages[i],
					sup_voltages[i + 1]);
			if (!mask)
				dev_err(dev, "Invalide voltage range %d\n", i);
			pdata->ocr_mask |= mask;
		}
		dev_dbg(dev, "OCR mask=0x%x\n", pdata->ocr_mask);
	}

	ret = msmsdcc_dt_get_array(dev, "qcom,clk-rates",
			&clk_table, &clk_table_len, 0);
	if (!ret) {
		pdata->sup_clk_table = clk_table;
		pdata->sup_clk_cnt = clk_table_len;
	}

	pdata->vreg_data = devm_kzalloc(dev,
			sizeof(struct msm_mmc_slot_reg_data), GFP_KERNEL);
	if (!pdata->vreg_data) {
		dev_err(dev, "could not allocate memory for vreg_data\n");
		goto err;
	}

	if (msmsdcc_dt_parse_vreg_info(dev,
			&pdata->vreg_data->vdd_data, "vdd"))
		goto err;

	if (msmsdcc_dt_parse_vreg_info(dev,
			&pdata->vreg_data->vdd_io_data, "vdd-io"))
		goto err;

	if (msmsdcc_dt_parse_gpio_info(dev, pdata))
		goto err;

	len = of_property_count_strings(np, "qcom,bus-speed-mode");

	for (i = 0; i < len; i++) {
		const char *name = NULL;

		of_property_read_string_index(np,
			"qcom,bus-speed-mode", i, &name);
		if (!name)
			continue;

		if (!strncmp(name, "SDR12", sizeof("SDR12")))
			pdata->uhs_caps |= MMC_CAP_UHS_SDR12;
		else if (!strncmp(name, "SDR25", sizeof("SDR25")))
			pdata->uhs_caps |= MMC_CAP_UHS_SDR25;
		else if (!strncmp(name, "SDR50", sizeof("SDR50")))
			pdata->uhs_caps |= MMC_CAP_UHS_SDR50;
		else if (!strncmp(name, "DDR50", sizeof("DDR50")))
			pdata->uhs_caps |= MMC_CAP_UHS_DDR50;
		else if (!strncmp(name, "SDR104", sizeof("SDR104")))
			pdata->uhs_caps |= MMC_CAP_UHS_SDR104;
		else if (!strncmp(name, "HS200_1p8v", sizeof("HS200_1p8v")))
			pdata->uhs_caps2 |= MMC_CAP2_HS200_1_8V_SDR;
		else if (!strncmp(name, "HS200_1p2v", sizeof("HS200_1p2v")))
			pdata->uhs_caps2 |= MMC_CAP2_HS200_1_2V_SDR;
		else if (!strncmp(name, "DDR_1p8v", sizeof("DDR_1p8v")))
			pdata->uhs_caps |= MMC_CAP_1_8V_DDR
						| MMC_CAP_UHS_DDR50;
		else if (!strncmp(name, "DDR_1p2v", sizeof("DDR_1p2v")))
			pdata->uhs_caps |= MMC_CAP_1_2V_DDR
						| MMC_CAP_UHS_DDR50;
	}

	if (of_get_property(np, "qcom,nonremovable", NULL))
		pdata->nonremovable = true;
	if (of_get_property(np, "qcom,disable-cmd23", NULL))
		pdata->disable_cmd23 = true;
	of_property_read_u32(np, "qcom,dat1-mpm-int",
					&pdata->mpm_sdiowakeup_int);

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

	if (disable_slots & (1 << (pdev->id - 1))) {
		pr_info("%s: Slot %d disabled\n", __func__, pdev->id);
		return -ENODEV;
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

	core_memres = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "core_mem");
	bam_memres = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "bam_mem");
	dml_memres = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "dml_mem");
	core_irqres = platform_get_resource_byname(pdev,
			IORESOURCE_IRQ, "core_irq");
	bam_irqres = platform_get_resource_byname(pdev,
			IORESOURCE_IRQ, "bam_irq");
	dmares = platform_get_resource_byname(pdev,
			IORESOURCE_DMA, "dma_chnl");
	dma_crci_res = platform_get_resource_byname(pdev,
			IORESOURCE_DMA, "dma_crci");

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
	host->pdev = pdev;
	host->plat = plat;
	host->mmc = mmc;
	host->curr.cmd = NULL;

	if (!plat->disable_bam && bam_memres && dml_memres && bam_irqres)
		set_hw_caps(host, MSMSDCC_SPS_BAM_SUP);
	else if (dmares)
		set_hw_caps(host, MSMSDCC_DMA_SUP);

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
	mutex_init(&host->clk_mutex);

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
	if (is_dma_mode(host)) {
		/* Setup DMA */
		ret = msmsdcc_init_dma(host);
		if (ret)
			goto ioremap_free;
	} else {
		host->dma.channel = -1;
		host->dma.crci = -1;
	}

	/*
	 * Setup SDCC bus voter clock.
	 */
	host->bus_clk = clk_get(&pdev->dev, "bus_clk");
	if (!IS_ERR_OR_NULL(host->bus_clk)) {
		/* Vote for max. clk rate for max. performance */
		ret = clk_set_rate(host->bus_clk, MSMSDCC_BUS_VOTE_MAX_RATE);
		if (ret)
			goto bus_clk_put;
		ret = clk_prepare_enable(host->bus_clk);
		if (ret)
			goto bus_clk_put;
		host->bus_clk_rate = MSMSDCC_BUS_VOTE_MAX_RATE;
	}

	/*
	 * Setup main peripheral bus clock
	 */
	host->pclk = clk_get(&pdev->dev, "iface_clk");
	if (!IS_ERR(host->pclk)) {
		ret = clk_prepare_enable(host->pclk);
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

	ret = clk_prepare_enable(host->clk);
	if (ret)
		goto clk_put;

	host->clk_rate = clk_get_rate(host->clk);
	if (!host->clk_rate)
		dev_err(&pdev->dev, "Failed to read MCLK\n");

	set_default_hw_caps(host);
	host->saved_tuning_phase = INVALID_TUNING_PHASE;

	/*
	 * Set the register write delay according to min. clock frequency
	 * supported and update later when the host->clk_rate changes.
	 */
	host->reg_write_delay =
		(1 + ((3 * USEC_PER_SEC) /
		      msmsdcc_get_min_sup_clk_rate(host)));

	atomic_set(&host->clks_on, 1);

	ret = msmsdcc_msm_bus_register(host);
	if (ret)
		goto clk_disable;

	if (host->msm_bus_vote.client_handle)
		INIT_DELAYED_WORK(&host->msm_bus_vote.vote_work,
				  msmsdcc_msm_bus_work);

	msmsdcc_msm_bus_cancel_work_and_set_vote(host, &mmc->ios);

	/* Disable SDHCi mode if supported */
	if (is_sdhci_supported(host))
		writel_relaxed(0, (host->base + MCI_CORE_HC_MODE));

	/* Apply Hard reset to SDCC to put it in power on default state */
	msmsdcc_hard_reset(host);

#define MSM_MMC_DEFAULT_CPUDMA_LATENCY 200 /* usecs */
	/* pm qos request to prevent apps idle power collapse */
	if (host->plat->cpu_dma_latency)
		host->cpu_dma_latency = host->plat->cpu_dma_latency;
	else
		host->cpu_dma_latency = MSM_MMC_DEFAULT_CPUDMA_LATENCY;
	pm_qos_add_request(&host->pm_qos_req_dma,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	ret = msmsdcc_vreg_init(host, true);
	if (ret) {
		pr_err("%s: msmsdcc_vreg_init() failed (%d)\n", __func__, ret);
		goto pm_qos_remove;
	}


	/* Clocks has to be running before accessing SPS/DML HW blocks */
	if (is_sps_mode(host)) {
		/* Initialize SPS */
		ret = msmsdcc_sps_init(host);
		if (ret)
			goto vreg_deinit;
		/* Initialize DML */
		ret = msmsdcc_dml_init(host);
		if (ret)
			goto sps_exit;
	}
	mmc_dev(mmc)->dma_mask = &dma_mask;

	/*
	 * Setup MMC host structure
	 */
	mmc->ops = &msmsdcc_ops;
	mmc->f_min = msmsdcc_get_min_sup_clk_rate(host);
	mmc->f_max = msmsdcc_get_max_sup_clk_rate(host);
	mmc->ocr_avail = plat->ocr_mask;
	mmc->clkgate_delay = MSM_MMC_CLK_GATE_DELAY;

	mmc->pm_caps |= MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;
	mmc->caps |= plat->mmc_bus_width;
	mmc->caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;
	mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY | MMC_CAP_ERASE;
	/*
	 * If we send the CMD23 before multi block write/read command
	 * then we need not to send CMD12 at the end of the transfer.
	 * If we don't send the CMD12 then only way to detect the PROG_DONE
	 * status is to use the AUTO_PROG_DONE status provided by SDCC4
	 * controller. So let's enable the CMD23 for SDCC4 only.
	 */
	if (!plat->disable_cmd23 && is_auto_prog_done(host))
		mmc->caps |= MMC_CAP_CMD23;

	mmc->caps |= plat->uhs_caps;
	mmc->caps2 |= plat->uhs_caps2;

	mmc->max_current_180 = msmsdcc_get_vreg_vdd_max_current(host);
	mmc->max_current_300 = msmsdcc_get_vreg_vdd_max_current(host);
	mmc->max_current_330 = msmsdcc_get_vreg_vdd_max_current(host);

	mmc->caps2 |= MMC_CAP2_PACKED_WR;
	mmc->caps2 |= MMC_CAP2_PACKED_WR_CONTROL;
	mmc->caps2 |= (MMC_CAP2_BOOTPART_NOACC | MMC_CAP2_DETECT_ON_ERR);
	mmc->caps2 |= MMC_CAP2_SANITIZE;
	mmc->caps2 |= MMC_CAP2_CACHE_CTRL;
	mmc->caps2 |= MMC_CAP2_POWEROFF_NOTIFY;
	mmc->caps2 |= MMC_CAP2_STOP_REQUEST;
	mmc->caps2 |= MMC_CAP2_ASYNC_SDIO_IRQ_4BIT_MODE;

	if (plat->nonremovable)
		mmc->caps |= MMC_CAP_NONREMOVABLE;
	mmc->caps |= MMC_CAP_SDIO_IRQ;

	if (plat->is_sdio_al_client)
		mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;

	mmc->max_segs = msmsdcc_get_nr_sg(host);
	mmc->max_blk_size = MMC_MAX_BLK_SIZE;
	mmc->max_blk_count = MMC_MAX_BLK_CNT;

	mmc->max_req_size = MMC_MAX_REQ_SIZE;
	mmc->max_seg_size = mmc->max_req_size;

	writel_relaxed(0, host->base + MMCIMASK0);
	writel_relaxed(MCI_CLEAR_STATIC_MASK, host->base + MMCICLEAR);
	msmsdcc_sync_reg_wr(host);

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

	if (!plat->sdiowakeup_irq) {
		/* Check if registered as IORESOURCE_IRQ */
		plat->sdiowakeup_irq =
			platform_get_irq_byname(pdev, "sdiowakeup_irq");
		if (plat->sdiowakeup_irq < 0)
			plat->sdiowakeup_irq = 0;
	}

	if (plat->sdiowakeup_irq) {
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
			if (!host->sdio_wakeupirq_disabled) {
				disable_irq_nosync(plat->sdiowakeup_irq);
				host->sdio_wakeupirq_disabled = 1;
			}
			spin_unlock_irqrestore(&host->lock, flags);
		}
	}

	if (plat->sdiowakeup_irq || plat->mpm_sdiowakeup_int) {
		wake_lock_init(&host->sdio_wlock, WAKE_LOCK_SUSPEND,
				mmc_hostname(mmc));
	}

	wake_lock_init(&host->sdio_suspend_wlock, WAKE_LOCK_SUSPEND,
			mmc_hostname(mmc));
	/*
	 * Setup card detect change
	 */

	if (!plat->status_gpio)
		plat->status_gpio = -ENOENT;
	if (!plat->wpswitch_gpio)
		plat->wpswitch_gpio = -ENOENT;

	if (plat->status || gpio_is_valid(plat->status_gpio)) {
		if (plat->status) {
			host->oldstat = plat->status(mmc_dev(host->mmc));
		} else {
			msmsdcc_enable_status_gpio(host);
			host->oldstat = msmsdcc_slot_status(host);
		}
		host->eject = !host->oldstat;
	}

	if (plat->status_irq) {
		ret = request_threaded_irq(plat->status_irq, NULL,
				  msmsdcc_platform_status_irq,
				  plat->irq_flags | IRQF_ONESHOT,
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
	pm_runtime_enable(&(pdev)->dev);
#else
	if (mmc->caps & MMC_CAP_NONREMOVABLE) {
		pm_runtime_enable(&(pdev)->dev);
	}
#endif
	host->idle_tout = MSM_MMC_DEFAULT_IDLE_TIMEOUT;
	setup_timer(&host->req_tout_timer, msmsdcc_req_tout_timer_hdlr,
			(unsigned long)host);

	mmc_add_host(mmc);

	mmc->clk_scaling.up_threshold = 35;
	mmc->clk_scaling.down_threshold = 5;
	mmc->clk_scaling.polling_delay_ms = 100;
	mmc->caps2 |= MMC_CAP2_CLK_SCALE;

	pr_info("%s: Qualcomm MSM SDCC-core %pr %pr,%d dma %d dmacrcri %d\n",
		mmc_hostname(mmc), core_memres, core_irqres,
		(unsigned int) plat->status_irq, host->dma.channel,
		host->dma.crci);

	pr_info("%s: Controller capabilities: 0x%.8x\n",
			mmc_hostname(mmc), host->hw_caps);
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

	if (is_dma_mode(host) && host->dma.channel != -1
			&& host->dma.crci != -1) {
		pr_info("%s: DM non-cached buffer at %p, dma_addr: %pa\n",
		       mmc_hostname(mmc), host->dma.nc, &host->dma.nc_busaddr);
		pr_info("%s: DM cmd busaddr: %pa, cmdptr busaddr: %pa\n",
		       mmc_hostname(mmc), &host->dma.cmd_busaddr,
		       &host->dma.cmdptr_busaddr);
	} else if (is_sps_mode(host)) {
		pr_info("%s: SPS-BAM data transfer mode available\n",
			mmc_hostname(mmc));
	} else
		pr_info("%s: PIO transfer enabled\n", mmc_hostname(mmc));

#if defined(CONFIG_DEBUG_FS)
	msmsdcc_dbg_createhost(host);
#endif

	host->max_bus_bw.show = show_sdcc_to_mem_max_bus_bw;
	host->max_bus_bw.store = store_sdcc_to_mem_max_bus_bw;
	sysfs_attr_init(&host->max_bus_bw.attr);
	host->max_bus_bw.attr.name = "max_bus_bw";
	host->max_bus_bw.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&pdev->dev, &host->max_bus_bw);
	if (ret)
		goto platform_irq_free;

	if (!plat->status_irq) {
		host->polling.show = show_polling;
		host->polling.store = store_polling;
		sysfs_attr_init(&host->polling.attr);
		host->polling.attr.name = "polling";
		host->polling.attr.mode = S_IRUGO | S_IWUSR;
		ret = device_create_file(&pdev->dev, &host->polling);
		if (ret)
			goto remove_max_bus_bw_file;
	}
	host->idle_timeout.show = show_idle_timeout;
	host->idle_timeout.store = store_idle_timeout;
	sysfs_attr_init(&host->idle_timeout.attr);
	host->idle_timeout.attr.name = "idle_timeout";
	host->idle_timeout.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&pdev->dev, &host->idle_timeout);
	if (ret)
		goto remove_polling_file;

	if (!is_auto_cmd19(host))
		goto add_auto_cmd21_atrr;

	/* Sysfs entry for AUTO CMD19 control */
	host->auto_cmd19_attr.show = show_enable_auto_cmd19;
	host->auto_cmd19_attr.store = store_enable_auto_cmd19;
	sysfs_attr_init(&host->auto_cmd19_attr.attr);
	host->auto_cmd19_attr.attr.name = "enable_auto_cmd19";
	host->auto_cmd19_attr.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&pdev->dev, &host->auto_cmd19_attr);
	if (ret)
		goto remove_idle_timeout_file;

 add_auto_cmd21_atrr:
	if (!is_auto_cmd21(host))
		goto exit;

	/* Sysfs entry for AUTO CMD21 control */
	host->auto_cmd21_attr.show = show_enable_auto_cmd21;
	host->auto_cmd21_attr.store = store_enable_auto_cmd21;
	sysfs_attr_init(&host->auto_cmd21_attr.attr);
	host->auto_cmd21_attr.attr.name = "enable_auto_cmd21";
	host->auto_cmd21_attr.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&pdev->dev, &host->auto_cmd21_attr);
	if (ret)
		goto remove_auto_cmd19_attr_file;

 exit:
	return 0;

 remove_auto_cmd19_attr_file:
	if (is_auto_cmd19(host))
		device_remove_file(&pdev->dev, &host->auto_cmd19_attr);
 remove_idle_timeout_file:
	device_remove_file(&pdev->dev, &host->idle_timeout);
 remove_polling_file:
	if (!plat->status_irq)
		device_remove_file(&pdev->dev, &host->polling);
 remove_max_bus_bw_file:
	device_remove_file(&pdev->dev, &host->max_bus_bw);
 platform_irq_free:
	del_timer_sync(&host->req_tout_timer);
	pm_runtime_disable(&(pdev)->dev);
	pm_runtime_set_suspended(&(pdev)->dev);

	if (plat->status_irq)
		free_irq(plat->status_irq, host);
	msmsdcc_disable_status_gpio(host);
 sdiowakeup_irq_free:
	if (plat->sdiowakeup_irq || plat->mpm_sdiowakeup_int)
		wake_lock_destroy(&host->sdio_wlock);
	wake_lock_destroy(&host->sdio_suspend_wlock);
	if (plat->sdiowakeup_irq)
		free_irq(plat->sdiowakeup_irq, host);
 pio_irq_free:
	free_irq(core_irqres->start, host);
 irq_free:
	free_irq(core_irqres->start, host);
 dml_exit:
	if (is_sps_mode(host))
		msmsdcc_dml_exit(host);
 sps_exit:
	if (is_sps_mode(host))
		msmsdcc_sps_exit(host);
 vreg_deinit:
	msmsdcc_vreg_init(host, false);
 pm_qos_remove:
	if (host->cpu_dma_latency)
		pm_qos_remove_request(&host->pm_qos_req_dma);
	msmsdcc_msm_bus_cancel_work_and_set_vote(host, NULL);
	msmsdcc_msm_bus_unregister(host);
 clk_disable:
	clk_disable_unprepare(host->clk);
 clk_put:
	clk_put(host->clk);
 pclk_disable:
	if (!IS_ERR(host->pclk))
		clk_disable_unprepare(host->pclk);
 pclk_put:
	if (!IS_ERR(host->pclk))
		clk_put(host->pclk);
	if (!IS_ERR_OR_NULL(host->bus_clk))
		clk_disable_unprepare(host->bus_clk);
 bus_clk_put:
	if (!IS_ERR_OR_NULL(host->bus_clk))
		clk_put(host->bus_clk);
	if (is_dma_mode(host)) {
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

#ifdef CONFIG_DEBUG_FS
static void msmsdcc_remove_debugfs(struct msmsdcc_host *host)
{
	debugfs_remove_recursive(host->debugfs_host_dir);
	host->debugfs_host_dir = NULL;
}
#else
static void msmsdcc_remove_debugfs(struct msmsdcc_host *host) {}
#endif

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

	if (is_auto_cmd19(host))
		device_remove_file(&pdev->dev, &host->auto_cmd19_attr);
	if (is_auto_cmd21(host))
		device_remove_file(&pdev->dev, &host->auto_cmd21_attr);
	device_remove_file(&pdev->dev, &host->max_bus_bw);
	if (!plat->status_irq)
		device_remove_file(&pdev->dev, &host->polling);
	device_remove_file(&pdev->dev, &host->idle_timeout);

	msmsdcc_remove_debugfs(host);

	del_timer_sync(&host->req_tout_timer);
	tasklet_kill(&host->dma_tlet);
	tasklet_kill(&host->sps.tlet);
	mmc_remove_host(mmc);

	if (plat->status_irq)
		free_irq(plat->status_irq, host);
	msmsdcc_disable_status_gpio(host);

	wake_lock_destroy(&host->sdio_suspend_wlock);
	if (plat->sdiowakeup_irq) {
		irq_set_irq_wake(plat->sdiowakeup_irq, 0);
		free_irq(plat->sdiowakeup_irq, host);
	}

	if (plat->sdiowakeup_irq || plat->mpm_sdiowakeup_int)
		wake_lock_destroy(&host->sdio_wlock);

	free_irq(host->core_irqres->start, host);
	free_irq(host->core_irqres->start, host);

	clk_put(host->clk);
	if (!IS_ERR(host->pclk))
		clk_put(host->pclk);
	if (!IS_ERR_OR_NULL(host->bus_clk))
		clk_put(host->bus_clk);

	if (host->cpu_dma_latency)
		pm_qos_remove_request(&host->pm_qos_req_dma);

	if (host->msm_bus_vote.client_handle) {
		msmsdcc_msm_bus_cancel_work_and_set_vote(host, NULL);
		msmsdcc_msm_bus_unregister(host);
	}

	msmsdcc_vreg_init(host, false);

	if (is_dma_mode(host)) {
		if (host->dmares)
			dma_free_coherent(NULL,
					sizeof(struct msmsdcc_nc_dmadata),
					host->dma.nc, host->dma.nc_busaddr);
	}

	if (is_sps_mode(host)) {
		msmsdcc_dml_exit(host);
		msmsdcc_sps_exit(host);
	}

	iounmap(host->base);
	mmc_free_host(mmc);

	pm_runtime_disable(&(pdev)->dev);
	pm_runtime_set_suspended(&(pdev)->dev);

	return 0;
}

#ifdef CONFIG_MSM_SDIO_AL
int msmsdcc_sdio_al_lpm(struct mmc_host *mmc, bool enable)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;
	int rc = 0;

	mutex_lock(&host->clk_mutex);
	spin_lock_irqsave(&host->lock, flags);
	pr_debug("%s: %sabling LPM\n", mmc_hostname(mmc),
			enable ? "En" : "Dis");

	if (enable) {
		if (!host->sdcc_irq_disabled) {
			writel_relaxed(0, host->base + MMCIMASK0);
			disable_irq_nosync(host->core_irqres->start);
			host->sdcc_irq_disabled = 1;
		}
		rc = msmsdcc_setup_clocks(host, false);
		if (rc)
			goto out;

		if (host->plat->sdio_lpm_gpio_setup &&
				!host->sdio_gpio_lpm) {
			spin_unlock_irqrestore(&host->lock, flags);
			host->plat->sdio_lpm_gpio_setup(mmc_dev(mmc), 0);
			spin_lock_irqsave(&host->lock, flags);
			host->sdio_gpio_lpm = 1;
		}

		if (host->sdio_wakeupirq_disabled) {
			msmsdcc_enable_irq_wake(host);
			enable_irq(host->plat->sdiowakeup_irq);
			host->sdio_wakeupirq_disabled = 0;
		}
	} else {
		rc = msmsdcc_setup_clocks(host, true);
		if (rc)
			goto out;

		if (!host->sdio_wakeupirq_disabled) {
			disable_irq_nosync(host->plat->sdiowakeup_irq);
			host->sdio_wakeupirq_disabled = 1;
			msmsdcc_disable_irq_wake(host);
		}

		if (host->plat->sdio_lpm_gpio_setup &&
				host->sdio_gpio_lpm) {
			spin_unlock_irqrestore(&host->lock, flags);
			host->plat->sdio_lpm_gpio_setup(mmc_dev(mmc), 1);
			spin_lock_irqsave(&host->lock, flags);
			host->sdio_gpio_lpm = 0;
		}

		if (host->sdcc_irq_disabled && atomic_read(&host->clks_on)) {
			writel_relaxed(host->mci_irqenable,
				       host->base + MMCIMASK0);
			mb();
			enable_irq(host->core_irqres->start);
			host->sdcc_irq_disabled = 0;
		}
	}
out:
	spin_unlock_irqrestore(&host->lock, flags);
	mutex_unlock(&host->clk_mutex);
	return rc;
}
#else
int msmsdcc_sdio_al_lpm(struct mmc_host *mmc, bool enable)
{
	return 0;
}
#endif

#ifdef CONFIG_PM
#ifdef CONFIG_MMC_CLKGATE
static inline void msmsdcc_gate_clock(struct msmsdcc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	unsigned long flags;

	mmc_host_clk_hold(mmc);
	spin_lock_irqsave(&mmc->clk_lock, flags);
	mmc->clk_old = mmc->ios.clock;
	mmc->ios.clock = 0;
	mmc->clk_gated = true;
	spin_unlock_irqrestore(&mmc->clk_lock, flags);
	mmc_set_ios(mmc);
	mmc_host_clk_release(mmc);
}

static inline void msmsdcc_ungate_clock(struct msmsdcc_host *host)
{
	struct mmc_host *mmc = host->mmc;

	mmc_host_clk_hold(mmc);
	mmc->ios.clock = host->clk_rate;
	mmc_set_ios(mmc);
	mmc_host_clk_release(mmc);
}
#else
static inline void msmsdcc_gate_clock(struct msmsdcc_host *host)
{
	struct mmc_host *mmc = host->mmc;

	mmc->ios.clock = 0;
	mmc_set_ios(mmc);
}

static inline void msmsdcc_ungate_clock(struct msmsdcc_host *host)
{
	struct mmc_host *mmc = host->mmc;

	mmc->ios.clock = host->clk_rate;
	mmc_set_ios(mmc);
}
#endif

#ifdef CONFIG_DEBUG_FS
static void msmsdcc_print_pm_stats(struct msmsdcc_host *host, ktime_t start,
				   const char *func, int err)
{
	ktime_t diff;

	if (host->print_pm_stats && !err) {
		diff = ktime_sub(ktime_get(), start);
		pr_info("%s: %s: Completed in %llu usec\n",
			mmc_hostname(host->mmc), func, (u64)ktime_to_us(diff));
	}
}
#else
static void msmsdcc_print_pm_stats(struct msmsdcc_host *host, ktime_t start,
				   const char *func, int err) {}
#endif

static int
msmsdcc_runtime_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;
	unsigned long flags;
	ktime_t start = ktime_get();

	if (host->plat->is_sdio_al_client) {
		rc = 0;
		goto out;
	}

	pr_debug("%s: %s: start\n", mmc_hostname(mmc), __func__);
	if (mmc) {
		host->sdcc_suspending = 1;
		mmc->suspend_task = current;

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
		/* If there is pending detect work abort runtime suspend */
		if (unlikely(work_busy(&mmc->detect.work)))
			rc = -EAGAIN;
		else
			rc = mmc_suspend_host(mmc);
		pm_runtime_put_noidle(dev);

		if (!rc) {
			spin_lock_irqsave(&host->lock, flags);
			host->sdcc_suspended = true;
			spin_unlock_irqrestore(&host->lock, flags);
			if (mmc->card && mmc_card_sdio(mmc->card) &&
				mmc->ios.clock) {
				/*
				 * If SDIO function driver doesn't want
				 * to power off the card, atleast turn off
				 * clocks to allow deep sleep (TCXO shutdown).
				 */
				msmsdcc_gate_clock(host);
			}
		}
		host->sdcc_suspending = 0;
		mmc->suspend_task = NULL;
		if (rc && wake_lock_active(&host->sdio_suspend_wlock))
			wake_unlock(&host->sdio_suspend_wlock);
	}
	pr_debug("%s: %s: ends with err=%d\n", mmc_hostname(mmc), __func__, rc);
out:
	/*
	 * Remove the vote immediately only if clocks are off in which
	 * case we might have queued work to remove vote but it may not
	 * be completed before runtime suspend or system suspend.
	 */
	if (!atomic_read(&host->clks_on))
		msmsdcc_msm_bus_cancel_work_and_set_vote(host, NULL);
	msmsdcc_print_pm_stats(host, start, __func__, rc);
	return rc;
}

static int
msmsdcc_runtime_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;
	ktime_t start = ktime_get();

	if (host->plat->is_sdio_al_client)
		goto out;

	pr_debug("%s: %s: start\n", mmc_hostname(mmc), __func__);
	if (mmc) {
		if (mmc->card && mmc_card_sdio(mmc->card) &&
				mmc_card_keep_power(mmc)) {
			msmsdcc_ungate_clock(host);
		}

		mmc_resume_host(mmc);

		/*
		 * FIXME: Clearing of flags must be handled in clients
		 * resume handler.
		 */
		spin_lock_irqsave(&host->lock, flags);
		mmc->pm_flags = 0;
		host->sdcc_suspended = false;
		spin_unlock_irqrestore(&host->lock, flags);

		/*
		 * After resuming the host wait for sometime so that
		 * the SDIO work will be processed.
		 */
		if (mmc->card && mmc_card_sdio(mmc->card)) {
			if ((host->plat->mpm_sdiowakeup_int ||
					host->plat->sdiowakeup_irq) &&
					wake_lock_active(&host->sdio_wlock))
				wake_lock_timeout(&host->sdio_wlock, 1);
		}

		wake_unlock(&host->sdio_suspend_wlock);
	}
	host->pending_resume = false;
	pr_debug("%s: %s: end\n", mmc_hostname(mmc), __func__);
out:
	msmsdcc_print_pm_stats(host, start, __func__, 0);
	return 0;
}

static int msmsdcc_runtime_idle(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);

	if (host->plat->is_sdio_al_client)
		return 0;

	/* Idle timeout is not configurable for now */
	pm_schedule_suspend(dev, host->idle_tout);

	return -EAGAIN;
}

static int msmsdcc_pm_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;
	ktime_t start = ktime_get();

	if (host->plat->is_sdio_al_client) {
		rc = 0;
		goto out;
	}
	if (host->plat->status_irq) {
		disable_irq(host->plat->status_irq);
		msmsdcc_disable_status_gpio(host);
	}

	/*
	 * If system comes out of suspend, msmsdcc_pm_resume() sets the
	 * host->pending_resume flag if the SDCC wasn't runtime suspended.
	 * Now if the system again goes to suspend without any SDCC activity
	 * then host->pending_resume flag will remain set which may cause
	 * the SDCC resume to happen first and then suspend.
	 * To avoid this unnecessary resume/suspend, make sure that
	 * pending_resume flag is cleared before calling the
	 * msmsdcc_runtime_suspend().
	 */
	if (!pm_runtime_suspended(dev) && !host->pending_resume)
		rc = msmsdcc_runtime_suspend(dev);
 out:
	/* This flag must not be set if system is entering into suspend */
	host->pending_resume = false;
	msmsdcc_print_pm_stats(host, start, __func__, rc);
	return rc;
}

static int msmsdcc_suspend_noirq(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;

	/*
	 * After platform suspend there may be active request
	 * which might have enabled clocks. For example, in SDIO
	 * case, ksdioirq thread might have scheduled after sdcc
	 * suspend but before system freeze. In that case abort
	 * suspend and retry instead of keeping the clocks on
	 * during suspend and not allowing TCXO.
	 */

	if (atomic_read(&host->clks_on) && !host->plat->is_sdio_al_client) {
		pr_warn("%s: clocks are on after suspend, aborting system "
				"suspend\n", mmc_hostname(mmc));
		rc = -EAGAIN;
	}

	return rc;
}

static int msmsdcc_pm_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);
	int rc = 0;
	ktime_t start = ktime_get();

	if (host->plat->is_sdio_al_client) {
		rc = 0;
		goto out;
	}
	if (mmc->card && mmc_card_sdio(mmc->card))
		rc = msmsdcc_runtime_resume(dev);
	/*
	 * As runtime PM is enabled before calling the device's platform resume
	 * callback, we use the pm_runtime_suspended API to know if SDCC is
	 * really runtime suspended or not and set the pending_resume flag only
	 * if its not runtime suspended.
	 */
	else if (!pm_runtime_suspended(dev))
		host->pending_resume = true;

	if (host->plat->status_irq) {
		msmsdcc_enable_status_gpio(host);
		msmsdcc_check_status((unsigned long)host);
		enable_irq(host->plat->status_irq);
	}
out:
	msmsdcc_print_pm_stats(host, start, __func__, rc);
	return rc;
}

#else
static int msmsdcc_runtime_suspend(struct device *dev)
{
	return 0;
}
static int msmsdcc_runtime_idle(struct device *dev)
{
	return 0;
}
static int msmsdcc_pm_suspend(struct device *dev)
{
	return 0;
}
static int msmsdcc_pm_resume(struct device *dev)
{
	return 0;
}
static int msmsdcc_suspend_noirq(struct device *dev)
{
	return 0;
}
static int msmsdcc_runtime_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops msmsdcc_dev_pm_ops = {
	.runtime_suspend = msmsdcc_runtime_suspend,
	.runtime_resume  = msmsdcc_runtime_resume,
	.runtime_idle    = msmsdcc_runtime_idle,
	.suspend 	 = msmsdcc_pm_suspend,
	.resume		 = msmsdcc_pm_resume,
	.suspend_noirq	 = msmsdcc_suspend_noirq,
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
	debugfs_remove(debugfs_dir);
#endif
}

module_init(msmsdcc_init);
module_exit(msmsdcc_exit);

MODULE_DESCRIPTION("Qualcomm Multimedia Card Interface driver");
MODULE_LICENSE("GPL");

#if defined(CONFIG_DEBUG_FS)
static int msmsdcc_dbg_idle_tout_get(void *data, u64 *val)
{
	struct msmsdcc_host *host = data;

	*val = host->idle_tout / 1000L;
	return 0;
}

static int msmsdcc_dbg_idle_tout_set(void *data, u64 val)
{
	struct msmsdcc_host *host = data;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->idle_tout = (u32)val * 1000;
	spin_unlock_irqrestore(&host->lock, flags);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(msmsdcc_dbg_idle_tout_ops,
			msmsdcc_dbg_idle_tout_get,
			msmsdcc_dbg_idle_tout_set,
			"%llu\n");

static int msmsdcc_dbg_pio_mode_get(void *data, u64 *val)
{
	struct msmsdcc_host *host = data;

	*val = (u64) host->enforce_pio_mode;
	return 0;
}

static int msmsdcc_dbg_pio_mode_set(void *data, u64 val)
{
	struct msmsdcc_host *host = data;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->enforce_pio_mode = !!val;
	spin_unlock_irqrestore(&host->lock, flags);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(msmsdcc_dbg_pio_mode_ops,
			msmsdcc_dbg_pio_mode_get,
			msmsdcc_dbg_pio_mode_set,
			"%llu\n");

static int msmsdcc_dbg_pm_stats_get(void *data, u64 *val)
{
	struct msmsdcc_host *host = data;

	*val = !!host->print_pm_stats;
	return 0;
}

static int msmsdcc_dbg_pm_stats_set(void *data, u64 val)
{
	struct msmsdcc_host *host = data;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	host->print_pm_stats = !!val;
	spin_unlock_irqrestore(&host->lock, flags);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(msmsdcc_dbg_pm_stats_ops,
			msmsdcc_dbg_pm_stats_get,
			msmsdcc_dbg_pm_stats_set,
			"%llu\n");

static void msmsdcc_dbg_createhost(struct msmsdcc_host *host)
{
	int err = 0;

	if (!debugfs_dir)
		return;

	host->debugfs_host_dir = debugfs_create_dir(
			mmc_hostname(host->mmc), debugfs_dir);
	if (IS_ERR(host->debugfs_host_dir)) {
		err = PTR_ERR(host->debugfs_host_dir);
		host->debugfs_host_dir = NULL;
		pr_err("%s: Failed to create debugfs dir for host with err=%d\n",
			mmc_hostname(host->mmc), err);
		return;
	}

	host->debugfs_idle_tout = debugfs_create_file("idle_tout",
		S_IRUSR | S_IWUSR, host->debugfs_host_dir, host,
		&msmsdcc_dbg_idle_tout_ops);

	if (IS_ERR(host->debugfs_idle_tout)) {
		err = PTR_ERR(host->debugfs_idle_tout);
		host->debugfs_idle_tout = NULL;
		pr_err("%s: Failed to create idle_tout debugfs entry with err=%d\n",
			mmc_hostname(host->mmc), err);
	}

	host->debugfs_pio_mode = debugfs_create_file("pio_mode",
		S_IRUSR | S_IWUSR, host->debugfs_host_dir, host,
		&msmsdcc_dbg_pio_mode_ops);

	if (IS_ERR(host->debugfs_pio_mode)) {
		err = PTR_ERR(host->debugfs_pio_mode);
		host->debugfs_pio_mode = NULL;
		pr_err("%s: Failed to create pio_mode debugfs entry with err=%d\n",
			mmc_hostname(host->mmc), err);
	}

	host->debugfs_pm_stats = debugfs_create_file("pm_stats",
		S_IRUSR | S_IWUSR, host->debugfs_host_dir, host,
		&msmsdcc_dbg_pm_stats_ops);
	if (IS_ERR(host->debugfs_pm_stats)) {
		err = PTR_ERR(host->debugfs_pm_stats);
		host->debugfs_pm_stats = NULL;
		pr_err("%s: Failed to create pm_stats debugfs entry with err=%d\n",
			mmc_hostname(host->mmc), err);
	}
}

static int __init msmsdcc_dbg_init(void)
{
	int err;

	debugfs_dir = debugfs_create_dir("msm_sdcc", 0);
	if (IS_ERR(debugfs_dir)) {
		err = PTR_ERR(debugfs_dir);
		debugfs_dir = NULL;
		return err;
	}

	return 0;
}
#endif
