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

#ifdef MVPU_SECURITY

//#define MVPU_SEC_BLOCK_WRONG_BUF_INFO   // Forbidden (buf_num == 0 || rp_num == 0)

#define MVPU_SEC_IMPROVE_MAP // improve info mapping

//function switches for debug
#define MVPU_SEC_USE_MEM_POOL   // Use mem pool instead of user's buf
#define MVPU_SEC_KREG_IN_POOL   // Copy CMD buf KREG part to mem pool

//#define MVPU_SEC_MPU_NUM_BLOCK  // Forbidden buffer num > MVPU_MPU_SEGMENT_NUMS
#define MVPU_SEC_UPDT_MPU       // Use mem pool's MPU region setting instead of engine's setting
#define MVPU_SEC_CLEAR_MPU      // Clear All MPU settings

//#define MVPU_SEC_USE_OLDEST_SESSION_ID //TBD: clear and use oldest session id
#define MVPU_SEC_USE_OLDEST_HASH_ID    //TBD: clear and use oldest hash id

#define MVPU_SEC_BLOCK_EDMA_KERNEL   // Forbidden edma in kernel code
// #define MVPU_SEC_BLOCK_EDMA_KERNEL_RETURN // FIXME: block jobs to use edma in kernel code
#define RT_BATCH_KERNEL_USING_EDMA  (1 << 27)
#define RT_BATCH_HASH_EXEC_CNT      ((1 << 31) | (1 << 30) | (1 << 29) | (1 << 28))
#define MVPU_BATCH_MASK             0x07FFFFFF
#define MVPU_ONLINE_BATCH_NAME_HASH 0xAAAAAAAA
#define MVPU_CMD_BUFF_ADDR          0x00000000

//mvpu_algo.img
//void * mvpu_algo_img;
static dma_addr_t mvpu_algo_iova;

static uint32_t *mvpu_algo_img;
static uint32_t ker_img_offset;

static uint32_t ptn_img_size;
static uint32_t knl_img_size;

static bool mvpu_algo_available;

static int mvpu_loglvl_sec;

//image headers
#define IMG_HEADER_SIZE 4
#define PTN_INFO_SIZE   2
#define KER_INFO_SIZE   3

#define PNT_SIZE_OFFSET     1
#define KER_SIZE_OFFSET     1
#define KER_NUM_OFFSET      2
#define KER_BIN_SIZE_OFFSET 3
#define KER_BIN_INFO_SIZE   2

#define MVPU_ADDR_ALIGN  128
#define MVPU_MPU_SIZE   4096

enum buffer_attr {
	BUF_NORMAL = 0,
	BUF_KERNEL,
	BUF_IO,
	BUF_RINGBUFFER,
};

// mem pool use
#define MAX_SAVE_SESSION  64
#define MAX_SAVE_HASH    128

static uint64_t saved_session[MAX_SAVE_SESSION];
static uint32_t sess_oldest;

// HASH
static struct mvpu_hash_pool *hash_pool[MAX_SAVE_SESSION];

struct mvpu_hash_pool {
	uint32_t buf_num[MAX_SAVE_HASH];
	uint32_t rp_num[MAX_SAVE_HASH];
	uint32_t hash_list[MAX_SAVE_HASH];
	uint32_t hash_oldest;

	struct dma_heap *dma_heap[MAX_SAVE_HASH];
	struct dma_buf *hash_dma_buf[MAX_SAVE_HASH];
	struct dma_buf_attachment *attach[MAX_SAVE_HASH];
	struct sg_table *sgt[MAX_SAVE_HASH];

	uint32_t hash_base_iova[MAX_SAVE_HASH];
	uint64_t hash_base_kva[MAX_SAVE_HASH];
	uint32_t hash_pool_size[MAX_SAVE_HASH];

	uint32_t *sec_chk_addr[MAX_SAVE_HASH];

#ifdef FULL_RP_INFO
	uint32_t *target_buf_old_base[MAX_SAVE_HASH];
	uint32_t *target_buf_old_offset[MAX_SAVE_HASH];
	uint32_t *target_buf_new_base[MAX_SAVE_HASH];
	uint32_t *target_buf_new_offset[MAX_SAVE_HASH];
	uint32_t *target_buf_old_map[MAX_SAVE_HASH];
	uint32_t *target_buf_new_map[MAX_SAVE_HASH];
#endif

	uint32_t *hash_offset[MAX_SAVE_HASH];
};

#define ITCM_VIRTUAL_BASE              0x19600000
#define ITCM_VIRTUAL_BASE_END          0x19620000
#define VIRTUAL_MVPU_TCM_BASE          0x19600000
#define VIRTUAL_MVPU_TCM_BASE_END      0x19700000
#define VIRTUAL_APUSYS_TCM_BASE        0x02000000
#define VIRTUAL_APUSYS_TCM_BASE_END    0x03000000

enum mpu_region_shift {
	ITCM_BASE_SFT = 0,
	ITCM_END_SFT,
	TCM_BASE_SFT,
	TCM_END_SFT,
	IMG_BASE_SFT,
	IMG_END_SFT,
	POOL_BASE_SFT,
	POOL_END_SFT,
	PMU_BASE_SFT,
	PMU_END_SFT,
	SYS_BUF_NUM,
};

enum KREG_OFFSET {
	KREG_OFST_18 = 18,

	KREG_OFST_26 = 26,
	KREG_OFST_28 = 28,

	KREG_OFST_END = 360,
};

enum DESC_TYPE {
	DESC_TYPE_NONE = 0,
	DESC_TYPE_GLSU = 1,
	DESC_TYPE_EDMA = 2,
	DESC_TYPE_END = 3,
};


void set_sec_log_lvl(int log_lvl);

