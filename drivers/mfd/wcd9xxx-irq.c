/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd9xxx/wcd9310_registers.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>
#include <linux/delay.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <mach/cpuidle.h>

#define BYTE_BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_BYTE))
#define BIT_BYTE(nr)			((nr) / BITS_PER_BYTE)

#define WCD9XXX_SYSTEM_RESUME_TIMEOUT_MS 100

#ifdef CONFIG_OF
struct wcd9xxx_irq_drv_data {
	struct irq_domain *domain;
	int irq;
};
#endif

static int virq_to_phyirq(struct wcd9xxx *wcd9xxx, int virq);
static int phyirq_to_virq(struct wcd9xxx *wcd9xxx, int irq);
static unsigned int wcd9xxx_irq_get_upstream_irq(struct wcd9xxx *wcd9xxx);
static void wcd9xxx_irq_put_upstream_irq(struct wcd9xxx *wcd9xxx);
static int wcd9xxx_map_irq(struct wcd9xxx *wcd9xxx, int irq);

static void wcd9xxx_irq_lock(struct irq_data *data)
{
	struct wcd9xxx *wcd9xxx = irq_data_get_irq_chip_data(data);
	mutex_lock(&wcd9xxx->irq_lock);
}

static void wcd9xxx_irq_sync_unlock(struct irq_data *data)
{
	struct wcd9xxx *wcd9xxx = irq_data_get_irq_chip_data(data);
	int i;

	if (ARRAY_SIZE(wcd9xxx->irq_masks_cur) > WCD9XXX_NUM_IRQ_REGS ||
		ARRAY_SIZE(wcd9xxx->irq_masks_cache) > WCD9XXX_NUM_IRQ_REGS) {
			pr_err("%s: Array Size out of bound\n", __func__);
			 return;
	}

	for (i = 0; i < ARRAY_SIZE(wcd9xxx->irq_masks_cur); i++) {
		/* If there's been a change in the mask write it back
		 * to the hardware.
		 */
		if (wcd9xxx->irq_masks_cur[i] != wcd9xxx->irq_masks_cache[i]) {
			wcd9xxx->irq_masks_cache[i] = wcd9xxx->irq_masks_cur[i];
			wcd9xxx_reg_write(wcd9xxx,
					  WCD9XXX_A_INTR_MASK0 + i,
					  wcd9xxx->irq_masks_cur[i]);
		}
	}

	mutex_unlock(&wcd9xxx->irq_lock);
}

static void wcd9xxx_irq_enable(struct irq_data *data)
{
	struct wcd9xxx *wcd9xxx = irq_data_get_irq_chip_data(data);
	int wcd9xxx_irq = virq_to_phyirq(wcd9xxx, data->irq);
	wcd9xxx->irq_masks_cur[BIT_BYTE(wcd9xxx_irq)] &=
		~(BYTE_BIT_MASK(wcd9xxx_irq));
}

static void wcd9xxx_irq_disable(struct irq_data *data)
{
	struct wcd9xxx *wcd9xxx = irq_data_get_irq_chip_data(data);
	int wcd9xxx_irq = virq_to_phyirq(wcd9xxx, data->irq);
	wcd9xxx->irq_masks_cur[BIT_BYTE(wcd9xxx_irq)]
		|= BYTE_BIT_MASK(wcd9xxx_irq);
}

static void wcd9xxx_irq_mask(struct irq_data *d)
{
	/* do nothing but required as linux calls irq_mask without NULL check */
}

static struct irq_chip wcd9xxx_irq_chip = {
	.name = "wcd9xxx",
	.irq_bus_lock = wcd9xxx_irq_lock,
	.irq_bus_sync_unlock = wcd9xxx_irq_sync_unlock,
	.irq_disable = wcd9xxx_irq_disable,
	.irq_enable = wcd9xxx_irq_enable,
	.irq_mask = wcd9xxx_irq_mask,
};

enum wcd9xxx_pm_state wcd9xxx_pm_cmpxchg(struct wcd9xxx *wcd9xxx,
		enum wcd9xxx_pm_state o,
		enum wcd9xxx_pm_state n)
{
	enum wcd9xxx_pm_state old;
	mutex_lock(&wcd9xxx->pm_lock);
	old = wcd9xxx->pm_state;
	if (old == o)
		wcd9xxx->pm_state = n;
	mutex_unlock(&wcd9xxx->pm_lock);
	return old;
}
EXPORT_SYMBOL_GPL(wcd9xxx_pm_cmpxchg);

