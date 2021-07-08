// SPDX-License-Identifier: GPL-2.0
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
	/* Virtual Machine ID string of the client */
	char			vm_id[16];
};

/* Context */
static struct client_ctx {
	/* Clients list */
	struct mutex		clients_lock;
	struct list_head	clients;
	/* Clients waiting for their last cwsm to be released */
	struct mutex		closing_clients_lock;
	struct list_head	closing_clients;
} client_ctx;

/* Buffer shared with SWd at client level */
struct cwsm {
	/* Client this cwsm belongs to */
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
 * Returns true if client is a kernel object.
 */
static inline bool client_is_kernel(struct tee_client *client)
{
	return !client->pid;
}

static struct cwsm *cwsm_create(struct tee_client *client,
				struct tee_mmu *mmu,
				const struct gp_shared_memory *memref,
				struct gp_return *gp_ret)
{
	struct cwsm *cwsm;
	u32 sva;
	int ret;

	cwsm = kzalloc(sizeof(*cwsm), GFP_KERNEL);
	if (!cwsm)
		return ERR_PTR(iwp_set_ret(-ENOMEM, gp_ret));

	if (mmu) {
		cwsm->mmu = mmu;
		tee_mmu_get(cwsm->mmu);
	} else {
		struct mc_ioctl_buffer buf = {
			.va = (uintptr_t)memref->buffer,
			.len = memref->size,
			.flags = memref->flags,
		};

		if (client_is_kernel(client)) {
			cwsm->mmu = tee_mmu_create(NULL, &buf);
		} else {
			struct mm_struct *mm = get_task_mm(current);

			if (!mm) {
				ret = -EPERM;
				mc_dev_err(ret, "can't get mm");
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
	}

	ret = iwp_register_shared_mem(cwsm->mmu, &sva, gp_ret);
	if (ret)
		goto err_mmu;

	/* Get a token on the client */
	client_get(client);
	cwsm->client = client;
	memcpy(&cwsm->memref, memref, sizeof(cwsm->memref));
	cwsm->sva = sva;
	kref_init(&cwsm->kref);
	INIT_LIST_HEAD(&cwsm->list);
	/* Add buffer to list */
	mutex_lock(&client->quick_lock);
	list_add_tail(&cwsm->list, &client->cwsms);
	mutex_unlock(&client->quick_lock);
	mc_dev_devel("created cwsm %p: client %p sva %x", cwsm, client, sva);
	/* Increment debug counter */
	atomic_inc(&g_ctx.c_cwsms);
	return cwsm;

err_mmu:
	tee_mmu_put(cwsm->mmu);
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
	tee_mmu_put(cwsm->mmu);
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

	mc_dev_devel("found sva %x", cwsm->sva);
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
struct tee_client *client_create(bool is_from_kernel, const char *vm_id)
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
	INIT_LIST_HEAD(&client->sessions);
	INIT_LIST_HEAD(&client->closing_sessions);
	mutex_init(&client->sessions_lock);
	INIT_LIST_HEAD(&client->list);
	mutex_init(&client->quick_lock);
	mutex_init(&client->cwsm_release_lock);
	INIT_LIST_HEAD(&client->cwsms);
	INIT_LIST_HEAD(&client->operations);
	strlcpy(client->vm_id, vm_id, sizeof(client->vm_id));
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

/* Client is closing: make sure all CSMs are gone */
static void client_release_cwsms(struct tee_client *client)
{
	/* Look for wsms that the client has not freed and put them */
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
		mc_dev_devel("flush cancelled operation %p for started %llu",
			     op, op->started);
		if (op->cancelled)
			kfree(op);
	}
	mutex_unlock(&client->quick_lock);
}

/*
 * Release a client and the session+cwsms objects it contains.
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
	/* Close all remaining sessions */
	client_close_sessions(client);
	/* Release all cwsms, no need to lock as sessions are closed */
	client_release_cwsms(client);
	client_release_gp_operations(client);
	client_put(client);
	mc_dev_devel("client %p closed", client);
}

/*
 * Clean all structures shared with the SWd (note: incomplete but unused)
 */
void client_cleanup(void)
{
	struct tee_client *client;

	mutex_lock(&client_ctx.clients_lock);
	list_for_each_entry(client, &client_ctx.clients, list) {
		mutex_lock(&client->sessions_lock);
		while (!list_empty(&client->sessions)) {
			struct tee_session *session;

			session = list_first_entry(&client->sessions,
						   struct tee_session, list);
			list_del(&session->list);
			session_mc_cleanup_session(session);
		}
		mutex_unlock(&client->sessions_lock);
	}
	mutex_unlock(&client_ctx.clients_lock);
}

/*
 * Get the vm id of the client passed in argument
 * @param client_t client
 * @return vm id of the client
 */
const char *client_vm_id(struct tee_client *client)
{
	return client->vm_id;
}

/*
 * Open TA for given client. TA binary is provided by the daemon.
 * @param
 * @return driver error code
 */
int client_mc_open_session(struct tee_client *client,
			   const struct mc_uuid_t *uuid,
			   uintptr_t tci_va, size_t tci_len, u32 *session_id)
{
	struct mcp_open_info info = {
		.type = TEE_MC_UUID,
		.uuid = uuid,
		.tci_va = tci_va,
		.tci_len = tci_len,
		.user = !client_is_kernel(client),
	};
	int ret;

	ret = client_mc_open_common(client, &info, session_id);
	mc_dev_devel("session %x, exit with %d", *session_id, ret);
	return ret;
}

/*
 * Open TA for given client. TA binary is provided by the client.
 * @param
 * @return driver error code
 */
int client_mc_open_trustlet(struct tee_client *client,
			    uintptr_t ta_va, size_t ta_len,
			    uintptr_t tci_va, size_t tci_len, u32 *session_id)
{
	struct mcp_open_info info = {
		.type = TEE_MC_TA,
		.va = ta_va,
		.len = ta_len,
		.tci_va = tci_va,
		.tci_len = tci_len,
		.user = !client_is_kernel(client),
	};
	int ret;

	ret = client_mc_open_common(client, &info, session_id);
	mc_dev_devel("session %x, exit with %d", *session_id, ret);
	return ret;
}

/*
 * Opens a TA and add corresponding session object to given client
 * return: driver error code
 */
int client_mc_open_common(struct tee_client *client, struct mcp_open_info *info,
			  u32 *session_id)
{
	struct tee_session *session = NULL;
	int ret = 0;

	/*
	 * Create session object with temp sid=0 BEFORE session is started,
	 * otherwise if a GP TA is started and NWd session object allocation
	 * fails, we cannot handle the potentially delayed GP closing.
	 * Adding session to list must be done AFTER it is started (once we have
	 * sid), therefore it cannot be done within session_create().
	 */
	session = session_create(client, NULL);
	if (IS_ERR(session))
		return PTR_ERR(session);

	ret = session_mc_open_session(session, info);
	if (ret)
		goto err;

	mutex_lock(&client->sessions_lock);
	/* Add session to client */
	list_add_tail(&session->list, &client->sessions);
	/* Set session ID returned by SWd */
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
		mc_dev_err(-ENXIO, "session %x not found", session_id);

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
	ret = session_mc_notify(session);
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
			     s32 timeout)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	ret = session_mc_wait(session, timeout);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit with %d", session_id, ret);
	return ret;
}

/*
 * Read session exit/termination code
 */
int client_get_session_exitcode(struct tee_client *client, u32 session_id,
				s32 *err)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	/* Retrieve error */
	ret = session_mc_get_err(session, err);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit code %d", session_id, *err);
	return ret;
}

