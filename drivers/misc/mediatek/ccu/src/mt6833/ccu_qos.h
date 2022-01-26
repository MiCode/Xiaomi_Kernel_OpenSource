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
#ifndef _CCU_QOS_H_
#define _CCU_QOS_H_
#include "ccu_reg.h"

#define CONFIG_MTK_QOS_SUPPORT_ENABLE 1

void ccu_qos_init(void);
void ccu_qos_update_req(uint32_t *ccu_bw);
void ccu_qos_uninit(void);


#endif
