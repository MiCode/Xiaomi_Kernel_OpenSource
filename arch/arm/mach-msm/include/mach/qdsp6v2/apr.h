/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#ifndef __APR_H_
#define __APR_H_

#define APR_Q6_NOIMG   0
#define APR_Q6_LOADING 1
#define APR_Q6_LOADED  2

struct apr_q6 {
	void *pil;
	uint32_t state;
	struct mutex lock;
};

struct apr_hdr {
	uint16_t hdr_field;
	uint16_t pkt_size;
	uint8_t src_svc;
	uint8_t src_domain;
	uint16_t src_port;
	uint8_t dest_svc;
	uint8_t dest_domain;
	uint16_t dest_port;
	uint32_t token;
	uint32_t opcode;
};

#define APR_HDR_LEN(hdr_len) ((hdr_len)/4)
#define APR_PKT_SIZE(hdr_len, payload_len) ((hdr_len) + (payload_len))
#define APR_HDR_FIELD(msg_type, hdr_len, ver)\
	(((msg_type & 0x3) << 8) | ((hdr_len & 0xF) << 4) | (ver & 0xF))

#define APR_HDR_SIZE sizeof(struct apr_hdr)

/* Version */
#define APR_PKT_VER		0x0

/* Command and Response Types */
#define APR_MSG_TYPE_EVENT	0x0
#define APR_MSG_TYPE_CMD_RSP	0x1
#define APR_MSG_TYPE_SEQ_CMD	0x2
#define APR_MSG_TYPE_NSEQ_CMD	0x3
#define APR_MSG_TYPE_MAX	0x04

/* APR Basic Response Message */
#define APR_BASIC_RSP_RESULT 0x000110E8
#define APR_RSP_ACCEPTED     0x000100BE

/* Domain IDs */
#define APR_DOMAIN_SIM	0x1
#define APR_DOMAIN_PC		0x2
#define APR_DOMAIN_MODEM	0x3
#define APR_DOMAIN_ADSP	0x4
#define APR_DOMAIN_APPS	0x5
#define APR_DOMAIN_MAX	0x6

/* ADSP service IDs */
#define APR_SVC_TEST_CLIENT     0x2
#define APR_SVC_ADSP_CORE	0x3
#define APR_SVC_AFE		0x4
#define APR_SVC_VSM		0x5
#define APR_SVC_VPM		0x6
#define APR_SVC_ASM		0x7
#define APR_SVC_ADM		0x8
#define APR_SVC_ADSP_MVM	0x09
#define APR_SVC_ADSP_CVS	0x0A
#define APR_SVC_ADSP_CVP	0x0B
#define APR_SVC_USM		0x0C
#define APR_SVC_MAX		0x0D

/* Modem Service IDs */
#define APR_SVC_MVS		0x3
#define APR_SVC_MVM		0x4
#define APR_SVC_CVS		0x5
#define APR_SVC_CVP		0x6
#define APR_SVC_SRD		0x7

/* APR Port IDs */
#define APR_MAX_PORTS		0x40

#define APR_NAME_MAX		0x40

#define RESET_EVENTS		0xFFFFFFFF

#define LPASS_RESTART_EVENT	0x1000
#define LPASS_RESTART_READY	0x1001

struct apr_client_data {
	uint16_t reset_event;
	uint16_t reset_proc;
	uint16_t payload_size;
	uint16_t hdr_len;
	uint16_t msg_type;
	uint16_t src;
	uint16_t dest_svc;
	uint16_t src_port;
	uint16_t dest_port;
	uint32_t token;
	uint32_t opcode;
	void *payload;
};

typedef int32_t (*apr_fn)(struct apr_client_data *data, void *priv);

struct apr_svc {
	uint16_t id;
	uint16_t dest_id;
	uint16_t client_id;
	uint8_t rvd;
	uint8_t port_cnt;
	uint8_t svc_cnt;
	uint8_t need_reset;
	apr_fn port_fn[APR_MAX_PORTS];
	void *port_priv[APR_MAX_PORTS];
	apr_fn fn;
	void *priv;
	struct mutex m_lock;
	spinlock_t w_lock;
};

struct apr_client {
	uint8_t id;
	uint8_t svc_cnt;
	uint8_t rvd;
	struct mutex m_lock;
	struct apr_svc_ch_dev *handle;
	struct apr_svc svc[APR_SVC_MAX];
};

struct apr_svc *apr_register(char *dest, char *svc_name, apr_fn svc_fn,
					uint32_t src_port, void *priv);
inline int apr_fill_hdr(void *handle, uint32_t *buf, uint16_t src_port,
			uint16_t msg_type, uint16_t dest_port,
			uint32_t token, uint32_t opcode, uint16_t len);

int apr_send_pkt(void *handle, uint32_t *buf);
int apr_deregister(void *handle);
void change_q6_state(int state);
void q6audio_dsp_not_responding(void);
void apr_reset(void *handle);
#endif
