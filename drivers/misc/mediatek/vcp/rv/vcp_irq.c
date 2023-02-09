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
#include <soc/mediatek/smi.h>
//#include <mt-plat/sync_write.h>
#include "vcp_ipi_pin.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp_status.h"

static inline void vcp_wdt_clear(uint32_t coreid)
{
	coreid == 0 ? writel(B_WDT_IRQ, R_CORE0_WDT_IRQ) :
	writel(B_WDT_IRQ, R_CORE1_WDT_IRQ);
}

void wait_vcp_ready_to_reboot(void)
{
	int retry = 0;
	unsigned long C0_H0 = CORE_RDY_TO_REBOOT;
	unsigned long C0_H1 = CORE_RDY_TO_REBOOT;
	unsigned long C1_H0 = CORE_RDY_TO_REBOOT;
	unsigned long C1_H1 = CORE_RDY_TO_REBOOT;

	/* clr after VCP side INT trigger,
	 * or VCP may lost INT max wait = 200ms
	 */
	for (retry = VCP_AWAKE_TIMEOUT; retry > 0; retry--) {
		C0_H0 = readl(VCP_GPR_C0_H0_REBOOT);
		if (vcpreg.twohart)
			C0_H1 = readl(VCP_GPR_C0_H1_REBOOT);

		if (vcpreg.core_nums == 2) {
			C1_H0 = readl(VCP_GPR_C1_H0_REBOOT);
			if (vcpreg.twohart)
				C1_H1 = readl(VCP_GPR_C1_H1_REBOOT);
		}

		if ((C0_H0 == CORE_RDY_TO_REBOOT) && (C0_H1 == CORE_RDY_TO_REBOOT)
			&& (C1_H0 == CORE_RDY_TO_REBOOT) && (C1_H1 == CORE_RDY_TO_REBOOT))
			break;
		udelay(1);
	}

	if (retry == 0)
		pr_notice("[VCP] wakeup timeout C0 H0:0x%x H1:0x%x C1 H0:0x%x H1:0x%x Status: 0x%x\n",
			C0_H0, C0_H1, C1_H0, C1_H1, readl(R_CORE0_STATUS));

	udelay(10);
}
EXPORT_SYMBOL_GPL(wait_vcp_ready_to_reboot);

/*
 * handler for wdt irq for vcp
 * dump vcp register
 */
static void vcp_A_wdt_handler(struct tasklet_struct *t)
{
	unsigned int reg0 = readl(R_CORE0_WDT_IRQ);
	unsigned int reg1 = vcpreg.core_nums == 2 ? readl(R_CORE1_WDT_IRQ) : 0;

	pr_notice("[VCP] %s\n", __func__);

	/*trigger smi dump to get more info.*/
	mtk_smi_dbg_hang_detect("VCP WDT");

	wait_vcp_ready_to_reboot();
	/* Wakeup mobile_log_d after vcp flush the log */
	vcp_logger_wakeup_handler(0, NULL, NULL, 0);
	vcp_dump_last_regs(mmup_enable_count());
#if VCP_RECOVERY_SUPPORT
	if (vcp_set_reset_status() == RESET_STATUS_STOP) {
		pr_debug("[VCP] start to reset vcp...\n");
		vcp_send_reset_wq(RESET_TYPE_WDT);
	} else
		pr_notice("%s: vcp resetting\n", __func__);
#endif
	if (reg0)
		vcp_wdt_clear(0);
	if (reg1)
		vcp_wdt_clear(1);

	enable_irq(t->data);
}

DECLARE_TASKLET(vcp_A_irq0_tasklet, vcp_A_wdt_handler);
DECLARE_TASKLET(vcp_A_irq1_tasklet, vcp_A_wdt_handler);

/*
 * dispatch vcp irq
 * reset vcp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t vcp_A_irq_handler(int irq, void *dev_id)
{
	disable_irq_nosync(irq);
	if (likely(irq == vcpreg.irq0))
		tasklet_schedule(&vcp_A_irq0_tasklet);
	else
		tasklet_schedule(&vcp_A_irq1_tasklet);

	return IRQ_HANDLED;
}

