/*
 * Copyright (c) 2013-2014 Google Inc. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __LINUX_TRUSTY_SMCALL_H
#define __LINUX_TRUSTY_SMCALL_H

/* To prevent redefined with hvcall.h */
#ifndef SMC_NUM_ENTITIES
#define SMC_NUM_ENTITIES	64
#define SMC_NUM_ARGS		4
#define SMC_NUM_PARAMS		(SMC_NUM_ARGS - 1)

#define SMC_IS_FASTCALL(smc_nr)	((smc_nr) & 0x80000000)
#define SMC_IS_SMC64(smc_nr)	((smc_nr) & 0x40000000)
#define SMC_ENTITY(smc_nr)	(((smc_nr) & 0x3F000000) >> 24)
#define SMC_FUNCTION(smc_nr)	((smc_nr) & 0x0000FFFF)

#define SMC_NR(entity, fn, fastcall, smc64) ((((fastcall)&0x1) << 31) | \
					     (((smc64)&0x1) << 30) | \
					     (((entity)&0x3F) << 24) | \
					     ((fn)&0xFFFF) \
					    )

#define SMC_FASTCALL_NR(entity, fn)	SMC_NR((entity), (fn), 1, 0)
#define SMC_STDCALL_NR(entity, fn)	SMC_NR((entity), (fn), 0, 0)
#define SMC_FASTCALL64_NR(entity, fn)	SMC_NR((entity), (fn), 1, 1)
#define SMC_STDCALL64_NR(entity, fn)	SMC_NR((entity), (fn), 0, 1)
#endif /* end of SMC_NUM_ENTITIES */

/* ARM Architecture calls */
#define	SMC_ENTITY_ARCH			0
/* CPU Service calls */
#define	SMC_ENTITY_CPU			1
/* SIP Service calls */
#define	SMC_ENTITY_SIP			2
/* OEM Service calls */
#define	SMC_ENTITY_OEM			3
/* Standard Service calls */
#define	SMC_ENTITY_STD			4
/* Reserved for future use */
#define	SMC_ENTITY_RESERVED		5
/* Trusted Application calls */
#define	SMC_ENTITY_TRUSTED_APP		48
/* Trusted OS calls */
#define	SMC_ENTITY_TRUSTED_OS		50
/* Used for secure -> nonsecure logging */
#define	SMC_ENTITY_LOGGING		51
/* MTK Trusted OS calls */
#define	SMC_ENTITY_MT_TRUSTED_OS	59
/* Trusted OS calls internal to secure monitor */
#define	SMC_ENTITY_SECURE_MONITOR	60


/* FC = Fast call, SC = Standard call */
#define SMC_SC_RESTART_LAST	SMC_STDCALL_NR(SMC_ENTITY_SECURE_MONITOR, 0)
#define SMC_SC_LOCKED_NOP	SMC_STDCALL_NR(SMC_ENTITY_SECURE_MONITOR, 1)

/**
 * SMC_SC_RESTART_FIQ - Re-enter trusty after it was interrupted by an fiq
 *
 * No arguments, no return value.
 *
 * Re-enter trusty after returning to ns to process an fiq. Must be called iff
 * trusty returns SM_ERR_FIQ_INTERRUPTED.
 *
 * Enable by selecting api version TRUSTY_API_VERSION_RESTART_FIQ (1) or later.
 */
#define SMC_SC_RESTART_FIQ	SMC_STDCALL_NR(SMC_ENTITY_SECURE_MONITOR, 2)

/**
 * SMC_SC_NOP - Enter trusty to run pending work.
 *
 * No arguments.
 *
 * Returns SM_ERR_NOP_INTERRUPTED or SM_ERR_NOP_DONE.
 * If SM_ERR_NOP_INTERRUPTED is returned, the call must be repeated.
 *
 * Enable by selecting api version TRUSTY_API_VERSION_SMP (2) or later.
 */
/* FIXME, for backward compatible.
 * SMC_SC_NOP=SMC_STDCALL_NR(SMC_ENTITY_SECURE_MONITOR, 3) is only for
 * MTEE 1.0 build, there is no SMC(60,3) in Nebula build.
 */
