/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2020 TRUSTONIC LIMITED
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

#ifndef MC_PROTOCOL_INTERNAL_H
#define MC_PROTOCOL_INTERNAL_H

#include "linux/atomic.h"

#include "iwp.h"
#include "mcp.h"
#include "mci/mciiwp.h"		/* struct interworld_session */
#include "mmu_internal.h"
#include "protocol.h"

#define TEE_BUFFERS	4

enum tee_protocol_fe_cmd {
	/* Communication */
	TEE_FE_NONE,
	TEE_CONNECT,
	TEE_GET_VERSION,
	/* TEE_MC_OPEN_DEVICE = 11,		SWd does not support this */
	/* TEE_MC_CLOSE_DEVICE,			SWd does not support this */
	TEE_MC_OPEN_SESSION = 13,
	TEE_MC_OPEN_TRUSTLET,
	TEE_MC_CLOSE_SESSION,
	TEE_MC_NOTIFY,
	TEE_MC_WAIT,
	TEE_MC_MAP,
	TEE_MC_UNMAP,
	TEE_MC_GET_ERR,
	/* TEE_GP_INITIALIZE_CONTEXT = 21,	SWd does not support this */
	/* TEE_GP_FINALIZE_CONTEXT,		SWd does not support this */
	TEE_GP_REGISTER_SHARED_MEM = 23,
	TEE_GP_RELEASE_SHARED_MEM,
	TEE_GP_OPEN_SESSION,
	TEE_GP_CLOSE_SESSION,
	TEE_GP_INVOKE_COMMAND,
	TEE_GP_REQUEST_CANCELLATION,
};

enum tee_protocol_be_cmd {
	/* Communication */
	TEE_BE_NONE,
	TEE_MC_WAIT_DONE = TEE_MC_WAIT,
	TEE_GP_OPEN_SESSION_DONE = TEE_GP_OPEN_SESSION,
	TEE_GP_CLOSE_SESSION_DONE = TEE_GP_CLOSE_SESSION,
	TEE_GP_INVOKE_COMMAND_DONE = TEE_GP_INVOKE_COMMAND,
};

union tee_protocol_mmu_table {
	/* Array of references to pages (PTE_ENTRIES_MAX or PMD_ENTRIES_MAX) */
	u64			*refs;
	/* Address of table */
	void			*addr;
	/* Page for table */
	unsigned long		page;
};

struct tee_protocol_buffer {
	/* Page Middle Directory, refs to tee_xen_pte_table's (full pages) */
	u64			pmd_ref;
	u64			addr;		/* Unique VM address */
	u32			offset;
	u32			length;
	u32			flags;
	u32			sva;
};

/* FE side, synchronous and asynchronous commands */
struct fe2be_data {
	char				server_name[32];
	/* Command for BE */
	enum tee_protocol_fe_cmd	cmd;		/* in */
	u32				id;		/* in (debug) */
	/* Return code of this command from BE */
	int				otherend_ret;	/* out */
	struct mc_uuid_t		uuid;		/* in */
	u32				session_id;	/* in/out */
	/* Trusted Application binary */
	struct tee_protocol_buffer	ta_bin;
	/* Buffers to share (4 for GP, 2 for mcOpenTrustlet) */
	struct tee_protocol_buffer	buffers[TEE_BUFFERS]; /* in */
	/* MC */
	struct mc_version_info		version_info;	/* out */
	s32				timeout;	/* in */
	s32				err;		/* out */
	/* GP */
	u64				operation_id;	/* in */
	struct gp_return		gp_ret;		/* out */
	struct interworld_session	iws;		/* in */
};

/* BE side, response to asynchronous command, never read by BE */
struct be2fe_data {
	char				server_name[32];
	/* Command for FE */
	enum tee_protocol_be_cmd	cmd;		/* in */
	u32				id;		/* in (debug) */
	/* Return code from command */
	int				cmd_ret;	/* in */
	/* The operation id is used to match GP request and response */
	u64				operation_id;	/* in */
	struct gp_return		gp_ret;		/* in */
	struct interworld_session	iws;		/* in */
	/* The session id is used to match MC request and response */
	u32				session_id;	/* in */
};

struct protocol_fe {
	struct tee_client		*client;
	/* Mutex */
	struct mutex			protocol_mutex;
	int				protocol_busy;
	/* FE side, synchronous and asynchronous commands */
	struct fe2be_data		*fe2be_data;
	/* BE side, response to asynchronous command, never read by BE */
	struct be2fe_data		*be2fe_data;
	/* Unique ID for commands */
	u32				fe_cmd_id;
	/* Unique ID for VM instance, so we detect a VM restart */
	atomic_t			vm_instance_no;
};

static inline void protocol_get(struct protocol_fe *pfe)
{
	mutex_lock(&pfe->protocol_mutex);
	pfe->protocol_busy = true;
}

static inline void protocol_put(struct protocol_fe *pfe)
{
	pfe->protocol_busy = false;
	mutex_unlock(&pfe->protocol_mutex);
}

