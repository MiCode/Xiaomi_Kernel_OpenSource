/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_wifi_def.h -- Data structure of MD WiFi module.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_WIFI_DEF_H
#define __MDDP_WIFI_DEF_H

enum wfpm_func_mode_e {
	WFPM_FUNC_MODE_TETHER = 0,
	WFPM_FUNC_MODE_MAX_NUM
};

#define WIFI_ONOFF_NOTIFICATION_LEN 1
#define MAC_ADDR_LEN            6
struct mddpw_txd_t {
	uint8_t version;
	uint8_t wlan_idx;
	uint8_t sta_idx;
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t txd_length;
	uint8_t txd[0];
} __packed;

struct mddpw_net_stat_t {
	uint64_t        tx_packets;
	uint64_t        rx_packets;
	uint64_t        tx_bytes;
	uint64_t        rx_bytes;
	uint64_t        tx_errors;
	uint64_t        rx_errors;
};

#define NW_IF_NAME_LEN_MAX      16
struct mddpw_net_stat_elem_ext_t {
	uint8_t         nw_if_name[NW_IF_NAME_LEN_MAX];
	uint32_t        reserved[2];
	uint64_t        tx_packets;
	uint64_t        rx_packets;
	uint64_t        tx_bytes;
	uint64_t        rx_bytes;
	uint64_t        tx_errors;
	uint64_t        rx_errors;
	uint64_t        tx_dropped;
	uint64_t        rx_dropped;
};

#define MDDP_DOUBLE_BUFFER      2
#define NW_IF_NUM_MAX           4
struct mddpw_net_stat_ext_t {
	uint32_t                         version;
	uint32_t                         reserved;
	uint32_t                         check_flag[2];
	struct mddpw_net_stat_elem_ext_t ifs[MDDP_DOUBLE_BUFFER][NW_IF_NUM_MAX];
};

struct mddpw_sys_stat_t {
	uint32_t        version[2];
	uint32_t        md_stat[4];
	uint32_t        ap_stat[4];
	uint32_t        reserved[2];
};

#define MAX_STAREC_NUM          32 // Max driver support station records
#define MAX_CLIENT_NUM          16 // AP client only support 16 station records
#define VIRTUAL_BUF_SIZE        512 // 4096 bits
#define TID_SIZE                8

/* Rx Reordering - AP to MD */
struct mddpw_ap_virtual_buf_t {
	uint16_t        start_idx;
	uint16_t        end_idx;
	uint8_t         virtual_buf[VIRTUAL_BUF_SIZE];
};

struct mddpw_ap_reorder_sync_table_t {
	// Mapping station record and virtual buffer.
	struct mddpw_ap_virtual_buf_t  virtual_buf[MAX_CLIENT_NUM][TID_SIZE];
};

/* Rx Reordering - MD to AP */
struct mddpw_md_reorder_info_t {
	uint8_t         buf_idx;
	uint8_t         reserved[7];
};

struct mddpw_md_virtual_buf_t {
	uint16_t        start_idx;
	uint16_t        end_idx;
	uint8_t         virtual_buf[VIRTUAL_BUF_SIZE];
};

struct mddpw_md_reorder_sync_table_t {
	// Mapping station record and virtual buffer.
	struct mddpw_md_reorder_info_t reorder_info[MAX_STAREC_NUM];
	struct mddpw_md_virtual_buf_t  virtual_buf[MAX_CLIENT_NUM][TID_SIZE];
};

enum mddpw_drv_info_id {
	MDDPW_DRV_INFO_NONE       = 0,
	MDDPW_DRV_INFO_DEVICE_MAC = 1,
	MDDPW_DRV_INFO_NOTIFY_WIFI_ONOFF = 2,
};

struct mddpw_drv_info_t {
	uint8_t         info_id;
	uint8_t         info_len;
	uint8_t         info[0];
};

struct mddpw_drv_notify_info_t {
	uint8_t         version;
	uint8_t         buf_len;
	uint8_t         info_num;
	uint8_t         buf[0];
};

