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

#ifndef __CAM_RES_MGR_API_H__
#define __CAM_RES_MGR_API_H__

#include <linux/leds.h>

/**
 * @brief: Register the led trigger
 *
 *  The newly registered led trigger is assigned to flash_res_list.
 *
 * @param name  : Pointer to int led trigger name
 * @param tp    : Save the returned led trigger
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
 * @param tp : Pointer to the led trigger
 *
 * @return None
 */
void cam_res_mgr_led_trigger_unregister(struct led_trigger *tp);

/**
 * @brief: Trigger the event to led core
 *
 * @param trig       : Pointer to the led trigger
 * @param brightness : The brightness need to fire
 *
 * @return None
 */
void cam_res_mgr_led_trigger_event(struct led_trigger *trig,
		enum led_brightness brightness);

/**
 * @brief: Get the corresponding pinctrl of dev
 *
 *  Init the shared pinctrl if shared pinctrl enabled.
 *
 * @return None
 */
void cam_res_mgr_shared_pinctrl_init(void);


/**
 * @brief: Put the pinctrl
 *
 *  Put the shared pinctrl.
 *
 * @return None
 */
void cam_res_mgr_shared_pinctrl_put(void);

/**
 * @brief: Select the corresponding state
 *
 *  Active state can be selected directly, but need hold to suspend the
 *  pinctrl if the gpios in this pinctrl also held by other pinctrl.
 *
 * @param active   : The flag to indicate whether active or suspend
 * the shared pinctrl.
 *
 * @return None
 */
void cam_res_mgr_shared_pinctrl_select_state(bool active);

/**
 * @brief: Request a gpio
 *
 *  Will alloc a gpio_res for the new gpio, other find the corresponding
 *  gpio_res.
 *
 * @param dev   : Pointer to the device
 * @param gpio  : The GPIO number
 * @param flags : GPIO configuration as specified by GPIOF_*
 * @param label : A literal description string of this GPIO
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
 * @param dev   : Pointer to the device
 * @param gpio  : Array of the GPIO number
 * @param num   : The number of gpio
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
 * @param gpio   : The GPIO number
 * @param value  : The power level need to setup
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 * -EINVAL will be returned if the gpio can't be found in gpio_res_list.
 */
int cam_res_mgr_gpio_set_value(unsigned int gpio, int value);

#endif /* __CAM_RES_MGR_API_H__ */
