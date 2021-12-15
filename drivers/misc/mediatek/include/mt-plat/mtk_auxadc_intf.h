/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __LINUX_MTK_AUXADC_INTF_H
#define __LINUX_MTK_AUXADC_INTF_H

#include <mach/mtk_pmic.h>

/* =========== User Layer ================== */
/*
 * pmic_get_auxadc_value(legacy PMIC AUXADC interface)
 * if return value < 0 -> means get data fail
 */
extern int pmic_get_auxadc_value(int list);

#endif /* __LINUX_MTK_AUXADC_INTF_H */
