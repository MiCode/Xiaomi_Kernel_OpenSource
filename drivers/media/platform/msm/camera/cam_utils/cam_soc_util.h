/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_SOC_UTIL_H_
#define _CAM_SOC_UTIL_H_

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include "cam_io_util.h"

#define NO_SET_RATE  -1
#define INIT_RATE    -2

/* maximum number of device block */
#define CAM_SOC_MAX_BLOCK           4

/* maximum number of device base */
#define CAM_SOC_MAX_BASE            CAM_SOC_MAX_BLOCK

/* maximum number of device regulator */
#define CAM_SOC_MAX_REGULATOR       4

/* maximum number of device clock */
#define CAM_SOC_MAX_CLK             32

/**
 * struct cam_soc_reg_map:   Information about the mapped register space
 *
 * @mem_base:               Starting location of MAPPED register space
 * @mem_cam_base:           Starting offset of this register space compared
 *                          to ENTIRE Camera register space
 * @size:                   Size of register space
 **/
struct cam_soc_reg_map {
	void __iomem                   *mem_base;
	uint32_t                        mem_cam_base;
	resource_size_t                 size;
};

/**
 * struct cam_hw_soc_info:  Soc information pertaining to specific instance of
 *                          Camera hardware driver module
 *
 * @pdev:                   Platform device pointer
 * @hw_version;             Camera device version
 * @index:                  Instance id for the camera device
 * @irq_name:               Name of the irq associated with the device
 * @irq_line:               Irq resource
 * @num_mem_block:          Number of entry in the "reg-names"
 * @mem_block_name:         Array of the reg block name
 * @mem_block_cam_base:     Array of offset of this register space compared
 *                          to ENTIRE Camera register space
 * @mem_block:              Associated resource structs
 * @reg_map:                Array of Mapped register info for the "reg-names"
 * @num_reg_map:            Number of mapped register space associated
 *                          with mem_block. num_reg_map = num_mem_block in
 *                          most cases
 * @num_rgltr:              Number of regulators
 * @rgltr_name:             Array of regulator names
 * @rgltr:                  Array of associated regulator resources
 * @num_clk:                Number of clocks
 * @clk_name:               Array of clock names
 * @clk:                    Array of associated clock resources
 * @clk_rate:               Array of default clock rates
 * @src_clk_idx:            Source clock index that is rate-controllable
 * @soc_private;            Soc private data
 *
 */
struct cam_hw_soc_info {
	struct platform_device         *pdev;
	uint32_t                        hw_version;
	uint32_t                        index;

	const char                     *irq_name;
	struct resource                *irq_line;

	uint32_t                        num_mem_block;
	const char                     *mem_block_name[CAM_SOC_MAX_BLOCK];
	uint32_t                        mem_block_cam_base[CAM_SOC_MAX_BLOCK];
	struct resource                *mem_block[CAM_SOC_MAX_BLOCK];
	struct cam_soc_reg_map          reg_map[CAM_SOC_MAX_BASE];
	uint32_t                        num_reg_map;

	uint32_t                        num_rgltr;
	const char                     *rgltr_name[CAM_SOC_MAX_REGULATOR];
	struct regulator               *rgltr[CAM_SOC_MAX_REGULATOR];

	uint32_t                        num_clk;
	const char                     *clk_name[CAM_SOC_MAX_CLK];
	struct clk                     *clk[CAM_SOC_MAX_CLK];
	int32_t                         clk_rate[CAM_SOC_MAX_CLK];
	int32_t                         src_clk_idx;

	void                           *soc_private;
};

/*
 * CAM_SOC_GET_REG_MAP_START
 *
 * @brief:              This MACRO will get the mapped starting address
 *                      where the register space can be accessed
 *
 * @__soc_info:         Device soc information
 * @__base_index:       Index of register space in the HW block
 *
 * @return:             Returns a pointer to the mapped register memory
 */
#define CAM_SOC_GET_REG_MAP_START(__soc_info, __base_index)          \
	((!__soc_info || __base_index >= __soc_info->num_reg_map) ?  \
		NULL : __soc_info->reg_map[__base_index].mem_base)

