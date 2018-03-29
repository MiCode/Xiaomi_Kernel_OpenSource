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
/*
 * MobiCore client library device management.
 *
 * Device and Trustlet Session management Functions.
 */
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/device.h>
#include "mc_kernel_api.h"
#include "public/mobicore_driver_api.h"

#include "device.h"
#include "common.h"

static struct wsm *wsm_create(void *virt_addr, uint32_t len, uint32_t handle)
{
	struct wsm *wsm;

	wsm = kzalloc(sizeof(*wsm), GFP_KERNEL);
	if (wsm == NULL) {
		MCDRV_DBG_ERROR(mc_kapi, "Allocation failure");
		return NULL;
	}
	wsm->virt_addr = virt_addr;
	wsm->len = len;
	wsm->handle = handle;
	return wsm;
}

struct mcore_device_t *mcore_device_create(uint32_t device_id,
					   struct connection *connection)
{
	struct mcore_device_t *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		MCDRV_DBG_ERROR(mc_kapi, "Allocation failure");
		return NULL;
	}
	dev->device_id = device_id;
	dev->connection = connection;

	INIT_LIST_HEAD(&dev->session_vector);
	INIT_LIST_HEAD(&dev->wsm_mmu_vector);
	mutex_init(&(dev->session_vector_lock));
	mutex_init(&(dev->wsm_mmu_vector_lock));

	return dev;
}

void mcore_device_cleanup(struct mcore_device_t *dev)
{
	struct session *session = NULL;
	struct wsm *wsm;
	struct list_head *pos, *q;

	/*
	 * Delete all session objects. Usually this should not be needed
	 * as close_device() requires that all sessions have been closed before.
	 */
	do {
		session = NULL;
		mutex_lock(&(dev->session_vector_lock));
		if (!list_empty(&(dev->session_vector))) {
			session = list_first_entry(&(dev->session_vector),
							struct session, list);
			list_del(&(session->list));
		}
		mutex_unlock(&(dev->session_vector_lock));
		if (!session)
			break;
		session_cleanup(session);
	} while (true);

	/* Free all allocated WSM descriptors */
	mutex_lock(&(dev->wsm_mmu_vector_lock));
	list_for_each_safe(pos, q, &(dev->wsm_mmu_vector)) {
		wsm = list_entry(pos, struct wsm, list);
		list_del(pos);
		kfree(wsm);
	}
	mutex_unlock(&(dev->wsm_mmu_vector_lock));
	connection_cleanup(dev->connection);

	mcore_device_close(dev);
	kfree(dev);
}

bool mcore_device_open(struct mcore_device_t *dev, const char *device_name)
{
	dev->instance = mobicore_open();
	return (dev->instance != NULL);
}

void mcore_device_close(struct mcore_device_t *dev)
{
	mobicore_release(dev->instance);
}

bool mcore_device_has_sessions(struct mcore_device_t *dev)
{
	int ret = 0;
	mutex_lock(&(dev->session_vector_lock));
	ret = !list_empty(&dev->session_vector);
	mutex_unlock(&(dev->session_vector_lock));
	return ret;
}

bool mcore_device_create_new_session(struct mcore_device_t *dev,
				     uint32_t session_id,
				     struct connection *connection)
{
	/* Check if session_id already exists */
	if (mcore_device_resolve_session_id(dev, session_id)) {
		MCDRV_DBG_ERROR(mc_kapi,
				" session %u already exists", session_id);
		return false;
	}
	struct session *session =
			session_create(session_id, dev->instance, connection);
	if (session == NULL)
		return false;
	mutex_lock(&(dev->session_vector_lock));
	list_add_tail(&(session->list), &(dev->session_vector));
	mutex_unlock(&(dev->session_vector_lock));
	return true;
}

bool mcore_device_remove_session(struct mcore_device_t *dev,
				 uint32_t session_id)
{
	bool found = false;
	struct session *session = NULL;
	struct list_head *pos;

	mutex_lock(&(dev->session_vector_lock));
	list_for_each(pos, &dev->session_vector) {
		session = list_entry(pos, struct session, list);
		if (session->session_id == session_id) {
			list_del(pos);
			found = true;
			break;
		}
	}
	mutex_unlock(&(dev->session_vector_lock));
	if (found)
		session_cleanup(session);
	return found;
}

struct session *mcore_device_resolve_session_id(struct mcore_device_t *dev,
						uint32_t session_id)
{
	struct session *ret = NULL;
	struct session *session;

	/* Get session for session_id */
	mutex_lock(&(dev->session_vector_lock));
	list_for_each_entry(session, &dev->session_vector, list) {
		if (session->session_id == session_id) {
			ret = session;
			break;
		}
	}
	mutex_unlock(&(dev->session_vector_lock));
	return ret;
}

struct wsm *mcore_device_allocate_contiguous_wsm(struct mcore_device_t *dev,
						 uint32_t len)
{
	struct wsm *wsm = NULL;
	do {
		if (len == 0)
			break;

		/* Allocate shared memory */
		void *virt_addr;
		uint32_t handle;
		int ret = mobicore_allocate_wsm(dev->instance, len, &handle,
						&virt_addr);
		if (ret != 0)
			break;

		/* Register (vaddr) with device */
		wsm = wsm_create(virt_addr, len, handle);
		if (wsm == NULL) {
			mobicore_free_wsm(dev->instance, handle);
			break;
		}

		mutex_lock(&(dev->wsm_mmu_vector_lock));
		list_add_tail(&(wsm->list), &(dev->wsm_mmu_vector));
		mutex_unlock(&(dev->wsm_mmu_vector_lock));

	} while (0);

	return wsm;
}

bool mcore_device_free_contiguous_wsm(struct mcore_device_t *dev,
				      struct wsm *wsm)
{
	bool ret = false;
	struct wsm *tmp;
	struct list_head *pos;

	mutex_lock(&(dev->wsm_mmu_vector_lock));
	list_for_each(pos, &dev->wsm_mmu_vector) {
		tmp = list_entry(pos, struct wsm, list);
		if (tmp == wsm) {
			ret = true;
			break;
		}
	}
	mutex_unlock(&(dev->wsm_mmu_vector_lock));
	if (ret) {
		MCDRV_DBG_VERBOSE(mc_kapi,
				  "freeWsm virt_addr=0x%p, handle=%d",
				  wsm->virt_addr, wsm->handle);

		/* ignore return code */
		mobicore_free_wsm(dev->instance, wsm->handle);

		list_del(pos);
		kfree(wsm);
	}
	return ret;
}

struct wsm *mcore_device_find_contiguous_wsm(struct mcore_device_t *dev,
					     void *virt_addr)
{
	struct wsm *wsm, *candidate = NULL;

	mutex_lock(&(dev->wsm_mmu_vector_lock));
	list_for_each_entry(wsm, &dev->wsm_mmu_vector, list) {
		if (virt_addr == wsm->virt_addr) {
			candidate = wsm;
			break;
		}
	}
	mutex_unlock(&(dev->wsm_mmu_vector_lock));

	return candidate;
}
