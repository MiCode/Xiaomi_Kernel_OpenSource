/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 *  BAM DMUX module.
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/clk.h>
#include <linux/wakelock.h>

#include <mach/sps.h>
#include <mach/bam_dmux.h>
#include <mach/msm_smsm.h>
#include <mach/subsystem_notif.h>
#include <mach/socinfo.h>

#define BAM_CH_LOCAL_OPEN       0x1
#define BAM_CH_REMOTE_OPEN      0x2
#define BAM_CH_IN_RESET         0x4

#define BAM_MUX_HDR_MAGIC_NO    0x33fc

#define BAM_MUX_HDR_CMD_DATA    0
#define BAM_MUX_HDR_CMD_OPEN    1
#define BAM_MUX_HDR_CMD_CLOSE   2

#define POLLING_MIN_SLEEP	950	/* 0.95 ms */
#define POLLING_MAX_SLEEP	1050	/* 1.05 ms */
#define POLLING_INACTIVITY	40	/* cycles before switch to intr mode */

#define LOW_WATERMARK		2
#define HIGH_WATERMARK		4

static int msm_bam_dmux_debug_enable;
module_param_named(debug_enable, msm_bam_dmux_debug_enable,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(DEBUG)
static uint32_t bam_dmux_read_cnt;
static uint32_t bam_dmux_write_cnt;
static uint32_t bam_dmux_write_cpy_cnt;
static uint32_t bam_dmux_write_cpy_bytes;
static uint32_t bam_dmux_tx_sps_failure_cnt;

#define DBG(x...) do {		                 \
		if (msm_bam_dmux_debug_enable)  \
			pr_debug(x);	         \
	} while (0)

#define DBG_INC_READ_CNT(x) do {	                               \
		bam_dmux_read_cnt += (x);                             \
		if (msm_bam_dmux_debug_enable)                        \
			pr_debug("%s: total read bytes %u\n",          \
				 __func__, bam_dmux_read_cnt);        \
	} while (0)

#define DBG_INC_WRITE_CNT(x)  do {	                               \
		bam_dmux_write_cnt += (x);                            \
		if (msm_bam_dmux_debug_enable)                        \
			pr_debug("%s: total written bytes %u\n",       \
				 __func__, bam_dmux_write_cnt);       \
	} while (0)

#define DBG_INC_WRITE_CPY(x)  do {	                                     \
		bam_dmux_write_cpy_bytes += (x);                            \
		bam_dmux_write_cpy_cnt++;                                   \
		if (msm_bam_dmux_debug_enable)                              \
			pr_debug("%s: total write copy cnt %u, bytes %u\n",  \
				 __func__, bam_dmux_write_cpy_cnt,          \
				 bam_dmux_write_cpy_bytes);                 \
	} while (0)

#define DBG_INC_TX_SPS_FAILURE_CNT() do {	\
		bam_dmux_tx_sps_failure_cnt++;		\
} while (0)

#else
#define DBG(x...) do { } while (0)
#define DBG_INC_READ_CNT(x...) do { } while (0)
#define DBG_INC_WRITE_CNT(x...) do { } while (0)
#define DBG_INC_WRITE_CPY(x...) do { } while (0)
#define DBG_INC_TX_SPS_FAILURE_CNT() do { } while (0)
#endif

struct bam_ch_info {
	uint32_t status;
	void (*notify)(void *, int, unsigned long);
	void *priv;
	spinlock_t lock;
	struct platform_device *pdev;
	char name[BAM_DMUX_CH_NAME_MAX_LEN];
	int num_tx_pkts;
	int use_wm;
};

struct tx_pkt_info {
	struct sk_buff *skb;
	dma_addr_t dma_address;
	char is_cmd;
	uint32_t len;
	struct work_struct work;
	struct list_head list_node;
};

struct rx_pkt_info {
	struct sk_buff *skb;
	dma_addr_t dma_address;
	struct work_struct work;
	struct list_head list_node;
};

#define A2_NUM_PIPES		6
#define A2_SUMMING_THRESHOLD	4096
#define A2_DEFAULT_DESCRIPTORS	32
#define A2_PHYS_BASE		0x124C2000
#define A2_PHYS_SIZE		0x2000
#define BUFFER_SIZE		2048
#define NUM_BUFFERS		32
static struct sps_bam_props a2_props;
static u32 a2_device_handle;
static struct sps_pipe *bam_tx_pipe;
static struct sps_pipe *bam_rx_pipe;
static struct sps_connect tx_connection;
static struct sps_connect rx_connection;
static struct sps_mem_buffer tx_desc_mem_buf;
static struct sps_mem_buffer rx_desc_mem_buf;
static struct sps_register_event tx_register_event;
static struct sps_register_event rx_register_event;

static struct bam_ch_info bam_ch[BAM_DMUX_NUM_CHANNELS];
static int bam_mux_initialized;

static int polling_mode;

static LIST_HEAD(bam_rx_pool);
static DEFINE_MUTEX(bam_rx_pool_mutexlock);
static LIST_HEAD(bam_tx_pool);
static DEFINE_SPINLOCK(bam_tx_pool_spinlock);

struct bam_mux_hdr {
	uint16_t magic_num;
	uint8_t reserved;
	uint8_t cmd;
	uint8_t pad_len;
	uint8_t ch_id;
	uint16_t pkt_len;
};

static void notify_all(int event, unsigned long data);
static void bam_mux_write_done(struct work_struct *work);
static void handle_bam_mux_cmd(struct work_struct *work);
static void rx_timer_work_func(struct work_struct *work);

static DECLARE_WORK(rx_timer_work, rx_timer_work_func);

static struct workqueue_struct *bam_mux_rx_workqueue;
static struct workqueue_struct *bam_mux_tx_workqueue;

/* A2 power collaspe */
#define UL_TIMEOUT_DELAY 1000	/* in ms */
static void toggle_apps_ack(void);
static void reconnect_to_bam(void);
static void disconnect_to_bam(void);
static void ul_wakeup(void);
static void ul_timeout(struct work_struct *work);
static void vote_dfab(void);
static void unvote_dfab(void);
static void kickoff_ul_wakeup_func(struct work_struct *work);

static int bam_is_connected;
static DEFINE_MUTEX(wakeup_lock);
static struct completion ul_wakeup_ack_completion;
static struct completion bam_connection_completion;
static struct delayed_work ul_timeout_work;
static int ul_packet_written;
static struct clk *dfab_clk;
static DEFINE_RWLOCK(ul_wakeup_lock);
static DECLARE_WORK(kickoff_ul_wakeup, kickoff_ul_wakeup_func);
static int bam_connection_is_active;
static int wait_for_ack;
static struct wake_lock bam_wakelock;
/* End A2 power collaspe */

/* subsystem restart */
static int restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data);

