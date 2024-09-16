/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "gps_dl_config.h"
#include "gps_dl_time_tick.h"

#include "gps_each_link.h"
#include "gps_dl_hal.h"
#include "gps_dl_hal_api.h"
#include "gps_dl_hal_util.h"
#include "gps_dl_hw_api.h"
#include "gps_dl_isr.h"
#include "gps_dl_lib_misc.h"
#include "gps_dsp_fsm.h"
#include "gps_dl_osal.h"
#include "gps_dl_name_list.h"
#include "gps_dl_context.h"
#include "gps_dl_subsys_reset.h"

#include "linux/jiffies.h"

#include "linux/errno.h"
#if GPS_DL_HAS_PLAT_DRV
#include "gps_dl_linux_plat_drv.h"
#endif

void gps_each_link_set_bool_flag(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_bool_state name, bool value)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);

	if (!p)
		return;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	switch (name) {
	case LINK_TO_BE_CLOSED:
		p->sub_states.to_be_closed = value;
		break;
	case LINK_USER_OPEN:
		p->sub_states.user_open = value;
		break;
	case LINK_OPEN_RESULT_OKAY:
		p->sub_states.open_result_okay = value;
		break;
	case LINK_NEED_A2Z_DUMP:
		p->sub_states.need_a2z_dump = value;
		break;
	case LINK_SUSPEND_TO_CLK_EXT:
		p->sub_states.suspend_to_clk_ext = value;
		break;
	default:
		break; /* do nothing */
	}
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
}

bool gps_each_link_get_bool_flag(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_bool_state name)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	bool value = false;

	if (!p)
		return false;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	switch (name) {
	case LINK_TO_BE_CLOSED:
		value = p->sub_states.to_be_closed;
		break;
	case LINK_USER_OPEN:
		value = p->sub_states.user_open;
		break;
	case LINK_OPEN_RESULT_OKAY:
		value = p->sub_states.open_result_okay;
		break;
	case LINK_NEED_A2Z_DUMP:
		value = p->sub_states.need_a2z_dump;
		break;
	case LINK_SUSPEND_TO_CLK_EXT:
		value = p->sub_states.suspend_to_clk_ext;
		break;
	default:
		break; /* TODO: warning it */
	}
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	return value;
}

void gps_dl_link_set_ready_to_write(enum gps_dl_link_id_enum link_id, bool is_ready)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	if (p)
		p->sub_states.is_ready_to_write = is_ready;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
}

bool gps_dl_link_is_ready_to_write(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	bool ready;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	if (p)
		ready = p->sub_states.is_ready_to_write;
	else
		ready = false;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	return ready;
}

void gps_each_link_set_active(enum gps_dl_link_id_enum link_id, bool is_active)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	if (p)
		p->sub_states.is_active = is_active;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
}

bool gps_each_link_is_active(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	bool ready;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	if (p)
		ready = p->sub_states.is_active;
	else
		ready = false;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	return ready;
}

void gps_each_link_inc_session_id(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	int sid;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	if (p->session_id >= GPS_EACH_LINK_SID_MAX)
		p->session_id = 1;
	else
		p->session_id++;
	sid = p->session_id;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	GDL_LOGXD(link_id, "new sid = %d, 1byte_mode = %d", sid, gps_dl_is_1byte_mode());
}

int gps_each_link_get_session_id(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	int sid;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	sid = p->session_id;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	return sid;
}

void gps_dl_link_open_wait(enum gps_dl_link_id_enum link_id, long *p_sigval)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum GDL_RET_STATUS gdl_ret;
	long sigval;

	gdl_ret = gps_dl_link_wait_on(&p->waitables[GPS_DL_WAIT_OPEN_CLOSE], &sigval);
	if (gdl_ret == GDL_FAIL_SIGNALED) {
		if (p_sigval != NULL) {
			*p_sigval = sigval;
			return;
		}
	} else if (gdl_ret == GDL_FAIL_NOT_SUPPORT)
		; /* show warnning */
}

void gps_dl_link_open_ack(enum gps_dl_link_id_enum link_id, bool okay, bool hw_resume)
{
#if 0
	enum GDL_RET_STATUS gdl_ret;
	struct gdl_dma_buf_entry dma_buf_entry;
#endif
	struct gps_each_link *p = gps_dl_link_get(link_id);
	bool send_msg = false;

	GDL_LOGXD_ONF(link_id, "hw_resume = %d", hw_resume);

	/* TODO: open fail case */
	gps_each_link_set_bool_flag(link_id, LINK_OPEN_RESULT_OKAY, okay);
	gps_dl_link_wake_up(&p->waitables[GPS_DL_WAIT_OPEN_CLOSE]);

	gps_each_link_take_big_lock(link_id, GDL_LOCK_FOR_OPEN_DONE);
	if (gps_each_link_get_bool_flag(link_id, LINK_USER_OPEN) && okay) {
		GDL_LOGXW_ONF(link_id,
			"user still online, try to change to opened");

		/* Note: if pre_status not OPENING, it might be RESETTING, not handle it here */
		if (hw_resume)
			gps_each_link_change_state_from(link_id, LINK_RESUMING, LINK_OPENED);
		else
			gps_each_link_change_state_from(link_id, LINK_OPENING, LINK_OPENED);

		/* TODO: ack on DSP reset done */
#if 0
		/* if has pending data, can send it now */
		gdl_ret = gdl_dma_buf_get_data_entry(&p->tx_dma_buf, &dma_buf_entry);
		if (gdl_ret == GDL_OKAY)
			gps_dl_hal_a2d_tx_dma_start(link_id, &dma_buf_entry);
#endif
	} else {
		GDL_LOGXW_ONF(link_id,
			"okay = %d or user already offline, try to change to closing", okay);

		/* Note: if pre_status not OPENING, it might be RESETTING, not handle it here */
		if (gps_each_link_change_state_from(link_id, LINK_OPENING, LINK_CLOSING))
			send_msg = true;
	}
	gps_each_link_give_big_lock(link_id);

	if (send_msg) {
		gps_dl_link_event_send(GPS_DL_EVT_LINK_CLOSE, link_id);
		gps_each_link_set_bool_flag(link_id, LINK_TO_BE_CLOSED, true);
	}
}

void gps_each_link_init(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);

	p->session_id = 0;
	gps_each_link_mutexes_init(p);
	gps_each_link_spin_locks_init(p);
	gps_each_link_set_active(link_id, false);
	gps_dl_link_set_ready_to_write(link_id, false);
	gps_each_link_context_init(link_id);
	gps_each_link_set_state(link_id, LINK_CLOSED);
}

void gps_each_link_deinit(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);

	gps_each_link_set_state(link_id, LINK_UNINIT);
	gps_each_link_mutexes_deinit(p);
	gps_each_link_spin_locks_deinit(p);
}

void gps_each_link_context_init(enum gps_dl_link_id_enum link_id)
{
	enum gps_each_link_waitable_type j;

	for (j = 0; j < GPS_DL_WAIT_NUM; j++)
		gps_dl_link_waitable_reset(link_id, j);
}

void gps_each_link_context_clear(enum gps_dl_link_id_enum link_id)
{
	gps_dl_link_waitable_reset(link_id, GPS_DL_WAIT_WRITE);
	gps_dl_link_waitable_reset(link_id, GPS_DL_WAIT_READ);
}

