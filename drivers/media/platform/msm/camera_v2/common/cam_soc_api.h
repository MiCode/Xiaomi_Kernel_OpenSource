/* Copyright (c) 2015-2016, 2018, The Linux Foundation. All rights reserved.
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
#include <linux/reset.h>
#include <soc/qcom/camera2.h>

enum cam_bus_client {
	CAM_BUS_CLIENT_VFE,
	CAM_BUS_CLIENT_CPP,
	CAM_BUS_CLIENT_FD,
	CAM_BUS_CLIENT_JPEG_ENC0,
	CAM_BUS_CLIENT_JPEG_ENC1,
	CAM_BUS_CLIENT_JPEG_DEC,
	CAM_BUS_CLIENT_JPEG_DMA,
	CAM_BUS_CLIENT_MAX
};

struct msm_cam_regulator {
	const char *name;
	struct regulator *vdd;
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
 * @brief      : Gets reset info
 *
 * This function extracts the reset information for a specific
 * platform device
 *
 * @param pdev   : platform device to get reset information
 * @param micro_iface_reset : Pointer to populate the reset names
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_get_reset_info(struct platform_device *pdev,
			struct reset_control **micro_iface_reset);
/**
 * @brief      : Sets flags of a clock
 *
 * This function will set the flags for a specified clock
 *
 * @param clk   : Pointer to clock to set flags for
 * @param flags : The flags to set
 *
 * @return Status of operation.
 */

int msm_camera_set_clk_flags(struct clk *clk, unsigned long flags);
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
 * @brief      : set the regultors mode
 *
 * This function sets the regulators for a specific
 * mode. say:REGULATOR_MODE_FAST/REGULATOR_MODE_NORMAL
 *
 * @param vdd_info: Pointer to list of regulators
 * @param cnt: Number of regulators to enable/disable
 * @param mode: Flags specifies either enable/disable
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int msm_camera_regulator_set_mode(struct msm_cam_regulator *vdd_info,
				int cnt, bool mode);


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
 * @brief      : Register the bus client
 *
 * This function registers the bus client
 *
 * @param pdev : Pointer to platform device
 * @param id : client identifier
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

uint32_t msm_camera_register_bus_client(struct platform_device *pdev,
	enum cam_bus_client id);

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

uint32_t msm_camera_update_bus_vector(enum cam_bus_client id,
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

uint32_t msm_camera_unregister_bus_client(enum cam_bus_client id);

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

#endif
