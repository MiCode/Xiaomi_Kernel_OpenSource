/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ISPV3_CORE_H_
#define _CAM_ISPV3_CORE_H_

#include "cam_ispv3_dev.h"

/**
 * @sd: V4L2 subdevice
 * @on: Turn off/on flag
 *
 * This API powers down the sensor module
 */
int cam_ispv3_power(struct v4l2_subdev *sd, int on);

/**
 * @apply: Req mgr structure for applying request
 *
 * This API applies the request that is mentioned
 */
int cam_ispv3_apply_request(struct cam_req_mgr_apply_request *apply);

/**
 * @flush: Req mgr structure for flushing request
 *
 * This API flushes the request that is mentioned
 */
int cam_ispv3_flush_request(struct cam_req_mgr_flush_request *flush);

/**
 * @info: Sub device info to req mgr
 *
 * Publish the subdevice info
 */
int cam_ispv3_publish_dev_info(struct cam_req_mgr_device_info *info);

/**
 * @link: Link setup info
 *
 * This API establishes link with sensor subdevice with req mgr
 */
int cam_ispv3_establish_link(struct cam_req_mgr_core_dev_link_setup *link);

/**
 * @s_ctrl: Sensor ctrl structure
 * @arg:    Camera control command argument
 *
 * This API handles the camera control argument reached to sensor
 */
int32_t cam_ispv3_driver_cmd(struct cam_ispv3_ctrl_t *s_ctrl, void *arg);

/**
 * @s_ctrl: Sensor ctrl structure
 *
 * This API handles the camera sensor close/shutdown
 */
void cam_ispv3_shutdown(struct cam_ispv3_ctrl_t *s_ctrl);

#endif /* _CAM_ISPV3_CORE_H_ */
