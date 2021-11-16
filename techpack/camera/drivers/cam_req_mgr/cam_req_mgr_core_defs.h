/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 */
#ifndef _CAM_REQ_MGR_CORE_DEFS_H_
#define _CAM_REQ_MGR_CORE_DEFS_H_

#define CRM_TRACE_ENABLE 0
#define CRM_DEBUG_MUTEX 0

#define SET_SUCCESS_BIT(ret, pd)  (ret |= (1 << (pd)))

#define SET_FAILURE_BIT(ret, pd)  (ret &= (~(1 << (pd))))

#define CRM_GET_REQ_ID(in_q, idx) in_q->slot[idx].req_id

#endif
