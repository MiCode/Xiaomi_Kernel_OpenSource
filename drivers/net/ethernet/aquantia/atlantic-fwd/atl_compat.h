/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * Portions Copyright (C) various contributors (see specific commit references)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_COMPAT_H_
#define _ATL_COMPAT_H_

#include <linux/version.h>

#include <linux/pci.h>
#include <linux/msi.h>

/* If the kernel is not RHEL / CentOS, then the 2 identifiers below will be
 * undefined. Define them this way to simplify the checks below.
 */
#ifndef RHEL_RELEASE_CODE
#define RHEL_RELEASE_CODE 0
#endif
#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a,b) 1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
#define ATL_HAVE_MINMAX_MTU
#elif (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,5)) && \
      (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8,0))
#define ndo_change_mtu ndo_change_mtu_rh74
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,3)
/* introduced in commit 3f1ac7a700d039c61d8d8b99f28d605d489a60cf */
#define ATL_HAVE_ETHTOOL_KSETTINGS

/* introduced in commit 72bb68721f80a1441e871b6afc9ab0b3793d5031 */
#define ATL_HAVE_IPV6_NTUPLE
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,3)
/* introduced in commit 892311f66f2411b813ca631009356891a0c2b0a1 */
#define ATL_HAVE_RXHASH_TYPE
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0) || RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,3)
/* introduced in commit 3de0b592394d17b2c41a261a6a493a521213f299 */
#define ATL_HAVE_ETHTOOL_RXHASH
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)

/* ->ndo_get_stats64 return type was changed to void in commit
 * bc1f44709cf27fb2a5766cadafe7e2ad5e9cb221. It's safe to just cast
 * the pointer to avoid the warning because the only place
 * ->ndo_get_stats64 was invoked before the change ignored the return
 * value. */
#define ATL_COMPAT_CAST_NDO_GET_STATS64

#endif	/* 4.11.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)

/* introduced in commit 94842b4fc4d6b1691cfc86c6f5251f299d27f4ba */
#define ETHTOOL_LINK_MODE_2500baseT_Full_BIT 47
#define ETHTOOL_LINK_MODE_5000baseT_Full_BIT 48

#endif	/* 4.10.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)

/* from commit 1dff8083a024650c75a9c961c38082473ceae8cf */
#define page_to_virt(x)	__va(PFN_PHYS(page_to_pfn(x)))
#endif	/* 4.7.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)

/* from commit d31eb342409b24e3d2e1989c775f3361e93acc08 */
/* Helpers to hide struct msi_desc implementation details */
#define msi_desc_to_dev(desc)		(&(desc)->dev.dev)
#define dev_to_msi_list(dev)		(&to_pci_dev((dev))->msi_list)
#define first_msi_entry(dev)		\
	list_first_entry(dev_to_msi_list((dev)), struct msi_desc, list)
#define for_each_msi_entry(desc, dev)	\
	list_for_each_entry((desc), dev_to_msi_list((dev)), list)

#define first_pci_msi_entry(pdev)	first_msi_entry(&(pdev)->dev)
#define for_each_pci_msi_entry(desc, pdev)	\
	for_each_msi_entry((desc), &(pdev)->dev)

static inline struct pci_dev *msi_desc_to_pci_dev(struct msi_desc *desc)
{
	return desc->dev;
}

#endif	/* 3.19.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,3)
/* ->xmit_more introduced in commit
 * 0b725a2ca61bedc33a2a63d0451d528b268cf975 for 3.18-rc1 */
static inline int skb_xmit_more(struct sk_buff *skb)
{
	return 0;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,2,0)
static inline int skb_xmit_more(struct sk_buff *skb)
{
	return netdev_xmit_more();
}
#else /* 3.18.0-5.2.0 for vanilla, anything less than 5.2.0 for RHEL */
static inline int skb_xmit_more(struct sk_buff *skb)
{
	return skb->xmit_more;
}
#endif	/* 3.18.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0) &&                           \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 3)
typedef u16 (*select_queue_fallback_t)(struct net_device *dev,
				       struct sk_buff *skb);
static inline u16 atlfwd_nl_pick0(struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}
#endif

