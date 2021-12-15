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
#include "scp_ipi_pin.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"

static inline void scp_wdt_clear(uint32_t coreid)
{
	coreid == 0 ? writel(B_WDT_IRQ, R_CORE0_WDT_IRQ) :
		writel(B_WDT_IRQ, R_CORE1_WDT_IRQ);
}

/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_A_wdt_handler(unsigned long data)
{
	unsigned int reg0 = readl(R_CORE0_WDT_IRQ);
	unsigned int reg1 = scpreg.core_nums == 2 ?
		readl(R_CORE1_WDT_IRQ) : 0;
#if SCP_RECOVERY_SUPPORT
	if (scp_set_reset_status() == RESET_STATUS_STOP) {
		pr_debug("[SCP] start to reset scp...\n");
		scp_dump_last_regs();
		scp_send_reset_wq(RESET_TYPE_WDT);
	} else
		pr_notice("%s: scp resetting\n", __func__);
#endif
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
		if (reg0)
			scp_do_wdt_clear(0);
		if (reg1)
			scp_do_wdt_clear(1);
	} else
#endif
	{
		if (reg0)
			scp_wdt_clear(0);
		if (reg1)
			scp_wdt_clear(1);
	}
	enable_irq(data);
}

DECLARE_TASKLET(scp_A_irq0_tasklet, scp_A_wdt_handler, 0);
DECLARE_TASKLET(scp_A_irq1_tasklet, scp_A_wdt_handler, 0);

/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_A_irq_handler(int irq, void *dev_id)
{
	disable_irq_nosync(irq);
	if (likely(irq == scpreg.irq0))
		tasklet_schedule(&scp_A_irq0_tasklet);
	else
		tasklet_schedule(&scp_A_irq1_tasklet);
	return IRQ_HANDLED;
}

