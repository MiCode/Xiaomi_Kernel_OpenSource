/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "net_adaption.h"
#define GEM4M_NET_ADAPTION_SUPPORT 0
#if GEM4M_NET_ADAPTION_SUPPORT
static struct test_thread_cb g_test_thread[SERV_THREAD_NUM];
#endif

struct service_test *net_ad_wrap_service(void *adapter)
{
	return (struct service_test *)adapter;
}

void net_ad_thread_proceed_tx(
	struct test_wlan_info *winfos, u_char band_idx)
{
}

void net_ad_thread_stop_tx(
	struct test_wlan_info *winfos)
{
}

s_int32 net_ad_init_thread(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	enum service_thread_list thread_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_release_thread(u_char thread_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_backup_cr(
	struct test_wlan_info *winfos, struct test_bk_cr *test_bkcr,
	u_long offset, enum test_bk_cr_type type)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_restore_cr(
	struct test_wlan_info *winfos, struct test_bk_cr *test_bkcr,
	u_long offset)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_cfg_queue(
	struct test_wlan_info *winfos, boolean enable)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_enter_normal(
	struct test_wlan_info *winfos,
	struct test_backup_params *bak)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_exit_normal(
	struct test_wlan_info *winfos,
	struct test_backup_params *bak)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_update_wdev(
	u_int8 band_idx,
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_init_wdev(
	struct test_wlan_info *winfos,
	struct test_configuration *configs, u_char band_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_release_wdev(
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_alloc_wtbl(
	struct test_wlan_info *winfos,
	u_char *da,
	void *virtual_dev,
	void **virtual_wtbl)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_free_wtbl(
	struct test_wlan_info *winfos,
	u_char *da,
	void *virtual_wtbl)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_apply_wtbl(
	struct test_wlan_info *winfos,
	void *virtual_dev,
	void *virtual_wtbl)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_match_wtbl(
	void *virtual_wtbl,
	u_int16 wcid)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_aid(
	void *virtual_wtbl,
	u_int16 aid)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_wmm_idx(
	void *virtual_device,
	u_int8 *wmm_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_band_idx(
	void *virtual_device,
	u_char *band_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_omac_idx(
	struct test_wlan_info *winfos,
	void *virtual_device,
	u_char *omac_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_fill_phy_info(
	void *virtual_wtbl,
	struct test_tx_info *tx_info)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_speidx(
	struct test_wlan_info *winfos,
	u_int16 ant_sel,
	u_int8 *spe_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_fill_spe_antid(
	struct test_wlan_info *winfos,
	void *virtual_wtbl,
	u_int8 spe_idx,
	u_int8 ant_pri)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_fill_pkt(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char *buf, u_int32 txlen, u_int32 hlen)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_alloc_pkt(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_int32 mpdu_length,
	void **pkt_skb)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_enq_pkt(
	struct test_wlan_info *winfos,
	u_short q_idx,
	void *virtual_wtbl,
	void *virtual_device,
	void *pkt)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_free_pkt(
	struct test_wlan_info *winfos,
	void *pkt_skb)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_trigger_tx(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_int8 band_idx,
	void *pkt)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_rx_done_handle(
	struct test_wlan_info *winfos,
	void *rx_blk)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_band_mode(
	struct test_wlan_info *winfos,
	struct test_band_state *band_state)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_txpwr_power_drop(
	struct test_wlan_info *winfos,
	u_char power_drop, u_char band_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_txpwr_percentage(
	struct test_wlan_info *winfos,
	u_char percentage_ctrl, u_char band_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_txpwr_backoff(
	struct test_wlan_info *winfos,
	u_char backoff_ctrl, u_char band_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_init_txpwr(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_handle_mcs32(
	struct test_wlan_info *winfos,
	void *virtual_wtbl, u_int8 bw)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_cfg_wtbl(
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_wmm_param_by_qid(
	u_int8 wmm_idx,
	u_int8 q_idx,
	struct test_wlan_info *winfos,
	struct test_configuration *configs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_clean_sta_q(
	struct test_wlan_info *winfos, u_char wcid)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_auto_resp(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	u_char band_idx, u_char mode)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_low_power(
	struct test_wlan_info *winfos, u_int32 control)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_read_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_write_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_read_bulk_mac_bbp_reg(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	struct test_register *regs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_read_bulk_rf_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_write_bulk_rf_reg(
	struct test_wlan_info *winfos,
	struct test_register *regs)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

void net_ad_read_ca53_reg(struct test_register *regs)
{
}

void net_ad_write_ca53_reg(struct test_register *regs)
{
}

s_int32 net_ad_read_write_eeprom(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms,
	boolean is_read)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_read_write_bulk_eeprom(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms,
	boolean is_read)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_free_efuse_block(
	struct test_wlan_info *winfos,
	struct test_eeprom *eprms)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_mps_tx_operation(
	struct test_wlan_info *winfos,
	struct test_configuration *configs,
	boolean is_start_tx)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_set_tmr(
	struct test_wlan_info *winfos,
	struct test_tmr_info *tmr_info)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_rxv_stat(
	struct test_wlan_info *winfos,
	u_char ctrl_band_idx,
	struct test_rx_stat *rx_stat)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_rxv_cnt(
	struct test_wlan_info *winfos,
	u_char ctrl_band_idx,
	u_int32 *byte_cnt)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

s_int32 net_ad_get_rxv_content(
	struct test_wlan_info *winfos,
	u_char ctrl_band_idx,
	void *content)
{
	return SERV_STATUS_OSAL_NET_FAIL;
}

