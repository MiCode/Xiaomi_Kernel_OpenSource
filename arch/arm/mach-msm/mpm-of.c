/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/arch_timer.h>
#include <mach/gpio.h>
#include <mach/mpm.h>
#include <mach/clk.h>
#include <mach/rpm-regulator-smd.h>

enum {
	MSM_MPM_GIC_IRQ_DOMAIN,
	MSM_MPM_GPIO_IRQ_DOMAIN,
	MSM_MPM_NR_IRQ_DOMAINS,
};

enum {
	MSM_MPM_SET_ENABLED,
	MSM_MPM_SET_WAKEUP,
	MSM_NR_IRQS_SET,
};

struct mpm_irqs_a2m {
	struct irq_domain *domain;
	struct device_node *parent;
	irq_hw_number_t hwirq;
	unsigned long pin;
	struct hlist_node node;
};
#define MAX_DOMAIN_NAME 5

struct mpm_irqs {
	struct irq_domain *domain;
	unsigned long *enabled_irqs;
	unsigned long *wakeup_irqs;
	unsigned long size;
	char domain_name[MAX_DOMAIN_NAME];
};

static struct mpm_irqs unlisted_irqs[MSM_MPM_NR_IRQ_DOMAINS];

static struct hlist_head irq_hash[MSM_MPM_NR_MPM_IRQS];
static unsigned int msm_mpm_irqs_m2a[MSM_MPM_NR_MPM_IRQS];
#define MSM_MPM_REG_WIDTH  DIV_ROUND_UP(MSM_MPM_NR_MPM_IRQS, 32)

#define MSM_MPM_IRQ_INDEX(irq)  (irq / 32)
#define MSM_MPM_IRQ_MASK(irq)  BIT(irq % 32)

#define hashfn(val) (val % MSM_MPM_NR_MPM_IRQS)
#define SCLK_HZ (32768)
#define ARCH_TIMER_HZ (19200000)
static struct msm_mpm_device_data msm_mpm_dev_data;

static struct clk *xo_clk;
static bool xo_enabled;
static struct workqueue_struct *msm_mpm_wq;
static struct work_struct msm_mpm_work;
static struct completion wake_wq;

enum mpm_reg_offsets {
	MSM_MPM_REG_WAKEUP,
	MSM_MPM_REG_ENABLE,
	MSM_MPM_REG_FALLING_EDGE,
	MSM_MPM_REG_RISING_EDGE,
	MSM_MPM_REG_POLARITY,
	MSM_MPM_REG_STATUS,
};

static DEFINE_SPINLOCK(msm_mpm_lock);

static uint32_t msm_mpm_enabled_irq[MSM_MPM_REG_WIDTH];
static uint32_t msm_mpm_wake_irq[MSM_MPM_REG_WIDTH];
static uint32_t msm_mpm_falling_edge[MSM_MPM_REG_WIDTH];
static uint32_t msm_mpm_rising_edge[MSM_MPM_REG_WIDTH];
static uint32_t msm_mpm_polarity[MSM_MPM_REG_WIDTH];

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

enum mpm_state {
	MSM_MPM_IRQ_MAPPING_DONE = BIT(0),
	MSM_MPM_DEVICE_PROBED = BIT(1),
};

static enum mpm_state msm_mpm_initialized;

static inline bool msm_mpm_is_initialized(void)
{
	return msm_mpm_initialized &
		(MSM_MPM_IRQ_MAPPING_DONE | MSM_MPM_DEVICE_PROBED);

}

static inline uint32_t msm_mpm_read(
	unsigned int reg, unsigned int subreg_index)
{
	unsigned int offset = reg * MSM_MPM_REG_WIDTH + subreg_index;
	return __raw_readl(msm_mpm_dev_data.mpm_request_reg_base + offset * 4);
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
	wmb();
}

static irqreturn_t msm_mpm_irq(int irq, void *dev_id)
{
	/*
	 * When the system resumes from deep sleep mode, the RPM hardware wakes
	 * up the Apps processor by triggering this interrupt. This interrupt
	 * has to be enabled and set as wake for the irq to get SPM out of
	 * sleep. Handle the interrupt here to make sure that it gets cleared.
	 */
	return IRQ_HANDLED;
}

