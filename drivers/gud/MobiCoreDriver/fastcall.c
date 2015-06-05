/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/moduleparam.h>

#include "public/mc_linux.h"
#include "public/mc_linux_api.h"

#include "mci/mcifc.h"

#include "platform.h"	/* MC_FASTCALL_WORKER_THREAD and more */
#include "debug.h"
#include "clock.h"	/* mc_clock_enable, mc_clock_disable */
#include "fastcall.h"

struct fastcall_work {
#ifdef MC_FASTCALL_WORKER_THREAD
	struct kthread_work work;
#else
	struct work_struct work;
#endif
	void *data;
};

/* generic fast call parameters */
union mc_fc_generic {
	struct {
		uint32_t cmd;
		uint32_t param[3];
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t param[2];
	} as_out;
};

/* fast call init */
union mc_fc_init {
	union mc_fc_generic as_generic;
	struct {
		uint32_t cmd;
		uint32_t base;
		uint32_t nq_info;
		uint32_t mcp_info;
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t rfu[2];
	} as_out;
};

/* fast call info parameters */
union mc_fc_info {
	union mc_fc_generic as_generic;
	struct {
		uint32_t cmd;
		uint32_t ext_info_id;
		uint32_t rfu[2];
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t state;
		uint32_t ext_info;
	} as_out;
};

#ifdef TBASE_CORE_SWITCHER
/* fast call switch Core parameters */
union mc_fc_swich_core {
	union mc_fc_generic as_generic;
	struct {
		uint32_t cmd;
		uint32_t core_id;
		uint32_t rfu[2];
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t state;
		uint32_t ext_info;
	} as_out;
};
#endif

#ifdef MC_FASTCALL_WORKER_THREAD
static struct task_struct *fastcall_thread;
static DEFINE_KTHREAD_WORKER(fastcall_worker);
#endif

/*
 * _smc() - fast call to MobiCore
 *
 * @data: pointer to fast call data
 */
static inline int _smc(union mc_fc_generic *mc_fc_generic)
{
	if (!mc_fc_generic)
		return -EINVAL;

#ifdef MC_SMC_FASTCALL
	return smc_fastcall(mc_fc_generic, sizeof(*mc_fc_generic));
#else /* MC_SMC_FASTCALL */
	{
#ifdef CONFIG_ARM64
		/* SMC expect values in x0-x3 */
		register u64 reg0 __asm__("x0") = mc_fc_generic->as_in.cmd;
		register u64 reg1 __asm__("x1") = mc_fc_generic->as_in.param[0];
		register u64 reg2 __asm__("x2") = mc_fc_generic->as_in.param[1];
		register u64 reg3 __asm__("x3") = mc_fc_generic->as_in.param[2];

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
		register u32 reg0 __asm__("r0") = mc_fc_generic->as_in.cmd;
		register u32 reg1 __asm__("r1") = mc_fc_generic->as_in.param[0];
		register u32 reg2 __asm__("r2") = mc_fc_generic->as_in.param[1];
		register u32 reg3 __asm__("r3") = mc_fc_generic->as_in.param[2];

		__asm__ volatile (
#ifdef MC_ARCH_EXTENSION_SEC
			/* This pseudo op is supported and required from
			 * binutils 2.21 on */
			".arch_extension sec\n"
#endif /* MC_ARCH_EXTENSION_SEC */
			"smc #0\n"
			: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
		);

#ifdef __ARM_VE_A9X4_QEMU__
		/* Qemu does not return to the address following the SMC
		 * instruction so we have to insert several nop instructions to
		 * workaround this Qemu bug. */
		__asm__ volatile (
			"nop\n"
			"nop\n"
			"nop\n"
			"nop"
		);
#endif /* __ARM_VE_A9X4_QEMU__ */
#endif /* !CONFIG_ARM64 */

		/* set response */
		mc_fc_generic->as_out.resp     = reg0;
		mc_fc_generic->as_out.ret      = reg1;
		mc_fc_generic->as_out.param[0] = reg2;
		mc_fc_generic->as_out.param[1] = reg3;
	}
	return 0;
#endif /* !MC_SMC_FASTCALL */
}

#ifdef TBASE_CORE_SWITCHER
static uint32_t active_cpu;

#ifdef MC_FASTCALL_WORKER_THREAD
static void mc_cpu_offline(int cpu)
{
	int i;

	if (active_cpu != cpu) {
		MCDRV_DBG("not active CPU, no action taken\n");
		return;
	}

	/* Chose the first online CPU and switch! */
	for_each_online_cpu(i) {
		if (cpu != i) {
			MCDRV_DBG("CPU %d is dying, switching to %d\n", cpu, i);
			mc_switch_core(i);
			break;
		}

		MCDRV_DBG("Skipping CPU %d\n", cpu);
	}
}

