// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <Johnson-CH.chiu@mediatek.com>
 *
 */

#include "mtk_imgsys-trace.h"

int imgsys_ftrace_en;
module_param(imgsys_ftrace_en, int, 0644);

unsigned long imgsys_get_tracing_mark(void)
{
	static unsigned long __read_mostly tracing_mark_write_addr;

	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");

	return tracing_mark_write_addr;
}

bool imgsys_core_ftrace_enabled(void)
{
	return imgsys_ftrace_en;
}


