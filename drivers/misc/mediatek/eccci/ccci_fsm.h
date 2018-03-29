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

#include <linux/list.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define FSM "FSM"

typedef enum {
	CCCI_FSM_INVALID = 0,
	CCCI_FSM_GATED,
	CCCI_FSM_STARTING,
	CCCI_FSM_READY,
	CCCI_FSM_STOPPING,
	CCCI_FSM_EXCEPTION,
} CCCI_FSM_STATE;

typedef enum {
	CCCI_EVENT_INVALID = 0,
	CCCI_EVENT_HS1,
	CCCI_EVENT_FS_IN,
	CCCI_EVENT_FS_OUT,
	CCCI_EVENT_HS2,
	CCCI_EVENT_CCIF_HS,
	CCCI_EVENT_MD_EX,
	CCCI_EVENT_MD_EX_REC_OK,
	CCCI_EVENT_MD_EX_PASS,
	CCCI_EVENT_MAX,
} CCCI_FSM_EVENT;

typedef enum {
	CCCI_COMMAND_INVALID = 0,
	CCCI_COMMAND_START, /* from md_init */
	CCCI_COMMAND_STOP, /* from md_init */
	CCCI_COMMAND_WDT, /* from MD */
	CCCI_COMMAND_EE, /* from MD or CCCI itself */
	CCCI_COMMAND_MD_HANG, /* from CCCI itself */
	CCCI_COMMAND_MAX,
} CCCI_FSM_COMMAND;

typedef enum {
	EXCEPTION_NONE = 0,
	EXCEPTION_HS1_TIMEOUT,
	EXCEPTION_HS2_TIMEOUT,
	EXCEPTION_WDT,
	EXCEPTION_EE,
	EXCEPTION_MD_HANG,
} CCCI_EE_REASON;

struct ccci_fsm_ctl {
	struct ccci_modem *md;
	CCCI_FSM_STATE curr_state;
	CCCI_FSM_STATE last_state;
	struct list_head command_queue;
	struct list_head event_queue;
	wait_queue_head_t command_wq;
	spinlock_t event_lock;
	spinlock_t command_lock;
	spinlock_t cmd_complete_lock;
	struct task_struct *fsm_thread;
	atomic_t fs_ongoing;
};

struct ccci_fsm_event {
	struct list_head entry;
	CCCI_FSM_EVENT event_id;
	unsigned int length;
	unsigned char data[0];
};

struct ccci_fsm_command {
	struct list_head entry;
	CCCI_FSM_COMMAND cmd_id;
	unsigned int flag;
	int complete; /* -1: fail; 0: on-going; 1: success */
	wait_queue_head_t complete_wq;
};

#define CCCI_CMD_FLAG_WAIT_FOR_COMPLETE (1 << 0)
#define CCCI_CMD_FLAG_FLIGHT_MODE (1 << 1)

#define EVENT_POLL_INTEVAL 500 /* ms */
#define BOOT_TIMEOUT 10000
#define MD_EX_CCIF_TIMEOUT 10000
#define MD_EX_REC_OK_TIMEOUT 10000
#define MD_EX_PASS_TIMEOUT 10000
#define EE_DONE_TIMEOUT 180 /* s */


int ccci_fsm_init(struct ccci_modem *md);
int ccci_fsm_append_command(struct ccci_modem *md, CCCI_FSM_COMMAND cmd_id, unsigned int flag);
int ccci_fsm_append_event(struct ccci_modem *md, CCCI_FSM_EVENT event_id,
	unsigned char *data, unsigned int length);

#ifndef CONFIG_MTK_ECCCI_C2K
extern void c2k_reset_modem(void);
#endif
