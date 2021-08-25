/*
 * Copyright (c) 2018 GoldenRiver Technology Co., Ltd. All rights reserved
 * Copyright (C) 2021 XiaoMi, Inc.
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

#define SMC_NUM_ENTITIES	64
#define SMC_NUM_ARGS		4
#define SMC_NUM_PARAMS		(SMC_NUM_ARGS - 1)

#define SMC_IS_FASTCALL(smc_nr)	((smc_nr) & 0x80000000)
#define SMC_IS_SMC64(smc_nr)	((smc_nr) & 0x40000000)
#define SMC_ENTITY(smc_nr)	(((smc_nr) & 0x3F000000) >> 24)
#define SMC_FUNCTION(smc_nr)	((smc_nr) & 0x0000FFFF)

#define SMC_NR(_entity, _fn, _fastcall, _smc64) \
	((((_fastcall) & 0x1) << 31) | \
	(((_smc64) & 0x1) << 30) | \
	(((_entity) & 0x3F) << 24) | \
	((_fn) & 0xFFFF))

#define SMC_FASTCALL_NR(entity, fn)	SMC_NR((entity), (fn), 1, 0)
#define SMC_STDCALL_NR(entity, fn)	SMC_NR((entity), (fn), 0, 0)
#define SMC_FASTCALL64_NR(entity, fn)	SMC_NR((entity), (fn), 1, 1)
#define SMC_STDCALL64_NR(entity, fn)	SMC_NR((entity), (fn), 0, 1)

#include "hvcall.h"

#define	SMC_ENTITY_ARCH			0		/* ARM Architecture calls */
#define	SMC_ENTITY_CPU			1		/* CPU Service calls */
#define	SMC_ENTITY_SIP			2		/* SIP Service calls */
#define	SMC_ENTITY_OEM			3		/* OEM Service calls */
#define	SMC_ENTITY_STD			4		/* Standard Service calls */
#define	SMC_ENTITY_RESERVED		5		/* Reserved for future use */
#define	SMC_ENTITY_TRUSTED_APP		48		/* Trusted Application calls */
#define	SMC_ENTITY_TRUSTED_OS		SMC_ENTITY_VM	/* Trusted OS calls */
#define	SMC_ENTITY_LOGGING		SMC_ENTITY_VM	/* Used for secure -> nonsecure logging */
#define	SMC_ENTITY_SECURE_MONITOR	SMC_ENTITY_VM	/* Trusted OS calls internal to secure monitor */

/****************************************************/
/********** Secure Monitor SMC Calls ****************/
/****************************************************/
/* Standard call */
#define SMC_SC_RESTART_LAST		SMC_SC_VM_RESTART_LAST
#define SMC_SC_LOCKED_NOP		SMC_SC_VM_NOP_LOCKED
#define SMC_SC_RESTART_FIQ		SMC_STDCALL_NR(SMC_ENTITY_SECURE_MONITOR, 123)
#define SMC_SC_NOP			SMC_SC_VM_NOP
#define SMC_SC_NS_RETURN		SMC_SC_VM_SMC_RETURN
/* Fast call */
#define SMC_FC_RESERVED			SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 0)
#define SMC_FC_FIQ_EXIT			SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 1)
#define SMC_FC_REQUEST_FIQ		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 2)
#define SMC_FC_GET_NEXT_IRQ		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 3)
#define SMC_FC_FIQ_ENTER		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 4)
#define SMC_FC64_SET_FIQ_HANDLER	SMC_FASTCALL64_NR(SMC_ENTITY_SECURE_MONITOR, 5)
#define SMC_FC64_GET_FIQ_REGS		SMC_FASTCALL64_NR(SMC_ENTITY_SECURE_MONITOR, 6)
#define SMC_FC_CPU_SUSPEND		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 7)
#define SMC_FC_CPU_RESUME		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 8)
#define SMC_FC_AARCH_SWITCH		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 9)
#define SMC_FC_GET_VERSION_STR		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 10)
#define SMC_FC_API_VERSION		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 11)
#define SMC_FC_FIQ_RESUME		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 12)
#define SMC_FC_IOREMAP_PA_INFO		SMC_FASTCALL_NR(SMC_ENTITY_SECURE_MONITOR, 13)

/****************************************************/
/********** Trusted OS SMC Calls ********************/
/****************************************************/
/* Standard call */
#define SMC_SC_VIRTIO_GET_DESCR		SMC_SC_VM_VIRTIO_GET_DESCR
#define SMC_SC_VIRTIO_START		SMC_SC_VM_VIRTIO_START
#define SMC_SC_VIRTIO_STOP		SMC_SC_VM_VIRTIO_STOP
#define SMC_SC_VDEV_RESET		SMC_SC_VM_VDEV_RESET
#define SMC_SC_VDEV_KICK_VQ		SMC_SC_VM_VDEV_KICK_VQ
#define SMC_NC_VDEV_KICK_VQ		SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 135)
#define SMC_SC_CREATE_QL_TIPC_DEV       SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 136)
#define SMC_SC_SHUTDOWN_QL_TIPC_DEV	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 137)
#define SMC_SC_HANDLE_QL_TIPC_DEV_CMD	SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 138)

/****************************************************/
/********** Logging SMC Calls ***********************/
/****************************************************/
/* Standard call */
#define SMC_SC_SHARED_LOG_VERSION	SMC_SC_VM_SHARED_LOG_VERSION
#define SMC_SC_SHARED_LOG_ADD		SMC_SC_VM_SHARED_LOG_ADD
#define SMC_SC_SHARED_LOG_RM		SMC_SC_VM_SHARED_LOG_RM

#define TRUSTY_API_VERSION_RESTART_FIQ	(1)
#define TRUSTY_API_VERSION_SMP		(2)
#define TRUSTY_API_VERSION_SMP_NOP      (3)
#define TRUSTY_API_VERSION_CURRENT	(3)

#endif /* __LINUX_TRUSTY_SMCALL_H */
