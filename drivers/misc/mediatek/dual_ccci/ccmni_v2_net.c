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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni.c
 *
 * Project:
 * --------
 *
 * Description:
 *------------
 *
 *
 * Author:
 * -------
 *
 *
 ****************************************************************************/

#include <ccci.h>
#include <linux/sockios.h>

#define CCMNI_DBG_INFO 1

#define SIOCSTXQSTATE (SIOCDEVPRIVATE + 0)

struct ccmni_v2_instance_t {
	int channel;
	int m_md_id;
	int uart_rx;
	int uart_rx_ack;
	int uart_tx;
	int uart_tx_ack;
	int ready;
	int net_if_off;
	int log_count;
	unsigned long flags;
	struct timer_list timer;
	unsigned long send_len;
	struct net_device *dev;
	struct wake_lock wake_lock;
	spinlock_t spinlock;
	atomic_t usage;
	struct shared_mem_ccmni_t *shared_mem;
	int shared_mem_phys_addr;

	unsigned char mac_addr[ETH_ALEN];

	struct tasklet_struct tasklet;
	void *owner;

};

struct ccmni_v2_ctl_block_t {
	int m_md_id;
	int ccci_is_ready;
	struct ccmni_v2_instance_t *ccmni_v2_instance[CCMNI_V2_PORT_NUM];
	struct wake_lock ccmni_wake_lock;
	char wakelock_name[16];
	struct MD_CALL_BACK_QUEUE ccmni_notifier;
};

static void ccmni_v2_read(unsigned long arg);

static void ccmni_make_etherframe(void *_eth_hdr, u8 *mac_addr,
				  int packet_type)
{
	struct ethhdr *eth_hdr = _eth_hdr;

	memcpy(eth_hdr->h_dest, mac_addr, sizeof(eth_hdr->h_dest));
	memset(eth_hdr->h_source, 0, sizeof(eth_hdr->h_source));
	if (packet_type == IPV6_VERSION)
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IPV6);
	else
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IP);
}

static unsigned char *ccmni_v2_phys_to_virt(int md_id, unsigned char *addr_phy)
{
	int ccmni_rx_base_phy;
	int ccmni_rx_base_virt;
	int p_to_v_offset;

	ccmni_v2_ul_base_req(md_id, &ccmni_rx_base_virt, &ccmni_rx_base_phy);
	p_to_v_offset =
	    (unsigned char *)(ccmni_rx_base_virt) -
	    (unsigned char *)(ccmni_rx_base_phy);

	return p_to_v_offset + addr_phy + get_md2_ap_phy_addr_fixed();
}

void ccmni_v2_dump(int md_id)
{
#if 0
	int i = 0, port = 0;
	struct ccmni_v2_instance_t *ccmni;
	struct ccmni_v2_ctl_block_t *ctl_b =
	    (struct ccmni_v2_ctl_block_t *) ccmni_ctl_block[md_id];

	CCCI_MSG_INF(md_id, "ctl", "ccmni v2 dump start\n");
	for (port = 0; port < CCMNI_V2_PORT_NUM; port++) {
		ccmni = ctl_b->ccmni_v2_instance[port];
		CCCI_MSG_INF(md_id, "ctl",
			     "Port%d RX CONTROL: read_out=%d, avai_out=%d, avai_in=%d,q_len=%d\n",
			     port, ccmni->shared_mem->rx_control.read_out,
			     ccmni->shared_mem->rx_control.avai_out,
			     ccmni->shared_mem->rx_control.avai_in,
			     ccmni->shared_mem->rx_control.q_length);
		CCCI_MSG_INF(md_id, "ctl", "Port%d RX ringbuff:\n", port);
		for (i = 0; i < CCMNI_CTRL_Q_RX_SIZE; i++) {
			if (ccmni->shared_mem->q_rx_ringbuff[i].ptr != NULL
			    && ccmni->shared_mem->q_rx_ringbuff[i].len != 0)
				CCCI_MSG_INF(md_id, "ctl",
					     "[%d]: ptr=%08X len=%d\n", i,
					     (int)(ccmni->
						   shared_mem->q_rx_ringbuff[i].
						   ptr),
					     ccmni->
					     shared_mem->q_rx_ringbuff[i].len);
		}
		CCCI_MSG_INF(md_id, "ctl",
			     "Port%d TX CONTROL: read_out=%d, avai_out=%d, avai_in=%d,q_len=%d\n",
			     port, ccmni->shared_mem->tx_control.read_out,
			     ccmni->shared_mem->tx_control.avai_out,
			     ccmni->shared_mem->tx_control.avai_in,
			     ccmni->shared_mem->tx_control.q_length);
		CCCI_MSG_INF(md_id, "ctl", "Port%d TX ringbuff:\n", port);

		for (i = 0; i < CCMNI_CTRL_Q_TX_SIZE; i++) {
			if (ccmni->shared_mem->q_tx_ringbuff[i].ptr != NULL
			    && ccmni->shared_mem->q_tx_ringbuff[i].len != 0)
				CCCI_MSG_INF(md_id, "ctl",
					     "[%d]: ptr=%08X len=%d\n", i,
					     (int)(ccmni->
						   shared_mem->q_tx_ringbuff[i].
						   ptr),
					     ccmni->
					     shared_mem->q_tx_ringbuff[i].len);
		}
	}
	CCCI_MSG_INF(md_id, "ctl", "ccmni v2 dump end\n");
#endif
}

