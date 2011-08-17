/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Shared memory logging implementation.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/remote_spinlock.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>

#include <mach/msm_iomap.h>
#include <mach/smem_log.h>

#include "smd_private.h"
#include "smd_rpc_sym.h"
#include "modem_notifier.h"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define D_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	int i; \
	printk(KERN_ERR "%s", prestr); \
	for (i = 0; i < cnt; i++) \
		printk(KERN_ERR "%.2x", buf[i]); \
	printk(KERN_ERR "\n"); \
} while (0)
#else
#define D_DUMP_BUFFER(prestr, cnt, buf)
#endif

#ifdef DEBUG
#define D(x...) printk(x)
#else
#define D(x...) do {} while (0)
#endif

#if defined(CONFIG_ARCH_MSM7X30) || defined(CONFIG_ARCH_MSM8X60) \
	|| defined(CONFIG_ARCH_FSM9XXX)
#define TIMESTAMP_ADDR (MSM_TMR_BASE + 0x08)
#else
#define TIMESTAMP_ADDR (MSM_TMR_BASE + 0x04)
#endif

struct smem_log_item {
	uint32_t identifier;
	uint32_t timetick;
	uint32_t data1;
	uint32_t data2;
	uint32_t data3;
};

#define SMEM_LOG_NUM_ENTRIES 2000
#define SMEM_LOG_EVENTS_SIZE (sizeof(struct smem_log_item) * \
			      SMEM_LOG_NUM_ENTRIES)

#define SMEM_LOG_NUM_STATIC_ENTRIES 150
#define SMEM_STATIC_LOG_EVENTS_SIZE (sizeof(struct smem_log_item) * \
				     SMEM_LOG_NUM_STATIC_ENTRIES)

#define SMEM_LOG_NUM_POWER_ENTRIES 2000
#define SMEM_POWER_LOG_EVENTS_SIZE (sizeof(struct smem_log_item) * \
			      SMEM_LOG_NUM_POWER_ENTRIES)

#define SMEM_SPINLOCK_SMEM_LOG		"S:2"
#define SMEM_SPINLOCK_STATIC_LOG	"S:5"
/* POWER shares with SMEM_SPINLOCK_SMEM_LOG */

static remote_spinlock_t remote_spinlock;
static remote_spinlock_t remote_spinlock_static;
static uint32_t smem_log_enable;
static int smem_log_initialized;

module_param_named(log_enable, smem_log_enable, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);


struct smem_log_inst {
	int which_log;
	struct smem_log_item __iomem *events;
	uint32_t __iomem *idx;
	uint32_t num;
	uint32_t read_idx;
	uint32_t last_read_avail;
	wait_queue_head_t read_wait;
	remote_spinlock_t *remote_spinlock;
};

enum smem_logs {
	GEN = 0,
	STA,
	POW,
	NUM
};

static struct smem_log_inst inst[NUM];

#if defined(CONFIG_DEBUG_FS)

#define HSIZE 13

struct sym {
	uint32_t val;
	char *str;
	struct hlist_node node;
};

struct sym id_syms[] = {
	{ SMEM_LOG_PROC_ID_MODEM, "MODM" },
	{ SMEM_LOG_PROC_ID_Q6, "QDSP" },
	{ SMEM_LOG_PROC_ID_APPS, "APPS" },
};

struct sym base_syms[] = {
	{ SMEM_LOG_ONCRPC_EVENT_BASE, "ONCRPC" },
	{ SMEM_LOG_SMEM_EVENT_BASE, "SMEM" },
	{ SMEM_LOG_TMC_EVENT_BASE, "TMC" },
	{ SMEM_LOG_TIMETICK_EVENT_BASE, "TIMETICK" },
	{ SMEM_LOG_DEM_EVENT_BASE, "DEM" },
	{ SMEM_LOG_ERROR_EVENT_BASE, "ERROR" },
	{ SMEM_LOG_DCVS_EVENT_BASE, "DCVS" },
	{ SMEM_LOG_SLEEP_EVENT_BASE, "SLEEP" },
	{ SMEM_LOG_RPC_ROUTER_EVENT_BASE, "ROUTER" },
};

struct sym event_syms[] = {
#if defined(CONFIG_MSM_N_WAY_SMSM)
	{ DEM_SMSM_ISR, "SMSM_ISR" },
	{ DEM_STATE_CHANGE, "STATE_CHANGE" },
	{ DEM_STATE_MACHINE_ENTER, "STATE_MACHINE_ENTER" },
	{ DEM_ENTER_SLEEP, "ENTER_SLEEP" },
	{ DEM_END_SLEEP, "END_SLEEP" },
	{ DEM_SETUP_SLEEP, "SETUP_SLEEP" },
	{ DEM_SETUP_POWER_COLLAPSE, "SETUP_POWER_COLLAPSE" },
	{ DEM_SETUP_SUSPEND, "SETUP_SUSPEND" },
	{ DEM_EARLY_EXIT, "EARLY_EXIT" },
	{ DEM_WAKEUP_REASON, "WAKEUP_REASON" },
	{ DEM_DETECT_WAKEUP, "DETECT_WAKEUP" },
	{ DEM_DETECT_RESET, "DETECT_RESET" },
	{ DEM_DETECT_SLEEPEXIT, "DETECT_SLEEPEXIT" },
	{ DEM_DETECT_RUN, "DETECT_RUN" },
	{ DEM_APPS_SWFI, "APPS_SWFI" },
	{ DEM_SEND_WAKEUP, "SEND_WAKEUP" },
	{ DEM_ASSERT_OKTS, "ASSERT_OKTS" },
	{ DEM_NEGATE_OKTS, "NEGATE_OKTS" },
	{ DEM_PROC_COMM_CMD, "PROC_COMM_CMD" },
	{ DEM_REMOVE_PROC_PWR, "REMOVE_PROC_PWR" },
	{ DEM_RESTORE_PROC_PWR, "RESTORE_PROC_PWR" },
	{ DEM_SMI_CLK_DISABLED, "SMI_CLK_DISABLED" },
	{ DEM_SMI_CLK_ENABLED, "SMI_CLK_ENABLED" },
	{ DEM_MAO_INTS, "MAO_INTS" },
	{ DEM_APPS_WAKEUP_INT, "APPS_WAKEUP_INT" },
	{ DEM_PROC_WAKEUP, "PROC_WAKEUP" },
	{ DEM_PROC_POWERUP, "PROC_POWERUP" },
	{ DEM_TIMER_EXPIRED, "TIMER_EXPIRED" },
	{ DEM_SEND_BATTERY_INFO, "SEND_BATTERY_INFO" },
	{ DEM_REMOTE_PWR_CB, "REMOTE_PWR_CB" },
	{ DEM_TIME_SYNC_START, "TIME_SYNC_START" },
	{ DEM_TIME_SYNC_SEND_VALUE, "TIME_SYNC_SEND_VALUE" },
	{ DEM_TIME_SYNC_DONE, "TIME_SYNC_DONE" },
	{ DEM_TIME_SYNC_REQUEST, "TIME_SYNC_REQUEST" },
	{ DEM_TIME_SYNC_POLL, "TIME_SYNC_POLL" },
	{ DEM_TIME_SYNC_INIT, "TIME_SYNC_INIT" },
	{ DEM_INIT, "INIT" },
#else

