// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Chaotian.Jing <chaotian.jing@mediatek.com>
 */
#include "mtk-mmc.h"
#include "mtk-mmc-dbg.h"
#include "rpmb-mtk.h"
#include "../core/card.h"
#include <linux/regulator/consumer.h>
#include <mt-plat/dvfsrc-exp.h>
#include <mt-plat/mtk_blocktag.h>

static int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode);

static const struct mtk_mmc_compatible mt8135_compat = {
	.clk_div_bits = 8,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE,
	.async_fifo = false,
	.data_tune = false,
	.busy_check = false,
	.stop_clk_fix = false,
	.enhance_rx = false,
	.support_64g = false,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt8173_compat = {
	.clk_div_bits = 8,
	.recheck_sdio_irq = true,
	.hs400_tune = true,
	.pad_tune_reg = MSDC_PAD_TUNE,
	.async_fifo = false,
	.data_tune = false,
	.busy_check = false,
	.stop_clk_fix = false,
	.enhance_rx = false,
	.support_64g = false,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt8183_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_fix = true,
	.enhance_rx = true,
	.support_64g = true,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt2701_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = false,
	.stop_clk_fix = false,
	.enhance_rx = false,
	.support_64g = false,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt2712_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_fix = true,
	.enhance_rx = true,
	.support_64g = true,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt7622_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_fix = true,
	.enhance_rx = true,
	.support_64g = false,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt8516_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_fix = true,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt7620_compat = {
	.clk_div_bits = 8,
	.recheck_sdio_irq = true,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE,
	.async_fifo = false,
	.data_tune = false,
	.busy_check = false,
	.stop_clk_fix = false,
	.enhance_rx = false,
	.use_internal_cd = true,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible mt6779_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_fix = true,
	.enhance_rx = true,
	.support_64g = true,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible common_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_fix = true,
	.enhance_rx = true,
	.support_64g = true,
	.need_gate_cg = true,
};

static const struct mtk_mmc_compatible common_v2_compat = {
	.clk_div_bits = 12,
	.recheck_sdio_irq = false,
	.hs400_tune = false,
	.pad_tune_reg = MSDC_PAD_TUNE0,
	.async_fifo = true,
	.data_tune = true,
	.busy_check = true,
	.stop_clk_fix = true,
	.enhance_rx = true,
	.support_64g = true,
	.need_gate_cg = false,
};

static const struct of_device_id msdc_of_ids[] = {
	{ .compatible = "mediatek,mt8135-mmc", .data = &mt8135_compat},
	{ .compatible = "mediatek,mt8173-mmc", .data = &mt8173_compat},
	{ .compatible = "mediatek,mt8183-mmc", .data = &mt8183_compat},
	{ .compatible = "mediatek,mt2701-mmc", .data = &mt2701_compat},
	{ .compatible = "mediatek,mt2712-mmc", .data = &mt2712_compat},
	{ .compatible = "mediatek,mt7622-mmc", .data = &mt7622_compat},
	{ .compatible = "mediatek,mt8516-mmc", .data = &mt8516_compat},
	{ .compatible = "mediatek,mt7620-mmc", .data = &mt7620_compat},
	{ .compatible = "mediatek,mt6779-mmc", .data = &mt6779_compat},
	{ .compatible = "mediatek,common-mmc", .data = &common_compat},
	{ .compatible = "mediatek,common-mmc-v2", .data = &common_v2_compat},
	{}
};
MODULE_DEVICE_TABLE(of, msdc_of_ids);

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

static void msdc_cmd_next(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd);
static void __msdc_enable_sdio_irq(struct msdc_host *host, int enb);

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
		bd[j].ptr = lower_32_bits(dma_address);
		if (host->dev_comp->support_64g) {
			bd[j].bd_info &= ~BDMA_DESC_PTR_H4;
			bd[j].bd_info |= (upper_32_bits(dma_address) & 0xf)
					 << 28;
		}

		if (host->dev_comp->support_64g) {
			bd[j].bd_data_len &= ~BDMA_DESC_BUFLEN_EXT;
			bd[j].bd_data_len |= (dma_len & BDMA_DESC_BUFLEN_EXT);
		} else {
			bd[j].bd_data_len &= ~BDMA_DESC_BUFLEN;
			bd[j].bd_data_len |= (dma_len & BDMA_DESC_BUFLEN);
		}

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
	if (host->dev_comp->support_64g)
		sdr_set_field(host->base + DMA_SA_H4BIT, DMA_ADDR_HIGH_4BIT,
			      upper_32_bits(dma->gpd_addr) & 0xf);
	writel(lower_32_bits(dma->gpd_addr), host->base + MSDC_DMA_SA);
}

static void msdc_prepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (!(data->host_cookie & MSDC_PREPARE_FLAG)) {
		data->host_cookie |= MSDC_PREPARE_FLAG;
		data->sg_count = dma_map_sg(host->dev, data->sg, data->sg_len,
					    mmc_get_dma_dir(data));
	}
}

static void msdc_unprepare_data(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (data->host_cookie & MSDC_ASYNC_FLAG)
		return;

	if (data->host_cookie & MSDC_PREPARE_FLAG) {
		dma_unmap_sg(host->dev, data->sg, data->sg_len,
			     mmc_get_dma_dir(data));
		data->host_cookie &= ~MSDC_PREPARE_FLAG;
	}
}

static u64 msdc_timeout_cal(struct msdc_host *host, u64 ns, u64 clks)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u64 timeout, clk_ns;
	u32 mode = 0;

	if (mmc->actual_clock == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000ULL;
		do_div(clk_ns, mmc->actual_clock);
		timeout = ns + clk_ns - 1;
		do_div(timeout, clk_ns);
		timeout += clks;
		/* in 1048576 sclk cycle unit */
		timeout = DIV_ROUND_UP(timeout, (0x1 << 20));
		if (host->dev_comp->clk_div_bits == 8)
			sdr_get_field(host->base + MSDC_CFG,
				      MSDC_CFG_CKMOD, &mode);
		else
			sdr_get_field(host->base + MSDC_CFG,
				      MSDC_CFG_CKMOD_EXTRA, &mode);
		/*DDR mode will double the clk cycles for data timeout */
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
	}
	return timeout;
}

/* clock control primitives */
static void msdc_set_timeout(struct msdc_host *host, u64 ns, u64 clks)
{
	u64 timeout;

	host->timeout_ns = ns;
	host->timeout_clks = clks;

	timeout = msdc_timeout_cal(host, ns, clks);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC,
		      (u32)(timeout > 255 ? 255 : timeout));
}

static void msdc_set_busy_timeout(struct msdc_host *host, u64 ns, u64 clks)
{
	u64 timeout;

	timeout = msdc_timeout_cal(host, ns, clks);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_WRDTOC,
		      (u32)(timeout > 8191 ? 8191 : timeout));
}

static void msdc_gate_clock(struct msdc_host *host)
{
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	clk_bulk_disable_unprepare(MSDC_NR_CLOCKS, host->bulk_clks);
	clk_disable_unprepare(host->src_clk_cg);
	clk_disable_unprepare(host->crypto_cg);
	clk_disable_unprepare(host->src_clk);
	clk_disable_unprepare(host->bus_clk);
	clk_disable_unprepare(host->h_clk);
	clk_disable_unprepare(host->crypto_clk);
#endif
}

static void msdc_ungate_clock(struct msdc_host *host)
{
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int ret;

	clk_prepare_enable(host->crypto_clk);
	clk_prepare_enable(host->h_clk);
	clk_prepare_enable(host->bus_clk);
	clk_prepare_enable(host->src_clk);
	clk_prepare_enable(host->crypto_cg);
	clk_prepare_enable(host->src_clk_cg);
	ret = clk_bulk_prepare_enable(MSDC_NR_CLOCKS, host->bulk_clks);
	if (ret) {
		dev_err(host->dev, "Cannot enable pclk/axi/ahb clock gates\n");
		return;
	}
#endif
	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();
}

static void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
#if !IS_ENABLED(CONFIG_MMC_AUTOK)
	u32 tune_reg = host->dev_comp->pad_tune_reg;
