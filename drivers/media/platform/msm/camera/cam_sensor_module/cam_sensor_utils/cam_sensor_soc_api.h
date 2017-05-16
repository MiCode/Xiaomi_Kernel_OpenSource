/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_SENSOR_SOC_API_H_
#define _CAM_SENSOR_SOC_API_H_

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include "cam_sensor_cmn_header.h"

struct msm_cam_regulator {
	const char *name;
	struct regulator *vdd;
};

struct msm_gpio_set_tbl {
	unsigned int gpio;
	unsigned long flags;
	uint32_t delay;
};

/**
 * @brief      : Gets clock information from dtsi
 *
 * This function extracts the clocks information for a specific
 * platform device
 *
 * @param pdev   : Platform device to get clocks information
 * @param clk_info   : Pointer to populate clock information array
 * @param clk_ptr   : Pointer to populate clock resource pointers
 * @param num_clk: Pointer to populate the number of clocks
 *                 extracted from dtsi
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_get_clk_info(struct platform_device *pdev,
			struct msm_cam_clk_info **clk_info,
			struct clk ***clk_ptr,
			size_t *num_clk);

/**
 * @brief      : Gets clock information from dtsi
 *
 * This function extracts the clocks information for a specific
 * i2c device
 *
 * @param dev   : i2c device to get clocks information
 * @param clk_info   : Pointer to populate clock information array
 * @param clk_ptr   : Pointer to populate clock resource pointers
 * @param num_clk: Pointer to populate the number of clocks
 *                 extracted from dtsi
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_i2c_dev_get_clk_info(struct device *dev,
			struct msm_cam_clk_info **clk_info,
			struct clk ***clk_ptr,
			size_t *num_clk);

/**
 * @brief      : Gets clock information and rates from dtsi
 *
 * This function extracts the clocks information for a specific
 * platform device
 *
 * @param pdev   : Platform device to get clocks information
 * @param clk_info   : Pointer to populate clock information array
 * @param clk_ptr   : Pointer to populate clock resource pointers
 * @param clk_rates   : Pointer to populate clock rates
 * @param num_set: Pointer to populate the number of sets of rates
 * @param num_clk: Pointer to populate the number of clocks
 *                 extracted from dtsi
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_get_clk_info_and_rates(
			struct platform_device *pdev,
			struct msm_cam_clk_info **clk_info,
			struct clk ***clk_ptr,
			uint32_t ***clk_rates,
			size_t *num_set,
			size_t *num_clk);

/**
 * @brief      : Puts clock information
 *
 * This function releases the memory allocated for the clocks
 *
 * @param pdev   : Pointer to platform device
 * @param clk_info   : Pointer to release the allocated memory
 * @param clk_ptr   : Pointer to release the clock resources
 * @param cnt   : Number of clk resources
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_put_clk_info(struct platform_device *pdev,
				struct msm_cam_clk_info **clk_info,
				struct clk ***clk_ptr, int cnt);

/**
 * @brief      : Puts clock information
 *
 * This function releases the memory allocated for the clocks
 *
 * @param dev   : Pointer to i2c device
 * @param clk_info   : Pointer to release the allocated memory
 * @param clk_ptr   : Pointer to release the clock resources
 * @param cnt   : Number of clk resources
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_i2c_dev_put_clk_info(struct device *dev,
			struct msm_cam_clk_info **clk_info,
			struct clk ***clk_ptr, int cnt);

/**
 * @brief      : Puts clock information
 *
 * This function releases the memory allocated for the clocks
 *
 * @param pdev   : Pointer to platform device
 * @param clk_info   : Pointer to release the allocated memory
 * @param clk_ptr   : Pointer to release the clock resources
 * @param clk_ptr   : Pointer to release the clock rates
 * @param set   : Number of sets of clock rates
 * @param cnt   : Number of clk resources
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_put_clk_info_and_rates(struct platform_device *pdev,
		struct msm_cam_clk_info **clk_info,
		struct clk ***clk_ptr, uint32_t ***clk_rates,
		size_t set, size_t cnt);
/**
 * @brief      : Enable clocks
 *
 * This function enables the clocks for a specified device
 *
 * @param dev   : Device to get clocks information
 * @param clk_info   : Pointer to populate clock information
 * @param clk_ptr   : Pointer to populate clock information
 * @param num_clk: Pointer to populate the number of clocks
 *                 extracted from dtsi
 * @param enable   : Flag to specify enable/disable
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_clk_enable(struct device *dev,
					struct msm_cam_clk_info *clk_info,
					struct clk **clk_ptr,
					int num_clk,
					int enable);
/**
 * @brief      : Set clock rate
 *
 * This function sets the rate for a specified clock and
 * returns the rounded value
 *
 * @param dev   : Device to get clocks information
 * @param clk   : Pointer to clock to set rate
 * @param clk_rate   : Rate to be set
 *
 * @return Status of operation. Negative in case of error. clk rate otherwise.
 */

