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

#include "cam_ir_led_core.h"

int cam_ir_led_stop_dev(struct cam_ir_led_ctrl *ictrl)
{
	return ictrl->func_tbl->camera_ir_led_off(ictrl);
}

int cam_ir_led_release_dev(struct cam_ir_led_ctrl *ictrl)
{
	int rc = 0;

	if (ictrl->device_hdl != -1) {
		rc = cam_destroy_device_hdl(ictrl->device_hdl);
		if (rc)
			CAM_ERR(CAM_IR_LED,
				"Failed in destroying device handle rc = %d",
				rc);
		ictrl->device_hdl = -1;
	}

	return rc;
}

void cam_ir_led_shutdown(struct cam_ir_led_ctrl *ictrl)
{
	int rc;

	if (ictrl->ir_led_state == CAM_IR_LED_STATE_INIT)
		return;

	if (ictrl->ir_led_state == CAM_IR_LED_STATE_ON) {
		rc = cam_ir_led_stop_dev(ictrl);
		if (rc)
			CAM_ERR(CAM_IR_LED, "Stop Failed rc: %d", rc);
	}

	rc = cam_ir_led_release_dev(ictrl);
	if (rc)
		CAM_ERR(CAM_IR_LED, "Release failed rc: %d", rc);
	else
		ictrl->ir_led_state = CAM_IR_LED_STATE_INIT;
}
