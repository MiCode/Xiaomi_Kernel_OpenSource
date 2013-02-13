/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include "msm_mercury_hw.h"
#include "msm_mercury_common.h"
#include "msm_mercury_hw_reg.h"
#include "msm_mercury_macros.h"

static void *mercury_region_base;
static uint32_t mercury_region_size;


void msm_mercury_hw_write(struct msm_mercury_hw_cmd *hw_cmd_p)
{
	uint32_t *paddr;
	uint32_t old_data, new_data;

	paddr = mercury_region_base + hw_cmd_p->offset;

	if (hw_cmd_p->mask == 0xffffffff) {
		old_data = 0;
	} else {
		old_data = readl_relaxed(paddr);
		old_data &= ~hw_cmd_p->mask;
	}

	new_data = hw_cmd_p->data & hw_cmd_p->mask;
	new_data |= old_data;
	writel_relaxed(new_data, paddr);
}

uint32_t msm_mercury_hw_read(struct msm_mercury_hw_cmd *hw_cmd_p)
{
	uint32_t *paddr;
	uint32_t data;

	paddr = mercury_region_base + hw_cmd_p->offset;

	data = readl_relaxed(paddr);
	data &= hw_cmd_p->mask;

	MCR_DBG("MERCURY_READ: offset=0x%04X data=0x%08X\n",
		hw_cmd_p->offset, data);

	return data;
}

void msm_mercury_hw_start_decode(void)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kread(JPEG_STATUS);
	mercury_kread(RTDMA_JPEG_RD_STA_ACK);
	mercury_kread(RTDMA_JPEG_WR_STA_ACK);
	mercury_kread(RTDMA_JPEG_RD_BUF_Y_PNTR);
	mercury_kread(RTDMA_JPEG_WR_BUF_Y_PNTR);
	mercury_kread(RTDMA_JPEG_WR_BUF_U_PNTR);
	mercury_kwrite(RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO, (7<<2));
	return;
}

void msm_mercury_hw_bitstream_buf_cfg(uint32_t bitstream_buf_addr)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(RTDMA_JPEG_RD_BUF_Y_PNTR, bitstream_buf_addr);
	return;
}


void msm_mercury_hw_output_y_buf_cfg(uint32_t y_buf_addr)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(RTDMA_JPEG_WR_BUF_Y_PNTR, y_buf_addr);
	return;
}

void msm_mercury_hw_output_u_buf_cfg(uint32_t u_buf_addr)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(RTDMA_JPEG_WR_BUF_U_PNTR, u_buf_addr);
	return;
}

void msm_mercury_hw_output_v_buf_cfg(uint32_t v_buf_addr)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(RTDMA_JPEG_WR_BUF_V_PNTR, v_buf_addr);
	return;
}

int msm_mercury_hw_wait(struct msm_mercury_hw_cmd *hw_cmd_p, int m_us)
{
	int tm = hw_cmd_p->n;
	uint32_t data;
	uint32_t wait_data = hw_cmd_p->data & hw_cmd_p->mask;

	data = msm_mercury_hw_read(hw_cmd_p);
	if (data != wait_data) {
		while (tm) {
			udelay(m_us);
			data = msm_mercury_hw_read(hw_cmd_p);
			if (data == wait_data)
				break;
			tm--;
		}
	}
	hw_cmd_p->data = data;
	return tm;
}

void msm_mercury_hw_irq_get_status(uint16_t *rd_irq, uint16_t *wr_irq)
{
	struct msm_mercury_hw_cmd hw_cmd;
	rmb();
	mercury_kread(RTDMA_JPEG_RD_STA_ACK);
	*rd_irq = hw_cmd.data;

	mercury_kread(RTDMA_JPEG_WR_STA_ACK);
	*wr_irq = hw_cmd.data;
	rmb();
}

void msm_mercury_hw_get_jpeg_status(uint32_t *jpeg_status)
{
	struct msm_mercury_hw_cmd hw_cmd;

	rmb();
	mercury_kread(JPEG_STATUS);
	*jpeg_status = hw_cmd.data;
	rmb();
}

uint32_t msm_mercury_get_restartInterval(void)
{
	struct msm_mercury_hw_cmd hw_cmd;

	rmb();
	mercury_kread(JPEG_DRI);
	rmb();
	return hw_cmd.data;

}

void msm_mercury_hw_rd_irq_clear(uint32_t val)
{
	struct msm_mercury_hw_cmd hw_cmd;
	mercury_kwrite(RTDMA_JPEG_RD_STA_ACK, val);
}

void msm_mercury_hw_wr_irq_clear(uint32_t val)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(RTDMA_JPEG_WR_STA_ACK, val);
}

void msm_mercury_hw_set_rd_irq_mask(uint32_t val)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(RTDMA_JPEG_RD_INT_EN, val);
}

void msm_mercury_hw_set_wr_irq_mask(uint32_t val)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(RTDMA_JPEG_WR_INT_EN, val);
}

void msm_mercury_set_jpeg_ctl_common(uint32_t val)
{
	struct msm_mercury_hw_cmd hw_cmd;

	mercury_kwrite(JPEG_CTRL_COMMON, val);
}

