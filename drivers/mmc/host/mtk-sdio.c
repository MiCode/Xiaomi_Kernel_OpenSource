/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Chaotian.Jing <chaotian.jing@mediatek.com>
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
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/slot-gpio.h>

#include "mtk-sdio.h"

static void sdr_set_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val |= bs;
	writel(val, reg);
}

static void sdr_clr_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val &= ~bs;
	writel(val, reg);
}

static void sdr_set_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = readl(reg);

	tv &= ~field;
	tv |= ((val) << (ffs((unsigned int)field) - 1));
	writel(tv, reg);
}

static void sdr_get_field(void __iomem *reg, u32 field, u32 *val)
{
	unsigned int tv = readl(reg);

	*val = ((tv & field) >> (ffs((unsigned int)field) - 1));
}

static void msdc_reset_hw(struct msdc_host *host)
{
	u32 val;

	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_RST);
	while (readl(host->base + MSDC_CFG) & MSDC_CFG_RST)
		cpu_relax();

	sdr_set_bits(host->base + MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	while (readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_CLR)
		cpu_relax();

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
}

static bool sdio_online_tune_fail;
static void msdc_dump_all_register(struct msdc_host *host);
static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd);
#ifndef SUPPORT_LEGACY_SDIO
static void msdc_recheck_sdio_irq(struct msdc_host *host);
#endif
static const u32 cmd_ints_mask = MSDC_INTEN_CMDRDY | MSDC_INTEN_RSPCRCERR |
			MSDC_INTEN_CMDTMO | MSDC_INTEN_ACMDRDY |
			MSDC_INTEN_ACMDCRCERR | MSDC_INTEN_ACMDTMO;
static const u32 data_ints_mask = MSDC_INTEN_XFER_COMPL | MSDC_INTEN_DATTMO |
			MSDC_INTEN_DATCRCERR | MSDC_INTEN_DMA_BDCSERR |
			MSDC_INTEN_DMA_GPDCSERR | MSDC_INTEN_DMA_PROTECT;

static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xff - (u8) sum;
}

static inline void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
		struct mmc_data *data)
{
	unsigned int j, dma_len;
	dma_addr_t dma_address;
	u32 dma_ctrl;
	struct scatterlist *sg;
	struct mt_gpdma_desc *gpd;
	struct mt_bdma_desc *bd;

	sg = data->sg;

	gpd = dma->gpd;
	bd = dma->bd;

	/* modify gpd */
	gpd->gpd_info |= GPDMA_DESC_HWO;
	gpd->gpd_info |= GPDMA_DESC_BDP;
	/* need to clear first. use these bits to calc checksum */
	gpd->gpd_info &= ~GPDMA_DESC_CHECKSUM;
	gpd->gpd_info |= msdc_dma_calcs((u8 *) gpd, 16) << 8;

	/* modify bd */
	for_each_sg(data->sg, sg, data->sg_count, j) {
		dma_address = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);

		/* init bd */
		bd[j].bd_info &= ~BDMA_DESC_BLKPAD;
		bd[j].bd_info &= ~BDMA_DESC_DWPAD;
		bd[j].ptr = (u32)dma_address;
		bd[j].bd_data_len &= ~BDMA_DESC_BUFLEN;
		bd[j].bd_data_len |= (dma_len & BDMA_DESC_BUFLEN);

		if (j == data->sg_count - 1) /* the last bd */
			bd[j].bd_info |= BDMA_DESC_EOL;
		else
			bd[j].bd_info &= ~BDMA_DESC_EOL;

		/* checksume need to clear first */
		bd[j].bd_info &= ~BDMA_DESC_CHECKSUM;
		bd[j].bd_info |= msdc_dma_calcs((u8 *)(&bd[j]), 16) << 8;
	}

	sdr_set_field(host->base + MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, 1);
	dma_ctrl = readl_relaxed(host->base + MSDC_DMA_CTRL);
	dma_ctrl &= ~(MSDC_DMA_CTRL_BRUSTSZ | MSDC_DMA_CTRL_MODE);
	dma_ctrl |= (MSDC_BURST_64B << 12 | 1 << 8);
	writel_relaxed(dma_ctrl, host->base + MSDC_DMA_CTRL);
	writel((u32)dma->gpd_addr, host->base + MSDC_DMA_SA);
}

static void msdc_prepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (!(data->host_cookie & MSDC_PREPARE_FLAG)) {
		bool read = (data->flags & MMC_DATA_READ) != 0;

		data->host_cookie |= MSDC_PREPARE_FLAG;
		data->sg_count = dma_map_sg(host->dev, data->sg, data->sg_len,
					   read ? DMA_FROM_DEVICE :
					   DMA_TO_DEVICE);
	}
}

static void msdc_unprepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (data->host_cookie & MSDC_ASYNC_FLAG)
		return;

	if (data->host_cookie & MSDC_PREPARE_FLAG) {
		bool read = (data->flags & MMC_DATA_READ) != 0;

		dma_unmap_sg(host->dev, data->sg, data->sg_len,
			     read ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		data->host_cookie &= ~MSDC_PREPARE_FLAG;
	}
}

/* clock control primitives */
static void msdc_set_timeout(struct msdc_host *host, u32 ns, u32 clks)
{
	u32 timeout, clk_ns;
	u32 mode = 0;

	host->timeout_ns = ns;
	host->timeout_clks = clks;
	if (host->sclk == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000UL / host->sclk;
		timeout = (ns + clk_ns - 1) / clk_ns + clks;
		/* in 1048576 sclk cycle unit */
		timeout = (timeout + (0x1 << 20) - 1) >> 20;
		sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKMOD, &mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, timeout);
}

static void msdc_gate_clock(struct msdc_host *host)
{
	clk_disable_unprepare(host->src_clk);
	clk_disable_unprepare(host->h_clk);
	clk_disable_unprepare(host->src_clk_cg);

	host->sdio_clk_cnt--;
	if (!host->sdio_clk_cnt)
		host->clock_on = false;
}

static void msdc_ungate_clock(struct msdc_host *host)
{
	clk_prepare_enable(host->src_clk_cg);
	clk_prepare_enable(host->h_clk);
	clk_prepare_enable(host->src_clk);
	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();

	host->clock_on = true;
	host->sdio_clk_cnt++;
}

static void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz)
{
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
	unsigned long irq_flags;

	if (!hz) {
		dev_info(host->dev, "set mclk to 0\n");
		host->mclk = 0;
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
		return;
	}

	if (hz >= 100 * 1000 * 1000 && sdio_online_tune_fail)
		hz = 50 * 1000 * 1000;

	spin_lock_irqsave(&host->irqlock, irq_flags);
	flags = readl(host->base + MSDC_INTEN);
	sdr_clr_bits(host->base + MSDC_INTEN, flags);
	spin_unlock_irqrestore(&host->irqlock, irq_flags);

	sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_HS400_CK_MODE);
	if (timing == MMC_TIMING_UHS_DDR50 ||
	    timing == MMC_TIMING_MMC_DDR52 ||
	    timing == MMC_TIMING_MMC_HS400) {
		if (timing == MMC_TIMING_MMC_HS400)
			mode = 0x3;
		else
			mode = 0x2; /* ddr mode and use divisor */

		if (hz >= (host->src_clk_freq >> 2)) {
			div = 0; /* mean div = 1/4 */
			sclk = host->src_clk_freq >> 2; /* sclk = clk / 4 */
		} else {
			div = (host->src_clk_freq + ((hz << 2) - 1)) /
			      (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
			div = (div >> 1);
		}

		if (timing == MMC_TIMING_MMC_HS400 &&
		    hz >= (host->src_clk_freq >> 1)) {
			sdr_set_bits(host->base + MSDC_CFG,
				     MSDC_CFG_HS400_CK_MODE);
			sclk = host->src_clk_freq >> 1;
			div = 0; /* div is ignore when bit18 is set */
		}
	} else if (hz >= host->src_clk_freq) {
		mode = 0x1; /* no divisor */
		div = 0;
		sclk = host->src_clk_freq;
	} else {
		mode = 0x0; /* use divisor */
		if (hz >= (host->src_clk_freq >> 1)) {
			div = 0; /* mean div = 1/2 */
			sclk = host->src_clk_freq >> 1; /* sclk = clk / 2 */
		} else {
			div = (host->src_clk_freq + ((hz << 2) - 1)) /
			      (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
		}
	}
	/*
	 * As src_clk/HCLK use the same bit to gate/ungate,
	 * So if want to only gate src_clk, need gate its parent(mux).
	 */
	sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
	if (host->src_clk_cg)
		clk_disable_unprepare(host->src_clk_cg);
	else
		clk_disable_unprepare(clk_get_parent(host->src_clk_cg));
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(mode << 12) | div);
	if (host->src_clk_cg)
		clk_prepare_enable(host->src_clk_cg);
	else
		clk_prepare_enable(clk_get_parent(host->src_clk_cg));
	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();
	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
	host->sclk = sclk;
	host->mclk = hz;
	host->timing = timing;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);

	spin_lock_irqsave(&host->irqlock, irq_flags);
	sdr_set_bits(host->base + MSDC_INTEN, flags);
	spin_unlock_irqrestore(&host->irqlock, irq_flags);

	if (host->sclk <= 52000000) {
		sdr_set_field(host->base + MSDC_PATCH_BIT1,
			      MSDC_PB1_WRDAT_CRCS_TA_CNTR, 0x1);
		sdr_set_field(host->base + MSDC_PATCH_BIT1,
			      MSDC_PB1_CMD_RSP_TA_CNTR, 0x1);
	} else {
		sdr_set_field(host->base + MSDC_PATCH_BIT1,
			      MSDC_PB1_WRDAT_CRCS_TA_CNTR, 0x2);
		sdr_set_field(host->base + MSDC_PATCH_BIT1,
			      MSDC_PB1_CMD_RSP_TA_CNTR, 0x4);
	}

	dev_info(host->dev, "sclk: %d, timing: %d hz:%d cfg:0x%x\n", host->sclk,
			   timing, hz, readl(host->base + MSDC_CFG));
}

static inline u32 msdc_cmd_find_resp(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 resp;

	switch (mmc_resp_type(cmd)) {
		/* Actually, R1, R5, R6, R7 are the same */
	case MMC_RSP_R1:
		resp = 0x1;
		break;
	case MMC_RSP_R1B:
		resp = 0x7;
		break;
	case MMC_RSP_R2:
		resp = 0x2;
		break;
	case MMC_RSP_R3:
		resp = 0x3;
		break;
	case MMC_RSP_NONE:
	default:
		resp = 0x0;
		break;
	}

	return resp;
}

static inline u32 msdc_cmd_prepare_raw_cmd(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	/* rawcmd :
	 * vol_swt << 30 | auto_cmd << 28 | blklen << 16 | go_irq << 15 |
	 * stop << 14 | rw << 13 | dtype << 11 | rsptyp << 7 | brk << 6 | opcode
	 */
	u32 opcode = cmd->opcode;
	u32 resp = msdc_cmd_find_resp(host, mrq, cmd);
	u32 rawcmd = (opcode & 0x3f) | ((resp & 0x7) << 7);

	host->cmd_rsp = resp;

	if ((opcode == SD_IO_RW_DIRECT &&
	    ((cmd->arg >> 9) & 0x1ffff) == SDIO_CCCR_ABORT) ||
	    opcode == MMC_STOP_TRANSMISSION)
		rawcmd |= (0x1 << 14);
	else if (opcode == SD_SWITCH_VOLTAGE)
		rawcmd |= (0x1 << 30);
	else if (opcode == SD_APP_SEND_SCR ||
		 opcode == SD_APP_SEND_NUM_WR_BLKS ||
		 (opcode == SD_SWITCH &&
		 mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == SD_APP_SD_STATUS &&
		 mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == MMC_SEND_EXT_CSD &&
		 mmc_cmd_type(cmd) == MMC_CMD_ADTC))
		rawcmd |= (0x1 << 11);

	if (cmd->data) {
		struct mmc_data *data = cmd->data;

		if (mmc_op_multi(opcode)) {
			if (mmc_card_mmc(host->mmc->card) && mrq->sbc &&
			    !(mrq->sbc->arg & 0xFFFF0000))
				rawcmd |= 0x2 << 28; /* AutoCMD23 */
		}

		rawcmd |= ((data->blksz & 0xFFF) << 16);
		if (data->flags & MMC_DATA_WRITE)
			rawcmd |= (0x1 << 13);
		if (data->blocks > 1)
			rawcmd |= (0x2 << 11);
		else
			rawcmd |= (0x1 << 11);
		/* Always use dma mode */
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_PIO);

		if (host->timeout_ns != data->timeout_ns ||
		    host->timeout_clks != data->timeout_clks)
			msdc_set_timeout(host, data->timeout_ns,
					data->timeout_clks);

		writel(data->blocks, host->base + SDC_BLK_NUM);
	}
	return rawcmd;
}

static void msdc_start_data(struct msdc_host *host, struct mmc_request *mrq,
			    struct mmc_command *cmd, struct mmc_data *data)
{
	unsigned long flags;
	bool read;

	WARN_ON(host->data);
	host->data = data;
	read = data->flags & MMC_DATA_READ;

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	msdc_dma_setup(host, &host->dma, data);

	spin_lock_irqsave(&host->irqlock, flags);
	sdr_set_bits(host->base + MSDC_INTEN, data_ints_mask);
	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);
	spin_unlock_irqrestore(&host->irqlock, flags);

	dev_dbg(host->dev, "DMA start\n");
	dev_dbg(host->dev, "%s: cmd=%d DMA data: %d blocks; read=%d\n",
			__func__, cmd->opcode, data->blocks, read);
}

static int msdc_auto_cmd_done(struct msdc_host *host, int events,
		struct mmc_command *cmd)
{
	u32 *rsp = cmd->resp;

	rsp[0] = readl(host->base + SDC_ACMD_RESP);

	if (events & MSDC_INT_ACMDRDY) {
		cmd->error = 0;
	} else {
		msdc_reset_hw(host);
		if (events & MSDC_INT_ACMDCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_STOP_EIO;
		} else if (events & MSDC_INT_ACMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_STOP_TMO;
		}
		dev_info(host->dev,
			"%s: AUTO_CMD%d arg=%08X; rsp %08X; cmd_error=%d\n",
			__func__, cmd->opcode, cmd->arg, rsp[0], cmd->error);
	}
	return cmd->error;
}

static void msdc_track_cmd_data(struct msdc_host *host,
				struct mmc_command *cmd, struct mmc_data *data)
{
	if (host->error)
		dev_info(host->dev, "cmd=%d arg=%08X; err=0x%08X\n",
			 cmd->opcode, cmd->arg, host->error);
}

