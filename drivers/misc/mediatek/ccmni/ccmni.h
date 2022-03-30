/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */


#ifndef __CCCI_CCMNI_H__
#define __CCCI_CCMNI_H__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/pm_wakeup.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/timer.h>
#include <linux/if_ether.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include "mt-plat/mtk_ccci_common.h"

/*
 * normal workqueue:   MODEM_CAP_NAPI=0, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=0
 * workqueue with GRO: MODEM_CAP_NAPI=0, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=1
 * NAPI without GRO:   MODEM_CAP_NAPI=1, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=0
 * NAPI with GRO:      MODEM_CAP_NAPI=1, ENABLE_NAPI_GRO=1, ENABLE_WQ_GRO=0
 */
/* #define ENABLE_NAPI_GRO */
#define ENABLE_WQ_GRO

#define  CCMNI_MTU              1500
#define  CCMNI_TX_QUEUE         1000
#define  CCMNI_NETDEV_WDT_TO    (1*HZ)

#define  IPV4_VERSION           0x40
#define  IPV6_VERSION           0x60

/* stop/start tx queue */
#define  SIOCSTXQSTATE          (SIOCDEVPRIVATE + 0)
/* configure ccmni/md remapping */
#define  SIOCCCMNICFG           (SIOCDEVPRIVATE + 1)
/* forward filter for ccmni tx packet */
#define  SIOCFWDFILTER          (SIOCDEVPRIVATE + 2)
/* disable ack first mechanism */
#define  SIOCACKPRIO          (SIOCDEVPRIVATE + 3)
/* push the queued packet to stack */
#define  SIOPUSHPENDING       (SIOCDEVPRIVATE + 4)



#define  IS_CCMNI_LAN(dev)      \
	(strncmp(dev->name, "ccmni-lan", 9) == 0)
#define  CCMNI_TX_PRINT_F	(0x1 << 0)
#define MDT_TAG_PATTERN         0x46464646
#define  CCMNI_FLT_NUM          32

/* #define CCMNI_MET_DEBUG */
#if defined(CCMNI_MET_DEBUG)
#define MET_LOG_TIMER           20 /*20ms*/
#define CCMNI_RX_MET_ID         0xF0000
#define CCMNI_TX_MET_ID         0xF1000
#endif


struct ccmni_ch {
	int		   rx;
	int		   rx_ack;
	int		   tx;
	int		   tx_ack;
	int		   dl_ack;
	int		   multiq;
};

enum {
	CCMNI_FLT_ADD    = 1,
	CCMNI_FLT_DEL    = 2,
	CCMNI_FLT_FLUSH  = 3,
};

struct ccmni_fwd_filter {
	u16 ver;           /* ipv4 or ipv6*/
	u8 s_pref;         /* mask number for source ip address */
	u8 d_pref;         /* mask number for dest ip address */
	union {
		struct {
			u32 saddr; /* source ip address */
			u32 daddr; /* dest ip address */
		} ipv4;
		struct {
			u32 saddr[4];
			u32 daddr[4];
		} ipv6;
	};
};

struct ccmni_flt_act {
	u32 action;
	struct ccmni_fwd_filter flt;
};

struct ccmni_instance {
	int                index;
	int                md_id;
	struct ccmni_ch    ch;
	int                net_if_off;
	unsigned int	   ack_prio_en;
	atomic_t           usage;
	/* use pointer to keep these items unique,
	 * while switching between CCMNI instances
	 */
	struct timer_list  *timer;
	struct net_device  *dev;
	struct napi_struct *napi;
	unsigned int       rx_seq_num;
	unsigned int       tx_seq_num[2];
	unsigned int       flags[2];
	spinlock_t         *spinlock;
	struct ccmni_ctl_block  *ctlb;
	unsigned long      tx_busy_cnt[2];
	unsigned long      tx_full_tick[2];
	unsigned long      tx_irq_tick[2];
	unsigned int       tx_full_cnt[2];
	unsigned int       tx_irq_cnt[2];
	unsigned int       rx_gro_cnt;
	unsigned int       flt_cnt;
	struct ccmni_fwd_filter flt_tbl[CCMNI_FLT_NUM];
#if defined(CCMNI_MET_DEBUG)
	unsigned long      rx_met_time;
	unsigned long      tx_met_time;
	unsigned long      rx_met_bytes;
	unsigned long      tx_met_bytes;
#endif
	struct timespec64 flush_time;
	void               *priv_data;