/* NB! select_queue_fallback_t MUST be defined before #include on RHEL < 7.3 */
#include "atl_fwdnl.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0) &&                           \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 3)
static inline u16 atlfwd_nl_select_queue(struct net_device *dev,
					 struct sk_buff *skb)
{
	return atlfwd_nl_select_queue_fallback(dev, skb, NULL, atlfwd_nl_pick0);
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0) &&                         \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 3)
static inline u16 atlfwd_nl_select_queue(struct net_device *dev,
					 struct sk_buff *skb, void *accel_priv)
{
	return atlfwd_nl_select_queue_fallback(
		dev, skb, (struct net_device *)accel_priv, atlfwd_nl_pick0);
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0) &&                         \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 0)
static inline u16 atlfwd_nl_select_queue(struct net_device *dev,
					 struct sk_buff *skb, void *accel_priv,
					 select_queue_fallback_t fallback)
{
	return atlfwd_nl_select_queue_fallback(
		dev, skb, (struct net_device *)accel_priv, fallback);
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
static inline u16 atlfwd_nl_select_queue(struct net_device *dev,
					 struct sk_buff *skb,
					 struct net_device *sb_dev,
					 select_queue_fallback_t fallback)
{
	return atlfwd_nl_select_queue_fallback(dev, skb, sb_dev, fallback);
}
#else
static inline u16 atlfwd_nl_select_queue(struct net_device *dev,
					 struct sk_buff *skb,
					 struct net_device *sb_dev)
{
	return atlfwd_nl_select_queue_fallback(dev, skb, sb_dev,
					       netdev_pick_tx);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
/* introduced in commit 686fef928bba6be13cabe639f154af7d72b63120 */
static inline void timer_setup(struct timer_list *timer,
	void (*callback)(struct timer_list *), unsigned int flags)
{
	setup_timer(timer, (void (*)(unsigned long))callback,
			(unsigned long)timer);
}

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)
#endif	/* 4.14.0 && RHEL < 7.6 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,5)

#define ATL_COMPAT_PCI_ALLOC_IRQ_VECTORS
int atl_compat_pci_alloc_irq_vectors(struct pci_dev *dev,
	unsigned int min_vecs, unsigned int max_vecs, unsigned int flags);
static inline int pci_alloc_irq_vectors(struct pci_dev *dev,
	unsigned int min_vecs, unsigned int max_vecs, unsigned int flags)
{
	return atl_compat_pci_alloc_irq_vectors(dev, min_vecs,
		max_vecs, flags);
}

int atl_compat_pci_irq_vector(struct pci_dev *dev, unsigned int nr);
static inline int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	return atl_compat_pci_irq_vector(dev, nr);
}

static inline void pci_free_irq_vectors(struct pci_dev *dev)
{
	pci_disable_msix(dev);
	pci_disable_msi(dev);
}

/* from commit 4fe0d154880bb6eb833cbe84fa6f385f400f0b9c */
#define PCI_IRQ_LEGACY		(1 << 0) /* allow legacy interrupts */
#define PCI_IRQ_MSI		(1 << 1) /* allow MSI interrupts */
#define PCI_IRQ_MSIX		(1 << 2) /* allow MSI-X interrupts */

#endif /* 4.8.0 && RHEL < 7.5 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,4)
/* from commit be9d2e8927cef02076bb7b5b2637cd9f4be2e8df */
static inline int
pci_request_mem_regions(struct pci_dev *pdev, const char *name)
{
	return pci_request_selected_regions(pdev,
			    pci_select_bars(pdev, IORESOURCE_MEM), name);
}
#endif /* 4.8.0 && RHEL < 7.4 */

#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,3)

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)

/* from commit fe896d1878949ea92ba547587bc3075cc688fb8f */
static inline void page_ref_inc(struct page *page)
{
	atomic_inc(&page->_count);
}

/* introduced in commit 795bb1c00dd338aa0d12f9a7f1f4776fb3160416 */
#define napi_consume_skb(__skb, __budget) dev_consume_skb_any(__skb)

/* from commit 3f1ac7a700d039c61d8d8b99f28d605d489a60cf */
#define ETHTOOL_LINK_MODE_10baseT_Half_BIT 0
#define ETHTOOL_LINK_MODE_10baseT_Full_BIT 1
#define ETHTOOL_LINK_MODE_100baseT_Half_BIT 2
#define ETHTOOL_LINK_MODE_100baseT_Full_BIT 3
#define ETHTOOL_LINK_MODE_1000baseT_Half_BIT 4
#define ETHTOOL_LINK_MODE_1000baseT_Full_BIT 5
#define ETHTOOL_LINK_MODE_10000baseT_Full_BIT 12

