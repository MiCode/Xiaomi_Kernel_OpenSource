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
#ifndef ADSP_LPM_VOTING_V01_H
#define ADSP_LPM_VOTING_V01_H

#define PGS_SERVICE_ID_V01 0x428
#define PGS_SERVICE_VERS_V01 0x01

#define PROD_SET_LPM_VOTE_REQ_V01 0x0001
#define PROD_SET_LPM_VOTE_RESP_V01 0x0001


struct prod_set_lpm_vote_req_msg_v01 {
	u8 keep_adsp_out_of_lpm;
};
#define PROD_SET_LPM_VOTE_REQ_MSG_V01_MAX_MSG_LEN 4
extern struct qmi_elem_info prod_set_lpm_vote_req_msg_v01_ei[];

struct prod_set_lpm_vote_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define PROD_SET_LPM_VOTE_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info prod_set_lpm_vote_resp_msg_v01_ei[];

#endif
