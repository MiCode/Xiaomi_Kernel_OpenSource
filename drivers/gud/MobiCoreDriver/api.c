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

#include "main.h"
#include "debug.h"
#include "mcp.h"
#include "api.h"

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
	mc_add_client(client);

	MCDRV_DBG("Created client 0x%p", client);
	return client;
}

/*
 * Try and mark client as "closing"
 * @return tbase driver error code
 */
int api_freeze_device(struct tbase_client *client)
{
	int err = 0;

	if (!mc_ref_client(client)) {
		err = -ENODEV;
		goto end;
	}

	if (!client_set_closing(client))
		err = -ENOTEMPTY;

	mc_unref_client(client);
end:
	MCDRV_DBG("client 0x%p, exit with %d\n", client, err);
	return err;
}

/*
 * Release a client and the session+cbuf objects it contains.
 * @param client_t client
 * @return tbase driver error code
 */
int api_close_device(struct tbase_client *client)
{
	/* Remove client from list of active clients */
	int err = mc_remove_client(client);

	if (!err) {
		client_close_sessions(client);
		mc_unref_client(client);
	}

	MCDRV_DBG("client 0x%p, exit with %d\n\n", client, err);
	return err;
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
		     bool		is_gp_uuid)
{
	int err = 0;
	uint32_t sid = 0;
	struct tbase_object *obj;

	/* Check parameters */
	if (!p_session_id)
		return -EINVAL;

	if (!uuid)
		return -EINVAL;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

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
	err = client_add_session(client, obj, (void *)tci, tci_len, &sid,
				 is_gp_uuid);
	/* Fill in return parameter */
	if (!err)
		*p_session_id = sid;

	/* Delete secure object */
	tbase_object_free(obj);

end:
	/* Release client */
	mc_unref_client(client);

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
	int err = 0;
	uint32_t sid = 0;
	struct tbase_object *obj;

	/* Check parameters */
	if (!p_session_id)
		return -EINVAL;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

	/* Create secure object from user-space trustlet binary */
	obj = tbase_object_read(spid, trustlet, trustlet_len);
	if (IS_ERR(obj)) {
		err = PTR_ERR(obj);
		goto end;
	}

	/* Open session */
	err = client_add_session(client, obj, (void *)tci, tci_len, &sid,
				 false);
	/* Fill in return parameter */
	if (!err)
		*p_session_id = sid;

	/* Delete secure object */
	tbase_object_free(obj);

end:
	/* Release client */
	mc_unref_client(client);

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
	int err = 0;

	/* Acquire client */
	if (!mc_ref_client(client)) {
		err = -ENODEV;
		goto end;
	}

	/* Close session */
	err = client_remove_session(client, session_id);

	/* Release client */
	mc_unref_client(client);

end:
	MCDRV_DBG("session %x, exit with %d\n", session_id, err);
	return err;
}

/*
 * Send a notification to TA
 * @return tbase driver error code
 */
int api_notify(struct tbase_client *client, uint32_t session_id)
{
	int err = 0;
	struct tbase_session *session = NULL;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

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

	/* Release client */
	mc_unref_client(client);

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

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	/* Wait for notification */
	if (!session) {
		err = -ENXIO;
	} else {
		err = session_wait(session, timeout);

		/* Release session */
		client_unref_session(session);
	}

	/* Release client */
	mc_unref_client(client);

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
		    void **p_addr, struct vm_area_struct *vmarea)
{
	int err = 0;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

	/* Allocate buffer */
	err = tbase_cbuf_alloc(client, len, p_addr, vmarea);

	/* Release client */
	mc_unref_client(client);

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
int api_free_cbuf(struct tbase_client *client, void *addr)
{
	int err = 0;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

	/* Free buffer */
	err = tbase_cbuf_free(client, addr);

	/* Release client */
	mc_unref_client(client);

	MCDRV_DBG("@ 0x%p, exit with %d\n", addr, err);
	return err;
}

/*
 * Share a buffer with given TA in SWd
 * @return tbase driver error code
 */
int api_map_wsm(struct tbase_client *client, uint32_t session_id, void *buf,
		uint32_t len, uint32_t *p_sva, uint32_t *p_slen)
{
	int err = 0;
	struct tbase_session *session = NULL;
	uint32_t sva;

	/* Check parameters */
	if (!buf || !len || !p_sva || !p_slen)
		return -EINVAL;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	if (session) {
		/* Add buffer to the session */
		err = session_add_wsm(session, buf, len, &sva);

		/* Release session */
		client_unref_session(session);
	} else {
		err = -ENXIO;
	}

	if (err) {
		*p_sva = 0;
		*p_slen = 0;
		MCDRV_ERROR("code %d", err);
	} else {
		*p_sva = sva;
		*p_slen = len;
	}

	/* Release client */
	mc_unref_client(client);

	MCDRV_DBG("0x%p, exit with %d\n", buf, err);
	return err;
}

/*
 * Stop sharing a buffer with SWd
 * @param client
 * @param session_id
 * @param buf
 * @param map_info
 * @return tbase driver error code
 */
int api_unmap_wsm(struct tbase_client *client, uint32_t session_id,
		  void *buf, uint32_t sva, uint32_t slen)
{
	int err = 0;
	struct tbase_session *session = NULL;

	/* Check parameters */
	if (!client)
		return -EINVAL;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	if (!session) {
		err = -ENXIO;
	} else {
		/* Remove buffer from session */
		err = session_remove_wsm(session, buf, sva, slen);

		/* Release session */
		client_unref_session(session);
	}

	/* Release client */
	mc_unref_client(client);

	MCDRV_DBG("0x%p, exit with %d\n", buf, err);
	return err;
}

/*
 * Read last error from received notifications
 * @return tbase driver error code
 */
int api_get_session_error(struct tbase_client *client, uint32_t session_id,
			  int32_t *last_error)
{
	int err = 0;
	struct tbase_session *session;

	/* Acquire client */
	if (!mc_ref_client(client))
		return -ENODEV;

	/* Acquire session */
	session = client_ref_session(client, session_id);

	if (!session) {
		err = -ENXIO;
	} else {
		/* Retrieve error */
		*last_error = session_get_err(session);

		/* Release session */
		client_unref_session(session);

		err = 0;
	}

	/* Release client */
	mc_unref_client(client);

	MCDRV_DBG("session %x, exit with %d\n", session_id, err);
	return err;
}
