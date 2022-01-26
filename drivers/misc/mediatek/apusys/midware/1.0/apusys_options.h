/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_OPTIONS_H__
#define __APUSYS_OPTIONS_H__

/* apusys interface support */
#define APUSYS_OPTIONS_INF_MNOC
#define APUSYS_OPTIONS_INF_REVISER
#define APUSYS_OPTIONS_INF_POWERARCH

#define APUSYS_OPTIONS_SUSPEND_SUPPORT

/* apusys memory support */
#define APUSYS_OPTIONS_MEM_ION
//#define APUSYS_OPTIONS_MEM_DMA
#define APUSYS_OPTIONS_MEM_VLM

/* scheduler param */
#define APUSYS_PARAM_WAIT_TIMEOUT (30*1000*1000)

#endif
