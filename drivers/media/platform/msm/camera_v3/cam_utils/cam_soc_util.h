/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/clk/qcom.h>
#include <linux/debugfs.h>

#include "cam_io_util.h"

#define NO_SET_RATE  -1
#define INIT_RATE    -2

/* maximum number of device block */
#define CAM_SOC_MAX_BLOCK           4

/* maximum number of device base */
#define CAM_SOC_MAX_BASE            CAM_SOC_MAX_BLOCK

/* maximum number of device regulator */
#define CAM_SOC_MAX_REGULATOR       5

/* maximum number of device clock */
#define CAM_SOC_MAX_CLK             32

/* soc id */
#define SDM670_SOC_ID 336
#define SDM710_SOC_ID 360
#define SDM712_SOC_ID 393

/* Minor Version */
#define SDM670_V1_1 0x1
/**
 * enum cam_vote_level - Enum for voting level
 *
 * @CAM_SUSPEND_VOTE  : Suspend vote
 * @CAM_MINSVS_VOTE   : Min SVS vote
 * @CAM_LOWSVS_VOTE   : Low SVS vote
 * @CAM_SVS_VOTE      : SVS vote
 * @CAM_SVSL1_VOTE    : SVS Plus vote
 * @CAM_NOMINAL_VOTE  : Nominal vote
 * @CAM_NOMINALL1_VOTE: Nominal plus vote
 * @CAM_TURBO_VOTE    : Turbo vote
 * @CAM_MAX_VOTE      : Max voting level, This is invalid level.
 */
enum cam_vote_level {
	CAM_SUSPEND_VOTE,
	CAM_MINSVS_VOTE,
	CAM_LOWSVS_VOTE,
	CAM_SVS_VOTE,
	CAM_SVSL1_VOTE,
	CAM_NOMINAL_VOTE,
	CAM_NOMINALL1_VOTE,
	CAM_TURBO_VOTE,
	CAM_MAX_VOTE,
};

/* pinctrl states */
#define CAM_SOC_PINCTRL_STATE_SLEEP "cam_suspend"
#define CAM_SOC_PINCTRL_STATE_DEFAULT "cam_default"

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
 * struct cam_soc_pinctrl_info:   Information about pinctrl data
 *
 * @pinctrl:               pintrl object
 * @gpio_state_active:     default pinctrl state
 * @gpio_state_suspend     suspend state of pinctrl
 **/
struct cam_soc_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};

/**
 * struct cam_soc_gpio_data:   Information about the gpio pins
 *
 * @cam_gpio_common_tbl:       It is list of al the gpios present in gpios node
 * @cam_gpio_common_tbl_size:  It is equal to number of gpios prsent in
 *                             gpios node in DTSI
 * @cam_gpio_req_tbl            It is list of al the requesetd gpios
 * @cam_gpio_req_tbl_size:      It is size of requested gpios
 **/
struct cam_soc_gpio_data {
	struct gpio *cam_gpio_common_tbl;
	uint8_t cam_gpio_common_tbl_size;
	struct gpio *cam_gpio_req_tbl;
	uint8_t cam_gpio_req_tbl_size;
};

/**
 * struct cam_hw_soc_info:  Soc information pertaining to specific instance of
 *                          Camera hardware driver module
 *
 * @pdev:                   Platform device pointer
 * @device:                 Device pointer
 * @hw_version:             Camera device version
 * @index:                  Instance id for the camera device
 * @dev_name:               Device Name
 * @irq_name:               Name of the irq associated with the device
 * @irq_line:               Irq resource
 * @irq_data:               Private data that is passed when IRQ is requested
 * @num_mem_block:          Number of entry in the "reg-names"
 * @mem_block_name:         Array of the reg block name
 * @mem_block_cam_base:     Array of offset of this register space compared
 *                          to ENTIRE Camera register space
 * @mem_block:              Associated resource structs
 * @reg_map:                Array of Mapped register info for the "reg-names"
 * @num_reg_map:            Number of mapped register space associated
 *                          with mem_block. num_reg_map = num_mem_block in
 *                          most cases
 * @reserve_mem:            Whether to reserve memory for Mem blocks
 * @num_rgltr:              Number of regulators
 * @rgltr_name:             Array of regulator names
 * @rgltr_ctrl_support:     Whether regulator control is supported
 * @rgltr_min_volt:         Array of minimum regulator voltage
 * @rgltr_max_volt:         Array of maximum regulator voltage
 * @rgltr_op_mode:          Array of regulator operation mode
 * @rgltr_type:             Array of regulator names
 * @rgltr:                  Array of associated regulator resources
 * @rgltr_delay:            Array of regulator delay values
 * @num_clk:                Number of clocks
 * @clk_name:               Array of clock names
 * @clk:                    Array of associated clock resources
 * @clk_rate:               2D array of clock rates representing clock rate
 *                          values at different vote levels
 * @prev_clk_level          Last vote level
 * @src_clk_idx:            Source clock index that is rate-controllable
 * @clk_level_valid:        Indicates whether corresponding level is valid
 * @gpio_data:              Pointer to gpio info
 * @pinctrl_info:           Pointer to pinctrl info
 * @dentry:                 Debugfs entry
 * @clk_level_override:     Clk level set from debugfs
 * @clk_control:            Enable/disable clk rate control through debugfs
 * @cam_cx_ipeak_enable     cx-ipeak enable/disable flag
 * @cam_cx_ipeak_bit        cx-ipeak mask for driver
 * @soc_private:            Soc private data
 */