long msm_camera_clk_set_rate(struct device *dev,
				struct clk *clk,
				long clk_rate);
/**
 * @brief      : Gets regulator info
 *
 * This function extracts the regulator information for a specific
 * platform device
 *
 * @param pdev   : platform device to get regulator information
 * @param vdd_info: Pointer to populate the regulator names
 * @param num_reg: Pointer to populate the number of regulators
 *                 extracted from dtsi
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_get_regulator_info(struct platform_device *pdev,
		struct msm_cam_regulator **vdd_info, int *num_reg);
/**
 * @brief      : Enable/Disable the regultors
 *
 * This function enables/disables the regulators for a specific
 * platform device
 *
 * @param vdd_info: Pointer to list of regulators
 * @param cnt: Number of regulators to enable/disable
 * @param enable: Flags specifies either enable/disable
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_regulator_enable(struct msm_cam_regulator *vdd_info,
				int cnt, int enable);

/**
 * @brief      : Release the regulators
 *
 * This function releases the regulator resources.
 *
 * @param pdev: Pointer to platform device
 * @param vdd_info: Pointer to list of regulators
 * @param cnt: Number of regulators to release
 */

void msm_camera_put_regulators(struct platform_device *pdev,
	struct msm_cam_regulator **vdd_info, int cnt);
/**
 * @brief      : Get the IRQ resource
 *
 * This function gets the irq resource from dtsi for a specific
 * platform device
 *
 * @param pdev   : Platform device to get IRQ
 * @param irq_name: Name of the IRQ resource to get from DTSI
 *
 * @return Pointer to resource if success else null
 */

struct resource *msm_camera_get_irq(struct platform_device *pdev,
							char *irq_name);
/**
 * @brief      : Register the IRQ
 *
 * This function registers the irq resource for specified hardware
 *
 * @param pdev    : Platform device to register IRQ resource
 * @param irq	  : IRQ resource
 * @param handler : IRQ handler
 * @param irqflags : IRQ flags
 * @param irq_name: Name of the IRQ
 * @param dev	 : Token of the device
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_register_irq(struct platform_device *pdev,
						struct resource *irq,
						irq_handler_t handler,
						unsigned long irqflags,
						char *irq_name,
						void *dev);

/**
 * @brief      : Register the threaded IRQ
 *
 * This function registers the irq resource for specified hardware
 *
 * @param pdev    : Platform device to register IRQ resource
 * @param irq	  : IRQ resource
 * @param handler_fn : IRQ handler function
 * @param thread_fn : thread handler function
 * @param irqflags : IRQ flags
 * @param irq_name: Name of the IRQ
 * @param dev	 : Token of the device
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_register_threaded_irq(struct platform_device *pdev,
						struct resource *irq,
						irq_handler_t handler_fn,
						irq_handler_t thread_fn,
						unsigned long irqflags,
						const char *irq_name,
						void *dev);

/**
 * @brief      : Enable/Disable the IRQ
 *
 * This function enables or disables a specific IRQ
 *
 * @param irq    : IRQ resource
 * @param flag   : flag to enable/disable
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_enable_irq(struct resource *irq, int flag);

/**
 * @brief      : UnRegister the IRQ
 *
 * This function Unregisters/Frees the irq resource
 *
 * @param pdev   : Pointer to platform device
 * @param irq    : IRQ resource
 * @param dev    : Token of the device
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_unregister_irq(struct platform_device *pdev,
	struct resource *irq, void *dev_id);

/**
 * @brief      : Gets device register base
 *
 * This function extracts the device's register base from the dtsi
 * for the specified platform device
 *
 * @param pdev   : Platform device to get regulator infor
 * @param device_name   : Name of the device to fetch the register base
 * @param reserve_mem   : Flag to decide whether to reserve memory
 * region or not.
 *
 * @return Pointer to resource if success else null
 */

