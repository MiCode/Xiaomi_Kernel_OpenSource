// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019,2021, The Linux Foundation. All rights reserved.
 */

#include <linux/soc/qcom/qmi.h>
#include "adsp_lpm_voting_v01.h"

struct qmi_elem_info prod_set_lpm_vote_req_msg_v01_ei[] = {
	{
		.data_type     = QMI_UNSIGNED_1_BYTE,
		.elem_len      = 1,
		.elem_size     = sizeof(u8),
		.array_type    = NO_ARRAY,
		.tlv_type      = 0x01,
		.offset        = offsetof(struct prod_set_lpm_vote_req_msg_v01,
					   keep_adsp_out_of_lpm),
	},
	{
		.data_type     = QMI_EOTI,
		.array_type    = NO_ARRAY,
	},
};

struct qmi_elem_info prod_set_lpm_vote_resp_msg_v01_ei[] = {
	{
		.data_type     = QMI_STRUCT,
		.elem_len      = 1,
		.elem_size     = sizeof(struct qmi_response_type_v01),
		.array_type    = NO_ARRAY,
		.tlv_type      = 0x02,
		.offset        = offsetof(struct prod_set_lpm_vote_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type     = QMI_EOTI,
		.array_type    = NO_ARRAY,
	},
};
