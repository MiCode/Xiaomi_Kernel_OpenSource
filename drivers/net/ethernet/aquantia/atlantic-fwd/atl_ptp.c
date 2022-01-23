// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2014-2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "atl_ptp.h"

#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
#include <linux/ptp_clock_kernel.h>
#include <linux/ptp_classify.h>
#include <linux/clocksource.h>
#endif

#include "atl_ring.h"
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
#include "atl_common.h"
#include "atl_ethtool.h"
#include "atl_hw_ptp.h"
#include "atl_ring_desc.h"

#define ATL_PTP_TX_TIMEOUT        (HZ *  10)

#define POLL_SYNC_TIMER_MS 15

#define MAX_PTP_GPIO_COUNT 4

#define PTP_8TC_RING_IDX             8
#define PTP_4TC_RING_IDX            16
#define PTP_HWTS_RING_IDX           31

enum ptp_perout_action {
	ptp_perout_disabled = 0,
	ptp_perout_enabled,
	ptp_perout_pps,
};

enum ptp_speed_offsets {
	ptp_offset_idx_10 = 0,
	ptp_offset_idx_100,
	ptp_offset_idx_1000,
	ptp_offset_idx_2500,
	ptp_offset_idx_5000,
	ptp_offset_idx_10000,
};

struct ptp_skb_ring {
	struct sk_buff **buff;
	spinlock_t lock;
	unsigned int size;
	unsigned int head;
	unsigned int tail;
};

struct ptp_tx_timeout {
	spinlock_t lock;
	bool active;
	unsigned long tx_start;
};

enum atl_ptp_queue {
	ATL_PTPQ_PTP = 0,
	ATL_PTPQ_HWTS = 1,
	ATL_PTPQ_NUM,
};

struct atl_ptp {
	struct atl_nic *nic;
	struct hwtstamp_config hwtstamp_config;
	spinlock_t ptp_lock;
	spinlock_t ptp_ring_lock;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_info;

	atomic_t offset_egress;
	atomic_t offset_ingress;

	struct ptp_tx_timeout ptp_tx_timeout;

	struct napi_struct *napi;
	unsigned int idx_vector;

	struct atl_queue_vec qvec[ATL_PTPQ_NUM];

	struct ptp_skb_ring skb_ring;

	s8 udp_filter_idx;
	s8 eth_type_filter_idx;

	struct delayed_work poll_sync;
	u32 poll_timeout_ms;

	bool extts_pin_enabled;
	u64 last_sync1588_ts;
};

#define atl_for_each_ptp_qvec(ptp, qvec)			\
	for (qvec = &ptp->qvec[0];				\
	     qvec < &ptp->qvec[ATL_PTPQ_NUM]; qvec++)

struct ptp_tm_offset {
	unsigned int mbps;
	int egress;
	int ingress;
};

static struct ptp_tm_offset ptp_offset[6];

static int atl_ptp_pps_reconfigure(struct atl_ptp *ptp);

static int __atl_ptp_skb_put(struct ptp_skb_ring *ring, struct sk_buff *skb)
{
	unsigned int next_head = (ring->head + 1) % ring->size;

	if (next_head == ring->tail)
		return -ENOMEM;

	ring->buff[ring->head] = skb_get(skb);
	ring->head = next_head;

	return 0;
}

static int atl_ptp_skb_put(struct ptp_skb_ring *ring, struct sk_buff *skb)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ring->lock, flags);
	ret = __atl_ptp_skb_put(ring, skb);
	spin_unlock_irqrestore(&ring->lock, flags);

	return ret;
}

static struct sk_buff *__atl_ptp_skb_get(struct ptp_skb_ring *ring)
{
	struct sk_buff *skb;

	if (ring->tail == ring->head)
		return NULL;

	skb = ring->buff[ring->tail];
	ring->tail = (ring->tail + 1) % ring->size;

	return skb;
}

static struct sk_buff *atl_ptp_skb_get(struct ptp_skb_ring *ring)
{
	unsigned long flags;
	struct sk_buff *skb;

	spin_lock_irqsave(&ring->lock, flags);
	skb = __atl_ptp_skb_get(ring);
	spin_unlock_irqrestore(&ring->lock, flags);

	return skb;
}

static unsigned int atl_ptp_skb_buf_len(struct ptp_skb_ring *ring)
{
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&ring->lock, flags);
	len = (ring->head >= ring->tail) ?
		ring->head - ring->tail :
		ring->size - ring->tail + ring->head;
	spin_unlock_irqrestore(&ring->lock, flags);

	return len;
}

static int atl_ptp_skb_ring_init(struct ptp_skb_ring *ring, unsigned int size)
{
	struct sk_buff **buff = kcalloc(size, sizeof(*buff), GFP_KERNEL);

	if (!buff)
		return -ENOMEM;

	spin_lock_init(&ring->lock);

	ring->buff = buff;
	ring->size = size;
	ring->head = 0;
	ring->tail = 0;

	return 0;
}

static void atl_ptp_skb_ring_clean(struct ptp_skb_ring *ring)
{
	struct sk_buff *skb;

	while ((skb = atl_ptp_skb_get(ring)) != NULL)
		dev_kfree_skb_any(skb);
}

