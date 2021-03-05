// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/*
 * Author: Xiao Wang <xiao.wang@mediatek.com>
 */
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include "ccci_fsm_internal.h"
#include "ccci_platform.h"

static struct ccci_fsm_ctl *ccci_fsm_entries[MAX_MD_NUM];

static void fsm_finish_command(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd, int result);
static void fsm_finish_event(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_event *event);

static int needforcestop;


int force_md_stop(struct ccci_fsm_monitor *monitor_ctl)
{
	int ret = -1;
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(monitor_ctl->md_id);

	if (!ctl) {
		CCCI_ERROR_LOG(-1, FSM,
			"%s:fsm_get_entity_by_md_id fail\n",
			__func__);
		return -1;
	}
	needforcestop = 1;
	ret = fsm_append_command(ctl, CCCI_COMMAND_STOP, 0);
	CCCI_NORMAL_LOG(monitor_ctl->md_id, FSM,
			"force md stop\n");
	return ret;
}

unsigned long __weak BAT_Get_Battery_Voltage(int polling_mode)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

void mdee_set_ex_time_str(unsigned char md_id, unsigned int type, char *str)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);

	if (!ctl) {
		CCCI_ERROR_LOG(-1, FSM,
			"%s:fsm_get_entity_by_md_id fail\n",
			__func__);
		return;
	}
	mdee_set_ex_start_str(&ctl->ee_ctl, type, str);
}

static struct ccci_fsm_command *fsm_check_for_ee(struct ccci_fsm_ctl *ctl,
	int xip)
{
	struct ccci_fsm_command *cmd, *next = NULL;
	unsigned long flags;

	spin_lock_irqsave(&ctl->command_lock, flags);
	if (!list_empty(&ctl->command_queue)) {
		cmd = list_first_entry(&ctl->command_queue,
			struct ccci_fsm_command, entry);
		if (cmd->cmd_id == CCCI_COMMAND_EE) {
			if (xip)
				list_del(&cmd->entry);
			next = cmd;
		}
	}
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	return next;
}

static inline int fsm_broadcast_state(struct ccci_fsm_ctl *ctl,
	enum MD_STATE state)
{
	if (unlikely(ctl->md_state != BOOT_WAITING_FOR_HS2 && state == READY)) {
		CCCI_NORMAL_LOG(ctl->md_id, FSM,
		"ignore HS2 when md_state=%d\n",
		ctl->md_state);
		return 0;
	}

	CCCI_NORMAL_LOG(ctl->md_id, FSM,
	"md_state change from %d to %d\n",
	ctl->md_state, state);
	ctl->md_state = state;

	/* update to port first,
	 * otherwise send message on HS2 may fail
	 */
	ccci_port_md_status_notify(ctl->md_id, state);
	ccci_hif_state_notification(ctl->md_id, state);
#ifdef FEATURE_SCP_CCCI_SUPPORT
	schedule_work(&ctl->scp_ctl.scp_md_state_sync_work);
#endif
	return 0;
}

static void fsm_routine_zombie(struct ccci_fsm_ctl *ctl)
{
	struct ccci_fsm_event *event, *evt_next;
	struct ccci_fsm_command *cmd, *cmd_next;
	unsigned long flags;

	CCCI_ERROR_LOG(ctl->md_id, FSM,
		"unexpected FSM state %d->%d, from %ps\n",
		ctl->last_state, ctl->curr_state,
		__builtin_return_address(0));
	spin_lock_irqsave(&ctl->command_lock, flags);
	list_for_each_entry_safe(cmd,
		cmd_next, &ctl->command_queue, entry) {
		CCCI_ERROR_LOG(ctl->md_id, FSM,
		"unhandled command %d\n", cmd->cmd_id);
		list_del(&cmd->entry);
		fsm_finish_command(ctl, cmd, -1);
	}
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event,
		evt_next, &ctl->event_queue, entry) {
		CCCI_ERROR_LOG(ctl->md_id, FSM,
		"unhandled event %d\n", event->event_id);
		fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);

}

