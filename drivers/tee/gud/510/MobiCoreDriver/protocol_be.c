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

#include "protocol_common.h"
#include "protocol.h"
#include "main.h"
#include "mmu.h"
#include "client.h"

/* Call Frontend */

static inline void protocol_be_get(struct protocol_fe *pfe)
{
	protocol_get(pfe);
	memset(pfe->be2fe_data, 0, sizeof(*pfe->be2fe_data));
}

static void protocol_call_fe(struct protocol_fe *pfe,
			     atomic_t call_vm_instance_no)
{
	WARN_ON(!pfe->protocol_busy);
	mc_dev_devel("BE -> FE command %u id %u ret %d",
		     pfe->be2fe_data->cmd, pfe->be2fe_data->id,
		     pfe->be2fe_data->cmd_ret);
	be_ops->call_fe(pfe, call_vm_instance_no);
}

/* MC protocol interface */

static int protocol_be_get_version(struct protocol_fe *pfe)
{
	struct mc_version_info version_info;
	int ret;

	ret = mcp_get_version(&version_info);
	if (ret)
		return ret;

	pfe->fe2be_data->version_info = version_info;
	return 0;
}

static int protocol_be_mc_open_session(struct protocol_fe *pfe)
{
	struct tee_protocol_buffer *tci_buffer = &pfe->fe2be_data->buffers[0];
	struct mcp_open_info info = {
		.type = TEE_MC_UUID,
		.uuid = &pfe->fe2be_data->uuid,
	};
	int ret;

	if (tci_buffer->flags) {
		struct protocol_be_map *map = NULL;
		struct mcp_buffer_map b_map = {
			.offset = tci_buffer->offset,
			.length = tci_buffer->length,
			.flags = tci_buffer->flags,
		};

		/* Verify all PMD, PTEs and pages and remap all */
		map = be_ops->be_map_create(tci_buffer, pfe);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto out;
		}

		/* Set mmu from map (protocol_be_map) */
		info.tci_mmu = be_ops->be_set_mmu(map, b_map);
		if (IS_ERR(info.tci_mmu)) {
			ret = PTR_ERR(info.tci_mmu);
			info.tci_mmu = NULL;
			goto out;
		}
	}

	/* Open session */
	ret = client_mc_open_common(pfe->client, &info,
				    &pfe->fe2be_data->session_id);

out:
	if (info.tci_mmu)
		/* Release our reference, now handled by the session */
		tee_mmu_put(info.tci_mmu);

	mc_dev_devel("session %x, exit with %d",
		     pfe->fe2be_data->session_id, ret);
	return ret;
}

static int protocol_be_mc_open_trustlet(struct protocol_fe *pfe)
{
	struct tee_protocol_buffer *ta_bin = &pfe->fe2be_data->ta_bin;
	struct tee_protocol_buffer *tci_buffer = &pfe->fe2be_data->buffers[0];
	struct mcp_open_info info = {
		.type = TEE_MC_TA,
		.uuid = &pfe->fe2be_data->uuid,
	};
	int ret = -ENOMEM;

	{
		struct protocol_be_map *map = NULL;
		struct mcp_buffer_map b_map = {
			.offset = ta_bin->offset,
			.length = ta_bin->length,
			.flags = ta_bin->flags,
		};

		/* Verify all PMD, PTEs and pages and remap all */
		map = be_ops->be_map_create(ta_bin, pfe);

		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto err_ta_map;
		}

		/* Set mmu from map (protocol_be_map) */
		info.ta_mmu = be_ops->be_set_mmu(map, b_map);
		if (IS_ERR(info.ta_mmu)) {
			be_ops->be_map_delete(map);
			ret = PTR_ERR(info.ta_mmu);
			info.ta_mmu = NULL;
			goto err_ta_map;
		}
	}

	if (tci_buffer->flags) {
		struct protocol_be_map *map = NULL;
		struct mcp_buffer_map b_map = {
			.offset = tci_buffer->offset,
			.length = tci_buffer->length,
			.flags = tci_buffer->flags,
		};

		/* Verify all PMD, PTEs and pages and remap all */
		map = be_ops->be_map_create(tci_buffer, pfe);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto err_tci_map;
		}

		/* Set mmu from map (protocol_be_map) */
		info.tci_mmu = be_ops->be_set_mmu(map, b_map);
		if (IS_ERR(info.tci_mmu)) {
			be_ops->be_map_delete(map);
			ret = PTR_ERR(info.tci_mmu);
			info.tci_mmu = NULL;
			goto err_tci_map;
		}
	}

	/* Open session */
	ret = client_mc_open_common(pfe->client, &info,
				    &pfe->fe2be_data->session_id);