#endif

	if (!hz) {
		dev_dbg(host->dev, "set mclk to 0\n");
		host->mclk = 0;
		mmc->actual_clock = 0;
		return;
	}

	flags = readl(host->base + MSDC_INTEN);
	sdr_clr_bits(host->base + MSDC_INTEN, flags);
	if (host->dev_comp->clk_div_bits == 8)
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_HS400_CK_MODE);
	else
		sdr_clr_bits(host->base + MSDC_CFG,
			     MSDC_CFG_HS400_CK_MODE_EXTRA);
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
			div = (host->src_clk_freq + ((hz << 2) - 1)) / (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
			div = (div >> 1);
		}

		if (timing == MMC_TIMING_MMC_HS400 &&
		    hz >= (host->src_clk_freq >> 1)) {
			if (host->dev_comp->clk_div_bits == 8)
				sdr_set_bits(host->base + MSDC_CFG,
					     MSDC_CFG_HS400_CK_MODE);
			else
				sdr_set_bits(host->base + MSDC_CFG,
					     MSDC_CFG_HS400_CK_MODE_EXTRA);
			sclk = host->src_clk_freq >> 1;
			div = 0; /* div is ignore when bit18 is set */
		}
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	} else if (hz >= host->src_clk_freq) {
		mode = 0x1; /* no divisor */
		div = 0;
		sclk = host->src_clk_freq;
#endif
	} else {
		mode = 0x0; /* use divisor */
		if (hz >= (host->src_clk_freq >> 1)) {
			div = 0; /* mean div = 1/2 */
			sclk = host->src_clk_freq >> 1; /* sclk = clk / 2 */
		} else {
			div = (host->src_clk_freq + ((hz << 2) - 1)) / (hz << 2);
			sclk = (host->src_clk_freq >> 2) / div;
		}
	}
	/*
	 * As src_clk/HCLK use the same bit to gate/ungate,
	 * So if want to only gate src_clk, need gate its parent(mux).
	 */

	if (host->dev_comp->need_gate_cg) {
		if (host->src_clk_cg)
			clk_disable_unprepare(host->src_clk_cg);
		else
			clk_disable_unprepare(clk_get_parent(host->src_clk));
	}
	if (host->dev_comp->clk_div_bits == 8)
		sdr_set_field(host->base + MSDC_CFG,
			      MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			      (mode << 8) | div);
	else
		sdr_set_field(host->base + MSDC_CFG,
			      MSDC_CFG_CKMOD_EXTRA | MSDC_CFG_CKDIV_EXTRA,
			      (mode << 12) | div);

	if (host->dev_comp->need_gate_cg) {
		if (host->src_clk_cg)
			clk_prepare_enable(host->src_clk_cg);
		else
			clk_prepare_enable(clk_get_parent(host->src_clk));
	}

	while (!(readl(host->base + MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();
	if (host->mclk == 0 && (mmc->caps2 & MMC_CAP2_NO_MMC)
		&& mmc->ios.signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
		dev_info(host->dev, "[%s]:enable clk free run 1ms+ for switch to 1.8v\n",
			__func__);
		sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
		usleep_range(1000, 1500);
		sdr_clr_bits(host->base + MSDC_CFG, MSDC_CFG_CKPDN);
	}
	mmc->actual_clock = sclk;
	host->mclk = hz;
	host->timing = timing;
	/* need because clk changed. */
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);
	sdr_set_bits(host->base + MSDC_INTEN, flags);

#if !IS_ENABLED(CONFIG_MMC_AUTOK)
	/*
	 * mmc_select_hs400() will drop to 50Mhz and High speed mode,
	 * tune result of hs200/200Mhz is not suitable for 50Mhz
	 */
	if (mmc->actual_clock <= 52000000) {
		writel(host->def_tune_para.iocon, host->base + MSDC_IOCON);
		if (host->top_base) {
			writel(host->def_tune_para.emmc_top_control,
			       host->top_base + EMMC_TOP_CONTROL);
			writel(host->def_tune_para.emmc_top_cmd,
			       host->top_base + EMMC_TOP_CMD);
		} else {
			writel(host->def_tune_para.pad_tune,
			       host->base + tune_reg);
		}
	} else {
		writel(host->saved_tune_para.iocon, host->base + MSDC_IOCON);
		writel(host->saved_tune_para.pad_cmd_tune,
		       host->base + PAD_CMD_TUNE);
		if (host->top_base) {
			writel(host->saved_tune_para.emmc_top_control,
			       host->top_base + EMMC_TOP_CONTROL);
			writel(host->saved_tune_para.emmc_top_cmd,
			       host->top_base + EMMC_TOP_CMD);
		} else {
			writel(host->saved_tune_para.pad_tune,
			       host->base + tune_reg);
		}
	}

	if (timing == MMC_TIMING_MMC_HS400 &&
	    host->dev_comp->hs400_tune)
		sdr_set_field(host->base + tune_reg,
			      MSDC_PAD_TUNE_CMDRRDLY,
			      host->hs400_cmd_int_delay);
#endif

	dev_info(host->dev, "sclk: %d, timing: %d\n", mmc->actual_clock,
		timing);
}

static inline u32 msdc_cmd_find_resp(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 resp;

	host->use_cmd_intr = false;
	switch (mmc_resp_type(cmd)) {
		/* Actually, R1, R5, R6, R7 are the same */
	case MMC_RSP_R1:
		resp = 0x1;
		break;
	case MMC_RSP_R1B:
		resp = 0x7;
		host->use_cmd_intr = true;
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
	struct mmc_host *mmc = mmc_from_priv(host);
	/* rawcmd :
	 * vol_swt << 30 | auto_cmd << 28 | blklen << 16 | go_irq << 15 |
	 * stop << 14 | rw << 13 | dtype << 11 | rsptyp << 7 | brk << 6 | opcode
	 */
	u32 opcode = cmd->opcode;
	u32 resp = msdc_cmd_find_resp(host, mrq, cmd);
	u32 rawcmd = (opcode & 0x3f) | ((resp & 0x7) << 7);

	host->cmd_rsp = resp;

	if ((opcode == SD_IO_RW_DIRECT && cmd->flags == (unsigned int) -1) ||
	    opcode == MMC_STOP_TRANSMISSION)
		rawcmd |= (0x1 << 14);
	else if (opcode == SD_SWITCH_VOLTAGE)
		rawcmd |= (0x1 << 30);
	else if (opcode == SD_APP_SEND_SCR ||
		 opcode == SD_APP_SEND_NUM_WR_BLKS ||
		 (opcode == SD_SWITCH && mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == SD_APP_SD_STATUS && mmc_cmd_type(cmd) == MMC_CMD_ADTC) ||
		 (opcode == MMC_SEND_EXT_CSD && mmc_cmd_type(cmd) == MMC_CMD_ADTC))
		rawcmd |= (0x1 << 11);

	if (cmd->data) {
		struct mmc_data *data = cmd->data;

		if (mmc_op_multi(opcode)) {
			if (mmc_card_mmc(mmc->card) && mrq->sbc &&
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
	bool read;

	WARN_ON(host->data);
	host->data = data;
	read = data->flags & MMC_DATA_READ;

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	msdc_dma_setup(host, &host->dma, data);
	sdr_set_bits(host->base + MSDC_INTEN, data_ints_mask);
	sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);
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
		if (host->need_tune == TUNE_AUTOK_PASS) {
			host->need_tune = TUNE_NONE;
			host->retune_times = 0;
		}
	} else {
		msdc_reset_hw(host);
		if (events & MSDC_INT_ACMDCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_STOP_EIO;
			host->need_tune = TUNE_CMD_CRC;
		} else if (events & MSDC_INT_ACMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_STOP_TMO;
			host->need_tune = TUNE_CMD_TMO;
		}
		dev_err(host->dev,
			"%s: AUTO_CMD%d arg=%08X; rsp %08X; cmd_error=%d\n",
			__func__, cmd->opcode, cmd->arg, rsp[0], cmd->error);
	}
	return cmd->error;
}

/*
 * msdc_recheck_sdio_irq - recheck whether the SDIO irq is lost
 *
 * Host controller may lost interrupt in some special case.
 * Add SDIO irq recheck mechanism to make sure all interrupts
 * can be processed immediately
 */
static void msdc_recheck_sdio_irq(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u32 reg_int, reg_inten, reg_ps;

	if (mmc->caps & MMC_CAP_SDIO_IRQ) {
		reg_inten = readl(host->base + MSDC_INTEN);
		if (reg_inten & MSDC_INTEN_SDIOIRQ) {
			reg_int = readl(host->base + MSDC_INT);
			reg_ps = readl(host->base + MSDC_PS);
			if (!(reg_int & MSDC_INT_SDIOIRQ ||
			      reg_ps & MSDC_PS_DATA1)) {
				__msdc_enable_sdio_irq(host, 0);
				sdio_signal_irq(mmc);
			}
		}
	}
}

static void msdc_track_cmd_data(struct msdc_host *host,
				struct mmc_command *cmd, struct mmc_data *data)
{
	if (host->error)
		dev_info(host->dev, "%s: cmd=%d arg=%08X; host->error=0x%08X\n",
			__func__, cmd->opcode, cmd->arg, host->error);
}

static void msdc_request_done(struct msdc_host *host, struct mmc_request *mrq)
{
	unsigned long flags;

	/*
	 * No need check the return value of cancel_delayed_work, as only ONE
	 * path will go here!
	 */
	cancel_delayed_work(&host->req_timeout);

	spin_lock_irqsave(&host->lock, flags);
	host->mrq = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	msdc_track_cmd_data(host, mrq->cmd, mrq->data);
	if (mrq->data)
		msdc_unprepare_data(host, mrq);
	if (host->error)
		msdc_reset_hw(host);
	if (mrq->data) {
		mmc_mtk_biolog_transfer_req_compl(mmc_from_priv(host), 0, 0);
		mmc_mtk_biolog_check(mmc_from_priv(host), 0);
	}
	mmc_request_done(mmc_from_priv(host), mrq);
	if (host->dev_comp->recheck_sdio_irq)
		msdc_recheck_sdio_irq(host);
}

/* returns true if command is fully handled; returns false otherwise */
static bool msdc_cmd_done(struct msdc_host *host, int events,
			  struct mmc_request *mrq, struct mmc_command *cmd)
{
	bool done = false;
	bool sbc_error;
	unsigned long flags;
	u32 *rsp;

	if (mrq->sbc && cmd == mrq->cmd &&
	    (events & (MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
				   | MSDC_INT_ACMDTMO)))
		msdc_auto_cmd_done(host, events, mrq->sbc);

	sbc_error = mrq->sbc && mrq->sbc->error;

	if (!sbc_error && !(events & (MSDC_INT_CMDRDY
					| MSDC_INT_RSPCRCERR
					| MSDC_INT_CMDTMO)))
		return done;

	spin_lock_irqsave(&host->lock, flags);
	done = !host->cmd;
	host->cmd = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;
	rsp = cmd->resp;

	sdr_clr_bits(host->base + MSDC_INTEN, cmd_ints_mask);

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
		if (events & MSDC_INT_CMDTMO ||
		    (cmd->opcode != MMC_SEND_TUNING_BLOCK &&
		     cmd->opcode != MMC_SEND_TUNING_BLOCK_HS200))
			/*
			 * should not clear fifo/interrupt as the tune data
			 * may have already come when cmd19/cmd21 gets response
			 * CRC error.
			 */
			msdc_reset_hw(host);
		if (events & MSDC_INT_RSPCRCERR) {
			cmd->error = -EILSEQ;
			host->error |= REQ_CMD_EIO;
			host->need_tune = TUNE_CMD_CRC;
		} else if (events & MSDC_INT_CMDTMO) {
			cmd->error = -ETIMEDOUT;
			host->error |= REQ_CMD_TMO;
			host->need_tune = TUNE_CMD_TMO;
		}
	} else {
		if (host->need_tune == TUNE_AUTOK_PASS) {
			host->need_tune = TUNE_NONE;
			host->retune_times = 0;
		}
	}
	if (cmd->error)
		dev_info(host->dev,
				"%s: cmd=%d arg=%08X; rsp %08X; cmd_error=%d\n",
				__func__, cmd->opcode, cmd->arg, rsp[0],
				cmd->error);

	msdc_cmd_next(host, mrq, cmd);
	return true;
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

	while ((readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) &&
			time_before(jiffies, tmo))
		cpu_relax();
	if (readl(host->base + SDC_STS) & SDC_STS_CMDBUSY) {
		dev_err(host->dev, "CMD bus busy detected\n");
		host->error |= REQ_CMD_BUSY;
		msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
		return false;
	}

	if (mmc_resp_type(cmd) == MMC_RSP_R1B || cmd->data) {
		tmo = jiffies + msecs_to_jiffies(20);
		/* R1B or with data, should check SDCBUSY */
		while ((readl(host->base + SDC_STS) & SDC_STS_SDCBUSY) &&
				time_before(jiffies, tmo))
			cpu_relax();
		if (readl(host->base + SDC_STS) & SDC_STS_SDCBUSY) {
			dev_err(host->dev, "Controller busy detected\n");
			host->error |= REQ_CMD_BUSY;
			msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
			return false;
		}
	}
	return true;
}

static bool msdc_command_resp_polling(struct msdc_host *host,
	struct mmc_request *mrq, struct mmc_command *cmd,
	unsigned long timeout)
{
	bool ret = false;
	unsigned long tmo;
	int events;

	/* polling */
	tmo = jiffies + timeout;
	while (1) {
		events = readl(host->base + MSDC_INT);
		if ((events & cmd_ints_mask) != 0) {
			/* clear all int flag */
			events &= cmd_ints_mask;
			writel(events, host->base + MSDC_INT);
			break;
		}

		if (time_after(jiffies, tmo) &&
			((readl(host->base + MSDC_INT) & cmd_ints_mask) == 0)) {
			dev_info(host->dev, "[%s]: CMD<%d> polling_for_completion timeout ARG<0x%.8x>\n",
				__func__, cmd->opcode, cmd->arg);
			ret = msdc_cmd_done(host, MSDC_INT_CMDTMO, mrq, cmd);
			goto exit;
		}
	}

	if (cmd)
		ret = msdc_cmd_done(host, events, mrq, cmd);

exit:
	return ret;
}

static void msdc_start_command(struct msdc_host *host,
		struct mmc_request *mrq, struct mmc_command *cmd)
{
	u32 rawcmd;
	unsigned long flags;

	WARN_ON(host->cmd);
	host->cmd = cmd;

	mod_delayed_work(system_wq, &host->req_timeout, DAT_TIMEOUT);
	if (!msdc_cmd_is_ready(host, mrq, cmd))
		return;

	if ((readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16 ||
	    readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) {
		dev_err(host->dev, "TX/RX FIFO non-empty before start of IO. Reset\n");
		msdc_reset_hw(host);
	}

	cmd->error = 0;
	rawcmd = msdc_cmd_prepare_raw_cmd(host, mrq, cmd);

	spin_lock_irqsave(&host->lock, flags);
	if (host->use_cmd_intr)
		sdr_set_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	else
		sdr_clr_bits(host->base + MSDC_INTEN, cmd_ints_mask);
	spin_unlock_irqrestore(&host->lock, flags);

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
	else if (cmd == mrq->sbc) {
		msdc_start_command(host, mrq, mrq->cmd);
		if (!host->use_cmd_intr)
			msdc_command_resp_polling(host, mrq,
				mrq->cmd, CMD_TIMEOUT);
	} else if (!cmd->data)
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

	if (!mrq->host)
		mrq->host = mmc;

	if (mrq->data) {
		mmc_mtk_biolog_send_command(0, mrq);
		mmc_mtk_biolog_check(mmc, 1);
	}

	/* if SBC is required, we have HW option and SW option.
	 * if HW option is enabled, and SBC does not have "special" flags,
	 * use HW option,  otherwise use SW option
	 */
	if (mrq->sbc && (!mmc_card_mmc(mmc->card) ||
	    (mrq->sbc->arg & 0xFFFF0000))) {
		msdc_start_command(host, mrq, mrq->sbc);
		if (!host->use_cmd_intr)
			msdc_command_resp_polling(host, mrq,
				mrq->sbc, CMD_TIMEOUT);
	} else {
		msdc_start_command(host, mrq, mrq->cmd);
		if (!host->use_cmd_intr)
			msdc_command_resp_polling(host, mrq,
				mrq->cmd, CMD_TIMEOUT);
	}
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
	    !mrq->sbc) {
		msdc_start_command(host, mrq, mrq->stop);
		if (!host->use_cmd_intr)
			msdc_command_resp_polling(host, mrq,
				mrq->stop, CMD_TIMEOUT);
	} else
		msdc_request_done(host, mrq);
}

static bool msdc_data_xfer_done(struct msdc_host *host, u32 events,
				struct mmc_request *mrq, struct mmc_data *data)
{
	struct mmc_command *stop;
	unsigned long flags;
	bool done;
	unsigned int check_data = events &
	    (MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO
	     | MSDC_INT_DMA_BDCSERR | MSDC_INT_DMA_GPDCSERR
	     | MSDC_INT_DMA_PROTECT);

	spin_lock_irqsave(&host->lock, flags);
	done = !host->data;
	if (check_data)
		host->data = NULL;
	spin_unlock_irqrestore(&host->lock, flags);

	if (done)
		return true;
	stop = data->stop;

	if (check_data || (stop && stop->error)) {
		dev_dbg(host->dev, "DMA status: 0x%8X\n",
				readl(host->base + MSDC_DMA_CFG));
		sdr_set_field(host->base + MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP,
				1);
		while (readl(host->base + MSDC_DMA_CFG) & MSDC_DMA_CFG_STS)
			cpu_relax();
		sdr_clr_bits(host->base + MSDC_INTEN, data_ints_mask);
		dev_dbg(host->dev, "DMA stop\n");

		if ((events & MSDC_INT_XFER_COMPL) && (!stop || !stop->error)) {
			data->bytes_xfered = data->blocks * data->blksz;
			if (host->need_tune == TUNE_AUTOK_PASS) {
				host->need_tune = TUNE_NONE;
				host->retune_times = 0;
			}
		} else {
			dev_dbg(host->dev, "interrupt events: %x\n", events);
			msdc_reset_hw(host);

			if (mrq->data->flags & MMC_DATA_WRITE)
				host->need_tune = TUNE_DATA_WRITE;
			else
				host->need_tune = TUNE_DATA_READ;

			host->error |= REQ_DAT_ERR;
			data->bytes_xfered = 0;

			if (events & MSDC_INT_DATTMO) {
				data->error = -ETIMEDOUT;
				host->data_timeout_cont++;
			} else if (events & MSDC_INT_DATCRCERR) {
				host->data_timeout_cont = 0;
				data->error = -EILSEQ;
			}

			dev_info(host->dev, "%s: cmd=%d; blocks=%d",
				__func__, mrq->cmd->opcode, data->blocks);
			dev_info(host->dev, "data_error=%d xfer_size=%d\n",
				(int)data->error, data->bytes_xfered);
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
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	struct msdc_host *host = mmc_priv(mmc);
	int ret;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		if (ios->signal_voltage != MMC_SIGNAL_VOLTAGE_330 &&
		    ios->signal_voltage != MMC_SIGNAL_VOLTAGE_180) {
			dev_err(host->dev, "Unsupported signal voltage!\n");
			return -EINVAL;
		}

		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret < 0) {
			dev_dbg(host->dev, "Regulator set error %d (%d)\n",
				ret, ios->signal_voltage);
			return ret;
		}

		/* Apply different pinctrl settings for different signal voltage */
		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			pinctrl_select_state(host->pinctrl, host->pins_uhs);
		else
			pinctrl_select_state(host->pinctrl, host->pins_default);
	}
#endif
	return 0;
}

static int msdc_card_busy(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 status = readl(host->base + MSDC_PS);

	/* only check if data0 is low */
	return !(status & BIT(16));
}

static void msdc_request_timeout(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
			req_timeout.work);

	/* simulate HW timeout status */
	dev_err(host->dev, "%s: aborting cmd/data/mrq\n", __func__);
	if (host->mrq) {
		dev_err(host->dev, "%s: aborting mrq=%p cmd=%d\n", __func__,
				host->mrq, host->mrq->cmd->opcode);
		if (host->cmd) {
			dev_err(host->dev, "%s: aborting cmd=%d\n",
					__func__, host->cmd->opcode);
			msdc_cmd_done(host, MSDC_INT_CMDTMO, host->mrq,
					host->cmd);
		} else if (host->data) {
			dev_err(host->dev, "%s: abort data: cmd%d; %d blocks\n",
					__func__, host->mrq->cmd->opcode,
					host->data->blocks);
			msdc_data_xfer_done(host, MSDC_INT_DATTMO, host->mrq,
					host->data);
		}
	}
}

static void __msdc_enable_sdio_irq(struct msdc_host *host, int enb)
{
	if (enb) {
		sdr_set_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
		if (host->dev_comp->recheck_sdio_irq)
			msdc_recheck_sdio_irq(host);
	} else {
		sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INTEN_SDIOIRQ);
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	}
}