	{ DEM_NO_SLEEP, "NO_SLEEP" },
	{ DEM_INSUF_TIME, "INSUF_TIME" },
	{ DEMAPPS_ENTER_SLEEP, "APPS_ENTER_SLEEP" },
	{ DEMAPPS_DETECT_WAKEUP, "APPS_DETECT_WAKEUP" },
	{ DEMAPPS_END_APPS_TCXO, "APPS_END_APPS_TCXO" },
	{ DEMAPPS_ENTER_SLEEPEXIT, "APPS_ENTER_SLEEPEXIT" },
	{ DEMAPPS_END_APPS_SLEEP, "APPS_END_APPS_SLEEP" },
	{ DEMAPPS_SETUP_APPS_PWRCLPS, "APPS_SETUP_APPS_PWRCLPS" },
	{ DEMAPPS_PWRCLPS_EARLY_EXIT, "APPS_PWRCLPS_EARLY_EXIT" },
	{ DEMMOD_SEND_WAKEUP, "MOD_SEND_WAKEUP" },
	{ DEMMOD_NO_APPS_VOTE, "MOD_NO_APPS_VOTE" },
	{ DEMMOD_NO_TCXO_SLEEP, "MOD_NO_TCXO_SLEEP" },
	{ DEMMOD_BT_CLOCK, "MOD_BT_CLOCK" },
	{ DEMMOD_UART_CLOCK, "MOD_UART_CLOCK" },
	{ DEMMOD_OKTS, "MOD_OKTS" },
	{ DEM_SLEEP_INFO, "SLEEP_INFO" },
	{ DEMMOD_TCXO_END, "MOD_TCXO_END" },
	{ DEMMOD_END_SLEEP_SIG, "MOD_END_SLEEP_SIG" },
	{ DEMMOD_SETUP_APPSSLEEP, "MOD_SETUP_APPSSLEEP" },
	{ DEMMOD_ENTER_TCXO, "MOD_ENTER_TCXO" },
	{ DEMMOD_WAKE_APPS, "MOD_WAKE_APPS" },
	{ DEMMOD_POWER_COLLAPSE_APPS, "MOD_POWER_COLLAPSE_APPS" },
	{ DEMMOD_RESTORE_APPS_PWR, "MOD_RESTORE_APPS_PWR" },
	{ DEMAPPS_ASSERT_OKTS, "APPS_ASSERT_OKTS" },
	{ DEMAPPS_RESTART_START_TIMER, "APPS_RESTART_START_TIMER" },
	{ DEMAPPS_ENTER_RUN, "APPS_ENTER_RUN" },
	{ DEMMOD_MAO_INTS, "MOD_MAO_INTS" },
	{ DEMMOD_POWERUP_APPS_CALLED, "MOD_POWERUP_APPS_CALLED" },
	{ DEMMOD_PC_TIMER_EXPIRED, "MOD_PC_TIMER_EXPIRED" },
	{ DEM_DETECT_SLEEPEXIT, "_DETECT_SLEEPEXIT" },
	{ DEM_DETECT_RUN, "DETECT_RUN" },
	{ DEM_SET_APPS_TIMER, "SET_APPS_TIMER" },
	{ DEM_NEGATE_OKTS, "NEGATE_OKTS" },
	{ DEMMOD_APPS_WAKEUP_INT, "MOD_APPS_WAKEUP_INT" },
	{ DEMMOD_APPS_SWFI, "MOD_APPS_SWFI" },
	{ DEM_SEND_BATTERY_INFO, "SEND_BATTERY_INFO" },
	{ DEM_SMI_CLK_DISABLED, "SMI_CLK_DISABLED" },
	{ DEM_SMI_CLK_ENABLED, "SMI_CLK_ENABLED" },
	{ DEMAPPS_SETUP_APPS_SUSPEND, "APPS_SETUP_APPS_SUSPEND" },
	{ DEM_RPC_EARLY_EXIT, "RPC_EARLY_EXIT" },
	{ DEMAPPS_WAKEUP_REASON, "APPS_WAKEUP_REASON" },
	{ DEM_INIT, "INIT" },
#endif
	{ DEMMOD_UMTS_BASE, "MOD_UMTS_BASE" },
	{ DEMMOD_GL1_GO_TO_SLEEP, "GL1_GO_TO_SLEEP" },
	{ DEMMOD_GL1_SLEEP_START, "GL1_SLEEP_START" },
	{ DEMMOD_GL1_AFTER_GSM_CLK_ON, "GL1_AFTER_GSM_CLK_ON" },
	{ DEMMOD_GL1_BEFORE_RF_ON, "GL1_BEFORE_RF_ON" },
	{ DEMMOD_GL1_AFTER_RF_ON, "GL1_AFTER_RF_ON" },
	{ DEMMOD_GL1_FRAME_TICK, "GL1_FRAME_TICK" },
	{ DEMMOD_GL1_WCDMA_START, "GL1_WCDMA_START" },
	{ DEMMOD_GL1_WCDMA_ENDING, "GL1_WCDMA_ENDING" },
	{ DEMMOD_UMTS_NOT_OKTS, "UMTS_NOT_OKTS" },
	{ DEMMOD_UMTS_START_TCXO_SHUTDOWN, "UMTS_START_TCXO_SHUTDOWN" },
	{ DEMMOD_UMTS_END_TCXO_SHUTDOWN, "UMTS_END_TCXO_SHUTDOWN" },
	{ DEMMOD_UMTS_START_ARM_HALT, "UMTS_START_ARM_HALT" },
	{ DEMMOD_UMTS_END_ARM_HALT, "UMTS_END_ARM_HALT" },
	{ DEMMOD_UMTS_NEXT_WAKEUP_SCLK, "UMTS_NEXT_WAKEUP_SCLK" },
	{ TIME_REMOTE_LOG_EVENT_START, "START" },
	{ TIME_REMOTE_LOG_EVENT_GOTO_WAIT,
	  "GOTO_WAIT" },
	{ TIME_REMOTE_LOG_EVENT_GOTO_INIT,
	  "GOTO_INIT" },
	{ ERR_ERROR_FATAL, "ERR_ERROR_FATAL" },
	{ ERR_ERROR_FATAL_TASK, "ERR_ERROR_FATAL_TASK" },
	{ DCVSAPPS_LOG_IDLE, "DCVSAPPS_LOG_IDLE" },
	{ DCVSAPPS_LOG_ERR, "DCVSAPPS_LOG_ERR" },
	{ DCVSAPPS_LOG_CHG, "DCVSAPPS_LOG_CHG" },
	{ DCVSAPPS_LOG_REG, "DCVSAPPS_LOG_REG" },
	{ DCVSAPPS_LOG_DEREG, "DCVSAPPS_LOG_DEREG" },
	{ SMEM_LOG_EVENT_CB, "CB" },
	{ SMEM_LOG_EVENT_START, "START" },
	{ SMEM_LOG_EVENT_INIT, "INIT" },
	{ SMEM_LOG_EVENT_RUNNING, "RUNNING" },
	{ SMEM_LOG_EVENT_STOP, "STOP" },
	{ SMEM_LOG_EVENT_RESTART, "RESTART" },
	{ SMEM_LOG_EVENT_SS, "SS" },
	{ SMEM_LOG_EVENT_READ, "READ" },
	{ SMEM_LOG_EVENT_WRITE, "WRITE" },
	{ SMEM_LOG_EVENT_SIGS1, "SIGS1" },
	{ SMEM_LOG_EVENT_SIGS2, "SIGS2" },
	{ SMEM_LOG_EVENT_WRITE_DM, "WRITE_DM" },
	{ SMEM_LOG_EVENT_READ_DM, "READ_DM" },
	{ SMEM_LOG_EVENT_SKIP_DM, "SKIP_DM" },
	{ SMEM_LOG_EVENT_STOP_DM, "STOP_DM" },
	{ SMEM_LOG_EVENT_ISR, "ISR" },
	{ SMEM_LOG_EVENT_TASK, "TASK" },
	{ SMEM_LOG_EVENT_RS, "RS" },
	{ ONCRPC_LOG_EVENT_SMD_WAIT, "SMD_WAIT" },
	{ ONCRPC_LOG_EVENT_RPC_WAIT, "RPC_WAIT" },
	{ ONCRPC_LOG_EVENT_RPC_BOTH_WAIT, "RPC_BOTH_WAIT" },
	{ ONCRPC_LOG_EVENT_RPC_INIT, "RPC_INIT" },
	{ ONCRPC_LOG_EVENT_RUNNING, "RUNNING" },
	{ ONCRPC_LOG_EVENT_APIS_INITED, "APIS_INITED" },
	{ ONCRPC_LOG_EVENT_AMSS_RESET, "AMSS_RESET" },
	{ ONCRPC_LOG_EVENT_SMD_RESET, "SMD_RESET" },
	{ ONCRPC_LOG_EVENT_ONCRPC_RESET, "ONCRPC_RESET" },
	{ ONCRPC_LOG_EVENT_CB, "CB" },
	{ ONCRPC_LOG_EVENT_STD_CALL, "STD_CALL" },
	{ ONCRPC_LOG_EVENT_STD_REPLY, "STD_REPLY" },
	{ ONCRPC_LOG_EVENT_STD_CALL_ASYNC, "STD_CALL_ASYNC" },
	{ NO_SLEEP_OLD, "NO_SLEEP_OLD" },
	{ INSUF_TIME, "INSUF_TIME" },
	{ MOD_UART_CLOCK, "MOD_UART_CLOCK" },
	{ SLEEP_INFO, "SLEEP_INFO" },
	{ MOD_TCXO_END, "MOD_TCXO_END" },
	{ MOD_ENTER_TCXO, "MOD_ENTER_TCXO" },
	{ NO_SLEEP_NEW, "NO_SLEEP_NEW" },
	{ RPC_ROUTER_LOG_EVENT_UNKNOWN, "UNKNOWN" },
	{ RPC_ROUTER_LOG_EVENT_MSG_READ, "MSG_READ" },
	{ RPC_ROUTER_LOG_EVENT_MSG_WRITTEN, "MSG_WRITTEN" },
	{ RPC_ROUTER_LOG_EVENT_MSG_CFM_REQ, "MSG_CFM_REQ" },
	{ RPC_ROUTER_LOG_EVENT_MSG_CFM_SNT, "MSG_CFM_SNT" },
	{ RPC_ROUTER_LOG_EVENT_MID_READ, "MID_READ" },
	{ RPC_ROUTER_LOG_EVENT_MID_WRITTEN, "MID_WRITTEN" },
	{ RPC_ROUTER_LOG_EVENT_MID_CFM_REQ, "MID_CFM_REQ" },
};