bool wcd9xxx_lock_sleep(struct wcd9xxx *wcd9xxx)
{
	enum wcd9xxx_pm_state os;

	/*
	 * wcd9xxx_{lock/unlock}_sleep will be called by wcd9xxx_irq_thread
	 * and its subroutines only motly.
	 * but btn0_lpress_fn is not wcd9xxx_irq_thread's subroutine and
	 * It can race with wcd9xxx_irq_thread.
	 * So need to embrace wlock_holders with mutex.
	 *
	 * If system didn't resume, we can simply return false so codec driver's
	 * IRQ handler can return without handling IRQ.
	 * As interrupt line is still active, codec will have another IRQ to
	 * retry shortly.
	 */
	mutex_lock(&wcd9xxx->pm_lock);
	if (wcd9xxx->wlock_holders++ == 0) {
		pr_debug("%s: holding wake lock\n", __func__);
		pm_qos_update_request(&wcd9xxx->pm_qos_req,
				      msm_cpuidle_get_deep_idle_latency());
	}
	mutex_unlock(&wcd9xxx->pm_lock);
	if (!wait_event_timeout(wcd9xxx->pm_wq,
			((os = wcd9xxx_pm_cmpxchg(wcd9xxx, WCD9XXX_PM_SLEEPABLE,
						WCD9XXX_PM_AWAKE)) ==
						    WCD9XXX_PM_SLEEPABLE ||
			 (os == WCD9XXX_PM_AWAKE)),
			msecs_to_jiffies(WCD9XXX_SYSTEM_RESUME_TIMEOUT_MS))) {
		pr_warn("%s: system didn't resume within %dms, s %d, w %d\n",
			__func__,
			WCD9XXX_SYSTEM_RESUME_TIMEOUT_MS, wcd9xxx->pm_state,
			wcd9xxx->wlock_holders);
		wcd9xxx_unlock_sleep(wcd9xxx);
		return false;
	}
	wake_up_all(&wcd9xxx->pm_wq);
	return true;
}
EXPORT_SYMBOL_GPL(wcd9xxx_lock_sleep);

void wcd9xxx_unlock_sleep(struct wcd9xxx *wcd9xxx)
{
	mutex_lock(&wcd9xxx->pm_lock);
	if (--wcd9xxx->wlock_holders == 0) {
		pr_debug("%s: releasing wake lock pm_state %d -> %d\n",
			 __func__, wcd9xxx->pm_state, WCD9XXX_PM_SLEEPABLE);
		/*
		 * if wcd9xxx_lock_sleep failed, pm_state would be still
		 * WCD9XXX_PM_ASLEEP, don't overwrite
		 */
		if (likely(wcd9xxx->pm_state == WCD9XXX_PM_AWAKE))
			wcd9xxx->pm_state = WCD9XXX_PM_SLEEPABLE;
		pm_qos_update_request(&wcd9xxx->pm_qos_req,
				PM_QOS_DEFAULT_VALUE);
	}
	mutex_unlock(&wcd9xxx->pm_lock);
	wake_up_all(&wcd9xxx->pm_wq);
}
EXPORT_SYMBOL_GPL(wcd9xxx_unlock_sleep);

void wcd9xxx_nested_irq_lock(struct wcd9xxx *wcd9xxx)
{
	mutex_lock(&wcd9xxx->nested_irq_lock);
}

void wcd9xxx_nested_irq_unlock(struct wcd9xxx *wcd9xxx)
{
	mutex_unlock(&wcd9xxx->nested_irq_lock);
}

static bool wcd9xxx_is_mbhc_irq(struct wcd9xxx *wcd9xxx, int irqbit)
{
	if ((irqbit <= WCD9XXX_IRQ_MBHC_INSERTION) &&
	    (irqbit >= WCD9XXX_IRQ_MBHC_REMOVAL))
		return true;
	else if (wcd9xxx->codec_type->id_major == TAIKO_MAJOR &&
		 irqbit == WCD9320_IRQ_MBHC_JACK_SWITCH)
		return true;
	else if (wcd9xxx->codec_type->id_major == TAPAN_MAJOR &&
		 irqbit == WCD9306_IRQ_MBHC_JACK_SWITCH)
		return true;
	else
		return false;
}