static struct notifier_block restart_notifier = {
	.notifier_call = restart_notifier_cb,
};
static int in_global_reset;
/* end subsystem restart */

#define bam_ch_is_open(x)						\
	(bam_ch[(x)].status == (BAM_CH_LOCAL_OPEN | BAM_CH_REMOTE_OPEN))

#define bam_ch_is_local_open(x)			\
	(bam_ch[(x)].status & BAM_CH_LOCAL_OPEN)

#define bam_ch_is_remote_open(x)			\
	(bam_ch[(x)].status & BAM_CH_REMOTE_OPEN)

#define bam_ch_is_in_reset(x)			\
	(bam_ch[(x)].status & BAM_CH_IN_RESET)

static void queue_rx(void)
{
	void *ptr;
	struct rx_pkt_info *info;

	if (in_global_reset)
		return;

	info = kmalloc(sizeof(struct rx_pkt_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: unable to alloc rx_pkt_info\n", __func__);
		return;
	}

	INIT_WORK(&info->work, handle_bam_mux_cmd);

	info->skb = __dev_alloc_skb(BUFFER_SIZE, GFP_KERNEL);
	if (info->skb == NULL) {
		pr_err("%s: unable to alloc skb\n", __func__);
		kfree(info);
		return;
	}
	ptr = skb_put(info->skb, BUFFER_SIZE);

	mutex_lock(&bam_rx_pool_mutexlock);
	list_add_tail(&info->list_node, &bam_rx_pool);
	mutex_unlock(&bam_rx_pool_mutexlock);

	/* need a way to handle error case */
	info->dma_address = dma_map_single(NULL, ptr, BUFFER_SIZE,
						DMA_FROM_DEVICE);
	sps_transfer_one(bam_rx_pipe, info->dma_address,
				BUFFER_SIZE, info,
				SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOT);
}

static void bam_mux_process_data(struct sk_buff *rx_skb)
{
	unsigned long flags;
	struct bam_mux_hdr *rx_hdr;
	unsigned long event_data;

	rx_hdr = (struct bam_mux_hdr *)rx_skb->data;

	rx_skb->data = (unsigned char *)(rx_hdr + 1);
	rx_skb->tail = rx_skb->data + rx_hdr->pkt_len;
	rx_skb->len = rx_hdr->pkt_len;
	rx_skb->truesize = rx_hdr->pkt_len + sizeof(struct sk_buff);

	event_data = (unsigned long)(rx_skb);

	spin_lock_irqsave(&bam_ch[rx_hdr->ch_id].lock, flags);
	if (bam_ch[rx_hdr->ch_id].notify)
		bam_ch[rx_hdr->ch_id].notify(
			bam_ch[rx_hdr->ch_id].priv, BAM_DMUX_RECEIVE,
							event_data);
	else
		dev_kfree_skb_any(rx_skb);
	spin_unlock_irqrestore(&bam_ch[rx_hdr->ch_id].lock, flags);

	queue_rx();
}

static void handle_bam_mux_cmd(struct work_struct *work)
{
	unsigned long flags;
	struct bam_mux_hdr *rx_hdr;
	struct rx_pkt_info *info;
	struct sk_buff *rx_skb;
	int ret;

	info = container_of(work, struct rx_pkt_info, work);
	rx_skb = info->skb;
	dma_unmap_single(NULL, info->dma_address, BUFFER_SIZE, DMA_FROM_DEVICE);
	kfree(info);

	rx_hdr = (struct bam_mux_hdr *)rx_skb->data;

	DBG_INC_READ_CNT(sizeof(struct bam_mux_hdr));
	DBG("%s: magic %x reserved %d cmd %d pad %d ch %d len %d\n", __func__,
			rx_hdr->magic_num, rx_hdr->reserved, rx_hdr->cmd,
			rx_hdr->pad_len, rx_hdr->ch_id, rx_hdr->pkt_len);
	if (rx_hdr->magic_num != BAM_MUX_HDR_MAGIC_NO) {
		pr_err("%s: dropping invalid hdr. magic %x reserved %d cmd %d"
			" pad %d ch %d len %d\n", __func__,
			rx_hdr->magic_num, rx_hdr->reserved, rx_hdr->cmd,
			rx_hdr->pad_len, rx_hdr->ch_id, rx_hdr->pkt_len);
		dev_kfree_skb_any(rx_skb);
		queue_rx();
		return;
	}

	if (rx_hdr->ch_id >= BAM_DMUX_NUM_CHANNELS) {
		pr_err("%s: dropping invalid LCID %d reserved %d cmd %d"
			" pad %d ch %d len %d\n", __func__,
			rx_hdr->ch_id, rx_hdr->reserved, rx_hdr->cmd,
			rx_hdr->pad_len, rx_hdr->ch_id, rx_hdr->pkt_len);
		dev_kfree_skb_any(rx_skb);
		queue_rx();
		return;
	}

	switch (rx_hdr->cmd) {
	case BAM_MUX_HDR_CMD_DATA:
		DBG_INC_READ_CNT(rx_hdr->pkt_len);
		bam_mux_process_data(rx_skb);
		break;
	case BAM_MUX_HDR_CMD_OPEN:
		spin_lock_irqsave(&bam_ch[rx_hdr->ch_id].lock, flags);
		bam_ch[rx_hdr->ch_id].status |= BAM_CH_REMOTE_OPEN;
		bam_ch[rx_hdr->ch_id].num_tx_pkts = 0;
		spin_unlock_irqrestore(&bam_ch[rx_hdr->ch_id].lock, flags);
		queue_rx();
		ret = platform_device_add(bam_ch[rx_hdr->ch_id].pdev);
		if (ret)
			pr_err("%s: platform_device_add() error: %d\n",
					__func__, ret);
		dev_kfree_skb_any(rx_skb);
		break;
	case BAM_MUX_HDR_CMD_CLOSE:
		/* probably should drop pending write */
		spin_lock_irqsave(&bam_ch[rx_hdr->ch_id].lock, flags);
		bam_ch[rx_hdr->ch_id].status &= ~BAM_CH_REMOTE_OPEN;
		spin_unlock_irqrestore(&bam_ch[rx_hdr->ch_id].lock, flags);
		queue_rx();
		platform_device_unregister(bam_ch[rx_hdr->ch_id].pdev);
		bam_ch[rx_hdr->ch_id].pdev =
			platform_device_alloc(bam_ch[rx_hdr->ch_id].name, 2);
		if (!bam_ch[rx_hdr->ch_id].pdev)
			pr_err("%s: platform_device_alloc failed\n", __func__);
		dev_kfree_skb_any(rx_skb);
		break;
	default:
		pr_err("%s: dropping invalid hdr. magic %x reserved %d cmd %d"
			" pad %d ch %d len %d\n", __func__,
			rx_hdr->magic_num, rx_hdr->reserved, rx_hdr->cmd,
			rx_hdr->pad_len, rx_hdr->ch_id, rx_hdr->pkt_len);
		dev_kfree_skb_any(rx_skb);
		queue_rx();
		return;
	}
}

static int bam_mux_write_cmd(void *data, uint32_t len)
{
	int rc;
	struct tx_pkt_info *pkt;
	dma_addr_t dma_address;
	unsigned long flags;

	pkt = kmalloc(sizeof(struct tx_pkt_info), GFP_ATOMIC);
	if (pkt == NULL) {
		pr_err("%s: mem alloc for tx_pkt_info failed\n", __func__);
		rc = -ENOMEM;
		return rc;
	}

	dma_address = dma_map_single(NULL, data, len,
					DMA_TO_DEVICE);
	if (!dma_address) {
		pr_err("%s: dma_map_single() failed\n", __func__);
		kfree(pkt);
		rc = -ENOMEM;
		return rc;
	}
	pkt->skb = (struct sk_buff *)(data);
	pkt->len = len;
	pkt->dma_address = dma_address;
	pkt->is_cmd = 1;
	INIT_WORK(&pkt->work, bam_mux_write_done);
	spin_lock_irqsave(&bam_tx_pool_spinlock, flags);
	list_add_tail(&pkt->list_node, &bam_tx_pool);
	spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);
	rc = sps_transfer_one(bam_tx_pipe, dma_address, len,
				pkt, SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOT);
	if (rc) {
		DBG("%s sps_transfer_one failed rc=%d\n", __func__, rc);
		spin_lock_irqsave(&bam_tx_pool_spinlock, flags);
		list_del(&pkt->list_node);
		DBG_INC_TX_SPS_FAILURE_CNT();
		spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);
		kfree(pkt);
	}

	ul_packet_written = 1;
	return rc;
}

