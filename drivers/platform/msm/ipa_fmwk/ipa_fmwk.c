// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/ipa_fmwk.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/list.h>

#define IPA_FMWK_DISPATCH_RETURN(api, p...) \
	do { \
		if (!ipa_fmwk_ctx) { \
			pr_err("%s:%d IPA framework was not inited\n", \
				__func__, __LINE__); \
			ret = -EPERM; \
		} \
		else { \
			if (ipa_fmwk_ctx->api) { \
				pr_debug("enter %s\n", __func__);\
				ret = ipa_fmwk_ctx->api(p); \
				pr_debug("exit %s\n", __func__);\
			} else { \
				WARN(1, \
					"%s was not registered on ipa_fmwk\n", \
						__func__); \
				ret = -EPERM; \
			} \
		} \
	} while (0)

#define IPA_FMWK_DISPATCH_RETURN_DP(api, p...) \
	do { \
		if (!ipa_fmwk_ctx) { \
			pr_err("%s:%d IPA framework was not inited\n", \
				__func__, __LINE__); \
			ret = -EPERM; \
		} \
		else { \
			if (ipa_fmwk_ctx->api) { \
				ret = ipa_fmwk_ctx->api(p); \
			} else { \
				WARN(1, \
					"%s was not registered on ipa_fmwk\n", \
						__func__); \
				ret = -EPERM; \
			} \
		} \
	} while (0)

#define IPA_FMWK_DISPATCH(api, p...) \
	do { \
		if (!ipa_fmwk_ctx) \
			pr_err("%s:%d IPA framework is not supported\n", \
				__func__, __LINE__); \
		else { \
			if (ipa_fmwk_ctx->api) { \
				pr_debug("enter %s\n", __func__);\
				ipa_fmwk_ctx->api(p); \
				pr_debug("exit %s\n", __func__);\
			} else { \
				WARN(1, \
					"%s was not registered on ipa_fmwk\n", \
						__func__); \
			} \
		} \
	} while (0)

#define IPA_FMWK_RETURN_BOOL(api, p...) \
	do { \
		if (!ipa_fmwk_ctx) { \
			pr_err("%s:%d IPA framework is not supported\n", \
				__func__, __LINE__); \
			ret = false; \
		} \
		else { \
			if (ipa_fmwk_ctx->api) { \
				pr_debug("enter %s\n", __func__);\
				ret = ipa_fmwk_ctx->api(p); \
				pr_debug("exit %s\n", __func__);\
			} else { \
				WARN(1, \
					"%s was not registered on ipa_fmwk\n", \
						__func__); \
				ret = false; \
			} \
		} \
	} while (0)

/**
 * struct ipa_ready_cb_info - A list of all the registrations
 *  for an indication of IPA driver readiness
 *
 * @link: linked list link
 * @ready_cb: callback
 * @user_data: User data
 *
 */
struct ipa_ready_cb_info {
	struct list_head link;
	ipa_ready_cb ready_cb;
	void *user_data;
};

struct ipa_fmwk_contex {
	bool ipa_ready;
	struct list_head ipa_ready_cb_list;
	struct mutex lock;
	ipa_uc_ready_cb uc_ready_cb;
	void *uc_ready_priv;
	struct ipa_eth_ready *eth_ready_info;
	enum ipa_uc_offload_proto proto;

	/* ipa core driver APIs */
	int (*ipa_tx_dp)(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

	enum ipa_hw_type (*ipa_get_hw_type)(void);

	int (*ipa_get_smmu_params)(struct ipa_smmu_in_params *in,
		struct ipa_smmu_out_params *out);

	int (*ipa_is_vlan_mode)(enum ipa_vlan_ifaces iface, bool *res);

	bool (*ipa_get_lan_rx_napi)(void);

	int (*ipa_dma_init)(void);

	int (*ipa_dma_enable)(void);

	int (*ipa_dma_disable)(void);

	int (*ipa_dma_sync_memcpy)(u64 dest, u64 src, int len);

	int (*ipa_dma_async_memcpy)(u64 dest, u64 src, int len,
		void (*user_cb)(void *user1), void *user_param);

	void (*ipa_dma_destroy)(void);

	int (*ipa_get_ep_mapping)(enum ipa_client_type client);

	int (*ipa_send_msg)(struct ipa_msg_meta *meta, void *buff,
		ipa_msg_free_fn callback);

	void (*ipa_free_skb)(struct ipa_rx_data *data);

	int (*ipa_setup_sys_pipe)(struct ipa_sys_connect_params *sys_in,
		u32 *clnt_hdl);

	int (*ipa_teardown_sys_pipe)(u32 clnt_hdl);

	int (*ipa_get_wdi_stats)(struct IpaHwStatsWDIInfoData_t *stats);


	int (*ipa_uc_bw_monitor)(struct ipa_wdi_bw_info *info);

	int (*ipa_broadcast_wdi_quota_reach_ind)(uint32_t fid,
		uint64_t num_bytes);

	int (*ipa_uc_wdi_get_dbpa)(struct ipa_wdi_db_params *out);

	int (*ipa_cfg_ep_ctrl)(u32 clnt_hdl,
		const struct ipa_ep_cfg_ctrl *ep_ctrl);

	int (*ipa_add_rt_rule)(struct ipa_ioc_add_rt_rule *rules);

	int (*ipa_put_rt_tbl)(u32 rt_tbl_hdl);

	int (*ipa_register_intf)(const char *name,
		const struct ipa_tx_intf *tx,
		const struct ipa_rx_intf *rx);

	int (*ipa_set_aggr_mode)(enum ipa_aggr_mode mode);

	int (*ipa_set_qcncm_ndp_sig)(char sig[3]);

	int (*ipa_set_single_ndp_per_mbim)(bool enable);

	int (*ipa_add_interrupt_handler)(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data);

	int (*ipa_restore_suspend_handler)(void);

	const struct ipa_gsi_ep_config *(*ipa_get_gsi_ep_info)(
		enum ipa_client_type client);

	int (*ipa_stop_gsi_channel)(u32 clnt_hdl);

	int (*ipa_rmnet_ctl_xmit)(struct sk_buff *skb);

	int (*ipa_register_rmnet_ctl_cb)(
		void (*ipa_rmnet_ctl_ready_cb)(void *user_data1),
		void *user_data1,
		void (*ipa_rmnet_ctl_stop_cb)(void *user_data2),
		void *user_data2,
		void (*ipa_rmnet_ctl_rx_notify_cb)(
			void *user_data3, void *rx_data),
		void *user_data3);

	int (*ipa_unregister_rmnet_ctl_cb)(void);

	int (*ipa_add_hdr)(struct ipa_ioc_add_hdr *hdrs);

	int (*ipa_del_hdr)(struct ipa_ioc_del_hdr *hdls);

	int (*ipa_get_hdr)(struct ipa_ioc_get_hdr *lookup);

	int (*ipa_deregister_intf)(const char *name);

	/* rmnet_ll APIs */
	int (*ipa_rmnet_ll_xmit)(struct sk_buff *skb);

	int (*ipa_register_rmnet_ll_cb)(
		void (*ipa_rmnet_ll_ready_cb)(void *user_data1),
		void *user_data1,
		void (*ipa_rmnet_ll_stop_cb)(void *user_data2),
		void *user_data2,
		void (*ipa_rmnet_ll_rx_notify_cb)(
			void *user_data3, void *rx_data),
		void *user_data3);

	int (*ipa_unregister_rmnet_ll_cb)(void);

	int (*ipa_register_notifier)(void *fn_ptr);

	int (*ipa_unregister_notifier)(void *fn_ptr);

	/* ipa_usb APIs */
	int (*ipa_usb_init_teth_prot)(enum ipa_usb_teth_prot teth_prot,
		struct ipa_usb_teth_params *teth_params,
		int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
			void *),
		void *user_data);

	int (*ipa_usb_xdci_connect)(
		struct ipa_usb_xdci_chan_params *ul_chan_params,
		struct ipa_usb_xdci_chan_params *dl_chan_params,
		struct ipa_req_chan_out_params *ul_out_params,
		struct ipa_req_chan_out_params *dl_out_params,
		struct ipa_usb_xdci_connect_params *connect_params);

	int (*ipa_usb_xdci_disconnect)(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
		enum ipa_usb_teth_prot teth_prot);

	int (*ipa_usb_deinit_teth_prot)(enum ipa_usb_teth_prot teth_prot);

	int (*ipa_usb_xdci_suspend)(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
		enum ipa_usb_teth_prot teth_prot,
		bool with_remote_wakeup);

	int (*ipa_usb_xdci_resume)(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
		enum ipa_usb_teth_prot teth_prot);