static void msdc_request_done(struct msdc_host *host, struct mmc_request *mrq)
{
	unsigned long flags;
	bool ret;

	ret = cancel_delayed_work(&host->req_timeout);
	if (!ret && in_interrupt()) {
		/* delay work already running */
		return;
	}
	spin_lock_irqsave(&host->lock, flags);
	host->mrq = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	msdc_track_cmd_data(host, mrq->cmd, mrq->data);
	if (mrq->data)
		msdc_unprepare_data(host, mrq);
	mmc_request_done(host->mmc, mrq);
#ifndef SUPPORT_LEGACY_SDIO
	msdc_recheck_sdio_irq(host);
#endif
}

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;
	unsigned long flags;
	u32 *rsp = cmd->resp;

	if (mrq->sbc && cmd == mrq->cmd &&
	    (events & (MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
				   | MSDC_INT_ACMDTMO)))
		msdc_auto_cmd_done(host, events, mrq->sbc);

	sbc_error = mrq->sbc && mrq->sbc->error;

	if (!sbc_error && !(events & (MSDC_INT_CMDRDY
					| MSDC_INT_RSPCRCERR
					| MSDC_INT_CMDTMO)))
		return done;

	done = !host->cmd;
	spin_lock_irqsave(&host->lock, flags);
	host->cmd = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;

	spin_lock_irqsave(&host->irqlock, flags);
	sdr_clr_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	spin_unlock_irqrestore(&host->irqlock, flags);

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			rsp[0] = readl(host->base + SDC_RESP3);
			rsp[1] = readl(host->base + SDC_RESP2);
			rsp[2] = readl(host->base + SDC_RESP1);
			rsp[3] = readl(host->base + SDC_RESP0);
		} else {
			rsp[0] = readl(host->base + SDC_RESP0);
		}
	}

	if (!sbc_error && !(events & MSDC_INT_CMDRDY)) {
		if (cmd->opcode != MMC_SEND_TUNING_BLOCK &&
		    cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200)
			/*
			 * should not clear fifo/interrupt as the tune data
			 * may have alreay come.
			 */
			msdc_reset_hw(host);
		if (events & MSDC_INT_RSPCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_CMD_EIO;
		} else if (events & MSDC_INT_CMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_CMD_TMO;
		}
	}
	if (cmd->error && cmd->opcode != MMC_SEND_TUNING_BLOCK)
		dev_dbg(host->dev,
			"%s: cmd=%d arg=%08X; rsp %08X; cmd_error=%d\n",
			__func__, cmd->opcode, cmd->arg, rsp[0],
			cmd->error);

	msdc_cmd_next(host, mrq, cmd);
	return true;
}

static int msdc_card_busy(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 status = readl(host->base + MSDC_PS);

	/* check if data0 is low */
	return !(status & BIT(16));
}

/* It is the core layer's responsibility to ensure card status
 * is correct before issue a request. but host design do below
 * checks recommended.
 */
static inline bool msdc_cmd_is_ready(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	/* The max busy time we can endure is 20ms */
	unsigned long tmo = jiffies + msecs_to_jiffies(20);
	u32 count = 0;

	if (in_interrupt()) {
		while ((readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) &&
		       (count < 1000)) {
			udelay(1);
			count++;
		}
	} else {
		while ((readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) &&
		       time_before(jiffies, tmo))
			cpu_relax();
	}

	if (readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) {
		dev_info(host->dev, "CMD bus busy detected\n");
		host->error |= REQ_CMD_BUSY;
		msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		return false;
	}

	if (cmd->opcode != MMC_SEND_STATUS) {
		count = 0;
		/* Consider that CMD6 crc error before card was init done,
		 * mmc_retune() will return directly as host->card is null.
		 * and CMD6 will retry 3 times, must ensure card is in transfer
		 * state when retry.
		 */
		tmo = jiffies + msecs_to_jiffies(60 * 1000);
		while (1) {
			if (msdc_card_busy(host->mmc)) {
				if (in_interrupt()) {
					udelay(1);
					count++;
				} else {
					msleep_interruptible(10);
				}
			} else {
				break;
			}
			/* Timeout if the device never
			 * leaves the program state.
			 */
			if (count > 1000 || time_after(jiffies, tmo)) {
				pr_info("%s: Card is in programming state!\n",
				       mmc_hostname(host->mmc));
				host->error |= REQ_CMD_BUSY;
				msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
				return false;
			}
		}
	}
	return true;
}

static void msdc_start_command(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	unsigned long flags;
	u32 rawcmd;

	WARN_ON(host->cmd);
	host->cmd = cmd;

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	if (!msdc_cmd_is_ready(host, mrq, cmd))
		return;

	if ((readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16 ||
	    readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) {
		dev_info(host->dev,
			"TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw(host);
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);

	spin_lock_irqsave(&host->irqlock, flags);
	sdr_set_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	spin_unlock_irqrestore(&host->irqlock, flags);

	writel(cmd->arg, host->base + SDC_ARG);
	writel(rawcmd, host->base + SDC_CMD);

}

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	if ((cmd->error &&
	    !(cmd->error == -EILSEQ &&
	      (cmd->opcode == MMC_SEND_TUNING_BLOCK ||
	       cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200))) ||
	    (mrq->sbc && mrq->sbc->error))
		msdc_request_done(host, mrq);
	else if (cmd == mrq->sbc)
		msdc_start_command(host, mrq, mrq->cmd);
	else if (!cmd->data)
		msdc_request_done(host, mrq);
	else
		msdc_start_data(host, mrq, cmd, cmd->data);
}

static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->error = 0;
	WARN_ON(host->mrq);
	host->mrq = mrq;

	if (mrq->data)
		msdc_prepare_data(host, mrq);

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option
	 */
	if (mrq->sbc && (!mmc_card_mmc(mmc->card) ||
	    (mrq->sbc->arg & 0xFFFF0000)))
		msdc_start_command(host, mrq, mrq->sbc);
	else
		msdc_start_command(host, mrq, mrq->cmd);
}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (!data)
		return;

	msdc_prepare_data(host, mrq);
	data->host_cookie |= MSDC_ASYNC_FLAG;
}

static void msdc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
		 int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;

	data = mrq->data;
	if (!data)
		return;
	if (data->host_cookie) {
		data->host_cookie &= ~MSDC_ASYNC_FLAG;
		msdc_unprepare_data(host, mrq);
	}
}

static void msdc_data_xfer_next(struct msdc_host *host,
				struct mmc_request *mrq, struct mmc_data *data)
{
	if (mmc_op_multi(mrq->cmd->opcode) && mrq->stop && !mrq->stop->error &&
	    !mrq->sbc)
		msdc_start_command(host, mrq, mrq->stop);
	else
		msdc_request_done(host, mrq);
}

static bool msdc_data_xfer_done(struct msdc_host *host, u32 events,
				struct mmc_request *mrq, struct mmc_data *data)
{
	struct mmc_command *stop = data->stop;
	unsigned long flags;
	bool done;
	unsigned int check_data = events &
	    (MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO
	     | MSDC_INT_DMA_BDCSERR | MSDC_INT_DMA_GPDCSERR
	     | MSDC_INT_DMA_PROTECT);

	done = !host->data;
	spin_lock_irqsave(&host->lock, flags);
	if (check_data)
		host->data = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;

	if (check_data || (stop && stop->error)) {
		dev_dbg(host->dev, "DMA status: 0x%8X\n",
				readl(host->base + MSDC_DMA_CFG));
		sdr_set_field(host->base + MSDC_DMA_CTRL,
			      MSDC_DMA_CTRL_STOP, 1);
		while (readl(host->base + MSDC_DMA_CFG) & MSDC_DMA_CFG_STS)
			cpu_relax();

		spin_lock_irqsave(&host->irqlock, flags);
		sdr_clr_bits(host->base + MSDC_INTEN, data_ints_mask);
		spin_unlock_irqrestore(&host->irqlock, flags);

		dev_dbg(host->dev, "DMA stop\n");

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error)) {
			data->bytes_xfered = data->blocks * data->blksz;
		} else {
			dev_info(host->dev, "interrupt events: %x\n", events);
			msdc_reset_hw(host);
			host->error |= REQ_DAT_ERR;
			data->bytes_xfered = 0;

			if (events & MSDC_INT_DATTMO)
				data->error = -ETIMEDOUT;
			else if (events & MSDC_INT_DATCRCERR)
				data->error = -EILSEQ;

			if (mrq->cmd->opcode != MMC_SEND_TUNING_BLOCK) {
				dev_info(host->dev, "%s: cmd=%d; blocks=%d",
				__func__, mrq->cmd->opcode, data->blocks);
				dev_info(host->dev, "data_error=%d xfer_size=%d\n",
					(int)data->error, data->bytes_xfered);
			}
		}

		msdc_data_xfer_next(host, mrq, data);
		done = true;
	}
	return done;
}

static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	u32 val = readl(host->base + SDC_CFG);

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	writel(val, host->base + SDC_CFG);
	dev_dbg(host->dev, "Bus Width = %d", width);
}

static int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{

	struct msdc_host *host = mmc_priv(mmc);
	int min_uv, max_uv;
	int ret = 0;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
			min_uv = 3300000;
			max_uv = 3300000;
		} else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
			min_uv = 1800000;
			max_uv = 1800000;
		} else {
			dev_info(host->dev, "Unsupported signal voltage!\n");
			return -EINVAL;
		}

		ret = regulator_set_voltage(mmc->supply.vqmmc, min_uv, max_uv);
		if (ret) {
			dev_dbg(host->dev, "Regulator set error %d (%d)\n",
				ret, ios->signal_voltage);
		} else {
			/* Apply different pinctrl settings
			 * for different signal voltage
			 */
			if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
				pinctrl_select_state(host->pinctrl,
						     host->pins_uhs);
			else
				pinctrl_select_state(host->pinctrl,
						     host->pins_default);
		}
	}
	return ret;
}

static void msdc_request_timeout(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
			req_timeout.work);

	/* simulate HW timeout status */
	dev_info(host->dev, "%s: aborting cmd/data/mrq\n", __func__);
	if (host->mrq) {
		dev_info(host->dev, "%s: aborting mrq=%p cmd=%d\n", __func__,
				host->mrq, host->mrq->cmd->opcode);
		if (host->cmd) {
			dev_info(host->dev,
				"%s: aborting cmd=%d, arg=0x%x\n", __func__,
				host->cmd->opcode, host->cmd->arg);
			msdc_cmd_done(host, MSDC_INT_CMDTMO, host->mrq,
				      host->cmd);
		} else if (host->data) {
			dev_info(host->dev,
				"%s: aborting data: cmd%d; %d blocks\n",
				    __func__, host->mrq->cmd->opcode,
				    host->data->blocks);
			msdc_data_xfer_done(host, MSDC_INT_DATTMO, host->mrq,
					host->data);
		}
	}
}

static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	unsigned long flags;
	struct msdc_host *host = (struct msdc_host *) dev_id;
	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;
	u32 events, event_mask;

	spin_lock_irqsave(&host->irqlock, flags);
	events = readl(host->base + MSDC_INT);
	event_mask = readl(host->base + MSDC_INTEN);
	/* clear interrupts */
	writel(events & event_mask, host->base + MSDC_INT);

	mrq = host->mrq;
	cmd = host->cmd;
	data = host->data;
	spin_unlock_irqrestore(&host->irqlock, flags);

	if ((events & event_mask) & MSDC_INT_SDIOIRQ) {
		mmc_signal_sdio_irq(host->mmc);
		if (!mrq)
			return IRQ_HANDLED;
	}

	if (!(events & (event_mask & ~MSDC_INT_SDIOIRQ)))
		return IRQ_HANDLED;

	if (!mrq) {
		dev_info(host->dev,
			"%s: MRQ=NULL; events=%08X; event_mask=%08X\n",
			__func__, events, event_mask);
		WARN_ON(1);
		return IRQ_HANDLED;
	}

	if (cmd)
		msdc_cmd_done(host, events, mrq, cmd);
	else if (data)
		msdc_data_xfer_done(host, events, mrq, data);

	return IRQ_HANDLED;
}

static struct msdc_host *sdio_host;

static void sdio_status_notify_cb(int card_present, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *)dev_id;

	pr_info("%s: card_present %d\n", mmc_hostname(host->mmc), card_present);

	if (card_present == 1) {
		host->mmc->rescan_disable = 0;
		mmc_detect_change(host->mmc, 0);
	} else if (card_present == 0) {
		host->mmc->detect_change = 0;
		host->mmc->rescan_disable = 1;
	}
}

void sdio_card_detect(int card_present)
{
	pr_info("%s: enter present:%d\n", __func__, card_present);
	if (sdio_host)
		sdio_status_notify_cb(card_present, sdio_host);

}
EXPORT_SYMBOL(sdio_card_detect);

static void msdc_init_hw(struct msdc_host *host)
{
	u32 val;
	unsigned long flags;

	/* Configure to MMC/SD mode, clock free running */
	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_MODE);

	/* Reset */
	msdc_reset_hw(host);

	/* Disable card detection */
	sdr_clr_bits(host->base + MSDC_PS, MSDC_PS_CDEN);

	/* Disable and clear all interrupts */
	spin_lock_irqsave(&host->irqlock, flags);
	writel(0, host->base + MSDC_INTEN);
	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
	spin_unlock_irqrestore(&host->irqlock, flags);

	writel(0, host->base + MSDC_IOCON);
	sdr_set_field(host->base + MSDC_IOCON, MSDC_IOCON_DDLSEL, 0);
	writel(0x403c0046, host->base + MSDC_PATCH_BIT0);
	sdr_set_field(host->base + MSDC_PATCH_BIT0, MSDC_CKGEN_MSDC_DLY_SEL, 1);
	writel(0xffff0089, host->base + MSDC_PATCH_BIT1);

	/* For SDIO3.0+ IP, this bit should be set to 0 */
	if (host->dev_comp->v3_plus)
		sdr_clr_bits(host->base + MSDC_PATCH_BIT1,
			MSDC_PB1_SINGLE_BURST);

	sdr_set_bits(host->base + EMMC50_CFG0, EMMC50_CFG_CFCSTS_SEL);

	/* Configure to enable SDIO mode.
	 * it's must otherwise sdio cmd5 failed
	 */
	sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIO);

	if (host->mmc->caps & MMC_CAP_SDIO_IRQ)
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	else
	/* disable detect SDIO device interrupt function */
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	/* Configure to default data timeout */
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 3);

	host->def_tune_para.iocon = readl(host->base + MSDC_IOCON);
	host->def_tune_para.pad_tune0 = readl(host->base + MSDC_PAD_TUNE0);
	host->def_tune_para.pad_tune1 = readl(host->base + MSDC_PAD_TUNE1);
	dev_dbg(host->dev, "init hardware done!");
}

static void msdc_deinit_hw(struct msdc_host *host)
{
	u32 val;
	unsigned long flags;

	/* Disable and clear all interrupts */
	spin_lock_irqsave(&host->irqlock, flags);
	writel(0, host->base + MSDC_INTEN);

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
	spin_unlock_irqrestore(&host->irqlock, flags);
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct mt_gpdma_desc *gpd = dma->gpd;
	struct mt_bdma_desc *bd = dma->bd;
	int i;

	memset(gpd, 0, sizeof(struct mt_gpdma_desc) * 2);

	gpd->gpd_info = GPDMA_DESC_BDP; /* hwo, cs, bd pointer */
	gpd->ptr = (u32)dma->bd_addr; /* physical address */
	/* gpd->next is must set for desc DMA
	 * That's why must alloc 2 gpd structure.
	 */
	gpd->next = (u32)dma->gpd_addr + sizeof(struct mt_gpdma_desc);
	memset(bd, 0, sizeof(struct mt_bdma_desc) * MAX_BD_NUM);
	for (i = 0; i < (MAX_BD_NUM - 1); i++)
		bd[i].next = (u32)dma->bd_addr + sizeof(*bd) * (i + 1);
}

static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	int ret;
	struct msdc_host *host = mmc_priv(mmc);

	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
	switch (ios->power_mode) {
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc)) {
			msdc_init_hw(host);
			ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
					ios->vdd);
			if (ret) {
				dev_info(host->dev, "Failed to set vmmc power!\n");
				return;
			}
		}
		break;
	case MMC_POWER_ON:
		if (!IS_ERR(mmc->supply.vqmmc) && !host->vqmmc_enabled) {
			ret = regulator_enable(mmc->supply.vqmmc);
			if (ret)
				dev_info(host->dev, "Failed to set vqmmc power!\n");
			else
				host->vqmmc_enabled = true;
		}
		break;
	case MMC_POWER_OFF:
		/* power always on */
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && host->vqmmc_enabled) {
			regulator_disable(mmc->supply.vqmmc);
			host->vqmmc_enabled = false;
		}
		break;
	default:
		break;
	}

	if (host->mclk != ios->clock || host->timing != ios->timing)
		msdc_set_mclk(host, ios->timing, ios->clock);
}

