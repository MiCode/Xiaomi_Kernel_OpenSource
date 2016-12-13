/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include <linux/ipa_mhi.h>
#include <linux/ipa_uc_offload.h>
#include <linux/ipa_wdi3.h>
#include "ipa_common_i.h"

#ifndef _IPA_API_H_
#define _IPA_API_H_

struct ipa_api_controller {
	int (*ipa_connect)(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps, u32 *clnt_hdl);

	int (*ipa_disconnect)(u32 clnt_hdl);

	int (*ipa_reset_endpoint)(u32 clnt_hdl);

	int (*ipa_clear_endpoint_delay)(u32 clnt_hdl);

	int (*ipa_disable_endpoint)(u32 clnt_hdl);

	int (*ipa_cfg_ep)(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg);

	int (*ipa_cfg_ep_nat)(u32 clnt_hdl,
		const struct ipa_ep_cfg_nat *ipa_ep_cfg);

	int (*ipa_cfg_ep_conn_track)(u32 clnt_hdl,
		const struct ipa_ep_cfg_conn_track *ipa_ep_cfg);

	int (*ipa_cfg_ep_hdr)(u32 clnt_hdl,
		const struct ipa_ep_cfg_hdr *ipa_ep_cfg);

	int (*ipa_cfg_ep_hdr_ext)(u32 clnt_hdl,
			const struct ipa_ep_cfg_hdr_ext *ipa_ep_cfg);

	int (*ipa_cfg_ep_mode)(u32 clnt_hdl,
		const struct ipa_ep_cfg_mode *ipa_ep_cfg);

	int (*ipa_cfg_ep_aggr)(u32 clnt_hdl,
		const struct ipa_ep_cfg_aggr *ipa_ep_cfg);

	int (*ipa_cfg_ep_deaggr)(u32 clnt_hdl,
		const struct ipa_ep_cfg_deaggr *ipa_ep_cfg);

	int (*ipa_cfg_ep_route)(u32 clnt_hdl,
		const struct ipa_ep_cfg_route *ipa_ep_cfg);

	int (*ipa_cfg_ep_holb)(u32 clnt_hdl,
		const struct ipa_ep_cfg_holb *ipa_ep_cfg);

	int (*ipa_cfg_ep_cfg)(u32 clnt_hdl,
		const struct ipa_ep_cfg_cfg *ipa_ep_cfg);

	int (*ipa_cfg_ep_metadata_mask)(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask *ipa_ep_cfg);

	int (*ipa_cfg_ep_holb_by_client)(enum ipa_client_type client,
		const struct ipa_ep_cfg_holb *ipa_ep_cfg);

	int (*ipa_cfg_ep_ctrl)(u32 clnt_hdl,
		const struct ipa_ep_cfg_ctrl *ep_ctrl);

	int (*ipa_add_hdr)(struct ipa_ioc_add_hdr *hdrs);

	int (*ipa_del_hdr)(struct ipa_ioc_del_hdr *hdls);

	int (*ipa_commit_hdr)(void);

	int (*ipa_reset_hdr)(void);

	int (*ipa_get_hdr)(struct ipa_ioc_get_hdr *lookup);

	int (*ipa_put_hdr)(u32 hdr_hdl);

	int (*ipa_copy_hdr)(struct ipa_ioc_copy_hdr *copy);

	int (*ipa_add_hdr_proc_ctx)(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs);

	int (*ipa_del_hdr_proc_ctx)(struct ipa_ioc_del_hdr_proc_ctx *hdls);

	int (*ipa_add_rt_rule)(struct ipa_ioc_add_rt_rule *rules);

	int (*ipa_del_rt_rule)(struct ipa_ioc_del_rt_rule *hdls);

	int (*ipa_commit_rt)(enum ipa_ip_type ip);

	int (*ipa_reset_rt)(enum ipa_ip_type ip);

	int (*ipa_get_rt_tbl)(struct ipa_ioc_get_rt_tbl *lookup);

