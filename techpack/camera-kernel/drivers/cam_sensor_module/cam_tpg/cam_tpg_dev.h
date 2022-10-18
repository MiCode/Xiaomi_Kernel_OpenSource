/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __CAM_TPG_DEV_H__
#define __CAM_TPG_DEV_H__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_defs.h>
#include <cam_sensor_cmn_header.h>
#include <cam_req_mgr_interface.h>
#include <cam_subdev.h>
#include <cam_io_util.h>
#include <media/cam_defs.h>
#include "cam_debug_util.h"
#include "cam_context.h"
#include "cam_soc_util.h"
#include <cam_cpas_api.h>
#include "tpg_hw/tpg_hw.h"

#define CAMX_TPG_DEV_NAME "cam-tpg-driver"

struct cam_tpg_device;
struct tpg_hw;

struct cam_tpg_ioctl_ops {
	int (*acquire_dev)(struct cam_tpg_device *tpg_dev,
			struct cam_acquire_dev_cmd *cmd);
	int (*release_dev)(struct cam_tpg_device *tpg_dev,
			struct cam_release_dev_cmd *cmd);
	int (*config_dev)(struct cam_tpg_device *tpg_dev,
			struct cam_config_dev_cmd *cmd);
	int (*start_dev)(struct cam_tpg_device *tpg_dev,
			struct cam_start_stop_dev_cmd *cmd);
	int (*stop_dev)(struct cam_tpg_device *tpg_dev,
			struct cam_start_stop_dev_cmd *cmd);
	int (*flush_dev)(struct cam_tpg_device *tpg_dev,
			struct cam_flush_dev_cmd *cmd);
	int (*acquire_hw)(struct cam_tpg_device *tpg_dev, void *args);
	int (*release_hw)(struct cam_tpg_device *tpg_dev, void *args);
};

struct cam_tpg_crm_ops {
	int (*get_dev_info)(struct cam_tpg_device *tpg_dev,
			struct cam_req_mgr_device_info *device_info);
	int (*link)(struct cam_tpg_device *tpg_dev,
			struct cam_req_mgr_core_dev_link_setup *link);
	int (*unlink)(struct cam_tpg_device *tpg_dev,
			struct cam_req_mgr_core_dev_link_setup *unlink);
	int (*apply_req)(struct cam_tpg_device *tpg_dev,
			struct cam_req_mgr_apply_request *apply);
	int (*notify_frame_skip)(struct cam_tpg_device *tpg_dev,
			struct cam_req_mgr_apply_request *apply);
	int (*flush_req)(struct cam_tpg_device *tpg_dev,
			struct cam_req_mgr_flush_request *flush);
	int (*process_evt)(struct cam_tpg_device *tpg_dev,
			struct cam_req_mgr_link_evt_data *evt_data);
};

struct tpg_device_ops {
	struct cam_tpg_ioctl_ops ioctl_ops;
	struct cam_tpg_crm_ops   crm_ops;
};

enum cam_tpg_state {
	CAM_TPG_STATE_INIT,
	CAM_TPG_STATE_ACQUIRE,
	CAM_TPG_STATE_START,
	CAM_TPG_STATE_STATE_MAX
};

/**
 * struct tpg_crm_intf_params
 * @device_hdl: Device Handle
 * @session_hdl: Session Handle
 * @link_hdl: Link Handle
 * @ops: KMD operations
 * @crm_cb: Callback API pointers
 */
struct tpg_crm_intf_params {
	int32_t device_hdl;
	int32_t session_hdl;
	int32_t link_hdl;
	struct cam_req_mgr_kmd_ops ops;
	struct cam_req_mgr_crm_cb *crm_cb;
};

/**
 * cam_tpg_device
 *
 * @device_name: tpg device name
 * @mutex: mutex object for private use
 * @tpg_subdev: tpg subdevice instance
 * @soc_info: tpg soc infrastructure
 * @cpas_handle: cpas handle
 * @state_machine: tpg state machine
 * @crm_intf: crm interface
 * @tpg_hw  : tpg hw instance
 * @state   : state machine states
 * @slot_id : slot index of this tpg
 * @phy_id  : phy index mapped to this tpg
 */
struct cam_tpg_device {
	char device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct mutex mutex;
	struct cam_subdev tpg_subdev;
	struct cam_hw_soc_info soc_info;
	uint32_t cpas_handle;
	struct tpg_device_ops state_machine[CAM_TPG_STATE_STATE_MAX];
	struct tpg_crm_intf_params crm_intf;
	struct tpg_hw tpg_hw;
	int state;
	int slot_id;
	int phy_id;
};


/**
 * @brief : tpg module init
 *
 * @return : 0 on success
 */
int32_t cam_tpg_init_module(void);

/**
 * @brief : tpg module exit
 */
void cam_tpg_exit_module(void);

#endif
