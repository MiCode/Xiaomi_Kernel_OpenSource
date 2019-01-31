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
#include <linux/fs.h>
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

/*
 * WSM state transition:
 *  * EMPTY	->	NEW	: MMU created [wsm_create]
 *  * NEW	->	ACTIVE	: buffer mapped to SWd and returned to client
 *  * ACTIVE	->	INACTIVE: "unmapped" by client, but not from SWd
 *  * INACTIVE	->	PENDING	: identified for re-use
 *  * NEW	->	EMPTY	: map failed or space needed or close [wsm_free]
 *  * INACTIVE	->	NEW	: unmapped from SWd, to make space
 *  * PENDING	->	INACTIVE: map of other buffer failed
 */

/* Cleanup for GP TAs, implemented as a worker to not impact other sessions */
static void session_close_worker(struct work_struct *work)
{
	struct mcp_session *mcp_session;
	struct tee_session *session;

	mcp_session = container_of(work, struct mcp_session, close_work);
	mc_dev_devel("session %x worker", mcp_session->id);
	session = container_of(mcp_session, struct tee_session, mcp_session);
	if (!mcp_close_session(mcp_session))
		complete(&session->close_completion);
}

static inline const char *wsm_state_str(enum tee_wsm_state state)
{
	switch (state) {
	case TEE_WSM_EMPTY:
		return "empty";
	case TEE_WSM_NEW:
		return "new";
	case TEE_WSM_ACTIVE:
		return "active";
	case TEE_WSM_INACTIVE:
		return "inactive";
	case TEE_WSM_PENDING:
		return "pending";
	case TEE_WSM_OBSOLETE:
		return "obsolete";
	}
	return "invalid";
}

static int wsm_create(struct tee_session *session, struct tee_wsm *wsm,
		      struct mc_ioctl_buffer *buf)
{
	if (wsm->state != TEE_WSM_EMPTY) {
		mc_dev_notice("invalid wsm state %s\n",
			wsm_state_str(wsm->state));
		return -EINVAL;
	}

	/* Mapping of other application buffer is force-disabled for now */
	session->flags &= ~MC_IO_SESSION_REMOTE_BUFFERS;
	wsm->mmu = client_mmu_create(session->client, session->pid,
				     session->flags, buf->va, buf->len,
				     &wsm->cbuf);
	if (IS_ERR(wsm->mmu))
		return PTR_ERR(wsm->mmu);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_wsms);
	wsm->va = buf->va;
	wsm->len = buf->len;
	wsm->flags = buf->flags;
	wsm->state = TEE_WSM_NEW;
	return 0;
}

static inline bool wsm_matches(struct tee_session *session, struct tee_wsm *wsm,
			       struct mc_ioctl_buffer *buf)
{
	struct tee_mmu *mmu;
	bool matches;

	if (wsm->state != TEE_WSM_INACTIVE) {
		mc_dev_notice("invalid wsm state %s\n",
			wsm_state_str(wsm->state));
		return false;
	}

	if ((buf->va != wsm->va) || (buf->len != wsm->len))
		return false;

	mmu = client_mmu_create(session->client, session->pid, session->flags,
				buf->va, buf->len, &wsm->cbuf);
	if (IS_ERR(mmu))
		return false;

	/* Compare MMUs and this backing physical addresses */
	matches = client_mmu_matches(wsm->mmu, mmu);
	client_mmu_free(session->client, wsm->va, mmu, wsm->cbuf);
	return matches;
}

/*
 * Free a WSM object, must be called under the session's wsms_lock
 */
static void wsm_free(struct tee_session *session, struct tee_wsm *wsm)
{
	if (wsm->state != TEE_WSM_NEW) {
		mc_dev_notice("invalid wsm state %s\n",
			wsm_state_str(wsm->state));
		return;
	}

	/* Free MMU table */
	client_mmu_free(session->client, wsm->va, wsm->mmu, wsm->cbuf);
	/* Delete wsm object */
	mc_dev_devel("freed wsm %p: mmu %p cbuf %p va %lx len %u sva %x\n",
		     wsm, wsm->mmu, wsm->cbuf, wsm->va, wsm->len, wsm->sva);
	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_wsms);
	wsm->state = TEE_WSM_EMPTY;
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

	mc_dev_devel("process path =\n");
	{
		char *c;

		for (c = path; *c; c++)
			mc_dev_devel("%c %d\n", *c, *c);
	}

	path_len = (unsigned int)strnlen(path, PAGE_SIZE);
	mc_dev_devel("path_len = %u\n", path_len);
	/* Compute hash of path */
	desc.tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm)) {
		ret = PTR_ERR(desc.tfm);
		mc_dev_devel("could not alloc hash = %d\n", ret);
		goto end;
	}

	desc.flags = 0;
	sg_init_one(&sg, path, path_len);
	crypto_hash_init(&desc);
	crypto_hash_update(&desc, &sg, path_len);
	if (data) {
		mc_dev_devel("current process path: hashing additional data\n");
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
				  struct identity *mcp_identity, pid_t pid)
{
	struct mc_identity *mcp_id = (struct mc_identity *)mcp_identity;
	u8 hash[SHA1_HASH_SIZE];
	bool application = false;
	const void *data;
	unsigned int data_len;
	struct task_struct *task;