struct mddpw_md_notify_info_t {
	uint8_t         version;      /* current ver = 0 */
	uint8_t         info_type;    /* ref enum mddp_md_notify_info_type */
	uint8_t         buf_len;      /* length start from buf[0] */
	uint8_t         buf[0];       /* content that MD need to send to DRV */
};

typedef int32_t (*drv_cbf_notify_md_info_t) (
		struct mddpw_md_notify_info_t *);

typedef int32_t (*mddpw_cbf_add_txd_t)(struct mddpw_txd_t *);
typedef int32_t (*mddpw_cbf_get_net_stat_t)(struct mddpw_net_stat_t *);
typedef int32_t (*mddpw_cbf_get_ap_rx_reorder_buf_t)(
		struct mddpw_ap_reorder_sync_table_t **);
typedef int32_t (*mddpw_cbf_get_md_rx_reorder_buf_t)(
		struct mddpw_md_reorder_sync_table_t **);
typedef int32_t (*mddpw_cbf_notify_drv_info_t)(
		struct mddpw_drv_notify_info_t *);
typedef int32_t (*mddpw_cbf_get_net_stat_ext_t)(struct mddpw_net_stat_ext_t *);
typedef int32_t (*mddpw_cbf_get_sys_stat_t)(struct mddpw_sys_stat_t **);
typedef int32_t (*mddpw_cbf_get_mddp_feature_t)(void);

struct mddpw_drv_handle_t {
	/* MDDPW invokes these APIs provided by driver. */
	drv_cbf_notify_md_info_t               notify_md_info;

	/* Driver invokes these APIs provided by MDDPW. */
	mddpw_cbf_add_txd_t                    add_txd;
	mddpw_cbf_get_net_stat_t               get_net_stat;
	mddpw_cbf_get_ap_rx_reorder_buf_t      get_ap_rx_reorder_buf;
	mddpw_cbf_get_md_rx_reorder_buf_t      get_md_rx_reorder_buf;
	mddpw_cbf_notify_drv_info_t            notify_drv_info;
	mddpw_cbf_get_net_stat_ext_t           get_net_stat_ext;
	mddpw_cbf_get_sys_stat_t               get_sys_stat;
	mddpw_cbf_get_mddp_feature_t           get_mddp_feature;
};

enum mddp_md_smem_user_id_e {
	MDDP_MD_SMEM_USER_RX_REORDER_TO_MD,
	MDDP_MD_SMEM_USER_RX_REORDER_FROM_MD,
	MDDP_MD_SMEM_USER_WIFI_STATISTICS,
	MDDP_MD_SMEM_USER_WIFI_STATISTICS_EXT,
	MDDP_MD_SMEM_USER_SYS_STAT_SYNC,

	MDDP_MD_SMEM_USER_NUM,
};

enum wfpm_smem_entry_attri_e {
	WFPM_SM_E_ATTRI_RO = (1 << 0),
	WFPM_SM_E_ATTRI_WO = (1 << 1),
	WFPM_SM_E_ATTRI_RW = (1 << 2),
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

enum mddp_drv_onoff_status {
	MDDPW_DRV_INFO_WLAN_ON_START = 0,
	MDDPW_DRV_INFO_WLAN_ON_END = 1,
	MDDPW_DRV_INFO_WLAN_OFF_START = 2,
	MDDPW_DRV_INFO_WLAN_OFF_END = 3,
	MDDPW_DRV_INFO_WLAN_ON_END_QOS = 4,
};

enum mddp_md_notify_info_type {
	MDDPW_MD_NOTIFY_NONE = 0,
	MDDPW_MD_NOTIFY_MD_POWERON,

	MDDPW_MD_NOTIFY_END,
};

#define MDDP_FEATURE_MCIF_WIFI (1<<1)
#define MDDP_FEATURE_MDDP_WH   (1<<2)

#endif /* __MDDP_WIFI_DEF_H */
