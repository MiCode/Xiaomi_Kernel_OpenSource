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
	SSMR_FEAT_SVP = TRUSTED_MEM_SVP,
	SSMR_FEAT_PROT_SHAREDMEM = TRUSTED_MEM_PROT,
	SSMR_FEAT_WFD = TRUSTED_MEM_WFD,
	SSMR_FEAT_TA_ELF = TRUSTED_MEM_HAPP,
	SSMR_FEAT_TA_STACK_HEAP = TRUSTED_MEM_HAPP_EXTRA,
	SSMR_FEAT_SDSP_FIRMWARE = TRUSTED_MEM_SDSP,
	SSMR_FEAT_SDSP_TEE_SHAREDMEM = TRUSTED_MEM_SDSP_SHARED,
	SSMR_FEAT_2D_FR = TRUSTED_MEM_2D_FR,
	SSMR_FEAT_TUI,
	__MAX_NR_SSMR_FEATURES,
};

int ssmr_offline(phys_addr_t *pa, unsigned long *size, bool is_64bit,
		 unsigned int feat);
int ssmr_online(unsigned int feat);
int ssmr_query_total_sec_heap_count(void);
int ssmr_query_heap_info(int heap_index, char *heap_name);
int ssmr_probe(struct platform_device *pdev);

#endif
