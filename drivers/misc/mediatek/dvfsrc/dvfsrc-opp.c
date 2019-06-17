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

