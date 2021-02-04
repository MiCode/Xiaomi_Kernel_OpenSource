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

#include "public/mc_user.h"
#include "public/mc_admin.h"

#include <linux/uidgid.h>
#include "main.h"
#include "mmu.h"		/* tee_mmu_buffer, tee_mmu_debug_structs */
#include "iwp.h"
#include "mcp.h"
#include "client.h"		/* *cbuf* */
#include "session.h"
#include "mci/mcimcp.h"		/* WSM_INVALID */

#define SHA1_HASH_SIZE       20

static int wsm_create(struct tee_session *session, struct tee_wsm *wsm,
		      const struct mc_ioctl_buffer *buf)
{
	if (wsm->in_use) {
		mc_dev_err(-EINVAL, "wsm already in use");
		return -EINVAL;
	}

	if (buf->len > BUFFER_LENGTH_MAX) {
		mc_dev_err(-EINVAL, "buffer size %u too big", buf->len);
		return -EINVAL;
	}

	wsm->mmu = client_mmu_create(session->client, buf, &wsm->cbuf);
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

static int wsm_wrap(struct tee_session *session, struct tee_wsm *wsm,
		    struct tee_mmu *mmu)
{
	struct mcp_buffer_map map;

	if (wsm->in_use) {
		mc_dev_err(-EINVAL, "wsm already in use");
		return -EINVAL;
	}

	wsm->mmu = mmu;
	tee_mmu_get(wsm->mmu);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_wsms);
	tee_mmu_buffer(wsm->mmu, &map);
	wsm->va = 0;
	wsm->len = map.length;
	wsm->flags = map.flags;
	wsm->in_use = true;
	return 0;
}

/*
 * Free a WSM object, must be called under the session's wsms_lock
 */
static void wsm_free(struct tee_session *session, struct tee_wsm *wsm)
{
	if (!wsm->in_use) {
		mc_dev_err(-EINVAL, "wsm not in use");
		return;
	}

	mc_dev_devel("free wsm %p: mmu %p cbuf %p va %lx len %u sva %x",
		     wsm, wsm->mmu, wsm->cbuf, wsm->va, wsm->len, wsm->sva);
	/* Free MMU table */
	tee_mmu_put(wsm->mmu);
	if (wsm->cbuf)
		tee_cbuf_put(wsm->cbuf);

	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_wsms);
	wsm->in_use = false;
}

static int hash_path_and_data(struct task_struct *task, u8 *hash,
			      const void *data, unsigned int data_len)
{
	struct file *exe_file;
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	size_t desc_size;
	char *buf;
	char *path;
	unsigned int path_len;
	int ret = 0;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	exe_file = get_task_exe_file(task);
	if (!exe_file) {
		ret = -ENOENT;
		goto end;
	}

	path = d_path(&exe_file->f_path, buf, PAGE_SIZE);
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
		mc_dev_err(ret, "cannot allocate shash");
		goto end;
	}

	desc_size = crypto_shash_descsize(tfm) + sizeof(*desc);
	desc = kzalloc(desc_size, GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto err_desc;
	}

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
	kfree(desc);
err_desc:
	crypto_free_shash(tfm);
end:
	free_page((unsigned long)buf);

	return ret;
}

