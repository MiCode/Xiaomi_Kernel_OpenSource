/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MVPU_CMD_DATA_H__
#define __MVPU_CMD_DATA_H__

#define MVPU_REQUEST_NAME_SIZE 32
#define MVPU_MPU_SEGMENT_NUMS  39

#define MVPU_MIN_CMDBUF_NUM     2
#define MVPU_CMD_INFO_IDX       0
#define MVPU_CMD_KREG_BASE_IDX  1

#ifndef MVPU_SECURITY
#define MVPU_SECURITY
#endif

typedef struct {
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
} BundleHeader;


/* MVPU command structure*/
struct mvpu_request {
	BundleHeader header;
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
	uint64_t mem_is_kernel;

	uint64_t target_buf_old_base;
	uint64_t target_buf_old_offset;
	uint64_t target_buf_new_base;
	uint64_t target_buf_new_offset;
#endif
} __attribute__ ((__packed__));

typedef struct mvpu_request mvpu_request_t;

#endif
