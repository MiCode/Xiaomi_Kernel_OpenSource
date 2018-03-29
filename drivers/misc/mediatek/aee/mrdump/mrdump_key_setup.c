/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kconfig.h>
#include <linux/module.h>
#ifdef CONFIG_MTK_WATCHDOG
#include <mach/wd_api.h>
static int __init mrdump_key_init(void)
{
	int res;
	struct wd_api *wd_api = NULL;

	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_alert("%s: get wd api error %d\n", __func__, res);
	} else {
		res = wd_api->wd_debug_key_eint_config(1, 1);
		if (res == -1)
			pr_alert("%s: MRDUMP_KEY not supported\n", __func__);
		else
			pr_alert("%s: MRDUMP_KEY enabled\n", __func__);
	}
	return 0;
}

module_init(mrdump_key_init);
#endif


