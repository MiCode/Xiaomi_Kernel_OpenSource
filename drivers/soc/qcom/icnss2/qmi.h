/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __ICNSS_QMI_H__
#define __ICNSS_QMI_H__

#include "device_management_service_v01.h"

#define QDSS_TRACE_SEG_LEN_MAX 32
#define QDSS_TRACE_FILE_NAME_MAX 16
#define M3_SEGMENTS_SIZE_MAX 10
#define M3_SEGMENT_NAME_LEN_MAX 16

struct icnss_mem_seg {
	u64 addr;
	u32 size;
};

struct icnss_qmi_event_qdss_trace_save_data {
	u32 total_size;
	u32 mem_seg_len;
	struct icnss_mem_seg mem_seg[QDSS_TRACE_SEG_LEN_MAX];
	char file_name[QDSS_TRACE_FILE_NAME_MAX + 1];
};

struct icnss_m3_segment {
	u32 type;
	u64 addr;
	u64 size;
	char name[M3_SEGMENT_NAME_LEN_MAX + 1];
};

struct icnss_m3_upload_segments_req_data {
	u32 pdev_id;
	u32 no_of_valid_segments;
	struct icnss_m3_segment m3_segment[M3_SEGMENTS_SIZE_MAX];
};

struct icnss_qmi_event_qdss_trace_req_data {
	u32 total_size;
	char file_name[QDSS_TRACE_FILE_NAME_MAX + 1];
};

#ifndef CONFIG_ICNSS2_QMI

static inline int wlfw_ind_register_send_sync_msg(struct icnss_priv *priv)
{
	return 0;
}
static inline int icnss_connect_to_fw_server(struct icnss_priv *priv,
					     void *data)
{
	return 0;
}
static inline int wlfw_msa_mem_info_send_sync_msg(struct icnss_priv *priv)
{
	return 0;
}
static inline int wlfw_msa_ready_send_sync_msg(struct icnss_priv *priv)
{
	return 0;
}
static inline int wlfw_cap_send_sync_msg(struct icnss_priv *priv)
{
	return 0;
}
static inline int wlfw_dynamic_feature_mask_send_sync_msg(
		struct icnss_priv *priv, uint64_t dynamic_feature_mask)
{
	return 0;
}
static inline int icnss_clear_server(struct icnss_priv *priv)
{
	return 0;
}
static inline int wlfw_rejuvenate_ack_send_sync_msg(struct icnss_priv *priv)
{
	return 0;
}
static inline void icnss_ignore_fw_timeout(bool ignore) {}
static int wlfw_send_modem_shutdown_msg(struct icnss_priv *priv)
{
	return 0;
}
static inline int wlfw_ini_send_sync_msg(struct icnss_priv *priv,
		uint8_t fw_log_mode)
{
	return 0;
}
static inline int wlfw_athdiag_read_send_sync_msg(struct icnss_priv *priv,
					   uint32_t offset, uint32_t mem_type,
					   uint32_t data_len, uint8_t *data)
{
	return 0;
}
static inline int wlfw_athdiag_write_send_sync_msg(struct icnss_priv *priv,
					    uint32_t offset, uint32_t mem_type,
					    uint32_t data_len, uint8_t *data)
{
	return 0;
}
static inline int wlfw_wlan_mode_send_sync_msg(struct icnss_priv *priv,
		enum icnss_driver_mode mode)
{
	return 0;
}
static int wlfw_host_cap_send_sync(struct icnss_priv *priv)
{
	return 0;
}
static inline int icnss_send_wlan_enable_to_fw(struct icnss_priv *priv,
		struct icnss_wlan_enable_cfg *config,
		enum icnss_driver_mode mode,
		const char *host_version)
{
	return 0;
}
static inline int icnss_send_wlan_disable_to_fw(struct icnss_priv *priv)
{
	return 0;
}
static inline int icnss_register_fw_service(struct icnss_priv *priv)
{
	return 0;
}
static inline void icnss_unregister_fw_service(struct icnss_priv *priv) {}
static inline int icnss_send_vbatt_update(struct icnss_priv *priv,
					  uint64_t voltage_uv)
{
	return 0;
}

static inline int wlfw_device_info_send_msg(struct icnss_priv *priv)
{
	return 0;
}
int wlfw_wlan_mode_send_sync_msg(struct icnss_priv *priv,
				 enum wlfw_driver_mode_enum_v01 mode)
{
	return 0;
}
int icnss_wlfw_bdf_dnld_send_sync(struct icnss_priv *priv, u32 bdf_type)
{
	return 0;
}

int wlfw_qdss_trace_mem_info_send_sync(struct icnss_priv *priv)
{
	return 0;
}

