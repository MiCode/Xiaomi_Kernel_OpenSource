/* Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
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

#define NO_PROCESS	0
#define NON_APPS_PROC	-1

#define RESET_AND_NO_QUEUE 0
#define RESET_AND_QUEUE 1

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
int diag_smd_write(struct diag_smd_info *smd_info, void *buf, int len);
void diag_process_hdlc(void *data, unsigned len);
void diag_smd_send_req(struct diag_smd_info *smd_info);
long diagchar_ioctl(struct file *, unsigned int, unsigned long);
int diag_device_write(void *buf, int len, int data_type, int ctxt);
int mask_request_validate(unsigned char mask_buf[]);
void diag_clear_reg(int);
int chk_config_get_id(void);
int chk_apps_only(void);
int chk_apps_master(void);
int chk_polling_response(void);
void diag_update_userspace_clients(unsigned int type);
void diag_update_sleeping_process(int process_id, int data_type);
void encode_rsp_and_send(int buf_length);
void diag_smd_notify(void *ctxt, unsigned event);
int diag_smd_constructor(struct diag_smd_info *smd_info, int peripheral,
			 int type);
void diag_smd_buffer_init(struct diag_smd_info *smd_info);
void diag_smd_destructor(struct diag_smd_info *smd_info);
void diag_cmp_logging_modes_diagfwd_bridge(int old_mode, int new_mode);
int diag_process_apps_pkt(unsigned char *buf, int len);
void diag_reset_smd_data(int queue);
void diag_update_pkt_buffer(unsigned char *buf, int type);
int diag_process_stm_cmd(unsigned char *buf, unsigned char *dest_buf);
extern int diag_debug_buf_idx;
extern unsigned char diag_debug_buf[1024];
extern struct platform_driver msm_diag_dci_driver;
#endif
