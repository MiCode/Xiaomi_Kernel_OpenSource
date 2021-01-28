// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "fusion_hub.h"

static int __init fusion_hub_init(void)
{
#if IS_ENABLED(CONFIG_MTK_ROTATVECHUB)
	rotatvechub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_GAMEROTVECHUB)
	gamerotvechub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_GMAGROTVECHUB)
	gmagrotvechub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_GRAVITYHUB)
	gravityhub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_LINEARACCHUB)
	linearacchub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_ORIENTHUB)
	orienthub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_ACCHUB)
	uncali_acchub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_GYROHUB)
	uncali_gyrohub_init();
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_MAGHUB)
	uncali_maghub_init();
#endif

	return 0;
}

static void __exit fusion_hub_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_ROTATVECHUB)
	rotatvechub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_GAMEROTVECHUB)
	gamerotvechub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_GMAGROTVECHUB)
	gmagrotvechub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_GRAVITYHUB)
	gravityhub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_LINEARACCHUB)
	linearacchub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_ORIENTHUB)
	orienthub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_ACCHUB)
	uncali_acchub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_GYROHUB)
	uncali_gyrohub_exit();
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_MAGHUB)
	uncali_maghub_exit();
#endif

}

module_init(fusion_hub_init);
module_exit(fusion_hub_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fusion hub driver");
MODULE_AUTHOR("Mediatek");