struct cam_hw_soc_info {
	struct platform_device         *pdev;
	struct device                  *dev;
	uint32_t                        hw_version;
	uint32_t                        index;
	const char                     *dev_name;
	const char                     *irq_name;
	struct resource                *irq_line;
	void                           *irq_data;

	uint32_t                        num_mem_block;
	const char                     *mem_block_name[CAM_SOC_MAX_BLOCK];
	uint32_t                        mem_block_cam_base[CAM_SOC_MAX_BLOCK];
	struct resource                *mem_block[CAM_SOC_MAX_BLOCK];
	struct cam_soc_reg_map          reg_map[CAM_SOC_MAX_BASE];
	uint32_t                        num_reg_map;
	uint32_t                        reserve_mem;

	uint32_t                        num_rgltr;
	const char                     *rgltr_name[CAM_SOC_MAX_REGULATOR];
	uint32_t                        rgltr_ctrl_support;
	uint32_t                        rgltr_min_volt[CAM_SOC_MAX_REGULATOR];
	uint32_t                        rgltr_max_volt[CAM_SOC_MAX_REGULATOR];
	uint32_t                        rgltr_op_mode[CAM_SOC_MAX_REGULATOR];
	uint32_t                        rgltr_type[CAM_SOC_MAX_REGULATOR];
	struct regulator               *rgltr[CAM_SOC_MAX_REGULATOR];
	uint32_t                        rgltr_delay[CAM_SOC_MAX_REGULATOR];

	uint32_t                        use_shared_clk;
	uint32_t                        num_clk;
	const char                     *clk_name[CAM_SOC_MAX_CLK];
	struct clk                     *clk[CAM_SOC_MAX_CLK];
	int32_t                         clk_rate[CAM_MAX_VOTE][CAM_SOC_MAX_CLK];
	int32_t                         prev_clk_level;
	int32_t                         src_clk_idx;
	bool                            clk_level_valid[CAM_MAX_VOTE];

	struct cam_soc_gpio_data       *gpio_data;
	struct cam_soc_pinctrl_info     pinctrl_info;

	struct dentry                  *dentry;
	uint32_t                        clk_level_override;
	bool                            clk_control_enable;
	bool                            cam_cx_ipeak_enable;
	int32_t                         cam_cx_ipeak_bit;

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
 * cam_soc_util_get_level_from_string()
 *
 * @brief:              Get the associated vote level for the input string
 *
 * @string:             Input string to compare with.
 * @level:              Vote level corresponds to input string.
 *
 * @return:             Success or failure
 */
int cam_soc_util_get_level_from_string(const char *string,
	enum cam_vote_level *level);

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
 * @clk_level:          Clock level to be applied.
 *                      Applicable only if enable_clocks is true
 *                          Valid range : 0 to (CAM_MAX_VOTE - 1)
 * @enable_irq:         Boolean flag:
 *                          TRUE: Enable IRQ in soc_info Now.
 *                          False: Don't enable IRQ Now. Driver will
 *                                 enable independently.
 *
 * @return:             Success or failure
 */
int cam_soc_util_enable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool enable_clocks, enum cam_vote_level clk_level, bool enable_irq);

/**
 * cam_soc_util_disable_platform_resource()
 *
 * @brief:              Disable regulator, irq resources
 *
 * @soc_info:           Device soc information
 * @disable_irq:        Boolean flag:
 *                          TRUE: Disable IRQ in soc_info Now.
 *                          False: Don't disable IRQ Now. Driver will
 *                                 disable independently.
 *
 * @return:             Success or failure
 */
int cam_soc_util_disable_platform_resource(struct cam_hw_soc_info *soc_info,
	bool disable_clocks, bool disable_irq);

