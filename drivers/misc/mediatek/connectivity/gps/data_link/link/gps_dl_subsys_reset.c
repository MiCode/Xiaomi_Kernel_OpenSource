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

#include "gps_dl_context.h"
#include "gps_dl_subsys_reset.h"
#include "gps_each_link.h"
#include "gps_dl_name_list.h"
#include "gps_dl_hw_api.h"

#if GPS_DL_HAS_CONNINFRA_DRV
#include "conninfra.h"
#endif

bool gps_dl_reset_level_is_none(enum gps_dl_link_id_enum link_id)
{
	struct gps_each_link *p = gps_dl_link_get(link_id);
	enum gps_each_link_state_enum state;
	enum gps_each_link_reset_level level;
	bool is_none;

	gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
	state = p->state_for_user;
	level = p->reset_level;
	is_none = (level == GPS_DL_RESET_LEVEL_NONE);
	gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

	if (!is_none)
		GDL_LOGW("state = %s, level = %d", gps_dl_link_state_name(state), level);

	return is_none;
}

enum GDL_RET_STATUS gps_dl_reset_level_set_and_trigger(
	enum gps_each_link_reset_level level, bool wait_reset_done)
{
	enum gps_dl_link_id_enum link_id;
	struct gps_each_link *p;
	enum gps_each_link_state_enum old_state, new_state;
	enum gps_each_link_reset_level old_level, new_level;
	bool need_wait[GPS_DATA_LINK_NUM] = {false};
	bool to_send_reset_event;
	long sigval;
	enum GDL_RET_STATUS wait_status;

	if (level != GPS_DL_RESET_LEVEL_GPS_SUBSYS && level !=  GPS_DL_RESET_LEVEL_CONNSYS) {
		GDL_LOGW("level = %d, do nothing and return", level);
		return GDL_FAIL_INVAL;
	}

	if (wait_reset_done)
		; /* TODO: take mutex to allow pending more waiter */

	for (link_id = 0; link_id < GPS_DATA_LINK_NUM; link_id++) {
		p = gps_dl_link_get(link_id);
		to_send_reset_event = false;

		gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
		old_state = p->state_for_user;
		old_level = p->reset_level;

		switch (old_state) {
		case LINK_CLOSED:
			need_wait[link_id] = false;
			p->state_for_user = LINK_DISABLED;
			p->reset_level = level;

			/* Send reset event to ctld:
			 *
			 * for GPS_DL_RESET_LEVEL_GPS_SUBSYS ctrld do nothing but
			 *   just change state from DISABLED back to CLOSED
			 *
			 * for GPS_DL_RESET_LEVEL_CONNSYS ctrld do nothing but
			 *   just change state from DISABLED state back to CLOSED
			 */
			to_send_reset_event = true;
			break;

		case LINK_OPENING:
		case LINK_OPENED:
		case LINK_CLOSING:
		case LINK_RESET_DONE:
		case LINK_RESUMING:
		case LINK_SUSPENDING:
		case LINK_SUSPENDED:
			need_wait[link_id] = true;
			p->state_for_user = LINK_RESETTING;
			p->reset_level = level;
			to_send_reset_event = true;
			break;

		case LINK_RESETTING:
			need_wait[link_id] = true;
			if (old_level < level)
				p->reset_level = level;
			break;

		case LINK_DISABLED:
		case LINK_UNINIT:
			need_wait[link_id] = false;
			break;

		default:
			need_wait[link_id] = false;
			break;
		}

		new_state = p->state_for_user;
		new_level = p->reset_level;
		gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

		if (to_send_reset_event) {
			gps_dl_link_waitable_reset(link_id, GPS_DL_WAIT_RESET);
			if (level == GPS_DL_RESET_LEVEL_CONNSYS)
				gps_dl_link_event_send(GPS_DL_EVT_LINK_PRE_CONN_RESET, link_id);
			else
				gps_dl_link_event_send(GPS_DL_EVT_LINK_RESET_GPS, link_id);
		}

		GDL_LOGXE_STA(link_id,
			"state change: %s -> %s, level = %d (%d -> %d), is_sent = %d, to_wait = %d",
			gps_dl_link_state_name(old_state), gps_dl_link_state_name(new_state),
			level, old_level, new_level,
			to_send_reset_event, need_wait[link_id]);
	}

	if (!wait_reset_done) {
		GDL_LOGE("force no wait");
		return GDL_OKAY;
	}