void __iomem *msm_camera_get_reg_base(struct platform_device *pdev,
		char *device_name, int reserve_mem);

/**
 * @brief      :  Puts device register base
 *
 * This function releases the memory region for the specified
 * resource
 *
 * @param pdev   : Pointer to platform device
 * @param base   : Pointer to base to unmap
 * @param device_name : Device name
 * @param reserve_mem   : Flag to decide whether to release memory
 * region or not.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_put_reg_base(struct platform_device *pdev, void __iomem *base,
		char *device_name, int reserve_mem);

/**
 * @brief      : Gets resource size
 *
 * This function returns the size of the resource for the
 * specified platform device
 *
 * @param pdev   : Platform device to get regulator infor
 * @param device_name   : Name of the device to fetch the register base
 *
 * @return size of the resource
 */

uint32_t msm_camera_get_res_size(struct platform_device *pdev,
	char *device_name);

/**
 * @brief      : Selects clock source
 *
 *
 * @param dev : Token of the device
 * @param clk_info : Clock Info structure
 * @param clk_src_info : Clock Info structure
 * @param num_clk : Number of clocks
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_cam_clk_sel_src(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct msm_cam_clk_info *clk_src_info, int num_clk);

/**
 * @brief      : Enables the clock
 *
 *
 * @param dev : Token of the device
 * @param clk_info : Clock Info structure
 * @param clk_tr : Pointer to lock strucure
 * @param num_clk : Number of clocks
 * @param enable : Enable/disable the clock
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_cam_clk_enable(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable);

/**
 * @brief      : Configures voltage regulator
 *
 *
 * @param dev : Token of the device
 * @param cam_vreg : Regulator dt structure
 * @param num_vreg : Number of regulators
 * @param vreg_seq : Regulator sequence type
 * @param num_clk : Number of clocks
 * @param reg_ptr : Regulator pointer
 * @param config : Enable/disable configuring the regulator
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_config_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int config);

/**
 * @brief      : Enables voltage regulator
 *
 *
 * @param dev : Token of the device
 * @param cam_vreg : Regulator dt structure
 * @param num_vreg : Number of regulators
 * @param vreg_seq : Regulator sequence type
 * @param num_clk : Number of clocks
 * @param reg_ptr : Regulator pointer
 * @param config : Enable/disable configuring the regulator
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_enable_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int enable);

/**
 * @brief      : Sets table of GPIOs
 *
 * @param gpio_tbl : GPIO table parsed from dt
 * @param gpio_tbl_size : Size of GPIO table
 * @param gpio_en : Enable/disable the GPIO
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_set_gpio_table(struct msm_gpio_set_tbl *gpio_tbl,
	uint8_t gpio_tbl_size, int gpio_en);

/**
 * @brief      : Configures single voltage regulator
 *
 *
 * @param dev : Token of the device
 * @param cam_vreg : Regulator dt structure
 * @param num_vreg : Number of regulators
 * @param reg_ptr : Regulator pointer
 * @param config : Enable/disable configuring the regulator
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_config_single_vreg(struct device *dev,
	struct camera_vreg_t *cam_vreg, struct regulator **reg_ptr, int config);

/**
 * @brief      : Request table of gpios
 *
 *
 * @param gpio_tbl : Table of GPIOs
 * @param size : Size of table
 * @param gpio_en : Enable/disable the gpio
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_request_gpio_table(struct gpio *gpio_tbl, uint8_t size,
	int gpio_en);

#endif /* _CAM_SENSOR_SOC_API_H_ */