/* Share a buffer with given TA in SWd */
int client_mc_map(struct tee_client *client, u32 session_id,
		  struct tee_mmu *mmu, struct mc_ioctl_buffer *buf)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	/* Add buffer to the session */
	ret = session_mc_map(session, mmu, buf);
	/* Put session */
	session_put(session);
	mc_dev_devel("session %x, exit with %d", session_id, ret);
	return ret;
}

/* Stop sharing a buffer with SWd */
int client_mc_unmap(struct tee_client *client, u32 session_id,
		    const struct mc_ioctl_buffer *buf)
{
	struct tee_session *session;
	int ret;

	/* Find/get session */
	session = client_get_session(client, session_id);
	if (!session)
		return -ENXIO;

	/* Remove buffer from session */
	ret = session_mc_unmap(session, buf);
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
				  struct tee_mmu *mmu, u32 *sva,
				  const struct gp_shared_memory *memref,
				  struct gp_return *gp_ret)
{
	struct cwsm *cwsm = NULL;

	if (memref->size > BUFFER_LENGTH_MAX) {
		mc_dev_err(-EINVAL, "buffer size %llu too big", memref->size);
		return -EINVAL;
	}

	if (!mmu)
		/* cwsm_find automatically takes a reference */
		cwsm = cwsm_find(client, memref);

	if (!cwsm)
		cwsm = cwsm_create(client, mmu, memref, gp_ret);

	/* gp_ret set by callee */
	if (IS_ERR(cwsm))
		return PTR_ERR(cwsm);