	bool (*ipa_usb_is_teth_prot_connected)(
		enum ipa_usb_teth_prot usb_teth_prot);

	/* ipa_wdi3 APIs */
	int (*ipa_wdi_init)(struct ipa_wdi_init_in_params *in,
		struct ipa_wdi_init_out_params *out);

	int (*ipa_wdi_cleanup)(void);

	int (*ipa_wdi_reg_intf)(
		struct ipa_wdi_reg_intf_in_params *in);

	int (*ipa_wdi_dereg_intf)(const char *netdev_name);

	int (*ipa_qdss_conn_pipes)(struct ipa_qdss_conn_in_params *in,
		struct ipa_qdss_conn_out_params *out);

	int (*ipa_qdss_disconn_pipes)(void);

	int (*ipa_wdi_conn_pipes)(struct ipa_wdi_conn_in_params *in,
		struct ipa_wdi_conn_out_params *out);

	int (*ipa_wdi_disconn_pipes)(void);

	int (*ipa_wdi_enable_pipes)(void);

	int (*ipa_wdi_disable_pipes)(void);

	int (*ipa_wdi_set_perf_profile)(struct ipa_wdi_perf_profile *profile);

	int (*ipa_wdi_create_smmu_mapping)(u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	int (*ipa_wdi_release_smmu_mapping)(u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	int (*ipa_wdi_get_stats)(struct IpaHwStatsWDIInfoData_t *stats);

	int (*ipa_wdi_bw_monitor)(struct ipa_wdi_bw_info *info);

	int (*ipa_wdi_sw_stats)(struct ipa_wdi_tx_info *info);

	int (*ipa_get_wdi_version)(void);

	int (*ipa_enable_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_disable_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_resume_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_suspend_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_connect_wdi_pipe)(struct ipa_wdi_in_params *in,
			struct ipa_wdi_out_params *out);

	int (*ipa_disconnect_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_reg_uc_rdyCB)(struct ipa_wdi_uc_ready_params *param);

	int (*ipa_dereg_uc_rdyCB)(void);

	bool (*ipa_wdi_is_tx1_used)(void);

	int (*ipa_wdi_get_capabilities)(struct ipa_wdi_capabilities_out_params *out);

	int (*ipa_wdi_init_per_inst)(struct ipa_wdi_init_in_params *in,
		struct ipa_wdi_init_out_params *out);

	int (*ipa_wdi_cleanup_per_inst)(u32 hdl);

	int (*ipa_wdi_reg_intf_per_inst)(
		struct ipa_wdi_reg_intf_in_params *in);

	int (*ipa_wdi_dereg_intf_per_inst)(const char *netdev_name, u32 hdl);

	int (*ipa_wdi_conn_pipes_per_inst)(struct ipa_wdi_conn_in_params *in,
		struct ipa_wdi_conn_out_params *out);

	int (*ipa_wdi_disconn_pipes_per_inst)(u32 hdl);

	int (*ipa_wdi_enable_pipes_per_inst)(u32 hdl);

	int (*ipa_wdi_disable_pipes_per_inst)(u32 hdl);

	int (*ipa_wdi_set_perf_profile_per_inst)(u32 hdl, struct ipa_wdi_perf_profile *profile);

	int (*ipa_wdi_create_smmu_mapping_per_inst)(u32 hdl, u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	int (*ipa_wdi_release_smmu_mapping_per_inst)(u32 hdl, u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	/* ipa_gsb APIs*/
	int (*ipa_bridge_init)(struct ipa_bridge_init_params *params, u32 *hdl);

	int (*ipa_bridge_connect)(u32 hdl);

	int (*ipa_bridge_set_perf_profile)(u32 hdl, u32 bandwidth);

	int (*ipa_bridge_disconnect)(u32 hdl);

	int (*ipa_bridge_suspend)(u32 hdl);

	int (*ipa_bridge_resume)(u32 hdl);

	int (*ipa_bridge_tx_dp)(u32 hdl, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

	int (*ipa_bridge_cleanup)(u32 hdl);

	/* ipa_uc_offload APIs */

	int (*ipa_uc_offload_reg_intf)(
		struct ipa_uc_offload_intf_params *in,
		struct ipa_uc_offload_out_params *out);

	int (*ipa_uc_offload_cleanup)(u32 clnt_hdl);

	int (*ipa_uc_offload_conn_pipes)(
		struct ipa_uc_offload_conn_in_params *in,
		struct ipa_uc_offload_conn_out_params *out);

	int (*ipa_uc_offload_disconn_pipes)(u32 clnt_hdl);

	int (*ipa_set_perf_profile)(struct ipa_perf_profile *profile);

	int (*ipa_uc_offload_reg_rdyCB)(struct ipa_uc_ready_params *param);

	void (*ipa_uc_offload_dereg_rdyCB)(enum ipa_uc_offload_proto proto);

	/* ipa_mhi APIs */
	int (*ipa_mhi_init)(struct ipa_mhi_init_params *params);

	int (*ipa_mhi_start)(struct ipa_mhi_start_params *params);

	int (*ipa_mhi_connect_pipe)(struct ipa_mhi_connect_params *in,
		u32 *clnt_hdl);

	int (*ipa_mhi_disconnect_pipe)(u32 clnt_hdl);

	int (*ipa_mhi_suspend)(bool force);

	int (*ipa_mhi_resume)(void);

	void (*ipa_mhi_destroy)(void);

	int (*ipa_mhi_handle_ipa_config_req)(
		struct ipa_config_req_msg_v01 *config_req);

	int (*ipa_mhi_update_mstate)(enum ipa_mhi_mstate mstate_info);

	/* ipa_wigig APIs */
	int (*ipa_wigig_init)(struct ipa_wigig_init_in_params *in,
		struct ipa_wigig_init_out_params *out);

	int (*ipa_wigig_cleanup)(void);

	bool (*ipa_wigig_is_smmu_enabled)(void);

	int (*ipa_wigig_reg_intf)(struct ipa_wigig_reg_intf_in_params *in);

	int (*ipa_wigig_dereg_intf)(const char *netdev_name);

	int (*ipa_wigig_conn_rx_pipe)(struct ipa_wigig_conn_rx_in_params *in,
		struct ipa_wigig_conn_out_params *out);

	int (*ipa_wigig_conn_rx_pipe_smmu)(
		struct ipa_wigig_conn_rx_in_params_smmu *in,
		struct ipa_wigig_conn_out_params *out);

	int (*ipa_wigig_conn_client)(struct ipa_wigig_conn_tx_in_params *in,
		struct ipa_wigig_conn_out_params *out);

	int (*ipa_wigig_conn_client_smmu)(
		struct ipa_wigig_conn_tx_in_params_smmu *in,
		struct ipa_wigig_conn_out_params *out);

	int (*ipa_wigig_disconn_pipe)(enum ipa_client_type client);

	int (*ipa_wigig_enable_pipe)(enum ipa_client_type client);

	int (*ipa_wigig_disable_pipe)(enum ipa_client_type client);

	int (*ipa_wigig_tx_dp)(enum ipa_client_type dst, struct sk_buff *skb);

	int (*ipa_wigig_set_perf_profile)(u32 max_supported_bw_mbps);

	int (*ipa_wigig_save_regs)(void);

	/* ipa eth APIs */
	int (*ipa_eth_register_ready_cb)(struct ipa_eth_ready *ready_info);

	int (*ipa_eth_unregister_ready_cb)(struct ipa_eth_ready *ready_info);

	int (*ipa_eth_client_conn_pipes)(struct ipa_eth_client *client);

	int (*ipa_eth_client_disconn_pipes)(struct ipa_eth_client *client);

	int (*ipa_eth_client_reg_intf)(struct ipa_eth_intf_info *intf);

	int (*ipa_eth_client_unreg_intf)(struct ipa_eth_intf_info *intf);

	int (*ipa_eth_client_set_perf_profile)(struct ipa_eth_client *client,
		struct ipa_eth_perf_profile *profile);

	int (*ipa_get_default_aggr_time_limit)(enum ipa_client_type client,
		u32 *default_aggr_time_limit);

	enum ipa_client_type (*ipa_eth_get_ipa_client_type_from_eth_type)(
		enum ipa_eth_client_type eth_client_type, enum ipa_eth_pipe_direction dir);

	bool (*ipa_eth_client_exist)(
		enum ipa_eth_client_type eth_client_type, int inst_id);
	int (*ipa_add_socksv5_conn)(struct ipa_socksv5_info *info);
	int (*ipa_del_socksv5_conn)(uint32_t handle);
};

static struct ipa_fmwk_contex *ipa_fmwk_ctx;

static inline void ipa_trigger_ipa_ready_cbs(void)
{
	struct ipa_ready_cb_info *info;
	struct ipa_ready_cb_info *next;

	/* Call all the CBs */
	list_for_each_entry_safe(info, next,
		&ipa_fmwk_ctx->ipa_ready_cb_list, link) {
		if (info->ready_cb)
			info->ready_cb(info->user_data);

		list_del(&info->link);
		kfree(info);
	}
}

static inline void ipa_late_register_ready_cb(void)
{
	if (ipa_fmwk_ctx->uc_ready_cb) {
		struct ipa_uc_ready_params param;

		param.notify = ipa_fmwk_ctx->uc_ready_cb;
		param.priv = ipa_fmwk_ctx->uc_ready_priv;
		param.proto = ipa_fmwk_ctx->proto;

		ipa_fmwk_ctx->ipa_uc_offload_reg_rdyCB(&param);
		/* if uc is already ready, client expects cb to be called */
		if (param.is_uC_ready) {
			ipa_fmwk_ctx->uc_ready_cb(
				ipa_fmwk_ctx->uc_ready_priv);
		}
	}

	if (ipa_fmwk_ctx->eth_ready_info) {
		/* just late call to ipa_eth_register_ready_cb */
		ipa_fmwk_ctx->ipa_eth_register_ready_cb(
			ipa_fmwk_ctx->eth_ready_info);
		/* nobody cares anymore about ready_info->is_eth_ready since
		 * if we got here it means that we already returned false there
		 */
	}
}

/* registration API for IPA core module */
int ipa_fmwk_register_ipa(const struct ipa_core_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	mutex_lock(&ipa_fmwk_ctx->lock);
	if (ipa_fmwk_ctx->ipa_ready) {
		pr_err("ipa core driver API were already registered\n");
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_tx_dp = in->ipa_tx_dp;
	ipa_fmwk_ctx->ipa_get_hw_type = in->ipa_get_hw_type;
	ipa_fmwk_ctx->ipa_is_vlan_mode = in->ipa_is_vlan_mode;
	ipa_fmwk_ctx->ipa_get_smmu_params = in->ipa_get_smmu_params;
	ipa_fmwk_ctx->ipa_get_lan_rx_napi = in->ipa_get_lan_rx_napi;
	ipa_fmwk_ctx->ipa_dma_init = in->ipa_dma_init;
	ipa_fmwk_ctx->ipa_dma_enable = in->ipa_dma_enable;
	ipa_fmwk_ctx->ipa_dma_disable = in->ipa_dma_disable;
	ipa_fmwk_ctx->ipa_dma_sync_memcpy = in->ipa_dma_sync_memcpy;
	ipa_fmwk_ctx->ipa_dma_async_memcpy = in->ipa_dma_async_memcpy;
	ipa_fmwk_ctx->ipa_dma_destroy = in->ipa_dma_destroy;
	ipa_fmwk_ctx->ipa_get_ep_mapping = in->ipa_get_ep_mapping;
	ipa_fmwk_ctx->ipa_send_msg = in->ipa_send_msg;
	ipa_fmwk_ctx->ipa_free_skb = in->ipa_free_skb;
	ipa_fmwk_ctx->ipa_setup_sys_pipe = in->ipa_setup_sys_pipe;
	ipa_fmwk_ctx->ipa_teardown_sys_pipe = in->ipa_teardown_sys_pipe;
	ipa_fmwk_ctx->ipa_get_wdi_stats = in->ipa_get_wdi_stats;
	ipa_fmwk_ctx->ipa_uc_bw_monitor = in->ipa_uc_bw_monitor;
	ipa_fmwk_ctx->ipa_broadcast_wdi_quota_reach_ind =
		in->ipa_broadcast_wdi_quota_reach_ind;
	ipa_fmwk_ctx->ipa_uc_wdi_get_dbpa = in->ipa_uc_wdi_get_dbpa;
	ipa_fmwk_ctx->ipa_cfg_ep_ctrl = in->ipa_cfg_ep_ctrl;
	ipa_fmwk_ctx->ipa_add_rt_rule = in->ipa_add_rt_rule;
	ipa_fmwk_ctx->ipa_put_rt_tbl = in->ipa_put_rt_tbl;
	ipa_fmwk_ctx->ipa_register_intf = in->ipa_register_intf;
	ipa_fmwk_ctx->ipa_deregister_intf = in->ipa_deregister_intf;
	ipa_fmwk_ctx->ipa_add_hdr = in->ipa_add_hdr;
	ipa_fmwk_ctx->ipa_del_hdr = in->ipa_del_hdr;
	ipa_fmwk_ctx->ipa_get_hdr = in->ipa_get_hdr;
	ipa_fmwk_ctx->ipa_set_aggr_mode = in->ipa_set_aggr_mode;
	ipa_fmwk_ctx->ipa_set_qcncm_ndp_sig = in->ipa_set_qcncm_ndp_sig;
	ipa_fmwk_ctx->ipa_set_single_ndp_per_mbim =
		in->ipa_set_single_ndp_per_mbim;
	ipa_fmwk_ctx->ipa_add_interrupt_handler = in->ipa_add_interrupt_handler;
	ipa_fmwk_ctx->ipa_restore_suspend_handler =
		in->ipa_restore_suspend_handler;
	ipa_fmwk_ctx->ipa_get_gsi_ep_info = in->ipa_get_gsi_ep_info;
	ipa_fmwk_ctx->ipa_stop_gsi_channel = in->ipa_stop_gsi_channel;
	ipa_fmwk_ctx->ipa_rmnet_ctl_xmit = in->ipa_rmnet_ctl_xmit;
	ipa_fmwk_ctx->ipa_register_rmnet_ctl_cb = in->ipa_register_rmnet_ctl_cb;
	ipa_fmwk_ctx->ipa_unregister_rmnet_ctl_cb =
		in->ipa_unregister_rmnet_ctl_cb;
	ipa_fmwk_ctx->ipa_get_default_aggr_time_limit = in->ipa_get_default_aggr_time_limit;
	ipa_fmwk_ctx->ipa_enable_wdi_pipe = in->ipa_enable_wdi_pipe;
	ipa_fmwk_ctx->ipa_disable_wdi_pipe = in->ipa_disable_wdi_pipe;
	ipa_fmwk_ctx->ipa_resume_wdi_pipe = in->ipa_resume_wdi_pipe;
	ipa_fmwk_ctx->ipa_suspend_wdi_pipe = in->ipa_suspend_wdi_pipe;
	ipa_fmwk_ctx->ipa_connect_wdi_pipe = in->ipa_connect_wdi_pipe;
	ipa_fmwk_ctx->ipa_disconnect_wdi_pipe = in->ipa_disconnect_wdi_pipe;
	ipa_fmwk_ctx->ipa_reg_uc_rdyCB = in->ipa_uc_reg_rdyCB;
	ipa_fmwk_ctx->ipa_dereg_uc_rdyCB = in->ipa_uc_dereg_rdyCB;
	ipa_fmwk_ctx->ipa_rmnet_ll_xmit = in->ipa_rmnet_ll_xmit;
	ipa_fmwk_ctx->ipa_register_rmnet_ll_cb = in->ipa_register_rmnet_ll_cb;
	ipa_fmwk_ctx->ipa_unregister_rmnet_ll_cb =
		in->ipa_unregister_rmnet_ll_cb;
	ipa_fmwk_ctx->ipa_register_notifier =
		in->ipa_unregister_notifier;
	ipa_fmwk_ctx->ipa_add_socksv5_conn = in->ipa_add_socksv5_conn;
	ipa_fmwk_ctx->ipa_del_socksv5_conn = in->ipa_del_socksv5_conn;

	ipa_fmwk_ctx->ipa_ready = true;
	ipa_trigger_ipa_ready_cbs();

	ipa_late_register_ready_cb();
	mutex_unlock(&ipa_fmwk_ctx->lock);

	pr_info("IPA driver is now in ready state\n");
	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa);

bool ipa_is_ready(void)
{
	if (!ipa_fmwk_ctx)
		return false;

	return ipa_fmwk_ctx->ipa_ready;
}
EXPORT_SYMBOL(ipa_is_ready);

int ipa_register_ipa_ready_cb(void(*ipa_ready_cb)(void *user_data),
	void *user_data)
{
	struct ipa_ready_cb_info *cb_info = NULL;

	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	mutex_lock(&ipa_fmwk_ctx->lock);
	if (ipa_fmwk_ctx->ipa_ready) {
		pr_debug("IPA driver finished initialization already\n");
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return -EEXIST;
	}

	cb_info = kmalloc(sizeof(struct ipa_ready_cb_info), GFP_KERNEL);
	if (!cb_info) {
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return -ENOMEM;
	}

	cb_info->ready_cb = ipa_ready_cb;
	cb_info->user_data = user_data;

	list_add_tail(&cb_info->link, &ipa_fmwk_ctx->ipa_ready_cb_list);
	mutex_unlock(&ipa_fmwk_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_register_ipa_ready_cb);

/* ipa core driver API wrappers*/

int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
	struct ipa_tx_meta *metadata)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_tx_dp,
		dst, skb, metadata);

	return ret;
}
EXPORT_SYMBOL(ipa_tx_dp);

enum ipa_hw_type ipa_get_hw_type(void)
{
	enum ipa_hw_type ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_get_hw_type);

	return ret;
}
EXPORT_SYMBOL(ipa_get_hw_type);

int ipa_is_vlan_mode(enum ipa_vlan_ifaces iface, bool *res)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_is_vlan_mode,
		iface, res);

	return ret;
}
EXPORT_SYMBOL(ipa_is_vlan_mode);

int ipa_get_smmu_params(struct ipa_smmu_in_params *in,
	struct ipa_smmu_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_get_smmu_params,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_get_smmu_params);

bool ipa_get_lan_rx_napi(void)
{
	int ret;

	IPA_FMWK_RETURN_BOOL(ipa_get_lan_rx_napi);

	return ret;
}
EXPORT_SYMBOL(ipa_get_lan_rx_napi);

int ipa_dma_init(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_dma_init);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_init);

