/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Shi.Ma <shi.ma@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_MMC_MT6771_H
#define _DT_BINDINGS_MMC_MT6771_H

#define MSDC_EMMC               (0)
#define MSDC_SD                 (1)
#define MSDC_SDIO               (2)

#define MSDC_CD_HIGH            (1)
#define MSDC_CD_LOW             (0)

#define MSDC50_CLKSRC_26MHZ     (0)
#define MSDC50_CLKSRC_400MHZ    (1)
#define MSDC50_CLKSRC_200MHZ    (2)
#define MSDC50_CLKSRC_156MHZ    (3)
#define MSDC50_CLKSRC_182MHZ    (4)
#define MSDC50_CLKSRC_312MHZ    (5)

#define MSDC30_CLKSRC_26MHZ     (0)
#define MSDC30_CLKSRC_208MHZ    (1)
#define MSDC30_CLKSRC_182MHZ    (2)
#define MSDC30_CLKSRC_156MHZ    (3)
#define MSDC30_CLKSRC_200MHZ    (4)

#define MSDC_SMPL_RISING        (0)
#define MSDC_SMPL_FALLING       (1)
#define MSDC_SMPL_SEPARATE      (2)

#endif /* _DT_BINDINGS_MMC_MT6771_H */
