/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
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
#include <linux/err.h>
#include <linux/sched.h>	/* struct task_struct */
#include <linux/version.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/mm.h>	/* get_task_mm */
#include <linux/sched/task.h>	/* put_task_struct */
#endif
#include <net/sock.h>		/* sockfd_lookup */
#include <linux/file.h>		/* fput */

#include "public/mc_user.h"
#include "public/mc_admin.h"

#include "main.h"
#include "admin.h"	/* tee_object* */
#include "mmu.h"
#include "session.h"
#include "client.h"

/* Client/context */
struct tee_client {
	/* PID of task that opened the device, 0 if kernel */
	pid_t			pid;
	/* Command for task*/
	char			comm[TASK_COMM_LEN];
	/* Number of references kept to this object */
	struct kref		kref;
	/* List of contiguous buffers allocated by mcMallocWsm for the client */
	struct list_head	cbufs;
	struct mutex		cbufs_lock;	/* lock for the cbufs list */
	/* List of TA sessions opened by this client */
	struct list_head	sessions;
	struct list_head	closing_sessions;
	struct mutex		sessions_lock;	/* sessions list + closing */
	/* Client lock for quick WSMs and operations changes */
	struct mutex		quick_lock;
	/* Client lock for CWSMs release functions */
	struct mutex		cwsm_release_lock;
	/* List of WSMs for a client */
	struct list_head	cwsms;
	/* List of GP operation for a client */
	struct list_head	operations;
	/* The list entry to attach to "ctx.clients" list */
	struct list_head	list;
	/* task_struct for the client application, if going through a proxy */
	struct task_struct	*task;
};

/* Context */
static struct client_ctx {
	/* Clients list */
	struct mutex		clients_lock;
	struct list_head	clients;
	/* Clients waiting for their last cbuf to be released */
	struct mutex		closing_clients_lock;
	struct list_head	closing_clients;
} client_ctx;

/* Buffer shared with SWd at client level */
struct cwsm {
	/* Client this cbuf belongs to */
	struct tee_client	*client;
	/* Buffer info */
	struct gp_shared_memory	memref;
	/* MMU L2 table */
	struct tee_mmu		*mmu;
	/* Buffer SWd addr */
	u32			sva;
	/* Number of references kept to this object */
	struct kref		kref;
	/* The list entry for the client to list its WSMs */
	struct list_head	list;
};

/*
 * Contiguous buffer allocated to TLCs.
 * These buffers are used as world shared memory (wsm) to share with
 * secure world.
 */
struct cbuf {
	/* Client this cbuf belongs to */
	struct tee_client	*client;
	/* List element for client's list of cbuf's */
	struct list_head	list;
	/* Number of references kept to this buffer */
	struct kref		kref;
	/* virtual Kernel start address */
	uintptr_t		addr;
	/* virtual Userspace start address */
	uintptr_t		uaddr;
	/* physical start address */
	phys_addr_t		phys;
	/* 2^order = number of pages allocated */
	unsigned int		order;
	/* Length of memory mapped to user */
	u32			len;
	/* Has been freed via the API */
	bool			api_freed;
};

static inline void cbuf_get(struct cbuf *cbuf)
{
	kref_get(&cbuf->kref);
}

/* Must only be called by tee_cbuf_put */
static void cbuf_release(struct kref *kref)
{
	struct cbuf *cbuf = container_of(kref, struct cbuf, kref);
	struct tee_client *client = cbuf->client;

	/* Unlist from client */
	list_del_init(&cbuf->list);
	/* Release client token */
	client_put(client);
	/* Free */
	free_pages(cbuf->addr, cbuf->order);
	mc_dev_devel("freed cbuf %p: client %p addr %lx uaddr %lx len %u",
		     cbuf, client, cbuf->addr, cbuf->uaddr, cbuf->len);
	kfree(cbuf);
	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_cbufs);
}

static inline void cbuf_put(struct cbuf *cbuf)
{
	struct tee_client *client = cbuf->client;

	mutex_lock(&client->cbufs_lock);
	kref_put(&cbuf->kref, cbuf_release);
	mutex_unlock(&client->cbufs_lock);
}

/*
 * Map a kernel contiguous buffer to user space
 */
static int cbuf_map(struct vm_area_struct *vmarea, uintptr_t addr, u32 len,
		    uintptr_t *uaddr)
{
	int ret;

	if (!uaddr)
		return -EINVAL;

	if (!vmarea)
		return -EINVAL;

	if (!addr)
		return -EINVAL;

	if (len != (u32)(vmarea->vm_end - vmarea->vm_start)) {
		mc_dev_notice("cbuf incompatible with vma");
		return -EINVAL;
	}

	vmarea->vm_flags |= VM_IO;
	ret = remap_pfn_range(vmarea, vmarea->vm_start,
			      page_to_pfn(virt_to_page(addr)),
			      vmarea->vm_end - vmarea->vm_start,
			      vmarea->vm_page_prot);
	if (ret) {
		*uaddr = 0;
		mc_dev_notice("User mapping failed");
		return ret;
	}

	*uaddr = vmarea->vm_start;
	return 0;
}

/*
 * Returns true if client is a kernel object.
 */
static inline bool client_is_kernel(struct tee_client *client)
{
	return !client->pid;
}

/*
 * The proxy gives us the fd of its server-side socket, so we can find out the
 * task then the mm of its client. mmput() must be called to free the resource.
 */
