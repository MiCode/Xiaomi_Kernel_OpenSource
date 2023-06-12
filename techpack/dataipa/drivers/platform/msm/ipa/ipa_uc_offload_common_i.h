/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/ipa_mhi.h>
#include <linux/ipa_qmi_service_v01.h>

#ifndef _IPA_UC_OFFLOAD_COMMON_I_H_
#define _IPA_UC_OFFLOAD_COMMON_I_H_

int ipa3_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *in,
	ipa_notify_cb notify, void *priv, u8 hdr_len,
	struct ipa_ntn_conn_out_params *outp);

int ipa3_tear_down_uc_offload_pipes(int ipa_ep_idx_ul, int ipa_ep_idx_dl,
	struct ipa_ntn_conn_in_params *params);

int ipa3_ntn_uc_reg_rdyCB(void (*ipauc_ready_cb)(void *user_data),
			      void *user_data);
void ipa3_ntn_uc_dereg_rdyCB(void);
#endif /* _IPA_UC_OFFLOAD_COMMON_I_H_ */
