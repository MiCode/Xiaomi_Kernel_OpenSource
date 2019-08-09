/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/list.h>

#include "public/mc_user.h"
#include "public/mc_admin.h"
#include "public/mobicore_driver_api.h"

#include "main.h"
#include "client.h"

static enum mc_result convert(int err)
{
	switch (-err) {
	case 0:
		return MC_DRV_OK;
	case ENOMSG:
		return MC_DRV_NO_NOTIFICATION;
	case EBADMSG:
		return MC_DRV_ERR_NOTIFICATION;
	case EAGAIN:
		return MC_DRV_ERR_OUT_OF_RESOURCES;
	case EHOSTDOWN:
		return MC_DRV_ERR_INIT;
	case ENODEV:
		return MC_DRV_ERR_UNKNOWN_DEVICE;
	case ENXIO:
		return MC_DRV_ERR_UNKNOWN_SESSION;
	case EPERM:
		return MC_DRV_ERR_INVALID_OPERATION;
	case EBADE:
		return MC_DRV_ERR_INVALID_RESPONSE;
	case ETIME:
		return MC_DRV_ERR_TIMEOUT;
	case ENOMEM:
		return MC_DRV_ERR_NO_FREE_MEMORY;
	case EUCLEAN:
		return MC_DRV_ERR_FREE_MEMORY_FAILED;
	case ENOTEMPTY:
		return MC_DRV_ERR_SESSION_PENDING;
	case EHOSTUNREACH:
		return MC_DRV_ERR_DAEMON_UNREACHABLE;
	case ENOENT:
		return MC_DRV_ERR_INVALID_DEVICE_FILE;
	case EINVAL:
		return MC_DRV_ERR_INVALID_PARAMETER;
	case EPROTO:
		return MC_DRV_ERR_KERNEL_MODULE;
	case ECOMM:
		return MC_DRV_INFO_NOTIFICATION;
	case EUNATCH:
		return MC_DRV_ERR_NQ_FAILED;
	case ERESTARTSYS:
		return MC_DRV_ERR_INTERRUPTED_BY_SIGNAL;
	default:
		mc_dev_devel("error is %d\n", err);
		return MC_DRV_ERR_UNKNOWN;
	}
}

static inline bool is_valid_device(u32 device_id)
{
	return device_id == MC_DEVICE_ID_DEFAULT;
}

static struct tee_client *client;
static int open_count;
static DEFINE_MUTEX(dev_mutex);	/* Lock for the device */

static bool clientlib_client_get(void)
{
	int ret = true;

	mutex_lock(&dev_mutex);
	if (!client)
		ret = false;
	else
		client_get(client);

	mutex_unlock(&dev_mutex);
	return ret;
}

static void clientlib_client_put(void)
{
	mutex_lock(&dev_mutex);
	if (client_put(client))
		client = NULL;
	mutex_unlock(&dev_mutex);
}

enum mc_result mc_open_device(u32 device_id)
{
	enum mc_result mc_result = MC_DRV_OK;

	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	mutex_lock(&dev_mutex);
	if (!open_count)
		client = client_create(true);

	if (client) {
		open_count++;
		mc_dev_devel("Successfully opened the device\n");
	} else {
		mc_result = MC_DRV_ERR_INVALID_DEVICE_FILE;
		mc_dev_devel("Could not open device\n");
	}

	mutex_unlock(&dev_mutex);
	return mc_result;
}
EXPORT_SYMBOL(mc_open_device);

enum mc_result mc_close_device(u32 device_id)
{
	enum mc_result mc_result = MC_DRV_OK;

	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	mutex_lock(&dev_mutex);

	if (open_count > 1) {
		open_count--;
		goto end;
	}

	/* Check sessions and freeze client */
	if (client_has_sessions(client)) {
		mc_result = MC_DRV_ERR_SESSION_PENDING;
		goto end;
	}

