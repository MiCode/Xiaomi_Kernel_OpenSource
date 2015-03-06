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
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/err.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "main.h"
#include "debug.h"
#include "mmu.h"
#include "mcp.h"
#include "api.h"

/*
 * Contiguous buffer allocated to TLCs.
 * These buffers are used as world shared memory (wsm) to share with
 * secure world.
 */
struct tbase_cbuf {
	/* Client this cbuf belongs to */
	struct tbase_client	*client;
	/* List element for client's list of cbuf's */
	struct list_head	list;
	/* Number of references kept to this buffer */
	struct kref		kref;
	/* virtual Kernel start address */
	void			*addr;
	/* virtual Userspace start address */
	void			*uaddr;
	/* physical start address */
	phys_addr_t		phys;
	/* 2^order = number of pages allocated */
	unsigned int		order;
	/* Length of memory mapped to user */
	uint32_t		len;
};

/*
 * Map a kernel contiguous buffer to user space
 */
static int map_cbuf(struct vm_area_struct *vmarea, void *addr, uint32_t len,
		    void **uaddr)
{
	int err = 0;

	if (WARN(!uaddr, "No uaddr pointer available"))
		return -EINVAL;

	if (WARN(!vmarea, "No vma available"))
		return -EINVAL;

	if (WARN(!addr, "No addr available"))
		return -EINVAL;

	if (len != (uint32_t)(vmarea->vm_end - vmarea->vm_start)) {
		MCDRV_ERROR("cbuf incompatible with vma");
		return -EINVAL;
	}

	*uaddr = NULL;
	vmarea->vm_flags |= VM_IO;

	/* CPI todo: use io_remap_page_range() to be consistent with VM_IO ? */
	err = remap_pfn_range(vmarea, vmarea->vm_start,
			      page_to_pfn(virt_to_page(addr)),
			      vmarea->vm_end - vmarea->vm_start,
			      vmarea->vm_page_prot);
	if (err)
		MCDRV_ERROR("User mapping failed");
	else
		*uaddr = (void *)vmarea->vm_start;

	return err;
}

/*
 * Allocate and initialize a client object
 */
struct tbase_client *client_create(bool is_from_kernel)
{
	struct tbase_client *client;

	/* allocate client structure */
	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		MCDRV_ERROR("Allocation failure");
		return NULL;
	}

	/* init members */
	client->kernel = is_from_kernel;
	kref_init(&client->kref);
	INIT_LIST_HEAD(&client->cbufs);
	mutex_init(&client->cbufs_lock);
	INIT_LIST_HEAD(&client->sessions);
	mutex_init(&client->sessions_lock);
	INIT_LIST_HEAD(&client->list);

	return client;
}

void client_close_sessions(struct tbase_client *client)
{
	struct tbase_session *session, *s;

	/* Remove all sessions from client and ask them to close */
	mutex_lock(&client->sessions_lock);
	list_for_each_entry_safe(session, s, &client->sessions, list) {
		session_remove(session);
		session_close(session);
	}

	mutex_unlock(&client->sessions_lock);
}

/*
 * Free client object + all objects it contains.
 * Can be called only by last user referencing the client,
 * therefore mutex lock seems overkill
 */
static void client_release(struct kref *kref)
{
	struct tbase_client *client;

	client = container_of(kref, struct tbase_client, kref);
	kfree(client);
}

void client_put(struct tbase_client *client)
{
	kref_put(&client->kref, client_release);
}

/*
 * Returns true if client is a kernel object.
 */
bool client_is_kernel(struct tbase_client *client)
{
	return client->kernel;
}

/*
 * Set client "closing" state, only if it contains no session.
 * Once in "closing" state, system "close" can be called.
 * Return: true if this state could be set.
 */
bool client_set_closing(struct tbase_client *client)
{
	bool clear = false;

	/* Check for sessions */
	mutex_lock(&client->sessions_lock);
	clear = list_empty(&client->sessions);
	client->closing = clear;
	mutex_unlock(&client->sessions_lock);
	MCDRV_DBG("return %d", clear);
	return clear;
}

