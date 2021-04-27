/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __CCCI_PORT_H__
#define __CCCI_PORT_H__

enum {
	CRIT_USR_FS,
	CRIT_USR_MUXD,
	CRIT_USR_MDLOG,
	CRIT_USR_META,
	CRIT_USR_MDLOG_CTRL = CRIT_USR_META,
	CRIT_USR_MAX,
};

/*
 * This API is called by ccci_modem,
 * and used to create all ccci port instance for per modem
 */
int ccci_port_init(int md_id);

/*
 * This API is called by ccci_fsm,
 * and used to all ccci port status for debugging
 */
void ccci_port_dump_status(int md_id);

/*
 * This API is called by ccci_fsm,
 * and used to dispatch modem status for related port
 */
void ccci_port_md_status_notify(int md_id, unsigned int state);

/*
 * This API is called by HIF,
 * and used to dispatch Queue status for related port
 */
void ccci_port_queue_status_notify(int md_id, int hif_id, int qno, int dir,
	unsigned int state);

/*
 * This API is called by HIF,
 * and used to dispatch RX data for related port
 * flag: if 0x1: with ccci_header, 0x2: with netif_header
 */
int ccci_port_recv_skb(int md_id, int hif_id, struct sk_buff *skb,
	unsigned int flag);

/*
 * This API is called by ccci fsm,
 * and used to check whether all critical user exited.
 */
int ccci_port_check_critical_user(int md_id);

/*
 * This API is called by ccci fsm,
 * and used to check critical user only ccci_fsd exited.
 */
int ccci_port_critical_user_only_fsd(int md_id);

/*
 * This API is called by ccci fsm,
 * and used to get critical user status.
 */
int ccci_port_get_critical_user(int md_id, unsigned int user_id);

/*
 * This API is called by ccci fsm,
 * and used to send a ccci msg for modem.
 */
int ccci_port_send_msg_to_md(int md_id, int ch, unsigned int msg,
	unsigned int resv, int blocking);

/*
 * This API is called by ccci fsm,
 * and used to set port traffic flag to catch traffic history
 * on some important channel.
 * port traffic use md_boot_data[MD_CFG_DUMP_FLAG] = 0x6000_000x
 * as port dump flag
 */
void ccci_port_set_traffic_flag(int md_id, unsigned int dump_flag);
#endif /* __CCCI_PORT_H__ */
