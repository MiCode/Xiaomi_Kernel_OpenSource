/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include "adsp_platform.h"
#include "adsp_reg.h"
#include "adsp_reserved_mem.h"
#include "adsp_semaphore.h"

#ifdef ADSP_BASE
#undef ADSP_BASE
#endif
#define ADSP_BASE                  mt_base

#define SET_BITS(addr, mask) writel(readl(addr) | (mask), addr)
#define CLR_BITS(addr, mask) writel(readl(addr) & ~(mask), addr)

static void __iomem *mt_base;

void adsp_mt_set_base(void *base)
{
	if (base)
		mt_base = base;
}

void adsp_mt_sw_reset(void)
{
	writel(ADSP_SW_RSTN, ADSP_CFGREG_SW_RSTN);
	udelay(1);
	writel(0, ADSP_CFGREG_SW_RSTN);
}

void adsp_mt_run(int core_id)
{
	//int timeout = 1000;
	if (core_id >= ADSP_CORE_TOTAL)
		return;

	/* request infra/26M/apsrc/v18/ ddr resource */
	if (core_id == ADSP_A_ID)
		SET_BITS(ADSP_A_DDREN_REQ, ADSP_SPM_SRC_BITS);
	else
		SET_BITS(ADSP_B_DDREN_REQ, ADSP_SPM_SRC_BITS);

	/* make sure SPM return ack */
#if 0
	while (readl(ADSP_SPM_ACK) != ADSP_SPM_SRC_BITS) {
		udelay(10);
		if (--timeout == 0) {
			pr_err("[ADSP] timeout: cannot get SPM ack\n");
			break;
		}
	}
#endif
	if (core_id == ADSP_A_ID)
		CLR_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_A_RUNSTALL);
	else
		CLR_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_B_RUNSTALL);
}

void adsp_mt_stop(int core_id)
{
	if (core_id >= ADSP_CORE_TOTAL)
		return;

	if (core_id == ADSP_A_ID)
		SET_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_A_RUNSTALL);
	else
		SET_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_B_RUNSTALL);
}

void adsp_mt_clear(void)
{
#if 0
	writel(0, CREG_BOOTUP_MARK);
	writel(0x0, ADSP_A_IRQ_EN);
	DRV_ClrReg32(ADSP_A_WDT_REG, WDT_EN_BIT);

	/** TCM back to initial state **/
	adsp_sram_reset_init();

	dsb(SY);
#endif
}

void adsp_mt_clr_spm(void)
{
	//CLR_BITS(ADSP_A_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
}

void adsp_platform_init(void *base)
{
	adsp_mt_set_base(base);
	adsp_init_reserve_memory();

	adsp_sem_init(SEMA_WAY_BITS, SEMA_CTRL_BIT,
		SEMA_TIMEOUT, ADSP_SEMAPHORE);
}