static void atl_ptp_skb_ring_release(struct ptp_skb_ring *ring)
{
	if (ring->buff) {
		atl_ptp_skb_ring_clean(ring);
		kfree(ring->buff);
		ring->buff = NULL;
	}
}

static void atl_ptp_tx_timeout_init(struct ptp_tx_timeout *timeout)
{
	spin_lock_init(&timeout->lock);
	timeout->active = false;
}

static void atl_ptp_tx_timeout_start(struct atl_ptp *ptp)
{
	struct ptp_tx_timeout *timeout = &ptp->ptp_tx_timeout;
	unsigned long flags;

	spin_lock_irqsave(&timeout->lock, flags);
	timeout->active = true;
	timeout->tx_start = jiffies;
	spin_unlock_irqrestore(&timeout->lock, flags);
}

static void atl_ptp_tx_timeout_update(struct atl_ptp *ptp)
{
	if (!atl_ptp_skb_buf_len(&ptp->skb_ring)) {
		struct ptp_tx_timeout *timeout = &ptp->ptp_tx_timeout;
		unsigned long flags;

		spin_lock_irqsave(&timeout->lock, flags);
		timeout->active = false;
		spin_unlock_irqrestore(&timeout->lock, flags);
	}
}

static void atl_ptp_tx_timeout_check(struct atl_ptp *ptp)
{
	struct ptp_tx_timeout *timeout = &ptp->ptp_tx_timeout;
	struct atl_nic *nic = ptp->nic;
	unsigned long flags;
	bool timeout_flag;

	timeout_flag = false;

	spin_lock_irqsave(&timeout->lock, flags);
	if (timeout->active) {
		timeout_flag = time_is_before_jiffies(timeout->tx_start +
						      ATL_PTP_TX_TIMEOUT);
		/* reset active flag if timeout detected */
		if (timeout_flag)
			timeout->active = false;
	}
	spin_unlock_irqrestore(&timeout->lock, flags);

	if (timeout_flag) {
		atl_nic_err("PTP Timeout. Clearing Tx Timestamp SKBs\n");
		atl_ptp_skb_ring_clean(&ptp->skb_ring);
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
/* atl_ptp_adjfine
 * @ptp_info: the ptp clock structure
 * @ppb: parts per billion adjustment from base
 *
 * adjust the frequency of the ptp cycle counter by the
 * indicated ppb from the base frequency.
 */
static int atl_ptp_adjfine(struct ptp_clock_info *ptp_info, long scaled_ppm)
{
	struct atl_ptp *ptp = container_of(ptp_info, struct atl_ptp, ptp_info);
	struct atl_nic *nic = ptp->nic;

	hw_atl_adj_clock_freq(&nic->hw, scaled_ppm_to_ppb(scaled_ppm));

	return 0;
}
#endif

/* atl_ptp_adjfreq
 * @ptp_info: the ptp clock structure
 * @ppb: parts per billion adjustment from base
 *
 * adjust the frequency of the ptp cycle counter by the
 * indicated ppb from the base frequency.
 */
static int atl_ptp_adjfreq(struct ptp_clock_info *ptp_info, s32 ppb)
{
	struct atl_ptp *ptp = container_of(ptp_info, struct atl_ptp, ptp_info);
	struct atl_nic *nic = ptp->nic;

	hw_atl_adj_clock_freq(&nic->hw, ppb);

	return 0;
}

/* atl_ptp_adjtime
 * @ptp_info: the ptp clock structure
 * @delta: offset to adjust the cycle counter by
 *
 * adjust the timer by resetting the timecounter structure.
 */
static int atl_ptp_adjtime(struct ptp_clock_info *ptp_info, s64 delta)
{
	struct atl_ptp *ptp = container_of(ptp_info, struct atl_ptp, ptp_info);
	struct atl_nic *nic = ptp->nic;
	unsigned long flags;

	spin_lock_irqsave(&ptp->ptp_lock, flags);
	hw_atl_adj_sys_clock(&nic->hw, delta);
	spin_unlock_irqrestore(&ptp->ptp_lock, flags);

	atl_ptp_pps_reconfigure(ptp);

	return 0;
}

/* atl_ptp_gettime
 * @ptp_info: the ptp clock structure
 * @ts: timespec structure to hold the current time value
 *
 * read the timecounter and return the correct value on ns,
 * after converting it into a struct timespec.
 */
static int atl_ptp_gettime(struct ptp_clock_info *ptp_info, struct timespec64 *ts)
{
	struct atl_ptp *ptp = container_of(ptp_info, struct atl_ptp, ptp_info);
	struct atl_nic *nic = ptp->nic;
	unsigned long flags;
	u64 ns;

	spin_lock_irqsave(&ptp->ptp_lock, flags);
	hw_atl_get_ptp_ts(&nic->hw, &ns);
	spin_unlock_irqrestore(&ptp->ptp_lock, flags);

	*ts = ns_to_timespec64(ns);

	return 0;
}

/* atl_ptp_settime
 * @ptp_info: the ptp clock structure
 * @ts: the timespec containing the new time for the cycle counter
 *
 * reset the timecounter to use a new base value instead of the kernel
 * wall timer value.
 */
static int atl_ptp_settime(struct ptp_clock_info *ptp_info,
			  const struct timespec64 *ts)
{
	struct atl_ptp *ptp = container_of(ptp_info, struct atl_ptp, ptp_info);
	struct atl_nic *nic = ptp->nic;
	unsigned long flags;
	u64 ns = timespec64_to_ns(ts);
	u64 now;

	spin_lock_irqsave(&ptp->ptp_lock, flags);
	hw_atl_get_ptp_ts(&nic->hw, &now);
	hw_atl_adj_sys_clock(&nic->hw, (s64)ns - (s64)now);
	spin_unlock_irqrestore(&ptp->ptp_lock, flags);

	atl_ptp_pps_reconfigure(ptp);

	return 0;
}

static void atl_ptp_convert_to_hwtstamp(struct skb_shared_hwtstamps *hwtstamp,
					u64 timestamp)
{
	memset(hwtstamp, 0, sizeof(*hwtstamp));
	hwtstamp->hwtstamp = ns_to_ktime(timestamp);
}

static int atl_ptp_hw_pin_conf(struct atl_nic *nic, u32 pin_index, u64 start,
			       u64 period)
{
	if (period)
		atl_nic_dbg("Enable GPIO %d pulsing, start time %llu, period %u\n",
			    pin_index, start, (u32)period);
	else
		atl_nic_dbg("Disable GPIO %d pulsing, start time %llu, period %u\n",
			    pin_index, start, (u32)period);

	/* Notify hardware of request to being sending pulses.
	 * If period is ZERO then pulsen is disabled.
	 */
	hw_atl_gpio_pulse(&nic->hw, pin_index, start, (u32)period);

	return 0;
}

static int atl_ptp_perout_pin_configure(struct ptp_clock_info *ptp_clock,
					struct ptp_clock_request *rq, int on)
{
	struct atl_ptp *ptp = container_of(ptp_clock, struct atl_ptp, ptp_info);
	struct ptp_clock_time *t = &rq->perout.period;
	struct ptp_clock_time *s = &rq->perout.start;
	u32 pin_index = rq->perout.index;
	struct atl_nic *nic = ptp->nic;
	u64 start, period;

	/* verify the request channel is there */
	if (pin_index >= ptp_clock->n_per_out)
		return -EINVAL;

	/* we cannot support periods greater
	 * than 4 seconds due to reg limit
	 */
	if (t->sec > 4 || t->sec < 0)
		return -ERANGE;

	/* convert to unsigned 64b ns,
	 * verify we can put it in a 32b register
	 */
	period = on ? t->sec * NSEC_PER_SEC + t->nsec : 0;

	/* verify the value is in range supported by hardware */
	if (period > U32_MAX)
		return -ERANGE;
	/* convert to unsigned 64b ns */
	/* TODO convert to AQ time */
	start = on ? s->sec * NSEC_PER_SEC + s->nsec : 0;

	atl_ptp_hw_pin_conf(nic, pin_index, start, period);
	ptp->ptp_info.pin_config[pin_index].rsv[2] = on ? ptp_perout_enabled :
							  ptp_perout_disabled;

	return 0;
}

static int atl_ptp_pps_pin_configure(struct ptp_clock_info *ptp_clock,
				     struct ptp_clock_request *rq, int on)
{
	struct atl_ptp *ptp = container_of(ptp_clock, struct atl_ptp, ptp_info);
	struct atl_nic *nic = ptp->nic;
	u64 start, period;
	u32 pin_index = 0;
	u32 rest = 0;

	/* verify the request channel is there */
	if (pin_index >= ptp_clock->n_per_out)
		return -EINVAL;

	hw_atl_get_ptp_ts(&nic->hw, &start);
	div_u64_rem(start, NSEC_PER_SEC, &rest);
	period = on ? NSEC_PER_SEC : 0; /* PPS - pulse per second */
	start = on ? start - rest + NSEC_PER_SEC *
		(rest > 990000000LL ? 2 : 1) : 0;

	atl_ptp_hw_pin_conf(nic, pin_index, start, period);
	ptp->ptp_info.pin_config[pin_index].rsv[2] = on ? ptp_perout_pps :
							  ptp_perout_disabled;

	return 0;
}

static int atl_ptp_pps_reconfigure(struct atl_ptp *ptp)
{
	struct atl_nic *nic = ptp->nic;
	u64 start, period;
	u32 rest = 0;
	int i;

	for (i = 0; i < ptp->ptp_info.n_pins; i++)
		if ((ptp->ptp_info.pin_config[i].func == PTP_PF_PEROUT) &&
		    (ptp->ptp_info.pin_config[i].rsv[2] == ptp_perout_pps)) {

			hw_atl_get_ptp_ts(&nic->hw, &start);
			div_u64_rem(start, NSEC_PER_SEC, &rest);
			period = NSEC_PER_SEC;
			start = start - rest + NSEC_PER_SEC * (rest > 990000000LL ? 2 : 1);

			atl_ptp_hw_pin_conf(nic, i, start, period);
		}

	return 0;

}

static void atl_ptp_extts_pin_ctrl(struct atl_ptp *ptp)
{
	struct atl_nic *nic = ptp->nic;
	u32 enable = ptp->extts_pin_enabled;

	hw_atl_extts_gpio_enable(&nic->hw, 0, enable);
}

static int atl_ptp_extts_pin_configure(struct ptp_clock_info *ptp_clock,
				       struct ptp_clock_request *rq, int on)
{
	struct atl_ptp *ptp = container_of(ptp_clock, struct atl_ptp, ptp_info);

	u32 pin_index = rq->extts.index;

	if (pin_index >= ptp_clock->n_ext_ts)
		return -EINVAL;

	ptp->extts_pin_enabled = !!on;
	if (on) {
		ptp->poll_timeout_ms = POLL_SYNC_TIMER_MS;
		cancel_delayed_work_sync(&ptp->poll_sync);
		schedule_delayed_work(&ptp->poll_sync,
				      msecs_to_jiffies(ptp->poll_timeout_ms));
	}

	atl_ptp_extts_pin_ctrl(ptp);
	return 0;
}

/* atl_ptp_gpio_feature_enable
 * @ptp: the ptp clock structure
 * @rq: the requested feature to change
 * @on: whether to enable or disable the feature
 */
static int atl_ptp_gpio_feature_enable(struct ptp_clock_info *ptp,
				       struct ptp_clock_request *rq, int on)
{
	switch (rq->type) {
	case PTP_CLK_REQ_EXTTS:
		return atl_ptp_extts_pin_configure(ptp, rq, on);
	case PTP_CLK_REQ_PEROUT:
		return atl_ptp_perout_pin_configure(ptp, rq, on);
	case PTP_CLK_REQ_PPS:
		return atl_ptp_pps_pin_configure(ptp, rq, on);
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

/* atl_ptp_verify
 * @ptp: the ptp clock structure
 * @pin: index of the pin in question
 * @func: the desired function to use
 * @chan: the function channel index to use
 */
static int atl_ptp_verify(struct ptp_clock_info *ptp, unsigned int pin,
			  enum ptp_pin_function func, unsigned int chan)
{
	/* verify the requested pin is there */
	if (!ptp->pin_config || pin >= ptp->n_pins)
		return -EINVAL;

	/* enforce locked channels, no changing them */
	if (chan != ptp->pin_config[pin].chan)
		return -EINVAL;

	/* we want to keep the functions locked as well */
	if (func != ptp->pin_config[pin].func)
		return -EINVAL;

	return 0;
}

/* atl_ptp_rx_hwtstamp - utility function which checks for RX time stamp
 * @skb: particular skb to send timestamp with
 *
 * if the timestamp is valid, we convert it into the timecounter ns
 * value, then store that result into the hwtstamps structure which
 * is passed up the network stack
 */
static void atl_ptp_rx_hwtstamp(struct atl_ptp *ptp, struct sk_buff *skb,
				u64 timestamp)
{
	timestamp -= atomic_read(&ptp->offset_ingress);
	atl_ptp_convert_to_hwtstamp(skb_hwtstamps(skb), timestamp);
}

static int atl_ptp_ring_index(enum atl_ptp_queue ptp_queue)
{
	switch (ptp_queue) {
	case ATL_PTPQ_PTP:
		/* multi-TC is not supported in FWD driver, so tc mode is
		 * always set to 4 TCs (each with 8 queues) for now
		 */
		return PTP_4TC_RING_IDX;
	case ATL_PTPQ_HWTS:
		return PTP_HWTS_RING_IDX;
	default:
		break;
	}

	WARN_ONCE(1, "Invalid ptp_queue");
	return 0;
}

static int atl_ptp_poll(struct napi_struct *napi, int budget)
{
	struct atl_queue_vec *qvec = container_of(napi, struct atl_queue_vec, napi);
	struct atl_ptp *ptp = qvec->nic->ptp;
	int work_done = 0;

	/* Processing PTP TX and RX traffic */
	work_done = atl_poll_qvec(&ptp->qvec[ATL_PTPQ_PTP], budget);

	/* Processing HW_TIMESTAMP RX traffic */
	atl_clean_hwts_rx(&ptp->qvec[ATL_PTPQ_HWTS].rx, budget);

	if (work_done < budget) {
		napi_complete_done(ptp->napi, work_done);
		atl_intr_enable(&qvec->nic->hw, BIT(atl_qvec_intr(qvec)));
		/* atl_set_intr_throttle(&nic->hw, qvec->idx); */
	}

	return work_done;
}

static struct ptp_clock_info atl_ptp_clock = {
	.owner		= THIS_MODULE,
	.name		= "atlantic ptp",
	.max_adj	= 999999999,
	.n_ext_ts	= 0,
	.pps		= 0,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
	.adjfine	= atl_ptp_adjfine,
#endif
	.adjfreq	= atl_ptp_adjfreq,
	.adjtime	= atl_ptp_adjtime,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	.gettime64	= atl_ptp_gettime,
	.settime64	= atl_ptp_settime,
#else
	.gettime	= atl_ptp_gettime,
	.settime	= atl_ptp_settime,
#endif
	.n_per_out	= 0,
	.enable		= atl_ptp_gpio_feature_enable,
	.n_pins		= 0,
	.verify		= atl_ptp_verify,
	.pin_config	= NULL,
};

#define ptp_offset_init(__idx, __mbps, __egress, __ingress)   do { \
		ptp_offset[__idx].mbps = (__mbps); \
		ptp_offset[__idx].egress = (__egress); \
		ptp_offset[__idx].ingress = (__ingress); } \
		while (0)

static void atl_ptp_offset_init_from_fw(const struct atl_ptp_offset_info *offsets)
{
	int i;

	/* Load offsets for PTP */
	for (i = 0; i < ARRAY_SIZE(ptp_offset); i++) {
		switch (i) {
		/* 100M */
		case ptp_offset_idx_100:
			ptp_offset_init(i, 100,
					offsets->egress_100,
					offsets->ingress_100);
			break;
		/* 1G */
		case ptp_offset_idx_1000:
			ptp_offset_init(i, 1000,
					offsets->egress_1000,
					offsets->ingress_1000);
			break;
		/* 2.5G */
		case ptp_offset_idx_2500:
			ptp_offset_init(i, 2500,
					offsets->egress_2500,
					offsets->ingress_2500);
			break;
		/* 5G */
		case ptp_offset_idx_5000:
			ptp_offset_init(i, 5000,
					offsets->egress_5000,
					offsets->ingress_5000);
			break;
		/* 10G */
		case ptp_offset_idx_10000:
			ptp_offset_init(i, 10000,
					offsets->egress_10000,
					offsets->ingress_10000);
			break;
		}
	}
}

static void atl_ptp_offset_init(const struct atl_ptp_offset_info *offsets)
{
	memset(ptp_offset, 0, sizeof(ptp_offset));

	atl_ptp_offset_init_from_fw(offsets);
}

static void atl_ptp_gpio_init(struct atl_nic *nic,
			      struct ptp_clock_info *info,
			      enum atl_gpio_pin_function *gpio_pin)
{
	struct ptp_pin_desc pin_desc[MAX_PTP_GPIO_COUNT];
	u32 extts_pin_cnt = 0;
	u32 out_pin_cnt = 0;
	u32 i;

	memset(pin_desc, 0, sizeof(pin_desc));

	for (i = 0; i < MAX_PTP_GPIO_COUNT - 1; i++) {
		if (gpio_pin[i] ==
		    (GPIO_PIN_FUNCTION_PTP0 + out_pin_cnt)) {
			snprintf(pin_desc[out_pin_cnt].name,
				 sizeof(pin_desc[out_pin_cnt].name),
				 "AQ_GPIO%d", i);
			pin_desc[out_pin_cnt].index = out_pin_cnt;
			pin_desc[out_pin_cnt].chan = out_pin_cnt;
			pin_desc[out_pin_cnt++].func = PTP_PF_PEROUT;
		}
	}

	info->n_per_out = out_pin_cnt;

	if (nic->hw.mcp.caps_ex & atl_fw2_ex_caps_phy_ctrl_ts_pin) {
		extts_pin_cnt += 1;

		snprintf(pin_desc[out_pin_cnt].name,
			 sizeof(pin_desc[out_pin_cnt].name),
			  "AQ_GPIO%d", out_pin_cnt);
		pin_desc[out_pin_cnt].index = out_pin_cnt;
		pin_desc[out_pin_cnt].chan = 0;
		pin_desc[out_pin_cnt].func = PTP_PF_EXTTS;
	}

	info->n_pins = out_pin_cnt + extts_pin_cnt;
	info->n_ext_ts = extts_pin_cnt;

	if (!info->n_pins)
		return;

	info->pin_config = kcalloc(info->n_pins, sizeof(struct ptp_pin_desc),
				   GFP_KERNEL);

	if (!info->pin_config)
		return;

	memcpy(info->pin_config, &pin_desc,
	       sizeof(struct ptp_pin_desc) * info->n_pins);
}

/* PTP external GPIO nanoseconds count */
static uint64_t atl_ptp_get_sync1588_ts(struct atl_nic *nic)
{
	u64 ts = 0;

	hw_atl_get_sync_ts(&nic->hw, &ts);

	return ts;
}

static void atl_ptp_start_work(struct atl_ptp *ptp)
{
	if (ptp->extts_pin_enabled) {
		ptp->poll_timeout_ms = POLL_SYNC_TIMER_MS;
		ptp->last_sync1588_ts = atl_ptp_get_sync1588_ts(ptp->nic);
		schedule_delayed_work(&ptp->poll_sync,
				      msecs_to_jiffies(ptp->poll_timeout_ms));
	}
}

static bool atl_ptp_sync_ts_updated(struct atl_ptp *ptp, u64 *new_ts)
{
	struct atl_nic *nic = ptp->nic;
	u64 sync_ts2;
	u64 sync_ts;

	sync_ts = atl_ptp_get_sync1588_ts(nic);

	if (sync_ts != ptp->last_sync1588_ts) {
		sync_ts2 = atl_ptp_get_sync1588_ts(nic);
		if (sync_ts != sync_ts2) {
			sync_ts = sync_ts2;
			sync_ts2 = atl_ptp_get_sync1588_ts(nic);
			if (sync_ts != sync_ts2) {
				atl_nic_err("%s: Unable to get correct GPIO TS",
					    __func__);
				sync_ts = 0;
			}
		}

		*new_ts = sync_ts;
		return true;
	}
	return false;
}

static int atl_ptp_check_sync1588(struct atl_ptp *ptp)
{
	struct atl_nic *nic = ptp->nic;
	u64 sync_ts;

	 /* Sync1588 pin was triggered */
	if (atl_ptp_sync_ts_updated(ptp, &sync_ts)) {
		if (ptp->extts_pin_enabled) {
			struct ptp_clock_event ptp_event;
			u64 time = 0;

			hw_atl_ts_to_sys_clock(&nic->hw, sync_ts, &time);
			ptp_event.index = ptp->ptp_info.n_pins - 1;
			ptp_event.timestamp = time;

			ptp_event.type = PTP_CLOCK_EXTTS;
			ptp_clock_event(ptp->ptp_clock, &ptp_event);
		}

		ptp->last_sync1588_ts = sync_ts;
	}

	return 0;
}

static void atl_ptp_poll_sync_work_cb(struct work_struct *w)
{
	struct delayed_work *dw = to_delayed_work(w);
	struct atl_ptp *ptp = container_of(dw, struct atl_ptp, poll_sync);

	atl_ptp_check_sync1588(ptp);

	if (ptp->extts_pin_enabled) {
		unsigned long timeout = msecs_to_jiffies(ptp->poll_timeout_ms);

		schedule_delayed_work(&ptp->poll_sync, timeout);
	}
}

#endif /* IS_REACHABLE(CONFIG_PTP_1588_CLOCK) */

irqreturn_t atl_ptp_irq(int irq, void *private)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = private;
	int err = 0;

	if (!ptp) {
		err = -EINVAL;
		goto err_exit;
	}
	napi_schedule_irqoff(ptp->napi);

err_exit:
	return err >= 0 ? IRQ_HANDLED : IRQ_NONE;
#else
	return IRQ_NONE;
#endif
}

void atl_ptp_tm_offset_set(struct atl_nic *nic, unsigned int mbps)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	int i, egress, ingress;

	if (!ptp)
		return;

	egress = 0;
	ingress = 0;

	for (i = 0; i < ARRAY_SIZE(ptp_offset); i++) {
		if (mbps == ptp_offset[i].mbps) {
			egress = ptp_offset[i].egress;
			ingress = ptp_offset[i].ingress;
			break;
		}
	}

	atomic_set(&ptp->offset_egress, egress);
	atomic_set(&ptp->offset_ingress, ingress);
#endif
}

/* atl_ptp_tx_hwtstamp - utility function which checks for TX time stamp
 *
 * if the timestamp is valid, we convert it into the timecounter ns
 * value, then store that result into the hwtstamps structure which
 * is passed up the network stack
 */
void atl_ptp_tx_hwtstamp(struct atl_nic *nic, u64 timestamp)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	struct sk_buff *skb = atl_ptp_skb_get(&ptp->skb_ring);
	struct skb_shared_hwtstamps hwtstamp;

	if (!skb) {
		atl_nic_err("have timestamp but tx_queues empty\n");
		return;
	}

	if (!timestamp) {
		atl_nic_err("ptp timestamp all-F from FW\n");
		memset(&hwtstamp, 0, sizeof(hwtstamp));
	} else {
		timestamp += atomic_read(&ptp->offset_egress);
		atl_ptp_convert_to_hwtstamp(&hwtstamp, timestamp);
	}
	do {
		skb_tstamp_tx(skb, &hwtstamp);
		dev_kfree_skb_any(skb);
		skb = atl_ptp_skb_get(&ptp->skb_ring);
	} while (skb);

	atl_ptp_tx_timeout_update(ptp);
#endif
}