int gps_each_link_open(enum gps_dl_link_id_enum link_id)
{
	enum gps_each_link_state_enum state, state2;
	enum GDL_RET_STATUS gdl_ret;
	long sigval = 0;
	bool okay;
	int retval;
#if GPS_DL_ON_CTP
	/* Todo: is it need on LINUX? */
	struct gps_each_link *p_link = gps_dl_link_get(link_id);

	gps_dma_buf_align_as_byte_mode(&p_link->tx_dma_buf);
	gps_dma_buf_align_as_byte_mode(&p_link->rx_dma_buf);
#endif

#if 0
#if (GPS_DL_ON_LINUX && !GPS_DL_NO_USE_IRQ && !GPS_DL_HW_IS_MOCK)
	if (!p_link->sub_states.irq_is_init_done) {
		gps_dl_irq_init();
		p_link->sub_states.irq_is_init_done = true;
	}
#endif
#endif

	state = gps_each_link_get_state(link_id);

	switch (state) {
	case LINK_RESUMING:
	case LINK_SUSPENDED:
	case LINK_SUSPENDING:
		retval = -EAGAIN;
		break;

	case LINK_CLOSING:
	case LINK_RESETTING:
	case LINK_DISABLED:
		retval = -EAGAIN;
		break;

	case LINK_RESET_DONE:
		/* RESET_DONE stands for user space not close me */
		retval = -EBUSY; /* twice open not allowed */
		break;

	case LINK_OPENED:
	case LINK_OPENING:
		retval = -EBUSY;; /* twice open not allowed */
		break;

	case LINK_CLOSED:
		okay = gps_each_link_change_state_from(link_id, LINK_CLOSED, LINK_OPENING);
		if (!okay) {
			retval = -EBUSY;
			break;
		}

		/* TODO: simplify the flags */
		gps_each_link_set_bool_flag(link_id, LINK_TO_BE_CLOSED, false);
		gps_each_link_set_bool_flag(link_id, LINK_USER_OPEN, true);

		gps_dl_link_waitable_reset(link_id, GPS_DL_WAIT_OPEN_CLOSE);
		gps_dl_link_event_send(GPS_DL_EVT_LINK_OPEN, link_id);
		gps_dl_link_open_wait(link_id, &sigval);

		/* TODO: Check this mutex can be removed?
		 * the possible purpose is make it's atomic from LINK_USER_OPEN and LINK_OPEN_RESULT_OKAY.
		 */
		gps_each_link_take_big_lock(link_id, GDL_LOCK_FOR_OPEN);
		if (sigval != 0) {
			gps_each_link_set_bool_flag(link_id, LINK_USER_OPEN, false);

			gdl_ret = gps_dl_link_try_wait_on(link_id, GPS_DL_WAIT_OPEN_CLOSE);
			if (gdl_ret == GDL_OKAY) {
				okay = gps_each_link_change_state_from(link_id, LINK_OPENED, LINK_CLOSING);

				/* Change okay, need to send event to trigger close */
				if (okay) {
					gps_each_link_give_big_lock(link_id);
					gps_dl_link_event_send(GPS_DL_EVT_LINK_CLOSE, link_id);
					GDL_LOGXW_ONF(link_id,
						"sigval = %ld, corner case 1: close it", sigval);
					retval = -EBUSY;
					break;
				}

				/* Not change okay, state maybe RESETTING or RESET_DONE */
				state2 = gps_each_link_get_state(link_id);
				if (state2 == LINK_RESET_DONE)
					gps_each_link_set_state(link_id, LINK_CLOSED);

				gps_each_link_give_big_lock(link_id);
				GDL_LOGXW_ONF(link_id, "sigval = %ld, corner case 2: %s",
					sigval, gps_dl_link_state_name(state2));
				retval = -EBUSY;
				break;
			}

			gps_each_link_give_big_lock(link_id);
			GDL_LOGXW_ONF(link_id, "sigval = %ld, normal case", sigval);
			retval = -EINVAL;
			break;
		}

		okay = gps_each_link_get_bool_flag(link_id, LINK_OPEN_RESULT_OKAY);
		gps_each_link_give_big_lock(link_id);

		if (okay)
			retval = 0;
		else {
			gps_each_link_set_bool_flag(link_id, LINK_USER_OPEN, false);
			retval = -EBUSY;
		}
		break;

	default:
		retval = -EINVAL;
		break;
	}

	if (retval == 0) {
		GDL_LOGXD_ONF(link_id, "prev_state = %s, retval = %d",
			gps_dl_link_state_name(state), retval);
	} else {
		GDL_LOGXW_ONF(link_id, "prev_state = %s, retval = %d",
			gps_dl_link_state_name(state), retval);
	}

	return retval;
}

int gps_each_link_reset(enum gps_dl_link_id_enum link_id)
{
	/*
	 * - set each link resetting flag
	 */
	enum gps_each_link_state_enum state, state2;
	bool okay;
	int retval;

	state = gps_each_link_get_state(link_id);

	switch (state) {
	case LINK_OPENING:
	case LINK_CLOSING:
	case LINK_CLOSED:
	case LINK_DISABLED:
		retval = -EBUSY;
		break;

	case LINK_RESETTING:
	case LINK_RESET_DONE:
		retval = 0;
		break;

	case LINK_RESUMING:
	case LINK_SUSPENDING:
	case LINK_SUSPENDED:
	case LINK_OPENED:
_try_change_to_reset_again:
		okay = gps_each_link_change_state_from(link_id, state, LINK_RESETTING);
		if (!okay) {
			state2 = gps_each_link_get_state(link_id);

			/* Already reset or close, not trigger reseeting again */
			GDL_LOGXW_ONF(link_id, "state flip to %s - corner case",
				gps_dl_link_state_name(state2));

			/* -ing state may become -ed state, try change to reset again */
			if ((state == LINK_SUSPENDING && state2 == LINK_SUSPENDED) ||
				(state == LINK_RESUMING && state2 == LINK_OPENED)) {
				state = state2;
				goto _try_change_to_reset_again;
			}

			if (state2 == LINK_RESETTING || state2 == LINK_RESET_DONE)
				retval = 0;
			else
				retval = -EBUSY;
			break;
		}

		gps_each_link_set_bool_flag(link_id, LINK_IS_RESETTING, true);

		/* no need to wait reset ack
		 * TODO: make sure message send okay
		 */
		gps_dl_link_waitable_reset(link_id, GPS_DL_WAIT_RESET);
		gps_dl_link_event_send(GPS_DL_EVT_LINK_RESET_DSP, link_id);
		retval = 0;
		break;

	default:
		retval = -EINVAL;
		break;
	}

	/* wait until cttld thread ack the status */
	if (retval == 0) {
		GDL_LOGXD_ONF(link_id, "prev_state = %s, retval = %d",
			gps_dl_link_state_name(state), retval);
	} else {
		GDL_LOGXW_ONF(link_id, "prev_state = %s, retval = %d",
			gps_dl_link_state_name(state), retval);
	}

	return retval;
}

void gps_dl_ctrld_set_resest_status(void)
{
	gps_each_link_set_active(GPS_DATA_LINK_ID0, false);
	gps_each_link_set_active(GPS_DATA_LINK_ID1, false);
}

