/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SITUTATION_HUB_H__
#define __SITUTATION_HUB_H__

#include <linux/module.h>

#if IS_ENABLED(CONFIG_MTK_INPKHUB)
#include "inpocket/inpocket.h"
#endif

#if IS_ENABLED(CONFIG_MTK_STATHUB)
#include "stationary/stationary.h"
#endif

#if IS_ENABLED(CONFIG_MTK_WAKEHUB)
#include "wake_gesture/wake_gesture.h"
#endif

#if IS_ENABLED(CONFIG_MTK_GLGHUB)
#include "glance_gesture/glance_gesture.h"
#endif

#if IS_ENABLED(CONFIG_MTK_PICKUPHUB)
#include "pickup_gesture/pickup_gesture.h"
#endif

#if IS_ENABLED(CONFIG_MTK_ANSWER_CALL_HUB)
#include "answercall/ancallhub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_DEVICE_ORIENTATION_HUB)
#include "device_orientation/device_orientation.h"
#endif

#if IS_ENABLED(CONFIG_MTK_MOTION_DETECT_HUB)
#include "motion_detect/motion_detect.h"
#endif

#if IS_ENABLED(CONFIG_MTK_TILTDETECTHUB)
#include "tilt_detector/tiltdetecthub.h"
#endif

#if IS_ENABLED(CONFIG_MTK_FLAT_HUB)
#include "flat/flat.h"
#endif

#if IS_ENABLED(CONFIG_MTK_SAR_HUB)
#include "sar/sarhub.h"
#endif

#endif
