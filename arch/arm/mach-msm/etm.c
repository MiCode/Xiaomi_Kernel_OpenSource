/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/percpu.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/pm_qos.h>

#include <asm/atomic.h>

#include "cp14.h"

#define LOG_BUF_LEN			32768
/* each slot is 4 bytes, 8kb total */
#define ETB_RAM_SLOTS			2048

#define DATALOG_SYNC			0xB5C7
#define ETM_DUMP_MSG_ID			0x000A6960
#define ETB_DUMP_MSG_ID			0x000A6961

/* ETB Registers */
#define ETB_REG_CONTROL			ETMIMPSPEC1
#define ETB_REG_STATUS			ETMIMPSPEC2
#define ETB_REG_COUNT			ETMIMPSPEC3
#define ETB_REG_ADDRESS			ETMIMPSPEC4
#define ETB_REG_DATA			ETMIMPSPEC5

/* Having etb macro accessors allows macro expansion for ETB reg defines */
#define etb_read(reg)			etm_read(reg)
#define etb_write(val, reg)		etm_write(val, reg)

/* Bitmasks for the ETM control register */
#define ETM_CONTROL_POWERDOWN		0x00000001
#define ETM_CONTROL_PROGRAM		0x00000400

/* Bitmasks for the ETM status register */
#define ETM_STATUS_PROGRAMMING		0x00000002

/* ETB Status Register bit definitions */
#define OV				0x00200000

/* ETB Control Register bit definitions */
#define AIR				0x00000008
#define AIW				0x00000004
#define CPTM				0x00000002
#define CPTEN				0x00000001

/* Bitmasks for the swconfig field of ETM_CONFIG
 * ETM trigger propagated to ETM instances on all cores
 */
#define TRIGGER_ALL			0x00000002

#define PROG_TIMEOUT_MS			500

static int trace_enabled;
static int cpu_to_dump;
static int next_cpu_to_dump;
static struct wake_lock etm_wake_lock;
static struct pm_qos_request etm_qos_req;
static int trace_on_boot;
module_param_named(
	trace_on_boot, trace_on_boot, int, S_IRUGO
);

struct b {
	uint8_t etm_log_buf[LOG_BUF_LEN];
	uint32_t log_end;
};

static struct b buf[NR_CPUS];
static struct b __percpu * *alloc_b;
static atomic_t etm_dev_in_use;

/* These default settings will be used to configure the ETM/ETB
 * when the driver loads. */
struct etm_config_struct {
	uint32_t etm_00_control;
	uint32_t etm_02_trigger_event;
	uint32_t etm_06_te_start_stop;
	uint32_t etm_07_te_single_addr_comp;
	uint32_t etm_08_te_event;
	uint32_t etm_09_te_control;
	uint32_t etm_0a_fifofull_region;
	uint32_t etm_0b_fifofull_level;
	uint32_t etm_0c_vd_event;
	uint32_t etm_0d_vd_single_addr_comp;
	uint32_t etm_0e_vd_mmd;
	uint32_t etm_0f_vd_control;
	uint32_t etm_addr_comp_value[8]; /* 10 to 17 */
	uint32_t etm_addr_access_type[8]; /* 20 to 27 */
	uint32_t etm_data_comp_value[2]; /* 30 and 32 */
	uint32_t etm_data_comp_mask[2]; /* 40 and 42 */
	uint32_t etm_counter_reload_value[2]; /* 50 to 51 */
	uint32_t etm_counter_enable[2]; /* 54 to 55 */
	uint32_t etm_counter_reload_event[2]; /* 58 to 59 */
	uint32_t etm_60_seq_event_1_to_2;
	uint32_t etm_61_seq_event_2_to_1;
	uint32_t etm_62_seq_event_2_to_3;
	uint32_t etm_63_seq_event_3_to_1;
	uint32_t etm_64_seq_event_3_to_2;
	uint32_t etm_65_seq_event_1_to_3;
	uint32_t etm_6c_cid_comp_value_1;
	uint32_t etm_6f_cid_comp_mask;
	uint32_t etm_78_sync_freq;
	uint32_t swconfig;
	uint32_t etb_trig_cnt;
	uint32_t etb_init_ptr;
};