void atl_ptp_hwtstamp_config_get(struct atl_nic *nic,
				 struct hwtstamp_config *config)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;

	*config = ptp->hwtstamp_config;
#endif
}

int atl_ptp_hwtstamp_config_set(struct atl_nic *nic,
				struct hwtstamp_config *config)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	static u32 ntuple_cmd =
		ATL_NTC_PROTO |
		ATL_NTC_L4_UDP |
		ATL_NTC_DP |
		ATL_RXF_ACT_TOHOST |
		ATL_NTC_RXQ;
	u32 ntuple_vec_idx =
		((ptp->qvec[ATL_PTPQ_PTP].idx << ATL_NTC_RXQ_SHIFT) & ATL_NTC_RXQ_MASK);
	static u32 etype_cmd =
		ETH_P_1588 |
		ATL_RXF_ACT_TOHOST |
		ATL_ETYPE_RXQ;
	u32 etype_vec_idx =
		((ptp->qvec[ATL_PTPQ_PTP].idx << ATL_ETYPE_RXQ_SHIFT) & ATL_ETYPE_RXQ_MASK);

	if (config->tx_type == HWTSTAMP_TX_ON ||
	    config->rx_filter == HWTSTAMP_FILTER_PTP_V2_EVENT) {
		atl_write(&nic->hw, ATL_NTUPLE_DPORT(ptp->udp_filter_idx),
			  PTP_EV_PORT);
		atl_write(&nic->hw, ATL_NTUPLE_CTRL(ptp->udp_filter_idx),
			  ATL_NTC_EN | ntuple_cmd | ntuple_vec_idx);

		atl_write(&nic->hw, ATL_RX_ETYPE_FLT(ptp->eth_type_filter_idx),
			  ATL_ETYPE_EN | etype_cmd | etype_vec_idx);

		nic->hw.link_state.ptp_datapath_up = true;
	} else {
		atl_write(&nic->hw, ATL_NTUPLE_CTRL(ptp->udp_filter_idx), ntuple_cmd);

		atl_write(&nic->hw, ATL_RX_ETYPE_FLT(ptp->eth_type_filter_idx), 0);

		nic->hw.link_state.ptp_datapath_up = false;
	}

	ptp->hwtstamp_config = *config;
