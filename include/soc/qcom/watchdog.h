/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _SOC_QCOM_WATCHDOG_H_
#define _SOC_QCOM_WATCHDOG_H_

#if IS_ENABLED(CONFIG_QCOM_WATCHDOG)
void msm_trigger_wdog_bite(void);
#else
static inline void msm_trigger_wdog_bite(void) { }
#endif

#endif
