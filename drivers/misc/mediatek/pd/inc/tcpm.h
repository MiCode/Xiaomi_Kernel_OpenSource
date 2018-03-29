/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TCPM_H_
#define TCPM_H_

#include <linux/kernel.h>
#include <linux/notifier.h>

struct tcpc_device;

/*
 * Type-C Port Notify Chain
 */

enum typec_attach_type {
	TYPEC_UNATTACHED = 0,
	TYPEC_ATTACHED_SNK,
	TYPEC_ATTACHED_SRC,
	TYPEC_ATTACHED_AUDIO,
	TYPEC_ATTACHED_DEBUG,
};

enum pd_connect_result {
	PD_CONNECT_NONE = 0,
	PD_CONNECT_TYPEC_ONLY,
	PD_CONNECT_TYPEC_ONLY_SNK_DFT,
	PD_CONNECT_TYPEC_ONLY_SNK,
	PD_CONNECT_TYPEC_ONLY_SRC,
	PD_CONNECT_PE_READY,
	PD_CONNECT_PE_READY_SNK,
	PD_CONNECT_PE_READY_SRC,
};

/* Power role */
#define PD_ROLE_SINK   0
#define PD_ROLE_SOURCE 1

/* Data role */
#define PD_ROLE_UFP    0
#define PD_ROLE_DFP    1

/* Vconn role */
#define PD_ROLE_VCONN_OFF 0
#define PD_ROLE_VCONN_ON  1

enum {
	TCP_NOTIFY_SOURCE_VCONN,
	TCP_NOTIFY_SOURCE_VBUS,
	TCP_NOTIFY_SINK_VBUS,
	TCP_NOTIFY_PR_SWAP,
	TCP_NOTIFY_DR_SWAP,
	TCP_NOTIFY_VCONN_SWAP,
	TCP_NOTIFY_AMA_DP_STATE,
	TCP_NOTIFY_AMA_DP_ATTENTION,
	TCP_NOTIFY_AMA_DP_HPD_STATE,

	TCP_NOTIFY_TYPEC_STATE,
	TCP_NOTIFY_PD_STATE,
#ifdef CONFIG_RT7207_ADAPTER
	TCP_NOTIFY_RT7207_VDM,
#endif /* CONFIG_RT7207_ADAPTER */
};

struct tcp_ny_pd_state {
	uint8_t connected;
};

struct tcp_ny_swap_state {
	uint8_t new_role;
};

struct tcp_ny_enable_state {
	bool en;
};

struct tcp_ny_typec_state {
	uint8_t rp_level;
	uint8_t polarity;
	uint8_t old_state;
	uint8_t new_state;
};

struct tcp_ny_vbus_state {
	int mv;
	int ma;
};

enum {
	SW_USB = 0,
	SW_DFP_D,
	SW_UFP_D,
};

struct tcp_ny_ama_dp_state {
	uint8_t sel_config;
	uint8_t signal;
	uint8_t pin_assignment;
	uint8_t polarity;
	uint8_t active;
};

enum {
	TCP_DP_UFP_U_MASK = 0x7C,
	TCP_DP_UFP_U_POWER_LOW = 1 << 2,
	TCP_DP_UFP_U_ENABLED = 1 << 3,
	TCP_DP_UFP_U_MF_PREFER = 1 << 4,
	TCP_DP_UFP_U_USB_CONFIG = 1 << 5,
	TCP_DP_UFP_U_EXIT_MODE = 1 << 6,
};

struct tcp_ny_ama_dp_attention {
	uint8_t state;
};

struct tcp_ny_ama_dp_hpd_state {
	bool irq : 1;
	bool state : 1;
};

struct tcp_notify {
	union {
		struct tcp_ny_enable_state en_state;
		struct tcp_ny_vbus_state vbus_state;
		struct tcp_ny_typec_state typec_state;
		struct tcp_ny_swap_state swap_state;
		struct tcp_ny_pd_state pd_state;
		struct tcp_ny_ama_dp_state ama_dp_state;
		struct tcp_ny_ama_dp_attention ama_dp_attention;
		struct tcp_ny_ama_dp_hpd_state ama_dp_hpd_state;
	};
#ifdef CONFIG_RT7207_ADAPTER
	bool rt7207_vdm_success;
	uint32_t payload[7];
#endif /* CONFIG_RT7207_ADAPTER */
};

extern struct tcpc_device
		*tcpc_dev_get_by_name(const char *name);

extern int register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				     struct notifier_block *nb);
extern int unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				       struct notifier_block *nb);

/*
 * Type-C Port Control I/F
 */

enum tcpm_error_list {
	TCPM_SUCCESS = 0,
	TCPM_ERROR_UNKNOWN = -1,
	TCPM_ERROR_PUT_EVENT = -2,
};

#define TCPM_PDO_MAX_SIZE	7

struct tcpm_power_cap {
	uint8_t cnt;
	uint32_t pdos[TCPM_PDO_MAX_SIZE];
};

/* Request TCPM to send PD Request */

extern int tcpm_power_role_swap(struct tcpc_device *tcpc_dev);
extern int tcpm_data_role_swap(struct tcpc_device *tcpc_dev);
extern int tcpm_vconn_swap(struct tcpc_device *tcpc_dev);
extern int tcpm_goto_min(struct tcpc_device *tcpc_dev);
extern int tcpm_soft_reset(struct tcpc_device *tcpc_dev);
extern int tcpm_hard_reset(struct tcpc_device *tcpc_dev);
extern int tcpm_get_source_cap(
	struct tcpc_device *tcpc_dev, struct tcpm_power_cap *cap);
extern int tcpm_get_sink_cap(
	struct tcpc_device *tcpc_dev, struct tcpm_power_cap *cap);
extern int tcpm_request(
	struct tcpc_device *tcpc_dev, int mv, int ma);
extern int tcpm_error_recovery(struct tcpc_device *tcpc_dev);

/* Request TCPM to send VDM */

extern int tcpm_discover_cable(
	struct tcpc_device *tcpc_dev, uint32_t *vdos);

extern int tcpm_vdm_request_id(
	struct tcpc_device *tcpc_dev, uint8_t *cnt, uint8_t *payload);

#ifdef CONFIG_RT7207_ADAPTER
extern int tcpm_vdm_request_rt7207(
	struct tcpc_device *tcpc_dev, uint16_t vdm_hdr, uint32_t data);
extern int tcpm_set_direct_charge_en(struct tcpc_device *tcpc_dev, bool en);
#endif /* CONFIG_RT7207_ADAPTER */

/* Request TCPM to send PD-DP Request */

#ifdef CONFIG_USB_PD_ALT_MODE

extern int tcpm_dp_attention(
	struct tcpc_device *tcpc_dev, uint32_t dp_status);

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
extern int tcpm_dp_status_update(
	struct tcpc_device *tcpc_dev, uint32_t dp_status);
extern int tcpm_dp_configuration(
	struct tcpc_device *tcpc_dev, uint32_t dp_config);
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */

#endif	/* CONFIG_USB_PD_ALT_MODE */

/* Notify TCPM */

extern int tcpm_notify_vbus_stable(struct tcpc_device *tcpc_dev);

#endif /* TCPM_H_ */