static struct etm_config_struct etm_config = {
	/* etm_00_control 0x0000D84E: 32-bit CID, cycle-accurate,
	 * monitorCPRT */
	.etm_00_control			= 0x0000D84E,
	/* etm_02_trigger_event 0x00000000: address comparator 0 matches */
	.etm_02_trigger_event		= 0x00000000,
	.etm_06_te_start_stop		= 0x00000000,
	.etm_07_te_single_addr_comp	= 0x00000000,
	/* etm_08_te_event 0x0000006F: always true */
	.etm_08_te_event		= 0x0000006F,
	/* etm_09_te_control 0x01000000: exclude none */
	.etm_09_te_control		= 0x01000000,
	.etm_0a_fifofull_region		= 0x00000000,
	.etm_0b_fifofull_level		= 0x00000000,
	/* etm_0c_vd_event 0x0000006F: always true */
	.etm_0c_vd_event                = 0x0000006F,
	.etm_0d_vd_single_addr_comp     = 0x00000000,
	.etm_0e_vd_mmd                  = 0x00000000,
	/* etm_0f_vd_control 0x00010000: exclude none */
	.etm_0f_vd_control              = 0x00010000,
	.etm_addr_comp_value[0]         = 0x00000000,
	.etm_addr_comp_value[1]         = 0x00000000,
	.etm_addr_comp_value[2]         = 0x00000000,
	.etm_addr_comp_value[3]         = 0x00000000,
	.etm_addr_comp_value[4]         = 0x00000000,
	.etm_addr_comp_value[5]         = 0x00000000,
	.etm_addr_comp_value[6]         = 0x00000000,
	.etm_addr_comp_value[7]         = 0x00000000,
	.etm_addr_access_type[0]        = 0x00000000,
	.etm_addr_access_type[1]        = 0x00000000,
	.etm_addr_access_type[2]        = 0x00000000,
	.etm_addr_access_type[3]        = 0x00000000,
	.etm_addr_access_type[4]        = 0x00000000,
	.etm_addr_access_type[5]        = 0x00000000,
	.etm_addr_access_type[6]        = 0x00000000,
	.etm_addr_access_type[7]        = 0x00000000,
	.etm_data_comp_value[0]         = 0x00000000,
	.etm_data_comp_value[1]         = 0x00000000,
	.etm_data_comp_mask[0]          = 0x00000000,
	.etm_data_comp_mask[1]          = 0x00000000,
	.etm_counter_reload_value[0]    = 0x00000000,
	.etm_counter_reload_value[1]    = 0x00000000,
	.etm_counter_enable[0]          = 0x0002406F,
	.etm_counter_enable[1]          = 0x0002406F,
	.etm_counter_reload_event[0]    = 0x0000406F,
	.etm_counter_reload_event[1]    = 0x0000406F,
	.etm_60_seq_event_1_to_2        = 0x0000406F,
	.etm_61_seq_event_2_to_1        = 0x0000406F,
	.etm_62_seq_event_2_to_3        = 0x0000406F,
	.etm_63_seq_event_3_to_1        = 0x0000406F,
	.etm_64_seq_event_3_to_2        = 0x0000406F,
	.etm_65_seq_event_1_to_3        = 0x0000406F,
	.etm_6c_cid_comp_value_1        = 0x00000000,
	.etm_6f_cid_comp_mask           = 0x00000000,
	.etm_78_sync_freq               = 0x00000400,
	.swconfig                       = 0x00000002,
	/* etb_trig_cnt 0x00000000: ignore trigger */
	.etb_trig_cnt                   = 0x00000000,
	/* etb_init_ptr 0x00000010: 16 marker bytes */
	.etb_init_ptr                   = 0x00000010,
};

/* ETM clock is derived from the processor clock and gets enabled on a
 * logical OR of below items on Scorpion:
 * 1.CPMR[ETMCLKEN] is set
 * 2.ETM is not idle. Also means ETMCR[PD] is 0
 * 3.Reset is asserted (core or debug)
 * 4.MRC/MCR to ETM reg (CP14 access)
 * 5.Debugger access to a ETM register in the core power domain
 *
 * 1. and 2. above are permanent enables whereas 3., 4. and 5. are
 * temporary enables
 *
 * We rely on 4. to be able to access ETMCR and then use 2. above for ETM
 * clock vote in the driver and the save-restore code uses 1. above
 * for its vote.
 */
static inline void __cpu_set_etm_pwrdwn(void)
{
	uint32_t etm_control;

	isb();
	etm_control = etm_read(ETMCR);
	etm_control |= ETM_CONTROL_POWERDOWN;
	etm_write(etm_control, ETMCR);
}

static inline void __cpu_clear_etm_pwrdwn(void)
{
	uint32_t etm_control;

	etm_control = etm_read(ETMCR);
	etm_control &= ~ETM_CONTROL_POWERDOWN;
	etm_write(etm_control, ETMCR);
	isb();
}

static void emit_log_char(uint8_t c)
{
	int this_cpu = get_cpu();
	struct b *mybuf = *per_cpu_ptr(alloc_b, this_cpu);
	char *log_buf = mybuf->etm_log_buf;
	int index = (mybuf->log_end)++ & (LOG_BUF_LEN - 1);
	log_buf[index] = c;
	put_cpu();
}

static void emit_log_word(uint32_t word)
{
	emit_log_char(word >> 24);
	emit_log_char(word >> 16);
	emit_log_char(word >> 8);
	emit_log_char(word >> 0);
}

static void __cpu_enable_etb(void)
{
	uint32_t etb_control;
	uint32_t i;

	/* enable auto-increment on reads and writes */
	etb_control = AIR | AIW;
	etb_write(etb_control, ETB_REG_CONTROL);

	/* write tags to the slots before the write pointer so we can
	 * detect overflow */
	etb_write(0x00000000, ETB_REG_ADDRESS);
	for (i = 0; i < (etm_config.etb_init_ptr >> 2); i++)
		etb_write(0xDEADBEEF, ETB_REG_DATA);

	etb_write(0x00000000, ETB_REG_STATUS);

	/* initialize write pointer */
	etb_write(etm_config.etb_init_ptr, ETB_REG_ADDRESS);

	/* multiple of 16 */
	etb_write(etm_config.etb_trig_cnt & 0xFFFFFFF0, ETB_REG_COUNT);

	/* Enable ETB and enable the trigger counter as appropriate. A
	 * trigger count of 0 will be used to signify that the user wants to
	 * ignore the trigger (just keep writing to the ETB and overwriting
	 * the oldest data).  For "trace before trigger" captures the user
	 * should set the trigger count to a small number. */

	etb_control |= CPTEN;
	if (etm_config.etb_trig_cnt)
		etb_control |= CPTM;
	etb_write(etb_control, ETB_REG_CONTROL);
}

