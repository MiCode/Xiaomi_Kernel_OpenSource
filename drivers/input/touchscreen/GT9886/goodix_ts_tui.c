 /*
  * Goodix Touchscreen Driver
  * Core layer of touchdriver architecture.
  *
  * Copyright (C) 2015 - 2016 Goodix, Inc.
  * Authors:  Yulong Cai <caiyulong@goodix.com>
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

#if IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI)
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
EXPORT_SYMBOL(tpd_enter_tui);

int tpd_exit_tui(void)
{
	int ret = 0;

	ts_info("[%s] exit TUI", __func__);

	atomic_set(&gt9886_tui_flag, false);

	goodix_ts_irq_enable(resume_core_data, false);
	goodix_ts_irq_enable(resume_core_data, true);

	return ret;
}
EXPORT_SYMBOL(tpd_exit_tui);
#endif

