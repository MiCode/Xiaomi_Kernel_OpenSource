// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2020 TRUSTONIC LIMITED
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

#include <linux/sched.h>	/* struct task_struct */

#include "main.h"		/* g_ctx */
#include "protocol_common.h"

static struct {
	struct protocol_fe	*pfe;
	/* MC sessions */
	struct mutex		mc_sessions_lock;
	struct list_head	mc_sessions;
	/* GP operations */
	struct mutex		gp_operations_lock;
	struct list_head	gp_operations;
} l_ctx;

struct protocol_mc_session {
	struct list_head	list;
	struct completion	completion;
	int			ret;
	struct mcp_session	*session;
};

struct protocol_gp_operation {
	struct list_head		list;
	struct completion		completion;
	int				ret;
	u64				slot;
	struct gp_return		*gp_ret;
	struct interworld_session	*iws;
};

int protocol_fe_init(struct protocol_fe *pfe)
{
	/* Initialize pfe in arg */
	mutex_init(&pfe->protocol_mutex);
	/* Init l_ctx */
	mutex_init(&l_ctx.mc_sessions_lock);
	INIT_LIST_HEAD(&l_ctx.mc_sessions);
	mutex_init(&l_ctx.gp_operations_lock);
	INIT_LIST_HEAD(&l_ctx.gp_operations);
	/* Store pfe in l_ctx */
	l_ctx.pfe = pfe;
	return 0;
}

static inline void protocol_fe_get(struct protocol_fe *pfe)
{
	protocol_get(pfe);
	memset(pfe->fe2be_data, 0, sizeof(*pfe->fe2be_data));
}

/* MC protocol */

static inline struct protocol_mc_session *find_mc_session(u32 session_id)
{
	struct protocol_mc_session *session = ERR_PTR(-ENXIO), *candidate;

	mutex_lock(&l_ctx.mc_sessions_lock);
	list_for_each_entry(candidate, &l_ctx.mc_sessions, list) {
		struct mcp_session *mcp_session = candidate->session;

		if (mcp_session->sid == session_id) {
			session = candidate;
			break;
		}
	}
	mutex_unlock(&l_ctx.mc_sessions_lock);

	WARN(IS_ERR(session), "MC session not found for ID %u", session_id);
	return session;
}

static int protocol_mc_wait_done(struct protocol_fe *pfe)
{
	struct protocol_mc_session *session;

	mc_dev_devel("received response to mc_wait for session %x: %d",
		     pfe->be2fe_data->session_id,
		     pfe->be2fe_data->cmd_ret);

	session = find_mc_session(pfe->be2fe_data->session_id);

	if (IS_ERR(session))
		return PTR_ERR(session);

	session->ret = pfe->be2fe_data->cmd_ret;
	complete(&session->completion);
	return 0;
}

/* GP protocol */

static struct protocol_gp_operation *find_gp_operation(u64 operation_id)
{
	struct protocol_gp_operation *operation =
		ERR_PTR(-ENXIO), *candidate;

	mutex_lock(&l_ctx.gp_operations_lock);
	list_for_each_entry(candidate, &l_ctx.gp_operations, list) {
		if (candidate->slot == operation_id) {
			operation = candidate;
			list_del(&operation->list);
			break;
		}
	}
	mutex_unlock(&l_ctx.gp_operations_lock);

	WARN(IS_ERR(operation), "GP operation not found for op id %llx",
	     operation_id);
	return operation;
}

static int protocol_gp_operation_done(struct protocol_fe *pfe)
{
	struct protocol_gp_operation *operation;

	mc_dev_devel("received response to GP operation for op id %llx",
		     pfe->be2fe_data->operation_id);
	operation = find_gp_operation(pfe->be2fe_data->operation_id);
	if (IS_ERR(operation))
		return PTR_ERR(operation);

	/* operation->iws is NULL for close session */
	if (operation->iws) {
		*operation->iws = pfe->be2fe_data->iws;
		*operation->gp_ret = pfe->be2fe_data->gp_ret;
	}

	operation->ret = pfe->be2fe_data->cmd_ret;
	complete(&operation->completion);
	return 0;
}

/* Dispatch function for the FE callback */

