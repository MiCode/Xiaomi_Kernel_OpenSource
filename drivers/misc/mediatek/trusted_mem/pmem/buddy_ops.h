/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef PMEM_MOCK_H
#define PMEM_MOCK_H

#include "private/tmem_device.h"

void get_mocked_peer_ops(struct trusted_driver_operations **ops);
void get_mocked_ssmr_ops(struct ssmr_operations **ops);

#endif /* end of PMEM_MOCK_H */
