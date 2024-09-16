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
#ifndef _GPS_EACH_LINK_H
#define _GPS_EACH_LINK_H

#include "gps_dl_config.h"

#if GPS_DL_ON_LINUX
#include <linux/wait.h>
#endif

#if GPS_DL_ON_CTP
#include "gps_dl_ctp_osal.h"
#endif

#if GPS_DL_HAS_CTRLD
#include "gps_dl_ctrld.h"
#endif

#include "gps_dl_subsys_reset.h"
#include "gps_dl_dma_buf.h"

struct gps_each_link_cfg {
	int tx_buf_size;
	int rx_buf_size;
};

enum gps_each_link_waitable_type {
	GPS_DL_WAIT_OPEN_CLOSE,
	GPS_DL_WAIT_WRITE,
	GPS_DL_WAIT_READ,
	GPS_DL_WAIT_RESET,
	GPS_DL_WAIT_NUM
};

struct gps_each_link_state_list {
	bool is_ready_to_write;
	bool is_active;
	bool to_be_closed;
	bool is_resetting;
	bool open_result_okay;
	bool user_open;
	bool need_a2z_dump;
	bool suspend_to_clk_ext;
};

enum gps_each_link_bool_state {
	LINK_WRITE_READY,
	LINK_IS_ACTIVE,
	LINK_TO_BE_CLOSED,
	LINK_USER_OPEN,
	LINK_WAIT_RESET_DONE,
	LINK_IS_RESETTING,
	LINK_OPEN_RESULT_OKAY,
	LINK_NEED_A2Z_DUMP,
	LINK_SUSPEND_TO_CLK_EXT,
	BOOL_STATE_NUM
};

void gps_each_link_set_bool_flag(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_bool_state name, bool value);
bool gps_each_link_get_bool_flag(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_bool_state name);

bool gps_dl_link_is_ready_to_write(enum gps_dl_link_id_enum link_id);
void gps_dl_link_set_ready_to_write(enum gps_dl_link_id_enum link_id, bool is_ready);

bool gps_each_link_is_active(enum gps_dl_link_id_enum link_id);
void gps_each_link_set_active(enum gps_dl_link_id_enum link_id, bool is_active);

enum gps_each_link_wait_status {
	WAITING,
	OKAY,
	FAIL,
	SIGNAL,
};

struct gps_each_link_waitable {
#if GPS_DL_ON_LINUX
	/* TODO: use completion */
	wait_queue_head_t wq;
#endif
	bool fired;
	bool waiting;
	enum gps_each_link_wait_status status;
	enum gps_each_link_waitable_type type;
};

enum gps_each_link_mutex {
	GPS_DL_MTX_BIG_LOCK,
	GPS_DL_MTX_NUM
};

void gps_each_link_mutex_take(enum gps_dl_link_id_enum link_id, enum gps_each_link_mutex mtx_id);
void gps_each_link_mutex_give(enum gps_dl_link_id_enum link_id, enum gps_each_link_mutex mtx_id);

enum gps_each_link_spinlock {
	GPS_DL_SPINLOCK_FOR_LINK_STATE,
	GPS_DL_SPINLOCK_FOR_DMA_BUF,
	GPS_DL_SPINLOCK_NUM
};

void gps_each_link_spin_lock_take(enum gps_dl_link_id_enum link_id, enum gps_each_link_spinlock spin_lock_id);
void gps_each_link_spin_lock_give(enum gps_dl_link_id_enum link_id, enum gps_each_link_spinlock spin_lock_id);


enum gps_each_link_state_enum {
	LINK_UNINIT,
	LINK_CLOSED,
	LINK_OPENING,
	LINK_OPENED,
	LINK_CLOSING,
	LINK_RESETTING, /* Not distinguish EACH_LINK or WHOLE_GPS or WHOLE_CONNSYS */
	LINK_RESET_DONE,
	LINK_DISABLED,
	LINK_SUSPENDING,
	LINK_SUSPENDED,
	LINK_RESUMING,
	LINK_STATE_NUM
};

#define GPS_EACH_LINK_SID_MAX       (0x7FFFFFFE)
#define GPS_EACH_LINK_SID_NO_CHECK  (0xFFFFFFFF)
struct gps_each_link {
	struct gps_each_link_cfg cfg;
	struct gps_each_device *p_device;
	struct gps_dl_dma_buf tx_dma_buf;
	struct gps_dl_dma_buf rx_dma_buf;
	struct gps_each_link_waitable waitables[GPS_DL_WAIT_NUM];
	struct gps_dl_osal_sleepable_lock mutexes[GPS_DL_MTX_NUM];
	struct gps_dl_osal_unsleepable_lock spin_locks[GPS_DL_SPINLOCK_NUM];
	struct gps_each_link_state_list sub_states;
	enum gps_each_link_state_enum state_for_user;
	enum gps_each_link_reset_level reset_level;
	int session_id;
};

void gps_each_link_mutexes_init(struct gps_each_link *p);
void gps_each_link_mutexes_deinit(struct gps_each_link *p);
void gps_each_link_spin_locks_init(struct gps_each_link *p);
void gps_each_link_spin_locks_deinit(struct gps_each_link *p);


struct gps_common_context {
	struct gps_dl_osal_sleepable_lock big_lock;
};

/* only ctrld can change it */
struct gps_dl_ctrl_context {
	bool gps_reset;
	bool connsys_reset;
};

