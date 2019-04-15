/* Copyright (c) 2012-2017,2019 The Linux Foundation. All rights reserved.
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
#include "msm_jpeg_hw.h"
#include "msm_jpeg_common.h"
#include "msm_camera_io_util.h"

#include <linux/io.h>

int msm_jpeg_hw_pingpong_update(struct msm_jpeg_hw_pingpong *pingpong_hw,
	struct msm_jpeg_hw_buf *buf, void *base)
{
	int buf_free_index = -1;

	if (!pingpong_hw->buf_status[0]) {
		buf_free_index = 0;
	} else if (!pingpong_hw->buf_status[1]) {
		buf_free_index = 1;
	} else {
		JPEG_PR_ERR("%s:%d: pingpong buffer busy\n",
		__func__, __LINE__);
		return -EBUSY;
	}

	pingpong_hw->buf[buf_free_index] = *buf;
	pingpong_hw->buf_status[buf_free_index] = 1;

	if (pingpong_hw->is_fe) {
		/* it is fe */
		msm_jpeg_hw_fe_buffer_update(
			&pingpong_hw->buf[buf_free_index], buf_free_index,
			base);
	} else {
		/* it is we */
		msm_jpeg_hw_we_buffer_update(
			&pingpong_hw->buf[buf_free_index], buf_free_index,
			base);
	}
	return 0;
}

int msm_jpegdma_hw_pingpong_update(struct msm_jpeg_hw_pingpong *pingpong_hw,
	struct msm_jpeg_hw_buf *buf, void *base)
{
	int buf_free_index = -1;

	if (!pingpong_hw->buf_status[0]) {
		buf_free_index = 0;
	} else if (!pingpong_hw->buf_status[1]) {
		buf_free_index = 1;
	} else {
		JPEG_PR_ERR("%s:%d: pingpong buffer busy\n",
		__func__, __LINE__);
		return -EBUSY;
	}

	pingpong_hw->buf[buf_free_index] = *buf;
	pingpong_hw->buf_status[buf_free_index] = 1;

	if (pingpong_hw->is_fe) {
		/* it is fe */
		msm_jpegdma_hw_fe_buffer_update(
			&pingpong_hw->buf[buf_free_index], buf_free_index,
			base);
	} else {
		/* it is we */
		msm_jpegdma_hw_we_buffer_update(
			&pingpong_hw->buf[buf_free_index], buf_free_index,
			base);
	}
	return 0;
}
void *msm_jpeg_hw_pingpong_irq(struct msm_jpeg_hw_pingpong *pingpong_hw)
{
	struct msm_jpeg_hw_buf *buf_p = NULL;

	if (pingpong_hw->buf_status[pingpong_hw->buf_active_index]) {
		buf_p = &pingpong_hw->buf[pingpong_hw->buf_active_index];
		pingpong_hw->buf_status[pingpong_hw->buf_active_index] = 0;
	}

	pingpong_hw->buf_active_index = !pingpong_hw->buf_active_index;

	return (void *) buf_p;
}

void *msm_jpeg_hw_pingpong_active_buffer(
	struct msm_jpeg_hw_pingpong *pingpong_hw)
{
	struct msm_jpeg_hw_buf *buf_p = NULL;

	if (pingpong_hw->buf_status[pingpong_hw->buf_active_index])
		buf_p = &pingpong_hw->buf[pingpong_hw->buf_active_index];

	return (void *) buf_p;
}

struct msm_jpeg_hw_cmd hw_cmd_irq_get_status[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_READ, 1, JPEG_IRQ_STATUS_ADDR,
		JPEG_IRQ_STATUS_BMSK, {0} },
};

int msm_jpeg_hw_irq_get_status(void *base)
{
	uint32_t n_irq_status = 0;
	n_irq_status = msm_jpeg_hw_read(&hw_cmd_irq_get_status[0], base);
	return n_irq_status;
}

struct msm_jpeg_hw_cmd hw_cmd_irq_get_dmastatus[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_READ, 1, JPEGDMA_IRQ_STATUS_ADDR,
		JPEGDMA_IRQ_STATUS_BMSK, {0} },
};

