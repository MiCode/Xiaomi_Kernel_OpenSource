// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "vpu_cfg.h"
#include "vpu_cmn.h"
#include "mtk_devinfo.h"
#include "vpu_debug.h"
#include <memory/mediatek/emi.h>

void vpu_emi_mpu_set(unsigned long start, unsigned int size)
{
#ifdef CONFIG_MEDIATEK_EMI
	struct emimpu_region_t md_region;

	mtk_emimpu_init_region(&md_region, MPU_PROCT_REGION);
	mtk_emimpu_set_addr(&md_region, start,
			    (start + (unsigned long)size) - 0x1);
	mtk_emimpu_set_apc(&md_region, MPU_PROCT_D0_AP,
			   MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&md_region, MPU_PROCT_D5_APUSYS,
			   MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_lock_region(&md_region, true);
	mtk_emimpu_set_protection(&md_region);
	mtk_emimpu_free_region(&md_region);
#endif
}

/**
 * vpu_is_disabled - enable/disable vpu from efuse
 * @vd: struct vpu_device to get the id
 *
 * return 1: this vd->id is disabled
 * return 0: this vd->id is enabled
 */
bool vpu_is_disabled(struct vpu_device *vd)
{
	return false;
}

