/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_export.h - Public API/structure provided for external modules.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_EXPORT_H
#define __MDDP_EXPORT_H

#include <linux/if.h>

#include "mddp_wifi_def.h"
//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
#define __MDDP_VERSION__            2
#define MDDP_TAG_PATTERN            0x4646

#define MDDP_MAX_GET_BUF_SZ         256

enum mddp_state_e {
	MDDP_STATE_UNINIT = 0,
	MDDP_STATE_ENABLING,
	MDDP_STATE_DEACTIVATED,
	MDDP_STATE_ACTIVATING,
	MDDP_STATE_ACTIVATED,
	MDDP_STATE_DEACTIVATING,
	MDDP_STATE_DISABLING,
	MDDP_STATE_DISABLED,

	MDDP_STATE_CNT,
	MDDP_STATE_DUMMY = 0x7fff /* Make it a 2-byte enum. */
};

enum mddp_app_type_e {
	MDDP_APP_TYPE_RESERVED_1 = 0,
	MDDP_APP_TYPE_WH,

	MDDP_APP_TYPE_CNT,

	MDDP_APP_TYPE_ALL = 0xff,
	MDDP_APP_TYPE_DUMMY = 0x7fff /* Mark it a 2-byte enum. */
};

struct mddp_drv_conf_t {
	enum mddp_app_type_e app_type;
};

typedef int32_t (*drv_cbf_change_state_t)(
	enum mddp_state_e state, void *buf, uint32_t *buf_len);

struct mddp_drv_handle_t {
	/* MDDP invokes these APIs provided by driver. */
	drv_cbf_change_state_t          change_state;

	/* Application layer handler. */
	union {
		struct mddpw_drv_handle_t     *wifi_handle;
	};
};

//------------------------------------------------------------------------------
// Public function definition - For driver
// -----------------------------------------------------------------------------
int32_t mddp_drv_attach(
	struct mddp_drv_conf_t *conf, struct mddp_drv_handle_t *handle);
void mddp_drv_detach(
	struct mddp_drv_conf_t *conf, struct mddp_drv_handle_t *handle);

//------------------------------------------------------------------------------
// Public function definition - For HIDL
// -----------------------------------------------------------------------------
/*
 * MDDP DEV control code.
 */
#define MDDP_CMCMD_MAGIC                'P'
#define MDDP_CMCMD_DEV_REQ              _IORW(MDDP_CMCMD_MAGIC, 0)
#define MDDP_CTRL_MSG_MCODE             0x2454
#define MDDP_CTRL_MSG_VERSION           1

enum mddp_dev_evt_type_e {
	MDDP_DEV_EVT_NONE = 0,
	MDDP_DEV_EVT_STARTED = 1,
	MDDP_DEV_EVT_STOPPED_ERROR = 2,
	MDDP_DEV_EVT_STOPPED_UNSUPPORTED = 3,
	MDDP_DEV_EVT_SUPPORT_AVAILABLE = 4,
	MDDP_DEV_EVT_STOPPED_LIMIT_REACHED = 5,
	MDDP_DEV_EVT_CONNECT_UPDATE = 6,
	MDDP_DEV_EVT_WARNING_REACHED = 7,

	MDDP_DEV_EVENT_CNT,
	MDDP_DEV_EVENT_DUMMY = 0x7fff /* Mark it a 2-byte enum */
};

enum mddp_ctrl_msg_e {
	/* CMCMD Request */
	MDDP_CMCMD_ENABLE_REQ = 0,
	MDDP_CMCMD_DISABLE_REQ,
	MDDP_CMCMD_ACT_REQ,
	MDDP_CMCMD_DEACT_REQ,
	MDDP_CMCMD_GET_OFFLOAD_STATS_REQ,
	MDDP_CMCMD_SET_DATA_LIMIT_REQ,
	MDDP_CMCMD_SET_CT_VALUE_REQ,
	MDDP_CMCMD_SET_WARNING_AND_DATA_LIMIT_REQ,

	/* CMCMD Response */
	MDDP_CMCMD_RSP_BEGIN = 0x100,
	MDDP_CMCMD_ENABLE_RSP = MDDP_CMCMD_RSP_BEGIN,
	MDDP_CMCMD_DISABLE_RSP,
	MDDP_CMCMD_ACT_RSP,
	MDDP_CMCMD_DEACT_RSP,
	MDDP_CMCMD_LIMIT_IND,
	MDDP_CMCMD_CT_IND,
	MDDP_CMCMD_WARNING_IND,
	MDDP_CMCMD_RSP_END,