static void bam_mux_write_done(struct work_struct *work)
{
	struct sk_buff *skb;
	struct bam_mux_hdr *hdr;
	struct tx_pkt_info *info;
	struct tx_pkt_info *info_expected;
	unsigned long event_data;
	unsigned long flags;

	if (in_global_reset)
		return;

	info = container_of(work, struct tx_pkt_info, work);

	spin_lock_irqsave(&bam_tx_pool_spinlock, flags);
	info_expected = list_first_entry(&bam_tx_pool,
			struct tx_pkt_info, list_node);
	if (unlikely(info != info_expected)) {
		struct list_head *node;

		pr_err("%s: bam_tx_pool mismatch .next=%p, list_node=%p\n",
				__func__, bam_tx_pool.next, &info->list_node);
		list_for_each(node, &bam_tx_pool)
			pr_err("%s: node=%p\n", __func__, node);
		spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);
		BUG();
	}
	list_del(&info->list_node);
	spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);

	if (info->is_cmd) {
		kfree(info->skb);
		kfree(info);
		return;
	}
	skb = info->skb;
	kfree(info);
	hdr = (struct bam_mux_hdr *)skb->data;
	DBG_INC_WRITE_CNT(skb->data_len);
	event_data = (unsigned long)(skb);
	spin_lock_irqsave(&bam_ch[hdr->ch_id].lock, flags);
	bam_ch[hdr->ch_id].num_tx_pkts--;
	spin_unlock_irqrestore(&bam_ch[hdr->ch_id].lock, flags);
	if (bam_ch[hdr->ch_id].notify)
		bam_ch[hdr->ch_id].notify(
			bam_ch[hdr->ch_id].priv, BAM_DMUX_WRITE_DONE,
							event_data);
	else
		dev_kfree_skb_any(skb);
}

int msm_bam_dmux_write(uint32_t id, struct sk_buff *skb)
{
	int rc = 0;
	struct bam_mux_hdr *hdr;
	unsigned long flags;
	struct sk_buff *new_skb = NULL;
	dma_addr_t dma_address;
	struct tx_pkt_info *pkt;

	if (id >= BAM_DMUX_NUM_CHANNELS)
		return -EINVAL;
	if (!skb)
		return -EINVAL;
	if (!bam_mux_initialized)
		return -ENODEV;

	DBG("%s: writing to ch %d len %d\n", __func__, id, skb->len);
	spin_lock_irqsave(&bam_ch[id].lock, flags);
	if (!bam_ch_is_open(id)) {
		spin_unlock_irqrestore(&bam_ch[id].lock, flags);
		pr_err("%s: port not open: %d\n", __func__, bam_ch[id].status);
		return -ENODEV;
	}

	if (bam_ch[id].use_wm &&
	    (bam_ch[id].num_tx_pkts >= HIGH_WATERMARK)) {
		spin_unlock_irqrestore(&bam_ch[id].lock, flags);
		pr_err("%s: watermark exceeded: %d\n", __func__, id);
		return -EAGAIN;
	}
	spin_unlock_irqrestore(&bam_ch[id].lock, flags);

	read_lock(&ul_wakeup_lock);
	if (!bam_is_connected) {
		read_unlock(&ul_wakeup_lock);
		ul_wakeup();
		read_lock(&ul_wakeup_lock);
		notify_all(BAM_DMUX_UL_CONNECTED, (unsigned long)(NULL));
	}

	/* if skb do not have any tailroom for padding,
	   copy the skb into a new expanded skb */
	if ((skb->len & 0x3) && (skb_tailroom(skb) < (4 - (skb->len & 0x3)))) {
		/* revisit, probably dev_alloc_skb and memcpy is effecient */
		new_skb = skb_copy_expand(skb, skb_headroom(skb),
					  4 - (skb->len & 0x3), GFP_ATOMIC);
		if (new_skb == NULL) {
			pr_err("%s: cannot allocate skb\n", __func__);
			goto write_fail;
		}
		dev_kfree_skb_any(skb);
		skb = new_skb;
		DBG_INC_WRITE_CPY(skb->len);
	}

	hdr = (struct bam_mux_hdr *)skb_push(skb, sizeof(struct bam_mux_hdr));

	/* caller should allocate for hdr and padding
	   hdr is fine, padding is tricky */
	hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
	hdr->cmd = BAM_MUX_HDR_CMD_DATA;
	hdr->reserved = 0;
	hdr->ch_id = id;
	hdr->pkt_len = skb->len - sizeof(struct bam_mux_hdr);
	if (skb->len & 0x3)
		skb_put(skb, 4 - (skb->len & 0x3));

	hdr->pad_len = skb->len - (sizeof(struct bam_mux_hdr) + hdr->pkt_len);

	DBG("%s: data %p, tail %p skb len %d pkt len %d pad len %d\n",
	    __func__, skb->data, skb->tail, skb->len,
	    hdr->pkt_len, hdr->pad_len);

	pkt = kmalloc(sizeof(struct tx_pkt_info), GFP_ATOMIC);
	if (pkt == NULL) {
		pr_err("%s: mem alloc for tx_pkt_info failed\n", __func__);
		goto write_fail2;
	}

	dma_address = dma_map_single(NULL, skb->data, skb->len,
					DMA_TO_DEVICE);
	if (!dma_address) {
		pr_err("%s: dma_map_single() failed\n", __func__);
		goto write_fail3;
	}
	pkt->skb = skb;
	pkt->dma_address = dma_address;
	pkt->is_cmd = 0;
	INIT_WORK(&pkt->work, bam_mux_write_done);
	spin_lock_irqsave(&bam_tx_pool_spinlock, flags);
	list_add_tail(&pkt->list_node, &bam_tx_pool);
	spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);
	rc = sps_transfer_one(bam_tx_pipe, dma_address, skb->len,
				pkt, SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOT);
	if (rc) {
		DBG("%s sps_transfer_one failed rc=%d\n", __func__, rc);
		spin_lock_irqsave(&bam_tx_pool_spinlock, flags);
		list_del(&pkt->list_node);
		DBG_INC_TX_SPS_FAILURE_CNT();
		spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);
		kfree(pkt);
		if (new_skb)
			dev_kfree_skb_any(new_skb);
	} else {
		spin_lock_irqsave(&bam_ch[id].lock, flags);
		bam_ch[id].num_tx_pkts++;
		spin_unlock_irqrestore(&bam_ch[id].lock, flags);
	}
	ul_packet_written = 1;
	read_unlock(&ul_wakeup_lock);
	return rc;

