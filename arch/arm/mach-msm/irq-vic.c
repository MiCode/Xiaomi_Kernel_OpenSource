/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009, 2011 Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/exception.h>

#include <mach/hardware.h>

#include <mach/msm_iomap.h>
#include <mach/fiq.h>

#include "fiq.h"
#include "smd_private.h"

enum {
	IRQ_DEBUG_SLEEP_INT_TRIGGER = 1U << 0,
	IRQ_DEBUG_SLEEP_INT = 1U << 1,
	IRQ_DEBUG_SLEEP_ABORT = 1U << 2,
	IRQ_DEBUG_SLEEP = 1U << 3,
	IRQ_DEBUG_SLEEP_REQUEST = 1U << 4,
};
static int msm_irq_debug_mask;
module_param_named(debug_mask, msm_irq_debug_mask, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

#define VIC_REG(off) (MSM_VIC_BASE + (off))
#define VIC_INT_TO_REG_ADDR(base, irq) (base + (irq / 32) * 4)
#define VIC_INT_TO_REG_INDEX(irq) ((irq >> 5) & 3)

#define VIC_INT_SELECT0     VIC_REG(0x0000)  /* 1: FIQ, 0: IRQ */
#define VIC_INT_SELECT1     VIC_REG(0x0004)  /* 1: FIQ, 0: IRQ */
#define VIC_INT_SELECT2     VIC_REG(0x0008)  /* 1: FIQ, 0: IRQ */
#define VIC_INT_SELECT3     VIC_REG(0x000C)  /* 1: FIQ, 0: IRQ */
#define VIC_INT_EN0         VIC_REG(0x0010)
#define VIC_INT_EN1         VIC_REG(0x0014)
#define VIC_INT_EN2         VIC_REG(0x0018)
#define VIC_INT_EN3         VIC_REG(0x001C)
#define VIC_INT_ENCLEAR0    VIC_REG(0x0020)
#define VIC_INT_ENCLEAR1    VIC_REG(0x0024)
#define VIC_INT_ENCLEAR2    VIC_REG(0x0028)
#define VIC_INT_ENCLEAR3    VIC_REG(0x002C)
#define VIC_INT_ENSET0      VIC_REG(0x0030)
#define VIC_INT_ENSET1      VIC_REG(0x0034)
#define VIC_INT_ENSET2      VIC_REG(0x0038)
#define VIC_INT_ENSET3      VIC_REG(0x003C)
#define VIC_INT_TYPE0       VIC_REG(0x0040)  /* 1: EDGE, 0: LEVEL  */
#define VIC_INT_TYPE1       VIC_REG(0x0044)  /* 1: EDGE, 0: LEVEL  */
#define VIC_INT_TYPE2       VIC_REG(0x0048)  /* 1: EDGE, 0: LEVEL  */
#define VIC_INT_TYPE3       VIC_REG(0x004C)  /* 1: EDGE, 0: LEVEL  */
#define VIC_INT_POLARITY0   VIC_REG(0x0050)  /* 1: NEG, 0: POS */
#define VIC_INT_POLARITY1   VIC_REG(0x0054)  /* 1: NEG, 0: POS */
#define VIC_INT_POLARITY2   VIC_REG(0x0058)  /* 1: NEG, 0: POS */
#define VIC_INT_POLARITY3   VIC_REG(0x005C)  /* 1: NEG, 0: POS */
#define VIC_NO_PEND_VAL     VIC_REG(0x0060)

#if defined(CONFIG_ARCH_MSM_SCORPION) && !defined(CONFIG_MSM_SMP)
#define VIC_NO_PEND_VAL_FIQ VIC_REG(0x0064)
#define VIC_INT_MASTEREN    VIC_REG(0x0068)  /* 1: IRQ, 2: FIQ     */
#define VIC_CONFIG          VIC_REG(0x006C)  /* 1: USE SC VIC */
#else
#define VIC_INT_MASTEREN    VIC_REG(0x0064)  /* 1: IRQ, 2: FIQ     */
#define VIC_PROTECTION      VIC_REG(0x006C)  /* 1: ENABLE          */
#define VIC_CONFIG          VIC_REG(0x0068)  /* 1: USE ARM1136 VIC */
#endif

#define VIC_IRQ_STATUS0     VIC_REG(0x0080)
#define VIC_IRQ_STATUS1     VIC_REG(0x0084)
#define VIC_IRQ_STATUS2     VIC_REG(0x0088)
#define VIC_IRQ_STATUS3     VIC_REG(0x008C)
#define VIC_FIQ_STATUS0     VIC_REG(0x0090)
#define VIC_FIQ_STATUS1     VIC_REG(0x0094)
#define VIC_FIQ_STATUS2     VIC_REG(0x0098)
#define VIC_FIQ_STATUS3     VIC_REG(0x009C)
#define VIC_RAW_STATUS0     VIC_REG(0x00A0)
#define VIC_RAW_STATUS1     VIC_REG(0x00A4)
#define VIC_RAW_STATUS2     VIC_REG(0x00A8)
#define VIC_RAW_STATUS3     VIC_REG(0x00AC)
#define VIC_INT_CLEAR0      VIC_REG(0x00B0)
#define VIC_INT_CLEAR1      VIC_REG(0x00B4)
#define VIC_INT_CLEAR2      VIC_REG(0x00B8)
#define VIC_INT_CLEAR3      VIC_REG(0x00BC)
#define VIC_SOFTINT0        VIC_REG(0x00C0)
#define VIC_SOFTINT1        VIC_REG(0x00C4)
#define VIC_SOFTINT2        VIC_REG(0x00C8)
#define VIC_SOFTINT3        VIC_REG(0x00CC)
#define VIC_IRQ_VEC_RD      VIC_REG(0x00D0)  /* pending int # */
#define VIC_IRQ_VEC_PEND_RD VIC_REG(0x00D4)  /* pending vector addr */
#define VIC_IRQ_VEC_WR      VIC_REG(0x00D8)

#if defined(CONFIG_ARCH_MSM_SCORPION) && !defined(CONFIG_MSM_SMP)
#define VIC_FIQ_VEC_RD      VIC_REG(0x00DC)
#define VIC_FIQ_VEC_PEND_RD VIC_REG(0x00E0)
#define VIC_FIQ_VEC_WR      VIC_REG(0x00E4)
#define VIC_IRQ_IN_SERVICE  VIC_REG(0x00E8)
#define VIC_IRQ_IN_STACK    VIC_REG(0x00EC)
#define VIC_FIQ_IN_SERVICE  VIC_REG(0x00F0)
#define VIC_FIQ_IN_STACK    VIC_REG(0x00F4)
#define VIC_TEST_BUS_SEL    VIC_REG(0x00F8)
#define VIC_IRQ_CTRL_CONFIG VIC_REG(0x00FC)
#else
#define VIC_IRQ_IN_SERVICE  VIC_REG(0x00E0)
#define VIC_IRQ_IN_STACK    VIC_REG(0x00E4)
#define VIC_TEST_BUS_SEL    VIC_REG(0x00E8)
#endif

#define VIC_VECTPRIORITY(n) VIC_REG(0x0200+((n) * 4))
#define VIC_VECTADDR(n)     VIC_REG(0x0400+((n) * 4))

#if defined(CONFIG_ARCH_MSM7X30) || defined(CONFIG_ARCH_FSM9XXX)
#define VIC_NUM_REGS	    4
#else
#define VIC_NUM_REGS	    2
#endif

#if VIC_NUM_REGS == 2
#define DPRINT_REGS(base_reg, format, ...)	      			\
	printk(KERN_INFO format " %x %x\n", ##__VA_ARGS__,		\
			readl(base_reg ## 0), readl(base_reg ## 1))
#define DPRINT_ARRAY(array, format, ...)				\
	printk(KERN_INFO format " %x %x\n", ##__VA_ARGS__,		\
			array[0], array[1])
#elif VIC_NUM_REGS == 4
#define DPRINT_REGS(base_reg, format, ...) \
	printk(KERN_INFO format " %x %x %x %x\n", ##__VA_ARGS__,	\
			readl(base_reg ## 0), readl(base_reg ## 1),	\
			readl(base_reg ## 2), readl(base_reg ## 3))
#define DPRINT_ARRAY(array, format, ...)				\
	printk(KERN_INFO format " %x %x %x %x\n", ##__VA_ARGS__,	\
			array[0], array[1],				\
			array[2], array[3])
#else
#error "VIC_NUM_REGS set to illegal value"
#endif

static uint32_t msm_irq_smsm_wake_enable[2];
static struct {
	uint32_t int_en[2];
	uint32_t int_type;
	uint32_t int_polarity;
	uint32_t int_select;
} msm_irq_shadow_reg[VIC_NUM_REGS];
static uint32_t msm_irq_idle_disable[VIC_NUM_REGS];

#define SMSM_FAKE_IRQ (0xff)
#if !defined(CONFIG_ARCH_FSM9XXX)
static uint8_t msm_irq_to_smsm[NR_IRQS] = {
#if !defined(CONFIG_ARCH_MSM7X27A)
	[INT_MDDI_EXT] = 1,
	[INT_MDDI_PRI] = 2,
	[INT_MDDI_CLIENT] = 3,
#endif
	[INT_USB_OTG] = 4,

	[INT_PWB_I2C] = 5,
	[INT_SDC1_0] = 6,
	[INT_SDC1_1] = 7,
	[INT_SDC2_0] = 8,

	[INT_SDC2_1] = 9,
	[INT_ADSP_A9_A11] = 10,
	[INT_UART1] = 11,
	[INT_UART2] = 12,

	[INT_UART3] = 13,
	[INT_UART1_RX] = 14,
	[INT_UART2_RX] = 15,
	[INT_UART3_RX] = 16,

	[INT_UART1DM_IRQ] = 17,
	[INT_UART1DM_RX] = 18,
	[INT_KEYSENSE] = 19,
#if !defined(CONFIG_ARCH_MSM7X30)
	[INT_AD_HSSD] = 20,
#endif

	[INT_NAND_WR_ER_DONE] = 21,
	[INT_NAND_OP_DONE] = 22,
	[INT_TCHSCRN1] = 23,
	[INT_TCHSCRN2] = 24,

	[INT_TCHSCRN_SSBI] = 25,
	[INT_USB_HS] = 26,
	[INT_UART2DM_RX] = 27,
	[INT_UART2DM_IRQ] = 28,

	[INT_SDC4_1] = 29,
	[INT_SDC4_0] = 30,
	[INT_SDC3_1] = 31,
	[INT_SDC3_0] = 32,

	/* fake wakeup interrupts */
	[INT_GPIO_GROUP1] = SMSM_FAKE_IRQ,
	[INT_GPIO_GROUP2] = SMSM_FAKE_IRQ,
	[INT_A9_M2A_0] = SMSM_FAKE_IRQ,
	[INT_A9_M2A_1] = SMSM_FAKE_IRQ,
	[INT_A9_M2A_5] = SMSM_FAKE_IRQ,
	[INT_GP_TIMER_EXP] = SMSM_FAKE_IRQ,
	[INT_DEBUG_TIMER_EXP] = SMSM_FAKE_IRQ,
	[INT_ADSP_A11] = SMSM_FAKE_IRQ,
#ifdef CONFIG_ARCH_QSD8X50
	[INT_SIRC_0] = SMSM_FAKE_IRQ,
	[INT_SIRC_1] = SMSM_FAKE_IRQ,
#endif
};
# else /* CONFIG_ARCH_FSM9XXX */
static uint8_t msm_irq_to_smsm[NR_IRQS] = {
	[INT_UART1] = 11,
	[INT_A9_M2A_0] = SMSM_FAKE_IRQ,
	[INT_A9_M2A_1] = SMSM_FAKE_IRQ,
	[INT_A9_M2A_5] = SMSM_FAKE_IRQ,
	[INT_GP_TIMER_EXP] = SMSM_FAKE_IRQ,
	[INT_DEBUG_TIMER_EXP] = SMSM_FAKE_IRQ,
	[INT_SIRC_0] = 10,
	[INT_ADSP_A11] = SMSM_FAKE_IRQ,
};
#endif /* CONFIG_ARCH_FSM9XXX */

static inline void msm_irq_write_all_regs(void __iomem *base, unsigned int val)
{
	int i;

	for (i = 0; i < VIC_NUM_REGS; i++)
		writel(val, base + (i * 4));
}

static void msm_irq_ack(struct irq_data *d)
{
	uint32_t mask;

	void __iomem *reg = VIC_INT_TO_REG_ADDR(VIC_INT_CLEAR0, d->irq);
	mask = 1 << (d->irq & 31);
	writel(mask, reg);
	mb();
}

static void msm_irq_disable(struct irq_data *d)
{
	void __iomem *reg = VIC_INT_TO_REG_ADDR(VIC_INT_ENCLEAR0, d->irq);
	unsigned index = VIC_INT_TO_REG_INDEX(d->irq);
	uint32_t mask = 1UL << (d->irq & 31);
	int smsm_irq = msm_irq_to_smsm[d->irq];

	if (!(msm_irq_shadow_reg[index].int_en[1] & mask)) {
		msm_irq_shadow_reg[index].int_en[0] &= ~mask;
		writel(mask, reg);
		mb();
		if (smsm_irq == 0)
			msm_irq_idle_disable[index] &= ~mask;
		else {
			mask = 1UL << (smsm_irq - 1);
			msm_irq_smsm_wake_enable[0] &= ~mask;
		}
	}
}

static void msm_irq_mask(struct irq_data *d)
{
	void __iomem *reg = VIC_INT_TO_REG_ADDR(VIC_INT_ENCLEAR0, d->irq);
	unsigned index = VIC_INT_TO_REG_INDEX(d->irq);
	uint32_t mask = 1UL << (d->irq & 31);
	int smsm_irq = msm_irq_to_smsm[d->irq];

	msm_irq_shadow_reg[index].int_en[0] &= ~mask;
	writel(mask, reg);
	mb();
	if (smsm_irq == 0)
		msm_irq_idle_disable[index] &= ~mask;
	else {
		mask = 1UL << (smsm_irq - 1);
		msm_irq_smsm_wake_enable[0] &= ~mask;
	}
}

static void msm_irq_unmask(struct irq_data *d)
{
	void __iomem *reg = VIC_INT_TO_REG_ADDR(VIC_INT_ENSET0, d->irq);
	unsigned index = VIC_INT_TO_REG_INDEX(d->irq);
	uint32_t mask = 1UL << (d->irq & 31);
	int smsm_irq = msm_irq_to_smsm[d->irq];

	msm_irq_shadow_reg[index].int_en[0] |= mask;
	writel(mask, reg);
	mb();

	if (smsm_irq == 0)
		msm_irq_idle_disable[index] |= mask;
	else {
		mask = 1UL << (smsm_irq - 1);
		msm_irq_smsm_wake_enable[0] |= mask;
	}
}

static int msm_irq_set_wake(struct irq_data *d, unsigned int on)
{
	unsigned index = VIC_INT_TO_REG_INDEX(d->irq);
	uint32_t mask = 1UL << (d->irq & 31);
	int smsm_irq = msm_irq_to_smsm[d->irq];

	if (smsm_irq == 0) {
		printk(KERN_ERR "msm_irq_set_wake: bad wakeup irq %d\n", d->irq);
		return -EINVAL;
	}
	if (on)
		msm_irq_shadow_reg[index].int_en[1] |= mask;
	else
		msm_irq_shadow_reg[index].int_en[1] &= ~mask;

	if (smsm_irq == SMSM_FAKE_IRQ)
		return 0;

	mask = 1UL << (smsm_irq - 1);
	if (on)
		msm_irq_smsm_wake_enable[1] |= mask;
	else
		msm_irq_smsm_wake_enable[1] &= ~mask;
	return 0;
}

static int msm_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
        void __iomem *treg = VIC_INT_TO_REG_ADDR(VIC_INT_TYPE0, d->irq);
	void __iomem *preg = VIC_INT_TO_REG_ADDR(VIC_INT_POLARITY0, d->irq);
	unsigned index = VIC_INT_TO_REG_INDEX(d->irq);
	int b = 1 << (d->irq & 31);
	uint32_t polarity;
	uint32_t type;

	polarity = msm_irq_shadow_reg[index].int_polarity;
	if (flow_type & (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW))
		polarity |= b;
	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_HIGH))
		polarity &= ~b;
	writel(polarity, preg);
	msm_irq_shadow_reg[index].int_polarity = polarity;

	type = msm_irq_shadow_reg[index].int_type;
	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		type |= b;
		__irq_set_handler_locked(d->irq, handle_edge_irq);
	}
	if (flow_type & (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW)) {
		type &= ~b;
		__irq_set_handler_locked(d->irq, handle_level_irq);
	}
	writel(type, treg);
	mb();
	msm_irq_shadow_reg[index].int_type = type;
	return 0;
}

unsigned int msm_irq_pending(void)
{
	unsigned int i, pending = 0;

	for (i = 0; (i < VIC_NUM_REGS) && !pending; i++)
		pending |= readl(VIC_IRQ_STATUS0 + (i * 4));

	return pending;
}

int msm_irq_idle_sleep_allowed(void)
{
	uint32_t i, disable = 0;

	if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP_REQUEST)
		DPRINT_ARRAY(msm_irq_idle_disable,
			     "msm_irq_idle_sleep_allowed: disable");

	for (i = 0; i < VIC_NUM_REGS; i++)
		disable |= msm_irq_idle_disable[i];

	return !disable;
}

/*
 * Prepare interrupt subsystem for entering sleep -- phase 1.
 * If modem_wake is true, return currently enabled interrupts in *irq_mask.
 */
void msm_irq_enter_sleep1(bool modem_wake, int from_idle, uint32_t *irq_mask)
{
	if (modem_wake) {
		*irq_mask = msm_irq_smsm_wake_enable[!from_idle];
		if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP)
			printk(KERN_INFO
				"%s irq_mask %x\n", __func__, *irq_mask);
	}
}

