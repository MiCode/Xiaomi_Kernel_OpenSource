// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "gsi_emulation.h"

/*
 * *****************************************************************************
 * The following used to set up the EMULATION interrupt controller...
 * *****************************************************************************
 */
int setup_emulator_cntrlr(
	void __iomem *intcntrlr_base,
	u32           intcntrlr_mem_size)
{
	uint32_t val, ver, intrCnt, rangeCnt, range;

	val = gsi_emu_readl(intcntrlr_base + GE_INT_CTL_VER_CNT);

	intrCnt  = val & 0xFFFF;
	ver      = (val >> 16) & 0xFFFF;
	rangeCnt = intrCnt / 32;

	GSIDBG(
	    "CTL_VER_CNT reg val(0x%x) intr cnt(%u) cntrlr ver(0x%x) rangeCnt(%u)\n",
	    val, intrCnt, ver, rangeCnt);

	/*
	 * Verify the interrupt controller version
	 */
	if (ver == 0 || ver == 0xFFFF || ver < DEO_IC_INT_CTL_VER_MIN) {
		GSIERR(
		  "Error: invalid interrupt controller version 0x%x\n",
		  ver);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	/*
	 * Verify the interrupt count
	 *
	 * NOTE: intrCnt must be at least one block and multiple of 32
	 */
	if ((intrCnt % 32) != 0) {
		GSIERR(
		  "Invalid interrupt count read from HW 0x%04x\n",
		  intrCnt);
		return -GSI_STATUS_ERROR;
	}

	/*
	 * Calculate number of ranges used, each range handles 32 int lines
	 */
	if (rangeCnt > DEO_IC_MAX_RANGE_CNT) {
		GSIERR(
		  "SW interrupt limit(%u) passed, increase DEO_IC_MAX_RANGE_CNT(%u)\n",
		  rangeCnt,
		  DEO_IC_MAX_RANGE_CNT);
		return -GSI_STATUS_ERROR;
	}

	/*
	 * Let's take the last register offset minus the first
	 * register offset (ie. range) and compare it to the interrupt
	 * controller's dtsi defined memory size.  The range better
	 * fit within the size.
	 */
	val = GE_SOFT_INT_n(rangeCnt-1) - GE_INT_CTL_VER_CNT;
	if (val > intcntrlr_mem_size) {
		GSIERR(
		    "Interrupt controller register range (%u) exceeds dtsi provisioned size (%u)\n",
		    val, intcntrlr_mem_size);
		return -GSI_STATUS_ERROR;
	}

	/*
	 * The following will disable the emulators interrupt controller,
	 * so that we can config it...
	 */
	GSIDBG("Writing GE_INT_MASTER_ENABLE\n");
	gsi_emu_writel(
		0x0,
		intcntrlr_base + GE_INT_MASTER_ENABLE);

	/*
	 * Init register maps of all ranges
	 */
	for (range = 0; range < rangeCnt; range++) {
		/*
		 * Disable all int sources by setting all enable clear bits
		 */
		GSIDBG("Writing GE_INT_ENABLE_CLEAR_n(%u)\n", range);
		gsi_emu_writel(
		    0xFFFFFFFF,
		    intcntrlr_base + GE_INT_ENABLE_CLEAR_n(range));

		/*
		 * Clear all raw statuses
		 */
		GSIDBG("Writing GE_INT_CLEAR_n(%u)\n", range);
		gsi_emu_writel(
		    0xFFFFFFFF,
		    intcntrlr_base + GE_INT_CLEAR_n(range));

		/*
		 * Init all int types
		 */
		GSIDBG("Writing GE_INT_TYPE_n(%u)\n", range);
		gsi_emu_writel(
		    0x0,
		    intcntrlr_base + GE_INT_TYPE_n(range));
	}

	/*
	 * The following tells the interrupt controller to interrupt us
	 * when it sees interrupts from ipa and/or gsi.
	 *
	 * Interrupts:
	 * ===================================================================
	 * DUT0                       [  63 :   16 ]
	 * ipa_irq                                        [ 3 : 0 ] <---HERE
	 * ipa_gsi_bam_irq                                [ 7 : 4 ] <---HERE
	 * ipa_bam_apu_sec_error_irq                      [ 8 ]
	 * ipa_bam_apu_non_sec_error_irq                  [ 9 ]
	 * ipa_bam_xpu2_msa_intr                          [ 10 ]
	 * ipa_vmidmt_nsgcfgirpt                          [ 11 ]
	 * ipa_vmidmt_nsgirpt                             [ 12 ]
	 * ipa_vmidmt_gcfgirpt                            [ 13 ]
	 * ipa_vmidmt_girpt                               [ 14 ]
	 * bam_xpu3_qad_non_secure_intr_sp                [ 15 ]
	 */
	GSIDBG("Writing GE_INT_ENABLE_n(0)\n");
	gsi_emu_writel(
	    0x00FF, /* See <---HERE above */
	    intcntrlr_base + GE_INT_ENABLE_n(0));

	/*
	 * The following will enable the IC post config...
	 */
	GSIDBG("Writing GE_INT_MASTER_ENABLE\n");
	gsi_emu_writel(
	    0x1,
	    intcntrlr_base + GE_INT_MASTER_ENABLE);

	return 0;
}

/*
 * *****************************************************************************
 * The following for EMULATION hard irq...
 * *****************************************************************************
 */
irqreturn_t emulator_hard_irq_isr(
	int   irq,
	void *ctxt)
{
	struct gsi_ctx *gsi_ctx_ptr = (struct gsi_ctx *) ctxt;

	uint32_t val;

	val = gsi_emu_readl(gsi_ctx_ptr->intcntrlr_base + GE_INT_MASTER_STATUS);

	/*
	 * If bit zero is set, interrupt is for us, hence return IRQ_NONE
	 * when it's not set...
	 */
	if (!(val & 0x00000001))
		return IRQ_NONE;

	/*
	 * The following will mask (ie. turn off) future interrupts from
	 * the emulator's interrupt controller. It wil stay this way until
	 * we turn back on...which will be done in the bottom half
	 * (ie. emulator_soft_irq_isr)...
	 */
	gsi_emu_writel(
		0x0,
		gsi_ctx_ptr->intcntrlr_base + GE_INT_OUT_ENABLE);

	return IRQ_WAKE_THREAD;
}

/*
 * *****************************************************************************
 * The following for EMULATION soft irq...
 * *****************************************************************************
 */
irqreturn_t emulator_soft_irq_isr(
	int   irq,
	void *ctxt)
{
	struct gsi_ctx *gsi_ctx_ptr = (struct gsi_ctx *) ctxt;

	irqreturn_t retVal = IRQ_HANDLED;
	uint32_t	val;

	val = gsi_emu_readl(gsi_ctx_ptr->intcntrlr_base + GE_IRQ_STATUS_n(0));

	GSIDBG("Got irq(%d) with status(0x%08X)\n", irq, val);

	if (val & 0xF0 && gsi_ctx_ptr->intcntrlr_gsi_isr) {
		GSIDBG("Got gsi interrupt\n");
		retVal = gsi_ctx_ptr->intcntrlr_gsi_isr(irq, ctxt);
	}

	if (val & 0x0F && gsi_ctx_ptr->intcntrlr_client_isr) {
		GSIDBG("Got ipa interrupt\n");
		retVal = gsi_ctx_ptr->intcntrlr_client_isr(irq, 0);
	}

	/*
	 * The following will clear the interrupts...
	 */
	gsi_emu_writel(
		0xFFFFFFFF,
		gsi_ctx_ptr->intcntrlr_base + GE_INT_CLEAR_n(0));

	/*
	 * The following will unmask (ie. turn on) future interrupts from
	 * the emulator's interrupt controller...
	 */
	gsi_emu_writel(
		0x1,
		gsi_ctx_ptr->intcntrlr_base + GE_INT_OUT_ENABLE);

	return retVal;
}
