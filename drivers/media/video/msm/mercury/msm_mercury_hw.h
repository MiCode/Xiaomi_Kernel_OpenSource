/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_MERCURY_HW_H
#define MSM_MERCURY_HW_H

#include <media/msm_mercury.h>

/*number of pel per block (horiz/vert)*/
#define JPEGDEC_BLOCK_SIZE                 (8)
/* Hardware alignment*/
#define JPEGDEC_HW_ALIGN                   (8)
#define JPEGDEC_HW_SAMPLING_RATIO_MAX      (4)

#define MSM_MERCURY_HW_IRQ_SW_RESET_ACK    (1<<3)
#define MSM_MERCURY_HW_IRQ_WR_ERR_ACK      (1<<2)
#define MSM_MERCURY_HW_IRQ_WR_EOI_ACK      (1<<1)
#define MSM_MERCURY_HW_IRQ_WR_SOF_ACK      (1<<0)

#define MSM_MERCURY_HW_IRQ_RD_EOF_ACK      (1<<1)
#define MSM_MERCURY_HW_IRQ_RD_SOF_ACK      (1<<0)

extern int mercury_core_reset(void);

struct msm_mercury_hw_buf {
		struct msm_mercury_buf vbuf;
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


void msm_mercury_hw_reset(void);
void msm_mercury_hw_init(void *base, int size);
void msm_mercury_hw_rd_irq_clear(uint32_t val);
void msm_mercury_hw_wr_irq_clear(uint32_t val);

uint32_t msm_mercury_hw_read(struct msm_mercury_hw_cmd *hw_cmd_p);
void msm_mercury_hw_write(struct msm_mercury_hw_cmd *hw_cmd_p);
int msm_mercury_hw_wait(struct msm_mercury_hw_cmd *hw_cmd_p, int m_us);
void msm_mercury_hw_delay(struct msm_mercury_hw_cmd *hw_cmd_p, int m_us);
int msm_mercury_hw_exec_cmds(struct msm_mercury_hw_cmd *hw_cmd_p, int m_cmds);
void msm_mercury_hw_region_dump(int size);


void msm_mercury_hw_irq_get_status(uint16_t *rd_irq, uint16_t *wr_irq);
void msm_mercury_hw_start_decode(void);
void msm_mercury_hw_get_jpeg_status(uint32_t *jpeg_status);
void msm_mercury_hw_output_y_buf_cfg(uint32_t y_buf_addr);
void msm_mercury_hw_output_u_buf_cfg(uint32_t u_buf_addr);
void msm_mercury_hw_output_v_buf_cfg(uint32_t v_buf_addr);
void msm_mercury_hw_bitstream_buf_cfg(uint32_t bitstream_buf_addr);

#endif /* MSM_MERCURY_HW_H */
