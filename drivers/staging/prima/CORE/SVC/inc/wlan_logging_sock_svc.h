/*
* Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
*
* Previously licensed under the ISC license by Qualcomm Atheros, Inc.
*
*
* Permission to use, copy, modify, and/or distribute this software for
* any purpose with or without fee is hereby granted, provided that the
* above copyright notice and this permission notice appear in all
* copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
* AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
* TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
* PERFORMANCE OF THIS SOFTWARE.
*/

/*
* This file was originally distributed by Qualcomm Atheros, Inc.
* under proprietary terms before Copyright ownership was assigned
* to the Linux Foundation.
*/

/******************************************************************************
 * wlan_logging_sock_svc.h
 *
 ******************************************************************************/

#ifndef WLAN_LOGGING_SOCK_SVC_H
#define WLAN_LOGGING_SOCK_SVC_H

#include <wlan_nlink_srv.h>
#include <vos_status.h>
#include <wlan_hdd_includes.h>
#include <vos_trace.h>
#include <wlan_nlink_common.h>


int wlan_logging_sock_init_svc(void);
int wlan_logging_sock_deinit_svc(void);
int wlan_logging_sock_activate_svc(int log_fe_to_console, int num_buf);
int wlan_logging_flush_pkt_queue(void);
int wlan_logging_sock_deactivate_svc(void);
int wlan_log_to_user(VOS_TRACE_LEVEL log_level, char *to_be_sent, int length);
int wlan_queue_logpkt_for_app(vos_pkt_t *pPacket, uint32 pkt_type);
void wlan_process_done_indication(uint8 type, uint32 reason_code);

void wlan_init_log_completion(void);
int wlan_set_log_completion(uint32 is_fatal,
                            uint32 indicator,
                            uint32 reason_code);
void wlan_get_log_completion(uint32 *is_fatal,
                             uint32 *indicator,
                             uint32 *reason_code);
bool wlan_is_log_report_in_progress(void);
void wlan_reset_log_report_in_progress(void);

void wlan_deinit_log_completion(void);

void wlan_logging_set_log_level(void);

bool wlan_is_logger_thread(int threadId);
#endif /* WLAN_LOGGING_SOCK_SVC_H */
