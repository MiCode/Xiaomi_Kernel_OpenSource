/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2021, The Linux Foundation. All rights reserved. */

#ifndef _CNSS_QMI_H
#define _CNSS_QMI_H

#include "wlan_firmware_service_v01.h"

struct cnss_plat_data;

struct cnss_qmi_event_server_arrive_data {
	unsigned int node;
	unsigned int port;
};

struct cnss_mem_seg {
	u64 addr;
	u32 size;
};

struct cnss_qmi_event_fw_mem_file_save_data {
	u32 total_size;
	u32 mem_seg_len;
	enum wlfw_mem_type_enum_v01 mem_type;
	struct cnss_mem_seg mem_seg[QMI_WLFW_MAX_NUM_MEM_SEG_V01];
	char file_name[QMI_WLFW_MAX_STR_LEN_V01 + 1];
};

#ifdef CONFIG_CNSS2_QMI
#include "coexistence_service_v01.h"
#include "ip_multimedia_subsystem_private_service_v01.h"
#include "device_management_service_v01.h"

int cnss_qmi_init(struct cnss_plat_data *plat_priv);
void cnss_qmi_deinit(struct cnss_plat_data *plat_priv);
unsigned int cnss_get_qmi_timeout(struct cnss_plat_data *plat_priv);
int cnss_wlfw_server_arrive(struct cnss_plat_data *plat_priv, void *data);
int cnss_wlfw_server_exit(struct cnss_plat_data *plat_priv);
int cnss_wlfw_respond_mem_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_tgt_cap_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv,
				 u32 bdf_type);
int cnss_wlfw_m3_dnld_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_wlan_mode_send_sync(struct cnss_plat_data *plat_priv,
				  enum cnss_driver_mode mode);
int cnss_wlfw_wlan_cfg_send_sync(struct cnss_plat_data *plat_priv,
				 struct cnss_wlan_enable_cfg *config,
				 const char *host_version);
int cnss_wlfw_athdiag_read_send_sync(struct cnss_plat_data *plat_priv,
				     u32 offset, u32 mem_type,
				     u32 data_len, u8 *data);
int cnss_wlfw_athdiag_write_send_sync(struct cnss_plat_data *plat_priv,
				      u32 offset, u32 mem_type,
				      u32 data_len, u8 *data);
int cnss_wlfw_ini_send_sync(struct cnss_plat_data *plat_priv,
			    u8 fw_log_mode);
int cnss_wlfw_antenna_switch_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_antenna_grant_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_dynamic_feature_mask_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_get_info_send_sync(struct cnss_plat_data *plat_priv, int type,
				 void *cmd, int cmd_len);
int cnss_process_wfc_call_ind_event(struct cnss_plat_data *plat_priv,
				    void *data);
int cnss_process_twt_cfg_ind_event(struct cnss_plat_data *plat_priv,
				   void *data);
int cnss_register_coex_service(struct cnss_plat_data *plat_priv);
void cnss_unregister_coex_service(struct cnss_plat_data *plat_priv);
int coex_antenna_switch_to_wlan_send_sync_msg(struct cnss_plat_data *plat_priv);
int coex_antenna_switch_to_mdm_send_sync_msg(struct cnss_plat_data *plat_priv);
int cnss_wlfw_qdss_trace_mem_info_send_sync(struct cnss_plat_data *plat_priv);
int cnss_register_ims_service(struct cnss_plat_data *plat_priv);
void cnss_unregister_ims_service(struct cnss_plat_data *plat_priv);
int cnss_wlfw_send_pcie_gen_speed_sync(struct cnss_plat_data *plat_priv);
void cnss_ignore_qmi_failure(bool ignore);
int cnss_qmi_get_dms_mac(struct cnss_plat_data *plat_priv);
int cnss_wlfw_wlan_mac_req_send_sync(struct cnss_plat_data *plat_priv,
				     u8 *mac, u32 mac_len);
int cnss_dms_init(struct cnss_plat_data *plat_priv);
void cnss_dms_deinit(struct cnss_plat_data *plat_priv);
int cnss_wlfw_qdss_dnld_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_qdss_data_send_sync(struct cnss_plat_data *plat_priv, char *file_name,
				  u32 total_size);
int wlfw_qdss_trace_start(struct cnss_plat_data *plat_priv);
int wlfw_qdss_trace_stop(struct cnss_plat_data *plat_priv, unsigned long long option);
int cnss_wlfw_cal_report_req_send_sync(struct cnss_plat_data *plat_priv,
				       u32 cal_file_download_size);
int cnss_wlfw_ini_file_send_sync(struct cnss_plat_data *plat_priv,
				 enum wlfw_ini_file_type_v01 file_type);
int cnss_wlfw_send_host_wfc_call_status(struct cnss_plat_data *plat_priv,
					struct cnss_wfc_cfg cfg);
