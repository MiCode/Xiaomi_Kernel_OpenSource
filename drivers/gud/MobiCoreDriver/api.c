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

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/cred.h>
#include <linux/sched.h>

#include <public/mc_linux.h>	/* MC_MAP_MAX */
#include "main.h"
#include "debug.h"
#include "mcp.h"
#include "admin.h"
#include "session.h"
#include "client.h"
#include "api.h"

static struct api_ctx {
	struct mutex		clients_lock;	/* Clients list + temp notifs */
	struct list_head	clients;	/* List of user-space clients */
} api_ctx;

/*
 * Initialize a new tbase client object
 * @return client pointer or NULL if no allocation was possible.
 */
struct tbase_client *api_open_device(bool is_from_kernel)
{
	struct tbase_client *client;

	/* Allocate and init client object */
	client = client_create(is_from_kernel);
	if (!client) {
		MCDRV_ERROR("Could not create client");
		return NULL;
	}

	/* Add client to list of clients */
	mutex_lock(&api_ctx.clients_lock);
	list_add_tail(&client->list, &api_ctx.clients);
	mutex_unlock(&api_ctx.clients_lock);

	MCDRV_DBG("created client %p", client);
	return client;
}

/*
 * Try and mark client as "closing"
 * @return tbase driver error code
 */
int api_freeze_device(struct tbase_client *client)
{
	int err = 0;

	if (!client_set_closing(client))
		err = -ENOTEMPTY;

	MCDRV_DBG("client %p, exit with %d\n", client, err);
	return err;
}

/*
 * Release a client and the session+cbuf objects it contains.
 * @param client_t client
 * @return tbase driver error code
 */
void api_close_device(struct tbase_client *client)
{
	/* Remove client from list of active clients */
	mutex_lock(&api_ctx.clients_lock);
	list_del(&client->list);
	mutex_unlock(&api_ctx.clients_lock);
	/* Close all remaining sessions */
	client_close_sessions(client);
	client_put(client);
	MCDRV_DBG("client %p closed\n", client);
}

/*
 * Open TA for given client. TA binary is provided by the daemon.
 * @param
 * @return tbase driver error code
 */
int api_open_session(struct tbase_client	*client,
		     uint32_t		*p_session_id,
		     const struct mc_uuid_t *uuid,
		     uintptr_t		tci,
		     size_t		tci_len,
		     bool		is_gp_uuid,
		     struct mc_identity	*identity)
{
	int err = 0;
	uint32_t sid = 0;
	struct tbase_object *obj;

	/* Check parameters */
	if (!p_session_id)
		return -EINVAL;

	if (!uuid)
		return -EINVAL;

	/* Get secure object */
	obj = tbase_object_get(uuid, is_gp_uuid);
	if (IS_ERR(obj)) {
		/* Try to select secure object inside the SWd if not found */
		if ((PTR_ERR(obj) == -ENOENT) && g_ctx.f_ta_auth)
			obj = tbase_object_select(uuid);

		if (IS_ERR(obj)) {
			err = PTR_ERR(obj);
			goto end;
		}
	}

	/* Open session */
	err = client_add_session(client, obj, tci, tci_len, &sid, is_gp_uuid,
				 identity);
	/* Fill in return parameter */
	if (!err)
		*p_session_id = sid;

	/* Delete secure object */
	tbase_object_free(obj);

end:

	MCDRV_DBG("session %x, exit with %d\n", sid, err);
	return err;
}

/*
 * Open TA for given client. TA binary is provided by the client.
 * @param
 * @return tbase driver error code
 */
int api_open_trustlet(struct tbase_client	*client,
		      uint32_t		*p_session_id,
		      uint32_t		spid,
		      uintptr_t		trustlet,
		      size_t		trustlet_len,
		      uintptr_t		tci,
		      size_t		tci_len)
{
	struct tbase_object *obj;
	struct mc_identity identity = {
		.login_type = TEEC_LOGIN_PUBLIC,
	};
	uint32_t sid = 0;
	int err = 0;

	/* Check parameters */
	if (!p_session_id)
		return -EINVAL;

	/* Create secure object from user-space trustlet binary */
	obj = tbase_object_read(spid, trustlet, trustlet_len);
	if (IS_ERR(obj)) {
		err = PTR_ERR(obj);
		goto end;
	}

	/* Open session */
	err = client_add_session(client, obj, tci, tci_len, &sid, false,
				 &identity);
	/* Fill in return parameter */
	if (!err)
		*p_session_id = sid;

	/* Delete secure object */
	tbase_object_free(obj);

end:
	MCDRV_DBG("session %x, exit with %d\n", sid, err);
	return err;
}

/*
 * Close a TA
 * @param
 * @return tbase driver error code
 */
int api_close_session(struct tbase_client *client, uint32_t session_id)
{
	int ret = client_remove_session(client, session_id);

	MCDRV_DBG("session %x, exit with %d\n", session_id, ret);
	return ret;
}

