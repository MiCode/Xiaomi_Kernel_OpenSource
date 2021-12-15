// SPDX-License-Identifier: GPL-2.0
/*
 * mtk-dsp-core.h --  Mediatek ADSP dmemory control
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: celine liu <celine.liu@mediatek.com>
 */

#include <adsp_helper.h>

bool is_adsp_core_ready(void)
{
	return is_adsp_ready(ADSP_A_ID);
}

bool is_adsp_feature_registered(void)
{
	return false;
}

