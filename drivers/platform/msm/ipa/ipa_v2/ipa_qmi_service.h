/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/msm_qmi_interface.h>
#include "ipa_i.h"
#include <linux/rmnet_ipa_fd_ioctl.h>

/**
 * name of the DL wwan default routing tables for v4 and v6
 */
#define IPA_A7_QMAP_HDR_NAME "ipa_qmap_hdr"
#define IPA_DFLT_WAN_RT_TBL_NAME "ipa_dflt_wan_rt"
#define MAX_NUM_Q6_RULE 35
#define MAX_NUM_QMI_RULE_CACHE 10
#define DEV_NAME "ipa-wan"
#define SUBSYS_MODEM "modem"

#define IPAWANDBG(fmt, args...) \
	do { \
		pr_debug(DEV_NAME " %s:%d " fmt, __func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			DEV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAWANDBG_LOW(fmt, args...) \
	do { \
		pr_debug(DEV_NAME " %s:%d " fmt, __func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAWANERR(fmt, args...) \
	do { \
		pr_err(DEV_NAME " %s:%d " fmt, __func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			DEV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAWANINFO(fmt, args...) \
	do { \
		pr_info(DEV_NAME " %s:%d " fmt, __func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			DEV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			DEV_NAME " %s:%d " fmt, ## args); \
	} while (0)


extern struct ipa_qmi_context *ipa_qmi_ctx;
extern struct mutex ipa_qmi_lock;

struct ipa_qmi_context {
struct ipa_ioc_ext_intf_prop q6_ul_filter_rule[MAX_NUM_Q6_RULE];
u32 q6_ul_filter_rule_hdl[MAX_NUM_Q6_RULE];
int num_ipa_install_fltr_rule_req_msg;
struct ipa_install_fltr_rule_req_msg_v01
		ipa_install_fltr_rule_req_msg_cache[MAX_NUM_QMI_RULE_CACHE];
int num_ipa_fltr_installed_notif_req_msg;
struct ipa_fltr_installed_notif_req_msg_v01
		ipa_fltr_installed_notif_req_msg_cache[MAX_NUM_QMI_RULE_CACHE];
bool modem_cfg_emb_pipe_flt;
};

struct rmnet_mux_val {
	uint32_t  mux_id;
	int8_t    vchannel_name[IFNAMSIZ];
	bool mux_channel_set;
	bool ul_flt_reg;
	bool mux_hdr_set;
	uint32_t  hdr_hdl;
};

extern struct elem_info ipa_init_modem_driver_req_msg_data_v01_ei[];
extern struct elem_info ipa_init_modem_driver_resp_msg_data_v01_ei[];
extern struct elem_info ipa_indication_reg_req_msg_data_v01_ei[];
extern struct elem_info ipa_indication_reg_resp_msg_data_v01_ei[];
extern struct elem_info ipa_master_driver_init_complt_ind_msg_data_v01_ei[];
extern struct elem_info ipa_install_fltr_rule_req_msg_data_v01_ei[];
extern struct elem_info ipa_install_fltr_rule_resp_msg_data_v01_ei[];
extern struct elem_info ipa_fltr_installed_notif_req_msg_data_v01_ei[];
extern struct elem_info ipa_fltr_installed_notif_resp_msg_data_v01_ei[];
extern struct elem_info ipa_enable_force_clear_datapath_req_msg_data_v01_ei[];
extern struct elem_info ipa_enable_force_clear_datapath_resp_msg_data_v01_ei[];
extern struct elem_info ipa_disable_force_clear_datapath_req_msg_data_v01_ei[];
extern struct elem_info ipa_disable_force_clear_datapath_resp_msg_data_v01_ei[];
extern struct elem_info ipa_config_req_msg_data_v01_ei[];
extern struct elem_info ipa_config_resp_msg_data_v01_ei[];
extern struct elem_info ipa_get_data_stats_req_msg_data_v01_ei[];
extern struct elem_info ipa_get_data_stats_resp_msg_data_v01_ei[];
extern struct elem_info ipa_get_apn_data_stats_req_msg_data_v01_ei[];
extern struct elem_info ipa_get_apn_data_stats_resp_msg_data_v01_ei[];
extern struct elem_info ipa_set_data_usage_quota_req_msg_data_v01_ei[];
extern struct elem_info ipa_set_data_usage_quota_resp_msg_data_v01_ei[];
extern struct elem_info ipa_data_usage_quota_reached_ind_msg_data_v01_ei[];
extern struct elem_info ipa_stop_data_usage_quota_req_msg_data_v01_ei[];
extern struct elem_info ipa_stop_data_usage_quota_resp_msg_data_v01_ei[];

/**
 * struct ipa_rmnet_context - IPA rmnet context
 * @ipa_rmnet_ssr: support modem SSR
 * @polling_interval: Requested interval for polling tethered statistics
 * @metered_mux_id: The mux ID on which quota has been set
 */
struct ipa_rmnet_context {
	bool ipa_rmnet_ssr;
	u64 polling_interval;
	u32 metered_mux_id;
};

extern struct ipa_rmnet_context ipa_rmnet_ctx;

#ifdef CONFIG_RMNET_IPA

int ipa_qmi_service_init(uint32_t wan_platform_type);

void ipa_qmi_service_exit(void);

/* sending filter-install-request to modem*/
int qmi_filter_request_send(struct ipa_install_fltr_rule_req_msg_v01 *req);

/* sending filter-installed-notify-request to modem*/
int qmi_filter_notify_send(struct ipa_fltr_installed_notif_req_msg_v01 *req);

/* voting for bus BW to ipa_rm*/
int vote_for_bus_bw(uint32_t *bw_mbps);

int qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req);

int qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req);

int copy_ul_filter_rule_to_ipa(struct ipa_install_fltr_rule_req_msg_v01
	*rule_req, uint32_t *rule_hdl);

int wwan_update_mux_channel_prop(void);

int wan_ioctl_init(void);

void wan_ioctl_stop_qmi_messages(void);

void wan_ioctl_enable_qmi_messages(void);

void wan_ioctl_deinit(void);

void ipa_qmi_stop_workqueues(void);

int rmnet_ipa_poll_tethering_stats(struct wan_ioctl_poll_tethering_stats *data);

int rmnet_ipa_set_data_quota(struct wan_ioctl_set_data_quota *data);

void ipa_broadcast_quota_reach_ind(uint32_t mux_id,
	enum ipa_upstream_type upstream_type);

int rmnet_ipa_set_tether_client_pipe(struct wan_ioctl_set_tether_client_pipe
	*data);

int rmnet_ipa_query_tethering_stats(struct wan_ioctl_query_tether_stats *data,
	bool reset);

int rmnet_ipa_reset_tethering_stats(struct wan_ioctl_reset_tether_stats *data);

int ipa_qmi_get_data_stats(struct ipa_get_data_stats_req_msg_v01 *req,
	struct ipa_get_data_stats_resp_msg_v01 *resp);

int ipa_qmi_get_network_stats(struct ipa_get_apn_data_stats_req_msg_v01 *req,
	struct ipa_get_apn_data_stats_resp_msg_v01 *resp);

int ipa_qmi_set_data_quota(struct ipa_set_data_usage_quota_req_msg_v01 *req);

int ipa_qmi_stop_data_qouta(void);

void ipa_q6_handshake_complete(bool ssr_bootup);

void ipa_qmi_init(void);

void ipa_qmi_cleanup(void);

#else /* CONFIG_RMNET_IPA */

static inline int ipa_qmi_service_init(uint32_t wan_platform_type)
{
	return -EPERM;
}

static inline void ipa_qmi_service_exit(void) { }

/* sending filter-install-request to modem*/
static inline int qmi_filter_request_send(
	struct ipa_install_fltr_rule_req_msg_v01 *req)
{
	return -EPERM;
}

/* sending filter-installed-notify-request to modem*/
static inline int qmi_filter_notify_send(
	struct ipa_fltr_installed_notif_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int copy_ul_filter_rule_to_ipa(
	struct ipa_install_fltr_rule_req_msg_v01 *rule_req, uint32_t *rule_hdl)
{
	return -EPERM;
}

static inline int wwan_update_mux_channel_prop(void)
{
	return -EPERM;
}

static inline int wan_ioctl_init(void)
{
	return -EPERM;
}

static inline void wan_ioctl_stop_qmi_messages(void) { }

static inline void wan_ioctl_enable_qmi_messages(void) { }

static inline void wan_ioctl_deinit(void) { }

static inline void ipa_qmi_stop_workqueues(void) { }

static inline int vote_for_bus_bw(uint32_t *bw_mbps)
{
	return -EPERM;
}

static inline int rmnet_ipa_poll_tethering_stats(
	struct wan_ioctl_poll_tethering_stats *data)
{
	return -EPERM;
}

static inline int rmnet_ipa_set_data_quota(
	struct wan_ioctl_set_data_quota *data)
{
	return -EPERM;
}

static inline void ipa_broadcast_quota_reach_ind
(
	uint32_t mux_id,
	enum ipa_upstream_type upstream_type)
{
}

static int rmnet_ipa_reset_tethering_stats
(
	struct wan_ioctl_reset_tether_stats *data
)
{
	return -EPERM;

}

static inline int ipa_qmi_get_data_stats(
	struct ipa_get_data_stats_req_msg_v01 *req,
	struct ipa_get_data_stats_resp_msg_v01 *resp)
{
	return -EPERM;
}

static inline int ipa_qmi_get_network_stats(
	struct ipa_get_apn_data_stats_req_msg_v01 *req,
	struct ipa_get_apn_data_stats_resp_msg_v01 *resp)
{
	return -EPERM;
}

static inline int ipa_qmi_set_data_quota(
	struct ipa_set_data_usage_quota_req_msg_v01 *req)
{
	return -EPERM;
}

static inline int ipa_qmi_stop_data_qouta(void)
{
	return -EPERM;
}

static inline void ipa_q6_handshake_complete(bool ssr_bootup) { }

static inline void ipa_qmi_init(void)
{
}

static inline void ipa_qmi_cleanup(void)
{
}

#endif /* CONFIG_RMNET_IPA */

#endif /* IPA_QMI_SERVICE_H */
