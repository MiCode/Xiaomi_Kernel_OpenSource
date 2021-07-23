/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include "nandx_util.h"
#include "nandx_errno.h"
#include "nandx_device_info.h"
#include "nfc_aries.h"
#include "nfc_core.h"

static inline void nfc_dump_register(struct nfc_info *info)
{
	pr_info("NFI_CNFG = 0x%x\nNFI_PAGEFMT = 0x%x\nNFI_CON = 0x%x\n",
		nfi_readl(info, NFI_CNFG), nfi_readl(info, NFI_PAGEFMT),
		nfi_readl(info, NFI_CON));
	pr_info
	    ("NFI_ACCCON = 0x%x\nNFI_INTR_EN = 0x%x\nNFI_INTR_STA = 0x%x\n",
	     nfi_readl(info, NFI_ACCCON), nfi_readl(info, NFI_INTR_EN),
	     nfi_readl(info, NFI_INTR_STA));
	pr_info("NFI_CMD = 0x%x\nNFI_ADDRNOB = 0x%x\nNFI_COLADDR = 0x%x\n",
		nfi_readl(info, NFI_CMD), nfi_readl(info, NFI_ADDRNOB),
		nfi_readl(info, NFI_COLADDR));
	pr_info
	    ("NFI_ROWADDR = 0x%x\nNFI_CNRNB = 0x%x\nNFI_PIO_DIRDY = 0x%x\n",
	     nfi_readl(info, NFI_ROWADDR), nfi_readl(info, NFI_CNRNB),
	     nfi_readl(info, NFI_PIO_DIRDY));
	pr_info("NFI_STA = 0x%x\nNFI_ADDRCNTR = 0x%x\nNFI_STRADDR = 0x%x\n",
		nfi_readl(info, NFI_STA), nfi_readl(info, NFI_ADDRCNTR),
		nfi_readl(info, NFI_STRADDR));
	pr_info("NFI_BYTELEN = 0x%x\nNFI_CSEL = 0x%x\nNFI_FDML0 = 0x%x\n",
		nfi_readl(info, NFI_BYTELEN), nfi_readl(info, NFI_CSEL),
		nfi_readl(info, NFI_FDML(0)));
	pr_info
	    ("NFI_FDMM0 = 0x%x\nNFI_DEBUG_CON1 = 0x%x\nNFI_MASTER_STA = 0x%x\n",
	     nfi_readl(info, NFI_FDMM(0)), nfi_readl(info, NFI_DEBUG_CON1),
	     nfi_readl(info, NFI_MASTER_STA));
	pr_info("NFI_SECCUS_SIZE = 0x%x\nNFI_RANDOM_CNFG = 0x%x\n",
		nfi_readl(info, NFI_SECCUS_SIZE),
		nfi_readl(info, NFI_RANDOM_CNFG));
	pr_info("NFI_EMPTY_THRESH = 0x%x\nNFI_NAND_TYPE_CNFG = 0x%x\n",
		nfi_readl(info, NFI_EMPTY_THRESH),
		nfi_readl(info, NFI_NAND_TYPE_CNFG));
	pr_info("NFI_ACCCON1 = 0x%x\nNFI_DELAY_CTRL = 0x%x\n",
		nfi_readl(info, NFI_ACCCON1),
		nfi_readl(info, NFI_DELAY_CTRL));
	pr_info("NFI_TLC_RD_WHR2 = 0x%x\n", nfi_readl(info, NFI_TLC_RD_WHR2));
	pr_info
	    ("ECC_ENCCON = 0x%x\nECC_ENCCNFG = 0x%x\nECC_ENCDIADDR = 0x%x\n",
	     ecc_readl(info, ECC_ENCCON), ecc_readl(info, ECC_ENCCNFG),
	     ecc_readl(info, ECC_ENCDIADDR));
	pr_info
	    ("ECC_ENCIDLE = 0x%x\nECC_ENCSTA = 0x%x\nECC_ENCIRQ_EN = 0x%x\n",
	     ecc_readl(info, ECC_ENCIDLE), ecc_readl(info, ECC_ENCSTA),
	     ecc_readl(info, ECC_ENCIRQ_EN));
	pr_info
	    ("ECC_ENCIRQ_STA = 0x%x\nECC_PIO_DIRDY = 0x%x\nECC_DECCON = 0x%x\n",
	     ecc_readl(info, ECC_ENCIRQ_STA), ecc_readl(info, ECC_PIO_DIRDY),
	     ecc_readl(info, ECC_DECCON));
	pr_info
	    ("ECC_DECCNFG = 0x%x\nECC_DECDIADDR = 0x%x\nECC_DECIDLE = 0x%x\n",
	     ecc_readl(info, ECC_DECCNFG), ecc_readl(info, ECC_DECDIADDR),
	     ecc_readl(info, ECC_DECIDLE));
	pr_info
	    ("ECC_DECENUM0 = 0x%x\nECC_DECDONE = 0x%x\nECC_DECIRQ_EN = 0x%x\n",
	     ecc_readl(info, ECC_DECENUM(0)), ecc_readl(info, ECC_DECDONE),
	     ecc_readl(info, ECC_DECIRQ_EN));
	pr_info("ECC_DECIRQ_STA = 0x%x\nECC_DECFSM = 0x%x\n",
		ecc_readl(info, ECC_DECIRQ_STA), ecc_readl(info, ECC_DECFSM));
}

static inline void ecc_wait_idle(struct nfc_info *info, enum ECC_OPERATION op)
{
	int ret;
	u32 reg;

	ret =
	    readl_poll_timeout_atomic(info->res->ecc_regs + ECC_IDLE_REG(op),
				      reg, reg & ECC_IDLE_MASK, 2,
				      ECC_TIMEOUT);
	if (ret) {
		pr_err("%s NOT idle\n",
		       op == ECC_ENCODE ? "encoder" : "decoder");
		nfc_dump_register(info);
	}
}

static inline void ecc_wait_ioready(struct nfc_info *info)
{
	int ret;
	u32 reg;

	ret = readl_poll_timeout_atomic(info->res->ecc_regs + ECC_PIO_DIRDY,
					reg, reg & PIO_DI_RDY, 2,
					ECC_TIMEOUT);
	if (ret)
		pr_err("ecc io not ready\n");
}

static int ecc_runtime_config(struct nfc_info *info)
{
	struct ecc_config *cfg = &info->ecccfg;
	u32 i, ecc_bit, dec_sz, enc_sz, reg;

	for (i = 0; i < info->ecc_strength_num; i++) {
		if (info->ecc_strength[i] == cfg->strength)
			break;
	}
	if (i == info->ecc_strength_num) {
		pr_err("invalid ecc strength %d\n",
		       info->format.ecc_strength);
		return -EINVAL;
	}

	ecc_bit = i;

	reg = ecc_bit | (cfg->mode << ECC_MODE_SHIFT);
	if (cfg->op == ECC_ENCODE) {
		/* configure ECC encoder (in bits) */
		enc_sz = cfg->len << 3;
		reg |= (enc_sz << ECC_MS_SHIFT);
	} else {
		/* configure ECC decoder (in bits) */
		dec_sz = (cfg->len << 3) + cfg->strength * ECC_PARITY_BITS;
		reg |= dec_sz << ECC_MS_SHIFT;
		reg |= cfg->deccon << DEC_CON_SHIFT;
		reg |= DEC_EMPTY_EN;
	}

	ecc_writel(info, reg, ECC_CNFG_REG(cfg->op));

	if (cfg->mode == ECC_DMA_MODE) {
		if (cfg->addr & 0x3)
			pr_err("%s: (0x%x) not 4B align\n",
			       __func__, cfg->addr);
		ecc_writel(info, cfg->addr, ECC_DIADDR_REG(cfg->op));
	}

	return 0;
}

