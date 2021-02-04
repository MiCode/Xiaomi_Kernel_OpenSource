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

#ifndef TMEM_PRIV_H
#define TMEM_PRIV_H

#include "private/tmem_device.h"

struct trusted_mem_device *
create_trusted_mem_device(enum TRUSTED_MEM_TYPE register_type,
			  struct trusted_mem_configs *cfg);
void destroy_trusted_mem_device(struct trusted_mem_device *tmem_device);
int register_trusted_mem_device(enum TRUSTED_MEM_TYPE register_type,
				struct trusted_mem_device *tmem_device);
void trusted_mem_ut_cmd_invoke(u64 cmd, u64 param1, u64 param2, u64 param3);
struct trusted_mem_device *
get_trusted_mem_device(enum TRUSTED_MEM_TYPE mem_type);
struct trusted_mem_device *create_and_register_shared_trusted_mem_device(
	enum TRUSTED_MEM_TYPE mem_type, struct trusted_mem_device *tmem_device,
	char *dev_name);

void get_ssmr_ops(struct ssmr_operations **ops);
struct peer_mgr_desc *create_peer_mgr_desc(void);
struct region_mgr_desc *
create_reg_mgr_desc(enum TRUSTED_MEM_TYPE register_type,
		    struct trusted_mem_device *mem_device);

void regmgr_region_ref_inc(struct region_mgr_desc *mgr_desc,
			   enum TRUSTED_MEM_TYPE try_mem_type);
void regmgr_region_ref_dec(struct region_mgr_desc *mgr_desc);
bool is_regmgr_region_on(struct region_mgr_desc *mgr_desc);
u64 get_regmgr_region_online_cnt(struct region_mgr_desc *mgr_desc);
u32 get_regmgr_region_ref_cnt(struct region_mgr_desc *mgr_desc);
int regmgr_online(struct region_mgr_desc *mgr_desc,
		  enum TRUSTED_MEM_TYPE try_mem_type);
int regmgr_offline(struct region_mgr_desc *mgr_desc);
bool get_device_busy_status(struct trusted_mem_device *mem_device);
bool is_mtee_mchunks(enum TRUSTED_MEM_TYPE mem_type);

#ifdef TCORE_PROFILING_SUPPORT
struct profile_mgr_desc *create_profile_mgr_desc(void);
void trusted_mem_core_profile_dump(struct trusted_mem_device *mem_device);
#endif

#if defined(CONFIG_MTK_SVP_DISABLE_SODI)
void spm_enable_sodi(bool en);
#endif

#endif /* end of TMEM_PRIV_H */