struct sym wakeup_syms[] = {
	{ 0x00000040, "OTHER" },
	{ 0x00000020, "RESET" },
	{ 0x00000010, "ALARM" },
	{ 0x00000008, "TIMER" },
	{ 0x00000004, "GPIO" },
	{ 0x00000002, "INT" },
	{ 0x00000001, "RPC" },
	{ 0x00000000, "NONE" },
};

struct sym wakeup_int_syms[] = {
	{ 0, "MDDI_EXT" },
	{ 1, "MDDI_PRI" },
	{ 2, "MDDI_CLIENT"},
	{ 3, "USB_OTG" },
	{ 4, "I2CC" },
	{ 5, "SDC1_0" },
	{ 6, "SDC1_1" },
	{ 7, "SDC2_0" },
	{ 8, "SDC2_1" },
	{ 9, "ADSP_A9A11" },
	{ 10, "UART1" },
	{ 11, "UART2" },
	{ 12, "UART3" },
	{ 13, "DP_RX_DATA" },
	{ 14, "DP_RX_DATA2" },
	{ 15, "DP_RX_DATA3" },
	{ 16, "DM_UART" },
	{ 17, "DM_DP_RX_DATA" },
	{ 18, "KEYSENSE" },
	{ 19, "HSSD" },
	{ 20, "NAND_WR_ER_DONE" },
	{ 21, "NAND_OP_DONE" },
	{ 22, "TCHSCRN1" },
	{ 23, "TCHSCRN2" },
	{ 24, "TCHSCRN_SSBI" },
	{ 25, "USB_HS" },
	{ 26, "UART2_DM_RX" },
	{ 27, "UART2_DM" },
	{ 28, "SDC4_1" },
	{ 29, "SDC4_0" },
	{ 30, "SDC3_1" },
	{ 31, "SDC3_0" },
};

struct sym smsm_syms[] = {
	{ 0x80000000, "UN" },
	{ 0x7F000000, "ERR" },
	{ 0x00800000, "SMLP" },
	{ 0x00400000, "ADWN" },
	{ 0x00200000, "PWRS" },
	{ 0x00100000, "DWLD" },
	{ 0x00080000, "SRBT" },
	{ 0x00040000, "SDWN" },
	{ 0x00020000, "ARBT" },
	{ 0x00010000, "REL" },
	{ 0x00008000, "SLE" },
	{ 0x00004000, "SLP" },
	{ 0x00002000, "WFPI" },
	{ 0x00001000, "EEX" },
	{ 0x00000800, "TIN" },
	{ 0x00000400, "TWT" },
	{ 0x00000200, "PWRC" },
	{ 0x00000100, "RUN" },
	{ 0x00000080, "SA" },
	{ 0x00000040, "RES" },
	{ 0x00000020, "RIN" },
	{ 0x00000010, "RWT" },
	{ 0x00000008, "SIN" },
	{ 0x00000004, "SWT" },
	{ 0x00000002, "OE" },
	{ 0x00000001, "I" },
};

/* never reorder */
struct sym voter_d2_syms[] = {
	{ 0x00000001, NULL },
	{ 0x00000002, NULL },
	{ 0x00000004, NULL },
	{ 0x00000008, NULL },
	{ 0x00000010, NULL },
	{ 0x00000020, NULL },
	{ 0x00000040, NULL },
	{ 0x00000080, NULL },
	{ 0x00000100, NULL },
	{ 0x00000200, NULL },
	{ 0x00000400, NULL },
	{ 0x00000800, NULL },
	{ 0x00001000, NULL },
	{ 0x00002000, NULL },
	{ 0x00004000, NULL },
	{ 0x00008000, NULL },
	{ 0x00010000, NULL },
	{ 0x00020000, NULL },
	{ 0x00040000, NULL },
	{ 0x00080000, NULL },
	{ 0x00100000, NULL },
	{ 0x00200000, NULL },
	{ 0x00400000, NULL },
	{ 0x00800000, NULL },
	{ 0x01000000, NULL },
	{ 0x02000000, NULL },
	{ 0x04000000, NULL },
	{ 0x08000000, NULL },
	{ 0x10000000, NULL },
	{ 0x20000000, NULL },
	{ 0x40000000, NULL },
	{ 0x80000000, NULL },
};

/* never reorder */
struct sym voter_d3_syms[] = {
	{ 0x00000001, NULL },
	{ 0x00000002, NULL },
	{ 0x00000004, NULL },
	{ 0x00000008, NULL },
	{ 0x00000010, NULL },
	{ 0x00000020, NULL },
	{ 0x00000040, NULL },
	{ 0x00000080, NULL },
	{ 0x00000100, NULL },
	{ 0x00000200, NULL },
	{ 0x00000400, NULL },
	{ 0x00000800, NULL },
	{ 0x00001000, NULL },
	{ 0x00002000, NULL },
	{ 0x00004000, NULL },
	{ 0x00008000, NULL },
	{ 0x00010000, NULL },
	{ 0x00020000, NULL },
	{ 0x00040000, NULL },
	{ 0x00080000, NULL },
	{ 0x00100000, NULL },
	{ 0x00200000, NULL },
	{ 0x00400000, NULL },
	{ 0x00800000, NULL },
	{ 0x01000000, NULL },
	{ 0x02000000, NULL },
	{ 0x04000000, NULL },
	{ 0x08000000, NULL },
	{ 0x10000000, NULL },
	{ 0x20000000, NULL },
	{ 0x40000000, NULL },
	{ 0x80000000, NULL },
};

struct sym dem_state_master_syms[] = {
	{ 0, "INIT" },
	{ 1, "RUN" },
	{ 2, "SLEEP_WAIT" },
	{ 3, "SLEEP_CONFIRMED" },
	{ 4, "SLEEP_EXIT" },
	{ 5, "RSA" },
	{ 6, "EARLY_EXIT" },
	{ 7, "RSA_DELAYED" },
	{ 8, "RSA_CHECK_INTS" },
	{ 9, "RSA_CONFIRMED" },
	{ 10, "RSA_WAKING" },
	{ 11, "RSA_RESTORE" },
	{ 12, "RESET" },
};

struct sym dem_state_slave_syms[] = {
	{ 0, "INIT" },
	{ 1, "RUN" },
	{ 2, "SLEEP_WAIT" },
	{ 3, "SLEEP_EXIT" },
	{ 4, "SLEEP_RUN_PENDING" },
	{ 5, "POWER_COLLAPSE" },
	{ 6, "CHECK_INTERRUPTS" },
	{ 7, "SWFI" },
	{ 8, "WFPI" },
	{ 9, "EARLY_EXIT" },
	{ 10, "RESET_RECOVER" },
	{ 11, "RESET_ACKNOWLEDGE" },
	{ 12, "ERROR" },
};

struct sym smsm_entry_type_syms[] = {
	{ 0, "SMSM_APPS_STATE" },
	{ 1, "SMSM_MODEM_STATE" },
	{ 2, "SMSM_Q6_STATE" },
	{ 3, "SMSM_APPS_DEM" },
	{ 4, "SMSM_MODEM_DEM" },
	{ 5, "SMSM_Q6_DEM" },
	{ 6, "SMSM_POWER_MASTER_DEM" },
	{ 7, "SMSM_TIME_MASTER_DEM" },
};

