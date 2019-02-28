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

#ifndef TEE_COMMON_H_
#define TEE_COMMON_H_

#define CMD_SEC_MEM_ALLOC 1
#define CMD_SEC_MEM_UNREF 3
#define CMD_SEC_MEM_ENABLE 7
#define CMD_SEC_MEM_DISABLE 8
#define CMD_SEC_MEM_ALLOC_ZERO 13
#define CMD_WFD_SMEM_ALLOC 30
#define CMD_WFD_SMEM_UNREF 31
#define CMD_WFD_SMEM_ENABLE 32
#define CMD_WFD_SMEM_DISABLE 33
#define CMD_WFD_SMEM_ALLOC_ZERO 34
#define CMD_SDSP_SMEM_ALLOC 40
#define CMD_SDSP_SMEM_UNREF 41
#define CMD_SDSP_SMEM_ENABLE 42
#define CMD_SDSP_SMEM_DISABLE 43
#define CMD_SDSP_SMEM_ALLOC_ZERO 44
#define CMD_SEC_MEM_INVOKE_CMD_START 100
#define CMD_SEC_MEM_SET_PROT_REGION CMD_SEC_MEM_INVOKE_CMD_START
#define CMD_SEC_MEM_DUMP_MEM_INFO 101
#define CMD_SEC_MEM_DYNAMIC_DEBUG_CONFIG 102
#define CMD_WFD_SMEM_DUMP_MEM_INFO 103
#define CMD_SEC_MEM_FORCE_HW_PROTECTION 104
#define CMD_SEC_MEM_SET_MCHUNKS_REGION 105
#define CMD_SEC_MEM_INVOKE_CMD_END 105
#define CMD_SEC_MEM_DUMP_INFO 255
#define CMD_SEC_MEM_INVALID (0xFFFFFFFF)

enum TEE_OP {
	TEE_OP_ALLOC = 0,
	TEE_OP_ALLOC_ZERO = 1,
	TEE_OP_FREE = 2,
	TEE_OP_REGION_ENABLE = 3,
	TEE_OP_REGION_DISABLE = 4,
	TEE_OP_MAX = 5,
};

enum TEE_MEM_TYPE {
	TEE_MEM_SVP = 0,
	TEE_MEM_WFD = 1,
	TEE_MEM_SDSP_SHARED = 2,
};

struct tee_op_cmd_mappings {
	enum TEE_OP tee_cmds[TEE_OP_MAX];
	enum TEE_MEM_TYPE tee_mem_type;
};

u32 get_tee_cmd(enum TEE_OP op, void *peer_priv);
u32 get_tee_mem_type(void *peer_priv);
void get_tee_peer_priv_data(enum TEE_MEM_TYPE tee_mem_type, void **peer_priv);

#endif /* TEE_COMMON_H_ */
