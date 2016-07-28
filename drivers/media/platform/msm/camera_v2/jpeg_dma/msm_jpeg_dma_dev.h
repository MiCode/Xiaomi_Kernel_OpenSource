/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_JPEG_DMA_DEV_H__
#define __MSM_JPEG_DMA_DEV_H__

#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ctrls.h>
#include <linux/msm-bus.h>
#include "cam_soc_api.h"

/* Max number of clocks defined in device tree */
#define MSM_JPEGDMA_MAX_CLK 10
/* Core clock index */
#define MSM_JPEGDMA_CORE_CLK "core_clk"
/* Max number of regulators defined in device tree */
#define MSM_JPEGDMA_MAX_REGULATOR_NUM 3
/* Max number of planes supported */
#define MSM_JPEGDMA_MAX_PLANES 3
/* Max number of hw pipes supported */
#define MSM_JPEGDMA_MAX_PIPES 2
/* Max number of hw configurations supported */
#define MSM_JPEGDMA_MAX_CONFIGS 2
/* Dma default fps */
#define MSM_JPEGDMA_DEFAULT_FPS 30

/* Dma input output size limitations */
#define MSM_JPEGDMA_MAX_WIDTH 65536
#define MSM_JPEGDMA_MIN_WIDTH 8
#define MSM_JPEGDMA_MAX_HEIGHT 65536
#define MSM_JPEGDMA_MIN_HEIGHT 8
#define MSM_JPEGDMA_STRIDE_ALIGN 8

/*
 * enum msm_jpegdma_plane_type - Dma format.
 * @JPEGDMA_PLANE_TYPE_Y: Y plane type.
 * @JPEGDMA_PLANE_TYPE_CR: Chroma CB plane.
 * @JPEGDMA_PLANE_TYPE_CB:  Chroma CR plane.
 * @JPEGDMA_PLANE_TYPE_CBCR: Interlevaed CbCr plane.
 */
enum msm_jpegdma_plane_type {
	JPEGDMA_PLANE_TYPE_Y,
	JPEGDMA_PLANE_TYPE_CR,
	JPEGDMA_PLANE_TYPE_CB,
	JPEGDMA_PLANE_TYPE_CBCR,
};

/*
 * struct msm_jpegdma_format - Dma format.
 * @name: Format name.
 * @fourcc: v4l2 fourcc code.
 * @depth: Number of bits per pixel.
 * @num_planes: number of planes.
 * @colplane_h: Color plane horizontal subsample.
 * @colplane_v: Color plane vertical subsample.
 * @h_align: Horizontal align.
 * @v_align: Vertical align.
 * @planes: Array with plane types.
 */
struct msm_jpegdma_format {
	char *name;
	u32 fourcc;
	int depth;
	int num_planes;
	int colplane_h;
	int colplane_v;
	int h_align;
	int v_align;
	enum msm_jpegdma_plane_type planes[MSM_JPEGDMA_MAX_PLANES];
};

/*
 * struct msm_jpegdma_size - Dma size.
 * @top: Top position.
 * @left: Left position
 * @width: Width
 * @height: height.
 * @scanline: Number of lines per plane.
 * @stride: Stride bytes per line.
 */
struct msm_jpegdma_size {
	unsigned int top;
	unsigned int left;
	unsigned int width;
	unsigned int height;
	unsigned int scanline;
	unsigned int stride;
};

/*
 * struct msm_jpegdma_size_config - Dma engine size configuration.
 * @in_size: Input size.
 * @out_size: Output size.
 * @format: Format.
 * @fps: Requested frames per second.
 */
struct msm_jpegdma_size_config {
	struct msm_jpegdma_size in_size;
	struct msm_jpegdma_size out_size;
	struct msm_jpegdma_format format;
	unsigned int fps;
	unsigned int in_offset;
	unsigned int out_offset;
};

/*
 * struct msm_jpegdma_block - Dma hw block.
 * @div: Block divider.
 * @width: Block width.
 * @reg_val: Block register value.
 */
struct msm_jpegdma_block {
	unsigned int div;
	unsigned int width;
	unsigned int reg_val;
};