static void __cpu_disable_etb(void)
{
	uint32_t etb_control;
	etb_control = etb_read(ETB_REG_CONTROL);
	etb_control &= ~CPTEN;
	etb_write(etb_control, ETB_REG_CONTROL);
}

static void __cpu_enable_etm(void)
{
	uint32_t etm_control;
	unsigned long timeout = jiffies + msecs_to_jiffies(PROG_TIMEOUT_MS);

	etm_control = etm_read(ETMCR);
	etm_control &= ~ETM_CONTROL_PROGRAM;
	etm_write(etm_control, ETMCR);

	while ((etm_read(ETMSR) & ETM_STATUS_PROGRAMMING) == 1) {
		cpu_relax();
		if (time_after(jiffies, timeout)) {
			pr_err("etm: timeout while clearing prog bit\n");
			break;
		}
	}
}

static void __cpu_disable_etm(void)
{
	uint32_t etm_control;
	unsigned long timeout = jiffies + msecs_to_jiffies(PROG_TIMEOUT_MS);

	etm_control = etm_read(ETMCR);
	etm_control |= ETM_CONTROL_PROGRAM;
	etm_write(etm_control, ETMCR);

	while ((etm_read(ETMSR) & ETM_STATUS_PROGRAMMING) == 0) {
		cpu_relax();
		if (time_after(jiffies, timeout)) {
			pr_err("etm: timeout while setting prog bit\n");
			break;
		}
	}
}

static void __cpu_enable_trace(void *unused)
{
	uint32_t etm_control;
	uint32_t etm_trigger;
	uint32_t etm_external_output;

	get_cpu();

	__cpu_disable_etb();
	/* vote for ETM power/clock enable */
	__cpu_clear_etm_pwrdwn();
	__cpu_disable_etm();

	etm_control = (etm_config.etm_00_control & ~ETM_CONTROL_POWERDOWN)
						| ETM_CONTROL_PROGRAM;
	etm_write(etm_control, ETMCR);

	etm_trigger = etm_config.etm_02_trigger_event;
	etm_external_output = 0x406F; /* always FALSE */

	if (etm_config.swconfig & TRIGGER_ALL) {
		uint32_t function = 0x5; /*  A OR B */
		uint32_t resource_b = 0x60; /* external input 1 */

		etm_trigger &= 0x7F; /* keep resource A, clear function and
				      * resource B */
		etm_trigger |= (function << 14);
		etm_trigger |= (resource_b << 7);
		etm_external_output = etm_trigger;
	}

	etm_write(etm_trigger, ETMTRIGGER);
	etm_write(etm_config.etm_06_te_start_stop, ETMTSSCR);
	etm_write(etm_config.etm_07_te_single_addr_comp, ETMTECR2);
	etm_write(etm_config.etm_08_te_event, ETMTEEVR);
	etm_write(etm_config.etm_09_te_control, ETMTECR1);
	etm_write(etm_config.etm_0a_fifofull_region, ETMFFRR);
	etm_write(etm_config.etm_0b_fifofull_level, ETMFFLR);
	etm_write(etm_config.etm_0c_vd_event, ETMVDEVR);
	etm_write(etm_config.etm_0d_vd_single_addr_comp, ETMVDCR1);
	etm_write(etm_config.etm_0e_vd_mmd, ETMVDCR2);
	etm_write(etm_config.etm_0f_vd_control, ETMVDCR3);
	etm_write(etm_config.etm_addr_comp_value[0], ETMACVR0);
	etm_write(etm_config.etm_addr_comp_value[1], ETMACVR1);
	etm_write(etm_config.etm_addr_comp_value[2], ETMACVR2);
	etm_write(etm_config.etm_addr_comp_value[3], ETMACVR3);
	etm_write(etm_config.etm_addr_comp_value[4], ETMACVR4);
	etm_write(etm_config.etm_addr_comp_value[5], ETMACVR5);
	etm_write(etm_config.etm_addr_comp_value[6], ETMACVR6);
	etm_write(etm_config.etm_addr_comp_value[7], ETMACVR7);
	etm_write(etm_config.etm_addr_access_type[0], ETMACTR0);
	etm_write(etm_config.etm_addr_access_type[1], ETMACTR1);
	etm_write(etm_config.etm_addr_access_type[2], ETMACTR2);
	etm_write(etm_config.etm_addr_access_type[3], ETMACTR3);
	etm_write(etm_config.etm_addr_access_type[4], ETMACTR4);
	etm_write(etm_config.etm_addr_access_type[5], ETMACTR5);
	etm_write(etm_config.etm_addr_access_type[6], ETMACTR6);
	etm_write(etm_config.etm_addr_access_type[7], ETMACTR7);
	etm_write(etm_config.etm_data_comp_value[0], ETMDCVR0);
	etm_write(etm_config.etm_data_comp_value[1], ETMDCVR2);
	etm_write(etm_config.etm_data_comp_mask[0], ETMDCMR0);
	etm_write(etm_config.etm_data_comp_mask[1], ETMDCMR2);
	etm_write(etm_config.etm_counter_reload_value[0], ETMCNTRLDVR0);
	etm_write(etm_config.etm_counter_reload_value[1], ETMCNTRLDVR1);
	etm_write(etm_config.etm_counter_enable[0], ETMCNTENR0);
	etm_write(etm_config.etm_counter_enable[1], ETMCNTENR1);
	etm_write(etm_config.etm_counter_reload_event[0], ETMCNTRLDEVR0);
	etm_write(etm_config.etm_counter_reload_event[1], ETMCNTRLDEVR1);
	etm_write(etm_config.etm_60_seq_event_1_to_2, ETMSQ12EVR);
	etm_write(etm_config.etm_61_seq_event_2_to_1, ETMSQ21EVR);
	etm_write(etm_config.etm_62_seq_event_2_to_3, ETMSQ23EVR);
	etm_write(etm_config.etm_63_seq_event_3_to_1, ETMSQ31EVR);
	etm_write(etm_config.etm_64_seq_event_3_to_2, ETMSQ32EVR);
	etm_write(etm_config.etm_65_seq_event_1_to_3, ETMSQ13EVR);
	etm_write(etm_external_output, ETMEXTOUTEVR0);
	etm_write(etm_config.etm_6c_cid_comp_value_1, ETMCIDCVR0);
	etm_write(etm_config.etm_6f_cid_comp_mask, ETMCIDCMR);
	etm_write(etm_config.etm_78_sync_freq, ETMSYNCFR);

	/* Note that we must enable the ETB before we enable the ETM if we
	 * want to capture the "always true" trigger event. */

	__cpu_enable_etb();
	__cpu_enable_etm();

	put_cpu();
}

