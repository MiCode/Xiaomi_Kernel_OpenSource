// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */


#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <mt-plat/aee.h>
#include <mt-plat/upmu_common.h>

#include "mtk_power_gs.h"

/* #define POWER_GS_DEBUG */
#undef mt_power_gs_dump_suspend
#undef mt_power_gs_dump_dpidle
#undef mt_power_gs_dump_sodi3
#undef mt_power_gs_t_dump_suspend
#undef mt_power_gs_t_dump_dpidle
#undef mt_power_gs_t_dump_sodi3


struct golden _g;

bool is_already_snap_shot;

bool slp_chk_golden_suspend = true;
bool slp_chk_golden_dpidle = true;
bool slp_chk_golden_sodi3 = true;
bool slp_chk_golden_diff_mode = true;

void __weak mt_power_gs_suspend_compare(unsigned int dump_flag)
{
	pr_info("Power_gs: %s does not implement\n", __func__);
}
void __weak mt_power_gs_dpidle_compare(unsigned int dump_flag)
{
	pr_info("Power_gs: %s does not implement\n", __func__);
}
void __weak mt_power_gs_sodi_compare(unsigned int dump_flag)
{
	pr_info("Power_gs: %s does not implement\n", __func__);
}

/* deprecated, temp used for api argument transfer */
void mt_power_gs_f_dump_suspend(unsigned int dump_flag)
{
	if (slp_chk_golden_suspend)
		mt_power_gs_suspend_compare(dump_flag);
}
void mt_power_gs_t_dump_suspend(int count, ...)
{
	unsigned int p1 = GS_ALL;
	va_list v;

	va_start(v, count);

	if (count)
		p1 = va_arg(v, unsigned int);

	/* if the argument is void, va_arg will get -1 */
	if (p1 > GS_ALL)
		p1 = GS_ALL;

	mt_power_gs_f_dump_suspend(p1);
	va_end(v);
}
EXPORT_SYMBOL(mt_power_gs_t_dump_suspend);
void mt_power_gs_f_dump_dpidle(unsigned int dump_flag)
{
	if (slp_chk_golden_dpidle)
		mt_power_gs_dpidle_compare(dump_flag);
}
void mt_power_gs_t_dump_dpidle(int count, ...)
{
	unsigned int p1 = GS_ALL;
	va_list v;

	va_start(v, count);

	if (count)
		p1 = va_arg(v, unsigned int);

	/* if the argument is void, va_arg will get -1 */
	if (p1 > GS_ALL)
		p1 = GS_ALL;

	mt_power_gs_f_dump_dpidle(p1);
	va_end(v);
}
EXPORT_SYMBOL(mt_power_gs_t_dump_dpidle);
void mt_power_gs_f_dump_sodi3(unsigned int dump_flag)
{
	if (slp_chk_golden_sodi3)
		mt_power_gs_sodi_compare(dump_flag);
}
void mt_power_gs_t_dump_sodi3(int count, ...)
{
	unsigned int p1 = GS_ALL;
	va_list v;

	va_start(v, count);

	if (count)
		p1 = va_arg(v, unsigned int);

	/* if the argument is void, va_arg will get -1 */
	if (p1 > GS_ALL)
		p1 = GS_ALL;

	mt_power_gs_f_dump_sodi3(p1);
	va_end(v);
}
EXPORT_SYMBOL(mt_power_gs_t_dump_sodi3);

void mt_power_gs_dump_suspend(void)
{
	mt_power_gs_f_dump_suspend(GS_ALL);
}
EXPORT_SYMBOL(mt_power_gs_dump_suspend);
void mt_power_gs_dump_dpidle(void)
{
	mt_power_gs_f_dump_dpidle(GS_ALL);
}
EXPORT_SYMBOL(mt_power_gs_dump_dpidle);
void mt_power_gs_dump_sodi3(void)
{
	mt_power_gs_f_dump_sodi3(GS_ALL);
}
EXPORT_SYMBOL(mt_power_gs_dump_sodi3);

int snapshot_golden_setting(const char *func, const unsigned int line)
{
	if (!is_already_snap_shot)
		return _snapshot_golden_setting(&_g, func, line);

	return 0;
}
EXPORT_SYMBOL(snapshot_golden_setting);

static void __exit mt_power_gs_exit(void)
{
}

static int __init mt_power_gs_init(void)
{
	return 0;
}

module_init(mt_power_gs_init);
module_exit(mt_power_gs_exit);

MODULE_DESCRIPTION("MT Low Power Golden Setting");
