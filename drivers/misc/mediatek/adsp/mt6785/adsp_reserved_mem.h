/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __ADSP_RESERVEDMEM_DEFINE_H__
#define __ADSP_RESERVEDMEM_DEFINE_H__

//#define FPGA_EARLY_DEVELOPMENT

/* adsp reserve memory ID definition*/
enum adsp_reserve_mem_id_t {
	ADSP_A_SYSTEM_MEM_ID, /*not for share, reserved for system*/
	ADSP_A_IPI_MEM_ID,
	ADSP_A_LOGGER_MEM_ID,
#ifndef FPGA_EARLY_DEVELOPMENT
	ADSP_A_TRAX_MEM_ID,
	ADSP_SPK_PROTECT_MEM_ID,
	ADSP_VOIP_MEM_ID,
	ADSP_A2DP_PLAYBACK_MEM_ID,
	ADSP_OFFLOAD_MEM_ID,
	ADSP_EFFECT_MEM_ID,
	ADSP_VOICE_CALL_MEM_ID,
	ADSP_AFE_MEM_ID,
	ADSP_PLAYBACK_MEM_ID,
	ADSP_DEEPBUF_MEM_ID,
	ADSP_PRIMARY_MEM_ID,
	ADSP_CAPTURE_UL1_MEM_ID,
	ADSP_DATAPROVIDER_MEM_ID,
	ADSP_CALL_FINAL_MEM_ID,
#endif
	ADSP_A_DEBUG_DUMP_MEM_ID,
	ADSP_A_CORE_DUMP_MEM_ID,
	ADSP_NUMS_MEM_ID,
	ADSP_A_SHARED_MEM_BEGIN = ADSP_A_IPI_MEM_ID,
	ADSP_A_SHARED_MEM_END = ADSP_A_CORE_DUMP_MEM_ID,
};

struct adsp_reserve_mblock {
	phys_addr_t phys_addr;
	void *virt_addr;
	size_t size;
};

/* Reserved Memory Method */
phys_addr_t adsp_get_reserve_mem_phys(enum adsp_reserve_mem_id_t id);
void *adsp_get_reserve_mem_virt(enum adsp_reserve_mem_id_t id);
size_t adsp_get_reserve_mem_size(enum adsp_reserve_mem_id_t id);
int adsp_set_reserve_mblock(
		enum adsp_reserve_mem_id_t id, phys_addr_t phys_addr,
		void *virt_addr, size_t size);
void *adsp_reserve_memory_ioremap(phys_addr_t phys_addr, size_t size);
ssize_t adsp_reserve_memory_dump(char *buffer, int size);

#endif /* __ADSP_RESERVEDMEM_DEFINE_H__ */
