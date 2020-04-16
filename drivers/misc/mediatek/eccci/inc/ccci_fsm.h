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

#ifndef __CCCI_FSM_H__
#define __CCCI_FSM_H__

enum MD_IRQ_TYPE {
	MD_IRQ_WDT,
	MD_IRQ_CCIF_EX,
};

int ccci_fsm_init(int md_id);
int ccci_fsm_recv_control_packet(int md_id, struct sk_buff *skb);
int ccci_fsm_recv_status_packet(int md_id, struct sk_buff *skb);
int ccci_fsm_recv_md_interrupt(int md_id, enum MD_IRQ_TYPE type);
long ccci_fsm_ioctl(int md_id, unsigned int cmd, unsigned long arg);
enum MD_STATE ccci_fsm_get_md_state(int md_id);
enum MD_STATE_FOR_USER ccci_fsm_get_md_state_for_user(int md_id);

extern void mdee_set_ex_time_str(unsigned char md_id, unsigned int type,
	char *str);
#endif /* __CCCI_FSM_H__ */

