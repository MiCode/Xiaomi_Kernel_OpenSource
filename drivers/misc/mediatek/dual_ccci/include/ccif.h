/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CCIF_H__
#define __CCIF_H__

/*  CCIF common macro definition */
#define CCIF_INTR_MAX_RE_ENTER_CNT            (5)

struct ccif_statistics_t {
	unsigned long long irq_cnt;
	unsigned int re_enter_cnt;
	unsigned int max_re_enter_cnt;
};

enum ccif_state_bit_t {
	CCIF_TOP_HALF_RUNNING = 0x0,
	CCIF_BOTTOM_HALF_RUNNING,
	CCIF_CALL_BACK_FUNC_LOCKED,
	CCIF_ISR_INFO_CALL_BACK_LOCKED,
};

struct ccif_msg_t {
	unsigned int data[2];
	unsigned int channel;
	unsigned int reserved;
};

typedef int (*ccif_push_func_t) (struct ccif_msg_t *);
typedef int (*ccif_notify_funct_t) (void);

struct ccif_t {
	unsigned long m_reg_base;
	unsigned long m_md_reg_base;
	unsigned long m_status;
	unsigned int m_rx_idx;
	unsigned int m_tx_idx;
	unsigned int m_irq_id;
	unsigned int m_irq_attr;
	unsigned int m_ccif_type;
	struct ccif_statistics_t m_statistics;
	spinlock_t m_lock;
	void *m_logic_ctl_block;
	unsigned int m_irq_dis_cnt;
	unsigned int m_md_id;

	int (*push_msg)(struct ccif_msg_t *, void *);
	void (*notify_push_done)(void *);
	void (*isr_notify)(int);
	int (*register_call_back_func)(struct ccif_t *,
			int (*push_func)(struct ccif_msg_t *, void *),
			void (*notify_func)(void *));
	int (*register_isr_notify_func)(struct ccif_t *, void (*additional)(int));
	int (*ccif_init)(struct ccif_t *);
	int (*ccif_de_init)(struct ccif_t *);
	int (*ccif_register_intr)(struct ccif_t *);
	int (*ccif_en_intr)(struct ccif_t *);
	void (*ccif_dis_intr)(struct ccif_t *);
	int (*ccif_dump_reg)(struct ccif_t *, unsigned int buf[], int len);
	int (*ccif_read_phy_ch_data)(struct ccif_t *, int ch,
			unsigned int buf[]);
	int (*ccif_write_phy_ch_data)(struct ccif_t *, unsigned int buf[],
			int retry_en);
	int (*ccif_get_rx_ch)(struct ccif_t *);
	int (*ccif_get_busy_state)(struct ccif_t *);
	void (*ccif_set_busy_state)(struct ccif_t *, unsigned int val);
	int (*ccif_ack_phy_ch)(struct ccif_t *, int ch);
	int (*ccif_clear_sram)(struct ccif_t *);
	int (*ccif_write_runtime_data)(struct ccif_t *, unsigned int buf[], int len);
	int (*ccif_intr_handler)(struct ccif_t *);
	int (*ccif_reset)(struct ccif_t *);
};

#endif				/* __CCIF_H__ */
