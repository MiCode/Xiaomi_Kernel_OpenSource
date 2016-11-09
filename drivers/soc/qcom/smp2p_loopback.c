/* drivers/soc/qcom/smp2p_loopback.c
 *
 * Copyright (c) 2013-2014,2016 The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/termios.h>
#include <linux/module.h>
#include <linux/remote_spinlock.h>
#include "smem_private.h"
#include "smp2p_private.h"

/**
 * struct smp2p_loopback_ctx - Representation of remote loopback object.
 *
 * @proc_id: Processor id of the processor that sends the loopback commands.
 * @out: Handle to the  smem entry structure for providing the response.
 * @out_nb: Notifies the opening of local entry.
 * @out_is_active: Outbound entry events should be processed.
 * @in_nb: Notifies changes in the remote entry.
 * @in_is_active: Inbound entry events should be processed.
 * @rmt_lpb_work: Work item that handles the incoming loopback commands.
 * @rmt_cmd: Structure that holds the current and previous value of the entry.
 */
struct smp2p_loopback_ctx {
	int proc_id;
	struct msm_smp2p_out *out;
	struct notifier_block out_nb;
	bool out_is_active;
	struct notifier_block in_nb;
	bool in_is_active;
	struct work_struct  rmt_lpb_work;
	struct msm_smp2p_update_notif rmt_cmd;
};

static struct smp2p_loopback_ctx  remote_loopback[SMP2P_NUM_PROCS];
static struct msm_smp2p_remote_mock remote_mock;

/**
 * remote_spinlock_test - Handles remote spinlock test.
 *
 * @ctx: Loopback context
 */
static void remote_spinlock_test(struct smp2p_loopback_ctx *ctx)
{
	uint32_t test_request;
	uint32_t test_response;
	unsigned long flags;
	int n;
	unsigned int lock_count = 0;
	remote_spinlock_t *smem_spinlock;

	test_request = 0x0;
	SMP2P_SET_RMT_CMD_TYPE_REQ(test_request);
	smem_spinlock = smem_get_remote_spinlock();
	if (!smem_spinlock) {
		pr_err("%s: unable to get remote spinlock\n", __func__);
		return;
	}

	for (;;) {
		remote_spin_lock_irqsave(smem_spinlock, flags);
		++lock_count;
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_LOCKED);
		(void)msm_smp2p_out_write(ctx->out, test_request);

		for (n = 0; n < 10000; ++n) {
			(void)msm_smp2p_in_read(ctx->proc_id,
					"smp2p", &test_response);
			test_response = SMP2P_GET_RMT_CMD(test_response);

			if (test_response == SMP2P_LB_CMD_RSPIN_END)
				break;

			if (test_response != SMP2P_LB_CMD_RSPIN_UNLOCKED)
				SMP2P_ERR("%s: invalid spinlock command %x\n",
					__func__, test_response);
		}

		if (test_response == SMP2P_LB_CMD_RSPIN_END) {
			SMP2P_SET_RMT_CMD_TYPE_RESP(test_request);
			SMP2P_SET_RMT_CMD(test_request,
					SMP2P_LB_CMD_RSPIN_END);
			SMP2P_SET_RMT_DATA(test_request, lock_count);
			(void)msm_smp2p_out_write(ctx->out, test_request);
			break;
		}

		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_UNLOCKED);
		(void)msm_smp2p_out_write(ctx->out, test_request);
		remote_spin_unlock_irqrestore(smem_spinlock, flags);
	}
	remote_spin_unlock_irqrestore(smem_spinlock, flags);
}

/**
 * smp2p_rmt_lpb_worker - Handles incoming remote loopback commands.
 *
 * @work: Work Item scheduled to handle the incoming commands.
 */
