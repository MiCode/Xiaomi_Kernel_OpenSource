/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef VCAP_VC_H
#define VCAP_VC_H

#include <linux/interrupt.h>

#include <media/vcap_v4l2.h>

#define VCAP_HARDWARE_VERSION 0x10000000

#define VCAP_HARDWARE_VERSION_REG (VCAP_BASE + 0x0000)

#define VCAP_VC_CTRL (VCAP_BASE + 0x0800)
#define VCAP_VC_NPL_CTRL (VCAP_BASE + 0x0804)
#define VCAP_VC_POLARITY (VCAP_BASE + 0x081c)
#define VCAP_VC_V_H_TOTAL (VCAP_BASE + 0x0820)
#define VCAP_VC_H_ACTIVE (VCAP_BASE + 0x0824)
#define VCAP_VC_V_ACTIVE (VCAP_BASE + 0x0828)
#define VCAP_VC_V_ACTIVE_F2 (VCAP_BASE + 0x0830)
#define VCAP_VC_VSYNC_VPOS (VCAP_BASE + 0x0834)
#define VCAP_VC_VSYNC_F2_VPOS (VCAP_BASE + 0x0838)
#define VCAP_VC_HSYNC_HPOS (VCAP_BASE + 0x0840)
#define VCAP_VC_VSYNC_F2_HPOS (VCAP_BASE + 0x083c)
#define VCAP_VC_BUF_CTRL (VCAP_BASE + 0x0848)

#define VCAP_VC_Y_STRIDE (VCAP_BASE + 0x084c)
#define VCAP_VC_C_STRIDE (VCAP_BASE + 0x0850)

#define VCAP_VC_Y_ADDR_1 (VCAP_BASE + 0x0854)
#define VCAP_VC_C_ADDR_1 (VCAP_BASE + 0x0858)
#define VCAP_VC_Y_ADDR_2 (VCAP_BASE + 0x085c)
#define VCAP_VC_C_ADDR_2 (VCAP_BASE + 0x0860)
#define VCAP_VC_Y_ADDR_3 (VCAP_BASE + 0x0864)
#define VCAP_VC_C_ADDR_3 (VCAP_BASE + 0x0868)
#define VCAP_VC_Y_ADDR_4 (VCAP_BASE + 0x086c)
#define VCAP_VC_C_ADDR_4 (VCAP_BASE + 0x0870)
#define VCAP_VC_Y_ADDR_5 (VCAP_BASE + 0x0874)
#define VCAP_VC_C_ADDR_5 (VCAP_BASE + 0x0878)
#define VCAP_VC_Y_ADDR_6 (VCAP_BASE + 0x087c)
#define VCAP_VC_C_ADDR_6 (VCAP_BASE + 0x0880)

#define VCAP_VC_IN_CTRL1 (VCAP_BASE + 0x0808)
#define VCAP_VC_IN_CTRL2 (VCAP_BASE + 0x080c)
#define VCAP_VC_IN_CTRL3 (VCAP_BASE + 0x0810)
#define VCAP_VC_IN_CTRL4 (VCAP_BASE + 0x0814)
#define VCAP_VC_IN_CTRL5 (VCAP_BASE + 0x0818)

#define VCAP_VC_INT_MASK (VCAP_BASE + 0x0884)
#define VCAP_VC_INT_CLEAR (VCAP_BASE + 0x0888)
#define VCAP_VC_INT_STATUS (VCAP_BASE + 0x088c)
#define VCAP_VC_TIMESTAMP (VCAP_BASE + 0x0034)

#define VC_BUFFER_WRITTEN (0x3 << 1)
#define VC_BUFFER_MASK 0x7E
#define VC_ERR_MASK 0xE0001E00
#define VC_VSYNC_MASK 0x1

int vc_start_capture(struct vcap_client_data *c_data);
int vc_hw_kick_off(struct vcap_client_data *c_data);
void vc_stop_capture(struct vcap_client_data *c_data);
int config_vc_format(struct vcap_client_data *c_data);
int detect_vc(struct vcap_dev *dev);
int deinit_vc(void);
irqreturn_t vc_handler(struct vcap_dev *dev);
#endif
