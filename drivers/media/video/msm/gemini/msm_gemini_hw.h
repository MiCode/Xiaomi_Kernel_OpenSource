/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_GEMINI_HW_H
#define MSM_GEMINI_HW_H

#include <media/msm_gemini.h>
#include "msm_gemini_hw_reg.h"
#include <mach/msm_subsystem_map.h>
#include <linux/ion.h>

struct msm_gemini_hw_buf {
	struct msm_gemini_buf vbuf;
	struct file  *file;
	uint32_t framedone_len;
	uint32_t y_buffer_addr;
	uint32_t y_len;
	uint32_t cbcr_buffer_addr;
	uint32_t cbcr_len;
	uint32_t num_of_mcu_rows;
	struct msm_mapped_buffer *msm_buffer;
	int *subsystem_id;
	struct ion_handle *handle;
};

struct msm_gemini_hw_pingpong {
	uint8_t is_fe; /* 1: fe; 0: we */
	struct  msm_gemini_hw_buf buf[2];
	int     buf_status[2];
	int     buf_active_index;
};

int msm_gemini_hw_pingpong_update(struct msm_gemini_hw_pingpong *pingpong_hw,
	struct msm_gemini_hw_buf *buf);
void *msm_gemini_hw_pingpong_irq(struct msm_gemini_hw_pingpong *pingpong_hw);
void *msm_gemini_hw_pingpong_active_buffer(struct msm_gemini_hw_pingpong
	*pingpong_hw);

void msm_gemini_hw_irq_clear(uint32_t, uint32_t);
int msm_gemini_hw_irq_get_status(void);
long msm_gemini_hw_encode_output_size(void);
#define MSM_GEMINI_HW_MASK_COMP_FRAMEDONE \
		MSM_GEMINI_HW_IRQ_STATUS_FRAMEDONE_MASK
#define MSM_GEMINI_HW_MASK_COMP_FE \
		MSM_GEMINI_HW_IRQ_STATUS_FE_RD_DONE_MASK
#define MSM_GEMINI_HW_MASK_COMP_WE \
		(MSM_GEMINI_HW_IRQ_STATUS_WE_Y_PINGPONG_MASK | \
		 MSM_GEMINI_HW_IRQ_STATUS_WE_CBCR_PINGPONG_MASK)
#define MSM_GEMINI_HW_MASK_COMP_RESET_ACK \
		MSM_GEMINI_HW_IRQ_STATUS_RESET_ACK_MASK
#define MSM_GEMINI_HW_MASK_COMP_ERR \
		(MSM_GEMINI_HW_IRQ_STATUS_FE_RTOVF_MASK | \
		MSM_GEMINI_HW_IRQ_STATUS_FE_VFE_OVERFLOW_MASK | \
		MSM_GEMINI_HW_IRQ_STATUS_WE_Y_BUFFER_OVERFLOW_MASK | \
		MSM_GEMINI_HW_IRQ_STATUS_WE_CBCR_BUFFER_OVERFLOW_MASK | \
		MSM_GEMINI_HW_IRQ_STATUS_WE_CH0_DATAFIFO_OVERFLOW_MASK | \
		MSM_GEMINI_HW_IRQ_STATUS_WE_CH1_DATAFIFO_OVERFLOW_MASK | \
		MSM_GEMINI_HW_IRQ_STATUS_BUS_ERROR_MASK | \
		MSM_GEMINI_HW_IRQ_STATUS_VIOLATION_MASK)

#define msm_gemini_hw_irq_is_frame_done(gemini_irq_status) \
	(gemini_irq_status & MSM_GEMINI_HW_MASK_COMP_FRAMEDONE)
#define msm_gemini_hw_irq_is_fe_pingpong(gemini_irq_status) \
	(gemini_irq_status & MSM_GEMINI_HW_MASK_COMP_FE)
#define msm_gemini_hw_irq_is_we_pingpong(gemini_irq_status) \
	(gemini_irq_status & MSM_GEMINI_HW_MASK_COMP_WE)
#define msm_gemini_hw_irq_is_reset_ack(gemini_irq_status) \
	(gemini_irq_status & MSM_GEMINI_HW_MASK_COMP_RESET_ACK)
#define msm_gemini_hw_irq_is_err(gemini_irq_status) \
	(gemini_irq_status & MSM_GEMINI_HW_MASK_COMP_ERR)

void msm_gemini_hw_fe_buffer_update(struct msm_gemini_hw_buf *p_input,
	uint8_t pingpong_index);
void msm_gemini_hw_we_buffer_update(struct msm_gemini_hw_buf *p_input,
	uint8_t pingpong_index);

void msm_gemini_hw_we_buffer_cfg(uint8_t is_realtime);

void msm_gemini_hw_fe_start(void);
void msm_gemini_hw_clk_cfg(void);

void msm_gemini_hw_reset(void *base, int size);
void msm_gemini_hw_irq_cfg(void);
void msm_gemini_hw_init(void *base, int size);

uint32_t msm_gemini_hw_read(struct msm_gemini_hw_cmd *hw_cmd_p);
void msm_gemini_hw_write(struct msm_gemini_hw_cmd *hw_cmd_p);
int msm_gemini_hw_wait(struct msm_gemini_hw_cmd *hw_cmd_p, int m_us);
void msm_gemini_hw_delay(struct msm_gemini_hw_cmd *hw_cmd_p, int m_us);
int msm_gemini_hw_exec_cmds(struct msm_gemini_hw_cmd *hw_cmd_p, int m_cmds);
void msm_gemini_hw_region_dump(int size);

#define MSM_GEMINI_PIPELINE_CLK_128MHZ 128 /* 8MP  128MHz */
#define MSM_GEMINI_PIPELINE_CLK_140MHZ 140 /* 9MP  140MHz */
#define MSM_GEMINI_PIPELINE_CLK_200MHZ 153 /* 12MP 153MHz */

#endif /* MSM_GEMINI_HW_H */