static void smp2p_rmt_lpb_worker(struct work_struct *work)
{
	struct smp2p_loopback_ctx *ctx;
	int lpb_cmd;
	int lpb_cmd_type;
	int lpb_data;

	ctx = container_of(work, struct smp2p_loopback_ctx, rmt_lpb_work);

	if (!ctx->in_is_active || !ctx->out_is_active)
		return;

	if (ctx->rmt_cmd.previous_value == ctx->rmt_cmd.current_value)
		return;

	lpb_cmd_type =  SMP2P_GET_RMT_CMD_TYPE(ctx->rmt_cmd.current_value);
	lpb_cmd = SMP2P_GET_RMT_CMD(ctx->rmt_cmd.current_value);
	lpb_data = SMP2P_GET_RMT_DATA(ctx->rmt_cmd.current_value);

	if (lpb_cmd & SMP2P_RLPB_IGNORE)
		return;

	switch (lpb_cmd) {
	case SMP2P_LB_CMD_NOOP:
		/* Do nothing */
		break;

	case SMP2P_LB_CMD_ECHO:
		SMP2P_SET_RMT_CMD_TYPE(ctx->rmt_cmd.current_value, 0);
		SMP2P_SET_RMT_DATA(ctx->rmt_cmd.current_value,
							lpb_data);
		(void)msm_smp2p_out_write(ctx->out,
					ctx->rmt_cmd.current_value);
		break;

	case SMP2P_LB_CMD_CLEARALL:
		ctx->rmt_cmd.current_value = 0;
		(void)msm_smp2p_out_write(ctx->out,
					ctx->rmt_cmd.current_value);
		break;

	case SMP2P_LB_CMD_PINGPONG:
		SMP2P_SET_RMT_CMD_TYPE(ctx->rmt_cmd.current_value, 0);
		if (lpb_data) {
			lpb_data--;
			SMP2P_SET_RMT_DATA(ctx->rmt_cmd.current_value,
					lpb_data);
			(void)msm_smp2p_out_write(ctx->out,
					ctx->rmt_cmd.current_value);
		}
		break;

	case SMP2P_LB_CMD_RSPIN_START:
		remote_spinlock_test(ctx);
		break;

	case SMP2P_LB_CMD_RSPIN_LOCKED:
	case SMP2P_LB_CMD_RSPIN_UNLOCKED:
	case SMP2P_LB_CMD_RSPIN_END:
		/* not used for remote spinlock test */
		break;

	default:
		SMP2P_DBG("%s: Unknown loopback command %x\n",
				__func__, lpb_cmd);
		break;
	}
}

/**
 * smp2p_rmt_in_edge_notify -  Schedules a work item to handle the commands.
 *
 * @nb: Notifier block, this is called when the value in remote entry changes.
 * @event: Takes value SMP2P_ENTRY_UPDATE or SMP2P_OPEN based on the event.
 * @data: Consists of previous and current value in case of entry update.
 * @returns: 0 for success (return value required for notifier chains).
 */
static int smp2p_rmt_in_edge_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct smp2p_loopback_ctx *ctx;

	if (!(event == SMP2P_ENTRY_UPDATE || event == SMP2P_OPEN))
		return 0;

	ctx = container_of(nb, struct smp2p_loopback_ctx, in_nb);
	if (data && ctx->in_is_active) {
		ctx->rmt_cmd = *(struct msm_smp2p_update_notif *)data;
		schedule_work(&ctx->rmt_lpb_work);
	}

	return 0;
}

/**
 * smp2p_rmt_out_edge_notify - Notifies on the opening of the outbound entry.
 *
 * @nb: Notifier block, this is called when the local entry is open.
 * @event: Takes on value SMP2P_OPEN when the local entry is open.
 * @data: Consist of current value of the remote entry, if entry is open.
 * @returns: 0 for success (return value required for notifier chains).
 */
static int smp2p_rmt_out_edge_notify(struct notifier_block  *nb,
				unsigned long event, void *data)
{
	struct smp2p_loopback_ctx *ctx;

	ctx = container_of(nb, struct smp2p_loopback_ctx, out_nb);
	if (event == SMP2P_OPEN)
		SMP2P_DBG("%s: 'smp2p':%d opened\n", __func__,
				ctx->proc_id);

	return 0;
}

