// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */
#include "hab.h"
#include <linux/module.h>

int32_t habmm_socket_open(int32_t *handle, uint32_t mm_ip_id,
		uint32_t timeout, uint32_t flags)
{
	return hab_vchan_open(hab_driver.kctx, mm_ip_id, handle,
				timeout, flags);
}
EXPORT_SYMBOL(habmm_socket_open);

int32_t habmm_socket_close(int32_t handle)
{
	return hab_vchan_close(hab_driver.kctx, handle);
}
EXPORT_SYMBOL(habmm_socket_close);

int32_t habmm_socket_send(int32_t handle, void *src_buff,
		uint32_t size_bytes, uint32_t flags)
{
	struct hab_send param = {0};

	param.vcid = handle;
	param.data = (uint64_t)(uintptr_t)src_buff;
	param.sizebytes = size_bytes;
	param.flags = flags;

	return hab_vchan_send(hab_driver.kctx, handle,
			size_bytes, src_buff, flags);
}
EXPORT_SYMBOL(habmm_socket_send);

int32_t habmm_socket_recv(int32_t handle, void *dst_buff, uint32_t *size_bytes,
		uint32_t timeout, uint32_t flags)
{
	int ret = 0;
	struct hab_message *msg = NULL;

	if (!size_bytes || !dst_buff)
		return -EINVAL;

	ret = hab_vchan_recv(hab_driver.kctx, &msg, handle, size_bytes, flags);

	if (ret == 0 && msg)
		memcpy(dst_buff, msg->data, msg->sizebytes);
	else if (ret && msg)
		pr_warn("vcid %X recv failed %d but msg is still received %zd bytes\n",
				handle, ret, msg->sizebytes);

	if (msg)
		hab_msg_free(msg);

	return ret;
}
EXPORT_SYMBOL(habmm_socket_recv);

int32_t habmm_export(int32_t handle, void *buff_to_share, uint32_t size_bytes,
		 uint32_t *export_id, uint32_t flags)
{
	int ret;
	struct hab_export param = {0};

	if (!export_id)
		return -EINVAL;

	param.vcid = handle;
	param.buffer = (uint64_t)(uintptr_t)buff_to_share;
	param.sizebytes = size_bytes;
	param.flags = flags;

	ret = hab_mem_export(hab_driver.kctx, &param, 1);

	*export_id = param.exportid;
	return ret;
}
EXPORT_SYMBOL(habmm_export);

int32_t habmm_unexport(int32_t handle, uint32_t export_id, uint32_t flags)
{
	struct hab_unexport param = {0};

	param.vcid = handle;
	param.exportid = export_id;

	return hab_mem_unexport(hab_driver.kctx, &param, 1);
}
EXPORT_SYMBOL(habmm_unexport);

int32_t habmm_import(int32_t handle, void **buff_shared, uint32_t size_bytes,
		uint32_t export_id, uint32_t flags)
{
	int ret;
	struct hab_import param = {0};

	if (!buff_shared)
		return -EINVAL;

	param.vcid = handle;
	param.sizebytes = size_bytes;
	param.exportid = export_id;
	param.flags = flags;

	ret = hab_mem_import(hab_driver.kctx, &param, 1);
	if (!ret)
		*buff_shared = (void *)(uintptr_t)param.kva;

	return ret;
}
EXPORT_SYMBOL(habmm_import);

int32_t habmm_unimport(int32_t handle,
		uint32_t export_id,
		void *buff_shared,
		uint32_t flags)
{
	struct hab_unimport param = {0};

	param.vcid = handle;
	param.exportid = export_id;
	param.kva = (uint64_t)(uintptr_t)buff_shared;

	return hab_mem_unimport(hab_driver.kctx, &param, 1);
}
EXPORT_SYMBOL(habmm_unimport);

int32_t habmm_socket_query(int32_t handle,
		struct hab_socket_info *info,
		uint32_t flags)
{
	int ret;
	uint64_t ids;
	char nm[VMNAME_SIZE * 2];

	if (!info)
		return -EINVAL;

	ret = hab_vchan_query(hab_driver.kctx, handle, &ids, nm, sizeof(nm), 1);
	if (!ret) {
		info->vmid_local = ids & 0xFFFFFFFF;
		info->vmid_remote = (ids & 0xFFFFFFFF00000000UL) > 32;

		strlcpy(info->vmname_local, nm, sizeof(info->vmname_local));
		strlcpy(info->vmname_remote, &nm[sizeof(info->vmname_local)],
			sizeof(info->vmname_remote));
	}
	return ret;
}
EXPORT_SYMBOL(habmm_socket_query);