/*
 * struct msm_jpegdma_block_config - Dma hw block configuration.
 * @block: Block settings.
 * @blocks_per_row: Blocks per row.
 * @blocks_per_col: Blocks per column.
 * @h_step: Horizontal step value
 * @v_step: Vertical step value
 * @h_step_last: Last horizontal step.
 * @v_step_last: Last vertical step.
 */
struct msm_jpegdma_block_config {
	struct msm_jpegdma_block block;
	unsigned int blocks_per_row;
	unsigned int blocks_per_col;
	unsigned int h_step;
	unsigned int v_step;
	unsigned int h_step_last;
	unsigned int v_step_last;
};

/*
 * msm_jpegdma_scale - Dma hw scale configuration.
 * @enable: Scale enable.
 * @hor_scale: Horizontal scale factor in Q21 format.
 * @ver_scale: Vertical scale factor in Q21 format.
 */
struct msm_jpegdma_scale {
	int enable;
	unsigned int hor_scale;
	unsigned int ver_scale;
};

/*
 * struct msm_jpegdma_config - Dma hw configuration.
 * @size_cfg: Size configuration.
 * @scale_cfg: Scale configuration
 * @block_cfg: Block configuration.
 * @phase: Starting phase.
 * @in_offset: Input offset.
 * @out_offset: Output offset.
 */
struct msm_jpegdma_config {
	struct msm_jpegdma_size_config size_cfg;
	struct msm_jpegdma_scale scale_cfg;
	struct msm_jpegdma_block_config block_cfg;
	unsigned int phase;
	unsigned int in_offset;
	unsigned int out_offset;
};

/*
 * struct msm_jpegdma_plane_config - Contain input output address.
 * @bus_ab: Bus average bandwidth.
 * @bus_ib: Bus instantaneous bandwidth.
 * @core_clock: Core clock freq.
 */
struct msm_jpegdma_speed {
	u64 bus_ab;
	u64 bus_ib;
	u64 core_clock;
};

/*
 * struct msm_jpegdma_plane_config - Contain input output address.
 * @active_pipes: Number of active pipes.
 * @config: Plane configurations.
 * @type: Plane type.
 */
struct msm_jpegdma_plane {
	unsigned int active_pipes;
	struct msm_jpegdma_config config[MSM_JPEGDMA_MAX_PIPES];
	enum msm_jpegdma_plane_type type;
};

/*
 * struct msm_jpegdma_plane_config - Contain input output address.
 * @num_planes: Number of planes.
 * @plane: Plane configuration.
 * @speed: Processing speed.
 */
struct msm_jpegdma_plane_config {
	unsigned int num_planes;
	struct msm_jpegdma_plane plane[MSM_JPEGDMA_MAX_PLANES];
	struct msm_jpegdma_speed speed;
};

/*
 * struct msm_jpegdma_addr - Contain input output address.
 * @in_addr: Input dma address.
 * @out_addr: Output dma address.
 */
struct msm_jpegdma_addr {
	u32 in_addr;
	u32 out_addr;
};

/*
 * struct msm_jpegdma_buf_handle - Structure contain dma buffer information.
 * @fd: ion dma from which this buffer is imported.
 * @dma: Pointer to jpeg dma device.
 * @size: Size of the buffer.
 * @addr: Adders of dma mmu mapped buffer. This address should be set to dma hw.
 */
struct msm_jpegdma_buf_handle {
	int fd;
	struct msm_jpegdma_device *dma;
	unsigned long size;
	ion_phys_addr_t addr;
};

/*
 * @jpegdma_ctx - Structure contains per open file handle context.
 * @lock: Lock protecting dma ctx.
 * @jdma_device: Pointer to dma device.
 * @active: Set if context is active.
 * @completion: Context processing completion.
 * @fh: V4l2 file handle.
 * @m2m_ctx: Memory to memory context.
 * @format_cap: Current capture format.
 * @format_out: Current output format.
 * @crop: Current crop.
 * @timeperframe: Time per frame in seconds.
 * @config_idx: Plane configuration active index.
 * @plane_config: Array of plane configurations.
 * @pending_config: Flag set if there is pending plane configuration.
 * @plane_idx: Processing plane index.
 * @format_idx: Current format index.
 */
struct jpegdma_ctx {
	struct msm_jpegdma_device *jdma_device;
	atomic_t active;
	struct completion completion;
	struct v4l2_fh fh;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct v4l2_format format_cap;
	struct v4l2_format format_out;
	struct v4l2_rect crop;
	struct v4l2_fract timeperframe;
	unsigned int in_offset;
	unsigned int out_offset;

