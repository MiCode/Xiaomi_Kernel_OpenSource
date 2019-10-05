/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/**
 * @file	mtk_eem_api.
 * @brief   Driver for EEM
 *
 */

#define __MTK_EEM_API_C__
#include "mtk_eem_api.h"
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_eem_internal_ap.h"
#include "mtk_eem_internal.h"

unsigned int drcc_offset_done;

static struct eem_det *id_to_eem_det(enum eem_det_id id)
{
	if (likely(id < NR_EEM_DET))
		return &eem_detectors[id];
	else
		return NULL;
}

void drcc_offset_set(void)
{
	enum eem_det_id id = EEM_DET_L;
	struct eem_det *det = id_to_eem_det(id);
	int i;

	for (i = 0; i < NR_FREQ; i++)
		det->volt_offset_drcc[i] = 7;
}

void drcc_fail_composite(void)
{
	enum eem_det_id id = EEM_DET_L;
	struct eem_det *det = id_to_eem_det(id);

	if (!drcc_offset_done) {
		drcc_offset_set();
		drcc_offset_done = 1;
		det->ops->set_volt(det);
	}
}
EXPORT_SYMBOL(drcc_fail_composite);
#undef __MTK_EEM_API_C__
