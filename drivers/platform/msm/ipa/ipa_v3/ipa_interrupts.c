/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#define DIS_SUSPEND_INTERRUPT_TIMEOUT 5
#define IPA_IRQ_NUM_MAX 32

struct ipa3_interrupt_info {
	ipa_irq_handler_t handler;
	enum ipa_irq_type interrupt;
	void *private_data;
	bool deferred_flag;
};

struct ipa3_interrupt_work_wrap {
	struct work_struct interrupt_work;
	ipa_irq_handler_t handler;
	enum ipa_irq_type interrupt;
	void *private_data;
	void *interrupt_data;
};

static struct ipa3_interrupt_info ipa_interrupt_to_cb[IPA_IRQ_NUM_MAX];
static struct workqueue_struct *ipa_interrupt_wq;
static u32 ipa_ee;

static void ipa3_tx_suspend_interrupt_wa(void);
static void ipa3_enable_tx_suspend_wa(struct work_struct *work);
static DECLARE_DELAYED_WORK(dwork_en_suspend_int,
						ipa3_enable_tx_suspend_wa);
static spinlock_t suspend_wa_lock;
static void ipa3_process_interrupts(bool isr_context);

static int ipa3_irq_mapping[IPA_IRQ_MAX] = {
	[IPA_UC_TX_CMD_Q_NOT_FULL_IRQ]		= -1,
	[IPA_UC_TO_PROC_ACK_Q_NOT_FULL_IRQ]	= -1,
	[IPA_BAD_SNOC_ACCESS_IRQ]		= 0,
	[IPA_EOT_COAL_IRQ]			= 1,
	[IPA_UC_IRQ_0]				= 2,
	[IPA_UC_IRQ_1]				= 3,
	[IPA_UC_IRQ_2]				= 4,
	[IPA_UC_IRQ_3]				= 5,
	[IPA_UC_IN_Q_NOT_EMPTY_IRQ]		= 6,
	[IPA_UC_RX_CMD_Q_NOT_FULL_IRQ]		= 7,
	[IPA_PROC_TO_UC_ACK_Q_NOT_EMPTY_IRQ]	= 8,
	[IPA_RX_ERR_IRQ]			= 9,
	[IPA_DEAGGR_ERR_IRQ]			= 10,
	[IPA_TX_ERR_IRQ]			= 11,
	[IPA_STEP_MODE_IRQ]			= 12,
	[IPA_PROC_ERR_IRQ]			= 13,
	[IPA_TX_SUSPEND_IRQ]			= 14,
	[IPA_TX_HOLB_DROP_IRQ]			= 15,
	[IPA_BAM_IDLE_IRQ]			= 16,
};

static void ipa3_interrupt_defer(struct work_struct *work);
static DECLARE_WORK(ipa3_interrupt_defer_work, ipa3_interrupt_defer);

static void ipa3_deferred_interrupt_work(struct work_struct *work)
{
	struct ipa3_interrupt_work_wrap *work_data =
			container_of(work,
			struct ipa3_interrupt_work_wrap,
			interrupt_work);
	IPADBG("call handler from workq...\n");
	work_data->handler(work_data->interrupt, work_data->private_data,
			work_data->interrupt_data);
	kfree(work_data->interrupt_data);
	kfree(work_data);
}

static bool ipa3_is_valid_ep(u32 ep_suspend_data)
{
	u32 bmsk = 1;
	u32 i = 0;

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if ((ep_suspend_data & bmsk) && (ipa3_ctx->ep[i].valid))
			return true;
		bmsk = bmsk << 1;
	}
	return false;
}

