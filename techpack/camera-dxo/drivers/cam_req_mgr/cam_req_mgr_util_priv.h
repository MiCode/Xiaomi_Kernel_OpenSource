/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_REQ_MGR_UTIL_PRIV_H_
#define _CAM_REQ_MGR_UTIL_PRIV_H_

/**
 * handle format:
 * @bits (0-7): handle index
 * @bits (8-11): handle type
 * @bits (12-15): reserved
 * @bits (16-23): random bits
 * @bits (24-31): zeros
 */

#define CAM_REQ_MGR_HDL_SIZE            32
#define CAM_REQ_MGR_RND1_SIZE           8
#define CAM_REQ_MGR_RVD_SIZE            4
#define CAM_REQ_MGR_HDL_TYPE_SIZE       4
#define CAM_REQ_MGR_HDL_IDX_SIZE        8

#define CAM_REQ_MGR_RND1_POS            24
#define CAM_REQ_MGR_RVD_POS             16
#define CAM_REQ_MGR_HDL_TYPE_POS        12

#define CAM_REQ_MGR_RND1_BYTES          1

#define CAM_REQ_MGR_HDL_TYPE_MASK      ((1 << CAM_REQ_MGR_HDL_TYPE_SIZE) - 1)

#define GET_DEV_HANDLE(rnd1, type, idx) \
	((rnd1 << (CAM_REQ_MGR_RND1_POS - CAM_REQ_MGR_RND1_SIZE)) | \
	(0x0  << (CAM_REQ_MGR_RVD_POS - CAM_REQ_MGR_RVD_SIZE)) | \
	(type << (CAM_REQ_MGR_HDL_TYPE_POS - CAM_REQ_MGR_HDL_TYPE_SIZE)) | \
	(idx << (CAM_REQ_MGR_HDL_IDX_POS - CAM_REQ_MGR_HDL_IDX_SIZE))) \

#define CAM_REQ_MGR_GET_HDL_IDX(hdl) (hdl & CAM_REQ_MGR_HDL_IDX_MASK)
#define CAM_REQ_MGR_GET_HDL_TYPE(hdl) \
	((hdl >> CAM_REQ_MGR_HDL_IDX_POS) & CAM_REQ_MGR_HDL_TYPE_MASK)

#endif