static void wcd9xxx_irq_dispatch(struct wcd9xxx *wcd9xxx, int irqbit)
{
	if (wcd9xxx_is_mbhc_irq(wcd9xxx, irqbit)) {
		wcd9xxx_nested_irq_lock(wcd9xxx);
		wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_INTR_CLEAR0 +
					   BIT_BYTE(irqbit),
				  BYTE_BIT_MASK(irqbit));
		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_INTR_MODE, 0x02);
		handle_nested_irq(phyirq_to_virq(wcd9xxx, irqbit));
		wcd9xxx_nested_irq_unlock(wcd9xxx);
	} else {
		wcd9xxx_nested_irq_lock(wcd9xxx);
		handle_nested_irq(phyirq_to_virq(wcd9xxx, irqbit));
		wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_INTR_CLEAR0 +
					   BIT_BYTE(irqbit),
				  BYTE_BIT_MASK(irqbit));
		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_INTR_MODE, 0x02);
		wcd9xxx_nested_irq_unlock(wcd9xxx);
	}
}

static int wcd9xxx_num_irq_regs(const struct wcd9xxx *wcd9xxx)
{
	return (wcd9xxx->codec_type->num_irqs / 8) +
		((wcd9xxx->codec_type->num_irqs % 8) ? 1 : 0);
}

static irqreturn_t wcd9xxx_irq_thread(int irq, void *data)
{
	int ret;
	int i;
	char linebuf[128];
	struct wcd9xxx *wcd9xxx = data;
	int num_irq_regs = wcd9xxx_num_irq_regs(wcd9xxx);
	u8 status[num_irq_regs], status1[num_irq_regs];
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ, 1);

	if (unlikely(wcd9xxx_lock_sleep(wcd9xxx) == false)) {
		dev_err(wcd9xxx->dev, "Failed to hold suspend\n");
		return IRQ_NONE;
	}
	ret = wcd9xxx_bulk_read(wcd9xxx, WCD9XXX_A_INTR_STATUS0,
				num_irq_regs, status);
	if (ret < 0) {
		dev_err(wcd9xxx->dev, "Failed to read interrupt status: %d\n",
			ret);
		dev_err(wcd9xxx->dev, "Disable irq %d\n", wcd9xxx->irq);
		disable_irq_wake(wcd9xxx->irq);
		disable_irq_nosync(wcd9xxx->irq);
		wcd9xxx_unlock_sleep(wcd9xxx);
		return IRQ_NONE;
	}

	/* Apply masking */
	for (i = 0; i < num_irq_regs; i++)
		status[i] &= ~wcd9xxx->irq_masks_cur[i];

	memcpy(status1, status, sizeof(status1));

	/* Find out which interrupt was triggered and call that interrupt's
	 * handler function
	 */
	if (status[BIT_BYTE(WCD9XXX_IRQ_SLIMBUS)] &
	    BYTE_BIT_MASK(WCD9XXX_IRQ_SLIMBUS)) {
		wcd9xxx_irq_dispatch(wcd9xxx, WCD9XXX_IRQ_SLIMBUS);
		status1[BIT_BYTE(WCD9XXX_IRQ_SLIMBUS)] &=
		    ~BYTE_BIT_MASK(WCD9XXX_IRQ_SLIMBUS);
	}

	/* Since codec has only one hardware irq line which is shared by
	 * codec's different internal interrupts, so it's possible master irq
	 * handler dispatches multiple nested irq handlers after breaking
	 * order.  Dispatch MBHC interrupts order to follow MBHC state
	 * machine's order */
	for (i = WCD9XXX_IRQ_MBHC_INSERTION;
	     i >= WCD9XXX_IRQ_MBHC_REMOVAL; i--) {
		if (status[BIT_BYTE(i)] & BYTE_BIT_MASK(i)) {
			wcd9xxx_irq_dispatch(wcd9xxx, i);
			status1[BIT_BYTE(i)] &= ~BYTE_BIT_MASK(i);
		}
	}
	for (i = WCD9XXX_IRQ_BG_PRECHARGE; i < wcd9xxx->codec_type->num_irqs;
	     i++) {
		if (status[BIT_BYTE(i)] & BYTE_BIT_MASK(i)) {
			wcd9xxx_irq_dispatch(wcd9xxx, i);
			status1[BIT_BYTE(i)] &= ~BYTE_BIT_MASK(i);
		}
	}

	/*
	 * As a failsafe if unhandled irq is found, clear it to prevent
	 * interrupt storm.
	 * Note that we can say there was an unhandled irq only when no irq
	 * handled by nested irq handler since Taiko supports qdsp as irqs'
	 * destination for few irqs.  Therefore driver shouldn't clear pending
	 * irqs when few handled while few others not.
	 */
	if (unlikely(!memcmp(status, status1, sizeof(status)))) {
		if (__ratelimit(&ratelimit)) {
			pr_warn("%s: Unhandled irq found\n", __func__);
			hex_dump_to_buffer(status, sizeof(status), 16, 1,
					   linebuf, sizeof(linebuf), false);
			pr_warn("%s: status0 : %s\n", __func__, linebuf);
			hex_dump_to_buffer(status1, sizeof(status1), 16, 1,
					   linebuf, sizeof(linebuf), false);
			pr_warn("%s: status1 : %s\n", __func__, linebuf);
		}

		memset(status, 0xff, num_irq_regs);
		wcd9xxx_bulk_write(wcd9xxx, WCD9XXX_A_INTR_CLEAR0,
				   num_irq_regs, status);
		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_INTR_MODE, 0x02);
	}
	wcd9xxx_unlock_sleep(wcd9xxx);

	return IRQ_HANDLED;
}