static int ecc_enable(struct nfc_info *info)
{
	enum ECC_OPERATION op = info->ecccfg.op;
	u16 reg_val;
	int ret;

	ecc_wait_idle(info, op);
	ret = ecc_runtime_config(info);
	if (ret)
		return ret;

	/*
	 * Now we only enable ecc irq if it supports pg_irq_sel
	 * and dma mode is on.
	 */
	if (info->mode.irq_en && info->mode.dma_en && info->pg_irq_sel
	    && (info->ecccfg.mode != ECC_NFI_MODE || op != ECC_ENCODE)) {
		nand_event_init(info->ecc_done);
		reg_val = ECC_IRQ_EN;

		/*
		 * For ECC_NFI_MODE, if handler->pg_irq_sel is 1, then it
		 * means this chip can only generate one ecc irq during page
		 * read / write. If is 0, generate one ecc irq each ecc step.
		 */
		if (info->pg_irq_sel && info->ecccfg.mode == ECC_NFI_MODE)
			reg_val |= ECC_PG_IRQ_SEL;
		ecc_writew(info, reg_val, ECC_IRQ_REG(op));
	}

	ecc_writew(info, ECC_OP_ENABLE, ECC_CTL_REG(op));

	return 0;
}

static int ecc_disable(struct nfc_info *info)
{
	enum ECC_OPERATION op = info->ecccfg.op;

	if (info->mode.irq_en) {
		if (op == ECC_DECODE)
			ecc_readw(info, ECC_DECIRQ_STA);
		ecc_writew(info, 0, ECC_IRQ_REG(op));
	}
	ecc_writew(info, ECC_OP_DISABLE, ECC_CTL_REG(op));

	return 0;
}

static int ecc_wait_done(struct nfc_info *info)
{
	enum ECC_OPERATION op = info->ecccfg.op;
	int ret = 0;
	u32 reg;
	void *ecc_regs = info->res->ecc_regs;

	if (ecc_readw(info, ECC_IRQ_REG(op))) {
		ret = nand_event_wait_complete(info->ecc_done, ECC_TIMEOUT);
		if (!ret)
			goto timeout;
	} else {
		if (op == ECC_ENCODE) {
			ret = readl_poll_timeout_atomic(ecc_regs + ECC_ENCSTA,
							reg, reg & ENC_IDLE,
							2, ECC_TIMEOUT);
			if (ret)
				goto timeout;
		} else {
			ret =
			    readw_poll_timeout_atomic(ecc_regs + ECC_DECDONE,
						      reg,
						      info->ecccfg.sectors
						      & reg,
						      2, ECC_TIMEOUT);
			if (ret)
				goto timeout;
		}
	}

	/* Wait for ECC decode state machine idle */
	if (op == ECC_DECODE && info->mode.dma_en) {
		ret = readl_poll_timeout_atomic(ecc_regs + ECC_DECFSM, reg,
						(reg & info->decfsm_mask) ==
						info->decfsm_idle, 2,
						ECC_TIMEOUT);
		if (ret)
			goto timeout;
	}

	return 0;

timeout:
	pr_err("%s: %s timeout!\n",
	       __func__, op == ECC_ENCODE ? "encode" : "decode");
	nfc_dump_register(info);

	return -ETIMEDOUT;
}

static bool nfc_flash_macro_is_idle(struct nfc_info *info)
{
	int ret;
	u32 reg;
	void *nfi_regs = info->res->nfi_regs;

	ret = readl_poll_timeout_atomic(nfi_regs + NFI_STA, reg,
					reg & FLASH_MACRO_IDLE, 2,
					MTK_TIMEOUT);
	if (ret)
		pr_err("wait flash macro idle timeout!\n");

	return ret ? false : true;
}

static inline void nfc_wait_ioready(struct nfc_info *info)
{
	int ret;
	u32 reg;
	void *nfi_regs = info->res->nfi_regs;

	ret = readl_poll_timeout_atomic(nfi_regs + NFI_PIO_DIRDY, reg,
					reg & PIO_DI_RDY, 2, MTK_TIMEOUT);
	if (ret)
		pr_err("data not ready\n");
}

static void nfc_read_fdm(struct nfc_handler *handler, u8 *fdm, u32 sectors)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 vall, valm, i, j;

	for (i = 0; i < sectors; i++) {
		vall = nfi_readl(info, NFI_FDML(i));
		valm = nfi_readl(info, NFI_FDMM(i));
		for (j = 0; j < handler->fdm_size; j++)
			*fdm++ = (j >= 4 ? valm : vall) >> ((j % 4) * 8);
	}
}

static void nfc_write_fdm(struct nfc_handler *handler, u8 *fdm)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 vall, valm, i, j;
	u32 sectors = info->format.page_size / handler->sector_size;

	for (i = 0; i < sectors; i++) {
		vall = 0;
		valm = 0;
		for (j = 0; j < 8; j++) {
			if (j < 4)
				vall |=
				    (j < handler->fdm_size ? *fdm++ : 0xff)
				    << (j * 8);
			else
				valm |=
				    (j < handler->fdm_size ? *fdm++ : 0xff)
				    << ((j - 4) * 8);
		}
		nfi_writel(info, vall, NFI_FDML(i));
		nfi_writel(info, valm, NFI_FDMM(i));
	}
}

static void nfc_hw_reset(struct nfc_info *info)
{
	int ret;
	u32 reg;
	void *nfi_regs = info->res->nfi_regs;

	/* reset all registers and force the NFI master to terminate */
	nfi_writel(info, CON_FIFO_FLUSH | CON_NFI_RST, NFI_CON);

	/* wait for the master to finish the last transaction */
	ret = readl_poll_timeout_atomic(nfi_regs + NFI_MASTER_STA, reg,
					!(reg & MASTER_STA_MASK), 2,
					MTK_RESET_TIMEOUT);
	if (ret)
		pr_err("NFI HW reset timeout!\n");

	/* ensure any status register affected by the NFI master is reset */
	nfi_writel(info, CON_FIFO_FLUSH | CON_NFI_RST, NFI_CON);
	nfi_writew(info, STAR_DE, NFI_STRDATA);
}

static void nfc_hw_init(struct nfc_info *info)
{
	u32 reg;

	nfi_writel(info, PAGEFMT_8K_16K, NFI_PAGEFMT);

	nfi_writel(info, NAND_TYPE_ASYNC, NFI_NAND_TYPE_CNFG);

	nfi_writel(info, 0x10804222, NFI_ACCCON);

	reg = nfi_readl(info, NFI_DEBUG_CON1);
	reg &= ~(BYPASS_MASTER_EN | ECC_CLK_EN | AUTOC_SRAM_MODE);
	nfi_writel(info, reg, NFI_DEBUG_CON1);

	nfc_hw_reset(info);

	nfi_readl(info, NFI_INTR_STA);
	nfi_writel(info, 0, NFI_INTR_EN);

	/* ecc init */
	ecc_wait_idle(info, ECC_ENCODE);
	ecc_writew(info, ECC_OP_DISABLE, ECC_ENCCON);

	ecc_wait_idle(info, ECC_DECODE);
	ecc_writel(info, ECC_OP_DISABLE, ECC_DECCON);

	reg = ecc_readl(info, ECC_BYPASS);
	reg &= ~ECC_BYPASS_EN;
	ecc_writel(info, reg, ECC_BYPASS);
}

static int nfc_setup_ecc_clk(struct nfc_info *info,
			     struct nfc_frequency *freq)
{
	int i, temp = 0;
	u32 rate = 0;

