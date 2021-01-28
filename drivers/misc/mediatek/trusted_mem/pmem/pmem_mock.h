/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef PMEM_MOCK_H
#define PMEM_MOCK_H

#include "private/tmem_device.h"

#ifdef PMEM_MOCK_OBJECT_SUPPORT
void get_mocked_peer_ops(struct trusted_driver_operations **ops);
void get_mocked_ssmr_ops(struct ssmr_operations **ops);
#endif

#endif /* end of PMEM_MOCK_H */
