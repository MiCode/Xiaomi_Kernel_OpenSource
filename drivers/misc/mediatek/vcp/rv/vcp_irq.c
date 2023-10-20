// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>
#include <linux/interrupt.h>
#include <linux/io.h>
//#include <mt-plat/sync_write.h>
#include "vcp_ipi_pin.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp_status.h"

/*
 * handler for wdt irq for vcp
 * dump vcp register
 */
static void vcp_A_wdt_handler(void)
{
	pr_notice("[VCP] %s\n", __func__);
	vcp_dump_last_regs(mmup_enable_count());
#if VCP_RECOVERY_SUPPORT
	if (vcp_set_reset_status() == RESET_STATUS_STOP) {
		pr_debug("[VCP] start to reset vcp...\n");
		vcp_send_reset_wq(RESET_TYPE_WDT);
	} else
		pr_notice("%s: vcp resetting\n", __func__);
#endif
}

void wait_vcp_wdt_irq_done(void)
{
	int retry = 0;
	unsigned long c0, c1;

	/* clr after VCP side INT trigger,
	 * or VCP may lost INT max wait = 200ms
	 */
	for (retry = VCP_AWAKE_TIMEOUT; retry > 0; retry--) {
		c0 = readl(VCP_GPR_CORE0_REBOOT);
		c1 = vcpreg.core_nums == 2? readl(VCP_GPR_CORE1_REBOOT):
			CORE_RDY_TO_REBOOT;

		if ((c0 == CORE_RDY_TO_REBOOT) && (c1 == CORE_RDY_TO_REBOOT))
			break;
		udelay(1);
	}

	if (retry == 0)
		pr_notice("[VCP] VCP wakeup timeout c0:0x%x c1:0x%x, Status: 0x%x\n",
			c0, c1, readl(R_CORE0_STATUS));

	udelay(10);
}
EXPORT_SYMBOL_GPL(wait_vcp_wdt_irq_done);

/*
 * dispatch vcp irq
 * reset vcp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t vcp_A_irq_handler(int irq, void *dev_id)
{
	unsigned int reg0 = readl(R_CORE0_WDT_IRQ);
	unsigned int reg1 = vcpreg.core_nums == 2? readl(R_CORE1_WDT_IRQ): 0;

	if (reg0 | reg1) {
		vcp_A_wdt_handler();
		/* clear IRQ */
		wait_vcp_wdt_irq_done();
		if (reg0)
			writel(B_WDT_IRQ, R_CORE0_WDT_IRQ);
		if (reg1)
			writel(B_WDT_IRQ, R_CORE1_WDT_IRQ);
	}
	return IRQ_HANDLED;
}

