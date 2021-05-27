/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CCU_QOS_H_
#define _CCU_QOS_H_
#include "ccu_cmn.h"
#define CCU_QOS_SUPPORT_ENABLE 1

void ccu_qos_init(struct ccu_device_s *ccu);
void ccu_qos_update_req(struct ccu_device_s *ccu, uint32_t *ccu_bw);
void ccu_qos_uninit(struct ccu_device_s *ccu);


#endif
