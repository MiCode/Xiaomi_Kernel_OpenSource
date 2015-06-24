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
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <crypto/hash.h>
#include <linux/scatterlist.h>
#include <linux/fs.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "platform.h"		/* MC_UIDGID_OLDSTYLE */
#include "main.h"
#include "debug.h"
#include "mmu.h"
#include "mcp.h"
#include "client.h"		/* *cbuf* */
#include "session.h"
#include "mci/mcimcp.h"

#define SHA1_HASH_SIZE       20

struct tbase_wsm {
	/* Buffer NWd addr (uva or kva, used only for lookup) */
	uintptr_t		va;
	/* buffer length */
	uint32_t		len;
	/* Buffer SWd addr */
	uint32_t		sva;
	/* mmu L2 table */
	struct tbase_mmu	*mmu;
	/* possibly a pointer to a cbuf */
	struct tbase_cbuf	*cbuf;
	/* list node */
	struct list_head	list;
};

/*
 * Postponed closing for GP TAs.
 * Implemented as a worker because cannot be executed from within isr_worker.
 */
static void session_close_worker(struct work_struct *work)
{
	struct mcp_session *mcp_session;
	struct tbase_session *session;

	mcp_session = container_of(work, struct mcp_session, close_work);
	session = container_of(mcp_session, struct tbase_session, mcp_session);
	session_close(session);
}

/* Forward declarations */
static struct tbase_wsm *wsm_create(struct tbase_session *session,
				    uintptr_t buf, uint32_t len);
static void wsm_free(struct tbase_wsm *wsm);

static int hash_path_and_data(char *hash, const void *data,
			      unsigned int data_len)
{
	struct mm_struct *mm = current->mm;
	struct hash_desc desc;
	struct scatterlist sg;
	char *buf;
	char *path;
	unsigned int path_len;
	int ret = 0;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	down_read(&mm->mmap_sem);
	if (!mm->exe_file) {
		ret = -ENOENT;
		goto end;
	}

	path = d_path(&mm->exe_file->f_path, buf, PAGE_SIZE);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		goto end;
	}

	MCDRV_DBG("current process path = ");
	{
		char *c;

		for (c = path; *c; c++)
			MCDRV_DBG("%c %d", *c, *c);
	}

	path_len = strnlen(path, PAGE_SIZE);
	MCDRV_DBG("path_len = %u", path_len);
	desc.tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm)) {
		ret = PTR_ERR(desc.tfm);
		MCDRV_DBG("could not alloc hash = %d", ret);
		goto end;
	}

	desc.flags = 0;
	sg_init_one(&sg, path, path_len);
	crypto_hash_init(&desc);
	crypto_hash_update(&desc, &sg, path_len);
	if (data) {
		MCDRV_DBG("current process path: hashing additional data\n");
		sg_init_one(&sg, data, data_len);
		crypto_hash_update(&desc, &sg, data_len);
	}

	crypto_hash_final(&desc, hash);
	crypto_free_hash(desc.tfm);

end:
	up_read(&mm->mmap_sem);
	free_page((unsigned long)buf);

	return ret;
}

static int check_prepare_identity(const struct mc_identity *identity,
				  struct identity *mcp_identity)
{
	struct mc_identity *mcp_id = (struct mc_identity *)mcp_identity;
	uint8_t hash[SHA1_HASH_SIZE];
	bool application = false;
	const void *data;
	unsigned int data_len;

	/* Mobicore doesn't support GP client authentication. */
	if (!g_ctx.f_client_login &&
	    (identity->login_type != TEEC_LOGIN_PUBLIC)) {
		MCDRV_DBG_WARN("Unsupported login type %d",
			       identity->login_type);
		return -EINVAL;
	}

	/* Copy login type */
	mcp_identity->login_type = identity->login_type;

	/* Fill in uid field */
	if ((identity->login_type == TEEC_LOGIN_USER) ||
	    (identity->login_type == TEEC_LOGIN_USER_APPLICATION)) {
		/* Set euid and ruid of the process. */
#if !defined(KUIDT_INIT) || defined(MC_UIDGID_OLDSTYLE)
		mcp_id->uid.euid = current_euid();
		mcp_id->uid.ruid = current_uid();
#else
		mcp_id->uid.euid = current_euid().val;
		mcp_id->uid.ruid = current_uid().val;
#endif
	}