/* cmd is not NULL only when reason is ordinary EE */
static void fsm_routine_exception(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd, enum CCCI_EE_REASON reason)
{
	int count = 0, ex_got = 0;
	int rec_ok_got = 0, pass_got = 0;
	struct ccci_fsm_event *event;
	unsigned long flags;

	CCCI_NORMAL_LOG(ctl->md_id, FSM,
		"exception %d, from %ps\n",
		reason, __builtin_return_address(0));
	fsm_monitor_send_message(ctl->md_id,
		CCCI_MD_MSG_EXCEPTION, 0);
	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_GATED) {
		if (cmd)
			fsm_finish_command(ctl, cmd, -1);
		fsm_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_EXCEPTION;
	if (reason == EXCEPTION_WDT
		|| reason == EXCEPTION_HS1_TIMEOUT
		|| reason == EXCEPTION_HS2_TIMEOUT)
		mdee_set_ex_start_str(&ctl->ee_ctl, 0, NULL);

	/* 2. check EE reason */
	switch (reason) {
	case EXCEPTION_HS1_TIMEOUT:
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"MD_BOOT_HS1_FAIL!\n");
		fsm_md_bootup_timeout_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_HS2_TIMEOUT:
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"MD_BOOT_HS2_FAIL!\n");
		fsm_md_bootup_timeout_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_MD_NO_RESPONSE:
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"MD_NO_RESPONSE!\n");
		fsm_broadcast_state(ctl, EXCEPTION);
		fsm_md_no_response_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_WDT:
		fsm_broadcast_state(ctl, EXCEPTION);
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"MD_WDT!\n");
		fsm_md_wdt_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_EE:
		fsm_broadcast_state(ctl, EXCEPTION);
		/* no need to implement another
		 * event polling in EE_CTRL,
		 * so we do it here
		 */
		ccci_md_exception_handshake(ctl->md_id,
			MD_EX_CCIF_TIMEOUT);
		count = 0;
		while (count < MD_EX_REC_OK_TIMEOUT/EVENT_POLL_INTEVAL) {
			spin_lock_irqsave(&ctl->event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue,
					struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX) {
					ex_got = 1;
					fsm_finish_event(ctl, event);
				} else if (event->event_id ==
						CCCI_EVENT_MD_EX_REC_OK) {
					rec_ok_got = 1;
					fsm_finish_event(ctl, event);
				}
			}
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			if (rec_ok_got)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		fsm_md_exception_stage(&ctl->ee_ctl, 1);
		count = 0;
		while (count < MD_EX_PASS_TIMEOUT/EVENT_POLL_INTEVAL) {
			spin_lock_irqsave(&ctl->event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue,
					struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX_PASS) {
					pass_got = 1;
					fsm_finish_event(ctl, event);
				}
			}
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			if (pass_got)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		fsm_md_exception_stage(&ctl->ee_ctl, 2);
		/*wait until modem memory dump done*/
		fsm_check_ee_done(&ctl->ee_ctl, EE_DONE_TIMEOUT);
		break;
	default:
		break;
	}
	/* 3. always end in exception state */
	if (cmd)
		fsm_finish_command(ctl, cmd, 1);
}

