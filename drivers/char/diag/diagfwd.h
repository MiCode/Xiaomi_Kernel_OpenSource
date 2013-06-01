/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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

#define CHK_OVERFLOW(bufStart, start, end, length) \
	((((bufStart) <= (start)) && ((end) - (start) >= (length))) ? 1 : 0)

void diagfwd_init(void);
void diagfwd_exit(void);
void diag_process_hdlc(void *data, unsigned len);
void diag_smd_send_req(struct diag_smd_info *smd_info);
void process_lock_enabling(struct diag_nrt_wake_lock *lock, int real_time);
void process_lock_on_notify(struct diag_nrt_wake_lock *lock);
void process_lock_on_read(struct diag_nrt_wake_lock *lock, int pkt_len);
void process_lock_on_copy(struct diag_nrt_wake_lock *lock);
void process_lock_on_copy_complete(struct diag_nrt_wake_lock *lock);
void diag_usb_legacy_notifier(void *, unsigned, struct diag_request *);
long diagchar_ioctl(struct file *, unsigned int, unsigned long);
int diag_device_write(void *, int, struct diag_request *);
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
void diag_smd_destructor(struct diag_smd_info *smd_info);
int diag_switch_logging(unsigned long);
int diag_command_reg(unsigned long);
void diag_cmp_logging_modes_sdio_pipe(int old_mode, int new_mode);
void diag_cmp_logging_modes_diagfwd_bridge(int old_mode, int new_mode);
int diag_process_apps_pkt(unsigned char *buf, int len);
void diag_reset_smd_data(int queue);
/* State for diag forwarding */
#ifdef CONFIG_DIAG_OVER_USB
int diagfwd_connect(void);
int diagfwd_disconnect(void);
#endif
extern int diag_debug_buf_idx;
extern unsigned char diag_debug_buf[1024];
extern struct platform_driver msm_diag_dci_driver;
#endif