static void msm_mpm_set(cycle_t wakeup, bool wakeset)
{
	uint32_t *irqs;
	unsigned int reg;
	int i;
	uint32_t *expiry_timer;

	expiry_timer = (uint32_t *)&wakeup;

	irqs = wakeset ? msm_mpm_wake_irq : msm_mpm_enabled_irq;
	for (i = 0; i < MSM_MPM_REG_WIDTH; i++) {
		reg = MSM_MPM_REG_WAKEUP;
		msm_mpm_write(reg, i, expiry_timer[i]);

		reg = MSM_MPM_REG_ENABLE;
		msm_mpm_write(reg, i, irqs[i]);

		reg = MSM_MPM_REG_FALLING_EDGE;
		msm_mpm_write(reg, i, msm_mpm_falling_edge[i]);

		reg = MSM_MPM_REG_RISING_EDGE;
		msm_mpm_write(reg, i, msm_mpm_rising_edge[i]);

		reg = MSM_MPM_REG_POLARITY;
		msm_mpm_write(reg, i, msm_mpm_polarity[i]);

		reg = MSM_MPM_REG_STATUS;
		msm_mpm_write(reg, i, 0);
	}

	/*
	 * Ensure that the set operation is complete before sending the
	 * interrupt
	 */
	wmb();
	msm_mpm_send_interrupt();
}

static inline unsigned int msm_mpm_get_irq_m2a(unsigned int pin)
{
	return msm_mpm_irqs_m2a[pin];
}

static inline uint16_t msm_mpm_get_irq_a2m(struct irq_data *d)
{
	struct mpm_irqs_a2m *node = NULL;

	hlist_for_each_entry(node, &irq_hash[hashfn(d->hwirq)], node) {
		if ((node->hwirq == d->hwirq)
				&& (d->domain == node->domain)) {
			/*
			 * Update the linux irq mapping. No update required for
			 * bypass interrupts
			 */
			if (node->pin != 0xff)
				msm_mpm_irqs_m2a[node->pin] = d->irq;
			break;
		}
	}
	return node ? node->pin : 0;
}

static int msm_mpm_enable_irq_exclusive(
	struct irq_data *d, bool enable, bool wakeset)
{
	uint16_t mpm_pin;

	WARN_ON(!d);
	if (!d)
		return 0;

	mpm_pin = msm_mpm_get_irq_a2m(d);

	if (mpm_pin == 0xff)
		return 0;

	if (mpm_pin) {
		uint32_t *mpm_irq_masks = wakeset ?
				msm_mpm_wake_irq : msm_mpm_enabled_irq;
		uint32_t index = MSM_MPM_IRQ_INDEX(mpm_pin);
		uint32_t mask = MSM_MPM_IRQ_MASK(mpm_pin);

		if (enable)
			mpm_irq_masks[index] |= mask;
		else
			mpm_irq_masks[index] &= ~mask;
	} else {
		int i;
		unsigned long *irq_apps;

		for (i = 0; i < MSM_MPM_NR_IRQ_DOMAINS; i++) {
			if (d->domain == unlisted_irqs[i].domain)
				break;
		}

		if (i == MSM_MPM_NR_IRQ_DOMAINS)
			return 0;
		irq_apps = wakeset ? unlisted_irqs[i].wakeup_irqs :
					unlisted_irqs[i].enabled_irqs;

		if (enable)
			__set_bit(d->hwirq, irq_apps);
		else
			__clear_bit(d->hwirq, irq_apps);

		if (!wakeset && (msm_mpm_initialized & MSM_MPM_DEVICE_PROBED))
			complete(&wake_wq);
	}

	return 0;
}

static void msm_mpm_set_edge_ctl(int pin, unsigned int flow_type)
{
	uint32_t index;
	uint32_t mask;

	index = MSM_MPM_IRQ_INDEX(pin);
	mask = MSM_MPM_IRQ_MASK(pin);

	if (flow_type & IRQ_TYPE_EDGE_FALLING)
		msm_mpm_falling_edge[index] |= mask;
	else
		msm_mpm_falling_edge[index] &= ~mask;

	if (flow_type & IRQ_TYPE_EDGE_RISING)
		msm_mpm_rising_edge[index] |= mask;
	else
		msm_mpm_rising_edge[index] &= ~mask;

}

