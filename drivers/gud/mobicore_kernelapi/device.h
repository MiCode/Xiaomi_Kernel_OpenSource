/** @addtogroup MCD_IMPL_LIB
 * @{
 * @file
 *
 * Client library device management.
 *
 * Device and Trustlet Session management Functions.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef DEVICE_H_
#define DEVICE_H_

#include <linux/list.h>

#include "connection.h"
#include "session.h"
#include "wsm.h"


struct mcore_device_t {
	struct list_head session_vector; /**< MobiCore Trustlet session
				associated with the device */
	struct list_head	 wsm_l2_vector; /**< WSM L2 Table  */

	uint32_t		device_id; /**< Device identifier */
	struct connection	*connection; /**< The device connection */
	struct mc_instance  *instance; /**< MobiCore Driver instance */

	struct list_head list; /**< The list param for using the kernel lists*/
};

struct mcore_device_t *mcore_device_create(
	uint32_t	  device_id,
	struct connection  *connection
);

void mcore_device_cleanup(
	struct mcore_device_t *dev
);

/**
  * Open the device.
  * @param deviceName Name of the kernel modules device file.
  * @return true if the device has been opened successfully
  */
bool mcore_device_open(
	struct mcore_device_t   *dev,
	const char *deviceName
);

/**
  * Closes the device.
  */
void mcore_device_close(
	struct mcore_device_t *dev
);

/**
  * Check if the device has open sessions.
  * @return true if the device has one or more open sessions.
  */
bool mcore_device_has_sessions(
	struct mcore_device_t *dev
);

/**
  * Add a session to the device.
  * @param session_id session ID
  * @param connection session connection
  */
bool mcore_device_create_new_session(
	struct mcore_device_t	  *dev,
	uint32_t	session_id,
	struct connection  *connection
);

/**
  * Remove the specified session from the device.
  * The session object will be destroyed and all resources associated with it
  * will be freed.
  *
  * @param session_id Session of the session to remove.
  * @return true if a session has been found and removed.
  */
bool mcore_device_remove_session(
	struct mcore_device_t *dev,
	uint32_t session_id
);

/**
  * Get as session object for a given session ID.
  * @param session_id Identified of a previously opened session.
  * @return Session object if available or NULL if no session has been found.
  */
struct session *mcore_device_resolve_session_id(
	struct mcore_device_t *dev,
	uint32_t session_id
);

/**
  * Allocate a block of contiguous WSM.
  * @param len The virtual address to be registered.
  * @return The virtual address of the allocated memory or NULL if no memory
  * is available.
  */
struct wsm *mcore_device_allocate_contiguous_wsm(
	struct mcore_device_t *dev,
	uint32_t len
);

/**
  * Unregister a vaddr from a device.
  * @param vaddr The virtual address to be registered.
  * @param paddr The physical address to be registered.
  */
bool mcore_device_free_contiguous_wsm(
	struct mcore_device_t  *dev,
	struct wsm *wsm
);

/**
  * Get a WSM object for a given virtual address.
  * @param vaddr The virtual address which has been allocate with mc_malloc_wsm()
  * in advance.
  * @return the WSM object or NULL if no address has been found.
  */
struct wsm *mcore_device_find_contiguous_wsm(
	struct mcore_device_t *dev,
	void   *virt_addr
);

#endif /* DEVICE_H_ */

/** @} */
