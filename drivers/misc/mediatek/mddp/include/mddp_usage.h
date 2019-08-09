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

#ifndef __MDDP_USAGE_H
#define __MDDP_USAGE_H

#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT) || \
defined(CONFIG_MTK_MDDP_WH_SUPPORT)


#include <linux/netdevice.h>

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
enum mddp_data_usage_cmd_e {
	MSG_ID_MDT_SET_GLOBAL_ALERT_REQ,
	MSG_ID_MDT_ALERT_GLOBAL_ALERT_IND,
	MSG_ID_MDT_SET_IQUOTA_REQ,
	MSG_ID_MDT_ALERT_IQUOTA_IND,
	MSG_ID_MDT_DEL_IQUOTA_REQ,
};

struct mddp_u_data_limit_t {
	uint32_t        cmd;
	uint32_t        trans_id;
	uint32_t        status;     /*unused */
	uint64_t        limit_buffer_size;
	int8_t          id;
	uint8_t         rsv[3];
} __packed;

struct mddp_u_iquota_t {
	struct          delayed_work dwork;
	bool            is_add;
	struct          mdt_data_limitation_t *user_data;
	spinlock_t      locker;
} __packed;

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_usage_init(void);
void mddp_usage_uninit(void);
void mddp_u_get_data_stats(void *buf, uint32_t *buf_len);
int32_t mddp_u_set_data_limit(uint8_t *buf, uint32_t buf_len);
int32_t mddp_u_msg_hdlr(void);

#else

/*
 * Null definition if current platform does not support DATA USAGE.
 */
#define mddp_usage_init() 0
#define mddp_usage_uninit()
#define mddp_u_get_data_stats(x, y)
#define mddp_u_set_data_limit(x, y) 0
#define mddp_u_msg_hdlr() 0

#endif /* MTK_MD_DIRECT_TETHERING_SUPPORT or MTK_MDDP_WH_SUPPORT */

#endif /* __MDDP_USAGE_H */
