/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "tee_impl/tee_priv.h"
#include "tee_impl/tee_common.h"

static struct tee_op_cmd_mappings tee_svp_secmem_op_cmd_mappings = {
	.tee_cmds[TEE_OP_ALLOC] = CMD_SEC_MEM_ALLOC,
	.tee_cmds[TEE_OP_ALLOC_ZERO] = CMD_SEC_MEM_ALLOC_ZERO,
	.tee_cmds[TEE_OP_FREE] = CMD_SEC_MEM_UNREF,
	.tee_cmds[TEE_OP_REGION_ENABLE] = CMD_SEC_MEM_ENABLE,
	.tee_cmds[TEE_OP_REGION_DISABLE] = CMD_SEC_MEM_DISABLE,
	.tee_mem_type = TEE_MEM_SVP,
};

static struct tee_op_cmd_mappings tee_wfd_smem_op_cmd_mappings = {
	.tee_cmds[TEE_OP_ALLOC] = CMD_WFD_SMEM_ALLOC,
	.tee_cmds[TEE_OP_ALLOC_ZERO] = CMD_WFD_SMEM_ALLOC_ZERO,
	.tee_cmds[TEE_OP_FREE] = CMD_WFD_SMEM_UNREF,
	.tee_cmds[TEE_OP_REGION_ENABLE] = CMD_WFD_SMEM_ENABLE,
	.tee_cmds[TEE_OP_REGION_DISABLE] = CMD_WFD_SMEM_DISABLE,
	.tee_mem_type = TEE_MEM_WFD,
};

static struct tee_op_cmd_mappings tee_sdsp_smem_op_cmd_mappings = {
	.tee_cmds[TEE_OP_ALLOC] = CMD_SDSP_SMEM_ALLOC,
	.tee_cmds[TEE_OP_ALLOC_ZERO] = CMD_SDSP_SMEM_ALLOC_ZERO,
	.tee_cmds[TEE_OP_FREE] = CMD_SDSP_SMEM_UNREF,
	.tee_cmds[TEE_OP_REGION_ENABLE] = CMD_SDSP_SMEM_ENABLE,
	.tee_cmds[TEE_OP_REGION_DISABLE] = CMD_SDSP_SMEM_DISABLE,
	.tee_mem_type = TEE_MEM_SDSP_SHARED,
};

u32 get_tee_cmd(enum TEE_OP op, void *peer_priv)
{
	struct tee_op_cmd_mappings *op_map =
		(struct tee_op_cmd_mappings *)peer_priv;

	if (unlikely(INVALID(peer_priv)))
		return CMD_SEC_MEM_INVALID;

	return op_map->tee_cmds[op];
}

u32 get_tee_mem_type(void *peer_priv)
{
	struct tee_op_cmd_mappings *op_map =
		(struct tee_op_cmd_mappings *)peer_priv;

	if (unlikely(INVALID(peer_priv)))
		return -1;

	return op_map->tee_mem_type;
}

void get_tee_peer_priv_data(enum TEE_MEM_TYPE tee_mem_type, void **peer_priv)
{
	if (tee_mem_type == TEE_MEM_SVP) {
		pr_info("TEE_MEM_SVP_PRIV_DATA\n");
		*peer_priv = &tee_svp_secmem_op_cmd_mappings;
	} else if (tee_mem_type == TEE_MEM_WFD) {
		pr_info("TEE_MEM_WFD_PRIV_DATA\n");
		*peer_priv = &tee_wfd_smem_op_cmd_mappings;
	} else if (tee_mem_type == TEE_MEM_SDSP_SHARED) {
		pr_info("TEE_MEM_SDSP_PRIV_DATA\n");
		*peer_priv = &tee_sdsp_smem_op_cmd_mappings;
	} else {
		pr_err("invalid tee memory type\n");
	}
}
