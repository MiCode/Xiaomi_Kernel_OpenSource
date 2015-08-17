/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/iommu.h>
#include <linux/msm_ion.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <media/videobuf2-core.h>

#include "msm_camera_io_util.h"
#include "cam_smmu_api.h"
#include "msm_jpeg_dma_dev.h"
#include "msm_jpeg_dma_hw.h"
#include "msm_jpeg_dma_regs.h"

/* Jpeg dma scale unity */
#define MSM_JPEGDMA_SCALE_UNI (1 << 21)
/* Jpeg dma bw numerator */
#define MSM_JPEGDMA_BW_NUM 38
/* Jpeg dma bw denominator */
#define MSM_JPEGDMA_BW_DEN 10
/* Jpeg bus client name */
#define MSM_JPEGDMA_BUS_CLIENT_NAME "msm_jpeg_dma"
/* Jpeg dma engine timeout in ms */
#define MSM_JPEGDMA_TIMEOUT_MS 500
/* Jpeg dma smmu name */
#define MSM_JPEGDMA_SMMU_NAME "jpeg_dma"

static const struct msm_jpegdma_block msm_jpegdma_block_sel[] = {
	{
		.div = 0x3C0000,
		.width = 256,
		.reg_val = 4,
	},
	{
		.div = 0x7C0000,
		.width = 128,
		.reg_val = 3,
	},
	{
		.div = 0xFC0000,
		.width = 64,
		.reg_val = 2,
	},
	{
		.div = 0x1FC0000,
		.width = 32,
		.reg_val = 1,
	},
	{
		.div = 0x4000000,
		.width = 16,
		.reg_val = 0,
	},
};

/*
 * msm_jpegdma_hw_read_reg - dma read from register.
 * @dma: Pointer to dma device.
 * @base_idx: dma memory resource index.
 * @reg: Register addr need to be read from.
 */
static inline u32 msm_jpegdma_hw_read_reg(struct msm_jpegdma_device *dma,
	enum msm_jpegdma_mem_resources base_idx, u32 reg)
{
	return msm_camera_io_r(dma->iomem_base[base_idx] + reg);
}

/*
 * msm_jpegdma_hw_write_reg - dma write to register.
 * @dma: Pointer to dma device.
 * @base_idx: dma memory resource index.
 * @reg: Register addr need to be read from.
 * @value: Value to be written.
 */
static inline void msm_jpegdma_hw_write_reg(struct msm_jpegdma_device *dma,
	enum msm_jpegdma_mem_resources base_idx, u32 reg, u32 value)
{
	pr_debug("%s:%d]%p %08x\n", __func__, __LINE__,
		dma->iomem_base[base_idx] + reg,
		value);
	msm_camera_io_w(value, dma->iomem_base[base_idx] + reg);
}

/*
 * msm_jpegdma_hw_enable_irq - Enable dma interrupts.
 * @dma: Pointer to dma device.
 */
static void msm_jpegdma_hw_enable_irq(struct msm_jpegdma_device *dma)
{
	u32 reg;

	reg = MSM_JPEGDMA_IRQ_MASK_SESSION_DONE |
		MSM_JPEGDMA_IRQ_MASK_AXI_HALT |
		MSM_JPEGDMA_IRQ_MASK_RST_DONE;

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_IRQ_MASK_ADDR, reg);
}

/*
 * msm_jpegdma_hw_disable_irq - Disable dma interrupts.
 * @dma: Pointer to dma device.
 */
static void msm_jpegdma_hw_disable_irq(struct msm_jpegdma_device *dma)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_IRQ_MASK_ADDR, 0);
}

/*
 * msm_jpegdma_hw_clear_irq - Clear dma interrupts.
 * @dma: Pointer to dma device.
 * @status: Status to clear.
 */
static void msm_jpegdma_hw_clear_irq(struct msm_jpegdma_device *dma,
	u32 status)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_IRQ_CLEAR_ADDR, status);
}

/*
 * msm_jpegdma_hw_get_irq_status - Get dma irq status
 * @dma: Pointer to dma device.
 */
static u32 msm_jpegdma_hw_get_irq_status(struct msm_jpegdma_device *dma)
{
	return msm_jpegdma_hw_read_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_IRQ_STATUS);
}

/*
 * msm_jpegdma_hw_get_num_pipes - Get number of dma pipes
 * @dma: Pointer to dma device.
 */
static int msm_jpegdma_hw_get_num_pipes(struct msm_jpegdma_device *dma)
{
	int num_pipes;
	u32 reg;

	reg = msm_jpegdma_hw_read_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_HW_CAPABILITY);

	num_pipes = (reg & MSM_JPEGDMA_HW_CAPABILITY_NUM_PIPES_BMSK) >>
		MSM_JPEGDMA_HW_CAPABILITY_NUM_PIPES_SHFT;

	return num_pipes;
}

/*
 * msm_jpegdma_hw_reset - Reset jpeg dma core.
 * @dma: Pointer to dma device.
 */
static int msm_jpegdma_hw_reset(struct msm_jpegdma_device *dma)
{
	unsigned long time;

	init_completion(&dma->hw_reset_completion);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_HW_JPEGDMA_RESET, MSM_HW_JPEGDMA_RESET_DEFAULT);

	time = wait_for_completion_timeout(&dma->hw_reset_completion,
		msecs_to_jiffies(MSM_JPEGDMA_TIMEOUT_MS));
	if (!time) {
		dev_err(dma->dev, "Jpeg dma detection reset timeout\n");
		return -ETIME;
	}
	return 0;
}

/*
* msm_jpegdma_hw_halt - Halt jpeg dma core.
* @dma: Pointer to dma device.
*/
static int msm_jpegdma_hw_halt(struct msm_jpegdma_device *dma)
{
	unsigned long time;

	init_completion(&dma->hw_halt_completion);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_CMD_ADDR, 0x4);

	time = wait_for_completion_timeout(&dma->hw_halt_completion,
		msecs_to_jiffies(MSM_JPEGDMA_TIMEOUT_MS));
	if (!time) {
		dev_err(dma->dev, "Jpeg dma detection halt timeout\n");
		return -ETIME;
	}
	return 0;
}

/*
* msm_jpegdma_hw_run - Enable dma processing.
* @dma: Pointer to dma device.
*/
static int msm_jpegdma_hw_run(struct msm_jpegdma_device *dma)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_CMD_ADDR, 0x1);

	return 0;
}

/*
 * msm_jpegdma_hw_core_config - Set jpeg dma core configuration.
 * @dma: Pointer to dma device.
 * @num_pipes: Number of pipes.
 * @scale_0: Scaler 0 enable.
 * @scale_1: Scaler 1 enable.
 */
static int msm_jpegdma_hw_core_config(struct msm_jpegdma_device *dma,
	int num_pipes, int scale_0, int scale_1)
{
	u32 reg;

	reg = (scale_0 << MSM_JPEGDMA_CORE_CFG_SCALE_0_ENABLE_SHFT) |
		(0x1 << MSM_JPEGDMA_CORE_CFG_TEST_BUS_ENABLE_SHFT) |
		(0x1 << MSM_JPEGDMA_CORE_CFG_BRIDGE_ENABLE_SHFT) |
		(0x1 << MSM_JPEGDMA_CORE_CFG_WE_0_ENABLE_SHFT) |
		(0x1 << MSM_JPEGDMA_CORE_CFG_FE_0_ENABLE_SHFT);

	/* Enable read write ports for second pipe */
	if (num_pipes > 1) {
		reg |= (scale_1 << MSM_JPEGDMA_CORE_CFG_SCALE_1_ENABLE_SHFT) |
			(0x1 << MSM_JPEGDMA_CORE_CFG_WE_1_ENABLE_SHFT) |
			(0x1 << MSM_JPEGDMA_CORE_CFG_FE_1_ENABLE_SHFT);
	}
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_CORE_CFG_ADDR, reg);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_0_block - Fetch engine 0 block configuration.
 * @dma: Pointer to dma device.
 * @block_config: Pointer to block configuration.
 * @plane_type: Plane type.
 */
static int msm_jpegdma_hw_fe_0_block(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_block_config *block_config,
	enum msm_jpegdma_plane_type plane_type)
{
	u32 reg;

	switch (plane_type) {
	case JPEGDMA_PLANE_TYPE_Y:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_Y <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	case JPEGDMA_PLANE_TYPE_CB:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_CB <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	case JPEGDMA_PLANE_TYPE_CR:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_CR <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	case JPEGDMA_PLANE_TYPE_CBCR:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_CBCR <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	default:
		dev_err(dma->dev, "Unsupported plane type %d\n", plane_type);
		return -EINVAL;
	}

