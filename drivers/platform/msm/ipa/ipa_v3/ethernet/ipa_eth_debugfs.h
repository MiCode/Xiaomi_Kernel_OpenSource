/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#ifndef _IPA_ETH_DEBUGFS_H_
#define _IPA_ETH_DEBUGFS_H_

#include "ipa_eth_i.h"

int ipa_eth_debugfs_add_device(struct ipa_eth_device *eth_dev);
void ipa_eth_debugfs_remove_device(struct ipa_eth_device *eth_dev);

int ipa_eth_debugfs_add_offload_driver(struct ipa_eth_offload_driver *od);
void ipa_eth_debugfs_remove_offload_driver(struct ipa_eth_offload_driver *od);

int ipa_eth_debugfs_init(void);
void ipa_eth_debugfs_cleanup(void);

#endif /* _IPA_ETH_DEBUGFS_H_ */
