/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/hardware/gic.h>
#include <mach/msm_iomap.h>
#include <mach/gpio.h>

#include <mach/mpm.h>

/******************************************************************************
 * Debug Definitions
 *****************************************************************************/

enum {
	MSM_MPM_DEBUG_NON_DETECTABLE_IRQ = BIT(0),
	MSM_MPM_DEBUG_PENDING_IRQ = BIT(1),
	MSM_MPM_DEBUG_WRITE = BIT(2),
	MSM_MPM_DEBUG_NON_DETECTABLE_IRQ_IDLE = BIT(3),
};

static int msm_mpm_debug_mask = 1;
module_param_named(
	debug_mask, msm_mpm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

/******************************************************************************
 * Request and Status Definitions
 *****************************************************************************/

enum {
	MSM_MPM_REQUEST_REG_ENABLE,
	MSM_MPM_REQUEST_REG_DETECT_CTL,
	MSM_MPM_REQUEST_REG_POLARITY,
	MSM_MPM_REQUEST_REG_CLEAR,
};

enum {
	MSM_MPM_STATUS_REG_PENDING,
};

/******************************************************************************
 * IRQ Mapping Definitions
 *****************************************************************************/

#define MSM_MPM_NR_APPS_IRQS  (NR_MSM_IRQS + NR_GPIO_IRQS)

#define MSM_MPM_REG_WIDTH  DIV_ROUND_UP(MSM_MPM_NR_MPM_IRQS, 32)
#define MSM_MPM_IRQ_INDEX(irq)  (irq / 32)
#define MSM_MPM_IRQ_MASK(irq)  BIT(irq % 32)

static struct msm_mpm_device_data msm_mpm_dev_data;
static uint8_t msm_mpm_irqs_a2m[MSM_MPM_NR_APPS_IRQS];

static DEFINE_SPINLOCK(msm_mpm_lock);

/*
 * Note: the following two bitmaps only mark irqs that are _not_
 * mappable to MPM.
 */
static DECLARE_BITMAP(msm_mpm_enabled_apps_irqs, MSM_MPM_NR_APPS_IRQS);
static DECLARE_BITMAP(msm_mpm_wake_apps_irqs, MSM_MPM_NR_APPS_IRQS);

static DECLARE_BITMAP(msm_mpm_gpio_irqs_mask, MSM_MPM_NR_APPS_IRQS);

static uint32_t msm_mpm_enabled_irq[MSM_MPM_REG_WIDTH];
static uint32_t msm_mpm_wake_irq[MSM_MPM_REG_WIDTH];
static uint32_t msm_mpm_detect_ctl[MSM_MPM_REG_WIDTH];
static uint32_t msm_mpm_polarity[MSM_MPM_REG_WIDTH];


/******************************************************************************
 * Low Level Functions for Accessing MPM
 *****************************************************************************/

static inline uint32_t msm_mpm_read(
	unsigned int reg, unsigned int subreg_index)
{
	unsigned int offset = reg * MSM_MPM_REG_WIDTH + subreg_index;
	return __raw_readl(msm_mpm_dev_data.mpm_status_reg_base + offset * 4);
}

static inline void msm_mpm_write(
	unsigned int reg, unsigned int subreg_index, uint32_t value)
{
	unsigned int offset = reg * MSM_MPM_REG_WIDTH + subreg_index;
	__raw_writel(value, msm_mpm_dev_data.mpm_request_reg_base + offset * 4);

	if (MSM_MPM_DEBUG_WRITE & msm_mpm_debug_mask)
		pr_info("%s: reg %u.%u: 0x%08x\n",
			__func__, reg, subreg_index, value);
}

static inline void msm_mpm_send_interrupt(void)
{
	__raw_writel(msm_mpm_dev_data.mpm_apps_ipc_val,
			msm_mpm_dev_data.mpm_apps_ipc_reg);
	/* Ensure the write is complete before returning. */
	mb();
}

static irqreturn_t msm_mpm_irq(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

/******************************************************************************
 * MPM Access Functions
 *****************************************************************************/

static void msm_mpm_set(bool wakeset)
{
	uint32_t *irqs;
	unsigned int reg;
	int i;

	irqs = wakeset ? msm_mpm_wake_irq : msm_mpm_enabled_irq;
	for (i = 0; i < MSM_MPM_REG_WIDTH; i++) {
		reg = MSM_MPM_REQUEST_REG_ENABLE;
		msm_mpm_write(reg, i, irqs[i]);

		reg = MSM_MPM_REQUEST_REG_DETECT_CTL;
		msm_mpm_write(reg, i, msm_mpm_detect_ctl[i]);

		reg = MSM_MPM_REQUEST_REG_POLARITY;
		msm_mpm_write(reg, i, msm_mpm_polarity[i]);

		reg = MSM_MPM_REQUEST_REG_CLEAR;
		msm_mpm_write(reg, i, 0xffffffff);
	}

	/* Ensure that the set operation is complete before sending the
	 * interrupt
	 */
	mb();
	msm_mpm_send_interrupt();
}

static void msm_mpm_clear(void)
{
	int i;

	for (i = 0; i < MSM_MPM_REG_WIDTH; i++) {
		msm_mpm_write(MSM_MPM_REQUEST_REG_ENABLE, i, 0);
		msm_mpm_write(MSM_MPM_REQUEST_REG_CLEAR, i, 0xffffffff);
	}

	/* Ensure the clear is complete before sending the interrupt */
	mb();
	msm_mpm_send_interrupt();
}

/******************************************************************************
 * Interrupt Mapping Functions
 *****************************************************************************/

static inline bool msm_mpm_is_valid_apps_irq(unsigned int irq)
{
	return irq < ARRAY_SIZE(msm_mpm_irqs_a2m);
}

static inline uint8_t msm_mpm_get_irq_a2m(unsigned int irq)
{
	return msm_mpm_irqs_a2m[irq];
}

static inline void msm_mpm_set_irq_a2m(unsigned int apps_irq,
	unsigned int mpm_irq)
{
	msm_mpm_irqs_a2m[apps_irq] = (uint8_t) mpm_irq;
}

static inline bool msm_mpm_is_valid_mpm_irq(unsigned int irq)
{
	return irq < msm_mpm_dev_data.irqs_m2a_size;
}

static inline uint16_t msm_mpm_get_irq_m2a(unsigned int irq)
{
	return msm_mpm_dev_data.irqs_m2a[irq];
}

static bool msm_mpm_bypass_apps_irq(unsigned int irq)
{
	int i;

	for (i = 0; i < msm_mpm_dev_data.bypassed_apps_irqs_size; i++)
		if (irq == msm_mpm_dev_data.bypassed_apps_irqs[i])
			return true;

	return false;
}

static int msm_mpm_enable_irq_exclusive(
	unsigned int irq, bool enable, bool wakeset)
{
	uint32_t mpm_irq;

	if (!msm_mpm_is_valid_apps_irq(irq))
		return -EINVAL;

	if (msm_mpm_bypass_apps_irq(irq))
		return 0;

	mpm_irq = msm_mpm_get_irq_a2m(irq);
	if (mpm_irq) {
		uint32_t *mpm_irq_masks = wakeset ?
				msm_mpm_wake_irq : msm_mpm_enabled_irq;
		uint32_t index = MSM_MPM_IRQ_INDEX(mpm_irq);
		uint32_t mask = MSM_MPM_IRQ_MASK(mpm_irq);

		if (enable)
			mpm_irq_masks[index] |= mask;
		else
			mpm_irq_masks[index] &= ~mask;
	} else {
		unsigned long *apps_irq_bitmap = wakeset ?
			msm_mpm_wake_apps_irqs : msm_mpm_enabled_apps_irqs;

		if (enable)
			__set_bit(irq, apps_irq_bitmap);
		else
			__clear_bit(irq, apps_irq_bitmap);
	}

	return 0;
}

static int msm_mpm_set_irq_type_exclusive(
	unsigned int irq, unsigned int flow_type)
{
	uint32_t mpm_irq;

	if (!msm_mpm_is_valid_apps_irq(irq))
		return -EINVAL;

	if (msm_mpm_bypass_apps_irq(irq))
		return 0;

	mpm_irq = msm_mpm_get_irq_a2m(irq);
	if (mpm_irq) {
		uint32_t index = MSM_MPM_IRQ_INDEX(mpm_irq);
		uint32_t mask = MSM_MPM_IRQ_MASK(mpm_irq);

		if (index >= MSM_MPM_REG_WIDTH)
			return -EFAULT;

		if (flow_type & IRQ_TYPE_EDGE_BOTH)
			msm_mpm_detect_ctl[index] |= mask;
		else
			msm_mpm_detect_ctl[index] &= ~mask;

		if (flow_type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_LEVEL_HIGH))
			msm_mpm_polarity[index] |= mask;
		else
			msm_mpm_polarity[index] &= ~mask;
	}

	return 0;
}

static int __msm_mpm_enable_irq(unsigned int irq, unsigned int enable)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&msm_mpm_lock, flags);
	rc = msm_mpm_enable_irq_exclusive(irq, (bool)enable, false);
	spin_unlock_irqrestore(&msm_mpm_lock, flags);

	return rc;
}

