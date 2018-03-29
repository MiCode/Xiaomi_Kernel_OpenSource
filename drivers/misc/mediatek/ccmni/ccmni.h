/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/timer.h>
#include <linux/if_ether.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <mt-plat/mt_ccci_common.h>

/*
 * normal workqueue:   MODEM_CAP_NAPI=0, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=0
 * workqueue with GRO: MODEM_CAP_NAPI=0, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=1
 * NAPI without GRO:   MODEM_CAP_NAPI=1, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=0
 * NAPI with GRO:      MODEM_CAP_NAPI=1, ENABLE_NAPI_GRO=1, ENABLE_WQ_GRO=0
 */
/* #define ENABLE_NAPI_GRO */
#ifdef CONFIG_MTK_ECCCI_C2K
#define ENABLE_WQ_GRO
#endif

#define  CCMNI_MTU              1500
#define  CCMNI_TX_QUEUE         1000
#define  CCMNI_NETDEV_WDT_TO    (1*HZ)

#define  IPV4_VERSION           0x40
#define  IPV6_VERSION           0x60

#define  SIOCSTXQSTATE          (SIOCDEVPRIVATE + 0)  /* stop/start tx queue */
#define  SIOCCCMNICFG           (SIOCDEVPRIVATE + 1)  /* configure ccmni/md remapping */

typedef struct ccmni_ctl_block ccmni_ctl_block_t;

struct ccmni_ch {
	int		   rx;
	int		   rx_ack;
	int		   tx;
	int		   tx_ack;
	int		   dl_ack;
	int		   multiq;
};

typedef struct ccmni_instance {
	int                index;
	int                md_id;
	struct ccmni_ch    ch;
	int                net_if_off;
	atomic_t           usage;
	/* use pointer to keep these items unique, while switching between CCMNI instances */
	struct timer_list  *timer;
	struct net_device  *dev;
	struct napi_struct *napi;
	unsigned int       rx_seq_num;
	unsigned int       tx_seq_num[2];
	unsigned int       flags;
	spinlock_t	       spinlock;
	ccmni_ctl_block_t  *ctlb;
	unsigned long      tx_busy_cnt[2];
	void               *priv_data;
} ccmni_instance_t;

typedef struct ccmni_ccci_ops {
	int                ccmni_ver;   /* CCMNI_DRV_VER */
	int                ccmni_num;
	unsigned char      name[16];	/* "ccmni" or "cc2mni" or "ccemni" */
	unsigned int       md_ability;
	unsigned int       irat_md_id;  /* with which md on iRAT */
	unsigned int       napi_poll_weigh;
	int (*send_pkt)(int md_id, int ccmni_idx, void *data, int is_ack);
	int (*napi_poll)(int md_id, int ccmni_idx, struct napi_struct *napi, int weight);
	int (*get_ccmni_ch)(int md_id, int ccmni_idx, struct ccmni_ch *channel);
} ccmni_ccci_ops_t;

typedef struct ccmni_ctl_block {
	ccmni_ccci_ops_t   *ccci_ops;
	ccmni_instance_t   *ccmni_inst[32];
	unsigned int       md_sta;
	struct wake_lock   ccmni_wakelock;
	char               wakelock_name[16];
	unsigned long long net_rx_delay[4];
} ccmni_ctl_block_t;

struct ccmni_dev_ops {
	/* must-have */
	int  skb_alloc_size;
	int  (*init)(int md_id, ccmni_ccci_ops_t *ccci_info);
	int  (*rx_callback)(int md_id, int ccmni_idx, struct sk_buff *skb, void *priv_data);
	void (*md_state_callback)(int md_id, int ccmni_idx, MD_STATE state, int is_ack);
	void (*exit)(int md_id);
	void (*dump)(int md_id, int ccmni_idx, unsigned int flag);
	void (*dump_rx_status)(int md_id, int ccmni_idx, unsigned long long *status);
	struct ccmni_ch *(*get_ch)(int md_id, int ccmni_idx);
	int (*is_ack_skb)(int md_id, struct sk_buff *skb);
};


typedef enum {
	CCMNI_DRV_V0   = 0,			/* for eemcs/eccci */
	CCMNI_DRV_V1   = 1,			/* for dual_ccci ccmni_v1 */
	CCMNI_DRV_V2   = 2,			/* for dual_ccci ccmni_v2 */
} CCMNI_DRV_VER;

typedef enum {
	CCMNI_RX_CH = 0,
	CCMNI_RX_ACK_CH = 1,
	CCMNI_TX_CH = 2,
	CCMNI_TX_ACK_CH = 3,
} CCMNI_CH;

typedef enum {
	CCMNI_ERR_TX_OK = 0,		/* ccci send pkt success */
	CCMNI_ERR_TX_BUSY = -1,		/* ccci tx packet buffer full and tx fail */
	CCMNI_ERR_MD_NO_READY = -2,	/* modem not ready and tx fail */
	CCMNI_ERR_TX_INVAL = -3, /* ccmni parameter error */
} CCMNI_ERRNO;

typedef enum {
	CCMNI_DBG_LEVEL_1 = (1<<0),
	CCMNI_DBG_LEVEL_TX = (1<<1),
	CCMNI_DBG_LEVEL_RX = (1<<2),
	CCMNI_DBG_LEVEL_ACK_SKB = (1<<3),
	CCMNI_DBG_LEVEL_TX_SKB = (1<<4),
	CCMNI_DBG_LEVEL_RX_SKB = (1<<5),
} CCMNI_DBG_LEVEL;

typedef enum {
	CCMNI_TXQ_NORMAL   = 0,
	CCMNI_TXQ_FAST     = 1,
	CCMNI_TXQ_NUM,
	CCMNI_TXQ_END     = CCMNI_TXQ_NUM
} CCMNI_TXQ_NO;

/*****************************extern function************************************/
/* int  ccmni_init(int md_id, ccmni_ccci_ops_t *ccci_info); */
/* void ccmni_exit(int md_id); */
/* int  ccmni_rx_callback(int md_id, struct sk_buff *skb, void *priv_data); */
/* void ccmni_md_state_callback(int md_id, int rx_ch, MD_STATE state); */


/*****************************ccmni debug function*******************************/
extern unsigned int ccmni_debug_level;

#define CCMNI_DBG_MSG(idx, fmt, args...) \
do { \
	if (ccmni_debug_level&CCMNI_DBG_LEVEL_1) \
		pr_debug("[ccci%d/net]" fmt, (idx+1), ##args); \
} while (0)

#define CCMNI_INF_MSG(idx, fmt, args...) pr_debug("[ccci%d/net]" fmt, (idx+1), ##args)
#define CCMNI_ERR_MSG(idx, fmt, args...) pr_err("[ccci%d/net][Error:%d]%s:" fmt, (idx+1), __LINE__, __func__, ##args)


#endif /* __CCCI_CCMNI_H__ */