	for (link_id = 0; link_id < GPS_DATA_LINK_NUM; link_id++) {
		if (!need_wait[link_id])
			continue;

		sigval = 0;
		p = gps_dl_link_get(link_id);
		wait_status = gps_dl_link_wait_on(&p->waitables[GPS_DL_WAIT_RESET], &sigval);
		if (wait_status == GDL_FAIL_SIGNALED) {
			GDL_LOGXE(link_id, "sigval = %ld", sigval);
			return GDL_FAIL_SIGNALED;
		}

		GDL_LOGXE(link_id, "wait ret = %s", gdl_ret_to_name(wait_status));
	}

	if (wait_reset_done)
		; /* TODO: take mutex to allow pending more waiter */

	return GDL_OKAY;
}

int gps_dl_trigger_gps_subsys_reset(bool wait_reset_done)
{
	enum GDL_RET_STATUS ret_status;

	ret_status = gps_dl_reset_level_set_and_trigger(GPS_DL_RESET_LEVEL_GPS_SUBSYS, wait_reset_done);
	if (ret_status != GDL_OKAY) {
		GDL_LOGE("status %s is not okay, return -1", gdl_ret_to_name(ret_status));
		return -1;
	}
	return 0;
}

void gps_dl_trigger_gps_print_hw_status(void)
{
	GDL_LOGE("");
	gps_dl_link_event_send(GPS_DL_EVT_LINK_PRINT_HW_STATUS, GPS_DATA_LINK_ID0);
	gps_dl_link_event_send(GPS_DL_EVT_LINK_PRINT_HW_STATUS, GPS_DATA_LINK_ID1);
}

void gps_dl_trigger_gps_print_data_status(void)
{
	GDL_LOGE("");
	gps_dl_link_event_send(GPS_DL_EVT_LINK_PRINT_DATA_STATUS, GPS_DATA_LINK_ID0);
	gps_dl_link_event_send(GPS_DL_EVT_LINK_PRINT_DATA_STATUS, GPS_DATA_LINK_ID1);
}

void gps_dl_handle_connsys_reset_done(void)
{
	enum gps_dl_link_id_enum link_id;
	struct gps_each_link *p;
	enum gps_each_link_state_enum state;
	enum gps_each_link_reset_level level;
	bool to_send_reset_event;

	for (link_id = 0; link_id < GPS_DATA_LINK_NUM; link_id++) {
		p = gps_dl_link_get(link_id);
		to_send_reset_event = false;

		gps_each_link_spin_lock_take(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);
		state = p->state_for_user;
		level = p->reset_level;

		if (level == GPS_DL_RESET_LEVEL_CONNSYS) {
			if (state == LINK_DISABLED || state == LINK_RESETTING)
				to_send_reset_event = true;
		}
		gps_each_link_spin_lock_give(link_id, GPS_DL_SPINLOCK_FOR_LINK_STATE);

		if (to_send_reset_event)
			gps_dl_link_event_send(GPS_DL_EVT_LINK_POST_CONN_RESET, link_id);

		GDL_LOGXE_STA(link_id, "state check: %s, level = %d, is_sent = %d",
			gps_dl_link_state_name(state), level, to_send_reset_event);
	}
}

int gps_dl_trigger_connsys_reset(void)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	int ret;

	GDL_LOGE("");
	ret = conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_GPS, "GPS debug");
	GDL_LOGE("conninfra_trigger_whole_chip_rst return = %d", ret);
#else
	GDL_LOGE("has no conninfra_drv");
#endif
	return 0;
}

#if GPS_DL_HAS_CONNINFRA_DRV
static bool gps_dl_connsys_is_resetting;
int gps_dl_on_pre_connsys_reset(enum consys_drv_type drv, char *reason)
{
	enum GDL_RET_STATUS ret_status;

	GDL_LOGE("already in resetting = %d", gps_dl_connsys_is_resetting);
	gps_dl_connsys_is_resetting = true;

	ret_status = gps_dl_reset_level_set_and_trigger(GPS_DL_RESET_LEVEL_CONNSYS, true);

	if (ret_status != GDL_OKAY) {
		GDL_LOGE("status %s is not okay, return -1", gdl_ret_to_name(ret_status));
		return -1;
	}

	return 0;
}

int gps_dl_on_post_connsys_reset(void)
{
	GDL_LOGE("already in resetting = %d", gps_dl_connsys_is_resetting);
	gps_dl_connsys_is_resetting = false;

	gps_dl_handle_connsys_reset_done();
	return 0;
}

struct sub_drv_ops_cb gps_dl_conninfra_ops_cb;
#endif

