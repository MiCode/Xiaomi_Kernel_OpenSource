/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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
#include <linux/regmap.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-irq.h>
#include <linux/delay.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <soc/qcom/pm.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define BYTE_BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_BYTE))
#define BIT_BYTE(nr)			((nr) / BITS_PER_BYTE)

#define WCD9XXX_SYSTEM_RESUME_TIMEOUT_MS 100

#ifndef NO_IRQ
#define NO_IRQ	(-1)
#endif

#ifdef CONFIG_OF
struct wcd9xxx_irq_drv_data {
	struct irq_domain *domain;
	int irq;
};
#endif

static int virq_to_phyirq(
	struct wcd9xxx_core_resource *wcd9xxx_res, int virq);
static int phyirq_to_virq(
	struct wcd9xxx_core_resource *wcd9xxx_res, int irq);
static unsigned int wcd9xxx_irq_get_upstream_irq(
	struct wcd9xxx_core_resource *wcd9xxx_res);
static void wcd9xxx_irq_put_upstream_irq(
	struct wcd9xxx_core_resource *wcd9xxx_res);
static int wcd9xxx_map_irq(
	struct wcd9xxx_core_resource *wcd9xxx_res, int irq);

static void wcd9xxx_irq_lock(struct irq_data *data)
{
	struct wcd9xxx_core_resource *wcd9xxx_res =
			irq_data_get_irq_chip_data(data);
	mutex_lock(&wcd9xxx_res->irq_lock);
}

static void wcd9xxx_irq_sync_unlock(struct irq_data *data)
{
	struct wcd9xxx_core_resource *wcd9xxx_res =
			irq_data_get_irq_chip_data(data);
	int i;

	if ((ARRAY_SIZE(wcd9xxx_res->irq_masks_cur) >
			WCD9XXX_MAX_IRQ_REGS) ||
		(ARRAY_SIZE(wcd9xxx_res->irq_masks_cache) >
			WCD9XXX_MAX_IRQ_REGS)) {
			pr_err("%s: Array Size out of bound\n", __func__);
			 return;
	}
	if (!wcd9xxx_res->wcd_core_regmap) {
		pr_err("%s: Codec core regmap not defined\n",
			__func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(wcd9xxx_res->irq_masks_cur); i++) {
		/* If there's been a change in the mask write it back
		 * to the hardware.
		 */
		if (wcd9xxx_res->irq_masks_cur[i] !=
					wcd9xxx_res->irq_masks_cache[i]) {

			wcd9xxx_res->irq_masks_cache[i] =
					wcd9xxx_res->irq_masks_cur[i];
			regmap_write(wcd9xxx_res->wcd_core_regmap,
			wcd9xxx_res->intr_reg[WCD9XXX_INTR_MASK_BASE] + i,
			wcd9xxx_res->irq_masks_cur[i]);
		}
	}

	mutex_unlock(&wcd9xxx_res->irq_lock);
}

static void wcd9xxx_irq_enable(struct irq_data *data)
{
	struct wcd9xxx_core_resource *wcd9xxx_res =
			irq_data_get_irq_chip_data(data);
	int wcd9xxx_irq = virq_to_phyirq(wcd9xxx_res, data->irq);
	int byte = BIT_BYTE(wcd9xxx_irq);
	int size = ARRAY_SIZE(wcd9xxx_res->irq_masks_cur);
	if ((byte < size) && (byte >= 0)) {
		wcd9xxx_res->irq_masks_cur[byte] &=
			~(BYTE_BIT_MASK(wcd9xxx_irq));
	} else {
		pr_err("%s: Array size is %d but index is %d: Out of range\n",
			__func__, size, byte);
	}
}

static void wcd9xxx_irq_disable(struct irq_data *data)
{
	struct wcd9xxx_core_resource *wcd9xxx_res =
			irq_data_get_irq_chip_data(data);
	int wcd9xxx_irq = virq_to_phyirq(wcd9xxx_res, data->irq);
	int byte = BIT_BYTE(wcd9xxx_irq);
	int size = ARRAY_SIZE(wcd9xxx_res->irq_masks_cur);
	if ((byte < size) && (byte >= 0)) {
		wcd9xxx_res->irq_masks_cur[byte]
			|= BYTE_BIT_MASK(wcd9xxx_irq);
	} else {
		pr_err("%s: Array size is %d but index is %d: Out of range\n",
			__func__, size, byte);
	}
}

static void wcd9xxx_irq_ack(struct irq_data *data)
{
	int wcd9xxx_irq = 0;
	struct wcd9xxx_core_resource *wcd9xxx_res =
			irq_data_get_irq_chip_data(data);

	if (wcd9xxx_res == NULL) {
		pr_err("%s: wcd9xxx_res is NULL\n", __func__);
		return;
	}
	wcd9xxx_irq = virq_to_phyirq(wcd9xxx_res, data->irq);
	pr_debug("%s: IRQ_ACK called for WCD9XXX IRQ: %d\n",
				__func__, wcd9xxx_irq);
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
	.irq_ack = wcd9xxx_irq_ack,
};

bool wcd9xxx_lock_sleep(
	struct wcd9xxx_core_resource *wcd9xxx_res)
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
	mutex_lock(&wcd9xxx_res->pm_lock);
	if (wcd9xxx_res->wlock_holders++ == 0) {
		pr_debug("%s: holding wake lock\n", __func__);
		pm_qos_update_request(&wcd9xxx_res->pm_qos_req,
				      msm_cpuidle_get_deep_idle_latency());
		pm_stay_awake(wcd9xxx_res->dev);
	}
	mutex_unlock(&wcd9xxx_res->pm_lock);

	if (!wait_event_timeout(wcd9xxx_res->pm_wq,
				((os =  wcd9xxx_pm_cmpxchg(wcd9xxx_res,
						  WCD9XXX_PM_SLEEPABLE,
						  WCD9XXX_PM_AWAKE)) ==
							WCD9XXX_PM_SLEEPABLE ||
					(os == WCD9XXX_PM_AWAKE)),
				msecs_to_jiffies(
					WCD9XXX_SYSTEM_RESUME_TIMEOUT_MS))) {
		pr_warn("%s: system didn't resume within %dms, s %d, w %d\n",
			__func__,
			WCD9XXX_SYSTEM_RESUME_TIMEOUT_MS, wcd9xxx_res->pm_state,
			wcd9xxx_res->wlock_holders);
		wcd9xxx_unlock_sleep(wcd9xxx_res);
		return false;
	}
	wake_up_all(&wcd9xxx_res->pm_wq);
	return true;
}
EXPORT_SYMBOL(wcd9xxx_lock_sleep);