static void msdc_enable_sdio_irq(struct mmc_host *mmc, int enb)
{
	unsigned long flags;
	struct msdc_host *host = mmc_priv(mmc);

	spin_lock_irqsave(&host->lock, flags);
	__msdc_enable_sdio_irq(host, enb);
	spin_unlock_irqrestore(&host->lock, flags);

	if (enb)
		pm_runtime_get_noresume(host->dev);
	else
		pm_runtime_put_noidle(host->dev);
}

static irqreturn_t msdc_cmdq_irq(struct msdc_host *host, u32 intsts)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	int cmd_err = 0, dat_err = 0;

	if (intsts & MSDC_INT_RSPCRCERR) {
		cmd_err = -EILSEQ;
		dev_err(host->dev, "%s: CMD CRC ERR", __func__);
	} else if (intsts & MSDC_INT_CMDTMO) {
		cmd_err = -ETIMEDOUT;
		dev_err(host->dev, "%s: CMD TIMEOUT ERR", __func__);
	}

	if (intsts & MSDC_INT_DATCRCERR) {
		dat_err = -EILSEQ;
		dev_err(host->dev, "%s: DATA CRC ERR", __func__);
	} else if (intsts & MSDC_INT_DATTMO) {
		dat_err = -ETIMEDOUT;
		dev_err(host->dev, "%s: DATA TIMEOUT ERR", __func__);
	}

	if (cmd_err || dat_err) {
		dev_err(host->dev, "cmd_err = %d, dat_err =%d, intsts = 0x%x",
			cmd_err, dat_err, intsts);
	}

	return cqhci_irq(mmc, 0, cmd_err, dat_err);
}

static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *) dev_id;
	struct mmc_host *mmc = mmc_from_priv(host);

	while (true) {
		struct mmc_request *mrq;
		struct mmc_command *cmd;
		struct mmc_data *data;
		u32 events, event_mask;

		spin_lock(&host->lock);
		events = readl(host->base + MSDC_INT);
		event_mask = readl(host->base + MSDC_INTEN);
		if ((events & event_mask) & MSDC_INT_SDIOIRQ)
			__msdc_enable_sdio_irq(host, 0);
		/* clear interrupts */
		writel(events & event_mask, host->base + MSDC_INT);

		mrq = host->mrq;
		cmd = host->cmd;
		data = host->data;
		spin_unlock(&host->lock);

		if ((events & event_mask) & MSDC_INT_SDIOIRQ)
			sdio_signal_irq(mmc);

		if ((events & event_mask) & MSDC_INT_CDSC) {
			if (host->internal_cd)
				mmc_detect_change(mmc, msecs_to_jiffies(20));
			events &= ~MSDC_INT_CDSC;
		}

		if (!(events & (event_mask & ~MSDC_INT_SDIOIRQ)))
			break;

		if ((mmc->caps2 & MMC_CAP2_CQE) &&
		    (events & MSDC_INT_CMDQ)) {
			msdc_cmdq_irq(host, events);
			/* clear interrupts */
			writel(events, host->base + MSDC_INT);
			return IRQ_HANDLED;
		}

		if (!mrq) {
			dev_err(host->dev,
				"%s: MRQ=NULL; events=%08X; event_mask=%08X\n",
				__func__, events, event_mask);
			WARN_ON(1);
			break;
		}

		dev_dbg(host->dev, "%s: events=%08X\n", __func__, events);

		if (cmd)
			msdc_cmd_done(host, events, mrq, cmd);
		else if (data)
			msdc_data_xfer_done(host, events, mrq, data);
	}

	return IRQ_HANDLED;
}

static void msdc_init_hw(struct msdc_host *host)
{
	u32 val;
#if !IS_ENABLED(CONFIG_MMC_AUTOK)
	u32 tune_reg = host->dev_comp->pad_tune_reg;
#endif

	if (host->reset) {
		reset_control_assert(host->reset);
		usleep_range(10, 50);
		reset_control_deassert(host->reset);
	}

	/* Configure to MMC/SD mode */
	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_MODE);

	/* Reset */
	msdc_reset_hw(host);

	/* Disable and clear all interrupts */
	writel(0, host->base + MSDC_INTEN);
	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);

	/* Configure card detection */
	if (host->internal_cd) {
		sdr_set_field(host->base + MSDC_PS, MSDC_PS_CDDEBOUNCE,
			      DEFAULT_DEBOUNCE);
		sdr_set_bits(host->base + MSDC_PS, MSDC_PS_CDEN);
		sdr_set_bits(host->base + MSDC_INTEN, MSDC_INTEN_CDSC);
		sdr_set_bits(host->base + SDC_CFG, SDC_CFG_INSWKUP);
	} else {
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_INSWKUP);
		sdr_clr_bits(host->base + MSDC_PS, MSDC_PS_CDEN);
		sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INTEN_CDSC);
	}