	/* Check gid field */
	if ((identity->login_type == TEEC_LOGIN_GROUP) ||
	    (identity->login_type == TEEC_LOGIN_GROUP_APPLICATION)) {
#if !defined(KUIDT_INIT) || defined(MC_UIDGID_OLDSTYLE)
		gid_t gid = identity->gid;
#else
		kgid_t gid = {
			.val = identity->gid,
		};
#endif
		/* Check if gid is one of: egid of the process, its rgid or one
		 * of its supplementary groups */
		if (!in_egroup_p(gid) && !in_group_p(gid)) {
			MCDRV_DBG("group %d not allowed", identity->gid);
			return -EACCES;
		}

		MCDRV_DBG("group %d found", identity->gid);
		mcp_id->gid = identity->gid;
	}

	switch (identity->login_type) {
	case TEEC_LOGIN_PUBLIC:
	case TEEC_LOGIN_USER:
	case TEEC_LOGIN_GROUP:
		break;
	case TEEC_LOGIN_APPLICATION:
		application = true;
		data = NULL;
		data_len = 0;
		break;
	case TEEC_LOGIN_USER_APPLICATION:
		application = true;
		data = &mcp_id->uid;
		data_len = sizeof(mcp_id->uid);
		break;
	case TEEC_LOGIN_GROUP_APPLICATION:
		application = true;
		data = &identity->gid;
		data_len = sizeof(identity->gid);
		break;
	default:
		/* Any other login_type value is invalid. */
		MCDRV_DBG_WARN("Invalid login type");
		return -EINVAL;
	}

	if (application) {
		if (hash_path_and_data(hash, data, data_len)) {
			MCDRV_DBG("error in hash calculation");
			return -EAGAIN;
		}

		memcpy(&mcp_id->login_data, hash, sizeof(mcp_id->login_data));
	}

	return 0;
}

/*
 * Create a session object.
 * Note: object is not attached to client yet.
 */
struct tbase_session *session_create(struct tbase_client *client, bool is_gp,
				     struct mc_identity *identity)
{
	struct tbase_session *session;
	struct identity mcp_identity;

	if (is_gp) {
		/* Check identity method and data. */
		int ret = check_prepare_identity(identity, &mcp_identity);

		if (ret)
			return ERR_PTR(ret);
	}

	/* Allocate session object */
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return ERR_PTR(-ENOMEM);

	mutex_init(&session->close_lock);
	/* Initialise object members */
	mcp_session_init(&session->mcp_session, is_gp, &mcp_identity);
	INIT_WORK(&session->mcp_session.close_work, session_close_worker);
	session->client = client;
	kref_init(&session->kref);
	INIT_LIST_HEAD(&session->list);
	mutex_init(&session->wsms_lock);
	INIT_LIST_HEAD(&session->wsms);
	MCDRV_DBG("created session %p: client %p", session, session->client);
	return session;
}

int session_open(struct tbase_session *session, const struct tbase_object *obj,
		 const struct tbase_mmu *obj_mmu, uintptr_t tci, size_t len)
{
	struct mcp_buffer_map map;

	tbase_mmu_buffer(obj_mmu, &map);
	/* Create wsm object for tci */
	if (tci && len) {
		struct tbase_wsm *wsm;
		struct mcp_buffer_map tci_map;
		int ret = 0;

		mutex_lock(&session->wsms_lock);
		wsm = wsm_create(session, tci, len);
		if (IS_ERR(wsm))
			ret = PTR_ERR(wsm);

		mutex_unlock(&session->wsms_lock);
		if (ret)
			return ret;

		tbase_mmu_buffer(wsm->mmu, &tci_map);
		ret = mcp_open_session(&session->mcp_session, obj, &map,
				       &tci_map);
		if (ret) {
			mutex_lock(&session->wsms_lock);
			wsm_free(wsm);
			mutex_unlock(&session->wsms_lock);
		}

		return ret;
	}

	if (tci || len) {
		MCDRV_ERROR("Tci pointer and length are incoherent");
		return -EINVAL;
	}

	return mcp_open_session(&session->mcp_session, obj, &map, NULL);
}

/*
 * Close TA and unreference session object.
 * Object will be freed if reference reaches 0.
 * Session object is assumed to have been removed from main list, which means
 * that session_close cannot be called anymore.
 */
