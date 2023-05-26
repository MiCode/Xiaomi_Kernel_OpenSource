/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <mtk_spm.h>
#include <mtk_spm_irq.h>
#include <mtk_spm_internal.h>
//#include <mt-plat/mtk_cirq.h> //FIXME: CIRQ driver not ready

#if 0 //FIXME
#include <mtk_spm_vcore_dvfs.h>
#endif

#if defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mtk_cirq.h>
#endif /* CONFIG_MTK_SYS_CIRQ */
#if defined(CONFIG_MTK_GIC_V3_EXT)
#include <linux/irqchip/mtk-gic-extend.h>
#endif /* CONFIG_MTK_GIC_V3_EXT */

char __attribute__((weak)) *spm_vcorefs_dump_dvfs_regs(char *p)
{
	return NULL;
}

void __attribute__((weak)) mt_cirq_clone_gic(void)
{
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_cirq_enable(void)
{
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_cirq_flush(void)
{
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_cirq_disable(void)
{
	printk_deferred("[name:spm&][SPM] NO %s !!!\n", __func__);
}

void __attribute__((weak)) set_wakeup_sources(u32 *list, u32 num_events)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}

/***************************************************
 * spm edge trigger irq backup/restore
 ***************************************************/

/* edge_trigger_irq_list is defined in header file 'mtk_spm_irq_edge.h' */
#include <mtk_spm_irq_edge.h>

#define IRQ_NUMBER (sizeof(list)/sizeof(struct edge_trigger_irq_list))
static u32 edge_trig_irqs[IRQ_NUMBER];

static void mtk_spm_get_edge_trigger_irq(void)
{
	int i;
	struct device_node *node;
	unsigned int irq_type;

	printk_deferred("[name:spm&][SPM] edge trigger irqs:\n");
	for (i = 0; i < IRQ_NUMBER; i++) {
		node = of_find_compatible_node(NULL, NULL, list[i].name);
		if (!node) {
			printk_deferred("[name:spm&][SPM] find '%s' node failed\n",
				list[i].name);
			continue;
		}

		edge_trig_irqs[i] =
			irq_of_parse_and_map(node, list[i].order);

		if (!edge_trig_irqs[i]) {
			printk_deferred("[name:spm&][SPM] get '%s' failed\n",
				list[i].name);
			continue;
		}

		/* Note: Initialize irq type to avoid pending irqs */
		irq_type = irq_get_trigger_type(edge_trig_irqs[i]);
		irq_set_irq_type(edge_trig_irqs[i], irq_type);

		printk_deferred("[name:spm&][SPM] '%s', irq=%d, type=%d\n",
			 list[i].name,	edge_trig_irqs[i], irq_type);
	}
}

#if defined(CONFIG_MTK_GIC_V3_EXT)
static void mtk_spm_unmask_edge_trig_irqs_for_cirq(void)
{
	int i;

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (edge_trig_irqs[i]) {
			/* unmask edge trigger irqs */
			mt_irq_unmask_for_sleep_ex(edge_trig_irqs[i]);
		}
	}
}

static bool spm_in_idle;
static int cpu_pm_callback_wakeup_src_restore(
	struct notifier_block *self, unsigned long cmd, void *v)
{
	int i;

	/* Note: cmd will be CPU_PM_ENTER/CPU_PM_EXIT/CPU_PM_ENTER_FAILED.
	 * Set edge trigger interrupt pending only in case CPU_PM_EXIT
	 */
	if (cmd == CPU_PM_EXIT && spm_in_idle) {
		for (i = 0; i < IRQ_NUMBER; i++) {
			if (spm_read(SPM_SW_RSV_0) & list[i].wakesrc)
				mt_irq_set_pending(edge_trig_irqs[i]);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block mtk_spm_cpu_pm_notifier_block = {
	.notifier_call = cpu_pm_callback_wakeup_src_restore,
};

#endif

static unsigned int spm_irq_0;
#if defined(CONFIG_MTK_GIC_V3_EXT)
static struct mtk_irq_mask irq_mask;
#endif

void mtk_spm_irq_backup(void)
{
#if defined(CONFIG_MTK_GIC_V3_EXT)
	spm_in_idle = true;
	mt_irq_mask_all(&irq_mask);
	mt_irq_unmask_for_sleep_ex(spm_irq_0);
	mtk_spm_unmask_edge_trig_irqs_for_cirq();
#endif

#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_sw_reset();
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif
}

void mtk_spm_irq_restore(void)
{
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_restore(&irq_mask);
	spm_in_idle = false;
#endif
}

unsigned int mtk_spm_get_irq_0(void)
{
	return spm_irq_0;
}

/********************************************************************
 * spm irq handler and initialize function
 *******************************************************************/

static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr = 0;
	unsigned long flags;
	struct twam_cfg twamsig;
	u32 twam_idle_sel = 0;
	struct twam_select twam_sel;
	twam_handler_t twam_handler;

	spin_lock_irqsave(&__spm_lock, flags);
	/* get ISR status */
	isr = spm_read(SPM_IRQ_STA);
	if (isr & ISRS_TWAM) {
		twamsig.byte[0].id = spm_read(SPM_TWAM_LAST_STA0);
		twamsig.byte[1].id = spm_read(SPM_TWAM_LAST_STA1);
		twamsig.byte[2].id = spm_read(SPM_TWAM_LAST_STA2);
		twamsig.byte[3].id = spm_read(SPM_TWAM_LAST_STA3);
		twam_idle_sel = spm_read(SPM_TWAM_IDLE_SEL);
		twam_sel.signal[0] = ((twam_idle_sel & 0x00000060) >> 5);
		twam_sel.signal[1] = ((twam_idle_sel & 0x00006000) >> 13);
		twam_sel.signal[2] = ((twam_idle_sel & 0x00600000) >> 21);
		twam_sel.signal[3] = ((twam_idle_sel & 0x60000000) >> 29);
		twam_sel.id[0] = (twam_idle_sel & 0x0000001F);
		twam_sel.id[1] = ((twam_idle_sel & 0x00001F00) >> 8);
		twam_sel.id[2] = ((twam_idle_sel & 0x001F0000) >> 16);
		twam_sel.id[3] = ((twam_idle_sel & 0x1F000000) >> 24);
		udelay(40); /* delay 1T @ 32K */
	}
	/* clean ISR status */
	SMC_CALL(IRQ0_HANDLER, isr, 0, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
	if (isr & (ISRS_SW_INT1)) {
		printk_deferred("[name:spm&][SPM] IRQ0 (ISRS_SW_INT1) HANDLER SHOULD NOT BE EXECUTED (0x%x)\n",
			isr);
		#if !defined(CONFIG_FPGA_EARLY_PORTING)
		spm_vcorefs_dump_dvfs_regs(NULL);
		#endif
		return IRQ_HANDLED;
	}

	twam_handler = spm_twam_handler_get();
	if ((isr & ISRS_TWAM) && twam_handler)
		twam_handler(&twamsig, &twam_sel);

	if (isr & (ISRS_SW_INT0 | ISRS_PCM_RETURN))
		printk_deferred("[name:spm&][SPM] IRQ0 HANDLER SHOULD NOT BE EXECUTED (0x%x)\n",
			isr);

	return IRQ_HANDLED;
}

struct spm_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

int mtk_spm_irq_register(unsigned int spmirq0)
{
	int i, err, r = 0;
	struct spm_irq_desc irqdesc[] = {
		{.irq = 0, .handler = spm_irq0_handler,}
	};
	irqdesc[0].irq = spmirq0;
	for (i = 0; i < ARRAY_SIZE(irqdesc); i++) {
		if (cpu_present(i)) {
			err = request_irq(irqdesc[i].irq, irqdesc[i].handler,
				IRQF_TRIGGER_LOW |
				IRQF_NO_SUSPEND |
				IRQF_PERCPU,
				"SPM", NULL);
			if (err) {
				printk_deferred("[name:spm&][SPM] FAILED TO REQUEST IRQ%d (%d)\n",
					i, err);
				r = -EPERM;
			}
		}
	}

	/* Assing local spm_irq_0 */
	spm_irq_0 = spmirq0;

	mtk_spm_get_edge_trigger_irq();

#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
	set_wakeup_sources(edge_trig_irqs, IRQ_NUMBER);
#endif

	#if defined(CONFIG_MTK_GIC_V3_EXT)
	cpu_pm_register_notifier(&mtk_spm_cpu_pm_notifier_block);
	#endif

	return r;
}

