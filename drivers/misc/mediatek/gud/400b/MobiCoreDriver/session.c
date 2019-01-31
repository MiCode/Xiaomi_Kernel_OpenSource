/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/net.h>
#include <net/sock.h>		/* sockfd_lookup */
#include <linux/version.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/clock.h>	/* local_clock */
#include <linux/sched/task.h>	/* put_task_struct */
#endif

#include "public/mc_user.h"
#include "public/mc_admin.h"

#if KERNEL_VERSION(3, 5, 0) <= LINUX_VERSION_CODE
#include <linux/uidgid.h>
#else
#define kuid_t uid_t
#define kgid_t gid_t
#define KGIDT_INIT(value) ((kgid_t)value)

static inline uid_t __kuid_val(kuid_t uid)
{
	return uid;
}

static inline gid_t __kgid_val(kgid_t gid)
{
	return gid;
}

static inline bool gid_eq(kgid_t left, kgid_t right)
{
	return __kgid_val(left) == __kgid_val(right);
}

static inline bool gid_gt(kgid_t left, kgid_t right)
{
	return __kgid_val(left) > __kgid_val(right);
}

static inline bool gid_lt(kgid_t left, kgid_t right)
{
	return __kgid_val(left) < __kgid_val(right);
}
#endif
#include "main.h"
#include "mmu.h"
#include "mcp.h"
#include "client.h"		/* *cbuf* */
#include "session.h"
#include "mci/mcimcp.h"		/* WSM_INVALID */

#define SHA1_HASH_SIZE       20

static int wsm_create(struct tee_session *session, struct tee_wsm *wsm,
		      const struct mc_ioctl_buffer *buf, int client_fd)
{
	if (wsm->in_use) {
		mc_dev_notice("wsm already in use");
		return -EINVAL;
	}

	wsm->mmu = client_mmu_create(session->client, buf, &wsm->cbuf,
				     client_fd);
	if (IS_ERR(wsm->mmu))
		return PTR_ERR(wsm->mmu);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_wsms);
	wsm->va = buf->va;
	wsm->len = buf->len;
	wsm->flags = buf->flags;
	wsm->in_use = true;
	return 0;
}

/*
 * Free a WSM object, must be called under the session's wsms_lock
 */
static void wsm_free(struct tee_session *session, struct tee_wsm *wsm)
{
	if (!wsm->in_use) {
		mc_dev_notice("wsm not in use");
		return;
	}

	/* Free MMU table */
	client_mmu_free(session->client, wsm->va, wsm->mmu, wsm->cbuf);
	/* Delete wsm object */
	mc_dev_devel("freed wsm %p: mmu %p cbuf %p va %lx len %u sva %x",
		     wsm, wsm->mmu, wsm->cbuf, wsm->va, wsm->len, wsm->sva);
	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_wsms);
	wsm->in_use = false;
}

