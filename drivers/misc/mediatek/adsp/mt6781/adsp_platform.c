/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <mt-plat/sync_write.h>
#include "adsp_platform.h"
#include "adsp_reg.h"
#include "adsp_reserved_mem.h"
#include "adsp_semaphore.h"
#include "adsp_platform_driver.h"

#ifdef ADSP_BASE
#undef ADSP_BASE
#endif
#define ADSP_BASE                  mt_base
#define INFRACFG_AO_BASE           infracfg_ao
#define PERICFG_BASE               pericfg

#define SET_BITS(addr, mask) writel(readl(addr) | (mask), addr)
#define CLR_BITS(addr, mask) writel(readl(addr) & ~(mask), addr)

#ifdef CONFIG_ARM64
#define IOMEM(a)                     ((void __force __iomem *)((a)))
#endif
#define adsp_reg_read(addr)             __raw_readl(IOMEM(addr))
#define adsp_reg_sync_write(addr, val)  mt_reg_sync_writel(val, addr)

#define MAGIC_PATTERN                   (0xfafafafa)

static void __iomem *mt_base;

static void __iomem *infracfg_ao;
static void __iomem *pericfg;


void adsp_mt_sw_reset(u32 cid)
{
	unsigned long flags;

	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;

	write_lock_irqsave(&access_rwlock, flags);
	SET_BITS(ADSP_CFGREG_SW_RSTN, ADSP_A_SW_RSTN);
	udelay(1);
	CLR_BITS(ADSP_CFGREG_SW_RSTN, ADSP_A_SW_RSTN);
	write_unlock_irqrestore(&access_rwlock, flags);
}

void adsp_mt_run(u32 cid)
{
	int timeout = 1000;

	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;

	/* request infra/26M/apsrc/v18/ ddr resource */
	SET_BITS(ADSP_SPM_REQ, ADSP_SPM_SRC_BITS);
	SET_BITS(ADSP_DDREN_REQ, ADSP_DDR_ENABLE);

	/* make sure SPM return ack */
	while (timeout) {
		if (readl(ADSP_SPM_ACK) == ((ADSP_DDR_ENABLE << 4) | ADSP_SPM_SRC_BITS))
			break;
		udelay(10);
		if (--timeout == 0) {
			pr_err("[ADSP] timeout: cannot get SPM ack\n");
			break;
		}
	}

	CLR_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_A_RUNSTALL);
}

void adsp_mt_stop(u32 cid)
{
	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;
	SET_BITS(ADSP_HIFI3_IO_CONFIG, ADSP_A_RUNSTALL);
}

void adsp_mt_clear(void)
{
	writel(0x0, ADSP_CFGREG_SW_RSTN);
	writel(0xC0001002, ADSP_HIFI3_IO_CONFIG);
	writel(0, ADSP_CREG_BOOTUP_MARK);
	writel(0xdf, ADSP_CLK_CTRL_BASE);
	writel(0x0, ADSP_IRQ_EN);
	writel(0x0, ADSP_A_WDT_REG);
}

void adsp_mt_clr_spm(u32 cid)
{
	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;

	CLR_BITS(ADSP_A_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
}

void adsp_mt_clr_sysirq(u32 cid)
{
	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;

	writel(ADSP_A_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
}

void adsp_mt_disable_wdt(u32 cid)
{
	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;

	CLR_BITS(ADSP_A_WDT_REG, WDT_EN_BIT);
}

bool check_hifi_status(u32 mask)
{
	return (readl(ADSP_SLEEP_STATUS_REG) & mask);
}

bool is_adsp_axibus_idle(void)
{
	/* no pending counter found when ap read this reg in mt6785 */
	return (readl(ADSP_DBG_PEND_CNT) == 0x000000);
}

void adsp_mt_set_bootup_mark(u32 cid)
{
	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;

	writel(MAGIC_PATTERN, ADSP_CREG_BOOTUP_MARK);
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
	CLR_BITS(ADSP_CFGREG_SW_RSTN, ADSP_A_SW_RSTN);
}

void adsp_mt_set_sw_int(u32 cid)
{
	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return;

	adsp_reg_sync_write(ADSP_SW_INT_SET, (1 << cid));
}

u32 adsp_mt_check_sw_int(u32 cid)
{
	if (unlikely(cid >= ADSP_CORE_TOTAL))
		return 0;

	return (adsp_reg_read(ADSP_SW_INT_SET) & (1 << cid));
}

void adsp_platform_init(void)
{
	if (unlikely(!adsp_cores[0]))
		return;
	mt_base = adsp_cores[0]->cfg;
	infracfg_ao = adsp_common.infracfg_ao;
	pericfg = adsp_common.pericfg;

	adsp_init_reserve_memory();
	adsp_sem_init(SEMA_WAY_BITS, SEMA_CTRL_BIT,
		SEMA_TIMEOUT, ADSP_SEMAPHORE);
}

/* mt6785 only */
static bool is_adsp_bus_protect_ready(void)
{
	return ((adsp_reg_read(INFRA_AXI_PROT_STA1) & ADSP_AXI_PROT_READY_MASK)
		== ADSP_AXI_PROT_READY_MASK);
}

void adsp_bus_sleep_protect(uint32_t enable)
{
	int timeout = 1000;

	if (enable) {
		/* enable adsp bus protect */
		adsp_reg_sync_write(INFRA_AXI_PROT_SET, ADSP_AXI_PROT_MASK);
		while (--timeout && !is_adsp_bus_protect_ready())
			udelay(1);
		if (!is_adsp_bus_protect_ready())
			pr_err("%s() ready timeout\n", __func__);
	} else {
		/* disable adsp bus protect */
		adsp_reg_sync_write(INFRA_AXI_PROT_CLR, ADSP_AXI_PROT_MASK);
	}
}

void adsp_way_en_ctrl(uint32_t enable)
{
	if (enable)
		adsp_reg_sync_write(ADSP_WAY_EN_CTRL,
			adsp_reg_read(ADSP_WAY_EN_CTRL) | ADSP_WAY_EN_MASK);
	else
		adsp_reg_sync_write(ADSP_WAY_EN_CTRL,
			adsp_reg_read(ADSP_WAY_EN_CTRL) & ~ADSP_WAY_EN_MASK);
}