	/* Copy login type */
	mcp_identity->login_type = identity->login_type;

	if ((identity->login_type == LOGIN_PUBLIC) ||
	    (identity->login_type == TEEC_LOGIN_KERNEL))
		return 0;

	/* Mobicore doesn't support GP client authentication. */
	if (!g_ctx.f_client_login) {
		mc_dev_notice("Unsupported login type %x\n",
			identity->login_type);
		return -EINVAL;
	}

	/* Only proxy can provide a PID */
	if (pid) {
		rcu_read_lock();
		task = pid_task(find_vpid(pid), PIDTYPE_PID);
		if (!task) {
			rcu_read_unlock();
			mc_dev_notice("No task for PID %d\n", pid);
			return -EINVAL;
		}
	} else {
		rcu_read_lock();
		task = current;
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
			rcu_read_unlock();
			mc_dev_notice("group %d not allowed\n", identity->gid);
			return -EACCES;
		}

		mc_dev_devel("group %d found\n", identity->gid);
		mcp_id->gid = identity->gid;
	}
	rcu_read_unlock();

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
		mc_dev_notice("Invalid login type %d\n", identity->login_type);
		return -EINVAL;
	}

	if (application) {
		int ret = hash_path_and_data(task, hash, data, data_len);

		if (ret) {
			mc_dev_devel("hash calculation returned %d\n", ret);
			return ret;
		}

		memcpy(&mcp_id->login_data, hash, sizeof(mcp_id->login_data));
	}

	return 0;
}

/*
 * Create a session object.
 * Note: object is not attached to client yet.
 */
struct tee_session *session_create(struct tee_client *client, bool is_gp,
				   struct mc_identity *identity, pid_t pid,
				   u32 flags)
{
	struct tee_session *session;
	struct identity mcp_identity;
	int i;

	/* Only proxy can provide a PID (Android system user) */
	if (pid) {
#ifndef CONFIG_ANDROID
		mc_dev_notice("Cannot provide PID\n");
		return ERR_PTR(-EPERM);
#else
		uid_t euid = __kuid_val(task_euid(current));

		if (euid != 1000) {
			mc_dev_notice("incorrect EUID %d for PID %d\n", euid,
				   current->tgid);
			return ERR_PTR(-EPERM);
		}
#endif
	}

	if (is_gp) {
		/* Check identity method and data. */
		int ret = check_prepare_identity(identity, &mcp_identity, pid);

		if (ret)
			return ERR_PTR(ret);
	}

	/* Allocate session object */
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return ERR_PTR(-ENOMEM);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_sessions);
	mutex_init(&session->close_lock);
	init_completion(&session->close_completion);
	/* Initialise object members */
	mcp_session_init(&session->mcp_session, is_gp, &mcp_identity);
	INIT_WORK(&session->mcp_session.close_work, session_close_worker);
	client_get(client);
	session->client = client;
	kref_init(&session->kref);
	INIT_LIST_HEAD(&session->list);
	mutex_init(&session->wsms_lock);
	session->pid = pid;
	for (i = 0; i < MC_MAP_MAX; i++)
		session->wsms_lru[i] = &session->wsms[i];
	session->flags = flags;
	mc_dev_devel("created session %p: client %p\n",
		     session, session->client);
	return session;
}

