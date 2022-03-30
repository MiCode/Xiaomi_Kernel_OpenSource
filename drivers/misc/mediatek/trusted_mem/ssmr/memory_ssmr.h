/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MEMORY_SSMR_H__
#define __MEMORY_SSMR_H__

#include <linux/platform_device.h>
#include "../private/tmem_device.h"

#define NAME_SIZE 32

enum ssmr_feature_type {
	SSMR_FEAT_SVP_REGION = TRUSTED_MEM_SVP_REGION,
	SSMR_FEAT_PROT_REGION = TRUSTED_MEM_PROT_REGION,
	SSMR_FEAT_WFD_REGION = TRUSTED_MEM_WFD_REGION,
	SSMR_FEAT_TA_ELF = TRUSTED_MEM_HAPP,
	SSMR_FEAT_TA_STACK_HEAP = TRUSTED_MEM_HAPP_EXTRA,
	SSMR_FEAT_SDSP_FIRMWARE = TRUSTED_MEM_SDSP,
	SSMR_FEAT_SDSP_TEE_SHAREDMEM = TRUSTED_MEM_SDSP_SHARED,
	SSMR_FEAT_2D_FR = TRUSTED_MEM_2D_FR,
	SSMR_FEAT_TUI = TRUSTED_MEM_TUI,
	SSMR_FEAT_SVP_PAGE = TRUSTED_MEM_SVP_PAGE,
	SSMR_FEAT_PROT_PAGE = TRUSTED_MEM_PROT_PAGE,
	SSMR_FEAT_WFD_PAGE = TRUSTED_MEM_WFD_PAGE,
	SSMR_FEAT_SAPU_DATA_SHM = TRUSTED_MEM_SAPU_DATA_SHM,
	SSMR_FEAT_SAPU_ENGINE_SHM = TRUSTED_MEM_SAPU_ENGINE_SHM,

	__MAX_NR_SSMR_FEATURES,
};

int ssmr_offline(phys_addr_t *pa, unsigned long *size, bool is_64bit,
		 unsigned int feat);
int ssmr_online(unsigned int feat);
int ssmr_query_total_sec_heap_count(void);
int ssmr_query_heap_info(int heap_index, char *heap_name);
int ssmr_init(struct platform_device *pdev);

bool is_page_based_memory(enum TRUSTED_MEM_TYPE mem_type);
bool is_svp_on_mtee(void);
bool is_svp_enabled(void);

#endif