/*
 * CAM_SOC_GET_REG_MAP_CAM_BASE
 *
 * @brief:              This MACRO will get the cam_base of the
 *                      register space
 *
 * @__soc_info:         Device soc information
 * @__base_index:       Index of register space in the HW block
 *
 * @return:             Returns an int32_t value.
 *                        Failure: -1
 *                        Success: Starting offset of register space compared
 *                                 to entire Camera Register Map
 */
#define CAM_SOC_GET_REG_MAP_CAM_BASE(__soc_info, __base_index)       \
	((!__soc_info || __base_index >= __soc_info->num_reg_map) ?  \
		-1 : __soc_info->reg_map[__base_index].mem_cam_base)

/*
 * CAM_SOC_GET_REG_MAP_SIZE
 *
 * @brief:              This MACRO will get the size of the mapped
 *                      register space
 *
 * @__soc_info:         Device soc information
 * @__base_index:       Index of register space in the HW block
 *
 * @return:             Returns a uint32_t value.
 *                        Failure: 0
 *                        Success: Non-zero size of mapped register space
 */
#define CAM_SOC_GET_REG_MAP_SIZE(__soc_info, __base_index)           \
	((!__soc_info || __base_index >= __soc_info->num_reg_map) ?  \
		0 : __soc_info->reg_map[__base_index].size)


/**
 * cam_soc_util_get_dt_properties()
 *
 * @brief:              Parse the DT and populate the common properties that
 *                      are part of the soc_info structure - register map,
 *                      clocks, regulators, irq, etc.
 *
 * @soc_info:           Device soc struct to be populated
 *
 * @return:             Success or failure
 */
int cam_soc_util_get_dt_properties(struct cam_hw_soc_info *soc_info);


/**
 * cam_soc_util_request_platform_resource()
 *
 * @brief:              Request regulator, irq, and clock resources
 *
 * @soc_info:           Device soc information
 * @handler:            Irq handler function pointer
 * @irq_data:           Irq handler function CB data
 *
 * @return:             Success or failure
 */
int cam_soc_util_request_platform_resource(struct cam_hw_soc_info *soc_info,
	irq_handler_t handler, void *irq_data);

/**
 * cam_soc_util_release_platform_resource()
 *
 * @brief:              Release regulator, irq, and clock resources
 *
 * @soc_info:           Device soc information
 *
 * @return:             Success or failure
 */
int cam_soc_util_release_platform_resource(struct cam_hw_soc_info *soc_info);

/**
 * cam_soc_util_enable_platform_resource()
 *
 * @brief:              Enable regulator, irq resources
 *
 * @soc_info:           Device soc information
 * @enable_clocks:      Boolean flag:
 *                          TRUE: Enable all clocks in soc_info Now.
 *                          False: Don't enable clocks Now. Driver will
 *                                 enable independently.
 @enable_irq:           Boolean flag:
 *                          TRUE: Enable IRQ in soc_info Now.
 *                          False: Don't enable IRQ Now. Driver will
 *                                 enable independently.
 *
 * @return:             Success or failure
 */
int cam_soc_util_enable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool enable_clocks, bool enable_irq);

/**
 * cam_soc_util_disable_platform_resource()
 *
 * @brief:              Disable regulator, irq resources
 *
 * @soc_info:           Device soc information
 * @disable_irq:        Boolean flag:
 *                          TRUE: Disable IRQ in soc_info Now.
 *                          False: Don't disble IRQ Now. Driver will
 *                                 disable independently.
 *
 * @return:             Success or failure
 */
int cam_soc_util_disable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool disable_clocks, bool disable_irq);

/**
 * cam_soc_util_clk_enable()
 *
 * @brief:              Enable clock specified in params
 *
 * @clk:                Clock that needs to be turned ON
 * @clk_name:           Clocks name associated with clk
 * @clk_rate:           Clocks rate associated with clk
 *
 * @return:             Success or failure
 */
int cam_soc_util_clk_enable(struct clk *clk, const char *clk_name,
	int32_t clk_rate);

/**
 * cam_soc_util_clk_disable()
 *
 * @brief:              Disable clock specified in params
 *
 * @clk:                Clock that needs to be turned OFF
 * @clk_name:           Clocks name associated with clk
 *
 * @return:             Success or failure
 */