static void fsm_routine_start(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd)
{
	int ret;
	int count = 0, user_exit = 0, hs1_got = 0, hs2_got = 0;
	struct ccci_fsm_event *event, *next;
	unsigned long flags;

	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_READY)
		goto success;
	if (ctl->curr_state != CCCI_FSM_GATED) {
		fsm_finish_command(ctl, cmd, -1);
		fsm_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STARTING;
	__pm_stay_awake(ctl->wakelock);
	/* 2. poll for critical users exit */
	while (count < BOOT_TIMEOUT/EVENT_POLL_INTEVAL && !needforcestop) {
		if ((ccci_port_check_critical_user(ctl->md_id) == 0) ||
			ccci_port_critical_user_only_fsd(ctl->md_id)) {
			user_exit = 1;
			break;
		}
		count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	/* what if critical user still alive:
	 * we can't wait for ever since this may be
	 * an illegal sequence (enter flight mode -> force start),
	 * and we must be able to recover from it.
	 * we'd better not entering exception state as
	 * start operation is not allowed in exception state.
	 * so we tango on...
	 */
	if (!user_exit)
		CCCI_ERROR_LOG(ctl->md_id, FSM, "critical user alive %d\n",
			ccci_port_check_critical_user(ctl->md_id));
	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, next, &ctl->event_queue, entry) {
		CCCI_NORMAL_LOG(ctl->md_id, FSM,
			"drop event %d before start\n", event->event_id);
		fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	/* 3. action and poll event queue */
	ccci_md_pre_start(ctl->md_id);
	fsm_broadcast_state(ctl, BOOT_WAITING_FOR_HS1);
	ret = ccci_md_start(ctl->md_id);
	if (ret)
		goto fail;
	ctl->boot_count++;
	count = 0;
	while (count < BOOT_TIMEOUT/EVENT_POLL_INTEVAL) {
		spin_lock_irqsave(&ctl->event_lock, flags);
		if (!list_empty(&ctl->event_queue)) {
			event = list_first_entry(&ctl->event_queue,
						struct ccci_fsm_event, entry);
			if (event->event_id == CCCI_EVENT_HS1) {
				hs1_got = 1;
				fsm_broadcast_state(ctl, BOOT_WAITING_FOR_HS2);

				if (event->length
					== sizeof(struct md_query_ap_feature)
					+ sizeof(struct ccci_header))
					ccci_md_prepare_runtime_data(ctl->md_id,
						event->data, event->length);
				else if (event->length
						== sizeof(struct ccci_header))
					CCCI_NORMAL_LOG(ctl->md_id, FSM,
						"old handshake1 message\n");
				else
					CCCI_ERROR_LOG(ctl->md_id, FSM,
						"invalid MD_QUERY_MSG %d\n",
						event->length);
#ifdef SET_EMI_STEP_BY_STAGE
				ccci_set_mem_access_protection_second_stage(
					ctl->md_id);
#endif
				ccci_md_dump_info(ctl->md_id,
					DUMP_MD_BOOTUP_STATUS, NULL, 0);
				fsm_finish_event(ctl, event);

				spin_unlock_irqrestore(&ctl->event_lock, flags);
				/* this API would alloc skb */
				ret = ccci_md_send_runtime_data(ctl->md_id);
				CCCI_NORMAL_LOG(ctl->md_id, FSM,
					"send runtime data %d\n", ret);
				spin_lock_irqsave(&ctl->event_lock, flags);
			} else if (event->event_id == CCCI_EVENT_HS2) {
				hs2_got = 1;

				fsm_broadcast_state(ctl, READY);

				fsm_finish_event(ctl, event);
			}
		}
		spin_unlock_irqrestore(&ctl->event_lock, flags);
		if (fsm_check_for_ee(ctl, 0)) {
			CCCI_ERROR_LOG(ctl->md_id, FSM,
				"early exception detected\n");
			goto fail_ee;
		}
		if (hs2_got)
			goto success;
		/* defeatured for now, just enlarge BOOT_TIMEOUT */
		if (atomic_read(&ctl->fs_ongoing))
			count = 0;
		else
			count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	/* 4. check result, finish command */
fail:
	if (hs1_got)
		fsm_routine_exception(ctl, NULL, EXCEPTION_HS2_TIMEOUT);
	else
		fsm_routine_exception(ctl, NULL, EXCEPTION_HS1_TIMEOUT);
	fsm_finish_command(ctl, cmd, -1);
	__pm_relax(ctl->wakelock);
	return;

fail_ee:
	/* exit imediately,
	 * let md_init have chance to start MD logger service
	 */
	fsm_finish_command(ctl, cmd, -1);
	__pm_relax(ctl->wakelock);
	return;

success:
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_READY;
	ccci_md_post_start(ctl->md_id);
	fsm_finish_command(ctl, cmd, 1);
	__pm_relax(ctl->wakelock);
	__pm_wakeup_event(ctl->wakelock, jiffies_to_msecs(10 * HZ));
}

static void fsm_routine_stop(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd)
{
	struct ccci_fsm_event *event, *next;
	struct ccci_fsm_command *ee_cmd = NULL;
	unsigned long flags;

	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_GATED)
		goto success;
	if (ctl->curr_state != CCCI_FSM_READY
			&& ctl->curr_state != CCCI_FSM_EXCEPTION) {
		fsm_finish_command(ctl, cmd, -1);
		fsm_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STOPPING;
	/* 2. pre-stop: polling MD for infinit sleep mode */
	ccci_md_pre_stop(ctl->md_id,
	cmd->flag & FSM_CMD_FLAG_FLIGHT_MODE
	?
	MD_FLIGHT_MODE_ENTER
	:
	MD_FLIGHT_MODE_NONE);
	/* 3. check for EE */
	ee_cmd = fsm_check_for_ee(ctl, 1);
	if (ee_cmd) {
		fsm_routine_exception(ctl, ee_cmd, EXCEPTION_EE);
		fsm_check_ee_done(&ctl->ee_ctl, EE_DONE_TIMEOUT);
	}
	/* to block port's write operation, must after EE flow done */
	fsm_broadcast_state(ctl, WAITING_TO_STOP);
	/*reset fsm poller*/
	ctl->poller_ctl.poller_state = FSM_POLLER_RECEIVED_RESPONSE;
	wake_up(&ctl->poller_ctl.status_rx_wq);
	/* 4. hardware stop */
	ccci_md_stop(ctl->md_id,
	cmd->flag & FSM_CMD_FLAG_FLIGHT_MODE
	?
	MD_FLIGHT_MODE_ENTER
	:
	MD_FLIGHT_MODE_NONE);
	/* 5. clear event queue */
	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, next,
		&ctl->event_queue, entry) {
		CCCI_NORMAL_LOG(ctl->md_id, FSM,
			"drop event %d after stop\n",
			event->event_id);
		fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	/* 6. always end in stopped state */
success:
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_GATED;
	fsm_broadcast_state(ctl, GATED);
	fsm_finish_command(ctl, cmd, 1);
}

static void fsm_routine_wdt(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd)
{
	int reset_md = 0;
	int is_epon_set = 0;
	struct device_node *node;
	unsigned int offset_apon_md1 = 0;
	struct ccci_smem_region *mdss_dbg
		= ccci_md_get_smem_by_user_id(ctl->md_id,
			SMEM_USER_RAW_MDSS_DBG);
	int ret;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (node) {
		ret = of_property_read_u32(node,
				"mediatek,offset_apon_md1", &offset_apon_md1);
		if (ret < 0)
			CCCI_ERROR_LOG(ctl->md_id, FSM,
				"[%s] not found: mediatek,offset_apon_md1\n",
				__func__);

	} else
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"[%s] error: not found the mediatek,mddriver\n",
			__func__);

	if (ctl->md_id == MD_SYS1)
		is_epon_set =
			*((int *)(mdss_dbg->base_ap_view_vir
				+ offset_apon_md1)) == 0xBAEBAE10;
	else if (ctl->md_id == MD_SYS3)
		is_epon_set = *((int *)(mdss_dbg->base_ap_view_vir
			+ CCCI_EE_OFFSET_EPON_MD3))
				== 0xBAEBAE10;

	if (is_epon_set) {
		CCCI_NORMAL_LOG(ctl->md_id, FSM,
			"reset MD after WDT\n");
		reset_md = 1;
	} else {
		if (ccci_port_get_critical_user(ctl->md_id,
				CRIT_USR_MDLOG) == 0) {
			CCCI_NORMAL_LOG(ctl->md_id, FSM,
				"mdlogger closed, reset MD after WDT\n");
			reset_md = 1;
		} else {
			fsm_routine_exception(ctl, NULL, EXCEPTION_WDT);
		}
	}
	if (reset_md) {
		fsm_monitor_send_message(ctl->md_id,
			CCCI_MD_MSG_RESET_REQUEST, 0);
		fsm_monitor_send_message(GET_OTHER_MD_ID(ctl->md_id),
			CCCI_MD_MSG_RESET_REQUEST, 0);
	}
	fsm_finish_command(ctl, cmd, 1);
}

static int fsm_main_thread(void *data)
{
	struct ccci_fsm_ctl *ctl = (struct ccci_fsm_ctl *)data;
	struct ccci_fsm_command *cmd = NULL;
	unsigned long flags;

	while (1) {
		wait_event(ctl->command_wq,
			!list_empty(&ctl->command_queue));
		spin_lock_irqsave(&ctl->command_lock, flags);
		cmd = list_first_entry(&ctl->command_queue,
			struct ccci_fsm_command, entry);
		/* delete first, otherwise hard to peek
		 * next command in routines
		 */
		list_del(&cmd->entry);
		spin_unlock_irqrestore(&ctl->command_lock, flags);

		CCCI_NORMAL_LOG(ctl->md_id, FSM,
			"command %d process\n", cmd->cmd_id);
		switch (cmd->cmd_id) {
		case CCCI_COMMAND_START:
			fsm_routine_start(ctl, cmd);
			break;
		case CCCI_COMMAND_STOP:
			fsm_routine_stop(ctl, cmd);
			break;
		case CCCI_COMMAND_WDT:
			fsm_routine_wdt(ctl, cmd);
			break;
		case CCCI_COMMAND_EE:
			fsm_routine_exception(ctl, cmd, EXCEPTION_EE);
			break;
		case CCCI_COMMAND_MD_HANG:
			fsm_routine_exception(ctl, cmd,
				EXCEPTION_MD_NO_RESPONSE);
			break;
		default:
			fsm_finish_command(ctl, cmd, -1);
			fsm_routine_zombie(ctl);
			break;
		};
	}
	return 0;
}


int fsm_append_command(struct ccci_fsm_ctl *ctl,
	enum CCCI_FSM_COMMAND cmd_id, unsigned int flag)
{
	struct ccci_fsm_command *cmd = NULL;
	int result = 0;
	unsigned long flags;

	if (cmd_id <= CCCI_COMMAND_INVALID
			|| cmd_id >= CCCI_COMMAND_MAX) {
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"invalid command %d\n", cmd_id);
		return -CCCI_ERR_INVALID_PARAM;
	}
	cmd = kmalloc(sizeof(struct ccci_fsm_command),
		(in_irq() || in_softirq()
		|| irqs_disabled()) ? GFP_ATOMIC : GFP_KERNEL);
	if (!cmd) {
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"fail to alloc command %d\n", cmd_id);
		return -CCCI_ERR_GET_MEM_FAIL;
	}
	INIT_LIST_HEAD(&cmd->entry);
	init_waitqueue_head(&cmd->complete_wq);
	cmd->cmd_id = cmd_id;
	cmd->complete = 0;
	if (in_irq() || irqs_disabled())
		flag &= ~FSM_CMD_FLAG_WAIT_FOR_COMPLETE;
	cmd->flag = flag;

	spin_lock_irqsave(&ctl->command_lock, flags);
	list_add_tail(&cmd->entry, &ctl->command_queue);
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	CCCI_NORMAL_LOG(ctl->md_id, FSM,
		"command %d is appended %x from %ps\n",
		cmd_id, flag,
		__builtin_return_address(0));
	/* after this line, only dereference cmd
	 * when "wait-for-complete"
	 */
	wake_up(&ctl->command_wq);
	if (flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETE) {
		wait_event(cmd->complete_wq, cmd->complete != 0);
		if (cmd->complete != 1)
			result = -1;
		spin_lock_irqsave(&ctl->cmd_complete_lock, flags);
		kfree(cmd);
		spin_unlock_irqrestore(&ctl->cmd_complete_lock, flags);
	}
	return result;
}

