/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_REQ_MGR_INTERFACE_H
#define _CAM_REQ_MGR_INTERFACE_H

#include <linux/types.h>
#include <media/cam_req_mgr.h>
#include "cam_req_mgr_core_defs.h"
#include "cam_req_mgr_util.h"

struct cam_req_mgr_trigger_notify;
struct cam_req_mgr_error_notify;
struct cam_req_mgr_add_request;
struct cam_req_mgr_device_info;
struct cam_req_mgr_core_dev_link_setup;
struct cam_req_mgr_apply_request;
struct cam_req_mgr_flush_request;
struct cam_req_mgr_link_evt_data;

/* Request Manager -- camera device driver interface */
/**
 * @brief: camera kernel drivers to cam req mgr communication
 *
 * @cam_req_mgr_notify_trigger: for device which generates trigger to inform CRM
 * @cam_req_mgr_notify_err    : device use this to inform about different errors
 * @cam_req_mgr_add_req       : to info CRm about new rqeuest received from
 *                              userspace
 */
typedef int (*cam_req_mgr_notify_trigger)(
	struct cam_req_mgr_trigger_notify *);
typedef int (*cam_req_mgr_notify_err)(struct cam_req_mgr_error_notify *);
typedef int (*cam_req_mgr_add_req)(struct cam_req_mgr_add_request *);

/**
 * @brief: cam req mgr to camera device drivers
 *
 * @cam_req_mgr_get_dev_info: to fetch details about device linked
 * @cam_req_mgr_link_setup  : to establish link with device for a session
 * @cam_req_mgr_notify_err  : to broadcast error happened on link for request id
 * @cam_req_mgr_apply_req   : CRM asks device to apply certain request id.
 * @cam_req_mgr_flush_req   : Flush or cancle request
 * cam_req_mgr_process_evt  : generic events
 */
typedef int (*cam_req_mgr_get_dev_info) (struct cam_req_mgr_device_info *);
typedef int (*cam_req_mgr_link_setup)(
	struct cam_req_mgr_core_dev_link_setup *);
typedef int (*cam_req_mgr_apply_req)(struct cam_req_mgr_apply_request *);
typedef int (*cam_req_mgr_flush_req)(struct cam_req_mgr_flush_request *);
typedef int (*cam_req_mgr_process_evt)(struct cam_req_mgr_link_evt_data *);

/**
 * @brief          : cam_req_mgr_crm_cb - func table
 *
 * @notify_trigger : payload for trigger indication event
 * @notify_err     : payload for different error occurred at device
 * @add_req        : payload to inform which device and what request is received
 */
struct cam_req_mgr_crm_cb {
	cam_req_mgr_notify_trigger  notify_trigger;
	cam_req_mgr_notify_err      notify_err;
	cam_req_mgr_add_req         add_req;
};

/**
 * @brief        : cam_req_mgr_kmd_ops - func table
 *
 * @get_dev_info : payload to fetch device details
 * @link_setup   : payload to establish link with device
 * @apply_req    : payload to apply request id on a device linked
 * @flush_req    : payload to flush request
 * @process_evt  : payload to generic event
 */
struct cam_req_mgr_kmd_ops {
	cam_req_mgr_get_dev_info     get_dev_info;
	cam_req_mgr_link_setup       link_setup;
	cam_req_mgr_apply_req        apply_req;
	cam_req_mgr_flush_req        flush_req;
	cam_req_mgr_process_evt      process_evt;
};

/**
 * enum cam_pipeline_delay
 * @brief     : enumerator for different pipeline delays in camera
 *
 * @DELAY_0   : device processed settings on same frame
 * @DELAY_1   : device processed settings after 1 frame
 * @DELAY_2   : device processed settings after 2 frames
 * @DELAY_MAX : maximum supported pipeline delay
 */
