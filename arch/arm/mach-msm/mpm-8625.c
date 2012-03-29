/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/spinlock.h>

#include <asm/hardware/gic.h>
#include <mach/msm_smsm.h>

#define NUM_REGS_ENABLE		2
/* (NR_MSM_IRQS/32) 96 max irqs supported */
#define NUM_REGS_DISABLE	3
#define GIC_IRQ_MASK(irq)	BIT(irq % 32)
#define GIC_IRQ_INDEX(irq)	(irq / 32)

#ifdef CONFIG_PM
u32 saved_spi_enable[DIV_ROUND_UP(256, 32)];
u32 saved_spi_conf[DIV_ROUND_UP(256, 16)];
u32 saved_spi_target[DIV_ROUND_UP(256, 4)];
u32 __percpu *saved_ppi_enable;
u32 __percpu *saved_ppi_conf;
#endif

enum {
	IRQ_DEBUG_SLEEP_INT_TRIGGER	= BIT(0),
	IRQ_DEBUG_SLEEP_INT		= BIT(1),
	IRQ_DEBUG_SLEEP_ABORT		= BIT(2),
	IRQ_DEBUG_SLEEP			= BIT(3),
	IRQ_DEBUG_SLEEP_REQUEST		= BIT(4)
};

static int msm_gic_irq_debug_mask;
module_param_named(debug_mask, msm_gic_irq_debug_mask, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

static uint32_t msm_gic_irq_smsm_wake_enable[NUM_REGS_ENABLE];
static uint32_t msm_gic_irq_idle_disable[NUM_REGS_DISABLE];

static DEFINE_RAW_SPINLOCK(gic_controller_lock);
static void __iomem *dist_base, *cpu_base;
static unsigned int max_irqs;

 /*
  * Some of the interrupts which will not be considered as wake capable
  * should be marked as FAKE.
  * Interrupts: GPIO, Timers etc..
  */
#define SMSM_FAKE_IRQ	(0xff)

 /* msm_gic_irq_to_smsm:  IRQ's those will be monitored by Modem */
static uint8_t msm_gic_irq_to_smsm[NR_IRQS] = {
	[MSM8625_INT_USB_OTG]		= 4,
	[MSM8625_INT_PWB_I2C]		= 5,
	[MSM8625_INT_SDC1_0]		= 6,
	[MSM8625_INT_SDC1_1]		= 7,
	[MSM8625_INT_SDC2_0]		= 8,
	[MSM8625_INT_SDC2_1]		= 9,
	[MSM8625_INT_ADSP_A9_A11]	= 10,
	[MSM8625_INT_UART1]		= 11,
	[MSM8625_INT_UART2]		= 12,
	[MSM8625_INT_UART3]		= 13,
	[MSM8625_INT_UART1_RX]		= 14,
	[MSM8625_INT_UART2_RX]		= 15,
	[MSM8625_INT_UART3_RX]		= 16,
	[MSM8625_INT_UART1DM_IRQ]	= 17,
	[MSM8625_INT_UART1DM_RX]	= 18,
	[MSM8625_INT_KEYSENSE]		= 19,
	[MSM8625_INT_AD_HSSD]		= 20,
	[MSM8625_INT_NAND_WR_ER_DONE]	= 21,
	[MSM8625_INT_NAND_OP_DONE]	= 22,
	[MSM8625_INT_TCHSCRN1]		= 23,
	[MSM8625_INT_TCHSCRN2]		= 24,
	[MSM8625_INT_TCHSCRN_SSBI]	= 25,
	[MSM8625_INT_USB_HS]		= 26,
	[MSM8625_INT_UART2DM_RX]	= 27,
	[MSM8625_INT_UART2DM_IRQ]	= 28,
	[MSM8625_INT_SDC4_1]		= 29,
	[MSM8625_INT_SDC4_0]		= 30,
	[MSM8625_INT_SDC3_1]		= 31,
	[MSM8625_INT_SDC3_0]		= 32,

	/* fake wakeup interrupts */
	[MSM8625_INT_GPIO_GROUP1]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_GPIO_GROUP2]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_A9_M2A_0]		= SMSM_FAKE_IRQ,
	[MSM8625_INT_A9_M2A_1]		= SMSM_FAKE_IRQ,
	[MSM8625_INT_A9_M2A_5]		= SMSM_FAKE_IRQ,
	[MSM8625_INT_GP_TIMER_EXP]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_DEBUG_TIMER_EXP]	= SMSM_FAKE_IRQ,
	[MSM8625_INT_ADSP_A11]		= SMSM_FAKE_IRQ,
};

