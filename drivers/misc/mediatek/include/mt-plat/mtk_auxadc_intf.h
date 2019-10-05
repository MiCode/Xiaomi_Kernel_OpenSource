/*
 * Copyright (C) 2018 MediaTek Inc.
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
