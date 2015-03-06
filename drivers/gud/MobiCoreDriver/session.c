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
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/err.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "main.h"
#include "fastcall.h"
#include "debug.h"
#include "mcp.h"
#include "api.h"
#include "scheduler.h"		/* nq_notify */

struct tbase_wsm {
	/* Buffer NWd addr (uva or kva, used only for lookup) */
	void			*buff;
	/* buffer length */
	uint32_t		len;
	/* Buffer SWd addr */
	uint32_t		s_buff;
	/* mmu L2 table */
	struct tbase_mmu	*table;
	/* possibly a pointer to a cbuf */
	struct tbase_cbuf	*cbuf;
	/* list node */
	struct list_head	list;
};

/*
 * Postponed closing for GP TAs.
 * Implemented as a worker because cannot be executed from within isr_worker.
 */
static void session_close_worker(struct work_struct *work)
{
	struct tbase_session *session = (struct tbase_session *)work;

	session_close(session);
	/* Release ref taken before scheduling worker */
	session_put(session);
}

/*
 * Create a session object.
 * Note: object is not attached to client yet.
 */
struct tbase_session *session_create(struct tbase_client *client, uint32_t sid)
{
	struct tbase_session *session;

	/* Allocate session object */
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return NULL;

	/* Initialise object members */
	mutex_init(&session->lock);
	session->client = client;
	session->id = sid;
	kref_init(&session->kref);
	/* Get session to check early notifs */
	session_get(session);
	INIT_LIST_HEAD(&session->list);
	session->last_error = NOTIF_OK;
	INIT_LIST_HEAD(&session->wsm_list);
	init_waitqueue_head(&session->nq);
	session->nq_flag = false;
	INIT_WORK(&session->close_work, session_close_worker);

	return session;
}

/*
 * Move a session object to the closing list.
 * Needed for GP sessions that may linger there waiting for SWd ack.
 */
void session_remove(struct tbase_session *session)
{
	mutex_lock(&g_ctx.closing_lock);
	if (likely(session))
		list_move(&session->list, &g_ctx.closing_sess);
	mutex_unlock(&g_ctx.closing_lock);
}

/*
 * Close TA and unreference session object.
 * Object will be freed if reference reaches 0.
 * Session object is assumed to have been removed from main list.
 */
int session_close(struct tbase_session *session)
{
	int err = 0;

	if (!session)
		return -ENXIO;

	/* "if(list_empty)" must be atomic with "list_del_init" */
	mutex_lock(&session->lock);

	/* Check if already terminated. May happen if TA dies while we close */
	if (list_empty(&session->list)) {
		mutex_unlock(&session->lock);
		return 0;
	}

	/* Close TA BEFORE session object cleanup, to prevent orphan wsm */
	err = mcp_close_session(session->id);
	if (!err) {
		/* TA is closed, remove object from closing list */
		mutex_lock(&g_ctx.closing_lock);
		list_del_init(&session->list); /* init to allow list_empty */
		mutex_unlock(&g_ctx.closing_lock);
	}

	mutex_unlock(&session->lock);

	/* Handle NWd session object */
	switch (err) {
	case 0:
		/* Remove the ref we took on creation */
		session_put(session);
		break;
	case -EBUSY:
		/*
		 * (GP) TA needs time to close. The "TA closed" notification
		 * will trigger a new call to session_close().
		 * Return OK but do not unref.
		 */
		err = 0;
		break;
	default:
		MCDRV_ERROR("Failed to close session %x in SWd: %d",
			    session->id, err);
		break;
	}

	return err;
}

/*
 * Free session object and all objects it contains (wsm).
 */
static void session_release(struct kref *kref)
{
	struct tbase_session *session;
	struct tbase_wsm *wsm, *next;

	/* Remove remaining shared buffers (unmapped in SWd by mcp_close) */
	session = container_of(kref, struct tbase_session, kref);
	list_for_each_entry_safe(wsm, next, &session->wsm_list, list) {
		list_del(&wsm->list);
		MCDRV_DBG("Removed WSM @ 0x%p", wsm->buff);
		session_free_wsm(wsm);
	}

	kfree(session);
}