static struct mm_struct *get_mm_from_client_fd(int client_fd)
{
	struct mm_struct *mm = NULL;
	struct socket *sock;
	int err;

	if (client_fd < 0)
		return get_task_mm(current);

	sock = sockfd_lookup(client_fd, &err);
	if (!sock)
		return NULL;

	if (sock->sk && sock->sk->sk_peer_pid) {
		struct task_struct *task;

		rcu_read_lock();
		task = pid_task(sock->sk->sk_peer_pid, PIDTYPE_PID);
		if (task)
			mm = get_task_mm(task);

		rcu_read_unlock();
	}

	sockfd_put(sock);
	return mm;
}

static struct cwsm *cwsm_create(struct tee_client *client,
				const struct gp_shared_memory *memref,
				struct gp_return *gp_ret, int client_fd)
{
	struct cwsm *cwsm;
	struct mcp_buffer_map map;
	struct mc_ioctl_buffer buf;
	int ret;

	cwsm = kzalloc(sizeof(*cwsm), GFP_KERNEL);
	if (!cwsm)
		return ERR_PTR(iwp_set_ret(-ENOMEM, gp_ret));

	buf.va = (uintptr_t)memref->buffer;
	buf.len = memref->size;
	buf.flags = memref->flags;
	if (client_is_kernel(client)) {
		cwsm->mmu = tee_mmu_create(NULL, &buf);
	} else {
		struct mm_struct *mm = get_mm_from_client_fd(client_fd);

		if (!mm) {
			mc_dev_notice("can't get mm from client fd %d",
				client_fd);
			ret = -EPERM;
			goto err_cwsm;
		}

		/* Build MMU table for buffer */
		cwsm->mmu = tee_mmu_create(mm, &buf);
		mmput(mm);
	}

	if (IS_ERR(cwsm->mmu)) {
		ret = iwp_set_ret(PTR_ERR(cwsm->mmu), gp_ret);
		goto err_cwsm;
	}

	/* Initialise maps */
	memset(&map, 0, sizeof(map));
	tee_mmu_buffer(cwsm->mmu, &map);
	/* FIXME Flags must be stored in MMU (needs recent trunk change) */
	map.flags = memref->flags;
	ret = iwp_register_shared_mem(&map, gp_ret);
	if (ret)
		goto err_mmu;

	cwsm->client = client;
	memcpy(&cwsm->memref, memref, sizeof(cwsm->memref));
	cwsm->sva = map.secure_va;
	kref_init(&cwsm->kref);
	INIT_LIST_HEAD(&cwsm->list);
	/* Get a token on the client */
	client_get(client);
	/* Add buffer to list */
	mutex_lock(&client->quick_lock);
	list_add_tail(&cwsm->list, &client->cwsms);
	mutex_unlock(&client->quick_lock);
	mc_dev_devel("created cwsm %p: client %p", cwsm, client);
	/* Increment debug counter */
	atomic_inc(&g_ctx.c_cwsms);
	return cwsm;

err_mmu:
	tee_mmu_delete(cwsm->mmu);
err_cwsm:
	kfree(cwsm);
	return ERR_PTR(ret);
}

static inline void cwsm_get(struct cwsm *cwsm)
{
	kref_get(&cwsm->kref);
}

/* Must only be called by cwsm_put */
static void cwsm_release(struct kref *kref)
{
	struct cwsm *cwsm = container_of(kref, struct cwsm, kref);
	struct tee_client *client = cwsm->client;
	struct mcp_buffer_map map;

	/* Unlist from client */
	list_del_init(&cwsm->list);
	/* Unmap buffer from SWd (errors ignored) */
	tee_mmu_buffer(cwsm->mmu, &map);
	map.secure_va = cwsm->sva;
	iwp_release_shared_mem(&map);
	/* Release MMU */
	tee_mmu_delete(cwsm->mmu);
	/* Release client token */
	client_put(client);
	/* Free */
	mc_dev_devel("freed cwsm %p: client %p", cwsm, client);
	kfree(cwsm);
	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_cwsms);
}

static inline void cwsm_put(struct cwsm *cwsm)
{
	struct tee_client *client = cwsm->client;

	mutex_lock(&client->quick_lock);
	kref_put(&cwsm->kref, cwsm_release);
	mutex_unlock(&client->quick_lock);
}

static inline struct cwsm *cwsm_find(struct tee_client *client,
				     const struct gp_shared_memory *memref)
{
	struct cwsm *cwsm = NULL, *candidate;

	mc_dev_devel("find shared mem for buf %llx size %llu flags %x",
		     memref->buffer, memref->size, memref->flags);
	mutex_lock(&client->quick_lock);
	list_for_each_entry(candidate, &client->cwsms, list) {
		mc_dev_devel("candidate buf %llx size %llu flags %x",
			     candidate->memref.buffer, candidate->memref.size,
			     candidate->memref.flags);
		if (candidate->memref.buffer == memref->buffer &&
		    candidate->memref.size == memref->size &&
		    candidate->memref.flags == memref->flags) {
			cwsm = candidate;
			cwsm_get(cwsm);
			mc_dev_devel("match");
			break;
		}
	}
	mutex_unlock(&client->quick_lock);
	return cwsm;
}

static inline struct cwsm *cwsm_find_by_sva(struct tee_client *client, u32 sva)
{
	struct cwsm *cwsm = NULL, *candidate;

	mutex_lock(&client->quick_lock);
	list_for_each_entry(candidate, &client->cwsms, list)
		if (candidate->sva == sva) {
			cwsm = candidate;
			cwsm_get(cwsm);
			break;
		}
	mutex_unlock(&client->quick_lock);
	return cwsm;
}

