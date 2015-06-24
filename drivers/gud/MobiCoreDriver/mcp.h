/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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
struct tbase_object {
	uint32_t	length;		/* Total length */
	uint32_t	header_length;	/* Length of header before payload */
	uint8_t		data[];		/* Header followed by payload */
};

/* Structure to hold all mapped buffer data to pass to MCP */
struct mcp_buffer_map {
	uint64_t	phys_addr;	/** Page-aligned physical address */
	uint64_t	secure_va;	/** Page-aligned physical address */
	uint32_t	offset;		/** Data offset inside the first page */
	uint32_t	length;		/** Length of the data */
	uint32_t	type;		/** Type of MMU */
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
	/* Notification received */
	struct completion	completion;
	/* Notification lock */
	struct mutex		exit_code_lock;
	/* Last notification */
	int32_t			exit_code;
	/* Session id */
	uint32_t		id;
	/* Session state (protected by mcp sessions_lock) */
	enum mcp_session_state {
		MCP_SESSION_RUNNING,
		MCP_SESSION_CLOSE_PREPARE,
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
int mcp_session_waitnotif(struct mcp_session *session, int32_t timeout);
int32_t mcp_session_exitcode(struct mcp_session *mcp_session);

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
bool mcp_get_idle_timeout(int32_t *timeout);
void mcp_reset_idle_timeout(void);

/* MCP commands */
int mcp_get_version(struct mc_version_info *version_info);
int mcp_load_token(uintptr_t data, const struct mcp_buffer_map *buffer_map);
int mcp_load_check(const struct tbase_object *obj,
		   const struct mcp_buffer_map *buffer_map);
int mcp_open_session(struct mcp_session *session,
		     const struct tbase_object *obj,
		     const struct mcp_buffer_map *map,
		     const struct mcp_buffer_map *tci_map);
int mcp_close_session(struct mcp_session *session);
int mcp_map(uint32_t session_id, struct mcp_buffer_map *buffer_map);
int mcp_unmap(uint32_t session_id, const struct mcp_buffer_map *buffer_map);
int mcp_multimap(uint32_t session_id, struct mcp_buffer_map *buffer_maps);
int mcp_multiunmap(uint32_t session_id,
		   const struct mcp_buffer_map *buffer_maps);
int mcp_notify(struct mcp_session *mcp_session);

/* MCP initialisation/cleanup */
int mcp_init(void);
void mcp_exit(void);
int mcp_start(void);
void mcp_stop(void);

#endif /* _MC_MCP_H_ */
