/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_TRUSTY_SM_ERR_MAPPING_H
#define __LINUX_TRUSTY_SM_ERR_MAPPING_H

#if defined(CONFIG_MTK_NEBULA_VM_SUPPORT) && defined(CONFIG_GZ_SMC_CALL_REMAP)
#include <linux/trusty/hv_err.h>
#include <linux/trusty/sm_err_remap.h>
#else
#include <linux/trusty/sm_err_trusty.h>
#endif

#endif /* __LINUX_TRUSTY_SM_ERR_MAPPING_H */