int msm_jpegdma_hw_irq_get_status(void *base)
{
	uint32_t n_irq_status = 0;
	n_irq_status = msm_jpeg_hw_read(&hw_cmd_irq_get_dmastatus[0], base);
	return n_irq_status;
}

struct msm_jpeg_hw_cmd hw_cmd_encode_output_size[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_READ, 1,
	JPEG_ENCODE_OUTPUT_SIZE_STATUS_ADDR,
	JPEG_ENCODE_OUTPUT_SIZE_STATUS_BMSK, {0} } ,
};

long msm_jpeg_hw_encode_output_size(void *base)
{
	uint32_t encode_output_size = 0;

	encode_output_size = msm_jpeg_hw_read(&hw_cmd_encode_output_size[0],
		base);

	return encode_output_size;
}

void msm_jpeg_hw_irq_clear(uint32_t mask, uint32_t data, void *base)
{
	struct msm_jpeg_hw_cmd cmd_irq_clear;

	cmd_irq_clear.type = MSM_JPEG_HW_CMD_TYPE_WRITE;
	cmd_irq_clear.n = 1;
	cmd_irq_clear.offset = JPEG_IRQ_CLEAR_ADDR;
	cmd_irq_clear.mask = mask;
	cmd_irq_clear.data = data;
	JPEG_DBG("%s:%d] mask %0x data %0x", __func__, __LINE__, mask, data);
	msm_jpeg_hw_write(&cmd_irq_clear, base);
}

void msm_jpegdma_hw_irq_clear(uint32_t mask, uint32_t data, void *base)
{
	struct msm_jpeg_hw_cmd cmd_irq_clear;

	cmd_irq_clear.type = MSM_JPEG_HW_CMD_TYPE_WRITE;
	cmd_irq_clear.n = 1;
	cmd_irq_clear.offset = JPEGDMA_IRQ_CLEAR_ADDR;
	cmd_irq_clear.mask = mask;
	cmd_irq_clear.data = data;
	JPEG_DBG("%s:%d] mask %0x data %0x", __func__, __LINE__, mask, data);
	msm_jpeg_hw_write(&cmd_irq_clear, base);
}

struct msm_jpeg_hw_cmd hw_cmd_fe_ping_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_IRQ_MASK_ADDR,
		JPEG_IRQ_MASK_BMSK, {JPEG_IRQ_ALLSOURCES_ENABLE} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_CMD_ADDR,
		JPEG_CMD_BMSK, {JPEG_CMD_CLEAR_WRITE_PLN_QUEUES} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN0_RD_OFFSET_ADDR,
		JPEG_PLN0_RD_OFFSET_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN0_RD_PNTR_ADDR,
		JPEG_PLN0_RD_PNTR_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN1_RD_OFFSET_ADDR,
		JPEG_PLN1_RD_OFFSET_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN1_RD_PNTR_ADDR,
		JPEG_PLN1_RD_PNTR_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN2_RD_OFFSET_ADDR,
		JPEG_PLN1_RD_OFFSET_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN2_RD_PNTR_ADDR,
		JPEG_PLN2_RD_PNTR_BMSK, {0} },
};

void msm_jpeg_hw_fe_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *base)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	if (pingpong_index == 0) {
		hw_cmd_p = &hw_cmd_fe_ping_update[0];
		/* ensure write is done */
		wmb();
		msm_jpeg_hw_write(hw_cmd_p++, base);
		/* ensure write is done */
		wmb();
		msm_jpeg_hw_write(hw_cmd_p++, base);
		/* ensure write is done */
		wmb();
		msm_jpeg_hw_write(hw_cmd_p++, base);
		/* ensure write is done */
		wmb();
		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = p_input->y_buffer_addr;
		msm_jpeg_hw_write(&tmp_hw_cmd, base);
		/* ensure write is done */
		wmb();
		msm_jpeg_hw_write(hw_cmd_p++, base);
		/* ensure write is done */
		wmb();
		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = p_input->cbcr_buffer_addr;
		msm_jpeg_hw_write(&tmp_hw_cmd, base);
		/* ensure write is done */
		wmb();
		msm_jpeg_hw_write(hw_cmd_p++, base);
		/* ensure write is done */
		wmb();
		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = p_input->pln2_addr;
		msm_jpeg_hw_write(&tmp_hw_cmd, base);
		/* ensure write is done */
		wmb();
	}
	return;
}