#endif

	return 0;
}

int atl_ptp_qvec_intr(struct atl_queue_vec *qvec)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	int i;

	for (i = 0; i != ATL_PTPQ_NUM; i++) {
		if (qvec->idx == atl_ptp_ring_index(i))
			return ATL_NUM_NON_RING_IRQS - 1;
	}
#endif

	WARN_ONCE(1, "Not a PTP queue vector");
	return ATL_NUM_NON_RING_IRQS;
}

bool atl_is_ptp_ring(struct atl_nic *nic, struct atl_desc_ring *ring)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;

	if (!ptp)
		return false;

	return &ptp->qvec[ATL_PTPQ_PTP].tx == ring ||
	       &ptp->qvec[ATL_PTPQ_PTP].rx == ring ||
	       &ptp->qvec[ATL_PTPQ_HWTS].rx == ring;
#else
	return false;
#endif
}

u16 atl_ptp_extract_ts(struct atl_nic *nic, struct sk_buff *skb, u8 *p,
		       unsigned int len)
{
	u16 ret = 0;
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	u64 timestamp = 0;

	ret = hw_atl_rx_extract_ts(&nic->hw, p, len, &timestamp);
	if (ret > 0)
		atl_ptp_rx_hwtstamp(ptp, skb, timestamp);
#endif

	return ret;
}

