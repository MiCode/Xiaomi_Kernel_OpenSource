/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
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
#ifndef MOBICORE_DRIVER_API_H
#define MOBICORE_DRIVER_API_H

#include "mc_user.h"

#define __MC_CLIENT_LIB_API

/*
 * Return values of MobiCore driver functions.
 */
enum mc_result {
	/* Function call succeeded. */
	MC_DRV_OK				= 0,
	/* No notification available. */
	MC_DRV_NO_NOTIFICATION			= 1,
	/* Error during notification on communication level. */
	MC_DRV_ERR_NOTIFICATION			= 2,
	/* Function not implemented. */
	MC_DRV_ERR_NOT_IMPLEMENTED		= 3,
	/* No more resources available. */
	MC_DRV_ERR_OUT_OF_RESOURCES		= 4,
	/* Driver initialization failed. */
	MC_DRV_ERR_INIT				= 5,
	/* Unknown error. */
	MC_DRV_ERR_UNKNOWN			= 6,
	/* The specified device is unknown. */
	MC_DRV_ERR_UNKNOWN_DEVICE		= 7,
	/* The specified session is unknown.*/
	MC_DRV_ERR_UNKNOWN_SESSION		= 8,
	/* The specified operation is not allowed. */
	MC_DRV_ERR_INVALID_OPERATION		= 9,
	/* The response header from the MC is invalid. */
	MC_DRV_ERR_INVALID_RESPONSE		= 10,
	/* Function call timed out. */
	MC_DRV_ERR_TIMEOUT			= 11,
	/* Can not allocate additional memory. */
	MC_DRV_ERR_NO_FREE_MEMORY		= 12,
	/* Free memory failed. */
	MC_DRV_ERR_FREE_MEMORY_FAILED		= 13,
	/* Still some open sessions pending. */
	MC_DRV_ERR_SESSION_PENDING		= 14,
	/* MC daemon not reachable */
	MC_DRV_ERR_DAEMON_UNREACHABLE		= 15,
	/* The device file of the kernel module could not be opened. */
	MC_DRV_ERR_INVALID_DEVICE_FILE		= 16,
	/* Invalid parameter. */
	MC_DRV_ERR_INVALID_PARAMETER		= 17,
	/* Unspecified error from Kernel Module*/
	MC_DRV_ERR_KERNEL_MODULE		= 18,
	/* Error during mapping of additional bulk memory to session. */
	MC_DRV_ERR_BULK_MAPPING			= 19,
	/* Error during unmapping of additional bulk memory to session. */
	MC_DRV_ERR_BULK_UNMAPPING		= 20,
	/* Notification received, exit code available. */
	MC_DRV_INFO_NOTIFICATION		= 21,
	/* Set up of NWd connection failed. */
	MC_DRV_ERR_NQ_FAILED			= 22,
	/* Wrong daemon version. */
	MC_DRV_ERR_DAEMON_VERSION		= 23,
	/* System Trustlet public key is wrong. */
	MC_DRV_ERR_WRONG_PUBLIC_KEY		= 25,
	/* No device associated with connection. */
	MC_DRV_ERR_DAEMON_DEVICE_NOT_OPEN	= 33,
	/* TA blob attestation is incorrect. */
	MC_DRV_ERR_TA_ATTESTATION_ERROR		= 34,
	/* Interrupted system call. */
	MC_DRV_ERR_INTERRUPTED_BY_SIGNAL	= 35,
	/* Service is blocked and opensession is thus not allowed. */
	MC_DRV_ERR_SERVICE_BLOCKED		= 36,
	/* Service is locked and opensession is thus not allowed. */
	MC_DRV_ERR_SERVICE_LOCKED		= 37,
	/* Service was killed by the TEE (due to an administrative command). */
	MC_DRV_ERR_SERVICE_KILLED		= 38,
	/* All permitted instances to the service are used */
	MC_DRV_ERR_NO_FREE_INSTANCES		= 39,
	/* TA blob header is incorrect. */
	MC_DRV_ERR_TA_HEADER_ERROR		= 40,
};

/*
 * Structure of Session Handle, includes the Session ID and the Device ID the
 * Session belongs to.
 * The session handle will be used for session-based MobiCore communication.
 * It will be passed to calls which address a communication end point in the
 * MobiCore environment.
 */
