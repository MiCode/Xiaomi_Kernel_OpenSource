// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
#include "goodix_ts_core.h"

atomic_t gt9895_tui_flag = ATOMIC_INIT(0);
EXPORT_SYMBOL_GPL(gt9895_tui_flag);

int tpd_gt9895_enter_tui(void)
{
	int ret = 0;

	ts_info("[%s] enter TUI", __func__);

	ts_core_gt9895_tui->ts_event.touch_data.touch_num = 0;
	goodix_ts_report_finger(ts_core_gt9895_tui->input_dev,
		&ts_core_gt9895_tui->ts_event.touch_data);

	ts_core_gt9895_tui->hw_ops->irq_enable(ts_core_gt9895_tui, false);

	mt_spi_enable_master_clk(ts_core_gt9895_tui->bus->spi_dev);

	goodix_ts_esd_off(ts_core_gt9895_tui);

	atomic_set(&gt9895_tui_flag, true);

	return ret;
}
EXPORT_SYMBOL(tpd_gt9895_enter_tui);

int tpd_gt9895_exit_tui(void)
{
	int ret = 0;

	ts_info("[%s] exit TUI", __func__);

	mt_spi_disable_master_clk(ts_core_gt9895_tui->bus->spi_dev);

	ts_core_gt9895_tui->hw_ops->irq_enable(ts_core_gt9895_tui, false);
	ts_core_gt9895_tui->hw_ops->irq_enable(ts_core_gt9895_tui, true);

	goodix_ts_esd_on(ts_core_gt9895_tui);

	atomic_set(&gt9895_tui_flag, false);

	return ret;
}
EXPORT_SYMBOL(tpd_gt9895_exit_tui);
#endif