/***************  SDIO AUTOK  ******************/
#define MSDC_FIFO_THD_1K                (1024)
#define TUNE_TX_CNT                     (100)
#define MSDC_FIFO_SZ			(128)
/*#define TUNE_DATA_TX_ADDR               (0x358000)*/
/* Use negative value to represent address from end of device,
 * 33 blocks used by SGPT at end of device,
 * 32768 blocks used by flashinfo immediate before SGPT
 */
#define TUNE_DATA_TX_ADDR               (-33-32768)
#define CMDQ
#define AUTOK_LATCH_CK_EMMC_TUNE_TIMES  (10) /* 5.0IP eMMC 1KB fifo ZIZE */
#define AUTOK_LATCH_CK_SDIO_TUNE_TIMES  (20)  /* 4.5IP SDIO 128fifo ZIZE */
#define AUTOK_LATCH_CK_SD_TUNE_TIMES    (3)  /* 4.5IP SD 128fifo ZIZE */
#define AUTOK_CMD_TIMES                 (20)
#define AUTOK_TUNING_INACCURACY         (3)  /* scan result may find xxxooxxx */
#define AUTOK_MARGIN_THOLD              (5)
#define AUTOK_BD_WIDTH_REF              (3)

#define AUTOK_READ                      0
#define AUTOK_WRITE                     1

#define AUTOK_FINAL_CKGEN_SEL           (0)
#define SCALE_TA_CNTR                   (8)
#define SCALE_CMD_RSP_TA_CNTR           (8)
#define SCALE_WDAT_CRC_TA_CNTR          (8)
#define SCALE_INT_DAT_LATCH_CK_SEL      (8)
#define SCALE_INTERNAL_DLY_CNTR         (32)
#define SCALE_PAD_DAT_DLY_CNTR          (32)

#define TUNING_INACCURACY (2)

/* autok platform specific setting */
#define AUTOK_CKGEN_VALUE                       (0)
#define AUTOK_CMD_LATCH_EN_HS400_VALUE          (3)
#define AUTOK_CMD_LATCH_EN_NON_HS400_VALUE      (2)
#define AUTOK_CRC_LATCH_EN_HS400_VALUE          (0)
#define AUTOK_CRC_LATCH_EN_NON_HS400_VALUE      (2)
#define AUTOK_LATCH_CK_VALUE                    (1)
#define AUTOK_CMD_TA_VALUE                      (0)
#define AUTOK_CRC_TA_VALUE                      (0)
#define AUTOK_CRC_MA_VALUE                      (1)
#define AUTOK_BUSY_MA_VALUE                     (1)

#define AUTOK_FAIL		-1

#define E_RESULT_PASS     (0)
#define E_RESULT_CMD_TMO  (1<<0)
#define E_RESULT_RSP_CRC  (1<<1)
#define E_RESULT_DAT_CRC  (1<<2)
#define E_RESULT_DAT_TMO  (1<<3)
#define E_RESULT_W_CRC    (1<<4)
#define E_RESULT_ERR      (1<<5)
#define E_RESULT_START    (1<<6)
#define E_RESULT_PW_SMALL (1<<7)
#define E_RESULT_KEEP_OLD (1<<8)
#define E_RESULT_CMP_ERR  (1<<9)
#define E_RESULT_FATAL_ERR  (1<<10)

#define E_RESULT_MAX

#ifndef NULL
#define NULL                0
#endif
#ifndef TRUE
#define TRUE                (0 == 0)
#endif
#ifndef FALSE
#define FALSE               (0 != 0)
#endif

#define ATK_OFF                             0
#define ATK_ERROR                           1
#define ATK_RES                             2
#define ATK_WARN                            3
#define ATK_TRACE                           4
#define ATK_LOUD                            5

static unsigned int autok_debug_level = ATK_RES;

#define ATK_DBG(_level, _fmt ...)	   \
({                                         \
	if (autok_debug_level >= _level) { \
		pr_info(_fmt);              \
	}                                  \
})

#define ATK_ERR(_fmt ...)           \
({                                         \
	pr_info(_fmt);                      \
})

enum AUTOK_PARAM {
	/* command response sample selection
	 * (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)
	 */
	CMD_EDGE,

	/* read data sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING) */
	RDATA_EDGE,

	/* read data async fifo out edge select */
	RD_FIFO_EDGE,

	/* write data crc status async fifo out edge select */
	WD_FIFO_EDGE,

	/* [Data Tune]CMD Pad RX Delay Line1 Control.
	 * This register is used to fine-tune CMD pad macro respose
	 * latch timing. Total 32 stages[Data Tune]
	 */
	CMD_RD_D_DLY1,

	/* [Data Tune]CMD Pad RX Delay Line1 Sel-> delay cell1 enable */
	CMD_RD_D_DLY1_SEL,

	/* [Data Tune]CMD Pad RX Delay Line2 Control. This register is used to
	 * fine-tune CMD pad macro respose latch timing.
	 * Total 32 stages[Data Tune]
	 */
	CMD_RD_D_DLY2,

	/* [Data Tune]CMD Pad RX Delay Line1 Sel-> delay cell2 enable */
	CMD_RD_D_DLY2_SEL,

	/* [Data Tune]DAT Pad RX Delay Line1 Control (for MSDC RD),
	 * Total 32 stages [Data Tune]
	 */
	DAT_RD_D_DLY1,

	/* [Data Tune]DAT Pad RX Delay Line1 Sel-> delay cell1 enable */
	DAT_RD_D_DLY1_SEL,

	/* [Data Tune]DAT Pad RX Delay Line2 Control (for MSDC RD),
	 * Total 32 stages [Data Tune]
	 */
	DAT_RD_D_DLY2,

	/* [Data Tune]DAT Pad RX Delay Line2 Sel-> delay cell2 enable */
	DAT_RD_D_DLY2_SEL,

	/* Internal MSDC clock phase selection. Total 8 stages,
	 * each stage can delay 1 clock period of msdc_src_ck
	 */
	INT_DAT_LATCH_CK,

	/* DS Pad Z clk delay count, range: 0~63, Z dly1(0~31)+Z dly2(0~31) */
	EMMC50_DS_Z_DLY1,

	/* DS Pad Z clk del sel: [dly2_sel:dly1_sel]
	 * -> [0,1]: dly1 enable [1,2]:dl2 & dly1 enable ,else :no dly enable
	 */
	EMMC50_DS_Z_DLY1_SEL,

	/* DS Pad Z clk delay count, range: 0~63, Z dly1(0~31)+Z dly2(0~31) */
	EMMC50_DS_Z_DLY2,

	/* DS Pad Z clk del sel: [dly2_sel:dly1_sel]
	 *  -> [0,1]: dly1 enable [1,2]:dl2 & dly1 enable ,else :no dly enable
	 */
	EMMC50_DS_Z_DLY2_SEL,

	/* DS Pad Z_DLY clk delay count, range: 0~31 */
	EMMC50_DS_ZDLY_DLY,
	TUNING_PARAM_COUNT,

	/* Data line rising/falling latch fine tune selection
	 * in read transaction.
	 * 1'b0: All data line share one value
	 *       indicated by MSDC_IOCON.R_D_SMPL.
	 * 1'b1: Each data line has its own  selection value
	 *       indicated by Data line (x): MSDC_IOCON.R_D(x)_SMPL
	 */
	READ_DATA_SMPL_SEL,

	/* Data line rising/falling latch fine tune selection
	 * in write transaction.
	 * 1'b0: All data line share one value indicated
	 *       by MSDC_IOCON.W_D_SMPL.
	 * 1'b1: Each data line has its own selection value indicated
	 *       by Data line (x): MSDC_IOCON.W_D(x)_SMPL
	 */
	WRITE_DATA_SMPL_SEL,

	/* Data line delay line fine tune selection.
	 * 1'b0: All data line share one delay
	 *       selection value indicated by PAD_TUNE.PAD_DAT_RD_RXDLY.
	 * 1'b1: Each data line has its own delay selection value indicated by
	 *       Data line (x): DAT_RD_DLY(x).DAT0_RD_DLY
	 */
	DATA_DLYLINE_SEL,

	/* [Data Tune]CMD & DATA Pin tune Data Selection[Data Tune Sel] */
	MSDC_DAT_TUNE_SEL,

	/* [Async_FIFO Mode Sel For Write Path] */
	MSDC_WCRC_ASYNC_FIFO_SEL,

	/* [Async_FIFO Mode Sel For CMD Path] */
	MSDC_RESP_ASYNC_FIFO_SEL,

	/* Write Path Mux for emmc50 function & emmc45 function ,
	 * Only emmc50 design valid,[1-eMMC50, 0-eMMC45]
	 */
	EMMC50_WDATA_MUX_EN,

	/* CMD Path Mux for emmc50 function & emmc45 function ,
	 * Only emmc50 design valid,[1-eMMC50, 0-eMMC45]
	 */
	EMMC50_CMD_MUX_EN,

	/* write data crc status async fifo output edge select */
	EMMC50_WDATA_EDGE,

	/* CKBUF in CKGEN Delay Selection. Total 32 stages */
	CKGEN_MSDC_DLY_SEL,

	/* CMD response turn around period.
	 * The turn around cycle = CMD_RSP_TA_CNTR + 2,
	 * Only for USH104 mode, this register should be
	 * set to 0 in non-UHS104 mode
	 */
	CMD_RSP_TA_CNTR,

	/* Write data and CRC status turn around period.
	 * The turn around cycle = WRDAT_CRCS_TA_CNTR + 2,
	 * Only for USH104 mode,  this register should be
	 * set to 0 in non-UHS104 mode
	 */
	WRDAT_CRCS_TA_CNTR,

	/* CLK Pad TX Delay Control.
	 * This register is used to add delay to CLK phase.
	 * Total 32 stages
	 */
	PAD_CLK_TXDLY,
	TOTAL_PARAM_COUNT
};

/*
 *********************************************************
 * Feature  Control Defination                           *
 *********************************************************
 */
#define AUTOK_OFFLINE_TUNE_TX_ENABLE 1
#define AUTOK_OFFLINE_TUNE_ENABLE 0
#define HS400_OFFLINE_TUNE_ENABLE 0
#define HS200_OFFLINE_TUNE_ENABLE 0
#define HS400_DSCLK_NEED_TUNING   0
#define AUTOK_PARAM_DUMP_ENABLE   0
/* #define CHIP_DENALI_3_DAT_TUNE */
/* #define SDIO_TUNE_WRITE_PATH */

enum TUNE_TYPE {
	TUNE_CMD = 0,
	TUNE_DATA,
	TUNE_LATCH_CK,
};

#define autok_msdc_retry(expr, retry, cnt) \
	do { \
		int backup = cnt; \
		while (retry) { \
			if (!(expr)) \
				break; \
			if (cnt-- == 0) { \
				retry--; cnt = backup; \
			} \
		} \
	WARN_ON(retry == 0); \
} while (0)

#define autok_msdc_reset() \
	do { \
		int retry = 3, cnt = 1000; \
		sdr_set_bits(base + MSDC_CFG, MSDC_CFG_RST); \
		/* ensure reset operation be sequential  */ \
		mb(); \
		autok_msdc_retry(readl(base + MSDC_CFG) & \
				 MSDC_CFG_RST, retry, cnt); \
	} while (0)