/*
 * Returns the secure virtual address from a registered mem
 */
u32 client_get_cwsm_sva(struct tee_client *client,
			const struct gp_shared_memory *memref)
{
	struct cwsm *cwsm = cwsm_find(client, memref);

	if (!cwsm)
		return 0;

	return cwsm->sva;
}

void client_get(struct tee_client *client)
{
	kref_get(&client->kref);
}

void client_put_cwsm_sva(struct tee_client *client, u32 sva)
{
	struct cwsm *cwsm;

	mutex_lock(&client->cwsm_release_lock);
	cwsm = cwsm_find_by_sva(client, sva);
	if (!cwsm)
		goto end;

	/* Release reference taken by cwsm_find_by_sva */
	cwsm_put(cwsm);
	cwsm_put(cwsm);
end:
	mutex_unlock(&client->cwsm_release_lock);
}

/*
 * Allocate and initialize a client object
 */
struct tee_client *client_create(bool is_from_kernel)
{
	struct tee_client *client;

	/* Allocate client structure */
	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_clients);
	/* initialize members */
	client->pid = is_from_kernel ? 0 : current->pid;
	memcpy(client->comm, current->comm, sizeof(client->comm));
	kref_init(&client->kref);
	INIT_LIST_HEAD(&client->cbufs);
	mutex_init(&client->cbufs_lock);
	INIT_LIST_HEAD(&client->sessions);
	INIT_LIST_HEAD(&client->closing_sessions);
	mutex_init(&client->sessions_lock);
	INIT_LIST_HEAD(&client->list);
	mutex_init(&client->quick_lock);
	mutex_init(&client->cwsm_release_lock);
	INIT_LIST_HEAD(&client->cwsms);
	INIT_LIST_HEAD(&client->operations);
	/* Add client to list of clients */
	mutex_lock(&client_ctx.clients_lock);
	list_add_tail(&client->list, &client_ctx.clients);
	mutex_unlock(&client_ctx.clients_lock);
	mc_dev_devel("created client %p", client);
	return client;
}

/* Must only be called by client_put */
static void client_release(struct kref *kref)
{
	struct tee_client *client;

	client = container_of(kref, struct tee_client, kref);
	/* Client is closed, remove from closing list */
	list_del(&client->list);
	mc_dev_devel("freed client %p", client);
	if (client->task)
		put_task_struct(client->task);

	kfree(client);
	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_clients);
}

int client_put(struct tee_client *client)
{
	int ret;

	mutex_lock(&client_ctx.closing_clients_lock);
	ret = kref_put(&client->kref, client_release);
	mutex_unlock(&client_ctx.closing_clients_lock);
	return ret;
}

/*
 * Set client "closing" state, only if it contains no session.
 * Once in "closing" state, system "close" can be called.
 * Return: 0 if this state could be set.
 */
bool client_has_sessions(struct tee_client *client)
{
	bool ret;

	/* Check for sessions */
	mutex_lock(&client->sessions_lock);
	ret = !list_empty(&client->sessions);
	mutex_unlock(&client->sessions_lock);
	mc_dev_devel("client %p, exit with %d", client, ret);
	return ret;
}

static inline void client_put_session(struct tee_client *client,
				      struct tee_session *session)
{
	/* Remove session from client's closing list */
	mutex_lock(&client->sessions_lock);
	list_del(&session->list);
	mutex_unlock(&client->sessions_lock);
	/* Release the ref we took on creation */
	session_put(session);
}

/*
 * At this point, nobody has access to the client anymore, so no new sessions
 * are being created.
 */
static void client_close_sessions(struct tee_client *client)
{
	struct tee_session *session;

	mutex_lock(&client->sessions_lock);
	while (!list_empty(&client->sessions)) {
		session = list_first_entry(&client->sessions,
					   struct tee_session, list);

		/* Move session to closing sessions list */
		list_move(&session->list, &client->closing_sessions);
		/* Call session_close without lock */
		mutex_unlock(&client->sessions_lock);
		if (!session_close(session))
			client_put_session(client, session);
		mutex_lock(&client->sessions_lock);
	}

	mutex_unlock(&client->sessions_lock);
}

/*
 * At this point, nobody has access to the client anymore, so no new contiguous
 * buffers are being created.
 */
static void client_close_kernel_cbufs(struct tee_client *client)
{
	/* Put buffers allocated and not freed via the kernel API */
	if (!client_is_kernel(client))
		return;

	/* Look for cbufs that the client has not freed and put them */
	while (true) {
		struct cbuf *cbuf = NULL, *candidate;

		mutex_lock(&client->cbufs_lock);
		list_for_each_entry(candidate, &client->cbufs, list) {
			if (!candidate->api_freed) {
				candidate->api_freed = true;
				cbuf = candidate;
				break;
			}
		}
		mutex_unlock(&client->cbufs_lock);

		if (!cbuf)
			break;

		cbuf_put(cbuf);
	}
}

/* Client is closing: make sure all CSMs are gone */
static void client_release_cwsms(struct tee_client *client)
{
	/* Look for cbufs that the client has not freed and put them */
	while (!list_empty(&client->cwsms)) {
		struct cwsm *cwsm;

		cwsm = list_first_entry(&client->cwsms, struct cwsm, list);
		cwsm_put(cwsm);
	}
}