netdev_tx_t atl_ptp_start_xmit(struct atl_nic *nic, struct sk_buff *skb)
{
	netdev_tx_t err = NETDEV_TX_OK;
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	struct atl_desc_ring *ring;
	unsigned long irq_flags;

	ring = &ptp->qvec[ATL_PTPQ_PTP].tx;

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		goto err_exit;
	}

	if (atl_tx_full(ring, skb_shinfo(skb)->nr_frags + 4)) {
		/* Drop packet because it doesn't make sence to delay it */
		dev_kfree_skb_any(skb);
		goto err_exit;
	}

	err = atl_ptp_skb_put(&ptp->skb_ring, skb);
	if (err) {
		atl_nic_err("SKB Ring is overflow!\n");
		return NETDEV_TX_BUSY;
	}
	skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
	atl_ptp_tx_timeout_start(ptp);
	skb_tx_timestamp(skb);

	spin_lock_irqsave(&ptp->ptp_ring_lock, irq_flags);
	err = atl_map_skb(skb, ring);
	spin_unlock_irqrestore(&ptp->ptp_ring_lock, irq_flags);

err_exit:
#endif
	return err;
}

void atl_ptp_work(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;

	if (!ptp)
		return;

	atl_ptp_tx_timeout_check(ptp);
#endif
}