	/* just enable ecc clk if this IP supports individual ecc clk */
	if (!info->ecc_clk_en) {
		pr_warn("%s: not support ecc clk\n", __func__);
		return 0;
	}

	/* 8167 issue */
	if (info->res->ver == NANDX_MT8167 && !info->mode.dma_en) {
		pr_warn("%s: do not enable ecc clk\n", __func__);
		return 0;
	}

	if (freq->ecc_clk_num < 0 || freq->sel_ecc_idx >= freq->ecc_clk_num) {
		pr_warn("%s: invalid clk, num %d, idx %d\n",
			__func__, freq->ecc_clk_num, freq->sel_ecc_idx);
		return -EINVAL;
	}

	if (freq->sel_ecc_idx < 0) {
		for (i = 0; i < freq->ecc_clk_num; i++) {
			if (freq->freq_ecc[i] <= rate)
				continue;
			rate = freq->freq_ecc[i];
			temp = i;
		}
		freq->sel_ecc_idx = temp;
	}

	temp = nfi_readl(info, NFI_DEBUG_CON1);
	temp |= ECC_CLK_EN;
	nfi_writel(info, temp, NFI_DEBUG_CON1);

	return 0;
}

static int nfc_change_legacy_interface(struct nfc_info *info,
				       struct nand_timing *timing,
				       struct nfc_frequency *freq)
{
	struct nandx_legacy_timing *legacy = timing->legacy;
	u32 rate, tpoecs, tprecs, tc2r, tw2r, twh, twst, trlt, tstrobe = 0;
	u32 reg = 0;

	if (!nfc_flash_macro_is_idle(info))
		return -ETIMEDOUT;

	/* There is a frequency divider in some IPs */
	info->nfi_clk_freq = freq->freq_async;
	rate = info->nfi_clk_freq;
	rate /= info->nfi_clk_div;

	/* turn clock rate into KHZ */
	rate /= 1000;

	tpoecs = MAX(legacy->tALH, legacy->tCLH);
	tpoecs = div_up(tpoecs * rate, 1000000);
	tpoecs &= 0xf;

	tprecs = MAX(legacy->tCLS, legacy->tALS);
	tprecs = div_up(tprecs * rate, 1000000);
	tprecs &= 0x3f;

	/* tCR which means CE# low to RE# low */
	tc2r = div_up(legacy->tCR * rate, 1000000);
	/*
	 * TODO: This is a correct case but running is not correct.
	 * tc2r = div_up(tc2r - 1, 2);
	 * tc2r &= 0x3f;
	 */

	tw2r = legacy->tWHR;
	tw2r = div_up(tw2r * rate, 1000000);
	tw2r = div_up(tw2r - 1, 2);
	tw2r &= 0xf;

	twh = MAX(legacy->tREH, legacy->tWH);
	twh = div_up(twh * rate, 1000000) - 1;
	twh &= 0xf;

	twst = legacy->tWP;
	twst = div_up(twst * rate, 1000000) - 1;
	twst &= 0xf;

	trlt = div_up(legacy->tRP * rate, 1000000) - 1;
	trlt &= 0xf;

	/* If tREA is bigger than tRP, setup strobe sel here */
	if ((trlt + 1) * 1000000 / rate < legacy->tREA) {
		tstrobe = legacy->tREA - (trlt + 1) * 1000000 / rate;
		tstrobe = div_up(tstrobe * rate, 1000000);
		reg = nfi_readl(info, NFI_DEBUG_CON1);
		reg &= ~STROBE_MASK;
		reg |= tstrobe << STROBE_SHIFT;
		nfi_writel(info, reg, NFI_DEBUG_CON1);
	}
	/*
	 * ACCON: access timing control register
	 * -------------------------------------
	 * 31:28: tpoecs, minimum required time for CS post pulling down after
	 *        accessing the device
	 * 27:22: tprecs, minimum required time for CS pre pulling down before
	 *        accessing the device
	 * 21:16: tc2r, minimum required time from NCEB low to NREB low
	 * 15:12: tw2r, minimum required time from NWEB high to NREB low.
	 * 11:08: twh, write enable hold time
	 * 07:04: twst, write wait states
	 * 03:00: trlt, read wait states
	 */
	trlt = ACCTIMING(tpoecs, tprecs, tc2r, tw2r, twh, twst, trlt);
	nfi_writel(info, trlt, NFI_ACCCON);

	/* set NAND type */
	nfi_writel(info, NAND_TYPE_ASYNC, NFI_NAND_TYPE_CNFG);

	pr_debug("acccon 0x%x, strobe %d\n", trlt, tstrobe);
	return 0;
}

static void nfc_set_delay_ctrl(struct nfc_info *info)
{
	/*
	 * set delay control & mux, take care of this setting.
	 * the registers to set are different on each NFI.
	 */
	if (info->res->ver == NANDX_MT8167) {
		nfi_writel(info, 0xa001, NFI_DELAY_CTRL);
#define MAC_CTL0	(0xf00)
#define MAC_CTL1	(0xf10)
		nwritel(0x3, info->res->top_regs + MAC_CTL0);
		nwritel(0x10, info->res->top_regs + MAC_CTL1);
	} else {
		pr_err("do delay control setting !!\n");
		NANDX_ASSERT(0);
	}
}

static int nfc_change_onfi_interface(struct nfc_info *info,
				     struct nand_timing *timing,
				     struct nfc_frequency *freq)
{
	u32 reg, rate = 0, temp, tclk;
	int i, idx = 0;
	u32 tpoecs, tprecs, tc2r, tw2r, twh, twst, trlt;
	u32 trdpre, trdpst, twrpre, twrpst, twpre, twpst;
	struct nandx_onfi_timing *onfi = timing->ddr.onfi;

	if (freq->nfi_clk_num <= 0)
		return -EINVAL;

	if (freq->sel_2x_idx >= freq->nfi_clk_num)
		return -EINVAL;

	if (!nfc_flash_macro_is_idle(info))
		return -ETIMEDOUT;

	/*
	 * The sample frequency is the same with the pin CLK frequency.
	 * So choose the max frequency supported directly.
	 */
	if (freq->sel_2x_idx < 0) {
		/* frequency index is not assigned by upper layer. */
		tclk = timing->ddr_clock * 1000000 * 2;
		for (i = 0; i < freq->nfi_clk_num; i++) {
			if (freq->freq_2x[i] > tclk ||
			    freq->freq_2x[i] <= rate)
				continue;
			rate = freq->freq_2x[i];
			idx = i;
		}
		freq->sel_2x_idx = idx;
	}

	rate = freq->freq_2x[freq->sel_2x_idx];
	/* change rate to 1x clk */
	rate >>= 1;
	info->nfi_clk_freq = rate;

	/* turn clock rate into KHZ */
	rate /= 1000;
	tclk = div_up(1000000, rate);
	twpre = tclk * onfi->tWPRE;
	twpst = tclk * onfi->tWPST;

	/* set NFI_ACCCON */
	tpoecs = 0;
	tprecs = MAX(onfi->tCAD, onfi->tWRCK);
	temp = (3 * 1000000) / (2 * rate);
	if (twpre > temp)
		tprecs = MAX(tprecs, twpre - temp);
	tprecs = div_up(tprecs * rate, 1000000);
	tprecs &= 0xf;
	tc2r = 0;
	tw2r = onfi->tWHR;
	tw2r = div_up(tw2r * rate, 1000000);
	tw2r = div_up(tw2r - 1, 2);
	tw2r &= 0xf;
	twh = twst = trlt = 0;
	reg = ACCTIMING(tpoecs, tprecs, tc2r, tw2r, twh, twst, trlt);
	nfi_writel(info, reg, NFI_ACCCON);

