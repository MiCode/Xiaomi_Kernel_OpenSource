/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __CAM_RES_MGR_PRIVATE_H__
#define __CAM_RES_MGR_PRIVATE_H__

#include <linux/list.h>
#include <linux/leds.h>
#include "cam_soc_util.h"

#define MAX_SHARED_GPIO_SIZE 16

/* pinctrl states name */
#define CAM_RES_MGR_SLEEP	"cam_res_mgr_suspend"
#define CAM_RES_MGR_DEFAULT	"cam_res_mgr_default"

/**
 * enum pinctrl_status - Enum for pinctrl status
 */
enum pinctrl_status {
	PINCTRL_STATUS_GOT = 0,
	PINCTRL_STATUS_ACTIVE,
	PINCTRL_STATUS_SUSPEND,
	PINCTRL_STATUS_PUT,
};

/**
 * struct cam_dev_res
 *
 * @list : List member used to append this node to a dev list
 * @dev  : Device pointer associated with device
 */
struct cam_dev_res {
	struct list_head list;
	struct device    *dev;
};

/**
 * struct cam_gpio_res
 *
 * @list           : List member used to append this node to a gpio list
 * @dev_list       : List the device which request this gpio
 * @gpio           : Gpio value
 * @power_on_count : Record the power on times of this gpio
 */
struct cam_gpio_res {
	struct list_head list;
	struct list_head dev_list;
	unsigned int     gpio;
	int              power_on_count;
};

/**
 * struct cam_pinctrl_res
 *
 * @list           : List member used to append this node to a linked list
 * @name           : Pointer to the flash trigger's name.
 * @trigger        : Pointer to the flash trigger
 */
struct cam_flash_res {
	struct list_head   list;
	const char         *name;
	struct led_trigger *trigger;
};

/**
 * struct cam_res_mgr_dt
 *
 * @shared_gpio     : Shared gpios list in the device tree
 * @num_shared_gpio : The number of shared gpio
 * @pinctrl_info    : Pinctrl information
 */
struct cam_res_mgr_dt {
	uint                        shared_gpio[MAX_SHARED_GPIO_SIZE];
	int                         num_shared_gpio;
	struct cam_soc_pinctrl_info pinctrl_info;
};

/**
 * struct cam_pinctrl_res
 *
 * @dev                 : Pointer to the device
 * @dt                  : Device tree resource
 * @shared_gpio_enabled : The flag to indicate if support shared gpio
 * @pstatus             : Shared pinctrl status
 * @gpio_res_list       : List head of the gpio resource
 * @flash_res_list      : List head of the flash resource
 * @gpio_res_lock       : GPIO resource lock
 * @flash_res_lock      : Flash resource lock
 * @clk_res_lock        : Clk resource lock
 */
struct cam_res_mgr {
	struct device         *dev;
	struct cam_res_mgr_dt dt;

	bool                  shared_gpio_enabled;
	enum pinctrl_status   pstatus;

	uint                  shared_clk_ref_count;

	struct list_head      gpio_res_list;
	struct list_head      flash_res_list;
	struct mutex          gpio_res_lock;
	struct mutex          flash_res_lock;
	struct mutex          clk_res_lock;
};

#endif /* __CAM_RES_MGR_PRIVATE_H__ */
