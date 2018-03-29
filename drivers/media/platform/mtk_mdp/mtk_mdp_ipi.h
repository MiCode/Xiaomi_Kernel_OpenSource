/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
`* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef __MTK_MDP_IPI_H__
#define __MTK_MDP_IPI_H__

enum mdp_ipi_msgid {
	AP_MDP_INIT		= 0xD000,
	AP_MDP_DEINIT		= 0xD001,
	AP_MDP_PROCESS		= 0xD002,

	VPU_MDP_INIT_ACK	= 0xE000,
	VPU_MDP_DEINIT_ACK	= 0xE001,
	VPU_MDP_PROCESS_ACK	= 0xE002,
};

enum mdp_ipi_msg_status {
	MDP_IPI_MSG_STATUS_OK	= 0,
	MDP_IPI_MSG_STATUS_FAIL	= -1,
	MDP_IPI_MSG_TIMEOUT	= -2
};

#pragma pack(push, 4)

struct mdp_ipi_init {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint32_t mdp_priv;
};

struct mdp_ipi_deinit {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint32_t h_drv;
	uint32_t mdp_priv;
};

struct mdp_ipi_config {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint32_t h_drv;
	uint32_t mdp_priv;
};

struct mdp_src_config {
	int32_t x;
	int32_t y;
	int32_t w;
	int32_t h;
	int32_t w_stride;
	int32_t h_stride;
	int32_t crop_x;
	int32_t crop_y;
	int32_t crop_w;
	int32_t crop_h;
	int32_t format;
};

struct mdp_src_buffer {
	uint32_t addr_mva[3];
	int32_t plane_size[3];
	int32_t plane_num;
};

struct mdp_dst_config {
	int32_t x;
	int32_t y;
	int32_t w;
	int32_t h;
	int32_t w_stride;
	int32_t h_stride;
	int32_t crop_x;
	int32_t crop_y;
	int32_t crop_w;
	int32_t crop_h;
	int32_t format;
};

struct mdp_dst_buffer {
	uint32_t addr_mva[3];
	int32_t plane_size[3];
	int32_t plane_num;
};

struct mdp_config_misc {
	int32_t orientation; /* 0, 90, 180, 270 */
	int32_t hflip;
	int32_t vflip;
	int32_t alpha; /* global alpha */
};

struct mdp_process_param {
	struct mdp_src_buffer src_buffer;
	struct mdp_src_config src_config;
	struct mdp_dst_buffer dst_buffer;
	struct mdp_dst_config dst_config;
	struct mdp_config_misc misc;
	uint32_t h_drv;
	uint32_t mdp_priv;
};


struct mdp_ipi_process {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint32_t h_drv;
	uint32_t mdp_priv;
};

struct mdp_ipi_init_ack {
	uint32_t msg_id;
	uint32_t ipi_id;
	int32_t status;
	int32_t reserved;
	uint32_t mdp_priv;
	uint32_t shmem_addr;
	uint32_t h_drv;
};

struct mdp_ipi_comm_ack {
	uint32_t msg_id;
	uint32_t ipi_id;
	int32_t status;
	int32_t reserved;
	uint32_t mdp_priv;
};

struct mdp_ipi_msg_common {
	uint32_t msg_id;
	uint32_t status;
	uint32_t mdp_inst;
};

struct mdp_vpu_config {

};

struct mdp_vpu_buf {

};

struct mdp_vpu_drv {
	struct mdp_vpu_config config;
	struct mdp_vpu_buf work_bufs;
};

struct mdp_vpu_inst {
	wait_queue_head_t wq_hd;
	int signaled;
	int failure;
	int state;
	unsigned int id;
	struct mdp_vpu_drv *drv;
};


struct mdp_inst {
	void __iomem *hw_base;
	bool work_buf_allocated;
	unsigned int frm_cnt;
	unsigned int prepend_hdr;
	unsigned int is_key_frm;
	struct mdp_vpu_inst vpu_inst;
	void *ctx;
	struct platform_device *dev;
};


#pragma pack(pop)

#endif /* __MTK_MDP_IPI_H__ */