	/* For queue packet before ready */
	struct workqueue_struct *worker;
	struct delayed_work pkt_queue_work;
};

struct ccmni_ccci_ops {
	int                ccmni_ver;   /* CCMNI_DRV_VER */
	int                ccmni_num;
	/* "ccmni" or "cc2mni" or "ccemni" */
	unsigned char      name[16];
	unsigned int       md_ability;
	/* with which md on iRAT */
	unsigned int       irat_md_id;
	unsigned int       napi_poll_weigh;
	int (*send_pkt)(int md_id, int ccmni_idx, void *data, int is_ack);
	int (*napi_poll)(int md_id, int ccmni_idx,
			struct napi_struct *napi, int weight);
	int (*get_ccmni_ch)(int md_id, int ccmni_idx, struct ccmni_ch *channel);
	void (*ccci_net_init)(char *name);
	int (*ccci_handle_port_list)(int status, char *name);
};

struct ccmni_ctl_block {
	struct ccmni_ccci_ops   *ccci_ops;
	struct ccmni_instance   *ccmni_inst[32];
	unsigned int       md_sta;
	struct wakeup_source   *ccmni_wakelock;
	char               wakelock_name[16];
	unsigned long long net_rx_delay[4];
};

struct ccmni_dev_ops {
	/* must-have */
	int  skb_alloc_size;
	int  (*init)(int md_id, struct ccmni_ccci_ops *ccci_info);
	int  (*rx_callback)(int md_id, int ccmni_idx,
			struct sk_buff *skb, void *priv_data);
	void (*md_state_callback)(int md_id,
		int ccmni_idx, enum MD_STATE state);
	void (*queue_state_callback)(int md_id, int ccmni_idx,
			enum HIF_STATE state, int is_ack);
	void (*exit)(int md_id);
	void (*dump)(int md_id, int ccmni_idx, unsigned int flag);
	void (*dump_rx_status)(int md_id, unsigned long long *status);
	struct ccmni_ch *(*get_ch)(int md_id, int ccmni_idx);
	int (*is_ack_skb)(int md_id, struct sk_buff *skb);
};

struct md_drt_tag {
	u8  in_netif_id;
	u8  out_netif_id;
	u16 port;
};

struct md_tag_packet {
	u32 guard_pattern;
	struct md_drt_tag info;
};

enum {
	/* for eemcs/eccci */
	CCMNI_DRV_V0   = 0,
	/* for dual_ccci ccmni_v1 */
	CCMNI_DRV_V1   = 1,
	/* for dual_ccci ccmni_v2 */
	CCMNI_DRV_V2   = 2,
};

enum {
	/* ccci send pkt success */
	CCMNI_ERR_TX_OK = 0,
	/* ccci tx packet buffer full and tx fail */
	CCMNI_ERR_TX_BUSY = -1,
	/* modem not ready and tx fail */
	CCMNI_ERR_MD_NO_READY = -2,
	/* modem not ready and tx fail */
	CCMNI_ERR_TX_INVAL = -3,
};

enum {
	CCMNI_TXQ_NORMAL = 0,
	CCMNI_TXQ_FAST = 1,
	CCMNI_TXQ_NUM,
	CCMNI_TXQ_END = CCMNI_TXQ_NUM
};

struct arphdr_in {
	__be16 ar_hrd;
	__be16 ar_pro;
	unsigned char ar_hln;
	unsigned char ar_pln;
	__be16 ar_op;

	unsigned char ar_sha[ETH_ALEN];
	unsigned char ar_sip[4];
	unsigned char ar_tha[ETH_ALEN];
	unsigned char ar_tip[4];
};

/***********************ccmni debug function*****************************/
#define CCMNI_DBG_MSG(idx, fmt, args...) \
	pr_debug("[ccci%d/net]" fmt, (idx+1), ##args)
#define CCMNI_INF_MSG(idx, fmt, args...) \
	pr_info("[ccci%d/net]" fmt, (idx+1), ##args)
#define CCMNI_PR_DBG(idx, fmt, args...) \
	pr_debug("[ccci%d/net][Error:%d]%s:" fmt, (idx+1),\
		__LINE__, __func__, ##args)

#endif /* __CCCI_CCMNI_H__ */