	reg |= (block_config->block.reg_val <<
		MSM_JPEGDMA_FE_CFG_BLOCK_WIDTH_SHFT) |
		(0x1 << MSM_JPEGDMA_FE_CFG_MAL_BOUNDARY_SHFT) |
		(0x1 << MSM_JPEGDMA_FE_CFG_MAL_EN_SHFT) |
		(0xF << MSM_JPEGDMA_FE_CFG_BURST_LENGTH_MAX_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_0_CFG_ADDR, reg);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_1_block - Fetch engine 1 block configuration.
 * @dma: Pointer to dma device.
 * @block_config: Pointer to block configuration.
 * @plane_type: Plane type.
 */
static int msm_jpegdma_hw_fe_1_block(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_block_config *block_config,
	enum msm_jpegdma_plane_type plane_type)
{
	u32 reg;

	switch (plane_type) {
	case JPEGDMA_PLANE_TYPE_Y:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_Y <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	case JPEGDMA_PLANE_TYPE_CB:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_CB <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	case JPEGDMA_PLANE_TYPE_CR:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_CR <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	case JPEGDMA_PLANE_TYPE_CBCR:
		reg = MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_CBCR <<
			MSM_JPEGDMA_FE_CFG_PLN_BLOCK_TYPE_SHFT;
		break;
	default:
		dev_err(dma->dev, "Unsupported plane type %d\n", plane_type);
		return -EINVAL;
	}

	reg |= (block_config->block.reg_val <<
		MSM_JPEGDMA_FE_CFG_BLOCK_WIDTH_SHFT) |
		(0xF << MSM_JPEGDMA_FE_CFG_BURST_LENGTH_MAX_SHFT) |
		(0x1 << MSM_JPEGDMA_FE_CFG_MAL_EN_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_1_CFG_ADDR, reg);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_0_phase - Fetch engine 0 phase configuration.
 * @dma: Pointer to dma device.
 * @phase: Fetch engine 0 phase.
 */
static int msm_jpegdma_hw_fe_0_phase(struct msm_jpegdma_device *dma, int phase)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_0_HINIT_ADDR, 0x00);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_0_HINIT_INT_ADDR, 0x00);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_0_VINIT_INT_ADDR, 0x00);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_0_VINIT_INT_ADDR, phase);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_1_phase - Fetch engine 1 phase configuration.
 * @dma: Pointer to dma device.
 * @phase: Fetch engine 1 phase.
 */
static int msm_jpegdma_hw_fe_1_phase(struct msm_jpegdma_device *dma, int phase)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_1_HINIT_ADDR, 0x00);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_1_HINIT_INT_ADDR, 0x00);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_1_VINIT_INT_ADDR, 0x00);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_1_VINIT_INT_ADDR, phase);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_0_size - Fetch engine 0 size configuration.
 * @dma: Pointer to dma device.
 * @size: Pointer to size configuration.
 * @plane_type: Plane type.
 */
static int msm_jpegdma_hw_fe_0_size(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size *size, enum msm_jpegdma_plane_type plane_type)
{
	u32 reg;

	reg = (size->width + size->left - 1) |
		((size->height + size->top - 1) <<
		MSM_JPEGDMA_FE_RD_BUFFER_SIZE_HEIGHT_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_BUFFER_SIZE_0_ADDR, reg);

	if (size->left && plane_type == JPEGDMA_PLANE_TYPE_CBCR)
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_FE_RD_0_HINIT_INT_ADDR, size->left / 2);
	else
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_FE_RD_0_HINIT_INT_ADDR, size->left);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_0_VINIT_INT_ADDR, size->top);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_0_STRIDE_ADDR, size->stride);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_1_size - Fetch engine 1 size configuration.
 * @dma: Pointer to dma device.
 * @size: Pointer to size configuration.
 * @plane_type: Plane type.
 */
static int msm_jpegdma_hw_fe_1_size(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size *size, enum msm_jpegdma_plane_type plane_type)
{
	u32 reg;

	reg = (size->width + size->left - 1) |
		((size->height + size->top - 1) <<
		MSM_JPEGDMA_FE_RD_BUFFER_SIZE_HEIGHT_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_BUFFER_SIZE_1_ADDR, reg);

	if (size->left && plane_type == JPEGDMA_PLANE_TYPE_CBCR)
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_FE_RD_1_HINIT_INT_ADDR, size->left / 2);
	else
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_FE_RD_1_HINIT_INT_ADDR, size->left);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_1_VINIT_INT_ADDR, size->top);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_1_STRIDE_ADDR, size->stride);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_0_addr - Set fetch engine 0 address.
 * @dma: Pointer to dma device.
 * @addr: Fetch engine addres.
 */
static int msm_jpegdma_hw_fe_0_addr(struct msm_jpegdma_device *dma, u32 addr)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_CMD_ADDR, MSM_JPEGDMA_CMD_CLEAR_READ_PLN_QUEUES);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_0_PNTR_ADDR, addr);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_1_addr - Set fetch engine 1 address.
 * @dma: Pointer to dma device.
 * @addr: Fetch engine addres.
 */
static int msm_jpegdma_hw_fe_1_addr(struct msm_jpegdma_device *dma, u32 addr)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_FE_RD_1_PNTR_ADDR, addr);

	return 0;
}

/*
 * msm_jpegdma_hw_fe_0_block - Write engine 0 block configuration.
 * @dma: Pointer to dma device.
 * @block_config: Pointer to block configuration.
 * @plane_type: Plane type.
 */
static int msm_jpegdma_hw_we_0_block(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_block_config *block,
	enum msm_jpegdma_plane_type plane_type)
{
	u32 reg;

	reg = (0xF << MSM_JPEGDMA_WE_CFG_BURST_LENGTH_MAX_SHFT) |
		(0x1 << MSM_JPEGDMA_WE_CFG_MAL_BOUNDARY_SHFT) |
		(0x1 << MSM_JPEGDMA_WE_CFG_MAL_EN_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_CFG_ADDR, reg);

	reg = ((block->blocks_per_row - 1) <<
		MSM_JPEGDMA_WE_PLN_WR_CFG_0_BLOCKS_PER_ROW_SHFT) |
		(block->blocks_per_col - 1);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_0_WR_CFG_0_ADDR, reg);

	reg = ((block->h_step_last - 1) <<
		MSM_JPEGDMA_WE_PLN_WR_CFG_1_LAST_H_STEP_SHFT) |
		(block->h_step - 1);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_0_WR_CFG_1_ADDR, reg);

	reg = ((block->v_step_last - 1) <<
		MSM_JPEGDMA_WE_PLN_WR_CFG_2_LAST_V_STEP_SHFT) |
		(block->v_step - 1);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_0_WR_CFG_2_ADDR, reg);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_0_WR_CFG_3_ADDR, 0x0);

	return 0;
}

/*
 * msm_jpegdma_hw_we_1_block - Write engine 1 block configuration.
 * @dma: Pointer to dma device.
 * @block_config: Pointer to block configuration.
 * @plane_type: Plane type.
 */
static int msm_jpegdma_hw_we_1_block(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_block_config *block,
	enum msm_jpegdma_plane_type plane_type)
{
	u32 reg;

	reg = ((block->blocks_per_row - 1) <<
		MSM_JPEGDMA_WE_PLN_WR_CFG_0_BLOCKS_PER_ROW_SHFT) |
		(block->blocks_per_col - 1);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_1_WR_CFG_0_ADDR, reg);

	reg = ((block->h_step_last - 1) <<
		MSM_JPEGDMA_WE_PLN_WR_CFG_1_LAST_H_STEP_SHFT) |
		(block->h_step - 1);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_1_WR_CFG_1_ADDR, reg);

	reg = ((block->v_step_last - 1) <<
		MSM_JPEGDMA_WE_PLN_WR_CFG_2_LAST_V_STEP_SHFT) |
		(block->v_step - 1);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_1_WR_CFG_2_ADDR, reg);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_1_WR_CFG_3_ADDR, 0x0);

	return 0;
}

/*
 * msm_jpegdma_hw_we_0_size - Write engine 0 size configuration.
 * @dma: Pointer to dma device.
 * @size: Pointer to size configuration.
 */
