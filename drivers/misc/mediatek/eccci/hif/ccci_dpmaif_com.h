/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_MODEM_DPMA_COMM_H__
#define __CCCI_MODEM_DPMA_COMM_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>

#include "ccci_debug.h"
#include "ccci_dpmaif_reg_com.h"
#include "ccci_dpmaif_drv_com.h"
#include "ccci_dpmaif_debug.h"


#define  IPV4_VERSION           0x40
#define  IPV6_VERSION           0x60

#define MAX_DPMAIF_VER          (3)
#define DEFAULT_PLAT_INF        (6983)


#define DPMAIF_TRAFFIC_MONITOR_INTERVAL 10

//#define DPMAIF_REDUCE_RX_FLUSH

enum error_num {
	LOW_MEMORY_ERR = -15,
	LOW_MEMORY_PIT,
	LOW_MEMORY_BAT,
	LOW_MEMORY_SKB,
	LOW_MEMORY_DRB,
	LOW_MEMORY_TYPE_MAX, /* -10 */

	DMA_MAPPING_ERR,
	FLOW_CHECK_ERR,
	DATA_CHECK_FAIL,
	HW_REG_CHK_FAIL,
	HW_REG_TIME_OUT,
	ERROR_STOP_MAX, /* -4 */
};

#define DPMAIF_MAX_LRO 50

struct rx_lro_data {
	unsigned int    bid;
	struct sk_buff *skb;
	unsigned int    hof;
};

struct dpmaif_rx_lro_info {
	struct rx_lro_data data[DPMAIF_MAX_LRO];

	unsigned int    count;
};


#define DPMAIF_CAP_LRO            (1 << 0)
#define DPMAIF_CAP_2RXQ           (1 << 1)
#define DPMAIF_CAP_USE_RESV_MEM   (1 << 2)

#define DPMAIF_RXQ_NUM            2
#define DPMAIF_TXQ_NUM            4

/*Default DPMAIF DL common setting*/
#define DPMAIF_HW_BAT_REMAIN      64


#define DPMAIF_HW_BAT_RSVLEN      0 /* 88 */
#define DPMAIF_HW_PKT_BIDCNT      1  /* 3-->1 should be 1 in E1 */
#define DPMAIF_HW_PKT_ALIGN       64
#define DPMAIF_HW_MTU_SIZE        (3*1024 + 8)


#define DPMAIF_UL_DRB_ENTRY_SIZE  2048

#define DPMAIF_DL_BAT_BYTE_SIZE   8
#define DPMAIF_UL_DRB_BYTE_SIZE   8

//#define SKB_FRAGMENT_TEST

#ifdef SKB_FRAGMENT_TEST
#define DPMAIF_PKT_SIZE           (128*8)
#define DPMAIF_FRG_SIZE           (128*19)
#else
#define DPMAIF_PKT_SIZE           (128*13) /* == 1664 */
#define DPMAIF_FRG_SIZE           (128*15) /* 1920  */
#endif

#define DPMAIF_HW_BAT_PKTBUF      DPMAIF_PKT_SIZE
#define DPMAIF_HW_FRG_PKTBUF      DPMAIF_FRG_SIZE

#define DPMAIF_BUF_PKT_SIZE       DPMAIF_PKT_SIZE
#define DPMAIF_BUF_FRAG_SIZE      DPMAIF_FRG_SIZE



enum dpmaif_state_t {
	DPMAIF_STATE_MIN = 0,
	DPMAIF_STATE_PWROFF,
	DPMAIF_STATE_PWRON,
	DPMAIF_STATE_EXCEPTION,
	DPMAIF_STATE_MAX,
};


/* packet_type */
#define DES_PT_PD       0x00
#define DES_PT_MSG      0x01

/* drb->dtype */
#define DES_DTYP_PD     0x00
#define DES_DTYP_MSG    0x01
#define DES_DRB_CBIT    0x01

/* c_bit */
#define PKT_LAST_ONE    0x0
/* buffer_type */
#define PKT_BUF_FRAG    0x1

#define DPMAIF_UL_DRB_ENTRY_WORD  2 /* (sizeof(dpmaif_drb_pd)/4)*/

#define DPMAIF_NORMAL_PIT_BASE_DEF  \
	unsigned int    packet_type:1;  \
	unsigned int    c_bit:1;        \
	unsigned int    buffer_type:1;  \
	unsigned int    buffer_id:13;   \
	unsigned int    data_len:16;    \
	unsigned int    p_data_addr

