/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TFE_IRQ_H_
#define _CAM_TFE_IRQ_H_

#include <linux/platform_device.h>

#define CAM_TFE_TOP_IRQ_REG_NUM 3

/*
 * cam_tfe_irq_config()
 *
 * @brief:                   Tfe hw irq configuration
 *
 * @tfe_core_data:           tfe core pointer
 * @irq_mask:                Irq mask for enable interrupts or disable
 * @num_reg:                 Number irq mask registers
 * @enable:                  enable = 1, enable the given irq mask interrupts
 *                           enable = 0 disable the given irq mask interrupts
 *
 * @Return:                  0: Success
 *                           Non-zero: Failure
 */
int cam_tfe_irq_config(void     *tfe_core_data,
	uint32_t  *irq_mask, uint32_t num_reg, bool enable);


#endif /* _CAM_TFE_IRQ_H_ */
