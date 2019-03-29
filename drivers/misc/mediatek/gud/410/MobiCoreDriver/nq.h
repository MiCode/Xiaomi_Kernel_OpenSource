/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
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
#include <linux/notifier.h>

/** Max number of interworld session allocated in MCI buffer */
#define MAX_IW_SESSION 256

enum nq_notif_state {
	NQ_NOTIF_IDLE,		/* Nothing happened yet */
	NQ_NOTIF_QUEUED,	/* Notification in overflow queue */
	NQ_NOTIF_SENT,		/* Notification in send queue */
	NQ_NOTIF_RECEIVED,	/* Notification received */
	NQ_NOTIF_CONSUMED,	/* Notification reported to CA */
	NQ_NOTIF_DEAD,		/* Error reported to CA */
};

/* FIXME to be renamed */
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
	enum nq_notif_state	state;
	/* Time at notification state change */
	u64			cpu_clk;
	/* This TA is of Global Platform type, set by upper layer */
	bool			is_gp;
};

/* Notification queue channel */
void nq_session_init(struct nq_session *session, bool is_gp);
void nq_session_exit(struct nq_session *session);
void nq_session_state_update(struct nq_session *session,
			     enum nq_notif_state state);
int nq_session_notify(struct nq_session *session, u32 id, u32 payload);
const char *nq_session_state(const struct nq_session *session, u64 *cpu_clk);

/* Services */
union mcp_message *nq_get_mcp_buffer(void);
struct interworld_session *nq_get_iwp_buffer(void);
void nq_set_version_ptr(char *version);
void nq_register_notif_handler(void (*handler)(u32 id, u32 payload), bool iwp);
int nq_register_tee_stop_notifier(struct notifier_block *nb);
int nq_unregister_tee_stop_notifier(struct notifier_block *nb);
ssize_t nq_get_stop_message(char __user *buffer, size_t size);
void nq_signal_tee_hung(void);

/* SWd suspend/resume */
int nq_suspend(void);
int nq_resume(void);

/* Start/stop TEE */
int nq_start(void);
void nq_stop(void);

/* Initialisation/cleanup */
int nq_init(void);
void nq_exit(void);

#endif /* _MC_NQ_H_ */