struct gps_each_link *gps_dl_link_get(enum gps_dl_link_id_enum link_id);

void gps_each_link_init(enum gps_dl_link_id_enum link_id);
void gps_each_link_deinit(enum gps_dl_link_id_enum link_id);
void gps_each_link_context_init(enum gps_dl_link_id_enum link_id);
void gps_each_link_context_clear(enum gps_dl_link_id_enum link_id);
void gps_each_link_inc_session_id(enum gps_dl_link_id_enum link_id);
int gps_each_link_get_session_id(enum gps_dl_link_id_enum link_id);

int gps_each_link_open(enum gps_dl_link_id_enum link_id);
void gps_dl_link_open_ack(enum gps_dl_link_id_enum link_id, bool okay, bool hw_resume);

enum gps_each_link_lock_reason {
	GDL_LOCK_FOR_OPEN,
	GDL_LOCK_FOR_OPEN_DONE,
	GDL_LOCK_FOR_CLOSE,
	GDL_LOCK_FOR_CLOSE_DONE,
	GDL_LOCK_FOR_RESET,
	GDL_LOCK_FOR_RESET_DONE,
};

enum gps_each_link_state_enum gps_each_link_get_state(enum gps_dl_link_id_enum link_id);
void gps_each_link_set_state(enum gps_dl_link_id_enum link_id, enum gps_each_link_state_enum state);
bool gps_each_link_change_state_from(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_state_enum from, enum gps_each_link_state_enum to);


int gps_each_link_take_big_lock(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_lock_reason reason);
int gps_each_link_give_big_lock(enum gps_dl_link_id_enum link_id);

int gps_each_link_reset(enum gps_dl_link_id_enum link_id);
void gps_dl_link_reset_ack(enum gps_dl_link_id_enum link_id);
void gps_dl_link_on_post_conn_reset(enum gps_dl_link_id_enum link_id);
bool gps_dl_link_try_to_clear_both_resetting_status(void);
int gps_dl_link_get_clock_flag(void);

enum gps_each_link_close_or_suspend_op {
	GDL_CLOSE,
	GDL_DPSTOP,
	GDL_CLKEXT,
};
int gps_each_link_enter_dsleep(enum gps_dl_link_id_enum link_id);
int gps_each_link_leave_dsleep(enum gps_dl_link_id_enum link_id);
int gps_each_link_hw_suspend(enum gps_dl_link_id_enum link_id, bool need_clk_ext);
int gps_each_link_hw_resume(enum gps_dl_link_id_enum link_id);
int gps_each_link_close(enum gps_dl_link_id_enum link_id);
int gps_each_link_check(enum gps_dl_link_id_enum link_id, int reason);

int gps_each_link_write(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len);
int gps_each_link_write_with_opt(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len, bool wait_tx_done);
int gps_each_link_read(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len);
int gps_each_link_read_with_timeout(enum gps_dl_link_id_enum link_id,
	unsigned char *buf, unsigned int len, int timeout_usec, bool *p_is_nodata);

bool gps_dl_link_start_tx_dma_if_has_data(enum gps_dl_link_id_enum link_id);

void gps_dl_link_waitable_init(struct gps_each_link_waitable *p,
	enum gps_each_link_waitable_type type);

void gps_dl_link_waitable_reset(enum gps_dl_link_id_enum link_id, enum gps_each_link_waitable_type type);

enum GDL_RET_STATUS gps_dl_link_wait_on(struct gps_each_link_waitable *p, long *p_sigval);

enum GDL_RET_STATUS gps_dl_link_try_wait_on(enum gps_dl_link_id_enum link_id,
	enum gps_each_link_waitable_type type);

void gps_dl_link_wake_up(struct gps_each_link_waitable *p);

enum gps_dl_link_event_id {
	GPS_DL_EVT_LINK_OPEN,
	GPS_DL_EVT_LINK_CLOSE,
	GPS_DL_EVT_LINK_WRITE,
	GPS_DL_EVT_LINK_READ,
	GPS_DL_EVT_LINK_DSP_ROM_READY_TIMEOUT,
	GPS_DL_EVT_LINK_DSP_FSM_TIMEOUT,
	GPS_DL_EVT_LINK_RESET_DSP,
	GPS_DL_EVT_LINK_RESET_GPS,
	GPS_DL_EVT_LINK_PRE_CONN_RESET,
	GPS_DL_EVT_LINK_POST_CONN_RESET,
	GPS_DL_EVT_LINK_PRINT_HW_STATUS,
	GPS_DL_EVT_LINK_ENTER_DPSLEEP,
	GPS_DL_EVT_LINK_LEAVE_DPSLEEP,
	GPS_DL_EVT_LINK_ENTER_DPSTOP,
	GPS_DL_EVT_LINK_LEAVE_DPSTOP,
	GPS_DL_EVT_LINK_UPDATE_SETTING,
	GPS_DL_EVT_LINK_PRINT_DATA_STATUS,
	GPS_DL_LINK_EVT_NUM,
};

void gps_dl_link_event_send(enum gps_dl_link_event_id evt,
	enum gps_dl_link_id_enum link_id);

void gps_dl_link_event_proc(enum gps_dl_link_event_id evt,
	enum gps_dl_link_id_enum link_id);

#endif /* _GPS_EACH_LINK_H */

