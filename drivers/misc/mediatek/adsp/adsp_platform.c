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

#define ADSP_BASE                  mt_base

#define SET_BITS(addr, mask) writel(readl(addr) | (mask), addr)
#define CLR_BITS(addr, mask) writel(readl(addr) & ~(mask), addr)

static void __iomem *mt_base;
static u32 axibus_idle_val;

/* below access adsp register necessary */
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

void adsp_mt_clr_spm(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (cid == ADSP_A_ID)
		CLR_BITS(ADSP_A_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
	else
		CLR_BITS(ADSP_B_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
}

bool check_hifi_status(u32 mask)
{
	return !!(readl(ADSP_SLEEP_STATUS_REG) & mask);
}

bool is_adsp_axibus_idle(void)
{
	return (readl(ADSP_DBG_PEND_CNT) == axibus_idle_val);
}

bool is_infrabus_timeout(void)
{
	return readl(ADSP_A_INTR_STATUS) & INFRABUS_TIMEOUT_IRQ;
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
	axibus_idle_val = adspsys->desc->axibus_idle_val;
}