	/* set NFI_ACCCON1 */
	trdpre = 0;
	trdpst = div_up(onfi->tDQSCK + tclk, tclk) - 1;
	trdpst &= 0x3f;
	twrpre = 0;
	twrpst = div_up(twpst * rate, 1000000) - 1;
	twrpst &= 0x1f;
	reg = ACCTIMING1(trdpre, twrpre, trdpst, twrpst);
	nfi_writel(info, reg, NFI_ACCCON1);

	/* set NAND type */
	nfi_writel(info, NAND_TYPE_SYNC, NFI_NAND_TYPE_CNFG);

	nfc_set_delay_ctrl(info);

	nfc_setup_ecc_clk(info, freq);

	return 0;
}

static int nfc_change_toggle_interface(struct nfc_info *info,
				       struct nand_timing *timing,
				       struct nfc_frequency *freq)
{
	return 0;
}

static void ecc_adjust_strength(struct nfc_info *info, u32 *p)
{
	const u8 *ecc_strength = info->ecc_strength;
	int i;

	for (i = 0; i < info->ecc_strength_num; i++) {
		if (*p <= ecc_strength[i]) {
			if (!i)
				*p = ecc_strength[i];
			else if (*p != ecc_strength[i])
				*p = ecc_strength[i - 1];
			return;
		}
	}

	*p = ecc_strength[info->ecc_strength_num - 1];
}

static int ecc_decode_stats(struct nfc_info *info, u32 sectors)
{
	u32 i, val = 0, err;
	u32 bitflips = 0;

	for (i = 0; i < sectors; i++) {
		if ((i % 4) == 0)
			val = ecc_readl(info, ECC_DECENUM(i / 4));
		err = val >> ((i % 4) * 8);
		err &= ERR_MASK;
		if (err == ERR_MASK) {
			/* uncorrectable errors */
			pr_debug("sector %d is uncorrect\n", i);
			return -ENANDREAD;
		}
		bitflips = MAX(bitflips, err);
	}

	return bitflips;
}

/**
 * ecc_cpu_correct - correct error manually, and feedback bitfilps
 * @info: nfc info structure
 * @data: source data buffer
 * @sector: which sector to correct (0 ~ 15)
 *
 * Return -ENANDREAD if uncorrectable ecc happens, otherwise return bitflips.
 */
static int ecc_cpu_correct(struct nfc_handler *handler, u8 *data, u32 sector)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 err, temp, i;
	u32 byteloc, bitloc, reg;
	int ret;

	info->ecccfg.sectors = 1 << sector;
	ret = ecc_wait_done(info);
	if (ret)
		return ret;

	temp = (sector >> 2);
	err = ecc_readl(info, ECC_DECENUM(temp));
	err >>= ((sector % 4) * 8);
	err &= ERR_MASK;
	if (err == ERR_MASK) {
		/* uncorrectable errors */
		return -ENANDREAD;
	}

	for (i = 0; i < err; i++) {
		temp = ecc_readl(info, ecc_decel_reg(info, i >> 1));
		temp >>= ((i & 0x1) << 4);
		temp &= EL_MASK;
		byteloc = temp >> 3;
		bitloc = temp & 0x7;
		if (info->ecccfg.mode != ECC_NFI_MODE) {
			data[byteloc] ^= (1 << bitloc);
			continue;
		}

		if (byteloc >= handler->sector_size
		    && byteloc < (handler->sector_size
				  + handler->fdm_ecc_size)) {
			/* error in fdm */
			byteloc -= handler->sector_size;
			if (byteloc < 4) {
				reg = NFI_FDML(sector);
			} else {
				byteloc -= 4;
				reg = NFI_FDMM(sector);
			}
			temp = nfi_readl(info, reg);
			temp ^= (1 << (bitloc + (byteloc << 3)));
			nfi_writel(info, temp, reg);
		} else if (byteloc < handler->sector_size) {
			/* error in main data */
			data[byteloc] ^= (1 << bitloc);
		}
	}

	return err;
}

enum NIRQ_RETURN nfi_irq_handler(void *arg)
{
	struct nfc_info *info = arg;
	u16 sta, ien;

	sta = nfi_readw(info, NFI_INTR_STA);
	ien = nfi_readw(info, NFI_INTR_EN);
	if (!(sta & ien))
		return NIRQ_NONE;

	nfi_writew(info, ~sta & ien, NFI_INTR_EN);

	nand_event_complete(info->nfi_done);

	return NIRQ_HANDLED;
}

enum NIRQ_RETURN ecc_irq_handler(void *arg)
{
	struct nfc_info *info = arg;
	u32 dec, enc;

	dec = ecc_readw(info, ECC_DECIRQ_STA) & ECC_IRQ_EN;
	if (dec) {
		dec = ecc_readw(info, ECC_DECDONE);
		if ((dec & info->ecccfg.sectors) == 0)
			return NIRQ_NONE;
		/*
		 * Clear decode IRQ status once again to ensure that
		 * there will be no extra IRQ.
		 */
		ecc_readw(info, ECC_DECIRQ_STA);
		info->ecccfg.sectors = 0;
		nand_event_complete(info->ecc_done);
	} else {
		enc = ecc_readl(info, ECC_ENCIRQ_STA) & ECC_IRQ_EN;
		if (enc == 0)
			return NIRQ_NONE;
		nand_event_complete(info->ecc_done);
	}

	return NIRQ_HANDLED;
}

void nfc_send_command(struct nfc_handler *handler, u8 cmd)
{
	struct nfc_info *info = handler_to_info(handler);
	int ret;
	u32 reg;
	void *nfi_regs = info->res->nfi_regs;

	/* reset controller */
	nfc_hw_reset(info);

	/* always use custom mode */
	reg = nfi_readw(info, NFI_CNFG);
	/* clear config setting except randomizer */
	reg &= CNFG_RAND_MASK;
	reg |= CNFG_OP_CUST;
	nfi_writew(info, reg, NFI_CNFG);

	nfi_writel(info, cmd, NFI_CMD);

	ret = readl_poll_timeout_atomic(nfi_regs + NFI_STA, reg,
					!(reg & STA_CMD), 2, MTK_TIMEOUT);
	if (ret)
		pr_err("send cmd 0x%x timeout\n", cmd);
}

void nfc_send_address(struct nfc_handler *handler, u32 col, u32 row,
		      u32 col_cycle, u32 row_cycle)
{
	struct nfc_info *info = handler_to_info(handler);
	int ret;
	u32 reg;
	void *nfi_regs = info->res->nfi_regs;

	nfi_writel(info, col, NFI_COLADDR);
	nfi_writel(info, row, NFI_ROWADDR);
	nfi_writel(info, col_cycle | (row_cycle << ROW_SHIFT), NFI_ADDRNOB);

	ret = readl_poll_timeout_atomic(nfi_regs + NFI_STA, reg,
					!(reg & STA_ADDR), 2, MTK_TIMEOUT);
	if (ret)
		pr_err("send address timeout\n");
}

u8 nfc_read_byte(struct nfc_handler *handler)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 reg;

	/* after each byte read, the NFI_STA reg is reset by the hardware */
	reg = nfi_readl(info, NFI_STA) & NFI_FSM_MASK;
	if (reg != NFI_FSM_CUSTDATA) {
		reg = nfi_readw(info, NFI_CNFG);
		reg |= CNFG_BYTE_RW | CNFG_READ_EN;
		nfi_writew(info, reg, NFI_CNFG);

		/*
		 * set to max sector to allow the HW to continue reading over
		 * unaligned accesses
		 */
		reg = (MTK_MAX_SECTOR << CON_SEC_SHIFT) | CON_BRD | CON_BWR;
		nfi_writel(info, reg, NFI_CON);

		/* trigger to fetch data */
		nfi_writew(info, STAR_EN, NFI_STRDATA);
	}

	nfc_wait_ioready(info);

	return nfi_readb(info, NFI_DATAR);
}

