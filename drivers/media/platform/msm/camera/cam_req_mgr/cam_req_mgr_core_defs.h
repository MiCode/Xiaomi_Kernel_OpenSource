/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _CAM_REQ_MGR_CORE_DEFS_H_
#define _CAM_REQ_MGR_CORE_DEFS_H_

#define CRM_TRACE_ENABLE 0
#define CRM_DEBUG_MUTEX 0

#define SET_SUCCESS_BIT(ret, pd)  (ret |= (1 << (pd)))

#define SET_FAILURE_BIT(ret, pd)  (ret &= (~(1 << (pd))))

#define CRM_GET_REQ_ID(in_q, idx) in_q->slot[idx].req_id

#endif

