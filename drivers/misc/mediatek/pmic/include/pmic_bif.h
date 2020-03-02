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

#ifndef __MTK_BIF_INTF_H
#define __MTK_BIF_INTF_H

extern int mtk_bif_init(void);
extern int mtk_bif_get_vbat(int *vbat);
extern int mtk_bif_get_tbat(int *tbat);
extern bool mtk_bif_is_hw_exist(void);
extern int pmic_bif_init(void);

#endif /* __MTK_BIF_INTF_H */
