/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#ifndef _DP_USBPD_H_
#define _DP_USBPD_H_

#include <linux/usb/usbpd.h>

#include <linux/types.h>
#include <linux/device.h>
#include "dp_hpd.h"

/**
 * enum dp_usbpd_port - usb/dp port type
 * @DP_USBPD_PORT_NONE: port not configured
 * @DP_USBPD_PORT_UFP_D: Upstream Facing Port - DisplayPort
 * @DP_USBPD_PORT_DFP_D: Downstream Facing Port - DisplayPort
 * @DP_USBPD_PORT_D_UFP_D: Both UFP & DFP - DisplayPort
 */

enum dp_usbpd_port {
	DP_USBPD_PORT_NONE,
	DP_USBPD_PORT_UFP_D,
	DP_USBPD_PORT_DFP_D,
	DP_USBPD_PORT_D_UFP_D,
};

/**
 * struct dp_usbpd - DisplayPort status
 *
 * @port: port configured
 * @low_pow_st: low power state
 * @adaptor_dp_en: adaptor functionality enabled
 * @usb_config_req: request to switch to usb
 * @exit_dp_mode: request exit from displayport mode
 * @debug_en: bool to specify debug mode
 */
struct dp_usbpd {
	struct dp_hpd base;
	enum dp_usbpd_port port;
	bool low_pow_st;
	bool adaptor_dp_en;
	bool usb_config_req;
	bool exit_dp_mode;
	bool debug_en;
};

/**
 * dp_usbpd_get() - setup usbpd module
 *
 * @dev: device instance of the caller
 * @cb: struct containing callback function pointers.
 *
 * This function allows the client to initialize the usbpd
 * module. The module will communicate with usb driver and
 * handles the power delivery (PD) communication with the
 * sink/usb device. This module will notify the client using
 * the callback functions about the connection and status.
 */
struct dp_hpd *dp_usbpd_get(struct device *dev, struct dp_hpd_cb *cb);

void dp_usbpd_put(struct dp_hpd *pd);
#endif /* _DP_USBPD_H_ */