	/* Close the device */
	client_close(client);
	open_count = 0;

end:
	mutex_unlock(&dev_mutex);
	clientlib_client_put();
	return mc_result;
}
EXPORT_SYMBOL(mc_close_device);

enum mc_result mc_open_session(struct mc_session_handle *session,
			       const struct mc_uuid_t *uuid, u8 *tci, u32 len)
{
	struct mc_identity identity = {
		.login_type = LOGIN_PUBLIC,
	};
	enum mc_result ret;

	/* Check parameters */
	if (!session || !uuid)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_open_session(client, &session->session_id, uuid,
					  (uintptr_t)tci, len, false,
					  &identity, 0, 0));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_open_session);

enum mc_result mc_open_trustlet(struct mc_session_handle *session, u32 spid,
				u8 *trustlet, u32 trustlet_len,
				u8 *tci, u32 len)
{
	enum mc_result ret;

	/* Check parameters */
	if (!session || !trustlet)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_open_trustlet(client, &session->session_id, spid,
					   (uintptr_t)trustlet, trustlet_len,
					   (uintptr_t)tci, len, 0, 0));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_open_trustlet);

enum mc_result mc_close_session(struct mc_session_handle *session)
{
	enum mc_result ret;

	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_remove_session(client, session->session_id));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_close_session);

enum mc_result mc_notify(struct mc_session_handle *session)
{
	enum mc_result ret;

	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_notify_session(client, session->session_id));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_notify);

enum mc_result mc_wait_notification(struct mc_session_handle *session,
				    s32 timeout)
{
	enum mc_result ret;

	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_waitnotif_session(client, session->session_id,
					       timeout, false));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_wait_notification);

enum mc_result mc_malloc_wsm(u32 device_id, u32 align, u32 len, u8 **wsm,
			     u32 wsm_flags)
{
	enum mc_result ret;
	uintptr_t va;

	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!len)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!wsm)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_cbuf_create(client, len, &va, NULL));
	if (ret == MC_DRV_OK)
		*wsm = (u8 *)va;

	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_malloc_wsm);

enum mc_result mc_free_wsm(u32 device_id, u8 *wsm)
{
	enum mc_result ret;
	uintptr_t va = (uintptr_t)wsm;

	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_cbuf_free(client, va));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_free_wsm);

enum mc_result mc_map(struct mc_session_handle *session, void *address,
		      u32 length, struct mc_bulk_map *map_info)
{
	enum mc_result ret;
	struct mc_ioctl_buffer bufs[MC_MAP_MAX];
	u32 i;

	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!map_info)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	bufs[0].va = (uintptr_t)address;
	bufs[0].len = length;
	for (i = 1; i < MC_MAP_MAX; i++)
		bufs[i].va = 0;

	ret = convert(client_map_session_wsms(client, session->session_id,
					      bufs));
	if (ret == MC_DRV_OK) {
		map_info->secure_virt_addr = bufs[0].sva;
		map_info->secure_virt_len = bufs[0].len;
	}

	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_map);

enum mc_result mc_unmap(struct mc_session_handle *session, void *address,
			struct mc_bulk_map *map_info)
{
	enum mc_result ret;
	struct mc_ioctl_buffer bufs[MC_MAP_MAX];
	u32 i;

	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!map_info)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	bufs[0].va = (uintptr_t)address;
	bufs[0].len = map_info->secure_virt_len;
	bufs[0].sva = map_info->secure_virt_addr;
	for (i = 1; i < MC_MAP_MAX; i++)
		bufs[i].va = 0;

	ret = convert(client_unmap_session_wsms(client, session->session_id,
						bufs));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_unmap);

enum mc_result mc_get_session_error_code(struct mc_session_handle *session,
					 s32 *exit_code)
{
	enum mc_result ret;

	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!exit_code)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!clientlib_client_get())
		return MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN;

	/* Call core api */
	ret = convert(client_get_session_exitcode(client, session->session_id,
						  exit_code));
	clientlib_client_put();
	return ret;
}
EXPORT_SYMBOL(mc_get_session_error_code);