static void msm_mpm_enable_irq(struct irq_data *d)
{
	__msm_mpm_enable_irq(d->irq, 1);
}

static void msm_mpm_disable_irq(struct irq_data *d)
{
	__msm_mpm_enable_irq(d->irq, 0);
}

static int msm_mpm_set_irq_wake(struct irq_data *d, unsigned int on)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&msm_mpm_lock, flags);
	rc = msm_mpm_enable_irq_exclusive(d->irq, (bool)on, true);
	spin_unlock_irqrestore(&msm_mpm_lock, flags);

	return rc;
}

static int msm_mpm_set_irq_type(struct irq_data *d, unsigned int flow_type)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&msm_mpm_lock, flags);
	rc = msm_mpm_set_irq_type_exclusive(d->irq, flow_type);
	spin_unlock_irqrestore(&msm_mpm_lock, flags);

	return rc;
}

/******************************************************************************
 * Public functions
 *****************************************************************************/
int msm_mpm_enable_pin(unsigned int pin, unsigned int enable)
{
	uint32_t index = MSM_MPM_IRQ_INDEX(pin);
	uint32_t mask = MSM_MPM_IRQ_MASK(pin);
	unsigned long flags;

	spin_lock_irqsave(&msm_mpm_lock, flags);

	if (enable)
		msm_mpm_enabled_irq[index] |= mask;
	else
		msm_mpm_enabled_irq[index] &= ~mask;

	spin_unlock_irqrestore(&msm_mpm_lock, flags);
	return 0;
}