#ifdef CONFIG_MTK_NEBULA_VM_SUPPORT
#define SMC_SC_NOP		SMC_SC_GZ_NOP
#else
#define SMC_SC_NOP		SMC_STDCALL_NR(SMC_ENTITY_SECURE_MONITOR, 3)
#endif
/*
 * Return from secure os to non-secure os with return value in r1
 */
#define SMC_SC_NS_RETURN	SMC_STDCALL_NR(SMC_ENTITY_SECURE_MONITOR, 0)

#define SMC_FC_RESERVED		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 0)
#define SMC_FC_FIQ_EXIT		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 1)
#define SMC_FC_REQUEST_FIQ	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 2)
#define SMC_FC_GET_NEXT_IRQ	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 3)
#define SMC_FC_FIQ_ENTER	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 4)

#define SMC_FC64_SET_FIQ_HANDLER SMC_FASTCALL64_NR(SMC_ENTITY_SECURE_MONITOR, 5)
#define SMC_FC64_GET_FIQ_REGS	SMC_FASTCALL64_NR(SMC_ENTITY_SECURE_MONITOR, 6)

#define SMC_FC_CPU_SUSPEND	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 7)
#define SMC_FC_CPU_RESUME	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 8)

#define SMC_FC_AARCH_SWITCH	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 9)
#define SMC_FC_GET_VERSION_STR	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 10)

/**
 * SMC_FC_API_VERSION - Find and select supported API version.
 *
 * @r1: Version supported by client.
 *
 * Returns version supported by trusty.
 *
 * If multiple versions are supported, the client should start by calling
 * SMC_FC_API_VERSION with the largest version it supports. Trusty will then
 * return a version it supports. If the client does not support the version
 * returned by trusty and the version returned is less than the version
 * requested, repeat the call with the largest supported version less than the
 * last returned version.
 *
 * This call must be made before any calls that are affected by the api version.
 */
#define TRUSTY_API_VERSION_RESTART_FIQ	(1)
#define TRUSTY_API_VERSION_SMP		(2)
#define TRUSTY_API_VERSION_SMP_NOP	(3)
#define TRUSTY_API_VERSION_CURRENT	(3)
#define SMC_FC_API_VERSION	SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 11)

/* TRUSTED_OS entity calls */
#define SMC_SC_VIRTIO_GET_DESCR	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 20)
#define SMC_SC_VIRTIO_START	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 21)
#define SMC_SC_VIRTIO_STOP	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 22)

#define SMC_SC_VDEV_RESET	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 23)
#define SMC_SC_VDEV_KICK_VQ	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 24)
#define SMC_NC_VDEV_KICK_VQ	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 25)

/* LOGGING entity calls */
#define SMC_SC_SHARED_LOG_VERSION	SMC_STDCALL_NR(SMC_ENTITY_LOGGING, 0)
#define SMC_SC_SHARED_LOG_ADD		SMC_STDCALL_NR(SMC_ENTITY_LOGGING, 1)
#define SMC_SC_SHARED_LOG_RM		SMC_STDCALL_NR(SMC_ENTITY_LOGGING, 2)

/* Debugging only */
#ifdef CONFIG_MT_GZ_TRUSTY_DEBUGFS
#define MT_SMC_SC_ADD	SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF00)
#endif
#define MT_SMC_FC_THREADS SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF00)
#define MT_SMC_FC_THREADSTATS SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF01)
#define MT_SMC_FC_THREADLOAD SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF02)
#define MT_SMC_FC_HEAP_DUMP SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF03)
#define MT_SMC_FC_APPS	SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF04)

#ifdef CONFIG_MTK_ENABLE_GENIEZONE
#define MT_SMC_SC_SET_RAMCONSOLE \
	SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF80)
#define MT_SMC_SC_VPU \
	SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF81)
#endif

#ifndef CONFIG_MTK_NEBULA_VM_SUPPORT /* FIXME: gz part, gz & nebula exclusive */
#define SMC_SC_GZ_SHARED_LOG_VERSION   SMC_SC_SHARED_LOG_VERSION
#define SMC_SC_GZ_SHARED_LOG_ADD       SMC_SC_SHARED_LOG_ADD
#define SMC_SC_GZ_SHARED_LOG_RM        SMC_SC_SHARED_LOG_RM
#endif

#endif /* __LINUX_TRUSTY_SMCALL_H */
