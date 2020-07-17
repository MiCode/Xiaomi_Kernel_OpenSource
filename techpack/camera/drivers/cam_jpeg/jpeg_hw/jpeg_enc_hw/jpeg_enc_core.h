/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_JPEG_ENC_CORE_H
#define CAM_JPEG_ENC_CORE_H

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "cam_jpeg_hw_intf.h"

struct cam_jpeg_enc_reg_offsets {
	uint32_t hw_version;
	uint32_t int_status;
	uint32_t int_clr;
	uint32_t int_mask;
	uint32_t hw_cmd;
	uint32_t reset_cmd;
	uint32_t encode_size;
};

struct cam_jpeg_enc_regval {
	uint32_t int_clr_clearall;
	uint32_t int_mask_disable_all;
	uint32_t int_mask_enable_all;
	uint32_t hw_cmd_start;
	uint32_t reset_cmd;
	uint32_t hw_cmd_stop;
};

struct cam_jpeg_enc_int_status {
	uint32_t framedone;
	uint32_t resetdone;
	uint32_t iserror;
	uint32_t stopdone;
};

struct cam_jpeg_enc_device_hw_info {
	struct cam_jpeg_enc_reg_offsets reg_offset;
	struct cam_jpeg_enc_regval reg_val;
	struct cam_jpeg_enc_int_status int_status;
};

enum cam_jpeg_enc_core_state {
	CAM_JPEG_ENC_CORE_NOT_READY,
	CAM_JPEG_ENC_CORE_READY,
	CAM_JPEG_ENC_CORE_RESETTING,
	CAM_JPEG_ENC_CORE_ABORTING,
	CAM_JPEG_ENC_CORE_STATE_MAX,
};

struct cam_jpeg_enc_device_core_info {
	enum cam_jpeg_enc_core_state core_state;
	struct cam_jpeg_enc_device_hw_info *jpeg_enc_hw_info;
	uint32_t cpas_handle;
	struct cam_jpeg_set_irq_cb irq_cb;
	int32_t ref_count;
	struct mutex core_mutex;
};

int cam_jpeg_enc_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_jpeg_enc_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_jpeg_enc_start_hw(void *device_priv,
	void *start_hw_args, uint32_t arg_size);
int cam_jpeg_enc_stop_hw(void *device_priv,
	void *stop_hw_args, uint32_t arg_size);
int cam_jpeg_enc_reset_hw(void *device_priv,
	void *reset_hw_args, uint32_t arg_size);
int cam_jpeg_enc_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);
irqreturn_t cam_jpeg_enc_irq(int irq_num, void *data);

#endif /* CAM_JPEG_ENC_CORE_H */
