/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __APS_UTILS_H__
#define __APS_UTILS_H__
#include "linux/device.h"

#define APS_TAG "[APS]"
#define APS_INFO(format, args...) pr_info(APS_TAG "[info] " format, ##args)
#define APS_WRN(format, args...) pr_info(APS_TAG "[warn] " format, ##args)
#define APS_ERR(format, args...) pr_info(APS_TAG "[error] " format, ##args)

#endif /* !__APS_UTILS_H__ */