int msm_mpm_set_pin_wake(unsigned int pin, unsigned int on)
{
	uint32_t index = MSM_MPM_IRQ_INDEX(pin);
	uint32_t mask = MSM_MPM_IRQ_MASK(pin);
	unsigned long flags;

	spin_lock_irqsave(&msm_mpm_lock, flags);

	if (on)
		msm_mpm_wake_irq[index] |= mask;
	else
		msm_mpm_wake_irq[index] &= ~mask;

	spin_unlock_irqrestore(&msm_mpm_lock, flags);
	return 0;
}

int msm_mpm_set_pin_type(unsigned int pin, unsigned int flow_type)
{
	uint32_t index = MSM_MPM_IRQ_INDEX(pin);
	uint32_t mask = MSM_MPM_IRQ_MASK(pin);
	unsigned long flags;

	spin_lock_irqsave(&msm_mpm_lock, flags);

	if (flow_type & IRQ_TYPE_EDGE_BOTH)
		msm_mpm_detect_ctl[index] |= mask;
	else
		msm_mpm_detect_ctl[index] &= ~mask;

	if (flow_type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_LEVEL_HIGH))
		msm_mpm_polarity[index] |= mask;
	else
		msm_mpm_polarity[index] &= ~mask;

	spin_unlock_irqrestore(&msm_mpm_lock, flags);
	return 0;
}

bool msm_mpm_irqs_detectable(bool from_idle)
{
	unsigned long *apps_irq_bitmap;
	int debug_mask;

	if (from_idle) {
		apps_irq_bitmap = msm_mpm_enabled_apps_irqs;
		debug_mask = msm_mpm_debug_mask &
					MSM_MPM_DEBUG_NON_DETECTABLE_IRQ_IDLE;
	} else {
		apps_irq_bitmap = msm_mpm_wake_apps_irqs;
		debug_mask = msm_mpm_debug_mask &
					MSM_MPM_DEBUG_NON_DETECTABLE_IRQ;
	}

	if (debug_mask) {
		static char buf[DIV_ROUND_UP(MSM_MPM_NR_APPS_IRQS, 32)*9+1];

		bitmap_scnprintf(buf, sizeof(buf), apps_irq_bitmap,
				MSM_MPM_NR_APPS_IRQS);
		buf[sizeof(buf) - 1] = '\0';

		pr_info("%s: cannot monitor %s", __func__, buf);
	}

	return (bool)__bitmap_empty(apps_irq_bitmap, MSM_MPM_NR_APPS_IRQS);
}

bool msm_mpm_gpio_irqs_detectable(bool from_idle)
{
	unsigned long *apps_irq_bitmap = from_idle ?
			msm_mpm_enabled_apps_irqs : msm_mpm_wake_apps_irqs;

	return !__bitmap_intersects(msm_mpm_gpio_irqs_mask, apps_irq_bitmap,
			MSM_MPM_NR_APPS_IRQS);
}

void msm_mpm_enter_sleep(bool from_idle)
{
	msm_mpm_set(!from_idle);
}