struct dpmaif_normal_pit_base {
	DPMAIF_NORMAL_PIT_BASE_DEF;
};

#define DPMAIF_MSG_PIT_BASE_DEF     \
	unsigned int    packet_type:1;  \
	unsigned int    c_bit:1;        \
	unsigned int    check_sum:2;    \
	unsigned int    error_bit:1

struct dpmaif_msg_pit_base {
	DPMAIF_MSG_PIT_BASE_DEF;

	unsigned int    reserved1:11;
	unsigned int    channel_id:8;
	unsigned int    network_type:3;
	unsigned int    reserved2:4;
	unsigned int    dp:1;

	unsigned int    count_l:16;
};

struct dpmaif_normal_pit_v1 {
	DPMAIF_NORMAL_PIT_BASE_DEF;

	unsigned int    data_addr_ext:8;
	unsigned int    reserved:24;
};

struct dpmaif_msg_pit_v1 {
	DPMAIF_MSG_PIT_BASE_DEF;

	unsigned int    reserved:11;
	unsigned int    channel_id:8;
	unsigned int    network_type:3;
	unsigned int    reserved2:5;

	unsigned int    count_l:16;
	unsigned int    flow:4;
	unsigned int    cmd:3;
	unsigned int    reserved3:9;
	unsigned int    reserved4;
};

struct dpmaif_normal_pit_v2 {
	DPMAIF_NORMAL_PIT_BASE_DEF;

	unsigned int    data_addr_ext;
	unsigned int    pit_seq:16;
	unsigned int    ig:1;
	unsigned int    reserved2:7;
	unsigned int    ulq_done:6;
	unsigned int    dlq_done:2;
};

struct dpmaif_msg_pit_v2 {
	DPMAIF_MSG_PIT_BASE_DEF;

	unsigned int    src_qid:3;
	unsigned int    reserved:8;
	unsigned int    channel_id:8;
	unsigned int    network_type:3;
	unsigned int    reserved2:4;
	unsigned int    dp:1;

	unsigned int    count_l:16;
	unsigned int    flow:5;
	unsigned int    reserved3:3;
	unsigned int    cmd:3;
	unsigned int    reserved4:5;

	unsigned int    reserved5:3;
	unsigned int    vbid:13;
	unsigned int    reserved6:16;

	unsigned int    pit_seq:16;
	unsigned int    ig:1;
	unsigned int    reserved7:7;
	unsigned int    ulq_done:6;
	unsigned int    dlq_done:2;
};

struct dpmaif_normal_pit_v3 {
	DPMAIF_NORMAL_PIT_BASE_DEF;

	unsigned int    data_addr_ext;
	unsigned int    pit_seq:8;
	unsigned int    h_bid:3;
	unsigned int    reserved2:5;
	unsigned int    ig:1;
	unsigned int    bi_f:2;
	unsigned int    header_offset:5;
	unsigned int    ulq_done:6;
	unsigned int    dlq_done:2;
};

struct dpmaif_msg_pit_v3 {
	DPMAIF_MSG_PIT_BASE_DEF;

	unsigned int    src_qid:3;
	unsigned int    hpc_idx:4;
	unsigned int    reserved:4;
	unsigned int    channel_id:8;
	unsigned int    network_type:3;
	unsigned int    reserved2:4;
	unsigned int    dp:1;

	unsigned int    count_l:16;
	unsigned int    flow:5;
	unsigned int    reserved3:3;
	unsigned int    cmd:3;
	unsigned int    hp_idx:5;

	unsigned int    reserved4:3;
	unsigned int    vbid:13;
	unsigned int    pro:2;
	unsigned int    reserved5:6;
	unsigned int    hash:8;

	unsigned int    pit_seq:8;
	unsigned int    h_bid:3;
	unsigned int    reserved6:5;
	unsigned int    ig:1;
	unsigned int    reserved7:3;
	unsigned int    mr:2;
	unsigned int    reserved8:1;
	unsigned int    ip:1;

	unsigned int    ulq_done:6;
	unsigned int    dlq_done:2;
};


struct dpmaif_bat_base {
	unsigned int    p_buffer_addr;
	unsigned int    buffer_addr_ext:8;
	unsigned int    reserved:24;
};

