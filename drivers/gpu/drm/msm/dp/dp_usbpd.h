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
 * orientation: plug orientation configuration
 * @low_pow_st: low power state
 * @adaptor_dp_en: adaptor functionality enabled
 * @multi_func: multi-function preferred
 * @usb_config_req: request to switch to usb
 * @exit_dp_mode: request exit from displayport mode
 * @hpd_high: Hot Plug Detect signal is high.
 * @hpd_irq: Change in the status since last message
 * @alt_mode_cfg_done: bool to specify alt mode status
 * @debug_en: bool to specify debug mode
 * @simulate_connect: simulate disconnect or connect for debug mode
 * @simulate_attention: simulate attention messages for debug mode
 */
struct dp_usbpd {
	enum dp_usbpd_port port;
	enum plug_orientation orientation;
	bool low_pow_st;
	bool adaptor_dp_en;
	bool multi_func;
	bool usb_config_req;
	bool exit_dp_mode;
	bool hpd_high;
	bool hpd_irq;
	bool alt_mode_cfg_done;
	bool debug_en;

	int (*simulate_connect)(struct dp_usbpd *dp_usbpd, bool hpd);
	int (*simulate_attention)(struct dp_usbpd *dp_usbpd, int vdo);
};

/**
 * struct dp_usbpd_cb - callback functions provided by the client
 *
 * @configure: called by usbpd module when PD communication has
 * been completed and the usb peripheral has been configured on
 * dp mode.
 * @disconnect: notify the cable disconnect issued by usb.
 * @attention: notify any attention message issued by usb.
 */
struct dp_usbpd_cb {
	int (*configure)(struct device *dev);
	int (*disconnect)(struct device *dev);
	int (*attention)(struct device *dev);
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
struct dp_usbpd *dp_usbpd_get(struct device *dev, struct dp_usbpd_cb *cb);

void dp_usbpd_put(struct dp_usbpd *pd);
#endif /* _DP_USBPD_H_ */
