/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include "ipa_i.h"

#define INTERRUPT_WORKQUEUE_NAME "ipa_interrupt_wq"

struct ipa_interrupt_info {
	ipa_irq_handler_t handler;
	void *private_data;
	bool deferred_flag;
};

struct ipa_interrupt_work_wrap {
	struct work_struct interrupt_work;
	ipa_irq_handler_t handler;
	enum ipa_irq_type interrupt;
	void *private_data;
	void *interrupt_data;
};

static struct ipa_interrupt_info ipa_interrupt_to_cb[IPA_IRQ_MAX];
static struct workqueue_struct *ipa_interrupt_wq;
static u32 ipa_ee;

static void deferred_interrupt_work(struct work_struct *work)
{
	struct ipa_interrupt_work_wrap *work_data =
			container_of(work,
			struct ipa_interrupt_work_wrap,
			interrupt_work);
	IPADBG("call handler from workq...\n");
	work_data->handler(work_data->interrupt, work_data->private_data,
			work_data->interrupt_data);
	kfree(work_data->interrupt_data);
	kfree(work_data);
}

static bool is_valid_ep(u32 ep_suspend_data)
{
	u32 bmsk = 1;
	u32 i = 0;

	for (i = 0; i < IPA_NUM_PIPES; i++) {
		if ((ep_suspend_data & bmsk) && (ipa_ctx->ep[i].valid))
			return true;
		bmsk = bmsk << 1;
	}
	return false;
}

static int handle_interrupt(enum ipa_irq_type interrupt)
{
	struct ipa_interrupt_info interrupt_info;
	struct ipa_interrupt_work_wrap *work_data;
	u32 suspend_data;
	void *interrupt_data = NULL;
	struct ipa_tx_suspend_irq_data *suspend_interrupt_data = NULL;
	int res;

	interrupt_info = ipa_interrupt_to_cb[interrupt];
	if (interrupt_info.handler == NULL) {
		IPAERR("A callback function wasn't set for interrupt type %d\n",
				interrupt);
		return -EINVAL;
	}

	switch (interrupt) {
	case IPA_TX_SUSPEND_IRQ:
		suspend_data = ipa_read_reg(ipa_ctx->mmio,
					IPA_IRQ_SUSPEND_INFO_EE_n_ADDR(ipa_ee));
		if (!is_valid_ep(suspend_data))
			return 0;

		suspend_interrupt_data =
			kzalloc(sizeof(*suspend_interrupt_data), GFP_ATOMIC);
		if (!suspend_interrupt_data) {
			IPAERR("failed allocating suspend_interrupt_data\n");
			return -ENOMEM;
		}
		suspend_interrupt_data->endpoints = suspend_data;
		interrupt_data = suspend_interrupt_data;
		break;
	default:
		break;
	}

	if (interrupt_info.deferred_flag) {
		work_data = kzalloc(sizeof(struct ipa_interrupt_work_wrap),
				GFP_ATOMIC);
		if (!work_data) {
			IPAERR("failed allocating ipa_interrupt_work_wrap\n");
			res = -ENOMEM;
			goto fail_alloc_work;
		}
		INIT_WORK(&work_data->interrupt_work, deferred_interrupt_work);
		work_data->handler = interrupt_info.handler;
		work_data->interrupt = interrupt;
		work_data->private_data = interrupt_info.private_data;
		work_data->interrupt_data = interrupt_data;
		queue_work(ipa_interrupt_wq, &work_data->interrupt_work);

	} else {
		interrupt_info.handler(interrupt, interrupt_info.private_data,
				interrupt_data);
		kfree(interrupt_data);
	}

	return 0;

fail_alloc_work:
	kfree(interrupt_data);
	return res;
}