#define msdc_rxfifocnt() \
	((readl(base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) >> 0)
#define msdc_txfifocnt() \
	((readl(base + MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16)

#define wait_cond(cond, tmo, left) \
	do { \
		u32 t = tmo; \
		while (1) { \
			if ((cond) || (t == 0)) \
				break; \
			if (t > 0) { \
				ndelay(1); \
				t--; \
			} \
		} \
		left = t; \
	} while (0)


#define msdc_clear_fifo() \
	do { \
		int retry = 5, cnt = 1000; \
		sdr_set_bits(base + MSDC_FIFOCS, MSDC_FIFOCS_CLR); \
		/* ensure fifo clear operation be sequential  */ \
		mb(); \
		autok_msdc_retry(readl(base + MSDC_FIFOCS) & MSDC_FIFOCS_CLR, \
				 retry, cnt); \
	} while (0)

struct AUTOK_PARAM_RANGE {
	unsigned int start;
	unsigned int end;
};

struct AUTOK_PARAM_INFO {
	struct AUTOK_PARAM_RANGE range;
	char *param_name;
};

struct BOUND_INFO {
	unsigned int Bound_Start;
	unsigned int Bound_End;
	unsigned int Bound_width;
	bool is_fullbound;
};

#define BD_MAX_CNT 4	/* Max Allowed Boundary Number */
struct AUTOK_SCAN_RES {
	/* Bound info record, currently only allow max to 2 bounds exist,
	 * but in extreme case, may have 4 bounds
	 */
	struct BOUND_INFO bd_info[BD_MAX_CNT];
	/* Bound cnt record, must be in rang [0,3] */
	unsigned int bd_cnt;
	/* Full boundary cnt record */
	unsigned int fbd_cnt;
};

struct AUTOK_REF_INFO {
	/* inf[0] - rising edge res, inf[1] - falling edge res */
	struct AUTOK_SCAN_RES scan_info[2];
	/* optimised sample edge select */
	unsigned int opt_edge_sel;
	/* optimised dly cnt sel */
	unsigned int opt_dly_cnt;
	/* 1clk cycle equal how many delay cell cnt, if cycle_cnt is 0,
	 * that is cannot calc cycle_cnt by current Boundary info
	 */
	unsigned int cycle_cnt;
};

unsigned int do_autok_offline_tune_tx;
u8 sdio_autok_res[TUNING_PARAM_COUNT];

static const struct AUTOK_PARAM_INFO autok_param_info[] = {
	{{0, 1}, "CMD_EDGE"},
	/* async fifo mode Pad dat edge must fix to 0 */
	{{0, 1}, "RDATA_EDGE"},
	{{0, 1}, "RD_FIFO_EDGE"},
	{{0, 1}, "WD_FIFO_EDGE"},

	/* Cmd Pad Tune Data Phase */
	{{0, 31}, "CMD_RD_D_DLY1"},
	{{0, 1}, "CMD_RD_D_DLY1_SEL"},
	{{0, 31}, "CMD_RD_D_DLY2"},
	{{0, 1}, "CMD_RD_D_DLY2_SEL"},

	/* Data Pad Tune Data Phase */
	{{0, 31}, "DAT_RD_D_DLY1"},
	{{0, 1}, "DAT_RD_D_DLY1_SEL"},
	{{0, 31}, "DAT_RD_D_DLY2"},
	{{0, 1}, "DAT_RD_D_DLY2_SEL"},

	/* Latch CK Delay for data read when clock stop */
	{{0, 7}, "INT_DAT_LATCH_CK"},

	/* eMMC50 Related tuning param */
	{{0, 31}, "EMMC50_DS_Z_DLY1"},
	{{0, 1}, "EMMC50_DS_Z_DLY1_SEL"},
	{{0, 31}, "EMMC50_DS_Z_DLY2"},
	{{0, 1}, "EMMC50_DS_Z_DLY2_SEL"},
	{{0, 31}, "EMMC50_DS_ZDLY_DLY"},

	/* ================================================= */
	/* Timming Related Mux & Common Setting Config */
	/* all data line path share sample edge */
	{{0, 1}, "READ_DATA_SMPL_SEL"},
	{{0, 1}, "WRITE_DATA_SMPL_SEL"},
	/* clK tune all data Line share dly */
	{{0, 1}, "DATA_DLYLINE_SEL"},
	/* data tune mode select */
	{{0, 1}, "MSDC_WCRC_ASYNC_FIFO_SEL"},
	/* data tune mode select */
	{{0, 1}, "MSDC_RESP_ASYNC_FIFO_SEL"},

	/* eMMC50 Function Mux */
	/* write path switch to emmc45 */
	{{0, 1}, "EMMC50_WDATA_MUX_EN"},
	/* response path switch to emmc45 */
	{{0, 1}, "EMMC50_CMD_MUX_EN"},
	{{0, 1}, "EMMC50_WDATA_EDGE"},
	/* Common Setting Config */
	{{0, 31}, "CKGEN_MSDC_DLY_SEL"},
	{{1, 7}, "CMD_RSP_TA_CNTR"},
	{{1, 7}, "WRDAT_CRCS_TA_CNTR"},
	/* tx clk dly fix to 0 for HQA res */
	{{0, 31}, "PAD_CLK_TXDLY"},
};

static int autok_send_tune_cmd(struct msdc_host *host, unsigned int opcode,
			       enum TUNE_TYPE tune_type_value)
{
	void __iomem *base = host->base;
	unsigned int value;
	unsigned int rawcmd = 0;
	unsigned int arg = 0;
	unsigned int sts = 0;
	unsigned int wints = 0;
	unsigned int tmo = 0;
	unsigned int left = 0;
	unsigned int fifo_have = 0;
	unsigned int fifo_1k_cnt = 0;
	unsigned int i = 0;
	int ret = E_RESULT_PASS;

	switch (opcode) {
	case MMC_SEND_EXT_CSD:
		rawcmd =  (512 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (8);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			writel(host->tune_latch_ck_cnt, base + SDC_BLK_NUM);
		else
			writel(1, base + SDC_BLK_NUM);
		break;
	case MMC_STOP_TRANSMISSION:
		rawcmd = (1 << 14)  | (7 << 7) | (12);
		arg = 0;
		break;
	case MMC_SEND_STATUS:
		rawcmd = (1 << 7) | (13);
		arg = (1 << 16);
		break;
	case MMC_READ_SINGLE_BLOCK:
		left = 512;
		rawcmd =  (512 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (17);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			writel(host->tune_latch_ck_cnt, base + SDC_BLK_NUM);
		else
			writel(1, base + SDC_BLK_NUM);
		break;
	case MMC_SEND_TUNING_BLOCK:
		left = 64;
		rawcmd =  (64 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (19);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			writel(host->tune_latch_ck_cnt, base + SDC_BLK_NUM);
		else
			writel(1, base + SDC_BLK_NUM);
		break;
	case MMC_SEND_TUNING_BLOCK_HS200:
		left = 128;
		rawcmd =  (128 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (21);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			writel(host->tune_latch_ck_cnt, base + SDC_BLK_NUM);
		else
			writel(1, base + SDC_BLK_NUM);
		break;
	case MMC_WRITE_BLOCK:
		rawcmd =  (512 << 16) | (1 << 13) | (1 << 11) | (1 << 7) | (24);
		if (TUNE_DATA_TX_ADDR >= 0)
			arg = TUNE_DATA_TX_ADDR;
		else
			arg = host->mmc->card->ext_csd.sectors
				+ TUNE_DATA_TX_ADDR;
		break;
	case SD_IO_RW_DIRECT:
		break;
	case SD_IO_RW_EXTENDED:
		break;
	}

	while ((readl(base + SDC_STS) & SDC_STS_SDCBUSY))
		;

	/* clear fifo */
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA)) {
		autok_msdc_reset();
		msdc_clear_fifo();
		writel(0xffffffff, base + MSDC_INT);
	}

	/* start command */
	writel(arg, base + SDC_ARG);
	writel(rawcmd, base + SDC_CMD);

	/* wait interrupt status */
	wints = MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR;
	tmo = 0x3FFFFF;
	wait_cond(((sts = readl(base + MSDC_INT)) & wints), tmo, tmo);
	if (tmo == 0) {
		ret = E_RESULT_CMD_TMO;
		goto end;
	}

	writel((sts & wints), base + MSDC_INT);
	if (sts == 0) {
		ret = E_RESULT_CMD_TMO;
		goto end;
	}

	if (sts & MSDC_INT_CMDRDY) {
		if (tune_type_value == TUNE_CMD) {
			ret = E_RESULT_PASS;
			goto end;
		}
	} else if (sts & MSDC_INT_RSPCRCERR) {
		ret = E_RESULT_RSP_CRC;
		goto end;
	} else if (sts & MSDC_INT_CMDTMO) {
		ret = E_RESULT_CMD_TMO;
		goto end;
	}
	if ((tune_type_value != TUNE_LATCH_CK) &&
	    (tune_type_value != TUNE_DATA))
		goto skip_tune_latch_ck_and_tune_data;

	while ((readl(base + SDC_STS) & SDC_STS_SDCBUSY)) {
		if (tune_type_value == TUNE_LATCH_CK) {
			fifo_have = msdc_rxfifocnt();
			if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) ||
			    (opcode == MMC_READ_SINGLE_BLOCK) ||
			    (opcode == MMC_SEND_EXT_CSD) ||
			    (opcode == MMC_SEND_TUNING_BLOCK)) {
				sdr_set_field(base + MSDC_DBG_SEL,
					      0xffff << 0, 0x0b);
				sdr_get_field(base + MSDC_DBG_OUT,
					      0x7ff << 0, &fifo_1k_cnt);
				if ((fifo_1k_cnt >= MSDC_FIFO_THD_1K) &&
				    (fifo_have >= MSDC_FIFO_SZ)) {
					value = readl(base + MSDC_RXDATA);
					value = readl(base + MSDC_RXDATA);
					value = readl(base + MSDC_RXDATA);
					value = readl(base + MSDC_RXDATA);
				}
			}
		} else if ((tune_type_value == TUNE_DATA) &&
			   (opcode == MMC_WRITE_BLOCK)) {
			for (i = 0; i < 64; i++) {
				writel(0x5af00fa5, base + MSDC_TXDATA);
				writel(0x33cc33cc, base + MSDC_TXDATA);
			}

			while ((readl(base + SDC_STS) & SDC_STS_SDCBUSY))
				;
		}
	}

	sts = readl(base + MSDC_INT);
	wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	if (sts) {
		/* clear status */
		writel((sts & wints), base + MSDC_INT);
		if (sts & MSDC_INT_XFER_COMPL)
			ret = E_RESULT_PASS;
		if (MSDC_INT_DATCRCERR & sts)
			ret = E_RESULT_DAT_CRC;
		if (MSDC_INT_DATTMO & sts)
			ret = E_RESULT_DAT_TMO;
	}

skip_tune_latch_ck_and_tune_data:
	while ((readl(base + SDC_STS) & SDC_STS_SDCBUSY))
		;
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA))
		msdc_clear_fifo();

end:
	if (opcode == MMC_STOP_TRANSMISSION) {
		while ((readl(base + MSDC_PS) & 0x10000) != 0x10000)
			;
	}

	return ret;
}

static int autok_simple_score64(char *res_str64, u64 result64)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (result64 == 0) {
		/* maybe result is 0 */
		strcpy(res_str64,
	"OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");
		return 64;
	}
	if (result64 == 0xFFFFFFFFFFFFFFFF) {
		strcpy(res_str64,
	"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
		return 0;
	}

	/* calc continue zero number */
	while (bit < 64) {
		if (result64 & ((u64) (1LL << bit))) {
			res_str64[bit] = 'X';
			bit++;
			if (old < num)
				old = num;
			num = 0;
			continue;
		}
		res_str64[bit] = 'O';
		bit++;
		num++;
	}
	if (num > old)
		old = num;

	return old;
}

enum {
	RD_SCAN_NONE,
	RD_SCAN_PAD_BOUND_S,
	RD_SCAN_PAD_BOUND_E,
	RD_SCAN_PAD_MARGIN,
};

static int autok_check_scan_res64(u64 rawdat, struct AUTOK_SCAN_RES *scan_res)
{
	unsigned int bit;
	unsigned int filter = 4;
	struct BOUND_INFO *pBD = (struct BOUND_INFO *)scan_res->bd_info;
	unsigned int RawScanSta = RD_SCAN_NONE;

	for (bit = 0; bit < 64; bit++) {
		if (rawdat & (1LL << bit)) {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				pBD->Bound_Start = 0;
				pBD->Bound_width = 1;
				scan_res->bd_cnt += 1;
				break;
			case RD_SCAN_PAD_MARGIN:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				pBD->Bound_Start = bit;
				pBD->Bound_width = 1;
				scan_res->bd_cnt += 1;
				break;
			case RD_SCAN_PAD_BOUND_E:
				if ((filter) && ((bit - pBD->Bound_End) <=
						 AUTOK_TUNING_INACCURACY)) {
					ATK_DBG(ATK_TRACE,
				"[AUTOK]WARN: Try to filter the holes\n");
					RawScanSta = RD_SCAN_PAD_BOUND_S;

					pBD->Bound_width += (bit -
							     pBD->Bound_End);
					pBD->Bound_End = 0;
					filter--;

					/* update full bound info */
					if (pBD->is_fullbound) {
						pBD->is_fullbound = 0;
						scan_res->fbd_cnt -= 1;
					}
				} else {
					/* No filter Check and Get the next
					 * boundary information
					 */
					RawScanSta = RD_SCAN_PAD_BOUND_S;
					pBD++;
					pBD->Bound_Start = bit;
					pBD->Bound_width = 1;
					scan_res->bd_cnt += 1;
					if (scan_res->bd_cnt > BD_MAX_CNT) {
						ATK_ERR(
				"[AUTOK]Error: more than %d Boundary Exist\n",
				BD_MAX_CNT);
						return -1;
					}
				}
				break;
			case RD_SCAN_PAD_BOUND_S:
				pBD->Bound_width++;
				break;
			default:
				break;
			}
		} else {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_MARGIN;
				break;
			case RD_SCAN_PAD_BOUND_S:
				RawScanSta = RD_SCAN_PAD_BOUND_E;
				pBD->Bound_End = bit - 1;
				/* update full bound info */
				if (pBD->Bound_Start > 0) {
					pBD->is_fullbound = 1;
					scan_res->fbd_cnt += 1;
				}
				break;
			case RD_SCAN_PAD_MARGIN:
			case RD_SCAN_PAD_BOUND_E:
			default:
				break;
			}
		}
	}
	if ((pBD->Bound_End == 0) && (pBD->Bound_width != 0))
		pBD->Bound_End = pBD->Bound_Start + pBD->Bound_width - 1;

	return 0;
}

static int autok_pad_dly_corner_check(struct AUTOK_REF_INFO *pInfo)
{
	/* scan result @ rising edge */
	struct AUTOK_SCAN_RES *pBdInfo_R = NULL;
	/* scan result @ falling edge */
	struct AUTOK_SCAN_RES *pBdInfo_F = NULL;
	struct AUTOK_SCAN_RES *p_Temp[2] = {NULL,};
	unsigned int i, j, k, l;
	unsigned int pass_bd_size[BD_MAX_CNT + 1];
	unsigned int max_pass = 0;
	unsigned int max_size = 0;
	unsigned int bd_max_size = 0;
	unsigned int bd_overlap = 0;
	unsigned int corner_case_flag = 0;

	pBdInfo_R = &(pInfo->scan_info[0]);
	pBdInfo_F = &(pInfo->scan_info[1]);
	/*
	 * for corner case
	 * oooooooooooooooooo rising has no fail bound
	 * oooooooooooooooooo falling has no fail bound
	 */
	if ((pBdInfo_R->bd_cnt == 0) && (pBdInfo_F->bd_cnt == 0)) {
		ATK_ERR("[ATUOK]Warn:can't find bd both edge\r\n");
		pInfo->opt_dly_cnt = 31;
		pInfo->opt_edge_sel = 0;
		return AUTOK_RECOVERABLE_ERROR;
	}
	/*
	 * for corner case
	 * xxxxxxxxxxxxxxxxxxxx rising only has one boundary,but all fail
	 * oooooooooxxooooooo falling has normal boundary
	 * or
	 * ooooooooooooxooooo rising has normal boundary
	 * xxxxxxxxxxxxxxxxxxxx falling only has one boundary,but all fail
	 */
	if ((pBdInfo_R->bd_cnt == 1) && (pBdInfo_F->bd_cnt == 1)
		&& (pBdInfo_R->bd_info[0].Bound_Start == 0)
		&& (pBdInfo_R->bd_info[0].Bound_End == 63)
		&& (pBdInfo_F->bd_info[0].Bound_Start == 0)
		&& (pBdInfo_F->bd_info[0].Bound_End == 63)) {
		ATK_ERR("[ATUOK]Err:can't find window both edge\r\n");
		return AUTOK_NONE_RECOVERABLE_ERROR;
	}
	for (j = 0; j < sizeof(p_Temp); j++) {
		if (j == 0) {
			p_Temp[0] = pBdInfo_R;
			p_Temp[1] = pBdInfo_F;
		} else {
			p_Temp[0] = pBdInfo_F;
			p_Temp[1] = pBdInfo_R;
		}
		/* check boundary overlap */
		for (k = 0; k < p_Temp[0]->bd_cnt; k++) {
			for (l = 0; l < p_Temp[1]->bd_cnt; l++)
				if (((p_Temp[0]->bd_info[k].Bound_Start
				    >= p_Temp[1]->bd_info[l].Bound_Start)
				    && (p_Temp[0]->bd_info[k].Bound_Start
				    <= p_Temp[1]->bd_info[l].Bound_End))
				    || ((p_Temp[0]->bd_info[k].Bound_End
				    <= p_Temp[1]->bd_info[l].Bound_End)
				    && (p_Temp[0]->bd_info[k].Bound_End
				    >= p_Temp[1]->bd_info[l].Bound_Start))
				    || ((p_Temp[1]->bd_info[l].Bound_Start
				    >= p_Temp[0]->bd_info[k].Bound_Start)
				    && (p_Temp[1]->bd_info[l].Bound_Start
				    <= p_Temp[0]->bd_info[k].Bound_End)))
					bd_overlap = 1;
		}
		/*check max boundary size */
		for (k = 0; k < p_Temp[0]->bd_cnt; k++) {
			if ((p_Temp[0]->bd_info[k].Bound_End
				- p_Temp[0]->bd_info[k].Bound_Start)
				>= 20)
				bd_max_size = 1;
		}
		if (((bd_overlap == 1)
			&& (bd_max_size == 1))
			|| ((p_Temp[1]->bd_cnt == 0)
			&& (bd_max_size == 1))) {
			corner_case_flag = 1;
		}
		if (((p_Temp[0]->bd_cnt == 1)
			&& (p_Temp[0]->bd_info[0].Bound_Start == 0)
			&& (p_Temp[0]->bd_info[0].Bound_End == 63))
			|| (corner_case_flag == 1)) {
			if (j == 0)
				pInfo->opt_edge_sel = 1;
			else
				pInfo->opt_edge_sel = 0;
			/* 1T calc fail,need check max pass bd,select mid */
			switch (p_Temp[1]->bd_cnt) {
			case 4:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
				    p_Temp[1]->bd_info[1].Bound_Start
					- p_Temp[1]->bd_info[0].Bound_End;
				pass_bd_size[2] =
				    p_Temp[1]->bd_info[2].Bound_Start
					- p_Temp[1]->bd_info[1].Bound_End;
				pass_bd_size[3] =
				    p_Temp[1]->bd_info[3].Bound_Start
					- p_Temp[1]->bd_info[2].Bound_End;
				pass_bd_size[4] =
				    63 - p_Temp[1]->bd_info[3].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 5; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
					p_Temp[1]->bd_info[0].Bound_Start
					/ 2;
				else if (max_pass == 4)
					pInfo->opt_dly_cnt =
					(63 +
					p_Temp[1]->bd_info[3].Bound_End)
					/ 2;
				else {
					pInfo->opt_dly_cnt =
				    (p_Temp[1]->bd_info[max_pass].Bound_Start
				    +
				    p_Temp[1]->bd_info[max_pass - 1].Bound_End)
				    / 2;
				}
				break;
			case 3:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
				    p_Temp[1]->bd_info[1].Bound_Start
					- p_Temp[1]->bd_info[0].Bound_End;
				pass_bd_size[2] =
				    p_Temp[1]->bd_info[2].Bound_Start
					- p_Temp[1]->bd_info[1].Bound_End;
				pass_bd_size[3] =
				    63 - p_Temp[1]->bd_info[2].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 4; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
				    p_Temp[1]->bd_info[0].Bound_Start / 2;
				else if (max_pass == 3)
					pInfo->opt_dly_cnt =
				    (63 + p_Temp[1]->bd_info[2].Bound_End) / 2;
				else {
					pInfo->opt_dly_cnt =
				    (p_Temp[1]->bd_info[max_pass].Bound_Start
				    +
				    p_Temp[1]->bd_info[max_pass - 1].Bound_End)
				    / 2;
				}
				break;
			case 2:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
				    p_Temp[1]->bd_info[1].Bound_Start
					- p_Temp[1]->bd_info[0].Bound_End;
				pass_bd_size[2] =
				    63 - p_Temp[1]->bd_info[1].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 3; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
					p_Temp[1]->bd_info[0].Bound_Start / 2;
				else if (max_pass == 2)
					pInfo->opt_dly_cnt =
				    (63 + p_Temp[1]->bd_info[1].Bound_End) / 2;
				else {
					pInfo->opt_dly_cnt =
				    (p_Temp[1]->bd_info[max_pass].Bound_Start
				    +
				    p_Temp[1]->bd_info[max_pass - 1].Bound_End)
				    / 2;
				}
				break;
			case 1:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
					63 -
					p_Temp[1]->bd_info[0].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 2; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
					p_Temp[1]->bd_info[0].Bound_Start
					/ 2;
				else if (max_pass == 1)
					pInfo->opt_dly_cnt =
				    (63 +
				    p_Temp[1]->bd_info[0].Bound_End)
				    / 2;
				break;
			case 0:
				pInfo->opt_dly_cnt = 31;
				break;
			default:
				break;
			}
			return AUTOK_RECOVERABLE_ERROR;
		}
	}
	return 0;
}