int ipa_dma_enable(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_dma_enable);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_enable);

int ipa_dma_disable(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_dma_disable);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_disable);

int ipa_dma_sync_memcpy(u64 dest, u64 src, int len)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_dma_sync_memcpy,
		dest, src, len);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_sync_memcpy);

int ipa_dma_async_memcpy(u64 dest, u64 src, int len,
	void (*user_cb)(void *user1), void *user_param)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_dma_async_memcpy,
		dest, src, len, user_cb, user_param);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_async_memcpy);

void ipa_dma_destroy(void)
{
	IPA_FMWK_DISPATCH(ipa_dma_destroy);
}
EXPORT_SYMBOL(ipa_dma_destroy);

int ipa_get_ep_mapping(enum ipa_client_type client)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_get_ep_mapping,
		client);

	return ret;
}
EXPORT_SYMBOL(ipa_get_ep_mapping);

int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
	ipa_msg_free_fn callback)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_send_msg,
		meta, buff, callback);

	return ret;
}
EXPORT_SYMBOL(ipa_send_msg);

void ipa_free_skb(struct ipa_rx_data *data)
{
	IPA_FMWK_DISPATCH(ipa_free_skb, data);
}
EXPORT_SYMBOL(ipa_free_skb);

int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_setup_sys_pipe, sys_in, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_setup_sys_pipe);