int protocol_fe_dispatch(struct protocol_fe *pfe)
{
	int ret = -EINVAL;

	switch (pfe->be2fe_data->cmd) {
	case TEE_BE_NONE:
		ret = -ENOMEM;
		break;
	case TEE_MC_WAIT_DONE:
		ret = protocol_mc_wait_done(pfe);
		break;
	case TEE_GP_OPEN_SESSION_DONE:
		ret = protocol_gp_operation_done(pfe);
		break;
	case TEE_GP_CLOSE_SESSION_DONE:
		ret = protocol_gp_operation_done(pfe);
		break;
	case TEE_GP_INVOKE_COMMAND_DONE:
		ret = protocol_gp_operation_done(pfe);
		break;
	}

	return ret;
}

/* Call Backend */

static int protocol_call_be(struct protocol_fe *pfe)
{
	WARN_ON(!pfe->protocol_busy);

	pfe->fe_cmd_id++;
	if (!pfe->fe_cmd_id)
		pfe->fe_cmd_id++;

	pfe->fe2be_data->id = pfe->fe_cmd_id;
	mc_dev_devel("FE -> BE request %u id %u pid %d",
		     pfe->fe2be_data->cmd, pfe->fe2be_data->id, current->pid);

	return fe_ops->call_be(pfe);
}

/* MC protocol interface */

int protocol_mc_get_version(struct mc_version_info *version_info)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	int ret;

	protocol_fe_get(pfe);
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_GET_VERSION;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	memcpy(version_info, &pfe->fe2be_data->version_info,
	       sizeof(*version_info));
out:
	protocol_put(pfe);
	return ret;
}

int protocol_mc_open_session(struct mcp_session *session,
			     struct mcp_open_info *info)
{
	struct tee_protocol_buffer *ta_bin = &l_ctx.pfe->fe2be_data->ta_bin;
	struct tee_protocol_buffer *tci_buffer =
		&l_ctx.pfe->fe2be_data->buffers[0];
	struct protocol_fe *pfe = l_ctx.pfe;
	struct protocol_mc_session *fe_mc_session;
	struct protocol_fe_map *ta_map = NULL;
	struct protocol_fe_map *tci_map = NULL;
	int ret;

	fe_mc_session = kzalloc(sizeof(*fe_mc_session), GFP_KERNEL);
	if (!fe_mc_session)
		return -ENOMEM;

	INIT_LIST_HEAD(&fe_mc_session->list);
	init_completion(&fe_mc_session->completion);
	fe_mc_session->session = session;

	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->uuid = *info->uuid;
	if (info->type == TEE_MC_UUID) {
		/* Set the FE command for the BE */
		pfe->fe2be_data->cmd = TEE_MC_OPEN_SESSION;
	} else {
		struct mcp_buffer_map b_map;

		/* Set the FE command for the BE */
		pfe->fe2be_data->cmd = TEE_MC_OPEN_TRUSTLET;
		/* Convert ta_mmu (tee_mmu) into b_map (mcp_buffer_map) */
		tee_mmu_buffer(info->ta_mmu, &b_map);

		/* Grant access to all mmu table */
		ta_map = fe_ops->fe_map_create(ta_bin, &b_map, pfe);
		if (IS_ERR(ta_map)) {
			ret = PTR_ERR(ta_map);
			ta_map = NULL;
			goto out;
		}
	}

	if (info->tci_mmu) {
		struct mcp_buffer_map b_map;

		/* Convert tci_mmu (tee_mmu) into b_map (mcp_buffer_map) */
		tee_mmu_buffer(info->tci_mmu, &b_map);

		/* Grant access to all mmu table */
		tci_map = fe_ops->fe_map_create(tci_buffer, &b_map, pfe);
		if (IS_ERR(tci_map)) {
			ret = PTR_ERR(tci_map);
			tci_map = NULL;
			goto out;
		}
	} else {
		tci_buffer->flags = 0;
	}

	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	if (!ret)
		session->sid = pfe->fe2be_data->session_id;

out:
	if (!ret) {
		mutex_lock(&l_ctx.mc_sessions_lock);
		list_add_tail(&fe_mc_session->list, &l_ctx.mc_sessions);
		mutex_unlock(&l_ctx.mc_sessions_lock);
	} else {
		kfree(fe_mc_session);
	}