static void ccmni_v2_reset_buffers(struct ccmni_v2_instance_t *ccmni)
{
	int *ccmni_rx_base_phy;
	int *ccmni_rx_base_virt;
	unsigned char *ptr_virt;
	int md_id;
	int count;
#if CCMNI_DBG_INFO
	struct dbg_info_ccmni_t *dbg_info;
#endif

	if (ccmni == NULL) {
		CCCI_MSG("[Error]CCMNI V2 get NULL pointer for buffer reset\n");
		return;
	}

	md_id = ccmni->m_md_id;
	/*  DL --RX */
	ccmni->shared_mem->rx_control.read_out = 0;
	ccmni->shared_mem->rx_control.avai_out = 0;
	ccmni->shared_mem->rx_control.avai_in =
	    CCMNI_CTRL_Q_RX_SIZE_DEFAULT - 1;
	ccmni->shared_mem->rx_control.q_length = CCMNI_CTRL_Q_RX_SIZE;
	/*  UP -- TX */
	memset(&ccmni->shared_mem->tx_control, 0,
	       sizeof(struct buffer_control_ccmni_t));
	memset(ccmni->shared_mem->q_tx_ringbuff, 0,
	       sizeof(struct q_ringbuf_ccmni_t) * CCMNI_CTRL_Q_TX_SIZE);

	memset(ccmni->shared_mem->q_rx_ringbuff, 0,
	       ccmni->shared_mem->rx_control.q_length *
	       sizeof(struct q_ringbuf_ccmni_t));
	ccmni_v2_dl_base_req(md_id, &ccmni_rx_base_virt, &ccmni_rx_base_phy);

	/* Each channel has 100 RX buffers default */
	for (count = 0; count < CCMNI_CTRL_Q_RX_SIZE_DEFAULT; count++) {
		ccmni->shared_mem->q_rx_ringbuff[count].ptr =
		    (CCMNI_CTRL_Q_RX_SIZE_DEFAULT * ccmni->channel +
		     count) * CCMNI_SINGLE_BUFF_SIZE +
		    (unsigned char *)ccmni_rx_base_phy +
		    CCMNI_BUFF_HEADER_SIZE + CCMNI_BUFF_DBG_INFO_SIZE -
		    get_md2_ap_phy_addr_fixed();

		ptr_virt =
		    ccmni_v2_phys_to_virt(md_id,
					  (unsigned char *)(ccmni->
							    shared_mem->q_rx_ringbuff
							    [count].ptr));

		/* buffer header and footer init */
		/* Assume int to be 32bit. May need further modifying!!!!! */
		*((int *)(ptr_virt - CCMNI_BUFF_HEADER_SIZE)) =
		    CCMNI_BUFF_HEADER;
		*((int *)(ptr_virt + CCMNI_BUFF_DATA_FIELD_SIZE)) =
		    CCMNI_BUFF_FOOTER;

#if CCMNI_DBG_INFO
		/* debug info */
		dbg_info =
		    (struct dbg_info_ccmni_t *) (ptr_virt - CCMNI_BUFF_HEADER_SIZE -
					  CCMNI_BUFF_DBG_INFO_SIZE);
		dbg_info->port = ccmni->channel;
		dbg_info->avai_in_no = count;
#endif
	}
	CCCI_MSG("ccmni_v2_reset_buffers\n");
}

int ccmni_v2_ipo_h_restore(int md_id)
{
	struct ccmni_v2_ctl_block_t *ctlb;
	int i;

	ctlb = ccmni_ctl_block[md_id];
	for (i = 0; i < CCMNI_V2_PORT_NUM; i++)
		ccmni_v2_reset_buffers(ctlb->ccmni_v2_instance[i]);

	return 0;
}

static void reset_ccmni_v2_instance_buffer(struct ccmni_v2_instance_t *
					   ccmni_v2_instance)
{
	unsigned long flags;

	spin_lock_irqsave(&ccmni_v2_instance->spinlock, flags);
	ccmni_v2_reset_buffers(ccmni_v2_instance);
	spin_unlock_irqrestore(&ccmni_v2_instance->spinlock, flags);
}

static void stop_ccmni_v2_instance(struct ccmni_v2_instance_t *ccmni_v2_instance)
{
	unsigned long flags;

	spin_lock_irqsave(&ccmni_v2_instance->spinlock, flags);
	if (ccmni_v2_instance->net_if_off == 0) {
		ccmni_v2_instance->net_if_off = 1;
		netif_carrier_off(ccmni_v2_instance->dev);
		del_timer(&ccmni_v2_instance->timer);
	}
	spin_unlock_irqrestore(&ccmni_v2_instance->spinlock, flags);
}

static void restore_ccmni_v2_instance(struct ccmni_v2_instance_t *ccmni_v2_instance)
{
	unsigned long flags;

	spin_lock_irqsave(&ccmni_v2_instance->spinlock, flags);
	if (ccmni_v2_instance->net_if_off) {
		ccmni_v2_instance->net_if_off = 0;
		netif_carrier_on(ccmni_v2_instance->dev);
	}
	spin_unlock_irqrestore(&ccmni_v2_instance->spinlock, flags);
}

static void ccmni_v2_notifier_call(struct MD_CALL_BACK_QUEUE *notifier,
				   unsigned long val)
{
	int i = 0;
	struct ccmni_v2_ctl_block_t *ctl_b =
	    container_of(notifier, struct ccmni_v2_ctl_block_t, ccmni_notifier);
	struct ccmni_v2_instance_t *instance;

	switch (val) {
	case CCCI_MD_EXCEPTION:
		ctl_b->ccci_is_ready = 0;
		for (i = 0; i < CCMNI_V2_PORT_NUM; i++) {
			instance = ctl_b->ccmni_v2_instance[i];
			if (instance)
				stop_ccmni_v2_instance(instance);
		}
		break;
	case CCCI_MD_STOP:
		for (i = 0; i < CCMNI_V2_PORT_NUM; i++) {
			instance = ctl_b->ccmni_v2_instance[i];
			if (instance)
				stop_ccmni_v2_instance(instance);
		}
		break;

	case CCCI_MD_RESET:
		ctl_b->ccci_is_ready = 0;
		for (i = 0; i < CCMNI_V2_PORT_NUM; i++) {
			instance = ctl_b->ccmni_v2_instance[i];
			if (instance)
				reset_ccmni_v2_instance_buffer(instance);
		}
		break;

	case CCCI_MD_BOOTUP:
		if (ctl_b->ccci_is_ready == 0) {
			ctl_b->ccci_is_ready = 1;
			for (i = 0; i < CCMNI_V2_PORT_NUM; i++) {
				instance = ctl_b->ccmni_v2_instance[i];
				if (instance)
					restore_ccmni_v2_instance(instance);
			}
		}
		break;

	default:
		break;
	}
}