static int msm_jpegdma_hw_we_0_size(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size *size)
{
	u32 reg;

	reg = (size->width) | ((size->height) <<
		MSM_JPEGDMA_WE_PLN_WR_BUFFER_SIZE_HEIGHT_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_WR_BUFFER_SIZE_0_ADDR, reg);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_0_WR_STRIDE_ADDR, size->stride);

	return 0;
}

/*
 * msm_jpegdma_hw_we_1_size - Write engine 1 size configuration.
 * @dma: Pointer to dma device.
 * @size: Pointer to size configuration.
 */
static int msm_jpegdma_hw_we_1_size(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size *size)
{
	u32 reg;

	reg = (size->width) | ((size->height) <<
		MSM_JPEGDMA_WE_PLN_WR_BUFFER_SIZE_HEIGHT_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_WR_BUFFER_SIZE_1_ADDR, reg);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_1_WR_STRIDE_ADDR, size->stride);

	return 0;
}

/*
 * msm_jpegdma_hw_we_0_addr - Set write engine 0 address.
 * @dma: Pointer to dma device.
 * @addr: Fetch engine addres.
 */
static int msm_jpegdma_hw_we_0_addr(struct msm_jpegdma_device *dma, u32 addr)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_CMD_ADDR, MSM_JPEGDMA_CMD_CLEAR_WRITE_PLN_QUEUES);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_0_WR_PNTR_ADDR, addr);

	return 0;
}

/*
 * msm_jpegdma_hw_we_1_addr - Set write engine 1 address.
 * @dma: Pointer to dma device.
 * @addr: Fetch engine addres.
 */
static int msm_jpegdma_hw_we_1_addr(struct msm_jpegdma_device *dma, u32 addr)
{
	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_WE_PLN_1_WR_PNTR_ADDR, addr);

	return 0;
}

/*
 * msm_jpegdma_hw_scale_0_config - Scale configuration for 0 pipeline.
 * @dma: Pointer to dma device.
 * @scale: Scale configuration.
 */
static int msm_jpegdma_hw_scale_0_config(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_scale *scale)
{
	u32 reg;
	u32 h_down_en;
	u32 v_down_en;

	h_down_en = (scale->hor_scale == MSM_JPEGDMA_SCALE_UNI) ? 0 : 1;
	v_down_en = (scale->ver_scale == MSM_JPEGDMA_SCALE_UNI) ? 0 : 1;

	reg = (h_down_en << MSM_JPEGDMA_PP_SCALE_CFG_HSCALE_ENABLE_SHFT) |
		(v_down_en << MSM_JPEGDMA_PP_SCALE_CFG_VSCALE_ENABLE_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_PP_0_SCALE_CFG_ADDR, reg);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_PP_0_SCALE_PHASEH_STEP_ADDR, scale->hor_scale);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_PP_0_SCALE_PHASEV_STEP_ADDR, scale->ver_scale);

	return 0;
}

/*
 * msm_jpegdma_hw_scale_1_config - Scale configuration for 1 pipeline.
 * @dma: Pointer to dma device.
 * @scale: Scale configuration.
 */
static int msm_jpegdma_hw_scale_1_config(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_scale *scale)
{
	u32 reg;
	u32 h_down_en;
	u32 v_down_en;

	h_down_en = (scale->hor_scale == MSM_JPEGDMA_SCALE_UNI) ? 0 : 1;
	v_down_en = (scale->ver_scale == MSM_JPEGDMA_SCALE_UNI) ? 0 : 1;

	reg = (h_down_en << MSM_JPEGDMA_PP_SCALE_CFG_HSCALE_ENABLE_SHFT) |
		(v_down_en << MSM_JPEGDMA_PP_SCALE_CFG_VSCALE_ENABLE_SHFT);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_PP_1_SCALE_CFG_ADDR, reg);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_PP_1_SCALE_PHASEH_STEP_ADDR, scale->hor_scale);

	msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
		MSM_JPEGDMA_PP_1_SCALE_PHASEV_STEP_ADDR, scale->ver_scale);

	return 0;
}

/*
 * msm_jpegdma_hw_config_qos - Configure qos registers.
 * @dma: Pointer to dma device.
 */
static void msm_jpegdma_hw_config_qos(struct msm_jpegdma_device *dma)
{
	int i;

	if (!dma->qos_regs_num)
		return;

	for (i = 0; i < dma->qos_regs_num; i++)
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			dma->qos_regs[i].reg, dma->qos_regs[i].val);

	return;
}

/*
 * msm_jpegdma_hw_config_vbif - Configure and vbif interface.
 * @dma: Pointer to dma device.
 */
static void msm_jpegdma_hw_config_vbif(struct msm_jpegdma_device *dma)
{
	int i;

	if (!dma->vbif_regs_num)
		return;

	for (i = 0; i < dma->vbif_regs_num; i++)
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_VBIF,
			dma->vbif_regs[i].reg, dma->vbif_regs[i].val);

	return;
}

/*
 * msm_jpegdma_hw_config_mmu_prefetch - Configure mmu prefetch registers.
 * @dma: Pointer to dma device.
 * @min_addr: Pointer to jpeg dma addr, containing min addrs of the plane.
 * @max_addr: Pointer to jpeg dma addr, containing max addrs of the plane.
 */
static void msm_jpegdma_hw_config_mmu_prefetch(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_addr *min_addr,
	struct msm_jpegdma_addr *max_addr)
{
	int i;

	if (!dma->prefetch_regs_num)
		return;

	for (i = 0; i < dma->prefetch_regs_num; i++)
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_VBIF,
			dma->prefetch_regs[i].reg, dma->prefetch_regs[i].val);

	if (min_addr != NULL && max_addr != NULL) {
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_S0_MMU_PF_ADDR_MIN, min_addr->in_addr);
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_S0_MMU_PF_ADDR_MAX, max_addr->in_addr);
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_S1_MMU_PF_ADDR_MIN, min_addr->out_addr);
		msm_jpegdma_hw_write_reg(dma, MSM_JPEGDMA_IOMEM_CORE,
			MSM_JPEGDMA_S1_MMU_PF_ADDR_MAX, max_addr->out_addr);
	}
}

/*
* msm_jpegdma_hw_calc_speed - Calculate speed based on framerate and size.
* @dma: Pointer to dma device.
* @size: Dma user size configuration.
* @speed: Calculated speed.
*/
static int msm_jpegdma_hw_calc_speed(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size_config *size,
	struct msm_jpegdma_speed *speed)
{
	u64 width;
	u64 height;
	u64 real_clock;
	u64 calc_rate;

	width = size->in_size.width + size->in_size.left;
	height = size->in_size.height + size->in_size.top;

	calc_rate = (width * height * size->format.depth * size->fps) / 16;
	real_clock = clk_round_rate(dma->clk[MSM_JPEGDMA_CORE_CLK], calc_rate);
	if (real_clock < 0) {
		dev_err(dma->dev, "Can not round core clock\n");
		return -EINVAL;
	}

	speed->bus_ab = calc_rate * 2;
	speed->bus_ib = (real_clock *
		(MSM_JPEGDMA_BW_NUM + MSM_JPEGDMA_BW_DEN - 1)) /
		MSM_JPEGDMA_BW_DEN;
	speed->core_clock = real_clock;
	dev_dbg(dma->dev, "Speed core clk %llu ab %llu ib %llu fps %d\n",
		speed->core_clock, speed->bus_ab, speed->bus_ib, size->fps);

	return 0;
}

/*
* msm_jpegdma_hw_set_speed - Configure clock and bus bandwidth based on
*   requested speed and dma clients.
* @size: Jpeg dma size configuration.
* @speed: Requested dma speed.
*/
static int msm_jpegdma_hw_set_speed(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size_config *size,
	struct msm_jpegdma_speed *speed)
{
	struct msm_jpegdma_speed new_sp;
	struct msm_jpegdma_size_config new_size;
	int ret;

	if (dma->active_clock_rate >= speed->core_clock)
		return 0;

	new_sp = *speed;
	if (dma->ref_count > 2) {
		new_size = *size;
		new_size.fps = size->fps * ((dma->ref_count + 1) / 2);
		ret = msm_jpegdma_hw_calc_speed(dma, &new_size, &new_sp);
		if (ret < 0)
			return -EINVAL;
	}

	ret = clk_set_rate(dma->clk[MSM_JPEGDMA_CORE_CLK], new_sp.core_clock);
	if (ret < 0) {
		dev_err(dma->dev, "Fail Core clock rate %d\n", ret);
		return -EINVAL;
	}
	dma->active_clock_rate = speed->core_clock;