int session_open(struct tee_session *session, const struct tee_object *obj,
		 const struct tee_mmu *obj_mmu, uintptr_t tci, size_t len)
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
		ret = wsm_create(session, wsm, &buf);
		if (ret)
			return ret;

		mc_dev_devel("created tci: mmu %p cbuf %p va %lx len %u\n",
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
		mc_dev_devel("Tci pointer and length are incoherent\n");
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
int session_close(struct tee_session *session)
{
	int ret = 0;

	mutex_lock(&session->close_lock);
	switch (mcp_close_session(&session->mcp_session)) {
	case 0:
		break;
	case -EAGAIN:
		/*
		 * GP TAs need time to close. The "TA closed" notification shall
		 * trigger the session_close_worker which will unblock us
		 */
		mc_dev_devel("wait for session %x worker\n",
			     session->mcp_session.id);
		wait_for_completion(&session->close_completion);
		break;
	default:
		mc_dev_devel("failed to close session %x in SWd\n",
			     session->mcp_session.id);
		ret = -EPERM;
	}
	mutex_unlock(&session->close_lock);

	if (ret)
		return ret;

	mc_dev_devel("closed session %x\n", session->mcp_session.id);
	/* Remove session from client's closing list */
	mutex_lock(&session->client->sessions_lock);
	list_del(&session->list);
	mutex_unlock(&session->client->sessions_lock);

	/* Remove the ref we took on creation */
	session_put(session);
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
		if (session->wsms[i].state < TEE_WSM_ACTIVE)
			continue;

		mc_dev_devel("session %p: free wsm #%d\n", session, i);
		/* Assume WSM unmapped by SWd */
		session->wsms[i].state = TEE_WSM_NEW;
		wsm_free(session, &session->wsms[i]);
		atomic_dec(&g_ctx.c_maps);
	}

	if (session->tci.state != TEE_WSM_EMPTY) {
		mc_dev_devel("session %p: free tci\n", session);
		wsm_free(session, &session->tci);
	}

	mc_dev_devel("freed session %p: client %p id %x\n",
		     session, session->client, session->mcp_session.id);
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
		mc_dev_devel("Session pointer is null\n");
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

	if (wsm->state == TEE_WSM_EMPTY)
		return 0;

	ret = kasnprintf(buf, "\t\t");
	if (no < 0)
		ret = kasnprintf(buf, "tci %p: cbuf %p va %lx len %u\n",
				 wsm, wsm->cbuf, wsm->va, wsm->len);
	else if (wsm->state > TEE_WSM_EMPTY)
		ret = kasnprintf(buf,
				 "wsm #%d: cbuf %p va %lx len %u sva %x %s\n",
				 no, wsm->cbuf, wsm->va, wsm->len, wsm->sva,
				 wsm_state_str(wsm->state));

	if (ret < 0)
		return ret;

	if (wsm->mmu) {
		ret = tee_mmu_debug_structs(buf, wsm->mmu);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int session_remap(struct tee_session *session, int need_freeing,
			 struct mcp_buffer_map *new_maps, bool use_multimap)
{
	/* Free some first inactive buffers in array */
	struct mcp_buffer_map maps[MC_MAP_MAX];
	int i, ret, to_unmap = 0;

	if (need_freeing)
		mc_dev_devel("need to unmap %d inactive wsms\n", need_freeing);

	/* Find inactive mappings */
	memset(maps, 0, sizeof(maps));
	for (i = 0; i < MC_MAP_MAX; i++) {
		struct tee_wsm *wsm = session->wsms_lru[i];

		/* Only inactive and obsolete buffers can be unmapped */
		switch (wsm->state) {
		case TEE_WSM_OBSOLETE:
			break;
		case TEE_WSM_INACTIVE:
			/* Only unmap inactive buffers if required */
			if (!need_freeing)
				continue;

			need_freeing--;
			break;
		default:
			continue;
		}

		wsm->state = TEE_WSM_NEW;
		tee_mmu_buffer(wsm->mmu, &maps[i]);
		maps[i].secure_va = wsm->sva;
		to_unmap++;
		mc_dev_devel(
			"maps[%d] rm va=%lx: t:%u a:%llx o:%u l:%u s:%llx\n",
			i, wsm->va, maps[i].type, maps[i].phys_addr,
			maps[i].offset, maps[i].length, maps[i].secure_va);
	}

	if (to_unmap) {
		mc_dev_devel("unmap %d wsms\n", to_unmap);
		ret = mcp_multiunmap(session->mcp_session.id, maps,
				     use_multimap);
		if (ret)
			return ret;

		/* Remove WSMs from session */
		for (i = 0; i < MC_MAP_MAX; i++) {
			struct tee_wsm *wsm = &session->wsms[i];

			if (wsm->state == TEE_WSM_NEW)
				/* Free wsm */
				wsm_free(session, &session->wsms[i]);
		}
	}

	/* Nothing to map */
	if (!new_maps)
		return 0;

	/* Map buffers */
	return mcp_multimap(session->mcp_session.id, new_maps, use_multimap);
}

/*
 * Share buffers with SWd and add corresponding WSM objects to session.
 * This may involve some re-use or cleanup of inactive mappings.
 */
int session_map(struct tee_session *session, struct mc_ioctl_buffer *bufs)
{
	struct tee_wsm wsms[MC_MAP_MAX];
	/*
	 * Array with same index as bufs, to send the buffers information to MCP
	 */
	struct mcp_buffer_map new_maps[MC_MAP_MAX];
	int i, bi, si, mapped = 0, active = 0, new = 0, ret = 0;
	const bool use_multimap = !(session->flags & MC_IO_SESSION_NO_MULTIMAP);

	memset(wsms, 0, sizeof(wsms));

	mutex_lock(&session->wsms_lock);

	/* Count mapped buffers and new ones, initilise temporary array */
	for (i = 0; i < MC_MAP_MAX; i++) {
		if (bufs[i].va)
			new++;

		if (session->wsms[i].state >= TEE_WSM_ACTIVE) {
			mapped++;
			if (session->wsms[i].state == TEE_WSM_ACTIVE)
				active++;
		}

		wsms[i].state = TEE_WSM_EMPTY;
	}

	mc_dev_devel("maps: mapped=%d active=%d new=%d\n", mapped, active, new);
	if (!new) {
		mc_dev_devel("no buffers to map\n");
		ret = -EINVAL;
		goto out;
	}

	/* Only four buffers maybe mapped at a single time */
	if ((active + new) > MC_MAP_MAX) {
		mc_dev_devel("too many buffers to map: %d\n", active + new);
		ret = -EPERM;
		goto out;
	}

	/* Re-use WSM or create MMU and map for each buffer */
	for (bi = 0; bi < MC_MAP_MAX; bi++) {
		/* No buffer at this index */
		if (!bufs[bi].va) {
			new_maps[bi].type = WSM_INVALID;
			continue;
		}

		/* Try to re-use */
		for (si = 0; si < MC_MAP_MAX; si++) {
			struct tee_wsm *wsm = &session->wsms[si];

			if (wsm->state != TEE_WSM_INACTIVE)
				continue;

			if (wsm_matches(session, wsm, &bufs[bi])) {
				wsm->state = TEE_WSM_PENDING;
				/* Use wsms[bi] to reference re-used WSM */
				wsms[bi].state = TEE_WSM_PENDING;
				wsms[bi].index = si;
				/* One less new buffer to map */
				new--;
				mc_dev_devel("maps[%d] va=%llx l=%u re-used\n",
					     bi, bufs[bi].va, bufs[bi].len);
				break;
			}
		}

		/* No need to map */
		if (wsms[bi].state == TEE_WSM_PENDING) {
			new_maps[bi].type = WSM_INVALID;
			continue;
		}

		/* Create new */
		ret = wsm_create(session, &wsms[bi], &bufs[bi]);
		if (ret) {
			mc_dev_devel("maps[%d] va=%llx create failed: %d\n",
				     bi, bufs[bi].va, ret);
			goto err;
		}

		mc_dev_devel("created wsm #%d: mmu %p cbuf %p va %lx len %u\n",
			     bi, wsms[bi].mmu, wsms[bi].cbuf, wsms[bi].va,
			     wsms[bi].len);
		tee_mmu_buffer(wsms[bi].mmu, &new_maps[bi]);
		mc_dev_devel("maps[%d] va=%llx: t:%u a:%llx o:%u l:%u\n",
			     bi, bufs[bi].va, new_maps[bi].type,
			     new_maps[bi].phys_addr, new_maps[bi].offset,
			     new_maps[bi].length);
	}

	/* Make space and map new buffers */
	ret = session_remap(session, (mapped + new) > MC_MAP_MAX ?
				(mapped + new) - MC_MAP_MAX : 0,
			    new_maps, use_multimap);
	if (ret)
		goto err;

	/* Update data */
	for (bi = 0; bi < MC_MAP_MAX; bi++) {
		switch (wsms[bi].state) {
		case TEE_WSM_EMPTY:
			/* Not used */
			continue;
		case TEE_WSM_NEW:
			/* Newly mapped */
			wsms[bi].state = TEE_WSM_ACTIVE;
			wsms[bi].sva = new_maps[bi].secure_va;
			bufs[bi].sva = wsms[bi].sva;

			/* Store WSM into session */
			for (si = 0; si < MC_MAP_MAX; si++) {
				if (session->wsms[si].state == TEE_WSM_EMPTY) {
					session->wsms[si] = wsms[bi];
					break;
				}
			}
			break;
		case TEE_WSM_PENDING: {
			/* Re-used */
			struct tee_wsm *wsm = &session->wsms[wsms[bi].index];

			wsm->state = TEE_WSM_ACTIVE;
			bufs[bi].sva = wsm->sva;
			} break;
		default:
			mc_dev_notice("unexpected temporary WSM state %s",
				   wsm_state_str(wsms[bi].state));
			goto err;
		}

		mc_dev_devel("maps[%d] va=%llx len=%u sva=%llx (%s)\n",
			     bi, bufs[bi].va, bufs[bi].len, bufs[bi].sva,
			     wsms[bi].state == TEE_WSM_ACTIVE ?
				"new" : "re-used");
	}

	goto out;

err:
	/* Cleanup */
	for (i = 0; i < MC_MAP_MAX; i++) {
		/* Bring session WSMs back to their previous states */
		struct tee_wsm *wsm = &session->wsms[i];

		switch (wsm->state) {
		case TEE_WSM_NEW:
			wsm_free(session, wsm);
			break;
		case TEE_WSM_PENDING:
			wsm->state = TEE_WSM_INACTIVE;
			break;
		default:
			break;
		}

		/* Free any newly created WSM */
		if (wsms[i].state == TEE_WSM_NEW)
			wsm_free(session, &wsms[i]);
	}

out:
	mutex_unlock(&session->wsms_lock);
	mc_dev_devel("ret=%d\n", ret);
	return ret;
}

/*
 * In theory, stop sharing buffers with the SWd. In fact, mark them inactive.
 */
int session_unmap(struct tee_session *session,
		  const struct mc_ioctl_buffer *bufs)
{
	int bi, ret = 0;
	bool at_least_one = false;
	const bool use_multimap = !(session->flags & MC_IO_SESSION_NO_MULTIMAP);

	mutex_lock(&session->wsms_lock);

	/* Find, check and map buffer */
	for (bi = 0; bi < MC_MAP_MAX; bi++) {
		struct tee_wsm *wsm;
		int si, li;
		bool lru_found = false;

		if (!bufs[bi].va)
			continue;

		/* Find corresponding mapping */
		for (si = 0; si < MC_MAP_MAX; si++) {
			wsm = &session->wsms[si];

			if ((wsm->state == TEE_WSM_ACTIVE) &&
			    (wsm->sva == bufs[bi].sva))
				break;
		}

		/* Not found */
		if (si == MC_MAP_MAX) {
			ret = -EINVAL;
			mc_dev_devel("maps[%d] va=%llx sva=%llx not found\n",
				     bi, bufs[bi].va, bufs[bi].sva);
			goto out;
		}

		/* Check VA */
		if (wsm->va != bufs[bi].va) {
			ret = -EINVAL;
			mc_dev_devel("maps[%d] va=%llx does not match %lx\n",
				     bi, bufs[bi].va, wsm->va);
			goto out;
		}

		/* Check length */
		if (wsm->len != bufs[bi].len) {
			ret = -EINVAL;
			mc_dev_devel("maps[%d] va=%llx len mismatch: %u != %u\n"
				     , bi, bufs[bi].va, wsm->len, bufs[bi].len);
			goto out;
		}

		/* Decide next state based on flags */
		if (wsm->flags & MC_IO_MAP_PERSISTENT) {
			wsm->state = TEE_WSM_INACTIVE;

			/* Update LRU array */
			if (session->wsms_lru[MC_MAP_MAX - 1] != wsm) {
				for (li = 0; li < MC_MAP_MAX; li++) {
					if (session->wsms_lru[li] == wsm)
						lru_found = true;
					else if (lru_found)
						session->wsms_lru[li - 1] =
							session->wsms_lru[li];
				}
				session->wsms_lru[MC_MAP_MAX - 1] = wsm;
			}
		} else {
			wsm->state = TEE_WSM_OBSOLETE;
		}

		at_least_one = true;
	}

	if (!at_least_one) {
		ret = -EINVAL;
		mc_dev_devel("no buffers to unmap\n");
		goto out;
	}

	/* Unmap obsolete buffers */
	ret = session_remap(session, 0, NULL, use_multimap);
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

int session_debug_structs(struct kasnprintf_buf *buf,
			  struct tee_session *session, bool is_closing)
{
	s32 exit_code;
	int i, ret;

	exit_code = mcp_session_exitcode(&session->mcp_session);
	ret = kasnprintf(buf, "\tsession %p [%d]: %x %s PID %d ec %d%s\n",
			 session, kref_read(&session->kref),
			 session->mcp_session.id,
			 session->mcp_session.is_gp ? "GP" : "MC", session->pid,
			 exit_code, is_closing ? " <closing>" : "");
	if (ret < 0)
		return ret;

	/* TCI */
	if (session->tci.state != TEE_WSM_EMPTY) {
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