static void timer_func(unsigned long data)
{
	struct ccmni_v2_instance_t *ccmni = (struct ccmni_v2_instance_t *) data;
	int contin = 0;
	int ret = 0;
	struct ccci_msg_t msg;
	struct ccmni_v2_ctl_block_t *ctl_b;
	int md_id;

	ctl_b = (struct ccmni_v2_ctl_block_t *) ccmni->owner;
	if (ctl_b == 0)
		return;
	md_id = ctl_b->m_md_id;
	spin_lock_bh(&ccmni->spinlock);
	if (test_bit(CCMNI_RECV_ACK_PENDING, &ccmni->flags)) {
		msg.magic = 0;
		msg.id = CCMNI_CHANNEL_OFFSET + ccmni->channel;
		msg.channel = ccmni->uart_rx_ack;
		msg.reserved = 0;
		ret = ccci_message_send(md_id, &msg, 1);

		if (ret == -CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL)
			contin = 1;
		else
			clear_bit(CCMNI_RECV_ACK_PENDING, &ccmni->flags);
	}

	if (test_bit(CCMNI_SEND_PENDING, &ccmni->flags)) {
		msg.magic = 0;
		msg.id = ccmni->send_len;
		msg.channel = ccmni->uart_tx;
		msg.reserved = 0;
		ret = ccci_message_send(md_id, &msg, 1);

		if (ret == -CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL)
			contin = 1;
		else {
			clear_bit(CCMNI_SEND_PENDING, &ccmni->flags);
			ccmni->send_len = 0;
		}
	}
	if (test_bit(CCMNI_RECV_PENDING, &ccmni->flags))
		tasklet_schedule(&ccmni->tasklet);
	spin_unlock_bh(&ccmni->spinlock);
	if (contin)
		mod_timer(&ccmni->timer, jiffies + 2);

	return;

}

static int ccmni_v2_check_info(int md_id, int ch,
			       const unsigned char *ccmni_ptr, int ccmni_len)
{
	int ret = 0;

	if ((ccmni_ptr == NULL) || (ccmni_len <= 0)) {
		CCCI_MSG_INF(md_id, "net",
			     "CCMNI%d_check_info() ptr_n or len_n error!\n",
			     ch);
		ret = -CCCI_ERR_INVALID_PARAM;
		goto check_info_error;
	}

	/* Check Header and Footer */
	if ((*(int *)(ccmni_ptr - CCMNI_BUFF_HEADER_SIZE) != CCMNI_BUFF_HEADER)
	    || (*(int *)(ccmni_ptr + CCMNI_BUFF_DATA_FIELD_SIZE) !=
		CCMNI_BUFF_FOOTER)) {
		CCCI_MSG_INF(md_id, "net",
			     "CCMNI%d_check_info() check header and footer error\n",
			     ch);
		ret = -CCCI_ERR_MEM_CHECK_FAIL;
		goto check_info_error;
	}

	/* Check End Byte */
	if (*(unsigned char *)
	    ((unsigned int)(ccmni_ptr + ccmni_len + 3) & 0xfffffffc) !=
	    CCMNI_DATA_END) {
		CCCI_MSG_INF(md_id, "net",
			     "CCMNI%d_check_info() check end byte error\n", ch);
		ret = -CCCI_ERR_MEM_CHECK_FAIL;
		goto check_info_error;
	}

	ret = 0;

 check_info_error:

	return ret;

}

static int ccmni_v2_receive(struct ccmni_v2_instance_t *ccmni,
			    const unsigned char *ccmni_ptr, int ccmni_len)
{
	int packet_type, ret = 0;
	struct sk_buff *skb;
	struct ccmni_v2_ctl_block_t *ctl_b;
	int md_id = -1;

	if (ccmni == NULL) {
		CCCI_MSG_INF(md_id, "net",
			     "CCMNI_v2_receive: ccmni is null\n");
		return -1;
	}
	if ((ccmni_ptr == NULL) || (ccmni_len <= 0)) {
		CCCI_MSG_INF(md_id, "net",
			     "CCMNI%d_receive: invalid private data\n",
			     ccmni->channel);
		return -1;
	}
	ctl_b = (struct ccmni_v2_ctl_block_t *) ccmni->owner;
	md_id = ctl_b->m_md_id;
	skb = dev_alloc_skb(ccmni_len);

	if (skb) {
		packet_type = ccmni_ptr[0] & 0xF0;
		memcpy(skb_put(skb, ccmni_len), ccmni_ptr, ccmni_len);
		ccmni_make_etherframe(skb->data - ETH_HLEN,
				      ccmni->dev->dev_addr, packet_type);
		skb_set_mac_header(skb, -ETH_HLEN);

		skb->dev = ccmni->dev;
		if (packet_type == IPV6_VERSION)
			skb->protocol = htons(ETH_P_IPV6);
		else
			skb->protocol = htons(ETH_P_IP);
		/* skb->ip_summed = CHECKSUM_UNNECESSARY; */
		skb->ip_summed = CHECKSUM_NONE;

		ret = netif_rx(skb);

		CCCI_CCMNI_MSG(md_id, "CCMNI%d invoke netif_rx()=%d\n",
			       ccmni->channel, ret);

		ccmni->dev->stats.rx_packets++;
		ccmni->dev->stats.rx_bytes += ccmni_len;
		CCCI_CCMNI_MSG(md_id,
			       "CCMNI%d rx_pkts=%ld, stats_rx_bytes=%ld\n",
			       ccmni->channel, ccmni->dev->stats.rx_packets,
			       ccmni->dev->stats.rx_bytes);

		ret = 0;
	} else {
		CCCI_MSG_INF(md_id, "net",
			     "CCMNI%d Socket buffer allocate fail\n",
			     ccmni->channel);
		ret = -CCCI_ERR_MEM_CHECK_FAIL;
	}

	return ret;
}

