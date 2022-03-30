/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef __TEEI_SMC_STRUCT_H__
#define __TEEI_SMC_STRUCT_H__

#include <linux/semaphore.h>
#define TEE_NAME_SIZE   (255)

struct teei_encode {
	struct list_head head;
	int encode_id;
	void *ker_req_data_addr;
	void *ker_res_data_addr;
	u32  enc_req_offset;
	u32  enc_res_offset;
	u32  enc_req_pos;
	u32  enc_res_pos;
	u32  dec_res_pos;
	u32  dec_offset;
	struct teei_encode_meta *meta;
};

struct teei_shared_mem {
	struct list_head head;
	struct list_head s_head;
	void *index;
	void *k_addr;
	void *u_addr;
	u32  len;
};

struct teei_context {
	/* ID */
	unsigned long cont_id;
	/* Name */
	char tee_name[TEE_NAME_SIZE];
	/* session counter */
	unsigned long sess_cnt;
	/* share memory counter */
	unsigned long shared_mem_cnt;
	/* link list for teei_context */
	struct list_head link;
	/* link list for the sessions of this context */
	struct list_head sess_link;
	/* link list for the share memory of this context */
	struct list_head shared_mem_list;
	struct semaphore cont_lock;
};

struct teei_session {
	/* ID */
	int sess_id;
	/* the teei_context pointer of this session */
	struct teei_context *parent_cont;
	/* link list for teei_session */
	struct list_head link;
	/* link list for the encode of this session */
	struct list_head encode_list;
	/* link list for the share memory of this session */
	struct list_head shared_mem_list;
};

#endif
