/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