/*
 * Unreference session.
 * Free session object if reference reaches 0.
 * MUST be called OUTSIDE of session->lock
 */
void session_put(struct tbase_session *session)
{
	if (likely(session))
		kref_put(&session->kref, session_release);
}

/*
 * Propagate a SWd notif to session object
 */
void session_notify_nwd(struct tbase_session *session, int32_t error)
{
	/* Check parameters */
	if (!session) {
		MCDRV_ERROR("Session pointer is null");
		return;
	}

	/* Set payload, without overwriting an existing one */
	mutex_lock(&session->lock);
	if (!session->last_error)
		session->last_error = error;

	mutex_unlock(&session->lock);

	session->nq_flag = true;

	wake_up_interruptible(&session->nq);
}

/*
 * Send a notification to TA
 */
int session_notify_swd(struct tbase_session *session)
{
	if (!session) {
		MCDRV_ERROR("Session pointer is null");
		return -EINVAL;
	}

	return mc_dev_notify(session->id);
}

/*
 * Read and clear last code in notification received from TA.
 */
int32_t session_get_err(struct tbase_session *session)
{
	int32_t code;

	mutex_lock(&session->lock);
	code = session->last_error;
	session->last_error = 0;
	mutex_unlock(&session->lock);

	return code;
}

/*
 * Allocate a WSM object
 */
struct tbase_wsm *session_alloc_wsm(void *buf, uint32_t len, uint32_t sva,
				    struct tbase_mmu *table,
				    struct tbase_cbuf *cbuf)
{
	struct tbase_wsm *wsm = kzalloc(sizeof(*wsm), GFP_KERNEL);

	if (wsm) {
		wsm->buff = buf;
		wsm->len = len;
		wsm->s_buff = sva;
		wsm->table = table;
		wsm->cbuf = cbuf;
		INIT_LIST_HEAD(&wsm->list);
	}
	return wsm;
}

/*
 * Free a WSM object
 */
void session_free_wsm(struct tbase_wsm *wsm)
{
	/* Free MMU table */
	if (wsm->table)
		tbase_mmu_delete(wsm->table);

	/* Unref cbuf if applicable */
	if (wsm->cbuf)
		tbase_cbuf_put(wsm->cbuf);

	/* Delete wsm object */
	kfree(wsm);
}

/*
 * Add a WSM object to session object.
 * Assume session lock is already taken, or no need to.
 */
void session_link_wsm(struct tbase_session *session, struct tbase_wsm *wsm)
{
	list_add(&wsm->list, &session->wsm_list);
}

/*
 * Share a buffer with SWd and add corresponding WSM object to session.
 */
int session_add_wsm(struct tbase_session *session, void *buf, uint32_t len,
		    uint32_t *p_sva)
{
	int err = 0;
	struct tbase_cbuf *cbuf = NULL;
	struct tbase_mmu *mmu = NULL;
	struct tbase_wsm *wsm = NULL;
	void *va;
	size_t offset;
	struct task_struct *task = NULL;
	uint32_t sva = 0;

	/* Check parameters */
	if (!session)
		return -ENXIO;

	/* Lock the session */
	mutex_lock(&session->lock);

	/*
	 * Search wsm list for overlaps.
	 * At the moment a buffer can only be shared once per session.
	 */
	list_for_each_entry(wsm, &session->wsm_list, list) {
		if (buf < (wsm->buff + wsm->len) && (buf + len) > wsm->buff) {
			MCDRV_ERROR("Buffer @ 0x%p overlaps with existing wsm",
				    wsm->buff);
			err = -EADDRINUSE;
			goto unlock;
		}
	}

	do {
		/* Check if buffer is contained in a cbuf */
		cbuf = tbase_cbuf_get_by_addr(session->client, buf);
		if (cbuf) {
			if (client_is_kernel(session->client))
				offset = buf - tbase_cbuf_addr(cbuf);
			else
				offset = buf - tbase_cbuf_uaddr(cbuf);

			if ((offset + len) > tbase_cbuf_len(cbuf)) {
				err = -EINVAL;
				MCDRV_ERROR("crosses cbuf boundary");
				break;
			}
			/* Provide kernel virtual address */
			va = tbase_cbuf_addr(cbuf) + offset;
			task = NULL;
		}
		/* Not a cbuf. va is uva or kva depending on client. */
		/* Provide "task" if client is user */
		else {
			va = buf;
			if (!client_is_kernel(session->client))
				task = current;
		}

		/* Build MMU table for buffer */
		mmu = tbase_mmu_create(task, va, len);
		if (IS_ERR(mmu)) {
			err = PTR_ERR(mmu);
			mmu = NULL;
			break;
		}

		/* Send MCP message to map buffer in SWd */
		err = mcp_map(session->id, buf, len, mmu, &sva);
		if (err)
			break;

		/* Alloc wsm object */
		wsm = session_alloc_wsm(buf, len, sva, mmu, cbuf);
		if (!wsm) {
			err = -ENOMEM;
			break;
		}

		/* Add wsm to list */
		session_link_wsm(session, wsm);

	} while (0);

	/* Fill in return parameter */
	if (!err) {
		*p_sva = sva;
	} else {
		/* Cleanup if error */
		*p_sva = 0;
		if (cbuf)
			tbase_cbuf_put(cbuf);

		if (sva)
			mcp_unmap(session->id, sva, len, mmu);

		if (mmu)
			tbase_mmu_delete(mmu);
	}

	/* Unlock the session */
unlock:
	mutex_unlock(&session->lock);

	return err;
}

