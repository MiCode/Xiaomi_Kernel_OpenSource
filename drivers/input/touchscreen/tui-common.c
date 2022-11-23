// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#if IS_ENABLED(CONFIG_TOUCHSCREEN_MTK_TUI_COMMON_API)
static int (*tpd_tui_enter_func_request)(void);
static int (*tpd_tui_exit_func_request)(void);

void register_tpd_tui_request(int (*enter_func)(void), int (*exit_func)(void))
{
	pr_info("[%s] request tui function\n", __func__);
	tpd_tui_enter_func_request = enter_func;
	tpd_tui_exit_func_request = exit_func;
}
EXPORT_SYMBOL(register_tpd_tui_request);

int tpd_enter_tui(void)
{
	int ret = 0;

	pr_info("[%s] enter TUI+\n", __func__);

	if (tpd_tui_enter_func_request != NULL) {
		tpd_tui_enter_func_request();
		pr_info("[%s] enter func request success\n", __func__);
	}

	pr_info("[%s] enter TUI-\n", __func__);

	return ret;
}
EXPORT_SYMBOL(tpd_enter_tui);

int tpd_exit_tui(void)
{
	int ret = 0;

	pr_info("[%s] exit TUI+\n", __func__);

	if (tpd_tui_exit_func_request != NULL) {
		tpd_tui_exit_func_request();
		pr_info("[%s] exit func request success\n", __func__);
	}

	pr_info("[%s] exit TUI-\n", __func__);

	return ret;
}
EXPORT_SYMBOL(tpd_exit_tui);
#endif

MODULE_DESCRIPTION("tui common");
MODULE_AUTHOR("mediatek");
MODULE_LICENSE("GPL v2");