void gps_dl_link_reset_ack_inner(enum gps_dl_link_id_enum link_id, bool post_conn_reset)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum gps_each_link_state_enum old_state, new_state;
	enum gps_each_link_reset_level old_level, new_level;
	bool user_still_open;
	bool both_clear_done = false;
	bool try_conn_infra_off = false;

	gps_each_link_take_big_lock(link_id, GDL_LOCK_FOR_RESET_DONE);
	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	old_state = p->state_for_user;
	old_level = p->reset_level;
	user_still_open = p->sub_states.user_open;

	switch (old_level) {
	case GPS_DL_RESET_LEVEL_GPS_SINGLE_LINK:
		p->reset_level = GPS_DL_RESET_LEVEL_NONE;
		if (p->sub_states.user_open)
			p->state_for_user = LINK_RESET_DONE;
		else
			p->state_for_user = LINK_CLOSED;
		break;

	case GPS_DL_RESET_LEVEL_CONNSYS:
		if (!post_conn_reset)
			break;
		p->reset_level = GPS_DL_RESET_LEVEL_NONE;
		both_clear_done = gps_dl_link_try_to_clear_both_resetting_status();
		try_conn_infra_off = true;
		break;

	case GPS_DL_RESET_LEVEL_GPS_SUBSYS:
		p->reset_level = GPS_DL_RESET_LEVEL_NONE;
		both_clear_done = gps_dl_link_try_to_clear_both_resetting_status();
		break;

	default:
		break;
	}

	new_state = p->state_for_user;
	new_level = p->reset_level;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	if (try_conn_infra_off) {
		/* During connsys resetting, conninfra_pwr_off may fail,
		 * it need to be called again when connsys reset done.
		 */
		gps_dl_hal_conn_infra_driver_off();
	}

	/* TODO: if both clear, show another link's log */
	GDL_LOGXE_STA(link_id,
		"state change: %s -> %s, level: %d -> %d, user = %d, post_reset = %d, both_clear = %d",
		gps_dl_link_state_name(old_state), gps_dl_link_state_name(new_state),
		old_level, new_level,
		user_still_open, post_conn_reset, both_clear_done);

	gps_each_link_give_big_lock(link_id);

	/* Note: for CONNSYS or GPS_SUBSYS RESET, here might be still RESETTING,
	 * if any other link not reset done (see both_clear_done print).
	 */
	gps_dl_link_wake_up(&p->waitables[GPS_DL_WAIT_RESET]);
}

bool gps_dl_link_try_to_clear_both_resetting_status(void)
{
	enum gps_dl_link_id_enum link_id;
	struct gps_each_link *p;

	for (link_id = 0; link_id < GPS_DATA_LINK_NUM; link_id++) {
		p = gps_dl_link_get(link_id);
		if (p->reset_level != GPS_DL_RESET_LEVEL_NONE)
			return false;
	}

	for (link_id = 0; link_id < GPS_DATA_LINK_NUM; link_id++) {
		p = gps_dl_link_get(link_id);

		if (p->sub_states.user_open)
			p->state_for_user = LINK_RESET_DONE;
		else
			p->state_for_user = LINK_CLOSED;
	}

	return true;
}

void gps_dl_link_reset_ack(enum gps_dl_link_id_enum link_id)
{
	gps_dl_link_reset_ack_inner(link_id, false);
}

void gps_dl_link_on_post_conn_reset(enum gps_dl_link_id_enum link_id)
{
	gps_dl_link_reset_ack_inner(link_id, true);
}

void gps_dl_link_close_wait(enum gps_dl_link_id_enum link_id, long *p_sigval)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum GDL_RET_STATUS gdl_ret;
	long sigval;

	gdl_ret = gps_dl_link_wait_on(&p->waitables[GPS_DL_WAIT_OPEN_CLOSE], &sigval);
	if (gdl_ret == GDL_FAIL_SIGNALED) {
		if (p_sigval != NULL) {
			*p_sigval = sigval;
			return;
		}
	} else if (gdl_ret == GDL_FAIL_NOT_SUPPORT)
		; /* show warnning */
}

void gps_dl_link_close_ack(enum gps_dl_link_id_enum link_id, bool hw_suspend)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);

	GDL_LOGXD_ONF(link_id, "hw_suspend = %d", hw_suspend);
	gps_dl_link_wake_up(&p->waitables[GPS_DL_WAIT_OPEN_CLOSE]);

	gps_each_link_take_big_lock(link_id, GDL_LOCK_FOR_CLOSE_DONE);

	/* gps_each_link_set_state(link_id, LINK_CLOSED); */
	/* For case of reset_done */
	if (hw_suspend) {
		gps_each_link_change_state_from(link_id, LINK_SUSPENDING, LINK_SUSPENDED);
		/* TODO */
	} else
		gps_each_link_change_state_from(link_id, LINK_CLOSING, LINK_CLOSED);

	gps_each_link_give_big_lock(link_id);
}

int gps_each_link_close_or_suspend_inner(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_state_enum state,
	enum gps_each_link_close_or_suspend_op close_or_suspend_op)
{
	enum gps_each_link_state_enum state2;
	bool okay;
	int retval = 0;
	bool hw_suspend;

	hw_suspend = !!(close_or_suspend_op == GDL_DPSTOP || close_or_suspend_op == GDL_CLKEXT);
	gps_each_link_take_big_lock(link_id, GDL_LOCK_FOR_CLOSE);
	do {
		if (hw_suspend) {
			okay = gps_each_link_change_state_from(link_id, LINK_OPENED, LINK_SUSPENDING);
			if (!okay) {
				state2 = gps_each_link_get_state(link_id);
				gps_each_link_give_big_lock(link_id);
				GDL_LOGXW_ONF(link_id, "state check: %s, return hw suspend fail",
					gps_dl_link_state_name(state2));
				retval = -EINVAL;
				break;
			}

			gps_each_link_set_bool_flag(link_id,
				LINK_SUSPEND_TO_CLK_EXT, close_or_suspend_op == GDL_CLKEXT);
		} else {
			if (state == LINK_SUSPENDED) {
				okay = gps_each_link_change_state_from(
					link_id, LINK_SUSPENDED, LINK_CLOSING);
			} else {
				okay = gps_each_link_change_state_from(
					link_id, LINK_OPENED, LINK_CLOSING);
			}
			gps_each_link_set_bool_flag(link_id, LINK_USER_OPEN, false);
			if (!okay) {
				state2 = gps_each_link_get_state(link_id);
				if (state2 == LINK_RESET_DONE)
					gps_each_link_set_state(link_id, LINK_CLOSED);
				else {
					GDL_LOGXW_ONF(link_id, "state check: %s, return close ok",
						gps_dl_link_state_name(state2));
				}
				gps_each_link_give_big_lock(link_id);
				retval = 0;
				break;
			}
		}
		gps_each_link_give_big_lock(link_id);
	} while (0);
	return retval;
}