	MDDP_CMCMD_DUMMY = 0x7fff /* Mark it a 2-byte enum */
};

enum mddp_f_e_tag_type_e {
	MDDP_E_TAG_NONE = 0,
	MDDP_E_TAG_MAC  = 1,
};

/*
 * MDDP control request structure.
 */
struct mddp_dev_req_common_t {
	uint16_t                mcode;
	uint16_t                version;
	enum mddp_app_type_e    app_type;
	enum mddp_ctrl_msg_e    msg;
	uint32_t                data_len;
	uint8_t                 data[0];
};

struct mddp_dev_req_enable_t {
	uint32_t                rsv;
};

struct mddp_dev_req_disable_t {
	uint32_t                rsv;
};

struct mddp_dev_req_act_t {
	uint8_t                 ul_dev_name[IFNAMSIZ];
	uint8_t                 dl_dev_name[IFNAMSIZ];
};

struct mddp_dev_req_deact_t {
	uint32_t                rsv;
};

struct mddp_dev_req_set_data_limit_t {
	uint8_t                 ul_dev_name[IFNAMSIZ];
	uint64_t                limit_size; /* Bytes */
};

struct mddp_dev_req_set_warning_and_data_limit_t {
	uint8_t                 ul_dev_name[IFNAMSIZ];
	uint64_t                limit_size; /* Bytes */
	uint64_t                warning_size; /* Bytes */
};

struct mddp_dev_req_set_ct_value_t {
	uint32_t                udp_ct_timeout;
	uint32_t                tcp_ct_timeout;
};

/*
 * MDDP control response structure.
 */
struct mddp_dev_rsp_common_t {
	uint16_t                mcode;
	uint16_t                status;
	enum mddp_app_type_e    app_type;
	enum mddp_ctrl_msg_e    msg;
	uint32_t                data_len;
	uint8_t                 data[0];
};

struct mddp_dev_rsp_enable_t {
	uint32_t                rsv;
};

struct mddp_dev_rsp_disable_t {
	uint32_t                rsv;
};

struct mddp_dev_rsp_act_t {
	uint32_t                rsv;
};

struct mddp_dev_rsp_deact_t {
	uint32_t                rsv;
};

/*
 * MDDP data statistics structure.
 */
struct mddp_u_data_stats_t {
	uint64_t        total_tx_bytes;
	uint64_t        total_tx_pkts;
	uint64_t        tx_tcp_bytes;
	uint64_t        tx_tcp_pkts;
	uint64_t        tx_udp_bytes;
	uint64_t        tx_udp_pkts;
	uint64_t        tx_others_bytes;
	uint64_t        tx_others_pkts;
	uint64_t        total_rx_bytes;
	uint64_t        total_rx_pkts;
	uint64_t        rx_tcp_bytes;
	uint64_t        rx_tcp_pkts;
	uint64_t        rx_udp_bytes;
	uint64_t        rx_udp_pkts;
	uint64_t        rx_others_bytes;
	uint64_t        rx_others_pkts;
};

/*
 * MDDP rule info. for connection track update
 */
struct mddp_ct_nat_table_t {
	uint8_t                 private_ip[4];
	uint8_t                 target_ip[4];
	uint8_t                 public_ip[4];
	uint16_t                private_port;
	uint16_t                target_port;
	uint16_t                public_port;
	uint8_t                 protocol;
	uint8_t                 reserved;
	uint32_t                timestamp;
	bool                    dst_nat;
};

#define MAX_CT_NAT_TABLE_NUM            64
struct mddp_ct_timeout_ind_t {
	uint16_t                        entry_num;
	struct mddp_ct_nat_table_t      nat_table[MAX_CT_NAT_TABLE_NUM];
};

/*
 * MDDP tag structure.
 */
struct mddp_f_tag_packet_t {
	u_int16_t	guard_pattern;
	u_int8_t	version;
	u_int8_t	tag_len;    // extension tag included
	struct {
		u_int8_t	tag_info;
		u_int8_t	reserved;
		u_int16_t	port;
		u_int32_t	lan_netif_id;
		u_int32_t	ip;
	} v2;
};

/*
 * Extended MDDP tag structure - COMMON
 */
struct mddp_f_e_tag_common_t {
	u_int8_t    type;
	u_int8_t    len;
	u_int8_t    value[0];
} __packed;

/*
 * Extended MDDP tag structure - MAC
 */
struct mddp_f_e_tag_mac_t {
	u_int8_t    mac_addr[6];
	u_int32_t   access_cnt;
} __packed;

#endif /* __MDDP_EXPORT_H */
