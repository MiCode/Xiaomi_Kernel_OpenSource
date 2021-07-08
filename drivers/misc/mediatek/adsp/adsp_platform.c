// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include "adsp_reg.h"
#include "adsp_core.h"
#include "adsp_platform_driver.h"
#include "adsp_platform.h"

#ifdef ADSP_BASE
#undef ADSP_BASE
#endif
#ifdef ADSP_SECURE_BASE
#undef ADSP_SECURE_BASE
#endif
#define ADSP_BASE                  mt_base
#define ADSP_SECURE_BASE           mt_secure

#define SET_BITS(addr, mask) writel(readl(addr) | (mask), addr)
#define CLR_BITS(addr, mask) writel(readl(addr) & ~(mask), addr)

static void __iomem *mt_base;
static void __iomem *mt_secure;

static void adsp_mt_clr_dma(void)
{
	u32 ch = 0;
	void __iomem *dma_base;

	for (ch = 0; ch < ADSP_DMA_CHANNEL; ch++) {
		dma_base = ADSP_DMA_BASE_CH(ch);
		CLR_BITS(ADSP_DMA_START(dma_base), ADSP_DMA_START_CLR_BIT);
		SET_BITS(ADSP_DMA_ACKINT(dma_base), ADSP_DMA_ACK_BIT);
	}
}

void adsp_mt_sw_reset(u32 cid)
{
	unsigned long flags;

	if (unlikely(cid >= get_adsp_core_total()))
		return;

	write_lock_irqsave(&access_rwlock, flags);
	if (cid == ADSP_A_ID) {
		SET_BITS(ADSP_CFGREG_SW_RSTN, ADSP_A_SW_RSTN);
		udelay(1);
		CLR_BITS(ADSP_CFGREG_SW_RSTN, ADSP_A_SW_RSTN);
	} else {
		SET_BITS(ADSP_CFGREG_SW_RSTN, ADSP_B_SW_RSTN);
		udelay(1);
		CLR_BITS(ADSP_CFGREG_SW_RSTN, ADSP_B_SW_RSTN);
	}
	write_unlock_irqrestore(&access_rwlock, flags);
}

void adsp_mt_run(u32 cid)
{
	int timeout = 1000;

	if (unlikely(cid >= get_adsp_core_total()))
		return;

	/* request infra/26M/apsrc/v18/ ddr resource */
	if (cid == ADSP_A_ID)
		SET_BITS(ADSP_A_DDREN_REQ, ADSP_SPM_SRC_BITS);
	else
		SET_BITS(ADSP_B_DDREN_REQ, ADSP_SPM_SRC_BITS);

	/* make sure SPM return ack */
	while (readl(ADSP_SPM_ACK) != ADSP_SPM_SRC_BITS) {
		udelay(10);
		if (--timeout == 0) {
			pr_err("[ADSP] timeout: cannot get SPM ack\n");
			break;
		}
	}

	if (cid == ADSP_A_ID)
		CLR_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_A_RUNSTALL);
	else
		CLR_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_B_RUNSTALL);
}

void adsp_mt_stop(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (cid == ADSP_A_ID)
		SET_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_A_RUNSTALL);
	else
		SET_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_B_RUNSTALL);
}

void adsp_mt_clear(void)
{
	writel(0x0, ADSP_CFGREG_SW_RSTN);
	writel(0xC0001002, ADSP_HIFI3_IO_CONFIG);
	writel(0xdf, ADSP_CLK_CTRL_BASE);
	writel(0x0, ADSP_A_IRQ_EN);
	writel(0x0, ADSP_B_IRQ_EN);
	writel(0x0, ADSP_A_WDT_REG);
	writel(0x0, ADSP_B_WDT_REG);
	adsp_mt_clr_dma();
}

void adsp_mt_clr_spm(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (cid == ADSP_A_ID)
		CLR_BITS(ADSP_A_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
	else
		CLR_BITS(ADSP_B_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
}

void adsp_mt_clr_sysirq(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (cid == ADSP_A_ID)
		writel(ADSP_A_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
	else
		writel(ADSP_B_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
}
EXPORT_SYMBOL(adsp_mt_clr_sysirq);

void adsp_mt_clr_auidoirq(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;
	/* just clear correct bits*/
	if (cid == ADSP_A_ID)
		writel(ADSP_A_AFE2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
	else
		writel(ADSP_B_AFE2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
}
EXPORT_SYMBOL(adsp_mt_clr_auidoirq);

void adsp_mt_disable_wdt(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (cid == ADSP_A_ID)
		CLR_BITS(ADSP_A_WDT_REG, WDT_EN_BIT);
	else
		CLR_BITS(ADSP_B_WDT_REG, WDT_EN_BIT);
}
EXPORT_SYMBOL(adsp_mt_disable_wdt);

void adsp_mt_set_swirq(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (cid == ADSP_A_ID)
		writel(ADSP_A_SW_INT, ADSP_SW_INT_SET);
	else
		writel(ADSP_B_SW_INT, ADSP_SW_INT_SET);
}

u32 adsp_mt_check_swirq(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return 0;

	if (cid == ADSP_A_ID)
		return readl(ADSP_SW_INT_SET) & ADSP_A_SW_INT;
	else
		return readl(ADSP_SW_INT_SET) & ADSP_B_SW_INT;
}

bool check_hifi_status(u32 mask)
{
	return !!(readl(ADSP_SLEEP_STATUS_REG) & mask);
}

bool is_adsp_axibus_idle(void)
{
	/* only one transation currently: AP read pending counter */
	return (readl(ADSP_DBG_PEND_CNT) == 0x000100);
}

u32 switch_adsp_clk_ctrl_cg(bool en, u32 mask)
{
	u32 retval = readl(ADSP_CLK_CTRL_BASE);

	if (en)
		SET_BITS(ADSP_CLK_CTRL_BASE, mask);
	else
		CLR_BITS(ADSP_CLK_CTRL_BASE, mask);

	return retval;
}

u32 switch_adsp_uart_ctrl_cg(bool en, u32 mask)
{
	u32 retval = readl(ADSP_UART_CTRL);

	if (en)
		SET_BITS(ADSP_UART_CTRL, mask);
	else
		CLR_BITS(ADSP_UART_CTRL, mask);

	return retval;
}

void adsp_mt_clr_sw_reset(void)
{
	CLR_BITS(ADSP_CFGREG_SW_RSTN, ADSP_A_SW_RSTN | ADSP_B_SW_RSTN);
}

void adsp_mt_set_dram_remap(u32 addr, u32 size)
{
	writel(0xF, R_SYS_REMAP_ENABLE);
	writel(((ADSP_SYSRAM_DSP_VIEW + size) & 0xFFFF0000)
		| (ADSP_SYSRAM_DSP_VIEW >> 16), R_SYS_REMAP0);
	writel(addr >> 16, R_SYS_REMAP0_ADDR);
}

void adsp_mt_toggle_semaphore(u32 bit)
{
	writel((1 << bit), ADSP_SEMAPHORE);
}

u32 adsp_mt_get_semaphore(u32 bit)
{
	return (readl(ADSP_SEMAPHORE) >> bit) & 0x1;
}

void adsp_hardware_init(struct adspsys_priv *adspsys)
{
	if (unlikely(!adspsys))
		return;

	mt_base = adspsys->cfg;
	mt_secure = adspsys->cfg_secure;
}

