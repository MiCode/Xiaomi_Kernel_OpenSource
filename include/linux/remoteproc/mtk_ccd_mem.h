/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __MTK_RPMSG_CCD_MEM_H__
#define __MTK_RPMSG_CCD_MEM_H__

#define MAX_NUMBER_OF_BUFFER (128)
struct vb2_mem_ops;

/**
 * struct mtk_ccd_mem - memory buffer allocated in kernel
 *
 * @mem_priv:   vb2_dc_buf
 * @size:       allocated buffer size
 */
struct mtk_ccd_mem {
	void *mem_priv;
	size_t size;
};

/**
 * struct mtk_ccd_memory
 *
 * @vcu:        struct mtk_ccd
 * @mmap_lock:  the lock to protect allocated buffer
 * @dev:        device
 * @num_buffers:allocated buffer number
 * @mem_ops:    the file operation of memory allocated
 * @bufs:       store the information of allocated buffers
 */
struct mtk_ccd_memory {
	void *priv;
	struct mutex mmap_lock;
	struct device *dev;
	unsigned int num_buffers;
	const struct vb2_mem_ops *mem_ops;
	struct mtk_ccd_mem bufs[MAX_NUMBER_OF_BUFFER];
};

struct mtk_ccd_memory *mtk_ccd_mem_init(struct device *dev);
void mtk_ccd_mem_release(struct mtk_ccd *ccd);
#endif
