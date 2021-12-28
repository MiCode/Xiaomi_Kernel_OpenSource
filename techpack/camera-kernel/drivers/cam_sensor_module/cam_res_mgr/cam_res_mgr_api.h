/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __CAM_RES_MGR_API_H__
#define __CAM_RES_MGR_API_H__

#include <linux/leds.h>

/**
 * @brief: Register the led trigger
 *
 *  The newly registered led trigger is assigned to flash_res_list.
 *
 * @name  : Pointer to int led trigger name
 * @tp    : Save the returned led trigger
 *
 * @return None
 */
void cam_res_mgr_led_trigger_register(const char *name,
	struct led_trigger **tp);

/**
 * @brief: Unregister the led trigger
 *
 *  Free the flash_res if this led trigger isn't used by other device .
 *
 * @tp : Pointer to the led trigger
 *
 * @return None
 */
void cam_res_mgr_led_trigger_unregister(struct led_trigger *tp);

/**
 * @brief: Trigger the event to led core
 *
 * @trig       : Pointer to the led trigger
 * @brightness : The brightness need to fire
 *
 * @return None
 */
void cam_res_mgr_led_trigger_event(struct led_trigger *trig,
	enum led_brightness brightness);

/**
 * @brief: Check for shared gpio
 *
 *  Will check whether requested device shares the gpio with other
 *  device. This function check against gpio table from device and
 *  shared gpio resources has been defined at res-mgr level
 *
 * @gpio_tbl  : The GPIO table for respective device
 * @size      : GPIO table size
 * @return Status of operation. False if not shared, true otherwise.
 */
bool cam_res_mgr_util_check_if_gpio_is_shared(
	struct gpio *gpio_tbl, uint8_t size);

/**
 * @brief: Request a gpio
 *
 *  Will alloc a gpio_res for the new gpio, other find the corresponding
 *  gpio_res.
 *
 * @dev   : Pointer to the device
 * @gpio  : The GPIO number
 * @flags : GPIO configuration as specified by GPIOF_*
 * @label : A literal description string of this GPIO
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_res_mgr_gpio_request(struct device *dev, unsigned int gpio,
		unsigned long flags, const char *label);

/**
 * @brief: Free a array GPIO
 *
 *  Free the GPIOs and release corresponding gpio_res.
 *
 * @dev   : Pointer to the device
 * @gpio  : Array of the GPIO number
 * @num   : The number of gpio
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
void cam_res_mgr_gpio_free_arry(struct device *dev,
	const struct gpio *array, size_t num);

/**
 * @brief: Set GPIO power level
 *
 *  Add ref count support for shared GPIOs.
 *
 * @gpio   : The GPIO number
 * @value  : The power level need to setup
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 * -EINVAL will be returned if the gpio can't be found in gpio_res_list.
 */
int cam_res_mgr_gpio_set_value(unsigned int gpio, int value);


/**
 * @brief : API to register RES_MGR to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_res_mgr_init(void);

/**
 * @brief : API to remove RES_MGR from platform framework.
 */
void cam_res_mgr_exit(void);

/**
 * @brief : API to get gpio idx from shared gpio list.
 * @return idx for Success and error if gpio not found or invalid.
 */
int cam_res_mgr_util_get_idx_from_shared_gpio(uint gpio);

/**
 * @brief : API to get gpio idx from shared pinctrl gpio list.
 * @return idx for Success and error if gpio not found or invalid.
 */
int cam_res_mgr_util_get_idx_from_shared_pctrl_gpio(uint gpio);

/**
 * @brief : API to check whether gpio is in use or can be free.
 * @return NEED_HOLD macro if gpio is in use CAN_FREE if gpio can be free,
 *	error if operation is not valid, 0 in case of res_mgr is not
 *	available.
 */
int cam_res_mgr_util_shared_gpio_check_hold(uint gpio);

#endif /* __CAM_RES_MGR_API_H__ */