write_fail3:
	kfree(pkt);
write_fail2:
	if (new_skb)
		dev_kfree_skb_any(new_skb);
write_fail:
	read_unlock(&ul_wakeup_lock);
	return -ENOMEM;
}

int msm_bam_dmux_open(uint32_t id, void *priv,
			void (*notify)(void *, int, unsigned long))
{
	struct bam_mux_hdr *hdr;
	unsigned long flags;
	int rc = 0;

	DBG("%s: opening ch %d\n", __func__, id);
	if (!bam_mux_initialized) {
		DBG("%s: not inititialized\n", __func__);
		return -ENODEV;
	}
	if (id >= BAM_DMUX_NUM_CHANNELS) {
		pr_err("%s: invalid channel id %d\n", __func__, id);
		return -EINVAL;
	}
	if (notify == NULL) {
		pr_err("%s: notify function is NULL\n", __func__);
		return -EINVAL;
	}

	hdr = kmalloc(sizeof(struct bam_mux_hdr), GFP_KERNEL);
	if (hdr == NULL) {
		pr_err("%s: hdr kmalloc failed. ch: %d\n", __func__, id);
		return -ENOMEM;
	}
	spin_lock_irqsave(&bam_ch[id].lock, flags);
	if (bam_ch_is_open(id)) {
		DBG("%s: Already opened %d\n", __func__, id);
		spin_unlock_irqrestore(&bam_ch[id].lock, flags);
		kfree(hdr);
		goto open_done;
	}
	if (!bam_ch_is_remote_open(id)) {
		DBG("%s: Remote not open; ch: %d\n", __func__, id);
		spin_unlock_irqrestore(&bam_ch[id].lock, flags);
		kfree(hdr);
		return -ENODEV;
	}

	bam_ch[id].notify = notify;
	bam_ch[id].priv = priv;
	bam_ch[id].status |= BAM_CH_LOCAL_OPEN;
	bam_ch[id].num_tx_pkts = 0;
	bam_ch[id].use_wm = 0;
	spin_unlock_irqrestore(&bam_ch[id].lock, flags);

	read_lock(&ul_wakeup_lock);
	if (!bam_is_connected) {
		read_unlock(&ul_wakeup_lock);
		ul_wakeup();
		read_lock(&ul_wakeup_lock);
		notify_all(BAM_DMUX_UL_CONNECTED, (unsigned long)(NULL));
	}

	hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
	hdr->cmd = BAM_MUX_HDR_CMD_OPEN;
	hdr->reserved = 0;
	hdr->ch_id = id;
	hdr->pkt_len = 0;
	hdr->pad_len = 0;

	rc = bam_mux_write_cmd((void *)hdr, sizeof(struct bam_mux_hdr));
	read_unlock(&ul_wakeup_lock);

open_done:
	DBG("%s: opened ch %d\n", __func__, id);
	return rc;
}

int msm_bam_dmux_close(uint32_t id)
{
	struct bam_mux_hdr *hdr;
	unsigned long flags;
	int rc;

	if (id >= BAM_DMUX_NUM_CHANNELS)
		return -EINVAL;
	DBG("%s: closing ch %d\n", __func__, id);
	if (!bam_mux_initialized)
		return -ENODEV;

	read_lock(&ul_wakeup_lock);
	if (!bam_is_connected && !bam_ch_is_in_reset(id)) {
		read_unlock(&ul_wakeup_lock);
		ul_wakeup();
		read_lock(&ul_wakeup_lock);
		notify_all(BAM_DMUX_UL_CONNECTED, (unsigned long)(NULL));
	}

	spin_lock_irqsave(&bam_ch[id].lock, flags);
	bam_ch[id].notify = NULL;
	bam_ch[id].priv = NULL;
	bam_ch[id].status &= ~BAM_CH_LOCAL_OPEN;
	spin_unlock_irqrestore(&bam_ch[id].lock, flags);

	if (bam_ch_is_in_reset(id)) {
		read_unlock(&ul_wakeup_lock);
		bam_ch[id].status &= ~BAM_CH_IN_RESET;
		return 0;
	}

	hdr = kmalloc(sizeof(struct bam_mux_hdr), GFP_ATOMIC);
	if (hdr == NULL) {
		pr_err("%s: hdr kmalloc failed. ch: %d\n", __func__, id);
		read_unlock(&ul_wakeup_lock);
		return -ENOMEM;
	}
	hdr->magic_num = BAM_MUX_HDR_MAGIC_NO;
	hdr->cmd = BAM_MUX_HDR_CMD_CLOSE;
	hdr->reserved = 0;
	hdr->ch_id = id;
	hdr->pkt_len = 0;
	hdr->pad_len = 0;

	rc = bam_mux_write_cmd((void *)hdr, sizeof(struct bam_mux_hdr));
	read_unlock(&ul_wakeup_lock);

	DBG("%s: closed ch %d\n", __func__, id);
	return rc;
}

