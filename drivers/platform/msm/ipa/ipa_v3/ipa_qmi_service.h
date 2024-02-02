/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
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

#ifndef IPA_QMI_SERVICE_H
#define IPA_QMI_SERVICE_H

#include <linux/ipa.h>
#include <linux/ipa_qmi_service_v01.h>
#include <uapi/linux/msm_rmnet.h>
#include <linux/soc/qcom/qmi.h>
#include "ipa_i.h"
#include <linux/rmnet_ipa_fd_ioctl.h>

/**
 * name of the DL wwan default routing tables for v4 and v6
 */
#define IPA_A7_QMAP_HDR_NAME "ipa_qmap_hdr"
#define IPA_DFLT_WAN_RT_TBL_NAME "ipa_dflt_wan_rt"
#define MAX_NUM_Q6_RULE 35
#define MAX_NUM_QMI_RULE_CACHE 10
#define MAX_NUM_QMI_MPM_AGGR_CACHE 3
#define DEV_NAME "ipa-wan"
#define SUBSYS_LOCAL_MODEM "modem"
#define SUBSYS_REMOTE_MODEM "esoc0"


#define IPAWANDBG(fmt, args...) \
	do { \
		pr_debug(DEV_NAME " %s:%d " fmt, __func__,\
				__LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				DEV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IPAWANDBG_LOW(fmt, args...) \
	do { \
		pr_debug(DEV_NAME " %s:%d " fmt, __func__,\
				__LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAWANERR(fmt, args...) \
	do { \
		pr_err(DEV_NAME " %s:%d " fmt, __func__,\
				__LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				DEV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAWANERR_RL(fmt, args...) \
	do { \
		pr_err_ratelimited_ipa(DEV_NAME " %s:%d " fmt, __func__,\
				__LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				DEV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAWANINFO(fmt, args...) \
	do { \
		pr_info(DEV_NAME " %s:%d " fmt, __func__,\
				__LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				DEV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)

extern struct ipa3_qmi_context *ipa3_qmi_ctx;

struct ipa_offload_connection_val {
	enum ipa_ip_type_enum_v01 ip_type;
	bool valid;
	uint32_t rule_id;
	uint32_t  rule_hdl;
};

struct ipa3_qmi_context {
	struct ipa_ioc_ext_intf_prop q6_ul_filter_rule[MAX_NUM_Q6_RULE];
	u32 q6_ul_filter_rule_hdl[MAX_NUM_Q6_RULE];
	int num_ipa_install_fltr_rule_req_msg;
	struct ipa_install_fltr_rule_req_msg_v01
		ipa_install_fltr_rule_req_msg_cache[MAX_NUM_QMI_RULE_CACHE];
	int num_ipa_install_fltr_rule_req_ex_msg;
	struct ipa_install_fltr_rule_req_ex_msg_v01
		ipa_install_fltr_rule_req_ex_msg_cache[MAX_NUM_QMI_RULE_CACHE];
	int num_ipa_fltr_installed_notif_req_msg;
	struct ipa_fltr_installed_notif_req_msg_v01
		ipa_fltr_installed_notif_req_msg_cache[MAX_NUM_QMI_RULE_CACHE];
	int num_ipa_configure_ul_firewall_rules_req_msg;
	struct ipa_configure_ul_firewall_rules_req_msg_v01
		ipa_configure_ul_firewall_rules_req_msg_cache
			[MAX_NUM_QMI_RULE_CACHE];
	struct ipa_mhi_prime_aggr_info_req_msg_v01
		ipa_mhi_prime_aggr_info_req_msg_cache
			[MAX_NUM_QMI_MPM_AGGR_CACHE];
	bool modem_cfg_emb_pipe_flt;
	struct sockaddr_qrtr client_sq;
	struct sockaddr_qrtr server_sq;
	int num_ipa_offload_connection;
	struct ipa_offload_connection_val
		ipa_offload_cache[QMI_IPA_MAX_FILTERS_V01];
	uint8_t ul_firewall_indices_list_valid;
	uint32_t ul_firewall_indices_list_len;
	uint32_t ul_firewall_indices_list[QMI_IPA_MAX_FILTERS_V01];
};

struct ipa3_rmnet_mux_val {
	uint32_t  mux_id;
	int8_t    vchannel_name[IFNAMSIZ];
	bool mux_channel_set;
	bool ul_flt_reg;
	bool mux_hdr_set;
	uint32_t  hdr_hdl;
};

extern struct qmi_elem_info
	ipa3_init_modem_driver_req_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_init_modem_driver_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_indication_reg_req_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_indication_reg_resp_msg_data_v01_ei[];

extern struct qmi_elem_info
	ipa3_master_driver_init_complt_ind_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_install_fltr_rule_req_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_install_fltr_rule_resp_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_fltr_installed_notif_req_msg_data_v01_ei[];

extern struct qmi_elem_info
	ipa3_fltr_installed_notif_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_enable_force_clear_datapath_req_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_enable_force_clear_datapath_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_disable_force_clear_datapath_req_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_disable_force_clear_datapath_resp_msg_data_v01_ei[];

extern struct qmi_elem_info ipa3_config_req_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_config_resp_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_get_data_stats_req_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_get_data_stats_resp_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_get_apn_data_stats_req_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_get_apn_data_stats_resp_msg_data_v01_ei[];
extern struct qmi_elem_info ipa3_set_data_usage_quota_req_msg_data_v01_ei[];

extern struct qmi_elem_info
	ipa3_set_data_usage_quota_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_data_usage_quota_reached_ind_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_stop_data_usage_quota_req_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_stop_data_usage_quota_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_init_modem_driver_cmplt_req_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_init_modem_driver_cmplt_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_install_fltr_rule_req_ex_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_install_fltr_rule_resp_ex_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_ul_firewall_rule_type_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_ul_firewall_config_result_type_data_v01_ei[];
extern struct
	qmi_elem_info ipa3_per_client_stats_info_type_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_enable_per_client_stats_req_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_enable_per_client_stats_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_get_stats_per_client_req_msg_data_v01_ei[];

extern struct qmi_elem_info
	ipa3_get_stats_per_client_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_configure_ul_firewall_rules_req_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_configure_ul_firewall_rules_resp_msg_data_v01_ei[];
extern struct qmi_elem_info
	ipa3_configure_ul_firewall_rules_ind_msg_data_v01_ei[];

extern struct qmi_elem_info ipa_mhi_ready_indication_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_mem_addr_info_type_v01_ei[];
extern struct qmi_elem_info ipa_mhi_tr_info_type_v01_ei[];
extern struct qmi_elem_info ipa_mhi_er_info_type_v01_ei[];
extern struct qmi_elem_info ipa_mhi_alloc_channel_req_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_ch_alloc_resp_type_v01_ei[];
extern struct qmi_elem_info ipa_mhi_alloc_channel_resp_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_clk_vote_req_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_clk_vote_resp_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_cleanup_req_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_cleanup_resp_msg_v01_ei[];

extern struct qmi_elem_info ipa_endp_desc_indication_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_prime_aggr_info_req_msg_v01_ei[];
extern struct qmi_elem_info ipa_mhi_prime_aggr_info_resp_msg_v01_ei[];
extern struct qmi_elem_info ipa_add_offload_connection_req_msg_v01_ei[];
extern struct qmi_elem_info ipa_add_offload_connection_resp_msg_v01_ei[];
extern struct qmi_elem_info ipa_remove_offload_connection_req_msg_v01_ei[];
extern struct qmi_elem_info ipa_remove_offload_connection_resp_msg_v01_ei[];
extern struct qmi_elem_info ipa_bw_change_ind_msg_v01_ei[];

/**
 * struct ipa3_rmnet_context - IPA rmnet context
 * @ipa_rmnet_ssr: support modem SSR
 * @polling_interval: Requested interval for polling tethered statistics
 * @metered_mux_id: The mux ID on which quota has been set
 */
struct ipa3_rmnet_context {
	bool ipa_rmnet_ssr;
	u64 polling_interval;
	u32 metered_mux_id;
};

extern struct ipa3_rmnet_context ipa3_rmnet_ctx;

#ifdef CONFIG_RMNET_IPA3

int ipa3_qmi_service_init(uint32_t wan_platform_type);

void ipa3_qmi_service_exit(void);

/* sending filter-install-request to modem*/
int ipa3_qmi_filter_request_send(
	struct ipa_install_fltr_rule_req_msg_v01 *req);

int ipa3_qmi_filter_request_ex_send(
	struct ipa_install_fltr_rule_req_ex_msg_v01 *req);

int ipa3_qmi_add_offload_request_send(
	struct ipa_add_offload_connection_req_msg_v01 *req);

int ipa3_qmi_rmv_offload_request_send(
	struct ipa_remove_offload_connection_req_msg_v01 *req);

int ipa3_qmi_ul_filter_request_send(
	struct ipa_configure_ul_firewall_rules_req_msg_v01 *req);

/* sending filter-installed-notify-request to modem*/
int ipa3_qmi_filter_notify_send(struct ipa_fltr_installed_notif_req_msg_v01
		*req);

/* voting for bus BW to ipa_rm*/
int ipa3_vote_for_bus_bw(uint32_t *bw_mbps);

int ipa3_qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req);

int ipa3_qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req);

int ipa3_copy_ul_filter_rule_to_ipa(struct ipa_install_fltr_rule_req_msg_v01
	*rule_req);

int ipa3_wwan_update_mux_channel_prop(void);

int ipa3_wan_ioctl_init(void);

void ipa3_wan_ioctl_stop_qmi_messages(void);

void ipa3_wan_ioctl_enable_qmi_messages(void);

void ipa3_wan_ioctl_deinit(void);

void ipa3_qmi_stop_workqueues(void);

int rmnet_ipa3_poll_tethering_stats(struct wan_ioctl_poll_tethering_stats
		*data);

int rmnet_ipa3_set_data_quota(struct wan_ioctl_set_data_quota *data);

void ipa3_broadcast_quota_reach_ind(uint32_t mux_id,
	enum ipa_upstream_type upstream_type);

int rmnet_ipa3_set_tether_client_pipe(struct wan_ioctl_set_tether_client_pipe
	*data);

int rmnet_ipa3_query_tethering_stats(struct wan_ioctl_query_tether_stats *data,
	bool reset);

int rmnet_ipa3_query_tethering_stats_all(
	struct wan_ioctl_query_tether_stats_all *data);

int rmnet_ipa3_reset_tethering_stats(struct wan_ioctl_reset_tether_stats *data);
int rmnet_ipa3_set_lan_client_info(struct wan_ioctl_lan_client_info *data);

int rmnet_ipa3_clear_lan_client_info(struct wan_ioctl_lan_client_info *data);

int rmnet_ipa3_send_lan_client_msg(struct wan_ioctl_send_lan_client_msg *data);

int rmnet_ipa3_enable_per_client_stats(bool *data);

int rmnet_ipa3_query_per_client_stats(
	struct wan_ioctl_query_per_client_stats *data);

int rmnet_ipa3_query_per_client_stats_v2(
	struct wan_ioctl_query_per_client_stats *data);

int ipa3_qmi_get_data_stats(struct ipa_get_data_stats_req_msg_v01 *req,
	struct ipa_get_data_stats_resp_msg_v01 *resp);

int ipa3_qmi_get_network_stats(struct ipa_get_apn_data_stats_req_msg_v01 *req,
	struct ipa_get_apn_data_stats_resp_msg_v01 *resp);

int ipa3_qmi_set_data_quota(struct ipa_set_data_usage_quota_req_msg_v01 *req);

int ipa3_qmi_set_aggr_info(
	enum ipa_aggr_enum_type_v01 aggr_enum_type);

int ipa3_qmi_req_ind(void);

int ipa3_qmi_stop_data_qouta(void);

void ipa3_q6_handshake_complete(bool ssr_bootup);

int ipa3_wwan_set_modem_perf_profile(int throughput);

int ipa3_wwan_set_modem_state(struct wan_ioctl_notify_wan_state *state);
int ipa3_qmi_enable_per_client_stats(
	struct ipa_enable_per_client_stats_req_msg_v01 *req,
	struct ipa_enable_per_client_stats_resp_msg_v01 *resp);

int ipa3_qmi_get_per_client_packet_stats(
	struct ipa_get_stats_per_client_req_msg_v01 *req,
	struct ipa_get_stats_per_client_resp_msg_v01 *resp);

int ipa3_qmi_send_mhi_ready_indication(
	struct ipa_mhi_ready_indication_msg_v01 *req);

int ipa3_qmi_send_endp_desc_indication(
	struct ipa_endp_desc_indication_msg_v01 *req);

int ipa3_qmi_send_mhi_cleanup_request(struct ipa_mhi_cleanup_req_msg_v01 *req);

void ipa3_qmi_init(void);

void ipa3_qmi_cleanup(void);

#else /* CONFIG_RMNET_IPA3 */

static inline int ipa3_qmi_service_init(uint32_t wan_platform_type)
{
	return -EPERM;
}

static inline void ipa3_qmi_service_exit(void) { }

/* sending filter-install-request to modem*/
static inline int ipa3_qmi_filter_request_send(
	struct ipa_install_fltr_rule_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_add_offload_request_send(
	struct ipa_add_offload_connection_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_rmv_offload_request_send(
	struct ipa_remove_offload_connection_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_ul_filter_request_send(
	struct ipa_configure_ul_firewall_rules_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_filter_request_ex_send(
	struct ipa_install_fltr_rule_req_ex_msg_v01 *req)
{
	return -EPERM;
}

/* sending filter-installed-notify-request to modem*/
static inline int ipa3_qmi_filter_notify_send(
	struct ipa_fltr_installed_notif_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_copy_ul_filter_rule_to_ipa(
	struct ipa_install_fltr_rule_req_msg_v01 *rule_req)
{
	return -EPERM;
}

static inline int ipa3_wwan_update_mux_channel_prop(void)
{
	return -EPERM;
}

static inline int ipa3_wan_ioctl_init(void)
{
	return -EPERM;
}

static inline void ipa3_wan_ioctl_stop_qmi_messages(void) { }

static inline void ipa3_wan_ioctl_enable_qmi_messages(void) { }

static inline void ipa3_wan_ioctl_deinit(void) { }

static inline void ipa3_qmi_stop_workqueues(void) { }

static inline int ipa3_vote_for_bus_bw(uint32_t *bw_mbps)
{
	return -EPERM;
}

static inline int rmnet_ipa3_poll_tethering_stats(
	struct wan_ioctl_poll_tethering_stats *data)
{
	return -EPERM;
}

static inline int rmnet_ipa3_set_data_quota(
	struct wan_ioctl_set_data_quota *data)
{
	return -EPERM;
}

static inline void ipa3_broadcast_quota_reach_ind(uint32_t mux_id,
	enum ipa_upstream_type upstream_type) { }

static inline int ipa3_qmi_get_data_stats(
	struct ipa_get_data_stats_req_msg_v01 *req,
	struct ipa_get_data_stats_resp_msg_v01 *resp)
{
	return -EPERM;
}

static inline int ipa3_qmi_get_network_stats(
	struct ipa_get_apn_data_stats_req_msg_v01 *req,
	struct ipa_get_apn_data_stats_resp_msg_v01 *resp)
{
	return -EPERM;
}

static inline int ipa3_qmi_set_data_quota(
	struct ipa_set_data_usage_quota_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_stop_data_qouta(void)
{
	return -EPERM;
}

static inline void ipa3_q6_handshake_complete(bool ssr_bootup) { }

static inline int ipa3_qmi_send_mhi_ready_indication(
	struct ipa_mhi_ready_indication_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_send_endp_desc_indication(
	struct ipa_endp_desc_indication_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_qmi_send_mhi_cleanup_request(
	struct ipa_mhi_cleanup_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa3_wwan_set_modem_perf_profile(
	int throughput)
{
	return -EPERM;
}
static inline int ipa3_qmi_enable_per_client_stats(
	struct ipa_enable_per_client_stats_req_msg_v01 *req,
	struct ipa_enable_per_client_stats_resp_msg_v01 *resp)
{
	return -EPERM;
}

static inline int ipa3_qmi_get_per_client_packet_stats(
	struct ipa_get_stats_per_client_req_msg_v01 *req,
	struct ipa_get_stats_per_client_resp_msg_v01 *resp)
{
	return -EPERM;
}

static inline int ipa3_qmi_set_aggr_info(
	enum ipa_aggr_enum_type_v01 aggr_enum_type)
{
	return -EPERM;
}

static inline void ipa3_qmi_init(void)
{

}

static inline void ipa3_qmi_cleanup(void)
{

}

#endif /* CONFIG_RMNET_IPA3 */

#endif /* IPA_QMI_SERVICE_H */