/* IPv6 NFC API introduced in commit
 * 72bb68721f80a1441e871b6afc9ab0b3793d5031 */

/* Define the IPv6 constants for kernels not supporting IPv6 in the
 * NFC API to reduce the number of #ifdefs in the code. The constants
 * themselves may already be defined for RSS hash management API, so
 * #undef them first */
#undef TCP_V6_FLOW
#define TCP_V6_FLOW 0x05

#undef UDP_V6_FLOW
#define UDP_V6_FLOW 0x06

#undef SCTP_V6_FLOW
#define SCTP_V6_FLOW 0x07

#undef IPV6_USER_FLOW
#define IPV6_USER_FLOW 0x0e
#define IPV4_USER_FLOW IP_USER_FLOW

#endif	/* 4.6.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0)

/* introduced in commit c7f5d105495a38ed09e70d825f75d9d7d5407264
 * stub it */
static inline int eth_platform_get_mac_address(struct device *dev, u8 *mac_addr)
{
	return -ENODEV;
}

#endif	/* 4.5.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0)

/* introduced in 2f064f3485cd29633ad1b3cfb00cc519509a3d72
 * The new implementation can't be used directly as it requires
 * changes to linux/mm code. Use the old check described in the
 * commit with older kernels insted. It can lead to false positives,
 * but as we only use it to determine whether the page is re-usable,
 * the false positives can only decrease performance. */
static inline bool page_is_pfmemalloc(struct page *page)
{
	return page->pfmemalloc && !page->mapping;
}
#endif	/* 4.1.0 */

#endif /* RHEL_RELEASE_CODE < 7.3 */

#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,2)

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
#define ETHTOOL_RX_FLOW_SPEC_RING      0x00000000FFFFFFFFLL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF   0x000000FF00000000LL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF 32
static inline __u64 ethtool_get_flow_spec_ring(__u64 ring_cookie)
{
       return ETHTOOL_RX_FLOW_SPEC_RING & ring_cookie;
};
static inline __u64 ethtool_get_flow_spec_ring_vf(__u64 ring_cookie)
{
       return (ETHTOOL_RX_FLOW_SPEC_RING_VF & ring_cookie) >>
                               ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF;
};
#endif /* 4.2.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)

/* renamed in commit df8a39defad46b83694ea6dd868d332976d62cc0 */
#define skb_vlan_tag_present(__skb) vlan_tx_tag_present(__skb)
#define skb_vlan_tag_get(__skb) vlan_tx_tag_get(__skb)
#endif	/* 4.0.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)

/* introduced in commit 1077fa36f23e259858caf6f269a47393a5aff523
 * use plain rmb() for now*/
#define dma_rmb()	rmb()

/* from commit 9c0c112422a2a6b06fcddcaf21957676490cebba */
static inline int eth_skb_pad(struct sk_buff *skb)
{
	unsigned int len = ETH_ZLEN;
	unsigned int size = skb->len;

	if (unlikely(size < len)) {
		len -= size;
		if (skb_pad(skb, len))
			return -ENOMEM;
		__skb_put(skb, len);
	}
	return 0;
}

/* introduced in commit 71dfda58aaaf4bf6b1bc59f9d8afa635fa1337d4 */
#define __dev_alloc_pages(__flags, __order) __skb_alloc_pages(__flags | __GFP_COMP, NULL, __order)

/* introduced in commit fd11a83dd3630ec6a60f8a702446532c5c7e1991 */
#define napi_alloc_skb(__napi, __len) netdev_alloc_skb_ip_align((__napi)->dev, __len)

/* introduced in commit 3b47d30396bae4f0bd1ff0dbcd7c4f5077e7df4e */
#define napi_complete_done(__napi, __work_done) napi_complete(__napi)

/* introduced in commit bc9ad166e38ae1cdcb5323a8aa45dff834d68bfa */
#define napi_schedule_irqoff(__napi) napi_schedule(__napi)

/* READ_ONCE() / WRITE_ONCE
 * from commit 230fa253df6352af12ad0a16128760b5cb3f92df with changes
 * from 43239cbe79fc369f5d2160bd7f69e28b5c50a58c and
 * 7bd3e239d6c6d1cad276e8f130b386df4234dcd7 */