void nfc_write_byte(struct nfc_handler *handler, u8 data)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 reg;

	reg = nfi_readl(info, NFI_STA) & NFI_FSM_MASK;

	if (reg != NFI_FSM_CUSTDATA) {
		reg = nfi_readw(info, NFI_CNFG) | CNFG_BYTE_RW;
		nfi_writew(info, reg, NFI_CNFG);

		reg = MTK_MAX_SECTOR << CON_SEC_SHIFT | CON_BWR | CON_BRD;
		nfi_writel(info, reg, NFI_CON);

		nfi_writew(info, STAR_EN, NFI_STRDATA);
	}

	nfc_wait_ioready(info);
	nfi_writeb(info, data, NFI_DATAW);
}

/**
 * nfc_read_sectors - read data by sector. nand chip layer should send change
 * column address command before calling nfc_read_sectors.
 * @handler: nfc handler structure
 * @num: total sectors to read
 * @data: buffer to store main data. If read without ecc, this buffer layout is
 *        sector data + FDM data + ECC data + dummy data. If read with ecc, this
 *        buffer only stores sector data.
 * @fdm: buffer to store FDM data. If read without ecc, this buffer is useless.
 *       If read with ecc, check whether buffer is NULL. If NULL, do nothing.
 *       If not NULL, store FDM data to this buffer.
 */
int nfc_read_sectors(struct nfc_handler *handler, int num, u8 *data, u8 *fdm)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 reg, dma_addr = 0, len = num * handler->sector_size;
	u32 data_len, j, k;
	bool autofmt = false, irq_en, byterw;
	u8 *buf = data, *data_phy = data;
	int i, ret = 0, bitflips = 0;
	void *nfi_regs = info->res->nfi_regs;

	/* enable irq if ahb_irq_en, irq_en and dma_en are all on */
	irq_en = info->ahb_irq_en && info->mode.irq_en && info->mode.dma_en;

	reg = nfi_readw(info, NFI_CNFG);
	reg |= CNFG_READ_EN;

	if (info->mode.ecc_en) {
		reg |= CNFG_HW_ECC_EN | CNFG_AUTO_FMT_EN;
		/* now we only enable AUTO_FORMAT if ecc is enabled */
		autofmt = true;

		/* setup ecc config */
		info->ecccfg.op = ECC_DECODE;
		info->ecccfg.mode = ECC_NFI_MODE;
		info->ecccfg.deccon = info->mode.dma_en ? ECC_DEC_CORRECT :
		    ECC_DEC_LOCATE;
		info->ecccfg.sectors = 1 << (num - 1);
		info->ecccfg.len =
		    handler->sector_size + handler->fdm_ecc_size;
		info->ecccfg.strength = handler->ecc_strength;
		ret = ecc_enable(info);
		if (ret) {
			pr_err("ecc enable failed!\n");
			return ret;
		};
	}

	if (info->mode.dma_en) {
		if (!virt_addr_valid(data))
			data_phy = info->buf;

		reg |= CNFG_DMA_BURST_EN | CNFG_AHB;

		if (!autofmt)
			len += num * handler->spare_size;
		/* dma address mapping */
		dma_addr = nand_dma_map(info->res->dev, (void *)data_phy,
					(u64)len, NDMA_FROM_DEV);
		nfi_writel(info, dma_addr, NFI_STRADDR);
	}

	nfi_writew(info, reg, NFI_CNFG);

	/* setup read sector number */
	nfi_writel(info, num << CON_SEC_SHIFT, NFI_CON);

	if (irq_en) {
		nand_event_init(info->nfi_done);
		nfi_writew(info, INTR_AHB_DONE_EN, NFI_INTR_EN);
	}

	reg = nfi_readl(info, NFI_CON) | CON_BRD | CON_BWR;
	nfi_writel(info, reg, NFI_CON);
	/* trigger to read */
	nfi_writew(info, STAR_EN, NFI_STRDATA);

	/* TODO: move to a sperate function */
	if (!info->mode.dma_en) {
		data_len = handler->sector_size
		    + (autofmt ? 0 : handler->spare_size);

		if (data_len & 0x3) {
			reg = nfi_readw(info, NFI_CNFG) | CNFG_BYTE_RW;
			nfi_writew(info, reg, NFI_CNFG);
			byterw = true;
		} else {
			data_len >>= 2;
			byterw = false;
		}

		for (i = 0; i < num; i++) {
			for (j = 0; j < data_len; j++) {
				nfc_wait_ioready(info);
				if (!byterw) {
					reg = nfi_readl(info, NFI_DATAR);
					for (k = 0; k < 4; k++)
						*buf++ = reg >> (k * 8);
				} else {
					*buf++ = nfi_readb(info, NFI_DATAR);
				}
			}

			if (info->mode.ecc_en) {
				ret = ecc_cpu_correct(handler, data + i *
						      handler->sector_size,
						      i);
				if (ret < 0)
					bitflips = ret;
				else if (bitflips >= 0)
					bitflips = MAX(bitflips, ret);
			}
		}

		goto emptycheck;
	}

	if (irq_en) {
		ret = nand_event_wait_complete(info->nfi_done, MTK_TIMEOUT);
		if (!ret) {
			pr_err("read dma done timeout!\n");
			nfc_dump_register(info);
			nfi_writew(info, 0, NFI_INTR_EN);
			bitflips = -ETIMEDOUT;
			goto dmaunmap;
		}
	}

	ret = readl_poll_timeout_atomic(nfi_regs + NFI_BYTELEN, reg,
					ADDRCNTR_SEC(reg) >= (u32)num, 2,
					MTK_TIMEOUT);
	/* HW issue: if not wait ahb done, need polling bus busy extra */
	if (ret == 0 && !irq_en)
		ret = readl_poll_timeout_atomic(nfi_regs + NFI_MASTER_STA,
						reg, !(reg & MASTER_BUS_BUSY),
						2, MTK_TIMEOUT);
	if (ret) {
		pr_err("wait bytelen timeout %d\n",
		       nfi_readl(info, NFI_BYTELEN));
		pr_err("cnfg 0x%x fmt 0x%x\n con 0x%x",
		       nfi_readl(info, NFI_CNFG),
		       nfi_readl(info, NFI_PAGEFMT),
		       nfi_readl(info, NFI_CON));
		nfc_dump_register(info);
		bitflips = -ETIMEDOUT;
		goto dmaunmap;
	}

	if (info->mode.ecc_en && info->mode.dma_en) {
		ret = ecc_wait_done(info);
		if (ret) {
			bitflips = ret;
			goto dmaunmap;
		}
		if (fdm)
			nfc_read_fdm(handler, fdm, num);
		bitflips = ecc_decode_stats(info, num);
	}

dmaunmap:
	if (info->mode.dma_en) {
		nand_dma_unmap(info->res->dev, data_phy, dma_addr, (u64)len,
			       NDMA_FROM_DEV);
		if (data_phy != data)
			memcpy(data, data_phy, len);
	}

emptycheck:
	if (info->mode.ecc_en) {
		ret = nfi_readl(info, NFI_STA) & STA_EMP_PAGE;
		if (ret) {
			pr_debug("empty page!\n");
			memset(data, 0xff, num * handler->sector_size);
			if (fdm)
				memset(fdm, 0xff, num * handler->fdm_size);
			bitflips = 0;
		}
	}

	/* disable nfi */
	nfi_writel(info, 0, NFI_CON);
	/* disable ecc */
	if (info->mode.ecc_en)
		ecc_disable(info);

	return bitflips;
}

