// SPDX-License-Identifier: GPL-2.0
//
// adsp_irq.c--  Mediatek ADSP IRQ handling
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>
#include <linux/interrupt.h>
#include <mt-plat/sync_write.h>
#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_excep.h"
#include "adsp_dvfs.h"

/*
 * handler for wdt irq for adsp
 * dump adsp register
 */

#define DRV_Reg32(addr)           readl(addr)
#define DRV_WriteReg32(addr, val) writel(val, addr)
#define DRV_SetReg32(addr, val)   DRV_WriteReg32(addr, DRV_Reg32(addr) | (val))
#define DRV_ClrReg32(addr, val)   DRV_WriteReg32(addr, DRV_Reg32(addr) & ~(val))
#ifdef CFG_RECOVERY_SUPPORT
#define ADSP_WDT_TIMEOUT (60 * HZ) /* 60 seconds*/
static struct timer_list adsp_wdt_timer;
unsigned int wdt_counter;
#endif

irqreturn_t  adsp_A_wdt_handler(int irq, void *dev_id)
{
	/*  disable wdt  */
	DRV_ClrReg32(ADSP_A_WDT_REG, WDT_EN_BIT);

#ifdef CFG_RECOVERY_SUPPORT
	if (!wdt_counter && !timer_pending(&adsp_wdt_timer))
		mod_timer(&adsp_wdt_timer, jiffies + ADSP_WDT_TIMEOUT);

	/* recovery failed reboot*/
	if (wdt_counter > WDT_LAST_WAIT_COUNT)
		WARN_ON(1);

	if (adsp_set_reset_status() == ADSP_RESET_STATUS_STOP) {
		wdt_counter++;
		pr_info("[ADSP] WDT exception (%u)\n", wdt_counter);
		adsp_send_reset_wq(ADSP_RESET_TYPE_WDT, ADSP_A_ID);
	} else
		pr_notice("[ADSP] resetting (%u)\n", wdt_counter);

#else
	adsp_aed_reset(EXCEP_RUNTIME, ADSP_A_ID);
#endif

	/* clear spm wakeup src */
	writel(0x0, ADSP_TO_SPM_REG);

	return IRQ_HANDLED;
}

#ifdef CFG_RECOVERY_SUPPORT
static void adsp_wdt_counter_reset(struct timer_list *t)
{
	del_timer(&adsp_wdt_timer);
	wdt_counter = 0;
	pr_info("[ADSP] %s\n", __func__);
}
#endif

/*
 * dispatch adsp irq
 * reset adsp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t adsp_A_irq_handler(int irq, void *dev_id)
{
	adsp_A_ipi_handler();
	/* write 1 clear */
	writel(ADSP_IRQ_ADSP2HOST, ADSP_A_TO_HOST_REG);
	dsb(SY);

	return IRQ_HANDLED;
}

/*
 * adsp irq initialize
 */
void adsp_A_irq_init(void)
{
	writel(ADSP_IRQ_ADSP2HOST, ADSP_A_TO_HOST_REG); /* clear adsp irq */
#ifdef CFG_RECOVERY_SUPPORT
	timer_setup(&adsp_wdt_timer, adsp_wdt_counter_reset, 0);
#endif
}