err_tci_map:
	tee_mmu_put(info.ta_mmu);

	if (info.tci_mmu)
		/* Release our reference, now handled by the session */
		tee_mmu_put(info.tci_mmu);

err_ta_map:
	mc_dev_devel("session %x, exit with %d",
		     pfe->fe2be_data->session_id, ret);
	return ret;
}

static int protocol_be_mc_close_session(struct protocol_fe *pfe)
{
	return client_remove_session(pfe->client,
				     pfe->fe2be_data->session_id);
}

static int protocol_be_mc_notify(struct protocol_fe *pfe)
{
	return client_notify_session(pfe->client,
				     pfe->fe2be_data->session_id);
}

/* mc_wait cannot keep the channel busy while waiting, so we use a worker */
struct mc_wait_work {
	struct work_struct	work;
	struct protocol_fe	*pfe;
	u32			session_id;
	s32			timeout;
	u32			id;
	atomic_t		vm_instance_no;
};

static void protocol_be_mc_wait_worker(struct work_struct *work)
{
	struct mc_wait_work *wait_work =
		container_of(work, struct mc_wait_work, work);
	struct protocol_fe *pfe = wait_work->pfe;
	int ret;

	ret = client_waitnotif_session(wait_work->pfe->client,
				       wait_work->session_id,
				       wait_work->timeout);

	/* Send return code */
	mc_dev_devel("MC wait session done %x, ret %d",
		     wait_work->session_id, ret);
	protocol_be_get(pfe);
	/* In */
	pfe->be2fe_data->cmd_ret = ret;
	pfe->be2fe_data->session_id = wait_work->session_id;
	pfe->be2fe_data->id = wait_work->id;
	/* Set the BE cmd for the FE */
	pfe->be2fe_data->cmd = TEE_MC_WAIT_DONE;
	/* Call */
	protocol_call_fe(pfe, wait_work->vm_instance_no);
	/* Out */
	protocol_put(pfe);
	kfree(wait_work);
}

static int protocol_be_mc_wait(struct protocol_fe *pfe)
{
	struct mc_wait_work *wait_work;

	/* Wait in a separate thread to release the communication ring */
	wait_work = kzalloc(sizeof(*wait_work), GFP_KERNEL);
	if (!wait_work)
		return -ENOMEM;

	wait_work->pfe = pfe;
	atomic_set(&wait_work->vm_instance_no,
		   atomic_read(&pfe->vm_instance_no));
	wait_work->session_id = pfe->fe2be_data->session_id;
	wait_work->timeout = pfe->fe2be_data->timeout;
	wait_work->id = pfe->fe2be_data->id;
	INIT_WORK(&wait_work->work, protocol_be_mc_wait_worker);
	schedule_work(&wait_work->work);
	return 0;
}