#if KERNEL_VERSION(4, 6, 0) <= LINUX_VERSION_CODE
static int hash_path_and_data(struct task_struct *task, u8 *hash,
			      const void *data, unsigned int data_len)
{
	struct mm_struct *mm = task->mm;
	struct crypto_shash *tfm;
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

	mc_dev_devel("process path =");
	{
		char *c;

		for (c = path; *c; c++)
			mc_dev_devel("%c %d", *c, *c);
	}

	path_len = (unsigned int)strnlen(path, PAGE_SIZE);
	mc_dev_devel("path_len = %u", path_len);
	/* Compute hash of path */
	tfm = crypto_alloc_shash("sha1", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		mc_dev_notice("cannot allocate shash: %d", ret);
		goto end;
	}

	{
		SHASH_DESC_ON_STACK(desc, tfm);

		desc->tfm = tfm;
		desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

		crypto_shash_init(desc);
		crypto_shash_update(desc, (u8 *)path, path_len);
		if (data) {
			mc_dev_devel("hashing additional data");
			crypto_shash_update(desc, data, data_len);
		}

		crypto_shash_final(desc, hash);
		shash_desc_zero(desc);
	}

	crypto_free_shash(tfm);

end:
	up_read(&mm->mmap_sem);
	free_page((unsigned long)buf);

	return ret;
}
#else
static int hash_path_and_data(struct task_struct *task, u8 *hash,
			      const void *data, unsigned int data_len)
{
	struct mm_struct *mm = task->mm;
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

	mc_dev_devel("process path =");
	{
		char *c;

		for (c = path; *c; c++)
			mc_dev_devel("%c %d", *c, *c);
	}

	path_len = (unsigned int)strnlen(path, PAGE_SIZE);
	mc_dev_devel("path_len = %u", path_len);
	/* Compute hash of path */
	desc.tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm)) {
		ret = PTR_ERR(desc.tfm);
		mc_dev_devel("could not alloc hash = %d", ret);
		goto end;
	}

	desc.flags = 0;
	sg_init_one(&sg, path, path_len);
	crypto_hash_init(&desc);
	crypto_hash_update(&desc, &sg, path_len);
	if (data) {
		mc_dev_devel("current process path: hashing additional data");
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
#endif

#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
#define GROUP_AT(gi, i) ((gi)->gid[i])
#endif

/*
 * groups_search is not EXPORTed so copied from kernel/groups.c
 * a simple bsearch
 */
static int has_group(const struct cred *cred, gid_t id_gid)
{
	const struct group_info *group_info = cred->group_info;
	unsigned int left, right;
	kgid_t gid = KGIDT_INIT(id_gid);

	if (gid_eq(gid, cred->fsgid) || gid_eq(gid, cred->egid))
		return 1;

	if (!group_info)
		return 0;

	left = 0;
	right = group_info->ngroups;
	while (left < right) {
		unsigned int mid = (left + right) / 2;

		if (gid_gt(gid, GROUP_AT(group_info, mid)))
			left = mid + 1;
		else if (gid_lt(gid, GROUP_AT(group_info, mid)))
			right = mid;
		else
			return 1;
	}
	return 0;
}

static int check_prepare_identity(const struct mc_identity *identity,
				  struct identity *mcp_identity,
				  struct task_struct *task)
{
	struct mc_identity *mcp_id = (struct mc_identity *)mcp_identity;
	u8 hash[SHA1_HASH_SIZE];
	bool application = false;
	const void *data;
	unsigned int data_len;

	/* Copy login type */
	mcp_identity->login_type = identity->login_type;

	if ((identity->login_type == LOGIN_PUBLIC) ||
	    (identity->login_type == TEEC_TT_LOGIN_KERNEL))
		return 0;

	/* Mobicore doesn't support GP client authentication. */
	if (!g_ctx.f_client_login) {
		mc_dev_notice("Unsupported login type %x",
			identity->login_type);
		return -EINVAL;
	}

	/* Fill in uid field */
	if ((identity->login_type == LOGIN_USER) ||
	    (identity->login_type == LOGIN_USER_APPLICATION)) {
		/* Set euid and ruid of the process. */
		mcp_id->uid.euid = __kuid_val(task_euid(task));
		mcp_id->uid.ruid = __kuid_val(task_uid(task));
	}

	/* Check gid field */
	if ((identity->login_type == LOGIN_GROUP) ||
	    (identity->login_type == LOGIN_GROUP_APPLICATION)) {
		const struct cred *cred = __task_cred(task);

		/*
		 * Check if gid is one of: egid of the process, its rgid or one
		 * of its supplementary groups
		 */
		if (!has_group(cred, identity->gid)) {
			mc_dev_notice("group %d not allowed", identity->gid);
			return -EACCES;
		}

		mc_dev_devel("group %d found", identity->gid);
		mcp_id->gid = identity->gid;
	}

	switch (identity->login_type) {
	case LOGIN_PUBLIC:
	case LOGIN_USER:
	case LOGIN_GROUP:
		break;
	case LOGIN_APPLICATION:
		application = true;
		data = NULL;
		data_len = 0;
		break;
	case LOGIN_USER_APPLICATION:
		application = true;
		data = &mcp_id->uid;
		data_len = sizeof(mcp_id->uid);
		break;
	case LOGIN_GROUP_APPLICATION:
		application = true;
		data = &identity->gid;
		data_len = sizeof(identity->gid);
		break;
	default:
		/* Any other login_type value is invalid. */
		mc_dev_notice("Invalid login type %d", identity->login_type);
		return -EINVAL;
	}

	if (application) {
		int ret = hash_path_and_data(task, hash, data, data_len);

		if (ret) {
			mc_dev_devel("hash calculation returned %d", ret);
			return ret;
		}

		memcpy(&mcp_id->login_data, hash, sizeof(mcp_id->login_data));
	}

	return 0;
}

/*
 * The proxy gives us the fd of its server-side socket, so we can find out the
 * PID of its client. put_task_struct() must be called to free the resource.
 */
static struct task_struct *get_task_struct_from_client_fd(int client_fd)
{
	struct task_struct *task = NULL;
	struct socket *sock;
	int err;

	if (client_fd < 0) {
		get_task_struct(current);
		return current;
	}

	sock = sockfd_lookup(client_fd, &err);
	if (!sock)
		return NULL;

	if (sock->sk && sock->sk->sk_peer_pid) {
		rcu_read_lock();
		task = pid_task(sock->sk->sk_peer_pid, PIDTYPE_PID);
		get_task_struct(task);
		rcu_read_unlock();
	}

	sockfd_put(sock);
	return task;
}

/*
 * Create a session object.
 * Note: object is not attached to client yet.
 */
struct tee_session *session_create(struct tee_client *client, bool is_gp,
				   struct mc_identity *identity, int client_fd)
{
	struct tee_session *session;
	struct identity mcp_identity;
	int i;

	if (is_gp) {
		/* Check identity method and data. */
		struct task_struct *task;
		int ret;

		task = get_task_struct_from_client_fd(client_fd);
		if (!task) {
			mc_dev_notice("can't get task from client fd %d",
				   client_fd);
			return ERR_PTR(-EINVAL);
		}

		ret = check_prepare_identity(identity, &mcp_identity, task);
		put_task_struct(task);
		if (ret)
			return ERR_PTR(ret);
	}

	/* Allocate session object */
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return ERR_PTR(-ENOMEM);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_sessions);
	/* Initialise object members */
	if (is_gp)
		iwp_session_init(&session->iwp_session, &mcp_identity);
	else
		mcp_session_init(&session->mcp_session);

	client_get(client);
	session->client = client;
	kref_init(&session->kref);
	INIT_LIST_HEAD(&session->list);
	mutex_init(&session->wsms_lock);
	for (i = 0; i < MC_MAP_MAX; i++)
		session->wsms_lru[i] = &session->wsms[i];
	mc_dev_devel("created session %p: client %p",
		     session, session->client);
	return session;
}

