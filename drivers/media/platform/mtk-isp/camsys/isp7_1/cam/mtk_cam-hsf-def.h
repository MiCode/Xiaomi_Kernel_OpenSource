/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_HSF_DEF_H
#define __MTK_CAM_HSF_DEF_H

//#include "tz_m4u.h"
#include <linux/time.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>

#define MAX_INIT_REQUEST_NUM                3
#define USING_CCU
//#define MTEE_USE
//#define PERFORMANCE_HSF
#define USING_HSF_SENSOR
#define MSG_TO_CCU_HSF_APPLY_CQ 0
#define MSG_TO_CCU_WRITE_READ_COUNT 1
#define MSG_TO_CCU_STREAM_ON 2
#define MSG_TO_CCU_HSF_CONFIG 3

struct mtk_cam_hsf_info {
	u32 cq_size;
	u64 cq_dst_iova;
	u32 cam_tg;
	u32 enable_raw;
	u32 cam_module;
	u64 chunk_iova;
	uint32_t chunk_hsfhandle;
};

struct cq_info {
	uint32_t tg;
	uint64_t dst_addr;
	uint64_t src_addr;
	uint64_t chunk_iova;
	uint32_t cq_size;
	uint32_t init_value;
	uint32_t cq_offset;
	uint32_t sub_cq_size;
	uint32_t sub_cq_offset;
	int32_t  ipc_status;
};

struct raw_info {
	uint32_t tg_idx;
	uint64_t chunk_iova;
	uint64_t cq_iova;
	uint32_t Hsf_en;
	uint32_t enable_raw;
};

struct mtk_cam_dma_map {
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	dma_addr_t dma_addr;
	uint32_t hsf_handle;
};

struct mtk_cam_hsf_ctrl {
#ifdef USING_CCU
	struct platform_device *ccu_pdev;
	phandle ccu_handle;
	int power_on_cnt;
#endif
	struct mtk_cam_hsf_info *share_buf;
	struct mtk_cam_dma_map *cq_buf;
	struct mtk_cam_dma_map *chk_buf;
};

#define CMD_INIT 1
#define CMD_UNINIT 2
#define CMD_HSFCONFIG 3
#define CMD_QUERYPA 4
#define CMD_TABREGISTER 5
#define CMD_TABMIGRATE 6
#define CMD_TABUNREGISTER 7
#define CMD_DUMPREG 8
#define CMD_DUMPHSFMEM 9
#define CMD_QUERYHWINFO 10
#define CMD_QUERYMVA 11
#define CMD_GETFRMHEADER 12
#define CMD_TABMIGRATEBACK 13
#define CMD_TABGETHSFTAB 14
#define CMD_QUERYTABMVA 15
#define CMD_READREG 16
#define CMD_WRITEREG 17
#define CMD_SETHSFCAM 18
#define CMD_GETHSFCAM 19
#define CMD_HSFMEMTEST 20
#define CMD_SETDAPCREG 21
#define CMD_ALLOCHSFFH 22
#define CMD_GETHSFFHINFO 23
#define CMD_FILLPROTSTATUS 24

#endif /*__MTK_CAM_HSF_DEF_H*/
