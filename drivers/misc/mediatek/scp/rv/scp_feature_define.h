/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SCP_FEATURE_DEFINE_H__
#define __SCP_FEATURE_DEFINE_H__

#include "scp.h"

/* scp platform configs*/
#define SCP_BOOT_TIME_OUT_MONITOR        (1)
#define SCP_RESERVED_MEM                 (1)
#ifdef CONFIG_MTK_TINYSYS_SCP_LOGGER_SUPPORT
#define SCP_LOGGER_ENABLE                (1)
#else
#define SCP_LOGGER_ENABLE                (0)
#endif
#define SCP_VOW_LOW_POWER_MODE           (1)

/* scp rescovery feature option*/
#define SCP_RECOVERY_SUPPORT             (1)
/* scp recovery timeout value (ms)*/
#define SCP_SYS_RESET_TIMEOUT            1000

#define SCP_PARAMS_TO_SCP_SUPPORT

/* scp aed definition*/
#define SCP_AED_STR_LEN                  (512)
#define SCP_CHECK_AED_STR_LEN(func, offset) ({\
	int ret; ret = func; ((ret > 0) && ((ret + offset) < (SCP_AED_STR_LEN - 1))) ? ret : 0; })

/* scp sub feature register API marco*/
#define SCP_REGISTER_SUB_SENSOR          (1)

/* emi mpu define*/
#define ENABLE_SCP_EMI_PROTECTION        (1)

#define MPU_REGION_ID_SCP_SMEM           7
#define MPU_DOMAIN_D0                    0
#define MPU_DOMAIN_D3                    3


#define SCPSYS_CORE0                     0
#define SCPSYS_CORE1                     1

struct scp_feature_tb {
	uint32_t feature:5,	/* max = 31 */
		 freq:10,	/* max = 1023 */
		 enable:1,	/* max = 1 */
		 sys_id:1;	/* max = 1, run at which subsys? */
};

extern struct scp_feature_tb feature_table[NUM_FEATURE_ID];

#endif


