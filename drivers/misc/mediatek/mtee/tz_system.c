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


#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>

#include "tz_cross/trustzone.h"
#include "tz_cross/ta_system.h"
#include "trustzone/kree/system.h"
#include "kree_int.h"
#include "sys_ipc.h"
#include "kree/mem.h"

#ifdef CONFIG_TRUSTY
#include "kree/tz_trusty.h"
#include <linux/trusty/trusty_ipc.h>
#endif

#define ARM_SMC_CALLING_CONVENTION

#ifndef CONFIG_TRUSTY
static int KREE_ServPuts(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
static int KREE_ServUSleep(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
static int tz_ree_service(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
static int KREE_ServThread_Create(u32 op,
					u8 uparam[REE_SERVICE_BUFFER_SIZE]);

static const KREE_REE_Service_Func ree_service_funcs[] = {
	0,
	KREE_ServPuts,
	KREE_ServUSleep,
	KREE_ServMutexCreate,
	KREE_ServMutexDestroy,
	KREE_ServMutexLock,
	KREE_ServMutexUnlock,
	KREE_ServMutexTrylock,
	KREE_ServMutexIslock,
	KREE_ServSemaphoreCreate,
	KREE_ServSemaphoreDestroy,
	KREE_ServSemaphoreDown,
	KREE_ServSemaphoreDownTimeout,
	KREE_ServSemaphoreDowntrylock,
	KREE_ServSemaphoreUp,
#if 0
	KREE_ServWaitqCreate,
	KREE_ServWaitqDestroy,
	KREE_ServWaitqWaitevent,
	KREE_ServWaitqWaiteventTimeout,
	KREE_ServWaitqWakeup,
#endif
	KREE_ServRequestIrq,
	KREE_ServEnableIrq,
	KREE_ServEnableClock,
	KREE_ServDisableClock,
	KREE_ServThread_Create,

	KREE_ServSemaphoreDownInterruptible,
#ifdef CONFIG_MTEE_CMA_SECURE_MEMORY
	KREE_ServGetChunkmemPool,
	KREE_ServReleaseChunkmemPool,
#else
	NULL,
	NULL,
#endif
	KREE_ServPMGet,
	KREE_ServPMPut,
};

#define ree_service_funcs_num \
	(ARRAY_SIZE(ree_service_funcs))
#endif

#if defined(ARM_SMC_CALLING_CONVENTION)
#define SMC_UNK 0xffffffff
#ifdef CONFIG_TRUSTY
struct smc_args_s {
	void *param;
	void *reebuf;
	uint32_t handle;
	uint32_t command;
	uint32_t paramTypes;
};

struct mtee_ipc_data {
	u32 smcnr;
	u32 smc_args_l;
	u32 smc_args_h;
	u32 param;
};

#define SMC_MTEE_SERVICE_CALL (0x34000008)
#define MTEE_SERVICE_PORT_NAME "com.mediatek.trusty.mteesrv"
static u32 tz_service_call(struct smc_args_s *smc_arg)
{
	s32 ret;
	u64 param[REE_SERVICE_BUFFER_SIZE / sizeof(u64)];
	tipc_k_handle h;
	struct mtee_ipc_data data;
	ssize_t c;
	int tz_ret;

	smc_arg->reebuf = param;

	ret = tipc_k_connect(&h, MTEE_SERVICE_PORT_NAME);
	if (ret != 0)
		return ret;

	data.smcnr = REE_SERV_NONE;
	data.smc_args_l = (u32)(u64)smc_arg;
	data.smc_args_h = (u32)((u64)smc_arg >> 32);
	data.param = (u32)(u64)param;
	c = tipc_k_write(h, &data, sizeof(data), 0);
	if (c < 0) {
		tipc_k_disconnect(h);
		return c;
	}

	while (1) {
		c = tipc_k_read(h, &data, sizeof(data), 0);
		if (c < 0) {
			tipc_k_disconnect(h);
			return c;
		}

		if (data.smcnr == REE_SERV_NONE) {
			ret = (u32)param[0];
			break;
		}

		if (data.smcnr == REE_SERV_REQUEST_IRQ) {
			tz_ret = KREE_ServRequestIrq(REE_SERV_REQUEST_IRQ,
							(u8 *)param);

			data.smcnr = REE_SERV_REQUEST_IRQ;
			data.param = tz_ret;
			c = tipc_k_write(h, &data, sizeof(data), 0);
			if (c < 0) {
				tipc_k_disconnect(h);
				return c;
			}

			if (tz_ret != TZ_RESULT_SUCCESS) {
				tipc_k_disconnect(h);
				return tz_ret;
			}
		}
	}

	tipc_k_disconnect(h);

	return ret;
}

int KREE_TeeServiceCallNoCheck(KREE_SESSION_HANDLE handle,
					uint32_t command, uint32_t paramTypes,
					union MTEEC_PARAM param[4])
{
	struct smc_args_s smc_arg = {	.param = param,
					.handle = handle,
					.command = command,
					.paramTypes = paramTypes };

	return (int) tz_service_call(&smc_arg);
}
#else /* ~CONFIG_TRUSTY */
#define SMC_MTEE_SERVICE_CALL (0x72000008)
#define SMC_MTEE32_SERVICE_CALL (0x32000008)
static u32 tz_service_call(u32 handle, u32 op, u32 arg1, unsigned long arg2)
{
#ifdef CONFIG_ARM64
	u64 param[REE_SERVICE_BUFFER_SIZE / sizeof(u64)];

	register u64 x0 asm("x0") = SMC_MTEE_SERVICE_CALL;
	register u64 x1 asm("x1") = handle;
	register u64 x2 asm("x2") = op;
	register u64 x3 asm("x3") = arg1;
	register u64 x4 asm("x4") = arg2;
	register u64 x5 asm("x5") = (unsigned long)param;

	asm volatile (
		      __asmeq("%0", "x0")
		      __asmeq("%1", "x1")
		      __asmeq("%2", "x2")
		      __asmeq("%3", "x3")
		      __asmeq("%4", "x0")
		      __asmeq("%5", "x1")
		      __asmeq("%6", "x2")
		      __asmeq("%7", "x3")
		      __asmeq("%8", "x4")
		      __asmeq("%9", "x5")
		      "smc    #0\n" :
		      "=r"(x0), "=r"(x1), "=r"(x2), "=r" (x3) :
		      "r"(x0), "r"(x1), "r"(x2), "r" (x3), "r"(x4), "r"(x5) :
		      "memory");

	while (x1 != 0 && x0 != SMC_UNK) {
		/* Need REE service */
		/* r0 is the command, parameter in param buffer */
		x1 = tz_ree_service(x2, (u8 *) param);

		/* Work complete. Going Back to TZ again */
		x0 = SMC_MTEE_SERVICE_CALL;
		asm volatile (
			      __asmeq("%0", "x0")
			      __asmeq("%1", "x1")
			      __asmeq("%2", "x2")
			      __asmeq("%3", "x3")
			      __asmeq("%4", "x0")
			      __asmeq("%5", "x1")
			      __asmeq("%6", "x2")
			      __asmeq("%7", "x3")
			      "smc    #0\n" :
			      "=r"(x0), "=r"(x1), "=r"(x2), "=r"(x3) :
			      "r"(x0), "r"(x1), "r"(x2), "r"(x3) :
			      "memory");
	}

	return x3;
#else
	u32 param[REE_SERVICE_BUFFER_SIZE / sizeof(u32)];

	register u32 r0 asm("r0") = SMC_MTEE32_SERVICE_CALL;
	register u32 r1 asm("r1") = handle;
	register u32 r2 asm("r2") = op;
	register u32 r3 asm("r3") = arg1;
	register u32 r4 asm("r4") = arg2;
	register u32 r5 asm("r5") = (unsigned long)param;

	asm volatile (".arch_extension sec\n"
		      __asmeq("%0", "r0")
		      __asmeq("%1", "r1")
		      __asmeq("%2", "r2")
		      __asmeq("%3", "r3")
		      __asmeq("%4", "r0")
		      __asmeq("%5", "r1")
		      __asmeq("%6", "r2")
		      __asmeq("%7", "r3")
		      __asmeq("%8", "r4")
		      __asmeq("%9", "r5")
		      "smc    #0\n" :
		      "=r"(r0), "=r"(r1), "=r"(r2), "=r"(r3) :
		      "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5) :
		      "memory");

	while (r1 != 0) {
		/* Need REE service */
		/* r2 is the command, parameter in param buffer */
		r1 = tz_ree_service(r2, (u8 *) param);

		/* Work complete. Going Back to TZ again */
		r0 = SMC_MTEE32_SERVICE_CALL;
		asm volatile (".arch_extension sec\n"
			      __asmeq("%0", "r0")
			      __asmeq("%1", "r1")
			      __asmeq("%2", "r2")
			      __asmeq("%3", "r3")
			      __asmeq("%4", "r0")
			      __asmeq("%5", "r1")
			      __asmeq("%6", "r2")
			      "smc    #0\n" :
			      "=r"(r0), "=r"(r1), "=r"(r2), "=r"(r3) :
			      "r"(r0), "r"(r1), "r"(r2) :
			      "memory");
	}

	return r3;
#endif

}

int KREE_TeeServiceCallNoCheck(KREE_SESSION_HANDLE handle,
					uint32_t command, uint32_t paramTypes,
					union MTEEC_PARAM param[4])
{
	return (int) tz_service_call(handle, command, paramTypes,
						(unsigned long) param);
}
#endif /* CONFIG_TRUSTY */

#else
static u32 tz_service_call(u32 handle, u32 op, u32 arg1, u32 arg2)
{
	/* Reserve buffer for REE service call parameters */
	u32 param[REE_SERVICE_BUFFER_SIZE / sizeof(u32)];

	register u32 r0 asm("r0") = SMC_MTEE_SERVICE_CALL;
	register u32 r1 asm("r1") = handle;
	register u32 r2 asm("r2") = op;
	register u32 r3 asm("r3") = arg1;
	register u32 r4 asm("r4") = arg2;
	register u32 r5 asm("r5") = (u32) param;

	asm volatile (".arch_extension sec\n"
		      __asmeq("%0", "r0")
		      __asmeq("%1", "r1")
		      __asmeq("%2", "r2")
		      __asmeq("%3", "r0")
		      __asmeq("%4", "r1")
		      __asmeq("%5", "r2")
		      __asmeq("%6", "r3")
		      __asmeq("%7", "r4")
		      __asmeq("%8", "r5")
		      "smc    #0\n" :
		      "=r"(r0), "=r"(r1), "=r"(r2) :
		      "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5) :
		      "memory");

	while (r1 != 0) {
		/* Need REE service */
		/* r0 is the command, parameter in param buffer */
		r1 = tz_ree_service(r0, (u8 *) param);

		/* Work complete. Going Back to TZ again */
		r0 = 0xffffffff;
		asm volatile (".arch_extension sec\n"
			      __asmeq("%0", "r0")
			      __asmeq("%1", "r1")
			      __asmeq("%2", "r2")
			      __asmeq("%3", "r0")
			      __asmeq("%4", "r1")
			      __asmeq("%5", "r5")
			      "smc    #0\n" :
			      "=r"(r0), "=r"(r1), "=r"(r2) :
			      "r"(r0), "r"(r1), "r"(r5) :
			      "memory");
	}

	return r0;
}

int KREE_TeeServiceCallNoCheck(KREE_SESSION_HANDLE handle,
					uint32_t command, uint32_t paramTypes,
					union MTEEC_PARAM param[4])
{
	return (int) tz_service_call(handle, command, paramTypes,
						(u32) param);
}

#endif

int KREE_TeeServiceCall(KREE_SESSION_HANDLE handle, uint32_t command,
			      uint32_t paramTypes, union MTEEC_PARAM oparam[4])
{
	int i, do_copy = 0;
	int ret;
	uint32_t tmpTypes;
	union MTEEC_PARAM param[4];

	/* Parameter processing. */
	memset(param, 0, sizeof(param));
	tmpTypes = paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_INPUT:
		case TZPT_VALUE_INOUT:
			param[i] = oparam[i];
			break;
		case TZPT_VALUE_OUTPUT:
			/* reset to zero if output */
			param[i].value.a = 0;
			param[i].value.b = 0;
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
			/* Check if point to kernel low memory */
			param[i] = oparam[i];
			if (param[i].mem.buffer < (void *)PAGE_OFFSET ||
			    param[i].mem.buffer >= high_memory) {
				/* No, we need to copy.... */
				if (param[i].mem.size > TEE_PARAM_MEM_LIMIT) {
					param[i].mem.buffer = 0;
					ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
					goto error;
				}

				param[i].mem.buffer = kmalloc(
						param[i].mem.size, GFP_KERNEL);
				if (!param[i].mem.buffer) {
					ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
					goto error;
				}

				if (type != TZPT_MEM_OUTPUT)
					memcpy(param[i].mem.buffer,
						oparam[i].mem.buffer,
						param[i].mem.size);
			}
			break;

		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
			/* Check if share memory is valid. */
			/* Not done yet. */
			param[i] = oparam[i];
			break;

		default:
			/* Bad format, return. */
			return TZ_RESULT_ERROR_BAD_FORMAT;
		}
	}

	/* Real call. */
	do_copy = 1;
	ret = KREE_TeeServiceCallNoCheck(handle, command, paramTypes, param);

error:
	tmpTypes = paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_INOUT:
		case TZPT_VALUE_OUTPUT:
			oparam[i] = param[i];
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
			/* Check if point to kernel memory */
			if (param[i].mem.buffer &&
			(param[i].mem.buffer != oparam[i].mem.buffer)) {
				if (type != TZPT_MEM_INPUT && do_copy)
					memcpy(oparam[i].mem.buffer,
						param[i].mem.buffer,
						param[i].mem.size);
				kfree(param[i].mem.buffer);
			}
			break;

		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
		case TZPT_VALUE_INPUT:
		default:
			/* Nothing to do */
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(KREE_TeeServiceCall);

#ifndef CONFIG_TRUSTY
static int KREE_ServPuts(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	param[REE_SERVICE_BUFFER_SIZE - 1] = 0;
	pr_info("%s", param);
	return TZ_RESULT_SUCCESS;
}

static int KREE_ServUSleep(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	struct ree_service_usleep *param = (struct ree_service_usleep *)uparam;

	usleep_range(param->ustime-1, param->ustime);
	return TZ_RESULT_SUCCESS;
}

/* TEE Kthread create by REE service, TEE service call body
 */
static int kree_thread_function(void *arg)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;
	struct REE_THREAD_INFO *info = (struct REE_THREAD_INFO *)arg;

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
	param[0].value.a = (uint32_t) info->handle;

	/* free parameter resource */
	kfree(info);

	/* create TEE kthread */
	ret = KREE_TeeServiceCall(
			(KREE_SESSION_HANDLE) MTEE_SESSION_HANDLE_SYSTEM,
			TZCMD_SYS_THREAD_CREATE, paramTypes, param);

	return ret;
}

/* TEE Kthread create by REE service
 */
static int KREE_ServThread_Create(u32 op,
			u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	struct REE_THREAD_INFO *info;

	/* get parameters */
	/* the resource will be freed after the thread stops */
	info = kmalloc(sizeof(struct REE_THREAD_INFO), GFP_KERNEL);
	if (info == NULL)
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;

	memcpy(info, uparam, sizeof(struct REE_THREAD_INFO));

	/* create thread and run ... */
	if (!kthread_run(kree_thread_function, info, info->name))
		return TZ_RESULT_ERROR_GENERIC;

	return TZ_RESULT_SUCCESS;
}


static int tz_ree_service(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	KREE_REE_Service_Func func;

	if (op >= ree_service_funcs_num)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	func = ree_service_funcs[op];
	if (!func)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	return (func) (op, param);
}

#endif /* ~CONFIG_TRUSTY */

int KREE_InitTZ(void)
{
	uint32_t paramTypes;
	union MTEEC_PARAM param[4];
	int ret;

	paramTypes = TZPT_NONE;
	ret = KREE_TeeServiceCall(
			(KREE_SESSION_HANDLE) MTEE_SESSION_HANDLE_SYSTEM,
			TZCMD_SYS_INIT, paramTypes, param);

	return ret;
}


int KREE_CreateSession(const char *ta_uuid, KREE_SESSION_HANDLE *pHandle)
{
	uint32_t paramTypes;
	union MTEEC_PARAM param[4];
	int ret;

	if (!ta_uuid || !pHandle)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	param[0].mem.buffer = (char *)ta_uuid;
	param[0].mem.size = strlen(ta_uuid) + 1;
	paramTypes = TZ_ParamTypes2(TZPT_MEM_INPUT, TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(
			(KREE_SESSION_HANDLE) MTEE_SESSION_HANDLE_SYSTEM,
			TZCMD_SYS_SESSION_CREATE, paramTypes, param);

	if (ret == TZ_RESULT_SUCCESS)
		*pHandle = (KREE_SESSION_HANDLE)param[1].value.a;

	return ret;
}
EXPORT_SYMBOL(KREE_CreateSession);

int KREE_CreateSessionWithTag(const char *ta_uuid,
					KREE_SESSION_HANDLE *pHandle,
					const char *tag)
{
	uint32_t paramTypes;
	union MTEEC_PARAM param[4];
	int ret;

	if (!ta_uuid || !pHandle)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	param[0].mem.buffer = (char *)ta_uuid;
	param[0].mem.size = strlen(ta_uuid) + 1;
	param[1].mem.buffer = (char *)tag;
	if (tag != NULL && strlen(tag) != 0)
		param[1].mem.size = strlen(tag) + 1;
	else
		param[1].mem.size = 0;
	paramTypes = TZ_ParamTypes3(TZPT_MEM_INPUT,
					TZPT_MEM_INPUT,
					TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(
			(KREE_SESSION_HANDLE) MTEE_SESSION_HANDLE_SYSTEM,
			TZCMD_SYS_SESSION_CREATE_WITH_TAG, paramTypes, param);

	if (ret == TZ_RESULT_SUCCESS)
		*pHandle = (KREE_SESSION_HANDLE)param[2].value.a;

	return ret;
}
EXPORT_SYMBOL(KREE_CreateSessionWithTag);

int KREE_CloseSession(KREE_SESSION_HANDLE handle)
{
	uint32_t paramTypes;
	union MTEEC_PARAM param[4];
	int ret;

	param[0].value.a = (uint32_t) handle;
	paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);

	ret = KREE_TeeServiceCall(
			(KREE_SESSION_HANDLE) MTEE_SESSION_HANDLE_SYSTEM,
			TZCMD_SYS_SESSION_CLOSE, paramTypes, param);

	return ret;
}
EXPORT_SYMBOL(KREE_CloseSession);

#include "tz_cross/tz_error_strings.h"

const char *TZ_GetErrorString(int res)
{
	return _TZ_GetErrorString(res);
}
EXPORT_SYMBOL(TZ_GetErrorString);
