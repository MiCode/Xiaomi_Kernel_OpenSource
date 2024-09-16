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
#include "gps_dl_config.h"
#include "gps_dl_context.h"
#include "gps_dl_hw_priv_util.h"
#include "gps_dl_subsys_reset.h"

#if GPS_DL_ON_LINUX
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/io.h>
#include "gps_dl_linux.h"
#if GPS_DL_HAS_PLAT_DRV
#include "gps_dl_linux_plat_drv.h"
#endif
#elif GPS_DL_ON_CTP
#include "kernel_to_ctp.h"
#endif

unsigned int gps_dl_bus_to_host_addr(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr)
{
	unsigned int host_addr = 0;

	switch (bus_id) {
	case GPS_DL_GPS_BUS:
		host_addr = gps_bus_to_host(bus_addr);
		break;
	case GPS_DL_BGF_BUS:
		host_addr = bgf_bus_to_host(bus_addr);
		break;
	case GPS_DL_CONN_INFRA_BUS:
		host_addr = bus_addr;
		break;
	default:
		host_addr = 0;
	}

	return host_addr;
}

void gps_dl_bus_wr_opt(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr, unsigned int val,
	unsigned int opt_bitmask)
{
	bool no_read_back    = !!(opt_bitmask & BMASK_WR_NO_READ_BACK);
	bool do_check     = !!(opt_bitmask & BMASK_RW_DO_CHECK);
	bool force_print  = !!(opt_bitmask & BMASK_RW_FORCE_PRINT);
	bool full_print   = !!(opt_bitmask & BMASK_RW_FULL_PRINT);
	bool print_vir_addr = false;

	unsigned int read_back_val = 0;
	unsigned int host_addr = gps_dl_bus_to_host_addr(bus_id, bus_addr);
#if GPS_DL_ON_LINUX
#if GPS_DL_HAS_PLAT_DRV
	void __iomem *host_vir_addr = gps_dl_host_addr_to_virt(host_addr);
#else
	void __iomem *host_vir_addr = phys_to_virt(host_addr);
#endif
#else
	void *host_vir_addr = NULL;
#endif

	/*
	 * For linux preparation and checking
	 */
#if GPS_DL_ON_LINUX
	if (host_vir_addr == NULL) {
		GDL_LOGW_RRW("bus_id = %d, addr = 0x%p/0x%08x/0x%08x, NULL!",
			bus_id, host_vir_addr, host_addr, bus_addr);
		return;
	}
	print_vir_addr = true;

	if (do_check) {
		/* gps_dl_conninfra_not_readable_show_warning(host_addr); */
		gps_dl_bus_check_and_print(host_addr);
	}
#endif /* GPS_DL_ON_LINUX */

	/*
	 * Do writing
	 */
#if GPS_DL_HW_IS_MOCK
	/* do nothing if it's mock */
#elif GPS_DL_ON_LINUX
	gps_dl_linux_sync_writel(val, host_vir_addr);
#else
	GPS_DL_HOST_REG_WR(host_addr, val);
#endif

	/*
	 * Do reading back
	 */
	if (!no_read_back) {
#if GPS_DL_HW_IS_MOCK
		/* do nothing if it's mock */
#elif GPS_DL_ON_LINUX
		read_back_val = __raw_readl(host_vir_addr);
#else
		read_back_val = GPS_DL_HOST_REG_RD(host_addr);
#endif
	}
	if (!(gps_dl_show_reg_rw_log() || force_print))
		return;

	/*
	 * Do printing if need
	 */
	if (no_read_back && (!full_print)) {
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%08x, w_val = 0x%08x",
			bus_id, host_addr, val);
	} else if (no_read_back && (full_print && !print_vir_addr)) {
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%08x/0x%08x, w_val = 0x%08x",
			bus_id, host_addr, bus_addr, val);
	} else if (no_read_back && (full_print && print_vir_addr)) {
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%p/0x%08x/0x%08x, w_val = 0x%08x",
			bus_id, host_vir_addr, host_addr, bus_addr, val);
	} else if (!no_read_back && (!full_print)) {
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%08x, w_val = 0x%08x, r_back = 0x%08x",
			bus_id, host_addr, val, read_back_val);
	} else if (!no_read_back && (full_print && !print_vir_addr)) {
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%08x/0x%08x, w_val = 0x%08x, r_back = 0x%08x",
			bus_id, host_addr, bus_addr, val, read_back_val);
	} else {
		/* if (!no_read_back && (full_print && print_vir_addr)) */
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%p/0x%08x/0x%08x, w_val = 0x%08x, r_back = 0x%08x",
			bus_id, host_vir_addr, host_addr, bus_addr, val, read_back_val);
	}
}