/*
 * Opens a TA and add corresponding session object to given client
 * return: t-base driver error code
 */
int client_add_session(struct tbase_client *client,
		       const struct tbase_object *obj,
		       void *tci, size_t len, uint32_t *p_sid, bool is_gp_uuid)
{
	int err = 0;
	struct tbase_session *session = NULL;
	struct tbase_mmu *tci_mmu = NULL;
	struct tbase_mmu *blob_mmu = NULL;
	struct tbase_wsm *wsm = NULL;
	struct tbase_cbuf *cbuf = NULL;
	struct task_struct *task = NULL;
	void *va;
	size_t offset;
	uint32_t session_id;
	int32_t nq_error;

	/* Create blob L2 table (blob is allocated by driver, so task=NULL) */
	blob_mmu = tbase_mmu_create(NULL, obj->data, obj->length);
	if (IS_ERR(blob_mmu)) {
		err = PTR_ERR(blob_mmu);
		blob_mmu = NULL;
		goto end;
	}

	/* Create wsm object for tci */
	if (tci && len) {
		/* Check if buffer is contained in a cbuf */
		/* TODO CPI: factorize this (duplicate of session_add_wsm()) */
		cbuf = tbase_cbuf_get_by_addr(client, tci);
		if (cbuf) {
			if (client_is_kernel(client))
				offset = tci - tbase_cbuf_addr(cbuf);
			else
				offset = tci - tbase_cbuf_uaddr(cbuf);

			if ((offset + len) > tbase_cbuf_len(cbuf)) {
				err = -EINVAL;
				MCDRV_ERROR("crosses cbuf boundary");
				goto end;
			}
			/* Provide kernel virtual address */
			va = tbase_cbuf_addr(cbuf) + offset;
			task = NULL;
		}
		/* Not a cbuf, differentiate user/kernel clients */
		else {
			va = tci;
			if (!client_is_kernel(client))
				task = current;
		}

		/* Create L2 table */
		tci_mmu = tbase_mmu_create(task, va, len);
		if (IS_ERR(tci_mmu)) {
			err = PTR_ERR(tci_mmu);
			tci_mmu = NULL;
			goto end;
		}

		/* Create wsm */
		wsm = session_alloc_wsm(tci, len, 0, tci_mmu, cbuf);
		if (!wsm) {
			err = -ENOMEM;
			goto end;
		}

	} else if (tci || len) {
		MCDRV_ERROR("Tci pointer and length are incoherent");
		err = -EINVAL;
		goto end;
	}

	/*
	 * Create session object with temp sid=0 BEFORE session is started,
	 * otherwise if a GP TA is started and NWd session object allocation
	 * fails, we cannot handle the potentially delayed GP closing.
	 * Adding session to list must be done AFTER it is started (once we have
	 * sid), therefore it cannot be done within session_create().
	 */
	session = session_create(client, 0);
	if (!session) {
		MCDRV_ERROR("Allocating session object failed.");
		err = -ENOMEM;
		goto end;
	}
	if (wsm)
		session_link_wsm(session, wsm);

	/* Send MCP open command */
	err = mcp_open_session(obj, blob_mmu, is_gp_uuid, tci, len, tci_mmu,
			       &session_id);

	if (err)
		goto end;

	/* Set sid returned by SWd */
	session->id = session_id;

	/* Add session to client */
	mutex_lock(&client->sessions_lock);
	if (client->closing) {
		/* Client has been frozen, abort */
		err = -ENODEV;
		mutex_unlock(&client->sessions_lock);
		goto end;
	}
	list_add(&session->list, &client->sessions);
	mutex_unlock(&client->sessions_lock);

	/* Read potential notif arrived before session was added to list */
	if (mc_read_notif(session->id, &nq_error))
		session_notify_nwd(session, nq_error);

	/* Everything fine, fill in return parameter */
	err = 0;
	*p_sid = session_id;

end:
	/* Release the ref used for early notifs */
	if (session)
		client_unref_session(session);

	/* Blob table no more needed */
	if (blob_mmu)
		tbase_mmu_delete(blob_mmu);

	/* Clean in case of error. No ref taken since session is not in list */
	if (err) {
		*p_sid = -1;
		if (session) {
			if (session->id) {
				/* Exists in SWd, close properly */
				session_remove(session);
				session_close(session);
			} else {
				/* Only clean the NWd object */
				session_put(session);
			}
		} else if (wsm) {
			session_free_wsm(wsm);
		} else {
			if (tci_mmu)
				tbase_mmu_delete(tci_mmu);

			if (cbuf)
				tbase_cbuf_put(cbuf);
		}
	}

	return err;
}