#if !IS_ENABLED(CONFIG_MMC_AUTOK)
	if (host->top_base) {
		writel(0, host->top_base + EMMC_TOP_CONTROL);
		writel(0, host->top_base + EMMC_TOP_CMD);
	} else {
		writel(0, host->base + tune_reg);
	}
	writel(0, host->base + MSDC_IOCON);
	sdr_set_field(host->base + MSDC_IOCON, MSDC_IOCON_DDLSEL, 0);
	writel(0x403c0046, host->base + MSDC_PATCH_BIT);
	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_CKGEN_MSDC_DLY_SEL, 1);
	writel(0xffff4089, host->base + MSDC_PATCH_BIT1);
	sdr_set_bits(host->base + EMMC50_CFG0, EMMC50_CFG_CFCSTS_SEL);

	if (host->dev_comp->stop_clk_fix) {
		sdr_set_field(host->base + MSDC_PATCH_BIT1,
			      MSDC_PATCH_BIT1_STOP_DLY, 3);
		sdr_clr_bits(host->base + SDC_FIFO_CFG,
			     SDC_FIFO_CFG_WRVALIDSEL);
		sdr_clr_bits(host->base + SDC_FIFO_CFG,
			     SDC_FIFO_CFG_RDVALIDSEL);
	}

	if (host->dev_comp->busy_check)
		sdr_clr_bits(host->base + MSDC_PATCH_BIT1, (1 << 7));

	if (host->dev_comp->async_fifo) {
		sdr_set_field(host->base + MSDC_PATCH_BIT2,
			      MSDC_PB2_RESPWAIT, 3);
		if (host->dev_comp->enhance_rx) {
			if (host->top_base)
				sdr_set_bits(host->top_base + EMMC_TOP_CONTROL,
					     SDC_RX_ENH_EN);
			else
				sdr_set_bits(host->base + SDC_ADV_CFG0,
					     SDC_RX_ENHANCE_EN);
		} else {
			sdr_set_field(host->base + MSDC_PATCH_BIT2,
				      MSDC_PB2_RESPSTSENSEL, 2);
			sdr_set_field(host->base + MSDC_PATCH_BIT2,
				      MSDC_PB2_CRCSTSENSEL, 2);
		}
		/* use async fifo, then no need tune internal delay */
		sdr_clr_bits(host->base + MSDC_PATCH_BIT2,
			     MSDC_PATCH_BIT2_CFGRESP);
		sdr_set_bits(host->base + MSDC_PATCH_BIT2,
			     MSDC_PATCH_BIT2_CFGCRCSTS);
	}
#endif

	if (host->dev_comp->support_64g)
		sdr_set_bits(host->base + MSDC_PATCH_BIT2,
			     MSDC_PB2_SUPPORT_64G);
#if !IS_ENABLED(CONFIG_MMC_AUTOK)
	if (host->dev_comp->data_tune) {
		if (host->top_base) {
			sdr_set_bits(host->top_base + EMMC_TOP_CONTROL,
				     PAD_DAT_RD_RXDLY_SEL);
			sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL,
				     DATA_K_VALUE_SEL);
			sdr_set_bits(host->top_base + EMMC_TOP_CMD,
				     PAD_CMD_RD_RXDLY_SEL);
		} else {
			sdr_set_bits(host->base + tune_reg,
				     MSDC_PAD_TUNE_RD_SEL |
				     MSDC_PAD_TUNE_CMD_SEL);
		}
	} else {
		/* choose clock tune */
		if (host->top_base)
			sdr_set_bits(host->top_base + EMMC_TOP_CONTROL,
				     PAD_RXDLY_SEL);
		else
			sdr_set_bits(host->base + tune_reg,
				     MSDC_PAD_TUNE_RXDLYSEL);
	}
#endif
	/* Configure to enable SDIO mode.
	 * it's must otherwise sdio cmd5 failed
	 */
	sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIO);

	/* Config SDIO device detect interrupt function */
	sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	sdr_set_bits(host->base + SDC_ADV_CFG0, SDC_DAT1_IRQ_TRIGGER);

	/* Configure to default data timeout */
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 3);

#if !IS_ENABLED(CONFIG_MMC_AUTOK)
	host->def_tune_para.iocon = readl(host->base + MSDC_IOCON);
	host->saved_tune_para.iocon = readl(host->base + MSDC_IOCON);
	if (host->top_base) {
		host->def_tune_para.emmc_top_control =
			readl(host->top_base + EMMC_TOP_CONTROL);
		host->def_tune_para.emmc_top_cmd =
			readl(host->top_base + EMMC_TOP_CMD);
		host->saved_tune_para.emmc_top_control =
			readl(host->top_base + EMMC_TOP_CONTROL);
		host->saved_tune_para.emmc_top_cmd =
			readl(host->top_base + EMMC_TOP_CMD);
	} else {
		host->def_tune_para.pad_tune = readl(host->base + tune_reg);
		host->saved_tune_para.pad_tune = readl(host->base + tune_reg);
	}
#endif

#if IS_ENABLED(CONFIG_MMC_AUTOK)
	/* reset tuning parameter */
	msdc_init_tune_setting(host);
	host->need_tune = TUNE_NONE;
	host->retune_times = 0;
#endif

	dev_info(host->dev, "init hardware done!");
}

static void msdc_deinit_hw(struct msdc_host *host)
{
	u32 val;

	if (host->internal_cd) {
		/* Disabled card-detect */
		sdr_clr_bits(host->base + MSDC_PS, MSDC_PS_CDEN);
		sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_INSWKUP);
	}

	/* Disable and clear all interrupts */
	writel(0, host->base + MSDC_INTEN);

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct mt_gpdma_desc *gpd = dma->gpd;
	struct mt_bdma_desc *bd = dma->bd;
	dma_addr_t dma_addr;
	int i;

	memset(gpd, 0, sizeof(struct mt_gpdma_desc) * 2);

	dma_addr = dma->gpd_addr + sizeof(struct mt_gpdma_desc);
	gpd->gpd_info = GPDMA_DESC_BDP; /* hwo, cs, bd pointer */
	/* gpd->next is must set for desc DMA
	 * That's why must alloc 2 gpd structure.
	 */
	gpd->next = lower_32_bits(dma_addr);
	if (host->dev_comp->support_64g)
		gpd->gpd_info |= (upper_32_bits(dma_addr) & 0xf) << 24;

	dma_addr = dma->bd_addr;
	gpd->ptr = lower_32_bits(dma->bd_addr); /* physical address */
	if (host->dev_comp->support_64g)
		gpd->gpd_info |= (upper_32_bits(dma_addr) & 0xf) << 28;

	memset(bd, 0, sizeof(struct mt_bdma_desc) * MAX_BD_NUM);
	for (i = 0; i < (MAX_BD_NUM - 1); i++) {
		dma_addr = dma->bd_addr + sizeof(*bd) * (i + 1);
		bd[i].next = lower_32_bits(dma_addr);
		if (host->dev_comp->support_64g)
			bd[i].bd_info |= (upper_32_bits(dma_addr) & 0xf) << 24;
	}
}

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
	switch (ios->power_mode) {
	case MMC_POWER_UP:
		msdc_init_hw(host);
		mmc->regulator_enabled = true;
		break;
	case MMC_POWER_ON:
		host->vqmmc_enabled = true;
		break;
	case MMC_POWER_OFF:
		mmc->regulator_enabled = false;
		host->vqmmc_enabled = false;
		break;
	default:
		break;
	}

	if (host->mclk != ios->clock || host->timing != ios->timing)
		msdc_set_mclk(host, ios->timing, ios->clock);
}
#else
static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret;

	msdc_set_buswidth(host, ios->bus_width);

	/* Suspend/Resume will do power off/on */
	switch (ios->power_mode) {
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc)) {
			msdc_init_hw(host);
			ret = mmc_regulator_set_ocr(mmc, mmc->supply.vmmc,
					ios->vdd);
			if (ret) {
				dev_err(host->dev, "Failed to set vmmc power!\n");
				return;
			}
			/* There is a pulse signal in some case,
			 * so add delay to avoid false alarm when vmmc power up
			 */
			mdelay(3);
			devm_regulator_register_notifier(mmc->supply.vmmc,
				&host->sd_oc.nb);
		}
		break;
	case MMC_POWER_ON:
		if (mmc->supply.vqmmc == NULL || IS_ERR(mmc->supply.vqmmc)) {
			dev_info(host->dev, "vqmmc is null, not set!\n");
			break;
		}
		if (!IS_ERR(mmc->supply.vqmmc) && !host->vqmmc_enabled) {
			ret = regulator_enable(mmc->supply.vqmmc);
			if (ret)
				dev_err(host->dev, "Failed to set vqmmc power!\n");
			else
				host->vqmmc_enabled = true;
		}
		break;
	case MMC_POWER_OFF:
		if (mmc->supply.vmmc == NULL  || IS_ERR(mmc->supply.vmmc)) {
			dev_info(host->dev, "vmmc is null, not set!\n");
			break;
		}
		devm_regulator_unregister_notifier(mmc->supply.vmmc,
			&host->sd_oc.nb);
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && host->vqmmc_enabled) {
			regulator_disable(mmc->supply.vqmmc);
			host->vqmmc_enabled = false;
		}

		if (host->id == MSDC_SD) {
			if (host->mclk == 100000) {
				host->block_bad_card = 1;
				pr_notice("[%s]: msdc%d power off at clk %dhz set block_bad_card = %d\n",
					__func__, host->id, host->mclk,
					host->block_bad_card);
			}
		}
		break;
	default:
		break;
	}

#if IS_ENABLED(CONFIG_MMC_AUTOK)
	if (host->timing != ios->timing && !mmc->doing_retune) {
		/* msdc setting TX parameter */
		autok_msdc_tx_setting(host, ios);
		if (ios->timing == MMC_TIMING_MMC_DDR52)
			msdc_init_tune_setting(host);
	}
	if ((host->mclk > ios->clock)
		 && (ios->clock <= 52000000)
		 && (ios->clock > 0)
		 && !mmc->doing_retune)
		msdc_init_tune_setting(host);
#endif

	if (host->mclk != ios->clock || host->timing != ios->timing)
		msdc_set_mclk(host, ios->timing, ios->clock);
#if IS_ENABLED(CONFIG_MMC_AUTOK)
	if (ios->timing == MMC_TIMING_MMC_HS400 &&
		ios->clock > 52000000) {
		dev_err(host->dev, "Tune HS400!\n");
		msdc_execute_tuning(mmc,
			MMC_SEND_TUNING_BLOCK_HS200);
	}
#endif
}
#endif

#if !IS_ENABLED(CONFIG_MMC_AUTOK)
static u32 test_delay_bit(u32 delay, u32 bit)
{
	bit %= PAD_DELAY_MAX;
	return delay & (1 << bit);
}

static int get_delay_len(u32 delay, u32 start_bit)
{
	int i;

	for (i = 0; i < (PAD_DELAY_MAX - start_bit); i++) {
		if (test_delay_bit(delay, start_bit + i) == 0)
			return i;
	}
	return PAD_DELAY_MAX - start_bit;
}

static struct msdc_delay_phase get_best_delay(struct msdc_host *host, u32 delay)
{
	int start = 0, len = 0;
	int start_final = 0, len_final = 0;
	u8 final_phase = 0xff;
	struct msdc_delay_phase delay_phase = { 0, };

	if (delay == 0) {
		dev_err(host->dev, "phase error: [map:%x]\n", delay);
		delay_phase.final_phase = final_phase;
		return delay_phase;
	}

	while (start < PAD_DELAY_MAX) {
		len = get_delay_len(delay, start);
		if (len_final < len) {
			start_final = start;
			len_final = len;
		}
		start += len ? len : 1;
		if (len >= 12 && start_final < 4)
			break;
	}

	/* The rule is that to find the smallest delay cell */
	if (start_final == 0)
		final_phase = (start_final + len_final / 3) % PAD_DELAY_MAX;
	else
		final_phase = (start_final + len_final / 2) % PAD_DELAY_MAX;
	dev_info(host->dev, "phase: [map:%x] [maxlen:%d] [final:%d]\n",
		 delay, len_final, final_phase);

	delay_phase.maxlen = len_final;
	delay_phase.start = start_final;
	delay_phase.final_phase = final_phase;
	return delay_phase;
}

static inline void msdc_set_cmd_delay(struct msdc_host *host, u32 value)
{
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	if (host->top_base)
		sdr_set_field(host->top_base + EMMC_TOP_CMD, PAD_CMD_RXDLY,
			      value);
	else
		sdr_set_field(host->base + tune_reg, MSDC_PAD_TUNE_CMDRDLY,
			      value);
}

