// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "flashlight-core.h"
/*N19A code for HQ-353578 by wangjie at 2023/12/1 start*/
const struct flashlight_device_id flashlight_id[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights-mt6768", 0, 1},
};
/*N19A code for HQ-353578 by wangjie at 2023/12/1 end*/
const int flashlight_device_num =
	sizeof(flashlight_id) / sizeof(struct flashlight_device_id);

