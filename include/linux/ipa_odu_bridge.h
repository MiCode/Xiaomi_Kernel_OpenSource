/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_ODO_BRIDGE_H_
#define _IPA_ODO_BRIDGE_H_

#if defined CONFIG_IPA || defined CONFIG_IPA3

int odu_bridge_init(struct odu_bridge_params *params);

int odu_bridge_connect(void);

int odu_bridge_disconnect(void);

int odu_bridge_tx_dp(struct sk_buff *skb, struct ipa_tx_meta *metadata);

int odu_bridge_cleanup(void);

#endif /* CONFIG_IPA || defined CONFIG_IPA3 */

#endif /* _IPA_ODO_BRIDGE_H */
