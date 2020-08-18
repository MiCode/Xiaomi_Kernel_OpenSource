/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __DEBUG_PLAT_API_H__
#define __DEBUG_PLAT_API_H__

#include <linux/types.h>
#include <linux/of_device.h>


#include "debug_plat_internal.h"


const struct of_device_id *debug_plat_get_device(void);


#endif /* __DEBUG_PLAT_API_H__ */
