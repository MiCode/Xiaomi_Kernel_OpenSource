// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/io.h>

int (*dvfsrc_query_opp_info)(u32 id);
void register_dvfsrc_opp_handler(int (*handler)(u32 id))
{
	dvfsrc_query_opp_info = handler;
}

int mtk_dvfsrc_query_opp_info(u32 id)
{
	if (dvfsrc_query_opp_info != NULL)
		return dvfsrc_query_opp_info(id);

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_opp_info);

int (*dvfsrc_query_debug_info)(u32 id);
void register_dvfsrc_debug_handler(int (*handler)(u32 id))
{
	dvfsrc_query_debug_info = handler;
}
EXPORT_SYMBOL(register_dvfsrc_debug_handler);

int mtk_dvfsrc_query_debug_info(u32 id)
{
	if (dvfsrc_query_debug_info != NULL)
		return dvfsrc_query_debug_info(id);

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_debug_info);

void (*dvfsrc_power_model_ddr_request)(u32 level);
void register_dvfsrc_cm_ddr_handler(void (*handler)(u32 level))
{
	dvfsrc_power_model_ddr_request = handler;
}
void dvfsrc_set_power_model_ddr_request(u32 level)
{
	if (dvfsrc_power_model_ddr_request != NULL)
		return dvfsrc_power_model_ddr_request(level);
}
EXPORT_SYMBOL(dvfsrc_set_power_model_ddr_request);

void (*dvfsrc_enable_dvfs_hopping)(int on);
void register_dvfsrc_hopping_handler(void (*handler)(int on))
{
	dvfsrc_enable_dvfs_hopping = handler;
}
void dvfsrc_enable_dvfs_freq_hopping(int on)
{
	if (dvfsrc_enable_dvfs_hopping != NULL)
		return dvfsrc_enable_dvfs_hopping(on);
}
EXPORT_SYMBOL(dvfsrc_enable_dvfs_freq_hopping);