static int msm_mpm_set_irq_type_exclusive(
	struct irq_data *d, unsigned int flow_type)
{
	uint32_t mpm_irq;

	mpm_irq = msm_mpm_get_irq_a2m(d);

	if (mpm_irq == 0xff)
		return 0;

	if (mpm_irq) {
		uint32_t index = MSM_MPM_IRQ_INDEX(mpm_irq);
		uint32_t mask = MSM_MPM_IRQ_MASK(mpm_irq);

		if (index >= MSM_MPM_REG_WIDTH)
			return -EFAULT;

		msm_mpm_set_edge_ctl(mpm_irq, flow_type);

		if (flow_type &  IRQ_TYPE_LEVEL_HIGH)
			msm_mpm_polarity[index] |= mask;
		else
			msm_mpm_polarity[index] &= ~mask;
	}
	return 0;
}

static int __msm_mpm_enable_irq(struct irq_data *d, bool enable)
{
	unsigned long flags;
	int rc;

	if (!msm_mpm_is_initialized())
		return -EINVAL;

	spin_lock_irqsave(&msm_mpm_lock, flags);

	rc = msm_mpm_enable_irq_exclusive(d, enable, false);
	spin_unlock_irqrestore(&msm_mpm_lock, flags);

	return rc;
}

static void msm_mpm_enable_irq(struct irq_data *d)
{
	__msm_mpm_enable_irq(d, true);
}

static void msm_mpm_disable_irq(struct irq_data *d)
{
	__msm_mpm_enable_irq(d, false);
}

static int msm_mpm_set_irq_wake(struct irq_data *d, unsigned int on)
{
	unsigned long flags;
	int rc;

	if (!msm_mpm_is_initialized())
		return -EINVAL;

	spin_lock_irqsave(&msm_mpm_lock, flags);
	rc = msm_mpm_enable_irq_exclusive(d, (bool)on, true);
	spin_unlock_irqrestore(&msm_mpm_lock, flags);

	return rc;
}

static int msm_mpm_set_irq_type(struct irq_data *d, unsigned int flow_type)
{
	unsigned long flags;
	int rc;

	if (!msm_mpm_is_initialized())
		return -EINVAL;

	spin_lock_irqsave(&msm_mpm_lock, flags);
	rc = msm_mpm_set_irq_type_exclusive(d, flow_type);
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

	if (!msm_mpm_is_initialized())
		return -EINVAL;

	if (pin >= MSM_MPM_NR_MPM_IRQS)
		return -EINVAL;

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

	if (!msm_mpm_is_initialized())
		return -EINVAL;

	if (pin >= MSM_MPM_NR_MPM_IRQS)
		return -EINVAL;

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

	if (!msm_mpm_is_initialized())
		return -EINVAL;

	if (pin >= MSM_MPM_NR_MPM_IRQS)
		return -EINVAL;

	spin_lock_irqsave(&msm_mpm_lock, flags);

	msm_mpm_set_edge_ctl(pin, flow_type);

	if (flow_type & IRQ_TYPE_LEVEL_HIGH)
		msm_mpm_polarity[index] |= mask;
	else
		msm_mpm_polarity[index] &= ~mask;

	spin_unlock_irqrestore(&msm_mpm_lock, flags);
	return 0;
}

static bool msm_mpm_interrupts_detectable(int d, bool from_idle)
{
	unsigned long *irq_bitmap;
	bool debug_mask, ret = false;
	struct mpm_irqs *unlisted = &unlisted_irqs[d];

	if (!msm_mpm_is_initialized())
		return false;

	if (from_idle) {
		irq_bitmap = unlisted->enabled_irqs;
		debug_mask = msm_mpm_debug_mask &
				MSM_MPM_DEBUG_NON_DETECTABLE_IRQ_IDLE;
	} else {
		irq_bitmap = unlisted->wakeup_irqs;
		debug_mask = msm_mpm_debug_mask &
				MSM_MPM_DEBUG_NON_DETECTABLE_IRQ;
	}

	ret = (bool) __bitmap_empty(irq_bitmap, unlisted->size);

	if (debug_mask && !ret) {
		int i = 0;
		i = find_first_bit(irq_bitmap, unlisted->size);
		pr_info("%s(): %s preventing system sleep modes during %s\n",
				__func__, unlisted->domain_name,
				from_idle ? "idle" : "suspend");

		while (i < unlisted->size) {
			pr_info("\thwirq: %d\n", i);
			i = find_next_bit(irq_bitmap, unlisted->size, i + 1);
		}
	}

	return ret;
}

