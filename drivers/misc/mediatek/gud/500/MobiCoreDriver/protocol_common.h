/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
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

#ifndef _MC_PROTOCOL_INTERNAL_H_
#define _MC_PROTOCOL_INTERNAL_H_

#include "iwp.h"
#include "mcp.h"

struct tee_protocol_fe_call_ops;

/* Front-end operations if any */
extern struct tee_protocol_fe_call_ops *fe_ops;

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
	int fe_uses_pages_and_vas;
};

struct tee_protocol_fe_call_ops {
	/* MC protocol interface */
	int (*mc_get_version)(
		struct mc_version_info *version_info);
	int (*mc_open_session)(
		struct mcp_session *session,
		struct mcp_open_info *info);
	int (*mc_close_session)(
		struct mcp_session *session);
	int (*mc_map)(
		u32 session_id,
	       struct tee_mmu *mmu,
	       u32 *sva);
	int (*mc_unmap)(
		u32 session_id,
		 const struct mcp_buffer_map *map);
	int (*mc_notify)(
		struct mcp_session *session);
	int (*mc_wait)(
		struct mcp_session *session,
		s32 timeout);
	int (*mc_get_err)(
		struct mcp_session *session,
		s32 *err);
	/* GP protocol interface */
	int (*gp_register_shared_mem)(
		struct tee_mmu *mmu,
		u32 *sva,
		struct gp_return *gp_ret);
	int (*gp_release_shared_mem)(
		struct mcp_buffer_map *map);
	int (*gp_open_session)(
		struct iwp_session *session,
		const struct mc_uuid_t *uuid,
		const struct iwp_buffer_map *maps,
		struct interworld_session *iws,
		struct interworld_session *op_iws,
		struct gp_return *gp_ret);
	int (*gp_close_session)(
		struct iwp_session *session);
	int (*gp_invoke_command)(
		struct iwp_session *session,
		const struct iwp_buffer_map *maps,
		struct interworld_session *iws,
		struct gp_return *gp_ret);
	int (*gp_request_cancellation)(
		u64 slot);
};

#endif /* _MC_PROTOCOL_INTERNAL_H_ */
