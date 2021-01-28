// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "scp_feature_define.h"
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_A_wdt_handler(void)
{
	pr_debug("[SCP] CM4 A WDT exception\n");
	scp_A_dump_regs();
}

/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_A_irq_handler(int irq, void *dev_id)
{
	unsigned int reg = readl(SCP_A_TO_HOST_REG);

#if SCP_RECOVERY_SUPPORT
	/* if WDT and IPI triggered on the same time, ignore the IPI */
	if (reg & SCP_IRQ_WDT) {
		int retry;
		unsigned long tmp;

		scp_A_wdt_handler();
		if (scp_set_reset_status() == RESET_STATUS_STOP) {
			pr_debug("[SCP] CM4 WDT handler start to reset scp...\n");
			scp_send_reset_wq(RESET_TYPE_WDT);
		} else
			pr_notice("scp_A_wdt_handler: scp resetting\n");

		/* clr after SCP side INT trigger,
		 * or SCP may lost INT max wait 5000*40u = 200ms
		 */
		for (retry = SCP_AWAKE_TIMEOUT; retry > 0; retry--) {
			tmp = readl(SCP_GPR_CM4_A_REBOOT);
			if (tmp == CM4_A_READY_TO_REBOOT)
				break;
			udelay(40);
		}
		if (retry == 0)
			pr_debug("[SCP] SCP_A wakeup timeout\n");
		udelay(10);
		writel(SCP_IRQ_WDT, SCP_A_TO_HOST_REG);
	} else if (reg & SCP_IRQ_SCP2HOST) {
		/* if WDT and IPI triggered on the same time, ignore the IPI */
		scp_A_ipi_handler();
		writel(SCP_IRQ_SCP2HOST, SCP_A_TO_HOST_REG);
	}
#else
	int reboot = 0;

	if (reg & SCP_IRQ_WDT) {
		scp_A_wdt_handler();
		reboot = 1;
		reg &= SCP_IRQ_WDT;
	}

	if (reg & SCP_IRQ_SCP2HOST) {
		/* if WDT and IPI triggered on the same time, ignore the IPI */
		if (!reboot)
			scp_A_ipi_handler();
		reg &= SCP_IRQ_SCP2HOST;
	}

	writel(reg, SCP_A_TO_HOST_REG);

	if (reboot)
		scp_aed_reset(EXCEP_RUNTIME, SCP_A_ID);
#endif  // SCP_RECOVERY_SUPPORT

	return IRQ_HANDLED;
}

/*
 * scp irq initialize
 */
void scp_A_irq_init(void)
{
	writel(SCP_IRQ_SCP2HOST, SCP_A_TO_HOST_REG); /* clear scp irq */
}