bool msm_mpm_gpio_irqs_detectable(bool from_idle)
{
	return msm_mpm_interrupts_detectable(MSM_MPM_GPIO_IRQ_DOMAIN,
			from_idle);
}
bool msm_mpm_irqs_detectable(bool from_idle)
{
	return msm_mpm_interrupts_detectable(MSM_MPM_GIC_IRQ_DOMAIN,
			from_idle);
}
void msm_mpm_enter_sleep(uint32_t sclk_count, bool from_idle)
{
	cycle_t wakeup = (u64)sclk_count * ARCH_TIMER_HZ;

	if (!msm_mpm_is_initialized()) {
		pr_err("%s(): MPM not initialized\n", __func__);
		return;
	}

	if (sclk_count) {
		do_div(wakeup, SCLK_HZ);
		wakeup += arch_counter_get_cntpct();
	} else {
		wakeup = (~0ULL);
	}

	msm_mpm_set(wakeup, !from_idle);
}

void msm_mpm_exit_sleep(bool from_idle)
{
	unsigned long pending;
	int i;
	int k;

	if (!msm_mpm_is_initialized()) {
		pr_err("%s(): MPM not initialized\n", __func__);
		return;
	}

	for (i = 0; i < MSM_MPM_REG_WIDTH; i++) {
		pending = msm_mpm_read(MSM_MPM_REG_STATUS, i);

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
				if (from_idle) {
					raw_spin_lock(&desc->lock);
					check_irq_resend(desc, apps_irq);
					raw_spin_unlock(&desc->lock);
				}
			}

			k = find_next_bit(&pending, 32, k + 1);
		}
	}
}
static void msm_mpm_sys_low_power_modes(bool allow)
{
	if (allow) {
		if (xo_enabled) {
			clk_disable_unprepare(xo_clk);
			xo_enabled = false;
		}
	} else {
		if (!xo_enabled) {
			/* If we cannot enable XO clock then we want to flag it,
			 * than having to deal with not being able to wakeup
			 * from a non-monitorable interrupt
			 */
			BUG_ON(clk_prepare_enable(xo_clk));
			xo_enabled = true;
		}
	}
}

void msm_mpm_suspend_prepare(void)
{
	bool allow = msm_mpm_irqs_detectable(false) &&
		msm_mpm_gpio_irqs_detectable(false);
	msm_mpm_sys_low_power_modes(allow);
}
EXPORT_SYMBOL(msm_mpm_suspend_prepare);

void msm_mpm_suspend_wake(void)
{
	bool allow = msm_mpm_irqs_detectable(true) &&
		msm_mpm_gpio_irqs_detectable(true);
	msm_mpm_sys_low_power_modes(allow);
}
EXPORT_SYMBOL(msm_mpm_suspend_wake);

static void msm_mpm_work_fn(struct work_struct *work)
{
	unsigned long flags;
	while (1) {
		bool allow;
		wait_for_completion(&wake_wq);
		spin_lock_irqsave(&msm_mpm_lock, flags);
		allow = msm_mpm_irqs_detectable(true) &&
				msm_mpm_gpio_irqs_detectable(true);
		spin_unlock_irqrestore(&msm_mpm_lock, flags);
		msm_mpm_sys_low_power_modes(allow);
	}
}

