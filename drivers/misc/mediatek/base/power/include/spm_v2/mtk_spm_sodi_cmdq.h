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

#ifndef __MTK_SPM_SODI_CMDQ_H
#define __MTK_SPM_SODI_CMDQ_H

#include <cmdq_def.h>
#include <cmdq_record.h>
#include <cmdq_reg.h>
#include <cmdq_core.h>

void exit_pd_by_cmdq(struct cmdqRecStruct *handler);
void enter_pd_by_cmdq(struct cmdqRecStruct *handler);

#endif /* __MTK_SPM_SODI_CMDQ_H */