struct msm_jpeg_hw_cmd hw_dma_cmd_fe_ping_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEGDMA_IRQ_MASK_ADDR,
		JPEGDMA_IRQ_MASK_BMSK, {JPEG_IRQ_ALLSOURCES_ENABLE} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEGDMA_CMD_ADDR,
		JPEGDMA_CMD_BMSK, {JPEGDMA_CMD_CLEAR_READ_PLN_QUEUES} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_FE_0_RD_PNTR,
		JPEG_PLN0_RD_PNTR_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_FE_1_RD_PNTR,
		JPEG_PLN1_RD_PNTR_BMSK, {0} },
};

void msm_jpegdma_hw_fe_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *base)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	if (pingpong_index != 0)
		return;

	hw_cmd_p = &hw_dma_cmd_fe_ping_update[0];
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = p_input->y_buffer_addr;
	msm_jpeg_hw_write(&tmp_hw_cmd, base);
	/* ensure write is done */
	wmb();
	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = p_input->cbcr_buffer_addr;
	msm_jpeg_hw_write(&tmp_hw_cmd, base);
	/* ensure write is done */
	wmb();
}

struct msm_jpeg_hw_cmd hw_cmd_fe_start[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_CMD_ADDR,
		JPEG_CMD_BMSK, {JPEG_OFFLINE_CMD_START} },
};

void msm_jpeg_hw_fe_start(void *base)
{
	msm_jpeg_hw_write(&hw_cmd_fe_start[0], base);

	return;
}

struct msm_jpeg_hw_cmd hw_cmd_we_ping_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN0_WR_PNTR_ADDR,
		JPEG_PLN0_WR_PNTR_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN1_WR_PNTR_ADDR,
		JPEG_PLN0_WR_PNTR_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_PLN2_WR_PNTR_ADDR,
		JPEG_PLN0_WR_PNTR_BMSK, {0} },
};

void msm_jpeg_decode_status(void *base)
{
	uint32_t data;
	data = msm_camera_io_r(base + JPEG_DECODE_MCUS_DECODED_STATUS);
	JPEG_DBG_HIGH("Decode MCUs decode status %u", data);
	data = msm_camera_io_r(base + JPEG_DECODE_BITS_CONSUMED_STATUS);
	JPEG_DBG_HIGH("Decode bits consumed status %u", data);
	data = msm_camera_io_r(base + JPEG_DECODE_PRED_Y_STATE);
	JPEG_DBG_HIGH("Decode prediction Y state %u", data);
	data = msm_camera_io_r(base + JPEG_DECODE_PRED_C_STATE);
	JPEG_DBG_HIGH("Decode prediction C state %u", data);
	data = msm_camera_io_r(base + JPEG_DECODE_RSM_STATE);
	JPEG_DBG_HIGH("Decode prediction RSM state %u", data);
}


void msm_jpeg_hw_we_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *base)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	if (pingpong_index == 0) {
		hw_cmd_p = &hw_cmd_we_ping_update[0];
		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = p_input->y_buffer_addr;
		JPEG_DBG_HIGH("%s Output pln0 buffer address is %x\n", __func__,
			p_input->y_buffer_addr);
		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = p_input->cbcr_buffer_addr;
		JPEG_DBG_HIGH("%s Output pln1 buffer address is %x\n", __func__,
			p_input->cbcr_buffer_addr);
		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = p_input->pln2_addr;
		JPEG_DBG_HIGH("%s Output pln2 buffer address is %x\n", __func__,
			p_input->pln2_addr);
		msm_jpeg_hw_write(&tmp_hw_cmd, base);
	}
	return;
}

