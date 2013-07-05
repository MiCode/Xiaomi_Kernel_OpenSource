/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/delay.h>
#include "msm_gemini_hw.h"
#include "msm_gemini_common.h"

#include <linux/io.h>

static void *gemini_region_base;
static uint32_t gemini_region_size;

int msm_gemini_hw_pingpong_update(struct msm_gemini_hw_pingpong *pingpong_hw,
	struct msm_gemini_hw_buf *buf)
{
	int buf_free_index = -1;

	if (!pingpong_hw->buf_status[0]) {
		buf_free_index = 0;
	} else if (!pingpong_hw->buf_status[1]) {
		buf_free_index = 1;
	} else {
		GMN_PR_ERR("%s:%d: pingpong buffer busy\n", __func__, __LINE__);
		return -1;
	}

	pingpong_hw->buf[buf_free_index] = *buf;
	pingpong_hw->buf_status[buf_free_index] = 1;

	if (pingpong_hw->is_fe) {
		/* it is fe */
		msm_gemini_hw_fe_buffer_update(
			&pingpong_hw->buf[buf_free_index], buf_free_index);
	} else {
		/* it is we */
		msm_gemini_hw_we_buffer_update(
			&pingpong_hw->buf[buf_free_index], buf_free_index);
	}
	return 0;
}

void *msm_gemini_hw_pingpong_irq(struct msm_gemini_hw_pingpong *pingpong_hw)
{
	struct msm_gemini_hw_buf *buf_p = NULL;

	if (pingpong_hw->buf_status[pingpong_hw->buf_active_index]) {
		buf_p = &pingpong_hw->buf[pingpong_hw->buf_active_index];
		pingpong_hw->buf_status[pingpong_hw->buf_active_index] = 0;
	}

	pingpong_hw->buf_active_index = !pingpong_hw->buf_active_index;

	return (void *) buf_p;
}

void *msm_gemini_hw_pingpong_active_buffer(
	struct msm_gemini_hw_pingpong *pingpong_hw)
{
	struct msm_gemini_hw_buf *buf_p = NULL;

	if (pingpong_hw->buf_status[pingpong_hw->buf_active_index])
		buf_p = &pingpong_hw->buf[pingpong_hw->buf_active_index];

	return (void *) buf_p;
}

struct msm_gemini_hw_cmd hw_cmd_irq_get_status[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_READ, 1, HWIO_JPEG_IRQ_STATUS_ADDR,
		HWIO_JPEG_IRQ_STATUS_RMSK, {0} },
};

int msm_gemini_hw_irq_get_status(void)
{
	uint32_t n_irq_status = 0;
	rmb();
	n_irq_status = msm_gemini_hw_read(&hw_cmd_irq_get_status[0]);
	rmb();
	return n_irq_status;
}

struct msm_gemini_hw_cmd hw_cmd_encode_output_size[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_READ, 1,
		HWIO_JPEG_STATUS_ENCODE_OUTPUT_SIZE_ADDR,
		HWIO_JPEG_STATUS_ENCODE_OUTPUT_SIZE_RMSK, {0} },
};

long msm_gemini_hw_encode_output_size(void)
{
	uint32_t encode_output_size = 0;

	encode_output_size = msm_gemini_hw_read(&hw_cmd_encode_output_size[0]);

	return encode_output_size;
}

struct msm_gemini_hw_cmd hw_cmd_irq_clear[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_IRQ_CLEAR_ADDR,
		HWIO_JPEG_IRQ_CLEAR_RMSK, {JPEG_IRQ_CLEAR_ALL} },
};

void msm_gemini_hw_irq_clear(uint32_t mask, uint32_t data)
{
	GMN_DBG("%s:%d] mask %0x data %0x", __func__, __LINE__, mask, data);
	hw_cmd_irq_clear[0].mask = mask;
	hw_cmd_irq_clear[0].data = data;
	msm_gemini_hw_write(&hw_cmd_irq_clear[0]);
}

