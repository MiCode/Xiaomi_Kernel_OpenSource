/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HDMI_UTIL_H_
#define _SDE_HDMI_UTIL_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/msm_ext_display.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "hdmi.h"
#include "sde_kms.h"
#include "sde_connector.h"
#include "msm_drv.h"
#include "sde_hdmi_regs.h"

#ifdef HDMI_UTIL_DEBUG_ENABLE
#define HDMI_UTIL_DEBUG(fmt, args...)   SDE_ERROR(fmt, ##args)
#else
#define HDMI_UTIL_DEBUG(fmt, args...)   SDE_DEBUG(fmt, ##args)
#endif

#define HDMI_UTIL_ERROR(fmt, args...)   SDE_ERROR(fmt, ##args)

struct sde_hdmi_tx_ddc_data {
	char *what;
	u8 *data_buf;
	u32 data_len;
	u32 dev_addr;
	u32 offset;
	u32 request_len;
	u32 retry_align;
	u32 hard_timeout;
	u32 timeout_left;
	int retry;
};

struct sde_hdmi_tx_ddc_ctrl {
	atomic_t rxstatus_busy_wait_done;
	struct dss_io_data *io;
	struct sde_hdmi_tx_ddc_data ddc_data;
};

/* DDC */
int sde_hdmi_ddc_write(void *cb_data);
int sde_hdmi_ddc_read(void *cb_data);

#endif /* _SDE_HDMI_UTIL_H_ */