static int protocol_be_mc_map(struct protocol_fe *pfe)
{
	struct tee_protocol_buffer *buffer = &pfe->fe2be_data->buffers[0];
	struct protocol_be_map *map = NULL;
	struct mc_ioctl_buffer buf;
	int ret;
	struct mcp_buffer_map b_map = {
		.offset = buffer->offset,
		.length = buffer->length,
		.flags = buffer->flags,
	};

	/* Verify all PMD, PTEs and pages and remap all */
	map = be_ops->be_map_create(buffer, pfe);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		return ret;
	}

	/* Set mmu from map (protocol_be_map) */
	map->mmu = be_ops->be_set_mmu(map, b_map);
	if (IS_ERR(map->mmu)) {
		int err = PTR_ERR(map->mmu);

		be_ops->be_map_delete(map);
		return err;
	}

	ret = client_mc_map(pfe->client,
			    pfe->fe2be_data->session_id,
			    map->mmu, &buf);
	if (!ret)
		buffer->sva = buf.sva;

	/* Releasing the MMU shall also clear the map */
	tee_mmu_put(map->mmu);

	mc_dev_devel("session %x, exit with %d",
		     pfe->fe2be_data->session_id, ret);
	return ret;
}

static int protocol_be_mc_unmap(struct protocol_fe *pfe)
{
	struct tee_protocol_buffer *buffer = &pfe->fe2be_data->buffers[0];
	struct mc_ioctl_buffer buf = {
		.len = buffer->length,
		.sva = buffer->sva,
	};
	int ret;

	ret = client_mc_unmap(pfe->client,
			      pfe->fe2be_data->session_id,
			      &buf);

	mc_dev_devel("session %x, exit with %d",
		     pfe->fe2be_data->session_id, ret);
	return ret;
}

static int protocol_be_mc_get_err(struct protocol_fe *pfe)
{
	int ret;

	ret = client_get_session_exitcode(pfe->client,
					  pfe->fe2be_data->session_id,
					  &pfe->fe2be_data->err);
	mc_dev_devel("session %x err %d, exit with %d",
		     pfe->fe2be_data->session_id,
		     pfe->fe2be_data->err,
		     ret);
	return ret;
}

/* GP protocol interface */

static int protocol_be_gp_register_shared_mem(struct protocol_fe *pfe)
{
	struct tee_protocol_buffer *buffer = &pfe->fe2be_data->buffers[0];
	struct protocol_be_map *map = NULL;
	struct gp_shared_memory memref = {
		.buffer = buffer->addr,
		.size = buffer->length,
		.flags = buffer->flags,
	};
	int ret;
	struct mcp_buffer_map b_map = {
		.offset = buffer->offset,
		.length = buffer->length,
		.flags = buffer->flags,
	};

	map = be_ops->be_map_create(buffer, pfe);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		return ret;
	}

	/* Set mmu from map (protocol_be_map) */
	map->mmu = be_ops->be_set_mmu(map, b_map);
	if (IS_ERR(map->mmu)) {
		int err = PTR_ERR(map->mmu);

		be_ops->be_map_delete(map);
		return err;
	}

	ret = client_gp_register_shared_mem(pfe->client, map->mmu,
					    &buffer->sva, &memref,
					    &pfe->fe2be_data->gp_ret);

	/* Releasing the MMU shall also clear the map */
	tee_mmu_put(map->mmu);

	mc_dev_devel("session %x, exit with %d",
		     pfe->fe2be_data->session_id, ret);
	return ret;
}

static int protocol_be_gp_release_shared_mem(struct protocol_fe *pfe)
{
	struct tee_protocol_buffer *buffer = &pfe->fe2be_data->buffers[0];
	struct gp_shared_memory memref = {
		.buffer = buffer->addr,
		.size = buffer->length,
		.flags = buffer->flags,
	};
	int ret;

	ret = client_gp_release_shared_mem(pfe->client, &memref);

	mc_dev_devel("exit with %d", ret);
	return ret;
}

/* GP functions cannot keep the ring busy while waiting, so we use a worker */
struct gp_work {
	struct work_struct		work;
	struct protocol_fe		*pfe;
	u64				operation_id;
	struct interworld_session	iws;
	struct tee_mmu			*ta_mmu;
	struct tee_mmu			*mmus[4];
	struct mc_uuid_t		uuid;
	u32				session_id;
	u32				id;
	atomic_t			vm_instance_no;
};

