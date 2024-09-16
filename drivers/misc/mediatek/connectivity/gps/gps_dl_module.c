/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/init.h>
#include <linux/module.h>

#include "gps_dl_config.h"

#if GPS_DL_IS_MODULE
#include "gps_data_link_devices.h"

/*****************************************************************************/
static int gps_dl_mod_init(void)
{
	int ret = 0;

	mtk_gps_data_link_devices_init();

	return ret;
}

/*****************************************************************************/
static void gps_dl_mod_exit(void)
{
	mtk_gps_data_link_devices_exit();
}

#ifdef MTK_WCN_REMOVE_KERNEL_MODULE
int mtk_wcn_gpsdl_drv_init(void)
{
	return gps_dl_mod_init();
}
EXPORT_SYMBOL(mtk_wcn_gpsdl_drv_init);

void mtk_wcn_gpsdl_drv_exit(void)
{
	return gps_dl_mod_exit();
}
EXPORT_SYMBOL(mtk_wcn_gpsdl_drv_exit);

#else
module_init(gps_dl_mod_init);
module_exit(gps_dl_mod_exit);

#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hua Fu");
#endif

