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

#ifndef __LINUX_TRUSTY_SMCALL_MAPPING_H
#define __LINUX_TRUSTY_SMCALL_MAPPING_H

#if 0
#if defined(CONFIG_MTK_NEBULA_VM_SUPPORT) && defined(CONFIG_GZ_SMC_CALL_REMAP)
#include <gz-trusty/hvcall.h>
#include <gz-trusty/smcall_remap.h>
#else
#include <gz-trusty/smcall_trusty.h>
#endif
#endif

#include <gz-trusty/hvcall.h>
#include <gz-trusty/smcall_trusty.h>

void trusty_fiq_glue_arm64(void);

#endif /* __LINUX_TRUSTY_SMCALL_MAPPING_H */