struct mc_session_handle {
	u32	session_id;		/* MobiCore session ID */
	u32	device_id;		/* Device ID the session belongs to */
};

/*
 * Information structure about additional mapped Bulk buffer between the
 * Trustlet Connector (NWd) and the Trustlet (SWd). This structure is
 * initialized from a Trustlet Connector by calling mc_map().
 * In order to use the memory within a Trustlet the Trustlet Connector has to
 * inform the Trustlet with the content of this structure via the TCI.
 */
struct mc_bulk_map {
	/*
	 * The virtual address of the Bulk buffer regarding the address space
	 * of the Trustlet, already includes a possible offset!
	 */
	u32	secure_virt_addr;
	u32	secure_virt_len;	/* Length of the mapped Bulk buffer */
};

/* The default device ID */
#define MC_DEVICE_ID_DEFAULT	0
/* Wait infinite for a response of the MC. */
#define MC_INFINITE_TIMEOUT	((s32)(-1))
/* Do not wait for a response of the MC. */
#define MC_NO_TIMEOUT		0
/* TCI/DCI must not exceed 1MiB */
#define MC_MAX_TCI_LEN		0x100000

/**
 * mc_open_device() - Open a new connection to a MobiCore device.
 * @device_id:		Identifier for the MobiCore device to be used.
 *			MC_DEVICE_ID_DEFAULT refers to the default device.
 *
 * Initializes all device specific resources required to communicate with a
 * MobiCore instance located on the specified device in the system. If the
 * device does not exist the function will return MC_DRV_ERR_UNKNOWN_DEVICE.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_ERR_INVALID_OPERATION:	device already opened
 *	MC_DRV_ERR_DAEMON_UNREACHABLE:	problems with daemon
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device_id unknown
 *	MC_DRV_ERR_INVALID_DEVICE_FILE:	kernel module under /dev/mobicore
 *					cannot be opened
 */
__MC_CLIENT_LIB_API enum mc_result mc_open_device(
	u32				device_id);

/**
 * mc_close_device() - Close the connection to a MobiCore device.
 * @device_id:		Identifier for the MobiCore device.
 *
 * When closing a device, active sessions have to be closed beforehand.
 * Resources associated with the device will be released.
 * The device may be opened again after it has been closed.
 *
 * MC_DEVICE_ID_DEFAULT refers to the default device.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id is invalid
 *	MC_DRV_ERR_SESSION_PENDING:	a session is still open
 *	MC_DRV_ERR_DAEMON_UNREACHABLE:	problems with daemon occur
 */
__MC_CLIENT_LIB_API enum mc_result mc_close_device(
	u32				device_id);

/**
 * mc_open_session() - Open a new session to a Trustlet.
 * @session:		On success, the session data will be returned
 * @uuid:		UUID of the Trustlet to be opened
 * @tci:		TCI buffer for communicating with the Trustlet
 * @tci_len:		Length of the TCI buffer. Maximum allowed value
 *			is MC_MAX_TCI_LEN
 *
 * The Trustlet with the given UUID has to be available in the flash filesystem.
 *
 * Write MCP open message to buffer and notify MobiCore about the availability
 * of a new command.
 *
 * Waits till the MobiCore responses with the new session ID (stored in the MCP
 * buffer).
 *
 * Note that session.device_id has to be the device id of an opened device.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	session parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id is invalid
 *	MC_DRV_ERR_DAEMON_UNREACHABLE:	problems with daemon socket occur
 *	MC_DRV_ERR_NQ_FAILED:		daemon returns an error
 */
__MC_CLIENT_LIB_API enum mc_result mc_open_session(
	struct mc_session_handle	*session,
	const struct mc_uuid_t		*uuid,
	u8				*tci,
	u32				tci_len);

