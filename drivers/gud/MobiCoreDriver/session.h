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

#ifndef _SESSION_H_
#define _SESSION_H_

#include <linux/list.h>

#include "mcp.h"

struct tbase_object;
struct tbase_mmu;
struct mc_ioctl_buffer;

struct tbase_session {
	/* Session list lock */
	struct mutex		close_lock;
	/* MCP session descriptor (MUST BE FIRST) */
	struct mcp_session	mcp_session;
	/* Owner */
	struct tbase_client	*client;
	/* Number of references kept to this object */
	struct kref		kref;
	/* The list entry to attach to session list of owner */
	struct list_head	list;
	/* Session WSMs lock */
	struct mutex		wsms_lock;
	/* List of WSMs for a session */
	struct list_head	wsms;
};

struct tbase_session *session_create(struct tbase_client *client, bool is_gp,
				     struct mc_identity *identity);
int session_open(struct tbase_session *session, const struct tbase_object *obj,
		 const struct tbase_mmu *obj_mmu, uintptr_t tci, size_t len);
int session_close(struct tbase_session *session);
static inline void session_get(struct tbase_session *session)
{
	kref_get(&session->kref);
}

int session_put(struct tbase_session *session);
int session_wsms_add(struct tbase_session *session,
		     struct mc_ioctl_buffer *bufs);
int session_wsms_remove(struct tbase_session *session,
			const struct mc_ioctl_buffer *bufs);
int32_t session_exitcode(struct tbase_session *session);
int session_notify_swd(struct tbase_session *session);
int session_waitnotif(struct tbase_session *session, int32_t timeout);
int session_info(struct tbase_session *session, struct kasnprintf_buf *buf);

#endif /* _SESSION_H_ */