static void ccmni_v2_read(unsigned long arg)
{
	int ret;
	int read_out, avai_out, avai_in, q_length;
	int packet_cnt, packet_cnt_save, consumed;
	int rx_buf_res_left_cnt;
#if CCMNI_DBG_INFO
	struct dbg_info_ccmni_t *dbg_info;
#endif
	struct ccmni_v2_instance_t *ccmni = (struct ccmni_v2_instance_t *) arg;
	unsigned char *ccmni_ptr;
	unsigned int ccmni_len, q_idx;
	struct ccmni_v2_ctl_block_t *ctl_b;
	int md_id = -1;
	struct ccci_msg_t msg;

	if (ccmni == NULL) {
		CCCI_MSG_INF(md_id, "net",
			     "ccmni_v2_read: ccmni is null\n");
		return;
	}

	ctl_b = (struct ccmni_v2_ctl_block_t *) ccmni->owner;
	md_id = ctl_b->m_md_id;

	spin_lock_bh(&ccmni->spinlock);

	if (ctl_b->ccci_is_ready == 0) {
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d_read fail when modem not ready\n",
			     ccmni->channel);
		goto out;
	}

	read_out = ccmni->shared_mem->rx_control.read_out;
	avai_out = ccmni->shared_mem->rx_control.avai_out;
	avai_in = ccmni->shared_mem->rx_control.avai_in;
	q_length = ccmni->shared_mem->rx_control.q_length;

	if ((read_out < 0) || (avai_out < 0) || (avai_in < 0) || (q_length < 0)) {
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d_read fail: avai_out=%d, read_out=%d, avai_in=%d, q_length=%d\n",
			     ccmni->channel, avai_out, read_out, avai_in,
			     q_length);
		goto out;
	}

	if ((read_out >= q_length) || (avai_out >= q_length)
	    || (avai_in >= q_length)) {
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d_read fail: avai_out=%d, read_out=%d, avai_in=%d, q_length=%d\n",
			     ccmni->channel, avai_out, read_out, avai_in,
			     q_length);
		goto out;
	}

	/* Number of packets waiting to be processed */
	packet_cnt =
	    avai_out >=
	    read_out ? (avai_out - read_out) : (avai_out - read_out + q_length);

	packet_cnt_save = packet_cnt;
	rx_buf_res_left_cnt =
	    avai_in >=
	    avai_out ? (avai_in - avai_out) : (avai_in - avai_out + q_length);

	if (packet_cnt <= 0) {
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d_read fail: nothing to read, avai_out=%d, read_out=%d, q_length=%d\n",
			     ccmni->channel, avai_out, read_out, q_length);
		goto out;
	}

	q_idx = read_out;

	CCCI_CCMNI_MSG(md_id,
		       "CCMNI%d_receive[Before]: avai_out=%d, read_out=%d, avai_in=%d, packet_cnt=%d\n",
		       ccmni->channel, avai_out, read_out, avai_in, packet_cnt);

	consumed = 0;

	for (; packet_cnt > 0; packet_cnt--) {
		q_idx &= q_length - 1;

		ccmni_ptr =
		    ccmni_v2_phys_to_virt(md_id,
					  (unsigned char *)(ccmni->
							    shared_mem->q_rx_ringbuff
							    [q_idx].ptr));
		ccmni_len = ccmni->shared_mem->q_rx_ringbuff[q_idx].len;
#if CCMNI_DBG_INFO
		/* DBG info */
		dbg_info =
		    (struct dbg_info_ccmni_t *) (ccmni_ptr - CCMNI_BUFF_HEADER_SIZE -
					  CCMNI_BUFF_DBG_INFO_SIZE);
#endif
		if (-CCCI_ERR_MEM_CHECK_FAIL ==
		    ccmni_v2_check_info(md_id, ccmni->channel, ccmni_ptr,
					ccmni_len)) {
			CCCI_DBG_MSG(md_id, "net",
				     "CCMNI%d_read: check info error, read_out=%d\n",
				     ccmni->channel, read_out);
#if CCMNI_DBG_INFO
			/* dbg_info->port        = ccmni->channel; */
			dbg_info->avai_in_no = q_idx;
			/* dbg_info->avai_out_no = q_idx; */
			dbg_info->read_out_no = q_idx;
#endif
			avai_in++;
			avai_in &= q_length - 1;

			ccmni->shared_mem->q_rx_ringbuff[avai_in].ptr =
			    ccmni->shared_mem->q_rx_ringbuff[q_idx].ptr;

			ccmni_ptr =
			    ccmni_v2_phys_to_virt(md_id,
						  (unsigned char
						   *)
						  (ccmni->shared_mem->q_rx_ringbuff
						   [avai_in].ptr));
#if CCMNI_DBG_INFO
			dbg_info =
			    (struct dbg_info_ccmni_t *) (ccmni_ptr -
						  CCMNI_BUFF_HEADER_SIZE -
						  CCMNI_BUFF_DBG_INFO_SIZE);
			dbg_info->avai_in_no = avai_in;
#endif
			q_idx++;
			consumed++;
			continue;
		}
		ret = ccmni_v2_receive(ccmni, ccmni_ptr, ccmni_len);
		if (0 == ret) {
#if CCMNI_DBG_INFO
			/* dbg_info->port        = ccmni->channel; */
			dbg_info->avai_in_no = q_idx;
			/* dbg_info->avai_out_no = q_idx; */
			dbg_info->read_out_no = q_idx;
#endif
			avai_in++;
			avai_in &= q_length - 1;
			ccmni->shared_mem->q_rx_ringbuff[avai_in].ptr =
			    ccmni->shared_mem->q_rx_ringbuff[q_idx].ptr;

			ccmni_ptr =
			    ccmni_v2_phys_to_virt(md_id,
						  (unsigned char
						   *)
						  (ccmni->shared_mem->q_rx_ringbuff
						   [avai_in].ptr));
#if CCMNI_DBG_INFO
			dbg_info =
			    (struct dbg_info_ccmni_t *) (ccmni_ptr -
						  CCMNI_BUFF_HEADER_SIZE -
						  CCMNI_BUFF_DBG_INFO_SIZE);
			dbg_info->avai_in_no = avai_in;
#endif
			q_idx++;
			consumed++;
		} else if (-CCCI_ERR_MEM_CHECK_FAIL == ret) {
			/* If dev_alloc_skb() failed, retry right now may still fail.
			 *  So setup timer, and retry later.
			 */
			set_bit(CCMNI_RECV_PENDING, &ccmni->flags);
			CCCI_DBG_MSG(md_id, "net",
				     "CCMNI%d_read: no sk_buff, retrying, read_out=%d, avai_out=%d\n",
				     ccmni->channel, q_idx, avai_out);

			mod_timer(&ccmni->timer, jiffies + msecs_to_jiffies(10));

			break;
			/* q_idx++; */
			/* consumed++; */
		}
	}

	read_out = (q_idx & (q_length - 1));

	CCCI_CCMNI_MSG(md_id, "CCMNI%d_receive[After]: consumed=%d\n",
		       ccmni->channel, consumed);

	if (consumed > packet_cnt_save) {
		/* Sanity check. This should not happen! */
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d_read fail: consumed more than packet_cnt, consumed = %d, packet_cnt = %d\n",
			     ccmni->channel, consumed, packet_cnt_save);

		/* Should ignore all data in buffer??? haow.wang */
		ccmni->shared_mem->rx_control.read_out = avai_out;
		ccmni->shared_mem->rx_control.avai_in = avai_in;
		goto out;
	}

	ccmni->shared_mem->rx_control.read_out = read_out;
	ccmni->shared_mem->rx_control.avai_in = avai_in;

	CCCI_CCMNI_MSG(md_id, "CCMNI%d_read to write mailbox(ch%d, tty%d)\n",
		       ccmni->channel, ccmni->uart_rx_ack,
		       CCMNI_CHANNEL_OFFSET + ccmni->channel);
	msg.magic = 0xFFFFFFFF;
	msg.id = CCMNI_CHANNEL_OFFSET + ccmni->channel;
	msg.channel = ccmni->uart_rx_ack;
	msg.reserved = 0;
	ret = ccci_message_send(md_id, &msg, 1);
	if (ret == -CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL) {
		set_bit(CCMNI_RECV_ACK_PENDING, &ccmni->flags);
		mod_timer(&ccmni->timer, jiffies);
	} else if (ret == sizeof(struct ccci_msg_t))
		clear_bit(CCMNI_RECV_ACK_PENDING, &ccmni->flags);

 out:
	spin_unlock_bh(&ccmni->spinlock);

	CCCI_CCMNI_MSG(md_id, "CCMNI%d_read invoke wake_lock_timeout(1s)\n",
		       ccmni->channel);
	wake_lock_timeout(&ctl_b->ccmni_wake_lock, HZ);
}