enum cam_pipeline_delay {
	CAM_PIPELINE_DELAY_0,
	CAM_PIPELINE_DELAY_1,
	CAM_PIPELINE_DELAY_2,
	CAM_PIPELINE_DELAY_MAX,
};

/**
 * @CAM_TRIGGER_POINT_SOF   : Trigger point for SOF
 * @CAM_TRIGGER_POINT_EOF   : Trigger point for EOF
 */
#define CAM_TRIGGER_POINT_SOF     (1 << 0)
#define CAM_TRIGGER_POINT_EOF     (1 << 1)

/**
 * enum cam_req_status
 * @brief   : enumerator for request status
 *
 * @SUCCESS : device processed settings successfully
 * @FAILED  : device processed settings failed
 * @MAX     : invalid status value
 */
enum cam_req_status {
	CAM_REQ_STATUS_SUCCESS,
	CAM_REQ_STATUS_FAILED,
	CAM_REQ_STATUS_MAX,
};

/**
 * enum cam_req_mgr_device_error
 * @brief      : enumerator for different errors occurred at device
 *
 * @NOT_FOUND  : settings asked by request manager is not found
 * @BUBBLE     : device hit timing issue and is able to recover
 * @FATAL      : device is in bad shape and can not recover from error
 * @PAGE_FAULT : Page fault while accessing memory
 * @OVERFLOW   : Bus Overflow for IFE/VFE
 * @TIMEOUT    : Timeout from cci or bus.
 * @MAX        : Invalid error value
 */
enum cam_req_mgr_device_error {
	CRM_KMD_ERR_NOT_FOUND,
	CRM_KMD_ERR_BUBBLE,
	CRM_KMD_ERR_FATAL,
	CRM_KMD_ERR_PAGE_FAULT,
	CRM_KMD_ERR_OVERFLOW,
	CRM_KMD_ERR_TIMEOUT,
	CRM_KMD_ERR_MAX,
};

/**
 * enum cam_req_mgr_device_id
 * @brief       : enumerator for different devices in subsystem
 *
 * @CAM_REQ_MGR : request manager itself
 * @SENSOR      : sensor device
 * @FLASH       : LED flash or dual LED device
 * @ACTUATOR    : lens mover
 * @IFE         : Image processing device
 * @EXTERNAL_1  : third party device
 * @EXTERNAL_2  : third party device
 * @EXTERNAL_3  : third party device
 * @MAX         : invalid device id
 */
enum cam_req_mgr_device_id {
	CAM_REQ_MGR_DEVICE,
	CAM_REQ_MGR_DEVICE_SENSOR,
	CAM_REQ_MGR_DEVICE_FLASH,
	CAM_REQ_MGR_DEVICE_ACTUATOR,
	CAM_REQ_MGR_DEVICE_IFE,
	CAM_REQ_MGR_DEVICE_EXTERNAL_1,
	CAM_REQ_MGR_DEVICE_EXTERNAL_2,
	CAM_REQ_MGR_DEVICE_EXTERNAL_3,
	CAM_REQ_MGR_DEVICE_ID_MAX,
};

/* Camera device driver to Req Mgr device interface */

/**
 * enum cam_req_mgr_link_evt_type
 * @CAM_REQ_MGR_LINK_EVT_ERR:
 * @CAM_REQ_MGR_LINK_EVT_MAX:
 */
enum cam_req_mgr_link_evt_type {
	CAM_REQ_MGR_LINK_EVT_ERR,
	CAM_REQ_MGR_LINK_EVT_PAUSE,
	CAM_REQ_MGR_LINK_EVT_RESUME,
	CAM_REQ_MGR_LINK_EVT_MAX,
};

/**
 * struct cam_req_mgr_trigger_notify
 * @link_hdl : link identifier
 * @dev_hdl  : device handle which has sent this req id
 * @frame_id : frame id for internal tracking
 * @trigger  : trigger point of this notification, CRM will send apply
 * only to the devices which subscribe to this point.
 */
