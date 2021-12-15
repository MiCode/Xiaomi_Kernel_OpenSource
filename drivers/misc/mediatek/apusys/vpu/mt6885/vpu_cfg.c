// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "vpu_cfg.h"
#include "vpu_cmn.h"
#include "mtk_devinfo.h"
#include "vpu_debug.h"
#include <memory/mediatek/emi.h>

/**
 * vpu_is_disabled - enable/disable vpu from efuse
 * @vd: struct vpu_device to get the id
 *
 * return 1: this vd->id is disabled
 * return 0: this vd->id is enabled
 */
bool vpu_is_disabled(struct vpu_device *vd)
{
	bool ret;
	unsigned int efuse;
	unsigned int seg;
	unsigned int mask;

	mask = 1 << vd->id;

	seg = get_devinfo_with_index(EFUSE_SEG_OFFSET);
	efuse = get_devinfo_with_index(EFUSE_VPU_OFFSET);
	efuse = (efuse >> EFUSE_VPU_SHIFT) & EFUSE_VPU_MASK;
	/* disabled by mask, or disabled by segment */
	ret = (efuse & mask) || ((seg == 0x1) && (vd->id >= 2));

	/* show efuse info to let user know */
	pr_info("%s: seg: 0x%x, efuse: 0x%x, core%d is %s\n",
		__func__, seg, efuse, vd->id,
		ret ? "disabled" : "enabled");

	return ret;
}

