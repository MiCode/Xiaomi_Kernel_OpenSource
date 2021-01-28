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

#ifndef TMEM_UT_COMMON_H
#define TMEM_UT_COMMON_H

#include "private/tmem_device.h"

#define REGMGR_REGION_FINAL_STATE_ON (1)
#define REGMGR_REGION_FINAL_STATE_OFF (0)

#define MEM_UNORDER_SIZE_TEST_CFG_ENABLE (1)
#define MEM_UNORDER_SIZE_TEST_CFG_DISABLE (0)

#define MEM_REGION_ON_OFF_STREE_ROUND (100ULL)

#define MULTIPLE_REGION_MULTIPLE_THREAD_TEST_ENABLE (0)
#define MTEE_MCHUNKS_MULTIPLE_THREAD_TEST_ENABLE (1)

enum UT_RET_STATE all_regmgr_state_off_check(void);
enum UT_RET_STATE mem_basic_test(enum TRUSTED_MEM_TYPE mem_type,
				 int region_final_state);
enum UT_RET_STATE mem_alloc_simple_test(enum TRUSTED_MEM_TYPE mem_type,
					u8 *mem_owner, int region_final_state,
					int un_order_sz_cfg);
enum UT_RET_STATE mem_alloc_alignment_test(enum TRUSTED_MEM_TYPE mem_type,
					   u8 *mem_owner,
					   int region_final_state);
enum UT_RET_STATE mem_handle_list_init(enum TRUSTED_MEM_TYPE mem_type);
enum UT_RET_STATE mem_handle_list_deinit(void);
enum UT_RET_STATE mem_alloc_saturation_test(enum TRUSTED_MEM_TYPE mem_type,
					    u8 *mem_owner,
					    int region_final_state, int round);
enum UT_RET_STATE
mem_regmgr_region_defer_off_test(enum TRUSTED_MEM_TYPE mem_type, u8 *mem_owner,
				 int region_final_state);
enum UT_RET_STATE
mem_regmgr_region_online_count_test(enum TRUSTED_MEM_TYPE mem_type,
				    u8 *mem_owner, int region_final_state);
enum UT_RET_STATE mem_region_on_off_stress_test(enum TRUSTED_MEM_TYPE mem_type,
						int region_final_state,
						int round);
enum UT_RET_STATE mem_alloc_multithread_test(enum TRUSTED_MEM_TYPE mem_type);
enum UT_RET_STATE mem_multi_type_alloc_multithread_test(void);
enum UT_RET_STATE mem_mtee_mchunks_alloc_multithread_test(void);
bool is_multi_type_alloc_multithread_test_locked(void);

#endif /* end of TMEM_UT_COMMON_H */
