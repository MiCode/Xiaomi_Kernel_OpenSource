/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_ACTUATOR_CORE_H_
#define _CAM_ACTUATOR_CORE_H_

#include "cam_actuator_dev.h"

/**
 * @power_info: power setting info to control the power
 *
 * This API construct the default actuator power setting.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int32_t cam_actuator_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info);

/**
 * @apply: Req mgr structure for applying request
 *
 * This API applies the request that is mentioned
 */
int32_t cam_actuator_apply_request(struct cam_req_mgr_apply_request *apply);

/**
 * @info: Sub device info to req mgr
 *
 * This API publish the subdevice info to req mgr
 */
int32_t cam_actuator_publish_dev_info(struct cam_req_mgr_device_info *info);

/**
 * @flush: Req mgr structure for flushing request
 *
 * This API flushes the request that is mentioned
 */
int cam_actuator_flush_request(struct cam_req_mgr_flush_request *flush);


/**
 * @link: Link setup info
 *
 * This API establishes link actuator subdevice with req mgr
 */
int32_t cam_actuator_establish_link(
	struct cam_req_mgr_core_dev_link_setup *link);

/**
 * @a_ctrl: Actuator ctrl structure
 * @arg:    Camera control command argument
 *
 * This API handles the camera control argument reached to actuator
 */
int32_t cam_actuator_driver_cmd(struct cam_actuator_ctrl_t *a_ctrl, void *arg);

/**
 * @a_ctrl: Actuator ctrl structure
 *
 * This API handles the shutdown ioctl/close
 */
void cam_actuator_shutdown(struct cam_actuator_ctrl_t *a_ctrl);

#endif /* _CAM_ACTUATOR_CORE_H_ */