	/* Release the PMD and PTEs, but not the pages so they remain pinned */
	if (fe_ops->fe_map_release_pmd_ptes) {
		if (ta_map)
			fe_ops->fe_map_release_pmd_ptes(ta_map, ta_bin);

		if (tci_map)
			fe_ops->fe_map_release_pmd_ptes(tci_map, tci_buffer);
	}

	protocol_put(pfe);
	return ret;
}

int protocol_mc_close_session(struct mcp_session *session)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct protocol_mc_session *fe_mc_session;
	int ret;

	fe_mc_session = find_mc_session(session->sid);
	if (!fe_mc_session)
		return -ENXIO;

	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->session_id = session->sid;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_MC_CLOSE_SESSION;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	if (!ret) {
		mutex_lock(&l_ctx.mc_sessions_lock);
		session->state = MCP_SESSION_CLOSED;
		list_del(&fe_mc_session->list);
		mutex_unlock(&l_ctx.mc_sessions_lock);
		kfree(fe_mc_session);
	}
out:
	protocol_put(pfe);
	return ret;
}

int protocol_mc_notify(struct mcp_session *session)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	int ret;

	mc_dev_devel("MC notify session %x", session->sid);
	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->session_id = session->sid;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_MC_NOTIFY;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
out:
	protocol_put(pfe);
	return ret;
}

int protocol_mc_wait(struct mcp_session *session, s32 timeout)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct protocol_mc_session *fe_mc_session;
	int ret;

	/* Locked by caller so no two waits can happen on one session */
	fe_mc_session = find_mc_session(session->sid);
	if (!fe_mc_session)
		return -ENXIO;

	fe_mc_session->ret = 0;

	mc_dev_devel("MC wait session %x", session->sid);
	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->session_id = session->sid;
	pfe->fe2be_data->timeout = timeout;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_MC_WAIT;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
out:
	protocol_put(pfe);
	if (ret)
		return ret;

	/* Now wait for notification from Dom0 */
	ret = wait_for_completion_interruptible(&fe_mc_session->completion);
	if (!ret)
		ret = fe_mc_session->ret;

	return ret;
}

int protocol_mc_map(u32 session_id, struct tee_mmu *mmu, u32 *sva)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct tee_protocol_buffer *buffer = &l_ctx.pfe->fe2be_data->buffers[0];
	struct mcp_buffer_map b_map;
	struct protocol_fe_map *map = NULL;
	int ret;

	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->session_id = session_id;
	tee_mmu_buffer(mmu, &b_map);

	map = fe_ops->fe_map_create(buffer, &b_map, pfe);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		goto out;
	}

	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_MC_MAP;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	if (!ret) {
		*sva = buffer->sva;
		atomic_inc(&g_ctx.c_maps);
	}
out:
	if (fe_ops->fe_map_release_pmd_ptes)
		fe_ops->fe_map_release_pmd_ptes(map, buffer);

	protocol_put(pfe);
	return ret;
}

int protocol_mc_unmap(u32 session_id, const struct mcp_buffer_map *map)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct tee_protocol_buffer *buffer = &l_ctx.pfe->fe2be_data->buffers[0];
	int ret;

	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->session_id = session_id;
	buffer->length = map->length;
	buffer->sva = map->secure_va;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_MC_UNMAP;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	if (!ret)
		atomic_dec(&g_ctx.c_maps);
out:
	protocol_put(pfe);
	return ret;
}

int protocol_mc_get_err(struct mcp_session *session, s32 *err)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	int ret;

	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->session_id = session->sid;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_MC_GET_ERR;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	if (!ret)
		*err = pfe->fe2be_data->err;

	mc_dev_devel("MC get_err session %x err %d", session->sid, *err);
out:
	protocol_put(pfe);
	return ret;
}

/* GP protocol interface */