struct sym smsm_state_syms[] = {
	{ 0x00000001, "INIT" },
	{ 0x00000002, "OSENTERED" },
	{ 0x00000004, "SMDWAIT" },
	{ 0x00000008, "SMDINIT" },
	{ 0x00000010, "RPCWAIT" },
	{ 0x00000020, "RPCINIT" },
	{ 0x00000040, "RESET" },
	{ 0x00000080, "RSA" },
	{ 0x00000100, "RUN" },
	{ 0x00000200, "PWRC" },
	{ 0x00000400, "TIMEWAIT" },
	{ 0x00000800, "TIMEINIT" },
	{ 0x00001000, "PWRC_EARLY_EXIT" },
	{ 0x00002000, "WFPI" },
	{ 0x00004000, "SLEEP" },
	{ 0x00008000, "SLEEPEXIT" },
	{ 0x00010000, "OEMSBL_RELEASE" },
	{ 0x00020000, "APPS_REBOOT" },
	{ 0x00040000, "SYSTEM_POWER_DOWN" },
	{ 0x00080000, "SYSTEM_REBOOT" },
	{ 0x00100000, "SYSTEM_DOWNLOAD" },
	{ 0x00200000, "PWRC_SUSPEND" },
	{ 0x00400000, "APPS_SHUTDOWN" },
	{ 0x00800000, "SMD_LOOPBACK" },
	{ 0x01000000, "RUN_QUIET" },
	{ 0x02000000, "MODEM_WAIT" },
	{ 0x04000000, "MODEM_BREAK" },
	{ 0x08000000, "MODEM_CONTINUE" },
	{ 0x80000000, "UNKNOWN" },
};

#define ID_SYM 0
#define BASE_SYM 1
#define EVENT_SYM 2
#define WAKEUP_SYM 3
#define WAKEUP_INT_SYM 4
#define SMSM_SYM 5
#define VOTER_D2_SYM 6
#define VOTER_D3_SYM 7
#define DEM_STATE_MASTER_SYM 8
#define DEM_STATE_SLAVE_SYM 9
#define SMSM_ENTRY_TYPE_SYM 10
#define SMSM_STATE_SYM 11

static struct sym_tbl {
	struct sym *data;
	int size;
	struct hlist_head hlist[HSIZE];
} tbl[] = {
	{ id_syms, ARRAY_SIZE(id_syms) },
	{ base_syms, ARRAY_SIZE(base_syms) },
	{ event_syms, ARRAY_SIZE(event_syms) },
	{ wakeup_syms, ARRAY_SIZE(wakeup_syms) },
	{ wakeup_int_syms, ARRAY_SIZE(wakeup_int_syms) },
	{ smsm_syms, ARRAY_SIZE(smsm_syms) },
	{ voter_d2_syms, ARRAY_SIZE(voter_d2_syms) },
	{ voter_d3_syms, ARRAY_SIZE(voter_d3_syms) },
	{ dem_state_master_syms, ARRAY_SIZE(dem_state_master_syms) },
	{ dem_state_slave_syms, ARRAY_SIZE(dem_state_slave_syms) },
	{ smsm_entry_type_syms, ARRAY_SIZE(smsm_entry_type_syms) },
	{ smsm_state_syms, ARRAY_SIZE(smsm_state_syms) },
};

static void find_voters(void)
{
	void *x, *next;
	unsigned size;
	int i = 0, j = 0;

	x = smem_get_entry(SMEM_SLEEP_STATIC, &size);
	next = x;
	while (next && (next < (x + size)) &&
	       ((i + j) < (ARRAY_SIZE(voter_d3_syms) +
			   ARRAY_SIZE(voter_d2_syms)))) {

		if (i < ARRAY_SIZE(voter_d3_syms)) {
			voter_d3_syms[i].str = (char *) next;
			i++;
		} else if (i >= ARRAY_SIZE(voter_d3_syms) &&
			   j < ARRAY_SIZE(voter_d2_syms)) {
			voter_d2_syms[j].str = (char *) next;
			j++;
		}

		next += 9;
	}
}

#define hash(val) (val % HSIZE)

static void init_syms(void)
{
	int i;
	int j;

	for (i = 0; i < ARRAY_SIZE(tbl); ++i)
		for (j = 0; j < HSIZE; ++j)
			INIT_HLIST_HEAD(&tbl[i].hlist[j]);

	for (i = 0; i < ARRAY_SIZE(tbl); ++i)
		for (j = 0; j < tbl[i].size; ++j) {
			INIT_HLIST_NODE(&tbl[i].data[j].node);
			hlist_add_head(&tbl[i].data[j].node,
				       &tbl[i].hlist[hash(tbl[i].data[j].val)]);
		}
}

static char *find_sym(uint32_t id, uint32_t val)
{
	struct hlist_node *n;
	struct sym *s;

	hlist_for_each(n, &tbl[id].hlist[hash(val)]) {
		s = hlist_entry(n, struct sym, node);
		if (s->val == val)
			return s->str;
	}

	return 0;
}

#else
static void init_syms(void) {}
#endif

static inline unsigned int read_timestamp(void)
{
	unsigned int tick = 0;

	/* no barriers necessary as the read value is a dependency for the
	 * comparison operation so the processor shouldn't be able to
	 * reorder things
	 */
	do {
		tick = __raw_readl(TIMESTAMP_ADDR);
	} while (tick != __raw_readl(TIMESTAMP_ADDR));

	return tick;
}

static void smem_log_event_from_user(struct smem_log_inst *inst,
				     const char __user *buf, int size, int num)
{
	uint32_t idx;
	uint32_t next_idx;
	unsigned long flags;
	uint32_t identifier = 0;
	uint32_t timetick = 0;
	int first = 1;
	int ret;

	remote_spin_lock_irqsave(inst->remote_spinlock, flags);

	while (num--) {
		idx = *inst->idx;

		if (idx < inst->num) {
			ret = copy_from_user(&inst->events[idx],
					     buf, size);
			if (ret) {
				printk("ERROR %s:%i tried to write "
				       "%i got ret %i",
				       __func__, __LINE__,
				       size, size - ret);
				goto out;
			}

			if (first) {
				identifier =
					inst->events[idx].
					identifier;
				timetick = read_timestamp();
				first = 0;
			} else {
				identifier |= SMEM_LOG_CONT;
			}
			inst->events[idx].identifier =
				identifier;
			inst->events[idx].timetick =
				timetick;
		}

		next_idx = idx + 1;
		if (next_idx >= inst->num)
			next_idx = 0;
		*inst->idx = next_idx;
		buf += sizeof(struct smem_log_item);
	}

 out:
	wmb();
	remote_spin_unlock_irqrestore(inst->remote_spinlock, flags);
}

static void _smem_log_event(
	struct smem_log_item __iomem *events,
	uint32_t __iomem *_idx,
	remote_spinlock_t *lock,
	int num,
	uint32_t id, uint32_t data1, uint32_t data2,
	uint32_t data3)
{
	struct smem_log_item item;
	uint32_t idx;
	uint32_t next_idx;
	unsigned long flags;

	item.timetick = read_timestamp();
	item.identifier = id;
	item.data1 = data1;
	item.data2 = data2;
	item.data3 = data3;

	remote_spin_lock_irqsave(lock, flags);

	idx = *_idx;

	if (idx < num) {
		memcpy(&events[idx],
		       &item, sizeof(item));
	}

	next_idx = idx + 1;
	if (next_idx >= num)
		next_idx = 0;
	*_idx = next_idx;
	wmb();

	remote_spin_unlock_irqrestore(lock, flags);
}

static void _smem_log_event6(
	struct smem_log_item __iomem *events,
	uint32_t __iomem *_idx,
	remote_spinlock_t *lock,
	int num,
	uint32_t id, uint32_t data1, uint32_t data2,
	uint32_t data3, uint32_t data4, uint32_t data5,
	uint32_t data6)
{
	struct smem_log_item item[2];
	uint32_t idx;
	uint32_t next_idx;
	unsigned long flags;

	item[0].timetick = read_timestamp();
	item[0].identifier = id;
	item[0].data1 = data1;
	item[0].data2 = data2;
	item[0].data3 = data3;
	item[1].identifier = item[0].identifier;
	item[1].timetick = item[0].timetick;
	item[1].data1 = data4;
	item[1].data2 = data5;
	item[1].data3 = data6;

	remote_spin_lock_irqsave(lock, flags);

	idx = *_idx;

	/* FIXME: Wrap around */
	if (idx < (num-1)) {
		memcpy(&events[idx],
			&item, sizeof(item));
	}

	next_idx = idx + 2;
	if (next_idx >= num)
		next_idx = 0;
	*_idx = next_idx;

	wmb();
	remote_spin_unlock_irqrestore(lock, flags);
}