static void protocol_be_gp_open_session_worker(struct work_struct *work)
{
	struct gp_work *gp_work = container_of(work, struct gp_work, work);
	struct protocol_fe *pfe = gp_work->pfe;
	struct gp_return gp_ret;
	int i, ret;

	ret = client_gp_open_session_domu(pfe->client, &gp_work->uuid,
					  gp_work->ta_mmu,
					  gp_work->operation_id, &gp_work->iws,
					  gp_work->mmus, &gp_ret);
	mc_dev_devel("GP open session done, ret %d", ret);
	for (i = 0; i < TEE_BUFFERS; i++)
		if (gp_work->mmus[i])
			tee_mmu_put(gp_work->mmus[i]);

	/* Send return code */
	protocol_be_get(pfe);
	/* In */
	pfe->be2fe_data->cmd_ret = ret;
	pfe->be2fe_data->operation_id = gp_work->operation_id;
	pfe->be2fe_data->iws = gp_work->iws;
	pfe->be2fe_data->gp_ret = gp_ret;
	/* Set the BE cmd for the FE */
	pfe->be2fe_data->cmd = TEE_GP_OPEN_SESSION_DONE;
	/* Call */
	protocol_call_fe(pfe, gp_work->vm_instance_no);
	/* Out */
	protocol_put(pfe);
	if (gp_work->ta_mmu)
		tee_mmu_put(gp_work->ta_mmu);

	kfree(gp_work);
}

static int protocol_be_gp_open_session(struct protocol_fe *pfe)
{
	struct tee_protocol_buffer *ta_bin = &pfe->fe2be_data->ta_bin;
	struct gp_work *gp_work;
	int i, ret = 0;

	gp_work = kzalloc(sizeof(*gp_work), GFP_KERNEL);
	if (!gp_work)
		return -ENOMEM;

	/* Map Trusted Application binary */
	if (ta_bin->flags) {
		struct protocol_be_map *map = NULL;
		struct mcp_buffer_map b_map = {
			.offset = ta_bin->offset,
			.length = ta_bin->length,
			.flags = ta_bin->flags,
		};

		/* Verify all PMD, PTEs and pages and remap all */
		map = be_ops->be_map_create(ta_bin, pfe);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto err_ta_map;
		}

		/* Set mmu from map (protocol_be_map) */
		gp_work->ta_mmu = be_ops->be_set_mmu(map, b_map);
		if (IS_ERR(gp_work->ta_mmu)) {
			be_ops->be_map_delete(map);
			ret = PTR_ERR(gp_work->ta_mmu);
			goto err_ta_map;
		}
	}

	/* Map tmpref buffers */
	for (i = 0; i < TEE_BUFFERS; i++) {
		struct tee_protocol_buffer *buffer =
			&pfe->fe2be_data->buffers[i];
		struct protocol_be_map *map = NULL;
		struct mcp_buffer_map b_map = {
			.offset = buffer->offset,
			.length = buffer->length,
			.flags = buffer->flags,
		};

		if (!buffer->flags)
			continue;

		map = be_ops->be_map_create(buffer, pfe);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto err_map;
		}

		/* Set mmu from map (protocol_be_map) */
		gp_work->mmus[i] = be_ops->be_set_mmu(map, b_map);
		if (IS_ERR(gp_work->mmus[i])) {
			be_ops->be_map_delete(map);
			ret = PTR_ERR(gp_work->mmus[i]);
			goto err_mmus;
		}
	}

	gp_work->pfe = pfe;
	atomic_set(&gp_work->vm_instance_no, atomic_read(&pfe->vm_instance_no));
	gp_work->operation_id = pfe->fe2be_data->operation_id;
	gp_work->iws = pfe->fe2be_data->iws;
	gp_work->uuid = pfe->fe2be_data->uuid;
	gp_work->id = pfe->fe2be_data->id;
	INIT_WORK(&gp_work->work, protocol_be_gp_open_session_worker);
	schedule_work(&gp_work->work);
	return 0;

