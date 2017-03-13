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

#if (CRM_TRACE_ENABLE == 1)
	#define CRM_DBG(fmt, args...) do { \
	trace_printk("%d: [crm_dbg] "fmt"\n", __LINE__, ##args); \
	pr_debug("%s:%d "fmt"\n", __func__, __LINE__, ##args); \
	} while (0)

	#define CRM_WARN(fmt, args...) do { \
	trace_printk("%d: [crm_warn] "fmt"\n", __LINE__, ##args); \
	pr_warn("%s:%d "fmt"\n", __func__, __LINE__, ##args); \
	} while (0)

	#define CRM_ERR(fmt, args...) do { \
	trace_printk("%d: [crm_err] "fmt"\n", __LINE__, ##args); \
	pr_err("%s:%d "fmt"\n", __func__, __LINE__, ##args);\
	} while (0)
#else
	#define CRM_DBG(fmt, args...) pr_debug("%s:%d "fmt"\n", \
	__func__, __LINE__, ##args)

	#define CRM_WARN(fmt, args...) pr_warn("%s:%d "fmt"\n", \
	__func__, __LINE__, ##args)

	#define CRM_ERR(fmt, args...) pr_err("%s:%d "fmt"\n", \
	__func__, __LINE__, ##args)
#endif
#endif