	int (*ipa_put_rt_tbl)(u32 rt_tbl_hdl);

	int (*ipa_query_rt_index)(struct ipa_ioc_get_rt_tbl_indx *in);

	int (*ipa_mdfy_rt_rule)(struct ipa_ioc_mdfy_rt_rule *rules);

	int (*ipa_add_flt_rule)(struct ipa_ioc_add_flt_rule *rules);

	int (*ipa_del_flt_rule)(struct ipa_ioc_del_flt_rule *hdls);

	int (*ipa_mdfy_flt_rule)(struct ipa_ioc_mdfy_flt_rule *rules);

	int (*ipa_commit_flt)(enum ipa_ip_type ip);

	int (*ipa_reset_flt)(enum ipa_ip_type ip);

	int (*allocate_nat_device)(struct ipa_ioc_nat_alloc_mem *mem);

	int (*ipa_nat_init_cmd)(struct ipa_ioc_v4_nat_init *init);

	int (*ipa_nat_dma_cmd)(struct ipa_ioc_nat_dma_cmd *dma);

	int (*ipa_nat_del_cmd)(struct ipa_ioc_v4_nat_del *del);

	int (*ipa_send_msg)(struct ipa_msg_meta *meta, void *buff,
		ipa_msg_free_fn callback);

	int (*ipa_register_pull_msg)(struct ipa_msg_meta *meta,
		ipa_msg_pull_fn callback);

	int (*ipa_deregister_pull_msg)(struct ipa_msg_meta *meta);

	int (*ipa_register_intf)(const char *name,
		const struct ipa_tx_intf *tx,
		const struct ipa_rx_intf *rx);

	int (*ipa_register_intf_ext)(const char *name,
		const struct ipa_tx_intf *tx,
		const struct ipa_rx_intf *rx,
		const struct ipa_ext_intf *ext);

	int (*ipa_deregister_intf)(const char *name);

	int (*ipa_set_aggr_mode)(enum ipa_aggr_mode mode);

	int (*ipa_set_qcncm_ndp_sig)(char sig[3]);

	int (*ipa_set_single_ndp_per_mbim)(bool enable);