void smem_log_event(uint32_t id, uint32_t data1, uint32_t data2,
		    uint32_t data3)
{
	if (smem_log_enable)
		_smem_log_event(inst[GEN].events, inst[GEN].idx,
				inst[GEN].remote_spinlock,
				SMEM_LOG_NUM_ENTRIES, id,
				data1, data2, data3);
}

void smem_log_event6(uint32_t id, uint32_t data1, uint32_t data2,
		     uint32_t data3, uint32_t data4, uint32_t data5,
		     uint32_t data6)
{
	if (smem_log_enable)
		_smem_log_event6(inst[GEN].events, inst[GEN].idx,
				 inst[GEN].remote_spinlock,
				 SMEM_LOG_NUM_ENTRIES, id,
				 data1, data2, data3, data4, data5, data6);
}

void smem_log_event_to_static(uint32_t id, uint32_t data1, uint32_t data2,
		    uint32_t data3)
{
	if (smem_log_enable)
		_smem_log_event(inst[STA].events, inst[STA].idx,
				inst[STA].remote_spinlock,
				SMEM_LOG_NUM_STATIC_ENTRIES, id,
				data1, data2, data3);
}

void smem_log_event6_to_static(uint32_t id, uint32_t data1, uint32_t data2,
		     uint32_t data3, uint32_t data4, uint32_t data5,
		     uint32_t data6)
{
	if (smem_log_enable)
		_smem_log_event6(inst[STA].events, inst[STA].idx,
				 inst[STA].remote_spinlock,
				 SMEM_LOG_NUM_STATIC_ENTRIES, id,
				 data1, data2, data3, data4, data5, data6);
}

static int _smem_log_init(void)
{
	int ret;

	inst[GEN].which_log = GEN;
	inst[GEN].events =
		(struct smem_log_item *)smem_alloc(SMEM_SMEM_LOG_EVENTS,
						  SMEM_LOG_EVENTS_SIZE);
	inst[GEN].idx = (uint32_t *)smem_alloc(SMEM_SMEM_LOG_IDX,
					     sizeof(uint32_t));
	if (!inst[GEN].events || !inst[GEN].idx)
		pr_info("%s: no log or log_idx allocated\n", __func__);

	inst[GEN].num = SMEM_LOG_NUM_ENTRIES;
	inst[GEN].read_idx = 0;
	inst[GEN].last_read_avail = SMEM_LOG_NUM_ENTRIES;
	init_waitqueue_head(&inst[GEN].read_wait);
	inst[GEN].remote_spinlock = &remote_spinlock;

	inst[STA].which_log = STA;
	inst[STA].events =
		(struct smem_log_item *)
		smem_alloc(SMEM_SMEM_STATIC_LOG_EVENTS,
			   SMEM_STATIC_LOG_EVENTS_SIZE);
	inst[STA].idx = (uint32_t *)smem_alloc(SMEM_SMEM_STATIC_LOG_IDX,
						     sizeof(uint32_t));
	if (!inst[STA].events || !inst[STA].idx)
		pr_info("%s: no static log or log_idx allocated\n", __func__);

	inst[STA].num = SMEM_LOG_NUM_STATIC_ENTRIES;
	inst[STA].read_idx = 0;
	inst[STA].last_read_avail = SMEM_LOG_NUM_ENTRIES;
	init_waitqueue_head(&inst[STA].read_wait);
	inst[STA].remote_spinlock = &remote_spinlock_static;

	inst[POW].which_log = POW;
	inst[POW].events =
		(struct smem_log_item *)
		smem_alloc(SMEM_SMEM_LOG_POWER_EVENTS,
			   SMEM_POWER_LOG_EVENTS_SIZE);
	inst[POW].idx = (uint32_t *)smem_alloc(SMEM_SMEM_LOG_POWER_IDX,
						     sizeof(uint32_t));
	if (!inst[POW].events || !inst[POW].idx)
		pr_info("%s: no power log or log_idx allocated\n", __func__);

	inst[POW].num = SMEM_LOG_NUM_POWER_ENTRIES;
	inst[POW].read_idx = 0;
	inst[POW].last_read_avail = SMEM_LOG_NUM_ENTRIES;
	init_waitqueue_head(&inst[POW].read_wait);
	inst[POW].remote_spinlock = &remote_spinlock;

	ret = remote_spin_lock_init(&remote_spinlock,
			      SMEM_SPINLOCK_SMEM_LOG);
	if (ret) {
		mb();
		return ret;
	}

	ret = remote_spin_lock_init(&remote_spinlock_static,
			      SMEM_SPINLOCK_STATIC_LOG);
	if (ret) {
		mb();
		return ret;
	}

	init_syms();
	mb();

	return 0;
}

static ssize_t smem_log_read_bin(struct file *fp, char __user *buf,
			size_t count, loff_t *pos)
{
	int idx;
	int orig_idx;
	unsigned long flags;
	int ret;
	int tot_bytes = 0;
	struct smem_log_inst *inst;

	inst = fp->private_data;

	remote_spin_lock_irqsave(inst->remote_spinlock, flags);

	orig_idx = *inst->idx;
	idx = orig_idx;

	while (1) {
		idx--;
		if (idx < 0)
			idx = inst->num - 1;
		if (idx == orig_idx) {
			ret = tot_bytes;
			break;
		}

		if ((tot_bytes + sizeof(struct smem_log_item)) > count) {
			ret = tot_bytes;
			break;
		}

		ret = copy_to_user(buf, &inst[GEN].events[idx],
				   sizeof(struct smem_log_item));
		if (ret) {
			ret = -EIO;
			break;
		}

		tot_bytes += sizeof(struct smem_log_item);

		buf += sizeof(struct smem_log_item);
	}

	remote_spin_unlock_irqrestore(inst->remote_spinlock, flags);

	return ret;
}

static ssize_t smem_log_read(struct file *fp, char __user *buf,
			size_t count, loff_t *pos)
{
	char loc_buf[128];
	int i;
	int idx;
	int orig_idx;
	unsigned long flags;
	int ret;
	int tot_bytes = 0;
	struct smem_log_inst *inst;

	inst = fp->private_data;

	remote_spin_lock_irqsave(inst->remote_spinlock, flags);

	orig_idx = *inst->idx;
	idx = orig_idx;

	while (1) {
		idx--;
		if (idx < 0)
			idx = inst->num - 1;
		if (idx == orig_idx) {
			ret = tot_bytes;
			break;
		}

		i = scnprintf(loc_buf, 128,
			      "0x%x 0x%x 0x%x 0x%x 0x%x\n",
			      inst->events[idx].identifier,
			      inst->events[idx].timetick,
			      inst->events[idx].data1,
			      inst->events[idx].data2,
			      inst->events[idx].data3);
		if (i == 0) {
			ret = -EIO;
			break;
		}

		if ((tot_bytes + i) > count) {
			ret = tot_bytes;
			break;
		}

		tot_bytes += i;

		ret = copy_to_user(buf, loc_buf, i);
		if (ret) {
			ret = -EIO;
			break;
		}

		buf += i;
	}

	remote_spin_unlock_irqrestore(inst->remote_spinlock, flags);

	return ret;
}

static ssize_t smem_log_write_bin(struct file *fp, const char __user *buf,
			 size_t count, loff_t *pos)
{
	if (count < sizeof(struct smem_log_item))
		return -EINVAL;

	if (smem_log_enable)
		smem_log_event_from_user(fp->private_data, buf,
					sizeof(struct smem_log_item),
					count / sizeof(struct smem_log_item));
	return count;
}