int protocol_gp_register_shared_mem(struct tee_mmu *mmu, u32 *sva,
				    struct gp_return *gp_ret)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct tee_protocol_buffer *buffer = &l_ctx.pfe->fe2be_data->buffers[0];
	struct mcp_buffer_map b_map;
	struct protocol_fe_map *map = NULL;
	int ret;

	protocol_fe_get(pfe);
	/* In */
	tee_mmu_buffer(mmu, &b_map);
	map = fe_ops->fe_map_create(buffer, &b_map, pfe);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		goto out;
	}

	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_GP_REGISTER_SHARED_MEM;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	if (!ret) {
		*sva = buffer->sva;
		atomic_inc(&g_ctx.c_maps);
	}

	if (pfe->fe2be_data->gp_ret.origin)
		*gp_ret = pfe->fe2be_data->gp_ret;
out:
	if (fe_ops->fe_map_release_pmd_ptes)
		fe_ops->fe_map_release_pmd_ptes(map, buffer);

	protocol_put(pfe);
	return ret;
}

int protocol_gp_release_shared_mem(struct mcp_buffer_map *map)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct tee_protocol_buffer *buffer = &l_ctx.pfe->fe2be_data->buffers[0];
	int ret;

	protocol_fe_get(pfe);
	/* In */
	buffer->addr = (uintptr_t)map->mmu;
	buffer->length = map->length;
	buffer->flags = map->flags;
	buffer->sva = map->secure_va;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_GP_RELEASE_SHARED_MEM;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
	if (!ret)
		atomic_dec(&g_ctx.c_maps);
out:
	protocol_put(pfe);
	return ret;
}

int protocol_gp_open_session(struct iwp_session *session,
			     const struct mc_uuid_t *uuid,
			     struct tee_mmu *ta_mmu,
			     const struct iwp_buffer_map *b_maps,
			     struct interworld_session *iws,
			     struct interworld_session *op_iws,
			     struct gp_return *gp_ret)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct tee_protocol_buffer *ta_bin = &pfe->fe2be_data->ta_bin;
	struct protocol_gp_operation operation = { .ret = 0 };
	struct protocol_fe_map *ta_map = NULL;
	struct protocol_fe_map *maps[4] = { NULL, NULL, NULL, NULL };
	int i, ret;

	/* Prepare operation first not to be racey */
	INIT_LIST_HEAD(&operation.list);
	init_completion(&operation.completion);
	/* Note: slot is a unique identifier for a session/operation */
	operation.slot = session->slot;
	operation.gp_ret = gp_ret;
	operation.iws = iws;
	mutex_lock(&l_ctx.gp_operations_lock);
	list_add_tail(&operation.list, &l_ctx.gp_operations);
	mutex_unlock(&l_ctx.gp_operations_lock);

	protocol_fe_get(pfe);
	/* The operation may contain tmpref's to map */
	for (i = 0; i < TEE_BUFFERS; i++) {
		if (!b_maps[i].map.addr) {
			pfe->fe2be_data->buffers[i].flags = 0;
			continue;
		}

		maps[i] = fe_ops->fe_map_create(&pfe->fe2be_data->buffers[i],
						&b_maps[i].map, pfe);
		if (IS_ERR(maps[i])) {
			ret = PTR_ERR(maps[i]);
			goto out;
		}
	}

	/* In */
	pfe->fe2be_data->uuid = *uuid;
	if (ta_mmu) {
		struct mcp_buffer_map b_map;

		tee_mmu_buffer(ta_mmu, &b_map);
		ta_map = fe_ops->fe_map_create(ta_bin, &b_map, pfe);
		if (IS_ERR(ta_map)) {
			ret = PTR_ERR(ta_map);
			ta_map = NULL;
			goto out;
		}
	}

	pfe->fe2be_data->operation_id = session->slot;
	pfe->fe2be_data->iws = *op_iws;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_GP_OPEN_SESSION;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
out:
	if (fe_ops->fe_map_release_pmd_ptes) {
		if (ta_map)
			fe_ops->fe_map_release_pmd_ptes(ta_map, ta_bin);

		for (i = 0; i < TEE_BUFFERS; i++)
			if (maps[i])
				fe_ops->fe_map_release_pmd_ptes(
					maps[i], &pfe->fe2be_data->buffers[i]);
	}

	protocol_put(pfe);
	if (ret) {
		mutex_lock(&l_ctx.gp_operations_lock);
		list_del(&operation.list);
		mutex_unlock(&l_ctx.gp_operations_lock);
		return ret;
	}

	/* Now wait for notification from Dom0 */
	wait_for_completion(&operation.completion);
	/* FIXME origins? */
	return operation.ret;
}