/**
 * msm_smp2p_init_rmt_lpb -  Initializes the remote loopback object.
 *
 * @ctx: Pointer to remote loopback object that needs to be initialized.
 * @pid: Processor id  of the processor that is sending the commands.
 * @entry: Name of the entry that needs to be opened locally.
 * @returns: 0 on success, standard Linux error code otherwise.
 */
static int msm_smp2p_init_rmt_lpb(struct  smp2p_loopback_ctx *ctx,
			int pid, const char *entry)
{
	int ret = 0;
	int tmp;

	if (!ctx || !entry || pid > SMP2P_NUM_PROCS)
		return -EINVAL;

	ctx->in_nb.notifier_call = smp2p_rmt_in_edge_notify;
	ctx->out_nb.notifier_call = smp2p_rmt_out_edge_notify;
	ctx->proc_id = pid;
	ctx->in_is_active = true;
	ctx->out_is_active = true;
	tmp = msm_smp2p_out_open(pid, entry, &ctx->out_nb,
						&ctx->out);
	if (tmp) {
		SMP2P_ERR("%s: open failed outbound entry '%s':%d - ret %d\n",
				__func__, entry, pid, tmp);
		ret = tmp;
	}

	tmp = msm_smp2p_in_register(ctx->proc_id,
				SMP2P_RLPB_ENTRY_NAME,
				&ctx->in_nb);
	if (tmp) {
		SMP2P_ERR("%s: unable to open inbound entry '%s':%d - ret %d\n",
				__func__, entry, pid, tmp);
		ret = tmp;
	}

	return ret;
}

/**
 * msm_smp2p_init_rmt_lpb_proc - Wrapper over msm_smp2p_init_rmt_lpb
 *
 * @remote_pid: Processor ID of the processor that sends loopback command.
 * @returns: Pointer to outbound entry handle.
 */
void *msm_smp2p_init_rmt_lpb_proc(int remote_pid)
{
	int tmp;
	void *ret = NULL;

	tmp = msm_smp2p_init_rmt_lpb(&remote_loopback[remote_pid],
			remote_pid, SMP2P_RLPB_ENTRY_NAME);
	if (!tmp)
		ret = remote_loopback[remote_pid].out;

	return ret;
}
EXPORT_SYMBOL(msm_smp2p_init_rmt_lpb_proc);

/**
 * msm_smp2p_deinit_rmt_lpb_proc - Unregister support for remote processor.
 *
 * @remote_pid:  Processor ID of the remote system.
 * @returns: 0 on success, standard Linux error code otherwise.
 *
 * Unregister loopback support for remote processor.
 */
int msm_smp2p_deinit_rmt_lpb_proc(int remote_pid)
{
	int ret = 0;
	int tmp;
	struct smp2p_loopback_ctx *ctx;

	if (remote_pid >= SMP2P_NUM_PROCS)
		return -EINVAL;

	ctx = &remote_loopback[remote_pid];

	/* abort any pending notifications */
	remote_loopback[remote_pid].out_is_active = false;
	remote_loopback[remote_pid].in_is_active = false;
	flush_work(&ctx->rmt_lpb_work);

	/* unregister entries */
	tmp = msm_smp2p_out_close(&remote_loopback[remote_pid].out);
	remote_loopback[remote_pid].out = NULL;
	if (tmp) {
		SMP2P_ERR("%s: outbound 'smp2p':%d close failed %d\n",
				__func__, remote_pid, tmp);
		ret = tmp;
	}

	tmp = msm_smp2p_in_unregister(remote_pid,
		SMP2P_RLPB_ENTRY_NAME, &remote_loopback[remote_pid].in_nb);
	if (tmp) {
		SMP2P_ERR("%s: inbound 'smp2p':%d close failed %d\n",
				__func__, remote_pid, tmp);
		ret = tmp;
	}

	return ret;
}
EXPORT_SYMBOL(msm_smp2p_deinit_rmt_lpb_proc);