static ssize_t smem_log_write(struct file *fp, const char __user *buf,
			 size_t count, loff_t *pos)
{
	int ret;
	const char delimiters[] = " ,;";
	char locbuf[256] = {0};
	uint32_t val[10] = {0};
	int vals = 0;
	char *token;
	char *running;
	struct smem_log_inst *inst;
	unsigned long res;

	inst = fp->private_data;

	count = count > 255 ? 255 : count;

	if (!smem_log_enable)
		return count;

	locbuf[count] = '\0';

	ret = copy_from_user(locbuf, buf, count);
	if (ret != 0) {
		printk(KERN_ERR "ERROR: %s could not copy %i bytes\n",
		       __func__, ret);
		return -EINVAL;
	}

	D(KERN_ERR "%s: ", __func__);
	D_DUMP_BUFFER("We got", len, locbuf);

	running = locbuf;

	token = strsep(&running, delimiters);
	while (token && vals < ARRAY_SIZE(val)) {
		if (*token != '\0') {
			D(KERN_ERR "%s: ", __func__);
			D_DUMP_BUFFER("", strlen(token), token);
			ret = strict_strtoul(token, 0, &res);
			if (ret) {
				printk(KERN_ERR "ERROR: %s:%i got bad char "
				       "at strict_strtoul\n",
				       __func__, __LINE__-4);
				return -EINVAL;
			}
			val[vals++] = res;
		}
		token = strsep(&running, delimiters);
	}

	if (vals > 5) {
		if (inst->which_log == GEN)
			smem_log_event6(val[0], val[2], val[3], val[4],
					val[7], val[8], val[9]);
		else if (inst->which_log == STA)
			smem_log_event6_to_static(val[0],
						  val[2], val[3], val[4],
						  val[7], val[8], val[9]);
		else
			return -1;
	} else {
		if (inst->which_log == GEN)
			smem_log_event(val[0], val[2], val[3], val[4]);
		else if (inst->which_log == STA)
			smem_log_event_to_static(val[0],
						 val[2], val[3], val[4]);
		else
			return -1;
	}

	return count;
}

static int smem_log_open(struct inode *ip, struct file *fp)
{
	fp->private_data = &inst[GEN];

	return 0;
}


static int smem_log_release(struct inode *ip, struct file *fp)
{
	return 0;
}

static long smem_log_ioctl(struct file *fp, unsigned int cmd,
					   unsigned long arg);

static const struct file_operations smem_log_fops = {
	.owner = THIS_MODULE,
	.read = smem_log_read,
	.write = smem_log_write,
	.open = smem_log_open,
	.release = smem_log_release,
	.unlocked_ioctl = smem_log_ioctl,
};

static const struct file_operations smem_log_bin_fops = {
	.owner = THIS_MODULE,
	.read = smem_log_read_bin,
	.write = smem_log_write_bin,
	.open = smem_log_open,
	.release = smem_log_release,
	.unlocked_ioctl = smem_log_ioctl,
};

static long smem_log_ioctl(struct file *fp,
			  unsigned int cmd, unsigned long arg)
{
	struct smem_log_inst *inst;

	inst = fp->private_data;

	switch (cmd) {
	default:
		return -ENOTTY;

	case SMIOC_SETMODE:
		if (arg == SMIOC_TEXT) {
			D("%s set text mode\n", __func__);
			fp->f_op = &smem_log_fops;
		} else if (arg == SMIOC_BINARY) {
			D("%s set bin mode\n", __func__);
			fp->f_op = &smem_log_bin_fops;
		} else {
			return -EINVAL;
		}
		break;
	case SMIOC_SETLOG:
		if (arg == SMIOC_LOG)
			fp->private_data = &inst[GEN];
		else if (arg == SMIOC_STATIC_LOG)
			fp->private_data = &inst[STA];
		else
			return -EINVAL;
		break;
	}

	return 0;
}

static struct miscdevice smem_log_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smem_log",
	.fops = &smem_log_fops,
};

#if defined(CONFIG_DEBUG_FS)

#define SMEM_LOG_ITEM_PRINT_SIZE 160

#define EVENTS_PRINT_SIZE \
(SMEM_LOG_ITEM_PRINT_SIZE * SMEM_LOG_NUM_ENTRIES)

static uint32_t smem_log_timeout_ms;
module_param_named(timeout_ms, smem_log_timeout_ms,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

static int smem_log_debug_mask;
module_param_named(debug_mask, smem_log_debug_mask, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

#define DBG(x...) do {\
	if (smem_log_debug_mask) \
		printk(KERN_DEBUG x);\
	} while (0)

static int update_read_avail(struct smem_log_inst *inst)
{
	int curr_read_avail;
	unsigned long flags = 0;

	remote_spin_lock_irqsave(inst->remote_spinlock, flags);

	curr_read_avail = (*inst->idx - inst->read_idx);
	if (curr_read_avail < 0)
		curr_read_avail = inst->num - inst->read_idx + *inst->idx;

	DBG("%s: read = %d write = %d curr = %d last = %d\n", __func__,
	    inst->read_idx, *inst->idx, curr_read_avail, inst->last_read_avail);

	if (curr_read_avail < inst->last_read_avail) {
		if (inst->last_read_avail != inst->num)
			pr_info("smem_log: skipping %d log entries\n",
				inst->last_read_avail);
		inst->read_idx = *inst->idx + 1;
		inst->last_read_avail = inst->num - 1;
	} else
		inst->last_read_avail = curr_read_avail;

	remote_spin_unlock_irqrestore(inst->remote_spinlock, flags);

	DBG("%s: read = %d write = %d curr = %d last = %d\n", __func__,
	    inst->read_idx, *inst->idx, curr_read_avail, inst->last_read_avail);

	return inst->last_read_avail;
}

static int _debug_dump(int log, char *buf, int max, uint32_t cont)
{
	unsigned int idx;
	int write_idx, read_avail = 0;
	unsigned long flags;
	int i = 0;

	if (!inst[log].events)
		return 0;

	if (cont && update_read_avail(&inst[log]) == 0)
		return 0;

	remote_spin_lock_irqsave(inst[log].remote_spinlock, flags);

	if (cont) {
		idx = inst[log].read_idx;
		write_idx = (inst[log].read_idx + inst[log].last_read_avail);
		if (write_idx >= inst[log].num)
			write_idx -= inst[log].num;
	} else {
		write_idx = *inst[log].idx;
		idx = (write_idx + 1);
	}

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num - 1);

	while ((max - i) > 50) {
		if ((inst[log].num - 1) < idx)
			idx = 0;

		if (idx == write_idx)
			break;

		if (inst[log].events[idx].identifier) {

			i += scnprintf(buf + i, max - i,
				       "%08x %08x %08x %08x %08x\n",
				       inst[log].events[idx].identifier,
				       inst[log].events[idx].timetick,
				       inst[log].events[idx].data1,
				       inst[log].events[idx].data2,
				       inst[log].events[idx].data3);
		}
		idx++;
	}
	if (cont) {
		inst[log].read_idx = idx;
		read_avail = (write_idx - inst[log].read_idx);
		if (read_avail < 0)
			read_avail = inst->num - inst->read_idx + write_idx;
		inst[log].last_read_avail = read_avail;
	}

	remote_spin_unlock_irqrestore(inst[log].remote_spinlock, flags);

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num);

	return i;
}

static int _debug_dump_voters(char *buf, int max)
{
	int k, i = 0;

	find_voters();

	i += scnprintf(buf + i, max - i, "Voters:\n");
	for (k = 0; k < ARRAY_SIZE(voter_d3_syms); ++k)
		if (voter_d3_syms[k].str)
			i += scnprintf(buf + i, max - i, "%s ",
				       voter_d3_syms[k].str);
	for (k = 0; k < ARRAY_SIZE(voter_d2_syms); ++k)
		if (voter_d2_syms[k].str)
			i += scnprintf(buf + i, max - i, "%s ",
				       voter_d2_syms[k].str);
	i += scnprintf(buf + i, max - i, "\n");

	return i;
}

