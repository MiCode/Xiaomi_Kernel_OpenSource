/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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
#include <linux/sched.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <net/net_namespace.h>
#include <linux/list.h>

#include "public/mobicore_driver_api.h"
#include "public/mobicore_driver_cmd.h"
#include "include/mcinq.h"
#include "device.h"
#include "session.h"

/* device list */
LIST_HEAD(devices);
atomic_t device_usage = ATOMIC_INIT(0);

static struct mcore_device_t *resolve_device_id(uint32_t device_id)
{
	struct mcore_device_t *tmp;
	struct list_head *pos;

	/* Get mcore_device_t for device_id */
	list_for_each(pos, &devices) {
		tmp = list_entry(pos, struct mcore_device_t, list);
		if (tmp->device_id == device_id)
			return tmp;
	}
	return NULL;
}

static void add_device(struct mcore_device_t *device)
{
	list_add_tail(&(device->list), &devices);
}

static bool remove_device(uint32_t device_id)
{
	struct mcore_device_t *tmp;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &devices) {
		tmp = list_entry(pos, struct mcore_device_t, list);
		if (tmp->device_id == device_id) {
			list_del(pos);
			mcore_device_cleanup(tmp);
			return true;
		}
	}
	return false;
}

enum mc_result mc_open_device(uint32_t device_id)
{
	enum mc_result mc_result = MC_DRV_OK;
	struct connection *dev_con = NULL;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		struct mcore_device_t *device = resolve_device_id(device_id);
		if (device != NULL) {
			MCDRV_DBG(mc_kapi,
				  "Device %d already opened\n", device_id);
			atomic_inc(&device_usage);
			mc_result = MC_DRV_OK;
			break;
		}

		/* Open new connection to device */
		dev_con = connection_new();
		if (dev_con == NULL) {
			mc_result = MC_DRV_ERR_NO_FREE_MEMORY;
			break;
		}

		if (!connection_connect(dev_con, MC_DAEMON_PID)) {
			MCDRV_DBG_ERROR(
				mc_kapi,
				"Could not setup netlink connection to PID %u",
				MC_DAEMON_PID);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		/* Forward device open to the daemon and read result */
		struct mc_drv_cmd_open_device_t mc_drv_cmd_open_device = {
			{
				MC_DRV_CMD_OPEN_DEVICE
			},
			{
				device_id
			}
		};

		int len = connection_write_data(
				dev_con,
				&mc_drv_cmd_open_device,
				sizeof(struct mc_drv_cmd_open_device_t));
		if (len < 0) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_OPEN_DEVICE writeCmd failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		struct mc_drv_response_header_t rsp_header;
		memset(&rsp_header, 0, sizeof(rsp_header));
		len = connection_read_datablock(
					dev_con,
					&rsp_header,
					sizeof(rsp_header));
		if (len != sizeof(rsp_header)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_OPEN_DEVICE readRsp failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}
		if (rsp_header.response_id != MC_DRV_RSP_OK) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_OPEN_DEVICE failed, respId=%d",
					rsp_header.response_id);
			switch (rsp_header.response_id) {
			case MC_DRV_RSP_PAYLOAD_LENGTH_ERROR:
				mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
				break;
			case MC_DRV_INVALID_DEVICE_NAME:
				mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
				break;
			case MC_DRV_RSP_DEVICE_ALREADY_OPENED:
			default:
				mc_result = MC_DRV_ERR_INVALID_OPERATION;
				break;
			}
			break;
		}

		/* there is no payload to read */

		device = mcore_device_create(device_id, dev_con);
		if (device == NULL) {
			mc_result = MC_DRV_ERR_NO_FREE_MEMORY;
			break;
		}
		if (!mcore_device_open(device, MC_DRV_MOD_DEVNODE_FULLPATH)) {
			mcore_device_cleanup(device);
			MCDRV_DBG_ERROR(mc_kapi,
					"could not open device file: %s",
					MC_DRV_MOD_DEVNODE_FULLPATH);
			mc_result = MC_DRV_ERR_INVALID_DEVICE_FILE;
			break;
		}

		add_device(device);
		atomic_inc(&device_usage);

	} while (false);

	if (mc_result != MC_DRV_OK)
		connection_cleanup(dev_con);

	return mc_result;
}
EXPORT_SYMBOL(mc_open_device);