/*   will be called when modem sends us something. */
/*   we will then copy it to the tty's buffer. */
/*   this is essentially the "read" fops. */
static void ccmni_v2_callback(void *private_data)
{
	struct logic_channel_info_t *ch_info = (struct logic_channel_info_t *) private_data;
	struct ccmni_v2_instance_t *ccmni = (struct ccmni_v2_instance_t *) (ch_info->m_owner);
	struct ccci_msg_t msg;

	while (get_logic_ch_data(ch_info, &msg)) {
		switch (msg.channel) {
		case CCCI_CCMNI1_TX_ACK:
		case CCCI_CCMNI2_TX_ACK:
		case CCCI_CCMNI3_TX_ACK:
			/*  this should be in an interrupt, */
			/*  so no locking required... */
			ccmni->ready = 1;
			if (atomic_read(&ccmni->usage) > 0)
				netif_wake_queue(ccmni->dev);
			break;

		case CCCI_CCMNI1_RX:
		case CCCI_CCMNI2_RX:
		case CCCI_CCMNI3_RX:
			tasklet_schedule(&ccmni->tasklet);
			break;

		default:
			break;
		}
	}
}

/*   The function start_xmit is called when there is one packet to transmit. */
static int ccmni_v2_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret = NETDEV_TX_OK;
	int result = 0;
	int read_out, avai_in, avai_out, q_length, q_idx;
#if CCMNI_DBG_INFO
	struct dbg_info_ccmni_t *dbg_info;
#endif

	unsigned char *ccmni_ptr;
	struct ccmni_v2_instance_t *ccmni = netdev_priv(dev);
	struct ccmni_v2_ctl_block_t *ctl_b = (struct ccmni_v2_ctl_block_t *) (ccmni->owner);
	int md_id = ctl_b->m_md_id;
	struct ccci_msg_t msg;

	spin_lock_bh(&ccmni->spinlock);

	if (ctl_b->ccci_is_ready == 0) {
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d transfer data fail when modem not ready\n",
			     ccmni->channel);
		ret = NETDEV_TX_BUSY;
		goto _ccmni_start_xmit_busy;
	}

	read_out = ccmni->shared_mem->tx_control.read_out;
	avai_in = ccmni->shared_mem->tx_control.avai_in;
	avai_out = ccmni->shared_mem->tx_control.avai_out;
	q_length = ccmni->shared_mem->tx_control.q_length;

	if ((read_out < 0) || (avai_out < 0) || (avai_in < 0) || (q_length < 0)) {
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d TX fail: avai_out=%d, read_out=%d, avai_in=%d, q_length=%d\n",
			     ccmni->channel, avai_out, read_out, avai_in,
			     q_length);
		goto _ccmni_start_xmit_busy;
	}

	if ((read_out >= q_length) || (avai_out >= q_length)
	    || (avai_in >= q_length)) {
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d TX fail: avai_out=%d, read_out=%d, avai_in=%d, q_length=%d\n",
			     ccmni->channel, avai_out, read_out, avai_in,
			     q_length);
		goto _ccmni_start_xmit_busy;
	}

	/* Choose Q index */
	q_idx = avai_out;
	ccmni_ptr = ccmni->shared_mem->q_tx_ringbuff[q_idx].ptr;

	/* check if too many data waiting to be read out or Q not initialized yet */
	/* ccmni_ptr=NULL when not initialized???? haow.wang */
	if ((q_idx == avai_in) || (ccmni_ptr == NULL)) {
		if (ccmni->log_count >= 1000) {
			CCCI_DBG_MSG(md_id, "net",
				     "CCMNI%d TX busy and stop queue: q_idx=%d, skb->len=%d\n",
				     ccmni->channel, q_idx, skb->len);
			CCCI_DBG_MSG(md_id, "net",
				     "       TX read_out = %d  avai_out = %d avai_in = %d\n",
				     ccmni->shared_mem->tx_control.read_out,
				     ccmni->shared_mem->tx_control.avai_out,
				     ccmni->shared_mem->tx_control.avai_in);
			CCCI_DBG_MSG(md_id, "net",
				     "       RX read_out = %d  avai_out = %d avai_in = %d\n",
				     ccmni->shared_mem->rx_control.read_out,
				     ccmni->shared_mem->rx_control.avai_out,
				     ccmni->shared_mem->rx_control.avai_in);
			ccmni->log_count = 1;
		} else {
			ccmni->log_count++;
		}
		netif_stop_queue(ccmni->dev);

		/* Set CCMNI ready to ZERO, and wait for the ACK from modem side. */
		ccmni->ready = 0;
		ret = NETDEV_TX_BUSY;
		goto _ccmni_start_xmit_busy;
	}

	ccmni_ptr =
	    ccmni_v2_phys_to_virt(md_id,
				  (unsigned char *)(ccmni->
						    shared_mem->q_tx_ringbuff
						    [q_idx].ptr));

	CCCI_CCMNI_MSG(md_id,
		       "CCMNI%d_start_xmit: skb_len=%d, ccmni_ready=%d\n",
		       ccmni->channel, skb->len, ccmni->ready);

	if (skb->len > CCMNI_MTU) {
		/* Sanity check; this should not happen! */
		/* Digest and return OK. */
		CCCI_DBG_MSG(md_id, "net",
			     "CCMNI%d packet size exceed 1500 bytes: size=%d\n",
			     ccmni->channel, skb->len);
		dev->stats.tx_dropped++;
		goto _ccmni_start_xmit_exit;
	}