int session_open(struct tee_session *session, const struct tee_object *obj,
		 const struct tee_mmu *obj_mmu, uintptr_t tci, size_t len,
		 int client_fd)
{
	struct mcp_buffer_map map;

	if (len > BUFFER_LENGTH_MAX)
		return -EINVAL;

	tee_mmu_buffer(obj_mmu, &map);
	/* Create wsm object for tci */
	if (tci && len) {
		struct tee_wsm *wsm = &session->tci;
		struct mcp_buffer_map tci_map;
		struct mc_ioctl_buffer buf;
		int ret = 0;

		buf.va = tci;
		buf.len = len;
		buf.flags = MC_IO_MAP_INPUT_OUTPUT;
		buf.sva = 0;
		ret = wsm_create(session, wsm, &buf, client_fd);
		if (ret)
			return ret;

		mc_dev_devel("created tci: mmu %p cbuf %p va %lx len %u",
			     wsm->mmu, wsm->cbuf, wsm->va, wsm->len);

		tee_mmu_buffer(wsm->mmu, &tci_map);
		ret = mcp_open_session(&session->mcp_session, obj, &map,
				       &tci_map);
		if (ret) {
			wsm_free(session, wsm);
			return ret;
		}

		return 0;
	}

	if (tci || len) {
		mc_dev_devel("Tci pointer and length are incoherent");
		return -EINVAL;
	}

	return mcp_open_session(&session->mcp_session, obj, &map, NULL);
}

/*
 * Close session and unreference session object.
 * Session object is assumed to have been removed from main list, which means
 * that session_close cannot be called anymore.
 */
int session_close(struct tee_session *session)
{
	int ret;

	if (nq_session_is_gp(&session->nq_session)) {
		ret = iwp_close_session(&session->iwp_session);
		if (!ret)
			mc_dev_devel("closed GP session %x",
				     session->iwp_session.sid);
	} else {
		ret = mcp_close_session(&session->mcp_session);
		if (!ret)
			mc_dev_devel("closed MC session %x",
				     session->mcp_session.sid);
	}
	return ret;
}

