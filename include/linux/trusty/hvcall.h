/*
 * Copyright (c) 2018 GoldenRiver Technology Co., Ltd. All rights reserved
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

#define SMC_NR(_entity, _fn, _fastcall, _smc64) \
    ((((_fastcall) & 0x1) << 31) | \
     (((_smc64) & 0x1) << 30) | \
     (((_entity) & 0x3F) << 24) | \
     ((_fn) & 0xFFFF))

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
#define SMC_FC_VM_INIT_DONE                 SMC_FASTCALL_NR(SMC_ENTITY_VM, 100)
#define SMC_FC_VM_STDCALL_SWITCH            SMC_FASTCALL_NR(SMC_ENTITY_VM, 101)
#define SMC_FC_CPU_HOTPLUG_ON               SMC_FASTCALL_NR(SMC_ENTITY_VM, 110)
#define SMC_FC_CPU_HOTPLUG_OFF              SMC_FASTCALL_NR(SMC_ENTITY_VM, 111)
#define SMC_FC_KERNEL_SUSPEND_OFF           SMC_FASTCALL_NR(SMC_ENTITY_VM, 112)
#define SMC_FC_KERNEL_SUSPEND_ON            SMC_FASTCALL_NR(SMC_ENTITY_VM, 113)
#define SMC_FC_VM_TEST_ADD                  SMC_FASTCALL_NR(SMC_ENTITY_VM, 200)
#define SMC_FC_VM_TEST_MULTIPLY             SMC_FASTCALL_NR(SMC_ENTITY_VM, 201)

#define SMC_SC_VM_SMC_RETURN                SMC_STDCALL_NR(SMC_ENTITY_VM, 100)
#define SMC_SC_VM_STDCALL_DONE              SMC_STDCALL_NR(SMC_ENTITY_VM, 101)
#define SMC_SC_VM_SHARED_LOG_VERSION        SMC_STDCALL_NR(SMC_ENTITY_VM, 110)
#define SMC_SC_VM_SHARED_LOG_ADD            SMC_STDCALL_NR(SMC_ENTITY_VM, 111)
#define SMC_SC_VM_SHARED_LOG_RM             SMC_STDCALL_NR(SMC_ENTITY_VM, 112)
#define SMC_SC_VM_NOP                       SMC_STDCALL_NR(SMC_ENTITY_VM, 120)
#define SMC_SC_VM_NOP_LOCKED                SMC_STDCALL_NR(SMC_ENTITY_VM, 121)
#define SMC_SC_VM_RESTART_LAST              SMC_STDCALL_NR(SMC_ENTITY_VM, 122)
#define SMC_SC_VM_VIRTIO_GET_DESCR          SMC_STDCALL_NR(SMC_ENTITY_VM, 130)
#define SMC_SC_VM_VIRTIO_START              SMC_STDCALL_NR(SMC_ENTITY_VM, 131)
#define SMC_SC_VM_VIRTIO_STOP               SMC_STDCALL_NR(SMC_ENTITY_VM, 132)
#define SMC_SC_VM_VDEV_RESET                SMC_STDCALL_NR(SMC_ENTITY_VM, 133)
#define SMC_SC_VM_VDEV_KICK_VQ              SMC_STDCALL_NR(SMC_ENTITY_VM, 134)
#define SMC_SC_VM_TEST_ADD                  SMC_STDCALL_NR(SMC_ENTITY_VM, 200)
#define SMC_SC_VM_TEST_MULTIPLY             SMC_STDCALL_NR(SMC_ENTITY_VM, 201)

/****************************************************/
/********** PLATFORM Specific SMC Calls *************/
/****************************************************/
#define SMC_FC_PLAT_GET_UART_PA             SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 100)
#define SMC_FC_PLAT_REGISTER_IRQ            SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 101)
#define SMC_FC_PLAT_GET_NEXT_IRQ            SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 102)
#define SMC_FC_PLAT_GET_GIC_VERSION         SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 103)
#define SMC_FC_PLAT_GET_GICD_PA             SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 104)
#define SMC_FC_PLAT_GET_GICR_PA             SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 105)
#define SMC_FC_PLAT_REGISTER_BOOT_ENTRY     SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 106)
#define SMC_FC_PLAT_GET_RPMB_KEY            SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 107)

#define SMC_SC_GZ_SHARED_LOG_VERSION        SMC_STDCALL_NR(SMC_ENTITY_PLAT, 100)
#define SMC_SC_GZ_SHARED_LOG_ADD            SMC_STDCALL_NR(SMC_ENTITY_PLAT, 101)
#define SMC_SC_GZ_SHARED_LOG_RM             SMC_STDCALL_NR(SMC_ENTITY_PLAT, 102)
#define SMC_SC_GZ_NOP                       SMC_STDCALL_NR(SMC_ENTITY_PLAT, 110)
#define SMC_SC_GZ_NOP_LOCKED                SMC_STDCALL_NR(SMC_ENTITY_PLAT, 111)
#define SMC_SC_GZ_RESTART_LAST              SMC_STDCALL_NR(SMC_ENTITY_PLAT, 112)
#define SMC_SC_PLAT_TEST_MULTIPLY           SMC_STDCALL_NR(SMC_ENTITY_PLAT, 115)
#define SMC_SC_PLAT_INIT_SHARE_MEMORY       SMC_STDCALL_NR(SMC_ENTITY_PLAT, 120)
#define SMC_SC_PLAT_MTEE_SERVICE_CMD        SMC_STDCALL_NR(SMC_ENTITY_PLAT, 121)
#define SMC_SC_GZ_VIRTIO_GET_DESCR          SMC_STDCALL_NR(SMC_ENTITY_PLAT, 130)
#define SMC_SC_GZ_VIRTIO_START              SMC_STDCALL_NR(SMC_ENTITY_PLAT, 131)
#define SMC_SC_GZ_VIRTIO_STOP               SMC_STDCALL_NR(SMC_ENTITY_PLAT, 132)
#define SMC_SC_GZ_VDEV_RESET                SMC_STDCALL_NR(SMC_ENTITY_PLAT, 133)
#define SMC_SC_GZ_VDEV_KICK_VQ              SMC_STDCALL_NR(SMC_ENTITY_PLAT, 134)

#endif /* __HV_CALL_H__ */
