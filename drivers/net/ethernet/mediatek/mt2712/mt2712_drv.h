/* Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MT2712_DRV_H__
#define __MT2712_DRV_H__

extern unsigned long dwc_eth_qos_platform_base_addr;

irqreturn_t ISR_SW_ETH(int irq, void *device_id);
int poll(struct prv_data *pdata, int budget, int q_inx);
inline unsigned int mtk_eth_reg_read(unsigned long addr);
#ifdef QUEUE_SELECT_ALGO
u16	select_queue(struct net_device *dev, struct sk_buff *skb);
#endif
int send_frame(struct prv_data *pdata, int num);

#endif