void wcd9xxx_unlock_sleep(
	struct wcd9xxx_core_resource *wcd9xxx_res)
{
	mutex_lock(&wcd9xxx_res->pm_lock);
	if (--wcd9xxx_res->wlock_holders == 0) {
		pr_debug("%s: releasing wake lock pm_state %d -> %d\n",
			 __func__, wcd9xxx_res->pm_state, WCD9XXX_PM_SLEEPABLE);
		/*
		 * if wcd9xxx_lock_sleep failed, pm_state would be still
		 * WCD9XXX_PM_ASLEEP, don't overwrite
		 */
		if (likely(wcd9xxx_res->pm_state == WCD9XXX_PM_AWAKE))
			wcd9xxx_res->pm_state = WCD9XXX_PM_SLEEPABLE;
		pm_qos_update_request(&wcd9xxx_res->pm_qos_req,
				PM_QOS_DEFAULT_VALUE);
		pm_relax(wcd9xxx_res->dev);
	}
	mutex_unlock(&wcd9xxx_res->pm_lock);
	wake_up_all(&wcd9xxx_res->pm_wq);
}
EXPORT_SYMBOL(wcd9xxx_unlock_sleep);

void wcd9xxx_nested_irq_lock(struct wcd9xxx_core_resource *wcd9xxx_res)
{
	mutex_lock(&wcd9xxx_res->nested_irq_lock);
}

void wcd9xxx_nested_irq_unlock(struct wcd9xxx_core_resource *wcd9xxx_res)
{
	mutex_unlock(&wcd9xxx_res->nested_irq_lock);
}


static void wcd9xxx_irq_dispatch(struct wcd9xxx_core_resource *wcd9xxx_res,
			struct intr_data *irqdata)
{
	int irqbit = irqdata->intr_num;
	if (!wcd9xxx_res->wcd_core_regmap) {
		pr_err("%s: codec core regmap not defined\n",
			__func__);
		return;
	}

