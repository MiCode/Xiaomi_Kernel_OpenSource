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

#ifndef _SESSION_H_
#define _SESSION_H_

#include <linux/list.h>

#include "mcp.h"
#include "iwp.h"

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
	int			in_use;
};

struct tee_session {
	/* Session descriptor */
	union {
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
	/* This TA is of Global Platform type */
	bool			is_gp;
};

struct tee_session *session_create(struct tee_client *client,
				   const struct mc_identity *identity);
static inline void session_get(struct tee_session *session)
{
	kref_get(&session->kref);
}

int session_put(struct tee_session *session);
int session_close(struct tee_session *session);

int session_mc_open_session(struct tee_session *session,
			    struct mcp_open_info *info);
int session_mc_cleanup_session(struct tee_session *session);
int session_mc_notify(struct tee_session *session);
int session_mc_wait(struct tee_session *session, s32 timeout,
		    bool silent_expiry);
int session_mc_map(struct tee_session *session, struct tee_mmu *mmu,
		   struct mc_ioctl_buffer *bufs);
int session_mc_unmap(struct tee_session *session,
		     const struct mc_ioctl_buffer *bufs);
int session_mc_get_err(struct tee_session *session, s32 *err);

int session_gp_open_session(struct tee_session *session,
			    const struct mc_uuid_t *uuid,
			    struct gp_operation *operation,
			    struct gp_return *gp_ret);
int session_gp_open_session_domu(struct tee_session *session,
				 const struct mc_uuid_t *uuid, u64 started,
				 struct interworld_session *iws,
				 struct tee_mmu **mmus,
				 struct gp_return *gp_ret);
int session_gp_invoke_command(struct tee_session *session, u32 command_id,
			      struct gp_operation *operation,
			      struct gp_return *gp_ret);
int session_gp_invoke_command_domu(struct tee_session *session,
				   u64 started, struct interworld_session *iws,
				   struct tee_mmu **mmus,
				   struct gp_return *gp_ret);
int session_gp_request_cancellation(u64 slot);

int session_debug_structs(struct kasnprintf_buf *buf,
			  struct tee_session *session, bool is_closing);

#endif /* _SESSION_H_ */
