/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QCOM_HV_HAPTICS_H
#define _QCOM_HV_HAPTICS_H

#if IS_ENABLED(CONFIG_INPUT_QCOM_HV_HAPTICS)
bool qcom_haptics_vi_sense_is_enabled(void);
#else
static inline bool qcom_haptics_vi_sense_is_enabled(void)
{
	return false;
}
#endif

#endif
