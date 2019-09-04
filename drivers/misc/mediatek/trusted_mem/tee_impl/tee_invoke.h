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

#ifndef TEE_INVOKE_H_
#define TEE_INVOKE_H_

int tee_directly_invoke_cmd(struct trusted_driver_cmd_params *invoke_params);

#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
int secmem_fr_set_prot_shared_region(u64 pa, u32 size, int remote_region_type);
int secmem_fr_dump_info(void);
#endif

#if defined(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)
int secmem_set_mchunks_region(u64 pa, u32 size, int remote_region_type);
#endif

#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)
int secmem_svp_dump_info(void);
int secmem_dynamic_debug_control(bool enable_dbg);
int secmem_force_hw_protection(void);
#endif

#if defined(CONFIG_MTK_WFD_SMEM_SUPPORT)
int wfd_smem_dump_info(void);
#endif

#endif /* TEE_INVOKE_H_ */
