/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_USAGE_H
#define __MDDP_USAGE_H

#ifdef CONFIG_MTK_MDDP_WH_SUPPORT

#include "mddp_export.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
enum mddp_data_usage_cmd_e {
	MSG_ID_DPFM_SET_GLOBAL_ALERT_REQ,
	MSG_ID_DPFM_ALERT_GLOBAL_ALERT_IND,
	MSG_ID_DPFM_SET_IQUOTA_REQ,
	MSG_ID_DPFM_ALERT_IQUOTA_IND,
	MSG_ID_DPFM_DEL_IQUOTA_REQ,
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
void mddp_u_set_wan_iface(uint8_t *devname);
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

#define mddp_u_set_wan_iface(x) 0
#define mddp_u_set_data_limit(x, y) 0
#define mddp_u_msg_hdlr() 0

#endif /* CONFIG_MTK_MDDP_WH_SUPPORT */

#endif /* __MDDP_USAGE_H */