static int autok_pad_dly_sel(struct AUTOK_REF_INFO *pInfo)
{
	/* scan result @ rising edge */
	struct AUTOK_SCAN_RES *pBdInfo_R = NULL;
	/* scan result @ falling edge */
	struct AUTOK_SCAN_RES *pBdInfo_F = NULL;
	/* Save the first boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdPrev = NULL;
	/* Save the second boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdNext = NULL;
	struct BOUND_INFO *pBdTmp = NULL;
	/* Full Boundary count */
	unsigned int FBound_Cnt_R = 0;
	unsigned int Bound_Cnt_R = 0;
	unsigned int Bound_Cnt_F = 0;
	unsigned int cycle_cnt = 64;
	int uBD_mid_prev = 0;
	int uBD_mid_next = 0;
	int uBD_width = 3;
	int uDlySel_F = 0;
	int uDlySel_R = 0;
	/* for falling edge margin compress */
	int uMgLost_F = 0;
	/* for rising edge margin compress */
	int uMgLost_R = 0;
	unsigned int i;
	unsigned int ret = 0;
	int corner_res = 0;

	pBdInfo_R = &(pInfo->scan_info[0]);
	pBdInfo_F = &(pInfo->scan_info[1]);
	FBound_Cnt_R = pBdInfo_R->fbd_cnt;
	Bound_Cnt_R = pBdInfo_R->bd_cnt;
	Bound_Cnt_F = pBdInfo_F->bd_cnt;

	corner_res = autok_pad_dly_corner_check(pInfo);
	if (corner_res == -1)
		return 0;
	else if (corner_res == -2)
		return -2;

	switch (FBound_Cnt_R) {
	case 4:	/* SSSS Corner may cover 2~3T */
	case 3:
		ATK_ERR("[AUTOK]Warning: Too Many Full boundary count:%d\r\n",
			FBound_Cnt_R);
	case 2:	/* mode_1 : 2 full boudary */
		for (i = 0; i < BD_MAX_CNT; i++) {
			if (pBdInfo_R->bd_info[i].is_fullbound) {
				if (pBdPrev == NULL) {
					pBdPrev = &(pBdInfo_R->bd_info[i]);
				} else {
					pBdNext = &(pBdInfo_R->bd_info[i]);
					break;
				}
			}
		}

		if (pBdPrev && pBdNext) {
			uBD_mid_prev = (pBdPrev->Bound_Start +
					pBdPrev->Bound_End) / 2;
			uBD_mid_next = (pBdNext->Bound_Start +
					pBdNext->Bound_End) / 2;
			/* while in 2 full bound case, bd_width calc */
			uBD_width = (pBdPrev->Bound_width +
				     pBdNext->Bound_width) / 2;
			cycle_cnt = uBD_mid_next - uBD_mid_prev;
			/* delay count sel at rising edge */
			if (uBD_mid_prev >= cycle_cnt / 2) {
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if ((cycle_cnt / 2 - uBD_mid_prev) >
				   AUTOK_MARGIN_THOLD) {
				uDlySel_R = uBD_mid_prev + cycle_cnt / 2;
				uMgLost_R = 0;
			} else {
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			}
			/* delay count sel at falling edge */
			pBdTmp = &(pBdInfo_R->bd_info[0]);
			if (pBdTmp->is_fullbound) {
				/* ooooxxxooooooxxxooo */
				uDlySel_F = uBD_mid_prev;
				uMgLost_F = 0;
			} else {
				/* xooooooxxxoooooooxxxoo */
				if (pBdTmp->Bound_End > uBD_width / 2) {
					uDlySel_F = (pBdTmp->Bound_End) -
						    (uBD_width / 2);
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F = (uBD_width / 2) -
						    (pBdTmp->Bound_End);
				}
			}
		} else {
			/* error can not find 2 foull boary */
			ATK_ERR("[AUTOK] can not find 2 full boudary @Mode1\n");
			return -1;
		}
		break;

	case 1:	/* rising edge find one full boundary */
		if (Bound_Cnt_R > 1) {
			/* mode_2: 1 full boundary and boundary count > 1 */
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			pBdNext = &(pBdInfo_R->bd_info[1]);

			if (pBdPrev->is_fullbound)
				uBD_width = pBdPrev->Bound_width;
			else
				uBD_width = pBdNext->Bound_width;

			if ((pBdPrev->is_fullbound) ||
			    (pBdNext->is_fullbound)) {
				if (pBdPrev->Bound_Start > 0)
					cycle_cnt = pBdNext->Bound_Start -
						    pBdPrev->Bound_Start;
				else
					cycle_cnt = pBdNext->Bound_End -
						    pBdPrev->Bound_End;

				/* delay count sel@rising & falling edge */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev = (pBdPrev->Bound_Start +
							pBdPrev->Bound_End) / 2;
					uDlySel_F = uBD_mid_prev;
					uMgLost_F = 0;
					if (uBD_mid_prev >= cycle_cnt / 2) {
						uDlySel_R = uBD_mid_prev -
							    cycle_cnt / 2;
						uMgLost_R = 0;
					} else if ((cycle_cnt / 2 -
						    uBD_mid_prev) >
						    AUTOK_MARGIN_THOLD) {
						uDlySel_R = uBD_mid_prev +
							    cycle_cnt / 2;
						uMgLost_R = 0;
					} else {
						uDlySel_R = 0;
						uMgLost_R = cycle_cnt / 2 -
							    uBD_mid_prev;
					}
				} else {
					/* first boundary not full boudary */
					uBD_mid_next = (pBdNext->Bound_Start +
							pBdNext->Bound_End) / 2;
					uDlySel_R = uBD_mid_next -
						    cycle_cnt / 2;
					uMgLost_R = 0;
					if (pBdPrev->Bound_End >
					    uBD_width / 2) {
						uDlySel_F = pBdPrev->Bound_End -
							    (uBD_width / 2);
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F = (uBD_width / 2) -
							(pBdPrev->Bound_End);
					}
				}
			} else {
				/* full bound must in first 2 boundary */
				return -1;
			}
		} else if (Bound_Cnt_F > 0) {
			/* mode_3: 1 full boundary and only
			 * one boundary exist @rising edge
			 */
			/* this boundary is full bound */
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			pBdNext = &(pBdInfo_F->bd_info[0]);
			uBD_mid_prev = (pBdPrev->Bound_Start +
					pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdNext->Bound_Start == 0) {
				cycle_cnt = (pBdPrev->Bound_End -
					     pBdNext->Bound_End) * 2;
			} else if (pBdNext->Bound_End == 63) {
				cycle_cnt = (pBdNext->Bound_Start -
					     pBdPrev->Bound_Start) * 2;
			} else {
				uBD_mid_next = (pBdNext->Bound_Start +
						pBdNext->Bound_End) / 2;

				if (uBD_mid_next > uBD_mid_prev)
					cycle_cnt = (uBD_mid_next -
						     uBD_mid_prev) * 2;
				else
					cycle_cnt = (uBD_mid_prev -
						     uBD_mid_next) * 2;
			}

			uDlySel_F = uBD_mid_prev;
			uMgLost_F = 0;

			if (uBD_mid_prev >= cycle_cnt / 2) {
				/* case 1 */
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if (cycle_cnt / 2 - uBD_mid_prev <=
				   AUTOK_MARGIN_THOLD) {
				/* case 2 */
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			} else if (cycle_cnt / 2 + uBD_mid_prev <= 63) {
				/* case 3 */
				uDlySel_R = cycle_cnt / 2 + uBD_mid_prev;
				uMgLost_R = 0;
			} else if (32 - uBD_mid_prev <= AUTOK_MARGIN_THOLD) {
				/* case 4 */
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			} else { /* case 5 */
				uDlySel_R = 63;
				uMgLost_R = uBD_mid_prev + cycle_cnt / 2 - 63;
			}
		} else {
			/* mode_4: falling edge no boundary found & rising
			 * edge only one full boundary exist
			 */
			/* this boundary is full bound */
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			uBD_mid_prev = (pBdPrev->Bound_Start +
					pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdPrev->Bound_End > (64 - pBdPrev->Bound_Start))
				cycle_cnt = 2 * (pBdPrev->Bound_End + 1);
			else
				cycle_cnt = 2 * (64 - pBdPrev->Bound_Start);

			uDlySel_R = (uBD_mid_prev >= 32) ? 0 : 63;
			/* Margin enough donot care margin lost */
			uMgLost_R = 0xFF;
			uDlySel_F = uBD_mid_prev;
			/* Margin enough donot care margin lost */
			uMgLost_F = 0xFF;

			ATK_ERR("[AUTOK]Warning: 1T > %d\n", cycle_cnt);
		}
		break;

	case 0:	/* rising edge cannot find full boudary */
		if (Bound_Cnt_R == 2) {
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			/* this boundary is full bound */
			pBdNext = &(pBdInfo_F->bd_info[0]);

			if (pBdNext->is_fullbound) {
				/* mode_5: rising_edge 2 boundary
				 * (not full bound), falling edge
				 * one full boundary
				 */
				uBD_width = pBdNext->Bound_width;
				cycle_cnt = 2 * (pBdNext->Bound_End -
						 pBdPrev->Bound_End);
				uBD_mid_next = (pBdNext->Bound_Start +
						pBdNext->Bound_End) / 2;
				uDlySel_R = uBD_mid_next;
				uMgLost_R = 0;
				if (pBdPrev->Bound_End >= uBD_width / 2) {
					uDlySel_F = pBdPrev->Bound_End -
						    uBD_width / 2;
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F = uBD_width / 2 -
						    pBdPrev->Bound_End;
				}
			} else {
				/* for falling edge there must be one full
				 * boundary between two bounary_mid at rising
				 */
				return -1;
			}
		} else if (Bound_Cnt_R == 1) {
			if (Bound_Cnt_F > 1) {
				/* when rising_edge have only one boundary
				 * (not full bound), falling edge should not
				 * more than 1Bound exist
				 */
				return -1;
			} else if (Bound_Cnt_F == 1) {
				/* mode_6: rising edge only 1 boundary
				 * (not full Bound)
				 * & falling edge have only 1 bound too
				 */
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				pBdNext = &(pBdInfo_F->bd_info[0]);
				if (pBdNext->is_fullbound) {
					uBD_width = pBdNext->Bound_width;
				} else {
					if (pBdNext->Bound_width >
					    pBdPrev->Bound_width)
						uBD_width = pBdNext->Bound_width
							+ 1;
					else
						uBD_width = pBdPrev->Bound_width
							+ 1;

					if (uBD_width < AUTOK_BD_WIDTH_REF)
						uBD_width = AUTOK_BD_WIDTH_REF;
				} /* Boundary width calc done */

				if (pBdPrev->Bound_Start == 0) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_Start == 0)
						return -1;

					cycle_cnt = (pBdNext->Bound_Start -
						     pBdPrev->Bound_End +
						     uBD_width) * 2;
				} else if (pBdPrev->Bound_End == 63) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_End == 63)
						return -1;

					cycle_cnt = (pBdPrev->Bound_Start -
						     pBdNext->Bound_End +
						     uBD_width) * 2;
				} /* cycle count calc done */

				/* calc optimise delay count */
				if (pBdPrev->Bound_Start == 0) {
					/* falling edge sel */
					if (pBdPrev->Bound_End >=
					    uBD_width / 2) {
						uDlySel_F = pBdPrev->Bound_End -
							    uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F = uBD_width / 2 -
							    pBdPrev->Bound_End;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_End - uBD_width / 2 +
					    cycle_cnt / 2 > 63) {
						uDlySel_R = 63;
						uMgLost_R =
						    pBdPrev->Bound_End -
						    uBD_width / 2 +
						    cycle_cnt / 2 - 63;
					} else {
						uDlySel_R =
						    pBdPrev->Bound_End -
						    uBD_width / 2 +
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else if (pBdPrev->Bound_End == 63) {
					/* falling edge sel */
					if (pBdPrev->Bound_Start +
					    uBD_width / 2 < 63) {
						uDlySel_F =
							pBdPrev->Bound_Start +
							uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 63;
						uMgLost_F =
							pBdPrev->Bound_Start +
							uBD_width / 2 - 63;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_Start +
					    uBD_width / 2 - cycle_cnt / 2 < 0) {
						uDlySel_R = 0;
						uMgLost_R =
						    cycle_cnt / 2 -
						    (pBdPrev->Bound_Start +
						     uBD_width / 2);
					} else {
						uDlySel_R =
						    pBdPrev->Bound_Start +
						    uBD_width / 2 -
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else {
					return -1;
				}
			} else if (Bound_Cnt_F == 0) {
				/* mode_7: rising edge only one bound
				 * (not full), falling no boundary
				 */
				cycle_cnt = 128;
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				if (pBdPrev->Bound_Start == 0) {
					uDlySel_F = 0;
					uDlySel_R = 63;
				} else if (pBdPrev->Bound_End == 63) {
					uDlySel_F = 63;
					uDlySel_R = 0;
				} else {
					return -1;
				}
				uMgLost_F = 0xFF;
				uMgLost_R = 0xFF;

				ATK_ERR("[AUTOK]Warning: 1T > %d\n", cycle_cnt);
			}
		} else if (Bound_Cnt_R == 0) { /* Rising Edge No Boundary */
			if (Bound_Cnt_F > 1) {
				/* falling edge not allowed two boundary
				 * Exist for this case
				 */
				return -1;
			} else if (Bound_Cnt_F > 0) {
				/* mode_8: falling edge has one Boundary */
				pBdPrev = &(pBdInfo_F->bd_info[0]);

				/* this boundary is full bound */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev =
					    (pBdPrev->Bound_Start +
					     pBdPrev->Bound_End) / 2;

					if (pBdPrev->Bound_End >
					    (64 - pBdPrev->Bound_Start))
						cycle_cnt =
						2 * (pBdPrev->Bound_End + 1);
					else
						cycle_cnt =
						2 * (64 - pBdPrev->Bound_Start);

					uDlySel_R = uBD_mid_prev;
					uMgLost_R = 0xFF;
					uDlySel_F =
						(uBD_mid_prev >= 32) ? 0 : 63;
					uMgLost_F = 0xFF;
				} else {
					cycle_cnt = 128;

					uDlySel_R = (pBdPrev->Bound_Start ==
						     0) ? 0 : 63;
					uMgLost_R = 0xFF;
					uDlySel_F = (pBdPrev->Bound_Start ==
						     0) ? 63 : 0;
					uMgLost_F = 0xFF;
				}

				ATK_ERR("[AUTOK]Warning: 1T > %d\n", cycle_cnt);
			} else {
				/* falling edge no boundary. no need tuning */
				cycle_cnt = 128;
				uDlySel_F = 0;
				uMgLost_F = 0xFF;
				uDlySel_R = 0;
				uMgLost_R = 0xFF;
				ATK_ERR("[AUTOK]Warning: 1T > %d\n", cycle_cnt);
			}
		} else {
			/* Error if bound_cnt > 3 there must be
			 * at least one full boundary exist
			 */
			return -1;
		}
		break;

	default:
		/* warning if boundary count > 4
		 * (from current hw design, this case cannot happen)
		 */
		return -1;
	}

	/* Select Optimised Sample edge & delay count (the small one) */
	pInfo->cycle_cnt = cycle_cnt;
	if (uDlySel_R <= uDlySel_F) {
		pInfo->opt_edge_sel = 0;
		pInfo->opt_dly_cnt = uDlySel_R;
	} else {
		pInfo->opt_edge_sel = 1;
		pInfo->opt_dly_cnt = uDlySel_F;

	}
	ATK_ERR("[AUTOK]Analysis Result: 1T = %d\n ", cycle_cnt);
	return ret;
}

