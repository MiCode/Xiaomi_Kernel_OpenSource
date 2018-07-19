/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _CAM_FLASH_DEV_H_
#define _CAM_FLASH_DEV_H_

#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/leds-qpnp-flash.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/cam_sensor.h>
#include <media/cam_req_mgr.h>
#include "cam_req_mgr_util.h"
#include "cam_req_mgr_interface.h"
#include "cam_subdev.h"
#include "cam_mem_mgr.h"
#include "cam_sensor_cmn_header.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"

#define CAMX_FLASH_DEV_NAME "cam-flash-dev"

#define CAM_FLASH_PIPELINE_DELAY 1

#define CAM_FLASH_PACKET_OPCODE_INIT                 0
#define CAM_FLASH_PACKET_OPCODE_SET_OPS              1
#define CAM_FLASH_PACKET_OPCODE_NON_REALTIME_SET_OPS 2

enum cam_flash_switch_trigger_ops {
	LED_SWITCH_OFF = 0,
	LED_SWITCH_ON,
};

enum cam_flash_state {
	CAM_FLASH_STATE_INIT,
	CAM_FLASH_STATE_ACQUIRE,
	CAM_FLASH_STATE_CONFIG,
	CAM_FLASH_STATE_START,
};

/**
 * struct cam_flash_intf_params
 * @device_hdl   : Device Handle
 * @session_hdl  : Session Handle
 * @link_hdl     : Link Handle
 * @ops          : KMD operations
 * @crm_cb       : Callback API pointers
 */
struct cam_flash_intf_params {
	int32_t                     device_hdl;
	int32_t                     session_hdl;
	int32_t                     link_hdl;
	struct cam_req_mgr_kmd_ops  ops;
	struct cam_req_mgr_crm_cb  *crm_cb;
};

/**
 * struct cam_flash_common_attr
 * @is_settings_valid  : Notify the valid settings
 * @request_id         : Request id provided by umd
 * @count              : Number of led count
 * @cmd_type           : Command buffer type
 */
struct cam_flash_common_attr {
	bool      is_settings_valid;
	uint64_t  request_id;
	uint16_t  count;
	uint8_t   cmd_type;
};

/**
 * struct flash_init_packet
 * @cmn_attr   : Provides common attributes
 * @flash_type : Flash type(PMIC/I2C/GPIO)
 */
struct cam_flash_init_packet {
	struct cam_flash_common_attr  cmn_attr;
	uint8_t                       flash_type;
};

/**
 * struct flash_frame_setting
 * @cmn_attr         : Provides common attributes
 * @num_iterations   : Iterations used to perform RER
 * @led_on_delay_ms  : LED on time in milisec
 * @led_off_delay_ms : LED off time in milisec
 * @opcode           : Command buffer opcode
 * @led_current_ma[] : LED current array in miliamps
 *
 */
struct cam_flash_frame_setting {
	struct cam_flash_common_attr cmn_attr;
	uint16_t                     num_iterations;
	uint16_t                     led_on_delay_ms;
	uint16_t                     led_off_delay_ms;
	int8_t                       opcode;
	uint32_t                     led_current_ma[CAM_FLASH_MAX_LED_TRIGGERS];
};

/**
 *  struct cam_flash_private_soc
 * @switch_trigger_name : Switch trigger name
 * @flash_trigger_name  : Flash trigger name array
 * @flash_op_current    : Flash operational current
 * @flash_max_current   : Max supported current for LED in flash mode
 * @flash_max_duration  : Max turn on duration for LED in Flash mode
 * @torch_trigger_name  : Torch trigger name array
 * @torch_op_current    : Torch operational current
 * @torch_max_current   : Max supported current for LED in torch mode
 * @is_wled_flash       : Detection between WLED/LED flash
 */

struct cam_flash_private_soc {
	const char   *switch_trigger_name;
	const char   *flash_trigger_name[CAM_FLASH_MAX_LED_TRIGGERS];
	uint32_t     flash_op_current[CAM_FLASH_MAX_LED_TRIGGERS];
	uint32_t     flash_max_current[CAM_FLASH_MAX_LED_TRIGGERS];
	uint32_t     flash_max_duration[CAM_FLASH_MAX_LED_TRIGGERS];
	const char   *torch_trigger_name[CAM_FLASH_MAX_LED_TRIGGERS];
	uint32_t     torch_op_current[CAM_FLASH_MAX_LED_TRIGGERS];
	uint32_t     torch_max_current[CAM_FLASH_MAX_LED_TRIGGERS];
	bool         is_wled_flash;
};

/**
 *  struct cam_flash_ctrl
 * @soc_info            : Soc related information
 * @pdev                : Platform device
 * @per_frame[]         : Per_frame setting array
 * @nrt_info            : NonRealTime settings
 * @of_node             : Of Node ptr
 * @v4l2_dev_str        : V4L2 device structure
 * @bridge_intf         : CRM interface
 * @flash_init_setting  : Init command buffer structure
 * @switch_trigger      : Switch trigger ptr
 * @flash_num_sources   : Number of flash sources
 * @torch_num_source    : Number of torch sources
 * @flash_mutex         : Mutex for flash operations
 * @flash_wq_mutex      : Mutex for flash apply setting
 * @flash_state         : Current flash state (LOW/OFF/ON/INIT)
 * @flash_type          : Flash types (PMIC/I2C/GPIO)
 * @is_regulator_enable : Regulator disable/enable notifier
 * @flash_trigger       : Flash trigger ptr
 * @torch_trigger       : Torch trigger ptr
 */
struct cam_flash_ctrl {
	struct cam_hw_soc_info          soc_info;
	struct platform_device         *pdev;
	struct cam_flash_frame_setting  per_frame[MAX_PER_FRAME_ARRAY];
	struct cam_flash_frame_setting  nrt_info;
	struct device_node             *of_node;
	struct cam_subdev               v4l2_dev_str;
	struct cam_flash_intf_params    bridge_intf;
	struct cam_flash_init_packet    flash_init_setting;
	struct led_trigger             *switch_trigger;
	uint32_t                        flash_num_sources;
	uint32_t                        torch_num_sources;
	struct mutex                    flash_mutex;
	struct mutex                    flash_wq_mutex;
	enum   cam_flash_state          flash_state;
	uint8_t                         flash_type;
	bool                            is_regulator_enabled;
	struct led_trigger           *flash_trigger[CAM_FLASH_MAX_LED_TRIGGERS];
	struct led_trigger           *torch_trigger[CAM_FLASH_MAX_LED_TRIGGERS];
};

#endif /*_CAM_FLASH_DEV_H_*/