static void fsm_finish_command(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd, int result)
{
	unsigned long flags;

	CCCI_NORMAL_LOG(ctl->md_id, FSM,
		"command %d is completed %d by %ps\n",
		cmd->cmd_id, result,
		__builtin_return_address(0));
	if (cmd->flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETE) {
		spin_lock_irqsave(&ctl->cmd_complete_lock, flags);
		cmd->complete = result;
		/* do not dereference cmd after this line */
		wake_up_all(&cmd->complete_wq);
		/* after cmd in list,
		 * processing thread may see it
		 * without being waked up,
		 * so spinlock is needed
		 */
		spin_unlock_irqrestore(&ctl->cmd_complete_lock, flags);
	} else {
		/* no one is waiting for this cmd, free to free */
		kfree(cmd);
	}
}

int fsm_append_event(struct ccci_fsm_ctl *ctl, enum CCCI_FSM_EVENT event_id,
	unsigned char *data, unsigned int length)
{
	struct ccci_fsm_event *event = NULL;
	unsigned long flags;

	if (event_id <= CCCI_EVENT_INVALID || event_id >= CCCI_EVENT_MAX) {
		CCCI_ERROR_LOG(ctl->md_id, FSM, "invalid event %d\n", event_id);
		return -CCCI_ERR_INVALID_PARAM;
	}
	if (event_id == CCCI_EVENT_FS_IN) {
		atomic_set(&(ctl->fs_ongoing), 1);
		return 0;
	} else if (event_id == CCCI_EVENT_FS_OUT) {
		atomic_set(&(ctl->fs_ongoing), 0);
		return 0;
	}
	event = kmalloc(sizeof(struct ccci_fsm_event) + length,
		in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
	if (!event) {
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"fail to alloc event%d\n", event_id);
		return -CCCI_ERR_GET_MEM_FAIL;
	}
	INIT_LIST_HEAD(&event->entry);
	event->event_id = event_id;
	event->length = length;
	if (data && length)
		memcpy(event->data, data, length);

	spin_lock_irqsave(&ctl->event_lock, flags);
	list_add_tail(&event->entry, &ctl->event_queue);
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	/* do not derefence event after here */
	CCCI_NORMAL_LOG(ctl->md_id, FSM,
		"event %d is appended from %ps\n", event_id,
		__builtin_return_address(0));
	return 0;
}

