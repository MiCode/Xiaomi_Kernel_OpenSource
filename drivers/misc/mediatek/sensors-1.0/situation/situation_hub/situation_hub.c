// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "situation_hub.h"

static int __init situation_hub_init(void)
{
#if IS_ENABLED(CONFIG_MTK_INPKHUB)
	inpocket_init();
#endif

#if IS_ENABLED(CONFIG_MTK_STATHUB)
	stat_init();
#endif

#if IS_ENABLED(CONFIG_MTK_WAKEHUB)
	wakehub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_GLGHUB)
	glghub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_PICKUPHUB)
	pkuphub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_ANSWER_CALL_HUB)
	ancallhub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_DEVICE_ORIENTATION_HUB)
	device_orientation_init();
#endif

#if IS_ENABLED(CONFIG_MTK_MOTION_DETECT_HUB)
	motion_detect_init();
#endif

#if IS_ENABLED(CONFIG_MTK_TILTDETECTHUB)
	tiltdetecthub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_FLAT_HUB)
	flat_init();
#endif

#if IS_ENABLED(CONFIG_MTK_SAR_HUB)
	sarhub_init();
#endif

	return 0;
}

static void __exit situation_hub_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_INPKHUB)
	inpocket_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_STATHUB)
	stat_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_WAKEHUB)
	wakehub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_GLGHUB)
	glghub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_PICKUPHUB)
	pkuphub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_ANSWER_CALL_HUB)
	ancallhub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_DEVICE_ORIENTATION_HUB)
	device_orientation_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_MOTION_DETECT_HUB)
	motion_detect_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_TILTDETECTHUB)
	tiltdetecthub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_FLAT_HUB)
	flat_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_SAR_HUB)
	sarhub_exit();
#endif
}

module_init(situation_hub_init);
module_exit(situation_hub_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("situtation hub driver");
MODULE_AUTHOR("Mediatek");

