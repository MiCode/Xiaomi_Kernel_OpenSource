/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_JPEG_PARSE_H
#define _MTK_JPEG_PARSE_H

#include "mtk_jpeg_dec_hw.h"

bool mtk_jpeg_parse(struct mtk_jpeg_dec_param *param, u8 *src_addr_va,
		    u32 src_size);

#endif /* _MTK_JPEG_PARSE_H */

