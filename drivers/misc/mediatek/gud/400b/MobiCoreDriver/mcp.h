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
#include "nq.h"

/* Structure to hold the TA/driver descriptor to pass to MCP */
struct tee_object {
	u32	length;		/* Total length */
	u32	header_length;	/* Length of header before payload */
	u8	data[];		/* Header followed by payload */
};

/* Structure to hold all mapped buffer data to pass to MCP */
struct mcp_buffer_map {
	u64	phys_addr;	/** Page-aligned physical address */
	u32	secure_va;	/** SWd virtual address */
	u32	offset;		/** Data offset inside the first page */
	u32	length;		/** Length of the data */
	u32	type;		/** Type of MMU */
	u32	flags;		/** Flags (typically read/write) */
};

struct mcp_session {
	/* Notification queue session (MUST BE FIRST) */
	struct nq_session	nq_session;
	/* Session ID */
	u32			sid;
	/* Sessions list (protected by mcp sessions_lock) */
	struct list_head	list;
	/* Notification waiter lock */
	struct mutex		notif_wait_lock;	/* Only one at a time */
	/* Notification received */
	struct completion	completion;
	/* Notification lock */
	struct mutex		exit_code_lock;
	/* Last notification */
	s32			exit_code;
	/* Session state (protected by mcp sessions_lock) */
	enum mcp_session_state {
		MCP_SESSION_RUNNING,
		MCP_SESSION_CLOSE_FAILED,
		MCP_SESSION_CLOSED,
	}			state;
};

/* Init for the mcp_session structure */
void mcp_session_init(struct mcp_session *session);
int mcp_session_waitnotif(struct mcp_session *session, s32 timeout,
			  bool silent_expiry);
s32 mcp_session_exitcode(struct mcp_session *mcp_session);

/* Commands */
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
int mcp_map(u32 session_id, struct mcp_buffer_map *map);
int mcp_unmap(u32 session_id, const struct mcp_buffer_map *map);
int mcp_notify(struct mcp_session *mcp_session);

/* Initialisation/cleanup */
int mcp_init(void);
void mcp_exit(void);
int mcp_start(void);
void mcp_stop(void);

/* Debug */
int mcp_debug_sessions(struct kasnprintf_buf *buf);
int mcp_debug_mcpcmds(struct kasnprintf_buf *buf);

#endif /* _MC_MCP_H_ */