/*
 * Prepare interrupt subsystem for entering sleep -- phase 2.
 * Detect any pending interrupts and configure interrupt hardware.
 *
 * Return value:
 * -EAGAIN: there are pending interrupt(s); interrupt configuration
 *          is not changed.
 *       0: success
 */
int msm_irq_enter_sleep2(bool modem_wake, int from_idle)
{
	int i, limit = 10;
	uint32_t pending[VIC_NUM_REGS];

	if (from_idle && !modem_wake)
		return 0;

	/* edge triggered interrupt may get lost if this mode is used */
	WARN_ON_ONCE(!modem_wake && !from_idle);

	if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP)
		DPRINT_REGS(VIC_IRQ_STATUS, "%s change irq, pend", __func__);

	for (i = 0; i < VIC_NUM_REGS; i++) {
		pending[i] = readl(VIC_IRQ_STATUS0 + (i * 4));
		pending[i] &= msm_irq_shadow_reg[i].int_en[!from_idle];
	}

	/*
	 * Clear INT_A9_M2A_5 since requesting sleep triggers it.
	 * In some arch e.g. FSM9XXX, INT_A9_M2A_5 may not be in the first set.
	 */
	pending[INT_A9_M2A_5 / 32] &= ~(1U << (INT_A9_M2A_5 % 32));

	for (i = 0; i < VIC_NUM_REGS; i++) {
		if (pending[i]) {
			if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP_ABORT)
				DPRINT_ARRAY(pending, "%s abort",
						       __func__);
			return -EAGAIN;
		}
	}

	msm_irq_write_all_regs(VIC_INT_EN0, 0);

	while (limit-- > 0) {
		int pend_irq;
		int irq = readl(VIC_IRQ_VEC_RD);
		if (irq == -1)
			break;
		pend_irq = readl(VIC_IRQ_VEC_PEND_RD);
		if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP_INT)
			printk(KERN_INFO "%s cleared int %d (%d)\n",
				__func__, irq, pend_irq);
	}

	if (modem_wake) {
		struct irq_data d = { .irq = INT_A9_M2A_6 };
		msm_irq_set_type(&d, IRQF_TRIGGER_RISING);
		__raw_writel(1U << (INT_A9_M2A_6 % 32),
			VIC_INT_TO_REG_ADDR(VIC_INT_ENSET0, INT_A9_M2A_6));
	} else {
		for (i = 0; i < VIC_NUM_REGS; i++)
			writel(msm_irq_shadow_reg[i].int_en[1],
						VIC_INT_ENSET0 + (i * 4));
	}
	mb();

	return 0;
}