/*
 ************************************************************************
 * FUNCTION
 *  autok_adjust_param
 *
 * DESCRIPTION
 *  This function for auto-K, adjust msdc parameter
 *
 * PARAMETERS
 *    host: msdc host manipulator pointer
 *    param: enum of msdc parameter
 *    value: value of msdc parameter
 *    rw: AUTOK_READ/AUTOK_WRITE
 *
 * RETURN VALUES
 *    error code: 0 success,
 *               -1 parameter input error
 *               -2 read/write fail
 *               -3 else error
 *************************************************************************
 */
static int autok_adjust_param(struct msdc_host *host,
				   enum AUTOK_PARAM param,
				   u32 *value,
				   int rw)
{
	void __iomem *base = host->base;
	void __iomem *base_top = host->base_top;
	u32 *reg;
	u32 field = 0;

	switch (param) {
	case READ_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("READ_DATA_SMPL_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}

		reg = (u32 *) (base + MSDC_IOCON);
		field = (u32) (MSDC_IOCON_R_D_SMPL_SEL);
		break;
	case WRITE_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("WRITE_DATA_SMPL_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}

		reg = (u32 *) (base + MSDC_IOCON);
		field = (u32) (MSDC_IOCON_W_D_SMPL_SEL);
		break;
	case DATA_DLYLINE_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("DATA_DLYLINE_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CONTROL);
			field = (u32) (DATA_K_VALUE_SEL);
		} else {
			reg = (u32 *) (base + MSDC_IOCON);
			field = (u32) (MSDC_IOCON_DDLSEL);
		}
		break;
	case MSDC_DAT_TUNE_SEL:	/* 0-Dat tune 1-CLk tune ; */
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("DATA_TUNE_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CONTROL);
			field = (u32) (PAD_RXDLY_SEL);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE0);
			field = (u32) (MSDC_PAD_TUNE0_RXDLYSEL);
		}
		break;
	case MSDC_WCRC_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("WCRC_ASYNC_FIFO_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT2);
		field = (u32) (MSDC_PB2_CFGCRCSTS);
		break;
	case MSDC_RESP_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("RESP_ASYNC_FIFO_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT2);
		field = (u32) (MSDC_PB2_CFGRESP);
		break;
	case CMD_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("CMD_EDGE(%d) is out of [0~1]\n", *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_IOCON);
		field = (u32) (MSDC_IOCON_RSPL);
		break;
	case RDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("RDATA_EDGE(%d) is out of [0~1]\n", *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_IOCON);
		field = (u32) (MSDC_IOCON_R_D_SMPL);
		break;
	case RD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("RD_FIFO_EDGE(%d) is out of [0~1]\n", *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT0);
		field = (u32) (MSDC_PB0_RD_DAT_SEL);
		break;
	case WD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("WD_FIFO_EDGE(%d) is out of [0~1]\n", *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT2);
		field = (u32) (MSDC_PB2_CFGCRCSTSEDGE);
		break;
	case CMD_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("CMD_RD_D_DLY1(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CMD);
			field = (u32) (PAD_CMD_RXDLY);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE0);
			field = (u32) (MSDC_PAD_TUNE0_CMDRDLY);
		}
		break;
	case CMD_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("CMD_RD_D_DLY1_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CMD);
			field = (u32) (PAD_CMD_RD_RXDLY_SEL);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE0);
			field = (u32) (MSDC_PAD_TUNE0_CMDRRDLYSEL);
		}
		break;
	case CMD_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("CMD_RD_D_DLY2(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CMD);
			field = (u32) (PAD_CMD_RXDLY2);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE1);
			field = (u32) (MSDC_PAD_TUNE1_CMDRDLY2);
		}
		break;
	case CMD_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("CMD_RD_D_DLY2_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CMD);
			field = (u32) (PAD_CMD_RD_RXDLY2_SEL);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE1);
			field = (u32) (MSDC_PAD_TUNE1_CMDRRDLY2SEL);
		}
		break;
	case DAT_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("DAT_RD_D_DLY1(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CONTROL);
			field = (u32) (PAD_DAT_RD_RXDLY);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE0);
			field = (u32) (MSDC_PAD_TUNE0_DATRRDLY);
		}
		break;
	case DAT_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("DAT_RD_D_DLY1_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CONTROL);
			field = (u32) (PAD_DAT_RD_RXDLY_SEL);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE0);
			field = (u32) (MSDC_PAD_TUNE0_DATRRDLYSEL);
		}
		break;
	case DAT_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("DAT_RD_D_DLY2(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CONTROL);
			field = (u32) (PAD_DAT_RD_RXDLY2);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE1);
			field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2);
		}
		break;
	case DAT_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("DAT_RD_D_DLY2_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_CONTROL);
			field = (u32) (PAD_DAT_RD_RXDLY2_SEL);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE1);
			field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2SEL);
		}
		break;
	case INT_DAT_LATCH_CK:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug("INT_DAT_LATCH_CK(%d) is out of [0~7]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT0);
		field = (u32) (MSDC_PB0_INT_DAT_LATCH_CK_SEL);
		break;
	case CKGEN_MSDC_DLY_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("CKGEN_MSDC_DLY_SEL(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT0);
		field = (u32) (MSDC_PB0_CKGEN_MSDC_DLY_SEL);
		break;
	case CMD_RSP_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug("CMD_RSP_TA_CNTR(%d) is out of [0~7]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT1);
		field = (u32) (MSDC_PB1_CMD_RSP_TA_CNTR);
		break;
	case WRDAT_CRCS_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug("WRDAT_CRCS_TA_CNTR(%d) is out of [0~7]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + MSDC_PATCH_BIT1);
		field = (u32) (MSDC_PB1_WRDAT_CRCS_TA_CNTR);
		break;
	case PAD_CLK_TXDLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("PAD_CLK_TXDLY(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_PAD_CTRL0);
			field = (u32) (MSDC_PAD_CLK_TXDLY);
		} else {
			reg = (u32 *) (base + MSDC_PAD_TUNE0);
			field = (u32) (MSDC_PAD_TUNE0_CLKTXDLY);
		}
		break;
	case EMMC50_WDATA_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("EMMC50_WDATA_MUX_EN(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + EMMC50_CFG0);
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_SEL);
		break;
	case EMMC50_CMD_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("EMMC50_CMD_MUX_EN(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + EMMC50_CFG0);
		field = (u32) (MSDC_EMMC50_CFG_CMD_RESP_SEL);
		break;
	case EMMC50_WDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("EMMC50_WDATA_EDGE(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		reg = (u32 *) (base + EMMC50_CFG0);
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_EDGE);
		break;
	case EMMC50_DS_Z_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("EMMC50_DS_Z_DLY1(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_PAD_DS_TUNE);
			field = (u32) (PAD_DS_DLY1);
		} else {
			reg = (u32 *) (base + EMMC50_PAD_DS_TUNE);
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY1);
		}
		break;
	case EMMC50_DS_Z_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("EMMC50_DS_Z_DLY1_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_PAD_DS_TUNE);
			field = (u32) (PAD_DS_DLY_SEL);
		} else {
			reg = (u32 *) (base + EMMC50_PAD_DS_TUNE);
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLYSEL);
		}
		break;
	case EMMC50_DS_Z_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("EMMC50_DS_Z_DLY2(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_PAD_DS_TUNE);
			field = (u32) (PAD_DS_DLY2);
		} else {
			reg = (u32 *) (base + EMMC50_PAD_DS_TUNE);
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2);
		}
		break;
	case EMMC50_DS_Z_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug("EMMC50_DS_Z_DLY2_SEL(%d) is out of [0~1]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_PAD_DS_TUNE);
			field = (u32) (PAD_DS_DLY2_SEL);
		} else {
			reg = (u32 *) (base + EMMC50_PAD_DS_TUNE);
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2SEL);
		}
		break;
	case EMMC50_DS_ZDLY_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug("EMMC50_DS_Z_DLY(%d) is out of [0~31]\n",
				 *value);
			return -1;
		}
		if (host->base_top) {
			reg = (u32 *) (base_top + MSDC_TOP_PAD_DS_TUNE);
			field = (u32) (PAD_DS_DLY3);
		} else {
			reg = (u32 *) (base + EMMC50_PAD_DS_TUNE);
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY3);
		}
		break;
	default:
		pr_debug("Value of [enum AUTOK_PARAM param] is wrong\n");
		return -1;
	}

	if (rw == AUTOK_READ)
		sdr_get_field(reg, field, value);
	else if (rw == AUTOK_WRITE) {
		sdr_set_field(reg, field, *value);

		if (param == CKGEN_MSDC_DLY_SEL)
			mdelay(1);
	} else {
		pr_debug("Value of [int rw] is wrong\n");
		return -1;
	}

	return 0;
}

static int autok_param_update(enum AUTOK_PARAM param_id,
			      unsigned int result, u8 *autok_tune_res)
{
	if (param_id < TUNING_PARAM_COUNT) {
		if ((result > autok_param_info[param_id].range.end) ||
		    (result < autok_param_info[param_id].range.start)) {
			ATK_ERR("[AUTOK]param:%d out of range[%d,%d]\n",
				result,
				autok_param_info[param_id].range.start,
				autok_param_info[param_id].range.end);
			return -1;
		}
		autok_tune_res[param_id] = (u8) result;
		return 0;
	}
	ATK_ERR("[AUTOK]param not found\r\n");

	return -1;
}

static int autok_param_apply(struct msdc_host *host, u8 *autok_tune_res)
{
	unsigned int i = 0;
	unsigned int value = 0;

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		value = (u8) autok_tune_res[i];
		autok_adjust_param(host, i, &value, AUTOK_WRITE);
	}

	return 0;
}

static void autok_tuning_parameter_init(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;
	/* void __iomem *base = host->base; */

	/* MSDC_SET_FIELD(MSDC_PATCH_BIT2, 7<<29, 2); */
	/* MSDC_SET_FIELD(MSDC_PATCH_BIT2, 7<<16, 4); */

	ret = autok_param_apply(host, res);
}

static int autok_result_dump(struct msdc_host *host, u8 *autok_tune_res)
{
	ATK_ERR("[AUTOK]CMD [EDGE:%d DLY1:%d DLY2:%d ]\n",
		autok_tune_res[0], autok_tune_res[4], autok_tune_res[6]);
	ATK_ERR("[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\n",
		autok_tune_res[1], autok_tune_res[2], autok_tune_res[3]);
	ATK_ERR("[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d ]\n",
		autok_tune_res[12], autok_tune_res[8], autok_tune_res[10]);
	ATK_ERR("[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\n",
		autok_tune_res[13], autok_tune_res[15], autok_tune_res[17]);

	return 0;
}

