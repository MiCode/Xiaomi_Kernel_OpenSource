/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#ifndef __IMP_H_
#define __IMP_H_

#ifdef CONFIG_IPA3_MHI_PROXY

#include "ipa_qmi_service.h"

void imp_handle_modem_ready(void);

struct ipa_mhi_alloc_channel_resp_msg_v01 *imp_handle_allocate_channel_req(
	struct ipa_mhi_alloc_channel_req_msg_v01 *req);

struct ipa_mhi_clk_vote_resp_msg_v01 *imp_handle_vote_req(bool vote);

void imp_handle_modem_shutdown(void);

#else /* CONFIG_IPA3_MHI_PROXY */

static inline void imp_handle_modem_ready(void)
{

}

static inline struct ipa_mhi_alloc_channel_resp_msg_v01
	*imp_handle_allocate_channel_req(
		struct ipa_mhi_alloc_channel_req_msg_v01 *req)
{
		return NULL;
}

static inline struct ipa_mhi_clk_vote_resp_msg_v01
	*imp_handle_vote_req(bool vote)
{
	return NULL;
}

static inline  void imp_handle_modem_shutdown(void)
{

}

#endif /* CONFIG_IPA3_MHI_PROXY */

#endif /* __IMP_H_ */
