/*
 * Copyright (c) 2017 TRUSTONIC LIMITED
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

#ifndef _MC_XEN_FE_H_
#define _MC_XEN_FE_H_

#include <linux/version.h>

#include "main.h"
#include "client.h"
#include "iwp.h"
#include "mcp.h"

#ifdef TRUSTONIC_XEN_DOMU
/* MC protocol interface */
int xen_mc_get_version(struct mc_version_info *version_info);
int xen_mc_open_session(struct mcp_session *session,
			struct mcp_open_info *info);
int xen_mc_close_session(struct mcp_session *session);
int xen_mc_map(u32 session_id, struct tee_mmu *mmu, u32 *sva);
int xen_mc_unmap(u32 session_id, const struct mcp_buffer_map *map);
int xen_mc_notify(struct mcp_session *session);
int xen_mc_wait(struct mcp_session *session, s32 timeout, bool silent_expiry);
int xen_mc_get_err(struct mcp_session *session, s32 *err);
/* GP protocol interface */
int xen_gp_register_shared_mem(struct tee_mmu *mmu, u32 *sva,
			       struct gp_return *gp_ret);
int xen_gp_release_shared_mem(struct mcp_buffer_map *map);
int xen_gp_open_session(struct iwp_session *session,
			const struct mc_uuid_t *uuid,
			const struct iwp_buffer_map *maps,
			struct interworld_session *iws,
			struct interworld_session *op_iws,
			struct gp_return *gp_ret);
int xen_gp_close_session(struct iwp_session *session);
int xen_gp_invoke_command(struct iwp_session *session,
			  const struct iwp_buffer_map *maps,
			  struct interworld_session *iws,
			  struct gp_return *gp_ret);
int xen_gp_request_cancellation(u64 slot);

int xen_fe_init(int (*probe)(void), int (*start)(void));
void xen_fe_exit(void);
#else
static inline int xen_fe_init(int (*probe)(void), int (*start)(void))
{
	return 0;
}

static inline void xen_fe_exit(void)
{
}
#endif

#endif /* _MC_XEN_FE_H_ */
