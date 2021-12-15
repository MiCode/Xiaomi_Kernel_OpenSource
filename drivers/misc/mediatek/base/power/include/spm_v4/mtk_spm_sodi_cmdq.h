/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_SPM_SODI_CMDQ_H__
#define __MTK_SPM_SODI_CMDQ_H__

#if !defined(SPM_K414_EARLY_PORTING)
#include <cmdq_def.h>
#include <cmdq_record.h>
#include <cmdq_reg.h>
#include <cmdq_core.h>

void exit_pd_by_cmdq(struct cmdqRecStruct *handler);
void enter_pd_by_cmdq(struct cmdqRecStruct *handler);
#endif

#endif /* __MTK_SPM_SODI_CMDQ_H__ */
