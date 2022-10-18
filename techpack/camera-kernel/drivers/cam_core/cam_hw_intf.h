/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_HW_INTF_H_
#define _CAM_HW_INTF_H_

#include <linux/types.h>
#include "cam_hw.h"

/*
 * This file declares Constants, Enums, Structures and APIs to be used as
 * Interface between HW driver and HW Manager.
 */

#define CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ 6

enum cam_clk_bw_state {
	CAM_CLK_BW_STATE_INIT,
	CAM_CLK_BW_STATE_UNCHANGED,
	CAM_CLK_BW_STATE_INCREASE,
	CAM_CLK_BW_STATE_DECREASE,
};

/**
 * struct cam_hw_ops - Hardware layer interface functions
 *
 * @get_hw_caps:           Function pointer for get hw caps
 * @init:                  Function poniter for initialize hardware
 * @deinit:                Function pointer for deinitialize hardware
 * @reset:                 Function pointer for reset hardware
 * @reserve:               Function pointer for reserve hardware
 * @release:               Function pointer for release hardware
 * @start:                 Function pointer for start hardware
 * @stop:                  Function pointer for stop hardware
 * @read:                  Function pointer for read hardware registers
 * @write:                 Function pointer for Write hardware registers
 * @process_cmd:           Function pointer for additional hardware controls
 * @flush_cmd:             Function pointer for flush requests
 *
 */
struct cam_hw_ops {
	int (*get_hw_caps)(void *hw_priv,
		void *get_hw_cap_args, uint32_t arg_size);
	int (*init)(void *hw_priv,
		void *init_hw_args, uint32_t arg_size);
	int (*deinit)(void *hw_priv,
		void *init_hw_args, uint32_t arg_size);
	int (*reset)(void *hw_priv,
		void *reset_core_args, uint32_t arg_size);
	int (*reserve)(void *hw_priv,
		void *reserve_args, uint32_t arg_size);
	int (*release)(void *hw_priv,
		void *release_args, uint32_t arg_size);
	int (*start)(void *hw_priv,
		void *start_args, uint32_t arg_size);
	int (*stop)(void *hw_priv,
		void *stop_args, uint32_t arg_size);
	int (*read)(void *hw_priv,
		void *read_args, uint32_t arg_size);
	int (*write)(void *hw_priv,
		void *write_args, uint32_t arg_size);
	int (*process_cmd)(void *hw_priv,
		uint32_t cmd_type, void *cmd_args, uint32_t arg_size);
	int (*flush)(void *hw_priv,
		void *flush_args, uint32_t arg_size);
};

/**
 * struct cam_hw_intf - Common hardware node
 *
 * @hw_type:               Hardware type
 * @hw_idx:                Hardware ID
 * @hw_ops:                Hardware interface function table
 * @hw_priv:               Private hardware node pointer
 *
 */
struct cam_hw_intf {
	uint32_t                     hw_type;
	uint32_t                     hw_idx;
	struct cam_hw_ops            hw_ops;
	void                        *hw_priv;
};

/* hardware event callback function type */
typedef int (*cam_hw_mgr_event_cb_func)(void *priv, uint32_t evt_id,
	void *evt_data);

#ifdef CONFIG_CAM_PRESIL
static inline void cam_hw_util_init_hw_lock(struct cam_hw_info *hw_info)
{
	mutex_init(&hw_info->presil_hw_lock);
}

static inline unsigned long cam_hw_util_hw_lock_irqsave(struct cam_hw_info *hw_info)
{
	mutex_lock(&hw_info->presil_hw_lock);

	return 0;
}

static inline void cam_hw_util_hw_unlock_irqrestore(struct cam_hw_info *hw_info,
	unsigned long flags)
{
	mutex_unlock(&hw_info->presil_hw_lock);
}

static inline void cam_hw_util_hw_lock(struct cam_hw_info *hw_info)
{
	mutex_lock(&hw_info->presil_hw_lock);
}

static inline void cam_hw_util_hw_unlock(struct cam_hw_info *hw_info)
{
	mutex_unlock(&hw_info->presil_hw_lock);
}
#else
static inline void cam_hw_util_init_hw_lock(struct cam_hw_info *hw_info)
{
	spin_lock_init(&hw_info->hw_lock);
}

static inline unsigned long cam_hw_util_hw_lock_irqsave(struct cam_hw_info *hw_info)
{
	unsigned long flags = 0;

	if (!in_irq())
		spin_lock_irqsave(&hw_info->hw_lock, flags);

	return flags;
}

static inline void cam_hw_util_hw_unlock_irqrestore(struct cam_hw_info *hw_info,
	unsigned long flags)
{
	if (!in_irq())
		spin_unlock_irqrestore(&hw_info->hw_lock, flags);
}

static inline void cam_hw_util_hw_lock(struct cam_hw_info *hw_info)
{
	spin_lock(&hw_info->hw_lock);
}

static inline void cam_hw_util_hw_unlock(struct cam_hw_info *hw_info)
{
	spin_unlock(&hw_info->hw_lock);
}
#endif /* CONFIG_CAM_PRESIL */

#endif /* _CAM_HW_INTF_H_ */