/* must be called within protection of event_lock */
static void fsm_finish_event(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_event *event)
{
	list_del(&event->entry);
	CCCI_NORMAL_LOG(ctl->md_id, FSM,
		"event %d is completed by %ps\n", event->event_id,
		__builtin_return_address(0));
	kfree(event);
}

struct ccci_fsm_ctl *fsm_get_entity_by_device_number(dev_t dev_n)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ccci_fsm_entries); i++) {
		if (ccci_fsm_entries[i]
			&& ccci_fsm_entries[i]->monitor_ctl.dev_n
			== dev_n)
			return ccci_fsm_entries[i];
	}
	return NULL;
}

struct ccci_fsm_ctl *fsm_get_entity_by_md_id(int md_id)
{

	int i;

	for (i = 0; i < ARRAY_SIZE(ccci_fsm_entries); i++) {
		if (ccci_fsm_entries[i]
			&& ccci_fsm_entries[i]->md_id == md_id)
			return ccci_fsm_entries[i];
	}
	return NULL;
}

int ccci_fsm_init(int md_id)
{
	struct ccci_fsm_ctl *ctl;
	int ret = 0;

	if (md_id < 0 || md_id >= ARRAY_SIZE(ccci_fsm_entries))
		return -CCCI_ERR_INVALID_PARAM;

	ctl = kzalloc(sizeof(struct ccci_fsm_ctl), GFP_KERNEL);
	ctl->md_id = md_id;
	ctl->last_state = CCCI_FSM_INVALID;
	ctl->curr_state = CCCI_FSM_GATED;
	INIT_LIST_HEAD(&ctl->command_queue);
	INIT_LIST_HEAD(&ctl->event_queue);
	init_waitqueue_head(&ctl->command_wq);
	spin_lock_init(&ctl->event_lock);
	spin_lock_init(&ctl->command_lock);
	spin_lock_init(&ctl->cmd_complete_lock);
	atomic_set(&ctl->fs_ongoing, 0);
	ret = snprintf(ctl->wakelock_name, sizeof(ctl->wakelock_name),
		"md%d_wakelock", ctl->md_id + 1);
	if (ret < 0 || ret >= sizeof(ctl->wakelock_name)) {
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"%s-%d:snprintf fail,ret=%d\n", __func__, __LINE__, ret);
		return -1;
	}
	ctl->wakelock = wakeup_source_register(NULL, ctl->wakelock_name);
	if (!ctl->wakelock) {
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}
	ctl->fsm_thread = kthread_run(fsm_main_thread, ctl,
		"ccci_fsm%d", md_id + 1);
