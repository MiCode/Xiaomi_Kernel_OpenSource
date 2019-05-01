// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include "hfi_common.h"
#include "hfi_io_common.h"

void __interrupt_init_iris1(struct venus_hfi_device *device)
{
	u32 mask_val = 0;

	/* All interrupts should be disabled initially 0x1F6 : Reset value */
	mask_val = __read_register(device, WRAPPER_INTR_MASK);

	/* Write 0 to unmask CPU and WD interrupts */
	mask_val &= ~(WRAPPER_INTR_MASK_A2HWD_BMSK |
			WRAPPER_INTR_MASK_A2HCPU_BMSK);
	__write_register(device, WRAPPER_INTR_MASK, mask_val);
}

void __setup_ucregion_memory_map_iris1(struct venus_hfi_device *device)
{
	/* initialize CPU QTBL & UCREGION */
	__write_register(device, UC_REGION_ADDR,
			(u32)device->iface_q_table.align_device_addr);
	__write_register(device, UC_REGION_SIZE, SHARED_QSIZE);
	__write_register(device, QTBL_ADDR,
			(u32)device->iface_q_table.align_device_addr);
	__write_register(device, QTBL_INFO, 0x01);
	if (device->sfr.align_device_addr)
		__write_register(device, SFR_ADDR,
				(u32)device->sfr.align_device_addr);
	if (device->qdss.align_device_addr)
		__write_register(device, MMAP_ADDR,
				(u32)device->qdss.align_device_addr);

	/* initialize DSP QTBL & UCREGION with CPU queues by default */
	__write_register(device, HFI_DSP_QTBL_ADDR,
			(u32)device->iface_q_table.align_device_addr);
	__write_register(device, HFI_DSP_UC_REGION_ADDR,
			(u32)device->iface_q_table.align_device_addr);
	__write_register(device, HFI_DSP_UC_REGION_SIZE, SHARED_QSIZE);
	if (device->res->domain_cvp) {
		/* initialize DSP QTBL & UCREGION with DSP queues */
		__write_register(device, HFI_DSP_QTBL_ADDR,
			(u32)device->dsp_iface_q_table.align_device_addr);
		__write_register(device, HFI_DSP_UC_REGION_ADDR,
			(u32)device->dsp_iface_q_table.align_device_addr);
		__write_register(device, HFI_DSP_UC_REGION_SIZE,
			device->dsp_iface_q_table.mem_data.size);
	}
}

void __clock_config_on_enable_iris1(struct venus_hfi_device *device)
{
	__write_register(device, WRAPPER_CPU_CGC_DIS, 0);
	__write_register(device, WRAPPER_CPU_CLOCK_CONFIG, 0);
}