/* Client is closing: make sure all cancelled operations are gone */
static void client_release_gp_operations(struct tee_client *client)
{
	struct client_gp_operation *op, *nop;

	mutex_lock(&client->quick_lock);
	list_for_each_entry_safe(op, nop, &client->operations, list) {
		/* Only cancelled operations are kzalloc'd */
		mc_dev_devel("flush cancelled operation %p for started %u",
			     op, op->started);
		if (op->cancelled)
			kfree(op);
	}
	mutex_unlock(&client->quick_lock);
}

/*
 * Release a client and the session+cbuf objects it contains.
 * @param client_t client
 * @return driver error code
 */
void client_close(struct tee_client *client)
{
	/* Move client from active clients to closing clients for debug */
	mutex_lock(&client_ctx.clients_lock);
	mutex_lock(&client_ctx.closing_clients_lock);
	list_move(&client->list, &client_ctx.closing_clients);
	mutex_unlock(&client_ctx.closing_clients_lock);
	mutex_unlock(&client_ctx.clients_lock);
	client_close_kernel_cbufs(client);
	/* Close all remaining sessions */
	client_close_sessions(client);
	/* Release all cwsms, no need to lock as sessions are closed */
	client_release_cwsms(client);
	client_release_gp_operations(client);
	client_put(client);
	mc_dev_devel("client %p closed", client);
}

/*
 * The TEE is going to die, so get rid of whatever is shared with it
 */
void clients_kill_sessions(void)
{
	struct tee_client *client;

	mutex_lock(&client_ctx.clients_lock);
	list_for_each_entry(client, &client_ctx.clients, list) {
		/*
		 * session_kill() will put the session which should get freed
		 * and free its wsms/mmus and put any cbuf concerned
		 */
		mutex_lock(&client->sessions_lock);
		while (!list_empty(&client->sessions)) {
			struct tee_session *session;

			session = list_first_entry(&client->sessions,
						   struct tee_session, list);
			list_del(&session->list);
			session_kill(session);
		}
		mutex_unlock(&client->sessions_lock);
	}
	mutex_unlock(&client_ctx.clients_lock);
}

/*
 * Open TA for given client. TA binary is provided by the daemon.
 * @param
 * @return driver error code
 */
int client_open_session(struct tee_client *client, u32 *session_id,
			const struct mc_uuid_t *uuid, uintptr_t tci,
			size_t tci_len, bool is_gp_uuid,
			struct mc_identity *identity, int client_fd)
{
	int err = 0;
	u32 sid = 0;
	struct tee_object *obj;

	/* Get secure object */
	obj = tee_object_get(uuid, is_gp_uuid);
	if (IS_ERR(obj)) {
		/* Try to select secure object inside the SWd if not found */
		if ((PTR_ERR(obj) == -ENOENT) && g_ctx.f_ta_auth)
			obj = tee_object_select(uuid);

		if (IS_ERR(obj)) {
			err = PTR_ERR(obj);
			goto end;
		}
	}

	/* Open session */
	err = client_add_session(client, obj, tci, tci_len, &sid, is_gp_uuid,
				 identity, client_fd);
	/* Fill in return parameter */
	if (!err)
		*session_id = sid;

	/* Delete secure object */
	tee_object_free(obj);

end:

	mc_dev_devel("session %x, exit with %d", sid, err);
	return err;
}

/*
 * Open TA for given client. TA binary is provided by the client.
 * @param
 * @return driver error code
 */
int client_open_trustlet(struct tee_client *client, u32 *session_id, u32 spid,
			 uintptr_t trustlet, size_t trustlet_len,
			 uintptr_t tci, size_t tci_len, int client_fd)
{
	struct tee_object *obj;
	struct mc_identity identity = {
		.login_type = LOGIN_PUBLIC,
	};
	u32 sid = 0;
	int err = 0;

	if (client_is_kernel(client))
		/* Create secure object from kernel-space trustlet binary */
		obj = tee_object_copy(trustlet, trustlet_len);
	else
		/* Create secure object from user-space trustlet binary */
		obj = tee_object_read(spid, trustlet, trustlet_len);
	if (IS_ERR(obj)) {
		err = PTR_ERR(obj);
		goto end;
	}

	/* Open session */
	err = client_add_session(client, obj, tci, tci_len, &sid, false,
				 &identity, client_fd);
	/* Fill in return parameter */
	if (!err)
		*session_id = sid;

	/* Delete secure object */
	tee_object_free(obj);

end:
	mc_dev_devel("session %x, exit with %d", sid, err);
	return err;
}

/*
 * Opens a TA and add corresponding session object to given client
 * return: driver error code
 */
int client_add_session(struct tee_client *client, const struct tee_object *obj,
		       uintptr_t tci, size_t len, u32 *session_id, bool is_gp,
		       struct mc_identity *identity, int client_fd)
{
	struct tee_session *session = NULL;
	struct tee_mmu *obj_mmu = NULL;
	struct mc_ioctl_buffer buf;
	int ret = 0;

	/*
	 * Create session object with temp sid=0 BEFORE session is started,
	 * otherwise if a GP TA is started and NWd session object allocation
	 * fails, we cannot handle the potentially delayed GP closing.
	 * Adding session to list must be done AFTER it is started (once we have
	 * sid), therefore it cannot be done within session_create().
	 */
	session = session_create(client, is_gp, identity, client_fd);
	if (IS_ERR(session))
		return PTR_ERR(session);

