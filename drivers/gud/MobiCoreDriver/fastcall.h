/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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
#ifndef _MC_FASTCALL_H_
#define _MC_FASTCALL_H_

#include "debug.h"
#include "platform.h"

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

/*
 * MobiCore SMCs
 */
#define MC_SMC_N_YIELD		0x3 /* Yield to switch from NWd to SWd. */
#define MC_SMC_N_SIQ		0x4  /* SIQ to switch from NWd to SWd. */

/*
 * MobiCore fast calls. See MCI documentation
 */
#ifdef MC_AARCH32_FC

#define MC_FC_STD64_BASE            ((uint32_t)0xFF000000)
/**< Initializing FastCall. */
#define MC_FC_INIT			(MC_FC_STD64_BASE+1)
/**< Info FastCall. */
#define MC_FC_INFO			(MC_FC_STD64_BASE+2)
/**< Enable SWd tracing via memory */
#define MC_FC_NWD_TRACE			(MC_FC_STD64_BASE+10)
#ifdef TBASE_CORE_SWITCHER
/**< Core switching fastcall */
#define MC_FC_SWITCH_CORE		(MC_FC_STD64_BASE+54)
#endif

#else

#define MC_FC_INIT			-1
#define MC_FC_INFO			-2
#define MC_FC_NWD_TRACE			-31
#ifdef TBASE_CORE_SWITCHER
#define MC_FC_SWITCH_CORE   0x84000005
#endif
#endif

/*
 * return code for fast calls
 */
#define MC_FC_RET_OK				0
#define MC_FC_RET_ERR_INVALID			1
#define MC_FC_RET_ERR_ALREADY_INITIALIZED	5


/* structure wrappers for specific fastcalls */

/* generic fast call parameters */
union fc_generic {
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
	union fc_generic as_generic;
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
	union fc_generic as_generic;
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
	union fc_generic as_generic;
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
/*
 * _smc() - fast call to MobiCore
 *
 * @data: pointer to fast call data
 */
#ifdef CONFIG_ARM64
static inline long _smc(void *data)
{
	int ret = 0;

	if (data == NULL)
		return -EPERM;

	union fc_generic *fc_generic = data;
	/* SMC expect values in x0-x3 */
	register u64 reg0 __asm__("x0") = fc_generic->as_in.cmd;
	register u64 reg1 __asm__("x1") = fc_generic->as_in.param[0];
	register u64 reg2 __asm__("x2") = fc_generic->as_in.param[1];
	register u64 reg3 __asm__("x3") = fc_generic->as_in.param[2];

	__asm__ volatile (
		"smc #0\n"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	);


	/* set response */
	fc_generic->as_out.resp     = reg0;
	fc_generic->as_out.ret      = reg1;
	fc_generic->as_out.param[0] = reg2;
	fc_generic->as_out.param[1] = reg3;
	return ret;
}

#else
static inline long _smc(void *data)
{
	int ret = 0;

	if (data == NULL)
		return -EPERM;

	#ifdef MC_SMC_FASTCALL
	{
		ret = smc_fastcall(data, sizeof(union fc_generic));
	}
	#else
	{
		union fc_generic *fc_generic = data;
		/* SMC expect values in r0-r3 */
		register u32 reg0 __asm__("r0") = fc_generic->as_in.cmd;
		register u32 reg1 __asm__("r1") = fc_generic->as_in.param[0];
		register u32 reg2 __asm__("r2") = fc_generic->as_in.param[1];
		register u32 reg3 __asm__("r3") = fc_generic->as_in.param[2];

		__asm__ volatile (
#ifdef MC_ARCH_EXTENSION_SEC
			/* This pseudo op is supported and required from
			 * binutils 2.21 on */
			".arch_extension sec\n"
#endif
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
#endif

		/* set response */
		fc_generic->as_out.resp     = reg0;
		fc_generic->as_out.ret      = reg1;
		fc_generic->as_out.param[0] = reg2;
		fc_generic->as_out.param[1] = reg3;
	}
	#endif
	return ret;
}
#endif

/*
 * convert fast call return code to linux driver module error code
 */
static inline int convert_fc_ret(uint32_t sret)
{
	int ret = -EFAULT;

	switch (sret) {
	case MC_FC_RET_OK:
		ret = 0;
		break;
	case MC_FC_RET_ERR_INVALID:
		ret = -EINVAL;
		break;
	case MC_FC_RET_ERR_ALREADY_INITIALIZED:
		ret = -EBUSY;
		break;
	default:
		break;
	}
	return ret;
}

#endif /* _MC_FASTCALL_H_ */
