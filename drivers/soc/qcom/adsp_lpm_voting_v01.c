 /* Copyright (c) 2019, The Linux Foundation. All rights reserved.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 and
  * only version 2 as published by the Free Software Foundation.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  */
#include <linux/soc/qcom/qmi.h>

#include "adsp_lpm_voting_v01.h"

struct qmi_elem_info prod_set_lpm_vote_req_msg_v01_ei[] = {
	{
		.data_type     = QMI_UNSIGNED_1_BYTE,
		.elem_len      = 1,
		.elem_size     = sizeof(u8),
		.is_array      = NO_ARRAY,
		.tlv_type      = 0x01,
		.offset        = offsetof(struct prod_set_lpm_vote_req_msg_v01,
					   keep_adsp_out_of_lpm),
	},
	{
		.data_type     = QMI_EOTI,
		.is_array      = NO_ARRAY,
	},
};

struct qmi_elem_info prod_set_lpm_vote_resp_msg_v01_ei[] = {
	{
		.data_type     = QMI_STRUCT,
		.elem_len      = 1,
		.elem_size     = sizeof(struct qmi_response_type_v01),
		.is_array      = NO_ARRAY,
		.tlv_type      = 0x02,
		.offset        = offsetof(struct prod_set_lpm_vote_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type     = QMI_EOTI,
		.is_array      = NO_ARRAY,
	},
};