struct msm_gemini_hw_cmd hw_cmd_fe_ping_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_BUFFER_CFG_ADDR,
		HWIO_JPEG_FE_BUFFER_CFG_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_Y_PING_ADDR_ADDR,
		HWIO_JPEG_FE_Y_PING_ADDR_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_CBCR_PING_ADDR_ADDR,
		HWIO_JPEG_FE_CBCR_PING_ADDR_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_CMD_ADDR,
		HWIO_JPEG_FE_CMD_RMSK, {JPEG_FE_CMD_BUFFERRELOAD} },
};

struct msm_gemini_hw_cmd hw_cmd_fe_pong_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_BUFFER_CFG_ADDR,
		HWIO_JPEG_FE_BUFFER_CFG_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_Y_PONG_ADDR_ADDR,
		HWIO_JPEG_FE_Y_PONG_ADDR_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_CBCR_PONG_ADDR_ADDR,
		HWIO_JPEG_FE_CBCR_PONG_ADDR_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_CMD_ADDR,
		HWIO_JPEG_FE_CMD_RMSK, {JPEG_FE_CMD_BUFFERRELOAD} },
};

void msm_gemini_hw_fe_buffer_update(struct msm_gemini_hw_buf *p_input,
	uint8_t pingpong_index)
{
	uint32_t n_reg_val = 0;

	struct msm_gemini_hw_cmd *hw_cmd_p;

	if (pingpong_index == 0) {
		hw_cmd_p = &hw_cmd_fe_ping_update[0];
		n_reg_val = ((((p_input->num_of_mcu_rows - 1) <<
			HWIO_JPEG_FE_BUFFER_CFG_CBCR_MCU_ROWS_SHFT) &
			HWIO_JPEG_FE_BUFFER_CFG_CBCR_MCU_ROWS_BMSK) |
			(((p_input->num_of_mcu_rows - 1) <<
			HWIO_JPEG_FE_BUFFER_CFG_Y_MCU_ROWS_SHFT) &
			HWIO_JPEG_FE_BUFFER_CFG_Y_MCU_ROWS_BMSK));
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		n_reg_val = ((p_input->y_buffer_addr <<
			HWIO_JPEG_FE_Y_PING_ADDR_FE_Y_PING_START_ADDR_SHFT) &
			HWIO_JPEG_FE_Y_PING_ADDR_FE_Y_PING_START_ADDR_BMSK);
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		n_reg_val = ((p_input->cbcr_buffer_addr<<
		HWIO_JPEG_FE_CBCR_PING_ADDR_FE_CBCR_PING_START_ADDR_SHFT) &
		HWIO_JPEG_FE_CBCR_PING_ADDR_FE_CBCR_PING_START_ADDR_BMSK);
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		msm_gemini_hw_write(hw_cmd_p);
	} else if (pingpong_index == 1) {
		hw_cmd_p = &hw_cmd_fe_pong_update[0];
		n_reg_val = ((((p_input->num_of_mcu_rows - 1) <<
			HWIO_JPEG_FE_BUFFER_CFG_CBCR_MCU_ROWS_SHFT) &
			HWIO_JPEG_FE_BUFFER_CFG_CBCR_MCU_ROWS_BMSK) |
			(((p_input->num_of_mcu_rows - 1) <<
			HWIO_JPEG_FE_BUFFER_CFG_Y_MCU_ROWS_SHFT) &
			HWIO_JPEG_FE_BUFFER_CFG_Y_MCU_ROWS_BMSK));
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		n_reg_val = ((p_input->y_buffer_addr <<
			HWIO_JPEG_FE_Y_PONG_ADDR_FE_Y_PONG_START_ADDR_SHFT) &
			HWIO_JPEG_FE_Y_PONG_ADDR_FE_Y_PONG_START_ADDR_BMSK);
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		n_reg_val = ((p_input->cbcr_buffer_addr<<
		HWIO_JPEG_FE_CBCR_PONG_ADDR_FE_CBCR_PONG_START_ADDR_SHFT) &
		HWIO_JPEG_FE_CBCR_PONG_ADDR_FE_CBCR_PONG_START_ADDR_BMSK);
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		msm_gemini_hw_write(hw_cmd_p);
	} else {
		/* shall not get to here */
	}

	return;
}