// image
bool get_mvpu_algo_available(void);
uint32_t get_ptn_total_size(void);
uint32_t get_ptn_size(uint32_t hash);
bool get_ptn_hash(uint32_t hash);
uint32_t get_kerbin_total_size(void);
uint32_t get_ker_img_offset(void);

void get_ker_info(uint32_t hash, uint32_t *ker_bin_offset, uint32_t *ker_bin_num);
void set_ker_iova(uint32_t ker_bin_offset, uint32_t ker_bin_num, uint32_t *ker_bin_each_iova);

// buf map
void map_base_buf_id(uint32_t buf_num,
					uint32_t *sec_chk_addr,
					uint32_t *mem_is_kernel,
					uint32_t rp_num,
					uint32_t *target_old_map,
					uint32_t *target_old_base,
					uint32_t *target_new_map,
					uint32_t *target_new_base,
					uint32_t buf_cmd_kreg,
					uint32_t buf_cmd_next);

// mem pool
uint32_t get_saved_session_id(void *session);
uint32_t get_avail_session_id(void);

void clear_session(void *session);

void update_session_id(uint32_t session_id, void *session);

uint32_t get_saved_hash_id(uint32_t session_id, uint32_t batch_name_hash);
uint32_t get_avail_hash_id(uint32_t session_id);

void clear_hash(uint32_t session_id, uint32_t hash_id);
void free_all_hash(uint32_t session_id);

int update_hash_pool(void *session,
							bool algo_in_img,
							uint32_t session_id,
							uint32_t hash_id,
							uint32_t batch_name_hash,
							uint32_t buf_num,
							void *kreg_kva,
							uint32_t *sec_chk_addr,
							uint32_t *sec_buf_size,
							uint32_t *sec_buf_attr);

#ifdef FULL_RP_INFO
int save_hash_info(uint32_t session_id,
						uint32_t hash_id,
						uint32_t buf_num,
						uint32_t rp_num,
						uint32_t *sec_chk_addr,
						uint32_t *target_buf_old_base,
						uint32_t *target_buf_old_offset,
						uint32_t *target_buf_new_base,
						uint32_t *target_buf_new_offset,
						uint32_t *target_buf_old_map,
						uint32_t *target_buf_new_map);
#else
int save_hash_info(uint32_t session_id,
						uint32_t hash_id,
						uint32_t buf_num,
						uint32_t *sec_chk_addr);
#endif

bool get_hash_info(void *session,
						uint32_t batch_name_hash,
						uint32_t *session_id,
						uint32_t *hash_id,
						uint32_t buf_num);

int replace_img_knl(void *session,
					uint32_t buf_num,
					uint32_t *sec_chk_addr,
					uint32_t *sec_buf_attr,
					uint32_t rp_num,
					uint32_t *target_buf_old_map,
					uint32_t *target_buf_old_base,
					uint32_t *target_buf_old_offset,
					uint32_t *target_buf_new_map,
					uint32_t *target_buf_new_base,
					uint32_t *target_buf_new_offset,
					uint32_t ker_bin_num,
					uint32_t *ker_bin_each_iova);

bool set_rp_skip_buf(uint32_t session_id,
						uint32_t hash_id,
						uint32_t buf_num,
						uint32_t *sec_chk_addr,
						uint32_t *mem_is_kernel,
						uint32_t *rp_skip_buf);


// replacement
int update_new_base_addr(bool algo_in_img,
						bool algo_in_pool,
						uint32_t session_id,
						uint32_t hash_id,
						uint32_t *sec_chk_addr,
						uint32_t *mem_is_kernel,
						uint32_t *rp_skip_buf,
						uint32_t rp_num,
						uint32_t *target_buf_new_map,
						uint32_t *target_buf_new_base,
						uint32_t ker_bin_num,
						uint32_t *ker_bin_each_iova,
						void *kreg_kva);

int replace_mem(uint32_t session_id,
					uint32_t hash_id,
					uint32_t *mem_is_kernel,
					bool algo_in_pool,
					uint32_t *rp_skip_buf,
					uint32_t rp_num,
					uint32_t *target_buf_old_map,
					uint32_t *target_buf_old_base,
					uint32_t *target_buf_old_offset,
					uint32_t *target_buf_new_map,
					uint32_t *target_buf_new_base,
					uint32_t *target_buf_new_offset,
					void *kreg_kva);

int replace_kerarg(void *session,
					uint32_t session_id,
					uint32_t hash_id,
					uint32_t kerarg_num,
					uint32_t *sec_chk_addr,
					uint32_t *kerarg_buf_id,
					uint32_t *kerarg_offset,
					uint32_t *kerarg_size,
					uint32_t primem_num,
					uint32_t *primem_src_buf_id,
					uint32_t *primem_dst_buf_id,
					uint32_t *primem_src_offset,
					uint32_t *primem_dst_offset,
					uint32_t *primem_size);

void get_pool_kreg_iova(uint32_t *kreg_iova_pool,
						uint32_t session_id,
						uint32_t hash_id,
						uint32_t buf_cmd_kreg);

int add_img_mpu(void *mvpu_cmd);

int update_mpu(void *mvpu_cmd,
					uint32_t session_id,
					uint32_t hash_id,
					uint32_t *sec_chk_addr,
					uint32_t *sec_buf_size,
					uint32_t *sec_buf_attr,
					bool protect_phase);

bool mem_use_iova(uint32_t addr);

int check_iova(void *session,
					void *cmd,
					uint32_t desc_type,
					uint32_t chk_num,
					uint32_t chk_base,
					uint32_t chk_size);

int check_batch_flow(void *session,
						void *cmd,
						uint32_t sec_level,
						uint32_t *kreg_kva,
						uint32_t knl_num);

int mvpu_load_img(struct device *dev);

int mvpu_sec_init(struct device *dev);

#endif