static inline void msdc_set_data_delay(struct msdc_host *host, u32 value)
{
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	if (host->top_base)
		sdr_set_field(host->top_base + EMMC_TOP_CONTROL,
			      PAD_DAT_RD_RXDLY, value);
	else
		sdr_set_field(host->base + tune_reg, MSDC_PAD_TUNE_DATRRDLY,
			      value);
}

static int msdc_tune_response(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay, final_fall_delay = { 0,};
	struct msdc_delay_phase internal_delay_phase;
	u8 final_delay, final_maxlen;
	u32 internal_delay = 0;
	u32 tune_reg = host->dev_comp->pad_tune_reg;
	int cmd_err;
	int i, j;

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
	    mmc->ios.timing == MMC_TIMING_UHS_SDR104)
		sdr_set_field(host->base + tune_reg,
			      MSDC_PAD_TUNE_CMDRRDLY,
			      host->hs200_cmd_int_delay);

	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	for (i = 0 ; i < PAD_DELAY_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		/*
		 * Using the same parameters, it may sometimes pass the test,
		 * but sometimes it may fail. To make sure the parameters are
		 * more stable, we test each set of parameters 3 times.
		 */
		for (j = 0; j < 3; j++) {
			mmc_send_tuning(mmc, opcode, &cmd_err);
			if (!cmd_err) {
				rise_delay |= (1 << i);
			} else {
				rise_delay &= ~(1 << i);
				break;
			}
		}
	}
	final_rise_delay = get_best_delay(host, rise_delay);
	/* if rising edge has enough margin, then do not scan falling edge */
	if (final_rise_delay.maxlen >= 12 ||
	    (final_rise_delay.start == 0 && final_rise_delay.maxlen >= 4))
		goto skip_fall;

	sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	for (i = 0; i < PAD_DELAY_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		/*
		 * Using the same parameters, it may sometimes pass the test,
		 * but sometimes it may fail. To make sure the parameters are
		 * more stable, we test each set of parameters 3 times.
		 */
		for (j = 0; j < 3; j++) {
			mmc_send_tuning(mmc, opcode, &cmd_err);
			if (!cmd_err) {
				fall_delay |= (1 << i);
			} else {
				fall_delay &= ~(1 << i);
				break;
			}
		}
	}
	final_fall_delay = get_best_delay(host, fall_delay);

skip_fall:
	final_maxlen = max(final_rise_delay.maxlen, final_fall_delay.maxlen);
	if (final_fall_delay.maxlen >= 12 && final_fall_delay.start < 4)
		final_maxlen = final_fall_delay.maxlen;
	if (final_maxlen == final_rise_delay.maxlen) {
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		final_delay = final_rise_delay.final_phase;
	} else {
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		final_delay = final_fall_delay.final_phase;
	}
	msdc_set_cmd_delay(host, final_delay);

	if (host->dev_comp->async_fifo || host->hs200_cmd_int_delay)
		goto skip_internal;

	for (i = 0; i < PAD_DELAY_MAX; i++) {
		sdr_set_field(host->base + tune_reg,
			      MSDC_PAD_TUNE_CMDRRDLY, i);
		mmc_send_tuning(mmc, opcode, &cmd_err);
		if (!cmd_err)
			internal_delay |= (1 << i);
	}
	dev_dbg(host->dev, "Final internal delay: 0x%x\n", internal_delay);
	internal_delay_phase = get_best_delay(host, internal_delay);
	sdr_set_field(host->base + tune_reg, MSDC_PAD_TUNE_CMDRRDLY,
		      internal_delay_phase.final_phase);
skip_internal:
	dev_dbg(host->dev, "Final cmd pad delay: %x\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}

static int hs400_tune_response(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 cmd_delay = 0;
	struct msdc_delay_phase final_cmd_delay = { 0,};
	u8 final_delay;
	int cmd_err;
	int i, j;

	/* select EMMC50 PAD CMD tune */
	sdr_set_bits(host->base + PAD_CMD_TUNE, BIT(0));
	sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_CMDTA, 2);

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
	    mmc->ios.timing == MMC_TIMING_UHS_SDR104)
		sdr_set_field(host->base + MSDC_PAD_TUNE,
			      MSDC_PAD_TUNE_CMDRRDLY,
			      host->hs200_cmd_int_delay);

	if (host->hs400_cmd_resp_sel_rising)
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	else
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	for (i = 0 ; i < PAD_DELAY_MAX; i++) {
		sdr_set_field(host->base + PAD_CMD_TUNE,
			      PAD_CMD_TUNE_RX_DLY3, i);
		/*
		 * Using the same parameters, it may sometimes pass the test,
		 * but sometimes it may fail. To make sure the parameters are
		 * more stable, we test each set of parameters 3 times.
		 */
		for (j = 0; j < 3; j++) {
			mmc_send_tuning(mmc, opcode, &cmd_err);
			if (!cmd_err) {
				cmd_delay |= (1 << i);
			} else {
				cmd_delay &= ~(1 << i);
				break;
			}
		}
	}
	final_cmd_delay = get_best_delay(host, cmd_delay);
	sdr_set_field(host->base + PAD_CMD_TUNE, PAD_CMD_TUNE_RX_DLY3,
		      final_cmd_delay.final_phase);
	final_delay = final_cmd_delay.final_phase;

	dev_dbg(host->dev, "Final cmd pad delay: %x\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}

static int msdc_tune_data(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay, final_fall_delay = { 0,};
	u8 final_delay, final_maxlen;
	int i, ret;

	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_INT_DAT_LATCH_CK_SEL,
		      host->latch_ck);
	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);
	for (i = 0 ; i < PAD_DELAY_MAX; i++) {
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			rise_delay |= (1 << i);
	}
	final_rise_delay = get_best_delay(host, rise_delay);
	/* if rising edge has enough margin, then do not scan falling edge */
	if (final_rise_delay.maxlen >= 12 ||
	    (final_rise_delay.start == 0 && final_rise_delay.maxlen >= 4))
		goto skip_fall;

	sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
	sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);
	for (i = 0; i < PAD_DELAY_MAX; i++) {
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			fall_delay |= (1 << i);
	}
	final_fall_delay = get_best_delay(host, fall_delay);

skip_fall:
	final_maxlen = max(final_rise_delay.maxlen, final_fall_delay.maxlen);
	if (final_maxlen == final_rise_delay.maxlen) {
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);
		final_delay = final_rise_delay.final_phase;
	} else {
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSPL);
		final_delay = final_fall_delay.final_phase;
	}
	msdc_set_data_delay(host, final_delay);

	dev_dbg(host->dev, "Final data pad delay: %x\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}

/*
 * MSDC IP which supports data tune + async fifo can do CMD/DAT tune
 * together, which can save the tuning time.
 */
static int msdc_tune_together(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 rise_delay = 0, fall_delay = 0;
	struct msdc_delay_phase final_rise_delay, final_fall_delay = { 0,};
	u8 final_delay, final_maxlen;
	int i, ret;

	sdr_set_field(host->base + MSDC_PATCH_BIT, MSDC_INT_DAT_LATCH_CK_SEL,
		      host->latch_ck);

	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	sdr_clr_bits(host->base + MSDC_IOCON,
		     MSDC_IOCON_DSPL | MSDC_IOCON_W_DSPL);
	for (i = 0 ; i < PAD_DELAY_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			rise_delay |= (1 << i);
	}
	final_rise_delay = get_best_delay(host, rise_delay);
	/* if rising edge has enough margin, then do not scan falling edge */
	if (final_rise_delay.maxlen >= 12 ||
	    (final_rise_delay.start == 0 && final_rise_delay.maxlen >= 4))
		goto skip_fall;

	sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
	sdr_set_bits(host->base + MSDC_IOCON,
		     MSDC_IOCON_DSPL | MSDC_IOCON_W_DSPL);
	for (i = 0; i < PAD_DELAY_MAX; i++) {
		msdc_set_cmd_delay(host, i);
		msdc_set_data_delay(host, i);
		ret = mmc_send_tuning(mmc, opcode, NULL);
		if (!ret)
			fall_delay |= (1 << i);
	}
	final_fall_delay = get_best_delay(host, fall_delay);

skip_fall:
	final_maxlen = max(final_rise_delay.maxlen, final_fall_delay.maxlen);
	if (final_maxlen == final_rise_delay.maxlen) {
		sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		sdr_clr_bits(host->base + MSDC_IOCON,
			     MSDC_IOCON_DSPL | MSDC_IOCON_W_DSPL);
		final_delay = final_rise_delay.final_phase;
	} else {
		sdr_set_bits(host->base + MSDC_IOCON, MSDC_IOCON_RSPL);
		sdr_set_bits(host->base + MSDC_IOCON,
			     MSDC_IOCON_DSPL | MSDC_IOCON_W_DSPL);
		final_delay = final_fall_delay.final_phase;
	}

	msdc_set_cmd_delay(host, final_delay);
	msdc_set_data_delay(host, final_delay);

	dev_dbg(host->dev, "Final pad delay: %x\n", final_delay);
	return final_delay == 0xff ? -EIO : 0;
}
#endif

static int msdc_get_cd(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	int val;

	if (mmc->caps & MMC_CAP_NONREMOVABLE) {
		host->card_inserted = 1;
		goto end;
	} else if (!host->internal_cd) {
		host->card_inserted = mmc_gpio_get_cd(mmc);
	} else {
		val = readl(host->base + MSDC_PS) & MSDC_PS_CDSTS;
		if (mmc->caps2 & MMC_CAP2_CD_ACTIVE_HIGH)
			host->card_inserted = !!val;
		else
			host->card_inserted = !val;
	}

	if (host->block_bad_card)
		host->card_inserted = 0;
end:
	pr_info(
		"%s:card status:%s block bad card<%d> trigger card event<%d>",
		__func__, host->card_inserted ? "inserted" : "removed",
		host->block_bad_card, mmc->trigger_card_event);

	return host->card_inserted;
}

static void msdc_reset_bad_sd_detecter(struct msdc_host *host)
{
	u32 i = 0;

	if (host == NULL) {
		pr_notice("WARN: host is NULL at %s\n", __func__);
		return;
	}

	host->block_bad_card = 0;
	for (i = 0; i < BAD_SD_DETECTER_COUNT; i++)
		bad_sd_detecter[i] = 0;
}

static void msdc_ops_card_event(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->power_cycle_cnt = 0;
	host->data_timeout_cont = 0;
	host->is_autok_done = 0;
	msdc_reset_bad_sd_detecter(host);

	msdc_get_cd(mmc);
}

void msdc_set_bad_card_and_remove(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);

	if (host->card_inserted) {
		host->block_bad_card = 1;
		host->card_inserted = 0;
	}

	if ((mmc == NULL) || (mmc->card == NULL)) {
		pr_info("WARN: mmc or card is NULL");
		return;
	}

	if (mmc->card) {
		mmc_card_set_removed(mmc->card);
		pr_info("schedule mmc_rescan");
		mmc_detect_change(mmc, msecs_to_jiffies(200));
		if (host->block_bad_card)
			pr_info("remove the bad card, block_bad_card=%d, card_inserted=%d",
				host->block_bad_card, host->card_inserted);
	}
}

int sdcard_hw_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;

	host->card_inserted = msdc_get_cd(mmc);

	if (!(host->card_inserted)) {
		pr_notice("card is not inserted!\n");
		msdc_set_bad_card_and_remove(host);
		ret = -1;
		return ret;
	}

	ret = mmc_hw_reset(mmc);
	if (ret) {
		if (++host->power_cycle_cnt
			> MSDC_MAX_POWER_CYCLE_FAIL_CONTINUOUS)
			msdc_set_bad_card_and_remove(host);
		pr_notice(
			"msdc%d power reset (%d) failed, block_bad_card = %d\n",
			host->id, host->power_cycle_cnt, host->block_bad_card);
	} else {
		host->power_cycle_cnt = 0;
		pr_notice("msdc%d power reset success\n", host->id);
	}

	return ret;
}

