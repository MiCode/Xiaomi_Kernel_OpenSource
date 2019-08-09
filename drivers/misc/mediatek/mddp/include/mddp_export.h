/*
 * mddp_export.h - Public API/structure provided for external modules.
 *
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

#ifndef __MDDP_EXPORT_H
#define __MDDP_EXPORT_H

#include <linux/netdevice.h>
#include "mddp_usb_def.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
#define __MDDP_VERSION__             2
#define MDDP_TAG_PATTERN            0x4646

#define MAX_USB_RET_BUF_SZ          256

enum mddp_state_e {
	MDDP_STATE_UNINIT = 0,
	MDDP_STATE_ENABLING,
	MDDP_STATE_DEACTIVATED,
	MDDP_STATE_ACTIVATING,
	MDDP_STATE_ACTIVATED,
	MDDP_STATE_DEACTIVATING,
	MDDP_STATE_DISABLING,

	MDDP_STATE_CNT,
	MDDP_STATE_DUMMY = 0x7fff /* Make it a 2-byte enum. */
};

enum mddp_app_type_e {
	MDDP_APP_TYPE_USB = 0,
	MDDP_APP_TYPE_WH,

	MDDP_APP_TYPE_CNT,

	MDDP_APP_TYPE_ALL = 0xff,
	MDDP_APP_TYPE_DUMMY = 0x7fff /* Mark it a 2-byte enum. */
};

struct mddp_drv_conf_t {
	enum mddp_app_type_e app_type;
};

typedef int32_t (*drv_cbf_change_state_t)(enum mddp_state_e);

#include "mddp_wifi_def.h"
struct mddp_drv_handle_t {
	/* MDDP invokes these APIs provided by driver. */
	drv_cbf_change_state_t          change_state;

	/* Driver invokes these APIs provided by MDDP. */

	/* Application layer handler. */
	union {
		struct mddpwh_drv_handle_t    *wh_handle;
	};
};

//------------------------------------------------------------------------------
// Public function definition.
// -----------------------------------------------------------------------------
/* MD to AP CCCI_IPC callback function */
//int mddp_md_msg_hdlr(struct ipc_ilm *ilm);
int mddp_md_msg_hdlr(void *in_ilm);

//------------------------------------------------------------------------------
// Public function definition - For driver
// -----------------------------------------------------------------------------
int32_t mddp_drv_attach(
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

	/* CMCMD Response */
	MDDP_CMCMD_RSP_BEGIN = 0x100,
	MDDP_CMCMD_ENABLE_RSP = MDDP_CMCMD_RSP_BEGIN,
	MDDP_CMCMD_DISABLE_RSP,
	MDDP_CMCMD_ACT_RSP,
	MDDP_CMCMD_DEACT_RSP,
	MDDP_CMCMD_RSP_END,

	MDDP_CMCMD_DUMMY = 0x7fff /* Mark it a 2-byte enum */
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
 * MDDP data rule structure.
 */
struct mddp_f_nat_table_entry {
	uint8_t                 private_ip[4];
	uint16_t                private_port;

	uint8_t                 target_ip[4];
	uint16_t                target_port;

	uint8_t                 public_ip[4];
	uint16_t                public_port;

	uint8_t                 protocol;
	uint32_t                timestamp;

	bool                    dst_nat;
};

/*
 * MDDP tag structure.
 */
struct mddp_f_tag_packet_t {
	u_int16_t	guard_pattern;
	u_int8_t	version;
	u_int8_t	reserved;
	union {
		struct {
			u_int8_t	in_netif_id;
			u_int8_t	out_netif_id;
			u_int16_t	port;
		} v1;
		struct {
			u_int8_t	tag_info;
			u_int8_t	reserved;
			u_int16_t	port;
			u_int32_t	lan_netif_id;
			u_int32_t	ip;
		} v2;
	};
};

#endif /* __MDDP_EXPORT_H */
