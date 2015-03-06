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

#include "admin.h"	/* struct tbase_object */
#include "mmu.h"	/* struct tbase_mmu */

struct mcp_context;

/* Helpers */
int mcp_set_sleep_mode_rq(uint16_t rq);
uint16_t mcp_get_sleep_mode_req(void);
/*
 * Get the requested SWd sleep timeout value (ms)
 * - if the timeout is -1, wait indefinitely
 * - if the timeout is 0, re-schedule immediately (timeouts in µs in the SWd)
 * - otherwise sleep for the required time
 * returns true if sleep is required, false otherwise
 */
bool mcp_get_idle_timeout(int32_t *timeout);
void mcp_reset_idle_timeout(void);
#ifdef MC_PM_RUNTIME
int mcp_suspend_prepare(void);
#endif
int mcp_switch_enter(void);
void mcp_wake_up(uint32_t nf_payload);

/* MCP commands */
int mcp_get_version(struct mc_version_info *version_info);
int mcp_load_token(uintptr_t data, size_t len, const struct tbase_mmu *mmu);
int mcp_load_check(const struct tbase_object *obj,
		   const struct tbase_mmu *mmu);
int mcp_open_session(const struct tbase_object *obj,
		     const struct tbase_mmu *mmu,
		     bool is_gp_uuid,
		     const void *buf, size_t buf_len,
		     const struct tbase_mmu *buf_mmu,
		     uint32_t *session_id);
int mcp_close_session(uint32_t session_id);
int mcp_map(uint32_t session_id, const void *buf, size_t buf_len,
	    const struct tbase_mmu *mmu, uint32_t *secure_va);
int mcp_unmap(uint32_t session_id, uint32_t secure_va, size_t buf_len,
	      const struct tbase_mmu *mmu);

/* MCP initialisation/cleanup */
int mcp_init(void *mcp_buffer_base);
void mcp_cleanup(void);

#endif /* _MC_MCP_H_ */