void gps_dl_bus_write(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr, unsigned int val)
{
	/* gps_dl_bus_wr_opt(bus_id, bus_addr, val, BMASK_RW_DO_CHECK); */
	gps_dl_bus_wr_opt(bus_id, bus_addr, val, 0);
}

void gps_dl_bus_write_no_rb(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr, unsigned int val)
{
	gps_dl_bus_wr_opt(bus_id, bus_addr, val, BMASK_WR_NO_READ_BACK);
}

unsigned int gps_dl_bus_rd_opt(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr,
	unsigned int opt_bitmask)
{
	bool do_check     = !!(opt_bitmask & BMASK_RW_DO_CHECK);
	bool force_print  = !!(opt_bitmask & BMASK_RW_FORCE_PRINT);
	bool full_print   = !!(opt_bitmask & BMASK_RW_FULL_PRINT);
	bool print_vir_addr = false;

	unsigned int val = 0;
	unsigned int host_addr = gps_dl_bus_to_host_addr(bus_id, bus_addr);
#if GPS_DL_ON_LINUX
#if GPS_DL_HAS_PLAT_DRV
	void __iomem *host_vir_addr = gps_dl_host_addr_to_virt(host_addr);
#else
	void __iomem *host_vir_addr = phys_to_virt(host_addr);
#endif
#else
	void *host_vir_addr = NULL;
#endif

	/*
	 * For linux preparation and checking
	 */
#if GPS_DL_ON_LINUX
	if (host_vir_addr == NULL) {
		GDL_LOGW_RRW("bus_id = %d, addr = 0x%p/0x%08x/0x%08x, NULL!",
			bus_id, host_vir_addr, host_addr, bus_addr);
		return 0;
	}
	print_vir_addr = true;

	if (do_check) {
		/* gps_dl_conninfra_not_readable_show_warning(host_addr); */
		gps_dl_bus_check_and_print(host_addr);
	}
#endif /* GPS_DL_ON_LINUX */

	/*
	 * Do reading
	 */
#if GPS_DL_HW_IS_MOCK
	/* do nothing if it's mock */
#elif GPS_DL_ON_LINUX
	val = __raw_readl(host_vir_addr);
#else
	val = GPS_DL_HOST_REG_RD(host_addr);
#endif
	if (!(gps_dl_show_reg_rw_log() || force_print))
		return val;

	/*
	 * Do printing if need
	 */
	if (!full_print) {
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%08x, r_val = 0x%08x",
			bus_id, host_addr, val);
	} else if (full_print && !print_vir_addr) {
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%08x/0x%08x, r_val = 0x%08x",
			bus_id, host_addr, bus_addr, val);
	} else {
		/* if (full_print && print_vir_addr) */
		GDL_LOGI_RRW("bus_id = %d, addr = 0x%p/0x%08x/0x%08x, r_val = 0x%08x",
			bus_id, host_vir_addr, host_addr, bus_addr, val);
	}
	return val;
}

unsigned int gps_dl_bus_read(enum GPS_DL_BUS_ENUM bus_id, unsigned int bus_addr)
{
	/* return gps_dl_bus_rd_opt(bus_id, bus_addr, BMASK_RW_DO_CHECK); */
	return gps_dl_bus_rd_opt(bus_id, bus_addr, 0);
}

