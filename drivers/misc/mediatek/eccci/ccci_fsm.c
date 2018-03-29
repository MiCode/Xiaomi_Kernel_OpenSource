/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ccci_core.h"
#include "ccci_modem.h"
#include "mdee_ctl.h"

static void ccci_fsm_finish_command(struct ccci_modem *md, struct ccci_fsm_command *cmd, int result);
static void ccci_fsm_finish_event(struct ccci_modem *md, struct ccci_fsm_event *event);

static struct ccci_fsm_command *ccci_check_for_ee(struct ccci_fsm_ctl *ctl, int xip)
{
	struct ccci_fsm_command *cmd, *next = NULL;
	struct ccci_modem *md = ctl->md;
	unsigned long flags;

	spin_lock_irqsave(&md->fsm.command_lock, flags);
	if (!list_empty(&ctl->command_queue)) {
		cmd = list_first_entry(&ctl->command_queue, struct ccci_fsm_command, entry);
		if (cmd->cmd_id == CCCI_COMMAND_EE) {
			if (xip)
				list_del(&cmd->entry);
			next = cmd;
		}
	}
	spin_unlock_irqrestore(&md->fsm.command_lock, flags);
	return next;
}

static void ccci_routine_zombie(struct ccci_fsm_ctl *ctl)
{
	struct ccci_fsm_event *event, *evt_next;
	struct ccci_fsm_command *cmd, *cmd_next;
	struct ccci_modem *md = ctl->md;
	unsigned long flags;

	CCCI_ERROR_LOG(md->index, FSM, "unexpected FSM state %d->%d, from %ps\n",
			ctl->last_state, ctl->curr_state,
			__builtin_return_address(0));
	spin_lock_irqsave(&md->fsm.command_lock, flags);
	list_for_each_entry_safe(cmd, cmd_next, &ctl->command_queue, entry) {
		CCCI_ERROR_LOG(md->index, FSM, "unhandled command %d\n", cmd->cmd_id);
		list_del(&cmd->entry);
		ccci_fsm_finish_command(md, cmd, -1);
	}
	spin_unlock_irqrestore(&md->fsm.command_lock, flags);
	spin_lock_irqsave(&md->fsm.event_lock, flags);
	list_for_each_entry_safe(event, evt_next, &ctl->event_queue, entry) {
		CCCI_ERROR_LOG(md->index, FSM, "unhandled event %d\n", event->event_id);
		ccci_fsm_finish_event(md, event);
	}
	spin_unlock_irqrestore(&md->fsm.event_lock, flags);
#if 0
	while (1)
		msleep(5000);
#endif
}

