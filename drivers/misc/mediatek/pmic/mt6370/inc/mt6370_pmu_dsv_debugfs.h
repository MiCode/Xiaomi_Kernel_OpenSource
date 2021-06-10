/*
 *  include header file for MediaTek MT6370 Display Bias Debugfs
 *
 *  Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __MT6370_PMU_DSV_DEBUGFS_H
#define __MT6370_PMU_DSV_DEBUGFS_H



enum dsv_dbg_mode_t {
	DSV_VNEG_OCP,
	DSV_VPOS_OCP,
	DSV_BST_OCP,
	DSV_VNEG_SCP,
	DSV_VPOS_SCP,
	DSV_MODE_MAX,
};

enum {
	DSV_VAR_VBST_ADJUSTMENT = 0x1,
	DSV_VAR_IRQ_COUNT = 0x2,
	DSV_VAR_IRQ_MASK = 0x3,
	DSV_VAR_IRQ_MASK_WARNING = 0x4,
	DSV_VAR_IRQ_DISABLE = 0x5,
	DSV_VAR_MAX,
};




#endif /* #ifndef ____MT6370_PMU_DSV_DEBUGFS_H */