int session_close(struct tbase_session *session)
{
	int ret = 0;

	if (!session)
		return -ENXIO;

	mutex_lock(&session->close_lock);
	switch (mcp_close_session(&session->mcp_session)) {
	case 0:
		/* TA is closed, remove from closing list */
		mutex_lock(&g_ctx.closing_lock);
		list_del(&session->list);
		mutex_unlock(&g_ctx.closing_lock);
		/* Remove the ref we took on creation, exit if session freed */
		if (session_put(session))
			return 0;

		break;
	case -EBUSY:
		/*
		 * (GP) TA needs time to close. The "TA closed" notification
		 * will trigger a new call to session_close().
		 * Return OK but do not unref.
		 */
		break;
	default:
		MCDRV_ERROR("Failed to close session %x in SWd",
			    session->mcp_session.id);
		ret = -EPERM;
	}

	mutex_unlock(&session->close_lock);
	return ret;
}

/*
 * Free session object and all objects it contains (wsm).
 */
static void session_free(struct kref *kref)
{
	struct tbase_session *session;
	struct tbase_wsm *wsm, *next;

	/* Remove remaining shared buffers (unmapped in SWd by mcp_close) */
	session = container_of(kref, struct tbase_session, kref);
	list_for_each_entry_safe(wsm, next, &session->wsms, list) {
		MCDRV_DBG("session %p: free wsm %p", session, wsm);
		wsm_free(wsm);
	}

	MCDRV_DBG("freed session %p: client %p id %x",
		  session, session->client, session->mcp_session.id);
	kfree(session);
}

/*
 * Unreference session.
 * Free session object if reference reaches 0.
 */
int session_put(struct tbase_session *session)
{
	return kref_put(&session->kref, session_free);
}

/*
 * Send a notification to TA
 */
int session_notify_swd(struct tbase_session *session)
{
	if (!session) {
		MCDRV_ERROR("Session pointer is null");
		return -EINVAL;
	}

	return mcp_notify(&session->mcp_session);
}

/*
 * Read and clear last notification received from TA
 */
int32_t session_exitcode(struct tbase_session *session)
{
	return mcp_session_exitcode(&session->mcp_session);
}

/*
 * Free a WSM object
 */
static void wsm_free(struct tbase_wsm *wsm)
{
	/* Remove wsm from its parent session's list */
	list_del(&wsm->list);
	/* Free MMU table */
	if (!IS_ERR_OR_NULL(wsm->mmu))
		tbase_mmu_delete(wsm->mmu);

	/* Unref cbuf if applicable */
	if (wsm->cbuf)
		tbase_cbuf_put(wsm->cbuf);

	/* Delete wsm object */
	MCDRV_DBG("freed wsm %p: mmu %p cbuf %p va %lx len %u",
		  wsm, wsm->mmu, wsm->cbuf, wsm->va, wsm->len);
	kfree(wsm);
}

static struct tbase_wsm *wsm_create(struct tbase_session *session,
				    uintptr_t buf, uint32_t len)
{
	struct tbase_wsm *wsm;
	struct task_struct *task = NULL;
	uintptr_t va;
	int ret;

	/* Allocate structure */
	wsm = kzalloc(sizeof(*wsm), GFP_KERNEL);
	if (!wsm) {
		ret = -ENOMEM;
		goto err_no_wsm;
	}

	/* Add wsm to list so destroy can find it */
	list_add(&wsm->list, &session->wsms);

	/* Check if buffer is contained in a cbuf */
	wsm->cbuf = tbase_cbuf_get_by_addr(session->client, buf);
	if (wsm->cbuf) {
		uintptr_t offset;

		if (client_is_kernel(session->client))
			offset = buf - tbase_cbuf_addr(wsm->cbuf);
		else
			offset = buf - tbase_cbuf_uaddr(wsm->cbuf);

		if ((offset + len) > tbase_cbuf_len(wsm->cbuf)) {
			ret = -EINVAL;
			MCDRV_ERROR("crosses cbuf boundary");
			goto err;
		}
		/* Provide kernel virtual address */
		va = tbase_cbuf_addr(wsm->cbuf) + offset;
	} else {
		/* Not a cbuf. va is uva or kva depending on client. */
		/* Provide "task" if client is user */
		va = buf;
		if (!client_is_kernel(session->client))
			task = current;
	}

	/* Build MMU table for buffer */
	wsm->mmu = tbase_mmu_create(task, (void *)va, len);
	if (IS_ERR(wsm->mmu)) {
		ret = PTR_ERR(wsm->mmu);
		goto err;
	}

	wsm->va = buf;
	wsm->len = len;
	MCDRV_DBG("created wsm %p: mmu %p cbuf %p va %lx len %u",
		  wsm, wsm->mmu, wsm->cbuf, wsm->va, wsm->len);
	goto end;

err:
	wsm_free(wsm);
err_no_wsm:
	wsm = ERR_PTR(ret);
end:
	return wsm;
}

