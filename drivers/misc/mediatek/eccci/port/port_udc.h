/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef __PORT_UDC__
#define __PORT_UDC__
#include "ccci_core.h"
#include "udc.h"
#include "port_t.h"
#include "udc_zlib.h"

/* #define UDC_DATA_DUMP */

#define UDC_API_RESP_ID	0xFFFF0000

void udc_cmd_handler(struct port_t *port, struct sk_buff *skb);
void set_udc_status(struct sk_buff *skb);

/**
 *	 @brief  Compression request (descriptor) for UDC compression
 *
 **/

struct udc_comp_req_t {
	u16 seg_len:13; /* segment length */
	u16 rsvd1:3;
	u16 buf_type:1; /* 0:non-cache 1:cache */
	u16 sit_type:1; /* SIT_Type = PRI (0)/NML (1) */
	u16 sdu_idx:12; /* sdu_idx in corresponding SIT */
	u16 rst:1; /* indicate whether to reset UDC buffer before compression */
	u16 con:1; /* indicate the last segment of an SDU (cont = 0) */
	u32 seg_phy_addr;/* comp_req_table offset */
} __packed;

/**
 *	 @brief  Compression result (meta) of UDC compression
 *
 **/

struct udc_comp_rslt_t {
	u32 cmp_len:13; /* compressed data length */
	u32 cksm:4;     /* UDC buffer checksum */
	u32 sit_type:1; /* which is copied from cmp_req */
	u32 sdu_idx:12; /* which is copied from cmp_req */
	u32 rst:1;      /* indicate whether to set FR bit in UDC header */
	u32 udc:1;      /* indicate whether to set FU bit in UDC header */
	u32 cmp_addr;	/* comp_rslt_table offset */
} __packed;

/* udc cmd interface */
enum ccci_udc_cmd_e {
	UDC_CMD_ACTV = 0,
	UDC_CMD_ACTV_DONE = UDC_CMD_ACTV | 0xFFFF0000,
	UDC_CMD_DEACTV = 1,
	UDC_CMD_DEACTV_DONE = UDC_CMD_DEACTV | 0xFFFF0000,
	UDC_CMD_KICK = 2,
	UDC_CMD_KICK_DONE = UDC_CMD_KICK | 0xFFFF0000,
	UDC_CMD_DISC = 3,
	UDC_CMD_DISC_DONE = UDC_CMD_DISC | 0xFFFF0000,
};

struct udc_state_ctl {
	unsigned int curr_state;
	unsigned int last_state;
};

enum ccci_udc_status {
	UDC_IDLE = 0,
	UDC_HighKick,
	UDC_HandleHighKick,
	UDC_DISCARD,
	UDC_DISC_DONE,
	UDC_DEACTV = 5,
	UDC_KICKDEACTV,
	UDC_DEACTV_DONE,
};

enum ccci_udc_error {
	/* udc default error 1-6 */
	CMP_BUF_FULL = 7,
	CMP_RSLT_FULL,
	CMP_ZERO_LEN,
	CMP_INST_ID_ERR,
};

enum ccci_udc_buf_sz_e {
	UDC_BUF_SZ_2_KBYTES = 11,
	UDC_BUF_SZ_4_KBYTES = 12,
	UDC_BUF_SZ_8_KBTES = 13,
};

struct ccci_udc_deactv_param_t {
	struct ccci_header header;
	u32 udc_inst_id;
	u32 udc_cmd;
} __packed;

enum ccci_udc_cmd_rslt_e {
	UDC_CMD_RSLT_OK = 0,
	UDC_CMD_RSLT_ERROR,
};

struct ccci_udc_cmd_rsp_t {
	struct ccci_header header;
	u32 udc_inst_id;
	u32 udc_cmd;
	u32 rslt;
} __packed;

struct ccci_udc_actv_param_t {
	struct ccci_header header;
	u32 udc_inst_id;
	u32 udc_cmd;
	u32 buf_sz;
	u32 dict_opt;
	u32 num_shm_mask_bits;
} __packed;

struct ccci_udc_comm_param_t {
	struct ccci_header header;
	u32 udc_inst_id;
	u32 udc_cmd;
	u32 comm_para;
} __packed;

struct ccci_udc_disc_param_t {
	struct ccci_header header;
	u32 udc_inst_id;
	u32 udc_cmd;
	u32 new_req_r;
} __packed;


struct ccci_udc_kick_param_t {
	struct ccci_header header;
	u32 udc_inst_id;
	u32 udc_cmd;
	u32 exp_tmr;/* (unit: milliscond) */
} __packed;
/* udc cmd end */

/*
 *struct ap_md_rw_index {
 *	unsigned int read;
 *	unsigned int write;
 *} __packed;
 */
struct ap_md_rw_index {
	struct {
		unsigned int read;
		unsigned int write;
	} md_des_ins0;
	struct {
		unsigned int read;
		unsigned int write;
	} ap_resp_ins0;
	struct {
		unsigned int read;
		unsigned int write;
	} md_des_ins1;
	struct {
		unsigned int read;
		unsigned int write;
	} ap_resp_ins1;
} __packed;

#endif	/*__PORT_UDC__*/