static void msm_gic_mask_irq(struct irq_data *d)
{
	unsigned int index = GIC_IRQ_INDEX(d->irq);
	uint32_t mask;
	int smsm_irq = msm_gic_irq_to_smsm[d->irq];

	mask = GIC_IRQ_MASK(d->irq);

	if (smsm_irq == 0) {
		msm_gic_irq_idle_disable[index] &= ~mask;
	} else {
		mask = GIC_IRQ_MASK(smsm_irq - 1);
		msm_gic_irq_smsm_wake_enable[0] &= ~mask;
	}
}

static void msm_gic_unmask_irq(struct irq_data *d)
{
	unsigned int index = GIC_IRQ_INDEX(d->irq);
	uint32_t mask;
	int smsm_irq = msm_gic_irq_to_smsm[d->irq];

	mask = GIC_IRQ_MASK(d->irq);

	if (smsm_irq == 0) {
		msm_gic_irq_idle_disable[index] |= mask;
	} else {
		mask = GIC_IRQ_MASK(smsm_irq - 1);
		msm_gic_irq_smsm_wake_enable[0] |= mask;
	}
}

static int msm_gic_set_irq_wake(struct irq_data *d, unsigned int on)
{
	uint32_t mask;
	int smsm_irq = msm_gic_irq_to_smsm[d->irq];

	if (smsm_irq == 0) {
		pr_err("bad wake up irq %d\n", d->irq);
		return  -EINVAL;
	}

	if (smsm_irq == SMSM_FAKE_IRQ)
		return 0;

	mask = GIC_IRQ_MASK(smsm_irq - 1);
	if (on)
		msm_gic_irq_smsm_wake_enable[1] |= mask;
	else
		msm_gic_irq_smsm_wake_enable[1] &= ~mask;

	return 0;
}

#ifdef CONFIG_PM
static void __init msm_gic_pm_init(void)
{
	saved_ppi_enable =  __alloc_percpu(DIV_ROUND_UP(32, 32) * 4,
						sizeof(u32));
	BUG_ON(!saved_ppi_enable);

	saved_ppi_conf = __alloc_percpu(DIV_ROUND_UP(32, 16) * 4,
						sizeof(u32));
	BUG_ON(!saved_ppi_conf);
}
#endif

void msm_gic_irq_extn_init(void __iomem *db, void __iomem *cb)
{
	dist_base = db;
	cpu_base = cb;
	max_irqs = readl_relaxed(dist_base + GIC_DIST_CTR) & 0x11f;
	max_irqs = (max_irqs + 1) * 32;

	gic_arch_extn.irq_mask	= msm_gic_mask_irq;
	gic_arch_extn.irq_unmask = msm_gic_unmask_irq;
	gic_arch_extn.irq_disable = msm_gic_mask_irq;
	gic_arch_extn.irq_set_wake = msm_gic_set_irq_wake;

#ifdef CONFIG_PM
	msm_gic_pm_init();
#endif
}

/* GIC APIs */

 /*
  * Save the GIC cpu and distributor context before PC and
  * restor it back after coming out of PC.
  */
static void msm_gic_save(void)
{
	int i;
	u32 *ptr;

	/* Save the Per CPU PPI's */
	ptr = __this_cpu_ptr(saved_ppi_enable);
	/* 0 - 31 the SGI and PPI */
	ptr[0] = readl_relaxed(dist_base +  GIC_DIST_ENABLE_SET);

	ptr = __this_cpu_ptr(saved_ppi_conf);
	for (i = 0; i < 2; i++)
		ptr[i] =  readl_relaxed(dist_base +  GIC_DIST_CONFIG + i * 4);

	/* Save the SPIs */
	for (i = 0; (i * 16) < max_irqs; i++)
		saved_spi_conf[i] =
			readl_relaxed(dist_base + GIC_DIST_CONFIG + i * 4);

	for (i = 0; (i * 4) < max_irqs; i++)
		saved_spi_target[i] =
			readl_relaxed(dist_base + GIC_DIST_TARGET + i * 4);

	for (i = 0; (i * 32) < max_irqs; i++)
		saved_spi_enable[i] =
		readl_relaxed(dist_base + GIC_DIST_ENABLE_SET + i * 4);
}