void wcd9xxx_free_irq(struct wcd9xxx *wcd9xxx, int irq, void *data)
{
	free_irq(phyirq_to_virq(wcd9xxx, irq), data);
}

void wcd9xxx_enable_irq(struct wcd9xxx *wcd9xxx, int irq)
{
	enable_irq(phyirq_to_virq(wcd9xxx, irq));
}

void wcd9xxx_disable_irq(struct wcd9xxx *wcd9xxx, int irq)
{
	disable_irq_nosync(phyirq_to_virq(wcd9xxx, irq));
}

void wcd9xxx_disable_irq_sync(struct wcd9xxx *wcd9xxx, int irq)
{
	disable_irq(phyirq_to_virq(wcd9xxx, irq));
}

static int wcd9xxx_irq_setup_downstream_irq(struct wcd9xxx *wcd9xxx)
{
	int irq, virq, ret;

	pr_debug("%s: enter\n", __func__);

	for (irq = 0; irq < wcd9xxx->codec_type->num_irqs; irq++) {
		/* Map OF irq */
		virq = wcd9xxx_map_irq(wcd9xxx, irq);
		pr_debug("%s: irq %d -> %d\n", __func__, irq, virq);
		if (virq == NO_IRQ) {
			pr_err("%s, No interrupt specifier for irq %d\n",
			       __func__, irq);
			return NO_IRQ;
		}

		ret = irq_set_chip_data(virq, wcd9xxx);
		if (ret) {
			pr_err("%s: Failed to configure irq %d (%d)\n",
			       __func__, irq, ret);
			return ret;
		}

		if (wcd9xxx->irq_level_high[irq])
			irq_set_chip_and_handler(virq, &wcd9xxx_irq_chip,
						 handle_level_irq);
		else
			irq_set_chip_and_handler(virq, &wcd9xxx_irq_chip,
						 handle_edge_irq);

		irq_set_nested_thread(virq, 1);
	}

	pr_debug("%s: leave\n", __func__);

	return 0;
}