struct msm_jpeg_hw_cmd hw_dma_cmd_we_ping_update[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEGDMA_CMD_ADDR,
		JPEGDMA_CMD_BMSK, {JPEGDMA_CMD_CLEAR_WRITE_PLN_QUEUES} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_WE_0_WR_PNTR,
		JPEG_PLN0_WR_PNTR_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_WE_1_WR_PNTR,
		JPEG_PLN0_WR_PNTR_BMSK, {0} },
};
void msm_jpegdma_hw_we_buffer_update(struct msm_jpeg_hw_buf *p_input,
	uint8_t pingpong_index, void *base)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	if (pingpong_index != 0)
		return;

	hw_cmd_p = &hw_dma_cmd_we_ping_update[0];
	msm_jpeg_hw_write(hw_cmd_p++, base);

	/* ensure write is done */
	wmb();
	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = p_input->y_buffer_addr;
	JPEG_DBG_HIGH("%s Output we 0 buffer address is %x\n", __func__,
			p_input->y_buffer_addr);
	msm_jpeg_hw_write(&tmp_hw_cmd, base);
	/* ensure write is done */
	wmb();

	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = p_input->cbcr_buffer_addr;
	JPEG_DBG_HIGH("%s Output we 1 buffer address is %x\n", __func__,
			p_input->cbcr_buffer_addr);
	msm_jpeg_hw_write(&tmp_hw_cmd, base);
	/* ensure write is done */
	wmb();
}

struct msm_jpeg_hw_cmd hw_cmd_fe_mmu_prefetch[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S0_MMU_PF_ADDR_MIN,
		MSM_JPEG_S0_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S0_MMU_PF_ADDR_MAX,
		MSM_JPEG_S0_MMU_PF_ADDR_MAX_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S1_MMU_PF_ADDR_MIN,
		MSM_JPEG_S1_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S1_MMU_PF_ADDR_MAX,
		MSM_JPEG_S1_MMU_PF_ADDR_MAX_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S2_MMU_PF_ADDR_MIN,
		MSM_JPEG_S2_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S2_MMU_PF_ADDR_MAX,
		MSM_JPEG_S2_MMU_PF_ADDR_MAX_BMSK, {0} },
};

/*
 * msm_jpeg_hw_fe_mmu_prefetch() - writes fe min/max addrs for each plane to
 * MMU prefetch registers.
 * @buf: Pointer to jpeg hw buffer.
 * @base: Pointer to base address.
 * @decode_flag: Jpeg decode flag.
 *
 * This function writes fe min/max address for each plane to MMU prefetch
 * registers, MMU prefetch hardware will only prefetch address translations
 * within this min/max boundary.
 *
 * Return: None.
 */
void msm_jpeg_hw_fe_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *base,
	uint8_t decode_flag)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	hw_cmd_p = &hw_cmd_fe_mmu_prefetch[0];
	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = buf->y_buffer_addr;

	JPEG_DBG("%s:%d: MIN y_buf_addr %08x\n",
		__func__, __LINE__, tmp_hw_cmd.data);

	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(&tmp_hw_cmd, base);

	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = buf->y_buffer_addr;
	if (buf->y_len)
		tmp_hw_cmd.data += buf->y_len - 1;

	JPEG_DBG("%s:%d: MAX y_buf_addr %08x, y_len %d\n",
		__func__, __LINE__, tmp_hw_cmd.data, buf->y_len);

	msm_jpeg_hw_write(&tmp_hw_cmd, base);

	if (!decode_flag) {
		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->cbcr_buffer_addr;

		JPEG_DBG("%s:%d: MIN cbcr_buf_addr %08x\n",
			__func__, __LINE__, tmp_hw_cmd.data);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->cbcr_buffer_addr;
		if (buf->cbcr_len)
			tmp_hw_cmd.data	+= buf->cbcr_len - 1;

		JPEG_DBG("%s:%d: MAX cbcr_buf_addr %08x, cbcr_len %d\n"
			, __func__, __LINE__, tmp_hw_cmd.data, buf->cbcr_len);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->pln2_addr;

		JPEG_DBG("%s:%d: MIN pln2_buf_addr %08x\n",
			__func__, __LINE__, tmp_hw_cmd.data);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->pln2_addr;
		if (buf->pln2_len)
			tmp_hw_cmd.data += buf->pln2_len - 1;

		JPEG_DBG("%s:%d: MAX pln2_buf_addr %08x, pln2_len %d\n"
			, __func__, __LINE__, tmp_hw_cmd.data, buf->pln2_len);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);
	}
	/* ensure write is done */
	wmb();
}