/*
 * Restore interrupt subsystem from sleep -- phase 1.
 * Configure interrupt hardware.
 */
void msm_irq_exit_sleep1(uint32_t irq_mask, uint32_t wakeup_reason,
	uint32_t pending_irqs)
{
	int i;
	struct irq_data d = { .irq = INT_A9_M2A_6 };

	msm_irq_ack(&d);

	for (i = 0; i < VIC_NUM_REGS; i++) {
		writel(msm_irq_shadow_reg[i].int_type,
			VIC_INT_TYPE0 + i * 4);
		writel(msm_irq_shadow_reg[i].int_polarity,
			VIC_INT_POLARITY0 + i * 4);
		writel(msm_irq_shadow_reg[i].int_en[0],
			VIC_INT_EN0 + i * 4);
		writel(msm_irq_shadow_reg[i].int_select,
			VIC_INT_SELECT0 + i * 4);
	}

	writel(3, VIC_INT_MASTEREN);
	mb();

	if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP)
		DPRINT_REGS(VIC_IRQ_STATUS, "%s %x %x %x now",
			__func__, irq_mask, pending_irqs, wakeup_reason);
}

/*
 * Restore interrupt subsystem from sleep -- phase 2.
 * Poke the specified pending interrupts into interrupt hardware.
 */