/**
 * nfc_write_page - write one page data.
 * @handler: nfc handler structure
 * @data: buffer that contains main data. If write without ecc, this buffer
 * layout is sector data + FDM data + ECC data + dummy data. If write with ecc,
 * this buffer only contains page data.
 * @fdm: buffer that contains store FDM data. If write without ecc, this buffer
 * is useless. If write with ecc, check whether buffer is NULL. If NULL, do
 * nothing. If not NULL, program FDM registers according to this buffer.
 */
int nfc_write_page(struct nfc_handler *handler, u8 *data, u8 *fdm)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 sectors = info->format.page_size / handler->sector_size;
	u32 reg, data_len, dma_addr = 0, i;
	u32 len = info->format.page_size;
	bool autofmt = false, irq_en, byterw;
	u8 *buf = data, *data_phy = data;
	u32 *buf32 = (u32 *)buf;
	int ret;
	void *nfi_regs = info->res->nfi_regs;

	/* enable irq if ahb_irq_en, irq_en and dma_en are all on */
	irq_en = info->ahb_irq_en && info->mode.irq_en && info->mode.dma_en;

	if (info->mode.ecc_en) {
		/* If ecc is enabled, set AUTO_FORMAT */
		reg = nfi_readw(info, NFI_CNFG) | CNFG_AUTO_FMT_EN;
		nfi_writew(info, reg | CNFG_HW_ECC_EN, NFI_CNFG);
		autofmt = true;

		/* setup ecc config */
		info->ecccfg.op = ECC_ENCODE;
		info->ecccfg.mode = ECC_NFI_MODE;
		info->ecccfg.len =
		    handler->sector_size + handler->fdm_ecc_size;
		info->ecccfg.strength = handler->ecc_strength;
		ret = ecc_enable(info);
		if (ret) {
			pr_err("%s: ecc enable fail!\n", __func__);
			return ret;
		}

		if (fdm)
			nfc_write_fdm(handler, fdm);
	}

	if (info->mode.dma_en) {
		reg = nfi_readw(info, NFI_CNFG);
		reg |= CNFG_DMA_BURST_EN | CNFG_AHB;
		nfi_writew(info, reg, NFI_CNFG);

		if (!autofmt)
			len += sectors * handler->spare_size;

		if (!virt_addr_valid(data)) {
			data_phy = info->buf;
			memcpy(data_phy, data, len);
		}

		/* dma address mapping */
		dma_addr = nand_dma_map(info->res->dev, (void *)data_phy,
					(u64)len, NDMA_TO_DEV);
		nfi_writel(info, dma_addr, NFI_STRADDR);
	}

	nfi_writel(info, sectors << CON_SEC_SHIFT, NFI_CON);

	if (irq_en) {
		nand_event_init(info->nfi_done);
		nfi_writew(info, INTR_AHB_DONE_EN, NFI_INTR_EN);
	}

	reg = nfi_readl(info, NFI_CON) | CON_BWR | CON_BRD;
	nfi_writel(info, reg, NFI_CON);
	/* trigger to write */
	nfi_writew(info, STAR_EN, NFI_STRDATA);

	if (!info->mode.dma_en) {
		data_len = handler->sector_size
		    + (autofmt ? handler->spare_size : 0);
		data_len *= sectors;

		if (data_len & 0x3) {
			reg = nfi_readw(info, NFI_CNFG) | CNFG_BYTE_RW;
			nfi_writew(info, reg, NFI_CNFG);
			byterw = true;
		} else {
			data_len >>= 2;
			byterw = false;
		}

		for (i = 0; i < data_len; i++) {
			nfc_wait_ioready(info);
			if (!byterw)
				nfi_writel(info, buf32[i], NFI_DATAW);
			else
				nfi_writeb(info, buf[i], NFI_DATAW);
		}
	}

	if (irq_en) {
		ret = nand_event_wait_complete(info->nfi_done, MTK_TIMEOUT);
		if (!ret) {
			pr_err("%s: dma timeout!\n", __func__);
			nfc_dump_register(info);
			nfi_writew(info, 0, NFI_INTR_EN);
		}
	}

	ret = readl_poll_timeout_atomic(nfi_regs + NFI_ADDRCNTR, reg,
					ADDRCNTR_SEC(reg) >= sectors, 2,
					MTK_TIMEOUT);
	if (ret) {
		pr_err("do page write timeout\n");
		nfc_dump_register(info);
	}

	if (info->mode.dma_en)
		nand_dma_unmap(info->res->dev, data_phy, dma_addr, (u64)len,
			       NDMA_TO_DEV);

	/* disable nfi */
	nfi_writel(info, 0, NFI_CON);

	/* disable ecc */
	ecc_disable(info);

	return ret;
}

int nfc_change_interface(struct nfc_handler *handler,
			 enum INTERFACE_TYPE type,
			 struct nand_timing *timing, void *arg)
{
	struct nfc_info *info = handler_to_info(handler);
	struct nfc_frequency *freq;
	int ret;

	freq = (struct nfc_frequency *)arg;

