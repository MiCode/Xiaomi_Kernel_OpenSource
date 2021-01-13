/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved
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