static int mobicore_cpu_callback(struct notifier_block *nfb,
				 unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		dev_info(g_ctx.mcd, "Cpu %u is going to die\n", cpu);
		mc_cpu_offline(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		dev_info(g_ctx.mcd, "Cpu %u is dead\n", cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block mobicore_cpu_notifer = {
	.notifier_call = mobicore_cpu_callback,
};
#endif /* MC_FASTCALL_WORKER_THREAD */

static cpumask_t mc_exec_core_switch(union mc_fc_generic *mc_fc_generic)
{
	cpumask_t cpu;
	uint32_t new_cpu;
	uint32_t cpu_id[] = CPU_IDS;

	new_cpu = mc_fc_generic->as_in.param[0];
	mc_fc_generic->as_in.param[0] = cpu_id[mc_fc_generic->as_in.param[0]];

	if (_smc(mc_fc_generic) != 0 || mc_fc_generic->as_out.ret != 0) {
		MCDRV_DBG("CoreSwap failed %d -> %d (cpu %d still active)\n",
			  raw_smp_processor_id(),
			  mc_fc_generic->as_in.param[0],
			  raw_smp_processor_id());
	} else {
		active_cpu = new_cpu;
		MCDRV_DBG("CoreSwap ok %d -> %d\n",
			  raw_smp_processor_id(), active_cpu);
	}
	cpumask_clear(&cpu);
	cpumask_set_cpu(active_cpu, &cpu);
	return cpu;
}
#else /* TBASE_CORE_SWITCHER */
static inline cpumask_t mc_exec_core_switch(union mc_fc_generic *mc_fc_generic)
{
	return CPU_MASK_CPU0;
}
#endif /* !TBASE_CORE_SWITCHER */

#ifdef MC_FASTCALL_WORKER_THREAD
static void fastcall_work_func(struct kthread_work *work)
#else
static void fastcall_work_func(struct work_struct *work)
#endif
{
	struct fastcall_work *fc_work =
		container_of(work, struct fastcall_work, work);
	union mc_fc_generic *mc_fc_generic = fc_work->data;

	if (!mc_fc_generic)
		return;

	mc_clock_enable();

	if (mc_fc_generic->as_in.cmd == MC_FC_SWAP_CPU) {
#ifdef MC_FASTCALL_WORKER_THREAD
		cpumask_t new_msk = mc_exec_core_switch(mc_fc_generic);

		set_cpus_allowed(fastcall_thread, new_msk);
#else
		mc_exec_core_switch(mc_fc_generic);
#endif
	} else {
		_smc(mc_fc_generic);
	}

	mc_clock_disable();
}

static bool mc_fastcall(void *data)
{
#ifdef MC_FASTCALL_WORKER_THREAD
	struct fastcall_work fc_work = {
		KTHREAD_WORK_INIT(fc_work.work, fastcall_work_func),
		.data = data,
	};

	if (!queue_kthread_work(&fastcall_worker, &fc_work.work))
		return false;

	/* If work is queued or executing, wait for it to finish execution */
	flush_kthread_work(&fc_work.work);
#else
	struct fastcall_work fc_work = {
		.data = data,
	};

	INIT_WORK_ONSTACK(&fc_work.work, fastcall_work_func);

	if (!schedule_work_on(0, &fc_work.work))
		return false;

	flush_work(&fc_work.work);
#endif
	return true;
}

int mc_fastcall_init(void)
{
	int ret = mc_clock_init();

	if (ret)
		return ret;

#ifdef MC_FASTCALL_WORKER_THREAD
	fastcall_thread = kthread_create(kthread_worker_fn, &fastcall_worker,
					 "mc_fastcall");
	if (IS_ERR(fastcall_thread)) {
		ret = PTR_ERR(fastcall_thread);
		fastcall_thread = NULL;
		MCDRV_ERROR("cannot create fastcall wq (%d)", ret);
		return ret;
	}

	/* this thread MUST run on CPU 0 at startup */
	set_cpus_allowed(fastcall_thread, CPU_MASK_CPU0);

	wake_up_process(fastcall_thread);
#ifdef TBASE_CORE_SWITCHER
	ret = register_cpu_notifier(&mobicore_cpu_notifer);
#endif
#endif /* MC_FASTCALL_WORKER_THREAD */
	return ret;
}

void mc_fastcall_exit(void)
{
#ifdef MC_FASTCALL_WORKER_THREAD
	if (!IS_ERR_OR_NULL(fastcall_thread)) {
#ifdef TBASE_CORE_SWITCHER
		unregister_cpu_notifier(&mobicore_cpu_notifer);
#endif
		kthread_stop(fastcall_thread);
		fastcall_thread = NULL;
	}
#endif /* MC_FASTCALL_WORKER_THREAD */
	mc_clock_exit();
}

/*
 * convert fast call return code to linux driver module error code
 */
static int convert_fc_ret(uint32_t ret)
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

int mc_fc_init(uintptr_t base_pa, ptrdiff_t off, size_t q_len, size_t buf_len)
{
#ifdef CONFIG_ARM64
	uint32_t base_high = (uint32_t)(base_pa >> 32);
#else
	uint32_t base_high = 0;
#endif
	union mc_fc_init fc_init;

	/* Call the INIT fastcall to setup MobiCore initialization */
	memset(&fc_init, 0, sizeof(fc_init));
	fc_init.as_in.cmd = MC_FC_INIT;
	/* base address of mci buffer PAGE_SIZE (default is 4KB) aligned */
	fc_init.as_in.base = (uint32_t)base_pa;
	/* notification buffer start/length [16:16] [start, length] */
	fc_init.as_in.nq_info =
	    ((base_high & 0xFFFF) << 16) | (q_len & 0xFFFF);
	/* mcp buffer start/length [16:16] [start, length] */
	fc_init.as_in.mcp_info = (off << 16) | (buf_len & 0xFFFF);
	MCDRV_DBG("cmd=0x%08x, base=0x%08x,nq_info=0x%08x, mcp_info=0x%08x",
		  fc_init.as_in.cmd, fc_init.as_in.base, fc_init.as_in.nq_info,
		  fc_init.as_in.mcp_info);
	mc_fastcall(&fc_init.as_generic);
	MCDRV_DBG("out cmd=0x%08x, ret=0x%08x", fc_init.as_out.resp,
		  fc_init.as_out.ret);
	return convert_fc_ret(fc_init.as_out.ret);
}

int mc_fc_info(uint32_t ext_info_id, uint32_t *state, uint32_t *ext_info)
{
	union mc_fc_info fc_info;
	int ret = 0;

	memset(&fc_info, 0, sizeof(fc_info));
	fc_info.as_in.cmd = MC_FC_INFO;
	fc_info.as_in.ext_info_id = ext_info_id;
	mc_fastcall(&fc_info.as_generic);
	ret = convert_fc_ret(fc_info.as_out.ret);
	if (ret) {
		if (state)
			*state = MC_STATUS_NOT_INITIALIZED;

		if (ext_info)
			*ext_info = 0;

		MCDRV_ERROR("code %d for idx %d", ret, ext_info_id);
	} else {
		if (state)
			*state = fc_info.as_out.state;

		if (ext_info)
			*ext_info = fc_info.as_out.ext_info;
	}

	return ret;
}

int mc_fc_mem_trace(phys_addr_t buffer, uint32_t size)
{
	union mc_fc_generic mc_fc_generic;

	memset(&mc_fc_generic, 0, sizeof(mc_fc_generic));
	mc_fc_generic.as_in.cmd = MC_FC_MEM_TRACE;
	mc_fc_generic.as_in.param[0] = (uint32_t)buffer;
#ifdef CONFIG_ARM64
	mc_fc_generic.as_in.param[1] = (uint32_t)(buffer >> 32);
#endif
	mc_fc_generic.as_in.param[2] = size;
	mc_fastcall(&mc_fc_generic);
	return convert_fc_ret(mc_fc_generic.as_out.ret);
}

int mc_fc_nsiq(void)
{
	union mc_fc_generic fc;
	int ret;

	memset(&fc, 0, sizeof(fc));
	fc.as_in.cmd = MC_SMC_N_SIQ;
	mc_fastcall(&fc);
	ret = convert_fc_ret(fc.as_out.ret);
	if (ret)
		MCDRV_ERROR("failed: %d", ret);

	return ret;
}

int mc_fc_yield(void)
{
	union mc_fc_generic fc;
	int ret;

	memset(&fc, 0, sizeof(fc));
	fc.as_in.cmd = MC_SMC_N_YIELD;
	mc_fastcall(&fc);
	ret = convert_fc_ret(fc.as_out.ret);
	if (ret)
		MCDRV_ERROR("failed: %d", ret);

	return ret;
}

#ifdef TBASE_CORE_SWITCHER
uint32_t mc_active_core(void)
{
	return active_cpu;
}

int mc_switch_core(uint32_t core_num)
{
	int32_t ret = 0;
	union mc_fc_swich_core fc_switch_core;

	if (!cpu_online(core_num))
		return 1;

	MCDRV_DBG_VERBOSE("enter\n");
	memset(&fc_switch_core, 0, sizeof(fc_switch_core));
	fc_switch_core.as_in.cmd = MC_FC_SWAP_CPU;
	if (core_num < COUNT_OF_CPUS)
		fc_switch_core.as_in.core_id = core_num;
	else
		fc_switch_core.as_in.core_id = 0;

	MCDRV_DBG("<- cmd=0x%08x, core_id=0x%08x\n",
		  fc_switch_core.as_in.cmd, fc_switch_core.as_in.core_id);
	MCDRV_DBG("<- core_num=0x%08x, active_cpu=0x%08x\n",
		  core_num, active_cpu);
	mc_fastcall(&fc_switch_core.as_generic);
	ret = convert_fc_ret(fc_switch_core.as_out.ret);
	MCDRV_DBG_VERBOSE("exit with %d/0x%08X\n", ret, ret);
	return ret;
}
#endif
