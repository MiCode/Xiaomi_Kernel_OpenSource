/* Copyright (c) 2008-2015, 2017 The Linux Foundation. All rights reserved.
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

#ifndef DIAGFWD_H
#define DIAGFWD_H

/*
 * The context applies to Diag SMD data buffers. It is used to identify the
 * buffer once these buffers are writtent to USB.
 */
#define SET_BUF_CTXT(p, d, n) \
	(((p & 0xFF) << 16) | ((d & 0xFF) << 8) | (n & 0xFF))
#define GET_BUF_PERIPHERAL(p)	((p & 0xFF0000) >> 16)
#define GET_BUF_TYPE(d)		((d & 0x00FF00) >> 8)
#define GET_BUF_NUM(n)		((n & 0x0000FF))

#define CHK_OVERFLOW(bufStart, start, end, length) \
	((((bufStart) <= (start)) && ((end) - (start) >= (length))) ? 1 : 0)

int diagfwd_init(void);
void diagfwd_exit(void);
void diag_process_hdlc_pkt(void *data, unsigned len,
			   struct diag_md_session_t *info);
void diag_process_non_hdlc_pkt(unsigned char *data, int len,
			       struct diag_md_session_t *info);
int chk_config_get_id(void);
int chk_apps_only(void);
int chk_apps_master(void);
int chk_polling_response(void);
int diag_cmd_log_on_demand(unsigned char *src_buf, int src_len,
			   unsigned char *dest_buf, int dest_len);
int diag_cmd_get_mobile_id(unsigned char *src_buf, int src_len,
			   unsigned char *dest_buf, int dest_len);
int diag_check_common_cmd(struct diag_pkt_header_t *header);
void diag_update_userspace_clients(unsigned int type);
void diag_update_sleeping_process(int process_id, int data_type);
int diag_process_apps_pkt(unsigned char *buf, int len,
			  struct diag_md_session_t *info);
void diag_send_error_rsp(unsigned char *buf, int len,
			 struct diag_md_session_t *info);
void diag_update_pkt_buffer(unsigned char *buf, uint32_t len, int type);
int diag_process_stm_cmd(unsigned char *buf, unsigned char *dest_buf);
void diag_md_hdlc_reset_timer_func(unsigned long pid);
void diag_update_md_clients(unsigned int type);
#endif