static void msm_gic_restore(void)
{
	int i;
	u32 *ptr;

	/* restore CPU Interface */
	/* Per CPU SGIs and PPIs */
	ptr = __this_cpu_ptr(saved_ppi_enable);
	writel_relaxed(ptr[0], dist_base + GIC_DIST_ENABLE_SET);

	ptr = __this_cpu_ptr(saved_ppi_conf);
	for (i = 0; i < 2; i++)
		writel_relaxed(ptr[i], dist_base +  GIC_DIST_CONFIG + i * 4);

	for (i = 0; (i * 4) < max_irqs; i++)
		writel_relaxed(0xa0a0a0a0, dist_base + GIC_DIST_PRI + i * 4);

	writel_relaxed(0xf0, cpu_base + GIC_CPU_PRIMASK);
	writel_relaxed(1, cpu_base + GIC_CPU_CTRL);

	/* restore Distributor */
	writel_relaxed(0, dist_base + GIC_DIST_CTRL);

	for (i = 0; (i * 16) < max_irqs; i++)
		writel_relaxed(saved_spi_conf[i],
				dist_base + GIC_DIST_CONFIG + i * 4);

	for (i = 0; (i * 4) < max_irqs; i++)
		writel_relaxed(0xa0a0a0a0,
				dist_base + GIC_DIST_PRI + i * 4);

	for (i = 0; (i * 4) < max_irqs; i++)
		writel_relaxed(saved_spi_target[i],
				dist_base + GIC_DIST_TARGET + i * 4);

	for (i = 0; (i * 32) < max_irqs; i++)
		writel_relaxed(saved_spi_enable[i],
				dist_base + GIC_DIST_ENABLE_SET + i * 4);

	writel_relaxed(1, dist_base + GIC_DIST_CTRL);

	mb();
}

/* Power APIs */

 /*
  *  Check for any interrupts which are enabled are pending
  *  in the pending set or not.
  *  Return :
  *       0 : No pending interrupts
  *       1 : Pending interrupts other than A9_M2A_5
  */
unsigned int msm_gic_spi_ppi_pending(void)
{
	unsigned int i, bit = 0;
	unsigned int pending_enb = 0, pending = 0;
	unsigned long value = 0;

	raw_spin_lock(&gic_controller_lock);
	/*
	 * PPI and SGI to be included.
	 * MSM8625_INT_A9_M2A_5 needs to be ignored, as A9_M2A_5
	 * requesting sleep triggers it
	 */
	for (i = 0; (i * 32) < max_irqs; i++) {
		pending = readl_relaxed(dist_base +
				GIC_DIST_PENDING_SET + i * 4);
		pending_enb = readl_relaxed(dist_base +
				GIC_DIST_ENABLE_SET + i * 4);
		value = pending & pending_enb;

		if (value) {
			for (bit = 0; bit < 32; bit++) {
				bit = find_next_bit(&value, 32, bit);
				if ((bit + 32 * i) != MSM8625_INT_A9_M2A_5) {
					raw_spin_unlock(&gic_controller_lock);
					return 1;
				}
			}
		}
	}
	raw_spin_unlock(&gic_controller_lock);

	return 0;
}

 /*
  * Iterate over the disable list
  */

int msm_gic_irq_idle_sleep_allowed(void)
{
	uint32_t i, disable = 0;

	for (i = 0; i < NUM_REGS_DISABLE; i++)
		disable |= msm_gic_irq_idle_disable[i];

	return !disable;
}

 /*
  * Prepare interrupt subsystem for entering sleep -- phase 1
  * If modem_wake is true, return currently enabled interrupt
  * mask in  *irq_mask
  */