	/* Create blob L2 table (blob is allocated by driver, so task=NULL) */
	buf.va = (uintptr_t)obj->data;
	buf.len = obj->length;
	buf.flags = MC_IO_MAP_INPUT;
	obj_mmu = tee_mmu_create(NULL, &buf);
	if (IS_ERR(obj_mmu)) {
		ret = PTR_ERR(obj_mmu);
		goto err;
	}

	/* Open session */
	ret = session_open(session, obj, obj_mmu, tci, len, client_fd);
	/* Blob table no more needed in any case */
	tee_mmu_delete(obj_mmu);
	if (ret)
		goto err;

	mutex_lock(&client->sessions_lock);
	/* Add session to client */
	list_add_tail(&session->list, &client->sessions);
	/* Set sid returned by SWd */
	*session_id = session->mcp_session.sid;
	mutex_unlock(&client->sessions_lock);

err:
	/* Close or free session on error */
	if (ret == -ENODEV) {
		/* The session must enter the closing process... */
		list_add_tail(&session->list, &client->closing_sessions);
		if (!session_close(session))
			client_put_session(client, session);
	} else if (ret) {
		session_put(session);
	}

	return ret;
}

/*
 * Opens a TA and add corresponding session object to given client
 * return: driver error code
 */
static int client_add_gp_session(struct tee_client *client,
				 const struct tee_object *obj,
				 u32 *session_id,
				 struct gp_operation *operation,
				 struct mc_identity *identity,
				 int client_fd, struct gp_return *gp_ret)
{
	struct tee_session *session = NULL;
	struct tee_mmu *obj_mmu = NULL;
	struct mc_ioctl_buffer buf;
	int ret = 0;

	/*
	 * Create session object with temp sid=0 BEFORE session is started,
	 * otherwise if a GP TA is started and NWd session object allocation
	 * fails, we cannot handle the potentially delayed GP closing.
	 * Adding session to list must be done AFTER it is started (once we have
	 * sid), therefore it cannot be done within session_create().
	 */
	session = session_create(client, true, identity, client_fd);
	if (IS_ERR(session))
		return iwp_set_ret(PTR_ERR(session), gp_ret);

	/* Create blob L2 table (blob is allocated by driver, so task=NULL) */
	buf.va = (uintptr_t)obj->data;
	buf.len = obj->length;
	buf.flags = MC_IO_MAP_INPUT;
	obj_mmu = tee_mmu_create(NULL, &buf);
	if (IS_ERR(obj_mmu)) {
		ret = PTR_ERR(obj_mmu);
		goto end;
	}

	/* Open session */
	ret = session_gp_open_session(session, obj, obj_mmu, operation, gp_ret,
				      client_fd);
	/* Blob table no more needed in any case */
	tee_mmu_delete(obj_mmu);
	if (ret)
		goto end;

	mutex_lock(&client->sessions_lock);
	/* Add session to client */
	list_add_tail(&session->list, &client->sessions);
	mutex_unlock(&client->sessions_lock);
	/* Set sid returned by SWd */
	*session_id = session->iwp_session.sid;

end:
	if (ret)
		session_put(session);

	return ret;
}

/*
 * Remove a session object from client and close corresponding TA
 * Return: true if session was found and closed
 */
int client_remove_session(struct tee_client *client, u32 session_id)
{
	struct tee_session *session = NULL, *candidate;
	int ret;

	/* Move session from main list to closing list */
	mutex_lock(&client->sessions_lock);
	list_for_each_entry(candidate, &client->sessions, list) {
		if (candidate->mcp_session.sid == session_id) {
			session = candidate;
			list_move(&session->list, &client->closing_sessions);
			break;
		}
	}

	mutex_unlock(&client->sessions_lock);
	if (!session)
		return -ENXIO;

	/* Close session */
	ret = session_close(session);
	if (!ret)
		client_put_session(client, session);

	return ret;
}

/*
 * Find a session object and increment its reference counter.
 * Object cannot be freed until its counter reaches 0.
 * return: pointer to the object, NULL if not found.
 */
static struct tee_session *client_get_session(struct tee_client *client,
					      u32 session_id)
{
	struct tee_session *session = NULL, *candidate;

	mutex_lock(&client->sessions_lock);
	list_for_each_entry(candidate, &client->sessions, list) {
		if (candidate->mcp_session.sid == session_id) {
			session = candidate;
			session_get(session);
			break;
		}
	}

	mutex_unlock(&client->sessions_lock);
	if (!session)
		mc_dev_notice("session %x not found", session_id);

	return session;
}

/*
 * Send a notification to TA
 * @return driver error code
 */
int client_notify_session(struct tee_client *client, u32 session_id)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	/* Send command to SWd */
	ret = session_notify_swd(session);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit with %d", session_id, ret);
	return ret;
}

/*
 * Wait for a notification from TA
 * @return driver error code
 */
int client_waitnotif_session(struct tee_client *client, u32 session_id,
			     s32 timeout, bool silent_expiry)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	ret = session_waitnotif(session, timeout, silent_expiry);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit with %d", session_id, ret);
	return ret;
}

/*
 * Read session exit/termination code
 */
int client_get_session_exitcode(struct tee_client *client, u32 session_id,
				s32 *exit_code)
{
	struct tee_session *session;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	/* Retrieve error */
	*exit_code = session_exitcode(session);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit code %d", session_id, *exit_code);
	return 0;
}

