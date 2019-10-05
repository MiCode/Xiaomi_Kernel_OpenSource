/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

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

irqreturn_t adsp_wdt_dispatch(int irq, void *dev_id)
{
	if (irq == adsp_core[ADSP_A_ID].wdt_irq) {
		adsp_A_wdt_handler(irq, dev_id);
		writel(readl(ADSP_A_SPM_WAKEUPSRC) & ~ADSP_WAKEUP_SPM,
			ADSP_A_SPM_WAKEUPSRC);
	} else {
//		adsp_A_wdt_handler(irq, dev_id);
		writel(readl(ADSP_B_SPM_WAKEUPSRC) & ~ADSP_WAKEUP_SPM,
			ADSP_B_SPM_WAKEUPSRC);
	}
	return IRQ_HANDLED;
}

irqreturn_t  adsp_A_wdt_handler(int irq, void *dev_id)
{
	/*  disable wdt  */
	DRV_ClrReg32(ADSP_A_WDT_REG, WDT_EN_BIT);

#ifdef CFG_RECOVERY_SUPPORT
	if (!wdt_counter && !timer_pending(&adsp_wdt_timer))
		mod_timer(&adsp_wdt_timer, jiffies + ADSP_WDT_TIMEOUT);

	/* recovery failed reboot*/
	if (wdt_counter > WDT_LAST_WAIT_COUNT)
		BUG_ON(1);

	if (adsp_set_reset_status() == ADSP_RESET_STATUS_STOP) {
		wdt_counter++;
		pr_info("[ADSP] WDT exception (%u)\n", wdt_counter);
		adsp_send_reset_wq(ADSP_RESET_TYPE_WDT, ADSP_A_ID);
	} else
		pr_notice("[ADSP] resetting (%u)\n", wdt_counter);

#else
	adsp_aed_reset(EXCEP_RUNTIME, ADSP_A_ID);
#endif

	return IRQ_HANDLED;
}

#ifdef CFG_RECOVERY_SUPPORT
static void adsp_wdt_counter_reset(unsigned long data)
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
irqreturn_t adsp_ipc_dispatch(int irq, void *dev_id)
{
	if (irq == adsp_core[ADSP_A_ID].ipc_irq) {
		/* write 1 clear */
		writel(ADSP_A_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
		adsp_A_ipi_handler();
		writel(readl(ADSP_A_SPM_WAKEUPSRC) & ~ADSP_WAKEUP_SPM,
			ADSP_A_SPM_WAKEUPSRC);
	} else if (irq == adsp_core[ADSP_B_ID].ipc_irq) {
		/* write 1 clear */
		writel(ADSP_B_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
		/*FIXME: specify handler for ADSP_B ? */
//		adsp_A_ipi_handler();
		writel(readl(ADSP_B_SPM_WAKEUPSRC) & ~ADSP_WAKEUP_SPM,
			ADSP_B_SPM_WAKEUPSRC);
	}

	return IRQ_HANDLED;
}

/*
 * adsp irq initialize
 */
void adsp_A_irq_init(void)
{
	writel(ADSP_GENERAL_IRQ_INUSED, ADSP_GENERAL_IRQ_CLR); // clear adsp irq
#ifdef CFG_RECOVERY_SUPPORT
	setup_timer(&adsp_wdt_timer, adsp_wdt_counter_reset, 0);
#endif
}

irqreturn_t adsp_audioipc_dispatch(int irq, void *dev_id)
{
	/* separate core0/1 from IRQ ID */
	if (irq == adsp_core[ADSP_A_ID].audioipc_irq) {
		writel(ADSP_A_AFE2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
		/* call adsp a handler */
		writel(readl(ADSP_A_SPM_WAKEUPSRC) & ~ADSP_WAKEUP_SPM,
			ADSP_A_SPM_WAKEUPSRC);
	} else if (irq == adsp_core[ADSP_B_ID].audioipc_irq) {
		writel(ADSP_B_AFE2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
		/* call adsp b handler */
		writel(readl(ADSP_B_SPM_WAKEUPSRC) & ~ADSP_WAKEUP_SPM,
			ADSP_B_SPM_WAKEUPSRC);
	}

	return IRQ_HANDLED;
}