#else
#define QMI_WLFW_TIMEOUT_MS		10000

static inline int cnss_qmi_init(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline void cnss_qmi_deinit(struct cnss_plat_data *plat_priv)
{
}

static inline
unsigned int cnss_get_qmi_timeout(struct cnss_plat_data *plat_priv)
{
	return QMI_WLFW_TIMEOUT_MS;
}

static inline int cnss_wlfw_server_arrive(struct cnss_plat_data *plat_priv,
					  void *data)
{
	return 0;
}

static inline int cnss_wlfw_server_exit(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int cnss_wlfw_respond_mem_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline int cnss_wlfw_tgt_cap_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv,
					       u32 bdf_type)
{
	return 0;
}

static inline int cnss_wlfw_m3_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int cnss_wlfw_wlan_mode_send_sync(struct cnss_plat_data *plat_priv,
				  enum cnss_driver_mode mode)
{
	return 0;
}

static inline
int cnss_wlfw_wlan_cfg_send_sync(struct cnss_plat_data *plat_priv,
				 struct cnss_wlan_enable_cfg *config,
				 const char *host_version)
{
	return 0;
}

static inline
int cnss_wlfw_athdiag_read_send_sync(struct cnss_plat_data *plat_priv,
				     u32 offset, u32 mem_type,
				     u32 data_len, u8 *data)
{
	return 0;
}

static inline
int cnss_wlfw_athdiag_write_send_sync(struct cnss_plat_data *plat_priv,
				      u32 offset, u32 mem_type,
				      u32 data_len, u8 *data)
{
	return 0;
}

static inline
int cnss_wlfw_ini_send_sync(struct cnss_plat_data *plat_priv,
			    u8 fw_log_mode)
{
	return 0;
}

static inline
int cnss_wlfw_antenna_switch_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int cnss_wlfw_antenna_grant_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int cnss_wlfw_dynamic_feature_mask_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int cnss_wlfw_get_info_send_sync(struct cnss_plat_data *plat_priv, int type,
				 void *cmd, int cmd_len)
{
	return 0;
}

static inline
int cnss_process_wfc_call_ind_event(struct cnss_plat_data *plat_priv,
				    void *data)
{
	return 0;
}

static inline
int cnss_process_twt_cfg_ind_event(struct cnss_plat_data *plat_priv,
				   void *data)
{
	return 0;
}

static inline
int cnss_register_coex_service(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
void cnss_unregister_coex_service(struct cnss_plat_data *plat_priv) {}

static inline
int coex_antenna_switch_to_wlan_send_sync_msg(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int coex_antenna_switch_to_mdm_send_sync_msg(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int cnss_wlfw_qdss_trace_mem_info_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
int cnss_register_ims_service(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline
void cnss_unregister_ims_service(struct cnss_plat_data *plat_priv) {}

static inline
int cnss_wlfw_send_pcie_gen_speed_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}
void cnss_ignore_qmi_failure(bool ignore) {};
static inline int cnss_qmi_get_dms_mac(struct cnss_plat_data *plat_priv)
{
	return 0;
}

int cnss_wlfw_wlan_mac_req_send_sync(struct cnss_plat_data *plat_priv,
				     u8 *mac, u32 mac_len)
{
	return 0;
}

static inline int cnss_dms_init(struct cnss_plat_data *plat_priv)
{
	return 0;
}

int cnss_wlfw_qdss_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	return 0;
}

int cnss_wlfw_qdss_data_send_sync(struct cnss_plat_data *plat_priv, char *file_name,
				  u32 total_size)
{
	return 0;
}

static inline void cnss_dms_deinit(struct cnss_plat_data *plat_priv) {}

int wlfw_qdss_trace_start(struct cnss_plat_data *plat_priv)
{
	return 0;
}

int wlfw_qdss_trace_stop(struct cnss_plat_data *plat_priv, unsigned long long option)
{
	return 0;
}

static inline
int cnss_wlfw_cal_report_req_send_sync(struct cnss_plat_data *plat_priv,
				       u32 cal_file_download_size)
{
	return 0;
}

int cnss_wlfw_ini_file_send_sync(struct cnss_plat_data *plat_priv,
				 enum wlfw_ini_file_type_v01 file_type)
{
	return 0;
}

int cnss_wlfw_send_host_wfc_call_status(struct cnss_plat_data *plat_priv,
					struct cnss_wfc_cfg cfg)
{
	return 0;
}
#endif /* CONFIG_CNSS2_QMI */

#ifdef CONFIG_CNSS2_DEBUG
static inline u32 cnss_get_host_build_type(void)
{
	return QMI_HOST_BUILD_TYPE_PRIMARY_V01;
}
#else
static inline u32 cnss_get_host_build_type(void)
{
	return QMI_HOST_BUILD_TYPE_SECONDARY_V01;
}
#endif

#endif /* _CNSS_QMI_H */