/* Share a buffer with given TA in SWd */
int client_map_session_wsms(struct tee_client *client, u32 session_id,
			    struct mc_ioctl_buffer *bufs, int client_fd)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	/* Add buffer to the session */
	ret = session_map(session, bufs, client_fd);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit with %d", session_id, ret);
	return ret;
}

/* Stop sharing a buffer with SWd */
int client_unmap_session_wsms(struct tee_client *client, u32 session_id,
			      const struct mc_ioctl_buffer *bufs)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	/* Remove buffer from session */
	ret = session_unmap(session, bufs);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit with %d", session_id, ret);
	return ret;
}

int client_gp_initialize_context(struct tee_client *client,
				 struct gp_return *gp_ret)
{
	return iwp_set_ret(0, gp_ret);
}

int client_gp_register_shared_mem(struct tee_client *client,
				  const struct gp_shared_memory *memref,
				  struct gp_return *gp_ret, int client_fd)
{
	struct cwsm *cwsm;

	/* cwsm_find automatically takes a reference */
	cwsm = cwsm_find(client, memref);
	if (!cwsm) {
		cwsm = cwsm_create(client, memref, gp_ret, client_fd);
		if (IS_ERR(cwsm))
			return iwp_set_ret(PTR_ERR(cwsm), gp_ret);
	}

	return iwp_set_ret(0, gp_ret);
}

int client_gp_release_shared_mem(struct tee_client *client,
				 const struct gp_shared_memory *memref)
{
	struct cwsm *cwsm;
	int ret = 0;

	mutex_lock(&client->cwsm_release_lock);
	cwsm = cwsm_find(client, memref);
	if (!cwsm) {
		ret = -ENOENT;
		goto end;
	}

	/* Release reference taken by cwsm_find */
	cwsm_put(cwsm);
	cwsm_put(cwsm);
end:
	mutex_unlock(&client->cwsm_release_lock);
	return ret;
}

/*
 * Open TA for given client. TA binary is provided by the daemon.
 * @param
 * @return driver error code
 */
int client_gp_open_session(struct tee_client *client, u32 *session_id,
			   const struct mc_uuid_t *uuid,
			   struct gp_operation *operation,
			   struct mc_identity *identity,
			   struct gp_return *gp_ret, int client_fd)
{
	struct tee_object *obj;
	int ret = 0;

	/* Get secure object */
	obj = tee_object_get(uuid, true);
	if (IS_ERR(obj)) {
		/* Try to select secure object inside the SWd if not found */
		if ((PTR_ERR(obj) == -ENOENT) && g_ctx.f_ta_auth)
			obj = tee_object_select(uuid);

		if (IS_ERR(obj)) {
			ret = PTR_ERR(obj);
			goto end;
		}
	}

	/* Open session */
	ret = client_add_gp_session(client, obj, session_id, operation,
				    identity, client_fd, gp_ret);

	/* Delete secure object */
	tee_object_free(obj);

end:
	mc_dev_devel("gp session %x, exit with %d", *session_id, ret);
	return ret;
}

int client_gp_close_session(struct tee_client *client, u32 session_id)
{
	struct tee_session *session = NULL, *candidate;
	int ret = 0;

	/* Move session from main list to closing list */
	mutex_lock(&client->sessions_lock);
	list_for_each_entry(candidate, &client->sessions, list) {
		if (candidate->iwp_session.sid == session_id) {
			session = candidate;
			list_move(&session->list, &client->closing_sessions);
			break;
		}
	}

	mutex_unlock(&client->sessions_lock);
	if (!session)
		return -ENXIO;

	ret = session_close(session);
	if (!ret)
		client_put_session(client, session);

	return ret;
}

/*
 * Send a command to the TA
 * @param
 * @return driver error code
 */
int client_gp_invoke_command(struct tee_client *client, u32 session_id,
			     u32 command_id,
			     struct gp_operation *operation,
			     struct gp_return *gp_ret, int client_fd)
{
	struct tee_session *session;
	int ret = 0;

	session = client_get_session(client, session_id);
	if (!session)
		return iwp_set_ret(-ENXIO, gp_ret);

	ret = session_gp_invoke_command(session, command_id, operation, gp_ret,
					client_fd);

	/* Put session */
	session_put(session);
	return ret;
}

void client_gp_request_cancellation(struct tee_client *client, u32 started)
{
	struct client_gp_operation *op;
	u32 slot;
	bool found = false;

	/* Look for operation */
	mutex_lock(&client->quick_lock);
	list_for_each_entry(op, &client->operations, list)
		if (op->started == started) {
			slot = op->slot;
			found = true;
			mc_dev_devel("found to operation cancel for started %u",
				     started);
			break;
		}

	/* Operation not found: assume it is coming */
	if (!found) {
		op = kzalloc(sizeof(*op), GFP_KERNEL);
		if (op) {
			op->started = started;
			op->cancelled = true;
			list_add_tail(&op->list, &client->operations);
			mc_dev_devel("add cancelled operation %p for started %u"
				, op, op->started);
		}
	}
	mutex_unlock(&client->quick_lock);

	if (found)
		session_gp_request_cancellation(slot);
}

/*
 * This callback is called on remap
 */
static void cbuf_vm_open(struct vm_area_struct *vmarea)
{
	struct cbuf *cbuf = vmarea->vm_private_data;

	cbuf_get(cbuf);
}

/*
 * This callback is called on unmap
 */
static void cbuf_vm_close(struct vm_area_struct *vmarea)
{
	struct cbuf *cbuf = vmarea->vm_private_data;

	cbuf_put(cbuf);
}

