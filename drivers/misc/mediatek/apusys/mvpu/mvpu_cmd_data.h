/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MVPU_CMD_DATA_H__
#define __MVPU_CMD_DATA_H__

#define MVPU_PE_NUM  64
#define MVPU_DUP_BUF_SIZE  (2 * MVPU_PE_NUM)

#define MVPU_REQUEST_NAME_SIZE 32
#define MVPU_MPU_SEGMENT_NUMS  39

#define MVPU_MIN_CMDBUF_NUM     2
#define MVPU_CMD_INFO_IDX       0
#define MVPU_CMD_KREG_BASE_IDX  1

#define MVPU_CMD_LITE_SIZE_0 0x12E
#define MVPU_CMD_LITE_SIZE_1 0x14A

#ifndef MVPU_SECURITY
#define MVPU_SECURITY
#endif

#ifdef MVPU_SECURITY
#define BUF_NUM_MASK     0x0000FFFF

#define KERARG_NUM_MASK  0x3FFF0000
#define KERARG_NUM_SHIFT 16

#define SEC_LEVEL_MASK   0xC0000000
#define SEC_LEVEL_SHIFT  30

enum MVPU_SEC_LEVEL {
	SEC_LVL_CHECK = 0,
	SEC_LVL_CHECK_ALL = 1,
	SEC_LVL_PROTECT = 2,
	SEC_LVL_END,
};
#endif

struct BundleHeader {
	union {
		unsigned int dwValue;
		struct {
			unsigned int kreg_bundle_thread_mode_cfg : 8;
			unsigned int kreg_bundle_high_priority : 1;
			unsigned int kreg_bundle_interrupt_en : 1;
			unsigned int kreg_0x0000_rsv0	  : 2;
			unsigned int kreg_kernel_thread_mode : 2;
			unsigned int kreg_0x0000_rsv1	  : 2;
			unsigned int kreg_kernel_num	  : 16;
		} s;
	} reg_bundle_setting_0;

	union {
		unsigned int dwValue;
		struct {
			unsigned int kreg_bundle_cfg_base : 32;
		} s;
	} reg_bundle_setting_1;

	union {
		unsigned int dwValue;
		struct {
			unsigned int kreg_bundle_event_id : 32;
		} s;
	} reg_bundle_setting_2;

	union {
		unsigned int dwValue;
		struct {
			unsigned int kreg_kernel_start_cnt : 16;
			unsigned int kreg_bundle_skip_dma_num : 4;
		} s;
	} reg_bundle_setting_3;
};


/* MVPU command structure*/
struct mvpu_request {
	struct BundleHeader header;
	uint32_t algo_id;
	char name[MVPU_REQUEST_NAME_SIZE];

	/* driver info, exception etc */
	uint32_t drv_info;
	uint32_t drv_ret;

	/* mpu setting */
	uint16_t mpu_num;
	uint32_t mpu_seg[MVPU_MPU_SEGMENT_NUMS];

	/* debug mode			 */
	/* 0x0	: debugger		 */
	/* 0x1	: rv break debug */
	/* 0x2	: safe mode for memory in-order */
	uint16_t debug_mode;
	/* debugger id when rv debug */
	uint16_t debug_id;

	/* PMU setting */
	uint16_t pmu_mode;
	uint16_t pmc_mode;
	uint32_t pmu_buff;
	uint32_t buff_size;

#ifdef MVPU_SECURITY
	uint32_t batch_name_hash;

	uint32_t buf_num;
	uint32_t rp_num;

	uint64_t sec_chk_addr;
	uint64_t sec_buf_size;
	uint64_t sec_buf_attr;

	uint64_t target_buf_old_base;
	uint64_t target_buf_old_offset;
	uint64_t target_buf_new_base;
	uint64_t target_buf_new_offset;

	uint32_t kerarg_num;
	uint64_t kerarg_buf_id;
	uint64_t kerarg_offset;
	uint64_t kerarg_size;

	uint32_t primem_num;
	uint64_t primem_src_buf_id;
	uint64_t primem_dst_buf_id;
	uint64_t primem_src_offset;
	uint64_t primem_dst_offset;
	uint64_t primem_size;
#endif
} __attribute__ ((__packed__));

#endif