void msm_mercury_hw_reset(void)
{
	uint32_t val;
	struct msm_mercury_hw_cmd hw_cmd;

	wmb();
	/* disable all interrupts*/
	mercury_kwrite(RTDMA_JPEG_RD_INT_EN, 0);

	mercury_kwrite(RTDMA_JPEG_WR_INT_EN, 0);

	/* clear pending interrupts*/
	val = 0;
	MEM_OUTF2(&val, RTDMA_JPEG_WR_STA_ACK,
		SW_RESET_ABORT_RDY_ACK,
		ERR_ACK, 1, 1);
	MEM_OUTF2(&val, RTDMA_JPEG_WR_STA_ACK, EOF_ACK, SOF_ACK, 1, 1);
	mercury_kwrite(RTDMA_JPEG_WR_STA_ACK, val);

	val = 0;
	MEM_OUTF2(&val, RTDMA_JPEG_RD_STA_ACK, EOF_ACK, SOF_ACK, 1, 1);
	mercury_kwrite(RTDMA_JPEG_RD_STA_ACK, val);

	/* enable SWResetAbortRdyInt for core reset*/
	val = 0;
	MEM_OUTF(&val, RTDMA_JPEG_WR_INT_EN, SW_RESET_ABORT_RDY_EN, 1);
	mercury_kwrite(RTDMA_JPEG_WR_INT_EN, val);

	/* Reset Core from MMSS Fabric*/
	mercury_core_reset();

	/* disable all interrupts*/
	mercury_kwrite(RTDMA_JPEG_WR_INT_EN, 0);

	/* clear pending interrupts*/
	val = 0;
	MEM_OUTF2(&val, RTDMA_JPEG_WR_STA_ACK,
		SW_RESET_ABORT_RDY_ACK,
		ERR_ACK, 1, 1);
	MEM_OUTF2(&val, RTDMA_JPEG_WR_STA_ACK, EOF_ACK, SOF_ACK, 1, 1);
	mercury_kwrite(RTDMA_JPEG_WR_STA_ACK, val);

	val = 0;
	MEM_OUTF2(&val, RTDMA_JPEG_RD_STA_ACK, EOF_ACK, SOF_ACK, 1, 1);
	mercury_kwrite(RTDMA_JPEG_RD_STA_ACK, val);

	/* enable neccessary interrupt source*/
	val = 0;
	MEM_OUTF2(&val, RTDMA_JPEG_WR_INT_EN, EOF_EN, ERR_EN, 1, 1);
	MEM_OUTF(&val, RTDMA_JPEG_WR_INT_EN, SW_RESET_ABORT_RDY_EN, 1);
	mercury_kwrite(RTDMA_JPEG_WR_INT_EN, val);

	wmb();

}

void msm_mercury_hw_init(void *base, int size)
{
	mercury_region_base = base;
	mercury_region_size = size;
}


void msm_mercury_hw_delay(struct msm_mercury_hw_cmd *hw_cmd_p, int m_us)
{
	int tm = hw_cmd_p->n;
	while (tm) {
		udelay(m_us);
		tm--;
	}
}

int msm_mercury_hw_exec_cmds(struct msm_mercury_hw_cmd *hw_cmd_p, int m_cmds)
{
	int is_copy_to_user = -1;
	uint32_t data;
	if (m_cmds > 1)
		MCR_DBG("m_cmds = %d\n", m_cmds);

	while (m_cmds--) {
		if (hw_cmd_p->offset > mercury_region_size) {
			MCR_PR_ERR("%s:%d] %d exceed hw region %d\n",
					__func__, __LINE__, hw_cmd_p->offset,
					mercury_region_size);
			return -EFAULT;
		}

		switch (hw_cmd_p->type) {
		case MSM_MERCURY_HW_CMD_TYPE_READ:
			hw_cmd_p->data = msm_mercury_hw_read(hw_cmd_p);
			is_copy_to_user = 1;
			break;

		case MSM_MERCURY_HW_CMD_TYPE_WRITE:
			msm_mercury_hw_write(hw_cmd_p);
			break;

		case MSM_MERCURY_HW_CMD_TYPE_WRITE_OR:
			data = msm_mercury_hw_read(hw_cmd_p);
			hw_cmd_p->data = (hw_cmd_p->data & hw_cmd_p->mask) |
				data;
			msm_mercury_hw_write(hw_cmd_p);
			break;

		case MSM_MERCURY_HW_CMD_TYPE_UWAIT:
			msm_mercury_hw_wait(hw_cmd_p, 1);
			break;

		case MSM_MERCURY_HW_CMD_TYPE_MWAIT:
			msm_mercury_hw_wait(hw_cmd_p, 1000);
			break;

		case MSM_MERCURY_HW_CMD_TYPE_UDELAY:
			msm_mercury_hw_delay(hw_cmd_p, 1);
			break;

		case MSM_MERCURY_HW_CMD_TYPE_MDELAY:
			msm_mercury_hw_delay(hw_cmd_p, 1000);
			break;

		default:
			MCR_DBG("wrong hw command type\n");
			break;
		}

		hw_cmd_p++;
	}
	return is_copy_to_user;
}

void msm_mercury_hw_region_dump(int size)
{
	uint32_t *p;
	uint8_t *p8;

	MCR_DBG("(%d)%s()\n", __LINE__, __func__);
	if (size > mercury_region_size)
		MCR_DBG("%s:%d] wrong region dump size\n",
			__func__, __LINE__);

	p = (uint32_t *) mercury_region_base;
	while (size >= 16) {
		MCR_DBG("0x%08X] %08X %08X %08X %08X\n",
			mercury_region_size - size,
			readl_relaxed(p), readl_relaxed(p+1),
			readl_relaxed(p+2), readl_relaxed(p+3));
		p += 4;
		size -= 16;
	}

	if (size > 0) {
		uint32_t d;
		MCR_DBG("0x%08X] ", mercury_region_size - size);
		while (size >= 4) {
			MCR_DBG("%08X ", readl_relaxed(p++));
			size -= 4;
		}

		d = readl_relaxed(p);
		p8 = (uint8_t *) &d;
		while (size) {
			MCR_DBG("%02X", *p8++);
			size--;
		}

		MCR_DBG("\n");
	}
}
