/*
 * mddp_wifi_def.h -- Data structure of MD WiFi module.
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef __MDDP_WIFI_DEF_H
#define __MDDP_WIFI_DEF_H

enum wfpm_func_mode_e {
	WFPM_FUNC_MODE_TETHER = 0,
	WFPM_FUNC_MODE_MAX_NUM
};

#define MAC_ADDR_LEN            6
struct mddpwh_txd_t {
	uint8_t version;
	uint8_t index;
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t txd_length;
	uint8_t txd[0];
} __packed;

struct mddpwh_net_stat_t {
	uint64_t        tx_packets;
	uint64_t        rx_packets;
	uint64_t        tx_bytes;
	uint64_t        rx_bytes;
	uint64_t        tx_errors;
	uint64_t        rx_errors;
};

#define MAX_STAREC_NUM          32 // Max driver support station records
#define MAX_CLIENT_NUM          16 // AP client only support 16 station records
#define VIRTUAL_BUF_SIZE        512 // 4096 bits
#define TID_SIZE                8

/* Rx Reordering - AP to MD */
struct mddpwh_ap_virtual_buf_t {
	uint16_t        start_idx;
	uint16_t        end_idx;
	uint8_t         virtual_buf[VIRTUAL_BUF_SIZE];
};

struct mddpwh_ap_reorder_sync_table_t {
	// Mapping station record and virtual buffer.
	struct mddpwh_ap_virtual_buf_t  virtual_buf[MAX_CLIENT_NUM][TID_SIZE];
};

/* Rx Reordering - MD to AP */
struct mddpwh_md_reorder_info_t {
	uint8_t         buf_idx;
	uint8_t         reserved[7];
};

struct mddpwh_md_virtual_buf_t {
	uint16_t        start_idx;
	uint16_t        end_idx;
	uint8_t         virtual_buf[VIRTUAL_BUF_SIZE];
};

struct mddpwh_md_reorder_sync_table_t {
	// Mapping station record and virtual buffer.
	struct mddpwh_md_reorder_info_t reorder_info[MAX_STAREC_NUM];
	struct mddpwh_md_virtual_buf_t  virtual_buf[MAX_CLIENT_NUM][TID_SIZE];
};

enum mddpwh_drv_info_id {
	MDDPWH_DRV_INFO_NONE       = 0,
	MDDPWH_DRV_INFO_DEVICE_MAC = 1,
};

struct mddpwh_drv_info_t {
	uint8_t         info_id;
	uint8_t         info_len;
	uint8_t         info[0];
};

struct mddpwh_drv_notify_info_t {
	uint8_t         version;
	uint8_t         buf_len;
	uint8_t         info_num;
	uint8_t         buf[0];
};

typedef int32_t (*mddpwh_cbf_add_txd_t)(struct mddpwh_txd_t *);
typedef int32_t (*mddpwh_cbf_get_net_stat_t)(struct mddpwh_net_stat_t *);
typedef int32_t (*mddpwh_cbf_get_ap_rx_reorder_buf_t)(
		struct mddpwh_ap_reorder_sync_table_t **);
typedef int32_t (*mddpwh_cbf_get_md_rx_reorder_buf_t)(
		struct mddpwh_md_reorder_sync_table_t **);
typedef int32_t (*mddpwh_cbf_notify_drv_info_t)(
		struct mddpwh_drv_notify_info_t *);

struct mddpwh_drv_handle_t {
	/* MDDPWH invokes these APIs provided by driver. */

	/* Driver invokes these APIs provided by MDDPWH. */
	mddpwh_cbf_add_txd_t                    add_txd;
	mddpwh_cbf_get_net_stat_t               get_net_stat;
	mddpwh_cbf_get_ap_rx_reorder_buf_t      get_ap_rx_reorder_buf;
	mddpwh_cbf_get_md_rx_reorder_buf_t      get_md_rx_reorder_buf;
	mddpwh_cbf_notify_drv_info_t            notify_drv_info;
};

enum mddp_md_smem_user_id_e {
	MDDP_MD_SMEM_USER_RX_REORDER_TO_MD,
	MDDP_MD_SMEM_USER_RX_REORDER_FROM_MD,
	MDDP_MD_SMEM_USER_WIFI_STATISTICS,

	MDDP_MD_SMEM_USER_NUM,
};

enum wfpm_smem_entry_attri_e {
	WFPM_SM_E_ATTRI_RO = (1 << 0),
	WFPM_SM_E_ATTRI_WO = (1 << 1),
};

struct wfpm_smem_info_t {
	uint8_t user_id;
	uint8_t reserved;
	uint16_t attribute;
	uint32_t offset;
	uint32_t size;
};

struct wfpm_enable_md_func_req_t {
	uint8_t                 mode;
	uint8_t                 version;
	uint16_t                smem_num;
	struct wfpm_smem_info_t smem_info[0];
} __packed;

struct wfpm_enable_md_func_rsp_t {
	uint8_t mode;
	uint8_t result;
	uint8_t version;
	uint8_t reserved;
} __packed;

struct wfpm_activate_md_func_req_t {
	uint8_t mode;
	uint8_t reserved[3];
} __packed;

struct wfpm_deactivate_md_func_rsp_t {
	uint8_t mode;
	uint8_t result;
	uint8_t reserved[2];
} __packed;

struct wfpm_md_fast_path_common_req_t {
	uint8_t mode;
	uint8_t reserved[3];
} __packed;

struct wfpm_md_fast_path_common_rsp_t {
	uint8_t mode;
	uint8_t result;
	uint8_t reserved[2];
} __packed;

#endif /* __MDDP_WIFI_DEF_H */