enum mc_result mc_close_device(uint32_t device_id)
{
	enum mc_result mc_result = MC_DRV_OK;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		struct mcore_device_t *device = resolve_device_id(device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}
		/* Check if it's not used by other modules */
		if (!atomic_dec_and_test(&device_usage)) {
			mc_result = MC_DRV_OK;
			break;
		}

		struct connection *dev_con = device->connection;

		/* Return if not all sessions have been closed */
		if (mcore_device_has_sessions(device)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"cannot close with sessions pending");
			mc_result = MC_DRV_ERR_SESSION_PENDING;
			break;
		}

		struct mc_drv_cmd_close_device_t mc_drv_cmd_close_device = {
			{
				MC_DRV_CMD_CLOSE_DEVICE
			}
		};
		int len = connection_write_data(
				dev_con,
				&mc_drv_cmd_close_device,
				sizeof(struct mc_drv_cmd_close_device_t));
		/* ignore error, but log details */
		if (len < 0) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_CLOSE_DEVICE writeCmd failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
		}

		struct mc_drv_response_header_t rsp_header;
		memset(&rsp_header, 0, sizeof(rsp_header));
		len = connection_read_datablock(
					dev_con,
					&rsp_header,
					sizeof(rsp_header));
		if (len != sizeof(rsp_header)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_CLOSE_DEVICE readResp failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		if (rsp_header.response_id != MC_DRV_RSP_OK) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_CLOSE_DEVICE failed, respId=%d",
					rsp_header.response_id);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		remove_device(device_id);

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_close_device);