struct msm_jpeg_hw_cmd hw_cmd_we_mmu_prefetch[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S1_MMU_PF_ADDR_MIN,
		MSM_JPEG_S1_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S1_MMU_PF_ADDR_MAX,
		MSM_JPEG_S1_MMU_PF_ADDR_MAX_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S2_MMU_PF_ADDR_MIN,
		MSM_JPEG_S2_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S2_MMU_PF_ADDR_MAX,
		MSM_JPEG_S2_MMU_PF_ADDR_MAX_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S3_MMU_PF_ADDR_MIN,
		MSM_JPEG_S3_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEG_S3_MMU_PF_ADDR_MAX,
		MSM_JPEG_S3_MMU_PF_ADDR_MAX_BMSK, {0} },
};

/*
 * msm_jpeg_hw_we_mmu_prefetch() - write we min/max addrs for each plane to
 * MMU prefetch registers.
 * @buf: Pointer to jpeg hw buffer.
 * @base: Pointer to base address.
 * @decode_flag: Jpeg decode flag.
 *
 * This function writes we min/max address for each plane to MMU prefetch
 * registers, MMU prefetch hardware will only prefetch address translations
 * within this min/max boundary.
 *
 * Return: None.
 */
void msm_jpeg_hw_we_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *base,
	uint8_t decode_flag)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	hw_cmd_p = &hw_cmd_we_mmu_prefetch[0];

	/* ensure write is done */
	wmb();
	if (decode_flag) {
		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->y_buffer_addr;

		JPEG_DBG("%s:%d: MIN y_buf_addr %08x\n",
			__func__, __LINE__, tmp_hw_cmd.data);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->y_buffer_addr;
		if (buf->y_len)
			tmp_hw_cmd.data += buf->y_len - 1;

		JPEG_DBG("%s:%d: MAX y_buf_addr %08x, y_len %d\n",
			__func__, __LINE__, tmp_hw_cmd.data, buf->y_len);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->cbcr_buffer_addr;

		JPEG_DBG("%s:%d: MIN cbcr_buf_addr %08x\n",
			__func__, __LINE__, tmp_hw_cmd.data);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->cbcr_buffer_addr;
		if (buf->cbcr_len)
			tmp_hw_cmd.data += buf->cbcr_len - 1;

		JPEG_DBG("%s:%d: MAX cbcr_buf_addr %08x, cbcr_len %d\n"
			, __func__, __LINE__, tmp_hw_cmd.data, buf->cbcr_len);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->pln2_addr;

		JPEG_DBG("%s:%d: MIN pln2_buf_addr %08x\n",
			__func__, __LINE__, tmp_hw_cmd.data);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->pln2_addr;
		if (buf->pln2_len)
			tmp_hw_cmd.data += buf->pln2_len - 1;

		JPEG_DBG("%s:%d: MIN pln2_buf_addr %08x, pln2_len %d\n"
			, __func__, __LINE__, tmp_hw_cmd.data, buf->pln2_len);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);
	} else {
		hw_cmd_p += 4;
		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->y_buffer_addr;

		JPEG_DBG("%s:%d: MIN y_buf_addr %08x\n",
			__func__, __LINE__, tmp_hw_cmd.data);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);

		tmp_hw_cmd = *hw_cmd_p++;
		tmp_hw_cmd.data = buf->y_buffer_addr;
		if (buf->y_len)
			tmp_hw_cmd.data += buf->y_len - 1;

		JPEG_DBG("%s:%d: MAX y_buf_addr %08x, y_len %d\n",
			__func__, __LINE__, tmp_hw_cmd.data, buf->y_len);

		msm_jpeg_hw_write(&tmp_hw_cmd, base);
	}
	/* ensure write is done */
	wmb();
}

