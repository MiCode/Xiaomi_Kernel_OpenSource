/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/module.h>

/* Symbol export */
int soidle3_enter(int cpu)
{
	return 0;
}
EXPORT_SYMBOL(soidle3_enter);

int soidle_enter(int cpu)
{
	return 0;
}
EXPORT_SYMBOL(soidle_enter);

int dpidle_enter(int cpu)
{
	return 0;
}
EXPORT_SYMBOL(dpidle_enter);

/* for display use, abandoned 'spm_enable_sodi' */
void mtk_idle_disp_is_ready(bool enable)
{
	pr_notice("Power/swap - %s not support anymore!\n", __func__);
}

