/* SPDX-License-Identifier: ISC */
/* Copyright (c) 2019, The Linux Foundation.
 * All rights reserved.
 */

#ifndef WIL6210_IPA_H
#define WIL6210_IPA_H

#include <linux/types.h>
#ifdef CONFIG_WIL6210_IPA
#include <linux/msi.h>
#include <linux/msm_ipa.h>
extern u8 ipa_offload;
#endif

#define WIL_IPA_BCAST_DESC_RING_SIZE 512
#define WIL_IPA_BCAST_SRING_SIZE (2 * WIL_IPA_BCAST_DESC_RING_SIZE)

#define WIL_IPA_DESC_RING_SIZE 2000
#define WIL_IPA_STATUS_RING_SIZE 2048

#define WIL_IPA_MAX_ASSOC_STA 4

#ifdef CONFIG_WIL6210_IPA

struct wil_dma_map_info {
	void *va;
	phys_addr_t pa;
};

struct wil_ipa_conn {
	enum ipa_client_type ipa_client;
	struct wil_dma_map_info *tx_bufs_addr;
	int tx_bufs_count;
};

struct wil_ipa_rx_buf {
	dma_addr_t pa;
	void *va;
	size_t sz;
};

struct wil_ipa {
	struct wil6210_priv *wil;
	struct iommu_domain *domain;
	struct timer_list bcast_timer;
	int bcast_sring_id;
	struct completion ipa_uc_ready_comp;
	u8 smmu_enabled;
	enum ipa_client_type rx_client_type;
	phys_addr_t uc_db_pa;
	struct wil_ipa_conn conn[WIL6210_MAX_CID];
	struct wil_ipa_rx_buf rx_buf; /* contiguous memory split into rx bufs */
	struct msi_msg orig_msi_msg;
	atomic_t outstanding_pkts;
};

static inline bool wil_ipa_offload(void) {return ipa_offload; }
void *wil_ipa_init(struct wil6210_priv *wil);
void wil_ipa_uninit(void *ipa_handle);
int wil_ipa_start_ap(void *ipa_handle);
int wil_ipa_conn_client(void *ipa_handle, int cid, int ring_id, int sring_id);
void wil_ipa_disconn_client(void *ipa_handle, int cid);
int wil_ipa_get_bcast_sring_id(struct wil6210_priv *wil);
void wil_ipa_set_bcast_sring_id(struct wil6210_priv *wil, int bcast_sring_id);
int wil_ipa_tx(void *ipa_handle, struct wil_ring *ring, struct sk_buff *skb);

#else /* CONFIG_WIL6210_IPA */

static inline bool wil_ipa_offload(void) {return false; }
static inline void *wil_ipa_init(struct wil6210_priv *wil) {return NULL; }
static inline void wil_ipa_uninit(void *ipa_handle) {}
static inline int wil_ipa_start_ap(void *ipa_handle) {return -EOPNOTSUPP; }
static inline int wil_ipa_conn_client(void *ipa_handle,
				      int cid, int ring_id,
				      int sring_id) {return -EOPNOTSUPP; }
static inline void wil_ipa_disconn_client(void *ipa_handle, int cid) {}
static inline int wil_ipa_get_bcast_sring_id(struct wil6210_priv *wil)
{
	return WIL6210_MAX_STATUS_RINGS;
}

static inline void wil_ipa_set_bcast_sring_id(struct wil6210_priv *wil,
					      int bcast_sring_id) {}
static inline int wil_ipa_tx(void *ipa_handle,
			     struct wil_ring *ring,
			     struct sk_buff *skb) {return -EOPNOTSUPP; }

#endif /* CONFIG_WIL6210_IPA */

#endif /* WIL6210_IPA_H */