int ipa_teardown_sys_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_teardown_sys_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_teardown_sys_pipe);

int ipa_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_get_wdi_stats, stats);

	return ret;
}
EXPORT_SYMBOL(ipa_get_wdi_stats);

int ipa_uc_bw_monitor(struct ipa_wdi_bw_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_uc_bw_monitor, info);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_bw_monitor);

int ipa_broadcast_wdi_quota_reach_ind(uint32_t fid,
	uint64_t num_bytes)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_broadcast_wdi_quota_reach_ind,
		fid, num_bytes);

	return ret;
}
EXPORT_SYMBOL(ipa_broadcast_wdi_quota_reach_ind);

int ipa_uc_wdi_get_dbpa(
	struct ipa_wdi_db_params *param)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_uc_wdi_get_dbpa, param);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_wdi_get_dbpa);

int ipa_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_cfg_ep_ctrl, clnt_hdl, ep_ctrl);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_ctrl);

int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_add_rt_rule, rules);

	return ret;
}
EXPORT_SYMBOL(ipa_add_rt_rule);

int ipa_put_rt_tbl(u32 rt_tbl_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_put_rt_tbl, rt_tbl_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_put_rt_tbl);

int ipa_register_intf(const char *name,
	const struct ipa_tx_intf *tx,
	const struct ipa_rx_intf *rx)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_register_intf, name, tx, rx);

	return ret;
}
EXPORT_SYMBOL(ipa_register_intf);

int ipa_deregister_intf(const char *name)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_deregister_intf, name);

	return ret;
}
EXPORT_SYMBOL(ipa_deregister_intf);

int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_add_hdr, hdrs);

	return ret;
}
EXPORT_SYMBOL(ipa_add_hdr);

int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_del_hdr, hdls);

	return ret;
}
EXPORT_SYMBOL(ipa_del_hdr);

int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_get_hdr, lookup);

	return ret;
}
EXPORT_SYMBOL(ipa_get_hdr);

int ipa_set_aggr_mode(enum ipa_aggr_mode mode)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_set_aggr_mode, mode);

	return ret;
}
EXPORT_SYMBOL(ipa_set_aggr_mode);

int ipa_set_qcncm_ndp_sig(char sig[3])
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_set_qcncm_ndp_sig, sig);

	return ret;
}
EXPORT_SYMBOL(ipa_set_qcncm_ndp_sig);

int ipa_set_single_ndp_per_mbim(bool enable)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_set_single_ndp_per_mbim, enable);

	return ret;
}
EXPORT_SYMBOL(ipa_set_single_ndp_per_mbim);

int ipa_add_interrupt_handler(enum ipa_irq_type interrupt,
	ipa_irq_handler_t handler,
	bool deferred_flag,
	void *private_data)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_add_interrupt_handler,
		interrupt, handler, deferred_flag, private_data);

	return ret;
}
EXPORT_SYMBOL(ipa_add_interrupt_handler);

int ipa_restore_suspend_handler(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_restore_suspend_handler);

	return ret;
}
EXPORT_SYMBOL(ipa_restore_suspend_handler);

const struct ipa_gsi_ep_config *ipa_get_gsi_ep_info(
	enum ipa_client_type client)
{
	if (!ipa_fmwk_ctx || !ipa_fmwk_ctx->ipa_get_gsi_ep_info)
		return NULL;

	return ipa_fmwk_ctx->ipa_get_gsi_ep_info(client);
}
EXPORT_SYMBOL(ipa_get_gsi_ep_info);

int ipa_stop_gsi_channel(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_stop_gsi_channel, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_stop_gsi_channel);

int ipa_rmnet_ctl_xmit(struct sk_buff *skb)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_rmnet_ctl_xmit, skb);

	return ret;
}
EXPORT_SYMBOL(ipa_rmnet_ctl_xmit);

int ipa_register_rmnet_ctl_cb(
	void (*ipa_rmnet_ctl_ready_cb)(void *user_data1),
	void *user_data1,
	void (*ipa_rmnet_ctl_stop_cb)(void *user_data2),
	void *user_data2,
	void (*ipa_rmnet_ctl_rx_notify_cb)(
		void *user_data3, void *rx_data),
	void *user_data3)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_register_rmnet_ctl_cb,
		ipa_rmnet_ctl_ready_cb, user_data1,
		ipa_rmnet_ctl_stop_cb, user_data2,
		ipa_rmnet_ctl_rx_notify_cb, user_data3);

	return ret;
}
EXPORT_SYMBOL(ipa_register_rmnet_ctl_cb);

int ipa_unregister_rmnet_ctl_cb(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_unregister_rmnet_ctl_cb);

	return ret;
}
EXPORT_SYMBOL(ipa_unregister_rmnet_ctl_cb);

/* registration API for rmnet_ll module */
int ipa_rmnet_ll_xmit(struct sk_buff *skb)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_rmnet_ll_xmit, skb);

	return ret;
}
EXPORT_SYMBOL(ipa_rmnet_ll_xmit);

int ipa_register_rmnet_ll_cb(
	void (*ipa_rmnet_ll_ready_cb)(void *user_data1),
	void *user_data1,
	void (*ipa_rmnet_ll_stop_cb)(void *user_data2),
	void *user_data2,
	void (*ipa_rmnet_ll_rx_notify_cb)(
		void *user_data3, void *rx_data),
	void *user_data3)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_register_rmnet_ll_cb,
		ipa_rmnet_ll_ready_cb, user_data1,
		ipa_rmnet_ll_stop_cb, user_data2,
		ipa_rmnet_ll_rx_notify_cb, user_data3);

	return ret;
}
EXPORT_SYMBOL(ipa_register_rmnet_ll_cb);

int ipa_unregister_rmnet_ll_cb(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_unregister_rmnet_ll_cb);

	return ret;
}
EXPORT_SYMBOL(ipa_unregister_rmnet_ll_cb);

int ipa_add_socksv5_conn(struct ipa_socksv5_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_add_socksv5_conn, info);

	return ret;
}
EXPORT_SYMBOL(ipa_add_socksv5_conn);

