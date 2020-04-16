/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_NQ_H_
#define _MC_NQ_H_

#include <linux/list.h>
#include <linux/mutex.h>

/* FIXME to be in .c file, renamed */
struct nq_session {
	/* Notification id */
	u32			id;
	/* Notification payload */
	u32			payload;
	/* Notifications list */
	struct list_head	list;
	/* Notification debug mutex */
	struct mutex		mutex;
	/* Current notification/session state */
	enum nq_notif_state {
		NQ_NOTIF_IDLE,		/* Nothing happened yet */
		NQ_NOTIF_QUEUED,	/* Notification in overflow queue */
		NQ_NOTIF_SENT,		/* Notification in send queue */
		NQ_NOTIF_RECEIVED,	/* Notification received */
		NQ_NOTIF_CONSUMED,	/* Notification reported to CA */
		NQ_NOTIF_DEAD,		/* Error reported to CA */
	}			state;
	/* Time at notification state change */
	u64			cpu_clk;
	/* This TA is of Global Platform type, set by upper layer */
	bool			is_gp;
};

/* Initialisation/cleanup */
int nq_init(void);
void nq_exit(void);

/* Start/stop TEE */
int nq_start(void);
void nq_stop(void);

/* SWd suspend/resume */
int nq_suspend(void);
int nq_resume(void);
bool nq_suspended(void);

/*
 * Get the requested SWd sleep timeout value (ms)
 * - if the timeout is -1, wait indefinitely
 * - if the timeout is 0, re-schedule immediately (timeouts in µs in the SWd)
 * - otherwise sleep for the required time
 * returns true if sleep is required, false otherwise
 */
bool nq_get_idle_timeout(s32 *timeout);
void nq_reset_idle_timeout(void);

/* Services */
/* Callback to scheduler registration */
enum nq_scheduler_commands {
	MC_NQ_YIELD,
	MC_NQ_NSIQ,
};

void nq_register_scheduler(int (*scheduler_cb)(enum nq_scheduler_commands));
void nq_register_crash_handler(void (*crashhandler_cb)(void));
void nq_dump_status(void);
void nq_update_time(void);
/* Accessors for MCP/IWP contexts */
union mcp_message *nq_get_mcp_message(void);
struct interworld_session *nq_get_iwp_buffer(void);
void nq_register_notif_handler(void (*handler)(u32 id, u32 payload), bool iwp);

/* Notifications */
void nq_session_init(struct nq_session *session, bool is_gp);
bool nq_session_is_gp(const struct nq_session *session);
u64 nq_session_notif_cpu_clk(const struct nq_session *session);
void nq_session_exit(struct nq_session *session);
void nq_session_state_update(struct nq_session *session,
			     enum nq_notif_state state);
const char *nq_session_state_string(struct nq_session *session);
int nq_session_notify(struct nq_session *session, u32 id, u32 payload);
bool nq_notifications_flush(void);

#endif /* _MC_NQ_H_ */
