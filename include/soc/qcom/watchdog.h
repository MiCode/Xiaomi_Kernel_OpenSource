/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 */

#ifndef _SOC_QCOM_WATCHDOG_H_
#define _SOC_QCOM_WATCHDOG_H_

#ifdef CONFIG_QCOM_FORCE_WDOG_BITE_ON_PANIC
#define WDOG_BITE_ON_PANIC 1
#else
#define WDOG_BITE_ON_PANIC 0
#endif

#if IS_ENABLED(CONFIG_QCOM_WATCHDOG)
void msm_trigger_wdog_bite(void);
#else
static inline void msm_trigger_wdog_bite(void) { }
#endif

#endif
