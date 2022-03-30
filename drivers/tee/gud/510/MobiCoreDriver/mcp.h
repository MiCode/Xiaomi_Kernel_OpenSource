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

#ifndef MC_MCP_H
#define MC_MCP_H

#include "nq.h"

struct tee_mmu;

/* Structure to hold the TA/driver information at open */
struct mcp_open_info {
	enum {
		TEE_MC_UUID,
		TEE_MC_TA,
		TEE_MC_DRIVER,
		TEE_MC_DRIVER_UUID,
	}	type;
	/* TA/driver */
	const struct mc_uuid_t	*uuid;
	uintptr_t		va;
	size_t			len;
	struct tee_mmu		*ta_mmu;
	/* TCI */
	uintptr_t		tci_va;
	size_t			tci_len;
	struct tee_mmu		*tci_mmu;
	/* Origin */
	int			user;
};

/* Structure to hold the TA/driver descriptor to pass to MCP */
struct tee_object {
	u32	length;		/* Total length */
	u8	data[];		/* Header followed by payload */
};

/* Structure to hold all mapped buffer data to pass to MCP */
struct mcp_buffer_map {
	u64		addr;		/** Page-aligned PA, or VA */
	u32		secure_va;	/** SWd virtual address */
	u32		offset;		/** Data offset inside the first page */
	u32		length;		/** Length of the data */
	u32		type;		/** Type of MMU */
	u32		flags;		/** Flags (typically read/write) */
	struct tee_mmu	*mmu;		/** MMU from which the map was made */
};

struct mcp_session {
	/* Notification queue session */
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
	/* Notification counter */
	u32			notif_count;
};

/* Init for the mcp_session structure */
void mcp_session_init(struct mcp_session *session);

/* Commands */
int mcp_get_version(struct mc_version_info *version_info);
int mcp_load_token(const struct mcp_buffer_map *buffer_map);
int mcp_open_session(struct mcp_session *session, struct mcp_open_info *info,
		     bool tci_in_use);
int mcp_close_session(struct mcp_session *session);
void mcp_cleanup_session(struct mcp_session *session);
int mcp_map(u32 session_id, struct tee_mmu *mmu, u32 *sva);
int mcp_unmap(u32 session_id, const struct mcp_buffer_map *map);
int mcp_notify(struct mcp_session *mcp_session);
int mcp_wait(struct mcp_session *session, s32 timeout);
int mcp_get_err(struct mcp_session *session, s32 *err);

/* Initialisation/cleanup */
int mcp_init(void);
void mcp_exit(void);
int mcp_start(void);
void mcp_stop(void);

#endif /* MC_MCP_H */
