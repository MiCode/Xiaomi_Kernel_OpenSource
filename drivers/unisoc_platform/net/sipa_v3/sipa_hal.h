/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SIPA_HAL_H_
#define _SIPA_HAL_H_

#include <linux/sipa.h>

#include "sipa_priv.h"

struct sipa_plat_drv_cfg;

int sipa_hal_init(struct device *dev);
int sipa_hal_set_enabled(struct device *dev, bool enable);
int sipa_hal_open_cmn_fifo(struct device *dev,
			   enum sipa_cmn_fifo_index fifo,
			   struct sipa_comm_fifo_params *attr,
			   struct sipa_ext_fifo_params *ext_attr,
			   bool force_intr, sipa_hal_notify_cb cb,
			   void *priv);
int sipa_hal_close_cmn_fifo(struct device *dev,
			    enum sipa_cmn_fifo_index fifo);
int sipa_hal_config_irq_affinity(int channel, int dst_cpu);
int sipa_hal_resume_cmn_fifo(struct device *dev);
bool sipa_hal_get_cmn_fifo_open_status(struct device *dev,
				       enum sipa_cmn_fifo_index fifo);
int sipa_hal_cmn_fifo_stop_recv(struct device *dev,
				enum sipa_cmn_fifo_index fifo_id,
				bool stop);
int sipa_hal_recv_node_from_tx_fifo(struct device *dev,
				    enum sipa_cmn_fifo_index fifo_id,
				    struct sipa_node_desc_tag *node,
				    int budget);
int sipa_hal_add_tx_fifo_rptr(struct device *dev, enum sipa_cmn_fifo_index id,
			      u32 num);
int sipa_hal_get_cmn_fifo_filled_depth(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id,
				       u32 *rx_filled, u32 *tx_filled);
void sipa_hal_enable_pcie_dl_dma(struct device *dev, bool eb);
int sipa_hal_reclaim_unuse_node(struct device *dev,
				enum sipa_cmn_fifo_index fifo_id);
int sipa_hal_reclaim_unuse_intr(struct device *dev,
				enum sipa_cmn_fifo_index fifo_id);
void sipa_prepare_modem_power_off(void);
void sipa_prepare_modem_power_on(void);
int sipa_hal_put_node_to_rx_fifo(struct device *dev,
				 enum sipa_cmn_fifo_index fifo_id,
				 struct sipa_node_desc_tag *node,
				 int budget);
int sipa_hal_put_node_to_tx_fifo(struct device *dev,
				 enum sipa_cmn_fifo_index fifo_id,
				 struct sipa_node_desc_tag *node,
				 int budget);
int sipa_hal_add_rx_fifo_wptr(struct device *dev,
			      enum sipa_cmn_fifo_index fifo_id,
			      u32 num);
int sipa_hal_add_tx_fifo_rptr(struct device *dev,
			      enum sipa_cmn_fifo_index fifo_id,
			      u32 num);
bool sipa_hal_get_rx_fifo_empty_status(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_get_tx_fifo_empty_status(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_check_rx_priv_fifo_is_empty(struct device *dev,
					  enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_check_rx_priv_fifo_is_full(struct device *dev,
					 enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_is_rx_fifo_full(struct device *dev,
			      enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_bk_fifo_node(struct device *dev,
			   enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_resume_fifo_node(struct device *dev,
			       enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_is_tx_fifo_empty(struct device *dev,
			       enum sipa_cmn_fifo_index fifo_id);
bool sipa_hal_check_send_cmn_fifo_com(struct device *dev,
				      enum sipa_cmn_fifo_index fifo_id);
int sipa_hal_cmn_fifo_get_filled_depth(struct device *dev,
				       enum sipa_cmn_fifo_index fifo_id,
				       u32 *rx, u32 *tx);
int sipa_hal_free_tx_rx_fifo_buf(struct device *dev);
int sipa_hal_init_pam_param(enum sipa_cmn_fifo_index dl_idx,
			    enum sipa_cmn_fifo_index ul_idx,
			    struct sipa_to_pam_info *out);
int sipa_hal_set_hash_sync_req(void);
int sipa_swap_hash_table(struct sipa_hash_table *new_tbl);
int sipa_config_ifilter(struct sipa_filter_tbl *ifilter);
int sipa_config_ofilter(struct sipa_filter_tbl *ofilter);
int sipa_hal_ctrl_ipa_action(u32 enable);
void sipa_hal_resume_glb_reg_cfg(struct device *dev);
bool sipa_hal_get_pause_status(void);
bool sipa_hal_get_resume_status(void);
struct sipa_node_desc_tag *sipa_hal_get_rx_node_wptr(struct device *dev,
						     enum sipa_cmn_fifo_index d,
						     u32 index);
struct sipa_node_desc_tag *sipa_hal_get_tx_node_rptr(struct device *dev,
						     enum sipa_cmn_fifo_index d,
						     u32 index);

int sipa_hal_sync_node_to_rx_fifo(struct device *dev,
				  enum sipa_cmn_fifo_index fifo_id,
				  int budget);
int sipa_hal_sync_node_from_tx_fifo(struct device *dev,
				    enum sipa_cmn_fifo_index fifo_id,
				    int budget);
void sipa_hal_set_map_flow_ctl_to_wifi(bool status);
void sipa_hal_set_wifi_ul_map0_int_sel(bool status);
int sipa_hal_cmn_fifo_non_stop_on_flowctl(enum sipa_cmn_fifo_index id,
					  bool status);
int sipa_hal_cmn_fifo_flowctl_recover(enum sipa_cmn_fifo_index id);
int sipa_hal_check_cmn_fifo_flowctl(enum sipa_cmn_fifo_index id);
int sipa_hal_clr_cmn_fifo_flowctl_interrupt(enum sipa_cmn_fifo_index id);
int sipa_hal_check_cmn_fifo_enter_flowctl(enum sipa_cmn_fifo_index id);
int sipa_hal_check_cmn_fifo_exit_flowctl(enum sipa_cmn_fifo_index id);
int sipa_hal_clr_cfifo_flowctl_enter_inter(enum sipa_cmn_fifo_index id);
int sipa_hal_clr_cfifo_flowctl_exit_inter(enum sipa_cmn_fifo_index id);
#endif /* !_SIPA_HAL_H_ */
