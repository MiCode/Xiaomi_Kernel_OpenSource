/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
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

int ccci_fsm_is_normal_mdee(void);
int ccci_fsm_increase_devapc_dump_counter(void);

#endif /* __CCCI_FSM_H__ */