int gps_each_link_close_or_suspend(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_close_or_suspend_op close_or_suspend_op)
{
	enum gps_each_link_state_enum state;
	long sigval = 0;
	int retval;
	bool hw_suspend;

	state = gps_each_link_get_state(link_id);
	hw_suspend = !!(close_or_suspend_op == GDL_DPSTOP || close_or_suspend_op == GDL_CLKEXT);

	switch (state) {
	case LINK_OPENING:
	case LINK_CLOSING:
	case LINK_CLOSED:
	case LINK_DISABLED:
		/* twice close */
		/* TODO: show user open flag */
		retval = -EINVAL;
		break;

	case LINK_SUSPENDING:
	case LINK_RESUMING:
	case LINK_RESETTING:
		if (hw_suspend) {
			if (state == LINK_SUSPENDING)
				retval = 0;
			else if (state == LINK_RESUMING)
				retval = -EBUSY;
			else
				retval = -EINVAL;
			break;
		}

		/* close on xxx-ing states: just recording user is not online
		 * ctrld will handle it on the end of xxx-ing handling
		 */
		gps_each_link_set_bool_flag(link_id, LINK_USER_OPEN, false);
		GDL_LOGXE_ONF(link_id, "state check: %s, return close ok", gps_dl_link_state_name(state));
		/* return okay to avoid twice close */
		retval = 0;
		break;

	case LINK_RESET_DONE:
		if (hw_suspend) {
			retval = -EINVAL;
			break;
		}
		gps_each_link_set_bool_flag(link_id, LINK_USER_OPEN, false);
		gps_each_link_set_state(link_id, LINK_CLOSED);
		retval = 0;
		break;

	case LINK_SUSPENDED:
	case LINK_OPENED:
		retval = gps_each_link_close_or_suspend_inner(link_id, state, close_or_suspend_op);
		if (retval != 0)
			break;

		/* clean the done(fired) flag before send and wait */
		gps_dl_link_waitable_reset(link_id, GPS_DL_WAIT_OPEN_CLOSE);
		if (hw_suspend)
			gps_dl_link_event_send(GPS_DL_EVT_LINK_ENTER_DPSTOP, link_id);
		else
			gps_dl_link_event_send(GPS_DL_EVT_LINK_CLOSE, link_id);

		/* set this status, hal proc will by pass the message from the link
		 * it can make LINK_CLOSE be processed faster
		 */
		gps_each_link_set_bool_flag(link_id, LINK_TO_BE_CLOSED, true);
		gps_dl_link_close_wait(link_id, &sigval);

		if (sigval) {
			retval = -EINVAL;
			break;
		}

		retval = 0;
		break;
	default:
		retval = -EINVAL;
		break;
	}

	if (retval == 0) {
		GDL_LOGXD_ONF(link_id, "prev_state = %s, retval = %d, op = %d",
			gps_dl_link_state_name(state), retval, close_or_suspend_op);
	} else {
		GDL_LOGXW_ONF(link_id, "prev_state = %s, retval = %d, op = %d",
			gps_dl_link_state_name(state), retval, close_or_suspend_op);
	}

	return retval;
}

int gps_each_link_close(enum gps_dl_link_id_enum link_id)
{
	return gps_each_link_close_or_suspend(link_id, GDL_CLOSE);
}
int gps_each_link_check(enum gps_dl_link_id_enum link_id, int reason)
{
	enum gps_each_link_state_enum state;
	enum gps_dl_link_event_id event;
	int retval = 0;

	state = gps_each_link_get_state(link_id);

	switch (state) {
	case LINK_OPENING:
	case LINK_CLOSING:
	case LINK_CLOSED:
	case LINK_DISABLED:
		break;

	case LINK_RESETTING:
#if 0
		if (rstflag == 1) {
			/* chip resetting */
			retval = -888;
		} else if (rstflag == 2) {
			/* chip reset end */
			retval = -889;
		} else {
			/* normal */
			retval = 0;
		}
#endif
		retval = -888;
		break;

	case LINK_RESET_DONE:
		retval = 889;
		break;

	case LINK_RESUMING:
	case LINK_SUSPENDING:
	case LINK_SUSPENDED:
	case LINK_OPENED:
		if (reason == 2)
			event = GPS_DL_EVT_LINK_PRINT_HW_STATUS;
		else if (reason == 4)
			event = GPS_DL_EVT_LINK_PRINT_DATA_STATUS;
		else
			break;

		/* if L1 trigger it, also print L5 status
		 * for this case, dump L5 firstly.
		 */
		if (link_id == GPS_DATA_LINK_ID0)
			gps_dl_link_event_send(event, GPS_DATA_LINK_ID1);

		gps_dl_link_event_send(event, link_id);
		break;

	default:
		break;
	}

	GDL_LOGXW_ONF(link_id, "prev_state = %s, reason = %d, retval = %d",
		gps_dl_link_state_name(state), reason, retval);

	return retval;
}

int gps_each_link_enter_dsleep(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);

	gps_dl_link_event_send(GPS_DL_EVT_LINK_ENTER_DPSLEEP, link_id);
	gps_dma_buf_reset(&p_link->tx_dma_buf);
	gps_dma_buf_reset(&p_link->rx_dma_buf);
	return 0;
}

int gps_each_link_leave_dsleep(enum gps_dl_link_id_enum link_id)
{
#if GPS_DL_ON_CTP
	struct gps_each_link *p_link = gps_dl_link_get(link_id);

	gps_dma_buf_align_as_byte_mode(&p_link->tx_dma_buf);
	gps_dma_buf_align_as_byte_mode(&p_link->rx_dma_buf);
#endif
	gps_dl_link_event_send(GPS_DL_EVT_LINK_LEAVE_DPSLEEP, link_id);
	return 0;
}


int gps_each_link_hw_suspend(enum gps_dl_link_id_enum link_id, bool need_clk_ext)
{
	enum gps_each_link_close_or_suspend_op op;

	if (need_clk_ext)
		op = GDL_CLKEXT;
	else
		op = GDL_DPSTOP;
	return gps_each_link_close_or_suspend(link_id, op);
}

int gps_each_link_hw_resume(enum gps_dl_link_id_enum link_id)
{
	enum gps_each_link_state_enum state;
	long sigval = 0;
	bool okay;
	int retval;
#if GPS_DL_ON_CTP
	struct gps_each_link *p_link = gps_dl_link_get(link_id);
#endif

	state = gps_each_link_get_state(link_id);
	do {
		if (state != LINK_SUSPENDED) {
			retval = -EINVAL;
			break;
		}

		okay = gps_each_link_change_state_from(link_id, LINK_SUSPENDED, LINK_RESUMING);
		if (!okay) {
			retval = -EBUSY;
			break;
		}

		gps_each_link_set_bool_flag(link_id, LINK_TO_BE_CLOSED, false);
		gps_dl_link_waitable_reset(link_id, GPS_DL_WAIT_OPEN_CLOSE);
#if GPS_DL_ON_CTP
		gps_dma_buf_align_as_byte_mode(&p_link->tx_dma_buf);
		gps_dma_buf_align_as_byte_mode(&p_link->rx_dma_buf);
#endif
		gps_dl_link_event_send(GPS_DL_EVT_LINK_LEAVE_DPSTOP, link_id);
		gps_dl_link_open_wait(link_id, &sigval);
		if (sigval != 0) {
			GDL_LOGXW_ONF(link_id, "sigval = %ld", sigval);
			retval = -EBUSY;
			break;
		}

		okay = gps_each_link_get_bool_flag(link_id, LINK_OPEN_RESULT_OKAY);
		if (okay)
			retval = 0;
		else
			retval = -EBUSY;
	} while (0);

	if (retval == 0) {
		GDL_LOGXD_ONF(link_id, "prev_state = %s, retval = %d",
			gps_dl_link_state_name(state), retval);
	} else {
		GDL_LOGXW_ONF(link_id, "prev_state = %s, retval = %d",
			gps_dl_link_state_name(state), retval);
	}
	return retval;
}

void gps_dl_link_waitable_init(struct gps_each_link_waitable *p,
	enum gps_each_link_waitable_type type)
{
	p->type = type;
	p->fired = false;
#if GPS_DL_ON_LINUX
	init_waitqueue_head(&p->wq);
#endif
}

void gps_dl_link_waitable_reset(enum gps_dl_link_id_enum link_id, enum gps_each_link_waitable_type type)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);

	/* TOOD: check NULL and boundary */
	p_link->waitables[type].fired = false;
}