struct msm_jpeg_hw_cmd hw_dma_cmd_fe_mmu_prefetch[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_S0_MMU_PF_ADDR_MIN,
		MSM_JPEGDMA_S0_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_S0_MMU_PF_ADDR_MAX,
		MSM_JPEGDMA_S0_MMU_PF_ADDR_MAX_BMSK, {0} },
};

/*
 * msm_jpegdma_hw_fe_mmu_prefetch() - write DMA fe min/max addrs to
 * MMU prefetch registers.
 * @buf: Pointer to jpeg hw buffer.
 * @base: Pointer to base address.
 *
 * This function writes DMA fe min/max address for each plane to MMU prefetch
 * registers, MMU prefetch hardware will only prefetch address translations
 * with in this min/max boundary.
 *
 * Return: None.
 */
void msm_jpegdma_hw_fe_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *base)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	hw_cmd_p = &hw_dma_cmd_fe_mmu_prefetch[0];

	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = buf->y_buffer_addr;

	JPEG_DBG("%s:%d: MIN DMA addr %08x , reg offset %08x\n",
		__func__, __LINE__, tmp_hw_cmd.data, tmp_hw_cmd.offset);

	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(&tmp_hw_cmd, base);

	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = buf->y_buffer_addr;
	if (buf->y_len)
		tmp_hw_cmd.data += buf->y_len - 1;

	JPEG_DBG("%s:%d: MAX DMA addr %08x , reg offset %08x , length %d\n",
		__func__, __LINE__, tmp_hw_cmd.data, tmp_hw_cmd.offset,
		buf->y_len);

	msm_jpeg_hw_write(&tmp_hw_cmd, base);
	/* ensure write is done */
	wmb();
}

struct msm_jpeg_hw_cmd hw_dma_cmd_we_mmu_prefetch[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_S1_MMU_PF_ADDR_MIN,
		MSM_JPEGDMA_S1_MMU_PF_ADDR_MIN_BMSK, {0} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, MSM_JPEGDMA_S1_MMU_PF_ADDR_MAX,
		MSM_JPEGDMA_S1_MMU_PF_ADDR_MAX_BMSK, {0} },
};

/*
 * msm_jpegdma_hw_we_mmu_prefetch() - write DMA we min/max addrs to
 * MMU prefetch registers.
 * @buf: Pointer to jpeg hw buffer.
 * @base: Pointer to base address.
 *
 * This function writes DMA we min/max address for each plane to MMU prefetch
 * registers, MMU prefetch hardware will only prefetch address translations
 * with in this min/max boundary.
 *
 * Return: None.
 */
void msm_jpegdma_hw_we_mmu_prefetch(struct msm_jpeg_hw_buf *buf, void *base)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;
	struct msm_jpeg_hw_cmd tmp_hw_cmd;

	hw_cmd_p = &hw_dma_cmd_we_mmu_prefetch[0];

	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = buf->y_buffer_addr;

	JPEG_DBG("%s:%d: MIN DMA addr %08x , reg offset %08x\n",
		__func__, __LINE__, tmp_hw_cmd.data, tmp_hw_cmd.offset);

	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(&tmp_hw_cmd, base);

	tmp_hw_cmd = *hw_cmd_p++;
	tmp_hw_cmd.data = buf->y_buffer_addr;
	if (buf->y_len)
		tmp_hw_cmd.data += buf->y_len - 1;

	JPEG_DBG("%s:%d: MAX DMA addr %08x , reg offset %08x , length %d\n",
		__func__, __LINE__, tmp_hw_cmd.data, tmp_hw_cmd.offset,
		buf->y_len);

	msm_jpeg_hw_write(&tmp_hw_cmd, base);
	/* ensure write is done */
	wmb();
}