static int _debug_dump_sym(int log, char *buf, int max, uint32_t cont)
{
	unsigned int idx;
	int write_idx, read_avail = 0;
	unsigned long flags;
	int i = 0;

	char *proc;
	char *sub;
	char *id;
	const char *sym = NULL;

	uint32_t data[3];

	uint32_t proc_val = 0;
	uint32_t sub_val = 0;
	uint32_t id_val = 0;
	uint32_t id_only_val = 0;
	uint32_t data1 = 0;
	uint32_t data2 = 0;
	uint32_t data3 = 0;

	if (!inst[log].events)
		return 0;

	find_voters();

	if (cont && update_read_avail(&inst[log]) == 0)
		return 0;

	remote_spin_lock_irqsave(inst[log].remote_spinlock, flags);

	if (cont) {
		idx = inst[log].read_idx;
		write_idx = (inst[log].read_idx + inst[log].last_read_avail);
		if (write_idx >= inst[log].num)
			write_idx -= inst[log].num;
	} else {
		write_idx = *inst[log].idx;
		idx = (write_idx + 1);
	}

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num - 1);

	for (; (max - i) > SMEM_LOG_ITEM_PRINT_SIZE; idx++) {
		if (idx > (inst[log].num - 1))
			idx = 0;

		if (idx == write_idx)
			break;

		if (idx < inst[log].num) {
			if (!inst[log].events[idx].identifier)
				continue;

			proc_val = PROC & inst[log].events[idx].identifier;
			sub_val = SUB & inst[log].events[idx].identifier;
			id_val = (SUB | ID) & inst[log].events[idx].identifier;
			id_only_val = ID & inst[log].events[idx].identifier;
			data1 = inst[log].events[idx].data1;
			data2 = inst[log].events[idx].data2;
			data3 = inst[log].events[idx].data3;

			if (!(proc_val & SMEM_LOG_CONT)) {
				i += scnprintf(buf + i, max - i, "\n");

				proc = find_sym(ID_SYM, proc_val);

				if (proc)
					i += scnprintf(buf + i, max - i,
						       "%4s: ", proc);
				else
					i += scnprintf(buf + i, max - i,
						       "%04x: ",
						       PROC &
						       inst[log].events[idx].
						       identifier);

				i += scnprintf(buf + i, max - i, "%10u ",
					       inst[log].events[idx].timetick);

				sub = find_sym(BASE_SYM, sub_val);

				if (sub)
					i += scnprintf(buf + i, max - i,
						       "%9s: ", sub);
				else
					i += scnprintf(buf + i, max - i,
						       "%08x: ", sub_val);

				id = find_sym(EVENT_SYM, id_val);

				if (id)
					i += scnprintf(buf + i, max - i,
						       "%11s: ", id);
				else
					i += scnprintf(buf + i, max - i,
						       "%08x: ", id_only_val);
			}

			if ((proc_val & SMEM_LOG_CONT) &&
			    (id_val == ONCRPC_LOG_EVENT_STD_CALL ||
			     id_val == ONCRPC_LOG_EVENT_STD_REPLY)) {
				data[0] = data1;
				data[1] = data2;
				data[2] = data3;
				i += scnprintf(buf + i, max - i,
					       " %.16s", (char *) data);
			} else if (proc_val & SMEM_LOG_CONT) {
				i += scnprintf(buf + i, max - i,
					       " %08x %08x %08x",
					       data1, data2, data3);
			} else if (id_val == ONCRPC_LOG_EVENT_STD_CALL) {
				sym = smd_rpc_get_sym(data2);

				if (sym)
					i += scnprintf(buf + i, max - i,
						       "xid:%4i %8s proc:%3i",
						       data1, sym, data3);
				else
					i += scnprintf(buf + i, max - i,
						       "xid:%4i %08x proc:%3i",
						       data1, data2, data3);
#if defined(CONFIG_MSM_N_WAY_SMSM)
			} else if (id_val == DEM_STATE_CHANGE) {
				if (data1 == 1) {
					i += scnprintf(buf + i, max - i,
						       "MASTER: ");
					sym = find_sym(DEM_STATE_MASTER_SYM,
						       data2);
				} else if (data1 == 0) {
					i += scnprintf(buf + i, max - i,
						       " SLAVE: ");
					sym = find_sym(DEM_STATE_SLAVE_SYM,
						       data2);
				} else {
					i += scnprintf(buf + i, max - i,
						       "%x: ",  data1);
					sym = NULL;
				}
				if (sym)
					i += scnprintf(buf + i, max - i,
						       "from:%s ", sym);
				else
					i += scnprintf(buf + i, max - i,
						       "from:0x%x ", data2);

				if (data1 == 1)
					sym = find_sym(DEM_STATE_MASTER_SYM,
						       data3);
				else if (data1 == 0)
					sym = find_sym(DEM_STATE_SLAVE_SYM,
						       data3);
				else
					sym = NULL;
				if (sym)
					i += scnprintf(buf + i, max - i,
						       "to:%s ", sym);
				else
					i += scnprintf(buf + i, max - i,
						       "to:0x%x ", data3);

			} else if (id_val == DEM_STATE_MACHINE_ENTER) {
				i += scnprintf(buf + i, max - i,
					       "swfi:%i timer:%i manexit:%i",
					       data1, data2, data3);

			} else if (id_val == DEM_TIME_SYNC_REQUEST ||
				   id_val == DEM_TIME_SYNC_POLL ||
				   id_val == DEM_TIME_SYNC_INIT) {
				sym = find_sym(SMSM_ENTRY_TYPE_SYM,
					       data1);
				if (sym)
					i += scnprintf(buf + i, max - i,
						       "hostid:%s", sym);
				else
					i += scnprintf(buf + i, max - i,
						       "hostid:%x", data1);

			} else if (id_val == DEM_TIME_SYNC_START ||
				   id_val == DEM_TIME_SYNC_SEND_VALUE) {
				unsigned mask = 0x1;
				unsigned tmp = 0;
				if (id_val == DEM_TIME_SYNC_START)
					i += scnprintf(buf + i, max - i,
						       "req:");
				else
					i += scnprintf(buf + i, max - i,
						       "pol:");
				while (mask) {
					if (mask & data1) {
						sym = find_sym(
							SMSM_ENTRY_TYPE_SYM,
							tmp);
						if (sym)
							i += scnprintf(buf + i,
								       max - i,
								       "%s ",
								       sym);
						else
							i += scnprintf(buf + i,
								       max - i,
								       "%i ",
								       tmp);
					}
					mask <<= 1;
					tmp++;
				}
				if (id_val == DEM_TIME_SYNC_SEND_VALUE)
					i += scnprintf(buf + i, max - i,
						       "tick:%x", data2);
			} else if (id_val == DEM_SMSM_ISR) {
				unsigned vals[] = {data2, data3};
				unsigned j;
				unsigned mask;
				unsigned tmp;
				unsigned once;
				sym = find_sym(SMSM_ENTRY_TYPE_SYM,
					       data1);
				if (sym)
					i += scnprintf(buf + i, max - i,
						       "%s ", sym);
				else
					i += scnprintf(buf + i, max - i,
						       "%x ", data1);

				for (j = 0; j < ARRAY_SIZE(vals); ++j) {
					i += scnprintf(buf + i, max - i, "[");
					mask = 0x80000000;
					once = 0;
					while (mask) {
						tmp = vals[j] & mask;
						mask >>= 1;
						if (!tmp)
							continue;
						sym = find_sym(SMSM_STATE_SYM,
							       tmp);

						if (once)
							i += scnprintf(buf + i,
								       max - i,
								       " ");
						if (sym)
							i += scnprintf(buf + i,
								       max - i,
								       "%s",
								       sym);
						else
							i += scnprintf(buf + i,
								       max - i,
								       "0x%x",
								       tmp);
						once = 1;
					}
					i += scnprintf(buf + i, max - i, "] ");
				}
#else
			} else if (id_val == DEMAPPS_WAKEUP_REASON) {
				unsigned mask = 0x80000000;
				unsigned tmp = 0;
				while (mask) {
					tmp = data1 & mask;
					mask >>= 1;
					if (!tmp)
						continue;
					sym = find_sym(WAKEUP_SYM, tmp);
					if (sym)
						i += scnprintf(buf + i,
							       max - i,
							       "%s ",
							       sym);
					else
						i += scnprintf(buf + i,
							       max - i,
							       "%08x ",
							       tmp);
				}
				i += scnprintf(buf + i, max - i,
					       "%08x %08x", data2, data3);
			} else if (id_val == DEMMOD_APPS_WAKEUP_INT) {
				sym = find_sym(WAKEUP_INT_SYM, data1);

				if (sym)
					i += scnprintf(buf + i, max - i,
						       "%s %08x %08x",
						       sym, data2, data3);
				else
					i += scnprintf(buf + i, max - i,
						       "%08x %08x %08x",
						       data1, data2, data3);
			} else if (id_val == DEM_NO_SLEEP ||
				   id_val == NO_SLEEP_NEW) {
				unsigned vals[] = {data3, data2};
				unsigned j;
				unsigned mask;
				unsigned tmp;
				unsigned once;
				i += scnprintf(buf + i, max - i, "%08x ",
					       data1);
				i += scnprintf(buf + i, max - i, "[");
				once = 0;
				for (j = 0; j < ARRAY_SIZE(vals); ++j) {
					mask = 0x00000001;
					while (mask) {
						tmp = vals[j] & mask;
						mask <<= 1;
						if (!tmp)
							continue;
						if (j == 0)
							sym = find_sym(
								VOTER_D3_SYM,
								tmp);
						else
							sym = find_sym(
								VOTER_D2_SYM,
								tmp);

						if (once)
							i += scnprintf(buf + i,
								       max - i,
								       " ");
						if (sym)
							i += scnprintf(buf + i,
								       max - i,
								       "%s",
								       sym);
						else
							i += scnprintf(buf + i,
								       max - i,
								       "%08x",
								       tmp);
						once = 1;
					}
				}
				i += scnprintf(buf + i, max - i, "] ");
#endif
			} else if (id_val == SMEM_LOG_EVENT_CB) {
				unsigned vals[] = {data2, data3};
				unsigned j;
				unsigned mask;
				unsigned tmp;
				unsigned once;
				i += scnprintf(buf + i, max - i, "%08x ",
					       data1);
				for (j = 0; j < ARRAY_SIZE(vals); ++j) {
					i += scnprintf(buf + i, max - i, "[");
					mask = 0x80000000;
					once = 0;
					while (mask) {
						tmp = vals[j] & mask;
						mask >>= 1;
						if (!tmp)
							continue;
						sym = find_sym(SMSM_SYM, tmp);

						if (once)
							i += scnprintf(buf + i,
								       max - i,
								       " ");
						if (sym)
							i += scnprintf(buf + i,
								       max - i,
								       "%s",
								       sym);
						else
							i += scnprintf(buf + i,
								       max - i,
								       "%08x",
								       tmp);
						once = 1;
					}
					i += scnprintf(buf + i, max - i, "] ");
				}
			} else {
				i += scnprintf(buf + i, max - i,
					       "%08x %08x %08x",
					       data1, data2, data3);
			}
		}
	}
	if (cont) {
		inst[log].read_idx = idx;
		read_avail = (write_idx - inst[log].read_idx);
		if (read_avail < 0)
			read_avail = inst->num - inst->read_idx + write_idx;
		inst[log].last_read_avail = read_avail;
	}

	remote_spin_unlock_irqrestore(inst[log].remote_spinlock, flags);

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num);

	return i;
}