/* READ_ONCE / WRTIE_ONCE were also cherry-picked into 3.12.58 as
 * b5be8baf9e0d5ea035588a0430b2af4989e07572  and
 * dda458f0183649e8613a62bb59bec4e5acb883aa */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,12,58) || LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)

static __always_inline void __read_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(__u8 *)res = *(volatile __u8 *)p; break;
	case 2: *(__u16 *)res = *(volatile __u16 *)p; break;
	case 4: *(__u32 *)res = *(volatile __u32 *)p; break;
	case 8: *(__u64 *)res = *(volatile __u64 *)p; break;
	default:
		barrier();
		__builtin_memcpy((void *)res, (const void *)p, size);
		barrier();
	}
}

static __always_inline void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile __u8 *)p = *(__u8 *)res; break;
	case 2: *(volatile __u16 *)p = *(__u16 *)res; break;
	case 4: *(volatile __u32 *)p = *(__u32 *)res; break;
	case 8: *(volatile __u64 *)p = *(__u64 *)res; break;
	default:
		barrier();
		__builtin_memcpy((void *)p, (const void *)res, size);
		barrier();
	}
}

#define READ_ONCE(x)							\
	({ typeof(x) __val; __read_once_size(&x, &__val, sizeof(__val)); __val; })

#define WRITE_ONCE(x, val) \
	({ typeof(x) __val; __val = val; __write_once_size(&x, &__val, sizeof(__val)); __val; })

#endif	/* 3.12.58 */

#endif	/* 3.19.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)

/* introduced in commit 56193d1bce2b2759cb4bdcc00cd05544894a0c90
 * pull the whole head buffer len for now*/
#define eth_get_headlen(__data, __max_len) (__max_len)

#endif	/* 3.18.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)

/* from commit 286ab723d4b83d37deb4017008ef1444a95cfb0d */
static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	*(u32 *)dst = *(const u32 *)src;
	*(u16 *)(dst + 4) = *(const u16 *)(src + 4);
#else
	u16 *a = (u16 *)dst;
	const u16 *b = (const u16 *)src;

	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
#endif
}

/* introduced in commit e6247027e5173c00efb2084d688d06ff835bc3b0 */
#define dev_consume_skb_any(__skb) dev_kfree_skb_any(__skb)

/* from commit 09323cc479316e046931a2c679932204b36fea6c */
enum pkt_hash_types {
	PKT_HASH_TYPE_NONE,	/* Undefined type */
	PKT_HASH_TYPE_L2,	/* Input: src_MAC, dest_MAC */
	PKT_HASH_TYPE_L3,	/* Input: src_IP, dst_IP */
	PKT_HASH_TYPE_L4,	/* Input: src_IP, dst_IP, src_port, dst_port */
};

static inline void
skb_set_hash(struct sk_buff *skb, __u32 hash, enum pkt_hash_types type)
{
	skb->l4_rxhash = (type == PKT_HASH_TYPE_L4);
	skb->rxhash = hash;
}

/* from commit 302a2523c277bea0bbe8340312b09507905849ed */
#define ATL_COMPAT_PCI_ENABLE_MSIX_RANGE
int atl_compat_pci_enable_msi_range(struct pci_dev *dev, int minvec,
	int maxvec);
int atl_compat_pci_enable_msix_range(struct pci_dev *dev,
	struct msix_entry *entries, int minvec, int maxvec);

static inline int pci_enable_msi_range(struct pci_dev *dev, int minvec,
	int maxvec)
{
	return atl_compat_pci_enable_msi_range(dev, minvec, maxvec);
}

static inline int pci_enable_msix_range(struct pci_dev *dev,
	struct msix_entry *entries, int minvec, int maxvec)
{
	return atl_compat_pci_enable_msix_range(dev, entries, minvec, maxvec);
}

#endif	/* 3.14.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,12,27)

/* from commit 4aa806b771d16b810771d86ce23c4c3160888db3
 * also cherry-picked into 3.12-stable as
 * 8bac7a35e60ca70c8d12ddbfdf28a8df5a976b2b */
static inline int dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	int rc = dma_set_mask(dev, mask);
	if (rc == 0)
		dma_set_coherent_mask(dev, mask);
	return rc;
}

#endif	/* 3.12.27 */

#endif /* RHEL_RELEASE_CODE < 7.2 */

#endif