/**
 * mc_open_trustlet() - Open a new session to the provided Trustlet.
 * @session:		On success, the session data will be returned
 * @trustlet		Memory buffer containing the Trusted Application binary
 * @trustlet_len	Trusted Application length
 * @tci:		TCI buffer for communicating with the Trustlet
 * @tci_len:		Length of the TCI buffer. Maximum allowed value
 *			is MC_MAX_TCI_LEN
 *
 * Write MCP open message to buffer and notify MobiCore about the availability
 * of a new command.
 *
 * Waits till the MobiCore responses with the new session ID (stored in the MCP
 * buffer).
 *
 * Note that session.device_id has to be the device id of an opened device.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	session parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id is invalid
 *	MC_DRV_ERR_DAEMON_UNREACHABLE:	problems with daemon socket occur
 *	MC_DRV_ERR_NQ_FAILED:		daemon returns an error
 */
__MC_CLIENT_LIB_API enum mc_result mc_open_trustlet(
	struct mc_session_handle	*session,
	u8				*trustlet,
	u32				trustlet_len,
	u8				*tci,
	u32				len);

/**
 * mc_close_session() - Close a Trustlet session.
 * @session:		Session to be closed.
 *
 * Closes the specified MobiCore session. The call will block until the
 * session has been closed.
 *
 * Device device_id has to be opened in advance.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	session parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_SESSION:	session id is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id of session is invalid
 *	MC_DRV_ERR_DAEMON_UNREACHABLE:	problems with daemon occur
 *	MC_DRV_ERR_INVALID_DEVICE_FILE:	daemon cannot open Trustlet file
 */
__MC_CLIENT_LIB_API enum mc_result mc_close_session(
	struct mc_session_handle	*session);

/**
 * mc_notify() - Notify a session.
 * @session:		The session to be notified.
 *
 * Notifies the session end point about available message data.
 * If the session parameter is correct, notify will always succeed.
 * Corresponding errors can only be received by mc_wait_notification().
 *
 * A session has to be opened in advance.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	session parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_SESSION:	session id is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id of session is invalid
 */
__MC_CLIENT_LIB_API enum mc_result mc_notify(
	struct mc_session_handle	*session);

/**
 * mc_wait_notification() - Wait for a notification.
 * @session:		The session the notification should correspond to.
 * @timeout:		Time in milliseconds to wait
 *			(MC_NO_TIMEOUT : direct return, > 0 : milliseconds,
 *			 MC_INFINITE_TIMEOUT : wait infinitely)
 *
 * Wait for a notification issued by the MobiCore for a specific session.
 * The timeout parameter specifies the number of milliseconds the call will wait
 * for a notification.
 *
 * If the caller passes 0 as timeout value the call will immediately return.
 * If timeout value is below 0 the call will block until a notification for the
 * session has been received.
 *
 * If timeout is below 0, call will block.
 *
 * Caller has to trust the other side to send a notification to wake him up
 * again.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_ERR_TIMEOUT:		no notification arrived in time
 *	MC_DRV_INFO_NOTIFICATION:	a problem with the session was
 *					encountered. Get more details with
 *					mc_get_session_error_code()
 *	MC_DRV_ERR_NOTIFICATION:	a problem with the socket occurred
 *	MC_DRV_INVALID_PARAMETER:	a parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_SESSION:	session id is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id of session is invalid
 */
__MC_CLIENT_LIB_API enum mc_result mc_wait_notification(
	struct mc_session_handle	*session,
	s32				timeout);

/**
 * mc_malloc_wsm() - Allocate a block of world shared memory (WSM).
 * @device_id:		The ID of an opened device to retrieve the WSM from.
 * @align:		The alignment (number of pages) of the memory block
 *			(e.g. 0x00000001 for 4kb).
 * @len:		Length of the block in bytes.
 * @wsm:		Virtual address of the world shared memory block.
 * @wsm_flags:		Platform specific flags describing the memory to
 *			be allocated.
 *
 * The MC driver allocates a contiguous block of memory which can be used as
 * WSM.
 * This implicates that the allocated memory is aligned according to the
 * alignment parameter.
 *
 * Always returns a buffer of size WSM_SIZE aligned to 4K.
 *
 * Align and wsm_flags are currently ignored
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	a parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id is invalid
 *	MC_DRV_ERR_NO_FREE_MEMORY:	no more contiguous memory is
 *					available in this size or for this
 *					process
 */
__MC_CLIENT_LIB_API enum mc_result mc_malloc_wsm(
	u32				device_id,
	u32				align,
	u32				len,
	u8				**wsm,
	u32				wsm_flags);