int protocol_fe_init(struct protocol_fe *pfe);

struct tee_protocol_fe_call_ops;
struct tee_protocol_be_call_ops;

/*
 * This is how the ops work:
 * as the driver has three phases: init, probe and start, so do we for
 * protocols.
 * * init: called at driver probe
 * * start: when the TEE is connected (delayed for some platforms)
 *
 * This is not supported, but we also have:
 * * stop: undo the start (the TEE can only be started once)
 * * exit: undo the init
 *
 * All VMs must provide a VM ID as a string.
 *
 * Front-ends provide their own call operations which use their specific
 * protocol.
 *
 * Some front-ends do the probe/start themselves, so they can set a flag to stop
 * the normal init short.
 */
struct tee_protocol_ops {
	const char *name;
	/* Return 1 to stop init and use own probe/start mechanism */
	int (*early_init)(int (*probe)(void), int (*start)(void));
	int (*init)(void);
	void (*exit)(void);
	int (*start)(void);
	void (*stop)(void);
	const char *vm_id;
	struct tee_protocol_fe_call_ops *fe_call_ops;
	struct tee_protocol_be_call_ops *be_call_ops;
	int fe_uses_pages_and_vas;
};

/***********************************************************************
 *                     Front-ends call operations
 **********************************************************************/

/* Front-end operations if any */
extern struct tee_protocol_fe_call_ops *fe_ops;

struct protocol_fe_map {
	/* MMU that contains physical addresses of PMD, PTE and pages */
	struct tee_mmu			*mmu;
	/* To auto-delete */
	struct tee_deleter		deleter;

	/* Array of PTE tables, so we can release the associated buffer refs */
	int				nr_pte_tables;
	int				nr_pages;
	int				readonly;
	int				refs_shared;	/* Leak check */
};

struct tee_protocol_fe_call_ops {
	int (*call_be)(
		struct protocol_fe *pfe);
	struct protocol_fe_map *(*fe_map_create)(
		struct tee_protocol_buffer *buffer,
		const struct mcp_buffer_map *b_map,
		struct protocol_fe *pfe);
	void (*fe_map_release_pmd_ptes)(
		struct protocol_fe_map *map,
		const struct tee_protocol_buffer *buffer);
};

int protocol_fe_dispatch(struct protocol_fe *pfe);

/* MC protocol interface */
int protocol_mc_get_version(struct mc_version_info *version_info);
int protocol_mc_open_session(struct mcp_session *session,
			     struct mcp_open_info *info);
int protocol_mc_close_session(struct mcp_session *session);
int protocol_mc_notify(struct mcp_session *session);
int protocol_mc_wait(struct mcp_session *session,
		     s32 timeout);
int protocol_mc_map(u32 session_id,
		    struct tee_mmu *mmu,
		    u32 *sva);
int protocol_mc_unmap(u32 session_id,
		      const struct mcp_buffer_map *map);
int protocol_mc_get_err(struct mcp_session *session,
			s32 *err);

/* GP protocol interface */
int protocol_gp_register_shared_mem(struct tee_mmu *mmu,
				    u32 *sva,
				    struct gp_return *gp_ret);
int protocol_gp_release_shared_mem(struct mcp_buffer_map *map);
int protocol_gp_open_session(struct iwp_session *session,
			     const struct mc_uuid_t *uuid,
			     struct tee_mmu *ta_mmu,
			     const struct iwp_buffer_map *b_maps,
			     struct interworld_session *iws,
			     struct interworld_session *op_iws,
			     struct gp_return *gp_ret);
int protocol_gp_close_session(struct iwp_session *session);
int protocol_gp_invoke_command(struct iwp_session *session,
			       const struct iwp_buffer_map *b_maps,
			       struct interworld_session *iws,
			       struct gp_return *gp_ret);
int protocol_gp_request_cancellation(u64 slot);

/***********************************************************************
 *                     Back-ends call operations
 **********************************************************************/

/* Back-end operations if any */
extern struct tee_protocol_be_call_ops *be_ops;

/* Buffer management */
struct protocol_be_map {
	/* MMU that contains physical addresses of PMD, PTE and pages */
	struct tee_mmu		*mmu;
	/* To auto-delete */
	struct tee_deleter	deleter;
};

struct tee_protocol_be_call_ops {
	void (*call_fe)(
		struct protocol_fe *pfe,
		atomic_t call_vm_instance_no);
	struct protocol_be_map *(*be_map_create)(
		struct tee_protocol_buffer *buffer,
		struct protocol_fe *pfe);
	void (*be_map_delete)(
		struct protocol_be_map *map);
	struct tee_mmu *(*be_set_mmu)(
		struct protocol_be_map *map,
		struct mcp_buffer_map b_map);
	int (*be_create_client)(
		struct protocol_fe *pfe);
};

int protocol_be_dispatch(struct protocol_fe *pfe);

#endif /* MC_PROTOCOL_INTERNAL_H */