int ipa_del_socksv5_conn(uint32_t handle)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_del_socksv5_conn, handle);

	return ret;
}
EXPORT_SYMBOL(ipa_del_socksv5_conn);

int ipa_register_notifier(void *fn_ptr)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_register_notifier, fn_ptr);

	return ret;
}
EXPORT_SYMBOL(ipa_register_notifier);

int ipa_unregister_notifier(void *fn_ptr)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_unregister_notifier, fn_ptr);

	return ret;
}
EXPORT_SYMBOL(ipa_unregister_notifier);

/* registration API for IPA usb module */
int ipa_fmwk_register_ipa_usb(const struct ipa_usb_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_usb_init_teth_prot ||
		ipa_fmwk_ctx->ipa_usb_xdci_connect ||
		ipa_fmwk_ctx->ipa_usb_xdci_disconnect ||
		ipa_fmwk_ctx->ipa_usb_deinit_teth_prot ||
		ipa_fmwk_ctx->ipa_usb_xdci_suspend ||
		ipa_fmwk_ctx->ipa_usb_xdci_resume ||
		ipa_fmwk_ctx->ipa_usb_is_teth_prot_connected) {
		pr_err("ipa_usb APIs were already initialized\n");
		return -EPERM;
	}
	ipa_fmwk_ctx->ipa_usb_init_teth_prot = in->ipa_usb_init_teth_prot;
	ipa_fmwk_ctx->ipa_usb_xdci_connect = in->ipa_usb_xdci_connect;
	ipa_fmwk_ctx->ipa_usb_xdci_disconnect = in->ipa_usb_xdci_disconnect;
	ipa_fmwk_ctx->ipa_usb_deinit_teth_prot = in->ipa_usb_deinit_teth_prot;
	ipa_fmwk_ctx->ipa_usb_xdci_suspend = in->ipa_usb_xdci_suspend;
	ipa_fmwk_ctx->ipa_usb_xdci_resume = in->ipa_usb_xdci_resume;
	ipa_fmwk_ctx->ipa_usb_is_teth_prot_connected =
		in->ipa_usb_is_teth_prot_connected;

	pr_info("ipa_usb registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa_usb);

/* ipa_usb API wrappers*/
int ipa_usb_init_teth_prot(enum ipa_usb_teth_prot teth_prot,
	struct ipa_usb_teth_params *teth_params,
	int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
		void *),
	void *user_data)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_init_teth_prot,
		teth_prot, teth_params, ipa_usb_notify_cb, user_data);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_init_teth_prot);

int ipa_usb_xdci_connect(struct ipa_usb_xdci_chan_params *ul_chan_params,
	struct ipa_usb_xdci_chan_params *dl_chan_params,
	struct ipa_req_chan_out_params *ul_out_params,
	struct ipa_req_chan_out_params *dl_out_params,
	struct ipa_usb_xdci_connect_params *connect_params)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_connect,
		ul_chan_params, dl_chan_params, ul_out_params,
		dl_out_params, connect_params);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_connect);

int ipa_usb_xdci_disconnect(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_disconnect,
		ul_clnt_hdl, dl_clnt_hdl, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_disconnect);

int ipa_usb_deinit_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_deinit_teth_prot,
		teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_deinit_teth_prot);

int ipa_usb_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot,
	bool with_remote_wakeup)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_suspend,
		ul_clnt_hdl, dl_clnt_hdl, teth_prot, with_remote_wakeup);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_suspend);

int ipa_usb_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_xdci_resume,
		ul_clnt_hdl, dl_clnt_hdl, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_resume);

bool ipa_usb_is_teth_prot_connected(enum ipa_usb_teth_prot usb_teth_prot)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_usb_is_teth_prot_connected,
		usb_teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_is_teth_prot_connected);

/* registration API for IPA wdi3 module */
int ipa_fmwk_register_ipa_wdi3(const struct ipa_wdi3_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_wdi_bw_monitor
		|| ipa_fmwk_ctx->ipa_wdi_init
		|| ipa_fmwk_ctx->ipa_wdi_cleanup
		|| ipa_fmwk_ctx->ipa_wdi_reg_intf
		|| ipa_fmwk_ctx->ipa_wdi_dereg_intf
		|| ipa_fmwk_ctx->ipa_wdi_conn_pipes
		|| ipa_fmwk_ctx->ipa_wdi_disconn_pipes
		|| ipa_fmwk_ctx->ipa_wdi_enable_pipes
		|| ipa_fmwk_ctx->ipa_wdi_disable_pipes
		|| ipa_fmwk_ctx->ipa_wdi_set_perf_profile
		|| ipa_fmwk_ctx->ipa_wdi_create_smmu_mapping
		|| ipa_fmwk_ctx->ipa_wdi_release_smmu_mapping
		|| ipa_fmwk_ctx->ipa_wdi_get_stats
		|| ipa_fmwk_ctx->ipa_get_wdi_version
		|| ipa_fmwk_ctx->ipa_wdi_sw_stats
		|| ipa_fmwk_ctx->ipa_wdi_is_tx1_used
		|| ipa_fmwk_ctx->ipa_wdi_get_capabilities
		|| ipa_fmwk_ctx->ipa_wdi_init_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_cleanup_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_reg_intf_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_dereg_intf_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_conn_pipes_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_disconn_pipes_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_enable_pipes_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_disable_pipes_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_set_perf_profile_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_create_smmu_mapping_per_inst
		|| ipa_fmwk_ctx->ipa_wdi_release_smmu_mapping_per_inst) {
		pr_err("ipa_wdi3 APIs were already initialized\n");
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_wdi_bw_monitor = in->ipa_wdi_bw_monitor;
	ipa_fmwk_ctx->ipa_wdi_init = in->ipa_wdi_init;
	ipa_fmwk_ctx->ipa_wdi_cleanup = in->ipa_wdi_cleanup;
	ipa_fmwk_ctx->ipa_wdi_reg_intf = in->ipa_wdi_reg_intf;
	ipa_fmwk_ctx->ipa_wdi_dereg_intf = in->ipa_wdi_dereg_intf;
	ipa_fmwk_ctx->ipa_wdi_conn_pipes = in->ipa_wdi_conn_pipes;
	ipa_fmwk_ctx->ipa_wdi_disconn_pipes = in->ipa_wdi_disconn_pipes;
	ipa_fmwk_ctx->ipa_wdi_enable_pipes = in->ipa_wdi_enable_pipes;
	ipa_fmwk_ctx->ipa_wdi_disable_pipes = in->ipa_wdi_disable_pipes;
	ipa_fmwk_ctx->ipa_wdi_set_perf_profile = in->ipa_wdi_set_perf_profile;
	ipa_fmwk_ctx->ipa_wdi_create_smmu_mapping =
		in->ipa_wdi_create_smmu_mapping;
	ipa_fmwk_ctx->ipa_wdi_release_smmu_mapping =
		in->ipa_wdi_release_smmu_mapping;
	ipa_fmwk_ctx->ipa_wdi_get_stats = in->ipa_wdi_get_stats;
	ipa_fmwk_ctx->ipa_wdi_sw_stats = in->ipa_wdi_sw_stats;
	ipa_fmwk_ctx->ipa_get_wdi_version = in->ipa_get_wdi_version;
	ipa_fmwk_ctx->ipa_wdi_is_tx1_used = in->ipa_wdi_is_tx1_used;
	ipa_fmwk_ctx->ipa_wdi_get_capabilities = in->ipa_wdi_get_capabilities;
	ipa_fmwk_ctx->ipa_wdi_init_per_inst = in->ipa_wdi_init_per_inst;
	ipa_fmwk_ctx->ipa_wdi_cleanup_per_inst = in->ipa_wdi_cleanup_per_inst;
	ipa_fmwk_ctx->ipa_wdi_reg_intf_per_inst = in->ipa_wdi_reg_intf_per_inst;
	ipa_fmwk_ctx->ipa_wdi_dereg_intf_per_inst =
		in->ipa_wdi_dereg_intf_per_inst;
	ipa_fmwk_ctx->ipa_wdi_conn_pipes_per_inst =
		in->ipa_wdi_conn_pipes_per_inst;
	ipa_fmwk_ctx->ipa_wdi_disconn_pipes_per_inst =
		in->ipa_wdi_disconn_pipes_per_inst;
	ipa_fmwk_ctx->ipa_wdi_enable_pipes_per_inst =
		in->ipa_wdi_enable_pipes_per_inst;
	ipa_fmwk_ctx->ipa_wdi_disable_pipes_per_inst =
		in->ipa_wdi_disable_pipes_per_inst;
	ipa_fmwk_ctx->ipa_wdi_set_perf_profile_per_inst =
		in->ipa_wdi_set_perf_profile_per_inst;
	ipa_fmwk_ctx->ipa_wdi_create_smmu_mapping_per_inst =
		in->ipa_wdi_create_smmu_mapping_per_inst;
	ipa_fmwk_ctx->ipa_wdi_release_smmu_mapping_per_inst =
		in->ipa_wdi_release_smmu_mapping_per_inst;
	pr_info("ipa_wdi3 registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa_wdi3);

/* ipa_wdi3 APIs */
int ipa_wdi_init(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_init,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_init);

bool ipa_wdi_is_tx1_used(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_is_tx1_used);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_is_tx1_used);

int ipa_wdi_cleanup(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_cleanup);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_cleanup);