/**
 * mc_free_wsm() - Free a block of world shared memory (WSM).
 * @device_id:		The ID to which the given address belongs
 * @wsm:		Address of WSM block to be freed
 *
 * The MC driver will free a block of world shared memory (WSM) previously
 * allocated with mc_malloc_wsm(). The caller has to assure that the address
 * handed over to the driver is a valid WSM address.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	a parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	when device id is invalid
 *	MC_DRV_ERR_FREE_MEMORY_FAILED:	on failure
 */
__MC_CLIENT_LIB_API enum mc_result mc_free_wsm(
	u32				device_id,
	u8				*wsm);

/**
 *mc_map() -	Map additional bulk buffer between a Trustlet Connector (TLC)
 *		and the Trustlet (TL) for a session
 * @session:		Session handle with information of the device_id and
 *			the session_id. The given buffer is mapped to the
 *			session specified in the sessionHandle
 * @buf:		Virtual address of a memory portion (relative to TLC)
 *			to be shared with the Trustlet, already includes a
 *			possible offset!
 * @len:		length of buffer block in bytes.
 * @map_info:		Information structure about the mapped Bulk buffer
 *			between the TLC (NWd) and the TL (SWd).
 *
 * Memory allocated in user space of the TLC can be mapped as additional
 * communication channel (besides TCI) to the Trustlet. Limitation of the
 * Trustlet memory structure apply: only 6 chunks can be mapped with a maximum
 * chunk size of 1 MiB each.
 *
 * It is up to the application layer (TLC) to inform the Trustlet
 * about the additional mapped bulk memory.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	a parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_SESSION:	session id is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id of session is invalid
 *	MC_DRV_ERR_DAEMON_UNREACHABLE:	problems with daemon occur
 *	MC_DRV_ERR_BULK_MAPPING:	buf is already uses as bulk buffer or
 *					when registering the buffer failed
 */
__MC_CLIENT_LIB_API enum mc_result mc_map(
	struct mc_session_handle	*session,
	void				*buf,
	u32				len,
	struct mc_bulk_map		*map_info);

/**
 * mc_unmap() -	Remove additional mapped bulk buffer between Trustlet Connector
 *		(TLC) and the Trustlet (TL) for a session
 * @session:		Session handle with information of the device_id and
 *			the session_id. The given buffer is unmapped from the
 *			session specified in the sessionHandle.
 * @buf:		Virtual address of a memory portion (relative to TLC)
 *			shared with the TL, already includes a possible offset!
 * @map_info:		Information structure about the mapped Bulk buffer
 *			between the TLC (NWd) and the TL (SWd)
 *
 * The bulk buffer will immediately be unmapped from the session context.
 *
 * The application layer (TLC) must inform the TL about unmapping of the
 * additional bulk memory before calling mc_unmap!
 *
 * The clientlib currently ignores the len field in map_info.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	a parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_SESSION:	session id is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id of session is invalid
 *	MC_DRV_ERR_DAEMON_UNREACHABLE:	problems with daemon occur
 *	MC_DRV_ERR_BULK_UNMAPPING:	buf was not registered earlier
 *					or when unregistering failed
 */
__MC_CLIENT_LIB_API enum mc_result mc_unmap(
	struct mc_session_handle	*session,
	void				*buf,
	struct mc_bulk_map		*map_info);

/*
 * mc_get_session_error_code() - Get additional error information of the last
 *				 error that occurred on a session.
 * @session:		Session handle with information of the device_id and
 *			the session_id
 * @exit_code:		>0 Trustlet has terminated itself with this value,
 *			<0 Trustlet is dead because of an error within the
 *			MobiCore (e.g. Kernel exception). See also MCI
 *			definition.
 *
 * After the request the stored error code will be deleted.
 *
 * Return codes:
 *	MC_DRV_OK:			operation completed successfully
 *	MC_DRV_INVALID_PARAMETER:	a parameter is invalid
 *	MC_DRV_ERR_UNKNOWN_SESSION:	session id is invalid
 *	MC_DRV_ERR_UNKNOWN_DEVICE:	device id of session is invalid
 */
__MC_CLIENT_LIB_API enum mc_result mc_get_session_error_code(
	struct mc_session_handle	*session,
	s32				*exit_code);

#endif /* MOBICORE_DRIVER_API_H */