#define GDL_TEST_TRUE_AND_SET_FALSE(x, x_old) \
	do {                \
		x_old = x;      \
		if (x_old) {    \
			x = false;  \
	} } while (0)

#define GDL_TEST_FALSE_AND_SET_TRUE(x, x_old) \
	do {                \
		x_old = x;      \
		if (!x_old) {   \
			x = true;   \
	} } while (0)

enum GDL_RET_STATUS gps_dl_link_wait_on(struct gps_each_link_waitable *p, long *p_sigval)
{
#if GPS_DL_ON_LINUX
	long val;
	bool is_fired;

	p->waiting = true;
	/* TODO: check race conditions? */
	GDL_TEST_TRUE_AND_SET_FALSE(p->fired, is_fired);
	if (is_fired) {
		GDL_LOGD("waitable = %s, no wait return", gps_dl_waitable_type_name(p->type));
		p->waiting = false;
		return GDL_OKAY;
	}

	GDL_LOGD("waitable = %s, wait start", gps_dl_waitable_type_name(p->type));
	val = wait_event_interruptible(p->wq, p->fired);
	p->waiting = false;

	if (val) {
		GDL_LOGI("signaled by %ld", val);
		if (p_sigval)
			*p_sigval = val;
		p->waiting = false;
		return GDL_FAIL_SIGNALED;
	}

	p->fired = false;
	p->waiting = false;
	GDL_LOGD("waitable = %s, wait done", gps_dl_waitable_type_name(p->type));
	return GDL_OKAY;
#else
	return GDL_FAIL_NOT_SUPPORT;
#endif
}

enum GDL_RET_STATUS gps_dl_link_try_wait_on(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_waitable_type type)
{
	struct gps_each_link *p_link;
	struct gps_each_link_waitable *p;
	bool is_fired;

	p_link = gps_dl_link_get(link_id);
	p = &p_link->waitables[type];

	GDL_TEST_TRUE_AND_SET_FALSE(p->fired, is_fired);
	if (is_fired) {
		GDL_LOGD("waitable = %s, okay", gps_dl_waitable_type_name(p->type));
		p->waiting = false;
		return GDL_OKAY;
	}

	return GDL_FAIL;
}

void gps_dl_link_wake_up(struct gps_each_link_waitable *p)
{
	bool is_fired;

	ASSERT_NOT_NULL(p, GDL_VOIDF());

	if (!p->waiting) {
		if (p->type == GPS_DL_WAIT_WRITE || p->type == GPS_DL_WAIT_READ) {
			/* normal case for read/write, not show warning */
			GDL_LOGD("waitable = %s, nobody waiting",
				gps_dl_waitable_type_name(p->type));
		} else {
			/* not return, just show warning */
			GDL_LOGW("waitable = %s, nobody waiting",
				gps_dl_waitable_type_name(p->type));
		}
	}

	GDL_TEST_FALSE_AND_SET_TRUE(p->fired, is_fired);
	GDL_LOGD("waitable = %s, fired = %d", gps_dl_waitable_type_name(p->type), is_fired);

	if (!is_fired) {
#if GPS_DL_ON_LINUX
		wake_up(&p->wq);
#else
#endif
	}
}

/* TODO: determine return value type */
int gps_each_link_write(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len)
{
	return gps_each_link_write_with_opt(link_id, buf, len, true);
}

#define GPS_DL_READ_SHOW_BUF_MAX_LEN (32)
int gps_each_link_write_with_opt(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len, bool wait_tx_done)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum GDL_RET_STATUS gdl_ret;
	long sigval = 0;

	if (NULL == p)
		return -1;

	if (len > p->tx_dma_buf.len)
		return -1;

	if (gps_each_link_get_state(link_id) != LINK_OPENED) {
		GDL_LOGXW_DRW(link_id, "not opened, drop the write data len = %d", len);
		return -EBUSY;
	}

	if (len <= GPS_DL_READ_SHOW_BUF_MAX_LEN)
		gps_dl_hal_show_buf("wr_buf", buf, len);
	else
		GDL_LOGXD_DRW(link_id, "wr_buf, len = %d", len);

	while (1) {
		gdl_ret = gdl_dma_buf_put(&p->tx_dma_buf, buf, len);

		if (gdl_ret == GDL_OKAY) {
			gps_dl_link_event_send(GPS_DL_EVT_LINK_WRITE, link_id);
#if (GPS_DL_NO_USE_IRQ == 1)
			if (wait_tx_done) {
				do {
					gps_dl_hal_a2d_tx_dma_wait_until_done_and_stop_it(
						link_id, GPS_DL_RW_NO_TIMEOUT, false);
					gps_dl_hal_event_send(GPS_DL_HAL_EVT_A2D_TX_DMA_DONE, link_id);
					/* for case tx transfer_max > 0, GPS_DL_HAL_EVT_A2D_TX_DMA_DONE may */
					/* start anthor dma session again, need to loop again until all data done */
				} while (!gps_dma_buf_is_empty(&p->tx_dma_buf));
			}
#endif
			return 0;
		} else if (gdl_ret == GDL_FAIL_NOSPACE || gdl_ret == GDL_FAIL_BUSY ||
			gdl_ret == GDL_FAIL_NOENTRY) {
			/* TODO: */
			/* 1. note: BUSY stands for others thread is do write, it should be impossible */
			/* - If wait on BUSY, should wake up the waitings or return eno_again? */
			/* 2. note: NOSPACE stands for need wait for tx dma working done */
			gps_dma_buf_show(&p->tx_dma_buf, false);
			GDL_LOGXD(link_id,
				"wait due to gdl_dma_buf_put ret = %s", gdl_ret_to_name(gdl_ret));
			gdl_ret = gps_dl_link_wait_on(&p->waitables[GPS_DL_WAIT_WRITE], &sigval);
			if (gdl_ret == GDL_FAIL_SIGNALED)
				break;
		} else {
			gps_dma_buf_show(&p->tx_dma_buf, true);
			GDL_LOGXW(link_id,
				"fail due to gdl_dma_buf_put ret = %s", gdl_ret_to_name(gdl_ret));
			break;
		}
	}

	return -1;
}

int gps_each_link_read(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len) {
	return gps_each_link_read_with_timeout(link_id, buf, len, GPS_DL_RW_NO_TIMEOUT, NULL);
}

int gps_each_link_read_with_timeout(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len, int timeout_usec, bool *p_is_nodata)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum GDL_RET_STATUS gdl_ret;
#if (GPS_DL_NO_USE_IRQ == 0)
	long sigval = 0;
