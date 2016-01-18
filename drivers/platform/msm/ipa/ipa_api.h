/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_API_H_
#define _IPA_API_H_

struct ipa_api_controller {
	int (*ipa_connect)(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps, u32 *clnt_hdl);

	int (*ipa_disconnect)(u32 clnt_hdl);

	int (*ipa_reset_endpoint)(u32 clnt_hdl);

	int (*ipa_clear_endpoint_delay)(u32 clnt_hdl);

	int (*ipa_cfg_ep)(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg);

	int (*ipa_cfg_ep_nat)(u32 clnt_hdl,
		const struct ipa_ep_cfg_nat *ipa_ep_cfg);

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

	int (*ipa_uc_wdi_get_dbpa)(struct ipa_wdi_db_params *out);

	int (*ipa_uc_reg_rdyCB)(struct ipa_wdi_uc_ready_params *param);

	int (*ipa_uc_dereg_rdyCB)(void);

	int (*ipa_rm_create_resource)(
		struct ipa_rm_create_params *create_params);

	int (*ipa_rm_delete_resource)(enum ipa_rm_resource_name resource_name);

	int (*ipa_rm_register)(enum ipa_rm_resource_name resource_name,
		struct ipa_rm_register_params *reg_params);

	int (*ipa_rm_deregister)(enum ipa_rm_resource_name resource_name,
		struct ipa_rm_register_params *reg_params);

	int (*ipa_rm_set_perf_profile)(enum ipa_rm_resource_name resource_name,
		struct ipa_rm_perf_profile *profile);

	int (*ipa_rm_add_dependency)(enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name);

	int (*ipa_rm_delete_dependency)(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name);

	int (*ipa_rm_request_resource)(enum ipa_rm_resource_name resource_name);

	int (*ipa_rm_release_resource)(enum ipa_rm_resource_name resource_name);

	int (*ipa_rm_notify_completion)(enum ipa_rm_event event,
		enum ipa_rm_resource_name resource_name);

	int (*ipa_rm_inactivity_timer_init)(enum ipa_rm_resource_name
		resource_name, unsigned long msecs);

	int (*ipa_rm_inactivity_timer_destroy)(
		enum ipa_rm_resource_name resource_name);

	int (*ipa_rm_inactivity_timer_request_resource)(
		enum ipa_rm_resource_name resource_name);

	int (*ipa_rm_inactivity_timer_release_resource)(
				enum ipa_rm_resource_name resource_name);

	int (*teth_bridge_init)(struct teth_bridge_init_params *params);

	int (*teth_bridge_disconnect)(enum ipa_client_type client);

	int (*teth_bridge_connect)(
		struct teth_bridge_connect_params *connect_params);

	void (*ipa_set_client)(
		int index, enum ipacm_client_enum client, bool uplink);

	enum ipacm_client_enum (*ipa_get_client)(int pipe_idx);

	bool (*ipa_get_client_uplink)(int pipe_idx);

	int (*odu_bridge_init)(struct odu_bridge_params *params);

	int (*odu_bridge_connect)(void);

	int (*odu_bridge_disconnect)(void);

	int (*odu_bridge_tx_dp)(struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

	int (*odu_bridge_cleanup)(void);

	int (*ipa_dma_init)(void);

	int (*ipa_dma_enable)(void);

	int (*ipa_dma_disable)(void);

	int (*ipa_dma_sync_memcpy)(u64 dest, u64 src, int len);

	int (*ipa_dma_async_memcpy)(u64 dest, u64 src, int len,
		void (*user_cb)(void *user1), void *user_param);

	int (*ipa_dma_uc_memcpy)(phys_addr_t dest, phys_addr_t src, int len);

	void (*ipa_dma_destroy)(void);

	int (*ipa_mhi_init)(struct ipa_mhi_init_params *params);

	int (*ipa_mhi_start)(struct ipa_mhi_start_params *params);

	int (*ipa_mhi_connect_pipe)(struct ipa_mhi_connect_params *in,
		u32 *clnt_hdl);

	int (*ipa_mhi_disconnect_pipe)(u32 clnt_hdl);

	int (*ipa_mhi_suspend)(bool force);

	int (*ipa_mhi_resume)(void);

	void (*ipa_mhi_destroy)(void);

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

	struct iommu_domain *(*ipa_get_smmu_domain)(void);

	int (*ipa_disable_apps_wan_cons_deaggr)(uint32_t agg_size,
						uint32_t agg_count);

	int (*ipa_rm_add_dependency_sync)(
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name);

	struct device *(*ipa_get_dma_dev)(void);

	int (*ipa_release_wdi_mapping)(u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	int (*ipa_create_wdi_mapping)(u32 num_buffers,
		struct ipa_wdi_buffer_info *info);

	struct ipa_gsi_ep_config *(*ipa_get_gsi_ep_info)(int ipa_ep_idx);

	int (*ipa_usb_init_teth_prot)(enum ipa_usb_teth_prot teth_prot,
		struct ipa_usb_teth_params *teth_params,
		int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event, void*),
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
		enum ipa_usb_teth_prot teth_prot);

	int (*ipa_usb_xdci_resume)(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
		enum ipa_usb_teth_prot teth_prot);

	int (*ipa_register_ipa_ready_cb)(void (*ipa_ready_cb)(void *user_data),
		void *user_data);

};

#ifdef CONFIG_IPA
int ipa_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl, struct of_device_id *pdrv_match);
#else
static inline int ipa_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl, struct of_device_id *pdrv_match)
{
	return -ENODEV;
}
#endif /* (CONFIG_IPA) */

#ifdef CONFIG_IPA3
int ipa3_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl, struct of_device_id *pdrv_match);
#else
static inline int ipa3_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl, struct of_device_id *pdrv_match)
{
	return -ENODEV;
}
#endif /* (CONFIG_IPA3) */

#endif /* _IPA_API_H_ */