int ipa_wdi_reg_intf(
	struct ipa_wdi_reg_intf_in_params *in)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_reg_intf,
		in);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_reg_intf);

int ipa_wdi_dereg_intf(const char *netdev_name)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_dereg_intf,
		netdev_name);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_dereg_intf);

int ipa_wdi_conn_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_conn_pipes,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_conn_pipes);

int ipa_wdi_disconn_pipes(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_disconn_pipes);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_disconn_pipes);

int ipa_wdi_enable_pipes(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_enable_pipes);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_enable_pipes);

int ipa_wdi_disable_pipes(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_disable_pipes);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_disable_pipes);

int ipa_wdi_set_perf_profile(struct ipa_wdi_perf_profile *profile)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_set_perf_profile,
		profile);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_set_perf_profile);

int ipa_wdi_create_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_wdi_create_smmu_mapping,
		num_buffers, info);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_create_smmu_mapping);

int ipa_wdi_release_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_wdi_release_smmu_mapping,
		num_buffers, info);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_release_smmu_mapping);

int ipa_wdi_get_capabilities(struct ipa_wdi_capabilities_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_get_capabilities, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_get_capabilities);

int ipa_wdi_init_per_inst(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_init_per_inst,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_init_per_inst);

int ipa_wdi_cleanup_per_inst(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_cleanup_per_inst, hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_cleanup_per_inst);

int ipa_wdi_reg_intf_per_inst(
	struct ipa_wdi_reg_intf_in_params *in)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_reg_intf_per_inst,
		in);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_reg_intf_per_inst);

int ipa_wdi_dereg_intf_per_inst(const char *netdev_name, u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_dereg_intf_per_inst,
		netdev_name, hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_dereg_intf_per_inst);

int ipa_wdi_conn_pipes_per_inst(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_conn_pipes_per_inst,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_conn_pipes_per_inst);

int ipa_wdi_disconn_pipes_per_inst(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_disconn_pipes_per_inst, hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_disconn_pipes_per_inst);

int ipa_wdi_enable_pipes_per_inst(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_enable_pipes_per_inst, hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_enable_pipes_per_inst);

int ipa_wdi_disable_pipes_per_inst(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_disable_pipes_per_inst, hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_disable_pipes_per_inst);

int ipa_wdi_set_perf_profile_per_inst(u32 hdl,
	struct ipa_wdi_perf_profile *profile)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_set_perf_profile_per_inst,
		hdl, profile);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_set_perf_profile_per_inst);

int ipa_wdi_create_smmu_mapping_per_inst(u32 hdl, u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_wdi_create_smmu_mapping_per_inst,
		hdl, num_buffers, info);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_create_smmu_mapping_per_inst);

int ipa_wdi_release_smmu_mapping_per_inst(u32 hdl, u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_wdi_release_smmu_mapping_per_inst,
		hdl, num_buffers, info);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_release_smmu_mapping_per_inst);

int ipa_wdi_get_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_get_stats,
		stats);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_get_stats);

int ipa_get_wdi_version(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_get_wdi_version);

	return ret;
}
EXPORT_SYMBOL(ipa_get_wdi_version);

int ipa_enable_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_enable_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_enable_wdi_pipe);

int ipa_disable_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_disable_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_disable_wdi_pipe);

int ipa_resume_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_resume_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_resume_wdi_pipe);

int ipa_suspend_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_suspend_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_suspend_wdi_pipe);

int ipa_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_connect_wdi_pipe, in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_connect_wdi_pipe);

int ipa_disconnect_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_disconnect_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_disconnect_wdi_pipe);

int ipa_reg_uc_rdyCB(struct ipa_wdi_uc_ready_params *param)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_reg_uc_rdyCB, param);

	return ret;
}
EXPORT_SYMBOL(ipa_reg_uc_rdyCB);

int ipa_dereg_uc_rdyCB(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_dereg_uc_rdyCB);

	return ret;
}
EXPORT_SYMBOL(ipa_dereg_uc_rdyCB);

int ipa_wdi_bw_monitor(struct ipa_wdi_bw_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_bw_monitor,
		info);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_bw_monitor);

int ipa_wdi_sw_stats(struct ipa_wdi_tx_info *info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wdi_sw_stats,
		info);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi_sw_stats);

int ipa_qdss_conn_pipes(struct ipa_qdss_conn_in_params *in,
	struct ipa_qdss_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_qdss_conn_pipes,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_qdss_conn_pipes);

int ipa_qdss_disconn_pipes(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_qdss_disconn_pipes);

	return ret;
}
EXPORT_SYMBOL(ipa_qdss_disconn_pipes);

/* registration API for IPA qdss module */
int ipa_fmwk_register_ipa_qdss(const struct ipa_qdss_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_qdss_conn_pipes
		|| ipa_fmwk_ctx->ipa_qdss_disconn_pipes) {
		pr_err("ipa_qdss APIs were already initialized\n");
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_qdss_conn_pipes = in->ipa_qdss_conn_pipes;
	ipa_fmwk_ctx->ipa_qdss_disconn_pipes = in->ipa_qdss_disconn_pipes;

	pr_info("ipa_qdss registered successfully\n");

	return 0;

}
EXPORT_SYMBOL(ipa_fmwk_register_ipa_qdss);

/* registration API for IPA gsb module */
int ipa_fmwk_register_gsb(const struct ipa_gsb_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_bridge_init
		|| ipa_fmwk_ctx->ipa_bridge_connect
		|| ipa_fmwk_ctx->ipa_bridge_set_perf_profile
		|| ipa_fmwk_ctx->ipa_bridge_disconnect
		|| ipa_fmwk_ctx->ipa_bridge_suspend
		|| ipa_fmwk_ctx->ipa_bridge_resume
		|| ipa_fmwk_ctx->ipa_bridge_tx_dp
		|| ipa_fmwk_ctx->ipa_bridge_cleanup) {
		pr_err("ipa_gsb APIs were already initialized\n");
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_bridge_init = in->ipa_bridge_init;
	ipa_fmwk_ctx->ipa_bridge_connect = in->ipa_bridge_connect;
	ipa_fmwk_ctx->ipa_bridge_set_perf_profile =
		in->ipa_bridge_set_perf_profile;
	ipa_fmwk_ctx->ipa_bridge_disconnect = in->ipa_bridge_disconnect;
	ipa_fmwk_ctx->ipa_bridge_suspend = in->ipa_bridge_suspend;
	ipa_fmwk_ctx->ipa_bridge_resume = in->ipa_bridge_resume;
	ipa_fmwk_ctx->ipa_bridge_tx_dp = in->ipa_bridge_tx_dp;
	ipa_fmwk_ctx->ipa_bridge_cleanup = in->ipa_bridge_cleanup;

	pr_info("ipa_gsb registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_gsb);

/* ipa_gsb APIs */
int ipa_bridge_init(struct ipa_bridge_init_params *params, u32 *hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_bridge_init,
		params, hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_init);

int ipa_bridge_connect(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_bridge_connect,
		hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_connect);

int ipa_bridge_set_perf_profile(u32 hdl, u32 bandwidth)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_bridge_set_perf_profile,
		hdl, bandwidth);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_set_perf_profile);

int ipa_bridge_disconnect(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_bridge_disconnect,
		hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_disconnect);

int ipa_bridge_suspend(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_bridge_suspend,
		hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_suspend);

int ipa_bridge_resume(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_bridge_resume,
		hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_resume);

int ipa_bridge_tx_dp(u32 hdl, struct sk_buff *skb,
	struct ipa_tx_meta *metadata)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_bridge_tx_dp,
		hdl, skb, metadata);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_tx_dp);

int ipa_bridge_cleanup(u32 hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_bridge_cleanup,
		hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_bridge_cleanup);