int wcd9xxx_irq_init(struct wcd9xxx *wcd9xxx)
{
	int i, ret;
	u8 irq_level[wcd9xxx_num_irq_regs(wcd9xxx)];

	mutex_init(&wcd9xxx->irq_lock);
	mutex_init(&wcd9xxx->nested_irq_lock);

	wcd9xxx->irq = wcd9xxx_irq_get_upstream_irq(wcd9xxx);
	if (!wcd9xxx->irq) {
		pr_warn("%s: irq driver is not yet initialized\n", __func__);
		mutex_destroy(&wcd9xxx->irq_lock);
		mutex_destroy(&wcd9xxx->nested_irq_lock);
		return -EPROBE_DEFER;
	}
	pr_debug("%s: probed irq %d\n", __func__, wcd9xxx->irq);

	/* Setup downstream IRQs */
	ret = wcd9xxx_irq_setup_downstream_irq(wcd9xxx);
	if (ret) {
		pr_err("%s: Failed to setup downstream IRQ\n", __func__);
		wcd9xxx_irq_put_upstream_irq(wcd9xxx);
		mutex_destroy(&wcd9xxx->irq_lock);
		mutex_destroy(&wcd9xxx->nested_irq_lock);
		return ret;
	}

	/* All other wcd9xxx interrupts are edge triggered */
	wcd9xxx->irq_level_high[0] = true;

	/* mask all the interrupts */
	memset(irq_level, 0, wcd9xxx_num_irq_regs(wcd9xxx));
	for (i = 0; i < wcd9xxx->codec_type->num_irqs; i++) {
		wcd9xxx->irq_masks_cur[BIT_BYTE(i)] |= BYTE_BIT_MASK(i);
		wcd9xxx->irq_masks_cache[BIT_BYTE(i)] |= BYTE_BIT_MASK(i);
		irq_level[BIT_BYTE(i)] |=
		    wcd9xxx->irq_level_high[i] << (i % BITS_PER_BYTE);
	}

	for (i = 0; i < wcd9xxx_num_irq_regs(wcd9xxx); i++) {
		/* Initialize interrupt mask and level registers */
		wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_INTR_LEVEL0 + i,
				  irq_level[i]);
		wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_INTR_MASK0 + i,
				  wcd9xxx->irq_masks_cur[i]);
	}

	ret = request_threaded_irq(wcd9xxx->irq, NULL, wcd9xxx_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "wcd9xxx", wcd9xxx);
	if (ret != 0)
		dev_err(wcd9xxx->dev, "Failed to request IRQ %d: %d\n",
			wcd9xxx->irq, ret);
	else {
		ret = enable_irq_wake(wcd9xxx->irq);
		if (ret)
			dev_err(wcd9xxx->dev, "Failed to set wake interrupt on"
				" IRQ %d: %d\n", wcd9xxx->irq, ret);
		if (ret)
			free_irq(wcd9xxx->irq, wcd9xxx);
	}

	if (ret) {
		pr_err("%s: Failed to init wcd9xxx irq\n", __func__);
		wcd9xxx_irq_put_upstream_irq(wcd9xxx);
		mutex_destroy(&wcd9xxx->irq_lock);
		mutex_destroy(&wcd9xxx->nested_irq_lock);
	}

	return ret;
}

int wcd9xxx_request_irq(struct wcd9xxx *wcd9xxx, int irq, irq_handler_t handler,
			const char *name, void *data)
{
	int virq;

	virq = phyirq_to_virq(wcd9xxx, irq);

	/*
	 * ARM needs us to explicitly flag the IRQ as valid
	 * and will set them noprobe when we do so.
	 */
#ifdef CONFIG_ARM
	set_irq_flags(virq, IRQF_VALID);
#else
	set_irq_noprobe(virq);
#endif

	return request_threaded_irq(virq, NULL, handler, IRQF_TRIGGER_RISING,
				    name, data);
}

void wcd9xxx_irq_exit(struct wcd9xxx *wcd9xxx)
{
	dev_dbg(wcd9xxx->dev, "%s: Cleaning up irq %d\n", __func__,
		wcd9xxx->irq);
	if (wcd9xxx->irq) {
		disable_irq_wake(wcd9xxx->irq);
		free_irq(wcd9xxx->irq, wcd9xxx);
		/* Release parent's of node */
		wcd9xxx_irq_put_upstream_irq(wcd9xxx);
	}
	mutex_destroy(&wcd9xxx->irq_lock);
	mutex_destroy(&wcd9xxx->nested_irq_lock);
}

#ifndef CONFIG_OF
static int phyirq_to_virq(struct wcd9xxx *wcd9xxx, int offset)
{
	return wcd9xxx->irq_base + offset;
}

static int virq_to_phyirq(struct wcd9xxx *wcd9xxx, int virq)
{
	return virq - wcd9xxx->irq_base;
}

static unsigned int wcd9xxx_irq_get_upstream_irq(struct wcd9xxx *wcd9xxx)
{
	return wcd9xxx->irq;
}

static void wcd9xxx_irq_put_upstream_irq(struct wcd9xxx *wcd9xxx)
{
	/* Do nothing */
}

static int wcd9xxx_map_irq(struct wcd9xxx *wcd9xxx, int irq)
{
	return phyirq_to_virq(wcd9xxx, irq);
}
#else
int __init wcd9xxx_irq_of_init(struct device_node *node,
			       struct device_node *parent)
{
	struct wcd9xxx_irq_drv_data *data;