int protocol_gp_close_session(struct iwp_session *session)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct protocol_gp_operation operation = { .ret = 0 };
	int ret;

	/* Prepare operation first not to be racey */
	INIT_LIST_HEAD(&operation.list);
	init_completion(&operation.completion);
	/* Note: slot is a unique identifier for a session/operation */
	operation.slot = session->slot;
	mutex_lock(&l_ctx.gp_operations_lock);
	list_add_tail(&operation.list, &l_ctx.gp_operations);
	mutex_unlock(&l_ctx.gp_operations_lock);

	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->session_id = session->sid;
	pfe->fe2be_data->operation_id = session->slot;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_GP_CLOSE_SESSION;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
out:
	protocol_put(pfe);
	if (ret) {
		mutex_lock(&l_ctx.gp_operations_lock);
		list_del(&operation.list);
		mutex_unlock(&l_ctx.gp_operations_lock);
		return ret;
	}

	/* Now wait for notification from Dom0 */
	wait_for_completion(&operation.completion);
	return operation.ret;
}

int protocol_gp_invoke_command(struct iwp_session *session,
			       const struct iwp_buffer_map *b_maps,
			       struct interworld_session *iws,
			       struct gp_return *gp_ret)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	struct protocol_gp_operation operation = { .ret = 0 };
	struct protocol_fe_map *maps[4] = { NULL, NULL, NULL, NULL };
	int i, ret;

	/* Prepare operation first not to be racey */
	INIT_LIST_HEAD(&operation.list);
	init_completion(&operation.completion);
	/* Note: slot is a unique identifier for a session/operation */
	operation.slot = session->slot;
	operation.gp_ret = gp_ret;
	operation.iws = iws;
	mutex_lock(&l_ctx.gp_operations_lock);
	list_add_tail(&operation.list, &l_ctx.gp_operations);
	mutex_unlock(&l_ctx.gp_operations_lock);

	protocol_fe_get(pfe);
	/* The operation is in op_iws and may contain tmpref's to map */
	for (i = 0; i < TEE_BUFFERS; i++) {
		if (!b_maps[i].map.addr) {
			pfe->fe2be_data->buffers[i].flags = 0;
			continue;
		}

		maps[i]  = fe_ops->fe_map_create(
			&pfe->fe2be_data->buffers[i], &b_maps[i].map, pfe);
		if (IS_ERR(maps[i])) {
			ret = PTR_ERR(maps[i]);
			goto out;
		}
	}

	/* In */
	pfe->fe2be_data->session_id = session->sid;
	pfe->fe2be_data->operation_id = session->slot;
	pfe->fe2be_data->iws = *iws;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_GP_INVOKE_COMMAND;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
out:
	if (fe_ops->fe_map_release_pmd_ptes) {
		for (i = 0; i < TEE_BUFFERS; i++)
			if (maps[i])
				fe_ops->fe_map_release_pmd_ptes(
					maps[i], &pfe->fe2be_data->buffers[i]);
	}

	protocol_put(pfe);
	if (ret) {
		mutex_lock(&l_ctx.gp_operations_lock);
		list_del(&operation.list);
		mutex_unlock(&l_ctx.gp_operations_lock);
		return ret;
	}

	/* Now wait for notification from Dom0 */
	wait_for_completion(&operation.completion);
	/* FIXME origins? */
	return operation.ret;
}

int protocol_gp_request_cancellation(u64 slot)
{
	struct protocol_fe *pfe = l_ctx.pfe;
	int ret;

	protocol_fe_get(pfe);
	/* In */
	pfe->fe2be_data->operation_id = slot;
	/* Set the FE command for the BE */
	pfe->fe2be_data->cmd = TEE_GP_REQUEST_CANCELLATION;
	/* Call */
	ret = protocol_call_be(pfe);
	if (ret)
		goto out;

	/* Out */
	ret = pfe->fe2be_data->otherend_ret;
out:
	protocol_put(pfe);
	return ret;
}

/* Device */