int msdc_data_timeout_cont_chk(struct msdc_host *host)
{
	if ((host->id == MSDC_SD) &&
		(host->data_timeout_cont >= MSDC_MAX_DATA_TIMEOUT_CONTINUOUS)) {
		pr_info("force remove bad card, data timeout continuous %d",
			host->data_timeout_cont);
		msdc_set_bad_card_and_remove(host);
		return 1;
	}

	return 0;
}

/* SDcard will change speed mode and power reset
 * UHS card
 *    UHS_SDR104 --> UHS_DDR50 --> UHS_SDR50 --> UHS_SDR25
 * HS card
 *    50MHz --> 25MHz --> 12.5MHz --> 6.25MHz
 */
int sdcard_reset_tuning(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	char *remove_cap;
	int ret = 0;

	if (!mmc->card) {
		pr_notice("mmc card = NULL, skip reset tuning\n");
		return -1;
	}

	if (mmc_card_uhs(mmc->card)) {
		if (mmc->card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR104) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR104;
			remove_cap = "UHS_SDR104";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_DDR50) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_DDR50;
			remove_cap = "UHS_DDR50";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_SDR50) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR50;
			remove_cap = "UHS_SDR50";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_SDR25) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR25;
			remove_cap = "UHS_SDR25";
		} else {
			remove_cap = "none";
		}
		pr_notice("msdc%d: remove %s mode then reinit card\n", host->id,
			remove_cap);
	} else if (mmc_card_hs(mmc->card)) {
		if (mmc->card->sw_caps.hs_max_dtr >= HIGH_SPEED_MAX_DTR / 4)
			mmc->card->sw_caps.hs_max_dtr /= 2;
		pr_notice("msdc%d: set hs speed %dhz then reinit card\n",
			host->id, mmc->card->sw_caps.hs_max_dtr);
	} else {
		pr_notice("msdc%d: ds card just reinit card\n", host->id);
	}

	/* force remove card for continuous data timeout */
	ret = msdc_data_timeout_cont_chk(host);
	if (ret) {
		ret = -1;
		goto done;
	}

	/* power cycle sdcard */
	ret = sdcard_hw_reset(mmc);
	if (ret) {
		ret = -1;
		goto done;
	}

done:
	return ret;
}

static int msdc_detect_bad_sd(struct msdc_host *host, u32 condition)
{
	unsigned long time_current = jiffies;
	int ret = 0;

	if (host == NULL) {
		pr_notice("WARN: host is NULL at %s\n", __func__);
		ret = -1;
		goto end;
	}

	if (condition >= BAD_SD_DETECTER_COUNT) {
		pr_notice("msdc1: BAD_SD_DETECTER_COUNT is %d, need check it's definition at %s\n",
			BAD_SD_DETECTER_COUNT, __func__);
		ret = -1;
		goto end;
	}

	if (bad_sd_forget[condition]
	&& time_after(time_current,
	(bad_sd_timer[condition] + bad_sd_forget[condition] * HZ)))
		bad_sd_detecter[condition] = 0;
	bad_sd_timer[condition] = time_current;

	if (++(bad_sd_detecter[condition]) >= bad_sd_tolerance[condition]) {
		msdc_set_bad_card_and_remove(host);
		ret = -1;
	}
	pr_notice("%s:bad_sd_detecter:%d\n", __func__, bad_sd_detecter[condition]);

end:
	return ret;
}

static int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;

#if IS_ENABLED(CONFIG_MMC_AUTOK)
	if (host->need_tune) {
		if (host->id == MSDC_SD) {
			if (msdc_get_cd(mmc) == 0)
				goto end;

			ret = msdc_detect_bad_sd(host, 0);
			if (ret)
				goto end;
		}
		pr_info("msdc%d: %s error retune %d times\n",
				host->id, host->id ? "SD" : "eMMC",
				++host->retune_times);
	} else {
		msdc_init_tune_path(host, mmc->ios.timing);
		autok_msdc_tx_setting(host, &mmc->ios);
	}

	host->tuning_in_progress = true;
	if (host->id == MSDC_EMMC)
		ret = emmc_execute_autok(host, opcode);
	else if (host->id == MSDC_SD || host->id == MSDC_SDIO)
		ret = sd_execute_autok(host, opcode);
	host->tuning_in_progress = false;

#if IS_ENABLED(CONFIG_MMC_DEBUG)
	if (ret)
		msdc_dump_info(NULL, 0, NULL, host);
#endif
	if (host->retune_times >= 4) {
		if (host->id == MSDC_SD)
			sdcard_reset_tuning(mmc);
	} else if (!ret) {
		pr_info("msdc%d autok pass\n", host->id);
		host->need_tune = TUNE_AUTOK_PASS;
	}
#else
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	if (host->dev_comp->data_tune && host->dev_comp->async_fifo) {
		ret = msdc_tune_together(mmc, opcode);
		if (host->hs400_mode) {
			sdr_clr_bits(host->base + MSDC_IOCON,
				     MSDC_IOCON_DSPL | MSDC_IOCON_W_DSPL);
			msdc_set_data_delay(host, 0);
		}
		goto tune_done;
	}
	if (host->hs400_mode &&
	    host->dev_comp->hs400_tune)
		ret = hs400_tune_response(mmc, opcode);
	else
		ret = msdc_tune_response(mmc, opcode);
	if (ret == -EIO) {
		dev_err(host->dev, "Tune response fail!\n");
		return ret;
	}
	if (host->hs400_mode == false) {
		ret = msdc_tune_data(mmc, opcode);
		if (ret == -EIO)
			dev_err(host->dev, "Tune data fail!\n");
	}

tune_done:
	host->saved_tune_para.iocon = readl(host->base + MSDC_IOCON);
	host->saved_tune_para.pad_tune = readl(host->base + tune_reg);
	host->saved_tune_para.pad_cmd_tune = readl(host->base + PAD_CMD_TUNE);
	if (host->top_base) {
		host->saved_tune_para.emmc_top_control = readl(host->top_base +
				EMMC_TOP_CONTROL);
		host->saved_tune_para.emmc_top_cmd = readl(host->top_base +
				EMMC_TOP_CMD);
	}
#endif

end:
	return ret;
}

static int msdc_prepare_hs400_tuning(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->hs400_mode = true;
#if !IS_ENABLED(CONFIG_MMC_AUTOK)
	if (host->top_base)
		writel(host->hs400_ds_delay,
		       host->top_base + EMMC50_PAD_DS_TUNE);
	else
		writel(host->hs400_ds_delay, host->base + PAD_DS_TUNE);
	/* hs400 mode must set it to 0 */
	sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_CFGCRCSTS);
	/* to improve read performance, set outstanding to 2 */
	sdr_set_field(host->base + EMMC50_CFG3, EMMC50_CFG3_OUTS_WR, 2);
#endif

	return 0;
}

static void msdc_hw_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	sdr_set_bits(host->base + EMMC_IOCON, 1);
	udelay(10); /* 10us is enough */
	sdr_clr_bits(host->base + EMMC_IOCON, 1);
}

static void msdc_ack_sdio_irq(struct mmc_host *mmc)
{
	unsigned long flags;
	struct msdc_host *host = mmc_priv(mmc);

	spin_lock_irqsave(&host->lock, flags);
	__msdc_enable_sdio_irq(host, 1);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void msdc_hs400_enhanced_strobe(struct mmc_host *mmc,
				       struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (ios->enhanced_strobe) {
		msdc_prepare_hs400_tuning(mmc, ios);
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_PADCMD_LATCHCK, 1);
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_CMD_RESP_SEL, 1);
		sdr_set_field(host->base + EMMC50_CFG1, EMMC50_CFG1_DS_CFG, 1);

		sdr_clr_bits(host->base + CQHCI_SETTING, CQHCI_RD_CMD_WND_SEL);
		sdr_clr_bits(host->base + CQHCI_SETTING, CQHCI_WR_CMD_WND_SEL);
		sdr_clr_bits(host->base + EMMC51_CFG0, CMDQ_RDAT_CNT);
	} else {
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_PADCMD_LATCHCK, 0);
		sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_CMD_RESP_SEL, 0);
		sdr_set_field(host->base + EMMC50_CFG1, EMMC50_CFG1_DS_CFG, 0);

		sdr_set_bits(host->base + CQHCI_SETTING, CQHCI_RD_CMD_WND_SEL);
		sdr_set_bits(host->base + CQHCI_SETTING, CQHCI_WR_CMD_WND_SEL);
		sdr_set_field(host->base + EMMC51_CFG0, CMDQ_RDAT_CNT, 0xb4);
	}
}

static void msdc_cqe_enable(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	/* enable cmdq irq */
	writel(MSDC_INT_CMDQ, host->base + MSDC_INTEN);
	/* enable busy check */
	sdr_set_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
	/* default write data / busy timeout 20s */
	msdc_set_busy_timeout(host, 20 * 1000000000ULL, 0);
	/* default read data timeout 1s */
	msdc_set_timeout(host, 1000000000ULL, 0);
}

static void msdc_cqe_disable(struct mmc_host *mmc, bool recovery)
{
	struct msdc_host *host = mmc_priv(mmc);

	/* disable cmdq irq */
	sdr_clr_bits(host->base + MSDC_INTEN, MSDC_INT_CMDQ);
	/* disable busy check */
	sdr_clr_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);

	if (recovery) {
		sdr_set_field(host->base + MSDC_DMA_CTRL,
			      MSDC_DMA_CTRL_STOP, 1);
		msdc_reset_hw(host);
	}
}

static void msdc_cqe_pre_enable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg |= CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static void msdc_cqe_post_disable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg &= ~CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static const struct mmc_host_ops mt_msdc_ops = {
	.post_req = msdc_post_req,
	.pre_req = msdc_pre_req,
	.request = msdc_ops_request,
	.set_ios = msdc_ops_set_ios,
	.get_ro = mmc_gpio_get_ro,
	.get_cd = msdc_get_cd,
	.hs400_enhanced_strobe = msdc_hs400_enhanced_strobe,
	.enable_sdio_irq = msdc_enable_sdio_irq,
	.ack_sdio_irq = msdc_ack_sdio_irq,
	.start_signal_voltage_switch = msdc_ops_switch_volt,
	.card_busy = msdc_card_busy,
	.execute_tuning = msdc_execute_tuning,
	.prepare_hs400_tuning = msdc_prepare_hs400_tuning,
	.hw_reset = msdc_hw_reset,
	.card_event = msdc_ops_card_event,
};

static const struct cqhci_host_ops msdc_cmdq_ops = {
	.enable         = msdc_cqe_enable,
	.disable        = msdc_cqe_disable,
	.pre_enable = msdc_cqe_pre_enable,
	.post_disable = msdc_cqe_post_disable,
};

static irqreturn_t sdio_eint_irq(int irq, void *dev_id)
{
	unsigned long flags;
	struct msdc_host *host = (struct msdc_host *)dev_id;
	struct mmc_host *mmc = mmc_from_priv(host);

	spin_lock_irqsave(&host->lock, flags);
	if (likely(host->sdio_irq_cnt > 0)) {
		disable_irq_nosync(host->eint_irq);
		disable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt--;
	}
	spin_unlock_irqrestore(&host->lock, flags);

	sdio_signal_irq(mmc);

	return IRQ_HANDLED;
}

