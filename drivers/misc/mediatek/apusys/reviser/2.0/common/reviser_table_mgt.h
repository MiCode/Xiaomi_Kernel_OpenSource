/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_MEM_MGT_H__
#define __APUSYS_REVISER_MEM_MGT_H__
#include <linux/types.h>
#include <linux/bitmap.h>


#include "reviser_mem_def.h"


/*TCM Resource*/
struct pgt_tcm {
	//unsigned long pgt[BITS_TO_LONGS(VLM_TCM_BANK_MAX)];
	//unsigned long pgt[1];
	unsigned long *pgt;
	uint32_t page_num;
};
/*VLM Resource*/
struct pgt_vlm {
	struct pgt_tcm tcm;
	uint32_t page_num; //Request Page Size
	uint32_t sys_num; // Sys-Ram Number (ex: TCM, SLB)
};
/*VLM Bank remap table*/
struct bank_vlm {
	enum REVISER_MEM_TYPE type;
	uint8_t dst;
	uint8_t vlm; //vlm table index
};
/*ctx memory info*/
struct ctx_pgt {
	//struct bank_vlm bank[VLM_DRAM_BANK_MAX]; //src-dst page_table
	struct bank_vlm *bank; //src-dst page_table
	struct pgt_vlm vlm;
};

/*remap info*/
struct rmp_rule {
	uint8_t valid;
	uint8_t src;
	uint8_t dst;
	uint32_t ctx;
};

/*remap */
struct rmp_table {
	//unsigned long valid[BITS_TO_LONGS(VLM_REMAP_TABLE_MAX)];
	unsigned long *valid;
	//struct rmp_rule remap[VLM_REMAP_TABLE_MAX];
	struct rmp_rule *remap;
};

/* contex id */
int reviser_table_init_ctx(void *drvinfo);
int reviser_table_uninit_ctx(void *drvinfo);
int reviser_table_get_ctx_sync(void *drvinfo, unsigned long *ctx);
int reviser_table_get_ctx(void *drvinfo, unsigned long *ctx);
int reviser_table_free_ctx(void *drvinfo, unsigned long ctx);
void reviser_table_print_ctx(void *drvinfo, void *s_file);

/* tcm */
int reviser_table_init_tcm(void *drvinfo);
int reviser_table_uninit_tcm(void *drvinfo);
int reviser_table_get_tcm_sync(void *drvinfo,
		uint32_t page_num, struct pgt_tcm *pg_table);
int reviser_table_get_tcm(void *drvinfo,
		uint32_t tcm_size, struct pgt_tcm *pg_table);
int reviser_table_free_tcm(void *drvinfo, struct pgt_tcm *pg_table);
void reviser_table_print_tcm(void *drvinfo, void *s_file);

/* vlm */
int reviser_table_init_ctx_pgt(void *drvinfo);
int reviser_table_uninit_ctx_pgt(void *drvinfo);
int reviser_table_get_vlm(void *drvinfo,
		uint32_t page_size, bool force,
		unsigned long *id, uint32_t *tcm_size);
int reviser_table_free_vlm(void *drvinfo, uint32_t ctx);
void reviser_table_print_vlm(void *drvinfo, uint32_t ctx, void *s_file);

/* remap table */
int reviser_table_init_remap(void *drvinfo);
int reviser_table_uninit_remap(void *drvinfo);
int reviser_table_set_remap(void *drvinfo, unsigned long ctx);
int reviser_table_clear_remap(void *drvinfo, unsigned long ctx);

/* init all table*/
int reviser_table_init(void *drvinfo);
int reviser_table_uninit(void *drvinfo);

int reviser_table_get_pool_index(uint32_t type, uint32_t *index);
#endif
