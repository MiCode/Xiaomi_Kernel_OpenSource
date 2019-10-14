/* Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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

#ifndef _ATL_QCOM_H_
#define _ATL_QCOM_H_

#include <linux/pci.h>

#ifdef CONFIG_AQFWD_QCOM

int atl_qcom_register(struct pci_driver *pdrv);
void atl_qcom_unregister(struct pci_driver *pdrv);

#else

static inline int atl_qcom_register(struct pci_driver *pdrv) { return 0; }
static inline void atl_qcom_unregister(struct pci_driver *pdrv) { return; }

#endif // CONFIG_AQFWD_QCOM

#endif // _ATL_QCOM_H_
