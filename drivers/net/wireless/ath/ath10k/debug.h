/*
 * Copyright (c) 2005-2011 Atheros Communications Inc.
 * Copyright (c) 2011-2013 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <linux/types.h>
#include "trace.h"

enum ath10k_debug_mask {
	ATH10K_DBG_PCI		= 0x00000001,
	ATH10K_DBG_WMI		= 0x00000002,
	ATH10K_DBG_HTC		= 0x00000004,
	ATH10K_DBG_HTT		= 0x00000008,
	ATH10K_DBG_MAC		= 0x00000010,
	ATH10K_DBG_BOOT		= 0x00000020,
	ATH10K_DBG_PCI_DUMP	= 0x00000040,
	ATH10K_DBG_HTT_DUMP	= 0x00000080,
	ATH10K_DBG_MGMT		= 0x00000100,
	ATH10K_DBG_DATA		= 0x00000200,
	ATH10K_DBG_BMI		= 0x00000400,
	ATH10K_DBG_REGULATORY	= 0x00000800,
	ATH10K_DBG_TESTMODE	= 0x00001000,
	ATH10K_DBG_WMI_PRINT	= 0x00002000,
	ATH10K_DBG_PCI_PS	= 0x00004000,
	ATH10K_DBG_AHB		= 0x00008000,
	ATH10K_DBG_SNOC		= 0x00010000,
	ATH10K_DBG_ANY		= 0xffffffff,
};

enum ath10k_pktlog_filter {
	ATH10K_PKTLOG_RX         = 0x000000001,
	ATH10K_PKTLOG_TX         = 0x000000002,
	ATH10K_PKTLOG_RCFIND     = 0x000000004,
	ATH10K_PKTLOG_RCUPDATE   = 0x000000008,
	ATH10K_PKTLOG_DBG_PRINT  = 0x000000010,
	ATH10K_PKTLOG_ANY        = 0x00000001f,
};

enum ath10k_dbg_aggr_mode {
	ATH10K_DBG_AGGR_MODE_AUTO,
	ATH10K_DBG_AGGR_MODE_MANUAL,
	ATH10K_DBG_AGGR_MODE_MAX,
};

#define IEEE80211_FC1_DIR_MASK              0x03
#define IEEE80211_FC1_DIR_NODS              0x00    /* STA->STA */
#define IEEE80211_FC1_DIR_TODS              0x01    /* STA->AP  */
#define IEEE80211_FC1_DIR_FROMDS            0x02    /* AP ->STA */
#define IEEE80211_FC1_DIR_DSTODS            0x03    /* AP ->AP  */
#define IEEE80211_ADDR_LEN  6                       /* size of 802.11 address */

#define MAX_PKT_INFO_MSDU_ID 192
#define MSDU_ID_INFO_ID_OFFSET  \
	((MAX_PKT_INFO_MSDU_ID >> 3) + 4)

#define PKTLOG_MAX_TXCTL_WORDS 57 /* +2 words for bitmap */
#define HTT_TX_MSDU_LEN_MASK 0xffff

struct txctl_frm_hdr {
	__le16 framectrl;       /* frame control field from header */
	__le16 seqctrl;         /* frame control field from header */
	__le16 bssid_tail;      /* last two octets of bssid */
	__le16 sa_tail;         /* last two octets of SA */
	__le16 da_tail;         /* last two octets of DA */
	__le16 resvd;
} __packed;

struct ath_pktlog_hdr {
	__le16 flags;
	__le16 missed_cnt;
	u8 log_type;
	u8 mac_id;
	__le16 size;
	__le32 timestamp;
	__le32 type_specific_data;
} __packed;

/* generic definitions for IEEE 802.11 frames */
struct ieee80211_frame {
	u8 i_fc[2];
	u8 i_dur[2];
	union {
		struct {
			u8 i_addr1[IEEE80211_ADDR_LEN];
			u8 i_addr2[IEEE80211_ADDR_LEN];
			u8 i_addr3[IEEE80211_ADDR_LEN];
		};
		u8 i_addr_all[3 * IEEE80211_ADDR_LEN];
	};
	u8 i_seq[2];
} __packed;

struct fw_pktlog_msdu_info {
	__le32 num_msdu;
	u8 bound_bmap[MAX_PKT_INFO_MSDU_ID >> 3];
	__le16 id[MAX_PKT_INFO_MSDU_ID];
} __packed;

struct ath_pktlog_txctl {
	struct ath_pktlog_hdr hdr;
	struct txctl_frm_hdr frm_hdr;
	__le32 txdesc_ctl[PKTLOG_MAX_TXCTL_WORDS];
} __packed;

struct ath_pktlog_msdu_id {
	struct ath_pktlog_hdr hdr;
	struct fw_pktlog_msdu_info msdu_info;
} __packed;

struct ath_pktlog_rx_info {
	struct ath_pktlog_hdr pl_hdr;
	struct rx_attention attention;
	struct rx_frag_info frag_info;
	struct rx_mpdu_start mpdu_start;
	struct rx_msdu_start msdu_start;
	struct rx_msdu_end msdu_end;
	struct rx_mpdu_end mpdu_end;
	struct rx_ppdu_start ppdu_start;
	struct rx_ppdu_end ppdu_end;
	u8 rx_hdr_status[RX_HTT_HDR_STATUS_LEN];
} __packed;

/* FIXME: How to calculate the buffer size sanely? */
#define ATH10K_FW_STATS_BUF_SIZE (1024 * 1024)
#define ATH10K_DATAPATH_BUF_SIZE (1024 * 1024)