#endif
	unsigned int data_len;

	if (NULL == p)
		return -1;

	while (1) {
		gdl_ret = gdl_dma_buf_get(&p->rx_dma_buf, buf, len, &data_len, p_is_nodata);

		if (gdl_ret == GDL_OKAY) {
			if (data_len <= GPS_DL_READ_SHOW_BUF_MAX_LEN)
				gps_dl_hal_show_buf("rd_buf", buf, data_len);
			else
				GDL_LOGXD_DRW(link_id, "rd_buf, len = %d", data_len);

			gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_DMA_BUF);
			if (p->rx_dma_buf.has_pending_rx) {
				p->rx_dma_buf.has_pending_rx = false;
				gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_DMA_BUF);

				GDL_LOGXI_DRW(link_id, "has pending rx, trigger again");
				gps_dl_hal_event_send(GPS_DL_HAL_EVT_D2A_RX_HAS_DATA, link_id);
			} else
				gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_DMA_BUF);

			return data_len;
		} else if (gdl_ret == GDL_FAIL_NODATA) {
			GDL_LOGXD_DRW(link_id, "gdl_dma_buf_get no data and wait");
#if (GPS_DL_NO_USE_IRQ == 1)
			gdl_ret = gps_dl_hal_wait_and_handle_until_usrt_has_data(
				link_id, timeout_usec);
			if (gdl_ret == GDL_FAIL_TIMEOUT)
				return -1;

			gdl_ret = gps_dl_hal_wait_and_handle_until_usrt_has_nodata_or_rx_dma_done(
				link_id, timeout_usec, true);
			if (gdl_ret == GDL_FAIL_TIMEOUT)
				return -1;
			continue;
#else
			gdl_ret = gps_dl_link_wait_on(&p->waitables[GPS_DL_WAIT_READ], &sigval);
			if (gdl_ret == GDL_FAIL_SIGNALED || gdl_ret == GDL_FAIL_NOT_SUPPORT)
				return -1;
#endif
		} else {
			GDL_LOGXW_DRW(link_id, "gdl_dma_buf_get fail %s", gdl_ret_to_name(gdl_ret));
			return -1;
		}
	}

	return 0;
}

void gps_dl_link_event_send(enum gps_dl_link_event_id evt,
	enum gps_dl_link_id_enum link_id)
{
#if (GPS_DL_HAS_CTRLD == 0)
	gps_dl_link_event_proc(evt, link_id);
#else
	{
		struct gps_dl_osal_lxop *pOp;
		struct gps_dl_osal_signal *pSignal;
		int iRet;

		pOp = gps_dl_get_free_op();
		if (!pOp)
			return;

		pSignal = &pOp->signal;
		pSignal->timeoutValue = 0;/* send data need to wait ?ms */
		if (link_id < GPS_DATA_LINK_NUM) {
			pOp->op.opId = GPS_DL_OPID_LINK_EVENT_PROC;
			pOp->op.au4OpData[0] = link_id;
			pOp->op.au4OpData[1] = evt;
			iRet = gps_dl_put_act_op(pOp);
		} else {
			gps_dl_put_op_to_free_queue(pOp);
			/*printf error msg*/
			return;
		}
	}
#endif
}

void gps_dl_link_irq_set(enum gps_dl_link_id_enum link_id, bool enable)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);
	bool dma_working, pending_rx;
	bool bypass_unmask_irq;

	if (enable) {
		gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_HAS_DATA, GPS_DL_IRQ_CTRL_FROM_THREAD);
		gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_HAS_NODATA, GPS_DL_IRQ_CTRL_FROM_THREAD);

		/* check if MCUB ROM ready */
		if (gps_dl_test_mask_mcub_irq_on_open_get(link_id)) {
			GDL_LOGXE(link_id, "test mask mcub irq, not unmask irq and wait reset");
			gps_dl_hal_set_mcub_irq_dis_flag(link_id, true);
			gps_dl_test_mask_mcub_irq_on_open_set(link_id, false);
		} else if (!gps_dl_hal_mcub_flag_handler(link_id)) {
			GDL_LOGXE(link_id, "mcub_flag_handler not okay, not unmask irq and wait reset");
			gps_dl_hal_set_mcub_irq_dis_flag(link_id, true);
		} else {
			gps_dl_irq_each_link_unmask(link_id,
				GPS_DL_IRQ_TYPE_MCUB, GPS_DL_IRQ_CTRL_FROM_THREAD);
		}
	} else {
		if (gps_dl_hal_get_mcub_irq_dis_flag(link_id)) {
			GDL_LOGXW(link_id, "mcub irq already disable, bypass mask irq");
			gps_dl_hal_set_mcub_irq_dis_flag(link_id, false);
		} else {
			gps_dl_irq_each_link_mask(link_id,
				GPS_DL_IRQ_TYPE_MCUB, GPS_DL_IRQ_CTRL_FROM_THREAD);
		}

		bypass_unmask_irq = false;
		if (gps_dl_hal_get_irq_dis_flag(link_id, GPS_DL_IRQ_TYPE_HAS_DATA)) {
			GDL_LOGXW(link_id, "hasdata irq already disable, bypass mask irq");
			gps_dl_hal_set_irq_dis_flag(link_id, GPS_DL_IRQ_TYPE_HAS_DATA, false);
			bypass_unmask_irq = true;
		}

		gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_DMA_BUF);
		dma_working = p_link->rx_dma_buf.dma_working_entry.is_valid;
		pending_rx = p_link->rx_dma_buf.has_pending_rx;
		if (dma_working || pending_rx) {
			p_link->rx_dma_buf.has_pending_rx = false;
			gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_DMA_BUF);

			/* It means this irq has already masked, */
			/* DON'T mask again, otherwise twice unmask might be needed */
			GDL_LOGXW(link_id,
				"has dma_working = %d, pending rx = %d, bypass mask irq",
				dma_working, pending_rx);
		} else {
			gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_DMA_BUF);
			if (!bypass_unmask_irq) {
				gps_dl_irq_each_link_mask(link_id,
					GPS_DL_IRQ_TYPE_HAS_DATA, GPS_DL_IRQ_CTRL_FROM_THREAD);
			}
		}

		/* TODO: avoid twice mask need to be handled if HAS_CTRLD */
		gps_dl_irq_each_link_mask(link_id, GPS_DL_IRQ_TYPE_HAS_NODATA, GPS_DL_IRQ_CTRL_FROM_THREAD);
	}
}

void gps_dl_link_pre_off_setting(enum gps_dl_link_id_enum link_id)
{
	/*
	 * The order is important:
	 * 1. disallow write, avoiding to start dma
	 * 2. stop tx/rx dma and mask dma irq if it is last link
	 * 3. mask link's irqs
	 * 4. set inactive after all irq mask done
	 * (at this time isr can check inactive and unmask irq safely due to step 3 already mask irqs)
	 */
	gps_dl_link_set_ready_to_write(link_id, false);
	gps_dl_hal_link_confirm_dma_stop(link_id);
	gps_dl_link_irq_set(link_id, false);
	gps_each_link_set_active(link_id, false);
}

void gps_dl_link_event_proc(enum gps_dl_link_event_id evt,
	enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);
	bool show_log = false;
	bool show_log2 = false;
	unsigned long j0, j1;
	int ret;
	enum gps_dsp_state_t dsp_state;

	j0 = jiffies;
	GDL_LOGXD_EVT(link_id, "evt = %s", gps_dl_link_event_name(evt));

	switch (evt) {
	case GPS_DL_EVT_LINK_OPEN:
		/* show_log = gps_dl_set_show_reg_rw_log(true); */
		gps_each_dsp_reg_gourp_read_init(link_id);
		gps_each_link_inc_session_id(link_id);
		gps_each_link_set_active(link_id, true);
		gps_each_link_set_bool_flag(link_id, LINK_NEED_A2Z_DUMP, false);

		ret = gps_dl_hal_conn_power_ctrl(link_id, 1);
		if (ret != 0) {
			gps_dl_link_open_ack(link_id, false, false);
			break;
		}

		ret = gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_POWER_ON);
		if (ret != 0) {
			gps_dl_link_open_ack(link_id, false, false);
			break;
		}

		gps_dsp_fsm(GPS_DSP_EVT_FUNC_ON, link_id);
		gps_dl_link_irq_set(link_id, true);
#if GPS_DL_NO_USE_IRQ
		gps_dl_wait_us(1000); /* wait 1ms */
