/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __SMI_COMMON_H__
#define __SMI_COMMON_H__

#include <mt-plat/aee.h>
#include "smi_configuration.h"

#define SMI_TAG "SMI"
#undef pr_fmt
#if 1
#define pr_fmt(fmt) "[" SMI_TAG "]" fmt
#else
#define pr_fmt(fmt) "%s:%d: " fmt, __func__, __LINE__
#endif

#define SMIMSG(string, args...) pr_notice(string, ##args)

#ifdef CONFIG_MTK_CMDQ
#include <cmdq_core.h>
#define SMIMSG3(onoff, string, args...) \
	do { \
		if (onoff == 1) \
			cmdq_core_save_first_dump(string, ##args); \
		pr_notice(string, ##args); \
	} while (0)
#else
#define SMIMSG3(onoff, string, args...) pr_notice(string, ##args)
#endif

#if 1
#define SMIERR(string, args...) pr_notice(string, ##args)
#else
#define SMIERR(string, args...) \
	do { \
		pr_notice(string, ##args); \
		aee_kernel_warning(SMI_TAG, string, ##args); \
	} while (0)

#define smi_aee_print(string, args...) \
	do { \
		char smi_name[100]; \
		snprintf(smi_name, 100, "[" SMI_LOG_TAG "]" string, ##args); \
	} while (0)
#endif

/* use function instead gLarbBaseAddr to prevent NULL pointer access error */
unsigned long get_larb_base_addr(int larb_id);
unsigned long get_common_base_addr(void);
unsigned int smi_clk_get_ref_count(const unsigned int reg_indx);

int smi_bus_regs_setting(int larb_id, int profile,
	struct SMI_SETTING *settings);
int smi_common_setting(struct SMI_SETTING *settings);
int smi_larb_setting(int larb_id, struct SMI_SETTING *settings);
#endif
