// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
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

/* unsigned int drcc_offset_done; */

#if 0
static struct eem_det *id_to_eem_det(enum eem_det_id id)
{
	if (likely(id < NR_EEM_DET))
		return &eem_detectors[id];
	else
		return NULL;
}

void drcc_offset_set(void)
{
	enum eem_det_id id = EEM_DET_B;
	struct eem_det *det = id_to_eem_det(id);
	int i;

	for (i = 0; i < NR_FREQ; i++)
		det->volt_offset_drcc[i] = 7;
}

void drcc_fail_composite(void)
{
	enum eem_det_id id = EEM_DET_B;
	struct eem_det *det = id_to_eem_det(id);

	if (!drcc_offset_done) {
		drcc_offset_set();
		drcc_offset_done = 1;
		det->ops->set_volt(det);
	}
}
EXPORT_SYMBOL(drcc_fail_composite);
#endif
#undef __MTK_EEM_API_C__
