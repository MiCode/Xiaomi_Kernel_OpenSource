/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */
#ifndef __MTK_MML_BUF_H__
#define __MTK_MML_BUF_H__

#include <linux/of.h>

struct mml_file_buf;

/* mml_buf_get_fd - get dma_buf instance from fd for later get iova
 *
 * @buf:	the mml buffer struct
 * @fd:		fd array from client
 * @cnt:	count of fd array
 * @name:	set buf name for iommu heap debug
 *
 * Note: Call mml_buf_put_fd for same mml_file_buf later to release it.
 */
s32 mml_buf_get_fd(struct mml_file_buf *buf, int32_t *fd, u32 cnt, const char *name);

/* mml_buf_get -  dma_buf instance
 *
 * @buf:	the mml buffer struct
 * @dmabufs:	dma_buf array from client
 * @cnt:	count of fd array
 * @name:	set buf name for iommu heap debug
 *
 * Note: Call mml_buf_put for same mml_file_buf later to release it.
 */
void mml_buf_get(struct mml_file_buf *buf, void **dmabufs, u32 cnt, const char *name);

/* mml_buf_iova_get - get iova by device and dmabuf
 *
 * @dev:	dma device, such as mml_rdma or mml_wrot
 * @buf:	mml buffer structure to store buffer for planes
 *
 * Return:	0 success; error no if fail
 *
 * Note: Should be call from dma component. And this api may take time to sync
 * cache FROM CPU TO DMA.
 */
int mml_buf_iova_get(struct device *dev, struct mml_file_buf *buf);

/* mml_buf_va_get - map kernel va from dma buf
 *
 * @buf:	mml buffer structure to store buffer for planes
 *
 * Return:	0 success; error no if fail
 *
 * Note: va will unmap in mml_buf_put
 */
int mml_buf_va_get(struct mml_file_buf *buf);

/* mml_buf_put - Unmap and detach instance when get iova. Then release instance
 * from mml_buf_get
 *
 * @buf:	the mml buffer struct
 *
 * Note: iova will not reset to 0 but will unmap and should not use anymore,
 * except for debug dump. This API may take time to sync cache FROM DMA TO CPU
 */
void mml_buf_put(struct mml_file_buf *buf);

/* mml_buf_flush - do flush/clean "cpu to device" sync to buffer
 *
 * @buf:	the mml buffer struct
 */
void mml_buf_flush(struct mml_file_buf *buf);

/* mml_buf_invalid - do invalid "device to cpu" sync to buffer
 *
 * @buf:	the mml buffer struct
 */
void mml_buf_invalid(struct mml_file_buf *buf);

#endif