/*
 * Remove a session object from client and close corresponding TA
 * Return: true if session was found and closed
 */
int client_remove_session(struct tbase_client *client, uint32_t session_id)
{
	struct tbase_session *session = NULL, *candidate;

	/* Move session from main list to closing list */
	mutex_lock(&client->sessions_lock);
	list_for_each_entry(candidate, &client->sessions, list) {
		if (candidate->id == session_id) {
			session = candidate;
			session_remove(session);
			break;
		}
	}

	mutex_unlock(&client->sessions_lock);

	/* Close session */
	return session_close(session);
}

/*
 * Find a session object and increment its reference counter.
 * Object cannot be freed until its counter reaches 0.
 * return: pointer to the object, NULL if not found.
 */
struct tbase_session *client_ref_session(struct tbase_client *client,
					 uint32_t session_id)
{
	struct tbase_session *session = NULL, *candidate;

	mutex_lock(&client->sessions_lock);
	list_for_each_entry(candidate, &client->sessions, list) {
		if (candidate->id == session_id) {
			session = candidate;
			session_get(session);
			break;
		}
	}

	mutex_unlock(&client->sessions_lock);
	return session;
}

/*
 * Decrement a session object's reference counter, and frees the object if it
 * was the last reference.
 * No lookup since session may have been removed from list
 */
void client_unref_session(struct tbase_session *session)
{
	session_put(session);
}

/*
 * This callback is called on remap
 */
static void cbuf_vm_open(struct vm_area_struct *vmarea)
{
	struct tbase_cbuf *cbuf = vmarea->vm_private_data;

	tbase_cbuf_get(cbuf);
}

/*
 * This callback is called on unmap
 */
static void cbuf_vm_close(struct vm_area_struct *vmarea)
{
	struct tbase_cbuf *cbuf = vmarea->vm_private_data;

	tbase_cbuf_put(cbuf);
}

static struct vm_operations_struct cbuf_vm_ops = {
	.open = cbuf_vm_open,
	.close = cbuf_vm_close,
};

/*
 * Create a cbuf object and add it to client
 */
int tbase_cbuf_alloc(struct tbase_client *client, uint32_t len, void **p_addr,
		     struct vm_area_struct *vmarea)
{
	int err = 0;
	struct tbase_cbuf *cbuf = NULL;
	void *addr = NULL;
	void *uaddr = NULL;
	unsigned int order;

	if (WARN(!client, "No client available"))
		return -EINVAL;

	if (WARN(!len, "No len available"))
		return -EINVAL;

	order = get_order(len);
	if (order > MAX_ORDER) {
		MCDRV_DBG_WARN("Buffer size too large");
		return -ENOMEM;
	}

	/* Allocate buffer descriptor structure */
	cbuf = kzalloc(sizeof(*cbuf), GFP_KERNEL);
	if (!cbuf) {
		MCDRV_DBG_WARN("kzalloc failed");
		return -ENOMEM;
	}

	/* Allocate buffer */
	addr = (void *)__get_free_pages(GFP_USER | __GFP_ZERO, order);
	if (!addr) {
		MCDRV_DBG_WARN("get_free_pages failed");
		kfree(cbuf);
		return -ENOMEM;
	}