#if CCMNI_DBG_INFO
	/* DBG info */
	dbg_info =
	    (struct dbg_info_ccmni_t *) (ccmni_ptr - CCMNI_BUFF_HEADER_SIZE -
				  CCMNI_BUFF_DBG_INFO_SIZE);
	dbg_info->avai_out_no = q_idx;
#endif

	memcpy(ccmni_ptr, skb->data, skb->len);
	ccmni->shared_mem->q_tx_ringbuff[q_idx].len = skb->len;

	/* End byte */
	*(unsigned char *)(ccmni_ptr + skb->len) = CCMNI_DATA_END;
	/* wait memory updated */
	mb();

	/* Update avail_out after data buffer filled */
	q_idx++;
	ccmni->shared_mem->tx_control.avai_out = (q_idx & (q_length - 1));
	/* wait memory updated */
	mb();

	msg.addr = 0;
	msg.len = skb->len;
	msg.channel = ccmni->uart_tx;
	msg.reserved = 0;
	result = ccci_message_send(md_id, &msg, 1);
	if (result == -CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL) {
		set_bit(CCMNI_SEND_PENDING, &ccmni->flags);
		ccmni->send_len += skb->len;
		mod_timer(&ccmni->timer, jiffies);
	} else if (result == sizeof(struct ccci_msg_t))
		clear_bit(CCMNI_SEND_PENDING, &ccmni->flags);

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

 _ccmni_start_xmit_exit:

	dev_kfree_skb(skb);

 _ccmni_start_xmit_busy:

	spin_unlock_bh(&ccmni->spinlock);

	return ret;
}

static int ccmni_v2_open(struct net_device *dev)
{
	struct ccmni_v2_instance_t *ccmni = netdev_priv(dev);
	struct ccmni_v2_ctl_block_t *ctl_b = (struct ccmni_v2_ctl_block_t *) ccmni->owner;
	int md_id = ctl_b->m_md_id;

	atomic_inc(&ccmni->usage);
	CCCI_MSG_INF(md_id, "net", "CCMNI%d open: usage=%d\n", ccmni->channel, atomic_read(&ccmni->usage));
	if (ctl_b->ccci_is_ready == 0) {
		CCCI_MSG_INF(md_id, "net",
			     "CCMNI%d open fail when modem not ready\n",
			     ccmni->channel);
		return -EIO;
	}
	netif_start_queue(dev);
	return 0;
}

static int ccmni_v2_close(struct net_device *dev)
{
	struct ccmni_v2_instance_t *ccmni = netdev_priv(dev);
	struct ccmni_v2_ctl_block_t *ctl_b = (struct ccmni_v2_ctl_block_t *) ccmni->owner;

	atomic_dec(&ccmni->usage);
	CCCI_MSG_INF(ctl_b->m_md_id, "net", "CCMNI%d close: usage=%d\n", ccmni->channel, atomic_read(&ccmni->usage));
	netif_stop_queue(dev);
	return 0;
}

static int ccmni_v2_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct ccmni_v2_instance_t *ccmni = netdev_priv(dev);
	struct ccmni_v2_ctl_block_t *ctl_b = (struct ccmni_v2_ctl_block_t *)ccmni->owner;

	switch (cmd) {
	case SIOCSTXQSTATE:
		/* ifru_ivalue[3~0]:start/stop; ifru_ivalue[7~4]:reserve; */
		/* ifru_ivalue[15~8]:user id, bit8=rild, bit9=thermal */
		/* ifru_ivalue[31~16]: watchdog timeout value */
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			if (atomic_read(&ccmni->usage) > 0) {
				atomic_dec(&ccmni->usage);
				netif_stop_queue(dev);
				dev->watchdog_timeo = 60*HZ;
			}
		} else {
			if (atomic_read(&ccmni->usage) <= 0) {
				if (netif_running(dev) && netif_queue_stopped(dev))
					netif_wake_queue(dev);
				dev->watchdog_timeo = 1*HZ;
				atomic_inc(&ccmni->usage);
			}
		}
		CCCI_MSG_INF(ctl_b->m_md_id, "net", "SIOCSTXQSTATE request=%d on CCMNI%d usge=%d\n",
			ifr->ifr_ifru.ifru_ivalue, ccmni->channel, atomic_read(&ccmni->usage));
		break;
	default:
		CCCI_MSG_INF(ctl_b->m_md_id, "net", "unknown ioctl cmd=%d on CCMNI%d\n", cmd, ccmni->channel);
		break;
	}

	return 0;
}

static void ccmni_v2_tx_timeout(struct net_device *dev)
{
	struct ccmni_v2_instance_t *ccmni = netdev_priv(dev);

	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0)
		netif_wake_queue(dev);
}