struct dpmaif_bat_skb {
	struct sk_buff *skb;
	dma_addr_t      data_phy_addr;
	unsigned int    data_len;
};

struct dpmaif_bat_page {
	struct page    *page;
	dma_addr_t      data_phy_addr;
	unsigned long   offset;
	unsigned int    data_len;
};

struct dpmaif_bat_request {
	unsigned int    bat_cnt;
	void           *bat_base;
	dma_addr_t      bat_phy_addr; /* physical address for DMA */

	unsigned int    bat_pkt_cnt;
	void           *bat_pkt_addr;/* collect skb/page linked to bat */

	atomic_t        bat_wr_idx;
	atomic_t        bat_rd_idx;

	unsigned int    pkt_buf_sz;
};

struct dpmaif_drb_pd {
	unsigned int    dtyp:2;
	unsigned int    c_bit:1;
	unsigned int    reserved:5;
	unsigned int    data_addr_ext:8;
	unsigned int    data_len:16;

	unsigned int    p_data_addr;
};

struct dpmaif_drb_msg {
	unsigned int    dtyp:2;
	unsigned int    c_bit:1;
	unsigned int    reserved:13;
	unsigned int    packet_len:16;
	unsigned int    count_l:16;
	unsigned int    channel_id:8;
	unsigned int    network_type:3;
	unsigned int    r:1;
	unsigned int    ipv4:1; /* enable ul checksum offload for ipv4 header */
	unsigned int    l4:1; /* enable ul checksum offload for tcp/udp */
	unsigned int    rsv:2;
};

struct dpmaif_drb_skb {
	struct sk_buff *skb;
	dma_addr_t      phy_addr; /* physical address for DMA */
	unsigned short  data_len;

	/* just for debug */
	unsigned short  drb_idx:13;
	unsigned short  is_msg:1;
	unsigned short  is_frag:1;
	unsigned short  is_last_one:1;
};

/* for isr count record */
struct dpmaif_isr_count {
	u64 ts_start;
	u64 ts_end;
	u32 irq_cnt[64];
};

struct dpmaif_rx_queue {
	unsigned char   index;
	bool            started;
	unsigned short  budget;
	unsigned int    enqueue_skb_cnt;

	unsigned int    pit_cnt;
	void           *pit_base;
	dma_addr_t      pit_phy_addr;

	atomic_t        pit_rd_idx;
	atomic_t        pit_wr_idx;

	struct tasklet_struct    rxq_task;
	wait_queue_head_t        rxq_wq;
	struct task_struct      *rxq_push_thread;
	atomic_t                 rxq_processing;

	unsigned int             cur_chn_idx;
	unsigned int             check_sum;
	int                      skb_idx;
	unsigned int             pit_dp;

	unsigned long            pit_dummy_cnt;
	unsigned long            pit_dummy_idx;
	unsigned char            pit_reload_en;

#ifdef DPMAIF_REDUCE_RX_FLUSH
	atomic_t                 rxq_need_flush;
#endif
	struct dpmaif_rx_lro_info lro_info;

	unsigned int            irq_id;
	unsigned long           irq_flags;
	atomic_t                irq_enabled;
	char                    irq_name[50];

	irqreturn_t (*rxq_isr)(int irq, void *data);
	void (*rxq_tasklet)(unsigned long data);
	void (*rxq_drv_unmask_dl_interrupt)(void);
	int  (*rxq_drv_dl_add_pit_remain_cnt)(unsigned short cnt);

	/* isr count record */
	struct dpmaif_isr_count *isr_cnt_each_rxq;
	u64 isr_pre_time;
	u32 isr_log_idx;
};


struct dpmaif_tx_queue {
	unsigned char       index;
	bool                started;
	atomic_t            txq_budget;

	unsigned int        drb_cnt;
	void               *drb_base;
	dma_addr_t          drb_phy_addr;  /* physical address for DMA */

	atomic_t            drb_wr_idx;
	atomic_t            drb_rd_idx;
	atomic_t            drb_rel_rd_idx;
	void               *drb_skb_base;

	wait_queue_head_t   req_wq;

	/* For Tx done Kernel thread */
	struct hrtimer      txq_done_timer;
	atomic_t            txq_done;
	wait_queue_head_t   txq_done_wait;
	struct task_struct *txq_done_thread;

