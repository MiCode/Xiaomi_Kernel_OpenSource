/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#ifndef _QMI_H_
#define _QMI_H_

#define ATH10K_SNOC_EVENT_PENDING		2989
#define ATH10K_SNOC_EVENT_SYNC			BIT(0)
#define ATH10K_SNOC_EVENT_UNINTERRUPTIBLE	BIT(1)
#define ATH10K_SNOC_WLAN_FW_READY_TIMEOUT	8000

#define WLFW_SERVICE_INS_ID_V01		0
#define WLFW_CLIENT_ID			0x41544851
#define WLFW_TIMEOUT_MS			20000

enum ath10k_snoc_driver_event_type {
	ATH10K_SNOC_DRIVER_EVENT_SERVER_ARRIVE,
	ATH10K_SNOC_DRIVER_EVENT_SERVER_EXIT,
	ATH10K_SNOC_DRIVER_EVENT_FW_READY_IND,
	ATH10K_SNOC_DRIVER_EVENT_MAX,
};

/* enum ath10k_driver_mode: ath10k driver mode
 * @ATH10K_MISSION: mission mode
 * @ATH10K_FTM: ftm mode
 * @ATH10K_EPPING: epping mode
 * @ATH10K_OFF: off mode
 */
enum ath10k_driver_mode {
	ATH10K_MISSION,
	ATH10K_FTM,
	ATH10K_EPPING,
	ATH10K_OFF
};

/* struct ath10k_ce_tgt_pipe_cfg: target pipe configuration
 * @pipe_num: pipe number
 * @pipe_dir: pipe direction
 * @nentries: entries in pipe
 * @nbytes_max: pipe max size
 * @flags: pipe flags
 * @reserved: reserved
 */
struct ath10k_ce_tgt_pipe_cfg {
	u32 pipe_num;
	u32 pipe_dir;
	u32 nentries;
	u32 nbytes_max;
	u32 flags;
	u32 reserved;
};

/* struct ath10k_ce_svc_pipe_cfg: service pipe configuration
 * @service_id: target version
 * @pipe_dir: pipe direction
 * @pipe_num: pipe number
 */
struct ath10k_ce_svc_pipe_cfg {
	u32 service_id;
	u32 pipe_dir;
	u32 pipe_num;
};

/* struct ath10k_shadow_reg_cfg: shadow register configuration
 * @ce_id: copy engine id
 * @reg_offset: offset to copy engine
 */
struct ath10k_shadow_reg_cfg {
	u16 ce_id;
	u16 reg_offset;
};

/* struct ath10k_wlan_enable_cfg: wlan enable configuration
 * @num_ce_tgt_cfg: no of ce target configuration
 * @ce_tgt_cfg: target ce configuration
 * @num_ce_svc_pipe_cfg: no of ce service configuration
 * @ce_svc_cfg: ce service configuration
 * @num_shadow_reg_cfg: no of shadow registers
 * @shadow_reg_cfg: shadow register configuration
 */
struct ath10k_wlan_enable_cfg {
	u32 num_ce_tgt_cfg;
	struct ath10k_ce_tgt_pipe_cfg *ce_tgt_cfg;
	u32 num_ce_svc_pipe_cfg;
	struct ath10k_ce_svc_pipe_cfg *ce_svc_cfg;
	u32 num_shadow_reg_cfg;
	struct ath10k_shadow_reg_cfg *shadow_reg_cfg;
};

/* struct ath10k_snoc_qmi_driver_event: qmi driver event
 * event_handled: event handled by event work handler
 * sync: event synced
 * ret: event received return value
 * list: list to queue qmi event for process
 * type: driver event type
 * complete: completion for event handle complete
 * data: encapsulate driver data for event handler callback
 */
struct ath10k_snoc_qmi_driver_event {
	atomic_t event_handled;
	bool sync;
	int ret;
	struct list_head list;
	enum ath10k_snoc_driver_event_type type;
	struct completion complete;
	void *data;
};

/* struct ath10k_snoc_qmi_config: qmi service configuration
 * fw_ready: wlan firmware ready for wlan operation
 * msa_ready: wlan firmware msa memory ready for board data download
 * server_connected: qmi server connected
 * event_work: QMI event work
 * event_list: QMI event list
 * qmi_recv_msg_work: QMI message receive work
 * event_wq: QMI event work queue
 * wlfw_clnt_nb: WLAN firmware indication callback
 * wlfw_clnt: QMI notifier handler for wlan firmware
 * qmi_ev_list: QMI event list
 * event_lock: spinlock for qmi event work queue
 */
struct ath10k_snoc_qmi_config {
	atomic_t fw_ready;
	bool msa_ready;
	atomic_t server_connected;
	struct work_struct event_work;
	struct list_head event_list;
	struct work_struct qmi_recv_msg_work;
	struct workqueue_struct *event_wq;
	struct notifier_block wlfw_clnt_nb;
	struct qmi_handle *wlfw_clnt;
	struct ath10k_snoc_qmi_driver_event
			qmi_ev_list[ATH10K_SNOC_DRIVER_EVENT_MAX];
	spinlock_t event_lock; /* spinlock for qmi event work queue */
};

int ath10k_snoc_pd_restart_enable(struct ath10k *ar);
int ath10k_snoc_modem_ssr_register_notifier(struct ath10k *ar);
int ath10k_snoc_modem_ssr_unregister_notifier(struct ath10k *ar);
int ath10k_snoc_pdr_unregister_notifier(struct ath10k *ar);
int ath10k_snoc_start_qmi_service(struct ath10k *ar);
void ath10k_snoc_stop_qmi_service(struct ath10k *ar);
int ath10k_snoc_qmi_wlan_enable(struct ath10k *ar,
				struct ath10k_wlan_enable_cfg *config,
				enum ath10k_driver_mode mode,
				const char *host_version);
int ath10k_snoc_qmi_wlan_disable(struct ath10k *ar);
#endif
