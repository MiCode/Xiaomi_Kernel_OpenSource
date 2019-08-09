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

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <linux/list.h>
#include <linux/sched.h>	/* TASK_COMM_LEN */

struct tee_object;

struct tee_client {
	/* PID of task that opened the device, 0 if kernel */
	pid_t			pid;
	/* Command for task*/
	char			comm[TASK_COMM_LEN];
	/* Number of references kept to this object */
	struct kref		kref;
	/* List of contiguous buffers allocated by mcMallocWsm for the client */
	struct list_head	cbufs;
	struct mutex		cbufs_lock;	/* lock for the cbufs list */
	/* List of TA sessions opened by this client */
	struct list_head	sessions;
	struct list_head	closing_sessions;
	struct mutex		sessions_lock;	/* sessions list + closing */
	/* The list entry to attach to "ctx.clients" list */
	struct list_head	list;
};

/* Client */
struct tee_client *client_create(bool is_from_kernel);
static inline void client_get(struct tee_client *client)
{
	kref_get(&client->kref);
}

int client_put(struct tee_client *client);
bool client_has_sessions(struct tee_client *client);
void client_close(struct tee_client *client);

/* All clients */
void clients_kill_sessions(void);

/* Session */
int client_open_session(struct tee_client *client, u32 *session_id,
			const struct mc_uuid_t *uuid, uintptr_t tci,
			size_t tci_len, bool is_gp_uuid,
			struct mc_identity *identity, pid_t pid, u32 flags);
int client_open_trustlet(struct tee_client *client, u32 *session_id, u32 spid,
			 uintptr_t trustlet, size_t trustlet_len,
			 uintptr_t tci, size_t tci_len, pid_t pid, u32 flags);
int client_add_session(struct tee_client *client,
		       const struct tee_object *obj, uintptr_t tci, size_t len,
		       u32 *p_sid, bool is_gp_uuid,
		       struct mc_identity *identity, pid_t pid, u32 flags);
int client_remove_session(struct tee_client *client, u32 session_id);
int client_notify_session(struct tee_client *client, u32 session_id);
int client_waitnotif_session(struct tee_client *client, u32 session_id,
			     s32 timeout, bool silent_expiry);
int client_get_session_exitcode(struct tee_client *client, u32 session_id,
				s32 *exit_code);
int client_map_session_wsms(struct tee_client *client, u32 session_id,
			    struct mc_ioctl_buffer *bufs);
int client_unmap_session_wsms(struct tee_client *client, u32 session_id,
			      const struct mc_ioctl_buffer *bufs);

/* Contiguous buffer */
int client_cbuf_create(struct tee_client *client, u32 len, uintptr_t *addr,
		       struct vm_area_struct *vmarea);
int client_cbuf_free(struct tee_client *client, uintptr_t addr);

/* MMU */
struct cbuf;

struct tee_mmu *client_mmu_create(struct tee_client *client, pid_t pid,
				  u32 flags, uintptr_t va, u32 len,
				  struct cbuf **cbuf_p);
void client_mmu_free(struct tee_client *client, uintptr_t buf,
		     struct tee_mmu *mmu, struct cbuf *cbuf);

/* Global */
void client_init(void);

/* Debug */
int clients_debug_structs(struct kasnprintf_buf *buf);

#endif /* _CLIENT_H_ */
