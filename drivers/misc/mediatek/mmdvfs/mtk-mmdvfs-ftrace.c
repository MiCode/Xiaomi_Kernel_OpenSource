// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include "mtk-mmdvfs-ftrace.h"

#define CREATE_TRACE_POINTS
#include "mmdvfs_events.h"

void ftrace_record_opp_v1(unsigned long rec, unsigned long opp)
{
	trace_mmdvfs__record_opp_v1(rec, opp);
}
EXPORT_SYMBOL_GPL(ftrace_record_opp_v1);

void ftrace_pwr_opp_v3(unsigned long pwr, unsigned long opp)
{
	trace_mmdvfs__pwr_opp_v3(pwr, opp);
}
EXPORT_SYMBOL_GPL(ftrace_pwr_opp_v3);

void ftrace_user_opp_v3_vcore(unsigned long user, unsigned long opp)
{
	trace_mmdvfs__user_opp_v3_vcore(user, opp);
}
EXPORT_SYMBOL_GPL(ftrace_user_opp_v3_vcore);

void ftrace_user_opp_v3_vmm(unsigned long user, unsigned long opp)
{
	trace_mmdvfs__user_opp_v3_vmm(user, opp);
}
EXPORT_SYMBOL_GPL(ftrace_user_opp_v3_vmm);

MODULE_DESCRIPTION("MMDVFS FTRACE");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
