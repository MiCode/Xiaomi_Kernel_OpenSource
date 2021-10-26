/*
 * Copyright (C) 2011-2015 MediaTek Inc.
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

#include <linux/module.h>       /* needed by all modules */
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_ipi_id.h"

/* debug API */
#include <memory/mediatek/emi.h>
__weak void dump_emi_outstanding(void) {}
__weak void mtk_spm_dump_debug_info(void) {}
__weak void usb_dump_debug_register(void) {}
__weak void dpmaif_dump_reg(void) {}

static char *pin_name[SSPM_IPI_COUNT] = {
	"PPM",
	"QOS",
	"PMIC",
	"MET",
	"THERMAL",
	"GPU_DVFS",
	"GPU_PM",
	"PLATFORM",
	"SMI",
	"CM",
	"SLBC",
	"QOS",
	"MET",
	"GPU_DVFS",
	"PLATFORM",
	"SLBC",
};

/* platform callback when ipi timeout */
void sspm_ipi_timeout_cb(int ipi_id)
{
	pr_info("Error: possible error IPI %d pin=%s\n",
		ipi_id, pin_name[ipi_id]);

	ipi_monitor_dump(&sspm_ipidev);

	BUG_ON(1);
}