err_mmus:
	for (i = 0; i < TEE_BUFFERS; i++)
		if (!IS_ERR_OR_NULL(gp_work->mmus[i]))
			tee_mmu_put(gp_work->mmus[i]);
err_map:
	if (!IS_ERR_OR_NULL(gp_work->ta_mmu))
		tee_mmu_put(gp_work->ta_mmu);
err_ta_map:
	kfree(gp_work);
	return ret;
}

static void protocol_be_gp_close_session_worker(
	struct work_struct *work)
{
	struct gp_work *gp_work = container_of(work, struct gp_work, work);
	struct protocol_fe *pfe = gp_work->pfe;
	int ret;

	ret = client_gp_close_session(pfe->client, gp_work->session_id);
	mc_dev_devel("GP close session done, ret %d", ret);

	/* Send return code */
	protocol_be_get(pfe);
	/* In */
	pfe->be2fe_data->cmd_ret = ret;
	pfe->be2fe_data->operation_id = gp_work->operation_id;
	/* Set the BE cmd for the FE */
	pfe->be2fe_data->cmd = TEE_GP_CLOSE_SESSION_DONE;
	/* Call */
	protocol_call_fe(pfe, gp_work->vm_instance_no);
	/* Out */
	protocol_put(pfe);
	kfree(gp_work);
}

static int protocol_be_gp_close_session(struct protocol_fe *pfe)
{
	struct gp_work *gp_work;

	gp_work = kzalloc(sizeof(*gp_work), GFP_KERNEL);
	if (!gp_work)
		return -ENOMEM;

	gp_work->pfe = pfe;
	atomic_set(&gp_work->vm_instance_no, atomic_read(&pfe->vm_instance_no));
	gp_work->operation_id = pfe->fe2be_data->operation_id;
	gp_work->session_id = pfe->fe2be_data->session_id;
	gp_work->id = pfe->fe2be_data->id;
	INIT_WORK(&gp_work->work, protocol_be_gp_close_session_worker);
	schedule_work(&gp_work->work);
	return 0;
}

static void protocol_be_gp_invoke_command_worker(
	struct work_struct *work)
{
	struct gp_work *gp_work = container_of(work, struct gp_work, work);
	struct protocol_fe *pfe = gp_work->pfe;
	struct gp_return gp_ret;
	int i, ret;

	ret = client_gp_invoke_command_domu(pfe->client,
					    gp_work->session_id,
					    gp_work->operation_id,
					    &gp_work->iws, gp_work->mmus,
					    &gp_ret);
	mc_dev_devel("GP invoke command done, ret %d", ret);
	for (i = 0; i < TEE_BUFFERS; i++)
		if (gp_work->mmus[i])
			tee_mmu_put(gp_work->mmus[i]);

	/* Send return code */
	protocol_be_get(pfe);
	/* In */
	pfe->be2fe_data->cmd_ret = ret;
	pfe->be2fe_data->operation_id = gp_work->operation_id;
	pfe->be2fe_data->iws = gp_work->iws;
	pfe->be2fe_data->gp_ret = gp_ret;
	/* Set the BE cmd for the FE */
	pfe->be2fe_data->cmd = TEE_GP_INVOKE_COMMAND_DONE;
	/* Call */
	protocol_call_fe(pfe, gp_work->vm_instance_no);
	/* Out */
	protocol_put(pfe);
	kfree(gp_work);
}

