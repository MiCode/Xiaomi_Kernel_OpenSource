/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ATL_IPA_H_
#define _ATL_IPA_H_

#include <linux/pci.h>

#if IS_ENABLED(CONFIG_AQFWD_QCOM_IPA)

int atl_ipa_register(struct pci_driver *drv);
void atl_ipa_unregister(struct pci_driver *drv);

#else

static inline int atl_ipa_register(struct pci_driver *drv)
{
	return 0;
}

static inline void atl_ipa_unregister(struct pci_driver *drv)
{ }

#endif /* CONFIG_AQFWD_QCOM_IPA */

#endif /* _ATL_IPA_H_ */