static irqreturn_t ipa_isr(int irq, void *ctxt)
{
	u32 reg;
	u32 bmsk = 1;
	u32 i = 0;
	u32 en;

	en = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	reg = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_STTS_EE_n_ADDR(ipa_ee));
	for (i = 0; i < IPA_IRQ_MAX; i++) {
		if (en & reg & bmsk)
			handle_interrupt(i);
		bmsk = bmsk << 1;
	}
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_CLR_EE_n_ADDR(ipa_ee), reg);

	return IRQ_HANDLED;
}
/**
* ipa_add_interrupt_handler() - Adds handler to an interrupt type
* @interrupt:		Interrupt type
* @handler:		The handler to be added
* @deferred_flag:	whether the handler processing should be deferred in
*			a workqueue
* @private_data:	the client's private data
*
* Adds handler to an interrupt type and enable the specific bit
* in IRQ_EN register, associated interrupt in IRQ_STTS register will be enabled
*/
int ipa_add_interrupt_handler(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data)
{
	u32 val;
	u32 bmsk;

	IPADBG("in ipa_add_interrupt_handler\n");
	if (interrupt < IPA_BAD_SNOC_ACCESS_IRQ || interrupt >= IPA_IRQ_MAX) {
		IPAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}
	ipa_interrupt_to_cb[interrupt].deferred_flag = deferred_flag;
	ipa_interrupt_to_cb[interrupt].handler = handler;
	ipa_interrupt_to_cb[interrupt].private_data = private_data;

	val = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	IPADBG("read IPA_IRQ_EN_EE_n_ADDR register. reg = %d\n", val);
	bmsk = 1<<interrupt;
	val |= bmsk;
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee), val);
	IPADBG("wrote IPA_IRQ_EN_EE_n_ADDR register. reg = %d\n", val);
	return 0;
}
EXPORT_SYMBOL(ipa_add_interrupt_handler);

/**
* ipa_remove_interrupt_handler() - Removes handler to an interrupt type
* @interrupt:		Interrupt type
*
* Removes the handler and disable the specific bit in IRQ_EN register
*/
int ipa_remove_interrupt_handler(enum ipa_irq_type interrupt)
{
	u32 val;
	u32 bmsk;

	if (interrupt < IPA_BAD_SNOC_ACCESS_IRQ || interrupt >= IPA_IRQ_MAX) {
		IPAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}
	ipa_interrupt_to_cb[interrupt].deferred_flag = false;
	ipa_interrupt_to_cb[interrupt].handler = NULL;
	ipa_interrupt_to_cb[interrupt].private_data = NULL;
	val = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	bmsk = 1<<interrupt;
	val &= ~bmsk;
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee), val);

	return 0;
}
EXPORT_SYMBOL(ipa_remove_interrupt_handler);

/**
* ipa_interrupts_init() - Initialize the IPA interrupts framework
* @ipa_irq:	The interrupt number to allocate
* @ee:		Execution environment
* @ipa_dev:	The basic device structure representing the IPA driver
*
* - Initialize the ipa_interrupt_to_cb array
* - Clear interrupts status
* - Register the ipa interrupt handler - ipa_isr
* - Enable apps processor wakeup by IPA interrupts
*/
int ipa_interrupts_init(u32 ipa_irq, u32 ee, struct device *ipa_dev)
{
	int idx;
	u32 reg = 0xFFFFFFFF;
	int res = 0;

	ipa_ee = ee;
	for (idx = 0; idx < IPA_IRQ_MAX; idx++) {
		ipa_interrupt_to_cb[idx].deferred_flag = false;
		ipa_interrupt_to_cb[idx].handler = NULL;
		ipa_interrupt_to_cb[idx].private_data = NULL;
	}

	ipa_interrupt_wq = create_workqueue(INTERRUPT_WORKQUEUE_NAME);
	if (!ipa_interrupt_wq) {
		IPAERR("workqueue creation failed\n");
		return -ENOMEM;
	}

	/*Clearing interrupts status*/
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_CLR_EE_n_ADDR(ipa_ee), reg);

	res = request_irq(ipa_irq, (irq_handler_t) ipa_isr,
				IRQF_TRIGGER_HIGH, "ipa", ipa_dev);
	if (res) {
		IPAERR("fail to register IPA IRQ handler irq=%d\n", ipa_irq);
		return -ENODEV;
	}
	IPADBG("IPA IRQ handler irq=%d registered\n", ipa_irq);

	res = enable_irq_wake(ipa_irq);
	if (res)
		IPAERR("fail to enable IPA IRQ wakeup irq=%d res=%d\n",
				ipa_irq, res);
	else
		IPADBG("IPA IRQ wakeup enabled irq=%d\n", ipa_irq);

	return 0;
}
