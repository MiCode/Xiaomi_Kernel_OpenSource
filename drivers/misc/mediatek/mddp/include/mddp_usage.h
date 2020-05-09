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

#if defined(MDDP_TETHERING_SUPPORT)

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
	uint32_t                        cmd;
	uint32_t                        trans_id;
	uint32_t                        status;     /*unused */
	uint64_t                        limit_buffer_size;
	int8_t                          id;
	uint8_t                         rsv[3];
} __packed;

struct mddp_u_iq_entry_t {
	uint32_t                        trans_id;
	uint32_t                        status;     /*unused */
	uint64_t                        limit_buffer_size;
	uint8_t                         dpfm_id;
	uint8_t                         rsv[3];
} __packed;

struct mddp_u_iquota_ind_t {
	uint32_t                        cmd;
	struct mddp_u_iq_entry_t        iq;
} __packed;

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_usage_init(void);
void mddp_usage_uninit(void);
void mddp_u_get_data_stats(void *buf, uint32_t *buf_len);
int32_t mddp_u_set_data_limit(uint8_t *buf, uint32_t buf_len);
int32_t mddp_u_msg_hdlr(uint32_t msg_id, void *buf, uint32_t buf_len);

#else

/*
 * Null definition if current platform does not support DATA USAGE.
 */
#define mddp_usage_init() 0
#define mddp_usage_uninit()
#define mddp_u_get_data_stats(x, y) \
	do { \
		*y = sizeof(struct mddp_u_data_stats_t); \
		memset(x, 0, *y); \
	} while (0)

#define mddp_u_set_data_limit(x, y) 0
#define mddp_u_msg_hdlr() 0

#endif /* MDDP_TETHERING_SUPPORT */

#endif /* __MDDP_USAGE_H */