static int ipa3_handle_interrupt(int irq_num, bool isr_context)
{
	struct ipa3_interrupt_info interrupt_info;
	struct ipa3_interrupt_work_wrap *work_data;
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
		IPADBG("processing TX_SUSPEND interrupt work-around\n");
		ipa3_tx_suspend_interrupt_wa();
		suspend_data = ipa_read_reg(ipa3_ctx->mmio,
					IPA_IRQ_SUSPEND_INFO_EE_n_ADDR(ipa_ee));
		if (!ipa3_is_valid_ep(suspend_data))
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

	/* Force defer processing if in ISR context. */
	if (interrupt_info.deferred_flag || isr_context) {
		work_data = kzalloc(sizeof(struct ipa3_interrupt_work_wrap),
				GFP_ATOMIC);
		if (!work_data) {
			IPAERR("failed allocating ipa3_interrupt_work_wrap\n");
			res = -ENOMEM;
			goto fail_alloc_work;
		}
		INIT_WORK(&work_data->interrupt_work,
				ipa3_deferred_interrupt_work);
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

static void ipa3_enable_tx_suspend_wa(struct work_struct *work)
{
	u32 en;
	u32 suspend_bmask;
	int irq_num;

	IPADBG("Enter\n");

	irq_num = ipa3_irq_mapping[IPA_TX_SUSPEND_IRQ];
	BUG_ON(irq_num == -1);

	/* make sure ipa hw is clocked on*/
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	en = ipa_read_reg(ipa3_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	suspend_bmask = 1 << irq_num;
	/*enable  TX_SUSPEND_IRQ*/
	en |= suspend_bmask;
	IPADBG("enable TX_SUSPEND_IRQ, IPA_IRQ_EN_EE reg, write val = %u\n"
		, en);
	ipa3_uc_rg10_write_reg(ipa3_ctx->mmio,
		IPA_IRQ_EN_EE_n_ADDR(ipa_ee), en);
	ipa3_process_interrupts(false);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("Exit\n");
}

static void ipa3_tx_suspend_interrupt_wa(void)
{
	u32 val;
	u32 suspend_bmask;
	int irq_num;

	IPADBG("Enter\n");
	irq_num = ipa3_irq_mapping[IPA_TX_SUSPEND_IRQ];
	BUG_ON(irq_num == -1);

	/*disable TX_SUSPEND_IRQ*/
	val = ipa_read_reg(ipa3_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	suspend_bmask = 1 << irq_num;
	val &= ~suspend_bmask;
	IPADBG("Disabling TX_SUSPEND_IRQ, write val: %u to IPA_IRQ_EN_EE reg\n",
		val);
	ipa3_uc_rg10_write_reg(ipa3_ctx->mmio,
		IPA_IRQ_EN_EE_n_ADDR(ipa_ee), val);

	IPADBG(" processing suspend interrupt work-around, delayed work\n");
	queue_delayed_work(ipa_interrupt_wq, &dwork_en_suspend_int,
			msecs_to_jiffies(DIS_SUSPEND_INTERRUPT_TIMEOUT));

	IPADBG("Exit\n");
}

static void ipa3_process_interrupts(bool isr_context)
{
	u32 reg;
	u32 bmsk;
	u32 i = 0;
	u32 en;
	unsigned long flags;

	IPADBG("Enter\n");

	spin_lock_irqsave(&suspend_wa_lock, flags);
	en = ipa_read_reg(ipa3_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	reg = ipa_read_reg(ipa3_ctx->mmio, IPA_IRQ_STTS_EE_n_ADDR(ipa_ee));
	while (en & reg) {
		bmsk = 1;
		for (i = 0; i < IPA_IRQ_NUM_MAX; i++) {
			if (en & reg & bmsk) {
				/*
				 * handle the interrupt with spin_lock
				 * unlocked to avoid calling client in atomic
				 * context. mutual exclusion still preserved
				 * as the read/clr is done with spin_lock
				 * locked.
				 */
				spin_unlock_irqrestore(&suspend_wa_lock, flags);
				ipa3_handle_interrupt(i, isr_context);
				spin_lock_irqsave(&suspend_wa_lock, flags);
			}
			bmsk = bmsk << 1;
		}
		ipa3_uc_rg10_write_reg(ipa3_ctx->mmio,
				IPA_IRQ_CLR_EE_n_ADDR(ipa_ee), reg);
		reg = ipa_read_reg(ipa3_ctx->mmio,
				IPA_IRQ_STTS_EE_n_ADDR(ipa_ee));
		/* since the suspend interrupt HW bug we must
		  * read again the EN register, otherwise the while is endless
		  */
		en = ipa_read_reg(ipa3_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	}

	spin_unlock_irqrestore(&suspend_wa_lock, flags);
	IPADBG("Exit\n");
}

static void ipa3_interrupt_defer(struct work_struct *work)
{
	IPADBG("processing interrupts in wq\n");
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipa3_process_interrupts(false);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG("Done\n");
}

static irqreturn_t ipa3_isr(int irq, void *ctxt)
{
	unsigned long flags;

	IPADBG("Enter\n");
	/* defer interrupt handling in case IPA is not clocked on */
	if (ipa3_active_clients_trylock(&flags) == 0) {
		IPADBG("defer interrupt processing\n");
		queue_work(ipa3_ctx->power_mgmt_wq, &ipa3_interrupt_defer_work);
		return IRQ_HANDLED;
	}

	if (ipa3_ctx->ipa3_active_clients.cnt == 0) {
		IPADBG("defer interrupt processing\n");
		queue_work(ipa3_ctx->power_mgmt_wq, &ipa3_interrupt_defer_work);
		goto bail;
	}

	ipa3_process_interrupts(true);
	IPADBG("Exit\n");

bail:
	ipa3_active_clients_trylock_unlock(&flags);
	return IRQ_HANDLED;
}
/**
* ipa3_add_interrupt_handler() - Adds handler to an interrupt type
* @interrupt:		Interrupt type
* @handler:		The handler to be added
* @deferred_flag:	whether the handler processing should be deferred in
*			a workqueue
* @private_data:	the client's private data
*
* Adds handler to an interrupt type and enable the specific bit
* in IRQ_EN register, associated interrupt in IRQ_STTS register will be enabled
*/
int ipa3_add_interrupt_handler(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data)
{
	u32 val;
	u32 bmsk;
	int irq_num;

	IPADBG("in ipa3_add_interrupt_handler\n");
	if (interrupt < IPA_BAD_SNOC_ACCESS_IRQ ||
		interrupt >= IPA_IRQ_MAX) {
		IPAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}

	irq_num = ipa3_irq_mapping[interrupt];
	if (irq_num < 0 || irq_num >= IPA_IRQ_NUM_MAX) {
		IPAERR("interrupt %d not supported\n", interrupt);
		WARN_ON(1);
		return -EFAULT;
	}

	ipa_interrupt_to_cb[irq_num].deferred_flag = deferred_flag;
	ipa_interrupt_to_cb[irq_num].handler = handler;
	ipa_interrupt_to_cb[irq_num].private_data = private_data;
	ipa_interrupt_to_cb[irq_num].interrupt = interrupt;

	val = ipa_read_reg(ipa3_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	IPADBG("read IPA_IRQ_EN_EE_n_ADDR register. reg = %d\n", val);
	bmsk = 1 << irq_num;
	val |= bmsk;
	ipa3_uc_rg10_write_reg(ipa3_ctx->mmio,
		IPA_IRQ_EN_EE_n_ADDR(ipa_ee), val);
	IPADBG("wrote IPA_IRQ_EN_EE_n_ADDR register. reg = %d\n", val);
	return 0;
}

/**
* ipa3_remove_interrupt_handler() - Removes handler to an interrupt type
* @interrupt:		Interrupt type
*
* Removes the handler and disable the specific bit in IRQ_EN register
*/
int ipa3_remove_interrupt_handler(enum ipa_irq_type interrupt)
{
	u32 val;
	u32 bmsk;
	int irq_num;

	if (interrupt < IPA_BAD_SNOC_ACCESS_IRQ ||
		interrupt >= IPA_IRQ_MAX) {
		IPAERR("invalid interrupt number %d\n", interrupt);
		return -EINVAL;
	}

	irq_num = ipa3_irq_mapping[interrupt];
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

	val = ipa_read_reg(ipa3_ctx->mmio, IPA_IRQ_EN_EE_n_ADDR(ipa_ee));
	bmsk = 1 << irq_num;
	val &= ~bmsk;
	ipa3_uc_rg10_write_reg(ipa3_ctx->mmio,
		IPA_IRQ_EN_EE_n_ADDR(ipa_ee), val);

	return 0;
}

/**
* ipa3_interrupts_init() - Initialize the IPA interrupts framework
* @ipa_irq:	The interrupt number to allocate
* @ee:		Execution environment
* @ipa_dev:	The basic device structure representing the IPA driver
*
* - Initialize the ipa_interrupt_to_cb array
* - Clear interrupts status
* - Register the ipa interrupt handler - ipa3_isr
* - Enable apps processor wakeup by IPA interrupts
*/
int ipa3_interrupts_init(u32 ipa_irq, u32 ee, struct device *ipa_dev)
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
	ipa3_uc_rg10_write_reg(ipa3_ctx->mmio,
		IPA_IRQ_CLR_EE_n_ADDR(ipa_ee), reg);

	res = request_irq(ipa_irq, (irq_handler_t) ipa3_isr,
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

	spin_lock_init(&suspend_wa_lock);
	return 0;
}

/**
* ipa3_suspend_active_aggr_wa() - Emulate suspend IRQ
* @clnt_hndl:		suspended client handle, IRQ is emulated for this pipe
*
*  Emulate suspend IRQ to unsuspend client which was suspended with an open
*  aggregation frame in order to bypass HW bug of IRQ not generated when
*  endpoint is suspended during an open aggregation.
*/
void ipa3_suspend_active_aggr_wa(u32 clnt_hdl)
{
	struct ipa3_interrupt_info interrupt_info;
	struct ipa3_interrupt_work_wrap *work_data;
	struct ipa_tx_suspend_irq_data *suspend_interrupt_data;
	int irq_num;
	int aggr_active_bitmap = ipa_read_reg(ipa3_ctx->mmio,
			IPA_STATE_AGGR_ACTIVE_OFST);

	if (aggr_active_bitmap & (1 << clnt_hdl)) {
		/* force close aggregation */
		ipa_write_reg(ipa3_ctx->mmio, IPA_AGGR_FORCE_CLOSE_OFST,
				(1 << clnt_hdl));

		/* simulate suspend IRQ */
		irq_num = ipa3_irq_mapping[IPA_TX_SUSPEND_IRQ];
		interrupt_info = ipa_interrupt_to_cb[irq_num];
		if (interrupt_info.handler == NULL) {
			IPAERR("no CB function for IPA_TX_SUSPEND_IRQ!\n");
			return;
		}
		suspend_interrupt_data = kzalloc(
				sizeof(*suspend_interrupt_data),
				GFP_KERNEL);
		if (!suspend_interrupt_data) {
			IPAERR("failed allocating suspend_interrupt_data\n");
			return;
		}
		suspend_interrupt_data->endpoints = 1 << clnt_hdl;

		work_data = kzalloc(sizeof(struct ipa3_interrupt_work_wrap),
				GFP_KERNEL);
		if (!work_data) {
			IPAERR("failed allocating ipa3_interrupt_work_wrap\n");
			goto fail_alloc_work;
		}
		INIT_WORK(&work_data->interrupt_work,
				ipa3_deferred_interrupt_work);
		work_data->handler = interrupt_info.handler;
		work_data->interrupt = IPA_TX_SUSPEND_IRQ;
		work_data->private_data = interrupt_info.private_data;
		work_data->interrupt_data = (void *)suspend_interrupt_data;
		queue_work(ipa_interrupt_wq, &work_data->interrupt_work);
		return;
fail_alloc_work:
		kfree(suspend_interrupt_data);
	}
}
