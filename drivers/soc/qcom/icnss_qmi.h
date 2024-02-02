/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ICNSS_QMI_H__
#define __ICNSS_QMI_H__

#ifndef CONFIG_ICNSS_QMI

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
#endif

#endif /* __ICNSS_QMI_H__*/
