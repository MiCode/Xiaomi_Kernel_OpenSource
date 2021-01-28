/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_RESERVEDMEM_DEFINE_H__
#define __ADSP_RESERVEDMEM_DEFINE_H__

/* emi mpu define*/
#define MPU_PROCT_REGION_ADSP_SHARED      30
#define MPU_PROCT_D0_AP                   0
#define MPU_PROCT_D10_ADSP                10

/* adsp reserve memory ID definition*/
enum adsp_reserve_mem_id_t {
	ADSP_A_IPI_DMA_MEM_ID = 0,
	ADSP_B_IPI_DMA_MEM_ID,
	ADSP_A_LOGGER_MEM_ID,
	ADSP_B_LOGGER_MEM_ID,
#ifndef CONFIG_FPGA_EARLY_PORTING
	ADSP_AUDIO_COMMON_MEM_ID,
	ADSP_OFFLOAD_MEM_ID,
	ADSP_CALL_FINAL_MEM_ID,
	ADSP_PHONE_CALL_MEM_ID,
#endif
	ADSP_C2C_MEM_ID,
	ADSP_A_DEBUG_DUMP_MEM_ID,
	ADSP_B_DEBUG_DUMP_MEM_ID,
	ADSP_A_CORE_DUMP_MEM_ID,
	ADSP_B_CORE_DUMP_MEM_ID,
	ADSP_NUMS_MEM_ID,
};

struct adsp_reserve_mblock {
	phys_addr_t phys_addr;
	void *virt_addr;
	size_t size;
};

struct adsp_mpu_info_t {
	u32 share_dram_addr;
	u32 share_dram_size;
};

struct adsp_priv;

/* Reserved Memory Method */
phys_addr_t adsp_get_reserve_mem_phys(enum adsp_reserve_mem_id_t id);
void *adsp_get_reserve_mem_virt(enum adsp_reserve_mem_id_t id);
size_t adsp_get_reserve_mem_size(enum adsp_reserve_mem_id_t id);
void adsp_init_reserve_memory(void);
ssize_t adsp_reserve_memory_dump(char *buffer, int size);

void adsp_set_emimpu_shared_region(void);
void adsp_update_mpu_memory_info(struct adsp_priv *pdata);

#endif /* __ADSP_RESERVEDMEM_DEFINE_H__ */