static void __cpu_disable_trace(void *unused)
{
	get_cpu();

	__cpu_disable_etm();

	/* program trace enable to be low by using always false event */
	etm_write(0x6F | BIT(14), ETMTEEVR);
	/* vote for ETM power/clock disable */
	__cpu_set_etm_pwrdwn();

	__cpu_disable_etb();

	put_cpu();
}

static void enable_trace(void)
{
	wake_lock(&etm_wake_lock);
	pm_qos_update_request(&etm_qos_req, 0);

	if (etm_config.swconfig & TRIGGER_ALL) {
		/* This register is accessible from either core.
		 * CPU1_extout[0] -> CPU0_extin[0]
		 * CPU_extout[0] -> CPU1_extin[0] */
		asm volatile("mcr p15, 3, %0, c15, c5, 2" : : "r" (0x1));
		asm volatile("isb");
	}

	get_cpu();
	__cpu_enable_trace(NULL);
	smp_call_function(__cpu_enable_trace, NULL, 1);
	put_cpu();

	/* 1. causes all online cpus to come out of idle PC
	 * 2. prevents idle PC until save restore flag is enabled atomically
	 *
	 * we rely on the user to prevent hotplug on/off racing with this
	 * operation and to ensure cores where trace is expected to be turned
	 * on are already hotplugged on
	 */
	trace_enabled = 1;

	pm_qos_update_request(&etm_qos_req, PM_QOS_DEFAULT_VALUE);
	wake_unlock(&etm_wake_lock);
}

static void disable_trace(void)
{
	wake_lock(&etm_wake_lock);
	pm_qos_update_request(&etm_qos_req, 0);

	get_cpu();
	__cpu_disable_trace(NULL);
	smp_call_function(__cpu_disable_trace, NULL, 1);
	put_cpu();

	/* 1. causes all online cpus to come out of idle PC
	 * 2. prevents idle PC until save restore flag is disabled atomically
	 *
	 * we rely on the user to prevent hotplug on/off racing with this
	 * operation and to ensure cores where trace is expected to be turned
	 * off are already hotplugged on
	 */
	trace_enabled = 0;

	cpu_to_dump = next_cpu_to_dump = 0;

	pm_qos_update_request(&etm_qos_req, PM_QOS_DEFAULT_VALUE);
	wake_unlock(&etm_wake_lock);
}

static void generate_etb_dump(void)
{
	uint32_t i;
	uint32_t full_slots;
	uint32_t etb_control;
	uint32_t prim_len;
	uint32_t uptime = 0;

	etb_control = etb_read(ETB_REG_CONTROL);
	etb_control |= AIR;
	etb_write(etb_control, ETB_REG_CONTROL);

	if (etb_read(ETB_REG_STATUS) & OV)
		full_slots = ETB_RAM_SLOTS;
	else
		full_slots = etb_read(ETB_REG_ADDRESS) >> 2;

	prim_len = 28 + (full_slots * 4);

	emit_log_char((DATALOG_SYNC >> 8) & 0xFF);
	emit_log_char((DATALOG_SYNC >> 0) & 0xFF);
	emit_log_char((prim_len >> 8) & 0xFF);
	emit_log_char((prim_len >> 0) & 0xFF);
	emit_log_word(uptime);
	emit_log_word(ETB_DUMP_MSG_ID);
	emit_log_word(etm_read(ETMCR));
	emit_log_word(etm_config.etb_init_ptr >> 2);
	emit_log_word(etb_read(ETB_REG_ADDRESS) >> 2);
	emit_log_word((etb_read(ETB_REG_STATUS) & OV) >> 21);

	etb_write(0x00000000, ETB_REG_ADDRESS);
	for (i = 0; i < full_slots; i++)
		emit_log_word(etb_read(ETB_REG_DATA));
}

