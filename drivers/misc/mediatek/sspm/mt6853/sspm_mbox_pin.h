/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __SSPM_MBOX_PIN_H__
#define __SSPM_MBOX_PIN_H__

#define SSPM_MBOX_TOTAL 5

/* definition of slot size for OUT PINs */
/* the following will use mbox 0 */
#define IPIS_C_PPM_OUT_SIZE            7
#define IPIS_C_QOS_OUT_SIZE            6
#define IPIS_C_PMIC_OUT_SIZE           5
#define IPIS_C_MET_OUT_SIZE            4
#define IPIS_C_THERMAL_OUT_SIZE        4
#define IPIS_C_GPU_DVFS_OUT_SIZE       4
#define IPIS_C_GPU_PM_OUT_SIZE         2
/* the following will use mbox 1 */
#define IPIS_C_PLATFORM_OUT_SIZE       3
#define IPIS_C_SMI_OUT_SIZE            3
#define IPIS_C_CM_OUT_SIZE             2
#define IPIS_C_SLBC_OUT_SIZE           2
#define IPIS_C_SPM_SUSPEND_OUT_SIZE    8
#define IPIR_C_MET_OUT_SIZE            1
#define IPIR_C_GPU_DVFS_OUT_SIZE       1
#define IPIR_C_PLATFORM_OUT_SIZE       1
#define IPIR_C_SLBC_OUT_SIZE           1

/* definition of slot offset for OUT PINs */
/* the following will use mbox 0 */
#define IPIS_C_PPM_OUT_OFFSET          0
#define IPIS_C_QOS_OUT_OFFSET          (IPIS_C_PPM_OUT_OFFSET \
					+ IPIS_C_PPM_OUT_SIZE)
#define IPIS_C_PMIC_OUT_OFFSET         (IPIS_C_QOS_OUT_OFFSET \
					+ IPIS_C_QOS_OUT_SIZE)
#define IPIS_C_MET_OUT_OFFSET          (IPIS_C_PMIC_OUT_OFFSET \
					+ IPIS_C_PMIC_OUT_SIZE)
#define IPIS_C_THERMAL_OUT_OFFSET      (IPIS_C_MET_OUT_OFFSET \
					+ IPIS_C_MET_OUT_SIZE)
#define IPIS_C_GPU_DVFS_OUT_OFFSET     (IPIS_C_THERMAL_OUT_OFFSET \
					+ IPIS_C_THERMAL_OUT_SIZE)
#define IPIS_C_GPU_PM_OUT_OFFSET       (IPIS_C_GPU_DVFS_OUT_OFFSET \
					+ IPIS_C_GPU_DVFS_OUT_SIZE)
/* the following will use mbox 1 */
#define IPIS_C_PLATFORM_OUT_OFFSET     0
#define IPIS_C_SMI_OUT_OFFSET          (IPIS_C_PLATFORM_OUT_OFFSET \
					+ IPIS_C_PLATFORM_OUT_SIZE)
#define IPIS_C_CM_OUT_OFFSET           (IPIS_C_SMI_OUT_OFFSET \
					+ IPIS_C_SMI_OUT_SIZE)
#define IPIS_C_SLBC_OUT_OFFSET         (IPIS_C_CM_OUT_OFFSET \
					+ IPIS_C_CM_OUT_SIZE)
#define IPIS_C_SPM_SUSPEND_OUT_OFFSET  (IPIS_C_SLBC_OUT_OFFSET \
					+ IPIS_C_SLBC_OUT_SIZE)
#define IPIR_C_MET_OUT_OFFSET          (IPIS_C_SPM_SUSPEND_OUT_OFFSET \
					+ IPIS_C_SPM_SUSPEND_OUT_SIZE)
#define IPIR_C_GPU_DVFS_OUT_OFFSET     (IPIR_C_MET_OUT_OFFSET \
					+ IPIR_C_MET_OUT_SIZE)
#define IPIR_C_PLATFORM_OUT_OFFSET     (IPIR_C_GPU_DVFS_OUT_OFFSET \
					+ IPIR_C_GPU_DVFS_OUT_SIZE)
#define IPIR_C_SLBC_OUT_OFFSET         (IPIR_C_PLATFORM_OUT_OFFSET \
					+ IPIR_C_PLATFORM_OUT_SIZE)

