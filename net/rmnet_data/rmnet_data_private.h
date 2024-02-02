/* Copyright (c) 2013-2014, 2016 The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_DATA_PRIVATE_H_
#define _RMNET_DATA_PRIVATE_H_

#define RMNET_DATA_MAX_VND              32
#define RMNET_DATA_MAX_PACKET_SIZE      16384
#define RMNET_DATA_DFLT_PACKET_SIZE     1500
#define RMNET_DATA_DEV_NAME_STR         "rmnet_data"
#define RMNET_DATA_NEEDED_HEADROOM      16
#define RMNET_DATA_TX_QUEUE_LEN         1000
#define RMNET_ETHERNET_HEADER_LENGTH    14

extern unsigned int rmnet_data_log_level;
extern unsigned int rmnet_data_log_module_mask;

#define RMNET_INIT_OK     0
#define RMNET_INIT_ERROR  1

#define RMNET_LOG_LVL_DBG BIT(4)
#define RMNET_LOG_LVL_LOW BIT(3)
#define RMNET_LOG_LVL_MED BIT(2)
#define RMNET_LOG_LVL_HI  BIT(1)
#define RMNET_LOG_LVL_ERR BIT(0)

#define RMNET_LOG_MODULE(X) \
	static u32 rmnet_mod_mask = X

#define RMNET_DATA_LOGMASK_CONFIG  BIT(0)
#define RMNET_DATA_LOGMASK_HANDLER BIT(1)
#define RMNET_DATA_LOGMASK_VND     BIT(2)
#define RMNET_DATA_LOGMASK_MAPD    BIT(3)
#define RMNET_DATA_LOGMASK_MAPC    BIT(4)

#define LOGE(fmt, ...) do { if (rmnet_data_log_level & RMNET_LOG_LVL_ERR) \
			pr_err("[RMNET:ERR] %s(): " fmt "\n", __func__, \
				##__VA_ARGS__); \
			} while (0)

#define LOGH(fmt, ...) do { if (rmnet_data_log_level & RMNET_LOG_LVL_HI) \
			pr_err("[RMNET:HI] %s(): " fmt "\n", __func__, \
				##__VA_ARGS__); \
			} while (0)

#define LOGM(fmt, ...) do { if (rmnet_data_log_level & RMNET_LOG_LVL_MED) \
			pr_warn("[RMNET:MED] %s(): " fmt "\n", __func__, \
				##__VA_ARGS__); \
			} while (0)

#define LOGL(fmt, ...) do { if (unlikely \
			(rmnet_data_log_level & RMNET_LOG_LVL_LOW)) \
			pr_notice("[RMNET:LOW] %s(): " fmt "\n", __func__, \
				##__VA_ARGS__); \
			} while (0)

/* Don't use pr_debug as it is compiled out of the kernel. We can be sure of
 * minimal impact as LOGD is not enabled by default.
 */
#define LOGD(fmt, ...) do { if (unlikely( \
			    (rmnet_data_log_level & RMNET_LOG_LVL_DBG) && \
			    (rmnet_data_log_module_mask & rmnet_mod_mask))) \
			pr_notice("[RMNET:DBG] %s(): " fmt "\n", __func__, \
				  ##__VA_ARGS__); \
			} while (0)

#endif /* _RMNET_DATA_PRIVATE_H_ */