int wlfw_power_save_send_msg(struct icnss_priv *priv,
			     enum wlfw_power_save_mode_v01 mode)
{
	return 0;
}

int icnss_wlfw_get_info_send_sync(struct icnss_priv *priv, int type,
				  void *cmd, int cmd_len)
{
	return 0;
}

int wlfw_send_soc_wake_msg(struct icnss_priv *priv,
			   enum wlfw_soc_wake_enum_v01 type)
{
	return 0;
}

int icnss_wlfw_m3_dump_upload_done_send_sync(struct icnss_priv *priv,
					     u32 pdev_id, int status)
{
	return 0;
}

int icnss_qmi_get_dms_mac(struct icnss_priv *priv)
{
	return 0;
}

int icnss_wlfw_wlan_mac_req_send_sync(struct icnss_priv *priv,
				      u8 *mac, u32 mac_len)
{
	return 0;
}

int icnss_dms_init(struct icns_priv *priv)
{
	return 0;
}

void icnss_dms_deinit(struct icnss_priv *priv)
{
}

#else
int wlfw_ind_register_send_sync_msg(struct icnss_priv *priv);
int icnss_connect_to_fw_server(struct icnss_priv *priv, void *data);
int wlfw_msa_mem_info_send_sync_msg(struct icnss_priv *priv);
int wlfw_msa_ready_send_sync_msg(struct icnss_priv *priv);
int wlfw_cap_send_sync_msg(struct icnss_priv *priv);
int icnss_qmi_pin_connect_result_ind(struct icnss_priv *priv,
					void *msg, unsigned int msg_len);
int wlfw_dynamic_feature_mask_send_sync_msg(struct icnss_priv *priv,
					   uint64_t dynamic_feature_mask);
int icnss_clear_server(struct icnss_priv *priv);
int wlfw_rejuvenate_ack_send_sync_msg(struct icnss_priv *priv);
void icnss_ignore_fw_timeout(bool ignore);
int wlfw_send_modem_shutdown_msg(struct icnss_priv *priv);
int wlfw_ini_send_sync_msg(struct icnss_priv *priv, uint8_t fw_log_mode);
int wlfw_athdiag_read_send_sync_msg(struct icnss_priv *priv,
					   uint32_t offset, uint32_t mem_type,
					   uint32_t data_len, uint8_t *data);
int wlfw_athdiag_write_send_sync_msg(struct icnss_priv *priv,
					    uint32_t offset, uint32_t mem_type,
					    uint32_t data_len, uint8_t *data);
int icnss_send_wlan_enable_to_fw(struct icnss_priv *priv,
		struct icnss_wlan_enable_cfg *config,
		enum icnss_driver_mode mode,
		const char *host_version);
int icnss_send_wlan_disable_to_fw(struct icnss_priv *priv);
int icnss_register_fw_service(struct icnss_priv *priv);
void icnss_unregister_fw_service(struct icnss_priv *priv);
int icnss_send_vbatt_update(struct icnss_priv *priv, uint64_t voltage_uv);
int wlfw_host_cap_send_sync(struct icnss_priv *priv);
int wlfw_device_info_send_msg(struct icnss_priv *priv);
int wlfw_wlan_mode_send_sync_msg(struct icnss_priv *priv,
				 enum wlfw_driver_mode_enum_v01 mode);
int icnss_wlfw_bdf_dnld_send_sync(struct icnss_priv *priv, u32 bdf_type);
int icnss_wlfw_qdss_dnld_send_sync(struct icnss_priv *priv);
int icnss_wlfw_qdss_data_send_sync(struct icnss_priv *priv, char *file_name,
				   u32 total_size);
int wlfw_qdss_trace_start(struct icnss_priv *priv);
int wlfw_qdss_trace_stop(struct icnss_priv *priv, unsigned long long option);
int wlfw_qdss_trace_mem_info_send_sync(struct icnss_priv *priv);
int wlfw_power_save_send_msg(struct icnss_priv *priv,
			     enum wlfw_power_save_mode_v01 mode);
int icnss_wlfw_get_info_send_sync(struct icnss_priv *priv, int type,
				  void *cmd, int cmd_len);
int wlfw_send_soc_wake_msg(struct icnss_priv *priv,
			   enum wlfw_soc_wake_enum_v01 type);
int icnss_wlfw_m3_dump_upload_done_send_sync(struct icnss_priv *priv,
					     u32 pdev_id, int status);
int icnss_qmi_get_dms_mac(struct icnss_priv *priv);
int icnss_wlfw_wlan_mac_req_send_sync(struct icnss_priv *priv,
				      u8 *mac, u32 mac_len);
int icnss_dms_init(struct icnss_priv *priv);
void icnss_dms_deinit(struct icnss_priv *priv);
#endif

#endif /* __ICNSS_QMI_H__*/
