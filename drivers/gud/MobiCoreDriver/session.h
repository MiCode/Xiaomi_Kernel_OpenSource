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

#ifndef _SESSION_H_
#define _SESSION_H_

#include <linux/list.h>
#include "mmu.h"

#define NOTIF_OK	0		/* No error in notification */

struct tbase_wsm;
struct tbase_cbuf;

struct tbase_session {
	/* Work descriptor to handle delayed closing (MUST BE FIRST) */
	struct work_struct	close_work;
	/* session lock */
	struct mutex		lock;
	/* owner */
	struct tbase_client		*client;
	/* Session id */
	uint32_t		id;
	/* Number of references kept to this object */
	struct kref		kref;
	/* The list entry to attach to session list of owner */
	struct list_head	list;
	/* Last notification error */
	int32_t			last_error;
	/* List of wsm for a session */
	struct list_head	wsm_list;
	/* Notification queue */
	wait_queue_head_t	nq;
	/* Notification flag */
	bool			nq_flag;
};

struct tbase_session *session_create(struct tbase_client *client, uint32_t sid);

void session_remove(struct tbase_session *session);

int session_close(struct tbase_session *session);

static inline void session_get(struct tbase_session *session)
{
	kref_get(&session->kref);
}

void session_put(struct tbase_session *session);

struct tbase_wsm *session_alloc_wsm(void *buf, uint32_t len, uint32_t sva,
				    struct tbase_mmu *table,
				    struct tbase_cbuf *cbuf);

void session_free_wsm(struct tbase_wsm *wsm);

void session_link_wsm(struct tbase_session *session, struct tbase_wsm *wsm);

int session_add_wsm(struct tbase_session *session, void *buf, uint32_t len,
		    uint32_t *p_sva);

int session_remove_wsm(struct tbase_session *session, void *buf,
		       uint32_t sva, uint32_t len);

int32_t session_get_err(struct tbase_session *session);

int session_notify_swd(struct tbase_session *session);

int session_wait(struct tbase_session *session, int32_t timeout);

void session_notify_nwd(struct tbase_session *session, int32_t error);

#endif /* _SESSION_H_ */