int msm_bam_dmux_is_ch_full(uint32_t id)
{
	unsigned long flags;
	int ret;

	if (id >= BAM_DMUX_NUM_CHANNELS)
		return -EINVAL;

	spin_lock_irqsave(&bam_ch[id].lock, flags);
	bam_ch[id].use_wm = 1;
	ret = bam_ch[id].num_tx_pkts >= HIGH_WATERMARK;
	DBG("%s: ch %d num tx pkts=%d, HWM=%d\n", __func__,
	     id, bam_ch[id].num_tx_pkts, ret);
	if (!bam_ch_is_local_open(id)) {
		ret = -ENODEV;
		pr_err("%s: port not open: %d\n", __func__, bam_ch[id].status);
	}
	spin_unlock_irqrestore(&bam_ch[id].lock, flags);

	return ret;
}

int msm_bam_dmux_is_ch_low(uint32_t id)
{
	int ret;

	if (id >= BAM_DMUX_NUM_CHANNELS)
		return -EINVAL;

	bam_ch[id].use_wm = 1;
	ret = bam_ch[id].num_tx_pkts <= LOW_WATERMARK;
	DBG("%s: ch %d num tx pkts=%d, LWM=%d\n", __func__,
	     id, bam_ch[id].num_tx_pkts, ret);
	if (!bam_ch_is_local_open(id)) {
		ret = -ENODEV;
		pr_err("%s: port not open: %d\n", __func__, bam_ch[id].status);
	}

	return ret;
}

static void rx_timer_work_func(struct work_struct *work)
{
	struct sps_iovec iov;
	struct list_head *node;
	struct rx_pkt_info *info;
	int inactive_cycles = 0;
	int ret;
	struct sps_connect cur_rx_conn;

	while (1) { /* timer loop */
		++inactive_cycles;
		while (1) { /* deplete queue loop */
			if (in_global_reset)
				return;
			sps_get_iovec(bam_rx_pipe, &iov);
			if (iov.addr == 0)
				break;
			inactive_cycles = 0;
			mutex_lock(&bam_rx_pool_mutexlock);
			node = bam_rx_pool.next;
			list_del(node);
			mutex_unlock(&bam_rx_pool_mutexlock);
			info = container_of(node, struct rx_pkt_info,
							list_node);
			handle_bam_mux_cmd(&info->work);
		}

		if (inactive_cycles == POLLING_INACTIVITY) {
			/*
			 * attempt to enable interrupts in this pipe
			 * if enabling interrupts fails, continue polling
			 */
			ret = sps_get_config(bam_rx_pipe, &cur_rx_conn);
			if (ret) {
				pr_err("%s: sps_get_config() failed, interrupts"
						" not enabled\n", __func__);
				queue_work(bam_mux_rx_workqueue,
						&rx_timer_work);
				return;
			} else {
				rx_register_event.options = SPS_O_EOT;
				/* should check return value */
				sps_register_event(bam_rx_pipe,
							&rx_register_event);
				cur_rx_conn.options = SPS_O_AUTO_ENABLE |
					SPS_O_EOT | SPS_O_ACK_TRANSFERS;
				ret = sps_set_config(bam_rx_pipe, &cur_rx_conn);
				if (ret) {
					pr_err("%s: sps_set_config() failed, "
						"interrupts not enabled\n",
						__func__);
					queue_work(bam_mux_rx_workqueue,
							&rx_timer_work);
					return;
				}
				polling_mode = 0;
			}
			if (in_global_reset)
				return;
			/* handle race condition - missed packet? */
			sps_get_iovec(bam_rx_pipe, &iov);
			if (iov.addr == 0)
				return;
			inactive_cycles = 0;
			mutex_lock(&bam_rx_pool_mutexlock);
			node = bam_rx_pool.next;
			list_del(node);
			mutex_unlock(&bam_rx_pool_mutexlock);
			info = container_of(node, struct rx_pkt_info,
							list_node);
			handle_bam_mux_cmd(&info->work);
			return;
		}

		usleep_range(POLLING_MIN_SLEEP, POLLING_MAX_SLEEP);
	}
}

static void bam_mux_tx_notify(struct sps_event_notify *notify)
{
	struct tx_pkt_info *pkt;

	DBG("%s: event %d notified\n", __func__, notify->event_id);

	if (in_global_reset)
		return;

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		pkt = notify->data.transfer.user;
		if (!pkt->is_cmd)
			dma_unmap_single(NULL, pkt->dma_address,
						pkt->skb->len,
						DMA_TO_DEVICE);
		else
			dma_unmap_single(NULL, pkt->dma_address,
						pkt->len,
						DMA_TO_DEVICE);
		queue_work(bam_mux_tx_workqueue, &pkt->work);
		break;
	default:
		pr_err("%s: recieved unexpected event id %d\n", __func__,
			notify->event_id);
	}
}

static void bam_mux_rx_notify(struct sps_event_notify *notify)
{
	int ret;
	struct sps_connect cur_rx_conn;

	DBG("%s: event %d notified\n", __func__, notify->event_id);

	if (in_global_reset)
		return;

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		/* attempt to disable interrupts in this pipe */
		if (!polling_mode) {
			ret = sps_get_config(bam_rx_pipe, &cur_rx_conn);
			if (ret) {
				pr_err("%s: sps_get_config() failed, interrupts"
					" not disabled\n", __func__);
				break;
			}
			cur_rx_conn.options = SPS_O_AUTO_ENABLE |
				SPS_O_ACK_TRANSFERS | SPS_O_POLL;
			ret = sps_set_config(bam_rx_pipe, &cur_rx_conn);
			if (ret) {
				pr_err("%s: sps_set_config() failed, interrupts"
					" not disabled\n", __func__);
				break;
			}
			polling_mode = 1;
			queue_work(bam_mux_rx_workqueue, &rx_timer_work);
		}
		break;
	default:
		pr_err("%s: recieved unexpected event id %d\n", __func__,
			notify->event_id);
	}
}

#ifdef CONFIG_DEBUG_FS

static int debug_tbl(char *buf, int max)
{
	int i = 0;
	int j;

	for (j = 0; j < BAM_DMUX_NUM_CHANNELS; ++j) {
		i += scnprintf(buf + i, max - i,
			"ch%02d  local open=%s  remote open=%s\n",
			j, bam_ch_is_local_open(j) ? "Y" : "N",
			bam_ch_is_remote_open(j) ? "Y" : "N");
	}

	return i;
}

static int debug_ul_pkt_cnt(char *buf, int max)
{
	struct list_head *p;
	unsigned long flags;
	int n = 0;

	spin_lock_irqsave(&bam_tx_pool_spinlock, flags);
	__list_for_each(p, &bam_tx_pool) {
		++n;
	}
	spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);

	return scnprintf(buf, max, "Number of UL packets in flight: %d\n", n);
}

static int debug_stats(char *buf, int max)
{
	int i = 0;

	i += scnprintf(buf + i, max - i,
			"skb copy cnt:    %u\n"
			"skb copy bytes:  %u\n"
			"sps tx failures: %u\n",
			bam_dmux_write_cpy_cnt,
			bam_dmux_write_cpy_bytes,
			bam_dmux_tx_sps_failure_cnt
			);

	return i;
}

