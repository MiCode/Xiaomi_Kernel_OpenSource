/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __MT_IOMMU_H__
#define __MT_IOMMU_H__

#if IS_ENABLED(CONFIG_MACH_MT6781)
#include <dt-bindings/memory/mt6781-larb-port.h>
#else
#include <dt-bindings/memory/mt6785-larb-port.h>
#endif

#include "mtk_iommu_ext.h"

#endif