static int msm_mpm_dev_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	int offset, ret;
	struct msm_mpm_device_data *dev = &msm_mpm_dev_data;

	if (msm_mpm_initialized & MSM_MPM_DEVICE_PROBED) {
		pr_warn("MPM device probed multiple times\n");
		return 0;
	}

	xo_clk = devm_clk_get(&pdev->dev, "xo");

	if (IS_ERR(xo_clk)) {
		pr_err("%s(): Cannot get clk resource for XO\n", __func__);
		return PTR_ERR(xo_clk);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vmpm");
	if (!res) {
		pr_err("%s(): Missing RPM memory resource\n", __func__);
		return -EINVAL;
	}

	dev->mpm_request_reg_base = devm_request_and_ioremap(&pdev->dev, res);

	if (!dev->mpm_request_reg_base) {
		pr_err("%s(): Unable to iomap\n", __func__);
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ipc");
	if (!res) {
		pr_err("%s(): Missing GCC memory resource\n", __func__);
		return -EINVAL;
	}

	dev->mpm_apps_ipc_reg = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!dev->mpm_apps_ipc_reg) {
		pr_err("%s(): Unable to iomap IPC register\n", __func__);
		return -EADDRNOTAVAIL;
	}

	if (of_property_read_u32(pdev->dev.of_node,
				"qcom,ipc-bit-offset", &offset)) {
		pr_info("%s(): Cannot read ipc bit offset\n", __func__);
		return -EINVAL ;
	}

	dev->mpm_apps_ipc_val = (1 << offset);

	dev->mpm_ipc_irq = platform_get_irq(pdev, 0);

	if (dev->mpm_ipc_irq == -ENXIO) {
		pr_info("%s(): Cannot find IRQ resource\n", __func__);
		return -ENXIO;
	}
	ret = devm_request_irq(&pdev->dev, dev->mpm_ipc_irq, msm_mpm_irq,
			IRQF_TRIGGER_RISING, pdev->name, msm_mpm_irq);

	if (ret) {
		pr_info("%s(): request_irq failed errno: %d\n", __func__, ret);
		return ret;
	}
	ret = irq_set_irq_wake(dev->mpm_ipc_irq, 1);

	if (ret) {
		pr_err("%s: failed to set wakeup irq %u: %d\n",
			__func__, dev->mpm_ipc_irq, ret);
		return ret;

	}

	init_completion(&wake_wq);

	INIT_WORK(&msm_mpm_work, msm_mpm_work_fn);
	msm_mpm_wq = create_singlethread_workqueue("mpm");

	if (msm_mpm_wq)
		queue_work(msm_mpm_wq, &msm_mpm_work);
	else  {
		pr_warn("%s(): Failed to create wq. So voting against XO off",
				__func__);
		/* Throw a BUG. Otherwise, its possible that system allows
		 * XO shutdown when there are non-monitored interrupts are
		 * pending and cause errors at a later point in time.
		 */
		BUG_ON(clk_prepare_enable(xo_clk));
		xo_enabled = true;
	}

	msm_mpm_initialized |= MSM_MPM_DEVICE_PROBED;
	return 0;
}

static inline int __init mpm_irq_domain_linear_size(struct irq_domain *d)
{
	return d->revmap_data.linear.size;
}

static inline int __init mpm_irq_domain_legacy_size(struct irq_domain *d)
{
	return d->revmap_data.legacy.size;
}