static const struct vm_operations_struct cbuf_vm_ops = {
	.open = cbuf_vm_open,
	.close = cbuf_vm_close,
};

/*
 * Create a cbuf object and add it to client
 */
int client_cbuf_create(struct tee_client *client, u32 len, uintptr_t *addr,
		       struct vm_area_struct *vmarea)
{
	int err = 0;
	struct cbuf *cbuf = NULL;
	unsigned int order;

	if (!client)
		return -EINVAL;

	if (!len || len > BUFFER_LENGTH_MAX)
		return -EINVAL;

	order = get_order(len);
	if (order > MAX_ORDER) {
		mc_dev_notice("Buffer size too large");
		return -ENOMEM;
	}

	/* Allocate buffer descriptor structure */
	cbuf = kzalloc(sizeof(*cbuf), GFP_KERNEL);
	if (!cbuf)
		return -ENOMEM;

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_cbufs);
	/* Allocate buffer */
	cbuf->addr = __get_free_pages(GFP_USER | __GFP_ZERO, order);
	if (!cbuf->addr) {
		kfree(cbuf);
		/* Decrement debug counter */
		atomic_dec(&g_ctx.c_cbufs);
		return -ENOMEM;
	}

	/* Map to user space if applicable */
	if (!client_is_kernel(client)) {
		err = cbuf_map(vmarea, cbuf->addr, len, &cbuf->uaddr);
		if (err) {
			free_pages(cbuf->addr, order);
			kfree(cbuf);
			/* Decrement debug counter */
			atomic_dec(&g_ctx.c_cbufs);
			return err;
		}
	}

	/* Init descriptor members */
	cbuf->client = client;
	cbuf->phys = virt_to_phys((void *)cbuf->addr);
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
	if (addr)
		*addr = cbuf->addr;

	/* Get a token on the client */
	client_get(client);

	/* Add buffer to list */
	mutex_lock(&client->cbufs_lock);
	list_add_tail(&cbuf->list, &client->cbufs);
	mutex_unlock(&client->cbufs_lock);
	mc_dev_devel("created cbuf %p: client %p addr %lx uaddr %lx len %u",
		     cbuf, client, cbuf->addr, cbuf->uaddr, cbuf->len);
	return err;
}

/*
 * Find a contiguous buffer (cbuf) in the cbuf list of given client that
 * contains given address and take a reference on it.
 * Return pointer to the object, or NULL if not found.
 */
static struct cbuf *cbuf_get_by_addr(struct tee_client *client, uintptr_t addr)
{
	struct cbuf *cbuf = NULL, *candidate;
	bool is_kernel = client_is_kernel(client);

	mutex_lock(&client->cbufs_lock);
	list_for_each_entry(candidate, &client->cbufs, list) {
		/* Compare to kernel VA or user VA depending on client type */
		uintptr_t start = is_kernel ?
			candidate->addr : candidate->uaddr;
		uintptr_t end = start + candidate->len;

		/* Check that (user) cbuf has not been unmapped */
		if (!start)
			break;

		if (addr >= start && addr < end) {
			cbuf = candidate;
			break;
		}
	}

	if (cbuf)
		cbuf_get(cbuf);

	mutex_unlock(&client->cbufs_lock);
	return cbuf;
}

/*
 * Remove a cbuf object from client, and mark it for freeing.
 * Freeing will happen once all current references are released.
 *
 * Note: this function could be subject to the same race condition as
 * client_gp_release_shared_mem() and client_put_cwsm_sva(), but it is trusted
 * as it can only be called by kernel drivers. So no lock around
 * cbuf_get_by_addr() and the two tee_cbuf_put().
 */
int client_cbuf_free(struct tee_client *client, uintptr_t addr)
{
	struct cbuf *cbuf = cbuf_get_by_addr(client, addr);

	if (!cbuf) {
		mc_dev_notice("cbuf %lu not found", addr);
		return -EINVAL;
	}

	/* Release reference taken by cbuf_get_by_addr */
	cbuf_put(cbuf);
	mutex_lock(&client->cbufs_lock);
	cbuf->api_freed = true;
	mutex_unlock(&client->cbufs_lock);
	cbuf_put(cbuf);
	return 0;
}

bool client_gp_operation_add(struct tee_client *client,
			     struct client_gp_operation *operation)
{
	struct client_gp_operation *op;
	bool found = false;

	mutex_lock(&client->quick_lock);
	list_for_each_entry(op, &client->operations, list)
		if (op->started == operation->started && op->cancelled) {
			found = true;
			break;
		}

	if (found) {
		list_del(&op->list);
		mc_dev_devel("found cancelled operation %p for started %u",
			     op, op->started);
		kfree(op);
	} else {
		list_add_tail(&operation->list, &client->operations);
		mc_dev_devel("add operation for started %u",
			     operation->started);
	}
	mutex_unlock(&client->quick_lock);
	return !found;
}

void client_gp_operation_remove(struct tee_client *client,
				struct client_gp_operation *operation)
{
	mutex_lock(&client->quick_lock);
	list_del(&operation->list);
	mutex_unlock(&client->quick_lock);
}

struct tee_mmu *client_mmu_create(struct tee_client *client,
				  const struct mc_ioctl_buffer *buf_in,
				  struct cbuf **cbuf_p, int client_fd)
{
	/* Check if buffer is contained in a cbuf */
	struct mc_ioctl_buffer buf = *buf_in;
	struct cbuf *cbuf = cbuf_get_by_addr(client, buf.va);
	struct mm_struct *mm = NULL;
	struct tee_mmu *mmu;