#ifdef FEATURE_SCP_CCCI_SUPPORT
	fsm_scp_init(&ctl->scp_ctl);
#endif
	fsm_poller_init(&ctl->poller_ctl);
	fsm_ee_init(&ctl->ee_ctl);
	fsm_monitor_init(&ctl->monitor_ctl);
	fsm_sys_init();

	ccci_fsm_entries[md_id] = ctl;
	return 0;
}

enum MD_STATE ccci_fsm_get_md_state(int md_id)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);

	if (ctl)
		return ctl->md_state;
	else
		return INVALID;
}
EXPORT_SYMBOL(ccci_fsm_get_md_state);

enum MD_STATE_FOR_USER ccci_fsm_get_md_state_for_user(int md_id)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);

	if (!ctl)
		return MD_STATE_INVALID;

	switch (ctl->md_state) {
	case INVALID:
	case WAITING_TO_STOP:
	case GATED:
		return MD_STATE_INVALID;
	case RESET:
	case BOOT_WAITING_FOR_HS1:
	case BOOT_WAITING_FOR_HS2:
		return MD_STATE_BOOTING;
	case READY:
		return MD_STATE_READY;
	case EXCEPTION:
		return MD_STATE_EXCEPTION;
	default:
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"Invalid md_state %d\n", ctl->md_state);
		return MD_STATE_INVALID;
	}
}
EXPORT_SYMBOL(ccci_fsm_get_md_state_for_user);