static int request_sdio_eint_irq(struct msdc_host *host)
{
	struct gpio_desc *desc;
	int ret = 0;
	int irq;

	desc = devm_gpiod_get_index(host->dev, "eint", 0, GPIOD_IN);
	if (IS_ERR(desc)) {
		pr_info("mmc%d:failed to get sdio eint gpiod\n", host->id);
		return PTR_ERR(desc);
	}

	irq = gpiod_to_irq(desc);
	if (irq >= 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(host->dev, irq,
				NULL, sdio_eint_irq,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"sdio-eint", host);
	} else
		ret = irq;

	host->eint_irq = irq;
	return ret;
}

static void msdc_of_property_parse(struct platform_device *pdev,
				   struct msdc_host *host)
{
	of_property_read_u32(pdev->dev.of_node, "mediatek,latch-ck",
			     &host->latch_ck);

	of_property_read_u32(pdev->dev.of_node, "hs400-ds-delay",
			     &host->hs400_ds_delay);

	of_property_read_u32(pdev->dev.of_node, "mediatek,hs200-cmd-int-delay",
			     &host->hs200_cmd_int_delay);

	of_property_read_u32(pdev->dev.of_node, "mediatek,hs400-cmd-int-delay",
			     &host->hs400_cmd_int_delay);

	of_property_read_u32(pdev->dev.of_node, "host-index",
			     &host->id);

	host->dvfsrc_vcore_power =
		devm_regulator_get_optional(&pdev->dev, "dvfsrc-vcore");
	if (IS_ERR(host->dvfsrc_vcore_power)) {
		pr_info("mmc%d:failed to get dvfsrc-vcore:%ld\n",
			host->id, PTR_ERR(host->dvfsrc_vcore_power));
		host->dvfsrc_vcore_power = NULL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "req-vcore",
		&host->req_vcore)) {
		pr_info("mmc%d:failed to get req-vcore\n", host->id);
		host->req_vcore = 0;
	} else
		pr_info("mmc%d:req-vcore:%d\n", host->id, host->req_vcore);

	if (of_property_read_u32(pdev->dev.of_node, "ocr-voltage", &host->ocr_volt)) {
		pr_info("mmc%d:failed to get ocr_volt\n", host->id);
		host->ocr_volt = 0;
	} else
		pr_info("mmc%d:ocr-voltage:%d\n", host->id, host->ocr_volt);

	if (of_property_read_bool(pdev->dev.of_node,
				  "mediatek,hs400-cmd-resp-sel-rising"))
		host->hs400_cmd_resp_sel_rising = true;
	else
		host->hs400_cmd_resp_sel_rising = false;

	if (of_property_read_bool(pdev->dev.of_node, "supports-cqe"))
		host->cqhci = true;
	else
		host->cqhci = false;
	if (of_property_read_bool(pdev->dev.of_node, "mediatek,mmc-qos"))
		host->qos_enable = true;
	else
		host->qos_enable = false;

	if (host->qos_enable) {
		host->bw_path = of_icc_get(&pdev->dev, "msdc-perf-bw");
		if (IS_ERR(host->bw_path)) {
			pr_info("mmc%d:failed to get icc path\n", host->id);
			host->bw_path = NULL;
		}
		host->peak_bw =
		    dvfsrc_get_required_opp_peak_bw(pdev->dev.of_node, 0);
	}
}
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static int msdc_of_clock_parse(struct platform_device *pdev,
			       struct msdc_host *host)
{
	int ret;

	host->src_clk = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(host->src_clk))
		return PTR_ERR(host->src_clk);

	host->h_clk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(host->h_clk))
		return PTR_ERR(host->h_clk);

	host->bus_clk = devm_clk_get_optional(&pdev->dev, "bus_clk");
	if (IS_ERR(host->bus_clk))
		host->bus_clk = NULL;

	host->crypto_clk = devm_clk_get_optional(&pdev->dev, "crypto_clk");
	if (IS_ERR(host->crypto_clk))
		host->crypto_clk = NULL;

	/*source clock control gate is optional clock*/
	host->src_clk_cg = devm_clk_get_optional(&pdev->dev, "source_cg");
	if (IS_ERR(host->src_clk_cg))
		host->src_clk_cg = NULL;

	host->sys_clk_cg = devm_clk_get_optional(&pdev->dev, "sys_cg");
	if (IS_ERR(host->sys_clk_cg))
		host->sys_clk_cg = NULL;

	host->crypto_cg = devm_clk_get_optional(&pdev->dev, "crypto_cg");
	if (IS_ERR(host->crypto_cg))
		host->crypto_cg = NULL;

	/* If present, always enable for this clock gate */
	clk_prepare_enable(host->sys_clk_cg);

	host->bulk_clks[0].id = "pclk_cg";
	host->bulk_clks[1].id = "axi_cg";
	host->bulk_clks[2].id = "ahb_cg";
	ret = devm_clk_bulk_get_optional(&pdev->dev, MSDC_NR_CLOCKS,
					 host->bulk_clks);
	if (ret) {
		dev_err(&pdev->dev, "Cannot get pclk/axi/ahb clock gates\n");
		return ret;
	}

	return 0;
}
#endif

void msdc_sd_power_off(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);

	if (mmc) {
		pr_notice("VMMC OC,Power Off SD card\n");
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);

		if (!IS_ERR(mmc->supply.vqmmc) && host->vqmmc_enabled) {
			regulator_disable(mmc->supply.vqmmc);
			host->vqmmc_enabled = false;
		}

		msdc_set_bad_card_and_remove(host);
	}
}

static int msdc_sd_event(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct msdc_host *host =
		container_of(nb, struct msdc_host, sd_oc.nb);

	switch (event) {
	case REGULATOR_EVENT_OVER_CURRENT:
	case REGULATOR_EVENT_FAIL:
		schedule_work(&host->sd_oc.work);
		break;
	default:
		break;
	};

	return NOTIFY_OK;
}

static void sdcard_oc_handler(struct work_struct *work)
{
	struct msdc_host *host =
		container_of(work, struct msdc_host, sd_oc.work);

	msdc_sd_power_off(host);
}

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct resource *res;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}

	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	ret = mmc_of_parse(mmc);
	if (ret)
		goto host_free;

	host->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto host_free;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		host->top_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(host->top_base))
			host->top_base = NULL;
	}

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	ret = mmc_regulator_get_supply(mmc);
	if (ret)
		goto host_free;

	ret = msdc_of_clock_parse(pdev, host);
	if (ret)
		goto host_free;
#endif

	host->reset = devm_reset_control_get_optional_exclusive(&pdev->dev,
								"hrst");
	if (IS_ERR(host->reset)) {
		ret = PTR_ERR(host->reset);
		goto host_free;
	}

	/* only eMMC has crypto property */
	if (!(mmc->caps2 & MMC_CAP2_NO_MMC)) {
		if (!IS_ERR_OR_NULL(host->crypto_clk) &&
			!IS_ERR_OR_NULL(host->crypto_cg))
			mmc->caps2 |= MMC_CAP2_CRYPTO;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = -EINVAL;
		goto host_free;
	}

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(host->pinctrl)) {
		ret = PTR_ERR(host->pinctrl);
		dev_err(&pdev->dev, "Cannot find pinctrl!\n");
		goto host_free;
	}

	host->pins_default = pinctrl_lookup_state(host->pinctrl, "default");
	if (IS_ERR(host->pins_default)) {
		ret = PTR_ERR(host->pins_default);
		dev_err(&pdev->dev, "Cannot find pinctrl default!\n");
		goto host_free;
	}

	host->pins_uhs = pinctrl_lookup_state(host->pinctrl, "state_uhs");
	if (IS_ERR(host->pins_uhs)) {
		ret = PTR_ERR(host->pins_uhs);
		dev_err(&pdev->dev, "Cannot find pinctrl uhs!\n");
		goto host_free;
	}
#endif

	msdc_of_property_parse(pdev, host);

	if (host->id == MSDC_SD) {
		host->sd_oc.nb.notifier_call = msdc_sd_event;
		INIT_WORK(&host->sd_oc.work, sdcard_oc_handler);
	}

	host->dev = &pdev->dev;
	host->dev_comp = of_device_get_match_data(&pdev->dev);
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	host->src_clk_freq = clk_get_rate(host->src_clk);
	/* Because of the chip manufacturing process, the VMC voltage cannot be too high */
	if (host->ocr_volt != 0)
		mmc->ocr_avail = host->ocr_volt;
#else
	host->src_clk_freq = FPGA_SRC_CLK;
	mmc->ocr_avail = MSDC_OCR_AVAIL;