void msm_irq_exit_sleep2(uint32_t irq_mask, uint32_t wakeup_reason,
	uint32_t pending)
{
	int i;

	if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP)
		DPRINT_REGS(VIC_IRQ_STATUS, "%s %x %x %x now",
			__func__, irq_mask, pending, wakeup_reason);

	for (i = 0; pending && i < ARRAY_SIZE(msm_irq_to_smsm); i++) {
		unsigned reg_offset = VIC_INT_TO_REG_ADDR(0, i);
		uint32_t reg_mask = 1UL << (i & 31);
		int smsm_irq = msm_irq_to_smsm[i];
		uint32_t smsm_mask;

		if (smsm_irq == 0)
			continue;

		smsm_mask = 1U << (smsm_irq - 1);
		if (!(pending & smsm_mask))
			continue;

		pending &= ~smsm_mask;
		if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP_INT)
			DPRINT_REGS(VIC_IRQ_STATUS,
				"%s: irq %d still pending %x now",
				__func__, i, pending);
#ifdef DEBUG_INTERRUPT_TRIGGER
		if (readl(VIC_IRQ_STATUS0 + reg_offset) & reg_mask)
			writel(reg_mask, VIC_INT_CLEAR0 + reg_offset);
#endif
		if (readl(VIC_IRQ_STATUS0 + reg_offset) & reg_mask)
			continue;

		writel(reg_mask, VIC_SOFTINT0 + reg_offset);

		if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP_INT_TRIGGER)
			DPRINT_REGS(VIC_IRQ_STATUS,
				"%s: irq %d need trigger, now",
				__func__, i);
	}
	mb();
}