struct cam_req_mgr_trigger_notify {
	int32_t  link_hdl;
	int32_t  dev_hdl;
	int64_t  frame_id;
	uint32_t trigger;
};

/**
 * struct cam_req_mgr_error_notify
 * @link_hdl : link identifier
 * @dev_hdl  : device handle which has sent this req id
 * @req_id   : req id which hit error
 * @error    : what error device hit while processing this req
 */
struct cam_req_mgr_error_notify {
	int32_t  link_hdl;
	int32_t  dev_hdl;
	uint64_t req_id;
	enum cam_req_mgr_device_error error;
};

/**
 * struct cam_req_mgr_add_request
 * @link_hdl             : link identifier
 * @dev_hdl              : device handle which has sent this req id
 * @req_id               : req id which device is ready to process
 * @skip_before_applying : before applying req mgr introduce bubble
 *                         by not sending request to device/s.
 *                         ex: IFE and Flash
 */
struct cam_req_mgr_add_request {
	int32_t  link_hdl;
	int32_t  dev_hdl;
	uint64_t req_id;
	uint32_t skip_before_applying;
};


/* CRM to KMD devices */
/**
 * struct cam_req_mgr_device_info
 * @dev_hdl : Input_param : device handle for reference
 * @name    : link link or unlink
 * @dev_id  : device id info
 * @p_delay : delay between time settings applied and take effect
 * @trigger : Trigger point for the client
 *
 */
struct cam_req_mgr_device_info {
	int32_t                     dev_hdl;
	char                        name[256];
	enum cam_req_mgr_device_id  dev_id;
	enum cam_pipeline_delay     p_delay;
	uint32_t                    trigger;
};

/**
 * struct cam_req_mgr_core_dev_link_setup
 * @link_enable     : link link or unlink
 * @link_hdl        : link identifier
 * @dev_hdl         : device handle for reference
 * @max_delay       : max pipeline delay on this link
 * @crm_cb          : callback funcs to communicate with req mgr
 * @subscribe_event : the mask of trigger points this link subscribes
 *
 */
struct cam_req_mgr_core_dev_link_setup {
	int32_t                    link_enable;
	int32_t                    link_hdl;
	int32_t                    dev_hdl;
	enum cam_pipeline_delay    max_delay;
	struct cam_req_mgr_crm_cb *crm_cb;
	uint32_t                   subscribe_event;
};

/**
 * struct cam_req_mgr_apply_request
 * @link_hdl         : link identifier
 * @dev_hdl          : device handle for cross check
 * @request_id       : request id settings to apply
 * @report_if_bubble : report to crm if failure in applying
 * @trigger_point    : the trigger point of this apply
 *
 */
struct cam_req_mgr_apply_request {
	int32_t    link_hdl;
	int32_t    dev_hdl;
	uint64_t   request_id;
	int32_t    report_if_bubble;
	uint32_t   trigger_point;
};

/**
 * struct cam_req_mgr_flush_request
 * @link_hdl    : link identifier
 * @dev_hdl     : device handle for cross check
 * @type        : cancel request type flush all or a request
 * @request_id  : request id to cancel
 *
 */
struct cam_req_mgr_flush_request {
	int32_t     link_hdl;
	int32_t     dev_hdl;
	uint32_t    type;
	uint64_t    req_id;
};

/**
 * struct cam_req_mgr_event_data
 * @link_hdl : link handle
 * @req_id   : request id
 *
 */
struct cam_req_mgr_link_evt_data {
	int32_t  link_hdl;
	int32_t  dev_hdl;
	uint64_t req_id;

	enum cam_req_mgr_link_evt_type evt_type;
	union {
		enum cam_req_mgr_device_error error;
	} u;
};

/**
 * struct cam_req_mgr_send_request
 * @link_hdl   : link identifier
 * @idx        : slot idx
 *
 */
struct cam_req_mgr_send_request {
	int32_t    link_hdl;
	struct cam_req_mgr_req_queue *in_q;
};
#endif