#define DEBUG_BUFMAX 4096
static char debug_buffer[DEBUG_BUFMAX];

static ssize_t debug_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	int (*fill)(char *buf, int max) = file->private_data;
	int bsize = fill(debug_buffer, DEBUG_BUFMAX);
	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
				struct dentry *dent,
				int (*fill)(char *buf, int max))
{
	debugfs_create_file(name, mode, dent, fill, &debug_ops);
}

#endif

static void notify_all(int event, unsigned long data)
{
	int i;

	for (i = 0; i < BAM_DMUX_NUM_CHANNELS; ++i) {
		if (bam_ch_is_open(i))
			bam_ch[i].notify(bam_ch[i].priv, event, data);
	}
}

static void kickoff_ul_wakeup_func(struct work_struct *work)
{
	read_lock(&ul_wakeup_lock);
	if (!bam_is_connected) {
		read_unlock(&ul_wakeup_lock);
		ul_wakeup();
		read_lock(&ul_wakeup_lock);
		ul_packet_written = 1;
		notify_all(BAM_DMUX_UL_CONNECTED, (unsigned long)(NULL));
	}
	read_unlock(&ul_wakeup_lock);
}

void msm_bam_dmux_kickoff_ul_wakeup(void)
{
	queue_work(bam_mux_tx_workqueue, &kickoff_ul_wakeup);
}

static void ul_timeout(struct work_struct *work)
{
	unsigned long flags;
	int ret;

	if (in_global_reset)
		return;
	ret = write_trylock_irqsave(&ul_wakeup_lock, flags);
	if (!ret) { /* failed to grab lock, reschedule and bail */
		schedule_delayed_work(&ul_timeout_work,
				msecs_to_jiffies(UL_TIMEOUT_DELAY));
		return;
	}
	if (ul_packet_written) {
		pr_info("%s: packet written\n", __func__);
		ul_packet_written = 0;
		schedule_delayed_work(&ul_timeout_work,
				msecs_to_jiffies(UL_TIMEOUT_DELAY));
	} else {
		pr_info("%s: powerdown\n", __func__);
		wait_for_ack = 1;
		INIT_COMPLETION(ul_wakeup_ack_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_A2_POWER_CONTROL, 0);
		bam_is_connected = 0;
		notify_all(BAM_DMUX_UL_DISCONNECTED, (unsigned long)(NULL));
	}
	write_unlock_irqrestore(&ul_wakeup_lock, flags);
}
static void ul_wakeup(void)
{
	int ret;

	mutex_lock(&wakeup_lock);
	if (bam_is_connected) { /* bam got connected before lock grabbed */
		mutex_unlock(&wakeup_lock);
		return;
	}
	pr_info("%s\n", __func__);
	/*
	 * must wait for the previous power down request to have been acked
	 * chances are it already came in and this will just fall through
	 * instead of waiting
	 */
	if (wait_for_ack) {
		ret = wait_for_completion_interruptible_timeout(
					&ul_wakeup_ack_completion, HZ);
		BUG_ON(ret == 0);
	}
	INIT_COMPLETION(ul_wakeup_ack_completion);
	smsm_change_state(SMSM_APPS_STATE, 0, SMSM_A2_POWER_CONTROL);
	ret = wait_for_completion_interruptible_timeout(
						&ul_wakeup_ack_completion, HZ);
	BUG_ON(ret == 0);
	ret = wait_for_completion_interruptible_timeout(
						&bam_connection_completion, HZ);
	BUG_ON(ret == 0);

	bam_is_connected = 1;
	schedule_delayed_work(&ul_timeout_work,
				msecs_to_jiffies(UL_TIMEOUT_DELAY));
	mutex_unlock(&wakeup_lock);
}

static void reconnect_to_bam(void)
{
	int i;

	in_global_reset = 0;
	vote_dfab();
	i = sps_device_reset(a2_device_handle);
	if (i)
		pr_err("%s: device reset failed rc = %d\n", __func__, i);
	i = sps_connect(bam_tx_pipe, &tx_connection);
	if (i)
		pr_err("%s: tx connection failed rc = %d\n", __func__, i);
	i = sps_connect(bam_rx_pipe, &rx_connection);
	if (i)
		pr_err("%s: rx connection failed rc = %d\n", __func__, i);
	i = sps_register_event(bam_tx_pipe, &tx_register_event);
	if (i)
		pr_err("%s: tx event reg failed rc = %d\n", __func__, i);
	i = sps_register_event(bam_rx_pipe, &rx_register_event);
	if (i)
		pr_err("%s: rx event reg failed rc = %d\n", __func__, i);
	for (i = 0; i < NUM_BUFFERS; ++i)
		queue_rx();
	toggle_apps_ack();
	bam_connection_is_active = 1;
	complete_all(&bam_connection_completion);
}

static void disconnect_to_bam(void)
{
	struct list_head *node;
	struct rx_pkt_info *info;

	bam_connection_is_active = 0;
	INIT_COMPLETION(bam_connection_completion);
	sps_disconnect(bam_tx_pipe);
	sps_disconnect(bam_rx_pipe);
	unvote_dfab();
	__memzero(rx_desc_mem_buf.base, rx_desc_mem_buf.size);
	__memzero(tx_desc_mem_buf.base, tx_desc_mem_buf.size);
	while (!list_empty(&bam_rx_pool)) {
		node = bam_rx_pool.next;
		list_del(node);
		info = container_of(node, struct rx_pkt_info, list_node);
		dma_unmap_single(NULL, info->dma_address, BUFFER_SIZE,
							DMA_FROM_DEVICE);
		dev_kfree_skb_any(info->skb);
		kfree(info);
	}
}

static void vote_dfab(void)
{
	int rc;

	rc = clk_enable(dfab_clk);
	if (rc)
		pr_err("bam_dmux vote for dfab failed rc = %d\n", rc);
}

static void unvote_dfab(void)
{
	clk_disable(dfab_clk);
}