/*
 * Restore interrupt subsystem from sleep -- phase 3.
 * Print debug information.
 */
void msm_irq_exit_sleep3(uint32_t irq_mask, uint32_t wakeup_reason,
	uint32_t pending_irqs)
{
	if (msm_irq_debug_mask & IRQ_DEBUG_SLEEP)
		DPRINT_REGS(VIC_IRQ_STATUS, "%s %x %x %x state %x now",
			__func__, irq_mask, pending_irqs, wakeup_reason,
			smsm_get_state(SMSM_MODEM_STATE));
}

static struct irq_chip msm_irq_chip = {
	.name		= "msm",
	.irq_disable	= msm_irq_disable,
	.irq_ack	= msm_irq_ack,
	.irq_mask	= msm_irq_mask,
	.irq_unmask	= msm_irq_unmask,
	.irq_set_wake	= msm_irq_set_wake,
	.irq_set_type	= msm_irq_set_type,
};

void __init msm_init_irq(void)
{
	unsigned n;

	/* select level interrupts */
	msm_irq_write_all_regs(VIC_INT_TYPE0, 0);

	/* select highlevel interrupts */
	msm_irq_write_all_regs(VIC_INT_POLARITY0, 0);

	/* select IRQ for all INTs */
	msm_irq_write_all_regs(VIC_INT_SELECT0, 0);

	/* disable all INTs */
	msm_irq_write_all_regs(VIC_INT_EN0, 0);

	/* don't use vic */
	writel(0, VIC_CONFIG);


	for (n = 0; n < NR_MSM_IRQS; n++) {
		irq_set_chip_and_handler(n, &msm_irq_chip, handle_level_irq);
		set_irq_flags(n, IRQF_VALID);
	}

	/* enable interrupt controller */
	writel(3, VIC_INT_MASTEREN);
	mb();
}