	if (irqdata->clear_first) {
		wcd9xxx_nested_irq_lock(wcd9xxx_res);
		regmap_write(wcd9xxx_res->wcd_core_regmap,
			wcd9xxx_res->intr_reg[WCD9XXX_INTR_CLEAR_BASE] +
					      BIT_BYTE(irqbit),
			BYTE_BIT_MASK(irqbit));

		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			regmap_write(wcd9xxx_res->wcd_core_regmap,
				wcd9xxx_res->intr_reg[WCD9XXX_INTR_CLR_COMMIT],
				0x02);
		handle_nested_irq(phyirq_to_virq(wcd9xxx_res, irqbit));
		wcd9xxx_nested_irq_unlock(wcd9xxx_res);
	} else {
		wcd9xxx_nested_irq_lock(wcd9xxx_res);
		handle_nested_irq(phyirq_to_virq(wcd9xxx_res, irqbit));
		regmap_write(wcd9xxx_res->wcd_core_regmap,
			wcd9xxx_res->intr_reg[WCD9XXX_INTR_CLEAR_BASE] +
					      BIT_BYTE(irqbit),
			BYTE_BIT_MASK(irqbit));
		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			regmap_write(wcd9xxx_res->wcd_core_regmap,
				wcd9xxx_res->intr_reg[WCD9XXX_INTR_CLR_COMMIT],
				0x02);

		wcd9xxx_nested_irq_unlock(wcd9xxx_res);
	}
}

static irqreturn_t wcd9xxx_irq_thread(int irq, void *data)
{
	int ret;
	int i;
	struct intr_data irqdata;
	char linebuf[128];
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ, 1);
	struct wcd9xxx_core_resource *wcd9xxx_res = data;
	int num_irq_regs = wcd9xxx_res->num_irq_regs;
	u8 status[4], status1[4] = {0}, unmask_status[4] = {0};

	if (unlikely(wcd9xxx_lock_sleep(wcd9xxx_res) == false)) {
		dev_err(wcd9xxx_res->dev, "Failed to hold suspend\n");
		return IRQ_NONE;
	}

	if (!wcd9xxx_res->wcd_core_regmap) {
		dev_err(wcd9xxx_res->dev,
			"%s: Codec core regmap not supplied\n",
			   __func__);
		goto err_disable_irq;
	}

	memset(status, 0, sizeof(status));
	ret = regmap_bulk_read(wcd9xxx_res->wcd_core_regmap,
		wcd9xxx_res->intr_reg[WCD9XXX_INTR_STATUS_BASE],
		status, num_irq_regs);

	if (ret < 0) {
		dev_err(wcd9xxx_res->dev,
				"Failed to read interrupt status: %d\n", ret);
		goto err_disable_irq;
	}
	/*
	 * If status is 0 return without clearing.
	 * status contains: HW status - masked interrupts
	 * status1 contains: unhandled interrupts - masked interrupts
	 * unmasked_status contains: unhandled interrupts
	 */
	if (unlikely(!memcmp(status, status1, sizeof(status)))) {
		pr_debug("%s: status is 0\n", __func__);
		wcd9xxx_unlock_sleep(wcd9xxx_res);
		return IRQ_HANDLED;
	}

	/*
	 * Copy status to unmask_status before masking, otherwise SW may miss
	 * to clear masked interrupt in corner case.
	 */
	memcpy(unmask_status, status, sizeof(unmask_status));

	/* Apply masking */
	for (i = 0; i < num_irq_regs; i++)
		status[i] &= ~wcd9xxx_res->irq_masks_cur[i];

	memcpy(status1, status, sizeof(status1));

	/* Find out which interrupt was triggered and call that interrupt's
	 * handler function
	 *
	 * Since codec has only one hardware irq line which is shared by
	 * codec's different internal interrupts, so it's possible master irq
	 * handler dispatches multiple nested irq handlers after breaking
	 * order.  Dispatch interrupts in the order that is maintained by
	 * the interrupt table.
	 */
	for (i = 0; i < wcd9xxx_res->intr_table_size; i++) {
		irqdata = wcd9xxx_res->intr_table[i];
		if (status[BIT_BYTE(irqdata.intr_num)] &
			BYTE_BIT_MASK(irqdata.intr_num)) {
			wcd9xxx_irq_dispatch(wcd9xxx_res, &irqdata);
			status1[BIT_BYTE(irqdata.intr_num)] &=
					~BYTE_BIT_MASK(irqdata.intr_num);
			unmask_status[BIT_BYTE(irqdata.intr_num)] &=
					~BYTE_BIT_MASK(irqdata.intr_num);
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
		/*
		 * unmask_status contains unhandled interrupts, hence clear all
		 * unhandled interrupts.
		 */
		ret = regmap_bulk_write(wcd9xxx_res->wcd_core_regmap,
			wcd9xxx_res->intr_reg[WCD9XXX_INTR_CLEAR_BASE],
			unmask_status, num_irq_regs);
		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			regmap_write(wcd9xxx_res->wcd_core_regmap,
				wcd9xxx_res->intr_reg[WCD9XXX_INTR_CLR_COMMIT],
				0x02);
	}
	wcd9xxx_unlock_sleep(wcd9xxx_res);

	return IRQ_HANDLED;

err_disable_irq:
		dev_err(wcd9xxx_res->dev,
				"Disable irq %d\n", wcd9xxx_res->irq);

		disable_irq_wake(wcd9xxx_res->irq);
		disable_irq_nosync(wcd9xxx_res->irq);
		wcd9xxx_unlock_sleep(wcd9xxx_res);
		return IRQ_NONE;
}

