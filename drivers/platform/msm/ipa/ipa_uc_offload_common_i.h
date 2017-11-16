/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/ipa_mhi.h>
#include <linux/ipa_qmi_service_v01.h>

#ifndef _IPA_UC_OFFLOAD_COMMON_I_H_
#define _IPA_UC_OFFLOAD_COMMON_I_H_

int ipa_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *in,
	ipa_notify_cb notify, void *priv, u8 hdr_len,
	struct ipa_ntn_conn_out_params *outp);
int ipa_tear_down_uc_offload_pipes(int ipa_ep_idx_ul, int ipa_ep_idx_dl);

int ipa_ntn_uc_reg_rdyCB(void (*ipauc_ready_cb)(void *user_data),
			      void *user_data);
void ipa_ntn_uc_dereg_rdyCB(void);
#endif /* _IPA_UC_OFFLOAD_COMMON_I_H_ */
