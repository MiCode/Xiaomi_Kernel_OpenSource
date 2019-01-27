/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_IR_LED_CORE_H_
#define _CAM_IR_LED_CORE_H_
#include "cam_ir_led_dev.h"

#define IRLED_INTENSITY_OFF    0
#define IRLED_INTENSITY_LEVEL1 1
#define IRLED_INTENSITY_LEVEL2 2
#define IRLED_INTENSITY_LEVEL3 3
#define IRLED_INTENSITY_LEVEL4 4
#define IRLED_INTENSITY_MAX    5


int cam_ir_led_parser(struct cam_ir_led_ctrl *fctrl, void *arg);
void cam_ir_led_shutdown(struct cam_ir_led_ctrl *ir_led_ctrl);
int cam_ir_led_stop_dev(struct cam_ir_led_ctrl *ir_led_ctrl);
int cam_ir_led_release_dev(struct cam_ir_led_ctrl *fctrl);
#endif /*_CAM_IR_LED_CORE_H_*/