/**
 * cam_soc_util_get_clk_round_rate()
 *
 * @brief:              Get the rounded clock rate for the given clock's
 *                      clock rate value
 *
 * @soc_info:           Device soc information
 * @clk_index:          Clock index in soc_info for which round rate is needed
 * @clk_rate:           Input clock rate for which rounded rate is needed
 *
 * @return:             Rounded clock rate
 */
long cam_soc_util_get_clk_round_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_index, unsigned long clk_rate);

/**
 * cam_soc_util_set_clk_flags()
 *
 * @brief:              Camera SOC util to set the flags for a specified clock
 *
 * @soc_info:           Device soc information
 * @clk_index:          Clock index in soc_info for which flags are to be set
 * @flags:              Flags to set
 *
 * @return:             Success or Failure
 */
int cam_soc_util_set_clk_flags(struct cam_hw_soc_info *soc_info,
	 uint32_t clk_index, unsigned long flags);

/**
 * cam_soc_util_set_src_clk_rate()
 *
 * @brief:              Set the rate on the source clock.
 *
 * @soc_info:           Device soc information
 * @clk_rate:           Clock rate associated with the src clk
 *
 * @return:             success or failure
 */
int cam_soc_util_set_src_clk_rate(struct cam_hw_soc_info *soc_info,
	int32_t clk_rate);

/**
 * cam_soc_util_get_option_clk_by_name()
 *
 * @brief:              Get reference to optional clk using name
 *
 * @soc_info:           Device soc information
 * @clk_name:           Name of clock to find reference for
 * @clk:                Clock reference pointer to be filled if Success
 * @clk_index:          Clk index in the option clk array to be returned
 * @clk_rate:           Clk rate in the option clk array
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_soc_util_get_option_clk_by_name(struct cam_hw_soc_info *soc_info,
	const char *clk_name, struct clk **clk, int32_t *clk_index,
	int32_t *clk_rate);

/**
 * cam_soc_util_clk_put()
 *
 * @brief:              Put clock specified in params
 *
 * @clk:                Reference to the Clock that needs to be put
 *
 * @return:             Success or failure
 */
int cam_soc_util_clk_put(struct clk **clk);

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
 * cam_soc_util_set_clk_rate_level()
 *
 * @brief:              Apply clock rates for the requested level.
 *                      This applies the new requested level for all
 *                      the clocks listed in DT based on their values.
 *
 * @soc_info:           Device soc information
 * @clk_level:          Clock level number to set
 *
 * @return:             Success or failure
 */
int cam_soc_util_set_clk_rate_level(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level clk_level);

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
 * cam_soc_util_regulator_enable()
 *
 * @brief:              Enable single regulator
 *
 * @rgltr               Regulator that needs to be turned ON
 * @rgltr_name          Associated Regulator name
 * @rgltr_min_volt:     Requested minimum volatage
 * @rgltr_max_volt:     Requested maximum volatage
 * @rgltr_op_mode:      Requested Load
 * @rgltr_delay:        Requested delay needed aaftre enabling regulator
 *
 * @return:             Success or failure
 */
int cam_soc_util_regulator_enable(struct regulator *rgltr,
	const char *rgltr_name,
	uint32_t rgltr_min_volt, uint32_t rgltr_max_volt,
	uint32_t rgltr_op_mode, uint32_t rgltr_delay);

/**
 * cam_soc_util_regulator_enable()
 *
 * @brief:              Disable single regulator
 *
 * @rgltr               Regulator that needs to be turned ON
 * @rgltr_name          Associated Regulator name
 * @rgltr_min_volt:     Requested minimum volatage
 * @rgltr_max_volt:     Requested maximum volatage
 * @rgltr_op_mode:      Requested Load
 * @rgltr_delay:        Requested delay needed aaftre enabling regulator
 *
 * @return:             Success or failure
 */
int cam_soc_util_regulator_disable(struct regulator *rgltr,
	const char *rgltr_name,
	uint32_t rgltr_min_volt, uint32_t rgltr_max_volt,
	uint32_t rgltr_op_mode, uint32_t rgltr_delay);


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

void cam_soc_util_clk_disable_default(struct cam_hw_soc_info *soc_info);

int cam_soc_util_clk_enable_default(struct cam_hw_soc_info *soc_info,
	enum cam_vote_level clk_level);
/**
 * cam_soc_util_get_soc_id()
 *
 * @brief:           Read soc id
 *
 * @return           SOC id
 */
uint32_t cam_soc_util_get_soc_id(void);

/**
 * cam_soc_util_get_hw_revision_node()
 *
 * @brief:           Camera HW ID
 *
 * @soc_info:        Device soc information
 *
 * @return           HW id
 */
uint32_t cam_soc_util_get_hw_revision_node(struct cam_hw_soc_info *soc_info);
#endif /* _CAM_SOC_UTIL_H_ */
