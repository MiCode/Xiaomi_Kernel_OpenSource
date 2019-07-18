/* Copyright (c) 2010-2011, 2016-2018, The Linux Foundation.
 * All rights reserved.
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
#ifndef __APR_TAL_H_
#define __APR_TAL_H_

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>

/* APR Client IDs */
#define APR_CLIENT_AUDIO	0x0
#define APR_CLIENT_VOICE	0x1
#define APR_CLIENT_MAX		0x2

#define APR_DL_SMD    0
#define APR_DL_MAX    1

#define APR_DEST_MODEM 0
#define APR_DEST_QDSP6 1
#define APR_DEST_MAX   2

#if defined(CONFIG_MSM_QDSP6_APRV2_GLINK) || \
	defined(CONFIG_MSM_QDSP6_APRV3_GLINK)
#define APR_MAX_BUF			512
#define APR_NUM_OF_TX_BUF		20
#else
#define APR_MAX_BUF			8092
#endif

#define APR_DEFAULT_NUM_OF_INTENTS 20

#define APR_OPEN_TIMEOUT_MS 5000

enum {
	/* If client sets the pkt_owner to APR_PKT_OWNER_DRIVER, APR
	 * driver will allocate a buffer, where the user packet is
	 * copied into, for each and every single Tx transmission.
	 * The buffer is thereafter passed to underlying link layer
	 * and freed upon the notification received from the link layer
	 * that the packet has been consumed.
	 */
	APR_PKT_OWNER_DRIVER,
	/* If client sets the pkt_owner to APR_PKT_OWNER_CLIENT, APR
	 * will pass the user packet memory address directly to underlying
	 * link layer. In this case it is the client's responsibility to
	 * make sure the packet is intact until being notified that the
	 * packet has been consumed.
	 */
	APR_PKT_OWNER_CLIENT,
};

struct apr_pkt_priv {
	/* This property is only applicable for APR over Glink.
	 * It is ignored in APR over SMD cases.
	 */
	uint8_t pkt_owner;
};

typedef void (*apr_svc_cb_fn)(void *buf, int len, void *priv);
struct apr_svc_ch_dev *apr_tal_open(uint32_t svc, uint32_t dest,
			uint32_t dl, apr_svc_cb_fn func, void *priv);
int apr_tal_write(struct apr_svc_ch_dev *apr_ch, void *data,
		struct apr_pkt_priv *pkt_priv, int len);
int apr_tal_close(struct apr_svc_ch_dev *apr_ch);
int apr_tal_rx_intents_config(struct apr_svc_ch_dev *apr_ch,
		int num_of_intents, uint32_t size);
int apr_tal_init(void);


#if defined(CONFIG_MSM_QDSP6_APRV2_GLINK) || \
	 defined(CONFIG_MSM_QDSP6_APRV3_GLINK)
int apr_tal_start_rx_rt(struct apr_svc_ch_dev *apr_ch);
int apr_tal_end_rx_rt(struct apr_svc_ch_dev *apr_ch);

struct apr_svc_ch_dev {
	void               *handle;
	spinlock_t         w_lock;
	spinlock_t         r_lock;
	struct mutex       m_lock;
	apr_svc_cb_fn      func;
	wait_queue_head_t  wait;
	void               *priv;
	unsigned int       channel_state;
	bool               if_remote_intent_ready;
};
#else

static inline int apr_tal_start_rx_rt(struct apr_svc_ch_dev *apr_ch)
{
	return 0;
}
static inline int apr_tal_end_rx_rt(struct apr_svc_ch_dev *apr_ch)
{
	return 0;
}
struct apr_svc_ch_dev {
	struct smd_channel *ch;
	spinlock_t         lock;
	spinlock_t         w_lock;
	struct mutex       m_lock;
	apr_svc_cb_fn      func;
	char               data[APR_MAX_BUF];
	wait_queue_head_t  wait;
	void               *priv;
	uint32_t           smd_state;
	wait_queue_head_t  dest;
	uint32_t           dest_state;
};
#endif

#endif
