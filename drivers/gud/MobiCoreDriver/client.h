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

struct task_struct;
struct tbase_object;
struct tbase_session;

struct tbase_client {
	/* PID of task that opened the device, 0 if kernel */
	pid_t			pid;
	/* Command for task*/
	char			comm[TASK_COMM_LEN];
	/* Number of references kept to this object */
	struct kref		kref;
	/* List of contiguous buffers allocated by mcMallocWsm for the client */
	struct list_head	cbufs;
	struct mutex		cbufs_lock;	/* lock for the cbufs list */
	/* List of tbase TA sessions opened by this client */
	struct list_head	sessions;
	struct mutex		sessions_lock;	/* sessions list + closing */
	/* Client state */
	bool			closing;
	/* The list entry to attach to "ctx.clients" list */
	struct list_head	list;
};

struct tbase_client *client_create(bool is_from_kernel);

void client_close_sessions(struct tbase_client *client);

static inline void client_get(struct tbase_client *client)
{
	kref_get(&client->kref);
}

void client_put(struct tbase_client *client);

bool client_is_kernel(struct tbase_client *client);

bool client_set_closing(struct tbase_client *client);

int client_add_session(struct tbase_client *client,
		       const struct tbase_object *obj, uintptr_t tci,
		       size_t len, uint32_t *p_sid, bool is_gp_uuid,
		       struct mc_identity *identity);

int client_remove_session(struct tbase_client *client, uint32_t session_id);

struct tbase_session *client_ref_session(struct tbase_client *client,
					 uint32_t session_id);

void client_unref_session(struct tbase_session *session);

int client_info(struct tbase_client *client, struct kasnprintf_buf *buf);

/*
 * Contiguous buffer allocated to TLCs.
 * These buffers are uses as world shared memory (wsm) and shared with
 * secure world.
 * The virtual kernel address is added for a simpler search algorithm.
 */
struct tbase_cbuf;

int tbase_cbuf_alloc(struct tbase_client *client, uint32_t len,
		     uintptr_t *addr, struct vm_area_struct *vmarea);

int tbase_cbuf_free(struct tbase_client *client, uintptr_t addr);

struct tbase_cbuf *tbase_cbuf_get_by_addr(struct tbase_client *client,
					  uintptr_t addr);

void tbase_cbuf_get(struct tbase_cbuf *cbuf);

void tbase_cbuf_put(struct tbase_cbuf *cbuf);

uintptr_t tbase_cbuf_addr(struct tbase_cbuf *cbuf);

uintptr_t tbase_cbuf_uaddr(struct tbase_cbuf *cbuf);

uint32_t tbase_cbuf_len(struct tbase_cbuf *cbuf);

#endif /* _CLIENT_H_ */