/*
 * Free session object and all objects it contains (wsm).
 */
static void session_release(struct kref *kref)
{
	struct tee_session *session;
	int i;

	/* Remove remaining shared buffers (unmapped in SWd by mcp_close) */
	session = container_of(kref, struct tee_session, kref);
	for (i = 0; i < MC_MAP_MAX; i++) {
		if (!session->wsms[i].in_use)
			continue;

		mc_dev_devel("session %p: free wsm #%d", session, i);
		wsm_free(session, &session->wsms[i]);
		/* Buffer unmapped by SWd */
		atomic_dec(&g_ctx.c_maps);
	}

	if (session->tci.in_use) {
		mc_dev_devel("session %p: free tci", session);
		wsm_free(session, &session->tci);
	}

	if (nq_session_is_gp(&session->nq_session))
		mc_dev_devel("freed GP session %p: client %p id %x", session,
			     session->client, session->iwp_session.sid);
	else
		mc_dev_devel("freed MC session %p: client %p id %x", session,
			     session->client, session->mcp_session.sid);

	client_put(session->client);
	kfree(session);
	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_sessions);
}

/*
 * Unreference session.
 * Free session object if reference reaches 0.
 */
int session_put(struct tee_session *session)
{
	return kref_put(&session->kref, session_release);
}

/*
 * Session is to be removed from NWd records as SWd is dead
 */
int session_kill(struct tee_session *session)
{
	mcp_kill_session(&session->mcp_session);
	return session_put(session);
}

/*
 * Send a notification to TA
 */
int session_notify_swd(struct tee_session *session)
{
	if (!session) {
		mc_dev_devel("Session pointer is null");
		return -EINVAL;
	}

	return mcp_notify(&session->mcp_session);
}

/*
 * Read and clear last notification received from TA
 */
s32 session_exitcode(struct tee_session *session)
{
	return mcp_session_exitcode(&session->mcp_session);
}

