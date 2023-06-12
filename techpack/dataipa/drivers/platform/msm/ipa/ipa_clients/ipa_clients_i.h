/* SPDX-License-Identifier: GPL-2.0-only */
/*
* Copyright (c) 2020, The Linux Foundation. All rights reserved.
*/

#if !defined(_IPA_CLIENTS_I_H)
#define _IPA_CLIENTS_I_H

int ipa3_usb_init(void);
void ipa3_usb_exit(void);

void ipa_wdi3_register(void);

void ipa_gsb_register(void);

void ipa_odu_bridge_register(void);

void ipa_uc_offload_register(void);

void ipa_mhi_register(void);

void ipa_wigig_register(void);

#endif /* _IPA_CLIENTS_I_H */
