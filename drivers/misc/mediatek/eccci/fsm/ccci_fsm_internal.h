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

#ifndef __CCCI_FSM_INTERNAL_H__
#define __CCCI_FSM_INTERNAL_H__

#include <linux/list.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include "ccci_core.h"
#include "ccci_fsm.h"
#include "ccci_modem.h"
#include "ccci_hif.h"
#include "ccci_port.h"
#include "port_t.h"

/************ enumerations ************/

enum {
	CCCI_FSM_INVALID = 0,
	CCCI_FSM_GATED,
	CCCI_FSM_STARTING,
	CCCI_FSM_READY,
	CCCI_FSM_STOPPING,
	CCCI_FSM_EXCEPTION,
};

enum CCCI_FSM_EVENT {
	CCCI_EVENT_INVALID = 0,
	CCCI_EVENT_HS1,
	CCCI_EVENT_FS_IN,
	CCCI_EVENT_FS_OUT,
	CCCI_EVENT_HS2,
	CCCI_EVENT_CCIF_EX_HS_DONE,
	CCCI_EVENT_MD_EX,
	CCCI_EVENT_MD_EX_REC_OK,
	CCCI_EVENT_MD_EX_PASS,
	CCCI_EVENT_MAX,
};

enum CCCI_FSM_COMMAND {
	CCCI_COMMAND_INVALID = 0,
	CCCI_COMMAND_START, /* from md_init */
	CCCI_COMMAND_STOP, /* from md_init */
	CCCI_COMMAND_WDT, /* from MD */
	CCCI_COMMAND_EE, /* from MD */
	CCCI_COMMAND_MD_HANG, /* from status polling thread */
	CCCI_COMMAND_MAX,
};

enum CCCI_EE_REASON {
	EXCEPTION_NONE = 0,
	EXCEPTION_HS1_TIMEOUT,
	EXCEPTION_HS2_TIMEOUT,
	EXCEPTION_WDT,
	EXCEPTION_EE,
	EXCEPTION_MD_NO_RESPONSE,
};

enum CCCI_FSM_POLLER_STATE {
	FSM_POLLER_INVALID = 0,
	FSM_POLLER_WAITING_RESPONSE,
	FSM_POLLER_RECEIVED_RESPONSE,
};

enum {
	SCP_CCCI_STATE_INVALID = 0,
	SCP_CCCI_STATE_BOOTING = 1,
	SCP_CCCI_STATE_RBREADY = 2,
};

enum CCCI_MD_MSG {
	CCCI_MD_MSG_FORCE_STOP_REQUEST = 0xFAF50001,
	CCCI_MD_MSG_FLIGHT_STOP_REQUEST,
	CCCI_MD_MSG_FORCE_START_REQUEST,
	CCCI_MD_MSG_FLIGHT_START_REQUEST,
	CCCI_MD_MSG_RESET_REQUEST,

	CCCI_MD_MSG_EXCEPTION,
	CCCI_MD_MSG_SEND_BATTERY_INFO,
	CCCI_MD_MSG_STORE_NVRAM_MD_TYPE,
	CCCI_MD_MSG_CFG_UPDATE,
	CCCI_MD_MSG_RANDOM_PATTERN,
};

enum {
	/* also used to check EE flow done */
	MD_EE_FLOW_START	= (1 << 0),
	MD_EE_MSG_GET		= (1 << 1),
	MD_EE_OK_MSG_GET	= (1 << 2),
	MD_EE_PASS_MSG_GET	= (1 << 3),
	MD_EE_PENDING_TOO_LONG	= (1 << 4),
	MD_EE_WDT_GET		= (1 << 5),
	MD_EE_DUMP_IN_GPD	= (1 << 6),
	MD_EE_SWINT_GET		= (1 << 7),
};

enum {
	/*
	 * we assume every valid EE is started with CCIF EX interrupt,
	 * and ignore EX messages without one.
	 * if we get EX_OK, then we can parse out valid EE info anyway,
	 * so no need to distinguish EX_OK only case.
	 * and we never encountered EX_PASS only case~
	 */
	MD_EE_CASE_NORMAL = 0,
	MD_EE_CASE_ONLY_SWINT,
	MD_EE_CASE_ONLY_EX,
	MD_EE_CASE_NO_RESPONSE,
	MD_EE_CASE_WDT,
};

enum MDEE_DUMP_LEVEL {
	MDEE_DUMP_LEVEL_BOOT_FAIL,
	MDEE_DUMP_LEVEL_STAGE1,
	MDEE_DUMP_LEVEL_STAGE2,
};

enum ccci_ipi_op_id {
	CCCI_OP_SCP_STATE,
	CCCI_OP_MD_STATE,
	CCCI_OP_SHM_INIT,
	CCCI_OP_SHM_RESET,

	CCCI_OP_LOG_LEVEL,
	CCCI_OP_GPIO_TEST,
	CCCI_OP_EINT_TEST,
	CCCI_OP_MSGSND_TEST,
	CCCI_OP_ASSERT_TEST,
};


/************ definetions ************/

#define FSM_NAME "ccci_fsm"
#define FSM_CMD_FLAG_WAIT_FOR_COMPLETE (1 << 0)
#define FSM_CMD_FLAG_FLIGHT_MODE (1 << 1)

#define EVENT_POLL_INTEVAL 20 /* ms */
#define BOOT_TIMEOUT (30*1000)
#define MD_EX_CCIF_TIMEOUT 10000
#define MD_EX_REC_OK_TIMEOUT 10000
#define MD_EX_PASS_TIMEOUT 10000
#define EE_DONE_TIMEOUT 30 /* s */
#define SCP_BOOT_TIMEOUT (30*1000)

#define GET_OTHER_MD_ID(a) (a == MD_SYS1 ? MD_SYS3 : MD_SYS1)