#endif
		/* set ready to write before open ack, otherwise need to check pending tx data
		 * gps_dl_link_set_ready_to_write(link_id, true);
		 * move it to DSP reset done handler
		 */
		gps_dl_link_open_ack(link_id, true, false); /* TODO: ack on DSP reset done */
		/* gps_dl_set_show_reg_rw_log(show_log); */
		break;
	case GPS_DL_EVT_LINK_LEAVE_DPSTOP:
		gps_each_dsp_reg_gourp_read_init(link_id);
		gps_each_link_inc_session_id(link_id);
		gps_each_link_set_active(link_id, true);
		gps_each_link_set_bool_flag(link_id, LINK_NEED_A2Z_DUMP, false);
		ret = gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_LEAVE_DPSTOP);
		if (ret != 0)
			gps_dl_link_open_ack(link_id, false, true);
		else {
			gps_dsp_fsm(GPS_DSP_EVT_HW_STOP_EXIT, link_id);
			gps_dl_link_irq_set(link_id, true);
			gps_dl_link_open_ack(link_id, true, true);
		}
		break;
	case GPS_DL_EVT_LINK_LEAVE_DPSLEEP:
		gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_LEAVE_DPSLEEP);
		gps_dl_link_irq_set(link_id, true);
		break;
	case GPS_DL_EVT_LINK_ENTER_DPSLEEP:
		gps_dl_link_pre_off_setting(link_id);
		gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_ENTER_DPSLEEP);
		break;
	case GPS_DL_EVT_LINK_ENTER_DPSTOP:
		dsp_state = gps_dsp_state_get(link_id);
		if ((GPS_DSP_ST_WORKING != dsp_state) && (GPS_DSP_ST_RESET_DONE != dsp_state)) {
			/* TODO: ever working check */
			GDL_LOGXE(link_id, "not enter dpstop due to dsp state = %s",
				gps_dl_dsp_state_name(dsp_state));

			/* TODO: ack fail */
			gps_dl_link_close_ack(link_id, true);
			break;
		}

		if (GPS_DSP_ST_WORKING == dsp_state) {
			GDL_LOGXW(link_id, "enter dpstop with dsp state = %s",
				gps_dl_dsp_state_name(dsp_state));
		}

		gps_dl_hal_set_need_clk_ext_flag(link_id,
			gps_each_link_get_bool_flag(link_id, LINK_SUSPEND_TO_CLK_EXT));

		gps_dl_link_pre_off_setting(link_id);
		/* TODO: handle fail */
		gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_ENTER_DPSTOP);
		gps_dsp_fsm(GPS_DSP_EVT_HW_STOP_REQ, link_id);
		gps_each_link_context_clear(link_id);
#if GPS_DL_ON_LINUX
		gps_dma_buf_reset(&p_link->tx_dma_buf);
		gps_dma_buf_reset(&p_link->rx_dma_buf);
#endif
		gps_dl_link_close_ack(link_id, true);
		break;
	case GPS_DL_EVT_LINK_DSP_ROM_READY_TIMEOUT:
		/* check again mcub not ready triggered */
		if (false)
			break; /* wait hal handle it */

		/* true: */
		if (!gps_each_link_change_state_from(link_id, LINK_OPENED, LINK_RESETTING)) {
			/* no handle it again */
			break;
		}
		/* TODO: go and do close */
	case GPS_DL_EVT_LINK_CLOSE:
	case GPS_DL_EVT_LINK_RESET_DSP:
	case GPS_DL_EVT_LINK_RESET_GPS:
	case GPS_DL_EVT_LINK_PRE_CONN_RESET:
		if (evt != GPS_DL_EVT_LINK_CLOSE)
			show_log = gps_dl_set_show_reg_rw_log(true);

		/* handle open fail case */
		if (!gps_each_link_get_bool_flag(link_id, LINK_OPEN_RESULT_OKAY)) {
			GDL_LOGXD(link_id, "not open okay, just power off for %s",
				gps_dl_link_event_name(evt));

			gps_each_link_set_active(link_id, false);
			gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_POWER_OFF);
			gps_dl_hal_conn_power_ctrl(link_id, 0);
			goto _close_or_reset_ack;
		}

		/* to avoid twice enter */
		if (GPS_DSP_ST_OFF == gps_dsp_state_get(link_id)) {
			GDL_LOGXD(link_id, "dsp state is off, do nothing for %s",
				gps_dl_link_event_name(evt));

			if (evt != GPS_DL_EVT_LINK_CLOSE)
				gps_dl_set_show_reg_rw_log(show_log);

			goto _close_or_reset_ack;
		} else if (GPS_DSP_ST_HW_STOP_MODE == gps_dsp_state_get(link_id)) {
			/* exit deep stop mode and turn off it
			 * before exit deep stop, need clear pwr stat to make sure dsp is in hold-on state
			 * after exit deep stop mode.
			 */
			gps_dl_hal_link_clear_hw_pwr_stat(link_id);
			gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_LEAVE_DPSTOP);
		} else {
			/* make sure current link's DMAs are stopped and mask the IRQs */
			gps_dl_link_pre_off_setting(link_id);
		}
		gps_dl_hal_set_need_clk_ext_flag(link_id, false);

		if (evt != GPS_DL_EVT_LINK_CLOSE) {
			/* try to dump host csr info if not normal close operation */
			if (gps_dl_conninfra_is_okay_or_handle_it(NULL, true))
				gps_dl_hw_dump_host_csr_gps_info(true);
		}

		if (gps_each_link_get_bool_flag(link_id, LINK_NEED_A2Z_DUMP)) {
			show_log2 = gps_dl_set_show_reg_rw_log(true);
			gps_dl_hw_do_gps_a2z_dump();
			gps_dl_set_show_reg_rw_log(show_log2);
		}

		gps_dl_hal_link_power_ctrl(link_id, GPS_DL_HAL_POWER_OFF);
		gps_dl_hal_conn_power_ctrl(link_id, 0);

		gps_dsp_fsm(GPS_DSP_EVT_FUNC_OFF, link_id);

		gps_each_link_context_clear(link_id);
#if GPS_DL_ON_LINUX
		gps_dma_buf_reset(&p_link->tx_dma_buf);
		gps_dma_buf_reset(&p_link->rx_dma_buf);
#endif

_close_or_reset_ack:
		if (evt != GPS_DL_EVT_LINK_CLOSE)
			gps_dl_set_show_reg_rw_log(show_log);

		if (GPS_DL_EVT_LINK_CLOSE == evt)
			gps_dl_link_close_ack(link_id, false); /* TODO: check fired race */
		else
			gps_dl_link_reset_ack(link_id);
		break;

	case GPS_DL_EVT_LINK_POST_CONN_RESET:
		gps_dl_link_on_post_conn_reset(link_id);
		break;

	case GPS_DL_EVT_LINK_WRITE:
		/* gps_dl_hw_print_usrt_status(link_id); */
		if (gps_dl_link_is_ready_to_write(link_id))
			gps_dl_link_start_tx_dma_if_has_data(link_id);
		else
			GDL_LOGXW(link_id, "too early writing");
		break;

	case GPS_DL_EVT_LINK_PRINT_HW_STATUS:
	case GPS_DL_EVT_LINK_PRINT_DATA_STATUS:
		if (!gps_each_link_is_active(link_id)) {
			GDL_LOGXW(link_id, "inactive, do not dump hw status");
			break;
		}

		gps_dma_buf_show(&p_link->rx_dma_buf, true);
		gps_dma_buf_show(&p_link->tx_dma_buf, true);
		if (!gps_dl_conninfra_is_okay_or_handle_it(NULL, true))
			break;

		show_log = gps_dl_set_show_reg_rw_log(true);
		if (evt == GPS_DL_EVT_LINK_PRINT_HW_STATUS) {
			gps_dl_hw_dump_host_csr_gps_info(true);
			gps_dl_hw_print_hw_status(link_id, true);
			gps_each_dsp_reg_gourp_read_start(link_id, true, 4);
		} else {
			gps_dl_hw_print_hw_status(link_id, false);
			gps_each_dsp_reg_gourp_read_start(link_id, true, 2);
		}
		gps_dl_set_show_reg_rw_log(show_log);
		break;

	case GPS_DL_EVT_LINK_DSP_FSM_TIMEOUT:
		gps_dsp_fsm(GPS_DSP_EVT_CTRL_TIMER_EXPIRE, link_id);
		break;
