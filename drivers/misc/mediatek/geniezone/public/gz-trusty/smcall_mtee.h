/*
 * Copyright (c) 2019 MediaTek Inc.
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

#ifndef __SMCALL_MTEE_H__
#define __SMCALL_MTEE_H__

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
/* Trusted OS calls internal to secure monitor */
#define	SMC_ENTITY_SECURE_MONITOR	60
#endif /* SMC_NUM_ENTITIES */

#ifndef SMC_ENTITY_GZ
/* Nebula SMC Calls */
#define	SMC_ENTITY_NEBULA		52
/* GZ SMC Calls */
#define	SMC_ENTITY_GZ			53
#endif /* SMC_ENTITY_GZ */

/* Used for GZ secure -> nonsecure logging */
#define	SMC_ENTITY_GZ_LOGGING		51

/* MTK Trusted OS calls */
#define	SMC_ENTITY_MT_TRUSTED_OS	59

/* Trusted OS calls internal to GZ secure monitor */
#define	SMC_ENTITY_GZ_SECURE_MONITOR	60

/**********************************/
/*** SMC route to GZ Hypervisor ***/
/**********************************/

#define SMC_FC_GZ_RESERVED		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 0)

#define SMC_FC_GZ_FIQ_EXIT		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 1)

#define SMC_FC_GZ_REQUEST_FIQ		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 2)

#define SMC_FC_GZ_GET_NEXT_IRQ		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 3)

#define SMC_FC_GZ_FIQ_ENTER		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 4)

#define SMC_FC64_GZ_SET_FIQ_HANDLER	\
			SMC_FASTCALL64_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 5)

#define SMC_FC64_GZ_GET_FIQ_REGS	\
			SMC_FASTCALL64_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 6)

#define SMC_FC_GZ_CPU_SUSPEND		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 7)

#define SMC_FC_GZ_CPU_RESUME		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 8)

#define SMC_FC_GZ_AARCH_SWITCH		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 9)

#define SMC_FC_GZ_GET_VERSION_STR	\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 10)

#define SMC_FC_GZ_API_VERSION		\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 11)

#define SMC_FC_GZ_GET_CMASK			\
			SMC_FASTCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 12)

#define SMC_SC_GZ_NS_RETURN		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 0)

#define TRUSTY_API_VERSION_RESTART_FIQ	(1)
#define TRUSTY_API_VERSION_SMP		(2)
#define TRUSTY_API_VERSION_SMCNR_TABLE	(3)
#define TRUSTY_API_VERSION_SMP_NOP	(4)
#define TRUSTY_API_VERSION_CURRENT	(3)
#define NEBULA_API_VERSION_CURRENT	(3)

/*************************/
/*** MT Debugging only ***/
/*************************/
#define MT_SMC_SC_GZ_ADD		\
			SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF00)

#define MT_SMC_SC_GZ_MDELAY		\
			SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF01)
#define MT_SMC_SC_GZ_IRQ_LATENCY	\
			SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF02)
#define MT_SMC_SC_GZ_INTERCEPT_MMIO	\
			SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF03)

#define MT_SMC_FC_GZ_THREADS		\
			SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF04)
#define MT_SMC_FC_GZ_THREADSTATS	\
			SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF05)
#define MT_SMC_FC_GZ_THREADLOAD		\
			SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF06)
#define MT_SMC_FC_GZ_HEAP_DUMP		\
			SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF07)
#define MT_SMC_FC_GZ_APPS		\
			SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF08)
#define MT_SMC_FC_GZ_MEM_USAGE		\
			SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF09)
#define MT_SMC_FC_GZ_DEVAPC_VIO		\
			SMC_FASTCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xF0A)

#define MT_SMC_SC_GZ_SET_RAMCONSOLE	\
			SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF80)
#define MT_SMC_SC_GZ_VPU		\
			SMC_STDCALL_NR(SMC_ENTITY_MT_TRUSTED_OS, 0xFF81)

/*********************************************/
/*** Reserve original secure monitor calls ***/
/*********************************************/

#define SMC_SC_TRU_RESTART_LAST		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 0)
#define SMC_SC_TRU_LOCKED_NOP		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 1)
#define SMC_SC_TRU_RESTART_FIQ		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 2)
#define SMC_SC_TRU_NOP			\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 3)

/****************************************/
/*** SMC route to Trusty OS (MTEE1.0) ***/
/****************************************/
/* Log */
#define SMC_SC_GZ_SHARED_LOG_VERSION	\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_LOGGING, 0)
#define SMC_SC_GZ_SHARED_LOG_ADD	\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_LOGGING, 1)
#define SMC_SC_GZ_SHARED_LOG_RM		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_LOGGING, 2)

/* VirtIO*/
#define SMC_SC_GZ_VIRTIO_GET_DESCR	\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 20)
#define SMC_SC_GZ_VIRTIO_START		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 21)
#define SMC_SC_GZ_VIRTIO_STOP		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 22)
#define SMC_SC_GZ_VDEV_RESET		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 23)
#define SMC_SC_GZ_VDEV_KICK_VQ		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 24)
#define SMC_NC_GZ_VDEV_KICK_VQ		\
			SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 25)

/*****************************************/
/*** SMC route to Nebula OS (MTEE 2.0) ***/
/*****************************************/

#define SMC_FC_NBL_TEST_ADD		SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 200)
#define SMC_FC_NBL_TEST_MULTIPLY	SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 201)
#define SMC_SC_NBL_TEST_ADD		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 200)
#define SMC_SC_NBL_TEST_MULTIPLY	SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 201)

#define SMC_SC_NBL_SHARED_LOG_VERSION	SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 110)
#define SMC_SC_NBL_SHARED_LOG_ADD	SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 111)
#define SMC_SC_NBL_SHARED_LOG_RM	SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 112)

#define SMC_SC_NBL_RESTART_LAST		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 122)
#define SMC_SC_NBL_LOCKED_NOP		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 121)
#define SMC_SC_NBL_NOP			SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 120)

#define SMC_SC_NBL_VIRTIO_GET_DESCR	SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 130)
#define SMC_SC_NBL_VIRTIO_START		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 131)
#define SMC_SC_NBL_VIRTIO_STOP		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 132)
#define SMC_SC_NBL_VDEV_RESET		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 133)
#define SMC_SC_NBL_VDEV_KICK_VQ		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 134)
#define SMC_NC_NBL_VDEV_KICK_VQ		SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 135)

/*******************************************/
/*** Legacy SMC for backward-compatible ****/
/*******************************************/

#define SMC_SC_GZ_RESTART_LAST	SMC_STDCALL_NR(SMC_ENTITY_GZ, 112)
#define SMC_SC_GZ_LOCKED_NOP	SMC_STDCALL_NR(SMC_ENTITY_GZ, 111)
#define SMC_SC_GZ_NOP		SMC_STDCALL_NR(SMC_ENTITY_GZ, 110)
/* FIXME: SMC_SC_RESTART_FIQ & SMC_SC_GZ_RESTART_FIQ use the same number */
#define SMC_SC_GZ_RESTART_FIQ	SMC_STDCALL_NR(SMC_ENTITY_GZ_SECURE_MONITOR, 2)

#endif /* __SMCALL_MTEE_H__ */