	spinlock_t          txq_lock;
	atomic_t            txq_processing;

	atomic_t            txq_resume_done;

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	unsigned int        busy_count;
#endif
};

struct dpmaif_ctrl {
	enum   dpmaif_state_t     dpmaif_state;
	struct dpmaif_tx_queue    txq[DPMAIF_TXQ_NUM];
	struct dpmaif_rx_queue   *rxq;

	unsigned char             hif_id;
	atomic_t                  wakeup_src;

	/* for 95 dpmaif new register */
	void __iomem              *ao_ul_base;
	void __iomem              *ao_dl_base;
	void __iomem              *pd_ul_base;
	void __iomem              *pd_dl_base;
	void __iomem              *pd_misc_base;
	void __iomem              *pd_md_misc_base;
	void __iomem              *pd_sram_base;

	/* for 97 dpmaif new register */
	void __iomem              *pd_rdma_base;
	void __iomem              *pd_wdma_base;
	void __iomem              *ao_md_dl_base;

	/* for 98 dpmaif new register */
	void __iomem              *ao_dl_sram_base;
	void __iomem              *ao_ul_sram_base;
	void __iomem              *ao_msic_sram_base;

	void __iomem              *pd_mmw_hpc_base;
	void __iomem              *pd_dl_lro_base;

	struct regmap             *infra_ao_base;
	void __iomem              *infra_ao_mem_base;
	void __iomem              *infra_reset_pd_base;

	struct device             *dev;

	atomic_t                   suspend_flag;
	unsigned int               capability;
	unsigned int               support_lro;
	unsigned int               support_2rxq;
	unsigned int               real_rxq_num;
	int                        enable_pit_debug;

	struct dpmaif_bat_request  *bat_skb;
	struct dpmaif_bat_request  *bat_frg;
	wait_queue_head_t           bat_alloc_wq;
	struct task_struct         *bat_alloc_thread;
	wait_queue_head_t           skb_alloc_wq;
	struct task_struct         *skb_alloc_thread;
	unsigned int                skb_start_alloc;
	atomic_t                    bat_need_alloc;
	atomic_t                    bat_paused_alloc;
	int                         bat_alloc_running;

	unsigned int                dl_bat_entry_size;
	unsigned int                dl_pit_entry_size;
	unsigned int                dl_pit_byte_size;

	struct dpmaif_clk_node     *clk_tbs;

	unsigned int                suspend_reg_int_mask_bak;

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	unsigned int                tx_tfc_pkgs[DPMAIF_TXQ_NUM];
	unsigned int               *rx_tfc_pkgs;
	unsigned int                tx_pre_tfc_pkgs[DPMAIF_TXQ_NUM];
	unsigned long long          tx_done_last_start_time[DPMAIF_TXQ_NUM];

	struct timer_list           traffic_monitor;
#endif
};

struct dpmaif_clk_node {
	struct clk    *clk_ref;
	unsigned char *clk_name;
};


extern unsigned int            g_plat_inf;
extern struct dpmaif_plat_ops  g_plt_ops;


int ccci_dpmaif_init_v1(struct device *dev);
int ccci_dpmaif_init_v2(struct device *dev);
int ccci_dpmaif_init_v3(struct device *dev);

u32 get_ringbuf_used_cnt(u32 len, u32 rdx, u32 wdx);
u32 get_ringbuf_free_cnt(u32 len, u32 rdx, u32 wdx);
u32 get_ringbuf_next_idx(u32 len, u32 idx, u32 cnt);
u32 get_ringbuf_release_cnt(u32 len, u32 rel_idx, u32 rd_idx);

void ccci_dpmaif_txq_release_skb(struct dpmaif_tx_queue *txq, unsigned int release_cnt);

extern struct regmap *syscon_regmap_lookup_by_phandle(
		struct device_node *np, const char *property);

extern int regmap_write(struct regmap *map,
		unsigned int reg, unsigned int val);

extern int regmap_read(struct regmap *map,
		unsigned int reg, unsigned int *val);

extern void ccmni_clr_flush_timer(void);

extern void mt_irq_dump_status(unsigned int irq);

extern void ccmni_set_tcp_is_need_gro(u32 tcp_is_need_gro);
extern void ccmni_set_cur_speed(u64 cur_dl_speed);

#endif				/* __CCCI_MODEM_DPMA_COMM_H__ */
