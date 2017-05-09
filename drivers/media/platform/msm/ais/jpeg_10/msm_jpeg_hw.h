/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_JPEG_HW_H
#define MSM_JPEG_HW_H

#include <media/msm_jpeg.h>
#include "msm_jpeg_hw_reg.h"
#include <linux/ion.h>

struct msm_jpeg_hw_buf {
	struct msm_jpeg_buf vbuf;
	struct file  *file;
	uint32_t framedone_len;
	uint32_t y_buffer_addr;
	uint32_t y_len;
	uint32_t cbcr_buffer_addr;
	uint32_t cbcr_len;
	uint32_t num_of_mcu_rows;
	int ion_fd;
	uint32_t pln2_addr;
	uint32_t pln2_len;
};

struct msm_jpeg_hw_pingpong {
	uint8_t is_fe; /* 1: fe; 0: we */
	struct  msm_jpeg_hw_buf buf[2];
	int     buf_status[2];
	int     buf_active_index;
};

int msm_jpeg_hw_pingpong_update(struct msm_jpeg_hw_pingpong *pingpong_hw,
	struct msm_jpeg_hw_buf *buf, void *);
int msm_jpegdma_hw_pingpong_update(struct msm_jpeg_hw_pingpong *pingpong_hw,
	struct msm_jpeg_hw_buf *buf, void *);
void *msm_jpeg_hw_pingpong_irq(struct msm_jpeg_hw_pingpong *pingpong_hw);
void *msm_jpeg_hw_pingpong_active_buffer(struct msm_jpeg_hw_pingpong
	*pingpong_hw);

void msm_jpeg_hw_irq_clear(uint32_t, uint32_t, void *);
void msm_jpegdma_hw_irq_clear(uint32_t, uint32_t, void *);
int msm_jpeg_hw_irq_get_status(void *);
int msm_jpegdma_hw_irq_get_status(void *);
long msm_jpeg_hw_encode_output_size(void *);
#define MSM_JPEG_HW_MASK_COMP_FRAMEDONE \
		MSM_JPEG_HW_IRQ_STATUS_FRAMEDONE_MASK
#define MSM_JPEG_HW_MASK_COMP_FE \
		MSM_JPEG_HW_IRQ_STATUS_FE_RD_DONE_MASK
#define MSM_JPEG_HW_MASK_COMP_WE \
		(MSM_JPEG_HW_IRQ_STATUS_WE_Y_PINGPONG_MASK | \
		 MSM_JPEG_HW_IRQ_STATUS_WE_CBCR_PINGPONG_MASK)
#define MSM_JPEG_HW_MASK_COMP_RESET_ACK \
		MSM_JPEG_HW_IRQ_STATUS_RESET_ACK_MASK
#define MSM_JPEG_HW_MASK_COMP_ERR \
		(MSM_JPEG_HW_IRQ_STATUS_DCD_UNESCAPED_FF | \
		MSM_JPEG_HW_IRQ_STATUS_DCD_HUFFMAN_ERROR | \
		MSM_JPEG_HW_IRQ_STATUS_DCD_COEFFICIENT_ERR | \
		MSM_JPEG_HW_IRQ_STATUS_DCD_MISSING_BIT_STUFF | \
		MSM_JPEG_HW_IRQ_STATUS_DCD_SCAN_UNDERFLOW | \
		MSM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM | \
		MSM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM_SEQ | \
		MSM_JPEG_HW_IRQ_STATUS_DCD_MISSING_RSM | \
		MSM_JPEG_HW_IRQ_STATUS_VIOLATION_MASK)

#define msm_jpeg_hw_irq_is_frame_done(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEG_HW_MASK_COMP_FRAMEDONE)
#define msm_jpeg_hw_irq_is_fe_pingpong(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEG_HW_MASK_COMP_FE)
#define msm_jpeg_hw_irq_is_we_pingpong(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEG_HW_MASK_COMP_WE)
#define msm_jpeg_hw_irq_is_reset_ack(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEG_HW_MASK_COMP_RESET_ACK)
#define msm_jpeg_hw_irq_is_err(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEG_HW_MASK_COMP_ERR)


#define MSM_JPEGDMA_HW_MASK_COMP_FRAMEDONE \
		MSM_JPEGDMA_HW_IRQ_STATUS_FRAMEDONE_MASK
#define MSM_JPEGDMA_HW_MASK_COMP_FE \
		MSM_JPEGDMA_HW_IRQ_STATUS_FE_RD_DONE_MASK
#define MSM_JPEGDMA_HW_MASK_COMP_WE \
		(MSM_JPEGDMA_HW_IRQ_STATUS_WE_WR_DONE_MASK)
#define MSM_JPEGDMA_HW_MASK_COMP_RESET_ACK \
		MSM_JPEGDMA_HW_IRQ_STATUS_RESET_ACK_MASK


#define msm_jpegdma_hw_irq_is_frame_done(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEGDMA_HW_MASK_COMP_FRAMEDONE)
#define msm_jpegdma_hw_irq_is_fe_pingpong(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEGDMA_HW_MASK_COMP_FE)
#define msm_jpegdma_hw_irq_is_we_pingpong(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEGDMA_HW_MASK_COMP_WE)
#define msm_jpegdma_hw_irq_is_reset_ack(jpeg_irq_status) \
	(jpeg_irq_status & MSM_JPEGDMA_HW_MASK_COMP_RESET_ACK)


void msm_jpeg_hw_fe_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *);
void msm_jpeg_hw_we_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *);
void msm_jpegdma_hw_fe_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *);
void msm_jpegdma_hw_we_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *);


void msm_jpeg_hw_we_buffer_cfg(uint8_t is_realtime);

void msm_jpeg_hw_fe_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *,
	uint8_t decode_flag);
void msm_jpeg_hw_we_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *,
	uint8_t decode_flag);
void msm_jpegdma_hw_fe_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *);
void msm_jpegdma_hw_we_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *);

void msm_jpeg_hw_fe_start(void *);
void msm_jpeg_hw_clk_cfg(void);

void msm_jpeg_hw_reset(void *base, int size);
void msm_jpeg_hw_irq_cfg(void);

uint32_t msm_jpeg_hw_read(struct msm_jpeg_hw_cmd *, void *);
void msm_jpeg_hw_write(struct msm_jpeg_hw_cmd *, void *);
int msm_jpeg_hw_wait(struct msm_jpeg_hw_cmd *, int, void *);
void msm_jpeg_hw_delay(struct msm_jpeg_hw_cmd *, int);
int msm_jpeg_hw_exec_cmds(struct msm_jpeg_hw_cmd *, uint32_t ,
	uint32_t , void *);
void msm_jpeg_hw_region_dump(int size);
void msm_jpeg_io_dump(void *base, int size);
void msm_jpeg_decode_status(void *base);
void msm_jpeg_hw_reset_dma(void *base, int size);

#endif /* MSM_JPEG_HW_H */
