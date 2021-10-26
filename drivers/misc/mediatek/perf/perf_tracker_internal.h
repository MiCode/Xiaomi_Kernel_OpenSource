/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef _PERF_TRACKER_INTERNAL_H
#define _PERF_TRACKER_INTERNAL_H

#include <net/sock.h>
#include <linux/skbuff.h>

#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
extern void MTKGPUPower_model_start(unsigned int interval_ns);
extern void MTKGPUPower_model_stop(void);
extern void MTKGPUPower_model_suspend(void);
extern void MTKGPUPower_model_resume(void);
void perf_update_gpu_counter(unsigned int gpu_pmu[], unsigned int len);
#endif

#ifdef CONFIG_MTK_SKB_OWNER
extern void
perf_update_tcp_rtt(struct sock *sk, long seq_rtt_us);
extern void
perf_net_pkt_trace(struct sock *sk, struct sk_buff *skb, int copied);
#else
static inline void
perf_update_tcp_rtt(struct sock *sk, long seq_rtt_us) {}
static inline void
perf_net_pkt_trace(struct sock *sk, struct sk_buff *skb, int copied) {}
#endif

#endif /* _PERF_TRACKER_INTERNAL_H */
