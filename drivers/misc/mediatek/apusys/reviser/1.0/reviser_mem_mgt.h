/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_REVISER_MEM_MGT_H__
#define __APUSYS_REVISER_MEM_MGT_H__
#include <linux/types.h>
#include <linux/bitmap.h>

#include "reviser_mem_def.h"
#include "reviser_reg.h"

#define TABLE_CTXID_MAX VLM_CTXT_CTX_ID_MAX
//#define TABLE_CTXID_MAX (4)
#define TABLE_TCM_MAX VLM_TCM_BANK_MAX


enum REVISER_MEM_TYPE {
	/* memory type */
	REVISER_MEM_TYPE_DRAM = 0x0,
	REVISER_MEM_TYPE_TCM = 0x1,
	REVISER_MEM_TYPE_INFRA = 0x2,
	REVISER_MEM_TYPE_L3_CACHE = 0x3,
	REVISER_MEM_TYPE_MAX
};



struct table_tcm {
	unsigned long table_tcm[BITS_TO_LONGS(TABLE_TCM_MAX)];
	uint32_t page_num;
};

struct table_vlm {
	struct table_tcm tcm_pgtable;
	uint32_t page_num; //Request Page Size
};

struct table_remap_mem {
	uint8_t src;
	uint8_t dst;
	uint32_t ctxid;
};

struct table_remap {
	unsigned long valid[BITS_TO_LONGS(VLM_REMAP_TABLE_MAX)];
	struct table_remap_mem table_remap_mem[VLM_REMAP_TABLE_MAX];
};
struct vlm_pgtable_mem {
	uint8_t valid;
	enum REVISER_MEM_TYPE type;
	uint8_t dst;
	uint8_t vlm; //vlm table index
};

struct vlm_pgtable {
	struct vlm_pgtable_mem page[VLM_REMAP_TABLE_MAX]; //src-dst page_table
	uint32_t sys_page_num; // Sys-Ram Number (ex: TCM, Infra-RAM)
	uint32_t page_num; // Valid page
	struct table_tcm tcm;
	uint64_t swap_addr;
};

/* contex id */
int reviser_table_init_ctxID(void *drvinfo);
int reviser_table_get_ctxID_sync(void *drvinfo, unsigned long *ctxID);
int reviser_table_get_ctxID(void *drvinfo, unsigned long *ctxID);
int reviser_table_free_ctxID(void *drvinfo, unsigned long ctxID);
void reviser_table_print_ctxID(void *drvinfo, void *s_file);

/* tcm */
int reviser_table_init_tcm(void *drvinfo);
int reviser_table_get_tcm_sync(void *drvinfo,
		uint32_t page_num, struct table_tcm *pg_table);
int reviser_table_get_tcm(void *drvinfo,
		uint32_t tcm_size, struct table_tcm *pg_table);
int reviser_table_free_tcm(void *drvinfo, struct table_tcm *pg_table);
void reviser_table_print_tcm(void *drvinfo, void *s_file);

/* vlm */
int reviser_table_init_vlm(void *drvinfo);
int reviser_table_get_vlm(void *drvinfo,
		uint32_t page_size, bool force,
		unsigned long *id, uint32_t *tcm_size);
int reviser_table_free_vlm(void *drvinfo, uint32_t ctxid);
void reviser_table_print_vlm(void *drvinfo, uint32_t ctxid, void *s_file);

/* remap table */
int reviser_table_init_remap(void *drvinfo);
int reviser_table_set_remap(void *drvinfo, unsigned long ctxid);
int reviser_table_clear_remap(void *drvinfo, unsigned long ctxid);

/* swap */
int reviser_table_swapout_vlm(void *drvinfo, unsigned long ctxid);
int reviser_table_swapin_vlm(void *drvinfo, unsigned long ctxid);

#endif