/* registration API for IPA uc_offload module */
int ipa_fmwk_register_uc_offload(const struct ipa_uc_offload_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_uc_offload_reg_intf
		|| ipa_fmwk_ctx->ipa_uc_offload_cleanup
		|| ipa_fmwk_ctx->ipa_uc_offload_conn_pipes
		|| ipa_fmwk_ctx->ipa_uc_offload_disconn_pipes
		|| ipa_fmwk_ctx->ipa_set_perf_profile
		|| ipa_fmwk_ctx->ipa_uc_offload_reg_rdyCB
		|| ipa_fmwk_ctx->ipa_uc_offload_dereg_rdyCB) {
		pr_err("ipa_uc_offload APIs were already initialized\n");
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_uc_offload_reg_intf = in->ipa_uc_offload_reg_intf;
	ipa_fmwk_ctx->ipa_uc_offload_cleanup = in->ipa_uc_offload_cleanup;
	ipa_fmwk_ctx->ipa_uc_offload_conn_pipes = in->ipa_uc_offload_conn_pipes;
	ipa_fmwk_ctx->ipa_uc_offload_disconn_pipes =
		in->ipa_uc_offload_disconn_pipes;
	ipa_fmwk_ctx->ipa_set_perf_profile = in->ipa_set_perf_profile;
	ipa_fmwk_ctx->ipa_uc_offload_reg_rdyCB = in->ipa_uc_offload_reg_rdyCB;
	ipa_fmwk_ctx->ipa_uc_offload_dereg_rdyCB =
		in->ipa_uc_offload_dereg_rdyCB;

	pr_info("ipa_uc_offload registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_uc_offload);

/* uc_offload APIs */
int ipa_uc_offload_reg_intf(
	struct ipa_uc_offload_intf_params *in,
	struct ipa_uc_offload_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_uc_offload_reg_intf,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_reg_intf);

int ipa_uc_offload_cleanup(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_uc_offload_cleanup,
		clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_cleanup);

int ipa_uc_offload_conn_pipes(struct ipa_uc_offload_conn_in_params *in,
	struct ipa_uc_offload_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_uc_offload_conn_pipes,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_conn_pipes);

int ipa_uc_offload_disconn_pipes(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_uc_offload_disconn_pipes,
		clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_disconn_pipes);

int ipa_set_perf_profile(struct ipa_perf_profile *profile)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_set_perf_profile,
		profile);

	return ret;
}
EXPORT_SYMBOL(ipa_set_perf_profile);

int ipa_uc_offload_reg_rdyCB(struct ipa_uc_ready_params *param)
{
	int ret;

	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	mutex_lock(&ipa_fmwk_ctx->lock);
	if (ipa_fmwk_ctx->ipa_ready) {
		/* call real func, unlock and return */
		ret = ipa_fmwk_ctx->ipa_uc_offload_reg_rdyCB(param);
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return ret;
	}

	ipa_fmwk_ctx->uc_ready_cb = param->notify;
	ipa_fmwk_ctx->uc_ready_priv = param->priv;
	ipa_fmwk_ctx->proto = param->proto;
	param->is_uC_ready = false;

	mutex_unlock(&ipa_fmwk_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_uc_offload_reg_rdyCB);

void ipa_uc_offload_dereg_rdyCB(enum ipa_uc_offload_proto proto)
{
	IPA_FMWK_DISPATCH(ipa_uc_offload_dereg_rdyCB,
		proto);
}
EXPORT_SYMBOL(ipa_uc_offload_dereg_rdyCB);

/* registration API for IPA mhi module */
int ipa_fmwk_register_ipa_mhi(const struct ipa_mhi_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_mhi_init
		|| ipa_fmwk_ctx->ipa_mhi_start
		|| ipa_fmwk_ctx->ipa_mhi_connect_pipe
		|| ipa_fmwk_ctx->ipa_mhi_disconnect_pipe
		|| ipa_fmwk_ctx->ipa_mhi_suspend
		|| ipa_fmwk_ctx->ipa_mhi_resume
		|| ipa_fmwk_ctx->ipa_mhi_destroy
		|| ipa_fmwk_ctx->ipa_mhi_handle_ipa_config_req
		|| ipa_fmwk_ctx->ipa_mhi_update_mstate) {
		pr_err("ipa_mhi APIs were already initialized\n");
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_mhi_init = in->ipa_mhi_init;
	ipa_fmwk_ctx->ipa_mhi_start = in->ipa_mhi_start;
	ipa_fmwk_ctx->ipa_mhi_connect_pipe = in->ipa_mhi_connect_pipe;
	ipa_fmwk_ctx->ipa_mhi_disconnect_pipe = in->ipa_mhi_disconnect_pipe;
	ipa_fmwk_ctx->ipa_mhi_suspend = in->ipa_mhi_suspend;
	ipa_fmwk_ctx->ipa_mhi_resume = in->ipa_mhi_resume;
	ipa_fmwk_ctx->ipa_mhi_destroy = in->ipa_mhi_destroy;
	ipa_fmwk_ctx->ipa_mhi_handle_ipa_config_req =
		in->ipa_mhi_handle_ipa_config_req;
	ipa_fmwk_ctx->ipa_mhi_update_mstate = in->ipa_mhi_update_mstate;

	pr_info("ipa_mhi registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa_mhi);

/* ipa_mhi APIs */
int ipa_mhi_init(struct ipa_mhi_init_params *params)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_init,
		params);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_init);

int ipa_mhi_start(struct ipa_mhi_start_params *params)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_start,
		params);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_start);

int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in, u32 *clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_connect_pipe,
		in, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_connect_pipe);

int ipa_mhi_disconnect_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_disconnect_pipe,
		clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_disconnect_pipe);

int ipa_mhi_suspend(bool force)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_suspend,
		force);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_suspend);

int ipa_mhi_resume(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_resume);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_resume);

void ipa_mhi_destroy(void)
{
	IPA_FMWK_DISPATCH(ipa_mhi_destroy);
}
EXPORT_SYMBOL(ipa_mhi_destroy);

int ipa_mhi_handle_ipa_config_req(struct ipa_config_req_msg_v01 *config_req)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_handle_ipa_config_req,
		config_req);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_handle_ipa_config_req);

int ipa_mhi_update_mstate(enum ipa_mhi_mstate mstate_info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_mhi_update_mstate, mstate_info);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_update_mstate);

/* registration API for IPA wigig module */
int ipa_fmwk_register_ipa_wigig(const struct ipa_wigig_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_wigig_init
		|| ipa_fmwk_ctx->ipa_wigig_cleanup
		|| ipa_fmwk_ctx->ipa_wigig_is_smmu_enabled
		|| ipa_fmwk_ctx->ipa_wigig_reg_intf
		|| ipa_fmwk_ctx->ipa_wigig_dereg_intf
		|| ipa_fmwk_ctx->ipa_wigig_conn_rx_pipe
		|| ipa_fmwk_ctx->ipa_wigig_conn_rx_pipe_smmu
		|| ipa_fmwk_ctx->ipa_wigig_conn_client
		|| ipa_fmwk_ctx->ipa_wigig_conn_client_smmu
		|| ipa_fmwk_ctx->ipa_wigig_disconn_pipe
		|| ipa_fmwk_ctx->ipa_wigig_enable_pipe
		|| ipa_fmwk_ctx->ipa_wigig_disable_pipe
		|| ipa_fmwk_ctx->ipa_wigig_tx_dp
		|| ipa_fmwk_ctx->ipa_wigig_set_perf_profile
		|| ipa_fmwk_ctx->ipa_wigig_save_regs) {
		pr_err("ipa_wigig APIs were already initialized\n");
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_wigig_init = in->ipa_wigig_init;
	ipa_fmwk_ctx->ipa_wigig_cleanup = in->ipa_wigig_cleanup;
	ipa_fmwk_ctx->ipa_wigig_is_smmu_enabled = in->ipa_wigig_is_smmu_enabled;
	ipa_fmwk_ctx->ipa_wigig_reg_intf = in->ipa_wigig_reg_intf;
	ipa_fmwk_ctx->ipa_wigig_dereg_intf = in->ipa_wigig_dereg_intf;
	ipa_fmwk_ctx->ipa_wigig_conn_rx_pipe = in->ipa_wigig_conn_rx_pipe;
	ipa_fmwk_ctx->ipa_wigig_conn_rx_pipe_smmu =
		in->ipa_wigig_conn_rx_pipe_smmu;
	ipa_fmwk_ctx->ipa_wigig_conn_client = in->ipa_wigig_conn_client;
	ipa_fmwk_ctx->ipa_wigig_conn_client_smmu =
		in->ipa_wigig_conn_client_smmu;
	ipa_fmwk_ctx->ipa_wigig_disconn_pipe = in->ipa_wigig_disconn_pipe;
	ipa_fmwk_ctx->ipa_wigig_enable_pipe = in->ipa_wigig_enable_pipe;
	ipa_fmwk_ctx->ipa_wigig_disable_pipe = in->ipa_wigig_disable_pipe;
	ipa_fmwk_ctx->ipa_wigig_tx_dp = in->ipa_wigig_tx_dp;
	ipa_fmwk_ctx->ipa_wigig_set_perf_profile =
		in->ipa_wigig_set_perf_profile;
	ipa_fmwk_ctx->ipa_wigig_save_regs =
		in->ipa_wigig_save_regs;

	pr_info("ipa_wigig registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa_wigig);

/* ipa_wigig APIs */
int ipa_wigig_init(struct ipa_wigig_init_in_params *in,
	struct ipa_wigig_init_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_init,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_init);

int ipa_wigig_cleanup(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_cleanup);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_cleanup);

bool ipa_wigig_is_smmu_enabled(void)
{
	int ret;

	IPA_FMWK_RETURN_BOOL(ipa_wigig_is_smmu_enabled);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_is_smmu_enabled);

int ipa_wigig_reg_intf(struct ipa_wigig_reg_intf_in_params *in)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_reg_intf,
		in);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_reg_intf);

