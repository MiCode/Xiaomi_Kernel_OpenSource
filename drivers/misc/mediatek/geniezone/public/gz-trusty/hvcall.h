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

#ifndef __HV_CALL_H__
#define __HV_CALL_H__

/* To prevent redefined with smcall.h */
#ifndef SMC_NUM_ENTITIES
#define SMC_NUM_ENTITIES        64
#define SMC_NUM_ARGS            4
#define SMC_NUM_PARAMS          (SMC_NUM_ARGS - 1)

#define SMC_IS_FASTCALL(smc_nr) ((smc_nr) & 0x80000000)
#define SMC_IS_SMC64(smc_nr)    ((smc_nr) & 0x40000000)
#define SMC_ENTITY(smc_nr)      (((smc_nr) & 0x3F000000) >> 24)
#define SMC_FUNCTION(smc_nr)    ((smc_nr) & 0x0000FFFF)

#define SMC_NR(entity, fn, fastcall, smc64) \
	((((fastcall)&0x1) << 31) | \
	 (((smc64)&0x1) << 30) | \
	 (((entity)&0x3F) << 24) | \
	 ((fn)&0xFFFF))

#define SMC_FASTCALL_NR(entity, fn)     SMC_NR((entity), (fn), 1, 0)
#define SMC_STDCALL_NR(entity, fn)      SMC_NR((entity), (fn), 0, 0)
#define SMC_FASTCALL64_NR(entity, fn)   SMC_NR((entity), (fn), 1, 1)
#define SMC_STDCALL64_NR(entity, fn)    SMC_NR((entity), (fn), 0, 1)
#endif /* end of SMC_NUM_ENTITIES */

#define	SMC_ENTITY_VM       52  /* VMM SMC Calls */
#define	SMC_ENTITY_PLAT     53  /* PLATFORM SMC Calls */

/****************************************************/
/********** VMM Specific SMC Calls ******************/
/****************************************************/
#define SMC_FC_VM_INIT_DONE             SMC_FASTCALL_NR(SMC_ENTITY_VM, 100)
#define SMC_FC_CPU_HOTPLUG_ON           SMC_FASTCALL_NR(SMC_ENTITY_VM, 110)
#define SMC_FC_CPU_HOTPLUG_OFF          SMC_FASTCALL_NR(SMC_ENTITY_VM, 111)
#define SMC_FC_KERNEL_SUSPEND_OFF       SMC_FASTCALL_NR(SMC_ENTITY_VM, 112)
#define SMC_FC_KERNEL_SUSPEND_ON        SMC_FASTCALL_NR(SMC_ENTITY_VM, 113)
#define SMC_FC_VM_TEST_ADD              SMC_FASTCALL_NR(SMC_ENTITY_VM, 200)
#define SMC_FC_VM_TEST_MULTIPLY         SMC_FASTCALL_NR(SMC_ENTITY_VM, 201)

#define SMC_SC_VM_SMC_RETURN            SMC_STDCALL_NR(SMC_ENTITY_VM, 100)
#define SMC_SC_VM_SHARED_LOG_VERSION    SMC_STDCALL_NR(SMC_ENTITY_VM, 110)
#define SMC_SC_VM_SHARED_LOG_ADD        SMC_STDCALL_NR(SMC_ENTITY_VM, 111)
#define SMC_SC_VM_SHARED_LOG_RM         SMC_STDCALL_NR(SMC_ENTITY_VM, 112)
#define SMC_SC_VM_NOP                   SMC_STDCALL_NR(SMC_ENTITY_VM, 120)
#define SMC_SC_VM_NOP_LOCKED            SMC_STDCALL_NR(SMC_ENTITY_VM, 121)
#define SMC_SC_VM_RESTART_LAST          SMC_STDCALL_NR(SMC_ENTITY_VM, 122)
#define SMC_SC_VM_VIRTIO_GET_DESCR      SMC_STDCALL_NR(SMC_ENTITY_VM, 130)
#define SMC_SC_VM_VIRTIO_START          SMC_STDCALL_NR(SMC_ENTITY_VM, 131)
#define SMC_SC_VM_VIRTIO_STOP           SMC_STDCALL_NR(SMC_ENTITY_VM, 132)
#define SMC_SC_VM_VDEV_RESET            SMC_STDCALL_NR(SMC_ENTITY_VM, 133)
#define SMC_SC_VM_VDEV_KICK_VQ          SMC_STDCALL_NR(SMC_ENTITY_VM, 134)
#define SMC_NC_VM_VDEV_KICK_VQ          SMC_STDCALL_NR(SMC_ENTITY_VM, 135)
#define SMC_SC_VM_TEST_ADD              SMC_STDCALL_NR(SMC_ENTITY_VM, 200)
#define SMC_SC_VM_TEST_MULTIPLY         SMC_STDCALL_NR(SMC_ENTITY_VM, 201)

/****************************************************/
/********** PLATFORM Specific SMC Calls *************/
/****************************************************/
#define SMC_FC_PLAT_GET_UART_PA         SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 100)
#define SMC_FC_PLAT_REGISTER_IRQ        SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 101)
#define SMC_FC_PLAT_GET_NEXT_IRQ        SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 102)

/* FIXME nebula part, gz & nebula exclusive */
#ifdef CONFIG_MTK_NEBULA_VM_SUPPORT
#define SMC_SC_GZ_SHARED_LOG_VERSION    SMC_STDCALL_NR(SMC_ENTITY_PLAT, 100)
#define SMC_SC_GZ_SHARED_LOG_ADD        SMC_STDCALL_NR(SMC_ENTITY_PLAT, 101)
#define SMC_SC_GZ_SHARED_LOG_RM         SMC_STDCALL_NR(SMC_ENTITY_PLAT, 102)
#endif

#define SMC_SC_GZ_NOP                   SMC_STDCALL_NR(SMC_ENTITY_PLAT, 110)
#define SMC_SC_GZ_NOP_LOCKED            SMC_STDCALL_NR(SMC_ENTITY_PLAT, 111)
#define SMC_SC_GZ_RESTART_LAST          SMC_STDCALL_NR(SMC_ENTITY_PLAT, 112)
#define SMC_SC_PLAT_INIT_SHARE_MEMORY   SMC_STDCALL_NR(SMC_ENTITY_PLAT, 120)
#define SMC_SC_PLAT_MTEE_SERVICE_CMD    SMC_STDCALL_NR(SMC_ENTITY_PLAT, 121)
#define SMC_SC_GZ_VIRTIO_GET_DESCR      SMC_STDCALL_NR(SMC_ENTITY_PLAT, 130)
#define SMC_SC_GZ_VIRTIO_START          SMC_STDCALL_NR(SMC_ENTITY_PLAT, 131)
#define SMC_SC_GZ_VIRTIO_STOP           SMC_STDCALL_NR(SMC_ENTITY_PLAT, 132)
#define SMC_SC_GZ_VDEV_RESET            SMC_STDCALL_NR(SMC_ENTITY_PLAT, 133)
#define SMC_SC_GZ_VDEV_KICK_VQ          SMC_STDCALL_NR(SMC_ENTITY_PLAT, 134)

#endif /* __HV_CALL_H__ */