/* cmd is not NULL only when reason is ordinary EE */
static void ccci_routine_exception(struct ccci_fsm_ctl *ctl, struct ccci_fsm_command *cmd, CCCI_EE_REASON reason)
{
	struct ccci_modem *md = ctl->md;
	int count = 0, ccif_got = 0, ex_got = 0, rec_ok_got = 0, pass_got = 0;
	struct ccci_fsm_event *event;
	unsigned long flags;

	CCCI_NORMAL_LOG(md->index, FSM, "exception %d, from %ps\n", reason, __builtin_return_address(0));
	port_proxy_send_msg_to_user(md->port_proxy_obj, CCCI_MONITOR_CH, CCCI_MD_MSG_EXCEPTION, 0);
	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_GATED) {
		if (cmd)
			ccci_fsm_finish_command(md, cmd, -1);
		ccci_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_EXCEPTION;
	/* 2. check EE reason */
	switch (reason) {
	case EXCEPTION_HS1_TIMEOUT:
		CCCI_ERROR_LOG(md->index, FSM, "MD_BOOT_HS1_FAIL!\n");
		ccci_md_exception_notify(md, MD_BOOT_TIMEOUT);
		break;
	case EXCEPTION_HS2_TIMEOUT:
		CCCI_ERROR_LOG(md->index, FSM, "MD_BOOT_HS2_FAIL!\n");
		ccci_md_exception_notify(md, MD_BOOT_TIMEOUT);
		break;
	case EXCEPTION_MD_HANG:
	case EXCEPTION_WDT:
		mdee_monitor_func(md->mdee_obj);
		msleep(MD_EX_PASS_TIMEOUT);
		mdee_monitor2_func(md->mdee_obj);
		break;
	case EXCEPTION_EE:
		while (count < MD_EX_CCIF_TIMEOUT/EVENT_POLL_INTEVAL) {
			spin_lock_irqsave(&md->fsm.event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue, struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_CCIF_HS) {
					ccif_got = 1;
					ccci_fsm_finish_event(md, event);
				}
			}
			spin_unlock_irqrestore(&md->fsm.event_lock, flags);
			if (ccif_got)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		count = 0;
		while (count < MD_EX_REC_OK_TIMEOUT/EVENT_POLL_INTEVAL) {
			spin_lock_irqsave(&md->fsm.event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue, struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX) {
					ex_got = 1;
					ccci_fsm_finish_event(md, event);
				} else if (event->event_id == CCCI_EVENT_MD_EX_REC_OK) {
					rec_ok_got = 1;
					ccci_fsm_finish_event(md, event);
				}
			}
			spin_unlock_irqrestore(&md->fsm.event_lock, flags);
			if (rec_ok_got)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		mdee_monitor_func(md->mdee_obj);
		count = 0;
		while (count < MD_EX_PASS_TIMEOUT/EVENT_POLL_INTEVAL) {
			spin_lock_irqsave(&md->fsm.event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue, struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX_PASS) {
					pass_got = 1;
					ccci_fsm_finish_event(md, event);
				}
			}
			spin_unlock_irqrestore(&md->fsm.event_lock, flags);
			if (pass_got)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		mdee_monitor2_func(md->mdee_obj);
		break;
	default:
		break;
	}
	/* 3. always end in exception state */
	if (cmd)
		ccci_fsm_finish_command(md, cmd, 1);
}