#define MD_IMG_DUMP_SIZE  (1<<8)
#define DSP_IMG_DUMP_SIZE (1<<9)
#define CCCI_AED_DUMP_EX_MEM		(1<<0)
#define CCCI_AED_DUMP_MD_IMG_MEM	(1<<1)
#define CCCI_AED_DUMP_CCIF_REG		(1<<2)
#define CCCI_AED_DUMP_EX_PKT		(1<<3)
#define MD_EX_MPU_STR_LEN (128)
#define MD_EX_START_TIME_LEN (128)

/************ structures ************/

struct ccci_ipi_msg {
	u16 md_id;
	u16 op_id;
	u32 data[1];
} __packed;

struct ccci_fsm_scp {
	int md_id;
	struct work_struct scp_md_state_sync_work;
};

struct ccci_fsm_poller {
	int md_id;
	enum CCCI_FSM_POLLER_STATE poller_state;
	struct task_struct *poll_thread;
	wait_queue_head_t status_rx_wq;
	unsigned long long latest_poll_start_time;
};

struct ccci_fsm_ee;
struct md_ee_ops {
	 void (*set_ee_pkg)(struct ccci_fsm_ee *ee_ctl, char *data, int len);
	 void (*dump_ee_info)(struct ccci_fsm_ee *ee_ctl,
			enum MDEE_DUMP_LEVEL level, int more_info);
};

struct ccci_fsm_ee {
	int md_id;
	unsigned int ee_info_flag;
	spinlock_t ctrl_lock;

	unsigned int ee_case;
	unsigned int ex_type;
	void *dumper_obj;
	struct md_ee_ops *ops;
	char ex_mpu_string[MD_EX_MPU_STR_LEN];
	char ex_start_time[MD_EX_START_TIME_LEN];
	unsigned int mdlog_dump_done;
};

struct ccci_fsm_monitor {
	int md_id;
	dev_t dev_n;
	struct cdev *char_dev;
	atomic_t usage_cnt;
	wait_queue_head_t rx_wq;
	struct ccci_skb_queue rx_skb_list;
};

struct ccci_fsm_ctl {
	int md_id;
	enum MD_STATE md_state;

	unsigned int curr_state;
	unsigned int last_state;
	struct list_head command_queue;
	struct list_head event_queue;
	wait_queue_head_t command_wq;
	spinlock_t event_lock;
	spinlock_t command_lock;
	spinlock_t cmd_complete_lock;
	struct task_struct *fsm_thread;
	atomic_t fs_ongoing;
	char wakelock_name[32];
	struct wakeup_source wakelock;

	unsigned long boot_count; /* for throttling feature */

	struct ccci_fsm_scp scp_ctl;
	struct ccci_fsm_poller poller_ctl;
	struct ccci_fsm_ee ee_ctl;
	struct ccci_fsm_monitor monitor_ctl;
};

struct ccci_fsm_event {
	struct list_head entry;
	enum CCCI_FSM_EVENT event_id;
	unsigned int length;
	unsigned char data[0];
};

struct ccci_fsm_command {
	struct list_head entry;
	enum CCCI_FSM_COMMAND cmd_id;
	unsigned int flag;
	int complete; /* -1: fail; 0: on-going; 1: success */
	wait_queue_head_t complete_wq;
};


/************ APIs ************/

int fsm_append_command(struct ccci_fsm_ctl *ctl,
	enum CCCI_FSM_COMMAND cmd_id, unsigned int flag);
int fsm_append_event(struct ccci_fsm_ctl *ctl, enum CCCI_FSM_EVENT event_id,
	unsigned char *data, unsigned int length);

int fsm_scp_init(struct ccci_fsm_scp *scp_ctl);
int fsm_poller_init(struct ccci_fsm_poller *poller_ctl);
int fsm_ee_init(struct ccci_fsm_ee *ee_ctl);
int fsm_monitor_init(struct ccci_fsm_monitor *monitor_ctl);
int fsm_sys_init(void);

struct ccci_fsm_ctl *fsm_get_entity_by_device_number(dev_t dev_n);
struct ccci_fsm_ctl *fsm_get_entity_by_md_id(int md_id);
int fsm_monitor_send_message(int md_id, enum CCCI_MD_MSG msg, u32 resv);
int fsm_ccism_init_ack_handler(int md_id, int data);

void fsm_md_bootup_timeout_handler(struct ccci_fsm_ee *ee_ctl);
void fsm_md_wdt_handler(struct ccci_fsm_ee *ee_ctl);
void fsm_md_no_response_handler(struct ccci_fsm_ee *ee_ctl);
void fsm_md_exception_stage(struct ccci_fsm_ee *ee_ctl, int stage);
void fsm_ee_message_handler(struct ccci_fsm_ee *ee_ctl, struct sk_buff *skb);
int fsm_check_ee_done(struct ccci_fsm_ee *ee_ctl, int timeout);
int force_md_stop(struct ccci_fsm_monitor *monitor_ctl);

extern int mdee_dumper_v1_alloc(struct ccci_fsm_ee *mdee);
extern int mdee_dumper_v2_alloc(struct ccci_fsm_ee *mdee);
extern int mdee_dumper_v3_alloc(struct ccci_fsm_ee *mdee);
extern int mdee_dumper_v5_alloc(struct ccci_fsm_ee *mdee);
extern void inject_md_status_event(int md_id, int event_type, char reason[]);
#ifdef SET_EMI_STEP_BY_STAGE
extern void ccci_set_mem_access_protection_second_stage(int md_id);
#endif
extern void mdee_set_ex_start_str(struct ccci_fsm_ee *ee_ctl,
	const unsigned int type, const char *str);
#endif /* __CCCI_FSM_INTERNAL_H__ */

