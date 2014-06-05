/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _EMAC_PTP_H_
#define _EMAC_PTP_H_

#define DEFAULT_RTC_REF_CLKRATE	125000000

int emac_ptp_init(struct net_device *netdev);
void emac_ptp_remove(struct net_device *netdev);
int emac_ptp_config(struct emac_hw *hw);
int emac_ptp_stop(struct emac_hw *hw);
int emac_ptp_set_linkspeed(struct emac_hw *hw, u32 speed);
int emac_tstamp_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd);

#endif /* _EMAC_PTP_H_ */
