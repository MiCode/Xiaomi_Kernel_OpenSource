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

#ifndef _SESSION_H_
#define _SESSION_H_

#include <linux/list.h>

#include "mcp.h"
#include "iwp.h"
#include "nq.h"

struct tee_object;
struct tee_mmu;
struct mc_ioctl_buffer;

struct tee_wsm {
	/* Buffer NWd address (uva or kva, used only for lookup) */
	uintptr_t		va;
	/* Buffer length */
	u32			len;
	/* Buffer flags */
	u32			flags;
	/* Buffer SWd address */
	u32			sva;
	union {
		/* MMU table */
		struct tee_mmu		*mmu;
		/* Index of re-used buffer (temporary) */
		int			index;
	};
	/* Pointer to associated cbuf, if relevant */
	struct cbuf		*cbuf;
	/* State of this WSM */
	bool			in_use;
};

struct tee_session {
	/* Session descriptor */
	union {
		struct nq_session	nq_session;
		struct mcp_session	mcp_session;
		struct iwp_session	iwp_session;
	};
	/* Owner */
	struct tee_client	*client;
	/* Number of references kept to this object */
	struct kref		kref;
	/* WSM for the TCI */
	struct tee_wsm		tci;
	/* The list entry to attach to session list of owner */
	struct list_head	list;
	/* Session WSMs lock */
	struct mutex		wsms_lock;
	/* WSMs for a session */
	struct tee_wsm		wsms[MC_MAP_MAX];
	/* Pointers to WSMs in LRU order (0 is oldest) */
	struct tee_wsm		*wsms_lru[MC_MAP_MAX];
};

struct tee_session *session_create(struct tee_client *client, bool is_gp,
				   struct mc_identity *identity, int client_fd);
int session_open(struct tee_session *session, const struct tee_object *obj,
		 const struct tee_mmu *obj_mmu, uintptr_t tci, size_t len,
		 int client_fd);
int session_close(struct tee_session *session);
static inline void session_get(struct tee_session *session)
{
	kref_get(&session->kref);
}

int session_put(struct tee_session *session);
int session_kill(struct tee_session *session);
int session_map(struct tee_session *session, struct mc_ioctl_buffer *bufs,
		int client_fd);
int session_unmap(struct tee_session *session,
		  const struct mc_ioctl_buffer *bufs);
s32 session_exitcode(struct tee_session *session);
int session_notify_swd(struct tee_session *session);
int session_waitnotif(struct tee_session *session, s32 timeout,
		      bool silent_expiry);

int session_gp_open_session(struct tee_session *session,
			    const struct tee_object *obj,
			    const struct tee_mmu *obj_mmu,
			    struct gp_operation *operation,
			    struct gp_return *gp_ret,
			    int client_fd);
int session_gp_invoke_command(struct tee_session *session, u32 command_id,
			      struct gp_operation *operation,
			      struct gp_return *gp_ret, int client_fd);
int session_gp_request_cancellation(u32 slot);

int session_debug_structs(struct kasnprintf_buf *buf,
			  struct tee_session *session, bool is_closing);

#endif /* _SESSION_H_ */