static int protocol_be_gp_invoke_command(struct protocol_fe *pfe)
{
	struct gp_work *gp_work;
	int i, ret = 0;

	gp_work = kzalloc(sizeof(*gp_work), GFP_KERNEL);
	if (!gp_work)
		return -ENOMEM;

	/* Map tmpref buffers */
	for (i = 0; i < TEE_BUFFERS; i++) {
		struct tee_protocol_buffer *buffer =
			&pfe->fe2be_data->buffers[i];
		struct protocol_be_map *map = NULL;
		struct mcp_buffer_map b_map = {
			.offset = buffer->offset,
			.length = buffer->length,
			.flags = buffer->flags,
		};

		if (!buffer->flags)
			continue;

		map = be_ops->be_map_create(buffer, pfe);
		if (IS_ERR(map)) {
			ret = PTR_ERR(map);
			goto err_map;
		}

		gp_work->mmus[i] = be_ops->be_set_mmu(map, b_map);
		if (IS_ERR(gp_work->mmus[i])) {
			be_ops->be_map_delete(map);

			ret = PTR_ERR(gp_work->mmus[i]);
			goto err_mmus;
		}
	}

	gp_work->pfe = pfe;
	atomic_set(&gp_work->vm_instance_no, atomic_read(&pfe->vm_instance_no));
	gp_work->operation_id = pfe->fe2be_data->operation_id;
	gp_work->iws = pfe->fe2be_data->iws;
	gp_work->session_id = pfe->fe2be_data->session_id;
	gp_work->id = pfe->fe2be_data->id;
	INIT_WORK(&gp_work->work, protocol_be_gp_invoke_command_worker);
	schedule_work(&gp_work->work);
	return 0;

err_mmus:
	for (i = 0; i < TEE_BUFFERS; i++)
		if (!IS_ERR_OR_NULL(gp_work->mmus[i]))
			tee_mmu_put(gp_work->mmus[i]);
err_map:
	kfree(gp_work);
	return ret;
}

static int protocol_be_gp_request_cancellation(struct protocol_fe *pfe)
{
	client_gp_request_cancellation(pfe->client,
				       pfe->fe2be_data->operation_id);
	return 0;
}

int protocol_be_dispatch(struct protocol_fe *pfe)
{
	int ret = -EINVAL;

	switch (pfe->fe2be_data->cmd) {
	case TEE_FE_NONE:
		break;
	case TEE_CONNECT:
		if (be_ops && be_ops->be_create_client)
			ret = be_ops->be_create_client(pfe);
		break;
	/* MC */
	case TEE_GET_VERSION:
		ret = protocol_be_get_version(pfe);
		break;
	case TEE_MC_OPEN_SESSION:
		ret = protocol_be_mc_open_session(pfe);
		break;
	case TEE_MC_OPEN_TRUSTLET:
		ret = protocol_be_mc_open_trustlet(pfe);
		break;
	case TEE_MC_CLOSE_SESSION:
		ret = protocol_be_mc_close_session(pfe);
		break;
	case TEE_MC_NOTIFY:
		ret = protocol_be_mc_notify(pfe);
		break;
	case TEE_MC_WAIT:
		ret = protocol_be_mc_wait(pfe);
		break;
	case TEE_MC_MAP:
		ret = protocol_be_mc_map(pfe);
		break;
	case TEE_MC_UNMAP:
		ret = protocol_be_mc_unmap(pfe);
		break;
	case TEE_MC_GET_ERR:
		ret = protocol_be_mc_get_err(pfe);
		break;
	/* GP */
	case TEE_GP_REGISTER_SHARED_MEM:
		ret = protocol_be_gp_register_shared_mem(pfe);
		break;
	case TEE_GP_RELEASE_SHARED_MEM:
		ret = protocol_be_gp_release_shared_mem(pfe);
		break;
	case TEE_GP_OPEN_SESSION:
		ret = protocol_be_gp_open_session(pfe);
		break;
	case TEE_GP_CLOSE_SESSION:
		ret = protocol_be_gp_close_session(pfe);
		break;
	case TEE_GP_INVOKE_COMMAND:
		ret = protocol_be_gp_invoke_command(pfe);
		break;
	case TEE_GP_REQUEST_CANCELLATION:
		ret = protocol_be_gp_request_cancellation(pfe);
		break;
	}

	return ret;
}

