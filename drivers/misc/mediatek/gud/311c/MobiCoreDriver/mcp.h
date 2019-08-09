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

#ifndef _MC_MCP_H_
#define _MC_MCP_H_

#include "mci/mcloadformat.h"		/* struct identity */

/* Structure to hold the TA/driver descriptor to pass to MCP */
struct tee_object {
	u32	length;		/* Total length */
	u32	header_length;	/* Length of header before payload */
	u8	data[];		/* Header followed by payload */
};

/* Structure to hold all mapped buffer data to pass to MCP */
struct mcp_buffer_map {
	u64	phys_addr;	/** Page-aligned physical address */
	u64	secure_va;	/** SWd virtual address */
	u32	offset;		/** Data offset inside the first page */
	u32	length;		/** Length of the data */
	u32	type;		/** Type of MMU */
};

struct mcp_session {
	/* Work descriptor to handle delayed closing, set by upper layer */
	struct work_struct	close_work;
	/* Sessions list (protected by mcp sessions_lock) */
	struct list_head	list;
	/* Notifications list (protected by mcp notifications_mutex) */
	struct list_head	notifications_list;
	/* Notification waiter lock */
	struct mutex		notif_wait_lock;	/* Only one at a time */
	/* Notification debug (protected by mcp notifications_mutex) */
	enum mcp_notification_state {
		MCP_NOTIF_IDLE,		/* Nothing happened yet */
		MCP_NOTIF_QUEUED,	/* Notification in overflow queue */
		MCP_NOTIF_SENT,		/* Notification in send queue */
		MCP_NOTIF_RECEIVED,	/* Notification received */
		MCP_NOTIF_CONSUMED,	/* Notification reported to CA */
		MCP_NOTIF_DEAD,		/* Error reported to CA */
	}			notif_state;
	/* Time at notification state change */
	u64			notif_cpu_clk;
	/* Notification received */
	struct completion	completion;
	/* Notification lock */
	struct mutex		exit_code_lock;
	/* Last notification */
	s32			exit_code;
	/* Session id */
	u32		id;
	/* Session state (protected by mcp sessions_lock) */
	enum mcp_session_state {
		MCP_SESSION_RUNNING,
		MCP_SESSION_CLOSE_FAILED,
		MCP_SESSION_CLOSE_REQUESTED,
		MCP_SESSION_CLOSE_NOTIFIED,
		MCP_SESSION_CLOSING_GP,
		MCP_SESSION_CLOSED,
	}			state;
	/* This TA is of Global Platform type, set by upper layer */
	bool			is_gp;
	/* GP TAs have login information */
	struct identity		identity;
};

/* Init for the mcp_session structure */
void mcp_session_init(struct mcp_session *session, bool is_gp,
		      const struct identity *identity);
int mcp_session_waitnotif(struct mcp_session *session, s32 timeout,
			  bool silent_expiry);
s32 mcp_session_exitcode(struct mcp_session *mcp_session);

/* SWd suspend/resume */
int mcp_suspend(void);
int mcp_resume(void);
bool mcp_suspended(void);

/* Callback to scheduler registration */
enum mcp_scheduler_commands {
	MCP_YIELD,
	MCP_NSIQ,
};

void mcp_register_scheduler(int (*scheduler_cb)(enum mcp_scheduler_commands));
bool mcp_notifications_flush(void);
void mcp_register_crashhandler(void (*crashhandler_cb)(void));

/*
 * Get the requested SWd sleep timeout value (ms)
 * - if the timeout is -1, wait indefinitely
 * - if the timeout is 0, re-schedule immediately (timeouts in µs in the SWd)
 * - otherwise sleep for the required time
 * returns true if sleep is required, false otherwise
 */
bool mcp_get_idle_timeout(s32 *timeout);
void mcp_reset_idle_timeout(void);
void mcp_update_time(void);

/* MCP commands */
int mcp_get_version(struct mc_version_info *version_info);
int mcp_load_token(uintptr_t data, const struct mcp_buffer_map *buffer_map);
int mcp_load_check(const struct tee_object *obj,
		   const struct mcp_buffer_map *buffer_map);
int mcp_open_session(struct mcp_session *session,
		     const struct tee_object *obj,
		     const struct mcp_buffer_map *map,
		     const struct mcp_buffer_map *tci_map);
int mcp_close_session(struct mcp_session *session);
void mcp_kill_session(struct mcp_session *session);
int mcp_multimap(u32 session_id, struct mcp_buffer_map *maps,
		 bool use_multimap);
int mcp_multiunmap(u32 session_id, struct mcp_buffer_map *maps,
		   bool use_multimap);
int mcp_notify(struct mcp_session *mcp_session);

/* MCP initialisation/cleanup */
int mcp_init(void);
void mcp_exit(void);
int mcp_start(void);
void mcp_stop(void);

/* MCP debug */
int mcp_debug_sessions(struct kasnprintf_buf *buf);
int mcp_debug_mcpcmds(struct kasnprintf_buf *buf);

#endif /* _MC_MCP_H_ */