	/* Map to user space if applicable */
	if (!client_is_kernel(client)) {
		err = map_cbuf(vmarea, addr, len, &uaddr);
		if (err) {
			free_pages((unsigned long)addr, order);
			kfree(cbuf);
			return err;
		}
	}

	/* Init descriptor members */
	cbuf->client = client;
	cbuf->phys = virt_to_phys(addr);
	cbuf->addr = addr;
	cbuf->uaddr = uaddr;
	cbuf->len = len;
	cbuf->order = order;
	kref_init(&cbuf->kref);
	INIT_LIST_HEAD(&cbuf->list);

	/* Keep cbuf in VMA private data for refcounting (user-space clients) */
	if (vmarea) {
		vmarea->vm_private_data = cbuf;
		vmarea->vm_ops = &cbuf_vm_ops;
	}

	/* Fill return parameter for k-api */
	if (p_addr)
		*p_addr = cbuf->addr;

	/* Get a token on the client */
	client_get(client);

	/* Add buffer to list */
	mutex_lock(&client->cbufs_lock);
	list_add(&cbuf->list, &client->cbufs);
	mutex_unlock(&client->cbufs_lock);

	MCDRV_DBG("@ 0x%p, size=%ld", addr, (1 << order) * PAGE_SIZE);
	return err;
}

/*
 * Remove a cbuf object from client, and mark it for freeing.
 * Freeing will happen once all current references are released.
 */
int tbase_cbuf_free(struct tbase_client *client, void *addr)
{
	struct tbase_cbuf *cbuf = tbase_cbuf_get_by_addr(client, addr);

	if (!cbuf)
		return -EINVAL;

	/* Two references to put: the caller's and the one we just took */
	tbase_cbuf_put(cbuf);
	tbase_cbuf_put(cbuf);
	return 0;
}

/*
 * Find a contiguous buffer (cbuf) in the cbuf list of given client that
 * contains given address and take a reference on it.
 * Return pointer to the object, or NULL if not found.
 */
struct tbase_cbuf *tbase_cbuf_get_by_addr(struct tbase_client *client,
					  void *addr)
{
	struct tbase_cbuf *cbuf = NULL, *candidate;
	bool is_kernel = client->kernel;

	mutex_lock(&client->cbufs_lock);
	list_for_each_entry(candidate, &client->cbufs, list) {
		/* Compare Vs kernel va OR user va depending on client type */
		void *start = is_kernel ? candidate->addr : candidate->uaddr;
		void *end = start + candidate->len;

		/* Check that (user) cbuf has not been unmapped */
		if (!start)
			break;

		if ((addr >= start) && (addr < end)) {
			cbuf = candidate;
			break;
		}
	}

	if (cbuf)
		tbase_cbuf_get(cbuf);

	mutex_unlock(&client->cbufs_lock);
	return cbuf;
}

void tbase_cbuf_get(struct tbase_cbuf *cbuf)
{
	kref_get(&cbuf->kref);
}

static void cbuf_release(struct kref *kref)
{
	struct tbase_cbuf *cbuf = container_of(kref, struct tbase_cbuf, kref);
	struct tbase_client *client = cbuf->client;

	MCDRV_DBG("Clean up 0x%p", cbuf->addr);
	/* Unlist from client */
	mutex_lock(&client->cbufs_lock);
	cbuf->uaddr = 0;
	list_del_init(&cbuf->list);
	mutex_unlock(&client->cbufs_lock);
	/* Release client token */
	client_put(client);
	/* Free */
	free_pages((unsigned long)cbuf->addr, cbuf->order);
	kfree(cbuf);
}

void tbase_cbuf_put(struct tbase_cbuf *cbuf)
{
	kref_put(&cbuf->kref, cbuf_release);
}

void *tbase_cbuf_addr(struct tbase_cbuf *cbuf)
{
	return cbuf->addr;
}

void *tbase_cbuf_uaddr(struct tbase_cbuf *cbuf)
{
	return cbuf->uaddr;
}

uint32_t tbase_cbuf_len(struct tbase_cbuf *cbuf)
{
	return cbuf->len;
}