int cam_soc_util_clk_disable(struct clk *clk, const char *clk_name);

/**
 * cam_soc_util_irq_enable()
 *
 * @brief:              Enable IRQ in SOC
 *
 * @soc_info:           Device soc information
 *
 * @return:             Success or failure
 */
int cam_soc_util_irq_enable(struct cam_hw_soc_info *soc_info);

/**
 * cam_soc_util_irq_disable()
 *
 * @brief:              Disable IRQ in SOC
 *
 * @soc_info:           Device soc information
 *
 * @return:             Success or failure
 */
int cam_soc_util_irq_disable(struct cam_hw_soc_info *soc_info);

/**
 * cam_soc_util_w()
 *
 * @brief:              Camera SOC util for register write
 *
 * @soc_info:           Device soc information
 * @base_index:         Index of register space in the HW block
 * @offset:             Offset of register to be read
 * @data:               Value to be written
 *
 * @return:             Success or Failure
 */
static inline int cam_soc_util_w(struct cam_hw_soc_info *soc_info,
	uint32_t base_index, uint32_t offset, uint32_t data)
{
	if (!CAM_SOC_GET_REG_MAP_START(soc_info, base_index))
		return -EINVAL;
	return cam_io_w(data,
		CAM_SOC_GET_REG_MAP_START(soc_info, base_index) + offset);
}

/**
 * cam_soc_util_w_mb()
 *
 * @brief:              Camera SOC util for register write with memory barrier.
 *                      Memory Barrier is only before the write to ensure the
 *                      order. If need to ensure this write is also flushed
 *                      call wmb() independently in the caller.
 *
 * @soc_info:           Device soc information
 * @base_index:         Index of register space in the HW block
 * @offset:             Offset of register to be read
 * @data:               Value to be written
 *
 * @return:             Success or Failure
 */
static inline int cam_soc_util_w_mb(struct cam_hw_soc_info *soc_info,
	uint32_t base_index, uint32_t offset, uint32_t data)
{
	if (!CAM_SOC_GET_REG_MAP_START(soc_info, base_index))
		return -EINVAL;
	return cam_io_w_mb(data,
		CAM_SOC_GET_REG_MAP_START(soc_info, base_index) + offset);
}

/**
 * cam_soc_util_r()
 *
 * @brief:              Camera SOC util for register read
 *
 * @soc_info:           Device soc information
 * @base_index:         Index of register space in the HW block
 * @offset:             Offset of register to be read
 *
 * @return:             Value read from the register address
 */
static inline uint32_t cam_soc_util_r(struct cam_hw_soc_info *soc_info,
	uint32_t base_index, uint32_t offset)
{
	if (!CAM_SOC_GET_REG_MAP_START(soc_info, base_index))
		return 0;
	return cam_io_r(
		CAM_SOC_GET_REG_MAP_START(soc_info, base_index) + offset);
}

/**
 * cam_soc_util_r_mb()
 *
 * @brief:              Camera SOC util for register read with memory barrier.
 *                      Memory Barrier is only before the write to ensure the
 *                      order. If need to ensure this write is also flushed
 *                      call rmb() independently in the caller.
 *
 * @soc_info:           Device soc information
 * @base_index:         Index of register space in the HW block
 * @offset:             Offset of register to be read
 *
 * @return:             Value read from the register address
 */
static inline uint32_t cam_soc_util_r_mb(struct cam_hw_soc_info *soc_info,
	uint32_t base_index, uint32_t offset)
{
	if (!CAM_SOC_GET_REG_MAP_START(soc_info, base_index))
		return 0;
	return cam_io_r_mb(
		CAM_SOC_GET_REG_MAP_START(soc_info, base_index) + offset);
}

/**
 * cam_soc_util_reg_dump()
 *
 * @brief:              Camera SOC util for dumping a range of register
 *
 * @soc_info:           Device soc information
 * @base_index:         Index of register space in the HW block
 * @offset:             Start register offset for the dump
 * @size:               Size specifying the range for dump
 *
 * @return:             Success or Failure
 */
int cam_soc_util_reg_dump(struct cam_hw_soc_info *soc_info,
	uint32_t base_index, uint32_t offset, int size);

#endif /* _CAM_SOC_UTIL_H_ */