/* definition of slot size for IN PINs */
/* the following will use mbox 2 */
#define IPIR_I_QOS_IN_SIZE             6
#define IPIR_C_MET_IN_SIZE             4
#define IPIR_C_GPU_DVFS_IN_SIZE        4
#define IPIR_C_PLATFORM_IN_SIZE        3
#define IPIR_C_SLBC_IN_SIZE            2
#define IPIS_C_PPM_IN_SIZE             1
#define IPIS_C_QOS_IN_SIZE             1
#define IPIS_C_PMIC_IN_SIZE            1
#define IPIS_C_MET_IN_SIZE             1
#define IPIS_C_THERMAL_IN_SIZE         1
#define IPIS_C_GPU_DVFS_IN_SIZE        1
#define IPIS_C_GPU_PM_IN_SIZE          1
#define IPIS_C_PLATFORM_IN_SIZE        1
#define IPIS_C_SMI_IN_SIZE             1
#define IPIS_C_CM_IN_SIZE              1
#define IPIS_C_SLBC_IN_SIZE            1
#define IPIS_C_SPM_SUSPEND_IN_SIZE     1

/* definition of slot offset for IN PINs */
/* the following will use mbox 2 */
#define IPIR_I_QOS_IN_OFFSET           0
#define IPIR_C_MET_IN_OFFSET           (IPIR_I_QOS_IN_OFFSET \
					+ IPIR_I_QOS_IN_SIZE)
#define IPIR_C_GPU_DVFS_IN_OFFSET      (IPIR_C_MET_IN_OFFSET \
					+ IPIR_C_MET_IN_SIZE)
#define IPIR_C_PLATFORM_IN_OFFSET      (IPIR_C_GPU_DVFS_IN_OFFSET \
					+ IPIR_C_GPU_DVFS_IN_SIZE)
#define IPIR_C_SLBC_IN_OFFSET          (IPIR_C_PLATFORM_IN_OFFSET \
					+ IPIR_C_PLATFORM_IN_SIZE)
#define IPIS_C_PPM_IN_OFFSET           (IPIR_C_SLBC_IN_OFFSET \
					+ IPIR_C_SLBC_IN_SIZE)
#define IPIS_C_QOS_IN_OFFSET           (IPIS_C_PPM_IN_OFFSET \
					+ IPIS_C_PPM_IN_SIZE)
#define IPIS_C_PMIC_IN_OFFSET          (IPIS_C_QOS_IN_OFFSET \
					+ IPIS_C_QOS_IN_SIZE)
#define IPIS_C_MET_IN_OFFSET           (IPIS_C_PMIC_IN_OFFSET \
					+ IPIS_C_PMIC_IN_SIZE)
#define IPIS_C_THERMAL_IN_OFFSET       (IPIS_C_MET_IN_OFFSET \
					+ IPIS_C_MET_IN_SIZE)
#define IPIS_C_GPU_DVFS_IN_OFFSET      (IPIS_C_THERMAL_IN_OFFSET \
					+ IPIS_C_THERMAL_IN_SIZE)
#define IPIS_C_GPU_PM_IN_OFFSET        (IPIS_C_GPU_DVFS_IN_OFFSET \
					+ IPIS_C_GPU_DVFS_IN_SIZE)
#define IPIS_C_PLATFORM_IN_OFFSET      (IPIS_C_GPU_PM_IN_OFFSET \
					+ IPIS_C_GPU_PM_IN_SIZE)
#define IPIS_C_SMI_IN_OFFSET           (IPIS_C_PLATFORM_IN_OFFSET \
					+ IPIS_C_PLATFORM_IN_SIZE)
#define IPIS_C_CM_IN_OFFSET            (IPIS_C_SMI_IN_OFFSET \
					+ IPIS_C_SMI_IN_SIZE)
#define IPIS_C_SLBC_IN_OFFSET          (IPIS_C_CM_IN_OFFSET \
					+ IPIS_C_CM_IN_SIZE)
#define IPIS_C_SPM_SUSPEND_IN_OFFSET   (IPIS_C_SLBC_IN_OFFSET \
					+ IPIS_C_SLBC_IN_SIZE)

#define SHAREMBOX_NO_MCDI              3
#define SHAREMBOX_OFFSET_MCDI          0
#define SHAREMBOX_SIZE_MCDI            20
#define SHAREMBOX_OFFSET_TIMESTAMP     (SHAREMBOX_OFFSET_MCDI \
					+ SHAREMBOX_SIZE_MCDI)
#define SHAREMBOX_SIZE_TIMESTAMP       6

#endif /* __SSPM_MBOX_PIN_H__ */
