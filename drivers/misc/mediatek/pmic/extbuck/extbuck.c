/*
 * Copyright (C) 2017 MediaTek Inc.
 * Sakys <jeff_chang@richtek.com>
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

#include <linux/regulator/consumer.h>
#include <mt-plat/upmu_common.h>
#if defined(CONFIG_REGULATOR_MT6315)
#include <linux/regulator/mt6315-misc.h>
#endif
#ifdef CONFIG_REGULATOR_RT5738
#include "rt5738-regulator.h"
#endif
#ifdef CONFIG_REGULATOR_MT6691
#include "mt6691-regulator.h"
#endif

int is_ext_buck_exist(void)
{
#ifndef CONFIG_MTK_EXTBUCK
	return 0;
#else
	#ifdef CONFIG_REGULATOR_ISL91302A
	struct regulator *reg;

	reg = regulator_get(NULL, "ext_buck_proc1");
	if (reg == NULL)
		return 0;
	regulator_put(reg);
	return 1;
	#endif /* CONFIG_REGULATOR_ISL91302A */
	#ifdef CONFIG_EXTBUCK_MT6311
	if ((is_mt6311_exist() == 1))
		return 1;
	#endif /* CONFIG_EXTBUCK_MT6311 */
	#if defined(CONFIG_MTK_PMIC_CHIP_MT6313)
	if ((is_mt6313_exist() == 1))
		return 1;
	#endif /* CONFIG_MTK_PMIC_CHIP_MT6313 */
	#if defined(CONFIG_REGULATOR_MT6315)
	if ((is_mt6315_exist() == 1))
		return 1;
	#endif
	#ifdef CONFIG_REGULATOR_RT5738
	if ((is_rt5738_exist() == 1))
		return 1;

	#endif /* CONFIG_REGULATOR_RT5738 */
#endif /* if not CONFIG_MTK_EXTBUCK */
	return 0;
}

int is_ext_buck2_exist(void)
{
#ifndef CONFIG_MTK_EXTBUCK
	return 0;
#else
	#ifdef CONFIG_REGULATOR_RT5738
	if ((is_rt5738_exist() == 1))
		return 1;
	#endif /* CONFIG_REGULATOR_RT5738 */
	#ifdef CONFIG_REGULATOR_MT6691
	return is_mt6691_exist();
	#endif /* CONFIG_REGULATOR_MT6691 */
	return 0;
#endif /* if not CONIFG_MTK_EXTBUCK */
}

int is_ext_buck_sw_ready(void)
{
#if defined(CONFIG_EXTBUCK_MT6311)
	if ((is_mt6311_sw_ready() == 1))
		return 1;
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6313)
	if ((is_mt6313_sw_ready() == 1))
		return 1;
#endif
	return 0;
}

