/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_DCM_H__
#define __MTK_DCM_H__
#include "mtk_dcm_common.h"

void mt_dcm_array_register(struct DCM *array, struct DCM_OPS *ops);
int mt_dcm_common_init(void);
bool is_dcm_initialized(void);
void mt_dcm_disable(void);
void mt_dcm_restore(void);
void dcm_disable(unsigned int type);
void dcm_dump_state(int type);

#endif /* #ifndef __MTK_DCM_H__ */

