// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/atomic.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "modem_sys.h"
#include "ccci_hif.h"
#include "md_sys1_platform.h"

#include "mt-plat/mtk_ccci_common.h"

#define TAG "md"

/* TO BE removed after variables in per_md_data move to the right module */
struct ccci_per_md *ccci_get_per_md_data(void)
{
	struct ccci_modem *md = ccci_get_modem();

	if (md)
		return &md->per_md_data;
	else
		return NULL;
}
EXPORT_SYMBOL(ccci_get_per_md_data);
