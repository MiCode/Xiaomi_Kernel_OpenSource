// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2016, 2020, The Linux Foundation. All rights reserved.
 */
#include <linux/interrupt.h>
#include "ipa_i.h"

#define INTERRUPT_WORKQUEUE_NAME "ipa_interrupt_wq"
#define IPA_IRQ_NUM_MAX 32

struct ipa_interrupt_info {
	ipa_irq_handler_t handler;
	enum ipa_irq_type interrupt;
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

static struct ipa_interrupt_info ipa_interrupt_to_cb[IPA_IRQ_NUM_MAX];
static struct workqueue_struct *ipa_interrupt_wq;
static u32 ipa_ee;

static void ipa_interrupt_defer(struct work_struct *work);
static DECLARE_WORK(ipa_interrupt_defer_work, ipa_interrupt_defer);

static int ipa2_irq_mapping[IPA_IRQ_MAX] = {
	[IPA_BAD_SNOC_ACCESS_IRQ]		= 0,
	[IPA_EOT_COAL_IRQ]			= 1,
	[IPA_UC_IRQ_0]				= 2,
	[IPA_UC_IRQ_1]				= 3,
	[IPA_UC_IRQ_2]				= 4,
	[IPA_UC_IRQ_3]				= 5,
	[IPA_UC_IN_Q_NOT_EMPTY_IRQ]		= 6,
	[IPA_UC_RX_CMD_Q_NOT_FULL_IRQ]		= 7,
	[IPA_UC_TX_CMD_Q_NOT_FULL_IRQ]		= 8,
	[IPA_UC_TO_PROC_ACK_Q_NOT_FULL_IRQ]	= 9,
	[IPA_PROC_TO_UC_ACK_Q_NOT_EMPTY_IRQ]	= 10,
	[IPA_RX_ERR_IRQ]			= 11,
	[IPA_DEAGGR_ERR_IRQ]			= 12,
	[IPA_TX_ERR_IRQ]			= 13,
	[IPA_STEP_MODE_IRQ]			= 14,
	[IPA_PROC_ERR_IRQ]			= 15,
	[IPA_TX_SUSPEND_IRQ]			= 16,
	[IPA_TX_HOLB_DROP_IRQ]			= 17,
	[IPA_BAM_IDLE_IRQ]			= 18,
};

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

	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
		if ((ep_suspend_data & bmsk) && (ipa_ctx->ep[i].valid))
			return true;
		bmsk = bmsk << 1;
	}
	return false;
}

static int handle_interrupt(int irq_num, bool isr_context)
{
	struct ipa_interrupt_info interrupt_info;
	struct ipa_interrupt_work_wrap *work_data;
	u32 suspend_data;
	void *interrupt_data = NULL;
	struct ipa_tx_suspend_irq_data *suspend_interrupt_data = NULL;
	int res;

	interrupt_info = ipa_interrupt_to_cb[irq_num];
	if (interrupt_info.handler == NULL) {
		IPAERR("A callback function wasn't set for interrupt num %d\n",
			irq_num);
		return -EINVAL;
	}

	switch (interrupt_info.interrupt) {
	case IPA_TX_SUSPEND_IRQ:
		IPADBG_LOW("processing TX_SUSPEND interrupt work-around\n");
		suspend_data = ipa_read_reg(ipa_ctx->mmio,
					IPA_IRQ_SUSPEND_INFO_EE_n_ADDR(ipa_ee));
		if (!is_valid_ep(suspend_data))
			return 0;
		IPADBG_LOW("get interrupt %d\n", suspend_data);
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

	/* Force defer processing if in ISR context. */
	if (interrupt_info.deferred_flag || isr_context) {
		work_data = kzalloc(sizeof(struct ipa_interrupt_work_wrap),
				GFP_ATOMIC);
		if (!work_data) {
			IPAERR("failed allocating ipa_interrupt_work_wrap\n");
			res = -ENOMEM;
			goto fail_alloc_work;
		}
		INIT_WORK(&work_data->interrupt_work, deferred_interrupt_work);
		work_data->handler = interrupt_info.handler;
		work_data->interrupt = interrupt_info.interrupt;
		work_data->private_data = interrupt_info.private_data;
		work_data->interrupt_data = interrupt_data;
		queue_work(ipa_interrupt_wq, &work_data->interrupt_work);

	} else {
		interrupt_info.handler(interrupt_info.interrupt,
			interrupt_info.private_data,
			interrupt_data);
		kfree(interrupt_data);
	}

	return 0;

fail_alloc_work:
	kfree(interrupt_data);
	return res;
}

static inline bool is_uc_irq(int irq_num)
{
	if (ipa_interrupt_to_cb[irq_num].interrupt >= IPA_UC_IRQ_0 &&
		ipa_interrupt_to_cb[irq_num].interrupt <= IPA_UC_IRQ_3)
		return true;
	else
		return false;
}

static void ipa_process_interrupts(bool isr_context)
{
	u32 reg;
	u32 bmsk;
	u32 i = 0;
	u32 en;
	bool uc_irq;

	en = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	reg = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_STTS_EE_n_ADDR(ipa_ee));
	IPADBG_LOW(
		"ISR enter\n isr_ctx = %d EN reg = 0x%x STTS reg = 0x%x\n",
		isr_context, en, reg);
	while (en & reg) {
		bmsk = 1;
		for (i = 0; i < IPA_IRQ_NUM_MAX; i++) {
			if (!(en & reg & bmsk)) {
				bmsk = bmsk << 1;
				continue;
			}
			uc_irq = is_uc_irq(i);
			/*
			 * Clear uC interrupt before processing to avoid
			 * clearing unhandled interrupts
			 */
			if (uc_irq)
				ipa_write_reg(ipa_ctx->mmio,
					IPA_IRQ_CLR_EE_n_ADDR(ipa_ee), bmsk);

			/* Process the interrupts */
			handle_interrupt(i, isr_context);

			/*
			 * Clear non uC interrupt after processing
			 * to avoid clearing interrupt data
			 */
			if (!uc_irq)
				ipa_write_reg(ipa_ctx->mmio,
				   IPA_IRQ_CLR_EE_n_ADDR(ipa_ee), bmsk);

			bmsk = bmsk << 1;
		}
		/*
		 * Check pending interrupts that may have
		 * been raised since last read
		 */
		reg = ipa_read_reg(ipa_ctx->mmio,
				IPA_IRQ_STTS_EE_n_ADDR(ipa_ee));
	}
	IPADBG_LOW("Exit\n");
}