	switch (type) {
	case INTERFACE_LEGACY:
		ret = nfc_change_legacy_interface(info, timing, freq);
		break;
	case INTERFACE_ONFI:
		ret = nfc_change_onfi_interface(info, timing, freq);
		break;
	case INTERFACE_TOGGLE:
		ret = nfc_change_toggle_interface(info, timing, freq);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int nfc_change_mode(struct nfc_handler *handler,
		    enum OPS_MODE_TYPE mode, bool enable, void *arg)
{
	struct nfc_info *info = handler_to_info(handler);

	switch (mode) {
	case OPS_MODE_DMA:
		info->mode.dma_en = enable;
		break;
	case OPS_MODE_IRQ:
		info->mode.irq_en = enable;
		break;
	case OPS_MODE_ECC:
		info->mode.ecc_en = enable;
		break;
	default:
		pr_err("not support mode: %d\n", mode);
		break;
	}

	return 0;
}

bool nfc_get_mode(struct nfc_handler *handler, enum OPS_MODE_TYPE mode)
{
	struct nfc_info *info = handler_to_info(handler);
	bool enable = false;

	switch (mode) {
	case OPS_MODE_DMA:
		enable = info->mode.dma_en;
		break;
	case OPS_MODE_IRQ:
		enable = info->mode.irq_en;
		break;
	case OPS_MODE_ECC:
		enable = info->mode.ecc_en;
		break;
	default:
		pr_err("not support mode: %d\n", mode);
		break;
	}

	return enable;
}

void nfc_select_chip(struct nfc_handler *handler, int cs)
{
	struct nfc_info *info = handler_to_info(handler);

	if (cs < 0 || cs >= info->max_cs)
		return;

	nfi_writel(info, cs, NFI_CSEL);
}

int nfc_set_format(struct nfc_handler *handler, struct nfc_format *format)
{
	struct nfc_info *info = handler_to_info(handler);
	u32 fmt, min_fdm, min_ecc, max_ecc;
	u32 ecc_strength = format->ecc_strength;

	info->buf = mem_alloc(1, format->page_size + format->oob_size);
	if (info->buf == NULL)
		return -ENOMEM;
	/* setup sector_size according to the min oob required */
	if (MIN_OOB_REQUIRED / FDM_MAX_SIZE > format->page_size / 1024) {
		if (format->page_size / 512 < MIN_OOB_REQUIRED / FDM_MAX_SIZE)
			return -EINVAL;
		handler->sector_size = 512;
		/* format->ecc_strength is the requirement per 1KB */
		ecc_strength >>= 1;
	} else {
		handler->sector_size = 1024;
	}

	/* calculate spare size */
	handler->spare_size = format->page_size / handler->sector_size;
	handler->spare_size = format->oob_size / handler->spare_size;

	/* calculate ecc strength and fdm size */
	min_ecc = (handler->spare_size - FDM_MAX_SIZE) * 8 / ECC_PARITY_BITS;
	ecc_adjust_strength(info, &min_ecc);
	min_fdm = format->page_size / handler->sector_size;
	min_fdm = div_up(MIN_OOB_REQUIRED, min_fdm);
	max_ecc = (handler->spare_size - min_fdm) * 8 / ECC_PARITY_BITS;
	ecc_adjust_strength(info, &max_ecc);
	min_fdm = format->page_size / handler->sector_size;
	min_fdm -= div_up(max_ecc * ECC_PARITY_BITS, 8);
	if (ecc_strength > max_ecc) {
		pr_warn("required ecc strength %d, max supported %d\n",
			ecc_strength, max_ecc);
		handler->ecc_strength = max_ecc;
		handler->fdm_size = min_fdm;
	} else if (format->ecc_strength < min_ecc) {
		handler->ecc_strength = min_ecc;
		handler->fdm_size = FDM_MAX_SIZE;
	} else {
		ecc_adjust_strength(info, &ecc_strength);
		handler->ecc_strength = ecc_strength;
		handler->fdm_size = div_up(ecc_strength * ECC_PARITY_BITS, 8);
		handler->fdm_size = handler->spare_size - handler->fdm_size;
	}
	/*
	 * now assign fdm ecc size the same with fdm size.
	 * different with upstream.
	 */
	handler->fdm_ecc_size = handler->fdm_size;

	switch (format->page_size) {
	case 512:
		fmt = PAGEFMT_512_2K | PAGEFMT_SEC_SEL_512;
		break;
	case KB(2):
		if (handler->sector_size == 512)
			fmt = PAGEFMT_2K_4K | PAGEFMT_SEC_SEL_512;
		else
			fmt = PAGEFMT_512_2K;
		break;
	case KB(4):
		if (handler->sector_size == 512)
			fmt = PAGEFMT_4K_8K | PAGEFMT_SEC_SEL_512;
		else
			fmt = PAGEFMT_2K_4K;
		break;
	case KB(8):
		if (handler->sector_size == 512)
			fmt = PAGEFMT_8K_16K | PAGEFMT_SEC_SEL_512;
		else
			fmt = PAGEFMT_4K_8K;
		break;
	case KB(16):
		fmt = PAGEFMT_8K_16K;
		break;
	default:
		pr_err("invalid page len: %d\n", format->page_size);
		return -EINVAL;
	}

	fmt |= handler->fdm_size << PAGEFMT_FDM_SHIFT;
	fmt |= handler->fdm_ecc_size << PAGEFMT_FDM_ECC_SHIFT;
	nfi_writel(info, fmt, NFI_PAGEFMT);

	/* setup customized sector size */
	fmt = handler->spare_size + handler->sector_size;
	fmt |= SECCUS_SIZE_EN;
	nfi_writel(info, fmt, NFI_SECCUS_SIZE);

	/* restore nand device format info */
	memcpy(&info->format, format, sizeof(struct nfc_format));

	return 0;
}

/**
 * nfc_enable_randomizer - enable randomizer function
 * @handler: nfc handler
 * @page: page index in one block
 */
void nfc_enable_randomizer(struct nfc_handler *handler, u32 page, bool encode)
{
	struct nfc_info *info = handler_to_info(handler);
	enum RANDOMIZER_OPERATION op = RAND_ENCODE;
	u32 reg;

	if (!encode)
		op = RAND_DECODE;

	/* randomizer type and reseed type setup */
	reg = nfi_readl(info, NFI_CNFG);
	reg |= CNFG_RAND_SEL | CNFG_RESEED_SEC_EN;
	nfi_writel(info, reg, NFI_CNFG);

	/* randomizer seed and type setup */
	reg = ss_randomizer_seed[page % SS_SEED_NUM] & RAN_SEED_MASK;
	reg <<= RAND_SEED_SHIFT(op);
	reg |= RAND_EN(op);
	nfi_writel(info, reg, NFI_RANDOM_CNFG);
}

void nfc_disable_randomizer(struct nfc_handler *handler)
{
	struct nfc_info *info = handler_to_info(handler);

	nfi_writel(info, 0, NFI_RANDOM_CNFG);
}

/**
 * nfc_wait_busy - wait ready status
 * @handler: nfc handler
 * @timeout: timeout in us if wait R/B#, in ns if wait tWHR2 or tCCS
 * @type: wait busy type.
 */
int nfc_wait_busy(struct nfc_handler *handler, int timeout,
		  enum WAIT_TYPE type)
{
	struct nfc_info *info = handler_to_info(handler);
	int ret = 1;
	u32 reg, rate;
	void *nfi_regs = info->res->nfi_regs;

	if (info->mode.irq_en && type == IRQ_WAIT_RB) {
		/*
		 * CNRNB: nand ready/busy register
		 * -------------------------------
		 * 7:4: timeout register for polling the NAND busy/ready signal
		 * 0: poll the status of the busy/ready signal
		 *    after [7:4] * 16 cycles.
		 */
		nfi_writew(info, 0xf1, NFI_CNRNB);
		/* set wait busy interrupt */
		nand_event_init(info->nfi_done);
		nfi_writew(info, INTR_BUSY_RETURN_EN, NFI_INTR_EN);
		/*
		 * This is a work-around.
		 * Sometimes busyreturn comes some quickly, then there
		 * will be no irq. So check NFI_STA at first.
		 */
		reg = nfi_readl(info, NFI_STA);
		if (reg & STA_BUSY2READY) {
			ret = 0;
			nfi_readw(info, NFI_INTR_STA);
			nfi_writew(info, 0, NFI_INTR_EN);
			goto done;
		}
		/* wait interrupt */
		ret = nand_event_wait_complete(info->nfi_done, timeout);
		ret = ret > 0 ? 0 : -ETIMEDOUT;
	} else if (type == IRQ_WAIT_RB || type == POLL_WAIT_RB) {
		nfi_writew(info, 0x21, NFI_CNRNB);
		ret = readl_poll_timeout_atomic(nfi_regs + NFI_STA, reg,
						reg & STA_BUSY2READY, 2,
						timeout);
	} else if (type == POLL_WAIT_TWHR2) {
		/* disable tWHR2 wait at first */
		nfi_writel(info, 0, NFI_TLC_RD_WHR2);

		/* There is a frequency divider in some IPs */
		rate = info->nfi_clk_freq;
		rate /= info->nfi_clk_div;
		/* turn clock rate into KHZ */
		rate /= 1000;
		rate = div_up(timeout * rate, 1000000);
		rate++;
		rate &= TLC_RD_WHR2_MASK;
		reg = TLC_RD_WHR2_EN;
		reg |= rate;
		nfi_writel(info, reg, NFI_TLC_RD_WHR2);
		ret = 0;
	}

	if (ret) {
		if (info->mode.irq_en && type == IRQ_WAIT_RB) {
			nfi_writew(info, 0, NFI_INTR_EN);
			/* workaround busy2return */
			ret = readl_poll_timeout_atomic(
					nfi_regs + NFI_STA, reg,
					reg & STA_BUSY2READY, 2,
					timeout);
			if (!ret)
				goto done;
		}
		pr_err("wait busy(%d) timeout!\n", type);
		nfc_dump_register(info);
	}

done:
	if (type == IRQ_WAIT_RB || type == POLL_WAIT_RB)
		nfi_writew(info, 0, NFI_CNRNB);

	return ret;
}

/**
 * nfc_calculate_ecc - calculate ecc parity data
 * @handler: nfc handler
 * @data: buffer that contains main data
 * @ecc: buffer that contains ecc parity data
 * @len: buffer length
 * @ecc_strength: ecc strength
 */
int nfc_calculate_ecc(struct nfc_handler *handler, u8 *data, u8 *ecc, u32 len,
		      u8 ecc_strength)
{
	struct nfc_info *info = handler_to_info(handler);
	struct ecc_config *cfg = &info->ecccfg;
	u32 dma_addr = 0, i, ecc_len, val = 0;
	u8 *data_phy = data;
	int ret = 0;

	if (info->mode.dma_en) {
		if (!virt_addr_valid(data)) {
			data_phy = info->buf;
			memcpy(data_phy, data, len);
		}

		/* dma address mapping */
		dma_addr = nand_dma_map(info->res->dev, (void *)data_phy,
					(u64)len, NDMA_TO_DEV);
		cfg->mode = ECC_DMA_MODE;
		cfg->addr = dma_addr;
	} else {
		cfg->mode = ECC_PIO_MODE;
	}

	cfg->op = ECC_ENCODE;
	cfg->len = len;
	cfg->strength = ecc_strength;
	ret = ecc_enable(info);
	if (ret)
		goto dmaunmap;

	if (cfg->mode == ECC_PIO_MODE) {
		val = 0;
		/* TODO: move to a sperate function */
		for (i = 0; i < len; i++) {
			val |= data[i] << ((i % 4) * 8);
			if (((i + 1) % 4 == 0) || (i == len - 1)) {
				ecc_wait_ioready(info);
				ecc_writel(info, val, ECC_PIO_DI);
				val = 0;
			}
		}
	}

	ret = ecc_wait_done(info);
	if (ret)
		goto dmaunmap;

	ecc_wait_idle(info, ECC_ENCODE);

	/* get ecc parity data */
	ecc_len = (ecc_strength * ECC_PARITY_BITS + 7) >> 3;
	for (i = 0; i < ecc_len; i++) {
		if ((i % 4) == 0)
			val = ecc_readl(info, ecc_encpar_reg(info, i / 4));
		ecc[i] = (val >> ((i % 4) * 8)) & 0xff;
	}

	ecc_disable(info);

dmaunmap:
	if (info->mode.dma_en)
		nand_dma_unmap(info->res->dev, data_phy, dma_addr, (u64)len,
			       NDMA_TO_DEV);

	return ret;
}

/**
 * nfc_correct_ecc - do ecc correct in DMA/PIO mode
 * @handler: nfc handler
 * @data: buffer that contains main data and parity data
 * @len: buffer length, should contain user data and parity data.
 * @ecc_strength: ecc strength
 */
int nfc_correct_ecc(struct nfc_handler *handler, u8 *data, u32 len,
		    u8 ecc_strength)
{
	struct nfc_info *info = handler_to_info(handler);
	struct ecc_config *cfg = &info->ecccfg;
	u32 dma_addr = 0, i, val;
	u8 *data_phy = data;
	int ret;

	if (info->mode.dma_en) {
		if (!virt_addr_valid(data)) {
			data_phy = info->buf;
			memcpy(data_phy, data, len);
		}

		/* dma address mapping */
		dma_addr = nand_dma_map(info->res->dev, (void *)data,
					(u64)len, NDMA_TO_DEV);
		cfg->mode = ECC_DMA_MODE;
		cfg->addr = dma_addr;
		cfg->deccon = ECC_DEC_CORRECT;
	} else {
		cfg->mode = ECC_PIO_MODE;
		cfg->deccon = ECC_DEC_LOCATE;
	}
	cfg->op = ECC_DECODE;
	cfg->len = len;
	cfg->strength = ecc_strength;
	ret = ecc_enable(info);
	if (ret)
		goto dmaunmap;

	if (cfg->mode == ECC_PIO_MODE) {
		val = 0;
		/* TODO: move to a sperate function */
		for (i = 0; i < len; i++) {
			val |= data[i] << ((i % 4) * 8);
			if (((i + 1) % 4 == 0) || (i == len - 1)) {
				ecc_wait_ioready(info);
				ecc_writel(info, val, ECC_PIO_DI);
				val = 0;
			}
		}

		ret = ecc_cpu_correct(handler, data, 0);
	} else {
		ret = ecc_wait_done(info);
	}

	ecc_disable(info);

dmaunmap:
	if (info->mode.dma_en)
		nand_dma_unmap(info->res->dev, data, dma_addr, (u64)len,
			       NDMA_TO_DEV);
	return ret;
}

int nfc_calibration(struct nfc_handler *handler)
{
	return 0;
}

static const u8 ecc_strength_max80[] = {
	4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36,
	40, 44, 48, 52, 56, 60, 68, 72, 80
};

struct nfc_handler *nfc_setup_hw(struct nfc_resource *res)
{
	struct nfc_info *info;
	int ret;

	info = (struct nfc_info *)mem_alloc(1, sizeof(struct nfc_info));
	if (!info)
		return NULL;
	memset(info, 0, sizeof(struct nfc_info));

	/* default setting */
	info->mode.ecc_en = true;
	info->mode.dma_en = true;
	info->mode.irq_en = false;
	info->ahb_irq_en = false;

	/* setup hw according to pdata */
	info->res = res;

	/* IC capacities setting */
	if (res->ver == NANDX_MT8167) {
		info->max_cs = 2;
		info->nfi_clk_div = 1;
		info->pg_irq_sel = false;
		info->ecc_clk_en = true;
		info->ecc_strength = ecc_strength_max80;
		info->ecc_strength_num = 23;
		info->decfsm_mask = 0x3f3fff0f;
		info->decfsm_idle = 0x01011101;
	}

	/* irq register */
	ret = nand_irq_register(res->nfi_irq, nfi_interrupt_handler,
				NFI_IRQ_NAME, info);
	if (ret) {
		pr_err("nfi irq register failed!\n");
		goto mem_free;
	}
	ret = nand_irq_register(res->ecc_irq, ecc_interrupt_handler,
				ECC_IRQ_NAME, info);
	if (ret) {
		pr_err("ecc irq register failed!\n");
		goto mem_free;
	}
	info->nfi_done = nand_event_create();
	info->ecc_done = nand_event_create();

	/* initialize nfc hw */
	nfc_hw_init(info);

	return &info->handler;

mem_free:
	mem_free(info);
	return NULL;
}

void nfc_release(struct nfc_handler *handler)
{
	struct nfc_info *info = handler_to_info(handler);

	nand_event_destroy(info->nfi_done);
	nand_event_destroy(info->ecc_done);
	mem_free(info->buf);
	mem_free(info);
}

int nfc_suspend(struct nfc_handler *handler)
{
	struct nfc_info *info = handler_to_info(handler);
	struct nfc_saved *saved;

	saved = &info->saved;

	saved->nfi_pagefmt = nfi_readl(info, NFI_PAGEFMT);
	saved->nfi_seccus_size = nfi_readl(info, NFI_SECCUS_SIZE);

	return 0;
}

int nfc_resume(struct nfc_handler *handler)
{
	struct nfc_info *info = handler_to_info(handler);
	struct nfc_saved *saved;

	saved = &info->saved;

	nfc_hw_init(info);

	nfi_writel(info, saved->nfi_pagefmt, NFI_PAGEFMT);
	nfi_writel(info, saved->nfi_seccus_size, NFI_SECCUS_SIZE);

	return 0;
}