extern unsigned int ath10k_debug_mask;

__printf(2, 3) void ath10k_info(struct ath10k *ar, const char *fmt, ...);
__printf(2, 3) void ath10k_err(struct ath10k *ar, const char *fmt, ...);
__printf(2, 3) void ath10k_warn(struct ath10k *ar, const char *fmt, ...);

void ath10k_debug_print_hwfw_info(struct ath10k *ar);
void ath10k_debug_print_board_info(struct ath10k *ar);
void ath10k_debug_print_boot_info(struct ath10k *ar);
void ath10k_print_driver_info(struct ath10k *ar);

#ifdef CONFIG_ATH10K_DEBUGFS
int ath10k_debug_start(struct ath10k *ar);
void ath10k_debug_stop(struct ath10k *ar);
int ath10k_debug_create(struct ath10k *ar);
void ath10k_debug_destroy(struct ath10k *ar);
int ath10k_debug_register(struct ath10k *ar);
void ath10k_debug_unregister(struct ath10k *ar);
void ath10k_debug_fw_stats_process(struct ath10k *ar, struct sk_buff *skb);
void ath10k_debug_tpc_stats_process(struct ath10k *ar,
				    struct ath10k_tpc_stats *tpc_stats);
struct ath10k_fw_crash_data *
ath10k_debug_get_new_fw_crash_data(struct ath10k *ar);

void ath10k_debug_dbglog_add(struct ath10k *ar, u8 *buffer, int len);
int ath10k_rx_record_pktlog(struct ath10k *ar, struct sk_buff *skb);
#define ATH10K_DFS_STAT_INC(ar, c) (ar->debug.dfs_stats.c++)

void ath10k_debug_get_et_strings(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 u32 sset, u8 *data);
int ath10k_debug_get_et_sset_count(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif, int sset);
void ath10k_debug_get_et_stats(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ethtool_stats *stats, u64 *data);
void fill_datapath_stats(struct ath10k *ar, struct ieee80211_rx_status *status);
size_t get_datapath_stat(char *buf, struct ath10k *ar);
int ath10k_pktlog_connect(struct ath10k *ar);
#else
static inline int ath10k_debug_start(struct ath10k *ar)
{
	return 0;
}

static inline void ath10k_debug_stop(struct ath10k *ar)
{
}

static inline int ath10k_debug_create(struct ath10k *ar)
{
	return 0;
}

static inline void ath10k_debug_destroy(struct ath10k *ar)
{
}

static inline int ath10k_debug_register(struct ath10k *ar)
{
	return 0;
}

static inline void ath10k_debug_unregister(struct ath10k *ar)
{
}

static inline void ath10k_debug_fw_stats_process(struct ath10k *ar,
						 struct sk_buff *skb)
{
}

static inline void ath10k_debug_tpc_stats_process(struct ath10k *ar,
						  struct ath10k_tpc_stats *tpc_stats)
{
	kfree(tpc_stats);
}

static inline void ath10k_debug_dbglog_add(struct ath10k *ar, u8 *buffer,
					   int len)
{
}

static inline int ath10k_rx_record_pktlog(struct ath10k *ar,
					  struct sk_buff *skb)
{
	return 0;
}

static inline struct ath10k_fw_crash_data *
ath10k_debug_get_new_fw_crash_data(struct ath10k *ar)
{
	return NULL;
}

static inline void fill_datapath_stats(struct ath10k *ar,
				       struct ieee80211_rx_status *status)
{
}

static inline size_t get_datapath_stat(char *buf, struct ath10k *ar)
{
	return 0;
}

static inline int ath10k_pktlog_connect(struct ath10k *ar)
{
	return 0;
}
#define ATH10K_DFS_STAT_INC(ar, c) do { } while (0)

#define ath10k_debug_get_et_strings NULL
#define ath10k_debug_get_et_sset_count NULL
#define ath10k_debug_get_et_stats NULL

#endif /* CONFIG_ATH10K_DEBUGFS */
#ifdef CONFIG_MAC80211_DEBUGFS
void ath10k_sta_add_debugfs(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta, struct dentry *dir);
void ath10k_sta_update_rx_duration(struct ath10k *ar,
				   struct ath10k_fw_stats *stats);
void ath10k_sta_statistics(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			   struct ieee80211_sta *sta,
			   struct station_info *sinfo);
#else
static inline
void ath10k_sta_update_rx_duration(struct ath10k *ar,
				   struct ath10k_fw_stats *stats)
{
}
#endif /* CONFIG_MAC80211_DEBUGFS */

#ifdef CONFIG_ATH10K_DEBUG
__printf(3, 4) void ath10k_dbg(struct ath10k *ar,
			       enum ath10k_debug_mask mask,
			       const char *fmt, ...);
void ath10k_dbg_dump(struct ath10k *ar,
		     enum ath10k_debug_mask mask,
		     const char *msg, const char *prefix,
		     const void *buf, size_t len);
#else /* CONFIG_ATH10K_DEBUG */

static inline int ath10k_dbg(struct ath10k *ar,
			     enum ath10k_debug_mask dbg_mask,
			     const char *fmt, ...)
{
	return 0;
}

static inline void ath10k_dbg_dump(struct ath10k *ar,
				   enum ath10k_debug_mask mask,
				   const char *msg, const char *prefix,
				   const void *buf, size_t len)
{
}
#endif /* CONFIG_ATH10K_DEBUG */
#endif /* _DEBUG_H_ */
