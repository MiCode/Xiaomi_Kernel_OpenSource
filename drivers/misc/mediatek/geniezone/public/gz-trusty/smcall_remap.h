/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __LINUX_TRUSTY_SMCALL_REMAP_H
#define __LINUX_TRUSTY_SMCALL_REMAP_H

/****************************************************/
/********** SMC CALLS ENTITY ************************/
/****************************************************/
#define SMC_ENTITY_ARCH             0   /* ARM Architecture calls */
#define SMC_ENTITY_CPU              1   /* CPU Service calls */
#define SMC_ENTITY_SIP              2   /* SIP Service calls */
#define SMC_ENTITY_OEM              3   /* OEM Service calls */
#define SMC_ENTITY_STD              4   /* Standard Service calls */
#define SMC_ENTITY_RESERVED         5   /* Reserved for future use */
#define SMC_ENTITY_TRUSTED_APP      48  /* Trusted Application calls */
#define SMC_ENTITY_TRUSTED_OS       50  /* Trusted OS calls */
#define SMC_ENTITY_LOGGING          51  /* Used for secure logging */
#define SMC_ENTITY_MT_SOS           59  /* MTK Trusted OS calls */
#define SMC_ENTITY_SM               60  /* Trusted OS calls to monitor */

/****************************************************/
/********** UN-CHANGED (SAME AS TRUSTY) *************/
/****************************************************/
#define SMC_SC_NS_RETURN            SMC_STDCALL_NR(SMC_ENTITY_SM, 0)
#define SMC_FC_RESERVED             SMC_FASTCALL_NR(SMC_ENTITY_SM, 0)
#define SMC_FC64_SET_FIQ_HANDLER    SMC_FASTCALL64_NR(SMC_ENTITY_SM, 5)
#define SMC_FC64_GET_FIQ_REGS       SMC_FASTCALL64_NR(SMC_ENTITY_SM, 6)
#define SMC_FC_AARCH_SWITCH         SMC_FASTCALL_NR(SMC_ENTITY_SM, 9)
#define SMC_FC_CPU_ON               SMC_FASTCALL_NR(SMC_ENTITY_SIP, 0)
#define SMC_FC_CPU_DORMANT          SMC_FASTCALL_NR(SMC_ENTITY_SIP, 1)
#define SMC_FC_CPU_DORMANT_CANCEL   SMC_FASTCALL_NR(SMC_ENTITY_SIP, 2)
#define SMC_FC_CPU_OFF              SMC_FASTCALL_NR(SMC_ENTITY_SIP, 3)
#define SMC_FC_CPU_ERRATA_802022    SMC_FASTCALL_NR(SMC_ENTITY_SIP, 4)
#ifdef CONFIG_MT_TRUSTY_DEBUGFS
#define MT_SMC_SC_ADD               SMC_STDCALL_NR(SMC_ENTITY_MT_SOS, 0xFF00)
#endif
#define MT_SMC_SC_MDELAY            SMC_STDCALL_NR(SMC_ENTITY_MT_SOS, 0xFF01)
#define MT_SMC_SC_IRQ_LATENCY       SMC_STDCALL_NR(SMC_ENTITY_MT_SOS, 0xFF02)
#define MT_SMC_SC_INTERCEPT_MMIO    SMC_STDCALL_NR(SMC_ENTITY_MT_SOS, 0xFF03)
#define MT_SMC_FC_THREADS           SMC_FASTCALL_NR(SMC_ENTITY_MT_SOS, 0xFF00)
#define MT_SMC_FC_THREADSTATS       SMC_FASTCALL_NR(SMC_ENTITY_MT_SOS, 0xFF01)
#define MT_SMC_FC_THREADLOAD        SMC_FASTCALL_NR(SMC_ENTITY_MT_SOS, 0xFF02)
#define MT_SMC_FC_HEAP_DUMP         SMC_FASTCALL_NR(SMC_ENTITY_MT_SOS, 0xFF03)
#define MT_SMC_FC_APPS              SMC_FASTCALL_NR(SMC_ENTITY_MT_SOS, 0xFF04)
#define MT_SMC_FC_MEM_USAGE         SMC_FASTCALL_NR(SMC_ENTITY_MT_SOS, 0xFF05)
#define MT_SMC_SC_SET_RAMCONSOLE    SMC_STDCALL_NR(SMC_ENTITY_MT_SOS, 0xFF80)
#define MT_SMC_SC_VPU               SMC_STDCALL_NR(SMC_ENTITY_MT_SOS, 0xFF81)

/****************************************************/
/********** REMAPPED SMC CALLS **********************/
/****************************************************/
#define SMC_SC_RESTART_LAST         SMC_SC_GZ_RESTART_LAST
#define SMC_SC_LOCKED_NOP           SMC_SC_GZ_NOP_LOCKED
#define SMC_SC_RESTART_FIQ          SMC_STDCALL_NR(SMC_ENTITY_SM, 2)
#define SMC_SC_NOP                  SMC_SC_GZ_NOP

#define SMC_SC_VIRTIO_GET_DESCR     SMC_SC_VM_VIRTIO_GET_DESCR
#define SMC_SC_VIRTIO_START         SMC_SC_VM_VIRTIO_START
#define SMC_SC_VIRTIO_STOP          SMC_SC_VM_VIRTIO_STOP
#define SMC_SC_VDEV_RESET           SMC_SC_VM_VDEV_RESET
#define SMC_SC_VDEV_KICK_VQ         SMC_SC_VM_VDEV_KICK_VQ
#define SMC_NC_VDEV_KICK_VQ         SMC_NC_VM_VDEV_KICK_VQ

#define SMC_FC_FIQ_EXIT             SMC_FASTCALL_NR(SMC_ENTITY_SM, 1)
#define SMC_FC_REQUEST_FIQ          SMC_FASTCALL_NR(SMC_ENTITY_SM, 2)
#define SMC_FC_GET_NEXT_IRQ         SMC_FC_PLAT_GET_NEXT_IRQ
#define SMC_FC_FIQ_ENTER            SMC_FASTCALL_NR(SMC_ENTITY_SM, 4)
#define SMC_FC_CPU_SUSPEND          SMC_FASTCALL_NR(SMC_ENTITY_SM, 7)
#define SMC_FC_CPU_RESUME           SMC_FASTCALL_NR(SMC_ENTITY_SM, 8)
#define SMC_FC_GET_VERSION_STR      SMC_FASTCALL_NR(SMC_ENTITY_SM, 10)

#define TRUSTY_API_VERSION_RESTART_FIQ  (1)
#define TRUSTY_API_VERSION_SMP          (2)
#define TRUSTY_API_VERSION_SMP_NOP      (3)
#define TRUSTY_API_VERSION_CURRENT      (2)
#define SMC_FC_API_VERSION          SMC_FASTCALL_NR(SMC_ENTITY_SM, 11)

#define SMC_SC_SHARED_LOG_VERSION   SMC_SC_VM_SHARED_LOG_VERSION
#define SMC_SC_SHARED_LOG_ADD       SMC_SC_VM_SHARED_LOG_ADD
#define SMC_SC_SHARED_LOG_RM        SMC_SC_VM_SHARED_LOG_RM

#endif /* __LINUX_TRUSTY_SMCALL_REMAP_H */