static const struct net_device_ops ccmni_v2_netdev_ops = {
	.ndo_open = ccmni_v2_open,
	.ndo_stop = ccmni_v2_close,
	.ndo_start_xmit = ccmni_v2_start_xmit,
	.ndo_do_ioctl = ccmni_v2_net_ioctl,
	.ndo_tx_timeout = ccmni_v2_tx_timeout,
};

static void ccmni_v2_setup(struct net_device *dev)
{
	struct ccmni_v2_instance_t *ccmni = netdev_priv(dev);
	int retry = 10;

	ether_setup(dev);

	dev->header_ops = NULL;
	dev->netdev_ops = &ccmni_v2_netdev_ops;
	dev->flags = IFF_NOARP & (~IFF_BROADCAST & ~IFF_MULTICAST);
	dev->mtu = CCMNI_MTU;
	dev->tx_queue_len = CCMNI_TX_QUEUE;
	dev->addr_len = ETH_ALEN;
	dev->destructor = free_netdev;

	while (retry-- > 0) {
		random_ether_addr((u8 *) dev->dev_addr);
		if (is_mac_addr_duplicate((u8 *) dev->dev_addr))
			continue;
		else
			break;
	}

	CCCI_CCMNI_MSG(ccmni->m_md_id,
		       "CCMNI%d_setup: features=0x%08x,flags=0x%08x\n",
		       ccmni->channel, (unsigned int)(dev->features),
		       dev->flags);
}

static int ccmni_v2_create_instance(int md_id, int channel)
{
	int ret, size, count;
	int uart_rx, uart_rx_ack;
	int uart_tx, uart_tx_ack;
	struct ccmni_v2_instance_t *ccmni;
	struct net_device *dev = NULL;
	int *ccmni_rx_base_phy;
	int *ccmni_rx_base_virt;
	unsigned char *ptr_virt;
#if CCMNI_DBG_INFO
	struct dbg_info_ccmni_t *dbg_info;
#endif
	struct ccmni_v2_ctl_block_t *ctl_b =
	    (struct ccmni_v2_ctl_block_t *) ccmni_ctl_block[md_id];

	/*   Network device creation and registration. */
	dev = alloc_netdev(sizeof(struct ccmni_v2_instance_t), "eth%d", NET_NAME_UNKNOWN, ccmni_v2_setup);
	if (dev == NULL) {
		CCCI_MSG_INF(md_id, "net", "CCMNI%d allocate netdev fail!\n",
			     channel);
		return -ENOMEM;
	}

	ccmni = netdev_priv(dev);
	ccmni->dev = dev;
	ccmni->channel = channel;
	ccmni->owner = ccmni_ctl_block[md_id];

	if (md_id == MD_SYS1) {
		sprintf(dev->name, "ccmni%d", channel);
	} else {
		sprintf(dev->name, "cc%dmni%d", md_id + 1, channel);
		/* sprintf(dev->name, "ccmni%d", channel); */
	}

	ret = register_netdev(dev);
	if (ret != 0) {
		CCCI_MSG_INF(md_id, "net", "CCMNI%d register netdev fail: %d\n",
			     ccmni->channel, ret);
		goto _ccmni_create_instance_exit;
	}

	ccci_ccmni_v2_ctl_mem_base_req(md_id, ccmni->channel,
		(int *)&ccmni->shared_mem, &ccmni->shared_mem_phys_addr, &size);

	if (ccmni->shared_mem == NULL) {
		CCCI_MSG_INF(md_id, "net", "CCMNI%d allocate memory fail\n",
			     ccmni->channel);
		unregister_netdev(dev);
		ret = -ENOMEM;
		goto _ccmni_create_instance_exit;
	}

	CCCI_CCMNI_MSG(md_id, "0x%08X:0x%08X:%d\n",
		       (unsigned int)ccmni->shared_mem,
		       (unsigned int)ccmni->shared_mem_phys_addr, size);

	ccmni->shared_mem->rx_control.read_out = 0;
	ccmni->shared_mem->rx_control.avai_out = 0;
	ccmni->shared_mem->rx_control.avai_in =
	    CCMNI_CTRL_Q_RX_SIZE_DEFAULT - 1;
	ccmni->shared_mem->rx_control.q_length = CCMNI_CTRL_Q_RX_SIZE;
	memset(ccmni->shared_mem->q_rx_ringbuff, 0,
	       ccmni->shared_mem->rx_control.q_length *
	       sizeof(struct q_ringbuf_ccmni_t));

	ccmni_v2_dl_base_req(md_id, &ccmni_rx_base_virt, &ccmni_rx_base_phy);

	if (ccmni_rx_base_virt == NULL || ccmni_rx_base_phy == NULL) {
		CCCI_MSG_INF(md_id, "net", "CCMNI%d allocate memory fail\n",
			     ccmni->channel);
		unregister_netdev(dev);
		ret = -ENOMEM;

		goto _ccmni_create_instance_exit;
	}

	switch (ccmni->channel) {
	case 0:
		uart_rx = CCCI_CCMNI1_RX;
		uart_rx_ack = CCCI_CCMNI1_RX_ACK;
		uart_tx = CCCI_CCMNI1_TX;
		uart_tx_ack = CCCI_CCMNI1_TX_ACK;
		break;

	case 1:
		uart_rx = CCCI_CCMNI2_RX;
		uart_rx_ack = CCCI_CCMNI2_RX_ACK;
		uart_tx = CCCI_CCMNI2_TX;
		uart_tx_ack = CCCI_CCMNI2_TX_ACK;
		break;

	case 2:
		uart_rx = CCCI_CCMNI3_RX;
		uart_rx_ack = CCCI_CCMNI3_RX_ACK;
		uart_tx = CCCI_CCMNI3_TX;
		uart_tx_ack = CCCI_CCMNI3_TX_ACK;
		break;

	default:
		CCCI_MSG_INF(md_id, "net",
			     "[Error]CCMNI%d Invalid ccmni number\n",
			     ccmni->channel);
		unregister_netdev(dev);
		ret = -ENOSYS;
		goto _ccmni_create_instance_exit;
	}
	ccmni->m_md_id = md_id;

	/* Each channel has 100 RX buffers default */
	for (count = 0; count < CCMNI_CTRL_Q_RX_SIZE_DEFAULT; count++) {
		ccmni->shared_mem->q_rx_ringbuff[count].ptr =
		    (CCMNI_CTRL_Q_RX_SIZE_DEFAULT * ccmni->channel +
		     count) * CCMNI_SINGLE_BUFF_SIZE +
		    (unsigned char *)ccmni_rx_base_phy +
		    CCMNI_BUFF_HEADER_SIZE + CCMNI_BUFF_DBG_INFO_SIZE -
		    get_md2_ap_phy_addr_fixed();

		ptr_virt =
		    ccmni_v2_phys_to_virt(md_id,
					  (unsigned char *)(ccmni->
							    shared_mem->q_rx_ringbuff
							    [count].ptr));

		/* buffer header and footer init */
		/* Assume int to be 32bit. May need further modifying!!!!! */
		*((int *)(ptr_virt - CCMNI_BUFF_HEADER_SIZE)) =
		    CCMNI_BUFF_HEADER;
		*((int *)(ptr_virt + CCMNI_BUFF_DATA_FIELD_SIZE)) =
		    CCMNI_BUFF_FOOTER;

#if CCMNI_DBG_INFO
		/* debug info */
		dbg_info =
		    (struct dbg_info_ccmni_t *) (ptr_virt - CCMNI_BUFF_HEADER_SIZE -
					  CCMNI_BUFF_DBG_INFO_SIZE);
		dbg_info->port = ccmni->channel;
		dbg_info->avai_in_no = count;
#endif
	}

	ccmni->uart_rx = uart_rx;
	ccmni->uart_rx_ack = uart_rx_ack;
	ccmni->uart_tx = uart_tx;
	ccmni->uart_tx_ack = uart_tx_ack;

	/*  Register this ccmni instance to the ccci driver. */
	/*  pass it the notification handler. */
	register_to_logic_ch(md_id, uart_rx, ccmni_v2_callback, (void *)ccmni);
	register_to_logic_ch(md_id, uart_tx_ack, ccmni_v2_callback, (void *)ccmni);

	/*  Initialize the spinlock. */
	spin_lock_init(&ccmni->spinlock);
	setup_timer(&ccmni->timer, timer_func, (unsigned long)ccmni);

	/*  Initialize the tasklet. */
	tasklet_init(&ccmni->tasklet, ccmni_v2_read, (unsigned long)ccmni);

	ctl_b->ccmni_v2_instance[channel] = ccmni;
	ccmni->ready = 1;
	ccmni->net_if_off = 0;
	ccmni->log_count = 1;

	return ret;

 _ccmni_create_instance_exit:
	free_netdev(dev);
	kfree(ccmni);
	ctl_b->ccmni_v2_instance[channel] = NULL;
	return ret;
}

