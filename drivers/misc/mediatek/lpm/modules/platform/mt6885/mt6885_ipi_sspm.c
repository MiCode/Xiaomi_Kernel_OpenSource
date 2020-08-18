// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <mtk_lpm_trace_event/mtk_lpm_trace.h>

#include <mtk_ipi_sspm_v1.h>

#include "mt6885_ipi_sspm.h"
#include "mt6885_common.h"

static int _mt6885_ipi_sspm_send(int flags, unsigned int cmd)
{
	printk_deferred("[name:spm&]#@# %s(%d) TINYSYS_SSPM_SUPPORT is not support\n",
			__func__, __LINE__);
	return 0;
}

static int _mt6885_ipi_sspm_response(int flags, unsigned int cmd)
{

	printk_deferred("[name:spm&]#@# %s(%d) TINYSYS_SSPM_SUPPORT is not support\n",
			__func__, __LINE__);
	return 0;
}

int mt6885_sspm_notify_enter(unsigned int cmd)
{
	return _mt6885_ipi_sspm_send(SSPM_NOTIFY_ENTER, cmd);
}

int mt6885_sspm_notify_leave(unsigned int cmd)
{
	return _mt6885_ipi_sspm_send(SSPM_NOTIFY_LEAVE, cmd);
}

int mt6885_sspm_notify_ansyc_enter(unsigned int cmd)
{
	return _mt6885_ipi_sspm_send(SSPM_NOTIFY_ENTER_ASYNC, cmd);
}

int mt6885_sspm_notify_ansyc_enter_respone(void)
{
	return _mt6885_ipi_sspm_response(SSPM_NOTIFY_ENTER_ASYNC, 0);
}

int mt6885_sspm_notify_ansyc_leave(unsigned int cmd)
{
	return _mt6885_ipi_sspm_send(SSPM_NOTIFY_LEAVE_ASYNC, cmd);
}

int mt6885_sspm_notify_ansyc_leave_respone(void)
{
	return _mt6885_ipi_sspm_response(SSPM_NOTIFY_LEAVE_ASYNC, 0);
}
