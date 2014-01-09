/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef HEAP_MEM_EXT_SERVICE_01_H
#define HEAP_MEM_EXT_SERVICE_01_H

#include <soc/qcom/msm_qmi_interface.h>

#define MEM_ALLOC_REQ_MAX_MSG_LEN_V01 255
#define MEM_FREE_REQ_MAX_MSG_LEN_V01 255

enum dhms_mem_block_align_enum_v01 {
	/* To force a 32 bit signed enum.  Do not change or use
	*/
	DHMS_MEM_BLOCK_ALIGN_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* Align allocated memory by 2 bytes  */
	DHMS_MEM_BLOCK_ALIGN_2_V01 = 0,
	/* Align allocated memory by 4 bytes  */
	DHMS_MEM_BLOCK_ALIGN_4_V01 = 1,
	/**<  Align allocated memory by 8 bytes */
	DHMS_MEM_BLOCK_ALIGN_8_V01 = 2,
	/**<  Align allocated memory by 16 bytes */
	DHMS_MEM_BLOCK_ALIGN_16_V01 = 3,
	/**<  Align allocated memory by 32 bytes */
	DHMS_MEM_BLOCK_ALIGN_32_V01 = 4,
	/**<  Align allocated memory by 64 bytes */
	DHMS_MEM_BLOCK_ALIGN_64_V01 = 5,
	/**<  Align allocated memory by 128 bytes */
	DHMS_MEM_BLOCK_ALIGN_128_V01 = 6,
	/**<  Align allocated memory by 256 bytes */
	DHMS_MEM_BLOCK_ALIGN_256_V01 = 7,
	/**<  Align allocated memory by 512 bytes */
	DHMS_MEM_BLOCK_ALIGN_512_V01 = 8,
	/**<  Align allocated memory by 1024 bytes */
	DHMS_MEM_BLOCK_ALIGN_1K_V01 = 9,
	/**<  Align allocated memory by 2048 bytes */
	DHMS_MEM_BLOCK_ALIGN_2K_V01 = 10,
	/**<  Align allocated memory by 4096 bytes */
	DHMS_MEM_BLOCK_ALIGN_4K_V01 = 11,
	DHMS_MEM_BLOCK_ALIGN_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use
	*/
};

/* Request Message; This command is used for getting
 * the multiple physically contiguous
 * memory blocks from the server memory subsystem
 */
struct mem_alloc_req_msg_v01 {

	/* Mandatory */
	/*requested size*/
	uint32_t num_bytes;

	/* Optional */
	/* Must be set to true if block_alignment
	 * is being passed
	 */
	uint8_t block_alignment_valid;
	/* The block alignment for the memory block to be allocated
	*/
	enum dhms_mem_block_align_enum_v01 block_alignment;
};  /* Message */

/* Response Message; This command is used for getting
 * the multiple physically contiguous memory blocks
 * from the server memory subsystem
 */
struct mem_alloc_resp_msg_v01 {

	/* Mandatory */
	/*  Result Code */
	/* The result of the requested memory operation
	*/
	enum qmi_result_type_v01 resp;
	/* Optional */
	/*  Memory Block Handle
	*/
	/* Must be set to true if handle is being passed
	*/
	uint8_t handle_valid;
	/* The physical address of the memory allocated on the HLOS
	*/
	uint64_t handle;
	/* Optional */
	/* Memory block size */
	/* Must be set to true if num_bytes is being passed
	*/
	uint8_t num_bytes_valid;
	/* The number of bytes actually allocated for the request.
	 * This value can be smaller than the size requested in
	 * QMI_DHMS_MEM_ALLOC_REQ_MSG.
	*/
	uint32_t num_bytes;
};  /* Message */

/* Request Message; This command is used for releasing
 * the multiple physically contiguous
 * memory blocks to the server memory subsystem
 */
struct mem_free_req_msg_v01 {

	/* Mandatory */
	/* Physical address of memory to be freed
	*/
	uint32_t handle;
};  /* Message */

/* Response Message; This command is used for releasing
 * the multiple physically contiguous
 * memory blocks to the server memory subsystem
 */
struct mem_free_resp_msg_v01 {

	/* Mandatory */
	/* Result of the requested memory operation, todo,
	 * need to check the async operation for free
	 */
	enum qmi_result_type_v01 resp;
};  /* Message */

extern struct elem_info mem_alloc_req_msg_data_v01_ei[];
extern struct elem_info mem_alloc_resp_msg_data_v01_ei[];
extern struct elem_info mem_free_req_msg_data_v01_ei[];
extern struct elem_info mem_free_resp_msg_data_v01_ei[];

/*Service Message Definition*/
#define MEM_ALLOC_REQ_MSG_V01 0x0020
#define MEM_ALLOC_RESP_MSG_V01 0x0020
#define MEM_FREE_REQ_MSG_V01 0x0021
#define MEM_FREE_RESP_MSG_V01 0x0021

#endif
