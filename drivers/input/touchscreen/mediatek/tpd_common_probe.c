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

#include "tpd.h"

static int __init tpd_probe_init(void)
{
	tpd_device_init();
	return 0;
}

static void __exit tpd_probe_exit(void)
{
	tpd_device_exit();
}
late_initcall(tpd_probe_init);
module_exit(tpd_probe_exit);

MODULE_DESCRIPTION("MediaTek touch panel driver");
MODULE_AUTHOR("Qiangming.xia@mediatek.com>");
MODULE_LICENSE("GPL");