/*
 * Send a notification to TA
 * @return tbase driver error code
 */
int api_notify(struct tbase_client *client, uint32_t session_id)
{
	int err = 0;
	struct tbase_session *session = NULL;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	/* Send command to SWd */
	if (!session) {
		err = -ENXIO;
	} else {
		err = session_notify_swd(session);

		/* Release session */
		client_unref_session(session);
	}

	MCDRV_DBG("session %x, exit with %d\n", session_id, err);
	return err;
}

/*
 * Wait for a notification from TA
 * @return tbase driver error code
 */
int api_wait_notification(struct tbase_client *client,
			  uint32_t session_id,
			  int32_t timeout)
{
	int err = 0;
	struct tbase_session *session = NULL;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	/* Wait for notification */
	if (!session) {
		err = -ENXIO;
	} else {
		err = session_waitnotif(session, timeout);

		/* Release session */
		client_unref_session(session);
	}

	MCDRV_DBG("session %x, exit with %d\n", session_id, err);
	return err;
}

/*
 * Allocate a contiguous buffer (cbuf) for given client
 *
 * @param client		client
 * @param len			size of the cbuf
 * @param **p_addr		pointer to the cbuf kva
 * @return tbase driver error code
 */
int api_malloc_cbuf(struct tbase_client *client, uint32_t len,
		    uintptr_t *addr, struct vm_area_struct *vmarea)
{
	int err = tbase_cbuf_alloc(client, len, addr, vmarea);

	MCDRV_DBG("exit with %d\n", err);
	return err;
}

/*
 * Free a contiguous buffer from given client
 * @param client
 * @param addr		kernel virtual address of the buffer
 *
 * @return tbase driver error code
 */
int api_free_cbuf(struct tbase_client *client, uintptr_t addr)
{
	int err = tbase_cbuf_free(client, addr);

	MCDRV_DBG("@ 0x%lx, exit with %d\n", addr, err);
	return err;
}

/* Share a buffer with given TA in SWd */
int api_map_wsms(struct tbase_client *client, uint32_t session_id,
		 struct mc_ioctl_buffer *bufs)
{
	struct tbase_session *session = NULL;
	int err = 0;

	if (!client)
		return -EINVAL;

	if (!bufs)
		return -EINVAL;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	if (session) {
		/* Add buffer to the session */
		err = session_wsms_add(session, bufs);

		/* Release session */
		client_unref_session(session);
	} else {
		err = -ENXIO;
	}

	MCDRV_DBG("exit with %d\n", err);
	return err;
}

/* Stop sharing a buffer with SWd */
int api_unmap_wsms(struct tbase_client *client, uint32_t session_id,
		   const struct mc_ioctl_buffer *bufs)
{
	struct tbase_session *session = NULL;
	int err = 0;

	if (!client)
		return -EINVAL;

	if (!bufs)
		return -EINVAL;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	if (!session) {
		err = -ENXIO;
	} else {
		/* Remove buffer from session */
		err = session_wsms_remove(session, bufs);
		/* Release session */
		client_unref_session(session);
	}

	MCDRV_DBG("exit with %d\n", err);
	return err;
}

/*
 * Read session exit/termination code
 */
int api_get_session_exitcode(struct tbase_client *client, uint32_t session_id,
			     int32_t *exit_code)
{
	int err = 0;
	struct tbase_session *session;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	if (!session) {
		err = -ENXIO;
	} else {
		/* Retrieve error */
		*exit_code = session_exitcode(session);

		/* Release session */
		client_unref_session(session);

		err = 0;
	}

	MCDRV_DBG("session %x, exit with %d\n", session_id, err);
	return err;
}

void api_init(void)
{
	INIT_LIST_HEAD(&api_ctx.clients);
	mutex_init(&api_ctx.clients_lock);

	INIT_LIST_HEAD(&g_ctx.closing_sess);
	mutex_init(&g_ctx.closing_lock);
}

int api_info(struct kasnprintf_buf *buf)
{
	struct tbase_client *client;
	struct tbase_session *session;
	ssize_t ret = 0;

	mutex_lock(&api_ctx.clients_lock);
	if (list_empty(&api_ctx.clients))
		goto done;

	list_for_each_entry(client, &api_ctx.clients, list) {
		ret = client_info(client, buf);
		if (ret < 0)
			break;
	}

done:
	mutex_unlock(&api_ctx.clients_lock);

	if (ret >= 0) {
		mutex_lock(&g_ctx.closing_lock);
		if (!list_empty(&g_ctx.closing_sess))
			ret = kasnprintf(buf, "closing sessions:\n");

		list_for_each_entry(session, &g_ctx.closing_sess, list) {
			ret = session_info(session, buf);
			if (ret < 0)
				break;
		}

		mutex_unlock(&g_ctx.closing_lock);
	}

	return ret;
}