static inline void msm_vic_handle_irq(void __iomem *base_addr, struct pt_regs
		*regs)
{
	u32 irqnr;

	do {
		/* 0xD0 has irq# or old irq# if the irq has been handled
		 * 0xD4 has irq# or -1 if none pending *but* if you just
		 * read 0xD4 you never get the first irq for some reason
		 */
		irqnr = readl_relaxed(base_addr + 0xD0);
		irqnr = readl_relaxed(base_addr + 0xD4);
		if (irqnr == -1)
			break;
		handle_IRQ(irqnr, regs);
	} while (1);
}

/* enable imprecise aborts */
#define local_cpsie_enable()  __asm__ __volatile__("cpsie a    @ enable")

asmlinkage void __exception_irq_entry vic_handle_irq(struct pt_regs *regs)
{
	local_cpsie_enable();
	msm_vic_handle_irq((void __iomem *)MSM_VIC_BASE, regs);
}

#if defined(CONFIG_MSM_FIQ_SUPPORT)
void msm_trigger_irq(int irq)
{
	void __iomem *reg = VIC_INT_TO_REG_ADDR(VIC_SOFTINT0, irq);
	uint32_t mask = 1UL << (irq & 31);
	writel(mask, reg);
	mb();
}

void msm_fiq_enable(int irq)
{
	struct irq_data d = { .irq = irq };
	unsigned long flags;
	local_irq_save(flags);
	msm_irq_unmask(&d);
	local_irq_restore(flags);
}