#define GROUP_AT(gi, i) ((gi)->gid[i])

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
	u8 hash[SHA1_HASH_SIZE] = { 0 };
	bool application = false;
	bool supplied_ca_identity = false;
	const void *data;
	unsigned int data_len;
	static const u8 zero_buffer[sizeof(identity->login_data)] = { 0 };

	/* Copy login type */
	mcp_identity->login_type = identity->login_type;

	if (identity->login_type == LOGIN_PUBLIC ||
	    identity->login_type == TEEC_TT_LOGIN_KERNEL)
		return 0;

	/* Fill in uid field */
	if (identity->login_type == LOGIN_USER ||
	    identity->login_type == LOGIN_USER_APPLICATION) {
		/* Set euid and ruid of the process. */
		mcp_id->uid.euid = __kuid_val(task_euid(task));
		mcp_id->uid.ruid = __kuid_val(task_uid(task));
	}

	/* Check gid field */
	if (identity->login_type == LOGIN_GROUP ||
	    identity->login_type == LOGIN_GROUP_APPLICATION) {
		const struct cred *cred = __task_cred(task);

		/*
		 * Check if gid is one of: egid of the process, its rgid or one
		 * of its supplementary groups
		 */
		if (!has_group(cred, identity->gid)) {
			mc_dev_err(-EACCES, "group %d not allowed",
				   identity->gid);
			return -EACCES;
		}

		mc_dev_devel("group %d found", identity->gid);
		mcp_id->gid = identity->gid;
	}

	switch (identity->login_type) {
	case LOGIN_PUBLIC:
	case LOGIN_GROUP:
		break;
	case LOGIN_USER:
		data = NULL;
		data_len = 0;
		break;
	case LOGIN_APPLICATION:
		application = true;
		supplied_ca_identity = true;
		data = NULL;
		data_len = 0;
		break;
	case LOGIN_USER_APPLICATION:
		application = true;
		supplied_ca_identity = true;
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
		mc_dev_err(-EINVAL, "Invalid login type %d",
			   identity->login_type);
		return -EINVAL;
	}

	/* let the supplied login_data pass through if it is LOGIN_APPLICATION
	 * or LOGIN_USER_APPLICATION and not a zero-filled buffer
	 * That buffer is expected to contain a NWd computed hash containing the
	 * CA identity
	 */
	if (supplied_ca_identity &&
	    memcmp(identity->login_data, zero_buffer,
		   sizeof(identity->login_data)) != 0) {
		memcpy(&mcp_id->login_data, identity->login_data,
		       sizeof(mcp_id->login_data));
	} else if (application) {
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
 * Create a session object.
 * Note: object is not attached to client yet.
 */
struct tee_session *session_create(struct tee_client *client,
				   const struct mc_identity *identity)
{
	struct tee_session *session;
	struct identity mcp_identity;

	if (!IS_ERR_OR_NULL(identity)) {
		/* Check identity method and data. */
		int ret;

		ret = check_prepare_identity(identity, &mcp_identity, current);
		if (ret)
			return ERR_PTR(ret);
	} else
		memset(&mcp_identity, 0, sizeof(mcp_identity));

	/* Allocate session object */
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return ERR_PTR(-ENOMEM);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_sessions);
	/* Initialise object members */
	if (identity) {
		session->is_gp = true;
		iwp_session_init(&session->iwp_session, &mcp_identity);
	} else {
		session->is_gp = false;
		mcp_session_init(&session->mcp_session);
	}

	client_get(client);
	session->client = client;
	kref_init(&session->kref);
	INIT_LIST_HEAD(&session->list);
	mutex_init(&session->wsms_lock);
	mc_dev_devel("created session %p: client %p",
		     session, session->client);
	return session;
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

	if (session->is_gp)
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

static int wsm_debug_structs(struct kasnprintf_buf *buf, struct tee_wsm *wsm,
			     int no)
{
	ssize_t ret;

	if (!wsm->in_use)
		return 0;

	ret = kasnprintf(buf, "\t\t");
	if (no < 0)
		ret = kasnprintf(buf, "tci %pK: cbuf %pK va %pK len %u\n",
				 wsm, wsm->cbuf, (void *)wsm->va, wsm->len);
	else if (wsm->in_use)
		ret = kasnprintf(buf,
				 "wsm #%d: cbuf %pK va %pK len %u sva %x\n",
				 no, wsm->cbuf, (void *)wsm->va, wsm->len,
				 wsm->sva);

	if (ret < 0)
		return ret;

	if (wsm->mmu) {
		ret = tee_mmu_debug_structs(buf, wsm->mmu);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int session_mc_open_session(struct tee_session *session,
			    struct mcp_open_info *info)
{
	struct tee_wsm *wsm = &session->tci;
	bool tci_in_use = false;
	int ret;

	/* Check that tci and its length make sense */
	if (info->tci_len > MC_MAX_TCI_LEN)
		return -EINVAL;

	if (!info->tci_va != !info->tci_len) {
		mc_dev_devel("TCI pointer and length are inconsistent");
		return -EINVAL;
	}

	/* Add existing TCI map */
	if (info->tci_mmu) {
		ret = wsm_wrap(session, wsm, info->tci_mmu);
		if (ret)
			return ret;

		tci_in_use = true;
		mc_dev_devel("wrapped tci: mmu %p len %u flags %x",
			     wsm->mmu, wsm->len, wsm->flags);
	}

	/* Create mapping for TCI */
	if (info->tci_va) {
		struct mc_ioctl_buffer buf = {
			.va = info->tci_va,
			.len = info->tci_len,
			.flags = MC_IO_MAP_INPUT_OUTPUT,
		};

		ret = wsm_create(session, wsm, &buf);
		if (ret)
			return ret;

		tci_in_use = true;
		info->tci_mmu = wsm->mmu;
		mc_dev_devel(
			"created tci: mmu %p cbuf %p va %lx len %u flags %x",
			wsm->mmu, wsm->cbuf, wsm->va, wsm->len, wsm->flags);
	}

	ret = mcp_open_session(&session->mcp_session, info, &tci_in_use);
	if (info->tci_va && (ret || !tci_in_use))
		wsm_free(session, &session->tci);

	return ret;
}

/*
 * Close session and unreference session object.
 * Session object is assumed to have been removed from main list, which means
 * that session_close cannot be called anymore.
 */
int session_close(struct tee_session *session)
{
	int ret;

	if (session->is_gp) {
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
 * Session is to be removed from NWd records as SWd is dead
 */
int session_mc_cleanup_session(struct tee_session *session)
{
	mcp_cleanup_session(&session->mcp_session);
	return session_put(session);
}

/*
 * Send a notification to TA
 */
int session_mc_notify(struct tee_session *session)
{
	if (!session) {
		mc_dev_devel("Session pointer is null");
		return -EINVAL;
	}

	return mcp_notify(&session->mcp_session);
}

/*
 * Sleep until next notification from SWd.
 */
int session_mc_wait(struct tee_session *session, s32 timeout,
		    bool silent_expiry)
{
	return mcp_wait(&session->mcp_session, timeout, silent_expiry);
}

/*
 * Share buffers with SWd and add corresponding WSM objects to session.
 * This may involve some re-use or cleanup of inactive mappings.
 */
int session_mc_map(struct tee_session *session, struct tee_mmu *mmu,
		   struct mc_ioctl_buffer *buf)
{
	struct tee_wsm *wsm;
	u32 sva;
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
	if (!mmu)
		ret = wsm_create(session, wsm, buf);
	else
		ret = wsm_wrap(session, wsm, mmu);

	if (ret) {
		mc_dev_devel("maps[%d] va=%llx create failed: %d",
			     i, buf->va, ret);
		goto out;
	}

	mc_dev_devel("created wsm #%d: mmu %p cbuf %p va %lx len %u flags %x",
		     i, wsm->mmu, wsm->cbuf, wsm->va, wsm->len, wsm->flags);
	ret = mcp_map(session->mcp_session.sid, wsm->mmu, &sva);
	if (ret) {
		wsm_free(session, wsm);
	} else {
		buf->sva = sva;
		wsm->sva = sva;
	}

out:
	mutex_unlock(&session->wsms_lock);
	mc_dev_devel("ret=%d", ret);
	return ret;
}

/*
 * In theory, stop sharing buffers with the SWd. In fact, mark them inactive.
 */
int session_mc_unmap(struct tee_session *session,
		     const struct mc_ioctl_buffer *buf)
{
	struct tee_wsm *wsm;
	struct mcp_buffer_map map;
	int i, ret = -EINVAL;

	mutex_lock(&session->wsms_lock);
	/* Look for buffer in the session WSMs array */
	for (i = 0; i < MC_MAP_MAX; i++)
		if (session->wsms[i].in_use &&
		    buf->va == session->wsms[i].va &&
		    buf->len == session->wsms[i].len &&
		    buf->sva == session->wsms[i].sva)
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
 * Read and clear last notification received from TA
 */
int session_mc_get_err(struct tee_session *session, s32 *err)
{
	return mcp_get_err(&session->mcp_session, err);
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
		       struct iwp_buffer_map *maps)
{
	int i, ret = 0;

	/* Create WSMs from bufs */
	mutex_lock(&session->wsms_lock);
	for (i = 0; i < MC_MAP_MAX; i++) {
		/* Reset reference for temporary memory */
		maps[i].map.addr = 0;
		/* Reset reference for registered memory */
		maps[i].sva = 0;
		if (bufs[i].va) {
			/* Temporary memory, needs mapping */
			ret = wsm_create(session, &session->wsms[i], &bufs[i]);
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

			mc_dev_devel("param[%d] has sva %x", i, maps[i].sva);
		}
	}
	mutex_unlock(&session->wsms_lock);

	/* Failed above */
	if (i < MC_MAP_MAX)
		unmap_gp_bufs(session, maps);

	return ret;
}

int session_gp_open_session(struct tee_session *session,
			    const struct mc_uuid_t *uuid,
			    struct gp_operation *operation,
			    struct gp_return *gp_ret)
{
	/* TEEC_MEMREF_TEMP_* buffers to map */
	struct mc_ioctl_buffer bufs[MC_MAP_MAX];
	struct iwp_buffer_map maps[MC_MAP_MAX];
	struct gp_shared_memory *parents[MC_MAP_MAX] = { NULL };
	struct client_gp_operation client_operation;
	int ret = 0;

	ret = iwp_open_session_prepare(&session->iwp_session, operation, bufs,
				       parents, gp_ret);
	if (ret)
		return ret;

	/* Create WSMs from bufs */
	ret = map_gp_bufs(session, bufs, parents, maps);
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
	ret = iwp_open_session(&session->iwp_session, uuid, operation, maps,
			       NULL, NULL, gp_ret);
	/* Cleanup */
	client_gp_operation_remove(session->client, &client_operation);
	unmap_gp_bufs(session, maps);
	return ret;
}

int session_gp_open_session_domu(struct tee_session *session,
				 const struct mc_uuid_t *uuid, u64 started,
				 struct interworld_session *iws,
				 struct tee_mmu **mmus,
				 struct gp_return *gp_ret)
{
	/* TEEC_MEMREF_TEMP_* buffers to map */
	struct client_gp_operation client_operation;
	int ret = 0;

	ret = iwp_open_session_prepare(&session->iwp_session, NULL, NULL, NULL,
				       gp_ret);
	if (ret)
		return ret;

	/* Tell client about operation */
	client_operation.started = started;
	client_operation.slot = iwp_session_slot(&session->iwp_session);
	client_operation.cancelled = false;
	if (!client_gp_operation_add(session->client, &client_operation)) {
		iwp_open_session_abort(&session->iwp_session);
		return iwp_set_ret(-ECANCELED, gp_ret);
	}

	/* Open/call TA */
	ret = iwp_open_session(&session->iwp_session, uuid, NULL, NULL, iws,
			       mmus, gp_ret);
	/* Cleanup */
	client_gp_operation_remove(session->client, &client_operation);
	return ret;
}

int session_gp_invoke_command(struct tee_session *session, u32 command_id,
			      struct gp_operation *operation,
			      struct gp_return *gp_ret)
{
	/* TEEC_MEMREF_TEMP_* buffers to map */
	struct mc_ioctl_buffer bufs[4];
	struct iwp_buffer_map maps[MC_MAP_MAX];
	struct gp_shared_memory *parents[MC_MAP_MAX] = { NULL };
	struct client_gp_operation client_operation;
	int ret = 0;

	ret = iwp_invoke_command_prepare(&session->iwp_session, command_id,
					 operation, bufs, parents, gp_ret);
	if (ret)
		return ret;

	/* Create WSMs from bufs */
	ret = map_gp_bufs(session, bufs, parents, maps);
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
	ret = iwp_invoke_command(&session->iwp_session, operation, maps, NULL,
				 NULL, gp_ret);
	/* Cleanup */
	client_gp_operation_remove(session->client, &client_operation);
	unmap_gp_bufs(session, maps);
	return ret;
}

int session_gp_invoke_command_domu(struct tee_session *session,
				   u64 started, struct interworld_session *iws,
				   struct tee_mmu **mmus,
				   struct gp_return *gp_ret)
{
	struct client_gp_operation client_operation;
	int ret = 0;

	ret = iwp_invoke_command_prepare(&session->iwp_session, 0, NULL, NULL,
					 NULL, gp_ret);
	if (ret)
		return ret;

	/* Tell client about operation */
	client_operation.started = started;
	client_operation.slot = iwp_session_slot(&session->iwp_session);
	client_operation.cancelled = false;
	if (!client_gp_operation_add(session->client, &client_operation)) {
		iwp_invoke_command_abort(&session->iwp_session);
		return iwp_set_ret(-ECANCELED, gp_ret);
	}

	/* Call TA */
	ret = iwp_invoke_command(&session->iwp_session, NULL, NULL, iws, mmus,
				 gp_ret);
	/* Cleanup */
	client_gp_operation_remove(session->client, &client_operation);
	return ret;
}

int session_gp_request_cancellation(u64 slot)
{
	return iwp_request_cancellation(slot);
}

int session_debug_structs(struct kasnprintf_buf *buf,
			  struct tee_session *session, bool is_closing)
{
	const char *type;
	u32 session_id;
	s32 err;
	int i, ret;

	if (session->is_gp) {
		session_id = session->iwp_session.sid;
		err = 0;
		type = "GP";
	} else {
		session_id = session->mcp_session.sid;
		session_mc_get_err(session, &err);
		type = "MC";
	}

	ret = kasnprintf(buf, "\tsession %pK [%d]: %4x %s ec %d%s\n",
			 session, kref_read(&session->kref), session_id, type,
			 err, is_closing ? " <closing>" : "");
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