	unsigned int config_idx;
	struct msm_jpegdma_plane_config plane_config[MSM_JPEGDMA_MAX_CONFIGS];
	unsigned int pending_config;

	unsigned int plane_idx;
	unsigned int format_idx;
};

/*
 * struct jpegdma_reg_cfg - Registry values configuration
 * @reg: Register offset.
 * @val: Register value.
 */
struct jpegdma_reg_cfg {
	unsigned int reg;
	unsigned int val;
};

/*
 * enum msm_jpegdma_mem_resources - jpegdma device iomem resources.
 * @MSM_JPEGDMA_IOMEM_CORE: Index of jpegdma core registers.
 * @MSM_JPEGDMA_IOMEM_VBIF: Index of jpegdma vbif registers.
 * @MSM_JPEGDMA_IOMEM_LAST: Not valid.
 */
enum msm_jpegdma_mem_resources {
	MSM_JPEGDMA_IOMEM_CORE,
	MSM_JPEGDMA_IOMEM_VBIF,
	MSM_JPEGDMA_IOMEM_LAST
};

/*
 * struct msm_jpegdma_device - FD device structure.
 * @lock: Lock protecting dma device.
 * @ref_count: Device reference count.
 * @irq_num: Face detection irq number.
 * @res_mem: Array of memory resources used by Dma device.
 * @iomem_base: Array of register mappings used by Dma device.
 * @ioarea: Array of register ioarea used by Dma device.
 * @vdd: Pointer to vdd regulator.
 * @regulator_num: Number of regulators attached to the device.
 * @clk_num: Number of clocks attached to the device.
 * @clk: Array of clock resources used by dma device.
 * @clk_rates: Array of clock rates.
 * @vbif_regs_num: number of vbif  regs.
 * @vbif_regs: Array of vbif regs need to be set.
 * @qos_regs_num: Number of qos regs .
 * @qos_regs: Array of qos regs need to be set.
 * @bus_client: Memory access bus client.
 * @bus_vectors: Bus vector
 * @bus_paths: Bus path.
 * @bus_scale_data: Memory access bus scale data.
 * @iommu_hndl: Dma device iommu handle.
 * @iommu_attached_cnt: Iommu attached devices reference count.
 * @iommu_dev: Pointer to Ion iommu device.
 * @dev: Pointer to device struct.
 * @v4l2_dev: V4l2 device.
 * @video: Video device.
 * @m2m_dev: Memory to memory device.
 * @hw_num_pipes: Number of dma hw pipes.
 * @active_clock_rate: Active clock rate index.
 * @hw_reset_completion: Dma reset completion.
 * @hw_halt_completion: Dma halt completion.
 */
struct msm_jpegdma_device {
	struct mutex lock;
	int ref_count;

	int irq_num;
	void __iomem *iomem_base[MSM_JPEGDMA_IOMEM_LAST];

	struct resource *irq;
	struct msm_cam_regulator *dma_vdd;
	int num_reg;

	struct clk **clk;
	size_t num_clk;
	struct msm_cam_clk_info *jpeg_clk_info;

	unsigned int vbif_regs_num;
	struct jpegdma_reg_cfg *vbif_regs;
	unsigned int qos_regs_num;
	struct jpegdma_reg_cfg *qos_regs;
	unsigned int prefetch_regs_num;
	struct jpegdma_reg_cfg *prefetch_regs;

	enum cam_bus_client bus_client;
	struct msm_bus_vectors bus_vectors;
	struct msm_bus_paths bus_paths;
	struct msm_bus_scale_pdata bus_scale_data;

	int iommu_hndl;
	unsigned int iommu_attached_cnt;

	struct device *iommu_dev;
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct video_device video;
	struct v4l2_m2m_dev *m2m_dev;

	int hw_num_pipes;
	struct completion hw_reset_completion;
	struct completion hw_halt_completion;
	u64 active_clock_rate;
	struct platform_device *pdev;
};

void msm_jpegdma_isr_processing_done(struct msm_jpegdma_device *dma);

#endif /* __MSM_JPEG_DMA_DEV_H__ */