	*cbuf_p = cbuf;
	if (cbuf) {
		uintptr_t offset;

		if (client_is_kernel(client)) {
			offset = buf.va - cbuf->addr;
		} else {
			offset = buf.va - cbuf->uaddr;
			/* Update va to point to kernel address */
			buf.va = cbuf->addr + offset;
		}

		if ((offset + buf.len) > cbuf->len) {
			mc_dev_notice("crosses cbuf boundary");
			cbuf_put(cbuf);
			return ERR_PTR(-EINVAL);
		}
	} else if (!client_is_kernel(client)) {
		mm = get_mm_from_client_fd(client_fd);
		if (!mm) {
			mc_dev_notice("can't get mm from client fd %d",
				client_fd);
			return ERR_PTR(-EPERM);
		}
	}

	/* Build MMU table for buffer */
	mmu = tee_mmu_create(mm, &buf);
	if (mm)
		mmput(mm);

	if (IS_ERR_OR_NULL(mmu) && cbuf)
		cbuf_put(cbuf);

	return mmu;
}

void client_mmu_free(struct tee_client *client, uintptr_t va,
		     struct tee_mmu *mmu, struct cbuf *cbuf)
{
	tee_mmu_delete(mmu);
	if (cbuf)
		cbuf_put(cbuf);
}

void client_init(void)
{
	INIT_LIST_HEAD(&client_ctx.clients);
	mutex_init(&client_ctx.clients_lock);

	INIT_LIST_HEAD(&client_ctx.closing_clients);
	mutex_init(&client_ctx.closing_clients_lock);
}

static inline int cbuf_debug_structs(struct kasnprintf_buf *buf,
				     struct cbuf *cbuf)
{
	return kasnprintf(buf,
			  "\tcbuf %pK [%d]: addr %pK uaddr %pK len %u\n",
			  cbuf, kref_read(&cbuf->kref), (void *)cbuf->addr,
			  (void *)cbuf->uaddr, cbuf->len);
}

static inline int cwsm_debug_structs(struct kasnprintf_buf *buf,
				     struct cwsm *cwsm)
{
	return kasnprintf(buf,
			  "\tcwsm %pK [%d]: buf %pK len %llu flags 0x%x\n",
			  cwsm, kref_read(&cwsm->kref),
			  (void *)(uintptr_t)cwsm->memref.buffer,
			  cwsm->memref.size, cwsm->memref.flags);
}

static int client_debug_structs(struct kasnprintf_buf *buf,
				struct tee_client *client, bool is_closing)
{
	struct cbuf *cbuf;
	struct cwsm *cwsm;
	struct tee_session *session;
	int ret;

	if (client->pid)
		ret = kasnprintf(buf, "client %pK [%d]: %s (%d)%s\n",
				 client, kref_read(&client->kref),
				 client->comm, client->pid,
				 is_closing ? " <closing>" : "");
	else
		ret = kasnprintf(buf, "client %pK [%d]: [kernel]%s\n",
				 client, kref_read(&client->kref),
				 is_closing ? " <closing>" : "");

	if (ret < 0)
		return ret;

	/* Buffers */
	mutex_lock(&client->cbufs_lock);
	if (list_empty(&client->cbufs))
		goto done_cbufs;

	list_for_each_entry(cbuf, &client->cbufs, list) {
		ret = cbuf_debug_structs(buf, cbuf);
		if (ret < 0)
			goto done_cbufs;
	}

done_cbufs:
	mutex_unlock(&client->cbufs_lock);
	if (ret < 0)
		return ret;

	/* WMSs */
	mutex_lock(&client->quick_lock);
	if (list_empty(&client->cwsms))
		goto done_cwsms;

	list_for_each_entry(cwsm, &client->cwsms, list) {
		ret = cwsm_debug_structs(buf, cwsm);
		if (ret < 0)
			goto done_cwsms;
	}

done_cwsms:
	mutex_unlock(&client->quick_lock);
	if (ret < 0)
		return ret;

	/* Sessions */
	mutex_lock(&client->sessions_lock);
	list_for_each_entry(session, &client->sessions, list) {
		ret = session_debug_structs(buf, session, false);
		if (ret < 0)
			goto done_sessions;
	}

	list_for_each_entry(session, &client->closing_sessions, list) {
		ret = session_debug_structs(buf, session, true);
		if (ret < 0)
			goto done_sessions;
	}

done_sessions:
	mutex_unlock(&client->sessions_lock);

	if (ret < 0)
		return ret;

	return 0;
}

int clients_debug_structs(struct kasnprintf_buf *buf)
{
	struct tee_client *client;
	ssize_t ret = 0;

	mutex_lock(&client_ctx.clients_lock);
	list_for_each_entry(client, &client_ctx.clients, list) {
		ret = client_debug_structs(buf, client, false);
		if (ret < 0)
			break;
	}
	mutex_unlock(&client_ctx.clients_lock);

	if (ret < 0)
		return ret;

	mutex_lock(&client_ctx.closing_clients_lock);
	list_for_each_entry(client, &client_ctx.closing_clients, list) {
		ret = client_debug_structs(buf, client, true);
		if (ret < 0)
			break;
	}
	mutex_unlock(&client_ctx.closing_clients_lock);

	return ret;
}