static int wsm_debug_structs(struct kasnprintf_buf *buf, struct tee_wsm *wsm,
			     int no)
{
	ssize_t ret;

	if (!wsm->in_use)
		return 0;

	ret = kasnprintf(buf, "\t\t");
	if (no < 0)
		ret = kasnprintf(buf, "tci %p: cbuf %p va %lx len %u\n",
				 wsm, wsm->cbuf, wsm->va, wsm->len);
	else if (wsm->in_use)
		ret = kasnprintf(buf,
				 "wsm #%d: cbuf %p va %lx len %u sva %x\n",
				 no, wsm->cbuf, wsm->va, wsm->len, wsm->sva);

	if (ret < 0)
		return ret;

	if (wsm->mmu) {
		ret = tee_mmu_debug_structs(buf, wsm->mmu);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * Share buffers with SWd and add corresponding WSM objects to session.
 * This may involve some re-use or cleanup of inactive mappings.
 */
int session_map(struct tee_session *session, struct mc_ioctl_buffer *buf,
		int client_fd)
{
	struct tee_wsm *wsm;
	struct mcp_buffer_map map;
	int i, ret;

	mutex_lock(&session->wsms_lock);
	/* Look for an available slot in the session WSMs array */
	for (i = 0; i < MC_MAP_MAX; i++)
		if (!session->wsms[i].in_use)
			break;

	if (i == MC_MAP_MAX) {
		ret = -EPERM;
		mc_dev_devel("no available WSM slot in session %x",
			     session->mcp_session.sid);
		goto out;
	}

	wsm = &session->wsms[i];
	ret = wsm_create(session, wsm, buf, client_fd);
	if (ret) {
		mc_dev_devel("maps[%d] va=%llx create failed: %d",
			     i, buf->va, ret);
		goto out;
	}

	mc_dev_devel("created wsm #%d: mmu %p cbuf %p va %lx len %u",
		     i, wsm->mmu, wsm->cbuf, wsm->va, wsm->len);
	tee_mmu_buffer(wsm->mmu, &map);
	mc_dev_devel("maps[%d] va=%llx: t:%u a:%llx o:%u l:%u", i, buf->va,
		     map.type, map.phys_addr, map.offset, map.length);

	ret = mcp_map(session->mcp_session.sid, &map);
	if (ret) {
		wsm_free(session, wsm);
	} else {
		buf->sva = map.secure_va;
		wsm->sva = map.secure_va;
	}

out:
	mutex_unlock(&session->wsms_lock);
	mc_dev_devel("ret=%d", ret);
	return ret;
}

/*
 * In theory, stop sharing buffers with the SWd. In fact, mark them inactive.
 */
int session_unmap(struct tee_session *session,
		  const struct mc_ioctl_buffer *buf)
{
	struct tee_wsm *wsm;
	struct mcp_buffer_map map;
	int i, ret = -EINVAL;

	mutex_lock(&session->wsms_lock);
	/* Look for buffer in the session WSMs array */
	for (i = 0; i < MC_MAP_MAX; i++)
		if ((session->wsms[i].in_use) &&
		    (buf->va == session->wsms[i].va) &&
		    (buf->len == session->wsms[i].len) &&
		    (buf->sva == session->wsms[i].sva))
			break;

	if (i == MC_MAP_MAX) {
		ret = -EINVAL;
		mc_dev_devel("maps[%d] va=%llx sva=%llx not found",
			     i, buf[i].va, buf[i].sva);
		goto out;
	}

	wsm = &session->wsms[i];
	tee_mmu_buffer(wsm->mmu, &map);
	map.secure_va = wsm->sva;
	ret = mcp_unmap(session->mcp_session.sid, &map);
	if (!ret)
		wsm_free(session, wsm);

out:
	mutex_unlock(&session->wsms_lock);
	return ret;
}

/*
 * Sleep until next notification from SWd.
 */
int session_waitnotif(struct tee_session *session, s32 timeout,
		      bool silent_expiry)
{
	return mcp_session_waitnotif(&session->mcp_session, timeout,
				     silent_expiry);
}

static void unmap_gp_bufs(struct tee_session *session,
			  struct iwp_buffer_map *maps)
{
	int i;

	/* Create WSMs from bufs */
	mutex_lock(&session->wsms_lock);
	for (i = 0; i < MC_MAP_MAX; i++) {
		if (session->wsms[i].in_use)
			wsm_free(session, &session->wsms[i]);

		if (maps[i].sva)
			client_put_cwsm_sva(session->client, maps[i].sva);
	}
	mutex_unlock(&session->wsms_lock);
}

static int map_gp_bufs(struct tee_session *session,
		       const struct mc_ioctl_buffer *bufs,
		       struct gp_shared_memory **parents,
		       struct iwp_buffer_map *maps, int client_fd)
{
	int i, ret = 0;

	/* Create WSMs from bufs */
	mutex_lock(&session->wsms_lock);
	for (i = 0; i < MC_MAP_MAX; i++) {
		/* Reset reference for temporary memory */
		maps[i].map.phys_addr = 0;
		/* Reset reference for registered memory */
		maps[i].sva = 0;
		if (bufs[i].va) {
			/* Temporary memory, needs mapping */
			ret = wsm_create(session, &session->wsms[i], &bufs[i],
					 client_fd);
			if (ret) {
				mc_dev_devel(
					"maps[%d] va=%llx create failed: %d",
					i, bufs[i].va, ret);
				break;
			}

			tee_mmu_buffer(session->wsms[i].mmu, &maps[i].map);
		} else if (parents[i]) {
			/* Registered memory, already mapped */
			maps[i].sva = client_get_cwsm_sva(session->client,
							  parents[i]);
			if (!maps[i].sva) {
				ret = -EINVAL;
				mc_dev_devel("couldn't find shared mem");
				break;
			}
		}
	}
	mutex_unlock(&session->wsms_lock);

	/* Failed above */
	if (i < MC_MAP_MAX)
		unmap_gp_bufs(session, maps);

	return ret;
}

int session_gp_open_session(struct tee_session *session,
			    const struct tee_object *obj,
			    const struct tee_mmu *obj_mmu,
			    struct gp_operation *operation,
			    struct gp_return *gp_ret,
			    int client_fd)
{
	/* TEEC_MEMREF_TEMP_* buffers to map */
	struct mc_ioctl_buffer bufs[MC_MAP_MAX];
	struct iwp_buffer_map maps[MC_MAP_MAX];
	struct gp_shared_memory *parents[MC_MAP_MAX];
	struct mcp_buffer_map obj_map;
	struct client_gp_operation client_operation;
	int ret = 0;

	ret = iwp_open_session_prepare(&session->iwp_session, obj, operation,
				       bufs, parents, gp_ret);
	if (ret)
		return ret;

	/* Create WSMs from bufs */
	ret = map_gp_bufs(session, bufs, parents, maps, client_fd);
	if (ret) {
		iwp_open_session_abort(&session->iwp_session);
		return iwp_set_ret(ret, gp_ret);
	}

	/* Tell client about operation */
	client_operation.started = operation->started;
	client_operation.slot = iwp_session_slot(&session->iwp_session);
	client_operation.cancelled = false;
	if (!client_gp_operation_add(session->client, &client_operation)) {
		iwp_open_session_abort(&session->iwp_session);
		return iwp_set_ret(-ECANCELED, gp_ret);
	}
	/* Open/call TA */
	tee_mmu_buffer(obj_mmu, &obj_map);
	ret = iwp_open_session(&session->iwp_session, operation, &obj_map, maps,
			       gp_ret);
	/* Cleanup */
	client_gp_operation_remove(session->client, &client_operation);
	unmap_gp_bufs(session, maps);
	return ret;
}

int session_gp_invoke_command(struct tee_session *session, u32 command_id,
			      struct gp_operation *operation,
			      struct gp_return *gp_ret, int client_fd)
{
	/* TEEC_MEMREF_TEMP_* buffers to map */
	struct mc_ioctl_buffer bufs[4];
	struct iwp_buffer_map maps[MC_MAP_MAX];
	struct gp_shared_memory *parents[MC_MAP_MAX];
	struct client_gp_operation client_operation;
	int ret = 0;

	ret = iwp_invoke_command_prepare(&session->iwp_session, command_id,
					 operation, bufs, parents, gp_ret);
	if (ret)
		return ret;

	/* Create WSMs from bufs */
	ret = map_gp_bufs(session, bufs, parents, maps, client_fd);
	if (ret) {
		iwp_invoke_command_abort(&session->iwp_session);
		return iwp_set_ret(ret, gp_ret);
	}

	/* Tell client about operation */
	client_operation.started = operation->started;
	client_operation.slot = iwp_session_slot(&session->iwp_session);
	client_operation.cancelled = false;
	if (!client_gp_operation_add(session->client, &client_operation)) {
		iwp_invoke_command_abort(&session->iwp_session);
		return iwp_set_ret(-ECANCELED, gp_ret);
	}
	/* Call TA */
	ret = iwp_invoke_command(&session->iwp_session, operation, maps,
				 gp_ret);
	/* Cleanup */
	client_gp_operation_remove(session->client, &client_operation);
	unmap_gp_bufs(session, maps);
	return ret;
}

int session_gp_request_cancellation(u32 slot)
{
	return iwp_request_cancellation(slot);
}

int session_debug_structs(struct kasnprintf_buf *buf,
			  struct tee_session *session, bool is_closing)
{
	const char *type;
	u32 session_id;
	s32 exit_code;
	int i, ret;

	if (nq_session_is_gp(&session->nq_session)) {
		session_id = session->iwp_session.sid;
		exit_code = 0;
		type = "GP";
	} else {
		session_id = session->mcp_session.sid;
		exit_code = mcp_session_exitcode(&session->mcp_session);
		type = "MC";
	}

	ret = kasnprintf(buf, "\tsession %p [%d]: %4x %s ec %d%s\n",
			 session, kref_read(&session->kref), session_id, type,
			 exit_code, is_closing ? " <closing>" : "");
	if (ret < 0)
		return ret;

	/* TCI */
	if (session->tci.in_use) {
		ret = wsm_debug_structs(buf, &session->tci, -1);
		if (ret < 0)
			return ret;
	}

	/* WMSs */
	mutex_lock(&session->wsms_lock);
	for (i = 0; i < MC_MAP_MAX; i++) {
		ret = wsm_debug_structs(buf, &session->wsms[i], i);
		if (ret < 0)
			break;
	}
	mutex_unlock(&session->wsms_lock);
	if (ret < 0)
		return ret;

	return 0;
}