void __init of_mpm_init(struct device_node *node)
{
	const __be32 *list;

	struct mpm_of {
		char *pkey;
		char *map;
		char name[MAX_DOMAIN_NAME];
		struct irq_chip *chip;
		int (*get_max_irqs)(struct irq_domain *d);
	};
	int i;

	struct mpm_of mpm_of_map[MSM_MPM_NR_IRQ_DOMAINS] = {
		{
			"qcom,gic-parent",
			"qcom,gic-map",
			"gic",
			&gic_arch_extn,
			mpm_irq_domain_linear_size,
		},
		{
			"qcom,gpio-parent",
			"qcom,gpio-map",
			"gpio",
	#ifdef CONFIG_USE_PINCTRL_IRQ
			&mpm_tlmm_irq_extn,
	#else
			&msm_gpio_irq_extn,
	#endif
			mpm_irq_domain_legacy_size,
		},
	};

	if (msm_mpm_initialized & MSM_MPM_IRQ_MAPPING_DONE) {
		pr_warn("%s(): MPM driver mapping exists\n", __func__);
		return;
	}

	for (i = 0; i < MSM_MPM_NR_MPM_IRQS; i++)
		INIT_HLIST_HEAD(&irq_hash[i]);

	for (i = 0; i < MSM_MPM_NR_IRQ_DOMAINS; i++) {
		struct device_node *parent = NULL;
		struct mpm_irqs_a2m *mpm_node = NULL;
		struct irq_domain *domain = NULL;
		int size;

		parent = of_parse_phandle(node, mpm_of_map[i].pkey, 0);

		if (!parent) {
			pr_warn("%s(): %s Not found\n", __func__,
					mpm_of_map[i].pkey);
			continue;
		}

		domain = irq_find_host(parent);

		if (!domain) {
			pr_warn("%s(): Cannot find irq controller for %s\n",
					__func__, mpm_of_map[i].pkey);
			continue;
		}

		size = mpm_of_map[i].get_max_irqs(domain);
		unlisted_irqs[i].size = size;
		memcpy(unlisted_irqs[i].domain_name, mpm_of_map[i].name,
				MAX_DOMAIN_NAME);

		unlisted_irqs[i].enabled_irqs =
			kzalloc(BITS_TO_LONGS(size) * sizeof(unsigned long),
					GFP_KERNEL);

		if (!unlisted_irqs[i].enabled_irqs)
			goto failed_malloc;

		unlisted_irqs[i].wakeup_irqs =
			kzalloc(BITS_TO_LONGS(size) * sizeof(unsigned long),
					GFP_KERNEL);

		if (!unlisted_irqs[i].wakeup_irqs)
			goto failed_malloc;

		unlisted_irqs[i].domain = domain;

		list = of_get_property(node, mpm_of_map[i].map, &size);

		if (!list || !size) {
			__WARN();
			continue;
		}

		/*
		 * Size is in bytes. Convert to size of uint32_t
		 */
		size /= sizeof(*list);

		/*
		 * The data is represented by a tuple mapping hwirq to a MPM
		 * pin. The number of mappings in the device tree would be
		 * size/2
		 */
		mpm_node = kzalloc(sizeof(struct mpm_irqs_a2m) * size / 2,
				GFP_KERNEL);
		if (!mpm_node)
			goto failed_malloc;

		while (size) {
			unsigned long pin = be32_to_cpup(list++);
			irq_hw_number_t hwirq = be32_to_cpup(list++);

			mpm_node->pin = pin;
			mpm_node->hwirq = hwirq;
			mpm_node->parent = parent;
			mpm_node->domain = domain;
			INIT_HLIST_NODE(&mpm_node->node);

			hlist_add_head(&mpm_node->node,
					&irq_hash[hashfn(mpm_node->hwirq)]);
			size -= 2;
			mpm_node++;
		}

		if (mpm_of_map[i].chip) {
			mpm_of_map[i].chip->irq_mask = msm_mpm_disable_irq;
			mpm_of_map[i].chip->irq_unmask = msm_mpm_enable_irq;
			mpm_of_map[i].chip->irq_disable = msm_mpm_disable_irq;
			mpm_of_map[i].chip->irq_set_type = msm_mpm_set_irq_type;
			mpm_of_map[i].chip->irq_set_wake = msm_mpm_set_irq_wake;
		}

	}
	msm_mpm_initialized |= MSM_MPM_IRQ_MAPPING_DONE;

	return;

failed_malloc:
	for (i = 0; i < MSM_MPM_NR_IRQ_DOMAINS; i++) {
		mpm_of_map[i].chip->irq_mask = NULL;
		mpm_of_map[i].chip->irq_unmask = NULL;
		mpm_of_map[i].chip->irq_disable = NULL;
		mpm_of_map[i].chip->irq_set_type = NULL;
		mpm_of_map[i].chip->irq_set_wake = NULL;

		kfree(unlisted_irqs[i].enabled_irqs);
		kfree(unlisted_irqs[i].wakeup_irqs);

	}
}

static struct of_device_id msm_mpm_match_table[] = {
	{.compatible = "qcom,mpm-v2"},
	{},
};

static struct platform_driver msm_mpm_dev_driver = {
	.probe = msm_mpm_dev_probe,
	.driver = {
		.name = "mpm-v2",
		.owner = THIS_MODULE,
		.of_match_table = msm_mpm_match_table,
	},
};

int __init msm_mpm_device_init(void)
{
	return platform_driver_register(&msm_mpm_dev_driver);
}
arch_initcall(msm_mpm_device_init);