static int restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data)
{
	int i;
	struct list_head *node;
	struct tx_pkt_info *info;
	int temp_remote_status;
	unsigned long flags;

	if (code != SUBSYS_AFTER_SHUTDOWN)
		return NOTIFY_DONE;

	in_global_reset = 1;
	for (i = 0; i < BAM_DMUX_NUM_CHANNELS; ++i) {
		temp_remote_status = bam_ch_is_remote_open(i);
		bam_ch[i].status &= ~BAM_CH_REMOTE_OPEN;
		bam_ch[i].num_tx_pkts = 0;
		if (bam_ch_is_local_open(i))
			bam_ch[i].status |= BAM_CH_IN_RESET;
		if (temp_remote_status) {
			platform_device_unregister(bam_ch[i].pdev);
			bam_ch[i].pdev = platform_device_alloc(
						bam_ch[i].name, 2);
		}
	}
	/*cleanup UL*/
	spin_lock_irqsave(&bam_tx_pool_spinlock, flags);
	while (!list_empty(&bam_tx_pool)) {
		node = bam_tx_pool.next;
		list_del(node);
		info = container_of(node, struct tx_pkt_info,
							list_node);
		if (!info->is_cmd) {
			dma_unmap_single(NULL, info->dma_address,
						info->skb->len,
						DMA_TO_DEVICE);
			dev_kfree_skb_any(info->skb);
		} else {
			dma_unmap_single(NULL, info->dma_address,
						info->len,
						DMA_TO_DEVICE);
			kfree(info->skb);
		}
		kfree(info);
	}
	spin_unlock_irqrestore(&bam_tx_pool_spinlock, flags);
	smsm_change_state(SMSM_APPS_STATE, SMSM_A2_POWER_CONTROL, 0);

	return NOTIFY_DONE;
}

static void bam_init(void)
{
	u32 h;
	dma_addr_t dma_addr;
	int ret;
	void *a2_virt_addr;
	int i;

	vote_dfab();
	/* init BAM */
	a2_virt_addr = ioremap_nocache(A2_PHYS_BASE, A2_PHYS_SIZE);
	if (!a2_virt_addr) {
		pr_err("%s: ioremap failed\n", __func__);
		ret = -ENOMEM;
		goto register_bam_failed;
	}
	a2_props.phys_addr = A2_PHYS_BASE;
	a2_props.virt_addr = a2_virt_addr;
	a2_props.virt_size = A2_PHYS_SIZE;
	a2_props.irq = A2_BAM_IRQ;
	a2_props.options = SPS_BAM_OPT_IRQ_WAKEUP;
	a2_props.num_pipes = A2_NUM_PIPES;
	a2_props.summing_threshold = A2_SUMMING_THRESHOLD;
	if (cpu_is_msm9615())
		a2_props.manage = SPS_BAM_MGR_DEVICE_REMOTE;
	/* need to free on tear down */
	ret = sps_register_bam_device(&a2_props, &h);
	if (ret < 0) {
		pr_err("%s: register bam error %d\n", __func__, ret);
		goto register_bam_failed;
	}
	a2_device_handle = h;

	bam_tx_pipe = sps_alloc_endpoint();
	if (bam_tx_pipe == NULL) {
		pr_err("%s: tx alloc endpoint failed\n", __func__);
		ret = -ENOMEM;
		goto register_bam_failed;
	}
	ret = sps_get_config(bam_tx_pipe, &tx_connection);
	if (ret) {
		pr_err("%s: tx get config failed %d\n", __func__, ret);
		goto tx_get_config_failed;
	}

	tx_connection.source = SPS_DEV_HANDLE_MEM;
	tx_connection.src_pipe_index = 0;
	tx_connection.destination = h;
	tx_connection.dest_pipe_index = 4;
	tx_connection.mode = SPS_MODE_DEST;
	tx_connection.options = SPS_O_AUTO_ENABLE | SPS_O_EOT;
	tx_desc_mem_buf.size = 0x800; /* 2k */
	tx_desc_mem_buf.base = dma_alloc_coherent(NULL, tx_desc_mem_buf.size,
							&dma_addr, 0);
	if (tx_desc_mem_buf.base == NULL) {
		pr_err("%s: tx memory alloc failed\n", __func__);
		ret = -ENOMEM;
		goto tx_mem_failed;
	}
	tx_desc_mem_buf.phys_base = dma_addr;
	memset(tx_desc_mem_buf.base, 0x0, tx_desc_mem_buf.size);
	tx_connection.desc = tx_desc_mem_buf;
	tx_connection.event_thresh = 0x10;

	ret = sps_connect(bam_tx_pipe, &tx_connection);
	if (ret < 0) {
		pr_err("%s: tx connect error %d\n", __func__, ret);
		goto tx_connect_failed;
	}

	bam_rx_pipe = sps_alloc_endpoint();
	if (bam_rx_pipe == NULL) {
		pr_err("%s: rx alloc endpoint failed\n", __func__);
		ret = -ENOMEM;
		goto tx_connect_failed;
	}
	ret = sps_get_config(bam_rx_pipe, &rx_connection);
	if (ret) {
		pr_err("%s: rx get config failed %d\n", __func__, ret);
		goto rx_get_config_failed;
	}

	rx_connection.source = h;
	rx_connection.src_pipe_index = 5;
	rx_connection.destination = SPS_DEV_HANDLE_MEM;
	rx_connection.dest_pipe_index = 1;
	rx_connection.mode = SPS_MODE_SRC;
	rx_connection.options = SPS_O_AUTO_ENABLE | SPS_O_EOT |
					SPS_O_ACK_TRANSFERS;
	rx_desc_mem_buf.size = 0x800; /* 2k */
	rx_desc_mem_buf.base = dma_alloc_coherent(NULL, rx_desc_mem_buf.size,
							&dma_addr, 0);
	if (rx_desc_mem_buf.base == NULL) {
		pr_err("%s: rx memory alloc failed\n", __func__);
		ret = -ENOMEM;
		goto rx_mem_failed;
	}
	rx_desc_mem_buf.phys_base = dma_addr;
	memset(rx_desc_mem_buf.base, 0x0, rx_desc_mem_buf.size);
	rx_connection.desc = rx_desc_mem_buf;
	rx_connection.event_thresh = 0x10;

	ret = sps_connect(bam_rx_pipe, &rx_connection);
	if (ret < 0) {
		pr_err("%s: rx connect error %d\n", __func__, ret);
		goto rx_connect_failed;
	}

	tx_register_event.options = SPS_O_EOT;
	tx_register_event.mode = SPS_TRIGGER_CALLBACK;
	tx_register_event.xfer_done = NULL;
	tx_register_event.callback = bam_mux_tx_notify;
	tx_register_event.user = NULL;
	ret = sps_register_event(bam_tx_pipe, &tx_register_event);
	if (ret < 0) {
		pr_err("%s: tx register event error %d\n", __func__, ret);
		goto rx_event_reg_failed;
	}

	rx_register_event.options = SPS_O_EOT;
	rx_register_event.mode = SPS_TRIGGER_CALLBACK;
	rx_register_event.xfer_done = NULL;
	rx_register_event.callback = bam_mux_rx_notify;
	rx_register_event.user = NULL;
	ret = sps_register_event(bam_rx_pipe, &rx_register_event);
	if (ret < 0) {
		pr_err("%s: tx register event error %d\n", __func__, ret);
		goto rx_event_reg_failed;
	}

	bam_mux_initialized = 1;
	for (i = 0; i < NUM_BUFFERS; ++i)
		queue_rx();
	toggle_apps_ack();
	bam_connection_is_active = 1;
	complete_all(&bam_connection_completion);
	return;

rx_event_reg_failed:
	sps_disconnect(bam_rx_pipe);
rx_connect_failed:
	dma_free_coherent(NULL, rx_desc_mem_buf.size, rx_desc_mem_buf.base,
				rx_desc_mem_buf.phys_base);
rx_mem_failed:
	sps_disconnect(bam_tx_pipe);
rx_get_config_failed:
	sps_free_endpoint(bam_rx_pipe);
tx_connect_failed:
	dma_free_coherent(NULL, tx_desc_mem_buf.size, tx_desc_mem_buf.base,
				tx_desc_mem_buf.phys_base);
tx_get_config_failed:
	sps_free_endpoint(bam_tx_pipe);
tx_mem_failed:
	sps_deregister_bam_device(h);
register_bam_failed:
	/*destroy_workqueue(bam_mux_workqueue);*/
	/*return ret;*/
	return;
}