/* online tuning for latch ck */
int autok_execute_tuning_latch_ck(struct msdc_host *host, unsigned int opcode,
	unsigned int latch_ck_initail_value)
{
	unsigned int ret = 0;
	unsigned int j, k;
	void __iomem *base = host->base;
	unsigned int tune_time;

	writel(0xffffffff, base + MSDC_INT);
	tune_time = AUTOK_LATCH_CK_SDIO_TUNE_TIMES;
	for (j = latch_ck_initail_value; j < 8;
	     j += (host->src_clk_freq / host->sclk)) {
		host->tune_latch_ck_cnt = 0;
		msdc_clear_fifo();
		sdr_set_field(base + MSDC_PATCH_BIT0,
			      MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
		for (k = 0; k < tune_time; k++) {
			if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
				switch (k) {
				case 0:
					host->tune_latch_ck_cnt = 1;
					break;
				default:
					host->tune_latch_ck_cnt = k;
					break;
				}
			} else if (opcode == MMC_SEND_TUNING_BLOCK) {
				switch (k) {
				case 0:
				case 1:
				case 2:
					host->tune_latch_ck_cnt = 1;
					break;
				default:
					host->tune_latch_ck_cnt = k - 1;
					break;
				}
			} else if (opcode == MMC_SEND_EXT_CSD) {
				host->tune_latch_ck_cnt = k + 1;
			} else
				host->tune_latch_ck_cnt++;
			ret = autok_send_tune_cmd(host, opcode, TUNE_LATCH_CK);
			if ((ret &
			     (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				ATK_ERR("[AUTOK]CMD Fail when tune LATCH CK\n");
				break;
			} else if ((ret &
				    (E_RESULT_DAT_CRC |
				     E_RESULT_DAT_TMO)) != 0) {
				ATK_ERR("[AUTOK]Tune LATCH_CK error %d\r\n", j);
				break;
			}
		}
		if (ret == 0) {
			sdr_set_field(base + MSDC_PATCH_BIT0,
				      MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
			break;
		}
	}
	host->tune_latch_ck_cnt = 0;
	return (j >= 8) ? 0 : j;
}

/*
 ******************************************************
 * Function: msdc_autok_adjust_paddly                 *
 * Param : value - delay cnt from 0 to 63             *
 *         pad_sel - 0 for cmd pad and 1 for data pad *
 ******************************************************
 */
#define CMD_PAD_RDLY 0
#define DAT_PAD_RDLY 1
#define DS_PAD_RDLY 2
static void msdc_autok_adjust_paddly(struct msdc_host *host,
				     unsigned int *value,
				     unsigned int pad_sel)
{
	unsigned int uCfgL = 0;
	unsigned int uCfgLSel = 0;
	unsigned int uCfgH = 0;
	unsigned int uCfgHSel = 0;
	unsigned int dly_cnt = *value;

	uCfgL = (dly_cnt > 31) ? (31) : dly_cnt;
	uCfgH = (dly_cnt > 31) ? (dly_cnt - 32) : 0;

	uCfgLSel = (uCfgL > 0) ? 1 : 0;
	uCfgHSel = (uCfgH > 0) ? 1 : 0;
	switch (pad_sel) {
	case CMD_PAD_RDLY:
		autok_adjust_param(host, CMD_RD_D_DLY1, &uCfgL, AUTOK_WRITE);
		autok_adjust_param(host, CMD_RD_D_DLY2, &uCfgH, AUTOK_WRITE);

		autok_adjust_param(host, CMD_RD_D_DLY1_SEL,
				   &uCfgLSel, AUTOK_WRITE);
		autok_adjust_param(host, CMD_RD_D_DLY2_SEL,
				   &uCfgHSel, AUTOK_WRITE);
		break;
	case DAT_PAD_RDLY:
		autok_adjust_param(host, DAT_RD_D_DLY1, &uCfgL, AUTOK_WRITE);
		autok_adjust_param(host, DAT_RD_D_DLY2, &uCfgH, AUTOK_WRITE);

		autok_adjust_param(host, DAT_RD_D_DLY1_SEL,
				   &uCfgLSel, AUTOK_WRITE);
		autok_adjust_param(host, DAT_RD_D_DLY2_SEL,
				   &uCfgHSel, AUTOK_WRITE);
		break;
	case DS_PAD_RDLY:
		autok_adjust_param(host, EMMC50_DS_Z_DLY1, &uCfgL, AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DS_Z_DLY2, &uCfgH, AUTOK_WRITE);

		autok_adjust_param(host, EMMC50_DS_Z_DLY1_SEL,
				   &uCfgLSel, AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DS_Z_DLY2_SEL,
				   &uCfgHSel, AUTOK_WRITE);
		break;
	}
}

static void autok_paddly_update(unsigned int pad_sel,
				unsigned int dly_cnt,
				u8 *autok_tune_res)
{
	unsigned int uCfgL = 0;
	unsigned int uCfgLSel = 0;
	unsigned int uCfgH = 0;
	unsigned int uCfgHSel = 0;

	uCfgL = (dly_cnt > 31) ? (31) : dly_cnt;
	uCfgH = (dly_cnt > 31) ? (dly_cnt - 32) : 0;

	uCfgLSel = (uCfgL > 0) ? 1 : 0;
	uCfgHSel = (uCfgH > 0) ? 1 : 0;
	switch (pad_sel) {
	case CMD_PAD_RDLY:
		autok_param_update(CMD_RD_D_DLY1, uCfgL, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2, uCfgH, autok_tune_res);

		autok_param_update(CMD_RD_D_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	case DAT_PAD_RDLY:
		autok_param_update(DAT_RD_D_DLY1, uCfgL, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2, uCfgH, autok_tune_res);

		autok_param_update(DAT_RD_D_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	case DS_PAD_RDLY:
		autok_param_update(EMMC50_DS_Z_DLY1, uCfgL, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2, uCfgH, autok_tune_res);

		autok_param_update(EMMC50_DS_Z_DLY1_SEL,
				   uCfgLSel, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2_SEL,
				   uCfgHSel, autok_tune_res);
		break;
	}
}

/*
 ******************************************************
 * Exectue tuning IF Implenment                       *
 ******************************************************
 */
static int autok_write_param(struct msdc_host *host,
			     enum AUTOK_PARAM param, u32 value)
{
	autok_adjust_param(host, param, &value, AUTOK_WRITE);

	return 0;
}

static int autok_path_sel(struct msdc_host *host)
{
	void __iomem *base = host->base;

	autok_write_param(host, READ_DATA_SMPL_SEL, 0);
	autok_write_param(host, WRITE_DATA_SMPL_SEL, 0);

	/* clK tune all data Line share dly */
	autok_write_param(host, DATA_DLYLINE_SEL, 0);

	/* data tune mode select */
#if defined(CHIP_DENALI_3_DAT_TUNE)
	autok_write_param(host, MSDC_DAT_TUNE_SEL, 1);
#else
	autok_write_param(host, MSDC_DAT_TUNE_SEL, 0);
#endif
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 1);
	autok_write_param(host, MSDC_RESP_ASYNC_FIFO_SEL, 0);

	/* eMMC50 Function Mux */
	/* write path switch to emmc45 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 0);

	/* response path switch to emmc45 */
	autok_write_param(host, EMMC50_CMD_MUX_EN, 0);
	autok_write_param(host, EMMC50_WDATA_EDGE, 0);

	/* Common Setting Config */
	autok_write_param(host, CKGEN_MSDC_DLY_SEL, AUTOK_CKGEN_VALUE);
	autok_write_param(host, CMD_RSP_TA_CNTR, AUTOK_CMD_TA_VALUE);
	autok_write_param(host, WRDAT_CRCS_TA_CNTR, AUTOK_CRC_TA_VALUE);

	sdr_set_field(base + MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA,
		      AUTOK_BUSY_MA_VALUE);
	sdr_set_field(base + MSDC_PATCH_BIT1, MSDC_PB1_GET_CRC_MA,
		      AUTOK_CRC_MA_VALUE);

	return 0;
}

static int autok_init_sdr104(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	/* LATCH_TA_EN Config for WCRC Path non_HS400 */
	sdr_set_field(base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		      AUTOK_CRC_LATCH_EN_NON_HS400_VALUE);

	/* LATCH_TA_EN Config for CMD Path non_HS400 */
	sdr_set_field(base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
		      AUTOK_CMD_LATCH_EN_NON_HS400_VALUE);

	return 0;
}

/* online tuning for SDIO/SD */
static int execute_online_tuning(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
	unsigned int opcode = MMC_SEND_TUNING_BLOCK;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];

	autok_init_sdr104(host);
	memset((void *)p_autok_tune_res, 0,
	       sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&
			  (uCmdDatInfo.scan_info[uCmdEdge]);
		autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host,
							  opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO |
					    E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		ATK_DBG(ATK_RES, "[AUTOK]CMD %d \t %d \t %s\r\n",
			       uCmdEdge, score, tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
			host->autok_error = AUTOK_FAIL;
			msdc_dump_all_register(host);
			return AUTOK_FAIL;
		}
		#if 0
		ATK_DBG(ATK_RES,
		"[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\n",
		uCmdEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

		for (i = 0; i < BD_MAX_CNT; i++) {
			ATK_DBG(ATK_RES,
		"[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\n",
		i, pBdInfo->bd_info[i].Bound_Start,
		pBdInfo->bd_info[i].Bound_End, pBdInfo->bd_info[i].Bound_width,
		pBdInfo->bd_info[i].is_fullbound);
		}
		#endif

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel,
				   p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt,
				    p_autok_tune_res);
	} else {
		ATK_DBG(ATK_RES, "[AUTOK]======Analysis Fail!!=======\n");
		host->autok_error = AUTOK_FAIL;
		msdc_dump_all_register(host);
		return AUTOK_FAIL;
	}

	/* Step2 : Tuning Data Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uDatEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&
			  (uCmdDatInfo.scan_info[uDatEdge]);
		autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode,
							  TUNE_DATA);
				if ((ret & (E_RESULT_CMD_TMO |
					    E_RESULT_RSP_CRC)) != 0) {
					ATK_ERR("[AUTOK]Tune read CMD Fail\n");
					host->autok_error = -1;
					return -1;
				} else if ((ret & (E_RESULT_DAT_CRC |
						   E_RESULT_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		ATK_DBG(ATK_RES, "[AUTOK]DAT %d \t %d \t %s\r\n",
			uDatEdge, score, tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
			host->autok_error = AUTOK_FAIL;
			msdc_dump_all_register(host);
			return AUTOK_FAIL;
		}
		#if 0
		ATK_DBG(ATK_RES,
		"[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\n",
		uDatEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

		for (i = 0; i < BD_MAX_CNT; i++) {
			ATK_DBG(ATK_RES,
		"[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n",
		i, pBdInfo->bd_info[i].Bound_Start,
		pBdInfo->bd_info[i].Bound_End, pBdInfo->bd_info[i].Bound_width,
		pBdInfo->bd_info[i].is_fullbound);
		}
		#endif

		uDatEdge ^= 0x1;
	} while (uDatEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(RD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel,
				   p_autok_tune_res);
		autok_paddly_update(DAT_PAD_RDLY, uCmdDatInfo.opt_dly_cnt,
				    p_autok_tune_res);
		autok_param_update(WD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel,
				   p_autok_tune_res);
	} else {
		ATK_DBG(ATK_RES, "[AUTOK][Error]=====Analysis Fail!!=======\n");
		msdc_dump_all_register(host);
		host->autok_error = AUTOK_FAIL;
		return AUTOK_FAIL;
	}

	autok_tuning_parameter_init(host, p_autok_tune_res);

	/* Step3 : Tuning LATCH CK */
	p_autok_tune_res[INT_DAT_LATCH_CK] = autok_execute_tuning_latch_ck(host,
				opcode, p_autok_tune_res[INT_DAT_LATCH_CK]);

	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}
	host->autok_error = 0;

	return 0;
}

static int autok_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	unsigned int retry_cnt = 3;
	void __iomem *base = host->base;

	do_gettimeofday(&tm_s);

	do {
		autok_msdc_reset();
		msdc_clear_fifo();
		int_en = readl(base + MSDC_INTEN);
		writel(0, base + MSDC_INTEN);
		sdr_get_field(base + MSDC_CFG, MSDC_CFG_CKPDN, &clk_pwdn);
		sdr_set_field(base + MSDC_CFG, MSDC_CFG_CKPDN, 1);
		ret = execute_online_tuning(host, res);
		if (!ret)
			break;
		retry_cnt--;
	} while (retry_cnt);

	autok_msdc_reset();
	msdc_clear_fifo();
	writel(0xffffffff, base + MSDC_INT);
	writel(int_en, base + MSDC_INTEN);
	sdr_set_field(base + MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 +
		 (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	ATK_ERR("[AUTOK]=========Time Cost:%d ms========\n", tm_val);

	return ret;
}

static void msdc_dump_all_register(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int i;
	unsigned int left_cnt;
	unsigned int byte16_align_cnt;

	byte16_align_cnt = MAX_REGISTER_ADDR / 16;
	for (i = 0; i < byte16_align_cnt; i++)
		pr_info("SDIO reg[%.2x]=0x%.8x reg[%.2x]=0x%.8x reg[%.2x]=0x%.8x reg[%.2x]=0x%.8x\n",
			i * 16, readl(base + i * 16),
			i * 16 + 4, readl(base + i * 16 + 4),
			i * 16 + 8, readl(base + i * 16 + 8),
			i * 16 + 12, readl(base + i * 16 + 12));

	left_cnt = (MAX_REGISTER_ADDR - byte16_align_cnt * 16) / 4 + 1;
	for (i = 0; i < left_cnt; i++)
		pr_info("SDIO reg[%.2x]=0x%.8x\n",
		       byte16_align_cnt * 16 + i * 4,
		       readl(base + byte16_align_cnt * 16 + i * 4));
}

static void msdc_dump_register(struct msdc_host *host)
{
	void __iomem *base = host->base;

	pr_info("SDIO MSDC_CFG=0x%.8x\n", readl(base + MSDC_CFG));
	pr_info("SDIO MSDC_IOCON=0x%.8x\n", readl(base + MSDC_IOCON));
	pr_info("SDIO MSDC_PATCH_BIT0=0x%.8x\n", readl(base + MSDC_PATCH_BIT0));
	pr_info("SDIO MSDC_PATCH_BIT1=0x%.8x\n", readl(base + MSDC_PATCH_BIT1));
	pr_info("SDIO MSDC_PATCH_BIT2=0x%.8x\n", readl(base + MSDC_PATCH_BIT2));
	pr_info("SDIO MSDC_PAD_TUNE0=0x%.8x\n", readl(base + MSDC_PAD_TUNE0));
	pr_info("SDIO MSDC_PAD_TUNE1=0x%.8x\n", readl(base + MSDC_PAD_TUNE1));
}

static int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (host->autok_done) {
		autok_init_sdr104(host);
		autok_param_apply(host, sdio_autok_res);
	} else {
		autok_execute_tuning(host, sdio_autok_res);
		host->autok_done = true;
	}

	msdc_dump_register(host);
	return 0;
}

static void msdc_hw_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	sdr_set_bits(host->base + EMMC_IOCON, 1);
	udelay(10); /* 10us is enough */
	sdr_clr_bits(host->base + EMMC_IOCON, 1);
}

/*
 * msdc_recheck_sdio_irq - recheck whether the SDIO IRQ is lost
 * @host: The host to check.
 *
 * Host controller may lost interrupt in some special case.
 * Add sdio IRQ recheck mechanism to make sure all interrupts
 * can be processed immediately
 *
 */
#ifndef SUPPORT_LEGACY_SDIO
static void msdc_recheck_sdio_irq(struct msdc_host *host)
{
	u32 reg_int, reg_ps, reg_inten;

	reg_inten = readl(host->base + MSDC_INTEN);
	if (host->clock_on && (host->mmc->caps & MMC_CAP_SDIO_IRQ) &&
			(reg_inten & MSDC_INTEN_SDIOIRQ) &&
			host->irq_thread_alive) {
		reg_int = readl(host->base + MSDC_INT);
		reg_ps  = readl(host->base + MSDC_PS);
		if (!((reg_int & MSDC_INT_SDIOIRQ) || (reg_ps & MSDC_PS_DATA1)))
			mmc_signal_sdio_irq(host->mmc);
	}
}
#endif

static void msdc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	unsigned long flags;
	struct msdc_host *host = mmc_priv(mmc);

	host->irq_thread_alive = true;

#ifdef SUPPORT_LEGACY_SDIO
	if (host->cap_eirq) {
		if (enable)
			host->enable_sdio_eirq(); /* combo_sdio_enable_eirq */
		else
			host->disable_sdio_eirq(); /* combo_sdio_disable_eirq */
	}
	return;
#endif

	if (enable) {
		pm_runtime_get_sync(host->dev);

		spin_lock_irqsave(&host->irqlock, flags);
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
		sdr_set_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		spin_unlock_irqrestore(&host->irqlock, flags);
		pm_runtime_mark_last_busy(host->dev);
		pm_runtime_put_autosuspend(host->dev);
	} else {
		spin_lock_irqsave(&host->irqlock, flags);
		sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		/*
		 * if no msdc_recheck_sdio_irq(), then
		 * no race condition of disable_irq
		 * twice and only enable_irq once time.
		 */
		if (likely(host->sdio_irq_cnt > 0)) {
			disable_irq_nosync(host->eint_irq);
			host->sdio_irq_cnt--;
			if (mmc->card && (mmc->card->cccr.eai == 0))
				pm_runtime_put_noidle(host->dev);
		}
		spin_unlock_irqrestore(&host->irqlock, flags);
	}
}

static struct mmc_host_ops mt_msdc_ops = {
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.get_ro = mmc_gpio_get_ro,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
	.card_busy = msdc_card_busy,
	.execute_tuning = msdc_execute_tuning,
	.hw_reset = msdc_hw_reset,
	.enable_sdio_irq = msdc_enable_sdio_irq,
};

#ifndef SUPPORT_LEGACY_SDIO
static irqreturn_t sdio_eint_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *)dev_id;

	mmc_signal_sdio_irq(host->mmc);

	return IRQ_HANDLED;
}

static int request_dat1_eint_irq(struct msdc_host *host)
{
	struct gpio_desc *desc;
	int ret = 0;
	int irq;

	desc = devm_gpiod_get_index(host->dev, "eint", 0, GPIOD_IN);
	if (IS_ERR(desc))
		return PTR_ERR(desc);

	irq = gpiod_to_irq(desc);
	if (irq >= 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(host->dev, irq,
				NULL, sdio_eint_irq,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"sdio-eint", host);
	} else {
		ret = irq;
	}

	host->eint_irq = irq;
	return ret;
}

#else
/* For backward compatible, remove later */
int wait_sdio_autok_ready(void *data)
{
	return 0;
}
EXPORT_SYMBOL(wait_sdio_autok_ready);