#if 0
	case GPS_DL_EVT_LINK_RESET_GPS:
		/* turn off GPS power directly */
		break;

	case GPS_DL_EVT_LINK_PRE_CONN_RESET:
		/* turn off Connsys power directly
		 * 1. no need to do anything, just make sure the message queue is empty
		 * 2. how to handle ctrld block issue
		 */
		/* gps_dl_link_open_ack(link_id); */
		break;
#endif
	default:
		break;
	}

	j1 = jiffies;
	GDL_LOGXI_EVT(link_id, "evt = %s, dj = %lu", gps_dl_link_event_name(evt), j1 - j0);
}

void gps_each_link_mutexes_init(struct gps_each_link *p)
{
	enum gps_each_link_mutex i;

	for (i = 0; i < GPS_DL_MTX_NUM; i++)
		gps_dl_osal_sleepable_lock_init(&p->mutexes[i]);
}

void gps_each_link_mutexes_deinit(struct gps_each_link *p)
{
	enum gps_each_link_mutex i;

	for (i = 0; i < GPS_DL_MTX_NUM; i++)
		gps_dl_osal_sleepable_lock_deinit(&p->mutexes[i]);
}

void gps_each_link_spin_locks_init(struct gps_each_link *p)
{
	enum gps_each_link_spinlock i;

	for (i = 0; i < GPS_DL_SPINLOCK_NUM; i++)
		gps_dl_osal_unsleepable_lock_init(&p->spin_locks[i]);
}

void gps_each_link_spin_locks_deinit(struct gps_each_link *p)
{
#if 0
	enum gps_each_link_spinlock i;

	for (i = 0; i < GPS_DL_SPINLOCK_NUM; i++)
		osal_unsleepable_lock_deinit(&p->spin_locks[i]);
#endif
}

void gps_each_link_mutex_take(enum gps_dl_link_id_enum link_id, enum gps_each_link_mutex mtx_id)
{
	/* TODO: check range */
	struct gps_each_link *p = gps_dl_link_get(link_id);

	/* TODO: handle killed */
	gps_dl_osal_lock_sleepable_lock(&p->mutexes[mtx_id]);
}

void gps_each_link_mutex_give(enum gps_dl_link_id_enum link_id, enum gps_each_link_mutex mtx_id)
{
	/* TODO: check range */
	struct gps_each_link *p = gps_dl_link_get(link_id);

	gps_dl_osal_unlock_sleepable_lock(&p->mutexes[mtx_id]);
}

void gps_each_link_spin_lock_take(enum gps_dl_link_id_enum link_id, enum gps_each_link_spinlock spin_lock_id)
{
	/* TODO: check range */
	struct gps_each_link *p = gps_dl_link_get(link_id);

	gps_dl_osal_lock_unsleepable_lock(&p->spin_locks[spin_lock_id]);
}

void gps_each_link_spin_lock_give(enum gps_dl_link_id_enum link_id, enum gps_each_link_spinlock spin_lock_id)
{
	/* TODO: check range */
	struct gps_each_link *p = gps_dl_link_get(link_id);

	gps_dl_osal_unlock_unsleepable_lock(&p->spin_locks[spin_lock_id]);
}

int gps_each_link_take_big_lock(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_lock_reason reason)
{
	gps_each_link_mutex_take(link_id, GPS_DL_MTX_BIG_LOCK);
	return 0;
}

int gps_each_link_give_big_lock(enum gps_dl_link_id_enum link_id)
{
	gps_each_link_mutex_give(link_id, GPS_DL_MTX_BIG_LOCK);
	return 0;
}

enum gps_each_link_state_enum gps_each_link_get_state(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum gps_each_link_state_enum state;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	state = p->state_for_user;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	return state;
}

void gps_each_link_set_state(enum gps_dl_link_id_enum link_id, enum gps_each_link_state_enum state)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum gps_each_link_state_enum pre_state;


	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	pre_state = p->state_for_user;
	p->state_for_user = state;
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	GDL_LOGXI_STA(link_id, "state change: %s -> %s",
		gps_dl_link_state_name(pre_state), gps_dl_link_state_name(state));
}

bool gps_each_link_change_state_from(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_state_enum from, enum gps_each_link_state_enum to)
{
	bool is_okay = false;
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum gps_each_link_state_enum pre_state;
	enum gps_each_link_reset_level old_level, new_level;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	pre_state = p->state_for_user;
	if (from == pre_state) {
		p->state_for_user = to;
		is_okay = true;

		if (to == LINK_RESETTING) {
			old_level = p->reset_level;
			if (old_level < GPS_DL_RESET_LEVEL_GPS_SINGLE_LINK)
				p->reset_level = GPS_DL_RESET_LEVEL_GPS_SINGLE_LINK;
			new_level = p->reset_level;
		}
	}
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	if (is_okay && (to == LINK_RESETTING)) {
		GDL_LOGXI_STA(link_id, "state change: %s -> %s, okay, level: %d -> %d",
			gps_dl_link_state_name(from), gps_dl_link_state_name(to),
			old_level, new_level);
	} else if (is_okay) {
		GDL_LOGXI_STA(link_id, "state change: %s -> %s, okay",
			gps_dl_link_state_name(from), gps_dl_link_state_name(to));
	} else {
		GDL_LOGXW_STA(link_id, "state change: %s -> %s, fail on pre_state = %s",
			gps_dl_link_state_name(from), gps_dl_link_state_name(to),
			gps_dl_link_state_name(pre_state));
	}

	return is_okay;
}

bool gps_dl_link_start_tx_dma_if_has_data(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);
	struct gdl_dma_buf_entry dma_buf_entry;
	enum GDL_RET_STATUS gdl_ret;
	bool tx_dma_started;

	gdl_ret = gdl_dma_buf_get_data_entry(&p_link->tx_dma_buf, &dma_buf_entry);

	if (gdl_ret == GDL_OKAY) {
		/* wait until dsp recevie last data done or timeout(10ms)
		 * TODO: handle timeout case
		 */
		gps_dl_hw_poll_usrt_dsp_rx_empty(link_id);
		gps_dl_hal_a2d_tx_dma_claim_emi_usage(link_id, true);
		gps_dl_hal_a2d_tx_dma_start(link_id, &dma_buf_entry);
		tx_dma_started = true;
	} else {
		GDL_LOGD("gdl_dma_buf_get_data_entry ret = %s", gdl_ret_to_name(gdl_ret));
		tx_dma_started = false;
	}

	return tx_dma_started;
}

int gps_dl_link_get_clock_flag(void)
{
	return gps_dl_hal_get_clock_flag();
}