struct msm_gemini_hw_cmd hw_cmd_fe_start[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_FE_CMD_ADDR,
		HWIO_JPEG_FE_CMD_RMSK, {JPEG_OFFLINE_CMD_START} },
};

void msm_gemini_hw_fe_start(void)
{
	msm_gemini_hw_write(&hw_cmd_fe_start[0]);

	return;
}

struct msm_gemini_hw_cmd hw_cmd_we_buffer_cfg[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_WE_Y_THRESHOLD_ADDR,
		HWIO_JPEG_WE_Y_THRESHOLD_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_WE_Y_UB_CFG_ADDR,
		HWIO_JPEG_WE_Y_UB_CFG_RMSK, {JPEG_WE_YUB_ENCODE} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_WE_CBCR_THRESHOLD_ADDR,
		HWIO_JPEG_WE_CBCR_THRESHOLD_RMSK, {0} },
};

/* first dimension is WE_ASSERT_STALL_TH and WE_DEASSERT_STALL_TH
   second dimension is for offline and real-time settings
 */
static const uint32_t GEMINI_WE_Y_THRESHOLD[2][2] = {
	{ 0x00000190, 0x000001ff },
	{ 0x0000016a, 0x000001ff }
};

/* first dimension is WE_ASSERT_STALL_TH and WE_DEASSERT_STALL_TH
   second dimension is for offline and real-time settings
 */
static const uint32_t GEMINI_WE_CBCR_THRESHOLD[2][2] = {
	{ 0x00000190, 0x000001ff },
	{ 0x0000016a, 0x000001ff }
};

void msm_gemini_hw_we_buffer_cfg(uint8_t is_realtime)
{
	uint32_t              n_reg_val = 0;

	struct msm_gemini_hw_cmd *hw_cmd_p = &hw_cmd_we_buffer_cfg[0];

	n_reg_val = (((GEMINI_WE_Y_THRESHOLD[1][is_realtime] <<
		HWIO_JPEG_WE_Y_THRESHOLD_WE_DEASSERT_STALL_TH_SHFT) &
		HWIO_JPEG_WE_Y_THRESHOLD_WE_DEASSERT_STALL_TH_BMSK) |
		((GEMINI_WE_Y_THRESHOLD[0][is_realtime] <<
		HWIO_JPEG_WE_Y_THRESHOLD_WE_ASSERT_STALL_TH_SHFT) &
		HWIO_JPEG_WE_Y_THRESHOLD_WE_ASSERT_STALL_TH_BMSK));
	hw_cmd_p->data = n_reg_val;
	msm_gemini_hw_write(hw_cmd_p++);

	msm_gemini_hw_write(hw_cmd_p++);

	/* @todo maybe not for realtime? */
	n_reg_val = (((GEMINI_WE_CBCR_THRESHOLD[1][is_realtime] <<
		HWIO_JPEG_WE_CBCR_THRESHOLD_WE_DEASSERT_STALL_TH_SHFT) &
		HWIO_JPEG_WE_CBCR_THRESHOLD_WE_DEASSERT_STALL_TH_BMSK) |
		((GEMINI_WE_CBCR_THRESHOLD[0][is_realtime] <<
		HWIO_JPEG_WE_CBCR_THRESHOLD_WE_ASSERT_STALL_TH_SHFT) &
		HWIO_JPEG_WE_CBCR_THRESHOLD_WE_ASSERT_STALL_TH_BMSK));
	hw_cmd_p->data = n_reg_val;
	msm_gemini_hw_write(hw_cmd_p);

	return;
}

struct msm_gemini_hw_cmd hw_cmd_we_ping_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_WE_Y_PING_BUFFER_CFG_ADDR,
		HWIO_JPEG_WE_Y_PING_BUFFER_CFG_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_WE_Y_PING_ADDR_ADDR,
		HWIO_JPEG_WE_Y_PING_ADDR_RMSK, {0} },
};

struct msm_gemini_hw_cmd hw_cmd_we_pong_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_WE_Y_PONG_BUFFER_CFG_ADDR,
		HWIO_JPEG_WE_Y_PONG_BUFFER_CFG_RMSK, {0} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_WE_Y_PONG_ADDR_ADDR,
		HWIO_JPEG_WE_Y_PONG_ADDR_RMSK, {0} },
};

