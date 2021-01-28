/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FUSION_HUB_H__
#define __FUSION_HUB_H__

#include <linux/module.h>
#include "fusion.h"

#if IS_ENABLED(CONFIG_MTK_ROTATVECHUB)
#include "rotatvechub/rotatvechub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_GAMEROTVECHUB)
#include "gamerotvechub/gamerotvechub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_GMAGROTVECHUB)
#include "gmagrotvechub/gmagrotvechub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_GRAVITYHUB)
#include "gravityhub/gravityhub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_LINEARACCHUB)
#include "linearacchub/linearacchub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_ORIENTHUB)
#include "orienthub/orienthub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_ACCHUB)
#include "uncali_acchub/uncali_acchub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_GYROHUB)
#include "uncali_gyrohub/uncali_gyrohub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_UNCALI_MAGHUB)
#include "uncali_maghub/uncali_maghub.h"
#endif

#endif