	if (sva)
		*sva = cwsm->sva;

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
 * Opens a TA and add corresponding session object to given client
 * return: driver error code
 */
int client_gp_open_session(struct tee_client *client,
			   const struct mc_uuid_t *uuid,
			   struct tee_mmu *ta_mmu,
			   struct gp_operation *operation,
			   const struct mc_identity *identity,
			   struct gp_return *gp_ret,
			   u32 *session_id)
{
	struct tee_session *session = NULL;
	int ret = 0;

	/*
	 * Create session object with temp sid=0 BEFORE session is started,
	 * otherwise if a GP TA is started and NWd session object allocation
	 * fails, we cannot handle the potentially delayed GP closing.
	 * Adding session to list must be done AFTER it is started (once we have
	 * sid), therefore it cannot be done within session_create().
	 */
	session = session_create(client, identity);
	if (IS_ERR(session))
		return iwp_set_ret(PTR_ERR(session), gp_ret);

	/* Open session */
	ret = session_gp_open_session(session, uuid, ta_mmu, operation, gp_ret);
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

	mc_dev_devel("gp session %x, exit with %d", *session_id, ret);
	return ret;
}

int client_gp_open_session_domu(struct tee_client *client,
				const struct mc_uuid_t *uuid,
				struct tee_mmu *ta_mmu,
				u64 started,
				struct interworld_session *iws,
				struct tee_mmu **mmus,
				struct gp_return *gp_ret)
{
	struct tee_session *session = NULL;
	int ret = 0;

	/* Don't pass NULL for identity as it would make a MC session */
	session = session_create(client, ERR_PTR(-ENOENT));
	if (IS_ERR(session))
		return iwp_set_ret(PTR_ERR(session), gp_ret);

	/* Open session */
	ret = session_gp_open_session_domu(session, uuid, ta_mmu, started, iws,
					   mmus, gp_ret);
	if (ret)
		goto end;

	mutex_lock(&client->sessions_lock);
	/* Add session to client */
	list_add_tail(&session->list, &client->sessions);
	mutex_unlock(&client->sessions_lock);

end:
	if (ret)
		session_put(session);

	mc_dev_devel("gp session %x, exit with %d",
		     session->iwp_session.sid, ret);
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
			     struct gp_return *gp_ret)
{
	struct tee_session *session;
	int ret = 0;

	session = client_get_session(client, session_id);
	if (!session)
		return iwp_set_ret(-ENXIO, gp_ret);

	ret = session_gp_invoke_command(session, command_id, operation, gp_ret);

	/* Put session */
	session_put(session);
	return ret;
}

int client_gp_invoke_command_domu(struct tee_client *client, u32 session_id,
				  u64 started, struct interworld_session *iws,
				  struct tee_mmu **mmus,
				  struct gp_return *gp_ret)
{
	struct tee_session *session;
	int ret = 0;

	session = client_get_session(client, session_id);
	if (!session)
		return iwp_set_ret(-ENXIO, gp_ret);

	ret = session_gp_invoke_command_domu(session, started, iws, mmus,
					     gp_ret);

	/* Put session */
	session_put(session);
	return ret;
}

void client_gp_request_cancellation(struct tee_client *client, u64 started)
{
	struct client_gp_operation *op;
	u64 slot;
	bool found = false;

	/* Look for operation */
	mutex_lock(&client->quick_lock);
	list_for_each_entry(op, &client->operations, list)
		if (op->started == started) {
			slot = op->slot;
			found = true;
			mc_dev_devel(
				"found no operation cancel for started %llu",
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
			mc_dev_devel(
				"add cancelled operation %p for started %llu",
				op, op->started);
		}
	}
	mutex_unlock(&client->quick_lock);

	if (found)
		session_gp_request_cancellation(slot);
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
		mc_dev_devel("found cancelled operation %p for started %llu",
			     op, op->started);
		kfree(op);
	} else {
		list_add_tail(&operation->list, &client->operations);
		mc_dev_devel("add operation for started %llu",
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
				  const struct mc_ioctl_buffer *buf_in)
{
	struct mc_ioctl_buffer buf = *buf_in;
	struct mm_struct *mm = NULL;
	struct tee_mmu *mmu;

	if (!client_is_kernel(client)) {
		mm = get_task_mm(current);
		if (!mm) {
			mc_dev_err(-EPERM, "can't get mm");
			return ERR_PTR(-EPERM);
		}
	}

	/* Build MMU table for buffer */
	mmu = tee_mmu_create(mm, &buf);
	if (mm)
		mmput(mm);

	return mmu;
}

void client_init(void)
{
	INIT_LIST_HEAD(&client_ctx.clients);
	mutex_init(&client_ctx.clients_lock);

	INIT_LIST_HEAD(&client_ctx.closing_clients);
	mutex_init(&client_ctx.closing_clients_lock);
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
	struct cwsm *cwsm;
	struct tee_session *session;
	int ret;

	if (client->pid)
		ret = kasnprintf(buf, "client %pK [%d]: %s (%d)%s\n",
				 client, kref_read(&client->kref),
				 client->comm, client->pid,
				 is_closing ? " <closing>" : "");
	else
		ret = kasnprintf(buf, "client %pK [%d]: [%s] %s\n",
				 client, kref_read(&client->kref),
				 client->vm_id[0] ? client->vm_id : "kernel",
				 is_closing ? " <closing>" : "");

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