static inline int wsm_check(struct tbase_session *session,
			    struct mc_ioctl_buffer *buf)
{
	struct tbase_wsm *wsm;

	list_for_each_entry(wsm, &session->wsms, list) {
		if ((buf->va < (wsm->va + wsm->len)) &&
		    ((buf->va + buf->len) > wsm->va)) {
			MCDRV_ERROR("buffer %lx overlaps with existing wsm",
				    wsm->va);
			return -EADDRINUSE;
		}
	}

	return 0;
}

static inline struct tbase_wsm *wsm_find(struct tbase_session *session,
					 uintptr_t va)
{
	struct tbase_wsm *wsm;

	list_for_each_entry(wsm, &session->wsms, list)
		if (wsm->va == va)
			return wsm;

	return NULL;
}

static inline int wsm_info(struct tbase_wsm *wsm, struct kasnprintf_buf *buf)
{
	ssize_t ret;

	ret = kasnprintf(buf, "\t\twsm %p: mmu %p cbuf %p va %lx len %u\n",
			 wsm, wsm->mmu, wsm->cbuf, wsm->va, wsm->len);
	if (ret < 0)
		return ret;

	if (wsm->mmu) {
		ret = tbase_mmu_info(wsm->mmu, buf);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * Share buffers with SWd and add corresponding WSM objects to session.
 */
int session_wsms_add(struct tbase_session *session,
		     struct mc_ioctl_buffer *bufs)
{
	struct mc_ioctl_buffer *buf;
	struct mcp_buffer_map maps[MC_MAP_MAX];
	struct mcp_buffer_map *map;
	int i, ret = 0;
	uint32_t n_null_buf = 0;

	/* Check parameters */
	if (!session)
		return -ENXIO;

	/* Lock the session */
	mutex_lock(&session->wsms_lock);

	for (i = 0, buf = bufs, map = maps; i < MC_MAP_MAX; i++, buf++, map++) {
		if (!buf->va) {
			n_null_buf++;
			continue;
		}

		/* Avoid mapping overlaps */
		if (wsm_check(session, buf)) {
			ret = -EADDRINUSE;
			MCDRV_ERROR("maps[%d] va=%llx already map'd", i,
				    buf->va);
			goto unlock;
		}
	}

	if (n_null_buf >= MC_MAP_MAX) {
		ret = -EINVAL;
		MCDRV_ERROR("va=NULL");
		goto unlock;
	}

	for (i = 0, buf = bufs, map = maps; i < MC_MAP_MAX; i++, buf++, map++) {
		struct tbase_wsm *wsm;

		if (!buf->va) {
			map->type = WSM_INVALID;
			continue;
		}

		wsm = wsm_create(session, buf->va, buf->len);
		if (IS_ERR(wsm)) {
			ret = PTR_ERR(wsm);
			MCDRV_ERROR("maps[%d] va=%llx create failed: %d", i,
				    buf->va, ret);
			goto end;
		}

		tbase_mmu_buffer(wsm->mmu, map);
		MCDRV_DBG("maps[%d] va=%llx: t:%u a:%llx o:%u l:%u", i, buf->va,
			  map->type, map->phys_addr, map->offset, map->length);
	}

	/* Map buffers */
	if (g_ctx.f_multimap) {
		/* Send MCP message to map buffers in SWd */
		ret = mcp_multimap(session->mcp_session.id, maps);
		if (ret)
			MCDRV_ERROR("multimap failed: %d", ret);
	} else {
		/* Map each buffer */
		for (i = 0, buf = bufs, map = maps; i < MC_MAP_MAX; i++, buf++,
		     map++) {
			if (!buf->va)
				continue;

			/* Send MCP message to map buffer in SWd */
			ret = mcp_map(session->mcp_session.id, map);
			if (ret) {
				MCDRV_ERROR("maps[%d] va=%llx map failed: %d",
					    i, buf->va, ret);
				break;
			}
		}
	}

end:
	for (i = 0, buf = bufs, map = maps; i < MC_MAP_MAX; i++, buf++, map++) {
		struct tbase_wsm *wsm = wsm_find(session, buf->va);

		if (!buf->va)
			continue;

		if (ret) {
			if (!wsm)
				break;

			/* Destroy mapping */
			wsm_free(wsm);
		} else {
			/* Store mapping */
			buf->sva = map->secure_va;
			wsm->sva = buf->sva;
			MCDRV_DBG("maps[%d] va=%llx map'd len=%u sva=%llx",
				  i, buf->va, buf->len, buf->sva);
		}
	}

unlock:
	/* Unlock the session */
	mutex_unlock(&session->wsms_lock);
	return ret;
}

/*
 * Stop sharing buffers and delete corrsponding WSM objects.
 */
int session_wsms_remove(struct tbase_session *session,
			const struct mc_ioctl_buffer *bufs)
{
	const struct mc_ioctl_buffer *buf;
	struct mcp_buffer_map maps[MC_MAP_MAX];
	struct mcp_buffer_map *map;
	int i, ret = 0;
	uint32_t n_null_buf = 0;

	if (!session) {
		MCDRV_ERROR("session pointer is null");
		return -EINVAL;
	}

	/* Lock the session */
	mutex_lock(&session->wsms_lock);

	/* Find, check and map buffer */
	for (i = 0, buf = bufs, map = maps; i < MC_MAP_MAX; i++, buf++, map++) {
		struct tbase_wsm *wsm;

		if (!buf->va) {
			n_null_buf++;
			map->secure_va = 0;
			continue;
		}

		wsm = wsm_find(session, buf->va);
		if (!wsm) {
			ret = -EADDRNOTAVAIL;
			MCDRV_ERROR("maps[%d] va=%llx not found", i,
				    buf->va);
			goto out;
		}

		/* Check input params consistency */
		/* TODO: fix the spec, "len" is NOT ignored anymore */
		if ((wsm->sva != buf->sva) || (wsm->len != buf->len)) {
			MCDRV_ERROR("maps[%d] va=%llx no match: %x != %llx",
				    i, buf->va, wsm->sva, buf->sva);
			MCDRV_ERROR("maps[%d] va=%llx no match: %u != %u",
				    i, buf->va, wsm->len, buf->len);
			ret = -EINVAL;
			goto out;
		}

		tbase_mmu_buffer(wsm->mmu, map);
		map->secure_va = buf->sva;
		MCDRV_DBG("maps[%d] va=%llx: t:%u a:%llx o:%u l:%u s:%llx", i,
			  buf->va, map->type, map->phys_addr, map->offset,
			  map->length, map->secure_va);
	}

	if (n_null_buf >= MC_MAP_MAX) {
		ret = -EINVAL;
		MCDRV_ERROR("va=NULL");
		goto out;
	}

	if (g_ctx.f_multimap) {
		/* Send MCP command to unmap buffers in SWd */
		ret = mcp_multiunmap(session->mcp_session.id, maps);
		if (ret)
			MCDRV_ERROR("mcp_multiunmap failed: %d", ret);
	} else {
		for (i = 0, buf = bufs, map = maps; i < MC_MAP_MAX;
		     i++, buf++, map++) {
			if (!buf->va)
				continue;

			/* Send MCP command to unmap buffer in SWd */
			ret = mcp_unmap(session->mcp_session.id, map);
			if (ret) {
				MCDRV_ERROR("maps[%d] va=%llx unmap failed: %d",
					    i, buf->va, ret);
				break;
			}
		}
	}

	for (i = 0, buf = bufs; i < MC_MAP_MAX; i++, buf++) {
		struct tbase_wsm *wsm = wsm_find(session, buf->va);

		if (!wsm)
			break;

		/* Free wsm */
		wsm_free(wsm);
		MCDRV_DBG("maps[%d] va=%llx unmap'd len=%u sva=%llx", i,
			  buf->va, buf->len, buf->sva);
	}

out:
	mutex_unlock(&session->wsms_lock);
	return ret;
}

/*
 * Sleep until next notification from SWd.
 */
int session_waitnotif(struct tbase_session *session, int32_t timeout)
{
	return mcp_session_waitnotif(&session->mcp_session, timeout);
}

int session_info(struct tbase_session *session, struct kasnprintf_buf *buf)
{
	struct tbase_wsm *wsm;
	int32_t exit_code = mcp_session_exitcode(&session->mcp_session);
	int ret;

	ret = kasnprintf(buf, "\tsession %p: %x rc %d\n", session,
			 session->mcp_session.id, exit_code);
	if (ret < 0)
		return ret;

	/* WMSs */
	mutex_lock(&session->wsms_lock);
	if (list_empty(&session->wsms))
		goto done;

	list_for_each_entry(wsm, &session->wsms, list) {
		ret = wsm_info(wsm, buf);
		if (ret < 0)
			goto done;
	}

done:
	mutex_unlock(&session->wsms_lock);
	if (ret < 0)
		return ret;

	return 0;
}
