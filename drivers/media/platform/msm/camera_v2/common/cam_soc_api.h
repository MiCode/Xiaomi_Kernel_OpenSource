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
#ifndef _CAM_SOC_API_H_
#define _CAM_SOC_API_H_

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <soc/qcom/camera2.h>
#include "cam_hw_ops.h"

/**
 * @brief      : Gets clock information from dtsi
 *
 * This function extracts the clocks information for a specific
 * platform device
 *
 * @param pdev   : Platform device to get clocks information
 * @param clk_info   : Pointer to populate clock information array
 * @param clk_info   : Pointer to populate clock resource pointers
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
 * This function sets the rate for a specified clock
 *
 * @param dev   : Device to get clocks information
 * @param clk   : Pointer to clock to set rate
 * @param clk_rate   : Rate to be set
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_clk_set_rate(struct device *dev,
				struct clk *clk,
				long clk_rate);

/**
 * @brief      : Gets regulator info
 *
 * This function extracts the regulator information for a specific
 * platform device
 *
 * @param pdev   : platform device to get regulator information
 * @param vdd: Pointer to populate the regulator names
 * @param num_reg: Pointer to populate the number of regulators
 *                 extracted from dtsi
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int msm_camera_get_regulator_info(struct platform_device *pdev,
		struct regulator ***vddd, int *num_reg);
/**
 * @brief      : Enable/Disable the regultors
 *
 * This function enables/disables the regulators for a specific
 * platform device
 *
 * @param vdd: Pointer to list of regulators
 * @param cnt: Number of regulators to enable/disable
 * @param enable: Flags specifies either enable/disable
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_regulator_enable(struct regulator **vdd,
				int cnt, int enable);

/**
 * @brief      : Release the regulators
 *
 * This function releases the regulator resources.
 *
 * @param pdev: Pointer to platform device
 * @param vdd: Pointer to list of regulators
 * @param cnt: Number of regulators to release
 */

void msm_camera_put_regulators(struct platform_device *pdev,
							struct regulator ***vdd,
							int cnt);
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
 * @param irq_name: Name of the IRQ
 * @param dev	 : Token of the device
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_register_irq(struct platform_device *pdev,
						struct resource *irq,
						irq_handler_t handler,
						char *irq_name,
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
 *
 * @return Pointer to resource if success else null
 */

void __iomem *msm_camera_get_reg_base(struct platform_device *pdev,
		char *device_name);

/**
 * @brief      :  Puts device register base
 *
 * This function releases the memory region for the specified
 * resource
 *
 * @param pdev   : Pointer to platform device
 * @param base   : Pointer to base to unmap
 * @param device_name : Device name
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_put_reg_base(struct platform_device *pdev, void __iomem *base,
		char *device_name);

/**
 * @brief      : Register the bus client
 *
 * This function registers the bus client
 *
 * @param client_id : client identifier
 * @param pdev : Pointer to platform device
 * @param vector_index : vector index to register
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

uint32_t msm_camera_register_bus_client(struct platform_device *pdev,
	enum cam_ahb_clk_client id);

/**
 * @brief      : Update bus vector
 *
 * This function votes for the specified vector to the bus
 *
 * @param id : client identifier
 * @param vector_index : vector index to register
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

uint32_t msm_camera_update_bus_vector(enum cam_ahb_clk_client id,
	int vector_index);

/**
 * @brief      : Update the bus bandwidth
 *
 * This function updates the bandwidth for the specific client
 *
 * @param client_id   : client identifier
 * @param ab    : Asolute bandwidth
 * @param ib    : Instantaneous bandwidth
 *
 * @return non-zero as client id if success else fail
 */

uint32_t msm_camera_update_bus_bw(int id, uint64_t ab, uint64_t ib);

/**
 * @brief      : UnRegister the bus client
 *
 * This function unregisters the bus client
 *
 * @param id   : client identifier
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

uint32_t msm_camera_unregister_bus_client(enum cam_ahb_clk_client id);

#endif
