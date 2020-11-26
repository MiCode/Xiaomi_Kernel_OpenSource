 /*
  * Goodix Touchscreen Driver
  * Core layer of touchdriver architecture.
  *
  * Copyright (C) 2015 - 2016 Goodix, Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be a reference
  * to you, when you are integrating the GOODiX's CTP IC into your system,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * General Public License for more details.
  *
  */

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include "goodix_ts_core.h"

atomic_t gt9896s_tui_flag = ATOMIC_INIT(0);
EXPORT_SYMBOL_GPL(gt9896s_tui_flag);

int tpd_enter_tui(void)
{
	int ret = 0;

	ts_info("[%s] enter TUI", __func__);

	ts_core_for_tui->ts_event.touch_data.touch_num = 0;
	gt9896s_ts_report_finger(ts_core_for_tui->input_dev,
		&ts_core_for_tui->ts_event.touch_data);

	gt9896s_ts_irq_enable(ts_core_for_tui, false);

	mt_spi_enable_master_clk(ts_core_for_tui->ts_dev->spi_dev);

	atomic_set(&gt9896s_tui_flag, true);

	return ret;
}

int tpd_exit_tui(void)
{
	int ret = 0;

	ts_info("[%s] exit TUI", __func__);

	mt_spi_disable_master_clk(ts_core_for_tui->ts_dev->spi_dev);

	gt9896s_ts_irq_enable(ts_core_for_tui, false);

	gt9896s_ts_irq_enable(ts_core_for_tui, true);

	atomic_set(&gt9896s_tui_flag, false);

	return ret;
}
#endif

