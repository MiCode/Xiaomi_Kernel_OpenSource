/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved. */

#ifndef _CNSS_QMI_H
#define _CNSS_QMI_H

#include <linux/soc/qcom/qmi.h>

struct cnss_plat_data;

#ifdef CONFIG_CNSS2_QMI
#include "wlan_firmware_service_v01.h"
#include "coexistence_service_v01.h"

struct cnss_qmi_event_server_arrive_data {
	unsigned int node;
	unsigned int port;
};

int cnss_qmi_init(struct cnss_plat_data *plat_priv);
void cnss_qmi_deinit(struct cnss_plat_data *plat_priv);
unsigned int cnss_get_qmi_timeout(struct cnss_plat_data *plat_priv);
int cnss_wlfw_server_arrive(struct cnss_plat_data *plat_priv, void *data);
int cnss_wlfw_server_exit(struct cnss_plat_data *plat_priv);
int cnss_wlfw_respond_mem_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_tgt_cap_send_sync(struct cnss_plat_data *plat_priv);
int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv);
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
int cnss_register_coex_service(struct cnss_plat_data *plat_priv);
void cnss_unregister_coex_service(struct cnss_plat_data *plat_priv);
int coex_antenna_switch_to_wlan_send_sync_msg(struct cnss_plat_data *plat_priv);
int coex_antenna_switch_to_mdm_send_sync_msg(struct cnss_plat_data *plat_priv);

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

static inline int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv)
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
#endif /* CONFIG_CNSS2_QMI */

#endif /* _CNSS_QMI_H */