static void ipa_interrupt_defer(struct work_struct *work)
{
	IPADBG_LOW("processing interrupts in wq\n");
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipa_process_interrupts(false);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG_LOW("Done\n");
}

static irqreturn_t ipa_isr(int irq, void *ctxt)
{
	unsigned long flags;

	IPADBG_LOW("Enter\n");
	/* defer interrupt handling in case IPA is not clocked on */
	if (ipa_active_clients_trylock(&flags) == 0) {
		IPADBG("defer interrupt processing\n");
		queue_work(ipa_ctx->power_mgmt_wq, &ipa_interrupt_defer_work);
		return IRQ_HANDLED;
	}

	if (ipa_ctx->ipa_active_clients.cnt == 0) {
		IPADBG("defer interrupt processing\n");
		queue_work(ipa_ctx->power_mgmt_wq, &ipa_interrupt_defer_work);
		goto bail;
	}

	ipa_process_interrupts(true);
	IPADBG_LOW("Exit\n");
bail:
	ipa_active_clients_trylock_unlock(&flags);
	return IRQ_HANDLED;
}
/**
 * ipa2_add_interrupt_handler() - Adds handler to an interrupt type
 * @interrupt:		Interrupt type
 * @handler:		The handler to be added
 * @deferred_flag:	whether the handler processing should be deferred in
 *			a workqueue
 * @private_data:	the client's private data
 *
 * Adds handler to an interrupt type and enable the specific bit
 * in IRQ_EN register, associated interrupt in IRQ_STTS register will be enabled
 */
int ipa2_add_interrupt_handler(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data)
{
	u32 val;
	u32 bmsk;
	int irq_num;

	IPADBG_LOW("interrupt_enum(%d)\n", interrupt);
	if (interrupt < IPA_BAD_SNOC_ACCESS_IRQ ||
		interrupt >= IPA_IRQ_MAX) {
		IPAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}

	irq_num = ipa2_irq_mapping[interrupt];
	if (irq_num < 0 || irq_num >= IPA_IRQ_NUM_MAX) {
		IPAERR("interrupt %d not supported\n", interrupt);
		WARN_ON(1);
		return -EFAULT;
	}

	ipa_interrupt_to_cb[irq_num].deferred_flag = deferred_flag;
	ipa_interrupt_to_cb[irq_num].handler = handler;
	ipa_interrupt_to_cb[irq_num].private_data = private_data;
	ipa_interrupt_to_cb[irq_num].interrupt = interrupt;

	val = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	IPADBG("read IPA_IRQ_EN_EE_n_ADDR register. reg = %d\n", val);
	bmsk = 1 << irq_num;
	val |= bmsk;
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee), val);
	IPADBG_LOW("wrote IPA_IRQ_EN_EE_n_ADDR register. reg = %d\n", val);
	return 0;
}

/**
 * ipa2_remove_interrupt_handler() - Removes handler to an interrupt type
 * @interrupt:		Interrupt type
 *
 * Removes the handler and disable the specific bit in IRQ_EN register
 */
int ipa2_remove_interrupt_handler(enum ipa_irq_type interrupt)
{
	u32 val;
	u32 bmsk;
	int irq_num;

	if (interrupt < IPA_BAD_SNOC_ACCESS_IRQ ||
		interrupt >= IPA_IRQ_MAX) {
		IPAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}

	irq_num = ipa2_irq_mapping[interrupt];
	if (irq_num < 0 || irq_num >= IPA_IRQ_NUM_MAX) {
		IPAERR("interrupt %d not supported\n", interrupt);
		WARN_ON(1);
		return -EFAULT;
	}

	kfree(ipa_interrupt_to_cb[irq_num].private_data);
	ipa_interrupt_to_cb[irq_num].deferred_flag = false;
	ipa_interrupt_to_cb[irq_num].handler = NULL;
	ipa_interrupt_to_cb[irq_num].private_data = NULL;
	ipa_interrupt_to_cb[irq_num].interrupt = -1;

	val = ipa_read_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	bmsk = 1 << irq_num;
	val &= ~bmsk;
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee), val);

	return 0;
}

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
	for (idx = 0; idx < IPA_IRQ_NUM_MAX; idx++) {
		ipa_interrupt_to_cb[idx].deferred_flag = false;
		ipa_interrupt_to_cb[idx].handler = NULL;
		ipa_interrupt_to_cb[idx].private_data = NULL;
		ipa_interrupt_to_cb[idx].interrupt = -1;
	}

	ipa_interrupt_wq = create_singlethread_workqueue(
			INTERRUPT_WORKQUEUE_NAME);
	if (!ipa_interrupt_wq) {
		IPAERR("workqueue creation failed\n");
		return -ENOMEM;
	}

	/*Clearing interrupts status*/
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_CLR_EE_n_ADDR(ipa_ee), reg);

	res = request_irq(ipa_irq, (irq_handler_t) ipa_isr,
				IRQF_TRIGGER_RISING, "ipa", ipa_dev);
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
