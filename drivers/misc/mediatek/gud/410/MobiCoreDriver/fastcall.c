/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/sched.h>	/* local_clock */
#include <linux/version.h>

#include "mci/mcifc.h"

#include "platform.h"	/* MC_SMC_FASTCALL */
#include "main.h"
#include "fastcall.h"

/* Use the arch_extension sec pseudo op before switching to secure world */
#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#ifndef CONFIG_ARM64
#define MC_ARCH_EXTENSION_SEC
#endif
#endif

/* Base for all fastcalls, do not use outside of other structs */
union fc_common {
	struct {
		u32 cmd;
		u32 param[3];
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 param[2];
	} out;
};

union fc_init {
	union fc_common common;

	struct {
		u32 cmd;
		u32 base;
		u32 nq_info;
		u32 mcp_info;
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 flags;
		u32 rfu;
	} out;
};

union fc_info {
	union fc_common common;

	struct {
		u32 cmd;
		u32 ext_info_id;
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 state;
		u32 ext_info;
	} out;
};

union fc_trace {
	union fc_common common;

	struct {
		u32 cmd;
		u32 buffer_low;
		u32 buffer_high;
		u32 size;
	} in;

	struct {
		u32 resp;
		u32 ret;
	} out;
};

union fc_nsiq {
	union fc_common common;

	struct {
		u32 cmd;
		u32 debug_ret;
		u32 debug_session_id;
		u32 debug_payload;
	} in;

	struct {
		u32 resp;
		u32 ret;
	} out;
};

union fc_yield {
	union fc_common common;

	struct {
		u32 cmd;
		u32 debug_ret;
		u32 debug_timeslice;
	} in;

	struct {
		u32 resp;
		u32 ret;
	} out;
};

/* Structure to log SMC calls */
struct smc_log_entry {
	u64 cpu_clk;
	int cpu_id;
	union fc_common fc;
};

#define SMC_LOG_SIZE 1024
static struct smc_log_entry smc_log[SMC_LOG_SIZE];
static int smc_log_index;

/*
 * convert fast call return code to linux driver module error code
 */
static int convert_fc_ret(u32 ret)
{
	switch (ret) {
	case MC_FC_RET_OK:
		return 0;
	case MC_FC_RET_ERR_INVALID:
		return -EINVAL;
	case MC_FC_RET_ERR_ALREADY_INITIALIZED:
		return -EBUSY;
	default:
		return -EFAULT;
	}
}

/*
 * __smc() - fast call to MobiCore
 *
 * @data: pointer to fast call data
 */
static inline int __smc(union fc_common *fc, const char *func)
{
	int ret = 0;

	/* Log SMC call */
	smc_log[smc_log_index].cpu_clk = local_clock();
	smc_log[smc_log_index].cpu_id  = raw_smp_processor_id();
	smc_log[smc_log_index].fc = *fc;
	if (++smc_log_index >= SMC_LOG_SIZE)
		smc_log_index = 0;

#ifdef MC_SMC_FASTCALL
	ret = smc_fastcall(fc, sizeof(*fc));
#else /* MC_SMC_FASTCALL */
	{
#ifdef CONFIG_ARM64
		/* SMC expect values in x0-x3 */
		register u64 reg0 __asm__("x0") = fc->in.cmd;
		register u64 reg1 __asm__("x1") = fc->in.param[0];
		register u64 reg2 __asm__("x2") = fc->in.param[1];
		register u64 reg3 __asm__("x3") = fc->in.param[2];

		/*
		 * According to AARCH64 SMC Calling Convention (ARM DEN 0028A),
		 * section 3.1: registers x4-x17 are unpredictable/scratch
		 * registers.  So we have to make sure that the compiler does
		 * not allocate any of those registers by letting him know that
		 * the asm code might clobber them.
		 */
		__asm__ volatile (
			"smc #0\n"
			: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
			:
			: "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
			  "x12", "x13", "x14", "x15", "x16", "x17"
		);
#else /* CONFIG_ARM64 */
		/* SMC expect values in r0-r3 */
		register u32 reg0 __asm__("r0") = fc->in.cmd;
		register u32 reg1 __asm__("r1") = fc->in.param[0];
		register u32 reg2 __asm__("r2") = fc->in.param[1];
		register u32 reg3 __asm__("r3") = fc->in.param[2];

		__asm__ volatile (
#ifdef MC_ARCH_EXTENSION_SEC
			/*
			 * This pseudo op is supported and required from
			 * binutils 2.21 on
			 */
			".arch_extension sec\n"
#endif /* MC_ARCH_EXTENSION_SEC */
			"smc #0\n"
			: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
		);

#endif /* !CONFIG_ARM64 */

		/* set response */
		fc->out.resp     = reg0;
		fc->out.ret      = reg1;
		fc->out.param[0] = reg2;
		fc->out.param[1] = reg3;
	}
#endif /* !MC_SMC_FASTCALL */

	if (ret) {
		mc_dev_err(ret, "failed for %s", func);
	} else {
		ret = convert_fc_ret(fc->out.ret);
		if (ret)
			mc_dev_err(ret, "%s failed (%x)", func, fc->out.ret);
	}

	return ret;
}