/* This should match the number of ETM registers being dumped below */
#define ETM_NUM_REGS_TO_DUMP	54
static void generate_etm_dump(void)
{
	uint32_t prim_len;
	uint32_t uptime = 0;

	prim_len = 12 + (4 * ETM_NUM_REGS_TO_DUMP);

	emit_log_char((DATALOG_SYNC >> 8) & 0xFF);
	emit_log_char((DATALOG_SYNC >> 0) & 0xFF);
	emit_log_char((prim_len >> 8) & 0xFF);
	emit_log_char((prim_len >> 0) & 0xFF);
	emit_log_word(uptime);
	emit_log_word(ETM_DUMP_MSG_ID);

	emit_log_word(etm_read(ETMCR));
	emit_log_word(etm_read(ETMSR));
	emit_log_word(etb_read(ETB_REG_CONTROL));
	emit_log_word(etb_read(ETB_REG_STATUS));
	emit_log_word(etb_read(ETB_REG_COUNT));
	emit_log_word(etb_read(ETB_REG_ADDRESS));
	emit_log_word(0); /* don't read ETB_REG_DATA, changes ETB_REG_ADDRESS */
	emit_log_word(etm_read(ETMTRIGGER));
	emit_log_word(etm_read(ETMTSSCR));
	emit_log_word(etm_read(ETMTECR2));
	emit_log_word(etm_read(ETMTEEVR));
	emit_log_word(etm_read(ETMTECR1));
	emit_log_word(etm_read(ETMFFRR));
	emit_log_word(etm_read(ETMFFLR));
	emit_log_word(etm_read(ETMVDEVR));
	emit_log_word(etm_read(ETMVDCR1));
	emit_log_word(etm_read(ETMVDCR2));
	emit_log_word(etm_read(ETMVDCR3));
	emit_log_word(etm_read(ETMACVR0));
	emit_log_word(etm_read(ETMACVR1));
	emit_log_word(etm_read(ETMACVR2));
	emit_log_word(etm_read(ETMACVR3));
	emit_log_word(etm_read(ETMACVR4));
	emit_log_word(etm_read(ETMACVR5));
	emit_log_word(etm_read(ETMACVR6));
	emit_log_word(etm_read(ETMACVR7));
	emit_log_word(etm_read(ETMACTR0));
	emit_log_word(etm_read(ETMACTR1));
	emit_log_word(etm_read(ETMACTR2));
	emit_log_word(etm_read(ETMACTR3));
	emit_log_word(etm_read(ETMACTR4));
	emit_log_word(etm_read(ETMACTR5));
	emit_log_word(etm_read(ETMACTR6));
	emit_log_word(etm_read(ETMACTR7));
	emit_log_word(etm_read(ETMDCVR0));
	emit_log_word(etm_read(ETMDCVR2));
	emit_log_word(etm_read(ETMDCMR0));
	emit_log_word(etm_read(ETMDCMR2));
	emit_log_word(etm_read(ETMCNTRLDVR0));
	emit_log_word(etm_read(ETMCNTRLDVR1));
	emit_log_word(etm_read(ETMCNTENR0));
	emit_log_word(etm_read(ETMCNTENR1));
	emit_log_word(etm_read(ETMCNTRLDEVR0));
	emit_log_word(etm_read(ETMCNTRLDEVR1));
	emit_log_word(etm_read(ETMSQ12EVR));
	emit_log_word(etm_read(ETMSQ21EVR));
	emit_log_word(etm_read(ETMSQ23EVR));
	emit_log_word(etm_read(ETMSQ31EVR));
	emit_log_word(etm_read(ETMSQ32EVR));
	emit_log_word(etm_read(ETMSQ13EVR));
	emit_log_word(etm_read(ETMEXTOUTEVR0));
	emit_log_word(etm_read(ETMCIDCVR0));
	emit_log_word(etm_read(ETMCIDCMR));
	emit_log_word(etm_read(ETMSYNCFR));
}

static void dump_all(void *unused)
{
	get_cpu();
	__cpu_disable_etb();
	generate_etm_dump();
	generate_etb_dump();
	if (trace_enabled)
		__cpu_enable_etb();
	put_cpu();
}

static void dump_trace(void)
{
	get_cpu();
	dump_all(NULL);
	smp_call_function(dump_all, NULL, 1);
	put_cpu();
}

static int bytes_to_dump;
static uint8_t *etm_buf_ptr;

static int etm_dev_open(struct inode *inode, struct file *file)
{
	if (atomic_cmpxchg(&etm_dev_in_use, 0, 1))
		return -EBUSY;

	pr_debug("%s: successfully opened\n", __func__);
	return 0;
}