int ipa_wigig_dereg_intf(const char *netdev_name)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_dereg_intf,
		netdev_name);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_dereg_intf);

int ipa_wigig_conn_rx_pipe(
	struct ipa_wigig_conn_rx_in_params *in,
	struct ipa_wigig_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_conn_rx_pipe,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_conn_rx_pipe);

int ipa_wigig_conn_rx_pipe_smmu(
	struct ipa_wigig_conn_rx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_conn_rx_pipe_smmu,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_conn_rx_pipe_smmu);

int ipa_wigig_conn_client(
	struct ipa_wigig_conn_tx_in_params *in,
	struct ipa_wigig_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_conn_client,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_conn_client);

int ipa_wigig_conn_client_smmu(
	struct ipa_wigig_conn_tx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_conn_client_smmu,
		in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_conn_client_smmu);

int ipa_wigig_disconn_pipe(enum ipa_client_type client)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_disconn_pipe,
		client);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_disconn_pipe);

int ipa_wigig_enable_pipe(enum ipa_client_type client)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_enable_pipe,
		client);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_enable_pipe);

int ipa_wigig_disable_pipe(enum ipa_client_type client)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_disable_pipe,
		client);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_disable_pipe);

int ipa_wigig_tx_dp(enum ipa_client_type dst,
	struct sk_buff *skb)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_wigig_tx_dp,
		dst, skb);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_tx_dp);

int ipa_wigig_set_perf_profile(u32 max_supported_bw_mbps)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_set_perf_profile,
		max_supported_bw_mbps);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_set_perf_profile);

int ipa_wigig_save_regs(void)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_wigig_save_regs);

	return ret;
}
EXPORT_SYMBOL(ipa_wigig_save_regs);

/* registration API for IPA eth module */
int ipa_fmwk_register_ipa_eth(const struct ipa_eth_data *in)
{
	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	if (ipa_fmwk_ctx->ipa_eth_register_ready_cb
		|| ipa_fmwk_ctx->ipa_eth_unregister_ready_cb
		|| ipa_fmwk_ctx->ipa_eth_client_conn_pipes
		|| ipa_fmwk_ctx->ipa_eth_client_disconn_pipes
		|| ipa_fmwk_ctx->ipa_eth_client_reg_intf
		|| ipa_fmwk_ctx->ipa_eth_client_unreg_intf
		|| ipa_fmwk_ctx->ipa_eth_client_set_perf_profile
		|| ipa_fmwk_ctx->ipa_eth_get_ipa_client_type_from_eth_type
		|| ipa_fmwk_ctx->ipa_eth_client_exist) {
		pr_err("ipa_eth APIs were already initialized\n");
		return -EPERM;
	}

	ipa_fmwk_ctx->ipa_eth_register_ready_cb = in->ipa_eth_register_ready_cb;
	ipa_fmwk_ctx->ipa_eth_unregister_ready_cb =
		in->ipa_eth_unregister_ready_cb;
	ipa_fmwk_ctx->ipa_eth_client_conn_pipes = in->ipa_eth_client_conn_pipes;
	ipa_fmwk_ctx->ipa_eth_client_disconn_pipes =
		in->ipa_eth_client_disconn_pipes;
	ipa_fmwk_ctx->ipa_eth_client_reg_intf = in->ipa_eth_client_reg_intf;
	ipa_fmwk_ctx->ipa_eth_client_unreg_intf = in->ipa_eth_client_unreg_intf;
	ipa_fmwk_ctx->ipa_eth_client_set_perf_profile =
		in->ipa_eth_client_set_perf_profile;
	ipa_fmwk_ctx->ipa_eth_get_ipa_client_type_from_eth_type =
		in->ipa_eth_get_ipa_client_type_from_eth_type;
	ipa_fmwk_ctx->ipa_eth_client_exist =
		in->ipa_eth_client_exist;

	pr_info("ipa_eth registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ipa_fmwk_register_ipa_eth);

int ipa_eth_register_ready_cb(struct ipa_eth_ready *ready_info)
{
	int ret;

	if (!ipa_fmwk_ctx) {
		pr_err("ipa framework hasn't been initialized yet\n");
		return -EPERM;
	}

	mutex_lock(&ipa_fmwk_ctx->lock);
	if (ipa_fmwk_ctx->ipa_ready) {
		/* call real func, unlock and return */
		ret = ipa_fmwk_ctx->ipa_eth_register_ready_cb(ready_info);
		mutex_unlock(&ipa_fmwk_ctx->lock);
		return ret;
	}
	ipa_fmwk_ctx->eth_ready_info = ready_info;
	ready_info->is_eth_ready = false;
	mutex_unlock(&ipa_fmwk_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_eth_register_ready_cb);

int ipa_eth_unregister_ready_cb(struct ipa_eth_ready *ready_info)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_unregister_ready_cb,
		ready_info);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_unregister_ready_cb);

int ipa_eth_client_conn_pipes(struct ipa_eth_client *client)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_client_conn_pipes,
		client);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_client_conn_pipes);

int ipa_eth_client_disconn_pipes(struct ipa_eth_client *client)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_client_disconn_pipes,
		client);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_client_disconn_pipes);

int ipa_eth_client_reg_intf(struct ipa_eth_intf_info *intf)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_client_reg_intf,
		intf);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_client_reg_intf);

int ipa_eth_client_unreg_intf(struct ipa_eth_intf_info *intf)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_client_unreg_intf,
		intf);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_client_unreg_intf);

int ipa_eth_client_set_perf_profile(struct ipa_eth_client *client,
	struct ipa_eth_perf_profile *profile)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_client_set_perf_profile,
		client, profile);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_client_set_perf_profile);

int ipa_get_default_aggr_time_limit(enum ipa_client_type client,
				u32 *default_aggr_time_limit)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN(ipa_get_default_aggr_time_limit,
		client, default_aggr_time_limit);

	return ret;
}
EXPORT_SYMBOL(ipa_get_default_aggr_time_limit);

enum ipa_client_type ipa_eth_get_ipa_client_type_from_eth_type(
	enum ipa_eth_client_type eth_client_type, enum ipa_eth_pipe_direction dir)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_get_ipa_client_type_from_eth_type,
		eth_client_type, dir);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_get_ipa_client_type_from_eth_type);

bool ipa_eth_client_exist(
	enum ipa_eth_client_type eth_client_type, int inst_id)
{
	int ret;

	IPA_FMWK_DISPATCH_RETURN_DP(ipa_eth_client_exist,
		eth_client_type, inst_id);

	return ret;
}
EXPORT_SYMBOL(ipa_eth_client_exist);

/* module functions */
static int __init ipa_fmwk_init(void)
{
	pr_info("IPA framework init\n");

	ipa_fmwk_ctx = kzalloc(sizeof(struct ipa_fmwk_contex), GFP_KERNEL);
	if (ipa_fmwk_ctx == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&ipa_fmwk_ctx->ipa_ready_cb_list);
	mutex_init(&ipa_fmwk_ctx->lock);

	return 0;
}
subsys_initcall(ipa_fmwk_init);

static void __exit ipa_fmwk_exit(void)
{
	struct ipa_ready_cb_info *info;
	struct ipa_ready_cb_info *next;

	pr_debug("IPA framework exit\n");
	list_for_each_entry_safe(info, next,
		&ipa_fmwk_ctx->ipa_ready_cb_list, link) {
		list_del(&info->link);
		kfree(info);
	}
	kfree(ipa_fmwk_ctx);
	ipa_fmwk_ctx = NULL;
}
module_exit(ipa_fmwk_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW framework");