void msm_gemini_hw_we_buffer_update(struct msm_gemini_hw_buf *p_input,
	uint8_t pingpong_index)
{
	uint32_t n_reg_val = 0;

	struct msm_gemini_hw_cmd *hw_cmd_p;

	if (pingpong_index == 0) {
		hw_cmd_p = &hw_cmd_we_ping_update[0];

		n_reg_val = ((p_input->y_len <<
			HWIO_JPEG_WE_Y_PING_BUFFER_CFG_WE_BUFFER_LENGTH_SHFT) &
			HWIO_JPEG_WE_Y_PING_BUFFER_CFG_WE_BUFFER_LENGTH_BMSK);
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		n_reg_val = p_input->y_buffer_addr;
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);
	} else if (pingpong_index == 1) {
		hw_cmd_p = &hw_cmd_we_pong_update[0];

		n_reg_val = ((p_input->y_len <<
			HWIO_JPEG_WE_Y_PONG_BUFFER_CFG_WE_BUFFER_LENGTH_SHFT) &
			HWIO_JPEG_WE_Y_PONG_BUFFER_CFG_WE_BUFFER_LENGTH_BMSK);
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);

		n_reg_val = p_input->y_buffer_addr;
		hw_cmd_p->data = n_reg_val;
		msm_gemini_hw_write(hw_cmd_p++);
	} else {
		/* shall not get to here */
	}

	return;
}

struct msm_gemini_hw_cmd hw_cmd_reset[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_IRQ_MASK_ADDR,
		HWIO_JPEG_IRQ_MASK_RMSK, {JPEG_IRQ_DISABLE_ALL} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_IRQ_CLEAR_ADDR,
		HWIO_JPEG_IRQ_MASK_RMSK, {JPEG_IRQ_CLEAR_ALL} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_IRQ_MASK_ADDR,
		HWIO_JPEG_IRQ_MASK_RMSK, {JPEG_IRQ_ALLSOURCES_ENABLE} },
	{MSM_GEMINI_HW_CMD_TYPE_WRITE, 1, HWIO_JPEG_RESET_CMD_ADDR,
		HWIO_JPEG_RESET_CMD_RMSK, {JPEG_RESET_DEFAULT} },
};

void msm_gemini_hw_init(void *base, int size)
{
	gemini_region_base = base;
	gemini_region_size = size;
}

void msm_gemini_hw_reset(void *base, int size)
{
	struct msm_gemini_hw_cmd *hw_cmd_p;

	hw_cmd_p = &hw_cmd_reset[0];

	wmb();
	msm_gemini_hw_write(hw_cmd_p++);
	msm_gemini_hw_write(hw_cmd_p++);
	msm_gemini_hw_write(hw_cmd_p++);
	msm_gemini_hw_write(hw_cmd_p);
	wmb();

	return;
}

uint32_t msm_gemini_hw_read(struct msm_gemini_hw_cmd *hw_cmd_p)
{
	uint32_t *paddr;
	uint32_t data;

	paddr = gemini_region_base + hw_cmd_p->offset;

	data = readl(paddr);
	data &= hw_cmd_p->mask;

	GMN_DBG("%s:%d] type-%d n-%d offset-0x%4x mask-0x%8x data-0x%8x\n",
		__func__, __LINE__, hw_cmd_p->type, hw_cmd_p->n,
		hw_cmd_p->offset, hw_cmd_p->mask, data);
	return data;
}

void msm_gemini_hw_write(struct msm_gemini_hw_cmd *hw_cmd_p)
{
	uint32_t *paddr;
	uint32_t old_data, new_data;

	/* type, repeat n times, offset, mask, data or pdata */
	GMN_DBG("%s:%d] type-%d n-%d offset-0x%4x mask-0x%8x data-0x%8x\n",
		__func__, __LINE__, hw_cmd_p->type, hw_cmd_p->n,
		hw_cmd_p->offset, hw_cmd_p->mask, hw_cmd_p->data);

	paddr = gemini_region_base + hw_cmd_p->offset;

	if (hw_cmd_p->mask == 0xffffffff) {
		old_data = 0;
	} else {
		old_data = readl(paddr);
		old_data &= ~hw_cmd_p->mask;
	}

	new_data = hw_cmd_p->data & hw_cmd_p->mask;
	new_data |= old_data;
	writel(new_data, paddr);
}