enum mc_result mc_open_session(struct mc_session_handle *session,
			       const struct mc_uuid_t *uuid,
			       uint8_t *tci, uint32_t tci_len)
{
	enum mc_result mc_result = MC_DRV_OK;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		if (session == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}
		if (uuid == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "UUID is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}
		if (tci == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "TCI is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}
		if (tci_len > MC_MAX_TCI_LEN) {
			MCDRV_DBG_ERROR(mc_kapi, "TCI length is longer than %d",
					MC_MAX_TCI_LEN);
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		/* Get the device associated with the given session */
		struct mcore_device_t *device =
				resolve_device_id(session->device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}
		struct connection *dev_con = device->connection;

		/* Get the wsm of the given TCI */
		struct wsm *wsm =
			mcore_device_find_contiguous_wsm(device, tci);
		if (wsm == NULL) {
			MCDRV_DBG_ERROR(mc_kapi,
					"Could not resolve TCI address ");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		if (wsm->len < tci_len) {
			MCDRV_DBG_ERROR(mc_kapi,
					"length is more than allocated TCI");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		/* Prepare open session command */
		struct mc_drv_cmd_open_session_t cmd_open_session = {
			{
				MC_DRV_CMD_OPEN_SESSION
			},
			{
				session->device_id,
				*uuid,
				(uint32_t)((uintptr_t)(wsm->virt_addr) & 0xFFF),
				wsm->handle,
				tci_len
			}
		};

		/* Transmit command data */
		int len = connection_write_data(dev_con,
						&cmd_open_session,
						sizeof(cmd_open_session));
		if (len != sizeof(cmd_open_session)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_OPEN_SESSION writeData failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		/* Read command response */

		/* read header first */
		struct mc_drv_response_header_t rsp_header;
		memset(&rsp_header, 0, sizeof(rsp_header));
		len = connection_read_datablock(dev_con,
						&rsp_header,
						sizeof(rsp_header));
		if (len != sizeof(rsp_header)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_OPEN_SESSION readResp failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		if (rsp_header.response_id != MC_DRV_RSP_OK) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_OPEN_SESSION failed, respId=%d",
					rsp_header.response_id);
			switch (rsp_header.response_id) {
			case MC_DRV_RSP_TRUSTLET_NOT_FOUND:
				mc_result = MC_DRV_ERR_INVALID_DEVICE_FILE;
				break;
			case MC_DRV_RSP_PAYLOAD_LENGTH_ERROR:
			case MC_DRV_RSP_DEVICE_NOT_OPENED:
			case MC_DRV_RSP_FAILED:
			default:
				mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
				break;
			}
			break;
		}

		/* read payload */
		struct mc_drv_rsp_open_session_payload_t
					rsp_open_session_payload;
		memset(&rsp_open_session_payload, 0,
		       sizeof(rsp_open_session_payload));
		len = connection_read_datablock(
					dev_con,
					&rsp_open_session_payload,
					sizeof(rsp_open_session_payload));
		if (len != sizeof(rsp_open_session_payload)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_OPEN_SESSION readPayload fail %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		/* Register session with handle */
		session->session_id = rsp_open_session_payload.session_id;

		/* Set up second channel for notifications */
		struct connection *session_connection = connection_new();
		if (session_connection == NULL) {
			mc_result = MC_DRV_ERR_NO_FREE_MEMORY;
			break;
		}

		if (!connection_connect(session_connection, MC_DAEMON_PID)) {
			MCDRV_DBG_ERROR(
				mc_kapi,
				"Could not setup netlink connection to PID %u",
				MC_DAEMON_PID);
			connection_cleanup(session_connection);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		/* Write command to use channel for notifications */
		struct mc_drv_cmd_nqconnect_t cmd_nqconnect = {
			{
				MC_DRV_CMD_NQ_CONNECT
			},
			{
				session->device_id,
				session->session_id,
				rsp_open_session_payload.device_session_id,
				rsp_open_session_payload.session_magic
			}
		};
		connection_write_data(session_connection,
				      &cmd_nqconnect,
				      sizeof(cmd_nqconnect));

		/* Read command response, header first */
		len = connection_read_datablock(session_connection,
						&rsp_header,
						sizeof(rsp_header));
		if (len != sizeof(rsp_header)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_NQ_CONNECT readRsp failed %d",
					len);
			connection_cleanup(session_connection);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		if (rsp_header.response_id != MC_DRV_RSP_OK) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_NQ_CONNECT failed, respId=%d",
					rsp_header.response_id);
			connection_cleanup(session_connection);
			mc_result = MC_DRV_ERR_NQ_FAILED;
			break;
		}

		/* there is no payload. */

		/* Session established, new session object must be created */
		if (!mcore_device_create_new_session(device,
						     session->session_id,
						     session_connection)) {
			connection_cleanup(session_connection);
			mc_result = MC_DRV_ERR_NO_FREE_MEMORY;
			break;
		}

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_open_session);

enum mc_result mc_close_session(struct mc_session_handle *session)
{
	enum mc_result mc_result = MC_DRV_OK;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		if (session == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		struct mcore_device_t *device =
					resolve_device_id(session->device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}
		struct connection *dev_con = device->connection;

		struct session *nq_session =
			mcore_device_resolve_session_id(device,
							session->session_id);

		if (nq_session == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session not found");
			mc_result = MC_DRV_ERR_UNKNOWN_SESSION;
			break;
		}

		/* Write close session command */
		struct mc_drv_cmd_close_session_t cmd_close_session = {
			{
				MC_DRV_CMD_CLOSE_SESSION
			},
			{
				session->session_id,
			}
		};
		connection_write_data(dev_con,
				      &cmd_close_session,
				      sizeof(cmd_close_session));

		/* Read command response */
		struct mc_drv_response_header_t rsp_header;
		memset(&rsp_header, 0, sizeof(rsp_header));
		int len = connection_read_datablock(dev_con,
						    &rsp_header,
						    sizeof(rsp_header));
		if (len != sizeof(rsp_header)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_CLOSE_SESSION readRsp failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		if (rsp_header.response_id != MC_DRV_RSP_OK) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_CLOSE_SESSION failed, respId=%d",
					rsp_header.response_id);
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}

		mcore_device_remove_session(device, session->session_id);
		mc_result = MC_DRV_OK;

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_close_session);

enum mc_result mc_notify(struct mc_session_handle *session)
{
	enum mc_result mc_result = MC_DRV_OK;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		if (session == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		struct mcore_device_t *device =
					resolve_device_id(session->device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}
		struct connection *dev_con = device->connection;

		struct session  *nqsession =
		 mcore_device_resolve_session_id(device, session->session_id);
		if (nqsession == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session not found");
			mc_result = MC_DRV_ERR_UNKNOWN_SESSION;
			break;
		}

		struct mc_drv_cmd_notify_t cmd_notify = {
			{
				MC_DRV_CMD_NOTIFY
			},
			{
				session->session_id
			}
		};

		connection_write_data(dev_con,
				      &cmd_notify,
				      sizeof(cmd_notify));

		/* Daemon will not return a response */

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_notify);

enum mc_result mc_wait_notification(struct mc_session_handle *session,
				    int32_t timeout)
{
	enum mc_result mc_result = MC_DRV_OK;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		if (session == NULL) {
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		struct mcore_device_t *device =
					resolve_device_id(session->device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}

		struct session *nq_session =
			mcore_device_resolve_session_id(device,
							session->session_id);
		if (nq_session == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session not found");
			mc_result = MC_DRV_ERR_UNKNOWN_SESSION;
			break;
		}

		struct connection *nqconnection =
					nq_session->notification_connection;
		uint32_t count = 0;

		/* Read notification queue till it's empty */
		for (;;) {
			struct notification notification;
			memset(&notification, 0, sizeof(notification));
			ssize_t num_read =
				connection_read_data(nqconnection,
						     &notification,
						     sizeof(notification),
						     timeout);
			/*
			 * Exit on timeout in first run. Later runs have
			 * timeout set to 0.
			 * -2 means, there is no more data.
			 */
			if (count == 0 && num_read == -2) {
				MCDRV_DBG_ERROR(mc_kapi, "read timeout");
				mc_result = MC_DRV_ERR_TIMEOUT;
				break;
			}
			/*
			 * After first notification the queue will be
			 * drained, Thus we set no timeout for the
			 * following reads
			 */
			timeout = 0;

			if (num_read != sizeof(struct notification)) {
				if (count == 0) {
					/* failure in first read, notify it */
					mc_result = MC_DRV_ERR_NOTIFICATION;
					MCDRV_DBG_ERROR(
					mc_kapi,
					"read notification failed, "
					"%i bytes received", (int)num_read);
					break;
				} else {
					/*
					 * Read of the n-th notification
					 * failed/timeout. We don't tell the
					 * caller, as we got valid notifications
					 * before.
					 */
					mc_result = MC_DRV_OK;
					break;
				}
			}

			count++;
			MCDRV_DBG_VERBOSE(mc_kapi,
					  "count=%d, SessionID=%d, Payload=%d",
					  count,
					  notification.session_id,
					  notification.payload);

			if (notification.payload != 0) {
				/* Session end point died -> store exit code */
				session_set_error_info(nq_session,
						       notification.payload);

				mc_result = MC_DRV_INFO_NOTIFICATION;
				break;
			}
		} /* for(;;) */

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_wait_notification);

enum mc_result mc_malloc_wsm(uint32_t device_id, uint32_t align, uint32_t len,
			     uint8_t **wsm, uint32_t wsm_flags)
{
	enum mc_result mc_result = MC_DRV_ERR_UNKNOWN;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		struct mcore_device_t *device = resolve_device_id(device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}
		if (wsm == NULL) {
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		struct wsm *wsm_stack =
			mcore_device_allocate_contiguous_wsm(device, len);
		if (wsm_stack == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Allocation of WSM failed");
			mc_result = MC_DRV_ERR_NO_FREE_MEMORY;
			break;
		}

		*wsm = (uint8_t *)wsm_stack->virt_addr;
		mc_result = MC_DRV_OK;

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_malloc_wsm);

enum mc_result mc_free_wsm(uint32_t device_id, uint8_t *wsm)
{
	enum mc_result mc_result = MC_DRV_ERR_UNKNOWN;
	struct mcore_device_t *device;


	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		/* Get the device associated wit the given session */
		device = resolve_device_id(device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}

		/* find WSM object */
		struct wsm *wsm_stack =
			mcore_device_find_contiguous_wsm(device, wsm);
		if (wsm_stack == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "unknown address");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		/* Free the given virtual address */
		if (!mcore_device_free_contiguous_wsm(device, wsm_stack)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"Free of virtual address failed");
			mc_result = MC_DRV_ERR_FREE_MEMORY_FAILED;
			break;
		}
		mc_result = MC_DRV_OK;

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_free_wsm);

enum mc_result mc_map(struct mc_session_handle *session_handle, void *buf,
		      uint32_t buf_len, struct mc_bulk_map *map_info)
{
	enum mc_result mc_result = MC_DRV_ERR_UNKNOWN;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		if (session_handle == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "session_handle is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}
		if (map_info == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "map_info is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}
		if (buf == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "buf is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		/* Determine device the session belongs to */
		struct mcore_device_t *device =
				resolve_device_id(session_handle->device_id);

		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}
		struct connection *dev_con = device->connection;

		/* Get session */
		uint32_t session_id = session_handle->session_id;
		struct session *session =
				mcore_device_resolve_session_id(device,
								session_id);
		if (session == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session not found");
			mc_result = MC_DRV_ERR_UNKNOWN_SESSION;
			break;
		}

		/*
		 * Register mapped bulk buffer to Kernel Module and keep mapped
		 * bulk buffer in mind
		 */
		struct bulk_buffer_descriptor *bulk_buf =
				session_add_bulk_buf(session, buf, buf_len);
		if (bulk_buf == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Error mapping bulk buffer");
			mc_result = MC_DRV_ERR_BULK_MAPPING;
			break;
		}

		/* Prepare map command */
		uintptr_t offset = (uintptr_t)(bulk_buf->virt_addr) & 0xFFF;
		struct mc_drv_cmd_map_bulk_mem_t mc_drv_cmd_map_bulk_mem = {
			{
				MC_DRV_CMD_MAP_BULK_BUF
			},
			{
				session->session_id,
				bulk_buf->handle,
				0,
				(uint32_t)offset,
				bulk_buf->len
			}
		};

		/* Transmit map command to MobiCore device */
		connection_write_data(dev_con,
				      &mc_drv_cmd_map_bulk_mem,
				      sizeof(mc_drv_cmd_map_bulk_mem));

		/* Read command response */
		struct mc_drv_response_header_t rsp_header;
		memset(&rsp_header, 0, sizeof(rsp_header));
		int len = connection_read_datablock(dev_con,
							&rsp_header,
							sizeof(rsp_header));
		if (len != sizeof(rsp_header)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_MAP_BULK_BUF readRsp failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		if (rsp_header.response_id != MC_DRV_RSP_OK) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_MAP_BULK_BUF failed, respId=%d",
					rsp_header.response_id);

			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;

			/*
			 * Unregister mapped bulk buffer from Kernel Module and
			 * remove mapped bulk buffer from session maintenance
			 */
			if (!session_remove_bulk_buf(session, buf)) {
				/* Removing of bulk buffer not possible */
				MCDRV_DBG_ERROR(mc_kapi,
						"Unreg of bulk memory failed");
			}
			break;
		}

		struct mc_drv_rsp_map_bulk_mem_payload_t
						rsp_map_bulk_mem_payload;
		memset(&rsp_map_bulk_mem_payload, 0,
		       sizeof(rsp_map_bulk_mem_payload));
		connection_read_datablock(dev_con,
					  &rsp_map_bulk_mem_payload,
					  sizeof(rsp_map_bulk_mem_payload));

		/* Set mapping info for Trustlet */
		map_info->secure_virt_addr =
			rsp_map_bulk_mem_payload.secure_virtual_adr;
		map_info->secure_virt_len = buf_len;
		mc_result = MC_DRV_OK;

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_map);

enum mc_result mc_unmap(struct mc_session_handle *session_handle, void *buf,
			struct mc_bulk_map *map_info)
{
	enum mc_result mc_result = MC_DRV_ERR_UNKNOWN;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		if (session_handle == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "session_handle is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}
		if (map_info == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "map_info is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}
		if (buf == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "buf is null");
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		/* Determine device the session belongs to */
		struct mcore_device_t  *device =
			resolve_device_id(session_handle->device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}
		struct connection *dev_con = device->connection;

		/* Get session */
		uint32_t session_id = session_handle->session_id;
		struct session  *session =
			mcore_device_resolve_session_id(device,
							session_id);
		if (session == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session not found");
			mc_result = MC_DRV_ERR_UNKNOWN_SESSION;
			break;
		}

		uint32_t handle = session_find_bulk_buf(session, buf);
		if (handle == 0) {
			MCDRV_DBG_ERROR(mc_kapi, "Buffer not found");
			mc_result = MC_DRV_ERR_BULK_UNMAPPING;
			break;
		}


		/* Prepare unmap command */
		struct mc_drv_cmd_unmap_bulk_mem_t cmd_unmap_bulk_mem = {
				{
					MC_DRV_CMD_UNMAP_BULK_BUF
				},
				{
					session->session_id,
					handle,
					map_info->secure_virt_addr,
					map_info->secure_virt_len
				}
			};

		connection_write_data(dev_con,
				      &cmd_unmap_bulk_mem,
				      sizeof(cmd_unmap_bulk_mem));

		/* Read command response */
		struct mc_drv_response_header_t rsp_header;
		memset(&rsp_header, 0, sizeof(rsp_header));
		int len = connection_read_datablock(dev_con,
						    &rsp_header,
						    sizeof(rsp_header));
		if (len != sizeof(rsp_header)) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_UNMAP_BULK_BUF readRsp failed %d",
					len);
			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		if (rsp_header.response_id != MC_DRV_RSP_OK) {
			MCDRV_DBG_ERROR(mc_kapi,
					"CMD_UNMAP_BULK_BUF failed, respId=%d",
					rsp_header.response_id);

			mc_result = MC_DRV_ERR_DAEMON_UNREACHABLE;
			break;
		}

		/*struct mc_drv_rsp_unmap_bulk_mem_payload_t
						rsp_unmap_bulk_mem_payload;
		connection_read_datablock(dev_con,
					  &rsp_unmap_bulk_mem_payload,
					  sizeof(rsp_unmap_bulk_mem_payload));*/

		/*
		 * Unregister mapped bulk buffer from Kernel Module and
		 * remove mapped bulk buffer from session maintenance
		 */
		if (!session_remove_bulk_buf(session, buf)) {
			/* Removing of bulk buffer not possible */
			MCDRV_DBG_ERROR(mc_kapi,
					"Unregistering of bulk memory failed");
			mc_result = MC_DRV_ERR_BULK_UNMAPPING;
			break;
		}

		mc_result = MC_DRV_OK;

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_unmap);

enum mc_result mc_get_session_error_code(struct mc_session_handle *session,
					 int32_t *last_error)
{
	enum mc_result mc_result = MC_DRV_OK;

	MCDRV_DBG_VERBOSE(mc_kapi, "===%s()===", __func__);

	do {
		if (session == NULL || last_error == NULL) {
			mc_result = MC_DRV_ERR_INVALID_PARAMETER;
			break;
		}

		/* Get device */
		struct mcore_device_t *device =
				resolve_device_id(session->device_id);
		if (device == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Device not found");
			mc_result = MC_DRV_ERR_UNKNOWN_DEVICE;
			break;
		}

		/* Get session */
		uint32_t session_id = session->session_id;
		struct session *nqsession =
				mcore_device_resolve_session_id(device,
								session_id);
		if (nqsession == NULL) {
			MCDRV_DBG_ERROR(mc_kapi, "Session not found");
			mc_result = MC_DRV_ERR_UNKNOWN_SESSION;
			break;
		}

		*last_error = session_get_last_err(nqsession);

	} while (false);

	return mc_result;
}
EXPORT_SYMBOL(mc_get_session_error_code);