void wcd9xxx_free_irq(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq, void *data)
{
	free_irq(phyirq_to_virq(wcd9xxx_res, irq), data);
}

void wcd9xxx_enable_irq(struct wcd9xxx_core_resource *wcd9xxx_res, int irq)
{
	if (wcd9xxx_res->irq)
		enable_irq(phyirq_to_virq(wcd9xxx_res, irq));
}

void wcd9xxx_disable_irq(struct wcd9xxx_core_resource *wcd9xxx_res, int irq)
{
	if (wcd9xxx_res->irq)
		disable_irq_nosync(phyirq_to_virq(wcd9xxx_res, irq));
}

void wcd9xxx_disable_irq_sync(
			struct wcd9xxx_core_resource *wcd9xxx_res, int irq)
{
	if (wcd9xxx_res->irq)
		disable_irq(phyirq_to_virq(wcd9xxx_res, irq));
}

static int wcd9xxx_irq_setup_downstream_irq(
			struct wcd9xxx_core_resource *wcd9xxx_res)
{
	int irq, virq, ret;

	pr_debug("%s: enter\n", __func__);

	for (irq = 0; irq < wcd9xxx_res->num_irqs; irq++) {
		/* Map OF irq */
		virq = wcd9xxx_map_irq(wcd9xxx_res, irq);
		pr_debug("%s: irq %d -> %d\n", __func__, irq, virq);
		if (virq == NO_IRQ) {
			pr_err("%s, No interrupt specifier for irq %d\n",
			       __func__, irq);
			return NO_IRQ;
		}

		ret = irq_set_chip_data(virq, wcd9xxx_res);
		if (ret) {
			pr_err("%s: Failed to configure irq %d (%d)\n",
			       __func__, irq, ret);
			return ret;
		}

		if (wcd9xxx_res->irq_level_high[irq])
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

int wcd9xxx_irq_init(struct wcd9xxx_core_resource *wcd9xxx_res)
{
	int i, ret;
	u8 irq_level[wcd9xxx_res->num_irq_regs];
	struct irq_domain *domain;
	struct device_node *pnode;

	mutex_init(&wcd9xxx_res->irq_lock);
	mutex_init(&wcd9xxx_res->nested_irq_lock);

	pnode = of_irq_find_parent(wcd9xxx_res->dev->of_node);
	if (unlikely(!pnode))
		return -EINVAL;

	domain = irq_find_host(pnode);
	if (unlikely(!domain))
		return -EINVAL;

	wcd9xxx_res->domain = domain;

	wcd9xxx_res->irq = wcd9xxx_irq_get_upstream_irq(wcd9xxx_res);
	if (!wcd9xxx_res->irq) {
		pr_warn("%s: irq driver is not yet initialized\n", __func__);
		mutex_destroy(&wcd9xxx_res->irq_lock);
		mutex_destroy(&wcd9xxx_res->nested_irq_lock);
		return -EPROBE_DEFER;
	}
	pr_debug("%s: probed irq %d\n", __func__, wcd9xxx_res->irq);

	/* Setup downstream IRQs */
	ret = wcd9xxx_irq_setup_downstream_irq(wcd9xxx_res);
	if (ret) {
		pr_err("%s: Failed to setup downstream IRQ\n", __func__);
		wcd9xxx_irq_put_upstream_irq(wcd9xxx_res);
		mutex_destroy(&wcd9xxx_res->irq_lock);
		mutex_destroy(&wcd9xxx_res->nested_irq_lock);
		return ret;
	}

	/* All other wcd9xxx interrupts are edge triggered */
	wcd9xxx_res->irq_level_high[0] = true;

	/* mask all the interrupts */
	memset(irq_level, 0, wcd9xxx_res->num_irq_regs);
	for (i = 0; i < wcd9xxx_res->num_irqs; i++) {
		wcd9xxx_res->irq_masks_cur[BIT_BYTE(i)] |= BYTE_BIT_MASK(i);
		wcd9xxx_res->irq_masks_cache[BIT_BYTE(i)] |= BYTE_BIT_MASK(i);
		irq_level[BIT_BYTE(i)] |=
		    wcd9xxx_res->irq_level_high[i] << (i % BITS_PER_BYTE);
	}

	if (!wcd9xxx_res->wcd_core_regmap) {
		dev_err(wcd9xxx_res->dev,
			"%s: Codec core regmap not defined\n",
			   __func__);
		ret = -EINVAL;
		goto fail_irq_init;
	}

	for (i = 0; i < wcd9xxx_res->num_irq_regs; i++) {
		/* Initialize interrupt mask and level registers */
		regmap_write(wcd9xxx_res->wcd_core_regmap,
			wcd9xxx_res->intr_reg[WCD9XXX_INTR_LEVEL_BASE] + i,
					irq_level[i]);
		regmap_write(wcd9xxx_res->wcd_core_regmap,
			wcd9xxx_res->intr_reg[WCD9XXX_INTR_MASK_BASE] + i,
			wcd9xxx_res->irq_masks_cur[i]);
	}

	ret = request_threaded_irq(wcd9xxx_res->irq, NULL, wcd9xxx_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "wcd9xxx", wcd9xxx_res);
	if (ret != 0)
		dev_err(wcd9xxx_res->dev, "Failed to request IRQ %d: %d\n",
			wcd9xxx_res->irq, ret);
	else {
		ret = enable_irq_wake(wcd9xxx_res->irq);
		if (ret)
			dev_err(wcd9xxx_res->dev,
				"Failed to set wake interrupt on IRQ %d: %d\n",
				wcd9xxx_res->irq, ret);
		if (ret)
			free_irq(wcd9xxx_res->irq, wcd9xxx_res);
	}

	if (ret)
		goto fail_irq_init;

	return ret;

fail_irq_init:
	dev_err(wcd9xxx_res->dev,
			"%s: Failed to init wcd9xxx irq\n", __func__);
	wcd9xxx_irq_put_upstream_irq(wcd9xxx_res);
	mutex_destroy(&wcd9xxx_res->irq_lock);
	mutex_destroy(&wcd9xxx_res->nested_irq_lock);
	return ret;
}

int wcd9xxx_request_irq(struct wcd9xxx_core_resource *wcd9xxx_res,
			int irq, irq_handler_t handler,
			const char *name, void *data)
{
	int virq;

	virq = phyirq_to_virq(wcd9xxx_res, irq);

	return request_threaded_irq(virq, NULL, handler, IRQF_TRIGGER_RISING,
				    name, data);
}

void wcd9xxx_irq_exit(struct wcd9xxx_core_resource *wcd9xxx_res)
{
	dev_dbg(wcd9xxx_res->dev, "%s: Cleaning up irq %d\n", __func__,
		wcd9xxx_res->irq);

	if (wcd9xxx_res->irq) {
		disable_irq_wake(wcd9xxx_res->irq);
		free_irq(wcd9xxx_res->irq, wcd9xxx_res);
		wcd9xxx_res->irq = 0;
		wcd9xxx_irq_put_upstream_irq(wcd9xxx_res);
	}
	mutex_destroy(&wcd9xxx_res->irq_lock);
	mutex_destroy(&wcd9xxx_res->nested_irq_lock);
}

#ifndef CONFIG_OF
static int phyirq_to_virq(
	struct wcd9xxx_core_resource *wcd9xxx_res,
	int offset)
{
	return wcd9xxx_res->irq_base + offset;
}

static int virq_to_phyirq(
	struct wcd9xxx_core_resource *wcd9xxx_res,
	int virq)
{
	return virq - wcd9xxx_res->irq_base;
}

static unsigned int wcd9xxx_irq_get_upstream_irq(
	struct wcd9xxx_core_resource *wcd9xxx_res)
{
	return wcd9xxx_res->irq;
}

static void wcd9xxx_irq_put_upstream_irq(
	struct wcd9xxx_core_resource *wcd9xxx_res)
{
	/* Do nothing */
}

static int wcd9xxx_map_irq(
	struct wcd9xxx_core_resource *wcd9xxx_core_res, int irq)
{
	return phyirq_to_virq(wcd9xxx_core_res, irq);
}
#else
static struct wcd9xxx_irq_drv_data *
wcd9xxx_irq_add_domain(struct device_node *node,
			       struct device_node *parent)
{
	struct wcd9xxx_irq_drv_data *data = NULL;

	pr_debug("%s: node %s, node parent %s\n", __func__,
		 node->name, node->parent->name);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return NULL;

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
		return NULL;
	}

	return data;
}

static struct wcd9xxx_irq_drv_data *
wcd9xxx_get_irq_drv_d(const struct wcd9xxx_core_resource *wcd9xxx_res)
{
	struct irq_domain *domain;

	domain = wcd9xxx_res->domain;

	if (domain)
		return domain->host_data;
	else
		return NULL;
}

static int phyirq_to_virq(struct wcd9xxx_core_resource *wcd9xxx_res, int offset)
{
	struct wcd9xxx_irq_drv_data *data;

	data = wcd9xxx_get_irq_drv_d(wcd9xxx_res);
	if (!data) {
		pr_warn("%s: not registered to interrupt controller\n",
			__func__);
		return -EINVAL;
	}
	return irq_linear_revmap(data->domain, offset);
}

static int virq_to_phyirq(struct wcd9xxx_core_resource *wcd9xxx_res, int virq)
{
	struct irq_data *irq_data = irq_get_irq_data(virq);
	if (unlikely(!irq_data)) {
		pr_err("%s: irq_data is NULL", __func__);
		return -EINVAL;
	}
	return irq_data->hwirq;
}

static unsigned int wcd9xxx_irq_get_upstream_irq(
				struct wcd9xxx_core_resource *wcd9xxx_res)
{
	struct wcd9xxx_irq_drv_data *data;

