/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DT_ARM_SMMU_H__
#define __DT_ARM_SMMU_H__

#define ARM_SMMU_OPT_SECURE_CFG_ACCESS  (1 << 0)
#define ARM_SMMU_OPT_FATAL_ASF		(1 << 1)
#define ARM_SMMU_OPT_SKIP_INIT		(1 << 2)
#define ARM_SMMU_OPT_DYNAMIC		(1 << 3)
#define ARM_SMMU_OPT_3LVL_TABLES	(1 << 4)
#define ARM_SMMU_OPT_NO_ASID_RETENTION	(1 << 5)
#define ARM_SMMU_OPT_DISABLE_ATOS	(1 << 6)
#define ARM_SMMU_OPT_MMU500_ERRATA1	(1 << 7)
#define ARM_SMMU_OPT_STATIC_CB          (1 << 8)
#define ARM_SMMU_OPT_HALT               (1 << 9)

#endif