#define smc(__fc__) __smc(__fc__.common, __func__)

int fc_init(uintptr_t addr, ptrdiff_t off, size_t q_len, size_t buf_len)
{
	union fc_init fc;
#ifdef CONFIG_ARM64
	u32 addr_high = (u32)(addr >> 32);
#else
	u32 addr_high = 0;
#endif

	/* Call the INIT fastcall to setup MobiCore initialization */
	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_INIT;
	/* base address of mci buffer PAGE_SIZE (default is 4KB) aligned */
	fc.in.base = (u32)addr;
	/* notification buffer start/length [16:16] [start, length] */
	fc.in.nq_info = (u32)(((addr_high & 0xFFFF) << 16) | (q_len & 0xFFFF));
	/* mcp buffer start/length [16:16] [start, length] */
	fc.in.mcp_info = (u32)((off << 16) | (buf_len & 0xFFFF));
	mc_dev_devel("cmd=0x%08x, base=0x%08x, nq_info=0x%08x, mcp_info=0x%08x",
		     fc.in.cmd, fc.in.base, fc.in.nq_info,
		     fc.in.mcp_info);
	return smc(&fc);
}

int fc_info(u32 ext_info_id, u32 *state, u32 *ext_info)
{
	union fc_info fc;
	int ret = 0;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_INFO;
	fc.in.ext_info_id = ext_info_id;
	ret = smc(&fc);
	if (ret) {
		if (state)
			*state = MC_STATUS_NOT_INITIALIZED;

		if (ext_info)
			*ext_info = 0;

		mc_dev_err(ret, "failed for index %d", ext_info_id);
	} else {
		if (state)
			*state = fc.out.state;

		if (ext_info)
			*ext_info = fc.out.ext_info;
	}

	return ret;
}

int fc_trace_init(phys_addr_t buffer, u32 size)
{
	union fc_trace fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_MEM_TRACE;
	fc.in.buffer_low = (u32)buffer;
#ifdef CONFIG_ARM64
	fc.in.buffer_high = (u32)(buffer >> 32);
#endif
	fc.in.size = size;
	return smc(&fc);
}

int fc_trace_deinit(void)
{
	return fc_trace_init(0, 0);
}

/* sid, payload only used for debug purpose */
int fc_nsiq(u32 session_id, u32 payload)
{
	union fc_nsiq fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_SMC_N_SIQ;
	fc.in.debug_session_id = session_id;
	fc.in.debug_payload = payload;
	return smc(&fc);
}

/* timeslice only used for debug purpose */
int fc_yield(u32 timeslice)
{
	union fc_yield fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_SMC_N_YIELD;
	fc.in.debug_timeslice = timeslice;
	return smc(&fc);
}

static int show_smc_log_entry(struct kasnprintf_buf *buf,
			      struct smc_log_entry *entry)
{
	return kasnprintf(buf, "%20llu %10d 0x%08x 0x%08x 0x%08x 0x%08x\n",
			  entry->cpu_clk, entry->cpu_id, entry->fc.in.cmd,
			  entry->fc.in.param[0], entry->fc.in.param[1],
			  entry->fc.in.param[2]);
}

/*
 * Dump SMC log circular buffer, starting from oldest command. It is assumed
 * nothing goes in any more at this point.
 */
int mc_fastcall_debug_smclog(struct kasnprintf_buf *buf)
{
	int i, ret = 0;

	ret = kasnprintf(buf, "%10s %20s %10s %-10s %-10s %-10s\n", "CPU id",
			 "CPU clock", "command", "param1", "param2", "param3");
	if (ret < 0)
		return ret;

	if (smc_log[smc_log_index].cpu_clk)
		/* Buffer has wrapped around, dump end (oldest records) */
		for (i = smc_log_index; i < SMC_LOG_SIZE; i++) {
			ret = show_smc_log_entry(buf, &smc_log[i]);
			if (ret < 0)
				return ret;
		}

	/* Dump first records */
	for (i = 0; i < smc_log_index; i++) {
		ret = show_smc_log_entry(buf, &smc_log[i]);
		if (ret < 0)
			return ret;
	}

	return ret;
}