/**
 * msm_smp2p_set_remote_mock_exists - Sets the remote mock configuration.
 *
 * @item_exists: true = Remote mock SMEM item exists
 *
 * This is used in the testing environment to simulate the existence of the
 * remote smem item in order to test the negotiation algorithm.
 */
void msm_smp2p_set_remote_mock_exists(bool item_exists)
{
	remote_mock.item_exists = item_exists;
}
EXPORT_SYMBOL(msm_smp2p_set_remote_mock_exists);

/**
 * msm_smp2p_get_remote_mock - Get remote mock object.
 *
 * @returns: Point to the remote mock object.
 */
void *msm_smp2p_get_remote_mock(void)
{
	return &remote_mock;
}
EXPORT_SYMBOL(msm_smp2p_get_remote_mock);

/**
 * msm_smp2p_get_remote_mock_smem_item - Returns a pointer to remote item.
 *
 * @size:    Size of item.
 * @returns: Pointer to mock remote smem item.
 */
void *msm_smp2p_get_remote_mock_smem_item(uint32_t *size)
{
	void *ptr = NULL;

	if (remote_mock.item_exists) {
		*size = sizeof(remote_mock.remote_item);
		ptr = &(remote_mock.remote_item);
	}

	return ptr;
}
EXPORT_SYMBOL(msm_smp2p_get_remote_mock_smem_item);

/**
 * smp2p_remote_mock_rx_interrupt - Triggers receive interrupt for mock proc.
 *
 * @returns: 0 for success
 *
 * This function simulates the receiving of interrupt by the mock remote
 * processor in a testing environment.
 */
int smp2p_remote_mock_rx_interrupt(void)
{
	remote_mock.rx_interrupt_count++;
	if (remote_mock.initialized)
		complete(&remote_mock.cb_completion);
	return 0;
}
EXPORT_SYMBOL(smp2p_remote_mock_rx_interrupt);

/**
 * smp2p_remote_mock_tx_interrupt - Calls the SMP2P interrupt handler.
 *
 * This function calls the interrupt handler of the Apps processor to simulate
 * receiving interrupts from a remote processor.
 */
static void smp2p_remote_mock_tx_interrupt(void)
{
	msm_smp2p_interrupt_handler(SMP2P_REMOTE_MOCK_PROC);
}

/**
 * smp2p_remote_mock_init - Initialize the remote mock and loopback objects.
 *
 * @returns: 0 for success
 */
static int __init smp2p_remote_mock_init(void)
{
	int i;
	struct smp2p_interrupt_config *int_cfg;

	smp2p_init_header(&remote_mock.remote_item.header,
			SMP2P_REMOTE_MOCK_PROC, SMP2P_APPS_PROC,
			0, 0);
	remote_mock.rx_interrupt_count = 0;
	remote_mock.rx_interrupt = smp2p_remote_mock_rx_interrupt;
	remote_mock.tx_interrupt = smp2p_remote_mock_tx_interrupt;
	remote_mock.item_exists = false;
	init_completion(&remote_mock.cb_completion);
	remote_mock.initialized = true;

	for (i = 0; i < SMP2P_NUM_PROCS; i++) {
		INIT_WORK(&(remote_loopback[i].rmt_lpb_work),
				smp2p_rmt_lpb_worker);
		if (i == SMP2P_REMOTE_MOCK_PROC)
			/* do not register loopback for remote mock proc */
			continue;

		int_cfg = smp2p_get_interrupt_config();
		if (!int_cfg) {
			SMP2P_ERR("Remote processor config unavailable\n");
			return 0;
		}
		if (!int_cfg[i].is_configured)
			continue;

		msm_smp2p_init_rmt_lpb(&remote_loopback[i],
			i, SMP2P_RLPB_ENTRY_NAME);
	}
	return 0;
}
module_init(smp2p_remote_mock_init);