static void register_legacy_sdio_apis(struct msdc_host *host)
{
	host->request_sdio_eirq = mt_sdio_ops[SDIO_USE_PORT].sdio_request_eirq;
	host->enable_sdio_eirq = mt_sdio_ops[SDIO_USE_PORT].sdio_enable_eirq;
	host->disable_sdio_eirq = mt_sdio_ops[SDIO_USE_PORT].sdio_disable_eirq;
	host->register_pm = mt_sdio_ops[SDIO_USE_PORT].sdio_register_pm;
}

static void msdc_eirq_sdio(void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;

	mmc_signal_sdio_irq(host->mmc);
}

static void msdc_pm(pm_message_t state, void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;

	int evt = state.event;

	if ((evt == PM_EVENT_SUSPEND) || (evt == PM_EVENT_USER_SUSPEND)) {
		if (host->suspend != 0)
			return;

		pr_info("msdc%d -> %s Suspend\n", SDIO_USE_PORT,
			evt == PM_EVENT_SUSPEND ? "PM" : "USR");
		host->suspend = 1;
		host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
		mmc_remove_host(host->mmc);
	}

	if ((evt == PM_EVENT_RESUME) || (evt == PM_EVENT_USER_RESUME)) {
		if (host->suspend == 0)
			return;

		pr_info("msdc%d -> %s Resume\n", SDIO_USE_PORT,
			evt == PM_EVENT_RESUME ? "PM" : "USR");
		host->suspend = 0;
		host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
		host->mmc->rescan_entered = 0;
		mmc_add_host(host->mmc);
	}
}
#endif

void sdio_set_card_clkpd(int on)
{
	if (!on)
		sdr_clr_bits(sdio_host->base + MSDC_CFG,
			 MSDC_CFG_CKPDN);
	else
		sdr_set_bits(sdio_host->base + MSDC_CFG,
			 MSDC_CFG_CKPDN);
}
EXPORT_SYMBOL(sdio_set_card_clkpd);

static const struct mt81xx_sdio_compatible mt8183_compat = {
	.v3_plus = true,
	.top_reg = true,
};

static const struct mt81xx_sdio_compatible mt8167_compat = {
	.v3_plus = false,
	.top_reg = false,
};

static const struct mt81xx_sdio_compatible mt2712_compat = {
	.v3_plus = false,
	.top_reg = false,
};

static const struct mt81xx_sdio_compatible mt8695_compat = {
	.v3_plus = true,
	.top_reg = false,
};

static const struct of_device_id msdc_of_ids[] = {
	{ .compatible = "mediatek,mt8183-sdio", .data = &mt8183_compat},
	{ .compatible = "mediatek,mt8167-sdio", .data = &mt8167_compat},
	{ .compatible = "mediatek,mt2712-sdio", .data = &mt2712_compat},
	{ .compatible = "mediatek,mt8695-sdio", .data = &mt8695_compat},
	{}
};

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct resource *res;
	struct resource *res_top;
	const struct of_device_id *of_id;
	int ret;
	u32 val;

	if (!pdev->dev.of_node) {
		dev_info(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}

	of_id = of_match_node(msdc_of_ids, pdev->dev.of_node);
	if (!of_id)
		return -EINVAL;
	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	ret = mmc_of_parse(mmc);
	if (ret)
		goto host_free;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto host_free;
	}

	host->dev_comp = of_id->data;
	if (host->dev_comp->top_reg) {
		res_top = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		host->base_top = devm_ioremap_resource(&pdev->dev, res_top);
		if (IS_ERR(host->base_top)) {
			ret = PTR_ERR(host->base_top);
			goto host_free;
		}
	} else {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		host->infra_reset = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(host->infra_reset)) {
			ret = PTR_ERR(host->infra_reset);
			goto host_free;
		}

		if (!of_property_read_u32(pdev->dev.of_node,
			"module_reset_bit", &host->module_reset_bit))
			dev_dbg(&pdev->dev, "module_reset_bit: %x\n",
				 host->module_reset_bit);
	}

	ret = mmc_regulator_get_supply(mmc);
	if (ret == -EPROBE_DEFER)
		goto host_free;

	host->src_clk = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(host->src_clk)) {
		ret = PTR_ERR(host->src_clk);
		goto host_free;
	}

	host->h_clk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(host->h_clk)) {
		ret = PTR_ERR(host->h_clk);
		goto host_free;
	}

	host->src_clk_cg = devm_clk_get(&pdev->dev, "source_cg");
	if (IS_ERR(host->src_clk_cg))
		host->src_clk_cg = NULL;

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = -EINVAL;
		goto host_free;
	}

	host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(host->pinctrl)) {
		ret = PTR_ERR(host->pinctrl);
		dev_info(&pdev->dev, "Cannot find pinctrl!\n");
		goto host_free;
	}

	host->pins_default = pinctrl_lookup_state(host->pinctrl, "default");
	if (IS_ERR(host->pins_default)) {
		ret = PTR_ERR(host->pins_default);
		dev_info(&pdev->dev, "Cannot find pinctrl default!\n");
		goto host_free;
	}

	host->pins_uhs = pinctrl_lookup_state(host->pinctrl, "state_uhs");
	if (IS_ERR(host->pins_uhs)) {
		ret = PTR_ERR(host->pins_uhs);
		dev_info(&pdev->dev, "Cannot ... find pinctrl uhs!\n");
		goto host_free;
	}
	pinctrl_select_state(host->pinctrl, host->pins_uhs);

	host->pins_dat1 = pinctrl_lookup_state(host->pinctrl, "state_dat1");
	if (IS_ERR(host->pins_dat1)) {
		ret = PTR_ERR(host->pins_dat1);
		dev_info(&pdev->dev, "Cannot find pinctrl dat1!\n");
		goto host_free;
	}

	host->pins_dat1_eint = pinctrl_lookup_state(host->pinctrl,
		"state_eint");
	if (IS_ERR(host->pins_dat1_eint)) {
		ret = PTR_ERR(host->pins_dat1_eint);
		dev_info(&pdev->dev, "Cannot find pinctrl dat1 eint!\n");
		goto host_free;
	}

	if (!of_property_read_u32(pdev->dev.of_node,
				"hs400-ds-delay", &host->hs400_ds_delay))
		dev_dbg(&pdev->dev, "hs400-ds-delay: %x\n",
			host->hs400_ds_delay);

#ifdef SUPPORT_LEGACY_SDIO
	if (of_property_read_bool(pdev->dev.of_node, "cap-sdio-irq"))
		host->cap_eirq = false;
	else
		host->cap_eirq = true;
#endif

	host->dev = &pdev->dev;
	host->mmc = mmc;
	host->src_clk_freq = clk_get_rate(host->src_clk);
	if (host->src_clk_freq > 200000000)
		host->src_clk_freq = 200000000;
	/* Set host parameters to mmc */
#ifdef SUPPORT_LEGACY_SDIO
	if (host->cap_eirq)
		mmc->caps |= MMC_CAP_SDIO_IRQ;
#endif
	mmc->ops = &mt_msdc_ops;
	mmc->f_min = host->src_clk_freq / (4 * 255);
	mmc->ocr_avail = MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 |
			 MMC_VDD_31_32 | MMC_VDD_32_33;

	mmc->caps |= MMC_CAP_ERASE | MMC_CAP_CMD23;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_BD_NUM;
	mmc->max_seg_size = BDMA_DESC_BUFLEN;
	mmc->max_blk_size = 2048;
	mmc->max_req_size = 512 * 1024;
	mmc->max_blk_count = mmc->max_req_size / 512;
	host->dma_mask = DMA_BIT_MASK(32);
	mmc_dev(mmc)->dma_mask = &host->dma_mask;

	host->timeout_clks = 3 * 1048576;
	host->irq_thread_alive = false;
	host->dma.gpd = dma_alloc_coherent(&pdev->dev,
				2 * sizeof(struct mt_gpdma_desc),
				&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd = dma_alloc_coherent(&pdev->dev,
				MAX_BD_NUM * sizeof(struct mt_bdma_desc),
				&host->dma.bd_addr, GFP_KERNEL);
	if (!host->dma.gpd || !host->dma.bd) {
		ret = -ENOMEM;
		goto release_mem;
	}
	msdc_init_gpd_bd(host, &host->dma);
	INIT_DELAYED_WORK(&host->req_timeout, msdc_request_timeout);
	spin_lock_init(&host->lock);
	spin_lock_init(&host->irqlock);

	platform_set_drvdata(pdev, mmc);
	msdc_ungate_clock(host);

	if (!host->dev_comp->top_reg) {
		/* just test module reset func */
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_MODE);
		/* do MSDC module reset */
		val = readl(host->infra_reset);
		pr_debug("init 0x10001030: 0x%x, MSDC_CFG: 0x%x\n",
				val, readl(host->base + MSDC_CFG));
		writel(0x1 << host->module_reset_bit, host->infra_reset);
		val = readl(host->infra_reset);
		udelay(1);
		pr_debug("msdc module resetting 0x10001030: 0x%x\n", val);
		writel(0x1 << host->module_reset_bit, host->infra_reset + 0x04);
		udelay(1);
		val = readl(host->infra_reset);
		pr_info("msdc module reset done 0x10001030: 0x%x, MSDC_CFG: 0x%x\n",
				val, readl(host->base + MSDC_CFG));
	}

	msdc_init_hw(host);

	ret = devm_request_irq(&pdev->dev, host->irq, msdc_irq,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, pdev->name, host);
	if (ret)
		goto release;

#ifndef SUPPORT_LEGACY_SDIO
	ret = request_dat1_eint_irq(host);
	if (ret) {
		dev_info(host->dev, "failed to register data1 eint irq!\n");
		goto release;
	}

	pinctrl_select_state(host->pinctrl, host->pins_dat1);
#else
	host->suspend = 0;

	register_legacy_sdio_apis(host);
	if (host->request_sdio_eirq)
		host->request_sdio_eirq(msdc_eirq_sdio, (void *)host);
	if (host->register_pm) {
		host->register_pm(msdc_pm, (void *)host);

		/* pm not controlled by system but by client. */
		mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
	}
#endif

	pm_runtime_set_active(host->dev);
	pm_runtime_set_autosuspend_delay(host->dev, MTK_MMC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(host->dev);
	pm_runtime_enable(host->dev);

	if (!host->dev_comp->top_reg)
		mmc->caps2 |= MMC_CAP2_NO_PRESCAN_POWERUP;
	host->mmc->caps |= MMC_CAP_NONREMOVABLE;
	host->mmc->pm_caps |= MMC_PM_KEEP_POWER;
	host->mmc->pm_flags |= MMC_PM_KEEP_POWER;

	ret = mmc_add_host(mmc);
	pr_info("%s: add new sdio_host %s, index=%d, ret=%d\n", __func__,
		mmc_hostname(host->mmc), mmc->index, ret);

	sdio_host = host;
	if (ret)
		goto end;

	return 0;
end:
	pm_runtime_disable(host->dev);
release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
	msdc_gate_clock(host);
release_mem:
	if (host->dma.gpd)
		dma_free_coherent(&pdev->dev,
			2 * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	if (host->dma.bd)
		dma_free_coherent(&pdev->dev,
			MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);
host_free:
	mmc_free_host(mmc);

	return ret;
}

static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;

	mmc = platform_get_drvdata(pdev);
	host = mmc_priv(mmc);

	pm_runtime_get_sync(host->dev);

	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(host->mmc);
	msdc_deinit_hw(host);
	msdc_gate_clock(host);

	if (host->mmc->caps & MMC_CAP_SDIO_IRQ)
		pm_runtime_put_sync(host->dev);

	pm_runtime_disable(host->dev);
	pm_runtime_put_noidle(host->dev);
	dma_free_coherent(&pdev->dev,
			sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(&pdev->dev, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	mmc_free_host(host->mmc);

	return 0;
}

#ifdef CONFIG_PM
static void msdc_save_reg(struct msdc_host *host)
{
	host->save_para.msdc_cfg = readl(host->base + MSDC_CFG);
	host->save_para.iocon = readl(host->base + MSDC_IOCON);
	host->save_para.sdc_cfg = readl(host->base + SDC_CFG);
	host->save_para.pad_tune0 = readl(host->base + MSDC_PAD_TUNE0);
	host->save_para.pad_tune1 = readl(host->base + MSDC_PAD_TUNE1);
	host->save_para.patch_bit0 = readl(host->base + MSDC_PATCH_BIT0);
	host->save_para.patch_bit1 = readl(host->base + MSDC_PATCH_BIT1);
	host->save_para.patch_bit2 = readl(host->base + MSDC_PATCH_BIT2);
	host->save_para.pad_ds_tune = readl(host->base + EMMC50_PAD_DS_TUNE);
	host->save_para.emmc50_cfg0 = readl(host->base + EMMC50_CFG0);
	host->save_para.msdc_inten = readl(host->base + MSDC_INTEN);
}

static void msdc_restore_reg(struct msdc_host *host)
{
	writel(host->save_para.msdc_cfg, host->base + MSDC_CFG);
	writel(host->save_para.iocon, host->base + MSDC_IOCON);
	writel(host->save_para.sdc_cfg, host->base + SDC_CFG);
	writel(host->save_para.pad_tune0, host->base + MSDC_PAD_TUNE0);
	writel(host->save_para.pad_tune1, host->base + MSDC_PAD_TUNE1);
	writel(host->save_para.patch_bit0, host->base + MSDC_PATCH_BIT0);
	writel(host->save_para.patch_bit1, host->base + MSDC_PATCH_BIT1);
	writel(host->save_para.patch_bit2, host->base + MSDC_PATCH_BIT2);
	writel(host->save_para.pad_ds_tune, host->base + EMMC50_PAD_DS_TUNE);
	writel(host->save_para.emmc50_cfg0, host->base + EMMC50_CFG0);
	writel(host->save_para.msdc_inten, host->base + MSDC_INTEN);
}

static int msdc_runtime_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

#ifdef SUPPORT_LEGACY_SDIO
	msdc_save_reg(host);
	msdc_gate_clock(host);
	return 0;
#else
	unsigned long flags;

	msdc_save_reg(host);
	disable_irq(host->irq);
	msdc_gate_clock(host);
	pinctrl_select_state(host->pinctrl, host->pins_dat1_eint);
	spin_lock_irqsave(&host->irqlock, flags);
	if (host->sdio_irq_cnt == 0) {
		enable_irq(host->eint_irq);
		enable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt++;
		/*
		 * if SDIO card do not support async irq,
		 * make clk always on.
		 */
		if (mmc->card && (mmc->card->cccr.eai == 0))
			pm_runtime_get_noresume(host->dev);
	}
	spin_unlock_irqrestore(&host->irqlock, flags);
	return 0;
#endif
}

static int msdc_runtime_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

#ifdef SUPPORT_LEGACY_SDIO
	msdc_ungate_clock(host);
	msdc_restore_reg(host);
	return 0;
#else
	unsigned long flags;

	spin_lock_irqsave(&host->irqlock, flags);
	if (host->sdio_irq_cnt > 0) {
		disable_irq_nosync(host->eint_irq);
		disable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt--;
		if (mmc->card && (mmc->card->cccr.eai == 0))
			pm_runtime_put_noidle(host->dev);
	}
	spin_unlock_irqrestore(&host->irqlock, flags);
	pinctrl_select_state(host->pinctrl, host->pins_dat1);
	msdc_ungate_clock(host);
	msdc_restore_reg(host);
	enable_irq(host->irq);
	return 0;
#endif
}
#endif

static const struct dev_pm_ops msdc_dev_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(msdc_runtime_suspend, msdc_runtime_resume, NULL)
};


static struct platform_driver mt_sdio_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.driver = {
		.name = "mtk-sdio",
		.of_match_table = msdc_of_ids,
		.pm = &msdc_dev_pm_ops,
	},
};

module_platform_driver(mt_sdio_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SDIO Driver");