static int debug_dump(char *buf, int max, uint32_t cont)
{
	int r;
	while (cont) {
		update_read_avail(&inst[GEN]);
		r = wait_event_interruptible_timeout(inst[GEN].read_wait,
						     inst[GEN].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: read available %d\n", __func__,
		    inst[GEN].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[GEN].last_read_avail)
			break;
	}

	return _debug_dump(GEN, buf, max, cont);
}

static int debug_dump_sym(char *buf, int max, uint32_t cont)
{
	int r;
	while (cont) {
		update_read_avail(&inst[GEN]);
		r = wait_event_interruptible_timeout(inst[GEN].read_wait,
						     inst[GEN].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: readavailable %d\n", __func__,
		    inst[GEN].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[GEN].last_read_avail)
			break;
	}

	return _debug_dump_sym(GEN, buf, max, cont);
}

static int debug_dump_static(char *buf, int max, uint32_t cont)
{
	int r;
	while (cont) {
		update_read_avail(&inst[STA]);
		r = wait_event_interruptible_timeout(inst[STA].read_wait,
						     inst[STA].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: readavailable %d\n", __func__,
		    inst[STA].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[STA].last_read_avail)
			break;
	}

	return _debug_dump(STA, buf, max, cont);
}

static int debug_dump_static_sym(char *buf, int max, uint32_t cont)
{
	int r;
	while (cont) {
		update_read_avail(&inst[STA]);
		r = wait_event_interruptible_timeout(inst[STA].read_wait,
						     inst[STA].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: readavailable %d\n", __func__,
		    inst[STA].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[STA].last_read_avail)
			break;
	}

	return _debug_dump_sym(STA, buf, max, cont);
}

static int debug_dump_power(char *buf, int max, uint32_t cont)
{
	int r;
	while (cont) {
		update_read_avail(&inst[POW]);
		r = wait_event_interruptible_timeout(inst[POW].read_wait,
						     inst[POW].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: readavailable %d\n", __func__,
		    inst[POW].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[POW].last_read_avail)
			break;
	}

	return _debug_dump(POW, buf, max, cont);
}

static int debug_dump_power_sym(char *buf, int max, uint32_t cont)
{
	int r;
	while (cont) {
		update_read_avail(&inst[POW]);
		r = wait_event_interruptible_timeout(inst[POW].read_wait,
						     inst[POW].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: readavailable %d\n", __func__,
		    inst[POW].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[POW].last_read_avail)
			break;
	}

	return _debug_dump_sym(POW, buf, max, cont);
}

static int debug_dump_voters(char *buf, int max, uint32_t cont)
{
	return _debug_dump_voters(buf, max);
}

static char debug_buffer[EVENTS_PRINT_SIZE];

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int r;
	static int bsize;
	int (*fill)(char *, int, uint32_t) = file->private_data;
	if (!(*ppos))
		bsize = fill(debug_buffer, EVENTS_PRINT_SIZE, 0);
	DBG("%s: count %d ppos %d\n", __func__, count, (unsigned int)*ppos);
	r =  simple_read_from_buffer(buf, count, ppos, debug_buffer,
				     bsize);
	return r;
}

static ssize_t debug_read_cont(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos)
{
	int (*fill)(char *, int, uint32_t) = file->private_data;
	char *buffer = kmalloc(count, GFP_KERNEL);
	int bsize;
	if (!buffer)
		return -ENOMEM;
	bsize = fill(buffer, count, 1);
	DBG("%s: count %d bsize %d\n", __func__, count, bsize);
	if (copy_to_user(buf, buffer, bsize)) {
		kfree(buffer);
		return -EFAULT;
	}
	kfree(buffer);
	return bsize;
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static const struct file_operations debug_ops_cont = {
	.read = debug_read_cont,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
			 struct dentry *dent,
			 int (*fill)(char *buf, int max, uint32_t cont),
			 const struct file_operations *fops)
{
	debugfs_create_file(name, mode, dent, fill, fops);
}

static void smem_log_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smem_log", 0);
	if (IS_ERR(dent))
		return;

	debug_create("dump", 0444, dent, debug_dump, &debug_ops);
	debug_create("dump_sym", 0444, dent, debug_dump_sym, &debug_ops);
	debug_create("dump_static", 0444, dent, debug_dump_static, &debug_ops);
	debug_create("dump_static_sym", 0444, dent,
		     debug_dump_static_sym, &debug_ops);
	debug_create("dump_power", 0444, dent, debug_dump_power, &debug_ops);
	debug_create("dump_power_sym", 0444, dent,
		     debug_dump_power_sym, &debug_ops);
	debug_create("dump_voters", 0444, dent,
		     debug_dump_voters, &debug_ops);

	debug_create("dump_cont", 0444, dent, debug_dump, &debug_ops_cont);
	debug_create("dump_sym_cont", 0444, dent,
		     debug_dump_sym, &debug_ops_cont);
	debug_create("dump_static_cont", 0444, dent,
		     debug_dump_static, &debug_ops_cont);
	debug_create("dump_static_sym_cont", 0444, dent,
		     debug_dump_static_sym, &debug_ops_cont);
	debug_create("dump_power_cont", 0444, dent,
		     debug_dump_power, &debug_ops_cont);
	debug_create("dump_power_sym_cont", 0444, dent,
		     debug_dump_power_sym, &debug_ops_cont);

	smem_log_timeout_ms = 500;
	smem_log_debug_mask = 0;
}
#else
static void smem_log_debugfs_init(void) {}
#endif

static int smem_log_initialize(void)
{
	int ret;

	ret = _smem_log_init();
	if (ret < 0) {
		pr_err("%s: init failed %d\n", __func__, ret);
		return ret;
	}

	ret = misc_register(&smem_log_dev);
	if (ret < 0) {
		pr_err("%s: device register failed %d\n", __func__, ret);
		return ret;
	}

	smem_log_enable = 1;
	smem_log_initialized = 1;
	smem_log_debugfs_init();
	return ret;
}

static int modem_notifier(struct notifier_block *this,
			  unsigned long code,
			  void *_cmd)
{
	switch (code) {
	case MODEM_NOTIFIER_SMSM_INIT:
		if (!smem_log_initialized)
			smem_log_initialize();
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call = modem_notifier,
};

static int __init smem_log_init(void)
{
	return modem_register_notifier(&nb);
}


module_init(smem_log_init);

MODULE_DESCRIPTION("smem log");
MODULE_LICENSE("GPL v2");