void msm_gic_irq_enter_sleep1(bool modem_wake, int from_idle, uint32_t
		*irq_mask)
{
	if (modem_wake) {
		*irq_mask = msm_gic_irq_smsm_wake_enable[!from_idle];
		if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
			pr_info("%s irq_mask %x\n", __func__, *irq_mask);
	}
}

 /*
  * Prepare interrupt susbsytem for entering sleep -- phase 2
  * Detect any pending interrupts and configure interrupt hardware.
  * Return value:
  * -EAGAIN: there are pending interrupt(s); interrupt configuration is not
  *		changed
  *	  0: Success
  */
int msm_gic_irq_enter_sleep2(bool modem_wake, int from_idle)
{
	int i;

	if (from_idle && !modem_wake)
		return 0;

	/* edge triggered interrupt may get lost if this mode is used */
	WARN_ON_ONCE(!modem_wake && !from_idle);

	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s interrupts pending\n", __func__);

	/* check the pending interrupts */
	if (msm_gic_spi_ppi_pending()) {
		if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP_ABORT)
			pr_info("%s aborted....\n", __func__);
		return -EAGAIN;
	}

	if (modem_wake) {
		/* save the contents of GIC CPU interface and Distributor */
		msm_gic_save();
		/* Disable all the Interrupts, if we enter from idle pc */
		if (from_idle) {
			for (i = 0; (i * 32) < max_irqs; i++)
				writel_relaxed(0xffffffff, dist_base
					+ GIC_DIST_ENABLE_CLEAR + i * 4);
		}
		irq_set_irq_type(MSM8625_INT_A9_M2A_6, IRQF_TRIGGER_RISING);
		enable_irq(MSM8625_INT_A9_M2A_6);
		pr_debug("%s going for sleep now\n", __func__);
	}

	return 0;
}

 /*
  * Restore interrupt subsystem from sleep -- phase 1
  * Configure the interrupt hardware.
  */
void msm_gic_irq_exit_sleep1(uint32_t irq_mask, uint32_t wakeup_reason,
		uint32_t pending_irqs)
{
	/* Restore GIC contents, which were saved */
	msm_gic_restore();

	/* Disable A9_M2A_6 */
	disable_irq(MSM8625_INT_A9_M2A_6);

	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s %x %x %x now\n", __func__, irq_mask,
				pending_irqs, wakeup_reason);
}

 /*
  * Restore interrupt subsystem from sleep -- phase 2
  * Poke the specified pending interrupts into interrupt hardware.
  */
void msm_gic_irq_exit_sleep2(uint32_t irq_mask, uint32_t wakeup_reason,
			uint32_t pending)
{
	int i, smsm_irq, smsm_mask;
	struct irq_desc *desc;

	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s %x %x %x now\n", __func__, irq_mask,
				 pending, wakeup_reason);

	for (i = 0; pending && i < ARRAY_SIZE(msm_gic_irq_to_smsm); i++) {
		smsm_irq = msm_gic_irq_to_smsm[i];

		if (smsm_irq == 0)
			continue;

		smsm_mask = BIT(smsm_irq - 1);
		if (!(pending & smsm_mask))
			continue;

		pending &= ~smsm_mask;

		if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP_INT)
			pr_info("%s, irq %d, still pending %x now\n",
					__func__, i, pending);
		/* Peding IRQ */
		desc = i ? irq_to_desc(i) : NULL;

		/* Check if the pending */
		if (desc && !irqd_is_level_type(&desc->irq_data)) {
			/* Mark the IRQ as pending, if not Level */
			irq_set_pending(i);
			check_irq_resend(desc, i);
		}
	}
}

 /*
  * Restore interrupt subsystem from sleep -- phase 3
  * Print debug information
  */
void msm_gic_irq_exit_sleep3(uint32_t irq_mask, uint32_t wakeup_reason,
		uint32_t pending_irqs)
{
	if (msm_gic_irq_debug_mask & IRQ_DEBUG_SLEEP)
		pr_info("%s, irq_mask %x pending_irqs %x, wakeup_reason %x,"
				"state %x now\n", __func__, irq_mask,
				pending_irqs, wakeup_reason,
				smsm_get_state(SMSM_MODEM_STATE));
}