int ccci_fsm_recv_md_interrupt(int md_id, enum MD_IRQ_TYPE type)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);

	if (!ctl)
		return -CCCI_ERR_INVALID_PARAM;

	__pm_wakeup_event(ctl->wakelock, jiffies_to_msecs(10 * HZ));

	if (type == MD_IRQ_WDT) {
		fsm_append_command(ctl, CCCI_COMMAND_WDT, 0);
	} else if (type == MD_IRQ_CCIF_EX) {
		fsm_md_exception_stage(&ctl->ee_ctl, 0);
		fsm_append_command(ctl, CCCI_COMMAND_EE, 0);
	}
	return 0;
}

int ccci_fsm_recv_control_packet(int md_id, struct sk_buff *skb)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	int ret = 0, free_skb = 1;
	struct c2k_ctrl_port_msg *c2k_ctl_msg;
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(md_id);

	if (!ctl)
		return -CCCI_ERR_INVALID_PARAM;

	CCCI_NORMAL_LOG(ctl->md_id, FSM,
		"control message 0x%X,0x%X\n",
		ccci_h->data[1], ccci_h->reserved);
	switch (ccci_h->data[1]) {
	case MD_INIT_START_BOOT: /* also MD_NORMAL_BOOT */
		if (ccci_h->reserved == MD_INIT_CHK_ID)
			fsm_append_event(ctl, CCCI_EVENT_HS1,
				skb->data, skb->len);
		else
			fsm_append_event(ctl, CCCI_EVENT_HS2, NULL, 0);
		break;
	case MD_EX:
	case MD_EX_REC_OK:
	case MD_EX_PASS:
	case CCCI_DRV_VER_ERROR:
		fsm_ee_message_handler(&ctl->ee_ctl, skb);
		break;

	case C2K_HB_MSG:
		free_skb = 0;
		ccci_fsm_recv_status_packet(ctl->md_id, skb);
		break;
	case C2K_STATUS_IND_MSG:
	case C2K_STATUS_QUERY_MSG:
		c2k_ctl_msg = (struct c2k_ctrl_port_msg *)&ccci_h->reserved;
		CCCI_NORMAL_LOG(ctl->md_id, FSM,
			"C2K line status %d: 0x%02x\n",
			ccci_h->data[1], c2k_ctl_msg->option);
		if (c2k_ctl_msg->option & 0x80)
			per_md_data->dtr_state = 1; /*connect */
		else
			per_md_data->dtr_state = 0; /*disconnect */
		break;
	case C2K_CCISM_SHM_INIT_ACK:
		fsm_ccism_init_ack_handler(ctl->md_id, ccci_h->reserved);
		break;
	case C2K_FLOW_CTRL_MSG:
		ccci_hif_start_queue(ctl->md_id, ccci_h->reserved, OUT);
		break;
	default:
		CCCI_ERROR_LOG(ctl->md_id, FSM,
			"unknown control message %x\n", ccci_h->data[1]);
		break;
	}

	if (free_skb)
		ccci_free_skb(skb);
	return ret;
}

/* requested by throttling feature */
unsigned long ccci_get_md_boot_count(int md_id)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);

	if (ctl)
		return ctl->boot_count;
	else
		return 0;
}