int atl_ptp_irq_alloc(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct pci_dev *pdev = nic->hw.pdev;
	struct atl_ptp *ptp = nic->ptp;

	if (!ptp)
		return 0;

	if (nic->flags & ATL_FL_MULTIPLE_VECTORS) {
		return request_irq(pci_irq_vector(pdev, ptp->idx_vector),
				  atl_ptp_irq, 0, nic->ndev->name, ptp);
	}

	return 0;
#else
	return 0;
#endif
}

void atl_ptp_irq_free(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_hw *hw = &nic->hw;
	struct atl_ptp *ptp = nic->ptp;

	if (!ptp)
		return;

	atl_intr_disable(hw, BIT(ptp->idx_vector));
	if (nic->flags & ATL_FL_MULTIPLE_VECTORS)
		free_irq(pci_irq_vector(hw->pdev, ptp->idx_vector), ptp);
#endif
}

int atl_ptp_ring_alloc(struct atl_nic *nic)
{
	int err = 0;
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	struct atl_queue_vec *qvec;
	int i;

	if (!ptp)
		return 0;

	for (i = 0; i != ATL_PTPQ_NUM; i++) {
		qvec = &ptp->qvec[i];

		atl_init_qvec(nic, qvec, atl_ptp_ring_index(i));
	}

	atl_for_each_ptp_qvec(ptp, qvec) {
		err = atl_alloc_qvec(qvec);
		if (err)
			goto free;
	}

	err = atl_ptp_skb_ring_init(&ptp->skb_ring, nic->requested_rx_size);
	if (err != 0) {
		err = -ENOMEM;
		goto free;
	}

	return 0;

free:
	while (--qvec >= &ptp->qvec[0])
		atl_free_qvec(qvec);
#endif

	return err;
}