static void ccmni_v2_destroy_instance(int md_id, int channel)
{
	struct ccmni_v2_ctl_block_t *ctl_b =
	    (struct ccmni_v2_ctl_block_t *) ccmni_ctl_block[md_id];
	struct ccmni_v2_instance_t *ccmni = ctl_b->ccmni_v2_instance[channel];

	if (ccmni != NULL) {
		ccmni->ready = 0;
		un_register_to_logic_ch(md_id, ccmni->uart_rx);
		un_register_to_logic_ch(md_id, ccmni->uart_tx_ack);

		if (ccmni->shared_mem != NULL) {
			ccmni->shared_mem = NULL;
			ccmni->shared_mem_phys_addr = 0;
		}

		if (ccmni->dev != NULL)
			unregister_netdev(ccmni->dev);
		/* tasklet_kill(&ccmni->tasklet); */
		ctl_b->ccmni_v2_instance[channel] = NULL;
	}
}

int ccmni_v2_init(int md_id)
{
	int count, ret, curr;
	struct ccmni_v2_ctl_block_t *ctl_b;

	/*  Create control block structure */
	ctl_b = kmalloc(sizeof(struct ccmni_v2_ctl_block_t),
					     GFP_KERNEL);
	if (ctl_b == NULL)
		return -CCCI_ERR_GET_MEM_FAIL;

	memset(ctl_b, 0, sizeof(struct ccmni_v2_ctl_block_t));
	ccmni_ctl_block[md_id] = ctl_b;

	/*  Init ctl_b */
	ctl_b->m_md_id = md_id;
	ctl_b->ccmni_notifier.call = ccmni_v2_notifier_call;
	ctl_b->ccmni_notifier.next = NULL;

	for (count = 0; count < CCMNI_V2_PORT_NUM; count++) {
		ret = ccmni_v2_create_instance(md_id, count);
		if (ret != 0) {
			CCCI_MSG_INF(md_id, "net",
				     "CCMNI%d create instance fail: %d\n",
				     count, ret);
			goto _CCMNI_INSTANCE_CREATE_FAIL;
		}
	}

	ret = md_register_call_chain(md_id, &ctl_b->ccmni_notifier);
	if (ret) {
		CCCI_MSG_INF(md_id, "net", "md_register_call_chain fail: %d\n",
			     ret);
		goto _CCMNI_INSTANCE_CREATE_FAIL;
	}

	snprintf(ctl_b->wakelock_name, sizeof(ctl_b->wakelock_name),
		 "ccci%d_net_v2", (md_id + 1));
	wake_lock_init(&ctl_b->ccmni_wake_lock, WAKE_LOCK_SUSPEND,
		       ctl_b->wakelock_name);

	return ret;

 _CCMNI_INSTANCE_CREATE_FAIL:
	for (curr = 0; curr <= count; curr++)
		ccmni_v2_destroy_instance(md_id, curr);
	kfree(ctl_b);
	ccmni_ctl_block[md_id] = NULL;
	return ret;
}

void ccmni_v2_exit(int md_id)
{
	int count;
	struct ccmni_v2_ctl_block_t *ctl_b =
	    (struct ccmni_v2_ctl_block_t *) ccmni_ctl_block[md_id];

	if (ctl_b) {
		for (count = 0; count < CCMNI_V2_PORT_NUM; count++)
			ccmni_v2_destroy_instance(md_id, count);
		md_unregister_call_chain(md_id, &ctl_b->ccmni_notifier);
		wake_lock_destroy(&ctl_b->ccmni_wake_lock);
	}
}