static ssize_t etm_dev_read(struct file *file, char __user *data,
				size_t len, loff_t *ppos)
{
	if (cpu_to_dump == next_cpu_to_dump) {
		if (cpu_to_dump == 0)
			dump_trace();
		bytes_to_dump = buf[cpu_to_dump].log_end;
		buf[cpu_to_dump].log_end = 0;
		etm_buf_ptr = buf[cpu_to_dump].etm_log_buf;
		next_cpu_to_dump++;
		if (next_cpu_to_dump >= num_possible_cpus())
			next_cpu_to_dump = 0;
	}

	if (len > bytes_to_dump)
		len = bytes_to_dump;

	if (copy_to_user(data, etm_buf_ptr, len)) {
		pr_debug("%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	bytes_to_dump -= len;
	etm_buf_ptr += len;

	pr_debug("%s: %d bytes copied, %d bytes left (cpu %d)\n",
		 __func__, len, bytes_to_dump, next_cpu_to_dump);
	return len;
}

static void setup_range_filter(char addr_type, char range, uint32_t reg1,
				uint32_t addr1, uint32_t reg2, uint32_t addr2)
{
	etm_config.etm_addr_comp_value[reg1] = addr1;
	etm_config.etm_addr_comp_value[reg2] = addr2;

	etm_config.etm_07_te_single_addr_comp |= (1 << reg1);
	etm_config.etm_07_te_single_addr_comp |= (1 << reg2);

	etm_config.etm_09_te_control |= (1 << (reg1/2));
	if (range == 'i')
		etm_config.etm_09_te_control &= ~(1 << 24);
	else if (range == 'e')
		etm_config.etm_09_te_control |= (1 << 24);

	if (addr_type == 'i') {
		etm_config.etm_addr_access_type[reg1] = 0x99;
		etm_config.etm_addr_access_type[reg2] = 0x99;
	} else if (addr_type == 'd') {
		etm_config.etm_addr_access_type[reg1] = 0x9C;
		etm_config.etm_addr_access_type[reg2] = 0x9C;
	}
}

static void setup_start_stop_filter(char addr_type, char start_stop,
				uint32_t reg, uint32_t addr)
{
	etm_config.etm_addr_comp_value[reg] = addr;

	if (start_stop == 's')
		etm_config.etm_06_te_start_stop |= (1 << reg);
	else if (start_stop == 't')
		etm_config.etm_06_te_start_stop |= (1 << (reg + 16));

	etm_config.etm_09_te_control |= (1 << 25);

	if (addr_type == 'i')
		etm_config.etm_addr_access_type[reg] = 0x99;
	else if (addr_type == 'd')
		etm_config.etm_addr_access_type[reg] = 0x9C;
}

static void setup_viewdata_range_filter(char range, uint32_t reg1,
				uint32_t addr1, uint32_t reg2, uint32_t addr2)
{
	etm_config.etm_addr_comp_value[reg1] = addr1;
	etm_config.etm_addr_comp_value[reg2] = addr2;

	if (range == 'i') {
		etm_config.etm_0d_vd_single_addr_comp |= (1 << reg1);
		etm_config.etm_0d_vd_single_addr_comp |= (1 << reg2);
		etm_config.etm_0f_vd_control |= (1 << (reg1/2));
	} else if (range == 'e') {
		etm_config.etm_0d_vd_single_addr_comp |= (1 << (reg1 + 16));
		etm_config.etm_0d_vd_single_addr_comp |= (1 << (reg2 + 16));
		etm_config.etm_0f_vd_control |= (1 << ((reg1/2) + 8));
	}
	etm_config.etm_0f_vd_control &= ~(1 << 16);

	etm_config.etm_addr_access_type[reg1] = 0x9C;
	etm_config.etm_addr_access_type[reg2] = 0x9C;
}

static void setup_viewdata_start_stop_filter(char start_stop, uint32_t reg,
				uint32_t addr)
{
	etm_config.etm_addr_comp_value[reg] = addr;

	if (start_stop == 's')
		etm_config.etm_06_te_start_stop |= (1 << reg);
	else if (start_stop == 't')
		etm_config.etm_06_te_start_stop |= (1 << (reg + 16));

	etm_config.etm_addr_access_type[reg] = 0x9C;
}

static void setup_access_type(uint32_t reg, uint32_t value)
{
	etm_config.etm_addr_access_type[reg] &= 0xFFFFFFF8;
	value &= 0x7;
	etm_config.etm_addr_access_type[reg] |= value;
}

static void reset_filter(void)
{
	etm_config.etm_00_control			= 0x0000D84E;
	/* etm_02_trigger_event 0x00000000: address comparator 0 matches */
	etm_config.etm_02_trigger_event		= 0x00000000;
	etm_config.etm_06_te_start_stop		= 0x00000000;
	etm_config.etm_07_te_single_addr_comp	= 0x00000000;
	/* etm_08_te_event 0x0000006F: always true */
	etm_config.etm_08_te_event		= 0x0000006F;
	/* etm_09_te_control 0x01000000: exclude none */
	etm_config.etm_09_te_control		= 0x01000000;
	etm_config.etm_0a_fifofull_region		= 0x00000000;
	etm_config.etm_0b_fifofull_level		= 0x00000000;
	/* etm_0c_vd_event 0x0000006F: always true */
	etm_config.etm_0c_vd_event                = 0x0000006F;
	etm_config.etm_0d_vd_single_addr_comp     = 0x00000000;
	etm_config.etm_0e_vd_mmd                  = 0x00000000;
	/* etm_0f_vd_control 0x00010000: exclude none */
	etm_config.etm_0f_vd_control              = 0x00010000;
	etm_config.etm_addr_comp_value[0]         = 0x00000000;
	etm_config.etm_addr_comp_value[1]         = 0x00000000;
	etm_config.etm_addr_comp_value[2]         = 0x00000000;
	etm_config.etm_addr_comp_value[3]         = 0x00000000;
	etm_config.etm_addr_comp_value[4]         = 0x00000000;
	etm_config.etm_addr_comp_value[5]         = 0x00000000;
	etm_config.etm_addr_comp_value[6]         = 0x00000000;
	etm_config.etm_addr_comp_value[7]         = 0x00000000;
	etm_config.etm_addr_access_type[0]        = 0x00000000;
	etm_config.etm_addr_access_type[1]        = 0x00000000;
	etm_config.etm_addr_access_type[2]        = 0x00000000;
	etm_config.etm_addr_access_type[3]        = 0x00000000;
	etm_config.etm_addr_access_type[4]        = 0x00000000;
	etm_config.etm_addr_access_type[5]        = 0x00000000;
	etm_config.etm_addr_access_type[6]        = 0x00000000;
	etm_config.etm_addr_access_type[7]        = 0x00000000;
	etm_config.etm_data_comp_value[0]         = 0x00000000;
	etm_config.etm_data_comp_value[1]         = 0x00000000;
	etm_config.etm_data_comp_mask[0]          = 0x00000000;
	etm_config.etm_data_comp_mask[1]          = 0x00000000;
	etm_config.etm_counter_reload_value[0]    = 0x00000000;
	etm_config.etm_counter_reload_value[1]    = 0x00000000;
	etm_config.etm_counter_enable[0]          = 0x0002406F;
	etm_config.etm_counter_enable[1]          = 0x0002406F;
	etm_config.etm_counter_reload_event[0]    = 0x0000406F;
	etm_config.etm_counter_reload_event[1]    = 0x0000406F;
	etm_config.etm_60_seq_event_1_to_2        = 0x0000406F;
	etm_config.etm_61_seq_event_2_to_1        = 0x0000406F;
	etm_config.etm_62_seq_event_2_to_3        = 0x0000406F;
	etm_config.etm_63_seq_event_3_to_1        = 0x0000406F;
	etm_config.etm_64_seq_event_3_to_2        = 0x0000406F;
	etm_config.etm_65_seq_event_1_to_3        = 0x0000406F;
	etm_config.etm_6c_cid_comp_value_1        = 0x00000000;
	etm_config.etm_6f_cid_comp_mask           = 0x00000000;
	etm_config.etm_78_sync_freq               = 0x00000400;
	etm_config.swconfig                       = 0x00000002;
	/* etb_trig_cnt 0x00000020: ignore trigger */
	etm_config.etb_trig_cnt                   = 0x00000000;
	/* etb_init_ptr 0x00000010: 16 marker bytes */
	etm_config.etb_init_ptr                   = 0x00000010;
}

#define MAX_COMMAND_STRLEN  40
static ssize_t etm_dev_write(struct file *file, const char __user *data,
				size_t len, loff_t *ppos)
{
	char command[MAX_COMMAND_STRLEN];
	int strlen;
	unsigned long value;
	unsigned long reg1, reg2;
	unsigned long addr1, addr2;

	strlen = strnlen_user(data, MAX_COMMAND_STRLEN);
	pr_debug("etm: string length: %d", strlen);
	if (strlen == 0 || strlen == (MAX_COMMAND_STRLEN+1)) {
		pr_err("etm: error in strlen: %d", strlen);
		return -EFAULT;
	}
	/* includes the null character */
	if (copy_from_user(command, data, strlen)) {
		pr_err("etm: error in copy_from_user: %d", strlen);
		return -EFAULT;
	}

	pr_debug("etm: input = %s", command);

	switch (command[0]) {
	case '0':
		if (trace_enabled) {
			disable_trace();
			pr_info("etm: tracing disabled\n");
		}
		break;
	case '1':
		if (!trace_enabled) {
			enable_trace();
			pr_info("etm: tracing enabled\n");
		}
		break;
	case 'f':
		switch (command[2]) {
		case 'i':
		case 'd':
			switch (command[4]) {
			case 'i':
				if (sscanf(&command[6], "%lx:%lx:%lx:%lx\\0",
					&reg1, &addr1, &reg2, &addr2) != 4)
					goto err_out;
				if (reg1 > 7 || reg2 > 7 || (reg1 % 2))
					goto err_out;
				setup_range_filter(command[2], 'i',
					reg1, addr1, reg2, addr2);
				break;
			case 'e':
				if (sscanf(&command[6], "%lx:%lx:%lx:%lx\\0",
					&reg1, &addr1, &reg2, &addr2) != 4)
					goto err_out;
				if (reg1 > 7 || reg2 > 7 || (reg1 % 2)
					|| command[2] == 'd')
					goto err_out;
				setup_range_filter(command[2], 'e',
					reg1, addr1, reg2, addr2);
				break;
			case 's':
				if (sscanf(&command[6], "%lx:%lx\\0",
					&reg1, &addr1) != 2)
					goto err_out;
				if (reg1 > 7)
					goto err_out;
				setup_start_stop_filter(command[2], 's',
					reg1, addr1);
				break;
			case 't':
				if (sscanf(&command[6], "%lx:%lx\\0",
						&reg1, &addr1) != 2)
					goto err_out;
				if (reg1 > 7)
					goto err_out;
				setup_start_stop_filter(command[2], 't',
					reg1, addr1);
				break;
			default:
				goto err_out;
			}
			break;
		case 'r':
			reset_filter();
			break;
		default:
			goto err_out;
		}
		break;
	case 'v':
		switch (command[2]) {
		case 'd':
			switch (command[4]) {
			case 'i':
				if (sscanf(&command[6], "%lx:%lx:%lx:%lx\\0",
					&reg1, &addr1, &reg2, &addr2) != 4)
					goto err_out;
				if (reg1 > 7 || reg2 > 7 || (reg1 % 2))
					goto err_out;
				setup_viewdata_range_filter('i',
					reg1, addr1, reg2, addr2);
				break;
			case 'e':
				if (sscanf(&command[6], "%lx:%lx:%lx:%lx\\0",
					&reg1, &addr1, &reg2, &addr2) != 4)
					goto err_out;
				if (reg1 > 7 || reg2 > 7 || (reg1 % 2))
					goto err_out;
				setup_viewdata_range_filter('e',
					reg1, addr1, reg2, addr2);
				break;
			case 's':
				if (sscanf(&command[6], "%lx:%lx\\0",
					&reg1, &addr1) != 2)
					goto err_out;
				if (reg1 > 7)
					goto err_out;
				setup_viewdata_start_stop_filter('s',
					reg1, addr1);
				break;
			case 't':
				if (sscanf(&command[6], "%lx:%lx\\0",
					&reg1, &addr1) != 2)
					goto err_out;
				if (reg1 > 7)
					goto err_out;
				setup_viewdata_start_stop_filter('t',
					reg1, addr1);
				break;
			default:
				goto err_out;
			}
			break;
		default:
			goto err_out;
		}
		break;
	case 'a':
		switch (command[2]) {
		case 't':
			if (sscanf(&command[4], "%lx:%lx\\0",
					&reg1, &value) != 2)
				goto err_out;
			if (reg1 > 7 || value > 6)
				goto err_out;
			setup_access_type(reg1, value);
			break;
		default:
			goto err_out;
		}
		break;
	default:
		goto err_out;
	}

	return len;

err_out:
	return -EFAULT;
}

static int etm_dev_release(struct inode *inode, struct file *file)
{
	if (cpu_to_dump == next_cpu_to_dump)
		next_cpu_to_dump = 0;
	cpu_to_dump = next_cpu_to_dump;

	atomic_set(&etm_dev_in_use, 0);
	pr_debug("%s: released\n", __func__);
	return 0;
}

static const struct file_operations etm_dev_fops = {
	.owner = THIS_MODULE,
	.open = etm_dev_open,
	.read = etm_dev_read,
	.write = etm_dev_write,
	.release = etm_dev_release,
};

static struct miscdevice etm_dev = {
	.name = "msm_etm",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &etm_dev_fops,
};

static void __cpu_clear_sticky(void *unused)
{
	etm_read(ETMPDSR); /* clear sticky bit in PDSR */
	isb();
}

static int __init etm_init(void)
{
	int ret, cpu;

	ret = misc_register(&etm_dev);
	if (ret)
		return -ENODEV;

	alloc_b = alloc_percpu(typeof(*alloc_b));
	if (!alloc_b)
		goto err1;

	for_each_possible_cpu(cpu)
		*per_cpu_ptr(alloc_b, cpu) = &buf[cpu];

	wake_lock_init(&etm_wake_lock, WAKE_LOCK_SUSPEND, "msm_etm");
	pm_qos_add_request(&etm_qos_req, PM_QOS_CPU_DMA_LATENCY,
						PM_QOS_DEFAULT_VALUE);

	/* No need to explicity turn on ETM clock since CP14 access go
	 * through via the autoclock turn on/off
	 */
	__cpu_clear_sticky(NULL);
	smp_call_function(__cpu_clear_sticky, NULL, 1);

	cpu_to_dump = next_cpu_to_dump = 0;

	pr_info("ETM/ETB intialized.\n");

	if (trace_on_boot)
		enable_trace();

	return 0;

err1:
	misc_deregister(&etm_dev);
	return -ENOMEM;
}
module_init(etm_init);

static void __exit etm_exit(void)
{
	disable_trace();
	pm_qos_remove_request(&etm_qos_req);
	wake_lock_destroy(&etm_wake_lock);
	free_percpu(alloc_b);
	misc_deregister(&etm_dev);
}
module_exit(etm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("embedded trace driver");