int atl_ptp_ring_start(struct atl_nic *nic)
{
	int err = 0;
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	struct atl_queue_vec *qvec;

	if (!ptp)
		return 0;

	atl_for_each_ptp_qvec(ptp, qvec) {
		err = atl_start_qvec(qvec);
		if (err)
			goto stop;
	}

	netif_napi_add(nic->ndev, ptp->napi, atl_ptp_poll, 64);
	napi_enable(ptp->napi);

	return 0;

stop:
	while (--qvec >= &ptp->qvec[0])
		atl_stop_qvec(qvec);
#endif

	return err;
}

void atl_ptp_ring_stop(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	struct atl_queue_vec *qvec;

	if (!ptp)
		return;

	napi_disable(ptp->napi);
	netif_napi_del(ptp->napi);

	atl_for_each_ptp_qvec(ptp, qvec)
		atl_stop_qvec(qvec);
#endif
}

void atl_ptp_ring_free(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;

	if (!ptp)
		return;

	atl_free_qvec(&ptp->qvec[ATL_PTPQ_PTP]);

	atl_ptp_skb_ring_release(&ptp->skb_ring);
#endif
}

void atl_ptp_clock_init(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);
	atl_ptp_settime(&ptp->ptp_info, &ts);
#endif
}

int atl_ptp_init(struct atl_nic *nic)
{
	int err = 0;
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_mcp *mcp = &nic->hw.mcp;
	struct atl_ptp *ptp;

	if (!mcp->ops->set_ptp) {
		nic->ptp = NULL;
		return 0;
	}

	if (!(mcp->caps_ex & atl_fw2_ex_caps_phy_ptp_en)) {
		nic->ptp = NULL;
		return 0;
	}

	ptp = kzalloc(sizeof(*ptp), GFP_KERNEL);
	if (!ptp) {
		err = -ENOMEM;
		goto err_exit;
	}

	ptp->nic = nic;

	ptp->qvec[ATL_PTPQ_PTP].type = ATL_QUEUE_PTP;
	ptp->qvec[ATL_PTPQ_HWTS].type = ATL_QUEUE_HWTS;

	spin_lock_init(&ptp->ptp_lock);
	spin_lock_init(&ptp->ptp_ring_lock);

	atl_ptp_tx_timeout_init(&ptp->ptp_tx_timeout);

	atomic_set(&ptp->offset_egress, 0);
	atomic_set(&ptp->offset_ingress, 0);

	ptp->napi = &ptp->qvec[ATL_PTPQ_PTP].napi;

	ptp->idx_vector = ATL_IRQ_PTP;

	nic->ptp = ptp;

	/* enable ptp counter */
	nic->hw.link_state.ptp_available = true;
	mcp->ops->set_ptp(&nic->hw, true);
	atl_ptp_clock_init(nic);

	INIT_DELAYED_WORK(&ptp->poll_sync, &atl_ptp_poll_sync_work_cb);
	ptp->eth_type_filter_idx = atl_reserve_filter(ATL_RXF_ETYPE);
	ptp->udp_filter_idx = atl_reserve_filter(ATL_RXF_NTUPLE);

	return 0;

err_exit:
	if (ptp)
		kfree(ptp->ptp_info.pin_config);
	kfree(ptp);
	nic->ptp = NULL;
#endif

	return err;
}