void msm_fiq_disable(int irq)
{
	struct irq_data d = { .irq = irq };
	unsigned long flags;
	local_irq_save(flags);
	msm_irq_mask(&d);
	local_irq_restore(flags);
}

void msm_fiq_select(int irq)
{
	void __iomem *reg = VIC_INT_TO_REG_ADDR(VIC_INT_SELECT0, irq);
	unsigned index = VIC_INT_TO_REG_INDEX(irq);
	uint32_t mask = 1UL << (irq & 31);
	unsigned long flags;

	local_irq_save(flags);
	msm_irq_shadow_reg[index].int_select |= mask;
	writel(msm_irq_shadow_reg[index].int_select, reg);
	mb();
	local_irq_restore(flags);
}

void msm_fiq_unselect(int irq)
{
	void __iomem *reg = VIC_INT_TO_REG_ADDR(VIC_INT_SELECT0, irq);
	unsigned index = VIC_INT_TO_REG_INDEX(irq);
	uint32_t mask = 1UL << (irq & 31);
	unsigned long flags;

	local_irq_save(flags);
	msm_irq_shadow_reg[index].int_select &= (!mask);
	writel(msm_irq_shadow_reg[index].int_select, reg);
	mb();
	local_irq_restore(flags);
}
/* set_fiq_handler originally from arch/arm/kernel/fiq.c */
static void set_fiq_handler(void *start, unsigned int length)
{
	memcpy((void *)0xffff001c, start, length);
	flush_icache_range(0xffff001c, 0xffff001c + length);
	if (!vectors_high())
		flush_icache_range(0x1c, 0x1c + length);
}

static void (*fiq_func)(void *data, void *regs);
static unsigned long long fiq_stack[256];

int msm_fiq_set_handler(void (*func)(void *data, void *regs), void *data)
{
	unsigned long flags;
	int ret = -ENOMEM;

	local_irq_save(flags);
	if (fiq_func == 0) {
		fiq_func = func;
		fiq_glue_setup(func, data, fiq_stack + 255);
		set_fiq_handler(&fiq_glue, (&fiq_glue_end - &fiq_glue));
		ret = 0;
	}
	local_irq_restore(flags);
	return ret;
}
#endif