	data = wcd9xxx_get_irq_drv_d(wcd9xxx_res);
	if (!data) {
		pr_err("%s: interrupt controller is not registerd\n", __func__);
		return 0;
	}

	rmb();
	return data->irq;
}

static void wcd9xxx_irq_put_upstream_irq(
			struct wcd9xxx_core_resource *wcd9xxx_res)
{
	wcd9xxx_res->domain = NULL;
}

static int wcd9xxx_map_irq(struct wcd9xxx_core_resource *wcd9xxx_res, int irq)
{
	return of_irq_to_resource(wcd9xxx_res->dev->of_node, irq, NULL);
}

static int wcd9xxx_irq_probe(struct platform_device *pdev)
{
	int irq, dir_apps_irq = -EINVAL;
	struct wcd9xxx_irq_drv_data *data;
	struct device_node *node = pdev->dev.of_node;
	int ret = -EINVAL;

	irq = of_get_named_gpio(node, "qcom,gpio-connect", 0);
	if (!gpio_is_valid(irq))
		dir_apps_irq = platform_get_irq_byname(pdev, "wcd_irq");

	if (!gpio_is_valid(irq) && dir_apps_irq < 0) {
		dev_err(&pdev->dev, "TLMM connect gpio not found\n");
		return -EPROBE_DEFER;
	} else {
		if (dir_apps_irq > 0) {
			irq = dir_apps_irq;
		} else {
			irq = gpio_to_irq(irq);
			if (irq < 0) {
				dev_err(&pdev->dev, "Unable to configure irq\n");
				return irq;
			}
		}
		dev_dbg(&pdev->dev, "%s: virq = %d\n", __func__, irq);
		data = wcd9xxx_irq_add_domain(node, node->parent);
		if (!data) {
			pr_err("%s: irq_add_domain failed\n", __func__);
			return -EINVAL;
		}
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
	if (unlikely(!domain)) {
		pr_err("%s: domain is NULL", __func__);
		return -EINVAL;
	}
	data = (struct wcd9xxx_irq_drv_data *)domain->host_data;
	data->irq = 0;
	wmb();
	irq_domain_remove(data->domain);
	kfree(data);
	domain->host_data = NULL;

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