int atl_ptp_register(struct atl_nic *nic)
{
	int err = 0;

#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp_offset_info ptp_offset_info;
	enum atl_gpio_pin_function gpio_pin[3];
	struct atl_mcp *mcp = &nic->hw.mcp;
	struct atl_ptp *ptp = nic->ptp;
	struct ptp_clock *clock;

	if (!ptp)
		return 0;

	err = atl_read_mcp_mem(&nic->hw, mcp->fw_stat_addr + atl_fw2_stat_ptp_offset,
		&ptp_offset_info, sizeof(ptp_offset_info));
	if (err)
		return err;

	err = atl_read_mcp_mem(&nic->hw, mcp->fw_stat_addr + atl_fw2_stat_gpio_pin,
		&gpio_pin, sizeof(gpio_pin));
	if (err)
		return err;

	atl_ptp_offset_init(&ptp_offset_info);

	ptp->ptp_info = atl_ptp_clock;
	atl_ptp_gpio_init(nic, &ptp->ptp_info, &gpio_pin[0]);
	clock = ptp_clock_register(&ptp->ptp_info, &nic->ndev->dev);
	if (IS_ERR_OR_NULL(clock)) {
		netdev_err(nic->ndev, "ptp_clock_register failed\n");
		err = PTR_ERR(clock);
		goto err_exit;
	}
	ptp->ptp_clock = clock;

err_exit:
#endif

	return err;
}

void atl_ptp_unregister(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;

	if (!ptp)
		return;

	ptp_clock_unregister(ptp->ptp_clock);
#endif
}

void atl_ptp_free(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_mcp *mcp = &nic->hw.mcp;
	struct atl_ptp *ptp = nic->ptp;

	if (!ptp)
		return;

	atl_release_filter(ATL_RXF_ETYPE);
	atl_release_filter(ATL_RXF_NTUPLE);

	cancel_delayed_work_sync(&ptp->poll_sync);
	/* disable ptp */
	mcp->ops->set_ptp(&nic->hw, false);

	kfree(ptp->ptp_info.pin_config);

	kfree(ptp);
	nic->ptp = NULL;
#endif
}

struct ptp_clock *atl_ptp_get_ptp_clock(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	return nic->ptp->ptp_clock;
#else
	return NULL;
#endif
}

int atl_ptp_link_change(struct atl_nic *nic)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	struct atl_ptp *ptp = nic->ptp;
	struct atl_hw *hw = &nic->hw;

	if (!ptp)
		return 0;

	if (hw->mcp.ops->check_link(hw))
		atl_ptp_start_work(ptp);
	else
		cancel_delayed_work_sync(&ptp->poll_sync);
#endif

	return 0;
}