void msm_mpm_exit_sleep(bool from_idle)
{
	unsigned long pending;
	int i;
	int k;

	for (i = 0; i < MSM_MPM_REG_WIDTH; i++) {
		pending = msm_mpm_read(MSM_MPM_STATUS_REG_PENDING, i);

		if (MSM_MPM_DEBUG_PENDING_IRQ & msm_mpm_debug_mask)
			pr_info("%s: pending.%d: 0x%08lx", __func__,
					i, pending);

		k = find_first_bit(&pending, 32);
		while (k < 32) {
			unsigned int mpm_irq = 32 * i + k;
			unsigned int apps_irq = msm_mpm_get_irq_m2a(mpm_irq);
			struct irq_desc *desc = apps_irq ?
				irq_to_desc(apps_irq) : NULL;

			if (desc && !irqd_is_level_type(&desc->irq_data)) {
				irq_set_pending(apps_irq);
				if (from_idle)
					check_irq_resend(desc, apps_irq);
			}

			k = find_next_bit(&pending, 32, k + 1);
		}
	}

	msm_mpm_clear();
}

static int __init msm_mpm_early_init(void)
{
	uint8_t mpm_irq;
	uint16_t apps_irq;

	for (mpm_irq = 0; msm_mpm_is_valid_mpm_irq(mpm_irq); mpm_irq++) {
		apps_irq = msm_mpm_get_irq_m2a(mpm_irq);
		if (apps_irq && msm_mpm_is_valid_apps_irq(apps_irq))
			msm_mpm_set_irq_a2m(apps_irq, mpm_irq);
	}

	return 0;
}
core_initcall(msm_mpm_early_init);

void __init msm_mpm_irq_extn_init(struct msm_mpm_device_data *mpm_data)
{
	gic_arch_extn.irq_mask = msm_mpm_disable_irq;
	gic_arch_extn.irq_unmask = msm_mpm_enable_irq;
	gic_arch_extn.irq_disable = msm_mpm_disable_irq;
	gic_arch_extn.irq_set_type = msm_mpm_set_irq_type;
	gic_arch_extn.irq_set_wake = msm_mpm_set_irq_wake;

	msm_gpio_irq_extn.irq_mask = msm_mpm_disable_irq;
	msm_gpio_irq_extn.irq_unmask = msm_mpm_enable_irq;
	msm_gpio_irq_extn.irq_disable = msm_mpm_disable_irq;
	msm_gpio_irq_extn.irq_set_type = msm_mpm_set_irq_type;
	msm_gpio_irq_extn.irq_set_wake = msm_mpm_set_irq_wake;

	bitmap_set(msm_mpm_gpio_irqs_mask, NR_MSM_IRQS, NR_GPIO_IRQS);

	if (!mpm_data) {
#ifdef CONFIG_MSM_MPM
		BUG();
#endif
		return;
	}

	memcpy(&msm_mpm_dev_data, mpm_data, sizeof(struct msm_mpm_device_data));

	msm_mpm_dev_data.irqs_m2a =
		kzalloc(msm_mpm_dev_data.irqs_m2a_size * sizeof(uint16_t),
			GFP_KERNEL);
	BUG_ON(!msm_mpm_dev_data.irqs_m2a);
	memcpy(msm_mpm_dev_data.irqs_m2a, mpm_data->irqs_m2a,
		msm_mpm_dev_data.irqs_m2a_size * sizeof(uint16_t));
	msm_mpm_dev_data.bypassed_apps_irqs =
		kzalloc(msm_mpm_dev_data.bypassed_apps_irqs_size *
			sizeof(uint16_t), GFP_KERNEL);
	BUG_ON(!msm_mpm_dev_data.bypassed_apps_irqs);
	memcpy(msm_mpm_dev_data.bypassed_apps_irqs,
		mpm_data->bypassed_apps_irqs,
		msm_mpm_dev_data.bypassed_apps_irqs_size * sizeof(uint16_t));
}

static int __init msm_mpm_init(void)
{
	unsigned int irq = msm_mpm_dev_data.mpm_ipc_irq;
	int rc;

	rc = request_irq(irq, msm_mpm_irq,
			IRQF_TRIGGER_RISING, "mpm_drv", msm_mpm_irq);

	if (rc) {
		pr_err("%s: failed to request irq %u: %d\n",
			__func__, irq, rc);
		goto init_bail;
	}

	rc = irq_set_irq_wake(irq, 1);
	if (rc) {
		pr_err("%s: failed to set wakeup irq %u: %d\n",
			__func__, irq, rc);
		goto init_free_bail;
	}

	return 0;

init_free_bail:
	free_irq(irq, msm_mpm_irq);

init_bail:
	return rc;
}
device_initcall(msm_mpm_init);