	dma->bus_vectors.ab = new_sp.bus_ab;
	dma->bus_vectors.ib = new_sp.bus_ib;

	ret = msm_bus_scale_client_update_request(dma->bus_client, 0);
	if (ret < 0) {
		dev_err(dma->dev, "Fail bus scale update %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
* msm_jpegdma_hw_add_plane_offset - Add plane offset to all pipelines.
* @plane: Jpeg dma plane configuration.
* @in_offset: Input plane offset.
* @out_offset: Output plane offset.
*/
static int msm_jpegdma_hw_add_plane_offset(struct msm_jpegdma_plane *plane,
	unsigned int in_offset, unsigned int out_offset)
{
	int i;

	for (i = 0; i < plane->active_pipes; i++) {
		plane->config[i].in_offset += in_offset;
		plane->config[i].out_offset += out_offset;
	}

	return 0;
}

/*
* msm_jpegdma_hw_calc_config - Calculate plane configuration.
* @size_cfg: Size configuration.
* @plane: Plane configuration need to be calculated.
*/
static int msm_jpegdma_hw_calc_config(struct msm_jpegdma_size_config *size_cfg,
	struct msm_jpegdma_plane *plane)
{
	u64 scale_hor, scale_ver, phase;
	u64 in_width, in_height;
	u64 out_width, out_height;
	struct msm_jpegdma_config *config;
	int i;

	if (!size_cfg->out_size.width || !size_cfg->out_size.height)
		return -EINVAL;

	config = &plane->config[0];
	config->scale_cfg.enable = 0;

	in_width = size_cfg->in_size.width;
	out_width = size_cfg->out_size.width;
	scale_hor = (in_width * MSM_JPEGDMA_SCALE_UNI) / out_width;
	if (scale_hor != MSM_JPEGDMA_SCALE_UNI)
		config->scale_cfg.enable = 1;

	in_height = size_cfg->in_size.height;
	out_height = size_cfg->out_size.height;
	scale_ver = (in_height * MSM_JPEGDMA_SCALE_UNI) / out_height;
	if (scale_ver != MSM_JPEGDMA_SCALE_UNI)
		config->scale_cfg.enable = 1;

	config->scale_cfg.ver_scale = scale_ver;
	config->scale_cfg.hor_scale = scale_hor;

	for (i = 0; ARRAY_SIZE(msm_jpegdma_block_sel); i++)
		if (scale_hor <= msm_jpegdma_block_sel[i].div)
			break;

	if (i == ARRAY_SIZE(msm_jpegdma_block_sel))
		return -EINVAL;

	config->block_cfg.block = msm_jpegdma_block_sel[i];

	if (plane->active_pipes > 1) {
		phase = (out_height * scale_ver + (plane->active_pipes - 1)) /
			plane->active_pipes;
		phase &= (MSM_JPEGDMA_SCALE_UNI - 1);
		out_height = (out_height + (plane->active_pipes - 1)) /
			plane->active_pipes;
		in_height = (out_height * scale_ver) / MSM_JPEGDMA_SCALE_UNI;
	}

	config->block_cfg.blocks_per_row = out_width /
		config->block_cfg.block.width;

	config->block_cfg.blocks_per_col = out_height;

	config->block_cfg.h_step = config->block_cfg.block.width;

	config->block_cfg.h_step_last = out_width %
		config->block_cfg.block.width;
	if (!config->block_cfg.h_step_last)
		config->block_cfg.h_step_last = config->block_cfg.h_step;
	else
		config->block_cfg.blocks_per_row++;

	config->block_cfg.v_step = 1;
	config->block_cfg.v_step_last = 1;

	config->size_cfg = *size_cfg;
	config->size_cfg.in_size.width = in_width;
	config->size_cfg.in_size.height = in_height;
	config->size_cfg.out_size.width = out_width;
	config->size_cfg.out_size.height = out_height;
	config->in_offset = 0;
	config->out_offset = 0;

	if (plane->active_pipes > 1) {
		plane->config[1] = *config;
		/* Recalculate offset for second pipe */
		plane->config[1].in_offset =
			config->size_cfg.in_size.scanline *
			config->size_cfg.in_size.stride;

		plane->config[1].out_offset =
			config->size_cfg.out_size.scanline *
			config->size_cfg.out_size.stride;

		plane->config[1].phase = phase;
	}

	return 0;
}

/*
* msm_jpegdma_hw_check_config - Check configuration based on size is possible.
 *@dma: Pointer to dma device.
* @size_cfg: Size configuration.
*/
int msm_jpegdma_hw_check_config(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size_config *size_cfg)
{
	u64 in_width, in_height;
	u64 out_width, out_height;
	u64 scale;

	if (!size_cfg->out_size.width || !size_cfg->out_size.height)
		return -EINVAL;

	in_width = size_cfg->in_size.width;
	out_width = size_cfg->out_size.width;
	scale = ((in_width * MSM_JPEGDMA_SCALE_UNI)) / out_width;
	if (scale < MSM_JPEGDMA_SCALE_UNI)
		return -EINVAL;


	in_height = size_cfg->in_size.height;
	out_height = size_cfg->out_size.height;
	scale = (in_height * MSM_JPEGDMA_SCALE_UNI) / out_height;
	if (scale < MSM_JPEGDMA_SCALE_UNI)
		return -EINVAL;

	return 0;
}

/*
* msm_jpegdma_hw_set_config - Set dma configuration based on size.
 *@dma: Pointer to dma device.
* @size_cfg: Size configuration.
* @plane_cfg: Calculated plane configuration.
*/
int msm_jpegdma_hw_set_config(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_size_config *size_cfg,
	struct msm_jpegdma_plane_config *plane_cfg)
{
	unsigned int in_offset;
	unsigned int out_offset;
	struct msm_jpegdma_size_config plane_size;
	int ret;
	int i;

	if (!size_cfg->format.colplane_h || !size_cfg->format.colplane_v)
		return -EINVAL;

	ret = msm_jpegdma_hw_calc_speed(dma, size_cfg, &plane_cfg->speed);
	if (ret < 0)
		return -EINVAL;

	dma->active_clock_rate = 0;

	plane_cfg->plane[0].active_pipes = dma->hw_num_pipes;
	plane_cfg->plane[0].type = size_cfg->format.planes[0];
	msm_jpegdma_hw_calc_config(size_cfg, &plane_cfg->plane[0]);
	if (size_cfg->format.num_planes == 1)
		return 0;

	in_offset = size_cfg->in_size.scanline *
		size_cfg->in_size.stride;
	out_offset = size_cfg->out_size.scanline *
		size_cfg->out_size.stride;

	memset(&plane_size, 0x00, sizeof(plane_size));
	for (i = 1; i < size_cfg->format.num_planes; i++) {
		plane_cfg->plane[i].active_pipes = dma->hw_num_pipes;
		plane_cfg->plane[i].type = size_cfg->format.planes[i];

		if (size_cfg->in_size.top)
			plane_size.in_size.top = size_cfg->in_size.top /
				size_cfg->format.colplane_v;

		if (size_cfg->in_size.left)
			plane_size.in_size.left = size_cfg->in_size.left /
				size_cfg->format.colplane_h;

		plane_size.in_size.width = size_cfg->in_size.width /
			size_cfg->format.colplane_h;
		plane_size.in_size.height = size_cfg->in_size.height /
			size_cfg->format.colplane_v;
		plane_size.in_size.scanline = size_cfg->in_size.scanline /
			size_cfg->format.colplane_v;

		plane_size.in_size.stride = size_cfg->in_size.stride;

		plane_size.out_size.width = size_cfg->out_size.width /
			size_cfg->format.colplane_h;
		plane_size.out_size.height = size_cfg->out_size.height /
			size_cfg->format.colplane_v;
		plane_size.out_size.scanline = size_cfg->out_size.scanline /
			size_cfg->format.colplane_v;

		plane_size.out_size.stride = size_cfg->out_size.stride;

		plane_size.format = size_cfg->format;
		plane_size.fps = size_cfg->fps;

		msm_jpegdma_hw_calc_config(&plane_size,
			&plane_cfg->plane[i]);

		msm_jpegdma_hw_add_plane_offset(&plane_cfg->plane[i],
			in_offset, out_offset);

		in_offset += (plane_size.in_size.scanline *
			plane_size.in_size.stride);
		out_offset += (plane_size.out_size.scanline *
			plane_size.out_size.stride);
	}
	return 0;
}

/*
* msm_jpegdma_hw_start - Start dma processing.
 *@dma: Pointer to dma device.
* @addr: Input address.
* @plane: Plane configuration.
* @speed: Clock and bus bandwidth configuration.
*/
int msm_jpegdma_hw_start(struct msm_jpegdma_device *dma,
	struct msm_jpegdma_addr *addr,
	struct msm_jpegdma_plane *plane,
	struct msm_jpegdma_speed *speed)
{
	struct msm_jpegdma_config *cfg;
	struct msm_jpegdma_addr prefetch_max_addr;
	unsigned int prefetch_in_size;
	unsigned int prefetch_out_size;

	int ret;

	if (!plane->active_pipes)
		return -EINVAL;

	if (plane->active_pipes > MSM_JPEGDMA_MAX_PIPES)
		return -EINVAL;
	ret = msm_jpegdma_hw_set_speed(dma, &plane->config[0].size_cfg, speed);
	if (ret < 0)
		return -EINVAL;

	msm_jpegdma_hw_core_config(dma, plane->active_pipes,
		plane->config[0].scale_cfg.enable,
		plane->config[1].scale_cfg.enable);

	cfg = &plane->config[0];
	msm_jpegdma_hw_scale_0_config(dma, &cfg->scale_cfg);

	msm_jpegdma_hw_fe_0_block(dma, &cfg->block_cfg, plane->type);
	msm_jpegdma_hw_fe_0_phase(dma, cfg->phase);
	msm_jpegdma_hw_fe_0_size(dma, &cfg->size_cfg.in_size, plane->type);
	msm_jpegdma_hw_fe_0_addr(dma, addr->in_addr + cfg->in_offset);
	prefetch_in_size = cfg->size_cfg.in_size.stride *
		cfg->size_cfg.in_size.scanline;

	msm_jpegdma_hw_we_0_block(dma, &cfg->block_cfg, plane->type);
	msm_jpegdma_hw_we_0_size(dma, &cfg->size_cfg.out_size);
	msm_jpegdma_hw_we_0_addr(dma, addr->out_addr + cfg->out_offset);
	prefetch_out_size = cfg->size_cfg.out_size.stride *
		cfg->size_cfg.out_size.scanline;

	if (plane->active_pipes > 1) {
		cfg = &plane->config[1];
		msm_jpegdma_hw_scale_1_config(dma, &cfg->scale_cfg);

		msm_jpegdma_hw_fe_1_block(dma, &cfg->block_cfg, plane->type);
		msm_jpegdma_hw_fe_1_phase(dma, cfg->phase);
		msm_jpegdma_hw_fe_1_size(dma, &cfg->size_cfg.in_size,
			plane->type);
		msm_jpegdma_hw_fe_1_addr(dma, addr->in_addr + cfg->in_offset);
		prefetch_in_size += (cfg->size_cfg.in_size.stride *
			cfg->size_cfg.in_size.scanline);

		msm_jpegdma_hw_we_1_block(dma, &cfg->block_cfg, plane->type);
		msm_jpegdma_hw_we_1_size(dma, &cfg->size_cfg.out_size);
		msm_jpegdma_hw_we_1_addr(dma, addr->out_addr + cfg->out_offset);
		prefetch_out_size += (cfg->size_cfg.out_size.stride *
			cfg->size_cfg.out_size.scanline);
	}

	if (prefetch_in_size > 0 && prefetch_out_size > 0) {
		prefetch_max_addr.in_addr = addr->in_addr +
			(prefetch_in_size - 1);
		prefetch_max_addr.out_addr = addr->out_addr +
			(prefetch_out_size - 1);
		msm_jpegdma_hw_config_mmu_prefetch(dma, addr,
			&prefetch_max_addr);
	}

	msm_jpegdma_hw_run(dma);

	return 1;
}

/*
* msm_jpegdma_hw_abort - abort dma processing.
 *@dma: Pointer to dma device.
*/
int msm_jpegdma_hw_abort(struct msm_jpegdma_device *dma)
{
	int ret;

	ret = msm_jpegdma_hw_halt(dma);
	if (ret < 0) {
		dev_err(dma->dev, "Fail to halt hw\n");
		return ret;
	}

	ret = msm_jpegdma_hw_reset(dma);
	if (ret < 0) {
		dev_err(dma->dev, "Fail to reset hw\n");
		return ret;
	}
	return 0;
}

/*
 * msm_jpegdma_hw_irq - Dma irq handler.
 * @irq: Irq number.
 * @dev_id: Pointer to dma device.
 */
static irqreturn_t msm_jpegdma_hw_irq(int irq, void *dev_id)
{
	struct msm_jpegdma_device *dma = dev_id;

	u32 irq_status;

	irq_status = msm_jpegdma_hw_get_irq_status(dma);
	msm_jpegdma_hw_clear_irq(dma, irq_status);

	if (irq_status & MSM_JPEGDMA_IRQ_STATUS_RST_DONE) {
		dev_dbg(dma->dev, "Jpeg v4l2 dma IRQ reset done\n");
		complete_all(&dma->hw_reset_completion);
	}

	if (irq_status & MSM_JPEGDMA_IRQ_STATUS_AXI_HALT) {
		dev_dbg(dma->dev, "Jpeg v4l2 dma IRQ AXI halt\n");
		complete_all(&dma->hw_halt_completion);
	}

	if (irq_status & MSM_JPEGDMA_IRQ_STATUS_SESSION_DONE) {
		dev_dbg(dma->dev, "Jpeg v4l2 dma IRQ session done\n");
		msm_jpegdma_isr_processing_done(dma);
	}

	return IRQ_HANDLED;
}

/*
 * msm_jpegdma_hw_request_irq - Request dma irq.
 * @pdev: Pointer to platform device.
 * @dma: Pointer to dma device.
 */
int msm_jpegdma_hw_request_irq(struct platform_device *pdev,
	struct msm_jpegdma_device *dma)
{
	int ret;

	dma->irq_num = platform_get_irq(pdev, 0);
	if (dma->irq_num < 0) {
		dev_err(dma->dev, "Can not get dma core irq resource\n");
		ret = -ENODEV;
		goto error_irq;
	}

	ret = request_threaded_irq(dma->irq_num, NULL,
		msm_jpegdma_hw_irq, IRQF_ONESHOT | IRQF_TRIGGER_RISING,
		dev_name(&pdev->dev), dma);
	if (ret) {
		dev_err(dma->dev, "Can not claim wrapper IRQ %d\n",
			dma->irq_num);
		goto error_irq;
	}

	return 0;

error_irq:
	return ret;
}

/*
 * msm_jpegdma_hw_release_irq - Free dma irq.
 * @dma: Pointer to dma device.
 */
void msm_jpegdma_hw_release_irq(struct msm_jpegdma_device *dma)
{
	if (dma->irq_num >= 0) {
		free_irq(dma->irq_num, dma);
		dma->irq_num = -1;
	}
}

/*
 * msm_jpegdma_hw_release_mem_resources - Releases memory resources.
 * @dma: Pointer to dma device.
 */
void msm_jpegdma_hw_release_mem_resources(struct msm_jpegdma_device *dma)
{
	int i;

	/* Prepare memory resources */
	for (i = 0; i < MSM_JPEGDMA_IOMEM_LAST; i++) {
		if (dma->iomem_base[i]) {
			iounmap(dma->iomem_base[i]);
			dma->iomem_base[i] = NULL;
		}
		if (dma->ioarea[i]) {
			release_mem_region(dma->res_mem[i]->start,
				resource_size(dma->res_mem[i]));
			dma->ioarea[i] = NULL;
		}
		dma->res_mem[i] = NULL;
	}
}

/*
 * msm_jpegdma_hw_get_mem_resources - Get memory resources.
 * @pdev: Pointer to dma platform device.
 * @dma: Pointer to dma device.
 *
 * Get and ioremap platform memory resources.
 */
int msm_jpegdma_hw_get_mem_resources(struct platform_device *pdev,
	struct msm_jpegdma_device *dma)
{
	int i;
	int ret = 0;

	/* Prepare memory resources */
	for (i = 0; i < MSM_JPEGDMA_IOMEM_LAST; i++) {
		/* Get resources */
		dma->res_mem[i] = platform_get_resource(pdev,
			IORESOURCE_MEM, i);
		if (!dma->res_mem[i]) {
			dev_err(dma->dev, "Fail get resource idx %d\n", i);
			ret = -ENODEV;
			break;
		}

		dma->ioarea[i] = request_mem_region(dma->res_mem[i]->start,
			resource_size(dma->res_mem[i]), dma->res_mem[i]->name);
		if (!dma->ioarea[i]) {
			dev_err(dma->dev, "%s can not request mem\n",
				dma->res_mem[i]->name);
			ret = -ENODEV;
			break;
		}

		dma->iomem_base[i] = ioremap(dma->res_mem[i]->start,
			resource_size(dma->res_mem[i]));
		if (!dma->iomem_base[i]) {
			dev_err(dma->dev, "%s can not remap region\n",
				dma->res_mem[i]->name);
			ret = -ENODEV;
			break;
		}
	}

	if (ret < 0)
		msm_jpegdma_hw_release_mem_resources(dma);

	return ret;
}

/*
 * msm_jpegdma_hw_get_regulators - Get jpeg dma regulators.
 * @dma: Pointer to dma device.
 *
 * Read regulator information from device tree and perform get regulator.
 */
int msm_jpegdma_hw_get_regulators(struct msm_jpegdma_device *dma)
{
	const char *name;
	uint32_t cnt;
	int i;
	int ret;

	if (of_get_property(dma->dev->of_node, "qcom,vdd-names", NULL)) {
		cnt = of_property_count_strings(dma->dev->of_node,
			"qcom,vdd-names");

		if ((cnt == 0) || (cnt == -EINVAL)) {
			dev_err(dma->dev, "no regulators found %d\n", cnt);
			return -EINVAL;
		}

		if (cnt > MSM_JPEGDMA_MAX_REGULATOR_NUM) {
			dev_err(dma->dev, "Exceed max regulators %d\n", cnt);
			return -EINVAL;
		}

		for (i = 0; i < cnt; i++) {
			ret = of_property_read_string_index(dma->dev->of_node,
				"qcom,vdd-names", i, &name);
			if (ret < 0) {
				dev_err(dma->dev, "Fail regulator idx %d\n", i);
				goto regulator_get_error;
			}

			dma->vdd[i] = devm_regulator_get(dma->dev, name);
			if (IS_ERR(dma->vdd[i])) {
				ret = PTR_ERR(dma->vdd[i]);
				dma->vdd[i] = NULL;
				dev_err(dma->dev, "Error regulator get %s\n",
					name);
				goto regulator_get_error;
			}
			dev_dbg(dma->dev, "Regulator %s idx %d\n", name, i);
		}
		dma->regulator_num = cnt;
	} else {
		dma->regulator_num = 1;
		dma->vdd[0] = devm_regulator_get(dma->dev, "vdd");
		if (IS_ERR(dma->vdd[0])) {
			dev_err(dma->dev, "Fail to get vdd regulator\n");
			ret = PTR_ERR(dma->vdd[0]);
			dma->vdd[0] = NULL;
			return ret;
		}
	}
	return 0;

regulator_get_error:
	for (; i > 0; i--) {
		if (!IS_ERR_OR_NULL(dma->vdd[i - 1]))
			devm_regulator_put(dma->vdd[i - 1]);
	}
	return ret;
}

/*
 * msm_jpegdma_hw_put_regulators - Put fd regulators.
 * @dma: Pointer to dma device.
 */
void msm_jpegdma_hw_put_regulators(struct msm_jpegdma_device *dma)
{
	int i;

	for (i = dma->regulator_num - 1; i >= 0; i--) {
		if (!IS_ERR_OR_NULL(dma->vdd[i]))
			devm_regulator_put(dma->vdd[i]);

		dma->vdd[i] = NULL;
	}
}

/*
 * msm_jpegdma_hw_enable_regulators - Prepare and enable fd regulators.
 * @dma: Pointer to dma device.
 */
static int msm_jpegdma_hw_enable_regulators(struct msm_jpegdma_device *dma)
{
	int i;
	int ret;

	for (i = 0; i < dma->regulator_num; i++) {

		ret = regulator_enable(dma->vdd[i]);
		if (ret < 0) {
			dev_err(dma->dev, "regulator enable failed %d\n", i);
			regulator_put(dma->vdd[i]);
			goto error;
		}
	}

	return 0;
error:
	for (; i > 0; i--) {
		if (!IS_ERR_OR_NULL(dma->vdd[i - 1])) {
			regulator_disable(dma->vdd[i - 1]);
			regulator_put(dma->vdd[i - 1]);
		}
	}
	return ret;
}

/*
 * msm_jpegdma_hw_disable_regulators - Disable jpeg dma regulator.
 * @dma: Pointer to dma device.
 */
static void msm_jpegdma_hw_disable_regulators(struct msm_jpegdma_device *dma)
{
	int i;

	for (i = dma->regulator_num - 1; i >= 0; i--) {
		if (!IS_ERR_OR_NULL(dma->vdd[i]))
			regulator_disable(dma->vdd[i]);
	}
}

/*
 * msm_jpegdma_hw_get_clocks - Get dma clocks.
 * @dma: Pointer to dma device.
 *
 * Read clock information from device tree and perform get clock.
 */
int msm_jpegdma_hw_get_clocks(struct msm_jpegdma_device *dma)
{
	const char *clk_name;
	size_t cnt;
	int i;
	int ret;

	cnt = of_property_count_strings(dma->dev->of_node, "clock-names");
	if (cnt > MSM_JPEGDMA_MAX_CLK) {
		dev_err(dma->dev, "Exceed max number of clocks %zu\n", cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(dma->dev->of_node,
			"clock-names", i, &clk_name);
		if (ret < 0) {
			dev_err(dma->dev, "Can not read clock name %d\n", i);
			goto error;
		}

		dma->clk[i] = clk_get(dma->dev, clk_name);
		if (IS_ERR(dma->clk[i])) {
			ret = -ENOENT;
			dev_err(dma->dev, "Error clock get %s\n", clk_name);
			goto error;
		}
		dev_dbg(dma->dev, "Clock name idx %d %s\n", i, clk_name);

		ret = of_property_read_u32_index(dma->dev->of_node,
			"qcom,clock-rates", i, &dma->clk_rates[i]);
		if (ret < 0) {
			dev_err(dma->dev, "Get clock rate fail %s\n", clk_name);
			goto error;
		}
		dev_dbg(dma->dev, "Clock rate idx %d value %d\n", i,
			dma->clk_rates[i]);
	}
	dma->clk_num = cnt;

	return 0;
error:
	for (; i > 0; i--)
		clk_put(dma->clk[i - 1]);

	return ret;
}

/*
 * msm_jpegdma_hw_put_clocks - Put dma clocks.
 * @dma: Pointer to dma device.
 */
int msm_jpegdma_hw_put_clocks(struct msm_jpegdma_device *dma)
{
	int i;

	for (i = 0; i < dma->clk_num; i++) {
		if (!IS_ERR_OR_NULL(dma->clk[i]))
			clk_put(dma->clk[i]);
		dma->clk_num = 0;
	}
	return 0;
}

/*
 * msm_jpegdma_hw_get_qos - Get dma qos settings from device-tree.
 * @dma: Pointer to dma device.
 */
int msm_jpegdma_hw_get_qos(struct msm_jpegdma_device *dma)
{
	int i;
	int ret;
	unsigned int cnt;
	const void *property;

	property = of_get_property(dma->dev->of_node, "qcom,qos-regs", &cnt);
	if (!property || !cnt) {
		dev_dbg(dma->dev, "Missing qos settings\n");
		return 0;
	}
	cnt /= 4;

	dma->qos_regs = kzalloc((sizeof(*dma->qos_regs) * cnt), GFP_KERNEL);
	if (!dma->qos_regs)
		return -ENOMEM;

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_u32_index(dma->dev->of_node,
			"qcom,qos-regs", i,
			&dma->qos_regs[i].reg);
		if (ret < 0) {
			dev_err(dma->dev, "can not read qos reg %d\n", i);
			goto error;
		}

		ret = of_property_read_u32_index(dma->dev->of_node,
			"qcom,qos-settings", i,
			&dma->qos_regs[i].val);
		if (ret < 0) {
			dev_err(dma->dev, "can not read qos setting %d\n", i);
			goto error;
		}
		dev_dbg(dma->dev, "Qos idx %d, reg %x val %x\n", i,
			dma->qos_regs[i].reg, dma->qos_regs[i].val);
	}
	dma->qos_regs_num = cnt;

	return 0;
error:
	kfree(dma->qos_regs);
	dma->qos_regs = NULL;

	return ret;
}

/*
 * msm_jpegdma_hw_put_qos - Free dma qos settings.
 * @dma: Pointer to dma device.
 */
void msm_jpegdma_hw_put_qos(struct msm_jpegdma_device *dma)
{
	kfree(dma->qos_regs);
	dma->qos_regs = NULL;
}

/*
 * msm_jpegdma_hw_get_vbif - Get dma vbif settings from device-tree.
 * @dma: Pointer to dma device.
 */
int msm_jpegdma_hw_get_vbif(struct msm_jpegdma_device *dma)
{
	int i;
	int ret;
	unsigned int cnt;
	const void *property;

	property = of_get_property(dma->dev->of_node, "qcom,vbif-regs", &cnt);
	if (!property || !cnt) {
		dev_dbg(dma->dev, "Missing vbif settings\n");
		return 0;
	}
	cnt /= 4;

	dma->vbif_regs = kzalloc((sizeof(*dma->vbif_regs) * cnt), GFP_KERNEL);
	if (!dma->vbif_regs)
		return -ENOMEM;

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_u32_index(dma->dev->of_node,
			"qcom,vbif-regs", i,
			&dma->vbif_regs[i].reg);
		if (ret < 0) {
			dev_err(dma->dev, "can not read vbif reg %d\n", i);
			goto error;
		}

		ret = of_property_read_u32_index(dma->dev->of_node,
			"qcom,vbif-settings", i,
			&dma->vbif_regs[i].val);
		if (ret < 0) {
			dev_err(dma->dev, "can not read vbif setting %d\n", i);
			goto error;
		}

		dev_dbg(dma->dev, "Vbif idx %d, reg %x val %x\n", i,
			dma->vbif_regs[i].reg, dma->vbif_regs[i].val);
	}
	dma->vbif_regs_num = cnt;

	return 0;
error:
	kfree(dma->vbif_regs);
	dma->vbif_regs = NULL;

	return ret;
}

/*
 * msm_jpegdma_hw_put_vbif - Put dma clocks.
 * @dma: Pointer to dma device.
 */
void msm_jpegdma_hw_put_vbif(struct msm_jpegdma_device *dma)
{
	kfree(dma->vbif_regs);
	dma->vbif_regs = NULL;
}

/*
 * msm_jpegdma_hw_get_prefetch - Get dma prefetch settings from device-tree.
 * @dma: Pointer to dma device.
 */
int msm_jpegdma_hw_get_prefetch(struct msm_jpegdma_device *dma)
{
	int i;
	int ret;
	unsigned int cnt;
	const void *property;

	property = of_get_property(dma->dev->of_node, "qcom,prefetch-regs",
		&cnt);
	if (!property || !cnt) {
		dev_dbg(dma->dev, "Missing prefetch settings\n");
		return 0;
	}
	cnt /= 4;

	dma->prefetch_regs = kcalloc(cnt, sizeof(*dma->prefetch_regs),
		GFP_KERNEL);
	if (!dma->prefetch_regs)
		return -ENOMEM;

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_u32_index(dma->dev->of_node,
			"qcom,prefetch-regs", i,
			&dma->prefetch_regs[i].reg);
		if (ret < 0) {
			dev_err(dma->dev, "can not read prefetch reg %d\n", i);
			goto error;
		}

		ret = of_property_read_u32_index(dma->dev->of_node,
			"qcom,prefetch-settings", i,
			&dma->prefetch_regs[i].val);
		if (ret < 0) {
			dev_err(dma->dev, "can not read prefetch setting %d\n",
				i);
			goto error;
		}

		dev_dbg(dma->dev, "Prefetch idx %d, reg %x val %x\n", i,
			dma->prefetch_regs[i].reg, dma->prefetch_regs[i].val);
	}
	dma->prefetch_regs_num = cnt;

	return 0;
error:
	kfree(dma->prefetch_regs);
	dma->prefetch_regs = NULL;

	return ret;
}

/*
 * msm_jpegdma_hw_put_prefetch - free prefetch settings.
 * @dma: Pointer to dma device.
 */
void msm_jpegdma_hw_put_prefetch(struct msm_jpegdma_device *dma)
{
	kfree(dma->prefetch_regs);
	dma->prefetch_regs = NULL;
}

/*
 * msm_jpegdma_hw_set_clock_rate - Set clock rates described in device tree.
 * @dma: Pointer to dma device.
 */
static int msm_jpegdma_hw_set_clock_rate(struct msm_jpegdma_device *dma)
{
	int ret;
	long clk_rate;
	int i;

	for (i = 0; i < dma->clk_num; i++) {

		clk_rate = clk_round_rate(dma->clk[i], dma->clk_rates[i]);
		if (clk_rate < 0) {
			dev_dbg(dma->dev, "Clk round rate fail skip %d\n", i);
			continue;
		}

		ret = clk_set_rate(dma->clk[i], clk_rate);
		if (ret < 0) {
			dev_err(dma->dev, "Fail clock rate %ld\n", clk_rate);
			return -EINVAL;
		}
		dev_dbg(dma->dev, "Clk rate %d-%ld\n", i, clk_rate);
	}

	return 0;
}

/*
 * msm_jpegdma_hw_enable_clocks - Prepare and enable dma clocks.
 * @dma: Pointer to dma device.
 */
static int msm_jpegdma_hw_enable_clocks(struct msm_jpegdma_device *dma)
{
	int i;
	int ret;

	for (i = 0; i < dma->clk_num; i++) {
		ret = clk_prepare(dma->clk[i]);
		if (ret < 0) {
			dev_err(dma->dev, "clock prepare failed %d\n", i);
			goto error;
		}

		ret = clk_enable(dma->clk[i]);
		if (ret < 0) {
			dev_err(dma->dev, "clock enable %d\n", i);
			clk_unprepare(dma->clk[i]);
			goto error;
		}
	}

	return 0;
error:
	for (; i > 0; i--) {
		clk_disable(dma->clk[i - 1]);
		clk_unprepare(dma->clk[i - 1]);
	}
	return ret;
}
/*
 * msm_jpegdma_hw_disable_clocks - Disable dma clock.
 * @dma: Pointer to dma device.
 */
static void msm_jpegdma_hw_disable_clocks(struct msm_jpegdma_device *dma)
{
	int i;

	for (i = 0; i < dma->clk_num; i++) {
		clk_disable(dma->clk[i]);
		clk_unprepare(dma->clk[i]);
	}
}

/*
 * msm_jpegdma_hw_bus_request - Request bus for memory access.
 * @dma: Pointer to dma device.
 * @clk_idx: Clock rate index.
 */
static int msm_jpegdma_hw_bus_request(struct msm_jpegdma_device *dma)
{
	int ret;

	dma->bus_vectors.src = MSM_BUS_MASTER_JPEG;
	dma->bus_vectors.dst = MSM_BUS_SLAVE_EBI_CH0;
	dma->bus_vectors.ab = dma->clk_rates[MSM_JPEGDMA_CORE_CLK] * 2;
	dma->bus_vectors.ib = dma->clk_rates[MSM_JPEGDMA_CORE_CLK] * 2;

	dma->bus_paths.num_paths = 1;
	dma->bus_paths.vectors = &dma->bus_vectors;

	dma->bus_scale_data.usecase = &dma->bus_paths;
	dma->bus_scale_data.num_usecases = 1;
	dma->bus_scale_data.name = MSM_JPEGDMA_BUS_CLIENT_NAME;

	dma->bus_client = msm_bus_scale_register_client(&dma->bus_scale_data);
	if (!dma->bus_client) {
		dev_err(dma->dev, "Fail to register bus client\n");
		return -ENOENT;
	}

	ret = msm_bus_scale_client_update_request(dma->bus_client, 0);
	if (ret < 0) {
		dev_err(dma->dev, "Fail bus scale update %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * msm_jpegdma_hw_bus_release - Release memory access bus.
 * @dma: Pointer to dma device.
 */
static void msm_jpegdma_hw_bus_release(struct msm_jpegdma_device *dma)
{
	if (dma->bus_client) {
		msm_bus_scale_unregister_client(dma->bus_client);
		dma->bus_client = 0;
	}
}

/*
 * msm_jpegdma_hw_update_bus_data - Update bus data request
 * @dma: Pointer to dma device.
 * @clk_idx: Clock rate index.
 */
int msm_jpegdma_hw_update_bus_data(struct msm_jpegdma_device *dma,
	u64 ab, u64 ib)
{
	int ret;

	dma->bus_vectors.ab = ab;
	dma->bus_vectors.ib = ib;

	ret = msm_bus_scale_client_update_request(dma->bus_client, 0);
	if (ret < 0) {
		dev_err(dma->dev, "Fail bus scale update %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * msm_jpegdma_hw_get_capabilities - Get dma hw for performing any hw operation.
 * @dma: Pointer to dma device.
 */
int msm_jpegdma_hw_get_capabilities(struct msm_jpegdma_device *dma)
{
	int ret = 0;

	mutex_lock(&dma->lock);

	ret = msm_jpegdma_hw_enable_regulators(dma);
	if (ret < 0) {
		dev_err(dma->dev, "Fail to enable regulators\n");
		goto error_regulators_get;
	}

	ret = msm_jpegdma_hw_set_clock_rate(dma);
	if (ret < 0) {
		dev_err(dma->dev, "Fail to set clock rate\n");
		goto error_fail_clock;
	}

	ret = msm_jpegdma_hw_enable_clocks(dma);
	if (ret < 0) {
		dev_err(dma->dev, "Fail to enable clocks\n");
		goto error_fail_clock;
	}
	dma->hw_num_pipes = msm_jpegdma_hw_get_num_pipes(dma);

	msm_jpegdma_hw_disable_clocks(dma);
	msm_jpegdma_hw_disable_regulators(dma);

	mutex_unlock(&dma->lock);

	return 0;

error_fail_clock:
	msm_jpegdma_hw_disable_regulators(dma);
error_regulators_get:
	mutex_unlock(&dma->lock);
	return ret;
}

/*
 * msm_jpegdma_hw_get - Get dma hw for performing any hw operation.
 * @dma: Pointer to dma device.
 * @clock_rate_idx: Clock rate index.
 *
 * Prepare dma hw for operation. Have reference count protected by
 * dma device mutex.
 */
int msm_jpegdma_hw_get(struct msm_jpegdma_device *dma)
{
	int ret;

	mutex_lock(&dma->lock);
	if (dma->ref_count == 0) {

		dev_dbg(dma->dev, "msm_jpegdma_hw_get E\n");
		ret = msm_jpegdma_hw_set_clock_rate(dma);
		if (ret < 0) {
			dev_err(dma->dev, "Fail to set clock rates\n");
			goto error;
		}

		ret = msm_jpegdma_hw_enable_regulators(dma);
		if (ret < 0) {
			dev_err(dma->dev, "Fail to enable regulators\n");
			goto error;
		}

		ret = msm_jpegdma_hw_enable_clocks(dma);
		if (ret < 0) {
			dev_err(dma->dev, "Fail to enable clocks\n");
			goto error_clocks;
		}

		ret = msm_jpegdma_hw_bus_request(dma);
		if (ret < 0) {
			dev_err(dma->dev, "Fail bus request\n");
			goto error_bus_request;
		}
		msm_jpegdma_hw_config_qos(dma);
		msm_jpegdma_hw_config_vbif(dma);

		msm_jpegdma_hw_enable_irq(dma);

		ret = msm_jpegdma_hw_reset(dma);
		if (ret < 0) {
			dev_err(dma->dev, "Fail to reset hw\n");
			goto error_hw_reset;
		}
		msm_jpegdma_hw_config_qos(dma);
		msm_jpegdma_hw_config_mmu_prefetch(dma, NULL, NULL);
		msm_jpegdma_hw_enable_irq(dma);
	}
	dma->ref_count++;
	dev_dbg(dma->dev, "msm_jpegdma_hw_get X\n");
	mutex_unlock(&dma->lock);

	return 0;

error_hw_reset:
	msm_jpegdma_hw_disable_irq(dma);
error_bus_request:
	msm_jpegdma_hw_disable_clocks(dma);
error_clocks:
	msm_jpegdma_hw_disable_regulators(dma);
error:
	mutex_unlock(&dma->lock);
	return ret;
}

/*
 * msm_jpegdma_hw_put - Put dma hw.
 * @dma: Pointer to dma device.
 *
 * Release dma hw. Have reference count protected by
 * dma device mutex.
 */
void msm_jpegdma_hw_put(struct msm_jpegdma_device *dma)
{
	mutex_lock(&dma->lock);
	BUG_ON(dma->ref_count == 0);

	if (--dma->ref_count == 0) {
		msm_jpegdma_hw_halt(dma);
		msm_jpegdma_hw_disable_irq(dma);
		msm_jpegdma_hw_bus_release(dma);
		msm_jpegdma_hw_disable_clocks(dma);
		msm_jpegdma_hw_disable_regulators(dma);
	}
	/* Reset clock rate, need to be updated on next processing */
	dma->active_clock_rate = -1;
	mutex_unlock(&dma->lock);
}

/*
 * msm_jpegdma_hw_attach_iommu - Attach iommu to jpeg dma engine.
 * @dma: Pointer to dma device.
 *
 * Iommu attach have reference count protected by
 * dma device mutex.
 */
static int msm_jpegdma_hw_attach_iommu(struct msm_jpegdma_device *dma)
{
	int ret;

	mutex_lock(&dma->lock);

	if (dma->iommu_attached_cnt == UINT_MAX) {
		dev_err(dma->dev, "Max count reached! can not attach iommu\n");
		goto error;
	}

	if (dma->iommu_attached_cnt == 0) {
		ret = cam_smmu_get_handle(MSM_JPEGDMA_SMMU_NAME,
			&dma->iommu_hndl);
		if (ret < 0) {
			dev_err(dma->dev, "Smmu get handle failed\n");
			ret = -ENOMEM;
			goto error;
		}
		ret = cam_smmu_ops(dma->iommu_hndl, CAM_SMMU_ATTACH);
		if (ret < 0) {
			dev_err(dma->dev, "Can not attach smmu.\n");
			goto error_attach;
		}
	}
	dma->iommu_attached_cnt++;
	mutex_unlock(&dma->lock);

	return 0;
error_attach:
	cam_smmu_destroy_handle(dma->iommu_hndl);
error:
	mutex_unlock(&dma->lock);
	return ret;
}

/*
 * msm_jpegdma_hw_detach_iommu - Detach iommu from jpeg dma engine.
 * @dma: Pointer to dma device.
 *
 * Iommu detach have reference count protected by
 * dma device mutex.
 */
static void msm_jpegdma_hw_detach_iommu(struct msm_jpegdma_device *dma)
{
	mutex_lock(&dma->lock);
	if (dma->iommu_attached_cnt == 0) {
		dev_err(dma->dev, "There is no attached device\n");
		mutex_unlock(&dma->lock);
		return;
	}

	if (--dma->iommu_attached_cnt == 0) {
		cam_smmu_ops(dma->iommu_hndl, CAM_SMMU_DETACH);
		cam_smmu_destroy_handle(dma->iommu_hndl);
	}
	mutex_unlock(&dma->lock);
}

/*
 * msm_jpegdma_hw_map_buffer - Map buffer to dma hw mmu.
 * @dma: Pointer to dma device.
 * @fd: Ion fd.
 * @buf: dma buffer handle, for storing mapped buffer information.
 *
 * It will map ion fd to dma hw smmu.
 */
int msm_jpegdma_hw_map_buffer(struct msm_jpegdma_device *dma, int fd,
	struct msm_jpegdma_buf_handle *buf)
{
	int ret;

	if (!dma || fd < 0)
		return -EINVAL;

	ret = msm_jpegdma_hw_attach_iommu(dma);
	if (ret < 0)
		goto error;

	buf->dma = dma;
	buf->fd = fd;

	ret = cam_smmu_get_phy_addr(dma->iommu_hndl, buf->fd,
		CAM_SMMU_MAP_RW, &buf->addr, &buf->size);
	if (ret < 0) {
		dev_err(dma->dev, "Can not get physical address\n");
		goto error_get_phy;
	}

	return buf->size;

error_get_phy:
	msm_jpegdma_hw_detach_iommu(dma);
error:
	return -ENOMEM;
}

/*
 * msm_jpegdma_hw_unmap_buffer - Unmap buffer from dma hw mmu.
 * @buf: dma buffer handle, for storing mapped buffer information.
 */
void msm_jpegdma_hw_unmap_buffer(struct msm_jpegdma_buf_handle *buf)
{
	if (buf->size && buf->dma) {
		cam_smmu_put_phy_addr(buf->dma->iommu_hndl,
			buf->fd);
		msm_jpegdma_hw_detach_iommu(buf->dma);
		buf->size = 0;
	}
	buf->fd = -1;
	buf->dma = NULL;
}