static void ccci_routine_start(struct ccci_fsm_ctl *ctl, struct ccci_fsm_command *cmd)
{
	int ret;
	int count = 0, user_exit = 0, hs1_got = 0, hs2_got = 0;
	struct ccci_modem *md = ctl->md;
	struct ccci_fsm_event *event, *next;
	unsigned long flags;

	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_READY)
		goto success;
	if (ctl->curr_state != CCCI_FSM_GATED) {
		ccci_fsm_finish_command(md, cmd, -1);
		ccci_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STARTING;
	/* 2. poll for critical users exit */
	while (count < BOOT_TIMEOUT/EVENT_POLL_INTEVAL) {
		if (port_proxy_check_critical_user(md->port_proxy_obj) == 0) {
			user_exit = 1;
			break;
		}
		count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	/* what if critical user still alive:
	 * we can't wait for ever since this may be an illegal sequence (enter flight mode -> force start), and we
	 * must be able to recover from it.
	 * we'd better not entering exception state as start operation is not allowed in exception state.
	 * so we tango on...
	 */
	if (!user_exit)
		CCCI_ERROR_LOG(md->index, FSM, "critical user alive %d\n",
			port_proxy_check_critical_user(md->port_proxy_obj));
	spin_lock_irqsave(&md->fsm.event_lock, flags);
	list_for_each_entry_safe(event, next, &ctl->event_queue, entry) {
		CCCI_NORMAL_LOG(md->index, FSM, "drop event %d before start\n", event->event_id);
		ccci_fsm_finish_event(md, event);
	}
	spin_unlock_irqrestore(&md->fsm.event_lock, flags);
	/* 3. action and poll event queue */
	ret = ccci_md_start(md);
	if (ret)
		goto fail;
	count = 0;
	while (count < BOOT_TIMEOUT/EVENT_POLL_INTEVAL) {
		spin_lock_irqsave(&md->fsm.event_lock, flags);
		if (!list_empty(&ctl->event_queue)) {
			event = list_first_entry(&ctl->event_queue, struct ccci_fsm_event, entry);
			if (event->event_id == CCCI_EVENT_HS1) {
				hs1_got = 1;
				ccci_fsm_finish_event(md, event);
			} else if (event->event_id == CCCI_EVENT_HS2 && hs1_got) {
				hs2_got = 1;
				ccci_fsm_finish_event(md, event);
			}
		}
		spin_unlock_irqrestore(&md->fsm.event_lock, flags);
		if (ccci_check_for_ee(ctl, 0)) {
			CCCI_ERROR_LOG(md->index, FSM, "early exception detected\n");
			goto fail_ee;
		}
		if (hs2_got)
			goto success;
		if (atomic_read(&ctl->fs_ongoing))
			count = 0;
		else
			count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	/* 4. check result, finish command */
fail:
	if (hs1_got)
		ccci_routine_exception(ctl, NULL, EXCEPTION_HS2_TIMEOUT);
	else
		ccci_routine_exception(ctl, NULL, EXCEPTION_HS1_TIMEOUT);
	ccci_fsm_finish_command(md, cmd, -1);
	return;

fail_ee:
	/* exit imediately, let md_init have chance to start MD logger service */
	ccci_fsm_finish_command(md, cmd, -1);
	return;

success:
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_READY;
	ccci_fsm_finish_command(md, cmd, 1);
}

static void ccci_routine_stop(struct ccci_fsm_ctl *ctl, struct ccci_fsm_command *cmd)
{
	struct ccci_modem *md = ctl->md;
	struct ccci_fsm_event *event, *next;
	struct ccci_fsm_command *ee_cmd = NULL;
	unsigned long flags;

	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_GATED)
		goto success;
	if (ctl->curr_state != CCCI_FSM_READY && ctl->curr_state != CCCI_FSM_EXCEPTION) {
		ccci_fsm_finish_command(md, cmd, -1);
		ccci_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STOPPING;
	/* 2. pre-stop: polling MD for infinit sleep mode */
	ccci_md_pre_stop(md, cmd->flag & CCCI_CMD_FLAG_FLIGHT_MODE ? MD_FLIGHT_MODE_ENTER : MD_FLIGHT_MODE_NONE);
	/* 3. check for EE */
	ee_cmd = ccci_check_for_ee(ctl, 1);
	if (ee_cmd) {
		ccci_routine_exception(ctl, ee_cmd, EXCEPTION_EE);
		ccci_md_check_ee_done(md, EE_DONE_TIMEOUT);
	}
	ccci_md_broadcast_state(md, WAITING_TO_STOP); /* to block port's write operation, must after EE flow done */
	/* 4. hardware stop */
	ccci_md_stop(md, cmd->flag & CCCI_CMD_FLAG_FLIGHT_MODE ? MD_FLIGHT_MODE_ENTER : MD_FLIGHT_MODE_NONE);
	/* 5. clear event queue */
	spin_lock_irqsave(&md->fsm.event_lock, flags);
	list_for_each_entry_safe(event, next, &ctl->event_queue, entry) {
		CCCI_NORMAL_LOG(md->index, FSM, "drop event %d after stop\n", event->event_id);
		ccci_fsm_finish_event(md, event);
	}
	spin_unlock_irqrestore(&md->fsm.event_lock, flags);
	/* 6. always end in stopped state */
success:
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_GATED;
	ccci_fsm_finish_command(md, cmd, 1);
}

static void ccci_routine_wdt(struct ccci_fsm_ctl *ctl, struct ccci_fsm_command *cmd)
{
	struct ccci_modem *md = ctl->md;
	int reset_md = 0;

	if (md->ops->is_epon_set && md->ops->is_epon_set(md)) {
		CCCI_NORMAL_LOG(md->index, FSM, "reset MD after WDT\n");
		reset_md = 1;
	} else {
		if (port_proxy_get_critical_user(md->port_proxy_obj, CRIT_USR_MDLOG) == 0) {
			CCCI_NORMAL_LOG(md->index, FSM, "mdlogger closed, reset MD after WDT\n");
			reset_md = 1;
		} else {
			ccci_routine_exception(ctl, NULL, EXCEPTION_WDT);
		}
	}
	if (reset_md) {
		port_proxy_send_msg_to_user(md->port_proxy_obj, CCCI_MONITOR_CH,
			CCCI_MD_MSG_RESET_REQUEST, 0);
#ifdef CONFIG_MTK_ECCCI_C2K
		if (md->index == MD_SYS1)
			exec_ccci_kern_func_by_md_id(MD_SYS3, ID_RESET_MD, NULL, 0);
		else if (md->index == MD_SYS3)
			exec_ccci_kern_func_by_md_id(MD_SYS1, ID_RESET_MD, NULL, 0);
#else
#if defined(CONFIG_MTK_MD3_SUPPORT) && (CONFIG_MTK_MD3_SUPPORT > 0)
		if (md->index == MD_SYS1 && ccci_get_opt_val("opt_c2k_lte_mode") == 1)
			c2k_reset_modem();
#endif
#endif
	}
	ccci_fsm_finish_command(md, cmd, 1);
}

static int ccci_fsm_main(void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct ccci_fsm_ctl *ctl = &md->fsm;
	struct ccci_fsm_command *cmd = NULL;
	unsigned long flags;

	while (1) {
		wait_event(ctl->command_wq, !list_empty(&ctl->command_queue));
		spin_lock_irqsave(&md->fsm.command_lock, flags);
		cmd = list_first_entry(&ctl->command_queue, struct ccci_fsm_command, entry);
		list_del(&cmd->entry); /* delete first, otherwise hard to peek next command in routines */
		spin_unlock_irqrestore(&md->fsm.command_lock, flags);

		CCCI_NORMAL_LOG(md->index, FSM, "command %d process\n", cmd->cmd_id);
		switch (cmd->cmd_id) {
		case CCCI_COMMAND_START:
			ccci_routine_start(ctl, cmd);
			break;
		case CCCI_COMMAND_STOP:
			ccci_routine_stop(ctl, cmd);
			break;
		case CCCI_COMMAND_WDT:
			ccci_routine_wdt(ctl, cmd);
			break;
		case CCCI_COMMAND_EE:
			ccci_routine_exception(ctl, cmd, EXCEPTION_EE);
			break;
		case CCCI_COMMAND_MD_HANG:
			ccci_routine_exception(ctl, NULL, EXCEPTION_MD_HANG);
			ccci_fsm_finish_command(md, cmd, 1);
			break;
		default:
			ccci_fsm_finish_command(md, cmd, -1);
			ccci_routine_zombie(ctl);
			break;
		};
	}
	return 0;
}

int ccci_fsm_init(struct ccci_modem *md)
{
	struct ccci_fsm_ctl *ctl = &md->fsm;

	ctl->last_state = CCCI_FSM_INVALID;
	ctl->curr_state = CCCI_FSM_GATED;
	INIT_LIST_HEAD(&ctl->command_queue);
	INIT_LIST_HEAD(&ctl->event_queue);
	init_waitqueue_head(&ctl->command_wq);
	spin_lock_init(&ctl->event_lock);
	spin_lock_init(&ctl->command_lock);
	spin_lock_init(&ctl->cmd_complete_lock);
	ctl->md = md;
	atomic_set(&md->fsm.fs_ongoing, 0);

	ctl->fsm_thread = kthread_run(ccci_fsm_main, md, "ccci_fsm%d", md->index + 1);
	return 0;
}

int ccci_fsm_append_command(struct ccci_modem *md, CCCI_FSM_COMMAND cmd_id, unsigned int flag)
{
	struct ccci_fsm_command *cmd = NULL;
	struct ccci_fsm_ctl *ctl = &md->fsm;
	int result = 0;
	unsigned long flags;

	if (cmd_id <= CCCI_COMMAND_INVALID || cmd_id >= CCCI_COMMAND_MAX) {
		CCCI_ERROR_LOG(md->index, FSM, "invalid command %d\n", cmd_id);
		return -EINVAL;
	}
	cmd = kmalloc(sizeof(struct ccci_fsm_command), (in_irq() || irqs_disabled()) ? GFP_ATOMIC : GFP_KERNEL);
	if (!cmd) {
		CCCI_ERROR_LOG(md->index, FSM, "fail to alloc command %d\n", cmd_id);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&cmd->entry);
	init_waitqueue_head(&cmd->complete_wq);
	cmd->cmd_id = cmd_id;
	cmd->complete = 0;
	if (in_irq() || irqs_disabled())
		flag &= ~CCCI_CMD_FLAG_WAIT_FOR_COMPLETE;
	cmd->flag = flag;

	spin_lock_irqsave(&md->fsm.command_lock, flags);
	list_add_tail(&cmd->entry, &md->fsm.command_queue);
	spin_unlock_irqrestore(&md->fsm.command_lock, flags);
	CCCI_NORMAL_LOG(md->index, FSM, "command %d is appended %x from %ps\n", cmd->cmd_id, cmd->flag,
			__builtin_return_address(0));
	wake_up(&ctl->command_wq); /* after this line, only dereference cmd when "wait-for-complete" */
	if (flag & CCCI_CMD_FLAG_WAIT_FOR_COMPLETE) {
		wait_event(cmd->complete_wq, cmd->complete != 0);
		if (cmd->complete != 1)
			result = -1;
		spin_lock_irqsave(&md->fsm.cmd_complete_lock, flags);
		kfree(cmd);
		spin_unlock_irqrestore(&md->fsm.cmd_complete_lock, flags);
	}
	return result;
}

static void ccci_fsm_finish_command(struct ccci_modem *md, struct ccci_fsm_command *cmd, int result)
{
	unsigned long flags;

	CCCI_NORMAL_LOG(md->index, FSM, "command %d is completed %d by %ps\n", cmd->cmd_id, result,
			__builtin_return_address(0));
	if (cmd->flag & CCCI_CMD_FLAG_WAIT_FOR_COMPLETE) {
		spin_lock_irqsave(&md->fsm.cmd_complete_lock, flags);
		cmd->complete = result;
		wake_up_all(&cmd->complete_wq); /* do not dereference cmd after this line */
		/* after cmd in list, processing thread may see it without being waked up, so spinlock is needed */
		spin_unlock_irqrestore(&md->fsm.cmd_complete_lock, flags);
	} else {
		/* no one is waiting for this cmd, free to free */
		kfree(cmd);
	}
}

int ccci_fsm_append_event(struct ccci_modem *md, CCCI_FSM_EVENT event_id,
	unsigned char *data, unsigned int length)
{
	struct ccci_fsm_event *event = NULL;
	unsigned long flags;

	if (event_id <= CCCI_EVENT_INVALID || event_id >= CCCI_EVENT_MAX) {
		CCCI_ERROR_LOG(md->index, FSM, "invalid event %d\n", event_id);
		return -EINVAL;
	}
	if (event_id == CCCI_EVENT_FS_IN) {
		atomic_set(&(md->fsm.fs_ongoing), 1);
		return 0;
	} else if (event_id == CCCI_EVENT_FS_OUT) {
		atomic_set(&(md->fsm.fs_ongoing), 0);
		return 0;
	}
	event = kmalloc(sizeof(struct ccci_fsm_event) + length, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
	if (!event) {
		CCCI_ERROR_LOG(md->index, FSM, "fail to alloc event%d\n", event_id);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&event->entry);
	event->event_id = event_id;
	event->length = length;
	if (data && length)
		memcpy(event->data, data, length);

	spin_lock_irqsave(&md->fsm.event_lock, flags);
	list_add_tail(&event->entry, &md->fsm.event_queue);
	spin_unlock_irqrestore(&md->fsm.event_lock, flags);
	/* do not derefence event after here */
	CCCI_NORMAL_LOG(md->index, FSM, "event %d is appended from %ps\n", event_id,
		__builtin_return_address(0));
	return 0;
}

/* must be called within protection of event_lock */
static void ccci_fsm_finish_event(struct ccci_modem *md, struct ccci_fsm_event *event)
{
	list_del(&event->entry);
	CCCI_NORMAL_LOG(md->index, FSM, "event %d is completed by %ps\n", event->event_id,
			__builtin_return_address(0));
	kfree(event);
}
