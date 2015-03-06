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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/list.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"
#include "public/mobicore_driver_api.h"

#include "main.h"
#include "debug.h"
#include "client.h"
#include "session.h"
#include "api.h"

/* TODO CPI: align codes on user api */

enum mc_result convert(int err)
{
	switch (-err) {
	case 0:
		return MC_DRV_OK;
	case ENOMSG:
		return MC_DRV_NO_NOTIFICATION;
	case EBADMSG:
		return MC_DRV_ERR_NOTIFICATION;
	case ENOSYS:
		return MC_DRV_ERR_NOT_IMPLEMENTED;
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
	case EADDRINUSE:
		return MC_DRV_ERR_BULK_MAPPING;
	case EADDRNOTAVAIL:
		return MC_DRV_ERR_BULK_UNMAPPING;
	case ECOMM:
		return MC_DRV_INFO_NOTIFICATION;
	case EUNATCH:
		return MC_DRV_ERR_NQ_FAILED;
	default:
		MCDRV_DBG("error is %d", err);
		return MC_DRV_ERR_UNKNOWN;
	}
}

static inline bool is_valid_device(uint32_t device_id)
{
	return MC_DEVICE_ID_DEFAULT == device_id;
}

static struct tbase_client *k_client;
atomic_t open_count = ATOMIC_INIT(0);
DEFINE_MUTEX(dev_mutex);	/* Lock for the device */

static bool is_open(void)
{
	return k_client;
}

enum mc_result mc_open_device(uint32_t device_id)
{
	enum mc_result mc_result = MC_DRV_OK;

	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	mutex_lock(&dev_mutex);
	if (!is_open()) {
		atomic_set(&open_count, 0);
		k_client = api_open_device(true);
	}

	if (!is_open()) {
		mc_result = MC_DRV_ERR_INVALID_DEVICE_FILE;
		goto end;
	}

	atomic_inc(&open_count);

end:
	mutex_unlock(&dev_mutex);
	if (mc_result != MC_DRV_OK)
		MCDRV_DBG("Could not open device");
	else
		MCDRV_DBG("Successfully opened the device.");

	return mc_result;
}
EXPORT_SYMBOL(mc_open_device);

enum mc_result mc_close_device(uint32_t device_id)
{
	enum mc_result mc_result;

	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	mutex_lock(&dev_mutex);
	/* Check if already closed */
	if (!is_open()) {
		atomic_set(&open_count, 0);
		mc_result = MC_DRV_OK;
		goto end;
	}

	/* Check if used by another kernel CA */
	if (!atomic_dec_and_test(&open_count)) {
		mc_result = MC_DRV_OK;
		goto end;
	}

	/* Check sessions and freeze client */
	mc_result = convert(api_freeze_device(k_client));
	if (MC_DRV_OK != mc_result)
		goto end;

	/* Close the device */
	api_close_device(k_client);
	k_client = NULL;

end:
	mutex_unlock(&dev_mutex);
	return mc_result;
}
EXPORT_SYMBOL(mc_close_device);

enum mc_result mc_open_session(struct mc_session_handle *session,
			       const struct mc_uuid_t *uuid,
			       uint8_t *tci, uint32_t len)
{
	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	/* Call core api */
	return convert(api_open_session(k_client, &session->session_id, uuid,
					(uintptr_t)tci, len, false));
}
EXPORT_SYMBOL(mc_open_session);

enum mc_result mc_close_session(struct mc_session_handle *session)
{
	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	/* Call core api */
	return convert(api_close_session(k_client, session->session_id));
}
EXPORT_SYMBOL(mc_close_session);

enum mc_result mc_notify(struct mc_session_handle *session)
{
	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	/* Call core api */
	return convert(api_notify(k_client, session->session_id));
}
EXPORT_SYMBOL(mc_notify);

enum mc_result mc_wait_notification(struct mc_session_handle *session,
				    int32_t timeout)
{
	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	/* Call core api */
	return convert(api_wait_notification(k_client, session->session_id,
					     timeout));
}
EXPORT_SYMBOL(mc_wait_notification);

enum mc_result mc_malloc_wsm(uint32_t device_id, uint32_t align, uint32_t len,
			     uint8_t **wsm, uint32_t wsm_flags)
{
	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!len)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!wsm)
		return MC_DRV_ERR_INVALID_PARAMETER;

	/* Call core api */
	return convert(api_malloc_cbuf(k_client, len, (void **)wsm, NULL));
}
EXPORT_SYMBOL(mc_malloc_wsm);

enum mc_result mc_free_wsm(uint32_t device_id, uint8_t *wsm)
{
	/* Check parameters */
	if (!is_valid_device(device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	/* Call core api */
	return convert(api_free_cbuf(k_client, (void *)wsm));
}
EXPORT_SYMBOL(mc_free_wsm);

enum mc_result mc_map(struct mc_session_handle *session, void *buf,
		      uint32_t buf_len, struct mc_bulk_map *map_info)
{
	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!map_info)
		return MC_DRV_ERR_INVALID_PARAMETER;

	/* Call core api */
	return convert(api_map_wsm(k_client, session->session_id, buf, buf_len,
				   &map_info->secure_virt_addr,
				   &map_info->secure_virt_len));
}
EXPORT_SYMBOL(mc_map);

enum mc_result mc_unmap(struct mc_session_handle *session, void *buf,
			struct mc_bulk_map *map_info)
{
	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!map_info)
		return MC_DRV_ERR_INVALID_PARAMETER;

	/* Call core api */
	return convert(api_unmap_wsm(k_client, session->session_id, buf,
				     map_info->secure_virt_addr,
				     map_info->secure_virt_len));
}
EXPORT_SYMBOL(mc_unmap);

enum mc_result mc_get_session_error_code(struct mc_session_handle *session,
					 int32_t *last_error)
{
	/* Check parameters */
	if (!session)
		return MC_DRV_ERR_INVALID_PARAMETER;

	if (!is_valid_device(session->device_id))
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!is_open())
		return MC_DRV_ERR_UNKNOWN_DEVICE;

	if (!last_error)
		return MC_DRV_ERR_INVALID_PARAMETER;

	/* Call core api */
	return convert(api_get_session_error(k_client, session->session_id,
					     last_error));
}
EXPORT_SYMBOL(mc_get_session_error_code);
