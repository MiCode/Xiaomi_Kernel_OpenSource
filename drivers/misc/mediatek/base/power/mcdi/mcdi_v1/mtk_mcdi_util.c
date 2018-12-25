/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/sched.h>

#include <mtk_mcdi_util.h>
#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_mbox.h>
#endif

static inline unsigned int mcdi_sspm_read(int id)
{
	unsigned int val = 0;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	sspm_mbox_read(MCDI_MBOX, id, &val, 1);
#endif

	return val;
}

static inline void mcdi_sspm_write(int id, unsigned int val)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	sspm_mbox_write(MCDI_MBOX, id, (void *)&val, 1);
#endif
}

static inline int mcdi_sspm_ready(void)
{
	return IS_BUILTIN(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) ? true : false;
}

static inline unsigned int mcdi_mcupm_read(int id)
{
	return mcdi_read((uintptr_t)id);
}

static inline void mcdi_mcupm_write(int id, unsigned int val)
{
	mcdi_write((uintptr_t)id, val);
}

static inline int mcdi_mcupm_ready(void)
{
	return false;
}

#if defined(MCDI_SSPM_INTF)

#define __mcdi_mbox_read(id)           mcdi_sspm_read(id)
#define __mcdi_mbox_write(id, val)     mcdi_sspm_write(id, val)
#define __mcdi_fw_is_ready()           mcdi_sspm_ready()

#elif defined(MCDI_MCUPM_INTF)

#define __mcdi_mbox_read(id)           mcdi_mcupm_read(id)
#define __mcdi_mbox_write(id, val)     mcdi_mcupm_write(id, val)
#define __mcdi_fw_is_ready()           mcdi_mcupm_ready()

#else

#define __mcdi_mbox_read(id)           0
#define __mcdi_mbox_write(id, val)
#define __mcdi_fw_is_ready()           0

#endif

unsigned int mcdi_mbox_read(int id)
{
	return __mcdi_mbox_read(id);
}

void mcdi_mbox_write(int id, unsigned int val)
{
	__mcdi_mbox_write(id, val);
}

int mcdi_fw_is_ready(void)
{
	return __mcdi_fw_is_ready();
}

unsigned long long idle_get_current_time_us(void)
{
	unsigned long long idle_current_time = sched_clock();

	do_div(idle_current_time, 1000);
	return idle_current_time;
}