struct msm_jpeg_hw_cmd hw_cmd_reset[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_IRQ_MASK_ADDR,
		JPEG_IRQ_MASK_BMSK, {JPEG_IRQ_DISABLE_ALL} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_IRQ_CLEAR_ADDR,
		JPEG_IRQ_MASK_BMSK, {JPEG_IRQ_CLEAR_ALL} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_IRQ_MASK_ADDR,
		JPEG_IRQ_MASK_BMSK, {JPEG_IRQ_ALLSOURCES_ENABLE} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEG_RESET_CMD_ADDR,
		JPEG_RESET_CMD_RMSK, {JPEG_RESET_DEFAULT} },
};

void msm_jpeg_hw_reset(void *base, int size)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;

	hw_cmd_p = &hw_cmd_reset[0];
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p, base);
	/* ensure write is done */
	wmb();

	return;
}
struct msm_jpeg_hw_cmd hw_cmd_reset_dma[] = {
	/* type, repeat n times, offset, mask, data or pdata */
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEGDMA_IRQ_MASK_ADDR,
		JPEGDMA_IRQ_MASK_BMSK, {JPEGDMA_IRQ_DISABLE_ALL} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEGDMA_IRQ_CLEAR_ADDR,
		JPEGDMA_IRQ_MASK_BMSK, {JPEGDMA_IRQ_CLEAR_ALL} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEGDMA_IRQ_MASK_ADDR,
		JPEGDMA_IRQ_MASK_BMSK, {JPEGDMA_IRQ_ALLSOURCES_ENABLE} },
	{MSM_JPEG_HW_CMD_TYPE_WRITE, 1, JPEGDMA_RESET_CMD_ADDR,
		JPEGDMA_RESET_CMD_BMSK, {JPEGDMA_RESET_DEFAULT} },
};

void msm_jpeg_hw_reset_dma(void *base, int size)
{
	struct msm_jpeg_hw_cmd *hw_cmd_p;

	hw_cmd_p = &hw_cmd_reset_dma[0];
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p++, base);
	/* ensure write is done */
	wmb();
	msm_jpeg_hw_write(hw_cmd_p, base);
	/* ensure write is done */
	wmb();

	return;
}

uint32_t msm_jpeg_hw_read(struct msm_jpeg_hw_cmd *hw_cmd_p,
	 void *jpeg_region_base)
{
	uint32_t *paddr;
	uint32_t data;

	paddr = jpeg_region_base + hw_cmd_p->offset;

	data = msm_camera_io_r(paddr);
	data &= hw_cmd_p->mask;

	return data;
}

void msm_jpeg_hw_write(struct msm_jpeg_hw_cmd *hw_cmd_p,
	void *jpeg_region_base)
{
	uint32_t *paddr;
	uint32_t old_data, new_data;

	paddr = jpeg_region_base + hw_cmd_p->offset;

	if (hw_cmd_p->mask == 0xffffffff) {
		old_data = 0;
	} else {
		old_data = msm_camera_io_r(paddr);
		old_data &= ~hw_cmd_p->mask;
	}

	new_data = hw_cmd_p->data & hw_cmd_p->mask;
	new_data |= old_data;
	JPEG_DBG("%s:%d] %pK %08x\n", __func__, __LINE__,
		paddr, new_data);
	msm_camera_io_w(new_data, paddr);
}

int msm_jpeg_hw_wait(struct msm_jpeg_hw_cmd *hw_cmd_p, int m_us,
	void *base)
{
	int tm = hw_cmd_p->n;
	uint32_t data;
	uint32_t wait_data = hw_cmd_p->data & hw_cmd_p->mask;

	data = msm_jpeg_hw_read(hw_cmd_p, base);
	if (data != wait_data) {
		while (tm) {
			udelay(m_us);
			data = msm_jpeg_hw_read(hw_cmd_p, base);
			if (data == wait_data)
				break;
			tm--;
		}
	}
	hw_cmd_p->data = data;
	return tm;
}

