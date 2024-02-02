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

#ifndef _ATL_QCOM_IPA_H_
#define _ATL_QCOM_IPA_H_

#include <linux/pci.h>

#ifdef CONFIG_AQFWD_QCOM_IPA

int atl_qcom_ipa_register(struct pci_driver *drv);
void atl_qcom_ipa_unregister(struct pci_driver *drv);

#else

static inline int atl_qcom_ipa_register(struct pci_driver *drv)
{
	return 0;
}

static inline void atl_qcom_ipa_unregister(struct pci_driver *drv)
{ }

#endif /* CONFIG_AQFWD_QCOM_IPA */

#endif /* _ATL_QCOM_IPA_H_ */
