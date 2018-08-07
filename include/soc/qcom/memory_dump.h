/* Copyright (c) 2012, 2014-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_MEMORY_DUMP_H
#define __MSM_MEMORY_DUMP_H

#include <linux/types.h>

enum dump_client_type {
	MSM_CPU_CTXT = 0,
	MSM_L1_CACHE,
	MSM_L2_CACHE,
	MSM_OCMEM,
	MSM_TMC_ETFETB,
	MSM_ETM0_REG,
	MSM_ETM1_REG,
	MSM_ETM2_REG,
	MSM_ETM3_REG,
	MSM_TMC0_REG, /* TMC_ETR */
	MSM_TMC1_REG, /* TMC_ETF */
	MSM_LOG_BUF,
	MSM_LOG_BUF_FIRST_IDX,
	MAX_NUM_CLIENTS,
};

struct msm_client_dump {
	enum dump_client_type id;
	unsigned long start_addr;
	unsigned long end_addr;
};

#ifdef CONFIG_QCOM_MEMORY_DUMP
extern int msm_dump_tbl_register(struct msm_client_dump *client_entry);
#else
static inline int msm_dump_tbl_register(struct msm_client_dump *entry)
{
	return -EIO;
}
#endif


#if defined(CONFIG_QCOM_MEMORY_DUMP) || defined(CONFIG_QCOM_MEMORY_DUMP_V2)
extern uint32_t msm_dump_table_version(void);
#else
static inline uint32_t msm_dump_table_version(void)
{
	return 0;
}
#endif

#define MSM_DUMP_MAKE_VERSION(ma, mi)	((ma << 20) | mi)
#define MSM_DUMP_MAJOR(val)		(val >> 20)
#define MSM_DUMP_MINOR(val)		(val & 0xFFFFF)


#define MAX_NUM_ENTRIES		0x150

enum msm_dump_data_ids {
	MSM_DUMP_DATA_CPU_CTX = 0x00,
	MSM_DUMP_DATA_L1_INST_CACHE = 0x60,
	MSM_DUMP_DATA_L1_DATA_CACHE = 0x80,
	MSM_DUMP_DATA_ETM_REG = 0xA0,
	MSM_DUMP_DATA_L2_CACHE = 0xC0,
	MSM_DUMP_DATA_L3_CACHE = 0xD0,
	MSM_DUMP_DATA_OCMEM = 0xE0,
	MSM_DUMP_DATA_CNSS_WLAN = 0xE1,
	MSM_DUMP_DATA_WIGIG = 0xE2,
	MSM_DUMP_DATA_PMIC = 0xE4,
	MSM_DUMP_DATA_DBGUI_REG = 0xE5,
	MSM_DUMP_DATA_DCC_REG = 0xE6,
	MSM_DUMP_DATA_DCC_SRAM = 0xE7,
	MSM_DUMP_DATA_MISC = 0xE8,
	MSM_DUMP_DATA_VSENSE = 0xE9,
	MSM_DUMP_DATA_RPM = 0xEA,
	MSM_DUMP_DATA_SCANDUMP = 0xEB,
	MSM_DUMP_DATA_RPMH = 0xEC,
	MSM_DUMP_DATA_TMC_ETF = 0xF0,
	MSM_DUMP_DATA_TMC_REG = 0x100,
	MSM_DUMP_DATA_TMC_ETR_REG = 0x100,
	MSM_DUMP_DATA_TMC_ETF_REG = 0x101,
	MSM_DUMP_DATA_LOG_BUF = 0x110,
	MSM_DUMP_DATA_LOG_BUF_FIRST_IDX = 0x111,
	MSM_DUMP_DATA_SCANDUMP_PER_CPU = 0x130,
	MSM_DUMP_DATA_LLCC_PER_INSTANCE = 0x140,
	MSM_DUMP_DATA_MAX = MAX_NUM_ENTRIES,
};

enum msm_dump_table_ids {
	MSM_DUMP_TABLE_APPS,
	MSM_DUMP_TABLE_MAX = MAX_NUM_ENTRIES,
};

enum msm_dump_type {
	MSM_DUMP_TYPE_DATA,
	MSM_DUMP_TYPE_TABLE,
};

struct msm_dump_data {
	uint32_t version;
	uint32_t magic;
	char name[32];
	uint64_t addr;
	uint64_t len;
	uint32_t reserved;
};

struct msm_dump_entry {
	uint32_t id;
	char name[32];
	uint32_t type;
	uint64_t addr;
};

struct dump_vaddr_entry {
	uint32_t id;
	void *dump_vaddr;
	struct msm_dump_data *dump_data_vaddr;
};

struct msm_mem_dump_vaddr_tbl {
	uint8_t num_node;
	struct dump_vaddr_entry *entries;
};

#ifdef CONFIG_QCOM_MEMORY_DUMP_V2
extern int msm_dump_data_register(enum msm_dump_table_ids id,
				  struct msm_dump_entry *entry);

extern struct dump_vaddr_entry *get_msm_dump_ptr(enum msm_dump_data_ids id);
#else
static inline int msm_dump_data_register(enum msm_dump_table_ids id,
					 struct msm_dump_entry *entry)
{
	return -EINVAL;
}
#endif

#endif
