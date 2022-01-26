/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>
#include <linux/interrupt.h>
#include <mt-plat/sync_write.h>
#include "scp_ipi_pin.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"


/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_A_wdt_handler(void)
{
	pr_notice("[SCP] %s\n", __func__);
	scp_dump_last_regs();
	if (scp_set_reset_status() == RESET_STATUS_STOP) {
		pr_debug("[SCP] start to reset scp...\n");
		scp_send_reset_wq(RESET_TYPE_WDT);
	} else
		pr_notice("scp_A_wdt_handler: scp resetting\n");
}

static void wait_scp_wdt_irq_done(void)
{
	int retry = 0;
	unsigned long c0, c1;

	/* clr after SCP side INT trigger,
	 * or SCP may lost INT max wait = 200ms
	 */
	for (retry = SCP_AWAKE_TIMEOUT; retry > 0; retry--) {
		c0 = readl(SCP_GPR_CORE0_REBOOT);
		c1 = readl(SCP_GPR_CORE1_REBOOT);
		if ((c0 == CORE_RDY_TO_REBOOT) && (c1 == CORE_RDY_TO_REBOOT))
			break;
		udelay(2);
	}

	if (retry == 0)
		pr_debug("[SCP] SCP wakeup timeout c0:%x c1:%x\n", c0, c1);

	udelay(10);


}
/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_A_irq_handler(int irq, void *dev_id)
{
	unsigned int reg0 = readl(R_CORE0_WDT_IRQ);
	unsigned int reg1 = readl(R_CORE1_WDT_IRQ);

	if (reg0 | reg1) {
		scp_A_wdt_handler();
		/* clear IRQ */
		wait_scp_wdt_irq_done();
		if (reg0)
			writel(B_WDT_IRQ, R_CORE0_WDT_IRQ);
		if (reg1)
			writel(B_WDT_IRQ, R_CORE1_WDT_IRQ);
	}
	return IRQ_HANDLED;
}

