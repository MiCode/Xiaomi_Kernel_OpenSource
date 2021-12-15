/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef _CCU_QOS_H_
#define _CCU_QOS_H_
#include "ccu_reg.h"

void ccu_qos_init(void);
void ccu_qos_update_req(uint32_t *ccu_bw);
void ccu_qos_uninit(void);


#endif
