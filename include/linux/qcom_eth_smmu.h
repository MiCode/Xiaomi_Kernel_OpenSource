/* Copyright (c) 2020 The Linux Foundation. All rights reserved.
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

#ifndef _QCOM_ETH_SMMU_H_
#define _QCOM_ETH_SMMU_H_

#include <linux/pci.h>


int qcom_smmu_register(struct pci_driver *pdrv);
void qcom_smmu_unregister(struct pci_driver *pdrv);


#endif // _QCOM_ETH_SMMU_H_