void gps_dl_register_conninfra_reset_cb(void)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	memset(&gps_dl_conninfra_ops_cb, 0, sizeof(gps_dl_conninfra_ops_cb));
	gps_dl_conninfra_ops_cb.rst_cb.pre_whole_chip_rst = gps_dl_on_pre_connsys_reset;
	gps_dl_conninfra_ops_cb.rst_cb.post_whole_chip_rst = gps_dl_on_post_connsys_reset;

	conninfra_sub_drv_ops_register(CONNDRV_TYPE_GPS, &gps_dl_conninfra_ops_cb);
#endif
}

void gps_dl_unregister_conninfra_reset_cb(void)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	conninfra_sub_drv_ops_unregister(CONNDRV_TYPE_GPS);
#endif
}

bool gps_dl_conninfra_is_readable(void)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	return (conninfra_reg_readable() != 0);
#else
	return true;
#endif
}

void gps_dl_conninfra_not_readable_show_warning(unsigned int host_addr)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	int readable;
	int hung_value = 0;

	readable = conninfra_reg_readable();
	if (readable)
		return;

	hung_value = conninfra_is_bus_hang();
	GDL_LOGW("readable = %d, hung_value = %d, before access 0x%08x",
		readable, hung_value, host_addr);
#endif
}

bool gps_dl_conninfra_is_okay_or_handle_it(int *p_hung_value, bool dump_on_hung_value_zero)
{
#if GPS_DL_HAS_CONNINFRA_DRV
	int readable;
	int hung_value = 0;
	bool trigger = false;
	int trigger_ret = 0;
	bool check_again;
	int check_cnt = 0;

	do {
		check_again = false;
		readable = conninfra_reg_readable();
		if (readable) {
			GDL_LOGD("readable = %d, okay", readable);
			return true;
		}

		hung_value = conninfra_is_bus_hang();
		if (p_hung_value != NULL)
			*p_hung_value = hung_value;

		/* hung_value > 0, need to trigger reset
		 * hung_value < 0, already in reset status
		 * hung_value = 0, connsys may not in proper status (such as conn_top_off is in sleep)
		 */
		if (hung_value > 0) {
			/* it's safe to cump gps host csr even hang value > 0 */
			gps_dl_hw_dump_host_csr_gps_info(true);

			trigger = true;
			trigger_ret = conninfra_trigger_whole_chip_rst(
				CONNDRV_TYPE_GPS, "GPS detect hung - case1");
		} else if (hung_value == 0) {
			if (dump_on_hung_value_zero)
				gps_dl_hw_dump_host_csr_gps_info(true);
			if (check_cnt < 1) {
				/* readable = 0 and hung_value = 0 may not be a stable state,
				 * check again to double confirm
				 */
				check_again = true;
			} else {
				/* trigger connsys reset if same result of checking again */
				trigger = true;
				trigger_ret = conninfra_trigger_whole_chip_rst(
					CONNDRV_TYPE_GPS, "GPS detect hung - case2");
			}
		} else {
			/* alreay in connsys resetting
			 * do nothing
			 */
		}

		check_cnt++;
		GDL_LOGE("cnt=%d, readable=%d, hung_value=0x%x, trigger_reset=%d(%d,%d)",
			check_cnt, readable, hung_value, trigger, trigger_ret, dump_on_hung_value_zero);
	} while (check_again);
	return false;
#else
	return true;
#endif
}


bool g_gps_dl_test_mask_mcub_irq_on_open_flag[GPS_DATA_LINK_NUM];
bool g_gps_dl_test_mask_hasdata_irq_flag[GPS_DATA_LINK_NUM];

void gps_dl_test_mask_mcub_irq_on_open_set(enum gps_dl_link_id_enum link_id, bool mask)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	g_gps_dl_test_mask_mcub_irq_on_open_flag[link_id] = mask;
}

bool gps_dl_test_mask_mcub_irq_on_open_get(enum gps_dl_link_id_enum link_id)
{
	ASSERT_LINK_ID(link_id, false);

	return g_gps_dl_test_mask_mcub_irq_on_open_flag[link_id];
}

void gps_dl_test_mask_hasdata_irq_set(enum gps_dl_link_id_enum link_id, bool mask)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	g_gps_dl_test_mask_hasdata_irq_flag[link_id] = mask;
}

bool gps_dl_test_mask_hasdata_irq_get(enum gps_dl_link_id_enum link_id)
{
	ASSERT_LINK_ID(link_id, false);

	return g_gps_dl_test_mask_hasdata_irq_flag[link_id];
}

