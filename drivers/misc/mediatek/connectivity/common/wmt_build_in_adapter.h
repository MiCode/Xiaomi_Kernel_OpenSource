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

#ifndef WMT_BUILD_IN_ADAPTER_H
#define WMT_BUILD_IN_ADAPTER_H

#include <mtk_wcn_cmb_stub.h>

#define KERNEL_mtk_wcn_cmb_sdio_request_eirq \
		mtk_wcn_cmb_sdio_request_eirq_by_wmt
void mtk_wcn_cmb_sdio_request_eirq_by_wmt(void);

/*******************************************************************************
 * Bridging from platform -> wmt_drv.ko
 ******************************************************************************/
typedef int (*wmt_bridge_thermal_query_cb)(void);
typedef int (*wmt_bridge_trigger_assert_cb)(void);
typedef void (*wmt_bridge_connsys_clock_fail_dump_cb)(void);

typedef int (*wmt_bridge_conninfra_reg_readable)(void);
typedef int (*wmt_bridge_conninfra_reg_is_bus_hang)(void);

struct wmt_platform_bridge {
	wmt_bridge_thermal_query_cb thermal_query_cb;
	wmt_bridge_trigger_assert_cb trigger_assert_cb;
	wmt_bridge_connsys_clock_fail_dump_cb clock_fail_dump_cb;

	/* for CONNAC 2 */
	wmt_bridge_conninfra_reg_readable conninfra_reg_readable_cb;
	wmt_bridge_conninfra_reg_is_bus_hang conninfra_reg_is_bus_hang_cb;
};

void wmt_export_platform_bridge_register(struct wmt_platform_bridge *cb);
void wmt_export_platform_bridge_unregister(void);


/*******************************************************************************
 * SDIO integration with platform MMC driver
 ******************************************************************************/
extern unsigned int wifi_irq;
extern pm_callback_t mtk_wcn_cmb_sdio_pm_cb;
extern void *mtk_wcn_cmb_sdio_pm_data;

void wmt_export_mtk_wcn_cmb_sdio_disable_eirq(void);
int wmt_export_mtk_wcn_sdio_irq_flag_set(int flag);

#endif /* WMT_BUILD_IN_ADAPTER_H */
