/*
 * Copyright (C) 2020 MediaTek Inc.
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
	return false;
}

