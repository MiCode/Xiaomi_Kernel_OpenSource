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

#ifndef __MTK_RSC_V4L2__
#define __MTK_RSC_V4L2__

#include <linux/types.h>
#include "mtk_rsc-ctx.h"

#define MTK_RSC_DEV_NAME     "MTK-RSC-V4L2"

/* Input: rrzo image */
#define MTK_ISP_CTX_RSC_PRE_RRZO_IN             (0)
#define MTK_ISP_CTX_RSC_CUR_RRZO_IN             (1)
/* Input: binary parameters */
#define MTK_ISP_CTX_RSC_TUNING_IN               (2)
#define MTK_ISP_CTX_RSC_TOTAL_OUTPUT            (3)

/* OUT: Main output for still or video */
#define MTK_ISP_CTX_RSC_RESULT_OUT              (3)
#define MTK_ISP_CTX_RSC_TOTAL_CAPTURE           (1)

#define MTK_ISP_CTX_RSC_TUNING_DATA_NUM         (23)



int mtk_isp_ctx_rsc_init(struct mtk_rsc_ctx *ctx);

#endif /*__MTK_RSC_V4L2__*/
