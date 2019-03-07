/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_FLASH_CORE_H_
#define _CAM_FLASH_CORE_H_

#include <linux/leds-qpnp-flash.h>
#include <media/cam_sensor.h>
#include "cam_flash_dev.h"
#include "cam_sync_api.h"
#include "cam_mem_mgr_api.h"

int cam_flash_parser(struct cam_flash_ctrl *fctrl, void *arg);
int cam_flash_publish_dev_info(struct cam_req_mgr_device_info *info);
int cam_flash_establish_link(struct cam_req_mgr_core_dev_link_setup *link);
int cam_flash_apply_setting(struct cam_flash_ctrl *fctrl, uint64_t req_id);
int cam_flash_apply_request(struct cam_req_mgr_apply_request *apply);
int cam_flash_process_evt(struct cam_req_mgr_link_evt_data *event_data);
int cam_flash_flush_request(struct cam_req_mgr_flush_request *flush);
int cam_flash_off(struct cam_flash_ctrl *fctrl);
int cam_flash_prepare(struct cam_flash_ctrl *flash_ctrl,
	bool regulator_enable);
void cam_flash_shutdown(struct cam_flash_ctrl *flash_ctrl);
int cam_flash_stop_dev(struct cam_flash_ctrl *flash_ctrl);
int cam_flash_release_dev(struct cam_flash_ctrl *fctrl);
#endif /*_CAM_FLASH_CORE_H_*/
