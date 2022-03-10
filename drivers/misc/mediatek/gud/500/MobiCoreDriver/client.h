/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
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

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <linux/list.h>
#include <linux/sched.h>	/* TASK_COMM_LEN */

#include "public/mc_user.h"	/* many types */

struct tee_client;
struct mcp_open_info;
struct tee_mmu;
struct interworld_session;

/* Client */
struct tee_client *client_create(bool is_from_kernel, const char *vm_id);
void client_get(struct tee_client *client);
int client_put(struct tee_client *client);
bool client_has_sessions(struct tee_client *client);
void client_close(struct tee_client *client);
void client_cleanup(void);
const char *client_vm_id(struct tee_client *client);

/* MC */
int client_mc_open_session(struct tee_client *client,
			   const struct mc_uuid_t *uuid,
			   uintptr_t tci_va, size_t tci_len, u32 *session_id);
int client_mc_open_trustlet(struct tee_client *client,
			    uintptr_t ta_va, size_t ta_len,
			    uintptr_t tci_va, size_t tci_len, u32 *session_id);
int client_mc_open_common(struct tee_client *client, struct mcp_open_info *info,
			  u32 *session_id);
int client_remove_session(struct tee_client *client, u32 session_id);
int client_notify_session(struct tee_client *client, u32 session_id);
int client_waitnotif_session(struct tee_client *client, u32 session_id,
			     s32 timeout);
int client_get_session_exitcode(struct tee_client *client, u32 session_id,
				s32 *exit_code);
int client_mc_map(struct tee_client *client, u32 session_id,
		  struct tee_mmu *mmu, struct mc_ioctl_buffer *buf);
int client_mc_unmap(struct tee_client *client, u32 session_id,
		    const struct mc_ioctl_buffer *buf);

/* GP */
int client_gp_initialize_context(struct tee_client *client,
				 struct gp_return *gp_ret);
int client_gp_register_shared_mem(struct tee_client *client,
				  struct tee_mmu *mmu, u32 *sva,
				  const struct gp_shared_memory *memref,
				  struct gp_return *gp_ret);
int client_gp_release_shared_mem(struct tee_client *client,
				 const struct gp_shared_memory *memref);
int client_gp_open_session(struct tee_client *client,
			   const struct mc_uuid_t *uuid,
			   struct gp_operation *operation,
			   const struct mc_identity *identity,
			   struct gp_return *gp_ret,
			   u32 *session_id);
int client_gp_open_session_domu(struct tee_client *client,
				const struct mc_uuid_t *uuid, u64 started,
				struct interworld_session *iws,
				struct tee_mmu **mmus,
				struct gp_return *gp_ret);
int client_gp_close_session(struct tee_client *client, u32 session_id);
int client_gp_invoke_command(struct tee_client *client, u32 session_id,
			     u32 command_id,
			     struct gp_operation *operation,
			     struct gp_return *gp_ret);
int client_gp_invoke_command_domu(struct tee_client *client, u32 session_id,
				  u64 started, struct interworld_session *iws,
				  struct tee_mmu **mmus,
				  struct gp_return *gp_ret);
void client_gp_request_cancellation(struct tee_client *client, u64 started);

/* Contiguous buffer */
int client_cbuf_create(struct tee_client *client, u32 len, uintptr_t *addr,
		       struct vm_area_struct *vmarea);
int client_cbuf_free(struct tee_client *client, uintptr_t addr);

/* GP internal */
struct client_gp_operation {
	struct list_head	list;
	u64			started;
	u64			slot;
	int			cancelled;
};

/* Called from session when a new operation starts/ends */
bool client_gp_operation_add(struct tee_client *client,
			     struct client_gp_operation *operation);
void client_gp_operation_remove(struct tee_client *client,
				struct client_gp_operation *operation);

/* MMU */
struct cbuf;

struct tee_mmu *client_mmu_create(struct tee_client *client,
				  const struct mc_ioctl_buffer *buf_in,
				  struct cbuf **cbuf_p);
void tee_cbuf_put(struct cbuf *cbuf);

/* Buffer shared with SWd at client level */
u32 client_get_cwsm_sva(struct tee_client *client,
			const struct gp_shared_memory *memref);
void client_put_cwsm_sva(struct tee_client *client, u32 sva);

/* Global */
void client_init(void);

/* Debug */
int clients_debug_structs(struct kasnprintf_buf *buf);

#endif /* _CLIENT_H_ */