void msm_jpeg_hw_delay(struct msm_jpeg_hw_cmd *hw_cmd_p, int m_us)
{
	int tm = hw_cmd_p->n;
	while (tm) {
		udelay(m_us);
		tm--;
	}
}

int msm_jpeg_hw_exec_cmds(struct msm_jpeg_hw_cmd *hw_cmd_p, uint32_t m_cmds,
	uint32_t max_size, void *base)
{
	int is_copy_to_user = 0;
	uint32_t data;

	while (m_cmds--) {
		if (hw_cmd_p->offset >= max_size) {
			JPEG_PR_ERR("%s:%d] %d exceed hw region %d\n", __func__,
				__LINE__, hw_cmd_p->offset, max_size);
			return -EFAULT;
		}
		if (hw_cmd_p->offset & 0x3) {
			JPEG_PR_ERR("%s:%d] %d Invalid alignment\n", __func__,
					__LINE__, hw_cmd_p->offset);
			return -EFAULT;
		}

		switch (hw_cmd_p->type) {
		case MSM_JPEG_HW_CMD_TYPE_READ:
			hw_cmd_p->data = msm_jpeg_hw_read(hw_cmd_p, base);
			is_copy_to_user = 1;
			break;

		case MSM_JPEG_HW_CMD_TYPE_WRITE:
			msm_jpeg_hw_write(hw_cmd_p, base);
			break;

		case MSM_JPEG_HW_CMD_TYPE_WRITE_OR:
			data = msm_jpeg_hw_read(hw_cmd_p, base);
			hw_cmd_p->data = (hw_cmd_p->data & hw_cmd_p->mask) |
				data;
			msm_jpeg_hw_write(hw_cmd_p, base);
			break;

		case MSM_JPEG_HW_CMD_TYPE_UWAIT:
			msm_jpeg_hw_wait(hw_cmd_p, 1, base);
			break;

		case MSM_JPEG_HW_CMD_TYPE_MWAIT:
			msm_jpeg_hw_wait(hw_cmd_p, 1000, base);
			break;

		case MSM_JPEG_HW_CMD_TYPE_UDELAY:
			msm_jpeg_hw_delay(hw_cmd_p, 1);
			break;

		case MSM_JPEG_HW_CMD_TYPE_MDELAY:
			msm_jpeg_hw_delay(hw_cmd_p, 1000);
			break;

		default:
			JPEG_PR_ERR("wrong hw command type\n");
			break;
		}

		hw_cmd_p++;
	}
	return is_copy_to_user;
}

void msm_jpeg_io_dump(void *base, int size)
{
	char line_str[140];
	void __iomem *addr = (void __iomem *)base;
	int i;
	u32 *p = (u32 *) addr;
	size_t offset = 0;
	size_t used = 0;
	size_t min_range = 0;
	size_t sizeof_line_str = sizeof(line_str);
	u32 data;
	JPEG_DBG_HIGH("%s:%d] %pK %d", __func__, __LINE__, addr, size);
	line_str[0] = '\0';
	for (i = 0; i < size; i = i+4) {
		if (i % 4 == 0) {
			used = snprintf(line_str + offset,
				sizeof_line_str - offset, "%pK", p+i);
			if ((used < min_range) ||
				(offset + used >= sizeof_line_str)) {
				JPEG_PR_ERR("%s\n", line_str);
				offset = 0;
				line_str[0] = '\0';
			} else {
				offset += used;
			}
		}
		data = msm_camera_io_r((void __iomem *) (p + i));
		used = snprintf(line_str + offset,
			sizeof_line_str - offset, " - %08x ", data);
		if ((used < min_range) ||
			(offset + used >= sizeof_line_str)) {
			JPEG_PR_ERR("%s\n", line_str);
			offset = 0;
			line_str[0] = '\0';
		} else {
			offset += used;
		}
	}
	if (line_str[0] != '\0')
		JPEG_DBG_HIGH("%s\n", line_str);
}