/*
 * Stop sharing buffer and delete corrsponding WSM object.
 */
int session_remove_wsm(struct tbase_session *session, void *buf, uint32_t sva,
		       uint32_t len)
{
	struct tbase_wsm *wsm = NULL, *candidate;
	int err = 0;

	if (!buf) {
		MCDRV_ERROR("buf is null");
		return -EINVAL;
	}
	if (!session) {
		MCDRV_ERROR("session pointer is null");
		return -EINVAL;
	}

	/* Lock the session */
	mutex_lock(&session->lock);

	do {
		/* Find buffer */
		list_for_each_entry(candidate, &session->wsm_list, list) {
			if (candidate->buff == buf)
				wsm = candidate;
		}

		if (!wsm) {
			err = -EADDRNOTAVAIL;
			break;
		}

		/* Check input params coherency */
		/* CPI TODO: Fix the spec, "len" is NOT ignored anymore */
		if ((wsm->s_buff != sva) || (wsm->len != len)) {
			err = -EINVAL;
			break;
		}

		/* Send MCP command to unmap buffer in SWd */
		err = mcp_unmap(session->id, (uint32_t)wsm->s_buff, wsm->len,
				wsm->table);
		if (err)
			break;

		/* Remove wsm from the list of its owner session */
		list_del(&wsm->list);

		/* Free wsm */
		session_free_wsm(wsm);

	} while (0);

	mutex_unlock(&session->lock);

	return err;
}

/*
 * Sleep until next notification from SWd.
 */
int session_wait(struct tbase_session *session, int32_t timeout)
{
	int err = 1; /* impossible value */
	int ret;
	bool infinite = false;
	unsigned long jiffies = 0;

	if (!session) {
		MCDRV_ERROR("session pointer is null");
		return -EINVAL;
	}

	/* Translate timeout into jiffies */
	if (timeout < 0) {
		/* Wake up every 1sec to check that session is still alive */
		timeout = 1000;
		infinite = true;
	}
	jiffies = (unsigned long)timeout * HZ / 1000;

	/*
	 * Only case we loop is on "1sec check" with session still alive.
	 * Session lock not needed since each session action is atomic
	 * and depends only on "ret".
	 */
	do {
		ret = wait_event_interruptible_timeout(session->nq,
						       session->nq_flag,
						       jiffies);
		session->nq_flag = false;

		/* Timeout */
		if (!ret) {
			if (infinite) {
				if (list_empty(&session->list)) {
					/* Detached <=> TA closed */
					err = -ENXIO;
				}
			} else {
				err = -ETIME;
			}
		}

		/* We did receive a notification */
		else if (ret > 0) {
			/* Handle notif payload */
			if (NOTIF_OK != session->last_error)
				err = -ECOMM;
			else
				err = 0;
		}

		/* We have been interrupted (-ERESTARTSYS) */
		else
			err = ret;
	} while (err == 1);

	return err;
}