static void toggle_apps_ack(void)
{
	static unsigned int clear_bit; /* 0 = set the bit, else clear bit */
	pr_info("%s: clear bit: %d\n", __func__, clear_bit);
	smsm_change_state(SMSM_APPS_STATE,
				clear_bit & SMSM_A2_POWER_CONTROL_ACK,
				~clear_bit & SMSM_A2_POWER_CONTROL_ACK);
	clear_bit = ~clear_bit;
}

static void bam_dmux_smsm_cb(void *priv, uint32_t old_state, uint32_t new_state)
{
	pr_info("%s: smsm activity 0x%08x -> 0x%08x\n", __func__, old_state,
							new_state);
	if (bam_mux_initialized && new_state & SMSM_A2_POWER_CONTROL) {
		pr_info("%s: reconnect\n", __func__);
		wake_lock(&bam_wakelock);
		reconnect_to_bam();
	} else if (bam_mux_initialized &&
					!(new_state & SMSM_A2_POWER_CONTROL)) {
		pr_info("%s: disconnect\n", __func__);
		disconnect_to_bam();
		wake_unlock(&bam_wakelock);
	} else if (new_state & SMSM_A2_POWER_CONTROL) {
		pr_info("%s: init\n", __func__);
		wake_lock(&bam_wakelock);
		bam_init();
	} else {
		pr_err("%s: unsupported state change\n", __func__);
	}

}

static void bam_dmux_smsm_ack_cb(void *priv, uint32_t old_state,
						uint32_t new_state)
{
	pr_info("%s: 0x%08x -> 0x%08x\n", __func__, old_state, new_state);
	complete_all(&ul_wakeup_ack_completion);
}

static int bam_dmux_probe(struct platform_device *pdev)
{
	int rc;

	DBG("%s probe called\n", __func__);
	if (bam_mux_initialized)
		return 0;

	dfab_clk = clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(dfab_clk)) {
		pr_err("%s: did not get dfab clock\n", __func__);
		return -EFAULT;
	}

	rc = clk_set_rate(dfab_clk, 64000000);
	if (rc)
		pr_err("%s: unable to set dfab clock rate\n", __func__);

	bam_mux_rx_workqueue = create_singlethread_workqueue("bam_dmux_rx");
	if (!bam_mux_rx_workqueue)
		return -ENOMEM;

	bam_mux_tx_workqueue = create_singlethread_workqueue("bam_dmux_tx");
	if (!bam_mux_tx_workqueue) {
		destroy_workqueue(bam_mux_rx_workqueue);
		return -ENOMEM;
	}

	for (rc = 0; rc < BAM_DMUX_NUM_CHANNELS; ++rc) {
		spin_lock_init(&bam_ch[rc].lock);
		scnprintf(bam_ch[rc].name, BAM_DMUX_CH_NAME_MAX_LEN,
					"bam_dmux_ch_%d", rc);
		/* bus 2, ie a2 stream 2 */
		bam_ch[rc].pdev = platform_device_alloc(bam_ch[rc].name, 2);
		if (!bam_ch[rc].pdev) {
			pr_err("%s: platform device alloc failed\n", __func__);
			destroy_workqueue(bam_mux_rx_workqueue);
			destroy_workqueue(bam_mux_tx_workqueue);
			return -ENOMEM;
		}
	}

	init_completion(&ul_wakeup_ack_completion);
	init_completion(&bam_connection_completion);
	INIT_DELAYED_WORK(&ul_timeout_work, ul_timeout);
	wake_lock_init(&bam_wakelock, WAKE_LOCK_SUSPEND, "bam_dmux_wakelock");

	rc = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_A2_POWER_CONTROL,
					bam_dmux_smsm_cb, NULL);

	if (rc) {
		destroy_workqueue(bam_mux_rx_workqueue);
		destroy_workqueue(bam_mux_tx_workqueue);
		pr_err("%s: smsm cb register failed, rc: %d\n", __func__, rc);
		return -ENOMEM;
	}

	rc = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_A2_POWER_CONTROL_ACK,
					bam_dmux_smsm_ack_cb, NULL);

	if (rc) {
		destroy_workqueue(bam_mux_rx_workqueue);
		destroy_workqueue(bam_mux_tx_workqueue);
		smsm_state_cb_deregister(SMSM_MODEM_STATE,
					SMSM_A2_POWER_CONTROL,
					bam_dmux_smsm_cb, NULL);
		pr_err("%s: smsm ack cb register failed, rc: %d\n", __func__,
				rc);
		for (rc = 0; rc < BAM_DMUX_NUM_CHANNELS; ++rc)
			platform_device_put(bam_ch[rc].pdev);
		return -ENOMEM;
	}

	if (smsm_get_state(SMSM_MODEM_STATE) & SMSM_A2_POWER_CONTROL)
		bam_dmux_smsm_cb(NULL, 0, smsm_get_state(SMSM_MODEM_STATE));

	return 0;
}

static struct platform_driver bam_dmux_driver = {
	.probe		= bam_dmux_probe,
	.driver		= {
		.name	= "BAM_RMNT",
		.owner	= THIS_MODULE,
	},
};

static int __init bam_dmux_init(void)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *dent;

	dent = debugfs_create_dir("bam_dmux", 0);
	if (!IS_ERR(dent)) {
		debug_create("tbl", 0444, dent, debug_tbl);
		debug_create("ul_pkt_cnt", 0444, dent, debug_ul_pkt_cnt);
		debug_create("stats", 0444, dent, debug_stats);
	}
#endif
	subsys_notif_register_notifier("modem", &restart_notifier);
	return platform_driver_register(&bam_dmux_driver);
}

late_initcall(bam_dmux_init); /* needs to init after SMD */
MODULE_DESCRIPTION("MSM BAM DMUX");
MODULE_LICENSE("GPL v2");