int msm_gemini_hw_wait(struct msm_gemini_hw_cmd *hw_cmd_p, int m_us)
{
	int tm = hw_cmd_p->n;
	uint32_t data;
	uint32_t wait_data = hw_cmd_p->data & hw_cmd_p->mask;

	data = msm_gemini_hw_read(hw_cmd_p);
	if (data != wait_data) {
		while (tm) {
			udelay(m_us);
			data = msm_gemini_hw_read(hw_cmd_p);
			if (data == wait_data)
				break;
			tm--;
		}
	}
	hw_cmd_p->data = data;
	return tm;
}

void msm_gemini_hw_delay(struct msm_gemini_hw_cmd *hw_cmd_p, int m_us)
{
	int tm = hw_cmd_p->n;
	while (tm) {
		udelay(m_us);
		tm--;
	}
}

int msm_gemini_hw_exec_cmds(struct msm_gemini_hw_cmd *hw_cmd_p, int m_cmds)
{
	int is_copy_to_user = -1;
	uint32_t data;

	while (m_cmds--) {
		if (hw_cmd_p->offset > gemini_region_size) {
			GMN_PR_ERR("%s:%d] %d exceed hw region %d\n", __func__,
				__LINE__, hw_cmd_p->offset, gemini_region_size);
			return -EFAULT;
		}

		switch (hw_cmd_p->type) {
		case MSM_GEMINI_HW_CMD_TYPE_READ:
			hw_cmd_p->data = msm_gemini_hw_read(hw_cmd_p);
			is_copy_to_user = 1;
			break;

		case MSM_GEMINI_HW_CMD_TYPE_WRITE:
			msm_gemini_hw_write(hw_cmd_p);
			break;

		case MSM_GEMINI_HW_CMD_TYPE_WRITE_OR:
			data = msm_gemini_hw_read(hw_cmd_p);
			hw_cmd_p->data = (hw_cmd_p->data & hw_cmd_p->mask) |
				data;
			msm_gemini_hw_write(hw_cmd_p);
			break;

		case MSM_GEMINI_HW_CMD_TYPE_UWAIT:
			msm_gemini_hw_wait(hw_cmd_p, 1);
			break;

		case MSM_GEMINI_HW_CMD_TYPE_MWAIT:
			msm_gemini_hw_wait(hw_cmd_p, 1000);
			break;

		case MSM_GEMINI_HW_CMD_TYPE_UDELAY:
			msm_gemini_hw_delay(hw_cmd_p, 1);
			break;

		case MSM_GEMINI_HW_CMD_TYPE_MDELAY:
			msm_gemini_hw_delay(hw_cmd_p, 1000);
			break;

		default:
			GMN_PR_ERR("wrong hw command type\n");
			break;
		}

		hw_cmd_p++;
	}
	return is_copy_to_user;
}

void msm_gemini_hw_region_dump(int size)
{
	uint32_t *p;
	uint8_t *p8;

	if (size > gemini_region_size)
		GMN_PR_ERR("%s:%d] wrong region dump size\n",
			__func__, __LINE__);

	p = (uint32_t *) gemini_region_base;
	while (size >= 16) {
		GMN_DBG("0x%08X] %08X %08X %08X %08X\n",
			gemini_region_size - size,
			readl(p), readl(p+1), readl(p+2), readl(p+3));
		p += 4;
		size -= 16;
	}

	if (size > 0) {
		uint32_t d;
		GMN_DBG("0x%08X] ", gemini_region_size - size);
		while (size >= 4) {
			GMN_DBG("%08X ", readl(p++));
			size -= 4;
		}

		d = readl(p);
		p8 = (uint8_t *) &d;
		while (size) {
			GMN_DBG("%02X", *p8++);
			size--;
		}

		GMN_DBG("\n");
	}
}

