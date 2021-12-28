/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2021 The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_JPEG_ENC_SOC_H_
#define _CAM_JPEG_ENC_SOC_H_

#include "cam_soc_util.h"
#include "cam_jpeg_hw_intf.h"

/*
 * struct cam_jpeg_enc_soc_private:
 *
 * @Brief:                   Private SOC data specific to JPEG ENC Driver
 *
 * @num_pid                  JPEG number of pids
 * @pid:                     JPEG enc pid value list
 * @rd_mid:                  JPEG enc inport read mid value
 * @wr_mid:                  JPEG enc outport write mid value
 */
struct cam_jpeg_enc_soc_private {
	uint32_t    num_pid;
	uint32_t    pid[CAM_JPEG_HW_MAX_NUM_PID];
	uint32_t    rd_mid;
	uint32_t    wr_mid;
};

int cam_jpeg_enc_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t jpeg_enc_irq_handler, void *irq_data);

int cam_jpeg_enc_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_jpeg_enc_disable_soc_resources(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_JPEG_ENC_SOC_H_*/
