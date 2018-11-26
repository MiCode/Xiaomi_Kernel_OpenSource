/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef _CAM_SENSOR_CORE_H_
#define _CAM_SENSOR_CORE_H_

#include "cam_sensor_dev.h"

#define MV_TEMP_SET 1

/**
 * @s_ctrl: Sensor ctrl structure
 *
 * This API powers up the camera sensor module
 */
int cam_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl);

/**
 * @s_ctrl: Sensor ctrl structure
 *
 * This API powers down the camera sensor module
 */
int cam_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl);

/**
 * @sd: V4L2 subdevice
 * @on: Turn off/on flag
 *
 * This API powers down the sensor module
 */
int cam_sensor_power(struct v4l2_subdev *sd, int on);

/**
 * @s_ctrl: Sensor ctrl structure
 * @req_id: Request id
 * @opcode: opcode for settings
 *
 * This API applies the req_id settings to sensor
 */
int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl, int64_t req_id,
	enum cam_sensor_packet_opcodes opcode);

/**
 * @apply: Req mgr structure for applying request
 *
 * This API applies the request that is mentioned
 */
int cam_sensor_apply_request(struct cam_req_mgr_apply_request *apply);

/**
 * @flush: Req mgr structure for flushing request
 *
 * This API flushes the request that is mentioned
 */
int cam_sensor_flush_request(struct cam_req_mgr_flush_request *flush);

/**
 * @info: Sub device info to req mgr
 *
 * Publish the subdevice info
 */
int cam_sensor_publish_dev_info(struct cam_req_mgr_device_info *info);

/**
 * @link: Link setup info
 *
 * This API establishes link with sensor subdevice with req mgr
 */
int cam_sensor_establish_link(struct cam_req_mgr_core_dev_link_setup *link);

/**
 * @s_ctrl: Sensor ctrl structure
 * @arg:    Camera control command argument
 *
 * This API handles the camera control argument reached to sensor
 */
int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl, void *arg);

/**
 * @s_ctrl: Sensor ctrl structure
 *
 * This API handles the camera sensor close/shutdown
 */
void cam_sensor_shutdown(struct cam_sensor_ctrl_t *s_ctrl);

#endif /* _CAM_SENSOR_CORE_H_ */
