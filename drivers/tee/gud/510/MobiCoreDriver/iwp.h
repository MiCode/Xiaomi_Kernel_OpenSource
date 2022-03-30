/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
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

#ifndef MC_IWP_H
#define MC_IWP_H

#include "mci/mcloadformat.h"		/* struct identity */

#include "nq.h"
#include "mcp.h" /* mcp_buffer_map FIXME move to nq? */

struct iwp_session {
	/* Notification queue session */
	struct nq_session	nq_session;
	/* Session ID */
	u32			sid;
	/* IWS slot */
	u64			slot;
	/* IWS other slot needed at open */
	u64			op_slot;
	/* Sessions list (protected by iwp sessions_lock) */
	struct list_head	list;
	/* Notification received */
	struct completion	completion;
	/* Interworld struct lock */
	struct mutex		iws_lock;
	/* Session state (protected by iwp sessions_lock) */
	enum iwp_session_state {
		IWP_SESSION_RUNNING,
		IWP_SESSION_CLOSE_REQUESTED,
		IWP_SESSION_CLOSED,
	}			state;
	/* GP TAs have login information */
	struct identity		client_identity;
};

struct iwp_buffer_map {
	struct mcp_buffer_map map;
	u32 sva;
};

/* Private to iwp_session structure */
void iwp_session_init(struct iwp_session *session,
		      const struct identity *identity);

/* Getters */
static inline u32 iwp_session_id(struct iwp_session *session)
{
	return session->sid;
}

static inline u64 iwp_session_slot(struct iwp_session *session)
{
	return session->slot;
}

/* Convert local errno to GP return values */
int iwp_set_ret(int ret, struct gp_return *gp_ret);

/* Commands */
int iwp_register_shared_mem(
	struct tee_mmu *mmu,
	u32 *sva,
	struct gp_return *gp_ret);
int iwp_release_shared_mem(
	struct mcp_buffer_map *map);
int iwp_open_session_prepare(
	struct iwp_session *session,
	struct gp_operation *operation,
	struct mc_ioctl_buffer *bufs,
	struct gp_shared_memory **parents,
	struct gp_return *gp_ret);
void iwp_open_session_abort(
	struct iwp_session *iwp_session);
int iwp_open_session(
	struct iwp_session *iwp_session,
	const struct mc_uuid_t *uuid,
	struct tee_mmu *ta_mmu,
	struct gp_operation *operation,
	const struct iwp_buffer_map *maps,
	struct interworld_session *iws,
	struct tee_mmu **mmus,
	struct gp_return *gp_ret,
	const char vm_id[16]);
int iwp_close_session(
	struct iwp_session *iwp_session);
int iwp_invoke_command_prepare(
	struct iwp_session *iwp_session,
	u32 command_id,
	struct gp_operation *operation,
	struct mc_ioctl_buffer *bufs,
	struct gp_shared_memory **parents,
	struct gp_return *gp_ret);
void iwp_invoke_command_abort(
	struct iwp_session *iwp_session);
int iwp_invoke_command(
	struct iwp_session *iwp_session,
	struct gp_operation *operation,
	const struct iwp_buffer_map *maps,
	struct interworld_session *iws,
	struct tee_mmu **mmus,
	struct gp_return *gp_ret);
int iwp_request_cancellation(
	u64 slot);

/* Initialisation/cleanup */
int iwp_init(void);
void iwp_exit(void);
int iwp_start(void);
void iwp_stop(void);

#endif /* MC_IWP_H */
