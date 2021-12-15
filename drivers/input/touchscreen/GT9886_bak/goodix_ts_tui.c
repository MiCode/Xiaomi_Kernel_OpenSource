/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include "goodix_ts_core.h"

atomic_t gt9886_tui_flag = ATOMIC_INIT(0);
EXPORT_SYMBOL(gt9886_tui_flag);

int tpd_enter_tui(void)
{
	int ret = 0;

	ts_info("[%s] enter TUI", __func__);

	atomic_set(&gt9886_tui_flag, true);

	return ret;
}

int tpd_exit_tui(void)
{
	int ret = 0;

	ts_info("[%s] exit TUI", __func__);

	atomic_set(&gt9886_tui_flag, false);

	goodix_ts_irq_enable(resume_core_data, false);
	goodix_ts_irq_enable(resume_core_data, true);

	return ret;
}
#endif

