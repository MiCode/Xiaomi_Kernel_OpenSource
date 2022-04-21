// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-plat.h"

const struct camsys_platform_data *cur_platform;

void set_platform_data(const struct camsys_platform_data *platform_data)
{
	cur_platform = platform_data;
}