	int (*ipa_tx_dp)(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

	int (*ipa_tx_dp_mul)(enum ipa_client_type dst,
			struct ipa_tx_data_desc *data_desc);

	void (*ipa_free_skb)(struct ipa_rx_data *);

	int (*ipa_setup_sys_pipe)(struct ipa_sys_connect_params *sys_in,
		u32 *clnt_hdl);

	int (*ipa_teardown_sys_pipe)(u32 clnt_hdl);

	int (*ipa_sys_setup)(struct ipa_sys_connect_params *sys_in,
		unsigned long *ipa_bam_hdl,
		u32 *ipa_pipe_num, u32 *clnt_hdl, bool en_status);

	int (*ipa_sys_teardown)(u32 clnt_hdl);

	int (*ipa_sys_update_gsi_hdls)(u32 clnt_hdl, unsigned long gsi_ch_hdl,
		unsigned long gsi_ev_hdl);

	int (*ipa_connect_wdi_pipe)(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out);

	int (*ipa_disconnect_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_enable_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_disable_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_resume_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_suspend_wdi_pipe)(u32 clnt_hdl);

	int (*ipa_get_wdi_stats)(struct IpaHwStatsWDIInfoData_t *stats);

	u16 (*ipa_get_smem_restr_bytes)(void);

	int (*ipa_broadcast_wdi_quota_reach_ind)(uint32_t fid,
		uint64_t num_bytes);

	int (*ipa_uc_wdi_get_dbpa)(struct ipa_wdi_db_params *out);

	int (*ipa_uc_reg_rdyCB)(struct ipa_wdi_uc_ready_params *param);

	int (*ipa_uc_dereg_rdyCB)(void);

	int (*teth_bridge_init)(struct teth_bridge_init_params *params);

	int (*teth_bridge_disconnect)(enum ipa_client_type client);

	int (*teth_bridge_connect)(
		struct teth_bridge_connect_params *connect_params);

	void (*ipa_set_client)(
		int index, enum ipacm_client_enum client, bool uplink);

	enum ipacm_client_enum (*ipa_get_client)(int pipe_idx);

	bool (*ipa_get_client_uplink)(int pipe_idx);

	int (*ipa_dma_init)(void);

	int (*ipa_dma_enable)(void);

	int (*ipa_dma_disable)(void);

	int (*ipa_dma_sync_memcpy)(u64 dest, u64 src, int len);

	int (*ipa_dma_async_memcpy)(u64 dest, u64 src, int len,
		void (*user_cb)(void *user1), void *user_param);

	int (*ipa_dma_uc_memcpy)(phys_addr_t dest, phys_addr_t src, int len);

	void (*ipa_dma_destroy)(void);

	bool (*ipa_has_open_aggr_frame)(enum ipa_client_type client);

	int (*ipa_generate_tag_process)(void);

	int (*ipa_disable_sps_pipe)(enum ipa_client_type client);

	void (*ipa_set_tag_process_before_gating)(bool val);

	int (*ipa_mhi_init_engine)(struct ipa_mhi_init_engine *params);

	int (*ipa_connect_mhi_pipe)(struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl);

	int (*ipa_disconnect_mhi_pipe)(u32 clnt_hdl);

	bool (*ipa_mhi_stop_gsi_channel)(enum ipa_client_type client);

	int (*ipa_qmi_disable_force_clear)(u32 request_id);

	int (*ipa_qmi_enable_force_clear_datapath_send)(
		struct ipa_enable_force_clear_datapath_req_msg_v01 *req);

	int (*ipa_qmi_disable_force_clear_datapath_send)(
		struct ipa_disable_force_clear_datapath_req_msg_v01 *req);

	bool (*ipa_mhi_sps_channel_empty)(enum ipa_client_type client);

	int (*ipa_mhi_reset_channel_internal)(enum ipa_client_type client);

	int (*ipa_mhi_start_channel_internal)(enum ipa_client_type client);

	void (*ipa_get_holb)(int ep_idx, struct ipa_ep_cfg_holb *holb);

	int (*ipa_mhi_query_ch_info)(enum ipa_client_type client,
			struct gsi_chan_info *ch_info);

	int (*ipa_mhi_resume_channels_internal)(
			enum ipa_client_type client,
			bool LPTransitionRejected,
			bool brstmode_enabled,
			union __packed gsi_channel_scratch ch_scratch,
			u8 index);

	int  (*ipa_mhi_destroy_channel)(enum ipa_client_type client);

	int (*ipa_uc_mhi_send_dl_ul_sync_info)
		(union IpaHwMhiDlUlSyncCmdData_t *cmd);

	int (*ipa_uc_mhi_init)
		(void (*ready_cb)(void), void (*wakeup_request_cb)(void));

	void (*ipa_uc_mhi_cleanup)(void);

	int (*ipa_uc_mhi_print_stats)(char *dbg_buff, int size);

	int (*ipa_uc_mhi_reset_channel)(int channelHandle);

	int (*ipa_uc_mhi_suspend_channel)(int channelHandle);

	int (*ipa_uc_mhi_stop_event_update_channel)(int channelHandle);

	int (*ipa_uc_state_check)(void);

	int (*ipa_write_qmap_id)(struct ipa_ioc_write_qmapid *param_in);

	int (*ipa_add_interrupt_handler)(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data);

	int (*ipa_remove_interrupt_handler)(enum ipa_irq_type interrupt);

	int (*ipa_restore_suspend_handler)(void);

	void (*ipa_bam_reg_dump)(void);

	int (*ipa_get_ep_mapping)(enum ipa_client_type client);

	bool (*ipa_is_ready)(void);

	void (*ipa_proxy_clk_vote)(void);

	void (*ipa_proxy_clk_unvote)(void);

	bool (*ipa_is_client_handle_valid)(u32 clnt_hdl);

	enum ipa_client_type (*ipa_get_client_mapping)(int pipe_idx);

	enum ipa_rm_resource_name (*ipa_get_rm_resource_from_ep)(int pipe_idx);

	bool (*ipa_get_modem_cfg_emb_pipe_flt)(void);

	enum ipa_transport_type (*ipa_get_transport_type)(void);

	int (*ipa_ap_suspend)(struct device *dev);

	int (*ipa_ap_resume)(struct device *dev);

	int (*ipa_stop_gsi_channel)(u32 clnt_hdl);

	int (*ipa_start_gsi_channel)(u32 clnt_hdl);

	struct iommu_domain *(*ipa_get_smmu_domain)(void);

	int (*ipa_disable_apps_wan_cons_deaggr)(uint32_t agg_size,
						uint32_t agg_count);

	struct device *(*ipa_get_dma_dev)(void);

	int (*ipa_release_wdi_mapping)(u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	int (*ipa_create_wdi_mapping)(u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	const struct ipa_gsi_ep_config *(*ipa_get_gsi_ep_info)
		(enum ipa_client_type client);

	int (*ipa_register_ipa_ready_cb)(void (*ipa_ready_cb)(void *user_data),
		void *user_data);

	void (*ipa_inc_client_enable_clks)(
		struct ipa_active_client_logging_info *id);

	void (*ipa_dec_client_disable_clks)(
		struct ipa_active_client_logging_info *id);

	int (*ipa_inc_client_enable_clks_no_block)(
		struct ipa_active_client_logging_info *id);

	int (*ipa_suspend_resource_no_block)(
		enum ipa_rm_resource_name resource);

	int (*ipa_resume_resource)(enum ipa_rm_resource_name name);

	int (*ipa_suspend_resource_sync)(enum ipa_rm_resource_name resource);

	int (*ipa_set_required_perf_profile)(
		enum ipa_voltage_level floor_voltage, u32 bandwidth_mbps);

	void *(*ipa_get_ipc_logbuf)(void);

	void *(*ipa_get_ipc_logbuf_low)(void);

	int (*ipa_rx_poll)(u32 clnt_hdl, int budget);

	void (*ipa_recycle_wan_skb)(struct sk_buff *skb);

	int (*ipa_setup_uc_ntn_pipes)(struct ipa_ntn_conn_in_params *in,
		ipa_notify_cb notify, void *priv, u8 hdr_len,
		struct ipa_ntn_conn_out_params *);

	int (*ipa_tear_down_uc_offload_pipes)(int ipa_ep_idx_ul,
		int ipa_ep_idx_dl);

	struct device *(*ipa_get_pdev)(void);

	int (*ipa_ntn_uc_reg_rdyCB)(void (*ipauc_ready_cb)(void *user_data),
		void *user_data);

	void (*ipa_ntn_uc_dereg_rdyCB)(void);

	int (*ipa_conn_wdi3_pipes)(struct ipa_wdi3_conn_in_params *in,
		struct ipa_wdi3_conn_out_params *out);

	int (*ipa_disconn_wdi3_pipes)(int ipa_ep_idx_tx,
		int ipa_ep_idx_rx);

	int (*ipa_enable_wdi3_pipes)(int ipa_ep_idx_tx,
		int ipa_ep_idx_rx);

	int (*ipa_disable_wdi3_pipes)(int ipa_ep_idx_tx,
		int ipa_ep_idx_rx);
};

#ifdef CONFIG_IPA
int ipa_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl,
	const struct of_device_id *pdrv_match);
#else
static inline int ipa_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl,
	const struct of_device_id *pdrv_match)
{
	return -ENODEV;
}
#endif /* (CONFIG_IPA) */

#ifdef CONFIG_IPA3
int ipa3_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl,
	const struct of_device_id *pdrv_match);
#else
static inline int ipa3_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl,
	const struct of_device_id *pdrv_match)
{
	return -ENODEV;
}
#endif /* (CONFIG_IPA3) */

#endif /* _IPA_API_H_ */