	pr_debug("%s: node %s, node parent %s\n", __func__,
		 node->name, node->parent->name);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/*
	 * wcd9xxx_intc interrupt controller supports N to N irq mapping with
	 * single cell binding with irq numbers(offsets) only.
	 * Use irq_domain_simple_ops that has irq_domain_simple_map and
	 * irq_domain_xlate_onetwocell.
	 */
	data->domain = irq_domain_add_linear(node, WCD9XXX_MAX_NUM_IRQS,
					     &irq_domain_simple_ops, data);
	if (!data->domain) {
		kfree(data);
		return -ENOMEM;
	}

	return 0;
}

static struct wcd9xxx_irq_drv_data *
wcd9xxx_get_irq_drv_d(const struct wcd9xxx *wcd9xxx)
{
	struct device_node *pnode;
	struct irq_domain *domain;

	pnode = of_irq_find_parent(wcd9xxx->dev->of_node);
	/* Shouldn't happen */
	if (unlikely(!pnode))
		return NULL;

	domain = irq_find_host(pnode);
	return (struct wcd9xxx_irq_drv_data *)domain->host_data;
}

static int phyirq_to_virq(struct wcd9xxx *wcd9xxx, int offset)
{
	struct wcd9xxx_irq_drv_data *data;

	data = wcd9xxx_get_irq_drv_d(wcd9xxx);
	if (!data) {
		pr_warn("%s: not registered to interrupt controller\n",
			__func__);
		return -EINVAL;
	}
	return irq_linear_revmap(data->domain, offset);
}

static int virq_to_phyirq(struct wcd9xxx *wcd9xxx, int virq)
{
	struct irq_data *irq_data = irq_get_irq_data(virq);
	return irq_data->hwirq;
}

static unsigned int wcd9xxx_irq_get_upstream_irq(struct wcd9xxx *wcd9xxx)
{
	struct wcd9xxx_irq_drv_data *data;

	/* Hold parent's of node */
	if (!of_node_get(of_irq_find_parent(wcd9xxx->dev->of_node)))
		return -EINVAL;

	data = wcd9xxx_get_irq_drv_d(wcd9xxx);
	if (!data) {
		pr_err("%s: interrupt controller is not registerd\n", __func__);
		return 0;
	}

	rmb();
	return data->irq;
}

static void wcd9xxx_irq_put_upstream_irq(struct wcd9xxx *wcd9xxx)
{
	/* Hold parent's of node */
	of_node_put(of_irq_find_parent(wcd9xxx->dev->of_node));
}

static int wcd9xxx_map_irq(struct wcd9xxx *wcd9xxx, int irq)
{
	return of_irq_to_resource(wcd9xxx->dev->of_node, irq, NULL);
}

static int __devinit wcd9xxx_irq_probe(struct platform_device *pdev)
{
	int irq;
	struct irq_domain *domain;
	struct wcd9xxx_irq_drv_data *data;
	int ret = -EINVAL;

	irq = platform_get_irq_byname(pdev, "cdc-int");
	if (irq < 0) {
		dev_err(&pdev->dev, "%s: Couldn't find cdc-int node(%d)\n",
			__func__, irq);
		return -EINVAL;
	} else {
		dev_dbg(&pdev->dev, "%s: virq = %d\n", __func__, irq);
		domain = irq_find_host(pdev->dev.of_node);
		data = (struct wcd9xxx_irq_drv_data *)domain->host_data;
		data->irq = irq;
		wmb();
		ret = 0;
	}

	return ret;
}

static int wcd9xxx_irq_remove(struct platform_device *pdev)
{
	struct irq_domain *domain;
	struct wcd9xxx_irq_drv_data *data;

	domain = irq_find_host(pdev->dev.of_node);
	data = (struct wcd9xxx_irq_drv_data *)domain->host_data;
	data->irq = 0;
	wmb();

	return 0;
}

static const struct of_device_id of_match[] = {
	{ .compatible = "qcom,wcd9xxx-irq" },
	{ }
};

static struct platform_driver wcd9xxx_irq_driver = {
	.probe = wcd9xxx_irq_probe,
	.remove = wcd9xxx_irq_remove,
	.driver = {
		.name = "wcd9xxx_intc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_match),
	},
};

static int wcd9xxx_irq_drv_init(void)
{
	return platform_driver_register(&wcd9xxx_irq_driver);
}
subsys_initcall(wcd9xxx_irq_drv_init);

static void wcd9xxx_irq_drv_exit(void)
{
	platform_driver_unregister(&wcd9xxx_irq_driver);
}
module_exit(wcd9xxx_irq_drv_exit);
#endif /* CONFIG_OF */
