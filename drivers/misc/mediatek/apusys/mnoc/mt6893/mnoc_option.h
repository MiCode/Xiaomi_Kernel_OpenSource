/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef __APUSYS_MNOC_OPTION_H__
#define __APUSYS_MNOC_OPTION_H__

#define MNOC_TIME_PROFILE (0)
#define MNOC_INT_ENABLE (1)
#if IS_ENABLED(CONFIG_MTK_QOS_V2)
#define MNOC_QOS_ENABLE (1)
#define MNOC_QOS_BOOST_ENABLE (1)
#else
#define MNOC_QOS_ENABLE (0)
#define MNOC_QOS_BOOST_ENABLE (0)
#endif
/* #define MNOC_QOS_DEBOUNCE */
#define MNOC_DBG_ENABLE (0)
#define MNOC_AEE_WARN_ENABLE (1)
#define MNOC_APU_PWR_CHK (1)

#define APU_QOS_IPUIF_ADJUST

#endif
