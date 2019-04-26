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

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)

/* introduced in commit 686fef928bba6be13cabe639f154af7d72b63120 */
static inline void timer_setup(struct timer_list *timer,
	void (*callback)(struct timer_list *), unsigned int flags)
{
	setup_timer(timer, (void (*)(unsigned long))callback,
			(unsigned long)timer);
}

#endif	/* 4.14.0 */

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

#else  /* 4.10.0 */

#define ATL_HAVE_MINMAX_MTU

#endif	/* 4.10.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)

/* from commit be9d2e8927cef02076bb7b5b2637cd9f4be2e8df */
static inline int
pci_request_mem_regions(struct pci_dev *pdev, const char *name)
{
	return pci_request_selected_regions(pdev,
			    pci_select_bars(pdev, IORESOURCE_MEM), name);
}

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

#endif /* 4.8.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)

/* from commit 1dff8083a024650c75a9c961c38082473ceae8cf */
#define page_to_virt(x)	__va(PFN_PHYS(page_to_pfn(x)))
#endif	/* 4.7.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)

/* from commit fe896d1878949ea92ba547587bc3075cc688fb8f */
static inline void page_ref_inc(struct page *page)
{
	atomic_inc(&page->_count);
}

/* introduced in commit 795bb1c00dd338aa0d12f9a7f1f4776fb3160416 */
#define napi_consume_skb(__skb, __budget) dev_consume_skb_any(__skb)

/* from commit 3f1ac7a700d039c61d8d8b99f28d605d489a60cf */
#define ETHTOOL_LINK_MODE_100baseT_Full_BIT 3
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

#else

/* introduced in commit 3f1ac7a700d039c61d8d8b99f28d605d489a60cf */
#define ATL_HAVE_ETHTOOL_KSETTINGS

/* introduced in commit 72bb68721f80a1441e871b6afc9ab0b3793d5031 */
#define ATL_HAVE_IPV6_NTUPLE

#endif	/* 4.6.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0)

/* introduced in commit c7f5d105495a38ed09e70d825f75d9d7d5407264
 * stub it */
static inline int eth_platform_get_mac_address(struct device *dev, u8 *mac_addr)
{
	return -ENODEV;
}

#endif	/* 4.5.0 */

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
#endif	/* 4.2.0 */

#endif
