/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ICP_UTILS_H_
#define _CAM_ICP_UTILS_H_

#include <linux/firmware.h>
#include <linux/elf.h>
#include <linux/iopoll.h>

#include "cam_debug_util.h"

/**
 * @brief : Validate FW elf image
 */
int32_t cam_icp_validate_fw(const uint8_t *elf, uint32_t machine_id);

/**
 * @brief : Get FW elf size
 */
int32_t cam_icp_get_fw_size(const uint8_t *elf, uint32_t *fw_size);

/**
 * @brief : Program FW memory
 */
int32_t cam_icp_program_fw(const uint8_t *elf,
	uintptr_t fw_kva_addr);

#endif /* _CAM_ICP_UTILS_H_ */
