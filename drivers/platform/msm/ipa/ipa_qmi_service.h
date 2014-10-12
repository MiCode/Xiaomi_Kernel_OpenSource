/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

/**
 * name of the DL wwan default routing tables for v4 and v6
 */
#define IPA_A7_QMAP_HDR_NAME "ipa_qmap_hdr"
#define IPA_DFLT_WAN_RT_TBL_NAME "ipa_dflt_wan_rt"
#define MAX_NUM_Q6_RULE 20
#define DEV_NAME "ipa-wan"
#define SUBSYS_MODEM "modem"

#define IPAWANDBG(fmt, args...) \
	pr_debug(DEV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define IPAWANERR(fmt, args...) \
	pr_err(DEV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

struct rmnet_mux_val {
	uint32_t  mux_id;
	int8_t    vchannel_name[IFNAMSIZ];
	bool mux_channel_set;
	bool ul_flt_reg;
	bool mux_hdr_set;
	uint32_t  hdr_hdl;
};

int ipa_qmi_service_init(bool load_uc, uint32_t wan_platform_type);
void ipa_qmi_service_exit(void);

/* sending filter-install-request to modem*/
int qmi_filter_request_send(struct ipa_install_fltr_rule_req_msg_v01 *req);

/* sending filter-installed-notify-request to modem*/
int qmi_filter_notify_send(struct ipa_fltr_installed_notif_req_msg_v01 *req);

int copy_ul_filter_rule_to_ipa(struct ipa_install_fltr_rule_req_msg_v01
		*rule_req, uint32_t *rule_hdl);

int wwan_update_mux_channel_prop(void);

int wan_ioctl_init(void);

void wan_ioctl_stop_qmi_messages(void);

void wan_ioctl_enable_qmi_messages(void);

void wan_ioctl_deinit(void);

extern struct elem_info ipa_init_modem_driver_req_msg_data_v01_ei[];
extern struct elem_info ipa_init_modem_driver_resp_msg_data_v01_ei[];
extern struct elem_info ipa_indication_reg_req_msg_data_v01_ei[];
extern struct elem_info ipa_indication_reg_resp_msg_data_v01_ei[];
extern struct elem_info ipa_master_driver_init_complt_ind_msg_data_v01_ei[];
extern struct elem_info ipa_install_fltr_rule_req_msg_data_v01_ei[];
extern struct elem_info ipa_install_fltr_rule_resp_msg_data_v01_ei[];
extern struct elem_info ipa_fltr_installed_notif_req_msg_data_v01_ei[];
extern struct elem_info ipa_fltr_installed_notif_resp_msg_data_v01_ei[];
extern struct elem_info ipa_config_req_msg_data_v01_ei[];
extern struct elem_info ipa_config_resp_msg_data_v01_ei[];

/**
 * struct ipa_rmnet_context - IPA rmnet context
 * @ipa_rmnet_ssr: support modem SSR
 */
struct ipa_rmnet_context {
	bool ipa_rmnet_ssr;
};

extern struct ipa_rmnet_context ipa_rmnet_ctx;
#endif /* IPA_QMI_SERVICE_H
 */
