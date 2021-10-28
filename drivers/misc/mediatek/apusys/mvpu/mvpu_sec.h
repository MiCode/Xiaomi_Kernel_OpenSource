/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>

#ifndef MVPU_SECURITY
#define MVPU_SECURITY
#endif

//#define MVPU_SEC_DEBUG
//#define MVPU_SEC_DEBUG_ADV
//#define MVPU_SEC_DEBUG_RP_INFO

#ifdef MVPU_SECURITY

//mvpu_algo.img
//void * mvpu_algo_img;
static dma_addr_t mvpu_algo_iova;

static uint32_t *mvpu_algo_img;
//static uint32_t mvpu_algo_iova = 0;

static uint32_t ker_img_offset;
//static uint32_t ker_img_iova;

#define IMG_HEADER_SIZE 4
#define PTN_INFO_SIZE 2
#define KER_INFO_SIZE 3

#define PNT_SIZE_OFFSET 1
#define KER_SIZE_OFFSET 1
#define KER_NUM_OFFSET 2
#define KER_BIN_INFO_SIZE 2

// mem pool use
#define MAX_SAVE_SESSION  5
#define MAX_SAVE_HASH    10

static void *saved_session[MAX_SAVE_SESSION];
static uint32_t sess_oldest;

// HASH
static struct mvpu_hash_pool *hash_pool[MAX_SAVE_SESSION];

struct mvpu_hash_pool {
	uint32_t buf_num;
	uint32_t hash_list[MAX_SAVE_HASH];
	uint32_t hash_oldest;

	struct dma_heap *dma_heap[MAX_SAVE_HASH];
	struct dma_buf *hash_dma_buf[MAX_SAVE_HASH];
	struct dma_buf_attachment *attach[MAX_SAVE_HASH];
	struct sg_table *sgt[MAX_SAVE_HASH];

	uint32_t hash_base_iova[MAX_SAVE_HASH];
	uint64_t hash_base_kva[MAX_SAVE_HASH];
	uint32_t *hash_offset[MAX_SAVE_HASH];
};

// image
uint32_t get_ptn_total_size(void);
uint32_t get_ptn_size(uint32_t hash);
uint32_t get_ker_img_offset(void);

//bool get_ker_info(uint32_t hash, uint32_t *ker_bin_offset, uint32_t *ker_size, uint32_t *ker_bin_num);
bool get_ker_info(uint32_t hash, uint32_t *ker_bin_offset, uint32_t *ker_bin_num);
void set_ker_iova(uint32_t ker_bin_offset, uint32_t ker_bin_num, uint32_t *ker_bin_each_iova);

// buf map
void map_base_buf_id(uint32_t buf_num, uint32_t *sec_chk_addr,
				uint32_t rp_num, uint32_t *target_map, uint32_t *target_base);

// mem pool
uint32_t get_saved_session_id(void *session);
uint32_t get_avail_session_id(void);

void clear_session(void *session);

void update_session_id(uint32_t session_id, void *session);

uint32_t get_saved_hash_id(uint32_t session_id, uint32_t batch_name_hash);
uint32_t get_avail_hash_id(uint32_t session_id);

void clear_hash(uint32_t session_id, uint32_t hash_id);
void free_all_hash(uint32_t session_id);

int update_hash_pool(void *session, bool algo_in_img,
				uint32_t session_id, uint32_t hash_id, uint32_t batch_name_hash,
				uint32_t buf_num, uint32_t *sec_chk_addr,
				uint32_t *sec_buf_size, uint32_t *mem_is_kernel);

// replacement
int update_new_base_addr(bool algo_in_img,
				uint32_t session_id, uint32_t hash_id,
				uint32_t *sec_chk_addr, uint32_t *mem_is_kernel, uint32_t rp_num,
				uint32_t *target_buf_new_map, uint32_t *target_buf_new_base,
				uint32_t ker_bin_num, uint32_t *ker_bin_each_iova,
				void *kreg_kva);

int replace_mem(uint32_t session_id, uint32_t hash_id, uint32_t rp_num,
				uint32_t *target_buf_old_map,
				uint32_t *target_buf_old_base, uint32_t *target_buf_old_offset,
				uint32_t *target_buf_new_base, uint32_t *target_buf_new_offset,
				void *kreg_kva);

int mvpu_load_img(struct device *dev);

int mvpu_sec_init(struct device *dev);

#endif