#endif
	/* Set host parameters to mmc */
	mmc->ops = &mt_msdc_ops;
	if (host->dev_comp->clk_div_bits == 8)
		mmc->f_min = DIV_ROUND_UP(host->src_clk_freq, 4 * 255);
	else
		mmc->f_min = DIV_ROUND_UP(host->src_clk_freq, 4 * 4095);

	if (!(mmc->caps & MMC_CAP_NONREMOVABLE) &&
	    !mmc_can_gpio_cd(mmc) &&
	    host->dev_comp->use_internal_cd) {
		/*
		 * Is removable but no GPIO declared, so
		 * use internal functionality.
		 */
		host->internal_cd = true;
	}

	if (mmc->caps & MMC_CAP_SDIO_IRQ)
		mmc->caps2 |= MMC_CAP2_SDIO_IRQ_NOTHREAD;

	mmc->caps |= MMC_CAP_CMD23;
	if (host->cqhci)
		mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_BD_NUM;
	if (host->dev_comp->support_64g)
		mmc->max_seg_size = BDMA_DESC_BUFLEN_EXT;
	else
		mmc->max_seg_size = BDMA_DESC_BUFLEN;
	mmc->max_blk_size = 2048;
	mmc->max_req_size = 512 * 1024;
	mmc->max_blk_count = mmc->max_req_size / 512;
	if (host->dev_comp->support_64g)
		host->dma_mask = DMA_BIT_MASK(36);
	else
		host->dma_mask = DMA_BIT_MASK(32);
	mmc_dev(mmc)->dma_mask = &host->dma_mask;

	/* here ungate due to cqhci init will access registers */
	msdc_ungate_clock(host);
	if (mmc->caps2 & MMC_CAP2_CQE) {
		host->cq_host = devm_kzalloc(mmc->parent,
					     sizeof(*host->cq_host),
					     GFP_KERNEL);
		if (!host->cq_host) {
			ret = -ENOMEM;
			goto host_free;
		}
		host->cq_host->caps |= CQHCI_TASK_DESC_SZ_128;
		host->cq_host->mmio = host->base + 0x800;
		host->cq_host->ops = &msdc_cmdq_ops;
		ret = cqhci_init(host->cq_host, mmc, true);
		if (ret)
			goto host_free;
		mmc->max_segs = 128;
		/* cqhci 16bit length */
		/* 0 size, means 65536 so we don't have to -1 here */
		mmc->max_seg_size = 64 * 1024;
	}

	host->timeout_clks = 3 * 1048576;
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

	platform_set_drvdata(pdev, mmc);
	msdc_init_hw(host);

	ret = devm_request_irq(&pdev->dev, host->irq, msdc_irq,
			       IRQF_TRIGGER_NONE, pdev->name, host);
	if (ret)
		goto release;

	if (host->id == MSDC_SDIO) {
		ret = request_sdio_eint_irq(host);
		if (ret)
			dev_info(host->dev, "failed to register sdio eint irq!\n");
	}

	cpu_latency_qos_add_request(&host->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	pm_runtime_set_active(host->dev);
	pm_runtime_set_autosuspend_delay(host->dev, MTK_MMC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(host->dev);
	pm_runtime_enable(host->dev);
	ret = mmc_add_host(mmc);

	if (ret)
		goto end;

#if IS_ENABLED(CONFIG_MMC_DEBUG)
	ret = mmc_dbg_register(mmc);
#endif

#if IS_ENABLED(CONFIG_RPMB)
	ret = mmc_rpmb_register(mmc);
#endif

	return 0;
end:
	pm_runtime_disable(host->dev);
	cpu_latency_qos_remove_request(&host->pm_qos_req);
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
	mmc_remove_host(mmc);
	msdc_deinit_hw(host);
	msdc_gate_clock(host);

	cpu_latency_qos_remove_request(&host->pm_qos_req);
	pm_runtime_disable(host->dev);
	pm_runtime_put_noidle(host->dev);
	dma_free_coherent(&pdev->dev,
			2 * sizeof(struct mt_gpdma_desc),
			host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(&pdev->dev, MAX_BD_NUM * sizeof(struct mt_bdma_desc),
			host->dma.bd, host->dma.bd_addr);

	mmc_free_host(mmc);

	mmc_mtk_biolog_exit();

	return 0;
}

#if !IS_ENABLED(CONFIG_MMC_AUTOK)
static void msdc_save_reg(struct msdc_host *host)
{
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	host->save_para.msdc_cfg = readl(host->base + MSDC_CFG);
	host->save_para.iocon = readl(host->base + MSDC_IOCON);
	host->save_para.sdc_cfg = readl(host->base + SDC_CFG);
	host->save_para.patch_bit0 = readl(host->base + MSDC_PATCH_BIT);
	host->save_para.patch_bit1 = readl(host->base + MSDC_PATCH_BIT1);
	host->save_para.patch_bit2 = readl(host->base + MSDC_PATCH_BIT2);
	host->save_para.pad_ds_tune = readl(host->base + PAD_DS_TUNE);
	host->save_para.pad_cmd_tune = readl(host->base + PAD_CMD_TUNE);
	host->save_para.emmc50_cfg0 = readl(host->base + EMMC50_CFG0);
	host->save_para.emmc50_cfg3 = readl(host->base + EMMC50_CFG3);
	host->save_para.sdc_fifo_cfg = readl(host->base + SDC_FIFO_CFG);
	if (host->top_base) {
		host->save_para.emmc_top_control =
			readl(host->top_base + EMMC_TOP_CONTROL);
		host->save_para.emmc_top_cmd =
			readl(host->top_base + EMMC_TOP_CMD);
		host->save_para.emmc50_pad_ds_tune =
			readl(host->top_base + EMMC50_PAD_DS_TUNE);
	} else {
		host->save_para.pad_tune = readl(host->base + tune_reg);
	}
}

static void msdc_restore_reg(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	writel(host->save_para.msdc_cfg, host->base + MSDC_CFG);
	writel(host->save_para.iocon, host->base + MSDC_IOCON);
	writel(host->save_para.sdc_cfg, host->base + SDC_CFG);
	writel(host->save_para.patch_bit0, host->base + MSDC_PATCH_BIT);
	writel(host->save_para.patch_bit1, host->base + MSDC_PATCH_BIT1);
	writel(host->save_para.patch_bit2, host->base + MSDC_PATCH_BIT2);
	writel(host->save_para.pad_ds_tune, host->base + PAD_DS_TUNE);
	writel(host->save_para.pad_cmd_tune, host->base + PAD_CMD_TUNE);
	writel(host->save_para.emmc50_cfg0, host->base + EMMC50_CFG0);
	writel(host->save_para.emmc50_cfg3, host->base + EMMC50_CFG3);
	writel(host->save_para.sdc_fifo_cfg, host->base + SDC_FIFO_CFG);
	if (host->top_base) {
		writel(host->save_para.emmc_top_control,
		       host->top_base + EMMC_TOP_CONTROL);
		writel(host->save_para.emmc_top_cmd,
		       host->top_base + EMMC_TOP_CMD);
		writel(host->save_para.emmc50_pad_ds_tune,
		       host->top_base + EMMC50_PAD_DS_TUNE);
	} else {
		writel(host->save_para.pad_tune, host->base + tune_reg);
	}

	if (sdio_irq_claimed(mmc))
		__msdc_enable_sdio_irq(host, 1);
}
#else
void msdc_save_timing_setting(struct msdc_host *host)
{
	int i;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	sdr_get_field(host->base + MSDC_IOCON,
		MSDC_IOCON_RSPL, &host->save_para.cmd_edge);
	sdr_get_field(host->base + MSDC_IOCON,
		MSDC_IOCON_DSPL, &host->save_para.rdata_edge);
	sdr_get_field(host->base + MSDC_IOCON,
		MSDC_IOCON_W_DSPL, &host->save_para.wdata_edge);

	/* this is for suspend only */
	host->save_para.msdc_cfg = readl(host->base + MSDC_CFG);
	host->save_para.sdc_cfg = readl(host->base + SDC_CFG);
	host->save_para.iocon = readl(host->base + MSDC_IOCON);
	host->save_para.emmc50_cfg0 = readl(host->base + EMMC50_CFG0);

	host->save_para.patch_bit0 = readl(host->base + MSDC_PATCH_BIT);
	host->save_para.patch_bit1 = readl(host->base + MSDC_PATCH_BIT1);
	host->save_para.patch_bit2 = readl(host->base + MSDC_PATCH_BIT2);
	host->save_para.sdc_fifo_cfg = readl(host->base + SDC_FIFO_CFG);
	host->save_para.sdc_adv_cfg0 = readl(host->base + SDC_ADV_CFG0);

	if (host->top_base) {
		host->save_para.emmc_top_control
			= readl(host->top_base + EMMC_TOP_CONTROL);
		host->save_para.emmc_top_cmd
			= readl(host->top_base + EMMC_TOP_CMD);
		host->save_para.top_emmc50_pad_ctl0
			= readl(host->top_base + EMMC50_PAD_CTL0);
		host->save_para.emmc50_pad_ds_tune
			= readl(host->top_base + EMMC50_PAD_DS_TUNE);
		for (i = 0; i < 8; i++) {
			host->save_para.top_emmc50_pad_dat_tune[i]
				= readl(host->top_base + EMMC50_PAD_DAT0_TUNE
					+ i * 4);
		}
	} else {
		host->save_para.pad_tune = readl(host->base + tune_reg);
		host->save_para.pad_tune1 = readl(host->base + MSDC_PAD_TUNE1);
	}
}

void msdc_restore_timing_setting(struct msdc_host *host)
{
	struct mmc_host *mmc = mmc_from_priv(host);
	int emmc = (host->id == MSDC_EMMC) ? 1 : 0;
	int i;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	autok_path_sel(host);
	writel(host->save_para.msdc_cfg, host->base + MSDC_CFG);
	writel(host->save_para.sdc_cfg, host->base + SDC_CFG);
	writel(host->save_para.iocon, host->base + MSDC_IOCON);

	if (!host->top_base) {
		writel(host->save_para.pad_tune, host->base + tune_reg);
		writel(host->save_para.pad_tune1, host->base + MSDC_PAD_TUNE1);
	}

	writel(host->save_para.patch_bit0, host->base + MSDC_PATCH_BIT);
	writel(host->save_para.patch_bit1, host->base + MSDC_PATCH_BIT1);
	writel(host->save_para.patch_bit2, host->base + MSDC_PATCH_BIT2);
	writel(host->save_para.sdc_fifo_cfg, host->base + SDC_FIFO_CFG);
	writel(host->save_para.sdc_adv_cfg0, host->base + SDC_ADV_CFG0);

	if (emmc && !host->top_base) {
		/* FIX ME: sdio shall add extra check for sdio3.0+ */
		sdr_set_field(host->base + PAD_DS_TUNE, PAD_DS_TUNE_DLY1,
			host->save_para.ds_dly1);
		sdr_set_field(host->base + PAD_DS_TUNE, PAD_DS_TUNE_DLY3,
			host->save_para.ds_dly3);
		writel(host->save_para.emmc50_pad_cmd_tune,
			host->base + PAD_CMD_TUNE);
		writel(host->save_para.emmc50_dat01,
			host->base + EMMC50_PAD_DAT01_TUNE);
		writel(host->save_para.emmc50_dat23,
			host->base + EMMC50_PAD_DAT23_TUNE);
		writel(host->save_para.emmc50_dat45,
			host->base + EMMC50_PAD_DAT45_TUNE);
		writel(host->save_para.emmc50_dat67,
			host->base + EMMC50_PAD_DAT67_TUNE);
	}

	if (emmc)
		writel(host->save_para.emmc50_cfg0, host->base + EMMC50_CFG0);

	if (host->top_base) {
		writel(host->save_para.emmc_top_control,
			host->top_base + EMMC_TOP_CONTROL);
		writel(host->save_para.emmc_top_cmd,
			host->top_base + EMMC_TOP_CMD);
		writel(host->save_para.top_emmc50_pad_ctl0,
			host->top_base + EMMC50_PAD_CTL0);
		writel(host->save_para.emmc50_pad_ds_tune,
			host->top_base + EMMC50_PAD_DS_TUNE);
		for (i = 0; i < 8; i++) {
			writel(host->save_para.top_emmc50_pad_dat_tune[i],
				host->top_base + EMMC50_PAD_DAT0_TUNE + i * 4);
		}
	}
	if (sdio_irq_claimed(mmc))
		__msdc_enable_sdio_irq(host, 1);
}
#endif
static int __maybe_unused msdc_runtime_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

#if IS_ENABLED(CONFIG_MMC_AUTOK)
	msdc_save_timing_setting(host);
#else
	msdc_save_reg(host);
#endif

	sdr_clr_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);
	if (host->sdio_irq_cnt == 0 && host->id == MSDC_SDIO) {
		enable_irq(host->eint_irq);
		enable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt++;
	}

	msdc_gate_clock(host);

	if (host->dvfsrc_vcore_power && host->req_vcore) {
		if (regulator_set_voltage(host->dvfsrc_vcore_power, 0, INT_MAX))
			pr_info("%s: failed to set vcore to MIN\n", __func__);
	}
	cpu_latency_qos_update_request(&host->pm_qos_req,
		PM_QOS_DEFAULT_VALUE);

	set_mmc_perf_mode(mmc, false);

	return 0;
}

static int __maybe_unused msdc_runtime_resume(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);

	cpu_latency_qos_update_request(&host->pm_qos_req, 0);
	if (host->dvfsrc_vcore_power && host->req_vcore) {
		if (regulator_set_voltage(host->dvfsrc_vcore_power,
			host->req_vcore, INT_MAX))
			pr_info("%s: failed to set vcore to %d\n",
				__func__, host->req_vcore);
	}

	msdc_ungate_clock(host);
#if IS_ENABLED(CONFIG_MMC_AUTOK)
	msdc_restore_timing_setting(host);
#else
	msdc_restore_reg(host);
#endif

	if (host->sdio_irq_cnt > 0 && host->id == MSDC_SDIO) {
		disable_irq_nosync(host->eint_irq);
		disable_irq_wake(host->eint_irq);
		host->sdio_irq_cnt--;
	}
	sdr_set_bits(host->base + SDC_CFG, SDC_CFG_SDIOIDE);

	return 0;
}

static int __maybe_unused msdc_suspend(struct device *dev)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	int ret;

	if (mmc->caps2 & MMC_CAP2_CQE) {
		ret = cqhci_suspend(mmc);
		if (ret)
			return ret;
	}

	return pm_runtime_force_suspend(dev);
}

static int __maybe_unused msdc_resume(struct device *dev)
{
	return pm_runtime_force_resume(dev);
}

static const struct dev_pm_ops msdc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msdc_suspend, msdc_resume)
	SET_RUNTIME_PM_OPS(msdc_runtime_suspend, msdc_runtime_resume, NULL)
};

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.driver = {
		.name = "mtk-msdc",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = msdc_of_ids,
		.pm = &msdc_dev_pm_ops,
	},
};

module_platform_driver(mt_msdc_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
